//included directly from plugin.c
//this is the client-only things.






static plugin_t *menuplug;	//plugin that has the current menu
static plugin_t *protocolclientplugin;



qintptr_t VARGS Plug_Menu_Control(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	if (qrenderer <= 0)
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

qintptr_t VARGS Plug_Key_GetKeyCode(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int modifier;
	return Key_StringToKeynum(VM_POINTER(arg[0]), &modifier);
}




qintptr_t VARGS Plug_SCR_CenterPrint(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	if (qrenderer <= 0)
		return 0;

	SCR_CenterPrint(0, VM_POINTER(arg[0]), true);
	return 0;
}
qintptr_t VARGS Plug_Media_ShowFrameRGBA_32(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	void *src = VM_POINTER(arg[0]);
	int srcwidth = VM_LONG(arg[1]);
	int srcheight = VM_LONG(arg[2]);
//	int x = VM_LONG(arg[3]);
//	int y = VM_LONG(arg[4]);
//	int width = VM_LONG(arg[5]);
//	int height = VM_LONG(arg[6]);

	if (qrenderer <= 0)
		return 0;

	Media_ShowFrameRGBA_32(src, srcwidth, srcheight);
	return 0;
}




typedef struct {
	//Make SURE that the engine has resolved all cvar pointers into globals before this happens.
	plugin_t *plugin;
	char name[64];
	qboolean picfromwad;
	mpic_t *pic;
} pluginimagearray_t;
int pluginimagearraylen;
pluginimagearray_t *pluginimagearray;

qintptr_t VARGS Plug_Draw_LoadImage(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *name = VM_POINTER(arg[0]);
	qboolean fromwad = arg[1];
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

	if (qrenderer > 0)
	{
		if (fromwad && Draw_SafePicFromWad)
			pic = Draw_SafePicFromWad(name);
		else
		{
			if (Draw_SafeCachePic)
				pic = Draw_SafeCachePic(name);
			else
				pic = NULL;
		}
	}
	else
		pic = NULL;

	Q_strncpyz(pluginimagearray[i].name, name, sizeof(pluginimagearray[i].name));
	pluginimagearray[i].picfromwad = fromwad;
	pluginimagearray[i].pic = pic;
	pluginimagearray[i].plugin = currentplug;
	return i + 1;
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

		if (Draw_SafePicFromWad)
			pluginimagearray[i].pic = Draw_SafePicFromWad(pluginimagearray[i].name);
		else if (Draw_SafeCachePic)
				pluginimagearray[i].pic = Draw_SafeCachePic(pluginimagearray[i].name);
		else
			pluginimagearray[i].pic = NULL;
	}
}

void Plug_FreePlugImages(plugin_t *plug)
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

//int Draw_Image (float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t image)
qintptr_t VARGS Plug_Draw_Image(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	mpic_t *pic;
	int i;
	if (qrenderer <= 0)
		return 0;
	if (!Draw_Image)
		return 0;

	i = VM_LONG(arg[8]);
	if (i <= 0 || i > pluginimagearraylen)
		return -1;	// you fool
	i = i - 1;
	if (pluginimagearray[i].plugin != currentplug)
		return -1;

	if (pluginimagearray[i].pic)
		pic = pluginimagearray[i].pic;
	else if (pluginimagearray[i].picfromwad)
		return 0;	//wasn't loaded.
	else
	{
		pic = Draw_SafeCachePic(pluginimagearray[i].name);
		if (!pic)
			return -1;
	}

	Draw_Image(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]), VM_FLOAT(arg[2]), VM_FLOAT(arg[3]), VM_FLOAT(arg[4]), VM_FLOAT(arg[5]), VM_FLOAT(arg[6]), VM_FLOAT(arg[7]), pic);
	return 1;
}
//x1,y1,x2,y2
qintptr_t VARGS Plug_Draw_Line(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	switch(qrenderer)	//FIXME: I don't want qrenderer seen outside the refresh
	{
#ifdef RGLQUAKE
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
	if (qrenderer <= 0)
		return 0;
	Draw_Character(arg[0], arg[1], (unsigned int)arg[2]);
	return 0;
}
void	(D3D_Draw_Fill_Colours)				(int x, int y, int w, int h);
qintptr_t VARGS Plug_Draw_Fill(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	float x, y, width, height;
	if (qrenderer <= 0)
		return 0;
	x = VM_FLOAT(arg[0]);
	y = VM_FLOAT(arg[1]);
	width = VM_FLOAT(arg[2]);
	height = VM_FLOAT(arg[3]);
	switch(qrenderer)	//FIXME: I don't want qrenderer seen outside the refresh
	{
#ifdef RGLQUAKE
	case QR_OPENGL:
		qglDisable(GL_TEXTURE_2D);
		qglBegin(GL_QUADS);
		qglVertex2f(x, y);
		qglVertex2f(x+width, y);
		qglVertex2f(x+width, y+height);
		qglVertex2f(x, y+height);
		qglEnd();
		qglEnable(GL_TEXTURE_2D);
		return 1;
#endif
	case QR_DIRECT3D:
//		D3D_Draw_Fill_Colours(x, y, width, height);
		break;
	default:
		break;
	}
	return 0;
}
qintptr_t VARGS Plug_Draw_ColourP(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	qbyte *pal = host_basepal + VM_LONG(arg[0])*3;

	if (arg[0]<0 || arg[0]>255)
		return false;

	if (Draw_ImageColours)
	{
		Draw_ImageColours(pal[0]/255.0f, pal[1]/255.0f, pal[2]/255.0f, 1);
		return 1;
	}
	return 0;
}
qintptr_t VARGS Plug_Draw_Colour3f(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	if (Draw_ImageColours)
	{
		Draw_ImageColours(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]), VM_FLOAT(arg[2]), 1);
		return 1;
	}
	return 0;
}
qintptr_t VARGS Plug_Draw_Colour4f(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	if (Draw_ImageColours)
	{
		Draw_ImageColours(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]), VM_FLOAT(arg[2]), VM_FLOAT(arg[3]));
		return 1;
	}
	return 0;
}









