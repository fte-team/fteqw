//copyright 'Spike', license gplv2+
//provides both a package manager and downloads menu.
//FIXME: block downloads of exe/dll/so/etc if not an https url (even if inside zips). also block such files from package lists over http.
#include "quakedef.h"

#ifdef PACKAGEMANAGER
	#if !defined(NOBUILTINMENUS) && !defined(SERVERONLY)
		#define DOWNLOADMENU
	#endif
#endif

#ifdef PACKAGEMANAGER
#include "fs.h"

//whole load of extra args for the downloads menu (for the downloads menu to handle engine updates).
#if defined(VKQUAKE) && !defined(SERVERONLY)
#define PHPVK "&vk=1"
#else
#define PHPVK
#endif
#if defined(GLQUAKE) && !defined(SERVERONLY)
#define PHPGL "&gl=1"
#else
#define PHPGL
#endif
#if defined(D3DQUAKE) && !defined(SERVERONLY)
#define PHPD3D "&d3d=1"
#else
#define PHPD3D
#endif
#ifdef MINIMAL
#define PHPMIN "&min=1"
#else
#define PHPMIN
#endif
#ifdef HAVE_LEGACY
#define PHPLEG "&leg=1&test=1"
#else
#define PHPLEG "&leg=0&test=1"
#endif
#if defined(_DEBUG) || defined(DEBUG)
#define PHPDBG "&dbg=1"
#else
#define PHPDBG
#endif
#ifndef SVNREVISION
#define SVNREVISION -
#endif
#define SVNREVISIONSTR STRINGIFY(SVNREVISION)
#define DOWNLOADABLESARGS "ver=" SVNREVISIONSTR PHPVK PHPGL PHPD3D PHPMIN PHPLEG PHPDBG "&arch="PLATFORM "_" ARCH_CPU_POSTFIX



extern cvar_t pm_autoupdate;
extern cvar_t pm_downloads_url;
#define INSTALLEDFILES	"installed.lst"	//the file that resides in the quakedir (saying what's installed).

//installed native okay [previously manually installed, or has no a qhash]
//installed cached okay [had a qhash]
//installed native corrupt [they overwrote it manually]
//installed cached corrupt [we fucked up, probably]
//installed native missing (becomes not installed) [deleted]
//installed cached missing (becomes not installed) [deleted]
//installed none [meta package with no files]

//!installed native okay [was manually installed, flag as installed now]
//!installed cached okay [they got it from some other source / previously installed]
//!installed native corrupt [manually installed conflict]
//!installed cached corrupt [we fucked up, probably]

//!installed * missing [simply not installed]

#define DPF_ENABLED					(1u<<0)
#define DPF_NATIVE					(1u<<1)	//appears to be installed properly
#define DPF_CACHED					(1u<<2)	//appears to be installed in their dlcache dir (and has a qhash)
#define DPF_CORRUPT					(1u<<3)	//will be deleted before it can be changed

#define DPF_USERMARKED				(1u<<4)	//user selected it
#define DPF_AUTOMARKED				(1u<<5)	//selected only to satisfy a dependancy
#define DPF_DISPLAYVERSION			(1u<<6)	//some sort of conflict, the package is listed twice, so show versions so the user knows what's old.
#define DPF_FORGETONUNINSTALL		(1u<<7)	//for previously installed packages, remove them from the list if there's no current version any more (should really be automatic if there's no known mirrors)
#define DPF_HIDDEN					(1u<<8)	//wrong arch, file conflicts, etc. still listed if actually installed.
#define DPF_PURGE					(1u<<9)	//package should be completely removed (ie: the dlcache dir too). if its still marked then it should be reinstalled anew. available on cached or corrupt packages, implied by native.
#define DPF_MANIFEST				(1u<<10)	//package was named by the manifest, and should only be uninstalled after a warning.
#define DPF_TESTING					(1u<<11)	//package is provided on a testing/trial basis, and will only be selected/listed if autoupdates are configured to allow it.

#define DPF_ENGINE					(1u<<12)	//engine update. replaces old autoupdate mechanism
#define DPF_PLUGIN					(1u<<13)	//this is a plugin package, with a dll

#define DPF_TRUSTED					(1u<<14)	//flag used when parsing package lists. if not set then packages will be ignored if they are anything but paks/pk3s

#define DPF_MARKED					(DPF_USERMARKED|DPF_AUTOMARKED)
#define DPF_PRESENT					(DPF_NATIVE|DPF_CACHED)
#define DPF_DISABLEDINSTALLED		(DPF_ENGINE|DPF_PLUGIN)	//engines+plugins can be installed without being enabled.
//pak.lst
//priories <0
//pakX
//manifest packages
//priority 0-999
//*.pak
//priority >=1000
#define PM_DEFAULTPRIORITY		1000

void CL_StartCinematicOrMenu(void);

#if defined(SERVERONLY)
#	define ENGINE_RENDERER "sv"
#elif defined(GLQUAKE) && (defined(VKQUAKE) || defined(D3DQUAKE) || defined(SWQUAKE))
#	define ENGINE_RENDERER "m"
#elif defined(GLQUAKE)
#	define ENGINE_RENDERER "gl"
#elif defined(VKQUAKE)
#	define ENGINE_RENDERER "vk"
#elif defined(D3DQUAKE)
#	define ENGINE_RENDERER "d3d"
#else
#	define ENGINE_RENDERER "none"
#endif
#if defined(NOCOMPAT)
#	define ENGINE_CLIENT "-nc"
#elif defined(MINIMAL)
#	define ENGINE_CLIENT "-min"
#elif defined(CLIENTONLY)
#	define ENGINE_CLIENT "-cl"
#else
#	define ENGINE_CLIENT
#endif

#define THISARCH PLATFORM "_" ARCH_CPU_POSTFIX
#define THISENGINE THISARCH "-" DISTRIBUTION "-" ENGINE_RENDERER ENGINE_CLIENT

typedef struct package_s {
	char *name;
	char *category;	//in path form

	struct package_s *alternative;	//alternative (hidden) forms of this package.

	char *mirror[8];	//FIXME: move to two types of dep...
	char gamedir[16];
	enum fs_relative fsroot;
	char version[16];
	char *arch;
	char *qhash;

	char *title;
	char *description;
	char *license;
	char *author;
	char *website;
	char *previewimage;
	enum
	{
		EXTRACT_COPY,	//just copy the download over
		EXTRACT_XZ,		//give the download code a write filter so that it automatically decompresses on the fly
		EXTRACT_GZ,		//give the download code a write filter so that it automatically decompresses on the fly
		EXTRACT_ZIP		//extract stuff once it completes. kinda sucky.
	} extract;

	struct packagedep_s
	{
		struct packagedep_s *next;
		enum
		{
			DEP_CONFLICT,
			DEP_FILECONFLICT,	//don't install if this file already exists.
			DEP_REQUIRE,
			DEP_RECOMMEND,	//like depend, but uninstalling will not bubble.
			DEP_SUGGEST,	//like recommend, but will not force install (ie: only prevents auto-uninstall)
//			DEP_MIRROR,
//			DEP_FAILEDMIRROR,

			DEP_FILE
		} dtype;
		char name[1];
	} *deps;

#ifdef WEBCLIENT
	struct dl_download *download;
	unsigned int trymirrors;
#endif

	int flags;
	int priority;
	struct package_s **link;
	struct package_s *next;
} package_t;

static qboolean loadedinstalled;
static package_t *availablepackages;
static int numpackages;
static char *manifestpackages;	//metapackage named by the manicfest.
static char *declinedpackages;	//metapackage named by the manicfest.
static int domanifestinstall;	//SECURITY_MANIFEST_*

#ifdef PLUGINS
static qboolean pluginpromptshown;	//so we only show prompts for new externally-installed plugins once, instead of every time the file is reloaded.
#endif
#ifdef WEBCLIENT
static qboolean doautoupdate;	//updates will be marked (but not applied without the user's actions)
static qboolean pkg_updating;	//when flagged, further changes are blocked until completion.
#else
static const qboolean pkg_updating = false;
#endif

//FIXME: these are allocated for the life of the exe. changing basedir should purge the list.
static int numdownloadablelists = 0;
static struct
{
	char *url;
	char *prefix;
	qboolean trustworthy;		//trusted 
	char received;				//says if we got a response yet or not
	qboolean save;				//written into our local file
	struct dl_download *curdl;	//the download context
} downloadablelist[32];
static int downloadablessequence;	//bumped any time any package is purged

static void PM_WriteInstalledPackages(void);

static void PM_FreePackage(package_t *p)
{
	struct packagedep_s *d;
	int i;

	if (p->link)
	{
		if (p->alternative)
		{	//replace it with its alternative package
			*p->link = p->alternative;
			p->alternative->alternative = p->alternative->next;
			if (p->alternative->alternative)
				p->alternative->alternative->link = &p->alternative->alternative;
			p->alternative->next = p->next;
			p->alternative->link = p->link;
		}
		else
		{	//just remove it from the list.
			*p->link = p->next;
			if (p->next)
				p->next->link = p->link;
		}
	}

	//free its data.
	while(p->deps)
	{
		d = p->deps;
		p->deps = d->next;
		Z_Free(d);
	}

	for (i = 0; i < countof(p->mirror); i++)
		Z_Free(p->mirror[i]);

	Z_Free(p->name);
	Z_Free(p->category);
	Z_Free(p->title);
	Z_Free(p->description);
	Z_Free(p->author);
	Z_Free(p->website);
	Z_Free(p->license);
	Z_Free(p->previewimage);
	Z_Free(p->qhash);
	Z_Free(p->arch);
	Z_Free(p);
}

static qboolean PM_PurgeOnDisable(package_t *p)
{
	//corrupt packages must be purged
	if (p->flags & DPF_CORRUPT)
		return true;
	//certain updates can be present and not enabled
	if (p->flags & DPF_DISABLEDINSTALLED)
		return false;
	//hashed packages can also be present and not enabled, but only if they're in the cache and not native
	if (*p->gamedir && p->qhash && (p->flags & DPF_PRESENT))
		return false;
	//FIXME: add basedir-plugins to the package manager so they can be enabled/disabled properly.
	//if (p->arch)
	//	return false;
	//all other packages must be deleted to disable them
	return true;
}

//checks the status of each package
static void PM_ValidatePackage(package_t *p)
{
	package_t *o;
	struct packagedep_s *dep;
	vfsfile_t *pf;
	p->flags &=~ (DPF_NATIVE|DPF_CACHED|DPF_CORRUPT);
	if (p->flags & DPF_ENABLED)
	{
		for (dep = p->deps; dep; dep = dep->next)
		{
			char *n;
			if (dep->dtype != DEP_FILE)
				continue;
			if (*p->gamedir)
				n = va("%s/%s", p->gamedir, dep->name);
			else
				n = dep->name;
			pf = FS_OpenVFS(n, "rb", p->fsroot);
			if (pf)
			{
				VFS_CLOSE(pf);
				p->flags |= DPF_NATIVE;
			}
			else if (*p->gamedir && p->qhash)
			{
				char temp[MAX_OSPATH];
				if (FS_GenCachedPakName(n, p->qhash, temp, sizeof(temp)))
				{
					pf = FS_OpenVFS(temp, "rb", p->fsroot);
					if (pf)
					{
						VFS_CLOSE(pf);
						p->flags |= DPF_CACHED;
					}
				}
			}
			if (!(p->flags & (DPF_NATIVE|DPF_CACHED)))
				Con_Printf("WARNING: %s (%s) no longer exists\n", p->name, n);
		}
	}
	else
	{
		for (dep = p->deps; dep; dep = dep->next)
		{
			char *n;
			struct packagedep_s *odep;
			unsigned int fl = DPF_NATIVE;
			if (dep->dtype != DEP_FILE)
				continue;
			if (*p->gamedir)
				n = va("%s/%s", p->gamedir, dep->name);
			else
				n = dep->name;
			pf = FS_OpenVFS(n, "rb", p->fsroot);
			if (!pf && *p->gamedir && p->qhash)
			{
				char temp[MAX_OSPATH];
				if (FS_GenCachedPakName(n, p->qhash, temp, sizeof(temp)))
				{
					pf = FS_OpenVFS(temp, "rb", p->fsroot);
					fl = DPF_CACHED;
				}
				//fixme: skip any archive checks
			}

			if (pf)
			{
				for (o = availablepackages; o; o = o->next)
				{
					if (o == p)
						continue;
					if (o->flags & DPF_ENABLED)
					{
						if (!strcmp(p->gamedir, o->gamedir) && p->fsroot == o->fsroot)
							if (strcmp(p->name, o->name) || strcmp(p->version, o->version))
							{
								for (odep = o->deps; odep; odep = odep->next)
								{
									if (!strcmp(dep->name, odep->name))
										break;
								}
								if (odep)
									break;
							}
					}
				}
				if ((o && o->qhash && p->qhash && (o->flags & DPF_CACHED)) || fl == DPF_CACHED)
					p->flags |= DPF_CACHED;
				else if (!o)
				{
					if (!PM_PurgeOnDisable(p))
					{
						p->flags |= fl;
						VFS_CLOSE(pf);
					}
					else if (p->qhash)
					{
						searchpathfuncs_t *archive;

#ifdef PACKAGE_Q1PAK
						char buf[8];
						if (!Q_strcasecmp(COM_FileExtension(n, buf, sizeof(buf)), "pak"))
							archive = FSPAK_LoadArchive(pf, NULL, n, n, NULL);
						else
#endif
						{
#ifdef PACKAGE_PK3					//assume zip/pk3/pk4/apk/etc
							archive = FSZIP_LoadArchive(pf, NULL, n, n, NULL);
#else
							archive = NULL;
#endif
						}

						if (archive)
						{
							unsigned int fqhash;
							pf = NULL;
							fqhash = archive->GeneratePureCRC(archive, 0, 0);
							archive->ClosePath(archive);

							if (fqhash == (unsigned int)strtoul(p->qhash, NULL, 0))
							{
								p->flags |= fl;
								if (fl&DPF_NATIVE)
									p->flags |= DPF_MARKED|DPF_ENABLED;
								break;
							}
							else
								pf = NULL;
						}
						else
							VFS_CLOSE(pf);
					}
					else
					{
						p->flags |= DPF_CORRUPT|fl;
						VFS_CLOSE(pf);
					}
					break;
				}
				VFS_CLOSE(pf);
			}
		}
	}
}

