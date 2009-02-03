#include "quakedef.h"
#include "netinc.h"

//#define com_gamedir com__gamedir

#include <ctype.h>


#include "hash.h"
hashtable_t filesystemhash;
qboolean com_fschanged = true;
extern cvar_t com_fs_cache;
int active_fs_cachetype;





typedef struct {
	void	(*PrintPath)(void *handle);
	void	(*ClosePath)(void *handle);
	void	(*BuildHash)(void *handle);
	qboolean (*FindFile)(void *handle, flocation_t *loc, char *name, void *hashedresult);	//true if found (hashedresult can be NULL)
		//note that if rawfile and offset are set, many Com_FileOpens will read the raw file
		//otherwise ReadFile will be called instead.
	void	(*ReadFile)(void *handle, flocation_t *loc, char *buffer);	//reads the entire file
	int		(*EnumerateFiles)(void *handle, char *match, int (*func)(char *, int, void *), void *parm);

	void	*(*OpenNew)(vfsfile_t *file, char *desc);	//returns a handle to a new pak/path

	int		(*GeneratePureCRC) (void *handle, int seed, int usepure);

	vfsfile_t *(*OpenVFS)(void *handle, flocation_t *loc, char *mode);
} searchpathfuncs_t;


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
		if (!VFS_READ(vf, &in, 1))
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
	char	descname[MAX_OSPATH];
	vfsfile_t	*handle;
	unsigned int filepos;	//the pos the subfiles left it at (to optimize calls to vfs_seek)
	int		numfiles;
	packfile_t	*files;
	int references;	//seeing as all vfiles from a pak file use the parent's vfsfile, we need to keep the parent open until all subfiles are closed.
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

char	com_gamedir[MAX_OSPATH];	//the os path where we write files
//char	*com_basedir;				//obsolete

char	com_quakedir[MAX_OSPATH];
char	com_homedir[MAX_OSPATH];

char	com_configdir[MAX_OSPATH];	//homedir/fte/configs

int fs_hash_dups;
int fs_hash_files;







int COM_FileOpenRead (char *path, FILE **hndl);



#define ENFORCEFOPENMODE(mode) {if (strcmp(mode, "r") && strcmp(mode, "w")/* && strcmp(mode, "rw")*/)Sys_Error("fs mode %s is not permitted here\n");}







//======================================================================================================
//STDIO files (OS)

typedef struct {
	vfsfile_t funcs;
	FILE *handle;
} vfsosfile_t;
int VFSOS_ReadBytes (struct vfsfile_s *file, void *buffer, int bytestoread)
{
	vfsosfile_t *intfile = (vfsosfile_t*)file;
	return fread(buffer, 1, bytestoread, intfile->handle);
}
int VFSOS_WriteBytes (struct vfsfile_s *file, void *buffer, int bytestoread)
{
	vfsosfile_t *intfile = (vfsosfile_t*)file;
	return fwrite(buffer, 1, bytestoread, intfile->handle);
}
qboolean VFSOS_Seek (struct vfsfile_s *file, unsigned long pos)
{
	vfsosfile_t *intfile = (vfsosfile_t*)file;
	return fseek(intfile->handle, pos, SEEK_SET) == 0;
}
unsigned long VFSOS_Tell (struct vfsfile_s *file)
{
	vfsosfile_t *intfile = (vfsosfile_t*)file;
	return ftell(intfile->handle);
}
unsigned long VFSOS_GetSize (struct vfsfile_s *file)
{
	vfsosfile_t *intfile = (vfsosfile_t*)file;

	unsigned int curpos;
	unsigned int maxlen;
	curpos = ftell(intfile->handle);
	fseek(intfile->handle, 0, SEEK_END);
	maxlen = ftell(intfile->handle);
	fseek(intfile->handle, curpos, SEEK_SET);

	return maxlen;
}
void VFSOS_Close(vfsfile_t *file)
{
	vfsosfile_t *intfile = (vfsosfile_t*)file;
	fclose(intfile->handle);
	Z_Free(file);
}

vfsfile_t *FS_OpenTemp(void)
{
	FILE *f;
	vfsosfile_t *file;

	f = tmpfile();
	if (!f)
		return NULL;

	file = Z_Malloc(sizeof(vfsosfile_t));
	file->funcs.ReadBytes = VFSOS_ReadBytes;
	file->funcs.WriteBytes = VFSOS_WriteBytes;
	file->funcs.Seek = VFSOS_Seek;
	file->funcs.Tell = VFSOS_Tell;
	file->funcs.GetLen = VFSOS_GetSize;
	file->funcs.Close = VFSOS_Close;
	file->handle = f;

	return (vfsfile_t*)file;
}

vfsfile_t *VFSOS_Open(char *osname, char *mode)
{
	FILE *f;
	vfsosfile_t *file;
	qboolean read = !!strchr(mode, 'r');
	qboolean write = !!strchr(mode, 'w');
	qboolean append = !!strchr(mode, 'a');
	qboolean text = !!strchr(mode, 't');
	char newmode[3];
	int modec = 0;

	if (read)
		newmode[modec++] = 'r';
	if (write)
		newmode[modec++] = 'w';
	if (append)
		newmode[modec++] = 'a';
	if (text)
		newmode[modec++] = 't';
	else
		newmode[modec++] = 'b';
	newmode[modec++] = '\0';

	f = fopen(osname, newmode);
	if (!f)
		return NULL;

	file = Z_Malloc(sizeof(vfsosfile_t));
	file->funcs.ReadBytes = strchr(mode, 'r')?VFSOS_ReadBytes:NULL;
	file->funcs.WriteBytes = (strchr(mode, 'w')||strchr(mode, 'a'))?VFSOS_WriteBytes:NULL;
	file->funcs.Seek = VFSOS_Seek;
	file->funcs.Tell = VFSOS_Tell;
	file->funcs.GetLen = VFSOS_GetSize;
	file->funcs.Close = VFSOS_Close;
	file->handle = f;

	return (vfsfile_t*)file;
}

vfsfile_t *FSOS_OpenVFS(void *handle, flocation_t *loc, char *mode)
{
	char diskname[MAX_OSPATH];

	snprintf(diskname, sizeof(diskname), "%s/%s", (char*)handle, loc->rawname);

	return VFSOS_Open(diskname, mode);
}

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
		return false;

/*
	if (!static_registered)
	{	// if not a registered version, don't ever go beyond base
		if ( strchr (filename, '/') || strchr (filename,'\\'))
			continue;
	}
*/

