//included directly from plugin.c
//this is the client-only things.






plugin_t *menuplug;	//plugin that has the current menu
static plugin_t *protocolclientplugin;



static qintptr_t VARGS Plug_Menu_Control(void *offset, quintptr_t mask, const qintptr_t *arg)
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
		}
		if (VM_LONG(arg[0]) != 1)
			return 1;
		//give us menu control
		menuplug = currentplug;
		Key_Dest_Add(kdm_emenu);
		return 1;
	case 2: //weather it's us or not.
		return currentplug == menuplug && Key_Dest_Has(kdm_emenu);
	case 3:	//weather a menu is active
		return !!Key_Dest_Has(kdm_emenu|kdm_gmenu);
	default:
		return 0;
	}
}

static qintptr_t VARGS Plug_Key_GetKeyCode(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int modifier;
	return Key_StringToKeynum(VM_POINTER(arg[0]), &modifier);
}




static qintptr_t VARGS Plug_SCR_CenterPrint(void *offset, quintptr_t mask, const qintptr_t *arg)
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

static qintptr_t VARGS Plug_Draw_LoadImage(char *name, int type, char *script)
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

static qintptr_t VARGS Plug_Draw_LoadImageData(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	qintptr_t ret = 0;
	char *name = VM_POINTER(arg[0]);
//	char *mimetype = VM_POINTER(arg[1]);
	void *codeddata = VM_POINTER(arg[2]);
	unsigned int datalength = VM_LONG(arg[3]);
	image_t *t;
	qbyte *rgbdata;
	unsigned int width, height;
	uploadfmt_t format;

	if (VM_OOB(arg[2], arg[3]))
		return 0;
		
	if ((rgbdata = ReadRawImageFile(codeddata, datalength, &width, &height, &format, false, name)))
	{
//		name = va("%s", name);
		
		t = Image_FindTexture(name, NULL, IF_PREMULTIPLYALPHA|IF_NOMIPMAP|IF_UIPIC|IF_CLAMP);
		if (!TEXVALID(t))
			t = Image_CreateTexture(name, NULL, IF_PREMULTIPLYALPHA|IF_NOMIPMAP|IF_UIPIC|IF_CLAMP);
		if (TEXVALID(t))
		{
			Image_Upload(t, format, rgbdata, NULL, width, height, IF_PREMULTIPLYALPHA|IF_NOMIPMAP|IF_UIPIC|IF_CLAMP);
			ret = Plug_Draw_LoadImage(name, 3, NULL);
		}
		
		BZ_Free(rgbdata);
	}
	return ret;
}
static qintptr_t VARGS Plug_Draw_LoadImageShader(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *name = VM_POINTER(arg[0]);
	char *script = VM_POINTER(arg[1]);
	return Plug_Draw_LoadImage(name, 2, script);
}
static qintptr_t VARGS Plug_Draw_LoadImagePic(void *offset, quintptr_t mask, const qintptr_t *arg)
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

//int R2D_ImageSize (qhandle_t image, float *w, float *h)
static qintptr_t VARGS Plug_Draw_ImageSize(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	float *w = VM_POINTER(arg[1]), *h = VM_POINTER(arg[2]);
	int iw, ih, ret;
	mpic_t *pic;
	int i;
	
	if (VM_OOB(arg[1], sizeof(*w)) || VM_OOB(arg[2], sizeof(*w)))
		return -1;
	*w = 0;
	*h = 0;
	if (qrenderer == QR_NONE)
		return 0;

	i = VM_LONG(arg[0]);
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

	ret = R_GetShaderSizes(pic, &iw, &ih, true);
	*w = iw;
	*h = ih;
	return ret;
}

