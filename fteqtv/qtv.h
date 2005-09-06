//each server that we are connected to has it's own state.
//it should be easy enough to use one thread per server.

//mvd info is forwarded to other proxies instantly
//qwd stuff is buffered and delayed. :(

//this means that when a new proxy connects, we have to send initial state as well as a chunk of pending state, expect to need to send new data before the proxy even has all the init stuff. We may need to raise MAX_PROXY_BUFFER to be larger than on the server

#ifdef _WIN32
	#include <winsock.h>
	#pragma comment (lib, "wsock32.lib")
	#define qerrno WSAGetLastError()
	#define EWOULDBLOCK WSAEWOULDBLOCK

	#ifndef _DEBUG
	#define static	//it breaks my symbol lookups. :(
	#endif

#elif defined(__CYGWIN__)

	#include <sys/types.h>
	#include <sys/socket.h>
	#include <sys/errno.h>
	#include <arpa/inet.h>
	#include <stdarg.h>
	#include <netdb.h>
	#include <stdlib.h>

	#ifndef SOCKET
		#define SOCKET int
	#endif
	#ifndef INVALID_SOCKET
		#define INVALID_SOCKET -1
	#endif
	#define qerrno errno

	#define ioctlsocket ioctl
	#define closesocket close

#elif defined(linux)
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netdb.h>
	#include <stdarg.h>
	#include <stdlib.h>
	#include <string.h>
	#include <errno.h>
	#include <sys/ioctl.h>
	#include <unistd.h>

	#ifndef SOCKET
		#define SOCKET int
	#endif
	#ifndef INVALID_SOCKET
		#define INVALID_SOCKET -1
	#endif
	#define qerrno errno

	#define ioctlsocket ioctl
	#define closesocket close
#else
#error "Please insert required headers here"
//try the cygwin ones
#endif

#include <stdio.h>

#define VERSION "0.01"	//this will be added to the serverinfo

#define PROX_DEFAULTLISTENPORT 27501
#define PROX_DEFAULTSERVER "localhost:27500"

#define	MAX_SERVERINFO_STRING	1024	//standard quake has 512 here.
#define MAX_USERINFO 192
#define MAX_CLIENTS 32
#define MAX_STATS 32
#define MAX_LIST 256
#define MAX_MODELS MAX_LIST
#define MAX_SOUNDS MAX_LIST
#define MAX_ENTITIES 512
#define MAX_STATICSOUNDS 64
#define MAX_LIGHTSTYLES 64

#define MAX_PROXY_BUFFER (1<<14)	//must be power-of-two
#define PREFERED_PROXY_BUFFER	1500 //the ammount of data we try to leave in our input buffer (must be large enough to contain any single mvd frame)


#define ENTS_PER_FRAME 512 //max number of entities per frame.
#define ENTITY_FRAMES 64 //number of frames to remember for deltaing

typedef enum {false, true} qboolean;

typedef unsigned char netadr_t[64];

#define MAX_MSGLEN 1400
#define MAX_BACKBUF_SIZE 1000	//this is smaller so we don't loose entities when lagging

typedef struct {
	unsigned int readpos;
	unsigned int cursize;
	unsigned int maxsize;
	char *data;
	unsigned int startpos;
	qboolean overflowed;
	qboolean allowoverflow;
} netmsg_t;

typedef struct {
	SOCKET sock;
	netadr_t remote_address;
	unsigned short qport;
	unsigned int last_received;
	unsigned int cleartime;

	int reliable_length;
	qboolean drop;

	netmsg_t message;
	char message_buf[MAX_MSGLEN];	//reliable message being built
	char reliable_buf[MAX_MSGLEN];	//reliable message that we're making sure arrives.
	float rate;


	unsigned int incoming_acknowledged;
	unsigned int last_reliable_sequence;
	unsigned int incoming_reliable_acknowledged;
	unsigned int incoming_reliable_sequence;
	unsigned int reliable_sequence;

	unsigned int incoming_sequence;
	unsigned int outgoing_sequence;
} netchan_t;

typedef struct {
#define MAX_QPATH 64
	char name[MAX_QPATH];
} filename_t;

typedef struct {
	unsigned char frame;
	unsigned char modelindex;
	unsigned char colormap;
	unsigned char skinnum;
	short origin[3];
	unsigned char angles[3];
	unsigned char effects;
} entity_state_t;
typedef struct {
	unsigned char frame;
	unsigned char modelindex;
	unsigned char skinnum;
	short origin[3];
	unsigned short angles[3];
	unsigned char effects;
	unsigned char weaponframe;
} player_state_t;
typedef struct {
	unsigned int stats[MAX_STATS];
	char userinfo[MAX_USERINFO];

	int ping;
	int frags;
	float entertime;

	qboolean active:1;
	qboolean gibbed:1;
	qboolean dead:1;
	player_state_t current;
	player_state_t old;
} playerinfo_t;

