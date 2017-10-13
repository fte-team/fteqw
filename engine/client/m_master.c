#include "quakedef.h"

#if defined(CL_MASTER) && !defined(NOBUILTINMENUS)
#include "cl_master.h"
#include "shader.h"

//filtering
static cvar_t	sb_sortcolumn		= CVARF("sb_sortcolumn",	"0",	CVAR_ARCHIVE);
static cvar_t	sb_filtertext		= CVARF("sb_filtertext",	"",		CVAR_NOSAVE);
static cvar_t	sb_hideempty		= CVARF("sb_hideempty",		"0",	CVAR_ARCHIVE);
static cvar_t	sb_hidenotempty		= CVARF("sb_hidenotempty",	"0",	CVAR_ARCHIVE);
static cvar_t	sb_hidefull			= CVARF("sb_hidefull",		"0",	CVAR_ARCHIVE);
static cvar_t	sb_hidedead			= CVARF("sb_hidedead",		"1",	CVAR_ARCHIVE);
static cvar_t	sb_hidenetquake		= CVARF("sb_hidenetquake",	"0",	CVAR_ARCHIVE);
static cvar_t	sb_hidequakeworld	= CVARF("sb_hidequakeworld","0",	CVAR_ARCHIVE);
static cvar_t	sb_hideproxies		= CVARF("sb_hideproxies",	"1",	CVAR_ARCHIVE);

static cvar_t	sb_showping			= CVARF("sb_showping",		"1",	CVAR_ARCHIVE);
static cvar_t	sb_showaddress		= CVARF("sb_showaddress",	"0",	CVAR_ARCHIVE);
static cvar_t	sb_showmap			= CVARF("sb_showmap",		"0",	CVAR_ARCHIVE);
static cvar_t	sb_showgamedir		= CVARF("sb_showgamedir",	"0",	CVAR_ARCHIVE);
static cvar_t	sb_showplayers		= CVARF("sb_showplayers",	"1",	CVAR_ARCHIVE);
static cvar_t	sb_showfraglimit	= CVARF("sb_showfraglimit",	"0",	CVAR_ARCHIVE);
static cvar_t	sb_showtimelimit	= CVARF("sb_showtimelimit",	"0",	CVAR_ARCHIVE);

static cvar_t	sb_alpha	= CVARF("sb_alpha",	"0.7",	CVAR_ARCHIVE);

vrect_t joinbutton, specbutton;
static float refreshedtime;
static int isrefreshing;
static int serverpreview;
extern cvar_t slist_writeserverstxt;
extern cvar_t slist_cacheinfo;

static void CalcFilters(menu_t *menu);

void M_Serverlist_Init(void)
{
	char *grp = "Server Browser Vars";

	Cvar_Register(&sb_alpha, grp);
	Cvar_Register(&sb_hideempty, grp);
	Cvar_Register(&sb_hidenotempty, grp);
	Cvar_Register(&sb_hidefull, grp);
	Cvar_Register(&sb_hidedead, grp);
	Cvar_Register(&sb_hidenetquake, grp);
	Cvar_Register(&sb_hidequakeworld, grp);
	Cvar_Register(&sb_hideproxies, grp);
	Cvar_Register(&sb_filtertext, grp);
	Cvar_Register(&sb_sortcolumn, grp);

	Cvar_Register(&sb_showping, grp);
	Cvar_Register(&sb_showaddress, grp);
	Cvar_Register(&sb_showmap, grp);
	Cvar_Register(&sb_showgamedir, grp);
	Cvar_Register(&sb_showplayers, grp);
	Cvar_Register(&sb_showfraglimit, grp);
	Cvar_Register(&sb_showtimelimit, grp);

	Cvar_Register(&slist_writeserverstxt, grp);
	Cvar_Register(&slist_cacheinfo, grp);
}

typedef struct {
	int servers_top;
	int visibleslots;
	int scrollpos;
	int selectedpos;
	int filtermodcount;

	int numslots;
	qboolean stillpolling;
	qbyte filter[8];
	menuedit_t *filtertext;

	char refreshtext[64];

	qboolean sliderpressed;

	menupicture_t *mappic;
} serverlist_t;

static void SL_DrawColumnTitle (int *x, int y, int xlen, int mx, char *str, qboolean recolor, qbyte clr, qboolean *filldraw)
{
	int xmin;

	if (x == NULL)
		xmin = 0;
	else
		xmin = (*x - xlen);

	if (recolor)
		str = va("^&%c-%s", clr, str);
	if (mx >= xmin && !(*filldraw))
	{
		*filldraw = true;
		R2D_ImageColours((sin(realtime*4.4)*0.25)+0.5, (sin(realtime*4.4)*0.25)+0.5, 0.08, 1.0);
		R2D_FillBlock(xmin, y, xlen, 8);
		R2D_ImageColours(1,1,1,1);
	}
	Draw_FunStringWidth(xmin, y, str, xlen, false, false);

	if (x != NULL)
		*x -= xlen + 8;
}

static void SL_TitlesDraw (int x, int y, menucustom_t *ths, menu_t *menu)
{
	int sf = Master_GetSortField();
	int mx = mousecursor_x;
	qboolean filldraw = false;
	qbyte clr;

	if (Master_GetSortDescending())
		clr = 'D';
	else
		clr = 'B';
	x = ths->common.width;
	if (mx > x || mousecursor_y < y || mousecursor_y >= y+8)
		filldraw = true;
	if (sb_showtimelimit.value)	{SL_DrawColumnTitle(&x, y, 3*8, mx, "tl", (sf==SLKEY_TIMELIMIT), clr, &filldraw);}
	if (sb_showfraglimit.value)	{SL_DrawColumnTitle(&x, y, 3*8, mx, "fl", (sf==SLKEY_FRAGLIMIT), clr, &filldraw);}
	if (sb_showplayers.value)	{SL_DrawColumnTitle(&x, y, 5*8, mx, "plyrs", (sf==SLKEY_NUMHUMANS), clr, &filldraw);}
	if (sb_showmap.value)		{SL_DrawColumnTitle(&x, y, 8*8, mx, "map", (sf==SLKEY_MAP), clr, &filldraw);}
	if (sb_showgamedir.value)	{SL_DrawColumnTitle(&x, y, 8*8, mx, "gamedir", (sf==SLKEY_GAMEDIR), clr, &filldraw);}
	if (sb_showping.value)		{SL_DrawColumnTitle(&x, y, 3*8, mx, "png", (sf==SLKEY_PING), clr, &filldraw);}
	if (sb_showaddress.value)	{SL_DrawColumnTitle(&x, y, 21*8, mx, "address", (sf==SLKEY_ADDRESS), clr, &filldraw);}
	SL_DrawColumnTitle(NULL, y, x, mx, "hostname ", (sf==SLKEY_NAME), clr, &filldraw);
}