//int R2D_Image (float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t image)
static qintptr_t VARGS Plug_Draw_Image(void *offset, quintptr_t mask, const qintptr_t *arg)
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
static qintptr_t VARGS Plug_Draw_Line(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	R2D_Line(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]), VM_FLOAT(arg[2]), VM_FLOAT(arg[3]), NULL);
	return 1;
}
static qintptr_t VARGS Plug_Draw_Character(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int x, y;
	if (qrenderer == QR_NONE)
		return 0;
	Font_BeginString(font_default, arg[0], arg[1], &x, &y);
	Font_DrawChar(x, y, CON_WHITEMASK, 0xe000 | (unsigned int)arg[2]);
	Font_EndString(font_default);
	return 0;
}
static qintptr_t VARGS Plug_Draw_CharacterH(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	float x = VM_FLOAT(arg[0]);
	float y = VM_FLOAT(arg[1]);
	float h = VM_FLOAT(arg[2]);
	unsigned int flags = VM_LONG(arg[3]);
	unsigned int charc = VM_LONG(arg[4]);
	conchar_t cmask = CON_WHITEMASK;
	if (qrenderer == QR_NONE)
		return 0;
	if (flags & 1)
		cmask |= CON_2NDCHARSETTEXT;
	if (!(flags & 2))
		cmask |= 0xe000;
	Font_BeginScaledString(font_default, x, y, h, h, &x, &y);
	Font_DrawScaleChar(x, y, cmask, charc);
	Font_EndString(font_default);
	return 0;
}
static qintptr_t VARGS Plug_Draw_String(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int ipx, px, py;
	conchar_t buffer[2048], *str;
	unsigned int codeflags, codepoint;
	if (qrenderer == QR_NONE)
		return 0;
	COM_ParseFunString(CON_WHITEMASK, VM_POINTER(arg[2]), buffer, sizeof(buffer), false);
	str = buffer;
	Font_BeginString(font_default, VM_FLOAT(arg[0]), VM_FLOAT(arg[1]), &px, &py);
	ipx = px;
	while(*str)
	{
		str = Font_Decode(str, &codeflags, &codepoint);
		if (codepoint == '\n')
			py += Font_CharHeight();
		else if (codepoint == '\r')
			px = ipx;
		else
			px = Font_DrawChar(px, py, codeflags, codepoint);
	}
	Font_EndString(font_default);
	return 0;
}
static qintptr_t VARGS Plug_Draw_StringH(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	float x = VM_FLOAT(arg[0]);
	float y = VM_FLOAT(arg[1]);
	float h = VM_FLOAT(arg[2]);
	unsigned int flags = VM_LONG(arg[3]);
	char *instr = VM_POINTER(arg[4]);
	float ipx;
	conchar_t buffer[2048], *str, cmask = CON_WHITEMASK;
	unsigned int codeflags, codepoint;
	unsigned int parseflags = 0;
	if (qrenderer == QR_NONE)
		return 0;
	if (flags & 1)
		cmask |= CON_2NDCHARSETTEXT;
	if (flags & 2)
		parseflags |= PFS_FORCEUTF8;
	COM_ParseFunString(CON_WHITEMASK, instr, buffer, sizeof(buffer), parseflags);
	str = buffer;
	Font_BeginScaledString(font_default, x, y, h, h, &x, &y);
	ipx = x;
	while(*str)
	{
		str = Font_Decode(str, &codeflags, &codepoint);
		if (codepoint == '\n')
			y += Font_CharScaleHeight();
		else if (codepoint == '\r')
			x = ipx;
		else
			x = Font_DrawScaleChar(x, y, codeflags, codepoint);
	}
	Font_EndString(font_default);
	return 0;
}

static qintptr_t VARGS Plug_Draw_StringWidth(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	qintptr_t ret;
	float h = VM_FLOAT(arg[0]);
	unsigned int flags = VM_LONG(arg[1]);
	char *instr = VM_POINTER(arg[2]);
	conchar_t buffer[2048], *str, cmask = CON_WHITEMASK;
	unsigned int parseflags = 0;
	float px,py;
	if (qrenderer == QR_NONE)
		return 0;
	if (flags & 1)
		cmask |= CON_2NDCHARSETTEXT;
	if (flags & 2)
		parseflags |= PFS_FORCEUTF8;
	str = COM_ParseFunString(CON_WHITEMASK, instr, buffer, sizeof(buffer), parseflags);
	
	Font_BeginScaledString(font_default, 0, 0, h, h, &px, &py);
	px = Font_LineScaleWidth(buffer, str);
	Font_EndString(NULL);
	
	//put it back in virtual space
	VM_FLOAT(ret) = (px*(float)vid.width) / (float)vid.rotpixelwidth;
	return ret;
}

