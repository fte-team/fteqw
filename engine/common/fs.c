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

void FS_BeginManifestUpdates(void);
static void QDECL fs_game_callback(cvar_t *var, char *oldvalue);
hashtable_t filesystemhash;
qboolean com_fschanged = true;
qboolean com_installer = false;
qboolean fs_readonly;
int waitingformanifest;
static unsigned int fs_restarts;
void *fs_thread_mutex;

cvar_t com_fs_cache			= CVARF("fs_cache", IFMINIMAL("2","1"), CVAR_ARCHIVE);
cvar_t fs_noreexec			= CVARD("fs_noreexec", "0", "Disables automatic re-execing configs on gamedir switches.\nThis means your cvar defaults etc may be from the wrong mod, and cfg_save will leave that stuff corrupted!");	
cvar_t cfg_reload_on_gamedir = CVAR("cfg_reload_on_gamedir", "1");
cvar_t fs_game = CVARFCD("game", "", CVAR_NOSAVE|CVAR_NORESET, fs_game_callback, "Provided for Q2 compat.");
cvar_t fs_gamedir = CVARFD("fs_gamedir", "", CVAR_NOUNSAFEEXPAND|CVAR_NOSET|CVAR_NOSAVE, "Provided for Q2 compat.");
cvar_t fs_basedir = CVARFD("fs_basedir", "", CVAR_NOUNSAFEEXPAND|CVAR_NOSET|CVAR_NOSAVE, "Provided for Q2 compat.");
int active_fs_cachetype;
static int fs_referencetype;
int fs_finds;
void COM_CheckRegistered (void);

static void QDECL fs_game_callback(cvar_t *var, char *oldvalue)
{
	static qboolean runaway = false;
	char buf[MAX_OSPATH];
	if (!strcmp(var->string, oldvalue))
		return;	//no change here.
	if (runaway)
		return;	//ignore that
	runaway = true;
	Cmd_ExecuteString(va("gamedir %s\n", COM_QuotedString(var->string, buf, sizeof(buf), false)), RESTRICT_LOCAL);
	runaway = false;
}

struct
{
	void *module;
	const char *extension;
	searchpathfuncs_t *(QDECL *OpenNew)(vfsfile_t *file, const char *desc, const char *prefix);
	qboolean loadscan;
} searchpathformats[64];

