#include "quakedef.h"
#include "fs.h"

#ifdef AVAIL_ZLIB

#include <zlib.h>
#include "unzip.c"

typedef struct
{
	char	name[MAX_QPATH];
	int		filepos, filelen;

	bucket_t bucket;
} zpackfile_t;


typedef struct zipfile_s
{
	char filename[MAX_OSPATH];
	unzFile handle;
	int		numfiles;
	zpackfile_t	*files;

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
static qboolean FSZIP_FLocate(void *handle, flocation_t *loc, const char *filename, void *hashedresult)
{
	zpackfile_t *pf = hashedresult;
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

static int FSZIP_EnumerateFiles (void *handle, const char *match, int (*func)(const char *, int, void *), void *parm)
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
static void *FSZIP_LoadZipFile (vfsfile_t *packhandle, const char *desc)
{
	int i;
	int nextfileziphandle;

	zipfile_t *zip;
	zpackfile_t		*newfiles;

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

	zip->files = newfiles = Z_Malloc (zip->numfiles * sizeof(zpackfile_t));
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

vfsfile_t *FSZIP_OpenVFS(void *handle, flocation_t *loc, const char *mode)
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

