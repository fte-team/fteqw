#include "quakedef.h"

#if defined(WEBCLIENT) && !defined(NOBUILTINMENUS)
#define DOWNLOADMENU
#endif

#ifdef DOWNLOADMENU


//whole load of extra args for the downloads menu (for the downloads menu to handle engine updates).
#ifdef VKQUAKE
#define PHPVK "&vk=1"
#else
#define PHPVK
#endif
#ifdef GLQUAKE
#define PHPGL "&gl=1"
#else
#define PHPGL
#endif
#ifdef D3DQUAKE
#define PHPD3D "&d3d=1"
#else
#define PHPD3D
#endif
#ifdef MINIMAL
#define PHPMIN "&min=1"
#else
#define PHPMIN
#endif
#ifdef NOLEGACY
#define PHPLEG "&leg=0"
#else
#define PHPLEG "&leg=1"
#endif
#if defined(_DEBUG) || defined(DEBUG)
#define PHPDBG "&dbg=1"
#else
#define PHPDBG
#endif
#ifndef SVNREVISION
#define SVNREVISION -
#endif
#define DOWNLOADABLESARGS "?ver=" STRINGIFY(SVNREVISION) PHPVK PHPGL PHPD3D PHPMIN PHPLEG PHPDBG



extern cvar_t fs_downloads_url;
#define INSTALLEDFILES	"installed.lst"	//the file that resides in the quakedir (saying what's installed).

#define DPF_HAVEAVERSION		1	//any old version
#define DPF_WANTTOINSTALL		2	//user selected it
#define DPF_DISPLAYVERSION		4	//some sort of conflict, the package is listed twice, so show versions so the user knows what's old.
#define DPF_FORGETONUNINSTALL	8	//for previously installed packages, remove them from the list if there's no current version any more (should really be automatic if there's no known mirrors)
#define DPF_UNKNOWNVERSION		16	//we have a file with this name already, with no idea where it came from.
#define DPF_HIDDEN				32	//wrong arch, file conflicts, etc. still listed if actually installed.

void CL_StartCinematicOrMenu(void);

//note: these are allocated for the life of the exe
static char *downloadablelist[32];
static char *downloadablelistnameprefix[countof(downloadablelist)];
static char downloadablelistreceived[countof(downloadablelist)];	//well
static int numdownloadablelists = 0;

#define THISARCH PLATFORM "_" ARCH_CPU_POSTFIX

typedef struct package_s {
	char fullname[256];
	char *name;

	struct package_s *override;	//the package that obscures this one (later version, or whatever)

	unsigned int trymirrors;
	char *mirror[8];
	char gamedir[16];
	enum fs_relative fsroot;
	char version[16];
	char *arch;
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

			DEP_FILE
		} dtype;
		char name[1];
	} *deps;

	struct dl_download *download;

	int flags;
	struct package_s *next;
} package_t;

typedef struct {
	menucustom_t *list;
	char intermediatefilename[MAX_QPATH];
	char pathprefix[MAX_QPATH];
	int parsedsourcenum;
	qboolean populated;
} dlmenu_t;

static package_t *availablepackages;
static int numpackages;
#ifdef HAVEAUTOUPDATE
static int autoupdatesetting = -1;
#endif

static qboolean MD_CheckFile(const char *filename, enum fs_relative base)
{
	vfsfile_t *f = FS_OpenVFS(filename, "rb", base);
	if (f)
	{
		VFS_CLOSE(f);
		return true;
	}
	return false;
}
void MD_AddDep(package_t *p, int deptype, const char *depname)
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

static void M_DL_AddSubList(const char *url, const char *prefix)
{
	int i;

	for (i = 0; i < numdownloadablelists; i++)
	{
		if (!strcmp(downloadablelist[i], url))
			break;
	}
	if (i == numdownloadablelists && i < countof(downloadablelist))
	{
		downloadablelist[i] = BZ_Malloc(strlen(url)+1);
		strcpy(downloadablelist[i], url);

		downloadablelistnameprefix[i] = BZ_Malloc(strlen(prefix)+1);
		strcpy(downloadablelistnameprefix[i], prefix);

		numdownloadablelists++;
	}
}

