#include "quakedef.h"
#include "winquake.h"
#include "sys_plugfte.h"
#include "../http/iweb.h"

static void UnpackAndExtractPakFiles_Complete(struct dl_download *dl);
static void pscript_property_splash_sets(struct context *ctx, const char *val);

typedef struct 
{
	vfsfile_t funcs;

	char *data;
	int maxlen;
	int writepos;
	int readpos;
} vfspipe_t;

void VFSPIPE_Close(vfsfile_t *f)
{
	vfspipe_t *p = (vfspipe_t*)f;
	free(p->data);
	free(p);
}
unsigned long VFSPIPE_GetLen(vfsfile_t *f)
{
	vfspipe_t *p = (vfspipe_t*)f;
	return p->writepos - p->readpos;
}
unsigned long VFSPIPE_Tell(vfsfile_t *f)
{
	return 0;
}
qboolean VFSPIPE_Seek(vfsfile_t *f, unsigned long offset)
{
	Con_Printf("Seeking is a bad plan, mmkay?");
	return false;
}
int VFSPIPE_ReadBytes(vfsfile_t *f, void *buffer, int len)
{
	vfspipe_t *p = (vfspipe_t*)f;
	if (len > p->writepos - p->readpos)
		len = p->writepos - p->readpos;
	memcpy(buffer, p->data+p->readpos, len);
	p->readpos += len;

	if (p->readpos > 8192)
	{
		//shift the memory down periodically
		//fixme: use cyclic buffer? max size, etc?
		memmove(p->data, p->data+p->readpos, p->writepos-p->readpos);

		p->writepos -= p->readpos;
		p->readpos = 0;
	}
	return len;
}
int VFSPIPE_WriteBytes(vfsfile_t *f, const void *buffer, int len)
{
	vfspipe_t *p = (vfspipe_t*)f;
	if (p->writepos + len > p->maxlen)
	{
		p->maxlen = p->writepos + len;
		p->data = realloc(p->data, p->maxlen);
	}
	memcpy(p->data+p->writepos, buffer, len);
	p->writepos += len;
	return len;
}

vfsfile_t *VFSPIPE_Open(void)
{
	vfspipe_t *newf;
	newf = malloc(sizeof(*newf));
	newf->data = NULL;
	newf->maxlen = 0;
	newf->readpos = 0;
	newf->writepos = 0;
	newf->funcs.Close = VFSPIPE_Close;
	newf->funcs.Flush = NULL;
	newf->funcs.GetLen = VFSPIPE_GetLen;
	newf->funcs.ReadBytes = VFSPIPE_ReadBytes;
	newf->funcs.Seek = VFSPIPE_Seek;
	newf->funcs.Tell = VFSPIPE_Tell;
	newf->funcs.WriteBytes = VFSPIPE_WriteBytes;
	newf->funcs.seekingisabadplan = true;

	return &newf->funcs;
}













struct context
{
	struct contextpublic pub;

	void *windowhnd;
	int windowwidth;
	int windowheight;

	int waitingfordatafiles;

	char *datadownload;
	char *gamename;
	char *password;
	char *onstart;
	char *onend;
	char *ondemoend;

	void *nppinstance;

	int read;
	int written;

	qtvfile_t qtvf;

	unsigned char *splashdata;
	int splashwidth;
	int splashheight;
	struct dl_download *splashdownload;
	struct dl_download *packagelist;

	void *mutex;
	void *thread;
	char resetvideo;
	qboolean shutdown;

	struct context *next;

	struct browserfuncs bfuncs;
};

#ifdef _WIN32

extern HWND sys_parentwindow;
extern unsigned int sys_parentwidth;
extern unsigned int sys_parentheight;
HINSTANCE	global_hInstance;
static char binaryname[MAX_PATH];

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		global_hInstance = hinstDLL;
		GetModuleFileName(global_hInstance, binaryname, sizeof(binaryname));
		break;
	default:
		break;
	}
	return TRUE;
}
#endif

struct context *activecontext;
struct context *contextlist;

#define ADDRARG(x) do {if (argc < maxargs) argv[argc++] = strdup(x);} while(0)
#define ADDCARG(x) do {if (argc < maxargs) argv[argc++] = strdup(cleanarg(x));} while(0)
char *cleanarg(char *arg)
{
	//no hacking us, please.
	while (*arg == '-' || *arg == '+')
		arg++;
	while (*arg && *(unsigned char*)arg <= ' ')
		arg++;
	if (*arg)
		return arg;
	return "badarg";
}

