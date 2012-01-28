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
//#define PEXT_BULLETENS			0x00000020 //obsolete
#define PEXT_ACCURATETIMINGS	0x00000040
#define PEXT_SOUNDDBL			0x00000080	//revised startsound protocol
#define PEXT_FATNESS			0x00000100	//GL only (or servers)
#define PEXT_HLBSP				0x00000200
#define PEXT_TE_BULLET			0x00000400
#define PEXT_HULLSIZE			0x00000800
#define PEXT_MODELDBL			0x00001000
#define PEXT_ENTITYDBL			0x00002000	//max of 1024 ents instead of 512
#define PEXT_ENTITYDBL2			0x00004000	//max of 1024 ents instead of 512
#define PEXT_FLOATCOORDS		0x00008000	//supports floating point origins.
//#define PEXT_VWEAP				0x00010000	//cause an extra qbyte to be sent, and an extra list of models for vweaps.
#ifdef Q2BSPS
#define PEXT_Q2BSP				0x00020000
#endif
#ifdef Q3BSPS
#define PEXT_Q3BSP				0x00040000
#endif

#define PEXT_COLOURMOD			0x00080000	//this replaces an older value which would rarly have caried any actual data.

#define PEXT_SPLITSCREEN		0x00100000
#define PEXT_HEXEN2				0x00200000	//more stats and working particle builtin.
#define PEXT_SPAWNSTATIC2		0x00400000	//Sends an entity delta instead of a baseline.
#define PEXT_CUSTOMTEMPEFFECTS	0x00800000	//supports custom temp ents.
#define PEXT_256PACKETENTITIES	0x01000000	//Client can recieve 256 packet entities.
//#define PEXT_NEVERUSED		0x02000000	//Client is able to cope with 64 players. Wow.
#define PEXT_SHOWPIC			0x04000000
#define PEXT_SETATTACHMENT		0x08000000	//md3 tags (needs networking, they need to lerp).
//#define PEXT_NEVERUSED		0x10000000	//retrieve a list of pk3s/pk3s/paks for downloading (with optional URL and crcs)
#define PEXT_CHUNKEDDOWNLOADS	0x20000000	//alternate file download method. Hopefully it'll give quadroupled download speed, especially on higher pings.

#ifdef CSQC_DAT
#define PEXT_CSQC				0x40000000	//csqc additions
#endif

#define PEXT_DPFLAGS			0x80000000	//extra flags for viewmodel/externalmodel and possible other persistant style flags.

#ifdef CSQC_DAT
#define PEXT_BIGUSERINFOS	PEXT_CSQC
#else
#define PEXT_BIGUSERINFOS	0xffffffff
#endif		

#define PEXT2_PRYDONCURSOR			0x00000001
#define PEXT2_VOICECHAT				0x00000002
#define PEXT2_SETANGLEDELTA			0x00000004
//#define PEXT2_64PLAYERS			0x02000000	//Client is able to cope with 64 players. Wow.
//#define PEXT2_PK3DOWNLOADS		0x10000000	//retrieve a list of pk3s/pk3s/paks for downloading (with optional URL and crcs)

//ZQuake transparent protocol extensions.
#define Z_EXT_PM_TYPE		(1<<0)	// basic PM_TYPE functionality (reliable jump_held)
#define Z_EXT_PM_TYPE_NEW	(1<<1)	// adds PM_FLY, PM_SPECTATOR
#define Z_EXT_VIEWHEIGHT	(1<<2)	// STAT_VIEWHEIGHT
#define Z_EXT_SERVERTIME	(1<<3)	// STAT_TIME
#define Z_EXT_PITCHLIMITS	(1<<4)	// serverinfo maxpitch & minpitch
#define Z_EXT_JOIN_OBSERVE	(1<<5)	// server: "join" and "observe" commands are supported
									// client: on-the-fly spectator <-> player switching supported

//#define Z_EXT_PF_ONGROUND	(1<<6)	// server: PF_ONGROUND is valid for all svc_playerinfo
#define Z_EXT_VWEP			(1<<7)
//#define Z_EXT_PF_SOLID		(1<<8)	//conflicts with many FTE extensions.

#define SUPPORTED_Z_EXTENSIONS (Z_EXT_PM_TYPE|Z_EXT_PM_TYPE_NEW|Z_EXT_VIEWHEIGHT|Z_EXT_SERVERTIME|Z_EXT_PITCHLIMITS|Z_EXT_JOIN_OBSERVE|Z_EXT_VWEP)


#define PROTOCOL_VERSION_FTE			(('F'<<0) + ('T'<<8) + ('E'<<16) + ('X' << 24))	//fte extensions.
#define PROTOCOL_VERSION_FTE2			(('F'<<0) + ('T'<<8) + ('E'<<16) + ('2' << 24))	//fte extensions.
#define PROTOCOL_VERSION_HUFFMAN		(('H'<<0) + ('U'<<8) + ('F'<<16) + ('F' << 24))	//packet compression
#define PROTOCOL_VERSION_VARLENGTH		(('v'<<0) + ('l'<<8) + ('e'<<16) + ('n' << 24))	//packet compression

#define PROTOCOL_INFO_GUID				(('G'<<0) + ('U'<<8) + ('I'<<16) + ('D' << 24))	//globally 'unique' client id info.

#define	PROTOCOL_VERSION_QW 28
#define	PROTOCOL_VERSION_Q2_MIN 31
#define	PROTOCOL_VERSION_Q2 34

//=========================================

#define	PORT_NQSERVER	26000
#define	PORT_QWCLIENT	27001
#define	PORT_QWMASTER	27000
#define	PORT_QWSERVER	27500
#define PORT_Q2CLIENT 27901
#define PORT_Q2SERVER 27910

