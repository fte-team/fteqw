#include <jni.h>
#include <errno.h>

#include <android/log.h>

#include "quakedef.h"
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <dirent.h>

#ifndef isDedicated
#ifdef SERVERONLY
qboolean isDedicated = true;
#else
qboolean isDedicated = false;
#endif
#endif
void *sys_window; /*public so the renderer can attach to the correct place*/
static qboolean sys_running = false;


#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, DISTRIBUTION"Droid", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, DISTRIBUTION"Droid", __VA_ARGS__))

static void *sys_memheap;
static unsigned int sys_lastframe;
JNIEXPORT void JNICALL Java_com_fteqw_FTEDroidEngine_frame(JNIEnv *env, jobject obj)
{
	#ifdef SERVERONLY
	SV_Frame();
	#else
	unsigned int now = Sys_Milliseconds();
	double tdelta = (now - sys_lastframe) * 0.001;
	Host_Frame(tdelta);
	sys_lastframe = now;
	#endif
}

JNIEXPORT void JNICALL Java_com_fteqw_FTEDroidEngine_init(JNIEnv *env, jobject obj,
                 jint width, jint height)
{
	vid.pixelwidth = width;
	vid.pixelheight = height;
	if (!sys_running)
	{
		quakeparms_t parms;
		parms.basedir = "/sdcard/fte";
		parms.argc = 0;
		parms.argv = NULL;
		parms.memsize = sys_memheap = 8*1024*1024;
		parms.membase = malloc(parms.memsize);
		if (!parms.membase)
		{
			Sys_Printf("Unable to alloc heap\n");
			return;
		}

		Sys_Printf("Starting up\n");

		COM_InitArgv(parms.argc, parms.argv);
		TL_InitLanguages();
		#ifdef SERVERONLY
			SV_Init(&parms);
		#else
			Host_Init(&parms);
		#endif
		sys_running = true;
		sys_lastframe = Sys_Milliseconds();
	}
}
JNIEXPORT void JNICALL Java_com_fteqw_FTEDroidEngine_keypress(JNIEnv *env, jobject obj,
                 jint down, jint keycode, jint unicode)
{
	Key_Event(0, keycode, unicode, down);
}

int mousecursor_x, mousecursor_y;
float mouse_x, mouse_y;
static float omouse_x, omouse_y;
JNIEXPORT void JNICALL Java_com_fteqw_FTEDroidEngine_motion(JNIEnv *env, jobject obj,
                 jint act, jfloat x, jfloat y)
{
	static float totalmoved;
	static qboolean down;
	float dx, dy;

	dx = x - omouse_x;
	dy = y - omouse_y;
	omouse_x = x;
	omouse_y = y;
	mousecursor_x = x;
	mousecursor_y = y;

	if (down)
	{	
		mouse_x += dx;
		mouse_y += dy;
		totalmoved += fabs(dx) + fabs(dy);
	}

	switch(act)
	{
	case 0: /*move*/
		break;
	case 1: /*down*/
		totalmoved = 0;
		down = true;
		break;
	case 2: /*up*/
		down = false;
		/*if it didn't move far, treat it as a regular click, if it did move a little then sorry if you just wanted a small turn!*/
		if (totalmoved < 3)
		{
			Key_Event(0, K_MOUSE1, 0, 1);
			Key_Event(0, K_MOUSE1, 0, 0);
		}
		break;
	}
}
JNIEXPORT void JNICALL Java_com_fteqw_FTEDroidEngine_accelerometer(JNIEnv *env, jobject obj,
                 jfloat x, jfloat y, jfloat z)
{
//	Con_Printf("Accelerometer: %f %f %f\n", x, y, z);
}

