#include "quakedef.h"
#include "fs.h"
#include "errno.h"

#ifdef WEBSVONLY
#define Z_Free free
#define Z_Malloc malloc
#else
#if !defined(_WIN32) || defined(_SDL)
#define VFSSTDIO_Open VFSOS_Open
#define stdiofilefuncs osfilefuncs
#endif
#define FSSTDIO_OpenTemp FS_OpenTemp
#endif

typedef struct {
	vfsfile_t funcs;
	FILE *handle;
} vfsstdiofile_t;
static int VFSSTDIO_ReadBytes (struct vfsfile_s *file, void *buffer, int bytestoread)
{
	vfsstdiofile_t *intfile = (vfsstdiofile_t*)file;
	return fread(buffer, 1, bytestoread, intfile->handle);
}
static int VFSSTDIO_WriteBytes (struct vfsfile_s *file, const void *buffer, int bytestoread)
{
	vfsstdiofile_t *intfile = (vfsstdiofile_t*)file;
	return fwrite(buffer, 1, bytestoread, intfile->handle);
}
static qboolean VFSSTDIO_Seek (struct vfsfile_s *file, unsigned long pos)
{
	vfsstdiofile_t *intfile = (vfsstdiofile_t*)file;
	return fseek(intfile->handle, pos, SEEK_SET) == 0;
}
static unsigned long VFSSTDIO_Tell (struct vfsfile_s *file)
{
	vfsstdiofile_t *intfile = (vfsstdiofile_t*)file;
	return ftell(intfile->handle);
}
static void VFSSTDIO_Flush(struct vfsfile_s *file)
{
	vfsstdiofile_t *intfile = (vfsstdiofile_t*)file;
	fflush(intfile->handle);
}
static unsigned long VFSSTDIO_GetSize (struct vfsfile_s *file)
{
	vfsstdiofile_t *intfile = (vfsstdiofile_t*)file;

	unsigned int curpos;
	unsigned int maxlen;
	curpos = ftell(intfile->handle);
	fseek(intfile->handle, 0, SEEK_END);
	maxlen = ftell(intfile->handle);
	fseek(intfile->handle, curpos, SEEK_SET);

	return maxlen;
}
static void VFSSTDIO_Close(vfsfile_t *file)
{
	vfsstdiofile_t *intfile = (vfsstdiofile_t*)file;
	fclose(intfile->handle);
	Z_Free(file);
}

vfsfile_t *FSSTDIO_OpenTemp(void)
{
	FILE *f;
	vfsstdiofile_t *file;

	f = tmpfile();
	if (!f)
		return NULL;

	file = Z_Malloc(sizeof(vfsstdiofile_t));
	file->funcs.ReadBytes = VFSSTDIO_ReadBytes;
	file->funcs.WriteBytes = VFSSTDIO_WriteBytes;
	file->funcs.Seek = VFSSTDIO_Seek;
	file->funcs.Tell = VFSSTDIO_Tell;
	file->funcs.GetLen = VFSSTDIO_GetSize;
	file->funcs.Close = VFSSTDIO_Close;
	file->funcs.Flush = VFSSTDIO_Flush;
	file->handle = f;

	return (vfsfile_t*)file;
}

#ifdef ANDROID
vfsfile_t *Sys_OpenAsset(const char *fname);
#endif