//hexen2: 26900

//=========================================

// out of band message id bytes

// M = master, S = server, C = client, A = any
// the second character will always be \n if the message isn't a single
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

#define C2M_MASTER_REQUEST  'c'
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



//QW svcs
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

//mvdsv extended svcs (for mvd playback)
#define svc_nails2			54		//qwe - [qbyte] num [52 bits] nxyzpy 8 12 12 12 4 8

//FTE extended svcs
#ifdef PEXT_SOUNDDBL
#define svcfte_soundextended			55
#define svcfte_soundlistshort			56
#endif
#ifdef PEXT_LIGHTSTYLECOL
#define svcfte_lightstylecol	57
#endif

//#define svcfte_svcremoved	58

//#define	svcfte_svcremoved		59

#ifdef PEXT_MODELDBL
#define	svcfte_modellistshort	60		// [strings]
#endif

#define svc_ftesetclientpersist	61	//ushort DATA

#define svc_setportalstate 62

#define	svcfte_particle2		63
#define	svcfte_particle3		64
#define	svcfte_particle4		65
#define svcfte_spawnbaseline2	66

#define	svcfte_customtempent	67

#define svcfte_choosesplitclient 68
#define svcfte_showpic			69
#define svcfte_hidepic			70
#define svcfte_movepic			71
#define svcfte_updatepic		72

#define svcfte_effect			74		// [vector] org [byte] modelindex [byte] startframe [byte] framecount [byte] framerate
#define svcfte_effect2			75		// [vector] org [short] modelindex [short] startframe [byte] framecount [byte] framerate

#ifdef PEXT_CSQC
#define svcfte_csqcentities	76	//entity lump for csqc
#endif

#define svcfte_precache		77

#define svcfte_updatestatstring 78
#define svcfte_updatestatfloat 79

#define svcfte_trailparticles	80		// [short] entnum [short] effectnum [vector] start [vector] end
#define svcfte_pointparticles	81		// [short] effectnum [vector] start [vector] velocity [short] count
#define svcfte_pointparticles1	82		// [short] effectnum [vector] start, same as svc_pointparticles except velocity is zero and count is 1

#define svcfte_cgamepacket	83
#define svcfte_voicechat	84
#define	svcfte_setangledelta		85	// [angle3] add this to the current viewangles


//fitz svcs
#define svcfitz_skybox				37
#define svcfitz_bf					40
#define svcfitz_fog					41
#define svcfitz_spawnbaseline2		42
#define svcfitz_spawnstatic2		43
#define svcfitz_spawnstaticsound2	44

//DP extended svcs
#define svcdp_downloaddata	50
#define svcdp_updatestatbyte	51
#define svcnq_effect		52		// [vector] org [byte] modelindex [byte] startframe [byte] framecount [byte] framerate
#define svcnq_effect2		53		// [vector] org [short] modelindex [short] startframe [byte] framecount [byte] framerate
#define	svcdp_precache		54		// [short] precacheindex [string] filename, precacheindex is + 0 for modelindex and +32768 for soundindex
#define svcdp_spawnbaseline2	55
#define svcdp_entities		57
#define svcdp_csqcentities 58
#define	svcdp_spawnstaticsound2	59	// [coord3] [short] samp [byte] vol [byte] aten
#define svcdp_trailparticles	60		// [short] entnum [short] effectnum [vector] start [vector] end
#define svcdp_pointparticles	61		// [short] effectnum [vector] start [vector] velocity [short] count
#define svcdp_pointparticles1	62		// [short] effectnum [vector] start, same as svc_pointparticles except velocity is zero and count is 1



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
#define	clc_bad				0
#define	clc_nop 			1
#define	clc_disconnect		2	//nq only
#define	clc_move			3	// [[usercmd_t]
#define	clc_stringcmd		4	// [string] message
#define	clc_delta			5	// [qbyte] sequence number, requests delta compression of message
#define clc_tmove			6	// teleport request, spectator only
#define clc_upload			7	// teleport request, spectator only

#define clcdp_ackframe			50
#define clcdp_ackdownloaddata	51

#define clc_qcrequest		81
#define clc_prydoncursor	82
#define clc_voicechat		83


//==============================================

// playerinfo flags from server
// playerinfo always sends: playernum, flags, origin[] and framenumber

#define	PF_MSEC			(1<<0)	//msecs says how long the player command was sitting on the server before it was sent back to the client
#define	PF_COMMAND		(1<<1)	//angles and movement values for other players (no msec or impulse)
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


#ifdef PEXT_HULLSIZE
#define	PF_HULLSIZE_Z		(1<<14)
#endif
#define PF_EXTRA_PFS	(1<<15)

#ifdef PEXT_SCALE
#define	PF_SCALE			(1<<16)
#endif
#ifdef PEXT_TRANS
#define	PF_TRANS			(1<<17)
#endif
#ifdef PEXT_FATNESS
#define	PF_FATNESS		(1<<18)
#endif

#define	PF_COLOURMOD		(1<<19)
//note that if you add any more, you may need to change the check in the client so more can be parsed



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
// ms and angle2 are always sent, the others are optional
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
#define	U_UNUSABLE	(1<<8)
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
#define U_UNUSED1	(1<<4)
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

#define U_COLOURMOD	(1<<10)	//rgb

#define U_DPFLAGS (1<<11)


#define U_TAGINFO (1<<12)
#define U_LIGHT (1<<13)
#define	U_EFFECTS16	(1<<14)

#define U_FARMORE (1<<15)

#endif


#ifdef NQPROT

