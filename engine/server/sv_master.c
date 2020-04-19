#include "quakedef.h"

#ifdef SV_MASTER

#ifdef _WIN32
#include "winquake.h"
#ifdef _MSC_VER
#include "wsipx.h"
#endif
#endif

#include "netinc.h"
#include "fs.h"

//quakeworld protocol
//    heartbeat: "a"
//        query: "c\n%i<sequence>\n%i<numplayers>\n"
//queryresponse: "d\naaaappaaaapp"
#define QUAKEWORLDPROTOCOLNAME "FTE-Quake"

//quake2 protocol
//    heartbeat: "heartbeat\n%s<serverinfo>\n%i<frags> %i<ping> \"%s\"<name>\n<repeat>"
//        query: "query\0"
//queryresponse: "servers\naaaappaaaapp"
#define QUAKE2PROTOCOLNAME "Quake2"

//quake3/dpmaster protocol
//    heartbeat: "heartbeat DarkPlaces\n"
//        query: "getservers[Ext<ipv6>] [%s<game>] %u<version> [empty] [full] [ipv6]"
//queryresponse: "getservers[Ext]Response\\aaaapp/aaaaaaaaaaaapp\\EOF"
#define QUAKE3PROTOCOLNAME "Quake3"

enum gametypes_e
{
	GT_FFA=0,
	GT_TOURNEY=1,
	GT_TEAM=3,
	GT_CTF=4,
};
typedef struct svm_server_s {
	netadr_t adr;
	const char *brokerid;	//from rtc broker, for ICE connections (persistent until killed).
	int protover;
	unsigned int clients;
	unsigned int maxclients;
	int needpass;
	char hostname[48];	//just for our own listings.
	char mapname[16];	//just for our own listings.
	char gamedir[16];	//again...
	unsigned short gametype;
	float expiretime;

	bucket_t bucket;	//for faster address lookups.
	struct svm_game_s *game;
	struct svm_server_s *next;
} svm_server_t;

typedef struct svm_game_s {
	struct svm_game_s *next;

	svm_server_t *firstserver;
	size_t numservers;
	qboolean persistent;
	char *aliases;	//list of terminated names, terminated with a double-null
	char name[1];	//eg: Quake
} svm_game_t;

typedef struct {
	float time;

	svm_game_t *firstgame;
	size_t numgames;

	hashtable_t serverhash;
	size_t numservers;

	struct rates_s
	{
		double timestamp;
		size_t heartbeats;
		size_t queries;
		size_t junk;

		size_t drops;
		size_t adds;
	} total, stamps[60];
	size_t stampring;
	double nextstamp;
} masterserver_t;

static masterserver_t svm;
ftenet_connections_t *svm_sockets;

static void QDECL SVM_Tcpport_Callback(struct cvar_s *var, char *oldvalue)
{
	FTENET_AddToCollection(svm_sockets, var->name, var->string, NA_IP, NP_STREAM);
}
static void QDECL SVM_Port_Callback(struct cvar_s *var, char *oldvalue)
{
	FTENET_AddToCollection(svm_sockets, var->name, var->string, NA_IP, NP_DGRAM);
}
static cvar_t sv_heartbeattimeout = CVARD("sv_heartbeattimeout", "300", "How many seconds a server should remain listed after its latest heartbeat. Larger values can avoid issues from packetloss, but can also make dos attacks easier.");
static cvar_t sv_masterport = CVARC("sv_masterport", STRINGIFY(PORT_QWMASTER)" "STRINGIFY(PORT_ICEBROKER), SVM_Port_Callback);
static cvar_t sv_masterport_tcp = CVARC("sv_masterport_tcp", STRINGIFY(PORT_ICEBROKER), SVM_Tcpport_Callback);
static cvar_t sv_maxgames = CVARD("sv_maxgames", "100", "Limits the number of games that may be known. This is to reduce denial of service attacks.");
static cvar_t sv_maxservers = CVARD("sv_maxservers", "10000", "Limits the number of servers (total from all games) that may be known. This is to reduce denial of service attacks.");
static cvar_t sv_hideinactivegames = CVARD("sv_hideinactivegames", "1", "Don't show known games that currently have no servers in html listings.");
static cvar_t sv_sortlist = CVARD("sv_sortlist", "3", "Controls sorting of the http output:\n0: don't bother\n1: clients then address\n2: hostname then address\n3: clients then hostname then address\n4: just address");
static cvar_t sv_hostname = CVARD("hostname", "Unnamed FTE-Master", "Controls sorting of the http output:\n0: don't bother\n1: clients then address\n2: hostname then address\n3: clients then hostname then address\n4: just address");
static char *master_css;

static unsigned int SVM_GenerateBrokerKey(const char *brokerid)
{
	return Hash_Key(brokerid, svm.serverhash.numbuckets);
}

//returns a hash key for a server's address.
static unsigned int SVM_GenerateAddressKey(const netadr_t *adr)
{
	unsigned int key;
	switch(adr->type)
	{
	case NA_IP:
		key  = *(const unsigned int*)adr->address.ip;
		key ^= 0xffff0000;	//match ipv6's ipv4-mapped addresses.
		key ^= adr->port;
		break;
	case NA_IPV6:
		key  = *(const unsigned int*)(adr->address.ip6+0);
		key ^= *(const unsigned int*)(adr->address.ip6+4);
		key ^= *(const unsigned int*)(adr->address.ip6+8);
		key ^= *(const unsigned int*)(adr->address.ip6+12);
		key ^= adr->port;
		break;
	default:
		key = 0;
		break;
	}
	return key;
}
static svm_server_t *SVM_GetServer(netadr_t *adr)
{
	svm_server_t *server;
	unsigned int key = SVM_GenerateAddressKey(adr);

	server = Hash_GetKey(&svm.serverhash, key);
	while (server)
	{
		if (!server->brokerid)	//don't report brokered servers by address.
			if (NET_CompareAdr(&server->adr, adr))
				return server;
		server = Hash_GetNextKey(&svm.serverhash, key, server);
	}
	return NULL;
}