int Plug_GenCommandline(struct context *ctx, char **argv, int maxargs)
{
	char *s;
	int argc;
	char tok[256];

	argv[0] = strdup(binaryname);
	argc = 1;

	switch(ctx->qtvf.connectiontype)
	{
	default:
		break;
	case QTVCT_STREAM:
		ADDRARG("+qtvplay");
		ADDCARG(ctx->qtvf.server);
		break;
	case QTVCT_CONNECT:
		ADDRARG("+connect");
		ADDCARG(ctx->qtvf.server);
		break;
	case QTVCT_JOIN:
		ADDRARG("+join");
		ADDCARG(ctx->qtvf.server);
		break;
	case QTVCT_OBSERVE:
		ADDRARG("+observe");
		ADDCARG(ctx->qtvf.server);
		break;
	case QTVCT_MAP:
		ADDRARG("+map");
		ADDCARG(ctx->qtvf.server);
		break;
	}

	if (ctx->password)
	{
		ADDRARG("+password");
		ADDCARG(ctx->password);
	}

	//figure out the game dirs (first token is the base game)
	s = ctx->gamename;
	s = COM_ParseOut(s, tok, sizeof(tok));
	if (!*tok || !strcmp(tok, "q1") || !strcmp(tok, "qw") || !strcmp(tok, "quake"))
		ADDRARG("-quake");
	else if (!strcmp(tok, "q2") || !strcmp(tok, "quake2"))
		ADDRARG("-q2");
	else if (!strcmp(tok, "q3") || !strcmp(tok, "quake3"))
		ADDRARG("-q3");
	else if (!strcmp(tok, "hl") || !strcmp(tok, "halflife"))
		ADDRARG("-halflife");
	else if (!strcmp(tok, "h2") || !strcmp(tok, "hexen2"))
		ADDRARG("-hexen2");
	else if (!strcmp(tok, "nex") || !strcmp(tok, "nexuiz"))
		ADDRARG("-nexuiz");
	else
	{
		ADDRARG("-basegame");
		ADDCARG(tok);
	}
	//later options are additions to that
	while ((s = COM_ParseOut(s, tok, sizeof(tok))))
	{
		if (argc == sizeof(argv)/sizeof(argv[0]))
			break;
		ADDRARG("-addbasegame");
		ADDCARG(tok);
	}
	return argc;
}

#if _MSC_VER >= 1300
#define CATCHCRASH
#endif

#ifdef CATCHCRASH
#ifdef _DEBUG
#include "dbghelp.h"
DWORD CrashExceptionHandler (DWORD exceptionCode, LPEXCEPTION_POINTERS exceptionInfo);
#endif
#endif

