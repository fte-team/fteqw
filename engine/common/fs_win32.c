#include "quakedef.h"
#include "fs.h"
#include "winquake.h"

#ifndef INVALID_SET_FILE_POINTER
#define INVALID_SET_FILE_POINTER ~0
#endif

//read-only memory mapped files.
//for write access, we use the stdio module as a fallback.
//do you think anyone will ever notice that utf8 filenames work even in windows? probably not. oh well, worth a try.

#define VFSW32_Open VFSOS_Open
#define VFSW32_OpenPath VFSOS_OpenPath

typedef struct {
	searchpathfuncs_t pub;
	HANDLE changenotification;
	int hashdepth;
	char rootpath[1];
} vfsw32path_t;
typedef struct {
	vfsfile_t funcs;
	HANDLE hand;
	HANDLE mmh;
	void *mmap;
	unsigned int length;
	unsigned int offset;
} vfsw32file_t;

wchar_t *widen(wchar_t *out, size_t outlen, const char *utf8)
{
	wchar_t *ret = out;
	//utf-8 to utf-16, not ucs-2.
	unsigned int codepoint;
	int error;
	if (!outlen)
		return L"";
	outlen /= sizeof(wchar_t);
	outlen--;
	while (*utf8)
	{
		codepoint = utf8_decode(&error, utf8, (void*)&utf8);
		if (error || codepoint > 0x10FFFFu)
			codepoint = 0xFFFDu;
		if (codepoint > 0xffff)
		{
			if (outlen < 2)
				break;
			outlen -= 2;
			codepoint -= 0x10000u;
			*out++ = 0xD800 | (codepoint>>10);
			*out++ = 0xDC00 | (codepoint&0x3ff);
		}
		else
		{
			if (outlen < 1)
				break;
			outlen -= 1;
			*out++ = codepoint;
		}
	}
	*out = 0;
	return ret;
}

char *narrowen(char *out, size_t outlen, wchar_t *wide)
{
	char *ret = out;
	int bytes;
	unsigned int codepoint;
	if (!outlen)
		return "";
	outlen--;
	//utf-8 to utf-16, not ucs-2.
	while (*wide)
	{
		codepoint = *wide++;
		if (codepoint >= 0xD800u && codepoint <= 0xDBFFu)
		{	//handle utf-16 surrogates
			if (*wide >= 0xDC00u && *wide <= 0xDFFFu)
			{
				codepoint = (codepoint&0x3ff)<<10;
				codepoint |= *wide++ & 0x3ff;
			}
			else
				codepoint = 0xFFFDu;
		}
		bytes = utf8_encode(out, codepoint, outlen);
		if (bytes <= 0)
			break;
		out += bytes;
		outlen -= bytes;
	}
	*out = 0;
	return ret;
}


static int QDECL VFSW32_ReadBytes (struct vfsfile_s *file, void *buffer, int bytestoread)
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
static int QDECL VFSW32_WriteBytes (struct vfsfile_s *file, const void *buffer, int bytestoread)
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
static qboolean QDECL VFSW32_Seek (struct vfsfile_s *file, unsigned long pos)
{
	unsigned long upper, lower;
	vfsw32file_t *intfile = (vfsw32file_t*)file;
	if (intfile->mmap)
	{
		intfile->offset = pos;
		return true;
	}

	lower = (pos & 0xffffffff);
	upper = ((pos>>16)>>16);

	return SetFilePointer(intfile->hand, lower, &upper, FILE_BEGIN) != INVALID_SET_FILE_POINTER;
}
static unsigned long QDECL VFSW32_Tell (struct vfsfile_s *file)
{
	vfsw32file_t *intfile = (vfsw32file_t*)file;
	if (intfile->mmap)
		return intfile->offset;
	return SetFilePointer(intfile->hand, 0, NULL, FILE_CURRENT);
}
static void QDECL VFSW32_Flush(struct vfsfile_s *file)
{
	vfsw32file_t *intfile = (vfsw32file_t*)file;
	if (intfile->mmap)
		FlushViewOfFile(intfile->mmap, intfile->length);
	FlushFileBuffers(intfile->hand);
}
static unsigned long QDECL VFSW32_GetSize (struct vfsfile_s *file)
{
	vfsw32file_t *intfile = (vfsw32file_t*)file;

	if (intfile->mmap)
		return intfile->length;
	return GetFileSize(intfile->hand, NULL);
}
static void QDECL VFSW32_Close(vfsfile_t *file)
{
	vfsw32file_t *intfile = (vfsw32file_t*)file;
	if (intfile->mmap)
	{
		UnmapViewOfFile(intfile->mmap);
		CloseHandle(intfile->mmh);
	}
	CloseHandle(intfile->hand);
	Z_Free(file);

	COM_FlushFSCache();
}

