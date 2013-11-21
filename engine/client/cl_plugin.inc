//included directly from plugin.c
//this is the client-only things.






static plugin_t *menuplug;	//plugin that has the current menu
static plugin_t *protocolclientplugin;



qintptr_t VARGS Plug_Menu_Control(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	if (qrenderer == QR_NONE)
		return 0;

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
			Key_Dest_Remove(kdm_menu);
		}
		if (VM_LONG(arg[0]) != 1)
			return 1;
		//give us menu control
		menuplug = currentplug;
		Key_Dest_Add(kdm_menu);
		m_state = m_plugin;
		return 1;
	case 2: //weather it's us or not.
		return currentplug == menuplug && m_state == m_plugin;
	case 3:	//weather a menu is active
		return !!Key_Dest_Has(kdm_menu);
	default:
		return 0;
	}
}

qintptr_t VARGS Plug_Key_GetKeyCode(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int modifier;
	return Key_StringToKeynum(VM_POINTER(arg[0]), &modifier);
}




qintptr_t VARGS Plug_SCR_CenterPrint(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	if (qrenderer == QR_NONE)
		return 0;

	SCR_CenterPrint(0, VM_POINTER(arg[0]), true);
	return 0;
}




typedef struct {
	//Make SURE that the engine has resolved all cvar pointers into globals before this happens.
	plugin_t *plugin;
	char name[64];
	int type;
	char *script;
	mpic_t *pic;
} pluginimagearray_t;
int pluginimagearraylen;
pluginimagearray_t *pluginimagearray;

#include "shader.h"

qintptr_t VARGS Plug_Draw_LoadImage(char *name, int type, char *script)
{
	int i;

	mpic_t *pic;

	if (!*name)
		return 0;

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
		pluginimagearray[i].pic = NULL;
	}

	if (pluginimagearray[i].pic)
		return i+1;	//already loaded.

	if (qrenderer != QR_NONE)
	{
		if (type == 3)
			pic = NULL;
		else if (type == 2)
			pic = R_RegisterShader(name, SUF_NONE, script);
		else if (type)
			pic = R2D_SafePicFromWad(name);
		else
			pic = R2D_SafeCachePic(name);
	}
	else
		pic = NULL;

	Q_strncpyz(pluginimagearray[i].name, name, sizeof(pluginimagearray[i].name));
	pluginimagearray[i].type = type;
	pluginimagearray[i].pic = pic;
	pluginimagearray[i].plugin = currentplug;
	pluginimagearray[i].script = script?Z_StrDup(script):NULL;
	return i + 1;
}

qbyte *Read32BitImageFile(qbyte *buf, int len, int *width, int *height, qboolean *hasalpha, char *fname);
qintptr_t VARGS Plug_Draw_LoadImageData(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	qintptr_t ret = 0;
	char *name = VM_POINTER(arg[0]);
	char *mimetype = VM_POINTER(arg[1]);
	void *codeddata = VM_POINTER(arg[2]);
	unsigned int datalength = VM_LONG(arg[3]);
	texid_t t;
	qbyte *rgbdata;
	unsigned int width, height;
	
	if (VM_OOB(arg[2], arg[3]))
		return 0;
		
	if ((rgbdata = Read32BitImageFile(codeddata, datalength, &width, &height, NULL, name)))
	{
		name = va("%s/", name);
		t = R_FindTexture(name, IF_NOMIPMAP|IF_UIPIC|IF_CLAMP);
		if (!TEXVALID(t))
			t = R_AllocNewTexture(name, width, height, IF_NOMIPMAP|IF_UIPIC|IF_CLAMP);
		if (TEXVALID(t))
		{
			R_Upload(t, name, TF_RGBA32, rgbdata, NULL, width, height, IF_NOMIPMAP|IF_UIPIC|IF_CLAMP);
			ret = Plug_Draw_LoadImage(name, 3, NULL);
		}
		
		BZ_Free(rgbdata);
	}
	return ret;
}
qintptr_t VARGS Plug_Draw_LoadImageShader(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *name = VM_POINTER(arg[0]);
	char *script = VM_POINTER(arg[1]);
	return Plug_Draw_LoadImage(name, 2, script);
}
qintptr_t VARGS Plug_Draw_LoadImagePic(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *name = VM_POINTER(arg[0]);
	int type = arg[1];
	if (type != 0 && type != 1)
		return 0;
	return Plug_Draw_LoadImage(name, type, NULL);
}

void Plug_DrawReloadImages(void)
{
	int i;
	for (i = 0; i < pluginimagearraylen; i++)
	{
		if (!pluginimagearray[i].plugin)
		{
			pluginimagearray[i].pic = NULL;
			continue;
		}

		pluginimagearray[i].pic = R2D_SafePicFromWad(pluginimagearray[i].name);
		//pluginimagearray[i].pic = R2D_SafeCachePic(pluginimagearray[i].name);
		//pluginimagearray[i].pic = NULL;
	}
}

