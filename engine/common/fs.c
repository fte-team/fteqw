#include "quakedef.h"
#include "netinc.h"

//#define com_gamedir com__gamedir

#include <ctype.h>
#include <limits.h>

#include "fs.h"
#include "shader.h"
#ifdef _WIN32
#include "winquake.h"
#endif

static void fs_game_callback(cvar_t *var, char *oldvalue);
hashtable_t filesystemhash;
qboolean com_fschanged = true;
qboolean fs_readonly;
static unsigned int fs_restarts;
void *fs_thread_mutex;

cvar_t com_fs_cache			= CVARF("fs_cache", IFMINIMAL("2","1"), CVAR_ARCHIVE);
cvar_t cfg_reload_on_gamedir = CVAR("cfg_reload_on_gamedir", "1");
cvar_t fs_game = CVARFDC("game", "", CVAR_NOSAVE|CVAR_NORESET, "Provided for Q2 compat.", fs_game_callback);
cvar_t fs_gamedir = CVARFD("fs_gamedir", "", CVAR_NOUNSAFEEXPAND|CVAR_NOSET|CVAR_NOSAVE, "Provided for Q2 compat.");
cvar_t fs_basedir = CVARFD("fs_basedir", "", CVAR_NOUNSAFEEXPAND|CVAR_NOSET|CVAR_NOSAVE, "Provided for Q2 compat.");
int active_fs_cachetype;
static int fs_referencetype;
int fs_finds;
void COM_CheckRegistered (void);

static void fs_game_callback(cvar_t *var, char *oldvalue)
{
	static qboolean runaway = false;
	char buf[MAX_OSPATH];
	if (!strcmp(var->string, oldvalue))
		return;	//no change here.
	if (runaway)
		return;	//ignore that
	runaway = true;
	Cmd_ExecuteString(va("gamedir %s\n", COM_QuotedString(var->string, buf, sizeof(buf), false)), Cmd_ExecLevel);
	runaway = false;
}

struct
{
	void *module;
	const char *extension;
	searchpathfuncs_t *(QDECL *OpenNew)(vfsfile_t *file, const char *desc);
	qboolean loadscan;
} searchpathformats[64];

int FS_RegisterFileSystemType(void *module, const char *extension, searchpathfuncs_t *(QDECL *OpenNew)(vfsfile_t *file, const char *desc), qboolean loadscan)
{
	unsigned int i;
	for (i = 0; i < sizeof(searchpathformats)/sizeof(searchpathformats[0]); i++)
	{
		if (searchpathformats[i].extension && !strcmp(searchpathformats[i].extension, extension))
			break;	//extension match always replaces
		if (!searchpathformats[i].extension && !searchpathformats[i].OpenNew)
			break;
	}
	if (i == sizeof(searchpathformats)/sizeof(searchpathformats[0]))
		return 0;

	searchpathformats[i].module = module;
	searchpathformats[i].extension = extension;
	searchpathformats[i].OpenNew = OpenNew;
	searchpathformats[i].loadscan = loadscan;
	com_fschanged = true;

	return i+1;
}

void FS_UnRegisterFileSystemType(int idx)
{
	if ((unsigned int)(idx-1) >= sizeof(searchpathformats)/sizeof(searchpathformats[0]))
		return;

	searchpathformats[idx-1].OpenNew = NULL;
	searchpathformats[idx-1].module = NULL;
	com_fschanged = true;

	//FS_Restart will be needed
}
void FS_UnRegisterFileSystemModule(void *module)
{
	int i;
	qboolean found = false;
	if (Sys_LockMutex(fs_thread_mutex))
	{
		for (i = 0; i < sizeof(searchpathformats)/sizeof(searchpathformats[0]); i++)
		{
			if (searchpathformats[i].module == module)
			{
				searchpathformats[i].OpenNew = NULL;
				searchpathformats[i].module = NULL;
				found = true;
			}
		}
		Sys_UnlockMutex(fs_thread_mutex);
		if (found)
		{
			Cmd_ExecuteString("fs_restart", RESTRICT_LOCAL);
		}
	}
}

char *VFS_GETS(vfsfile_t *vf, char *buffer, int buflen)
{
	char in;
	char *out = buffer;
	int len;
	len = buflen-1;
	if (len == 0)
		return NULL;
	while (len > 0)
	{
		if (VFS_READ(vf, &in, 1) != 1)
		{
			if (len == buflen-1)
				return NULL;
			*out = '\0';
			return buffer;
		}
		if (in == '\n')
			break;
		*out++ = in;
		len--;
	}
	*out = '\0';

	//if there's a trailing \r, strip it.
	if (out > buffer)
		if (out[-1] == '\r')
			out[-1] = 0;

	return buffer;
}

void VARGS VFS_PRINTF(vfsfile_t *vf, char *format, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr, format);
	vsnprintf (string,sizeof(string)-1, format,argptr);
	va_end (argptr);

	VFS_PUTS(vf, string);
}






char	gamedirfile[MAX_OSPATH];
char	pubgamedirfile[MAX_OSPATH];	//like gamedirfile, but not set to the fte-only paths




char	com_gamepath[MAX_OSPATH];	//c:\games\quake
char	com_homepath[MAX_OSPATH];	//c:\users\foo\my docs\fte\quake
qboolean	com_homepathenabled;
qboolean	com_homepathusable;	//com_homepath is safe, even if not enabled.

char	com_configdir[MAX_OSPATH];	//homedir/fte/configs

int fs_hash_dups;
int fs_hash_files;







static const char *FS_GetCleanPath(const char *pattern, char *outbuf, int outlen);
void FS_RegisterDefaultFileSystems(void);
static void	COM_CreatePath (char *path);
ftemanifest_t *FS_ReadDefaultManifest(char *newbasedir, size_t newbasedirsize, qboolean fixedbasedir);

#define ENFORCEFOPENMODE(mode) {if (strcmp(mode, "r") && strcmp(mode, "w")/* && strcmp(mode, "rw")*/)Sys_Error("fs mode %s is not permitted here\n");}






//forget a manifest entirely.
void FS_Manifest_Free(ftemanifest_t *man)
{
	int i, j;
	if (!man)
		return;
	Z_Free(man->updateurl);
	Z_Free(man->updatefile);
	Z_Free(man->installation);
	Z_Free(man->formalname);
	Z_Free(man->protocolname);
	Z_Free(man->defaultexec);
	for (i = 0; i < sizeof(man->gamepath) / sizeof(man->gamepath[0]); i++)
	{
		Z_Free(man->gamepath[i].path);
	}
	for (i = 0; i < sizeof(man->package) / sizeof(man->package[0]); i++)
	{
		Z_Free(man->package[i].path);
		Z_Free(man->package[i].extractname);
		for (j = 0; j < sizeof(man->package[i].mirrors) / sizeof(man->package[i].mirrors[0]); j++)
			Z_Free(man->package[i].mirrors[j]);
	}
	Z_Free(man);
}

//clone a manifest, so we can hack at it.
static ftemanifest_t *FS_Manifest_Clone(ftemanifest_t *oldm)
{
	ftemanifest_t *newm;
	int i, j;
	newm = Z_Malloc(sizeof(*newm));
	if (oldm->updateurl)
		newm->updateurl = Z_StrDup(oldm->updateurl);
	if (oldm->installation)
		newm->installation = Z_StrDup(oldm->installation);
	if (oldm->formalname)
		newm->formalname = Z_StrDup(oldm->formalname);
	if (oldm->protocolname)
		newm->protocolname = Z_StrDup(oldm->protocolname);
	if (oldm->defaultexec)
		newm->defaultexec = Z_StrDup(oldm->defaultexec);
	newm->disablehomedir = oldm->disablehomedir;

	for (i = 0; i < sizeof(newm->gamepath) / sizeof(newm->gamepath[0]); i++)
	{
		if (oldm->gamepath[i].path)
			newm->gamepath[i].path = Z_StrDup(oldm->gamepath[i].path);
		newm->gamepath[i].base = oldm->gamepath[i].base;
	}
	for (i = 0; i < sizeof(newm->package) / sizeof(newm->package[0]); i++)
	{
		if (oldm->package[i].path)
			newm->package[i].path = Z_StrDup(oldm->package[i].path);
		if (oldm->package[i].extractname)
			newm->package[i].extractname = Z_StrDup(oldm->package[i].extractname);
		for (j = 0; j < sizeof(newm->package[i].mirrors) / sizeof(newm->package[i].mirrors[0]); j++)
			if (oldm->package[i].mirrors[j])
				newm->package[i].mirrors[j] = Z_StrDup(oldm->package[i].mirrors[j]);
	}

	return newm;
}

void FS_Manifest_Print(ftemanifest_t *man)
{
	char buffer[1024];
	int i, j;
	if (man->updateurl)
		Con_Printf("updateurl %s\n", COM_QuotedString(man->updateurl, buffer, sizeof(buffer), false));
	if (man->installation)
		Con_Printf("game %s\n", COM_QuotedString(man->installation, buffer, sizeof(buffer), false));
	if (man->formalname)
		Con_Printf("name %s\n", COM_QuotedString(man->formalname, buffer, sizeof(buffer), false));
	if (man->protocolname)
		Con_Printf("protocolname %s\n", COM_QuotedString(man->protocolname, buffer, sizeof(buffer), false));
	if (man->defaultexec)
		Con_Printf("defaultexec %s\n", COM_QuotedString(man->defaultexec, buffer, sizeof(buffer), false));

	for (i = 0; i < sizeof(man->gamepath) / sizeof(man->gamepath[0]); i++)
	{
		if (man->gamepath[i].path)
		{
			if (man->gamepath[i].base)
				Con_Printf("basegame %s\n", COM_QuotedString(man->gamepath[i].path, buffer, sizeof(buffer), false));
			else
				Con_Printf("gamedir %s\n", COM_QuotedString(man->gamepath[i].path, buffer, sizeof(buffer), false));
		}
	}

	for (i = 0; i < sizeof(man->package) / sizeof(man->package[0]); i++)
	{
		if (man->package[i].path)
		{
			if (man->package[i].extractname)
				Con_Printf("achived");
			if (man->package[i].crcknown)
				Con_Printf("package %s 0x%x", COM_QuotedString(man->package[i].path, buffer, sizeof(buffer), false), man->package[i].crc);
			else
				Con_Printf("package %s -", COM_QuotedString(man->package[i].path, buffer, sizeof(buffer), false));
			if (man->package[i].extractname)
				Con_Printf(" %s", COM_QuotedString(man->package[i].extractname, buffer, sizeof(buffer), false));
			for (j = 0; j < sizeof(man->package[i].mirrors) / sizeof(man->package[i].mirrors[0]); j++)
				if (man->package[i].mirrors[j])
					Con_Printf(" %s", COM_QuotedString(man->package[i].mirrors[j], buffer, sizeof(buffer), false));
			Con_Printf("\n");
		}
	}
}

//forget any mod dirs.
static void FS_Manifest_PurgeGamedirs(ftemanifest_t *man)
{
	int i;
	for (i = 0; i < sizeof(man->gamepath) / sizeof(man->gamepath[0]); i++)
	{
		if (man->gamepath[i].path && !man->gamepath[i].base)
		{
			Z_Free(man->gamepath[i].path);
			man->gamepath[i].path = NULL;

			//FIXME: remove packages from the removed paths.
		}
	}
}

//create a new empty manifest with default values.
static ftemanifest_t *FS_Manifest_Create(const char *syspath)
{
	ftemanifest_t *man = Z_Malloc(sizeof(*man));

	man->formalname = Z_StrDup(FULLENGINENAME);

	if (syspath)
		man->updatefile = Z_StrDup(syspath);	//this should be a system path.
	return man;
}
//parse Cmd_Argv tokens into the manifest.
static qboolean FS_Manifest_ParseTokens(ftemanifest_t *man)
{
	qboolean result = true;
	char *fname;
	if (!Cmd_Argc())
		return result;
	fname = Cmd_Argv(0);

	if (*fname == '*')
		fname++;
	if (!Q_strcasecmp(fname, "minver"))
	{
		//ignore minimum versions for other engines.
		if (!strcmp(Cmd_Argv(2), DISTRIBUTION))
			man->minver = atoi(Cmd_Argv(3));
	}
	else if (!Q_strcasecmp(fname, "maxver"))
	{
		//ignore minimum versions for other engines.
		if (!strcmp(Cmd_Argv(2), DISTRIBUTION))
			man->maxver = atoi(Cmd_Argv(3));
	}
	else if (!Q_strcasecmp(fname, "game"))
	{
		Z_Free(man->installation);
		man->installation = Z_StrDup(Cmd_Argv(1));
	}
	else if (!Q_strcasecmp(fname, "name"))
	{
		Z_Free(man->formalname);
		man->formalname = Z_StrDup(Cmd_Argv(1));
	}
	else if (!Q_strcasecmp(fname, "protocolname"))
	{
		Z_Free(man->protocolname);
		man->protocolname = Z_StrDup(Cmd_Argv(1));
	}
	else if (!Q_strcasecmp(fname, "defaultexec"))
	{
		Z_Free(man->defaultexec);
		man->defaultexec = Z_StrDup(Cmd_Argv(1));
	}
	else if (!Q_strcasecmp(fname, "updateurl"))
	{
		Z_Free(man->updateurl);
		man->updateurl = Z_StrDup(Cmd_Argv(1));
	}
	else if (!Q_strcasecmp(fname, "disablehomedir"))
	{
		man->disablehomedir = !!atoi(Cmd_Argv(1));
	}
	else if (!Q_strcasecmp(fname, "basegame") || !Q_strcasecmp(fname, "gamedir"))
	{
		int i;
		char *newdir = Cmd_Argv(1);

		//reject various evil path arguments.
		if (!*newdir || strchr(newdir, '\n') || strchr(newdir, '\r') || !strcmp(newdir, ".") || !strcmp(newdir, "..") || strchr(newdir, ':') || strchr(newdir, '/') || strchr(newdir, '\\') || strchr(newdir, '$'))
		{
			Con_Printf("Illegal path specified: %s\n", newdir);
		}
		else
		{
			for (i = 0; i < sizeof(man->gamepath) / sizeof(man->gamepath[0]); i++)
			{
				if (!man->gamepath[i].path)
				{
					man->gamepath[i].base = !Q_strcasecmp(fname, "basegame");
					man->gamepath[i].path = Z_StrDup(newdir);
					break;
				}
			}
			if (i == sizeof(man->gamepath) / sizeof(man->gamepath[0]))
			{
				Con_Printf("Too many game paths specified in manifest\n");
			}
		}
	}
	else if (!Q_strcasecmp(fname, "package") || !Q_strcasecmp(fname, "archivedpackage"))
	{
		qboolean crcknown;
		int crc;
		int i, j;
		int arg = 1;

		fname = Cmd_Argv(arg++);

		crcknown = (strcmp(Cmd_Argv(arg), "-") && *Cmd_Argv(arg));
		crc = strtoul(Cmd_Argv(arg++), NULL, 0);

		for (i = 0; i < sizeof(man->package) / sizeof(man->package[0]); i++)
		{
			if (!man->package[i].path)
			{
				man->package[i].path = Z_StrDup(fname);
				man->package[i].crcknown = crcknown;
				man->package[i].crc = crc;
				if (!Q_strcasecmp(fname, "archivedpackage"))
				{
					char *extr = Cmd_Argv(arg++);
					man->package[i].extractname = Z_StrDup(extr);
				}
				else
					man->package[i].extractname = NULL;
				for (j = 0; arg+j < Cmd_Argc() && j < sizeof(man->package[i].mirrors) / sizeof(man->package[i].mirrors[0]); j++)
				{
					man->package[i].mirrors[j] = Z_StrDup(Cmd_Argv(arg+j));
				}
				break;
			}
		}
		if (i == sizeof(man->package) / sizeof(man->package[0]))
		{
			Con_Printf("Too many packages specified in manifest\n");
		}
	}
	else
		result = false;
	return result;
}
//read a manifest file
ftemanifest_t *FS_Manifest_Parse(const char *fname, const char *data)
{
	int hasheaderver = 0;
	ftemanifest_t *man;
	if (!data)
		return NULL;
	while (*data == ' ' || *data == '\t' || *data == '\r' || *data == '\n')
		data++;
	if (!*data)
		return NULL;

	if (!Q_strncasecmp(data, "FTEMANIFEST", 11))
	{
		data = Cmd_TokenizeString((char*)data, false, false);
		hasheaderver = atoi(Cmd_Argv(1));
	}

	man = FS_Manifest_Create(fname);

	while (data && *data)
	{
		data = Cmd_TokenizeString((char*)data, false, false);
		if (!FS_Manifest_ParseTokens(man) && !hasheaderver)
		{	//only support unknown things if there's an actual version specified.
			FS_Manifest_Free(man);
			return NULL;
		}
	}
	if (!man->installation)
	{	//every manifest should have an internal name specified, so we can guess the correct basedir
		//if we don't recognise it, then we'll typically prompt (or just use the working directory), but always assuming a default at least ensures things are sane.
		//fixme: we should probably fill in the basegame here (and share that logic with the legacy manifest generation code)
#ifdef BRANDING_NAME
		data = Cmd_TokenizeString((char*)"game "STRINGIFY(BRANDING_NAME), false, false);
#else
		data = Cmd_TokenizeString((char*)"game quake", false, false);
#endif
		FS_Manifest_ParseTokens(man);
	}

#ifdef SVNREVISION
	//svnrevision is often '-', which means we can't just use it as a constant.
	{
		int ver = atoi(STRINGIFY(SVNREVISION));
		if (man->minver > ver || (man->maxver && man->maxver < ver))
		{
			FS_Manifest_Free(man);
			return NULL;
		}
	}
#endif
	return man;
}

//======================================================================================================


static ftemanifest_t	*fs_manifest;	//currently active manifest.
static searchpath_t	*com_searchpaths;
static searchpath_t	*com_purepaths;
static searchpath_t	*com_base_searchpaths;	// without gamedirs

static int fs_puremode;				//0=deprioritise pure, 1=prioritise pure, 2=pure only.
static char *fs_refnames;			//list of allowed packages
static char *fs_refcrcs;			//list of crcs for those packages. one token per package.
static char *fs_purenames;			//list of allowed packages
static char *fs_purecrcs;			//list of crcs for those packages. one token per package.
static unsigned int fs_pureseed;	//used as a key so the server knows we're obeying. completely unreliable/redundant in an open source project, but needed for q3 network compat.

int QDECL COM_FileSize(const char *path)
{
	int len;
	flocation_t loc;
	len = FS_FLocateFile(path, FSLFRT_LENGTH, &loc);
	return len;
}

