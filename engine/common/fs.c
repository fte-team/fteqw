#include "quakedef.h"

#include <ctype.h>


#include "hash.h"
hashtable_t filesystemhash;
qboolean com_fschanged = true;
extern cvar_t com_fs_cache;
int active_fs_cachetype;











char	gamedirfile[MAX_OSPATH];




//the various COM_LoadFiles set these on return
int	com_filesize;
qboolean com_file_copyprotected;


//
// in memory
//

typedef struct
{
	char	name[MAX_QPATH];
	int		filepos, filelen;

	bucket_t bucket;
} packfile_t;

typedef struct pack_s
{
	char	filename[MAX_OSPATH];
	FILE	*handle;
	int		numfiles;
	packfile_t	*files;
} pack_t;

//
// on disk
//
typedef struct
{
	char	name[56];
	int		filepos, filelen;
} dpackfile_t;

typedef struct
{
	int		filepos, filelen;
	char	name[8];
} dwadfile_t;

typedef struct
{
	char	id[4];
	int		dirofs;
	int		dirlen;
} dpackheader_t;

typedef struct
{
	char	id[4];
	int		dirlen;
	int		dirofs;
} dwadheader_t;

#define	MAX_FILES_IN_PACK	2048

char	com_gamedir[MAX_OSPATH];
char	*com_basedir;

char	com_quakedir[MAX_OSPATH];
char	com_homedir[MAX_OSPATH];

int fs_hash_dups;
int fs_hash_files;



typedef struct {
	void	(*PrintPath)(void *handle);
	void	(*ClosePath)(void *handle);
	void	(*BuildHash)(void *handle);
	qboolean (*FindFile)(void *handle, flocation_t *loc, char *name, void *hashedresult);	//true if found (hashedresult can be NULL)
		//note that if rawfile and offset are set, many Com_FileOpens will read the raw file
		//otherwise ReadFile will be called instead.
	void	(*ReadFile)(void *handle, flocation_t *loc, char *buffer);	//reads the entire file
	int		(*EnumerateFiles)(void *handle, char *match, int (*func)(char *, int, void *), void *parm);

	void	*(*OpenNew)(char *name);

	int		(*GeneratePureCRC) (void *handle, int seed, int usepure);
} searchpathfuncs_t;























//======================================================================================================
//STDIO files (OS)

void FSOS_PrintPath(void *handle)
{
	Con_Printf("%s\n", handle);
}
void FSOS_ClosePath(void *handle)
{
	Z_Free(handle);
}
int FSOS_RebuildFSHash(char *filename, int filesize, void *data)
{
	if (filename[strlen(filename)-1] == '/')
	{	//this is actually a directory

		char childpath[256];
		sprintf(childpath, "%s*", filename);
		Sys_EnumerateFiles((char*)data, childpath, FSOS_RebuildFSHash, data);
		return true;
	}
	if (!Hash_GetInsensative(&filesystemhash, filename))
	{
		bucket_t *bucket = (bucket_t*)BZ_Malloc(sizeof(bucket_t) + strlen(filename)+1);
		strcpy((char *)(bucket+1), filename);
#ifdef _WIN32
		Q_strlwr((char *)(bucket+1));
#endif
		Hash_AddInsensative(&filesystemhash, (char *)(bucket+1), data, bucket);

		fs_hash_files++;
	}
	else
		fs_hash_dups++;
	return true;
}
void FSOS_BuildHash(void *handle)
{
	Sys_EnumerateFiles(handle, "*", FSOS_RebuildFSHash, handle);
}
qboolean FSOS_FLocate(void *handle, flocation_t *loc, char *filename, void *hashedresult)
{
	FILE *f;
	int len;
	char netpath[MAX_OSPATH];


	if (hashedresult && (void *)hashedresult != handle)
		return -1;

/*
	if (!static_registered)
	{	// if not a registered version, don't ever go beyond base
		if ( strchr (filename, '/') || strchr (filename,'\\'))
			continue;
	}
*/			

// check a file in the directory tree
	_snprintf (netpath, sizeof(netpath)-1, "%s/%s",handle, filename);
	
	f = fopen(netpath, "rb");
	if (!f)
		return -1;

	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fclose(f);
	if (loc)
	{
		loc->len = len;
		loc->offset = 0;
		loc->index = 0;
		Q_strncpyz(loc->rawname, netpath, sizeof(loc->rawname));
	}

	return len;
}
void FSOS_ReadFile(void *handle, flocation_t *loc, char *buffer)
{
	FILE *f;
	f = fopen(loc->rawname, "rb");
	if (!f)	//err...
		return;
	fseek(f, loc->offset, SEEK_SET);
	fread(buffer, 1, loc->len, f);
	fclose(f);
}
searchpathfuncs_t osfilefuncs = {
	FSOS_PrintPath,
	FSOS_ClosePath,
	FSOS_BuildHash,
	FSOS_FLocate,
	FSOS_ReadFile,
	Sys_EnumerateFiles
};


//======================================================================================================
//PACK files (*.pak)

