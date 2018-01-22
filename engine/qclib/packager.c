#include "qcc.h"
#include <time.h>
void QCC_Canonicalize(char *fullname, size_t fullnamesize, const char *newfile, const char *base);

/*
dataset common
{
	output default data.pk3
	output logic textures.pk3
}
dataset desktop
{
	output tex textures_pc.pk3
}
dataset mobile
{
	output tex textures_mobile.pk3
}
input pak0.pk3

rule dxt1 {
 dataset desktop
 output tex
 newext dds
 command "@\"c:/program files/Compressonator/CompressonatorCLI\" -fd DXT1 $input $output"
}

rule etc2 {
 dataset mobile
 output tex
 newext ktx
 command "@\"c:/program files/Compressonator/CompressonatorCLI\" -fd ETC2 $input $output"
}

logic {
	progs.dat
}
class texa0 {
	output tex
	desktop: dxt1
	mobile: etc2
}
texa0
{
	gfx/conback.txt 
}
*/

#define countof(x) (sizeof(x)/sizeof((x)[0]))

struct pkgctx_s
{
	void (*messagecallback)(void *userctx, char *message, ...);
	void *userctx;

	char *listfile;

	pbool readoldpacks;
	char gamepath[MAX_PATH];
	time_t buildtime;

	//skips the file if its listed in one of these packages, unless the modification time on disk is newer.
	struct oldpack_s
	{
		struct oldpack_s *next;
		char filename[128];
		size_t numfiles;
		struct
		{
			char name[128];
			size_t size;
//			unsigned int zcrc;
//			timestamp;
		} *file;
	} *oldpacks;

	struct dataset_s
	{
		struct dataset_s *next;

		//these are the output pk3s from this package.
		struct output_s
		{
			struct output_s *next;
			char code[128];
			char filename[128];
			struct file_s *files;
		} *outputs;

		char name[1];
	} *datasets;
	struct rule_s
	{
		struct rule_s *next;
		char name[128];

		int dropfile:1;

		char *newext;
		char *command;
	} *rules;

	struct class_s
	{
		char name[128];
		struct class_s *next;

		//the output package codename to write to. class is skipped if the dataset doesn't include that name.
		char outname[128];

		struct
		{
			struct dataset_s *set;
			struct rule_s *rule;
		} dataset[8];
		struct rule_s *defaultrule;

		struct file_s
		{
			struct file_s *next;
			char name[128];

			//temp data for tracking what's getting written.
			struct
			{
				char name[128];
				struct file_s *nextwrite;
				struct rule_s *rule;
				unsigned int zcrc;
				unsigned int zhdrofs;
				unsigned int pakofs;
				unsigned int rawsize;
				unsigned int zipsize;
			} write;
		} *files;
	} *classes;
};

static struct rule_s *PKG_FindRule(struct pkgctx_s *ctx, char *code)
{
	struct rule_s *o;
	for (o = ctx->rules; o; o = o->next)
	{
		if (!strcmp(o->name, code))
			return o;
	}
	return NULL;
}
static struct class_s *PKG_FindClass(struct pkgctx_s *ctx, char *code)
{
	struct class_s *c;
	for (c = ctx->classes; c; c = c->next)
	{
		if (!strcmp(c->name, code))
			return c;
	}
	return NULL;
}
static struct dataset_s *PKG_FindDataset(struct pkgctx_s *ctx, char *code)
{
	struct dataset_s *o;
	for (o = ctx->datasets; o; o = o->next)
	{
		if (!strcmp(o->name, code))
			return o;
	}
	return NULL;
}
static struct dataset_s *PKG_GetDataset(struct pkgctx_s *ctx, char *code)
{
	struct dataset_s *s = PKG_FindDataset(ctx, code);
	if (!s)
	{
		s = malloc(sizeof(*s)+strlen(code));
		strcpy(s->name, code);
		s->outputs = NULL;
		s->next = ctx->datasets;
		ctx->datasets = s;
	}
	return s;
}

static pbool PKG_SkipWhite(struct pkgctx_s *ctx, pbool linebreak)
{
	for(;;)
	{
		if (qcc_iswhite(*ctx->listfile))
		{
			if (qcc_islineending(ctx->listfile[0], ctx->listfile[1]) && !linebreak)
				return false;
			ctx->listfile++;
			continue;
		}
		if (ctx->listfile[0] == '/' && ctx->listfile[1] == '/')
		{
			while (!qcc_islineending(ctx->listfile[0], ctx->listfile[1]))
				ctx->listfile++;
			continue;
		}
		if (ctx->listfile[0] == '/' && ctx->listfile[1] == '*')
		{
			ctx->listfile+=2;
			while (*ctx->listfile)
			{
				if (ctx->listfile[0]=='*' && ctx->listfile[1]=='/')
				{
					ctx->listfile+=2;
					break;
				}
				ctx->listfile++;
			}
			continue;
		}
		break;
	}
	return true;
}
static pbool PKG_GetToken(struct pkgctx_s *ctx, char *token, size_t sizeoftoken, pbool linebreak)
{
	if (!PKG_SkipWhite(ctx, linebreak))
		return false;
	if (*ctx->listfile)
	{
		while(*ctx->listfile)
		{
			if (qcc_iswhite(*ctx->listfile))
				break;
			*token = *ctx->listfile++;
			if (sizeoftoken > 1)
			{
				token++;
				sizeoftoken--;
			}
		}
		*token = 0;
		return true;
	}
	return false;
}