// check a file in the directory tree
	snprintf (netpath, sizeof(netpath)-1, "%s/%s",(char*)handle, filename);

	f = fopen(netpath, "rb");
	if (!f)
		return false;

	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fclose(f);
	if (loc)
	{
		loc->len = len;
		loc->offset = 0;
		loc->index = 0;
		Q_strncpyz(loc->rawname, filename, sizeof(loc->rawname));
	}

	return true;
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
int FSOS_EnumerateFiles (void *handle, char *match, int (*func)(char *, int, void *), void *parm)
{
	return Sys_EnumerateFiles(handle, match, func, parm);
}

searchpathfuncs_t osfilefuncs = {
	FSOS_PrintPath,
	FSOS_ClosePath,
	FSOS_BuildHash,
	FSOS_FLocate,
	FSOS_ReadFile,
	FSOS_EnumerateFiles,
	NULL,
	NULL,
	FSOS_OpenVFS
};


//======================================================================================================
//PACK files (*.pak)

void FSPAK_PrintPath(void *handle)
{
	pack_t *pak = handle;

	if (pak->references != 1)
		Con_Printf("%s (%i)\n", pak->descname, pak->references-1);
	else
		Con_Printf("%s\n", pak->descname);
}
void FSPAK_ClosePath(void *handle)
{
	pack_t *pak = handle;

	pak->references--;
	if (pak->references > 0)
		return;	//not free yet


	VFS_CLOSE (pak->handle);
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
			return false;	//was found in a different path
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
			snprintf(loc->rawname, sizeof(loc->rawname), "%s/%s", pak->descname, filename);
			loc->offset = pf->filepos;
			loc->len = pf->filelen;
		}
		return true;
	}
	return false;
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
void *FSPAK_LoadPackFile (vfsfile_t *file, char *desc)
{
	dpackheader_t	header;
	int				i;
//	int				j;
	packfile_t		*newfiles;
	int				numpackfiles;
	pack_t			*pack;
	vfsfile_t		*packhandle;
	dpackfile_t		info;
	int read;
//	unsigned short		crc;

	packhandle = file;
	if (packhandle == NULL)
		return NULL;

	VFS_READ(packhandle, &header, sizeof(header));
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

	VFS_SEEK(packhandle, header.dirofs);
//	fread (&info, 1, header.dirlen, packhandle);

// crc the directory to check for modifications
//	crc = QCRC_Block((qbyte *)info, header.dirlen);


//	QCRC_Init (&crc);

	pack = (pack_t*)Z_Malloc (sizeof (pack_t));
// parse the directory
	for (i=0 ; i<numpackfiles ; i++)
	{
		*info.name = '\0';
		read = VFS_READ(packhandle, &info, sizeof(info));
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
	strcpy (pack->descname, desc);
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;
	pack->filepos = 0;
	VFS_SEEK(packhandle, pack->filepos);

	pack->references++;

	Con_TPrintf (TL_ADDEDPACKFILE, desc, numpackfiles);
	return pack;
}


typedef struct {
	vfsfile_t funcs;
	pack_t *parentpak;
	unsigned long startpos;
	unsigned long length;
	unsigned long currentpos;
} vfspack_t;
int VFSPAK_ReadBytes (struct vfsfile_s *vfs, void *buffer, int bytestoread)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	int read;

	if (bytestoread == 0)
		return 0;

	if (vfsp->currentpos - vfsp->startpos + bytestoread > vfsp->length)
		bytestoread = vfsp->length - (vfsp->currentpos - vfsp->startpos);
	if (bytestoread <= 0)
	{
		return -1;
	}

	if (vfsp->parentpak->filepos != vfsp->currentpos)
		VFS_SEEK(vfsp->parentpak->handle, vfsp->currentpos);
	read = VFS_READ(vfsp->parentpak->handle, buffer, bytestoread);
	vfsp->currentpos += read;
	vfsp->parentpak->filepos = vfsp->currentpos;

	return read;
}
int VFSPAK_WriteBytes (struct vfsfile_s *vfs, void *buffer, int bytestoread)
{	//not supported.
	Sys_Error("Cannot write to pak files\n");
	return 0;
}
qboolean VFSPAK_Seek (struct vfsfile_s *vfs, unsigned long pos)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	if (pos < 0 || pos > vfsp->length)
		return false;
	vfsp->currentpos = pos + vfsp->startpos;

	return true;
}
unsigned long VFSPAK_Tell (struct vfsfile_s *vfs)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	return vfsp->currentpos - vfsp->startpos;
}
unsigned long VFSPAK_GetLen (struct vfsfile_s *vfs)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	return vfsp->length;
}
void VFSPAK_Close(vfsfile_t *vfs)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	FSPAK_ClosePath(vfsp->parentpak);	//tell the parent that we don't need it open any more (reference counts)
	Z_Free(vfsp);	//free ourselves.
}
vfsfile_t *FSPAK_OpenVFS(void *handle, flocation_t *loc, char *mode)
{
	pack_t *pack = (pack_t*)handle;
	vfspack_t *vfs;

	if (strcmp(mode, "rb"))
		return NULL; //urm, unable to write/append

	vfs = Z_Malloc(sizeof(vfspack_t));

	vfs->parentpak = pack;
	vfs->parentpak->references++;

	vfs->startpos = loc->offset;
	vfs->length = loc->len;
	vfs->currentpos = vfs->startpos;

	vfs->funcs.Close = VFSPAK_Close;
	vfs->funcs.GetLen = VFSPAK_GetLen;
	vfs->funcs.ReadBytes = VFSPAK_ReadBytes;
	vfs->funcs.Seek = VFSPAK_Seek;
	vfs->funcs.Tell = VFSPAK_Tell;
	vfs->funcs.WriteBytes = VFSPAK_WriteBytes;	//not supported

	return (vfsfile_t *)vfs;
}

searchpathfuncs_t packfilefuncs = {
	FSPAK_PrintPath,
	FSPAK_ClosePath,
	FSPAK_BuildHash,
	FSPAK_FLocate,
	FSOS_ReadFile,
	FSPAK_EnumerateFiles,
	FSPAK_LoadPackFile,
	NULL,
	FSPAK_OpenVFS
};