static void Plug_FreePlugImages(plugin_t *plug)
{
	int i;
	for (i = 0; i < pluginimagearraylen; i++)
	{
		if (pluginimagearray[i].plugin == plug)
		{
			pluginimagearray[i].plugin = 0;
			pluginimagearray[i].pic = NULL;
			pluginimagearray[i].name[0] = '\0';
		}
	}
}

//int R2D_Image (float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t image)
qintptr_t VARGS Plug_Draw_Image(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	mpic_t *pic;
	int i;
	if (qrenderer == QR_NONE)
		return 0;

	i = VM_LONG(arg[8]);
	if (i <= 0 || i > pluginimagearraylen)
		return -1;	// you fool
	i = i - 1;
	if (pluginimagearray[i].plugin != currentplug)
		return -1;

	if (pluginimagearray[i].pic)
		pic = pluginimagearray[i].pic;
	else if (pluginimagearray[i].type == 1)
		return 0;	//wasn't loaded.
	else
	{
		pic = R2D_SafeCachePic(pluginimagearray[i].name);
		if (!pic)
			return -1;
	}

	R2D_Image(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]), VM_FLOAT(arg[2]), VM_FLOAT(arg[3]), VM_FLOAT(arg[4]), VM_FLOAT(arg[5]), VM_FLOAT(arg[6]), VM_FLOAT(arg[7]), pic);
	return 1;
}
//x1,y1,x2,y2
qintptr_t VARGS Plug_Draw_Line(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	switch(qrenderer)	//FIXME: I don't want qrenderer seen outside the refresh
	{
#ifdef GLQUAKE
	case QR_OPENGL:
		qglDisable(GL_TEXTURE_2D);
		qglBegin(GL_LINES);
		qglVertex2f(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]));
		qglVertex2f(VM_FLOAT(arg[2]), VM_FLOAT(arg[3]));
		qglEnd();
		qglEnable(GL_TEXTURE_2D);
		break;
#endif
	default:
		return 0;
	}
	return 1;
}
qintptr_t VARGS Plug_Draw_Character(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int x, y;
	if (qrenderer == QR_NONE)
		return 0;
	Font_BeginString(font_conchar, arg[0], arg[1], &x, &y);
	Font_DrawChar(x, y, CON_WHITEMASK | 0xe000 | (unsigned int)arg[2]);
	Font_EndString(font_conchar);
	return 0;
}
qintptr_t VARGS Plug_Draw_String(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int ipx, px, py;
	conchar_t buffer[2048], *str;
	if (qrenderer == QR_NONE)
		return 0;
	COM_ParseFunString(CON_WHITEMASK, VM_POINTER(arg[2]), buffer, sizeof(buffer), false);
	str = buffer;
	Font_BeginString(font_conchar, VM_FLOAT(arg[0]), VM_FLOAT(arg[1]), &px, &py);
	ipx = px;
	while(*str)
	{
		if ((*str & CON_CHARMASK) == '\n')
			py += Font_CharHeight();
		else if ((*str & CON_CHARMASK) == '\r')
			px = ipx;
		else
			px = Font_DrawChar(px, py, *str);
		str++;
	}
	Font_EndString(font_conchar);
	return 0;
}

qintptr_t VARGS Plug_Draw_Fill(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	float x, y, width, height;
	if (qrenderer == QR_NONE)
		return 0;
	x = VM_FLOAT(arg[0]);
	y = VM_FLOAT(arg[1]);
	width = VM_FLOAT(arg[2]);
	height = VM_FLOAT(arg[3]);

	R2D_FillBlock(x, y, width, height);
	return 0;
}
qintptr_t VARGS Plug_Draw_ColourP(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	if (arg[0]<0 || arg[0]>255)
		return false;

	R2D_ImagePaletteColour(arg[0], 1);
	return 1;
}
qintptr_t VARGS Plug_Draw_Colour3f(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	R2D_ImageColours(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]), VM_FLOAT(arg[2]), 1);
	return 1;
}
qintptr_t VARGS Plug_Draw_Colour4f(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	R2D_ImageColours(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]), VM_FLOAT(arg[2]), VM_FLOAT(arg[3]));
	return 1;
}









qintptr_t VARGS Plug_LocalSound(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	if (qrenderer == QR_NONE)
		return false;

	S_LocalSound(VM_POINTER(arg[0]));
	return 0;
}



