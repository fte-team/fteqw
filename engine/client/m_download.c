#include "quakedef.h"
#ifndef MINIMAL

#define ROOTDOWNLOADABLESSOURCE "http://fteqw.sourceforge.net/downloadables.txt"
#define INSTALLEDFILES	"installed.lst"	//the file that resides in the quakedir (saying what's installed).

#define DPF_HAVEAVERSION	1	//any old version
#define DPF_WANTTOINSTALL	2	//user selected it
#define DPF_DISPLAYVERSION	4	//some sort of conflict, the package is listed twice, so show versions so the user knows what's old.
#define DPF_DELETEONUNINSTALL 8	//for previously installed packages, remove them from the list
#define DPF_DOWNLOADING 16


int dlcount=1;

extern char	*com_basedir;

//note: these are allocated for the life of the exe
char *downloadablelist[256] = {
	ROOTDOWNLOADABLESSOURCE
};
char *downloadablelistnameprefix[256] = {
	""
};
int numdownloadablelists = 1;

typedef struct package_s {
	char fullname[256];
	char *name;

	char src[256];
	char dest[64];
	char gamedir[16];
	unsigned int version;	//integral.

	int dlnum;

	int flags;
	struct package_s *next;
} package_t;

typedef struct {
	menucustom_t *list;
	char intermediatefilename[MAX_QPATH];
	int parsedsourcenum;

	int firstpackagenum;
	int highlightednum;
} dlmenu_t;

package_t *availablepackages;
int numpackages;

static package_t *BuildPackageList(FILE *f, int flags, char *prefix)
{
	char line[1024];
	package_t *p;
	package_t *first = NULL;
	char *sl;

	int version;

	do
	{
		fgets(line, sizeof(line)-1, f);
		while((sl=strchr(line, '\n')))
			*sl = '\0';
		while((sl=strchr(line, '\r')))
			*sl = '\0';
		Cmd_TokenizeString (line, false, false);
	} while (!feof(f) && !Cmd_Argc());

	if (strcmp(Cmd_Argv(0), "version"))
		return NULL;	//it's not the right format.

	version = atoi(Cmd_Argv(1));
	if (version != 0 && version != 1)
	{
		Con_Printf("Packagelist is of a future or incompatable version\n");
		return NULL;	//it's not the right version.
	}

	while(!feof(f))
	{
		if (!fgets(line, sizeof(line)-1, f))
			break;
		while((sl=strchr(line, '\n')))
			*sl = '\0';
		while((sl=strchr(line, '\r')))
			*sl = '\0';
		Cmd_TokenizeString (line, false, false);
		if (Cmd_Argc())
		{
			if (!strcmp(Cmd_Argv(0), "sublist"))
			{
				int i;
				sl = Cmd_Argv(1);

				for (i = 0; i < sizeof(downloadablelist)/sizeof(downloadablelist[0])-1; i++)
				{
					if (!downloadablelist[i])
						break;
					if (!strcmp(downloadablelist[i], sl))
						break;
				}
				if (!downloadablelist[i])
				{
					downloadablelist[i] = BZ_Malloc(strlen(sl)+1);
					strcpy(downloadablelist[i], sl);

					if (*prefix)
						sl = va("%s/%s", prefix, Cmd_Argv(2));
					else
						sl = Cmd_Argv(2);
					downloadablelistnameprefix[i] = BZ_Malloc(strlen(sl)+1);
					strcpy(downloadablelistnameprefix[i], sl);

					i++;
				}
				continue;
			}

			if (Cmd_Argc() > 5 || Cmd_Argc() < 3)
			{
				Con_Printf("Package list is bad - %s\n", line);
				continue;	//but try the next line away
			}

			p = BZ_Malloc(sizeof(*p));

			if (*prefix)
				Q_strncpyz(p->fullname, va("%s/%s", prefix, Cmd_Argv(0)), sizeof(p->fullname));
			else
				Q_strncpyz(p->fullname, Cmd_Argv(0), sizeof(p->fullname));
			p->name = p->fullname;
			while((sl = strchr(p->name, '/')))
				p->name = sl+1;

			Q_strncpyz(p->src, Cmd_Argv(1), sizeof(p->src));
			Q_strncpyz(p->dest, Cmd_Argv(2), sizeof(p->dest));
			p->version = atoi(Cmd_Argv(3));
			Q_strncpyz(p->gamedir, Cmd_Argv(4), sizeof(p->gamedir));
			if (!*p->gamedir)
				strcpy(p->gamedir, "id1");

			p->flags = flags;

			p->next = first;
			first = p;
		}
	}

	return first;
}