//appends a / on the end of the directory if it does not already have one.
void FS_CleanDir(char *out, int outlen)
{
	int olen = strlen(out);
	if (!olen || olen >= outlen-1)
		return;

	if (out[olen-1] == '\\')
		out[olen-1] = '/';
	else if (out[olen-1] != '/')
	{
		out[olen+1] = '\0';
		out[olen] = '/';
	}
}

/*
============
COM_Path_f

============
*/
static void COM_PathLine(searchpath_t *s)
{
	Con_Printf("%s  %s%s%s%s%s\n", s->logicalpath,
		(s->flags & SPF_REFERENCED)?"^[(ref)\\tip\\Referenced\\desc\\Package will auto-download to clients^]":"",
		(s->flags & SPF_TEMPORARY)?"^[(temp)\\tip\\Temporary\\desc\\Flushed on map change^]":"",
		(s->flags & SPF_COPYPROTECTED)?"^[(c)\\tip\\Copyrighted\\desc\\Copy-Protected and is not downloadable^]":"",
		(s->flags & SPF_EXPLICIT)?"^[(e)\\tip\\Explicit\\desc\\Loaded explicitly by the gamedir^]":"",
		(s->flags & SPF_UNTRUSTED)?"^[(u)\\tip\\Untrusted\\desc\\Configs and scripts will not be given access to passwords^]":"" );
}
void COM_Path_f (void)
{
	searchpath_t	*s;

	Con_TPrintf ("Current search path:\n");

	if (com_purepaths || fs_puremode)
	{
		Con_Printf ("Pure paths:\n");
		for (s=com_purepaths ; s ; s=s->nextpure)
		{
			COM_PathLine(s);
		}
		Con_Printf ("----------\n");
		if (fs_puremode == 2)
			Con_Printf ("Inactive paths:\n");
		else
			Con_Printf ("Impure paths:\n");
	}


	for (s=com_searchpaths ; s ; s=s->next)
	{
		if (s == com_base_searchpaths)
			Con_Printf ("----------\n");

		COM_PathLine(s);
	}
}


/*
============
COM_Dir_f

============
*/
static int QDECL COM_Dir_List(const char *name, qofs_t size, void *parm, searchpathfuncs_t *spath)
{
	searchpath_t	*s;
	for (s=com_searchpaths ; s ; s=s->next)
	{
		if (s->handle == spath)
			break;
	}
	if (size > 1.0*1024*1024*1024)
		Con_Printf(U8("%s \t(%#.3ggb) (%s)\n"), name, size/(1024.0*1024*1024), s?s->logicalpath:"??");
	else if (size > 1.0*1024*1024)
		Con_Printf(U8("%s \t(%#.3gmb) (%s)\n"), name, size/(1024.0*1024), s?s->logicalpath:"??");
	else if (size > 1.0*1024)
		Con_Printf(U8("%s \t(%#.3gkb) (%s)\n"), name, size/1024.0, s?s->logicalpath:"??");
	else
		Con_Printf(U8("%s \t(%ub) (%s)\n"), name, (unsigned int)size, s?s->logicalpath:"??");
	return 1;
}

void COM_Dir_f (void)
{
	char match[MAX_QPATH];

	Q_strncpyz(match, Cmd_Argv(1), sizeof(match));
	if (Cmd_Argc()>2)
	{
		strncat(match, "/*.", sizeof(match)-1);
		match[sizeof(match)-1] = '\0';
		strncat(match, Cmd_Argv(2), sizeof(match)-1);
		match[sizeof(match)-1] = '\0';
	}
//	else
//		strncat(match, "/*", sizeof(match)-1);

	COM_EnumerateFiles(match, COM_Dir_List, NULL);
}

/*
============
COM_Locate_f

============
*/
void COM_Locate_f (void)
{
	flocation_t loc;
	if (FS_FLocateFile(Cmd_Argv(1), FSLFRT_LENGTH, &loc)>=0)
	{
		if (!*loc.rawname)
		{
			Con_Printf("File is %u bytes compressed inside %s\n", (unsigned)loc.len, loc.search->logicalpath);
		}
		else
		{
			Con_Printf("Inside %s (%u bytes)\n  %s\n", loc.rawname, (unsigned)loc.len, loc.search->logicalpath);
		}
	}
	else
		Con_Printf("Not found\n");
}

/*
============
COM_WriteFile

The filename will be prefixed by the current game directory
============
*/
void COM_WriteFile (const char *filename, const void *data, int len)
{
	vfsfile_t *vfs;

	Sys_Printf ("COM_WriteFile: %s\n", filename);

	FS_CreatePath(filename, FS_GAMEONLY);
	vfs = FS_OpenVFS(filename, "wb", FS_GAMEONLY);
	if (vfs)
	{
		VFS_WRITE(vfs, data, len);
		VFS_CLOSE(vfs);
	}

	com_fschanged=true;
}

/*
============
COM_CreatePath

Only used for CopyFile and download
============
*/
static void	COM_CreatePath (char *path)
{
	char	*ofs;

	if (fs_readonly)
		return;

	for (ofs = path+1 ; *ofs ; ofs++)
	{
		if (*ofs == '/')
		{	// create the directory
			*ofs = 0;
			Sys_mkdir (path);
			*ofs = '/';
		}
	}
}


/*
===========
COM_CopyFile

Copies a file over from the net to the local cache, creating any directories
needed.  This is for the convenience of developers using ISDN from home.
===========
*/
/*
static void COM_CopyFile (char *netpath, char *cachepath)
{
	FILE	*in, *out;
	int		remaining, count;
	char	buf[4096];

	remaining = COM_FileOpenRead (netpath, &in);
	COM_CreatePath (cachepath);	// create directories up to the cache file
	out = fopen(cachepath, "wb");
	if (!out)
		Sys_Error ("Error opening %s", cachepath);

	while (remaining)
	{
		if (remaining < sizeof(buf))
			count = remaining;
		else
			count = sizeof(buf);
		fread (buf, 1, count, in);
		fwrite (buf, 1, count, out);
		remaining -= count;
	}

	fclose (in);
	fclose (out);
}
//*/

int fs_hash_dups;
int fs_hash_files;

//normally the filesystem drivers pass a pre-allocated bucket and static strings to us
//the OS driver can't really be expected to track things that reliably however, so it just gives names via the stack.
//these files are grouped up to avoid excessive memory allocations.
struct fsbucketblock
{
	struct fsbucketblock *prev;
	int used;
	int total;
	qbyte data[1];
};
static struct fsbucketblock *fs_hash_filebuckets;

void FS_FlushFSHashReally(qboolean domutexes)
{
	COM_AssertMainThread("FS_FlushFSHashReally");
	if (!domutexes || Sys_LockMutex(fs_thread_mutex))
	{
		com_fschanged = true;

		if (filesystemhash.numbuckets)
		{
			int i;
			for (i = 0; i < filesystemhash.numbuckets; i++)
				filesystemhash.bucket[i] = NULL;
		}

		while (fs_hash_filebuckets)
		{
			struct fsbucketblock *n = fs_hash_filebuckets->prev;
			Z_Free(fs_hash_filebuckets);
			fs_hash_filebuckets = n;
		}

		if (domutexes)
			Sys_UnlockMutex(fs_thread_mutex);
	}
}
void FS_FlushFSHashWritten(void)
{
	/*automatically handled*/
}
void FS_FlushFSHashRemoved(void)
{
	FS_FlushFSHashReally(true);
}

static void QDECL FS_AddFileHash(int depth, const char *fname, fsbucket_t *filehandle, void *pathhandle)
{
	fsbucket_t *old;

	old = Hash_GetInsensitiveBucket(&filesystemhash, fname);

	if (old)
	{
		fs_hash_dups++;
		if (depth >= old->depth)
		{
			return;
		}

		//remove the old version
		Hash_RemoveBucket(&filesystemhash, fname, &old->buck);
	}

	if (!filehandle)
	{
		int nlen = strlen(fname)+1;
		if (!fs_hash_filebuckets || fs_hash_filebuckets->used+sizeof(*filehandle)+nlen > fs_hash_filebuckets->total)
		{
			void *o = fs_hash_filebuckets;
			fs_hash_filebuckets = Z_Malloc(65536);
			fs_hash_filebuckets->total = 65536 - sizeof(*fs_hash_filebuckets);
			fs_hash_filebuckets->prev = o;
		}
		filehandle = (fsbucket_t*)(fs_hash_filebuckets->data+fs_hash_filebuckets->used);
		fs_hash_filebuckets->used += sizeof(*filehandle)+nlen;

		if (!filehandle)
			return;	//eep!
		memcpy((char*)(filehandle+1), fname, nlen);
		fname = (char*)(filehandle+1);
	}
	filehandle->depth = depth;

	Hash_AddInsensitive(&filesystemhash, fname, pathhandle, &filehandle->buck);
	fs_hash_files++;
}

void FS_RebuildFSHash(qboolean domutex)
{
	int depth = 1;
	searchpath_t	*search;
	if (!com_fschanged)
		return;

	COM_AssertMainThread("FS_RebuildFSHash");
	if (domutex && !Sys_LockMutex(fs_thread_mutex))
		return;	//amg!
	

	if (!filesystemhash.numbuckets)
	{
		filesystemhash.numbuckets = 1024;
		filesystemhash.bucket = (bucket_t**)Z_Malloc(Hash_BytesForBuckets(filesystemhash.numbuckets));
	}
	else
	{
		FS_FlushFSHashReally(false);
	}
	Hash_InitTable(&filesystemhash, filesystemhash.numbuckets, filesystemhash.bucket);

	fs_hash_dups = 0;
	fs_hash_files = 0;

	if (com_purepaths)
	{	//go for the pure paths first.
		for (search = com_purepaths; search; search = search->nextpure)
		{
			search->handle->BuildHash(search->handle, depth++, FS_AddFileHash);
		}
	}
	if (fs_puremode < 2)
	{
		for (search = com_searchpaths ; search ; search = search->next)
		{
			search->handle->BuildHash(search->handle, depth++, FS_AddFileHash);
		}
	}

	com_fschanged = false;

	if (domutex)
		Sys_UnlockMutex(fs_thread_mutex);

	Con_DPrintf("%i unique files, %i duplicates\n", fs_hash_files, fs_hash_dups);
}

/*
===========
COM_FindFile

Finds the file in the search path.
Sets com_filesize and one of handle or file
===========
*/
//if loc is valid, loc->search is always filled in, the others are filled on success.
//returns -1 if couldn't find.
int FS_FLocateFile(const char *filename, FSLF_ReturnType_e returntype, flocation_t *loc)
{
	int depth=0;
	searchpath_t	*search;
	char cleanpath[MAX_QPATH];
	flocation_t allownoloc;

	void *pf;
	unsigned int found = FF_NOTFOUND;

	if (!loc)
		loc = &allownoloc;

	loc->index = 0;
	loc->offset = 0;
	*loc->rawname = 0;
	loc->search = NULL;
	loc->len = -1;

	filename = FS_GetCleanPath(filename, cleanpath, sizeof(cleanpath));
	if (!filename)
	{
		pf = NULL;
		goto fail;
	}

	if (com_fs_cache.ival && !com_fschanged)
	{
		pf = Hash_GetInsensitive(&filesystemhash, filename);
		if (!pf)
			goto fail;
	}
	else
		pf = NULL;

	if (com_purepaths && found == FF_NOTFOUND)
	{
		//check if its in one of the 'pure' packages. these override the default ones.
		for (search = com_purepaths ; search ; search = search->nextpure)
		{
			depth += ((search->flags & SPF_EXPLICIT) || returntype == FSLFRT_DEPTH_ANYPATH);
			fs_finds++;
			found = search->handle->FindFile(search->handle, loc, filename, pf);
			if (found)
			{
				if (returntype != FSLFRT_DEPTH_OSONLY && returntype != FSLFRT_DEPTH_ANYPATH)
				{
					if ((search->flags & fs_referencetype) != fs_referencetype)
						Con_DPrintf("%s became referenced due to %s\n", search->purepath, filename);
					search->flags |= fs_referencetype;
				}
				loc->search = search;
				break;
			}
		}
	}

	if (fs_puremode < 2 && found == FF_NOTFOUND)
	{
		// optionally check the non-pure paths too.
		for (search = com_searchpaths ; search ; search = search->next)
		{
			depth += ((search->flags & SPF_EXPLICIT) || returntype == FSLFRT_DEPTH_ANYPATH);
			fs_finds++;
			found = search->handle->FindFile(search->handle, loc, filename, pf);
			if (found)
			{
				if (returntype != FSLFRT_DEPTH_OSONLY && returntype != FSLFRT_DEPTH_ANYPATH)
				{
					if ((search->flags & fs_referencetype) != fs_referencetype)
						Con_DPrintf("%s became referenced due to %s\n", search->purepath, filename);
					search->flags |= fs_referencetype;
				}
				loc->search = search;
				break;
			}
		}
	}
fail:
	if (found == FF_SYMLINK)
	{
		static int blocklink;
		if (blocklink < 4 && loc->len < MAX_QPATH)
		{
			//read the link target
			char *s, *b;
			char targname[MAX_QPATH];
			char mergedname[MAX_QPATH];
			targname[loc->len] = 0;
			loc->search->handle->ReadFile(loc->search->handle, loc, targname);

			//properlyish unixify
			while((s = strchr(targname, '\\')))
				*s = '/';
			if (*targname == '/')
				Q_strncpyz(mergedname, targname+1, sizeof(mergedname));
			else
			{
				Q_strncpyz(mergedname, filename, sizeof(mergedname));
				while((s = strchr(mergedname, '\\')))
					*s = '/';
				b = COM_SkipPath(mergedname);
				*b = 0;
				for (s = targname; !strncmp(s, "../", 3) && b > mergedname; )
				{
					s += 3;
					if (b[-1] == '/')
						*--b = 0;
					*b = 0;
					b = strrchr(mergedname, '/');
					if (b)
						*++b = 0;
					else
					{
						//no prefix left.
						*mergedname = 0;
						break;
					}
				}
				b = mergedname + strlen(mergedname);
				Q_strncpyz(b, s, sizeof(mergedname) - (b - mergedname));
			}

			//and locate that instead.
			blocklink++;
			depth = FS_FLocateFile(mergedname, returntype, loc);
			blocklink--;
			if (!loc->search)
				Con_Printf("Symlink %s -> %s (%s) is dead\n", filename, targname, mergedname);
			return depth;
		}
	}

/*	if (len>=0)
	{
		if (loc)
			Con_Printf("Found %s:%i\n", loc->rawname, loc->len);
		else
			Con_Printf("Found %s\n", filename);
	}
	else
		Con_Printf("Failed\n");
*/
	if (returntype == FSLFRT_IFFOUND)
		return (found != FF_NOTFOUND) && (loc->len != -1);
	else if (returntype == FSLFRT_LENGTH)
	{
		if (found == FF_NOTFOUND)
			return -1;
		return loc->len;
	}
	else
	{
		if (found == FF_NOTFOUND)
			return 0x7fffffff;
		return depth;
	}
}

char *FS_WhichPackForLocation(flocation_t *loc, qboolean makereferenced)
{
	char *ret;
	if (!loc->search)
		return NULL;	//huh? not a valid location.

	ret = strchr(loc->search->purepath, '/');
	if (ret)
	{
		ret++;
		if (!strchr(ret, '/'))
		{
			if (makereferenced)
				loc->search->flags |= SPF_REFERENCED;
			return ret;
		}
	}
	return NULL;
}

/*requires extension*/
qboolean FS_GetPackageDownloadable(const char *package)
{
	searchpath_t	*search;

	for (search = com_searchpaths ; search ; search = search->next)
	{
		if (!Q_strcasecmp(package, search->purepath))
			return !(search->flags & SPF_COPYPROTECTED);
	}
	return false;
}

char *FS_GetPackHashes(char *buffer, int buffersize, qboolean referencedonly)
{
	searchpath_t	*search;
	buffersize--;
	*buffer = 0;

	if (com_purepaths)
	{
		for (search = com_purepaths ; search ; search = search->nextpure)
		{
			Q_strncatz(buffer, va("%i ", search->crc_check), buffersize);
		}
		return buffer;
	}
	else
	{
		for (search = com_searchpaths ; search ; search = search->next)
		{
			if (!search->crc_check && search->handle->GeneratePureCRC)
				search->crc_check = search->handle->GeneratePureCRC(search->handle, 0, 0);
			if (search->crc_check)
			{
				Q_strncatz(buffer, va("%i ", search->crc_check), buffersize);
			}
		}
		return buffer;
	}
}
/*
referencedonly=0: show all paks
referencedonly=1: show only paks that are referenced (q3-compat)
referencedonly=2: show all paks, but paks that are referenced are prefixed with a star
ext=0: hide extensions (q3-compat)
ext=1: show extensions.
*/
char *FS_GetPackNames(char *buffer, int buffersize, int referencedonly, qboolean ext)
{
	char temp[MAX_OSPATH];
	searchpath_t	*search;
	buffersize--;
	*buffer = 0;

	if (com_purepaths)
	{
		for (search = com_purepaths ; search ; search = search->nextpure)
		{
			if (referencedonly == 0 && !(search->flags & SPF_REFERENCED))
				continue;
			if (referencedonly == 2 && (search->flags & SPF_REFERENCED))
				Q_strncatz(buffer, "*", buffersize);

			if (!ext)
			{
				COM_StripExtension(search->purepath, temp, sizeof(temp));
				Q_strncatz(buffer, va("%s ", temp), buffersize);
			}
			else
			{
				Q_strncatz(buffer, va("%s ", search->purepath), buffersize);
			}
		}
		return buffer;
	}
	else
	{
		for (search = com_searchpaths ; search ; search = search->next)
		{
			if (!search->crc_check && search->handle->GeneratePureCRC)
				search->crc_check = search->handle->GeneratePureCRC(search->handle, 0, 0);
			if (search->crc_check)
			{
				if (referencedonly == 0 && !(search->flags & SPF_REFERENCED))
					continue;
				if (referencedonly == 2 && (search->flags & SPF_REFERENCED))
				{
					// '*' prefix is meant to mean 'referenced'.
					//really all that means to the client is that it definitely wants to download it.
					//if its copyrighted, the client shouldn't try to do so, as it won't be allowed.
					if (!(search->flags & SPF_COPYPROTECTED))
						Q_strncatz(buffer, "*", buffersize);
				}

				if (!ext)
				{
					COM_StripExtension(search->purepath, temp, sizeof(temp));
					Q_strncatz(buffer, va("%s ", temp), buffersize);
				}
				else
				{
					Q_strncatz(buffer, va("%s ", search->purepath), buffersize);
				}
			}
		}
		return buffer;
	}
}

