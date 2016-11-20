#include "quakedef.h"

//client may remap messages from the server to a regional bit of text.
//server may remap progs messages

//basic language is english (cos that's what (my version of) Quake uses).
//translate is english->lang
//untranslate is lang->english for console commands.



char sys_language[64] = "";
static char langpath[MAX_OSPATH] = "";
struct language_s languages[MAX_LANGUAGES];

static void QDECL TL_LanguageChanged(struct cvar_s *var, char *oldvalue)
{
#ifndef CLIENTONLY
	svs.language = TL_FindLanguage(var->string);
#endif
#ifndef SERVERONLY
	cls.language = TL_FindLanguage(var->string);
#endif
}

cvar_t language = CVARFC("lang", sys_language, CVAR_USERINFO, TL_LanguageChanged);

void TranslateInit(void)
{
	Cvar_Register(&language, "International variables");
}

void TL_Shutdown(void)
{
	int j;

	for (j = 0; j < MAX_LANGUAGES; j++)
	{
		if (!languages[j].name)
			continue;
		free(languages[j].name);
		languages[j].name = NULL;
		PO_Close(languages[j].po);
		languages[j].po = NULL;
	}
}

static int TL_LoadLanguage(char *lang)
{
	vfsfile_t *f;
	int j;
	char *u;
	for (j = 0; j < MAX_LANGUAGES; j++)
	{
		if (!languages[j].name)
			break;
		if (!stricmp(languages[j].name, lang))
			return j;
	}

	//err... oops, ran out of languages...
	if (j == MAX_LANGUAGES)
		return 0;

	if (*lang)
		f = FS_OpenVFS(va("%sfteqw.%s.po", langpath, lang), "rb", FS_SYSTEM);
	else
		f = NULL;
	if (!f && *lang)
	{
		//keep truncating until we can find a name that works
		u = strrchr(lang, '_');
		if (u)
			*u = 0;
		else
			lang = "";
		return TL_LoadLanguage(lang);
	}
	languages[j].name = strdup(lang);
	languages[j].po = f?PO_Load(f):NULL;

	return j;
}
int TL_FindLanguage(const char *lang)
{
	char trimname[64];
	Q_strncpyz(trimname, lang, sizeof(trimname));
	return TL_LoadLanguage(trimname);
}

//need to set up default languages for any early prints before cvars are inited.
void TL_InitLanguages(const char *newlangpath)
{
	int i;
	char *lang;

	if (!newlangpath)
		newlangpath = "";

	Q_strncpyz(langpath, newlangpath, sizeof(langpath));

	//lang can override any environment or system settings.
	if ((i = COM_CheckParm("-lang")))
		Q_strncpyz(sys_language, com_argv[i+1], sizeof(sys_language));
	else
	{
		lang = NULL;
		if (!lang)
			lang = getenv("LANGUAGE");
		if (!lang)
			lang = getenv("LC_ALL");
		if (!lang)
			lang = getenv("LC_MESSAGES");
		if (!lang)
			lang = getenv("LANG");
		if (!lang)
			lang = "";
		if (!strcmp(lang, "C") || !strcmp(lang, "POSIX"))
			lang = "";

		//windows will have already set the locale from the windows settings, so only replace it if its actually valid.
		if (*lang)
			Q_strncpyz(sys_language, lang, sizeof(sys_language));
	}

	//clean it up.
	//takes the form: [language[_territory][.codeset][@modifier]]
	//we don't understand modifiers
	lang = strrchr(sys_language, '@');
	if (lang)
		*lang = 0;
	//we don't understand codesets sadly.
	lang = strrchr(sys_language, '.');
	if (lang)
		*lang = 0;
	//we also only support the single primary locale (no fallbacks, we're just using the language[+territory])
	lang = strrchr(sys_language, ':');
	if (lang)
		*lang = 0;
	//but we do support territories.
	
#ifndef CLIENTONLY
	svs.language = TL_FindLanguage(sys_language);
#endif
#ifndef SERVERONLY
	cls.language = TL_FindLanguage(sys_language);
#endif

	//make sure a fallback exists, but not as language 0
	TL_FindLanguage("");
}




#ifdef HEXEN2
//this stuff is for hexen2 translation strings.
//(hexen2 is uuuuggllyyyy...)
static char *strings_list;
static char **strings_table;
static int strings_count;
static qboolean strings_loaded;
void T_FreeStrings(void)
{	//on map change, following gamedir change
	if (strings_loaded)
	{
		BZ_Free(strings_list);
		BZ_Free(strings_table);
		strings_count = 0;
		strings_loaded = false;
	}
}
void T_LoadString(void)
{
	int i;
	char *s, *s2;
	//count new lines
	strings_loaded = true;
	strings_count = 0;
	strings_list = FS_LoadMallocFile("strings.txt", NULL);
	if (!strings_list)
		return;

	for (s = strings_list; *s; s++)
	{
		if (*s == '\n')
			strings_count++;
	}
	strings_table = BZ_Malloc(sizeof(char*)*strings_count);

	s = strings_list;
	for (i = 0; i < strings_count; i++)
	{
		strings_table[i] = s;
		s2 = strchr(s, '\n');
		if (!s2)
			break;

		while (s < s2)
		{
			if (*s == '\r')
				*s = '\0';
			else if (*s == '^' || *s == '@')	//becomes new line
				*s = '\n';
			s++;
		}
		s = s2+1;
		*s2 = '\0';
	}
}
char *T_GetString(int num)
{
	if (!strings_loaded)
	{
		T_LoadString();
	}
	if (num<0 || num >= strings_count)
		return "BAD STRING";

	return strings_table[num];
}

