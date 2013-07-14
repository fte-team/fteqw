/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// server.h

#define	QW_SERVER

#define	MAX_MASTERS	8				// max recipients for heartbeat packets

#define	MAX_SIGNON_BUFFERS	16

typedef enum {
	ss_dead,			// no map loaded
	ss_loading,			// spawning level edicts
	ss_active,			// actively running
	ss_cinematic
} server_state_t;
// some qc commands are only valid before the server has finished
// initializing (precache commands, static sounds / objects, etc)

#ifdef SVCHAT
typedef struct chatvar_s {
	char varname[64];
	float value;

	struct chatvar_s *next;
} chatvar_t;
typedef struct {
	qboolean active;
	char filename[64];
	edict_t *edict;

	char maintext[1024];
	struct
	{
		float tag;
		char text[256];
	} option[6];
	int options;

	chatvar_t *vars;

	float time;
} svchat_t;
#endif

typedef struct {
	int netstyle;
	char particleeffecttype[64];
	char stain[3];
	qbyte radius;
	qbyte dlightrgb[3];
	qbyte dlightradius;
	qbyte dlighttime;
	qbyte dlightcfade[3];
} svcustomtents_t;

typedef struct laggedpacket_s
{
	double time;
	struct laggedpacket_s *next;
	int length;
	unsigned char data[MAX_QWMSGLEN+10];
} laggedpacket_t;

typedef struct
{
	vec3_t	origin;
	char	angles[3];
	qbyte	modelindex;
	qbyte	frame;
	qbyte	colormap;
	qbyte	skinnum;
	qbyte	effects;

	qbyte	scale;
	qbyte	trans;
	char	fatness;
} mvdentity_state_t;

extern entity_state_t *sv_staticentities;
extern int sv_max_staticentities;

typedef struct
{
	qboolean	active;				// false when server is going down
	server_state_t	state;			// precache commands are only valid during load

	float		gamespeed;	//time progression multiplier, fixed per-level.
	qboolean	csqcdebug;
	unsigned int csqcchecksum;
	qboolean	mapchangelocked;

	double		time;
	double		starttime;
	int framenum;

	qboolean	paused;				// are we paused?
	float		pausedstart;

	//check player/eyes models for hacks
	unsigned	model_player_checksum;
	unsigned	eyes_player_checksum;

	char		name[64];			// file map name
	char		mapname[256];
	char		modelname[MAX_QPATH];		// maps/<name>.bsp, for model_precache[0]

	world_t world;

	union {
#ifdef Q2SERVER
		struct {
			char configstring[Q2MAX_CONFIGSTRINGS][MAX_QPATH];
		};
#endif
		struct {
			char		*vw_model_precache[32];
			char		*model_precache[MAX_MODELS];	// NULL terminated
			char		particle_precache[MAX_SSPARTICLESPRE][MAX_QPATH];	// NULL terminated
			char		sound_precache[MAX_SOUNDS][MAX_QPATH];	// NULL terminated
			char		*lightstyles[MAX_LIGHTSTYLES];
			char		lightstylecolours[MAX_LIGHTSTYLES];
		};
	} strings;
	qbyte		h2cdtrack;

	int			allocated_client_slots;	//number of slots available. (used mostly to stop single player saved games cacking up)
	int			spawned_client_slots; //number of PLAYER slots which are active (ie: putclientinserver was called)
	int			spawned_observer_slots;

	model_t	*models[MAX_MODELS];
	qbyte		*pvs, *phs;			// fully expanded and decompressed

	// added to every client's unreliable buffer each frame, then cleared
	sizebuf_t	datagram;
	qbyte		datagram_buf[MAX_DATAGRAM];

	// added to every client's reliable buffer each frame, then cleared
	sizebuf_t	reliable_datagram;
	qbyte		reliable_datagram_buf[MAX_QWMSGLEN];

	// the multicast buffer is used to send a message to a set of clients
	sizebuf_t	multicast;
	qbyte		multicast_buf[MAX_QWMSGLEN];

#ifdef NQPROT
	sizebuf_t	nqdatagram;
	qbyte		nqdatagram_buf[MAX_NQDATAGRAM];

	sizebuf_t	nqreliable_datagram;
	qbyte		nqreliable_datagram_buf[MAX_NQMSGLEN];

	sizebuf_t	nqmulticast;
	qbyte		nqmulticast_buf[MAX_NQMSGLEN];
#endif


	sizebuf_t	q2datagram;
	qbyte		q2datagram_buf[MAX_Q2DATAGRAM];

	sizebuf_t	q2reliable_datagram;
	qbyte		q2reliable_datagram_buf[MAX_Q2MSGLEN];

	sizebuf_t	q2multicast;
	qbyte		q2multicast_buf[MAX_Q2MSGLEN];

	// the master buffer is used for building log packets
	sizebuf_t	master;
	qbyte		master_buf[MAX_DATAGRAM];

	// the signon buffer will be sent to each client as they connect
	// traditionally includes the entity baselines, the static entities, etc
	// large levels will have >MAX_DATAGRAM sized signons, so
	// multiple signon messages are kept
	// fte only stores writebyted stuff in here. everything else is regenerated based upon the client's extensions.
	sizebuf_t	signon;
	int			num_signon_buffers;
	int			signon_buffer_size[MAX_SIGNON_BUFFERS];
	qbyte		signon_buffers[MAX_SIGNON_BUFFERS][MAX_DATAGRAM];

	qboolean msgfromdemo;

	qboolean gamedirchanged;





	qboolean mvdrecording;

//====================================================
//this lot is for serverside playback of demos
#ifdef SERVER_DEMO_PLAYBACK
	qboolean mvdplayback;
	float realtime;
	vfsfile_t *demofile;	//also signifies playing the thing.

	int lasttype;
	int lastto;

//playback spikes (svc_nails/nails2)
	int numdemospikes;
	struct {
		vec3_t org;
		qbyte id;
		qbyte pitch;
		qbyte yaw;
		qbyte modelindex;
	} demospikes[255];

//playback of entities (svc_nails/nails2)
	mvdentity_state_t	*demostate;
	mvdentity_state_t	*demobaselines;
	int demomaxents;
	qboolean demostatevalid;

//players
	struct {
		int stats[MAX_CL_STATS];
		int pl;
		int ping;
		int frags;
		int userid;
		int weaponframe;
		char			userinfo[MAX_INFO_STRING];
		vec3_t oldorg;
		vec3_t oldang;
		float updatetime;
	} recordedplayer[MAX_CLIENTS];

//gamestate
	char		demoinfo[MAX_SERVERINFO_STRING];
	char		demmodel_precache[MAX_MODELS][MAX_QPATH];	// NULL terminated
	char		demsound_precache[MAX_SOUNDS][MAX_QPATH];	// NULL terminated
	char		demgamedir[64];
	char		demname[64];			// map name

	qboolean	democausesreconnect;	//this makes current clients go through the connection process (and when the demo ends too)
	sizebuf_t	demosignon;
	int			num_demosignon_buffers;
	int			demosignon_buffer_size[MAX_SIGNON_BUFFERS];
	qbyte		demosignon_buffers[MAX_SIGNON_BUFFERS][MAX_DATAGRAM];
	char		demfullmapname[64];

	char		*demolightstyles[MAX_LIGHTSTYLES];
#endif
//====================================================
//	movevars_t	demomovevars;	//FIXME:!
//end this lot... (demo playback)

	int num_static_entities;

	svcustomtents_t customtents[255];

	int		*csqcentversion;//prevents ent versions from going backwards
} server_t;