static qboolean PM_MergePackage(package_t *oldp, package_t *newp)
{
	//we don't track mirrors for previously-installed packages.
	//use the file list of the installed package, zips ignore the file list of the remote package but otherwise they must match to be mergeable
	//local installed copies of the package may lack some information, like mirrors.
	//the old package *might* be installed, the new won't be. this means we need to use the old's file list rather than the new
	if (!oldp->qhash || !strcmp(oldp->qhash?oldp->qhash:"", newp->qhash?newp->qhash:""))
	{
		unsigned int om, nm;
		struct packagedep_s *od, *nd;
		qboolean ignorefiles;
		for (om = 0; om < countof(oldp->mirror) && oldp->mirror[om]; om++)
			;
		for (nm = 0; nm < countof(newp->mirror) && newp->mirror[nm]; nm++)
			;
//		if (oldp->priority != newp->priority)
//			return false;

		ignorefiles = (oldp->extract==EXTRACT_ZIP);	//zips ignore the remote file list, its only important if its already installed (so just keep the old file list and its fine).
		if (oldp->extract != newp->extract)
		{	//if both have mirrors of different types then we have some sort of conflict
			if (ignorefiles || (om && nm))
				return false;
		}
		for (od = oldp->deps, nd = newp->deps; od && nd; )
		{
			//if its a zip then the 'remote' file list will be blank while the local list is not (we can just keep the local list).
			//if the file list DOES change, then bump the version.
			if (ignorefiles)
			{
				if (od->dtype == DEP_FILE)
				{
					od = od->next;
					continue;
				}
				if (nd->dtype == DEP_FILE)
				{
					nd = nd->next;
					continue;
				}
			}

			if (od->dtype != nd->dtype)
				return false;	//deps don't match
			if (strcmp(od->name, nd->name))
				return false;
			od = od->next;
			nd = nd->next;
		}

		//overwrite these. use the 'new' / remote values for each of them
		//the versions of the two packages will be the same, so the texts should be the same. still favour the new one so that things can be corrected serverside without needing people to redownload everything.
		if (newp->qhash){Z_Free(oldp->qhash); oldp->qhash = Z_StrDup(newp->qhash);}
		if (newp->description){Z_Free(oldp->description); oldp->description = Z_StrDup(newp->description);}
		if (newp->license){Z_Free(oldp->license); oldp->license = Z_StrDup(newp->license);}
		if (newp->author){Z_Free(oldp->author); oldp->author = Z_StrDup(newp->author);}
		if (newp->previewimage){Z_Free(oldp->previewimage); oldp->previewimage = Z_StrDup(newp->previewimage);}
		oldp->priority = newp->priority;

		if (nm)
		{	//copy over the mirrors
			oldp->extract = newp->extract;
			for (; nm --> 0 && om < countof(oldp->mirror); om++)
			{
				oldp->mirror[om] = newp->mirror[nm];
				newp->mirror[nm] = NULL;
			}
		}
		//these flags should only remain set if set in both.
		oldp->flags &= ~(DPF_FORGETONUNINSTALL|DPF_TESTING) | (newp->flags & (DPF_FORGETONUNINSTALL|DPF_TESTING));

		PM_FreePackage(newp);
		return true;
	}
	return false;
}

static void PM_InsertPackage(package_t *p)
{
	package_t **link;
	for (link = &availablepackages; *link; link = &(*link)->next)
	{
		package_t *prev = *link;
		int v = strcmp(prev->name, p->name);
		if (v > 0)
			break;	//insert before this one
		else if (v == 0)
		{	//name matches.
			//if (!strcmp(p->fullname),prev->fullname)
			if (!strcmp(p->version, prev->version))
			if (!strcmp(p->gamedir, prev->gamedir))
			if (!strcmp(p->arch?p->arch:"", prev->arch?prev->arch:""))
			{ /*package matches, merge them somehow, don't add*/
				package_t *a;
				if (PM_MergePackage(prev, p))
					return;
				for (a = p->alternative; a; a = a->next)
				{
					if (PM_MergePackage(a, p))
						return;
				}
				p->next = prev->alternative;
				prev->alternative = p;
				p->link = &prev->alternative;
				return;
			}

			//something major differs, display both independantly.
			p->flags |= DPF_DISPLAYVERSION;
			prev->flags |= DPF_DISPLAYVERSION;
		}
	}
	p->next = *link;
	p->link = link;
	*link = p;
	PM_ValidatePackage(p);
	numpackages++;
}


static qboolean PM_CheckFile(const char *filename, enum fs_relative base)
{
	vfsfile_t *f = FS_OpenVFS(filename, "rb", base);
	if (f)
	{
		VFS_CLOSE(f);
		return true;
	}
	return false;
}
static void PM_AddDep(package_t *p, int deptype, const char *depname)
{
	struct packagedep_s *nd, **link;

	//no dupes.
	for (link = &p->deps; (nd=*link) ; link = &nd->next)
	{
		if (nd->dtype == deptype && !strcmp(nd->name, depname))
			return;
	}

	//add it on the end, preserving order.
	nd = Z_Malloc(sizeof(*nd) + strlen(depname));
	nd->dtype = deptype;
	strcpy(nd->name, depname);
	nd->next = *link;
	*link = nd;
}

static void PM_AddSubList(const char *url, const char *prefix, qboolean save, qboolean trustworthy)
{
	int i;
	if (!*url)
		return;
	if (strchr(url, '\"') || strchr(url, '\n'))
		return;
	if (strchr(prefix, '\"') || strchr(prefix, '\n'))
		return;

	for (i = 0; i < numdownloadablelists; i++)
	{
		if (!strcmp(downloadablelist[i].url, url))
			break;
	}
	if (i == numdownloadablelists && i < countof(downloadablelist))
	{
		if (!strncmp(url, "https:", 6))
			downloadablelist[i].trustworthy = trustworthy;
		else
			downloadablelist[i].trustworthy = false;	//if its not a secure url, never consider it as trustworthy
		downloadablelist[i].save = save;

		downloadablelist[i].url = BZ_Malloc(strlen(url)+1);
		strcpy(downloadablelist[i].url, url);

		downloadablelist[i].prefix = BZ_Malloc(strlen(prefix)+1);
		strcpy(downloadablelist[i].prefix, prefix);

		numdownloadablelists++;
	}
}
static void PM_RemSubList(const char *url)
{
	int i;
	for (i = 0; i < numdownloadablelists; i++)
	{
		if (!strcmp(downloadablelist[i].url, url))
		{
			downloadablelist[i].save = false;
		}
	}
}

static qboolean PM_ParsePackageList(vfsfile_t *f, int parseflags, const char *url, const char *prefix)
{
	char line[65536];
	package_t *p;
	struct packagedep_s *dep;
	char *sl;

	int version;
	char defaultgamedir[64];
	char mirror[countof(p->mirror)][MAX_OSPATH];
	int nummirrors = 0;
	qboolean isauto;
	char *tokstart;
	qboolean forcewrite = false;

	if (!f)
		return forcewrite;

	Q_strncpyz(defaultgamedir, FS_GetGamedir(false), sizeof(defaultgamedir));

	if (url)
	{
		Q_strncpyz(mirror[nummirrors], url, sizeof(mirror[nummirrors]));
		sl = COM_SkipPath(mirror[nummirrors]);
		*sl = 0;
		nummirrors++;
	}

	do
	{
		if (!VFS_GETS(f, line, sizeof(line)-1))
			break;
		while((sl=strchr(line, '\n')))
			*sl = '\0';
		while((sl=strchr(line, '\r')))
			*sl = '\0';
		Cmd_TokenizeString (line, false, false);
	} while (!Cmd_Argc());

	if (strcmp(Cmd_Argv(0), "version"))
		return forcewrite;	//it's not the right format.

	version = atoi(Cmd_Argv(1));
	if (version != 0 && version != 1 && version != 2)
	{
		Con_Printf("Packagelist is of a future or incompatible version\n");
		return forcewrite;	//it's not the right version.
	}

	while(1)
	{
		if (!VFS_GETS(f, line, sizeof(line)-1))
			break;
		while((sl=strchr(line, '\n')))
			*sl = '\0';
		while((sl=strchr(line, '\r')))
			*sl = '\0';

		tokstart = COM_StringParse (line, com_token, sizeof(com_token), false, false);
		if (*com_token)
		{
			if (!strcmp(com_token, "sublist"))
			{
				char *subprefix;
				char url[MAX_OSPATH];
				tokstart = COM_StringParse (tokstart, url, sizeof(url), false, false);
				tokstart = COM_StringParse (tokstart, com_token, sizeof(com_token), false, false);
				if (*prefix)
					subprefix = va("%s/%s", prefix, com_token);
				else
					subprefix = com_token;

#if defined(HAVE_LEGACY) && defined(WEBCLIENT)
				//hack. I'm trying to retire the self-signed cert on [fte.]triptohell.info
				if (!strcmp(url, "https://triptohell.info/downloadables.php") || !strcmp(url, "https://fte.triptohell.info/downloadables.php"))
				{
					Q_strncpyz(url, "https://updates.triptohell.info/downloadables.php", sizeof(url));
					forcewrite = true;
				}
#endif

				PM_AddSubList(url, subprefix, (parseflags & DPF_ENABLED)?true:false, (parseflags&DPF_TRUSTED));
				continue;
			}
			if (!strcmp(com_token, "set"))
			{
				tokstart = COM_StringParse (tokstart, com_token, sizeof(com_token), false, false);
				if (!strcmp(com_token, "gamedir"))
				{
					tokstart = COM_StringParse (tokstart, com_token, sizeof(com_token), false, false);
					if (!*com_token)
						Q_strncpyz(defaultgamedir, FS_GetGamedir(false), sizeof(defaultgamedir));
					else
						Q_strncpyz(defaultgamedir, com_token, sizeof(defaultgamedir));
				}
				else if (!strcmp(com_token, "mirrors"))
				{
					nummirrors = 0;
					while (nummirrors < countof(mirror) && tokstart)
					{
						tokstart = COM_StringParse (tokstart, com_token, sizeof(com_token), false, false);
						if (*com_token)
						{
							Q_strncpyz(mirror[nummirrors], com_token, sizeof(mirror[nummirrors]));
							nummirrors++;
						}
					}
				}
				else if (!strcmp(com_token, "updatemode"))
				{
					tokstart = COM_StringParse (tokstart, com_token, sizeof(com_token), false, false);
					if (parseflags & DPF_ENABLED)	//don't use a downloaded file's version of this, only use the local version of it.
						Cvar_ForceSet(&pkg_autoupdate, com_token);
				}
				else if (!strcmp(com_token, "declined"))
				{
					if (parseflags & DPF_ENABLED)	//don't use a downloaded file's version of this, only use the local version of it.
					{
						tokstart = COM_StringParse (tokstart, com_token, sizeof(com_token), false, false);
						Z_Free(declinedpackages);
						if (*com_token)
							declinedpackages = Z_StrDup(com_token);
						else
							declinedpackages = NULL;
					}
				}
				else
				{
					//erk
				}
				continue;
			}
			isauto = false;
#if 0
			if (version < 2)
			{
				char pathname[256];
				const char *fullname = Cmd_Argv(0);
				if (argc > 5 || argc < 3)
				{
					Con_Printf("Package list is bad - %s\n", line);
					continue;	//but try the next line away
				}

				p = Z_Malloc(sizeof(*p));
				if (*prefix)
					Q_snprintfz(pathname, sizeof(pathname), "%s/%s", prefix, fullname);
				else
					Q_snprintfz(pathname, sizeof(pathname), "%s", fullname);
				p->name = Z_StrDup(COM_SkipPath(pathname));
				p->title = Z_StrDup(p->name);
				*COM_SkipPath(pathname) = 0;
				p->category = Z_StrDup(pathname);
				p->mirror[0] = Z_StrDup(p->name);

				p->priority = PM_DEFAULTPRIORITY;
				p->flags = parseflags;

				p->mirror[0] = Z_StrDup(Cmd_Argv(1));
				PM_AddDep(p, DEP_FILE, Cmd_Argv(2));
				Q_strncpyz(p->version, Cmd_Argv(3), sizeof(p->version));
				Q_strncpyz(p->gamedir, Cmd_Argv(4), sizeof(p->gamedir));
				if (!strcmp(p->gamedir, "../"))
				{
					p->fsroot = FS_ROOT;
					*p->gamedir = 0;
				}
				else
				{
					if (!*p->gamedir)
					{
						strcpy(p->gamedir, FS_GetGamedir(false));
		//				p->fsroot = FS_GAMEONLY;
					}
					p->fsroot = FS_ROOT;
				}
			}
			else
#endif
			{
				char pathname[256];
				char *fullname = Z_StrDup(com_token);
				char *file = NULL;
				char *url = NULL;
				char *gamedir = NULL;
				char *ver = NULL;
				char *arch = NULL;
				char *qhash = NULL;
				char *title = NULL;
				char *category = NULL;
				char *description = NULL;
				char *license = NULL;
				char *author = NULL;
				char *previewimage = NULL;
				char *website = NULL;
				int extract = EXTRACT_COPY;
				int priority = PM_DEFAULTPRIORITY;
				unsigned int flags = parseflags;
				enum fs_relative fsroot = FS_ROOT;
				int i;

				if (version > 2)
					flags &= ~DPF_ENABLED;

				p = Z_Malloc(sizeof(*p));
				for (i = 1; tokstart; i++)
				{
					char key[8192];
					char val[8192];
					char *eq;
					//the following are [\]["]key=["]value["] parameters, which is definitely messy, yes.

					//skip leading whitespace
					while (*tokstart>0 && *tokstart <= ' ')
						tokstart++;

					*val = 0;
					if (*tokstart == '\\' || *tokstart == '\"')
					{	//legacy quoting
						tokstart = COM_StringParse (tokstart, key, sizeof(key), false, false);
						eq = strchr(key, '=');
						if (eq)
						{
							*eq = 0;
							Q_strncpyz(val, eq+1, sizeof(val));
						}
					}
					else
					{
						tokstart = COM_ParseTokenOut(tokstart, "=", key, sizeof(key), NULL);
						if (!*key)
							continue;
						if (tokstart && *tokstart == '=')
						{
							tokstart++;
							if (!(*tokstart >= 0 && *tokstart <= ' '))
								tokstart = COM_ParseCString(tokstart, val, sizeof(val), NULL);
						}
					}

					if (!strcmp(key, "url"))
						Z_StrDupPtr(&url, val);
					else if (!strcmp(key, "category"))
						Z_StrDupPtr(&category, val);
					else if (!strcmp(key, "title"))
						Z_StrDupPtr(&title, val);
					else if (!strcmp(key, "gamedir"))
						Z_StrDupPtr(&gamedir, val);
					else if (!strcmp(key, "ver") || !strcmp(key, "v"))
						Z_StrDupPtr(&ver, val);
					else if (!strcmp(key, "arch"))
						Z_StrDupPtr(&arch, val);
					else if (!strcmp(key, "priority"))
						priority = atoi(val);
					else if (!strcmp(key, "qhash"))
						Z_StrDupPtr(&qhash, val);
					else if (!strcmp(key, "desc") || !strcmp(key, "description"))
						Z_StrDupPtr(&description, val);
					else if (!strcmp(key, "license"))
						Z_StrDupPtr(&license, val);
					else if (!strcmp(key, "author"))
						Z_StrDupPtr(&author, val);
					else if (!strcmp(key, "preview"))
						Z_StrDupPtr(&previewimage, val);
					else if (!strcmp(key, "website"))
						Z_StrDupPtr(&website, val);
					else if (!strcmp(key, "file"))
					{
						if (!file)
							Z_StrDupPtr(&file, val);
						PM_AddDep(p, DEP_FILE, val);
					}
					else if (!strcmp(key, "extract"))
					{
						if (!strcmp(val, "xz"))
							extract = EXTRACT_XZ;
						else if (!strcmp(val, "gz"))
							extract = EXTRACT_GZ;
						else if (!strcmp(val, "zip"))
							extract = EXTRACT_ZIP;
						else
							Con_Printf("Unknown decompression method: %s\n", val);
					}
					else if (!strcmp(key, "depend"))
						PM_AddDep(p, DEP_REQUIRE, val);
					else if (!strcmp(key, "conflict"))
						PM_AddDep(p, DEP_CONFLICT, val);
					else if (!strcmp(key, "fileconflict"))
						PM_AddDep(p, DEP_FILECONFLICT, val);
					else if (!strcmp(key, "recommend"))
						PM_AddDep(p, DEP_RECOMMEND, val);
					else if (!strcmp(key, "suggest"))
						PM_AddDep(p, DEP_SUGGEST, val);
					else if (!strcmp(key, "test"))
						flags |= DPF_TESTING;
					else if (!strcmp(key, "stale") && version==2)
						flags &= ~DPF_ENABLED;	//known about, (probably) cached, but not actually enabled.
					else if (!strcmp(key, "installed") && version>2)
						flags |= parseflags & DPF_ENABLED;
					else if (!strcmp(key, "auto"))
						isauto = true;	//autoinstalled and NOT user-installed
					else if (!strcmp(key, "root") && (parseflags&DPF_ENABLED))
					{
						if (!Q_strcasecmp(val, "bin"))
							fsroot = FS_BINARYPATH;
						else
							fsroot = FS_ROOT;
					}
					else
					{
						Con_DPrintf("Unknown package property\n");
					}
				}

				if (category)
				{
					p->name = fullname;

					if (*prefix)
						Q_snprintfz(pathname, sizeof(pathname), "%s/%s", prefix, category);
					else
						Q_snprintfz(pathname, sizeof(pathname), "%s", category);
					if (*pathname)
					{
						if (pathname[strlen(pathname)-1] != '/')
							Q_strncatz(pathname, "/", sizeof(pathname));
					}
					p->category = Z_StrDup(pathname);
				}
				else
				{
					if (*prefix)
						Q_snprintfz(pathname, sizeof(pathname), "%s/%s", prefix, fullname);
					else
						Q_snprintfz(pathname, sizeof(pathname), "%s", fullname);
					Z_Free(fullname);
					p->name = Z_StrDup(COM_SkipPath(pathname));
					*COM_SkipPath(pathname) = 0;
					p->category = Z_StrDup(pathname);
				}

				if (!title)
					title = Z_StrDup(p->name);

				Q_strncpyz(p->version, ver?ver:"", sizeof(p->version));

				Q_snprintfz(p->gamedir, sizeof(p->gamedir), "%s", gamedir?gamedir:defaultgamedir);
				p->fsroot = fsroot;
				p->extract = extract;
				p->priority = priority;
				p->flags = flags;

				p->title = title;
				p->arch = arch;
				p->qhash = qhash;
				p->description = description;
				p->license = license;
				p->author = author;
				p->previewimage = previewimage;
				p->website = website;

				if (url && (!strncmp(url, "http://", 7) || !strncmp(url, "https://", 8)))
					p->mirror[0] = Z_StrDup(url);
				else
				{
					int m;
					char *ext = "";
					char *relurl = url;
					if (!relurl)
					{
						if (extract == EXTRACT_XZ)
							ext = ".xz";
						else if (extract == EXTRACT_GZ)
							ext = ".gz";
						else if (extract == EXTRACT_ZIP)
							ext = ".zip";
						relurl = file;
					}
					if (relurl)
					{
						for (m = 0; m < nummirrors; m++)
							p->mirror[m] = Z_StrDup(va("%s%s%s", mirror[m], relurl, ext));
					}
				}
				Z_Free(ver);
				Z_Free(file);
				Z_Free(url);
				Z_Free(gamedir);
				Z_Free(category);
			}
			if (p->arch)
			{
				if (!Q_strcasecmp(p->arch, THISENGINE))
				{
					if (!Sys_EngineCanUpdate())
						p->flags |= DPF_HIDDEN;
					else
						p->flags |= DPF_ENGINE;
				}
				else if (!Q_strcasecmp(p->arch, THISARCH))
				{
					if ((p->fsroot == FS_ROOT || p->fsroot == FS_BINARYPATH) && !*p->gamedir && p->priority == PM_DEFAULTPRIORITY)
						p->flags |= DPF_PLUGIN;
				}
				else
					p->flags |= DPF_HIDDEN;	//other engine builds or other cpus are all hidden
			}
			for (dep = p->deps; dep; dep = dep->next)
			{
				if (dep->dtype == DEP_FILECONFLICT)
				{
					const char *n;
					if (*p->gamedir)
						n = va("%s/%s", p->gamedir, dep->name);
					else
						n = dep->name;
					if (PM_CheckFile(n, p->fsroot))
						p->flags |= DPF_HIDDEN;
				}
			}
			if (p->flags & DPF_ENABLED)
			{
				if (isauto)
					p->flags |= DPF_AUTOMARKED;	//FIXME: we don't know if this was manual or auto
				else
					p->flags |= DPF_USERMARKED;	//FIXME: we don't know if this was manual or auto
			}

			PM_InsertPackage(p);
		}
	}

	return forcewrite;
}

