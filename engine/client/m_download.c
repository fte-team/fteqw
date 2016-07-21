#include "quakedef.h"

#if defined(WEBCLIENT) && !defined(NOBUILTINMENUS)
#define DOWNLOADMENU
#endif

#ifdef DOWNLOADMENU

extern cvar_t fs_downloads_url;
#define INSTALLEDFILES	"installed.lst"	//the file that resides in the quakedir (saying what's installed).

#define DPF_HAVEAVERSION		1	//any old version
#define DPF_WANTTOINSTALL		2	//user selected it
#define DPF_DISPLAYVERSION		4	//some sort of conflict, the package is listed twice, so show versions so the user knows what's old.
#define DPF_FORGETONUNINSTALL	8	//for previously installed packages, remove them from the list if there's no current version any more (should really be automatic if there's no known mirrors)
#define DPF_UNKNOWNVERSION		16	//we have a file with this name already, with no idea where it came from.

void CL_StartCinematicOrMenu(void);

//note: these are allocated for the life of the exe
static char *downloadablelist[32];
static char *downloadablelistnameprefix[countof(downloadablelist)];
static char downloadablelistreceived[countof(downloadablelist)];	//well
static int numdownloadablelists = 0;

typedef struct package_s {
	char fullname[256];
	char *name;

	unsigned int trymirrors;
	char *mirror[8];
	char dest[MAX_QPATH];
	char gamedir[16];
	enum fs_relative fsroot;
	char version[16];
	int extract;

	struct packagedep_s
	{
		struct packagedep_s *next;
		enum
		{
			DEP_CONFLICT,
			DEP_REQUIRE,
			DEP_RECOMMEND,	//like depend, but uninstalling will not bubble.
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
				struct packagedep_s *deps = NULL, *nd;
				int extract = 0;
				int i;
				for (i = 1; i < argc; i++)
				{
					char *arg = Cmd_Argv(i);
					if (!strncmp(arg, "file=", 5))
						file = arg+5;
					else if (!strncmp(arg, "url=", 4))
						url = arg+4;
					else if (!strncmp(arg, "gamedir=", 8))
						gamedir = arg+8;
					else if (!strncmp(arg, "ver=", 4))
						ver = arg+4;
					else if (!strncmp(arg, "v=", 2))
						ver = arg+2;
//					else if (!strncmp(arg, "arch=", 5))
//						arch = arg+5;
					else if (!strncmp(arg, "extract=", 8))
					{
						if (!strcmp(arg+8, "xz"))
							extract = 1;
						else if (!strcmp(arg+8, "gz"))
							extract = 2;
						else
							Con_Printf("Unknown decompression method: %s\n", arg+8);
					}
					else if (!strncmp(arg, "depend=", 7))
					{
						arg += 7;
						nd = Z_Malloc(sizeof(*nd) + strlen(arg));
						nd->dtype = DEP_REQUIRE;
						strcpy(nd->name, arg);
						nd->next = deps;
						deps = nd;
					}
					else if (!strncmp(arg, "conflict=", 9))
					{
						arg += 9;
						nd = Z_Malloc(sizeof(*nd) + strlen(arg));
						nd->dtype = DEP_CONFLICT;
						strcpy(nd->name, arg);
						nd->next = deps;
						deps = nd;
					}
					else if (!strncmp(arg, "recommend=", 10))
					{
						arg += 10; 
						nd = Z_Malloc(sizeof(*nd) + strlen(arg));
						nd->dtype = DEP_RECOMMEND;
						strcpy(nd->name, arg);
						nd->next = deps;
						deps = nd;
					}
					else
						break;
				}

				p = Z_Malloc(sizeof(*p));
				if (*prefix)
					Q_snprintfz(p->fullname, sizeof(p->fullname), "%s/%s", prefix, Cmd_Argv(0));
				else
					Q_snprintfz(p->fullname, sizeof(p->fullname), "%s", Cmd_Argv(0));
				p->name = COM_SkipPath(p->fullname);

				if (!file)
					file = p->name;
				if (!gamedir)
					gamedir = defaultgamedir;

				Q_snprintfz(p->dest, sizeof(p->dest), "%s", file);
				Q_strncpyz(p->version, ver?ver:"", sizeof(p->version));

				Q_snprintfz(p->gamedir, sizeof(p->gamedir), "%s", gamedir);
				p->fsroot = FS_ROOT;
				p->extract = extract;
				p->deps = deps;

				if (url && (!strncmp(url, "http://", 7) || !strncmp(url, "https://", 8)))
					p->mirror[0] = Z_StrDup(url);
				else
				{
					int m;
					char *ext = "";
					if (!url)
					{
						if (extract == 1)
							ext = ".xz";
						else if (extract == 2)
							ext = ".gz";
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
				Q_strncpyz(p->dest, Cmd_Argv(2), sizeof(p->dest));
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

			if (*p->gamedir)
				pf = FS_OpenVFS(va("%s/%s", p->gamedir, p->dest), "rb", p->fsroot);
			else
				pf = FS_OpenVFS(p->dest, "rb", p->fsroot);
			if (pf)
				VFS_CLOSE(pf);

			if (flags & DPF_HAVEAVERSION)
			{
				if (!pf)
					Con_Printf("WARNING: %s no longer exists\n", p->fullname);
			}
			else
			{
				for (o = availablepackages; o; o = o->next)
				{
					if (o->flags & DPF_HAVEAVERSION)
						if (!strcmp(p->dest, o->dest) && p->fsroot == o->fsroot)
							if (strcmp(p->fullname, o->fullname) || strcmp(p->version, o->version))
								break;
				}
				if (pf && !o)
					flags |= DPF_UNKNOWNVERSION;
			}

			p->flags = flags;

			p->next = first;
			first = p;
		}
	}

	return first;
}

static void WriteInstalledPackages(void)
{
	char *s;
	package_t *p;
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
			s = va("\"%s\" \"file=%s\" \"ver=%s\" \"gamedir=%s\"\n", p->fullname, p->dest, p->version, p->gamedir);
			VFS_WRITE(f, s, strlen(s));
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
		if (!strcmp((*l)->dest, p->dest))
		{ /*package matches, free, don't add*/
			int i;
			for (i = 0; i < countof(p->mirror); i++)
			{
				Z_Free((*l)->mirror[i]);
				(*l)->mirror[i] = p->mirror[i];
			}
			(*l)->extract = p->extract;
			(*l)->flags |= p->flags;
			(*l)->flags &= ~DPF_FORGETONUNINSTALL;
			BZ_Free(p);
			return true;
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
		if (p->download)
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
				Draw_FunString (x, y, "REM");
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
	struct packagedep_s *dep;
	qboolean replacing = false;

	if (package->flags & DPF_WANTTOINSTALL)
		return;	//looks like its already picked.
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
				if (!strcmp(o->dest, package->dest))
					MD_RemovePackage(o);
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
	p = c->dptr;
	if (key == K_ENTER || key == K_KP_ENTER || key == K_MOUSE1)
	{
		if (p->flags & DPF_UNKNOWNVERSION)
			p->flags &= ~DPF_UNKNOWNVERSION;
		else if (p->flags & DPF_WANTTOINSTALL)
			MD_RemovePackage(p);
		else
			MD_AddPackage(p);

		if (p->flags&DPF_WANTTOINSTALL)
		{
			//any other packages that conflict should be flagged for uninstall now that this one will replace it.
			for (p2 = availablepackages; p2; p2 = p2->next)
			{
				if (p == p2)
					continue;
				if (!strcmp(p->dest, p2->dest))
					p2->flags &= ~DPF_WANTTOINSTALL;
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
		
			if (*p->gamedir)
				temp = va("%s/%s.tmp", p->gamedir, p->dest);
			else
				temp = va("%s.tmp", p->dest);

			//FIXME: we should lock in the temp path, in case the user foolishly tries to change gamedirs.

			FS_CreatePath(temp, p->fsroot);
			switch (p->extract)
			{
			case 0:
				tmpfile = FS_OpenVFS(temp, "wb", p->fsroot);
				break;
#ifdef AVAIL_XZDEC
			case 1:
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
			case 2:
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

static qboolean MD_CheckFile(char *filename, enum fs_relative base)
{
	vfsfile_t *f = FS_OpenVFS(filename, "rb", base);
	if (f)
	{
		VFS_CLOSE(f);
		return true;
	}
	return false;
}

static qboolean MD_ApplyDownloads (union menuoption_s *mo,struct menu_s *m,int key)
{
	if (key == K_ENTER || key == K_KP_ENTER || key == K_MOUSE1)
	{
		package_t *last = NULL, *p;

#ifdef HAVEAUTOUPDATE
		if (autoupdatesetting != -1)
		{
			Sys_SetAutoUpdateSetting(autoupdatesetting);
			autoupdatesetting = -1;
		}
#endif

		//delete any that don't exist
		for (p = availablepackages; p ; p=p->next)
		{
			if (!(p->flags&DPF_WANTTOINSTALL) && (p->flags&DPF_HAVEAVERSION))
			{	//if we don't want it but we have it anyway:

				char ext[8];
				COM_FileExtension(p->dest, ext, sizeof(ext));
				if (!stricmp(ext, "pak") || !stricmp(ext, "pk3"))
				{
					FS_UnloadPackFiles();
					FS_Remove(p->dest, p->fsroot);
					FS_ReloadPackFiles();
				}
				else
				{
					FS_Remove(p->dest, p->fsroot);
				}

				//if its no longer readable then we were successful.
				//this should give 'success' in the case of it being readonly or not existing in the first place (which is why we didn't depend upon FS_Remove succeeding).
				if (!MD_CheckFile(p->dest, p->fsroot))
				{
					p->flags&=~DPF_HAVEAVERSION;
					WriteInstalledPackages();
				}

				if (p->flags & DPF_FORGETONUNINSTALL)
				{
					if (last)
						last->next = p->next;
					else
						availablepackages = p->next;

				//	BZ_Free(p);

					return true;
				}
			}
			last = p;
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
				dl = HTTP_CL_Get(downloadablelist[info->parsedsourcenum], NULL, M_DL_Notification);
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

static void Menu_Download_Got(struct dl_download *dl)
{
	qboolean successful = dl->status == DL_FINISHED;
	char ext[8];
	package_t *p;

	if (dl->file)
	{
		VFS_CLOSE(dl->file);
		dl->file = NULL;
	}

	for (p = availablepackages; p ; p=p->next)
	{
		if (p->download == dl)
		{
			char *destname;
			char *tempname;
			p->download = NULL;

			if (!successful)
			{
				Con_Printf("Couldn't download %s (from %s to %s)\n", p->name, dl->url, p->dest);
				if (*p->gamedir)
					destname = va("%s/%s", p->gamedir, p->dest);
				else
					destname = va("%s", p->dest);
				tempname = va("%s.tmp", destname);
				FS_Remove (tempname, p->fsroot);
				MD_StartADownload();
				return;
			}

			COM_FileExtension(p->dest, ext, sizeof(ext));
			if (!stricmp(ext, "pak") || !stricmp(ext, "pk3"))
				FS_UnloadPackFiles();	//we reload them after

			if (*p->gamedir)
				destname = va("%s/%s", p->gamedir, p->dest);
			else
				destname = va("%s", p->dest);
			tempname = va("%s.tmp", destname);
			if (FS_Remove(destname, p->fsroot))
				;
			if (!FS_Rename2(tempname, destname, p->fsroot, p->fsroot))
			{
				Con_Printf("Couldn't rename %s to %s. Removed instead.\n", tempname, destname);
				FS_Remove (tempname, p->fsroot);
				MD_StartADownload();
				return;
			}
			Con_Printf("Downloaded %s (to %s)\n", p->name, destname);
			p->flags |= DPF_HAVEAVERSION;

			WriteInstalledPackages();

			if (!stricmp(ext, "pak") || !stricmp(ext, "pk3"))
				FS_ReloadPackFiles();

			MD_StartADownload();
			return;
		}
	}

	Con_Printf("menu_download: Can't figure out where %s came from (url: %s)\n", dl->localname, dl->url);

	FS_Remove (dl->localname, FS_GAMEONLY);
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