typedef enum
{
	cs_free,		// can be reused for a new connection
	cs_zombie,		// client has been disconnected, but don't reuse
					// connection for a couple seconds
	cs_connected,	// has been assigned to a client_t, but not in game yet
	cs_spawned		// client is fully in game
} client_conn_state_t;

typedef struct
{
	// received from client

	// reply
	double				senttime;		//time we sent this frame to the client, for ping calcs
	int					sequence;		//the outgoing sequence - without mask, meaning we know if its current or stale
	float				ping_time;		//how long it took for the client to ack it, may be negativ
	float				move_msecs;		//
	int					packetsizein;	//amount of data received for this frame
	int					packetsizeout;	//amount of data that was sent in the frame
	vec3_t				playerpositions[MAX_CLIENTS];	//where each player was in this frame, for antilag
	qboolean			playerpresent[MAX_CLIENTS];		//whether the player was actually present
	packet_entities_t	entities;		//package containing entity states that were sent in this frame, for deltaing
	unsigned int		*resendentnum;	//the number of each entity that was sent in this frame
	unsigned int		*resendentbits;	//the bits of each entity that were sent in this frame
} client_frame_t;

#ifdef Q2SERVER
typedef struct	//merge?
{
	int					areabytes;
	qbyte				areabits[MAX_Q2MAP_AREAS/8];		// portalarea visibility bits
	q2player_state_t	ps;
	int					num_entities;
	int					first_entity;		// into the circular sv_packet_entities[]
	int					senttime;			// for ping calculations
	float				ping_time;
} q2client_frame_t;
#endif
#ifdef Q3SERVER
#include "clq3defs.h"
typedef struct	//merge?
{
	int					flags;
	int					areabytes;
	qbyte				areabits[MAX_Q2MAP_AREAS/8];		// portalarea visibility bits
	q3playerState_t		ps;
	int					num_entities;
	int					first_entity;		// into the circular sv_packet_entities[]
	int					senttime;			// for ping calculations


	int				serverMessageNum;
	int				serverCommandNum;
	int				serverTime;		// server time the message is valid for (in msec)
	int				localTime;
	int				deltaFrame;
} q3client_frame_t;
#endif

#define MAX_BACK_BUFFERS 16

enum
{
	PRESPAWN_INVALID=0,
	PRESPAWN_SERVERINFO,
	PRESPAWN_SOUNDLIST,	//nq skips these
	PRESPAWN_MODELLIST,
	PRESPAWN_MAPCHECK,	//wait for old prespawn command
	PRESPAWN_PARTICLES,
	PRESPAWN_CUSTOMTENTS,
	PRESPAWN_SIGNON_BUF,
	PRESPAWN_SPAWNSTATIC,
	PRESPAWN_BASELINES,
	PRESPAWN_DONE
};