static svm_game_t *SVM_FindGame(const char *game, int create)
{
	svm_game_t *g, **link;
	const char *sanitise;
	const char *a;
	for (g = svm.firstgame; g; g = g->next)
	{
		if (!Q_strcasecmp(game, g->name))
			return g;

		if (g->aliases)
		{
			for (a = g->aliases; *a; a+=strlen(a)+1)
			{
				if (!Q_strcasecmp(game, a))
					return g;
			}
		}
	}

	if (create)
	{
		if (create != 2)
		{
			if (svm.numgames >= sv_maxgames.ival)
			{
				Con_DPrintf("game limit exceeded\n");
				return NULL;
			}
			//block some chars that may cause issues/exploits. sorry.
			for (sanitise = game; *sanitise; sanitise++)
			{
				if ((*sanitise >= 'a' && *sanitise <= 'z') ||	//allow lowercase
					(*sanitise >= 'A' && *sanitise <= 'Z') ||	//allow uppercase
					(*sanitise >= '0' && *sanitise <= '9') ||	//allow numbers (but not leeding, see below)
					(*sanitise == '-' || *sanitise == '_'))		//allow a little punctuation, to make up for the lack of spaces.
					continue;
				return NULL;
			}
		}
		if (!*game || (*game >= '0' && *game <= '9'))
			return NULL;	//must not start with a number either.
		g = ZF_Malloc(sizeof(*g) + strlen(game));
		if (g)
		{
			char *n;
			strcpy(g->name, game);
			for (n = g->name; *n; n++)
			{	//some extra fixups, because formalnames are messy.
				if (*n == ' ' || *n == '\t' || *n == ':' || *n == '?' || *n == '#' || *n == '.')
					*n = '_';
			}
			g->persistent = create==2;
			g->next = NULL;

			//add it at the end.
			for (link = &svm.firstgame; *link; link = &(*link)->next)
				;
			*link = g;
			svm.numgames++;
			Con_DPrintf("Creating game \"%s\"\n", g->name);
		}
	}
	return g;
}

static int QDECL SVM_SortOrder(const void *v1, const void *v2)
{
	svm_server_t const*const s1 = *(svm_server_t const*const*const)v1;
	svm_server_t const*const s2 = *(svm_server_t const*const*const)v2;
	int t, i;
	if (sv_sortlist.ival&8)
		return s1->expiretime > s2->expiretime;

	if (sv_sortlist.ival&1)
		if ((t=(s2->clients-s1->clients)))
			return (t>0)?1:-1;
	if (sv_sortlist.ival&2)
		if ((t=strcmp(s1->hostname, s2->hostname)))
			return (t>0)?1:-1;

	//sort by scheme, address family, and ip
	if ((t=(s1->adr.prot-s2->adr.prot)))
		return (t>0)?1:-1;
	if ((t=(s1->adr.type-s2->adr.type)))
		return (t>0)?1:-1;
	if (s1->adr.type==NA_IP)
		i = sizeof(s1->adr.address.ip);
	else if (s1->adr.type==NA_IPV6)
		i = sizeof(s1->adr.address.ip6);
	else i = 0;
	for(t = 0; t < i; t++)
		if (s1->adr.address.ip6[i] != s2->adr.address.ip6[i])
			return (s2->adr.address.ip6[i]>s1->adr.address.ip6[i])?1:-1;

	//and now do port numbers too.
	t = BigShort(s1->adr.port) - BigShort(s2->adr.port);
	if (t)
		return (t>0)?1:-1;
	return 0;
}

static void SVM_SortServers(svm_game_t *game)
{
	svm_server_t **serverlink, *s;
	svm_server_t **sv = malloc(sizeof(*sv)*game->numservers);
	int i;
	if (!sv_sortlist.ival)
		return;

	for (i=0, s = game->firstserver; s; s = s->next)
		sv[i++] = s;
	qsort(sv, i, sizeof(*sv), SVM_SortOrder);

	for (i = 0, serverlink = &game->firstserver; i < game->numservers; i++)
	{
		*serverlink = sv[i];
		serverlink = &sv[i]->next;
	}
	*serverlink = NULL;
}

static void SVM_RemoveOldServers(void)
{
	svm_game_t **gamelink, *g;
	svm_server_t **serverlink, *s;
	for (gamelink = &svm.firstgame; (g=*gamelink); )
	{
		for (serverlink = &g->firstserver; (s=*serverlink); )
		{
			//brokered servers don't time out (they drop when their tcp connection dies)
			if (!s->brokerid && s->expiretime < svm.time)
			{
				if (developer.ival)
				{
					char buf[256];
					Con_Printf("timeout: %s\n", NET_AdrToString(buf, sizeof(buf), &s->adr));
				}

				Hash_RemoveDataKey(&svm.serverhash, SVM_GenerateAddressKey(&s->adr), s);

				svm.total.drops++;
				*serverlink = s->next;
				BZ_Free(s);
				g->numservers--;
				svm.numservers--;
			}
			else
				serverlink = &s->next;
		}

		if (!g->firstserver && !g->persistent)
		{
			Con_DPrintf("game \"%s\" has no active servers\n", g->name);
			*gamelink = g->next;
			Z_Free(g);
			svm.numgames--;
		}
		else
			gamelink = &g->next;
	}
}

int SVM_AddIPAddresses(sizebuf_t *sb, int first, int ver, const char *gamename, int v4, int v6, qboolean empty, qboolean full, qboolean prefixes, int gametype)
{
	int number = 0;
	svm_server_t *server;
	int prefix;
	int len;
	svm_game_t *game = SVM_FindGame(gamename, false);
	if (game)
	{
		for (server = game->firstserver; server; server = server->next)
		{
			if (number == first)
				break;

			first--;
		}

		for (; server; server = server->next)
		{
			if (server->protover != ver)
				continue;
			if (server->clients == 0 && !empty)
				continue;
			if (server->clients >= server->maxclients && !full)
				continue;
			if (gametype != -1 && server->gametype != gametype)
				continue;
			switch(server->adr.type)
			{
			case NA_IP:
				if (!v4)
					continue;
				prefix = '\\';
				len = 4;
				break;
			case NA_IPV6:
				if (!v6)
					continue;
				prefix = '/';
				len = 16;
				break;
			default:
				continue;
			}

			if (prefixes)
				MSG_WriteByte(sb, prefix);

			SZ_Write(sb, server->adr.address.ip, len);
			MSG_WriteShort(sb, server->adr.port);

			number++;
		}
	}
	return number;
}