qintptr_t VARGS Plug_CL_GetStats(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int i;
	int pnum = VM_LONG(arg[0]);
	unsigned int *stats = VM_POINTER(arg[1]);
	int pluginstats = VM_LONG(arg[2]);
	int max;

	if (VM_OOB(arg[1], arg[2]*4))
		return 0;

	if (qrenderer == QR_NONE || !cls.state)
		return 0;

	max = pluginstats;
	if (max > MAX_CL_STATS)
		max = MAX_CL_STATS;
	for (i = 0; i < max; i++)
	{	//fill stats with the right player's stats
		stats[i] = cl.playerview[pnum].stats[i];
	}
	for (; i < pluginstats; i++)	//plugin has too many stats (wow)
		stats[i] = 0;					//fill the rest.
	return max;
}

#define PLUGMAX_SCOREBOARDNAME 64
typedef struct {
	int topcolour;
	int bottomcolour;
	int frags;
	char name[PLUGMAX_SCOREBOARDNAME];
	int ping;
	int pl;
	int starttime;
	int userid;
	int spectator;
	char userinfo[1024];
	char team[8];
} vmplugclientinfo_t;

qintptr_t VARGS Plug_GetPlayerInfo(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int i, pt;
	vmplugclientinfo_t *out;

	if (VM_OOB(arg[1], sizeof(vmplugclientinfo_t)))
		return -1;
	if (VM_LONG(arg[0]) < -1 || VM_LONG(arg[0] ) >= MAX_CLIENTS)
		return -2;

	i = VM_LONG(arg[0]);
	out = VM_POINTER(arg[1]);
	if (out)
	{
		if (i < 0)
		{
			if (i >= -MAX_SPLITS)
				i = cl.playerview[-i-1].playernum;
			if (i < 0)
			{
				memset(out, 0, sizeof(*out));
				return 0;
			}	
		}
		out->bottomcolour = cl.players[i].rbottomcolor;
		out->topcolour = cl.players[i].rtopcolor;
		out->frags = cl.players[i].frags;
		Q_strncpyz(out->name, cl.players[i].name, PLUGMAX_SCOREBOARDNAME);
		out->ping = cl.players[i].ping;
		out->pl = cl.players[i].pl;
		out->starttime = cl.players[i].entertime;
		out->userid = cl.players[i].userid;
		out->spectator = cl.players[i].spectator;
		Q_strncpyz(out->userinfo, cl.players[i].userinfo, sizeof(out->userinfo));
		Q_strncpyz(out->team, cl.players[i].team, sizeof(out->team));
	}

	pt = Cam_TrackNum(&cl.playerview[0]);
	if (pt < 0)
		return (cl.playerview[0].playernum == i);
	else
		return pt == i;
}

qintptr_t VARGS Plug_LocalPlayerNumber(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	return cl.playerview[0].playernum;
}

qintptr_t VARGS Plug_GetServerInfo(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *outptr = VM_POINTER(arg[0]);
	unsigned int outlen = VM_LONG(arg[1]);

	if (VM_OOB(arg[0], outlen))
		return false;

	Q_strncpyz(outptr, cl.serverinfo, outlen);

	return true;
}

qintptr_t VARGS Plug_SetUserInfo(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *key = VM_POINTER(arg[0]);
	char *value = VM_POINTER(arg[1]);

	CL_SetInfo(0, key, value);

	return true;
}

qintptr_t VARGS Plug_GetLocationName(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	float *locpoint = VM_POINTER(arg[0]);
	char *locname = VM_POINTER(arg[1]);
	unsigned int locnamelen = VM_LONG(arg[2]);
	char *result;

	if (VM_OOB(arg[1], locnamelen))
		return 0;

	result = TP_LocationName(locpoint);
	Q_strncpyz(locname, result, locnamelen);
	return VM_LONG(arg[1]);
}

