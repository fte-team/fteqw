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
// protocol.h -- communications protocols
#define PEXT_SETVIEW			0x00000001

#define PEXT_SCALE				0x00000002
#define PEXT_LIGHTSTYLECOL		0x00000004
#define PEXT_TRANS				0x00000008
#ifdef SIDEVIEWS
	#define PEXT_VIEW2			0x00000010
#endif
#define PEXT_BULLETENS			0x00000020
#ifdef AVAIL_ZLIB
//	#define PEXT_ZLIBDL			0x00000040
#endif
//#define PEXT_LIGHTUPDATES		0x00000080	//send progs/zap.mdl in the same mannor as a nail.
#define PEXT_FATNESS			0x00000100	//GL only (or servers)
#define PEXT_HLBSP				0x00000200
#define PEXT_TE_BULLET			0x00000400
#define PEXT_HULLSIZE			0x00000800
#define PEXT_MODELDBL			0x00001000
#define PEXT_ENTITYDBL			0x00002000	//max of 1024 ents instead of 512
#define PEXT_ENTITYDBL2			0x00004000	//max of 1024 ents instead of 512
//#define PEXT_ORIGINDBL			0x00008000	//max of +-8192 instead of +-4096 of map origins (Achieved by *2 on origins in writecoord)
#define PEXT_VWEAP				0x00010000	//cause an extra qbyte to be sent, and an extra list of models for vweaps.
#ifdef Q2BSPS
#define PEXT_Q2BSP				0x00020000
#endif
#ifdef Q3BSPS
#define PEXT_Q3BSP				0x00040000
#endif
#define PEXT_SEEF1				0x00080000
#define PEXT_SPLITSCREEN		0x00100000
#define PEXT_HEXEN2				0x00200000
#define PEXT_SPAWNSTATIC2		0x00400000	//Sends an entity delta instead of a baseline.
#define PEXT_CUSTOMTEMPEFFECTS	0x00800000	//supports custom temp ents.
#define PEXT_256PACKETENTITIES	0x01000000	//Client can recieve 256 packet entities.
//#define PEXT_64PLAYERS			0x02000000	//Client is able to cope with 64 players. Wow.
#define PEXT_SHOWPIC			0x04000000



//ZQuake transparent protocol extensions.
#define Z_EXT_PM_TYPE		(1<<0)	// basic PM_TYPE functionality (reliable jump_held)
#define Z_EXT_PM_TYPE_NEW	(1<<1)	// adds PM_FLY, PM_SPECTATOR
#define Z_EXT_VIEWHEIGHT	(1<<2)	// STAT_VIEWHEIGHT
#define Z_EXT_SERVERTIME	(1<<3)	// STAT_TIME
#define Z_EXT_PITCHLIMITS	(1<<4)	// serverinfo maxpitch & minpitch
#define Z_EXT_JOIN_OBSERVE	(1<<5)	// server: "join" and "observe" commands are supported
									// client: on-the-fly spectator <-> player switching supported
#define SUPPORTED_EXTENSIONS (Z_EXT_PM_TYPE|Z_EXT_PM_TYPE_NEW|Z_EXT_VIEWHEIGHT|Z_EXT_SERVERTIME|Z_EXT_PITCHLIMITS|Z_EXT_JOIN_OBSERVE)


#define PROTOCOL_VERSION_FTE			(('F'<<0) + ('T'<<8) + ('E'<<16) + ('X' << 24))	//fte extensions.
#define PROTOCOL_VERSION_HUFFMAN		(('H'<<0) + ('U'<<8) + ('F'<<16) + ('F' << 24))	//packet compression

#define	PROTOCOL_VERSION	28
#define	PROTOCOL_VERSION_Q2_MIN	31
#define	PROTOCOL_VERSION_Q2	34

//=========================================

#define	PORT_CLIENT	27001
#define	PORT_MASTER	27000
#define	PORT_SERVER	27500
#define Q2PORT_CLIENT 27901
#define Q2PORT_SERVER 27910

//=========================================

// out of band message id bytes