static qintptr_t VARGS Plug_Draw_Fill(void *offset, quintptr_t mask, const qintptr_t *arg)
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
static qintptr_t VARGS Plug_Draw_ColourP(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int p = VM_LONG(arg[0]);
	if (p<0 || p>255)
		return false;

	R2D_ImagePaletteColour(p, 1);
	return 1;
}
static qintptr_t VARGS Plug_Draw_ColourPA(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int p = VM_LONG(arg[0]);
	float a = VM_FLOAT(arg[1]);
	if (p<0 || p>255)
		return false;

	R2D_ImagePaletteColour(p, a);
	return 1;
}
static qintptr_t VARGS Plug_Draw_Colour3f(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	R2D_ImageColours(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]), VM_FLOAT(arg[2]), 1);
	return 1;
}
static qintptr_t VARGS Plug_Draw_Colour4f(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	R2D_ImageColours(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]), VM_FLOAT(arg[2]), VM_FLOAT(arg[3]));
	return 1;
}









static qintptr_t VARGS Plug_LocalSound(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	if (qrenderer == QR_NONE)
		return false;

	S_LocalSound(VM_POINTER(arg[0]));
	return 0;
}



static qintptr_t VARGS Plug_CL_GetStats(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int i = 0;
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
	if (pnum < 0)
	{
		pnum = -pnum-1;
		if (pnum < MAX_CLIENTS)
		{
			for (i = 0; i < max; i++)
				stats[i] = cl.players[pnum].stats[i];
		}
	}
	else if (pnum < cl.splitclients)
	{
		for (i = 0; i < max; i++)
		{	//fill stats with the right player's stats
			stats[i] = cl.playerview[pnum].stats[i];
		}
	}

	max = i;
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
	float activetime;
	int userid;
	int spectator;
	char userinfo[1024];
	char team[8];
} vmplugclientinfo_t;

static qintptr_t VARGS Plug_GetPlayerInfo(void *offset, quintptr_t mask, const qintptr_t *arg)
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
			if (i >= -(int)MAX_SPLITS)
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
		out->activetime = realtime - cl.players[i].realentertime;
		out->userid = cl.players[i].userid;
		out->spectator = cl.players[i].spectator;
		InfoBuf_ToString(&cl.players[i].userinfo, out->userinfo, sizeof(out->userinfo), basicuserinfos, NULL, NULL, NULL, NULL);
		Q_strncpyz(out->team, cl.players[i].team, sizeof(out->team));
	}

	pt = Cam_TrackNum(&cl.playerview[0]);
	if (pt < 0)
		return (cl.playerview[0].playernum == i);
	else
		return pt == i;
}

static qintptr_t VARGS Plug_LocalPlayerNumber(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	return cl.playerview[0].playernum;
}
static qintptr_t VARGS Plug_GetLocalPlayerNumbers(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int i;
	int	first = VM_LONG(arg[0]);
	int	count = VM_LONG(arg[1]);
	int *playernums = VM_POINTER(arg[2]);
	int *spectracks = VM_POINTER(arg[3]);
	if (count < 0 || count > 1000) count = 0;
	if (VM_OOB(arg[2], sizeof(*playernums)*count) || VM_OOB(arg[3], sizeof(*playernums)*count))
		return false;
	if (first < 0) first = 0;
	if (first > cl.splitclients) first = cl.splitclients;
	if (first+count > cl.splitclients) count = cl.splitclients-first;
	for (i = 0; i < count; i++)
	{
		playernums[i] = cl.playerview[i].playernum;
		spectracks[i] = Cam_TrackNum(&cl.playerview[i]);
	}
	return count;
}

static qintptr_t VARGS Plug_GetServerInfo(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *outptr = VM_POINTER(arg[0]);
	unsigned int outlen = VM_LONG(arg[1]);
	extern float demtime;

	if (VM_OOB(arg[0], outlen))
		return false;

	InfoBuf_ToString(&cl.serverinfo, outptr, outlen, NULL, NULL, NULL, NULL, NULL);
	Q_strncatz(outptr, va("\\intermission\\%i", cl.intermissionmode), outlen);
	switch(cls.demoplayback)
	{
	case DPB_NONE:
		break;
	case DPB_MVD:
	case DPB_EZTV:
		Q_strncatz(outptr, "\\demotype\\mvd", outlen);
		break;
	case DPB_QUAKEWORLD:
		Q_strncatz(outptr, "\\demotype\\qw", outlen);
		break;
#ifdef NQPROT
	case DPB_NETQUAKE:
		Q_strncatz(outptr, "\\demotype\\nq", outlen);
		break;
#endif
#ifdef Q2CLIENT
	case DPB_QUAKE2:
		Q_strncatz(outptr, "\\demotype\\q2", outlen);
		break;
#endif
	}
	Q_strncatz(outptr, va("\\demotime\\%f", demtime-cls.demostarttime), outlen);

#ifdef QUAKEHUD
	if (cl.playerview[0].statsf[STAT_MATCHSTARTTIME])
		Q_strncatz(outptr, va("\\matchstart\\%f", cl.playerview[0].statsf[STAT_MATCHSTARTTIME]/1000), outlen);
	else
#endif
		Q_strncatz(outptr, va("\\matchstart\\%f", cl.matchgametimestart), outlen);

	return true;
}

