#include "quakedef.h"
#ifdef MENU_NATIVECODE
static dllhandle_t *libmenu;
menu_export_t *mn_entry;

extern unsigned int r2d_be_flags;
#include "pr_common.h"
#include "shader.h"
#include "cl_master.h"

static int MN_checkextension(const char *extname)
{
	unsigned int i;
	for (i = 0; i < QSG_Extensions_count; i++)
	{
		if (!strcmp(QSG_Extensions[i].name, extname))
			return true;
	}
	return false;
}
static void MN_localcmd(const char *text)
{
	Cbuf_AddText(text, RESTRICT_LOCAL);	//menus are implicitly trusted. latching and other stuff would be a nightmare otherwise.
}
static void MN_registercvar(const char *cvarname, const char *defaulttext, unsigned int flags, const char *description)
{
	Cvar_Get2(cvarname, defaulttext, flags, description, NULL);
}
static int MN_getserverstate(void)
{
	if (!sv.active)
		return 0;
	if (svs.allocated_client_slots <= 1)
		return 1;
	return 2;
}
static int MN_getclientstate(void)
{
	if (cls.state >= ca_active)
		return 2;
	if (cls.state != ca_disconnected)
		return 1;
	return 0;
}
static void MN_fclose(vfsfile_t *f)
{
	VFS_CLOSE(f);
}
static void *MN_precache_pic(const char *picname)
{
	return R2D_SafeCachePic(picname);
}
static int MN_drawgetimagesize(void *pic, int *w, int *h)
{
	return R_GetShaderSizes(pic, w, h, true);
}
static void MN_drawquad(vec2_t position[4], vec2_t texcoords[4], void *pic, vec4_t rgba, unsigned int be_flags)
{
	r2d_be_flags = be_flags;
	R2D_ImageColours(rgba[0], rgba[1], rgba[2], rgba[3]);
	R2D_Image2dQuad(position, texcoords, pic);
	r2d_be_flags = 0;
}
static float MN_drawstring(vec2_t position, const char *text, float height, vec4_t rgba, unsigned int be_flags)
{
	float px, py, ix;
	unsigned int codeflags, codepoint;
	conchar_t buffer[2048], *str = buffer;
	COM_ParseFunString(CON_WHITEMASK, text, buffer, sizeof(buffer), false);

	Font_BeginScaledString(font_default, position[0], position[1], height, height, &px, &py);
	ix=px;
	while(*str)
	{
		str = Font_Decode(str, &codeflags, &codepoint);
		px = Font_DrawScaleChar(px, py, codeflags, codepoint);
	}
	Font_EndString(font_default);
	return ((px-ix)*(float)vid.width)/(float)vid.rotpixelwidth;
}
static float MN_stringwidth(const char *text, float height)
{
	float px, py;
	conchar_t buffer[2048], *end;
	end = COM_ParseFunString(CON_WHITEMASK, text, buffer, sizeof(buffer), false);

	Font_BeginScaledString(font_default, 0, 0, height, height, &px, &py);
	px = Font_LineScaleWidth(buffer, end);
	Font_EndString(font_default);
	return (px * (float)vid.width) / (float)vid.rotpixelwidth;
}
static void MN_drawsetcliparea(float x, float y, float width, float height)
{
	srect_t srect;
	if (R2D_Flush)
		R2D_Flush();

	srect.x = x / (float)vid.fbvwidth;
	srect.y = y / (float)vid.fbvheight;
	srect.width = width / (float)vid.fbvwidth;
	srect.height = height / (float)vid.fbvheight;
	srect.dmin = -99999;
	srect.dmax = 99999;
	srect.y = (1-srect.y) - srect.height;
	BE_Scissor(&srect);
}
static void MN_drawresetcliparea(void)
{
	if (R2D_Flush)
		R2D_Flush();
	BE_Scissor(NULL);
}
static qboolean MN_setkeydest(qboolean focused)
{
	qboolean ret = Key_Dest_Has(kdm_nmenu);
	if (ret == focused)
		return false;
	if (focused)
	{
		if (key_dest_absolutemouse & kdm_nmenu)
		{	//we're activating the mouse cursor now... make sure the position is actually current.
			struct menu_inputevent_args_s ev = {MIE_MOUSEABS, -1};
			ev.mouse.screen[0] = mousecursor_x;
			ev.mouse.screen[1] = mousecursor_y;
			mn_entry->InputEvent(ev);
		}
		Key_Dest_Add(kdm_nmenu);
	}
	else
		Key_Dest_Remove(kdm_nmenu);
	return true;
}
static int MN_getkeydest(void)
{
	if (Key_Dest_Has(kdm_nmenu))
	{
		if (Key_Dest_Has_Higher(kdm_nmenu))
			return -1;
		return 1;
	}
	return 0;
}
static int MN_setmousetarget(const char *cursorname, float hot_x, float hot_y, float scale)
{
	if (cursorname)
	{
		struct key_cursor_s *m = &key_customcursor[kc_nmenu];
		if (scale <= 0)
			scale = 1;
		if (!strcmp(m->name, cursorname) || m->hotspot[0] != hot_x || m->hotspot[1] != hot_y || m->scale != scale)
		{
			Q_strncpyz(m->name, cursorname, sizeof(m->name));
			m->hotspot[0] = hot_x;
			m->hotspot[1] = hot_y;
			m->scale = scale;
			m->dirty = true;
		}
		key_dest_absolutemouse |= kdm_nmenu;
	}
	else
		key_dest_absolutemouse &= ~kdm_nmenu;
	return true;
}