static pbool PKG_GetStringToken(struct pkgctx_s *ctx, char *token, size_t sizeoftoken)
{
	if (!PKG_SkipWhite(ctx, false))
		return false;
	if (*ctx->listfile == '\"')
	{
		ctx->listfile++;
		while(*ctx->listfile)
		{
			if (*ctx->listfile == '\"')
			{
				ctx->listfile++;
				break;
			}
			else if (*ctx->listfile == '\\')
			{
				ctx->listfile++;
				switch(*ctx->listfile++)
				{
				case '\"':	*token = '\"'; break;
				case '\\':	*token = '\\'; break;
				case '\r':	*token = '\r'; break;
				case '\n':	*token = '\n'; break;
				case '\t':	*token = '\t'; break;
				default:	*token = '?'; break;
				}
				sizeoftoken--;
			}
			else
				*token = *ctx->listfile++;
			if (sizeoftoken > 1)
			{
				token++;
				sizeoftoken--;
			}
		}
		*token = 0;
		return true;
	}
	return false;
}

static pbool PKG_Expect(struct pkgctx_s *ctx, char *token)
{
	char tok[128];
	if (PKG_GetToken(ctx, tok, sizeof(tok), true))
	{
		if (!strcmp(tok, token))
			return true;
	}
	ctx->messagecallback(ctx->userctx, "Expected '%s', found '%s'\n", token, tok);
	return false;
}

static void PKG_ReplaceString(char *str, char *find, char *newpart)
{
	char *oldpart;
	size_t oldlen = strlen(find);
	size_t nlen = strlen(newpart);
	while((oldpart = strstr(str, find)))
	{
		memmove(oldpart+nlen, oldpart+oldlen, strlen(oldpart+oldlen)+1);
		memmove(oldpart, newpart, nlen);
		str = oldpart+nlen;
	}
}
static void PKG_CreateOutput(struct pkgctx_s *ctx, struct dataset_s *s, const char *code, const char *filename)
{
	char path[MAX_PATH];
	char date[64];
	struct output_s *o;
	for (o = s->outputs; o; o = o->next)
	{
		if (!strcmp(o->code, code))
		{
			ctx->messagecallback(ctx->userctx, "Dataset '%s' defined with dupe output\n", s->name, code);
			return;
		}
	}

	if (strlen(code) >= sizeof(o->code))
	{
		ctx->messagecallback(ctx->userctx, "Output '%s' name too long\n", code);
		return;
	}

	strcpy(path, filename);
	strftime(date, sizeof(date), "%Y%m%d", localtime(&ctx->buildtime));
	PKG_ReplaceString(path, "$date", date);

	o = malloc(sizeof(*o));
	memset(o, 0, sizeof(*o));
	strcpy(o->code, code);
	QCC_Canonicalize(o->filename, sizeof(o->filename), path, ctx->gamepath);
	o->next = s->outputs;
	s->outputs = o;
}

static void PKG_ParseOutput(struct pkgctx_s *ctx)
{
	struct dataset_s *s;
	char name[128];
	char prop[128];
	char fname[128];

	if (!PKG_GetToken(ctx, name, sizeof(name), false))
	{
		ctx->messagecallback(ctx->userctx, "Output: Expected name\n");
		return;
	}

	if (PKG_GetStringToken(ctx, prop, sizeof(prop)))
	{
		s = PKG_GetDataset(ctx, "core");
		PKG_CreateOutput(ctx, s, name, prop);
	}
	else
	{
		if (!PKG_Expect(ctx, "{"))
			return;
		while(PKG_GetToken(ctx, prop, sizeof(prop), true))
		{
			if (!strcmp(prop, "}"))
				break;
			else
			{
				char *e = strchr(prop, ':');
				if (e && !e[1])
				{
					*e = 0;
					s = PKG_GetDataset(ctx, prop);
					if (PKG_GetStringToken(ctx, fname, sizeof(fname)))
						PKG_CreateOutput(ctx, s, name, fname);
					else
						ctx->messagecallback(ctx->userctx, "Output '%s[%s]' filename omitted\n", name, prop);
				}
				else
					ctx->messagecallback(ctx->userctx, "Output '%s' has unknown property '%s'\n", name, prop);
			}

			//skip any junk
			while(PKG_GetToken(ctx, prop, sizeof(prop), false))
			{
				if (!strcmp(prop, ";"))
					break;
			}
		}
	}
}

