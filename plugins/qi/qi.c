#include "../plugin.h"

#include "../jabber/xml.h"

#define DATABASEURL		"https://www.quaddicted.com/reviews/quaddicted_database.xml"
#define FILEIMAGEURL	"https://www.quaddicted.com/reviews/screenshots/%s_injector.jpg"
#define FILEDOWNLOADURL	"https://www.quaddicted.com/filebase/%s.zip"
#define WINDOWTITLE		"Quaddicted Map+Mod Archive"
#define WINDOWNAME		"QI"

/*
<file id="downloadname" type="1=map. 2=mod" rating="5-star-rating">
	<author>meh</author>
	<title>some readable name</title>
	<md5sum>xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx</md5sum>
	<size>size-in-kb</size>
	<date>dd.mm.yy</date>
	<description>HTML-encoded text. just to be awkward</description>
	<techinfo>
		<zipbasedir>additional path needed to make it relative to the quake directory</zipbasedir>
		<requirements>
			<file id="quoth" />
		</requirements>
	</techinfo>
</file>
*/

xmltree_t *thedatabase;
qhandle_t dlcontext = -1;

struct 
{
	char namefilter[256];
	int minrating;
	int maxrating;
	int type;
} filters;

static void Con_SubPrintf(const char *subname, char *format, ...)
{
	va_list		argptr;
	static char		string[8192];

	va_start (argptr, format);
	Q_vsnprintf (string, sizeof(string), format,argptr);
	va_end (argptr);

	pCon_SubPrint(subname, string);
}


static qintptr_t QI_Shutdown(qintptr_t *args)
{
	if (dlcontext != -1)
	{	//we're still downloading something? :o
		pFS_Close(dlcontext);
		dlcontext = -1;
	}
	if (thedatabase)
		XML_Destroy(thedatabase);
	thedatabase = NULL;
	return false;
}

static qboolean QI_SetupWindow(const char *console, qboolean force)
{
	if (!BUILTINISVALID(Con_GetConsoleFloat))
		return false;

	//only redraw the window if it actually exists. if they closed it, then don't mess things up.
	if (!force && pCon_GetConsoleFloat(console, "iswindow") <= 0)
		return false;

	if (pCon_GetConsoleFloat(console, "iswindow") != true)
	{
		pCon_SetConsoleString(console, "title", WINDOWTITLE);
		pCon_SetConsoleFloat(console, "iswindow", true);
		pCon_SetConsoleFloat(console, "forceutf8", true);
		pCon_SetConsoleFloat(console, "linebuffered", false);
		pCon_SetConsoleFloat(console, "maxlines", 16384);	//the line limit is more a sanity thing than anything else. so long as we explicitly clear before spamming more, then its not an issue...
		pCon_SetConsoleFloat(console, "wnd_x", 8);
		pCon_SetConsoleFloat(console, "wnd_y", 8);
		pCon_SetConsoleFloat(console, "wnd_w", pvid.width-16);
		pCon_SetConsoleFloat(console, "wnd_h", pvid.height-16);
		pCon_SetConsoleString(console, "footer", "");
	}
	pCon_SetConsoleFloat(console, "linecount", 0);	//clear it
	if (force)
		pCon_SetActive(console);
	return true;
}
static void QI_DeHTML(const char *in, char *out, size_t outsize)
{
	outsize--;
	while(*in && outsize > 0)
	{
		if (*in == '\r' || *in == '\n')
			in++;
		else if (*in == '<')
		{
			char tag[256];
			int i;
			qboolean open = false;
			qboolean close = false;
			in++;
			if (*in == '/')
			{
				in++;
				close = true;
			}
			else
				open = true;
			while (*in == ' ' || *in == '\n' || *in == '\t' || *in == '\r')
				in++;
			for (i = 0; i < countof(tag)-1; )
			{
				if (*in == '>')
					break;
				if (!*in || *in == ' ' || *in == '\n' || *in == '\t' || *in == '\r' || (in[0] == '/' && in[1] == '>'))
					break;
				tag[i++] = *in++;
			}
			tag[i] = 0;
			while (*in && *in)
			{
				if (*in == '/' && in[1] == '>')
				{
					in += 2;
					close = true;
					break;
				}
				if (*in++ == '>')
					break;
			}
			if (!strcmp(tag, "br") && open && outsize > 0)
			{	//new lines!
				*out++ = '\n';
				outsize--;
			}
			else if (!strcmp(tag, "b") && open != close && outsize > 1)
			{	//bold
				*out++ = '^';
				outsize--;
				*out++ = 'a';
				outsize--;
			}
			else if (!strcmp(tag, "i") && open != close && outsize > 1)
			{	//italics
				*out++ = '^';
				outsize--;
				*out++ = close?'7':'3';
				outsize--;
			}
		}
		else if (*in == '^')
		{
			if (outsize > 1)
			{
				*out++ = *in;
				outsize--;
				*out++ = *in++;
				outsize--;
			}
		}
		else
		{
			*out++ = *in++;
			outsize--;
		}
	}
	*out = 0;
}