static char *QuakeCharsToHTML(char *outhtml, size_t outsize, const char *quake, qboolean deunderscore)
{
	char *ret = outhtml;
	conchar_t chars[8192], *c=chars, *end;
	unsigned int codeflags, codepoint, oldflags=CON_WHITEMASK;
	unsigned int b;

	const char *htmlnames[16] = {
		"#000000",	//black
		"#0000AA",	//dark blue
		"#00AA00",	//dark green
		"#00AAAA",	//dark cyan
		"#AA0000",	//dark red
		"#AA00AA",	//dark magenta
		"#AA5500",	//brown
		"#AAAAAA",	//grey
		"#555555",	//dark grey
		"#5555FF",	//blue
		"#55FF55",	//green
		"#55FFFF",	//cyan
		"#FF5555",	//red
		"#FF55FF",	//magenta
		"#FFFF55",	//yellow
		"#FFFFFF",	//white
	};

	if (!outsize--)
		return NULL;	//no space for the null

	end = COM_ParseFunString(oldflags, quake, chars, sizeof(chars), false);
	while (c < end)
	{
		c = Font_Decode(c, &codeflags, &codepoint);

		if (codeflags & CON_HIDDEN)
			continue; //erk...?

		if (codeflags & CON_RICHFORECOLOUR)	//fixme...?
			codeflags = CON_WHITEMASK;
		if ((codeflags&CON_FGMASK) != (oldflags&CON_FGMASK))
		{
			if (oldflags != CON_WHITEMASK)
			{
				Q_strncpyz(outhtml, "</span>", outsize);
				b=strlen(outhtml);
				outhtml += b;
				outsize -= b;
			}
			if (codeflags != CON_WHITEMASK)
			{
				Q_snprintfz(outhtml, outsize, "<span style=\"color:%s\">", htmlnames[(codeflags&CON_FGMASK)>>CON_FGSHIFT]);
				b=strlen(outhtml);
				outhtml += b;
				outsize -= b;
			}
			oldflags = codeflags;
		}
		if (codepoint == '<')
		{
			Q_strncpyz(outhtml, "&lt;", outsize);
			b=strlen(outhtml);
		}
		else if (codepoint == '>')
		{
			Q_strncpyz(outhtml, "&gt;", outsize);
			b=strlen(outhtml);
		}
		else if (codepoint == '&')
		{
			Q_strncpyz(outhtml, "&amp;", outsize);
			b=strlen(outhtml);
		}
		else if (codepoint == '\"')
		{
			Q_strncpyz(outhtml, "&quot;", outsize);
			b=strlen(outhtml);
		}
		else if (codepoint == '\'')
		{
			Q_strncpyz(outhtml, "&apos;", outsize);
			b=strlen(outhtml);
		}
		else if (codepoint == '_' && deunderscore)
			*outhtml = ' ', b =1;
		else
			b = utf8_encode(outhtml, codepoint, outsize);
		if (b > 0)
		{
			outhtml += b;
			outsize -= b;
		}
	}
	*outhtml = 0;
	return ret;
}

static void SVM_Init(void)
{
	master_css = FS_MallocFile("master.css", FS_ROOT, NULL);
	if (!master_css)
		master_css = Z_StrDup(
			"<style type=\"text/css\">"
					"body {"
						"background-color:	#303030;"
						"color:			#998080;"
						"font-family:		verdanah, trebuchet ms, arial, sans-serif;"
					"}"
					"a         { text-decoration: none; }"
					"a:link    { color: #308090; }"
					"a:visited { color: #308090; }"
					"a:hover   { color: #308090; text-decoration: underline; }"
					".header { text-align: center; }"
					".footer { text-align: center; }"
					".main { width: 1024px; margin: 0 auto; }"
					"H1 {"
						"text-align: center;"
						"background-color:	#202020;"
						"color:			#808080;"
					"}"
					"table { width: 100%; border-collapse: collapse; }"
					"th { text-align: left; }"
					"tr:hover { background-color:	#202020; }"
				"</style>"
			);
}

