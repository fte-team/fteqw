#include "quakedef.h"

#ifdef MULTITHREAD
#include <SDL_thread.h>
#endif

#include <SDL_loadso.h>

#include <emscripten/emscripten.h>
#include "ftejslib.h"

#ifndef isDedicated
qboolean isDedicated;
#endif

quakeparms_t	parms;

void Sys_Error (const char *error, ...)
{
	va_list argptr;
	char string[1024];

	va_start (argptr,error);
	vsnprintf (string, sizeof (string), error, argptr);
	va_end (argptr);
	COM_WorkerAbort(string);
	Sys_Printf("Error: %s\n", string);

	Con_Print ("Quake Error: ");
	Con_Print (string);
	Con_Print ("\n");

	Host_Shutdown ();
	emscriptenfte_alert(string);
	emscriptenfte_abortmainloop("Sys_Error", true);
	exit (1);
}

void Sys_RecentServer(char *command, char *target, char *title, char *desc)
{
}

qboolean Sys_RandomBytes(qbyte *string, int len)
{
	return false;
}

//print into stdout
void Sys_Printf (char *fmt, ...)
{
	va_list		argptr;	
	char buf[1024];
		
	va_start (argptr,fmt);
	vsnprintf (buf, sizeof(buf), fmt, argptr);
	emscriptenfte_print(buf);
	va_end (argptr);
}

#if 1
//use Performance.now() instead of Date.now() - its likely to both provide higher precision and no NTP/etc issues.
double Sys_DoubleTime (void)
{
	double t = emscriptenfte_uptime_ms()/1000;	//we need it as seconds...
	static double old = -99999999;
	if (t < old)
		t = old;	//don't let t step backwards, ever. this shouldn't happen, but some CPUs don't keep their high-precision timers synced properly.
	return old=t;
}
unsigned int Sys_Milliseconds(void)
{
	return Sys_DoubleTime() * (uint64_t)1000;
}
#else
unsigned int Sys_Milliseconds(void)
{
	static int first = true;
	static unsigned long oldtime = 0, curtime = 0;
	unsigned long newtime;

	newtime = emscriptenfte_ticks_ms();	//return Date.now()

	if (first)
	{
		first = false;
		oldtime = newtime;
	}
	if (newtime < oldtime)
		Con_Printf("Sys_Milliseconds stepped backwards!\n");
	else
		curtime += newtime - oldtime;
	oldtime = newtime;
	return curtime;
}

//return the current time, in the form of a double
double Sys_DoubleTime (void)
{
	return Sys_Milliseconds() / 1000.0;
}
#endif

//create a directory
void Sys_mkdir (const char *path)
{
}
qboolean Sys_rmdir (const char *path)
{
	return true;
}

//unlink a file
qboolean Sys_remove (const char *path)
{
	emscriptenfte_buf_delete(path);
	return true;
}

qboolean Sys_Rename (const char *oldfname, const char *newfname)
{
	return emscriptenfte_buf_rename(oldfname, newfname);
}
qboolean Sys_GetFreeDiskSpace(const char *path, quint64_t *freespace)
{	//not implemented. we could try querying local storage quotas, but our filesystem is otherwise purely ram so doesn't have much of a limit in 64bit browsers. hurrah for swap space.
	return false;
}

//someone used the 'quit' command
#include "glquake.h"
void Sys_Quit (void)
{
	if (host_initialized)
	{
		qglClearColor(0,0,0,1);
		qglClear(GL_COLOR_BUFFER_BIT);
		Draw_FunString (0, 0, "Reload the page to restart");

		Host_Shutdown();
	}

	exit (0);
}