static package_t *BuildPackageList(vfsfile_t *f, int flags, const char *url, const char *prefix)
{
	char line[1024];
	package_t *p, *o;
	package_t *first = NULL;
	struct packagedep_s *dep;
	char *sl;
	vfsfile_t *pf;

	int version;
	char defaultgamedir[64];
	char mirror[countof(p->mirror)][MAX_OSPATH];
	int nummirrors = 0;
	int argc;

	if (!f)
		return NULL;

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
		return NULL;	//it's not the right format.

	version = atoi(Cmd_Argv(1));
	if (version != 0 && version != 1 && version != 2)
	{
		Con_Printf("Packagelist is of a future or incompatible version\n");
		return NULL;	//it's not the right version.
	}

	while(1)
	{
		if (!VFS_GETS(f, line, sizeof(line)-1))
			break;
		while((sl=strchr(line, '\n')))
			*sl = '\0';
		while((sl=strchr(line, '\r')))
			*sl = '\0';
		Cmd_TokenizeString (line, false, false);
		argc = Cmd_Argc();
		if (argc)
		{
			if (!strcmp(Cmd_Argv(0), "sublist"))
			{
				char *subprefix;
				if (*prefix)
					subprefix = va("%s/%s", prefix, Cmd_Argv(2));
				else
					subprefix = Cmd_Argv(2);
				M_DL_AddSubList(Cmd_Argv(1), subprefix);
				continue;
			}
			if (!strcmp(Cmd_Argv(0), "set"))
			{
				if (!strcmp(Cmd_Argv(1), "gamedir"))
				{
					if (argc == 2)
						Q_strncpyz(defaultgamedir, FS_GetGamedir(false), sizeof(defaultgamedir));
					else
						Q_strncpyz(defaultgamedir, Cmd_Argv(2), sizeof(defaultgamedir));
				}
				else if (!strcmp(Cmd_Argv(1), "mirrors"))
				{
					nummirrors = 0;
					while (nummirrors < countof(mirror) && 2+nummirrors < argc)
					{
						Q_strncpyz(mirror[nummirrors], Cmd_Argv(2+nummirrors), sizeof(mirror[nummirrors]));
						if (!*mirror[nummirrors])
							break;
						nummirrors++;
					}
				}
				else
				{
					//erk
				}
				continue;
			}
			if (version == 2)
			{
				char *fullname = Cmd_Argv(0);
				char *file = NULL;
				char *url = NULL;
				char *gamedir = NULL;
				char *ver = NULL;
				int extract = EXTRACT_COPY;
				int i;

				p = Z_Malloc(sizeof(*p));
				for (i = 1; i < argc; i++)
				{
					char *arg = Cmd_Argv(i);
					if (!strncmp(arg, "url=", 4))
						url = arg+4;
					else if (!strncmp(arg, "gamedir=", 8))
						gamedir = arg+8;
					else if (!strncmp(arg, "ver=", 4))
						ver = arg+4;
					else if (!strncmp(arg, "v=", 2))
						ver = arg+2;
					else if (!strncmp(arg, "arch=", 5))
						/*arch = arg+5*/;
					else if (!strncmp(arg, "file=", 5))
					{
						if (!file)
							file = arg+5;
						MD_AddDep(p, DEP_FILE, arg+5);
					}
					else if (!strncmp(arg, "extract=", 8))
					{
						if (!strcmp(arg+8, "xz"))
							extract = EXTRACT_XZ;
						else if (!strcmp(arg+8, "gz"))
							extract = EXTRACT_GZ;
						else if (!strcmp(arg+8, "zip"))
							extract = EXTRACT_ZIP;
						else
							Con_Printf("Unknown decompression method: %s\n", arg+8);
					}
					else if (!strncmp(arg, "depend=", 7))
						MD_AddDep(p, DEP_REQUIRE, arg+7);
					else if (!strncmp(arg, "conflict=", 9))
						MD_AddDep(p, DEP_CONFLICT, arg+9);
					else if (!strncmp(arg, "fileconflict=", 13))
						MD_AddDep(p, DEP_FILECONFLICT, arg+13);
					else if (!strncmp(arg, "recommend=", 10))
						MD_AddDep(p, DEP_RECOMMEND, arg+10);
					else
					{
						Con_DPrintf("Unknown package property\n");
					}
				}

				if (*prefix)
					Q_snprintfz(p->fullname, sizeof(p->fullname), "%s/%s", prefix, fullname);
				else
					Q_snprintfz(p->fullname, sizeof(p->fullname), "%s", fullname);
				p->name = COM_SkipPath(p->fullname);

				if (!gamedir)
					gamedir = defaultgamedir;

				Q_strncpyz(p->version, ver?ver:"", sizeof(p->version));

				Q_snprintfz(p->gamedir, sizeof(p->gamedir), "%s", gamedir);
				p->fsroot = FS_ROOT;
				p->extract = extract;

				if (url && (!strncmp(url, "http://", 7) || !strncmp(url, "https://", 8)))
					p->mirror[0] = Z_StrDup(url);
				else
				{
					int m;
					char *ext = "";
					if (!url)
					{
						if (extract == EXTRACT_XZ)
							ext = ".xz";
						else if (extract == EXTRACT_GZ)
							ext = ".gz";
						else if (extract == EXTRACT_ZIP)
							ext = ".zip";
						url = file;
					}
					for (m = 0; m < nummirrors; m++)
						p->mirror[m] = Z_StrDup(va("%s%s%s", mirror[m], url, ext));
				}
			}
			else
			{
				if (argc > 5 || argc < 3)
				{
					Con_Printf("Package list is bad - %s\n", line);
					continue;	//but try the next line away
				}

				p = Z_Malloc(sizeof(*p));

				if (*prefix)
					Q_strncpyz(p->fullname, va("%s/%s", prefix, Cmd_Argv(0)), sizeof(p->fullname));
				else
					Q_strncpyz(p->fullname, Cmd_Argv(0), sizeof(p->fullname));
				p->name = p->fullname;
				while((sl = strchr(p->name, '/')))
					p->name = sl+1;

				p->mirror[0] = Z_StrDup(Cmd_Argv(1));
				MD_AddDep(p, DEP_FILE, Cmd_Argv(2));
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
			p->flags = flags;

			if (p->arch && Q_strcasecmp(p->arch, THISARCH))
				p->flags |= DPF_HIDDEN;
			for (dep = p->deps; dep; dep = dep->next)
			{
				if (dep->dtype == DEP_FILECONFLICT)
				{
					const char *n;
					if (*p->gamedir)
						n = va("%s/%s", p->gamedir, dep->name);
					else
						n = dep->name;
					if (MD_CheckFile(n, p->fsroot))
						p->flags |= DPF_HIDDEN;
				}
			}

			if (flags & DPF_HAVEAVERSION)
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
						VFS_CLOSE(pf);
					else
						Con_Printf("WARNING: %s (%s) no longer exists\n", p->fullname, n);
				}
			}
			else
			{
				for (dep = p->deps; dep; dep = dep->next)
				{
					char *n;
					struct packagedep_s *odep;
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

						for (o = availablepackages; o; o = o->next)
						{
							if (o->flags & DPF_HAVEAVERSION)
							{
								if (!strcmp(p->gamedir, o->gamedir) && p->fsroot == o->fsroot)
									if (strcmp(p->fullname, o->fullname) || strcmp(p->version, o->version))
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
						if (!o)
						{
							p->flags |= DPF_UNKNOWNVERSION;
							break;
						}
					}
				}
			}

			p->next = first;
			first = p;
		}
	}

	return first;
}

