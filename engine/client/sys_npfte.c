#include "quakedef.h"
#include "winquake.h"
#define bool int	//we ain't c++ (grr microsoft stdbool.h gief!)

#ifdef _WIN32
#ifndef _WINDOWS
#define _WINDOWS	//stupid GCC
#endif
#else
#define XP_UNIX
#define MOZ_X11
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#endif

#include "../libs/npapi/npupp.h"
#include "sys_plugfte.h"

/*work around absolute crapness in the npapi headers*/
#define Q_STRINGZ_TO_NPVARIANT(_val, _v)                                        \
NP_BEGIN_MACRO                                                                \
	NPString str = { _val, strlen(_val) };                                    \
    (_v).type = NPVariantType_String;                                         \
    (_v).value.stringValue = str;                                             \
NP_END_MACRO
#undef STRINGZ_TO_NPVARIANT
#define STRINGZ_TO_NPVARIANT Q_STRINGZ_TO_NPVARIANT

#define Q_STRINGN_TO_NPVARIANT(_val, _len, _v)                                        \
NP_BEGIN_MACRO                                                                \
	NPString str = { _val, _len };                                    \
    (_v).type = NPVariantType_String;                                         \
    (_v).value.stringValue = str;                                             \
NP_END_MACRO
#undef STRINGN_TO_NPVARIANT
#define STRINGN_TO_NPVARIANT Q_STRINGN_TO_NPVARIANT

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
#ifndef GWLP_WNDPROC
#define GWLP_WNDPROC GWL_WNDPROC
#endif
#ifndef GWLP_USERDATA
#define GWLP_USERDATA GWL_USERDATA
#endif
#endif



qboolean NPFTE_BeginDownload(void *ctx, struct pipetype *ftype, char *url)
{
	return NPERR_NO_ERROR==browserfuncs->geturlnotify(ctx, url, NULL, ftype);
}

void NPFTE_StatusChanged(void *sysctx)
{	//potentially called from another thread
	NPP instance = sysctx;
	struct contextpublic *pub = instance->pdata;
#ifdef _WIN32
	InvalidateRgn(pub->oldwnd, NULL, FALSE);
#endif
}

#ifdef _WIN32
void DrawWndBack(struct context *ctx, HWND hWnd, HDC hdc, PAINTSTRUCT *p)
{
	int width, height;
	HBITMAP bmp = Plug_GetSplashBack(ctx, hdc, &width, &height);
	if (bmp)
	{
		HDC memDC;
		RECT irect;

		memDC = CreateCompatibleDC(hdc);
		SelectObject(memDC, bmp);
		GetClientRect(hWnd, &irect);
		StretchBlt(hdc, irect.left, irect.top, irect.right-irect.left,irect.bottom-irect.top, memDC, 0, 0, width, height, SRCCOPY);
		SelectObject(memDC, NULL);
		DeleteDC(memDC);
		Plug_ReleaseSplashBack(ctx, bmp);
	}
	else
		PatBlt(hdc, p->rcPaint.left, p->rcPaint.top, p->rcPaint.right-p->rcPaint.left,p->rcPaint.bottom-p->rcPaint.top,PATCOPY);
}