// M = master, S = server, C = client, A = any
// the second character will allways be \n if the message isn't a single
// qbyte long (?? not true anymore?)

#define	S2C_CHALLENGE		'c'
#define	S2C_CONNECTION		'j'
#define	A2A_PING			'k'	// respond with an A2A_ACK
#define	A2A_ACK				'l'	// general acknowledgement without info
#define	A2A_NACK			'm'	// [+ comment] general failure
#define A2A_ECHO			'e' // for echoing
#define	A2C_PRINT			'n'	// print a message on client

#define	S2M_HEARTBEAT		'a'	// + serverinfo + userlist + fraglist
#define	A2C_CLIENT_COMMAND	'B'	// + command line
#define	S2M_SHUTDOWN		'C'

#define M2C_MASTER_REPLY	'd'	// + \n + qw server port list
//==================
// note that there are some defs.qc that mirror to these numbers
// also related to svc_strings[] in cl_parse
//==================

//
// server to client
//
#define	svc_bad				0
#define	svc_nop				1
#define	svc_disconnect		2
#define	svc_updatestat		3	// [qbyte] [qbyte]
#define	svc_version			4	// [long] server version
#define	svc_setview			5	// [short] entity number
#define	svc_sound			6	// <see code>
#define	svc_time			7	// [float] server time
#define	svc_print			8	// [qbyte] id [string] null terminated string
#define	svc_stufftext		9	// [string] stuffed into client's console buffer
								// the string should be \n terminated
#define	svc_setangle		10	// [angle3] set the view angle to this absolute value

#define	svc_serverdata		11	// [long] protocol ...
#define	svc_lightstyle		12	// [qbyte] [string]
#define	svc_updatename		13	// [qbyte] [string]
#define	svc_updatefrags		14	// [qbyte] [short]
#define	svc_clientdata		15	// <shortbits + data>
#define	svc_stopsound		16	// <see code>
#define	svc_updatecolors	17	// [qbyte] [qbyte] [qbyte]
#define	svc_particle		18	// [vec3] <variable>
#define	svc_damage			19
	
#define	svc_spawnstatic		20
#define	svc_spawnstatic2	21
#define	svc_spawnbaseline	22
	
#define	svc_temp_entity		23	// variable
#define	svc_setpause		24	// [qbyte] on / off
#define	svc_signonnum		25	// [qbyte]  used for the signon sequence

#define	svc_centerprint		26	// [string] to put in center of the screen

#define	svc_killedmonster	27
#define	svc_foundsecret		28

#define	svc_spawnstaticsound	29	// [coord3] [qbyte] samp [qbyte] vol [qbyte] aten

#define	svc_intermission	30		// [vec3_t] origin [vec3_t] angle
#define	svc_finale			31		// [string] text

#define	svc_cdtrack			32		// [qbyte] track
#define svc_sellscreen		33

#define svc_cutscene		34	//hmm... nq only... added after qw tree splitt?

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
#define	svc_nails			43		// [qbyte] num [48 bits] xyzpy 12 12 12 4 8 
#define	svc_chokecount		44		// [qbyte] packets choked
#define	svc_modellist		45		// [strings]
#define	svc_soundlist		46		// [strings]
#define	svc_packetentities	47		// [...]
#define	svc_deltapacketentities	48		// [...]
#define svc_maxspeed		49		// maxspeed change, for prediction
#define svc_entgravity		50		// gravity change, for prediction
#define svc_setinfo			51		// setinfo on a client
#define svc_serverinfo		52		// serverinfo
#define svc_updatepl		53		// [qbyte] [qbyte]

#define svc_nails2			54		//qwe - [qbyte] num [52 bits] nxyzpy 8 12 12 12 4 8 


#ifdef PEXT_VIEW2
#define svc_view2			56
#endif
#ifdef PEXT_LIGHTSTYLECOL
#define svc_lightstylecol	57
#endif

#ifdef PEXT_BULLETENS
#define svc_bulletentext	58
#endif

#ifdef PEXT_LIGHTUPDATES
#define	svc_lightnings		59
#endif

#ifdef PEXT_MODELDBL
#define	svc_modellistshort	60		// [strings]
#endif