qintptr_t VARGS Plug_Con_SubPrint(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *name = VM_POINTER(arg[0]);
	char *text = VM_POINTER(arg[1]);
	console_t *con;
	if (!name)
		name = "";

	if (qrenderer == QR_NONE)
	{
		if (!*name)
		{
			Con_Printf("%s", text);
			return 1;
		}
		return false;
	}

	con = Con_FindConsole(name);
	if (!con)
	{
		con = Con_Create(name, 0);
		Con_SetActive(con);

		if (currentplug->conexecutecommand)
		{
			con->notif_x = 0;
			con->notif_y = 8*4;
			con->notif_w = vid.width;
			con->notif_t = 8;
			con->notif_l = 4;
			con->flags |= CONF_NOTIFY;
			con->userdata = currentplug;
			con->linebuffered = Plug_SubConsoleCommand;
		}
	}

	Con_PrintCon(con, text);

	return 1;
}
qintptr_t VARGS Plug_Con_RenameSub(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *name = VM_POINTER(arg[0]);
	console_t *con;
	if (qrenderer == QR_NONE)
		return false;
	con = Con_FindConsole(name);
	if (!con)
		return 0;

	Q_strncpyz(con->name, name, sizeof(con->name));

	return 1;
}
qintptr_t VARGS Plug_Con_IsActive(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *name = VM_POINTER(arg[0]);
	console_t *con;
	if (qrenderer == QR_NONE)
		return false;
	con = Con_FindConsole(name);
	if (!con)
		return false;

	return Con_IsActive(con);
}
qintptr_t VARGS Plug_Con_SetActive(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *name = VM_POINTER(arg[0]);
	console_t *con;
	if (qrenderer == QR_NONE)
		return false;
	con = Con_FindConsole(name);
	if (!con)
		con = Con_Create(name, 0);

	Con_SetActive(con);
	return true;
}
qintptr_t VARGS Plug_Con_Destroy(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *name = VM_POINTER(arg[0]);
	console_t *con;
	if (qrenderer == QR_NONE)
		return false;
	con = Con_FindConsole(name);
	if (!con)
		return false;

	Con_Destroy(con);
	return true;
}
qintptr_t VARGS Plug_Con_NameForNum(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char num = VM_LONG(arg[0]);
	char *buffer = VM_POINTER(arg[1]);
	int buffersize = VM_LONG(arg[2]);
	if (VM_OOB(arg[1], buffersize) || buffersize < 1)
		return false;
	if (qrenderer == QR_NONE)
		return false;

	return Con_NameForNum(num, buffer, buffersize);
}

qintptr_t VARGS Plug_S_RawAudio(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int sourceid = VM_LONG(arg[0]);
	qbyte *data = VM_POINTER(arg[1]);
	int speed = VM_LONG(arg[2]);
	int samples = VM_LONG(arg[3]);
	int channels = VM_LONG(arg[4]);
	int width = VM_LONG(arg[5]);
	int volume = VM_FLOAT(arg[6]);
	
	int datasize = samples * channels * width;
	
	if (VM_OOB(arg[1], datasize) || datasize < 1)
		return false;
		
	S_RawAudio(sourceid, data, speed, samples, channels, width, volume);
	return 0;
}

void Plug_Client_Init(void)
{
	Plug_RegisterBuiltin("CL_GetStats",				Plug_CL_GetStats, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Menu_Control",			Plug_Menu_Control, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Key_GetKeyCode",			Plug_Key_GetKeyCode, PLUG_BIF_NEEDSRENDERER);

	Plug_RegisterBuiltin("Draw_LoadImageData",		Plug_Draw_LoadImageData, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_LoadImageShader",	Plug_Draw_LoadImageShader, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_LoadImage",			Plug_Draw_LoadImagePic, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_Image",				Plug_Draw_Image, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_Character",			Plug_Draw_Character, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_String",				Plug_Draw_String, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_Fill",				Plug_Draw_Fill, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_Line",				Plug_Draw_Line, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_Colourp",			Plug_Draw_ColourP, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_Colour3f",			Plug_Draw_Colour3f, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_Colour4f",			Plug_Draw_Colour4f, PLUG_BIF_NEEDSRENDERER);

	Plug_RegisterBuiltin("Con_SubPrint",			Plug_Con_SubPrint, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Con_RenameSub",			Plug_Con_RenameSub, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Con_IsActive",			Plug_Con_IsActive, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Con_SetActive",			Plug_Con_SetActive, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Con_Destroy",				Plug_Con_Destroy, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Con_NameForNum",			Plug_Con_NameForNum, PLUG_BIF_NEEDSRENDERER);

	Plug_RegisterBuiltin("LocalSound",				Plug_LocalSound, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("SCR_CenterPrint",			Plug_SCR_CenterPrint, PLUG_BIF_NEEDSRENDERER);

	Plug_RegisterBuiltin("GetLocationName",			Plug_GetLocationName, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("GetPlayerInfo",			Plug_GetPlayerInfo, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("LocalPlayerNumber",		Plug_LocalPlayerNumber, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("GetServerInfo",			Plug_GetServerInfo, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("SetUserInfo",				Plug_SetUserInfo, PLUG_BIF_NEEDSRENDERER);
	
	Plug_RegisterBuiltin("S_RawAudio",				Plug_S_RawAudio, PLUG_BIF_NEEDSRENDERER);
}

void Plug_Client_Close(plugin_t *plug)
{
	Plug_FreePlugImages(plug);


	if (menuplug == plug)
	{
		menuplug = NULL;
		Key_Dest_Remove(kdm_menu);
	}
	if (protocolclientplugin == plug)
	{
		protocolclientplugin = NULL;
		if (cls.protocol == CP_PLUGIN)
			cls.protocol = CP_UNKNOWN;
	}
}

void Plug_Client_Shutdown(void)
{
	BZ_Free(pluginimagearray);
	pluginimagearray = NULL;
	pluginimagearraylen = 0;
}