void FS_ReferenceControl(unsigned int refflag, unsigned int resetflags)
{
	searchpath_t	*s;

	refflag &= SPF_REFERENCED;
	resetflags &= SPF_REFERENCED;

	if (resetflags)
	{
		for (s=com_searchpaths ; s ; s=s->next)
		{
			s->flags &= ~resetflags;
		}
	}

	fs_referencetype = refflag;
}

//outbuf might not be written into
static const char *FS_GetCleanPath(const char *pattern, char *outbuf, int outlen)
{
	const char *s;
	char *o;
	char *seg;

	s = pattern;
	seg = o = outbuf;
	for(;;)
	{
		if (*s == ':')
		{
			if (s == pattern+1 && (s[1] == '/' || s[1] == '\\'))
				Con_Printf("Error: absolute path in filename %s\n", pattern);
			else
				Con_Printf("Error: alternative data stream in filename %s\n", pattern);
			return NULL;
		}
		else if (*s == '\\' || *s == '/' || !*s)
		{	//end of segment
			if (o == seg && *s)
			{
				if (o == outbuf)
				{
					Con_Printf("Error: absolute path in filename %s\n", pattern);
					return NULL;
				}
				Con_Printf("Error: empty directory name (%s)\n", pattern);
				s++;
				continue;
			}
			//ignore any leading spaces in the name segment
			//it should just make more stuff invalid
			while (*seg == ' ')
				seg++;
			if (seg[0] == '.')
			{
				if (o == seg+1)
					Con_Printf("Error: source directory (%s)\n", pattern);
				else if (seg[1] == '.')
					Con_Printf("Error: parent directory (%s)\n", pattern);
				else
					Con_Printf("Error: hidden name (%s)\n", pattern);
				return NULL;
			}
#if defined(_WIN32) || defined(__CYGWIN__)
			//in win32, we use the //?/ trick to get around filename length restrictions.
			//4-letter reserved paths: comX, lptX
			//we'll allow this elsewhere to save cycles, just try to avoid running it on a fat32 or ntfs filesystem from linux
			if (((seg[0] == 'c' || seg[0] == 'C') &&
				 (seg[1] == 'o' || seg[1] == 'O') &&
				 (seg[2] == 'm' || seg[2] == 'M') &&
				 (seg[3] >= '0' && seg[3] <= '9')) ||
				((seg[0] == 'l' || seg[0] == 'L') &&
				 (seg[1] == 'p' || seg[1] == 'P') &&
				 (seg[2] == 't' || seg[2] == 'T') &&
				 (seg[3] >= '0' && seg[3] <= '9')))
			{
				if (o == seg+4 || seg[4] == ' '|| seg[4] == '\t' || seg[4] == '.')
				{
					Con_Printf("Error: reserved name in path (%c%c%c%c in %s)\n", seg[0], seg[1], seg[2], seg[3], pattern);
					return NULL;
				}
			}
			//3 letter reserved paths: con, nul, prn
			if (((seg[0] == 'c' || seg[0] == 'C') &&
				 (seg[1] == 'o' || seg[1] == 'O') &&
				 (seg[2] == 'n' || seg[2] == 'N')) ||
				((seg[0] == 'p' || seg[0] == 'P') &&
				 (seg[1] == 'r' || seg[1] == 'R') &&
				 (seg[2] == 'n' || seg[2] == 'N')) ||
				((seg[0] == 'n' || seg[0] == 'N') &&
				 (seg[1] == 'u' || seg[1] == 'U') &&
				 (seg[2] == 'l' || seg[2] == 'L')))
			{
				if (o == seg+3 || seg[3] == ' '|| seg[3] == '\t' || seg[3] == '.')
				{
					Con_Printf("Error: reserved name in path (%c%c%c in %s)\n", seg[0], seg[1], seg[2], pattern);
					return NULL;
				}
			}
#endif

			if (*s++)
				*o++ = '/';
			else
			{
				*o++ = '\0';
				break;
			}
			seg = o;
		}
		else
			*o++ = *s++;
	}

//	Sys_Printf("%s changed to %s\n", pattern, outbuf);
	return outbuf;
}

vfsfile_t *VFS_Filter(const char *filename, vfsfile_t *handle)
{
//	char *ext;

	if (!handle || handle->WriteBytes || handle->seekingisabadplan)	//only on readonly files
		return handle;
//	ext = COM_FileExtension (filename);
#ifdef AVAIL_ZLIB
//	if (!Q_strcasecmp(ext, ".gz"))
	{
		return FS_DecompressGZip(handle, NULL);
	}
#endif
	return handle;
}

qboolean FS_NativePath(const char *fname, enum fs_relative relativeto, char *out, int outlen)
{
	char *last;
	int i;
	char cleanname[MAX_QPATH];

	if (relativeto == FS_SYSTEM)
	{
		//system is already the native path. we can just pass it through. perhaps we should clean it up first however, although that's just making sure all \ are /
		snprintf(out, outlen, "%s", fname);

		for (; *out; out++)
		{
			if (*out == '\\')
				*out = '/';
		}
		return true;
	}

	fname = FS_GetCleanPath(fname, cleanname, sizeof(cleanname));
	if (!fname)
		return false;

	switch (relativeto)
	{
	case FS_GAMEONLY:
	case FS_GAME:
		if (com_homepathenabled)
			snprintf(out, outlen, "%s%s/%s", com_homepath, gamedirfile, fname);
		else
			snprintf(out, outlen, "%s%s/%s", com_gamepath, gamedirfile, fname);
		break;
	case FS_BINARYPATH:
		if (host_parms.binarydir && *host_parms.binarydir)
			snprintf(out, outlen, "%s%s", host_parms.binarydir, fname);
		else
			snprintf(out, outlen, "%s%s", host_parms.basedir, fname);
		break;
	case FS_ROOT:
		if (com_homepathenabled)
			snprintf(out, outlen, "%s%s", com_homepath, fname);
		else
			snprintf(out, outlen, "%s%s", com_gamepath, fname);
		break;
	case FS_BASEGAMEONLY:
		last = NULL;
		for (i = 0; i < sizeof(fs_manifest->gamepath)/sizeof(fs_manifest->gamepath[0]); i++)
		{
			if (fs_manifest->gamepath[i].base && fs_manifest->gamepath[i].path)
			{
				if (!strcmp(fs_manifest->gamepath[i].path, "*"))
					continue;
				last = fs_manifest->gamepath[i].path;
				if (*last == '*')
					last++;
			}
		}
		if (!last)
			return false;	//eep?
		if (com_homepathenabled)
			snprintf(out, outlen, "%s%s/%s", com_homepath, last, fname);
		else
			snprintf(out, outlen, "%s%s/%s", com_gamepath, last, fname);
		break;
	case FS_PUBBASEGAMEONLY:
		last = NULL;
		for (i = 0; i < sizeof(fs_manifest->gamepath)/sizeof(fs_manifest->gamepath[0]); i++)
		{
			if (fs_manifest->gamepath[i].base && fs_manifest->gamepath[i].path)
			{
				if (*fs_manifest->gamepath[i].path == '*')
					continue;
				last = fs_manifest->gamepath[i].path;
			}
		}
		if (!last)
			return false;	//eep?
		if (com_homepathenabled)
			snprintf(out, outlen, "%s%s/%s", com_homepath, last, fname);
		else
			snprintf(out, outlen, "%s%s/%s", com_gamepath, last, fname);
		break;
	default:
		Sys_Error("FS_NativePath case not handled\n");
	}
	return true;
}

/*locates and opens a file*/
vfsfile_t *FS_OpenVFS(const char *filename, const char *mode, enum fs_relative relativeto)
{
	char cleanname[MAX_QPATH];
	char fullname[MAX_OSPATH];
	flocation_t loc;
	vfsfile_t *vfs;

	//eventually, this function will be the *ONLY* way to get at files

	if (relativeto == FS_SYSTEM)
		return VFSOS_Open(filename, mode);

	//blanket-bans

	filename = FS_GetCleanPath(filename, cleanname, sizeof(cleanname));
	if (!filename)
		return NULL;

#ifdef _DEBUG
	if (strcmp(mode, "rb"))
		if (strcmp(mode, "r+b"))
			if (strcmp(mode, "wb"))
				if (strcmp(mode, "w+b"))
					if (strcmp(mode, "ab"))
						if (strcmp(mode, "wbp"))
							return NULL; //urm, unable to write/append
#endif

	//if there can only be one file (eg: write access) find out where it is.
	switch (relativeto)
	{
	case FS_GAMEONLY:	//OS access only, no paks
		if (com_homepathenabled)
		{
			snprintf(fullname, sizeof(fullname), "%s%s/%s", com_homepath, gamedirfile, filename);
			if (*mode == 'w')
				COM_CreatePath(fullname);
			vfs = VFSOS_Open(fullname, mode);
			if (vfs)
				return vfs;
		}
		if (*gamedirfile)
		{
			snprintf(fullname, sizeof(fullname), "%s%s/%s", com_gamepath, gamedirfile, filename);
			if (*mode == 'w')
				COM_CreatePath(fullname);
			return VFSOS_Open(fullname, mode);
		}
		return NULL;
	case FS_GAME:	//load from paks in preference to system paths. overwriting be damned.
	case FS_PUBBASEGAMEONLY:	//load from paks in preference to system paths. overwriting be damned.
		FS_NativePath(filename, relativeto, fullname, sizeof(fullname));
		break;
	case FS_BINARYPATH:
		if (*mode == 'w')
			COM_CreatePath(fullname);
		FS_NativePath(filename, relativeto, fullname, sizeof(fullname));
		return VFSOS_Open(fullname, mode);
	case FS_ROOT:	//always bypass packs and gamedirs
		if (com_homepathenabled)
		{
			snprintf(fullname, sizeof(fullname), "%s%s", com_homepath, filename);
			vfs = VFSOS_Open(fullname, mode);
			if (vfs)
				return vfs;
		}
		snprintf(fullname, sizeof(fullname), "%s%s", com_gamepath, filename);
		return VFSOS_Open(fullname, mode);
	case FS_BASEGAMEONLY:		//always bypass packs+pure.
		if (com_homepathenabled)
		{
			snprintf(fullname, sizeof(fullname), "%sfte/%s", com_homepath, filename);
			vfs = VFSOS_Open(fullname, mode);
			if (vfs)
				return vfs;
		}
		snprintf(fullname, sizeof(fullname), "%sfte/%s", com_gamepath, filename);
		return VFSOS_Open(fullname, mode);
	default:
		Sys_Error("FS_OpenVFS: Bad relative path (%i)", relativeto);
		break;
	}

	FS_FLocateFile(filename, FSLFRT_IFFOUND, &loc);

	if (loc.search)
	{
		return VFS_Filter(filename, loc.search->handle->OpenVFS(loc.search->handle, &loc, mode));
	}

	//if we're meant to be writing, best write to it.
	if (strchr(mode , 'w') || strchr(mode , 'a'))
	{
		COM_CreatePath(fullname);
		return VFSOS_Open(fullname, mode);
	}
	return NULL;
}

/*opens a vfsfile from an already discovered location*/
vfsfile_t *FS_OpenReadLocation(flocation_t *location)
{
	if (location->search)
	{
		return VFS_Filter(NULL, location->search->handle->OpenVFS(location->search->handle, location, "rb"));
	}
	return NULL;
}

qboolean FS_Rename2(const char *oldf, const char *newf, enum fs_relative oldrelativeto, enum fs_relative newrelativeto)
{
	char oldfullname[MAX_OSPATH];
	char newfullname[MAX_OSPATH];

	if (!FS_NativePath(oldf, oldrelativeto, oldfullname, sizeof(oldfullname)))
		return false;
	if (!FS_NativePath(newf, newrelativeto, newfullname, sizeof(newfullname)))
		return false;

	FS_CreatePath(newf, newrelativeto);
	return Sys_Rename(oldfullname, newfullname);
}
qboolean FS_Rename(const char *oldf, const char *newf, enum fs_relative relativeto)
{
	return FS_Rename2(oldf, newf, relativeto, relativeto);
}
qboolean FS_Remove(const char *fname, enum fs_relative relativeto)
{
	char fullname[MAX_OSPATH];

	if (!FS_NativePath(fname, relativeto, fullname, sizeof(fullname)))
		return false;

	return Sys_remove (fullname);
}
//create a path for the given filename (dir-only must have trailing slash)
void FS_CreatePath(const char *pname, enum fs_relative relativeto)
{
	char fullname[MAX_OSPATH];
	if (!FS_NativePath(pname, relativeto, fullname, sizeof(fullname)))
		return;

	COM_CreatePath(fullname);
}

void *FS_MallocFile(const char *filename, enum fs_relative relativeto, qofs_t *filesize)
{
	vfsfile_t *f;
	qbyte *buf;
	qofs_t len;

	f = FS_OpenVFS(filename, "rb", relativeto);
	if (!f)
		return NULL;
	len = VFS_GETLEN(f);
	if (filesize)
		*filesize = len;

	buf = (qbyte*)BZ_Malloc(len+1);
	if (!buf)
		Sys_Error ("FS_MallocFile: out of memory loading %s", filename);

	((qbyte *)buf)[len] = 0;

	VFS_READ(f, buf, len);
	VFS_CLOSE(f);
	return buf;
}
qboolean FS_WriteFile (const char *filename, const void *data, int len, enum fs_relative relativeto)
{
	vfsfile_t *f;
	FS_CreatePath(filename, relativeto);
	f = FS_OpenVFS(filename, "wb", relativeto);
	if (!f)
		return false;
	VFS_WRITE(f, data, len);
	VFS_CLOSE(f);

	return true;
}

qboolean FS_Copy(const char *source, const char *dest, enum fs_relative relativesource, enum fs_relative relativedest)
{
	vfsfile_t *d, *s;
	char buffer[8192*8];
	int read;
	qboolean result = false;
	FS_CreatePath(dest, relativedest);
	s = FS_OpenVFS(source, "rb", relativesource);
	if (s)
	{
		d = FS_OpenVFS(dest, "wbp", relativedest);
		if (d)
		{
			result = true;

			for (;;)
			{
				read = VFS_READ(s, buffer, sizeof(buffer));
				if (read <= 0)
					break;
				if (VFS_WRITE(d, buffer, read) != read)
				{
					result = false;
					break;
				}
			}

			VFS_CLOSE(d);

			if (!result)
				FS_Remove(dest, relativedest);
		}
		VFS_CLOSE(s);
	}
	return result;
}

static qbyte	*loadbuf;
static int		loadsize;

/*
============
COM_LoadFile

Filename are reletive to the quake directory.
Always appends a 0 qbyte to the loaded data.
============
*/
qbyte *COM_LoadFile (const char *path, int usehunk, size_t *filesize)
{
	vfsfile_t *f;
	qbyte *buf;
	qofs_t len;
	flocation_t loc;
	FS_FLocateFile(path, FSLFRT_LENGTH, &loc);

	if (!loc.search)
		return NULL;	//wasn't found

	if (loc.len > 0x7fffffff)	//don't malloc 5000gb sparse files or anything crazy on a 32bit system...
		return NULL;

	f = loc.search->handle->OpenVFS(loc.search->handle, &loc, "rb");
	if (!f)
		return NULL;

	len = VFS_GETLEN(f);
	if (filesize)
		*filesize = len;

	if (usehunk == 2 || usehunk == 4 || usehunk == 6)
		COM_AssertMainThread("COM_LoadFile+hunk");

	if (usehunk == 0)
		buf = (qbyte*)Z_Malloc (len+1);
	else if (usehunk == 2)
		buf = (qbyte*)Hunk_TempAlloc (len+1);
	else if (usehunk == 4)
	{
		if (len+1 > loadsize)
			buf = (qbyte*)Hunk_TempAlloc (len+1);
		else
			buf = loadbuf;
	}
	else if (usehunk == 5)
		buf = (qbyte*)BZ_Malloc(len+1);
	else if (usehunk == 6)
		buf = (qbyte*)Hunk_TempAllocMore (len+1);
	else
	{
		Sys_Error ("COM_LoadFile: bad usehunk");
		buf = NULL;
	}

	if (!buf)
		Sys_Error ("COM_LoadFile: not enough space for %s", path);

	((qbyte *)buf)[len] = 0;

	VFS_READ(f, buf, len);
	VFS_CLOSE(f);

	return buf;
}

qbyte *FS_LoadMallocFile (const char *path, size_t *fsize)
{
	return COM_LoadFile (path, 5, fsize);
}

void *FS_LoadMallocGroupFile(zonegroup_t *ctx, char *path, size_t *fsize)
{
	char *mem = NULL;
	vfsfile_t *f = FS_OpenVFS(path, "rb", FS_GAME);
	if (f)
	{
		int len = VFS_GETLEN(f);
		mem = ZG_Malloc(ctx, len+1);
		if (mem)
		{
			mem[len] = 0;
			if (VFS_READ(f, mem, len) == len)
				*fsize = len;
			else
				mem = NULL;
		}

		VFS_CLOSE(f);
	}
	return mem;
}

qbyte *COM_LoadTempFile (const char *path, size_t *fsize)
{
	return COM_LoadFile (path, 2, fsize);
}
qbyte *COM_LoadTempMoreFile (const char *path, size_t *fsize)
{
	return COM_LoadFile (path, 6, fsize);
}

// uses temp hunk if larger than bufsize
qbyte *QDECL COM_LoadStackFile (const char *path, void *buffer, int bufsize, size_t *fsize)
{
	qbyte	*buf;

	COM_AssertMainThread("COM_LoadStackFile");

	loadbuf = (qbyte *)buffer;
	loadsize = bufsize;
	buf = COM_LoadFile (path, 4, fsize);

	return buf;
}


/*warning: at some point I'll change this function to return only read-only buffers*/
qofs_t FS_LoadFile(const char *name, void **file)
{
	size_t fsz;
	*file = COM_LoadFile (name, 5, &fsz);
	if (!*file)
		return (qofs_t)-1;
	return fsz;
}
void FS_FreeFile(void *file)
{
	BZ_Free(file);
}



void COM_EnumerateFiles (const char *match, int (QDECL *func)(const char *, qofs_t, void *, searchpathfuncs_t*), void *parm)
{
	searchpath_t    *search;
	for (search = com_searchpaths; search ; search = search->next)
	{
	// is the element a pak file?
		if (!search->handle->EnumerateFiles(search->handle, match, func, parm))
			break;
	}
}