static char *QI_strcasestr(const char *haystack, const char *needle)
{
	int c1, c2, c2f;
	int i;
	c2f = *needle;
	if (c2f >= 'a' && c2f <= 'z')
		c2f -= ('a' - 'A');
	if (!c2f)
		return (char*)haystack;
	while (1)
	{
		c1 = *haystack;
		if (!c1)
			return NULL;
		if (c1 >= 'a' && c1 <= 'z')
			c1 -= ('a' - 'A');
		if (c1 == c2f)
		{
			for (i = 1; ; i++)
			{
				c1 = haystack[i];
				c2 = needle[i];
				if (c1 >= 'a' && c1 <= 'z')
					c1 -= ('a' - 'A');
				if (c2 >= 'a' && c2 <= 'z')
					c2 -= ('a' - 'A');
				if (!c2)
					return (char*)haystack;	//end of needle means we found a complete match
				if (!c1)	//end of haystack means we can't possibly find needle in it any more
					return NULL;
				if (c1 != c2)	//mismatch means no match starting at haystack[0]
					break;
			}
		}
		haystack++;
	}
	return NULL;	//didn't find it
}

static void QI_RefreshMapList(qboolean forcedisplay)
{
	xmltree_t *file;
	const char *console = WINDOWNAME;
	char descbuf[1024];
	if (!QI_SetupWindow(console, forcedisplay))
		return;

	if (!thedatabase)
	{
		if (dlcontext != -1)
			Con_SubPrintf(console, "Downloading database...\n");
		else
			Con_SubPrintf(console, "Unable to download HTTP database\n");
		return;
	}

	if (!thedatabase->child)
	{
		Con_SubPrintf(console, "No maps in database... SPIRIT! YOU BROKE IT!\n");
		return;
	}

	for (file = thedatabase->child; file; file = file->sibling)
	{
		const char *id = XML_GetParameter(file, "id", "unnamed");
		const char *rating = XML_GetParameter(file, "rating", "");
		int ratingnum = atoi(rating);
		const char *author = XML_GetChildBody(file, "author", "unknown");
		const char *desc = XML_GetChildBody(file, "description", "<NO DESCRIPTION>");
		const char *type;
		const char *date;
		int year, month, day;
		int startmapnum, i;
		char ratingtext[65];
		xmltree_t *tech;
		xmltree_t *startmap;
		if (strcmp(file->name, "file"))
			continue;	//erk?
		if (atoi(XML_GetParameter(file, "hide", "")) || atoi(XML_GetParameter(file, "fte_hide", "")))
			continue;
		type = XML_GetParameter(file, "type", "");
		if (filters.type && atoi(type) != filters.type)
			continue;
		switch(atoi(type))
		{
		case 1:
			type = "map";
			break;
		case 2:
			type = "mod";
			break;
		case 5:
			type = "otr";
			break;
		default:
			type = "???";
			break;
		}

		if (filters.maxrating>=0 && ratingnum > filters.maxrating)
			continue;
		if (filters.minrating>=0 && ratingnum < filters.minrating)
			continue;

		tech = XML_ChildOfTree(file, "techinfo", 0);
		//if the filter isn't contained in the id/desc then don't display it.
		if (*filters.namefilter)
		{
			if (!QI_strcasestr(id, filters.namefilter) && !QI_strcasestr(desc, filters.namefilter) && !QI_strcasestr(author, filters.namefilter))
			{
				//check map list too
				for (startmapnum = 0; ; startmapnum++)
				{
					startmap = XML_ChildOfTree(tech, "startmap", startmapnum);
					if (!startmap)
						break;
					if (QI_strcasestr(startmap->body, filters.namefilter))
						break;
				}
				if (!startmap)
					continue;
			}
		}

		if (ratingnum > (sizeof(ratingtext)-5)/6)
			ratingnum = (sizeof(ratingtext)-5)/6;
		if (ratingnum)
		{
			Q_snprintf(ratingtext, sizeof(ratingtext), "^a");
			for (i = 0; i < ratingnum; i++)
				Q_snprintf(ratingtext + i+2, sizeof(ratingtext)-i*2+2, "*");
			Q_snprintf(ratingtext + i+2, sizeof(ratingtext)-i*2+2, "^a");
		}
		else if (*rating)
			Q_snprintf(ratingtext, sizeof(ratingtext), "%s", rating);
		else
			Q_snprintf(ratingtext, sizeof(ratingtext), "%s", "unrated");

		
		date = XML_GetChildBody(file, "date", "1.1.1990");
		day = atoi(date?date:"1");
		date = date?strchr(date, '.'):NULL;
		month = atoi(date?date+1:"1");
		date = date?strchr(date, '.'):NULL;
		year = atoi(date?date+1:"1990");
		if (year < 90)
			year += 2000;
		else if (year < 1900)
			year += 1900;
		Q_snprintf(descbuf, sizeof(descbuf), "Id: %s\nAuthor: %s\nDate: %04u-%02u-%02u\nRating: %s\n\n", id, author, year, month, day, ratingtext);

		QI_DeHTML(desc, descbuf + strlen(descbuf), sizeof(descbuf) - strlen(descbuf));
		desc = descbuf;

		for (startmapnum = 0; ; startmapnum++)
		{
			startmap = XML_ChildOfTree(tech, "startmap", startmapnum);
			if (!startmap)
				break;
			Con_SubPrintf(console, "%s ^[%s (%s)\\tip\\%s\\tipimg\\"FILEIMAGEURL"\\id\\%s\\startmap\\%s^]\n", type, XML_GetChildBody(file, "title", "<NO TITLE>"), startmap->body, desc, id, id, startmap->body);
		}
		if (!startmapnum)
			Con_SubPrintf(console, "%s ^[%s\\tip\\%s\\tipimg\\"FILEIMAGEURL"\\id\\%s^]\n", type, XML_GetChildBody(file, "title", "<NO TITLE>"), desc, id, id);
	}

	Con_SubPrintf(console, "\nFilter:\n");
	if (*filters.namefilter)
		Con_SubPrintf(console, "Contains: %s ", filters.namefilter);
	Con_SubPrintf(console, "^[Change Filter^]\n");
	Con_SubPrintf(console, "^[Maps^] %s\n", (filters.type!=2)?"shown":"hidden");
	Con_SubPrintf(console, "^[Mods^] %s\n", (filters.type!=1)?"shown":"hidden");
	if (filters.minrating == filters.maxrating)
	{
		char *gah[] = {"Any Rating", "Unrated", "1","2","3","4","5"};
		int i;
		Con_SubPrintf(console, "Rating");
		for (i = 0; i < countof(gah); i++)
		{
			if (i == filters.minrating+1)
				Con_SubPrintf(console, " %s", gah[i]);
			else
				Con_SubPrintf(console, " ^[%s^]", gah[i]);
		}
		Con_SubPrintf(console, "\n");
	}
	else
	{
		if (filters.minrating)
			Con_SubPrintf(console, "Min Rating: %i stars\n", filters.minrating);
		if (filters.maxrating)
			Con_SubPrintf(console, "Max Rating: %i stars\n", filters.maxrating);
	}
}

