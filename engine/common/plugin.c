//This file should be easily portable.
//The biggest strength of this plugin system is that ALL interactions are performed via
//named functions, this makes it *really* easy to port plugins from one engine to annother.

#include "quakedef.h"

#ifdef PLUGINS

#include "glquake.h"

typedef struct plugin_s {
	char *name;
	vm_t *vm;
	int tick;
	int executestring;
	int menufunction;
	int sbarlevel[3];	//0 - main sbar, 1 - supplementry sbar sections (make sure these can be switched off), 2 - overlays (scoreboard). menus kill all.
	int reschange;

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

		if (newplug->reschange)
			VM_Call(newplug->vm, newplug->reschange, vid.width, vid.height);
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
	else if (!strcmp(name, "UpdateVideo"))
		currentplug->reschange = arg[1];
	else if (!strcmp(name, "SbarBase"))			//basic SBAR.
		currentplug->sbarlevel[0] = arg[1];
	else if (!strcmp(name, "SbarSupplement"))	//supplementry stuff - teamplay
		currentplug->sbarlevel[1] = arg[1];
	else if (!strcmp(name, "SbarOverlay"))		//overlay - scoreboard type stuff.
		currentplug->sbarlevel[2] = arg[1];
	else
		return 0;
	return 1;
}

typedef struct {
	//Make SURE that the engine has resolved all cvar pointers into globals before this happens.
	plugin_t *plugin;
	cvar_t *var;
} plugincvararray_t;
int plugincvararraylen;
plugincvararray_t *plugincvararray;
//qhandle_t Cvar_Register (char *name, char *defaultval, int flags, char *grouphint);
int Plug_Cvar_Register(void *offset, unsigned int mask, const long *arg)
{
	char *name = VM_POINTER(arg[0]);
	char *defaultvalue = VM_POINTER(arg[1]);
	unsigned int flags = VM_LONG(arg[2]);
	char *groupname = VM_POINTER(arg[3]);
	cvar_t *var;
	int i;

	var = Cvar_Get(name, defaultvalue, flags&1, groupname);

	for (i = 0; i < plugincvararraylen; i++)
	{
		if (!plugincvararray[i].var)
		{	//hmm... a gap...
			plugincvararray[i].plugin = currentplug;
			plugincvararray[i].var = var;
			return i;
		}
	}

	plugincvararray = BZ_Realloc(plugincvararray, (plugincvararraylen+1)*sizeof(plugincvararray_t));
	plugincvararray[plugincvararraylen].plugin = currentplug;
	plugincvararray[plugincvararraylen].var = var;
	plugincvararraylen++;
	return plugincvararraylen-1;
}
//int Cvar_Update, (qhandle_t handle, int modificationcount, char *stringv, float *floatv));	//stringv is 256 chars long, don't expect this function to do anything if modification count is unchanged.
int Plug_Cvar_Update(void *offset, unsigned int mask, const long *arg)
{
	int handle;
	int modcount;
	char *stringv;	//255 bytes long.
	float *floatv;
	cvar_t *var;
	handle = VM_LONG(arg[0]);
	if (handle < 0 || handle >= plugincvararraylen)
		return 0;
	if (plugincvararray[handle].plugin != currentplug)
		return 0;	//I'm not letting you know what annother plugin has registered.

	if (VM_OOB(arg[2], 256) || VM_OOB(arg[3], 4))	//Oi, plugin - you screwed up
		return 0;

	modcount = VM_LONG(arg[1]);
	stringv = VM_POINTER(arg[2]);
	floatv = VM_POINTER(arg[3]);

	var = plugincvararray[handle].var;


	strcpy(stringv, var->string);
	*floatv = var->value;

	return var->modified;
}

//void Cmd_Args(char *buffer, int buffersize)
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
//void Cmd_Argv(int num, char *buffer, int buffersize)
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
//int Cmd_Argc(void)
int Plug_Cmd_Argc(void *offset, unsigned int mask, const long *arg)
{
	return Cmd_Argc();
}