vfsfile_t *VFSSTDIO_Open(const char *osname, const char *mode)
{
	FILE *f;
	vfsstdiofile_t *file;
	qboolean read = !!strchr(mode, 'r');
	qboolean write = !!strchr(mode, 'w');
	qboolean append = !!strchr(mode, 'a');
	qboolean text = !!strchr(mode, 't');
	char newmode[3];
	int modec = 0;

#ifdef ANDROID
//	if (!strncmp("asset/", osname, 6))
	{
		if (append || write)
			return NULL;
		return Sys_OpenAsset(osname);
	}
#endif

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

	file = Z_Malloc(sizeof(vfsstdiofile_t));
	file->funcs.ReadBytes = strchr(mode, 'r')?VFSSTDIO_ReadBytes:NULL;
	file->funcs.WriteBytes = (strchr(mode, 'w')||strchr(mode, 'a'))?VFSSTDIO_WriteBytes:NULL;
	file->funcs.Seek = VFSSTDIO_Seek;
	file->funcs.Tell = VFSSTDIO_Tell;
	file->funcs.GetLen = VFSSTDIO_GetSize;
	file->funcs.Close = VFSSTDIO_Close;
	file->funcs.Flush = VFSSTDIO_Flush;
	file->handle = f;

	return (vfsfile_t*)file;
}


#ifndef WEBSVONLY
static vfsfile_t *FSSTDIO_OpenVFS(void *handle, flocation_t *loc, const char *mode)
{
	char diskname[MAX_OSPATH];

	//path is already cleaned, as anything that gets a valid loc needs cleaning up first.

	snprintf(diskname, sizeof(diskname), "%s/%s", (char*)handle, loc->rawname);

	return VFSOS_Open(diskname, mode);
}

static void FSSTDIO_PrintPath(void *handle)
{
	Con_Printf("%s\n", (char*)handle);
}
static void FSSTDIO_ClosePath(void *handle)
{
	Z_Free(handle);
}
static int FSSTDIO_RebuildFSHash(const char *filename, int filesize, void *data)
{
	if (filename[strlen(filename)-1] == '/')
	{	//this is actually a directory

		char childpath[256];
		Q_snprintfz(childpath, sizeof(childpath), "%s*", filename);
		Sys_EnumerateFiles((char*)data, childpath, FSSTDIO_RebuildFSHash, data);
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
static void FSSTDIO_BuildHash(void *handle)
{
	Sys_EnumerateFiles(handle, "*", FSSTDIO_RebuildFSHash, handle);
}
static qboolean FSSTDIO_FLocate(void *handle, flocation_t *loc, const char *filename, void *hashedresult)
{
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

#ifdef ANDROID
	{
		vfsfile_t *f = VFSSTDIO_Open(netpath, "rb");
		if (!f)
			return false;
		len = VFS_GETLEN(f);
		VFS_CLOSE(f);
	}
#else
	{
		FILE *f = fopen(netpath, "rb");
		if (!f)
			return false;

		fseek(f, 0, SEEK_END);
		len = ftell(f);
		fclose(f);
	}
#endif
	if (loc)
	{
		loc->len = len;
		loc->offset = 0;
		loc->index = 0;
		Q_strncpyz(loc->rawname, filename, sizeof(loc->rawname));
	}

	return true;
}
static void FSSTDIO_ReadFile(void *handle, flocation_t *loc, char *buffer)
{
	FILE *f;
	size_t result;

	f = fopen(loc->rawname, "rb");
	if (!f)	//err...
		return;
	fseek(f, loc->offset, SEEK_SET);
	result = fread(buffer, 1, loc->len, f); // do soemthing with result

	if (result != loc->len)
		Con_Printf("FSSTDIO_ReadFile() fread: Filename: %s, expected %i, result was %u (%s)\n",loc->rawname,loc->len,(unsigned int)result,strerror(errno));

	fclose(f);
}
static int FSSTDIO_EnumerateFiles (void *handle, const char *match, int (*func)(const char *, int, void *), void *parm)
{
	return Sys_EnumerateFiles(handle, match, func, parm);
}

searchpathfuncs_t stdiofilefuncs = {
	FSSTDIO_PrintPath,
	FSSTDIO_ClosePath,
	FSSTDIO_BuildHash,
	FSSTDIO_FLocate,
	FSSTDIO_ReadFile,
	FSSTDIO_EnumerateFiles,
	NULL,
	NULL,
	FSSTDIO_OpenVFS
};
#endif
