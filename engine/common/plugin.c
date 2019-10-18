//This file should be easily portable.
//The biggest strength of this plugin system is that ALL interactions are performed via
//named functions, this makes it *really* easy to port plugins from one engine to annother.

#include "quakedef.h"
#include "fs.h"
#define FTEENGINE
#include "../plugins/plugin.h"

#ifdef PLUGINS

cvar_t plug_sbar = CVARD("plug_sbar", "3", "Controls whether plugins are allowed to draw the hud, rather than the engine (when allowed by csqc). This is typically used to permit the ezhud plugin without needing to bother unloading it.\n=0: never use hud plugins.\n&1: Use hud plugins in deathmatch.\n&2: Use hud plugins in singleplayer/coop.\n=3: Always use hud plugins (when loaded).");
cvar_t plug_loaddefault = CVARD("plug_loaddefault", "1", "0: Load plugins only via explicit plug_load commands\n1: Load built-in plugins and those selected via the package manager\n2: Scan for misc plugins, loading all that can be found, but not built-ins.\n3: Scan for plugins, and then load any built-ins");

static struct
{
	const char *name;
	qboolean (QDECL *initfunction)(void);
} staticplugins[] = 
{
#if defined(USE_INTERNAL_BULLET)
	{"bullet_internal", Plug_Bullet_Init},
#endif
#if defined(USE_INTERNAL_ODE)
	{"ODE_internal", Plug_ODE_Init},
#endif
	{NULL}
};
//for internal plugins to link against
plugcorefuncs_t *plugfuncs;
plugcvarfuncs_t *cvarfuncs;
plugcmdfuncs_t *cmdfuncs;

#ifdef GLQUAKE
#include "glquake.h"
#endif
#include "com_mesh.h"
#include "shader.h"

static void *QDECL PlugBI_GetEngineInterface(const char *interfacename, size_t structsize);

#ifdef SKELETALMODELS
static int QDECL Plug_RegisterModelFormatText(const char *formatname, char *magictext, qboolean (QDECL *load) (struct model_s *mod, void *buffer, size_t fsize))
{
	void *module = currentplug;
	return Mod_RegisterModelFormatText(module, formatname, magictext, load);
}
static int QDECL Plug_RegisterModelFormatMagic(const char *formatname, unsigned int magic, qboolean (QDECL *load) (struct model_s *mod, void *buffer, size_t fsize))
{
	void *module = currentplug;
	return Mod_RegisterModelFormatMagic(module, formatname, magic, load);
}
static void QDECL Plug_UnRegisterModelFormat(int idx)
{
	void *module = currentplug;
	Mod_UnRegisterModelFormat(module, idx);
}
static void QDECL Plug_UnRegisterAllModelFormats(void)
{
	void *module = currentplug;
	Mod_UnRegisterAllModelFormats(module);
}
#endif

//custom plugin builtins.
void Plug_RegisterBuiltin_(char *name, funcptr_t bi, int flags);
#define Plug_RegisterBuiltin(n,bi,fl) Plug_RegisterBuiltin_(n,(funcptr_t)bi,fl)
#define PLUG_BIF_NEEDSRENDERER 4

#include "netinc.h"

typedef struct plugin_s {
	char *name;
	char filename[MAX_OSPATH];
	dllhandle_t *lib;

	void (QDECL *tick)(double realtime, double gametime);
	qboolean (QDECL *executestring)(qboolean isinsecure);
#ifndef SERVERONLY
	qboolean (QDECL *consolelink)(void);
	qboolean (QDECL *consolelinkmouseover)(float x, float y);
	int (QDECL *conexecutecommand)(qboolean isinsecure);
	qboolean (QDECL *menufunction)(int eventtype, int keyparam, int unicodeparm, float mousecursor_x, float mousecursor_y, float vidwidth, float vidheight);
	int (QDECL *sbarlevel[3])(int seat, float x, float y, float w, float h, unsigned int showscores);	//0 - main sbar, 1 - supplementry sbar sections (make sure these can be switched off), 2 - overlays (scoreboard). menus kill all.
	void (QDECL *reschange)(int width, int height);

	//protocol-in-a-plugin
	int (QDECL *connectionlessclientpacket)(const char *buffer, size_t size, netadr_t *from);
#endif
	qboolean (QDECL *svmsgfunction)(int messagelevel);
	qboolean (QDECL *chatmsgfunction)(int talkernum, int tpflags);
	qboolean (QDECL *centerprintfunction)(int clientnum);
	qboolean (QDECL *mayshutdown)(void);	//lets the plugin report when its safe to close it.
	void (QDECL *shutdown)(void);

	struct plugin_s *next;
} plugin_t;

int Plug_SubConsoleCommand(console_t *con, const char *line);

plugin_t *currentplug;



#ifndef SERVERONLY
#include "cl_plugin.inc"
#else
void Plug_Client_Init(void){}
void Plug_Client_Close(plugin_t *plug) {}
#endif

void Plug_Close(plugin_t *plug);


static plugin_t *plugs;

#ifdef USERBE
#include "pr_common.h"
#endif

static char *Plug_CleanName(const char *file, char *out, size_t sizeof_out)
{
	size_t len;
	//"fteplug_ezhud_x86.REV.dll" gets converted into "ezhud"

	//skip fteplug_
	if (!Q_strncasecmp(file, PLUGINPREFIX, strlen(PLUGINPREFIX)))
		file += strlen(PLUGINPREFIX);

	//strip .REV.dll
	COM_StripAllExtensions(file, out, sizeof_out);

	//strip _x86
	len = strlen(out);
	if (len > strlen("_"ARCH_CPU_POSTFIX) && !Q_strncasecmp(out+len-strlen("_"ARCH_CPU_POSTFIX), "_"ARCH_CPU_POSTFIX, strlen("_"ARCH_CPU_POSTFIX)))
	{
		len -= strlen("_"ARCH_CPU_POSTFIX);
		out[len] = 0;
	}
	return out;
}
static plugin_t *Plug_Load(const char *file)
{
	static enum fs_relative prefixes[] =
	{
		FS_BINARYPATH,
#ifndef ANDROID
		FS_ROOT,
#endif
	};
	static char *postfixes[] =
	{
		"_" ARCH_CPU_POSTFIX ARCH_DL_POSTFIX,
#ifndef ANDROID
		ARCH_DL_POSTFIX,
#endif
	};
	size_t i, j;
	char temp[MAX_OSPATH];
	plugin_t *newplug;
	int nlen = strlen(file);
	qboolean success = false;
	qboolean (QDECL *initfunction)(plugcorefuncs_t*);
	dllfunction_t funcs[] =
	{
		{(void**)&initfunction, "FTEPlug_Init"},
		{NULL,NULL},
	};

	//reject obviously invalid names
	if (!*file || strchr(file, '/') || strchr(file, '\\'))
		return NULL;

	Plug_CleanName(file, temp, sizeof(temp));
	for (newplug = plugs; newplug; newplug = newplug->next)
	{
		if (!Q_strcasecmp(newplug->name, temp))
			return newplug;
	}

	newplug = Z_Malloc(sizeof(plugin_t)+strlen(temp)+1);
	newplug->name = (char*)(newplug+1);
	strcpy(newplug->name, temp);

	//[basedir|binroot]fteplug_%s[_cpu][.so]
	for (i = 0; i < countof(prefixes) && !newplug->lib; i++)
	{
		//if the name matches a postfix then just go with that
		for (j = 0; j < countof(postfixes); j++)
		{
			int pfl = strlen(postfixes[j]);
			if (nlen > pfl && !Q_strcasecmp(file+nlen-pfl, postfixes[j]))
				break;
		}
		if (j < countof(postfixes))
		{	//already postfixed, don't mess with the name given
			//mandate the fteplug_ prefix (don't let them load random dlls)
			if (!Q_strncasecmp(file, PLUGINPREFIX, strlen(PLUGINPREFIX)))
				if (FS_NativePath(file, prefixes[i], newplug->filename, sizeof(newplug->filename)))
					newplug->lib = Sys_LoadLibrary(newplug->filename, funcs);
		}
		else
		{	//otherwise scan for it
			for (j = 0; j < countof(postfixes) && !newplug->lib; j++)
			{
				if (FS_NativePath(va(PLUGINPREFIX"%s%s", file, postfixes[j]), prefixes[i], newplug->filename, sizeof(newplug->filename)))
					newplug->lib = Sys_LoadLibrary(newplug->filename, funcs);
			}
		}
	}

#ifdef _WIN32
	{
		char *mssuck;
		while ((mssuck=strchr(newplug->filename, '\\')))
			*mssuck = '/';
	}
#endif

	newplug->next = plugs;
	plugs = newplug;
	if (newplug->lib)
	{
		Con_DPrintf("Created plugin %s\n", file);

		currentplug = newplug;
		success = initfunction(PlugBI_GetEngineInterface(plugcorefuncs_name, sizeof(plugcorefuncs_t)));
	}
	else
	{
		unsigned int u;
		for (u = 0; staticplugins[u].name; u++)
		{
			if (!Q_strcasecmp(file, staticplugins[u].name))
			{
				Con_DPrintf("Activated module %s\n", file);
				newplug->lib = NULL;

				currentplug = newplug;
				success = staticplugins[u].initfunction();
				break;
			}
		}
	}

	if (!success)
	{
		Plug_Close(newplug);
		return NULL;
	}


#ifndef SERVERONLY
	if (newplug->reschange)
		newplug->reschange(vid.width, vid.height);
#endif

	currentplug = NULL;

	return newplug;
}
static void Plug_Load_Update(const char *name)
{
	Plug_Load(name);
}