#define	NQU_MOREBITS	(1<<0)
#define	NQU_ORIGIN1	(1<<1)
#define	NQU_ORIGIN2	(1<<2)
#define	NQU_ORIGIN3	(1<<3)
#define	NQU_ANGLE2	(1<<4)
#define	NQU_NOLERP	(1<<5)		// don't interpolate movement
#define	NQU_FRAME		(1<<6)
#define NQU_SIGNAL	(1<<7)		// just differentiates from other updates

// svc_update can pass all of the fast update bits, plus more
#define	NQU_ANGLE1	(1<<8)
#define	NQU_ANGLE3	(1<<9)
#define	NQU_MODEL		(1<<10)
#define	NQU_COLORMAP	(1<<11)
#define	NQU_SKIN		(1<<12)
#define	NQU_EFFECTS	(1<<13)
#define	NQU_LONGENTITY	(1<<14)


// LordHavoc's: protocol extension
#define DPU_EXTEND1		(1<<15)
// LordHavoc: first extend byte
#define DPU_DELTA			(1<<16) // no data, while this is set the entity is delta compressed (uses previous frame as a baseline, meaning only things that have changed from the previous frame are sent, except for the forced full update every half second)
#define DPU_ALPHA			(1<<17) // 1 byte, 0.0-1.0 maps to 0-255, not sent if exactly 1, and the entity is not sent if <=0 unless it has effects (model effects are checked as well)
#define DPU_SCALE			(1<<18) // 1 byte, scale / 16 positive, not sent if 1.0
#define DPU_EFFECTS2		(1<<19) // 1 byte, this is .effects & 0xFF00 (second byte)
#define DPU_GLOWSIZE		(1<<20) // 1 byte, encoding is float/4.0, unsigned, not sent if 0
#define DPU_GLOWCOLOR		(1<<21) // 1 byte, palette index, default is 254 (white), this IS used for darklight (allowing colored darklight), however the particles from a darklight are always black, not sent if default value (even if glowsize or glowtrail is set)
// LordHavoc: colormod feature has been removed, because no one used it
#define DPU_COLORMOD		(1<<22) // 1 byte, 3 bit red, 3 bit green, 2 bit blue, this lets you tint an object artifically, so you could make a red rocket, or a blue fiend...
#define DPU_EXTEND2		(1<<23) // another byte to follow
// LordHavoc: second extend byte
#define DPU_GLOWTRAIL		(1<<24) // leaves a trail of particles (of color .glowcolor, or black if it is a negative glowsize)
#define DPU_VIEWMODEL		(1<<25) // attachs the model to the view (origin and angles become relative to it), only shown to owner, a more powerful alternative to .weaponmodel and such
#define DPU_FRAME2		(1<<26) // 1 byte, this is .frame & 0xFF00 (second byte)
#define DPU_MODEL2		(1<<27) // 1 byte, this is .modelindex & 0xFF00 (second byte)
#define DPU_EXTERIORMODEL	(1<<28) // causes this model to not be drawn when using a first person view (third person will draw it, first person will not)
#define DPU_UNUSED29		(1<<29) // future expansion
#define DPU_UNUSED30		(1<<30) // future expansion
#define DPU_EXTEND3		(1<<31) // another byte to follow, future expansion

#define FITZU_ALPHA (1<<16)
#define FITZU_FRAME2 (1<<17)
#define FITZU_MODEL2 (1<<18)
#define FITZU_LERPFINISH (1<<19)

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
// the sound field has bits 0-2: channel, 3-12: entity, 13: unused, 14-15: flags
#define	SND_VOLUME		(1<<15)		// a qbyte
#define	SND_ATTENUATION	(1<<14)		// a qbyte

#define	NQSND_VOLUME		(1<<0)		// a qbyte
#define	NQSND_ATTENUATION	(1<<1)		// a qbyte
#define DPSND_LOOPING		(1<<2)		// a long, supposedly
#define DPSND_LARGEENTITY	(1<<3)
#define DPSND_LARGESOUND	(1<<4)
#define FTESND_PITCHADJ		(1<<7)		//a byte (speed percent (0=100%))

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

	TEQW_BLOOD			= 12,
	TENQ_EXPLOSION2		= 12,
	TEQW_LIGHTNINGBLOOD	= 13,
	TENQ_BEAM			= 13,

#ifdef PEXT_TE_BULLET
	TE_BULLET			= 14,
	TE_SUPERBULLET		= 15,
#endif

	TE_RAILTRAIL		= 17,

	// hexen 2
	TEH2_STREAM_LIGHTNING_SMALL	= 24,
	TEH2_STREAM_CHAIN			= 25,
	TEH2_STREAM_SUNSTAFF1		= 26,
	TEH2_STREAM_SUNSTAFF2		= 27,
	TEH2_STREAM_LIGHTNING		= 28,
	TEH2_STREAM_COLORBEAM		= 29,
	TEH2_STREAM_ICECHUNKS		= 30,
	TEH2_STREAM_GAZE			= 31,
	TEH2_STREAM_FAMINE		= 32,

	TEDP_BLOOD			= 50,
	TEDP_SPARK			= 51,
	TEDP_BLOODSHOWER	= 52,
	TEDP_EXPLOSIONRGB	= 53,
	TEDP_PARTICLECUBE	= 54,
	TEDP_PARTICLERAIN	= 55, // [vector] min [vector] max [vector] dir [short] count [byte] color
	TEDP_PARTICLESNOW	= 56, // [vector] min [vector] max [vector] dir [short] count [byte] color
	TEDP_GUNSHOTQUAD	= 57, // [vector] origin
	TEDP_SPIKEQUAD		= 58, // [vector] origin
	TEDP_SUPERSPIKEQUAD	= 59, // [vector] origin
	TEDP_EXPLOSIONQUAD	= 70, // [vector] origin
	TEDP_SMALLFLASH		= 72, // [vector] origin
	TEDP_CUSTOMFLASH	= 73,
	TEDP_FLAMEJET		= 74,
	TEDP_PLASMABURN		= 75,
	TEDP_TEI_G3			= 76,
	TEDP_SMOKE			= 77,
	TEDP_TEI_BIGEXPLOSION = 78,
	TEDP_TEI_PLASMAHIT	= 79,
};


