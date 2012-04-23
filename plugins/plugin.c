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
int (QDECL *plugin_syscall)( int arg, ... );
#endif

#define PASSFLOAT(f) *(int*)&(f)

#define ARGNAMES ,funcname
BUILTINR(funcptr_t, Plug_GetEngineFunction, (char *funcname));
#undef ARGNAMES

#define ARGNAMES ,funcname,expnum
BUILTINR(int, Plug_ExportToEngine, (char *funcname, int expnum));
#undef ARGNAMES

#ifndef Q3_VM
#define ARGNAMES ,funcname,func
BUILTINR(qboolean, Plug_ExportNative, (char *funcname, void *func));
#undef ARGNAMES
#endif

#define ARGNAMES ,text
BUILTIN(void, Con_Print, (char *text));	//on to main console.
#undef ARGNAMES

#define ARGNAMES ,conname,text
BUILTIN(void, Con_SubPrint, (char *conname, char *text));	//on to named sub console (creating it too).
#undef ARGNAMES
#define ARGNAMES ,old,new
BUILTIN(void, Con_RenameSub, (char *old, char *new));	//rename a subconsole
#undef ARGNAMES
#define ARGNAMES ,conname
BUILTINR(int, Con_IsActive, (char *conname));
#undef ARGNAMES
#define ARGNAMES ,conname
BUILTIN(void, Con_SetActive, (char *conname));
#undef ARGNAMES
#define ARGNAMES ,conname
BUILTIN(void, Con_Destroy, (char *conname));
#undef ARGNAMES
#define ARGNAMES ,connum,conname,connamelen
BUILTIN(void, Con_NameForNum, (int connum, char *conname, int connamelen));
#undef ARGNAMES

#define ARGNAMES ,message
BUILTIN(void, Sys_Error, (char *message));	//abort the entire engine.
#undef ARGNAMES
#define ARGNAMES 
BUILTINR(unsigned int, Sys_Milliseconds, (void));	//get the time the engine has been running.
#undef ARGNAMES

#define ARGNAMES ,buffer
BUILTINR(int, Cmd_AddCommand, (char *buffer));	//register a command.
#undef ARGNAMES
#define ARGNAMES ,buffer,bufsize
BUILTIN(void, Cmd_Args, (char *buffer, int bufsize));	//retrieve some arguments.
#undef ARGNAMES
#define ARGNAMES ,argnum,buffer,bufsize
BUILTIN(void, Cmd_Argv, (int argnum, char *buffer, int bufsize));	//retrieve a single argument at a time.
#undef ARGNAMES
#define ARGNAMES
BUILTINR(int, Cmd_Argc, (void));	//get the argument count.
#undef ARGNAMES
#define ARGNAMES ,msg
BUILTIN(void, Cmd_TokenizeString, (char *msg));	//tokenize a string.
#undef ARGNAMES

#define ARGNAMES ,text,insert
BUILTIN(void, Cmd_AddText, (char *text, qboolean insert));	//add stuff to the console input.
#undef ARGNAMES

#define ARGNAMES ,name,value
BUILTIN(void, Cvar_SetString, (char *name, char *value));	//set a cvar string
#undef ARGNAMES
#define ARGNAMES ,name,PASSFLOAT(value)
BUILTIN(void, Cvar_SetFloat, (char *name, float value));	//set a cvar float
#undef ARGNAMES
#define ARGNAMES ,name,retstring,sizeofretstring
BUILTINR(qboolean, Cvar_GetString, (char *name, char *retstring, int sizeofretstring));	//retrieve a cvar string
#undef ARGNAMES
#define ARGNAMES ,name
BUILTINR(float, Cvar_GetFloat, (char *name));			//get a cvar's value
#undef ARGNAMES
#define ARGNAMES ,name,defaultval,flags,grouphint
BUILTINR(qhandle_t,	Cvar_Register, (char *name, char *defaultval, int flags, char *grouphint));	//register a new cvar
#undef ARGNAMES
#define ARGNAMES ,handle,modificationcount,stringv,floatv
BUILTINR(int, Cvar_Update, (qhandle_t handle, int *modificationcount, char *stringv, float *floatv));	//stringv is 256 chars long, don't expect this function to do anything if modification count is unchanged.
#undef ARGNAMES