static void QI_UpdateFilter(char *filtertext)
{
	if (!strcmp(filtertext, "all"))
	{
		filters.type = 0;
		filters.minrating = filters.maxrating = -1;
		Q_strlcpy(filters.namefilter, "", sizeof(filters.namefilter));
	}
	else if (*filtertext == '>')
		filters.minrating = atoi(filtertext+1);
	else if (*filtertext == '<')
		filters.maxrating = atoi(filtertext+1);
	else if (*filtertext == '=')
		filters.minrating = filters.maxrating = atoi(filtertext+1);
	else if (!strcmp(filtertext, "any"))
	{
		filters.type = 0;
		filters.minrating = filters.maxrating = -1;
	}
	else if (!strcmp(filtertext, "maps"))
		filters.type = 1;
	else if (!strcmp(filtertext, "mods"))
		filters.type = 2;
	else
		Q_strlcpy(filters.namefilter, filtertext, sizeof(filters.namefilter));
}

static xmltree_t *QI_FindArchive(const char *name)
{
	xmltree_t *file;
	for (file = thedatabase->child; file; file = file->sibling)
	{
		const char *id = XML_GetParameter(file, "id", "unnamed");
		if (strcmp(file->name, "file"))
			continue;	//erk?

		if (!strcmp(id, name))
			return file;
	}
	return NULL;
}
static void QI_AddPackages(xmltree_t *qifile)
{
	const char *id;
	char extra[1024];
	char clean[512];
	unsigned int i;

	xmltree_t *tech;
	const char *basedir;
	xmltree_t *requires;
	xmltree_t *req;
	//quaddicted's database contains various zips that are meant to be extracted to various places.
	//this can either be the quake root (no path), a mod directory (typically the name of the mod), or the maps directory (id1/maps or some such)
	//unfortunately, quake files are relative to the subdir, so we need to strip the first subdir. anyone that tries to put dlls in there is evil. and if there isn't one, we need to get the engine to compensate.
	//we also need to clean up the paths so they're not badly formed
	tech = XML_ChildOfTree(qifile, "techinfo", 0);
	basedir = XML_GetChildBody(tech, "zipbasedir", "");

	//skip any dodgy leading slashes
	while (*basedir == '/' || *basedir == '\\')
		basedir++;
	if (!*basedir)
		strcpy(clean, "..");	//err, there wasn't a directory... we still need to 'strip' it though.
	else
	{
		//skip the gamedir
		while (*basedir && *basedir != '/' && *basedir != '\\')
			basedir++;
		//skip any trailing
		while (*basedir == '/' || *basedir == '\\')
			basedir++;
		for (i = 0; *basedir; i++)
		{
			if (i >= sizeof(clean)-1)
				break;
			if (*basedir == '\\')	//sigh
				clean[i] = '/';
			else
				clean[i] = *basedir;
			basedir++;
		}
		while (i > 0 && clean[i-1] == '/')
			i--;
		clean[i] = 0;
	}

	requires = XML_ChildOfTree(tech, "requirements", 0);
	if (requires)
	{
		for (i = 0; ; i++)
		{
			req = XML_ChildOfTree(requires, "file", i);
			if (!req)
				break;
			id = XML_GetParameter(req, "id", "unknown");
			QI_AddPackages(QI_FindArchive(id));
		}
	}

	id = XML_GetParameter(qifile, "id", "unknown");
	if (strchr(clean, '\\') || strchr(clean, '\"') || strchr(clean, '\n') || strchr(clean, ';'))
		return;
	if (strchr(id, '\\') || strchr(id, '\"') || strchr(id, '\n') || strchr(id, ';'))
		return;


	Q_snprintf(extra, sizeof(extra), " package \""FILEDOWNLOADURL"\" prefix \"%s\"", id, clean);
	pCmd_AddText(extra, false);
}
static void QI_RunMap(xmltree_t *qifile, const char *map)
{
	if (!qifile)
	{
		return;
	}

	//type 1 (maps) often don't list any map names
	if (!*map)// && atoi(XML_GetParameter(qifile, "type", "0")) == 1)
		map = XML_GetParameter(qifile, "id", "unknown");
	if (!*map || strchr(map, '\\') || strchr(map, '\"') || strchr(map, '\n') || strchr(map, ';'))
		map = "";


	pCmd_AddText("fs_changemod map \"", false);
	pCmd_AddText(map, false);
	pCmd_AddText("\"", false);
	QI_AddPackages(qifile);
//	Con_Printf("Command: %s\n", cmd);
	pCmd_AddText("\n", false);
}