qintptr_t VARGS Plug_LocalSound(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	if (qrenderer <= 0)
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

	if (qrenderer <= 0)
		return false;

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
		if (i == -1)
		{
			i = cl.playernum[0];
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

	pt = Cam_TrackNum(0);
	if (pt < 0)
		return (cl.playernum[0] == i);
	else
		return pt == i;
}

qintptr_t VARGS Plug_LocalPlayerNumber(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	return cl.playernum[0];
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

	CL_SetInfo(key, value);

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

	if (qrenderer <= 0)
		return false;

	con = Con_FindConsole(name);
	if (!con)
	{
		con = Con_Create(name);
		Con_SetActive(con);

		if (currentplug->conexecutecommand)
		{
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
	if (qrenderer <= 0)
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
	if (qrenderer <= 0)
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
	if (qrenderer <= 0)
		return false;
	con = Con_FindConsole(name);
	if (!con)
		con = Con_Create(name);

	Con_SetActive(con);
	return true;
}
qintptr_t VARGS Plug_Con_Destroy(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *name = VM_POINTER(arg[0]);
	console_t *con;
	if (qrenderer <= 0)
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
	if (VM_OOB(arg[1], arg[2]) || buffersize < 1)
		return false;
	if (qrenderer <= 0)
		return false;

	return Con_NameForNum(num, buffer, buffersize);
}



void Plug_Client_Init(void)
{
	Plug_RegisterBuiltin("CL_GetStats",				Plug_CL_GetStats, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Menu_Control",			Plug_Menu_Control, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Key_GetKeyCode",			Plug_Key_GetKeyCode, PLUG_BIF_NEEDSRENDERER);

	Plug_RegisterBuiltin("Draw_LoadImage",			Plug_Draw_LoadImage, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_Image",				Plug_Draw_Image, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_Character",			Plug_Draw_Character, PLUG_BIF_NEEDSRENDERER);
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
	Plug_RegisterBuiltin("Media_ShowFrameRGBA_32",	Plug_Media_ShowFrameRGBA_32, PLUG_BIF_NEEDSRENDERER);

	Plug_RegisterBuiltin("GetLocationName",			Plug_GetLocationName, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("GetPlayerInfo",			Plug_GetPlayerInfo, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("LocalPlayerNumber",		Plug_LocalPlayerNumber, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("GetServerInfo",			Plug_GetServerInfo, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("SetUserInfo",				Plug_SetUserInfo, PLUG_BIF_NEEDSRENDERER);
}

void Plug_Client_Close(plugin_t *plug)
{
	Plug_FreePlugImages(plug);


	if (menuplug == plug)
	{
		menuplug = NULL;
		key_dest = key_game;
	}
	if (protocolclientplugin == plug)
	{
		protocolclientplugin = NULL;
		if (cls.protocol == CP_PLUGIN)
			cls.protocol = CP_UNKNOWN;
	}
}