#ifdef PLUGINS
void PM_EnumeratePlugins(void (*callback)(const char *name))
{
	package_t *p;
	struct packagedep_s *d;
	for (p = availablepackages; p; p = p->next)
	{
		if ((p->flags & DPF_ENABLED) && (p->flags & DPF_PLUGIN))
		{
			for (d = p->deps; d; d = d->next)
			{
				if (d->dtype == DEP_FILE)
				{
					if (!Q_strncasecmp(d->name, PLUGINPREFIX, strlen(PLUGINPREFIX)))
						callback(d->name);
				}
			}
		}
	}
}
#endif

#ifdef PLUGINS
static package_t *PM_FindPackage(const char *packagename);
static int QDECL PM_EnumeratedPlugin (const char *name, qofs_t size, time_t mtime, void *param, searchpathfuncs_t *spath)
{
	static const char *knownarch[] =
	{
		"x32", "x64", "amd64", "x86",	//various x86 ABIs
		"arm", "arm64", "armhf",		//various arm ABIs
		"ppc", "unk",					//various misc ABIs
	};
	package_t *p;
	struct packagedep_s *dep;
	char vmname[MAX_QPATH];
	int len, l, a;
	char *dot;
	if (!strncmp(name, PLUGINPREFIX, strlen(PLUGINPREFIX)))
		Q_strncpyz(vmname, name+strlen(PLUGINPREFIX), sizeof(vmname));
	else
		Q_strncpyz(vmname, name, sizeof(vmname));
	len = strlen(vmname);
	l = strlen(ARCH_CPU_POSTFIX ARCH_DL_POSTFIX);
	if (len > l && !strcmp(vmname+len-l, ARCH_CPU_POSTFIX ARCH_DL_POSTFIX))
	{
		len -= l;
		vmname[len] = 0;
	}
	else
	{
		dot = strchr(vmname, '.');
		if (dot)
		{
			*dot = 0;
			len = strlen(vmname);

			//if we can find a known cpu arch there then ignore it - its a different cpu arch
			for (a = 0; a < countof(knownarch); a++)
			{
				l = strlen(knownarch[a]);
				if (len > l && !Q_strcasecmp(vmname + len-l, knownarch[a]))
					return true;	//wrong arch! ignore it.
			}
		}
	}
	if (len > 0 && vmname[len-1] == '_')
		vmname[len-1] = 0;

	for (p = availablepackages; p; p = p->next)
	{
		if (!(p->flags & DPF_PLUGIN))
			continue;
		for (dep = p->deps; dep; dep = dep->next)
		{
			if (dep->dtype != DEP_FILE)
				continue;
			if (!Q_strcasecmp(dep->name, name))
				return true;
		}
	}

	if (PM_FindPackage(vmname))
		return true;	//don't include it if its a dupe anyway.

	p = Z_Malloc(sizeof(*p));
	p->deps = Z_Malloc(sizeof(*p->deps) + strlen(name));
	p->deps->dtype = DEP_FILE;
	strcpy(p->deps->name, name);
	p->arch = Z_StrDup(THISARCH);
	p->name = Z_StrDup(vmname);
	p->title = Z_StrDup(vmname);
	p->category = Z_StrDup("Plugins/");
	p->priority = PM_DEFAULTPRIORITY;
	p->fsroot = FS_BINARYPATH;
	strcpy(p->version, "??""??");
	p->flags = DPF_PLUGIN|DPF_NATIVE|DPF_FORGETONUNINSTALL;
	PM_InsertPackage(p);

	*(int*)param = true;

	return true;
}
#ifndef SERVERONLY
void PM_PluginDetected(void *ctx, int status)
{
	PM_WriteInstalledPackages();
	if (status == 0)
	{
		Cmd_ExecuteString("menu_download\n", RESTRICT_LOCAL);
		Cmd_ExecuteString("menu_download \"Plugins/\"\n", RESTRICT_LOCAL);
	}
}
#endif
#endif

static void PM_PreparePackageList(void)
{
	//figure out what we've previously installed.
	if (!loadedinstalled)
	{
		vfsfile_t *f = FS_OpenVFS(INSTALLEDFILES, "rb", FS_ROOT);
		loadedinstalled = true;
		if (f)
		{
			if (PM_ParsePackageList(f, DPF_FORGETONUNINSTALL|DPF_ENABLED, NULL, ""))
				PM_WriteInstalledPackages();
			VFS_CLOSE(f);
		}

#ifdef PLUGINS
		{
			int foundone = false;
			char nat[MAX_OSPATH];
			FS_NativePath("", FS_BINARYPATH, nat, sizeof(nat));
			Con_DPrintf("Loading plugins from \"%s\"\n", nat);
			Sys_EnumerateFiles(nat, PLUGINPREFIX"*" ARCH_DL_POSTFIX, PM_EnumeratedPlugin, &foundone, NULL);
			if (foundone && !pluginpromptshown)
			{
				pluginpromptshown = true;
#ifndef SERVERONLY
				M_Menu_Prompt(PM_PluginDetected, NULL, "Plugin(s) appears to have\nbeen installed externally.\nUse the updates menu\nto enable them.", "View", NULL, "Disable");
#endif
			}
		}
#endif
	}
}

void PM_LoadPackages(searchpath_t **oldpaths, const char *parent_pure, const char *parent_logical, searchpath_t *search, unsigned int loadstuff, int minpri, int maxpri)
{
	package_t *p;
	struct packagedep_s *d;
	char temp[MAX_OSPATH];
	int pri;

	//figure out what we've previously installed.
	PM_PreparePackageList();

	do
	{
		//find the lowest used priority above the previous
		pri = maxpri;
		for (p = availablepackages; p; p = p->next)
		{
			if ((p->flags & DPF_ENABLED) && p->qhash && p->priority>=minpri&&p->priority<pri && !Q_strcasecmp(parent_pure, p->gamedir))
				pri = p->priority;
		}
		minpri = pri+1;

		for (p = availablepackages; p; p = p->next)
		{
			if ((p->flags & DPF_ENABLED) && p->qhash && p->priority==pri && !Q_strcasecmp(parent_pure, p->gamedir))
			{
				for (d = p->deps; d; d = d->next)
				{
					if (d->dtype == DEP_FILE)
					{
						Q_snprintfz(temp, sizeof(temp), "%s/%s", p->gamedir, d->name);
						FS_AddHashedPackage(oldpaths, parent_pure, parent_logical, search, loadstuff, temp, p->qhash, NULL, SPF_COPYPROTECTED|SPF_UNTRUSTED);
					}
				}
			}
		}
	} while (pri < maxpri);
}

void PM_Shutdown(void)
{
	//free everything...
	pkg_downloads_url.modified = false;

	downloadablessequence++;

	while(numdownloadablelists > 0)
	{
		numdownloadablelists--;

#ifdef WEBCLIENT
		if (downloadablelist[numdownloadablelists].curdl)
		{
			DL_Close(downloadablelist[numdownloadablelists].curdl);
			downloadablelist[numdownloadablelists].curdl = NULL;
		}
#endif
		downloadablelist[numdownloadablelists].received = 0;
		Z_Free(downloadablelist[numdownloadablelists].url);
		downloadablelist[numdownloadablelists].url = NULL;
		Z_Free(downloadablelist[numdownloadablelists].prefix);
		downloadablelist[numdownloadablelists].prefix = NULL;
	}

	while (availablepackages)
		PM_FreePackage(availablepackages);
	loadedinstalled = false;
}

//finds the newest version
static package_t *PM_FindPackage(const char *packagename)
{
	package_t *p, *r = NULL;

	//fixme: NAME (>VER)
	//fixme: NAME (<VER)
	//fixme: NAME:ARCH (>VER)

	for (p = availablepackages; p; p = p->next)
	{
		if (!strcmp(p->name, packagename))
		{
			if (!r || strcmp(r->version, p->version)>0)
				r = p;
		}
	}
	return r;
}
//returns the marked version of a package, if any.
static package_t *PM_MarkedPackage(const char *packagename, int markflag)
{
	package_t *p;
	for (p = availablepackages; p; p = p->next)
	{
		if (p->flags & markflag)
			if (!strcmp(p->name, packagename))
				return p;
	}
	return NULL;
}

//just resets all actions, so that a following apply won't do anything.
static void PM_RevertChanges(void)
{
	package_t *p;

	if (pkg_updating)
		return;

	for (p = availablepackages; p; p = p->next)
	{
		if (p->flags & DPF_ENABLED)
			p->flags |= DPF_USERMARKED;
		else
			p->flags &= ~DPF_MARKED;
		p->flags &= ~DPF_PURGE;
	}
}

static qboolean PM_HasDependant(package_t *package, unsigned int markflag)
{
	package_t *o;
	struct packagedep_s *dep;
	for (o = availablepackages; o; o = o->next)
	{
		if (o->flags & markflag)
			for (dep = o->deps; dep; dep = dep->next)
				if (dep->dtype == DEP_REQUIRE || dep->dtype == DEP_RECOMMEND || dep->dtype == DEP_SUGGEST)
					if (!strcmp(package->name, dep->name))
						return true;
	}
	return false;
}