int Plug_PluginThread(void *ctxptr)
{
	char *argv[16];
	int argc = 0;
	struct context *ctx = ctxptr;
	struct dl_download *dl;

#ifdef CATCHCRASH
	__try
#endif
	{
		argc = Plug_GenCommandline(ctx, argv, sizeof(argv)/sizeof(argv[0]));
		NPQTV_Sys_Startup(argc, argv);

		if (ctx->datadownload)
		{
			char *s = ctx->datadownload;
			char *c;
			vfsfile_t *f;
			Sys_LockMutex(ctx->mutex);
			while ((s = COM_ParseOut(s, com_token, sizeof(com_token))))
			{
				//FIXME: do we want to add some sort of file size indicator?
				c = strchr(com_token, ':');
				if (!c)
					continue;
				*c++ = 0;
				f = FS_OpenVFS(com_token, "rb", FS_ROOT);
				if (f)
				{
					Con_Printf("Already have %s\n", com_token);
					VFS_CLOSE(f);
					continue;
				}

				Con_Printf("Attempting to download %s\n", c);
				VS_DebugLocation(__FILE__, __LINE__, "Queuing Download %s", c);

				dl = DL_Create(c);
				dl->user_ctx = ctx;
				dl->next = ctx->packagelist;
				if (DL_CreateThread(dl, FS_OpenTemp(), UnpackAndExtractPakFiles_Complete))
					ctx->packagelist = dl;
			}
			Sys_UnlockMutex(ctx->mutex);
		}

		ctx->pub.downloading = true;
		while(host_initialized && !ctx->shutdown && ctx->packagelist)
		{
			int total=0, done=0;
			ctx->resetvideo = false;
			Sys_LockMutex(ctx->mutex);
			for (dl = ctx->packagelist; dl; dl = dl->next)
			{
				total += dl->totalsize;
				done += dl->completed;
			}
			dl = ctx->packagelist;
			if (total != ctx->pub.dlsize || done != ctx->pub.dldone)
			{
				ctx->pub.dlsize = total;
				ctx->pub.dldone = done;
				if (ctx->bfuncs.StatusChanged)
					ctx->bfuncs.StatusChanged(&ctx->pub);
			}
			if (!dl->file)
				ctx->packagelist = dl->next;
			else
				dl = NULL;
			Sys_UnlockMutex(ctx->mutex);

			/*file downloads are not canceled while the plugin is locked, to avoid a race condition*/
			if (dl)
				DL_Close(dl);
			Sleep(10);
		}
		ctx->pub.downloading = false;

		if (host_initialized && !ctx->shutdown)
		{
			Sys_LockMutex(ctx->mutex);
			ctx->resetvideo = false;
			sys_parentwindow = ctx->windowhnd;
			sys_parentwidth = ctx->windowwidth;
			sys_parentheight = ctx->windowheight;
			VS_DebugLocation(__FILE__, __LINE__, "Host_FinishInit");
			Host_FinishInit();
			Sys_UnlockMutex(ctx->mutex);
		}
		if (ctx->bfuncs.StatusChanged)
			ctx->bfuncs.StatusChanged(&ctx->pub);

		VS_DebugLocation(__FILE__, __LINE__, "main loop");
		while(host_initialized)
		{
			Sys_LockMutex(ctx->mutex);
			if (ctx->shutdown)
			{
				ctx->shutdown = false;
				VS_DebugLocation(__FILE__, __LINE__, "Sys_Shutdown");
				Host_Shutdown(); /*will shut down the host*/
			}
			else if (ctx->resetvideo)
			{
				sys_parentwindow = ctx->windowhnd;
				sys_parentwidth = ctx->windowwidth;
				sys_parentheight = ctx->windowheight;
				if (ctx->resetvideo == 2)
					SetParent(mainwindow, sys_parentwindow);
				ctx->resetvideo = false;
				Cbuf_AddText("vid_recenter\n", RESTRICT_LOCAL);
			}
			else
				NPQTV_Sys_MainLoop();
			Sys_UnlockMutex(ctx->mutex);
		}
	}
#ifdef CATCHCRASH
#ifdef _DEBUG
	__except (CrashExceptionHandler(GetExceptionCode(), GetExceptionInformation()))
	{

	}
#else
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		Host_Shutdown();
		MessageBox(sys_parentwindow, "Sorry, FTE plugin crashed.\nYou probably should restart your browser", "FTE crashed", 0);
	}
#endif
#endif

	Sys_LockMutex(ctx->mutex);
	while (ctx->packagelist)
	{
		dl = ctx->packagelist;
		ctx->packagelist = dl->next;

		/*don't close while locked*/
		Sys_UnlockMutex(ctx->mutex);
		DL_Close(dl);
		Sys_LockMutex(ctx->mutex);
	}
	ctx->pub.running = false;
	Sys_UnlockMutex(ctx->mutex);
	if (ctx->bfuncs.StatusChanged)
		ctx->bfuncs.StatusChanged(&ctx->pub);

	while(argc-- > 0)
		free(argv[argc]);

	activecontext = NULL;
	return 0;
}

void Plug_LockPlugin(struct context *ctx, qboolean lockstate)
{
	if (!ctx || !ctx->mutex)
		return;

	if (lockstate)
		Sys_LockMutex(ctx->mutex);
	else
		Sys_UnlockMutex(ctx->mutex);
}
//#define Plug_LockPlugin(c,s) do{Plug_LockPlugin(c,s);VS_DebugLocation(__FILE__, __LINE__, s?"Lock":"Unlock"); }while(0)

//begins the context, fails if one is already active
qboolean Plug_StartContext(struct context *ctx)
{
	if (activecontext)
		return false;
	ctx->pub.running = true;
	activecontext = ctx;
	ctx->mutex = Sys_CreateMutex();
	ctx->thread = Sys_CreateThread(Plug_PluginThread, ctx, 0);

	return true;
}

//asks a context to stop, is not instant.
void Plug_StopContext(struct context *ctx)
{
	if (ctx == NULL)
		ctx = activecontext;
	if (!ctx)
		return;
	if (ctx->pub.running == true)
		ctx->shutdown = true;
}

