#include "hash.h"
extern hashtable_t filesystemhash;	//this table is the one to build your hash references into
extern int fs_hash_dups;	//for tracking efficiency. no functional use.
extern int fs_hash_files;	//for tracking efficiency. no functional use.


typedef struct {
	void	(*PrintPath)(void *handle);
	void	(*ClosePath)(void *handle);
	void	(*BuildHash)(void *handle);
	qboolean (*FindFile)(void *handle, flocation_t *loc, const char *name, void *hashedresult);	//true if found (hashedresult can be NULL)
		//note that if rawfile and offset are set, many Com_FileOpens will read the raw file
		//otherwise ReadFile will be called instead.
	void	(*ReadFile)(void *handle, flocation_t *loc, char *buffer);	//reads the entire file in one go (size comes from loc, so make sure the loc is valid, this is for performance with compressed archives)
	int		(*EnumerateFiles)(void *handle, const char *match, int (*func)(const char *, int, void *), void *parm);

	void	*(*OpenNew)(vfsfile_t *file, const char *desc);	//returns a handle to a new pak/path

	int		(*GeneratePureCRC) (void *handle, int seed, int usepure);

	vfsfile_t *(*OpenVFS)(void *handle, flocation_t *loc, const char *mode);
} searchpathfuncs_t;

//the stdio filesystem is special as that's the starting point of the entire filesystem
//warning: the handle is known to be a string pointer to the dir name
extern searchpathfuncs_t osfilefuncs;
vfsfile_t *VFSOS_Open(const char *osname, const char *mode);
vfsfile_t *FS_DecompressGZip(vfsfile_t *infile);

int FS_RegisterFileSystemType(const char *extension, searchpathfuncs_t *funcs);
void FS_UnRegisterFileSystemType(int idx);