#define svc_ftesetclientpersist	61	//ushort DATA

#define svc_setportalstate 62

#define	svc_particle2		63
#define	svc_particle3		64
#define	svc_particle4		65
#define svc_spawnbaseline2	66

#define	svc_customtempent	67

#define svc_choosesplitclient 68
#define svc_showpic			69
#define svc_hidepic			70
#define svc_movepic			71
#define svc_updatepic		72

#define svc_invalid			256


enum svcq2_ops_e
{
	svcq2_bad,	//0

	// these ops are known to the game dll
	svcq2_muzzleflash,	//1
	svcq2_muzzleflash2,	//2
	svcq2_temp_entity,	//3
	svcq2_layout,		//4
	svcq2_inventory,	//5

	// the rest are private to the client and server
	svcq2_nop,			//6
	svcq2_disconnect,	//7
	svcq2_reconnect,	//8
	svcq2_sound,		//9			// <see code>
	svcq2_print,		//10			// [qbyte] id [string] null terminated string
	svcq2_stufftext,	//11			// [string] stuffed into client's console buffer, should be \n terminated
	svcq2_serverdata,	//12			// [long] protocol ...
	svcq2_configstring,	//13		// [short] [string]
	svcq2_spawnbaseline,//14		
	svcq2_centerprint,	//15		// [string] to put in center of the screen
	svcq2_download,		//16		// [short] size [size bytes]
	svcq2_playerinfo,	//17			// variable
	svcq2_packetentities,//18			// [...]
	svcq2_deltapacketentities,//19	// [...]
	svcq2_frame			//20 (the bastard to implement.)
};

enum clcq2_ops_e
{
	clcq2_bad,
	clcq2_nop, 		
	clcq2_move,				// [[usercmd_t]
	clcq2_userinfo,			// [[userinfo string]
	clcq2_stringcmd			// [string] message
};


//==============================================

//
// client to server
//
#define	clc_bad			0
#define	clc_nop 		1
#define	clc_disconnect	2	//nq only
#define	clc_move		3		// [[usercmd_t]
#define	clc_stringcmd	4		// [string] message
#define	clc_delta		5		// [qbyte] sequence number, requests delta compression of message
#define clc_tmove		6		// teleport request, spectator only
#define clc_upload		7		// teleport request, spectator only


//==============================================

// playerinfo flags from server
// playerinfo allways sends: playernum, flags, origin[] and framenumber

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

//ZQuake.
#define	PF_PMC_MASK		((1<<11)	+\
						 (1<<12)	+\
						 (1<<13))

#define PF_EXTRA_PFS	(1<<15)

//FIXME: Resolve this.

// bits 11..13 are player move type bits

#ifdef PEXT_SCALE
#define	PF_SCALE_NOZ		(1<<12)
#define	PF_SCALE_Z			(1<<16)
#endif
#ifdef PEXT_TRANS
#define	PF_TRANS_NOZ		(1<<13)
#define	PF_TRANS_Z			(1<<17)
#endif
#ifdef PEXT_FATNESS
#define	PF_FATNESS_NOZ		(1<<14)
#define	PF_FATNESS_Z		(1<<18)
#endif
#ifdef PEXT_HULLSIZE
#define PF_HULLSIZE_NOZ		(1<<15)
#define	PF_HULLSIZE_Z		(1<<14)
#endif

#define	PF_ORIGINDBL		(1<<19)



#define PF_PMC_SHIFT	11



// player move types
#define PMC_NORMAL			0		// normal ground movement
#define PMC_NORMAL_JUMP_HELD	1	// normal ground novement + jump_held
#define PMC_OLD_SPECTATOR	2		// fly through walls (QW compatibility mode)
#define PMC_SPECTATOR		3		// fly through walls
#define PMC_FLY				4		// fly, bump into walls
#define PMC_NONE			5		// can't move (client had better lerp the origin...)
#define PMC_FREEZE			6		// TODO: lerp movement and viewangles
#define PMC_EXTRA3			7		// future extension

//any more will require a different protocol message.

//==============================================