LRESULT CALLBACK MyPluginWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	struct context *ctx;
	struct contextpublic *pub;
	ctx = (struct context *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (!ctx)
		return DefWindowProc(hWnd, msg, wParam, lParam);
	pub = (struct contextpublic*)ctx;

	switch(msg)
	{
	case WM_USER:
		/*if the plugin is somewhere in video code, the plugin might depend upon us being able to respond to window messages*/
/*		while(ctx->queuedstreams)
		{
			struct qstream *strm;
			strm = ctx->queuedstreams;
			ctx->queuedstreams = strm->next;

			if (!browserfuncs->geturlnotify(ctx->nppinstance, strm->url, NULL, strm->type))
			{
				VS_DebugLocation(__FILE__, __LINE__, "Starting Download %s", strm->url);
				if (strm->type->wait == WAIT_YES)
					ctx->waitingfordatafiles++;
			}
			free(strm);
		}
*/
		return TRUE;

	case WM_ERASEBKGND:
		return FALSE;
	case WM_PAINT:
		if (*pub->statusmessage)
		{
			HDC hdc;
			PAINTSTRUCT paint;
			unsigned int progress;
			unsigned int total;

			progress = pub->dldone;
			total = pub->dlsize;

			hdc = BeginPaint(hWnd, &paint);
			DrawWndBack(ctx, hWnd, hdc, &paint);
			SetBkMode(hdc, TRANSPARENT);
			TextOutA(hdc, 0, 0, pub->statusmessage, strlen(pub->statusmessage));
			EndPaint(hWnd, &paint);
			return TRUE;
		}
		else if (pub->downloading)
		{
			HDC hdc;
			PAINTSTRUCT paint;
			char s[32];
			unsigned int progress;
			unsigned int total;

			progress = pub->dldone;
			total = pub->dlsize;

			hdc = BeginPaint(hWnd, &paint);
			DrawWndBack(ctx, hWnd, hdc, &paint);
			SetBkMode(hdc, TRANSPARENT);
			TextOutA(hdc, 0, 0, "Downloading Data, please wait", 16);
			if (!progress && !total)
				sprintf(s, "connecting");
			else if (total)
				sprintf(s, "%i bytes (%i%%)", progress, (int)((100.0f*progress)/total));
			else
				sprintf(s, "%i bytes", progress);
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
			if (!pub->running)
			{
				s = "Click to activate";
				TextOutA(hdc, 0, 0, s, strlen(s));

				if (pub->availver)
				{
					s = "Your plugin may be incompatible";
					TextOutA(hdc, 0, 32, s, strlen(s));
					s = "A newer version is available. Your version is dated " __DATE__ ".";
					TextOutA(hdc, 0, 48, s, strlen(s));
				}
			}
			EndPaint(hWnd, &paint);
			return TRUE;
		}
		break;

	case WM_LBUTTONDOWN:
		SetActiveWindow(hWnd);
		if (!Plug_StartContext(ctx))
			Plug_StopContext(NULL, false);
		break;
	default:
		break;
	}

	//I would call the previous wndproc... but that crashes firefox
	return DefWindowProc(hWnd, msg, wParam, lParam);
}
#else
static struct
{
	void *x11;
	void *xt;

	//X11 funcs
       int (*XSelectInput)(Display *display, Window w, long event_mask);

	//Xt funcs
	Widget (*XtWindowToWidget)(Display *display, Window window);
	void (*XtAddEventHandler)(Widget w, EventMask event_mask, Boolean nonmaskable, XtEventHandler proc, XtPointer client_data);
} x;
qboolean x_initlibs(void)
{
	static dllfunction_t xt_funcs[] =
	{
		{(void**)&x.XtWindowToWidget,	"XtWindowToWidget"},
		{(void**)&x.XtAddEventHandler,	"XtAddEventHandler"},
		{NULL, NULL}
	};
	static dllfunction_t x11_funcs[] =
	{
		{(void**)&x.XSelectInput,	"XSelectInput"},
		{NULL, NULL}
	};
	if (!x.xt)
		x.xt = Sys_LoadLibrary("libXt", xt_funcs);
	if (!x.xt)
	{
		Con_Printf("Please install the appropriate libXt for your browser's cpu archetecture\n");
		return false;
	}
	if (!x.x11)
		x.x11 = Sys_LoadLibrary("libX11", x11_funcs);
	if (!x.x11)
	{
		Con_Printf("Please install the appropriate libX11 for your browser's cpu archetecture\n");
		return false;
	}
	return true;
}
#endif

static const struct browserfuncs npfte_browserfuncs =
{
	NPFTE_BeginDownload,
	NPFTE_StatusChanged
};

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

