#include "quakedef.h"
#include "fs.h"
#include "errno.h"

#ifndef NACL

#ifdef WEBSVONLY
#define Z_Free free
#define Z_Malloc malloc
#else
#if !defined(_WIN32) || defined(_SDL)
#define stdiofilefuncs osfilefuncs
#endif
#define FSSTDIO_OpenTemp FS_OpenTemp
#endif

typedef struct {
	int depth;
	void (QDECL *AddFileHash)(int depth, const char *fname, fsbucket_t *filehandle, void *pathhandle);
	char rootpath[1];
} stdiopath_t;
typedef struct {
	vfsfile_t funcs;
	FILE *handle;
} vfsstdiofile_t;
static int QDECL VFSSTDIO_ReadBytes (struct vfsfile_s *file, void *buffer, int bytestoread)
{
	vfsstdiofile_t *intfile = (vfsstdiofile_t*)file;
	return fread(buffer, 1, bytestoread, intfile->handle);
}
static int QDECL VFSSTDIO_WriteBytes (struct vfsfile_s *file, const void *buffer, int bytestoread)
{
	vfsstdiofile_t *intfile = (vfsstdiofile_t*)file;
	return fwrite(buffer, 1, bytestoread, intfile->handle);
}
static qboolean QDECL VFSSTDIO_Seek (struct vfsfile_s *file, unsigned long pos)
{
	vfsstdiofile_t *intfile = (vfsstdiofile_t*)file;
	return fseek(intfile->handle, pos, SEEK_SET) == 0;
}
static unsigned long QDECL VFSSTDIO_Tell (struct vfsfile_s *file)
{
	vfsstdiofile_t *intfile = (vfsstdiofile_t*)file;
	return ftell(intfile->handle);
}
static void QDECL VFSSTDIO_Flush(struct vfsfile_s *file)
{
	vfsstdiofile_t *intfile = (vfsstdiofile_t*)file;
	fflush(intfile->handle);
}
static unsigned long QDECL VFSSTDIO_GetSize (struct vfsfile_s *file)
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
static void QDECL VFSSTDIO_Close(vfsfile_t *file)
{
	vfsstdiofile_t *intfile = (vfsstdiofile_t*)file;
	fclose(intfile->handle);
	Z_Free(file);
}

#ifdef _WIN32
static void QDECL VFSSTDIO_CloseTemp(vfsfile_t *file)
{
	vfsstdiofile_t *intfile = (vfsstdiofile_t*)file;
	char *fname = (char*)(intfile+1); 
	fclose(intfile->handle);
	_unlink(fname);
	Z_Free(file);
}
#endif

vfsfile_t *FSSTDIO_OpenTemp(void)
{
	FILE *f;
	vfsstdiofile_t *file;

#ifdef _WIN32
	/*warning: annother app might manage to open the file before we can. if the file is not opened exclusively then we can end up with issues
	on windows, fopen is typically exclusive anyway, but not on unix. but on unix, tmpfile is actually usable, so special-case the windows code*/
	char *fname = _tempnam(NULL, "ftemp");
	f = fopen(fname, "w+b");
	if (!f)
		return NULL;

	file = Z_Malloc(sizeof(vfsstdiofile_t) + strlen(fname)+1);
	file->funcs.Close = VFSSTDIO_CloseTemp;
	strcpy((char*)(file+1), fname);
	free(fname);
#else
	f = tmpfile();
	if (!f)
		return NULL;

	file = Z_Malloc(sizeof(vfsstdiofile_t));
	file->funcs.Close = VFSSTDIO_Close;
#endif
#ifdef _DEBUG
	Q_strncpyz(file->funcs.dbgname, "FSSTDIO_OpenTemp", sizeof(file->funcs.dbgname));
#endif
	file->funcs.ReadBytes = VFSSTDIO_ReadBytes;
	file->funcs.WriteBytes = VFSSTDIO_WriteBytes;
	file->funcs.Seek = VFSSTDIO_Seek;
	file->funcs.Tell = VFSSTDIO_Tell;
	file->funcs.GetLen = VFSSTDIO_GetSize;
	file->funcs.Flush = VFSSTDIO_Flush;
	file->handle = f;

	return (vfsfile_t*)file;
}

#if 0//def ANDROID
vfsfile_t *Sys_OpenAsset(const char *fname);
#endif