#define ARGNAMES ,pnum,stats,maxstats
BUILTIN(void, CL_GetStats, (int pnum, unsigned int *stats, int maxstats));
#undef ARGNAMES
#define ARGNAMES ,pnum,info
BUILTINR(int, GetPlayerInfo, (int pnum, plugclientinfo_t *info));
#undef ARGNAMES
#define ARGNAMES
BUILTINR(int, LocalPlayerNumber, (void));
#undef ARGNAMES
#define ARGNAMES ,info,infolen
BUILTIN(void, GetServerInfo, (char *info, int infolen));
#undef ARGNAMES
#define ARGNAMES ,key,value
BUILTIN(void, SetUserInfo, (char *key, char *value));
#undef ARGNAMES

#define ARGNAMES ,pos,buffer,bufferlen
BUILTIN(void, GetLocationName, (float *pos, char *buffer, int bufferlen));
#undef ARGNAMES

#define ARGNAMES ,soundname
BUILTIN(void, LocalSound, (char *soundname));
#undef ARGNAMES

#define ARGNAMES ,plugnum, buffer, bufsize
BUILTIN(void, GetPluginName, (int plugnum, char *buffer, int bufsize));
#undef ARGNAMES

#define ARGNAMES ,name,iswadimage
BUILTINR(qhandle_t, Draw_LoadImage, (char *name, qboolean iswadimage));	//wad image is ONLY for loading out of q1 gfx.wad
#undef ARGNAMES
#define ARGNAMES ,PASSFLOAT(x),PASSFLOAT(y),PASSFLOAT(w),PASSFLOAT(h),PASSFLOAT(s1),PASSFLOAT(t1),PASSFLOAT(s2),PASSFLOAT(t2),image
BUILTINR(int, Draw_Image, (float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t image));
#undef ARGNAMES
#define ARGNAMES ,PASSFLOAT(x1),PASSFLOAT(y1),PASSFLOAT(x2),PASSFLOAT(y2)
BUILTIN(void, Draw_Line, (float x1, float y1, float x2, float y2));
#undef ARGNAMES
#define ARGNAMES ,PASSFLOAT(x),PASSFLOAT(y),PASSFLOAT(w),PASSFLOAT(h)
BUILTIN(void, Draw_Fill,	(float x, float y, float w, float h));
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

#define ARGNAMES ,s
BUILTIN(void, SCR_CenterPrint, (char *s));
#undef ARGNAMES

#define ARGNAMES ,mnum
BUILTIN(void, Menu_Control, (int mnum));
#undef ARGNAMES

#define ARGNAMES ,keyname
BUILTINR(int, Key_GetKeyCode, (char *keyname));
#undef ARGNAMES

#define ARGNAMES ,name,handle,mode
BUILTINR(int, FS_Open, (char *name, qhandle_t *handle, int mode));
#undef ARGNAMES
#define ARGNAMES ,handle
BUILTIN(void, FS_Close, (qhandle_t handle));
#undef ARGNAMES
#define ARGNAMES ,handle,data,len
BUILTINR(int, FS_Write, (qhandle_t handle, void *data, int len));
#undef ARGNAMES
#define ARGNAMES ,handle,data,len
BUILTINR(int, FS_Read, (qhandle_t handle, void *data, int len));
#undef ARGNAMES


#define ARGNAMES ,ip,port
BUILTINR(qhandle_t, Net_TCPConnect, (char *ip, int port));
#undef ARGNAMES
#define ARGNAMES ,ip,port,maxcount
BUILTINR(qhandle_t, Net_TCPListen, (char *ip, int port, int maxcount));
#undef ARGNAMES
#define ARGNAMES ,socket,address,addresslen
BUILTINR(qhandle_t, Net_Accept, (qhandle_t socket, char *address, int addresslen));
#undef ARGNAMES
#define ARGNAMES ,socket,buffer,len
BUILTINR(int, Net_Recv, (qhandle_t socket, void *buffer, int len));
#undef ARGNAMES
#define ARGNAMES ,socket,buffer,len
BUILTINR(int, Net_Send, (qhandle_t socket, void *buffer, int len));
#undef ARGNAMES
#define ARGNAMES ,socket
BUILTIN(void, Net_Close, (qhandle_t socket));
#undef ARGNAMES

#define ARGNAMES ,inputbuffer,buffersize
BUILTINR(int, ReadInputBuffer, (void *inputbuffer, int buffersize));
#undef ARGNAMES
#define ARGNAMES ,inputbuffer,bytes
BUILTINR(int, UpdateInputBuffer, (void *inputbuffer, int bytes));
#undef ARGNAMES

