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
int sys_glesversion;


#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, DISTRIBUTION"Droid", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, DISTRIBUTION"Droid", __VA_ARGS__))

static void *sys_memheap;
static unsigned int sys_lastframe;
JNIEXPORT int JNICALL Java_com_fteqw_FTEDroidEngine_frame(JNIEnv *env, jobject obj,
				jfloat ax, jfloat ay, jfloat az)
{
	static vec3_t oac;
	#ifdef SERVERONLY
	SV_Frame();
	#else
	unsigned int now = Sys_Milliseconds();
	double tdelta = (now - sys_lastframe) * 0.001;
	if (oac[0] != ax || oac[1] != ay || oac[2] != az)
	{
		CSQC_Accelerometer(ax, ay, az);
		oac[0] = ax;
		oac[1] = ay;
		oac[2] = az;
	}
	Host_Frame(tdelta);
	sys_lastframe = now;
	#endif

	if (key_dest == key_console || key_dest == key_message)
		return 1;
	return 0;
}

JNIEXPORT void JNICALL Java_com_fteqw_FTEDroidEngine_init(JNIEnv *env, jobject obj,
                 jint width, jint height, jint glesversion, jstring japkpath, jstring jusrpath)
{
	char *tmp;
	vid.pixelwidth = width;
	vid.pixelheight = height;
	sys_glesversion = glesversion;
	if (sys_running)
		Cmd_ExecuteString("vid_restart\n", RESTRICT_LOCAL);
	else
	{
		const char *args [] =
		{
			"ftedroid",
			"-basepack",
			NULL,	/*filled in later*/
			"",
			""
		};
		int align;
		quakeparms_t parms;
		if (sys_memheap)
			free(sys_memheap);
		parms.basedir = NULL;	/*filled in later*/
		parms.argc = 3;
		parms.argv = args;
		parms.memsize = 16*1024*1024;
		parms.membase = sys_memheap = malloc(parms.memsize);
		if (!parms.membase)
		{
			Sys_Printf("Unable to alloc heap\n");
			return;
		}


		args[2] = parms.membase;
		tmp = (*env)->GetStringUTFChars(env, japkpath, NULL);
		strcpy(args[2], tmp);
		(*env)->ReleaseStringUTFChars(env, japkpath, tmp);
		parms.membase += strlen(args[2])+1;
		parms.memsize -= strlen(args[2])+1;

		parms.basedir = parms.membase;
		tmp = (*env)->GetStringUTFChars(env, jusrpath, NULL);
		strcpy(parms.basedir, tmp);
		(*env)->ReleaseStringUTFChars(env, jusrpath, tmp);
		parms.membase += strlen(parms.basedir)+1;
		parms.memsize -= strlen(parms.basedir)+1;

		align = (int)parms.membase & 15;
		if (align)
		{
			align = 16-align;
			parms.membase += align;
			parms.memsize -= align;
		}


		Sys_Printf("Starting up (apk=%s, usr=%s)\n", args[2], parms.basedir);

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
	//don't implement - fix q2 code instead
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
qboolean Sys_Rename (char *oldfname, char *newfname)
{
	return !rename(oldfname, newfname);
}
void Sys_SendKeyEvents(void)
{
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

#if 0
#include <android/asset_manager.h>
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
#endif