//	browserfuncs->setvalue(instance, NPPVpluginWindowBool, (void*)FALSE);
//	browserfuncs->setvalue(instance, NPPVpluginTransparentBool, (void*)TRUE);

	ctx = Plug_CreateContext(instance, &npfte_browserfuncs);
	instance->pdata = ctx;
	if (!ctx)
	{
		return NPERR_OUT_OF_MEMORY_ERROR;
	}

	//parse out the properties
	for (i = 0; i < argc; i++)
	{
		Plug_SetString(ctx, Plug_FindProp(ctx, argn[i]), argv[i]);
	}

	return NPERR_NO_ERROR;
}
NPError NP_LOADDS NPP_Destroy(NPP instance, NPSavedData** save)
{
	struct context *ctx = instance->pdata;
	struct contextpublic *pub = (struct contextpublic *)ctx;

	if (!ctx)
		return NPERR_INVALID_INSTANCE_ERROR;

#ifdef _WIN32
	if (pub->oldwnd)
	{
		if (pub->oldproc)
			SetWindowLongPtr(pub->oldwnd, GWLP_WNDPROC, (LONG_PTR)pub->oldproc);
		SetWindowLongPtr(pub->oldwnd, GWLP_USERDATA, (LONG_PTR)NULL);
	}
#endif

	Plug_DestroyContext(ctx);
	instance->pdata = NULL;

	return NPERR_NO_ERROR;
}

#ifdef MOZ_X11
static void myxteventcallback (Widget w, XtPointer closure, XEvent *event, Boolean *cont)
{
	switch(event->type)
	{
	case MotionNotify:
//		Con_Printf("Motion Mouse event\n");
		break;
	case ButtonPress:
		if (event->xbutton.button == 1)
		{
			if (!Plug_StartContext(closure))
        	                Plug_StopContext(NULL, false);
		}
		break;
	default:
//		Con_Printf("other event (%i)\n", event->type);
		break;
	}
}
#endif

NPError NP_LOADDS NPP_SetWindow(NPP instance, NPWindow* window)
{
	struct context *ctx = instance->pdata;
	struct contextpublic *pub = (struct contextpublic*)ctx;

#ifdef _WIN32
	WNDPROC p;

	if (!ctx)
		return NPERR_INVALID_INSTANCE_ERROR;

	//if the window changed
	if (Plug_ChangeWindow(ctx, window->window, 0, 0, window->width, window->height))
	{
		//we switched window?
		if (pub->oldwnd && pub->oldproc)
		{
			SetWindowLongPtr(pub->oldwnd, GWLP_WNDPROC, (LONG_PTR)pub->oldproc);
		}
		pub->oldproc = NULL;

		p = (WNDPROC)GetWindowLongPtr(window->window, GWLP_WNDPROC);
		if (p != MyPluginWndProc)
			pub->oldproc = p;
		pub->oldwnd = window->window;

		SetWindowLongPtr(window->window, GWLP_WNDPROC, (LONG_PTR)MyPluginWndProc);
		SetWindowLongPtr(window->window, GWLP_USERDATA, (LONG_PTR)ctx);
	}

	InvalidateRgn(window->window, NULL, FALSE);
#endif
#ifdef MOZ_X11
	Window wnd = (Window)window->window;
	NPSetWindowCallbackStruct *ws = window->ws_info;
	Widget xtw = x.XtWindowToWidget (ws->display, wnd);
	x.XSelectInput(ws->display, wnd, ButtonPressMask|ButtonReleaseMask|PointerMotionMask|KeyPressMask|KeyReleaseMask|EnterWindowMask);
	x.XtAddEventHandler(xtw, ButtonPressMask|ButtonReleaseMask|PointerMotionMask, FALSE, myxteventcallback, ctx);

	if (Plug_ChangeWindow(ctx, window->window, 0, 0, window->width, window->height))
	{
	}
#endif
	return NPERR_NO_ERROR;
}