static int QDECL Plug_EnumeratedRoot (const char *name, qofs_t size, time_t mtime, void *param, searchpathfuncs_t *spath)
{
	char vmname[MAX_QPATH];
	int len;
	char *dot;
	if (!strncmp(name, PLUGINPREFIX, strlen(PLUGINPREFIX)))
		name += 8;
	Q_strncpyz(vmname, name, sizeof(vmname));
	len = strlen(vmname);
	len -= strlen(ARCH_CPU_POSTFIX ARCH_DL_POSTFIX);
	if (!strcmp(vmname+len, ARCH_CPU_POSTFIX ARCH_DL_POSTFIX))
		vmname[len] = 0;
	else
	{
		dot = strchr(vmname, '.');
		if (dot)
			*dot = 0;
	}
	len = strlen(vmname);
	if (len > 0 && vmname[len-1] == '_')
		vmname[len-1] = 0;
	if (!Plug_Load(vmname))
		Con_Printf("Couldn't load plugin %s\n", vmname);

	return true;
}

static void QDECL Plug_Con_Print(const char *text)
{
	Con_Printf("%s", text);
}
static void QDECL Plug_Sys_Error(const char *text)
{
	Sys_Error("%s", text);
}
static quintptr_t QDECL Plug_Sys_Milliseconds(void)
{
	return Sys_DoubleTime()*1000u;
}

qboolean VARGS PlugBI_ExportFunction(const char *name, funcptr_t function)
{
	if (!strcmp(name, "Tick"))					//void(int realtime)
		currentplug->tick = function;
	else if (!strcmp(name, "ExecuteCommand"))	//bool(isinsecure)
		currentplug->executestring = function;
	else if (!strcmp(name, "Shutdown"))			//void()
		currentplug->shutdown = function;
	else if (!strcmp(name, "MayShutdown")||!strcmp(name, "MayUnload"))
		currentplug->mayshutdown = function;
#ifdef HAVE_CLIENT
	else if (!strcmp(name, "ConsoleLink"))
		currentplug->consolelink = function;
	else if (!strcmp(name, "ConsoleLinkMouseOver"))
		currentplug->consolelinkmouseover = function;
	else if (!strcmp(name, "ConExecuteCommand"))
		currentplug->conexecutecommand = function;
	else if (!strcmp(name, "MenuEvent"))
		currentplug->menufunction = function;
	else if (!strcmp(name, "UpdateVideo"))
		currentplug->reschange = function;
	else if (!strcmp(name, "SbarBase"))			//basic SBAR.
		currentplug->sbarlevel[0] = function;
	else if (!strcmp(name, "SbarSupplement"))	//supplementry stuff - teamplay
		currentplug->sbarlevel[1] = function;
	else if (!strcmp(name, "SbarOverlay"))		//overlay - scoreboard type stuff.
		currentplug->sbarlevel[2] = function;
	else if (!strcmp(name, "ConnectionlessClientPacket"))
		currentplug->connectionlessclientpacket = function;
	else if (!strcmp(name, "ServerMessageEvent"))
		currentplug->svmsgfunction = function;
	else if (!strcmp(name, "ChatMessageEvent"))
		currentplug->chatmsgfunction = function;
	else if (!strcmp(name, "CenterPrintMessage"))
		currentplug->centerprintfunction = function;
	else if (!strcmp(name, "S_LoadSound"))	//a hook for loading extra types of sound (wav, mp3, ogg, midi, whatever you choose to support)
		S_RegisterSoundInputPlugin(currentplug, function);
#endif
	else if (!strncmp(name, "FS_RegisterArchiveType_", 23))	//module as in pak/pk3
		FS_RegisterFileSystemType(currentplug, name+23, function, true);
	else
		return 0;
	return 1;
}

//retrieve a plugin's name
static qboolean QDECL PlugBI_GetPluginName(int plugnum, char *outname, size_t namesize)
{
	plugin_t *plug;
	//int plugnum (0 for current), char *buffer, int bufferlen

	if (plugnum <= 0)
	{
		if (!currentplug)
			return false;
		else if (plugnum == -1 && currentplug->lib)	//plugin file name
			Q_strncpyz(outname, currentplug->filename, namesize);
		else	//plugin name
			Q_strncpyz(outname, currentplug->name, namesize);
		return true;
	}

	for (plug = plugs; plug; plug = plug->next)
	{
		if (--plugnum == 0)
		{
			Q_strncpyz(outname, plug->name, namesize);
			return true;
		}
	}
	return false;
}

static qboolean QDECL PlugBI_ExportInterface(const char *name, void *interfaceptr, size_t structsize)
{
	if (0)
		;
	/*
	else if (!strncmp(name, "VID_DisplayDriver"))	//a video driver, loaded by name as given by vid_renderer
	{
		FS_RegisterModuleDriver(, func);
		currentplug->blockcloses++;
	}
	*/
#if defined(PLUGINS) && !defined(SERVERONLY)
#ifdef HAVE_MEDIA_DECODER
	else if (!strcmp(name, "Media_VideoDecoder"))
		Media_RegisterDecoder(currentplug, interfaceptr);
#endif
#ifdef HAVE_MEDIA_ENCODER
	else if (!strcmp(name, "Media_VideoEncoder"))
		Media_RegisterEncoder(currentplug, interfaceptr);
#endif
#endif
	else
		return 0;
	return 1;
}

static cvar_t *QDECL Plug_Cvar_GetNVFDG(const char *name, const char *defaultvalue, unsigned int flags, const char *description, const char *groupname)
{
	if (!defaultvalue)
		return Cvar_FindVar(name);
	return Cvar_Get2(name, defaultvalue, flags&1, description, groupname);
}


typedef struct {
	//Make SURE that the engine has resolved all cvar pointers into globals before this happens.
	plugin_t *plugin;
	cvar_t *var;
} plugincvararray_t;
static int plugincvararraylen;
static plugincvararray_t *plugincvararray;
//qhandle_t Cvar_Register (char *name, char *defaultval, int flags, char *grouphint);
static qhandle_t QDECL Plug_Cvar_Register(const char *name, const char *defaultvalue, int flags, const char *groupname)
{
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

	i = plugincvararraylen;
	plugincvararraylen++;
	plugincvararray = BZ_Realloc(plugincvararray, (plugincvararraylen)*sizeof(plugincvararray_t));
	plugincvararray[i].plugin = currentplug;
	plugincvararray[i].var = var;
	return i;
}
//int Cvar_Update, (qhandle_t handle, int modificationcount, char *stringv, float *floatv));	//stringv is 256 chars long, don't expect this function to do anything if modification count is unchanged.
static qboolean QDECL Plug_Cvar_Update(qhandle_t handle, int *modificationcount, char *outstringv, size_t stringsize, float *outfloatv)
{
	cvar_t *var;
	if (handle < 0 || handle >= plugincvararraylen)
		return false;
	if (plugincvararray[handle].plugin != currentplug)
		return false;	//I'm not letting you know what annother plugin has registered.

	var = plugincvararray[handle].var;

	//if (var->modified != *modificationcount)	//for future optimisation
	{
		//*modificationcount = var->modified;
		Q_strncpyz(outstringv, var->string, stringsize);
		*outfloatv = var->value;
		return true;
	}
	return false;
}

