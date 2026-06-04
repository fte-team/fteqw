#include "../plugin.h"
#include "../../engine/common/fs.h"

static plugfsfuncs_t *filefuncs = NULL;
static plugthreadfuncs_t *threadfuncs = NULL;
#define Sys_CreateMutex() (threadfuncs?threadfuncs->CreateMutex():NULL)
#define Sys_LockMutex(m) (threadfuncs?threadfuncs->LockMutex(m):true)
#define Sys_UnlockMutex if(threadfuncs)threadfuncs->UnlockMutex
#define Sys_DestroyMutex if(threadfuncs)threadfuncs->DestroyMutex

typedef struct oreheader {
	uint32_t header_size;
	uint32_t num_directories;
} oreheader_t;

typedef struct orefile {
	fsbucket_t bucket;
	char name[MAX_QPATH];
	size_t ofs;
	size_t len;
} orefile_t;

typedef struct ore {
	searchpathfuncs_t pub;
	char desc[MAX_OSPATH];
	size_t handlepos;
	vfsfile_t *handle;
	size_t num_files;
	orefile_t *files;
	void *mutex;
	int references;
} ore_t;

static qboolean VFS_READZ(vfsfile_t *file, char *out, size_t outlen)
{
	size_t i;
	for (i = 0; i < outlen; i++)
	{
		if (VFS_READ(file, &out[i], sizeof(char)) != sizeof(char))
			break;
		if (out[i] == 0)
			return true;
	}

	// too long
	out[i] = 0;
	return false;
}

static void QDECL FSORE_GetPathDetails(searchpathfuncs_t *handle, char *out, size_t outlen)
{
	ore_t *ore = (ore_t *)handle;
	*out = 0;
	if (ore->references != 1)
		Q_snprintfz(out, outlen, "(%i)", ore->references - 1);
}

static void QDECL FSORE_ClosePath(searchpathfuncs_t *handle)
{
	qboolean stillopen;
	ore_t *ore = (ore_t *)handle;

	if (!Sys_LockMutex(ore->mutex))
		return; //ohnoes
	stillopen = --ore->references > 0;
	Sys_UnlockMutex(ore->mutex);
	if (stillopen)
		return; //not free yet

	VFS_CLOSE(ore->handle);

	Sys_DestroyMutex(ore->mutex);

	plugfuncs->Free(ore->files);
	plugfuncs->Free(ore);
}

static void QDECL FSORE_BuildHash(searchpathfuncs_t *handle, int depth, void (QDECL *AddFileHash)(int depth, const char *fname, fsbucket_t *filehandle, void *pathhandle))
{
	ore_t *ore = (ore_t *)handle;
	int i;
	for (i = 0; i < ore->num_files; i++)
		AddFileHash(depth, ore->files[i].name, &ore->files[i].bucket, &ore->files[i]);
}

static unsigned int QDECL FSORE_FindFile(searchpathfuncs_t *handle, flocation_t *loc, const char *filename, void *hashedresult)
{
	orefile_t *file = (orefile_t *)hashedresult;
	ore_t *ore = (ore_t *)handle;
	int i;

	if (file)
	{
		//is this a pointer to a file in this pak?
		if (file < ore->files || file > ore->files + ore->num_files)
			return FF_NOTFOUND;
	}
	else
	{
		for (i = 0; i < ore->num_files; i++)
		{
			if (strcasecmp(filename, ore->files[i].name) == 0)
			{
				file = &ore->files[i];
				break;
			}
		}
	}

	if (file)
	{
		if (loc)
		{
			loc->fhandle = file;
			*loc->rawname = 0;
			loc->offset = (qofs_t)-1;
			loc->len = file->len;
		}

		return FF_FOUND;
	}

	return FF_NOTFOUND;
}

typedef struct {
	vfsfile_t funcs;
	ore_t *parent;
	qofs_t startpos;
	qofs_t length;
	qofs_t currentpos;
} vfsore_t;