//just flags, doesn't delete (yet)
//markflag should be DPF_AUTOMARKED or DPF_USERMARKED
static void PM_UnmarkPackage(package_t *package, unsigned int markflag)
{
	package_t *o;
	struct packagedep_s *dep;

	if (pkg_updating)
		return;

	if (!(package->flags & markflag))
		return;	//looks like its already deselected.
	package->flags &= ~(markflag);

	if (!(package->flags & DPF_MARKED))
	{
#ifdef WEBCLIENT
		//Is this safe?
		package->trymirrors = 0;	//if its enqueued, cancel that quickly...
		if (package->download)
		{					//if its currently downloading, cancel it.
			DL_Close(package->download);
			package->download = NULL;
		}
#endif

		//remove stuff that depends on us
		for (o = availablepackages; o; o = o->next)
		{
			for (dep = o->deps; dep; dep = dep->next)
				if (dep->dtype == DEP_REQUIRE)
					if (!strcmp(dep->name, package->name))
						PM_UnmarkPackage(o, DPF_MARKED);
		}
	}
	if (!(package->flags & DPF_USERMARKED))
	{
		//go through dependancies and unmark any automarked packages if nothing else depends upon them
		for (dep = package->deps; dep; dep = dep->next)
		{
			if (dep->dtype == DEP_REQUIRE || dep->dtype == DEP_RECOMMEND)
			{
				package_t *d = PM_MarkedPackage(dep->name, DPF_AUTOMARKED);
				if (d && !(d->flags & DPF_USERMARKED))
				{
					if (!PM_HasDependant(d, DPF_MARKED))
						PM_UnmarkPackage(d, DPF_AUTOMARKED);
				}
			}
		}
	}
}

//just flags, doesn't install
static void PM_MarkPackage(package_t *package, unsigned int markflag)
{
	package_t *o;
	struct packagedep_s *dep, *dep2;
	qboolean replacing = false;

	if (pkg_updating)
		return;

	if (package->flags & DPF_MARKED)
	{
		package->flags |= markflag;
		return;	//looks like its already picked. marking it again will do no harm.
	}

#ifndef WEBCLIENT
	//can't mark for download if we cannot download.
	if (!(package->flags & DPF_PRESENT))
	{	//though we can at least unmark it for deletion...
		package->flags &= ~DPF_PURGE;
		return;
	}
#endif

	//any file-conflicts prevent the package from being installable.
	//this is mostly for pak1.pak
	for (dep = package->deps; dep; dep = dep->next)
	{
		if (dep->dtype == DEP_FILECONFLICT)
		{
			const char *n;
			if (*package->gamedir)
				n = va("%s/%s", package->gamedir, dep->name);
			else
				n = dep->name;
			if (PM_CheckFile(n, package->fsroot))
				return;
		}
	}

	package->flags |= markflag;

	//first check to see if we're replacing a different version of the same package
	for (o = availablepackages; o; o = o->next)
	{
		if (o == package)
			continue;

		if (o->flags & DPF_MARKED)
		{
			if (!strcmp(o->name, package->name))
			{	//replaces this package
				o->flags &= ~DPF_MARKED;
				replacing = true;
			}
			else
			{	//two packages with the same filename are always mutually incompatible, but with totally separate dependancies etc.
				qboolean remove = false;
				for (dep = package->deps; dep; dep = dep->next)
				{
					if (dep->dtype == DEP_FILE)
					for (dep2 = o->deps; dep2; dep2 = dep2->next)
					{
						if (dep2->dtype == DEP_FILE)
						if (!strcmp(dep->name, dep2->name))
						{
							PM_UnmarkPackage(o, DPF_MARKED);
							remove = true;
							break;
						}
					}
					if (remove)
						break;
				}
				//fixme: zip content conflicts
			}
		}
	}

	//if we are replacing an existing one, then dependancies are already settled (only because we don't do version deps)
	if (replacing)
		return;

	//satisfy our dependancies.
	for (dep = package->deps; dep; dep = dep->next)
	{
		if (dep->dtype == DEP_REQUIRE || dep->dtype == DEP_RECOMMEND)
		{
			package_t *d = PM_MarkedPackage(dep->name, DPF_MARKED);
			if (!d)
			{
				d = PM_FindPackage(dep->name);
				if (d)
					PM_MarkPackage(d, DPF_AUTOMARKED);
				else
					Con_DPrintf("Couldn't find dependancy \"%s\"\n", dep->name);
			}
		}
		if (dep->dtype == DEP_CONFLICT)
		{
			for (;;)
			{
				package_t *d = PM_MarkedPackage(dep->name, DPF_MARKED);
				if (!d)
					break;
				PM_UnmarkPackage(d, DPF_MARKED);
			}
		}
	}

	//remove any packages that conflict with us.
	for (o = availablepackages; o; o = o->next)
	{
		for (dep = o->deps; dep; dep = dep->next)
			if (dep->dtype == DEP_CONFLICT)
				if (!strcmp(dep->name, package->name))
					PM_UnmarkPackage(o, DPF_MARKED);
	}
}

static qboolean PM_NameIsInStrings(const char *strings, const char *match)
{
	char tok[1024];
	while (strings && *strings)
	{
		strings = COM_ParseStringSetSep(strings, ';', tok, sizeof(tok));
		if (!Q_strcasecmp(tok, match))	//okay its here.
			return true;
	}
	return false;
}

//just flag stuff as needing updating
unsigned int PM_MarkUpdates (void)
{
	unsigned int changecount = 0;
	package_t *p, *o, *b, *e = NULL;

	if (manifestpackages)
	{
		char tok[1024];
		char *strings = manifestpackages;
		while (strings && *strings)
		{
			strings = COM_ParseStringSetSep(strings, ';', tok, sizeof(tok));
			if (PM_NameIsInStrings(declinedpackages, tok))
				continue;

			p = PM_MarkedPackage(tok, DPF_MARKED);
			if (!p)
			{
				p = PM_FindPackage(tok);
				if (p)
				{
					PM_MarkPackage(p, DPF_AUTOMARKED);
					changecount++;
				}
			}
			else if (!(p->flags & DPF_ENABLED))
				changecount++;
		}
	}

	for (p = availablepackages; p; p = p->next)
	{
		if ((p->flags & DPF_ENGINE) && !(p->flags & DPF_HIDDEN))
		{
			if (!(p->flags & DPF_TESTING) || pkg_autoupdate.ival >= UPD_TESTING)
				if (!e || strcmp(e->version, p->version) < 0)	//package must be more recent than the previously found engine
					if (strcmp(SVNREVISIONSTR, "-") && strcmp(SVNREVISIONSTR, p->version) < 0)	//package must be more recent than the current engine too, there's no point auto-updating to an older revision.
						e = p;
		}
		if (p->flags & DPF_MARKED)
		{
			b = NULL;
			for (o = availablepackages; o; o = o->next)
			{
				if (p == o || (o->flags & DPF_HIDDEN))
					continue;
				if (!(o->flags & DPF_TESTING) || pkg_autoupdate.ival >= UPD_TESTING)
					if (!strcmp(o->name, p->name) && !strcmp(o->arch?o->arch:"", p->arch?p->arch:"") && strcmp(o->version, p->version) > 0)
					{
						if (!b || strcmp(b->version, o->version) < 0)
							b = o;
					}
			}

			if (b)
			{
				changecount++;
				PM_MarkPackage(b, p->flags&DPF_MARKED);
				PM_UnmarkPackage(p, DPF_MARKED);
			}
		}
	}
	if (e && !(e->flags & DPF_MARKED))
	{
		if (pkg_autoupdate.ival >= UPD_STABLE)
		{
			changecount++;
			PM_MarkPackage(e, DPF_AUTOMARKED);
		}
	}

	return changecount;
}

#if defined(M_Menu_Prompt) || defined(SERVERONLY)
#else
static unsigned int PM_ChangeList(char *out, size_t outsize)
{
	unsigned int changes = 0;
	const char *change;
	package_t *p;
	size_t l;
	size_t ofs = 0;
	if (!outsize)
		out = NULL;
	else
		*out = 0;
	for (p = availablepackages; p; p=p->next)
	{
		if (!(p->flags & DPF_MARKED) != !(p->flags & DPF_ENABLED) || (p->flags & DPF_PURGE))
		{
			changes++;
			if (!out)
				continue;

			if (p->flags & DPF_MARKED)
			{
				if (p->flags & DPF_PURGE)
					change = va(" reinstall %s\n", p->name);
				else if (p->flags & DPF_PRESENT)
					change = va(" enable %s\n", p->name);
				else
					change = va(" install %s\n", p->name);
			}
			else if ((p->flags & DPF_PURGE) || !(p->qhash && (p->flags & DPF_PRESENT)))
				change = va(" uninstall %s\n", p->name);
			else
				change = va(" disable %s\n", p->name);

			l = strlen(change);
			if (ofs+l >= outsize)
			{
				Q_strncpyz(out, "Too many changes\n", outsize);
				out = NULL;

				break;
			}
			else
			{
				memcpy(out+ofs, change, l);
				ofs += l;
				out[ofs] = 0;
			}
		}
	}
	return changes;
}
#endif

static void PM_PrintChanges(void)
{
	qboolean changes = 0;
	package_t *p;
	for (p = availablepackages; p; p=p->next)
	{
		if (!(p->flags & DPF_MARKED) != !(p->flags & DPF_ENABLED) || (p->flags & DPF_PURGE))
		{
			changes++;
			if (p->flags & DPF_MARKED)
			{
				if (p->flags & DPF_PURGE)
					Con_Printf(" reinstall %s\n", p->name);
				else
					Con_Printf(" install %s\n", p->name);
			}
			else if ((p->flags & DPF_PURGE) || !(p->qhash && (p->flags & DPF_CACHED)))
				Con_Printf(" uninstall %s\n", p->name);
			else
				Con_Printf(" disable %s\n", p->name);
		}
	}
	if (!changes)
		Con_Printf("<no changes>\n");
	else
		Con_Printf("<%i package(s) changed>\n", changes);
}

#ifdef WEBCLIENT
static void PM_ListDownloaded(struct dl_download *dl)
{
	int i;
	vfsfile_t *f;
	f = dl->file;
	dl->file = NULL;

	i = dl->user_num;

	if (dl != downloadablelist[i].curdl)
	{
		//this request looks stale.
		VFS_CLOSE(f);
		return;
	}
	downloadablelist[i].curdl = NULL;

	if (f)
	{
		downloadablelist[i].received = 1;
		PM_ParsePackageList(f, 0, dl->url, downloadablelist[i].prefix);
		VFS_CLOSE(f);
	}
	else
		downloadablelist[i].received = -1;

	if (!doautoupdate && !domanifestinstall)
		return;	//don't spam this.
	for (i = 0; i < numdownloadablelists; i++)
	{
		if (!downloadablelist[i].received)
			break;
	}
/*
	if (domanifestinstall == MANIFEST_SECURITY_INSTALLER && manifestpackages)
	{
		package_t *meta;
		meta = PM_MarkedPackage(manifestpackages);
		if (!meta)
		{
			meta = PM_FindPackage(manifestpackages);
			if (meta)
			{
				PM_RevertChanges();
				PM_MarkPackage(meta);
				PM_ApplyChanges();

#ifdef DOWNLOADMENU
				if (!isDedicated)
				{
					if (Key_Dest_Has(kdm_emenu))
					{
						Key_Dest_Remove(kdm_emenu);
					}
#ifdef MENU_DAT
					if (Key_Dest_Has(kdm_gmenu))
						MP_Toggle(0);
#endif
					Cmd_ExecuteString("menu_download\n", RESTRICT_LOCAL);
				
				}
#endif
				return;
			}
		}
	}
*/
	if ((doautoupdate || domanifestinstall == MANIFEST_SECURITY_DEFAULT) && i == numdownloadablelists)
	{
		if (PM_MarkUpdates())
		{
#ifdef DOWNLOADMENU
			if (!isDedicated)
			{
				if (Key_Dest_Has(kdm_emenu))
					Key_Dest_Remove(kdm_emenu);
#ifdef MENU_DAT
				if (Key_Dest_Has(kdm_gmenu))
					MP_Toggle(0);
#endif
				Cmd_ExecuteString("menu_download\n", RESTRICT_LOCAL);
			}
			else
#endif
				PM_PrintChanges();
		}
	}
}
#endif
//retry 1==
static void PM_UpdatePackageList(qboolean autoupdate, int retry)
{
	unsigned int i;

	if (retry>1 || pkg_downloads_url.modified)
		PM_Shutdown();

	PM_PreparePackageList();

	//make sure our sources are okay.
	if (*pkg_downloads_url.string)
		PM_AddSubList(pkg_downloads_url.string, "", false, true);

#ifndef WEBCLIENT
	for (i = 0; i < numdownloadablelists; i++)
		downloadablelist[i].received = true;
#else
	doautoupdate |= autoupdate;

	//kick off the initial tier of list-downloads.
	for (i = 0; i < numdownloadablelists; i++)
	{
		if (downloadablelist[i].received)
			continue;
		autoupdate = false;
		if (downloadablelist[i].curdl)
			continue;

		downloadablelist[i].curdl = HTTP_CL_Get(va("%s%s"DOWNLOADABLESARGS, downloadablelist[i].url, strchr(downloadablelist[i].url,'?')?"&":"?"), NULL, PM_ListDownloaded);
		if (downloadablelist[i].curdl)
		{
			downloadablelist[i].curdl->user_num = i;

			downloadablelist[i].curdl->file = VFSPIPE_Open(1, false);
			downloadablelist[i].curdl->isquery = true;
			DL_CreateThread(downloadablelist[i].curdl, NULL, NULL);
		}
		else
		{
			Con_Printf("Could not contact updates server - %s\n", downloadablelist[i].url);
			downloadablelist[i].received = -1;
		}
	}

	if (autoupdate)
	{
		doautoupdate = 0;
		if (PM_MarkUpdates())
		{
#ifdef DOWNLOADMENU
			if (!isDedicated)
				Cbuf_AddText("menu_download\n", RESTRICT_LOCAL);
			else
#endif
				PM_PrintChanges();
		}
	}
#endif
}