static qboolean SL_TitlesKey (menucustom_t *ths, menu_t *menu, int key, unsigned int unicode)
{
	int x;
	int mx = mousecursor_x/8;
	int sortkey;
	qboolean descending;
	char sortchar = 0;

	if (key != K_MOUSE1)
		return false;

	do {
		x = ths->common.width/8;
		if (mx > x) return false;	//out of bounds
		if (sb_showtimelimit.value)	{x-=4;if (mx > x) {sortkey = SLKEY_TIMELIMIT;	sortchar='t';	break;}}
		if (sb_showfraglimit.value)	{x-=4;if (mx > x) {sortkey = SLKEY_FRAGLIMIT;	sortchar='f';	break;}}
		if (sb_showplayers.value)	{x-=6;if (mx > x) {sortkey = SLKEY_NUMHUMANS;	sortchar='p';	break;}}
		if (sb_showmap.value)		{x-=9;if (mx > x) {sortkey = SLKEY_MAP;			sortchar='m';	break;}}
		if (sb_showgamedir.value)	{x-=9;if (mx > x) {sortkey = SLKEY_GAMEDIR;		sortchar='g';	break;}}
		if (sb_showping.value)		{x-=4;if (mx > x) {sortkey = SLKEY_PING;		sortchar='l';	break;}}
		if (sb_showaddress.value)	{x-=22;if (mx > x) {sortkey = SLKEY_ADDRESS;	sortchar='a';	break;}}
														sortkey = SLKEY_NAME;		sortchar='n';	break;
	} while (0);

//	if (sortkey == SLKEY_ADDRESS)
//		return true;

	switch(sortkey)
	{
	case SLKEY_NUMPLAYERS:
	case SLKEY_NUMHUMANS:
		//favour descending order (low first)
		descending = Master_GetSortField()!=sortkey||!Master_GetSortDescending();
		break;
	default:
		//favour ascending order (low first)
		descending = Master_GetSortField()==sortkey&&!Master_GetSortDescending();
		break;
	}
	if (descending)
		Cvar_Set(&sb_sortcolumn, va("-%c", sortchar));
	else
		Cvar_Set(&sb_sortcolumn, va("+%c", sortchar));
	Master_SetSortField(sortkey, descending);
	Master_SortServers();
	return true;
}

typedef enum {
	ST_NORMALQW,
	ST_FTESERVER,
	ST_QUAKE2,
	ST_QUAKE3,
	ST_NETQUAKE,
	ST_QTV,
	ST_PROXY,
	ST_FAVORITE,
	MAX_SERVERTYPES
} servertypes_t;

static float serverbackcolor[MAX_SERVERTYPES * 2][3] =
{
	{0.08, 0.08, 0.08}, // default
	{0.16, 0.16, 0.16},
	{0.14, 0.07, 0.07}, // FTE server
	{0.28, 0.14, 0.14},
	{0.04, 0.09, 0.04}, // Quake 2
	{0.08, 0.18, 0.08},
	{0.05, 0.05, 0.12}, // Quake 3
	{0.10, 0.10, 0.24},
	{0.12, 0.08, 0.02}, // NetQuake
	{0.24, 0.16, 0.04},
	{0.10, 0.05, 0.10}, // FTEQTV
	{0.20, 0.10, 0.20},
	{0.10, 0.05, 0.10}, // qizmo
	{0.20, 0.10, 0.20},
	{0.01, 0.13, 0.13}, // Favorite
	{0.02, 0.26, 0.26}
};

static float serverhighlight[MAX_SERVERTYPES][3] =
{
	{0.35, 0.35, 0.45}, // Default
	{0.60, 0.30, 0.30}, // FTE Server
	{0.25, 0.45, 0.25}, // Quake 2
	{0.20, 0.20, 0.60}, // Quake 3
	{0.40, 0.40, 0.25}, // NetQuake
	{0.45, 0.20, 0.45}, // FTEQTV
	{0.45, 0.20, 0.45}, // qizmo
	{0.10, 0.60, 0.60}  // Favorite
};

static servertypes_t flagstoservertype(int flags)
{
	if (flags & SS_FAVORITE)
		return ST_FAVORITE;
	if (flags & SS_PROXY)
	{
		if (flags & SS_FTESERVER)
			return ST_QTV;
		else
			return ST_PROXY;
	}
#ifdef _DEBUG
	if (flags & SS_FTESERVER)
		return ST_FTESERVER;
#endif


	switch(flags & SS_PROTOCOLMASK)
	{
	case SS_NETQUAKE:
	case SS_DARKPLACES:
		return ST_NETQUAKE;
	case SS_QUAKE2:
		return ST_QUAKE2;
	case SS_QUAKE3:
		return ST_QUAKE3;
	case SS_QUAKEWORLD:
		return ST_NORMALQW;
	case SS_UNKNOWN:
		return ST_NORMALQW;
	default:
		return ST_FTESERVER;	//bug
	}
}

