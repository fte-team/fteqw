#include "quakedef.h"

#ifdef CL_MASTER
#include "cl_master.h"

enum {
SLISTTYPE_SERVERS,
SLISTTYPE_FAVORITES,
SLISTTYPE_SOURCES,
SLISTTYPE_OPTIONS	//must be last
} slist_option;

int slist_numoptions;
int slist_firstoption;

int slist_type;

//filtering
cvar_t	sb_hideempty		= {"sb_hideempty",		"0",	NULL, CVAR_ARCHIVE};
cvar_t	sb_hidenotempty		= {"sb_hidenotempty",	"0",	NULL, CVAR_ARCHIVE};
cvar_t	sb_hidefull			= {"sb_hidefull",		"0",	NULL, CVAR_ARCHIVE};
cvar_t	sb_hidedead			= {"sb_hidedead",		"1",	NULL, CVAR_ARCHIVE};
cvar_t	sb_hidequake2		= {"sb_hidequake2",		"1",	NULL, CVAR_ARCHIVE};
cvar_t	sb_hidenetquake		= {"sb_hidenetquake",	"1",	NULL, CVAR_ARCHIVE};
cvar_t	sb_hidequakeworld	= {"sb_hidequakeworld",	"0",	NULL, CVAR_ARCHIVE};
cvar_t	sb_maxping			= {"sb_maxping",		"0",	NULL, CVAR_ARCHIVE};
cvar_t	sb_gamedir			= {"sb_gamedir",		"",		NULL, CVAR_ARCHIVE};
cvar_t	sb_mapname			= {"sb_mapname",		"",		NULL, CVAR_ARCHIVE};

cvar_t	sb_showping			= {"sb_showping",		"1",	NULL, CVAR_ARCHIVE};
cvar_t	sb_showaddress		= {"sb_showaddress",	"0",	NULL, CVAR_ARCHIVE};
cvar_t	sb_showmap			= {"sb_showmap",		"0",	NULL, CVAR_ARCHIVE};
cvar_t	sb_showgamedir		= {"sb_showgamedir",	"0",	NULL, CVAR_ARCHIVE};
cvar_t	sb_showplayers		= {"sb_showplayers",	"1",	NULL, CVAR_ARCHIVE};
cvar_t	sb_showfraglimit	= {"sb_showfraglimit",	"0",	NULL, CVAR_ARCHIVE};
cvar_t	sb_showtimelimit	= {"sb_showtimelimit",	"0",	NULL, CVAR_ARCHIVE};

cvar_t	sb_filterkey		= {"sb_filterkey",		"hostname", NULL, CVAR_ARCHIVE};
cvar_t	sb_filtervalue		= {"sb_filtervalue",	"",		NULL, CVAR_ARCHIVE};

void M_Serverlist_Init(void)
{
	char *grp = "Server Browser Vars";

	Cvar_Register(&sb_hideempty, grp);
	Cvar_Register(&sb_hidenotempty, grp);
	Cvar_Register(&sb_hidefull, grp);
	Cvar_Register(&sb_hidedead, grp);
	Cvar_Register(&sb_hidequake2, grp);
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
}


static void NM_DrawColouredCharacter (int cx, int line, unsigned int num)
{
	Draw_ColouredCharacter(cx, line, num);
}
static void NM_Print (int cx, int cy, qbyte *str)
{
	while (*str)
	{
		Draw_ColouredCharacter (cx, cy, (*str)|128|M_COLOR_WHITE);
		str++;
		cx += 8;
	}
}

static void NM_PrintWhite (int cx, int cy, qbyte *str)
{
	while (*str)
	{
		Draw_ColouredCharacter (cx, cy, (*str)+M_COLOR_WHITE);
		str++;
		cx += 8;
	}
}