void COM_FlushTempoaryPacks(void)
{
#if 0
	searchpath_t *sp, **link;
	link = &com_searchpaths;
	while (*link)
	{
		sp = *link;
		if (sp->flags & SPF_TEMPORARY)
		{
			FS_FlushFSHashReally();

			*link = sp->next;

			sp->handle->ClosePath(sp->handle);
			Z_Free (sp);
		}
		else
			link = &sp->next;
	}
	com_purepaths = NULL;
#endif
}

qboolean COM_LoadMapPackFile (const char *filename, qofs_t ofs)
{
	return false;
}

static searchpath_t *FS_AddPathHandle(searchpath_t **oldpaths, const char *purepath, const char *probablepath, searchpathfuncs_t *handle, unsigned int flags, unsigned int loadstuff);
searchpathfuncs_t *FS_GetOldPath(searchpath_t **oldpaths, const char *dir, unsigned int *keepflags)
{
	searchpath_t *p;
	searchpathfuncs_t *r = NULL;
	*keepflags = 0;
	while(*oldpaths)
	{
		p = *oldpaths;

		if (!Q_strcasecmp(p->logicalpath, dir))
		{
			*keepflags |= p->flags & (SPF_REFERENCED | SPF_UNTRUSTED);
			*oldpaths = p->next;
			r = p->handle;
			Z_Free(p);
			break;
		}

		oldpaths = &(*oldpaths)->next;
	}
	return r;
}

typedef struct {
	searchpathfuncs_t *(QDECL *OpenNew)(vfsfile_t *file, const char *desc);
	searchpath_t **oldpaths;
	const char *parentdesc;
	const char *puredesc;
} wildpaks_t;

static int QDECL FS_AddWildDataFiles (const char *descriptor, qofs_t size, void *vparam, searchpathfuncs_t *funcs)
{
	wildpaks_t *param = vparam;
	vfsfile_t *vfs;
	searchpath_t	*search;
	searchpathfuncs_t	*newpak;
	char			pakfile[MAX_OSPATH];
	char			purefile[MAX_OSPATH];
	flocation_t loc;
	unsigned int keptflags = 0;

	Q_snprintfz (pakfile, sizeof(pakfile), "%s%s", param->parentdesc, descriptor);

	for (search = com_searchpaths; search; search = search->next)
	{
		if (!Q_strcasecmp(search->logicalpath, pakfile))	//assumption: first member of structure is a char array
			return true; //already loaded (base paths?)
	}

	newpak = FS_GetOldPath(param->oldpaths, pakfile, &keptflags);
	if (!newpak)
	{
		if (param->OpenNew == VFSOS_OpenPath)
		{
			vfs = NULL;
		}
		else
		{
			fs_finds++;
			if (!funcs->FindFile(funcs, &loc, descriptor, NULL))
				return true;	//not found..
			vfs = funcs->OpenVFS(funcs, &loc, "rb");
			if (!vfs)
				return true;
		}
		newpak = param->OpenNew (vfs, pakfile);
		if (!newpak)
		{
			VFS_CLOSE(vfs);
			return true;
		}
	}

	Q_snprintfz (pakfile, sizeof(pakfile), "%s%s", param->parentdesc, descriptor);
	if (*param->puredesc)
		snprintf (purefile, sizeof(purefile), "%s/%s", param->puredesc, descriptor);
	else
		Q_strncpyz(purefile, descriptor, sizeof(purefile));
	FS_AddPathHandle(param->oldpaths, purefile, pakfile, newpak, ((!Q_strncasecmp(descriptor, "pak", 3))?SPF_COPYPROTECTED:0)|keptflags, (unsigned int)-1);

	return true;
}

searchpathfuncs_t *FS_OpenPackByExtension(vfsfile_t *f, const char *pakname)
{
	searchpathfuncs_t *pak;
	int j;
	char ext[8];
	COM_FileExtension(pakname, ext, sizeof(ext));
	for (j = 0; j < sizeof(searchpathformats)/sizeof(searchpathformats[0]); j++)
	{
		if (!searchpathformats[j].extension || !searchpathformats[j].OpenNew)
			continue;
		if (!strcmp(ext, searchpathformats[j].extension))
		{
			pak = searchpathformats[j].OpenNew(f, pakname);
			if (pak)
				return pak;
			Con_Printf("Unable to open %s - corrupt?\n", pakname);
			break;
		}
	}

	VFS_CLOSE(f);
	return NULL;
}

static void FS_AddDataFiles(searchpath_t **oldpaths, const char *purepath, const char *logicalpath, searchpath_t *search, const char *extension, searchpathfuncs_t *(QDECL *OpenNew)(vfsfile_t *file, const char *desc))
{
	//search is the parent
	int				i;
	searchpathfuncs_t	*handle;
	char			pakfile[MAX_OSPATH];
	char			logicalpaths[MAX_OSPATH];	//with a slash
	char			purefile[MAX_OSPATH];
	unsigned int	keptflags;
	vfsfile_t *vfs;
	flocation_t loc;
	wildpaks_t wp;

	Q_strncpyz(logicalpaths, logicalpath, sizeof(logicalpaths));
	FS_CleanDir(logicalpaths, sizeof(logicalpaths));

	//first load all the numbered pak files
	for (i=0 ; ; i++)
	{
		snprintf (pakfile, sizeof(pakfile), "pak%i.%s", i, extension);
		fs_finds++;
		if (!search->handle->FindFile(search->handle, &loc, pakfile, NULL))
			break;	//not found..

		snprintf (pakfile, sizeof(pakfile), "%spak%i.%s", logicalpaths, i, extension);
		snprintf (purefile, sizeof(purefile), "%s/pak%i.%s", purepath, i, extension);

		handle = FS_GetOldPath(oldpaths, pakfile, &keptflags);
		if (!handle)
		{
			vfs = search->handle->OpenVFS(search->handle, &loc, "r");
			if (!vfs)
				break;
			handle = OpenNew (vfs, pakfile);
			if (!handle)
				break;
		}
		FS_AddPathHandle(oldpaths, purefile, pakfile, handle, SPF_COPYPROTECTED|keptflags, (unsigned int)-1);
	}

	//now load the random ones
	Q_snprintfz (pakfile, sizeof(pakfile), "*.%s", extension);
	wp.OpenNew = OpenNew;
	wp.parentdesc = logicalpaths;
	wp.puredesc = purepath;
	wp.oldpaths = oldpaths;
	search->handle->EnumerateFiles(search->handle, pakfile, FS_AddWildDataFiles, &wp);


	//and load any named in the manifest (this happens when they're crced or whatever)
	{
		int ptlen, palen;
		ptlen = strlen(purepath);
		for (i = 0; i < sizeof(fs_manifest->package) / sizeof(fs_manifest->package[0]); i++)
		{
			char ext[8];
			if (fs_manifest->package[i].path && !strcmp(COM_FileExtension(fs_manifest->package[i].path, ext, sizeof(ext)), extension))
			{
				palen = strlen(fs_manifest->package[i].path);
				if (palen > ptlen && (fs_manifest->package[i].path[ptlen] == '/' || fs_manifest->package[i].path[ptlen] == '\\' )&& !strncmp(purepath, fs_manifest->package[i].path, ptlen))
				{
					searchpath_t *oldp;
					char pname[MAX_OSPATH];
					char lname[MAX_OSPATH];
					if (fs_manifest->package[i].crcknown)
						snprintf(lname, sizeof(lname), "%#x", fs_manifest->package[i].crc);
					else
						snprintf(lname, sizeof(lname), "");
					if (!FS_GenCachedPakName(fs_manifest->package[i].path, lname, pname, sizeof(pname)))
						continue;
					snprintf (lname, sizeof(lname), "%s%s", logicalpaths, pname+ptlen+1);

					for (oldp = com_searchpaths; oldp; oldp = oldp->next)
					{
						if (!Q_strcasecmp(oldp->purepath, fs_manifest->package[i].path))
							break;
						if (!Q_strcasecmp(oldp->logicalpath, lname))
							break;
					}
					if (!oldp)
					{
						handle = FS_GetOldPath(oldpaths, lname, &keptflags);
						if (!handle)
						{
							if (search->handle->FindFile(search->handle, &loc, pname+ptlen+1, NULL))
							{
								vfs = search->handle->OpenVFS(search->handle, &loc, "r");
								if (vfs)
									handle = OpenNew (vfs, lname);
							}
						}
						if (handle && fs_manifest->package[i].crcknown)
						{
							int truecrc = handle->GeneratePureCRC(handle, 0, false);
							if (truecrc != fs_manifest->package[i].crc)
							{
								Con_Printf(CON_ERROR "File \"%s\" has hash %#x (required: %#x). Please delete it or move it away\n", lname, truecrc, fs_manifest->package[i].crc);
								handle->ClosePath(handle);
								handle = NULL;
							}
						}
						if (handle)
							FS_AddPathHandle(oldpaths, fs_manifest->package[i].path, lname, handle, SPF_COPYPROTECTED|SPF_UNTRUSTED|keptflags, (unsigned int)-1);
					}
				}
			}
		}
	}
}

static searchpath_t *FS_AddPathHandle(searchpath_t **oldpaths, const char *purepath, const char *logicalpath, searchpathfuncs_t *handle, unsigned int flags, unsigned int loadstuff)
{
	unsigned int i;

	searchpath_t *search, **link;

	if (!handle)
	{
		Con_Printf("COM_AddPathHandle: not a valid handle (%s)\n", logicalpath);
		return NULL;
	}

	if (handle->fsver != FSVER)
	{
		Con_Printf("%s: file system driver is outdated (%u should be %u)\n", logicalpath, handle->fsver, FSVER);
		handle->ClosePath(handle);
		return NULL;
	}

	search = (searchpath_t*)Z_Malloc (sizeof(searchpath_t));
	search->flags = flags;
	search->handle = handle;
	Q_strncpyz(search->purepath, purepath, sizeof(search->purepath));
	Q_strncpyz(search->logicalpath, logicalpath, sizeof(search->logicalpath));

	//temp packages also do not nest
	if (!(flags & SPF_TEMPORARY))
	{
		for (i = 0; i < sizeof(searchpathformats)/sizeof(searchpathformats[0]); i++)
		{
			if (!searchpathformats[i].extension || !searchpathformats[i].OpenNew || !searchpathformats[i].loadscan)
				continue;
			if (loadstuff & (1<<i))
			{
				FS_AddDataFiles(oldpaths, purepath, logicalpath, search, searchpathformats[i].extension, searchpathformats[i].OpenNew);
			}
		}
	}

	if (flags & SPF_TEMPORARY)
	{
		//add at end. pureness will reorder if needed.
		link = &com_searchpaths;
		while(*link)
		{
			link = &(*link)->next;
		}
		*link = search;
	}
	else
	{
		search->next = com_searchpaths;
		com_searchpaths = search;
	}
	com_fschanged = true;

	return search;
}

void COM_RefreshFSCache_f(void)
{
	com_fschanged=true;
}

//optionally purges the cache and rebuilds it
void COM_FlushFSCache(qboolean purge, qboolean domutex)
{
	searchpath_t *search;
	if (com_fs_cache.ival && com_fs_cache.ival != 2)
	{
		for (search = com_searchpaths ; search ; search = search->next)
		{
			if (search->handle->PollChanges)
				com_fschanged |= search->handle->PollChanges(search->handle);
		}
	}

#ifdef FTE_TARGET_WEB
	//web target doesn't support filesystem enumeration, so make sure the cache is kept invalid and disabled.
	com_fschanged = true;
#else
	if (com_fs_cache.ival && com_fschanged)
	{
		//rebuild it if needed
		FS_RebuildFSHash(domutex);
	}
#endif
}

/*since should start as 0, otherwise this can be used to poll*/
qboolean FS_Restarted(unsigned int *since)
{
	if (*since < fs_restarts)
	{
		*since = fs_restarts;
		return true;
	}
	return false;
}

/*
================
FS_AddGameDirectory

Sets com_gamedir, adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ...
================
*/
void FS_AddGameDirectory (searchpath_t **oldpaths, const char *puredir, const char *dir, unsigned int loadstuff, unsigned int flags)
{
	unsigned int	keptflags;
	searchpath_t	*search;

	char			*p;
	void			*handle;

	fs_restarts++;

	for (search = com_searchpaths; search; search = search->next)
	{
		if (!Q_strcasecmp(search->logicalpath, dir))
			return; //already loaded (base paths?)
	}

	if (!(flags & SPF_PRIVATE))
	{
		if ((p = strrchr(dir, '/')) != NULL)
			strcpy(pubgamedirfile, ++p);
		else
			strcpy(pubgamedirfile, dir);
	}
	if ((p = strrchr(dir, '/')) != NULL)
		strcpy(gamedirfile, ++p);
	else
		strcpy(gamedirfile, dir);

//
// add the directory to the search path
//
	handle = FS_GetOldPath(oldpaths, dir, &keptflags);
	if (!handle)
		handle = VFSOS_OpenPath(NULL, dir);

	FS_AddPathHandle(oldpaths, puredir, dir, handle, flags|keptflags, loadstuff);
}

//if syspath, something like c:\quake\baseq2
//otherwise just baseq2. beware of dupes.
searchpathfuncs_t *COM_IteratePaths (void **iterator, char *pathbuffer, int pathbuffersize, char *dirname, int dirnamesize)
{
	searchpath_t	*s;
	void			*prev;

	prev = NULL;
	for (s=com_searchpaths ; s ; s=s->next)
	{
		if (!(s->flags & SPF_EXPLICIT))
			continue;

		if (*iterator == prev)
		{
			*iterator = s->handle;
			if (!strchr(s->purepath, '/'))
			{
				if (pathbuffer)
				{
					Q_strncpyz(pathbuffer, s->logicalpath, pathbuffersize-1);
					FS_CleanDir(pathbuffer, pathbuffersize);
				}
				if (dirname)
				{
					Q_strncpyz(dirname, s->purepath, dirnamesize-1);
				}
				return s->handle;
			}
		}
		prev = s->handle;
	}

	*iterator = NULL;
	if (pathbuffer)
		*pathbuffer = 0;
	if (dirname)
		*dirname = 0;
	return NULL;
}

char *FS_GetGamedir(qboolean publicpathonly)
{
	if (publicpathonly)
		return pubgamedirfile;
	else
		return gamedirfile;
}

//returns the commandline arguments required to duplicate the fs details
char *FS_GetManifestArgs(void)
{
	char *homearg = com_homepathenabled?"-usehome ":"-nohome ";
	if (fs_manifest->updatefile)
		return va("%s-manifest %s -basedir %s -outputdebugstring", homearg, fs_manifest->updatefile, com_gamepath);
	
	return va("%s-game %s -basedir %s -outputdebugstring", homearg, pubgamedirfile, com_gamepath);
}

//given a 'c:/foo/bar/' path, will extract 'bar'.
void FS_ExtractDir(char *in, char *out, int outlen)
{
	char *end;
	if (!outlen)
		return;
	end = in + strlen(in);
	//skip over any trailing slashes
	while (end > in)
	{
		if (end[-1] == '/' || end[-1] == '\\')
			end--;
		else
			break;
	}

	//skip over the path
	while (end > in)
	{
		if (end[-1] != '/' && end[-1] != '\\')
			end--;
		else
			break;
	}

	//copy string into the dest
	while (--outlen)
	{
		if (*end == '/' || *end == '\\' || !*end)
			break;
		*out++ = *end++;
	}
	*out = 0;
}

/*
================
COM_Gamedir

Sets the gamedir and path to a different directory.
================
*/
void COM_Gamedir (const char *dir)
{
	ftemanifest_t *man;
	if (!fs_manifest)
		FS_ChangeGame(NULL, true);

	//don't allow leading dots, hidden files are evil.
	//don't allow complex paths. those are evil too.
	if (!*dir || *dir == '.' || !strcmp(dir, ".") || strstr(dir, "..") || strstr(dir, "/")
		|| strstr(dir, "\\") || strstr(dir, ":") )
	{
		Con_Printf ("Gamedir should be a single filename, not a path\n");
		return;
	}

	man = NULL;
	if (!man)
	{
		vfsfile_t *f = VFSOS_Open(va("%s%s.fmf", com_gamepath, dir), "rb");
		if (f)
		{
			size_t len = VFS_GETLEN(f);
			char *fdata = BZ_Malloc(len+1);
			if (fdata)
			{
				VFS_READ(f, fdata, len);
				fdata[len] = 0;
				man = FS_Manifest_Parse(NULL, fdata);
				BZ_Free(fdata);
			}
			VFS_CLOSE(f);
		}
	}

	if (!man)
	{
		//generate a new manifest based upon the current one.
		man = FS_ReadDefaultManifest(com_gamepath, sizeof(com_gamepath), true);
		if (man && strcmp(man->installation, fs_manifest->installation))
		{
			FS_Manifest_Free(man);
			man = NULL;
		}
		if (!man)
			man = FS_Manifest_Clone(fs_manifest);
		FS_Manifest_PurgeGamedirs(man);
		if (*dir)
		{
			char token[MAX_QPATH];
			char *dup = Z_StrDup(dir);	//FIXME: is this really needed?
			dir = dup;
			while ((dir = COM_ParseStringSet(dir, token, sizeof(token))))
			{
				if (!strcmp(dir, ";"))
					continue;
				if (!*token)
					continue;

				Cmd_TokenizeString(va("gamedir \"%s\"", token), false, false);
				FS_Manifest_ParseTokens(man);
			}
			Z_Free(dup);
		}
	}
	FS_ChangeGame(man, cfg_reload_on_gamedir.ival);
}