static void SL_ServerDraw (int x, int y, menucustom_t *ths, menu_t *menu)
{
	serverlist_t *info = (serverlist_t*)(menu + 1);
	serverinfo_t *si;
	int thisone = ths->dint + info->scrollpos;
	servertypes_t stype;
	char adr[MAX_ADR_SIZE];

	if (sb_filtertext.modified != info->filtermodcount)
		CalcFilters(menu);

	si = Master_SortedServer(thisone);
	if (si)
	{
		x = ths->common.width;
		stype = flagstoservertype(si->special);
		if (thisone == info->selectedpos)
		{
			R2D_ImageColours(
				serverhighlight[(int)stype][0],
				serverhighlight[(int)stype][1],
				serverhighlight[(int)stype][2],
				1.0);
		}
		else if (thisone == info->scrollpos + (int)(mousecursor_y-info->servers_top)/8 && mousecursor_x < x)
			R2D_ImageColours((sin(realtime*4.4)*0.25)+0.5, (sin(realtime*4.4)*0.25)+0.5, 0.08, sb_alpha.value);
		else if (selectedserver.inuse && NET_CompareAdr(&si->adr, &selectedserver.adr))
			R2D_ImageColours(((sin(realtime*4.4)*0.25)+0.5) * 0.5, ((sin(realtime*4.4)*0.25)+0.5)*0.5, 0.08*0.5, sb_alpha.value);
		else
		{
			R2D_ImageColours(
				serverbackcolor[(int)stype * 2 + (thisone & 1)][0],
				serverbackcolor[(int)stype * 2 + (thisone & 1)][1],
				serverbackcolor[(int)stype * 2 + (thisone & 1)][2],
				sb_alpha.value);
		}
		R2D_FillBlock(0, y, ths->common.width, 8);

		R2D_ImageColours(1,1,1,1);
		if (sb_showtimelimit.value)	{Draw_FunStringWidth((x-3*8), y, va("%i", si->tl), 3*8, false, false); x-=4*8;}
		if (sb_showfraglimit.value)	{Draw_FunStringWidth((x-3*8), y, va("%i", si->fl), 3*8, false, false); x-=4*8;}
		if (sb_showplayers.value)	{Draw_FunStringWidth((x-5*8), y, va("%2i/%2i", si->numhumans, si->maxplayers), 5*8, false, false); x-=6*8;}
		if (sb_showmap.value)		{Draw_FunStringWidth((x-8*8), y, si->map, 8*8, false, false); x-=9*8;}
		if (sb_showgamedir.value)	{Draw_FunStringWidth((x-8*8), y, si->gamedir, 8*8, false, false); x-=9*8;}
		if (sb_showping.value)		{Draw_FunStringWidth((x-3*8), y, va("%i", si->ping), 3*8, false, false); x-=4*8;}
		if (sb_showaddress.value)	{Draw_FunStringWidth((x-21*8), y, NET_AdrToString(adr, sizeof(adr), &si->adr), 21*8, false, false); x-=22*8;}
		Draw_FunStringWidth(0, y, si->name, x, false, false);
	}
}
void MC_EditBox_Key(menuedit_t *edit, int key, unsigned int unicode);
static qboolean SL_ServerKey (menucustom_t *ths, menu_t *menu, int key, unsigned int unicode)
{
	static int lastclick;
	int curtime;
	int oldselection;
	serverlist_t *info = (serverlist_t*)(menu + 1);
	serverinfo_t *server;
	extern qboolean	keydown[];
	qboolean ctrl = keydown[K_LCTRL] || keydown[K_RCTRL];

	if (key == K_MOUSE1)
	{
		oldselection = info->selectedpos;
		info->selectedpos = info->scrollpos + (mousecursor_y-info->servers_top)/8;
		server = Master_SortedServer(info->selectedpos);

		selectedserver.inuse = true;
		SListOptionChanged(server);

		if (server)
		{
			snprintf(info->mappic->picturename, 32, "levelshots/%s", server->map);
			if (!*server->map || !R2D_SafeCachePic(info->mappic->picturename))
				snprintf(info->mappic->picturename, 32, "levelshots/nomap");
		}
		else
		{
			snprintf(info->mappic->picturename, 32, "levelshots/nomap");
			return true;
		}

		curtime = Sys_Milliseconds();
		if (lastclick > curtime || lastclick < curtime-250)
		{	//shouldn't happen, or too old a click
			lastclick = curtime;
			return true;
		}
		if (oldselection == info->selectedpos)
			serverpreview = true;
		return true;
	}

	else if (ctrl && key == 'f')
	{
		server = Master_SortedServer(info->selectedpos);
		if (server)
		{
			server->special ^= SS_FAVORITE;
		}
	}

	else if (key == K_ENTER || key == K_KP_ENTER || key == K_GP_START || (ctrl && (key == 's' || key == 'j')) || key == K_SPACE)
	{
		server = Master_SortedServer(info->selectedpos);
		if (server)
		{
			serverpreview = true;
			selectedserver.inuse = true;
			SListOptionChanged(server);
		}
		return true;
	}
	else
	{
		MC_EditBox_Key(info->filtertext, key, unicode);
		return true;
	}
	return false;
}
static void SL_PreDraw	(menu_t *menu)
{
	serverlist_t *info = (serverlist_t*)(menu + 1);
	Master_CheckPollSockets();

	if (isrefreshing)
	{
		if (!CL_QueryServers())
		{
			//extra second, to ensure we got replies
			if (isrefreshing != 2)
			{
				isrefreshing = 2;
				refreshedtime = Sys_DoubleTime()+1;
			}
		}
		else
			isrefreshing = 1;	//something new came up

		if (isrefreshing == 2)
		{
			if (refreshedtime < Sys_DoubleTime())
			{
				isrefreshing = false;
				Master_SortServers();
			}
		}

	}

	info->numslots = Master_NumSorted();
	snprintf(info->refreshtext, sizeof(info->refreshtext), "Refresh - %u/%u/%u\n", info->numslots, Master_NumAlive(), Master_TotalCount());
}
void NET_SendPollPacket(int len, void *data, netadr_t to);
static void SL_PostDraw	(menu_t *menu)
{
	static char *helpstrings[] =
	{
		"rmb: cancel",
		"j: join",
		"o: observe",
		"b: join with automatic best route",
		"v: say server info",
		"ctrl-v: say_team server info",
		"c: copy server info to clipboard",
		"ctrl-c: copy server info only to clipboard",
		"i: view serverinfo",
		"k: toggle this info"
	};

	char buf[64];
	serverlist_t *info = (serverlist_t*)(menu + 1);
	Master_CheckPollSockets();

	if (serverpreview)
	{
		serverinfo_t *server = selectedserver.inuse?Master_InfoForServer(&selectedserver.adr):NULL;
		int h = 0;
		int w = 240;
		if (server && selectedserver.refreshtime < realtime)
		{
			selectedserver.refreshtime = realtime + 4;
			server->sends++;
#ifdef NQPROT
			//we might have gotten stuck. reset the poll
			if ((server->special&SS_PROTOCOLMASK) == SS_NETQUAKE)
			{	//start spamming the server to get all of its details. silly protocols.
				selectedserver.lastplayer = 0;
				*selectedserver.lastrule = 0;

				SZ_Clear(&net_message);
				net_message.packing = SZ_RAWBYTES;
				net_message.currentbit = 0;
				MSG_WriteLong(&net_message, 0);// save space for the header, filled in later
				MSG_WriteByte(&net_message, CCREQ_PLAYER_INFO);
				MSG_WriteByte(&net_message, selectedserver.lastplayer);
				*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
				NET_SendPollPacket(net_message.cursize, net_message.data, server->adr);
				SZ_Clear(&net_message);
				MSG_WriteLong(&net_message, 0);// save space for the header, filled in later
				MSG_WriteByte(&net_message, CCREQ_RULE_INFO);
				MSG_WriteString(&net_message, selectedserver.lastrule);
				*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
				NET_SendPollPacket(net_message.cursize, net_message.data, server->adr);
				SZ_Clear(&net_message);
			}
			else
#endif
				Master_QueryServer(server);
		}
		R2D_ImageColours(1,1,1,1);
		if (server && server->moreinfo)
		{
			int lx, x, y, i;
			int skins = 0;
			if (serverpreview == 4)
			{
				//count the number of proxies the best route will need
				serverinfo_t *prox;
				for (h = 1, prox = server; prox; h++, prox = prox->prevpeer)
					;
				w += 120;
			}
			else if (serverpreview == 3)
				h = countof(helpstrings);
			else if (serverpreview == 2)
			{
				for (i = 0; ; i++)
				{
					char *key = Info_KeyForNumber(server->moreinfo->info, i);
					if (!strcmp(key, "hostname") || !strcmp(key, "status"))	//these are part of the header
						;
					else if (*key)
						h++;
					else
						break;
				}
			}
			else
			{
				h += server->moreinfo->numplayers+2;

				for (i = 0; i < server->moreinfo->numplayers; i++)
				{
					if (*server->moreinfo->players[i].skin && strcmp(server->moreinfo->players[i].skin, "base"))
					{
						skins = true;
						w += 8*8+8;
						break;
					}
				}
			}
			h += 4;
			h *= 8;

			Draw_TextBox(vid.width/2 - w/2-12, vid.height/2 - h/2 - 8-8, w/8+1, h/8+1);

			lx = vid.width/2 - w/2;
			y = vid.height/2 - h/2 - 4;

			x = lx;
			Draw_FunStringWidth (x, y, Info_ValueForKey(server->moreinfo->info, "hostname"), w, 2, false);
			y += 8;
			Draw_FunStringWidth (x, y, Info_ValueForKey(server->moreinfo->info, "status"), w, 2, false);
			y += 8;
			Draw_FunStringWidth (x, y, NET_AdrToString(buf, sizeof(buf), &server->adr), w, 2, false);
			y += 8;

			Draw_FunStringWidth (x, y, "^Ue01d^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01f", w, 2, false);
			y+=8;

			if (serverpreview == 4)
			{
				serverinfo_t *prox;
				for (prox = server; prox; prox = prox->prevpeer)
				{
					Draw_FunStringWidth (x, y, va("%i", prox->cost), 32-8, true, false);
					Draw_FunStringWidth (x + 32, y, NET_AdrToString(buf, sizeof(buf), &prox->adr), w/2 - 8 - 32, true, false);
					Draw_FunStringWidth (x + w/2, y, prox->name, w/2, false, false);
					y += 8;
				}
			}
			else if (serverpreview == 3)
			{
				x = lx;
				for (i = 0; i < countof(helpstrings); i++)
				{
					Draw_FunStringWidth (x, y, helpstrings[i], w, false, false);
					y += 8;
				}
			}
			else if (serverpreview == 2)
			{
				for (i = 0; ; i++)
				{
					char *key = Info_KeyForNumber(server->moreinfo->info, i);
					if (!strcmp(key, "hostname") || !strcmp(key, "status"))
						;
					else if (*key)
					{
						char *value = Info_ValueForKey(server->moreinfo->info, key);
						x = lx;
						Draw_FunStringWidth (x, y, key, w/2 - 8, true, true);
						x+=w/2;
						Draw_FunStringWidth (x, y, value, w/2, false, false);
						y += 8;
					}
					else
						break;
				}
			}
			else
			{
				int teamplay = atoi(Info_ValueForKey(server->moreinfo->info, "teamplay"));
				x = lx;
				Draw_FunStringWidth (x, y, "^mFrgs", 28, true, false);
				x += 32+8;
				Draw_FunStringWidth (x, y, "^mPng", 28, true, false);
				x += 3*8+8;

				if (teamplay)
				{
					Draw_FunStringWidth (x, y, "^mTeam", 4*8, false, false);
					x += 4*8+8;
					Draw_FunStringWidth (x, y, "^mName", 12*8, false, false);
				}
				else
				{
					Draw_FunStringWidth (x, y, "^mName", 16*8, false, false);
				}
				if (skins)
				{
					Draw_FunStringWidth (lx+w-(8*8+8), y, "^mSkin", 8*8, false, false);
					x = lx+w;
				}

				y+=8;
				for (i = 0; i < server->moreinfo->numplayers; i++)
				{
					x = lx;
					if (server->moreinfo->players[i].isspec&1)
						Draw_FunStringWidth (x, y, "spec", 32, true, false);
					else
					{
						R2D_ImagePaletteColour (Sbar_ColorForMap(server->moreinfo->players[i].topc), 1.0);
						R2D_FillBlock (x, y+1, 32, 3);
						R2D_ImagePaletteColour (Sbar_ColorForMap(server->moreinfo->players[i].botc), 1.0);
						R2D_FillBlock (x, y+4, 32, 4);
						R2D_ImageColours (1.0, 1.0, 1.0, 1.0);
						Draw_FunStringWidth (x, y, va("%3i", server->moreinfo->players[i].frags), 32-4, true, false);
					}
					x += 32+8;
					if (server->moreinfo->players[i].isspec&2)
						Draw_FunStringWidth (x-8, y, "bot", 3*8+8, true, false);
					else
						Draw_FunStringWidth (x-8, y, va("%3i", server->moreinfo->players[i].ping), 3*8+8, true, false);
					x += 3*8+8;

					if (teamplay)
					{
						Draw_FunStringWidth (x, y, server->moreinfo->players[i].team, 4*8, false, false);
						x += 4*8+8;
					}

					if (skins)
					{
						Draw_FunStringWidth (x, y, server->moreinfo->players[i].name, lx+w-(8*8+8)-x, false, false);
						x += lx+w-(8*8+8)-x;
						Draw_FunStringWidth (x, y, server->moreinfo->players[i].skin, 8*8, false, false);
					}
					else
						Draw_FunStringWidth (x, y, server->moreinfo->players[i].name, lx+w-x, false, false);

					y += 8;
				}

				Draw_FunStringWidth (lx, y, "^h(left/rightarrow for different info)", w, false, false);
			}
		}
		else
		{
			Draw_TextBox(vid.width/2 - 100-12, vid.height/2 - 32, 200/8, 64/8);
			Draw_FunStringWidth(vid.width/2 - 100, vid.height/2 - 8, "Querying server", 200, 2, false);
			Draw_FunStringWidth(vid.width/2 - 100, vid.height/2 + 0, "Please wait", 200, 2, false);
		}

		if ((server->special & SS_PROTOCOLMASK) == SS_QUAKEWORLD)
		{
			int lx = vid.width/2 - w/2;
			int y = vid.height/2 - h/2 - 4 + h;
			int bh, bw;
			qboolean active = false;
			bw = w+16+12;
			bh = 24;
//			lx += bw-12;
			bw = strlen("Observe")*8 + 24;
			bw = ((bw+15)/16) * 16;	//width must be a multiple of 16
//			lx -= bw;

			specbutton.x = lx;
			specbutton.y = y;
			specbutton.width = bw + 16;
			specbutton.height = bh + 16;
			R2D_ImageColours(1,1,1,1);
			Draw_TextBox(lx, y, bw/8, bh/8);
			y += 8;
			lx += 8;

			if (mousecursor_x >= specbutton.x && mousecursor_x < specbutton.x+specbutton.width)
				if (mousecursor_y >= specbutton.y && mousecursor_y < specbutton.y+specbutton.height)
					active = true;

			Draw_FunStringWidth(lx, y + (bh-8)/2, "Observe", bw, 2, active);y+=8;
		}

		{
			int lx = vid.width/2 - w/2;
			int y = vid.height/2 - h/2 - 4 + h;
			int bw, bh;
			qboolean active = false;
			bw = w+16;
			bh = 24;
			lx += w-12;
			bw = strlen("Join")*8 + 24;
			bw = ((bw+15)/16) * 16;	//width must be a multiple of 16
			lx -= bw;

			joinbutton.x = lx;
			joinbutton.y = y;
			joinbutton.width = bw + 16;
			joinbutton.height = bh + 16;
			R2D_ImageColours(1,1,1,1);
			Draw_TextBox(lx, y, bw/8, bh/8);
			y += 8;
			lx += 8;

			if (mousecursor_x >= joinbutton.x && mousecursor_x < joinbutton.x+joinbutton.width)
				if (mousecursor_y >= joinbutton.y && mousecursor_y < joinbutton.y+joinbutton.height)
					active = true;

			Draw_FunStringWidth(lx, y + (bh-8)/2, "Join", bw, 2, active);y+=8;
		}
	}
	else if (isrefreshing)
	{
		R2D_ImageColours(1,1,1,1);
		Draw_TextBox(vid.width/2 - 100-12, vid.height/2 - 32, 200/8, 64/8);
		Draw_FunStringWidth(vid.width/2 - 100, vid.height/2 - 8, "Refreshing, please wait", 200, 2, false);
		Draw_FunStringWidth(vid.width/2 - 100, vid.height/2 + 0, va("%i of %i", Master_NumPolled(), Master_TotalCount()), 200, 2, false);
	}
	else if (!info->numslots)
	{
		R2D_ImageColours(1,1,1,1);
		if (!Master_TotalCount())
		{
			Draw_FunStringWidth(0, vid.height/2 - 8, "No servers found", vid.width, 2, false);
			Draw_FunStringWidth(0, vid.height/2 + 0, "Check internet connection", vid.width, 2, false);
		}
		else if (!Master_NumAlive())
		{
			Draw_FunStringWidth(0, vid.height/2 - 8, "No servers responding", vid.width, 2, false);
			Draw_FunStringWidth(0, vid.height/2 + 0, "Check udp internet connection", vid.width, 2, false);
		}
		else
		{
			Draw_FunStringWidth(0, vid.height/2 - 8, "All servers were filtered out", vid.width, 2, false);
			Draw_FunStringWidth(0, vid.height/2 + 0, "Change filter settings", vid.width, 2, false);
		}
	}
}
static qboolean SL_Key	(int key, menu_t *menu)
{
	serverlist_t *info = (serverlist_t*)(menu + 1);

	if (serverpreview)
	{
		char buf[64];
		serverinfo_t *server = selectedserver.inuse?Master_InfoForServer(&selectedserver.adr):NULL;
		extern qboolean	keydown[];
		qboolean ctrldown = keydown[K_LCTRL] || keydown[K_RCTRL];

		if (key == K_ESCAPE || key == K_GP_BACK || key == K_MOUSE2)
		{
			serverpreview = false;
			return true;
		}
		else if (key == K_MOUSE1)
		{
			if (mousecursor_x >= joinbutton.x && mousecursor_x < joinbutton.x+joinbutton.width)
				if (mousecursor_y >= joinbutton.y && mousecursor_y < joinbutton.y+joinbutton.height)
				{
					serverpreview = false;
					goto dojoin;
				}
			if (mousecursor_x >= specbutton.x && mousecursor_x < specbutton.x+joinbutton.width)
				if (mousecursor_y >= specbutton.y && mousecursor_y < specbutton.y+joinbutton.height)
				{
					serverpreview = false;
					goto dospec;
				}
			return true;
		}
		else if (key == 'i')
		{
			serverpreview = ((serverpreview==2)?1:2);
			return true;
		}
		else if (key == 'k')
		{
			serverpreview = ((serverpreview==3)?1:3);
			return true;
		}
		else if (key == K_LEFTARROW || key == K_KP_LEFTARROW || key == K_GP_DPAD_LEFT)
		{
			if (--serverpreview < 1)
				serverpreview = 4;

			if (serverpreview == 4)
				Master_FindRoute(server->adr);
			return true;
		}
		else if (key == K_RIGHTARROW || key == K_KP_RIGHTARROW || key == K_GP_DPAD_RIGHT)
		{
			if (++serverpreview > 4)
				serverpreview = 1;

			if (serverpreview == 4)
				Master_FindRoute(server->adr);
			return true;
		}
		else if (key == 'b' && serverpreview != 4)
		{
			Master_FindRoute(server->adr);
			serverpreview = 4;
		}
		else if (key == 'b' || key == 'o' || key == 'j' || key == K_ENTER || key == K_KP_ENTER || key != K_GP_START)	//join
		{
			if (key == 's' || key == 'o')
			{
dospec:
				Cbuf_AddText("spectator 1\n", RESTRICT_LOCAL);
			}
			else if (key == 'j')
			{
dojoin:
				Cbuf_AddText("spectator 0\n", RESTRICT_LOCAL);
			}

			//which connect command are we using?
			if ((server->special & SS_PROTOCOLMASK) == SS_NETQUAKE)
				Cbuf_AddText("nqconnect ", RESTRICT_LOCAL);
			else
				Cbuf_AddText("connect ", RESTRICT_LOCAL);

			//output the server's address
			Cbuf_AddText(va("%s", NET_AdrToString(buf, sizeof(buf), &server->adr)), RESTRICT_LOCAL);
			if (serverpreview == 4 || key == 'b')
			{	//and postfix it with routing info if we're going for a proxied route.
				if (serverpreview != 4)
					Master_FindRoute(server->adr);
				for (server = server->prevpeer; server; server = server->prevpeer)
					Cbuf_AddText(va("@%s", NET_AdrToString(buf, sizeof(buf), &server->adr)), RESTRICT_LOCAL);
			}
			Cbuf_AddText("\n", RESTRICT_LOCAL);


			M_RemoveAllMenus(true);
			return true;
		}
		else if (server && key == 'c' && ctrldown)	//copy to clip
		{
			Sys_SaveClipboard(NET_AdrToString(buf, sizeof(buf), &server->adr));
			return true;
		}
		else if (server && (key == 'v' || key == 'c'))	//say to current server
		{
			char *s;
			char safename[128];
			Q_strncpyz(safename, server->name, sizeof(safename));
			//ALWAYS sanitize your inputs.
			while((s = strchr(safename, ';')))
				*s = ' ';
			while((s = strchr(safename, '\n')))
				*s = ' ';
			if (key == 'c')
				Sys_SaveClipboard(va("%s - %s\n", server->name, NET_AdrToString(buf, sizeof(buf), &server->adr)));
			else if (ctrldown)
				Cbuf_AddText(va("say_team %s - %s\n", server->name, NET_AdrToString(buf, sizeof(buf), &server->adr)), RESTRICT_LOCAL);
			else
				Cbuf_AddText(va("say %s - %s\n", server->name, NET_AdrToString(buf, sizeof(buf), &server->adr)), RESTRICT_LOCAL);
			return true;
		}
		//eat (nearly) all keys
		else if (!(key == K_UPARROW || key == K_KP_UPARROW || key == K_GP_DPAD_UP || key == K_DOWNARROW || key == K_KP_DOWNARROW || key == K_GP_DPAD_DOWN))
			return true;
	}
	if (key == K_HOME)
	{
		info->scrollpos = 0;
		info->selectedpos = 0;
	}
	else if (key == K_END)
	{
		info->selectedpos = info->numslots-1;
		info->scrollpos = info->selectedpos - (vid.height-16-7)/8+8;
	}
	else if (key == K_PGDN)
		info->selectedpos += 10;
	else if (key == K_PGUP)
		info->selectedpos -= 10;
	else if (key == K_DOWNARROW || key == K_KP_DOWNARROW || key == K_GP_DPAD_DOWN)
		info->selectedpos += 1;
	else if (key == K_UPARROW || key == K_KP_UPARROW || key == K_GP_DPAD_UP)
		info->selectedpos -= 1;
	else if (key == K_MWHEELUP)
		info->selectedpos -= 3;
	else if (key == K_MWHEELDOWN)
		info->selectedpos += 3;
	else
		return false;

	{
		serverinfo_t *server;
		server = Master_SortedServer(info->selectedpos);

//		selectedserver.inuse = true;
//		SListOptionChanged(server);

		if (server)
		{
			snprintf(info->mappic->picturename, 32, "levelshots/%s", server->map);
			if (!*server->map || !R2D_SafeCachePic(info->mappic->picturename))
				snprintf(info->mappic->picturename, 32, "levelshots/nomap");
		}
		else
		{
			snprintf(info->mappic->picturename, 32, "levelshots/nomap");
		}

		if (/*serverpreview &&*/ server)
		{
			selectedserver.inuse = true;
			SListOptionChanged(server);

			if (serverpreview == 4)
				Master_FindRoute(server->adr);
		}
	}

	if (info->selectedpos < 0)
		info->selectedpos = 0;
	if (info->selectedpos > info->numslots-1)
		info->selectedpos = info->numslots-1;
	if (info->scrollpos < info->selectedpos - info->visibleslots)
		info->scrollpos = info->selectedpos - info->visibleslots;
	if (info->selectedpos < info->scrollpos)
		info->scrollpos = info->selectedpos;

	return true;
}