#define CTE_CUSTOMCOUNT		1
#define CTE_CUSTOMDIRECTION	2
#define CTE_STAINS			4
#define CTE_GLOWS			8
#define CTE_CHANNELFADE		16
#define CTE_CUSTOMVELOCITY	32
#define CTE_ISBEAM			128

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

#define	Q3UPDATE_BACKUP	32	// copies of entity_state_t to keep buffered
							// must be power of two
#define	Q3UPDATE_MASK		(Q3UPDATE_BACKUP-1)


// entity_state_t is the information conveyed from the server
// in an update message

typedef struct entity_state_s
{
	unsigned short		number;			// edict index
	unsigned short		modelindex;
	unsigned int		bitmask;		// for dp ents, so lost state can be repeated in replacement packets.

	unsigned int		flags;			// nolerp, etc

	unsigned int		effects;

	vec3_t	origin;
	vec3_t	angles;
#if defined(Q2CLIENT) || defined(Q2SERVER)
	int		renderfx;		//q2
	vec3_t	old_origin;		//q2/q3
	qbyte		modelindex3;	//q2
	qbyte		modelindex4;	//q2
	qbyte		sound;			//q2
	qbyte		event;			//q2

	unsigned short		modelindex2;	//q2
#endif
	unsigned short		frame;
	unsigned int		skinnum; /*q2 needs 32 bits, which is quite impressive*/
	unsigned short		colormap;
	//pad 2 bytes
	qbyte glowsize;
	qbyte glowcolour;
	qbyte	scale;
	char	fatness;

	qbyte	hexen2flags;
	qbyte	abslight;
	qbyte	dpflags;
	//pad

	qbyte	colormod[3];//multiply this by 8 to read as 0 to 1...
	qbyte	trans;

	qbyte lightstyle;
	qbyte lightpflags;
	unsigned short solid;

	unsigned short light[4];

	unsigned short tagentity;
	unsigned short tagindex;
} entity_state_t;
extern entity_state_t nullentitystate;


#define MAX_EXTENDED_PACKET_ENTITIES	256	//sanity limit.
#define	MAX_STANDARD_PACKET_ENTITIES	64	// doesn't count nails
#define	MAX_MVDPACKET_ENTITIES	196	// doesn't count nails
typedef struct
{
	float		servertime;
	int		num_entities;
	int		max_entities;
	entity_state_t	*entities;
} packet_entities_t;

typedef struct usercmd_s
{
	//the first members of this structure MUST match the q2 version
	qbyte	msec;
	qbyte	buttons_compat;
	short	angles[3];
	short	forwardmove, sidemove, upmove;
	qbyte	impulse;
	qbyte lightlevel;

	//freestyle
	int buttons;
	int weapon;
	int servertime;
	float fservertime;
	float fclienttime;
} usercmd_t;

typedef struct q2usercmd_s
{
	qbyte	msec;
	qbyte	buttons;
	short	angles[3];
	short	forwardmove, sidemove, upmove;
	qbyte	impulse;
	qbyte lightlevel;
} q2usercmd_t;

typedef struct q1usercmd_s
{
	qbyte	msec;
	vec3_t	angles;
	short	forwardmove, sidemove, upmove;
	qbyte	buttons;
	qbyte	impulse;
} q1usercmd_t;
#define SHORT2ANGLE(x) (x) * (360.0/65536)
#define ANGLE2SHORT(x) (x) * (65536/360.0)




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


//for the local player
#define	Q2PS_M_TYPE			(1<<0)
#define	Q2PS_M_ORIGIN			(1<<1)
#define	Q2PS_M_VELOCITY		(1<<2)
#define	Q2PS_M_TIME			(1<<3)
#define	Q2PS_M_FLAGS			(1<<4)
#define	Q2PS_M_GRAVITY		(1<<5)
#define	Q2PS_M_DELTA_ANGLES	(1<<6)

#define	Q2PS_VIEWOFFSET		(1<<7)
#define	Q2PS_VIEWANGLES		(1<<8)
#define	Q2PS_KICKANGLES		(1<<9)
#define	Q2PS_BLEND			(1<<10)
#define	Q2PS_FOV				(1<<11)
#define	Q2PS_WEAPONINDEX		(1<<12)
#define	Q2PS_WEAPONFRAME		(1<<13)
#define	Q2PS_RDFLAGS			(1<<14)



// entity_state_t->renderfx flags
#define	Q2RF_MINLIGHT			1		// always have some light (viewmodel)
#define	Q2RF_EXTERNALMODEL		2		// don't draw through eyes, only mirrors
#define	Q2RF_WEAPONMODEL		4		// only draw through eyes
#define	Q2RF_FULLBRIGHT			8		// always draw full intensity
#define	Q2RF_DEPTHHACK			16		// for view weapon Z crunching
#define	Q2RF_TRANSLUCENT		32
#define	Q2RF_FRAMELERP			64
#define Q2RF_BEAM				128

#define	Q2RF_CUSTOMSKIN			256		// skin is an index in image_precache
#define	Q2RF_GLOW				512		// pulse lighting for bonus items
#define Q2RF_SHELL_RED			1024
#define	Q2RF_SHELL_GREEN		2048
#define Q2RF_SHELL_BLUE			4096