static void PKG_AddOldPack(struct pkgctx_s *ctx, const char *fname)
{
	struct oldpack_s *pack;

	pack = malloc(sizeof(*pack));
	strcpy(pack->filename, fname);
	pack->numfiles = 0;
	pack->file = NULL;
	pack->next = ctx->oldpacks;
	ctx->oldpacks = pack;
}
static void PKG_ParseOldPack(struct pkgctx_s *ctx)
{
	char token[MAX_PATH];
	char oldpack[MAX_PATH];

	if (!PKG_GetStringToken(ctx, token, sizeof(token)))
		return;

#ifdef _WIN32
	{
		WIN32_FIND_DATA fd;
		HANDLE h;
		QCC_Canonicalize(oldpack, sizeof(oldpack), token, ctx->gamepath);
		h = FindFirstFile(oldpack, &fd);
		if (h == INVALID_HANDLE_VALUE)
			ctx->messagecallback(ctx->userctx, "wildcard string '%s' found no files\n", token);
		else
		{
			do
			{
				QCC_Canonicalize(token, sizeof(token), fd.cFileName, oldpack);
				PKG_AddOldPack(ctx, token);
			} while(FindNextFile(h, &fd));
		}
	}
#else
	ctx->messagecallback(ctx->userctx, "no wildcard support, sorry\n");
#endif
}
/*
static void PKG_ParseDataset(struct pkgctx_s *ctx)
{
	struct dataset_s *s;
	char name[128];
	char prop[128];

	if (!PKG_GetToken(ctx, name, sizeof(name), false))
	{
		ctx->messagecallback(ctx->userctx, "Dataset: Expected name\n");
		return;
	}

	if (strlen(name) >= sizeof(s->name))
	{
		ctx->messagecallback(ctx->userctx, "Dataset '%s' name too long\n", name);
		return;
	}

	s = malloc(sizeof(*s));
	memset(s, 0, sizeof(*s));
	strcpy(s->name, name);

	if (PKG_Expect(ctx, "{"))
	{
		while(PKG_GetToken(ctx, prop, sizeof(prop), true))
		{
			if (!strcmp(prop, "}"))
				break;
			else if (!strcmp(prop, "output"))
			{
				if (PKG_GetToken(ctx, name, sizeof(name), false))
					if (PKG_GetStringToken(ctx, prop, sizeof(prop)))
					{
						PKG_CreateOutput(ctx, s, name, prop);
					}
			}
			else if (!strcmp(prop, "base"))
				PKG_GetStringToken(ctx, prop, sizeof(prop));
			else
				ctx->messagecallback(ctx->userctx, "Dataset '%s' has unknown property '%s'\n", name, prop);

			//skip any junk
			while(PKG_GetToken(ctx, prop, sizeof(prop), false))
			{
				if (!strcmp(prop, ";"))
					break;
			}
		}
	}

	if (PKG_FindDataset(ctx, name))
		ctx->messagecallback(ctx->userctx, "Dataset '%s' is already defined\n", name);
	else
	{	//link it in!
		s->next = ctx->datasets;
		ctx->datasets = s;
		return;
	}
	PKG_DestroyDataset(s);
	return;
}*/