//creates a plugin context
struct context *Plug_CreateContext(void *sysctx, const struct browserfuncs *funcs)
{
	struct context *ctx;

	if (!sysctx || !funcs)
		return NULL;

	ctx = malloc(sizeof(struct context));
	if (!ctx)
		return NULL;
	memset(ctx, 0, sizeof(struct context));
	memcpy(&ctx->bfuncs, funcs, sizeof(ctx->bfuncs));

	//link the instance to the context and the context to the instance
	ctx->nppinstance = sysctx;

	ctx->gamename = strdup("q1");

	//add it to the linked list
	ctx->next = contextlist;
	contextlist = ctx;

	ctx->qtvf.connectiontype = QTVCT_NONE;

	return ctx;
}

//change the plugin's parent window, width, and height, returns true if the window handle actually changed, false otherwise
qboolean Plug_ChangeWindow(struct context *ctx, void *whnd, int width, int height)
{
	qboolean result = false;

	Plug_LockPlugin(ctx, true);

	//if the window changed
	if (ctx->windowhnd != whnd)
	{
		result = true;
		ctx->windowhnd = whnd;
		ctx->resetvideo = 2;
	}
	if (ctx->windowwidth != width || ctx->windowheight != height)
	{
		ctx->windowwidth = width;
		ctx->windowheight = height;
	}
	if (ctx->pub.running && !ctx->resetvideo)
		ctx->resetvideo = true;
	Plug_LockPlugin(ctx, false);

	while(ctx->pub.running && ctx->resetvideo)
		Sleep(10);

	return result;
}

void Plug_DestroyContext(struct context *ctx)
{
	struct context *prev;
	if (ctx == contextlist)
		contextlist = ctx->next;
	else
	{
		for (prev = contextlist; prev->next; prev = prev->next)
		{
			if (prev->next == ctx)
			{
				prev->next = ctx->next;
				break;
			}
		}
	}

	if (ctx->splashdownload)
	{
		DL_Close(ctx->splashdownload);
		ctx->splashdownload = NULL;
	}

	ctx->shutdown = true;
	if (ctx->thread)
		Sys_WaitOnThread(ctx->thread);
	ctx->thread = NULL;

	//actually these ifs are not required, just the frees
	if (ctx->gamename)
		free(ctx->gamename);
	if (ctx->password)
		free(ctx->password);
	if (ctx->datadownload)
		free(ctx->datadownload);
	if (ctx->splashdata)
		free(ctx->splashdata);

	free(ctx);
}


////////////////////////////////////////

#include "fs.h"
extern searchpathfuncs_t zipfilefuncs;

static int ExtractDataFile(const char *fname, int fsize, void *ptr)
{
	char buffer[8192];
	int read;
	void *zip = ptr;
	flocation_t loc;
	int slashes;
	const char *s;
	vfsfile_t *compressedpak;
	vfsfile_t *decompressedpak;

	if (zipfilefuncs.FindFile(zip, &loc, fname, NULL))
	{
		compressedpak = zipfilefuncs.OpenVFS(zip, &loc, "rb");
		if (compressedpak)
		{
			//this extra logic is so we can handle things like nexuiz/data/blah.pk3
			//as well as just data/blah.pk3
			slashes = 0;
			for (s = strchr(fname, '/'); s; s = strchr(s+1, '/'))
				slashes++;
			for (; slashes > 1; slashes--)
				fname = strchr(fname, '/')+1;

			if (!slashes)
			{
				FS_CreatePath(fname, FS_GAMEONLY);
				decompressedpak = FS_OpenVFS(fname, "wb", FS_GAMEONLY);
			}
			else
			{
				FS_CreatePath(fname, FS_ROOT);
				decompressedpak = FS_OpenVFS(fname, "wb", FS_ROOT);
			}
			if (decompressedpak)
			{
				for(;;)
				{
					read = VFS_READ(compressedpak, buffer, sizeof(buffer));
					if (read <= 0)
						break;
					VFS_WRITE(decompressedpak, buffer, read);
				}
				VFS_CLOSE(decompressedpak);
			}
			VFS_CLOSE(compressedpak);
		}
	}
	return true;
}