int Plug_Menu_Control(void *offset, unsigned int mask, const long *arg)
{
	switch(VM_LONG(arg[0]))
	{
	case 0:	//take away all menus
	case 1:
		if (menuplug)
		{
			plugin_t *oldplug = currentplug;
			currentplug = menuplug;
			Plug_Menu_Event(3, 0);
			menuplug = NULL;
			currentplug = oldplug;
			key_dest = key_game;
		}
		if (VM_LONG(arg[0]) != 1)
			return 1;
		//give us menu control
		menuplug = currentplug;
		key_dest = key_menu;
		m_state = m_plugin;
		return 1;
	case 2: //weather it's us or not.
		return currentplug == menuplug && m_state == m_plugin;
	case 3:	//weather a menu is active
		return key_dest == key_menu;
	default:
		return 0;
	}
}

typedef struct {
	//Make SURE that the engine has resolved all cvar pointers into globals before this happens.
	plugin_t *plugin;
	char name[64];
	qboolean picfromwad;
	qpic_t *pic;
} pluginimagearray_t;
int pluginimagearraylen;
pluginimagearray_t *pluginimagearray;

int Plug_Draw_LoadImage(void *offset, unsigned int mask, const long *arg)
{
	char *name = VM_POINTER(arg[0]);
	qboolean fromwad = arg[1];
	int i;

	qpic_t *pic;

	for (i = 0; i < pluginimagearraylen; i++)
	{
		if (!pluginimagearray[i].plugin)
			break;
		if (pluginimagearray[i].plugin == currentplug)
		{
			if (!strcmp(name, pluginimagearray[i].name))
				break;
		}
	}
	if (i == pluginimagearraylen)
	{
		pluginimagearraylen++;
		pluginimagearray = BZ_Realloc(pluginimagearray, pluginimagearraylen*sizeof(pluginimagearray_t));
	}

	if (qrenderer)
	{
		if (fromwad)
			pic = Draw_SafePicFromWad(name);
		else
		{
#ifdef RGLQUAKE	//GL saves images persistantly (so don't bother with cachepic stuff)
			if (qrenderer == QR_OPENGL)
				pic = Draw_SafeCachePic(name);
			else
#endif
				pic = NULL;
		}
	}
	else
		pic = NULL;

	Q_strncpyz(pluginimagearray[i].name, name, sizeof(pluginimagearray[i].name));
	pluginimagearray[i].picfromwad = fromwad;
	pluginimagearray[i].pic = pic;
	pluginimagearray[i].plugin = currentplug;
	return i;
}

void Plug_DrawReloadImages(void)
{
	int i;
	for (i = 0; i < pluginimagearraylen; i++)
	{
		if (!pluginimagearray[i].plugin)
			continue;

		if (pluginimagearray[i].picfromwad)
			pluginimagearray[i].pic = Draw_SafePicFromWad(pluginimagearray[i].name);
#ifdef RGLQUAKE
		else if (qrenderer == QR_OPENGL)
				pluginimagearray[i].pic = Draw_SafeCachePic(pluginimagearray[i].name);
#endif
		else
			pluginimagearray[i].pic = NULL;
	}
}

void GLDraw_Image(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qpic_t *pic);
void SWDraw_Image (float xp, float yp, float wp, float hp, float s1, float t1, float s2, float t2, qpic_t *pic);
//int Draw_Image (float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t image)
int Plug_Draw_Image(void *offset, unsigned int mask, const long *arg)
{
	qpic_t *pic;
	int i;
	if (!qrenderer)
		return 0;

	i = VM_LONG(arg[8]);
	if (i < 0 || i >= pluginimagearraylen)
		return -1;	// you fool
	if (pluginimagearray[i].plugin != currentplug)
		return -1;

	if (pluginimagearray[i].pic)
		pic = pluginimagearray[i].pic;
	else if (pluginimagearray[i].picfromwad)
		return 0;	//wasn't loaded.
	else
		pic = Draw_CachePic(pluginimagearray[i].name);

	switch (qrenderer)
	{
#ifdef RGLQUAKE
	case QR_OPENGL:
		glEnable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_ALPHA_TEST);
		GLDraw_Image(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]), VM_FLOAT(arg[2]), VM_FLOAT(arg[3]), VM_FLOAT(arg[4]), VM_FLOAT(arg[5]), VM_FLOAT(arg[6]), VM_FLOAT(arg[7]), pic);
		break;
