#include "quakedef.h"
#include "fs.h"
#include "errno.h"

#if !defined(NACL) && !defined(FTE_TARGET_WEB)

#ifdef WEBSVONLY
#define Z_Free free
#define Z_Malloc malloc
#else
#if !defined(_WIN32) || defined(FTE_SDL) || defined(WINRT) || defined(_XBOX)
#define FSSTDIO_OpenPath VFSOS_OpenPath
#endif
#define FSSTDIO_OpenTemp FS_OpenTemp
#endif

typedef struct {
	searchpathfuncs_t pub;
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
static qboolean QDECL VFSSTDIO_Seek (struct vfsfile_s *file, qofs_t pos)
{
	vfsstdiofile_t *intfile = (vfsstdiofile_t*)file;
	return fseek(intfile->handle, pos, SEEK_SET) == 0;
}
static qofs_t QDECL VFSSTDIO_Tell (struct vfsfile_s *file)
{
	vfsstdiofile_t *intfile = (vfsstdiofile_t*)file;
	return ftell(intfile->handle);
}
static void QDECL VFSSTDIO_Flush(struct vfsfile_s *file)
{
	vfsstdiofile_t *intfile = (vfsstdiofile_t*)file;
	fflush(intfile->handle);
}
static qofs_t QDECL VFSSTDIO_GetSize (struct vfsfile_s *file)
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
static qboolean QDECL VFSSTDIO_Close(vfsfile_t *file)
{
	qboolean success;
	vfsstdiofile_t *intfile = (vfsstdiofile_t*)file;
	success = !ferror(intfile->handle);
	fclose(intfile->handle);
	Z_Free(file);
	return success;
}

#ifdef _WIN32
static qboolean QDECL VFSSTDIO_CloseTemp(vfsfile_t *file)
{
	qboolean success;
	vfsstdiofile_t *intfile = (vfsstdiofile_t*)file;
	char *fname = (char*)(intfile+1); 
	success = !ferror(intfile->handle);
	fclose(intfile->handle);
	_unlink(fname);
	Z_Free(file);
	return success;
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
	char newmode[5];
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
//	if (append)
//		newmode[modec++] = '+';
	if (text)
		newmode[modec++] = 't';
	else
		newmode[modec++] = 'b';
#ifdef __linux__
	newmode[modec++] = 'e';	//otherwise forks get messy.
#endif
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

#ifndef WEBSVONLY
#if !defined(_WIN32) || defined(FTE_SDL) || defined(WINRT) || defined(_XBOX)
vfsfile_t *VFSOS_Open(const char *osname, const char *mode)
{
	vfsfile_t *f;
	qboolean needsflush;
	f = VFSSTDIO_Open(osname, mode, &needsflush);
	if (needsflush)
		FS_FlushFSHashFull();
	return f;
}
#endif

static vfsfile_t *QDECL FSSTDIO_OpenVFS(searchpathfuncs_t *handle, flocation_t *loc, const char *mode)
{
	vfsfile_t *f;
	stdiopath_t *sp = (void*)handle;
	qboolean needsflush;

	f = VFSSTDIO_Open(loc->rawname, mode, &needsflush);
	if (needsflush && sp->AddFileHash)
		sp->AddFileHash(sp->depth, loc->rawname, NULL, sp);
	return f;
}

static void QDECL FSSTDIO_ClosePath(searchpathfuncs_t *handle)
{
	Z_Free(handle);
}
static qboolean QDECL FSSTDIO_PollChanges(searchpathfuncs_t *handle)
{
//	stdiopath_t *np = handle;
	return true;	//can't verify that or not, so we have to assume the worst
}
static int QDECL FSSTDIO_RebuildFSHash(const char *filename, qofs_t filesize, time_t mtime, void *data, searchpathfuncs_t *spath)
{
	stdiopath_t *sp = (void*)spath;
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
static void QDECL FSSTDIO_BuildHash(searchpathfuncs_t *handle, int depth, void (QDECL *AddFileHash)(int depth, const char *fname, fsbucket_t *filehandle, void *pathhandle))
{
	stdiopath_t *sp = (void*)handle;
	sp->depth = depth;
	sp->AddFileHash = AddFileHash;
	Sys_EnumerateFiles(sp->rootpath, "*", FSSTDIO_RebuildFSHash, AddFileHash, handle);
}
static unsigned int QDECL FSSTDIO_FLocate(searchpathfuncs_t *handle, flocation_t *loc, const char *filename, void *hashedresult)
{
	stdiopath_t *sp = (void*)handle;
	int len;
	char netpath[MAX_OSPATH];

	if (hashedresult && (void *)hashedresult != handle)
		return FF_NOTFOUND;

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
			return FF_NOTFOUND;

		fseek(f, 0, SEEK_END);
		len = ftell(f);
		fclose(f);
	}
#endif
	if (loc)
	{
		loc->len = len;
		loc->offset = 0;
		loc->fhandle = handle;
		Q_strncpyz(loc->rawname, netpath, sizeof(loc->rawname));
	}
	return FF_FOUND;
}
static void QDECL FSSTDIO_ReadFile(searchpathfuncs_t *handle, flocation_t *loc, char *buffer)
{
	FILE *f;
	size_t result;

	f = fopen(loc->rawname, "rb");
	if (!f)	//err...
		return;
	fseek(f, loc->offset, SEEK_SET);
	result = fread(buffer, 1, loc->len, f); // do soemthing with result

	if (result != loc->len)
		Con_Printf("FSSTDIO_ReadFile() fread: Filename: %s, expected %u, result was %u (%s)\n",loc->rawname,(unsigned int)loc->len,(unsigned int)result,strerror(errno));

	fclose(f);
}
static int QDECL FSSTDIO_EnumerateFiles (searchpathfuncs_t *handle, const char *match, int (QDECL *func)(const char *, qofs_t, time_t mtime, void *, searchpathfuncs_t *spath), void *parm)
{
	stdiopath_t *sp = (stdiopath_t*)handle;
	return Sys_EnumerateFiles(sp->rootpath, match, func, parm, handle);
}


searchpathfuncs_t *QDECL FSSTDIO_OpenPath(vfsfile_t *mustbenull, const char *desc, const char *prefix)
{
	stdiopath_t *np;
	int dlen = strlen(desc);
	if (mustbenull)
		return NULL;
	if (prefix && *prefix)
		return NULL;	//don't try to support this. too risky with absolute paths etc.
	np = Z_Malloc(sizeof(*np) + dlen);
	if (np)
	{
		np->depth = 0;
		memcpy(np->rootpath, desc, dlen+1);
	}

	np->pub.fsver			= FSVER;
	np->pub.ClosePath		= FSSTDIO_ClosePath;
	np->pub.BuildHash		= FSSTDIO_BuildHash;
	np->pub.FindFile		= FSSTDIO_FLocate;
	np->pub.ReadFile		= FSSTDIO_ReadFile;
	np->pub.EnumerateFiles	= FSSTDIO_EnumerateFiles;
	np->pub.OpenVFS			= FSSTDIO_OpenVFS;
	np->pub.PollChanges		= FSSTDIO_PollChanges;
	return &np->pub;
}

#endif
#endif