void FSPAK_PrintPath(void *handle)
{
	pack_t *pak = handle;
	
	Con_Printf("%s\n", pak->filename);
}
void FSPAK_ClosePath(void *handle)
{
	pack_t *pak = handle;
	fclose (pak->handle);
	if (pak->files)
		Z_Free(pak->files);
	Z_Free(pak);
}
void FSPAK_BuildHash(void *handle)
{
	pack_t *pak = handle;
	int i;

	for (i = 0; i < pak->numfiles; i++)
	{
		if (!Hash_GetInsensative(&filesystemhash, pak->files[i].name))
		{
			fs_hash_files++;
			Hash_AddInsensative(&filesystemhash, pak->files[i].name, &pak->files[i], &pak->files[i].bucket);
		}
		else
			fs_hash_dups++;
	}
}
qboolean FSPAK_FLocate(void *handle, flocation_t *loc, char *filename, void *hashedresult)
{
	packfile_t *pf = hashedresult;
	int i, len;
	pack_t		*pak = handle;

// look through all the pak file elements

	if (pf)
	{	//is this a pointer to a file in this pak?
		if (pf < pak->files || pf > pak->files + pak->numfiles)
			return -1;	//was found in a different path
	}
	else
	{
		for (i=0 ; i<pak->numfiles ; i++)	//look for the file
		{
			if (!strcmp (pak->files[i].name, filename))
			{
				pf = &pak->files[i];
				break;
			}
		}
	}

	if (pf)
	{
		len = pf->filelen;
		if (loc)
		{
			loc->index = pf - pak->files;
			strcpy(loc->rawname, pak->filename);
			loc->offset = pf->filepos;
			loc->len = pf->filelen;
		}
		return len;
	}
	return -1;
}
int FSPAK_EnumerateFiles (void *handle, char *match, int (*func)(char *, int, void *), void *parm)
{
	pack_t	*pak = handle;
	int		num;

	for (num = 0; num<(int)pak->numfiles; num++)
	{
		if (wildcmp(match, pak->files[num].name))
		{
			if (!func(pak->files[num].name, pak->files[num].filelen, parm))
				return false;
		}
	}

	return true;
}
/*
=================
COM_LoadPackFile

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
void *FSPAK_LoadPackFile (char *packfile)
{
	dpackheader_t	header;
	int				i;
//	int				j;
	packfile_t		*newfiles;
	int				numpackfiles;
	pack_t			*pack;
	FILE			*packhandle;
	dpackfile_t		info;
//	unsigned short		crc;

	if (COM_FileOpenRead (packfile, &packhandle) == -1)
		return NULL;

	fread (&header, 1, sizeof(header), packhandle);
	if (header.id[0] != 'P' || header.id[1] != 'A'
	|| header.id[2] != 'C' || header.id[3] != 'K')
	{
		return NULL;
//		Sys_Error ("%s is not a packfile", packfile);
	}
	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	numpackfiles = header.dirlen / sizeof(dpackfile_t);

//	if (numpackfiles > MAX_FILES_IN_PACK)
//		Sys_Error ("%s has %i files", packfile, numpackfiles);

//	if (numpackfiles != PAK0_COUNT)
//		com_modified = true;	// not the original file

	newfiles = (packfile_t*)Z_Malloc (numpackfiles * sizeof(packfile_t));

	fseek (packhandle, header.dirofs, SEEK_SET);
//	fread (&info, 1, header.dirlen, packhandle);

// crc the directory to check for modifications
//	crc = CRC_Block((qbyte *)info, header.dirlen);
	

//	CRC_Init (&crc);

	pack = (pack_t*)Z_Malloc (sizeof (pack_t));
// parse the directory
	for (i=0 ; i<numpackfiles ; i++)
	{
		fread (&info, 1, sizeof(info), packhandle);
/*
		for (j=0 ; j<sizeof(info) ; j++)
			CRC_ProcessByte(&crc, ((qbyte *)&info)[j]);
*/
		strcpy (newfiles[i].name, info.name);
		COM_CleanUpPath(newfiles[i].name);	//blooming tanks.
		newfiles[i].filepos = LittleLong(info.filepos);
		newfiles[i].filelen = LittleLong(info.filelen);
	}
/*
	if (crc != PAK0_CRC)
		com_modified = true;
*/
	strcpy (pack->filename, packfile);
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;
	
	Con_TPrintf (TL_ADDEDPACKFILE, packfile, numpackfiles);
	return pack;
}
searchpathfuncs_t packfilefuncs = {
	FSPAK_PrintPath,
	FSPAK_ClosePath,
	FSPAK_BuildHash,
	FSPAK_FLocate,
	FSOS_ReadFile,
	FSPAK_EnumerateFiles,
	FSPAK_LoadPackFile
};

//======================================================================================================
//ZIP files (*.zip *.pk3)



#define ZEXPORT VARGS

#ifdef ZLIB
#ifdef _WIN32
#pragma comment( lib, "../libs/zlib.lib" )
#endif

//#define uShort ZLIBuShort
//#define uLong ZLIBuLong

#include <zlib.h>
#include "unzip.c"

typedef struct zipfile_s
{
	char filename[MAX_QPATH];
	unzFile handle;
	int		numfiles;
	packfile_t	*files;

#ifdef HASH_FILESYSTEM
	hashtable_t hash;
#endif
} zipfile_t;

void *com_pathforfile;	//fread and stuff is preferable if null.