static qintptr_t QI_ConsoleLink(qintptr_t *args)
{
	xmltree_t *file;
	char *map;
	char *id;
	char *e;
	char text[2048];
	char link[8192];
	pCmd_Argv(0, text, sizeof(text));
	pCmd_Argv(1, link, sizeof(link));

	if (!strcmp(text, "Change Filter") && !*link)
	{
		const char *console = WINDOWNAME;
		pCon_SetConsoleFloat(console, "linebuffered", true);
		pCon_SetConsoleString(console, "footer", "Please enter filter:");
		return true;
	}
	if (!strcmp(text, "Maps") && !*link)
	{
		filters.type = (filters.type==2)?0:2;
		QI_RefreshMapList(true);
		return true;
	}
	if (!strcmp(text, "Mods") && !*link)
	{
		filters.type = (filters.type==1)?0:1;
		QI_RefreshMapList(true);
		return true;
	}
	if (!strcmp(text, "Any Rating") && !*link)
	{
		filters.minrating = filters.maxrating = -1;
		QI_RefreshMapList(true);
		return true;
	}
	if ((atoi(text) || !strcmp(text, "Unrated")) && !*link)
	{
		filters.minrating = filters.maxrating = atoi(text);
		QI_RefreshMapList(true);
		return true;
	}


	id = strstr(link, "\\id\\");
	map = strstr(link, "\\startmap\\");
	if (id)
	{
		id+=4;
		e = strchr(id, '\\');
		if (e)
			*e = 0;
		if (map)
		{
			map += 10;
			e = strchr(map, '\\');
			if (e)
				*e = 0;
		}
		else
			map = "";

		file = QI_FindArchive(id);
		if (!file)
		{
			Con_Printf("Unknown file \"%s\"\n", id);
			return true;
		}
		QI_RunMap(file, map);
		return true;
	}
	return false;
}
static qintptr_t QI_Tick(qintptr_t *args)
{
	if (dlcontext != -1)
	{
		unsigned int flen;
		if (pFS_GetLen(dlcontext, &flen, NULL))
		{
			int ofs = 0;
			char *file;
			qboolean archive = true;
			if (flen == 0)
			{
				pFS_Close(dlcontext);
				flen = pFS_Open("**plugconfig", &dlcontext, 1);
				if (dlcontext == -1)
				{	
					QI_RefreshMapList(false);
					return false;
				}
				archive = false;
			}
			file = malloc(flen+1);
			file[flen] = 0;
			pFS_Read(dlcontext, file, flen);
			pFS_Close(dlcontext);
			if (archive)
			{
				pFS_Open("**plugconfig", &dlcontext, 2);
				if (dlcontext != -1)
				{
					pFS_Write(dlcontext, file, flen);
					pFS_Close(dlcontext);
				}
			}
			dlcontext = -1;
			do
			{
				if (thedatabase)
					XML_Destroy(thedatabase);
				thedatabase = XML_Parse(file, &ofs, flen, false, "");
			} while(thedatabase && !thedatabase->child);
			free(file);

			QI_RefreshMapList(false);

//			XML_ConPrintTree(thedatabase, "quadicted_xml", 0);
		}
	}
	return false;
}

