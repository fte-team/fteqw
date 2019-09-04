//included directly from plugin.c
//this is the client-only things.

static plugin_t *protocolclientplugin;


static void PlugMenu_Close(menu_t *m)
{
	Z_Free(m);
}
static qboolean PlugMenu_Event(menu_t *m, int eventtype, int keyparam, int unicodeparam)	//eventtype = draw/keydown/keyup, param = time/key
{
	plugin_t *oc=currentplug;
	qboolean ret;

	currentplug = m->ctx;
	ret = currentplug->menufunction(eventtype, keyparam, unicodeparam, mousecursor_x, mousecursor_y, vid.width, vid.height);
	currentplug=oc;
	return ret;
}
static qboolean PlugMenu_KeyEvent(menu_t *m, qboolean isdown, unsigned int devid, int key, int unicode)
{
	return PlugMenu_Event(m, isdown?1:2, key, unicode);
}
static void PlugMenu_Draw(menu_t *m)
{
	PlugMenu_Event (m, 0, (realtime*1000), 0);
}
static qboolean QDECL Plug_SetMenuFocus (qboolean wantkeyfocus, const char *cursorname, float hot_x, float hot_y, float scale) //null cursorname=relmouse, set/empty cursorname=absmouse
{
	menu_t *m;
	if (qrenderer == QR_NONE)
		return false;

	m = Menu_FindContext(currentplug);

	if (wantkeyfocus)
	{
		if (!m)
		{
			m = Z_Malloc(sizeof(*m));
			m->ctx = currentplug;
			m->cursor = &key_customcursor[kc_plugin];
			m->release = PlugMenu_Close;
			m->keyevent = PlugMenu_KeyEvent;
			m->drawmenu = PlugMenu_Draw;
			Menu_Push(m, false);
		}
	}
	else if (m)
		Menu_Unlink(m);

	if (wantkeyfocus)
	{
		struct key_cursor_s *mc = &key_customcursor[kc_plugin];

		if (cursorname)
		{
			if (scale <= 0)
				scale = 1;
			if (strcmp(cursorname, mc->name) || mc->hotspot[0] != hot_x || mc->hotspot[1] != hot_y || mc->scale != scale)
			{
				Q_strncpyz(mc->name, cursorname, sizeof(mc->name));
				mc->hotspot[0] = hot_x;
				mc->hotspot[1] = hot_y;
				mc->scale = scale;
				mc->dirty = true;
			}
		}
	}
	return true;
}
static qboolean QDECL Plug_HasMenuFocus(void)
{
	return topmenu&&topmenu->ctx==currentplug && Key_Dest_Has(kdm_menu);
}

static int QDECL Plug_Key_GetKeyCode(const char *keyname, int *modifier)
{
	int modifier_;
	if (!modifier)
		modifier = &modifier_;
	return Key_StringToKeynum(keyname, modifier);
}
static const char *QDECL Plug_Key_GetKeyName(int keycode, int modifier)
{
	return Key_KeynumToString(keycode, modifier);
}

const char *QDECL Plug_Key_GetKeyBind(int bindmap, int keynum, int modifier)
{
	return Key_GetBinding(keynum, bindmap, modifier);
}
void QDECL Plug_Key_SetKeyBind(int bindmap, int keycode, int modifier, const char *newbinding)
{
	if (bindmap && !modifier)
		modifier = (bindmap-1) | KEY_MODIFIER_ALTBINDMAP;
	Key_SetBinding (keycode, modifier, newbinding, RESTRICT_LOCAL);
}

/*

static void QDECL Plug_SCR_CenterPrint(int seat, const char *text)
{
	if (qrenderer != QR_NONE)
		SCR_CenterPrint(seat, text, true);
}
*/



typedef struct {
	//Make SURE that the engine has resolved all cvar pointers into globals before this happens.
	plugin_t *plugin;
	char name[64];
	int type;	//cache, wad, shader, raw
	char *script;
	mpic_t *pic;
} pluginimagearray_t;
size_t pluginimagearraylen;
pluginimagearray_t *pluginimagearray;

#include "shader.h"