static void COM_QuotedConcat(const char *cat, char *buf, size_t bufsize)
{
	const unsigned char *gah;
	for (gah = (const unsigned char*)cat; *gah; gah++)
	{
		if (*gah <= ' ' || *gah == '$' || *gah == '\"')
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
static void WriteInstalledPackages(void)
{
	char *s;
	package_t *p;
	struct packagedep_s *dep;
	vfsfile_t *f = FS_OpenVFS(INSTALLEDFILES, "wb", FS_ROOT);
	if (!f)
	{
		Con_Printf("menu_download: Can't update installed list\n");
		return;
	}

	s = "version 2\n";
	VFS_WRITE(f, s, strlen(s));
	for (p = availablepackages; p ; p=p->next)
	{
		if (p->flags & DPF_HAVEAVERSION)
		{
			char buf[8192];
			buf[0] = 0;
			COM_QuotedString(p->fullname, buf, sizeof(buf), false);
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
			if (p->arch)
			{
				Q_strncatz(buf, " ", sizeof(buf));
				COM_QuotedConcat(va("arch=%s", p->arch), buf, sizeof(buf));
			}

			for (dep = p->deps; dep; dep = dep->next)
			{
				if (dep->dtype == DEP_FILE)
				{
					Q_strncatz(buf, " ", sizeof(buf));
					COM_QuotedConcat(va("file=%s", dep->name), buf, sizeof(buf));
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

			Q_strncatz(buf, "\n", sizeof(buf));
			VFS_WRITE(f, buf, strlen(buf));
		}
	}

	VFS_CLOSE(f);
}

static qboolean ComparePackages(package_t **l, package_t *p)
{
	int v = strcmp((*l)->fullname, p->fullname);
	if (v > 0)
	{
		p->next = (*l);
		(*l) = p;

		numpackages++;
		return true;
	}
	else if (v == 0)
	{
		if (!strcmp(p->version, (*l)->version))
		if (!strcmp(p->gamedir, (*l)->gamedir))
//		if (!strcmp((*l)->fullname, p->fullname))
		{ /*package matches, free the new one, don't add*/
			p->override = *l;
			return false;
		}

		p->flags |= DPF_DISPLAYVERSION;
		(*l)->flags |= DPF_DISPLAYVERSION;
	}
	return false;
}

static void InsertPackage(package_t **l, package_t *p)
{
	package_t *lp;
	if (!*l)	//there IS no list.
	{
		*l = p;
		p->next = NULL;

		numpackages++;
		return;
	}
	if (ComparePackages(l, p))
		return;
	for (lp = *l; lp->next; lp=lp->next)
	{
		if (ComparePackages(&lp->next, p))
			return;
	}
	lp->next = p;
	p->next = NULL;
	numpackages++;
}
static void ConcatPackageLists(package_t *l2)
{
	package_t *n;
	while(l2)
	{
		n = l2->next;
		l2->next = NULL;
		InsertPackage(&availablepackages, l2);
		l2 = n;
	}
}

static void M_DL_Notification(struct dl_download *dl)
{
	int i;
	vfsfile_t *f;
	f = dl->file;
	dl->file = NULL;

	i = dl->user_num;

	if (f)
	{
		downloadablelistreceived[i] = 1;
		ConcatPackageLists(BuildPackageList(f, 0, dl->url, downloadablelistnameprefix[i]));
		VFS_CLOSE(f);
	}
	else
		downloadablelistreceived[i] = -1;
}

static void MD_Draw (int x, int y, struct menucustom_s *c, struct menu_s *m)
{
	package_t *p;
	int fl;
	char *n;
	p = c->dptr;
	if (p)
	{
		fl = p->flags & (DPF_HAVEAVERSION | DPF_WANTTOINSTALL);
		if (p->flags & DPF_HIDDEN)
		{
			Draw_FunString (x+4, y, "---");
			return;
		}
		else if (p->download)
			Draw_FunString (x+4, y, va("%i", (int)p->download->qdownload.percent));
		else if (p->trymirrors)
			Draw_FunString (x+4, y, "PND");
		else
		{
			switch(fl)
			{
			case 0:
				if (p->flags & DPF_UNKNOWNVERSION)
					Draw_FunString (x, y, "???");
				else
				{
					Draw_FunString (x+4, y, "^Ue080^Ue082");
					Draw_FunString (x+8, y, "^Ue081");
				}
				break;
			case DPF_HAVEAVERSION:
				Draw_FunString (x, y, "DEL");
				break;
			case DPF_WANTTOINSTALL:
				Draw_FunString (x, y, "GET");
				break;
			case DPF_HAVEAVERSION | DPF_WANTTOINSTALL:
				Draw_FunString (x+4, y, "^Ue080^Ue082");
				Draw_FunString (x+8, y, "^Ue083");
				break;
			}
		}

		n = p->name;
		if (p->flags & DPF_DISPLAYVERSION)
			n = va("%s (%s)", n, *p->version?p->version:"unversioned");

		if (&m->selecteditem->common == &c->common)
			Draw_AltFunString (x+48, y, n);
		else
			Draw_FunString(x+48, y, n);
	}
}

//finds the newest version
static package_t *MD_FindPackage(char *packagename)
{
	package_t *p, *r = NULL;
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
//returns the installed version of a package, if any.
static package_t *MD_HavePackage(char *packagename)
{
	package_t *p;
	for (p = availablepackages; p; p = p->next)
	{
		if (p->flags & DPF_WANTTOINSTALL)
			if (!strcmp(p->name, packagename))
				return p;
	}
	return NULL;
}

//just flags, doesn't delete
static void MD_RemovePackage(package_t *package)
{
	package_t *o;
	struct packagedep_s *dep;

	if (!(package->flags & DPF_WANTTOINSTALL))
		return;	//looks like its already deselected.
	package->flags &= ~DPF_WANTTOINSTALL;

	//Is this safe?
	package->trymirrors = 0;	//if its enqueued, cancel that quickly...
	if (package->download)
	{					//if its currently downloading, cancel it.
		DL_Close(package->download);
		package->download = NULL;
	}

	//remove stuff that depends on us
	for (o = availablepackages; o; o = o->next)
	{
		for (dep = package->deps; dep; dep = dep->next)
			if (dep->dtype == DEP_REQUIRE)
				if (!strcmp(dep->name, package->name))
					MD_RemovePackage(o);
	}
}

//just flags, doesn't install
static void MD_AddPackage(package_t *package)
{
	package_t *o;
	struct packagedep_s *dep, *dep2;
	qboolean replacing = false;

	if (package->flags & DPF_WANTTOINSTALL)
		return;	//looks like its already picked.

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
			if (MD_CheckFile(n, package->fsroot))
				return;
		}
	}

	package->flags |= DPF_WANTTOINSTALL;

	//first check to see if we're replacing a different version of the same package
	for (o = availablepackages; o; o = o->next)
	{
		if (o == package)
			continue;

		if (o->flags & DPF_WANTTOINSTALL)
		{
			if (!strcmp(o->fullname, package->fullname))
			{	//replaces this package
				o->flags &= ~DPF_WANTTOINSTALL;
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
							MD_RemovePackage(o);
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
			package_t *d = MD_FindPackage(dep->name);
			if (d)
				MD_AddPackage(d);
		}
		if (dep->dtype == DEP_CONFLICT)
		{
			for (;;)
			{
				package_t *d = MD_HavePackage(dep->name);
				if (!d)
					break;
				MD_RemovePackage(d);
			}
		}
	}

	//remove any packages that conflict with us.
	for (o = availablepackages; o; o = o->next)
	{
		for (dep = package->deps; dep; dep = dep->next)
			if (dep->dtype == DEP_CONFLICT)
				if (!strcmp(dep->name, package->name))
					MD_RemovePackage(o);
	}
}
static qboolean MD_Key (struct menucustom_s *c, struct menu_s *m, int key, unsigned int unicode)
{
	package_t *p, *p2;
	struct packagedep_s *dep, *dep2;
	p = c->dptr;
	if (p->flags & DPF_HIDDEN)
		return false;
	if (key == K_ENTER || key == K_KP_ENTER || key == K_MOUSE1)
	{
		if (p->flags & DPF_WANTTOINSTALL)
			MD_RemovePackage(p);
//		else if (p->flags & DPF_UNKNOWNVERSION)
//			p->flags &= ~DPF_UNKNOWNVERSION;
		else
			MD_AddPackage(p);

		if (p->flags&DPF_WANTTOINSTALL)
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
							p2->flags &= ~DPF_WANTTOINSTALL;
							break;
						}
					}
				}
			}
		}
		else
			p->trymirrors = 0;
		return true;
	}

	return false;
}