//ROGUE
#define Q2RF_IR_VISIBLE			0x00008000		// 32768
#define	Q2RF_SHELL_DOUBLE		0x00010000		// 65536
#define	Q2RF_SHELL_HALF_DAM		0x00020000
#define Q2RF_USE_DISGUISE		0x00040000
//ROGUE

#define Q2RF_ADDITIVE			0x00080000
#define RF_NOSHADOW				0x00100000
#define RF_NODEPTHTEST			0x00200000

// player_state_t->refdef flags
#define	Q2RDF_UNDERWATER		1		// warp the screen as apropriate
#define Q2RDF_NOWORLDMODEL		2		// used for player configuration screen

//ROGUE
#define	Q2RDF_IRGOGGLES			4
#define Q2RDF_UVGOGGLES			8
//ROGUE




#define	Q2SND_VOLUME		(1<<0)		// a qbyte
#define	Q2SND_ATTENUATION	(1<<1)		// a qbyte
#define	Q2SND_POS			(1<<2)		// three coordinates
#define	Q2SND_ENT			(1<<3)		// a short 0-2: channel, 3-12: entity
#define	Q2SND_OFFSET		(1<<4)		// a qbyte, msec offset from frame start

#define Q2DEFAULT_SOUND_PACKET_VOLUME	1.0
#define Q2DEFAULT_SOUND_PACKET_ATTENUATION 1.0


#define ATTN_NONE	0
#define ATTN_NORM	1
#define CHAN_AUTO   0
#define CHAN_WEAPON 1
#define CHAN_VOICE  2
#define CHAN_ITEM   3
#define CHAN_BODY   4

#define	Q2MZ_BLASTER			0
#define Q2MZ_MACHINEGUN		1
#define	Q2MZ_SHOTGUN			2
#define	Q2MZ_CHAINGUN1		3
#define	Q2MZ_CHAINGUN2		4
#define	Q2MZ_CHAINGUN3		5
#define	Q2MZ_RAILGUN			6
#define	Q2MZ_ROCKET			7
#define	Q2MZ_GRENADE			8
#define	Q2MZ_LOGIN			9
#define	Q2MZ_LOGOUT			10
#define	Q2MZ_RESPAWN			11
#define	Q2MZ_BFG				12
#define	Q2MZ_SSHOTGUN			13
#define	Q2MZ_HYPERBLASTER		14
#define	Q2MZ_ITEMRESPAWN		15
// RAFAEL
#define Q2MZ_IONRIPPER		16
#define Q2MZ_BLUEHYPERBLASTER 17
#define Q2MZ_PHALANX			18
#define Q2MZ_SILENCED			128		// bit flag ORed with one of the above numbers

//ROGUE
#define Q2MZ_ETF_RIFLE		30
#define Q2MZ_UNUSED			31
#define Q2MZ_SHOTGUN2			32
#define Q2MZ_HEATBEAM			33
#define Q2MZ_BLASTER2			34
#define	Q2MZ_TRACKER			35
#define	Q2MZ_NUKE1			36
#define	Q2MZ_NUKE2			37
#define	Q2MZ_NUKE4			38
#define	Q2MZ_NUKE8			39
//ROGUE

//
// monster muzzle flashes
//
#define Q2MZ2_TANK_BLASTER_1				1
#define Q2MZ2_TANK_BLASTER_2				2
#define Q2MZ2_TANK_BLASTER_3				3
#define Q2MZ2_TANK_MACHINEGUN_1			4
#define Q2MZ2_TANK_MACHINEGUN_2			5
#define Q2MZ2_TANK_MACHINEGUN_3			6
#define Q2MZ2_TANK_MACHINEGUN_4			7
#define Q2MZ2_TANK_MACHINEGUN_5			8
#define Q2MZ2_TANK_MACHINEGUN_6			9
#define Q2MZ2_TANK_MACHINEGUN_7			10
#define Q2MZ2_TANK_MACHINEGUN_8			11
#define Q2MZ2_TANK_MACHINEGUN_9			12
#define Q2MZ2_TANK_MACHINEGUN_10			13
#define Q2MZ2_TANK_MACHINEGUN_11			14
#define Q2MZ2_TANK_MACHINEGUN_12			15
#define Q2MZ2_TANK_MACHINEGUN_13			16
#define Q2MZ2_TANK_MACHINEGUN_14			17
#define Q2MZ2_TANK_MACHINEGUN_15			18
#define Q2MZ2_TANK_MACHINEGUN_16			19
#define Q2MZ2_TANK_MACHINEGUN_17			20
#define Q2MZ2_TANK_MACHINEGUN_18			21
#define Q2MZ2_TANK_MACHINEGUN_19			22
#define Q2MZ2_TANK_ROCKET_1				23
#define Q2MZ2_TANK_ROCKET_2				24
#define Q2MZ2_TANK_ROCKET_3				25

#define Q2MZ2_INFANTRY_MACHINEGUN_1		26
#define Q2MZ2_INFANTRY_MACHINEGUN_2		27
#define Q2MZ2_INFANTRY_MACHINEGUN_3		28
#define Q2MZ2_INFANTRY_MACHINEGUN_4		29
#define Q2MZ2_INFANTRY_MACHINEGUN_5		30
#define Q2MZ2_INFANTRY_MACHINEGUN_6		31
#define Q2MZ2_INFANTRY_MACHINEGUN_7		32
#define Q2MZ2_INFANTRY_MACHINEGUN_8		33
#define Q2MZ2_INFANTRY_MACHINEGUN_9		34
#define Q2MZ2_INFANTRY_MACHINEGUN_10		35
#define Q2MZ2_INFANTRY_MACHINEGUN_11		36
#define Q2MZ2_INFANTRY_MACHINEGUN_12		37
#define Q2MZ2_INFANTRY_MACHINEGUN_13		38