static int QDECL VFSORE_ReadBytes(struct vfsfile_s *vfs, void *buffer, int bytestoread)
{
	vfsore_t *vfsp = (vfsore_t *)vfs;
	int read = 0;

	if (bytestoread + vfsp->currentpos > vfsp->length)
		bytestoread = vfsp->length - vfsp->currentpos;

	if (Sys_LockMutex(vfsp->parent->mutex))
	{
		VFS_SEEK(vfsp->parent->handle, vfsp->startpos + vfsp->currentpos);
		read = VFS_READ(vfsp->parent->handle, buffer, bytestoread);
		vfsp->currentpos += read;
		Sys_UnlockMutex(vfsp->parent->mutex);
	}

	return read;
}

static int QDECL VFSORE_WriteBytes(struct vfsfile_s *vfs, const void *buffer, int bytestoread)
{
	plugfuncs->Error("Cannot write to ore files\n");
	return 0;
}

static qofs_t QDECL VFSORE_Tell(struct vfsfile_s *vfs)
{
	vfsore_t *vfsp = (vfsore_t *)vfs;
	return vfsp->currentpos;
}

static qofs_t QDECL VFSORE_GetLen(struct vfsfile_s *vfs)
{
	vfsore_t *vfsp = (vfsore_t *)vfs;
	return vfsp->length;
}

static qboolean QDECL VFSORE_Close(vfsfile_t *vfs)
{
	vfsore_t *vfsp = (vfsore_t *)vfs;
	FSORE_ClosePath(&vfsp->parent->pub); //tell the parent that we don't need it open any more (reference counts)
	plugfuncs->Free(vfsp); //free ourselves.
	return true;
}

static qboolean QDECL VFSORE_Seek(struct vfsfile_s *vfs, qofs_t pos)
{
	vfsore_t *vfsp = (vfsore_t *)vfs;
	if (pos > vfsp->length)
		return false;
	vfsp->currentpos = pos;
	return true;
}

static vfsfile_t *QDECL FSORE_OpenVFS(searchpathfuncs_t *handle, flocation_t *loc, const char *mode)
{
	ore_t *ore = (ore_t *)handle;
	vfsore_t *vfs;
	orefile_t *f = (orefile_t *)loc->fhandle;

	if (strcmp(mode, "rb") != 0)
		return NULL;

	vfs = plugfuncs->Malloc(sizeof(vfsore_t));

	vfs->parent = ore;
	if (!Sys_LockMutex(vfs->parent->mutex))
	{
		plugfuncs->Free(vfs);
		return NULL;
	}

	vfs->parent->references++;
	Sys_UnlockMutex(vfs->parent->mutex);

	vfs->startpos = f->ofs;
	vfs->length = loc->len;
	vfs->currentpos = 0;

#ifdef _DEBUG
	Q_strlcpy(vfs->funcs.dbgname, f->name, sizeof(vfs->funcs.dbgname));
#endif
	vfs->funcs.Close = VFSORE_Close;
	vfs->funcs.GetLen = VFSORE_GetLen;
	vfs->funcs.ReadBytes = VFSORE_ReadBytes;
	vfs->funcs.Seek = VFSORE_Seek;
	vfs->funcs.Tell = VFSORE_Tell;
	vfs->funcs.WriteBytes = VFSORE_WriteBytes; //not supported

	return (vfsfile_t *)vfs;
}

static void QDECL FSORE_ReadFile(searchpathfuncs_t *handle, flocation_t *loc, char *buffer)
{
	vfsfile_t *f;
	f = FSORE_OpenVFS(handle, loc, "rb");
	if (!f)
		return;
	VFS_READ(f, buffer, loc->len);
	VFS_CLOSE(f);
}

static int QDECL FSORE_EnumerateFiles(searchpathfuncs_t *handle, const char *match, int (QDECL *func)(const char *, qofs_t, time_t mtime, void *, searchpathfuncs_t *spath), void *parm)
{
	ore_t *ore = (ore_t *)handle;
	int i;

	for (i = 0; i < ore->num_files; i++)
	{
		if (filefuncs->WildCmp(match, ore->files[i].name))
		{
			if (!func(ore->files[i].name, ore->files[i].len, 0, parm, handle))
				return false;
		}
	}

	return true;
}