#ifdef HAVEAUTOUPDATE
static void MD_EngineUpdate_Draw (int x, int y, struct menucustom_s *c, struct menu_s *m)
{
	char *settings[] = 
	{
		"Unsupported",
		"Revert",
		"Off",
		"Stable Updates",
		"Unsable Updates"
	};
	int setting = autoupdatesetting;
	char *text;
	if (setting == -1)
	{
		setting = Sys_GetAutoUpdateSetting();
		text = va("Auto Update: %s", settings[setting+1]);
	}
	else
		text = va("Auto Update: %s (unsaved)", settings[setting+1]);
	if (&m->selecteditem->common == &c->common)
		Draw_AltFunString (x+4, y, text);
	else
		Draw_FunString (x+4, y, text);
}
static qboolean MD_EngineUpdate_Key (struct menucustom_s *c, struct menu_s *m, int key, unsigned int unicode)
{
	if (key == K_ENTER || key == K_KP_ENTER || key == K_MOUSE1)
	{
		if (autoupdatesetting == -1)
			autoupdatesetting = Sys_GetAutoUpdateSetting();
		if (autoupdatesetting != -1)
		{
			autoupdatesetting+=1;
			if (autoupdatesetting >= 4)
				autoupdatesetting = 0;
		}
	}
	return false;
}
#endif