// if the high bit of the client to server qbyte is set, the low bits are
// client move cmd bits
// ms and angle2 are allways sent, the others are optional
#define	CM_ANGLE1 	(1<<0)
#define	CM_ANGLE3 	(1<<1)
#define	CM_FORWARD	(1<<2)
#define	CM_SIDE		(1<<3)
#define	CM_UP		(1<<4)
#define	CM_BUTTONS	(1<<5)
#define	CM_IMPULSE	(1<<6)
#define	CM_ANGLE2 	(1<<7)

//sigh...
#define	Q2CM_ANGLE1 	(1<<0)
#define	Q2CM_ANGLE2 	(1<<1)
#define	Q2CM_ANGLE3 	(1<<2)
#define	Q2CM_FORWARD	(1<<3)
#define	Q2CM_SIDE		(1<<4)
#define	Q2CM_UP			(1<<5)
#define	Q2CM_BUTTONS	(1<<6)
#define	Q2CM_IMPULSE	(1<<7)

//==============================================

// the first 16 bits of a packetentities update holds 9 bits
// of entity number and 7 bits of flags
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
#ifdef PROTOCOLEXTENSIONS
#define U_EVENMORE	(1<<7)	//extension info follows

//fte extensions
//EVENMORE flags
#ifdef PEXT_SCALE
#define U_SCALE		(1<<0)	//scaler of alias models
#endif
#ifdef PEXT_TRANS
#define U_TRANS		(1<<1)	//transparency value
#endif
#ifdef PEXT_FATNESS
#define U_FATNESS	(1<<2)	//qbyte describing how fat an alias model should be. (moves verticies along normals). Useful for vacuum chambers...
#endif
#ifdef PEXT_MODELDBL
#define U_MODELDBL	(1<<3)	//extra bit for modelindexes
#endif
//FIXME: IMPLEMENT
#ifdef PEXT_ENTITYDBL
#define U_ENTITYDBL	(1<<5)	//use an extra qbyte for origin parts, cos one of them is off
#endif
#ifdef PEXT_ENTITYDBL2
#define U_ENTITYDBL2 (1<<6)	//use an extra qbyte for origin parts, cos one of them is off
#endif
#define U_YETMORE	(1<<7)	//even more extension info stuff.

#define U_DRAWFLAGS	(1<<8)	//use an extra qbyte for origin parts, cos one of them is off
#define U_ABSLIGHT	(1<<9)	//Force a lightlevel

#ifdef PEXT_BIGORIGINS
#define U_ORIGINDBL	(1<<10)	//use an extra qbyte for origin parts, cos one of them is off
#endif


#endif





#define	Q2U_ORIGIN1	(1<<0)
#define	Q2U_ORIGIN2	(1<<1)
#define	Q2U_ANGLE2	(1<<2)
#define	Q2U_ANGLE3	(1<<3)
#define	Q2U_FRAME8	(1<<4)		// frame is a qbyte
#define	Q2U_EVENT		(1<<5)
#define	Q2U_REMOVE	(1<<6)		// REMOVE this entity, don't add it
#define	Q2U_MOREBITS1	(1<<7)		// read one additional qbyte

// second qbyte
#define	Q2U_NUMBER16	(1<<8)		// NUMBER8 is implicit if not set
#define	Q2U_ORIGIN3	(1<<9)
#define	Q2U_ANGLE1	(1<<10)
#define	Q2U_MODEL		(1<<11)
#define Q2U_RENDERFX8	(1<<12)		// fullbright, etc
#define	Q2U_EFFECTS8	(1<<14)		// autorotate, trails, etc
#define	Q2U_MOREBITS2	(1<<15)		// read one additional qbyte

// third qbyte
#define	Q2U_SKIN8		(1<<16)
#define	Q2U_FRAME16	(1<<17)		// frame is a short
#define	Q2U_RENDERFX16 (1<<18)	// 8 + 16 = 32
#define	Q2U_EFFECTS16	(1<<19)		// 8 + 16 = 32
#define	Q2U_MODEL2	(1<<20)		// weapons, flags, etc
#define	Q2U_MODEL3	(1<<21)
#define	Q2U_MODEL4	(1<<22)
#define	Q2U_MOREBITS3	(1<<23)		// read one additional qbyte

