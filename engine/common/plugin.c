//This file should be easily portable.
//The biggest strength of this plugin system is that ALL interactions are performed via
//named functions, this makes it *really* easy to port plugins from one engine to annother.

#include "quakedef.h"



typedef struct plugin_s {
	char *name;
	vm_t *vm;
	int tick;
	int executestring;
	int menufunction;

	struct plugin_s *next;
} plugin_t;

plugin_t *currentplug;

//custom plugin builtins.
typedef int (*Plug_Builtin_t)(void *offset, unsigned int mask, const long *arg);
void Plug_RegisterBuiltin(char *name, Plug_Builtin_t bi, int flags);
#define PLUG_BIF_DLLONLY	1
#define PLUG_BIF_QVMONLY	2

void Plug_Init(void);

void Plug_Tick(void);
qboolean Plugin_ExecuteString(void);
void Plug_Shutdown(void);


static plugin_t *plugs;
static plugin_t *menuplug;	//plugin that has the current menu


typedef struct {
	char *name;
	Plug_Builtin_t func;
} Plug_Plugins_t;
Plug_Plugins_t *plugbuiltins;
int numplugbuiltins;

void Plug_RegisterBuiltin(char *name, Plug_Builtin_t bi, int flags)
{
	//randomize the order a little.
	int newnum;
	
	newnum = rand()%128;
	while(newnum < numplugbuiltins && plugbuiltins[newnum].func)
		newnum+=128;

	if (newnum > numplugbuiltins)
	{
		numplugbuiltins = newnum+128;
		plugbuiltins = BZ_Realloc(plugbuiltins, sizeof(Plug_Plugins_t)*numplugbuiltins);
	}

	//got an empty number.
	plugbuiltins[newnum].name = name;
	plugbuiltins[newnum].func = bi;
}

/*
static void Plug_RegisterBuiltinIndex(char *name, Plug_Builtin_t bi, int flags, int index)	//I d
{
	//randomize the order a little.
	int newnum;
	
	newnum = rand()%128;
	while(newnum+1 < numplugbuiltins && plugbuiltins[newnum+1].func)
		newnum+=128;

	newnum++;

	if (newnum > numplugbuiltins)
	{
		numplugbuiltins = newnum+128;
		plugbuiltins = BZ_Realloc(plugbuiltins, sizeof(Plug_Plugins_t)*numplugbuiltins);
	}

	//got an empty number.
	plugbuiltins[newnum].name = name;
	plugbuiltins[newnum].func = bi;
}
*/

int Plug_FindBuiltin(void *offset, unsigned int mask, const long *args)
{
	int i;
	for (i = 0; i < numplugbuiltins; i++)
		if (plugbuiltins[i].name)
			if (!strcmp(plugbuiltins[i].name, (char *)VM_POINTER(args[0])))
				return -i;

	return 0;
}

long Plug_SystemCallsEx(void *offset, unsigned int mask, int fn, const long *arg)
{
	fn = fn+1;

	if (fn>=0 && fn < numplugbuiltins && plugbuiltins[fn].func!=NULL)
		return plugbuiltins[fn].func(offset, mask, arg);

	Sys_Error("QVM Plugin tried calling invalid builtin %i", fn);
	return 0;
}

#ifdef _DEBUG
static long Plug_SystemCallsExWrapper(void *offset, unsigned int mask, int fn, const long *arg)
{	//this is so we can use edit and continue properly (vc doesn't like function pointers for edit+continue)
	return Plug_SystemCallsEx(offset, mask, fn, arg);
}
#define Plug_SystemCallsEx Plug_SystemCallsExWrapper
#endif

//I'm not keen on this.
//but dlls call it without saying what sort of vm it comes from, so I've got to have them as specifics
static int EXPORT_FN Plug_SystemCalls(int arg, ...)
{
	long args[9];
	va_list argptr;

	va_start(argptr, arg);
	args[0]=va_arg(argptr, int);
	args[1]=va_arg(argptr, int);
	args[2]=va_arg(argptr, int);
	args[3]=va_arg(argptr, int);
	args[4]=va_arg(argptr, int);
	args[5]=va_arg(argptr, int);
	args[6]=va_arg(argptr, int);
	args[7]=va_arg(argptr, int);
	args[8]=va_arg(argptr, int);
	va_end(argptr);

	arg = -arg;

	if (arg>=0 && arg < numplugbuiltins && plugbuiltins[arg].func)
		return plugbuiltins[arg].func(NULL, ~0, args);

	Sys_Error("DLL Plugin tried calling invalid biultin %i", arg);
	return 0;
}


plugin_t *Plug_Load(char *file)
{
	plugin_t *newplug;
	int argarray;

	for (newplug = plugs; newplug; newplug = newplug->next)
	{
		if (!stricmp(newplug->name, file))
			return NULL;
	}

	newplug = Z_Malloc(sizeof(plugin_t)+strlen(file)+1);
	newplug->name = (char*)(newplug+1);
	strcpy(newplug->name, file);
	
	newplug->vm = VM_Create(NULL, file, Plug_SystemCalls, Plug_SystemCallsEx);
	currentplug = newplug;
	if (newplug->vm)
	{
		newplug->next = plugs;
		plugs = newplug;

		argarray = (int)"Plug_GetEngineFunction";
		VM_Call(newplug->vm, 0, Plug_FindBuiltin(NULL, ~0, &argarray));
	}
	else
	{
		Z_Free(newplug);
		newplug = NULL;
	}
	currentplug = NULL;

	return newplug;
}