int FS_RegisterFileSystemType(void *module, const char *extension, searchpathfuncs_t *(QDECL *OpenNew)(vfsfile_t *file, const char *desc, const char *prefix), qboolean loadscan)
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
	if (!fs_thread_mutex || Sys_LockMutex(fs_thread_mutex))
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
		if (fs_thread_mutex)
		{
			Sys_UnlockMutex(fs_thread_mutex);
			if (found)
			{
				Cmd_ExecuteString("fs_restart", RESTRICT_LOCAL);
			}
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







const char *FS_GetCleanPath(const char *pattern, char *outbuf, int outlen);
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
	Z_Free(man->downloadsurl);
	Z_Free(man->installupd);
	Z_Free(man->protocolname);
	Z_Free(man->eula);
	Z_Free(man->defaultexec);
	Z_Free(man->defaultoverrides);
	Z_Free(man->rtcbroker);
	for (i = 0; i < sizeof(man->gamepath) / sizeof(man->gamepath[0]); i++)
	{
		Z_Free(man->gamepath[i].path);
	}
	for (i = 0; i < sizeof(man->package) / sizeof(man->package[0]); i++)
	{
		Z_Free(man->package[i].path);
		Z_Free(man->package[i].prefix);
		Z_Free(man->package[i].condition);
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
	if (oldm->downloadsurl)
		newm->downloadsurl = Z_StrDup(oldm->downloadsurl);
	if (oldm->installupd)
		newm->installupd = Z_StrDup(oldm->installupd);
	if (oldm->protocolname)
		newm->protocolname = Z_StrDup(oldm->protocolname);
	if (oldm->eula)
		newm->eula = Z_StrDup(oldm->eula);
	if (oldm->defaultexec)
		newm->defaultexec = Z_StrDup(oldm->defaultexec);
	if (oldm->defaultoverrides)
		newm->defaultoverrides = Z_StrDup(oldm->defaultoverrides);
	if (oldm->rtcbroker)
		newm->rtcbroker = Z_StrDup(oldm->rtcbroker);
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
		if (oldm->package[i].prefix)
			newm->package[i].prefix = Z_StrDup(oldm->package[i].prefix);
		if (oldm->package[i].condition)
			newm->package[i].condition = Z_StrDup(oldm->package[i].condition);
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
	if (man->eula)
		Con_Printf("eula %s\n", COM_QuotedString(man->eula, buffer, sizeof(buffer), false));
	if (man->installation)
		Con_Printf("game %s\n", COM_QuotedString(man->installation, buffer, sizeof(buffer), false));
	if (man->formalname)
		Con_Printf("name %s\n", COM_QuotedString(man->formalname, buffer, sizeof(buffer), false));
	if (man->downloadsurl)
		Con_Printf("downloadsurl %s\n", COM_QuotedString(man->downloadsurl, buffer, sizeof(buffer), false));
	if (man->installupd)
		Con_Printf("install %s\n", COM_QuotedString(man->installupd, buffer, sizeof(buffer), false));
	if (man->protocolname)
		Con_Printf("protocolname %s\n", COM_QuotedString(man->protocolname, buffer, sizeof(buffer), false));
	if (man->defaultexec)
		Con_Printf("defaultexec %s\n", COM_QuotedString(man->defaultexec, buffer, sizeof(buffer), false));
	if (man->defaultoverrides)
		Con_Printf("%s", man->defaultoverrides);
	if (man->rtcbroker)
		Con_Printf("rtcbroker %s\n", COM_QuotedString(man->rtcbroker, buffer, sizeof(buffer), false));

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
			if (man->package[i].type == mdt_installation)
				Con_Printf("library ");
			else
				Con_Printf("package ");
			Con_Printf("%s", COM_QuotedString(man->package[i].path, buffer, sizeof(buffer), false));
			if (man->package[i].condition)
				Con_Printf(" prefix %s", COM_QuotedString(man->package[i].condition, buffer, sizeof(buffer), false));
			if (man->package[i].condition)
				Con_Printf(" condition %s", COM_QuotedString(man->package[i].condition, buffer, sizeof(buffer), false));
			if (man->package[i].crcknown)
				Con_Printf(" crc 0x%x", man->package[i].crc);
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

#ifdef _DEBUG	//FOR TEMPORARY TESTING ONLY.
//	man->doinstall = true;
#endif

	if (syspath)
		man->updatefile = Z_StrDup(syspath);	//this should be a system path.
	return man;
}

static qboolean FS_Manifest_ParsePackage(ftemanifest_t *man, int type)
{
	char *path = "";
	unsigned int crc = 0;
	qboolean crcknown = false;
	char *legacyextractname = NULL;
	char *condition = NULL;
	char *prefix = NULL;
	char *arch = NULL;
	unsigned int arg = 1;
	unsigned int mirrors = 0;
	char *mirror[countof(man->package[0].mirrors)];
	size_t i, j;
	char *a;

	a = Cmd_Argv(0);
	if (!Q_strcasecmp(a, "filedependancies") || !Q_strcasecmp(a, "archiveddependancies"))
		arch = Cmd_Argv(arg++);

	path = Cmd_Argv(arg++);

#ifndef NOLEGACY
	a = Cmd_Argv(arg);
	if (!strcmp(a, "-"))
	{
		arg++;
	}
	else if (*a)
	{
		crc = strtoul(a, &a, 0);
		if (!*a)
		{
			crcknown = true;
			arg++;
		}
	}

	if (!strncmp(Cmd_Argv(0), "archived", 8))
		legacyextractname = Cmd_Argv(arg++);
#endif

	while (arg < Cmd_Argc())
	{
		a = Cmd_Argv(arg++);
		if (!strcmp(a, "crc"))
		{
			crcknown = true;
			crc = strtoul(Cmd_Argv(arg++), NULL, 0);
		}
		else if (!strcmp(a, "condition"))
			condition = Cmd_Argv(arg++);
		else if (!strcmp(a, "prefix"))
			prefix = Cmd_Argv(arg++);
		else if (!strcmp(a, "arch"))
			arch = Cmd_Argv(arg++);
		else if (!strcmp(a, "mirror"))
		{
			a = Cmd_Argv(arg++);
			goto mirror;	//oo evil.
		}
		else if (strchr(a, ':') || man->parsever < 1)
		{
mirror:
			if (mirrors == countof(mirror))
				Con_Printf("too many mirrors for package %s\n", path);
			else if (legacyextractname)
			{
				if (!strcmp(legacyextractname, "xz") || !strcmp(legacyextractname, "gz"))
					mirror[mirrors++] = Z_StrDup(va("%s:%s", legacyextractname, a));
				else
					mirror[mirrors++] = Z_StrDup(va("unzip:%s,%s", legacyextractname, a));
			}
			else
				mirror[mirrors++] = Z_StrDup(a);
		}
		else if (man->parsever <= MANIFEST_CURRENTVER)
			Con_Printf("unknown mirror / property %s for package %s\n", a, path);
	}

	for (i = 0; i < countof(man->package); i++)
	{
		if (!man->package[i].path)
		{
			if (type == mdt_singlepackage && (!strchr(path, '/') || strchr(path, ':') || strchr(path, '\\')))
			{
				Con_Printf("invalid package path specified in manifest (%s)\n", path);
				break;
			}
			if (arch)
			{
#ifdef PLATFORM
				if (Q_strcasecmp(PLATFORM, arch))
#endif
					break;
			}
			man->package[i].type = type;
			man->package[i].path = Z_StrDup(path);
			man->package[i].prefix = prefix?Z_StrDup(prefix):NULL;
			man->package[i].condition = condition?Z_StrDup(condition):NULL;
			man->package[i].crcknown = crcknown;
			man->package[i].crc = crc;
			for (j = 0; j < mirrors; j++)
				man->package[i].mirrors[j] = mirror[j];
			return true;
		}
	}
	if (i == countof(man->package))
		Con_Printf("Too many packages specified in manifest\n");
	for (j = 0; j < mirrors; j++)
		Z_Free(mirror[j]);
	return false;
}

//parse Cmd_Argv tokens into the manifest.
static qboolean FS_Manifest_ParseTokens(ftemanifest_t *man)
{
	qboolean result = true;
	char *cmd = Cmd_Argv(0);
	if (!*cmd)
		return result;

	if (*cmd == '*')
		cmd++;

	if (!Q_strcasecmp(cmd, "ftemanifestver") || !Q_strcasecmp(cmd, "ftemanifest"))
		man->parsever = atoi(Cmd_Argv(1));
	else if (!Q_strcasecmp(cmd, "minver"))
	{
		//ignore minimum versions for other engines.
		if (!strcmp(Cmd_Argv(2), DISTRIBUTION))
			man->minver = atoi(Cmd_Argv(3));
	}
	else if (!Q_strcasecmp(cmd, "maxver"))
	{
		//ignore minimum versions for other engines.
		if (!strcmp(Cmd_Argv(2), DISTRIBUTION))
			man->maxver = atoi(Cmd_Argv(3));
	}
	else if (!Q_strcasecmp(cmd, "game"))
	{
		Z_Free(man->installation);
		man->installation = Z_StrDup(Cmd_Argv(1));
	}
	else if (!Q_strcasecmp(cmd, "name"))
	{
		Z_Free(man->formalname);
		man->formalname = Z_StrDup(Cmd_Argv(1));
	}
	else if (!Q_strcasecmp(cmd, "eula"))
	{
		Z_Free(man->eula);
		man->eula = Z_StrDup(Cmd_Argv(1));
	}
	else if (!Q_strcasecmp(cmd, "downloadsurl"))
	{
		Z_Free(man->downloadsurl);
		man->downloadsurl = Z_StrDup(Cmd_Argv(1));
	}
	else if (!Q_strcasecmp(cmd, "install"))
	{
		Z_Free(man->installupd);
		man->installupd = Z_StrDup(Cmd_Argv(1));
	}
	else if (!Q_strcasecmp(cmd, "protocolname"))
	{
		Z_Free(man->protocolname);
		man->protocolname = Z_StrDup(Cmd_Argv(1));
	}
	else if (!Q_strcasecmp(cmd, "defaultexec"))
	{
		Z_Free(man->defaultexec);
		man->defaultexec = Z_StrDup(Cmd_Argv(1));
	}
	else if (!Q_strcasecmp(cmd, "bind") || !Q_strcasecmp(cmd, "set") || !Q_strcasecmp(cmd, "seta"))
	{
		Z_StrCat(&man->defaultoverrides, va("%s %s\n", Cmd_Argv(0), Cmd_Args()));
	}
	else if (!Q_strcasecmp(cmd, "rtcbroker"))
	{
		Z_Free(man->rtcbroker);
		man->rtcbroker = Z_StrDup(Cmd_Argv(1));
	}
	else if (!Q_strcasecmp(cmd, "updateurl"))
	{
		Z_Free(man->updateurl);
		man->updateurl = Z_StrDup(Cmd_Argv(1));
	}
	else if (!Q_strcasecmp(cmd, "disablehomedir"))
	{
		man->disablehomedir = !!atoi(Cmd_Argv(1));
	}
	else if (!Q_strcasecmp(cmd, "basegame") || !Q_strcasecmp(cmd, "gamedir"))
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
					man->gamepath[i].base = !Q_strcasecmp(cmd, "basegame");
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
#ifndef NOLEGACY
	else if (!Q_strcasecmp(cmd, "filedependancies") || !Q_strcasecmp(cmd, "archiveddependancies"))
		FS_Manifest_ParsePackage(man, mdt_installation);
	else if (!Q_strcasecmp(cmd, "archivedpackage"))
		FS_Manifest_ParsePackage(man, mdt_singlepackage);
#endif
	else if (!Q_strcasecmp(cmd, "library"))
		FS_Manifest_ParsePackage(man, mdt_installation);
	else if (!Q_strcasecmp(cmd, "package") || !Q_strcasecmp(cmd, "archivedpackage"))
		FS_Manifest_ParsePackage(man, mdt_singlepackage);
	else
	{
		Con_Printf("Unknown token: %s\n", cmd);
		result = false;
	}
	return result;
}
//read a manifest file
ftemanifest_t *FS_Manifest_Parse(const char *fname, const char *data)
{
	ftemanifest_t *man;
	if (!data)
		return NULL;
	while (*data == ' ' || *data == '\t' || *data == '\r' || *data == '\n')
		data++;
	if (!*data)
		return NULL;

	man = FS_Manifest_Create(fname);

	while (data && *data)
	{
		data = Cmd_TokenizeString((char*)data, false, false);
		if (!FS_Manifest_ParseTokens(man) && man->parsever <= 1)
		{
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


char *fs_loadedcommand;			//execed once all packages are (down)loaded
ftemanifest_t	*fs_manifest;	//currently active manifest.
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
	flocation_t loc;
	if (FS_FLocateFile(path, FSLF_IFFOUND, &loc))
		return loc.len;
	else
		return -1;
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
	Con_Printf(U8("%s")"  %s%s%s%s%s%s%s\n", s->logicalpath,
		(s->flags & SPF_REFERENCED)?"^[(ref)\\tip\\Referenced\\desc\\Package will auto-download to clients^]":"",
		(s->flags & SPF_TEMPORARY)?"^[(temp)\\tip\\Temporary\\desc\\Flushed on map change^]":"",
		(s->flags & SPF_COPYPROTECTED)?"^[(c)\\tip\\Copyrighted\\desc\\Copy-Protected and is not downloadable^]":"",
		(s->flags & SPF_EXPLICIT)?"^[(e)\\tip\\Explicit\\desc\\Loaded explicitly by the gamedir^]":"",
		(s->flags & SPF_UNTRUSTED)?"^[(u)\\tip\\Untrusted\\desc\\Configs and scripts will not be given access to passwords^]":"",
		(s->flags & SPF_WRITABLE)?"^[(w)\\tip\\Writable\\desc\\We can probably write here^]":"",
		(s->handle->GeneratePureCRC)?va("^[(h)\\tip\\Hash: %x^]", s->handle->GeneratePureCRC(s->handle, 0, 0)):"");
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
static int QDECL COM_Dir_List(const char *name, qofs_t size, time_t mtime, void *parm, searchpathfuncs_t *spath)
{
	searchpath_t	*s;
	char link[512];
	char *colour;
	flocation_t loc;
	for (s=com_searchpaths ; s ; s=s->next)
	{
		if (s->handle == spath)
			break;
	}

	if (*name && name[strlen(name)-1] == '/')
	{
		colour = "^7";	//superseeded
		Q_snprintfz(link, sizeof(link), "\\dir\\%s*", name);
	}
	else if (!FS_FLocateFile(name, FSLF_IFFOUND, &loc))
	{
		colour = "^1";	//superseeded
		Q_snprintfz(link, sizeof(link), "\\tip\\flocate error");
	}
	else if (loc.search->handle == spath)
	{
		colour = "^2";
		COM_FileExtension(name, link, sizeof(link));
		if ((!Q_strcasecmp(link, "bsp") || !Q_strcasecmp(link, "map") || !Q_strcasecmp(link, "hmp")) && !strncmp(name, "maps/", 5) && strncmp(name, "maps/b_", 7))
		{
			Q_snprintfz(link, sizeof(link), "\\map\\%s", name);
			colour = "^4";	//disconnects
		}
		else if (!Q_strcasecmp(link, "bsp") || !Q_strcasecmp(link, "spr") || !Q_strcasecmp(link, "mdl") || !Q_strcasecmp(link, "md3") || !Q_strcasecmp(link, "iqm") || !Q_strcasecmp(link, "vvm") || !Q_strcasecmp(link, "psk") || !Q_strcasecmp(link, "dpm") || !Q_strcasecmp(link, "zym") || !Q_strcasecmp(link, "md5mesh") || !Q_strcasecmp(link, "md5anim"))
			Q_snprintfz(link, sizeof(link), "\\modelviewer\\%s", name);
		else if (!Q_strcasecmp(link, "qc") || !Q_strcasecmp(link, "src") || !Q_strcasecmp(link, "qh") || !Q_strcasecmp(link, "h") || !Q_strcasecmp(link, "c")
			|| !Q_strcasecmp(link, "cfg") || !Q_strcasecmp(link, "rc")
			|| !Q_strcasecmp(link, "txt") || !Q_strcasecmp(link, "log")
			|| !Q_strcasecmp(link, "ent") || !Q_strcasecmp(link, "rtlights")
			|| !Q_strcasecmp(link, "shader") || !Q_strcasecmp(link, "framegroups"))
			Q_snprintfz(link, sizeof(link), "\\edit\\%s", name);
		else if (!Q_strcasecmp(link, "tga") || !Q_strcasecmp(link, "png") || !Q_strcasecmp(link, "jpg") || !Q_strcasecmp(link, "jpeg") || !Q_strcasecmp(link, "lmp") || !Q_strcasecmp(link, "pcx")|| !Q_strcasecmp(link, "bmp"))
		{
			//FIXME: image replacements are getting in the way here.
			Q_snprintfz(link, sizeof(link), "\\tiprawimg\\%s\\tip\\(note: image replacement rules are context-dependant, including base path, sub path, extension, or complete replacement via a shader)", name);
			colour = "^6";	//shown on mouseover
		}
		else if (!Q_strcasecmp(link, "qwd") || !Q_strcasecmp(link, "dem") || !Q_strcasecmp(link, "mvd") || !Q_strcasecmp(link, "dm2"))
		{
			Q_snprintfz(link, sizeof(link), "\\demo\\%s", name);
			colour = "^4";	//disconnects
		}
		else if (!Q_strcasecmp(link, "roq") || !Q_strcasecmp(link, "cin") || !Q_strcasecmp(link, "avi") || !Q_strcasecmp(link, "mp4") || !Q_strcasecmp(link, "mkv"))
			Q_snprintfz(link, sizeof(link), "\\film\\%s", name);
		else
		{
			colour = "^3";	//nothing
			*link = 0;
		}
	}
	else
	{
		char *gah;
		colour = "^1";	//superseeded
		Q_snprintfz(link, sizeof(link), "\\tip\\overriden by file from %s", loc.search->logicalpath);
		gah = link + 20;	//whatever
		while ((gah = strchr(gah, '\\')))
			*gah = '/';
	}

	if (size > 1.0*1024*1024*1024)
		Con_Printf(U8("^[%s%s%s^] \t(%#.3ggb) (%s)\n"), colour, name, link, size/(1024.0*1024*1024), s?s->logicalpath:"??");
	else if (size > 1.0*1024*1024)
		Con_Printf(U8("^[%s%s%s^] \t(%#.3gmb) (%s)\n"), colour, name, link, size/(1024.0*1024), s?s->logicalpath:"??");
	else if (size > 1.0*1024)
		Con_Printf(U8("^[%s%s%s^] \t(%#.3gkb) (%s)\n"), colour, name, link, size/1024.0, s?s->logicalpath:"??");
	else
		Con_Printf(U8("^[%s%s%s^] \t(%ub) (%s)\n"), colour, name, link, (unsigned int)size, s?s->logicalpath:"??");
	return 1;
}

void COM_Dir_f (void)
{
	char match[MAX_QPATH];

	if (Cmd_Argc()>1)
		Q_strncpyz(match, Cmd_Argv(1), sizeof(match));
	else
		Q_strncpyz(match, "*", sizeof(match));

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
	char *f = Cmd_Argv(1);
	if (strchr(f, '^'))	//fte's filesystem is assumed to be utf-8, but that doesn't mean that console input is. and I'm too lazy to utf-8ify the string (in part because markup can be used to exploit ascii assumptions).
		Con_Printf("Warning: filename contains markup. If this is because of unicode, set com_parseutf8 1\n");
	if (FS_FLocateFile(f, FSLF_IFFOUND, &loc))
	{
		if (!*loc.rawname)
		{
			Con_Printf("File is %u bytes compressed inside "U8("%s")"\n", (unsigned)loc.len, loc.search->logicalpath);
		}
		else
		{
			Con_Printf("Inside "U8("%s")" (%u bytes)\n  "U8("%s")"\n", loc.rawname, (unsigned)loc.len, loc.search->logicalpath);
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
void COM_WriteFile (const char *filename, enum fs_relative fsroot, const void *data, int len)
{
	vfsfile_t *vfs;

	Sys_Printf ("COM_WriteFile: %s\n", filename);

	FS_CreatePath(filename, fsroot);
	vfs = FS_OpenVFS(filename, "wb", fsroot);
	if (vfs)
	{
		VFS_WRITE(vfs, data, len);
		VFS_CLOSE(vfs);

		if (fsroot >= FS_GAME)
			FS_FlushFSHashWritten(filename);
		else
			com_fschanged=true;
	}
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

static void FS_FlushFSHashReally(qboolean domutexes)
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

static void FS_RebuildFSHash(qboolean domutex)
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

static void FS_RebuildFSHash_Update(const char *fname)
{
	flocation_t loc;
	searchpath_t *search;
	int depth = 0;
	fsbucket_t *old;
	void *filehandle = NULL;

	if (com_fschanged)
		return;

	if (!filehandle && com_purepaths)
	{	//go for the pure paths first.
		for (search = com_purepaths; search; search = search->nextpure)
		{
			if (search->handle->FindFile(search->handle, &loc, fname, NULL))
			{
				filehandle = loc.fhandle;
				break;
			}
			depth++;
		}
	}
	if (!filehandle && fs_puremode < 2)
	{
		for (search = com_searchpaths ; search ; search = search->next)
		{
			if (search->handle->FindFile(search->handle, &loc, fname, NULL))
			{
				filehandle = loc.fhandle;
				break;
			}
			depth++;
		}
	}


	COM_WorkerFullSync();
	if (!Sys_LockMutex(fs_thread_mutex))
		return;	//amg!

	old = Hash_GetInsensitiveBucket(&filesystemhash, fname);
	if (old)
	{
		Hash_RemoveBucket(&filesystemhash, fname, &old->buck);
		fs_hash_files--;
	}

	if (filehandle)
		FS_AddFileHash(depth, fname, NULL, filehandle);

	Sys_UnlockMutex(fs_thread_mutex);
}

void FS_FlushFSHashWritten(const char *fname)
{
	FS_RebuildFSHash_Update(fname);
}
void FS_FlushFSHashRemoved(const char *fname)
{
	FS_RebuildFSHash_Update(fname);
}
void FS_FlushFSHashFull(void)
{	//any calls to this are typically a bug...
	//that said, figuring out if the file was actually within quake's filesystem isn't easy.
	com_fschanged = true;

	//for safety we would need to sync with all threads, so lets just not bother.
	//FS_FlushFSHashReally(true);
}


/*
===========
COM_FindFile

Finds the file in the search path.
Sets com_filesize and one of handle or file
===========
*/
//if loc is valid, loc->search is always filled in, the others are filled on success.
//returns 0 if couldn't find.
int FS_FLocateFile(const char *filename, unsigned int lflags, flocation_t *loc)
{
	int depth=0;
	searchpath_t	*search;
	char cleanpath[MAX_OSPATH];
	flocation_t allownoloc;

	void *pf;
	unsigned int found = FF_NOTFOUND;

	if (!loc)
		loc = &allownoloc;

	loc->fhandle = NULL;
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

	if (com_fs_cache.ival && !com_fschanged && !(lflags & FSLF_IGNOREPURE))
	{
		pf = Hash_GetInsensitive(&filesystemhash, filename);
		if (!pf)
			goto fail;
	}
	else
		pf = NULL;

	if (com_purepaths && found == FF_NOTFOUND && !(lflags & FSLF_IGNOREPURE))
	{
		//check if its in one of the 'pure' packages. these override the default ones.
		for (search = com_purepaths ; search ; search = search->nextpure)
		{
			if ((lflags & FSLF_SECUREONLY) && !(search->flags & SPF_UNTRUSTED))
				continue;
			if (!((lflags & FSLF_IGNOREBASEDEPTH) && (search->flags & SPF_BASEPATH)))
				depth += ((search->flags & SPF_EXPLICIT) || (lflags & FSLF_DEPTH_INEXPLICIT));
			fs_finds++;
			found = search->handle->FindFile(search->handle, loc, filename, pf);
			if (found)
			{
				if (!(lflags & FSLF_DONTREFERENCE))
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

	if (((lflags & FSLF_IGNOREPURE) || fs_puremode < 2) && found == FF_NOTFOUND)
	{
		// optionally check the non-pure paths too.
		for (search = com_searchpaths ; search ; search = search->next)
		{
			if ((lflags & FSLF_SECUREONLY) && (search->flags & SPF_UNTRUSTED))
				continue;
			if (!((lflags & FSLF_IGNOREBASEDEPTH) && (search->flags & SPF_BASEPATH)))
				depth += ((search->flags & SPF_EXPLICIT) || (lflags & FSLF_DEPTH_INEXPLICIT));
			fs_finds++;
			found = search->handle->FindFile(search->handle, loc, filename, pf);
			if (found)
			{
				if (!(lflags & FSLF_DONTREFERENCE))
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
	if (found == FF_SYMLINK && !(lflags & FSLF_IGNORELINKS))
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
			depth = FS_FLocateFile(mergedname, lflags, loc);
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
	if (found == FF_NOTFOUND || loc->len == -1)
	{
		if (lflags & FSLF_DEEPONFAILURE)
			return 0x7fffffff;	//if we're asking for depth, the file is reported to be so far into the filesystem as to be irrelevant.
		return 0;
	}
	return depth+1;
}

//returns the package/'gamedir/foo.pk3' filename to tell the client to download
//unfortunately foo.pk3 may contain a 'bar.pk3' and downloading dir/foo.pk3/bar.pk3 won't work
//so if loc->search is dir/foo.pk3/bar.pk3 find dir/foo.pk3 instead
char *FS_GetPackageDownloadFilename(flocation_t *loc)
{
	searchpath_t *sp, *search;

	for (sp = loc->search; ;)
	{
		for (search = com_searchpaths; search; search = search->next)
		{
			if (search != sp)
				if (search->handle->GeneratePureCRC) //only consider files that have a pure hash. this excludes system paths
					if (!strncmp(search->purepath, sp->purepath, strlen(search->purepath)))
						if (sp->purepath[strlen(search->purepath)] == '/')	//also ensures that the path gets shorter, avoiding infinite loops as it fights between base+home dirs.
							break;
		}
		if (search)
			sp = search;
		else
			break;
	}

	if (sp && strchr(sp->purepath, '/'))	//never allow any packages that are directly sitting in the basedir.
		return sp->purepath;
	return NULL;
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
const char *FS_GetCleanPath(const char *pattern, char *outbuf, int outlen)
{
	const char *s;
	char *o;
	char *seg;
	char *end = outbuf + outlen;

	s = pattern;
	seg = o = outbuf;
	if (!pattern)
	{
		Con_Printf("Error: Empty filename\n");
		return NULL;
	}
	for(;;)
	{
		if (o == end)
		{
			Con_Printf("Error: filename too long\n");
			return NULL;
		}
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
			if (o == seg)
			{
				if (o == outbuf)
				{
					Con_Printf("Error: absolute path in filename %s\n", pattern);
					return NULL;
				}
				if (!*s)
				{
					*o++ = '\0';
					break;
				}
				Con_Printf("Error: empty directory name (%s)\n", pattern);
				s++;
				continue;
			}
			//ignore any leading spaces in the name segment
			//it should just make more stuff invalid
			while (*seg == ' ')
				seg++;
			if (!seg[0])
			{
				Con_Printf("Error: No filename (%s)\n", pattern);
				return NULL;
			}
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

	if (!handle || handle->WriteBytes || handle->seekstyle == SS_SLOW || handle->seekstyle == SS_UNSEEKABLE)	//only on readonly files for which we can undo any header read damage
		return handle;
//	ext = COM_FileExtension (filename);
#ifdef AVAIL_GZDEC
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

	if (*fname == 0)
	{
		//this is sometimes used to query the actual path.
		//don't alow it for other stuff though.
		if (relativeto != FS_ROOT && relativeto != FS_BINARYPATH)
			return false;
	}
	else
	{
		fname = FS_GetCleanPath(fname, cleanname, sizeof(cleanname));
		if (!fname)
			return false;
	}

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
		if (com_installer)
			return false;
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
				break;
			}
		}
		if (!last)
			return false;	//eep?
		if (com_homepathenabled)
			snprintf(out, outlen, "%s%s/%s", com_homepath, last, fname);
		else
			snprintf(out, outlen, "%s%s/%s", com_gamepath, last, fname);
		break;
	case FS_PUBGAMEONLY:
		last = NULL;
		for (i = 0; i < sizeof(fs_manifest->gamepath)/sizeof(fs_manifest->gamepath[0]); i++)
		{
			if (fs_manifest->gamepath[i].path)
			{
				if (*fs_manifest->gamepath[i].path == '*')
					continue;
				last = fs_manifest->gamepath[i].path;
				break;
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
			if (fs_manifest && fs_manifest->gamepath[i].base && fs_manifest->gamepath[i].path)
			{
				if (*fs_manifest->gamepath[i].path == '*')
					continue;
				last = fs_manifest->gamepath[i].path;
				break;
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

//returns false to stop the enumeration. check the return value of the fs enumerator to see if it was canceled by this return value.
static int QDECL FS_NullFSEnumerator(const char *fname, qofs_t fsize, time_t mtime, void *parm, searchpathfuncs_t *spath)
{
	return false;
}

//opens a file in the same (writable) path that contains an existing version of the file or one of the other patterns listed
vfsfile_t *FS_OpenWithFriends(const char *fname, char *sysname, size_t sysnamesize, int numfriends, ...)
{
	searchpath_t *search;
	searchpath_t *lastwritable = NULL;
	flocation_t loc;
	va_list ap;
	int i;
	char cleanname[MAX_QPATH];

	fname = FS_GetCleanPath(fname, cleanname, sizeof(cleanname));
	if (!fname)
		return NULL;

	for (search = com_searchpaths; search ; search = search->next)
	{
		if ((search->flags & SPF_EXPLICIT) && (search->flags & SPF_WRITABLE))
			lastwritable = search;
		if (search->handle->FindFile(search->handle, &loc, fname, NULL))
			break;
		
		va_start(ap, numfriends);
		for (i = 0; i < numfriends; i++)
		{
			char *path = va_arg(ap, char*);
			if (!search->handle->EnumerateFiles(search->handle, path, FS_NullFSEnumerator, NULL))
				break;
		}
		va_end(ap);
		if (i < numfriends)
			break;
	}
	if (lastwritable)
	{
		//figure out the system path
		Q_strncpyz(sysname, lastwritable->logicalpath, sysnamesize);
		FS_CleanDir(sysname, sysnamesize);
		Q_strncatz(sysname, fname, sysnamesize);

		//create the dir if needed and open the file.
		COM_CreatePath(sysname);
		return VFSOS_Open(sysname, "wbp");
	}
	FS_NativePath(fname, FS_GAMEONLY, sysname, sysnamesize);
	return NULL;
}

//returns false if the string didn't fit. we're not trying to be clever and reallocate the buffer
qboolean try_snprintf(char *buffer, size_t size, const char *format, ...)
{
	size_t ret;
	va_list		argptr;

	va_start (argptr, format);
#ifdef _WIN32
#undef _vsnprintf
	ret = _vsnprintf(buffer, size, format, argptr);
#define _vsnprintf unsafe_vsnprintf
#else
	ret = vsnprintf (buffer, size, format,argptr);
#endif
	va_end (argptr);
	if (ret > size-1)	//should cope with microsoft's -1s and linuxes total-length return values.
		return false;
	return true;
}

/*locates and opens a file
modes:
r = read
w = write
a = append
t = text mode (because windows sucks). binary is otherwise assumed.
p = persist (ie: saved games and configs, but not downloads or large content)
*/
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
		vfs = NULL;
		if (com_homepathenabled)
		{
			if (!try_snprintf(fullname, sizeof(fullname), "%s%s/%s", com_homepath, gamedirfile, filename))
				return NULL;
			if (*mode == 'w')
				COM_CreatePath(fullname);
			vfs = VFSOS_Open(fullname, mode);
		}
		if (!vfs && *gamedirfile)
		{
			if (!try_snprintf(fullname, sizeof(fullname), "%s%s/%s", com_gamepath, gamedirfile, filename))
				return NULL;
			if (*mode == 'w')
				COM_CreatePath(fullname);
			vfs =  VFSOS_Open(fullname, mode);
		}
		if (vfs || !(*mode == 'w' || *mode == 'a'))
			return vfs;
		//fall through
	case FS_PUBGAMEONLY:
		if (!FS_NativePath(filename, relativeto, fullname, sizeof(fullname)))
			return NULL;
		if (*mode == 'w')
			COM_CreatePath(fullname);
		return VFSOS_Open(fullname, mode);
	case FS_GAME:	//load from paks in preference to system paths. overwriting be damned.
	case FS_PUBBASEGAMEONLY:	//load from paks in preference to system paths. overwriting be damned.
		if (!FS_NativePath(filename, relativeto, fullname, sizeof(fullname)))
			return NULL;
		break;
	case FS_BINARYPATH:
		if (!FS_NativePath(filename, relativeto, fullname, sizeof(fullname)))
			return NULL;
		if (*mode == 'w')
			COM_CreatePath(fullname);
		return VFSOS_Open(fullname, mode);
	case FS_ROOT:	//always bypass packs and gamedirs
		if (com_installer)
			return NULL;
		if (com_homepathenabled)
		{
			if (!try_snprintf(fullname, sizeof(fullname), "%s%s", com_homepath, filename))
				return NULL;
			vfs = VFSOS_Open(fullname, mode);
			if (vfs)
				return vfs;
		}
		if (!try_snprintf(fullname, sizeof(fullname), "%s%s", com_gamepath, filename))
			return NULL;
		return VFSOS_Open(fullname, mode);
	case FS_BASEGAMEONLY:		//always bypass packs+pure.
		if (com_homepathenabled)
		{
			if (!try_snprintf(fullname, sizeof(fullname), "%sfte/%s", com_homepath, filename))
				return NULL;
			vfs = VFSOS_Open(fullname, mode);
			if (vfs)
				return vfs;
		}
		if (!try_snprintf(fullname, sizeof(fullname), "%sfte/%s", com_gamepath, filename))
			return NULL;
		return VFSOS_Open(fullname, mode);
	default:
		Sys_Error("FS_OpenVFS: Bad relative path (%i)", relativeto);
		break;
	}

	FS_FLocateFile(filename, FSLF_IFFOUND, &loc);

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

qboolean FS_GetLocMTime(flocation_t *location, time_t *modtime)
{
	*modtime = 0;
	if (!location->search->handle->FileStat || !location->search->handle->FileStat(location->search->handle, location, modtime))
		return false;
	return true;
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
	
	if (!FS_FLocateFile(path, FSLF_IFFOUND, &loc) || !loc.search)
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



void COM_EnumerateFiles (const char *match, int (QDECL *func)(const char *, qofs_t, time_t mtime, void *, searchpathfuncs_t*), void *parm)
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

static searchpath_t *FS_AddPathHandle(searchpath_t **oldpaths, const char *purepath, const char *probablepath, searchpathfuncs_t *handle, const char *prefix, unsigned int flags, unsigned int loadstuff);
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
	searchpathfuncs_t *(QDECL *OpenNew)(vfsfile_t *file, const char *desc, const char *prefix);
	searchpath_t **oldpaths;
	const char *parentdesc;
	const char *puredesc;
	unsigned int inheritflags;
} wildpaks_t;

static int QDECL FS_AddWildDataFiles (const char *descriptor, qofs_t size, time_t mtime, void *vparam, searchpathfuncs_t *funcs)
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
		newpak = param->OpenNew (vfs, pakfile, "");
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
	FS_AddPathHandle(param->oldpaths, purefile, pakfile, newpak, "", ((!Q_strncasecmp(descriptor, "pak", 3))?SPF_COPYPROTECTED:0)|keptflags|param->inheritflags, (unsigned int)-1);

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
			pak = searchpathformats[j].OpenNew(f, pakname, "");
			if (pak)
				return pak;
			Con_Printf("Unable to open %s - corrupt?\n", pakname);
			break;
		}
	}

	VFS_CLOSE(f);
	return NULL;
}

//
void FS_AddHashedPackage(searchpath_t **oldpaths, const char *parentpath, const char *logicalpaths, searchpath_t *search, unsigned int loadstuff, const char *pakpath, const char *qhash, const char *pakprefix)
{
	searchpathfuncs_t	*handle;
	searchpath_t *oldp;
	char pname[MAX_OSPATH];
	char lname[MAX_OSPATH];
	char lname2[MAX_OSPATH];
	unsigned int	keptflags;
	flocation_t loc;
	int fmt;
	char ext[8];
	int ptlen = strlen(parentpath);

	COM_FileExtension(pakpath, ext, sizeof(ext));

	//figure out which file format its meant to be.
	for (fmt = 0; fmt < sizeof(searchpathformats)/sizeof(searchpathformats[0]); fmt++)
	{
		if (!searchpathformats[fmt].extension || !searchpathformats[fmt].OpenNew)// || !searchpathformats[i].loadscan)
			continue;
		if ((loadstuff & (1<<fmt)) && !Q_strcasecmp(ext, searchpathformats[fmt].extension))
		{

			//figure out the logical path names
			if (!FS_GenCachedPakName(pakpath, qhash, pname, sizeof(pname)))
				return;	//file name was invalid, panic.
			snprintf (lname, sizeof(lname), "%s%s", logicalpaths, pname+ptlen+1);
			snprintf (lname2, sizeof(lname), "%s%s", logicalpaths, pakpath+ptlen+1);

			//see if we already added it
			for (oldp = com_searchpaths; oldp; oldp = oldp->next)
			{
				if (strcmp(oldp->prefix, pakprefix?pakprefix:""))	//probably will only happen from typos, but should be correct.
					continue;
				if (!Q_strcasecmp(oldp->purepath, pakpath))
					break;
				if (!Q_strcasecmp(oldp->logicalpath, lname))
					break;
				if (!Q_strcasecmp(oldp->logicalpath, lname2))
					break;
			}
			if (!oldp)
			{
				//see if we can get an old archive handle from before whatever fs_restart
				handle = FS_GetOldPath(oldpaths, lname2, &keptflags);
				if (handle)
					snprintf (lname, sizeof(lname), "%s", lname2);
				else
				{
					handle = FS_GetOldPath(oldpaths, lname, &keptflags);

					//seems new, load it.
					if (!handle)
					{
						vfsfile_t *vfs = NULL;
						if (search)
						{
							if (search->handle->FindFile(search->handle, &loc, pakpath+ptlen+1, NULL))
							{
								vfs = search->handle->OpenVFS(search->handle, &loc, "rb");
								snprintf (lname, sizeof(lname), "%s", lname2);
							}
							else if (search->handle->FindFile(search->handle, &loc, pname+ptlen+1, NULL))
								vfs = search->handle->OpenVFS(search->handle, &loc, "rb");
						}
						else
						{
							vfs = FS_OpenVFS(pakpath, "rb", FS_ROOT);
							if (vfs)
								snprintf (lname, sizeof(lname), "%s", lname2);
							else
								vfs = FS_OpenVFS(pname, "rb", FS_ROOT);
						}

						if (vfs)
							handle = searchpathformats[fmt].OpenNew (vfs, lname, pakprefix?pakprefix:"");
						if (!handle && vfs)
							VFS_CLOSE(vfs);	//erk
					}
				}

				//insert it into our path lists.
				if (handle && qhash)
				{
					int truecrc = handle->GeneratePureCRC(handle, 0, false);
					if (truecrc != (int)strtoul(qhash, NULL, 0))
					{
						Con_Printf(CON_ERROR "File \"%s\" has hash %#x (required: %s). Please delete it or move it away\n", lname, truecrc, qhash);
						handle->ClosePath(handle);
						handle = NULL;
					}
				}
				if (handle)
					FS_AddPathHandle(oldpaths, pakpath, lname, handle, pakprefix, SPF_COPYPROTECTED|SPF_UNTRUSTED|keptflags, (unsigned int)-1);
			}
			return;
		}
	}
}

static void FS_AddManifestPackages(searchpath_t **oldpaths, const char *purepath, const char *logicalpaths, searchpath_t *search, unsigned int loadstuff)
{
	int				i;

	int ptlen, palen;
	ptlen = strlen(purepath);
	for (i = 0; i < sizeof(fs_manifest->package) / sizeof(fs_manifest->package[0]); i++)
	{
		char qhash[16];
		if (!fs_manifest->package[i].path)
			continue;

		palen = strlen(fs_manifest->package[i].path);
		if (palen > ptlen && (fs_manifest->package[i].path[ptlen] == '/' || fs_manifest->package[i].path[ptlen] == '\\' )&& !strncmp(purepath, fs_manifest->package[i].path, ptlen))
		{
			Q_snprintfz(qhash, sizeof(qhash), "%#x", fs_manifest->package[i].crc);
			FS_AddHashedPackage(oldpaths,purepath,logicalpaths,search,loadstuff, fs_manifest->package[i].path,fs_manifest->package[i].crcknown?qhash:NULL,fs_manifest->package[i].prefix);
		}
	}
}

static void FS_AddDownloadManifestPackages(searchpath_t **oldpaths, unsigned int loadstuff)//, const char *purepath, searchpath_t *search, const char *extension, searchpathfuncs_t *(QDECL *OpenNew)(vfsfile_t *file, const char *desc))
{
	char logicalroot[MAX_OSPATH];
	FS_NativePath("downloads/", FS_ROOT, logicalroot, sizeof(logicalroot));

	FS_AddManifestPackages(oldpaths, "downloads", logicalroot, NULL, loadstuff);
}

static void FS_AddDataFiles(searchpath_t **oldpaths, const char *purepath, const char *logicalpath, searchpath_t *search, unsigned int pflags, unsigned int loadstuff)
{
	//search is the parent
	int				i, j;
	searchpath_t	*existing;
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
	wp.parentdesc = logicalpaths;
	wp.puredesc = purepath;
	wp.oldpaths = oldpaths;
	wp.inheritflags = pflags;

	//read pak.lst to get some sort of official ordering of pak files
	if (search->handle->FindFile(search->handle, &loc, "pak.lst", NULL) == FF_FOUND)
	{
		char filename[MAX_QPATH];
		char *buffer = BZ_Malloc(loc.len+1);
		char *names = buffer;
		search->handle->ReadFile(search->handle, &loc, buffer);
		buffer[loc.len] = 0;
		
		while (names && *names)
		{
			names = COM_ParseOut(names, filename, sizeof(filename));
			if (*filename)
			{
				char extension[MAX_QPATH];
				COM_FileExtension(filename, extension, sizeof(extension));

				//I dislike that this is tied to extensions, but whatever.
				for (j = 0; j < sizeof(searchpathformats)/sizeof(searchpathformats[0]); j++)
				{
					if (!searchpathformats[j].extension || !searchpathformats[j].OpenNew || !searchpathformats[j].loadscan)
						continue;
					if (!stricmp(extension, searchpathformats[j].extension))
					{
						if (loadstuff & (1<<j))
						{
							wp.OpenNew = searchpathformats[j].OpenNew;
							FS_AddWildDataFiles(filename, 0, 0, &wp, search->handle);
						}
						break;
					}
				}
			}
		}
		BZ_Free(buffer);
	}

	PM_LoadPackages(oldpaths, purepath, logicalpaths, search, loadstuff, 0x80000000, -1);

	for (j = 0; j < sizeof(searchpathformats)/sizeof(searchpathformats[0]); j++)
	{
		if (!searchpathformats[j].extension || !searchpathformats[j].OpenNew || !searchpathformats[j].loadscan)
			continue;
		if (loadstuff & (1<<j))
		{
			const char *extension = searchpathformats[j].extension;

			//first load all the numbered pak files
			for (i=0 ; ; i++)
			{
				snprintf (pakfile, sizeof(pakfile), "pak%i.%s", i, extension);
				fs_finds++;
				if (!search->handle->FindFile(search->handle, &loc, pakfile, NULL))
					break;	//not found..

				snprintf (pakfile, sizeof(pakfile), "%spak%i.%s", logicalpaths, i, extension);
				snprintf (purefile, sizeof(purefile), "%s/pak%i.%s", purepath, i, extension);

				for (existing = com_searchpaths; existing; existing = existing->next)
				{
					if (!Q_strcasecmp(existing->logicalpath, pakfile))	//assumption: first member of structure is a char array
						break; //already loaded (base paths?)
				}
				if (!existing)
				{
					handle = FS_GetOldPath(oldpaths, pakfile, &keptflags);
					if (!handle)
					{
						vfs = search->handle->OpenVFS(search->handle, &loc, "rb");
						if (!vfs)
							break;
						handle = searchpathformats[j].OpenNew (vfs, pakfile, "");
						if (!handle)
							break;
					}
					FS_AddPathHandle(oldpaths, purefile, pakfile, handle, "", SPF_COPYPROTECTED|pflags|keptflags, (unsigned int)-1);
				}
			}
		}
	}

	//now load ones from the manifest
	FS_AddManifestPackages(oldpaths, purepath, logicalpaths, search, loadstuff);

	PM_LoadPackages(oldpaths, purepath, logicalpaths, search, loadstuff, 0x0, 1000-1);

	//now load the random ones
	for (j = 0; j < sizeof(searchpathformats)/sizeof(searchpathformats[0]); j++)
	{
		if (!searchpathformats[j].extension || !searchpathformats[j].OpenNew || !searchpathformats[j].loadscan)
			continue;
		if (loadstuff & (1<<j))
		{
			const char *extension = searchpathformats[j].extension;
			wp.OpenNew = searchpathformats[j].OpenNew;

			Q_snprintfz (pakfile, sizeof(pakfile), "*.%s", extension);
			search->handle->EnumerateFiles(search->handle, pakfile, FS_AddWildDataFiles, &wp);
		}
	}

	PM_LoadPackages(oldpaths, purepath, logicalpaths, search, loadstuff, 1000, 0x7fffffff);
}

static searchpath_t *FS_AddPathHandle(searchpath_t **oldpaths, const char *purepath, const char *logicalpath, searchpathfuncs_t *handle, const char *prefix, unsigned int flags, unsigned int loadstuff)
{
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
	if (prefix)
		Q_strncpyz(search->prefix, prefix, sizeof(search->prefix));

	flags &= ~SPF_WRITABLE;

	//temp packages also do not nest
//	if (!(flags & SPF_TEMPORARY))
		FS_AddDataFiles(oldpaths, purepath, logicalpath, search, flags&(SPF_COPYPROTECTED|SPF_UNTRUSTED|SPF_TEMPORARY|SPF_PRIVATE), loadstuff);

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

static void COM_RefreshFSCache_f(void)
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
		{
			search->flags |= flags & SPF_WRITABLE;
			return; //already loaded (base paths?)
		}
	}

	if (!(flags & SPF_PRIVATE))
	{
		if ((p = strrchr(dir, '/')) != NULL)
			Q_strncpyz(pubgamedirfile, ++p, sizeof(pubgamedirfile));
		else
			Q_strncpyz(pubgamedirfile, dir, sizeof(pubgamedirfile));
	}
	if ((p = strrchr(dir, '/')) != NULL)
		Q_strncpyz(gamedirfile, ++p, sizeof(gamedirfile));
	else
		Q_strncpyz(gamedirfile, dir, sizeof(gamedirfile));

//
// add the directory to the search path
//
	handle = FS_GetOldPath(oldpaths, dir, &keptflags);
	if (!handle)
		handle = VFSOS_OpenPath(NULL, dir, "");

	FS_AddPathHandle(oldpaths, puredir, dir, handle, "", flags|keptflags, loadstuff);
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
		return va("%s-manifest %s -basedir %s", homearg, fs_manifest->updatefile, com_gamepath);
	
	return va("%s-game %s -basedir %s", homearg, pubgamedirfile, com_gamepath);
}
int FS_GetManifestArgv(char **argv, int maxargs)
{
	int c = 0;
	if (maxargs < 5)
		return c;
	argv[c++] = com_homepathenabled?"-usehome ":"-nohome ";
	if (fs_manifest->updatefile)
	{
		argv[c++] = "-manifest";
		argv[c++] = fs_manifest->updatefile;
	}
	else
	{
		argv[c++] = "-game";
		argv[c++] = pubgamedirfile;
	}
	argv[c++] = "-basedir";
	argv[c++] = com_gamepath;
	return c;
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

qboolean FS_PathURLCache(const char *url, char *path, size_t pathsize)
{
	const char *FS_GetCleanPath(const char *pattern, char *outbuf, int outlen);
	char tmp[MAX_QPATH];
	char *o = tmp;
	const char *i = url;
	strcpy(o, "downloads/");
	o += strlen(o);
	while(*i)
	{
		if (*i == ':' || *i == '?' || *i == '*' || *i == '&')
		{
			if (i[0] == ':' && i[1] == '/' && i[2] == '/')
			{
				i+=2;
				continue;
			}
			*o++ = '_';
			i++;
			continue;
		}
		if (*i == '\\')
		{
			*o++ = '/';
			i++;
			continue;
		}
		*o++ = *i++;
	}
	*o = 0;

	if (!FS_GetCleanPath(tmp, path, pathsize))
		return false;

	return true;
}

/*
================
COM_Gamedir

Sets the gamedir and path to a different directory.
================
*/
void COM_Gamedir (const char *dir, const struct gamepacks *packagespaths)
{
	ftemanifest_t *man;
	if (!fs_manifest)
		FS_ChangeGame(NULL, true, false);

	//don't allow leading dots, hidden files are evil.
	//don't allow complex paths. those are evil too.
	if (*dir == '.' || !strcmp(dir, ".") || strstr(dir, "..") || strstr(dir, "/")
		|| strstr(dir, "\\") || strstr(dir, ":") || strstr(dir, "\"") )
	{
		Con_Printf ("Gamedir should be a single filename, not \"%s\"\n", dir);
		return;
	}

	man = NULL;
	if (!man)
	{
		vfsfile_t *f = *dir?VFSOS_Open(va("%s%s.fmf", com_gamepath, dir), "rb"):NULL;
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
			char token[MAX_QPATH], quot[MAX_QPATH];
			char *dup = Z_StrDup(dir);	//FIXME: is this really needed?
			dir = dup;
			while ((dir = COM_ParseStringSet(dir, token, sizeof(token))))
			{
				if (!strcmp(dir, ";"))
					continue;
				if (!*token)
					continue;

				Cmd_TokenizeString(va("gamedir %s", COM_QuotedString(token, quot, sizeof(quot), false)), false, false);
				FS_Manifest_ParseTokens(man);
			}
			Z_Free(dup);
		}
		while(packagespaths && packagespaths->path)
		{
			char quot[MAX_QPATH];
			char quot2[MAX_OSPATH];
			char quot3[MAX_OSPATH];
			Cmd_TokenizeString(va("package %s prefix %s %s", COM_QuotedString(packagespaths->path, quot, sizeof(quot), false), COM_QuotedString(packagespaths->subpath?packagespaths->subpath:"", quot3, sizeof(quot3), false), COM_QuotedString(packagespaths->url, quot2, sizeof(quot2), false)), false, false);
			FS_Manifest_ParseTokens(man);
			packagespaths++;
		}
	}
	FS_ChangeGame(man, cfg_reload_on_gamedir.ival, false);
}

#if defined(NOLEGACY) || defined(SERVERONLY)
	#define ZFIXHACK
#elif defined(ANDROID) //on android, these numbers seem to be generating major weirdness, so disable these.
	#define ZFIXHACK
#elif defined(FTE_TARGET_WEB) //on firefox (but not chrome or ie), these numbers seem to be generating major weirdness, so tone them down significantly by default.
	#define ZFIXHACK "set r_polygonoffset_submodel_offset 1\nset r_polygonoffset_submodel_factor 0.05\n"
#else	//many quake maps have hideous z-fighting. this provides a way to work around it, although the exact numbers are gpu and bitdepth dependant, and trying to fix it can actually break other things.
	#define ZFIXHACK "set r_polygonoffset_submodel_offset 25\nset r_polygonoffset_submodel_factor 0.05\n"
#endif

/*quake requires a few settings for compatibility*/
#define EZQUAKECOMPETITIVE "set ruleset_allow_fbmodels 1\n"
#define QCFG "set com_parseutf8 0\nset allow_download_refpackages 0\nset sv_bigcoords \"\"\nmap_autoopenportals 1\n"  "sv_port "STRINGIFY(PORT_QWSERVER)" "STRINGIFY(PORT_NQSERVER)"\n" ZFIXHACK EZQUAKECOMPETITIVE
//nehahra has to be weird with extra cvars, and buggy fullbrights.
#define NEHCFG QCFG "set nospr32 0\nset cutscene 1\nalias startmap_sp \"map nehstart\"\nr_fb_bmodels 0\nr_fb_models 0\n"
/*stuff that makes dp-only mods work a bit better*/
#define DPCOMPAT QCFG "gl_specular 1\nset _cl_playermodel \"\"\n set dpcompat_set 1\ndpcompat_console 1\nset dpcompat_corruptglobals 1\nset vid_pixelheight 1\n"
/*nexuiz/xonotic has a few quirks/annoyances...*/
#define NEXCFG DPCOMPAT "cl_nopred 1\ncl_loopbackprotocol dpp7\nset sv_listen_dp 1\nset sv_listen_qw 0\nset sv_listen_nq 0\nset dpcompat_nopreparse 1\nset r_particlesdesc effectinfo\nset sv_bigcoords 1\nset sv_maxairspeed \"400\"\nset sv_jumpvelocity 270\nset sv_mintic \"0.01\"\ncl_nolerp 0\npr_enable_uriget 0\n"
#define XONCFG NEXCFG "set qport $qport_\ncom_parseutf8 1\npr_fixbrokenqccarrays 2\n"
/*some modern non-compat settings*/
#define DMFCFG "set com_parseutf8 1\npm_airstep 1\nsv_demoExtensions 1\n"
/*set some stuff so our regular qw client appears more like hexen2. sv_mintic is required to 'fix' the ravenstaff so that its projectiles don't impact upon each other*/
#define HEX2CFG "set com_parseutf8 -1\nset gl_font gfx/hexen2\nset in_builtinkeymap 0\nset_calc cl_playerclass int (random * 5) + 1\nset sv_maxspeed 640\ncl_run 0\nset watervis 1\nset r_lavaalpha 1\nset r_lavastyle -2\nset r_wateralpha 0.5\nset sv_pupglow 1\ngl_shaftlight 0.5\nsv_mintic 0.015\nset mod_warnmodels 0\nset cl_model_bobbing 1\nsv_sound_watersplash \"misc/hith2o.wav\"\nsv_sound_land \"fx/thngland.wav\"\n"
/*yay q2!*/
#define Q2CFG "set com_parseutf8 0\ncom_nogamedirnativecode 0\nset sv_bigcoords 0\n"
/*Q3's ui doesn't like empty model/headmodel/handicap cvars, even if the gamecode copes*/
#define Q3CFG "set com_parseutf8 0\ngl_overbright 2\nseta model sarge\nseta headmodel sarge\nseta handicap 100\ncom_nogamedirnativecode 0\n"
//#define RMQCFG "sv_bigcoords 1\n"

typedef struct {
	const char *argname;	//used if this was used as a parameter.
	const char *exename;	//used if the exe name contains this
	const char *protocolname;	//sent to the master server when this is the current gamemode (Typically set for DP compat).
	const char *auniquefile[4];	//used if this file is relative from the gamedir. needs just one file

	const char *customexec;

	const char *dir[4];
	const char *poshname;	//Full name for the game.
	const char *downloadsurl;
	const char *manifestfile;
} gamemode_info_t;
const gamemode_info_t gamemode_info[] = {
#ifdef GAME_SHORTNAME
	#ifndef GAME_PROTOCOL
	#define GAME_PROTOCOL			DISTRIBUTION
	#endif
	#ifndef GAME_IDENTIFYINGFILES
	#define GAME_IDENTIFYINGFILES	NULL	//
	#endif
	#ifndef GAME_DEFAULTCMDS
	#define GAME_DEFAULTCMDS		NULL	//doesn't need anything
	#endif
	#ifndef GAME_BASEGAMES
	#define GAME_BASEGAMES			"data"
	#endif
	#ifndef GAME_FULLNAME
	#define GAME_FULLNAME			FULLENGINENAME
	#endif
	#ifndef GAME_MANIFESTUPDATE
	#define GAME_MANIFESTUPDATE		NULL
	#endif

	{"-"GAME_SHORTNAME,		GAME_SHORTNAME,			GAME_PROTOCOL,					{GAME_IDENTIFYINGFILES}, GAME_DEFAULTCMDS, {GAME_BASEGAMES}, GAME_FULLNAME, GAME_MANIFESTUPDATE},
#endif
//note that there is no basic 'fte' gamemode, this is because we aim for network compatability. Darkplaces-Quake is the closest we get.
//this is to avoid having too many gamemodes anyway.

//mission packs should generally come after the main game to avoid prefering the main game. we violate this for hexen2 as the mission pack is mostly a superset.
//whereas the quake mission packs replace start.bsp making the original episodes unreachable.
//for quake, we also allow extracting all files from paks. some people think it loads faster that way or something.
#ifndef NOLEGACY
	//cmdline switch exename    protocol name(dpmaster)  identifying file				exec     dir1       dir2    dir3       dir(fte)     full name
	{"-quake",		"q1",		"FTE-Quake DarkPlaces-Quake",	{"id1/pak0.pak", "id1/quake.rc"},QCFG,	{"id1",		"qw",				"*fte"},		"Quake", "https://fte.triptohell.info/downloadables.php" /*,"id1/pak0.pak|http://quakeservers.nquake.com/qsw106.zip|http://nquake.localghost.net/qsw106.zip|http://qw.quakephil.com/nquake/qsw106.zip|http://fnu.nquake.com/qsw106.zip"*/},
	//quake's mission packs should not be favoured over the base game nor autodetected
	//third part mods also tend to depend upon the mission packs for their huds, even if they don't use any other content.
	//and q2 also has a rogue/pak0.pak file that we don't want to find and cause quake2 to look like dissolution of eternity
	//so just make these require the same files as good ol' quake.
	{"-hipnotic",	"hipnotic",	"FTE-Hipnotic",{"id1/pak0.pak","id1/quake.rc"},QCFG,	{"id1",		"qw",	"hipnotic",	"*fte"},		"Quake: Scourge of Armagon"},
	{"-rogue",		"rogue",	"FTE-Rogue",	{"id1/pak0.pak","id1/quake.rc"},QCFG,	{"id1",		"qw",	"rogue",	"*fte"},		"Quake: Dissolution of Eternity"},

	//various quake-dependant non-standalone mods that require hacks
	//quoth needed an extra arg just to enable hipnotic hud drawing, it doesn't actually do anything weird, but most engines have a -quoth arg, so lets have one too.
	{"-quoth",		"quoth",	"FTE-Quake",	{"id1/pak0.pak","id1/quake.rc"},QCFG,	{"id1",		"qw",	"quoth",	"*fte"},		"Quake: Quoth"},
	{"-nehahra",	"nehahra",	"FTE-Quake",	{"id1/pak0.pak","id1/quake.rc"},NEHCFG,	{"id1",		"qw",	"nehahra",	"*fte"},		"Quake: Seal Of Nehahra"},
	//various quake-based standalone mods.
	{"-nexuiz",		"nexuiz",	"Nexuiz",				{"nexuiz.exe"},					NEXCFG,	{"data",						"*ftedata"},	"Nexuiz"},
	{"-xonotic",	"xonotic",	"Xonotic",				{"xonotic.exe"},				XONCFG,	{"data",						"*ftedata"},	"Xonotic"},
//	{"-spark",		"spark",	"Spark",				{"base/src/progs.src",
//														 "base/qwprogs.dat",
//														 "base/pak0.pak"},				DMFCFG,	{"base",						         },	"Spark"},
//	{"-scouts",		"scouts",	"FTE-SJ",				{"basesj/src/progs.src",
//														 "basesj/progs.dat",
//														 "basesj/pak0.pak"},			NULL,	{"basesj",						         },	"Scouts Journey"},
//	{"-rmq",		"rmq",		"RMQ",					{NULL},							RMQCFG,	{"id1",		"qw",	"rmq",		"*fte"},		"Remake Quake"},

#ifdef HEXEN2
	//hexen2's mission pack generally takes precedence if both are installed.
	{"-portals",	"h2mp",		"FTE-H2MP",				{"portals/hexen.rc",
														 "portals/pak3.pak"},			HEX2CFG,{"data1",	"portals",			"*fteh2"},	"Hexen II MP"},
	{"-hexen2",		"hexen2",	"FTE-Hexen2",			{"data1/pak0.pak"},				HEX2CFG,{"data1",						"*fteh2"},	"Hexen II"},
#endif
#if defined(Q2CLIENT) || defined(Q2SERVER)
	{"-quake2",		"q2",		"FTE-Quake2",			{"baseq2/pak0.pak"},			Q2CFG,	{"baseq2",						"*fteq2"},	"Quake II"},
	//mods of the above that should generally work.
	{"-dday",		"dday",		"FTE-Quake2",			{"dday/pak0.pak"},				Q2CFG,	{"baseq2",	"dday",				"*fteq2"},	"D-Day: Normandy"},
#endif

#if defined(Q3CLIENT) || defined(Q3SERVER)
	{"-quake3",		"q3",		"FTE-Quake3",			{"baseq3/pak0.pk3"},			Q3CFG,	{"baseq3",						"*fteq3"},	"Quake III Arena"},
	//the rest are not supported in any real way. maps-only mostly, if that
//	{"-quake4",		"q4",		"FTE-Quake4",			{"q4base/pak00.pk4"},			NULL,	{"q4base",						"*fteq4"},	"Quake 4"},
//	{"-et",			NULL,		"FTE-EnemyTerritory",	{"etmain/pak0.pk3"},			NULL,	{"etmain",						"*fteet"},	"Wolfenstein - Enemy Territory"},

//	{"-jk2",		"jk2",		"FTE-JK2",				{"base/assets0.pk3"},			NULL,	{"base",						"*ftejk2"},	"Jedi Knight II: Jedi Outcast"},
//	{"-warsow",		"warsow",	"FTE-Warsow",			{"basewsw/pak0.pk3"},			NULL,	{"basewsw",						"*ftewsw"},	"Warsow"},
#endif
#if !defined(QUAKETC) && !defined(MINIMAL)
//	{"-doom",		"doom",		"FTE-Doom",				{"doom.wad"},					NULL,	{"*",							"*ftedoom"},	"Doom"},
//	{"-doom2",		"doom2",	"FTE-Doom2",			{"doom2.wad"},					NULL,	{"*",							"*ftedoom"},	"Doom2"},
//	{"-doom3",		"doom3",	"FTE-Doom3",			{"doom3.wad"},					NULL,	{"based3",						"*ftedoom3"},"Doom3"},

	//for the luls
//	{"-diablo2",	NULL,		"FTE-Diablo2",			{"d2music.mpq"},				NULL,	{"*",							"*fted2"},	"Diablo 2"},
#endif
#if defined(HLSERVER) || defined(HLCLIENT)
	//can run in windows, needs hl gamecode enabled. maps can always be viewed, but meh.
	{"-halflife",	"halflife",	"FTE-HalfLife",			{"valve/liblist.gam"},			NULL,	{"valve",						"*ftehl"},	"Half-Life"},
#endif
#endif

	{NULL}
};

void QDECL Q_strnlowercatz(char *d, const char *s, int n)
{
	int c = strlen(d);
	d += c;
	n -= c;
	n -= 1;	//for the null
	while (*s && n-- > 0)
	{
		if (*s >= 'A' && *s <= 'Z')
			*d = (*s-'A') + 'a';
		else
			*d = *s;
		d++;
		s++;
	}
	*d = 0;
}

qboolean FS_GenCachedPakName(const char *pname, const char *crc, char *local, int llen)
{
	const char *fn;
	char hex[16];
	if (strstr(pname, "dlcache"))
	{
		*local = 0;
		return false;
	}

	if (!strncmp(pname, "downloads/", 10))
	{
		*local = 0;
		Q_strnlowercatz(local, pname, llen);
		return true;
	}

	for (fn = pname; *fn; fn++)
	{
		if (*fn == '\\' || *fn == '/')
		{
			fn++;
			break;
		}
	}
//	fn = COM_SkipPath(pname);
	if (fn == pname)
	{	//only allow it if it has some game path first.
		*local = 0;
		return false;
	}
	Q_strncpyz(local, pname, min((fn - pname) + 1, llen));
	Q_strncatz(local, "dlcache/", llen);
	Q_strnlowercatz(local, fn, llen);
	if (crc && *crc)
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
	char *ext = "zip";//(pname);
	searchpathfuncs_t *handle;
	searchpath_t *oldlist = NULL;

	searchpath_t *sp;

	com_fschanged = true;

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
				com_fschanged = true;
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
//name is restored to its original state after the call, only technically not const
vfsfile_t *CL_OpenFileInPackage(searchpathfuncs_t *search, char *name)
{
	int found;
	vfsfile_t *f;
	flocation_t loc;
	char e, *n;
	char ext[8];
	char *end;
	int i;

	//keep chopping off the last part of the filename until we get an actual package
	//once we do, recurse into that package

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
				found = FS_FLocateFile(name, FSLF_IFFOUND, &loc); 
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
						found = FS_FLocateFile(name, FSLF_IFFOUND, &loc); 
					if (found)
					{
						f = (search?search:loc.search->handle)->OpenVFS(search?search:loc.search->handle, &loc, "rb");
						if (f)
						{
							searchpathfuncs_t *newsearch = searchpathformats[i].OpenNew(f, name, "");
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

//some annoying struct+func to prefix the enumerated file name properly.
struct CL_ListFilesInPackageCB_s
{
	char *nameprefix;
	size_t nameprefixlen;

	int (QDECL *func)(const char *fname, qofs_t fsize, time_t mtime, void *parm, searchpathfuncs_t *spath);
	void *parm;
	searchpathfuncs_t *spath;
};
static int QDECL CL_ListFilesInPackageCB(const char *fname, qofs_t fsize, time_t mtime, void *parm, searchpathfuncs_t *spath)
{
	struct CL_ListFilesInPackageCB_s *cb = parm;
	char name[MAX_OSPATH];
	if (cb->nameprefixlen)
	{
		memcpy(name, cb->nameprefix, cb->nameprefixlen-1);
		name[cb->nameprefixlen-1] = '/';
		Q_strncpyz(name+cb->nameprefixlen, fname, sizeof(name)-(cb->nameprefixlen));
		return cb->func(name, fsize, mtime, cb->parm, cb->spath);
	}
	else
		return cb->func(fname, fsize, mtime, cb->parm, cb->spath);
}

//'small' wrapper to list foo.zip/* to list files within zips that are not part of the gamedir.
//same rules as CL_OpenFileInPackage, except that wildcards should only be in the final part
qboolean CL_ListFilesInPackage(searchpathfuncs_t *search, char *name, int (QDECL *func)(const char *fname, qofs_t fsize, time_t mtime, void *parm, searchpathfuncs_t *spath), void *parm, void *recursioninfo)
{
	int found;
	vfsfile_t *f;
	flocation_t loc;
	char e, *n;
	char ext[8];
	char *end;
	int i;
	qboolean ret = false;
	struct CL_ListFilesInPackageCB_s cb;
	cb.nameprefix = recursioninfo?recursioninfo:name;
	cb.nameprefixlen = name-cb.nameprefix;
	cb.func = func;
	cb.parm = parm;

	//keep chopping off the last part of the filename until we get an actual package
	//once we do, recurse into that package

	end = name + strlen(name);

	while (end > name)
	{
		e = *end;
		*end = 0;

		COM_FileExtension(name, ext, sizeof(ext));
		for (i = 0; i < countof(searchpathformats); i++)
		{
			if (!searchpathformats[i].extension || !searchpathformats[i].OpenNew)
				continue;
			if (!strcmp(ext, searchpathformats[i].extension))
			{
				loc.search = NULL;
				if (search)
					found = search->FindFile(search, &loc, name, NULL);
				else
					found = FS_FLocateFile(name, FSLF_IFFOUND, &loc); 
				if (found)
				{
					f = (search?search:loc.search->handle)->OpenVFS(search?search:loc.search->handle, &loc, "rb");
					if (f)
					{
						searchpathfuncs_t *newsearch = searchpathformats[i].OpenNew(f, name, "");
						if (newsearch)
						{
							ret = CL_ListFilesInPackage(newsearch, end+1, func, parm, cb.nameprefix);
							newsearch->ClosePath(newsearch);
							if (ret)
							{
								*end = e;
								return ret;
							}
						}
						else
							VFS_CLOSE(f);
					}
				}
				break;
			}
		}

		n = COM_SkipPath(name);
		*end = e;
		end = n-1;
	}

	//always open the last file properly.
	loc.search = NULL;
	if (search)
		ret = search->EnumerateFiles(search, name, CL_ListFilesInPackageCB, &cb);
	else
	{
		ret = true;
		if (ret)
			COM_EnumerateFiles(name, CL_ListFilesInPackageCB, &cb);
	}
	return ret;
}

void FS_PureMode(int puremode, char *purenamelist, char *purecrclist, char *refnamelist, char *refcrclist, int pureseed)
{
	qboolean pureflush;

#ifndef CLIENTONLY
	//if we're the server, we can't be impure.
	if (sv.state)
		return;
#endif

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

	FS_ChangeGame(fs_manifest, false, false);

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

	if (FS_FLocateFile("vm/cgame.qvm", FSLF_IFFOUND, &loc))
	{
		Q_strncatz(buffer, va("%i ", loc.search->crc_reply), maxlen);
		basechecksum ^= loc.search->crc_reply;
	}
	else Q_strncatz(buffer, va("%i ", 0), maxlen);

	if (FS_FLocateFile("vm/ui.qvm", FSLF_IFFOUND, &loc))
	{
		Q_strncatz(buffer, va("%i ", loc.search->crc_reply), maxlen);
		basechecksum ^= loc.search->crc_reply;
	}
	else Q_strncatz(buffer, va("%i ", 0), maxlen);

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
		if (pak)	//logically should have SPF_EXPLICIT set, but that would give it a worse gamedir depth
			FS_AddPathHandle(&oldpaths, "", pakname, pak, "", SPF_COPYPROTECTED, reloadflags);
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
				Con_Printf ("Gamedir should be a single filename, not \"%s\"\n", dir);
				continue;
			}

			if (!Q_strncasecmp(dir, "downloads", 9))
			{
				Con_Printf ("Gamedir should not be \"%s\"\n", dir);
				continue;
			}

			//paths equal to '*' actually result in loading packages without an actual gamedir. note that this does not imply that we can write anything.
			if (!strcmp(dir, "*"))
			{
				searchpathfuncs_t *handle = VFSOS_OpenPath(NULL, com_gamepath, "");
				searchpath_t *search = (searchpath_t*)Z_Malloc (sizeof(searchpath_t));
				search->flags = 0;
				search->handle = handle;
				Q_strncpyz(search->purepath, "", sizeof(search->purepath));
				Q_strncpyz(search->logicalpath, com_gamepath, sizeof(search->logicalpath));

				FS_AddDataFiles(&oldpaths, search->purepath, search->logicalpath, search, SPF_EXPLICIT, reloadflags);

				handle->ClosePath(handle);
				Z_Free(search);
			}
			else if (*dir == '*')
			{
				//paths with a leading * are private, and not announced to clients that ask what the current gamedir is.
				Q_snprintfz(syspath, sizeof(syspath), "%s%s", com_gamepath, dir+1);
				FS_AddGameDirectory(&oldpaths, dir+1, syspath, reloadflags, SPF_EXPLICIT|SPF_PRIVATE|(com_homepathenabled?0:SPF_WRITABLE));
				if (com_homepathenabled)
				{
					Q_snprintfz(syspath, sizeof(syspath), "%s%s", com_homepath, dir+1);
					FS_AddGameDirectory(&oldpaths, dir+1, syspath, reloadflags, SPF_EXPLICIT|SPF_PRIVATE|SPF_WRITABLE);
				}
			}
			else
			{
				Q_snprintfz(syspath, sizeof(syspath), "%s%s", com_gamepath, dir);
				FS_AddGameDirectory(&oldpaths, dir, syspath, reloadflags, SPF_EXPLICIT|(com_homepathenabled?0:SPF_WRITABLE));
				if (com_homepathenabled)
				{
					Q_snprintfz(syspath, sizeof(syspath), "%s%s", com_homepath, dir);
					FS_AddGameDirectory(&oldpaths, dir, syspath, reloadflags, SPF_EXPLICIT|SPF_WRITABLE);
				}
			}
		}
	}

	//now mark the depth values
	if (com_searchpaths)
		for (next = com_searchpaths->next; next; next = next->next)
			next->flags |= SPF_BASEPATH;
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
				FS_AddGameDirectory(&oldpaths, dir, va("%s%s", com_gamepath, dir), reloadflags, SPF_EXPLICIT|(com_homepathenabled?0:SPF_WRITABLE));
				if (com_homepathenabled)
					FS_AddGameDirectory(&oldpaths, dir, va("%s%s", com_homepath, dir), reloadflags, SPF_EXPLICIT|SPF_WRITABLE);
			}
		}
	}

	FS_AddDownloadManifestPackages(&oldpaths, reloadflags);

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
						sp = FS_AddPathHandle(&oldpaths, pname, local, handle, "", SPF_COPYPROTECTED|SPF_UNTRUSTED|SPF_TEMPORARY|keptflags, (unsigned int)-1);
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
							handle = searchpathformats[i].OpenNew (vfs, local, "");
							if (!handle)
								break;
							sp = FS_AddPathHandle(&oldpaths, pname, local, handle, "", SPF_COPYPROTECTED|SPF_UNTRUSTED|SPF_TEMPORARY, (unsigned int)-1);

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

		Con_DPrintf("%s is no longer needed\n", oldpaths->logicalpath);
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
		FS_ReloadPackFilesFlags(0);
		Sys_UnlockMutex(fs_thread_mutex);
	}
}

void FS_ReloadPackFiles(void)
{
	//extra junk is to ensure the palette is reloaded if that changed.
	flocation_t paletteloc = {NULL};
	flocation_t paletteloc2 = {NULL};
	FS_FLocateFile("gfx/palette.lmp", 0, &paletteloc);

	if (Sys_LockMutex(fs_thread_mutex))
	{
		FS_ReloadPackFilesFlags(~0);
		Sys_UnlockMutex(fs_thread_mutex);
	}

	FS_FLocateFile("gfx/palette.lmp", 0, &paletteloc2);
	if (paletteloc.search != paletteloc2.search)
		Cbuf_AddText("vid_reload\n", RESTRICT_LOCAL);

}

void FS_ReloadPackFiles_f(void)
{
	if (Sys_LockMutex(fs_thread_mutex))
	{
		if (*Cmd_Argv(1))
			FS_ReloadPackFilesFlags(atoi(Cmd_Argv(1)));
		else
			FS_ReloadPackFilesFlags(~0);
		Sys_UnlockMutex(fs_thread_mutex);
	}
	if (host_initialized)
		FS_BeginManifestUpdates();
}

#if defined(_WIN32) && !defined(FTE_SDL) && !defined(WINRT) && !defined(_XBOX)
#include "winquake.h"
#ifdef MINGW
#define byte BYTE	//some versions of mingw headers are broken slightly. this lets it compile.
#endif
static qboolean Sys_SteamHasFile(char *basepath, int basepathlen, char *steamdir, char *fname)
{
	/*
	Find where Valve's Steam distribution platform is installed.
	Then take a look at that location for the relevent installed app.
	*/
	FILE *f;
	DWORD resultlen;
	HKEY key = NULL;
	
	if (RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Valve\\Steam", 0, STANDARD_RIGHTS_READ|KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
	{
		wchar_t suckysucksuck[MAX_OSPATH];
		resultlen = sizeof(suckysucksuck);
		RegQueryValueExW(key, L"SteamPath", NULL, NULL, (void*)suckysucksuck, &resultlen);
		RegCloseKey(key);
		narrowen(basepath, basepathlen, suckysucksuck);
		Q_strncatz(basepath, va("/SteamApps/common/%s", steamdir), basepathlen);
		if ((f = fopen(va("%s/%s", basepath, fname), "rb")))
		{
			fclose(f);
			return true;
		}
	}
	return false;
}

#ifndef SERVERONLY
static INT CALLBACK StupidBrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lp, LPARAM pData) 
{	//'stolen' from microsoft's knowledge base.
	//required to work around microsoft being annoying.
	wchar_t szDir[MAX_PATH];
	wchar_t *foo;
	switch(uMsg) 
	{
	case BFFM_INITIALIZED: 
		if (GetCurrentDirectoryW(sizeof(szDir)/sizeof(TCHAR), szDir))
		{
//			foo = strrchr(szDir, '\\');
//			if (foo)
//				*foo = 0;
//			foo = strrchr(szDir, '\\');
//			if (foo)
//				*foo = 0;
			SendMessageW(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)szDir);
		}
		break;
	case BFFM_VALIDATEFAILEDW:
		break;	//FIXME: validate that the gamedir contains what its meant to
	case BFFM_SELCHANGED: 
		if (SHGetPathFromIDListW((LPITEMIDLIST) lp, szDir))
		{
			wchar_t statustxt[MAX_OSPATH];
			while((foo = wcschr(szDir, '\\')))
				*foo = '/';
			if (pData)
				_snwprintf(statustxt, countof(statustxt), L"%s/%s", szDir, pData);
			else
				_snwprintf(statustxt, countof(statustxt), L"%s", szDir);
			statustxt[countof(statustxt)-1] = 0;	//ms really suck.
			SendMessageW(hwnd,BFFM_SETSTATUSTEXT,0,(LPARAM)statustxt);
		}
		break;
	}
	return 0;
}
int MessageBoxU(HWND hWnd, char *lpText, char *lpCaption, UINT uType);
#endif

qboolean Sys_DoDirectoryPrompt(char *basepath, size_t basepathsize, const char *poshname, const char *savedname)
{
#ifndef SERVERONLY
	wchar_t resultpath[MAX_OSPATH];
	wchar_t title[MAX_OSPATH];
	BROWSEINFOW bi;
	LPITEMIDLIST il;
	memset(&bi, 0, sizeof(bi));
	bi.hwndOwner = mainwindow; //note that this is usually still null
	bi.pidlRoot = NULL;
	GetCurrentDirectoryW(sizeof(resultpath)-1, resultpath);
	bi.pszDisplayName = resultpath;

	widen(resultpath, sizeof(resultpath), poshname);
	_snwprintf(title, countof(title), L"Please locate your existing %s installation", resultpath);

	//force mouse to deactivate, so that we can actually see it.
	INS_UpdateGrabs(false, false);

	bi.lpszTitle = title;

	bi.ulFlags = BIF_RETURNONLYFSDIRS|BIF_STATUSTEXT;
	bi.lpfn = StupidBrowseCallbackProc;
	bi.lParam = 0;//(LPARAM)poshname;
	bi.iImage = 0;

	il = SHBrowseForFolderW(&bi);
	if (il)
	{
		SHGetPathFromIDListW(il, resultpath);
		CoTaskMemFree(il);
		narrowen(basepath, basepathsize, resultpath);
		if (savedname)
		{
			if (MessageBoxU(mainwindow, va("Would you like to save the location of %s as:\n%s", poshname, basepath), "Save Instaltion path", MB_YESNO|MB_DEFBUTTON2) == IDYES)
				MyRegSetValue(HKEY_CURRENT_USER, "SOFTWARE\\" FULLENGINENAME "\\GamePaths", savedname, REG_SZ, basepath, strlen(basepath));
		}
		return true;
	}
#endif
	return false;
}

DWORD GetFileAttributesU(const char * lpFileName)
{
	wchar_t wide[MAX_OSPATH];
	widen(wide, sizeof(wide), lpFileName);
	return GetFileAttributesW(wide);
}
qboolean Sys_FindGameData(const char *poshname, const char *gamename, char *basepath, int basepathlen, qboolean allowprompts)
{
#ifndef INVALID_FILE_ATTRIBUTES
	#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif

	//first, try and find it in our game paths location
	if (MyRegGetStringValue(HKEY_CURRENT_USER, "SOFTWARE\\" FULLENGINENAME "\\GamePaths", gamename, basepath, basepathlen))
	{
		if (GetFileAttributesU(basepath) != INVALID_FILE_ATTRIBUTES)
			return true;
	}


	if (!strcmp(gamename, "quake"))
	{
		char *prefix[] =
		{
			"c:/quake/",						//quite a lot of people have it in c:\quake, as that's the default install location from the quake cd.
			"c:/games/quake/",					//personally I use this

			"c:/nquake/",						//nquake seems to have moved out of programfiles now. woo.
#ifdef _WIN64
			//quite a few people have nquake installed. FIXME: we need to an api function to read the directory for non-english-windows users.
			va("%s/nQuake/", getenv("%ProgramFiles(x86)%")),	//64bit builds should look in both places
			va("%s/nQuake/", getenv("%ProgramFiles%")),			//
#else
			va("%s/nQuake/", getenv("%ProgramFiles%")),			//32bit builds will get the x86 version anyway.
#endif
			NULL
		};
		int i;

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
			if (GetFileAttributesU(syspath) != INVALID_FILE_ATTRIBUTES)
			{
				Q_strncpyz(basepath, prefix[i], basepathlen);
				return true;
			}
			Q_snprintfz(syspath, sizeof(syspath), "%squake.exe", prefix[i]);
			if (GetFileAttributesU(syspath) != INVALID_FILE_ATTRIBUTES)
			{
				Q_strncpyz(basepath, prefix[i], basepathlen);
				return true;
			}
		}
	}

	if (!strcmp(gamename, "quake2"))
	{
		//look for HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\Quake2_exe\Path
		if (MyRegGetStringValue(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\Quake2_exe", "Path", basepath, basepathlen))
		{
			if (GetFileAttributesU(va("%s/quake2.exe", basepath)) != INVALID_FILE_ATTRIBUTES)
				return true;
		}

		if (Sys_SteamHasFile(basepath, basepathlen, "quake 2", "quake2.exe"))
			return true;
	}

	if (!strcmp(gamename, "et"))
	{
		//reads HKEY_LOCAL_MACHINE\SOFTWARE\Activision\Wolfenstein - Enemy Territory
		if (MyRegGetStringValue(HKEY_LOCAL_MACHINE, "SOFTWARE\\Activision\\Wolfenstein - Enemy Territory", "InstallPath", basepath, basepathlen))
		{
//			if (GetFileAttributesU(va("%s/ET.exe", basepath) != INVALID_FILE_ATTRIBUTES)
//				return true;
			return true;
		}
	}

	if (!strcmp(gamename, "quake3"))
	{
		//reads HKEY_LOCAL_MACHINE\SOFTWARE\id\Quake III Arena\InstallPath
		if (MyRegGetStringValue(HKEY_LOCAL_MACHINE, "SOFTWARE\\id\\Quake III Arena", "InstallPath", basepath, basepathlen))
		{
			if (GetFileAttributesU(va("%s/quake3.exe", basepath)) != INVALID_FILE_ATTRIBUTES)
				return true;
		}

		if (Sys_SteamHasFile(basepath, basepathlen, "quake 3 arena", "quake3.exe"))
			return true;
	}

	if (!strcmp(gamename, "wop"))
	{
		if (MyRegGetStringValue(HKEY_LOCAL_MACHINE, "SOFTWARE\\World Of Padman", "Path", basepath, basepathlen))
			return true;
	}

/*
	if (!strcmp(gamename, "d3"))
	{
		//reads HKEY_LOCAL_MACHINE\SOFTWARE\id\Doom 3\InstallPath
		if (MyRegGetStringValue(HKEY_LOCAL_MACHINE, L"SOFTWARE\\id\\Doom 3", "InstallPath", basepath, basepathlen))
			return true;
	}
*/

	if (!strcmp(gamename, "hexen2") || !strcmp(gamename, "h2mp"))
	{
		//append SteamApps\common\hexen 2
		if (Sys_SteamHasFile(basepath, basepathlen, "hexen 2", "glh2.exe"))
			return true;
	}

#if !defined(NPFTE) && !defined(SERVERONLY) //this is *really* unfortunate, but doing this crashes the browser
	if (allowprompts && poshname && *gamename && !COM_CheckParm("-manifest"))
	{
		if (Sys_DoDirectoryPrompt(basepath, basepathlen, poshname, gamename))
			return true;
	}
#endif

	return false;
}
#else
#if defined(__linux__) || defined(__unix__) || defined(__apple__)
#include <sys/stat.h>

static qboolean Sys_SteamHasFile(char *basepath, int basepathlen, char *steamdir, char *fname)
{
	/*
	Find where Valve's Steam distribution platform is installed.
	Then take a look at that location for the relevent installed app.
	*/
	FILE *f;
	DWORD resultlen;
	HKEY key = NULL;

	char *ev = getenv("HOME");
	if (ev && *ev)
	{
		Q_snprintfz(basepath, basepathlen, "%s/.steam/steam/SteamApps/common/%s", ev, steamdir);
		if ((f = fopen(va("%s/%s", basepath, fname), "rb")))
		{
			fclose(f);
			return true;
		}

		Q_snprintfz(basepath, basepathlen, "%s/.local/share/Steam/SteamApps/common/%s", ev, steamdir);
		if ((f = fopen(va("%s/%s", basepath, fname), "rb")))
		{
			fclose(f);
			return true;
		}

		/.local/share/Steam
	}
	return false;
}
#endif
qboolean Sys_FindGameData(const char *poshname, const char *gamename, char *basepath, int basepathlen, qboolean allowprompts)
{
#if defined(__linux__) || defined(__unix__) || defined(__apple__)
	struct stat sb;
	char *s;
	if (!*gamename)
		gamename = "quake";	//just a paranoia fallback, shouldn't be needed.
	if (!strcmp(gamename, "quake"))
	{
		if (Sys_SteamHasFile(basepath, basepathlen, "quake", "id1/pak0.pak"))
			return true;

		if (stat("/usr/share/quake/", &sb) == 0)
		{
			if (S_ISDIR(sb.st_mode))
			{
				Q_strncpyz(basepath, "/usr/share/quake/", basepathlen);
				return true;
			}
		}
	}
	else if (!strcmp(gamename, "quake2"))
	{
		if (Sys_SteamHasFile(basepath, basepathlen, "quake 2", "baseq2/pak0.pak"))
				return true;
	}
	else if (!strcmp(gamename, "hexen2") || !strcmp(gamename, "h2mp"))
	{
		if (Sys_SteamHasFile(basepath, basepathlen, "hexen 2", "data/pak0.pak"))
			return true;
	}

	s = va("/usr/share/games/%s/", gamename);
	if (stat(s, &sb) == 0)
	{
		if (S_ISDIR(sb.st_mode))
		{
			Q_strncpyz(basepath, s, basepathlen);
			return true;
		}
	}
	s = va("/usr/share/games/%s-demo/", gamename);
	if (stat(s, &sb) == 0)
	{
		if (S_ISDIR(sb.st_mode))
		{
			Q_strncpyz(basepath, s, basepathlen);
			return true;
		}
	}
#endif
	return false;
}
#define Sys_DoDirectoryPrompt(bp,bps,game,savename) false
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
	if (!fs_thread_mutex)
		return;

	PM_ManifestPackage(NULL, false);
	FS_FreePaths();
	Sys_DestroyMutex(fs_thread_mutex);
	fs_thread_mutex = NULL;

	Cvar_SetEngineDefault(&fs_gamename, NULL);
	Cvar_SetEngineDefault(&pm_downloads_url, NULL);
	Cvar_SetEngineDefault(&com_protocolname, NULL);
}

//returns false if the directory is not suitable.
//returns true if it contains a known package. if we don't actually know of any packages that it should have, we just have to assume that its okay.
static qboolean FS_DirHasAPackage(char *basedir, ftemanifest_t *man)
{
	qboolean defaultret = true;
	int j;
	vfsfile_t *f;

	f = VFSOS_Open(va("%sdefault.fmf", basedir), "rb");
	if (f)
	{
		VFS_CLOSE(f);
		return true;
	}

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
static qboolean FS_DirHasGame(const char *basedir, int gameidx)
{
	int j;
	vfsfile_t *f;

	//none listed, just assume its correct.
	if (!gamemode_info[gameidx].auniquefile[0])
		return true;

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
static int FS_IdentifyDefaultGameFromDir(const char *basedir)
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
//6: try the homedir, just in case.
//7: fallback to prompting. just returns -1 here.
//if autobasedir is not set, block gamedir changes/prompts.
static int FS_IdentifyDefaultGame(char *newbase, int sizeof_newbase, qboolean fixedbase)
{
	int i;
	int gamenum = -1;

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
	if (gamenum == -1 && *com_homepath && !fixedbase)
	{
		gamenum = FS_IdentifyDefaultGameFromDir(com_homepath);
		if (gamenum != -1)
			Q_strncpyz(newbase, com_homepath, sizeof_newbase);
	}
	return gamenum;
}

//allowed to modify newbasedir if fixedbasedir isn't set
static ftemanifest_t *FS_GenerateLegacyManifest(char *newbasedir, int sizeof_newbasedir, qboolean fixedbasedir, int game)
{
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
	}
	man->security = MANIFEST_SECURITY_INSTALLER;
	return man;
}

static void FS_AppendManifestGameArguments(ftemanifest_t *man)
{
	int i;

	if (!man)
		return;

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

#ifdef WEBCLIENT
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

static struct dl_download *curpackagedownload;
static enum manifestdeptype_e fspdl_type;
static enum {
	X_DLONLY,		//simple pk3 file
	X_COPY,			//we copy it from an existing install (ie: special install path for total conversion)
	X_MULTIUNZIP,	//zip with multiple files that need extracting
	X_UNZIP,		//pull a single file from a zip
	X_GZ,			//dlonly+ungzip
	X_XZ			//dlonly+unxzip
} fspdl_extracttype;
static char fspdl_internalname[MAX_QPATH];
static char fspdl_temppath[MAX_OSPATH];
static char fspdl_finalpath[MAX_OSPATH];
static void FS_BeginNextPackageDownload(void);
qboolean FS_DownloadingPackage(void)
{
	if (PM_IsApplying(false))
		return true;
	return !fs_manifest || !!curpackagedownload;
}
//vfsfile_t *FS_DecompressXZip(vfsfile_t *infile, vfsfile_t *outfile);
static void FS_ExtractPackage(searchpathfuncs_t *archive, flocation_t *loc, const char *origname, const char *finalname)
{
	vfsfile_t *in = archive->OpenVFS(archive, loc, "rb");
	if (in)
	{
		vfsfile_t *out = VFSOS_Open(finalname, "wb");
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
		else
			Con_Printf("Unable to write %s\n", finalname);
		VFS_CLOSE(in);
	}
	else
		Con_Printf("Unable to read %s\n", origname);
}
static int QDECL FS_ExtractAllPackages(const char *fname, qofs_t fsize, time_t mtime, void *parm, searchpathfuncs_t *spath)
{
	//okay, this package naming thing is getting stupid.
	flocation_t loc;
	int status = FF_NOTFOUND;
	status = spath->FindFile(spath, &loc, fname, NULL);
	//FIXME: symlinks?
	if (status == FF_FOUND)
		FS_ExtractPackage(spath, &loc, fname, va("%s%s", (const char*)parm, fname));
	return 1;
}
static void FS_PackagePrompt(char *finalpath, char *filename, char *game)
{
	static char existingbase[MAX_OSPATH];
	static char prevgame[64];
	vfsfile_t *in = NULL;
	const char *posh = NULL;
	int i;
	if (game)
	{
		for (i = 0; i < sizeof(gamemode_info) / sizeof(gamemode_info[0]); i++)
		{
			if (!Q_strcasecmp(gamemode_info[i].poshname, gamemode_info[i].argname+1))
			{
				posh = gamemode_info[i].poshname;
				break;
			}
		}
		if (*existingbase && !strcmp(prevgame, game))
		{
			in = VFSOS_Open(va("%s/%s", existingbase, filename), "rb");
			if (!in)
				in = VFSOS_Open(va("%s/%s", existingbase, COM_SkipPath(filename)), "rb");
		}
		Q_strncpyz(prevgame, game, sizeof(prevgame));
		if (!posh)
			posh = game;
		else if (!in && Sys_FindGameData(NULL, game, existingbase, sizeof(existingbase), false))
		{
			in = VFSOS_Open(va("%s/%s", existingbase, filename), "rb");
			if (!in)
				in = VFSOS_Open(va("%s/%s", existingbase, COM_SkipPath(filename)), "rb");
		}
	}
	while(!in && Sys_DoDirectoryPrompt(existingbase, sizeof(existingbase), posh, NULL))
	{
		in = VFSOS_Open(va("%s/%s", existingbase, filename), "rb");
		if (!in)
			in = VFSOS_Open(va("%s/%s", existingbase, COM_SkipPath(filename)), "rb");
	}

	if (in)
	{
		vfsfile_t *out = VFSOS_Open(finalpath, "wb");
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
		else
			Con_Printf("Unable to write %s\n", finalpath);
		VFS_CLOSE(in);
	}
	else
		Con_Printf("Unable to read %s%s\n", existingbase, filename);
}
static void FS_PackageDownloaded(struct dl_download *dl)
{
	curpackagedownload = NULL;

	if (dl->file)
	{
		VFS_CLOSE(dl->file);
		dl->file = NULL;
	}
	if (dl->status == DL_FAILED)
		Con_Printf("download for %s:%s failed\n", fspdl_finalpath, fspdl_internalname);

	if (dl->status == DL_FINISHED)
	{
		//rename the file as needed.
		COM_CreatePath(fspdl_finalpath);

		if (fspdl_extracttype == X_UNZIP || fspdl_extracttype == X_MULTIUNZIP)	//if zip...
		{	//archive
			searchpathfuncs_t *archive = FSZIP_LoadArchive(VFSOS_Open(fspdl_temppath, "rb"), dl->url, "");
			if (archive)
			{
				flocation_t loc;
				if (fspdl_extracttype == X_MULTIUNZIP)
				{
					char *f = fspdl_internalname;
					char *e;
					for (f = fspdl_internalname; *f; f = e)
					{
						e = strchr(f, ':');
						if (e)
							*e++ = 0;
						else
							e = f + strlen(f);

						if (strchr(f, '*'))
							archive->EnumerateFiles(archive, f, FS_ExtractAllPackages, fspdl_finalpath);
						else
						{
							int status = archive->FindFile(archive, &loc, f, NULL);
							if (status == FF_FOUND)
								FS_ExtractPackage(archive, &loc, f, va("%s%s", fspdl_finalpath, f));
						}
					}
				}
				else
				{
					flocation_t loc;
					int status = FF_NOTFOUND;
//FIXME: loop through all other packages to extract all of them as appropriate
					if (status == FF_NOTFOUND)
						status = archive->FindFile(archive, &loc, fspdl_internalname, NULL);
					if (status == FF_NOTFOUND)
						status = archive->FindFile(archive, &loc, COM_SkipPath(fspdl_internalname), NULL);

					if (status == FF_FOUND)
						FS_ExtractPackage(archive, &loc, fspdl_internalname, fspdl_finalpath);
					else
					{
						Con_Printf("Unable to find %s in %s\n", fspdl_internalname, fspdl_temppath);
					}
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
//			else
//				PM_AddDownloadedPackage(fspdl_finalpath);
		}
	}
	Sys_remove (fspdl_temppath);

	fs_restarts++;
	FS_ChangeGame(fs_manifest, true, false);

	FS_BeginNextPackageDownload();
}

static qboolean FS_BeginPackageDownload(struct manpack_s *pack, char *baseurl, qboolean allownoncache)
{
	char *crcstr = "";
	vfsfile_t *check;
	vfsfile_t *tmpf;

	char buffer[MAX_OSPATH], *url;
	qboolean multiex = false;

	//check this package's conditional
	if (pack->condition)
	{
		if (!If_EvaluateBoolean(pack->condition, RESTRICT_LOCAL))
			return false;
	}

	if (pack->type == mdt_installation)
	{	//libraries are in the root directory. we allow extracting multiple of them from a zip, etc.
		//they are not packages and thus do NOT support crcs.
		char *s, *e;
		if (!allownoncache)	//don't even THINK about allowing the download unless its part of an initial install.
			return false;
		for (s = pack->path; *s; s = e)
		{
			while(*s == ':')
				s++;
			e = strchr(s, ':');
			if (!e)
				e = s+strlen(s);
			if (e-s >= sizeof(buffer))
				continue;
			memcpy(buffer, s, e-s);
			buffer[e-s] = 0;

			check = FS_OpenVFS(buffer, "rb", FS_ROOT);
			if (check)
			{
				VFS_CLOSE(check);
				continue;
			}
			break;
		}
		if (!*s)	//all files were already present, apparently
			return false;
	}
	else
	{
		//check if we already have a version of the pak. if the user installed one, don't corrupt it with some unwanted pak. this may cause problems but whatever, user versitility wins.
		//this matches the rules for loading packs too. double is utterly pointless.
		check = FS_OpenVFS(pack->path, "rb", FS_ROOT);
		if (check)
		{
			VFS_CLOSE(check);
			return false;
		}

		//figure out what the cached name should be and see if we already have that or not
		if (pack->crcknown)
			crcstr = va("%#x", pack->crc);
		if (!pack->crcknown && allownoncache)
			Q_strncpyz(buffer, pack->path, sizeof(buffer));
		else if (!FS_GenCachedPakName(pack->path, crcstr, buffer, sizeof(buffer)))
			return false;
		check = FS_OpenVFS(buffer, "rb", FS_ROOT);
		if (check)
		{
			VFS_CLOSE(check);
			return false;
		}
	}

	if (pack->type == mdt_installation)
	{
		if (!strchr(pack->path, ':'))
		{
			if (!FS_NativePath(pack->path, FS_ROOT, fspdl_finalpath, sizeof(fspdl_finalpath)) ||
				!FS_NativePath(va("%s.tmp", pack->path), FS_ROOT, fspdl_temppath, sizeof(fspdl_temppath)))
				return false;
		}
		else
		{
			if (!FS_NativePath("", FS_ROOT, fspdl_finalpath, sizeof(fspdl_finalpath)) ||
				!FS_NativePath(va("%s.tmp", fs_manifest->installation), FS_ROOT, fspdl_temppath, sizeof(fspdl_temppath)))
				return false;
			multiex = true;
		}
	}
	else
	{
		//figure out a temp name and figure out where we're going to get it from.
		if (!FS_NativePath(buffer, FS_ROOT, fspdl_finalpath, sizeof(fspdl_finalpath)))
			return false;
		if (!pack->crcknown && allownoncache)
			Q_strncpyz(buffer, va("%s.tmp", pack->path), sizeof(buffer));
		else if (!FS_GenCachedPakName(va("%s.tmp", pack->path), crcstr, buffer, sizeof(buffer)))
			return false;
		if (!FS_NativePath(buffer, FS_ROOT, fspdl_temppath, sizeof(fspdl_temppath)))
			return false;
	}

	url = NULL;
	while(!url)
	{
		//ran out of mirrors?
		if (pack->mirrornum == (sizeof(pack->mirrors) / sizeof(pack->mirrors[0])))
			break;

		if (pack->mirrors[pack->mirrornum])
			url = FS_RelativeURL(baseurl, pack->mirrors[pack->mirrornum], buffer, sizeof(buffer));
		pack->mirrornum++;
	}
	//no valid mirrors
	if (!url)
		return false;

	fspdl_extracttype = X_DLONLY;
	if (!strncmp(url, "gz:", 3))
	{
		url+=3;
		fspdl_extracttype = X_GZ;
	}
	else if (!strncmp(url, "xz:", 3))
	{
		url+=3;
		fspdl_extracttype = X_XZ;
	}
	else if (!strncmp(url, "unzip:", 6))
	{
		url+=6;
		fspdl_extracttype = X_UNZIP;
	}
	else if (!strncmp(url, "prompt:", 7))
	{
		url+=7;
		fspdl_extracttype = X_COPY;
	}
	else
		fspdl_extracttype = X_DLONLY;

	if (fspdl_extracttype == X_UNZIP || fspdl_extracttype == X_COPY)
	{
		char *o = fspdl_internalname;
		while(o+1 < fspdl_internalname+sizeof(fspdl_internalname) && *url)
		{
			if (*url == ',')
			{
				url++;
				break;
			}
			*o++ = *url++;
		}
		*o = 0;
	}
	else
		*fspdl_internalname = 0;

	if (multiex)
	{
		if (fspdl_extracttype != X_UNZIP && fspdl_extracttype != X_DLONLY)
			return false;	//multiunzip is only supported with unzip urls... (or assumed if its a direct download
		fspdl_extracttype = X_MULTIUNZIP;

		if (!*fspdl_internalname)
			Q_strncpyz(fspdl_internalname, pack->path, sizeof(fspdl_internalname));
	}

	fspdl_type = pack->type;

	if (fspdl_extracttype == X_COPY)
	{
		FS_PackagePrompt(fspdl_finalpath, url, fspdl_internalname);
		return false;
	}

	COM_CreatePath(fspdl_temppath);
	tmpf = VFSOS_Open(fspdl_temppath, "wb");

	if (tmpf)
	{
		switch(fspdl_extracttype)
		{
		case X_XZ:
#ifdef AVAIL_XZDEC
			tmpf = FS_XZ_DecompressWriteFilter(tmpf);
#else
			VFS_CLOSE(tmpf);
			tmpf = NULL;
#endif
			break;
		case X_GZ:
#ifdef AVAIL_GZDEC
			tmpf = FS_GZ_WriteFilter(tmpf, true, false);
#else
			VFS_CLOSE(tmpf);
			tmpf = NULL;
#endif
			break;
		default:
			break;
		}

		if (!tmpf)
			Sys_remove (fspdl_temppath);
	}
	if (tmpf)
	{
		Con_Printf("Downloading %s from %s\n", fspdl_finalpath, url);
		curpackagedownload = HTTP_CL_Get(url, NULL, FS_PackageDownloaded);
		if (curpackagedownload)
		{
			curpackagedownload->file = tmpf;
#ifdef MULTITHREAD
			DL_CreateThread(curpackagedownload, NULL, NULL);
#endif
			return true;
		}
		VFS_CLOSE(tmpf);
		Sys_remove (fspdl_temppath);
	}
	return false;
}
static void FS_ManifestUpdated(struct dl_download *dl);
static void FS_BeginNextPackageDownload(void)
{
	int j;
	ftemanifest_t *man = fs_manifest;
	if (curpackagedownload || !man || com_installer)
		return;

	if (man->security != MANIFEST_SECURITY_NOT)
	{
		for (j = 0; j < sizeof(fs_manifest->package) / sizeof(fs_manifest->package[0]); j++)
		{
			if (man->package[j].type != mdt_installation)
				continue;

			if (FS_BeginPackageDownload(&man->package[j], man->updateurl, true))
				return;
		}
	}

	if (man->updateurl && !man->blockupdate)
	{
		man->blockupdate = true;
		Con_Printf("Updating manifest from %s\n", man->updateurl);
		waitingformanifest++;
		curpackagedownload = HTTP_CL_Get(man->updateurl, NULL, FS_ManifestUpdated);
		if (curpackagedownload)
		{
			curpackagedownload->user_ctx = man;
			return;
		}
	}

	for (j = 0; j < sizeof(fs_manifest->package) / sizeof(fs_manifest->package[0]); j++)
	{
		if (man->package[j].type != mdt_singlepackage)
			continue;

		if (FS_BeginPackageDownload(&man->package[j], man->updateurl, false))
			return;
	}
}
static void FS_ManifestUpdated(struct dl_download *dl)
{
	ftemanifest_t *man = fs_manifest;

	curpackagedownload = NULL;
	waitingformanifest--;

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
							FS_ChangeGame(man, true, false);
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
	PM_ManifestPackage(man->installupd, man->security);
	if (curpackagedownload || !man)
		return;

	if (!curpackagedownload)
		FS_BeginNextPackageDownload();
}
#else
qboolean FS_DownloadingPackage(void)
{
	return false;
}
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
	int i;
	int game = -1;
	ftemanifest_t *man = NULL;

	vfsfile_t *f;

	//commandline generally takes precedence
	if (!man && game == -1)
	{
		int i;
		for (i = 0; gamemode_info[i].argname; i++)
		{
			if (COM_CheckParm(gamemode_info[i].argname))
			{
				game = i;
				break;
			}
		}
	}

	//hopefully this will be used for TCs.
	if (!man && game == -1)
	{
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
				man->security = MANIFEST_SECURITY_DEFAULT;
				BZ_Free(fdata);
			}
			VFS_CLOSE(f);
		}
	}

	//-basepack is primarily an android feature
	i = COM_CheckParm ("-basepack");
	while (!man && game == -1 && i && i < com_argc-1)
	{
		const char *pakname = com_argv[i+1];
		searchpathfuncs_t *pak;
		vfsfile_t *vfs = VFSOS_Open(pakname, "rb");
		pak = FS_OpenPackByExtension(vfs, pakname);
		if (pak)
		{
			flocation_t loc;
			if (pak->FindFile(pak, &loc, "default.fmf", NULL))
			{
				f = pak->OpenVFS(pak, &loc, "rb");
				if (f)
				{
					size_t len = VFS_GETLEN(f);
					char *fdata = BZ_Malloc(len+1);
					if (fdata)
					{
						VFS_READ(f, fdata, len);
						fdata[len] = 0;
						man = FS_Manifest_Parse(NULL, fdata);
						man->security = MANIFEST_SECURITY_DEFAULT;
						BZ_Free(fdata);
					}
					VFS_CLOSE(f);
				}
			}
			pak->ClosePath(pak);
		}
		i = COM_CheckNextParm ("-basepack", i);
	}


	if (!man && game == -1 && host_parms.manifest)
	{
		man = FS_Manifest_Parse(va("%sdefault.fmf", newbasedir), host_parms.manifest);
		if (man)
			man->security = MANIFEST_SECURITY_INSTALLER;
	}

	if (!man)
	{
		if (game == -1)
			game = FS_IdentifyDefaultGame(newbasedir, newbasedirsize, fixedbasedir);
		if (game != -1)
			man = FS_GenerateLegacyManifest(newbasedir, newbasedirsize, fixedbasedir, game);
	}

	FS_AppendManifestGameArguments(man);
	return man;
}

qboolean FS_FixPath(char *path, size_t pathsize)
{
	size_t len = strlen(path);
	if (len)
	{
		if (path[len-1] == '/')
			return true;
#ifdef _WIN32
		if (path[len-1] == '\\')
			return true;
#endif
		if (len >= pathsize-1)
			return false;
		path[len] = '/';
		path[len+1] = 0;
	}
	return true;
}

//this is potentially unsafe. needs lots of testing.
qboolean FS_ChangeGame(ftemanifest_t *man, qboolean allowreloadconfigs, qboolean allowbasedirchange)
{
	int i, j;
	char realpath[MAX_OSPATH-1];
	char newbasedir[MAX_OSPATH];
	char *olddownloadsurl;
	qboolean fixedbasedir;
	qboolean reloadconfigs = false;
	qboolean builtingame = false;
	flocation_t loc;
	qboolean allowvidrestart = true;

	char *vidfile[] = {"gfx.wad", "gfx/conback.lmp",	//misc stuff
		"gfx/palette.lmp", "pics/colormap.pcx"};		//palettes
	searchpathfuncs_t *vidpath[countof(vidfile)];

	//if any of these files change location, the configs will be re-execed.
	//note that we reuse path handles if they're still valid, so we can just check the pointer to see if it got unloaded/replaced.
	char *conffile[] = {"quake.rc", "hexen.rc", "default.cfg", "server.cfg"};
	searchpathfuncs_t *confpath[countof(conffile)];

	for (i = 0; i < countof(vidfile); i++)
	{
		if (allowvidrestart)
		{
			FS_FLocateFile(vidfile[i], FSLF_IFFOUND, &loc);	//q1
			vidpath[i] = loc.search?loc.search->handle:NULL;
		}
		else
			vidpath[i] = NULL;
	}

	if (allowreloadconfigs && fs_noreexec.ival)
		allowreloadconfigs = false;
	for (i = 0; i < countof(conffile); i++)
	{
		if (allowreloadconfigs)
		{
			FS_FLocateFile(conffile[i], FSLF_IFFOUND, &loc);	//q1
			confpath[i] = loc.search?loc.search->handle:NULL;
		}
		else
			confpath[i] = NULL;
	}

#if defined(NACL) || defined(FTE_TARGET_WEB) || defined(ANDROID) || defined(WINRT)
	//these targets are considered to be sandboxed already, and have their own app-based base directory which they will always use.
	Q_strncpyz (newbasedir, host_parms.basedir, sizeof(newbasedir));
	fixedbasedir = true;
#else
	i = COM_CheckParm ("-basedir");
	fixedbasedir = i && i < com_argc-1;
	Q_strncpyz (newbasedir, fixedbasedir?com_argv[i+1]:host_parms.basedir, sizeof(newbasedir));
#endif

	//make sure it has a trailing slash, or is empty. woo.
	FS_CleanDir(newbasedir, sizeof(newbasedir));

	if (!allowreloadconfigs || !allowbasedirchange || (man && fs_manifest && !Q_strcasecmp(man->installation, fs_manifest->installation)))
	{
		fixedbasedir = true;
		Q_strncpyz (newbasedir, com_gamepath, sizeof(newbasedir));
	}

	if (!man)
	{
		//if we're already running a game, don't autodetect.
		if (fs_manifest)
			return false;

		man = FS_ReadDefaultManifest(newbasedir, sizeof(newbasedir), fixedbasedir);

		if (!man)
		{
			int found = FS_EnumerateKnownGames(FS_FoundManifest, &man);
			if (found != 1)
			{
				//we found more than 1 (or none)
				//if we're a client, display a menu to pick between them (or display an error)
				//servers can just use the first they find, they'd effectively just crash otherwise, but still give a warning.
				if (!isDedicated)
					man = NULL;
				else if (found)
					Con_Printf(CON_WARNING "Warning: found multiple possible games. Using the first found.\n");
				else
					Con_Printf(CON_ERROR "Error: unable to determine correct game/basedir.\n");
			}
		}
		if (!man)
		{
			man = FS_Manifest_Parse(NULL,
				"FTEManifestVer 1\n"
				"game \"\"\n"
				"name \"" FULLENGINENAME "\"\n"
				"defaultexec \\\"vid_fullscreen 0; gl_font cour;vid_width 640; vid_height 480\"\n"
				);
		}
		if (!man)
			Sys_Error("couldn't generate dataless manifest\n");
	}

	if (fs_manifest && fs_manifest->downloadsurl)
		olddownloadsurl = Z_StrDup(fs_manifest->downloadsurl);
	else if (!fs_manifest && man->downloadsurl)
		olddownloadsurl = Z_StrDup(man->downloadsurl);
	else
		olddownloadsurl = NULL;

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

	if (man->security == MANIFEST_SECURITY_NOT && strcmp(man->downloadsurl?man->downloadsurl:"", olddownloadsurl?olddownloadsurl:""))
	{	//make sure we only fuck over the user if this is a 'secure' manifest, and not hacked in some way.
		Z_Free(man->downloadsurl);
		man->downloadsurl = olddownloadsurl;
	}
	else
		Z_Free(olddownloadsurl);

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

				if (!man->downloadsurl && gamemode_info[i].downloadsurl)
				{
					Cmd_TokenizeString(va("downloadsurl \"%s\"", gamemode_info[i].downloadsurl), false, false);
					FS_Manifest_ParseTokens(man);
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
					if (Sys_FindGameData(man->formalname, man->installation, realpath, sizeof(realpath), man->security != MANIFEST_SECURITY_INSTALLER) && FS_FixPath(realpath, sizeof(realpath)) && FS_DirHasGame(realpath, i))
						Q_strncpyz (newbasedir, realpath, sizeof(newbasedir));
				break;
			}
		}
	}

	if (!fixedbasedir)
	{
		if (!builtingame && !fixedbasedir && !FS_DirHasAPackage(newbasedir, man))
		{
			if (Sys_FindGameData(man->formalname, man->installation, realpath, sizeof(realpath), man->security != MANIFEST_SECURITY_INSTALLER) && FS_FixPath(realpath, sizeof(realpath)) && FS_DirHasAPackage(realpath, man))
				Q_strncpyz (newbasedir, realpath, sizeof(newbasedir));
#ifndef SERVERONLY
			else
			{
				Z_Free(man->updatefile);
				man->updatefile = NULL;
				com_installer = true;
			}
#endif
		}
	}
	if (!fixedbasedir && !com_installer)
	{
		if (strcmp(com_gamepath, newbasedir))
		{
			PM_Shutdown();
			Q_strncpyz (com_gamepath, newbasedir, sizeof(com_gamepath));
		}
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
		//write a .nomedia file to avoid people from getting random explosion sounds etc interspersed with their music
		vfsfile_t *f;
		char nomedia[MAX_OSPATH];
		//figure out the path we're going to end up writing to
		if (com_homepathenabled)
			snprintf(nomedia, sizeof(nomedia), "%s%s", com_homepath, ".nomedia");
		else
			snprintf(nomedia, sizeof(nomedia), "%s%s", com_gamepath, ".nomedia");

		//make sure it exists.
		f = VFSOS_Open(nomedia, "rb");
		if (!f)	//don't truncate
		{
			COM_CreatePath(nomedia);
			f = VFSOS_Open(nomedia, "wb");
		}
		if (f)
			VFS_CLOSE(f);
	}
#endif

	if (Sys_LockMutex(fs_thread_mutex))
	{
		qboolean vidrestart = false;

		FS_ReloadPackFilesFlags(~0);

		Sys_UnlockMutex(fs_thread_mutex);

		FS_BeginManifestUpdates();

#ifdef WEBCLIENT
		if (curpackagedownload && fs_loadedcommand)
			allowreloadconfigs = false;
#endif

		COM_CheckRegistered();


		if (qrenderer != QR_NONE && allowvidrestart)
		{
			for (i = 0; i < countof(vidfile); i++)
			{
				FS_FLocateFile(vidfile[i], FSLF_IFFOUND, &loc);
				if (vidpath[i] != (loc.search?loc.search->handle:NULL))
				{
					vidrestart = true;
					Con_DPrintf("Restarting video because %s has changed\n", vidfile[i]);
				}
			}
		}

		if (allowreloadconfigs)
		{
			for (i = 0; i < countof(conffile); i++)
			{
				FS_FLocateFile(conffile[i], FSLF_IFFOUND, &loc);
				if (confpath[i] != (loc.search?loc.search->handle:NULL))
				{
					reloadconfigs = true;
					Con_DPrintf("Reloading configs because %s has changed\n", conffile[i]);
				}
			}

			if (reloadconfigs)
			{
				Cvar_SetEngineDefault(&fs_gamename, man->formalname?man->formalname:"FTE");
				Cvar_SetEngineDefault(&pm_downloads_url, man->downloadsurl?man->downloadsurl:"");
				Cvar_SetEngineDefault(&com_protocolname, man->protocolname?man->protocolname:"FTE");
				//FIXME: flag this instead and do it after a delay?
				Cvar_ForceSet(&fs_gamename, fs_gamename.enginevalue);
				Cvar_ForceSet(&pm_downloads_url, pm_downloads_url.enginevalue);
				Cvar_ForceSet(&com_protocolname, com_protocolname.enginevalue);
				vidrestart = false;

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
			else if (vidrestart)
			{
#ifndef SERVERONLY
				Cbuf_AddText ("vid_reload\n", RESTRICT_LOCAL);
#endif
				vidrestart = false;
			}
			if (fs_loadedcommand)
			{
				Cbuf_AddText(fs_loadedcommand, RESTRICT_INSECURE);
				Z_Free(fs_loadedcommand);
				fs_loadedcommand = NULL;
			}
		}
		if (vidrestart)
		{
#ifndef SERVERONLY
			Cbuf_AddText ("vid_reload\n", RESTRICT_LOCAL);
#endif
			vidrestart = false;
		}

		//rebuild the cache now, should be safe to waste some cycles on it
		COM_FlushFSCache(false, true);
	}

#ifndef SERVERONLY
	Validation_FlushFileList();	//prevent previous hacks from making a difference.
#endif

	{
		void (QDECL *callback)(struct cvar_s *var, char *oldvalue) = fs_game.callback;
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

void FS_CreateBasedir(const char *path)
{
	vfsfile_t *f;
	com_installer = false;
	Q_strncpyz (com_gamepath, path, sizeof(com_gamepath));
	COM_CreatePath(com_gamepath);
	fs_manifest->security = MANIFEST_SECURITY_INSTALLER;
	FS_ChangeGame(fs_manifest, true, false);

	if (host_parms.manifest)
	{
		f = FS_OpenVFS("default.fmf", "wb", FS_ROOT);
		if (f)
		{
			VFS_WRITE(f, host_parms.manifest, strlen(host_parms.manifest));
			VFS_CLOSE(f);
		}
	}
}

typedef struct
{
	int found;
	qboolean (*callback)(void *usr, ftemanifest_t *man);
	void *usr;
} fmfenums_t;
static int QDECL FS_EnumerateFMFs(const char *fname, qofs_t fsize, time_t mtime, void *inf, searchpathfuncs_t *spath)
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

int FS_EnumerateKnownGames(qboolean (*callback)(void *usr, ftemanifest_t *man), void *usr)
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
			if (gamemode_info[i].manifestfile || Sys_FindGameData(NULL, gamemode_info[i].argname+1, basedir, sizeof(basedir), true))
			{
				ftemanifest_t *man = FS_GenerateLegacyManifest(NULL, 0, true, i);
				if (e.callback(e.usr, man))
					e.found++;
				else
					FS_Manifest_Free(man);
			}
		}
	}
	return e.found;
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
							FS_ChangeGame(FS_GenerateLegacyManifest(NULL, 0, true, game), true, true);
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
		COM_Gamedir(filename, NULL);
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
				PM_Shutdown();
				FS_ChangeGame(FS_GenerateLegacyManifest(NULL, 0, true, i), true, true);
				return;
			}
		}

#ifndef SERVERONLY
		if (!Host_RunFile(arg, strlen(arg), NULL))
			Con_Printf("Game unknown\n");
#endif
	}
}

void FS_ChangeMod_f(void)
{
	char cachename[512];
	struct gamepacks packagespaths[16];
	int i;
	int packages = 0;
	const char *arg = "?";
	qboolean okay = false;

	if (Cmd_IsInsecure())
		return;

	Z_Free(fs_loadedcommand);
	fs_loadedcommand = NULL;

	memset(packagespaths, 0, sizeof(packagespaths));

	for (i = 1; ; )
	{
		if (i == Cmd_Argc())
		{
			okay = true;
			break;
		}
		arg = Cmd_Argv(i++);
		if (!strcmp(arg, "package"))
		{
			arg = Cmd_Argv(i++);
			if (packages == countof(packagespaths))	//must leave space for one, as a terminator.
				continue;
			if (FS_PathURLCache(arg, cachename, sizeof(cachename)))
			{
				packagespaths[packages].url = Z_StrDup(arg);
				packagespaths[packages].path = Z_StrDup(cachename);
				packages++;
			}
		}
		else if (!strcmp(arg, "prefix"))
		{
			if (!packages)
				break;
			arg = Cmd_Argv(i++);
			packagespaths[packages-1].subpath = Z_StrDup(arg);
		}
		else if (!strcmp(arg, "map"))
		{
			Z_Free(fs_loadedcommand);
			arg = va("map \"%s\"\n", Cmd_Argv(i++));
			fs_loadedcommand = Z_StrDup(arg);
		}
		else if (!strcmp(arg, "restart"))
		{
			Z_Free(fs_loadedcommand);
			fs_loadedcommand = Z_StrDup("restart\n");
		}
		else
			break;
	}

	if (okay)
		COM_Gamedir("", packagespaths);
	else
	{
		Con_Printf("unsupported args: %s\n", arg);
		Z_Free(fs_loadedcommand);
		fs_loadedcommand = NULL;
	}

	for (i = 0; i < packages; i++)
	{
		Z_Free(packagespaths[i].url);
		Z_Free(packagespaths[i].path);
		Z_Free(packagespaths[i].subpath);
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
	Cmd_AddCommandD("fs_changegame", FS_ChangeGame_f, "Switch between different manifests (or registered games)");
	Cmd_AddCommandD("fs_changemod", FS_ChangeMod_f, "Provides the backend functionality of a transient online installer. Eg, for quaddicted's map/mod database.");
	Cmd_AddCommand("fs_showmanifest", FS_ShowManifest_f);
	Cmd_AddCommand ("fs_flush", COM_RefreshFSCache_f);

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
	Cvar_Register(&pm_downloads_url, "Filesystem");
	Cvar_Register(&pm_autoupdate, "Filesystem");
	Cvar_Register(&com_protocolname, "Server Info");
	Cvar_Register(&fs_game, "Filesystem");
#ifdef Q2SERVER
	Cvar_Register(&fs_gamedir, "Filesystem");
	Cvar_Register(&fs_basedir, "Filesystem");
#endif

	usehome = false;

	//assume the home directory is the working directory.
	*com_homepath = '\0';

#if defined(_WIN32) && !defined(FTE_SDL) && !defined(WINRT) && !defined(_XBOX)
	{	//win32 sucks.
		HRESULT (WINAPI *dSHGetFolderPathW) (HWND hwndOwner, int nFolder, HANDLE hToken, DWORD dwFlags, wchar_t *pszPath) = NULL;
		dllfunction_t funcs[] =
		{
			{(void**)&dSHGetFolderPathW, "SHGetFolderPathW"},
			{NULL,NULL}
		};
		DWORD winver = (DWORD)LOBYTE(LOWORD(GetVersion()));
		/*HMODULE shfolder =*/ Sys_LoadLibrary("shfolder.dll", funcs);

		if (dSHGetFolderPathW)
		{
			wchar_t wfolder[MAX_PATH];
			char folder[MAX_OSPATH];
			// 0x5 == CSIDL_PERSONAL
			if (dSHGetFolderPathW(NULL, 0x5, NULL, 0, wfolder) == S_OK)
			{
				narrowen(folder, sizeof(folder), wfolder);
				Q_snprintfz(com_homepath, sizeof(com_homepath), "%s/My Games/%s/", folder, FULLENGINENAME);
			}
		}
//		if (shfolder)
//			FreeLibrary(shfolder);

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
			BOOL (WINAPI *dCheckTokenMembership) (HANDLE TokenHandle, PSID SidToCheck, PBOOL IsMember) = NULL;
			dllfunction_t funcs[] =
			{
				{(void**)&dCheckTokenMembership, "CheckTokenMembership"},
				{NULL,NULL}
			};
			advapi32 = Sys_LoadLibrary("advapi32.dll", funcs);
			if (advapi32)
			{
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

				Sys_CloseLibrary(advapi32);
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
}





//this is at the bottom of the file to ensure these globals are not used elsewhere
extern searchpathfuncs_t *(QDECL VFSOS_OpenPath) (vfsfile_t *file, const char *desc, const char *prefix);
#if 1//def AVAIL_ZLIB
extern searchpathfuncs_t *(QDECL FSZIP_LoadArchive) (vfsfile_t *packhandle, const char *desc, const char *prefix);
#endif
extern searchpathfuncs_t *(QDECL FSPAK_LoadArchive) (vfsfile_t *packhandle, const char *desc, const char *prefix);
#ifdef PACKAGE_DOOMWAD
extern searchpathfuncs_t *(QDECL FSDWD_LoadArchive) (vfsfile_t *packhandle, const char *desc, const char *prefix);
#endif
void FS_RegisterDefaultFileSystems(void)
{
#ifdef PACKAGE_DZIP
	FS_RegisterFileSystemType(NULL, "dz", FSDZ_LoadArchive, false);
#endif
#ifdef PACKAGE_Q1PAK
	FS_RegisterFileSystemType(NULL, "pak", FSPAK_LoadArchive, true);
#if !defined(_WIN32) && !defined(ANDROID)
	/*for systems that have case sensitive paths, also include *.PAK */
	FS_RegisterFileSystemType(NULL, "PAK", FSPAK_LoadArchive, true);
#endif
#endif
	FS_RegisterFileSystemType(NULL, "pk3dir", VFSOS_OpenPath, true);
#ifdef PACKAGE_PK3
	FS_RegisterFileSystemType(NULL, "pk3", FSZIP_LoadArchive, true);
	FS_RegisterFileSystemType(NULL, "pk4", FSZIP_LoadArchive, true);
	FS_RegisterFileSystemType(NULL, "apk", FSZIP_LoadArchive, false);
	FS_RegisterFileSystemType(NULL, "zip", FSZIP_LoadArchive, false);
#endif
#ifdef PACKAGE_DOOMWAD
	FS_RegisterFileSystemType(NULL, "wad", FSDWD_LoadArchive, true);
#endif
}