static void SL_ServerPlayer (int x, int y, menucustom_t *ths, menu_t *menu)
{
	if (selectedserver.inuse)
	{
		if (selectedserver.detail)
			if (ths->dint < selectedserver.detail->numplayers)
			{
				int i = ths->dint;
				if (selectedserver.detail->players[i].isspec&1)
					Draw_FunStringWidth (x, y, "spectator", 32, false, false);
				else
				{
					R2D_ImagePaletteColour (Sbar_ColorForMap(selectedserver.detail->players[i].topc), 1.0);
					R2D_FillBlock (x, y, 32, 4);
					R2D_ImagePaletteColour (Sbar_ColorForMap(selectedserver.detail->players[i].botc), 1.0);
					R2D_FillBlock (x, y+4, 32, 4);
					R2D_ImageColours (1.0, 1.0, 1.0, 1.0);
					Draw_FunStringWidth (x, y, va("%3i", selectedserver.detail->players[i].frags), 32-4, true, false);
				}

				Draw_FunStringWidth (x+36, y, selectedserver.detail->players[i].name, 128-36, false, false);
			}
	}
}

static void SL_SliderDraw (int x, int y, menucustom_t *ths, menu_t *menu)
{
	extern qboolean	keydown[K_MAX];
	serverlist_t *info = (serverlist_t*)(menu + 1);

	mpic_t *pic;

	R2D_ImageColours(1,1,1,1);

	pic = R2D_SafeCachePic("scrollbars/slidebg.tga");
	if (pic && R_GetShaderSizes(pic, NULL, NULL, false)>0)
	{
		R2D_ScalePic(x + ths->common.width - 8, y+8, 8, ths->common.height-16, pic);

		pic = R2D_SafeCachePic("scrollbars/arrow_up.tga");
		R2D_ScalePic(x + ths->common.width - 8, y, 8, 8, pic);

		pic = R2D_SafeCachePic("scrollbars/arrow_down.tga");
		R2D_ScalePic(x + ths->common.width - 8, y + ths->common.height - 8, 8, 8, pic);

		y += ((info->scrollpos) / ((float)info->numslots - info->visibleslots)) * (float)(ths->common.height-(64+16-1));

		y += 8;

		pic = R2D_SafeCachePic("scrollbars/slider.tga");
		R2D_ScalePic(x + ths->common.width - 8, y, 8, 64, pic);
	}
	else
	{
		R2D_ImageColours(0.1, 0.1, 0.2, 1.0);
		R2D_FillBlock(x, y, ths->common.width, ths->common.height);

		y += ((info->scrollpos) / ((float)info->numslots - info->visibleslots)) * (ths->common.height-8);

		R2D_ImageColours(0.35, 0.35, 0.55, 1.0);
		R2D_FillBlock(x, y, 8, 8);
		R2D_ImageColours(1,1,1,1);
	}

	if (keydown[K_MOUSE1])
		if (mousecursor_x >= ths->common.posx && mousecursor_x < ths->common.posx + ths->common.width)
			if (mousecursor_y >= ths->common.posy && mousecursor_y < ths->common.posy + ths->common.height)
				info->sliderpressed = true;
	if (info->sliderpressed)
	{
		if (keydown[K_MOUSE1])
		{
			float my;
			serverlist_t *info = (serverlist_t*)(menu + 1);

			my = mousecursor_y;
			my -= ths->common.posy;
			if (R_GetShaderSizes(R2D_SafeCachePic("scrollbars/slidebg.tga"), NULL, NULL, false)>0)
			{
				my -= 32+8;
				my /= ths->common.height - (64+16);
			}
			else
				my /= ths->common.height;
			my *= (info->numslots-info->visibleslots);

			if (my > info->numslots-info->visibleslots-1)
				my = info->numslots-info->visibleslots-1;
			if (my < 0)
				my = 0;

			info->scrollpos = my;
		}
		else
			info->sliderpressed = false;
	}
}
static qboolean SL_SliderKey (menucustom_t *ths, menu_t *menu, int key, unsigned int unicode)
{
	if (key == K_MOUSE1)
	{
		float my;
		serverlist_t *info = (serverlist_t*)(menu + 1);

		my = mousecursor_y;
		my -= ths->common.posy;
		if (R_GetShaderSizes(R2D_SafeCachePic("scrollbars/slidebg.tga"), NULL, NULL, false)>0)
		{
			my -= 32+8;
			my /= ths->common.height - (64+16);
		}
		else
			my /= ths->common.height;
		my *= (info->numslots-info->visibleslots);

		if (my > info->numslots-info->visibleslots-1)
			my = info->numslots-info->visibleslots-1;
		if (my < 0)
			my = 0;

		info->scrollpos = my;
		info->sliderpressed = true;
		return true;
	}
	return false;
}