vfsfile_t *SVM_GenerateIndex(const char *requesthost, const char *fname)
{
	char tmpbuf[256];
	char hostname[1024];
	const char *url;
	svm_game_t *game;
	svm_server_t *server;
	vfsfile_t *f = NULL;
	unsigned clients = 0, maxclients=0, totalclients=0;
	if (!master_css)
		SVM_Init();
	if (!strcmp(fname, "index.html"))
	{
		f = VFSPIPE_Open(1, false);
		VFS_PRINTF(f, "%s", master_css);
		VFS_PRINTF(f, "<h1>%s</h1>\n", sv_hostname.string);
		VFS_PRINTF(f, "<table border=1>\n");
		VFS_PRINTF(f, "<tr><th>Active Games</th><th>Players</th><th>Server Count</th></tr>\n");
		for (game = svm.firstgame; game; game = game->next)
		{
			for (clients=0, server = game->firstserver; server; server = server->next)
				clients += server->clients;
			if (game->numservers || !sv_hideinactivegames.ival)	//only show active servers
			{
				QuakeCharsToHTML(tmpbuf, sizeof(tmpbuf), game->name, true);
				VFS_PRINTF(f, "<tr><td><a href=\"game/%s\">%s</a></td><td>%u player(s)</td><td>%u server(s)</td></tr>\n", game->name, tmpbuf, clients, (unsigned)game->numservers);
			}
			totalclients += clients;
		}
		VFS_PRINTF(f, "</table>\n");
		VFS_PRINTF(f, "%u game(s), %u player(s), %u server(s)\n", (unsigned)svm.numgames, totalclients, (unsigned)svm.numservers);
	}
	else if (!strncmp(fname, "server/", 7))
	{
		netadr_t adr[64];
		int count;

		f = VFSPIPE_Open(1, false);
		VFS_PRINTF(f, "%s", master_css);
		VFS_PRINTF(f, "<h1>Single Server Info</h1>\n");

		VFS_PRINTF(f, "<table border=1>\n");
		VFS_PRINTF(f, "<tr><th>Game</th><th>Address</th><th>Hostname</th><th>Mod dir</th><th>Mapname</th><th>Players</th></tr>\n");
		//FIXME: block dns lookups here?
		count = NET_StringToAdr2(fname+7, 0, adr, countof(adr), NULL);
		while(count-->0)
		{
			server = SVM_GetServer(&adr[count]);
			if (server)
			{
				QuakeCharsToHTML(hostname, sizeof(hostname), server->hostname, false);
				VFS_PRINTF(f, "<tr><td>%s</td><td>%s</td><td>%s%s</td><td>%s</td><td>%s</td><td>%u/%u</td></tr>\n", server->game?server->game->name:"Unknown", NET_AdrToString(tmpbuf, sizeof(tmpbuf), &server->adr), (server->needpass&1)?"&#x1F512;":"", hostname, server->gamedir, server->mapname, server->clients, server->maxclients);
			}
			else
				VFS_PRINTF(f, "<tr><td>?</td><td>%s</td><td>?</td><td>?</td><td>?</td><td>?/?</td></tr>\n", NET_AdrToString(tmpbuf, sizeof(tmpbuf), &adr[count]));
		}
		VFS_PRINTF(f, "</table>\n");
	}
	else if (!strncmp(fname, "game/", 5))
	{
		const char *gamename = fname+5;
		game = SVM_FindGame(gamename, false);

		f = VFSPIPE_Open(1, false);
		VFS_PRINTF(f, "%s", master_css);

		if (!strcmp(gamename, "UNKNOWN"))
		{
			VFS_PRINTF(f, "<h1>Unresponsive Servers</h1>\n");
			VFS_PRINTF(f, "These servers have sent a heartbeat but have failed to respond to an appropriate query from a different port. This can happen when:<ul>"
			"<li>Server is firewalled and all inbound packets are dropped. Try reconfiguring your firewall to allow packets to that process or port.</li>"
			"<li>An intermediate router implements an Address/Port-Dependant-Filtering NAT. Try setting up port forwarding.</li>"
			"<li>Outgoing connections are asynchronous with regard to port forwarding. Such servers will show with arbitrary ephemerial ports. Docker: you can supposedly work around this with --net=host.</li>"
			"<li>Plain and simple packet loss, servers in this state for less than 5 mins may still be fine.</li>"
			"<li>Server crashed or was reconfigured before it could respond.</li>"
			"<li>MTU limits with failed defragmentation.</li>"
			"<li>Routing table misconfiguration.</li>"
			"<li>Other.</li>"
			"</ul>\n");
		}
		else
			VFS_PRINTF(f, "<h1>Servers for %s</h1>\n", QuakeCharsToHTML(tmpbuf, sizeof(tmpbuf), gamename, true));

		if(game)
		{
			SVM_SortServers(game);

			VFS_PRINTF(f, "<table border=1>\n");
			VFS_PRINTF(f, "<tr><th>Address</th><th>Hostname</th><th>Gamedir</th><th>Mapname</th><th>Players</th></tr>\n");
			for (server = game->firstserver; server; server = server->next)
			{
				if (server->brokerid)
				{
					url = tmpbuf;
					Q_snprintfz(tmpbuf, sizeof(tmpbuf), "rtc://%s/%s", requesthost, server->brokerid);
				}
				else
					url = NET_AdrToString(tmpbuf, sizeof(tmpbuf), &server->adr);
				QuakeCharsToHTML(hostname, sizeof(hostname), server->hostname, false);
				VFS_PRINTF(f, "<tr><td>%s</td><td>%s%s</td><td>%s</td><td>%s</td><td>%u/%u</td></tr>\n", url, (server->needpass&1)?"&#x1F512;":"", hostname, server->gamedir, server->mapname, server->clients, server->maxclients);
				clients += server->clients;
				maxclients += server->maxclients;
			}
			VFS_PRINTF(f, "</table>\n");
			VFS_PRINTF(f, "%u server(s), %u/%u client(s)\n", (unsigned)game->numservers, clients, maxclients);
		}
		else
			VFS_PRINTF(f, "Protocol '%s' is not known\n", gamename);
	}
	else if (!strncmp(fname, "raw/", 4))
	{	//just spews all
		COM_StripExtension(fname+4, tmpbuf, sizeof(tmpbuf));
		game = SVM_FindGame(tmpbuf, false);

		f = VFSPIPE_Open(1, false);
		VFS_PRINTF(f, "#Server list for \"%s\"\n", tmpbuf);
		for (server = (game?game->firstserver:NULL); server; server = server->next)
		{
			if (server->brokerid)
				VFS_PRINTF(f, "rtc:///%s \\maxclients\\%u\\clients\\%u\\hostname\\%s\\modname\\%s\\mapname\\%s\\needpass\\%i\n", server->brokerid, server->maxclients, server->clients, server->hostname, server->gamedir, server->mapname, server->needpass);
			else
				VFS_PRINTF(f, "%s\n", NET_AdrToString(tmpbuf, sizeof(tmpbuf), &server->adr));
		}
	}
	return f;
}