static qintptr_t VARGS Plug_SetUserInfo(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *key = VM_POINTER(arg[0]);
	char *value = VM_POINTER(arg[1]);

	CL_SetInfo(0, key, value);

	return true;
}

typedef struct {
	int seats;
	struct
	{
		float s_avg;
		float s_mn;
		float s_mx;
		float ms_stddev;
		float fr_avg;
		int fr_mn;
		int fr_mx;
	} ping;
	struct
	{	//decimals
		float dropped;
		float choked;
		float invalid;
	} loss;
	float mlatency;
	float mrate;
	float vlatency;
	float vrate;
	vec3_t speed;	//player speed
	
	struct
	{
		float in_pps;
		float in_bps;
		float out_pps;
		float out_bps;
	} clrate;
	struct
	{
		float in_pps;
		float in_bps;
		float out_pps;
		float out_bps;
	} svrate;
	int capturing;
} vmnetinfo_t;
#define has(x) (((quintptr_t)&((vmnetinfo_t*)NULL)->x + sizeof(((vmnetinfo_t*)NULL)->x)) <= outlen)
//aka: misc other hud timing crap
static qintptr_t VARGS Plug_GetNetworkInfo(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	vmnetinfo_t *outptr = VM_POINTER(arg[0]);
	unsigned int outlen = VM_LONG(arg[1]);
	if (VM_OOB(arg[0], outlen))
		return false;

	if (has(capturing))
	{
#ifdef HAVE_MEDIA_ENCODER
		outptr->capturing = Media_Capturing();
#else
		outptr->capturing = 0;
#endif
	}
	
	if (has(seats))
		outptr->seats = cl.splitclients;
	if (has(ping))		
		CL_CalcNet2 (&outptr->ping.s_avg, &outptr->ping.s_mn, &outptr->ping.s_mx, &outptr->ping.ms_stddev, &outptr->ping.fr_avg, &outptr->ping.fr_mn, &outptr->ping.fr_mx, &outptr->loss.dropped, &outptr->loss.choked, &outptr->loss.invalid);
		
	if (has(mlatency))
		outptr->mlatency = 0;
	if (has(mrate))
		outptr->mrate = IN_DetermineMouseRate();
	if (has(vlatency))
		outptr->vlatency = 0;
		
	if (has(speed))
		VectorCopy(outptr->speed, r_refdef.playerview->simvel);

	if (has(clrate))
		NET_GetRates(cls.sockets, &outptr->clrate.in_pps, &outptr->clrate.out_pps, &outptr->clrate.in_bps, &outptr->clrate.out_bps);		
	if (has(svrate))
	{
		memset(&outptr->svrate, 0, sizeof(outptr->svrate));
#ifndef CLIENTONLY
		NET_GetRates(svs.sockets, &outptr->svrate.in_pps, &outptr->svrate.out_pps, &outptr->svrate.in_bps, &outptr->svrate.out_bps);
#endif
	}
	
	return sizeof(vmnetinfo_t);
}
#undef has

#ifdef QUAKEHUD
static qintptr_t VARGS Plug_GetTrackerOwnFrags(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int ret;
	int seat = VM_LONG(arg[0]);
	char *outptr = VM_POINTER(arg[1]);
	size_t outlen = VM_LONG(arg[2]);
	if (VM_OOB(arg[1], outlen) || !outlen)
		VM_FLOAT(ret) = 0;
	else
		VM_FLOAT(ret) = Stats_GetLastOwnFrag(seat, outptr, outlen);
	return ret;
}
#endif