static void COM_QuotedConcat(const char *cat, char *buf, size_t bufsize)
{
	const unsigned char *gah;
	for (gah = (const unsigned char*)cat; *gah; gah++)
	{
		if (*gah <= ' ' || *gah == '$' || *gah == '\"' || *gah == '\n' || *gah == '\r')
			break;
	}
	if (*gah || *cat == '\\' ||
		strstr(cat, "//") || strstr(cat, "/*"))
	{	//contains some dodgy stuff.
		size_t curlen = strlen(buf);
		buf += curlen;
		bufsize -= curlen;
		COM_QuotedString(cat, buf, bufsize, false);
	}
	else
	{	//okay, no need for quotes.
		Q_strncatz(buf, cat, bufsize);
	}
}
static void PM_WriteInstalledPackages(void)
{
	char buf[65536];
	int i;
	char *s;
	package_t *p, *e = NULL;
	struct packagedep_s *dep, *ef = NULL;
	vfsfile_t *f = FS_OpenVFS(INSTALLEDFILES, "wb", FS_ROOT);
	if (!f)
	{
		Con_Printf("package manager: Can't update installed list\n");
		return;
	}

	s = "version 2\n";
	VFS_WRITE(f, s, strlen(s));

	s = va("set updatemode %s\n", COM_QuotedString(pkg_autoupdate.string, buf, sizeof(buf), false));
	VFS_WRITE(f, s, strlen(s));
	s = va("set declined %s\n", COM_QuotedString(declinedpackages?declinedpackages:"", buf, sizeof(buf), false));
	VFS_WRITE(f, s, strlen(s));

	for (i = 0; i < numdownloadablelists; i++)
	{
		if (downloadablelist[i].save)
		{
			s = va("sublist \"%s\" \"%s\"\n", downloadablelist[i].url, downloadablelist[i].prefix);
			VFS_WRITE(f, s, strlen(s));
		}
	}

	for (p = availablepackages; p ; p=p->next)
	{
		if (p->flags & (DPF_PRESENT|DPF_ENABLED))
		{
			buf[0] = 0;
			COM_QuotedString(va("%s%s", p->category, p->name), buf, sizeof(buf), false);
			if (p->flags & DPF_ENABLED)
			{	//v3+
//				Q_strncatz(buf, " ", sizeof(buf));
//				COM_QuotedConcat(va("installed=1"), buf, sizeof(buf));
			}
			else
			{	//v2
				Q_strncatz(buf, " ", sizeof(buf));
				COM_QuotedConcat(va("stale=1"), buf, sizeof(buf));
			}
			if (*p->title && strcmp(p->title, p->name))
			{
				Q_strncatz(buf, " ", sizeof(buf));
				COM_QuotedConcat(va("title=%s", p->title), buf, sizeof(buf));
			}
			if (*p->version)
			{
				Q_strncatz(buf, " ", sizeof(buf));
				COM_QuotedConcat(va("ver=%s", p->version), buf, sizeof(buf));
			}
			//if (*p->gamedir)
			{
				Q_strncatz(buf, " ", sizeof(buf));
				COM_QuotedConcat(va("gamedir=%s", p->gamedir), buf, sizeof(buf));
			}
			if (p->qhash)
			{
				Q_strncatz(buf, " ", sizeof(buf));
				COM_QuotedConcat(va("qhash=%s", p->qhash), buf, sizeof(buf));
			}
			if (p->priority!=PM_DEFAULTPRIORITY)
			{
				Q_strncatz(buf, " ", sizeof(buf));
				COM_QuotedConcat(va("priority=%i", p->priority), buf, sizeof(buf));
			}
			if (p->arch)
			{
				Q_strncatz(buf, " ", sizeof(buf));
				COM_QuotedConcat(va("arch=%s", p->arch), buf, sizeof(buf));
			}

			if (p->license)
			{
				Q_strncatz(buf, " ", sizeof(buf));
				COM_QuotedConcat(va("license=%s", p->license), buf, sizeof(buf));
			}
			if (p->website)
			{
				Q_strncatz(buf, " ", sizeof(buf));
				COM_QuotedConcat(va("website=%s", p->website), buf, sizeof(buf));
			}
			if (p->author)
			{
				Q_strncatz(buf, " ", sizeof(buf));
				COM_QuotedConcat(va("author=%s", p->author), buf, sizeof(buf));
			}
			if (p->description)
			{
				Q_strncatz(buf, " ", sizeof(buf));
				COM_QuotedConcat(va("desc=%s", p->description), buf, sizeof(buf));
			}
			if (p->previewimage)
			{
				Q_strncatz(buf, " ", sizeof(buf));
				COM_QuotedConcat(va("preview=%s", p->previewimage), buf, sizeof(buf));
			}

			if (p->fsroot == FS_BINARYPATH)
			{
				Q_strncatz(buf, " ", sizeof(buf));
				COM_QuotedConcat("root=bin", buf, sizeof(buf));
			}

			for (dep = p->deps; dep; dep = dep->next)
			{
				if (dep->dtype == DEP_FILE)
				{
					Q_strncatz(buf, " ", sizeof(buf));
					COM_QuotedConcat(va("file=%s", dep->name), buf, sizeof(buf));
					if ((p->flags & DPF_ENABLED) && (p->flags & DPF_ENGINE) && (!e || strcmp(e->version, p->version) < 0))
					{
						e = p;
						ef = dep;
					}
				}
				else if (dep->dtype == DEP_REQUIRE)
				{
					Q_strncatz(buf, " ", sizeof(buf));
					COM_QuotedConcat(va("depend=%s", dep->name), buf, sizeof(buf));
				}
				else if (dep->dtype == DEP_CONFLICT)
				{
					Q_strncatz(buf, " ", sizeof(buf));
					COM_QuotedConcat(va("conflict=%s", dep->name), buf, sizeof(buf));
				}
				else if (dep->dtype == DEP_FILECONFLICT)
				{
					Q_strncatz(buf, " ", sizeof(buf));
					COM_QuotedConcat(va("fileconflict=%s", dep->name), buf, sizeof(buf));
				}
				else if (dep->dtype == DEP_RECOMMEND)
				{
					Q_strncatz(buf, " ", sizeof(buf));
					COM_QuotedConcat(va("recommend=%s", dep->name), buf, sizeof(buf));
				}
			}

			if (p->flags & DPF_TESTING)
			{
				Q_strncatz(buf, " ", sizeof(buf));
				COM_QuotedConcat("test=1", buf, sizeof(buf));
			}

			if ((p->flags & DPF_AUTOMARKED) && !(p->flags & DPF_USERMARKED))
			{
				Q_strncatz(buf, " ", sizeof(buf));
				COM_QuotedConcat("auto", buf, sizeof(buf));
			}

			buf[sizeof(buf)-2] = 0;	//just in case.
			Q_strncatz(buf, "\n", sizeof(buf));
			VFS_WRITE(f, buf, strlen(buf));
		}
	}

	VFS_CLOSE(f);

	if (ef)
	{
		char native[MAX_OSPATH];
		FS_NativePath(ef->name, e->fsroot, native, sizeof(native));
		Sys_SetUpdatedBinary(native);
	}
}

#ifdef WEBCLIENT
//callback from PM_Download_Got, extracts each file from an archive
static int QDECL PM_ExtractFiles(const char *fname, qofs_t fsize, time_t mtime, void *parm, searchpathfuncs_t *spath)
{	//this is gonna suck. threading would help, but gah.
	package_t *p = parm;
	flocation_t loc;
	if (fname[strlen(fname)-1] == '/')
	{	//directory.

	}
	else if (spath->FindFile(spath, &loc, fname, NULL) && loc.len < 0x80000000u)
	{
		char *f = malloc(loc.len);
		const char *n;
		if (f)
		{
			spath->ReadFile(spath, &loc, f);
			if (*p->gamedir)
				n = va("%s/%s", p->gamedir, fname);
			else
				n = fname;
			if (FS_WriteFile(n, f, loc.len, p->fsroot))
				p->flags |= DPF_NATIVE|DPF_ENABLED;
			free(f);

			//keep track of the installed files, so we can delete them properly after.
			PM_AddDep(p, DEP_FILE, fname);
		}
	}
	return 1;
}

//package has been downloaded and installed, but some packages need to be enabled
//(plugins might have other dll dependancies, so this can only happen AFTER the entire package was extracted)
static void PM_PackageEnabled(package_t *p)
{
	char ext[8];
	struct packagedep_s *dep;
	FS_FlushFSHashFull();
	for (dep = p->deps; dep; dep = dep->next)
	{
		if (dep->dtype != DEP_FILE)
			continue;
		COM_FileExtension(dep->name, ext, sizeof(ext));
		if (!stricmp(ext, "pak") || !stricmp(ext, "pk3"))
			FS_ReloadPackFiles();
#ifdef PLUGINS
		if ((p->flags & DPF_PLUGIN) && !Q_strncasecmp(dep->name, PLUGINPREFIX, strlen(PLUGINPREFIX)))
			Cmd_ExecuteString(va("plug_load %s\n", dep->name), RESTRICT_LOCAL);
#endif
#ifdef MENU_DAT
		if (!Q_strcasecmp(dep->name, "menu.dat"))
			Cmd_ExecuteString("menu_restart\n", RESTRICT_LOCAL);
#endif
	}
}

static void PM_StartADownload(void);
//callback from PM_StartADownload
static void PM_Download_Got(struct dl_download *dl)
{
	char native[MAX_OSPATH];
	qboolean successful = dl->status == DL_FINISHED;
	package_t *p;
	char *tempname = dl->user_ctx;

	for (p = availablepackages; p ; p=p->next)
	{
		if (p->download == dl)
			break;
	}

	if (dl->file)
	{
		VFS_CLOSE(dl->file);
		dl->file = NULL;
	}

	if (p)
	{
		char ext[8];
		char *destname;
		struct packagedep_s *dep;
		p->download = NULL;

		if (!successful)
		{
			Con_Printf("Couldn't download %s (from %s)\n", p->name, dl->url);
			FS_Remove (tempname, p->fsroot);
			Z_Free(tempname);
			PM_StartADownload();
			return;
		}

		if (p->extract == EXTRACT_ZIP)
		{
#ifdef PACKAGE_PK3
			vfsfile_t *f = FS_OpenVFS(tempname, "rb", p->fsroot);
			if (f)
			{
				searchpathfuncs_t *archive = FSZIP_LoadArchive(f, NULL, tempname, tempname, NULL);
				if (archive)
				{
					p->flags &= ~(DPF_NATIVE|DPF_CACHED|DPF_CORRUPT|DPF_ENABLED);
					archive->EnumerateFiles(archive, "*", PM_ExtractFiles, p);
					archive->EnumerateFiles(archive, "*/*", PM_ExtractFiles, p);
					archive->EnumerateFiles(archive, "*/*/*", PM_ExtractFiles, p);
					archive->EnumerateFiles(archive, "*/*/*/*", PM_ExtractFiles, p);
					archive->EnumerateFiles(archive, "*/*/*/*/*", PM_ExtractFiles, p);
					archive->EnumerateFiles(archive, "*/*/*/*/*/*", PM_ExtractFiles, p);
					archive->EnumerateFiles(archive, "*/*/*/*/*/*/*", PM_ExtractFiles, p);
					archive->EnumerateFiles(archive, "*/*/*/*/*/*/*/*", PM_ExtractFiles, p);
					archive->EnumerateFiles(archive, "*/*/*/*/*/*/*/*/*", PM_ExtractFiles, p);
					archive->ClosePath(archive);

					PM_WriteInstalledPackages();

//					if (!stricmp(ext, "pak") || !stricmp(ext, "pk3"))
//						FS_ReloadPackFiles();
				}
				else
					VFS_CLOSE(f);
			}
			PM_ValidatePackage(p);
#else
			Con_Printf("zip format not supported in this build - %s (from %s)\n", p->name, dl->url);
#endif
			FS_Remove (tempname, p->fsroot);
			Z_Free(tempname);
			PM_StartADownload();
			return;
		}
		else
		{
			for (dep = p->deps; dep; dep = dep->next)
			{
				unsigned int nfl;
				if (dep->dtype != DEP_FILE)
					continue;

				COM_FileExtension(dep->name, ext, sizeof(ext));
				if (!stricmp(ext, "pak") || !stricmp(ext, "pk3"))
					FS_UnloadPackFiles();	//we reload them after
#ifdef PLUGINS
				if ((!stricmp(ext, "dll") || !stricmp(ext, "so")) && !Q_strncmp(dep->name, PLUGINPREFIX, strlen(PLUGINPREFIX)))
					Cmd_ExecuteString(va("plug_close %s\n", dep->name), RESTRICT_LOCAL);	//try to purge plugins so there's no files left open
#endif

				nfl = DPF_NATIVE;
				if (*p->gamedir)
				{
					char temp[MAX_OSPATH];
					destname = va("%s/%s", p->gamedir, dep->name);
					if (p->qhash && FS_GenCachedPakName(destname, p->qhash, temp, sizeof(temp)))
					{
						nfl = DPF_CACHED;
						destname = va("%s", temp);
					}
				}
				else
					destname = dep->name;
				nfl |= DPF_ENABLED | (p->flags & ~(DPF_CACHED|DPF_NATIVE|DPF_CORRUPT));
				FS_CreatePath(destname, p->fsroot);
				if (FS_Remove(destname, p->fsroot))
					;
				if (!FS_Rename2(tempname, destname, p->fsroot, p->fsroot))
				{
					//error!
					if (!FS_NativePath(destname, p->fsroot, native, sizeof(native)))
						Q_strncpyz(native, destname, sizeof(native));
					Con_Printf("Couldn't rename %s to %s. Removed instead.\n", tempname, native);
					FS_Remove (tempname, p->fsroot);
				}
				else
				{	//success!
					if (!FS_NativePath(destname, p->fsroot, native, sizeof(native)))
						Q_strncpyz(native, destname, sizeof(native));
					Con_Printf("Downloaded %s (to %s)\n", p->name, native);
					p->flags = nfl;
					PM_WriteInstalledPackages();
				}

				PM_ValidatePackage(p);

				PM_PackageEnabled(p);

				Z_Free(tempname);
				PM_StartADownload();
				return;
			}
		}
		Con_Printf("menu_download: %s has no filename info\n", p->name);
	}
	else
		Con_Printf("menu_download: Can't figure out where %s came from (url: %s)\n", dl->localname, dl->url);

	FS_Remove (tempname, FS_GAMEONLY);
	Z_Free(tempname);
	PM_StartADownload();
}

static char *PM_GetTempName(package_t *p)
{
	struct packagedep_s *dep, *fdep;
	char *destname, *t, *ts;
	//always favour the file so that we can rename safely without needing a copy.
	for (dep = p->deps, fdep = NULL; dep; dep = dep->next)
	{
		if (dep->dtype != DEP_FILE)
			continue;
		if (fdep)
		{
			fdep = NULL;
			break;
		}
		fdep = dep;
	}
	if (fdep)
	{
		if (*p->gamedir)
			destname = va("%s/%s.tmp", p->gamedir, fdep->name);
		else
			destname = va("%s.tmp", fdep->name);
		return Z_StrDup(destname);
	}
	ts = Z_StrDup(p->name);
	for (t = ts; *t; t++)
	{
		switch(*t)
		{
		case '/':
		case '?':
		case '<':
		case '>':
		case '\\':
		case ':':
		case '*':
		case '|':
		case '\"':
		case '.':
			*t = '_';
			break;
		default:
			break;
		}
	}
	if (*ts)
	{
		if (*p->gamedir)
			destname = va("%s/%s.tmp", p->gamedir, ts);
		else
			destname = va("%s.tmp", ts);
	}
	else
		destname = va("%x.tmp", (unsigned int)(quintptr_t)p);
	Z_Free(ts);
	return Z_StrDup(destname);
}
#endif