// fourth qbyte
#define	Q2U_OLDORIGIN	(1<<24)		// FIXME: get rid of this
#define	Q2U_SKIN16	(1<<25)
#define	Q2U_SOUND		(1<<26)
#define	Q2U_SOLID		(1<<27)

//==============================================

// a sound with no channel is a local only sound
// the sound field has bits 0-2: channel, 3-12: entity
#define	SND_VOLUME		(1<<15)		// a qbyte
#define	SND_ATTENUATION	(1<<14)		// a qbyte

#define DEFAULT_SOUND_PACKET_VOLUME 255
#define DEFAULT_SOUND_PACKET_ATTENUATION 1.0


#define	DEFAULT_VIEWHEIGHT	22


// svc_print messages have an id, so messages can be filtered
#define	PRINT_LOW			0
#define	PRINT_MEDIUM		1
#define	PRINT_HIGH			2
#define	PRINT_CHAT			3	// also go to chat buffer

//
// temp entity events
//
enum {
	TE_SPIKE			= 0,
	TE_SUPERSPIKE		= 1,
	TE_GUNSHOT			= 2,
	TE_EXPLOSION		= 3,
	TE_TAREXPLOSION		= 4,
	TE_LIGHTNING1		= 5,
	TE_LIGHTNING2		= 6,
	TE_WIZSPIKE			= 7,
	TE_KNIGHTSPIKE		= 8,
	TE_LIGHTNING3		= 9,
	TE_LAVASPLASH		= 10,
	TE_TELEPORT			= 11,

	TE_BLOOD			= 12,
	TE_LIGHTNINGBLOOD	= 13,

#ifdef PEXT_TE_BULLET
	TE_BULLET			= 14,
	TE_SUPERBULLET		= 15,
#endif

	TE_RAILTRAIL		= 17,

		// hexen 2
	TE_STREAM_CHAIN			= 25,
	TE_STREAM_SUNSTAFF1		= 26,
	TE_STREAM_SUNSTAFF2		= 27,
	TE_STREAM_LIGHTNING		= 28,
	TE_STREAM_COLORBEAM		= 29,
	TE_STREAM_ICECHUNKS		= 30,
	TE_STREAM_GAZE			= 31,
	TE_STREAM_FAMINE		= 32,

	TE_BIGGRENADE			= 33,
	TE_CHUNK				= 34,
	TE_HWBONEPOWER			= 35,
	TE_HWBONEPOWER2			= 36,
	TE_METEORHIT			= 37,
	TE_HWRAVENDIE			= 38,
	TE_HWRAVENEXPLODE		= 39,
	TE_XBOWHIT				= 40,

	TE_CHUNK2				= 41,
	TE_ICEHIT				= 42,
	TE_ICESTORM				= 43,
	TE_HWMISSILEFLASH		= 44,
	TE_SUNSTAFF_CHEAP		= 45,
	TE_LIGHTNING_HAMMER		= 46,
	TE_DRILLA_EXPLODE		= 47,
	TE_DRILLA_DRILL			= 48,

	TE_HWTELEPORT			= 49,
	TE_SWORD_EXPLOSION		= 50,

	TE_AXE_BOUNCE			= 51,
	TE_AXE_EXPLODE			= 52,
	TE_TIME_BOMB			= 53,
	TE_FIREBALL				= 54,
	TE_SUNSTAFF_POWER		= 55,
	TE_PURIFY2_EXPLODE		= 56,
	TE_PLAYER_DEATH			= 57,
	TE_PURIFY1_EFFECT		= 58,
	TE_TELEPORT_LINGER		= 59,
	TE_LINE_EXPLOSION		= 60,
	TE_METEOR_CRUSH			= 61,
//MISSION PACK
	TE_STREAM_LIGHTNING_SMALL	= 62,