static void CalcFilters(menu_t *menu)
{
	serverlist_t *info = (serverlist_t*)(menu + 1);
	info->filtermodcount = sb_filtertext.modified;

	Master_ClearMasks();

//	Master_SetMaskInteger(false, SLKEY_PING, 0, SLIST_TEST_GREATEREQUAL);
	Master_SetMaskInteger(false, SLKEY_BASEGAME, SS_UNKNOWN, SLIST_TEST_NOTEQUAL);
	if (info->filter[1] && info->filter[2])
		Master_SetMaskInteger(false, SLKEY_FLAGS, SS_PROXY, SLIST_TEST_CONTAINS);
	else
	{
		if (info->filter[1]) Master_SetMaskInteger(false, SLKEY_BASEGAME, SS_NETQUAKE, SLIST_TEST_NOTEQUAL);
		if (info->filter[1]) Master_SetMaskInteger(false, SLKEY_BASEGAME, SS_DARKPLACES, SLIST_TEST_NOTEQUAL);
		if (info->filter[2]) Master_SetMaskInteger(false, SLKEY_BASEGAME, SS_QUAKEWORLD, SLIST_TEST_NOTEQUAL);
	}
	if (info->filter[3]) Master_SetMaskInteger(false, SLKEY_FLAGS, SS_PROXY, SLIST_TEST_NOTCONTAIN);
	if (!info->filter[5]) Master_SetMaskInteger(false, SLKEY_FLAGS, SS_FAVORITE, SLIST_TEST_CONTAINS);
	if (info->filter[6]) Master_SetMaskInteger(false, SLKEY_NUMHUMANS, 0, SLIST_TEST_NOTEQUAL);
	if (info->filter[7]) Master_SetMaskInteger(false, SLKEY_FREEPLAYERS, 0, SLIST_TEST_NOTEQUAL);

	if (*sb_filtertext.string) Master_SetMaskString(false, SLKEY_NAME, sb_filtertext.string, SLIST_TEST_CONTAINS);

	Master_SortServers();
}

