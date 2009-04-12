#include "quakedef.h"
#include "winquake.h"
#define bool int	//we ain't c++ (grr microsoft stdbool.h gief!)

#ifdef _WIN32
#ifndef _WINDOWS
#define _WINDOWS	//stupid GCC
#endif
#endif

#include "npapi/npupp.h"

#define Q_STRINGZ_TO_NPVARIANT(_val, _v)                                        \
NP_BEGIN_MACRO                                                                \
	NPString str = { _val, strlen(_val) };                                    \
    (_v).type = NPVariantType_String;                                         \
    (_v).value.stringValue = str;                                             \
NP_END_MACRO
#undef STRINGZ_TO_NPVARIANT
#define STRINGZ_TO_NPVARIANT Q_STRINGZ_TO_NPVARIANT



#define NPQTV_VERSION 0.1

#define FIREFOX_BUGS_OVER_25MB

//TODO: player name input (before allowing them to join)
//TODO: fix active gl context (per thread, and we hijacked the browser's thread)


NPNetscapeFuncs *browserfuncs;




#ifdef _WIN32
#ifndef GetWindowLongPtr
#define GetWindowLongPtr GetWindowLong
#endif
#ifndef SetWindowLongPtr
#define SetWindowLongPtr SetWindowLong
#define LONG_PTR LONG
#endif


extern HWND sys_parentwindow;
extern unsigned int sys_parentwidth;
extern unsigned int sys_parentheight;
HINSTANCE	global_hInstance;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		global_hInstance = hinstDLL;
		break;
	default:
		break;
	}
	return TRUE;
}
#endif






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

char binaryname[MAX_PATH];

struct qstream
{
	vfsfile_t *pipe;
	struct pipetype *type;

	struct qstream *next;

	char url[1];
};
struct context
{
	NPWindow window;
	qboolean contextrunning;
	int waitingfordatafiles;
	float availver;

#ifdef _WIN32
	WNDPROC oldproc;
#endif

	char *datadownload;
	char *gamename;
	char *password;
	char *onstart;
	char *onend;
	char *ondemoend;

	NPP nppinstance;

	struct qstream *donestreams;

	int wait_size;
	int wait_offset;
	struct qstream *wait_stream;

	qtvfile_t qtvf;

	unsigned char *splashdata;
	int splashwidth;
	int splashheight;

	struct context *next;
};

struct context *activecontext;
struct context *contextlist;









////////////////////////////////////////

struct pipetype
{
	enum {
		WAIT_NO,
		WAIT_YES,
		WAIT_DONE
	} wait;
	qboolean needseeking;
	void (*completionfunc)	(struct context *ctx, vfsfile_t *file, const char *streamsource);
	void (*beginfunc)		(struct context *ctx, vfsfile_t *file, const char *streamsource);
};

#include "fs.h"
extern searchpathfuncs_t zipfilefuncs;

int ExtractDataFile(const char *fname, int fsize, void *ptr)
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

void UnpackAndExtractPakFiles_Complete(struct context *ctx, vfsfile_t *file, const char *streamsource)
{
	extern searchpathfuncs_t zipfilefuncs;
	void *zip;

	zip = zipfilefuncs.OpenNew(file, streamsource);
	if (zip)
	{
		zipfilefuncs.EnumerateFiles(zip, "*.pk3", ExtractDataFile, zip);
		zipfilefuncs.EnumerateFiles(zip, "*.pak", ExtractDataFile, zip);

		zipfilefuncs.ClosePath(zip);

		Cmd_ExecuteString("fs_restart", RESTRICT_LOCAL);
	}
}
struct pipetype UnpackAndExtractPakFiles =
{
	WAIT_YES,
	true,
	UnpackAndExtractPakFiles_Complete
};

void LoadSplashImage(struct context *ctx, vfsfile_t *f, const char *name)
{
	int x, y;
	int width = 0;
	int height = 0;
	int len = VFS_GETLEN(f);
	char *buffer = malloc(len);
	unsigned char *image;
	VFS_READ(f, buffer, len);
	VFS_CLOSE(f);

	image = NULL;
	if (!image)
		image = ReadJPEGFile(buffer, len, &width, &height);
	if (!image)
		image = ReadPNGFile(buffer, len, &width, &height, name);

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

		if (ctx->window.window)
			InvalidateRgn(ctx->window.window, NULL, FALSE);
	}
}

struct pipetype SplashscreenImageDescriptor =
{
	WAIT_DONE,
	false,
	LoadSplashImage
};

