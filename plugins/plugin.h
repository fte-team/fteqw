#ifdef Q3_VM

//qvms just call the return value, and the engine works out which one it called.
#define EBUILTIN(t, n, args) extern t (*n) args
#define BUILTINR(t, n, args) t (*n) args
#define BUILTIN(t, n, args) t (*n) args
#define CHECKBUILTIN(n) n = (void*)Plug_GetEngineFunction(#n);
#define BUILTINISVALID(n) n!=NULL

#define double float	//all floats are 32bit, qvm stuff

typedef char *va_list;
#define va_start(va,f) (va = (char *)&f + sizeof(int))
#define va_arg(va, type) (*(type *)((va += sizeof(int)) - sizeof(int)))
#define va_end(va) (va = NULL)
#define NULL (void*)0

#else

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
//DLLs need a wrapper to add the extra parameter and call a boring function.
#define EBUILTIN(t, n, args) extern int BUILTIN_##n; t n args
#define BUILTINR(t, n, args) int BUILTIN_##n; t n args {return (t)syscall(BUILTIN_##n ARGNAMES);}
#define BUILTIN(t, n, args) int BUILTIN_##n; t n args {syscall(BUILTIN_##n ARGNAMES);}
#define CHECKBUILTIN(n) BUILTIN_##n = (int)Plug_GetEngineFunction(#n);
#define BUILTINISVALID(n) BUILTIN_##n != 0
#ifdef _WIN32
#define QDECL __cdecl
#else
#define QDECL
#endif
extern int (*syscall)( int arg, ... );

#endif

typedef enum {false, true} qboolean;
typedef void *qhandle_t;



//Basic builtins:
EBUILTIN(void*, Plug_GetEngineFunction, (char *funcname));	//set up in vmMain, use this to get all other builtins
EBUILTIN(void, Con_Print, (char *text));	//on to main console.
EBUILTIN(void, Sys_Error, (char *message));	//abort the entire engine.

EBUILTIN(void, Cmd_Args, (char *buffer, int bufsize));	//abort the entire engine.
EBUILTIN(void, Cmd_Argv, (int argnum, char *buffer, int bufsize));	//abort the entire engine.
EBUILTIN(void, Cmd_Argc, (void));	//abort the entire engine.
EBUILTIN(void, Cmd_AddText, (char *text, qboolean insert));

EBUILTIN(void, Cvar_SetString, (char *name, char *value));
EBUILTIN(void, Cvar_SetFloat, (char *name, float value));
EBUILTIN(qboolean, Cvar_GetString, (char *name, char *retstring, int sizeofretstring));
EBUILTIN(float, Cvar_GetFloat, (char *name));
EBUILTIN(qhandle_t,	Cvar_Register, (char *name, char *defaultval, int flags, char *grouphint));
EBUILTIN(int, Cvar_Update, (qhandle_t handle, int modificationcount, char *stringv, float *floatv));	//stringv is 256 chars long, don't expect this function to do anything if modification count is unchanged.

EBUILTIN(void, LocalSound, (char *soundname));
EBUILTIN(void, CL_GetStats, (int pnum, unsigned int *stats, int maxstats));

EBUILTIN(void, Menu_Control, (int mnum));
EBUILTIN(int, Key_GetKeyCode, (char *keyname));

EBUILTIN(qhandle_t, Draw_LoadImage, (char *name, qboolean iswadimage));	//wad image is ONLY for loading out of q1 gfx.wad
EBUILTIN(void, Draw_Image, (float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t image));
EBUILTIN(void, Draw_Fill,	(int x, int y, int w, int h));
EBUILTIN(void, Draw_Character, (int x, int y, unsigned int characture));
EBUILTIN(void, Draw_Colourp, (int palcol));
EBUILTIN(void, Draw_Colour3f, (float r, float g, float b));
EBUILTIN(void, Draw_Colour4f, (float r, float g, float b, float a));

typedef int (*export_t) (int *args);
char	*va(char *format, ...);
int Plug_Init(int *args);
qboolean Plug_Export(char *name, export_t func);
void Con_Printf(char *format, ...);
void Sys_Errorf(char *format, ...);
typedef unsigned char qbyte;


typedef struct {
	char *name;
	char string[256];
	char *group;
	int flags;
	float value;
	qhandle_t handle;
	int modificationcount;
} vmcvar_t;

typedef struct {
	int width;
	int height;
} vmvideo_t;
extern vmvideo_t vid;

#define VMCvar_SetString(c,v)							\
	do{													\
		strcpy(c->string, v);							\
		c->value = (float)atof(v);						\
		Cvar_SetString(c->name, c->string);				\
	} while (0)
#define VMCvar_SetFloat(c,v)							\
	do {												\
		snprintf(c->string, sizeof(c->string), "%f", v);\
		c->value = (float)(v);							\
		Cvar_SetFloat(c->name, c->value);				\
	} while(0)											\


#define	MAX_INFO_KEY	64
char *Info_ValueForKey (char *s, char *key);
void Info_RemoveKey (char *s, char *key);
void Info_RemovePrefixedKeys (char *start, char prefix);
void Info_RemoveNonStarKeys (char *start);
void Info_SetValueForKey (char *s, char *key, char *value, int maxsize);
void Info_SetValueForStarKey (char *s, char *key, char *value, int maxsize);