static void UnpackAndExtractPakFiles_Complete(struct dl_download *dl)
{
	extern searchpathfuncs_t zipfilefuncs;
	void *zip;

	Plug_LockPlugin(dl->user_ctx, true);

	if (dl->status == DL_FINISHED)
		zip = zipfilefuncs.OpenNew(dl->file, dl->url);
	else
		zip = NULL;
	/*the zip code will have eaten the file handle*/
	dl->file = NULL;
	if (zip)
	{
		/*scan it to extract its contents*/
		zipfilefuncs.EnumerateFiles(zip, "*.pk3", ExtractDataFile, zip);
		zipfilefuncs.EnumerateFiles(zip, "*.pak", ExtractDataFile, zip);

		/*close it, delete the temp file from disk, etc*/
		zipfilefuncs.ClosePath(zip);

		/*restart the filesystem so those new files can be found*/
		Cmd_ExecuteString("fs_restart", RESTRICT_LOCAL);
	}

	Plug_LockPlugin(dl->user_ctx, false);
}

void LoadSplashImage(struct dl_download *dl)
{
	struct context *ctx = dl->user_ctx;
	vfsfile_t *f = dl->file;
	int x, y;
	int width = 0;
	int height = 0;
	int len;
	char *buffer;
	unsigned char *image;

	ctx->splashwidth = 0;
	ctx->splashheight = 0;
	image = ctx->splashdata;
	ctx->splashdata = NULL;
	free(image);

	if (!f)
	{
		if (ctx->bfuncs.StatusChanged)
			ctx->bfuncs.StatusChanged(&ctx->pub);
		return;
	}

	len = VFS_GETLEN(f);
	buffer = malloc(len);
	VFS_READ(f, buffer, len);
	VFS_CLOSE(f);
	dl->file = NULL;

	image = NULL;
	if (!image)
		image = ReadJPEGFile(buffer, len, &width, &height);
	if (!image)
		image = ReadPNGFile(buffer, len, &width, &height, dl->url);

	free(buffer);
	if (image)
	{
		if (ctx->splashdata)
			free(ctx->splashdata);
		ctx->splashdata = malloc(width*height*4);
		for (y = 0; y < height; y++)
		{
			for (x = 0; x < width; x++)
			{
				ctx->splashdata[(y*width + x)*4+0] = image[((height-y-1)*width + x)*4+2];
				ctx->splashdata[(y*width + x)*4+1] = image[((height-y-1)*width + x)*4+1];
				ctx->splashdata[(y*width + x)*4+2] = image[((height-y-1)*width + x)*4+0];
			}
		}
		ctx->splashwidth = width;
		ctx->splashheight = height;
		BZ_Free(image);

		if (ctx->bfuncs.StatusChanged)
			ctx->bfuncs.StatusChanged(&ctx->pub);
	}
}

static void ReadQTVFileDescriptor(struct context *ctx, vfsfile_t *f, const char *name)
{
	CL_ParseQTVFile(f, name, &ctx->qtvf);

	pscript_property_splash_sets(ctx, ctx->qtvf.splashscreen);
}

void CL_QTVPlay (vfsfile_t *newf, qboolean iseztv);
static void BeginDemo(struct context *ctx, vfsfile_t *f, const char *name)
{
	if (!activecontext)
		activecontext = ctx;

	CL_QTVPlay(f, false);
}
static void EndDemo(struct context *ctx, vfsfile_t *f, const char *name)
{
	Cmd_ExecuteString("disconnect", RESTRICT_LOCAL);
}

/////////////////////////////////////






struct pscript_property
{
	char *name;
	qboolean onlyifactive;

	cvar_t *cvar;

	char *(*getstring)(struct context *ctx);
	void (*setstring)(struct context *ctx, const char *val);

	int (*getint)(struct context *ctx);
	void (*setint)(struct context *ctx, int val);

	float (*getfloat)(struct context *ctx);
	void (*setfloat)(struct context *ctx, float val);
};

int pscript_property_running_getb(struct context *ctx)
{
	if (ctx->pub.running)
		return true;
	else
		return false;
}

void pscript_property_running_setb(struct context *ctx, int i)
{
	i = !!i;
	if (ctx->pub.running == i)
		return;
	if (i)
		Plug_StartContext(ctx);
	else
		Plug_StopContext(ctx);
}