static void ReadQTVFileDescriptor(struct context *ctx, vfsfile_t *f, const char *name)
{
	CL_ParseQTVFile(f, name, &ctx->qtvf);

	if (*ctx->qtvf.splashscreen)
	{
		browserfuncs->geturlnotify(ctx->nppinstance, ctx->qtvf.splashscreen, NULL, &SplashscreenImageDescriptor);
	}
}
struct pipetype QTVFileDescriptor =
{
	WAIT_DONE,
	false,
	ReadQTVFileDescriptor
};
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
struct pipetype DemoFileDescriptor =
{
	WAIT_NO,
	false,
	EndDemo,
	BeginDemo
};

/////////////////////////////////////


#ifdef _WIN32
void DrawWndBack(struct context *ctx, HWND hWnd, HDC hdc, PAINTSTRUCT *p)
{
	if (ctx->splashdata)
	{
		HBITMAP bmp;
		BITMAPINFOHEADER bmh;
		HDC memDC;

		bmh.biSize = sizeof(bmh);
        bmh.biWidth = ctx->splashwidth;
        bmh.biHeight = ctx->splashheight;
        bmh.biPlanes = 1;
        bmh.biBitCount = 32;
        bmh.biCompression = BI_RGB;
        bmh.biSizeImage = 0;
        bmh.biXPelsPerMeter = 0;
        bmh.biYPelsPerMeter = 0;
        bmh.biClrUsed = 0;
        bmh.biClrImportant = 0;

		memDC = CreateCompatibleDC(hdc);
		bmp = CreateDIBitmap(hdc, 
                &bmh, 
                CBM_INIT, 
                (LPSTR)ctx->splashdata, 
                (LPBITMAPINFO)&bmh, 
                DIB_RGB_COLORS ); 

		SelectObject(memDC, bmp);
//		StretchBlt(hdc, 0, 0, p->rcPaint.right-p->rcPaint.left, p->rcPaint.bottom-p->rcPaint.top, memDC, 0, 0, ctx->splashwidth, ctx->splashheight, SRCCOPY);
		StretchBlt(hdc, 0, 0, ctx->window.width, ctx->window.height, memDC, 0, 0, ctx->splashwidth, ctx->splashheight, SRCCOPY);
		SelectObject(memDC, NULL);
		DeleteDC(memDC);
		DeleteObject(bmp);
	}
	else
		PatBlt(hdc, p->rcPaint.left, p->rcPaint.top, p->rcPaint.right-p->rcPaint.left,p->rcPaint.bottom-p->rcPaint.top,PATCOPY);
}

char *cleanarg(char *arg)
{
	//no hacking us, please.
	while (*arg == '-' || *arg == '+')
		arg++;
	while (*arg && *arg <= ' ')
		arg++;
	if (*arg)
		return arg;
	return "badarg";
}