static int secbase;
double Sys_DoubleTime(void)
{
        struct timeval tp;
        struct timezone tzp;

        gettimeofday(&tp, &tzp);

        if (!secbase)
        {
                secbase = tp.tv_sec;
                return tp.tv_usec/1000000.0;
        }

        return (tp.tv_sec - secbase) + tp.tv_usec/1000000.0;
}
unsigned int Sys_Milliseconds(void)
{
        struct timeval tp;
        struct timezone tzp;

        gettimeofday(&tp, &tzp);

        if (!secbase)
        {
                secbase = tp.tv_sec;
                return tp.tv_usec/1000;
        }

        return (tp.tv_sec - secbase)*1000 + tp.tv_usec/1000;
}

void Sys_Shutdown(void)
{
	free(sys_memheap);
}
void Sys_Quit(void)
{
#ifndef SERVERONLY
	Host_Shutdown ();
#else
	SV_Shutdown();
#endif

	exit (0);
}
void Sys_Error (const char *error, ...)
{
	va_list         argptr;
	char             string[1024];

	va_start (argptr, error);
	vsnprintf (string,sizeof(string)-1, error,argptr);
	va_end (argptr);

	LOGW("%s", string);

	exit(1);
}
void Sys_Printf (char *fmt, ...)
{
	va_list         argptr;
	char             string[1024];

	va_start (argptr, fmt);
	vsnprintf (string,sizeof(string)-1, fmt,argptr);
	va_end (argptr);

	LOGI("%s", string);
}
void Sys_Warn (char *fmt, ...)
{
	va_list         argptr;
	char             string[1024];

	va_start (argptr, fmt);
	vsnprintf (string,sizeof(string)-1, fmt,argptr);
	va_end (argptr);

	LOGW("%s", string);
}

void Sys_CloseLibrary(dllhandle_t *lib)
{
	dlclose(lib);
}
dllhandle_t *Sys_LoadLibrary(const char *name, dllfunction_t *funcs)
{
	dllhandle_t *h;
	h = dlopen(name, RTLD_LAZY);
	return h;
}
void *Sys_GetAddressForName(dllhandle_t *module, const char *exportname)
{
	return dlsym(module, exportname);
}
void *Sys_GetGameAPI (void *parms)
{
	return NULL;
}
void Sys_UnloadGame(void)
{
}
char *Sys_ConsoleInput (void)
{
	return NULL;
}
void Sys_mkdir (char *path)    //not all pre-unix systems have directories (including dos 1)
{
	mkdir(path, 0777);
}
qboolean Sys_remove (char *path)
{
	return !unlink(path);
}
void Sys_Init(void)
{
}

qboolean Sys_GetDesktopParameters(int *width, int *height, int *bpp, int *refreshrate)
{
	*width = 320;
	*height = 240;
	*bpp = 16;
	*refreshrate = 60;
	return false;
}
qboolean Sys_RandomBytes(qbyte *string, int len)
{
	qboolean res = false;
	int fd = open("/dev/urandom", 0);
	if (fd >= 0)
	{
		res = (read(fd, string, len) == len);
		close(fd);
	}

	return res;
}

void Sys_ServerActivity(void)
{
	/*FIXME: flash window*/
}

qboolean Sys_InitTerminal(void)
{
	/*switching to dedicated mode, show text window*/
	return false;
}
void Sys_CloseTerminal(void)
{
}

char *Sys_GetClipboard(void)
{
	return NULL;
}
void Sys_CloseClipboard(char *buf)
{
}
void Sys_SaveClipboard(char *text)
{
}

