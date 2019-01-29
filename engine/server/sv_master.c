#include "quakedef.h"

#ifdef SV_MASTER

#ifdef _WIN32
#include "winquake.h"
#ifdef _MSC_VER
#include "wsipx.h"
#endif
#endif

#include "netinc.h"

//quakeworld protocol
//    heartbeat: "a"
//        query: "c\n%i<sequence>\n%i<numplayers>\n"
//queryresponse: "d\naaaappaaaapp"

//quake2 protocol
//    heartbeat: "heartbeat\n%s<serverinfo>\n%i<frags> %i<ping> \"%s\"<name>\n<repeat>"
//        query: "query\0"
//queryresponse: "servers\naaaappaaaapp"

//quake3/dpmaster protocol
//    heartbeat: "heartbeat DarkPlaces\n"
//        query: "getservers[Ext<ipv6>] [%s<game>] %u<version> [empty] [full] [ipv6]"
//queryresponse: "getservers[Ext]Response\\aaaapp/aaaaaaaaaaaapp\\EOF"

enum gametypes_e
{
	GT_FFA=0,
	GT_TOURNEY=1,
	GT_TEAM=3,
	GT_CTF=4,
};
typedef struct svm_server_s {
	netadr_t adr;
	int protover;
	unsigned int clients;
	unsigned int maxclients;
	char hostname[48];	//just for our own listings.
	char mapname[16];	//just for our own listings.
	char gamedir[16];	//again...
	unsigned short gametype;
	float expiretime;

	bucket_t bucket;
	struct svm_game_s *game;
	struct svm_server_s *next;
} svm_server_t;

typedef struct svm_game_s {
	struct svm_game_s *next;

	svm_server_t *firstserver;
	size_t numservers;
	char name[1];
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
static cvar_t sv_heartbeattimeout = CVARD("sv_heartbeattimeout", "600", "How many seconds a server should remain listed after its latest heartbeat. Larger values can avoid issues from packetloss, but can also make dos attacks easier.");
static cvar_t sv_masterport = CVARC("sv_masterport", "27000 28000", SVM_Port_Callback);
static cvar_t sv_masterport_tcp = CVARC("sv_masterport_tcp", "27000 28000", SVM_Tcpport_Callback);
static cvar_t sv_maxgames = CVARD("sv_maxgames", "100", "Limits the number of games that may be known. This is to reduce denial of service attacks.");
static cvar_t sv_maxservers = CVARD("sv_maxservers", "1000", "Limits the number of servers (total from all games) that may be known. This is to reduce denial of service attacks.");

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
	while (server && !NET_CompareAdr(&server->adr, adr))
	{
		server = Hash_GetNextKey(&svm.serverhash, key, server);
	}
	return server;
}

static svm_game_t *SVM_FindGame(const char *game, qboolean create)
{
	svm_game_t *g;
	for (g = svm.firstgame; g; g = g->next)
	{
		if (!Q_strcasecmp(game, g->name))
			return g;
	}

	if (create)
	{
		if (svm.numgames >= sv_maxgames.ival)
		{
			Con_DPrintf("game limit exceeded\n");
			return NULL;
		}
		//block some chars that may cause issues/exploits. sorry.
		if (strchr(game, '.') || strchr(game, '\"') || strchr(game, '/') || strchr(game, '?') || strchr(game, '&') || strchr(game, '+') || strchr(game, '\'') || strchr(game, '<') || strchr(game, '>'))
			return NULL;
		if (!*game || (*game >= '0' && *game <= '9'))
			return NULL;	//must not start with a number either.
		g = ZF_Malloc(sizeof(*g) + strlen(game));
		if (g)
		{
			strcpy(g->name, game);
			g->next = svm.firstgame;
			svm.firstgame = g;
			svm.numgames++;
			Con_DPrintf("Creating game \"%s\"\n", g->name);
		}
	}
	return g;
}