static void NM_PrintColoured (int cx, int cy, int colour, qbyte *str)
{
	while (*str)
	{
		NM_DrawColouredCharacter (cx, cy, (*str) + (colour<<8));
		str++;
		cx += 8;
	}
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
#ifdef NQPROT
	if (sb_hidenetquake.value)
#endif
		if (server->special & SS_NETQUAKE)
			return true;
	if (sb_hidequakeworld.value)
		if (!(server->special & (SS_QUAKE2|SS_NETQUAKE)))
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
			Draw_Fill (12, y, 28, 4, Sbar_ColorForMap(selectedserver.detail->players[i].topc));
			Draw_Fill (12, y+4, 28, 4, Sbar_ColorForMap(selectedserver.detail->players[i].botc));
			NM_PrintWhite (12, y, va("%3i", selectedserver.detail->players[i].frags));
			NM_Print (12+8*4, y, selectedserver.detail->players[i].name);
		}
		y+=8;
	}

	if (y<=miny)	//whoops, there was a hole at the end, try scrolling up.
		selectedserver.linenum--;
}

int M_AddColumn (int right, int y, char *text, int maxchars, int colour)
{
	int left;
	left = right - maxchars*8;
	if (left < 0)
		return right;

	right = left;
	while (*text && maxchars>0)
	{
		NM_DrawColouredCharacter (right, y, (*(unsigned char *)text) + (colour<<8));
		text++;
		right += 8;
		maxchars--;
	}
	return left;
}
void M_DrawServerList(void)
{
	serverinfo_t *server;
	int op=0, filtered=0;
	int snum=0;
	int blink = 0;
	int colour;

	int x;
	int y = 8*3;

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
		x = M_AddColumn(x, y, "tl",			3, 1);
	if (sb_showfraglimit.value)
		x = M_AddColumn(x, y, "fl",			3, 1);
	if (sb_showplayers.value)
		x = M_AddColumn(x, y, "plyrs",		6, 1);
	if (sb_showmap.value)
		x = M_AddColumn(x, y, "map",		9, 1);
	if (sb_showgamedir.value)
		x = M_AddColumn(x, y, "gamedir",	9, 1);
	if (sb_showping.value)
		x = M_AddColumn(x, y, "png",		4, 1);
	if (sb_showaddress.value)
		x = M_AddColumn(x, y, "address",	21, 1);
	x = M_AddColumn(x, y, "name",		x/8-1, 1);

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
			blink = (int)(realtime*3)&1;
		if (*server->name)
		{
			if (blink)
				colour = COLOR_CYAN;
			else if (server->special & SS_FAVORITE)
				colour = COLOR_GREEN;
			else if (server->special & SS_FTESERVER)
				colour = COLOR_RED;
			else if (server->special & SS_QUAKE2)
				colour = COLOR_YELLOW;
			else if (server->special & SS_NETQUAKE)
				colour = COLOR_MAGENTA;
			else
				colour = COLOR_WHITE;

			x = vid.width;

			if (sb_showtimelimit.value)
				x = M_AddColumn(x, y, va("%i", server->tl),			3, colour);	//time limit
			if (sb_showfraglimit.value)
				x = M_AddColumn(x, y, va("%i", server->fl),			3, colour);	//frag limit
			if (sb_showplayers.value)
				x = M_AddColumn(x, y, va("%i/%i", server->players, server->maxplayers),			6, colour);
			if (sb_showmap.value)
				x = M_AddColumn(x, y, server->map,		9, colour);
			if (sb_showgamedir.value)
				x = M_AddColumn(x, y, server->gamedir,	9, colour);
			if (sb_showping.value)
				x = M_AddColumn(x, y, va("%i", server->ping),			4, colour);	//frag limit
			if (sb_showaddress.value)
				x = M_AddColumn(x, y, NET_AdrToString(server->adr),	21, colour);
			x = M_AddColumn(x, y, server->name,		x/8-1, colour);
		}

		blink = 0;
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
	int blink;
	int snum=0;
	int op;
	int y = 3*8;
	master_t *mast;

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

		if (slist_option == snum)
			blink = (int)(realtime*3)&1;
		else
			blink = 0;

		if (blink)
			NM_PrintColoured(46, y, 6, va("%s", mast->name));	//blinking.
		else if (mast->type == MT_MASTERQW || mast->type == MT_MASTERQ2)
			NM_PrintColoured(46, y, COLOR_WHITE, va("%s", mast->name));	//white.
#ifdef NQPROT
		else if (mast->type == MT_SINGLENQ)
			NM_PrintColoured(46, y, COLOR_GREEN, va("%s", mast->name));	//green.
#endif
		else if (mast->type == MT_SINGLEQW || mast->type == MT_SINGLEQ2)
			NM_PrintColoured(46, y, COLOR_GREEN, va("%s", mast->name));	//green.
		else
			NM_PrintColoured(46, y, COLOR_RED, va("%s", mast->name));	//red.
		y+=8;
		snum++;
	}
}