static qintptr_t VARGS Plug_GetLocationName(void *offset, quintptr_t mask, const qintptr_t *arg)
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

#ifdef QUAKEHUD
typedef struct
{
	unsigned int client;
	unsigned int items;
	float armor;
	float health;
	vec3_t org;
	char nick[16];
} teamplayerinfo_t;
static qintptr_t VARGS Plug_GetTeamInfo(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	teamplayerinfo_t *players = VM_POINTER(arg[0]);
	size_t maxplayers = VM_LONG(arg[1]);
	qboolean showenemies = VM_LONG(arg[2]);
	qboolean showself = VM_LONG(arg[3]);
	int count = 0;
	int i;
	int self;
	lerpents_t		*le;
	player_info_t	*pl;

	if (VM_OOB(arg[0], maxplayers*sizeof(*players)))
		return 0;
	maxplayers = min(maxplayers, cl.allocated_client_slots);
	
	Cvar_Get("ti", "1", CVAR_USERINFO, "Hacks because ktx sucks. Must be 1 in order to receive team information in ktx.");
	
	self = cl.playerview[0].playernum;
	if (cl.playerview[0].cam_state != CAM_FREECAM)
		self = cl.playerview[0].cam_spec_track;
	
	for (i = 0; i < cl.allocated_client_slots && maxplayers > 0; i++)
	{
		if (!*cl.players[i].name)	//empty slot
			continue;
		if (cl.players[i].spectator)	//shoo!
			continue;
		if (i == self && !showself)
			continue;
		if (!showenemies && strcmp(cl.players[i].team, cl.players[self].team))
			continue;
		players->client = i;

		pl = &cl.players[i];
		if (pl->tinfo.time > cl.time)
		{	//mod is explicitly telling us this junk
			players->items = pl->tinfo.items;
			players->health = pl->tinfo.health;
			players->armor = pl->tinfo.armour;
			VectorCopy(pl->tinfo.org, players->org);
			Q_strncpyz(players->nick, pl->tinfo.nick, sizeof(players->nick));
		}
		else if (i == self)
		{	//oh hey look, its me.
			players->items = cl.playerview[0].stats[STAT_ITEMS];
			players->armor = cl.playerview[0].statsf[STAT_ARMOR];
			players->health = cl.playerview[0].statsf[STAT_HEALTH];
			Q_strncpyz(players->nick, "", sizeof(players->nick));
		}
		else if (cls.demoplayback == DPB_MVD || cls.demoplayback == DPB_EZTV)
		{	//scrape it from the mvd (assuming there is one...
			players->items = cl.players[i].stats[STAT_ITEMS];
			players->armor = cl.players[i].statsf[STAT_ARMOR];
			players->health = cl.players[i].statsf[STAT_HEALTH];
			Q_strncpyz(players->nick, "", sizeof(players->nick));
			
			VectorClear(players->org);
		}
		else
			continue;	//no stats, don't bother telling the plugin.

		//scrape origin from interpolation, if its more valid.
		if (i+1 < cl.maxlerpents && cl.lerpentssequence && cl.lerpents[i+1].sequence == cl.lerpentssequence)
		{
			le = &cl.lerpents[i+1];
			VectorCopy(le->origin, players->org);
		}
		else if (cl.lerpentssequence && cl.lerpplayers[i].sequence == cl.lerpentssequence)
		{
			le = &cl.lerpplayers[i];
			VectorCopy(le->origin, players->org);
		}

		players++;
		maxplayers--;
		count++;
	}
	
	return VM_LONG(count);
}
#endif
#ifdef QUAKEHUD
static qintptr_t VARGS Plug_GetWeaponStats(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int self = VM_LONG(arg[0]);
	struct wstats_s *result = VM_POINTER(arg[1]);
	size_t maxresults = VM_LONG(arg[2]);

	if (VM_OOB(arg[0], maxresults*sizeof(*result)))
		return 0;
	
	//FIXME: we should support some way to clear this to 0 again, other than nosave.
	Cvar_Get("wpsx", "1", CVAR_USERINFO|CVAR_NOSAVE, "Hacks because ktx sucks. Must be 1 in order to receive weapon stats information in ktx.");

	if (self < 0)
	{
		unsigned int seat = (unsigned)(-self-1)%MAX_SPLITS;
		self = cl.playerview[seat].playernum;
		if (cl.playerview[seat].cam_state != CAM_FREECAM)
			self = cl.playerview[seat].cam_spec_track;
	}
	if (self < 0)
		return 0;

	if (maxresults > countof(cl.players[self].weaponstats))
		maxresults = countof(cl.players[self].weaponstats);
	memcpy(result, cl.players[self].weaponstats, sizeof(*result) * maxresults);
	return VM_LONG(maxresults);
}
#endif