static qintptr_t QI_ConExecuteCommand(qintptr_t *args)
{
	char console[256];
	char filter[256];
	pCmd_Argv(0, console, sizeof(console));
	pCmd_Args(filter, sizeof(filter));
	QI_UpdateFilter(filter);

	QI_RefreshMapList(true);
	pCon_SetConsoleFloat(console, "linebuffered", false);
	pCon_SetConsoleString(console, "footer", "");
	return true;
}

static qintptr_t QI_ExecuteCommand(qintptr_t *args)
{
	char cmd[256];
	pCmd_Argv(0, cmd, sizeof(cmd));
	if (!strcmp(cmd, "qi") || !strcmp(cmd, "quaddicted"))
	{
		if (pCmd_Argc() > 1)
		{
			pCmd_Args(cmd, sizeof(cmd));
			QI_UpdateFilter(cmd);
		}
		else if (QI_SetupWindow(WINDOWNAME, false))
		{
			pCon_SetActive(WINDOWNAME);
			return true;
		}

		if (!thedatabase && dlcontext == -1)
			pFS_Open(DATABASEURL, &dlcontext, 1);

		QI_RefreshMapList(true);
		return true;
	}
	return false;
}

extern void (*Con_TrySubPrint)(const char *conname, const char *message);
qintptr_t Plug_Init(qintptr_t *args)
{
	filters.minrating = filters.maxrating = -1;
	Con_TrySubPrint = pCon_SubPrint;
	if (Plug_Export("Tick", QI_Tick) &&
		Plug_Export("Shutdown", QI_Shutdown) &&
		Plug_Export("ExecuteCommand", QI_ExecuteCommand) &&
		Plug_Export("ConExecuteCommand", QI_ConExecuteCommand) &&
		Plug_Export("ConsoleLink", QI_ConsoleLink))
	{
		pCmd_AddCommand("qi");
		pCmd_AddCommand("quaddicted");
		return true;
	}
	return false;
}