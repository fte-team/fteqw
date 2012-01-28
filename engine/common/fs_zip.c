#include "quakedef.h"
#include "fs.h"

#ifdef AVAIL_ZLIB

#ifndef ZEXPORT
	#define ZEXPORT VARGS
#endif
#include <zlib.h>

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

#ifdef DYNAMIC_ZLIB
#define ZLIB_LOADED() (zlib_handle!=NULL)
void *zlib_handle;
#define ZSTATIC(n)
#else
#define ZLIB_LOADED() 1
#define ZSTATIC(n) = &n

#ifdef _MSC_VER
# ifdef _WIN64
# pragma comment(lib, MSVCLIBSPATH "zlib64.lib")
# else
# pragma comment(lib, MSVCLIBSPATH "zlib.lib")
# endif
#endif
#endif

//#pragma comment(lib, MSVCLIBSPATH "zlib.lib")

static int (ZEXPORT *qinflateEnd) OF((z_streamp strm)) ZSTATIC(inflateEnd);
static int (ZEXPORT *qinflate) OF((z_streamp strm, int flush)) ZSTATIC(inflate);
static int (ZEXPORT *qinflateInit2_) OF((z_streamp strm, int  windowBits,
                                      const char *version, int stream_size)) ZSTATIC(inflateInit2_);
static uLong (ZEXPORT *qcrc32)   OF((uLong crc, const Bytef *buf, uInt len)) ZSTATIC(crc32);

#define qinflateInit2(strm, windowBits) \
        qinflateInit2_((strm), (windowBits), ZLIB_VERSION, sizeof(z_stream))

qboolean LibZ_Init(void)
{
	#ifdef DYNAMIC_ZLIB
	static dllfunction_t funcs[] =
	{
		{(void*)&qinflateEnd,		"inflateEnd"},
		{(void*)&qinflate,			"inflate"},
		{(void*)&qinflateInit2_,	"inflateInit2_"},
		{(void*)&qcrc32,			"crc32"},
		{NULL, NULL}
	};
	if (!ZLIB_LOADED())
		zlib_handle = Sys_LoadLibrary("zlib1", funcs);
	#endif
	return ZLIB_LOADED();
}