static void SVM_RemoveOldServers(void)
{
	svm_game_t **gamelink, *g;
	svm_server_t **serverlink, *s;
	for (gamelink = &svm.firstgame; (g=*gamelink); )
	{
		for (serverlink = &g->firstserver; (s=*serverlink); )
		{
			if (s->expiretime < svm.time)
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

		if (!g->firstserver)
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

int SVM_AddIPAddresses(sizebuf_t *sb, int first, const char *gamename, int v4, int v6, qboolean prefixes, int gametype)
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
//FIXME
//			if (gametype != -1 && server->gametype != gametype)
//				continue;
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

vfsfile_t *SVM_GenerateIndex(const char *fname)
{
	static const char *thecss =
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
				"</style>";
	char tmpbuf[256];
	svm_game_t *game;
	svm_server_t *server;
	vfsfile_t *f = NULL;
	unsigned clients = 0, maxclients=0;
	if (!strcmp(fname, "index.html"))
	{
		f = VFSPIPE_Open(1, false);
		VFS_PRINTF(f, "%s", thecss);
		VFS_PRINTF(f, "<h1>FTE-Master</h1>\n");
		VFS_PRINTF(f, "<table border=1>\n");
		for (game = svm.firstgame; game; game = game->next)
		{
			VFS_PRINTF(f, "<tr><td><a href=\"game/%s.html\">%s</a></td><td%u server(s)</td></tr>\n", game->name, game->name, (unsigned)game->numservers);
			clients += game->numservers;
		}
		VFS_PRINTF(f, "</table>\n");
		VFS_PRINTF(f, "%u game(s), %u server(s)\n", (unsigned)svm.numgames, clients);
	}
	else if (!strncmp(fname, "server/", 7))
	{
		netadr_t adr[64];
		int count;

		f = VFSPIPE_Open(1, false);
		VFS_PRINTF(f, "%s", thecss);
		VFS_PRINTF(f, "<h1>Single Server Info</h1>\n");

		VFS_PRINTF(f, "<table border=1>\n");
		VFS_PRINTF(f, "<tr><th>Game</th><th>Address</th><th>Hostname</th><th>Mod dir</th><th>Mapname</th><th>Players</th></tr>\n");
		count = NET_StringToAdr2(fname+7, 0, adr, countof(adr));
		while(count-->0)
		{
			server = SVM_GetServer(&adr[count]);
			if (server)
				VFS_PRINTF(f, "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%u/%u</td></tr>\n", server->game?server->game->name:"Unknown", NET_AdrToString(tmpbuf, sizeof(tmpbuf), &server->adr), server->hostname, server->gamedir, server->mapname, server->clients, server->maxclients);
			else
				VFS_PRINTF(f, "<tr><td>?</td><td>%s</td><td>?</td><td>?</td><td>?</td><td>?/?</td></tr>\n", NET_AdrToString(tmpbuf, sizeof(tmpbuf), &adr[count]));
		}
		VFS_PRINTF(f, "</table>\n");
	}
	else if (!strncmp(fname, "game/", 5))
	{
		COM_StripExtension(fname+5, tmpbuf, sizeof(tmpbuf));
		game = SVM_FindGame(tmpbuf, false);

		f = VFSPIPE_Open(1, false);
		VFS_PRINTF(f, "%s", thecss);
		VFS_PRINTF(f, "<h1>Servers for %s</h1>\n", tmpbuf);

		if(game)
		{
			VFS_PRINTF(f, "<table border=1>\n");
			VFS_PRINTF(f, "<tr><th>Address</th><th>Hostname</th><th>Gamedir</th><th>Mapname</th><th>Players</th></tr>\n");
			for (server = game->firstserver; server; server = server->next)
			{
				VFS_PRINTF(f, "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%u/%u</td></tr>\n", NET_AdrToString(tmpbuf, sizeof(tmpbuf), &server->adr), server->hostname, server->gamedir, server->mapname, server->clients, server->maxclients);
				clients += server->clients;
				maxclients += server->maxclients;
			}
			VFS_PRINTF(f, "</table>\n");
			VFS_PRINTF(f, "%u server(s), %u/%u client(s)\n", (unsigned)game->numservers, clients, maxclients);
		}
		else
			VFS_PRINTF(f, "No servers known for %s\n", tmpbuf);
	}
	else if (!strncmp(fname, "raw/", 4))
	{	//just spews all
		COM_StripExtension(fname+4, tmpbuf, sizeof(tmpbuf));
		game = SVM_FindGame(tmpbuf, false);

		f = VFSPIPE_Open(1, false);
		for (server = (game?game->firstserver:NULL); server; server = server->next)
			VFS_PRINTF(f, "%s\n", NET_AdrToString(tmpbuf, sizeof(tmpbuf), &server->adr));
	}
	return f;
}

static svm_server_t *SVM_Heartbeat(const char *gamename, netadr_t *adr, int numclients, float validuntil)
{
	svm_server_t *server = SVM_GetServer(adr);
	svm_game_t *game = SVM_FindGame(gamename, true);
	if (!game)
		return NULL;

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

void SVM_Think(int port)
{
	char *s;
	int cookie = 0;
	int giveup = 500;

	while (giveup-- > 0 && (cookie=NET_GetPacket (svm_sockets, cookie)) >= 0)
	{
		net_message.data[net_message.cursize] = '\0';	//null term all strings.

		svm.time = Sys_DoubleTime();

		MSG_BeginReading(msg_nullnetprim);
		if (MSG_ReadLong() != -1 || msg_badread)
		{	//go back to start...
			MSG_BeginReading(msg_nullnetprim);
		}
		s = MSG_ReadStringLine();
		s = COM_Parse(s);
		if (!strcmp(com_token, "getservers") || !strcmp(com_token, "getserversExt"))
		{	//q3
			sizebuf_t sb;
			int ver;
			char *eos;
			char game[64];
			qboolean ext = !strcmp(com_token, "getserversExt");
			qboolean empty = false;
			qboolean full = false;
			qboolean ipv4 = !ext;
			qboolean ipv6 = false;
			int gametype = -1;
			s = COM_ParseOut(s, game, sizeof(game));
			ver = strtol(game, &eos, 0);
			if (*eos)
			{
				s = COM_Parse(s);
				ver = strtol(com_token, NULL, 0);
			}
			else
				Q_strncpyz(game, "Quake3", sizeof(game));
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
			svm.total.queries++;
			memset(&sb, 0, sizeof(sb));
			sb.maxsize = sizeof(net_message_buffer)-2;
			sb.data = net_message_buffer;
			MSG_WriteLong(&sb, -1);

			if (!ipv4 && !ipv6)
				ipv4 = ipv6 = true; //neither specified? use both
			(void)ver, (void)full, (void)empty;
			if (ext)
			{	//ipv6 and ipv4 addresses
				MSG_WriteString(&sb, "getserversExtResponse");
				SVM_AddIPAddresses(&sb, 0, game, ipv4, ipv6, true, gametype);
			}
			else
			{	//ipv4 only
				MSG_WriteString(&sb, "getserversResponse");
				SVM_AddIPAddresses(&sb, 0, game, ipv4, ipv6, true, gametype);
			}
			sb.maxsize+=2;
			MSG_WriteByte(&sb, '\\');	//otherwise the last may be considered invalid and ignored.
//			MSG_WriteByte(&sb, 'E');
//			MSG_WriteByte(&sb, 'O');
//			MSG_WriteByte(&sb, 'T');
			NET_SendPacket(svm_sockets, sb.cursize, sb.data, &net_from);
		}
		else if (!strcmp(com_token, "heartbeat"))
		{	//quake2 heartbeat. Serverinfo and players should follow.
			if (*s == '\n' && s[1] == '\\')
			{	//there's some serverinfo there, must be q2...
				svm.total.heartbeats++;
				SVM_Heartbeat("Quake2", &net_from, 0, svm.time + sv_heartbeattimeout.ival);
			}
			else
			{	//dp/q3/etc are annoying, but we can query from an emphemerial socket to check NAT rules.
				sizebuf_t sb;
				svm.total.queries++;
				memset(&sb, 0, sizeof(sb));
				sb.maxsize = sizeof(net_message_buffer);
				sb.data = net_message_buffer;
				MSG_WriteLong(&sb, -1);
				MSG_WriteString(&sb, "getinfo CAKE\n");
				sb.cursize--;
				NET_SendPacket(svm_sockets, sb.cursize, sb.data, &net_from);
			}
		}
		else if (!strcmp(com_token, "infoResponse"))
		{
			int clients;
			const char *game, *chal;
			svm_server_t *srv;
			s = MSG_ReadStringLine();
			svm.total.heartbeats++;
			chal = Info_ValueForKey(s, "challenge");
			if (!strcmp(chal, "CAKE"))
			{
				clients = atoi(Info_ValueForKey(s, "clients"));
				game = Info_ValueForKey(s, "gamename");
				srv = SVM_Heartbeat(game, &net_from, clients, svm.time + sv_heartbeattimeout.ival);
				if (srv)
				{
					if (developer.ival)
						Info_Print(s, "\t");
					srv->protover = atoi(Info_ValueForKey(s, "protocol"));
					srv->maxclients = atoi(Info_ValueForKey(s, "sv_maxclients"));
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
			SVM_AddIPAddresses(&sb, 0, "Quake2", true, false, false, -1);
			NET_SendPacket(svm_sockets, sb.cursize, sb.data, &net_from);
		}
		else if (*com_token == S2M_HEARTBEAT)	//sequence, players
		{	//quakeworld heartbeat
			int players;
			s = MSG_ReadStringLine();
			//sequence = atoi(s);
			s = MSG_ReadStringLine();
			players = atoi(s);
			svm.total.heartbeats++;
			SVM_Heartbeat("QuakeWorld", &net_from, players, svm.time + sv_heartbeattimeout.ival);
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
			SVM_AddIPAddresses(&sb, 0, "QuakeWorld", true, false, false, -1);
			NET_SendPacket(svm_sockets, sb.cursize, sb.data, &net_from);
		}
		else if (*com_token == A2A_PING)
		{	//quakeworld server listing request
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
		else
			svm.total.junk++;
	}

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

	svm_sockets = FTENET_CreateCollection(true);
	Hash_InitTable(&svm.serverhash, 1024, Z_Malloc(Hash_BytesForBuckets(1024)));

	Cvar_Register(&sv_masterport, "server control variables");
	Cvar_Register(&sv_masterport_tcp, "server control variables");
	Cvar_Register(&sv_heartbeattimeout, "server control variables");
	Cvar_Register(&sv_maxgames, "server control variables");
	Cvar_Register(&sv_maxservers, "server control variables");

	Cvar_ParseWatches();
	host_initialized = true;

	manarg = COM_CheckParm("-manifest");
	if (manarg && manarg < com_argc-1 && com_argv[manarg+1])
	{
		char *man = FS_MallocFile(com_argv[manarg+1], FS_SYSTEM, NULL);

		FS_ChangeGame(FS_Manifest_Parse(NULL, man), true, true);
		BZ_Free(man);
	}
	else
		FS_ChangeGame(NULL, true, true);

	Cmd_StuffCmds();
	Cbuf_Execute ();

	Cvar_ForceCallback(&sv_masterport);
	Cvar_ForceCallback(&sv_masterport_tcp);


	Con_TPrintf ("Exe: %s %s\n", __DATE__, __TIME__);

	Con_Printf ("%s\n", version_string());

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