#ifdef DOOMWADS
void *FSPAK_LoadDoomWadFile (vfsfile_t *packhandle, char *desc)
{
	dwadheader_t	header;
	int				i;
	packfile_t		*newfiles;
	int				numpackfiles;
	pack_t			*pack;
	dwadfile_t		info;

	int section=0;
	char sectionname[MAX_QPATH];
	char filename[52];
	char neatwadname[52];

	if (packhandle == NULL)
		return NULL;

	VFS_READ(packhandle, &header, sizeof(header));
	if (header.id[1] != 'W'	|| header.id[2] != 'A' || header.id[3] != 'D')
		return NULL;	//not a doom wad

	//doom wads come in two sorts. iwads and pwads.
	//iwads are the master wads, pwads are meant to replace parts of the master wad.
	//this is awkward, of course.
	//we ignore the i/p bit for the most part, but with maps, pwads are given a prefixed name.
	if (header.id[0] == 'I')
		*neatwadname = '\0';
	else if (header.id[0] == 'P')
	{
		COM_FileBase(desc, neatwadname, sizeof(neatwadname));
		strcat(neatwadname, "#");
	}
	else
		return NULL;

	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	numpackfiles = header.dirlen;
	newfiles = (packfile_t*)Z_Malloc (numpackfiles * sizeof(packfile_t));
	VFS_SEEK(packhandle, header.dirofs);

	//doom wads are awkward.
	//they have no directory structure, except for start/end 'files'.
	//they follow along the lines of lumps after the parent name.
	//a map is the name of that map, and then a squence of the lumps that form that map (found by next-with-that-name).
	//this is a problem for a real virtual filesystem, so we add a hack to recognise special names and expand them specially.
	for (i=0 ; i<numpackfiles ; i++)
	{
		VFS_READ (packhandle, &info, sizeof(info));

		strcpy (filename, info.name);
		filename[8] = '\0';
		Q_strlwr(filename);

		newfiles[i].filepos = LittleLong(info.filepos);
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
					sprintf (newfiles[i].name, "sprites/%s", filename);	//the model loader has a hack to recognise .dsp
					break;
				}
				if (!strcmp(filename, "p_start"))
				{
					section = 3;
					sprintf (newfiles[i].name, "patches/%s", filename); //the map loader will find these.
					break;
				}
				if (!strcmp(filename, "f_start"))
				{
					section = 4;
					sprintf (newfiles[i].name, "flats/%s", filename);	//the map loader will find these
					break;
				}
				if ((filename[0] == 'e' && filename[2] == 'm') || !strncmp(filename, "map", 3))
				{	//this is the start of a beutiful new map
					section = 1;
					strcpy(sectionname, filename);
					sprintf (newfiles[i].name, "maps/%s%s.bsp", neatwadname, filename);	//generate fake bsps to allow the server to find them
					newfiles[i].filepos = 0;
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

			sprintf (newfiles[i].name, "wad/%s", filename);	//but there are many files that we don't recognise/know about. archive them off to keep the vfs moderatly clean.
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
	}

	pack = (pack_t*)Z_Malloc (sizeof (pack_t));
	strcpy (pack->descname, desc);
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;
	pack->filepos = 0;
	VFS_SEEK(packhandle, pack->filepos);

	pack->references++;

	Con_TPrintf (TL_ADDEDPACKFILE, desc, numpackfiles);
	return pack;
}
searchpathfuncs_t doomwadfilefuncs = {
	FSPAK_PrintPath,
	FSPAK_ClosePath,
	FSPAK_BuildHash,
	FSPAK_FLocate,
	FSOS_ReadFile,
	FSPAK_EnumerateFiles,
	FSPAK_LoadDoomWadFile,
	NULL,
	FSPAK_OpenVFS
};
#endif
//======================================================================================================
//ZIP files (*.zip *.pk3)

void *com_pathforfile;	//fread and stuff is preferable if null

#ifndef ZEXPORT
#define ZEXPORT VARGS
#endif

#ifdef AVAIL_ZLIB
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

	vfsfile_t *raw;
	vfsfile_t *currentfile;	//our unzip.c can only handle one active file at any one time
							//so we have to keep closing and switching.
							//slow, but it works. most of the time we'll only have a single file open anyway.
	int references;	//and a reference count
} zipfile_t;