static void Plug_Draw_UnloadImage(qhandle_t image)
{
	size_t i = image-1;
	if (i < 0 || i >= pluginimagearraylen)
		return;
	if (pluginimagearray[i].plugin == currentplug)
	{
		pluginimagearray[i].plugin = 0;
		if (pluginimagearray[i].pic)
			R_UnloadShader(pluginimagearray[i].pic);
		pluginimagearray[i].pic = NULL;
		pluginimagearray[i].name[0] = '\0';
	}
}
static void Plug_FreePlugImages(plugin_t *plug)
{
	size_t i;
	for (i = 0; i < pluginimagearraylen; i++)
	{
		if (pluginimagearray[i].plugin == plug)
		{
			pluginimagearray[i].plugin = 0;
			if (pluginimagearray[i].pic)
				R_UnloadShader(pluginimagearray[i].pic);
			pluginimagearray[i].pic = NULL;
			pluginimagearray[i].name[0] = '\0';
		}
	}
}

static qhandle_t Plug_Draw_LoadImage(const char *name, int type, const char *script)
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

static qhandle_t QDECL Plug_Draw_LoadImageData(const char *name, const char *mimetype, void *codeddata, size_t datalength)
{
	qhandle_t ret = 0;
	image_t *t;
	qbyte *rgbdata;
	unsigned int width, height;
	uploadfmt_t format;
		
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
static qhandle_t QDECL Plug_Draw_LoadImageShader(const char *name, const char *script)
{
	return Plug_Draw_LoadImage(name, 2, script);
}
static qhandle_t QDECL Plug_Draw_LoadImagePic(const char *name, qboolean type)
{
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

//int R2D_ImageSize (qhandle_t image, float *w, float *h)
static int QDECL Plug_Draw_ImageSize(qhandle_t image, float *w, float *h)
{
	int iw, ih, ret;
	mpic_t *pic;
	int i;
	
	*w = 0;
	*h = 0;
	if (qrenderer == QR_NONE)
		return 0;

	i = image;
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

static int QDECL Plug_Draw_Image(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t image)
{
	mpic_t *pic;
	int i;
	if (qrenderer == QR_NONE)
		return 0;

	i = image;
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

	R2D_Image(x, y, w, h, s1, t1, s2, t2, pic);
	return 1;
}
//x1,y1,x2,y2
static void QDECL Plug_Draw_Line(float x1, float y1, float x2, float y2)
{
	R2D_Line(x1,y1, x2,y2, NULL);
}
static void QDECL Plug_Draw_Character(float x, float y, unsigned int character)
{
	int px, py;
	if (qrenderer == QR_NONE)
		return;
	Font_BeginString(font_default, x, y, &px, &py);
	Font_DrawChar(x, y, CON_WHITEMASK, character);
	Font_EndString(font_default);
}
static void QDECL Plug_Draw_CharacterH(float x, float y, float h, unsigned int flags, unsigned int charc)
{
	conchar_t cmask = CON_WHITEMASK;
	if (qrenderer == QR_NONE)
		return;
	if (flags & 1)
		cmask |= CON_2NDCHARSETTEXT;
	if (!(flags & 2))
		cmask |= 0xe000;
	Font_BeginScaledString(font_default, x, y, h, h, &x, &y);
	Font_DrawScaleChar(x, y, cmask, charc);
	Font_EndString(font_default);
}
static void QDECL Plug_Draw_String(float x, float y, const char *string)
{
	int ipx, px, py;
	conchar_t buffer[2048], *str;
	unsigned int codeflags, codepoint;
	if (qrenderer == QR_NONE)
		return;
	COM_ParseFunString(CON_WHITEMASK, string, buffer, sizeof(buffer), false);
	str = buffer;
	Font_BeginString(font_default, x, y, &px, &py);
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
}
static void QDECL Plug_Draw_StringH(float x, float y, float h, unsigned int flags, const char *instr)
{
	float ipx;
	conchar_t buffer[2048], *str, cmask = CON_WHITEMASK;
	unsigned int codeflags, codepoint;
	unsigned int parseflags = 0;
	if (qrenderer == QR_NONE)
		return;
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
}

static float QDECL Plug_Draw_StringWidth(float h, unsigned int flags, const char *instr)
{
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
	return (px*(float)vid.width) / (float)vid.rotpixelwidth;
}

static void QDECL Plug_Draw_Fill(float x, float y, float width, float height)
{
	if (qrenderer != QR_NONE)
		R2D_FillBlock(x, y, width, height);
}
static void QDECL Plug_Draw_ColourP(int palcol, float a)
{
	if (palcol>=0 && palcol<=255)
		R2D_ImagePaletteColour(palcol, a);
}
static void QDECL Plug_Draw_Colour4f(float r, float g, float b, float a)
{
	R2D_ImageColours(r,g,b,a);
}









static void QDECL Plug_LocalSound(const char *soundname, int channel, float volume)
{
	if (qrenderer != QR_NONE)
		S_LocalSound(soundname);
}



static int QDECL Plug_CL_GetStats(int pnum, unsigned int *stats, int maxstats)
{
	int i = 0;
	int max;

	if (qrenderer == QR_NONE || !cls.state)
		return 0;

	max = maxstats;
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
	for (; i < maxstats; i++)	//plugin has too many stats (wow)
		stats[i] = 0;					//fill the rest.
	return max;
}

static void QDECL Plug_GetPlayerInfo(int playernum, plugclientinfo_t *out)
{
	int i;

	if (playernum < -cl.splitclients || playernum >= MAX_CLIENTS)
		return;

	i = playernum;
	if (out)
	{
		if (i < 0)
		{
			i = cl.playerview[-i-1].playernum;
			if (i < 0)
			{
				memset(out, 0, sizeof(*out));
				return;
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
}

static size_t QDECL Plug_GetLocalPlayerNumbers(size_t first, size_t count, int *playernums, int *spectracks)
{
	size_t i;
	if (count < 0 || count > 1000) count = 0;
	if (first > cl.splitclients) first = cl.splitclients;
	if (first+count > cl.splitclients) count = cl.splitclients-first;
	for (i = 0; i < count; i++)
	{
		playernums[i] = cl.playerview[first+i].playernum;
		spectracks[i] = Cam_TrackNum(&cl.playerview[first+i]);
	}
	return count;
}

static void QDECL Plug_GetServerInfo(char *outptr, size_t outlen)
{
	extern float demtime;

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
}

static void QDECL Plug_SetUserInfo(int seat, const char *key, const char *value)
{
	CL_SetInfo(seat, key, value);
}

static qboolean QDECL Plug_GetLastInputFrame(int seat, usercmd_t *outcmd)
{
	unsigned int curframe = (cl.movesequence-1u) & UPDATE_MASK;
	if (!cl.movesequence || seat < 0 || seat >= cl.splitclients)
		return false;
	*outcmd = cl.outframes[curframe].cmd[seat];
	return true;
}

#define has(x) (((quintptr_t)&((plugnetinfo_t*)NULL)->x + sizeof(((plugnetinfo_t*)NULL)->x)) <= outlen)
//aka: misc other hud timing crap
static size_t QDECL Plug_GetNetworkInfo(plugnetinfo_t *outptr, size_t outlen)
{
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
	
	return min(outlen,sizeof(*outptr));
}
#undef has

#ifdef QUAKEHUD
static float QDECL Plug_GetTrackerOwnFrags(int seat, char *outptr, size_t outlen)
{
	if (!outlen)
		return 0;
	else
		return Stats_GetLastOwnFrag(seat, outptr, outlen);
}
#endif

static void QDECL Plug_GetLocationName(const float *locpoint, char *outbuffer, size_t bufferlen)
{
	const char *result = TP_LocationName(locpoint);
	Q_strncpyz(outbuffer, result, bufferlen);
}

#ifdef QUAKEHUD
static size_t QDECL Plug_GetTeamInfo(teamplayerinfo_t *players, size_t maxplayers, qboolean showenemies, int seat)
{
	int count = 0;
	int i;
	int self;
	lerpents_t		*le;
	player_info_t	*pl;

	maxplayers = min(maxplayers, cl.allocated_client_slots);
	
	Cvar_Get("ti", "1", CVAR_USERINFO, "Hacks because ktx sucks. Must be 1 in order to receive team information in ktx.");
	
	if (seat >= 0)
	{
		self = cl.playerview[seat].playernum;
		if (cl.playerview[seat].cam_state != CAM_FREECAM)
			self = cl.playerview[seat].cam_spec_track;
	}
	else
		self = -1;
	
	for (i = 0; i < cl.allocated_client_slots && maxplayers > 0; i++)
	{
		if (!*cl.players[i].name)	//empty slot
			continue;
		if (cl.players[i].spectator)	//shoo!
			continue;
		if (i == self)
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
			players->items = cl.playerview[seat].stats[STAT_ITEMS];
			players->armor = cl.playerview[seat].statsf[STAT_ARMOR];
			players->health = cl.playerview[seat].statsf[STAT_HEALTH];
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
	
	return count;
}
#endif
#ifdef QUAKEHUD
static int QDECL Plug_GetWeaponStats(int self, struct wstats_s *result, size_t maxresults)
{
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
	return maxresults;
}
#endif

static qboolean QDECL Plug_Con_SubPrint(const char *name, const char *text)
{
	console_t *con;
	if (!name)
		name = "";

	if (qrenderer == QR_NONE)
	{
		if (!*name)
		{
			Con_Printf("%s", text);
			return true;
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

	return true;
}
static qboolean QDECL Plug_Con_RenameSub(const char *oldname, const char *newname)
{
	console_t *con;
	if (qrenderer == QR_NONE)
		return false;
	con = Con_FindConsole(oldname);
	if (!con)
		return false;

	Q_strncpyz(con->name, newname, sizeof(con->name));

	return true;
}
static qboolean QDECL Plug_Con_IsActive(const char *conname)
{
	console_t *con;
	if (qrenderer == QR_NONE)
		return false;
	con = Con_FindConsole(conname);
	if (!con)
		return false;

	return Con_IsActive(con);
}
static qboolean QDECL Plug_Con_SetActive(const char *conname)
{
	console_t *con;
	if (qrenderer == QR_NONE)
		return false;
	con = Con_FindConsole(conname);
	if (!con)
		con = Con_Create(conname, 0);

	Con_SetActive(con);
	return true;
}
static qboolean QDECL Plug_Con_Destroy(const char *conname)
{
	console_t *con;
	if (qrenderer == QR_NONE)
		return false;
	con = Con_FindConsole(conname);
	if (!con)
		return false;

	Con_Destroy(con);
	return true;
}
static qboolean QDECL Plug_Con_NameForNum(qintptr_t connum, char *outconname, size_t connamesize)
{
	if (qrenderer == QR_NONE)
		return false;

	return Con_NameForNum(connum, outconname, connamesize);
}

static float QDECL Plug_Con_GetConsoleFloat(const char *conname, const char *attrib)
{
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

	return ret;
}

static qboolean QDECL Plug_Con_SetConsoleFloat(const char *conname, const char *attrib, float val)
{
	console_t *con = Con_FindConsole(conname);

	if (!con)
	{
		con = Con_Create(conname, 0);
		if (!con)
			return false;
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
			return false;
	}
	else
		return false;
	return true;
}

static qboolean QDECL Plug_Con_GetConsoleString(const char *conname, const char *attrib, char *value, size_t size)
{
	console_t *con = Con_FindConsole(conname);

	if (!con)
		return false;
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
	else
		return false;
	return true;
}
static qboolean QDECL Plug_Con_SetConsoleString(const char *conname, const char *attrib, const char *value)
{
	console_t *con = Con_FindConsole(conname);

	if (!con)
	{
		con = Con_Create(conname, 0);
		if (!con)
			return false;
		con->userdata = currentplug;
		con->linebuffered = Plug_SubConsoleCommand;
	}
	if (!con)
		return false;
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
		return false;
	return true;
}

static void QDECL Plug_S_RawAudio(int sourceid, void *data, int speed, int samples, int channels, int width, float volume)
{
	S_RawAudio(sourceid, data, speed, samples, channels, width, volume);
}

static void Plug_Client_Close(plugin_t *plug)
{
	menu_t *m = Menu_FindContext(currentplug);
	Plug_FreePlugImages(plug);

	if (m)
		Menu_Unlink(m);
	if (protocolclientplugin == plug)
	{
		protocolclientplugin = NULL;
		if (cls.protocol == CP_PLUGIN)
			cls.protocol = CP_UNKNOWN;
	}
}

static void Plug_Client_Shutdown(void)
{
	BZ_Free(pluginimagearray);
	pluginimagearray = NULL;
	pluginimagearraylen = 0;
}






