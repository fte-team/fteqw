#include "quakedef.h"
#include "fs.h"

typedef struct {
	vfsfile_t funcs;
	FILE *handle;
} vfsosfile_t;
int VFSOS_ReadBytes (struct vfsfile_s *file, void *buffer, int bytestoread)
{
	vfsosfile_t *intfile = (vfsosfile_t*)file;
	return fread(buffer, 1, bytestoread, intfile->handle);
}
int VFSOS_WriteBytes (struct vfsfile_s *file, const void *buffer, int bytestoread)
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
void VFSOS_Flush(struct vfsfile_s *file)
{
	vfsosfile_t *intfile = (vfsosfile_t*)file;
	fflush(intfile->handle);
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
	file->funcs.Flush = VFSOS_Flush;
	file->handle = f;

	return (vfsfile_t*)file;
}

vfsfile_t *VFSOS_Open(const char *osname, const char *mode)
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
	file->funcs.Flush = VFSOS_Flush;
	file->handle = f;

	return (vfsfile_t*)file;
}

vfsfile_t *FSOS_OpenVFS(void *handle, flocation_t *loc, const char *mode)
{
	char diskname[MAX_OSPATH];

	//path is already cleaned, as anything that gets a valid loc needs cleaning up first.

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
int FSOS_RebuildFSHash(const char *filename, int filesize, void *data)
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
qboolean FSOS_FLocate(void *handle, flocation_t *loc, const char *filename, void *hashedresult)
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
int FSOS_EnumerateFiles (void *handle, const char *match, int (*func)(const char *, int, void *), void *parm)
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