#define Q2MZ2_SOLDIER_BLASTER_1			39
#define Q2MZ2_SOLDIER_BLASTER_2			40
#define Q2MZ2_SOLDIER_SHOTGUN_1			41
#define Q2MZ2_SOLDIER_SHOTGUN_2			42
#define Q2MZ2_SOLDIER_MACHINEGUN_1		43
#define Q2MZ2_SOLDIER_MACHINEGUN_2		44

#define Q2MZ2_GUNNER_MACHINEGUN_1			45
#define Q2MZ2_GUNNER_MACHINEGUN_2			46
#define Q2MZ2_GUNNER_MACHINEGUN_3			47
#define Q2MZ2_GUNNER_MACHINEGUN_4			48
#define Q2MZ2_GUNNER_MACHINEGUN_5			49
#define Q2MZ2_GUNNER_MACHINEGUN_6			50
#define Q2MZ2_GUNNER_MACHINEGUN_7			51
#define Q2MZ2_GUNNER_MACHINEGUN_8			52
#define Q2MZ2_GUNNER_GRENADE_1			53
#define Q2MZ2_GUNNER_GRENADE_2			54
#define Q2MZ2_GUNNER_GRENADE_3			55
#define Q2MZ2_GUNNER_GRENADE_4			56

#define Q2MZ2_CHICK_ROCKET_1				57

#define Q2MZ2_FLYER_BLASTER_1				58
#define Q2MZ2_FLYER_BLASTER_2				59

#define Q2MZ2_MEDIC_BLASTER_1				60

#define Q2MZ2_GLADIATOR_RAILGUN_1			61

#define Q2MZ2_HOVER_BLASTER_1				62

#define Q2MZ2_ACTOR_MACHINEGUN_1			63

#define Q2MZ2_SUPERTANK_MACHINEGUN_1		64
#define Q2MZ2_SUPERTANK_MACHINEGUN_2		65
#define Q2MZ2_SUPERTANK_MACHINEGUN_3		66
#define Q2MZ2_SUPERTANK_MACHINEGUN_4		67
#define Q2MZ2_SUPERTANK_MACHINEGUN_5		68
#define Q2MZ2_SUPERTANK_MACHINEGUN_6		69
#define Q2MZ2_SUPERTANK_ROCKET_1			70
#define Q2MZ2_SUPERTANK_ROCKET_2			71
#define Q2MZ2_SUPERTANK_ROCKET_3			72

#define Q2MZ2_BOSS2_MACHINEGUN_L1			73
#define Q2MZ2_BOSS2_MACHINEGUN_L2			74
#define Q2MZ2_BOSS2_MACHINEGUN_L3			75
#define Q2MZ2_BOSS2_MACHINEGUN_L4			76
#define Q2MZ2_BOSS2_MACHINEGUN_L5			77
#define Q2MZ2_BOSS2_ROCKET_1				78
#define Q2MZ2_BOSS2_ROCKET_2				79
#define Q2MZ2_BOSS2_ROCKET_3				80
#define Q2MZ2_BOSS2_ROCKET_4				81

#define Q2MZ2_FLOAT_BLASTER_1				82

#define Q2MZ2_SOLDIER_BLASTER_3			83
#define Q2MZ2_SOLDIER_SHOTGUN_3			84
#define Q2MZ2_SOLDIER_MACHINEGUN_3		85
#define Q2MZ2_SOLDIER_BLASTER_4			86
#define Q2MZ2_SOLDIER_SHOTGUN_4			87
#define Q2MZ2_SOLDIER_MACHINEGUN_4		88
#define Q2MZ2_SOLDIER_BLASTER_5			89
#define Q2MZ2_SOLDIER_SHOTGUN_5			90
#define Q2MZ2_SOLDIER_MACHINEGUN_5		91
#define Q2MZ2_SOLDIER_BLASTER_6			92
#define Q2MZ2_SOLDIER_SHOTGUN_6			93
#define Q2MZ2_SOLDIER_MACHINEGUN_6		94
#define Q2MZ2_SOLDIER_BLASTER_7			95
#define Q2MZ2_SOLDIER_SHOTGUN_7			96
#define Q2MZ2_SOLDIER_MACHINEGUN_7		97
#define Q2MZ2_SOLDIER_BLASTER_8			98
#define Q2MZ2_SOLDIER_SHOTGUN_8			99
#define Q2MZ2_SOLDIER_MACHINEGUN_8		100