LRESULT CALLBACK MyPluginWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	struct qstream *str;
	struct context *ctx;
	ctx = (struct context *)GetWindowLongPtr(hWnd, GWL_USERDATA);
	if (!ctx)
		return DefWindowProc(hWnd, msg, wParam, lParam);

	switch(msg)
	{
	case WM_MOVE:
		if (ctx->contextrunning)
		{
			PostMessage(mainwindow, WM_MOVE, 0, 0);
		}
		break;
	case WM_TIMER:
		if (ctx->contextrunning && !ctx->waitingfordatafiles)
		{
			while (ctx->donestreams)
			{
				str = ctx->donestreams;
				ctx->donestreams = str->next;

				if (str->pipe)
				{
					if (str->type->completionfunc)
						str->type->completionfunc(ctx, str->pipe, str->url);
					else
						VFS_CLOSE(str->pipe);
				}

				free(str);
			}

			if (sys_parentwindow != ctx->window.window)
			{
				if (qrenderer == -1)
				{
					//urgh, its not started up yet
					sys_parentwindow = ctx->window.window;

					Host_FinishInit();
					if (ctx->onstart)
						browserfuncs->geturl(ctx->nppinstance, va("javascript:%s;", ctx->onstart), "_self");
				}
				else
				{
					sys_parentwindow = ctx->window.window;
					if (sys_parentwindow)
					{
						sys_parentwidth = ctx->window.width;
						sys_parentheight = ctx->window.height;
						Cmd_ExecuteString("vid_restart", RESTRICT_LOCAL);
					}
				}

			}
			else if (sys_parentwindow)
			{
				NPQTV_Sys_MainLoop();
				if (!host_initialized)
				{
					//quit was issued
					ctx->contextrunning = false;
					activecontext = NULL;
					InvalidateRgn(hWnd, NULL, FALSE);

					if (ctx->onend)
						browserfuncs->geturl(ctx->nppinstance, va("javascript:%s;", ctx->onend), "_self");
				}
			}
		}
		return TRUE;

	case WM_PAINT:
		if (activecontext == ctx && !ctx->contextrunning && ctx->window.window)
		{
			char *s;
			int argc;
			char *argv[16];
			sys_parentwindow = NULL;

			GetModuleFileName(global_hInstance, binaryname, sizeof(binaryname));
			argv[0] = binaryname;
			argc = 1;

			activecontext = ctx;

			switch(ctx->qtvf.connectiontype)
			{
			default:
				break;
			case QTVCT_STREAM:
				argv[argc++] = "+qtvplay";
				argv[argc++] = cleanarg(ctx->qtvf.server);
				break;
			case QTVCT_CONNECT:
				argv[argc++] = "+connect";
				argv[argc++] = cleanarg(ctx->qtvf.server);
				break;
			case QTVCT_JOIN:
				argv[argc++] = "+join";
				argv[argc++] = cleanarg(ctx->qtvf.server);
				break;
			case QTVCT_OBSERVE:
				argv[argc++] = "+observe";
				argv[argc++] = cleanarg(ctx->qtvf.server);
				break;
			case QTVCT_MAP:
				argv[argc++] = "+map";
				argv[argc++] = cleanarg(ctx->qtvf.server);
				break;
			}

			if (ctx->password)
			{
				argv[argc++] = "+password";
				argv[argc++] = cleanarg(ctx->password);
			}

			//figure out the game dirs (first token is the base game)
			s = ctx->gamename;
			s = COM_ParseOut(s, com_token, sizeof(com_token));
			if (!*com_token || !strcmp(com_token, "q1") || !strcmp(com_token, "qw") || !strcmp(com_token, "quake"))
				argv[argc++] = "-quake";
			else if (!strcmp(com_token, "q2") || !strcmp(com_token, "quake2"))
				argv[argc++] = "-q2";
			else if (!strcmp(com_token, "q3") || !strcmp(com_token, "quake3"))
				argv[argc++] = "-q3";
			else if (!strcmp(com_token, "hl") || !strcmp(com_token, "halflife"))
				argv[argc++] = "-halflife";
			else if (!strcmp(com_token, "h2") || !strcmp(com_token, "hexen2"))
				argv[argc++] = "-hexen2";
			else if (!strcmp(com_token, "nex") || !strcmp(com_token, "nexuiz"))
				argv[argc++] = "-nexuiz";
			else
			{
				argv[argc++] = "-basegame";
				argv[argc++] = strdup(cleanarg(com_token));	//FIXME: this will leak
			}
			//later options are additions to that
			while ((s = COM_ParseOut(s, com_token, sizeof(com_token))))
			{
				if (argc == sizeof(argv)/sizeof(argv[0]))
					break;
				argv[argc++] = "-addbasegame";
				argv[argc++] = strdup(cleanarg(com_token));	//FIXME: this will leak
			}
			
			sys_parentwidth = ctx->window.width;
			sys_parentheight = ctx->window.height;
			ctx->contextrunning = NPQTV_Sys_Startup(argc, argv);

			//now that the file system is started up, check to make sure its complete
			if (ctx->datadownload)
			{
				char *s = ctx->datadownload;
				char *c;
				vfsfile_t *f;
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
					if (!browserfuncs->geturlnotify(ctx->nppinstance, c, NULL, &UnpackAndExtractPakFiles))
						ctx->waitingfordatafiles++;
				}
			}

			if (ctx->contextrunning)
			{
				//windows timers have low precision, ~10ms
				//they're low priority anyway, so we might as well just create lots and spam them
				SetTimer(hWnd, 1, 1, NULL);
				SetTimer(hWnd, 2, 1, NULL);
				SetTimer(hWnd, 3, 1, NULL);
				SetTimer(hWnd, 4, 1, NULL);
				SetTimer(hWnd, 5, 1, NULL);
			}
		}

		if (ctx->waitingfordatafiles)
		{
			HDC hdc;
			PAINTSTRUCT paint;
			char *s;

			hdc = BeginPaint(hWnd, &paint);
			DrawWndBack(ctx, hWnd, hdc, &paint);
			SetBkMode(hdc, TRANSPARENT);
			TextOutA(hdc, 0, 0, "Downloading Data, please wait", 16);
			if (!ctx->wait_stream)
				s = "connecting";
			else if (ctx->wait_size > 0)
				s = va("%i bytes (%i%%)", ctx->wait_offset, (int)((100.0f*ctx->wait_offset)/ctx->wait_size));
			else
				s = va("%i bytes", ctx->wait_offset);
			TextOutA(hdc, 0, 32, s, strlen(s));
			EndPaint(hWnd, &paint);
			return TRUE;
		}
		else
		{
			HDC hdc;
			PAINTSTRUCT paint;
			char *s;

			hdc = BeginPaint(hWnd, &paint);
			DrawWndBack(ctx, hWnd, hdc, &paint);
			SetBkMode(hdc, TRANSPARENT);
			if (!ctx->contextrunning)
			{
				if (!activecontext)
				{
					s = "Click to activate";
					TextOutA(hdc, 0, 0, s, strlen(s));
				}
				if (ctx->availver)
				{
					s = va("Your plugin is out of date");
					TextOutA(hdc, 0, 16, s, strlen(s));
					s = va("Version %3.1f is available", ctx->availver);
					TextOutA(hdc, 0, 32, s, strlen(s));
				}
			}
			EndPaint(hWnd, &paint);
			return TRUE;
		}
		break;

	case WM_LBUTTONDOWN:
		if (!activecontext)
		{
			activecontext = ctx;
			InvalidateRgn(hWnd, NULL, FALSE);
		}
		else if (activecontext != ctx)
			Cbuf_AddText("quit\n", RESTRICT_LOCAL);
		break;
	default:
		break;
	}

	//I would call the previous wndproc... but that crashes firefox
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

