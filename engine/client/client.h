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
// client.h

#include "particles.h"

typedef struct
{
	char		name[64];
	int			width;
	int			height;

	//for hardware 32bit texture overrides
	texnums_t	textures;

	qboolean	failedload;		// the name isn't a valid skin
	cache_user_t	cache;
} skin_t;

// player_state_t is the information needed by a player entity
// to do move prediction and to generate a drawable entity
typedef struct
{
	int			messagenum;		// all player's won't be updated each frame

	double		state_time;		// not the same as the packet time,
								// because player commands come asyncronously
	usercmd_t	command;		// last command for prediction

	vec3_t		origin;
	vec3_t		predorigin;		// pre-predicted pos
	vec3_t		viewangles;		// only for demos, not from server
	vec3_t		velocity;
	int			weaponframe;

	unsigned int			modelindex;
	int			frame;
	int			skinnum;
	int			effects;

#ifdef PEXT_SCALE
	float scale;
#endif
	qbyte colourmod[3];
	qbyte alpha;
#ifdef PEXT_FATNESS
	float fatness;
#endif

	int			flags;			// dead, gib, etc

	int			pm_type;
	float		waterjumptime;
	qboolean	onground;
	qboolean	jump_held;
	int			jump_msec;		// hack for fixing bunny-hop flickering on non-ZQuake servers
	vec3_t		szmins, szmaxs;
	vec3_t		gravitydir;

	float lerpstarttime;
	int oldframe;
} player_state_t;


#if defined(Q2CLIENT) || defined(Q2SERVER)
typedef enum
{
	// can accelerate and turn
	Q2PM_NORMAL,
	Q2PM_SPECTATOR,
	// no acceleration or turning
	Q2PM_DEAD,
	Q2PM_GIB,		// different bounding box
	Q2PM_FREEZE
} q2pmtype_t;
typedef struct
{	//shared with q2 dll

	q2pmtype_t	pm_type;

	short		origin[3];		// 12.3
	short		velocity[3];	// 12.3
	qbyte		pm_flags;		// ducked, jump_held, etc
	qbyte		pm_time;		// each unit = 8 ms
	short		gravity;
	short		delta_angles[3];	// add to command angles to get view direction
									// changed by spawns, rotating objects, and teleporters
} q2pmove_state_t;

typedef struct
{	//shared with q2 dll

	q2pmove_state_t	pmove;		// for prediction

	// these fields do not need to be communicated bit-precise

	vec3_t		viewangles;		// for fixed views
	vec3_t		viewoffset;		// add to pmovestate->origin
	vec3_t		kick_angles;	// add to view direction to get render angles
								// set by weapon kicks, pain effects, etc

	vec3_t		gunangles;
	vec3_t		gunoffset;
	int			gunindex;
	int			gunframe;

	float		blend[4];		// rgba full screen effect

	float		fov;			// horizontal field of view

	int			rdflags;		// refdef flags

	short		stats[Q2MAX_STATS];		// fast status bar updates
} q2player_state_t;
#endif

typedef struct colourised_s {
	char name[64];
	unsigned int topcolour;
	unsigned int bottomcolour;
	char skin[64];
	struct colourised_s *next;
} colourised_t;

#define	MAX_SCOREBOARDNAME	64
#define MAX_DISPLAYEDNAME	16
typedef struct player_info_s
{
	int		userid;
	char	userinfo[EXTENDED_INFO_STRING];
	char	teamstatus[128];
	float	teamstatustime;

	// scoreboard information
	char	name[MAX_SCOREBOARDNAME];
	char	team[MAX_INFO_KEY];
	float	entertime;
	int		frags;
	int		ping;
	qbyte	pl;

	qboolean ignored;
	qboolean vignored;

	colourised_t *colourised;

	// skin information
	unsigned int		rtopcolor;	//real, according to their userinfo
	unsigned int		rbottomcolor;

	unsigned int		ttopcolor;	//team, according to colour forcing
	unsigned int		tbottomcolor;

	int		spectator;
	skin_t	*skin;

	struct model_s	*model;

//	unsigned short vweapindex;
	unsigned char h2playerclass;

	int prevcount;

	int stats[MAX_CL_STATS];
	int statsf[MAX_CL_STATS];
} player_info_t;


typedef struct
{
	double		senttime;			// time cmd was sent off

	// generated on client side
	usercmd_t	cmd[MAX_SPLITS];	// cmd that generated the frame
	int			cmd_sequence;
	int			server_message_num;

	int server_time;
	int client_time;
} outframe_t;

typedef struct
{
	//this is the sequence we requested for this frame.
	int			delta_sequence;		// sequence number to delta from, -1 = full update
	float		latency;

	// received from server
	double		receivedtime;	// time message was received, or -1
	player_state_t	playerstate[MAX_CLIENTS+MAX_SPLITS];	// message received that reflects performing
								// the usercmd
	packet_entities_t	packet_entities;
	qboolean	invalid;		// true if the packet_entities delta was invalid
} inframe_t;

#ifdef Q2CLIENT
typedef struct
{
	qboolean		valid;			// cleared if delta parsing was invalid
	int				serverframe;
	int				servertime;		// server time the message is valid for (in msec)
	int				deltaframe;
	qbyte			areabits[MAX_Q2MAP_AREAS/8];		// portalarea visibility bits
	q2player_state_t	playerstate;
	int				num_entities;
	int				parse_entities;	// non-masked index into cl_parse_entities array
} q2frame_t;
#endif

typedef struct
{
	int		destcolor[3];
	float		percent;		// 0-256
} cshift_t;

#define	CSHIFT_CONTENTS	0
#define	CSHIFT_DAMAGE	1
#define	CSHIFT_BONUS	2
#define	CSHIFT_POWERUP	3
#define CSHIFT_SERVER	4
#define	NUM_CSHIFTS		5