#define QCFG "set allow_download_refpackages 0\n"
/*stuff that makes dp-only mods work a bit better*/
#define DPCOMPAT QCFG "set _cl_playermodel \"\"\n set dpcompat_set 1\nset dpcompat_corruptglobals 1\nset vid_pixelheight 1\n"
/*nexuiz/xonotic has a few quirks/annoyances...*/
#define NEXCFG DPCOMPAT "set r_particlesdesc effectinfo\nset sv_bigcoords 1\nset sv_maxairspeed \"400\"\nset sv_jumpvelocity 270\nset sv_mintic \"0.01\"\ncl_nolerp 0\npr_enable_uriget 0\n"
/*some modern non-compat settings*/
#define DMFCFG "set com_parseutf8 1\npm_airstep 1\nsv_demoExtensions 1\n"
/*set some stuff so our regular qw client appears more like hexen2. sv_mintic is required to 'fix' the ravenstaff so that its projectiles don't impact upon each other*/
#define HEX2CFG "set com_parseutf8 -1\nset gl_font gfx/hexen2\nset in_builtinkeymap 0\nset_calc cl_playerclass int (random * 5) + 1\nset sv_maxspeed 640\ncl_run 0\nset watervis 1\nset r_lavaalpha 1\nset r_lavastyle -2\nset r_wateralpha 0.5\nset sv_pupglow 1\ngl_shaftlight 0.5\nsv_mintic 0.015\nset mod_warnmodels 0\nset cl_model_bobbing 1\nsv_sound_land \"fx/thngland.wav\"\n"
/*yay q2!*/
#define Q2CFG "com_nogamedirnativecode 0\n"
/*Q3's ui doesn't like empty model/headmodel/handicap cvars, even if the gamecode copes*/
#define Q3CFG "gl_overbright 2\nseta model sarge\nseta headmodel sarge\nseta handicap 100\ncom_nogamedirnativecode 0\n"
//#define RMQCFG "sv_bigcoords 1\n"

typedef struct {
	const char *argname;	//used if this was used as a parameter.
	const char *exename;	//used if the exe name contains this
	const char *protocolname;	//sent to the master server when this is the current gamemode (Typically set for DP compat).
	const char *auniquefile[4];	//used if this file is relative from the gamedir. needs just one file

	const char *customexec;

	const char *dir[4];
	const char *poshname;	//Full name for the game.
	const char *manifestfile;
} gamemode_info_t;
const gamemode_info_t gamemode_info[] = {
#define MASTER_PREFIX "FTE-"
//note that there is no basic 'fte' gamemode, this is because we aim for network compatability. Darkplaces-Quake is the closest we get.
//this is to avoid having too many gamemodes anyway.

//mission packs should generally come after the main game to avoid prefering the main game. we violate this for hexen2 as the mission pack is mostly a superset.
//whereas the quake mission packs replace start.bsp making the original episodes unreachable.
//for quake, we also allow extracting all files from paks. some people think it loads faster that way or something.

	//cmdline switch exename    protocol name(dpmaster)  identifying file		exec     dir1       dir2    dir3       dir(fte)     full name
	{"-quake",		"q1",		MASTER_PREFIX"Quake",	{"id1/pak0.pak",
														 "id1/quake.rc"},		QCFG,	{"id1",		"qw",				"*fte"},		"Quake"/*,    "id1/pak0.pak|http://quakeservers.nquake.com/qsw106.zip|http://nquake.localghost.net/qsw106.zip|http://qw.quakephil.com/nquake/qsw106.zip|http://fnu.nquake.com/qsw106.zip"*/},
	{"-hipnotic",	"hipnotic",	MASTER_PREFIX"Hipnotic",{"hipnotic/pak0.pak"},	QCFG,	{"id1",		"qw",	"hipnotic",	"*fte"},		"Quake: Scourge of Armagon"},
	{"-rogue",		"rogue",	MASTER_PREFIX"Rogue",	{"rogue/pak0.pak"},		QCFG,	{"id1",		"qw",	"rogue",	"*fte"},		"Quake: Dissolution of Eternity"},

	//various quake-based mods.
	{"-nexuiz",		"nexuiz",	"Nexuiz",				{"nexuiz.exe"},			NEXCFG,	{"data",						"*ftedata"},	"Nexuiz"},
	{"-xonotic",	"xonotic",	"Xonotic",				{"xonotic.exe"},		NEXCFG,	{"data",						"*ftedata"},	"Xonotic"},
//	{"-spark",		"spark",	"Spark",				{"base/src/progs.src",
//														 "base/qwprogs.dat",
//														 "base/pak0.pak"},		DMFCFG,	{"base",						         },	"Spark"},
//	{"-scouts",		"scouts",	"FTE-SJ",				{"basesj/src/progs.src",
//														 "basesj/progs.dat",
//														 "basesj/pak0.pak"},	NULL,	{"basesj",						         },	"Scouts Journey"},
//	{"-rmq",		"rmq",		"RMQ",					{NULL},					RMQCFG,	{"id1",		"qw",	"rmq",		"*fte"},		"Remake Quake"},

#ifdef HEXEN2
	//hexen2's mission pack generally takes precedence if both are installed.
	{"-portals",	"h2mp",		"FTE-H2MP",				{"portals/hexen.rc",
														 "portals/pak3.pak"},	HEX2CFG,{"data1",	"portals",			"*fteh2"},	"Hexen II MP"},
	{"-hexen2",		"hexen2",	"FTE-Hexen2",			{"data1/pak0.pak"},		HEX2CFG,{"data1",						"*fteh2"},	"Hexen II"},
#endif
#if defined(Q2CLIENT) || defined(Q2SERVER)
	{"-quake2",		"q2",		"FTE-Quake2",			{"baseq2/pak0.pak"},	Q2CFG,	{"baseq2",						"*fteq2"},	"Quake II"},
	//mods of the above that should generally work.
	{"-dday",		"dday",		"FTE-Quake2",			{"dday/pak0.pak"},		Q2CFG,	{"baseq2",	"dday",				"*fteq2"},	"D-Day: Normandy"},
#endif

#if defined(Q3CLIENT) || defined(Q3SERVER)
	{"-quake3",		"q3",		"FTE-Quake3",			{"baseq3/pak0.pk3"},	Q3CFG,	{"baseq3",						"*fteq3"},	"Quake III Arena"},
	//the rest are not supported in any real way. maps-only mostly, if that
//	{"-quake4",		"q4",		"FTE-Quake4",			{"q4base/pak00.pk4"},	NULL,	{"q4base",						"*fteq4"},	"Quake 4"},
//	{"-et",			NULL,		"FTE-EnemyTerritory",	{"etmain/pak0.pk3"},	NULL,	{"etmain",						"*fteet"},	"Wolfenstein - Enemy Territory"},

//	{"-jk2",		"jk2",		"FTE-JK2",				{"base/assets0.pk3"},	NULL,	{"base",						"*ftejk2"},	"Jedi Knight II: Jedi Outcast"},
//	{"-warsow",		"warsow",	"FTE-Warsow",			{"basewsw/pak0.pk3"},	NULL,	{"basewsw",						"*ftewsw"},	"Warsow"},
#endif
#if !defined(QUAKETC) && !defined(MINIMAL)
//	{"-doom",		"doom",		"FTE-Doom",				{"doom.wad"},			NULL,	{"*",							"*ftedoom"},	"Doom"},
//	{"-doom2",		"doom2",	"FTE-Doom2",			{"doom2.wad"},			NULL,	{"*",							"*ftedoom"},	"Doom2"},
//	{"-doom3",		"doom3",	"FTE-Doom3",			{"doom3.wad"},			NULL,	{"based3",						"*ftedoom3"},"Doom3"},

	//for the luls
//	{"-diablo2",	NULL,		"FTE-Diablo2",			{"d2music.mpq"},		NULL,	{"*",							"*fted2"},	"Diablo 2"},
#endif
#if defined(HLSERVER) || defined(HLCLIENT)
	//can run in windows, needs hl gamecode enabled. maps can always be viewed, but meh.
	{"-halflife",	"halflife",	"FTE-HalfLife",			{"valve/liblist.gam"},	NULL,	{"valve",						"*ftehl"},	"Half-Life"},
#endif

	{NULL}
};

qboolean FS_GenCachedPakName(char *pname, char *crc, char *local, int llen)
{
	char *fn;
	char hex[16];
	if (strstr(pname, "dlcache"))
	{
		*local = 0;
		return false;
	}

	fn = COM_SkipPath(pname);
	if (fn == pname)
	{	//only allow it if it has some game path first.
		*local = 0;
		return false;
	}
	Q_strncpyz(local, pname, min((fn - pname) + 1, llen));
	Q_strncatz(local, "dlcache/", llen);
	Q_strncatz(local, fn, llen);
	if (*crc)
	{
		Q_strncatz(local, ".", llen);
		snprintf(hex, sizeof(hex), "%x", (unsigned int)strtoul(crc, NULL, 0));
		Q_strncatz(local, hex, llen);
	}
	return true;
}

#if 0
qboolean FS_LoadPackageFromFile(vfsfile_t *vfs, char *pname, char *localname, int *crc, unsigned int flags)
{
	int i;
	char *ext = COM_FileExtension(pname);
	searchpathfuncs_t *handle;
	searchpath_t *oldlist = NULL;

	searchpath_t *sp;

	for (i = 0; i < sizeof(searchpathformats)/sizeof(searchpathformats[0]); i++)
	{
		if (!searchpathformats[i].extension || !searchpathformats[i].OpenNew)
			continue;
		if (!strcmp(ext, searchpathformats[i].extension))
		{
			handle = searchpathformats[i].OpenNew (vfs, localname);
			if (!handle)
			{
				Con_Printf("file %s isn't a %s after all\n", pname, searchpathformats[i].extension);
				break;
			}
			if (crc)
			{
				int truecrc = handle->GeneratePureCRC(handle, 0, false);
				if (truecrc != *crc)
				{
					*crc = truecrc;
					handle->ClosePath(handle);
					return false;
				}
			}
			sp = FS_AddPathHandle(&oldlist, pname, localname, handle, flags, (unsigned int)-1);

			if (sp)
			{
				FS_FlushFSHashReally();
				return true;
			}
		}
	}

	VFS_CLOSE(vfs);
	return false;
}
#endif

//'small' wrapper to open foo.zip/bar to read files within zips that are not part of the gamedir.
//name needs to be null terminated. recursive. pass null for search.
vfsfile_t *CL_OpenFileInPackage(searchpathfuncs_t *search, char *name)
{
	int found;
	vfsfile_t *f;
	flocation_t loc;
	char e, *n;
	char ext[8];
	char *end;
	int i;

	end = name + strlen(name);

	while (end > name)
	{
		e = *end;
		*end = 0;

		if (!e)
		{
			//always open the last file properly.
			loc.search = NULL;
			if (search)
				found = search->FindFile(search, &loc, name, NULL);
			else
				found = FS_FLocateFile(name, FSLFRT_IFFOUND, &loc); 
			if (found)
			{
				f = (search?search:loc.search->handle)->OpenVFS(search?search:loc.search->handle, &loc, "rb");
				if (f)
					return f;
			}
		}
		else
		{
			COM_FileExtension(name, ext, sizeof(ext));
			for (i = 0; i < sizeof(searchpathformats)/sizeof(searchpathformats[0]); i++)
			{
				if (!searchpathformats[i].extension || !searchpathformats[i].OpenNew)
					continue;
				if (!strcmp(ext, searchpathformats[i].extension))
				{
					loc.search = NULL;
					if (search)
						found = search->FindFile(search, &loc, name, NULL);
					else
						found = FS_FLocateFile(name, FSLFRT_IFFOUND, &loc); 
					if (found)
					{
						f = (search?search:loc.search->handle)->OpenVFS(search?search:loc.search->handle, &loc, "rb");
						if (f)
						{
							searchpathfuncs_t *newsearch = searchpathformats[i].OpenNew(f, name);
							if (newsearch)
							{
								f = CL_OpenFileInPackage(newsearch, end+1);
								newsearch->ClosePath(newsearch);
								if (f)
								{
									*end = e;
									return f;
								}
							}
							else
								VFS_CLOSE(f);
						}
						break;
					}
				}
			}
		}

		n = COM_SkipPath(name);
		*end = e;
		end = n-1;
	}
	return NULL;
}

void FS_PureMode(int puremode, char *purenamelist, char *purecrclist, char *refnamelist, char *refcrclist, int pureseed)
{
	qboolean pureflush;

	if (puremode == fs_puremode && fs_pureseed == pureseed)
	{
		if ((!purenamelist && !fs_purenames) || !strcmp(fs_purenames?fs_purenames:"", purenamelist?purenamelist:""))
			if ((!purecrclist && !fs_purecrcs) || !strcmp(fs_purecrcs?fs_purecrcs:"", purecrclist?purecrclist:""))
				if ((!refnamelist && !fs_refnames) || !strcmp(fs_refnames?fs_refnames:"", refnamelist?refnamelist:""))
					if ((!refcrclist && !fs_refcrcs) || !strcmp(fs_refcrcs?fs_refcrcs:"", refcrclist?refcrclist:""))
						return;
	}

	Z_Free(fs_purenames);
	Z_Free(fs_purecrcs);
	Z_Free(fs_refnames);
	Z_Free(fs_refcrcs);

	pureflush = (fs_puremode != 2 && puremode == 2);
	fs_puremode = puremode;
	fs_purenames = purenamelist?Z_StrDup(purenamelist):NULL;
	fs_purecrcs = purecrclist?Z_StrDup(purecrclist):NULL;
	fs_pureseed = pureseed;
	fs_refnames = refnamelist?Z_StrDup(refnamelist):NULL;
	fs_refcrcs = refcrclist?Z_StrDup(refcrclist):NULL;

	FS_ChangeGame(fs_manifest, false);

	if (pureflush)
	{
#ifndef SERVERONLY
		Shader_NeedReload(true);
#endif
		Mod_ClearAll();
		Cache_Flush();
	}
}

char *FSQ3_GenerateClientPacksList(char *buffer, int maxlen, int basechecksum)
{	//this is for q3 compatibility.

	flocation_t loc;
	int numpaks = 0;
	searchpath_t *sp;

	FS_FLocateFile("vm/cgame.qvm", FSLFRT_LENGTH, &loc);
	Q_strncatz(buffer, va("%i ", loc.search->crc_reply), maxlen);
	basechecksum ^= loc.search->crc_reply;

	FS_FLocateFile("vm/ui.qvm", FSLFRT_LENGTH, &loc);
	Q_strncatz(buffer, va("%i ", loc.search->crc_reply), maxlen);
	basechecksum ^= loc.search->crc_reply;

	Q_strncatz(buffer, "@ ", maxlen);

	for (sp = com_purepaths; sp; sp = sp->nextpure)
	{
		if (sp->crc_reply)
		{
			Q_strncatz(buffer, va("%i ", sp->crc_reply), maxlen);
			basechecksum ^= sp->crc_reply;
			numpaks++;
		}
	}

	basechecksum ^= numpaks;
	Q_strncatz(buffer, va("%i ", basechecksum), maxlen);

	return buffer;
}