//void Cmd_Args(char *buffer, int buffersize)
static void QDECL Plug_Cmd_Args(char *buffer, int maxsize)
{
	char *args;
	args = Cmd_Args();
	if (strlen(args)+1>maxsize)
	{
		if (maxsize)
			*buffer = 0;
	}
	else
		strcpy(buffer, args);
}
//void Cmd_Argv(int num, char *buffer, int buffersize)
static void QDECL Plug_Cmd_Argv(int argn, char *outbuffer, size_t buffersize)
{
	char *args;
	args = Cmd_Argv(argn);
	
	if (strlen(args)+1>buffersize)
	{
		if (buffersize)
			*outbuffer = 0;
	}
	else
		strcpy(outbuffer, args);
}
//int Cmd_Argc(void)
static int QDECL Plug_Cmd_Argc(void)
{
	return Cmd_Argc();
}

//void Cvar_SetString (char *name, char *value);
static void QDECL Plug_Cvar_SetString(const char *name, const char *value)
{
	cvar_t *var = Cvar_Get(name, value, 0, "Plugin vars");
	if (var)
		Cvar_Set(var, value);
}

//void Cvar_SetFloat (char *name, float value);
static void QDECL Plug_Cvar_SetFloat(const char *cvarname, float newvalue)
{
	cvar_t *var = Cvar_Get(cvarname, "", 0, "Plugin vars");	//"" because I'm lazy
	if (var)
		Cvar_SetValue(var, newvalue);
}

//void Cvar_GetFloat (char *name);
static float QDECL Plug_Cvar_GetFloat(const char *cvarname)
{
	int ret;
	cvar_t *var;
#ifndef CLIENTONLY
	if (!strcmp(cvarname, "sv.state"))	//ugly hack
		return sv.state;
	else
#endif
	{
		var = Cvar_Get(cvarname, "", 0, "Plugin vars");
		if (var)
			return var->value;
		else
			return 0;
	}
	return ret;
}

//qboolean Cvar_GetString (char *name, char *retstring, int sizeofretstring);
static qboolean QDECL Plug_Cvar_GetString(const char *name, char *outbuffer, quintptr_t sizeofbuffer)
{
	cvar_t *var;

	if (!strcmp(name, "sv.mapname"))
	{
#ifdef CLIENTONLY
		Q_strncpyz(outbuffer, "", sizeofbuffer);
#else
		Q_strncpyz(outbuffer, svs.name, sizeofbuffer);
#endif
	}
	else
	{
		var = Cvar_Get(name, "", 0, "Plugin vars");
		if (strlen(var->name)+1 > sizeofbuffer)
			return false;

		strcpy(outbuffer, var->string);
	}

	return true;
}

//void Cmd_AddText (char *text, qboolean insert);	//abort the entire engine.
static void QDECL Plug_Cmd_AddText(const char *text, qboolean insert)
{
	int level = RESTRICT_LOCAL;
	if (insert)
		Cbuf_InsertText(text, level, false);
	else
		Cbuf_AddText(text, level);
}

static int plugincommandarraylen;
typedef struct {
	plugin_t *plugin;
	char command[64];
} plugincommand_t;
static plugincommand_t *plugincommandarray;
void Plug_Command_f(void)
{
	int i;
	char *cmd = Cmd_Argv(0);
	plugin_t *oldplug = currentplug;
	for (i = 0; i < plugincommandarraylen; i++)
	{
		if (!plugincommandarray[i].plugin)
			continue;	//don't check commands who's owners died.

		if (Q_strcasecmp(plugincommandarray[i].command, cmd))	//not the right command
			continue;

		currentplug = plugincommandarray[i].plugin;

		if (currentplug->executestring)
			currentplug->executestring(Cmd_IsInsecure());
		break;
	}

	currentplug = oldplug;
}

static qboolean QDECL Plug_Cmd_AddCommand(const char *name)
{
	int i;
	for (i = 0; i < plugincommandarraylen; i++)
	{
		if (!plugincommandarray[i].plugin)
			break;
		if (plugincommandarray[i].plugin == currentplug)
		{
			if (!strcmp(name, plugincommandarray[i].command))
				return true;	//already registered
		}
	}
	if (i == plugincommandarraylen)
	{
		plugincommandarraylen++;
		plugincommandarray = BZ_Realloc(plugincommandarray, plugincommandarraylen*sizeof(plugincommand_t));
	}

	Q_strncpyz(plugincommandarray[i].command, name, sizeof(plugincommandarray[i].command));
	if (!Cmd_AddCommand(plugincommandarray[i].command, Plug_Command_f))
		return false;
	plugincommandarray[i].plugin = currentplug;	//worked
	return true;
}
static void Plug_FreeConCommands(plugin_t *plug)
{
	int i;
	for (i = 0; i < plugincommandarraylen; i++)
	{
		if (plugincommandarray[i].plugin == plug)
		{
			plugincommandarray[i].plugin = NULL;
			Cmd_RemoveCommand(plugincommandarray[i].command);
		}
	}
}

typedef enum{
	STREAM_NONE,
	STREAM_SOCKET,
	STREAM_VFS,
	STREAM_WEB,
} plugstream_e;

typedef struct {
	plugin_t *plugin;
	plugstream_e type;
	int socket;
	vfsfile_t *vfs;
	struct dl_download *dl;
	struct {
		char filename[MAX_QPATH];
//		qbyte *buffer;
//		int buflen;
//		int curlen;
//		int curpos;
	} file;
} pluginstream_t;
static pluginstream_t *pluginstreamarray;
static unsigned int pluginstreamarraylen;

static int Plug_NewStreamHandle(plugstream_e type)
{
	int i;
	for (i = 0; i < pluginstreamarraylen; i++)
	{
		if (!pluginstreamarray[i].plugin)
			break;
	}
	if (i >= pluginstreamarraylen)
	{
		pluginstreamarraylen=i+16;
		pluginstreamarray = BZ_Realloc(pluginstreamarray, pluginstreamarraylen*sizeof(pluginstream_t));
	}

	memset(&pluginstreamarray[i], 0, sizeof(pluginstream_t));
	pluginstreamarray[i].plugin = currentplug;
	pluginstreamarray[i].type = type;
	pluginstreamarray[i].socket = -1;
	*pluginstreamarray[i].file.filename = '\0';

	return i;
}

#ifdef HAVE_CLIENT
static qhandle_t QDECL Plug_Con_POpen(const char *consolename)
{
	int handle;
	if (!currentplug)
		return -3;	//streams depend upon current plugin context. which isn't valid in a thread.
	handle = Plug_NewStreamHandle(STREAM_VFS);
	pluginstreamarray[handle].vfs = Con_POpen(consolename);
	return handle;
}
#endif