static void FSZIP_PrintPath(void *handle)
{
	zipfile_t *zip = handle;
	
	Con_Printf("%s\n", zip->filename);
}
static void FSZIP_ClosePath(void *handle)
{
	zipfile_t *zip = handle;
	unzClose(zip->handle);
	if (zip->files)
		Z_Free(zip->files);
	Z_Free(zip);
}
static void FSZIP_BuildHash(void *handle)
{
	zipfile_t *zip = handle;
	int i;

	for (i = 0; i < zip->numfiles; i++)
	{
		if (!Hash_GetInsensative(&filesystemhash, zip->files[i].name))
		{
			fs_hash_files++;
			Hash_AddInsensative(&filesystemhash, zip->files[i].name, &zip->files[i], &zip->files[i].bucket);
		}
		else
			fs_hash_dups++;
	}
}
static qboolean FSZIP_FLocate(void *handle, flocation_t *loc, char *filename, void *hashedresult)
{
	packfile_t *pf = hashedresult;
	int i, len;
	zipfile_t	*zip = handle;

// look through all the pak file elements

	if (pf)
	{	//is this a pointer to a file in this pak?
		if (pf < zip->files || pf >= zip->files + zip->numfiles)
			return -1;	//was found in a different path
	}
	else
	{
		for (i=0 ; i<zip->numfiles ; i++)	//look for the file
		{
			if (!strcmp (zip->files[i].name, filename))
			{
				pf = &zip->files[i];
				break;
			}
		}
	}

	if (pf)
	{
		len = pf->filelen;
		if (loc)
		{
			loc->index = pf - zip->files;
			strcpy(loc->rawname, zip->filename);
			loc->offset = pf->filepos;
			loc->len = pf->filelen;

			unzLocateFileMy (zip->handle, loc->index, zip->files[loc->index].filepos);
			loc->offset = unzGetCurrentFileUncompressedPos(zip->handle);
			if (loc->offset<0)
			{	//file not found, or is compressed.
				*loc->rawname = '\0';
				loc->offset=0;
			}
		}
		return len;
	}
	return -1;
}

static void FSZIP_ReadFile(void *handle, flocation_t *loc, char *buffer)
{
	zipfile_t *zip = handle;
	int err;

	unzLocateFileMy (zip->handle, loc->index, zip->files[loc->index].filepos);

	unzOpenCurrentFile (zip->handle);
	err = unzReadCurrentFile (zip->handle, buffer, zip->files[loc->index].filelen);
	unzCloseCurrentFile (zip->handle);

	if (err!=zip->files[loc->index].filelen)
	{
		Con_Printf ("Can't extract file \"%s:%s\" (corrupt)\n", zip->filename, zip->files[loc->index].name);
		return;
	}
	return;
}

static int FSZIP_EnumerateFiles (void *handle, char *match, int (*func)(char *, int, void *), void *parm)
{
	zipfile_t *zip = handle;
	int		num;

	for (num = 0; num<(int)zip->numfiles; num++)
	{
		if (wildcmp(match, zip->files[num].name))
		{
			if (!func(zip->files[num].name, zip->files[num].filelen, parm))
				return false;
		}
	}

	return true;
}