//
// client_state_t should hold all pieces of the client state
//
//the light array works thusly:
//dlights are allocated DL_LAST downwards to 0, static wlights are allocated DL_LAST+1 to MAX_RTLIGHTS.
//thus to clear the dlights but not rtlights, set the first light to RTL_FIRST
#define DL_LAST				(sizeof(unsigned int)*8-1)
#define RTL_FIRST			(sizeof(unsigned int)*8)

#define LFLAG_NORMALMODE	(1<<0) /*ppl with r_shadow_realtime_dlight*/
#define LFLAG_REALTIMEMODE	(1<<1) /*ppl with r_shadow_realtime_world*/
#define LFLAG_LIGHTMAP		(1<<2)
#define LFLAG_FLASHBLEND	(1<<3)

#define LFLAG_NOSHADOWS		(1<<8)
#define LFLAG_SHADOWMAP		(1<<9)
#define LFLAG_CREPUSCULAR	(1<<10)

#define LFLAG_DYNAMIC (LFLAG_LIGHTMAP | LFLAG_FLASHBLEND | LFLAG_NORMALMODE | LFLAG_REALTIMEMODE)

typedef struct dlight_s
{
	int		key;				// so entities can reuse same entry
	vec3_t	origin;
	vec3_t	axis[3];
	float	radius;
	float	die;				// stop lighting after this time
	float	decay;				// drop this each second
	float	minlight;			// don't add when contributing less
	float   color[3];
	float	channelfade[3];
	vec3_t lightcolourscales; //ambient, diffuse, specular
	float	corona;
	float	coronascale;

	unsigned int flags;
	char	cubemapname[64];

	//the following are used for rendering (client code should clear on create)
	qboolean rebuildcache;
	struct	shadowmesh_s *worldshadowmesh;
	texid_t cubetexture;
	struct {
		float updatetime;
	} face [6];
	int style;	//multiply by style values if > 0
	float	fov; //spotlight
	struct dlight_s *next;
} dlight_t;

typedef struct
{
	int		length;
	char	map[MAX_STYLESTRING];
	int colour;
} lightstyle_t;



#define	MAX_EFRAGS		512

#define	MAX_DEMOS		8
#define	MAX_DEMONAME	16

typedef enum {
ca_disconnected, 	// full screen console with no connection
ca_demostart,		// starting up a demo
ca_connected,		// netchan_t established, waiting for svc_serverdata
ca_onserver,		// processing data lists, donwloading, etc
ca_active			// everything is in, so frames can be rendered
} cactive_t;

typedef enum {
	dl_none,
	dl_model,
	dl_sound,
	dl_skin,
	dl_wad,
	dl_single,
	dl_singlestuffed
} dltype_t;		// download type

//
// the client_static_t structure is persistant through an arbitrary number
// of server connections
//
typedef struct
{
// connection information
	cactive_t	state;

	/*Specifies which protocol family we're speaking*/
	enum {
		CP_UNKNOWN,
		CP_QUAKEWORLD,
		CP_NETQUAKE,
		CP_QUAKE2,
		CP_QUAKE3,
		CP_PLUGIN
	} protocol;

	/*QuakeWorld protocol flags*/
#ifdef PROTOCOLEXTENSIONS
	unsigned long fteprotocolextensions;
	unsigned long fteprotocolextensions2;
#endif
	unsigned long z_ext;

	/*NQ Protocol flags*/
	enum
	{
		CPNQ_ID,
		CPNQ_PROQUAKE3_4,
		CPNQ_FITZ666, /*and rmqe999 protocol*/
		CPNQ_DP5,
		CPNQ_DP6,
		CPNQ_DP7
	} protocol_nq;
	#define CPNQ_IS_DP (cls.protocol_nq >= CPNQ_DP5)


	qboolean resendinfo;
	qboolean findtrack;

	int framecount;

	int realip_ident;
	netadr_t realserverip;

// network stuff
	netchan_t	netchan;
	float lastarbiatarypackettime;	//used to mark when packets were sent to prevent mvdsv servers from causing us to disconnect.

// private userinfo for sending to masterless servers
	char		userinfo[MAX_SPLITS][EXTENDED_INFO_STRING];

	char		servername[MAX_OSPATH];	// name of server from original connect

	int			qport;

	struct ftenet_connections_s *sockets;

	enum {DL_NONE, DL_QW, DL_QWCHUNKS, DL_Q3, DL_DARKPLACES, DL_QWPENDING, DL_HTTP, DL_FTP} downloadmethod;
	vfsfile_t		*downloadqw;		// file transfer from server
	char			downloadtempname[MAX_OSPATH];	//file its currently writing to.
	char			downloadlocalname[MAX_OSPATH];	//file its going to be renamed to.
	char			downloadremotename[MAX_OSPATH];	//file its coming from.
	float			downloadpercent;	//for progress indicator.
	int				downloadchunknum;	//for QW downloads only.
	float			downloadstarttime;	//for speed info
	unsigned int	downloadedbytes;	//number of bytes downloaded, for progress/speed info

// demo loop control
	int			demonum;		// -1 = don't play demos
	char		demos[MAX_DEMOS][MAX_DEMONAME];		// when not playing

// demo recording info must be here, because record is started before
// entering a map (and clearing client_state_t)
	qboolean	demorecording;
	vfsfile_t	*demooutfile;

	enum{DPB_NONE,DPB_QUAKEWORLD,DPB_MVD,DPB_EZTV,
#ifdef NQPROT
		DPB_NETQUAKE,
#endif
#ifdef Q2CLIENT
		DPB_QUAKE2
#endif
	}	demoplayback;
	qboolean	demoseeking;
	float		demoseektime;
	qboolean	timedemo;
	vfsfile_t	*demoinfile;
	float		td_lastframe;		// to meter out one message a frame
	int			td_startframe;		// host_framecount at start
	float		td_starttime;		// realtime at second frame of timedemo

	int			challenge;

	float		latency;		// rolling average

	qboolean	allow_anyparticles;
	qboolean	allow_lightmapgamma;
	qboolean	allow_rearview;
	qboolean	allow_skyboxes;
	qboolean	allow_mirrors;
	qboolean	allow_watervis;
	qboolean	allow_luma;
	float		allow_fbskins;	//fraction of allowance
	qboolean	allow_postproc;
	qboolean	allow_cheats;
	qboolean	allow_semicheats;	//defaults to true, but this allows a server to enforce a strict ruleset (smackdown type rules).
	float		maxfps;	//server capped
	int			deathmatch;

#ifdef NQPROT
	int signon;
#endif
	translation_t language;

	colourised_t *colourised;
} client_static_t;