#define NUMSLISTOPTIONS (7+7+3)
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
		{"Using map",		&sb_mapname,	2}
	};

void M_DrawSListOptions (void)
{
	int c;
	int op;

	slist_numoptions = NUMSLISTOPTIONS;

	for (op = 0; op < NUMSLISTOPTIONS; op++)
	{
		if (slist_option == op && (int)(realtime*3)&1)
			c = COLOR_CYAN;	//cyan
		else
			c = (options[op].cvar->value>0 || (*options[op].cvar->string && *options[op].cvar->string != '0'))?COLOR_RED:COLOR_WHITE;//red if on.
		switch(options[op].type)
		{
		default:
			NM_PrintColoured(46, op*8+8*3, c, options[op].title);
			break;
		case 1:
			if (!options[op].cvar->value)
			{
				NM_PrintColoured(46, op*8+8*3, c, va("%s ", options[op].title));
				break;
			}
		case 2:
			NM_PrintColoured(46, op*8+8*3, c, va("%s %s", options[op].title, options[op].cvar->string));
			break;
		}
	}
}

void M_SListOptions_Key (int key)
{
	if (key == K_UPARROW)
	{
		slist_option--;
		if (slist_option<0)
			slist_option=0;
	}
	else if (key == K_DOWNARROW)
	{
		slist_option++;
		if (slist_option >= slist_numoptions)
			slist_option = slist_numoptions-1;
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
		if ((key >= '0' && key <= '9') || (key >= 'a' && key <= 'z') || key == '_')
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
		NM_PrintColoured(width*snum+width/2 - strlen(titles[snum])*4, 0, slist_type==snum?COLOR_RED:COLOR_WHITE, titles[snum]);
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

		if (slist_type == SLISTTYPE_SERVERS)
			SListOptionChanged(M_FindCurrentServer());	//go for these early.
	}
	else if (key == K_DOWNARROW)
	{
		slist_option++;
		if (slist_option >= slist_numoptions)
			slist_option = slist_numoptions-1;

		if (slist_type == SLISTTYPE_SERVERS)
			SListOptionChanged(M_FindCurrentServer());	//go for these early.
	}
	else if (key == K_PGDN)
	{
		slist_option+=10;
		if (slist_option >= slist_numoptions)
			slist_option = slist_numoptions-1;

		if (slist_type == SLISTTYPE_SERVERS)
			SListOptionChanged(M_FindCurrentServer());	//go for these early.
	}
	else if (key == K_PGUP)
	{
		slist_option-=10;
		if (slist_option<0)
			slist_option=0;

		if (slist_type == SLISTTYPE_SERVERS)
			SListOptionChanged(M_FindCurrentServer());	//go for these early.
	}
	else if (key == 'r')
		MasterInfo_Begin();
	else if (key == K_SPACE)
	{
		if (slist_type == SLISTTYPE_SERVERS)
		{
			selectedserver.inuse = !selectedserver.inuse;
			if (selectedserver.inuse)
				SListOptionChanged(M_FindCurrentServer());
		}
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
				Cbuf_AddText(va("nqconnect %s\n", NET_AdrToString(server->adr)), RESTRICT_LOCAL);
			else
				Cbuf_AddText(va("connect %s\n", NET_AdrToString(server->adr)), RESTRICT_LOCAL);

			M_ToggleMenu_f();
			M_ToggleMenu_f();
		}
		else if (slist_type == SLISTTYPE_SOURCES)
		{
			MasterInfo_Request(M_FindCurrentMaster());
		}

		return;
	}	
}
#endif