#ifdef HAVE_PACKET
//EBUILTIN(int, NET_TCPListen, (char *ip, int port, int maxcount));
//returns a new socket with listen enabled.
static qhandle_t QDECL Plug_Net_TCPListen(const char *localip, int localport, int maxcount)
{
	int handle;
	int sock;
	struct sockaddr_qstorage address;
	int _true = 1;
	int alen;

	netadr_t a;
	if (!currentplug)
		return -3;	//streams depend upon current plugin context. which isn't valid in a thread.
	if (!localip)
		localip = "tcp://0.0.0.0";	//pass "[::]" for ipv6

	if (!NET_StringToAdr(localip, localport, &a))
		return -1;
	if (a.prot != NP_STREAM && a.prot != NP_DGRAM)
		return -1;
	alen = NetadrToSockadr(&a, &address);

	if ((sock = socket(((struct sockaddr*)&address)->sa_family, SOCK_STREAM, 0)) == -1)
	{
		Con_Printf("Failed to create socket\n");
		return -2;
	}
	if (ioctlsocket (sock, FIONBIO, (u_long *)&_true) == -1)
	{
		closesocket(sock);
		return -2;
	}
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&_true, sizeof(_true));

	if (bind (sock, (void *)&address, alen) == -1)
	{
		closesocket(sock);
		return -2;
	}
	if (listen (sock, maxcount) == -1)
	{
		closesocket(sock);
		return -2;
	}

	handle = Plug_NewStreamHandle(STREAM_SOCKET);
	pluginstreamarray[handle].socket = sock;

	return handle;

}

static qhandle_t QDECL Plug_Net_Accept(qhandle_t handle, char *outaddress, int outaddresssize)
{
	struct sockaddr_qstorage address;
	int addrlen;
	int sock;
	int _true = 1;
	char adr[MAX_ADR_SIZE];

	if (!currentplug || handle < 0 || handle >= pluginstreamarraylen || pluginstreamarray[handle].plugin != currentplug || pluginstreamarray[handle].type != STREAM_SOCKET)
		return -2;
	sock = pluginstreamarray[handle].socket;

	if (sock < 0)
		return -1;

	addrlen = sizeof(address);
	sock = accept(sock, (struct sockaddr *)&address, &addrlen);
	if (sock < 0)
		return -1;

	if (ioctlsocket (sock, FIONBIO, (u_long *)&_true) == -1)	//now make it non blocking.
	{
		closesocket(sock);
		return -1;
	}

	if (outaddresssize)
	{
		netadr_t a;
		char *s;
		SockadrToNetadr(&address, addrlen, &a);
		s = NET_AdrToString(adr, sizeof(adr), &a);
		Q_strncpyz(outaddress, s, addrlen);
	}

	handle = Plug_NewStreamHandle(STREAM_SOCKET);
	pluginstreamarray[handle].socket = sock;

	return handle;
}

static qhandle_t QDECL Plug_Net_TCPConnect(const char *remoteip, int remoteport)
{
	int handle;
	vfsfile_t *stream = FS_OpenTCP(remoteip, remoteport);
	if (!currentplug || !stream)
		return -1;
	handle = Plug_NewStreamHandle(STREAM_VFS);
	pluginstreamarray[handle].vfs = stream;
	return handle;

}


void Plug_Net_Close_Internal(qhandle_t handle);
static int QDECL Plug_Net_SetTLSClient(qhandle_t handle, const char *certhostname)
{
#ifndef HAVE_SSL
	return -1;
#else
	pluginstream_t *stream;
	if (handle >= pluginstreamarraylen || pluginstreamarray[handle].plugin != currentplug)
	{
		Con_Printf("Plug_Net_SetTLSClient: socket does not belong to you (or is invalid)\n");
		return -2;
	}
	stream = &pluginstreamarray[handle];
	if (stream->type != STREAM_VFS)
	{	//not a socket - invalid
		Con_Printf("Plug_Net_SetTLSClient: Not a socket handle\n");
		return -2;
	}

	stream->vfs = FS_OpenSSL(certhostname, stream->vfs, false);
	if (!stream->vfs)
	{
		Plug_Net_Close_Internal(handle);
		return -1;
	}
	return 0;
#endif
}

static int QDECL Plug_Net_GetTLSBinding(qhandle_t handle, char *outbinddata, int *outbinddatalen)
{
#ifndef HAVE_SSL
	return -1;
#else
	pluginstream_t *stream;
	size_t sz;
	int r;
	if ((size_t)handle >= pluginstreamarraylen || pluginstreamarray[handle].plugin != currentplug)
	{
		Con_Printf("Plug_Net_GetTLSBinding: socket does not belong to you (or is invalid)\n");
		return -2;
	}
	stream = &pluginstreamarray[handle];
	if (stream->type != STREAM_VFS)
	{	//not a socket - invalid
		Con_Printf("Plug_Net_GetTLSBinding: Not a socket handle\n");
		return -2;
	}

	sz = *outbinddatalen;
	r = TLS_GetChannelBinding(stream->vfs, outbinddata, &sz);
	*outbinddatalen = sz;
	return r;
#endif
}
#endif

#ifdef WEBCLIENT
static void Plug_DownloadComplete(struct dl_download *dl)
{
	int handle = dl->user_num;
	dl->file = NULL;
	pluginstreamarray[handle].dl = NULL;			//download no longer needs to be canceled.
	pluginstreamarray[handle].type = STREAM_VFS;	//getlen should start working now.
}
#endif


static qhandle_t QDECL Plug_FS_Open(const char *fname, qhandle_t *outhandle, int modenum)
{
	//modes:
	//1: read
	//2: write

	//char *name, int *handle, int mode

	//return value is length of the file.

	int handle;
	//char *data;
	char *mode;
	vfsfile_t *f;

	*outhandle = -1;
	if (!currentplug)
		return -3;	//streams depend upon current plugin context. which isn't valid in a thread.

	switch(modenum)
	{
	case 1:
		mode = "rb";
		break;
	case 2:
		mode = "wb";
		break;
	default:
		return -2;
	}
	if (!strcmp(fname, "**plugconfig"))
		f = FS_OpenVFS(va("%s.cfg", currentplug->name), mode, FS_ROOT);
	else if (!strncmp(fname, "http://", 7) || !strncmp(fname, "https://", 8))
	{
#ifndef WEBCLIENT
		f = NULL;
#else
		handle = Plug_NewStreamHandle(STREAM_WEB);
		pluginstreamarray[handle].dl = HTTP_CL_Get(fname, NULL, Plug_DownloadComplete);
		pluginstreamarray[handle].dl->user_num = handle;
		pluginstreamarray[handle].dl->file = pluginstreamarray[handle].vfs = VFSPIPE_Open(2, true);
		pluginstreamarray[handle].dl->isquery = true;
#ifdef MULTITHREAD
		DL_CreateThread(pluginstreamarray[handle].dl, NULL, NULL);
#endif
		*outhandle = handle;
		return VFS_GETLEN(pluginstreamarray[handle].vfs);
#endif
	}
	else if (modenum == 2)
		f = FS_OpenVFS(fname, mode, FS_GAMEONLY);
	else
		f = FS_OpenVFS(fname, mode, FS_GAME);
	if (!f)
		return -1;
	handle = Plug_NewStreamHandle(STREAM_VFS);
	pluginstreamarray[handle].vfs = f;
	Q_strncpyz(pluginstreamarray[handle].file.filename, fname, sizeof(pluginstreamarray[handle].file.filename));
	*outhandle = handle;
	return VFS_GETLEN(pluginstreamarray[handle].vfs);
}
static int QDECL Plug_FS_Seek(qhandle_t handle, qofs_t offset)
{
	pluginstream_t *stream;

	if (handle >= pluginstreamarraylen)
		return -1;
	stream = &pluginstreamarray[handle];
	if (stream->type != STREAM_VFS)
		return -1;
	VFS_SEEK(stream->vfs, offset);
	return VFS_TELL(stream->vfs);
}

static qboolean QDECL Plug_FS_GetLength(qhandle_t handle, qofs_t *outsize)
{
	pluginstream_t *stream;

	if (handle >= pluginstreamarraylen)
		return false;
	stream = &pluginstreamarray[handle];
	if (stream->type == STREAM_VFS)
	if (stream->vfs->GetLen)
	{
		*outsize = VFS_GETLEN(stream->vfs);
		return true;
	}
	*outsize = 0;
	return false;
}