#endif
#ifdef SWQUAKE
	case QR_SOFTWARE:
		SWDraw_Image(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]), VM_FLOAT(arg[2]), VM_FLOAT(arg[3]), VM_FLOAT(arg[4]), VM_FLOAT(arg[5]), VM_FLOAT(arg[6]), VM_FLOAT(arg[7]), pic);
		break;
#endif
	default:
		break;
	}

	return 1;
}

int Plug_Draw_Character(void *offset, unsigned int mask, const long *arg)
{
	Draw_Character(arg[0], arg[1], (unsigned int)arg[2]);
	return 0;
}

int Plug_Draw_Fill(void *offset, unsigned int mask, const long *arg)
{
	float x, y, width, height;
	x = VM_FLOAT(arg[0]);
	y = VM_FLOAT(arg[1]);
	width = VM_FLOAT(arg[2]);
	height = VM_FLOAT(arg[3]);
	switch(qrenderer)
	{
#ifdef RGLQUAKE
	case QR_OPENGL:
		glDisable(GL_TEXTURE_2D);
		glBegin(GL_QUADS);
		glVertex2f(x, y);
		glVertex2f(x+width, y);
		glVertex2f(x+width, y+height);
		glVertex2f(x, y+height);
		glEnd();
		glEnable(GL_TEXTURE_2D);
		return 1;
#endif
	}
	return 0;
}

//hrm.... FIXME!
int Plug_Draw_ColourP(void *offset, unsigned int mask, const long *arg)
{
	qbyte *pal = host_basepal + VM_LONG(arg[0])*3;

	if (arg[0]<0 || arg[0]>255)
		return false;

	switch(qrenderer)
	{
#ifdef RGLQUAKE
	case QR_OPENGL:
		glColor3f(pal[0]/255.0f, pal[1]/255.0f, pal[2]/255.0f);
		break;
#endif
#ifdef SWQUAKE
	case QR_SOFTWARE:
		SWDraw_ImageColours(pal[0]/255.0f, pal[1]/255.0f, pal[2]/255.0f, 1);
		break;
#endif
	default:
		return 0;
	}
	return 1;
}

int Plug_Draw_Colour3f(void *offset, unsigned int mask, const long *arg)
{
	switch(qrenderer)
	{
#ifdef RGLQUAKE
	case QR_OPENGL:
		glColor3f(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]), VM_FLOAT(arg[2]));
		break;
#endif
#ifdef SWQUAKE
	case QR_SOFTWARE:
		SWDraw_ImageColours(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]), VM_FLOAT(arg[2]), 1);
		break;
#endif
	default:
		return 0;
	}
	return 1;
}
int Plug_Draw_Colour4f(void *offset, unsigned int mask, const long *arg)
{
	switch(qrenderer)
	{
#ifdef RGLQUAKE
	case QR_OPENGL:
		glColor4f(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]), VM_FLOAT(arg[2]), VM_FLOAT(arg[3]));
		break;
#endif
#ifdef SWQUAKE
	case QR_SOFTWARE:
		SWDraw_ImageColours(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]), VM_FLOAT(arg[2]), VM_FLOAT(arg[3]));
		break;
#endif
	default:
		return 0;
	}
	return 1;
}

int Plug_Key_GetKeyCode(void *offset, unsigned int mask, const long *arg)
{
	int modifier;
	return Key_StringToKeynum(VM_POINTER(arg[0]), &modifier);
}

//void Cvar_SetString (char *name, char *value);
int Plug_Cvar_SetString(void *offset, unsigned int mask, const long *arg)
{
	char *name = VM_POINTER(arg[0]),
		*value = VM_POINTER(arg[1]);
	cvar_t *var = Cvar_Get(name, value, 0, "Plugin vars");
	if (var)
	{
		Cvar_Set(var, value);
		return 1;
	}

	return 0;
}