vfsfile_t *FS_DecompressGZip(vfsfile_t *infile)
{
	char inchar;
	unsigned short inshort;
	vfsfile_t *temp;
	gzheader_t header;

	if (VFS_READ(infile, &header, sizeofgzheader_t) == sizeofgzheader_t)
	{
		if (header.ident1 != 0x1f || header.ident2 != 0x8b || header.cm != 8 || header.flags & GZ_RESERVED)
		{
			VFS_SEEK(infile, 0);
			return infile;
		}
	}
	else
	{
		VFS_SEEK(infile, 0);
		return infile;
	}
	if (header.flags & GZ_FEXTRA)
	{
		VFS_READ(infile, &inshort, sizeof(inshort));
		inshort = LittleShort(inshort);
		VFS_SEEK(infile, VFS_TELL(infile) + inshort);
	}

	if (header.flags & GZ_FNAME)
	{
		Con_Printf("gzipped file name: ");
		do {
			if (VFS_READ(infile, &inchar, sizeof(inchar)) != 1)
				break;
			Con_Printf("%c", inchar);
		} while(inchar);
		Con_Printf("\n");
	}

	if (header.flags & GZ_FCOMMENT)
	{
		Con_Printf("gzipped file comment: ");
		do {
			if (VFS_READ(infile, &inchar, sizeof(inchar)) != 1)
				break;
			Con_Printf("%c", inchar);
		} while(inchar);
		Con_Printf("\n");
	}

	if (header.flags & GZ_FHCRC)
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
		unsigned char inbuffer[16384];
		unsigned char outbuffer[16384];
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

		qinflateInit2(&strm, -MAX_WBITS);

		while ((ret=qinflate(&strm, Z_SYNC_FLUSH)) != Z_STREAM_END)
		{
			if (strm.avail_in == 0 || strm.avail_out == 0)
			{
				if (strm.avail_in == 0)
				{
					strm.avail_in = VFS_READ(infile, inbuffer, sizeof(inbuffer));
					strm.next_in = inbuffer;
					if (!strm.avail_in)
						break;
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
			if (ret != Z_STREAM_END)
			{
				qinflateEnd(&strm);
				Con_Printf("Couldn't decompress gz file\n");
				VFS_CLOSE(temp);
				VFS_CLOSE(infile);
				return NULL;
			}
		}
		//we got to the end
		VFS_WRITE(temp, outbuffer, strm.total_out);

		qinflateEnd(&strm);

		VFS_SEEK(temp, 0);
	}
	VFS_CLOSE(infile);

	return temp;
}

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
		if (!*newfiles[i].name || newfiles[i].name[strlen(newfiles[i].name)-1] == '/')
			newfiles[i].filelen = -1;
		else
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

	int result;
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
		result = Com_BlockChecksum(filecrcs, numcrcs*sizeof(int));
	else
		result = Com_BlockChecksum(filecrcs+1, (numcrcs-1)*sizeof(int));

	BZ_Free(filecrcs);
	return result;
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
qboolean VFSZIP_MakeActive(vfszip_t *vfsz)
{
	int i;
	char buffer[8192];	//must be power of two

	if ((vfszip_t*)vfsz->parent->currentfile == vfsz)
		return true;	//already us
	if (vfsz->parent->currentfile)
		unzCloseCurrentFile(vfsz->parent->handle);

	unzLocateFileMy(vfsz->parent->handle, vfsz->index, vfsz->startpos);
	if (unzOpenCurrentFile(vfsz->parent->handle) == UNZ_BADZIPFILE)
	{
		unz_file_info	file_info;
		buffer[0] = '?';
		buffer[1] = 0;
		if (unzGetCurrentFileInfo (vfsz->parent->handle, &file_info, buffer, sizeof(buffer), NULL, 0, NULL, 0) != UNZ_OK)
			Con_Printf("Zip Error\n");
		if (file_info.compression_method && file_info.compression_method != Z_DEFLATED)
			Con_Printf("unsupported compression method on %s/%s\n", vfsz->parent->filename, buffer);
		else
			Con_Printf("corrupt file within zip, %s/%s\n", vfsz->parent->filename, buffer);
		vfsz->parent->currentfile = NULL;
		return false;
	}


	if (vfsz->pos > 0)
	{
		Con_DPrintf("VFSZIP_MakeActive: Shockingly inefficient\n");

		//now we need to seek up to where we had previously gotten to.
		for (i = 0; i < vfsz->pos-sizeof(buffer); i++)
			unzReadCurrentFile(vfsz->parent->handle, buffer, sizeof(buffer));
		unzReadCurrentFile(vfsz->parent->handle, buffer, vfsz->pos - i);
	}

	vfsz->parent->currentfile = (vfsfile_t*)vfsz;
	return true;
}

int VFSZIP_ReadBytes (struct vfsfile_s *file, void *buffer, int bytestoread)
{
	int read;
	vfszip_t *vfsz = (vfszip_t*)file;

	if (vfsz->defer)
		return VFS_READ(vfsz->defer, buffer, bytestoread);

	if (vfsz->iscompressed)
	{
		if (!VFSZIP_MakeActive(vfsz))
			return 0;
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
	if (vfsz->iscompressed)
	{	//if they're going to seek on a file in a zip, let's just copy it out
		char buffer[8192];
		unsigned int chunk;
		unsigned int i;
		unsigned int length;

		vfsz->defer = FS_OpenTemp();
		if (vfsz->defer)
		{
			if (vfsz->pos)
			{
				unzCloseCurrentFile(vfsz->parent->handle);
				vfsz->parent->currentfile = NULL;	//make it not us
			}

			length = vfsz->length;
			i = 0;
			vfsz->pos = 0;
			if (!VFSZIP_MakeActive(vfsz))
			{
				/*shouldn't really happen*/
				VFS_CLOSE(vfsz->defer);
				vfsz->defer = NULL;
				return false;
			}

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

		unzCloseCurrentFile(vfsz->parent->handle);
		vfsz->parent->currentfile = NULL;	//make it not us

		if (vfsz->defer)
			return VFS_SEEK(vfsz->defer, pos);
		else
		{
			unzCloseCurrentFile(vfsz->parent->handle);
			vfsz->parent->currentfile = NULL;	//make it not us, so the next read starts at the right place
		}
	}
	else
	{
		vfsz->parent->currentfile = NULL;
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

	if (loc->len < 0)
		return NULL;

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
	else if (!ZLIB_LOADED())
	{
		Z_Free(vfsz);
		return NULL;
	}
	else if (!VFSZIP_MakeActive(vfsz))	/*this is called purely as a test*/
	{
		/*
		windows explorer tends to use deflate64 on large files, which zlib and thus we, do not support, thus this is a 'common' failure path
		this might also trigger from other errors, of course.
		*/
		Z_Free(vfsz);
		return NULL;
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