struct enumctx_s
{
	char name[MAX_OSPATH];
	const char *gpath;
	size_t gpathlen;
	const char *match;
	int (*callback)(const char *, qofs_t, time_t mtime, void *, searchpathfuncs_t *);
	void *ctx;
	searchpathfuncs_t *spath;
	int ret;
};
static void Sys_EnumeratedFile(void *vctx, size_t fsize)
{	//called for each enumerated file.
	//we don't need the whole EnumerateFiles2 thing as our filesystem is flat, so */* isn't an issue for us (we don't expect a lot of different 'files' if only because they're a pain to download).
	struct enumctx_s *ctx = vctx;
	if (!ctx->ret)
		return;	//we're meant to stop when if it returns false...
	if (!strncmp(ctx->name, ctx->gpath, ctx->gpathlen))		//ignore any gamedir prefix
		if (wildcmp(ctx->match, ctx->name+ctx->gpathlen))	//match it within the searched gamedir...
			ctx->ret = ctx->callback(ctx->name+ctx->gpathlen, fsize, 0, ctx->ctx, ctx->spath);	//call the callback
}
int Sys_EnumerateFiles (const char *gpath, const char *match, int (*func)(const char *, qofs_t, time_t mtime, void *, searchpathfuncs_t *), void *parm, searchpathfuncs_t *spath)
{
	struct enumctx_s ctx;
	char tmp[MAX_OSPATH];
	if (!gpath)
		gpath = "";
	ctx.gpathlen = strlen(gpath);
	if (ctx.gpathlen && gpath[ctx.gpathlen-1] != '/')
	{	//make sure gpath is /-terminated.
		if (ctx.gpathlen >= sizeof(tmp)-1)
			return false;	//just no...
		Q_strncpyz(tmp, gpath, sizeof(tmp));
		gpath = tmp;
		tmp[ctx.gpathlen++] = '/';
	}
	ctx.gpath = gpath;
	ctx.match = match;
	ctx.callback = func;
	ctx.ctx = parm;
	ctx.spath = spath;
	ctx.ret = true;
	emscritenfte_buf_enumerate(Sys_EnumeratedFile, &ctx, sizeof(ctx.name));
	return ctx.ret;
}

//blink window if possible (it's not)
void Sys_ServerActivity(void)
{
}

void Sys_CloseLibrary(dllhandle_t *lib)
{
}
dllhandle_t *Sys_LoadLibrary(const char *name, dllfunction_t *funcs)
{
	return NULL;
}
void *Sys_GetAddressForName(dllhandle_t *module, const char *exportname)
{
	return NULL;
}



void Sys_BrowserRedirect_f(void)
{
	emscriptenfte_window_location(Cmd_Argv(1));
}

char *Sys_URIScheme_NeedsRegistering(void)
{	//just disables the prompts that we can't honour anyway.
	return NULL;
}

void Sys_Init(void)
{
	extern cvar_t vid_width, vid_height, vid_fullscreen;
	//vid_fullscreen takes effect only on mouse clicks, any suggestion to do a vid_restart is pointless.
	vid_fullscreen.flags &= ~CVAR_VIDEOLATCH;
	//these are not really supported. so silence any spam that suggests we do something about something not even supported.
	vid_width.flags &= ~CVAR_VIDEOLATCH;
	vid_height.flags &= ~CVAR_VIDEOLATCH;

	Cmd_AddCommand("sys_browserredirect", Sys_BrowserRedirect_f);
}
void Sys_Shutdown(void)
{
}



int VARGS Sys_DebugLog(char *file, char *fmt, ...)
{
	return 0;
};



qboolean Sys_InitTerminal(void)
{
	return true;
}
char *Sys_ConsoleInput(void)
{
	return NULL;
}
void Sys_CloseTerminal (void)
{
}

int Sys_MainLoop(double newtime)
{
	extern cvar_t vid_vsync;
	static double oldtime;
	double time;

	if (newtime)
		newtime /= 1000;	//use RAF's timing for slightly greater precision.
	else
		newtime = Sys_DoubleTime ();	//otherwise fall back on internally consistent timing...
	if (newtime < oldtime)
		newtime = oldtime;	//don't let ourselves go backwards...
	if (!oldtime)
		oldtime = newtime;
	time = newtime - oldtime;
	if (!host_initialized)
	{
		Sys_Printf ("Host_Init\n");
		Host_Init (&parms);
		return 1;
	}

	oldtime = newtime;
	Host_Frame (time);

	return vid_vsync.ival;
}