static qboolean SL_ReFilter (menucheck_t *option, menu_t *menu, chk_set_t set)
{
	serverlist_t *info = (serverlist_t*)(menu + 1);
	switch(set)
	{
	case CHK_CHECKED:
		return !info->filter[option->bits];
	case CHK_TOGGLE:
		if (option->bits>0)
		{
			info->filter[option->bits] ^= 1;
			Cvar_Set(&sb_hidenetquake, info->filter[1]?"1":"0");
			Cvar_Set(&sb_hidequakeworld, info->filter[2]?"1":"0");
			Cvar_Set(&sb_hideproxies, info->filter[3]?"1":"0");

			Cvar_Set(&sb_hideempty, info->filter[6]?"1":"0");
			Cvar_Set(&sb_hidefull, info->filter[7]?"1":"0");
		}

		CalcFilters(menu);

		return true;
	}

	return true;
}

static void SL_Remove	(menu_t *menu)
{
	serverlist_t *info = (serverlist_t*)(menu + 1);

	Cvar_Set(&sb_hidenetquake, info->filter[1]?"1":"0");
	Cvar_Set(&sb_hidequakeworld, info->filter[2]?"1":"0");
	Cvar_Set(&sb_hideproxies, info->filter[3]?"1":"0");
	Cvar_Set(&sb_hideempty, info->filter[6]?"1":"0");
	Cvar_Set(&sb_hidefull, info->filter[7]?"1":"0");
}