static qintptr_t VARGS Plug_Con_SubPrint(void *offset, quintptr_t mask, const qintptr_t *arg)
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

	Con_PrintCon(con, text, con->parseflags);

	return 1;
}
static qintptr_t VARGS Plug_Con_RenameSub(void *offset, quintptr_t mask, const qintptr_t *arg)
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
static qintptr_t VARGS Plug_Con_IsActive(void *offset, quintptr_t mask, const qintptr_t *arg)
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
static qintptr_t VARGS Plug_Con_SetActive(void *offset, quintptr_t mask, const qintptr_t *arg)
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
static qintptr_t VARGS Plug_Con_Destroy(void *offset, quintptr_t mask, const qintptr_t *arg)
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
static qintptr_t VARGS Plug_Con_NameForNum(void *offset, quintptr_t mask, const qintptr_t *arg)
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

static qintptr_t VARGS Plug_Con_GetConsoleFloat(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *conname = VM_POINTER(arg[0]);
	char *attrib = VM_POINTER(arg[1]);
	int retbuf;
	float ret;
	console_t *con = Con_FindConsole(conname);
	ret = -1;

	if (!con)
		ret = -1;
	else if (!strcmp(attrib, "unseen"))
		ret = con->unseentext;
	else if (!strcmp(attrib, "markup"))	
	{
		if (con->parseflags & PFS_NOMARKUP)
			ret = 0;
		else if (con->parseflags & PFS_KEEPMARKUP)
			ret = 2;
		else
			ret = 1;
	}
	else if (!strcmp(attrib, "forceutf8"))
		ret = (con->parseflags&PFS_FORCEUTF8)?true:false;
	else if (!strcmp(attrib, "hidden"))
		ret = (con->flags & CONF_HIDDEN)?true:false;
	else if (!strcmp(attrib, "iswindow"))
		ret = (con->flags & CONF_ISWINDOW)?true:false;
	else if (!strcmp(attrib, "maxlines"))
		ret = con->maxlines;
	else if (!strcmp(attrib, "wnd_x"))
		ret = con->wnd_x;
	else if (!strcmp(attrib, "wnd_y"))
		ret = con->wnd_y;
	else if (!strcmp(attrib, "wnd_w"))
		ret = con->wnd_w;
	else if (!strcmp(attrib, "wnd_h"))
		ret = con->wnd_h;
	else if (!strcmp(attrib, "linecount"))
		ret = con->linecount;

	VM_FLOAT(retbuf) = ret;
	return retbuf;
}

