#include "quakedef.h"

#ifdef CL_MASTER
#include "cl_master.h"

//filtering
cvar_t	sb_hideempty		= SCVARF("sb_hideempty",	"0",	CVAR_ARCHIVE);
cvar_t	sb_hidenotempty		= SCVARF("sb_hidenotempty",	"0",	CVAR_ARCHIVE);
cvar_t	sb_hidefull			= SCVARF("sb_hidefull",		"0",	CVAR_ARCHIVE);
cvar_t	sb_hidedead			= SCVARF("sb_hidedead",		"1",	CVAR_ARCHIVE);
cvar_t	sb_hidequake2		= SCVARF("sb_hidequake2",	"1",	CVAR_ARCHIVE);
cvar_t	sb_hidequake3		= SCVARF("sb_hidequake3",	"1",	CVAR_ARCHIVE);
cvar_t	sb_hidenetquake		= SCVARF("sb_hidenetquake",	"1",	CVAR_ARCHIVE);
cvar_t	sb_hidequakeworld	= SCVARF("sb_hidequakeworld","0",	CVAR_ARCHIVE);
cvar_t	sb_maxping			= SCVARF("sb_maxping",		"0",	CVAR_ARCHIVE);
cvar_t	sb_gamedir			= SCVARF("sb_gamedir",		"",		CVAR_ARCHIVE);
cvar_t	sb_mapname			= SCVARF("sb_mapname",		"",		CVAR_ARCHIVE);

cvar_t	sb_showping			= SCVARF("sb_showping",		"1",	CVAR_ARCHIVE);
cvar_t	sb_showaddress		= SCVARF("sb_showaddress",	"0",	CVAR_ARCHIVE);
cvar_t	sb_showmap			= SCVARF("sb_showmap",		"0",	CVAR_ARCHIVE);
cvar_t	sb_showgamedir		= SCVARF("sb_showgamedir",	"0",	CVAR_ARCHIVE);
cvar_t	sb_showplayers		= SCVARF("sb_showplayers",	"1",	CVAR_ARCHIVE);
cvar_t	sb_showfraglimit	= SCVARF("sb_showfraglimit","0",	CVAR_ARCHIVE);
cvar_t	sb_showtimelimit	= SCVARF("sb_showtimelimit","0",	CVAR_ARCHIVE);

cvar_t	sb_filterkey		= SCVARF("sb_filterkey",	"hostname", CVAR_ARCHIVE);
cvar_t	sb_filtervalue		= SCVARF("sb_filtervalue",	"",		CVAR_ARCHIVE);

extern cvar_t slist_writeserverstxt;
extern cvar_t slist_cacheinfo;