int Plug_Emumerated (char *name, int size, void *param)
{
	char vmname[MAX_QPATH];
	strcpy(vmname, name);
	vmname[strlen(vmname) - strlen(param)] = '\0';
	Plug_Load(vmname);

	return true;
}

int Plug_Con_Print(void *offset, unsigned int mask, const long *arg)
{
	Con_Print((char*)VM_POINTER(arg[0]));
	return 0;
}
int Plug_Sys_Error(void *offset, unsigned int mask, const long *arg)
{
	Sys_Error("%s", (char*)offset+arg[0]);
	return 0;
}
int Plug_ExportToEngine(void *offset, unsigned int mask, const long *arg)
{
	char *name = (char*)VM_POINTER(arg[0]);
	if (!strcmp(name, "Tick"))
		currentplug->tick = arg[1];
	else if (!strcmp(name, "ExecuteCommand"))
		currentplug->executestring = arg[1];
	else if (!strcmp(name, "MenuEvent"))
		currentplug->menufunction = arg[1];
	else
		return 0;
	return 1;
}
//void(char *buffer, int buffersize)
int Plug_Cmd_Args(void *offset, unsigned int mask, const long *arg)
{
	char *buffer = (char*)VM_POINTER(arg[0]);
	char *args;
	args = Cmd_Args();
	if (strlen(args)+1>arg[1])
		return 0;
	strcpy(buffer, args);
	return 1;
}
//void(int num, char *buffer, int buffersize)
int Plug_Cmd_Argv(void *offset, unsigned int mask, const long *arg)
{
	char *buffer = (char*)VM_POINTER(arg[1]);
	char *args;
	args = Cmd_Argv(arg[0]);
	if (strlen(args)+1>arg[2])
		return 0;
	strcpy(buffer, args);
	return 1;
}
//int(void)
int Plug_Cmd_Argc(void *offset, unsigned int mask, const long *arg)
{
	return Cmd_Argc();
}

int Plug_Menu_Control(void *offset, unsigned int mask, const long *arg)
{
	switch(VM_LONG(arg[0]))
	{
	case 0: //weather it's us or not.
		return currentplug == menuplug && m_state == m_plugin;
	case 1:	//weather a menu is active
		return key_dest == key_menu;
	case 2:	//give us menu control
		menuplug = currentplug;
		key_dest = key_menu;
		return 1;
	default:
		return 0;
	}
}

void Plug_Init(void)
{
	Plug_RegisterBuiltin("Plug_GetEngineFunction", Plug_FindBuiltin, 0);//plugin wishes to find a builtin number.
	Plug_RegisterBuiltin("Plug_ExportToEngine", Plug_ExportToEngine, 0);	//plugin has a call back that we might be interested in.
	Plug_RegisterBuiltin("Con_Print", Plug_Con_Print, 0);	//printf is not possible - qvm floats are never doubles, vararg floats in a cdecl call are always converted to doubles.
	Plug_RegisterBuiltin("Sys_Error", Plug_Sys_Error, 0);
	Plug_RegisterBuiltin("Com_Error", Plug_Sys_Error, 0);	//make zquake programmers happy.

	Plug_RegisterBuiltin("Cmd_Args", Plug_Cmd_Args, 0);
	Plug_RegisterBuiltin("Cmd_Argc", Plug_Cmd_Argc, 0);
	Plug_RegisterBuiltin("Cmd_Argv", Plug_Cmd_Argv, 0);

	Plug_RegisterBuiltin("Menu_Control", Plug_Menu_Control, 0);

#ifdef _WIN32
	COM_EnumerateFiles("plugins/*x86.dll", Plug_Emumerated, "x86.dll");
#elif defined(__linux__)
	COM_EnumerateFiles("plugins/*x86.so", Plug_Emumerated, "x86.so");
#endif
	COM_EnumerateFiles("plugins/*.qvm", Plug_Emumerated, ".qvm");
}

void Plug_Tick(void)
{
	plugin_t *plug;
	for (plug = plugs; plug; plug = plug->next)
	{
		if (plug->tick)
		{
			currentplug = plug;
			VM_Call(plug->vm, plug->tick, (int)(realtime*1000));
		}
	}
}

qboolean Plugin_ExecuteString(void)
{
	plugin_t *plug;
	if (Cmd_Argc()>0)
	{
		for (plug = plugs; plug; plug = plug->next)
		{
			if (plug->executestring)
			{
				currentplug = plug;
				if (VM_Call(plug->vm, plug->executestring, 0))
					return true;
			}
		}
	}
	return false;
}

qboolean Plug_Menu_Event(int eventtype, int param)	//eventtype = draw/keydown/keyup, param = time/key
{
	if (!menuplug)
		return false;

	return VM_Call(menuplug->vm, menuplug->menufunction, eventtype, param);
}

void Plug_Close(plugin_t *plug)
{
	if (plugs == plug)
		plugs = plug->next;
	else
	{
		plugin_t *prev;
		for (prev = plugs; prev; prev = prev->next)
		{
			if (prev->next == plug)
				break;
		}
		if (!prev)
			Sys_Error("Plug_Close: not linked\n");
		prev->next = plug->next;
	}

	VM_Destroy(plug->vm);
}

void Plug_Shutdown(void)
{

}