	TE_ACIDBALL				= 63,
	TE_ACIDBLOB				= 64,
	TE_FIREWALL				= 65,
	TE_FIREWALL_IMPACT		= 66,
	TE_HWBONERIC			= 67,
	TE_POWERFLAME			= 68,
	TE_BLOODRAIN			= 69,
	TE_AXE					= 70,
	TE_PURIFY2_MISSILE		= 71,
	TE_SWORD_SHOT			= 72,
	TE_ICESHOT				= 73,
	TE_METEOR				= 74,
	TE_LIGHTNINGBALL		= 75,
	TE_MEGAMETEOR			= 76,
	TE_CUBEBEAM				= 77,
	TE_LIGHTNINGEXPLODE		= 78,
	TE_ACID_BALL_FLY		= 79,
	TE_ACID_BLOB_FLY		= 80,
	TE_CHAINLIGHTNING		= 81
};

#define NQTE_EXPLOSION2	12
#define NQTE_BEAM		13


#define TE_SEEF_BRIGHTFIELD	200
#define TE_SEEF_DARKLIGHT	201
#define TE_SEEF_DARKFIELD	202
#define	TE_SEEF_LIGHT		203

//FTE's version of TEI_SHOWLMP2
#define SL_ORG_NW	0
#define SL_ORG_NE	1
#define SL_ORG_SW	2
#define SL_ORG_SE	3
#define SL_ORG_CC	4
#define SL_ORG_CN	5
#define SL_ORG_CS	6
#define SL_ORG_CW	7
#define SL_ORG_CE	8

/*
==========================================================

  ELEMENTS COMMUNICATED ACROSS THE NET

==========================================================
*/

#define	MAX_CLIENTS		32

#define	UPDATE_BACKUP	64	// copies of entity_state_t to keep buffered
							// must be power of two
#define	UPDATE_MASK		(UPDATE_BACKUP-1)

#define	Q2UPDATE_BACKUP	16	// copies of entity_state_t to keep buffered
							// must be power of two
#define	Q2UPDATE_MASK		(Q2UPDATE_BACKUP-1)

// entity_state_t is the information conveyed from the server
// in an update message

//FIXME: split the q2 vars.
#ifdef SERVERONLY
typedef struct entity_state_s
{
	int		number;			// edict index

	int		flags;			// nolerp, etc
	vec3_t	origin;
	vec3_t	angles;
	int		modelindex;
	int		frame;
	int		colormap;
	int		skinnum;
	int		effects;

#ifdef PEXT_SCALE
	float	scale;
#endif
#ifdef PEXT_TRANS
	float	trans;
#endif
#ifdef PEXT_FATNESS
	float fatness;
#endif

	int		drawflags;
	int		abslight;
} entity_state_t;
#else
typedef struct entity_state_s
{
	int		number;			// edict index

	int		flags;			// nolerp, etc
	vec3_t	origin;
	vec3_t	old_origin;		//q2
	vec3_t	angles;
	int		modelindex;
	int		modelindex2;	//q2
	int		modelindex3;	//q2
	int		modelindex4;	//q2
	int		frame;
	int		colormap;
	int		skinnum;
	int		effects;
	int		renderfx;		//q2
	int		sound;			//q2
	int		event;			//q2

	int		solid;

#ifdef PEXT_SCALE
	float	scale;
#endif
#ifdef PEXT_TRANS
	float	trans;
#endif
#ifdef PEXT_FATNESS
	float	fatness;
#endif
	int		drawflags;
	int		abslight;
} entity_state_t;
#endif

#define MAX_EXTENDED_PACKET_ENTITIES	256	//sanity limit.
#define	MAX_STANDARD_PACKET_ENTITIES	64	// doesn't count nails
#define	MAX_MVDPACKET_ENTITIES	196	// doesn't count nails
typedef struct
{
	int		num_entities;
	int		max_entities;
	entity_state_t	*entities;
} packet_entities_t;

typedef struct usercmd_s
{
	qbyte	msec;
	qbyte	buttons;
	short	angles[3];
	short	forwardmove, sidemove, upmove;
	qbyte	impulse;
	qbyte lightlevel;
} usercmd_t;
#define SHORT2ANGLE(x) (x) * (360.0/65536)