NPError NP_LOADDS NPP_NewStream(NPP instance, NPMIMEType type,
                                NPStream* stream, NPBool seekable,
                                uint16* stype)
{
	return NPERR_NO_ERROR;
/*	struct context *ctx = instance->pdata;
	struct qstream *qstr;

	stream->pdata = qstr = malloc(sizeof(*qstr) + strlen(stream->url));
	memset(qstr, 0, sizeof(*qstr));
	strcpy(qstr->url, stream->url);

	Plug_LockPlugin(ctx, true);
	qstr->next = ctx->activestreams;
	if (qstr->next)
		qstr->next->prev = qstr;
	ctx->activestreams = qstr;
	Plug_LockPlugin(ctx, false);

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

	return NPERR_NO_ERROR;*/
}
NPError NP_LOADDS NPP_DestroyStream(NPP instance, NPStream* stream,
                                    NPReason reason)
{
	return NPERR_NO_ERROR;
/*	struct context *ctx = instance->pdata;
	struct qstream *qstr = stream->pdata;

	if (!qstr)	//urm, got canceled before it finished downloading?
		return NPERR_NO_ERROR;

	if (qstr->type->wait == WAIT_YES)
	{
		ctx->waitingfordatafiles--;
	}

	if (qstr->next)
		qstr->next->prev = qstr->prev;
	if (qstr->prev)
		qstr->prev->next = qstr->next;
	else
		ctx->activestreams = qstr->next;

	if (qstr->type->wait == WAIT_NONACTIVE)
	{
		Plug_LockPlugin(ctx, true);
		qstr->type->completionfunc(ctx, qstr->pipe, qstr->url);
		Plug_LockPlugin(ctx, false);
	}
	else
	{
		qstr->next = ctx->donestreams;
		ctx->donestreams = qstr;
	}

	if (qstr && qstr->type && qstr->type->wait)
	{
		InvalidateRgn(ctx->window.window, NULL, FALSE);
	}
	return NPERR_NO_ERROR;*/
}
int32   NP_LOADDS NPP_WriteReady(NPP instance, NPStream* stream)
{
	return 8192;

/*	struct qstream *qstr = stream->pdata;
	vfsfile_t *pipe = qstr?qstr->pipe:NULL;
	
	if (pipe && pipe->seekingisabadplan)
		return 1024*1024 - VFS_GETLEN(pipe);
	else
		return 8192;*/
}
int32   NP_LOADDS NPP_Write(NPP instance, NPStream* stream, int32 offset,
                            int32 len, void* buffer)
{
	return len;
/*	int bytes = NPP_WriteReady(instance, stream);
	struct context *ctx = instance->pdata;
	struct qstream *qstr = stream->pdata;

	if (qstr && qstr->type && qstr->type->wait)
	{
		qstr->offset = offset;
		qstr->size = stream->end;
		InvalidateRgn(ctx->window.window, NULL, FALSE);
	}

	if (!qstr || !qstr->pipe)
		return bytes;

	//we're not meant to read more bytes than we said we could read.
	if (len > bytes)
		len = bytes;

	return VFS_WRITE(qstr->pipe, buffer, len);*/
}
void    NP_LOADDS NPP_StreamAsFile(NPP instance, NPStream* stream,
                                   const char* fname)
{
	return;
/*	struct qstream *qstr = stream->pdata;

	if (!qstr)
		return;

	if (qstr->pipe)
		VFS_CLOSE(qstr->pipe);
	qstr->pipe = VFSOS_Open(fname, "rb");
*/
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

struct npscript
{
	NPObject obj;

	struct context *ctx;
};

NPObject *npscript_allocate(NPP npp, NPClass *aClass)
{
	struct npscript *obj;
	obj = malloc(sizeof(*obj));
	obj->obj._class = aClass;
	obj->obj.referenceCount = 1;
	obj->ctx = npp->pdata;

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
	NPUTF8 *pname;
	pname = browserfuncs->utf8fromidentifier(name);

	if (Plug_FindProp(obj->ctx, pname))
		return true;
	return false;
}
bool npscript_getProperty(NPObject *npobj, NPIdentifier name, NPVariant *result)
{
	struct npscript *obj = (struct npscript *)npobj;
	struct context *ctx = obj->ctx;
	NPUTF8 *pname;
	int prop;
	bool success = false;
	const char *strval;
	int intval;
	float floatval;
	pname = browserfuncs->utf8fromidentifier(name);

	Plug_LockPlugin(ctx, true);
	prop = Plug_FindProp(obj->ctx, pname);
	if (prop >= 0)
	{
		if (Plug_GetString(ctx, prop, &strval))
		{
			char *ns;
			int len;
			len = strlen(strval);
			if (!len)
			{
				STRINGN_TO_NPVARIANT(NULL, 0, *result);
				success = true;
			}
			else
			{
				ns = browserfuncs->memalloc(len+1);
				if (ns)
				{
					memcpy(ns, strval, len+1);
					STRINGZ_TO_NPVARIANT(ns, *result);
					success = true;
				}
			}
			Plug_GotString(strval);
		}
		else if (Plug_GetInteger(ctx, prop, &intval))
		{
			INT32_TO_NPVARIANT(intval, *result);
			success = true;
		}
		else if (Plug_GetFloat(ctx, prop, &floatval))
		{
			DOUBLE_TO_NPVARIANT(floatval, *result);
			success = true;
		}
	}
	Plug_LockPlugin(ctx, false);
	return success;
}
bool npscript_setProperty(NPObject *npobj, NPIdentifier name, const NPVariant *value)
{
	struct npscript *obj = (struct npscript *)npobj;
	struct context *ctx = obj->ctx;
	NPUTF8 *pname;
	NPString str;
	int prop;
	bool success = false;
	pname = browserfuncs->utf8fromidentifier(name);

	Plug_LockPlugin(ctx, true);
	prop = Plug_FindProp(obj->ctx, pname);
	if (prop >= 0)
	{
		success = true;
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
			Plug_SetString(ctx, prop, str.utf8characters);
			if (t)
				free(t);
		}
		else if (NPVARIANT_IS_INT32(*value))
			Plug_SetInteger(ctx, prop, NPVARIANT_TO_INT32(*value));
		else if (NPVARIANT_IS_BOOLEAN(*value))
			Plug_SetInteger(ctx, prop, NPVARIANT_TO_BOOLEAN(*value));
		else if (NPVARIANT_IS_DOUBLE(*value))
			Plug_SetFloat(ctx, prop, NPVARIANT_TO_DOUBLE(*value));
		else
			success = false;
	}
	Plug_LockPlugin(ctx, false);
	return success;
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

NPError OSCALL NP_Shutdown(void)
{
/*	if (contextlist)
	{	//the browser isn't meant to call this when there's still instances left...
		return NPERR_GENERIC_ERROR;
	}
*/
	return NPERR_NO_ERROR;
}

NPError OSCALL NP_GetValue(void *instance, NPPVariable variable, void *value)
{
	if (value == NULL)
		return NPERR_INVALID_PARAM;

	switch(variable)
	{
	case NPPVpluginNameString:
		*(char**)value = FULLENGINENAME;
		break;
	case NPPVpluginDescriptionString:
		*(char**)value = FULLENGINENAME;
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

#ifdef _WIN32
NPError OSCALL NP_Initialize(NPNetscapeFuncs* pFuncs)
{
	Plug_GetFuncs(1);
	browserfuncs = pFuncs;
	return NPERR_NO_ERROR;
}
#else
NPError OSCALL NP_Initialize(NPNetscapeFuncs *aNPNFuncs, NPPluginFuncs *aNPPFuncs)
{
	NPError err;
	Plug_GetFuncs(1);
	browserfuncs = aNPNFuncs;
	
	err = NP_GetEntryPoints(aNPPFuncs);
	if (err != NPERR_NO_ERROR)
		return err;
	if (!x_initlibs())
		return NPERR_MODULE_LOAD_FAILED_ERROR;
	return NPERR_NO_ERROR;
}
#endif

char *NP_GetMIMEDescription(void)
{
	return
		"text/x-quaketvident:qtv:QTV Stream Description"
			";"
		"application/x-multiviewdemo:mvd:Multiview Demo"
			";"
		"application/x-fteplugin:fmf:FTE Engine Plugin"
		;
}