static svm_game_t *SVM_GameFromBrokerID(const char **brokerid)
{
	size_t l;
	char name[128];
	const char *in = *brokerid;
	if (*in == '/')
		in++;
	for (l = 0; *in && *in != '/' && *in != '?' && *in != '#'; in++)
		if (l < countof(name)-1)
			name[l++] = *in;
	name[l] = 0;
	if (*in == '/')
		in++;
	else
		return NULL;	//only one? no game specified? get lost.
	*brokerid = in;
	return SVM_FindGame(name, true);
}
static svm_server_t *SVM_FindBrokerHost(const char *brokerid)
{
	svm_server_t *server;
	unsigned int key = SVM_GenerateBrokerKey(brokerid);

	server = Hash_GetKey(&svm.serverhash, key);
	while (server)
	{
		if (server->brokerid)	//don't report brokered servers by address.
			if (server->brokerid == brokerid)
				return server;
		server = Hash_GetNextKey(&svm.serverhash, key, server);
	}
	return NULL;
}
void SVM_RemoveBrokerGame(const char *brokerid)
{
	svm_server_t *s, **link;
	svm_game_t *game = SVM_GameFromBrokerID(&brokerid);
	if (!game)
	{
		Con_Printf("SVM_RemoveBrokerGame: failed to find game for brokered server: %s\n", brokerid);
		return;
	}

	for (link = &game->firstserver; (s=*link); )
	{
		if (s->brokerid == brokerid)
		{
			*link = s->next;
			Hash_RemoveDataKey(&svm.serverhash, SVM_GenerateBrokerKey(brokerid), s);
			Z_Free(s);
			game->numservers--;
			svm.numservers--;
			return;
		}
		else
			link = &s->next;
	}

	Con_Printf("SVM_RemoveBrokerGame: failed to remove brokered server: %s\n", brokerid);
}
void SVM_AddBrokerGame(const char *brokerid, const char *info)
{
	svm_game_t *game = SVM_GameFromBrokerID(&brokerid);
	svm_server_t *server = SVM_FindBrokerHost(brokerid);
	if (!server)
	{
		if (!game)
			return;
		if (svm.numservers >= sv_maxservers.ival)
		{
			Con_DPrintf("server limit exceeded\n");
			return;
		}
		Con_DPrintf("heartbeat(new - %s): /%s\n", game->name, brokerid);

		server = Z_Malloc(sizeof(svm_server_t));
		server->game = game;
		server->brokerid = brokerid;

		server->next = game->firstserver;
		game->firstserver = server;
		game->numservers++;
		svm.numservers++;

		svm.total.adds++;

		Hash_AddKey(&svm.serverhash, SVM_GenerateBrokerKey(brokerid), server, &server->bucket);
	}
	else
		Con_DPrintf("heartbeat(update - %s): /%s\n", game->name, brokerid);

	server->protover = atoi(Info_ValueForKey(info, "protocol"));
	server->maxclients = atoi(Info_ValueForKey(info, "maxclients"));
	server->clients = atoi(Info_ValueForKey(info, "clients"));
	Q_strncpyz(server->hostname, Info_ValueForKey(info, "hostname"), sizeof(server->hostname));
	Q_strncpyz(server->gamedir, Info_ValueForKey(info, "modname"), sizeof(server->gamedir));
	Q_strncpyz(server->mapname, Info_ValueForKey(info, "mapname"), sizeof(server->mapname));
}

static svm_server_t *SVM_Heartbeat(const char *gamename, netadr_t *adr, int numclients, float validuntil)
{
	svm_server_t *server = SVM_GetServer(adr);
	svm_game_t *game;

	if (!gamename)
	{	//no gamename is a placeholder server, to say that there's a server there but it isn't responding to our getinfos... (ie: to list misconfigured servers too)
		if (server)
		{	//it still exists, renew it, but don't otherwise care too much.
			server->expiretime = validuntil;
			return server;
		}
		game = SVM_FindGame("UNKNOWN", true);
	}
	else
	{
		game = SVM_FindGame(gamename, true);
		if (!game)
			return NULL;
	}

	if (server && server->game != game)
	{
		server->expiretime = realtime - 1;
		server = NULL;
	}

	if (!server)	//not found
	{
		if (svm.numservers >= sv_maxservers.ival)
		{
			Con_DPrintf("server limit exceeded\n");
			return NULL;
		}
		if (developer.ival)
		{
			char buf[256];
			Con_Printf("heartbeat(new - %s): %s\n", game->name, NET_AdrToString(buf, sizeof(buf), adr));
		}

		server = Z_Malloc(sizeof(svm_server_t));
		server->game = game;
		server->next = game->firstserver;
		game->firstserver = server;
		game->numservers++;
		svm.numservers++;

		server->adr = *adr;

		svm.total.adds++;

		Hash_AddKey(&svm.serverhash, SVM_GenerateAddressKey(adr), server, &server->bucket);
	}
	else
	{
		if (developer.ival)
		{
			char buf[256];
			Con_Printf("heartbeat(refresh): %s\n", NET_AdrToString(buf, sizeof(buf), &server->adr));
		}
	}

	server->clients = numclients;
	server->expiretime = validuntil;
	return server;
}

void SVM_GenChallenge(char *out, size_t outsize, netadr_t *foradr)
{	//this function needs to return some sort of unguessable string so that you can't spoof the server with fake responses
	char adr[64];
	static char randumb[16];
	char digest[256];
	void *ctx = alloca(hash_sha1.contextsize);

	if (!*randumb)
	{
		int i;
		srand(time(NULL));	//lame
		for (i = 0; i < sizeof(randumb)-1; i++)
			while (!randumb[i])
				randumb[i] = rand();
	}
	NET_AdrToString(adr, sizeof(adr), foradr);

	hash_sha1.init(ctx);
	hash_sha1.process(ctx, randumb, sizeof(randumb)-1);
	hash_sha1.process(ctx, adr, strlen(adr));
	hash_sha1.terminate(digest, ctx);

	Base64_EncodeBlock(digest, hash_sha1.digestsize, out, outsize);
}

//switch net_from's reported connection, so we reply from a different udp socket from the one a packet was received from.
static qboolean SVM_SwitchQuerySocket(void)
{
	size_t c;
	//switch the info query to our other udp socket, so any firewall/nat over the server blocks it.
	//this is to prevent people from thinking that the server is actually accessible.
	for (c = 0; c < countof(svm_sockets->conn); c++)
	{
		if (!svm_sockets->conn[c])
			continue;	//that one's dead, jim
		if (c+1 == net_from.connum)
			continue;	//ignore this one, its the one we received the packet from
		//make sure its a datagram connection, and not some tcp weirdness.
		if (svm_sockets->conn[c]->prot == NP_DGRAM &&
			(svm_sockets->conn[c]->addrtype[0] == net_from.type || svm_sockets->conn[c]->addrtype[1] == net_from.type))
		{	//okay, looks like we should be able to respond on this one. lets see if their firewall stops us from finding out more about them.
			net_from.connum = c+1;
			return true;
		}
	}
	return false;
}

