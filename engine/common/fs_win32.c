#include "quakedef.h"
#include "fs.h"
#include <windows.h>

#ifndef INVALID_SET_FILE_POINTER
#define INVALID_SET_FILE_POINTER ~0
#endif

//read-only memory mapped files.
//for write access, we use the stdio module as a fallback.

#define VFSW32_Open VFSOS_Open
#define w32filefuncs osfilefuncs

typedef struct {
	vfsfile_t funcs;
	HANDLE hand;
	HANDLE mmh;
	void *mmap;
	unsigned int length;
	unsigned int offset;
} vfsw32file_t;
static int VFSW32_ReadBytes (struct vfsfile_s *file, void *buffer, int bytestoread)
{
	DWORD read;
	vfsw32file_t *intfile = (vfsw32file_t*)file;
	if (intfile->mmap)
	{
		if (intfile->offset+bytestoread > intfile->length)
			bytestoread = intfile->length-intfile->offset;

		memcpy(buffer, (char*)intfile->mmap + intfile->offset, bytestoread);
		intfile->offset += bytestoread;
		return bytestoread;
	}
	if (!ReadFile(intfile->hand, buffer, bytestoread, &read, NULL))
		return 0;
	return read;
}
static int VFSW32_WriteBytes (struct vfsfile_s *file, const void *buffer, int bytestoread)
{
	DWORD written;
	vfsw32file_t *intfile = (vfsw32file_t*)file;
	if (intfile->mmap)
	{
		if (intfile->offset+bytestoread > intfile->length)
			bytestoread = intfile->length-intfile->offset;

		memcpy((char*)intfile->mmap + intfile->offset, buffer, bytestoread);
		intfile->offset += bytestoread;
		return bytestoread;
	}

	if (!WriteFile(intfile->hand, buffer, bytestoread, &written, NULL))
		return 0;
	return written;
}
static qboolean VFSW32_Seek (struct vfsfile_s *file, unsigned long pos)
{
	vfsw32file_t *intfile = (vfsw32file_t*)file;
	if (intfile->mmap)
	{
		intfile->offset = pos;
		return true;
	}

	return SetFilePointer(intfile->hand, pos, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER;
}
static unsigned long VFSW32_Tell (struct vfsfile_s *file)
{
	vfsw32file_t *intfile = (vfsw32file_t*)file;
	if (intfile->mmap)
		return intfile->offset;
	return SetFilePointer(intfile->hand, 0, NULL, FILE_CURRENT);
}
static void VFSW32_Flush(struct vfsfile_s *file)
{
	vfsw32file_t *intfile = (vfsw32file_t*)file;
	if (intfile->mmap)
		FlushViewOfFile(intfile->mmap, intfile->length);
	FlushFileBuffers(intfile->hand);
}
static unsigned long VFSW32_GetSize (struct vfsfile_s *file)
{
	vfsw32file_t *intfile = (vfsw32file_t*)file;

	if (intfile->mmap)
		return intfile->length;
	return GetFileSize(intfile->hand, NULL);
}
static void VFSW32_Close(vfsfile_t *file)
{
	vfsw32file_t *intfile = (vfsw32file_t*)file;
	if (intfile->mmap)
	{
		UnmapViewOfFile(intfile->mmap);
		CloseHandle(intfile->mmh);
	}
	CloseHandle(intfile->hand);
	Z_Free(file);
}

vfsfile_t *VFSW32_Open(const char *osname, const char *mode)
{
	HANDLE h, mh;
	unsigned int fsize;
	void *mmap;

	vfsw32file_t *file;
	qboolean read = !!strchr(mode, 'r');
	qboolean write = !!strchr(mode, 'w');
	qboolean append = !!strchr(mode, 'a');
	qboolean text = !!strchr(mode, 't');
	write |= append;
	if (strchr(mode, '+'))
		read = write = true;

	if (write && read)
		h = CreateFileA(osname, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_DELETE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	else if (write)
		h = CreateFileA(osname, GENERIC_READ|GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	else if (read)
		h = CreateFileA(osname, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	else
		h = INVALID_HANDLE_VALUE;
	if (h == INVALID_HANDLE_VALUE)
		return NULL;

	if (write || append || text)
	{
		fsize = 0;
		mh = INVALID_HANDLE_VALUE;
		mmap = NULL;
	}
	else
	{
		fsize = GetFileSize(h, NULL);
		mh = CreateFileMapping(h, NULL, PAGE_READONLY, 0, 0, NULL);
		if (mh == INVALID_HANDLE_VALUE)
			mmap = NULL;
		else
		{
			mmap = MapViewOfFile(mh, FILE_MAP_READ, 0, 0, fsize);
			if (mmap == NULL)
			{
				CloseHandle(mh);
				mh = INVALID_HANDLE_VALUE;
			}
		}
	}

	file = Z_Malloc(sizeof(vfsw32file_t));
	file->funcs.ReadBytes = read?VFSW32_ReadBytes:NULL;
	file->funcs.WriteBytes = (write||append)?VFSW32_WriteBytes:NULL;
	file->funcs.Seek = VFSW32_Seek;
	file->funcs.Tell = VFSW32_Tell;
	file->funcs.GetLen = VFSW32_GetSize;
	file->funcs.Close = VFSW32_Close;
	file->funcs.Flush = VFSW32_Flush;
	file->hand = h;
	file->mmh = mh;
	file->mmap = mmap;
	file->offset = 0;
	file->length = fsize;

	return (vfsfile_t*)file;
}

static vfsfile_t *VFSW32_OpenVFS(void *handle, flocation_t *loc, const char *mode)
{
	char diskname[MAX_OSPATH];

	//path is already cleaned, as anything that gets a valid loc needs cleaning up first.

	snprintf(diskname, sizeof(diskname), "%s/%s", (char*)handle, loc->rawname);

	return VFSW32_Open(diskname, mode);
}

static void VFSW32_PrintPath(void *handle)
{
	Con_Printf("%s\n", handle);
}
static void VFSW32_ClosePath(void *handle)
{
	Z_Free(handle);
}
static int VFSW32_RebuildFSHash(const char *filename, int filesize, void *data)
{
	if (filename[strlen(filename)-1] == '/')
	{	//this is actually a directory

		char childpath[256];
		sprintf(childpath, "%s*", filename);
		Sys_EnumerateFiles((char*)data, childpath, VFSW32_RebuildFSHash, data);
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
static void VFSW32_BuildHash(void *handle)
{
	Sys_EnumerateFiles(handle, "*", VFSW32_RebuildFSHash, handle);
}
static qboolean VFSW32_FLocate(void *handle, flocation_t *loc, const char *filename, void *hashedresult)
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
static void VFSW32_ReadFile(void *handle, flocation_t *loc, char *buffer)
{
	FILE *f;
	f = fopen(loc->rawname, "rb");
	if (!f)	//err...
		return;
	fseek(f, loc->offset, SEEK_SET);
	fread(buffer, 1, loc->len, f);
	fclose(f);
}
static int VFSW32_EnumerateFiles (void *handle, const char *match, int (*func)(const char *, int, void *), void *parm)
{
	return Sys_EnumerateFiles(handle, match, func, parm);
}

searchpathfuncs_t w32filefuncs = {
	VFSW32_PrintPath,
	VFSW32_ClosePath,
	VFSW32_BuildHash,
	VFSW32_FLocate,
	VFSW32_ReadFile,
	VFSW32_EnumerateFiles,
	NULL,
	NULL,
	VFSW32_OpenVFS
};