void Plug_Net_Close_Internal(qhandle_t handle)
{
	switch(pluginstreamarray[handle].type)
	{
	case STREAM_NONE:
		break;
	case STREAM_WEB:
#ifdef WEBCLIENT
		if (pluginstreamarray[handle].dl)
		{
			pluginstreamarray[handle].dl->file = NULL;
			DL_Close(pluginstreamarray[handle].dl);
		}
#endif
		//fall through
	case STREAM_VFS:
		if (pluginstreamarray[handle].vfs)
		{
			if (*pluginstreamarray[handle].file.filename && pluginstreamarray[handle].vfs->WriteBytes)
			{
				VFS_CLOSE(pluginstreamarray[handle].vfs);
				FS_FlushFSHashWritten(pluginstreamarray[handle].file.filename);
			}
			else
				VFS_CLOSE(pluginstreamarray[handle].vfs);
		}
		pluginstreamarray[handle].vfs = NULL;
		break;
	case STREAM_SOCKET:
#ifdef HAVE_PACKET
		closesocket(pluginstreamarray[handle].socket);
#endif
		break;
	}

	pluginstreamarray[handle].type = STREAM_NONE;
	pluginstreamarray[handle].plugin = NULL;
}

static int QDECL Plug_Net_Recv(qhandle_t handle, void *dest, int destlen)
{
	int read;

	if (handle < 0 || handle >= pluginstreamarraylen || pluginstreamarray[handle].plugin != currentplug)
		return -2;
	switch(pluginstreamarray[handle].type)
	{
#ifdef HAVE_PACKET
	case STREAM_SOCKET:
		read = recv(pluginstreamarray[handle].socket, dest, destlen, 0);
		if (read < 0)
		{
			if (neterrno() == NET_EWOULDBLOCK)
				return -1;
			else
				return -2;
		}
		else if (read == 0)
			return -2;	//closed by remote connection.
		return read;
#endif

	case STREAM_WEB:
	case STREAM_VFS:
		return VFS_READ(pluginstreamarray[handle].vfs, dest, destlen);
	default:
		return -2;
	}
}
static int QDECL Plug_Net_Send(qhandle_t handle, void *src, int srclen)
{
	int written;
	if (handle < 0 || handle >= pluginstreamarraylen || pluginstreamarray[handle].plugin != currentplug)
		return -2;
	switch(pluginstreamarray[handle].type)
	{
#ifdef HAVE_PACKET
	case STREAM_SOCKET:
		written = send(pluginstreamarray[handle].socket, src, srclen, 0);
		if (written < 0)
		{
			if (neterrno() == NET_EWOULDBLOCK)
				return -1;
			else
				return -2;
		}
		else if (written == 0)
			return -2;	//closed by remote connection.
		return written;
#endif

	case STREAM_VFS:
		return VFS_WRITE(pluginstreamarray[handle].vfs, src, srclen);

	default:
		return -2;
	}
}
static int QDECL Plug_Net_SendTo(qhandle_t handle, void *src, int srclen, netadr_t *address)
{
	int written;

	struct sockaddr_qstorage sockaddr;
	if (handle == -1)
	{
#ifdef HAVE_CLIENT
		NET_SendPacket(cls.sockets, srclen, src, address);
		return srclen;
#else
		return -2;
#endif
	}

	NetadrToSockadr(address, &sockaddr);

	if (handle < 0 || handle >= pluginstreamarraylen || pluginstreamarray[handle].plugin != currentplug)
		return -2;
	switch(pluginstreamarray[handle].type)
	{
#ifdef HAVE_PACKET
	case STREAM_SOCKET:
		written = sendto(pluginstreamarray[handle].socket, src, srclen, 0, (struct sockaddr*)&sockaddr, sizeof(sockaddr));
		if (written < 0)
		{
			if (neterrno() == NET_EWOULDBLOCK)
				return -1;
			else
				return -2;
		}
		else if (written == 0)
			return -2;	//closed by remote connection.
		return written;
#endif
	default:
		return -2;
	}
}
static void QDECL Plug_Net_Close(qhandle_t handle)
{
	if (handle < 0 || handle >= pluginstreamarraylen || pluginstreamarray[handle].plugin != currentplug)
		return;
	Plug_Net_Close_Internal(handle);
}

#if defined(HAVE_SERVER) && defined(HAVE_CLIENT)
qboolean FS_PathURLCache(const char *url, char *path, size_t pathsize);
static qboolean QDECL Plug_MapLog_Query(const char *packagename, const char *mapname, float *vals)
{
	if (!strncmp(packagename, "http://", 7) || !strncmp(packagename, "https://", 8))
	{
		char temp[MAX_OSPATH];
		if (!FS_PathURLCache(packagename, temp, sizeof(temp)))
			return false;
		if (Log_CheckMapCompletion(temp, mapname, &vals[0], &vals[1], &vals[2], &vals[3]))
			return true;
		return false;
	}
	return Log_CheckMapCompletion(packagename, mapname, &vals[0], &vals[1], &vals[2], &vals[3]);
}
#endif

void Plug_CloseAll_f(void);
void Plug_List_f(void);
void Plug_Close_f(void);
static void Plug_Load_f(void)
{
	char *plugin;
	plugin = Cmd_Argv(1);
	if (!*plugin)
	{
		Con_Printf("Loads a plugin\n");
		Con_Printf("plug_load [pluginpath]\n");
		Con_Printf("example pluginpath: blah\n");
		Con_Printf("will load "PLUGINPREFIX"blah"ARCH_CPU_POSTFIX ARCH_DL_POSTFIX"\n");
		return;
	}
	if (!Plug_Load(plugin))
		Con_Printf("Couldn't load plugin %s\n", Cmd_Argv(1));
}

void Plug_Initialise(qboolean fromgamedir)
{
	char nat[MAX_OSPATH];

	if (!plugfuncs)
	{
		Cvar_Register(&plug_sbar, "plugins");
		Cvar_Register(&plug_loaddefault, "plugins");

		Cmd_AddCommand("plug_closeall", Plug_CloseAll_f);
		Cmd_AddCommand("plug_close", Plug_Close_f);
		Cmd_AddCommand("plug_load", Plug_Load_f);
		Cmd_AddCommand("plug_list", Plug_List_f);

		//set up internal plugins
		plugfuncs = PlugBI_GetEngineInterface(plugcorefuncs_name, sizeof(*plugfuncs));
		cvarfuncs = plugfuncs->GetEngineInterface(plugcvarfuncs_name, sizeof(*cvarfuncs));
		cmdfuncs = plugfuncs->GetEngineInterface(plugcmdfuncs_name, sizeof(*cmdfuncs));
	}

#ifdef SUBSERVERS
	if (!SSV_IsSubServer())	//subservers will need plug_load I guess
#endif
	if (plug_loaddefault.ival & 2)
	{
		if (!fromgamedir)
		{
			FS_NativePath("", FS_BINARYPATH, nat, sizeof(nat));
			Con_DPrintf("Loading plugins from \"%s\"\n", nat);
			Sys_EnumerateFiles(nat, PLUGINPREFIX"*" ARCH_CPU_POSTFIX ARCH_DL_POSTFIX, Plug_EnumeratedRoot, NULL, NULL);
		}
	}
	if (plug_loaddefault.ival & 1)
	{
		unsigned int u;
		PM_EnumeratePlugins(Plug_Load_Update);
		for (u = 0; staticplugins[u].name; u++)
		{
			Plug_Load(staticplugins[u].name);
		}
	}
}

void Plug_Tick(void)
{
	plugin_t *oldplug = currentplug;
	for (currentplug = plugs; currentplug; currentplug = currentplug->next)
	{
		if (currentplug->tick)
		{
			double st = 0;
#ifdef SERVERONLY 
			st = sv.time;
#elif defined(CLIENTONLY)
			st = cl.time;
#else
			st = sv.state?sv.time:cl.time;
#endif
			currentplug->tick(realtime, st);
		}
	}
	currentplug = oldplug;
}

#ifndef SERVERONLY
void Plug_ResChanged(void)
{
	plugin_t *oldplug = currentplug;
	for (currentplug = plugs; currentplug; currentplug = currentplug->next)
	{
		if (currentplug->reschange)
			currentplug->reschange(vid.width, vid.height);
	}
	currentplug = oldplug;
}
#endif