static void SVM_ProcessUDPPacket(void)
{
	char *s, *line;

	//we shouldn't be taking anything else...
	if (net_from.prot != NP_DGRAM)
	{
		Con_DPrintf("master: ignoring non-datagram message\n");
		return;
	}

	net_message.data[net_message.cursize] = '\0';	//null term all strings.

	//well that's annoying. why is our networking code not doing this? LAME!
	if (net_from.type == NA_IPV6 &&
		!*(int*)&net_from.address.ip6[0] &&
		!*(int*)&net_from.address.ip6[4] &&
		!*(short*)&net_from.address.ip6[8] &&
		*(short*)&net_from.address.ip6[10]==(short)0xffff)
	{	//convert this ipv4-mapped-ipv6 address back to actual ipv4, so we don't get confused about stuff.
		net_from.type = NA_IP;
		*(int*)&net_from.address.ip[0] = *(int*)&net_from.address.ip6[12];
		//and null it out, just in case.
		*(int*)&net_from.address.ip6[8]=0;
		*(int*)&net_from.address.ip6[12]=0;
	}

	if (NET_WasSpecialPacket(svm_sockets))
	{
		Con_DPrintf("master: ignoring special packet\n");
		return;
	}

	svm.time = Sys_DoubleTime();

	MSG_BeginReading(msg_nullnetprim);
	if (MSG_ReadLong() != -1 || msg_badread)
	{	//go back to start...
		MSG_BeginReading(msg_nullnetprim);
	}
	line = MSG_ReadStringLine();
	s = COM_Parse(line);
	if (!strcmp(com_token, "getservers") || !strcmp(com_token, "getserversExt"))
	{	//q3/dpmaster
		sizebuf_t sb;
		int ver;
		char *eos;
		char game[64];
		qboolean ext = !strcmp(com_token, "getserversExt");
		const char *resp=ext?"getserversExtResponse":"getserversResponse";
		qboolean empty = false;
		qboolean full = false;
		qboolean ipv4 = !ext;
		qboolean ipv6 = false;
		int gametype = -1;
		s = COM_ParseOut(s, game, sizeof(game));
		ver = strtol(game, &eos, 0);
		if (*eos)
		{	//not a number, must have been a dpmaster game name.
			s = COM_Parse(s);
			ver = strtol(com_token, NULL, 0);
		}
		else	//first arg was a number. that means its vanilla quake3.
			Q_strncpyz(game, QUAKE3PROTOCOLNAME, sizeof(game));
		for(;s&&*s;)
		{
			s = COM_Parse(s);
			if (!strcmp(com_token, "empty"))
				empty = true;
			else if (!strcmp(com_token, "full"))
				full = true;

			else if (!strcmp(com_token, "ipv4"))
				ipv4 = true;
			else if (!strcmp(com_token, "ipv6"))
				ipv6 = true;

			else if (!strcmp(com_token, "ffa"))
				gametype = GT_FFA;
			else if (!strcmp(com_token, "tourney"))
				gametype = GT_TOURNEY;
			else if (!strcmp(com_token, "team"))
				gametype = GT_TEAM;
			else if (!strcmp(com_token, "ctf"))
				gametype = GT_CTF;
			else if (!strncmp(com_token, "gametype=", 9))
				gametype = atoi(com_token+9);
			else
			{
				char buf[256];
				Con_DPrintf("Unknown request filter: %s\n", COM_QuotedString(com_token, buf, sizeof(buf), false));
			}
		}
		if (!ipv4 && !ipv6)
			ipv4 = ipv6 = true; //neither specified? use both


		svm.total.queries++;
		memset(&sb, 0, sizeof(sb));
		sb.maxsize = sizeof(net_message_buffer)-2;
		sb.data = net_message_buffer;
		MSG_WriteLong(&sb, -1);
		SZ_Write(&sb, resp, strlen(resp));	//WriteString, but without the null.
		SVM_AddIPAddresses(&sb, 0, ver, game, ipv4, ipv6, empty, full, true, gametype);
		sb.maxsize+=2;
		MSG_WriteByte(&sb, '\\');	//otherwise the last may be considered invalid and ignored.
		MSG_WriteByte(&sb, 'E');
		MSG_WriteByte(&sb, 'O');
		MSG_WriteByte(&sb, 'T');
		NET_SendPacket(svm_sockets, sb.cursize, sb.data, &net_from);
	}
	else if (!strcmp(com_token, "heartbeat"))
	{	//quake2 heartbeat. Serverinfo and players should follow.
		if (*s == '\n' && s[1] == '\\')
		{	//there's some serverinfo there, must be q2...
			svm.total.heartbeats++;
			SVM_Heartbeat(QUAKE2PROTOCOLNAME, &net_from, 0, svm.time + sv_heartbeattimeout.ival);
		}
		else
		{	//dp/q3/etc are annoying, but we can query from an emphemerial socket to check NAT rules.
			sizebuf_t sb;
			netadr_t a;

			char ourchallenge[256];
			SVM_GenChallenge(ourchallenge, sizeof(ourchallenge), &net_from);
			svm.total.queries++;

			//placeholder listing...
			if (SVM_Heartbeat(NULL, &net_from, 0, svm.time + sv_heartbeattimeout.ival))
				a = net_from;
			else
				a.type = NA_INVALID;
			if (!SVM_SwitchQuerySocket())
				a.type = NA_INVALID;

			memset(&sb, 0, sizeof(sb));
			sb.maxsize = sizeof(net_message_buffer);
			sb.data = net_message_buffer;
			MSG_WriteLong(&sb, -1);
			MSG_WriteString(&sb, va("getinfo %s\n", ourchallenge));
			sb.cursize--;
			NET_SendPacket(svm_sockets, sb.cursize, sb.data, &net_from);

			if (a.type != NA_INVALID)
			{	//they were unknown... send a special getinfo, so we can get their hostname while leaving them as 'unknown'
				memset(&sb, 0, sizeof(sb));
				sb.maxsize = sizeof(net_message_buffer);
				sb.data = net_message_buffer;
				MSG_WriteLong(&sb, -1);
				MSG_WriteString(&sb, va("getinfo ?%s\n", ourchallenge));
				sb.cursize--;
				NET_SendPacket(svm_sockets, sb.cursize, sb.data, &a);
			}
		}
	}
	else if (!strcmp(com_token, "infoResponse"))
	{
		char ourchallenge[256];
		int clients;
		const char *game, *chal;
		svm_server_t *srv;
		qboolean unknownresp = false;
		s = MSG_ReadStringLine();
		svm.total.heartbeats++;
		chal = Info_ValueForKey(s, "challenge");
		unknownresp = *chal=='?';
		chal += unknownresp?1:0;
		SVM_GenChallenge(ourchallenge, sizeof(ourchallenge), &net_from);
		if (!strcmp(chal, ourchallenge))
		{
			clients = atoi(Info_ValueForKey(s, "clients"));
			game = Info_ValueForKey(s, "gamename");
			if (!*game)
				game = QUAKE3PROTOCOLNAME;
			if (unknownresp)
				game = NULL;	//ignore the gamename and classify it as unknown. this won't break anything if we've already has a proper heartbeat from them.
			srv = SVM_Heartbeat(game, &net_from, clients, svm.time + sv_heartbeattimeout.ival);
			if (srv)
			{
				if (developer.ival)
					Info_Print(s, "\t");
				srv->clients = clients;
				if (game)
					srv->protover = atoi(Info_ValueForKey(s, "protocol"));
				srv->maxclients = atoi(Info_ValueForKey(s, "sv_maxclients"));
				srv->needpass = atoi(Info_ValueForKey(s, "needpass"));
				Q_strncpyz(srv->hostname, Info_ValueForKey(s, "hostname"), sizeof(srv->hostname));
				Q_strncpyz(srv->gamedir, Info_ValueForKey(s, "modname"), sizeof(srv->gamedir));
				Q_strncpyz(srv->mapname, Info_ValueForKey(s, "mapname"), sizeof(srv->mapname));
			}
		}
	}
	else if (!strcmp(com_token, "query"))
	{	//quake2 server listing request
		sizebuf_t sb;
		svm.total.queries++;
		memset(&sb, 0, sizeof(sb));
		sb.maxsize = sizeof(net_message_buffer);
		sb.data = net_message_buffer;
		MSG_WriteLong(&sb, -1);
		MSG_WriteString(&sb, "servers\n");
		sb.cursize--;
		SVM_AddIPAddresses(&sb, 0, 0, QUAKE2PROTOCOLNAME, true, false, true, true, false, -1);
		NET_SendPacket(svm_sockets, sb.cursize, sb.data, &net_from);
	}
	else if (*com_token == S2M_HEARTBEAT)	//sequence, players
	{	//quakeworld heartbeat
		int players;
		sizebuf_t sb;
		s = MSG_ReadStringLine();
		//sequence = atoi(s);
		s = MSG_ReadStringLine();
		players = atoi(s);
		svm.total.heartbeats++;


		//placeholder listing...
		SVM_Heartbeat(NULL, &net_from, players, svm.time + sv_heartbeattimeout.ival);
		SVM_SwitchQuerySocket();

		//send it a proper query. We'll fill in the other details on response.
		memset(&sb, 0, sizeof(sb));
		sb.maxsize = sizeof(net_message_buffer);
		sb.data = net_message_buffer;
		MSG_WriteLong(&sb, -1);
		MSG_WriteString(&sb, va("status %i\n", 1));
		sb.cursize--;
		NET_SendPacket(svm_sockets, sb.cursize, sb.data, &net_from);
	}
	else if (*com_token == A2C_PRINT)
	{	//quakeworld response from 'status' requests, providing for actual info (and so that we know its reachable from other addresses)
		//there's no challenge, these could easily be spoofed. :(
		int clients;
		const char *game;
		svm_server_t *srv;
		s = ++line;

		clients = atoi(Info_ValueForKey(s, "clients"));
		game = Info_ValueForKey(s, "gamename");
		if (!*game)
			game = QUAKEWORLDPROTOCOLNAME;
		srv = SVM_Heartbeat(game, &net_from, clients, svm.time + sv_heartbeattimeout.ival);
		if (srv)
		{
			if (developer.ival)
				Info_Print(s, "\t");
			srv->protover = 3;//atoi(Info_ValueForKey(s, "protocol"));
			srv->maxclients = atoi(Info_ValueForKey(s, "maxclients"));
			srv->needpass = atoi(Info_ValueForKey(s, "needpass"));
			Q_strncpyz(srv->hostname, Info_ValueForKey(s, "hostname"), sizeof(srv->hostname));
			Q_strncpyz(srv->gamedir, Info_ValueForKey(s, "*gamedir"), sizeof(srv->gamedir));
			Q_strncpyz(srv->mapname, Info_ValueForKey(s, "map"), sizeof(srv->mapname));
		}
	}
	else if (*com_token == C2M_MASTER_REQUEST)
	{	//quakeworld server listing request
		sizebuf_t sb;
		svm.total.queries++;
		memset(&sb, 0, sizeof(sb));
		sb.maxsize = sizeof(net_message_buffer);
		sb.data = net_message_buffer;
		MSG_WriteLong(&sb, -1);
		MSG_WriteByte(&sb, M2C_MASTER_REPLY);
		MSG_WriteByte(&sb, '\n');
		SVM_AddIPAddresses(&sb, 0, 0, QUAKEWORLDPROTOCOLNAME, true, false, true, true, false, -1);
		NET_SendPacket(svm_sockets, sb.cursize, sb.data, &net_from);
	}
	else if (*com_token == A2A_PING)
	{	//quakeworld server ping request... because we can.
		sizebuf_t sb;
		svm.total.queries++;
		memset(&sb, 0, sizeof(sb));
		sb.maxsize = sizeof(net_message_buffer);
		sb.data = net_message_buffer;
		MSG_WriteLong(&sb, -1);
		MSG_WriteByte(&sb, A2A_ACK);
		MSG_WriteByte(&sb, '\n');
		NET_SendPacket(svm_sockets, sb.cursize, sb.data, &net_from);
	}
	else if (*com_token == S2M_SHUTDOWN)
	{	//quakeworld server shutting down...
		//this isn't actually useful. we can't use it because we can't protect against spoofed denial-of-service attacks.
		//we could only use this by sending it a few pings to see if it is actually still responding. which is unreliable (especially if we're getting spammed by packet floods).
	}
	else
		svm.total.junk++;
}