typedef struct client_s
{
	client_conn_state_t	state;

	unsigned int	prespawn_stage;
	unsigned int	prespawn_idx;

	int				spectator;			// non-interactive
	int				redirect;

	qboolean		sendinfo;			// at end of frame, send info to all
										// this prevents malicious multiple broadcasts
	float			lastnametime;		// time of last name change
	int				lastnamecount;		// time of last name change
	unsigned		checksum;			// checksum for calcs
	qboolean		drop;				// lose this guy next opportunity
	int				lossage;			// loss percentage

	int challenge;
	int				userid;							// identifying number
	char			userinfo[EXTENDED_INFO_STRING];		// infostring

	usercmd_t		lastcmd;			// for filling in big drops and partial predictions
	double			localtime;			// of last message
	qboolean jump_held;
	qboolean lockangles;	//mod is spamming angle changes, don't do relative changes

	float			maxspeed;			// localized maxspeed
	float			entgravity;			// localized ent gravity

	int viewent;	//fake the entity positioning.

	edict_t			*edict;				// EDICT_NUM(clientnum+1)
//additional game modes use additional edict pointers. this ensures that references are crashes.
#ifdef Q2SERVER
	q2edict_t		*q2edict;				// EDICT_NUM(clientnum+1)
#endif
#ifdef HLSERVER
	struct hledict_s	*hledict;
#endif

	int				playercolor;
	int				playerclass;
	char			teambuf[32];
	char			*team;
	char			*name;
	char			namebuf[32];			// for printing to other people
										// extracted from userinfo
	char			guid[32]; /*+2 for split+pad*/
	int				messagelevel;		// for filtering printed messages

	// the datagram is written to after every frame, but only cleared
	// when it is sent out to the client.  overflow is tolerated.
	sizebuf_t		datagram;
	qbyte			datagram_buf[MAX_OVERALLMSGLEN/2];

	// back buffers for client reliable data
	sizebuf_t	backbuf;
	int			num_backbuf;
	int			backbuf_size[MAX_BACK_BUFFERS];
	qbyte		backbuf_data[MAX_BACK_BUFFERS][MAX_BACKBUFLEN];

	double			connection_started;	// or time of disconnect for zombies
	qboolean		send_message;		// set on frames a datagram arived on

	laggedentinfo_t	laggedents[MAX_CLIENTS];
	unsigned int	laggedents_count;
	float			laggedents_frac;

// spawn parms are carried from level to level
	float			spawn_parms[NUM_SPAWN_PARMS];
	char			*spawninfo;
	float			spawninfotime;
	float			nextservertimeupdate;

// client known data for deltas
	int				old_frags;

	int				statsi[MAX_CL_STATS];
	float			statsf[MAX_CL_STATS];
	char			*statss[MAX_CL_STATS];
	char			*centerprintstring;

	union{	//save space
		client_frame_t	*frames;	// updates can be deltad from here
#ifdef Q2SERVER
		q2client_frame_t	*q2frames;
#endif
#ifdef Q3SERVER
		q3client_frame_t	*q3frames;
#endif
	} frameunion;
	packet_entities_t sentents;
	unsigned int	*pendingentbits;

	char			downloadfn[MAX_QPATH];
	vfsfile_t		*download;			// file being downloaded
	unsigned int	downloadsize;		// total bytes
	unsigned int	downloadcount;		// bytes sent

	int				downloadacked;		//DP-specific
	int				downloadstarted;	//DP-specific

	int				spec_track;			// entnum of player tracking

#ifdef Q3SERVER
	int	gamestatesequence;	//the sequence number the initial gamestate was sent in.
	int last_server_command_num;
	int last_client_command_num;
	int num_server_commands;
	int num_client_commands;
	char server_commands[64][1024];
	char last_client_command[1024];
#endif
#ifdef PEXT_CSQC
	int				csqclastsentsequence;
	int				*csqcentsequence;//the sequence number a csqc entity was sent in
	int				*csqcentversions;//the version of the entity when it was sent in that sequenced packet.
#endif

	//true/false/persist
	qbyte		ismuted;
	qbyte		iscuffed;
	qbyte		iscrippled;

	qbyte		istobeloaded;	//loadgame creates place holders for clients to connect to. Effectivly loading a game reconnects all clients, but has precreated ents.

	double			floodprotmessage;
	double			lastspoke;
 	double			lockedtill;

	qboolean		upgradewarn;		// did we warn him?

	vfsfile_t		*upload;
	char			uploadfn[MAX_QPATH];
	netadr_t		snap_from;
	qboolean		remote_snap;

//===== NETWORK ============
	int				chokecount;
	qboolean		waschoked;
	int				delta_sequence;		// -1 = no compression
	int				last_sequence;
	netchan_t		netchan;
	qboolean		isindependant;

	int				lastsequence_acknowledged;

#ifdef VOICECHAT
	unsigned int voice_read;	/*place in ring*/
	unsigned char voice_mute[(MAX_CLIENTS+7)/8];
	qboolean voice_active;
	enum
	{
		/*note - when recording an mvd, only 'all' will be received by non-spectating viewers. all other chat will only be heard when spectating the receiver(or sender) of said chat*/

		/*should we add one to respond to the last speaker? or should that be an automagic +voip_reply instead?*/
		VT_TEAM,
		VT_ALL,
		VT_NONMUTED,	/*cheap, but allows custom private channels with no external pesters*/
		VT_PLAYERSLOT0
		/*player0+...*/
	} voice_target;
#endif

#ifdef SVCHAT
	svchat_t chat;
#endif
#ifdef SVRANKING
	int rankid;

	int	kills;
	int	deaths;

	double			stats_started;
#endif

	qboolean		csqcactive;
#ifdef PROTOCOL_VERSION_FTE
	qboolean        pextknown;
	unsigned int	fteprotocolextensions;
	unsigned int	fteprotocolextensions2;
#endif
	unsigned int	zquake_extensions;
	unsigned int    max_net_ents; /*highest entity number the client can receive (limited by either protocol or client's buffer size)*/
	unsigned int	max_net_clients; /*max number of player slots supported by the client */
	unsigned int	maxmodels; /*max models supported by whatever the protocol is*/

	enum {
		SCP_BAD,	//don't send (a bot)
		SCP_QUAKEWORLD,
		SCP_QUAKE2,
		SCP_QUAKE3,
		//all the below are considered netquake clients.
		SCP_NETQUAKE,
		SCP_PROQUAKE,
		SCP_FITZ666,
		SCP_DARKPLACES6,
		SCP_DARKPLACES7	//extra prediction stuff
		//note, nq is nq+
	} protocol;

//speed cheat testing
	int msecs;
	int msec_cheating;
	float last_check;

	qboolean gibfilter;

	int trustlevel;

	qboolean wasrecorded;	//this client shouldn't get any net messages sent to them

	vec3_t	specorigin;	//mvds need to use a different origin from the one QC has.
	vec3_t	specvelocity;

	int language;	//the clients language

//	struct {
//		qbyte vweap;
//	} otherclientsknown[MAX_CLIENTS];	//updated as needed. Flag at a time, or all flags.

	struct client_s *controller;	/*first in splitscreen chain, NULL=nosplitscreen*/
	struct client_s *controlled;	/*next in splitscreen chain*/

	/*these are the current rates*/
	float ratetime;
	float inrate;
	float outrate;

	int rate;
	int drate;

	netadr_t realip;
	int realip_status;
	int realip_num;
	int realip_ping;

	float delay;
	laggedpacket_t *laggedpacket;
	laggedpacket_t *laggedpacket_last;
} client_t;