/*
=================
COM_LoadZipFile

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
static void *FSZIP_LoadZipFile (char *filename)
{
	int i;

	zipfile_t *zip;
	packfile_t		*newfiles;

	unz_global_info	globalinf;
	unz_file_info	file_info;

	FILE			*packhandle;
	if (COM_FileOpenRead (filename, &packhandle) == -1)
		return NULL;
	fclose(packhandle);	

	zip = Z_Malloc(sizeof(zipfile_t));
	Q_strncpyz(zip->filename, filename, sizeof(zip->filename));
	zip->handle = unzOpen (filename);
	if (!zip->handle)
	{
		Con_TPrintf (TL_COULDNTOPENZIP, filename);
		return NULL;
	}

	unzGetGlobalInfo (zip->handle, &globalinf);

	zip->numfiles = globalinf.number_entry;

	zip->files = newfiles = Z_Malloc (zip->numfiles * sizeof(packfile_t));
	for (i = 0; i < zip->numfiles; i++)
	{
		unzGetCurrentFileInfo (zip->handle, &file_info, newfiles[i].name, sizeof(newfiles[i].name), NULL, 0, NULL, 0);
		Q_strlwr(newfiles[i].name);
		newfiles[i].filelen = file_info.uncompressed_size;
		newfiles[i].filepos = file_info.c_offset;
		unzGoToNextFile (zip->handle);
	}


	Con_TPrintf (TL_ADDEDZIPFILE, filename, zip->numfiles);
	return zip;
}

int FSZIP_GeneratePureCRC(void *handle, int seed, int crctype)
{
	zipfile_t *zip = handle;
	unz_file_info	file_info;

	int *filecrcs;
	int numcrcs=0;
	int i;

	filecrcs = BZ_Malloc((zip->numfiles+1)*sizeof(int));
	filecrcs[numcrcs++] = seed;

	unzGoToFirstFile(zip->handle);
	for (i = 0; i < zip->numfiles; i++)
	{
		if (zip->files[i].filelen>0)
		{
			unzGetCurrentFileInfo (zip->handle, &file_info, NULL, 0, NULL, 0, NULL, 0);
			filecrcs[numcrcs++] = file_info.crc;
		}
		unzGoToNextFile (zip->handle);
	}

	if (crctype)
		return Com_BlockChecksum(filecrcs, numcrcs*sizeof(int));
	else
		return Com_BlockChecksum(filecrcs+1, (numcrcs-1)*sizeof(int));
}

searchpathfuncs_t zipfilefuncs = {
	FSZIP_PrintPath,
	FSZIP_ClosePath,
	FSZIP_BuildHash,
	FSZIP_FLocate,
	FSZIP_ReadFile,
	FSZIP_EnumerateFiles,
	FSZIP_LoadZipFile,
	FSZIP_GeneratePureCRC
};

#endif


//======================================================================================================





typedef struct searchpath_s
{
	searchpathfuncs_t *funcs;
	qboolean copyprotected;	//don't allow downloads from here.
	qboolean istemporary;
	void *handle;

	int crc_check;	//client sorts packs according to this checksum
	int crc_reply;	//client sends a different crc back to the server, for the paks it's actually loaded.

	struct searchpath_s *next;
	struct searchpath_s *nextpure;
} searchpath_t;

searchpath_t	*com_searchpaths;
searchpath_t	*com_purepaths;
searchpath_t	*com_base_searchpaths;	// without gamedirs


void COM_AddPathHandle(searchpathfuncs_t *funcs, void *handle, qboolean copyprotect, qboolean istemporary)
{
	searchpath_t *search;

	search = (searchpath_t*)Z_Malloc (sizeof(searchpath_t));
	search->copyprotected = copyprotect;
	search->istemporary = istemporary;
	search->handle = handle;
	search->funcs = funcs;

	search->next = com_searchpaths;
	com_searchpaths = search;

	com_fschanged = true;
}

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
int COM_FileOpenRead (char *path, FILE **hndl)
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

int COM_FileSize(char *path)
{
	int len;
	FILE *h;	
	len = COM_FOpenFile(path, &h);
	if (len>=0)
		fclose(h);

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
		for (s=com_purepaths ; s ; s=s->nextpure)
		{
			s->funcs->PrintPath(s->handle);
		}
		Con_Printf ("----------\n");
	}


	for (s=com_searchpaths ; s ; s=s->next)
	{
		if (s == com_base_searchpaths)
			Con_Printf ("----------\n");

		s->funcs->PrintPath(s->handle);
	}
}


/*
============
COM_Dir_f

============
*/
static int COM_Dir_List(char *name, int size, void *parm)
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
	if (FS_FLocateFile(Cmd_Argv(1), FSLFRT_LENGTH, &loc))
	{
		if (!*loc.rawname)
			Con_Printf("File is compressed\n");
		else
			Con_Printf("Inside %s\n", loc.rawname);
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
void COM_WriteFile (char *filename, void *data, int len)
{
	FILE	*f;
	char	name[MAX_OSPATH];
	
	sprintf (name, "%s/%s", com_gamedir, filename);

	COM_CreatePath(name);
	
	f = fopen (name, "wb");
	if (!f)
	{
		Sys_mkdir(com_gamedir);
		f = fopen (name, "wb");
		if (!f)
		{
			Con_Printf("Error opening %s for writing\n", filename);
			return;
		}
	}
	
	Sys_Printf ("COM_WriteFile: %s\n", name);
	fwrite (data, 1, len, f);
	fclose (f);

	com_fschanged=true;
}

FILE *COM_WriteFileOpen (char *filename)	//like fopen, but based around quake's paths.
{
	FILE	*f;
	char	name[MAX_OSPATH];
	
	sprintf (name, "%s/%s", com_gamedir, filename);

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
void	COM_CreatePath (char *path)
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
void COM_CopyFile (char *netpath, char *cachepath)
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
				if (bucket->keystring == (char*)(bucket+1))
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
		filesystemhash.bucket = (bucket_t**)BZ_Malloc(Hash_BytesForBuckets(filesystemhash.numbuckets));
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

	Con_Printf("%i unique files, %i duplicates\n", fs_hash_files, fs_hash_dups);
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
int FS_FLocateFile(char *filename, FSLF_ReturnType_e returntype, flocation_t *loc)
{
	int depth=0, len;
	searchpath_t	*search;

	void *pf;
//Con_Printf("Finding %s: ", filename);

	if (com_fs_cache.value)
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
			len = search->funcs->FindFile(search->handle, loc, filename, pf);
			if (len>=0)
			{
				if (loc)
					loc->search = search;
				goto out;
			}
		}
	}

//
// search through the path, one element at a time
//
	for (search = com_searchpaths ; search ; search = search->next)
	{
		len = search->funcs->FindFile(search->handle, loc, filename, pf);
		if (len>=0)
		{
			if (loc)
				loc->search = search;
			goto out;
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
*/	if (returntype == FSLFRT_LENGTH)
		return len;
	else
		return depth;
}
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
int COM_FOpenWriteFile(char *filename, FILE **file)
{
	COM_CreatePath(filename);
	*file = fopen(filename, "wb");
	return !!*file;
}
//int COM_FOpenFile (char *filename, FILE **file) {file_from_pak=0;return COM_FOpenFile2 (filename, file, false);}	//FIXME: TEMPORARY


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
qbyte *COM_LoadFile (char *path, int usehunk)
{
	qbyte *buf;
	FILE *f;
	int len;
	char	base[32];
	flocation_t loc;
	FS_FLocateFile(path, FSLFRT_LENGTH, &loc);

	if (!loc.search)
		return NULL;	//wasn't found

	if (*loc.rawname)
	{
//		Con_Printf("Opening %s\n", loc.rawname);
		f = fopen(loc.rawname, "rb");
		if (!f)
			return NULL;
		fseek(f, loc.offset, SEEK_SET);
	}
	else
		f = NULL;

	com_filesize = len = loc.len;
	// extract the filename base name for hunk tag
	COM_FileBase (path, base);

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
#ifndef SERVERONLY
	if (qrenderer)
		if (Draw_BeginDisc)
			Draw_BeginDisc ();
#endif

	if (f)
	{
		fread (buf, 1, len, f);
		fclose (f);
	}
	else
	{
		if (loc.search->funcs->ReadFile)
		{
			loc.search->funcs->ReadFile(loc.search->handle, &loc, buf);
		}
		else
			return NULL;
	}

#ifndef SERVERONLY
	if (qrenderer)
		if (Draw_EndDisc)
			Draw_EndDisc ();
#endif

	return buf;
}

qbyte *COM_LoadMallocFile (char *path)	//used for temp info along side temp hunk
{
	return COM_LoadFile (path, 5);
}

qbyte *COM_LoadHunkFile (char *path)
{
	return COM_LoadFile (path, 1);
}

qbyte *COM_LoadTempFile (char *path)
{
	return COM_LoadFile (path, 2);
}
qbyte *COM_LoadTempFile2 (char *path)
{
	return COM_LoadFile (path, 6);
}

void COM_LoadCacheFile (char *path, struct cache_user_s *cu)
{
	loadcache = cu;
	COM_LoadFile (path, 3);
}

// uses temp hunk if larger than bufsize
qbyte *COM_LoadStackFile (char *path, void *buffer, int bufsize)
{
	qbyte	*buf;
	
	loadbuf = (qbyte *)buffer;
	loadsize = bufsize;
	buf = COM_LoadFile (path, 4);
	
	return buf;
}



void COM_EnumerateFiles (char *match, int (*func)(char *, int, void *), void *parm)
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
	searchpath_t *next;
	while (com_searchpaths && com_searchpaths->istemporary)
	{
		com_searchpaths->funcs->ClosePath(com_searchpaths->handle);
		next = com_searchpaths->next;
		Z_Free (com_searchpaths);
		com_searchpaths = next;

		com_fschanged = true;
	}
}

qboolean COM_LoadMapPackFile (char *filename, int ofs)
{
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
}

#ifdef DOOMWADS
qboolean COM_LoadWadFile (char *wadname)
{
	dwadheader_t	header;
	int				i;
	packfile_t		*newfiles;
	int				numpackfiles;
	pack_t			*pack;
	FILE			*packhandle;
	dwadfile_t		info;
	int fstart;

	int section=0;
	char sectionname[MAX_QPATH];
	char filename[52];
	char neatwadname[52];

	searchpath_t *search;
	flocation_t loc;

	FS_FLocateFile(wadname, FSLFRT_LENGTH, &loc);

	if (!loc.search)
	{
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
		Con_Printf("Couldn't open file\n");
		return false;
	}
	fseek(packhandle, loc.offset, SEEK_SET);

	fstart = loc.offset;
	fseek(packhandle, fstart, SEEK_SET);

	fread (&header, 1, sizeof(header), packhandle);
	if (header.id[1] != 'W'
	|| header.id[2] != 'A' || header.id[3] != 'D')
	{
		return false;
	}
	if (header.id[0] == 'I')
		*neatwadname = '\0';
	else if (header.id[0] == 'P')
	{
		COM_StripExtension(wadname, neatwadname);
		strcat(neatwadname, ":");
	}
	else
		return false;
	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	numpackfiles = header.dirlen;

	newfiles = (packfile_t*)Z_Malloc (numpackfiles * sizeof(packfile_t));

	fseek (packhandle, header.dirofs+fstart, SEEK_SET);

	pack = (pack_t*)Z_Malloc (sizeof (pack_t));
#ifdef HASH_FILESYSTEM
	Hash_InitTable(&pack->hash, numpackfiles+1, Z_Malloc(Hash_BytesForBuckets(numpackfiles+1)));
#endif
// parse the directory
	for (i=0 ; i<numpackfiles ; i++)
	{
		fread (&info, 1, sizeof(info), packhandle);

		strcpy (filename, info.name);
		filename[8] = '\0';
		Q_strlwr(filename);

		newfiles[i].filepos = LittleLong(info.filepos)+fstart;
		newfiles[i].filelen = LittleLong(info.filelen);

		switch(section)	//be prepared to remap filenames.
		{
newsection:
		case 0:
			if (info.filelen == 0)
			{	//marker for something...

				if (!strcmp(filename, "s_start"))
				{
					section = 2;
					sprintf (newfiles[i].name, "sprites/%s", filename);
					break;
				}
				if (!strcmp(filename, "p_start"))
				{
					section = 3;
					sprintf (newfiles[i].name, "patches/%s", filename);
					break;
				}
				if (!strcmp(filename, "f_start"))
				{
					section = 4;
					sprintf (newfiles[i].name, "flats/%s", filename);
					break;
				}
				if ((filename[0] == 'e' && filename[2] == 'm') || !strncmp(filename, "map", 3))
				{	//this is the start of a beutiful new map
					section = 1;
					strcpy(sectionname, filename);
					sprintf (newfiles[i].name, "maps/%s%s.bsp", neatwadname, filename);
					newfiles[i].filepos = fstart;
					newfiles[i].filelen = 4;
					break;
				}
				if (!strncmp(filename, "gl_", 3) && ((filename[4] == 'e' && filename[5] == 'm') || !strncmp(filename+3, "map", 3)))
				{	//this is the start of a beutiful new map
					section = 5;
					strcpy(sectionname, filename+3);
					break;
				}
			}

			sprintf (newfiles[i].name, "wad/%s", filename);
			break;
		case 1:	//map section
			if (strcmp(filename, "things") &&
				strcmp(filename, "linedefs") &&
				strcmp(filename, "sidedefs") &&
				strcmp(filename, "vertexes") &&
				strcmp(filename, "segs") &&
				strcmp(filename, "ssectors") &&
				strcmp(filename, "nodes") &&
				strcmp(filename, "sectors") &&
				strcmp(filename, "reject") &&
				strcmp(filename, "blockmap"))
			{
				section = 0;
				goto newsection;
			}
			sprintf (newfiles[i].name, "maps/%s%s.%s", neatwadname, sectionname, filename);
			break;
		case 5:	//glbsp output section
			if (strcmp(filename, "gl_vert") &&
				strcmp(filename, "gl_segs") &&
				strcmp(filename, "gl_ssect") &&
				strcmp(filename, "gl_pvs") &&
				strcmp(filename, "gl_nodes"))
			{
				section = 0;
				goto newsection;
			}
			sprintf (newfiles[i].name, "maps/%s%s.%s", neatwadname, sectionname, filename);
			break;
		case 2:	//sprite section
			if (!strcmp(filename, "s_end"))
			{
				section = 0;
				goto newsection;
			}
			sprintf (newfiles[i].name, "sprites/%s", filename);
			break;
		case 3:	//patches section
			if (!strcmp(filename, "p_end"))
			{
				section = 0;
				goto newsection;
			}
			sprintf (newfiles[i].name, "patches/%s", filename);
			break;
		case 4:	//flats section
			if (!strcmp(filename, "f_end"))
			{
				section = 0;
				goto newsection;
			}
			sprintf (newfiles[i].name, "flats/%s", filename);
			break;
		}
#ifdef HASH_FILESYSTEM
		Hash_AddInsensative(&pack->hash, newfiles[i].name, &newfiles[i], &newfiles[i].bucket);
#endif
	}

	strcpy (pack->filename, loc.rawname);
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;

	Con_Printf ("Added wad file %s\n", wadname, numpackfiles);

	COM_AddPathHandle(&packfilefuncs, pack, true, false);

	COM_StripExtension(wadname, sectionname);
	strcat(sectionname, ".gwa");
	if (strcmp(sectionname, wadname))
		COM_LoadWadFile(sectionname);
	return true;
}
#endif



#ifdef DOOMWADS
static int COM_AddWad (char *descriptor)
{
	searchpath_t	*search;
	char			pakfile[MAX_OSPATH];

	sprintf (pakfile, descriptor, com_gamedir);

	for (search = com_searchpaths; search; search = search->next)
	{
		if (!stricmp(search->filename, pakfile))
			return true; //already loaded (base paths?)
	}

	return COM_LoadWadFile (descriptor);
}
#endif

static int COM_AddWildDataFiles (char *descriptor, int size, void *param)
{
	searchpathfuncs_t *funcs = param;
	searchpath_t	*search;
	pack_t			*pak;
	char			pakfile[MAX_OSPATH];

	sprintf (pakfile, "%s/%s", com_gamedir, descriptor);

	for (search = com_searchpaths; search; search = search->next)
	{
		if (search->funcs != funcs)
			continue;
		if (!stricmp((char*)search->handle, pakfile))	//assumption: first member of structure is a char array
			return true; //already loaded (base paths?)
	}

	pak = funcs->OpenNew (pakfile);
	if (!pak)
		return true;
	COM_AddPathHandle(funcs, pak, true, false);

	return true;
}

static void COM_AddDataFiles(char *extension, searchpathfuncs_t *funcs)
{
	int				i;
	void 			*handle;
	char			pakfile[MAX_OSPATH];

	for (i=0 ; ; i++)
	{
		sprintf (pakfile, "%s/pak%i.%s", com_gamedir, i, extension);
		handle = funcs->OpenNew (pakfile);
		if (!handle)
			break;
		COM_AddPathHandle(funcs, handle, true, false);
	}

	sprintf (pakfile, "*.%s", extension);
	Sys_EnumerateFiles(com_gamedir, pakfile, COM_AddWildDataFiles, funcs);
}

void COM_RefreshFSCache_f(void)
{
	com_fschanged=true;
}

void COM_FlushFSCache(void)
{
	if (com_fs_cache.value != 2)
		com_fschanged=true;
}
/*
================
COM_AddGameDirectory

Sets com_gamedir, adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ... 
================
*/
void COM_AddGameDirectory (char *dir)
{
	searchpath_t	*search;

	char			*p;

	if ((p = strrchr(dir, '/')) != NULL)
		strcpy(gamedirfile, ++p);
	else
		strcpy(gamedirfile, p);
	strcpy (com_gamedir, dir);

	for (search = com_searchpaths; search; search = search->next)
	{
		if (search->funcs != &osfilefuncs)
			continue;
		if (!stricmp(search->handle, com_gamedir))
			return; //already loaded (base paths?)
	}

//
// add the directory to the search path
//
	p = Z_Malloc(strlen(dir)+1);
	strcpy(p, dir);
	COM_AddPathHandle(&osfilefuncs, p, false, false);

//add any data files too
	COM_AddDataFiles("pak", &packfilefuncs);//q1/hl/h2/q2
	//pk2s never existed.
#ifdef ZLIB
	COM_AddDataFiles("pk3", &zipfilefuncs);	//q3 + offspring
	COM_AddDataFiles("pk4", &zipfilefuncs);	//q4
	//we could easily add zip, but it's friendlier not to
#endif
}

char *COM_NextPath (char *prevpath)
{
	searchpath_t	*s;
	char			*prev;

	if (!prevpath)
		return com_gamedir;

	prev = com_gamedir;
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
#ifdef WEBSERVER
	extern cvar_t httpserver;
#endif

	searchpath_t	*s;
	static char name[MAX_OSPATH];
	char			*protocol;

	for (s=com_searchpaths ; s ; s=s->next)
	{
		i--;
		if (!i)
			break;
	}
	if (i)	//too high.
		return NULL;

#ifdef WEBSERVER
	if (httpserver.value)
		protocol = va("http://%s/", NET_AdrToString (net_local_sv_ipadr));
	else
#endif
		protocol = "qw://";

	*crc = 0;//s->crc;
	strcpy(name, "FIXME");
//	Q_strncpyz(name, va("%s%s", protocol, COM_SkipPath(s->filename)), sizeof(name));
	return name;
}
#endif


/*
================
COM_Gamedir

Sets the gamedir and path to a different directory.
================
*/
void COM_Gamedir (char *dir)
{
	searchpath_t	*next;

	if (strstr(dir, "..") || strstr(dir, "/")
		|| strstr(dir, "\\") || strstr(dir, ":") )
	{
		Con_TPrintf (TL_GAMEDIRAINTPATH);
		return;
	}

	if (!strcmp(gamedirfile, dir))
		return;		// still the same

	FS_ForceToPure(NULL, NULL, 0);

#ifndef SERVERONLY
	Host_WriteConfiguration();	//before we change anything.
#endif

	strcpy (gamedirfile, dir);

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
		/*
		switch(com_searchpaths->type)
		{
		case SPT_PACK:
			fclose (com_searchpaths->u.pack->handle);
			Z_Free (com_searchpaths->u.pack->files);
#ifdef HASH_FILESYSTEM
			Z_Free (com_searchpaths->u.pack->hash.bucket);
#endif
			Z_Free (com_searchpaths->u.pack);
			break;
#ifdef ZLIB
		case SPT_ZIP:
			unzClose(com_searchpaths->u.zip->handle);
			Z_Free (com_searchpaths->u.zip->files);
#ifdef HASH_FILESYSTEM
			Z_Free (com_searchpaths->u.zip->hash.bucket);
#endif
			Z_Free (com_searchpaths->u.zip);
			break;
#endif
		case SPT_OS:
			break;
		}*/
		next = com_searchpaths->next;
		Z_Free (com_searchpaths);
		com_searchpaths = next;
	}

	com_fschanged = true;

	//
	// flush all data, so it will be forced to reload
	//
	Cache_Flush ();

	COM_AddGameDirectory(va("%s/%s", com_quakedir, dir));
	if (*com_homedir)
		COM_AddGameDirectory(va("%s/%s", com_homedir, dir));


#ifndef SERVERONLY
	{
		char	fn[MAX_OSPATH];
		FILE *f;

//		if (qrenderer)	//only do this if we have already started the renderer
//			Cbuf_InsertText("vid_restart\n", RESTRICT_LOCAL);

		sprintf(fn, "%s/%s", com_gamedir, "config.cfg");
		if ((f = fopen(fn, "r")) != NULL)
		{
			fclose(f);
			Cbuf_InsertText("cl_warncmd 0\n"
							"exec config.cfg\n"
							"exec fte.cfg\n"
							"exec frontend.cfg\n"
							"cl_warncmd 1\n", RESTRICT_LOCAL);
		}
	}

#ifdef Q3SHADERS
	{
		extern void Shader_Init(void);
		Shader_Init();	//FIXME!
	}
#endif

	Validation_FlushFileList();	//prevent previous hacks from making a difference.

	//FIXME: load new palette, if different cause a vid_restart.

#endif
}