static void FSZIP_PrintPath(void *handle)
{
	zipfile_t *zip = handle;

	if (zip->references != 1)
		Con_Printf("%s (%i)\n", zip->filename, zip->references-1);
	else
		Con_Printf("%s\n", zip->filename);
}
static void FSZIP_ClosePath(void *handle)
{
	zipfile_t *zip = handle;

	if (--zip->references > 0)
		return;	//not yet time

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
			return false;	//was found in a different path
	}
	else
	{
		for (i=0 ; i<zip->numfiles ; i++)	//look for the file
		{
			if (!stricmp (zip->files[i].name, filename))
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
//			if (loc->offset<0)
//			{	//file not found, or is compressed.
//				*loc->rawname = '\0';
//				loc->offset=0;
//			}
		}
		return true;
	}
	return false;
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
static void *FSZIP_LoadZipFile (vfsfile_t *packhandle, char *desc)
{
	int i;
	int nextfileziphandle;

	zipfile_t *zip;
	packfile_t		*newfiles;

	unz_global_info	globalinf = {0};
	unz_file_info	file_info;

	zip = Z_Malloc(sizeof(zipfile_t));
	Q_strncpyz(zip->filename, desc, sizeof(zip->filename));
	zip->handle = unzOpen ((zip->raw = packhandle));
	if (!zip->handle)
	{
		Z_Free(zip);
		Con_TPrintf (TL_COULDNTOPENZIP, desc);
		return NULL;
	}

	unzGetGlobalInfo (zip->handle, &globalinf);

	zip->numfiles = globalinf.number_entry;

	zip->files = newfiles = Z_Malloc (zip->numfiles * sizeof(packfile_t));
	for (i = 0; i < zip->numfiles; i++)
	{
		if (unzGetCurrentFileInfo (zip->handle, &file_info, newfiles[i].name, sizeof(newfiles[i].name), NULL, 0, NULL, 0) != UNZ_OK)
			Con_Printf("Zip Error\n");
		Q_strlwr(newfiles[i].name);
		newfiles[i].filelen = file_info.uncompressed_size;
		newfiles[i].filepos = file_info.c_offset;

		nextfileziphandle = unzGoToNextFile (zip->handle);
		if (nextfileziphandle == UNZ_END_OF_LIST_OF_FILE)
			break;
		else if (nextfileziphandle != UNZ_OK)
			Con_Printf("Zip Error\n");
	}

	zip->references = 1;
	zip->currentfile = NULL;

	Con_TPrintf (TL_ADDEDZIPFILE, desc, zip->numfiles);
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

typedef struct {
	vfsfile_t funcs;

	vfsfile_t *defer;

	//in case we're forced away.
	zipfile_t *parent;
	qboolean iscompressed;
	int pos;
	int length;	//try and optimise some things
	int index;
	int startpos;
} vfszip_t;
void VFSZIP_MakeActive(vfszip_t *vfsz)
{
	int i;
	char buffer[8192];	//must be power of two

	if ((vfszip_t*)vfsz->parent->currentfile == vfsz)
		return;	//already us
	if (vfsz->parent->currentfile)
		unzCloseCurrentFile(vfsz->parent->handle);

	unzLocateFileMy(vfsz->parent->handle, vfsz->index, vfsz->startpos);
	unzOpenCurrentFile(vfsz->parent->handle);


	if (vfsz->pos > 0)
	{
		Con_DPrintf("VFSZIP_MakeActive: Shockingly inefficient\n");

		//now we need to seek up to where we had previously gotten to.
		for (i = 0; i < vfsz->pos-sizeof(buffer); i++)
			unzReadCurrentFile(vfsz->parent->handle, buffer, sizeof(buffer));
		unzReadCurrentFile(vfsz->parent->handle, buffer, vfsz->pos - i);
	}

	vfsz->parent->currentfile = (vfsfile_t*)vfsz;
}

int VFSZIP_ReadBytes (struct vfsfile_s *file, void *buffer, int bytestoread)
{
	int read;
	vfszip_t *vfsz = (vfszip_t*)file;

	if (vfsz->defer)
		return VFS_READ(vfsz->defer, buffer, bytestoread);

	if (vfsz->iscompressed)
	{
		VFSZIP_MakeActive(vfsz);
		read = unzReadCurrentFile(vfsz->parent->handle, buffer, bytestoread);
	}
	else
	{
		if (vfsz->parent->currentfile != file)
		{
			unzCloseCurrentFile(vfsz->parent->handle);
			VFS_SEEK(vfsz->parent->raw, vfsz->pos+vfsz->startpos);
			vfsz->parent->currentfile = file;
		}
		read = VFS_READ(vfsz->parent->raw, buffer, bytestoread);
	}

	vfsz->pos += read;
	return read;
}
int VFSZIP_WriteBytes (struct vfsfile_s *file, void *buffer, int bytestoread)
{
	Sys_Error("VFSZIP_WriteBytes: Not supported\n");
	return 0;
}
qboolean VFSZIP_Seek (struct vfsfile_s *file, unsigned long pos)
{
	vfszip_t *vfsz = (vfszip_t*)file;

	if (vfsz->defer)
		return VFS_SEEK(vfsz->defer, pos);

	//This is *really* inefficient
	if (vfsz->parent->currentfile == file)
	{
		if (vfsz->iscompressed)
		{	//if they're going to seek on a file in a zip, let's just copy it out
			char buffer[8192];
			unsigned int chunk;
			unsigned int i;
			unsigned int length;

			vfsz->defer = FS_OpenTemp();
			if (vfsz->defer)
			{
				unzCloseCurrentFile(vfsz->parent->handle);
				vfsz->parent->currentfile = NULL;	//make it not us

				length = vfsz->length;
				i = 0;
				vfsz->pos = 0;
				VFSZIP_MakeActive(vfsz);
				while (1)
				{
					chunk = length - i;
					if (chunk > sizeof(buffer))
						chunk = sizeof(buffer);
					if (chunk == 0)
						break;
					unzReadCurrentFile(vfsz->parent->handle, buffer, chunk);
					VFS_WRITE(vfsz->defer, buffer, chunk);

					i += chunk;
				}
			}
		}

		unzCloseCurrentFile(vfsz->parent->handle);
		vfsz->parent->currentfile = NULL;	//make it not us

		if (vfsz->defer)
			return VFS_SEEK(vfsz->defer, pos);
	}



	if (pos < 0 || pos > vfsz->length)
		return false;
	vfsz->pos = pos;

	return true;
}
unsigned long VFSZIP_Tell (struct vfsfile_s *file)
{
	vfszip_t *vfsz = (vfszip_t*)file;

	if (vfsz->defer)
		return VFS_TELL(vfsz->defer);

	return vfsz->pos;
}
unsigned long VFSZIP_GetLen (struct vfsfile_s *file)
{
	vfszip_t *vfsz = (vfszip_t*)file;
	return vfsz->length;
}
void VFSZIP_Close (struct vfsfile_s *file)
{
	vfszip_t *vfsz = (vfszip_t*)file;

	if (vfsz->parent->currentfile == file)
		vfsz->parent->currentfile = NULL;	//make it not us

	if (vfsz->defer)
		VFS_CLOSE(vfsz->defer);

	FSZIP_ClosePath(vfsz->parent);
	Z_Free(vfsz);
}

vfsfile_t *FSZIP_OpenVFS(void *handle, flocation_t *loc, char *mode)
{
	int rawofs;
	zipfile_t *zip = handle;
	vfszip_t *vfsz;

	if (strcmp(mode, "rb"))
		return NULL; //urm, unable to write/append

	vfsz = Z_Malloc(sizeof(vfszip_t));

	vfsz->parent = zip;
	vfsz->index = loc->index;
	vfsz->startpos = zip->files[loc->index].filepos;
	vfsz->length = loc->len;

	vfsz->funcs.Close = VFSZIP_Close;
	vfsz->funcs.GetLen = VFSZIP_GetLen;
	vfsz->funcs.ReadBytes = VFSZIP_ReadBytes;
	vfsz->funcs.Seek = VFSZIP_Seek;
	vfsz->funcs.Tell = VFSZIP_Tell;
	vfsz->funcs.WriteBytes = NULL;
	vfsz->funcs.seekingisabadplan = true;

	unzLocateFileMy(vfsz->parent->handle, vfsz->index, vfsz->startpos);
	rawofs = unzGetCurrentFileUncompressedPos(zip->handle);
	vfsz->iscompressed = rawofs<0;
	if (!vfsz->iscompressed)
	{
		vfsz->startpos = rawofs;
		VFS_SEEK(zip->raw, vfsz->startpos);
		vfsz->parent->currentfile = (vfsfile_t*)vfsz;
	}

	zip->references++;

	return (vfsfile_t*)vfsz;
}

searchpathfuncs_t zipfilefuncs = {
	FSZIP_PrintPath,
	FSZIP_ClosePath,
	FSZIP_BuildHash,
	FSZIP_FLocate,
	FSZIP_ReadFile,
	FSZIP_EnumerateFiles,
	FSZIP_LoadZipFile,
	FSZIP_GeneratePureCRC,
	FSZIP_OpenVFS
};

#endif


//======================================================================================================





typedef struct searchpath_s
{
	searchpathfuncs_t *funcs;
	qboolean copyprotected;	//don't allow downloads from here.
	qboolean istemporary;
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

static void COM_AddDataFiles(char *purepath, char *pathto, searchpath_t *search, char *extension, searchpathfuncs_t *funcs);

searchpath_t *COM_AddPathHandle(char *purepath, char *probablepath, searchpathfuncs_t *funcs, void *handle, qboolean copyprotect, qboolean istemporary, unsigned int loadstuff)
{
	searchpath_t *search;

	search = (searchpath_t*)Z_Malloc (sizeof(searchpath_t));
	search->copyprotected = copyprotect;
	search->istemporary = istemporary;
	search->handle = handle;
	search->funcs = funcs;
	Q_strncpyz(search->purepath, purepath, sizeof(search->purepath));

	search->next = com_searchpaths;
	com_searchpaths = search;

	com_fschanged = true;


	//add any data files too
	if (loadstuff & 2)
		COM_AddDataFiles(purepath, probablepath, search, "pak", &packfilefuncs);//q1/hl/h2/q2
	//pk2s never existed.
#ifdef AVAIL_ZLIB
	if (loadstuff & 4)
		COM_AddDataFiles(purepath, probablepath, search, "pk3", &zipfilefuncs);	//q3 + offspring
	if (loadstuff & 8)
		COM_AddDataFiles(purepath, probablepath, search, "pk4", &zipfilefuncs);	//q4
	//we could easily add zip, but it's friendlier not to
#endif

#ifdef DOOMWADS
	if (loadstuff & 16)
		COM_AddDataFiles(purepath, probablepath, search, "wad", &doomwadfilefuncs);	//q4
#endif

	return search;
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
			s->funcs->PrintPath(s->handle);
		}
		Con_Printf ("----------\n");
		Con_Printf ("Impure paths:\n");
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
	if (FS_FLocateFile(Cmd_Argv(1), FSLFRT_LENGTH, &loc)>=0)
	{
		if (!*loc.rawname)
		{
			Con_Printf("File is compressed inside ");
			loc.search->funcs->PrintPath(loc.search->handle);
		}
		else
		{
			Con_Printf("Inside %s\n", loc.rawname);
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
void COM_WriteFile (char *filename, void *data, int len)
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
			if (search->funcs->FindFile(search->handle, loc, filename, pf))
			{
				if (loc)
				{
					loc->search = search;
					len = loc->len;
				}
				else
					len = 0;
				com_file_copyprotected = search->copyprotected;
				goto out;
			}
			depth += (search->funcs != &osfilefuncs || returntype == FSLFRT_DEPTH_ANYPATH);
		}
	}

//
// search through the path, one element at a time
//
	for (search = com_searchpaths ; search ; search = search->next)
	{
		if (search->funcs->FindFile(search->handle, loc, filename, pf))
		{
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
		depth += (search->funcs != &osfilefuncs || returntype == FSLFRT_DEPTH_ANYPATH);
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
char *FS_GetPackNames(char *buffer, int buffersize, qboolean referencedonly)
{
	searchpath_t	*search;
	buffersize--;
	*buffer = 0;
	
	if (com_purepaths)
	{
		for (search = com_purepaths ; search ; search = search->nextpure)
		{
			Q_strncatz(buffer, va("%s ", search->purepath), buffersize);
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
				Q_strncatz(buffer, va("%s ", search->purepath), buffersize);
			}
		}
		return buffer;
	}
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

//true if protection kicks in
qboolean Sys_PathProtection(char *pattern)
{
	if (strchr(pattern, '\\'))
	{
		char *s;
		Con_Printf("Warning: \\ characters in filename %s\n", pattern);
		while((s = strchr(pattern, '\\')))
			*s = '/';
	}

	if (strstr(pattern, ".."))
		Con_Printf("Error: '..' characters in filename %s\n", pattern);
	else if (pattern[0] == '/')
		Con_Printf("Error: absolute path in filename %s\n", pattern);
	else if (strstr(pattern, ":")) //win32 drive seperator (or mac path seperator, but / works there and they're used to it)
		Con_Printf("Error: absolute path in filename %s\n", pattern);
	else
		return false;
	return true;
}

#ifdef AVAIL_ZLIB
typedef struct {
	unsigned char ident1;
	unsigned char ident2;
	unsigned char cm;
	unsigned char flags;
	unsigned int mtime;
	unsigned char xflags;
	unsigned char os;
} gzheader_t;
#define sizeofgzheader_t 10

#define	GZ_FTEXT	1
#define	GZ_FHCRC	2
#define GZ_FEXTRA	4
#define GZ_FNAME	8
#define GZ_FCOMMENT	16
#define GZ_RESERVED (32|64|128)

#include <zlib.h>

vfsfile_t *FS_DecompressGZip(vfsfile_t *infile, gzheader_t *header)
{
	char inchar;
	unsigned short inshort;
	vfsfile_t *temp;

	if (header->flags & GZ_RESERVED)
	{	//reserved bits should be 0
		//this is probably static, so it's not a gz. doh.
		VFS_SEEK(infile, 0);
		return infile;
	}

	if (header->flags & GZ_FEXTRA)
	{
		VFS_READ(infile, &inshort, sizeof(inshort));
		inshort = LittleShort(inshort);
		VFS_SEEK(infile, VFS_TELL(infile) + inshort);
	}

	if (header->flags & GZ_FNAME)
	{
		Con_Printf("gzipped file name: ");
		do {
			if (VFS_READ(infile, &inchar, sizeof(inchar)) != 1)
				break;
			Con_Printf("%c", inchar);
		} while(inchar);
		Con_Printf("\n");
	}

	if (header->flags & GZ_FCOMMENT)
	{
		Con_Printf("gzipped file comment: ");
		do {
			if (VFS_READ(infile, &inchar, sizeof(inchar)) != 1)
				break;
			Con_Printf("%c", inchar);
		} while(inchar);
		Con_Printf("\n");
	}

	if (header->flags & GZ_FHCRC)
	{
		VFS_READ(infile, &inshort, sizeof(inshort));
	}



	temp = FS_OpenTemp();
	if (!temp)
	{
		VFS_SEEK(infile, 0);	//doh
		return infile;
	}


	{
		char inbuffer[16384];
		char outbuffer[16384];
		int ret;

		z_stream strm = {
			inbuffer,
			0,
			0,

			outbuffer,
			sizeof(outbuffer),
			0,

			NULL,
			NULL,

			NULL,
			NULL,
			NULL,

			Z_UNKNOWN,
			0,
			0
		};

		strm.avail_in = VFS_READ(infile, inbuffer, sizeof(inbuffer));
		strm.next_in = inbuffer;

		inflateInit2(&strm, -MAX_WBITS);

		while ((ret=inflate(&strm, Z_SYNC_FLUSH)) != Z_STREAM_END)
		{
			if (strm.avail_in == 0 || strm.avail_out == 0)
			{
				if (strm.avail_in == 0)
				{
					strm.avail_in = VFS_READ(infile, inbuffer, sizeof(inbuffer));
					strm.next_in = inbuffer;
				}

				if (strm.avail_out == 0)
				{
					strm.next_out = outbuffer;
					VFS_WRITE(temp, outbuffer, strm.total_out);
					strm.total_out = 0;
					strm.avail_out = sizeof(outbuffer);
				}
				continue;
			}

			//doh, it terminated for no reason
			inflateEnd(&strm);
			if (ret != Z_STREAM_END)
			{
				Con_Printf("Couldn't decompress gz file\n");
				VFS_CLOSE(temp);
				VFS_CLOSE(infile);
				return NULL;
			}
		}
		//we got to the end
		VFS_WRITE(temp, outbuffer, strm.total_out);

		inflateEnd(&strm);

		VFS_SEEK(temp, 0);
	}
	VFS_CLOSE(infile);

	return temp;
}
#endif

vfsfile_t *VFS_Filter(char *filename, vfsfile_t *handle)
{
//	char *ext;

	if (!handle || handle->WriteBytes || handle->seekingisabadplan)	//only on readonly files
		return handle;
//	ext = COM_FileExtension (filename);
#ifdef AVAIL_ZLIB
//	if (!stricmp(ext, ".gz"))
	{
		gzheader_t gzh;
		if (VFS_READ(handle, &gzh, sizeofgzheader_t) == sizeofgzheader_t)
		{
			if (gzh.ident1 == 0x1f && gzh.ident2 == 0x8b && gzh.cm == 8)
			{	//it'll do
				return FS_DecompressGZip(handle, &gzh);
			}
		}
		VFS_SEEK(handle, 0);
	}
#endif
	return handle;
}

vfsfile_t *FS_OpenVFS(char *filename, char *mode, int relativeto)
{
	char fullname[MAX_OSPATH];
	flocation_t loc;
	vfsfile_t *vfs;

	//eventually, this function will be the *ONLY* way to get at files

	//blanket-bans

	if (Sys_PathProtection(filename) )
		return NULL;


	if (strcmp(mode, "rb"))
		if (strcmp(mode, "wb"))
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
		return VFSOS_Open(fullname, mode);
	return NULL;
}

vfsfile_t *FS_OpenReadLocation(flocation_t *location)
{
	if (location->search)
	{
		com_file_copyprotected = location->search->copyprotected;
		return VFS_Filter(NULL, location->search->funcs->OpenVFS(location->search->handle, location, "rb"));
	}
	return NULL;
}

int FS_Rename2(char *oldf, char *newf, int oldrelativeto, int newrelativeto)
{
	char oldfullname[MAX_OSPATH];
	char newfullname[MAX_OSPATH];

	switch (oldrelativeto)
	{
	case FS_GAME:
		if (*com_homedir)
			snprintf(oldfullname, sizeof(oldfullname), "%s%s/", com_homedir, gamedirfile);
		else
			snprintf(oldfullname, sizeof(oldfullname), "%s%s/", com_quakedir, gamedirfile);
		break;
	case FS_SKINS:
		if (*com_homedir)
			snprintf(oldfullname, sizeof(oldfullname), "%sqw/skins/", com_homedir);
		else
			snprintf(oldfullname, sizeof(oldfullname), "%sqw/skins/", com_quakedir);
		break;
	case FS_ROOT:
		if (*com_homedir)
			snprintf(oldfullname, sizeof(oldfullname), "%s", com_homedir);
		else
			snprintf(oldfullname, sizeof(oldfullname), "%s", com_quakedir);
		break;
	default:
		Sys_Error("FS_Rename case not handled\n");
	}

	switch (newrelativeto)
	{
	case FS_GAME:
		if (*com_homedir)
			snprintf(newfullname, sizeof(newfullname), "%s%s/", com_homedir, gamedirfile);
		else
			snprintf(newfullname, sizeof(newfullname), "%s%s/", com_quakedir, gamedirfile);
		break;
	case FS_SKINS:
		if (*com_homedir)
			snprintf(newfullname, sizeof(newfullname), "%sqw/skins/", com_homedir);
		else
			snprintf(newfullname, sizeof(newfullname), "%sqw/skins/", com_quakedir);
		break;
	case FS_ROOT:
		if (*com_homedir)
			snprintf(newfullname, sizeof(newfullname), "%s", com_homedir);
		else
			snprintf(newfullname, sizeof(newfullname), "%s", com_quakedir);
		break;
	default:
		Sys_Error("FS_Rename case not handled\n");
	}

	Q_strncatz(oldfullname, oldf, sizeof(oldfullname));
	Q_strncatz(newfullname, newf, sizeof(newfullname));

	FS_CreatePath(newf, newrelativeto);
	return rename(oldfullname, newfullname);
}
int FS_Rename(char *oldf, char *newf, int relativeto)
{
	char oldfullname[MAX_OSPATH];
	char newfullname[MAX_OSPATH];

	switch (relativeto)
	{
	case FS_GAME:
		if (*com_homedir)
			snprintf(oldfullname, sizeof(oldfullname), "%s%s/", com_homedir, gamedirfile);
		else
			snprintf(oldfullname, sizeof(oldfullname), "%s%s/", com_quakedir, gamedirfile);
		break;
	case FS_SKINS:
		if (*com_homedir)
			snprintf(oldfullname, sizeof(oldfullname), "%sqw/skins/", com_homedir);
		else
			snprintf(oldfullname, sizeof(oldfullname), "%sqw/skins/", com_quakedir);
		break;
	case FS_ROOT:
		if (*com_homedir)
			snprintf(oldfullname, sizeof(oldfullname), "%s", com_homedir);
		else
			snprintf(oldfullname, sizeof(oldfullname), "%s", com_quakedir);
		break;
	default:
		Sys_Error("FS_Rename case not handled\n");
	}

	Q_strncpy(newfullname, oldfullname, sizeof(newfullname));
	Q_strncatz(oldfullname, oldf, sizeof(oldfullname));
	Q_strncatz(newfullname, newf, sizeof(newfullname));

	return rename(oldfullname, newfullname);
}
int FS_Remove(char *fname, int relativeto)
{
	char fullname[MAX_OSPATH];

	switch (relativeto)
	{
	case FS_GAME:
		if (*com_homedir)
			snprintf(fullname, sizeof(fullname), "%s%s/%s", com_homedir, gamedirfile, fname);
		else
			snprintf(fullname, sizeof(fullname), "%s%s/%s", com_quakedir, gamedirfile, fname);
		break;
	case FS_SKINS:
		if (*com_homedir)
			snprintf(fullname, sizeof(fullname), "%sqw/skins/%s", com_homedir, fname);
		else
			snprintf(fullname, sizeof(fullname), "%sqw/skins/%s", com_quakedir, fname);
		break;
	case FS_ROOT:
		if (*com_homedir)
			snprintf(fullname, sizeof(fullname), "%s%s", com_homedir, fname);
		else
			snprintf(fullname, sizeof(fullname), "%s%s", com_quakedir, fname);
		break;
	default:
		Sys_Error("FS_Rename case not handled\n");
	}

	return unlink (fullname);
}
void FS_CreatePath(char *pname, int relativeto)
{
	char fullname[MAX_OSPATH];
	switch (relativeto)
	{
	case FS_GAMEONLY:
	case FS_GAME:
		snprintf(fullname, sizeof(fullname), "%s/%s", com_gamedir, pname);
		break;
	case FS_ROOT:
		if (*com_homedir)
			snprintf(fullname, sizeof(fullname), "%s%s", com_homedir, pname);
		else
			snprintf(fullname, sizeof(fullname), "%s%s", com_quakedir, pname);
		break;
	case FS_SKINS:
		if (*com_homedir)
			snprintf(fullname, sizeof(fullname), "%sqw/skins/%s", com_homedir, pname);
		else
			snprintf(fullname, sizeof(fullname), "%sqw/skins/%s", com_quakedir, pname);
		break;
	case FS_CONFIGONLY:
		if (*com_homedir)
			snprintf(fullname, sizeof(fullname), "%sfte/%s", com_homedir, pname);
		else
			snprintf(fullname, sizeof(fullname), "%sfte/%s", com_quakedir, pname);
		break;
	default:
		Sys_Error("FS_CreatePath: Bad relative path (%i)", relativeto);
		break;
	}
	COM_CreatePath(fullname);
}

qboolean FS_WriteFile (char *filename, void *data, int len, int relativeto)
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
#ifndef SERVERONLY
	if (qrenderer)
		if (Draw_BeginDisc)
			Draw_BeginDisc ();
#endif

	VFS_READ(f, buf, len);
	VFS_CLOSE(f);

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



typedef struct {
	searchpathfuncs_t *funcs;
	searchpath_t *parentpath;
	char *parentdesc;
	char *puredesc;
} wildpaks_t;

static int COM_AddWildDataFiles (char *descriptor, int size, void *vparam)
{
	wildpaks_t *param = vparam;
	vfsfile_t *vfs;
	searchpathfuncs_t *funcs = param->funcs;
	searchpath_t	*search;
	pack_t			*pak;
	char			pakfile[MAX_OSPATH];
	char			purefile[MAX_OSPATH];
	flocation_t loc;

	sprintf (pakfile, "%s%s", param->parentdesc, descriptor);

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
	pak = funcs->OpenNew (vfs, pakfile);
	if (!pak)
		return true;

	sprintf (pakfile, "%s%s/", param->parentdesc, descriptor);
	if (*param->puredesc)
		snprintf (purefile, sizeof(purefile), "%s/%s", param->puredesc, descriptor);
	else
		Q_strncpyz(purefile, descriptor, sizeof(purefile));
	COM_AddPathHandle(purefile, pakfile, funcs, pak, true, false, (unsigned int)-1);

	return true;
}


static void COM_AddDataFiles(char *purepath, char *pathto, searchpath_t *search, char *extension, searchpathfuncs_t *funcs)
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
		Con_Printf("Opened %s\n", pakfile);
		handle = funcs->OpenNew (vfs, pakfile);
		if (!handle)
			break;
		snprintf (pakfile, sizeof(pakfile), "%spak%i.%s/", pathto, i, extension);
		snprintf (purefile, sizeof(pakfile), "%spak%i.%s", purepath, i, extension);
		COM_AddPathHandle(purefile, pakfile, funcs, handle, true, false, (unsigned int)-1);
	}

	//now load the random ones
	sprintf (pakfile, "*.%s", extension);
	wp.funcs = funcs;
	wp.parentdesc = pathto;
	wp.parentpath = search;
	wp.puredesc = purepath;
	search->funcs->EnumerateFiles(search->handle, pakfile, COM_AddWildDataFiles, &wp);
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
void COM_AddGameDirectory (char *puredir, char *dir, unsigned int loadstuff)
{
	searchpath_t	*search;

	char			*p;

	if ((p = strrchr(dir, '/')) != NULL)
		strcpy(gamedirfile, ++p);
	else
		strcpy(gamedirfile, dir);
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
	COM_AddPathHandle((*dir?puredir:""), va("%s/", dir), &osfilefuncs, p, false, false, loadstuff);
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


/*
================
COM_Gamedir

Sets the gamedir and path to a different directory.
================
*/
void COM_Gamedir (char *dir)
{
	searchpath_t	*next;

	if (!*dir || strstr(dir, "..") || strstr(dir, "/")
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
		next = com_searchpaths->next;
		Z_Free (com_searchpaths);
		com_searchpaths = next;
	}

	com_fschanged = true;

	//
	// flush all data, so it will be forced to reload
	//
	Cache_Flush ();

	COM_AddGameDirectory(dir, va("%s%s", com_quakedir, dir), (unsigned int)-1);
	if (*com_homedir)
		COM_AddGameDirectory(dir, va("%s%s", com_homedir, dir), (unsigned int)-1);


#ifndef SERVERONLY
	if (!isDedicated)
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
							"cl_warncmd 1\n", RESTRICT_LOCAL, false);
		}
	}

#ifdef Q3SHADERS
	{
		extern void Shader_Init(void);
		Shader_Init();	//FIXME!
	}
#endif

	COM_Effectinfo_Clear();

	Validation_FlushFileList();	//prevent previous hacks from making a difference.

	//FIXME: load new palette, if different cause a vid_restart.

#endif
}

/*
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
*/

#define NEXCFG "set sv_maxairspeed \"400\"\nset sv_mintic \"0.01\"\ncl_nolerp 0\n"

typedef struct {
	char *gamename;	//sent to the master server when this is the current gamemode.
	char *exename;	//used if the exe name contains this
	char *argname;	//used if this was used as a parameter.
	char *auniquefile;	//used if this file is relative from the gamedir

	char *customexec;

	char *dir1;
	char *dir2;
	char *dir3;
	char *dir4;
} gamemode_info_t;
gamemode_info_t gamemode_info[] = {
//note that there is no basic 'fte' gamemode, this is because we aim for network compatability. Darkplaces-Quake is the closest we get.
//this is to avoid having too many gamemodes anyway.

//rogue/hipnotic have no special files - the detection conflicts and stops us from running regular quake
	{"Darkplaces-Quake",	"darkplaces",	"-quake",		"id1/pak0.pak",		NULL,	"id1",		"qw",				"fte"},
	{"Darkplaces-Hipnotic",	"hipnotic",		"-hipnotic",	NULL,				NULL,	"id1",		"qw",	"hipnotic",	"fte"},
	{"Darkplaces-Rogue",	"rogue",		"-rogue",		NULL,				NULL,	"id1",		"qw",	"rogue",	"fte"},
	{"Nexuiz",				"nexuiz",		"-nexuiz",		"nexuiz.exe",		NEXCFG,	"id1",		"qw",	"data",		"fte"},

	//supported commercial mods (some are currently only partially supported)
	{"FTE-Hexen2",			"hexen",		"-hexen2",		"data1/pak0.pak",	NULL,	"data1",						"fte"},
	{"FTE-Quake2",			"q2",			"-q2",			"baseq2/pak0.pak",	NULL,	"baseq2",						"fte"},
	{"FTE-Quake3",			"q3",			"-q3",			"baseq3/pak0.pk3",	NULL,	"baseq3",						"fte"},

	{"FTE-JK2",				"jk2",			"-jk2",			"base/assets0.pk3",	NULL,	"base",							"fte"},

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
		if (com_purepaths)
			Con_Printf("Pure FS deactivated\n");
		com_purepaths = NULL;
		FS_FlushFSHash();
		return;
	}
	if (!com_purepaths)
		Con_Printf("Pure FS activated\n");

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

	while(oldpaths)
	{
		next = oldpaths->nextpure;

		if (oldbase == oldpaths)
			com_base_searchpaths = com_searchpaths;

		if (oldpaths->funcs == &osfilefuncs)
			COM_AddGameDirectory(oldpaths->purepath, oldpaths->handle, reloadflags);

		oldpaths->funcs->ClosePath(oldpaths->handle);
		Z_Free(oldpaths);
		oldpaths = next;
	}

	if (!com_base_searchpaths)
		com_base_searchpaths = com_searchpaths;
}