qboolean Plugin_ExecuteString(void)
{
	plugin_t *oldplug = currentplug;
	if (Cmd_Argc()>0)
	{
		for (currentplug = plugs; currentplug; currentplug = currentplug->next)
		{
			if (currentplug->executestring)
			{
				if (currentplug->executestring(0))
				{
					currentplug = oldplug;
					return true;
				}
			}
		}
	}
	currentplug = oldplug;
	return false;
}

#ifndef SERVERONLY
qboolean Plug_ConsoleLinkMouseOver(float x, float y, char *text, char *info)
{
	qboolean result = false;
	plugin_t *oldplug = currentplug;
	for (currentplug = plugs; currentplug; currentplug = currentplug->next)
	{
		if (currentplug->consolelinkmouseover)
		{
			char buffer[8192];
			char *ptr;
			ptr = (char*)COM_QuotedString(text, buffer, sizeof(buffer)-10, false);
			ptr += strlen(ptr);
			*ptr++ = ' ';
			COM_QuotedString(info, ptr, sizeof(buffer)-(ptr-buffer), false);

			Cmd_TokenizeString(buffer, false, false);
			result = currentplug->consolelinkmouseover(x, y);
			if (result)
				break;
		}
	}
	currentplug = oldplug;
	return result;
}
qboolean Plug_ConsoleLink(char *text, char *info, const char *consolename)
{
	qboolean result = false;
	plugin_t *oldplug = currentplug;
	for (currentplug = plugs; currentplug; currentplug = currentplug->next)
	{
		if (currentplug->consolelink)
		{
			char buffer[8192];
			char *ptr, oinfo = *info;
			*info = 0;
			ptr = (char*)COM_QuotedString(text, buffer, sizeof(buffer)-10, false);
			ptr += strlen(ptr);
			*ptr++ = ' ';
			*info = oinfo;
			ptr = (char*)COM_QuotedString(info, ptr, sizeof(buffer)-(ptr-buffer)-5, false);
			ptr += strlen(ptr);
			*ptr++ = ' ';
			COM_QuotedString(consolename, ptr, sizeof(buffer)-(ptr-buffer), false);

			Cmd_TokenizeString(buffer, false, false);
			result = currentplug->consolelink();
			if (result)
				break;
		}
	}
	currentplug = oldplug;
	return result;
}

int Plug_SubConsoleCommand(console_t *con, const char *line)
{
	int ret;
	char buffer[2048];
	plugin_t *oldplug = currentplug;	//shouldn't really be needed, but oh well
	currentplug = con->userdata;

	Q_strncpyz(buffer, va("\"%s\" %s", con->name, line), sizeof(buffer));
	Cmd_TokenizeString(buffer, false, false);
	ret = currentplug->conexecutecommand(0);
	currentplug = oldplug;
	return ret;
}
#endif

#ifndef SERVERONLY
int Plug_ConnectionlessClientPacket(char *buffer, int size)
{
	for (currentplug = plugs; currentplug; currentplug = currentplug->next)
	{
		if (currentplug->connectionlessclientpacket)
		{
			switch (currentplug->connectionlessclientpacket(buffer, size, &net_from))
			{
			case 0:
				continue;	//wasn't handled
			case 1:
				currentplug = NULL;	//was handled with no apparent result
				return true;
			case 2:
#ifndef SERVERONLY
				cls.protocol = CP_PLUGIN;	//woo, the plugin wants to connect to them!
				protocolclientplugin = currentplug;
#endif
				currentplug = NULL;
				return true;
			}
		}
	}
	return false;
}
#endif
#ifndef SERVERONLY
void Plug_SBar(playerview_t *pv)
{
#ifdef QUAKEHUD
	#define sb_showscores pv->sb_showscores
	#define sb_showteamscores pv->sb_showteamscores
#else
	#define sb_showscores 0
	#define sb_showteamscores 0
#endif

	plugin_t *oc=currentplug;
	int ret;
	int cleared = false;
	int hudmode;

	if (!Sbar_ShouldDraw(pv))
	{
		SCR_TileClear (0);
		return;
	}

	ret = 0;
	if (cl.deathmatch)
		hudmode = 1;
	else
		hudmode = 2;
	if (!(plug_sbar.ival&4) && ((cls.protocol != CP_QUAKEWORLD && cls.protocol != CP_NETQUAKE) || M_GameType()!=MGT_QUAKE1))
		currentplug = NULL;	//disable the hud if we're not running quake. q2/q3/h2 must not display the hud, allowing for a simpler install-anywhere installer that can include it.
	else if (!(plug_sbar.ival & hudmode))
		currentplug = NULL;
	else
	{
		for (currentplug = plugs; currentplug; currentplug = currentplug->next)
		{
			if (currentplug->sbarlevel[0])
			{
				//if you don't use splitscreen, use a full videosize rect.
				R2D_ImageColours(1, 1, 1, 1); // ensure menu colors are reset
				if (!cleared)
				{
					cleared = true;
					SCR_TileClear (0);
				}
				ret |= currentplug->sbarlevel[0](pv-cl.playerview, (int)r_refdef.vrect.x, (int)r_refdef.vrect.y, (int)r_refdef.vrect.width, (int)r_refdef.vrect.height, sb_showscores+sb_showteamscores*2);
				break;
			}
		}
	}
	if (!(ret & 1))
	{
		if (!cleared)
			SCR_TileClear (sb_lines);
		Sbar_Draw(pv);
	}

	for (currentplug = plugs; currentplug; currentplug = currentplug->next)
	{
		if (currentplug->sbarlevel[1])
		{
			R2D_ImageColours(1, 1, 1, 1); // ensure menu colors are reset
			ret |= currentplug->sbarlevel[1](pv-cl.playerview, r_refdef.vrect.x, r_refdef.vrect.y, r_refdef.vrect.width, r_refdef.vrect.height, sb_showscores+sb_showteamscores*2);
		}
	}

	for (currentplug = plugs; currentplug; currentplug = currentplug->next)
	{
		if (currentplug->sbarlevel[2])
		{
			R2D_ImageColours(1, 1, 1, 1); // ensure menu colors are reset
			ret |= currentplug->sbarlevel[2](pv-cl.playerview, r_refdef.vrect.x, r_refdef.vrect.y, r_refdef.vrect.width, r_refdef.vrect.height, sb_showscores+sb_showteamscores*2);
		}
	}

	if (!(ret & 2))
	{
		Sbar_DrawScoreboard(pv);
	}


	currentplug = oc;
}
#endif

qboolean Plug_ServerMessage(char *buffer, int messagelevel)
{
	qboolean ret = true;

	Cmd_TokenizeString(buffer, false, false);
	Cmd_Args_Set(buffer, strlen(buffer));

	for (currentplug = plugs; currentplug; currentplug = currentplug->next)
	{
		if (currentplug->svmsgfunction)
		{
			ret &= currentplug->svmsgfunction(messagelevel);
		}
	}

	Cmd_Args_Set(NULL, 0);

	return ret; // true to display message, false to supress
}

qboolean Plug_ChatMessage(char *buffer, int talkernum, int tpflags)
{
	qboolean ret = true;

	Cmd_TokenizeString(buffer, false, false);
	Cmd_Args_Set(buffer, strlen(buffer));

	for (currentplug = plugs; currentplug; currentplug = currentplug->next)
	{
		if (currentplug->chatmsgfunction)
		{
			ret &= currentplug->chatmsgfunction(talkernum, tpflags);
		}
	}

	Cmd_Args_Set(NULL, 0);

	return ret; // true to display message, false to supress
}

qboolean Plug_CenterPrintMessage(char *buffer, int clientnum)
{
	qboolean ret = true;

	Cmd_TokenizeString(buffer, false, false);
	Cmd_Args_Set(buffer, strlen(buffer));

	for (currentplug = plugs; currentplug; currentplug = currentplug->next)
	{
		if (currentplug->centerprintfunction)
		{
			ret &= currentplug->centerprintfunction(clientnum);
		}
	}

	Cmd_Args_Set(NULL, 0);

	return ret; // true to display message, false to supress
}