/*
================
FS_ReloadPackFiles
================

Called when the client has downloaded a new pak/pk3 file
*/
void FS_ReloadPackFilesFlags(unsigned int reloadflags)
{
	searchpath_t	*oldpaths;
	searchpath_t	*next;
	int i;
	int orderkey;
	char syspath[MAX_OSPATH];

	COM_AssertMainThread("FS_ReloadPackFilesFlags");
	COM_WorkerFullSync();

	orderkey = 0;
	if (com_purepaths)
		for (next = com_purepaths; next; next = next->nextpure)
			next->orderkey = ++orderkey;
	if (fs_puremode < 2)
		for (next = com_purepaths; next; next = next->nextpure)
			next->orderkey = ++orderkey;

	oldpaths = com_searchpaths;
	com_searchpaths = NULL;
	com_purepaths = NULL;
	com_base_searchpaths = NULL;

	i = COM_CheckParm ("-basepack");
	while (i && i < com_argc-1)
	{
		const char *pakname = com_argv[i+1];
		searchpathfuncs_t *pak;
		vfsfile_t *vfs = VFSOS_Open(pakname, "rb");
		pak = FS_OpenPackByExtension(vfs, pakname);
		if (pak)
			FS_AddPathHandle(&oldpaths, "", pakname, pak, SPF_COPYPROTECTED|SPF_EXPLICIT, reloadflags);
		i = COM_CheckNextParm ("-basepack", i);
	}

	for (i = 0; i < sizeof(fs_manifest->gamepath) / sizeof(fs_manifest->gamepath[0]); i++)
	{
		char *dir = fs_manifest->gamepath[i].path;
		if (dir && fs_manifest->gamepath[i].base)
		{
			//don't allow leading dots, hidden files are evil.
			//don't allow complex paths. those are evil too.
			if (!*dir || *dir == '.' || !strcmp(dir, ".") || strstr(dir, "..") || strstr(dir, "/")
				|| strstr(dir, "\\") || strstr(dir, ":") )
			{
				Con_Printf ("Gamedir should be a single filename, not a path\n");
				continue;
			}

			//paths equal to '*' actually result in loading packages without an actual gamedir. note that this does not imply that we can write anything.
			if (!strcmp(dir, "*"))
			{
				int j;
				searchpathfuncs_t *handle = VFSOS_OpenPath(NULL, com_gamepath);
				searchpath_t *search = (searchpath_t*)Z_Malloc (sizeof(searchpath_t));
				search->flags = 0;
				search->handle = handle;
				Q_strncpyz(search->purepath, "", sizeof(search->purepath));
				Q_strncpyz(search->logicalpath, com_gamepath, sizeof(search->logicalpath));

				for (j = 0; j < sizeof(searchpathformats)/sizeof(searchpathformats[0]); j++)
				{
					if (!searchpathformats[j].extension || !searchpathformats[j].OpenNew || !searchpathformats[j].loadscan)
						continue;
					if (reloadflags & (1<<j))
					{
						FS_AddDataFiles(&oldpaths, search->purepath, search->logicalpath, search, searchpathformats[j].extension, searchpathformats[j].OpenNew);
					}
				}
				handle->ClosePath(handle);
				Z_Free(search);
			}
			else if (*dir == '*')
			{
				//paths with a leading * are private, and not announced to clients that ask what the current gamedir is.
				Q_snprintfz(syspath, sizeof(syspath), "%s%s", com_gamepath, dir+1);
				FS_AddGameDirectory(&oldpaths, dir+1, syspath, reloadflags, SPF_EXPLICIT|SPF_PRIVATE);
				if (com_homepathenabled)
				{
					Q_snprintfz(syspath, sizeof(syspath), "%s%s", com_homepath, dir+1);
					FS_AddGameDirectory(&oldpaths, dir+1, syspath, reloadflags, SPF_EXPLICIT|SPF_PRIVATE);
				}
			}
			else
			{
				Q_snprintfz(syspath, sizeof(syspath), "%s%s", com_gamepath, dir);
				FS_AddGameDirectory(&oldpaths, dir, syspath, reloadflags, SPF_EXPLICIT);
				if (com_homepathenabled)
				{
					Q_snprintfz(syspath, sizeof(syspath), "%s%s", com_homepath, dir);
					FS_AddGameDirectory(&oldpaths, dir, syspath, reloadflags, SPF_EXPLICIT);
				}
			}
		}
	}
	com_base_searchpaths = com_searchpaths;
	for (i = 0; i < sizeof(fs_manifest->gamepath) / sizeof(fs_manifest->gamepath[0]); i++)
	{
		char *dir = fs_manifest->gamepath[i].path;
		if (dir && !fs_manifest->gamepath[i].base)
		{
			//don't allow leading dots, hidden files are evil.
			//don't allow complex paths. those are evil too.
			if (!*dir || *dir == '.' || !strcmp(dir, ".") || strstr(dir, "..") || strstr(dir, "/")
				|| strstr(dir, "\\") || strstr(dir, ":") )
			{
				Con_Printf ("Gamedir should be a single filename, not a path\n");
				continue;
			}

			if (*dir == '*')
			{
			}
			else
			{
				FS_AddGameDirectory(&oldpaths, dir, va("%s%s", com_gamepath, dir), reloadflags, SPF_EXPLICIT);
				if (com_homepathenabled)
					FS_AddGameDirectory(&oldpaths, dir, va("%s%s", com_homepath, dir), reloadflags, SPF_EXPLICIT);
			}
		}
	}

	/*sv_pure: Reload pure paths*/
	if (fs_purenames && fs_purecrcs)
	{
		char crctok[64];
		char nametok[MAX_QPATH];
		char nametok2[MAX_QPATH];
		searchpath_t *sp, *lastpure = NULL;
		char *names = fs_purenames, *pname;
		char *crcs = fs_purecrcs;
		int crc;

		for (sp = com_searchpaths; sp; sp = sp->next)
		{
			if (sp->handle->GeneratePureCRC)
			{
				sp->nextpure = (void*)0x1;
				sp->crc_check = sp->handle->GeneratePureCRC(sp->handle, fs_pureseed, 0);
				sp->crc_reply = sp->handle->GeneratePureCRC(sp->handle, fs_pureseed, 1);
			}
			else
			{
				sp->nextpure = NULL;
				sp->crc_check = 0;
				sp->crc_reply = 0;
			}
		}

		while(names && crcs)
		{
			crcs = COM_ParseOut(crcs, crctok, sizeof(crctok));
			names = COM_ParseOut(names, nametok, sizeof(nametok));

			crc = strtoul(crctok, NULL, 0);
			if (!crc)
				continue;

			pname = nametok;

			if (fs_refnames && fs_refcrcs)
			{	//q3 is annoying as hell
				int crc2;
				char *rc = fs_refcrcs;
				char *rn = fs_refnames;
				pname = "";
				for (; rc && rn; )
				{
					rc = COM_ParseOut(rc, crctok, sizeof(crctok));
					rn = COM_ParseOut(rn, nametok2, sizeof(nametok2));
					crc2 = strtoul(crctok, NULL, 0);
					if (crc2 == crc)
					{
						COM_DefaultExtension(nametok2, ".pk3", sizeof(nametok2));
						pname = nametok2;
						break;
					}
				}
			}
			if (*pname == '*')	// * means that its 'referenced' (read: actually useful) thus should be downloaded, which is not relevent here.
				pname++;

			for (sp = com_searchpaths; sp; sp = sp->next)
			{
				if (sp->nextpure == (void*)0x1)	//don't add twice.
					if (sp->crc_check == crc)
					{
						if (fs_puremode)
						{
							if (lastpure)
								lastpure->nextpure = sp;
							else
								com_purepaths = sp;
							sp->nextpure = NULL;
							lastpure = sp;
						}
						break;
					}
			}
			if (!fs_puremode && !sp)
			{	//if we're not pure, we don't care if the version differs. don't load the server's version.
				//this works around 1.01 vs 1.06 issues.
				for (sp = com_searchpaths; sp; sp = sp->next)
				{
					if (!Q_strcasecmp(pname, sp->purepath))
						break;
				}
			}
			//if its not already loaded (via wildcards), load it from the download cache, if we can
			if (!sp && *pname)
			{
				char local[MAX_OSPATH];
				vfsfile_t *vfs;
				char ext[8];
				void *handle;
				int i;
				COM_FileExtension(pname, ext, sizeof(ext));

				if (FS_GenCachedPakName(pname, va("%i", crc), local, sizeof(local)))
				{
					unsigned int keptflags;
					handle = FS_GetOldPath(&oldpaths, local, &keptflags);
					if (handle)
					{
						sp = FS_AddPathHandle(&oldpaths, pname, local, handle, SPF_COPYPROTECTED|SPF_TEMPORARY|keptflags, (unsigned int)-1);
						if (sp->crc_check == crc)
						{
							if (fs_puremode)
							{
								if (lastpure)
									lastpure->nextpure = sp;
								else
									com_purepaths = sp;
								sp->nextpure = NULL;
								lastpure = sp;
							}
						}
						continue;
					}
					vfs = FS_OpenVFS(local, "rb", FS_ROOT);
				}
				else
					vfs = NULL;
				if (vfs)
				{
					for (i = 0; i < sizeof(searchpathformats)/sizeof(searchpathformats[0]); i++)
					{
						if (!searchpathformats[i].extension || !searchpathformats[i].OpenNew)
							continue;
						if (!strcmp(ext, searchpathformats[i].extension))
						{
							handle = searchpathformats[i].OpenNew (vfs, local);
							if (!handle)
								break;
							sp = FS_AddPathHandle(&oldpaths, pname, local, handle, SPF_COPYPROTECTED|SPF_TEMPORARY, (unsigned int)-1);

							sp->crc_check = sp->handle->GeneratePureCRC(sp->handle, fs_pureseed, 0);
							sp->crc_reply = sp->handle->GeneratePureCRC(sp->handle, fs_pureseed, 1);

							if (sp->crc_check == crc)
							{
								if (fs_puremode)
								{
									if (lastpure)
										lastpure->nextpure = sp;
									else
										com_purepaths = sp;
									sp->nextpure = NULL;
									lastpure = sp;
								}
							}
							break;
						}
					}
				}

				if (!sp)
					Con_DPrintf("Pure crc %i wasn't found\n", crc);
			}
		}
	}

	while(oldpaths)
	{
		next = oldpaths->next;

		Con_Printf("%s is no longer needed\n", oldpaths->logicalpath);
		oldpaths->handle->ClosePath(oldpaths->handle);
		Z_Free(oldpaths);
		oldpaths = next;
	}


	i = orderkey;
	orderkey = 0;
	next = NULL;
	if (com_purepaths)
		for (next = com_purepaths; next; next = next->nextpure)
			if (next->orderkey != ++orderkey)
				break;
	if (!next && fs_puremode < 2)
		for (next = com_purepaths; next; next = next->nextpure)
			if (next->orderkey != ++orderkey)
				break;

	if (next || i != orderkey)//some path changed. make sure the fs cache is flushed.
		FS_FlushFSHashReally(false);

#ifndef SERVERONLY
	Shader_NeedReload(true);
#endif
//	Mod_ClearAll();
//	Cache_Flush();
}

void FS_UnloadPackFiles(void)
{
	if (Sys_LockMutex(fs_thread_mutex))
	{
		FS_ReloadPackFilesFlags(1);
		Sys_UnlockMutex(fs_thread_mutex);
	}
}

void FS_ReloadPackFiles(void)
{
	if (Sys_LockMutex(fs_thread_mutex))
	{
		FS_ReloadPackFilesFlags(~0);
		Sys_UnlockMutex(fs_thread_mutex);
	}
}

void FS_ReloadPackFiles_f(void)
{
	if (Sys_LockMutex(fs_thread_mutex))
	{
		if (atoi(Cmd_Argv(1)))
			FS_ReloadPackFilesFlags(atoi(Cmd_Argv(1)));
		else
			FS_ReloadPackFilesFlags(~0);
		Sys_UnlockMutex(fs_thread_mutex);
	}
}