//void Cvar_SetFloat (char *name, float value);
int Plug_Cvar_SetFloat(void *offset, unsigned int mask, const long *arg)
{
	char *name = VM_POINTER(arg[0]);
	float value = VM_FLOAT(arg[1]);
	cvar_t *var = Cvar_Get(name, "", 0, "Plugin vars");	//"" because I'm lazy
	if (var)
	{
		Cvar_SetValue(var, value);
		return 1;
	}

	return 0;
}

//void Cvar_GetFloat (char *name);
int Plug_Cvar_GetFloat(void *offset, unsigned int mask, const long *arg)
{
	char *name = VM_POINTER(arg[0]);
	float value = VM_FLOAT(arg[1]);
	int ret;
	cvar_t *var = Cvar_Get(name, "", 0, "Plugin vars");
	if (var)
	{
		VM_FLOAT(ret) = var->value;
	}
	else
		VM_FLOAT(ret) = 0;
	return ret;
}

//qboolean Cvar_GetString (char *name, char *retstring, int sizeofretstring);
int Plug_Cvar_GetString(void *offset, unsigned int mask, const long *arg)
{
	char *name, *ret;
	int retsize;
	cvar_t *var;
	if (VM_OOB(arg[1], arg[2]))
	{
		return false;
	}

	name = VM_POINTER(arg[0]);
	ret = VM_POINTER(arg[1]);
	retsize = VM_LONG(arg[2]);


	var = Cvar_Get(name, "", 0, "Plugin vars");
	if (strlen(var->name)+1 > retsize)
		return false;

	strcpy(ret, var->string);

	return true;
}

//void Cmd_AddText (char *text, qboolean insert);	//abort the entire engine.
int Plug_Cmd_AddText(void *offset, unsigned int mask, const long *arg)
{
	if (VM_LONG(arg[1]))
		Cbuf_InsertText(VM_POINTER(arg[0]), RESTRICT_LOCAL);
	else
		Cbuf_AddText(VM_POINTER(arg[0]), RESTRICT_LOCAL);

	return 1;
}

int Plug_CL_GetStats(void *offset, unsigned int mask, const long *arg)
{
	int i;
	int pnum = VM_LONG(arg[0]);
	unsigned int *stats = VM_POINTER(arg[1]);
	int pluginstats = VM_LONG(arg[2]);
	int max;

	if (VM_OOB(arg[1], arg[2]*4))
		return 0;

	max = pluginstats;
	if (max > MAX_CL_STATS)
		max = MAX_CL_STATS;
	for (i = 0; i < max; i++)
	{	//fill stats with the right player's stats
		stats[i] = cl.stats[pnum][i];
	}
	for (; i < pluginstats; i++)	//plugin has too many stats (wow)
		stats[i] = 0;					//fill the rest.
	return max;
}