vfsfile_t *VFSSTDIO_Open(const char *osname, const char *mode, qboolean *needsflush)
{
	FILE *f;
	vfsstdiofile_t *file;
	qboolean read = !!strchr(mode, 'r');
	qboolean write = !!strchr(mode, 'w');
	qboolean append = !!strchr(mode, 'a');
	qboolean text = !!strchr(mode, 't');
	char newmode[3];
	int modec = 0;

	if (needsflush)
		*needsflush = false;

#if 0//def ANDROID
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

	if (write || append)
	{
		if (needsflush)
			*needsflush = true;
	}

	file = Z_Malloc(sizeof(vfsstdiofile_t));
#ifdef _DEBUG
	Q_strncpyz(file->funcs.dbgname, osname, sizeof(file->funcs.dbgname));
#endif
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

#if !defined(_WIN32) || defined(_SDL)
vfsfile_t *VFSOS_Open(const char *osname, const char *mode)
{
	vfsfile_t *f;
	qboolean needsflush;
	f = VFSSTDIO_Open(osname, mode, &needsflush);
	if (needsflush)
		FS_FlushFSHashReally();
	return f;
}
#endif

#ifndef WEBSVONLY
static vfsfile_t *QDECL FSSTDIO_OpenVFS(void *handle, flocation_t *loc, const char *mode)
{
	vfsfile_t *f;
	stdiopath_t *sp = handle;
	char diskname[MAX_OSPATH];
	qboolean needsflush;

	f = VFSSTDIO_Open(loc->rawname, mode, &needsflush);
	if (needsflush && sp->AddFileHash)
		sp->AddFileHash(sp->depth, loc->rawname, NULL, sp);
	return f;
}

static void QDECL FSSTDIO_GetDisplayPath(void *handle, char *out, unsigned int outlen)
{
	stdiopath_t *np = handle;
	Q_strncpyz(out, np->rootpath, outlen);
}
static void QDECL FSSTDIO_ClosePath(void *handle)
{
	Z_Free(handle);
}
static qboolean QDECL FSSTDIO_PollChanges(void *handle)
{
//	stdiopath_t *np = handle;
	return true;	//can't verify that or not, so we have to assume the worst
}
static void *QDECL FSSTDIO_OpenPath(vfsfile_t *mustbenull, const char *desc)
{
	stdiopath_t *np;
	int dlen = strlen(desc);
	if (mustbenull)
		return NULL;
	np = Z_Malloc(sizeof(*np) + dlen);
	if (np)
	{
		np->depth = 0;
		memcpy(np->rootpath, desc, dlen+1);
	}
	return np;
}
static int QDECL FSSTDIO_RebuildFSHash(const char *filename, int filesize, void *data, void *spath)
{
	stdiopath_t *sp = spath;
	void (QDECL *AddFileHash)(int depth, const char *fname, fsbucket_t *filehandle, void *pathhandle) = data;
	if (filename[strlen(filename)-1] == '/')
	{	//this is actually a directory

		char childpath[256];
		Q_snprintfz(childpath, sizeof(childpath), "%s*", filename);
		Sys_EnumerateFiles(sp->rootpath, childpath, FSSTDIO_RebuildFSHash, data, spath);
		return true;
	}
	AddFileHash(sp->depth, filename, NULL, sp);
	return true;
}
static void QDECL FSSTDIO_BuildHash(void *handle, int depth, void (QDECL *AddFileHash)(int depth, const char *fname, fsbucket_t *filehandle, void *pathhandle))
{
	stdiopath_t *sp = handle;
	sp->depth = depth;
	sp->AddFileHash = AddFileHash;
	Sys_EnumerateFiles(sp->rootpath, "*", FSSTDIO_RebuildFSHash, AddFileHash, handle);
}
static qboolean QDECL FSSTDIO_FLocate(void *handle, flocation_t *loc, const char *filename, void *hashedresult)
{
	stdiopath_t *sp = handle;
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
	snprintf (netpath, sizeof(netpath)-1, "%s/%s", sp->rootpath, filename);

#ifdef ANDROID
	{
		vfsfile_t *f = VFSSTDIO_Open(netpath, "rb", NULL);
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
		Q_strncpyz(loc->rawname, netpath, sizeof(loc->rawname));
	}

	return true;
}
static void QDECL FSSTDIO_ReadFile(void *handle, flocation_t *loc, char *buffer)
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
static int QDECL FSSTDIO_EnumerateFiles (void *handle, const char *match, int (QDECL *func)(const char *, int, void *, void *spath), void *parm)
{
	stdiopath_t *sp = handle;
	return Sys_EnumerateFiles(sp->rootpath, match, func, parm, handle);
}

searchpathfuncs_t stdiofilefuncs = {
	FSSTDIO_GetDisplayPath,
	FSSTDIO_ClosePath,
	FSSTDIO_BuildHash,
	FSSTDIO_FLocate,
	FSSTDIO_ReadFile,
	FSSTDIO_EnumerateFiles,
	FSSTDIO_OpenPath,
	NULL,
	FSSTDIO_OpenVFS,
	FSSTDIO_PollChanges
};
#endif
#endif