#endif

NPError NP_LOADDS NPP_New(NPMIMEType pluginType, NPP instance,
                          uint16 mode, int16 argc, char* argn[],
                          char* argv[], NPSavedData* saved)
{
	int i;
	struct context *ctx;

	if (!instance || instance->pdata)
	{
		return NPERR_INVALID_INSTANCE_ERROR;
	}
	if (mode != NP_EMBED && mode != NP_FULL)
	{
		return NPERR_INVALID_PLUGIN_ERROR;
	}

	ctx = malloc(sizeof(struct context));
	if (!ctx)
	{
		return NPERR_OUT_OF_MEMORY_ERROR;
	}

	memset(ctx, 0, sizeof(struct context));

	//link the instance to the context and the context to the instance
	instance->pdata = ctx;
	ctx->nppinstance = instance;

	ctx->gamename = strdup("q1");

	//parse out the properties
	for (i = 0; i < argc; i++)
	{
		if (!stricmp(argn[i], "dataDownload"))
		{
			ctx->datadownload = strdup(argv[i]);
		}
		else if (!stricmp(argn[i], "game"))
		{
			if (!strstr(argn[i], "."))
				if (!strstr(argn[i], "/"))
					if (!strstr(argn[i], "\\"))
						if (!strstr(argn[i], ":"))
						{
							free(ctx->gamename);
							ctx->gamename = strdup(argv[i]);
						}
		}
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
		else if (!stricmp(argn[i], "server") || !stricmp(argn[i], "stream"))
		{
			if (*ctx->qtvf.server)
				continue;
			Q_strncpyz(ctx->qtvf.server, argv[i], sizeof(ctx->qtvf.server));
		}
		else if (!stricmp(argn[i], "map"))
		{
			if (ctx->qtvf.connectiontype)
				continue;
			ctx->qtvf.connectiontype = QTVCT_MAP;
			Q_strncpyz(ctx->qtvf.server, argv[i], sizeof(ctx->qtvf.server));
		}
		else if (!stricmp(argn[i], "stream"))
		{
			if (ctx->qtvf.connectiontype)
				continue;
			ctx->qtvf.connectiontype = QTVCT_STREAM;
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
		else if (!stricmp(argn[i], "splash"))
		{
			Q_strncpyz(ctx->qtvf.splashscreen, argv[i], sizeof(ctx->qtvf.splashscreen));
			browserfuncs->geturlnotify(ctx->nppinstance, ctx->qtvf.splashscreen, NULL, &SplashscreenImageDescriptor);
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
		else if (!stricmp(argn[i], "availVer"))
		{
			ctx->availver = atof(argv[i]);
			if (ctx->availver <= NPQTV_VERSION)
				ctx->availver = 0;
		}
		else if (!stricmp(argn[i], "begin"))
		{
			if (atoi(argv[i]) && !activecontext)
				activecontext = ctx;
		}
	}

	if (!*ctx->qtvf.server)
		ctx->qtvf.connectiontype = QTVCT_NONE;
	else if (ctx->qtvf.connectiontype == QTVCT_NONE)
		ctx->qtvf.connectiontype = QTVCT_STREAM;

	//add it to the linked list
	ctx->next = contextlist;
	contextlist = ctx;
	return NPERR_NO_ERROR;
}
NPError NP_LOADDS NPP_Destroy(NPP instance, NPSavedData** save)
{
	struct context *ctx = instance->pdata;
	struct context *prev;

	if (!ctx)
		return NPERR_INVALID_INSTANCE_ERROR;

#ifdef _WIN32
	if (ctx->window.window)
	{
		if (ctx->oldproc)
			SetWindowLongPtr(ctx->window.window, GWL_WNDPROC, (LONG_PTR)ctx->oldproc);
		SetWindowLongPtr(ctx->window.window, GWL_USERDATA, (LONG_PTR)NULL);
	}
#endif

	//actually these ifs are not required, just the frees
	if (ctx->gamename)
		free(ctx->gamename);
	if (ctx->password)
		free(ctx->password);
	if (ctx->datadownload)
		free(ctx->datadownload);
	if (ctx->splashdata)
		free(ctx->splashdata);

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

	if (ctx->contextrunning)
	{
		NPQTV_Sys_Shutdown();
	}
	if (ctx == activecontext)
	{
		activecontext = NULL;
		sys_parentwindow = NULL;
	}

	free(ctx);
	instance->pdata = NULL;

	return NPERR_NO_ERROR;
}
NPError NP_LOADDS NPP_SetWindow(NPP instance, NPWindow* window)
{
	extern cvar_t vid_width;
	struct context *ctx = instance->pdata;

#ifdef _WIN32
	HWND oldwindow;
	WNDPROC p;

	if (!ctx)
		return NPERR_INVALID_INSTANCE_ERROR;

	oldwindow = ctx->window.window;

	memcpy(&ctx->window, window, sizeof(ctx->window));

	//if the window changed
	if (ctx->window.window != oldwindow)
	{
		//we switched window?
		if (oldwindow && ctx->oldproc)
		{
			SetWindowLongPtr(oldwindow, GWL_WNDPROC, (LONG_PTR)ctx->oldproc);
			ctx->oldproc = NULL;
		}

		p = (WNDPROC)GetWindowLongPtr(ctx->window.window, GWL_WNDPROC);
		if (p != MyPluginWndProc)
			ctx->oldproc = p;

		SetWindowLongPtr(ctx->window.window, GWL_WNDPROC, (LONG_PTR)MyPluginWndProc);
		SetWindowLongPtr(ctx->window.window, GWL_USERDATA, (LONG_PTR)ctx);

		if (ctx->contextrunning && mainwindow && oldwindow == sys_parentwindow)
		{
			sys_parentwindow = ctx->window.window;
			SetParent(mainwindow, ctx->window.window);

			oldwindow = sys_parentwindow;
		}
	}

	if (ctx->contextrunning)
	{
		extern cvar_t vid_conwidth;
		sys_parentwidth = ctx->window.width;
		sys_parentheight = ctx->window.height;
		Cvar_ForceCallback(&vid_width);
		Cvar_ForceCallback(&vid_conwidth);
	}

	InvalidateRgn(ctx->window.window, NULL, FALSE);
#endif
	return NPERR_NO_ERROR;
}

NPError NP_LOADDS NPP_NewStream(NPP instance, NPMIMEType type,
                                NPStream* stream, NPBool seekable,
                                uint16* stype)
{
//	struct context *ctx = instance->pdata;
	struct qstream *qstr;

	stream->pdata = qstr = malloc(sizeof(*qstr) + strlen(stream->url));
	memset(qstr, 0, sizeof(*qstr));
	strcpy(qstr->url, stream->url);

	if (!stream->notifyData)
	{
		//choose source type based on mime type
		if (!strncmp(type, "text/x-quaketvident", 5))
			stream->notifyData = &QTVFileDescriptor;
		else if (!strcmp(type, "application/x-multiviewdemo"))
			stream->notifyData = &DemoFileDescriptor;

		//well that failed, try choosing based on extension
		else if (!strcmp(COM_FileExtension(stream->url), "qtv"))
			stream->notifyData = &QTVFileDescriptor;

		else 
			return NPERR_INVALID_PARAM;
	}
	qstr->type = stream->notifyData;

	if (qstr->type->needseeking)
	{
		*stype = NP_ASFILEONLY;	//everything is a download

#ifdef FIREFOX_BUGS_OVER_25MB
		*stype = NP_NORMAL;
		qstr->pipe = FS_OpenTemp();
#endif
	}
	else
	{
		*stype = NP_NORMAL;
		qstr->pipe = VFSPIPE_Open();
	}

	return NPERR_NO_ERROR;
}
NPError NP_LOADDS NPP_DestroyStream(NPP instance, NPStream* stream,
                                    NPReason reason)
{
	struct context *ctx = instance->pdata;
	struct qstream *qstr = stream->pdata;

	if (!qstr)	//urm, got canceled before it finished downloading?
		return NPERR_NO_ERROR;

	if (ctx->wait_stream == qstr)
		ctx->wait_stream = NULL;

	if (qstr->type->wait == WAIT_YES)
	{
		ctx->waitingfordatafiles--;
	}

	if (qstr->type->wait == WAIT_DONE)
		qstr->type->completionfunc(ctx, qstr->pipe, qstr->url);
	else
	{
		qstr->next = ctx->donestreams;
		ctx->donestreams = qstr;
	}

	return NPERR_NO_ERROR;
}
int32   NP_LOADDS NPP_WriteReady(NPP instance, NPStream* stream)
{
	struct qstream *qstr = stream->pdata;
	vfsfile_t *pipe = qstr?qstr->pipe:NULL;
	
	if (pipe && pipe->seekingisabadplan)
		return 1024*1024 - VFS_GETLEN(pipe);
	else
		return 8192;
}
int32   NP_LOADDS NPP_Write(NPP instance, NPStream* stream, int32 offset,
                            int32 len, void* buffer)
{
	int bytes = NPP_WriteReady(instance, stream);
	struct context *ctx = instance->pdata;
	struct qstream *qstr = stream->pdata;

	if (qstr && qstr->type && qstr->type->wait)
	{
		if (!ctx->wait_stream)
			ctx->wait_stream = qstr;
		if (ctx->wait_stream == qstr)
		{
			ctx->wait_offset = offset;
			ctx->wait_size = stream->end;

			InvalidateRgn(ctx->window.window, NULL, FALSE);
		}
	}

	if (!qstr || !qstr->pipe)
		return bytes;

	//we're not meant to read more bytes than we said we could read.
	if (len > bytes)
		len = bytes;

	return VFS_WRITE(qstr->pipe, buffer, len);
}
void    NP_LOADDS NPP_StreamAsFile(NPP instance, NPStream* stream,
                                   const char* fname)
{
	struct qstream *qstr = stream->pdata;

	if (!qstr)
		return;

	if (qstr->pipe)
		VFS_CLOSE(qstr->pipe);
	qstr->pipe = VFSOS_Open(fname, "rb");
}
void    NP_LOADDS NPP_Print(NPP instance, NPPrint* platformPrint)
{
	//we don't support printing.
	//paper and ink doesn't give a good frame rate.
	return;
}
int16   NP_LOADDS NPP_HandleEvent(NPP instance, void* event)
{
//	MessageBox(NULL, "NPP_HandleEvent", "npapi", 0);
	return NPERR_NO_ERROR;
}
void    NP_LOADDS NPP_URLNotify(NPP instance, const char* url,
                                NPReason reason, void* notifyData)
{
}

struct npscript_property
{
	char *name;
	qboolean onlyifactive;

	cvar_t *cvar;

	char *(*getstring)(struct context *ctx);
	void (*setstring)(struct context *ctx, const char *val);

	int (*getint)(struct context *ctx);
	void (*setint)(struct context *ctx, int val);
};

int npscript_property_isrunning_getb(struct context *ctx)
{
	if (ctx->contextrunning)
		return true;
	else
		return false;
}

char *npscript_property_startserver_gets(struct context *ctx)
{
	return ctx->qtvf.server;
}
void npscript_property_startserver_sets(struct context *ctx, const char *val)
{
	ctx->qtvf.connectiontype = QTVCT_CONNECT;
	Q_strncpyz(ctx->qtvf.server, val, sizeof(ctx->qtvf.server));
}
char *npscript_property_curserver_gets(struct context *ctx)
{
	if (!npscript_property_isrunning_getb(ctx))
		return npscript_property_startserver_gets(ctx);

	return cls.servername;
}
void npscript_property_curserver_sets(struct context *ctx, const char *val)
{
	if (!npscript_property_isrunning_getb(ctx))
	{
		npscript_property_startserver_sets(ctx, val);
		return;
	}

	Q_strncpyz(cls.servername, val, sizeof(cls.servername));
	CL_BeginServerConnect();
}

extern cvar_t skin, team, topcolor, bottomcolor, vid_fullscreen;
struct npscript_property npscript_properties[] =
{
	{"isrunning",	false,	NULL,	NULL, NULL, npscript_property_isrunning_getb},
	{"startserver",	false,	NULL,	npscript_property_startserver_gets, npscript_property_startserver_sets},
	{"server",		false,	NULL,	npscript_property_curserver_gets, npscript_property_curserver_sets},
	{"playername",	true,	&name},
	{NULL,			true,	&skin},
	{NULL,			true,	&team},
	{NULL,			true,	&topcolor},
	{NULL,			true,	&bottomcolor},
	{NULL,			true,	&password},
//	{NULL,			true,	&spectator},
	{"fullscreen",	true,	&vid_fullscreen},
	{NULL}
};

struct npscript
{
	NPObject obj;

	struct context *ctx;

	struct npscript_property *props;
};

NPObject *npscript_allocate(NPP npp, NPClass *aClass)
{
	struct npscript_property *prop;
	struct npscript *obj;
	obj = malloc(sizeof(*obj));
	obj->obj._class = aClass;
	obj->obj.referenceCount = 1;
	obj->ctx = npp->pdata;

	obj->props = npscript_properties;

	for (prop = obj->props; prop->name||prop->cvar; prop++)
	{
		if(!prop->name)
			prop->name = prop->cvar->name;
	}
	return (NPObject*)obj;
}
void npscript_deallocate(NPObject *npobj)
{
	free(npobj);
}
void npscript_invalidate(NPObject *npobj)
{
	struct npscript *obj = (struct npscript *)npobj;
	obj->ctx = NULL;
}
bool npscript_hasMethod(NPObject *npobj, NPIdentifier name)
{
	NPUTF8 *mname;
	mname = browserfuncs->utf8fromidentifier(name);
	return false;
}
bool npscript_invoke(NPObject *npobj, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
	return false;
}
bool npscript_invokeDefault(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
	return false;
}
bool npscript_hasProperty(NPObject *npobj, NPIdentifier name)
{
	struct npscript *obj = (struct npscript *)npobj;
	struct npscript_property *prop;
	NPUTF8 *pname;
	pname = browserfuncs->utf8fromidentifier(name);

	for (prop = obj->props; prop->name; prop++)
	{
		if (!strcmp(prop->name, pname))
			return true;
	}
	return false;
}
bool npscript_getProperty(NPObject *npobj, NPIdentifier name, NPVariant *result)
{
	struct npscript *obj = (struct npscript *)npobj;
	struct context *ctx = obj->ctx;
	NPUTF8 *pname;
	char *res, *ns;
	int len;
	struct npscript_property *prop;
	pname = browserfuncs->utf8fromidentifier(name);

	for (prop = obj->props; prop->name; prop++)
	{
		if (!strcmp(prop->name, pname))
		{
			if (prop->onlyifactive)
			{
				if (!ctx->contextrunning)
					return false;
			}
			if (prop->getstring)
			{
				//FIXME: Are we meant to malloc a new string buffer here?
				res = prop->getstring(ctx);
				len = strlen(res);
				ns = browserfuncs->memalloc(len);
				if (!ns)
					return false;
				memcpy(ns, res, len);
				STRINGZ_TO_NPVARIANT(ns, *result);
				return true;
			}
			else if (prop->getint)
			{
				INT32_TO_NPVARIANT(prop->getint(ctx), *result);
				return true;
			}
			else if (prop->cvar)
			{
				//FIXME: Are we meant to malloc a new string buffer here?
				res = prop->cvar->string;
				len = strlen(res);
				ns = browserfuncs->memalloc(len);
				if (!ns)
					return false;
				memcpy(ns, res, len);
				STRINGZ_TO_NPVARIANT(ns, *result);
				return true;
			}
			return false;
		}
	}
	return false;
}
bool npscript_setProperty(NPObject *npobj, NPIdentifier name, const NPVariant *value)
{
	struct npscript *obj = (struct npscript *)npobj;
	struct context *ctx = obj->ctx;
	NPUTF8 *pname;
	NPString str;
	struct npscript_property *prop;
	pname = browserfuncs->utf8fromidentifier(name);

	for (prop = obj->props; prop->name; prop++)
	{
		if (!strcmp(prop->name, pname))
		{
			if (prop->onlyifactive)
			{
				if (!ctx->contextrunning)
					return false;
			}

			if (NPVARIANT_IS_STRING(*value))
			{
				char *t = NULL;

				str = NPVARIANT_TO_STRING(*value);
				if (str.utf8characters[str.utf8length] != 0)
				{
					t = malloc(str.utf8length+1);
					memcpy(t, str.utf8characters, str.utf8length);
					t[str.utf8length] = 0;
					str.utf8characters = t;
				}
				if (prop->setstring)
				{
					prop->setstring(ctx, str.utf8characters);
					if (t)
						free(t);
					return true;
				}
				if (prop->setint)
				{
					prop->setint(ctx, atoi(str.utf8characters));
					if (t)
						free(t);
					return true;
				}
				if (t)
					free(t);
			}
			if (NPVARIANT_IS_INT32(*value))
			{
				if (prop->setint)
				{
					prop->setint(ctx, NPVARIANT_TO_INT32(*value));
					return true;
				}
			}
			if (NPVARIANT_IS_DOUBLE(*value))
			{
				if (prop->setint)
				{
					prop->setint(ctx, NPVARIANT_TO_DOUBLE(*value));
					return true;
				}
			}

			if (prop->cvar)
			{
				if (NPVARIANT_IS_STRING(*value))
				{
					str = NPVARIANT_TO_STRING(*value);
					Cvar_Set(prop->cvar, str.utf8characters);
					return true;
				}
				if (NPVARIANT_IS_INT32(*value))
				{
					Cvar_SetValue(prop->cvar, NPVARIANT_TO_INT32(*value));
					return true;
				}
				if (NPVARIANT_IS_DOUBLE(*value))
				{
					Cvar_SetValue(prop->cvar, NPVARIANT_TO_DOUBLE(*value));
					return true;
				}
				if (NPVARIANT_IS_BOOLEAN(*value))
				{
					Cvar_SetValue(prop->cvar, NPVARIANT_TO_BOOLEAN(*value));
					return true;
				}
			}
			//sorry, no can do
			return false;
		}
	}
	//not known
	return false;
}
bool npscript_removeProperty(NPObject *npobj, NPIdentifier name)
{
	return false;
}
bool npscript_enumerate(NPObject *npobj, NPIdentifier **value, uint32_t *count)
{
	return false;
}
bool npscript_construct(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
	return false;
}

NPClass npscript_class =
{
	NP_CLASS_STRUCT_VERSION,

    npscript_allocate,
    npscript_deallocate,
    npscript_invalidate,
    npscript_hasMethod,
    npscript_invoke,
    npscript_invokeDefault,
    npscript_hasProperty,
    npscript_getProperty,
    npscript_setProperty,
    npscript_removeProperty,
    npscript_enumerate,
    npscript_construct
};

NPError NP_LOADDS NPP_GetValue(NPP instance, NPPVariable variable, void *value)
{
	switch(variable)
	{
	case NPPVpluginScriptableNPObject:
		*(void**)value = browserfuncs->createobject(instance, &npscript_class);
		return NPERR_NO_ERROR;
	default:
		return NPERR_INVALID_PARAM;
	}

	return NPERR_NO_ERROR;
}
NPError NP_LOADDS NPP_SetValue(NPP instance, NPNVariable variable, void *value)
{
	switch(variable)
	{
	default:
		return NPERR_INVALID_PARAM;
	}
	return NPERR_NO_ERROR;
}

NPError OSCALL NP_Initialize(NPNetscapeFuncs* pFuncs)
{
	browserfuncs = pFuncs;
	return NPERR_NO_ERROR;
}

NPError OSCALL NP_Shutdown(void)
{
	if (contextlist)
	{	//the browser isn't meant to call this when there's still instances left...
		return NPERR_GENERIC_ERROR;
	}

	return NPERR_NO_ERROR;
}

NPError OSCALL NP_GetValue(void *instance, NPPVariable variable, void *value)
{
	if (value == NULL)
		return NPERR_INVALID_PARAM;

	switch(variable)
	{
	case NPPVpluginNameString:
		*(char**)value = "QTV Viewer";
		break;
	case NPPVpluginDescriptionString:
		*(char**)value = "QTV Viewer";
		break;
	default:
		return NPERR_INVALID_PARAM;
	}
	return NPERR_NO_ERROR;
}

NPError OSCALL NP_GetEntryPoints (NPPluginFuncs* pFuncs)
{
	if (pFuncs->size < sizeof(NPPluginFuncs))
		return NPERR_INVALID_FUNCTABLE_ERROR;
	pFuncs->size = sizeof(NPPluginFuncs);

    pFuncs->version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;

    pFuncs->newp = NPP_New;
    pFuncs->destroy = NPP_Destroy;
    pFuncs->setwindow = NPP_SetWindow;
    pFuncs->newstream = NPP_NewStream;
    pFuncs->destroystream = NPP_DestroyStream;
    pFuncs->asfile = NPP_StreamAsFile;
    pFuncs->writeready = NPP_WriteReady;
    pFuncs->write = NPP_Write;
    pFuncs->print = NPP_Print;
    pFuncs->event = NPP_HandleEvent;
    pFuncs->urlnotify = NPP_URLNotify;
    pFuncs->javaClass = NULL;
    pFuncs->getvalue = NPP_GetValue;
    pFuncs->setvalue = NPP_SetValue;

	return NPERR_NO_ERROR;
}

char *NP_GetMIMEDescription(void)
{
	return "test/x-qtv:qtv:QTV Stream Description";
}