void Plug_Close(plugin_t *plug)
{
	int i;
	currentplug = plug;
	if (plug->mayshutdown && !plug->mayshutdown())
	{
		currentplug = NULL;
		Con_Printf("Plugin %s provides driver features, and cannot safely be unloaded at this time\n", plug->name);
		return;
	}
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

	if (!com_workererror)
		Con_DPrintf("Closing plugin %s\n", plug->name);

	//ensure any active contexts provided by the plugin are closed (stuff with destroy callbacks)
#if defined(PLUGINS) && !defined(SERVERONLY)
#ifdef HAVE_MEDIA_DECODER
	Media_UnregisterDecoder(plug, NULL);
#endif
#ifdef HAVE_MEDIA_ENCODER
	Media_UnregisterEncoder(plug, NULL);
#endif
#endif
#ifdef HAVE_CLIENT
	S_UnregisterSoundInputModule(plug);
#endif
	FS_UnRegisterFileSystemModule(plug);
	Mod_UnRegisterAllModelFormats(plug);

	//tell the plugin that everything is closed and that it should free up any lingering memory/stuff
	//it is still allowed to create/have open files.
	if (plug->shutdown)
	{
		plugin_t *cp = currentplug;
		currentplug = plug;
		plug->shutdown();
		currentplug = cp;
	}

	if (plug->lib)
		Sys_CloseLibrary(plug->lib);

	//make sure anything else that was left is unlinked (stuff without destroy callbacks).
	for (i = 0; i < pluginstreamarraylen; i++)
	{
		if (pluginstreamarray[i].plugin == plug)
		{
			Plug_Net_Close_Internal(i);
		}
	}

	Plug_FreeConCommands(plug);

	Plug_Client_Close(plug);
	Z_Free(plug);

	currentplug = NULL;
}

void Plug_Close_f(void)
{
	plugin_t *plug;
	char *name = Cmd_Argv(1);
	char cleaned[MAX_OSPATH];
	if (Cmd_Argc()<2)
	{
		Con_Printf("Close which plugin?\n");
		return;
	}

	name = Plug_CleanName(name, cleaned, sizeof(cleaned));

	if (currentplug)
		Sys_Error("Plug_CloseAll_f called inside a plugin!\n");

	for (plug = plugs; plug; plug = plug->next)
	{
		if (!strcmp(plug->name, name))
		{
			Plug_Close(plug);
			return;
		}
	}

	name = va("plugins/%s", name);
	for (plug = plugs; plug; plug = plug->next)
	{
		if (!strcmp(plug->name, name))
		{
			Plug_Close(plug);
			return;
		}
	}
	Con_Printf("Plugin %s does not appear to be loaded\n", Cmd_Argv(1));
}

void Plug_CloseAll_f(void)
{
	plugin_t *p;
	if (currentplug)
		Sys_Error("Plug_CloseAll_f called inside a plugin!\n");
	while(plugs)
	{
		p = plugs;
		while (p->mayshutdown && !p->mayshutdown())
		{
			p = p->next;
			if (!p)
				return;
		}
		Plug_Close(p);
	}
}

int QDECL Plug_List_Print(const char *fname, qofs_t fsize, time_t modtime, void *parm, searchpathfuncs_t *spath)
{
	plugin_t *plug;
	char plugname[MAX_QPATH];
	//lots of awkward logic so we hide modules for other cpus.
	size_t nl = strlen(fname);
	size_t u;
	static const char *knownarch[] =
	{
		"x32", "x64", "amd64", "x86",	//various x86 ABIs
		"arm", "arm64", "armhf",		//various arm ABIs
		"ppc", "unk",					//various misc ABIs
	};
#ifdef _WIN32
	char *mssuck;
	while ((mssuck=strchr(fname, '\\')))
		*mssuck = '/';
#endif
	if (nl >= strlen(ARCH_DL_POSTFIX) && !Q_strcasecmp(fname+nl-strlen(ARCH_DL_POSTFIX), ARCH_DL_POSTFIX))
	{
		nl -= strlen(ARCH_DL_POSTFIX);
		for (u = 0; u < countof(knownarch); u++)
		{
			size_t al = strlen(knownarch[u]);
			if (!Q_strncasecmp(fname+nl-al, knownarch[u], al))
			{
				nl -= al;
				break;
			}
		}
		if (u == countof(knownarch) || !Q_strcasecmp(knownarch[u], ARCH_CPU_POSTFIX))
		{
			if (nl > sizeof(plugname)-1)
				nl = sizeof(plugname)-1;
			if (nl>0&&fname[nl] == '_')
				nl--;	//ignore the _ before the ABI name.
			memcpy(plugname, fname, nl);
			plugname[nl] = 0;

			//don't bother printing it if its already loaded.
			for (plug = plugs; plug; plug = plug->next)
			{
				if (!Q_strncasecmp(plug->filename, parm, strlen(parm)) && !Q_strcasecmp(plug->filename+strlen(parm), fname))
					return true;
			}
			Con_Printf("^[^1%s%s\\type\\plug_load %s\\^]: not loaded\n", (const char*)parm, fname, plugname+((!Q_strncasecmp(plugname,PLUGINPREFIX, strlen(PLUGINPREFIX)))?strlen(PLUGINPREFIX):0));
		}
	}
	return true;
}

void Plug_List_f(void)
{
	char binarypath[MAX_OSPATH];
	char rootpath[MAX_OSPATH];
	unsigned int u;
	plugin_t *plug;
	Con_Printf("Loaded plugins:\n");
	for (plug = plugs; plug; plug = plug->next)
		Con_Printf("^[^2%s\\type\\plug_close %s\\^]: loaded\n", plug->filename, plug->name);

	if (FS_NativePath("", FS_BINARYPATH, binarypath, sizeof(binarypath)))
	{
#ifdef _WIN32
		char *mssuck;
		while ((mssuck=strchr(binarypath, '\\')))
			*mssuck = '/';
#endif
		Con_Printf("Scanning for plugins at %s:\n", binarypath);
		Sys_EnumerateFiles(binarypath, PLUGINPREFIX"*" ARCH_DL_POSTFIX, Plug_List_Print, binarypath, NULL);
	}
	if (FS_NativePath("", FS_ROOT, rootpath, sizeof(rootpath)))
	{
#ifdef _WIN32
		char *mssuck;
		while ((mssuck=strchr(rootpath, '\\')))
			*mssuck = '/';
#endif
		if (strcmp(binarypath, rootpath))
		{
			Con_Printf("Scanning for plugins at %s:\n", rootpath);
			Sys_EnumerateFiles(rootpath, PLUGINPREFIX"*" ARCH_DL_POSTFIX, Plug_List_Print, rootpath, NULL);
		}
	}

	for (u = 0; staticplugins[u].name; u++)
		Plug_List_Print(staticplugins[u].name, 0, 0, "", NULL);
}

void Plug_Shutdown(qboolean preliminary)
{
	plugin_t **p;
	if (preliminary)
	{
		//close the non-block-closes plugins first, before most of the rest of the subsystems are down
		for (p = &plugs; *p; )
		{
			if ((*p)->mayshutdown && !(*p)->mayshutdown())
				p = &(*p)->next;
			else
				Plug_Close(*p);
		}
	}
	else
	{
		//now that our various handles etc are closed, its safe to terminate the various driver plugins.
		while(plugs)
		{
			plugs->mayshutdown = NULL;
			Plug_Close(plugs);
		}

		BZ_Free(pluginstreamarray);
		pluginstreamarray = NULL;
		pluginstreamarraylen = 0;

		plugincvararraylen = 0;
		BZ_Free(plugincvararray);
		plugincvararray = NULL;

		plugincommandarraylen = 0;
		BZ_Free(plugincommandarray);
		plugincommandarray = NULL;

#ifndef SERVERONLY
		Plug_Client_Shutdown();
#endif
	}
}





