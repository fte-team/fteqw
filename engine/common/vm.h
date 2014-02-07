#ifndef _VM_H
#define _VM_H

#ifdef _WIN32
	#define EXPORT_FN __cdecl
#else
	#define EXPORT_FN
#endif

#if __STDC_VERSION__ >= 199901L
	//C99 has a stdint header which hopefully contains an intptr_t
	//its optional... but if its not in there then its unlikely you'll actually be able to get the engine to a stage where it *can* load anything
	#include <stdint.h>
	#define qintptr_t intptr_t
	#define quintptr_t uintptr_t
#else
	#if defined(_WIN64)
		#define qintptr_t __int64
		#define FTE_WORDSIZE 64
		#define quintptr_t unsigned qintptr_t
	#elif defined(_WIN32)
		typedef __int32 qintptr_t;	//add __w64 if you need msvc to shut up about unsafe type conversions
		typedef unsigned __int32 quintptr_t;
//		#define qintptr_t __int32
//		#define quintptr_t unsigned qintptr_t
		#define FTE_WORDSIZE 32
	#else
		#if __WORDSIZE == 64
			#define qintptr_t long long
			#define FTE_WORDSIZE 64
		#else
			#define qintptr_t long
			#define FTE_WORDSIZE 32
		#endif
		#define quintptr_t unsigned qintptr_t
	#endif
#endif

#ifndef FTE_WORDSIZE
#ifdef __WORDSIZE
#define FTE_WORDSIZE __WORDSIZE
#else
#define FTE_WORDSIZE 32
#endif
#endif

typedef qintptr_t (EXPORT_FN *sys_calldll_t) (qintptr_t arg, ...);
typedef int (*sys_callqvm_t) (void *offset, quintptr_t mask, int fn, const int *arg);

typedef struct vm_s vm_t;

// for syscall users
#define VM_LONG(x)		(*(int*)&(x))	//note: on 64bit platforms, the later bits can contain junk
#define VM_FLOAT(x)		(*(float*)&(x))	//note: on 64bit platforms, the later bits can contain junk
#define VM_POINTER(x)	((x)?(void*)((char *)offset+((x)%mask)):NULL)
#define VM_OOB(p,l)		(p + l >= mask || VM_POINTER(p) < offset)
// ------------------------- * interface * -------------------------

void VM_PrintInfo(vm_t *vm);
vm_t *VM_Create(const char *name, sys_calldll_t syscalldll, sys_callqvm_t syscallqvm);
void VM_Destroy(vm_t *vm);
//qboolean VM_Restart(vm_t *vm);
qintptr_t VARGS VM_Call(vm_t *vm, qintptr_t instruction, ...);
qboolean VM_NonNative(vm_t *vm);
void *VM_MemoryBase(vm_t *vm);
quintptr_t VM_MemoryMask(vm_t *vm);



//plugin functions
#ifdef PLUGINS
qboolean	Plug_CenterPrintMessage(char *buffer, int clientnum);
qboolean	Plug_ChatMessage(char *buffer, int talkernum, int tpflags);
void		Plug_Command_f(void);
int			Plug_ConnectionlessClientPacket(char *buffer, int size);
qboolean	Plug_ConsoleLink(char *text, char *info);
qboolean	Plug_ConsoleLinkMouseOver(float x, float y, char *text, char *info);
void		Plug_DrawReloadImages(void);
void		Plug_Initialise(qboolean fromgamedir);
void		Plug_Shutdown(qboolean preliminary);
qboolean	Plug_Menu_Event(int eventtype, int param);
void		Plug_ResChanged(void);
void		Plug_SBar(playerview_t *pv);
qboolean	Plug_ServerMessage(char *buffer, int messagelevel);
void		Plug_Tick(void);
qboolean	Plugin_ExecuteString(void);
#endif


#define VM_TOSTRCACHE(a) VMQ3_StringToHandle(VM_POINTER(a))
#define VM_FROMSTRCACHE(a) VMQ3_StringFromHandle(a)
char *VMQ3_StringFromHandle(int handle);
int VMQ3_StringToHandle(char *str);
void VMQ3_FlushStringHandles(void);

#ifdef VM_UI
qboolean UI_Command(void);
void UI_Init (void);
void UI_Start (void);
void UI_Stop (void);
qboolean UI_OpenMenu(void);
void UI_Restart_f(void);
qboolean UI_Q2LayoutChanged(void);
void UI_StringChanged(int num);
qboolean UI_MousePosition(int xpos, int ypos);
int UI_MenuState(void);
qboolean UI_KeyPress(int key, int unicode, qboolean down);
void UI_Reset(void);
void UI_DrawMenu(void);
qboolean UI_DrawStatusBar(int scores);
qboolean UI_DrawIntermission(void);
qboolean UI_DrawFinale(void);
int UI_MenuState(void);

//sans botlib
struct pc_token_s;
int Script_LoadFile(char *filename);
void Script_Free(int handle);
int Script_Read(int handle, struct pc_token_s *token);
void Script_Get_File_And_Line(int handle, char *filename, int *line);
#endif

#define VM_FS_READ 0
#define VM_FS_WRITE 1
#define VM_FS_APPEND 2
#define VM_FS_APPEND2 3	//I don't know, don't ask me. look at q3 source
int VM_fopen (char *name, int *handle, int fmode, int owner);
int VM_FRead (char *dest, int quantity, int fnum, int owner);
void VM_fclose (int fnum, int owner);
void VM_fcloseall (int owner);
int VM_GetFileList(char *path, char *ext, char *output, int buffersize);

#ifdef VM_CG
void CG_Stop (void);
void CG_Start (void);
int CG_Refresh(void);
qboolean CG_Command(void);
qboolean CG_KeyPress(int key, int unicode, int down);
#endif

typedef struct {
	int			handle;
	int			modificationCount;
	float		value;
	int			integer;
	char		string[256];
} q3vmcvar_t;
int VMQ3_Cvar_Register(q3vmcvar_t *v, char *name, char *defval, int flags);
int VMQ3_Cvar_Update(q3vmcvar_t *v);

#endif