#define ISQWCLIENT(cl) ((cl)->protocol == SCP_QUAKEWORLD)
#define ISQ2CLIENT(cl) ((cl)->protocol == SCP_QUAKE2)
#define ISQ3CLIENT(cl) ((cl)->protocol == SCP_QUAKE3)
#define ISNQCLIENT(cl) ((cl)->protocol >= SCP_NETQUAKE)
#define ISDPCLIENT(cl) ((cl)->protocol >= SCP_DARKPLACES6)

// a client can leave the server in one of four ways:
// dropping properly by quiting or disconnecting
// timing out if no valid messages are received for timeout.value seconds
// getting kicked off by the server operator
// a program error, like an overflowed reliable buffer




//=============================================================================

//mvd stuff

#define	MSG_BUF_SIZE 8192

typedef struct
{
	vec3_t	origin;
	vec3_t	angles;
	int		weaponframe;
	int		skinnum;
	int		model;
	int		effects;
}	demoinfo_t;

typedef struct
{
	demoinfo_t	info;
	float		sec;
	int			parsecount;
	qboolean	fixangle;
	vec3_t		angle;
	float		cmdtime;
	int			flags;
	int			frame;
} demo_client_t;

typedef struct {
	qbyte type;
	qbyte full;
	int to;
	int size;
	qbyte data[1]; //gcc doesn't allow [] (?)
} header_t;

typedef struct
{
	sizebuf_t sb;
	int		bufsize;
	header_t *h;
} demobuf_t;

typedef struct
{
	demo_client_t clients[MAX_CLIENTS];
	double		time;
	demobuf_t	buf;
} demo_frame_t;

typedef struct {
	qbyte	*data;
	int		start, end, last;
	int		maxsize;
} dbuffer_t;

#define DEMO_FRAMES 64
#define DEMO_FRAMES_MASK (DEMO_FRAMES - 1)

typedef struct
{
//	demobuf_t	*dbuf;
//	dbuffer_t	dbuffer;
	sizebuf_t	datagram;
	qbyte		datagram_data[MSG_BUF_SIZE];
	int			lastto;
	int			lasttype;
	double		time, pingtime;
	int			statsi[MAX_CLIENTS][MAX_CL_STATS]; // ouch!
	float		statsf[MAX_CLIENTS][MAX_CL_STATS]; // ouch!
	char		*statss[MAX_CLIENTS][MAX_CL_STATS]; // ouch!
	client_t	recorder;
	qboolean	fixangle[MAX_CLIENTS];
	float		fixangletime[MAX_CLIENTS];
	vec3_t		angles[MAX_CLIENTS];
	int			parsecount;
	int			lastwritten;
	demo_frame_t	frames[DEMO_FRAMES];
	demoinfo_t	info[MAX_CLIENTS];
	qbyte		buffer[20*MAX_QWMSGLEN];
	int			bufsize;
	int			forceFrame;

	struct mvddest_s *dest;
	struct mvdpendingdest_s *pendingdest;
} demo_t;


//=============================================================================


#define	STATFRAMES	100
typedef struct
{
	double	active;
	double	idle;
	int		count;
	int		packets;

	double	latched_active;
	double	latched_idle;
	int		latched_packets;
} svstats_t;

// MAX_CHALLENGES is made large to prevent a denial
// of service attack that could cycle all of them
// out before legitimate users connected
#define	MAX_CHALLENGES	1024

typedef struct
{
	netadr_t	adr;
	int			challenge;
	int			time;
} challenge_t;

typedef struct bannedips_s {
	enum {BAN_BAN, BAN_FILTER, BAN_PERMIT} type;
	struct bannedips_s *next;
	netadr_t	adr;
	netadr_t	adrmask;
	unsigned int expiretime;
	char reason[1];
} bannedips_t;

typedef enum {
	GT_PROGS,	//q1, qw, h2 are similar enough that we consider it only one game mode. (We don't support the h2 protocol)
	GT_Q1QVM,
	GT_HALFLIFE,
	GT_QUAKE2,	//q2 servers run from a q2 game dll
	GT_QUAKE3,	//q3 servers run off the q3 qvm api
	GT_MAX
} gametype_e;

typedef struct levelcache_s {
	struct levelcache_s *next;
	char *mapname;
	gametype_e gametype;
} levelcache_t;

#ifdef TCPCONNECT
typedef struct svtcpstream_s {
	int socketnum;
	int inlen;
	qboolean waitingforprotocolconfirmation;
	char inbuffer[1500];
	float timeouttime;
	netadr_t remoteaddr;
	struct svtcpstream_s *next;
} svtcpstream_t;
#endif