static qintptr_t VARGS Plug_Con_SetConsoleFloat(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *conname = VM_POINTER(arg[0]);
	char *attrib = VM_POINTER(arg[1]);
	float val = VM_FLOAT(arg[2]);
	console_t *con = Con_FindConsole(conname);

	if (!con)
	{
		con = Con_Create(conname, 0);
		if (!con)
			return -1;
		con->userdata = currentplug;
		con->linebuffered = Plug_SubConsoleCommand;
	}

	if (!strcmp(attrib, "unseen"))
		con->unseentext = !!val;
	else if (!strcmp(attrib, "markup"))	
	{
		int cur = val;
		con->parseflags &= ~(PFS_NOMARKUP|PFS_KEEPMARKUP);
		if (cur == 0)
			con->parseflags |= PFS_NOMARKUP;
		else if (cur == 2)
			con->parseflags |= PFS_KEEPMARKUP;
	}
	else if (!strcmp(attrib, "forceutf8"))
		con->parseflags = (con->parseflags & ~PFS_FORCEUTF8) | (val?PFS_FORCEUTF8:0);
	else if (!strcmp(attrib, "hidden"))
		con->flags = (con->flags & ~CONF_HIDDEN) | (val?CONF_HIDDEN:0);
	else if (!strcmp(attrib, "iswindow"))
	{
		con->flags = (con->flags & ~CONF_ISWINDOW) | (val?CONF_ISWINDOW:0);
		con->flags = (con->flags & ~CONF_NOTIFY) | (val>1?CONF_NOTIFY:0);
		if (con_curwindow == con && !(con->flags & CONF_ISWINDOW))
			con_curwindow = NULL;
		else if (!con_curwindow && (con->flags & CONF_ISWINDOW))
			con_curwindow = con;
	}
	else if (!strcmp(attrib, "maxlines"))
		con->maxlines = val;
	else if (!strcmp(attrib, "wnd_x"))
		con->wnd_x = val;
	else if (!strcmp(attrib, "wnd_y"))
		con->wnd_y = val;
	else if (!strcmp(attrib, "wnd_w"))
		con->wnd_w = val;
	else if (!strcmp(attrib, "wnd_h"))
		con->wnd_h = val;
	else if (!strcmp(attrib, "linebuffered"))
	{
		con->userdata = currentplug;
		if (val == 2)
			con->linebuffered = NULL;//Con_Navigate;
		else if (val == 1)
			con->linebuffered = Plug_SubConsoleCommand;
		else
			con->linebuffered = NULL;
	}
	else if (!strcmp(attrib, "linecount"))
	{
		if (val == 0)
		{
			int pfl = con->parseflags;
			Con_ClearCon(con);
			con->parseflags = pfl;
		}
		else
			return -1;
	}
	else
		return -1;
	return true;
}

static qintptr_t VARGS Plug_Con_GetConsoleString(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	const char *conname = VM_POINTER(arg[0]);
	const char *attrib = VM_POINTER(arg[1]);
	char *value = VM_POINTER(arg[2]);
	size_t size = VM_LONG(arg[3]);
	console_t *con = Con_FindConsole(conname);
	
	if (VM_OOB(arg[2], arg[3]))
		return 0;

	if (!con)
		return 0;
	else if (!strcmp(attrib, "footer"))
		;
	else if (!strcmp(attrib, "title"))
	{
		Q_strncpyz(value, con->title, size);
	}
	else if (!strcmp(attrib, "icon"))
	{
		Q_strncpyz(value, con->icon, size);
	}
	else if (!strcmp(attrib, "prompt"))
	{
		Q_strncpyz(value, con->prompt, size);
	}
	else if (!strcmp(attrib, "backimage"))
	{
		if (con->backshader)
			Q_strncpyz(value, con->backshader->name, size);
		else
			Q_strncpyz(value, con->backimage, size);
	}
	return 0;
}
static qintptr_t VARGS Plug_Con_SetConsoleString(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	const char *conname = VM_POINTER(arg[0]);
	const char *attrib = VM_POINTER(arg[1]);
	const char *value = VM_POINTER(arg[2]);
	console_t *con = Con_FindConsole(conname);

	if (!con)
	{
		con = Con_Create(conname, 0);
		if (!con)
			return -1;
		con->userdata = currentplug;
		con->linebuffered = Plug_SubConsoleCommand;
	}
	if (!con)
		return 0;
	else if (!strcmp(attrib, "footer"))
		Con_Footerf(con, false, "%s", value);
	else if (!strcmp(attrib, "title"))
		Q_strncpyz(con->title, value, sizeof(con->title));
	else if (!strcmp(attrib, "icon"))
		Q_strncpyz(con->icon, value, sizeof(con->icon));
	else if (!strcmp(attrib, "prompt"))
		Q_strncpyz(con->prompt, value, sizeof(con->prompt));
	else if (!strcmp(attrib, "backimage"))
	{
		Q_strncpyz(con->backimage, value, sizeof(con->backimage));
		if (con->backshader)
			R_UnloadShader(con->backshader);
	}
	else if (!strcmp(attrib, "backvideomap"))
	{
		Q_strncpyz(con->backimage, "", sizeof(con->backimage));
		if (con->backshader)
			R_UnloadShader(con->backshader);
		if (qrenderer != QR_NONE)
			con->backshader = R_RegisterCustom(va("consolevid_%s", con->name), SUF_NONE, Shader_DefaultCinematic, value);
		else
			con->backshader = NULL;
	}
	else
		return -1;
	return 0;
}

static qintptr_t VARGS Plug_S_RawAudio(void *offset, quintptr_t mask, const qintptr_t *arg)
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