extern client_static_t	cls;

typedef struct downloadlist_s {
	char rname[128];
	char localname[128];
	unsigned int size;
	unsigned int flags;
#define DLLF_VERBOSE 1			//tell the user that its downloading
#define DLLF_REQUIRED 2			//means that it won't load models etc until its downloaded (ie: requiredownloads 0 makes no difference)
#define DLLF_OVERWRITE 4		//overwrite it even if it already exists
#define DLLF_SIZEUNKNOWN 8		//download's size isn't known
#define DLLF_IGNOREFAILED 16	//
#define DLLF_NONGAME 32			//means the requested download filename+localname is gamedir explicit (so id1/foo.txt is distinct from qw/foo.txt)
#define DLLF_TEMPORARY 64		//download it, but don't actually save it (DLLF_OVERWRITE doesn't actually overwrite, but does ignore any local files)
	struct downloadlist_s *next;
} downloadlist_t;


typedef struct {
	//current persistant state
	trailstate_t *trailstate;	//when to next throw out a trail
	trailstate_t *emitstate;    //when to next emit

	//current origin
	vec3_t origin;	//current render position
	vec3_t angles;

	//previous rendering frame (for trails)
	vec3_t lastorigin;
	qboolean isnew;
	qboolean isplayer;

	//intermediate values for frame lerping
	float framelerpdeltatime;
	float newframestarttime;
	int newframe;
	float oldframestarttime;
	int oldframe;

	//intermediate values for origin lerping of stepping things
	float orglerpdeltatime;
	float orglerpstarttime;
	vec3_t neworigin; /*origin that we're lerping towards*/
	vec3_t oldorigin; /*origin that we're lerping away from*/
	vec3_t newangle;
	vec3_t oldangle;

	//for further info
	int skeletalobject;
	int sequence;	/*so csqc code knows that the ent is still valid*/
	entity_state_t *entstate;
} lerpents_t;
//
// the client_state_t structure is wiped completely at every
// server signon
//
typedef struct
{
	int			fpd;
	int			servercount;	// server identification for prespawns

	float		gamespeed;
	qboolean	csqcdebug;
	qboolean	allowsendpacket;

	char		serverinfo[MAX_SERVERINFO_STRING];
	char		serverpaknames[1024];
	char		serverpakcrcs[1024];
	qboolean	serverpakschanged;

	int			parsecount;		// server message counter
	int			oldparsecount;
	int			oldvalidsequence;
	int			ackedmovesequence;	//in quakeworld/q2 this is always equal to validsequence. nq can differ. may only be updated when validsequence is updated.
	int			validsequence;	// this is the sequence number of the last good
								// packetentity_t we got.  If this is 0, we can't
								// render a frame yet
	int			movesequence;	// 

	int			spectator;

	double		last_ping_request;	// while showing scoreboard
	double		last_servermessage;

	//list of ent frames that still need to be acked.
	int numackframes;
	int ackframes[64];

#ifdef Q2CLIENT
	q2frame_t	q2frame;
	q2frame_t	q2frames[Q2UPDATE_BACKUP];
#endif

// sentcmds[cl.netchan.outgoing_sequence & UPDATE_MASK] = cmd
	outframe_t	outframes[UPDATE_BACKUP];	//user inputs (cl.ackedmovesequence+1 to cl.movesequence are still pending)
	inframe_t	inframes[UPDATE_BACKUP];	//server state (cl.validsequence is the most recent set)
	lerpents_t	*lerpents;
	int			maxlerpents;	//number of slots allocated.
	int			lerpentssequence;
	lerpents_t	lerpplayers[MAX_CLIENTS];

	cshift_t	cshifts[NUM_CSHIFTS];	// color shifts for damage, powerups and content types

	//when running splitscreen, we have multiple viewports all active at once
	int			splitclients;	//we are running this many clients split screen.
	struct playerview_s
	{
		// information for local display
		int			stats[MAX_CL_STATS];	// health, etc
		float		statsf[MAX_CL_STATS];	// health, etc
		char		*statsstr[MAX_CL_STATS];	// health, etc
		float		item_gettime[32];	// cl.time of aquiring item, for blinking
		float		faceanimtime;		// use anim frame if cl.time < this


	// the client maintains its own idea of view angles, which are
	// sent to the server each frame.  And only reset at level change
	// and teleport times
		vec3_t		viewangles;
		vec3_t		viewanglechange;
		vec3_t		gravitydir;

		// pitch drifting vars
		float		pitchvel;
		qboolean	nodrift;
		float		driftmove;
		double		laststop;

		vec3_t		simorg;
		vec3_t		simvel;
		vec3_t		simangles;
		float		rollangle;

		qboolean	fixangle;		//received a fixangle - so disable prediction till the next packet.
		qboolean	oldfixangle;	//received a fixangle - so disable prediction till the next packet.
		vec3_t		fixangles;		//received a fixangle - so disable prediction till the next packet.
		vec3_t		oldfixangles;	//received a fixangle - so disable prediction till the next packet.
	} playerview[MAX_SPLITS];

	float		crouch[MAX_SPLITS];			// local amount for smoothing stepups
	qboolean	onground[MAX_SPLITS];
	float		viewheight[MAX_SPLITS];

	entity_t	viewent[MAX_SPLITS];		// weapon model
	float		punchangle[MAX_SPLITS];		// temporary view kick from weapon firing

	int			playernum[MAX_SPLITS];
	qboolean	nolocalplayer[MAX_SPLITS];

	#ifdef PEXT_SETVIEW
	int viewentity[MAX_SPLITS];
	#endif
	int waterlevel[MAX_SPLITS];	//for smartjump

	// localized movement vars
	float		entgravity[MAX_SPLITS];
	float		maxspeed[MAX_SPLITS];
	float		bunnyspeedcap;
	int pmovetype[MAX_SPLITS];

// the client simulates or interpolates movement to get these values
	double		time;			// this is the time value that the client
								// is rendering at.  always <= realtime

	float servertime;	//current server time, bound between gametime and gametimemark
	float mtime;		//server time as on the server when we last received a packet. not allowed to decrease.
	float oldmtime;		//server time as on the server for the previously received packet.

	float gametime;
	float gametimemark;
	float oldgametime;		//used as the old time to lerp cl.time from.
	float oldgametimemark;	//if it's 0, cl.time will casually increase.

	float minpitch;
	float maxpitch;

	qboolean	paused;			// send over by server

	int			intermission;	// don't change view angle, full screen, etc
	float		completed_time;	// latched ffrom time at intermission start

#define Q2MAX_VISIBLE_WEAPONS 32 //q2 has about 20.
	int		numq2visibleweapons;	//q2 sends out visible-on-model weapons in a wierd gender-nutral way.
	char	*q2visibleweapons[Q2MAX_VISIBLE_WEAPONS];//model names beginning with a # are considered 'sexed', and are loaded on a per-client basis. yay. :(

//
// information that is static for the entire time connected to a server
//
	char		model_name_vwep[MAX_VWEP_MODELS][MAX_QPATH];
	char		model_name[MAX_MODELS][MAX_QPATH];
	char		sound_name[MAX_SOUNDS][MAX_QPATH];
	char		*particle_ssname[MAX_SSPARTICLESPRE];
	char		image_name[Q2MAX_IMAGES][MAX_QPATH];

	struct model_s		*model_precache_vwep[MAX_VWEP_MODELS];
	struct model_s		*model_precache[MAX_MODELS];
	struct sfx_s		*sound_precache[MAX_SOUNDS];
	int					particle_ssprecache[MAX_SSPARTICLESPRE];	//these are actually 1-based, so 0 can be used to lazy-init them. I cheat.

	char				model_csqcname[MAX_CSMODELS][MAX_QPATH];
	struct model_s		*model_csqcprecache[MAX_CSMODELS];
	char				*particle_csname[MAX_CSPARTICLESPRE];
	int					particle_csprecache[MAX_CSPARTICLESPRE];	//these are actually 1-based, so we can be lazy and do a simple negate.

	qboolean			model_precaches_added;
	qboolean			particle_ssprecaches;	//says to not try to do any dp-compat hacks.
	qboolean			particle_csprecaches;	//says to not try to do any dp-compat hacks.

	//used for q2 sky/configstrings
	char skyname[MAX_QPATH];
	float skyrotate;
	vec3_t skyaxis;

	qboolean	fog_locked;
	float		fog_density;
	vec3_t		fog_colour;

	char		levelname[40];	// for display on solo scoreboard

// refresh related state
	struct model_s	*worldmodel;	// cl_entitites[0].model
	int			num_entities;	// stored bottom up in cl_entities array
	int			num_statics;	// stored top down in cl_entitiers

	int			cdtrack;		// cd audio

// all player information
	unsigned int    allocated_client_slots;
	player_info_t	players[MAX_CLIENTS];


	downloadlist_t *downloadlist;
	downloadlist_t *faileddownloads;

	qboolean gamedirchanged;

	char		q2statusbar[1024];
	char		q2layout[1024];
	int parse_entities;
	float lerpfrac;
	vec3_t predicted_origin;
	vec3_t predicted_angles;
	vec3_t prediction_error;
	float predicted_step_time;
	float predicted_step;

	packet_entities_t	*currentpackentities;
	float				currentpacktime;


	int teamplay;
	int deathmatch;

	qboolean teamfortress;	// *sigh*. This is used for teamplay stuff. This sucks.
	qboolean hexen2pickups;

	qboolean sendprespawn;
	int contentstage;

	double matchgametime;
	enum {
		MATCH_DONTKNOW,
		MATCH_COUNTDOWN,
		MATCH_STANDBY,
		MATCH_INPROGRESS
	} matchstate;
} client_state_t;