static qboolean SL_DoRefresh (menuoption_t *opt, menu_t *menu, int key)
{
	MasterInfo_Refresh();
	isrefreshing = true;
	return true;
}

void M_Menu_ServerList2_f(void)
{
	int i, y, x;
	menu_t *menu;
	menucustom_t *cust;
	serverlist_t *info;
	qboolean descending;
	int sortkey;
	char *sc;

	if (!qrenderer)
	{
		Cbuf_AddText("wait; menu_servers\n", Cmd_ExecLevel);
		return;
	}

	serverpreview = false;	//in case it was lingering.

	Key_Dest_Remove(kdm_console);
	Key_Dest_Add(kdm_emenu);

	menu = M_CreateMenu(sizeof(serverlist_t));
	menu->predraw = SL_PreDraw;
	menu->postdraw = SL_PostDraw;
	menu->key = SL_Key;
	menu->remove = SL_Remove;

	info = (serverlist_t*)(menu + 1);

	y = 16;
	cust = MC_AddCustom(menu, 0, y, NULL, 0);
	cust->draw = SL_TitlesDraw;
	cust->key = SL_TitlesKey;
	cust->common.height = 8;
	cust->common.width = vid.width-8;
	y+=8;

	info->servers_top = y;
	info->visibleslots = (vid.height-info->servers_top - 64);

	cust = MC_AddCustom(menu, vid.width-8, 16, NULL, 0);
	cust->draw = SL_SliderDraw;
	cust->key = SL_SliderKey;
	cust->common.height = info->visibleslots;
	cust->common.width = 8;

	info->visibleslots = (info->visibleslots-8)/8;
	for (i = 0, y = info->servers_top; i <= info->visibleslots; y +=8, i++)
	{
		cust = MC_AddCustom(menu, 0, y, NULL, i);
		if (i==0)
			menu->selecteditem = (menuoption_t*)&cust->common;
		cust->draw = SL_ServerDraw;
		cust->key = SL_ServerKey;
		cust->common.height = 8;
		cust->common.width = vid.width-8;
		cust->common.noselectionsound = true;
	}
	menu->dontexpand = true;

	i = 0;
	for (x = 256; x < vid.width-64; x += 128)
	{
		for (y = vid.height-64+8; y < vid.height; y += 8, i++)
		{
			cust = MC_AddCustom(menu, x+16, y, NULL, i);
			cust->draw = SL_ServerPlayer;
			cust->key = NULL;
			cust->common.height = 8;
			cust->common.width = 0;
		}
	}

	strcpy(info->refreshtext, "Refresh");

	MC_AddCheckBox(menu, 0, 72, vid.height - 64+8*1, "Ping     ", &sb_showping, 1);
	MC_AddCheckBox(menu, 0, 72, vid.height - 64+8*2, "Address  ", &sb_showaddress, 1);
	MC_AddCheckBox(menu, 0, 72, vid.height - 64+8*3, "Map      ", &sb_showmap, 1);
	MC_AddCheckBox(menu, 0, 72, vid.height - 64+8*4, "Gamedir  ", &sb_showgamedir, 1);
	MC_AddCheckBox(menu, 0, 72, vid.height - 64+8*5, "Players  ", &sb_showplayers, 1);
	MC_AddCheckBox(menu, 0, 72, vid.height - 64+8*6, "Fraglimit", &sb_showfraglimit, 1);
	MC_AddCheckBox(menu, 0, 72, vid.height - 64+8*7, "Timelimit", &sb_showtimelimit, 1);

#ifdef NQPROT
	if (M_GameType() == MGT_QUAKE1)
	{
		MC_AddCheckBoxFunc(menu, 128, 208, vid.height - 64+8*1, "Show NQ   ", SL_ReFilter, 1);
		MC_AddCheckBoxFunc(menu, 128, 208, vid.height - 64+8*2, "Show QW   ", SL_ReFilter, 2);
	}
#endif
	MC_AddCheckBoxFunc(menu, 128, 208, vid.height - 64+8*3, "Show Proxies", SL_ReFilter, 3);
	info->filtertext =
	MC_AddEditCvar    (menu, 128, 200, vid.height - 64+8*4, "Filter   ",	sb_filtertext.name, true);
	MC_AddCheckBoxFunc(menu, 128, 208, vid.height - 64+8*5, "Only Favs ", SL_ReFilter, 5);
	MC_AddCheckBoxFunc(menu, 128, 208, vid.height - 64+8*6, "Show Empty", SL_ReFilter, 6);
	MC_AddCheckBoxFunc(menu, 128, 208, vid.height - 64+8*7, "Show Full ", SL_ReFilter, 7);

	MC_AddCommand(menu, 64, 320, 0, info->refreshtext, SL_DoRefresh);

	info->filter[1] = !!sb_hidenetquake.value;
	info->filter[2] = !!sb_hidequakeworld.value;
	info->filter[3] = !!sb_hideproxies.value;
	info->filter[5] = true;//!sb_showonlyfavourites.value;
	info->filter[6] = !!sb_hideempty.value;
	info->filter[7] = !!sb_hidefull.value;

	info->mappic = (menupicture_t *)MC_AddPicture(menu, vid.width - 64, vid.height - 64, 64, 64, "012345678901234567890123456789012");

	descending = false;

	sc = sb_sortcolumn.string;
	if (*sc == '-')
		descending = true;
	else if (*sc == '+')
		descending = false;
	else
		sc--;
	sc++;
	switch(*sc)
	{
	case 't':	sortkey = SLKEY_TIMELIMIT;	break;
	case 'f':	sortkey = SLKEY_FRAGLIMIT;	break;
	case 'p':	sortkey = SLKEY_NUMHUMANS;	break;
	case 'm':	sortkey = SLKEY_MAP;		break;
	case 'g':	sortkey = SLKEY_GAMEDIR;	break;
	case 'l':	sortkey = SLKEY_PING;		break;
	case 'a':	sortkey = SLKEY_ADDRESS;	break;
	case 'n':	sortkey = SLKEY_NAME;		break;
	default:	sortkey = SLKEY_PING;		break;
	}
	Master_SetSortField(sortkey, descending);

	if (!Master_TotalCount())
	{
		MasterInfo_Refresh();
		isrefreshing = true;
	}

	CalcFilters(menu);
}