/*static void PM_AddDownloadedPackage(const char *filename)
{
	char pathname[1024];
	package_t *p;
	Q_snprintfz(pathname, sizeof(pathname), "%s/%s", "Cached", filename);
	p->name = Z_StrDup(COM_SkipPath(pathname));
	*COM_SkipPath(pathname) = 0;
	p->category = Z_StrDup(pathname);

	Q_strncpyz(p->version, "", sizeof(p->version));

	Q_snprintfz(p->gamedir, sizeof(p->gamedir), "%s", "");
	p->fsroot = FS_ROOT;
	p->extract = EXTRACT_COPY;
	p->priority = 0;
	p->flags = DPF_INSTALLED;

	p->title = Z_StrDup(p->name);
	p->arch = NULL;
	p->qhash = NULL; //FIXME
	p->description = NULL;
	p->license = NULL;
	p->author = NULL;
	p->previewimage = NULL;
}*/

int PM_IsApplying(qboolean listsonly)
{
	int count = 0;
#ifdef WEBCLIENT
	package_t *p;
	int i;
	if (!listsonly)
	{
		for (p = availablepackages; p ; p=p->next)
		{
			if (p->download)
				count++;
		}
	}
	for (i = 0; i < numdownloadablelists; i++)
	{
		if (downloadablelist[i].curdl)
			count++;
	}
#endif
	return count;
}

//looks for the next package that needs downloading, and grabs it
static void PM_StartADownload(void)
{
#ifdef WEBCLIENT
	vfsfile_t *tmpfile;
	char *temp;
	package_t *p;
	const int simultaneous = PM_IsApplying(true)?1:2;
	int i;
	qboolean downloading = false;

	for (p = availablepackages; p && simultaneous > PM_IsApplying(false); p=p->next)
	{
		if (p->download)
			downloading = true;

		if (p->trymirrors)
		{	//flagged for a (re?)download
			char *mirror = NULL;
			for (i = 0; i < countof(p->mirror); i++)
			{
				if (p->mirror[i] && (p->trymirrors & (1u<<i)))
				{
					mirror = p->mirror[i];
					p->trymirrors &= ~(1u<<i);
					break;
				}
			}
			if (!mirror)
			{	//erk...
				p->trymirrors = 0;

				for (i = 0; i < countof(p->mirror); i++)
					if (p->mirror[i])
						break;
				if (i == countof(p->mirror))
				{	//this appears to be a meta package with no download
					//just directly install it.
					//FIXME: make sure there's no files...
					p->flags &= ~(DPF_NATIVE|DPF_CACHED|DPF_CORRUPT);
					p->flags |= DPF_ENABLED;

					Con_Printf("Enabled meta package %s\n", p->name);
					PM_WriteInstalledPackages();
					PM_PackageEnabled(p);
				}
				continue;
			}

			if ((p->flags & DPF_PRESENT) && !PM_PurgeOnDisable(p))
			{	//its in our cache directory, so lets just use that
				p->trymirrors = 0;
				p->flags |= DPF_ENABLED;

				Con_Printf("Enabled cached package %s\n", p->name);
				PM_WriteInstalledPackages();
				PM_PackageEnabled(p);
				continue;
			}


			temp = PM_GetTempName(p);

			//FIXME: we should lock in the temp path, in case the user foolishly tries to change gamedirs.

			FS_CreatePath(temp, p->fsroot);
			switch (p->extract)
			{
			case EXTRACT_ZIP:
			case EXTRACT_COPY:
				tmpfile = FS_OpenVFS(temp, "wb", p->fsroot);
				break;
#ifdef AVAIL_XZDEC
			case EXTRACT_XZ:
				{
					vfsfile_t *raw;
					raw = FS_OpenVFS(temp, "wb", p->fsroot);
					tmpfile = FS_XZ_DecompressWriteFilter(raw);
					if (!tmpfile)
						VFS_CLOSE(raw);
				}
				break;
#endif
#ifdef AVAIL_GZDEC
			case EXTRACT_GZ:
				{
					vfsfile_t *raw;
					raw = FS_OpenVFS(temp, "wb", p->fsroot);
					tmpfile = FS_GZ_WriteFilter(raw, true, false);
					if (!tmpfile)
						VFS_CLOSE(raw);
				}
				break;
#endif
			default:
				Con_Printf("decompression method not supported\n");
				continue;
			}

			if (tmpfile)
			{
				p->download = HTTP_CL_Get(mirror, NULL, PM_Download_Got);
				if (!p->download)
					Con_Printf("Unable to download %s\n", p->name);
			}
			else
			{
				char syspath[MAX_OSPATH];
				FS_NativePath(temp, p->fsroot, syspath, sizeof(syspath));
				Con_Printf("Unable to write %s. Fix permissions before trying to download %s\n", syspath, p->name);
			}
			if (p->download)
			{
				Con_Printf("Downloading %s\n", p->name);
				p->download->file = tmpfile;
				p->download->user_ctx = temp;

				DL_CreateThread(p->download, NULL, NULL);
				downloading = true;
			}
			else
			{
				p->flags &= ~DPF_MARKED;	//can't do it.
				if (tmpfile)
					VFS_CLOSE(tmpfile);
				FS_Remove(temp, p->fsroot);
			}
		}
	}

	//clear the updating flag once there's no more activity needed
	pkg_updating = downloading;
#endif
}
//'just' starts doing all the things needed to remove/install selected packages
void PM_ApplyChanges(void)
{
	package_t *p, **link;
	char temp[MAX_OSPATH];

#ifdef WEBCLIENT
	if (pkg_updating)
		return;
	pkg_updating = true;
#endif

//delete any that don't exist
	for (link = &availablepackages; *link ; )
	{
		p = *link;
#ifdef WEBCLIENT
		if (p->download)
			; //erk, dude, don't do two!
		else
#endif
			if ((p->flags & DPF_PURGE) || (!(p->flags&DPF_MARKED) && (p->flags&DPF_ENABLED)))
		{	//if we don't want it but we have it anyway. don't bother to follow this logic when reinstalling
			qboolean reloadpacks = false;
			struct packagedep_s *dep;


			for (dep = p->deps; dep; dep = dep->next)
			{
				if (dep->dtype == DEP_FILE)
				{
					char ext[8];
					COM_FileExtension(dep->name, ext, sizeof(ext));
					if (!stricmp(ext, "pak") || !stricmp(ext, "pk3"))
						reloadpacks = true;

#ifdef PLUGINS		//when disabling/purging plugins, be sure to unload them first (unfortunately there might be some latency before this can actually happen).
					if ((p->flags & DPF_PLUGIN) && !Q_strncasecmp(dep->name, PLUGINPREFIX, strlen(PLUGINPREFIX)))
						Cmd_ExecuteString(va("plug_close %s\n", dep->name), RESTRICT_LOCAL);	//try to purge plugins so there's no files left open
#endif

				}
			}
			if (reloadpacks)	//okay, some package was removed, unload all, do the deletions/disables, then reload them. This is kinda shit. Would be better to remove individual packages, which would avoid unnecessary config execs.
				FS_UnloadPackFiles();

			if ((p->flags & DPF_PURGE) || PM_PurgeOnDisable(p))
			{
				Con_Printf("Purging package %s\n", p->name);
				for (dep = p->deps; dep; dep = dep->next)
				{
					if (dep->dtype == DEP_FILE)
					{
						if (*p->gamedir)
						{
							char *f = va("%s/%s", p->gamedir, dep->name);
							if (p->qhash && FS_GenCachedPakName(f, p->qhash, temp, sizeof(temp)) && PM_CheckFile(temp, p->fsroot))
							{
								if (!FS_Remove(temp, p->fsroot))
									p->flags |= DPF_CACHED;
							}
							else if (!FS_Remove(va("%s/%s", p->gamedir, dep->name), p->fsroot))
								p->flags |= DPF_NATIVE;
						}
						else if (!FS_Remove(dep->name, p->fsroot))
							p->flags |= DPF_NATIVE;
					}
				}
			}
			else
			{
				for (dep = p->deps; dep; dep = dep->next)
				{
					if (dep->dtype == DEP_FILE)
					{
						if (*p->gamedir)
						{
							char *f = va("%s/%s", p->gamedir, dep->name);
							if ((p->flags & DPF_NATIVE) && p->qhash && FS_GenCachedPakName(f, p->qhash, temp, sizeof(temp)))
								FS_Rename(f, temp, p->fsroot);
						}
					}
				}
				Con_Printf("Disabling package %s\n", p->name);
			}
			p->flags &= ~(DPF_PURGE|DPF_ENABLED);

			/* FIXME: windows bug:
			** deleting an exe might 'succeed' but leave the file on disk for a while anyway.
			** the file will eventually disappear, but until then we'll still see it as present,
			**  be unable to delete it again, and trying to open it to see if it still exists
			**  will fail.
			** there's nothing we can do other than wait until whatever part of
			**  windows that's fucking up releases its handles.
			** thankfully this only affects reinstalling exes/engines.
			*/

			PM_ValidatePackage(p);
			PM_WriteInstalledPackages();

			if (reloadpacks)
				FS_ReloadPackFiles();

			if ((p->flags & DPF_FORGETONUNINSTALL) && !(p->flags & DPF_PRESENT))
			{
#if 1
				downloadablessequence++;
				PM_FreePackage(p);
#else
				if (p->alternative)
				{	//replace it with its alternative package
					*p->link = p->alternative;
					p->alternative->alternative = p->alternative->next;
					if (p->alternative->alternative)
						p->alternative->alternative->link = &p->alternative->alternative;
					p->alternative->next = p->next;
				}
				else
				{	//just remove it from the list.
					*p->link = p->next;
					if (p->next)
						p->next->link = p->link;
				}

//FIXME: the menu(s) hold references to packages, so its not safe to purge them
				p->flags |= DPF_HIDDEN;
//					BZ_Free(p);
#endif

				continue;
			}
		}

		link = &(*link)->next;
	}

#ifdef WEBCLIENT
	//and flag any new/updated ones for a download
	for (p = availablepackages; p ; p=p->next)
	{
		if ((p->flags&DPF_MARKED) && !(p->flags&DPF_ENABLED) && !p->download)
			p->trymirrors = ~0u;
	}
#endif
	PM_StartADownload();	//and try to do those downloads.
}

#if defined(M_Menu_Prompt) || defined(SERVERONLY)
//if M_Menu_Prompt is a define, then its a stub...
static void PM_PromptApplyChanges(void)
{
	PM_ApplyChanges();
}
#else
static qboolean PM_DeclinedPackages(char *out, size_t outsize)
{
	size_t ofs = 0;
	package_t *p;
	qboolean ret = false;
	if (manifestpackages)
	{
		char tok[1024];
		char *strings = manifestpackages;
		while (strings && *strings)
		{
			strings = COM_ParseStringSetSep(strings, ';', tok, sizeof(tok));

			//already in the list
			if (PM_NameIsInStrings(declinedpackages, tok))
				continue;

			p = PM_MarkedPackage(tok, DPF_MARKED);
			if (p)	//don't mark it as declined if it wasn't
				continue;

			p = PM_FindPackage(tok);
			if (p)
			{	//okay, it was declined
				ret = true;
				if (!out)
				{	//we're confirming that they should be flagged as declined
					if (declinedpackages)
					{
						char *t = declinedpackages;
						declinedpackages = Z_StrDup(va("%s;%s", declinedpackages, tok));
						Z_Free(t);
					}
					else
						declinedpackages = Z_StrDup(tok);
				}
				else
				{	//we're collecting a list of package names
					char *change = va("%s\n", p->name);
					size_t l = strlen(change);
					if (ofs+l >= outsize)
					{
						Q_strncpyz(out, "Too many changes\n", outsize);
						out = NULL;

						break;
					}
					else
					{
						memcpy(out+ofs, change, l);
						ofs += l;
						out[ofs] = 0;
					}
					break;
				}
			}
		}
	}
	if (!out && ret)
		PM_WriteInstalledPackages();
	return ret;
}
static void PM_PromptApplyChanges_Callback(void *ctx, int opt)
{
#ifdef WEBCLIENT
	pkg_updating = false;
#endif
	if (opt == 0)
		PM_ApplyChanges();
}
static void PM_PromptApplyChanges(void);
static void PM_PromptApplyDecline_Callback(void *ctx, int opt)
{
#ifdef WEBCLIENT
	pkg_updating = false;
#endif
	if (opt == 1)
	{
		PM_DeclinedPackages(NULL, 0);
		PM_PromptApplyChanges();
	}
}
static void PM_PromptApplyChanges(void)
{
	unsigned int changes;
	char text[8192];
#ifdef WEBCLIENT
	//lock it down, so noone can make any changes while this prompt is still displayed
	if (pkg_updating)
	{
		M_Menu_Prompt(PM_PromptApplyChanges_Callback, NULL, "An update is already in progress\nPlease wait\n", NULL, NULL, "Cancel");
		return;
	}
	pkg_updating = true;
#endif

	strcpy(text, "Really decline the following\nrecommendedpackages?\n\n");
	if (PM_DeclinedPackages(text+strlen(text), sizeof(text)-strlen(text)))
		M_Menu_Prompt(PM_PromptApplyDecline_Callback, NULL, text, NULL, "Confirm", "Cancel");
	else
	{
		strcpy(text, "Apply the following changes?\n\n");
		changes = PM_ChangeList(text+strlen(text), sizeof(text)-strlen(text));
		if (!changes)
		{
#ifdef WEBCLIENT
			pkg_updating = false;//no changes...
#endif
		}
		else
			M_Menu_Prompt(PM_PromptApplyChanges_Callback, NULL, text, "Apply", NULL, "Cancel");
	}
}
#endif

//names packages that were listed from the  manifest.
//if 'mark' is true, then this is an initial install.
void PM_ManifestPackage(const char *metaname, int security)
{
	domanifestinstall = security;
	Z_Free(manifestpackages);
	if (metaname && security)
	{
		manifestpackages = Z_StrDup(metaname);
//		PM_UpdatePackageList(false, false);
	}
	else
		manifestpackages = NULL;
}

qboolean PM_CanInstall(const char *packagename)
{
	int i;
	package_t *p = PM_FindPackage(packagename);
	if (p && !(p->flags&(DPF_ENABLED|DPF_CORRUPT|DPF_HIDDEN)))
	{
		for (i = 0; i < countof(p->mirror); i++)
			if (p->mirror[i])
				return true;
	}
	return false;
}