extern unsigned int		cl_teamtopcolor;
extern unsigned int		cl_teambottomcolor;
extern unsigned int		cl_enemytopcolor;
extern unsigned int		cl_enemybottomcolor;

//FPD values
//(commented out ones are ones that we don't support)
#define FPD_NO_FORCE_SKIN	256
#define FPD_NO_FORCE_COLOR	512
#define FPD_LIMIT_PITCH		(1 << 14)	//limit scripted pitch changes
#define FPD_LIMIT_YAW		(1 << 15)	//limit scripted yaw changes

//
// cvars
//
extern  cvar_t	cl_warncmd;
extern	cvar_t	cl_upspeed;
extern	cvar_t	cl_forwardspeed;
extern	cvar_t	cl_backspeed;
extern	cvar_t	cl_sidespeed;

extern	cvar_t	cl_movespeedkey;

extern	cvar_t	cl_yawspeed;
extern	cvar_t	cl_pitchspeed;

extern	cvar_t	cl_anglespeedkey;

extern	cvar_t	cl_shownet;
extern	cvar_t	cl_sbar;
extern	cvar_t	cl_hudswap;

extern	cvar_t	cl_pitchdriftspeed;
extern	cvar_t	lookspring;
extern	cvar_t	lookstrafe;
extern	cvar_t	sensitivity;

extern	cvar_t	m_pitch;
extern	cvar_t	m_yaw;
extern	cvar_t	m_forward;
extern	cvar_t	m_side;

extern cvar_t		_windowed_mouse;

extern	cvar_t	name;


