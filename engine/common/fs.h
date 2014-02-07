#include "hash.h"

#define FSVER 2

#define FF_NOTFOUND	0	//file wasn't found
#define FF_FOUND	1	//file was found
#define FF_SYMLINK	2	//file contents are the name of a different file (symlink). do a recursive lookup on the name

typedef struct
{
	bucket_t buck;
	int depth;	/*shallower files will remove deeper files*/
} fsbucket_t;
extern hashtable_t filesystemhash;	//this table is the one to build your hash references into
extern int fs_hash_dups;	//for tracking efficiency. no functional use.
extern int fs_hash_files;	//for tracking efficiency. no functional use.

struct searchpath_s;
struct searchpathfuncs_s
{
	int				fsver;
	void			(QDECL *ClosePath)(searchpathfuncs_t *handle);

	void			(QDECL *GetPathDetails)(searchpathfuncs_t *handle, char *outdetails, size_t sizeofdetails);
	void			(QDECL *BuildHash)(searchpathfuncs_t *handle, int depth, void (QDECL *FS_AddFileHash)(int depth, const char *fname, fsbucket_t *filehandle, void *pathhandle));
	unsigned int	(QDECL *FindFile)(searchpathfuncs_t *handle, flocation_t *loc, const char *name, void *hashedresult);	//true if found (hashedresult can be NULL)
		//note that if rawfile and offset are set, many Com_FileOpens will read the raw file
		//otherwise ReadFile will be called instead.
	void			(QDECL *ReadFile)(searchpathfuncs_t *handle, flocation_t *loc, char *buffer);	//reads the entire file in one go (size comes from loc, so make sure the loc is valid, this is for performance with compressed archives)
	int				(QDECL *EnumerateFiles)(searchpathfuncs_t *handle, const char *match, int (QDECL *func)(const char *fname, qofs_t fsize, void *parm, searchpathfuncs_t *spath), void *parm);

	int				(QDECL *GeneratePureCRC) (searchpathfuncs_t *handle, int seed, int usepure);

	vfsfile_t *		(QDECL *OpenVFS)(searchpathfuncs_t *handle, flocation_t *loc, const char *mode);

	qboolean		(QDECL *PollChanges)(searchpathfuncs_t *handle);	//returns true if there were changes

	qboolean		(QDECL *RenameFile)(searchpathfuncs_t *handle, const char *oldname, const char *newname);	//returns true on success, false if source doesn't exist, or if dest does.
	qboolean		(QDECL *RemoveFile)(searchpathfuncs_t *handle, const char *filename);	//returns true on success, false if it wasn't found or is readonly.
	qboolean		(QDECL *MkDir)(searchpathfuncs_t *handle, const char *filename);	//is this really needed?
};
//searchpathfuncs_t *(QDECL *OpenNew)(vfsfile_t *file, const char *desc);	//returns a handle to a new pak/path

//the stdio filesystem is special as that's the starting point of the entire filesystem
//warning: the handle is known to be a string pointer to the dir name
extern searchpathfuncs_t *(QDECL VFSOS_OpenPath) (vfsfile_t *file, const char *desc);
extern searchpathfuncs_t *(QDECL FSZIP_LoadArchive) (vfsfile_t *packhandle, const char *desc);
extern searchpathfuncs_t *(QDECL FSPAK_LoadArchive) (vfsfile_t *packhandle, const char *desc);
extern searchpathfuncs_t *(QDECL FSDWD_LoadArchive) (vfsfile_t *packhandle, const char *desc);
vfsfile_t *QDECL VFSOS_Open(const char *osname, const char *mode);
vfsfile_t *FS_DecompressGZip(vfsfile_t *infile, vfsfile_t *outfile);

int FS_RegisterFileSystemType(void *module, const char *extension, searchpathfuncs_t *(QDECL *OpenNew)(vfsfile_t *file, const char *desc), qboolean loadscan);
void FS_UnRegisterFileSystemType(int idx);
void FS_UnRegisterFileSystemModule(void *module);

#define SPF_REFERENCED		1	//something has been loaded from this path. should filter out client references...
#define SPF_COPYPROTECTED	2	//downloads are not allowed fom here.
#define SPF_TEMPORARY		4	//a map-specific path, purged at map change.
#define SPF_EXPLICIT		8	//a root gamedir (bumps depth on gamedir depth checks). 
#define SPF_UNTRUSTED		16	//has been downloaded from somewhere. configs inside it should never be execed with local access rights.
qboolean FS_LoadPackageFromFile(vfsfile_t *vfs, char *pname, char *localname, int *crc, unsigned int flags);