qboolean MD_PopMenu (union menuoption_s *mo,struct menu_s *m,int key)
{
	if (key == K_ENTER || key == K_KP_ENTER || key == K_MOUSE1)
	{
		M_RemoveMenu(m);
		return true;
	}
	return false;
}

vfsfile_t *FS_XZ_DecompressWriteFilter(vfsfile_t *infile);
vfsfile_t *FS_GZ_DecompressWriteFilter(vfsfile_t *outfile, qboolean autoclosefile);

static char *MD_GetTempName(package_t *p)
{
	struct packagedep_s *dep;
	char *destname, *t, *ts;
	//always favour a file so that we can rename safely without needing a copy.
	for (dep = p->deps; dep; dep = dep->next)
	{
		if (dep->dtype != DEP_FILE)
			continue;
		if (*p->gamedir)
			destname = va("%s/%s.tmp", p->gamedir, dep->name);
		else
			destname = va("%s.tmp", dep->name);
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

static void Menu_Download_Got(struct dl_download *dl);
static void MD_StartADownload(void)
{
	vfsfile_t *tmpfile;
	char *temp;
//	char native[MAX_OSPATH];
	package_t *p;
	int simultaneous = 1;
	int i;

	for (p = availablepackages; p ; p=p->next)
	{
		if (p->download)
			simultaneous--;
	}

	for (p = availablepackages; p && simultaneous > 0; p=p->next)
	{
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
				continue;
			}
		
			temp = MD_GetTempName(p);

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
					tmpfile = FS_GZ_DecompressWriteFilter(raw, true);
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
				p->download = HTTP_CL_Get(mirror, NULL, Menu_Download_Got);
			if (p->download)
			{
				Con_Printf("Downloading %s\n", p->fullname);
				p->download->file = tmpfile;
				p->download->user_ctx = temp;

#ifdef MULTITHREAD
				DL_CreateThread(p->download, NULL, NULL);
#endif
			}
			else
			{
				Con_Printf("Unable to download %s\n", p->fullname);
				p->flags &= ~DPF_WANTTOINSTALL;	//can't do it.
				if (tmpfile)
					VFS_CLOSE(tmpfile);
				FS_Remove(temp, p->fsroot);
			}

			simultaneous--;
		}
	}
}