extern cvar_t ruleset_allow_playercount;
extern cvar_t ruleset_allow_frj;
extern cvar_t ruleset_allow_semicheats;
extern cvar_t ruleset_allow_packet;
extern cvar_t ruleset_allow_particle_lightning;
extern cvar_t ruleset_allow_overlongsounds;
extern cvar_t ruleset_allow_larger_models;
extern cvar_t ruleset_allow_modified_eyes;
extern cvar_t ruleset_allow_sensative_texture_replacements;
extern cvar_t ruleset_allow_localvolume;
extern cvar_t ruleset_allow_shaders;

#ifndef SERVERONLY
extern	client_state_t	cl;
#endif

typedef struct
{
	entity_t		ent;
	trailstate_t   *emit;
	int	mdlidx;	/*negative are csqc indexes*/
	pvscache_t		pvscache;
} static_entity_t;

// FIXME, allocate dynamically
extern	entity_state_t *cl_baselines;
extern	static_entity_t		*cl_static_entities;
extern  unsigned int    cl_max_static_entities;
extern	lightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
extern	dlight_t		*cl_dlights;
extern	unsigned int	cl_maxdlights;

extern int rtlights_first, rtlights_max;
extern int cl_baselines_count;

extern	qboolean	nomaster;
extern float	server_version;	// version of server we connected to

//=============================================================================


//
// cl_main
//
void CL_InitDlights(void);
void CL_FreeDlights(void);
dlight_t *CL_AllocDlight (int key);
dlight_t *CL_AllocSlight (void);	//allocates a static light
dlight_t *CL_NewDlight (int key, const vec3_t origin, float radius, float time, float r, float g, float b);
dlight_t *CL_NewDlightRGB (int key, const vec3_t origin, float radius, float time, float r, float g, float b);
dlight_t *CL_NewDlightCube (int key, const vec3_t origin, vec3_t angles, float radius, float time, vec3_t colours);
void	CL_DecayLights (void);

void CLQW_ParseDelta (struct entity_state_s *from, struct entity_state_s *to, int bits, qboolean);

void CL_Init (void);
void Host_WriteConfiguration (void);
void CL_CheckServerInfo(void);
void CL_CheckServerPacks(void);

void CL_EstablishConnection (char *host);

void CL_Disconnect (void);
void CL_Disconnect_f (void);
void CL_Reconnect_f (void);
void CL_NextDemo (void);
void CL_Startdemos_f (void);
void CL_Demos_f (void);
void CL_Stopdemo_f (void);
void CL_Changing_f (void);
void CL_Reconnect_f (void);
void CL_ConnectionlessPacket (void);
qboolean CL_DemoBehind(void);
void CL_SaveInfo(vfsfile_t *f);
void CL_SetInfo (int pnum, char *key, char *value);

void CL_BeginServerConnect(int port);
char *CL_TryingToConnect(void);

void CL_ExecInitialConfigs(void);
qboolean CL_CheckBootDownloads(void);

extern	int				cl_numvisedicts;
extern	int				cl_maxvisedicts;
extern	entity_t		*cl_visedicts;

/*these are for q3 really*/
typedef struct {
	struct shader_s *shader;
	int firstvert;
	int firstidx;
	int numvert;
	int numidx;
	unsigned int flags;
} scenetris_t;
extern scenetris_t		*cl_stris;
extern vecV_t			*cl_strisvertv;
extern vec4_t			*cl_strisvertc;
extern vec2_t			*cl_strisvertt;
extern index_t			*cl_strisidx;
extern unsigned int cl_numstrisidx;
extern unsigned int cl_maxstrisidx;
extern unsigned int cl_numstrisvert;
extern unsigned int cl_maxstrisvert;
extern unsigned int cl_numstris;
extern unsigned int cl_maxstris;

extern char emodel_name[], pmodel_name[], prespawn_name[], modellist_name[], soundlist_name[];

qboolean TraceLineN (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal);
qboolean Q2TraceLineN (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal);

//
// cl_input
//
typedef struct
{
	int		down[MAX_SPLITS][2];		// key nums holding it down
	int		state[MAX_SPLITS];			// low bit is down state
} kbutton_t;

extern	kbutton_t	in_mlook, in_klook;
extern 	kbutton_t 	in_strafe;
extern 	kbutton_t 	in_speed;

extern	float in_sensitivityscale;

void CL_MakeActive(char *gamename);
void CL_UpdateWindowTitle(void);

void CL_InitInput (void);
void CL_SendCmd (double frametime, qboolean mainloop);
void CL_SendMove (usercmd_t *cmd);
#ifdef NQPROT
void CL_ParseTEnt (qboolean nqprot);
#else
void CL_ParseTEnt (void);
#endif
void CL_UpdateTEnts (void);
void CL_AddBeam (int tent, int ent, vec3_t start, vec3_t end);

void CL_ClearState (void);
void CLQ2_ClearState(void);

void CL_ReadPackets (void);
void CL_ClampPitch (int pnum);

int  CL_ReadFromServer (void);
void CL_WriteToServer (usercmd_t *cmd);
void CL_BaseMove (usercmd_t *cmd, int pnum, float extra, float wantfps);


float CL_KeyState (kbutton_t *key, int pnum);
char *Key_KeynumToString (int keynum);
int Key_StringToKeynum (char *str, int *modifier);
char *Key_GetBinding(int keynum);

void CL_UseIndepPhysics(qboolean allow);

void CL_FlushClientCommands(void);
void VARGS CL_SendClientCommand(qboolean reliable, char *format, ...) LIKEPRINTF(2);
float CL_FilterTime (double time, float wantfps, qboolean ignoreserver);
int CL_RemoveClientCommands(char *command);
void CL_AllowIndependantSendCmd(qboolean allow);

void CL_DrawPrydonCursor(void);

//
// cl_demo.c
//
void CL_StopPlayback (void);
qboolean CL_GetMessage (void);
void CL_WriteDemoCmd (usercmd_t *pcmd);
void CL_Demo_ClientCommand(char *commandtext);	//for QTV.