typedef struct {
	entity_state_t ents[ENTS_PER_FRAME];	//ouchie ouchie!
	unsigned short entnum[ENTS_PER_FRAME];
	int numents;
} packet_entities_t;

#define MAX_BACK_BUFFERS	16
typedef struct viewer_s {
	qboolean drop;
	netchan_t netchan;
	qboolean maysend;
	qboolean chokeme;
	qboolean thinksitsconnected;
	int delta_frame;

	netmsg_t backbuf[MAX_BACK_BUFFERS];	//note data is malloced!
	int backbuffered;

	unsigned int currentstats[MAX_STATS];
	unsigned int trackplayer;

	packet_entities_t frame[ENTITY_FRAMES];

	struct viewer_s *next;

	float origin[3];
} viewer_t;

typedef struct oproxy_s {
	qboolean flushing;
	qboolean drop;
	SOCKET sock;
	unsigned char buffer[MAX_PROXY_BUFFER];
	unsigned int buffersize;	//use cyclic buffering.
	unsigned int bufferpos;
	struct oproxy_s *next;
} oproxy_t;

typedef struct {
	short origin[3];
	unsigned char soundindex;
	unsigned char volume;
	unsigned char attenuation;
} staticsound_t;

typedef struct sv_s {
	netadr_t serveraddress;

	unsigned char buffer[MAX_PROXY_BUFFER];	//this doesn't cycle.
	int buffersize;	//it memmoves down

	unsigned int parsetime;

	int servercount;
	char gamedir[MAX_QPATH];
	char mapname[256];
	struct {
		float gravity;
		float maxspeed;
		float spectatormaxspeed;
		float accelerate;
		float airaccelerate;
		float waterfriction;
		float entgrav;
		float stopspeed;
		float wateraccelerate;
		float friction;
	} movevars;
	int cdtrack;
	entity_state_t baseline[MAX_ENTITIES];
	entity_state_t curents[MAX_ENTITIES];
	entity_state_t oldents[MAX_ENTITIES];
	unsigned int entupdatetime[MAX_ENTITIES];	//to stop lerping when it's an old entity (bodies, stationary grenades, ...)
	int maxents;
	staticsound_t staticsound[MAX_STATICSOUNDS];
	int staticsound_count;
	filename_t lightstyle[MAX_LIGHTSTYLES];

	char serverinfo[MAX_SERVERINFO_STRING];
	playerinfo_t players[MAX_CLIENTS];

	filename_t modellist[MAX_MODELS];
	filename_t soundlist[MAX_SOUNDS];
	int modelindex_spike;	// qw is wierd.

	FILE *file;
	unsigned int filelength;
	SOCKET sourcesock;

	SOCKET listenmvd;	//tcp + mvd protocol
	SOCKET qwdsocket;	//udp + quakeworld protocols

	viewer_t *viewers;
	oproxy_t *proxies;

	qboolean parsingconnectiondata;	//so reject any new connects for now

	unsigned int curtime;
	unsigned int oldpackettime;
	unsigned int nextpackettime;

	int tcplistenportnum;
	int qwlistenportnum;
	char server[MAX_QPATH];

	//options:
	qboolean chokeonnotupdated;
	qboolean lateforward;
} sv_t;

typedef struct {
	sv_t server;
} qtv_t;







unsigned char ReadByte(netmsg_t *b);
unsigned short ReadShort(netmsg_t *b);
unsigned int ReadLong(netmsg_t *b);
float ReadFloat(netmsg_t *b);
void ReadString(netmsg_t *b, char *string, int maxlen);





#define	clc_bad			0
#define	clc_nop 		1
//define	clc_doublemove	2
#define	clc_move		3		// [[usercmd_t]
#define	clc_stringcmd	4		// [string] message
#define	clc_delta		5		// [byte] sequence number, requests delta compression of message
#define clc_tmove		6		// teleport request, spectator only
#define clc_upload		7		// teleport request, spectator only






#define	svc_bad				0
#define	svc_nop				1
//#define	svc_disconnect		2
#define	svc_updatestat		3	// [qbyte] [qbyte]
//#define	svc_version			4	// [long] server version
//#define	svc_setview			5	// [short] entity number
#define	svc_sound			6	// <see code>
//#define	svc_time			7	// [float] server time
#define	svc_print			8	// [qbyte] id [string] null terminated string
#define	svc_stufftext		9	// [string] stuffed into client's console buffer
								// the string should be \n terminated
#define	svc_setangle		10	// [angle3] set the view angle to this absolute value

#define	svc_serverdata		11	// [long] protocol ...
#define	svc_lightstyle		12	// [qbyte] [string]
//#define	svc_updatename		13	// [qbyte] [string]
#define	svc_updatefrags		14	// [qbyte] [short]
//#define	svc_clientdata		15	// <shortbits + data>
//#define	svc_stopsound		16	// <see code>
//#define	svc_updatecolors	17	// [qbyte] [qbyte] [qbyte]
//#define	svc_particle		18	// [vec3] <variable>
#define	svc_damage			19
	
