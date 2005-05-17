//contains generic plugin code for dll/qvm
//it's this one or the engine...
#include "plugin.h"

typedef struct {
	char *name;
	export_t func;
} exports_t;
extern exports_t exports[16];

int vmMain( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11  )
{
	int ret;
	int args[12];
	args[0] = arg0;
	args[1] = arg1;
	args[2] = arg2;
	args[3] = arg3;
	args[4] = arg4;
	args[5] = arg5;
	args[6] = arg6;
	args[7] = arg7;
	args[8] = arg8;
	args[9] = arg9;
	args[10] = arg10;
	args[11] = arg11;
//	return exports[command].func(args);
	ret = exports[command].func(args);
	return ret;
}


#ifndef Q3_VM
int (QDECL *syscall)( int arg, ... );
#endif

#define PASSFLOAT(f) *(int*)&(f)

#define ARGNAMES ,funcname
BUILTINR(void*, Plug_GetEngineFunction, (char *funcname));
#undef ARGNAMES

#define ARGNAMES ,funcname,expnum
BUILTINR(int, Plug_ExportToEngine, (char *funcname, int expnum));
#undef ARGNAMES

#define ARGNAMES ,text
BUILTIN(void, Con_Print, (char *text));	//on to main console.
#undef ARGNAMES

#define ARGNAMES ,message
BUILTIN(void, Sys_Error, (char *message));	//abort the entire engine.
#undef ARGNAMES

#define ARGNAMES ,buffer,bufsize
BUILTIN(void, Cmd_Args, (char *buffer, int bufsize));	//abort the entire engine.
#undef ARGNAMES
#define ARGNAMES ,argnum,buffer,bufsize
BUILTIN(void, Cmd_Argv, (int argnum, char *buffer, int bufsize));	//abort the entire engine.
#undef ARGNAMES
#define ARGNAMES
BUILTIN(void, Cmd_Argc, (void));	//abort the entire engine.
#undef ARGNAMES

#define ARGNAMES ,text,insert
BUILTIN(void, Cmd_AddText, (char *text, qboolean insert));	//abort the entire engine.
#undef ARGNAMES

#define ARGNAMES ,name,value
BUILTIN(void, Cvar_SetString, (char *name, char *value));
#undef ARGNAMES
#define ARGNAMES ,name,value
BUILTIN(void, Cvar_SetFloat, (char *name, float value));
#undef ARGNAMES
#define ARGNAMES ,name,retstring,sizeofretstring
BUILTINR(qboolean, Cvar_GetString, (char *name, char *retstring, int sizeofretstring));
#undef ARGNAMES
#define ARGNAMES ,name
BUILTINR(float, Cvar_GetFloat, (char *name));
#undef ARGNAMES
#define ARGNAMES ,name,defaultval,flags,grouphint
BUILTINR(qhandle_t,	Cvar_Register, (char *name, char *defaultval, int flags, char *grouphint));
#undef ARGNAMES
#define ARGNAMES ,handle,modificationcount,stringv,floatv
BUILTINR(int, Cvar_Update, (qhandle_t handle, int modificationcount, char *stringv, float *floatv));	//stringv is 256 chars long, don't expect this function to do anything if modification count is unchanged.
#undef ARGNAMES

#define ARGNAMES ,pnum,stats,maxstats
BUILTIN(void, CL_GetStats, (int pnum, unsigned int *stats, int maxstats));
#undef ARGNAMES

#define ARGNAMES ,soundname
BUILTIN(void, LocalSound, (char *soundname));
#undef ARGNAMES

#define ARGNAMES ,name,iswadimage
BUILTINR(qhandle_t, Draw_LoadImage, (char *name, qboolean iswadimage));	//wad image is ONLY for loading out of q1 gfx.wad
#undef ARGNAMES
#define ARGNAMES ,PASSFLOAT(x),PASSFLOAT(y),PASSFLOAT(w),PASSFLOAT(h),PASSFLOAT(s1),PASSFLOAT(t1),PASSFLOAT(s2),PASSFLOAT(t2),image
BUILTIN(void, Draw_Image, (float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t image));
#undef ARGNAMES
#define ARGNAMES ,PASSFLOAT(x),PASSFLOAT(y),PASSFLOAT(w),PASSFLOAT(h)
BUILTIN(void, Draw_Fill,	(int x, int y, int w, int h));
#undef ARGNAMES
#define ARGNAMES ,x,y,characture
BUILTIN(void, Draw_Character, (int x, int y, unsigned int characture));
#undef ARGNAMES
#define ARGNAMES ,palcol
BUILTIN(void, Draw_Colourp, (int palcol));
#undef ARGNAMES
#define ARGNAMES ,PASSFLOAT(r),PASSFLOAT(g),PASSFLOAT(b)
BUILTIN(void, Draw_Colour3f, (float r, float g, float b));
#undef ARGNAMES
#define ARGNAMES ,PASSFLOAT(r),PASSFLOAT(g),PASSFLOAT(b),PASSFLOAT(a)
BUILTIN(void, Draw_Colour4f, (float r, float g, float b, float a));
#undef ARGNAMES