typedef struct {
	char *file;
	char *path;
} potentialgamepath_t;

potentialgamepath_t pgp[] = {
	{"%s/id1/pak0.pak",		"%s/id1"},		//quake1
	{"%s/baseq2/pak0.pak",	"%s/baseq2"},	//quake2
	{"%s/data1/pak0.pak",	"%s/data1"},	//hexen2
	{"%s/data/data.pk3",	"%s/data"},		//nexuiz
	{"%s/baseq3/pak0.pk3",	"%s/baseq3"},	//quake3
	{"%s/base/assets0.pk3",	"%s/base"}		//jk2
};


typedef struct {
	char *gamename;	//sent to the master server when this is the current gamemode.
	char *exename;	//used if the exe name contains this
	char *argname;	//used if this was used as a parameter.
	char *auniquefile;	//used if this file is relative from the gamedir

	char *dir1;
	char *dir2;
	char *dir3;
	char *dir4;
} gamemode_info_t;
gamemode_info_t gamemode_info[] = {
//note that there is no basic 'fte' gamemode, this is because we aim for network compatability. Darkplaces-Quake is the closest we get.
//this is to avoid having too many gamemodes anyway.
	{"Darkplaces-Quake",	"darkplaces",	"-quake",		"id1/pak0.pak",		"id1",		"qw",				"fte"},
	{"Darkplaces-Hipnotic",	"hipnotic",		"-hipnotic",	NULL/*"hipnotic/pak0.pak"*/,"id1",		"qw",	"hipnotic",	"fte"},
	{"Darkplaces-Rogue",	"rogue",		"-rogue",		NULL/*"rogue/pak0.pak",	"id1"*/,		"qw",	"rogue",	"fte"},
	{"Nexuiz",				"nexuiz",		"-nexuiz",		"data/data20050531.pk3",	"id1",		"qw",	"data",		"fte"},

	//supported commercial mods (some are currently only partially supported)
	{"FTE-Hexen2",			"hexen",		"-hexen2",		"data1/pak0.pak",	"data1",						"fte"},
	{"FTE-Quake2",			"q2",			"-q2",			"baseq2/pak0.pak",	"baseq2",						"fte"},
	{"FTE-Quake3",			"q3",			"-q3",			"baseq3/pak0.pk3",	"baseq3",						"fte"},

	{"FTE-JK2",				"jk2",			"-jk2",			"base/assets0.pk3",	"base",							"fte"},

	{NULL}
};