static qboolean MD_ApplyDownloads (union menuoption_s *mo,struct menu_s *m,int key)
{
	if (key == K_ENTER || key == K_KP_ENTER || key == K_MOUSE1)
	{
		package_t *p, *o, **link;

#ifdef HAVEAUTOUPDATE
		if (autoupdatesetting != -1)
		{
			Sys_SetAutoUpdateSetting(autoupdatesetting);
			autoupdatesetting = -1;
		}
#endif

		//delete any that don't exist
		for (link = &availablepackages; *link ; )
		{
			p = *link;
			if (!(p->flags&DPF_WANTTOINSTALL) && (p->flags&DPF_HAVEAVERSION))
			{	//if we don't want it but we have it anyway:
				qboolean reloadpacks = false;
				struct packagedep_s *dep;
				for (dep = p->deps; dep; dep = dep->next)
				{
					if (dep->dtype == DEP_FILE)
					{
						if (!reloadpacks)
						{
							char ext[8];
							COM_FileExtension(dep->name, ext, sizeof(ext));
							if (!stricmp(ext, "pak") || !stricmp(ext, "pk3"))
							{
								reloadpacks = true;
								FS_UnloadPackFiles();
							}
						}
						if (*p->gamedir)
							FS_Remove(va("%s/%s", p->gamedir, dep->name), p->fsroot);
						else
							FS_Remove(dep->name, p->fsroot);
					}
				}
				if (reloadpacks)
					FS_ReloadPackFiles();

				p->flags &= ~DPF_UNKNOWNVERSION;
				p->flags &= ~DPF_HAVEAVERSION;

				//make sure it actually got wiped. if there's still a file there then something went screwy.
				//we don't reliably know if the remove actually succeeded or failed.
				for (dep = p->deps; dep; dep = dep->next)
				{
					if (dep->dtype == DEP_FILE)
					{
						const char *n;
						if (*p->gamedir)
							n = va("%s/%s", p->gamedir, dep->name);
						else
							n = dep->name;
						if (MD_CheckFile(n, p->fsroot))
						{
							p->flags |= DPF_UNKNOWNVERSION;
							break;
						}
					}
				}
				WriteInstalledPackages();

				if (p->flags & DPF_FORGETONUNINSTALL)
				{
					*link = p->next;

					for (o = availablepackages; o; o = o->next)
					{
						if (o->override == p)
							o->override = NULL;
					}

					p->flags |= DPF_HIDDEN;
//					BZ_Free(p);

					continue;
				}
			}

			link = &(*link)->next;
		}

		//and flag any new/updated ones for a download
		for (p = availablepackages; p ; p=p->next)
		{
			if ((p->flags&DPF_WANTTOINSTALL) && !(p->flags&DPF_HAVEAVERSION) && !p->download)
				p->trymirrors = ~0u;
		}
		MD_StartADownload();	//and try to do those downloads.
		return true;
	}
	return false;
}