static void *QDECL PlugBI_GetEngineInterface(const char *interfacename, size_t structsize)
{
	if (!strcmp(interfacename, plugcorefuncs_name))
	{
		static plugcorefuncs_t funcs =
		{
			PlugBI_GetEngineInterface,
			PlugBI_ExportFunction,
			PlugBI_ExportInterface,
			PlugBI_GetPluginName,
			Plug_Con_Print,
			Plug_Sys_Error,
			Plug_Sys_Milliseconds,
		};
		if (structsize == sizeof(funcs))
			return &funcs;
	}
	if (!strcmp(interfacename, plugcmdfuncs_name))
	{
		static plugcmdfuncs_t funcs =
		{
			Plug_Cmd_AddCommand,
			NULL,//Plug_Cmd_TokenizeString,
			Plug_Cmd_Args,
			Plug_Cmd_Argv,
			Plug_Cmd_Argc,
			Plug_Cmd_AddText,
		};
		if (structsize == sizeof(funcs))
			return &funcs;
	}
	if (!strcmp(interfacename, plugcvarfuncs_name))
	{
		static plugcvarfuncs_t funcs =
		{
			Plug_Cvar_SetString,
			Plug_Cvar_SetFloat,
			Plug_Cvar_GetString,
			Plug_Cvar_GetFloat,
			Plug_Cvar_Register,
			Plug_Cvar_Update,
			Plug_Cvar_GetNVFDG,
		};
		if (structsize == sizeof(funcs))
			return &funcs;
	}
	if (!strcmp(interfacename, plugfsfuncs_name))
	{
		static plugfsfuncs_t funcs =
		{
			Plug_FS_Open,
			Plug_Net_Close,
			Plug_Net_Send,
			Plug_Net_Recv,
			Plug_FS_Seek,
			Plug_FS_GetLength,

			FS_OpenVFS,
			FS_NativePath,
			COM_EnumerateFiles,
		};
		if (structsize == sizeof(funcs))
			return &funcs;
	}
	if (!strcmp(interfacename, plugnetfuncs_name))
	{
		static plugnetfuncs_t funcs =
		{
			Plug_Net_TCPConnect,
			Plug_Net_TCPListen,
			Plug_Net_Accept,
			Plug_Net_Recv,
			Plug_Net_Send,
			Plug_Net_SendTo,
			Plug_Net_Close,
			Plug_Net_SetTLSClient,
			Plug_Net_GetTLSBinding,
		};
		if (structsize == sizeof(funcs))
			return &funcs;
	}
#ifdef HAVE_CLIENT
	if (!strcmp(interfacename, plug2dfuncs_name))
	{
		static plug2dfuncs_t funcs =
		{
			Plug_Draw_LoadImageData,
			Plug_Draw_LoadImageShader,
			Plug_Draw_LoadImagePic,
			Plug_Draw_UnloadImage,
			Plug_Draw_Image,
			Plug_Draw_ImageSize,
			Plug_Draw_Fill,
			Plug_Draw_Line,
			Plug_Draw_Character,
			Plug_Draw_String,
			Plug_Draw_CharacterH,
			Plug_Draw_StringH,
			Plug_Draw_StringWidth,
			Plug_Draw_ColourP,
			Plug_Draw_Colour4f,

			Plug_LocalSound,
		};
		if (structsize == sizeof(funcs))
			return &funcs;
	}
	if (!strcmp(interfacename, plugclientfuncs_name))
	{
		static plugclientfuncs_t funcs =
		{
			Plug_CL_GetStats,
			Plug_GetPlayerInfo,
			Plug_GetNetworkInfo,
			Plug_GetLocalPlayerNumbers,
			Plug_GetLocationName,
			Plug_GetLastInputFrame,
			Plug_GetServerInfo,
			Plug_SetUserInfo,
#if defined(HAVE_SERVER) && defined(HAVE_CLIENT)
			Plug_MapLog_Query,
#else
			NULL,
#endif
#ifdef QUAKEHUD
			Plug_GetTeamInfo,
			Plug_GetWeaponStats,
			Plug_GetTrackerOwnFrags,
#else
			NULL,
			NULL,
			NULL,
#endif
		};
		if (structsize == sizeof(funcs))
			return &funcs;
	}
	if (!strcmp(interfacename, pluginputfuncs_name))
	{
		static pluginputfuncs_t funcs =
		{
			Plug_SetMenuFocus,
			Plug_HasMenuFocus,

			//for menu input
			Plug_Key_GetKeyCode,
			Plug_Key_GetKeyName,
			M_FindKeysForBind,
			Plug_Key_GetKeyBind,
			Plug_Key_SetKeyBind,
		};
		if (structsize == sizeof(funcs))
			return &funcs;
	}
	if (!strcmp(interfacename, plugsubconsolefuncs_name))
	{
		static plugsubconsolefuncs_t funcs =
		{
			Plug_Con_POpen,
			Plug_Con_SubPrint,
			Plug_Con_RenameSub,
			Plug_Con_IsActive,
			Plug_Con_SetActive,
			Plug_Con_Destroy,
			Plug_Con_NameForNum,
			Plug_Con_GetConsoleFloat,
			Plug_Con_SetConsoleFloat,
			Plug_Con_GetConsoleString,
			Plug_Con_SetConsoleString,
		};
		if (structsize == sizeof(funcs))
			return &funcs;
	}
	if (!strcmp(interfacename, plugaudiofuncs_name))
	{
		static plugaudiofuncs_t funcs =
		{
			Plug_LocalSound,
			Plug_S_RawAudio,
		};
		if (structsize == sizeof(funcs))
			return &funcs;
	}
#endif

#ifdef SUPPORT_ICE
	if (!strcmp(interfacename, ICE_API_CURRENT) && structsize == sizeof(iceapi))
		return &iceapi;
#endif
#ifdef USERBE
	if (!strcmp(interfacename, "RBE"))
	{
		static rbeplugfuncs_t funcs =
		{
			RBEPLUGFUNCS_VERSION,

			World_RegisterPhysicsEngine,
			World_UnregisterPhysicsEngine,
			World_GenerateCollisionMesh,
			World_ReleaseCollisionMesh,
			World_LinkEdict,

			VectorAngles,
			AngleVectors
		};
		if (structsize == sizeof(funcs))
			return &funcs;
	}
#endif
#ifdef MULTITHREAD
	if (!strcmp(interfacename, plugthreadfuncs_name))
	{
		static plugthreadfuncs_t funcs =
		{
			Sys_CreateMutex,
			Sys_LockMutex,
			Sys_UnlockMutex,
			Sys_DestroyMutex
		};

		if (structsize == sizeof(funcs))
			return &funcs;
	}
#endif
#ifdef SKELETALMODELS
	if (!strcmp(interfacename, "Models"))
	{
		static plugmodfuncs_t funcs =
		{
			MODPLUGFUNCS_VERSION,

			Plug_RegisterModelFormatText,
			Plug_RegisterModelFormatMagic,
			Plug_UnRegisterModelFormat,
			Plug_UnRegisterAllModelFormats,

			ZG_Malloc,
			COM_StripExtension,

			R_ConcatTransforms,
			Matrix3x4_Invert_Simple,
			VectorAngles,
			AngleVectors,
			GenMatrixPosQuat4Scale,

			Alias_ForceConvertBoneData,

#ifdef HAVE_CLIENT
			Image_GetTexture,
#else
			NULL,
#endif
			Mod_AccumulateTextureVectors,
			Mod_NormaliseTextureVectors,
			Mod_ForName
		};
		if (structsize == sizeof(funcs))
			return &funcs;
	}
#endif

#ifdef TERRAIN
	if (!strcmp(interfacename, "Terrain"))
		return Terr_GetTerrainFuncs(structsize);
#endif

#ifdef HAVE_SERVER
	if (!strcmp(interfacename, "SSQCVM"))
		return sv.world.progs;
#endif
#if defined(CSQC_DAT) && defined(HAVE_CLIENT)
	if (!strcmp(interfacename, "CSQCVM"))
	{
		extern world_t csqc_world;
		return csqc_world.progs;
	}
#endif
#if defined(MENU_DAT) && defined(HAVE_CLIENT)
	if (!strcmp(interfacename, "MenuQCVM"))
	{
		extern world_t menu_world;
		return menu_world.progs;
	}
#endif
	return NULL;
}

#endif