typedef struct
{
	gametype_e	gametype;
	int			spawncount;			// number of servers spawned since start,
									// used to check late spawns
	int framenum;	//physics frame number for out-of-sequence thinks (fix for slow rockets)

	struct ftenet_connections_s *sockets;

	client_t	clients[MAX_CLIENTS];
	int			serverflags;		// episode completion information

	double		last_heartbeat;
	int			heartbeat_sequence;
	svstats_t	stats;

	char		info[MAX_SERVERINFO_STRING];

	// log messages are used so that fraglog processes can get stats
	int			logsequence;	// the message currently being filled
	double		logtime;		// time of last swap
	sizebuf_t	log[2];
	qbyte		log_buf[2][MAX_DATAGRAM];

	challenge_t	challenges[MAX_CHALLENGES];	// to prevent invalid IPs from connecting

	bannedips_t *bannedips;

	char progsnames[MAX_PROGS][32];
	progsnum_t progsnum[MAX_PROGS];
	int numprogs;

	struct netprim_s netprim;

	qboolean demoplayback;
	qboolean demorecording;
	qboolean msgfromdemo;

	int language;	//the server operators language
	laggedpacket_t *free_lagged_packet;
	packet_entities_t entstatebuffer; /*just a temp buffer*/

	levelcache_t *levcache;
} server_static_t;

//=============================================================================

// edict->movetype values
#define	MOVETYPE_NONE			0		// never moves
#define	MOVETYPE_ANGLENOCLIP	1
#define	MOVETYPE_ANGLECLIP		2
#define	MOVETYPE_WALK			3		// gravity
#define	MOVETYPE_STEP			4		// gravity, special edge handling
#define	MOVETYPE_FLY			5
#define	MOVETYPE_TOSS			6		// gravity
#define	MOVETYPE_PUSH			7		// no clip to world, push and crush
#define	MOVETYPE_NOCLIP			8
#define	MOVETYPE_FLYMISSILE		9		// extra size to monsters
#define	MOVETYPE_BOUNCE			10
#define MOVETYPE_BOUNCEMISSILE	11		// bounce w/o gravity
#define MOVETYPE_FOLLOW			12		// track movement of aiment
#define MOVETYPE_H2PUSHPULL		13		// pushable/pullable object
#define MOVETYPE_H2SWIM			14		// should keep the object in water
#define MOVETYPE_PHYSICS		32

// edict->solid values
#define	SOLID_NOT				0		// no interaction with other objects
#define	SOLID_TRIGGER			1		// touch on edge, but not blocking
#define	SOLID_BBOX				2		// touch on edge, block
#define	SOLID_SLIDEBOX			3		// touch on edge, but not an onground
#define	SOLID_BSP				4		// bsp clip, touch on edge, block
#define	SOLID_PHASEH2			5
#define	SOLID_CORPSE			5
#define SOLID_LADDER			20		//dmw. touch on edge, not blocking. Touching players have different physics. Otherwise a SOLID_TRIGGER

#define	DAMAGE_NO				0
#define	DAMAGE_YES				1
#define	DAMAGE_AIM				2

#define PVSF_NORMALPVS		0x0
#define PVSF_NOTRACECHECK	0x1
#define PVSF_USEPHS			0x2
#define PVSF_IGNOREPVS		0x3
#define PVSF_MODE_MASK		0x3
#define PVSF_NOREMOVE		0x80

// entity effects

//define	EF_BRIGHTFIELD			1
//define	EF_MUZZLEFLASH 			2
//#define	EF_BRIGHTLIGHT 			(1<<2)
//#define	EF_DIMLIGHT 			(1<<4)

//#define	EF_FULLBRIGHT			512


#define	SPAWNFLAG_NOT_EASY			(1<<8)
#define	SPAWNFLAG_NOT_MEDIUM		(1<<9)
#define	SPAWNFLAG_NOT_HARD			(1<<10)
#define	SPAWNFLAG_NOT_DEATHMATCH	(1<<11)

#define SPAWNFLAG_NOT_H2PALADIN			(1<<8)
#define SPAWNFLAG_NOT_H2CLERIC			(1<<9)
#define SPAWNFLAG_NOT_H2NECROMANCER		(1<<10)
#define SPAWNFLAG_NOT_H2THEIF			(1<<11)
#define	SPAWNFLAG_NOT_H2EASY			(1<<12)
#define	SPAWNFLAG_NOT_H2MEDIUM			(1<<13)
#define	SPAWNFLAG_NOT_H2HARD		    (1<<14)
#define	SPAWNFLAG_NOT_H2DEATHMATCH		(1<<15)
#define SPAWNFLAG_NOT_H2COOP			(1<<16)
#define SPAWNFLAG_NOT_H2SINGLE			(1<<17)

#if 0//ndef Q2SERVER
typedef enum multicast_e
{
	MULTICAST_ALL,
	MULTICAST_PHS,
	MULTICAST_PVS,
	MULTICAST_ALL_R,
	MULTICAST_PHS_R,
	MULTICAST_PVS_R
} multicast_t;
#endif


//shared with qc
#define MSG_PRERELONE	-100
#define	MSG_BROADCAST	0		// unreliable to all
#define	MSG_ONE			1		// reliable to one (msg_entity)
#define	MSG_ALL			2		// reliable to all
#define	MSG_INIT		3		// write to the init string
#define	MSG_MULTICAST	4		// for multicast()
#define MSG_CSQC		5		// for writing csqc entities

//============================================================================

extern	cvar_t	sv_mintic, sv_maxtic, sv_limittics;
extern	cvar_t	sv_maxspeed;
extern	cvar_t	sv_antilag;
extern	cvar_t	sv_antilag_frac;