void M_AddItemsToDownloadMenu(menu_t *m)
{
	char path[MAX_QPATH];
	int y;
	package_t *p;
	menucustom_t *c;
	char *slash;
	menuoption_t *mo;
	dlmenu_t *info = m->data;
	int prefixlen;
	p = availablepackages;

	prefixlen = strlen(info->pathprefix);
	y = 48;
	
	MC_AddCommand(m, 0, 170, y, "Apply", MD_ApplyDownloads);
	y+=8;
	MC_AddCommand(m, 0, 170, y, "Back", MD_PopMenu);
	y+=8;
#ifdef HAVEAUTOUPDATE
	if (!prefixlen)
	{
		c = MC_AddCustom(m, 0, y, p, 0);
		c->draw = MD_EngineUpdate_Draw;
		c->key = MD_EngineUpdate_Key;
		c->common.width = 320;
		c->common.height = 8;
		y += 8;
	}
#endif

	y+=4;	//small gap
	for (p = availablepackages; p; p = p->next)
	{
		if (strncmp(p->fullname, info->pathprefix, prefixlen))
			continue;
		if ((p->flags & DPF_HIDDEN) && !(p->flags & DPF_HAVEAVERSION))
			continue;
		if (p->override)
			continue;

		slash = strchr(p->fullname+prefixlen, '/');
		if (slash)
		{
			Q_strncpyz(path, p->fullname, MAX_QPATH);
			slash = strchr(path+prefixlen, '/');
			if (slash)
				*slash = '\0';

			for (mo = m->options; mo; mo = mo->common.next)
				if (mo->common.type == mt_button)
					if (!strcmp(mo->button.text, path + prefixlen))
						break;
			if (!mo)
			{
				menubutton_t *b = MC_AddConsoleCommand(m, 6*8, 170, y, path+prefixlen, va("menu_download \"%s/\"", path));
				y += 8;

				if (!m->selecteditem)
					m->selecteditem = (menuoption_t*)b;
			}
		}
		else
		{
			c = MC_AddCustom(m, 0, y, p, 0);
			c->draw = MD_Draw;
			c->key = MD_Key;
			c->common.width = 320;
			c->common.height = 8;
			y += 8;

			if (!m->selecteditem)
				m->selecteditem = (menuoption_t*)c;
		}
	}
}

void M_Download_UpdateStatus(struct menu_s *m)
{
	struct dl_download *dl;
	dlmenu_t *info = m->data;
	int i;

	while (!cls.download && (info->parsedsourcenum==-1 || info->parsedsourcenum < numdownloadablelists))
	{	//done downloading
		info->parsedsourcenum++;

		if (info->parsedsourcenum < numdownloadablelists)
		{
			if (!downloadablelistreceived[info->parsedsourcenum])
			{
				dl = HTTP_CL_Get(va("%s"DOWNLOADABLESARGS, downloadablelist[info->parsedsourcenum]), NULL, M_DL_Notification);
				if (dl)
				{
					dl->user_num = info->parsedsourcenum;

					dl->file = VFSPIPE_Open();
					dl->isquery = true;
				}
				else
					Con_Printf("Could not contact server\n");
				return;
			}
		}
	}
	for (i = 0; i < numdownloadablelists; i++)
	{
		if (!downloadablelistreceived[i])
		{
//			Draw_String(x+8, y+8, "Waiting for package list");
			return;
		}
	}

	if (!info->populated)
	{
		info->populated = true;
		M_AddItemsToDownloadMenu(m);
	}
}

#include "fs.h"
static int QDECL MD_ExtractFiles(const char *fname, qofs_t fsize, time_t mtime, void *parm, searchpathfuncs_t *spath)
{	//this is gonna suck. threading would help, but gah.
	package_t *p = parm;
	flocation_t loc;
	if (spath->FindFile(spath, &loc, fname, NULL) && loc.len < 0x80000000u)
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
			FS_WriteFile(n, f, loc.len, p->fsroot);
			free(f);

			//keep track of the installed files, so we can delete them properly after.
			MD_AddDep(p, DEP_FILE, fname);
		}
	}
	return 1;
}