static void PKG_ParseRule(struct pkgctx_s *ctx)
{
	struct rule_s *r;
	char name[128];
	char prop[128];
	char newext[128];
	char command[4096];
	int dropfile = false;

	if (!PKG_GetToken(ctx, name, sizeof(name), false))
		return;

	if (strlen(name) >= sizeof(r->name))
	{
		ctx->messagecallback(ctx->userctx, "Rule '%s' name too long\n", name);
		return;
	}

	*newext = *command = 0;
	if (PKG_Expect(ctx, "{"))
	{
		while(PKG_GetToken(ctx, prop, sizeof(prop), true))
		{
			if (!strcmp(prop, "}"))
				break;
			else if (!strcmp(prop, "newext"))
				PKG_GetToken(ctx, newext, sizeof(newext), false);
			else if (!strcmp(prop, "skip"))
			{
				if (PKG_GetToken(ctx, prop, sizeof(prop), false))
					dropfile = atoi(prop);
				else
					dropfile = true;
			}
			else if (!strcmp(prop, "command"))
				PKG_GetStringToken(ctx, command, sizeof(command));
			else
				ctx->messagecallback(ctx->userctx, "Rule '%s' has unknown property '%s'\n", name, prop);

			//skip any junk
			while(PKG_GetToken(ctx, prop, sizeof(prop), false))
			{
				if (!strcmp(prop, ";"))
					break;
			}
		}
	}

	r = PKG_FindRule(ctx, name);
	if (r)
	{
		ctx->messagecallback(ctx->userctx, "Rule %s is already defined\n", name);
		return;
	}

	r = malloc(sizeof(*r));
	memset(r, 0, sizeof(*r));
	strcpy(r->name, name);
	r->newext = strdup(newext);
	r->command = strdup(command);
	r->dropfile = dropfile;
	r->next = ctx->rules;
	ctx->rules = r;
}
static void PKG_AddClassFile(struct pkgctx_s *ctx, struct class_s *c, const char *fname)
{
	struct file_s *f;

	if (strlen(fname) >= sizeof(f->name))
	{
		ctx->messagecallback(ctx->userctx, "File name '%s' too long in class %s\n", fname, c->name);
		return;
	}

	f = malloc(sizeof(*f));
	memset(f, 0, sizeof(*f));
	strcpy(f->name, fname);
	f->next = c->files;
	c->files = f;
}
static void PKG_AddClassFiles(struct pkgctx_s *ctx, struct class_s *c, const char *fname)
{
#ifdef _WIN32
	WIN32_FIND_DATA fd;
	HANDLE h;
	char basepath[MAX_PATH];
	QCC_Canonicalize(basepath, sizeof(basepath), fname, ctx->gamepath);
	h = FindFirstFile(basepath, &fd);
	if (h == INVALID_HANDLE_VALUE)
		ctx->messagecallback(ctx->userctx, "wildcard string '%s' found no files\n", fname);
	else
	{
		do
		{
			QCC_Canonicalize(basepath, sizeof(basepath), fd.cFileName, fname);
			PKG_AddClassFile(ctx, c, basepath);
		} while(FindNextFile(h, &fd));
	}
#else
	ctx->messagecallback(ctx->userctx, "no wildcard support, sorry\n");
#endif
}
static void PKG_ParseClass(struct pkgctx_s *ctx, char *output)
{
	struct class_s *c;
	struct rule_s *r;
	struct dataset_s *s;
	char *e;
	char name[128];
	char prop[128];
	size_t u;

	if (output)
	{
		if (!PKG_Expect(ctx, "{"))
			return;
		*name = 0;
	}
	else if (!PKG_GetToken(ctx, name, sizeof(name), false))
		return;

	if (output || !strcmp(name, "{"))
	{
		c = malloc(sizeof(*c));
		memset(c, 0, sizeof(*c));
		strcpy(c->name, "");
		strcpy(c->outname, (output && *output)?output:"default");
		c->next = ctx->classes;
		ctx->classes = c;
	}
	else
	{
		if (strlen(name) >= sizeof(c->name))
		{
			ctx->messagecallback(ctx->userctx, "Class '%s' name too long\n", name);
			return;
		}

		c = PKG_FindClass(ctx, name);
		if (!c)
		{
			c = malloc(sizeof(*c));
			memset(c, 0, sizeof(*c));
			strcpy(c->name, name);
			strcpy(c->outname, (output && *output)?output:"default");
			c->next = ctx->classes;
			ctx->classes = c;
		}

		if (!PKG_Expect(ctx, "{"))
			return;
	}

	{
		while(PKG_GetToken(ctx, prop, sizeof(prop), true))
		{
			if (!strcmp(prop, "}"))
				break;
			else if (!strcmp(prop, "output"))
				PKG_GetToken(ctx, c->outname, sizeof(c->outname), false);
			else if (!strcmp(prop, "rule"))
			{
				if (PKG_GetToken(ctx, prop, sizeof(prop), false))
				{
					if (c->defaultrule)
						ctx->messagecallback(ctx->userctx, "Class '%s' already has a default rule\n", name);
					c->defaultrule = PKG_FindRule(ctx, prop);
					if (!c->defaultrule)
						ctx->messagecallback(ctx->userctx, "Class '%s' specifies unknown rule %s\n", name, prop);
				}
			}
			else
			{
				e = strchr(prop, ':');
				if (e && !e[1])
				{
					*e = 0;
					s = PKG_FindDataset(ctx, prop);
					PKG_GetToken(ctx, prop, sizeof(prop), false);
					if (s)
					{
						r = PKG_FindRule(ctx, prop);
						for (u = 0; ; u++)
						{
							if (u == countof(c->dataset))
							{
								ctx->messagecallback(ctx->userctx, "Class '%s' specialises for too many datasets\n", c->name, s->name);
								break;
							}
							if (c->dataset[u].set == s)
								ctx->messagecallback(ctx->userctx, "Class '%s' already defines a rule for dataset '%s'\n", c->name, s->name);
							else if (!c->dataset[u].set)
							{
								c->dataset[u].set = s;
								c->dataset[u].rule = r;
								break;
							}
						}
					}
				}
				else if (strchr(prop, '.'))
				{
					if (strchr(prop, '*') || strchr(prop, '?'))
						PKG_AddClassFiles(ctx, c, prop);
					else
						PKG_AddClassFile(ctx, c, prop);
				}
				else
					ctx->messagecallback(ctx->userctx, "Class '%s' has unknown property '%s'\n", name, prop);
			}

			//skip any junk
			while(PKG_GetToken(ctx, prop, sizeof(prop), false))
			{
				if (!strcmp(prop, ";"))
					break;
			}
		}
	}
}
static void PKG_ParseClassFiles(struct pkgctx_s *ctx, struct class_s *c)
{
	char prop[128];

	if (PKG_Expect(ctx, "{"))
	{
		while(PKG_GetToken(ctx, prop, sizeof(prop), true))
		{
			if (!strcmp(prop, "}"))
				break;
			if (!strcmp(prop, ";"))
				continue;

			if (strchr(prop, '*') || strchr(prop, '?'))
				PKG_AddClassFiles(ctx, c, prop);
			else
				PKG_AddClassFile(ctx, c, prop);
		}
	}
}




#ifdef AVAIL_ZLIB
#include <zlib.h>
static unsigned int PKG_DeflateToFile(FILE *f, unsigned int rawsize, void *in, int method)
{
	char out[8192];
	int i=0;

	z_stream strm = {
		(char *)in,
		rawsize,
		0,

		out,
		sizeof(out),
		0,

		NULL,
		NULL,

		NULL,
		NULL,
		NULL,

		Z_BINARY,
		0,
		0
	};

	if (method == 8)
		deflateInit2(&strm, 9, Z_DEFLATED, -MAX_WBITS, 9, Z_DEFAULT_STRATEGY);		//zip deflate compression
	else
		deflateInit(&strm, Z_BEST_COMPRESSION);	//zlib compression
	while(deflate(&strm, Z_FINISH) == Z_OK)
	{
		fwrite(out, 1, sizeof(out) - strm.avail_out, f);	//compress in chunks of 8192. Saves having to allocate a huge-mega-big buffer
		i+=sizeof(out) - strm.avail_out;
		strm.next_out = out;
		strm.avail_out = sizeof(out);
	}
	fwrite(out, 1, sizeof(out) - strm.avail_out, f);
	i+=sizeof(out) - strm.avail_out;
	deflateEnd(&strm);
	return i;
}
#endif