void M_Serverlist_Init(void)
{
	char *grp = "Server Browser Vars";

	Cvar_Register(&sb_hideempty, grp);
	Cvar_Register(&sb_hidenotempty, grp);
	Cvar_Register(&sb_hidefull, grp);
	Cvar_Register(&sb_hidedead, grp);
	Cvar_Register(&sb_hidequake2, grp);
	Cvar_Register(&sb_hidequake3, grp);
	Cvar_Register(&sb_hidenetquake, grp);
	Cvar_Register(&sb_hidequakeworld, grp);

	Cvar_Register(&sb_maxping, grp);
	Cvar_Register(&sb_gamedir, grp);
	Cvar_Register(&sb_mapname, grp);

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

enum {
	SLISTTYPE_SERVERS,
	SLISTTYPE_FAVORITES,
	SLISTTYPE_SOURCES,
	SLISTTYPE_OPTIONS	//must be last
} slist_option;

int slist_numoptions;
int slist_firstoption;

int slist_type;



static void NM_Print (int cx, int cy, qbyte *str)
{
	Draw_AltFunString(cx, cy, str);
}

static void NM_PrintWhite (int cx, int cy, qbyte *str)
{
	Draw_FunString(cx, cy, str);
}

static void NM_PrintColoured (int cx, int cy, int colour, qbyte *str)
{
#pragma message("NM_PrintColoured: needs reimplementing")
/*
	while (*str)
	{
		NM_DrawColouredCharacter (cx, cy, (*str) | (colour<<CON_FGSHIFT));
		str++;
		cx += 8;
	}
*/
}

static void NM_PrintHighlighted (int cx, int cy, int colour, int bg, qbyte *str)
{
#pragma message("NM_PrintHighlighted: needs reimplementing")
/*
	while (*str)
	{
		NM_DrawColouredCharacter (cx, cy, (*str) | (colour<<CON_FGSHIFT) | (bg<<CON_BGSHIFT) | CON_NONCLEARBG);
		str++;
		cx += 8;
	}
*/
}

qboolean M_IsFiltered(serverinfo_t *server)	//figure out if we should filter a server.
{
	if (slist_type == SLISTTYPE_FAVORITES)
		if (!(server->special & SS_FAVORITE))
			return true;
#ifdef Q2CLIENT
	if (sb_hidequake2.value)
#endif
		if (server->special & SS_QUAKE2)
			return true;
#ifdef Q2CLIENT
	if (sb_hidequake3.value)
#endif
		if (server->special & SS_QUAKE3)
			return true;
#ifdef NQPROT
	if (sb_hidenetquake.value)
#endif
		if (server->special & (SS_NETQUAKE|SS_DARKPLACES))
			return true;
	if (sb_hidequakeworld.value)
		if (!(server->special & (SS_QUAKE2|SS_QUAKE3|SS_NETQUAKE|SS_DARKPLACES)))
			return true;
	if (sb_hideempty.value)
		if (!server->players)
			return true;
	if (sb_hidenotempty.value)
		if (server->players)
			return true;
	if (sb_hidefull.value)
		if (server->players == server->maxplayers)
			return true;
	if (sb_hidedead.value)
		if (server->maxplayers == 0)
			return true;
	if (sb_maxping.value)
		if (server->ping > sb_maxping.value)
			return true;
	if (*sb_gamedir.string)
		if (strcmp(server->gamedir, sb_gamedir.string))
			return true;
	if (*sb_mapname.string)
		if (!strstr(server->map, sb_mapname.string))
			return true;
	
	return false;
}

qboolean M_MasterIsFiltered(master_t *mast)
{
#ifndef Q2CLIENT
	if (mast->type == MT_BCASTQ2 || mast->type == MT_SINGLEQ2 || mast->type == MT_MASTERQ2)
		return true;
#endif
#ifndef NQPROT
	if (mast->type == MT_BCASTNQ || mast->type == MT_SINGLENQ)
		return true;
#endif
	return false;
}


void M_DrawOneServer (int inity)
{
	char	key[512];
	char	value[512];
	char	*o;
	int		l, i;
	char *s;
	
	int miny=8*5;
	int y=8*(5-selectedserver.linenum);	

	miny += inity;
	y += inity;

	if (!selectedserver.detail)
	{
		NM_Print (0, y, "No details\n");
		return;
	}

	s = selectedserver.detail->info;

	if (*s == '\\')
		s++;
	while (*s)
	{
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;

		l = o - key;
//		if (l < 20)
//		{
//			memset (o, ' ', 20-l);
//			key[20] = 0;
//		}
//		else
			*o = 0;
		if (y>=miny)
			NM_Print (0, y, va("%19s", key));

		if (!*s)
		{
			if (y>=miny)
				NM_Print (0, y, "MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;
		if (y>=miny)
			NM_Print (320/2, y, va("%s\n", value));

		y+=8;
	}

	for ( i = 0; i < selectedserver.detail->numplayers; i++)
	{
		if (y>=miny)
		{
			R2D_ImagePaletteColour(Sbar_ColorForMap(selectedserver.detail->players[i].topc), 1.0);
			R2D_FillBlock (12, y, 28, 4);
			R2D_ImagePaletteColour(Sbar_ColorForMap(selectedserver.detail->players[i].botc), 1.0);
			R2D_FillBlock (12, y+4, 28, 4);
			R2D_ImageColours(1.0, 1.0, 1.0, 1.0);
			NM_PrintWhite (12, y, va("%3i", selectedserver.detail->players[i].frags));
			NM_Print (12+8*4, y, selectedserver.detail->players[i].name);
		}
		y+=8;
	}

	if (y<=miny)	//whoops, there was a hole at the end, try scrolling up.
		selectedserver.linenum--;
}

int M_AddColumn (int right, int y, char *text, int maxchars, int colour, int highlight)
{
	int left;
	left = right - maxchars*8;
	if (left < 0)
		return right;

	right = left;

#pragma message("M_AddColumn: needs reimplementing")
/*
	if (highlight >= 0)
	{
		while (*text && maxchars>0)
		{
			NM_DrawColouredCharacter (right, y, (*(unsigned char *)text) | (colour<<CON_FGSHIFT) | (highlight<<CON_BGSHIFT) | CON_NONCLEARBG);
			text++;
			right += 8;
			maxchars--;
		}
	}
	else
	{
		while (*text && maxchars>0)
		{
			NM_DrawColouredCharacter (right, y, (*(unsigned char *)text) | (colour<<CON_FGSHIFT));
			text++;
			right += 8;
			maxchars--;
		}
	}
*/
	return left;
}
void M_DrawServerList(void)
{
	serverinfo_t *server;
	int op=0, filtered=0;
	int snum=0;
	int colour;
	int highlight;

	int x;
	int y = 8*3;

	char adr[MAX_ADR_SIZE];

	CL_QueryServers();
	
	slist_numoptions = 0;

	//find total servers.
	for (server = firstserver; server; server = server->next)
		if (M_IsFiltered(server))
			filtered++;
		else
			slist_numoptions++;

	if (!slist_numoptions)
	{
		char *text, *text2="", *text3="";
		if (filtered)
		{
			if (slist_type == SLISTTYPE_FAVORITES)
			{
				text	= "Highlight a server";
				text2	= "and press \'f\'";
				text3	= "to add it to this list";
			}
			else
				text = "All servers were filtered out";
		}
		else
			text = "No servers found";
		NM_PrintColoured((vid.width-strlen(text)*8)/2, 8*5, COLOR_WHITE, text);
		NM_PrintColoured((vid.width-strlen(text2)*8)/2, 8*5+8, COLOR_WHITE, text2);
		NM_PrintColoured((vid.width-strlen(text3)*8)/2, 8*5+16, COLOR_WHITE, text3);

		return;
	}

	
	if (slist_option >= slist_numoptions)
		slist_option = slist_numoptions-1;
	op = vid.height/2/8;
	op/=2;
	op=slist_option-op;
	snum = op;


	if (selectedserver.inuse == true)
	{
		M_DrawOneServer(8*5);
		return;
	}

	if (op < 0)
		op = 0;
	if (snum < 0)
		snum = 0;
	//find the server that we want
	for (server = firstserver; op>0; server=server->next)
	{
		if (M_IsFiltered(server))
			continue;
		op--;
	}

	y = 8*2;
	x = vid.width;
	if (sb_showtimelimit.value)
		x = M_AddColumn(x, y, "tl",			3, COLOR_RED, -1);
	if (sb_showfraglimit.value)
		x = M_AddColumn(x, y, "fl",			3, COLOR_RED, -1);
	if (sb_showplayers.value)
		x = M_AddColumn(x, y, "plyrs",		6, COLOR_RED, -1);
	if (sb_showmap.value)
		x = M_AddColumn(x, y, "map",		9, COLOR_RED, -1);
	if (sb_showgamedir.value)
		x = M_AddColumn(x, y, "gamedir",	9, COLOR_RED, -1);
	if (sb_showping.value)
		x = M_AddColumn(x, y, "png",		4, COLOR_RED, -1);
	if (sb_showaddress.value)
		x = M_AddColumn(x, y, "address",	21, COLOR_RED, -1);
	x = M_AddColumn(x, y, "name",		x/8-1, COLOR_RED, -1);

	y = 8*3;
	while(server)
	{
		if (M_IsFiltered(server))
		{
			server = server->next;
			continue;	//doesn't count
		}

		if (y > vid.height/2)
			break;

		if (slist_option == snum)
			highlight = COLOR_DARKBLUE;
		else
			highlight = -1;

		if (*server->name)
		{
			if (server->special & SS_FAVORITE)
				colour = COLOR_GREEN;
			else if (server->special & SS_FTESERVER)
				colour = COLOR_RED;
			else if (server->special & SS_QUAKE2)
				colour = COLOR_YELLOW;
			else if (server->special & SS_QUAKE3)
				colour = COLOR_BLUE;
			else if (server->special & SS_NETQUAKE)
				colour = COLOR_GREY;
			else if (server->special & SS_PROXY)
				colour = COLOR_MAGENTA;
			else
				colour = COLOR_WHITE;

			x = vid.width;

			// make sure we have a highlighted background
			if (highlight >= 0)
			{
				R2D_ImageColours(consolecolours[highlight].fr, consolecolours[highlight].fg, consolecolours[highlight].fb, 1.0);
				R2D_FillBlock(8, y, vid.width-16, 8);
			}

			if (sb_showtimelimit.value)
				x = M_AddColumn(x, y, va("%i", server->tl),			3, colour, highlight);	//time limit
			if (sb_showfraglimit.value)
				x = M_AddColumn(x, y, va("%i", server->fl),			3, colour, highlight);	//frag limit
			if (sb_showplayers.value)
				x = M_AddColumn(x, y, va("%i/%i", server->players, server->maxplayers),			6, colour, highlight);
			if (sb_showmap.value)
				x = M_AddColumn(x, y, server->map,		9, colour, highlight);
			if (sb_showgamedir.value)
				x = M_AddColumn(x, y, server->gamedir,	9, colour, highlight);
			if (sb_showping.value)
				x = M_AddColumn(x, y, va("%i", server->ping),			4, colour, highlight);	//frag limit
			if (sb_showaddress.value)
				x = M_AddColumn(x, y, NET_AdrToString(adr, sizeof(adr), server->adr),	21, colour, highlight);
			x = M_AddColumn(x, y, server->name,		x/8-1, colour, highlight);
		}

		if (*server->name)
			y+=8;

		server = server->next;

		snum++;
	}

	selectedserver.inuse=2;
	M_DrawOneServer(vid.height/2-4*8);
}

void M_DrawSources (void)
{
	int snum=0;
	int op;
	int y = 3*8;
	master_t *mast;
	int clr;

	slist_numoptions = 0;
	//find total sources.
	for (mast = master; mast; mast = mast->next)
		slist_numoptions++;

	if (!slist_numoptions)
	{
		char *text;
		if (0)//filtered)
			text = "All servers were filtered out\n";
		else
			text = "No sources were found\n";
		NM_PrintColoured((vid.width-strlen(text)*8)/2, 8*5, COLOR_WHITE, text);

		return;
	}

	if (slist_option >= slist_numoptions)
		slist_option = slist_numoptions-1;
	op=slist_option-vid.height/2/8;
	snum = op;

	if (op < 0)
		op = 0;
	if (snum < 0)
		snum = 0;
	//find the server that we want
	for (mast = master; op>0; mast=mast->next)
	{
		if (M_MasterIsFiltered(mast))
			continue;
		op--;
	}

	for (; mast; mast = mast->next)
	{
		if (M_MasterIsFiltered(mast))
			continue;

		switch (mast->type)
		{
		case MT_MASTERHTTP:
		case MT_MASTERHTTPQW:
			clr = COLOR_YELLOW;
			break;
		case MT_MASTERQW:
		case MT_MASTERQ2:
			clr = COLOR_WHITE;
			break;
		case MT_SINGLENQ:
		case MT_SINGLEQW:
		case MT_SINGLEQ2:
			clr = COLOR_GREEN;
			break;
		default:
			clr = COLOR_RED;
		}

		if (slist_option == snum) // highlight it if selected
			NM_PrintHighlighted(46, y, clr, COLOR_DARKBLUE, va("%s", mast->name));
		else
			NM_PrintColoured(46, y, clr, va("%s", mast->name));

		y+=8;
		snum++;
	}
}

#define NUMSLISTOPTIONS (8+7+4)
	struct {
		char *title;
		cvar_t *cvar;
		int type;
	} options[NUMSLISTOPTIONS] = {
		{"Hide Empty",		&sb_hideempty},
		{"Hide Not Empty",	&sb_hidenotempty},
		{"Hide Full",		&sb_hidefull},
		{"Hide Dead",		&sb_hidedead},
		{"Hide Quake 2",	&sb_hidequake2},
		{"Hide Quake 3",	&sb_hidequake3},
		{"Hide Quake 1",	&sb_hidenetquake},
		{"Hide QuakeWorld",	&sb_hidequakeworld},

		{"Show pings",		&sb_showping},
		{"Show Addresses",	&sb_showaddress},
		{"Show map",		&sb_showmap},
		{"Show Game Dir",	&sb_showgamedir},
		{"Show Players",	&sb_showplayers},
		{"Show Fraglimit",	&sb_showfraglimit},
		{"Show Timelimit",	&sb_showtimelimit},

		{"Max ping",		&sb_maxping,	1},
		{"GameDir",			&sb_gamedir,	2},
		{"Using map",		&sb_mapname,	2},
		{"Game name",		&com_gamename,	2}
	};

void M_DrawSListOptions (void)
{
	int c;
	int op;
	char *s;

	slist_numoptions = NUMSLISTOPTIONS;

	for (op = 0; op < NUMSLISTOPTIONS; op++)
	{
		if (options[op].cvar->value>0 || (*options[op].cvar->string && *options[op].cvar->string != '0'))
			c = COLOR_RED;
		else
			c = COLOR_WHITE;

		switch(options[op].type)
		{
		default:
			s = options[op].title;
			break;
		case 1:
			if (!options[op].cvar->value)
			{
				s = va("%s ", options[op].title);
				break;
			}
		case 2:
			s = va("%s %s", options[op].title, options[op].cvar->string);
			break;
		}

		if (slist_option == op) // selected
			NM_PrintHighlighted(46, op*8+8*3, c, COLOR_DARKBLUE, s);
		else
			NM_PrintColoured(46, op*8+8*3, c, s);
	}
}

void M_SListOptions_Key (int key)
{
	if (key == K_UPARROW)
	{
		slist_option--;
		if (slist_option<0)
			slist_option=0;
		return;
	}
	else if (key == K_DOWNARROW)
	{
		slist_option++;
		if (slist_option >= slist_numoptions)
			slist_option = slist_numoptions-1;
		return;
	}
	
	switch(options[slist_option].type)
	{
	default:
		if (key == K_ENTER)
		{
			if (options[slist_option].cvar->value)
				Cvar_Set(options[slist_option].cvar, "0");
			else
				Cvar_Set(options[slist_option].cvar, "1");
		}
		break;
	case 1:
		if (key >= '0' && key <= '9')
			Cvar_SetValue(options[slist_option].cvar, options[slist_option].cvar->value*10+key-'0');
		else if (key == K_DEL)
			Cvar_SetValue(options[slist_option].cvar, 0);
		else if (key == K_BACKSPACE)
			Cvar_SetValue(options[slist_option].cvar, (int)options[slist_option].cvar->value/10);
		break;
	case 2:
		if ((key >= '0' && key <= '9') || (key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z')|| key == '_')
			Cvar_Set(options[slist_option].cvar, va("%s%c", options[slist_option].cvar->string, key));
		else if (key == K_DEL)
			Cvar_Set(options[slist_option].cvar, "");
		else if (key == K_BACKSPACE)	//FIXME
			Cvar_Set(options[slist_option].cvar, "");
		break;
	}
}


void M_DrawServers(void)
{
#define NUMSLISTHEADERS (SLISTTYPE_OPTIONS+1)
	char *titles[NUMSLISTHEADERS] = {
		"Servers",
		"Favorites",
		"Sources",
//		"Players",
		"Options"
	};
	int snum=0;

	int width, lofs;

	NET_CheckPollSockets();	//see if we were told something important.

	width = vid.width / NUMSLISTHEADERS;
	lofs = width/2 - 7*4;
	for (snum = 0; snum < NUMSLISTHEADERS; snum++)
	{
		if (slist_type == snum)
			NM_PrintHighlighted(width*snum+width/2 - strlen(titles[snum])*4, 0, COLOR_WHITE, COLOR_DARKBLUE, titles[snum]);
		else
			NM_PrintColoured(width*snum+width/2 - strlen(titles[snum])*4, 0, COLOR_WHITE, titles[snum]);
	}
	NM_PrintColoured(8, 8, COLOR_WHITE, "\35");
	for (snum = 16; snum < vid.width-16; snum+=8)
		NM_PrintColoured(snum, 8, COLOR_WHITE, "\36");
	NM_PrintColoured(snum, 8, COLOR_WHITE, "\37");

	switch(slist_type)
	{
	case SLISTTYPE_SERVERS:
	case SLISTTYPE_FAVORITES:
		M_DrawServerList();
		break;
	case SLISTTYPE_SOURCES:
		M_DrawSources ();
		break;
	case SLISTTYPE_OPTIONS:
		M_DrawSListOptions ();
		break;
	}
}

serverinfo_t *M_FindCurrentServer(void)
{
	serverinfo_t *server;
	int op = slist_option;
	for (server = firstserver; server; server = server->next)
	{
		if (M_IsFiltered(server))
			continue;	//doesn't count
		if (!op--)
			return server;
	}
	return NULL;
}

master_t *M_FindCurrentMaster(void)
{
	master_t *mast;
	int op = slist_option;

	for (mast = master; mast; mast = mast->next)
	{
		if (M_MasterIsFiltered(mast))
			continue;
		if (!op--)
			return mast;
	}
	return NULL;
}

void M_SListKey(int key)
{
	char adr[MAX_ADR_SIZE];

	if (key == K_ESCAPE)
	{
//		if (selectedserver.inuse)
//			selectedserver.inuse = false;
//		else
			M_Menu_MultiPlayer_f();
		return;
	}
	else if (key == K_LEFTARROW)
	{
		slist_type--;
		if (slist_type<0)
			slist_type=0;

		selectedserver.linenum--;
		if (selectedserver.linenum<0)
			selectedserver.linenum=0;

		slist_numoptions=0;
		return;
	}
	else if (key == K_RIGHTARROW)
	{
		slist_type++;
		if (slist_type>NUMSLISTHEADERS-1)
			slist_type=NUMSLISTHEADERS-1;

		selectedserver.linenum++;

		slist_numoptions = 0;
		return;
	}
	else if (key == 'q')
		selectedserver.linenum--;
	else if (key == 'a')
		selectedserver.linenum++;

	if (!slist_numoptions)
		return;

	if (slist_type == SLISTTYPE_OPTIONS)
	{
		M_SListOptions_Key(key);
		return;
	}
	
	if (key == K_UPARROW)
	{
		slist_option--;
		if (slist_option<0)
			slist_option=0;

		if (slist_type == SLISTTYPE_SERVERS || slist_type == SLISTTYPE_FAVORITES)
			SListOptionChanged(M_FindCurrentServer());	//go for these early.
	}
	else if (key == K_DOWNARROW)
	{
		slist_option++;
		if (slist_option >= slist_numoptions)
			slist_option = slist_numoptions-1;

		if (slist_type == SLISTTYPE_SERVERS || slist_type == SLISTTYPE_FAVORITES)
			SListOptionChanged(M_FindCurrentServer());	//go for these early.
	}
	else if (key == K_PGDN)
	{
		slist_option+=10;
		if (slist_option >= slist_numoptions)
			slist_option = slist_numoptions-1;

		if (slist_type == SLISTTYPE_SERVERS || slist_type == SLISTTYPE_FAVORITES)
			SListOptionChanged(M_FindCurrentServer());	//go for these early.
	}
	else if (key == K_PGUP)
	{
		slist_option-=10;
		if (slist_option<0)
			slist_option=0;

		if (slist_type == SLISTTYPE_SERVERS || slist_type == SLISTTYPE_FAVORITES)
			SListOptionChanged(M_FindCurrentServer());	//go for these early.
	}
	else if (key == 'r')
		MasterInfo_Begin();
	else if (key == K_SPACE)
	{
		if (slist_type == SLISTTYPE_SERVERS || slist_type == SLISTTYPE_FAVORITES)
		{
			selectedserver.inuse = !selectedserver.inuse;
			if (selectedserver.inuse)
				SListOptionChanged(M_FindCurrentServer());
		}
	}
	else if (key == 'c')
	{
		Sys_SaveClipboard(NET_AdrToString(adr, sizeof(adr), selectedserver.adr));
	}
	else if (key == 'f')
	{
		serverinfo_t *server;
		if (slist_type == SLISTTYPE_SERVERS)	//add to favorites
		{
			server = M_FindCurrentServer();
			if (server)
			{
				server->special |= SS_FAVORITE;
				MasterInfo_WriteServers();
			}
		}
		if (slist_type == SLISTTYPE_FAVORITES)	//remove from favorites
		{
			server = M_FindCurrentServer();
			if (server)
			{
				server->special &= ~SS_FAVORITE;
				MasterInfo_WriteServers();
			}
		}
	}
	else if (key==K_ENTER || key == 's' || key == 'j')
	{
		serverinfo_t *server;
		if (slist_type == SLISTTYPE_SERVERS || slist_type == SLISTTYPE_FAVORITES)
		{
			if (!selectedserver.inuse)
			{
				selectedserver.inuse = true;
				SListOptionChanged(M_FindCurrentServer());
				return;
			}
			server = M_FindCurrentServer();
			if (!server)
				return;	//ah. off the end.

			if (key == 's')
				Cbuf_AddText("spectator 1\n", RESTRICT_LOCAL);
			else if (key == 'j')
				Cbuf_AddText("spectator 0\n", RESTRICT_LOCAL);

			if (server->special & SS_NETQUAKE)
				Cbuf_AddText(va("nqconnect %s\n", NET_AdrToString(adr, sizeof(adr), server->adr)), RESTRICT_LOCAL);
			else
				Cbuf_AddText(va("connect %s\n", NET_AdrToString(adr, sizeof(adr), server->adr)), RESTRICT_LOCAL);

			M_ToggleMenu_f();
			M_ToggleMenu_f();
		}
		else if (slist_type == SLISTTYPE_SOURCES)
		{
			MasterInfo_Request(M_FindCurrentMaster(), true);
		}

		return;
	}	
}













typedef struct {
	int visibleslots;
	int scrollpos;
	int selectedpos;

	int numslots;
	qboolean stillpolling;
	qbyte filter[8];

	qboolean sliderpressed;

	menupicture_t *mappic;
} serverlist_t;

void SL_DrawColumnTitle (int *x, int y, int xlen, int mx, char *str, qboolean recolor, qbyte clr, qboolean *filldraw)
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
	}
	Draw_FunStringWidth(xmin, y, str, xlen);

	if (x != NULL)
		*x -= xlen + 8;
}

void SL_TitlesDraw (int x, int y, menucustom_t *ths, menu_t *menu)
{
	int sf = Master_GetSortField();
	extern int mousecursor_x, mousecursor_y;
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
	if (sb_showplayers.value)	{SL_DrawColumnTitle(&x, y, 5*8, mx, "plyrs", (sf==SLKEY_NUMPLAYERS), clr, &filldraw);}
	if (sb_showmap.value)		{SL_DrawColumnTitle(&x, y, 8*8, mx, "map", (sf==SLKEY_MAP), clr, &filldraw);}
	if (sb_showgamedir.value)	{SL_DrawColumnTitle(&x, y, 8*8, mx, "gamedir", (sf==SLKEY_GAMEDIR), clr, &filldraw);}
	if (sb_showping.value)		{SL_DrawColumnTitle(&x, y, 3*8, mx, "png", (sf==SLKEY_PING), clr, &filldraw);}
	if (sb_showaddress.value)	{SL_DrawColumnTitle(&x, y, 21*8, mx, "address", (sf==SLKEY_ADDRESS), clr, &filldraw);}
	SL_DrawColumnTitle(NULL, y, x, mx, "hostname ", (sf==SLKEY_NAME), clr, &filldraw);
}

qboolean SL_TitlesKey (menucustom_t *ths, menu_t *menu, int key)
{
	int x;
	extern int mousecursor_x, mousecursor_y;
	int mx = mousecursor_x/8;
	int sortkey;

	if (key != K_MOUSE1)
		return false;

	do {
		x = ths->common.width/8;
		if (mx > x) return false;	//out of bounds
		if (sb_showtimelimit.value)	{x-=4;if (mx > x) {sortkey = SLKEY_TIMELIMIT; break;}}
		if (sb_showfraglimit.value)	{x-=4;if (mx > x) {sortkey = SLKEY_FRAGLIMIT; break;}}
		if (sb_showplayers.value)	{x-=6;if (mx > x) {sortkey = SLKEY_NUMPLAYERS; break;}}
		if (sb_showmap.value)		{x-=9;if (mx > x) {sortkey = SLKEY_MAP; break;}}
		if (sb_showgamedir.value)	{x-=9;if (mx > x) {sortkey = SLKEY_GAMEDIR; break;}}
		if (sb_showping.value)		{x-=4;if (mx > x) {sortkey = SLKEY_PING; break;}}
		if (sb_showaddress.value)	{x-=22;if (mx > x) {sortkey = SLKEY_ADDRESS; break;}}
			sortkey = SLKEY_NAME;break;
	} while (0);

	if (sortkey == SLKEY_ADDRESS)
		return true;

	Master_SetSortField(sortkey, Master_GetSortField()!=sortkey||!Master_GetSortDescending());
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

float serverbackcolor[MAX_SERVERTYPES * 2][3] =
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

float serverhighlight[MAX_SERVERTYPES][3] =
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

servertypes_t flagstoservertype(int flags)
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
	if (flags & SS_FTESERVER)
		return ST_FTESERVER;
	if ((flags & SS_NETQUAKE) || (flags & SS_DARKPLACES))
		return ST_NETQUAKE;
	if (flags & SS_QUAKE2)
		return ST_QUAKE2;
	if (flags & SS_QUAKE3)
		return ST_QUAKE3;

	return ST_NORMALQW;
}

void SL_ServerDraw (int x, int y, menucustom_t *ths, menu_t *menu)
{
	extern int mousecursor_x, mousecursor_y;
	serverlist_t *info = (serverlist_t*)(menu + 1);
	serverinfo_t *si;
	int thisone = (int)ths->data + info->scrollpos;
	servertypes_t stype;
	char adr[MAX_ADR_SIZE];

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
		else if (thisone == info->scrollpos + (mousecursor_y-16)/8 && mousecursor_x < x)
			R2D_ImageColours((sin(realtime*4.4)*0.25)+0.5, (sin(realtime*4.4)*0.25)+0.5, 0.08, 1.0);
		else if (selectedserver.inuse && NET_CompareAdr(si->adr, selectedserver.adr))
			R2D_ImageColours(((sin(realtime*4.4)*0.25)+0.5) * 0.5, ((sin(realtime*4.4)*0.25)+0.5)*0.5, 0.08*0.5, 1.0);
		else
		{		
			R2D_ImageColours(
				serverbackcolor[(int)stype * 2 + (thisone & 1)][0],
				serverbackcolor[(int)stype * 2 + (thisone & 1)][1],
				serverbackcolor[(int)stype * 2 + (thisone & 1)][2],
				1.0);
		}
		R2D_FillBlock(0, y, ths->common.width, 8);

		if (sb_showtimelimit.value)	{Draw_FunStringWidth((x-3*8), y, va("%i", si->tl), 3*8); x-=4*8;}
		if (sb_showfraglimit.value)	{Draw_FunStringWidth((x-3*8), y, va("%i", si->fl), 3*8); x-=4*8;}
		if (sb_showplayers.value)	{Draw_FunStringWidth((x-5*8), y, va("%2i/%2i", si->players, si->maxplayers), 5*8); x-=6*8;}
		if (sb_showmap.value)		{Draw_FunStringWidth((x-8*8), y, si->map, 8*8); x-=9*8;}
		if (sb_showgamedir.value)	{Draw_FunStringWidth((x-8*8), y, si->gamedir, 8*8); x-=9*8;}
		if (sb_showping.value)		{Draw_FunStringWidth((x-3*8), y, va("%i", si->ping), 3*8); x-=4*8;}
		if (sb_showaddress.value)	{Draw_FunStringWidth((x-21*8), y, NET_AdrToString(adr, sizeof(adr), si->adr), 21*8); x-=22*8;}
		Draw_FunStringWidth(0, y, si->name, x);
	}
}
qboolean SL_ServerKey (menucustom_t *ths, menu_t *menu, int key)
{
	static int lastclick;
	int curtime;
	int oldselection;
	extern int mousecursor_x, mousecursor_y;
	serverlist_t *info = (serverlist_t*)(menu + 1);
	serverinfo_t *server;
	char adr[MAX_ADR_SIZE];

	if (key == K_MOUSE1)
	{
		oldselection = info->selectedpos;
		info->selectedpos = info->scrollpos + (mousecursor_y-16)/8;
		server = Master_SortedServer(info->selectedpos);

		selectedserver.inuse = true;
		SListOptionChanged(server);

		if (server)
		{
			snprintf(info->mappic->picturename, 32, "levelshots/%s", server->map);
			if (!R2D_SafeCachePic(info->mappic->picturename))
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
			goto joinserver;
		return true;
	}

	if (key == 'f')
	{
		server = Master_SortedServer(info->selectedpos);
		if (server)
		{
			server->special ^= SS_FAVORITE;
		}
	}

	if (key == K_ENTER || key == 's' || key == 'j' || key == K_SPACE)
	{
		server = Master_SortedServer(info->selectedpos);
		if (server)
		{
			if (key == 's' || key == K_SPACE)
				Cbuf_AddText("spectator 1\n", RESTRICT_LOCAL);
			else if (key == 'j')
			{
joinserver:
				Cbuf_AddText("spectator 0\n", RESTRICT_LOCAL);
			}

			if (server->special & SS_NETQUAKE)
				Cbuf_AddText(va("nqconnect %s\n", NET_AdrToString(adr, sizeof(adr), server->adr)), RESTRICT_LOCAL);
			else
				Cbuf_AddText(va("connect %s\n", NET_AdrToString(adr, sizeof(adr), server->adr)), RESTRICT_LOCAL);

			M_RemoveAllMenus();
		}
		return true;
	}
	return false;
}
void SL_PreDraw	(menu_t *menu)
{
	serverlist_t *info = (serverlist_t*)(menu + 1);
	NET_CheckPollSockets();

	CL_QueryServers();

	info->numslots = Master_NumSorted();
}
qboolean SL_Key	(int key, menu_t *menu)
{
	serverlist_t *info = (serverlist_t*)(menu + 1);

	if (key == K_HOME)
	{
		info->scrollpos = 0;
		info->selectedpos = 0;
		return true;
	}
	if (key == K_END)
	{
		info->selectedpos = info->numslots-1;
		info->scrollpos = info->selectedpos - (vid.height-16-7)/8;
		return true;
	}
	if (key == K_PGDN)
		info->selectedpos += 10;
	else if (key == K_PGUP)
		info->selectedpos -= 10;
	else if (key == K_DOWNARROW)
		info->selectedpos += 1;
	else if (key == K_UPARROW)
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
			if (!R2D_SafeCachePic(info->mappic->picturename))
				snprintf(info->mappic->picturename, 32, "levelshots/nomap");
		}
		else
		{
			snprintf(info->mappic->picturename, 32, "levelshots/nomap");
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

void SL_ServerPlayer (int x, int y, menucustom_t *ths, menu_t *menu)
{
	if (selectedserver.inuse)
	{
		if (selectedserver.detail)
			if ((int)ths->data < selectedserver.detail->numplayers)
			{
				int i = (int)ths->data;
				R2D_ImagePaletteColour (Sbar_ColorForMap(selectedserver.detail->players[i].topc), 1.0);
				R2D_FillBlock (x, y, 28, 4);
				R2D_ImagePaletteColour (Sbar_ColorForMap(selectedserver.detail->players[i].botc), 1.0);
				R2D_FillBlock (x, y+4, 28, 4);
				NM_PrintWhite (x, y, va("%3i", selectedserver.detail->players[i].frags));

				Draw_FunStringWidth (x+28, y, selectedserver.detail->players[i].name, 12*8);
			}
	}
}

void SL_SliderDraw (int x, int y, menucustom_t *ths, menu_t *menu)
{
	serverlist_t *info = (serverlist_t*)(menu + 1);

	mpic_t *pic;

	pic = R2D_SafeCachePic("scrollbars/slidebg.png");
	if (pic)
	{
		R2D_ScalePic(x + ths->common.width - 8, y+8, 8, ths->common.height-16, pic);

		pic = R2D_SafeCachePic("scrollbars/arrow_up.png");
		R2D_ScalePic(x + ths->common.width - 8, y, 8, 8, pic);

		pic = R2D_SafeCachePic("scrollbars/arrow_down.png");
		R2D_ScalePic(x + ths->common.width - 8, y + ths->common.height - 8, 8, 8, pic);

		y += ((info->scrollpos) / ((float)info->numslots - info->visibleslots)) * (float)(ths->common.height-(64+16-1));

		y += 8;

		pic = R2D_SafeCachePic("scrollbars/slider.png");
		R2D_ScalePic(x + ths->common.width - 8, y, 8, 64, pic);
	}
	else
	{
		R2D_ImageColours(0.1, 0.1, 0.2, 1.0);
		R2D_FillBlock(x, y, ths->common.width, ths->common.height);

		y += ((info->scrollpos) / ((float)info->numslots - info->visibleslots)) * (ths->common.height-8);

		R2D_ImageColours(0.35, 0.35, 0.55, 1.0);
		R2D_FillBlock(x, y, 8, 8);
	}

	if (info->sliderpressed)
	{
		extern qboolean	keydown[K_MAX];
		if (keydown[K_MOUSE1])
		{
			extern int mousecursor_x, mousecursor_y;
			float my;
			serverlist_t *info = (serverlist_t*)(menu + 1);

			my = mousecursor_y;
			my -= ths->common.posy;
			if (R2D_SafeCachePic("scrollbars/slidebg.png"))
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
qboolean SL_SliderKey (menucustom_t *ths, menu_t *menu, int key)
{
	if (key == K_MOUSE1)
	{
		extern int mousecursor_x, mousecursor_y;
		float my;
		serverlist_t *info = (serverlist_t*)(menu + 1);

		my = mousecursor_y;
		my -= ths->common.posy;
		if (R2D_SafeCachePic("scrollbars/slidebg.png"))
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

void CalcFilters(menu_t *menu)
{
	serverlist_t *info = (serverlist_t*)(menu + 1);

	Master_ClearMasks();

	Master_SetMaskInteger(false, SLKEY_PING, 0, SLIST_TEST_LESS);
	if (info->filter[1]) Master_SetMaskInteger(true, SLKEY_BASEGAME, SS_NETQUAKE|SS_DARKPLACES, SLIST_TEST_CONTAINS);
	if (info->filter[2]) Master_SetMaskInteger(true, SLKEY_BASEGAME, SS_NETQUAKE|SS_DARKPLACES|SS_QUAKE2|SS_QUAKE3, SLIST_TEST_NOTCONTAIN);
	if (info->filter[3]) Master_SetMaskInteger(true, SLKEY_BASEGAME, SS_QUAKE2, SLIST_TEST_CONTAINS);
	if (info->filter[4]) Master_SetMaskInteger(true, SLKEY_BASEGAME, SS_QUAKE3, SLIST_TEST_CONTAINS);
	if (info->filter[5]) Master_SetMaskInteger(false, SLKEY_BASEGAME, SS_FAVORITE, SLIST_TEST_CONTAINS);
	if (info->filter[6]) Master_SetMaskInteger(false, SLKEY_NUMPLAYERS, 0, SLIST_TEST_NOTEQUAL);
	if (info->filter[7]) Master_SetMaskInteger(false, SLKEY_FREEPLAYERS, 0, SLIST_TEST_NOTEQUAL);
}

qboolean SL_ReFilter (menucheck_t *option, menu_t *menu, chk_set_t set)
{
	serverlist_t *info = (serverlist_t*)(menu + 1);
	switch(set)
	{
	case CHK_CHECKED:
		return info->filter[option->bits];
	case CHK_TOGGLE:
		if (option->bits>0)
		{
			info->filter[option->bits] ^= 1;
			Cvar_Set(&sb_hidenetquake, info->filter[1]?"0":"1");
			Cvar_Set(&sb_hidequakeworld, info->filter[2]?"0":"1");
			Cvar_Set(&sb_hidequake2, info->filter[3]?"0":"1");
			Cvar_Set(&sb_hidequake3, info->filter[4]?"0":"1");

			Cvar_Set(&sb_hideempty, info->filter[6]?"1":"0");
			Cvar_Set(&sb_hidefull, info->filter[7]?"1":"0");
		}

		CalcFilters(menu);

		return true;
	}

	return true;
}

void SL_Remove	(menu_t *menu)
{
	serverlist_t *info = (serverlist_t*)(menu + 1);

	Cvar_Set(&sb_hidenetquake, info->filter[1]?"0":"1");
	Cvar_Set(&sb_hidequakeworld, info->filter[2]?"0":"1");
	Cvar_Set(&sb_hidequake2, info->filter[3]?"0":"1");
	Cvar_Set(&sb_hidequake3, info->filter[4]?"0":"1");

	Cvar_Set(&sb_hideempty, info->filter[6]?"1":"0");
	Cvar_Set(&sb_hidefull, info->filter[7]?"1":"0");
}

qboolean SL_DoRefresh (menuoption_t *opt, menu_t *menu, int key)
{
	MasterInfo_Begin();
	return true;
}

void M_Menu_ServerList2_f(void)
{
	int i, y, x;
	menu_t *menu;
	menucustom_t *cust;
	serverlist_t *info;

	if (!qrenderer)
	{
		Cbuf_AddText("wait; menu_servers\n", Cmd_ExecLevel);
		return;
	}

	key_dest = key_menu;
	m_state = m_complex;

	MasterInfo_Begin();

	menu = M_CreateMenu(sizeof(serverlist_t));
	menu->event = SL_PreDraw;
	menu->key = SL_Key;
	menu->remove = SL_Remove;

	info = (serverlist_t*)(menu + 1);

	y = 8;
	cust = MC_AddCustom(menu, 0, y, 0);
	cust->draw = SL_TitlesDraw;
	cust->key = SL_TitlesKey;
	cust->common.height = 8;
	cust->common.width = vid.width-8;

	info->visibleslots = (vid.height-16 - 64);

	cust = MC_AddCustom(menu, vid.width-8, 16, NULL);
	cust->draw = SL_SliderDraw;
	cust->key = SL_SliderKey;
	cust->common.height = info->visibleslots;
	cust->common.width = 8;

	info->visibleslots = (info->visibleslots-7)/8;
	for (i = 0, y = 16; i <= info->visibleslots; y +=8, i++)
	{
		cust = MC_AddCustom(menu, 0, y, (void*)i);
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
			cust = MC_AddCustom(menu, x+16, y, (void*)i);
			cust->draw = SL_ServerPlayer;
			cust->key = NULL;
			cust->common.height = 8;
			cust->common.width = 0;
		}
	}

	MC_AddCheckBox(menu, 0, vid.height - 64+8*1, "Ping     ", &sb_showping, 1);
	MC_AddCheckBox(menu, 0, vid.height - 64+8*2, "Address  ", &sb_showaddress, 1);
	MC_AddCheckBox(menu, 0, vid.height - 64+8*3, "Map      ", &sb_showmap, 1);
	MC_AddCheckBox(menu, 0, vid.height - 64+8*4, "Gamedir  ", &sb_showgamedir, 1);
	MC_AddCheckBox(menu, 0, vid.height - 64+8*5, "Players  ", &sb_showplayers, 1);
	MC_AddCheckBox(menu, 0, vid.height - 64+8*6, "Fraglimit", &sb_showfraglimit, 1);
	MC_AddCheckBox(menu, 0, vid.height - 64+8*7, "Timelimit", &sb_showtimelimit, 1);

	MC_AddCheckBoxFunc(menu, 128, vid.height - 64+8*1, "List NQ   ", SL_ReFilter, 1);
	MC_AddCheckBoxFunc(menu, 128, vid.height - 64+8*2, "List QW   ", SL_ReFilter, 2);
	MC_AddCheckBoxFunc(menu, 128, vid.height - 64+8*3, "List Q2   ", SL_ReFilter, 3);
	MC_AddCheckBoxFunc(menu, 128, vid.height - 64+8*4, "List Q3   ", SL_ReFilter, 4);
	MC_AddCheckBoxFunc(menu, 128, vid.height - 64+8*5, "Only Favs ", SL_ReFilter, 5);
	MC_AddCheckBoxFunc(menu, 128, vid.height - 64+8*6, "Hide Empty", SL_ReFilter, 6);
	MC_AddCheckBoxFunc(menu, 128, vid.height - 64+8*7, "Hide Full ", SL_ReFilter, 7);

	MC_AddCommand(menu, 64, 0, "Refresh", SL_DoRefresh);

	info->filter[1] = !sb_hidenetquake.value;
	info->filter[2] = !sb_hidequakeworld.value;
	info->filter[3] = !sb_hidequake2.value;
	info->filter[4] = !sb_hidequake3.value;
	info->filter[6] = !!sb_hideempty.value;
	info->filter[7] = !!sb_hidefull.value;

	info->mappic = (menupicture_t *)MC_AddPicture(menu, vid.width - 64, vid.height - 64, 64, 64, "012345678901234567890123456789012");

	CalcFilters(menu);

	Master_SetSortField(SLKEY_PING, true);
}

float quickconnecttimeout;

void M_QuickConnect_PreDraw(menu_t *menu)
{
	serverinfo_t *best = NULL;
	serverinfo_t *s;
	char adr[MAX_ADR_SIZE];

	NET_CheckPollSockets();	//see if we were told something important.
	CL_QueryServers();

	if (Sys_DoubleTime() > quickconnecttimeout)
	{
		for (s = firstserver; s; s = s->next)
		{
			if (!s->maxplayers)	//no response?
				continue;
			if (s->players == s->maxplayers)
				continue;	//server is full already
			if (s->special & SS_PROXY)
				continue;	//don't quickconnect to a proxy. their player counts are often wrong (especially with qtv)
			if (s->ping < 50)	//don't like servers with too high a ping
			{
				if (s->players > 0)
				{
					if (best)
						if (best->players > s->players)
							continue;	//go for the one with most players
					best = s;
				}
			}
		}

		if (best)
		{
			Con_Printf("Quick connect found %s (gamedir %s, players %i/%i, ping %ims)\n", best->name, best->gamedir, best->players, best->maxplayers, best->ping);

			if (best->special & SS_NETQUAKE)
				Cbuf_AddText(va("nqconnect %s\n", NET_AdrToString(adr, sizeof(adr), best->adr)), RESTRICT_LOCAL);
			else
				Cbuf_AddText(va("join %s\n", NET_AdrToString(adr, sizeof(adr), best->adr)), RESTRICT_LOCAL);

			M_ToggleMenu_f();
			return;
		}

		//retry
		MasterInfo_Begin();

		quickconnecttimeout = Sys_DoubleTime() + 5;
	}
}

qboolean M_QuickConnect_Key	(int key, menu_t *menu)
{
	return false;
}

void M_QuickConnect_Remove	(menu_t *menu)
{
}

qboolean M_QuickConnect_Cancel (menuoption_t *opt, menu_t *menu, int key)
{
	return false;
}

void M_QuickConnect_DrawStatus (int x, int y, menucustom_t *ths, menu_t *menu)
{
	Draw_FunString(x, y, va("Polling, %i secs\n", (int)(quickconnecttimeout - Sys_DoubleTime() + 0.9)));
}

void M_QuickConnect_f(void)
{
	menucustom_t *cust;
	menu_t *menu;

	key_dest = key_menu;
	m_state = m_complex;

	MasterInfo_Begin();

	quickconnecttimeout = Sys_DoubleTime() + 5;

	menu = M_CreateMenu(sizeof(serverlist_t));
	menu->event = M_QuickConnect_PreDraw;
	menu->key = M_QuickConnect_Key;
	menu->remove = M_QuickConnect_Remove;

	cust = MC_AddCustom(menu, 64, 64, NULL);
	cust->draw = M_QuickConnect_DrawStatus;
	cust->common.height = 8;
	cust->common.width = vid.width-8;

	MC_AddCommand(menu, 64, 128, "Refresh", SL_DoRefresh);
	MC_AddCommand(menu, 64, 136, "Cancel", M_QuickConnect_Cancel);
}





#endif