void FS_UnloadPackFiles(void)
{
	FS_ReloadPackFilesFlags(1);
}

void FS_ReloadPackFiles(void)
{
	FS_ReloadPackFilesFlags((unsigned int)-1);
}

void FS_ReloadPackFiles_f(void)
{
	if (atoi(Cmd_Argv(1)))
		FS_ReloadPackFilesFlags(atoi(Cmd_Argv(1)));
	else
		FS_ReloadPackFilesFlags((unsigned int)-1);
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
	qboolean usehome;

	int gamenum=-1;

	Cmd_AddCommand("fs_restart", FS_ReloadPackFiles_f);

//
// -basedir <path>
// Overrides the system supplied base directory (under id1)
//
	i = COM_CheckParm ("-basedir");
	if (i && i < com_argc-1)
		strcpy (com_quakedir, com_argv[i+1]);
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



	Cvar_Register(&com_gamename, "evil hacks");
	//identify the game from a telling file
	for (i = 0; gamemode_info[i].gamename; i++)
	{
		if (!gamemode_info[i].auniquefile)
			continue;	//no more
		f = fopen(va("%s%s", com_quakedir, gamemode_info[i].auniquefile), "rb");
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

	if (gamemode_info[gamenum].customexec)
		Cbuf_AddText(gamemode_info[gamenum].customexec, RESTRICT_LOCAL);

	usehome = false;

#ifdef _WIN32
	{	//win32 sucks.
		HMODULE shfolder = LoadLibrary("shfolder.dll");
		HMODULE advapi32;
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
			FreeLibrary(shfolder);
		}

		if (winver >= 0x6) // Windows Vista and above
			usehome = true; // always use home directory by default, as Vista+ mimics this behavior anyway
		else if (winver >= 0x5) // Windows 2000/XP/2003
		{
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

		if (!*com_homedir)
		{
			ev = getenv("USERPROFILE");
			if (ev)
				Q_snprintfz(com_homedir, sizeof(com_homedir), "%s/My Documents/My Games/%s/", ev, FULLENGINENAME);
		}
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

//
// start up with id1 by default
//
	i = COM_CheckParm ("-basegame");
	if (i && i < com_argc-1)
	{
		do	//use multiple -basegames
		{
			COM_AddGameDirectory (com_argv[i+1], va("%s%s", com_quakedir, com_argv[i+1]), (unsigned int)-1);

			i = COM_CheckNextParm ("-basegame", i);
		}
		while (i && i < com_argc-1);
	}
	else
	{
		if (gamemode_info[gamenum].dir1)
			COM_AddGameDirectory (gamemode_info[gamenum].dir1, va("%s%s", com_quakedir, gamemode_info[gamenum].dir1), (unsigned int)-1);
		if (gamemode_info[gamenum].dir2)
			COM_AddGameDirectory (gamemode_info[gamenum].dir2, va("%s%s", com_quakedir, gamemode_info[gamenum].dir2), (unsigned int)-1);
		if (gamemode_info[gamenum].dir3)
			COM_AddGameDirectory (gamemode_info[gamenum].dir3, va("%s%s", com_quakedir, gamemode_info[gamenum].dir3), (unsigned int)-1);
		if (gamemode_info[gamenum].dir4)
			COM_AddGameDirectory (gamemode_info[gamenum].dir4, va("%s%s", com_quakedir, gamemode_info[gamenum].dir4), (unsigned int)-1);
	}

	if (*com_homedir)
		COM_AddGameDirectory ("fte", va("%sfte", com_homedir), (unsigned int)-1);

	// any set gamedirs will be freed up to here
	com_base_searchpaths = com_searchpaths;

	i = COM_CheckParm ("-game");	//effectivly replace with +gamedir x (But overridable)
	if (i && i < com_argc-1)
	{
		COM_AddGameDirectory (com_argv[i+1], va("%s%s", com_quakedir, com_argv[i+1]), (unsigned int)-1);

#ifndef CLIENTONLY
		Info_SetValueForStarKey (svs.info, "*gamedir", com_argv[i+1], MAX_SERVERINFO_STRING);
#endif
	}
}