vfsfile_t *QDECL VFSW32_Open(const char *osname, const char *mode)
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

	if (!WinNT)
	{
		if ((write && read) || append)
			h = CreateFileA(osname, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_DELETE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		else if (write)
			h = CreateFileA(osname, GENERIC_READ|GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		else if (read)
			h = CreateFileA(osname, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		else
			h = INVALID_HANDLE_VALUE;
	}
	else
	{
		wchar_t wide[MAX_OSPATH];
		widen(wide, sizeof(wide), osname);
		if ((write && read) || append)
			h = CreateFileW(wide, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_DELETE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		else if (write)
			h = CreateFileW(wide, GENERIC_READ|GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		else if (read)
			h = CreateFileW(wide, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		else
			h = INVALID_HANDLE_VALUE;
	}
	if (h == INVALID_HANDLE_VALUE)
		return NULL;

	fsize = GetFileSize(h, NULL);
	if (write || append || text || fsize > 1024*1024*5)
	{
		fsize = 0;
		mh = INVALID_HANDLE_VALUE;
		mmap = NULL;

		/*if appending, set the access position to the end of the file*/
		if (append)
			SetFilePointer(h, 0, NULL, FILE_END);
	}
	else
	{
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
#ifdef _DEBUG
	Q_strncpyz(file->funcs.dbgname, osname, sizeof(file->funcs.dbgname));
#endif
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

static vfsfile_t *QDECL VFSW32_OpenVFS(searchpathfuncs_t *handle, flocation_t *loc, const char *mode)
{
	//path is already cleaned, as anything that gets a valid loc needs cleaning up first.

	return VFSW32_Open(loc->rawname, mode);
}
static void QDECL VFSW32_ClosePath(searchpathfuncs_t *handle)
{
	vfsw32path_t *wp = (void*)handle;
	if (wp->changenotification != INVALID_HANDLE_VALUE)
		FindCloseChangeNotification(wp->changenotification);
	Z_Free(wp);
}
static qboolean QDECL VFSW32_PollChanges(searchpathfuncs_t *handle)
{
	qboolean result = false;
	vfsw32path_t *wp = (void*)handle;

	if (wp->changenotification == INVALID_HANDLE_VALUE)
		return true;
	for(;;)
	{
		switch(WaitForSingleObject(wp->changenotification, 0))
		{
		case WAIT_OBJECT_0:
			result = true;
			break;
		case WAIT_TIMEOUT:
			return result;
		default:
			FindCloseChangeNotification(wp->changenotification);
			wp->changenotification = INVALID_HANDLE_VALUE;
			return true;
		}
		FindNextChangeNotification(wp->changenotification);
	}
	return result;
}
static int QDECL VFSW32_RebuildFSHash(const char *filename, int filesize, void *handle, searchpathfuncs_t *spath)
{
	vfsw32path_t *wp = (void*)spath;
	void (QDECL *AddFileHash)(int depth, const char *fname, fsbucket_t *filehandle, void *pathhandle) = handle;
	if (filename[strlen(filename)-1] == '/')
	{	//this is actually a directory

		char childpath[256];
		Q_snprintfz(childpath, sizeof(childpath), "%s*", filename);
		Sys_EnumerateFiles(wp->rootpath, childpath, VFSW32_RebuildFSHash, handle, spath);
		return true;
	}

	AddFileHash(wp->hashdepth, filename, NULL, wp);
	return true;
}
static void QDECL VFSW32_BuildHash(searchpathfuncs_t *handle, int hashdepth, void (QDECL *AddFileHash)(int depth, const char *fname, fsbucket_t *filehandle, void *pathhandle))
{
	vfsw32path_t *wp = (void*)handle;
	wp->hashdepth = hashdepth;
	Sys_EnumerateFiles(wp->rootpath, "*", VFSW32_RebuildFSHash, AddFileHash, handle);
}
static qboolean QDECL VFSW32_FLocate(searchpathfuncs_t *handle, flocation_t *loc, const char *filename, void *hashedresult)
{
	vfsw32path_t *wp = (void*)handle;
	FILE *f;
	int len;
	char netpath[MAX_OSPATH];
	wchar_t wide[MAX_OSPATH];


	if (hashedresult && (void *)hashedresult != wp)
		return false;

/*
	if (!static_registered)
	{	// if not a registered version, don't ever go beyond base
		if ( strchr (filename, '/') || strchr (filename,'\\'))
			continue;
	}
*/

// check a file in the directory tree
	snprintf (netpath, sizeof(netpath)-1, "%s/%s", wp->rootpath, filename);

	if (!WinNT)
		f = fopen(netpath, "rb");
	else
		f = _wfopen(widen(wide, sizeof(wide), netpath), L"rb");
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
		snprintf(loc->rawname, sizeof(loc->rawname), "%s/%s", wp->rootpath, filename);
	}

	return true;
}
static void QDECL VFSW32_ReadFile(searchpathfuncs_t *handle, flocation_t *loc, char *buffer)
{
//	vfsw32path_t *wp = handle;

	FILE *f;
	wchar_t wide[MAX_OSPATH];
	if (!WinNT)
		f = fopen(loc->rawname, "rb");
	else
		f = _wfopen(widen(wide, sizeof(wide), loc->rawname), L"rb");
	if (!f)	//err...
		return;
	fseek(f, loc->offset, SEEK_SET);
	fread(buffer, 1, loc->len, f);
	fclose(f);
}
static int QDECL VFSW32_EnumerateFiles (searchpathfuncs_t *handle, const char *match, int (QDECL *func)(const char *, int, void *, searchpathfuncs_t *spath), void *parm)
{
	vfsw32path_t *wp = (vfsw32path_t*)handle;
	return Sys_EnumerateFiles(wp->rootpath, match, func, parm, handle);
}

qboolean QDECL VFSW32_RenameFile(searchpathfuncs_t *handle, const char *oldfname, const char *newfname)
{
	vfsw32path_t *wp = (vfsw32path_t*)handle;
	char oldsyspath[MAX_OSPATH];
	char newsyspath[MAX_OSPATH];
	snprintf (oldsyspath, sizeof(oldsyspath)-1, "%s/%s", wp->rootpath, oldfname);
	snprintf (newsyspath, sizeof(newsyspath)-1, "%s/%s", wp->rootpath, newfname);
	return Sys_Rename(oldsyspath, newsyspath);
}
qboolean QDECL VFSW32_RemoveFile(searchpathfuncs_t *handle, const char *filename)
{
	vfsw32path_t *wp = (vfsw32path_t*)handle;
	char syspath[MAX_OSPATH];
	snprintf (syspath, sizeof(syspath)-1, "%s/%s", wp->rootpath, filename);
	return Sys_remove(syspath);
}
qboolean QDECL VFSW32_MkDir(searchpathfuncs_t *handle, const char *filename)
{
	vfsw32path_t *wp = (vfsw32path_t*)handle;
	char syspath[MAX_OSPATH];
	snprintf (syspath, sizeof(syspath)-1, "%s/%s", wp->rootpath, filename);
	Sys_mkdir(syspath);
	return true;
}

searchpathfuncs_t *QDECL VFSW32_OpenPath(vfsfile_t *mustbenull, const char *desc)
{
	vfsw32path_t *np;
	int dlen = strlen(desc);
	if (mustbenull)
		return NULL;
	np = Z_Malloc(sizeof(*np) + dlen);
	if (np)
	{
		wchar_t wide[MAX_OSPATH];
		memcpy(np->rootpath, desc, dlen+1);
		if (!WinNT)
			np->changenotification = FindFirstChangeNotificationA(np->rootpath, true, FILE_NOTIFY_CHANGE_FILE_NAME);
		else
			np->changenotification = FindFirstChangeNotificationW(widen(wide, sizeof(wide), np->rootpath), true, FILE_NOTIFY_CHANGE_FILE_NAME);
	}

	np->pub.fsver			= FSVER;
	np->pub.ClosePath		= VFSW32_ClosePath;
	np->pub.BuildHash		= VFSW32_BuildHash;
	np->pub.FindFile		= VFSW32_FLocate;
	np->pub.ReadFile		= VFSW32_ReadFile;
	np->pub.EnumerateFiles	= VFSW32_EnumerateFiles;
	np->pub.OpenVFS			= VFSW32_OpenVFS;
	np->pub.PollChanges		= VFSW32_PollChanges;

	np->pub.RenameFile		= VFSW32_RenameFile;
	np->pub.RemoveFile		= VFSW32_RemoveFile;
	np->pub.MkDir			= VFSW32_MkDir;

	return &np->pub;
}