char *pscript_property_startserver_gets(struct context *ctx)
{
	return strdup(ctx->qtvf.server);
}
void pscript_property_startserver_sets(struct context *ctx, const char *val)
{
	if (strchr(val, '$') || strchr(val, ';') || strchr(val, '\n'))
		return;

	ctx->qtvf.connectiontype = QTVCT_JOIN;
	Q_strncpyz(ctx->qtvf.server, val, sizeof(ctx->qtvf.server));
}
char *pscript_property_curserver_gets(struct context *ctx)
{
	extern char lastdemoname[];
	if (!pscript_property_running_getb(ctx))
		return pscript_property_startserver_gets(ctx);

	if (cls.demoplayback)
		return strdup(va("demo:%s",lastdemoname));
	else if (cls.state != ca_disconnected)
		return strdup(cls.servername);
	else
		return strdup("");
}
void pscript_property_curserver_sets(struct context *ctx, const char *val)
{
	if (strchr(val, '$') || strchr(val, ';') || strchr(val, '\n'))
		return;

	if (!pscript_property_running_getb(ctx))
	{
		pscript_property_startserver_sets(ctx, val);
		return;
	}

	Q_strncpyz(cls.servername, val, sizeof(cls.servername));
	CL_BeginServerConnect(0);
}

void pscript_property_stream_sets(struct context *ctx, const char *val)
{
	if (strchr(val, '$') || strchr(val, ';') || strchr(val, '\n'))
		return;

	ctx->qtvf.connectiontype = QTVCT_STREAM;
	Q_strncpyz(ctx->qtvf.server, val, sizeof(ctx->qtvf.server));

	if (pscript_property_running_getb(ctx))
		Cmd_ExecuteString(va("qtvplay \"%s\"\n", val), RESTRICT_INSECURE);
}
void pscript_property_map_sets(struct context *ctx, const char *val)
{
	if (strchr(val, '$') || strchr(val, ';') || strchr(val, '\n'))
		return;
	ctx->qtvf.connectiontype = QTVCT_MAP;
	Q_strncpyz(ctx->qtvf.server, val, sizeof(ctx->qtvf.server));

	if (pscript_property_running_getb(ctx))
		Cmd_ExecuteString(va("map \"%s\"\n", val), RESTRICT_INSECURE);
}

float pscript_property_curver_getf(struct context *ctx)
{
	return version_number();
}

void pscript_property_availver_setf(struct context *ctx, float val)
{
	ctx->pub.availver = val;
	if (ctx->pub.availver <= version_number())
		ctx->pub.availver = 0;
}

void pscript_property_datadownload_sets(struct context *ctx, const char *val)
{
	free(ctx->datadownload);
	ctx->datadownload = strdup(val);
}

void pscript_property_game_sets(struct context *ctx, const char *val)
{
	if (strchr(val, '$') || strchr(val, ';') || strchr(val, '\n'))
		return;

	if (!strstr(val, "."))
		if (!strstr(val, "/"))
			if (!strstr(val, "\\"))
				if (!strstr(val, ":"))
				{
					free(ctx->gamename);
					ctx->gamename = strdup(val);
				}
}

void pscript_property_splash_sets(struct context *ctx, const char *val)
{
	if (ctx->splashdownload)
		DL_Close(ctx->splashdownload);
	ctx->splashdownload = NULL;

	if (val != ctx->qtvf.splashscreen)
		Q_strncpyz(ctx->qtvf.splashscreen, val, sizeof(ctx->qtvf.splashscreen));

	ctx->splashdownload = DL_Create(ctx->qtvf.splashscreen);
	ctx->splashdownload->user_ctx = ctx;
	if (!DL_CreateThread(ctx->splashdownload, VFSPIPE_Open(), LoadSplashImage))
	{
		DL_Close(ctx->splashdownload);
		ctx->splashdownload = NULL;
	}
}

char *pscript_property_build_gets(struct context *ctx)
{
	return strdup(DISTRIBUTION " " __DATE__ " " __TIME__
#if defined(DEBUG) || defined(_DEBUG)
		" (debug)"
#endif
		);
}

extern cvar_t skin, team, topcolor, bottomcolor, vid_fullscreen, cl_download_mapsrc;
static struct pscript_property pscript_properties[] =
{
	{"running",		false,	NULL,	NULL, NULL, pscript_property_running_getb, pscript_property_running_setb},
	{"startserver",	false,	NULL,	pscript_property_startserver_gets, pscript_property_startserver_sets},
	{"server",		false,	NULL,	pscript_property_curserver_gets, pscript_property_curserver_sets},
	{"join",		false,	NULL,	NULL, pscript_property_curserver_sets},
	{"playername",	true,	&name},
	{NULL,			true,	&skin},
	{NULL,			true,	&team},
	{NULL,			true,	&topcolor},
	{NULL,			true,	&bottomcolor},
	{NULL,			true,	&password},
//	{NULL,			true,	&spectator},
	{"mapsrc",		true,	&cl_download_mapsrc},
	{"fullscreen",	true,	&vid_fullscreen},