//#define	svc_spawnstatic		20
//#define	svc_spawnstatic2	21
#define	svc_spawnbaseline	22
	
#define	svc_temp_entity		23	// variable
//#define	svc_setpause		24	// [qbyte] on / off
//#define	svc_signonnum		25	// [qbyte]  used for the signon sequence

#define	svc_centerprint		26	// [string] to put in center of the screen

//#define	svc_killedmonster	27
//#define	svc_foundsecret		28

#define	svc_spawnstaticsound	29	// [coord3] [qbyte] samp [qbyte] vol [qbyte] aten

//#define	svc_intermission	30		// [vec3_t] origin [vec3_t] angle
//#define	svc_finale			31		// [string] text

#define	svc_cdtrack			32		// [qbyte] track
//#define svc_sellscreen		33

//#define svc_cutscene		34	//hmm... nq only... added after qw tree splitt?

#define	svc_smallkick		34		// set client punchangle to 2
#define	svc_bigkick			35		// set client punchangle to 4

#define	svc_updateping		36		// [qbyte] [short]
#define	svc_updateentertime	37		// [qbyte] [float]

#define	svc_updatestatlong	38		// [qbyte] [long]

#define	svc_muzzleflash		39		// [short] entity

#define	svc_updateuserinfo	40		// [qbyte] slot [long] uid
									// [string] userinfo

#define	svc_download		41		// [short] size [size bytes]
#define	svc_playerinfo		42		// variable
//#define	svc_nails			43		// [qbyte] num [48 bits] xyzpy 12 12 12 4 8 
#define	svc_chokecount		44		// [qbyte] packets choked
#define	svc_modellist		45		// [strings]
#define	svc_soundlist		46		// [strings]
#define	svc_packetentities	47		// [...]
#define	svc_deltapacketentities	48		// [...]
//#define svc_maxspeed		49		// maxspeed change, for prediction
//#define svc_entgravity		50		// gravity change, for prediction
//#define svc_setinfo			51		// setinfo on a client
#define svc_serverinfo		52		// serverinfo
#define svc_updatepl		53		// [qbyte] [qbyte]

//#define svc_nails2			54		//qwe - [qbyte] num [52 bits] nxyzpy 8 12 12 12 4 8 






#define dem_cmd			0
#define dem_read		1
#define dem_set			2
#define dem_multiple	3
#define	dem_single		4
#define dem_stats		5
#define dem_all			6

#define dem_mask		7


#define	PROTOCOL_VERSION	28


//flags on entities
#define	U_ORIGIN1	(1<<9)
#define	U_ORIGIN2	(1<<10)
#define	U_ORIGIN3	(1<<11)
#define	U_ANGLE2	(1<<12)
#define	U_FRAME		(1<<13)
#define	U_REMOVE	(1<<14)		// REMOVE this entity, don't add it
#define	U_MOREBITS	(1<<15)

// if MOREBITS is set, these additional flags are read in next
#define	U_ANGLE1	(1<<0)
#define	U_ANGLE3	(1<<1)
#define	U_MODEL		(1<<2)
#define	U_COLORMAP	(1<<3)
#define	U_SKIN		(1<<4)
#define	U_EFFECTS	(1<<5)
#define	U_SOLID		(1<<6)		// the entity should be solid for prediction





//flags on players
#define	PF_MSEC			(1<<0)
#define	PF_COMMAND		(1<<1)
#define	PF_VELOCITY1	(1<<2)
#define	PF_VELOCITY2	(1<<3)
#define	PF_VELOCITY3	(1<<4)
#define	PF_MODEL		(1<<5)
#define	PF_SKINNUM		(1<<6)
#define	PF_EFFECTS		(1<<7)
#define	PF_WEAPONFRAME	(1<<8)		// only sent for view player
#define	PF_DEAD			(1<<9)		// don't block movement any more
#define	PF_GIB			(1<<10)		// offset the view height differently








void InitNetMsg(netmsg_t *b, char *buffer, int bufferlength);
unsigned char ReadByte(netmsg_t *b);
unsigned short ReadShort(netmsg_t *b);
unsigned int ReadLong(netmsg_t *b);
float ReadFloat(netmsg_t *b);
void ReadString(netmsg_t *b, char *string, int maxlen);
void WriteByte(netmsg_t *b, unsigned char c);
void WriteShort(netmsg_t *b, unsigned short l);
void WriteLong(netmsg_t *b, unsigned int l);
void WriteFloat(netmsg_t *b, float f);
void WriteString2(netmsg_t *b, const char *str);
void WriteString(netmsg_t *b, const char *str);
void WriteData(netmsg_t *b, const char *data, int length);

void ParseMessage(sv_t *tv, char *buffer, int length, int to, int mask);
void BuildServerData(sv_t *tv, netmsg_t *msg, qboolean mvd);
SOCKET QW_InitUDPSocket(int port);
void QW_UpdateUDPStuff(sv_t *qtv);