#ifdef _WIN32
static void StupidWindowsPopenAlternativeCrap(struct pkgctx_s *ctx, char *commandline)
{
	PROCESS_INFORMATION piProcInfo = {0}; 
	SECURITY_ATTRIBUTES saAttr = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
	STARTUPINFO siStartInfo = {sizeof(STARTUPINFO)};
	HANDLE readpipe = INVALID_HANDLE_VALUE;
	HANDLE writepipe = INVALID_HANDLE_VALUE;
	siStartInfo.hStdError = siStartInfo.hStdOutput = siStartInfo.hStdInput = INVALID_HANDLE_VALUE;
	if (CreatePipe(&readpipe, &siStartInfo.hStdOutput, &saAttr, 0))
	{
		if (CreatePipe(&siStartInfo.hStdInput, &writepipe, &saAttr, 0))
		{
			SetHandleInformation(readpipe, HANDLE_FLAG_INHERIT, 0);
			SetHandleInformation(writepipe, HANDLE_FLAG_INHERIT, 0);
			siStartInfo.hStdError = siStartInfo.hStdOutput;
			siStartInfo.dwFlags |= STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW/*ZOMGWTFBBQ*/;
			if (!CreateProcess(NULL, (*commandline=='@')?commandline+1:commandline,  NULL, NULL, TRUE, 0, NULL, NULL, &siStartInfo, &piProcInfo)) 
				ctx->messagecallback(ctx->userctx, "Unable to execute command %s\n", commandline);
			else 
			{
				CloseHandle(piProcInfo.hProcess);
				CloseHandle(piProcInfo.hThread);
			}
		}
	}
	CloseHandle(siStartInfo.hStdOutput);
	CloseHandle(siStartInfo.hStdInput);

	CloseHandle(writepipe);
	for (;;) 
	{ 
		char buf[64];
		DWORD SHOUTY;
		if (!ReadFile(readpipe, buf, sizeof(buf)-1, &SHOUTY, NULL) || SHOUTY == 0)
			break;
		if (*commandline == '@')
			continue;
		buf[SHOUTY] = 0;
		ctx->messagecallback(ctx->userctx, "%s", buf);
	}
	CloseHandle(readpipe);
}
#endif

static void *PKG_OpenSourceFile(struct pkgctx_s *ctx, struct file_s *file, size_t *fsize)
{
	char fullname[1024];
	FILE *f;
	char *data;
	size_t size;
	struct rule_s *rule = file->write.rule;

	*fsize = 0;

	QCC_Canonicalize(fullname, sizeof(fullname), file->name, ctx->gamepath);

	f = fopen(fullname, "rb");
	if (!f)
		return NULL;

	if (rule)
		ctx->messagecallback(ctx->userctx, "\t\tProcessing %s (%s)\n", file->name, rule->name);
	else
		ctx->messagecallback(ctx->userctx, "\t\tCompressing %s\n", file->name);

	strcpy(file->write.name, file->name);
	if (rule)
	{
		data = strrchr(file->write.name, '.');
		if (!data)
			data = file->write.name+strlen(file->write.name);
		if (strchr(rule->newext, '.'))
			strcpy(data, rule->newext);	//note: this allows weird _foo.tga postfixes.
		else
		{
			*data = '.';
			strcpy(data+1, rule->newext);
		}

		if (rule->command)
		{
			int i;
			char commandline[4096];
			char *cmd;
			char tempname[1024]; 
			//generate a sequenced temp filename
			//run the external tool to write that file
			//read the temp file.
			//delete temp file...
			fclose(f);

			QCC_Canonicalize(tempname, sizeof(tempname), file->write.name, ctx->gamepath);
			f = fopen(tempname, "rb");
			if (f)
			{
				fclose(f);
				ctx->messagecallback(ctx->userctx, "Temp file %s already exists... not replacing+deleting\n", tempname);
				return NULL;
			}
			
			for (i = 0, cmd = rule->command; *cmd && i < countof(commandline)-1; )
			{
				if (!strncmp(cmd, "$input", 6))
				{
					strcpy(&commandline[i], fullname);
					i += strlen(&commandline[i]);
					cmd += 6;
				}
				else if (!strncmp(cmd, "$output", 7))
				{
					strcpy(&commandline[i], tempname);
					i += strlen(&commandline[i]);
					cmd += 7;
				}
				else
					commandline[i++] = *cmd++;
			}
			commandline[i] = 0;
//			ctx->messagecallback(ctx->userctx, "Commandline is %s\n", commandline);


#ifdef _WIN32		//windows is so fucking useless sometimes. sure, _popen 'works'... its just perverse enough that its not an option, forcing system-specific crap in anything that isn't originally from unix... maybe it is just incompetence? still feels like malice to me.
			StupidWindowsPopenAlternativeCrap(ctx, commandline);
#else
			{
				FILE *p;
				p = popen((*commandline=='@')?commandline+1:commandline, "rt");
				if (!p)
				{
					ctx->messagecallback(ctx->userctx, "Unable to execute command\n", tempname);
					return NULL;
				}
				while(fgets(commandline, sizeof(commandline), p))
					ctx->messagecallback(ctx->userctx, "%s", commandline);
				if (feof(p))
					ctx->messagecallback(ctx->userctx, "Process returned %d\n", _pclose( p ));
				else
				{
					printf( "Error: Failed to read the pipe to the end.\n");
					_pclose(p);
				}
			}
#endif

			f = fopen(tempname, "rb");
			if (!f)
			{
				ctx->messagecallback(ctx->userctx, "Temp file %s wasn't created\n", tempname);
				return NULL;
			}

			fseek(f, 0, SEEK_END);
			size = ftell(f);
			fseek(f, 0, SEEK_SET);
			data = malloc(size+1);
			fread(data, 1, size, f);
			fclose(f);
			*fsize = size;

			_unlink(tempname);
			return data;
		}
	}


	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	data = malloc(size+1);
	fread(data, 1, size, f);
	fclose(f);
	*fsize = size;

	return data;
}