	{"datadownload",false,	NULL,	NULL, pscript_property_datadownload_sets},
	
	{"game",		false,	NULL,	NULL, pscript_property_game_sets},
	{"availver",	false,	NULL,	NULL, NULL,	NULL, NULL,	NULL, pscript_property_availver_setf},
	{"plugver",		false,	NULL,	NULL, NULL,	NULL, NULL,	pscript_property_curver_getf},
	
	{"splash",		false,	NULL,	NULL, pscript_property_splash_sets},

	{"stream",		false,	NULL,	NULL, pscript_property_stream_sets},
	{"map",			false,	NULL,	NULL, pscript_property_map_sets},

	{"build",		false,	NULL,	pscript_property_build_gets},
/*
		else if (!stricmp(argn[i], "connType"))
		{
			if (ctx->qtvf.connectiontype)
				continue;
			if (!stricmp(argn[i], "join"))
				ctx->qtvf.connectiontype = QTVCT_JOIN;
			else if (!stricmp(argn[i], "qtv"))
				ctx->qtvf.connectiontype = QTVCT_STREAM;
			else if (!stricmp(argn[i], "connect"))
				ctx->qtvf.connectiontype = QTVCT_CONNECT;
			else if (!stricmp(argn[i], "map"))
				ctx->qtvf.connectiontype = QTVCT_MAP;
			else if (!stricmp(argn[i], "join"))
				ctx->qtvf.connectiontype = QTVCT_JOIN;
			else if (!stricmp(argn[i], "observe"))
				ctx->qtvf.connectiontype = QTVCT_OBSERVE;
			else
				ctx->qtvf.connectiontype = QTVCT_NONE;
		}
		else if (!stricmp(argn[i], "map"))
		{
			if (ctx->qtvf.connectiontype)
				continue;
			ctx->qtvf.connectiontype = QTVCT_MAP;
			Q_strncpyz(ctx->qtvf.server, argv[i], sizeof(ctx->qtvf.server));
		}
		else if (!stricmp(argn[i], "join"))
		{
			if (ctx->qtvf.connectiontype)
				continue;
			ctx->qtvf.connectiontype = QTVCT_JOIN;
			Q_strncpyz(ctx->qtvf.server, argv[i], sizeof(ctx->qtvf.server));
		}
		else if (!stricmp(argn[i], "observe"))
		{
			if (ctx->qtvf.connectiontype)
				continue;
			ctx->qtvf.connectiontype = QTVCT_OBSERVE;
			Q_strncpyz(ctx->qtvf.server, argv[i], sizeof(ctx->qtvf.server));
		}
		else if (!stricmp(argn[i], "password"))
		{
			ctx->password = strdup(argv[i]);
		}
		else if (!stricmp(argn[i], "onStart"))
		{
			ctx->onstart = strdup(argv[i]);
		}
		else if (!stricmp(argn[i], "onEnd"))
		{
			ctx->onend = strdup(argv[i]);
		}
		else if (!stricmp(argn[i], "onDemoEnd"))
		{
			ctx->ondemoend = strdup(argv[i]);
		}
*/
	{NULL}
};

struct pscript_property *Plug_FindProp(struct context *ctx, const char *field)
{
	struct pscript_property *prop;
	for (prop = pscript_properties; prop->name||prop->cvar; prop++)
	{
		if (!stricmp(prop->name?prop->name:prop->cvar->name, field))
		{
			if (prop->onlyifactive)
			{
				if (!ctx->pub.running)
					return NULL;
			}
			return prop;
		}
	}
	return NULL;
}