void SVM_Think(int port)
{
	NET_ReadPackets (svm_sockets);
	SVM_RemoveOldServers();
}
#else
void SVM_Think(int port){}
#endif


#ifdef MASTERONLY
static void SV_Quit_f (void)
{
	Con_TPrintf ("Shutting down.\n");
	Sys_Quit ();
}
static void SVM_Status_f(void)
{
	svm_game_t *g;
	svm_server_t *s;
	unsigned clients;
	struct rates_s *s1, *s2;
	float period;
	size_t r;

	NET_PrintAddresses(svm_sockets);
	NET_PrintConnectionsStatus(svm_sockets);
	Con_Printf("Game count: %u\n", (unsigned)svm.numgames);
	for (g = svm.firstgame; g; g = g->next)
	{
		clients = 0;
		for (s = g->firstserver; s; s = s->next)
			clients += s->clients;
		Con_Printf("Game %s: %u servers, %u clients\n", g->name, (unsigned)g->numservers, clients);
	}

	s1 = &svm.total;
	r = (svm.stampring >= countof(svm.stamps)-1)?svm.stampring-countof(svm.stamps)-1:0;
	s2 = &svm.stamps[r%countof(svm.stamps)];

	period = s1->timestamp-s2->timestamp;
	period/=60;
	if (!period)
		period=1;
	Con_Printf("Heartbeats/min: %f\n", (s1->heartbeats-s2->heartbeats)/period);
	Con_Printf("Queries/min: %f\n", (s1->queries-s2->queries)/period);
}