static void WriteInstalledPackages(void)
{
	package_t *p;
	char *fname = va("%s/%s", com_basedir, INSTALLEDFILES);
	FILE *f = fopen(fname, "wb");
	if (!f)
	{
		Con_Printf("menu_download: Can't update installed list\n");
		return;
	}

	fprintf(f, "version 1\n");
	for (p = availablepackages; p ; p=p->next)
	{
		if (p->flags & DPF_HAVEAVERSION)
			fprintf(f, "\"%s\" \"%s\" \"%s\" %i \"%s\"\n", p->fullname, p->src, p->dest, p->version, p->gamedir);
	}

	fclose(f);
}

static qboolean ComparePackages(package_t **l, package_t *p)
{
	int v = strcmp((*l)->fullname, p->fullname);
	if (v < 0)
	{
		p->next = (*l);
		(*l) = p;
		return true;
	}
	else if (v == 0)
	{
		if (p->version == (*l)->version)
		if (!strcmp((*l)->dest, p->dest))
		{ /*package matches, free, don't add*/
			strcpy((*l)->src, p->src);	//use the source of the new package (existing packages are read FIRST)
			(*l)->flags |= p->flags;
			(*l)->flags &= ~DPF_DELETEONUNINSTALL;
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
}
static void ConcatPackageLists(package_t *l2)
{
	package_t *n;
	while(l2)
	{
		n = l2->next;
		InsertPackage(&availablepackages, l2);
		l2 = n;

		numpackages++;
	}
}

static void dlnotification(char *localfile, qboolean sucess)
{
	int i;
	FILE *f;
	COM_RefreshFSCache_f();
	COM_FOpenFile(localfile, &f);
	if (f)
	{
		i = atoi(localfile+7);
		ConcatPackageLists(BuildPackageList(f, 0, downloadablelistnameprefix[i]));
		fclose(f);
	}
}

static void M_Download_Draw (int x, int y, struct menucustom_s *c, struct menu_s *m)
{
	int pn;

	int lastpathlen = 0;
	char *lastpath="";

	package_t *p;
	dlmenu_t *info = m->data;

	if (!cls.downloadmethod && (info->parsedsourcenum==-1 || downloadablelist[info->parsedsourcenum]))
	{	//done downloading
		char basename[64];

		info->parsedsourcenum++;

		if (downloadablelist[info->parsedsourcenum])
		{
			sprintf(basename, "dlinfo_%i.inf", info->parsedsourcenum);
			if (!HTTP_CL_Get(downloadablelist[info->parsedsourcenum], basename, dlnotification))
				Con_Printf("Could not contact server\n");
		}
	}
	if (!availablepackages)
	{
		Draw_String(x+8, y+8, "Could not obtain a package list");
		return;
	}

	y+=8;
	Draw_Alt_String(x+4, y, "I H");
	(info->highlightednum==0?Draw_Alt_String:Draw_String)(x+40, y, "Apply changes");
	y+=8;

	for (pn = 1, p = availablepackages; p && pn < info->firstpackagenum ; p=p->next, pn++)
		;
	for (; p; p = p->next, y+=8, pn++)
	{
		if (lastpathlen != p->name - p->fullname || strncmp(p->fullname, lastpath, lastpathlen))
		{
			lastpathlen = p->name - p->fullname;
			lastpath = p->fullname;


			if (!lastpathlen)
				Draw_FunStringLen(x+40, y, "/", 1);
			else
				Draw_FunStringLen(x+40, y, p->fullname, lastpathlen);
			y+=8;
		}
		Draw_Character (x, y, 128);
		Draw_Character (x+8, y, 130);
		Draw_Character (x+16, y, 128);
		Draw_Character (x+24, y, 130);

		//if you want it
		if (p->flags&DPF_WANTTOINSTALL)
			Draw_Character (x+4, y, 131);
		else
			Draw_Character (x+4, y, 129);

		//if you have it already
		if (p->flags&(DPF_HAVEAVERSION | ((((int)(realtime*4))&1)?DPF_DOWNLOADING:0) ))
			Draw_Character (x+20, y, 131);
		else
			Draw_Character (x+20, y, 129);

		if (pn == info->highlightednum)
			Draw_Alt_String(x+48, y, p->name);
		else
			Draw_String(x+48, y, p->name);

		if (p->flags & DPF_DISPLAYVERSION)
		{
			Draw_String(x+48+strlen(p->name)*8, y, va(" (%i.%i)", p->version/1000, p->version%1000));
		}
	}
}

static void Menu_Download_Got(char *fname, qboolean successful)
{
	package_t *p;
	int dlnum = atoi(fname+3);

	for (p = availablepackages; p ; p=p->next)
	{
		if (p->dlnum == dlnum)
		{
			char *destname;
			char *diskname = va("%s/%s", com_gamedir, fname);

			if (!successful)
			{
				p->flags &= ~DPF_DOWNLOADING;
				Con_Printf("Couldn't download %s (from %s to %s)\n", p->name, p->src, p->dest);
				return;
			}

			if (*p->gamedir)
				destname = va("%s/%s/%s", com_basedir, p->gamedir, p->dest);
			else
				destname = va("%s/%s", com_gamedir, p->dest);

			if (!(p->flags & DPF_DOWNLOADING))
			{
				Con_Printf("menu_download: We're not downloading %s, apparently\n", p->dest);
				return;
			}

			p->flags &= ~DPF_DOWNLOADING;


			if (!rename(diskname, destname))
			{
				Con_Printf("Couldn't rename %s to %s. Removed instead.\nPerhaps you already have it\n", diskname, destname);
				unlink(diskname);
				return;
			}
			Con_Printf("Downloaded %s (to %s)\n", p->name, destname);
			p->flags |= DPF_HAVEAVERSION;

			WriteInstalledPackages();

			FS_ReloadPackFiles();
			return;
		}
	}

	Con_Printf("menu_download: Can't figure out where %s came from\n", fname);
}

static qboolean M_Download_Key (struct menucustom_s *c, struct menu_s *m, int key)
{
	char *temp;
	int pn;
	package_t *p, *p2;
	dlmenu_t *info = m->data;

	switch (key)
	{
	case K_UPARROW:
		if (info->highlightednum>0)
			info->highlightednum--;
		return true;
	case K_DOWNARROW:	//cap range when drawing
		if (info->highlightednum < numpackages)
			info->highlightednum++;
		return true;
	case K_ENTER:
		if (!info->highlightednum)
		{	//do it
			//uninstall packages first
			package_t *last = NULL;

			for (p = availablepackages; p ; p=p->next)
			{
				if (!(p->flags&DPF_WANTTOINSTALL) && (p->flags&DPF_HAVEAVERSION))
				{	//if we don't want it but we have it anyway:
					char *fname = va("%s/%s", com_basedir, p->dest);
					unlink(fname);
					p->flags&=~DPF_HAVEAVERSION;	//FIXME: This is error prone.

					WriteInstalledPackages();

					if (p->flags & DPF_DELETEONUNINSTALL)
					{
						if (last)
							last->next = p->next;
						else
							availablepackages = p->next;

						BZ_Free(p);

						return M_Download_Key(c, m, key);	//I'm lazy.
					}
				}
				last = p;
			}

			for (p = availablepackages; p ; p=p->next)
			{
				if ((p->flags&DPF_WANTTOINSTALL) && !(p->flags&(DPF_HAVEAVERSION|DPF_DOWNLOADING)))
				{	//if we want it and don't have it:
					p->dlnum = dlcount++;
					temp = va("dl_%i.tmp", p->dlnum);
					Con_Printf("Downloading %s (to %s)\n", p->fullname, temp);
					COM_CreatePath(va("%s/%s", com_gamedir, p->dest));
					p->flags|=DPF_DOWNLOADING;
					if (!HTTP_CL_Get(p->src, temp, Menu_Download_Got))
						p->flags&=~DPF_DOWNLOADING;
				}
			}
		}
		else
		{
			for (pn = 1, p = availablepackages; p && pn < info->highlightednum ; p=p->next, pn++)
				;
			if (p)
			{
				p->flags = (p->flags&~DPF_WANTTOINSTALL) | DPF_WANTTOINSTALL - (p->flags&DPF_WANTTOINSTALL);

				if (p->flags&DPF_WANTTOINSTALL)
				{
					for (p2 = availablepackages; p2; p2 = p2->next)
					{
						if (p == p2)
							continue;
						if (!strcmp(p->dest, p2->dest))
							p2->flags &= ~DPF_WANTTOINSTALL;
					}
				}
			}
		}
		return true;
	}
	return false;
}

void Menu_DownloadStuff_f (void)
{
	menu_t *menu;
	dlmenu_t *info;

	key_dest = key_menu;
	m_state = m_complex;
	m_entersound = true;

	menu = M_CreateMenu(sizeof(dlmenu_t));
	info = menu->data;

	menu->selecteditem = (menuoption_t *)(info->list = MC_AddCustom(menu, 0, 32, NULL));
	info->list->draw = M_Download_Draw;
	info->list->key = M_Download_Key;

	info->parsedsourcenum = -1;

	MC_AddWhiteText(menu, 24, 8, "Downloads", false);
	MC_AddWhiteText(menu, 0, 16, "Probably buggy, press escape now and avoid this place!", false);
	MC_AddWhiteText(menu, 16, 24, "\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37", false);

	{
		static qboolean loadedinstalled;
		char *fname = va("%s/%s", com_basedir, INSTALLEDFILES);
		FILE *f = loadedinstalled?NULL:fopen(fname, "rb");
		loadedinstalled = true;
		if (f)
		{
			ConcatPackageLists(BuildPackageList(f, DPF_DELETEONUNINSTALL|DPF_HAVEAVERSION|DPF_WANTTOINSTALL, ""));
			fclose(f);
		}
	}
}
#endif
