enum masterprotocol_e
{
	MP_UNSPECIFIED,
	MP_QUAKEWORLD,
#if defined(Q2CLIENT) || defined(Q2SERVER)
	MP_QUAKE2,
#endif
#if defined(Q3CLIENT) || defined(Q3SERVER)
	MP_QUAKE3,
#endif
#ifdef NQPROT
	MP_NETQUAKE,
#endif
	MP_DPMASTER
};

#if defined(CL_MASTER) && !defined(SERVERONLY)
#define SS_PROTOCOLMASK 0xf
#define SS_UNKNOWN		0
#define SS_QUAKEWORLD	1
#define SS_NETQUAKE		2
#define SS_DARKPLACES	3
#define SS_QUAKE2		4
#define SS_QUAKE3		5
//#define SS_UNUSED		6
//#define SS_UNUSED		7

#define SS_LOCAL		(1<<3u)	//local servers are ones we detected without being listed on a master server (masters will report public ips, so these may appear as dupes if they're also public)
#define SS_FTESERVER	(1<<4u)	//hehehe...
#define SS_FAVORITE		(1<<5u)	//filter all others.
#define SS_KEEPINFO		(1<<6u)
#define SS_PROXY		(1<<7u)


//despite not supporting nq or q2, we still load them. We just filter them. This is to make sure we properly write the listing files.
enum mastertype_e
{
	MT_BAD,			//this would be an error
	MT_MASTERHTTPJSON,
	MT_MASTERHTTP,
	MT_MASTERUDP,
	MT_BCAST,
	MT_SINGLE,
};


typedef enum
{
	SLKEY_PING,
	SLKEY_MAP,
	SLKEY_NAME,
	SLKEY_ADDRESS,
	SLKEY_NUMPLAYERS,
	SLKEY_MAXPLAYERS,
	SLKEY_GAMEDIR,

	SLKEY_FREEPLAYERS,
	SLKEY_BASEGAME,
	SLKEY_FLAGS,
	SLKEY_TIMELIMIT,
	SLKEY_FRAGLIMIT,

	SLKEY_MOD,
	SLKEY_PROTOCOL,
	SLKEY_NUMBOTS,		//uninteresting bots that will presumably get kicked if people join.
	SLKEY_NUMSPECTATORS,//spectators
	SLKEY_NUMHUMANS,	//actual players
	SLKEY_QCSTATUS,
//	SLKEY_PLAYERS,	//eep!
	SLKEY_ISFAVORITE,//eep!
	SLKEY_ISLOCAL,
	SLKEY_ISPROXY,
	SLKEY_SERVERINFO,


	SLKEY_TOOMANY,
	SLKEY_PLAYER0,
	SLKEY_CUSTOM = SLKEY_PLAYER0+MAX_CLIENTS
} hostcachekey_t;

typedef enum
{
	SLIST_TEST_CONTAINS,
	SLIST_TEST_NOTCONTAIN,
	SLIST_TEST_LESSEQUAL,
	SLIST_TEST_LESS,
	SLIST_TEST_EQUAL,
	SLIST_TEST_GREATER,
	SLIST_TEST_GREATEREQUAL,
	SLIST_TEST_NOTEQUAL,
	SLIST_TEST_STARTSWITH,
	SLIST_TEST_NOTSTARTSWITH
} slist_test_t;


//contains info about a server in greater detail. Could be too mem intensive.
typedef struct serverdetailedinfo_s
{
	char info[MAX_SERVERINFO_STRING];

	int numplayers;

	struct serverdetailedplayerinfo_s
	{
		int userid;
		int frags;
		float time;
		int ping;
		char name[64];
		char skin[16];	//is this even useful?
		char team[16];
		char topc;
		char botc;
		qbyte isspec;
	} players[MAX_CLIENTS];
} serverdetailedinfo_t;

//hold minimum info.
typedef struct serverinfo_s
{
	char name[64];	//hostname.
	netadr_t adr;

	short special;	//flags
	short protocol;

	qbyte players;
	qbyte maxplayers;
	qbyte sends;
	qbyte status; //1=alive, 2=displayed

	qbyte numspectators;
	qbyte numhumans;
	qbyte numbots;
	qbyte freeslots;

	char modname[8+1];
	char qcstatus[8+1];

	char gamedir[8+1];
	char map[16];

	unsigned short gameversion;
	unsigned short ping;

	short tl;
	short fl;

	float refreshtime;

	serverdetailedinfo_t *moreinfo;

	struct serverinfo_s *prevpeer;
	unsigned short cost;
	unsigned short numpeers;
	struct peers_s
	{
		struct serverinfo_s *peer;
		unsigned short ping;
	} *peers;

	struct serverinfo_s *next;
} serverinfo_t;

typedef struct master_s
{
	struct master_s *next;
	netadr_t adr;
	char *address;	//text based address (http servers)
	struct dl_download *dl;
	qbyte nosave;
	qbyte mastertype;
	qbyte protocoltype;
	int sends; /*needs to resend?*/
	char name[1];
} master_t;

extern struct selectedserver_s
{
	qboolean inuse;
	netadr_t adr;
	float	refreshtime;
	int		lastplayer;
	char	lastrule[64];

	serverdetailedinfo_t *detail;

	int linenum;
} selectedserver;

typedef struct player_s
{
	char name[16];
	int frags;
	int colour;
	char skin[8];
	char team[8];
	netadr_t adr;

	struct player_s *next;
} player_t;

void SListOptionChanged(serverinfo_t *newserver);

extern serverinfo_t *firstserver;
extern master_t *master;
extern player_t *mplayers;
extern qboolean sb_favouriteschanged;

void Master_SetupSockets(void);
qboolean CL_QueryServers(void);
int Master_CheckPollSockets(void);
void MasterInfo_Shutdown(void);
void MasterInfo_WriteServers(void);
void MasterInfo_Request(master_t *mast);
serverinfo_t *Master_InfoForServer (netadr_t *addr);
serverinfo_t *Master_InfoForNum (int num);
unsigned int Master_TotalCount(void);
unsigned int Master_NumPolled(void);	//progress indicator
unsigned int Master_NumAlive(void);
void Master_SetupSockets(void);
void MasterInfo_Refresh(void);
void Master_QueryServer(serverinfo_t *server);
void MasterInfo_WriteServers(void);

int Master_KeyForName(const char *keyname);
float Master_ReadKeyFloat(serverinfo_t *server, int keynum);
char *Master_ReadKeyString(serverinfo_t *server, int keynum);

void Master_SortServers(void);
void Master_SetSortField(hostcachekey_t field, qboolean descending);
hostcachekey_t Master_GetSortField(void);
qboolean Master_GetSortDescending(void);

int Master_NumSorted(void);
void Master_ClearMasks(void);
serverinfo_t *Master_SortedServer(int idx);
void Master_SetMaskString(qboolean or, hostcachekey_t field, const char *param, slist_test_t testop);
void Master_SetMaskInteger(qboolean or, hostcachekey_t field, int param, slist_test_t testop);
serverinfo_t *Master_FindRoute(netadr_t target);
#else
#define MasterInfo_WriteServers()
#endif