static void PKG_WritePackageData(struct pkgctx_s *ctx, struct output_s *out)
{
	//helpers to deal with misaligned data. writes little-endian.
#define misbyte(ptr,ofs,data) ((unsigned char*)(ptr))[ofs] = (data)&0xff;
#define misshort(ptr,ofs,data) misbyte((ptr),(ofs),(data));misbyte((ptr),(ofs)+1,(data)>>8);
#define misint(ptr,ofs,data) misshort((ptr),(ofs),(data));misshort((ptr),(ofs)+2,(data)>>16);
	int num=0;
	int ofs;
	pbool pak = false;
	int startofs = 0;

	struct file_s *f;
	char centralheader[46+sizeof(f->write.name)];
	int centraldirsize;

	char *filedata;

	FILE *outf;
	struct
	{
		char magic[4];
		unsigned int tabofs;
		unsigned int tabbytes;
	} pakheader = {"PACK", 0, 0};
	char *ext;

#ifdef AVAIL_ZLIB
	#define compmethod (pak?0:8)/*Z_DEFLATED*/
#else
	#define compmethod 0/*Z_RAW*/
#endif
	if (!compmethod)
		pak = true; //might as well boost compat...
	ext = strrchr(out->filename, '.');
	if (ext && !QC_strcasecmp(ext, ".pak"))
		pak = true;

	outf = fopen(out->filename, "wb");
	if (!outf)
	{
		ctx->messagecallback(ctx->userctx, "Unable to open %s\n", out->filename);
		return;
	}

	if (pak)	//reserve space for the pak header
		fwrite(&pakheader, 1, sizeof(pakheader), outf);

	for (f = out->files; f ; f=f->write.nextwrite)
	{
		char header[32+sizeof(f->write.name)];
		size_t fnamelen = strlen(f->write.name);

		filedata = PKG_OpenSourceFile(ctx, f, &f->write.rawsize);
		if (!filedata)
		{
			ctx->messagecallback(ctx->userctx, "Unable to open %s\n", f->name);
		}

		f->write.zcrc = QC_encodecrc(f->write.rawsize, filedata);
		misint  (header, 0, 0x04034b50);
		misshort(header, 4, 0);//minver
		misshort(header, 6, 0);//general purpose flags
		misshort(header, 8, 0);//compression method, 0=store, 8=deflate
		misshort(header, 10, 0);//lastmodfiletime
		misshort(header, 12, 0);//lastmodfiledate
		misint  (header, 14, f->write.zcrc);//crc32
		misint  (header, 18, f->write.rawsize);//compressed size
		misint  (header, 22, f->write.rawsize);//uncompressed size
		misshort(header, 26, fnamelen);//filename length
		misshort(header, 28, 0);//extradata length
		strcpy(header+30, f->write.name);

		f->write.zhdrofs = ftell(outf);
		fwrite(header, 1, 30+fnamelen, outf);

#ifdef AVAIL_ZLIB
		if (compmethod == 2 || compmethod == 8)
		{
			size_t end;
			f->write.pakofs = 0;

			f->write.zipsize = PKG_DeflateToFile(outf, f->write.rawsize, filedata, compmethod);

			misshort(header, 8, compmethod);//compression method, 0=store, 8=deflate
			misint  (header, 18, f->write.zipsize);

			end = ftell(outf);
			fseek(outf, f->write.zhdrofs, SEEK_SET);
			fwrite(header, 1, 30+fnamelen, outf);
			fseek(outf, end, SEEK_SET);
		}
		else
#endif
		{
			f->write.pakofs = ftell(outf);
			f->write.zipsize = fwrite(filedata, 1, f->write.rawsize, outf);
		}
		free(filedata);
		num++;
	}

	if (pak)
	{
		struct 
		{
			char name[56];
			unsigned int size;
			unsigned int offset;
		} pakentry;
		//prepare the header
		pakheader.tabbytes = num * sizeof(pakentry);
		pakheader.tabofs = ftell(outf);
		fseek(outf, 0, SEEK_SET);
		fwrite(&pakheader, 1, sizeof(pakheader), outf);
		//now go and write the file table.
		fseek(outf, pakheader.tabofs, SEEK_SET);
		for (f = out->files,num=0; f ; f=f->write.nextwrite)
		{
			memset(&pakentry, 0, sizeof(pakentry));
			QC_strlcpy(pakentry.name, f->write.name, sizeof(pakentry.name));
			pakentry.size = (f->write.pakofs==0)?0:f->write.rawsize;
			pakentry.offset = f->write.pakofs;
			fwrite(&pakentry, 1, sizeof(pakentry), outf);
			num++;
		}
	}

	ofs = ftell(outf);
	for (f = out->files,num=0; f ; f=f->write.nextwrite)
	{
		size_t fnamelen;
		fnamelen = strlen(f->write.name);
		misint  (centralheader, 0, 0x02014b50);
		misshort(centralheader, 4, 0);//ourver
		misshort(centralheader, 6, 0);//minver
		misshort(centralheader, 8, 0);//general purpose flags
		misshort(centralheader, 10, compmethod);//compression method, 0=store, 8=deflate
		misshort(centralheader, 12, 0);//lastmodfiletime
		misshort(centralheader, 14, 0);//lastmodfiledate
		misint  (centralheader, 16, f->write.zcrc);//crc32
		misint  (centralheader, 20, f->write.zipsize);//compressed size
		misint  (centralheader, 24, f->write.rawsize);//uncompressed size
		misshort(centralheader, 28, fnamelen);//filename length
		misshort(centralheader, 30, 0);//extradata length
		misshort(centralheader, 32, 0);//comment length
		misshort(centralheader, 34, 0);//first disk number
		misshort(centralheader, 36, 0);//internal file attribs
		misint  (centralheader, 38, 0);//external file attribs
		misint  (centralheader, 42, f->write.zhdrofs);//local header offset
		strcpy(centralheader+46, f->write.name);
		fwrite(centralheader, 1, 46 + fnamelen, outf);
		num++;
	}

	centraldirsize = ftell(outf)-ofs;
	misint  (centralheader, 0, 0x06054b50);
	misshort(centralheader, 4, 0);	//this disk number
	misshort(centralheader, 6, 0);	//centraldir first disk
	misshort(centralheader, 8, num);	//centraldir entries
	misshort(centralheader, 10, num);	//total centraldir entries
	misint  (centralheader, 12, centraldirsize);	//centraldir size
	misint  (centralheader, 16, ofs);	//centraldir offset
	misshort(centralheader, 20, 0);	//comment length
	fwrite(centralheader, 1, 22, outf);

	fclose(outf);
}