int QDECL main(int argc, char **argv)
{
	memset(&parms, 0, sizeof(parms));


	parms.basedir = "";

	parms.argc = argc;
	parms.argv = (const char**)argv;
#ifdef CONFIG_MANIFEST_TEXT
	parms.manifest = CONFIG_MANIFEST_TEXT;
#endif

	COM_InitArgv (parms.argc, parms.argv);

	TL_InitLanguages("");

	emscriptenfte_setupmainloop(Sys_MainLoop);
	return 0;
}

qboolean Sys_GetDesktopParameters(int *width, int *height, int *bpp, int *refreshrate)
{
	return false;
}

#ifdef WEBCLIENT
qboolean Sys_RunInstaller(void)
{       //not implemented
	return false;
}
#endif

#define SYS_CLIPBOARD_SIZE  256
static char *clipboard_buffer;
void Sys_Clipboard_PasteText(clipboardtype_t cbt, void (*callback)(void *cb, const char *utf8), void *ctx)
{
	callback(ctx, clipboard_buffer);
}
void Sys_SaveClipboard(clipboardtype_t cbt, const char *text)
{
	free(clipboard_buffer);
	clipboard_buffer = strdup(text);
}

#ifdef MULTITHREAD
/* Thread creation calls */
void *Sys_CreateThread(char *name, int (*func)(void *), void *args, int priority, int stacksize)
{
	// SDL threads do not support setting thread stack size
	return (void *)SDL_CreateThread(func, args);
}

void Sys_WaitOnThread(void *thread)
{
	SDL_WaitThread((SDL_Thread *)thread, NULL);
}


/* Mutex calls */
// SDL mutexes don't have try-locks for mutexes in the spec so we stick with 1-value semaphores
void *Sys_CreateMutex(void)
{
	return (void *)SDL_CreateSemaphore(1);
}

qboolean Sys_TryLockMutex(void *mutex)
{
	return !SDL_SemTryWait(mutex);
}

qboolean Sys_LockMutex(void *mutex)
{
	return !SDL_SemWait(mutex);
}

qboolean Sys_UnlockMutex(void *mutex)
{
	return !SDL_SemPost(mutex);
}

void Sys_DestroyMutex(void *mutex)
{
	SDL_DestroySemaphore(mutex);
}

/* Conditional wait calls */
typedef struct condvar_s
{
	SDL_mutex *mutex;
	SDL_cond *cond;
} condvar_t;

void *Sys_CreateConditional(void)
{
	condvar_t *condv;
	SDL_mutex *mutex;
	SDL_cond *cond;
	
	condv = (condvar_t *)malloc(sizeof(condvar_t));
	if (!condv)
		return NULL;
		
	mutex = SDL_CreateMutex();
	cond = SDL_CreateCond();
	
	if (mutex)
	{
		if (cond)
		{
			condv->cond = cond;
			condv->mutex = mutex;
		
			return (void *)condv;
		}
		else
			SDL_DestroyMutex(mutex);
	}
	
	free(condv);
	return NULL;	
}

qboolean Sys_LockConditional(void *condv)
{
	return !SDL_mutexP(((condvar_t *)condv)->mutex);
}

qboolean Sys_UnlockConditional(void *condv)
{
	return !SDL_mutexV(((condvar_t *)condv)->mutex);
}

qboolean Sys_ConditionWait(void *condv)
{
	return !SDL_CondWait(((condvar_t *)condv)->cond, ((condvar_t *)condv)->mutex);
}

qboolean Sys_ConditionSignal(void *condv)
{
	return !SDL_CondSignal(((condvar_t *)condv)->cond);
}

qboolean Sys_ConditionBroadcast(void *condv)
{
	return !SDL_CondBroadcast(((condvar_t *)condv)->cond);
}

void Sys_DestroyConditional(void *condv)
{
	condvar_t *cv = (condvar_t *)condv;
	
	SDL_DestroyCond(cv->cond);
	SDL_DestroyMutex(cv->mutex);
	free(cv);
}
#endif

void Sys_Sleep (double seconds)
{
	//SDL_Delay(seconds * 1000);
}