static int QDECL FSORE_GeneratePureCRC(searchpathfuncs_t *handle, const int *seed)
{
	ore_t *ore = (ore_t *)handle;
	return filefuncs->BlockChecksum(ore->files, sizeof(*ore->files) * ore->num_files);
}

static searchpathfuncs_t *QDECL FSORE_LoadArchive(vfsfile_t *file, searchpathfuncs_t *parent, const char *filename, const char *desc, const char *prefix)
{
	oreheader_t header;
	ore_t *ore;
	int i, j;

	// sanity checks
	if (!file) return NULL;
	if (prefix && *prefix) return NULL;

	// read header
	if (VFS_READ(file, &header, sizeof(header)) < sizeof(header))
		return NULL;

	// alloc
	ore = (ore_t *)plugfuncs->Malloc(sizeof(*ore));
	ore->num_files = 0;
	ore->files = NULL;

	// determine number of files
	for (i = 0; i < header.num_directories; i++)
	{
		char dname[MAX_QPATH];
		uint32_t filetableoffset;
		uint32_t nextdiroffset;
		uint64_t sentinel;
		uint32_t num_files;

		// read name and offset to file table
		VFS_READZ(file, dname, sizeof(dname));
		VFS_READ(file, &filetableoffset, sizeof(filetableoffset));
		nextdiroffset = VFS_TELL(file);

		// read file table
		VFS_SEEK(file, filetableoffset);
		VFS_READ(file, &sentinel, sizeof(sentinel));
		if (sentinel != 8)
		{
			Con_Printf("%s: bad sentinel on directory %s\n", filename, dname);
			return NULL;
		}

		// reallocate internal file table
		VFS_READ(file, &num_files, sizeof(num_files));
		ore->files = plugfuncs->Realloc(ore->files, sizeof(*ore->files) * (ore->num_files + num_files));

		// parse each file
		for (j = ore->num_files; j < ore->num_files + num_files; j++)
		{
			char fname[MAX_QPATH];
			uint32_t ofs_data, len_data;
			VFS_READZ(file, fname, sizeof(fname));
			VFS_READ(file, &ofs_data, sizeof(ofs_data));
			VFS_READ(file, &len_data, sizeof(len_data));
			Q_snprintfz(ore->files[j].name, sizeof(ore->files[j].name), "%s/%s", dname, fname);
			ore->files[j].ofs = filetableoffset + ofs_data + 8;
			ore->files[j].len = len_data;
		}

		// accumulate
		ore->num_files += num_files;

		// seek to next
		VFS_SEEK(file, nextdiroffset);
	}

	// setup info
	ore->references++;
	ore->handle = file;
	ore->handlepos = 0;
	strcpy(ore->desc, desc);
	VFS_SEEK(ore->handle, ore->handlepos);
	ore->mutex = Sys_CreateMutex();

	// setup funcs
	ore->pub.fsver = FSVER;
	ore->pub.GetPathDetails = FSORE_GetPathDetails;
	ore->pub.ClosePath = FSORE_ClosePath;
	ore->pub.BuildHash = FSORE_BuildHash;
	ore->pub.FindFile = FSORE_FindFile;
	ore->pub.ReadFile = FSORE_ReadFile;
	ore->pub.EnumerateFiles = FSORE_EnumerateFiles;
	ore->pub.GeneratePureCRC = FSORE_GeneratePureCRC;
	ore->pub.OpenVFS = FSORE_OpenVFS;

	return (searchpathfuncs_t *)ore;
}

qboolean ORE_Init(void)
{
	// get funcs
	threadfuncs = plugfuncs->GetEngineInterface(plugthreadfuncs_name, sizeof(*threadfuncs));
	filefuncs = plugfuncs->GetEngineInterface(plugfsfuncs_name, sizeof(*filefuncs));
	if (!filefuncs) // threadfuncs optional
		return false;

	// export our fs function
	if (!plugfuncs->ExportFunction("FS_RegisterArchiveType_ore", FSORE_LoadArchive))
	{
		Con_Printf("engine doesn't support filesystem plugins\n");
		return false;
	}

	return true;
}