void MN_Shutdown(void)
{
	Key_Dest_Remove(kdm_nmenu);
	if (mn_entry)
	{
		mn_entry->Shutdown();
		mn_entry = NULL;
	}
	if (libmenu)
	{
		Sys_CloseLibrary(libmenu);
		libmenu = NULL;
	}
}
qboolean MN_Init(void)
{
	menu_export_t *(QDECL *pGetMenuAPI) ( menu_import_t *import );
	static menu_import_t imports =
	{
		NATIVEMENU_API_VERSION_MAX,

		MN_checkextension,
		Host_Error,
		Con_Printf,
		Con_DPrintf,
		MN_localcmd,
		Cvar_VariableValue,
		Cvar_VariableString,
		Cvar_SetNamed,
		MN_registercvar,

		MN_getserverstate,
		MN_getclientstate,
		S_LocalSound2,
	
		// file input / search crap
		FS_OpenVFS,
		MN_fclose,
		VFS_GETS,
		VFS_PRINTF,
		COM_EnumerateFiles,

	// Drawing stuff
		MN_precache_pic,
		MN_drawgetimagesize,
		MN_drawquad,
		MN_drawstring,
		MN_stringwidth,
		MN_drawsetcliparea,
		MN_drawresetcliparea,

		// Menu specific stuff
		MN_setkeydest,
		MN_getkeydest,
		MN_setmousetarget,
		Key_KeynumToString,
		Key_StringToKeynum,
		M_FindKeysForBind,

		// Server browser stuff
		NULL,//MN_gethostcachevalue,
		Master_ReadKeyString,
		Master_ReadKeyFloat,

		Master_ClearMasks,
		Master_SetMaskString,
		Master_SetMaskInteger,
		Master_SetSortField,
		Master_SortServers,
		Master_SortedServer,
		MasterInfo_Refresh,

		Master_KeyForName,
	};
	dllfunction_t funcs[] =
	{
		{(void*)&pGetMenuAPI, "GetMenuAPI"},
		{NULL}
	};
	void *iterator = NULL;
	char syspath[MAX_OSPATH];
	char gamepath[MAX_QPATH];

	while(COM_IteratePaths(&iterator, syspath, sizeof(syspath), gamepath, sizeof(gamepath)))
	{
		if (!com_nogamedirnativecode.ival)
			libmenu = Sys_LoadLibrary(va("%smenu_"ARCH_CPU_POSTFIX ARCH_DL_POSTFIX, syspath), funcs);
		if (libmenu)
			break;

		if (host_parms.binarydir && !strchr(gamepath, '/') && !strchr(gamepath, '\\'))
			libmenu = Sys_LoadLibrary(va("%smenu_%s_"ARCH_CPU_POSTFIX ARCH_DL_POSTFIX, host_parms.binarydir, gamepath), funcs);
		if (libmenu)
			break;

		//some build systems don't really know the cpu type.
		if (host_parms.binarydir && !strchr(gamepath, '/') && !strchr(gamepath, '\\'))
			libmenu = Sys_LoadLibrary(va("%smenu_%s" ARCH_DL_POSTFIX, host_parms.binarydir, gamepath), funcs);
		if (libmenu)
			break;
	}

	if (libmenu)
	{
		key_dest_absolutemouse |= kdm_nmenu;

		mn_entry = pGetMenuAPI (&imports); 
		if (mn_entry && mn_entry->api_version >= NATIVEMENU_API_VERSION_MIN && mn_entry->api_version <= NATIVEMENU_API_VERSION_MAX)
		{
			mn_entry->Init();
			return true;
		}
		else
			mn_entry = NULL;
		MN_Shutdown();
		Sys_CloseLibrary(libmenu);
		libmenu = NULL;
	}

	return false;
}
#endif