#include "com_mesh.h"

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
qintptr_t VARGS Plug_Mod_GetPluginModelFuncs(void *offset, quintptr_t mask, const qintptr_t *arg)
{
#ifdef SKELETALMODELS
	static modplugfuncs_t funcs =
	{
		MODPLUGFUNCS_VERSION,
		
		Plug_RegisterModelFormatText,
		Plug_RegisterModelFormatMagic,
		Plug_UnRegisterModelFormat,
		Plug_UnRegisterAllModelFormats,
		ZG_Malloc,

		R_ConcatTransforms,
		Matrix3x4_Invert_Simple,
		VectorAngles,
		AngleVectors,
		GenMatrixPosQuat4Scale,
		COM_StripExtension,
		Alias_ForceConvertBoneData,
#ifdef TERRAIN
		Terr_GetTerrainFuncs,
#else
		NULL,
#endif
		NULL,
		Image_GetTexture,
		FS_OpenVFS
	};
	if (VM_LONG(arg[0]) >= sizeof(funcs))
		return (qintptr_t)&funcs;
	else
#endif
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
	Plug_RegisterBuiltin("Draw_ImageSize",			Plug_Draw_ImageSize, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_Character",			Plug_Draw_Character, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_CharacterH",			Plug_Draw_CharacterH, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_String",				Plug_Draw_String, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_StringH",			Plug_Draw_StringH, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_StringWidth",		Plug_Draw_StringWidth, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_Fill",				Plug_Draw_Fill, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_Line",				Plug_Draw_Line, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_Colourp",			Plug_Draw_ColourP, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_Colourpa",			Plug_Draw_ColourPA, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_Colour3f",			Plug_Draw_Colour3f, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_Colour4f",			Plug_Draw_Colour4f, PLUG_BIF_NEEDSRENDERER);

	Plug_RegisterBuiltin("Con_SubPrint",			Plug_Con_SubPrint, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Con_RenameSub",			Plug_Con_RenameSub, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Con_IsActive",			Plug_Con_IsActive, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Con_SetActive",			Plug_Con_SetActive, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Con_Destroy",				Plug_Con_Destroy, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Con_NameForNum",			Plug_Con_NameForNum, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Con_GetConsoleFloat",		Plug_Con_GetConsoleFloat, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Con_SetConsoleFloat",		Plug_Con_SetConsoleFloat, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Con_GetConsoleString",	Plug_Con_GetConsoleString, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Con_SetConsoleString",	Plug_Con_SetConsoleString, PLUG_BIF_NEEDSRENDERER);

	Plug_RegisterBuiltin("LocalSound",				Plug_LocalSound, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("SCR_CenterPrint",			Plug_SCR_CenterPrint, PLUG_BIF_NEEDSRENDERER);

#ifdef QUAKEHUD
	Plug_RegisterBuiltin("GetTeamInfo",				Plug_GetTeamInfo, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("GetWeaponStats",			Plug_GetWeaponStats, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("GetTrackerOwnFrags",		Plug_GetTrackerOwnFrags, PLUG_BIF_NEEDSRENDERER);
#endif
	Plug_RegisterBuiltin("GetLocationName",			Plug_GetLocationName, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("GetPlayerInfo",			Plug_GetPlayerInfo, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("LocalPlayerNumber",		Plug_LocalPlayerNumber, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("GetLocalPlayerNumbers",	Plug_GetLocalPlayerNumbers, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("GetServerInfo",			Plug_GetServerInfo, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("SetUserInfo",				Plug_SetUserInfo, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("GetNetworkInfo",			Plug_GetNetworkInfo, PLUG_BIF_NEEDSRENDERER);
	
	Plug_RegisterBuiltin("S_RawAudio",				Plug_S_RawAudio, PLUG_BIF_NEEDSRENDERER);
	
	Plug_RegisterBuiltin("Mod_GetPluginModelFuncs",	Plug_Mod_GetPluginModelFuncs, PLUG_BIF_NEEDSRENDERER|PLUG_BIF_DLLONLY);
}

void Plug_Client_Close(plugin_t *plug)
{
	Plug_FreePlugImages(plug);


	if (menuplug == plug)
	{
		menuplug = NULL;
		Key_Dest_Remove(kdm_emenu);
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