qboolean Plug_SetString(struct context *ctx, struct pscript_property *field, const char *value)
{
	if (!ctx || !field || !value)
		return false;
	if (field->setstring)
	{
		Plug_LockPlugin(ctx, true);
		field->setstring(ctx, value);
		Plug_LockPlugin(ctx, false);
	}
	else if (field->setint)
	{
		Plug_LockPlugin(ctx, true);
		field->setint(ctx, atoi(value));
		Plug_LockPlugin(ctx, false);
	}
	else if (field->setfloat)
	{
		Plug_LockPlugin(ctx, true);
		field->setfloat(ctx, atof(value));
		Plug_LockPlugin(ctx, false);
	}
	else if (field->cvar && ctx->pub.running)
	{
		Plug_LockPlugin(ctx, true);
		Cvar_Set(field->cvar, value);
		Plug_LockPlugin(ctx, false);
	}
	else
		return false;
	return true;
}
qboolean Plug_SetInteger(struct context *ctx, struct pscript_property *field, int value)
{
	if (!ctx || !field)
		return false;
	if (field->setint)
	{
		Plug_LockPlugin(ctx, true);
		field->setint(ctx, value);
		Plug_LockPlugin(ctx, false);
	}
	else if (field->setfloat)
	{
		Plug_LockPlugin(ctx, true);
		field->setfloat(ctx, value);
		Plug_LockPlugin(ctx, false);
	}
	else if (field->cvar && ctx->pub.running)
	{
		Plug_LockPlugin(ctx, true);
		Cvar_SetValue(field->cvar, value);
		Plug_LockPlugin(ctx, false);
	}
	else
		return false;
	return true;
}
qboolean Plug_SetFloat(struct context *ctx, struct pscript_property *field, float value)
{
	if (!ctx || !field)
		return false;
	if (field->setfloat)
	{
		Plug_LockPlugin(ctx, true);
		field->setfloat(ctx, value);
		Plug_LockPlugin(ctx, false);
	}
	else if (field->setint)
	{
		Plug_LockPlugin(ctx, true);
		field->setint(ctx, value);
		Plug_LockPlugin(ctx, false);
	}
	else if (field->cvar && ctx->pub.running)
	{
		Plug_LockPlugin(ctx, true);
		Cvar_SetValue(field->cvar, value);
		Plug_LockPlugin(ctx, false);
	}
	else
		return false;
	return true;
}

qboolean Plug_GetString(struct context *ctx, struct pscript_property *field, const char **value)
{
	if (field->getstring)
	{
		*value = field->getstring(ctx);
		return true;
	}
	return false;
}
void Plug_GotString(const char *value)
{
	free((char*)value);
}
qboolean Plug_GetInteger(struct context *ctx, struct pscript_property *field, int *value)
{
	if (field->getint)
	{
		*value = field->getint(ctx);
		return true;
	}
	return false;
}
qboolean Plug_GetFloat(struct context *ctx, struct pscript_property *field, float *value)
{
	if (field->getfloat)
	{
		*value = field->getfloat(ctx);
		return true;
	}
	return false;
}

#ifdef _WIN32
void *Plug_GetSplashBack(struct context *ctx, void *hdc, int *width, int *height)
{
	BITMAPINFOHEADER bmh;

	if (!ctx->splashdata)
		return NULL;

	bmh.biSize = sizeof(bmh);
    bmh.biWidth = *width = ctx->splashwidth;
    bmh.biHeight = *height = ctx->splashheight;
    bmh.biPlanes = 1;
    bmh.biBitCount = 32;
    bmh.biCompression = BI_RGB;
    bmh.biSizeImage = 0;
    bmh.biXPelsPerMeter = 0;
    bmh.biYPelsPerMeter = 0;
    bmh.biClrUsed = 0;
    bmh.biClrImportant = 0;

	return CreateDIBitmap(hdc, 
            &bmh, 
            CBM_INIT, 
            (LPSTR)ctx->splashdata, 
            (LPBITMAPINFO)&bmh, 
            DIB_RGB_COLORS ); 
}
void Plug_ReleaseSplashBack(struct context *ctx, void *bmp)
{
	DeleteObject(bmp);
}
#endif

static const struct plugfuncs exportedplugfuncs_1 =
{
	Plug_CreateContext,
	Plug_DestroyContext,
	Plug_LockPlugin,
	Plug_StartContext,
	Plug_StopContext,
	Plug_ChangeWindow,

	Plug_FindProp,
	Plug_SetString,
	Plug_GetString,
	Plug_GotString,
	Plug_SetInteger,
	Plug_GetInteger,
	Plug_SetFloat,
	Plug_GetFloat,

	Plug_GetSplashBack,
	Plug_ReleaseSplashBack
};

const struct plugfuncs *Plug_GetFuncs(int ver)
{
	if (ver == 1)
		return &exportedplugfuncs_1;
	else
		return NULL;
}