void CL_Stop_f (void);
void CL_Record_f (void);
void CL_ReRecord_f (void);
void CL_PlayDemo_f (void);
void CL_QTVPlay_f (void);
void CL_QTVPoll (void);
void CL_QTVList_f (void);
void CL_QTVDemos_f (void);
void CL_DemoJump_f(void);
void CL_ProgressDemoTime(void);
void CL_TimeDemo_f (void);
typedef struct 
{
	enum
	{
		QTVCT_NONE,
		QTVCT_STREAM,
		QTVCT_CONNECT,
		QTVCT_JOIN,
		QTVCT_OBSERVE,
		QTVCT_MAP
	} connectiontype;
	enum
	{
		QTVCT_NETQUAKE,
		QTVCT_QUAKEWORLD,
		QTVCT_QUAKE2,
		QTVCT_QUAKE3
	} protocol;
	char server[256];
	char splashscreen[256];
	//char *datafiles;
} qtvfile_t;
void CL_ParseQTVFile(vfsfile_t *f, const char *fname, qtvfile_t *result);

//
// cl_parse.c
//
#define NET_TIMINGS 256
#define NET_TIMINGSMASK 255
extern int	packet_latency[NET_TIMINGS];
int CL_CalcNet (void);
void CL_ClearParseState(void);
void CL_Parse_Disconnected(void);
void CL_DumpPacket(void);
void CL_ParseEstablished(void);
void CLQW_ParseServerMessage (void);
void CLNQ_ParseServerMessage (void);
#ifdef Q2CLIENT
void CLQ2_ParseServerMessage (void);
#endif
void CL_NewTranslation (int slot);

qboolean CL_CheckOrEnqueDownloadFile (char *filename, char *localname, unsigned int flags);
qboolean CL_EnqueDownload(char *filename, char *localname, unsigned int flags);
downloadlist_t *CL_DownloadFailed(char *name, qboolean cancel);
int CL_DownloadRate(void);
void CL_GetDownloadSizes(unsigned int *filecount, unsigned int *totalsize, qboolean *somesizesunknown);
qboolean CL_ParseOOBDownload(void);
void CL_DownloadFinished(void);
void CL_RequestNextDownload (void);
void CL_SendDownloadReq(sizebuf_t *msg);
void Sound_CheckDownload(char *s); /*checkorenqueue a sound file*/

qboolean CL_IsUploading(void);
void CL_NextUpload(void);
void CL_StartUpload (qbyte *data, int size);
void CL_StopUpload(void);

qboolean CL_CheckBaselines (int size);

//
// view.c
//
void V_StartPitchDrift (int pnum);
void V_StopPitchDrift (int pnum);

void V_RenderView (void);
void V_Register (void);
void V_ParseDamage (int pnum);
void V_SetContentsColor (int contents);

//used directly by csqc
void V_CalcRefdef (int pnum);
void V_CalcGunPositionAngle (int pnum, float bob);
float V_CalcBob (int pnum, qboolean queryold);
void DropPunchAngle (int pnum);


//
// cl_tent
//
void CL_RegisterParticles(void);
void CL_InitTEnts (void);
void CL_InitTEntSounds (void);
void CL_ClearTEnts (void);
void CL_ClearTEntParticleState (void);
void CL_ClearCustomTEnts(void);
void CL_ParseCustomTEnt(void);
void CL_ParseEffect (qboolean effect2);

void CLNQ_ParseParticleEffect (void);
void CL_ParseParticleEffect2 (void);
void CL_ParseParticleEffect3 (void);
void CL_ParseParticleEffect4 (void);

int CL_TranslateParticleFromServer(int sveffect);
void CL_ParseTrailParticles(void);
void CL_ParsePointParticles(qboolean compact);
void CL_SpawnSpriteEffect(vec3_t org, vec3_t dir, struct model_s *model, int startframe, int framecount, float framerate, float alpha, float randspin, float gravity);	/*called from the particlesystem*/

//
// cl_ents.c
//
void CL_SetSolidPlayers (void);
void CL_SetUpPlayerPrediction(qboolean dopred);
void CL_LinkStaticEntities(void *pvs);
void CL_TransitionEntities (void); /*call at the start of the frame*/
void CL_EmitEntities (void);
void CL_ClearProjectiles (void);
void CL_ParseProjectiles (int modelindex, qboolean nails2);
void CLQW_ParsePacketEntities (qboolean delta);
void CLFTE_ParseEntities (void);
void CLFTE_ParseBaseline(entity_state_t *es, qboolean numberisimportant);
void CL_SetSolidEntities (void);
void CL_ParsePlayerinfo (void);
void CL_ParseClientPersist(void);
//these last ones are needed for csqc handling of engine-bound ents.
void CL_ClearEntityLists(void);
void CL_FreeVisEdicts(void);
void CL_LinkViewModel(void);
void CL_LinkPlayers (void);
void CL_LinkPacketEntities (void);
void CL_LinkProjectiles (void);
void CL_ClearLerpEntsParticleState (void);
qboolean CL_MayLerp(void);

//
//clq3_parse.c
//
#ifdef Q3CLIENT
void VARGS CLQ3_SendClientCommand(const char *fmt, ...) LIKEPRINTF(1);
void CLQ3_SendAuthPacket(netadr_t gameserver);
void CLQ3_SendConnectPacket(netadr_t to);
void CLQ3_SendCmd(usercmd_t *cmd);
qboolean CLQ3_Netchan_Process(void);
void CLQ3_ParseServerMessage (void);
struct snapshot_s;
qboolean CG_FillQ3Snapshot(int snapnum, struct snapshot_s *snapshot);

void CG_InsertIntoGameState(int num, char *str);
void CG_Restart_f(void);

char *CG_GetConfigString(int num);
#endif