extern	netadr_t	master_adr[MAX_MASTERS];	// address of the master server

extern	cvar_t	pr_ssqc_progs;
extern	cvar_t	spawn;
extern	cvar_t	teamplay;
extern	cvar_t	deathmatch;
extern	cvar_t	coop;
extern	cvar_t	fraglimit;
extern	cvar_t	timelimit;

extern	server_static_t	svs;				// persistant server info
extern	server_t		sv;					// local server

extern	client_t	*host_client;

extern	edict_t		*sv_player;

extern	char		localmodels[MAX_MODELS][5];	// inline model names for precache

extern	char		localinfo[MAX_LOCALINFO_STRING+1];

extern	vfsfile_t	*sv_fraglogfile;

//===========================================================

void SV_AddDebugPolygons(void);
char *SV_CheckRejectConnection(netadr_t *adr, char *uinfo, unsigned int protocol, unsigned int pext1, unsigned int pext2, char *guid);

//
// sv_main.c
//
NORETURN void VARGS SV_Error (char *error, ...) LIKEPRINTF(1);
void SV_Shutdown (void);
float SV_Frame (void);
void SV_FinalMessage (char *message);
void SV_DropClient (client_t *drop);
struct quakeparms_s;
void SV_Init (struct quakeparms_s *parms);
void SV_ExecInitialConfigs(char *defaultexec);

int SV_CalcPing (client_t *cl, qboolean forcecalc);
void SV_FullClientUpdate (client_t *client, client_t *to);
void SV_GeneratePublicUserInfo(int pext, client_t *cl, char *info, int infolength);

int SV_ModelIndex (char *name);

void SV_WriteClientdataToMessage (client_t *client, sizebuf_t *msg);
void SVQW_WriteDelta (entity_state_t *from, entity_state_t *to, sizebuf_t *msg, qboolean force, unsigned int protext);

void SV_SaveSpawnparms (qboolean);
void SV_SaveLevelCache(char *savename, qboolean dontharmgame);
void SV_Savegame (char *savename);
qboolean SV_LoadLevelCache(char *savename, char *level, char *startspot, qboolean ignoreplayers);

void SV_Physics_Client (edict_t	*ent, int num);

void SV_ExecuteUserCommand (char *s, qboolean fromQC);
void SV_InitOperatorCommands (void);

void SV_SendServerinfo (client_t *client);
void SV_ExtractFromUserinfo (client_t *cl, qboolean verbose);

void SV_SaveInfos(vfsfile_t *f);


void Master_Heartbeat (void);
void Master_Packet (void);

void SV_FixupName(char *in, char *out, unsigned int outlen);

//
// sv_init.c
//
void SV_SpawnServer (char *server, char *startspot, qboolean noents, qboolean usecinematic);
void SV_UnspawnServer (void);
void SV_FlushSignon (void);

void SV_FilterImpulseInit(void);
qboolean SV_FilterImpulse(int imp, int level);

//svq2_game.c
qboolean SVQ2_InitGameProgs(void);
void VARGS SVQ2_ShutdownGameProgs (void);

//svq2_ents.c
void SVQ2_BuildClientFrame (client_t *client);
void SVQ2_WriteFrameToClient (client_t *client, sizebuf_t *msg);
#ifdef Q2SERVER
void MSGQ2_WriteDeltaEntity (q2entity_state_t *from, q2entity_state_t *to, sizebuf_t *msg, qboolean force, qboolean newentity);
void SVQ2_BuildBaselines(void);
#endif

//q3 stuff
#ifdef Q3SERVER
void SVQ3_ShutdownGame(void);
qboolean SVQ3_InitGame(void);
qboolean SVQ3_ConsoleCommand(void);
qboolean SVQ3_HandleClient(void);
void SVQ3_DirectConnect(void);
void SVQ3_DropClient(client_t *cl);
int SVQ3_AddBot(void);
void SVQ3_RunFrame(void);
void SVQ3_SendMessage(client_t *client);
qboolean SVQ3_Command(void);
#endif


//
// sv_phys.c
//
void SV_SetMoveVars(void);
void WPhys_RunNewmis (world_t *w);
qboolean SV_Physics (void);
void WPhys_CheckVelocity (world_t *w, wedict_t *ent);
trace_t WPhys_Trace_Toss (world_t *w, wedict_t *ent, wedict_t *ignore);
void SV_ProgStartFrame (void);
void WPhys_RunEntity (world_t *w, wedict_t *ent);
qboolean WPhys_RunThink (world_t *w, wedict_t *ent);
void WPhys_MoveChain(world_t *w, wedict_t *ent, wedict_t *movechain, float *initial_origin, float *initial_angle); /*here for player movement to do movechains too*/

//
// sv_send.c
//
void SV_CalcNetRates(client_t *cl, double *ftime, int *frames, double *minf, double *maxf);	//gets received framerate etc info
qboolean SV_ChallengePasses(int challenge);
void SV_QCStatName(int type, char *name, int statnum);
void SV_QCStatFieldIdx(int type, unsigned int fieldindex, int statnum);
void SV_QCStatGlobal(int type, char *globalname, int statnum);
void SV_QCStatPtr(int type, void *ptr, int statnum);
void SV_ClearQCStats(void);

void SV_SendClientMessages (void);

void VARGS SV_Multicast (vec3_t origin, multicast_t to);
#define FULLDIMENSIONMASK 0xffffffff
void SV_MulticastProtExt(vec3_t origin, multicast_t to, int dimension_mask, int with, int without);