static void Menu_Download_Got(struct dl_download *dl)
{
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
			MD_StartADownload();
			return;
		}

		if (p->extract == EXTRACT_ZIP)
		{
			vfsfile_t *f = FS_OpenVFS(tempname, "rb", p->fsroot);
			if (f)
			{
				searchpathfuncs_t *archive = FSZIP_LoadArchive(f, tempname, NULL);
				if (archive)
				{
					archive->EnumerateFiles(archive, "*", MD_ExtractFiles, p);
					archive->ClosePath(archive);

					p->flags |= DPF_HAVEAVERSION;
					WriteInstalledPackages();

					if (!stricmp(ext, "pak") || !stricmp(ext, "pk3"))
						FS_ReloadPackFiles();
				}
				else
					VFS_CLOSE(f);
			}
			FS_Remove (tempname, FS_GAMEONLY);
			Z_Free(tempname);
			MD_StartADownload();
			return;
		}
		else
		{
			for (dep = p->deps; dep; dep = dep->next)
			{
				if (dep->dtype != DEP_FILE)
					continue;

				COM_FileExtension(dep->name, ext, sizeof(ext));
				if (!stricmp(ext, "pak") || !stricmp(ext, "pk3"))
					FS_UnloadPackFiles();	//we reload them after

				if (*p->gamedir)
					destname = va("%s/%s", p->gamedir, dep->name);
				else
					destname = dep->name;
				if (FS_Remove(destname, p->fsroot))
					;
				if (!FS_Rename2(tempname, destname, p->fsroot, p->fsroot))
				{
					Con_Printf("Couldn't rename %s to %s. Removed instead.\n", tempname, destname);
					FS_Remove (tempname, p->fsroot);
					Z_Free(tempname);
					MD_StartADownload();
					return;
				}
				Z_Free(tempname);
				Con_Printf("Downloaded %s (to %s)\n", p->name, destname);

				p->flags |= DPF_HAVEAVERSION;

				WriteInstalledPackages();

				if (!stricmp(ext, "pak") || !stricmp(ext, "pk3"))
					FS_ReloadPackFiles();

				MD_StartADownload();
				return;
			}
		}
		Con_Printf("menu_download: %s has no filename info\n", p->name);
	}
	else
		Con_Printf("menu_download: Can't figure out where %s came from (url: %s)\n", dl->localname, dl->url);

	FS_Remove (tempname, FS_GAMEONLY);
	Z_Free(tempname);
	MD_StartADownload();
}

void Menu_DownloadStuff_f (void)
{
	static qboolean loadedinstalled;
	int i;
	menu_t *menu;
	dlmenu_t *info;

	Key_Dest_Add(kdm_emenu);
	m_state = m_complex;

	menu = M_CreateMenu(sizeof(dlmenu_t));
	info = menu->data;

	menu->predraw = M_Download_UpdateStatus;
/*
	menu->selecteditem = (menuoption_t *)(info->list = MC_AddCustom(menu, 0, 32, NULL));
	info->list->draw = M_Download_Draw;
	info->list->key = M_Download_Key;
*/
	info->parsedsourcenum = -1;

	if (*fs_downloads_url.string)
		M_DL_AddSubList(fs_downloads_url.string, "");

	Q_strncpyz(info->pathprefix, Cmd_Argv(1), sizeof(info->pathprefix));
	if (!*info->pathprefix)
	{
		for (i = 0; i < numdownloadablelists; i++)
			downloadablelistreceived[i] = 0;
	}

	MC_AddWhiteText(menu, 24, 170, 8, "Downloads", false);
	MC_AddWhiteText(menu, 16, 170, 24, "^Ue01d^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01f", false);

	if (!loadedinstalled)
	{
		vfsfile_t *f = FS_OpenVFS(INSTALLEDFILES, "rb", FS_ROOT);
		loadedinstalled = true;
		if (f)
		{
			ConcatPackageLists(BuildPackageList(f, DPF_FORGETONUNINSTALL|DPF_HAVEAVERSION|DPF_WANTTOINSTALL, NULL, ""));
			VFS_CLOSE(f);
		}
	}
}
#else
void Menu_DownloadStuff_f (void)
{
	Con_Printf("Download menu not implemented in this build\n");
}
#endif