//
//pr_csqc.c
//
#ifdef CSQC_DAT
qboolean CSQC_Inited(void);
void CSQC_RendererRestarted(void);
qboolean CSQC_UnconnectedOkay(qboolean inprinciple);
qboolean CSQC_UnconnectedInit(void);
qboolean CSQC_Init (qboolean anycsqc, qboolean csdatenabled, unsigned int checksum);
qboolean CSQC_ConsoleLink(char *text, char *info);
void CSQC_RegisterCvarsAndThings(void);
qboolean CSQC_DrawView(void);
void CSQC_Shutdown(void);
qboolean CSQC_StuffCmd(int lplayernum, char *cmd, char *cmdend);
qboolean CSQC_LoadResource(char *resname, char *restype);
qboolean CSQC_ParsePrint(char *message, int printlevel);
qboolean CSQC_ParseGamePacket(void);
qboolean CSQC_CenterPrint(int lplayernum, char *cmd);
void CSQC_Input_Frame(int lplayernum, usercmd_t *cmd);
void CSQC_WorldLoaded(void);
qboolean CSQC_ParseTempEntity(unsigned char firstbyte);
qboolean CSQC_ConsoleCommand(char *cmd);
qboolean CSQC_KeyPress(int key, int unicode, qboolean down, int devid);
qboolean CSQC_MouseMove(float xdelta, float ydelta, int devid);
qboolean CSQC_MousePosition(float xabs, float yabs, int devid);
qboolean CSQC_Accelerometer(float x, float y, float z);
int CSQC_StartSound(int entnum, int channel, char *soundname, vec3_t pos, float vol, float attenuation, float pitchmod);
void CSQC_ParseEntities(void);
qboolean CSQC_SettingListener(void);

qboolean CSQC_DeltaPlayer(int playernum, player_state_t *state);
void CSQC_DeltaStart(float time);
qboolean CSQC_DeltaUpdate(entity_state_t *src);
void CSQC_DeltaEnd(void);

void CSQC_CvarChanged(cvar_t *var);
#else
#define CSQC_UnconnectedOkay(inprinciple) false
#define CSQC_UnconnectedInit() false
#endif

//
// cl_pred.c
//
void CL_InitPrediction (void);
void CL_PredictMove (void);
void CL_PredictUsercmd (int pnum, int entnum, player_state_t *from, player_state_t *to, usercmd_t *u);
#ifdef Q2CLIENT
void CLQ2_CheckPredictionError (void);
#endif
void CL_CalcClientTime(void);

//
// cl_cam.c
//
#define CAM_NONE	0
#define CAM_TRACK	1

extern	int		autocam[MAX_SPLITS];
extern int spec_track[MAX_SPLITS]; // player# of who we are tracking

qboolean Cam_DrawViewModel(int pnum);
qboolean Cam_DrawPlayer(int pnum, int playernum);
int Cam_TrackNum(int pnum);
void Cam_Unlock(int pnum);
void Cam_Lock(int pnum, int playernum);
void Cam_SelfTrack(int pnum);
void Cam_Track(int pnum, usercmd_t *cmd);
void Cam_TrackCrosshairedPlayer(int pnum);
void Cam_SetAutoTrack(int userid);
void Cam_FinishMove(int pnum, usercmd_t *cmd);
void Cam_Reset(void);
void Cam_TrackPlayer(int pnum, char *cmdname, char *plrarg);
void Cam_Lock(int pnum, int playernum);
void CL_InitCam(void);

void QDECL vectoangles(vec3_t fwd, vec3_t ang);

//
//zqtp.c
//
#define TPM_UNKNOWN    0
#define TPM_NORMAL     1
#define TPM_TEAM       2
#define TPM_SPECTATOR  4
#define TPM_FAKED     16
#define TPM_OBSERVEDTEAM  32

void		CL_Say (qboolean team, char *extra);
int			TP_CategorizeMessage (char *s, int *offset, player_info_t **plr);
void		TP_CheckPickupSound(char *s, vec3_t org);
qboolean	TP_CheckSoundTrigger (char *str);
int			TP_CountPlayers (void);
char*		TP_EnemyName (void);
char*		TP_EnemyTeam (void);
void		TP_ExecTrigger (char *s);
qboolean	TP_FilterMessage (char *s);
void		TP_Init(void);
char*		TP_LocationName (vec3_t location);
char*		TP_MapName (void);
void		TP_NewMap (void);
void		TP_ParsePlayerInfo(player_state_t *oldstate, player_state_t *state, player_info_t *info);
qboolean	TP_IsPlayerVisible(vec3_t origin);
char*		TP_PlayerName (void);
char*		TP_PlayerTeam (void);
void		TP_SearchForMsgTriggers (char *s, int level);
qboolean	TP_SoundTrigger(char *message);
void		TP_StatChanged (int stat, int value);
qboolean	TP_SuppressMessage(char *buf);
colourised_t *TP_FindColours(char *name);
void		TP_UpdateAutoStatus(void);

//
// skin.c
//

typedef struct
{
    char	manufacturer;
    char	version;
    char	encoding;
    char	bits_per_pixel;
    unsigned short	xmin,ymin,xmax,ymax;
    unsigned short	hres,vres;
    unsigned char	palette[48];
    char	reserved;
    char	color_planes;
    unsigned short	bytes_per_line;
    unsigned short	palette_type;
    char	filler[58];
//    unsigned char	data;			// unbounded
} pcx_t;
qbyte *ReadPCXData(qbyte *buf, int length, int width, int height, qbyte *result);


char *Skin_FindName (player_info_t *sc);
void	Skin_Find (player_info_t *sc);
qbyte	*Skin_Cache8 (skin_t *skin);
qbyte	*Skin_Cache32 (skin_t *skin);
void	Skin_Skins_f (void);
void	Skin_FlushSkin(char *name);
void	Skin_AllSkins_f (void);
void	Skin_NextDownload (void);
void Skin_FlushPlayers(void);
void Skin_FlushAll(void);

#define RSSHOT_WIDTH 320
#define RSSHOT_HEIGHT 200