void SV_StartSound (int ent, vec3_t origin, int seenmask, int channel, char *sample, int volume, float attenuation, int pitchadj);
void SVQ1_StartSound (float *origin, wedict_t *entity, int channel, char *sample, int volume, float attenuation, int pitchadj);
void SV_PrintToClient(client_t *cl, int level, char *string);
void SV_StuffcmdToClient(client_t *cl, char *string);
void VARGS SV_ClientPrintf (client_t *cl, int level, char *fmt, ...) LIKEPRINTF(3);
void VARGS SV_ClientTPrintf (client_t *cl, int level, translation_t text, ...);
void VARGS SV_BroadcastPrintf (int level, char *fmt, ...) LIKEPRINTF(2);
void VARGS SV_BroadcastTPrintf (int level, translation_t fmt, ...);
void VARGS SV_BroadcastCommand (char *fmt, ...) LIKEPRINTF(1);
void SV_SendServerInfoChange(char *key, const char *value);
void SV_SendMessagesToAll (void);
void SV_FindModelNumbers (void);

//
// sv_user.c
//
#ifdef NQPROT
void SVNQ_New_f (void);
void SVNQ_ExecuteClientMessage (client_t *cl);
#endif
qboolean SV_UserInfoIsBasic(char *infoname);	//standard message.
void SV_ExecuteClientMessage (client_t *cl);
void SVQ2_ExecuteClientMessage (client_t *cl);
int SV_PMTypeForClient (client_t *cl);
void SV_UserInit (void);
qboolean SV_TogglePause (client_t *cl);

#ifdef PEXT2_VOICECHAT
void SV_VoiceInitClient(client_t *client);
void SV_VoiceSendPacket(client_t *client, sizebuf_t *buf);
#endif

void SV_ClientThink (void);
void SV_Begin_Core(client_t *split);

void VoteFlushAll(void);
void SV_SetUpClientEdict (client_t *cl, edict_t *ent);
void SV_UpdateToReliableMessages (void);
void SV_FlushBroadcasts (void);
qboolean SV_CanTrack(client_t *client, int entity);

void SV_DarkPlacesDownloadChunk(client_t *cl, sizebuf_t *msg);
void SV_New_f (void);

void SV_PreRunCmd(void);
void SV_RunCmd (usercmd_t *ucmd, qboolean recurse);
void SV_PostRunCmd(void);

void SV_SendClientPrespawnInfo(client_t *client);
void SV_ClientProtocolExtensionsChanged(client_t *client);

//sv_master.c
void SVM_Think(int port);


//
// svonly.c
//
typedef enum {RD_NONE, RD_CLIENT, RD_PACKET, RD_OBLIVION} redirect_t;	//oblivion is provided so people can read the output before the buffer is wiped.
void SV_BeginRedirect (redirect_t rd, int lang);
void SV_EndRedirect (void);

//
// sv_ccmds.c
//
void SV_Status_f (void);






qboolean PR_GameCodePacket(char *s);
qboolean PR_GameCodePausedTic(float pausedtime);
qboolean PR_ShouldTogglePause(client_t *initiator, qboolean pausedornot);

//
// sv_ents.c
//
void SV_WriteEntitiesToClient (client_t *client, sizebuf_t *msg, qboolean ignorepvs);
void SVFTE_EmitBaseline(entity_state_t *to, qboolean numberisimportant, sizebuf_t *msg);
void SVQ3Q1_BuildEntityPacket(client_t *client, packet_entities_t *pack);
int SV_HullNumForPlayer(int h2hull, float *mins, float *maxs);
void SV_GibFilterInit(void);
void SV_GibFilterPurge(void);
void SV_CleanupEnts(void);

void SV_CSQC_DroppedPacket(client_t *client, int sequence);
void SV_CSQC_DropAll(client_t *client);

//
// sv_nchan.c
//

void ClientReliableCheckBlock(client_t *cl, int maxsize);
void ClientReliable_FinishWrite(client_t *cl);
void ClientReliableWrite_Begin(client_t *cl, int c, int maxsize);
void ClientReliableWrite_Angle(client_t *cl, float f);
void ClientReliableWrite_Angle16(client_t *cl, float f);
void ClientReliableWrite_Byte(client_t *cl, int c);
void ClientReliableWrite_Char(client_t *cl, int c);
void ClientReliableWrite_Float(client_t *cl, float f);
void ClientReliableWrite_Coord(client_t *cl, float f);
void ClientReliableWrite_Long(client_t *cl, int c);
void ClientReliableWrite_Short(client_t *cl, int c);
void ClientReliableWrite_Entity(client_t *cl, int c);
void ClientReliableWrite_String(client_t *cl, char *s);
void ClientReliableWrite_SZ(client_t *cl, void *data, int len);


#ifdef  SVRANKING
//flags
#define RANK_MUTED		2
#define RANK_CUFFED		4
#define RANK_CRIPPLED	8	//ha ha... get speed cheaters with this!... :o)

#define NUM_RANK_SPAWN_PARMS 32

typedef struct {	//stats info
	int kills;
	int deaths;
	float parm[NUM_RANK_SPAWN_PARMS];
	float timeonserver;
	qbyte flags1;
	qbyte trustlevel;
	char pad2;
	char pad3;
} rankstats_t;

typedef struct {	//name, identity and order.
	int prev;		//score is held for convineance.
	int next;
	char name[32];
	int pwd;
	float score;
} rankheader_t;

typedef struct {
	rankheader_t h;
	rankstats_t s;
} rankinfo_t;

int Rank_GetPlayerID(char *guid, char *name, int pwd, qboolean allowcreate, qboolean requirepasswordtobeset);
void Rank_SetPlayerStats(int id, rankstats_t *stats);
rankstats_t *Rank_GetPlayerStats(int id, rankstats_t *buffer);
rankinfo_t *Rank_GetPlayerInfo(int id, rankinfo_t *buffer);
qboolean Rank_OpenRankings(void);
void Rank_Flush (void);