#include <sys/stat.h>
static time_t PKG_GetFileTime(const char *filename)
{
	struct stat s;
	if (stat(filename, &s) != -1)
		return s.st_mtime;
}

static void PKG_ReadPackContents(struct pkgctx_s *ctx, struct oldpack_s *old)
{
#define longfromptr(p) (((p)[0]<<0)|((p)[1]<<8)|((p)[2]<<16)|((p)[3]<<24))
#define shortfromptr(p) (((p)[0]<<0)|((p)[1]<<8))
	size_t u, namelen;
	unsigned int foffset;
	unsigned char header[46];
	int i;
	FILE *f;

	//ignore packages if we're going to be overwritten.
	struct dataset_s *set;
	struct output_s *out;
	for (set = ctx->datasets; set; set = set->next)
	{
		for (out = set->outputs; out; out = out->next)
		{
			if (!strcmp(out->filename, old->filename))
				return;
		}
	}


	f = fopen(old->filename, "rb");
	if (f)
	{
		//find end-of-central-dir
		fseek(f, -22, SEEK_END);
		fread(header, 1, 22, f);

		if (header[0] == 'P' && header[1] == 'K' && header[2] == 5 && header[3] == 6)
		{
			old->numfiles = shortfromptr(header+8);
			foffset = longfromptr(header+16);

			old->file = malloc(sizeof(*old->file)*old->numfiles);


			fseek(f, foffset, SEEK_SET);
			for(u = 0; u < old->numfiles; u++)
			{
				fread(header, 1, 46, f);
				//zcrc @ 16
				old->file[u].size = longfromptr(header+24);
				namelen = shortfromptr(header+28);
				fread(old->file[u].name, 1, namelen, f);
				old->file[u].name[namelen] = 0;
				i = shortfromptr(header+30)+shortfromptr(header+32);
				if (i)
					fseek(f, i, SEEK_CUR);
			}
		}
		else
		{
			fseek(f, 0, SEEK_SET);
			fread(header, 1, 12, f);
			if (header[0] == 'P' && header[1] == 'A' && header[2] == 'C' && header[3] == 'K')
			{
				unsigned int ofs = longfromptr(header+4);
				unsigned int dsz = longfromptr(header+8);
				struct 
				{
					char name[56];
					unsigned int size;
					unsigned int offset;
				} *files;
				files = malloc(dsz);
				fseek(f, ofs, SEEK_SET);
				fread(files, 1, dsz, f);
				old->numfiles = dsz / sizeof(*files);
				old->file = malloc(sizeof(*old->file)*old->numfiles);
				for (u = 0; u < old->numfiles; u++)
				{
					strcpy(old->file[u].name, files[u].name);
					old->file[u].size = files[u].size;
				}
				free(files);
			}
			else
				ctx->messagecallback(ctx->userctx, "%s does not appear to be a package\n", old->filename);
		}

		//walk central directory
		fclose(f);
	}
}