#if defined(_WIN32) && !defined(WINRT)
#include "winquake.h"
#ifdef MINGW
#define byte BYTE	//some versions of mingw headers are broken slightly. this lets it compile.
#endif
#include <shlobj.h>
static qboolean Sys_SteamHasFile(char *basepath, int basepathlen, char *steamdir, char *fname)
{
	/*
	Find where Valve's Steam distribution platform is installed.
	Then take a look at that location for the relevent installed app.
	*/
	FILE *f;
	DWORD resultlen;
	HKEY key = NULL;
	
	if (RegOpenKeyEx(HKEY_CURRENT_USER, "SOFTWARE\\Valve\\Steam", 0, STANDARD_RIGHTS_READ|KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
	{
		resultlen = basepathlen;
		RegQueryValueEx(key, "SteamPath", NULL, NULL, basepath, &resultlen);
		RegCloseKey(key);
		Q_strncatz(basepath, va("/SteamApps/common/%s", steamdir), basepathlen);
		if ((f = fopen(va("%s/%s", basepath, fname), "rb")))
		{
			fclose(f);
			return true;
		}
	}
	return false;
}
qboolean Sys_FindGameData(const char *poshname, const char *gamename, char *basepath, int basepathlen)
{
	DWORD resultlen;
	HKEY key = NULL;

#ifndef INVALID_FILE_ATTRIBUTES
	#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif

	//first, try and find it in our game paths location
	if (RegOpenKeyEx(HKEY_CURRENT_USER, "SOFTWARE\\" FULLENGINENAME "\\GamePaths", 0, STANDARD_RIGHTS_READ|KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
	{
		resultlen = basepathlen;
		if (!RegQueryValueEx(key, gamename, NULL, NULL, basepath, &resultlen))
		{
			if (GetFileAttributes(basepath) != INVALID_FILE_ATTRIBUTES)
			{
				RegCloseKey(key);
				return true;
			}
		}

		RegCloseKey(key);
	}


	if (!strcmp(gamename, "quake"))
	{
		char *prefix[] =
		{
			"c:/quake/",						//quite a lot of people have it in c:\quake, as that's the default install location from the quake cd.
			"c:/games/quake/",					//personally I use this
#ifdef _WIN64
			//quite a few people have nquake installed. we need to an api function to read the directory for non-english-windows users.
			va("%s/nQuake/", getenv("%ProgramFiles(x86)%")),	//64bit builds should look in both places
			va("%s/nQuake/", getenv("%ProgramFiles%")),			//
#else
			va("%s/nQuake/", getenv("%ProgramFiles%")),			//32bit builds will get the x86 version anyway.
#endif
			NULL
		};
		int i;
		FILE *f;

		//try and find it via steam
		//reads HKEY_LOCAL_MACHINE\SOFTWARE\Valve\Steam\InstallPath
		//append SteamApps\common\quake
		//use it if we find winquake.exe there
		if (Sys_SteamHasFile(basepath, basepathlen, "quake", "Winquake.exe"))
			return true;
		//well, okay, so they don't have quake installed from steam.

		//check various 'unadvertised' paths
		for (i = 0; prefix[i]; i++)
		{
			char syspath[MAX_OSPATH];
			Q_snprintfz(syspath, sizeof(syspath), "%sid1/pak0.pak", prefix[i]);
			if ((f = fopen("c:/quake/quake.exe", "rb")))
			{
				fclose(f);
				Q_strncpyz(basepath, prefix[i], sizeof(basepath));
				return true;
			}
		}
	}

	if (!strcmp(gamename, "quake2"))
	{
		FILE *f;
		DWORD resultlen;
		HKEY key = NULL;

		//look for HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\Quake2_exe\Path
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\Quake2_exe", 0, STANDARD_RIGHTS_READ|KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
		{
			resultlen = basepathlen;
			RegQueryValueEx(key, "Path", NULL, NULL, basepath, &resultlen);
			RegCloseKey(key);
			if ((f = fopen(va("%s/quake2.exe", basepath), "rb")))
			{
				fclose(f);
				return true;
			}
		}

		if (Sys_SteamHasFile(basepath, basepathlen, "quake 2", "quake2.exe"))
			return true;
	}

	if (!strcmp(gamename, "et"))
	{
		FILE *f;
		DWORD resultlen;
		HKEY key = NULL;
		//reads HKEY_LOCAL_MACHINE\SOFTWARE\Activision\Wolfenstein - Enemy Territory
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Activision\\Wolfenstein - Enemy Territory", 0, STANDARD_RIGHTS_READ|KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
		{
			resultlen = basepathlen;
			RegQueryValueEx(key, "InstallPath", NULL, NULL, basepath, &resultlen);
			RegCloseKey(key);

			if ((f = fopen(va("%s/ET.exe", basepath), "rb")))
			{
				fclose(f);
				return true;
			}
			return true;
		}
	}

	if (!strcmp(gamename, "quake3"))
	{
		FILE *f;
		DWORD resultlen;
		HKEY key = NULL;

		//reads HKEY_LOCAL_MACHINE\SOFTWARE\id\Quake III Arena\InstallPath
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\id\\Quake III Arena", 0, STANDARD_RIGHTS_READ|KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
		{
			resultlen = basepathlen;
			RegQueryValueEx(key, "InstallPath", NULL, NULL, basepath, &resultlen);
			RegCloseKey(key);

			if ((f = fopen(va("%s/quake3.exe", basepath), "rb")))
			{
				fclose(f);
				return true;
			}
		}

		if (Sys_SteamHasFile(basepath, basepathlen, "quake 3 arena", "quake3.exe"))
			return true;
	}

	if (!strcmp(gamename, "wop"))
	{
		DWORD resultlen;
		HKEY key = NULL;
		//reads HKEY_LOCAL_MACHINE\SOFTWARE\World Of Padman\Path
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\World Of Padman", 0, STANDARD_RIGHTS_READ|KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
		{
			resultlen = basepathlen;
			RegQueryValueEx(key, "Path", NULL, NULL, basepath, &resultlen);
			RegCloseKey(key);
			return true;
		}
	}

/*
	if (!strcmp(gamename, "d3"))
	{
		DWORD resultlen;
		HKEY key = NULL;
		//reads HKEY_LOCAL_MACHINE\SOFTWARE\id\Doom 3\InstallPath
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\id\\Doom 3", 0, STANDARD_RIGHTS_READ|KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
		{
			resultlen = basepathlen;
			RegQueryValueEx(key, "InstallPath", NULL, NULL, basepath, &resultlen);
			RegCloseKey(key);
			return true;
		}
	}
*/

	if (!strcmp(gamename, "hexen2") || !strcmp(gamename, "h2mp"))
	{
		//append SteamApps\common\hexen 2
		if (Sys_SteamHasFile(basepath, basepathlen, "hexen 2", "glh2.exe"))
			return true;
	}

#if !defined(NPFTE) && !defined(SERVERONLY) //this is *really* unfortunate, but doing this crashes the browser
	if (poshname && *gamename && !COM_CheckParm("-manifest"))
	{
		char resultpath[MAX_PATH];
		BROWSEINFO bi;
		LPITEMIDLIST il;
		memset(&bi, 0, sizeof(bi));
		bi.hwndOwner = mainwindow; //note that this is usually still null
		bi.pidlRoot = NULL;
		bi.pszDisplayName = resultpath;
		bi.lpszTitle = va("Please locate your existing %s installation", poshname);
		bi.ulFlags = BIF_RETURNONLYFSDIRS;
		bi.lpfn = NULL;
		bi.lParam = 0;
		bi.iImage = 0;

		il = SHBrowseForFolder(&bi);
		if (il)
		{
			SHGetPathFromIDList(il, resultpath);
			CoTaskMemFree(il);
			Q_strncpyz(basepath, resultpath, basepathlen-1);

			//and save it into the windows registry
			if (RegCreateKeyEx(HKEY_CURRENT_USER, "SOFTWARE\\" FULLENGINENAME "\\GamePaths",
				0, NULL,
				REG_OPTION_NON_VOLATILE,
				KEY_WRITE,
				NULL,
				&key,
				NULL) == ERROR_SUCCESS)
			{
				RegSetValueEx(key, gamename, 0, REG_SZ, basepath, strlen(basepath));

				RegCloseKey(key);
			}

			return true;
		}
	}
#endif

	return false;
}
#else
#ifdef __linux__
#include <sys/stat.h>
#endif
qboolean Sys_FindGameData(const char *poshname, const char *gamename, char *basepath, int basepathlen)
{
#ifdef __linux__
	struct stat sb;
        if (!strcmp(gamename, "quake"))
	{
		if (stat("/usr/share/quake/", &sb) == 0)
		{
			// /usr/share/quake
			if (S_ISDIR(sb.st_mode))
			{
				Q_strncpyz(basepath, "/usr/share/quake/", basepathlen);
				return true;
			}
		}
	}
#endif
	return false;
}
#endif

static void FS_FreePaths(void)
{
	searchpath_t *next;
	FS_FlushFSHashReally(true);

	//
	// free up any current game dir info
	//
	while (com_searchpaths)
	{
		com_searchpaths->handle->ClosePath(com_searchpaths->handle);
		next = com_searchpaths->next;
		Z_Free (com_searchpaths);
		com_searchpaths = next;
	}

	com_fschanged = true;


	if (filesystemhash.numbuckets)
	{
		BZ_Free(filesystemhash.bucket);
		filesystemhash.bucket = NULL;
		filesystemhash.numbuckets = 0;
	}

	FS_Manifest_Free(fs_manifest);
	fs_manifest = NULL;
}
void FS_Shutdown(void)
{
	FS_FreePaths();
	Sys_DestroyMutex(fs_thread_mutex);
	fs_thread_mutex = NULL;

	Cvar_SetEngineDefault(&fs_gamename, NULL);
	Cvar_SetEngineDefault(&com_protocolname, NULL);
}

//returns false if the directory is not suitable.
//returns true if it contains a known package. if we don't actually know of any packages that it should have, we just have to assume that its okay.
static qboolean FS_DirHasAPackage(char *basedir, ftemanifest_t *man)
{
	qboolean defaultret = true;
	int j;
	vfsfile_t *f;
	for (j = 0; j < sizeof(fs_manifest->package) / sizeof(fs_manifest->package[0]); j++)
	{
		if (!man->package[j].path)
			continue;
		defaultret = false;

		f = VFSOS_Open(va("%s%s", basedir, man->package[j].path), "rb");
		if (f)
		{
			VFS_CLOSE(f);
			return true;
		}
	}
	return defaultret;
}

//just check each possible file, see if one is there.
static qboolean FS_DirHasGame(char *basedir, int gameidx)
{
	int j;
	vfsfile_t *f;
	for (j = 0; j < 4; j++)
	{
		if (!gamemode_info[gameidx].auniquefile[j])
			continue;	//no more
		f = VFSOS_Open(va("%s%s", basedir, gamemode_info[gameidx].auniquefile[j]), "rb");
		if (f)
		{
			VFS_CLOSE(f);
			return true;
		}
	}
	return false;
}

//check em all
static int FS_IdentifyDefaultGameFromDir(char *basedir)
{
	int i;
	for (i = 0; gamemode_info[i].argname; i++)
	{
		if (FS_DirHasGame(basedir, i))
			return i;
	}
	return -1;
}

//attempt to work out which game we're meant to be trying to run based upon a few things
//1: fs_changegame console command override. fixme: needs to cope with manifests too.
//2: -quake3 argument implies that the user wants to run quake3.
//3: if we are ftequake3.exe then we always try to run quake3.
//4: identify characteristic files within the working directory (like id1/pak0.pak implies we're running quake)
//5: check where the exe actually is instead of simply where we're being run from.
//6: fallback to prompting. just returns -1 here.
//if autobasedir is not set, block gamedir changes/prompts.
static int FS_IdentifyDefaultGame(char *newbase, int sizeof_newbase, qboolean fixedbase)
{
	int i;
	int gamenum = -1;

	if (gamenum == -1)
	{
		for (i = 0; gamemode_info[i].argname; i++)
		{
			if (COM_CheckParm(gamemode_info[i].argname))
			{
				gamenum = i;
				break;
			}
		}
	}
	//use the game based on an exe name over the filesystem one (could easily have multiple fs path matches).
	if (gamenum == -1)
	{
		char *ev, *v0 = COM_SkipPath(com_argv[0]);
		for (i = 0; gamemode_info[i].argname; i++)
		{
			if (!gamemode_info[i].exename)
				continue;
			ev = strstr(v0, gamemode_info[i].exename);
			if (ev && (!strchr(ev, '\\') && !strchr(ev, '/')))
				gamenum = i;
		}
	}

	//identify the game from a telling file in the working directory
	if (gamenum == -1)
		gamenum = FS_IdentifyDefaultGameFromDir(newbase);
	//identify the game from a telling file relative to the exe's directory. for when shortcuts don't set the working dir sensibly.
	if (gamenum == -1 && host_parms.binarydir && *host_parms.binarydir && !fixedbase)
	{
		gamenum = FS_IdentifyDefaultGameFromDir(host_parms.binarydir);
		if (gamenum != -1)
			Q_strncpyz(newbase, host_parms.binarydir, sizeof_newbase);
	}
	return gamenum;
}

//allowed to modify newbasedir if fixedbasedir isn't set
static ftemanifest_t *FS_GenerateLegacyManifest(char *newbasedir, int sizeof_newbasedir, qboolean fixedbasedir, int game)
{
	int i;
	ftemanifest_t *man;

	if (gamemode_info[game].manifestfile)
		man = FS_Manifest_Parse(NULL, gamemode_info[game].manifestfile);
	else
	{
		man = FS_Manifest_Create(NULL);

		Cmd_TokenizeString(va("game \"%s\"", gamemode_info[game].argname+1), false, false);
		FS_Manifest_ParseTokens(man);
		if (gamemode_info[game].poshname)
		{
			Cmd_TokenizeString(va("name \"%s\"", gamemode_info[game].poshname), false, false);
			FS_Manifest_ParseTokens(man);
		}

		i = COM_CheckParm ("-basegame");
		if (i)
		{
			do
			{
				Cmd_TokenizeString(va("basegame \"%s\"", com_argv[i+1]), false, false);
				FS_Manifest_ParseTokens(man);

				i = COM_CheckNextParm ("-basegame", i);
			}
			while (i && i < com_argc-1);
		}

		i = COM_CheckParm ("-game");
		if (i)
		{
			do
			{
				Cmd_TokenizeString(va("gamedir \"%s\"", com_argv[i+1]), false, false);
				FS_Manifest_ParseTokens(man);

				i = COM_CheckNextParm ("-game", i);
			}
			while (i && i < com_argc-1);
		}

		i = COM_CheckParm ("+gamedir");
		if (i)
		{
			do
			{
				Cmd_TokenizeString(va("gamedir \"%s\"", com_argv[i+1]), false, false);
				FS_Manifest_ParseTokens(man);

				i = COM_CheckNextParm ("+gamedir", i);
			}
			while (i && i < com_argc-1);
		}
	}
	return man;
}

static char *FS_RelativeURL(char *base, char *file, char *buffer, int bufferlen)
{
	//fixme: cope with windows paths
	qboolean baseisurl = base?!!strchr(base, ':'):false;
	qboolean fileisurl = !!strchr(file, ':');
	//qboolean baseisabsolute = (*base == '/' || *base == '\\');
	qboolean fileisabsolute = (*file == '/' || *file == '\\');
	char *ebase;
	
	if (fileisurl || !base)
		return file;
	if (fileisabsolute)
	{
		if (baseisurl)
		{
			ebase = strchr(base, ':');
			ebase++;
			while(*ebase == '/')
				ebase++;
			while(*ebase && *ebase != '/')
				ebase++;
		}
		else
			ebase = base;
	}
	else
		ebase = COM_SkipPath(base);
	memcpy(buffer, base, ebase-base);
	strcpy(buffer+(ebase-base), file);

	return buffer;
}

#ifdef WEBCLIENT
static struct dl_download *curpackagedownload;
static char fspdl_internalname[MAX_QPATH];
static char fspdl_temppath[MAX_OSPATH];
static char fspdl_finalpath[MAX_OSPATH];
static void FS_BeginNextPackageDownload(void);
static void FS_PackageDownloaded(struct dl_download *dl)
{
	curpackagedownload = NULL;
	
	if (dl->file)
	{
		VFS_CLOSE(dl->file);
		dl->file = NULL;
	}
	if (dl->status == DL_FINISHED)
	{
		//rename the file as needed.
		COM_CreatePath(fspdl_finalpath);

		if (*fspdl_internalname)	//if zip...
		{	//archive
			searchpathfuncs_t *archive = FSZIP_LoadArchive(VFSOS_Open(fspdl_temppath, "rb"), dl->url);
			if (archive)
			{
				flocation_t loc;
				int status = FF_NOTFOUND;
//FIXME: loop through all packages to extract all of them as appropriate
				if (status == FF_NOTFOUND)
					status = archive->FindFile(archive, &loc, fspdl_internalname, NULL);
				if (status == FF_NOTFOUND)
					status = archive->FindFile(archive, &loc, COM_SkipPath(fspdl_internalname), NULL);

				if (status == FF_FOUND)
				{
					vfsfile_t *in = archive->OpenVFS(archive, &loc, "rb");
					if (in)
					{
						vfsfile_t *out = VFSOS_Open(fspdl_finalpath, "wb");
						qofs_t remaining = VFS_GETLEN(in);
						size_t count;
						if (out)
						{
							char buf[65536];
							while (remaining)
							{
								if (remaining < sizeof(buf))
									count = remaining;
								else
									count = sizeof(buf);
								VFS_READ(in, buf, count);
								VFS_WRITE(out, buf, count);
								remaining -= count;
							}
							VFS_CLOSE(out);
						}
						VFS_CLOSE(in);
					}
				}
				else
				{
					Con_Printf("Unable to find %s in %s\n", fspdl_internalname, fspdl_temppath);
				}
				archive->ClosePath(archive);
			}
		}
		else
		{
			//direct file
			if (!Sys_Rename(fspdl_temppath, fspdl_finalpath))
			{
				Con_Printf("Unable to rename \"%s\" to \"%s\"\n", fspdl_temppath, fspdl_finalpath);
			}
		}
	}
	Sys_remove (fspdl_temppath);

	fs_restarts++;
	FS_ChangeGame(fs_manifest, true);

	FS_BeginNextPackageDownload();
}
static void FS_BeginNextPackageDownload(void)
{
	char *crcstr;
	int j;
	ftemanifest_t *man = fs_manifest;
	vfsfile_t *check;
	if (curpackagedownload || !man)
		return;

	for (j = 0; j < sizeof(fs_manifest->package) / sizeof(fs_manifest->package[0]); j++)
	{
		char buffer[MAX_OSPATH], *url;
		if (!man->package[j].path)
			continue;

		//check if we already have a version of the pak. if the user installed one, don't corrupt it with some unwanted pak. this may cause problems but whatever, user versitility wins.
		//this matches the rules for loading packs too. double is utterly pointless.
		check = FS_OpenVFS(man->package[j].path, "rb", FS_ROOT);
		if (check)
		{
			VFS_CLOSE(check);
			continue;
		}

		//figure out what the cached name should be and see if we already have that or not
		if (man->package[j].crcknown)
			crcstr = va("%#x", man->package[j].crc);
		else
			crcstr = "";
		if (!FS_GenCachedPakName(man->package[j].path, crcstr, buffer, sizeof(buffer)))
			continue;
		check = FS_OpenVFS(buffer, "rb", FS_ROOT);
		if (check)
		{
			VFS_CLOSE(check);
			continue;
		}

		if (man->package[j].extractname)
			Q_strncpyz(fspdl_internalname, man->package[j].extractname, sizeof(fspdl_internalname));
		else
			Q_strncpyz(fspdl_internalname, "", sizeof(fspdl_internalname));

		//figure out a temp name and figure out where we're going to get it from.
		FS_NativePath(buffer, FS_ROOT, fspdl_finalpath, sizeof(fspdl_finalpath));
		if (!FS_GenCachedPakName(va("%s.tmp", man->package[j].path), crcstr, buffer, sizeof(buffer)))
			continue;
		FS_NativePath(buffer, FS_ROOT, fspdl_temppath, sizeof(fspdl_temppath));

		url = NULL;
		while(!url)
		{
			//ran out of mirrors?
			if (man->package[j].mirrornum == (sizeof(man->package[j].mirrors) / sizeof(man->package[j].mirrors[0])))
				break;

			if (man->package[j].mirrors[man->package[j].mirrornum])
				url = FS_RelativeURL(man->updateurl, man->package[j].mirrors[man->package[j].mirrornum], buffer, sizeof(buffer));
			man->package[j].mirrornum++;
		}
		//no valid mirrors
		if (!url)
			continue;

		Con_Printf("Downloading %s from %s\n", fspdl_finalpath, url);
		curpackagedownload = HTTP_CL_Get(url, NULL, FS_PackageDownloaded);
		if (curpackagedownload)
		{
			COM_CreatePath(fspdl_temppath);
			curpackagedownload->file = VFSOS_Open(fspdl_temppath, "wb");
			return;
		}
	}
}
static void FS_ManifestUpdated(struct dl_download *dl)
{
	ftemanifest_t *man = fs_manifest;

	curpackagedownload = NULL;

	if (dl->file)
	{
		if (dl->user_ctx == man)
		{
			size_t len = VFS_GETLEN(dl->file), len2;
			char *fdata = BZ_Malloc(len+1), *fdata2 = NULL;
			if (fdata)
			{
				VFS_READ(dl->file, fdata, len);
				fdata[len] = 0;
				man = FS_Manifest_Parse(fs_manifest->updatefile, fdata);
				if (man)
				{
					//the updateurl MUST match the current one in order for the local version of the manifest to be saved (to avoid extra updates, and so it appears in the menu_mods)
					//this is a paranoia measure to avoid too much damage from buggy/malicious proxies that return empty pages or whatever.
					if (man->updateurl && fs_manifest->updateurl && !strcmp(man->updateurl, fs_manifest->updateurl))
					{
						man->blockupdate = true;	//don't download it multiple times. that's just crazy.
						if (man->updatefile)
						{
							vfsfile_t *f2 = FS_OpenVFS(fs_manifest->updatefile, "rb", FS_SYSTEM);
							if (f2)
							{
								len2 = VFS_GETLEN(f2);
								if (len != len2)
								{
									fdata2 = NULL;
									len2 = 0;
								}
								else
								{
									fdata2 = BZ_Malloc(len2);
									VFS_READ(f2, fdata2, len2);
								}
								VFS_CLOSE(f2);
								if (len == len2 && !memcmp(fdata, fdata2, len))
								{
									//files match, no need to use this new manifest at all.
									FS_Manifest_Free(man);
									man = NULL;
								}
								BZ_Free(fdata2);
							}
							if (man)
								FS_WriteFile(man->updatefile, fdata, len, FS_SYSTEM);
						}
						if (man)
							FS_ChangeGame(man, true);
					}
					else
						FS_Manifest_Free(man);
				}
				BZ_Free(fdata);
			}
		}

		VFS_CLOSE(dl->file);
		dl->file = NULL;
	}

	FS_BeginNextPackageDownload();
}
void FS_BeginManifestUpdates(void)
{
	ftemanifest_t *man = fs_manifest;
	if (curpackagedownload || !man)
		return;

	if (man->updateurl && !man->blockupdate)
	{
		man->blockupdate = true;
		Con_Printf("Updating manifest from %s\n", man->updateurl);
		curpackagedownload = HTTP_CL_Get(man->updateurl, NULL, FS_ManifestUpdated);
		if (curpackagedownload)
			curpackagedownload->user_ctx = man;
	}

	//urr, failed for some reason (like the url not specified?)
	if (!curpackagedownload)
		FS_BeginNextPackageDownload();
}
#else
void FS_BeginManifestUpdates(void)
{
}
#endif

qboolean FS_FoundManifest(void *usr, ftemanifest_t *man)
{
	if (!*(ftemanifest_t**)usr)
	{
		*(ftemanifest_t**)usr = man;
		return true;
	}
	return false;
}

//reads the default manifest based upon the basedir, the commandline arguments, the name of the exe, etc.
//may still fail if no game was identified.
//if fixedbasedir is true, stuff like -quake won't override/change the active basedir (ie: -basedir or gamedir switching without breaking gamedir)
ftemanifest_t *FS_ReadDefaultManifest(char *newbasedir, size_t newbasedirsize, qboolean fixedbasedir)
{
	ftemanifest_t *man = NULL;

	vfsfile_t *f;
#ifdef BRANDING_NAME
	f = VFSOS_Open(va("%s"STRINGIFY(BRANDING_NAME)".fmf", newbasedir), "rb");
	if (!f)
#endif
		f = VFSOS_Open(va("%sdefault.fmf", newbasedir), "rb");
	if (f)
	{
		size_t len = VFS_GETLEN(f);
		char *fdata = BZ_Malloc(len+1);
		if (fdata)
		{
			VFS_READ(f, fdata, len);
			fdata[len] = 0;
			man = FS_Manifest_Parse(NULL, fdata);
			BZ_Free(fdata);
		}
		VFS_CLOSE(f);
	}

	if (!man)
	{
		int game = FS_IdentifyDefaultGame(newbasedir, newbasedirsize, fixedbasedir);
		if (game != -1)
			man = FS_GenerateLegacyManifest(newbasedir, newbasedirsize, fixedbasedir, game);
	}
	return man;
}

//this is potentially unsafe. needs lots of testing.
qboolean FS_ChangeGame(ftemanifest_t *man, qboolean allowreloadconfigs)
{
	int i, j;
	char realpath[MAX_OSPATH-1];
	char newbasedir[MAX_OSPATH];
	qboolean fixedbasedir;
	qboolean reloadconfigs = false;
	qboolean builtingame = false;
	flocation_t loc;

	//if any of these files change location, the configs will be re-execed.
	//note that we reuse path handles if they're still valid, so we can just check the pointer to see if it got unloaded/replaced.
	char *conffile[] = {"quake.rc", "hexen.rc", "default.cfg", "server.cfg", NULL};
	searchpathfuncs_t *confpath[sizeof(conffile)/sizeof(conffile[0])];
	for (i = 0; conffile[i]; i++)
	{
		FS_FLocateFile(conffile[i], FSLFRT_IFFOUND, &loc);	//q1
		confpath[i] = loc.search?loc.search->handle:NULL;
	}

	i = COM_CheckParm ("-basedir");
	fixedbasedir = i && i < com_argc-1;
	Q_strncpyz (newbasedir, fixedbasedir?com_argv[i+1]:host_parms.basedir, sizeof(newbasedir));

	//make sure it has a trailing slash, or is empty. woo.
	FS_CleanDir(newbasedir, sizeof(newbasedir));

	if (!man)
	{
		//if we're already running a game, don't autodetect.
		if (fs_manifest)
			return false;

		man = FS_ReadDefaultManifest(newbasedir, sizeof(newbasedir), fixedbasedir);

		if (!man && isDedicated)
		{	//dedicated servers have no menu code, so just pick the first fmf we could find.
			FS_EnumerateKnownGames(FS_FoundManifest, &man);
		}
		if (!man)
		{
			man = FS_Manifest_Parse(NULL,
				"FTEMANIFEST 1\n"
				"game \"\"\n"
				"name \"" FULLENGINENAME "\"\n"
				"defaultexec \\\"vid_fullscreen 0; gl_font cour;vid_width 640; vid_height 480\"\n"
				);
		}
	}

	if (man == fs_manifest)
	{
		//don't close anything. theoretically nothing is changing, and we don't want to load new defaults either.
	}
	else if (!fs_manifest || !strcmp(fs_manifest->installation?fs_manifest->installation:"", man->installation?man->installation:""))
	{
		if (!fs_manifest)
			reloadconfigs = true;
		FS_Manifest_Free(fs_manifest);
	}
	else
	{
		FS_FreePaths();

		reloadconfigs = true;
	}
	fs_manifest = man;

	if (man->installation && *man->installation)
	{
		for (i = 0; gamemode_info[i].argname; i++)
		{
			if (!strcmp(man->installation, gamemode_info[i].argname+1))
			{
				//if there's no base dirs, edit the manifest to give it its default ones.
				for (j = 0; j < sizeof(man->gamepath) / sizeof(man->gamepath[0]); j++)
				{
					if (man->gamepath[j].path && man->gamepath[j].base)
						break;
				}
				if (j == sizeof(man->gamepath) / sizeof(man->gamepath[0]))
				{
					for (j = 0; j < 4; j++)
						if (gamemode_info[i].dir[j])
						{
							Cmd_TokenizeString(va("basegame \"%s\"", gamemode_info[i].dir[j]), false, false);
							FS_Manifest_ParseTokens(man);
						}
				}

				if (!man->protocolname)
				{
					Cmd_TokenizeString(va("protocolname \"%s\"", gamemode_info[i].protocolname), false, false);
					FS_Manifest_ParseTokens(man);
				}
				if (!man->defaultexec && gamemode_info[i].customexec)
				{
					man->defaultexec = Z_StrDup(gamemode_info[i].customexec);
				}

				builtingame = true;
				if (!fixedbasedir && !FS_DirHasGame(newbasedir, i))
					if (Sys_FindGameData(man->formalname, man->installation, realpath, sizeof(realpath)))
						Q_strncpyz (newbasedir, realpath, sizeof(newbasedir));
				break;
			}
		}
	}

	if (allowreloadconfigs)
	{
		if (!builtingame && !fixedbasedir && !FS_DirHasAPackage(newbasedir, man))
			if (Sys_FindGameData(man->formalname, man->installation, realpath, sizeof(realpath)))
				Q_strncpyz (newbasedir, realpath, sizeof(newbasedir));

		Q_strncpyz (com_gamepath, newbasedir, sizeof(com_gamepath));
	}
	//make sure it has a trailing slash, or is empty. woo.
	FS_CleanDir(com_gamepath, sizeof(com_gamepath));

	{
		qboolean oldhome = com_homepathenabled;
		com_homepathenabled = com_homepathusable;

		if (man->disablehomedir && !COM_CheckParm("-usehome"))
			com_homepathenabled = false;

		if (com_homepathenabled != oldhome)
		{
			if (com_homepathenabled)
				Con_TPrintf("Using home directory \"%s\"\n", com_homepath);
			else
				Con_TPrintf("Disabled home directory suport\n");
		}
	}

#ifdef ANDROID
	{
		vfsfile_t *f;
		//write a .nomedia file to avoid people from getting random explosion sounds etc intersperced with their music
		f = FS_OpenVFS(".nomedia", "rb", FS_ROOT);
		if (f)
			VFS_CLOSE(f);
		else
			FS_WriteFile(".nomedia", NULL, 0, FS_ROOT);
	}
#endif

	if (Sys_LockMutex(fs_thread_mutex))
	{
		FS_ReloadPackFilesFlags(~0);

		FS_BeginManifestUpdates();

		COM_CheckRegistered();

		Sys_UnlockMutex(fs_thread_mutex);

		if (allowreloadconfigs)
		{
			for (i = 0; conffile[i]; i++)
			{
				FS_FLocateFile(conffile[i], FSLFRT_IFFOUND, &loc);
				if (confpath[i] != (loc.search?loc.search->handle:NULL))
				{
					reloadconfigs = true;
					Con_DPrintf("Reloading configs because %s has changed\n", conffile[i]);
				}
			}

			if (reloadconfigs)
			{
				Cvar_SetEngineDefault(&fs_gamename, man->formalname?man->formalname:"FTE");
				Cvar_SetEngineDefault(&com_protocolname, man->protocolname?man->protocolname:"FTE");
				//FIXME: flag this instead and do it after a delay?
				Cvar_ForceSet(&fs_gamename, fs_gamename.enginevalue);
				Cvar_ForceSet(&com_protocolname, com_protocolname.enginevalue);

				if (isDedicated)
				{
	#ifndef CLIENTONLY
					SV_ExecInitialConfigs(man->defaultexec?man->defaultexec:"");
	#endif
				}
				else
				{
	#ifndef SERVERONLY
					CL_ExecInitialConfigs(man->defaultexec?man->defaultexec:"");
	#endif
				}
			}
		}

		//rebuild the cache now, should be safe to waste some cycles on it
		COM_FlushFSCache(false, true);
	}

//	COM_Effectinfo_Clear();
#ifndef SERVERONLY
	Validation_FlushFileList();	//prevent previous hacks from making a difference.
#endif

	{
		void (*callback)(struct cvar_s *var, char *oldvalue) = fs_game.callback;
		fs_game.callback = NULL;
		Cvar_ForceSet(&fs_game, FS_GetGamedir(false));
		fs_game.callback = callback;
	}
#ifdef Q2SERVER
	Cvar_ForceSet(&fs_gamedir, va("%s%s", com_gamepath, FS_GetGamedir(false)));
	Cvar_ForceSet(&fs_basedir, com_gamepath);
#endif

	return true;
}

typedef struct
{
	int found;
	qboolean (*callback)(void *usr, ftemanifest_t *man);
	void *usr;
} fmfenums_t;
static int QDECL FS_EnumerateFMFs(const char *fname, qofs_t fsize, void *inf, searchpathfuncs_t *spath)
{
	fmfenums_t *e = inf;
	vfsfile_t *f = NULL;
	char *homem = va("%s%s", com_homepathenabled?com_homepath:com_gamepath, COM_SkipPath(fname));
	if (!f)	//always try the homedir first, because that can be updated automagically.
		f = VFSOS_Open(fname, "rb");
	if (!f)
	{	//*then* try in packages or basedir etc.
		if (spath)
		{
			flocation_t loc;
			if (spath->FindFile(spath, &loc, fname, NULL))
				f = spath->OpenVFS(spath, &loc, "rb");
		}
		else
			f = VFSOS_Open(fname, "rb");
	}
	if (f)
	{
		size_t l = VFS_GETLEN(f);
		char *data = Z_Malloc(l+1);
		if (data)
		{
			ftemanifest_t *man;
			VFS_READ(f, data, l);
			data[l] = 0;	//just in case.

			man = FS_Manifest_Parse(homem, data);
			if (man)
			{
				if (e->callback(e->usr, man))
					e->found++;
				else
					FS_Manifest_Free(man);
			}
			Z_Free(data);
		}
		VFS_CLOSE(f);
	}

	return true;
}

void FS_EnumerateKnownGames(qboolean (*callback)(void *usr, ftemanifest_t *man), void *usr)
{
	int i;
	char basedir[MAX_OSPATH];
	fmfenums_t e;
	e.found = 0;
	e.callback = callback;
	e.usr = usr;

	//-basepack is primarily an android feature, where the apk file is specified.
	//this allows custom mods purely by customising the apk
	i = COM_CheckParm ("-basepack");
	while (i && i < com_argc-1)
	{
		const char *pakname = com_argv[i+1];
		searchpathfuncs_t *pak;
		vfsfile_t *vfs = VFSOS_Open(pakname, "rb");
		pak = FS_OpenPackByExtension(vfs, pakname);
		if (pak)
		{
			pak->EnumerateFiles(pak, "*.fmf", FS_EnumerateFMFs, &e);
			pak->ClosePath(pak);
		}
		i = COM_CheckNextParm ("-basepack", i);
	}

	//okay, no manifests in the basepack, try looking in the basedir.
	//this defaults to the working directory. perhaps try the exe's location instead?
	if (!e.found)
		Sys_EnumerateFiles(host_parms.basedir, "*.fmf", FS_EnumerateFMFs, &e, NULL);

	//right, no fmf files anywhere.
	//just make stuff up from whatever games they may have installed on their system.
	if (!e.found)
	{
		for (i = 0; gamemode_info[i].argname; i++)
		{
			if (gamemode_info[i].manifestfile || Sys_FindGameData(NULL, gamemode_info[i].argname+1, basedir, sizeof(basedir)))
			{
				ftemanifest_t *man = FS_GenerateLegacyManifest(NULL, 0, true, i);
				if (e.callback(e.usr, man))
					e.found++;
				else
					FS_Manifest_Free(man);
			}
		}
	}
}

//attempts to find a new basedir for 'input', changing to it as appropriate
//returns fixed up filename relative to the new gamedir.
//input must be an absolute path.
qboolean FS_FixupGamedirForExternalFile(char *input, char *filename, size_t fnamelen)
{
	char syspath[MAX_OSPATH];
	char gamepath[MAX_OSPATH];
	void *iterator;
	char *sep,*bs;
	char *src = NULL;

	Q_strncpyz(filename, input, fnamelen);

	iterator = NULL;
	while(COM_IteratePaths(&iterator, syspath, sizeof(syspath), gamepath, sizeof(gamepath)))
	{
		if (!Q_strncasecmp(syspath, filename, strlen(syspath)))
		{
			src = filename+strlen(syspath);
			memmove(filename, src, strlen(src)+1);
			break;
		}
	}
	if (!src)
	{
		for(;;)
		{
			sep = strchr(filename, '\\');
			if (sep)
				*sep = '/';
			else
				break;
		}
		for (sep = NULL;;)
		{
			bs = sep;
			sep = strrchr(filename, '/');
			if (bs)
				*bs = '/';
			if (sep)
			{
				int game;
				*sep = 0;
				if (strchr(filename, '/'))	//make sure there's always at least one /
				{
					char temp[MAX_OSPATH];
					Q_snprintfz(temp, sizeof(temp), "%s/", filename);
					game = FS_IdentifyDefaultGameFromDir(temp);
					if (game != -1)
					{
						static char newbase[MAX_OSPATH];
						if (!host_parms.basedir || strcmp(host_parms.basedir, filename))
						{
							Con_Printf("switching basedir+game to %s for %s\n", filename, input);
							Q_strncpyz(newbase, filename, sizeof(newbase));
							host_parms.basedir = newbase;
							FS_ChangeGame(FS_GenerateLegacyManifest(NULL, 0, true, game), true);
						}
						*sep = '/';
						sep = NULL;
						src = filename+strlen(host_parms.basedir);
						memmove(filename, src, strlen(src)+1);
						break;
					}
				}
			}
			else
				break;
		}
		if (sep)
			*sep = '/';
	}
	if (!src && host_parms.binarydir && !Q_strncasecmp(host_parms.binarydir, filename, strlen(host_parms.binarydir)))
	{
		src = filename+strlen(host_parms.binarydir);
		memmove(filename, src, strlen(src)+1);
	}
	if (!src && host_parms.basedir && !Q_strncasecmp(host_parms.basedir, filename, strlen(host_parms.basedir)))
	{
		src = filename+strlen(host_parms.basedir);
		memmove(filename, src, strlen(src)+1);
	}
	if (!src)
	{
		Q_snprintfz(filename, fnamelen, "#%s", input);
		return false;
	}
	if (*filename == '/' || *filename == '\\')
		memmove(filename, filename+1, strlen(filename+1)+1);

	sep = strchr(filename, '/');
	bs = strchr(filename, '\\');
	if (bs && (!sep || bs < sep))
		sep = bs;
	if (sep)
	{
		Con_Printf("switching gamedir for %s\n", filename);
		*sep = 0;
		COM_Gamedir(filename);
		memmove(filename, sep+1, strlen(sep+1)+1);
		return true;
	}
	Q_snprintfz(filename, fnamelen, "#%s", input);
	return false;
}


void FS_ChangeGame_f(void)
{
	int i;
	char *arg = Cmd_Argv(1);

	//don't execute this if we're executing rcon commands, as this can change game directories.
	if (cmd_blockwait)
		return;

	if (!*arg)
	{
		Con_Printf("Valid games are:\n");
		for (i = 0; gamemode_info[i].argname; i++)
		{
			Con_Printf(" %s\n", gamemode_info[i].argname+1);
		}
	}
	else
	{
		for (i = 0; gamemode_info[i].argname; i++)
		{			
			if (!Q_strcasecmp(gamemode_info[i].argname+1, arg))
			{
				Con_Printf("Switching to %s\n", gamemode_info[i].argname+1);
				FS_ChangeGame(FS_GenerateLegacyManifest(NULL, 0, true, i), true);
				return;
			}
		}

#ifndef SERVERONLY
		if (!Host_RunFile(arg, strlen(arg), NULL))
			Con_Printf("Game unknown\n");
#endif
	}
}
void FS_ShowManifest_f(void)
{
	if (fs_manifest)
		FS_Manifest_Print(fs_manifest);
	else
		Con_Printf("no manifest loaded...\n");
}
/*
================
COM_InitFilesystem

note: does not actually load any packs, just makes sure the basedir+cvars+etc is set up. vfs_fopens will still fail.
================
*/
void COM_InitFilesystem (void)
{
	int		i;

	char *ev;
	qboolean usehome;

	FS_RegisterDefaultFileSystems();

	Cmd_AddCommand("fs_restart", FS_ReloadPackFiles_f);
	Cmd_AddCommand("fs_changegame", FS_ChangeGame_f);
	Cmd_AddCommand("fs_showmanifest", FS_ShowManifest_f);

//
// -basedir <path>
// Overrides the system supplied base directory (under id1)
//
	i = COM_CheckParm ("-basedir");
	if (i && i < com_argc-1)
		strcpy (com_gamepath, com_argv[i+1]);
	else
		strcpy (com_gamepath, host_parms.basedir);

	FS_CleanDir(com_gamepath, sizeof(com_gamepath));


	Cvar_Register(&cfg_reload_on_gamedir, "Filesystem");
	Cvar_Register(&com_fs_cache, "Filesystem");
	Cvar_Register(&fs_gamename, "Filesystem");
	Cvar_Register(&fs_gamemanifest, "Filesystem");
	Cvar_Register(&com_protocolname, "Server Info");
	Cvar_Register(&fs_game, "Filesystem");
#ifdef Q2SERVER
	Cvar_Register(&fs_gamedir, "Filesystem");
	Cvar_Register(&fs_basedir, "Filesystem");
#endif

	usehome = false;

#if defined(_WIN32) && !defined(WINRT)
	{	//win32 sucks.
		HMODULE shfolder = LoadLibrary("shfolder.dll");
		DWORD winver = (DWORD)LOBYTE(LOWORD(GetVersion()));

		if (shfolder)
		{
			HRESULT (WINAPI *dSHGetFolderPath) (HWND hwndOwner, int nFolder, HANDLE hToken, DWORD dwFlags, LPTSTR pszPath);
			dSHGetFolderPath = (void *)GetProcAddress(shfolder, "SHGetFolderPathA");
			if (dSHGetFolderPath)
			{
				char folder[MAX_PATH];
				// 0x5 == CSIDL_PERSONAL
				if (dSHGetFolderPath(NULL, 0x5, NULL, 0, folder) == S_OK)
					Q_snprintfz(com_homepath, sizeof(com_homepath), "%s/My Games/%s/", folder, FULLENGINENAME);
			}
//			FreeLibrary(shfolder);
		}

		if (!*com_homepath)
		{
			ev = getenv("USERPROFILE");
			if (ev)
				Q_snprintfz(com_homepath, sizeof(com_homepath), "%s/My Documents/My Games/%s/", ev, FULLENGINENAME);
		}

#ifdef NPFTE
		if (!*com_homepath)
			Q_snprintfz(com_homepath, sizeof(com_homepath), "/%s/", FULLENGINENAME);
		//as a browser plugin, always use their home directory
		usehome = true;
#else
		/*would it not be better to just check to see if we have write permission to the basedir?*/
		if (winver >= 0x6) // Windows Vista and above
			usehome = true; // always use home directory by default, as Vista+ mimics this behavior anyway
		else if (winver >= 0x5) // Windows 2000/XP/2003
		{
			HMODULE advapi32;
			advapi32 = LoadLibrary("advapi32.dll");

			if (advapi32)
			{
				BOOL (WINAPI *dCheckTokenMembership) (HANDLE TokenHandle, PSID SidToCheck, PBOOL IsMember);
				dCheckTokenMembership = (void *)GetProcAddress(advapi32, "CheckTokenMembership");

				if (dCheckTokenMembership)
				{
					// on XP systems, only use a home directory by default if we're a limited user or if we're on a network
					BOOL isadmin, isonnetwork;
					SID_IDENTIFIER_AUTHORITY ntauth = {SECURITY_NT_AUTHORITY};
					PSID adminSID, networkSID;

					isadmin = AllocateAndInitializeSid(&ntauth,
						2,
						SECURITY_BUILTIN_DOMAIN_RID,
						DOMAIN_ALIAS_RID_ADMINS,
						0, 0, 0, 0, 0, 0,
						&adminSID);

					// just checking the network rid should be close enough to matching domain logins
					isonnetwork = AllocateAndInitializeSid(&ntauth,
						1,
						SECURITY_NETWORK_RID,
						0, 0, 0, 0, 0, 0, 0,
						&networkSID);

					if (isadmin && !dCheckTokenMembership(0, adminSID, &isadmin))
						isadmin = 0;

					if (isonnetwork && !dCheckTokenMembership(0, networkSID, &isonnetwork))
						isonnetwork = 0;

					usehome = isonnetwork || !isadmin;

					FreeSid(networkSID);
					FreeSid(adminSID);
				}

				FreeLibrary(advapi32);
			}
		}
#endif
	}
#else
	//yay for unix!.
	ev = getenv("HOME");
	if (ev && *ev)
	{
		if (ev[strlen(ev)-1] == '/')
			Q_snprintfz(com_homepath, sizeof(com_homepath), "%s.fte/", ev);
		else
			Q_snprintfz(com_homepath, sizeof(com_homepath), "%s/.fte/", ev);
		usehome = true; // always use home on unix unless told not to
	}
	else
		*com_homepath = '\0';
#endif

	com_homepathusable = usehome;
	com_homepathenabled = false;

	if (COM_CheckParm("-usehome"))
		com_homepathusable = true;
	if (COM_CheckParm("-nohome"))
		com_homepathusable = false;
	if (!*com_homepath)
		com_homepathusable = false;

	fs_readonly = COM_CheckParm("-readonly");

	fs_thread_mutex = Sys_CreateMutex();

#ifdef PLUGINS
	Plug_Initialise(false);
#endif
}





//this is at the bottom of the file to ensure these globals are not used elsewhere
extern searchpathfuncs_t *(QDECL VFSOS_OpenPath) (vfsfile_t *file, const char *desc);
#if 1//def AVAIL_ZLIB
extern searchpathfuncs_t *(QDECL FSZIP_LoadArchive) (vfsfile_t *packhandle, const char *desc);
#endif
extern searchpathfuncs_t *(QDECL FSPAK_LoadArchive) (vfsfile_t *packhandle, const char *desc);
#ifdef DOOMWADS
extern searchpathfuncs_t *(QDECL FSDWD_LoadArchive) (vfsfile_t *packhandle, const char *desc);
#endif
void FS_RegisterDefaultFileSystems(void)
{
	FS_RegisterFileSystemType(NULL, "pak", FSPAK_LoadArchive, true);
#if !defined(_WIN32) && !defined(ANDROID)
	/*for systems that have case sensitive paths, also include *.PAK */
	FS_RegisterFileSystemType(NULL, "PAK", FSPAK_LoadArchive, true);
#endif
	FS_RegisterFileSystemType(NULL, "pk3dir", VFSOS_OpenPath, true);
#if 1//def AVAIL_ZLIB
	FS_RegisterFileSystemType(NULL, "pk3", FSZIP_LoadArchive, true);
	FS_RegisterFileSystemType(NULL, "pk4", FSZIP_LoadArchive, true);
	FS_RegisterFileSystemType(NULL, "apk", FSZIP_LoadArchive, false);
	FS_RegisterFileSystemType(NULL, "zip", FSZIP_LoadArchive, false);
#endif
#ifdef DOOMWADS
	FS_RegisterFileSystemType(NULL, "wad", FSDWD_LoadArchive, true);
#endif
}