void PM_Command_f(void)
{
	package_t *p;
	const char *act = Cmd_Argv(1);
	const char *key;

	if (Cmd_FromGamecode())
	{
		Con_Printf("%s may not be used from gamecode\n", Cmd_Argv(0));
		return;
	}
	
	if (!loadedinstalled)
		PM_UpdatePackageList(false, false);

	if (!strcmp(act, "list"))
	{
		for (p = availablepackages; p; p=p->next)
		{
			const char *status;
			char *markup;
			if (p->flags & DPF_ENABLED)
				markup = S_COLOR_GREEN;
			else if (p->flags & DPF_CORRUPT)
				markup = S_COLOR_RED;
			else if (p->flags & (DPF_CACHED))
				markup = S_COLOR_YELLOW;	//downloaded but not active
			else
				markup = S_COLOR_WHITE;

			if (!(p->flags & DPF_MARKED) != !(p->flags & DPF_ENABLED) || (p->flags & DPF_PURGE))
			{
				if (p->flags & DPF_MARKED)
				{
					if (p->flags & DPF_PURGE)
						status = S_COLOR_CYAN"<reinstall>";
					else
						status = S_COLOR_CYAN"<inst	all>";
				}
				else if ((p->flags & DPF_PURGE) || !(p->qhash && (p->flags & DPF_CACHED)))
					status = S_COLOR_CYAN"<uninstall>";
				else
					status = S_COLOR_CYAN"<disable>";
			}
			else if ((p->flags & (DPF_ENABLED|DPF_CACHED)) == DPF_CACHED)
				status = S_COLOR_CYAN"<disabled>";
			else if (p->flags & DPF_USERMARKED)
				status = S_COLOR_GRAY"<manual>";
			else if (p->flags & DPF_AUTOMARKED)
				status = S_COLOR_GRAY"<auto>";
			else
				status = "";

			Con_Printf(" ^["S_COLOR_GRAY"%s%s%s%s%s^] %s"S_COLOR_GRAY" %s (%s%s)\n", p->category?p->category:"", markup, p->name, p->arch?":":"", p->arch?p->arch:"", status, strcmp(p->name, p->title)?p->title:"", p->version, (p->flags&DPF_TESTING)?"-testing":"");
		}
		Con_Printf("<end of list>\n");
	}
	else if (!strcmp(act, "show"))
	{
		key = Cmd_Argv(2);
		p = PM_FindPackage(key);
		if (p)
		{
			if (p->previewimage)
				Con_Printf("^[%s (%s)\\tipimg\\%s\\tip\\%s^]\n", p->name, p->version, p->previewimage, "");
			else
				Con_Printf("%s (%s)\n", p->name, p->version);
			if (p->title)
				Con_Printf("	title: %s\n", p->title);
			if (p->license)
				Con_Printf("	license: %s\n", p->license);
			if (p->author)
				Con_Printf("	author: %s\n", p->author);
			if (p->website)
				Con_Printf("	website: %s\n", p->website);
			if (p->description)
				Con_Printf("%s\n", p->description);

			if (p->flags & DPF_MARKED)
			{
				if (p->flags & DPF_ENABLED)
				{
					if (p->flags & DPF_PURGE)
						Con_Printf("	package is flagged to be re-installed\n");
					else
						Con_Printf("	package is currently installed\n");
				}
				else
					Con_Printf("	package is flagged to be installed\n");
			}
			else
			{
				if (p->flags & DPF_ENABLED)
				{
					if (p->flags & DPF_PURGE)
						Con_Printf("	package is flagged to be purged\n");
					else
						Con_Printf("	package is flagged to be disabled\n");
				}
				else
					Con_Printf("	package is not installed\n");
			}
			if (p->flags & DPF_NATIVE)
				Con_Printf("	package is native\n");
			if (p->flags & DPF_CACHED)
				Con_Printf("	package is cached\n");
			if (p->flags & DPF_CORRUPT)
				Con_Printf("	package is corrupt\n");
			if (p->flags & DPF_DISPLAYVERSION)
				Con_Printf("	package has a version conflict\n");
			if (p->flags & DPF_FORGETONUNINSTALL)
				Con_Printf("	package is obsolete\n");
			if (p->flags & DPF_HIDDEN)
				Con_Printf("	package is hidden\n");
			if (p->flags & DPF_ENGINE)
				Con_Printf("	package is an engine update\n");
			if (p->flags & DPF_TESTING)
				Con_Printf("	package is untested\n");
			return;
		}
		Con_Printf("<package not found>\n");
	}
	else if (!strcmp(act, "search") || !strcmp(act, "find"))
	{
		const char *key = Cmd_Argv(2);
		for (p = availablepackages; p; p=p->next)
		{
			if (Q_strcasestr(p->name, key) || (p->title && Q_strcasestr(p->title, key)) || (p->description && Q_strcasestr(p->description, key)))
			{
				Con_Printf("%s\n", p->name);
			}
		}
		Con_Printf("<end of list>\n");
	}
#ifdef WEBCLIENT
	else if (!strcmp(act, "sources") || !strcmp(act, "addsource"))
	{
		if (Cmd_Argc() == 2)
		{
			int i;
			for (i = 0; i < numdownloadablelists; i++)
				Con_Printf("%s %s\n", downloadablelist[i].url, downloadablelist[i].save?"(explicit)":"(implicit)");
			Con_Printf("<%i sources>\n", numdownloadablelists);
		}
		else
		{
			PM_AddSubList(Cmd_Argv(2), "", true, true);
			PM_WriteInstalledPackages();
		}
	}
#endif
	else if (!strcmp(act, "remsource"))
	{
		PM_RemSubList(Cmd_Argv(2));
		PM_WriteInstalledPackages();
	}
	else if (!strcmp(act, "apply"))
	{
		Con_Printf("Applying package changes\n");
		if (qrenderer != QR_NONE)
			PM_PromptApplyChanges();
		else if (Cmd_ExecLevel == RESTRICT_LOCAL)
			PM_ApplyChanges();
	}
	else if (!strcmp(act, "changes"))
	{
		PM_PrintChanges();
	}
	else if (!strcmp(act, "reset") || !strcmp(act, "revert"))
	{
		PM_RevertChanges();
	}
#ifdef WEBCLIENT
	else if (!strcmp(act, "update"))
	{	//flush package cache, make a new request.
		int i;
		for (i = 0; i < numdownloadablelists; i++)
			downloadablelist[i].received = 0;
	}
	else if (!strcmp(act, "upgrade"))
	{	//auto-mark any updated packages.
		unsigned int changes = PM_MarkUpdates();
		if (changes)
		{
			Con_Printf("%u packages flagged\n", changes);
			PM_PromptApplyChanges();
		}
		else
			Con_Printf("Already using latest versions of all packages\n");
	}
#endif
	else if (!strcmp(act, "add") || !strcmp(act, "get") || !strcmp(act, "install") || !strcmp(act, "enable"))
	{	//FIXME: make sure this updates.
		int arg = 2;
		for (arg = 2; arg < Cmd_Argc(); arg++)
		{
			const char *key = Cmd_Argv(arg);
			p = PM_FindPackage(key);
			if (p)
				PM_MarkPackage(p, DPF_USERMARKED);
			else
				Con_Printf("%s: package %s not known\n", Cmd_Argv(0), key);
		}
		PM_PrintChanges();
	}
#ifdef WEBCLIENT
	else if (!strcmp(act, "reinstall"))
	{	//fixme: favour the current verson.
		int arg = 2;
		for (arg = 2; arg < Cmd_Argc(); arg++)
		{
			const char *key = Cmd_Argv(arg);
			p = PM_FindPackage(key);
			if (p)
			{
				PM_MarkPackage(p, DPF_USERMARKED);
				p->flags |= DPF_PURGE;
			}
			else
				Con_Printf("%s: package %s not known\n", Cmd_Argv(0), key);
		}
		PM_PrintChanges();
	}
#endif
	else if (!strcmp(act, "disable") || !strcmp(act, "rem") || !strcmp(act, "remove"))
	{
		int arg = 2;
		for (arg = 2; arg < Cmd_Argc(); arg++)
		{
			const char *key = Cmd_Argv(arg);
			p = PM_MarkedPackage(key, DPF_MARKED);
			if (!p)
				p = PM_FindPackage(key);
			if (p)
				PM_UnmarkPackage(p, DPF_MARKED);
			else
				Con_Printf("%s: package %s not known\n", Cmd_Argv(0), key);
		}
		PM_PrintChanges();
	}
	else if (!strcmp(act, "del") || !strcmp(act, "purge") || !strcmp(act, "delete") || !strcmp(act, "uninstall"))
	{
		int arg = 2;
		for (arg = 2; arg < Cmd_Argc(); arg++)
		{
			const char *key = Cmd_Argv(arg);
			p = PM_MarkedPackage(key, DPF_MARKED);
			if (!p)
				p = PM_FindPackage(key);
			if (p)
			{
				PM_UnmarkPackage(p, DPF_MARKED);
				p->flags |=	DPF_PURGE;
			}
			else
				Con_Printf("%s: package %s not known\n", Cmd_Argv(0), key);
		}
		PM_PrintChanges();
	}
	else
		Con_Printf("%s: Unknown action %s\nShould be one of list, show, search, upgrade, revert, add, rem, del, changes, apply, sources, addsource, remsource\n", Cmd_Argv(0), act);
}

qboolean PM_FindUpdatedEngine(char *syspath, size_t syspathsize)
{
	struct packagedep_s *dep;
	package_t *e = NULL, *p;
	char *pfname;
	//figure out what we've previously installed.
	PM_PreparePackageList();

	for (p = availablepackages; p; p = p->next)
	{
		if ((p->flags & DPF_ENGINE) && !(p->flags & DPF_HIDDEN) && p->fsroot == FS_ROOT)
		{
			if ((p->flags & DPF_ENABLED) && (!e || strcmp(e->version, p->version) < 0))
			if (strcmp(SVNREVISIONSTR, "-") && strcmp(SVNREVISIONSTR, p->version) < 0)	//package must be more recent than the current engine too, there's no point auto-updating to an older revision.
			{
				for (dep = p->deps, pfname = NULL; dep; dep = dep->next)
				{
					if (dep->dtype != DEP_FILE)
						continue;
					if (pfname)
					{
						pfname = NULL;
						break;
					}
					pfname = dep->name;
				}

				if (pfname && PM_CheckFile(pfname, p->fsroot))
				{
					if (FS_NativePath(pfname, p->fsroot, syspath, syspathsize))
						e = p;
				}
			}
		}
	}

	if (e)
		return true;
	return false;
}

#else
qboolean PM_CanInstall(const char *packagename)
{
	return false;
}
void PM_EnumeratePlugins(void (*callback)(const char *name))
{
}
void PM_ManifestPackage(const char *metaname, int security)
{
}
int PM_IsApplying(qboolean listsonly)
{
	return false;
}
qboolean PM_FindUpdatedEngine(char *syspath, size_t syspathsize)
{
	return false;
}
#endif

#ifdef DOWNLOADMENU
typedef struct {
	menucustom_t *list;
	char intermediatefilename[MAX_QPATH];
	char pathprefix[MAX_QPATH];
	int downloadablessequence;
	char titletext[128];
	qboolean populated;
} dlmenu_t;

static void MD_Draw (int x, int y, struct menucustom_s *c, struct menu_s *m)
{
	package_t *p;
	char *n;
	if (c->dint != downloadablessequence)
		return;	//probably stale
	p = c->dptr;
	if (p)
	{
		if (p->alternative && (p->flags & DPF_HIDDEN))
			p = p->alternative;

#ifdef WEBCLIENT
		if (p->download)
			Draw_FunStringWidth (x, y, va("%i%%", (int)p->download->qdownload.percent), 48, 2, false);
		else if (p->trymirrors)
			Draw_FunStringWidth (x, y, "PND", 48, 2, false);
		else
#endif
		{
			if (p->flags & DPF_USERMARKED)
			{
				if (!(p->flags & DPF_ENABLED))
				{	//DPF_MARKED|!DPF_ENABLED:
					if (p->flags & DPF_PURGE)
						Draw_FunStringWidth (x, y, "GET", 48, 2, false);
					else if (p->flags & (DPF_PRESENT))
						Draw_FunStringWidth (x, y, "USE", 48, 2, false);
					else
						Draw_FunStringWidth (x, y, "GET", 48, 2, false);
				}
				else
				{	//DPF_MARKED|DPF_ENABLED:
					if (p->flags & DPF_PURGE)
						Draw_FunStringWidth (x, y, "GET", 48, 2, false);	//purge and reinstall.
					else if (p->flags & DPF_CORRUPT)
						Draw_FunStringWidth (x, y, "?""?""?", 48, 2, false);
					else
					{
						Draw_FunStringWidth (x, y, "^Ue080^Ue082", 48, 2, false);
						Draw_FunStringWidth (x, y, "^Ue083", 48, 2, false);
					}
				}
			}
			else if (p->flags & DPF_MARKED)
			{
				if (!(p->flags & DPF_ENABLED))
				{	//DPF_MARKED|!DPF_ENABLED:
					if (p->flags & DPF_PURGE)
						Draw_FunStringWidth (x, y, "^hGET", 48, 2, false);
					else if (p->flags & (DPF_PRESENT))
						Draw_FunStringWidth (x, y, "^hUSE", 48, 2, false);
					else
						Draw_FunStringWidth (x, y, "^hGET", 48, 2, false);
				}
				else
				{	//DPF_MARKED|DPF_ENABLED:
					if (p->flags & DPF_PURGE)
						Draw_FunStringWidth (x, y, "^hGET", 48, 2, false);	//purge and reinstall.
					else if (p->flags & DPF_CORRUPT)
						Draw_FunStringWidth (x, y, "?""?""?", 48, 2, false);
					else
					{
						Draw_FunStringWidth (x, y, "^Ue080^Ue082", 48, 2, false);
						Draw_FunStringWidth (x, y, "^Ue083", 48, 2, false);
					}
				}
			}
			else
			{
				if (!(p->flags & DPF_ENABLED))
				{	//!DPF_MARKED|!DPF_ENABLED:
					if (p->flags & DPF_PURGE)
						Draw_FunStringWidth (x, y, "DEL", 48, 2, false);	//purge
					else if (p->flags & DPF_HIDDEN)
						Draw_FunStringWidth (x, y, "---", 48, 2, false);
					else if (p->flags & DPF_CORRUPT)
						Draw_FunStringWidth (x, y, "!!!", 48, 2, false);
					else
					{
						Draw_FunStringWidth (x, y, "^Ue080^Ue082", 48, 2, false);
						Draw_FunStringWidth (x, y, "^Ue081", 48, 2, false);
						if (p->flags & DPF_PRESENT)
							Draw_FunStringWidth (x, y, "-", 48, 2, false);
					}
				}
				else
				{	//!DPF_MARKED|DPF_ENABLED:
					if ((p->flags & DPF_PURGE) || PM_PurgeOnDisable(p))
						Draw_FunStringWidth (x, y, "DEL", 48, 2, false);
					else
						Draw_FunStringWidth (x, y, "REM", 48, 2, false);
				}
			}
		}

		n = p->title;
		if (p->flags & DPF_DISPLAYVERSION)
			n = va("%s (%s)", n, *p->version?p->version:"unversioned");

		if (p->flags & DPF_TESTING)	//hide testing updates 
			n = va("^h%s", n);
//			if (!(p->flags & (DPF_ENABLED|DPF_MARKED|DPF_PRESENT))
//				continue;

//		if (&m->selecteditem->common == &c->common)
//			Draw_AltFunString (x+48, y, n);
//		else
			Draw_FunString(x+48, y, n);
	}
}