// --- Xian shit below ---
#define	Q2MZ2_MAKRON_BFG					101
#define Q2MZ2_MAKRON_BLASTER_1			102
#define Q2MZ2_MAKRON_BLASTER_2			103
#define Q2MZ2_MAKRON_BLASTER_3			104
#define Q2MZ2_MAKRON_BLASTER_4			105
#define Q2MZ2_MAKRON_BLASTER_5			106
#define Q2MZ2_MAKRON_BLASTER_6			107
#define Q2MZ2_MAKRON_BLASTER_7			108
#define Q2MZ2_MAKRON_BLASTER_8			109
#define Q2MZ2_MAKRON_BLASTER_9			110
#define Q2MZ2_MAKRON_BLASTER_10			111
#define Q2MZ2_MAKRON_BLASTER_11			112
#define Q2MZ2_MAKRON_BLASTER_12			113
#define Q2MZ2_MAKRON_BLASTER_13			114
#define Q2MZ2_MAKRON_BLASTER_14			115
#define Q2MZ2_MAKRON_BLASTER_15			116
#define Q2MZ2_MAKRON_BLASTER_16			117
#define Q2MZ2_MAKRON_BLASTER_17			118
#define Q2MZ2_MAKRON_RAILGUN_1			119
#define	Q2MZ2_JORG_MACHINEGUN_L1			120
#define	Q2MZ2_JORG_MACHINEGUN_L2			121
#define	Q2MZ2_JORG_MACHINEGUN_L3			122
#define	Q2MZ2_JORG_MACHINEGUN_L4			123
#define	Q2MZ2_JORG_MACHINEGUN_L5			124
#define	Q2MZ2_JORG_MACHINEGUN_L6			125
#define	Q2MZ2_JORG_MACHINEGUN_R1			126
#define	Q2MZ2_JORG_MACHINEGUN_R2			127
#define	Q2MZ2_JORG_MACHINEGUN_R3			128
#define	Q2MZ2_JORG_MACHINEGUN_R4			129
#define Q2MZ2_JORG_MACHINEGUN_R5			130
#define	Q2MZ2_JORG_MACHINEGUN_R6			131
#define Q2MZ2_JORG_BFG_1					132
#define Q2MZ2_BOSS2_MACHINEGUN_R1			133
#define Q2MZ2_BOSS2_MACHINEGUN_R2			134
#define Q2MZ2_BOSS2_MACHINEGUN_R3			135
#define Q2MZ2_BOSS2_MACHINEGUN_R4			136
#define Q2MZ2_BOSS2_MACHINEGUN_R5			137

//ROGUE
#define	Q2MZ2_CARRIER_MACHINEGUN_L1		138
#define	Q2MZ2_CARRIER_MACHINEGUN_R1		139
#define	Q2MZ2_CARRIER_GRENADE				140
#define Q2MZ2_TURRET_MACHINEGUN			141
#define Q2MZ2_TURRET_ROCKET				142
#define Q2MZ2_TURRET_BLASTER				143
#define Q2MZ2_STALKER_BLASTER				144
#define Q2MZ2_DAEDALUS_BLASTER			145
#define Q2MZ2_MEDIC_BLASTER_2				146
#define	Q2MZ2_CARRIER_RAILGUN				147
#define	Q2MZ2_WIDOW_DISRUPTOR				148
#define	Q2MZ2_WIDOW_BLASTER				149
#define	Q2MZ2_WIDOW_RAIL					150
#define	Q2MZ2_WIDOW_PLASMABEAM			151		// PMM - not used
#define	Q2MZ2_CARRIER_MACHINEGUN_L2		152
#define	Q2MZ2_CARRIER_MACHINEGUN_R2		153
#define	Q2MZ2_WIDOW_RAIL_LEFT				154
#define	Q2MZ2_WIDOW_RAIL_RIGHT			155
#define	Q2MZ2_WIDOW_BLASTER_SWEEP1		156
#define	Q2MZ2_WIDOW_BLASTER_SWEEP2		157
#define	Q2MZ2_WIDOW_BLASTER_SWEEP3		158
#define	Q2MZ2_WIDOW_BLASTER_SWEEP4		159
#define	Q2MZ2_WIDOW_BLASTER_SWEEP5		160
#define	Q2MZ2_WIDOW_BLASTER_SWEEP6		161
#define	Q2MZ2_WIDOW_BLASTER_SWEEP7		162
#define	Q2MZ2_WIDOW_BLASTER_SWEEP8		163
#define	Q2MZ2_WIDOW_BLASTER_SWEEP9		164
#define	Q2MZ2_WIDOW_BLASTER_100			165
#define	Q2MZ2_WIDOW_BLASTER_90			166
#define	Q2MZ2_WIDOW_BLASTER_80			167
#define	Q2MZ2_WIDOW_BLASTER_70			168
#define	Q2MZ2_WIDOW_BLASTER_60			169
#define	Q2MZ2_WIDOW_BLASTER_50			170
#define	Q2MZ2_WIDOW_BLASTER_40			171
#define	Q2MZ2_WIDOW_BLASTER_30			172
#define	Q2MZ2_WIDOW_BLASTER_20			173
#define	Q2MZ2_WIDOW_BLASTER_10			174
#define	Q2MZ2_WIDOW_BLASTER_0				175
#define	Q2MZ2_WIDOW_BLASTER_10L			176
#define	Q2MZ2_WIDOW_BLASTER_20L			177
#define	Q2MZ2_WIDOW_BLASTER_30L			178
#define	Q2MZ2_WIDOW_BLASTER_40L			179
#define	Q2MZ2_WIDOW_BLASTER_50L			180
#define	Q2MZ2_WIDOW_BLASTER_60L			181
#define	Q2MZ2_WIDOW_BLASTER_70L			182
#define	Q2MZ2_WIDOW_RUN_1					183
#define	Q2MZ2_WIDOW_RUN_2					184
#define	Q2MZ2_WIDOW_RUN_3					185
#define	Q2MZ2_WIDOW_RUN_4					186
#define	Q2MZ2_WIDOW_RUN_5					187
#define	Q2MZ2_WIDOW_RUN_6					188
#define	Q2MZ2_WIDOW_RUN_7					189
#define	Q2MZ2_WIDOW_RUN_8					190
#define	Q2MZ2_CARRIER_ROCKET_1			191
#define	Q2MZ2_CARRIER_ROCKET_2			192
#define	Q2MZ2_CARRIER_ROCKET_3			193
#define	Q2MZ2_CARRIER_ROCKET_4			194
#define	Q2MZ2_WIDOW2_BEAMER_1				195
#define	Q2MZ2_WIDOW2_BEAMER_2				196
#define	Q2MZ2_WIDOW2_BEAMER_3				197
#define	Q2MZ2_WIDOW2_BEAMER_4				198
#define	Q2MZ2_WIDOW2_BEAMER_5				199
#define	Q2MZ2_WIDOW2_BEAM_SWEEP_1			200
#define	Q2MZ2_WIDOW2_BEAM_SWEEP_2			201
#define	Q2MZ2_WIDOW2_BEAM_SWEEP_3			202
#define	Q2MZ2_WIDOW2_BEAM_SWEEP_4			203
#define	Q2MZ2_WIDOW2_BEAM_SWEEP_5			204
#define	Q2MZ2_WIDOW2_BEAM_SWEEP_6			205
#define	Q2MZ2_WIDOW2_BEAM_SWEEP_7			206
#define	Q2MZ2_WIDOW2_BEAM_SWEEP_8			207
#define	Q2MZ2_WIDOW2_BEAM_SWEEP_9			208
#define	Q2MZ2_WIDOW2_BEAM_SWEEP_10		209
#define	Q2MZ2_WIDOW2_BEAM_SWEEP_11		210