static pbool PKG_FileIsModified(struct pkgctx_s *ctx, const char *filename)
{
	struct oldpack_s *old;
	size_t u;
	for (old = ctx->oldpacks; old; old = old->next)
	{
		for (u = 0; u < old->numfiles; u++)
		{
			if (!strcmp(old->file[u].name, filename))
			{
				ctx->messagecallback(ctx->userctx, "\t%s already contains %s\n", old->filename, filename);
				return false;
			}
		}
	}
	return true;
}

static void PKG_WriteDataset(struct pkgctx_s *ctx, struct dataset_s *set)
{
	struct class_s *cls;
	struct output_s *out;
	struct file_s *file;
	struct rule_s *rule;
	struct oldpack_s *old;
	size_t u;

	if (!ctx->readoldpacks)
	{
		ctx->readoldpacks = true;

		for(old = ctx->oldpacks; old; old = old->next)
		{	//fixme: strip any wildcarded paks that match an output, to avoid weirdness.
			PKG_ReadPackContents(ctx, old);
		}
	}

	ctx->messagecallback(ctx->userctx, "Building dataset %s\n", set->name);

	for (cls = ctx->classes; cls; cls = cls->next)
	{
		for (out = set->outputs; out; out = out->next)
		{
			if (!strcmp(out->code, cls->outname))
				break;
		}
		if (!out)	//dataset doesn't name this.
			continue;

		rule = cls->defaultrule;
		for (u = 0; u < countof(cls->dataset); u++)
		{
			if (cls->dataset[u].set == set)
			{
				rule = cls->dataset[u].rule;
				break;
			}
		}

		if (rule && rule->dropfile)
			continue;

		for (file = cls->files; file; file = file->next)
		{
			if (!PKG_FileIsModified(ctx, file->name))
				continue;
//			ctx->messagecallback(ctx->userctx, "\t\tFile %s, rule %s\n", file->name, rule?rule->name:"");

			file->write.nextwrite = out->files;
			file->write.rule = rule;
			out->files = file;
		}
	}

	for (out = set->outputs; out; out = out->next)
	{
		if (!out->files)
		{
			ctx->messagecallback(ctx->userctx, "\tOutput %s[%s] \"%s\" has no files\n", out->code, set->name, out->filename);
			continue;
		}
		
		ctx->messagecallback(ctx->userctx, "\tGenerating %s[%s] \"%s\"\n", out->code, set->name, out->filename);
		PKG_WritePackageData(ctx, out);
	}
}
void Packager_WriteDataset(struct pkgctx_s *ctx, char *setname)
{
	struct dataset_s *dataset;
	if (setname && strcmp(setname, "*"))
	{
		dataset = PKG_FindDataset(ctx, setname);
		if (dataset)
			PKG_WriteDataset(ctx, dataset);
		else
			ctx->messagecallback(ctx->userctx, "Dataset %s not known\n", setname);
	}
	else
	{
		for (dataset = ctx->datasets; dataset; dataset = dataset->next)
			PKG_WriteDataset(ctx, dataset);
	}
}
struct pkgctx_s *Packager_Create(void (*messagecallback)(void *userctx, char *message), void *userctx)
{
	struct pkgctx_s *ctx;
	ctx = malloc(sizeof(*ctx));
	memset(ctx, 0, sizeof(*ctx));
	ctx->messagecallback = messagecallback;
	ctx->userctx = userctx;
	time(&ctx->buildtime);
	return ctx;
}
void Packager_Parse(struct pkgctx_s *ctx, char *scriptname)
{
	size_t remaining = 0;
	char *file = QCC_ReadFile(scriptname, NULL, NULL, &remaining);
	char cmd[128];

	strcpy(ctx->gamepath, scriptname);

	ctx->listfile = file;
	while (PKG_GetToken(ctx, cmd, sizeof(cmd), true))
	{
//		if (!strcmp(cmd, "dataset"))
//			PKG_ParseDataset(ctx);
		if (!strcmp(cmd, "output"))
			PKG_ParseOutput(ctx);
		else if (!strcmp(cmd, "rule"))
			PKG_ParseRule(ctx);
		else if (!strcmp(cmd, "class"))
			PKG_ParseClass(ctx, NULL);
		else if (!strcmp(cmd, "ignore")||!strcmp(cmd, "oldpack"))
			PKG_ParseOldPack(ctx);
		else
		{
			char *e = strchr(cmd, ':');
			if (e && !e[1])
			{
				*e = 0;
				PKG_ParseClass(ctx, cmd);
			}
			else
			{
				struct class_s *c = PKG_FindClass(ctx, cmd);
				if (c)
					PKG_ParseClassFiles(ctx, c);
				else
					ctx->messagecallback(ctx->userctx, "Unrecognised token at global scope '%s'\n", cmd);
			}
		}
		//skip any junk
		while(PKG_GetToken(ctx, cmd, sizeof(cmd), false))
		{
			if (!strcmp(cmd, ";"))
				break;
		}
	}
	free(file);
}

void Packager_Destroy(struct pkgctx_s *ctx)
{
}