//space-seperate pk3 names followed by space-seperated crcs
//note that we'll need to reorder and filter out files that don't match the crc.
void FS_ForceToPure(char *str, char *crcs, int seed)
{
	//pure files are more important than non-pure.

	searchpath_t *sp;
	searchpath_t *lastpure = NULL;
	int crc;

	if (!str)
	{	//pure isn't in use.
		com_purepaths = NULL;
		FS_FlushFSHash();
		return;
	}

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
			sp->crc_check = 0;
			sp->crc_reply = 0;
		}
	}

	while(crcs)
	{
		crcs = COM_Parse(crcs);
		crc = atoi(com_token);
		
		if (!crc)
			continue;

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
			Con_Printf("Pure crc %i wasn't found\n", crc);
	}

/* don't add any extras.
	for (sp = com_searchpaths; sp; sp = sp->next)
	{
		if (sp->nextpure == (void*)0x1)
		{
			if (lastpure)
				lastpure->nextpure = sp;
			sp->nextpure = NULL;
			lastpure = sp;
		}
	}
*/

	FS_FlushFSHash();
}

char *FS_GenerateClientPacksList(char *buffer, int maxlen, int basechecksum)
{
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
COM_InitFilesystem
================
*/
void COM_InitFilesystem (void)
{
	FILE *f;
	int		i;

	char *ev;


	int gamenum=-1;

//
// -basedir <path>
// Overrides the system supplied base directory (under id1)
//
	i = COM_CheckParm ("-basedir");
	if (i && i < com_argc-1)
		strcpy (com_quakedir, com_argv[i+1]);
	else
		strcpy (com_quakedir, host_parms.basedir);



	Cvar_Register(&com_gamename, "evil hacks");
	//identify the game from a telling file
	for (i = 0; gamemode_info[i].gamename; i++)
	{
		if (!gamemode_info[i].auniquefile)
			continue;	//no more
		f = fopen(va("%s/%s", com_quakedir, gamemode_info[i].auniquefile), "rb");
		if (f)
		{
			fclose(f);
			gamenum = i;
			break;
		}
	}
	//use the game based on an exe name over the filesystem one.
	for (i = 0; gamemode_info[i].gamename; i++)
	{
		if (strstr(com_argv[0], gamemode_info[i].exename))
			gamenum = i;
	}
	//use the game based on an parameter over all else.
	for (i = 0; gamemode_info[i].gamename; i++)
	{
		if (COM_CheckParm(gamemode_info[i].argname))
			gamenum = i;
	}

	if (gamenum<0)
	{
		for (i = 0; gamemode_info[i].gamename; i++)
		{
			if (!strcmp(gamemode_info[i].argname, "-quake"))
				gamenum = i;
		}
	}
	Cvar_Set(&com_gamename, gamemode_info[gamenum].gamename);


#ifdef _WIN32
	{	//win32 sucks.
		ev = getenv("HOMEDRIVE");
		if (ev)
			strcpy(com_homedir, ev);
		else
			strcpy(com_homedir, "");
		ev = getenv("HOMEPATH");
		if (ev)
			strcat(com_homedir, ev);
		else
			strcat(com_homedir, "/");
	}
#else
	//yay for unix!.
	ev = getenv("HOME");
	if (ev)
		Q_strncpyz(com_homedir, ev, sizeof(com_homedir));
	else
		*com_homedir = '\0';
#endif

	if (!COM_CheckParm("-usehome"))
		*com_homedir = '\0';

	if (*com_homedir)
	{
		strcat(com_homedir, "/.fte/");
		com_basedir = com_homedir;
	}
	else
	{
		com_basedir = com_quakedir;
	}

//
// start up with id1 by default
//
	i = COM_CheckParm ("-basegame");
	if (i && i < com_argc-1)
	{
		do	//use multiple -basegames
		{
			COM_AddGameDirectory (va("%s/%s", com_quakedir, com_argv[i+1]) );

			i = COM_CheckNextParm ("-basegame", i);
		}
		while (i && i < com_argc-1);
	}
	else
	{
		if (gamemode_info[gamenum].dir1)
			COM_AddGameDirectory (va("%s/%s", com_quakedir, gamemode_info[gamenum].dir1));
		if (gamemode_info[gamenum].dir2)
			COM_AddGameDirectory (va("%s/%s", com_quakedir, gamemode_info[gamenum].dir2));
		if (gamemode_info[gamenum].dir3)
			COM_AddGameDirectory (va("%s/%s", com_quakedir, gamemode_info[gamenum].dir3));
		if (gamemode_info[gamenum].dir4)
			COM_AddGameDirectory (va("%s/%s", com_quakedir, gamemode_info[gamenum].dir4));
	}

	if (*com_homedir)
		COM_AddGameDirectory (va("%s/fte", com_homedir) );

	// any set gamedirs will be freed up to here
	com_base_searchpaths = com_searchpaths;

	i = COM_CheckParm ("-game");	//effectivly replace with +gamedir x (But overridable)
	if (i && i < com_argc-1)
	{
		COM_AddGameDirectory (va("%s/%s", com_quakedir, com_argv[i+1]) );

#ifndef CLIENTONLY
		Info_SetValueForStarKey (svs.info, "*gamedir", com_argv[i+1], MAX_SERVERINFO_STRING);
#endif
	}
}