static float quickconnecttimeout;

static void M_QuickConnect_PreDraw(menu_t *menu)
{
	serverinfo_t *best = NULL;
	serverinfo_t *s;
	char adr[MAX_ADR_SIZE];
	int ping;

	Master_CheckPollSockets();	//see if we were told something important.
	CL_QueryServers();

	if (Sys_DoubleTime() > quickconnecttimeout)
	{
		quickconnecttimeout = Sys_DoubleTime() + 15;

		for (ping = 50; ping < 200 && !best; ping += 50)
		{
			for (s = firstserver; s; s = s->next)
			{
				if (!s->maxplayers)	//no response?
					continue;
				if (s->players == s->maxplayers)
					continue;	//server is full already
				if (s->special & SS_PROXY)
					continue;	//don't quickconnect to a proxy. their player counts are often wrong (especially with qtv)
				if (s->ping < ping)	//don't like servers with too high a ping
				{
					if (s->numhumans > 0)
					{
						if (best)
							if (best->numhumans > s->numhumans)
								continue;	//go for the one with most players
						best = s;
					}
				}
			}
		}

		if (best)
		{
			Con_Printf("Quick connect found %s (gamedir %s, players %i/%i/%i, ping %ims)\n", best->name, best->gamedir, best->numhumans, best->players, best->maxplayers, best->ping);

			if ((best->special & SS_PROTOCOLMASK) == SS_NETQUAKE)
				Cbuf_AddText(va("nqconnect %s\n", NET_AdrToString(adr, sizeof(adr), &best->adr)), RESTRICT_LOCAL);
			else
				Cbuf_AddText(va("join %s\n", NET_AdrToString(adr, sizeof(adr), &best->adr)), RESTRICT_LOCAL);

			M_ToggleMenu_f();
			return;
		}

		//retry
		MasterInfo_Refresh();
		isrefreshing = true;
	}
}

static qboolean M_QuickConnect_Key	(int key, menu_t *menu)
{
	return false;
}

static void M_QuickConnect_Remove	(menu_t *menu)
{
}

static qboolean M_QuickConnect_Cancel (menuoption_t *opt, menu_t *menu, int key)
{
	return false;
}

static void M_QuickConnect_DrawStatus (int x, int y, menucustom_t *ths, menu_t *menu)
{
	Draw_FunString(x, y, va("Polling, %i secs\n", (int)(quickconnecttimeout - Sys_DoubleTime() + 0.9)));
}

void M_QuickConnect_f(void)
{
	menucustom_t *cust;
	menu_t *menu;

	Key_Dest_Add(kdm_emenu);

	MasterInfo_Refresh();
	isrefreshing = true;

	quickconnecttimeout = Sys_DoubleTime() + 5;

	menu = M_CreateMenu(sizeof(serverlist_t));
	menu->predraw = M_QuickConnect_PreDraw;
	menu->key = M_QuickConnect_Key;
	menu->remove = M_QuickConnect_Remove;

	cust = MC_AddCustom(menu, 64, 64, NULL, 0);
	cust->draw = M_QuickConnect_DrawStatus;
	cust->common.height = 8;
	cust->common.width = vid.width-8;

	MC_AddCommand(menu, 64, 0, 128, "Refresh", SL_DoRefresh);
	MC_AddCommand(menu, 64, 0, 136, "Cancel", M_QuickConnect_Cancel);
}





#endif