static void SVM_RegisterAlias(svm_game_t *game, char *aliasname)
{
	const char *a;
	size_t l;
	svm_game_t *aliasgame;
	if (!game)
		return;

	//make sure we never have dupes. they confuse EVERYTHING.
	aliasgame = SVM_FindGame(aliasname, false);
	if (aliasgame == game)
		return;	//already in there somehow.
	if (aliasgame)
	{
		Con_Printf("game alias of %s is already registered\n", aliasname);
		return;
	}
	game->persistent = true;	//don't forget us!

	if (!*aliasname)
		return;

	a = game->aliases;
	if (a) for (; *a; a+=strlen(a)+1);
	l = a-game->aliases;
	game->aliases = BZ_Realloc(game->aliases, l+strlen(aliasname)+2);
	memcpy(game->aliases+l, aliasname, strlen(aliasname)+1);
	l += strlen(aliasname)+1;
	game->aliases[l] = 0;
}
static qboolean SVM_FoundManifest(void *usr, ftemanifest_t *man)
{
	svm_game_t *game;
	const char *g;
	if (man->protocolname)
	{	//FIXME: we ought to do this for each manifest we could find.
		g = man->protocolname;

#if 1
		game = SVM_FindGame(man->formalname, 2);
#else
		g = COM_Parse(g);
		game = SVM_FindGame(com_token, 2);
#endif
		while (*g)
		{
			g = COM_Parse(g);
			SVM_RegisterAlias(game, com_token);
		}
	}

	return false;
}

static void SVM_GameAlias_f(void)
{
	svm_game_t *game = SVM_FindGame(Cmd_Argv(1), 2);
	if (!game)
	{
		Con_Printf("Unable to register game %s\n", Cmd_Argv(1));
		return;
	}
	SVM_RegisterAlias(game, Cmd_Argv(2));
}
void SV_Init (struct quakeparms_s *parms)
{
	int manarg;

	COM_InitArgv (parms->argc, parms->argv);

	host_parms = *parms;

	Cvar_Init();

	Memory_Init();

	Sys_Init();

	COM_ParsePlusSets(false);

	Cbuf_Init ();
	Cmd_Init ();

	NET_Init ();
	COM_Init ();

#ifdef WEBSERVER
	IWebInit();
#endif

	Cmd_AddCommand ("quit", SV_Quit_f);
	Cmd_AddCommand ("status", SVM_Status_f);
	Cmd_AddCommand ("gamealias", SVM_GameAlias_f);

	svm_sockets = FTENET_CreateCollection(true, SVM_ProcessUDPPacket);
	Hash_InitTable(&svm.serverhash, 1024, Z_Malloc(Hash_BytesForBuckets(1024)));

	Cvar_Register(&sv_masterport, "server control variables");
	Cvar_Register(&sv_masterport_tcp, "server control variables");
	Cvar_Register(&sv_heartbeattimeout, "server control variables");
	Cvar_Register(&sv_maxgames, "server control variables");
	Cvar_Register(&sv_maxservers, "server control variables");
	Cvar_Register(&sv_hideinactivegames, "server control variables");
	Cvar_Register(&sv_sortlist, "server control variables");
	Cvar_Register(&sv_hostname, "server control variables");

	Cvar_ParseWatches();
	host_initialized = true;

	manarg = COM_CheckParm("-manifest");
	if (manarg && manarg < com_argc-1 && com_argv[manarg+1])
		FS_ChangeGame(FS_Manifest_ReadSystem(com_argv[manarg+1], NULL), true, true);
	else
		FS_ChangeGame(NULL, true, true);

	Cmd_StuffCmds();
	Cbuf_Execute ();

	Cvar_ForceCallback(&sv_masterport);
	Cvar_ForceCallback(&sv_masterport_tcp);

	SVM_FoundManifest(NULL, fs_manifest);
	FS_EnumerateKnownGames(SVM_FoundManifest, NULL);

	Con_Printf ("Exe: %s\n", version_string());

	Con_TPrintf ("======== %s Initialized ========\n", "FTEMaster");
}
float SV_Frame (void)
{
	realtime = Sys_DoubleTime();
	while (1)
	{
		const char *cmd = Sys_ConsoleInput ();
		if (!cmd)
			break;
		Log_String(LOG_CONSOLE, cmd);
		Cbuf_AddText (cmd, RESTRICT_LOCAL);
		Cbuf_AddText ("\n", RESTRICT_LOCAL);
	}
	Cbuf_Execute ();

	SVM_Think(sv_masterport.ival);

	//record lots of info over multiple frames, for smoother stats info.
	svm.total.timestamp = realtime;
	if (svm.nextstamp < realtime)
	{
		svm.stamps[svm.stampring%countof(svm.stamps)] = svm.total;
		svm.stampring++;
		svm.nextstamp = realtime+60;
	}

	return 4;
}
#endif