void Plug_Init(void)
{
	Plug_RegisterBuiltin("Plug_GetEngineFunction",	Plug_FindBuiltin, 0);//plugin wishes to find a builtin number.
	Plug_RegisterBuiltin("Plug_ExportToEngine",		Plug_ExportToEngine, 0);	//plugin has a call back that we might be interested in.
	Plug_RegisterBuiltin("Con_Print",				Plug_Con_Print, 0);	//printf is not possible - qvm floats are never doubles, vararg floats in a cdecl call are always converted to doubles.
	Plug_RegisterBuiltin("Sys_Error",				Plug_Sys_Error, 0);
	Plug_RegisterBuiltin("Com_Error",				Plug_Sys_Error, 0);	//make zquake programmers happy.

	Plug_RegisterBuiltin("Cmd_Args",				Plug_Cmd_Args, 0);
	Plug_RegisterBuiltin("Cmd_Argc",				Plug_Cmd_Argc, 0);
	Plug_RegisterBuiltin("Cmd_Argv",				Plug_Cmd_Argv, 0);
	Plug_RegisterBuiltin("Cmd_AddText",				Plug_Cmd_AddText, 0);

	Plug_RegisterBuiltin("CL_GetStats",				Plug_CL_GetStats, 0);
	Plug_RegisterBuiltin("Menu_Control",			Plug_Menu_Control, 0);
	Plug_RegisterBuiltin("Key_GetKeyCode",			Plug_Key_GetKeyCode, 0);

	Plug_RegisterBuiltin("Cvar_Register",			Plug_Cvar_Register, 0);
	Plug_RegisterBuiltin("Cvar_SetString",			Plug_Cvar_SetString, 0);
	Plug_RegisterBuiltin("Cvar_SetFloat",			Plug_Cvar_SetFloat, 0);
	Plug_RegisterBuiltin("Cvar_GetString",			Plug_Cvar_GetString, 0);
	Plug_RegisterBuiltin("Cvar_GetFloat",			Plug_Cvar_GetFloat, 0);

	Plug_RegisterBuiltin("Draw_LoadImage",			Plug_Draw_LoadImage, 0);
	Plug_RegisterBuiltin("Draw_Image",				Plug_Draw_Image, 0);

	Plug_RegisterBuiltin("Draw_Character",			Plug_Draw_Character, 0);
	Plug_RegisterBuiltin("Draw_Fill",				Plug_Draw_Fill, 0);
	Plug_RegisterBuiltin("Draw_Colourp",			Plug_Draw_ColourP, 0);
	Plug_RegisterBuiltin("Draw_Colour3f",			Plug_Draw_Colour3f, 0);
	Plug_RegisterBuiltin("Draw_Colour4f",			Plug_Draw_Colour4f, 0);

#ifdef _WIN32
	COM_EnumerateFiles("plugins/*x86.dll",	Plug_Emumerated, "x86.dll");
#elif defined(__linux__)
	COM_EnumerateFiles("plugins/*x86.so",	Plug_Emumerated, "x86.so");
#endif
	COM_EnumerateFiles("plugins/*.qvm",		Plug_Emumerated, ".qvm");
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

void Plug_ResChanged(void)
{
	plugin_t *oldplug = currentplug;
	for (currentplug = plugs; currentplug; currentplug = currentplug->next)
	{
		if (currentplug->reschange)
			VM_Call(currentplug->vm, currentplug->reschange, vid.width, vid.height);
	}
	currentplug = oldplug;
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
	extern int mousecursor_x, mousecursor_y;

	if (!menuplug)
		return false;

	return VM_Call(menuplug->vm, menuplug->menufunction, eventtype, param, mousecursor_x, mousecursor_y);
}

void Plug_SBar(void)
{
	plugin_t *oc=currentplug;
	int cp;
	vrect_t rect;

	for (currentplug = plugs; currentplug; currentplug = currentplug->next)
	{
		if (currentplug->sbarlevel[0])
		{
			for (cp = 0; cp < cl.splitclients; cp++)
			{	//if you don't use splitscreen, use a full videosize rect.
				SCR_VRectForPlayer(&rect, cp);
				VM_Call(currentplug->vm, currentplug->sbarlevel[0], cp, rect.x, rect.y, rect.width, rect.height);
			}
			break;
		}
	}
	if (!currentplug)
	{
		Sbar_Draw();
		return;	//our current sbar draws a scoreboard too. We don't want that bug to be quite so apparent.
				//please don't implement this identical hack in your engines...
	}

	for (currentplug = plugs; currentplug; currentplug = currentplug->next)
	{
		if (currentplug->sbarlevel[1])
		{
			for (cp = 0; cp < cl.splitclients; cp++)
			{	//if you don't use splitscreen, use a full videosize rect.
				SCR_VRectForPlayer(&rect, cp);
				VM_Call(currentplug->vm, currentplug->sbarlevel[1], cp, rect.x, rect.y, rect.width, rect.height);
			}
		}
	}

	for (currentplug = plugs; currentplug; currentplug = currentplug->next)
	{
		if (currentplug->sbarlevel[2])
		{
			for (cp = 0; cp < cl.splitclients; cp++)
			{	//if you don't use splitscreen, use a full videosize rect.
				SCR_VRectForPlayer(&rect, cp);
				VM_Call(currentplug->vm, currentplug->sbarlevel[2], cp, rect.x, rect.y, rect.width, rect.height);
			}
		}
	}


	currentplug = oc;
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

#endif