static qboolean MD_Key (struct menucustom_s *c, struct menu_s *m, int key, unsigned int unicode)
{
	extern qboolean	keydown[];
	qboolean ctrl = keydown[K_LCTRL] || keydown[K_RCTRL];
	package_t *p, *p2;
	struct packagedep_s *dep, *dep2;
	if (c->dint != downloadablessequence)
		return false;	//probably stale
	p = c->dptr;
	if (key == 'c' && ctrl)
		Sys_SaveClipboard(CBT_CLIPBOARD, p->website);
	else if (key == K_ENTER || key == K_KP_ENTER || key == K_GP_START || key == K_MOUSE1)
	{
		if (p->alternative && (p->flags & DPF_HIDDEN))
			p = p->alternative;

		if (p->flags & DPF_ENABLED)
		{
			switch (p->flags & (DPF_PURGE|DPF_MARKED))
			{
			case DPF_USERMARKED:
			case DPF_AUTOMARKED:
			case DPF_MARKED:
				PM_UnmarkPackage(p, DPF_MARKED);	//deactivate it
				break;
			case 0:
				p->flags |= DPF_PURGE;	//purge
				if (!PM_PurgeOnDisable(p))
					break;
				//fall through
			case DPF_PURGE:
				PM_MarkPackage(p, DPF_USERMARKED);		//reinstall
//				if (!(p->flags & DPF_HIDDEN) && !(p->flags & DPF_CACHED))
//					break;
				//fall through
			case DPF_USERMARKED|DPF_PURGE:
			case DPF_AUTOMARKED|DPF_PURGE:
			case DPF_MARKED|DPF_PURGE:
				p->flags &= ~DPF_PURGE;	//back to no-change
				break;
			}
		}
		else
		{
			switch (p->flags & (DPF_PURGE|DPF_MARKED))
			{
			case 0:
				PM_MarkPackage(p, DPF_USERMARKED);
				//now: try to install
				break;
			case DPF_AUTOMARKED:	//
				p->flags |= DPF_USERMARKED;
				break;
			case DPF_USERMARKED:
			case DPF_MARKED:
				p->flags |= DPF_PURGE;
#ifdef WEBCLIENT
				//now: re-get despite already having it.
				if ((p->flags & DPF_CORRUPT) || ((p->flags & DPF_PRESENT) && !PM_PurgeOnDisable(p)))
					break;	//only makes sense if we already have a cached copy that we're not going to use.
#endif
				//fallthrough
			case DPF_USERMARKED|DPF_PURGE:
			case DPF_AUTOMARKED|DPF_PURGE:
			case DPF_MARKED|DPF_PURGE:
				PM_UnmarkPackage(p, DPF_MARKED);
				//now: delete
				if ((p->flags & DPF_CORRUPT) || ((p->flags & DPF_PRESENT) && !PM_PurgeOnDisable(p)))
					break;	//only makes sense if we have a cached/corrupt copy of it already
				//fallthrough
			case DPF_PURGE:
				p->flags &= ~DPF_PURGE;
				//now: no change
				break;
			}
		}

		if (p->flags&DPF_MARKED)
		{
			//any other packages that conflict should be flagged for uninstall now that this one will replace it.
			for (p2 = availablepackages; p2; p2 = p2->next)
			{
				if (p == p2)
					continue;
				for (dep = p->deps; dep; dep = dep->next)
				{
					if (dep->dtype != DEP_FILE)
						continue;
					for (dep2 = p2->deps; dep2; dep2 = dep2->next)
					{
						if (dep2->dtype != DEP_FILE)
							continue;
						if (!strcmp(dep->name, dep2->name))
						{
							PM_UnmarkPackage(p2, DPF_MARKED);
							break;
						}
					}
				}
			}
		}
#ifdef WEBCLIENT
		else
			p->trymirrors = 0;
#endif
		return true;
	}

	return false;
}

#ifdef WEBCLIENT
static void MD_AutoUpdate_Draw (int x, int y, struct menucustom_s *c, struct menu_s *m)
{
	char *settings[] = 
	{
		"Off",
		"Stable Updates",
		"Test Updates"
	};
	char *text;
	int setting = bound(0, pkg_autoupdate.ival, 2);
	text = va("Auto Update: ^a%s", settings[setting]);
//	if (&m->selecteditem->common == &c->common)
//		Draw_AltFunString (x, y, text);
//	else
		Draw_FunString (x, y, text);
}
static qboolean MD_AutoUpdate_Key (struct menucustom_s *c, struct menu_s *m, int key, unsigned int unicode)
{
	if (key == K_ENTER || key == K_KP_ENTER || key == K_GP_START || key == K_MOUSE1)
	{
		char nv[8] = "0";
		if (pkg_autoupdate.ival < UPD_TESTING && pkg_autoupdate.ival >= 0)
			Q_snprintfz(nv, sizeof(nv), "%i", pkg_autoupdate.ival+1);
		Cvar_ForceSet(&pkg_autoupdate, nv);
		PM_WriteInstalledPackages();

		PM_UpdatePackageList(true, 0);
	}
	return false;
}

static qboolean MD_MarkUpdatesButton (union menuoption_s *mo,struct menu_s *m,int key)
{
	if (key == K_ENTER || key == K_KP_ENTER || key == K_GP_START || key == K_MOUSE1)
	{
		PM_MarkUpdates();
		return true;
	}
	return false;
}
#endif

qboolean MD_PopMenu (union menuoption_s *mo,struct menu_s *m,int key)
{
	if (key == K_ENTER || key == K_KP_ENTER || key == K_GP_START || key == K_MOUSE1)
	{
		M_RemoveMenu(m);
		return true;
	}
	return false;
}

static qboolean MD_ApplyDownloads (union menuoption_s *mo,struct menu_s *m,int key)
{
	if (key == K_ENTER || key == K_KP_ENTER || key == K_GP_START || key == K_MOUSE1)
	{
		PM_PromptApplyChanges();
		return true;
	}
	return false;
}

static qboolean MD_RevertUpdates (union menuoption_s *mo,struct menu_s *m,int key)
{
	if (key == K_ENTER || key == K_KP_ENTER || key == K_GP_START || key == K_MOUSE1)
	{
		PM_RevertChanges();
		return true;
	}
	return false;
}

static int MD_AddItemsToDownloadMenu(menu_t *m, int y, const char *pathprefix)
{
	char path[MAX_QPATH];
	package_t *p;
	menucustom_t *c;
	char *slash;
	menuoption_t *mo;
	int prefixlen = strlen(pathprefix);

	//add all packages in this dir
	for (p = availablepackages; p; p = p->next)
	{
		if (strncmp(p->category, pathprefix, prefixlen))
			continue;
		if ((p->flags & DPF_HIDDEN) && (p->arch || !(p->flags & DPF_ENABLED)))
			continue;
		slash = strchr(p->category+prefixlen, '/');
		if (!slash)
		{
			char *desc;
			if (p->author || p->license || p->website)
				desc = va("^aauthor: ^a%s\n^alicense: ^a%s\n^awebsite: ^a%s\n%s", p->author?p->author:"^hUnknown^h", p->license?p->license:"^hUnknown^h", p->website?p->website:"^hUnknown^h", p->description);
			else
				desc = p->description;
			c = MC_AddCustom(m, 0, y, p, downloadablessequence, desc);
			c->draw = MD_Draw;
			c->key = MD_Key;
			c->common.width = 320;
			c->common.height = 8;
			y += 8;

			if (!m->selecteditem)
				m->selecteditem = (menuoption_t*)c;
		}
	}

	//and then try to add any subdirs...
	for (p = availablepackages; p; p = p->next)
	{
		if (strncmp(p->category, pathprefix, prefixlen))
			continue;
		if ((p->flags & DPF_HIDDEN) && (p->arch || !(p->flags & DPF_ENABLED)))
			continue;

		slash = strchr(p->category+prefixlen, '/');
		if (slash)
		{
			Q_strncpyz(path, p->category, MAX_QPATH);
			slash = strchr(path+prefixlen, '/');
			if (slash)
				*slash = '\0';

			for (mo = m->options; mo; mo = mo->common.next)
				if (mo->common.type == mt_text/*mt_button*/)
					if (!strcmp(mo->button.text, path + prefixlen))
						break;
			if (!mo)
			{
				y += 8;
				MC_AddBufferedText(m, 48, 320, y, path+prefixlen, false, true);
				y += 8;
				Q_strncatz(path, "/", sizeof(path));
				y = MD_AddItemsToDownloadMenu(m, y, path);
			}
		}
	}
	return y;
}

#include "shader.h"
static void MD_Download_UpdateStatus(struct menu_s *m)
{
	dlmenu_t *info = m->data;
	int i, y;
	package_t *p;
	unsigned int totalpackages=0, selectedpackages=0, addpackages=0, rempackages=0;
	menuoption_t *si;
	menubutton_t *b, *d;
#ifdef WEBCLIENT
	unsigned int downloads=0;
	menucustom_t *c;
#endif

	if (info->downloadablessequence != downloadablessequence || !info->populated)
	{
		while(m->options)
		{
			menuoption_t *op = m->options;
			m->options = op->common.next;
			if (op->common.iszone)
				Z_Free(op);
		}
		m->cursoritem = m->selecteditem = NULL;
		info->downloadablessequence = downloadablessequence;

		info->populated = false;
		MC_AddWhiteText(m, 24, 320, 8, "Downloads", false)->text = info->titletext;
		MC_AddWhiteText(m, 16, 320, 24, "^Ue01d^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01f", false);

		//FIXME: should probably reselect the previous selected item. lets just assume everyone uses a mouse...
	}

	for (p = availablepackages; p; p = p->next)
	{
		if (p->alternative && (p->flags & DPF_HIDDEN))
			p = p->alternative;

		totalpackages++;
#ifdef WEBCLIENT
		if (p->download || p->trymirrors)
			downloads++;	//downloading or pending
#endif
		if (p->flags & DPF_MARKED)
		{
			if (p->flags & DPF_ENABLED)
			{
				selectedpackages++;
				if (p->flags & DPF_PURGE)
				{
					rempackages++;
					addpackages++;
				}
			}
			else
			{
				selectedpackages++;
				if (p->flags & DPF_PURGE)
					rempackages++;	//adding, but also deleting. how weird is that!
				addpackages++;
			}
		}
		else
		{
			if (p->flags & DPF_ENABLED)
				rempackages++;
			else
			{
				if (p->flags & DPF_PURGE)
					rempackages++;
			}
		}
	}

	//show status.
	if (cls.download)
	{	//we can actually download more than one thing at a time, but that makes the UI messy, so only show one active download here.
		if (cls.download->sizeunknown && cls.download->size == 0)
			Q_snprintfz(info->titletext, sizeof(info->titletext), "Downloads (%ukbps - %s)", CL_DownloadRate()/1000, cls.download->localname);
		else
			Q_snprintfz(info->titletext, sizeof(info->titletext), "Downloads (%u%% %ukbps - %s)", (int)cls.download->percent, CL_DownloadRate()/1000, cls.download->localname);
	}
	else if (!addpackages && !rempackages)
		Q_snprintfz(info->titletext, sizeof(info->titletext), "Downloads (%i of %i)", selectedpackages, totalpackages);
	else
		Q_snprintfz(info->titletext, sizeof(info->titletext), "Downloads (+%u -%u)", addpackages, rempackages);

	if (!info->populated)
	{
		for (i = 0; i < numdownloadablelists; i++)
		{
			if (!downloadablelist[i].received)
			{
				Draw_FunStringWidth(0, vid.height - 8, "Querying for package list", vid.width, 2, false);
				return;
			}
		}

		info->populated = true;
		MC_AddFrameStart(m, 48);
		y = 48;
		b = MC_AddCommand(m, 48, 170, y, "Apply", MD_ApplyDownloads);
		b->rightalign = false;
		b->common.tooltip = "Enable/Disable/Download/Delete packages to match any changes made (you will be prompted with a list of the changes that will be made).";
		y+=8;
		d = b = MC_AddCommand(m, 48, 170, y, "Back", MD_PopMenu);
		b->rightalign = false;
		y+=8;
#ifdef WEBCLIENT
		b = MC_AddCommand(m, 48, 170, y, "Mark Updates", MD_MarkUpdatesButton);
		b->rightalign = false;
		b->common.tooltip = "Select any updated versions of packages that are already installed.";
		y+=8;
#endif
		b = MC_AddCommand(m, 48, 170, y, "Revert Updates", MD_RevertUpdates);
		b->rightalign = false;
		b->common.tooltip = "Reset selection to only those packages that are currently installed.";
		y+=8;
#ifdef WEBCLIENT
		c = MC_AddCustom(m, 48, y, p, 0, NULL);
		c->draw = MD_AutoUpdate_Draw;
		c->key = MD_AutoUpdate_Key;
		c->common.width = 320;
		c->common.height = 8;
		y += 8;
#endif
		y+=4;	//small gap
		MD_AddItemsToDownloadMenu(m, y, info->pathprefix);
		if (!m->selecteditem)
			m->selecteditem = (menuoption_t*)d;
		m->cursoritem = (menuoption_t*)MC_AddWhiteText(m, 40, 0, m->selecteditem->common.posy, NULL, false);
		MC_AddFrameEnd(m, 48);
	}

	si = m->mouseitem;
	if (!si)
		si = m->selecteditem;
	if (si && si->common.type == mt_custom && si->custom.dptr)
	{
		package_t *p = si->custom.dptr;
		if (p->previewimage)
		{
			shader_t *sh = R_RegisterPic(p->previewimage, NULL);
			if (R_GetShaderSizes(sh, NULL, NULL, false) > 0)
				R2D_Image(0, 0, vid.width, vid.height, 0, 0, 1, 1, sh);
		}
	}
}

void Menu_DownloadStuff_f (void)
{
	menu_t *menu;
	dlmenu_t *info;

	Key_Dest_Add(kdm_emenu);

	menu = M_CreateMenu(sizeof(dlmenu_t));
	info = menu->data;

	menu->persist = true;
	menu->predraw = MD_Download_UpdateStatus;
	info->downloadablessequence = downloadablessequence;


	Q_strncpyz(info->pathprefix, Cmd_Argv(1), sizeof(info->pathprefix));
	if (!*info->pathprefix || !loadedinstalled)
		PM_UpdatePackageList(false, true);

	info->populated = false;	//will add any headers as needed
}

//should only be called AFTER the filesystem etc is inited.
void Menu_Download_Update(void)
{
	if (!pkg_autoupdate.ival)
		return;

	PM_UpdatePackageList(true, 2);
}
#else
void Menu_Download_Update(void)
{
#ifdef PACKAGEMANAGER
	PM_UpdatePackageList(true, 2);
#endif
}
void Menu_DownloadStuff_f (void)
{
	Con_Printf("Download menu not implemented in this build\n");
}
#endif
