
#define SS_FTESERVER	1	//hehehe...
#define SS_QUAKE2		2	//useful (and cool). Could be blamed for swamping.
#define SS_NETQUAKE		4
#define SS_FAVORITE		8	//filter all others.
#define SS_KEEPINFO		16


//despite not supporting nq or q2, we still load them. We just filter them. This is to make sure we properly write the listing files.
#define MT_BAD			0	//this would be an error
#define MT_BCASTQW		1	//-1status
#define MT_BCASTQ2		2	//-1status
#define MT_BCASTNQ		3	//see code
#define MT_SINGLEQW		4	//-1status
#define MT_SINGLEQ2		5	//-1status
#define MT_SINGLENQ		6	//see code.
#define MT_MASTERQW		7	//c\n\0
#define MT_MASTERQ2		8	//query


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

	short players;
	short maxplayers;

	short tl;
	short fl;
	char gamedir[8+1];
	char map[8+1];

	float refreshtime;
	qbyte special;	//flags
	unsigned short ping;
	int sends;

	serverdetailedinfo_t *moreinfo;

	struct serverinfo_s *next;
} serverinfo_t;

typedef struct master_s{
	struct master_s *next;
	netadr_t adr;
	int type;
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

void CL_QueryServers(void);
int NET_CheckPollSockets(void);
void MasterInfo_Request(master_t *mast, qboolean evenifwedonthavethefiles);
serverinfo_t *Master_InfoForServer (netadr_t addr);
serverinfo_t *Master_InfoForNum (int num);
int Master_TotalCount(void);
void Master_QueryServer(serverinfo_t *server);
void MasterInfo_WriteServers(void);