void Rank_ListTop10_f (void);
void Rank_RegisterCommands(void);
int Rank_GetPass (char *name);

extern cvar_t rank_needlogin;


client_t *SV_GetClientForString(char *name, int *id);
qboolean    SV_MayCheat(void);


qboolean ReloadRanking(client_t *cl, char *newname);
#endif






void NPP_Flush(void);
void NPP_NQWriteByte(int dest, qbyte data);
void NPP_NQWriteChar(int dest, char data);
void NPP_NQWriteShort(int dest, short data);
void NPP_NQWriteLong(int dest, long data);
void NPP_NQWriteAngle(int dest, float data);
void NPP_NQWriteCoord(int dest, float data);
void NPP_NQWriteString(int dest, char *data);
void NPP_NQWriteEntity(int dest, short data);

void NPP_QWWriteByte(int dest, qbyte data);
void NPP_QWWriteChar(int dest, char data);
void NPP_QWWriteShort(int dest, short data);
void NPP_QWWriteLong(int dest, long data);
void NPP_QWWriteAngle(int dest, float data);
void NPP_QWWriteCoord(int dest, float data);
void NPP_QWWriteString(int dest, char *data);
void NPP_QWWriteEntity(int dest, short data);



void NPP_MVDForceFlush(void);


//replacement rand function (useful cos it's fully portable, with seed grabbing)
void predictablesrand(unsigned int x);
int predictablerandgetseed(void);
int predictablerand(void);







#ifdef SVCHAT
void SV_WipeChat(client_t *client);
int SV_ChatMove(int impulse);
void SV_ChatThink(client_t *client);
#endif


void SV_ConSay_f(void);





//
// sv_mvd.c
//
//qtv proxies are meant to send a small header now, bit like http
//this header gives supported version numbers and stuff
typedef struct mvdpendingdest_s {
	qboolean error;	//disables writers, quit ASAP.
	int socket;

	char inbuffer[2048];
	char outbuffer[2048];

	char challenge[64];
	qboolean hasauthed;
	qboolean isreverse;

	int insize;
	int outsize;

	struct mvdpendingdest_s *nextdest;
} mvdpendingdest_t;

typedef struct mvddest_s {
	qboolean error;	//disables writers, quit ASAP.
	qboolean droponmapchange;

	enum {DEST_NONE, DEST_FILE, DEST_BUFFEREDFILE, DEST_STREAM} desttype;

	int socket;
	vfsfile_t *file;

	char name[MAX_QPATH];
	char path[MAX_QPATH];

	char *cache;
	int cacheused;
	int maxcachesize;

	unsigned int totalsize;

	struct mvddest_s *nextdest;
} mvddest_t;
void SV_MVDPings (void);
void SV_MVD_FullClientUpdate(sizebuf_t *msg, client_t *player);
sizebuf_t *MVDWrite_Begin(qbyte type, int to, int size);
void MVDSetMsgBuf(demobuf_t *prev,demobuf_t *cur);
void SV_MVDStop (int reason, qboolean mvdonly);
void SV_MVDStop_f (void);
qboolean SV_MVDWritePackets (int num);
void MVD_Init (void);
void SV_MVD_RunPendingConnections(void);
void SV_MVD_SendInitialGamestate(mvddest_t *dest);

extern demo_t			demo;				// server demo struct

extern cvar_t	sv_demofps;
extern cvar_t	sv_demoPings;
extern cvar_t	sv_demoUseCache;
extern cvar_t	sv_demoMaxSize;
extern cvar_t	sv_demoMaxDirSize;

char *SV_Demo_CurrentOutput(void);
void SV_MVDInit(void);
char *SV_MVDNum(char *buffer, int bufferlen, int num);
void SV_SendMVDMessage(void);
qboolean SV_ReadMVD (void);
void SV_FlushDemoSignon (void);
void DestFlush(qboolean compleate);

// savegame.c
void SV_FlushLevelCache(void);

int SV_RateForClient(client_t *cl);

qboolean TransformedNativeTrace (struct model_s *model, int hulloverride, int frame, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, unsigned int against, struct trace_s *trace, vec3_t origin, vec3_t angles);

void SVVC_Frame (qboolean enabled);
void SV_CalcPHS (void);

void SV_GetConsoleCommands (void);
void SV_CheckTimer(void);

void SV_LogPlayer(client_t *cl, char *msg);

void AddLinksToPmove ( edict_t *player, areanode_t *node );


#ifdef HLSERVER
void SVHL_SaveLevelCache(char *filename);

//network frame info
void SVHL_Snapshot_Build(client_t *client, packet_entities_t *pack, qbyte *pvs, edict_t *clent, qboolean ignorepvs);
qbyte	*SVHL_Snapshot_SetupPVS(client_t *client, qbyte *pvs, unsigned int pvsbufsize);
void SVHL_BuildStats(client_t *client, int *si, float *sf, char **ss);

//gamecode entry points
int SVHL_InitGame(void);
void SVHL_SetupGame(void);
void SVHL_SpawnEntities(char *entstring);
void SVHL_RunFrame (void);
qboolean SVHL_ClientConnect(client_t *client, netadr_t adr, char rejectmessage[128]);
void SVHL_PutClientInServer(client_t *client);
void SVHL_RunPlayerCommand(client_t *cl, usercmd_t *oldest, usercmd_t *oldcmd, usercmd_t *newcmd);
qboolean HLSV_ClientCommand(client_t *client);
void SVHL_DropClient(client_t *drop);
void SVHL_ShutdownGame(void);
#endif