int Sys_EnumerateFiles (const char *gpath, const char *match, int (*func)(const char *, int, void *), void *parm)
{
        DIR *dir;
        char apath[MAX_OSPATH];
        char file[MAX_OSPATH];
        char truepath[MAX_OSPATH];
        char *s;
        struct dirent *ent;
        struct stat st;

//printf("path = %s\n", gpath);
//printf("match = %s\n", match);

        if (!gpath)
                gpath = "";
        *apath = '\0';

        Q_strncpyz(apath, match, sizeof(apath));
        for (s = apath+strlen(apath)-1; s >= apath; s--)
        {
                if (*s == '/')
                {
                        s[1] = '\0';
                        match += s - apath+1;
                        break;
                }
        }
        if (s < apath)  //didn't find a '/' 
                *apath = '\0'; 
 
        Q_snprintfz(truepath, sizeof(truepath), "%s/%s", gpath, apath); 
 
 
//printf("truepath = %s\n", truepath); 
//printf("gamepath = %s\n", gpath); 
//printf("apppath = %s\n", apath); 
//printf("match = %s\n", match); 
        dir = opendir(truepath); 
        if (!dir) 
        { 
                Con_DPrintf("Failed to open dir %s\n", truepath); 
                return true; 
        } 
        do 
        { 
                ent = readdir(dir); 
                if (!ent) 
                        break; 
                if (*ent->d_name != '.') 
                { 
                        if (wildcmp(match, ent->d_name)) 
                        { 
                                Q_snprintfz(file, sizeof(file), "%s/%s", truepath, ent->d_name); 
 
                                if (stat(file, &st) == 0) 
                                { 
                                        Q_snprintfz(file, sizeof(file), "%s%s%s", apath, ent->d_name, S_ISDIR(st.st_mode)?"/":""); 
 
                                        if (!func(file, st.st_size, parm)) 
                                        { 
                                                closedir(dir); 
                                                return false; 
                                        } 
                                } 
                                else 
                                        printf("Stat failed for \"%s\"\n", file); 
                        } 
                } 
        } while(1); 
        closedir(dir); 
 
        return true; 
}

/*
int Sys_EnumerateFiles (const char *gpath, const char *match, int (*func)(const char *, int, void *), void *parm)
{
	qboolean go = true;
	const char *f;

	struct AAssetDir *ad;
	ad = AAssetManager_openDir(assetmgr, gpath);

	while(go && (f = AAssetDir_getNextFileName(ad)))
	{
		if (wildcmp(match, f))
		{
Sys_Printf("Found %s\n", f);
			go = func(f, 0, parm);
		}
	}

	AAssetDir_close(ad);
	return 0;
}
typedef struct
{
	vfsfile_t funcs;
	AAsset *handle;
} assetfile_t;
static int AF_ReadBytes(vfsfile_t *h, void *buf, int len)
{
	assetfile_t *f = (assetfile_t*)h;
	return AAsset_read(f->handle, buf, len);
}
static qboolean AF_Seek(vfsfile_t *h, unsigned long offs)
{
	assetfile_t *f = (assetfile_t*)h;
	AAsset_seek(f->handle, offs, SEEK_SET);
	return true;
}
static unsigned long AF_Tell(vfsfile_t *h)
{
	assetfile_t *f = (assetfile_t*)h;
	return AAsset_seek(f->handle, 0, SEEK_CUR);
}
static unsigned long AF_GetSize(vfsfile_t *h)
{
	assetfile_t *f = (assetfile_t*)h;
	return AAsset_getLength(f->handle);
}

static void AF_Close(vfsfile_t *h)
{
	assetfile_t *f = (assetfile_t*)h;
	AAsset_close(f->handle);
	Z_Free(f);
}
static void AF_Flush(vfsfile_t *h)
{
}
vfsfile_t *Sys_OpenAsset(char *fname)
{
	assetfile_t *file;
	AAsset *a;
	a = AAssetManager_open(assetmgr, fname, AASSET_MODE_UNKNOWN);
	if (!a)
	{
		Sys_Printf("Unable to open asset %s\n", fname);
		return NULL;
	}
	Sys_Printf("opened asset %s\n", fname);

        file = Z_Malloc(sizeof(assetfile_t));
        file->funcs.ReadBytes = AF_ReadBytes;
        file->funcs.WriteBytes = NULL;
        file->funcs.Seek = AF_Seek;
        file->funcs.Tell = AF_Tell;
        file->funcs.GetLen = AF_GetSize;
        file->funcs.Close = AF_Close;
        file->funcs.Flush = AF_Flush;
        file->handle = a;

        return (vfsfile_t*)file;
}
*/