#ifndef SERVERONLY
//for hexen2's objectives and stuff.
static char *info_strings_list;
static char **info_strings_table;
static int info_strings_count;
static qboolean info_strings_loaded;
void T_FreeInfoStrings(void)
{	//on map change, following gamedir change
	if (info_strings_loaded)
	{
		BZ_Free(info_strings_list);
		BZ_Free(info_strings_table);
		info_strings_count = 0;
		info_strings_loaded = false;
	}
}
void T_LoadInfoString(void)
{
	int i;
	char *s, *s2;
	//count new lines
	info_strings_loaded = true;
	info_strings_count = 0;
	info_strings_list = FS_LoadMallocFile("infolist.txt", NULL);
	if (!info_strings_list)
		return;

	for (s = info_strings_list; *s; s++)
	{
		if (*s == '\n')
			info_strings_count++;
	}
	info_strings_table = BZ_Malloc(sizeof(char*)*info_strings_count);

	s = info_strings_list;
	for (i = 0; i < info_strings_count; i++)
	{
		info_strings_table[i] = s;
		s2 = strchr(s, '\n');
		if (!s2)
			break;

		while (s < s2)
		{
			if (*s == '\r')
				*s = '\0';
			else if (*s == '^' || *s == '@')	//becomes new line
				*s = '\n';
			s++;
		}
		s = s2+1;
		*s2 = '\0';
	}
}
char *T_GetInfoString(int num)
{
	if (!info_strings_loaded)
	{
		T_LoadInfoString();
	}
	if (num<0 || num >= info_strings_count)
		return "BAD STRING";

	return info_strings_table[num];
}
#endif
#endif

struct poline_s
{
	bucket_t buck;
	struct poline_s *next;
	char *orig;
	char *translated;
};

struct po_s
{
	hashtable_t hash;

	struct poline_s *lines;
};

const char *PO_GetText(struct po_s *po, const char *msg)
{
	struct poline_s *line;
	if (!po)
		return msg;
	line = Hash_Get(&po->hash, msg);
	if (line)
		return line->translated;
	return msg;
}
static void PO_AddText(struct po_s *po, const char *orig, const char *trans)
{
	size_t olen = strlen(orig)+1;
	size_t tlen = strlen(trans)+1;
	struct poline_s *line = Z_Malloc(sizeof(*line)+olen+tlen);
	memcpy(line+1, orig, olen);
	orig = (const char*)(line+1);
	line->translated = (char*)(line+1)+olen;
	memcpy(line->translated, trans, tlen);
	trans = (const char*)(line->translated);
	Hash_Add(&po->hash, orig, line, &line->buck);

	line->next = po->lines;
	po->lines = line;
}
struct po_s *PO_Load(vfsfile_t *file)
{
	struct po_s *po;
	unsigned int buckets = 1024;
	char *instart, *in, *end;
	int inlen;
	char msgid[32768];
	char msgstr[32768];

	qboolean allowblanks = !!COM_CheckParm("-translatetoblank");

	po = Z_Malloc(sizeof(*po) + Hash_BytesForBuckets(buckets));
	Hash_InitTable(&po->hash, buckets, po+1);

	inlen = file?VFS_GETLEN(file):0;
	instart = in = BZ_Malloc(inlen+1);
	if (file)
		VFS_READ(file, in, inlen);
	in[inlen] = 0;
	if (file)
		VFS_CLOSE(file);

	end = in + inlen;
	while(in < end)
	{
		while(*in == ' ' || *in == '\n' || *in == '\r' || *in == '\t')
			in++;
		if (*in == '#')
		{
			while (*in && *in != '\n')
				in++;
		}
		else if (!strncmp(in, "msgid", 5) && (in[5] == ' ' || in[5] == '\t' || in[5] == '\r' || in[5] == '\n'))
		{
			size_t start = 0;
			size_t ofs = 0;
			in += 5;
			while(1)
			{
				while(*in == ' ' || *in == '\n' || *in == '\r' || *in == '\t')
					in++;
				if (*in == '\"')
				{
					in = COM_ParseCString(in, msgid+start, sizeof(msgid) - start, &ofs);
					start += ofs;
				}
				else
					break;
			}
		}
		else if (!strncmp(in, "msgstr", 6) && (in[6] == ' ' || in[6] == '\t' || in[6] == '\r' || in[6] == '\n'))
		{
			size_t start = 0;
			size_t ofs = 0;
			in += 6;
			while(1)
			{
				while(*in == ' ' || *in == '\n' || *in == '\r' || *in == '\t')
					in++;
				if (*in == '\"')
				{
					in = COM_ParseCString(in, msgstr+start, sizeof(msgstr) - start, &ofs);
					start += ofs;
				}
				else
					break;
			}

			if ((*msgid && start) || allowblanks)
				PO_AddText(po, msgid, msgstr);
		}
		else
		{
			//some sort of junk?
			in++;
			while (*in && *in != '\n')
				in++;
		}
	}

	BZ_Free(instart);
	return po;
}
void PO_Close(struct po_s *po)
{
	if (!po)
		return;
	while(po->lines)
	{
		struct poline_s *r = po->lines;
		po->lines = r->next;
		Z_Free(r);
	}
	Z_Free(po);
}
