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
	unsigned int spectators;	//
	unsigned int bots;			//non-human players
	unsigned int clients;		//human players
	unsigned int maxclients;	//limit of bots+clients, but not necessarily spectators.
	int needpass:1;
	int coop:1;
	char hostname[64];	//just for our own listings.
	char mapname[16];	//just for our own listings.
	char gamedir[16];	//again...
	char version[48];
	unsigned short gametype;
	double expiretime;

	bucket_t bucket;	//for faster address lookups.
	struct svm_game_s *game;
	struct svm_server_s *next;
	char rules[1024];
} svm_server_t;

typedef struct svm_game_s {
	struct svm_game_s *next;

	svm_server_t *firstserver;
	size_t numservers;
	qboolean persistent;
	char *aliases;	//list of terminated names, terminated with a double-null
	char *scheme;
	char name[1];	//eg: Quake
} svm_game_t;

typedef struct {
	double time;

	svm_game_t *firstgame;
	size_t numgames;

	hashtable_t serverhash;
	size_t numservers;

	struct rates_s
	{
		double timestamp;
		size_t heartbeats;	//heartbeats/serverinfos (general maintainence things due to server counts)
		size_t queries;		//players querying for info
		size_t junk;		//unknown packets
		size_t stun;		//special packets handled by the network layer... though I suppose these should count as queries.

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
static cvar_t sv_slaverequery = CVARD("sv_slaverequery", "120", "Requery slave masters at this frequency.");
static struct sv_masterslave_s
{
	int type;
	cvar_t var;
	size_t numaddr;
	netadr_t addr[8];
} sv_masterslave[] = {
	{0,	CVARD("sv_qwmasterslave1", "", "Specifies a different (quakeworld-protocol) master from which to steal server listings.")},
	{0,	CVARD("sv_qwmasterslave2", "", "Specifies a different (quakeworld-protocol) master from which to steal server listings.")},
	{0,	CVARD("sv_qwmasterslave3", "", "Specifies a different (quakeworld-protocol) master from which to steal server listings.")},
//	{1,	CVARD("sv_q2masterslave1", "", "Specifies a different (quake2-protocol) master from which to steal server listings.")},
//	{1,	CVARD("sv_q2masterslave2", "", "Specifies a different (quake2-protocol) master from which to steal server listings.")},
//	{1,	CVARD("sv_q2masterslave3", "", "Specifies a different (quake2-protocol) master from which to steal server listings.")},
//	{2,	CVARD("sv_q3masterslave1", "", "Specifies a different (quake3-protocol) master from which to steal server listings.")},
//	{2,	CVARD("sv_q3masterslave2", "", "Specifies a different (quake3-protocol) master from which to steal server listings.")},
//	{2,	CVARD("sv_q3masterslave3", "", "Specifies a different (quake3-protocol) master from which to steal server listings.")},
	{3,	CVARD("sv_dpmasterslave1", "", "Specifies a different (dpmaster-protocol) master from which to steal server listings.")},
	{3,	CVARD("sv_dpmasterslave2", "", "Specifies a different (dpmaster-protocol) master from which to steal server listings.")},
	{3,	CVARD("sv_dpmasterslave3", "", "Specifies a different (dpmaster-protocol) master from which to steal server listings.")},
};
static struct
{
	netadr_t a;
	char *query;
} *pingring;
static size_t pingring_first;
static size_t pingring_count;
static size_t pingring_max;
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
	{
		if ((t=(s2->clients-s1->clients)))
			return (t>0)?1:-1;
		if ((t=(s2->spectators-s1->spectators)))
			return (t>0)?1:-1;
		if ((t=(s2->bots-s1->bots)))
			return (t>0)?1:-1;
	}
	if (sv_sortlist.ival&16)
		if ((t=strcmp(s1->version, s2->version)))
			return (t>0)?1:-1;
	if (sv_sortlist.ival&32)
		if ((t=strcmp(s1->gamedir, s2->gamedir)))
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
		if (s1->adr.address.ip6[t] != s2->adr.address.ip6[t])
			return (s1->adr.address.ip6[t]>s2->adr.address.ip6[t])?1:-1;

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
			if (server->clients+server->bots >= server->maxclients && !full)
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
		else if (codepoint >= 0xe086 && codepoint <= 0xe089)
		{
			const char *lednames[] = {"green", "red", "yellow", "blue"};
			Q_snprintfz(outhtml, outsize, "<span style=\"color:%s\">&#x25A0;</span>", lednames[codepoint-0xe086]);
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
			"<meta charset=\"UTF-8\">"
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

vfsfile_t *SVM_Generate_Gamelist(const char **mimetype, const char *query)
{
	vfsfile_t *f = VFSPIPE_Open(1, false);
	char tmpbuf[256];
	char hostname[1024];
	svm_game_t *game;
	svm_server_t *server;
	unsigned clients=0,bots=0,specs=0, totalclients=0, totalbots=0, totalspecs=0;

	VFS_PRINTF(f, "%s", master_css);
	VFS_PRINTF(f, "<h1>%s</h1>\n", sv_hostname.string);
	VFS_PRINTF(f, "<table border=1>\n");
	VFS_PRINTF(f, "<tr><th>Active Games</th><th>Players</th><th>Server Count</th></tr>\n");
	for (game = svm.firstgame; game; game = game->next)
	{
		for (clients=0,bots=0,specs=0, server = game->firstserver; server; server = server->next)
		{
			clients += server->clients;
			bots += server->bots;
			specs += server->spectators;
		}
		if (game->numservers || !sv_hideinactivegames.ival)	//only show active servers
		{
			QuakeCharsToHTML(tmpbuf, sizeof(tmpbuf), game->name, true);
			VFS_PRINTF(f, "<tr><td><a href=\"game/%s%s%s\">%s</a></td><td>%u player%s", game->name, query?"?":"", query?query:"", tmpbuf, clients, clients==1?"":"s");
			if (bots)
				VFS_PRINTF(f, ", %u bot%s", bots, bots==1?"":"s");
			if (specs)
				VFS_PRINTF(f, ", %u spectator%s", specs, specs==1?"":"s");
			VFS_PRINTF(f, "</td><td>%u server%s</td></tr>\n", (unsigned)game->numservers, game->numservers==1?"":"s");
		}
		totalclients += clients;
		totalbots += bots;
		totalspecs += specs;
	}
	VFS_PRINTF(f, "</table>\n");
	VFS_PRINTF(f, "%u game%s", (unsigned)svm.numgames, svm.numgames==1?"":"s");
	if (totalclients)
		VFS_PRINTF(f, ", %u player%s", totalclients, totalclients==1?"":"s");
	if (totalbots)
		VFS_PRINTF(f, ", %u bot%s", totalbots, totalbots==1?"":"s");
	if (totalspecs)
		VFS_PRINTF(f, ", %u spectator%s", totalspecs, totalspecs==1?"":"s");
	VFS_PRINTF(f, ", %u server%s<br/>\n", (unsigned)svm.numservers, svm.numservers==1?"":"s");

	net_from.prot = NP_DGRAM;
	VFS_PRINTF(f, "Your IP is %s<br/>\n", NET_BaseAdrToString(hostname, sizeof(hostname), &net_from));

	*mimetype = "text/html";
	return f;
}
struct rulelist_s
{
	unsigned int lines;
	unsigned int blobofs;
	char *line[64];
	char blob[8192];
};
static void SVM_GatherServerRule(void *ctx, const char *key, const char *val)
{
	struct rulelist_s *rules = ctx;
	char niceval[256];
	if (rules->lines == countof(rules->line))
		return;	//overflow
	QuakeCharsToHTML(niceval, sizeof(niceval), val, false);
	if (!Q_snprintfz(rules->blob+rules->blobofs, sizeof(rules->blob)-rules->blobofs, "<tr><td>%s</td><td>%s</td></tr>\n", key, niceval))
	{
		rules->line[rules->lines++] = rules->blob+rules->blobofs;
		rules->blobofs += strlen(rules->blob+rules->blobofs)+1;
	}
}
static int QDECL SVM_SortServerRule(const void *r1, const void *r2)
{
	return Q_strcasecmp(*(char*const*const)r1, *(char*const*const)r2);
}

vfsfile_t *SVM_Generate_Serverinfo(const char **mimetype, const char *serveraddr, const char *query)
{
	vfsfile_t *f = VFSPIPE_Open(1, false);
	char tmpbuf[256];
	char hostname[1024];
	svm_server_t *server;
	netadr_t adr[64];
	size_t count, u;
	const char *url;

	VFS_PRINTF(f, "%s", master_css);
	VFS_PRINTF(f, "<h1>Single Server Info</h1>\n");

	//FIXME: block dns lookups here?
	count = NET_StringToAdr2(serveraddr, 0, adr, countof(adr), NULL);
	while(count-->0)
	{
		server = SVM_GetServer(&adr[count]);
		if (server)
		{
			VFS_PRINTF(f, "<table border=1>\n");
			VFS_PRINTF(f, "<tr><th>Game</th><th>Address</th><th>Hostname</th><th>Mod dir</th><th>Mapname</th><th>Players</th></tr>\n");
			QuakeCharsToHTML(hostname, sizeof(hostname), server->hostname, false);

			url = NET_AdrToString(tmpbuf, sizeof(tmpbuf), &server->adr);
			if (server->game->scheme && !server->brokerid)
				url = va("<a href=\"%s://%s\">%s</a>", server->game->scheme, url, url);

			VFS_PRINTF(f, "<tr><td>%s</td><td>%s</td><td>%s%s</td><td>%s</td><td>%s</td><td>%u/%u</td></tr>\n", server->game?server->game->name:"Unknown", url, (server->needpass&1)?"&#x1F512;":"", hostname, server->gamedir, server->mapname, server->clients, server->maxclients);
			VFS_PRINTF(f, "</table>\n");
			VFS_PRINTF(f, "<br/>\n");

			if (*server->rules)
			{
				struct rulelist_s rules;
				rules.lines = rules.blobofs = 0;
				Info_Enumerate(server->rules, &rules, SVM_GatherServerRule);
				qsort(rules.line, rules.lines, sizeof(rules.line[0]), SVM_SortServerRule);

				//VFS_PRINTF(f, "<table border=0>\n");
				//	VFS_PRINTF(f, "<td></td><td>");
					VFS_PRINTF(f, "<table border=1>\n");
						VFS_PRINTF(f, "</th><th>Rule</th><th>Value</th></tr>\n");
						for (u = 0; u < rules.lines; u++)
							VFS_PUTS(f, rules.line[u]);
					VFS_PRINTF(f, "</table>");
				//	VFS_PRINTF(f, "</td>");
				//VFS_PRINTF(f, "</table>\n");
				VFS_PRINTF(f, "<br/>\n");
			}
		}
		else
		{
			VFS_PRINTF(f, "<table border=1>\n");
			VFS_PRINTF(f, "<tr><th>Game</th><th>Address</th><th>Hostname</th><th>Mod dir</th><th>Mapname</th><th>Players</th></tr>\n");
			VFS_PRINTF(f, "<tr><td>?</td><td>%s</td><td>?</td><td>?</td><td>?</td><td>?/?</td></tr>\n", NET_AdrToString(tmpbuf, sizeof(tmpbuf), &adr[count]));
			VFS_PRINTF(f, "</table>\n");
		}
	}

	*mimetype = "text/html";
	return f;
}

vfsfile_t *SVM_Generate_Serverlist(const char **mimetype, const char *masteraddr, const char *gamename, const char *query)
{
	vfsfile_t *f = VFSPIPE_Open(1, false);
	char tmpbuf[256];
	char hostname[1024];
	const char *url, *infourl;
	svm_game_t *game;
	svm_server_t *server;
	unsigned clients=0,bots=0,specs=0;

	qboolean showver = query && !!strstr(query, "ver=1");
	game = SVM_FindGame(gamename, false);

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
		VFS_PRINTF(f, "<tr><th>Address</th><th>Hostname</th><th>Gamedir</th><th>Mapname</th><th>Players</th>");
		if (showver)
			VFS_PRINTF(f, "<th>Version</th>");
		VFS_PRINTF(f, "</tr>\n");
		for (server = game->firstserver; server; server = server->next)
		{
			if (server->brokerid)
			{
				url = tmpbuf;
				Q_snprintfz(tmpbuf, sizeof(tmpbuf), "rtc://%s/%s", masteraddr, server->brokerid);
				infourl = tmpbuf;
			}
			else
			{
				infourl = url = NET_AdrToString(tmpbuf, sizeof(tmpbuf), &server->adr);
			}
			QuakeCharsToHTML(hostname, sizeof(hostname), server->hostname, false);
			VFS_PRINTF(f, "<tr><td><a href=\"/server/%s\">%s</a></td><td>%s%s%s</td><td>%s</td><td>%s</td><td>%u", infourl, url, (server->needpass&1)?"&#x1F512;":"", (server->coop&1)?"&#x1F6B8;":"", hostname, server->gamedir, server->mapname, server->clients);
			if (server->bots)
				VFS_PRINTF(f, "+%ub", server->bots);
			VFS_PRINTF(f, "/%u", server->maxclients);
			if (server->spectators)
				VFS_PRINTF(f, ", %us", server->spectators);
			VFS_PRINTF(f, "</td>");
			if (showver)
				VFS_PRINTF(f, "<td>%s</td>", server->version);
			VFS_PRINTF(f, "</tr>\n");
			clients += server->clients;
			bots += server->bots;
			specs += server->spectators;
		}
		VFS_PRINTF(f, "</table>\n");
		VFS_PRINTF(f, "%u server%s", (unsigned)game->numservers, game->numservers==1?"":"s");
		if (clients)
			VFS_PRINTF(f, ", %u client%s", (unsigned)clients, clients==1?"":"s");
		if (bots)
			VFS_PRINTF(f, ", %u bot%s", (unsigned)bots, bots==1?"":"s");
		if (specs)
			VFS_PRINTF(f, ", %u spectator%s", (unsigned)specs, specs==1?"":"s");
		VFS_PRINTF(f, "\n");
	}
	else
		VFS_PRINTF(f, "Protocol '%s' is not known\n", gamename);

	*mimetype = "text/html";
	return f;
}

vfsfile_t *SVM_Generate_Rawlist(const char **mimetype, const char *masteraddr, const char *gamename, const char *query)
{	//just spews all
	char tmpbuf[256];
	svm_game_t *game;
	svm_server_t *server;
	vfsfile_t *f = VFSPIPE_Open(1, false);

	COM_StripExtension(gamename, tmpbuf, sizeof(tmpbuf));
	game = SVM_FindGame(tmpbuf, false);

	f = VFSPIPE_Open(1, false);
	VFS_PRINTF(f, "#Server list for \"%s\"\n", tmpbuf);
	for (server = (game?game->firstserver:NULL); server; server = server->next)
	{
		if (server->brokerid)
			VFS_PRINTF(f, "rtc://%s/%s \\maxclients\\%u\\clients\\%u\\bots\\%u\\hostname\\%s\\modname\\%s\\mapname\\%s\\needpass\\%i\n", masteraddr, server->brokerid, server->maxclients, server->clients, server->bots, server->hostname, server->gamedir, server->mapname, server->needpass);
		else
			VFS_PRINTF(f, "%s\n", NET_AdrToString(tmpbuf, sizeof(tmpbuf), &server->adr));
	}

	*mimetype = "text/plain";
	return f;
}

vfsfile_t *SVM_GenerateIndex(const char *requesthost, const char *fname, const char **mimetype, const char *query)
{
	vfsfile_t *f = NULL;
	if (!master_css)
		SVM_Init();
	if (!strcmp(fname, "index.html"))
		f = SVM_Generate_Gamelist(mimetype, query);
	else if (!strncmp(fname, "server/", 7))
		f = SVM_Generate_Serverinfo(mimetype, fname+7, query);
	else if (!strncmp(fname, "game/", 5))
		f = SVM_Generate_Serverlist(mimetype, requesthost, fname+5, query);
	else if (!strncmp(fname, "raw/", 4))
		f = SVM_Generate_Rawlist(mimetype, requesthost, fname+4, query);
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
	Q_strncpyz(server->version, Info_ValueForKey(info, "version"), sizeof(server->version));
	if (!*server->version)
		Q_strncpyz(server->version, Info_ValueForKey(info, "*version"), sizeof(server->version));
	if (!*server->version)
		Q_strncpyz(server->version, Info_ValueForKey(info, "ver"), sizeof(server->version));
}

static svm_server_t *SVM_Heartbeat(const char *gamename, netadr_t *adr, int numclients, int numbots, int numspecs, double validuntil)
{
	svm_server_t *server = SVM_GetServer(adr);
	svm_game_t *game;

	if (!gamename)
	{	//no gamename is a placeholder server, to say that there's a server there but it isn't responding to our getinfos... (ie: to list misconfigured servers too)
		if (server)
		{	//it still exists, renew it, but don't otherwise care too much.
			server->expiretime = max(validuntil, server->expiretime);
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
		server->expiretime = validuntil;

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
		server->expiretime = max(server->expiretime, validuntil);
	}

	server->clients = numclients;
	server->bots = numbots;
	server->spectators = numspecs;
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

static void SVM_DiscoveredServer(netadr_t *a, const char *query)
{
	size_t idx, j;
	//add it, despite having no actual info. don't give it a valid protocol, so dead ones don't get reported with infinite feedback.
	svm_server_t *srv = SVM_Heartbeat(NULL, a, 0,0,0, svm.time + sv_slaverequery.value);

	if (strcmp(srv->game->name, "UNKNOWN") && srv->expiretime > svm.time + sv_slaverequery.value)
		return;	//we already know something about it, don't spam pings...

	if (!*srv->hostname && !strcmp(srv->game->name, "UNKNOWN"))
	{
		for (idx = 0; idx < countof(sv_masterslave); idx++)
		{
			for (j = 0; j < sv_masterslave[idx].numaddr; j++)
			{
				if (NET_CompareAdr(&net_from, &sv_masterslave[idx].addr[j]))
				{
					Q_snprintfz(srv->hostname, sizeof(srv->hostname), "[via %s]", sv_masterslave[idx].var.string);
					idx = countof(sv_masterslave);
					break;
				}
			}
		}
	}

	if (pingring_count == pingring_max)
	{	//just too many
		Z_ReallocElements((void**)&pingring, &pingring_max, pingring_max*2+1, sizeof(*pingring));
	}
	else if (pingring_first + pingring_count == pingring_max)
	{	//we're at the end.
		memmove(pingring, pingring+pingring_first, sizeof(*pingring)*pingring_count);
		pingring_first=0;
	}
	idx = pingring_first+pingring_count++;
	pingring[idx].a = *a;
	pingring[idx].query = strdup(query);
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
		svm.total.stun++;
//		Con_DPrintf("master: ignoring special packet\n");
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
			SVM_Heartbeat(QUAKE2PROTOCOLNAME, &net_from, 0,0,0, svm.time + sv_heartbeattimeout.ival);
		}
		else
		{	//dp/q3/etc are annoying, but we can query from an emphemerial socket to check NAT rules.
			sizebuf_t sb;
			netadr_t a;

			char ourchallenge[256];
			SVM_GenChallenge(ourchallenge, sizeof(ourchallenge), &net_from);
			svm.total.queries++;

			//placeholder listing...
			if (SVM_Heartbeat(NULL, &net_from, 0,0,0, svm.time + sv_heartbeattimeout.ival))
				a = net_from;
			else
				a.type = NA_INVALID;
			if (!SVM_SwitchQuerySocket())	//changes net_from to use a different master-side port so their firewall sees us as someone else
				a.type = NA_INVALID;

			//send a packet from our alternative port
			memset(&sb, 0, sizeof(sb));
			sb.maxsize = sizeof(net_message_buffer);
			sb.data = net_message_buffer;
			MSG_WriteLong(&sb, -1);
			MSG_WriteString(&sb, va("getinfo %s\n", ourchallenge));
			sb.cursize--;
			NET_SendPacket(svm_sockets, sb.cursize, sb.data, &net_from);

			if (a.type != NA_INVALID)
			{	//they were unknown... send a formal response so we can get their hostname while leaving them as 'unknown'
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
		int clients, bots, specs;
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

			bots = atoi(Info_ValueForKey(s, "bots"));
			clients = atoi(Info_ValueForKey(s, "clients"));
			clients = max(0, clients-bots);
			specs = atoi(Info_ValueForKey(s, "specs"));
			game = Info_ValueForKey(s, "gamename");
			if (!*game)
				game = QUAKE3PROTOCOLNAME;
			if (unknownresp)
				game = NULL;	//ignore the gamename and classify it as unknown. this won't break anything if we've already has a proper heartbeat from them.
			srv = SVM_Heartbeat(game, &net_from, clients,bots,specs, svm.time + sv_heartbeattimeout.ival);
			if (srv)
			{
				Q_strncpyz(srv->rules, s, sizeof(srv->rules));
				if (developer.ival)
					Info_Print(s, "\t");
				if (game)
					srv->protover = atoi(Info_ValueForKey(s, "protocol"));
				srv->maxclients = atoi(Info_ValueForKey(s, "sv_maxclients"));
				srv->needpass = atoi(Info_ValueForKey(s, "needpass"));
				srv->coop = atoi(Info_ValueForKey(s, "coop"));
				if (!srv->coop)
				{	//deathmatch 0 also means coop 1... servers that report neither are probably annoying DP servers that report nothing useful and should default to DM.
					const char *v = Info_ValueForKey(s, "deathmatch");
					srv->coop = *v && !atoi(v);
				}
				Q_strncpyz(srv->hostname, Info_ValueForKey(s, "hostname"), sizeof(srv->hostname));
				Q_strncpyz(srv->gamedir, Info_ValueForKey(s, "modname"), sizeof(srv->gamedir));
				Q_strncpyz(srv->mapname, Info_ValueForKey(s, "mapname"), sizeof(srv->mapname));
				Q_strncpyz(srv->version, Info_ValueForKey(s, "version"), sizeof(srv->version));
				if (!*srv->version)
					Q_strncpyz(srv->version, Info_ValueForKey(s, "*version"), sizeof(srv->version));
				if (!*srv->version)
					Q_strncpyz(srv->version, Info_ValueForKey(s, "ver"), sizeof(srv->version));
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
		SVM_Heartbeat(NULL, &net_from, players,0,0, svm.time + sv_heartbeattimeout.ival);
		SVM_SwitchQuerySocket();

		//send it a proper query. We'll fill in the other details on response.
		memset(&sb, 0, sizeof(sb));
		sb.maxsize = sizeof(net_message_buffer);
		sb.data = net_message_buffer;
		MSG_WriteLong(&sb, -1);
		MSG_WriteString(&sb, va("status %i\n", 15));
		sb.cursize--;
		NET_SendPacket(svm_sockets, sb.cursize, sb.data, &net_from);
	}
	else if (*com_token == M2C_MASTER_REPLY && !com_token[1])
	{	//response from a QW master request (lots of IPs from a 'slave' master that we're stealing)
		netadr_t a = {NA_IP};
		svm.total.heartbeats++;
		for (;;)
		{
			a.address.ip[0] = MSG_ReadByte();
			a.address.ip[1] = MSG_ReadByte();
			a.address.ip[2] = MSG_ReadByte();
			a.address.ip[3] = MSG_ReadByte();
			a.port = MSG_ReadShort();
			if (msg_badread)
				break;
			SVM_DiscoveredServer(&a, "\xff\xff\xff\xff""status 15\n");
		}
	}
	else if (!strncmp(com_token, "getserversExtResponse", 21) && com_token[21] == '\\')
	{	//response from a FTE-master request (lots of IPs from a 'slave' master that we're stealing)
		netadr_t a = {NA_INVALID};
		msg_readcount = 4+21;	//grr
		msg_badread = false;
		svm.total.heartbeats++;
		for (;;)
		{
			qbyte lead = MSG_ReadByte();
			if (lead == '\\')
			{
				a.type = NA_IP;
				MSG_ReadData(a.address.ip, sizeof(a.address.ip));
			}
			else if (lead == '/')
			{
				a.type = NA_IPV6;
				MSG_ReadData(a.address.ip6, sizeof(a.address.ip6));
			}
			else
				break;	//no idea
			a.port = MSG_ReadShort();
			if (msg_badread)
				break;	//read too much junk

			{
				char ourchallenge[256];
				SVM_GenChallenge(ourchallenge, sizeof(ourchallenge), &a);
				SVM_DiscoveredServer(&a, va("\xff\xff\xff\xffgetinfo %s\n", ourchallenge));
			}
		}
	}
	else if (*com_token == A2C_PRINT)
	{	//quakeworld response from 'status' requests, providing for actual info (and so that we know its reachable from other addresses)
		//there's no challenge, these could easily be spoofed. :(
		int clients = 0, bots = 0, specs = 0;
		const char *game;
		svm_server_t *srv;
		const char *t, *playerinfo = MSG_ReadString();
		s = ++line;

		t = Info_ValueForKey(s, "clients");
		if (*t)
		{
			bots = atoi(Info_ValueForKey(s, "bots"))-bots;
			clients = atoi(Info_ValueForKey(s, "clients"));
			specs = atoi(Info_ValueForKey(s, "specs"));
		}
		else
		{
			while (*playerinfo)
			{
				//USERID FRAGS TIME PING NAME SKIN TOP BOTTOM [TEAM]
				const char *s = playerinfo;
				qboolean isspec;
				qboolean isbot;
				int ping;

				s = COM_Parse(s);//userid
				s = COM_Parse(s);//frags
				isspec = !strcmp(com_token, "S");
				s = COM_Parse(s);//time
				s = COM_Parse(s);//ping
				ping = atoi(com_token);
				s = COM_Parse(s);//name
				isbot = (ping == 807 /*random hack*/) || !strncmp(com_token, "BOT:", 4);
				//s = COM_Parse(s);//skin
				//s = COM_Parse(s);//top
				//s = COM_Parse(s);//bottom
				//s = COM_Parse(s);//team

				if (isbot)
					bots++;
				else if (isspec)
					specs++;
				else
					clients++;

				while(*playerinfo)
				{
					if (*playerinfo++ == '\n')
						break;
				}
			}
		}

		game = Info_ValueForKey(s, "gamename");
		if (!*game)
		{
			game = Info_ValueForKey(s, "*version");
			if (!strncmp(game, "QTV", 3))
				game = "QTV";
			else if (!strncmp(game, "qwfwd", 5))
				game = "qwfwd";
			else
				game = QUAKEWORLDPROTOCOLNAME;
		}
		srv = SVM_Heartbeat(game, &net_from, clients,bots,specs, svm.time + sv_heartbeattimeout.ival);
		if (srv)
		{
			Q_strncpyz(srv->rules, s, sizeof(srv->rules));
			if (developer.ival)
				Info_Print(s, "\t");
			srv->protover = 3;//atoi(Info_ValueForKey(s, "protocol"));
			srv->maxclients = atoi(Info_ValueForKey(s, "maxclients"));
			srv->needpass = atoi(Info_ValueForKey(s, "needpass"));
			srv->coop = atoi(Info_ValueForKey(s, "coop"));
			if (!srv->coop)
			{	//deathmatch 0 also means coop 1... servers that report neither are probably annoying proxies servers that report nothing useful and should default to DM.
				const char *v = Info_ValueForKey(s, "deathmatch");
				srv->coop = *v && !atoi(v);
			}
			Q_strncpyz(srv->hostname, Info_ValueForKey(s, "hostname"), sizeof(srv->hostname));
			Q_strncpyz(srv->gamedir, Info_ValueForKey(s, "*gamedir"), sizeof(srv->gamedir));
			Q_strncpyz(srv->mapname, Info_ValueForKey(s, "map"), sizeof(srv->mapname));
			Q_strncpyz(srv->version, Info_ValueForKey(s, "version"), sizeof(srv->version));
			if (!*srv->version)
				Q_strncpyz(srv->version, Info_ValueForKey(s, "*version"), sizeof(srv->version));
			if (!*srv->version)
				Q_strncpyz(srv->version, Info_ValueForKey(s, "ver"), sizeof(srv->version));
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

float SVM_RequerySlaves(void)
{
	static int slaveseq = 0;
	static double nextslavetime;

	if (slaveseq == countof(sv_masterslave) || !nextslavetime)
	{
		if (nextslavetime < realtime && !pingring_count)
		{
			nextslavetime = realtime + sv_slaverequery.value;
			slaveseq = 0;
			pingring_first = 0;	//no active entries.
		}
	}

	while (slaveseq < countof(sv_masterslave))
	{
		struct sv_masterslave_s *s = &sv_masterslave[slaveseq];
		slaveseq++;

		if (*s->var.string)
		{
			size_t h;
			int defaultport[] = {PORT_QWMASTER, PORT_Q2MASTER, PORT_Q3MASTER, PORT_DPMASTER};
			const char *querystring[] = {
					/*C2M_MASTER_REQUEST*/"c\n",	//quakeworld
					NULL,							//quake2
					"\xff\xff\xff\xffgetservers 68 empty full\n",	//quake3
					"\xff\xff\xff\xffgetserversExt %s %g empty full ipv4 ipv6\n"	//fte/dp master
					};
			const char *q;

			s->numaddr = NET_StringToAdr2(s->var.string, defaultport[s->type], s->addr, countof(s->addr), NULL);
			if (s->numaddr)
			{	//send it to each...
				if (strstr(querystring[s->type], "%s"))
				{
					const char *prots = com_protocolname.string;
					while ((prots=COM_Parse(prots)))
					{
						q = va(querystring[s->type], com_token, com_protocolversion.value);
						for (h = 0; h < s->numaddr; h++)
							NET_SendPacket(svm_sockets, strlen(q), q, &s->addr[h]);
					}
				}
				else
				{
					q = querystring[s->type];
					for (h = 0; h < s->numaddr; h++)
						NET_SendPacket(svm_sockets, strlen(q), q, &s->addr[h]);
				}
			}
			else
				Con_Printf("%s: unable to resolve %s\n", s->var.name, s->var.string);
			return 1;	//something happened. might just be a name lookup lockup. :(
		}
	}

	if (pingring_count)
	{
		netadr_t *a = &pingring[pingring_first].a;
		char *q = pingring[pingring_first].query;
		pingring[pingring_first].query = NULL;
		pingring_first++;
		pingring_count--;
 		NET_SendPacket(svm_sockets, strlen(q), q, a);
 		free(q);

		return sv_slaverequery.value / pingring_max;
	}
	return 4;	//nothing happening.
}

float SVM_Think(int port)
{
	NET_ReadPackets (svm_sockets);
	SVM_RemoveOldServers();
	return SVM_RequerySlaves();
}
#else
float SVM_Think(int port){return 4;}
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
	if (s1->stun!=s2->stun)
		Con_Printf("Stun/min: %f\n", (s1->stun-s2->stun)/period);
	if (s1->junk!=s2->junk)
		Con_Printf("Junk/min: %f\n", (s1->junk-s2->junk)/period);

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
		if (!game)
			return false;
		if (man->schemes && !game->scheme)
		{
			COM_Parse(man->schemes);
			game->scheme = Z_StrDup(com_token);
		}
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
	size_t u;

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
	Cvar_Register(&sv_slaverequery, "server control variables");
	for (u = 0; u < countof(sv_masterslave); u++)
		Cvar_Register(&sv_masterslave[u].var, "server control variables");

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
	float sleeptime;
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

	sleeptime = SVM_Think(sv_masterport.ival);

	//record lots of info over multiple frames, for smoother stats info.
	svm.total.timestamp = realtime;
	if (svm.nextstamp < realtime)
	{
		svm.stamps[svm.stampring%countof(svm.stamps)] = svm.total;
		svm.stampring++;
		svm.nextstamp = realtime+60;
	}

	return sleeptime;
}
#endif