//
// per-level limits
//
#define	Q2MAX_CLIENTS			256		// absolute limit
#define	Q2MAX_EDICTS			1024	// must change protocol to increase more
#define	Q2MAX_LIGHTSTYLES		256
#define	Q2MAX_MODELS			256		// these are sent over the net as bytes
#define	Q2MAX_SOUNDS			256		// so they cannot be blindly increased
#define	Q2MAX_IMAGES			256
#define	Q2MAX_ITEMS				256
#define Q2MAX_GENERAL			(Q2MAX_CLIENTS*2)	// general config strings


#define	Q2CS_NAME				0
#define	Q2CS_CDTRACK			1
#define	Q2CS_SKY				2
#define	Q2CS_SKYAXIS			3		// %f %f %f format
#define	Q2CS_SKYROTATE			4
#define	Q2CS_STATUSBAR			5		// display program string

#define Q2CS_AIRACCEL			29		// air acceleration control
#define	Q2CS_MAXCLIENTS			30
#define	Q2CS_MAPCHECKSUM		31		// for catching cheater maps

#define	Q2CS_MODELS				32
#define	Q2CS_SOUNDS				(Q2CS_MODELS	+Q2MAX_MODELS)
#define	Q2CS_IMAGES				(Q2CS_SOUNDS	+Q2MAX_SOUNDS)
#define	Q2CS_LIGHTS				(Q2CS_IMAGES	+Q2MAX_IMAGES)
#define	Q2CS_ITEMS				(Q2CS_LIGHTS	+Q2MAX_LIGHTSTYLES)
#define	Q2CS_PLAYERSKINS		(Q2CS_ITEMS		+Q2MAX_ITEMS)
#define Q2CS_GENERAL			(Q2CS_PLAYERSKINS	+Q2MAX_CLIENTS)
#define	Q2MAX_CONFIGSTRINGS		(Q2CS_GENERAL	+Q2MAX_GENERAL)


// player_state->stats[] indexes
#define Q2STAT_HEALTH_ICON		0
#define	Q2STAT_HEALTH				1
#define	Q2STAT_AMMO_ICON			2
#define	Q2STAT_AMMO				3
#define	Q2STAT_ARMOR_ICON			4
#define	Q2STAT_ARMOR				5
#define	Q2STAT_SELECTED_ICON		6
#define	Q2STAT_PICKUP_ICON		7
#define	Q2STAT_PICKUP_STRING		8
#define	Q2STAT_TIMER_ICON			9
#define	Q2STAT_TIMER				10
#define	Q2STAT_HELPICON			11
#define	Q2STAT_SELECTED_ITEM		12
#define	Q2STAT_LAYOUTS			13
#define	Q2STAT_FRAGS				14
#define	Q2STAT_FLASHES			15		// cleared each frame, 1 = health, 2 = armor
#define Q2STAT_CHASE				16
#define Q2STAT_SPECTATOR			17

#define	Q2MAX_STATS				32



// edict->drawflags
#define MLS_MASKIN				7	// Model Light Style
#define MLS_MASKOUT				248
#define MLS_NONE				0
#define MLS_FULLBRIGHT			1
#define MLS_POWERMODE			2
#define MLS_TORCH				3
#define MLS_TOTALDARK			4
#define MLS_ABSLIGHT			7
#define SCALE_TYPE_MASKIN		24
#define SCALE_TYPE_MASKOUT		231
#define SCALE_TYPE_UNIFORM		0	// Scale X, Y, and Z
#define SCALE_TYPE_XYONLY		8	// Scale X and Y
#define SCALE_TYPE_ZONLY		16	// Scale Z
#define SCALE_ORIGIN_MASKIN		96
#define SCALE_ORIGIN_MASKOUT	159
#define SCALE_ORIGIN_CENTER		0	// Scaling origin at object center
#define SCALE_ORIGIN_BOTTOM		32	// Scaling origin at object bottom
#define SCALE_ORIGIN_TOP		64	// Scaling origin at object top
#define DRF_TRANSLUCENT			128

