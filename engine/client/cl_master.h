
#define SS_FTESERVER	1	//hehehe...
#define SS_QUAKE2		2	//useful (and cool). Could be blamed for swamping.
#define SS_NETQUAKE		4
#define SS_FAVORITE		8	//filter all others.
#define SS_KEEPINFO		16
#define SS_DARKPLACES	32
#define SS_QUAKE3		64
#define SS_PROXY		128


//despite not supporting nq or q2, we still load them. We just filter them. This is to make sure we properly write the listing files.
enum {
	MT_BAD,			//this would be an error
	MT_MASTERHTTP,  //an http/ftp based master server with NQ servers
	MT_MASTERHTTPQW,//an http/ftp based master server with QW servers
	MT_BCASTQW,		//-1status
	MT_BCASTQ2,		//-1status
	MT_BCASTQ3,
	MT_BCASTNQ,		//see code
	MT_BCASTDP,
	MT_SINGLEQW,	//-1status
	MT_SINGLEQ2,	//-1status
	MT_SINGLEQ3,
	MT_SINGLENQ,	//see code.
	MT_SINGLEDP,
	MT_MASTERQW,	//c\n\0
	MT_MASTERQ2,	//query
	MT_MASTERQ3,
	MT_MASTERDP		//-1getservers %s 3 empty full\x0A
};


typedef enum{
	SLKEY_PING,
	SLKEY_MAP,
	SLKEY_NAME,
	SLKEY_ADDRESS,
	SLKEY_NUMPLAYERS,
	SLKEY_MAXPLAYERS,
	SLKEY_GAMEDIR,

	SLKEY_FREEPLAYERS,
	SLKEY_BASEGAME,
	SLKEY_TIMELIMIT,
	SLKEY_FRAGLIMIT,

	SLKEY_MOD,
	SLKEY_PROTOCOL,
	SLKEY_NUMBOTS,
	SLKEY_NUMHUMANS,
	SLKEY_QCSTATUS,
//	SLKEY_PLAYERS,	//eep!
	SLKEY_ISFAVORITE,//eep!


	SLKEY_TOOMANY,
	SLKEY_CUSTOM
} hostcachekey_t;

typedef enum {
	SLIST_TEST_CONTAINS,
	SLIST_TEST_NOTCONTAIN,
	SLIST_TEST_LESSEQUAL,
	SLIST_TEST_LESS,
	SLIST_TEST_EQUAL,
	SLIST_TEST_GREATER,
	SLIST_TEST_GREATEREQUAL,
	SLIST_TEST_NOTEQUAL
} slist_test_t;


//contains info about a server in greater detail. Could be too mem intensive.
typedef struct serverdetailedinfo_s {
	char info[MAX_SERVERINFO_STRING];

	int numplayers;

	struct {
		int userid;
		int frags;
		float time;
		int ping;
		char name[64];
		char skin[64];
		char topc;
		char botc;				
	} players[MAX_CLIENTS];
} serverdetailedinfo_t;

//hold minimum info.
typedef struct serverinfo_s {
	char name[64];	//hostname.
	netadr_t adr;

	unsigned char players;
	unsigned char maxplayers;
	qbyte special;	//flags
	qbyte sends;
	qbyte insortedlist;

	qbyte numhumans;
	qbyte numbots;
	qbyte freeslots;
	qbyte protocol;

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

	struct serverinfo_s *next;
} serverinfo_t;

typedef struct master_s{
	struct master_s *next;
	netadr_t adr;
	char *address;	//text based address (http servers
	int type;
	int servertype;	//filled in for http servers
	char name[1];
} master_t;

struct {
	qboolean inuse;
	netadr_t adr;

	serverdetailedinfo_t *detail;

	int linenum;
} selectedserver;

typedef struct player_s {
	char name[16];
	int frags;
	int colour;
	char skin[8];
	netadr_t adr;

	struct player_s *next;
} player_t;

void SListOptionChanged(serverinfo_t *newserver);

extern serverinfo_t *firstserver;
extern master_t *master;
extern player_t *mplayers;

void Master_SetupSockets(void);
void CL_QueryServers(void);
int NET_CheckPollSockets(void);
void MasterInfo_Request(master_t *mast, qboolean evenifwedonthavethefiles);
serverinfo_t *Master_InfoForServer (netadr_t addr);
serverinfo_t *Master_InfoForNum (int num);
int Master_TotalCount(void);
void Master_SetupSockets(void);
void Master_QueryServer(serverinfo_t *server);
void MasterInfo_WriteServers(void);

int Master_KeyForName(char *keyname);
float Master_ReadKeyFloat(serverinfo_t *server, int keynum);
char *Master_ReadKeyString(serverinfo_t *server, int keynum);

void Master_SortServers(void);
void Master_SetSortField(hostcachekey_t field, qboolean descending);
hostcachekey_t Master_GetSortField(void);
qboolean Master_GetSortDescending(void);

int Master_NumSorted(void);
void Master_ClearMasks(void);
serverinfo_t *Master_SortedServer(int idx);
void Master_SetMaskString(qboolean or, hostcachekey_t field, char *param, slist_test_t testop);
void Master_SetMaskInteger(qboolean or, hostcachekey_t field, int param, slist_test_t testop);