//valid.c
void	RulesetLatch(cvar_t *cvar);
void	Validation_Apply_Ruleset(void);
void	Validation_FlushFileList(void);
void	Validation_CheckIfResponse(char *text);
void	Validation_DelatchRulesets(void);
void	InitValidation(void);
void	Validation_IncludeFile(char *filename, char *file, int filelen);
void	Validation_Auto_Response(int playernum, char *s);

extern	qboolean f_modified_particles;
extern	qboolean care_f_modified;


//random files (fixme: clean up)

#ifdef Q2CLIENT
void CLQ2_ParseTEnt (void);
void CLQ2_AddEntities (void);
void CLQ2_ParseBaseline (void);
void CLQ2_ParseFrame (void);
void CLQ2_RunMuzzleFlash2 (int ent, int flash_number);
int CLQ2_RegisterTEntModels (void);
#endif

#ifdef HLCLIENT
//networking
void CLHL_LoadClientGame(void);
int CLHL_ParseGamePacket(void);
int CLHL_AnimateViewEntity(entity_t *ent);
//screen
int CLHL_DrawHud(void);
//inputs
int CLHL_GamecodeDoesMouse(void);
int CLHL_MouseEvent(unsigned int buttonmask);
void CLHL_SetMouseActive(int activate);
int CLHL_BuildUserInput(int msecs, usercmd_t *cmd);
#endif

#ifdef NQPROT
void CLNQ_ParseEntity(unsigned int bits);
void NQ_P_ParseParticleEffect (void);
void CLNQ_SignonReply (void);
void NQ_BeginConnect(char *to);
void NQ_ContinueConnect(char *to);
int CLNQ_GetMessage (void);
#endif

void CL_BeginServerReconnect(void);

void SV_User_f (void);	//called by client version of the function
void SV_Serverinfo_f (void);



#ifdef TEXTEDITOR
extern qboolean editoractive;
extern qboolean editormodal;
void Editor_Draw(void);
void Editor_Init(void);
struct pubprogfuncs_s;
void Editor_ProgsKilled(struct pubprogfuncs_s *dead);
#endif

void SCR_StringToRGB (char *rgbstring, float *rgb, float rgbinputscale);

struct model_s;
void CL_AddVWeapModel(entity_t *player, struct model_s *model);

/*q2 cinematics*/
struct cinematics_s;
void CIN_StopCinematic (struct cinematics_s *cin);
struct cinematics_s *CIN_PlayCinematic (char *arg);
int CIN_RunCinematic (struct cinematics_s *cin, qbyte **outdata, int *outwidth, int *outheight, qbyte **outpalette);

typedef struct cin_s cin_t;
#ifdef NOMEDIA
#define Media_Playing() false
#define Media_Init() 0
#define Media_PlayingFullScreen() false
#define Media_PlayFilm(n) false
#else
/*media playing system*/
qboolean Media_PlayingFullScreen(void);
void Media_Init(void);
qboolean Media_PlayFilm(char *name);
qboolean Media_Playing(void);
struct cin_s *Media_StartCin(char *name);
texid_tf Media_UpdateForShader(cin_t *cin);
void Media_ShutdownCin(cin_t *cin);
qboolean Media_FakeTrack(int i, qboolean loop);
#endif

//these accept NULL for cin to mean the current fullscreen video
void Media_Send_Command(cin_t *cin, char *command);
void Media_Send_MouseMove(cin_t *cin, float x, float y);
void Media_Send_Resize(cin_t *cin, int x, int y);
void Media_Send_GetSize(cin_t *cin, int *x, int *y);
void Media_Send_KeyEvent(cin_t *cin, int button, int unicode, int event);

void MVD_Interpolate(void);

int Stats_GetKills(int playernum);
int Stats_GetTKills(int playernum);
int Stats_GetDeaths(int playernum);
int Stats_GetTouches(int playernum);
int Stats_GetCaptures(int playernum);
qboolean Stats_HaveFlags(void);
qboolean Stats_HaveKills(void);
void VARGS Stats_Message(char *msg, ...) LIKEPRINTF(1);
int qm_strcmp(char *s1, char *s2);
int qm_stricmp(char *s1, char *s2);
void Stats_ParsePrintLine(char *line);
void Stats_NewMap(void);

enum uploadfmt;
typedef struct
{
	void *(VARGS *createdecoder)(char *name);
	void *(VARGS *decodeframe)(void *ctx, qboolean nosound, enum uploadfmt *fmt, int *width, int *height);
	void (VARGS *doneframe)(void *ctx, void *img);
	void (VARGS *shutdown)(void *ctx);
	void (VARGS *rewind)(void *ctx);

	//these are any interactivity functions you might want...
	void (VARGS *cursormove) (void *ctx, float posx, float posy);	//pos is 0-1
	void (VARGS *key) (void *ctx, int code, int unicode, int event);
	qboolean (VARGS *setsize) (void *ctx, int width, int height);
	void (VARGS *getsize) (void *ctx, int *width, int *height);
	void (VARGS *changestream) (void *ctx, char *streamname);
} media_decoder_funcs_t;
typedef struct {
	void *(VARGS *capture_begin) (char *streamname, int videorate, int width, int height, int *sndkhz, int *sndchannels, int *sndbits);
	void (VARGS *capture_video) (void *ctx, void *data, int frame, int width, int height);
	void (VARGS *capture_audio) (void *ctx, void *data, int bytes);
	void (VARGS *capture_end) (void *ctx);
} media_encoder_funcs_t;
extern struct plugin_s *currentplug;
qboolean Media_RegisterDecoder(struct plugin_s *plug, media_decoder_funcs_t *funcs);
qboolean Media_UnregisterDecoder(struct plugin_s *plug, media_decoder_funcs_t *funcs);
qboolean Media_RegisterEncoder(struct plugin_s *plug, media_encoder_funcs_t *funcs);
qboolean Media_UnregisterEncoder(struct plugin_s *plug, media_encoder_funcs_t *funcs);
