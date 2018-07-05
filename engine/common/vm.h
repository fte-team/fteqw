#ifndef _VM_H
#define _VM_H

#ifdef _WIN32
	#define EXPORT_FN __cdecl
#else
	#define EXPORT_FN
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
vm_t *VM_CreateBuiltin(const char *name, sys_calldll_t syscalldll, qintptr_t (*init)(qintptr_t *args));
vm_t *VM_Create(const char *name, sys_calldll_t syscalldll, sys_callqvm_t syscallqvm);
const char *VM_GetFilename(vm_t *vm);
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
qboolean	Plug_ConsoleLink(char *text, char *info, const char *consolename);
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
#define VM_FS_APPEND_SYNC 3	//I don't know, don't ask me. look at q3 source
qofs_t VM_fopen (const char *name, int *handle, int fmode, int owner);
int VM_FRead (char *dest, int quantity, int fnum, int owner);
int VM_FWrite (const char *dest, int quantity, int fnum, int owner);
qboolean VM_FSeek (int fnum, qofs_t offset, int seektype, int owner);
qofs_t VM_FTell (int fnum, int owner);
void VM_fclose (int fnum, int owner);
void VM_fcloseall (int owner);
int VM_GetFileList(const char *path, const char *ext, char *output, int buffersize);

#ifdef VM_CG
void CG_Stop (void);
void CG_Start (void);
qboolean CG_VideoRestarted(void);
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