#define ARGNAMES ,mnum
BUILTIN(void, Menu_Control, (int mnum));
#undef ARGNAMES

#define ARGNAMES ,keyname
BUILTINR(int, Key_GetKeyCode, (char *keyname));
#undef ARGNAMES

char	*va(char *format, ...)	//Identical in function to the one in Quake, though I can assure you that I wrote it...
{					//It's not exactly hard, just easy to use, so gets duplicated lots.
	va_list		argptr;
	static char		string[1024];
		
	va_start (argptr, format);
	vsnprintf (string, sizeof(string), format,argptr);
	va_end (argptr);

	return string;	
}

void Con_Printf(char *format, ...)
{
	va_list		argptr;
	static char		string[1024];
		
	va_start (argptr, format);
	vsnprintf (string, sizeof(string), format,argptr);
	va_end (argptr);

	Con_Print(string);	
}
void Sys_Errorf(char *format, ...)
{
	va_list		argptr;
	static char		string[1024];
		
	va_start (argptr, format);
	vsnprintf (string, sizeof(string), format,argptr);
	va_end (argptr);

	Sys_Error(string);	
}

void Plug_InitStandardBuiltins(void)
{
	CHECKBUILTIN(Plug_ExportToEngine);
	CHECKBUILTIN(Con_Print);
	CHECKBUILTIN(Sys_Error);

	CHECKBUILTIN(Cmd_Args);
	CHECKBUILTIN(Cmd_Argv);
	CHECKBUILTIN(Cmd_Argc);
	CHECKBUILTIN(Cmd_AddText);

	CHECKBUILTIN(Cvar_SetString);
	CHECKBUILTIN(Cvar_SetFloat);
	CHECKBUILTIN(Cvar_GetString);
	CHECKBUILTIN(Cvar_GetFloat);
	CHECKBUILTIN(Cvar_Register);
	CHECKBUILTIN(Cvar_Update);

	CHECKBUILTIN(CL_GetStats);
	CHECKBUILTIN(LocalSound);
	CHECKBUILTIN(Menu_Control);
	CHECKBUILTIN(Key_GetKeyCode);

	CHECKBUILTIN(Draw_LoadImage);
	CHECKBUILTIN(Draw_Image);
	CHECKBUILTIN(Draw_Fill);
	CHECKBUILTIN(Draw_Character);
	CHECKBUILTIN(Draw_Colourp);
	CHECKBUILTIN(Draw_Colour3f);
	CHECKBUILTIN(Draw_Colour4f);
}

#ifndef Q3_VM
void dllEntry(int (QDECL *funcptr)(int,...))
{
	syscall = funcptr;
}
#endif

vmvideo_t vid;
int Plug_UpdateVideo(int *args)
{
	vid.width = args[0];
	vid.height = args[1];

	return true;
}

int Plug_InitAPI(int *args)
{
#ifdef Q3_VM
	Plug_GetEngineFunction = (void*)args[0];
#else
	BUILTIN_Plug_GetEngineFunction = args[0];
#endif

	Plug_InitStandardBuiltins();

	Plug_Export("UpdateVideo", Plug_UpdateVideo);
	return Plug_Init(args);
}

qboolean Plug_Export(char *name, export_t func)
{
	int i;
	for (i = 0; i < sizeof(exports)/sizeof(exports[0]); i++)
	{
		if (!exports[i].name)
		{
			exports[i].name = name;
			exports[i].func = func;
			return Plug_ExportToEngine(name, i);
		}
	}
	Sys_Error("Plugin exports too many functions");
	return 0;
}


exports_t exports[sizeof(exports)/sizeof(exports[0])] = {
	{"Plug_Init", Plug_InitAPI},
};