#ifdef Q3_VM
#define ARGNAMES ,out,in,len
BUILTIN(void, memcpy, (void *out, void *in, int len));
#undef ARGNAMES
#define ARGNAMES ,out,in,len
BUILTIN(void, memset, (void *out, int in, int len));
#undef ARGNAMES
#define ARGNAMES ,out,in,len
BUILTIN(void, memmove, (void *out, void *in, int len));
#undef ARGNAMES
#define ARGNAMES ,PASSFLOAT(f)
BUILTINR(float, sqrt, (float f));
BUILTINR(float, sin, (float f));
BUILTINR(float, cos, (float f));
#undef ARGNAMES
#endif


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

void BadBuiltin(void)
{
	Sys_Error("Plugin tried calling a missing builtin\n");
}

void Plug_InitStandardBuiltins(void)
{
	//con_print is used if the others don't exist, and MUST come first (for the sake of sanity)
	CHECKBUILTIN(Con_Print);

	CHECKBUILTIN(Plug_ExportToEngine);
#ifndef Q3_VM
	CHECKBUILTIN(Plug_ExportNative);
#endif
	CHECKBUILTIN(Sys_Error);

	CHECKBUILTIN(ReadInputBuffer);
	CHECKBUILTIN(UpdateInputBuffer);

#ifdef Q3_VM
	CHECKBUILTIN(memcpy);
	CHECKBUILTIN(memmove);
	CHECKBUILTIN(memset);
	CHECKBUILTIN(sqrt);
	CHECKBUILTIN(sin);
	CHECKBUILTIN(cos);
#endif

	CHECKBUILTIN(Sys_Milliseconds);

	//command execution
	CHECKBUILTIN(Cmd_AddCommand);
	CHECKBUILTIN(Cmd_Args);
	CHECKBUILTIN(Cmd_Argv);
	CHECKBUILTIN(Cmd_Argc);
	CHECKBUILTIN(Cmd_AddText);

	//cvar stuff
	CHECKBUILTIN(Cvar_SetString);
	CHECKBUILTIN(Cvar_SetFloat);
	CHECKBUILTIN(Cvar_GetString);
	CHECKBUILTIN(Cvar_GetFloat);
	CHECKBUILTIN(Cvar_Register);
	CHECKBUILTIN(Cvar_Update);

	//file system
	CHECKBUILTIN(FS_Open);
	CHECKBUILTIN(FS_Read);
	CHECKBUILTIN(FS_Write);
	CHECKBUILTIN(FS_Close);

	//networking
	CHECKBUILTIN(Net_TCPConnect);
	CHECKBUILTIN(Net_TCPListen);
	CHECKBUILTIN(Net_Accept);
	CHECKBUILTIN(Net_Recv);
	CHECKBUILTIN(Net_Send);
	CHECKBUILTIN(Net_Close);

	//random things
	CHECKBUILTIN(CL_GetStats);
	CHECKBUILTIN(GetPlayerInfo);
	CHECKBUILTIN(LocalPlayerNumber);
	CHECKBUILTIN(GetServerInfo);
	CHECKBUILTIN(SetUserInfo);
	CHECKBUILTIN(LocalSound);
	CHECKBUILTIN(Menu_Control);
	CHECKBUILTIN(Key_GetKeyCode);
	CHECKBUILTIN(GetLocationName);

	//drawing routines
	CHECKBUILTIN(Draw_LoadImage);
	CHECKBUILTIN(Draw_Image);
	CHECKBUILTIN(Draw_Line);
	CHECKBUILTIN(Draw_Fill);
	CHECKBUILTIN(Draw_Character);
	CHECKBUILTIN(Draw_Colourp);
	CHECKBUILTIN(Draw_Colour3f);
	CHECKBUILTIN(Draw_Colour4f);
	CHECKBUILTIN(SCR_CenterPrint);

	CHECKBUILTIN(GetPluginName);

	//sub consoles (optional)
	CHECKBUILTIN(Con_SubPrint);
	CHECKBUILTIN(Con_RenameSub);
	CHECKBUILTIN(Con_IsActive);
	CHECKBUILTIN(Con_SetActive);
	CHECKBUILTIN(Con_Destroy);
	CHECKBUILTIN(Con_NameForNum);
}

#ifndef Q3_VM
void dllEntry(int (QDECL *funcptr)(int,...))
{
	plugin_syscall = funcptr;
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