#define MAX_MAP_AREA_BYTES		32

// edict->drawflags (hexen2 stuff)
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


//TENEBRAE_GFX_DLIGHTS
#define PFLAGS_NOSHADOW		1
#define PFLAGS_CORONA		2
#define PFLAGS_FULLDYNAMIC	128

#define RENDER_STEP 1
#define RENDER_GLOWTRAIL 2
#define RENDER_VIEWMODEL 4
#define RENDER_EXTERIORMODEL 8
#define RENDER_LOWPRECISION 16 // send as low precision coordinates to save bandwidth
#define RENDER_COLORMAPPED 32
//#define RENDER_INDIRECT 64
#define RENDER_SHADOW 65536 // cast shadow
#define RENDER_LIGHT 131072 // receive light
#define RENDER_TRANSPARENT 262144 // can't light during opaque stage

//darkplaces protocols 5 to 7 use these
// reset all entity fields (typically used if status changed)
#define E5_FULLUPDATE (1<<0)
// E5_ORIGIN32=0: short[3] = s->origin[0] * 8, s->origin[1] * 8, s->origin[2] * 8
// E5_ORIGIN32=1: float[3] = s->origin[0], s->origin[1], s->origin[2]
#define E5_ORIGIN (1<<1)
// E5_ANGLES16=0: byte[3] = s->angle[0] * 256 / 360, s->angle[1] * 256 / 360, s->angle[2] * 256 / 360
// E5_ANGLES16=1: short[3] = s->angle[0] * 65536 / 360, s->angle[1] * 65536 / 360, s->angle[2] * 65536 / 360
#define E5_ANGLES (1<<2)
// E5_MODEL16=0: byte = s->modelindex
// E5_MODEL16=1: short = s->modelindex
#define E5_MODEL (1<<3)
// E5_FRAME16=0: byte = s->frame
// E5_FRAME16=1: short = s->frame
#define E5_FRAME (1<<4)
// byte = s->skin
#define E5_SKIN (1<<5)
// E5_EFFECTS16=0 && E5_EFFECTS32=0: byte = s->effects
// E5_EFFECTS16=1 && E5_EFFECTS32=0: short = s->effects
// E5_EFFECTS16=0 && E5_EFFECTS32=1: int = s->effects
// E5_EFFECTS16=1 && E5_EFFECTS32=1: int = s->effects
#define E5_EFFECTS (1<<6)
// bits >= (1<<8)
#define E5_EXTEND1 (1<<7)

// byte = s->renderflags
#define E5_FLAGS (1<<8)
// byte = bound(0, s->alpha * 255, 255)
#define E5_ALPHA (1<<9)
// byte = bound(0, s->scale * 16, 255)
#define E5_SCALE (1<<10)
// flag
#define E5_ORIGIN32 (1<<11)
// flag
#define E5_ANGLES16 (1<<12)
// flag
#define E5_MODEL16 (1<<13)
// byte = s->colormap
#define E5_COLORMAP (1<<14)
// bits >= (1<<16)
#define E5_EXTEND2 (1<<15)

// short = s->tagentity
// byte = s->tagindex
#define E5_ATTACHMENT (1<<16)
// short[4] = s->light[0], s->light[1], s->light[2], s->light[3]
// byte = s->lightstyle
// byte = s->lightpflags
#define E5_LIGHT (1<<17)
// byte = s->glowsize
// byte = s->glowcolor
#define E5_GLOW (1<<18)
// short = s->effects
#define E5_EFFECTS16 (1<<19)
// int = s->effects
#define E5_EFFECTS32 (1<<20)
// flag
#define E5_FRAME16 (1<<21)
// unused
#define E5_COLORMOD (1<<22)
// bits >= (1<<24)
#define E5_EXTEND3 (1<<23)

// unused
#define E5_UNUSED24 (1<<24)
// unused
#define E5_UNUSED25 (1<<25)
// unused
#define E5_UNUSED26 (1<<26)
// unused
#define E5_UNUSED27 (1<<27)
// unused
#define E5_UNUSED28 (1<<28)
// unused
#define E5_UNUSED29 (1<<29)
// unused
#define E5_UNUSED30 (1<<30)
// bits2 > 0
#define E5_EXTEND4 (1<<31)

#define E5_ALLUNUSED (E5_UNUSED24|E5_UNUSED25|E5_UNUSED26|E5_UNUSED27|E5_UNUSED28|E5_UNUSED29|E5_UNUSED30)

#define FITZB_LARGEMODEL	(1<<0)	// modelindex is short instead of byte
#define FITZB_LARGEFRAME	(1<<1)	// frame is short instead of byte
#define FITZB_ALPHA			(1<<2)	// 1 byte, uses ENTALPHA_ENCODE, not sent if ENTALPHA_DEFAULT
