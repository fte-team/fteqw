#ifndef _VM_H
#define _VM_H

#ifdef _WIN32
#define EXPORT_FN __cdecl
#else
#define EXPORT_FN
#endif




typedef int (EXPORT_FN *sys_calldll_t) (int arg, ...);
typedef long (*sys_callqvm_t) (void *offset, unsigned int mask, int fn, const long *arg);

typedef struct vm_s vm_t;

// for syscall users
#define VM_LONG(x)		(*(long*)&(x))
#define VM_FLOAT(x)		(*(float*)&(x))
#define VM_POINTER(x)	((x)?(void*)((char *)offset+((x)%mask)):NULL)
#define VM_OOB(p,l)		(p + l >= mask || VM_POINTER(p) < offset)
// ------------------------- * interface * -------------------------

void VM_PrintInfo(vm_t *vm);
vm_t *VM_Create(vm_t *vm, const char *name, sys_calldll_t syscalldll, sys_callqvm_t syscallqvm);
void VM_Destroy(vm_t *vm);
qboolean VM_Restart(vm_t *vm);
int VARGS VM_Call(vm_t *vm, int instruction, ...);
void *VM_MemoryBase(vm_t *vm);



//plugin functions
#ifdef PLUGINS
qboolean Plug_Menu_Event(int eventtype, int param);
qboolean Plugin_ExecuteString(void);
void Plug_ResChanged(void);
void Plug_Tick(void);
void Plug_Init(void);

void Plug_SBar(void);
void Plug_DrawReloadImages(void);
#endif




//these things are specific to FTE QuakeWorld
void UI_Init (void);
void UI_Restart_f(void);
void UI_Stop (void);
qboolean UI_Q2LayoutChanged(void);
void UI_StringChanged(int num);
void UI_MousePosition(int xpos, int ypos);
int UI_MenuState(void);
qboolean UI_KeyPress(int key, qboolean down);
void UI_Reset(void);
void UI_DrawMenu(void);
qboolean UI_DrawStatusBar(int scores);
qboolean UI_DrawIntermission(void);
qboolean UI_DrawFinale(void);
int UI_MenuState(void);

#ifdef VM_CG
void CG_Stop (void);
void CG_Start (void);
int CG_Refresh(void);
qboolean CG_Command(void);
#endif

#endif
