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
// cl_parse.c  -- parse a message received from the server

#include "quakedef.h"

void CL_GetNumberedEntityInfo (int num, float *org, float *ang);
void CLNQ_ParseDarkPlaces5Entities(void);
void CL_SetStat (int pnum, int stat, int value);

void R_ParseParticleEffect2 (void);
void R_ParseParticleEffect3 (void);
void R_ParseParticleEffect4 (void);

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


int nq_dp_protocol;



char *svc_strings[] =
{
	"svc_bad",
	"svc_nop",
	"svc_disconnect",
	"svc_updatestat",
	"svc_version",		// [long] server version
	"svc_setview",		// [short] entity number
	"svc_sound",			// <see code>
	"svc_time",			// [float] server time
	"svc_print",			// [string] null terminated string
	"svc_stufftext",		// [string] stuffed into client's console buffer
						// the string should be \n terminated
	"svc_setangle",		// [vec3] set the view angle to this absolute value
	
	"svc_serverdata",		// [long] version ...
	"svc_lightstyle",		// [qbyte] [string]
	"svc_updatename",		// [qbyte] [string]
	"svc_updatefrags",	// [qbyte] [short]
	"svc_clientdata",		// <shortbits + data>
	"svc_stopsound",		// <see code>
	"svc_updatecolors",	// [qbyte] [qbyte]
	"svc_particle",		// [vec3] <variable>
	"svc_damage",			// [qbyte] impact [qbyte] blood [vec3] from
	
	"svc_spawnstatic",
	"svc_spawnstatic2",
	"svc_spawnbaseline",
	
	"svc_temp_entity",		// <variable>
	"svc_setpause",
	"svc_signonnum",
	"svc_centerprint",
	"svc_killedmonster",
	"svc_foundsecret",
	"svc_spawnstaticsound",
	"svc_intermission",
	"svc_finale",

	"svc_cdtrack",
	"svc_sellscreen",

	"svc_smallkick",
	"svc_bigkick",

	"svc_updateping",
	"svc_updateentertime",

	"svc_updatestatlong",
	"svc_muzzleflash",
	"svc_updateuserinfo",
	"svc_download",
	"svc_playerinfo",
	"svc_nails",
	"svc_choke",
	"svc_modellist",
	"svc_soundlist",
	"svc_packetentities",
 	"svc_deltapacketentities",
	"svc_maxspeed",
	"svc_entgravity",

	"svc_setinfo",
	"svc_serverinfo",
	"svc_updatepl",
	"NEW svc_nails2",
	"OBSOLETE",
	"NEW svc_view2",
	"NEW svc_lightstylecol",
	"NEW svc_bulletentext",
	"NEW svc_lightnings",
	"NEW svc_modellistshort",
	"NEW svc_ftesetclientpersist",
	"NEW svc_setportalstate",
	"NEW svc_particle2",
	"NEW svc_particle3",
	"NEW svc_particle4",
	"NEW svc_spawnbaseline2",
	"NEW svc_customtempent",
	"NEW svc_choosesplitclient",
	"NEW PROTOCOL"
};

char *svc_nqstrings[] =
{
	"nqsvc_bad",
	"nqsvc_nop",
	"nqsvc_disconnect",
	"nqsvc_updatestat",
	"nqsvc_version",		// [long] server version
	"nqsvc_setview",		// [short] entity number
	"nqsvc_sound",			// <see code>
	"nqsvc_time",			// [float] server time
	"nqsvc_print",			// [string] null terminated string
	"nqsvc_stufftext",		// [string] stuffed into client's console buffer
						// the string should be \n terminated
	"nqsvc_setangle",		// [vec3] set the view angle to this absolute value

	"nqsvc_serverinfo",		// [long] version
						// [string] signon string
						// [string]..[0]model cache [string]...[0]sounds cache
						// [string]..[0]item cache
	"nqsvc_lightstyle",		// [qbyte] [string]
	"nqsvc_updatename",		// [qbyte] [string]
	"nqsvc_updatefrags",	// [qbyte] [short]
	"nqsvc_clientdata",		// <shortbits + data>
	"nqsvc_stopsound",		// <see code>
	"nqsvc_updatecolors",	// [qbyte] [qbyte]
	"nqsvc_particle",		// [vec3] <variable>
	"nqsvc_damage",			// [qbyte] impact [qbyte] blood [vec3] from

	"nqsvc_spawnstatic",
	"nqOBSOLETE svc_spawnbinary",
	"nqsvc_spawnbaseline",

	"nqsvc_temp_entity",		// <variable>
	"nqsvc_setpause",
	"nqsvc_signonnum",
	"nqsvc_centerprint",
	"nqsvc_killedmonster",
	"nqsvc_foundsecret",
	"nqsvc_spawnstaticsound",
	"nqsvc_intermission",
	"nqsvc_finale",			// [string] music [string] text
	"nqsvc_cdtrack",			// [qbyte] track [qbyte] looptrack
	"nqsvc_sellscreen",
	"nqsvc_cutscene",

	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL"
};

extern cvar_t requiredownloads, cl_standardchat;
int	oldparsecountmod;
int	parsecountmod;
double	parsecounttime;

int		cl_spikeindex, cl_playerindex, cl_h_playerindex, cl_flagindex, cl_rocketindex, cl_grenadeindex, cl_gib1index, cl_gib2index, cl_gib3index;

#ifdef PEXT_LIGHTUPDATES
int		cl_lightningindex;
#endif

//=============================================================================

int packet_latency[NET_TIMINGS];

int CL_CalcNet (void)
{
	int		a, i;
	frame_t	*frame;
	int lost;
//	char st[80];

	for (i=cls.netchan.outgoing_sequence-UPDATE_BACKUP+1
		; i <= cls.netchan.outgoing_sequence
		; i++)
	{
		frame = &cl.frames[i&UPDATE_MASK];
		if (frame->receivedtime == -1)
			packet_latency[i&NET_TIMINGSMASK] = 9999;	// dropped
		else if (frame->receivedtime == -2)
			packet_latency[i&NET_TIMINGSMASK] = 10000;	// choked
		else if (frame->invalid)
			packet_latency[i&NET_TIMINGSMASK] = 9998;	// invalid delta
		else
			packet_latency[i&NET_TIMINGSMASK] = (frame->receivedtime - frame->senttime)*20;
	}

	lost = 0;
	for (a=0 ; a<NET_TIMINGS ; a++)
	{
		i = (cls.netchan.outgoing_sequence-a) & NET_TIMINGSMASK;
		if (packet_latency[i] == 9999)
			lost++;
	}
	return lost * 100 / NET_TIMINGS;
}

//=============================================================================

//note: this will overwrite existing files.
//returns true if the download is going to be downloaded after the call.
qboolean CL_EnqueDownload(char *filename, qboolean verbose, qboolean ignorefailedlist)
{
	downloadlist_t *dl;
	if (strchr(filename, '\\') || strchr(filename, ':') || strstr(filename, ".."))
	{
		Con_Printf("Denying download of \"%s\"\n", filename);
		return false;
	}

	if (cls.demoplayback)
		return false;

	if (!ignorefailedlist)
	{
		for (dl = cl.faileddownloads; dl; dl = dl->next)	//yeah, so it failed... Ignore it.
		{
			if (!strcmp(dl->name, filename))
			{
				if (verbose)
					Con_Printf("We've failed to download \"%s\" already\n", filename);
				return false;
			}
		}
	}

	for (dl = cl.downloadlist; dl; dl = dl->next)	//It's already on our list. Ignore it.
	{
		if (!strcmp(dl->name, filename))
		{
			if (verbose)
				Con_Printf("Already waiting for \"%s\"\n", filename);
			return true;
		}
	}

	if (!strcmp(cls.downloadname, filename))
	{
		if (verbose)
			Con_Printf("Already downloading \"%s\"\n", filename);
		return true;
	}

	dl = Z_Malloc(sizeof(downloadlist_t));
	strcpy(dl->name, filename);
	dl->next = cl.downloadlist;
	cl.downloadlist = dl;

	if (verbose)
		Con_Printf("Enqued download of \"%s\"\n", filename);

	return true;
}

void CL_DisenqueDownload(char *filename)
{
	downloadlist_t *dl, *nxt;
	if(cl.downloadlist)	//remove from enqued download list
	{
		if (!strcmp(cl.downloadlist->name, filename))
		{
			dl = cl.downloadlist;
			cl.downloadlist = cl.downloadlist->next;
			Z_Free(dl);
		}
		else
		{
			for (dl = cl.downloadlist->next; dl->next; dl = dl->next)
			{
				if (!strcmp(dl->next->name, filename))
				{
					nxt = dl->next->next;
					Z_Free(dl->next);
					dl->next = nxt;
					break;
				}
			}
		}
	}
}

void CL_SendDownloadRequest(char *filename)
{
	strcpy (cls.downloadname, filename);
	Con_TPrintf (TL_DOWNLOADINGFILE, cls.downloadname);

	// download to a temp name, and only rename
	// to the real name when done, so if interrupted
	// a runt file wont be left
	COM_StripExtension (cls.downloadname, cls.downloadtempname);
	strcat (cls.downloadtempname, ".tmp");

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	MSG_WriteString (&cls.netchan.message, va("download %s", cls.downloadname));

	//prevent ftp/http from changing stuff
	cls.downloadmethod = DL_QWPENDING;
	cls.downloadpercent = 0;

	CL_DisenqueDownload(filename);
}

//Do any reloading for the file that just reloaded.
void CL_FinishDownload(char *filename, char *tempname)
{
	int i;
	extern int mod_numknown;
	extern model_t	mod_known[];

	COM_RefreshFSCache_f();

	cls.downloadmethod = DL_NONE;

	// rename the temp file to it's final name
	if (tempname)
	{
		char oldn[MAX_OSPATH], newn[MAX_OSPATH];
		if (strcmp(tempname, filename))
		{
			if (strncmp(tempname,"skins/",6))
			{
				sprintf (oldn, "%s/%s", com_gamedir, tempname);
				sprintf (newn, "%s/%s", com_gamedir, filename);
			}
			else
			{
				sprintf (oldn, "qw/%s", tempname);
				sprintf (newn, "qw/%s", filename);
			}
			if (rename (oldn, newn))
				Con_TPrintf (TL_RENAMEFAILED);
		}
	}

	if (!strcmp(filename, "gfx/palette.lmp"))
	{
		Cbuf_AddText("vid_restart\n", RESTRICT_LOCAL);
	}
	else
	{
		for (i = 0; i < mod_numknown; i++)	//go and load this model now.
		{
			if (!strcmp(mod_known[i].name, filename))
			{
				Mod_ForName(mod_known[i].name, false);	//throw away result.
				break;
			}
		}
		Skin_FlushSkin(filename);
	}
}

/*
===============
CL_CheckOrDownloadFile

Returns true if the file exists, otherwise it attempts
to start a download from the server.
===============
*/
qboolean	CL_CheckOrDownloadFile (char *filename, int nodelay)
{
	if (strstr (filename, ".."))
	{
		Con_TPrintf (TL_NORELATIVEPATHS);
		return true;
	}
	
	if (COM_FCheckExists (filename))
	{	// it exists, no need to download
		return true;
	}

	//ZOID - can't download when recording
	if (cls.demorecording)
	{
		Con_TPrintf (TL_NODOWNLOADINDEMO, filename);
		return true;
	}
	//ZOID - can't download when playback
	if (cls.demoplayback)
		return true;

	SCR_EndLoadingPlaque();	//release console.

	if (CL_EnqueDownload(filename, false, false))
		return !nodelay;
	else
		return true;
}


qboolean CL_CheckMD2Skins (char *name)
{
	md2_t *pheader;
	qbyte *precache_model;
	int precache_model_skin = 1;

	// checking for skins in the model
	precache_model = COM_LoadMallocFile (name);
	if (!precache_model) {
		precache_model_skin = 0;
		return false; // couldn't load it
	}
	if (LittleLong(*(unsigned *)precache_model) != MD2IDALIASHEADER) {
		// not an alias model
		BZ_Free(precache_model);
		precache_model = 0;
		precache_model_skin = 0;
		return false;
	}
	pheader = (md2_t *)precache_model;
	if (LittleLong (pheader->version) != MD2ALIAS_VERSION) {
		BZ_Free(precache_model);
		precache_model = 0;
		precache_model_skin = 0;
		return false;
	}

	pheader = (md2_t *)precache_model;

	while (precache_model_skin - 1 < LittleLong(pheader->num_skins)) {
		if (!CL_CheckOrDownloadFile((char *)precache_model +
			LittleLong(pheader->ofs_skins) + 
			(precache_model_skin - 1)*MD2MAX_SKINNAME, false)) {
			precache_model_skin++;

			BZ_Free(precache_model);
			precache_model=NULL;
			return true; // started a download
		}
		precache_model_skin++;
	}
	if (precache_model) { 
		BZ_Free(precache_model);
		precache_model=NULL;
	}
	precache_model_skin = 0;
	
	return false;
}
/*
=================
Model_NextDownload
=================
*/
void Sound_NextDownload (void);
void Model_NextDownload (void)
{
//	char *twf;
	char	*s;
	int		i;
	extern	char gamedirfile[];

	if (cls.downloadnumber == 0)
	{
		Con_TPrintf (TLC_CHECKINGMODELS);
		cls.downloadnumber = 1;

		cl.worldmodel = NULL;
	}
#ifdef Q2CLIENT
	if (cls.q2server)
	{
		R_SetSky(cl.skyname, 0, r_origin);
		for (i = 0; i < Q2MAX_IMAGES; i++)
		{
			char picname[256];
			if (!*cl.image_name[i])
				continue;
			sprintf(picname, "pics/%s.pcx", cl.image_name[i]);
			if (!CL_CheckOrDownloadFile(picname, false))
				return;
		}
		if (!CLQ2_RegisterTEntModels())
			return;
	}
#endif

	cls.downloadtype = dl_model;

	for ( 
		; cl.model_name[cls.downloadnumber][0]
		; cls.downloadnumber++)
	{
		s = cl.model_name[cls.downloadnumber];
		if (s[0] == '*')
			continue;	// inline brush model

		if (!stricmp(COM_FileExtension(s), "dsp"))	//doom sprites are weird, and not really downloadable via this system
			continue;

#ifdef Q2CLIENT
		if (cls.q2server && s[0] == '#')	//this is a vweap
			continue;
#endif

		if (!CL_CheckOrDownloadFile(s, cls.downloadnumber==1))	//world is required to be loaded.
			return;		// started a download

		if (strstr(s, ".md2"))
			if (CL_CheckMD2Skins(s))
				return;
	}
	
	if (cl.playernum[0] == -1)
	{	//q2 cinematic - don't load the models.
		cl.worldmodel = cl.model_precache[1] = Mod_ForName ("", false);
	}
	else if (!cl.worldmodel)
	{
		for (i=1 ; i<MAX_MODELS ; i++)
		{
			if (!cl.model_name[i][0])
				break;
				
			Hunk_Check();

			cl.model_precache[i] = NULL;
			cl.model_precache[i] = Mod_ForName (cl.model_name[i], false);
			
			Hunk_Check();

			if (!cl.model_precache[i] || (i == 1 && (cl.model_precache[i]->type == mod_dummy || cl.model_precache[i]->needload)))
			{
				Con_TPrintf (TL_FILE_X_MISSING, cl.model_name[i]);
				Con_TPrintf (TL_GETACLIENTPACK, gamedirfile);
				CL_Disconnect ();
				return;
			}

			S_ExtraUpdate();
		}

		cl.worldmodel = cl.model_precache[1];
	}

	if (!R_CheckSky())
		return;
	if (!Wad_NextDownload())	//world is required to be loaded.
		return;		// started a download

	{
		extern model_t *loadmodel;
		loadmodel = cl.worldmodel;
		if (!cl.worldmodel)
			Host_EndGame("No worldmodel was loaded\n");

		if (R_PreNewMap)
			R_PreNewMap();

		Mod_NowLoadExternal();
	}

	// all done
	R_NewMap ();
	Hunk_Check ();		// make sure nothing is hurt
#ifdef Q2CLIENT
	if (cls.q2server)
	{
		cls.downloadnumber = 0;
		Skin_NextDownload();
	}
	else
#endif
	{
	// done with modellist, request first of static signon messages
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
//		MSG_WriteString (&cls.netchan.message, va("prespawn %i 0 %i", cl.servercount, cl.worldmodel->checksum2));
		MSG_WriteString (&cls.netchan.message, va(prespawn_name, cl.servercount, cl.worldmodel->checksum2));
	}
}

/*
=================
Sound_NextDownload
=================
*/
void Sound_NextDownload (void)
{
	char	*s;
	int		i;

	if (cls.downloadnumber == 0)
	{
		Con_TPrintf (TLC_CHECKINGSOUNDS);
		cls.downloadnumber = 1;
	}

	cls.downloadtype = dl_sound;
	for ( 
		; cl.sound_name[cls.downloadnumber][0]
		; cls.downloadnumber++)
	{
		s = cl.sound_name[cls.downloadnumber];
		if (*s == '*')
			continue;
		if (!CL_CheckOrDownloadFile(va("sound/%s",s), false))
			return;		// started a download
	}

	for (i=1 ; i<MAX_SOUNDS ; i++)
	{
		if (!cl.sound_name[i][0])
			break;
		cl.sound_precache[i] = S_PrecacheSound (cl.sound_name[i]);

		S_ExtraUpdate();
	}

	// done with sounds, request models now
	memset (cl.model_precache, 0, sizeof(cl.model_precache));
	cl_playerindex = -1;
	cl_h_playerindex = -1;
	cl_spikeindex = -1;
	cl_flagindex = -1;
	cl_rocketindex = -1;
	cl_grenadeindex = -1;
	cl_gib1index = -1;
	cl_gib2index = -1;
	cl_gib3index = -1;
#ifdef PEXT_LIGHTUPDATES
	cl_lightningindex = -1;
#endif
#ifdef Q2CLIENT
	if (cls.q2server)
	{
		cls.downloadnumber = 0;
		Model_NextDownload();
	}
	else
#endif
	{
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
//		MSG_WriteString (&cls.netchan.message, va("modellist %i 0", cl.servercount));
		MSG_WriteString (&cls.netchan.message, va(modellist_name, cl.servercount, 0));
	}
}


/*
======================
CL_RequestNextDownload
======================
*/
void CL_RequestNextDownload (void)
{
	if (cl.downloadlist)
	{
		if (!COM_FCheckExists (cl.downloadlist->name))
			CL_SendDownloadRequest(cl.downloadlist->name);
		else
		{
			Con_Printf("Already have %s\n", cl.downloadlist->name);
			CL_DisenqueDownload(cl.downloadlist->name);
		}
		return;
	}
	switch (cls.downloadtype)
	{
	case dl_single:
	case dl_singlestuffed:
		break;
	case dl_skin:
		Skin_NextDownload ();
		break;
	case dl_model:
		Model_NextDownload ();
		break;
	case dl_sound:
		Sound_NextDownload ();
		break;
	case dl_none:
	default:
		Con_DPrintf("Unknown download type.\n");
	}
}

int CL_RequestADownloadChunk(void);
void CL_SendDownloadReq(sizebuf_t *msg)
{
	if (cl.downloadlist && !cls.downloadmethod)
	{
		CL_RequestNextDownload();
		return;
	}

#ifdef PEXT_CHUNKEDDOWNLOADS
	if (cls.downloadmethod == DL_QWCHUNKS)
	{
		int i = CL_RequestADownloadChunk();
		if (i < 0)
		{
			//we can stop downloading now.
		}
		else
		{
			MSG_WriteByte(msg, clc_stringcmd);
			MSG_WriteString(msg, va("nextdl %i\n", i));
		}
		return;
	}
#endif
}

#ifdef PEXT_ZLIBDL
#ifdef _WIN32
#define ZEXPORT VARGS
#include "../../zip/zlib.h"

//# pragma comment (lib, "zip/zlib.lib") 
#else
#include <zlib.h>
#endif

char *ZLibDownloadDecode(int *messagesize, char *input, int finalsize)
{
	char *outbuf = Hunk_TempAlloc(finalsize);
	z_stream zs;

	*messagesize = (*(short*)input);
	input+=2;

	if (!*messagesize)
	{
		*messagesize = finalsize+2;
		return input;
	}

	memset(&zs, 0, sizeof(zs));


	zs.next_in = input;
    zs.avail_in = *messagesize;	//tell it that it has a lot. Possibly a bad idea.
    zs.total_in = 0;

    zs.next_out = outbuf;
    zs.avail_out = finalsize;	//this is the limiter.
    zs.total_out = 0;

    zs.data_type = Z_BINARY;

	inflateInit(&zs);
	inflate(&zs, Z_FINISH);	//decompress it in one go.
	inflateEnd(&zs);

	*messagesize = zs.total_in+2;
	return outbuf;
}
#endif

void CL_DownloadFailed(char *name)
{
	//add this to our failed list. (so we don't try downloading it again...)
	downloadlist_t *failed;
	failed = Z_Malloc(sizeof(downloadlist_t));
	failed->next = cl.faileddownloads;
	cl.faileddownloads = failed;
	Q_strncpyz(failed->name, name, sizeof(failed->name));

	cls.downloadmethod = DL_NONE;
}

float downloadstarttime;
#ifdef PEXT_CHUNKEDDOWNLOADS
#define MAXBLOCKS 64	//must be power of 2
#define DLBLOCKSIZE 1024
int downloadsize;
int receivedbytes;
int recievedblock[MAXBLOCKS];
int firstblock;
int blockcycle;
void CL_ParseChunkedDownload(void)
{
	qbyte	*svname;
	qbyte	osname[MAX_OSPATH];
	int totalsize;
	int chunknum;
	char data[DLBLOCKSIZE];

	chunknum = MSG_ReadLong();
	if (chunknum < 0)
	{
		totalsize = MSG_ReadLong();
		svname = MSG_ReadString();
		if (cls.demoplayback)
			return;

		if (totalsize < 0)
		{
			if (totalsize == -2)
				Con_Printf("Server permissions deny downloading file %s\n", svname);
			else
				Con_Printf("Couldn't find file %s on the server\n", svname);

			CL_DownloadFailed(svname);

			CL_RequestNextDownload();
			return;
		}

		if (cls.downloadmethod == DL_QWCHUNKS)
			Host_EndGame("Received second download - \"%s\"\n", svname);

		//start the new download
		cls.downloadmethod = DL_QWCHUNKS;
		cls.downloadpercent = 0;
		downloadsize = totalsize;

		downloadstarttime = Sys_DoubleTime();

		strcpy(cls.downloadname, svname);
		COM_StripExtension(svname, cls.downloadtempname);
		COM_DefaultExtension(cls.downloadtempname, ".tmp");

		if (!strncmp(cls.downloadtempname,"skins/",6))
			sprintf (osname, "qw/%s", cls.downloadtempname);	//skins go to quake/qw/skins. never quake/gamedir/skins (blame id)
		else
			sprintf (osname, "%s/%s", com_gamedir, cls.downloadtempname);
		COM_CreatePath (osname);
		cls.downloadqw = fopen (osname, "wb");

		firstblock = 0;
		receivedbytes = 0;
		blockcycle = -1;	//so it requests 0 first. :)
		memset(recievedblock, 0, sizeof(recievedblock));
		return;
	}

//	Con_Printf("Received dl block %i: ", chunknum);

	MSG_ReadData(data, DLBLOCKSIZE);

	if (cls.demoplayback)
	{	//err, yeah, when playing demos we don't actually pay any attention to this.
		return;
	}
	if (chunknum < firstblock)
	{
//		Con_Printf("too old\n", chunknum);
		return;
	}
	if (chunknum-firstblock >= MAXBLOCKS)
	{
//		Con_Printf("^1too new!\n", chunknum);
		return;
	}
	
	if (recievedblock[chunknum&(MAXBLOCKS-1)])
	{
//		Con_Printf("duplicated\n", chunknum);
		return;
	}
//	Con_Printf("usable\n", chunknum);
	receivedbytes+=DLBLOCKSIZE;
	recievedblock[chunknum&(MAXBLOCKS-1)] = true;

	while(recievedblock[firstblock&(MAXBLOCKS-1)])
	{
		recievedblock[firstblock&(MAXBLOCKS-1)] = false;
		firstblock++;
	}

	fseek(cls.downloadqw, chunknum*DLBLOCKSIZE, SEEK_SET);
	if (downloadsize - chunknum*DLBLOCKSIZE < DLBLOCKSIZE)	//final block is actually meant to be smaller than we recieve.
		fwrite(data, 1, downloadsize - chunknum*DLBLOCKSIZE, cls.downloadqw);
	else
		fwrite(data, 1, DLBLOCKSIZE, cls.downloadqw);

	cls.downloadpercent = receivedbytes/(float)downloadsize*100;
}

int CL_RequestADownloadChunk(void)
{
	int i;
	int b;

	if (cls.downloadmethod != DL_QWCHUNKS)
	{
		Con_Printf("download not initiated\n");
		return 0;
	}

	blockcycle++;
	for (i = 0; i < MAXBLOCKS; i++)
	{
		b = ((i+blockcycle)&(MAXBLOCKS-1))
			+ firstblock;
		if (!recievedblock[b&(MAXBLOCKS-1)])	//don't ask for ones we've already got.
		{
			if (b >= (downloadsize+DLBLOCKSIZE-1)/DLBLOCKSIZE)	//don't ask for blocks that are over the size of the file.
				continue;
//			Con_Printf("Requesting block %i\n", b);
			return b;
		}
	}

//	Con_Printf("^1 EOF?\n");

	fclose(cls.downloadqw);
	CL_FinishDownload(cls.downloadname, cls.downloadtempname);

	Con_Printf("Download took %i seconds\n", (int)(Sys_DoubleTime() - downloadstarttime));

	*cls.downloadname = '\0';
	cls.downloadqw = NULL;
	cls.downloadpercent = 0;

	return -1;
}

#endif

/*
=====================
CL_ParseDownload

A download message has been received from the server
=====================
*/
void CL_ParseDownload (void)
{
	int		size, percent;
	qbyte	name[1024];

#ifdef PEXT_CHUNKEDDOWNLOADS
	if (cls.fteprotocolextensions & PEXT_CHUNKEDDOWNLOADS)
	{
		CL_ParseChunkedDownload();
		return;
	}
#endif

	// read the data
	size = MSG_ReadShort ();
	percent = MSG_ReadByte ();

	if (cls.demoplayback)
	{
		if (size > 0)
			msg_readcount += size;
		return; // not in demo playback
	}

	if (!*cls.downloadname)	//huh... that's not right...
	{
		Con_Print("^1Warning: Server sending unknown file.\n");
		strcpy(cls.downloadname, "unknown.txt");
		strcpy(cls.downloadtempname, "unknown.tmp");
	}

	if (size < 0)
	{
		Con_TPrintf (TL_FILENOTFOUND);
		if (cls.downloadqw)
		{
			Con_TPrintf (TL_CLS_DOWNLOAD_ISSET);
			fclose (cls.downloadqw);
			cls.downloadqw = NULL;
		}
		if (cl.downloadlist && !strcmp(cl.downloadlist->name, cls.downloadname))
		{
			downloadlist_t *next;
			next = cl.downloadlist->next;
			Z_Free(cl.downloadlist);
			cl.downloadlist = next;
		}

		CL_DownloadFailed(cls.downloadname);

		CL_RequestNextDownload ();
		return;
	}

	// open the file if not opened yet
	if (!cls.downloadqw)
	{
		if (strncmp(cls.downloadtempname,"skins/",6))
			sprintf (name, "%s/%s", com_gamedir, cls.downloadtempname);
		else
			sprintf (name, "qw/%s", cls.downloadtempname);

		COM_CreatePath (name);

		cls.downloadqw = fopen (name, "wb");
		if (!cls.downloadqw)
		{
			msg_readcount += size;
			Con_TPrintf (TL_FAILEDTOOPEN, cls.downloadtempname);
			CL_RequestNextDownload ();
			return;
		}

		downloadstarttime = Sys_DoubleTime();
		SCR_EndLoadingPlaque();
	}
#ifdef PEXT_ZLIBDL
	if (percent >= 101 && percent <= 201)// && cls.fteprotocolextensions & PEXT_ZLIBDL)
	{
		int compsize;

		percent = percent - 101;

		fwrite (ZLibDownloadDecode(&compsize, net_message.data + msg_readcount, size), 1, size, cls.download);

		msg_readcount += compsize;
	}
	else
#endif
	{
		fwrite (net_message.data + msg_readcount, 1, size, cls.downloadqw);
		msg_readcount += size;
	}

	if (cls.downloadmethod == DL_QWPENDING)
		cls.downloadmethod = DL_QW;

	if (percent != 100)
	{
// change display routines by zoid
		// request next block
		cls.downloadpercent = percent;

		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, "nextdl");
	}
	else
	{
		fclose (cls.downloadqw);

		CL_FinishDownload(cls.downloadname, cls.downloadtempname);
		*cls.downloadname = '\0';
		cls.downloadqw = NULL;
		cls.downloadpercent = 0;

		Con_Printf("Download took %i seconds\n", (int)(Sys_DoubleTime() - downloadstarttime));

		// get another file if needed

		CL_RequestNextDownload ();
	}
}

static qbyte *upload_data;
static int upload_pos;
static int upload_size;

void CL_NextUpload(void)
{
	qbyte	buffer[1024];
	int		r;
	int		percent;
	int		size;

	if (!upload_data)
		return;

	r = upload_size - upload_pos;
	if (r > 768)
		r = 768;
	memcpy(buffer, upload_data + upload_pos, r);
	MSG_WriteByte (&cls.netchan.message, clc_upload);
	MSG_WriteShort (&cls.netchan.message, r);

	upload_pos += r;
	size = upload_size;
	if (!size)
		size = 1;
	percent = upload_pos*100/size;
	MSG_WriteByte (&cls.netchan.message, percent);
	SZ_Write (&cls.netchan.message, buffer, r);

Con_DPrintf ("UPLOAD: %6d: %d written\n", upload_pos - r, r);

	if (upload_pos != upload_size)
		return;

	Con_TPrintf (TL_UPLOADCOMPLEATE);

	BZ_Free(upload_data);
	upload_data = 0;
	upload_pos = upload_size = 0;
}

void CL_StartUpload (qbyte *data, int size)
{
	if (cls.state < ca_onserver)
		return; // gotta be connected

	// override
	if (upload_data)
		BZ_Free(upload_data);

Con_DPrintf("Upload starting of %d...\n", size);

	upload_data = BZ_Malloc(size);
	memcpy(upload_data, data, size);
	upload_size = size;
	upload_pos = 0;

	CL_NextUpload();
} 

qboolean CL_IsUploading(void)
{
	if (upload_data)
		return true;
	return false;
}

void CL_StopUpload(void)
{
	if (upload_data)
		BZ_Free(upload_data);
	upload_data = NULL;
}

/*
=====================================================================

  SERVER CONNECTING MESSAGES

=====================================================================
*/
#ifdef CLIENTONLY
float nextdemotime;
#endif

/*
==================
CL_ParseServerData
==================
*/
void CL_ParseServerData (void)
{
	int pnum;
	int clnum;
	char	*str;
	int protover, svcnt;

	float maxspeed, entgrav;
	
	Con_DPrintf ("Serverdata packet received.\n");
//
// wipe the client_state_t struct
//	

	SCR_BeginLoadingPlaque();

// parse protocol version number
// allow 2.2 and 2.29 demos to play
#ifdef PROTOCOL_VERSION_FTE
	cls.fteprotocolextensions=0;
	for(;;)
	{
		protover = MSG_ReadLong ();
		if (protover == PROTOCOL_VERSION_FTE)
		{
			cls.fteprotocolextensions =  MSG_ReadLong();
			Con_TPrintf (TL_FTEEXTENSIONS, cls.fteprotocolextensions);
			continue;
		}
		if (protover == PROTOCOL_VERSION)	//this ends the version info
			break;
		if (cls.demoplayback && (protover == 26 || protover == 27 || protover == 28))	//older versions, maintain demo compatability.
			break;
		Host_EndGame ("Server returned version %i, not %i\nYou probably need to upgrade.\nCheck http://www.quakeworld.net/", protover, PROTOCOL_VERSION);
	}
#else
	protover = MSG_ReadLong ();	
	if (protover != PROTOCOL_VERSION && 
		!(cls.demoplayback && (protover == 26 || protover == 27 || protover == 28)))
		Host_EndGame ("Server returned version %i, not %i\nYou probably need to upgrade.\nCheck http://www.quakeworld.net/", protover, PROTOCOL_VERSION);
#endif

	if (cls.fteprotocolextensions & PEXT_FLOATCOORDS)
	{
		sizeofcoord = 4;
		sizeofangle = 1;
	}
	else
	{
		sizeofcoord = 2;
		sizeofangle = 1;
	}

	svcnt = MSG_ReadLong ();	

	// game directory
	str = MSG_ReadString ();

#ifndef CLIENTONLY
	if (!sv.state)
#endif
	{
		COM_FlushTempoaryPacks();
		COM_Gamedir(str);
#ifndef CLIENTONLY
		Info_SetValueForStarKey (svs.info, "*gamedir", str, MAX_SERVERINFO_STRING);
#endif
		COM_FlushFSCache();
	}

	CL_ClearState ();
	R_Part_NewServer();
	Stats_NewMap();
	cl.servercount = svcnt;

	cl.teamfortress = !Q_strcasecmp(str, "fortress");

	if (cl.gamedirchanged)
	{
		cl.gamedirchanged = false;
#ifndef CLIENTONLY
		if (!sv.state)
#endif
			Wads_Flush();

		T_FreeStrings();
	}

	if (cls.demoplayback == DPB_MVD)
	{
		int i;
		extern float nextdemotime;
		cls.netchan.last_received = nextdemotime = /*olddemotime =*/ MSG_ReadFloat();
		cl.playernum[0] = MAX_CLIENTS - 1;
		cl.spectator = true;
		for (i = 0; i < UPDATE_BACKUP; i++)
			cl.frames[i].playerstate[cl.playernum[0]].pm_type = PM_SPECTATOR;

		cl.splitclients = 1;
	}
	else
	{
		// parse player slot, high bit means spectator
		pnum = MSG_ReadByte ();
		for (clnum = 0; ; clnum++)
		{
			cl.playernum[clnum] = pnum;
			if (cl.playernum[clnum] & 128)
			{
				cl.spectator = true;
				cl.playernum[clnum] &= ~128;
			}

			if (!(cls.fteprotocolextensions & PEXT_SPLITSCREEN))
				break;

			pnum = MSG_ReadByte ();

			if (pnum == 128)
				break;

			if (clnum == MAX_SPLITS)
				Host_EndGame("Server sent us too many alternate clients\n");
		}
		cl.splitclients = clnum+1;
	}

	// get the full level name
	str = MSG_ReadString ();
	Q_strncpyz (cl.levelname, str, sizeof(cl.levelname));

	// get the movevars
	movevars.gravity			= MSG_ReadFloat();
	movevars.stopspeed			= MSG_ReadFloat();
	maxspeed					= MSG_ReadFloat();
	movevars.spectatormaxspeed	= MSG_ReadFloat();
	movevars.accelerate			= MSG_ReadFloat();
	movevars.airaccelerate		= MSG_ReadFloat();
	movevars.wateraccelerate	= MSG_ReadFloat();
	movevars.friction			= MSG_ReadFloat();
	movevars.waterfriction		= MSG_ReadFloat();
	entgrav						= MSG_ReadFloat();

	for (clnum = 0; clnum < cl.splitclients; clnum++)
	{
		cl.maxspeed[clnum] = maxspeed;
		cl.entgravity[clnum] = entgrav;
	}

	// seperate the printfs so the server message can have a color
	Con_TPrintf (TLC_LINEBREAK_NEWLEVEL);
	Con_TPrintf (TLC_PC_PS_NL, 2, str);

	memset(cl.sound_name, 0, sizeof(cl.sound_name));
#ifdef PEXT_PK3DOWNLOADS
	if (cls.fteprotocolextensions & PEXT_PK3DOWNLOADS)	//instead of going for a soundlist, go for the pk3 list instead. The server will make us go for the soundlist after.
	{
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, va("pk3list %i 0", cl.servercount, 0));
	}
	else
#endif
	{
		// ask for the sound list next
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	//	MSG_WriteString (&cls.netchan.message, va("soundlist %i 0", cl.servercount));
		MSG_WriteString (&cls.netchan.message, va(soundlist_name, cl.servercount, 0));
	}

	// now waiting for downloads, etc
	cls.state = ca_onserver;

#ifdef VM_CG
	CG_Stop();
#endif
#ifdef CSQC_DAT
	CSQC_Shutdown();
#endif
}

void CLQ2_ParseServerData (void)
{
	char	*str;
	int		i;
	int svcnt;
//	int cflag;

	sizeofcoord = 2;
	sizeofangle = 1;
	
	Con_DPrintf ("Serverdata packet received.\n");
//
// wipe the client_state_t struct
//
	SCR_BeginLoadingPlaque();
//	CL_ClearState ();
	cls.state = ca_onserver;

// parse protocol version number
	i = MSG_ReadLong ();
//	cls.serverProtocol = i;

	if (i > PROTOCOL_VERSION_Q2 || i < PROTOCOL_VERSION_Q2_MIN)
		Host_EndGame ("Server returned version %i, not %i", i, PROTOCOL_VERSION_Q2);

	svcnt = MSG_ReadLong ();
	/*cl.attractloop =*/ MSG_ReadByte ();

	// game directory
	str = MSG_ReadString ();
//	strncpy (cl.gamedir, str, sizeof(cl.gamedir)-1);

	// set gamedir
	if (!*str)
		COM_Gamedir("baseq2");
	else
		COM_Gamedir(str);
	COM_FlushFSCache();
//	if ((*str && (!fs_gamedirvar->string || !*fs_gamedirvar->string || strcmp(fs_gamedirvar->string, str))) || (!*str && (fs_gamedirvar->string || *fs_gamedirvar->string)))
//		Cvar_Set("game", str);

	CL_ClearState ();
	cl.minpitch = -89;
	cl.maxpitch = 89;
	cl.servercount = svcnt;

	Stats_NewMap();


	// parse player entity number
	cl.playernum[0] = MSG_ReadShort ();
	cl.splitclients = 1;
	cl.spectator = false;

	// get the full level name
	str = MSG_ReadString ();
	Q_strncpyz (cl.levelname, str, sizeof(cl.levelname));

	if (cl.playernum[0] == -1)
	{	// playing a cinematic or showing a pic, not a level
		SCR_EndLoadingPlaque();
		if (!Media_PlayFilm(str))
			Con_TPrintf (TLC_NOQ2CINEMATICSSUPPORT, cl.servercount);
		else
			cls.state = ca_active;
	}
	else
	{
		// seperate the printfs so the server message can have a color
		Con_TPrintf (TLC_LINEBREAK_NEWLEVEL);
		Con_TPrintf (TLC_PC_PS_NL, 2, str);

		Media_PlayFilm("");

		// need to prep refresh at next oportunity
		//cl.refresh_prepped = false;
	}


	if (R_PreNewMap)
		R_PreNewMap();
}



#ifdef NQPROT
//FIXME: move to header
#define NQ_PROTOCOL_VERSION 15
void CL_KeepaliveMessage(void){}
void CLNQ_ParseServerData(void)		//Doesn't change gamedir - use with caution.
{
	int	nummodels, numsounds, i;
	char	*str;
	int protover;
	if (developer.value)
		Con_TPrintf (TLC_GOTSVDATAPACKET);
	CL_ClearState ();
	Stats_NewMap();

	protover = MSG_ReadLong ();	

	sizeofcoord = 2;
	sizeofangle = 1;

	nq_dp_protocol = 0;
	cls.z_ext = 0;

	if (protover == 250)
		Host_EndGame ("Nehahra demo net protocol is not supported\n");
	else if (protover == 3502)
	{
		//darkplaces5
		nq_dp_protocol = 5;
		sizeofcoord = 4;
		sizeofangle = 2;
	}
	else if (protover == 3503)
	{
		//darkplaces6 (it's a small difference from dp5)
		nq_dp_protocol = 6;
		sizeofcoord = 4;
		sizeofangle = 2;

		cls.z_ext = Z_EXT_VIEWHEIGHT;
	}
	else if (protover != NQ_PROTOCOL_VERSION)
	{
		Host_EndGame ("Server returned version %i, not %i\nYou will need to use a different client.", protover, NQ_PROTOCOL_VERSION);
	}
	
	if (MSG_ReadByte() > MAX_SCOREBOARD)
	{
		Con_TPrintf (TLC_BAD_MAXCLIENTS);
		return;
	}

	cl.splitclients = 1;

	/*cl.gametype =*/ MSG_ReadByte ();

	str = MSG_ReadString ();
	Q_strncpyz (cl.levelname, str, sizeof(cl.levelname));

	// seperate the printfs so the server message can have a color
	Con_TPrintf (TLC_LINEBREAK_NEWLEVEL);
	Con_TPrintf (TLC_PC_PS_NL, 2, str);

	SCR_BeginLoadingPlaque();

	if (R_PreNewMap)
		R_PreNewMap();

	memset (cl.model_name, 0, sizeof(cl.model_name));
	for (nummodels=1 ; ; nummodels++)
	{
		str = MSG_ReadString ();
		if (!str[0])
			break;
		if (nummodels==MAX_MODELS)
		{
			Con_TPrintf (TLC_TOOMANYMODELPRECACHES);
			return;
		}
		strcpy (cl.model_name[nummodels], str);
		Mod_TouchModel (str);

//		cl.model_precache[nummodels] = Mod_ForName (cl.model_name[nummodels], false);
	}

	memset (cl.sound_name, 0, sizeof(cl.sound_name));
	for (numsounds=1 ; ; numsounds++)
	{
		str = MSG_ReadString ();		
		if (!str[0])
			break;
		if (numsounds==MAX_SOUNDS)
		{
			Con_TPrintf (TLC_TOOMANYSOUNDPRECACHES);
			return;
		}
		strcpy (cl.sound_name[numsounds], str);
		S_TouchSound (str);

//		cl.sound_precache[numsounds] = S_PrecacheSound (cl.sound_name[numsounds]);
	}

//
// now we try to load everything else until a cache allocation fails
//

	for (i=1 ; i<nummodels ; i++)
	{
		cl.model_precache[i] = Mod_ForName (cl.model_name[i], i==1);
//		if (!ignorenonprecached.value || i == 1)	//need world
		{
			if (cl.model_precache[i] == NULL)
			{
				Host_EndGame("Model %s not found\n", cl.model_name[i]);								
			}
		}
		CL_KeepaliveMessage ();
	}

	S_BeginPrecaching ();
	for (i=1 ; i<numsounds ; i++)
	{
		cl.sound_precache[i] = S_PrecacheSound (cl.sound_name[i]);		
		CL_KeepaliveMessage ();
	}
	S_EndPrecaching ();

	cl.worldmodel = cl.model_precache[1];

	
	R_NewMap ();

	SCR_EndLoadingPlaque();

	Hunk_Check ();		// make sure nothing is hurt

	cls.state = ca_onserver;
}
void CLNQ_SignonReply (void)
{		
	char 	str[8192];
	extern cvar_t	topcolor;
	extern cvar_t	bottomcolor;

Con_DPrintf ("CL_SignonReply: %i\n", cls.signon);

	switch (cls.signon)
	{
	case 1:
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "prespawn");
		break;

	case 2:		
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, va("name \"%s\"\n", name.string));
		name.modified = false;
	
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, va("color %i %i\n", (int)topcolor.value, (int)bottomcolor.value));
	
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		sprintf (str, "spawn %s", "");
		MSG_WriteString (&cls.netchan.message, str);
		break;
		
	case 3:	
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "begin");
		Cache_Report ();		// print remaining memory
#ifdef VM_CG
		CG_Start();
#endif
#ifdef CSQC_DAT
		CSQC_Init();
#endif
		break;
		
	case 4:
		SCR_EndLoadingPlaque ();		// allow normal screen updates		
		break;
	}
}

#define	SU_VIEWHEIGHT	(1<<0)
#define	SU_IDEALPITCH	(1<<1)
#define	SU_PUNCH1		(1<<2)
#define	SU_PUNCH2		(1<<3)
#define	SU_PUNCH3		(1<<4)
#define	SU_VELOCITY1	(1<<5)
#define	SU_VELOCITY2	(1<<6)
#define	SU_VELOCITY3	(1<<7)
//define	SU_AIMENT		(1<<8)  AVAILABLE BIT
#define	SU_ITEMS		(1<<9)
#define	SU_ONGROUND		(1<<10)		// no data follows, the bit is it
#define	SU_INWATER		(1<<11)		// no data follows, the bit is it
#define	SU_WEAPONFRAME	(1<<12)
#define	SU_ARMOR		(1<<13)
#define	SU_WEAPON		(1<<14)

#define DPSU_EXTEND1		(1<<15)
// first extend byte
#define DPSU_PUNCHVEC1	(1<<16)
#define DPSU_PUNCHVEC2	(1<<17)
#define DPSU_PUNCHVEC3	(1<<18)
#define DPSU_VIEWZOOM		(1<<19) // byte factor (0 = 0.0 (not valid), 255 = 1.0)
#define DPSU_UNUSED20		(1<<20)
#define DPSU_UNUSED21		(1<<21)
#define DPSU_UNUSED22		(1<<22)
#define DPSU_EXTEND2		(1<<23) // another byte to follow, future expansion
// second extend byte
#define DPSU_UNUSED24		(1<<24)
#define DPSU_UNUSED25		(1<<25)
#define DPSU_UNUSED26		(1<<26)
#define DPSU_UNUSED27		(1<<27)
#define DPSU_UNUSED28		(1<<28)
#define DPSU_UNUSED29		(1<<29)
#define DPSU_UNUSED30		(1<<30)
#define DPSU_EXTEND3		(1<<31) // another byte to follow, future expansion


#define	DEFAULT_VIEWHEIGHT	22
void CLNQ_ParseClientdata (int bits)
{
	int		i;

	bits &= 0xffff;

	if (bits & DPSU_EXTEND1)
		bits |= (MSG_ReadByte() << 16);
	if (bits & DPSU_EXTEND2)
		bits |= (MSG_ReadByte() << 24);
	
	if (bits & SU_VIEWHEIGHT)
		CL_SetStat(0, STAT_VIEWHEIGHT, MSG_ReadChar ());
	else if (nq_dp_protocol != 6)
		CL_SetStat(0, STAT_VIEWHEIGHT, DEFAULT_VIEWHEIGHT);

	if (bits & SU_IDEALPITCH)
		/*cl.idealpitch =*/ MSG_ReadChar ();
	/*else
		cl.idealpitch = 0;*/
	
//	VectorCopy (cl.mvelocity[0], cl.mvelocity[1]);
	for (i=0 ; i<3 ; i++)
	{
		if (bits & (SU_PUNCH1<<i) )
			/*cl.punchangle[i] =*/ nq_dp_protocol?MSG_ReadAngle16():MSG_ReadChar();
//		else
//			cl.punchangle[i] = 0;

		if (bits & (DPSU_PUNCHVEC1<<i))
		{
			/*cl.punchvector[i] =*/ MSG_ReadCoord();
		}
//		else
//			cl.punchvector[i] = 0;

		if (bits & (SU_VELOCITY1<<i) )
		{
			if (nq_dp_protocol == 5 || nq_dp_protocol == 6)
				/*cl.simvel[0][i] =*/ MSG_ReadFloat();
			else
			/*cl.mvelocity[0][i] =*/ MSG_ReadChar()/**16*/;
		}
//		else
//			cl.mvelocity[0][i] = 0;
	}

	if (bits & SU_ITEMS)
		CL_SetStat(0, STAT_ITEMS, MSG_ReadLong());

//	cl.onground = (bits & SU_ONGROUND) != 0;
//	cl.inwater = (bits & SU_INWATER) != 0;

	if (nq_dp_protocol == 6)
	{
	}
	else if (nq_dp_protocol == 5)
	{
		CL_SetStat(0, STAT_WEAPONFRAME, (bits & SU_WEAPONFRAME)?(unsigned short)MSG_ReadShort():0);
		CL_SetStat(0, STAT_ARMOR, (bits & SU_ARMOR)?MSG_ReadShort():0);
		CL_SetStat(0, STAT_WEAPON, (bits & SU_WEAPON)?MSG_ReadShort():0);

		CL_SetStat(0, STAT_HEALTH, MSG_ReadShort());

		CL_SetStat(0, STAT_AMMO, MSG_ReadShort());

		CL_SetStat(0, STAT_SHELLS, MSG_ReadShort());
		CL_SetStat(0, STAT_NAILS, MSG_ReadShort());
		CL_SetStat(0, STAT_ROCKETS, MSG_ReadShort());
		CL_SetStat(0, STAT_CELLS, MSG_ReadShort());

		CL_SetStat(0, STAT_ACTIVEWEAPON, (unsigned short)MSG_ReadShort());
	}
	else
	{
		CL_SetStat(0, STAT_WEAPONFRAME, (bits & SU_WEAPONFRAME)?(unsigned char)MSG_ReadByte():0);
		CL_SetStat(0, STAT_ARMOR, (bits & SU_ARMOR)?MSG_ReadByte():0);
		CL_SetStat(0, STAT_WEAPON, (bits & SU_WEAPON)?MSG_ReadByte():0);

		CL_SetStat(0, STAT_HEALTH, MSG_ReadShort());

		CL_SetStat(0, STAT_AMMO, MSG_ReadByte());

		CL_SetStat(0, STAT_SHELLS, MSG_ReadByte());
		CL_SetStat(0, STAT_NAILS, MSG_ReadByte());
		CL_SetStat(0, STAT_ROCKETS, MSG_ReadByte());
		CL_SetStat(0, STAT_CELLS, MSG_ReadByte());

		CL_SetStat(0, STAT_ACTIVEWEAPON, MSG_ReadByte());
	}

	if (bits & DPSU_VIEWZOOM)
	{
		if (nq_dp_protocol == 5 || nq_dp_protocol == 6)
			i = (unsigned short) MSG_ReadShort();
		else
			i = MSG_ReadByte();
		if (i < 2)
			i = 2;
		CL_SetStat(0, STAT_VIEWZOOM, i);
	}
	else if (nq_dp_protocol != 6)
		CL_SetStat(0, STAT_VIEWZOOM, 255);
}
#endif
/*
==================
CL_ParseSoundlist
==================
*/
void CL_ParseSoundlist (void)
{
	int	numsounds;
	char	*str;
	int n;

// precache sounds
//	memset (cl.sound_precache, 0, sizeof(cl.sound_precache));

	numsounds = MSG_ReadByte();

	for (;;) {
		str = MSG_ReadString ();
		if (!str[0])
			break;
		numsounds++;
		if (numsounds == MAX_SOUNDS)
			Host_EndGame ("Server sent too many sound_precache");
				
//		if (strlen(str)>4)
//		if (!strcmp(str+strlen(str)-4, ".mp3"))	//don't let the server send us a specific mp3. convert it to wav and this way we know not to look outside the quake path for it.
//			strcpy(str+strlen(str)-4, ".wav");

		strcpy (cl.sound_name[numsounds], str);		
	}

	n = MSG_ReadByte();

	if (n) {
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
//		MSG_WriteString (&cls.netchan.message, va("soundlist %i %i", cl.servercount, n));
		MSG_WriteString (&cls.netchan.message, va(soundlist_name, cl.servercount, n));
		return;
	}

	cls.downloadnumber = 0;
	cls.downloadtype = dl_sound;
	Sound_NextDownload ();
}

/*
==================
CL_ParseModellist
==================
*/
void CL_ParseModellist (qboolean lots)
{
	int	nummodels;
	char	*str;
	int n;

// precache models and note certain default indexes
	if (lots)
		nummodels = MSG_ReadShort();
	else
		nummodels = MSG_ReadByte();

	for (;;)
	{
		str = MSG_ReadString ();
		if (!str[0])
			break;
		nummodels++;
		if (nummodels==MAX_MODELS)
			Host_EndGame ("Server sent too many model_precache");
		strcpy (cl.model_name[nummodels], str);

		if (!strcmp(cl.model_name[nummodels],"progs/spike.mdl"))
			cl_spikeindex = nummodels;
#ifdef PEXT_LIGHTUPDATES
		if (!strcmp(cl.model_name[nummodels], "progs/zap.mdl"))
			cl_lightningindex = nummodels;
#endif
		if (!strcmp(cl.model_name[nummodels],"progs/player.mdl"))
			cl_playerindex = nummodels;
		if (!strcmp(cl.model_name[nummodels],"progs/h_player.mdl"))
			cl_h_playerindex = nummodels;
		if (!strcmp(cl.model_name[nummodels],"progs/flag.mdl"))
			cl_flagindex = nummodels;

		if (!strcmp(cl.model_name[nummodels],"progs/missile.mdl"))
			cl_rocketindex = nummodels;
		if (!strcmp(cl.model_name[nummodels],"progs/grenade.mdl"))
			cl_grenadeindex = nummodels;

		
		if (!strcmp(cl.model_name[nummodels],"progs/gib1.mdl"))
			cl_gib1index = nummodels;
		if (!strcmp(cl.model_name[nummodels],"progs/gib2.mdl"))
			cl_gib2index = nummodels;
		if (!strcmp(cl.model_name[nummodels],"progs/gib3.mdl"))
			cl_gib3index = nummodels;
	}

	if (nummodels)
		SCR_ImageName(cl.model_name[1]);

	n = MSG_ReadByte();

	if (n) {
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
//		MSG_WriteString (&cls.netchan.message, va("modellist %i %i", cl.servercount, n));
		MSG_WriteString (&cls.netchan.message, va(modellist_name, cl.servercount, (nummodels&0xff00) + n));
		return;
	}

	cls.downloadnumber = 0;
	cls.downloadtype = dl_model;
	Model_NextDownload ();
}

void CL_ProcessUserInfo (int slot, player_info_t *player);
void CLQ2_ParseClientinfo(int i, char *s)
{
	char *skin, *model, *name;
	player_info_t *player;
	//s contains "name\model/skin"

	player = &cl.players[i];

	*player->userinfo = '\0';

	model = strchr(s, '\\');
	if (model)
	{
		*model = '\0';
		model++;
		name = s;
	}
	else
	{
		name = "Unnammed";
		model = "male";
	}

	skin = strchr(model, '/');
	if (skin)
	{
		*skin = '\0';
		skin++;
	}
	else
		skin = "";
	Info_SetValueForKey(player->userinfo, "name", name, MAX_INFO_STRING);
	Info_SetValueForKey(player->userinfo, "model", model, MAX_INFO_STRING);
	Info_SetValueForKey(player->userinfo, "skin", skin, MAX_INFO_STRING);
	CL_ProcessUserInfo (i, player);
}

void CLQ2_ParseConfigString (void)
{
	int		i;
	char	*s;
//	char	olds[MAX_QPATH];

	i = MSG_ReadShort ();
	if (i < 0 || i >= Q2MAX_CONFIGSTRINGS)
		Host_EndGame ("configstring > Q2MAX_CONFIGSTRINGS");
	s = MSG_ReadString();

//	strncpy (olds, cl.configstrings[i], sizeof(olds));
//	olds[sizeof(olds) - 1] = 0;

//	strcpy (cl.configstrings[i], s);

	// do something apropriate 

	if (i == Q2CS_SKY)
	{
		Q_strncpyz (cl.skyname, s, sizeof(cl.skyname));
	}
	else if (i == Q2CS_STATUSBAR)
	{
		Q_strncpyz(cl.q2statusbar, s, sizeof(cl.q2statusbar));
	}
	else if (i >= Q2CS_LIGHTS && i < Q2CS_LIGHTS+Q2MAX_LIGHTSTYLES)
	{
#ifdef PEXT_LIGHTSTYLECOL
		cl_lightstyle[i - Q2CS_LIGHTS].colour = 7;	//white
#endif
		Q_strncpyz (cl_lightstyle[i - Q2CS_LIGHTS].map,  s, sizeof(cl_lightstyle[i-Q2CS_LIGHTS].map));
		cl_lightstyle[i - Q2CS_LIGHTS].length = Q_strlen(cl_lightstyle[i - Q2CS_LIGHTS].map);

	}
	else if (i == Q2CS_CDTRACK)
	{
//		if (cl.refresh_prepped)
			CDAudio_Play (atoi(s), true);
	}
	else if (i >= Q2CS_MODELS && i < Q2CS_MODELS+Q2MAX_MODELS)
	{
//		if (cl.refresh_prepped)
		{
			Q_strncpyz(cl.model_name[i-Q2CS_MODELS], s, MAX_QPATH);
			cl.model_precache[i-Q2CS_MODELS] = Mod_ForName (cl.model_name[i-Q2CS_MODELS], false);
		}
	}
	else if (i >= Q2CS_SOUNDS && i < Q2CS_SOUNDS+Q2MAX_MODELS)
	{
//		if (cl.refresh_prepped)
		Q_strncpyz(cl.sound_name[i-Q2CS_SOUNDS], s, MAX_QPATH);
			cl.sound_precache[i-Q2CS_SOUNDS] = S_PrecacheSound (s);
	}
	else if (i >= Q2CS_IMAGES && i < Q2CS_IMAGES+Q2MAX_MODELS)
	{	//ignore
		Q_strncpyz(cl.image_name[i-Q2CS_IMAGES], s, MAX_QPATH);
	}
	else if (i >= Q2CS_PLAYERSKINS && i < Q2CS_PLAYERSKINS+Q2MAX_CLIENTS)
	{
//		if (cl.refresh_prepped && strcmp(olds, s))
			CLQ2_ParseClientinfo (i-Q2CS_PLAYERSKINS, s);
	}

	UI_StringChanged(i);
}



/*
==================
CL_ParseBaseline
==================
*/
void CL_ParseBaseline (entity_state_t *es)
{
	int			i;

	memset(es, 0, sizeof(entity_state_t));
	
	es->modelindex = MSG_ReadByte ();
	es->frame = MSG_ReadByte ();
	es->colormap = MSG_ReadByte();
	es->skinnum = MSG_ReadByte();

	for (i=0 ; i<3 ; i++)
	{
		es->origin[i] = MSG_ReadCoord ();
		es->angles[i] = MSG_ReadAngle ();
	}
#ifdef PEXT_SCALE
	es->scale = 1;
#endif
#ifdef PEXT_TRANS
	es->trans = 1;
#endif
}
void CL_ParseBaseline2 (void)
{
	entity_state_t nullst, es;

	memset(&nullst, 0, sizeof(entity_state_t));
	memset(&es, 0, sizeof(entity_state_t));
	
	CL_ParseDelta(&nullst, &es, MSG_ReadShort(), true);
	memcpy(&cl_baselines[es.number], &es, sizeof(es));
}

void CLQ2_Precache_f (void)
{
#ifdef VM_CG
	CG_Start();
#endif

	cls.downloadnumber = 0;
	cls.downloadtype = dl_sound;

	CL_RequestNextDownload();
}



/*
=====================
CL_ParseStatic

Static entities are non-interactive world objects
like torches
=====================
*/
void CL_ParseStatic (int version)
{
	entity_t *ent;
	int		i;
	entity_state_t	es, nullstate;

	if (version == 1)
	{
		CL_ParseBaseline (&es);
		i = cl.num_statics;
		cl.num_statics++;
	}
	else
	{
		memset(&nullstate, 0, sizeof(nullstate));
		CL_ParseDelta(&nullstate, &es, MSG_ReadShort(), true);
		es.number+=MAX_EDICTS;

		for (i = 0; i < cl.num_statics; i++)
			if (cl_static_entities[i].keynum == es.number)
			{
				R_RemoveEfrags (&cl_static_entities[i]);
				break;
			}

		if (i == cl.num_statics)
			cl.num_statics++;
	}
		
	if (i >= MAX_STATIC_ENTITIES)
	{
		cl.num_statics--;
		Con_Printf ("Too many static entities");
		return;
	}
	ent = &cl_static_entities[i];
	memset(ent, 0, sizeof(*ent));

	ent->keynum = es.number;

// copy it to the current state
	ent->model = cl.model_precache[es.modelindex];
	ent->oldframe = ent->frame = es.frame;
	ent->colormap = vid.colormap;
	ent->skinnum = es.skinnum;
	ent->drawflags = es.drawflags;

#ifdef PEXT_SCALE
	ent->scale = es.scale;
#endif
#ifdef PEXT_TRANS
	ent->alpha = es.trans;
#endif
	ent->fatness = es.fatness;
	ent->abslight = es.abslight;

	VectorCopy (es.origin, ent->origin);
	VectorCopy (es.angles, ent->angles);
	es.angles[0]*=-1;
	AngleVectors(es.angles, ent->axis[0], ent->axis[1], ent->axis[2]);
	VectorInverse(ent->axis[1]);

	if (!cl.worldmodel)
	{
		Con_TPrintf (TLC_PARSESTATICWITHNOMAP);
		return;
	}
	
	R_AddEfrags (ent);
}

/*
===================
CL_ParseStaticSound
===================
*/
void CL_ParseStaticSound (void)
{
	extern cvar_t cl_staticsounds;
	vec3_t		org;
	int			sound_num, vol, atten;
	int			i;
	
	for (i=0 ; i<3 ; i++)
		org[i] = MSG_ReadCoord ();
	sound_num = MSG_ReadByte ();
	vol = MSG_ReadByte ();
	atten = MSG_ReadByte ();

	if (!cl_staticsounds.value)
		return;
	
	S_StaticSound (cl.sound_precache[sound_num], org, vol, atten);
}



/*
=====================================================================

ACTION MESSAGES

=====================================================================
*/

/*
==================
CL_ParseStartSoundPacket
==================
*/
void CL_ParseStartSoundPacket(void)
{
    vec3_t  pos;
    int 	channel, ent;
    int 	sound_num;
    int 	volume;
    float 	attenuation;  
 	int		i;
	           
    channel = MSG_ReadShort(); 

    if (channel & SND_VOLUME)
		volume = MSG_ReadByte ();
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;
	
    if (channel & SND_ATTENUATION)
		attenuation = MSG_ReadByte () / 64.0;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;
	
	sound_num = MSG_ReadByte ();

	for (i=0 ; i<3 ; i++)
		pos[i] = MSG_ReadCoord ();
 
	ent = (channel>>3)&1023;
	channel &= 7;

	if (ent > MAX_EDICTS)
		Host_EndGame ("CL_ParseStartSoundPacket: ent = %i", ent);
	
    S_StartSound (ent, channel, cl.sound_precache[sound_num], pos, volume/255.0, attenuation);


	TP_CheckPickupSound(cl.sound_name[sound_num], pos);
}

#ifdef Q2CLIENT
void CLQ2_ParseStartSoundPacket(void)
{
    vec3_t  pos_v;
	float	*pos;
    int 	channel, ent;
    int 	sound_num;
    float 	volume;
    float 	attenuation;  
	int		flags;
	float	ofs;

	flags = MSG_ReadByte ();
	sound_num = MSG_ReadByte ();

    if (flags & Q2SND_VOLUME)
		volume = MSG_ReadByte () / 255.0;
	else
		volume = Q2DEFAULT_SOUND_PACKET_VOLUME;
	
    if (flags & Q2SND_ATTENUATION)
		attenuation = MSG_ReadByte () / 64.0;
	else
		attenuation = Q2DEFAULT_SOUND_PACKET_ATTENUATION;	

    if (flags & Q2SND_OFFSET)
		ofs = MSG_ReadByte () / 1000.0;
	else
		ofs = 0;

	if (flags & Q2SND_ENT)
	{	// entity reletive
		channel = MSG_ReadShort(); 
		ent = channel>>3;
		if (ent > MAX_EDICTS)
			Host_EndGame ("CL_ParseStartSoundPacket: ent = %i", ent);

		channel &= 7;
	}
	else
	{
		ent = 0;
		channel = 0;
	}

	if (flags & Q2SND_POS)
	{	// positioned in space
		MSG_ReadPos (pos_v);
 
		pos = pos_v;
	}
	else	// use entity number
	{
		CL_GetNumberedEntityInfo(ent, pos_v, NULL);
		pos = pos_v;
//		pos = NULL;
	}

	if (!cl.sound_precache[sound_num])
		return;

	S_StartSound (ent, channel, cl.sound_precache[sound_num], pos, volume, attenuation);
}
#endif

#ifdef NQPROT
#define	NQSND_VOLUME		(1<<0)		// a qbyte
#define	NQSND_ATTENUATION	(1<<1)		// a qbyte
void CLNQ_ParseStartSoundPacket(void)
{
    vec3_t  pos;
    int 	channel, ent;
    int 	sound_num;
    int 	volume;
    int 	field_mask;
    float 	attenuation;  
 	int		i;
	           
    field_mask = MSG_ReadByte(); 

    if (field_mask & NQSND_VOLUME)
		volume = MSG_ReadByte ();
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;
	
    if (field_mask & NQSND_ATTENUATION)
		attenuation = MSG_ReadByte () / 64.0;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;
	
	channel = MSG_ReadShort ();
	sound_num = MSG_ReadByte ();

	ent = channel >> 3;
	channel &= 7;

	if (ent > MAX_EDICTS)
		Host_EndGame ("CL_ParseStartSoundPacket: ent = %i", ent);
	
	for (i=0 ; i<3 ; i++)
		pos[i] = MSG_ReadCoord ();
 
    S_StartSound (ent, channel, cl.sound_precache[sound_num], pos, volume/255.0, attenuation);
}        
#endif


/*
==================
CL_ParseClientdata

Server information pertaining to this client only, sent every frame
==================
*/
void CL_ParseClientdata (void)
{
	int				i;
	float		latency;
	frame_t		*frame;

// calculate simulated time of message
	oldparsecountmod = parsecountmod;

	i = cls.netchan.incoming_acknowledged;
	if (cls.demoplayback == DPB_MVD)
		cl.oldparsecount = i - 1;
	cl.parsecount = i;
	i &= UPDATE_MASK;
	parsecountmod = i;
	frame = &cl.frames[i];
	if (cls.demoplayback == DPB_MVD)
		frame->senttime = realtime - host_frametime;
	parsecounttime = cl.frames[i].senttime;

	frame->receivedtime = realtime;

// calculate latency
	latency = frame->receivedtime - frame->senttime;

	if (latency < 0 || latency > 1.0)
	{
//		Con_Printf ("Odd latency: %5.2f\n", latency);
	}
	else
	{
	// drift the average latency towards the observed latency
		if (latency < cls.latency)
			cls.latency = latency;
		else
			cls.latency += 0.001;	// drift up, so correction are needed
	}	
}

/*
=====================
CL_NewTranslation
=====================
*/
void CL_NewTranslation (int slot)
{
#ifdef SWQUAKE
	int		i, j;
	int		top, bottom;
	qbyte	*dest, *source;
#endif

	char *s;
	player_info_t	*player;

	if (slot >= MAX_CLIENTS)
		Sys_Error ("CL_NewTranslation: slot > MAX_CLIENTS");

	player = &cl.players[slot];

	s = Skin_FindName (player);
	COM_StripExtension(s, s);
	if (player->skin && !stricmp(s, player->skin->name))
		player->skin = NULL;


#ifdef RGLQUAKE
	if (qrenderer == QR_OPENGL)
	{	//gl doesn't need to do anything except prevent the sys_error below.
	}
	else
#endif
#ifdef SWQUAKE
		if (qrenderer == QR_SOFTWARE)
	{
		top = player->topcolor;
		bottom = player->bottomcolor;
		if (!cl.splitclients && !(cl.fpd & FPD_NO_FORCE_COLOR))	//no colour/skin forcing in splitscreen.
		{
			if (cl.teamplay && !strcmp(player->team, cl.players[cl.playernum[0]].team))
			{
				if (cl_teamtopcolor>=0)
					top = cl_teamtopcolor;
				if (cl_teambottomcolor>=0)
					bottom = cl_teambottomcolor;
			}
			else
			{
				if (cl_enemytopcolor>=0)
					top = cl_enemytopcolor;
				if (cl_enemybottomcolor>=0)
					bottom = cl_enemybottomcolor;
			}
		}


		if (player->_topcolor != top ||
			player->_bottomcolor != bottom || !player->skin) {
			player->_topcolor = top;
			player->_bottomcolor = bottom;

			dest = player->translations;
			source = vid.colormap;
			memcpy (dest, vid.colormap, sizeof(player->translations));
//			top = player->topcolor;
			if (top > 13 || top < 0)
				top = 13;
			top *= 16;
//			bottom = player->bottomcolor;
			if (bottom > 13 || bottom < 0)
				bottom = 13;
			bottom *= 16;

			for (i=0 ; i<VID_GRADES ; i++, dest += 256, source+=256)
			{
				if (top < 128)	// the artists made some backwards ranges.  sigh.
					memcpy (dest + TOP_RANGE, source + top, 16);
				else
					for (j=0 ; j<16 ; j++)
						dest[TOP_RANGE+j] = source[top+15-j];
						
				if (bottom < 128)
					memcpy (dest + BOTTOM_RANGE, source + bottom, 16);
				else
					for (j=0 ; j<16 ; j++)
						dest[BOTTOM_RANGE+j] = source[bottom+15-j];		
			}
		}
	}
	else
#endif
		Sys_Error("Bad rendering method in CL_NewTranslation");
}

/*
==============
CL_UpdateUserinfo
==============
*/
void CL_ProcessUserInfo (int slot, player_info_t *player)
{
	Q_strncpyz (player->name, Info_ValueForKey (player->userinfo, "name"), sizeof(player->name));
	Q_strncpyz (player->team, Info_ValueForKey (player->userinfo, "team"), sizeof(player->team));
	player->topcolor = atoi(Info_ValueForKey (player->userinfo, "topcolor"));
	player->bottomcolor = atoi(Info_ValueForKey (player->userinfo, "bottomcolor"));
	if (atoi(Info_ValueForKey (player->userinfo, "*spectator")))
		player->spectator = true;
	else
		player->spectator = false;

	if (slot == cl.playernum[0] && player->name[0])
		cl.spectator = player->spectator;

	player->model = NULL;

	if (cls.state == ca_active)
		Skin_Find (player);

	Sbar_Changed ();
	CL_NewTranslation (slot);
}

/*
==============
CL_UpdateUserinfo
==============
*/
void CL_UpdateUserinfo (void)
{
	int		slot;
	player_info_t	*player;

	slot = MSG_ReadByte ();
	if (slot >= MAX_CLIENTS)
		Host_EndGame ("CL_ParseServerMessage: svc_updateuserinfo > MAX_SCOREBOARD");

	player = &cl.players[slot];
	player->userid = MSG_ReadLong ();
	Q_strncpyz (player->userinfo, MSG_ReadString(), sizeof(player->userinfo));

	CL_ProcessUserInfo (slot, player);
}

/*
==============
CL_SetInfo
==============
*/
void CL_SetInfo (void)
{
	int		slot;
	player_info_t	*player;
	char key[MAX_QWMSGLEN];
	char value[MAX_QWMSGLEN];

	slot = MSG_ReadByte ();
	if (slot >= MAX_CLIENTS)
		Host_EndGame ("CL_ParseServerMessage: svc_setinfo > MAX_SCOREBOARD");

	player = &cl.players[slot];

	Q_strncpyz (key, MSG_ReadString(), sizeof(key));
	Q_strncpyz (value, MSG_ReadString(), sizeof(value));

	Con_DPrintf("SETINFO %s: %s=%s\n", player->name, key, value);

	Info_SetValueForStarKey (player->userinfo, key, value, MAX_INFO_STRING);

	CL_ProcessUserInfo (slot, player);
}

/*
==============
CL_ServerInfo
==============
*/
void CL_ServerInfo (void)
{
//	int		slot;
//	player_info_t	*player;
	char key[MAX_QWMSGLEN];
	char value[MAX_QWMSGLEN];

	Q_strncpyz (key, MSG_ReadString(), sizeof(key));
	Q_strncpyz (value, MSG_ReadString(), sizeof(value));

	Con_DPrintf("SERVERINFO: %s=%s\n", key, value);

	Info_SetValueForKey (cl.serverinfo, key, value, MAX_SERVERINFO_STRING);

	CL_CheckServerInfo();
}

/*
=====================
CL_SetStat
=====================
*/
void CL_SetStat (int pnum, int stat, int value)
{
	int	j;
	if (stat < 0 || stat >= MAX_CL_STATS)
		return;
//		Host_EndGame ("CL_SetStat: %i is invalid", stat);

	if (cls.demoplayback == DPB_MVD)
	{
		extern int cls_lastto;
		cl.players[cls_lastto].stats[stat]=value;
		if ( spec_track[pnum] != cls_lastto )
			return;
	}

	if (cl.stats[pnum][stat] != value)
		Sbar_Changed ();
	
	if (stat == STAT_ITEMS)
	{	// set flash times
		for (j=0 ; j<32 ; j++)
			if ( (value & (1<<j)) && !(cl.stats[pnum][stat] & (1<<j)))
				cl.item_gettime[pnum][j] = cl.time;
	}

	if (stat == STAT_VIEWHEIGHT && cls.z_ext & Z_EXT_VIEWHEIGHT)
		cl.viewheight[pnum] = value;

	if (stat == STAT_TIME && cls.z_ext & Z_EXT_SERVERTIME)
	{
//		cl.servertime_works = true;
		cl.gametime = value * 0.001;
		cl.gametimemark = realtime;
	}

	cl.stats[pnum][stat] = value;

	if (pnum == 0)
		TP_StatChanged(stat, value);
}

/*
==============
CL_MuzzleFlash
==============
*/
void CL_MuzzleFlash (void)
{
	vec3_t		fv, rv, uv;
	dlight_t	*dl=NULL;
	int			i;
	player_state_t	*pl;

	packet_entities_t *pack;
	entity_state_t *s1;
	int pnum;

	extern cvar_t cl_muzzleflash;

	i = MSG_ReadShort ();

	//was it us?
	if (i == cl.playernum[0])
	{
		if (!cl_muzzleflash.value)
			return;
	}

	pack = &cl.frames[cls.netchan.incoming_sequence&UPDATE_MASK].packet_entities;	


	for (pnum=0 ; pnum<pack->num_entities ; pnum++)	//try looking for an entity with that id first
	{
		s1 = &pack->entities[pnum];

		if (s1->number == i)
		{
			dl = CL_AllocDlight (i);
			VectorCopy (s1->origin,  dl->origin);
			break;
		}
	}
	if (pnum==pack->num_entities)
	{	//that ent number doesn't exist, go for a player with that number
		if ((unsigned)(i) <= MAX_CLIENTS && i > 0)
		{
			// don't draw our own muzzle flash in gl if flashblending
			if (i-1 == cl.playernum[0] && r_flashblend.value)
				return;

			pl = &cl.frames[parsecountmod].playerstate[i-1];

			dl = CL_AllocDlight (i);
			VectorCopy (pl->origin,  dl->origin);	//set it's origin

			AngleVectors (pl->viewangles, fv, rv, uv);	//shift it up a little
			VectorMA (dl->origin, 18, fv, dl->origin);
		}
		else
			return;
	}

	dl->radius = 200 + (rand()&31);
	dl->minlight = 32;
	dl->die = cl.time + 0.1334;
	dl->color[0] = 0.2;
	dl->color[1] = 0.1;
	dl->color[2] = 0.05;

	dl->channelfade[0] = 1.5; 
	dl->channelfade[1] = 0.75; 
	dl->channelfade[2] = 0.375;
}

#ifdef Q2CLIENT
void Q2S_StartSound(vec3_t origin, int entnum, int entchannel, sfx_t *sfx, float fvol, float attenuation, float timeofs);
void CLQ2_ParseMuzzleFlash (void)
{
	vec3_t		fv, rv, dummy;
	dlight_t	*dl;
	int			i, weapon;
	vec3_t		org, ang;
	int			silenced;
	float		volume;
	char		soundname[64];

	i = MSG_ReadShort ();
	if (i < 1 || i >= Q2MAX_EDICTS)
		Host_Error ("CL_ParseMuzzleFlash: bad entity");

	weapon = MSG_ReadByte ();
	silenced = weapon & Q2MZ_SILENCED;
	weapon &= ~Q2MZ_SILENCED;

	CL_GetNumberedEntityInfo(i, org, ang);

	dl = CL_AllocDlight (i);
	VectorCopy (org,  dl->origin);
	AngleVectors (ang, fv, rv, dummy);
	VectorMA (dl->origin, 18, fv, dl->origin);
	VectorMA (dl->origin, 16, rv, dl->origin);
	if (silenced)
		dl->radius = 100 + (rand()&31);
	else
		dl->radius = 200 + (rand()&31);
	dl->minlight = 32;
	dl->die = cl.time+0.05; //+ 0.1;
	dl->decay = 1;

	dl->channelfade[0] = 2;
	dl->channelfade[1] = 2;
	dl->channelfade[2] = 2;

	if (silenced)
		volume = 0.2;
	else
		volume = 1;


	switch (weapon)
	{
	case Q2MZ_BLASTER:
		dl->color[0] = 0.2;dl->color[1] = 0.2;dl->color[2] = 0;
		Q2S_StartSound (NULL, i, CHAN_WEAPON, S_PrecacheSound("weapons/blastf1a.wav"), volume, ATTN_NORM, 0);
		break;
	case Q2MZ_BLUEHYPERBLASTER:
		dl->color[0] = 0;dl->color[1] = 0;dl->color[2] = 0.2;
		Q2S_StartSound (NULL, i, CHAN_WEAPON, S_PrecacheSound("weapons/hyprbf1a.wav"), volume, ATTN_NORM, 0);
		break;
	case Q2MZ_HYPERBLASTER:
		dl->color[0] = 0.2;dl->color[1] = 0.2;dl->color[2] = 0;
		Q2S_StartSound (NULL, i, CHAN_WEAPON, S_PrecacheSound("weapons/hyprbf1a.wav"), volume, ATTN_NORM, 0);
		break;
	case Q2MZ_MACHINEGUN:
		dl->color[0] = 0.2;dl->color[1] = 0.2;dl->color[2] = 0;
		_snprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (rand() % 5) + 1);
		Q2S_StartSound (NULL, i, CHAN_WEAPON, S_PrecacheSound(soundname), volume, ATTN_NORM, 0);
		break;
		
	case Q2MZ_SHOTGUN:
		dl->color[0] = 0.2;dl->color[1] = 0.2;dl->color[2] = 0;
		Q2S_StartSound (NULL, i, CHAN_WEAPON, S_PrecacheSound("weapons/shotgf1b.wav"), volume, ATTN_NORM, 0);
		Q2S_StartSound (NULL, i, CHAN_AUTO,   S_PrecacheSound("weapons/shotgr1b.wav"), volume, ATTN_NORM, 0.1);
		break;
	case Q2MZ_SSHOTGUN:
		dl->color[0] = 0.2;dl->color[1] = 0.2;dl->color[2] = 0;
		Q2S_StartSound (NULL, i, CHAN_WEAPON, S_PrecacheSound("weapons/sshotf1b.wav"), volume, ATTN_NORM, 0);
		break;
	case Q2MZ_CHAINGUN1:
		dl->radius = 200 + (rand()&31);
		dl->color[0] = 0.2;dl->color[1] = 0.05;dl->color[2] = 0;
		_snprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (rand() % 5) + 1);
		Q2S_StartSound (NULL, i, CHAN_WEAPON, S_PrecacheSound(soundname), volume, ATTN_NORM, 0);
		break;
	case Q2MZ_CHAINGUN2:
		dl->radius = 225 + (rand()&31);
		dl->color[0] = 0.2;dl->color[1] = 0.1;dl->color[2] = 0;
		dl->die = cl.time  + 0.1;	// long delay
		_snprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (rand() % 5) + 1);
		Q2S_StartSound (NULL, i, CHAN_WEAPON, S_PrecacheSound(soundname), volume, ATTN_NORM, 0);
		_snprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (rand() % 5) + 1);
		Q2S_StartSound (NULL, i, CHAN_WEAPON, S_PrecacheSound(soundname), volume, ATTN_NORM, 0.05);
		break;
	case Q2MZ_CHAINGUN3:
		dl->radius = 250 + (rand()&31);
		dl->color[0] = 0.2;dl->color[1] = 0.2;dl->color[2] = 0;
		dl->die = cl.time  + 0.1;	// long delay
		_snprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (rand() % 5) + 1);
		Q2S_StartSound (NULL, i, CHAN_WEAPON, S_PrecacheSound(soundname), volume, ATTN_NORM, 0);
		_snprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (rand() % 5) + 1);
		Q2S_StartSound (NULL, i, CHAN_WEAPON, S_PrecacheSound(soundname), volume, ATTN_NORM, 0.033);
		_snprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (rand() % 5) + 1);
		Q2S_StartSound (NULL, i, CHAN_WEAPON, S_PrecacheSound(soundname), volume, ATTN_NORM, 0.066);
		break;
		
	case Q2MZ_RAILGUN:
		dl->color[0] = 0.1;dl->color[1] = 0.1;dl->color[2] = 0.2;
		Q2S_StartSound (NULL, i, CHAN_WEAPON, S_PrecacheSound("weapons/railgf1a.wav"), volume, ATTN_NORM, 0);
		break;
	case Q2MZ_ROCKET:
		dl->color[0] = 0.2;dl->color[1] = 0.1;dl->color[2] = 0.04;
		Q2S_StartSound (NULL, i, CHAN_WEAPON, S_PrecacheSound("weapons/rocklf1a.wav"), volume, ATTN_NORM, 0);
		Q2S_StartSound (NULL, i, CHAN_AUTO,   S_PrecacheSound("weapons/rocklr1b.wav"), volume, ATTN_NORM, 0.1);
		break;
	case Q2MZ_GRENADE:
		dl->color[0] = 0.2;dl->color[1] = 0.1;dl->color[2] = 0;
		Q2S_StartSound (NULL, i, CHAN_WEAPON, S_PrecacheSound("weapons/grenlf1a.wav"), volume, ATTN_NORM, 0);
		Q2S_StartSound (NULL, i, CHAN_AUTO,   S_PrecacheSound("weapons/grenlr1b.wav"), volume, ATTN_NORM, 0.1);
		break;
	case Q2MZ_BFG:
		dl->color[0] = 0;dl->color[1] = 0.2;dl->color[2] = 0;
		Q2S_StartSound (NULL, i, CHAN_WEAPON, S_PrecacheSound("weapons/bfg__f1y.wav"), volume, ATTN_NORM, 0);
		break;

	case Q2MZ_LOGIN:
		dl->color[0] = 0;dl->color[1] = 0.2; dl->color[2] = 0;
		dl->die = cl.time + 1.0;
		Q2S_StartSound (NULL, i, CHAN_WEAPON, S_PrecacheSound("weapons/grenlf1a.wav"), 1, ATTN_NORM, 0);
//		CL_LogoutEffect (pl->current.origin, weapon);
		break;
	case Q2MZ_LOGOUT:
		dl->color[0] = 0.2;dl->color[1] = 0; dl->color[2] = 0;
		dl->die = cl.time + 1.0;
		Q2S_StartSound (NULL, i, CHAN_WEAPON, S_PrecacheSound("weapons/grenlf1a.wav"), 1, ATTN_NORM, 0);
//		CL_LogoutEffect (pl->current.origin, weapon);
		break;
	case Q2MZ_RESPAWN:
		dl->color[0] = 0.2;dl->color[1] = 0.2; dl->color[2] = 0;
		dl->die = cl.time + 1.0;
		Q2S_StartSound (NULL, i, CHAN_WEAPON, S_PrecacheSound("weapons/grenlf1a.wav"), 1, ATTN_NORM, 0);
//		CL_LogoutEffect (pl->current.origin, weapon);
		break;
	// RAFAEL
	case Q2MZ_PHALANX:
		dl->color[0] = 0.2;dl->color[1] = 0.1; dl->color[2] = 0.1;
		Q2S_StartSound (NULL, i, CHAN_WEAPON, S_PrecacheSound("weapons/plasshot.wav"), volume, ATTN_NORM, 0);
		break;
	// RAFAEL
	case Q2MZ_IONRIPPER:	
		dl->color[0] = 0.2;dl->color[1] = 0.1; dl->color[2] = 0.1;
		Q2S_StartSound (NULL, i, CHAN_WEAPON, S_PrecacheSound("weapons/rippfire.wav"), volume, ATTN_NORM, 0);
		break;

// ======================
// PGM
	case Q2MZ_ETF_RIFLE:
		dl->color[0] = 0.18;dl->color[1] = 0.14;dl->color[2] = 0;
		Q2S_StartSound (NULL, i, CHAN_WEAPON, S_PrecacheSound("weapons/nail1.wav"), volume, ATTN_NORM, 0);
		break;
	case Q2MZ_SHOTGUN2:
		dl->color[0] = 0.2;dl->color[1] = 0.2;dl->color[2] = 0;
		Q2S_StartSound (NULL, i, CHAN_WEAPON, S_PrecacheSound("weapons/shotg2.wav"), volume, ATTN_NORM, 0);
		break;
	case Q2MZ_HEATBEAM:
		dl->color[0] = 0.2;dl->color[1] = 0.2;dl->color[2] = 0;
		dl->die = cl.time + 100;
	//	Q2S_StartSound (NULL, i, CHAN_WEAPON, S_PrecacheSound("weapons/bfg__l1a.wav"), volume, ATTN_NORM, 0);
		break;
	case Q2MZ_BLASTER2:
		dl->color[0] = 0;dl->color[1] = 0.2;dl->color[2] = 0;
		// FIXME - different sound for blaster2 ??
		Q2S_StartSound (NULL, i, CHAN_WEAPON, S_PrecacheSound("weapons/blastf1a.wav"), volume, ATTN_NORM, 0);
		break;
	case Q2MZ_TRACKER:
		// negative flashes handled the same in gl/soft until CL_AddDLights
		dl->color[0] = -0.2;dl->color[1] = -0.2;dl->color[2] = -0.2;
		Q2S_StartSound (NULL, i, CHAN_WEAPON, S_PrecacheSound("weapons/disint2.wav"), volume, ATTN_NORM, 0);
		break;		
	case Q2MZ_NUKE1:
		dl->color[0] = 0.2;dl->color[1] = 0;dl->color[2] = 0;
		dl->die = cl.time + 100;
		break;
	case Q2MZ_NUKE2:
		dl->color[0] = 0.2;dl->color[1] = 0.2;dl->color[2] = 0;
		dl->die = cl.time + 100;
		break;
	case Q2MZ_NUKE4:
		dl->color[0] = 0;dl->color[1] = 0;dl->color[2] = 0.2;
		dl->die = cl.time + 100;
		break;
	case Q2MZ_NUKE8:
		dl->color[0] = 0;dl->color[1] = 0.2;dl->color[2] = 0.2;
		dl->die = cl.time + 100;
		break;
// PGM
// ======================
	}
}

void CLQ2_ParseMuzzleFlash2 (void) 
{
	int			ent;
//	vec3_t		origin;
	int			flash_number;
//	dlight_t	*dl;
//	vec3_t		forward, right;
//	char		soundname[64];

	ent = MSG_ReadShort ();
	if (ent < 1 || ent >= Q2MAX_EDICTS)
		Host_EndGame ("CL_ParseMuzzleFlash2: bad entity");

	flash_number = MSG_ReadByte ();
/*
	// locate the origin
	AngleVectors (cl_entities[ent].current.angles, forward, right, NULL);
	origin[0] = cl_entities[ent].current.origin[0] + forward[0] * monster_flash_offset[flash_number][0] + right[0] * monster_flash_offset[flash_number][1];
	origin[1] = cl_entities[ent].current.origin[1] + forward[1] * monster_flash_offset[flash_number][0] + right[1] * monster_flash_offset[flash_number][1];
	origin[2] = cl_entities[ent].current.origin[2] + forward[2] * monster_flash_offset[flash_number][0] + right[2] * monster_flash_offset[flash_number][1] + monster_flash_offset[flash_number][2];

	dl = CL_AllocDlight (ent);
	VectorCopy (origin,  dl->origin);
	dl->radius = 200 + (rand()&31);
	dl->minlight = 32;
	dl->die = cl.time;	// + 0.1;

	switch (flash_number)
	{
	case Q2MZ2_INFANTRY_MACHINEGUN_1:
	case Q2MZ2_INFANTRY_MACHINEGUN_2:
	case Q2MZ2_INFANTRY_MACHINEGUN_3:
	case Q2MZ2_INFANTRY_MACHINEGUN_4:
	case Q2MZ2_INFANTRY_MACHINEGUN_5:
	case Q2MZ2_INFANTRY_MACHINEGUN_6:
	case Q2MZ2_INFANTRY_MACHINEGUN_7:
	case Q2MZ2_INFANTRY_MACHINEGUN_8:
	case Q2MZ2_INFANTRY_MACHINEGUN_9:
	case Q2MZ2_INFANTRY_MACHINEGUN_10:
	case Q2MZ2_INFANTRY_MACHINEGUN_11:
	case Q2MZ2_INFANTRY_MACHINEGUN_12:
	case Q2MZ2_INFANTRY_MACHINEGUN_13:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash(origin);
		Q2S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("infantry/infatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case Q2MZ2_SOLDIER_MACHINEGUN_1:
	case Q2MZ2_SOLDIER_MACHINEGUN_2:
	case Q2MZ2_SOLDIER_MACHINEGUN_3:
	case Q2MZ2_SOLDIER_MACHINEGUN_4:
	case Q2MZ2_SOLDIER_MACHINEGUN_5:
	case Q2MZ2_SOLDIER_MACHINEGUN_6:
	case Q2MZ2_SOLDIER_MACHINEGUN_7:
	case Q2MZ2_SOLDIER_MACHINEGUN_8:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash(origin);
		Q2S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("soldier/solatck3.wav"), 1, ATTN_NORM, 0);
		break;

	case Q2MZ2_GUNNER_MACHINEGUN_1:
	case Q2MZ2_GUNNER_MACHINEGUN_2:
	case Q2MZ2_GUNNER_MACHINEGUN_3:
	case Q2MZ2_GUNNER_MACHINEGUN_4:
	case Q2MZ2_GUNNER_MACHINEGUN_5:
	case Q2MZ2_GUNNER_MACHINEGUN_6:
	case Q2MZ2_GUNNER_MACHINEGUN_7:
	case Q2MZ2_GUNNER_MACHINEGUN_8:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash(origin);
		Q2S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("gunner/gunatck2.wav"), 1, ATTN_NORM, 0);
		break;

	case Q2MZ2_ACTOR_MACHINEGUN_1:
	case Q2MZ2_SUPERTANK_MACHINEGUN_1:
	case Q2MZ2_SUPERTANK_MACHINEGUN_2:
	case Q2MZ2_SUPERTANK_MACHINEGUN_3:
	case Q2MZ2_SUPERTANK_MACHINEGUN_4:
	case Q2MZ2_SUPERTANK_MACHINEGUN_5:
	case Q2MZ2_SUPERTANK_MACHINEGUN_6:
	case Q2MZ2_TURRET_MACHINEGUN:			// PGM
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;

		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash(origin);
		Q2S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("infantry/infatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case Q2MZ2_BOSS2_MACHINEGUN_L1:
	case Q2MZ2_BOSS2_MACHINEGUN_L2:
	case Q2MZ2_BOSS2_MACHINEGUN_L3:
	case Q2MZ2_BOSS2_MACHINEGUN_L4:
	case Q2MZ2_BOSS2_MACHINEGUN_L5:
	case Q2MZ2_CARRIER_MACHINEGUN_L1:		// PMM
	case Q2MZ2_CARRIER_MACHINEGUN_L2:		// PMM
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;

		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash(origin);
		Q2S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("infantry/infatck1.wav"), 1, ATTN_NONE, 0);
		break;

	case Q2MZ2_SOLDIER_BLASTER_1:
	case Q2MZ2_SOLDIER_BLASTER_2:
	case Q2MZ2_SOLDIER_BLASTER_3:
	case Q2MZ2_SOLDIER_BLASTER_4:
	case Q2MZ2_SOLDIER_BLASTER_5:
	case Q2MZ2_SOLDIER_BLASTER_6:
	case Q2MZ2_SOLDIER_BLASTER_7:
	case Q2MZ2_SOLDIER_BLASTER_8:
	case Q2MZ2_TURRET_BLASTER:			// PGM
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		Q2S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("soldier/solatck2.wav"), 1, ATTN_NORM, 0);
		break;

	case Q2MZ2_FLYER_BLASTER_1:
	case Q2MZ2_FLYER_BLASTER_2:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		Q2S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("flyer/flyatck3.wav"), 1, ATTN_NORM, 0);
		break;

	case Q2MZ2_MEDIC_BLASTER_1:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		Q2S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("medic/medatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case Q2MZ2_HOVER_BLASTER_1:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		Q2S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("hover/hovatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case Q2MZ2_FLOAT_BLASTER_1:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		Q2S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("floater/fltatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case Q2MZ2_SOLDIER_SHOTGUN_1:
	case Q2MZ2_SOLDIER_SHOTGUN_2:
	case Q2MZ2_SOLDIER_SHOTGUN_3:
	case Q2MZ2_SOLDIER_SHOTGUN_4:
	case Q2MZ2_SOLDIER_SHOTGUN_5:
	case Q2MZ2_SOLDIER_SHOTGUN_6:
	case Q2MZ2_SOLDIER_SHOTGUN_7:
	case Q2MZ2_SOLDIER_SHOTGUN_8:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		CL_SmokeAndFlash(origin);
		Q2S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("soldier/solatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case Q2MZ2_TANK_BLASTER_1:
	case Q2MZ2_TANK_BLASTER_2:
	case Q2MZ2_TANK_BLASTER_3:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		Q2S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("tank/tnkatck3.wav"), 1, ATTN_NORM, 0);
		break;

	case Q2MZ2_TANK_MACHINEGUN_1:
	case Q2MZ2_TANK_MACHINEGUN_2:
	case Q2MZ2_TANK_MACHINEGUN_3:
	case Q2MZ2_TANK_MACHINEGUN_4:
	case Q2MZ2_TANK_MACHINEGUN_5:
	case Q2MZ2_TANK_MACHINEGUN_6:
	case Q2MZ2_TANK_MACHINEGUN_7:
	case Q2MZ2_TANK_MACHINEGUN_8:
	case Q2MZ2_TANK_MACHINEGUN_9:
	case Q2MZ2_TANK_MACHINEGUN_10:
	case Q2MZ2_TANK_MACHINEGUN_11:
	case Q2MZ2_TANK_MACHINEGUN_12:
	case Q2MZ2_TANK_MACHINEGUN_13:
	case Q2MZ2_TANK_MACHINEGUN_14:
	case Q2MZ2_TANK_MACHINEGUN_15:
	case Q2MZ2_TANK_MACHINEGUN_16:
	case Q2MZ2_TANK_MACHINEGUN_17:
	case Q2MZ2_TANK_MACHINEGUN_18:
	case Q2MZ2_TANK_MACHINEGUN_19:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash(origin);
		Com_sprintf(soundname, sizeof(soundname), "tank/tnkatk2%c.wav", 'a' + rand() % 5);
		Q2S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound(soundname), 1, ATTN_NORM, 0);
		break;

	case Q2MZ2_CHICK_ROCKET_1:
	case Q2MZ2_TURRET_ROCKET:			// PGM
		dl->color[0] = 1;dl->color[1] = 0.5;dl->color[2] = 0.2;
		Q2S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("chick/chkatck2.wav"), 1, ATTN_NORM, 0);
		break;

	case Q2MZ2_TANK_ROCKET_1:
	case Q2MZ2_TANK_ROCKET_2:
	case Q2MZ2_TANK_ROCKET_3:
		dl->color[0] = 1;dl->color[1] = 0.5;dl->color[2] = 0.2;
		Q2S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("tank/tnkatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case Q2MZ2_SUPERTANK_ROCKET_1:
	case Q2MZ2_SUPERTANK_ROCKET_2:
	case Q2MZ2_SUPERTANK_ROCKET_3:
	case Q2MZ2_BOSS2_ROCKET_1:
	case Q2MZ2_BOSS2_ROCKET_2:
	case Q2MZ2_BOSS2_ROCKET_3:
	case Q2MZ2_BOSS2_ROCKET_4:
	case Q2MZ2_CARRIER_ROCKET_1:
//	case Q2MZ2_CARRIER_ROCKET_2:
//	case Q2MZ2_CARRIER_ROCKET_3:
//	case Q2MZ2_CARRIER_ROCKET_4:
		dl->color[0] = 1;dl->color[1] = 0.5;dl->color[2] = 0.2;
		Q2S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("tank/rocket.wav"), 1, ATTN_NORM, 0);
		break;

	case Q2MZ2_GUNNER_GRENADE_1:
	case Q2MZ2_GUNNER_GRENADE_2:
	case Q2MZ2_GUNNER_GRENADE_3:
	case Q2MZ2_GUNNER_GRENADE_4:
		dl->color[0] = 1;dl->color[1] = 0.5;dl->color[2] = 0;
		Q2S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("gunner/gunatck3.wav"), 1, ATTN_NORM, 0);
		break;

	case Q2MZ2_GLADIATOR_RAILGUN_1:
	// PMM
	case Q2MZ2_CARRIER_RAILGUN:
	case Q2MZ2_WIDOW_RAIL:
	// pmm
		dl->color[0] = 0.5;dl->color[1] = 0.5;dl->color[2] = 1.0;
		break;

// --- Xian's shit starts ---
	case Q2MZ2_MAKRON_BFG:
		dl->color[0] = 0.5;dl->color[1] = 1 ;dl->color[2] = 0.5;
		//Q2S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("makron/bfg_fire.wav"), 1, ATTN_NORM, 0);
		break;

	case Q2MZ2_MAKRON_BLASTER_1:
	case Q2MZ2_MAKRON_BLASTER_2:
	case Q2MZ2_MAKRON_BLASTER_3:
	case Q2MZ2_MAKRON_BLASTER_4:
	case Q2MZ2_MAKRON_BLASTER_5:
	case Q2MZ2_MAKRON_BLASTER_6:
	case Q2MZ2_MAKRON_BLASTER_7:
	case Q2MZ2_MAKRON_BLASTER_8:
	case Q2MZ2_MAKRON_BLASTER_9:
	case Q2MZ2_MAKRON_BLASTER_10:
	case Q2MZ2_MAKRON_BLASTER_11:
	case Q2MZ2_MAKRON_BLASTER_12:
	case Q2MZ2_MAKRON_BLASTER_13:
	case Q2MZ2_MAKRON_BLASTER_14:
	case Q2MZ2_MAKRON_BLASTER_15:
	case Q2MZ2_MAKRON_BLASTER_16:
	case Q2MZ2_MAKRON_BLASTER_17:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		Q2S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("makron/blaster.wav"), 1, ATTN_NORM, 0);
		break;
	
	case Q2MZ2_JORG_MACHINEGUN_L1:
	case Q2MZ2_JORG_MACHINEGUN_L2:
	case Q2MZ2_JORG_MACHINEGUN_L3:
	case Q2MZ2_JORG_MACHINEGUN_L4:
	case Q2MZ2_JORG_MACHINEGUN_L5:
	case Q2MZ2_JORG_MACHINEGUN_L6:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash(origin);
		Q2S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("boss3/xfire.wav"), 1, ATTN_NORM, 0);
		break;

	case Q2MZ2_JORG_MACHINEGUN_R1:
	case Q2MZ2_JORG_MACHINEGUN_R2:
	case Q2MZ2_JORG_MACHINEGUN_R3:
	case Q2MZ2_JORG_MACHINEGUN_R4:
	case Q2MZ2_JORG_MACHINEGUN_R5:
	case Q2MZ2_JORG_MACHINEGUN_R6:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash(origin);
		break;

	case Q2MZ2_JORG_BFG_1:
		dl->color[0] = 0.5;dl->color[1] = 1 ;dl->color[2] = 0.5;
		break;

	case Q2MZ2_BOSS2_MACHINEGUN_R1:
	case Q2MZ2_BOSS2_MACHINEGUN_R2:
	case Q2MZ2_BOSS2_MACHINEGUN_R3:
	case Q2MZ2_BOSS2_MACHINEGUN_R4:
	case Q2MZ2_BOSS2_MACHINEGUN_R5:
	case Q2MZ2_CARRIER_MACHINEGUN_R1:			// PMM
	case Q2MZ2_CARRIER_MACHINEGUN_R2:			// PMM

		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;

		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash(origin);
		break;

// ======
// ROGUE
	case Q2MZ2_STALKER_BLASTER:
	case Q2MZ2_DAEDALUS_BLASTER:
	case Q2MZ2_MEDIC_BLASTER_2:
	case Q2MZ2_WIDOW_BLASTER:
	case Q2MZ2_WIDOW_BLASTER_SWEEP1:
	case Q2MZ2_WIDOW_BLASTER_SWEEP2:
	case Q2MZ2_WIDOW_BLASTER_SWEEP3:
	case Q2MZ2_WIDOW_BLASTER_SWEEP4:
	case Q2MZ2_WIDOW_BLASTER_SWEEP5:
	case Q2MZ2_WIDOW_BLASTER_SWEEP6:
	case Q2MZ2_WIDOW_BLASTER_SWEEP7:
	case Q2MZ2_WIDOW_BLASTER_SWEEP8:
	case Q2MZ2_WIDOW_BLASTER_SWEEP9:
	case Q2MZ2_WIDOW_BLASTER_100:
	case Q2MZ2_WIDOW_BLASTER_90:
	case Q2MZ2_WIDOW_BLASTER_80:
	case Q2MZ2_WIDOW_BLASTER_70:
	case Q2MZ2_WIDOW_BLASTER_60:
	case Q2MZ2_WIDOW_BLASTER_50:
	case Q2MZ2_WIDOW_BLASTER_40:
	case Q2MZ2_WIDOW_BLASTER_30:
	case Q2MZ2_WIDOW_BLASTER_20:
	case Q2MZ2_WIDOW_BLASTER_10:
	case Q2MZ2_WIDOW_BLASTER_0:
	case Q2MZ2_WIDOW_BLASTER_10L:
	case Q2MZ2_WIDOW_BLASTER_20L:
	case Q2MZ2_WIDOW_BLASTER_30L:
	case Q2MZ2_WIDOW_BLASTER_40L:
	case Q2MZ2_WIDOW_BLASTER_50L:
	case Q2MZ2_WIDOW_BLASTER_60L:
	case Q2MZ2_WIDOW_BLASTER_70L:
	case Q2MZ2_WIDOW_RUN_1:
	case Q2MZ2_WIDOW_RUN_2:
	case Q2MZ2_WIDOW_RUN_3:
	case Q2MZ2_WIDOW_RUN_4:
	case Q2MZ2_WIDOW_RUN_5:
	case Q2MZ2_WIDOW_RUN_6:
	case Q2MZ2_WIDOW_RUN_7:
	case Q2MZ2_WIDOW_RUN_8:
		dl->color[0] = 0;dl->color[1] = 1;dl->color[2] = 0;
		Q2S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("tank/tnkatck3.wav"), 1, ATTN_NORM, 0);
		break;

	case Q2MZ2_WIDOW_DISRUPTOR:
		dl->color[0] = -1;dl->color[1] = -1;dl->color[2] = -1;
		Q2S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("weapons/disint2.wav"), 1, ATTN_NORM, 0);
		break;

	case Q2MZ2_WIDOW_PLASMABEAM:
	case Q2MZ2_WIDOW2_BEAMER_1:
	case Q2MZ2_WIDOW2_BEAMER_2:
	case Q2MZ2_WIDOW2_BEAMER_3:
	case Q2MZ2_WIDOW2_BEAMER_4:
	case Q2MZ2_WIDOW2_BEAMER_5:
	case Q2MZ2_WIDOW2_BEAM_SWEEP_1:
	case Q2MZ2_WIDOW2_BEAM_SWEEP_2:
	case Q2MZ2_WIDOW2_BEAM_SWEEP_3:
	case Q2MZ2_WIDOW2_BEAM_SWEEP_4:
	case Q2MZ2_WIDOW2_BEAM_SWEEP_5:
	case Q2MZ2_WIDOW2_BEAM_SWEEP_6:
	case Q2MZ2_WIDOW2_BEAM_SWEEP_7:
	case Q2MZ2_WIDOW2_BEAM_SWEEP_8:
	case Q2MZ2_WIDOW2_BEAM_SWEEP_9:
	case Q2MZ2_WIDOW2_BEAM_SWEEP_10:
	case Q2MZ2_WIDOW2_BEAM_SWEEP_11:
		dl->radius = 300 + (rand()&100);
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		dl->die = cl.time + 200;
		break;
// ROGUE
// ======

// --- Xian's shit ends ---

  //hmm... he must take AGES on the loo.... :p

	}
	*/
}
#endif

int getplayerid(char *msg);
int build_number( void );
//return if we want to print the message.
qboolean CL_ParseChat(char *text)
{	
extern cvar_t cl_chatsound, cl_nofake;

	char *s;
	s = strchr(text, ':');	//Hmm.. FIXME: Can a player's name contain a ':'?... I think the answer is a yes... Hmmm.. problematic eh?
	if (!s || s[1] != ' ')	//wasn't a real chat...
		return true;

	if (!cls.demoplayback)
		Sys_ServerActivity();	//chat always flashes the screen..

//check f_ stuff
	if (!strncmp(s+2, "f_", 2))
	{
		static float versionresponsetime;
		static float modifiedresponsetime;
		static float skinsresponsetime;
		static float serverresponsetime;

		if (!strncmp(s+2, "f_version", 9) && versionresponsetime < Sys_DoubleTime())	//respond to it.
		{
			ValidationPrintVersion(text);
			versionresponsetime = Sys_DoubleTime() + 5;
		}
		else if (!strncmp(s+2, "f_server", 9) && serverresponsetime < Sys_DoubleTime())	//respond to it.
		{
			Validation_Server();
			serverresponsetime = Sys_DoubleTime() + 5;
		}
		else if (!strncmp(s+2, "f_modified", 10) && modifiedresponsetime < Sys_DoubleTime())	//respond to it.
		{
			Validation_FilesModified();
			modifiedresponsetime = Sys_DoubleTime() + 5;
		}
		else if (!strncmp(s+2, "f_skins", 7) && skinsresponsetime < Sys_DoubleTime())	//respond to it.
		{
			Validation_Skins();
			skinsresponsetime = Sys_DoubleTime() + 5;
		}
		return true;
	}

	Validation_CheckIfResponse(text);



	{
		int flags;
		int offset=0;
		qboolean	suppress_talksound;
		char *p;

		flags = TP_CategorizeMessage (text, &offset);

		if (flags == 2 && !TP_FilterMessage(text + offset))
			return false;

		suppress_talksound = false;

		if (flags == 2 || (!cl.teamplay && flags))
			suppress_talksound = TP_CheckSoundTrigger (text + offset);

		if (!cl_chatsound.value ||		// no sound at all
			(cl_chatsound.value == 2 && flags != 2))	// only play sound in mm2
			suppress_talksound = true;

		if (!suppress_talksound)
			S_LocalSound ("misc/talk.wav");

		if (cl_nofake.value == 1 || (cl_nofake.value == 2 && flags != 2)) {
			for (p = s; *p; p++)
				if (*p == 13 || (*p == 10 && p[1]))
					*p = ' '; 
		}

		//funky /me stuff
		p = strchr(text, ':');
		if (!strncmp(p, ": /me", 5))
		{
	//shift name right 1 (for the *)
			memmove(text+1, text, p - text);
			*text = '*';

			memmove(p+1, p+5, strlen(p+5)+1);
		}
	}
	return true;
}

char printtext[1024];
void CL_ParsePrint(char *msg, int level)
{
	strncat(printtext, msg, sizeof(printtext)-1);
	while((msg = strchr(printtext, '\n')))
	{
		*msg = '\0';
		if (level != PRINT_CHAT)
			Stats_ParsePrintLine(printtext);
		TP_SearchForMsgTriggers(msg, level);
		msg++;

		memmove(printtext, msg, strlen(msg)+1);
	}
}

char stufftext[4096];
void CL_ParseStuffCmd(char *msg, int destsplit)	//this protects stuffcmds from network segregation.
{
	strncat(stufftext, msg, sizeof(stufftext)-1);
	while((msg = strchr(stufftext, '\n')))
	{
		*msg = '\0';
		Con_DPrintf("stufftext: %s\n", stufftext);
		if (!strncmp(stufftext, "fullserverinfo ", 15))
			Cmd_ExecuteString(stufftext, RESTRICT_SERVER+destsplit);
		else
		{
			Cbuf_AddText (stufftext, RESTRICT_SERVER+destsplit);
			Cbuf_AddText ("\n", RESTRICT_SERVER+destsplit);
		}
		msg++;

		memmove(stufftext, msg, strlen(msg)+1);
	}
}

int getplayerid(char *msg)
{
	int i;
	int namelen;
	char *colon = strstr(msg, ":");
	if (!colon)
		return -1;

	namelen = colon-msg;

	for (i=0 ; i<MAX_CLIENTS ; i++)
	{
		if (!strncmp(msg, cl.players[i].name, namelen))
			if (!cl.players[i].name[namelen])
				return i;		
	}

	return -1;
}

int getplayerchatcolour(char *msg)
{
	int id;
	int c;
	id = getplayerid(msg);
	if (id == -1)	//not a user/server
		return 1;

	//primary override.
	msg = Info_ValueForKey(cl.players[id].userinfo, "tc");
	if (*msg)
	{
		c = atoi(msg);			
		return c;
	}

	//override based on team
	if (cl.teamfortress)
	{
		switch (cl.players[id].bottomcolor)
		{	//translate q1 skin colours to console colours
		case 4:	//red
			return 1;
		case 13:	//blue
			return 5;
		//fixme: add the others
		}
	}
	return cl.players[id].userid;
}

#define SHOWNET(x) if(cl_shownet.value==2)Con_Printf ("%3i:%s\n", msg_readcount-1, x);
#define SHOWNET2(x, y) if(cl_shownet.value==2)Con_Printf ("%3i:%3i:%s\n", msg_readcount-1, y, x);
/*
=====================
CL_ParseServerMessage
=====================
*/
char *Translate(char *message);
int	received_framecount;
void CL_ParseServerMessage (void)
{
	int			cmd;
	char		*s;
	int			i, j;
	int			destsplit;

	received_framecount = host_framecount;
	cl.last_servermessage = realtime;
	CL_ClearProjectiles ();
	cl.fixangle = false;

//
// if recording demos, copy the message out
//
	if (cl_shownet.value == 1)
		Con_TPrintf (TL_INT_SPACE,net_message.cursize);
	else if (cl_shownet.value == 2)
		Con_TPrintf (TLC_LINEBREAK_MINUS);


	CL_ParseClientdata ();

//
// parse the message
//
	while (1)
	{
		if (msg_badread)
		{
			Host_EndGame ("CL_ParseServerMessage: Bad server message");
			break;
		}

		cmd = MSG_ReadByte ();

		if (cmd == svc_choosesplitclient)
		{
			SHOWNET(svc_strings[cmd]);

			destsplit = MSG_ReadByte();
			cmd = MSG_ReadByte();
		}
		else
			destsplit = 0;

		if (cmd == -1)
		{
			msg_readcount++;	// so the EOM showner has the right value
			SHOWNET("END OF MESSAGE");
			break;
		}

		SHOWNET(svc_strings[cmd]);
	
	// other commands
		switch (cmd)
		{
		default:
			Host_EndGame ("CL_ParseServerMessage: Illegible server message");
			return;

		case svc_time:
			cl.gametime = MSG_ReadFloat();
			cl.gametimemark = realtime;
			break;
			
		case svc_nop:
//			Con_Printf ("svc_nop\n");
			break;
			
		case svc_disconnect:
			if (cls.state == ca_connected)
			{
				Host_EndGame ("Server disconnected\n"
					"Server version may not be compatible");
			}
			else
				Host_EndGame ("Server disconnected");
			break;

		case svc_print:
			i = MSG_ReadByte ();
			s = MSG_ReadString ();
			if (i == PRINT_CHAT)
			{
				if (CL_ParseChat(s))
				{
					CL_ParsePrint(s, i);

					if (!cl_standardchat.value)
						Con_TPrintf (TL_CSPECIALPRINT, getplayerchatcolour(s)%6+'1', s);	//don't ever print it in white.
					else
					{
						con_ormask = CON_STANDARDMASK;
						Con_TPrintf (TL_ST, s);	//Standard text - makes LEDs work in the ocrana (so I'm told) charset.
						con_ormask = 0;			//it's a special/wierd characture set.
					}
				}
			}
			else
			{
				CL_ParsePrint(s, i);
				Con_TPrintf (TL_ST, Translate(s));
			}
			break;
			
		case svc_centerprint:
			SCR_CenterPrint (destsplit, Translate(MSG_ReadString ()));
			break;
			
		case svc_stufftext:
			s = MSG_ReadString ();

			CL_ParseStuffCmd(s, destsplit);	
			break;

		case svc_damage:
			V_ParseDamage (destsplit);
			break;

		case svc_serverdata:
			Cbuf_Execute ();		// make sure any stuffed commands are done
 			CL_ParseServerData ();
			vid.recalc_refdef = true;	// leave full screen intermission
			break;
#ifdef PEXT_SETVIEW
		case svc_setview:
			if (!(cls.fteprotocolextensions & PEXT_SETVIEW))
				Host_EndGame("PEXT_SETVIEW is meant to be disabled\n");
			cl.viewentity[destsplit]=MSG_ReadShort();			
			break;
#endif
		case svc_setangle:
			if (cls.demoplayback == DPB_MVD)
			{
				i = MSG_ReadByte();
				if (i != spec_track[0] || !autocam[0])
				{	//this wasn't for us.
					for (i=0 ; i<3 ; i++)
						MSG_ReadAngle ();
					break;
				}
				cl.fixangle=true;
				for (i=0 ; i<3 ; i++)
					cl.simangles[destsplit][i] = cl.viewangles[destsplit][i] = MSG_ReadAngle ();
				break;
			}
			cl.fixangle=true;
			for (i=0 ; i<3 ; i++)
				cl.viewangles[destsplit][i] = MSG_ReadAngle ();
//			cl.viewangles[PITCH] = cl.viewangles[ROLL] = 0;
			break;

		case svc_lightstyle:
			i = MSG_ReadByte ();
			if (i >= MAX_LIGHTSTYLES)
				Host_EndGame ("svc_lightstyle > MAX_LIGHTSTYLES");
#ifdef PEXT_LIGHTSTYLECOL
			cl_lightstyle[i].colour = 7;	//white
#endif
			Q_strncpyz (cl_lightstyle[i].map,  MSG_ReadString(), sizeof(cl_lightstyle[i].map));
			cl_lightstyle[i].length = Q_strlen(cl_lightstyle[i].map);
			break;
#ifdef PEXT_LIGHTSTYLECOL
		case svc_lightstylecol:
			if (!(cls.fteprotocolextensions & PEXT_LIGHTSTYLECOL))
				Host_EndGame("PEXT_LIGHTSTYLECOL is meant to be disabled\n");
			i = MSG_ReadByte ();
			if (i >= MAX_LIGHTSTYLES)
				Sys_Error ("svc_lightstyle > MAX_LIGHTSTYLES");
			cl_lightstyle[i].colour = MSG_ReadByte();
			Q_strncpyz (cl_lightstyle[i].map,  MSG_ReadString(), sizeof(cl_lightstyle[i].map));
			cl_lightstyle[i].length = Q_strlen(cl_lightstyle[i].map);
			break;
#endif

		case svc_sound:
			CL_ParseStartSoundPacket();
			break;
			
		case svc_stopsound:
			i = MSG_ReadShort();
			S_StopSound(i>>3, i&7);
			break;
		
		case svc_updatefrags:
			Sbar_Changed ();
			i = MSG_ReadByte ();
			if (i >= MAX_CLIENTS)
				Host_EndGame ("CL_ParseServerMessage: svc_updatefrags > MAX_SCOREBOARD");
			cl.players[i].frags = MSG_ReadShort ();
			break;			

		case svc_updateping:
			i = MSG_ReadByte ();
			if (i >= MAX_CLIENTS)
				Host_EndGame ("CL_ParseServerMessage: svc_updateping > MAX_SCOREBOARD");
			cl.players[i].ping = MSG_ReadShort ();
			break;
			
		case svc_updatepl:
			i = MSG_ReadByte ();
			if (i >= MAX_CLIENTS)
				Host_EndGame ("CL_ParseServerMessage: svc_updatepl > MAX_SCOREBOARD");
			cl.players[i].pl = MSG_ReadByte ();
			break;
			
		case svc_updateentertime:
		// time is sent over as seconds ago
			i = MSG_ReadByte ();
			if (i >= MAX_CLIENTS)
				Host_EndGame ("CL_ParseServerMessage: svc_updateentertime > MAX_SCOREBOARD");
			cl.players[i].entertime = realtime - MSG_ReadFloat ();
			break;
			
		case svc_spawnbaseline:
			i = MSG_ReadShort ();
			CL_ParseBaseline (&cl_baselines[i]);
			break;
		case svc_spawnbaseline2:
			CL_ParseBaseline2 ();
			break;
		case svc_spawnstatic:
			CL_ParseStatic (1);
			break;
		case svc_spawnstatic2:
			CL_ParseStatic (2);
			break;
		case svc_temp_entity:
#ifdef NQPROT
			CL_ParseTEnt (false);
#else
			CL_ParseTEnt ();
#endif
			break;
		case svc_customtempent:
			CL_ParseCustomTEnt();
			break;

		case svc_particle:
			NQ_R_ParseParticleEffect ();
			break;
		case svc_particle2:
			R_ParseParticleEffect2 ();
			break;
		case svc_particle3:
			R_ParseParticleEffect3 ();
			break;
		case svc_particle4:
			R_ParseParticleEffect4 ();
			break;

		case svc_killedmonster:
			cl.stats[0][STAT_MONSTERS]++;
			break;

		case svc_foundsecret:
			cl.stats[0][STAT_SECRETS]++;
			break;

		case svc_updatestat:
			i = MSG_ReadByte ();
			j = MSG_ReadByte ();
			CL_SetStat (destsplit, i, j);
			break;
		case svc_updatestatlong:
			i = MSG_ReadByte ();
			j = MSG_ReadLong ();	//make qbyte if nq compatability?
			CL_SetStat (destsplit, i, j);
			break;
			
		case svc_spawnstaticsound:
			CL_ParseStaticSound ();
			break;

		case svc_cdtrack:
			cl.cdtrack = MSG_ReadByte ();
			CDAudio_Play ((qbyte)cl.cdtrack, true);
			break;

		case svc_intermission:
			cl.intermission = 1;
			cl.completed_time = realtime;
			vid.recalc_refdef = true;	// go to full screen
			for (i=0 ; i<3 ; i++)
				cl.simorg[0][i] = MSG_ReadCoord ();			
			for (i=0 ; i<3 ; i++)
				cl.simangles[0][i] = MSG_ReadAngle ();
			VectorCopy (vec3_origin, cl.simvel[0]);

			VectorCopy (cl.simvel[0], cl.simvel[1]);
			VectorCopy (cl.simangles[0], cl.simangles[1]);
			VectorCopy (cl.simorg[0], cl.simorg[1]);
			break;

		case svc_finale:
			cl.intermission = 2;
			cl.completed_time = realtime;
			vid.recalc_refdef = true;	// go to full screen
			SCR_CenterPrint (destsplit, MSG_ReadString ());			
			break;
			
		case svc_sellscreen:
			Cmd_ExecuteString ("help", RESTRICT_RCON);
			break;

		case svc_smallkick:
			cl.punchangle[destsplit] = -2;
			break;
		case svc_bigkick:
			cl.punchangle[destsplit] = -4;
			break;

		case svc_muzzleflash:
			CL_MuzzleFlash ();
			break;

		case svc_updateuserinfo:
			CL_UpdateUserinfo ();
			break;

		case svc_setinfo:
			CL_SetInfo ();
			break;

		case svc_serverinfo:
			CL_ServerInfo ();
			break;

		case svc_download:
			CL_ParseDownload ();
			break;

		case svc_playerinfo:
			CL_ParsePlayerinfo ();
			break;

		case svc_nails:
			CL_ParseProjectiles (cl_spikeindex, false);
			break;
		case svc_nails2:
			CL_ParseProjectiles (cl_spikeindex, true);
			break;

		case svc_chokecount:		// some preceding packets were choked
			i = MSG_ReadByte ();
			for (j=0 ; j<i ; j++)
				cl.frames[ (cls.netchan.incoming_acknowledged-1-j)&UPDATE_MASK ].receivedtime = -2;
			break;

		case svc_modellist:
			CL_ParseModellist (false);
			break;
		case svc_modellistshort:
			CL_ParseModellist (true);
			break;

		case svc_soundlist:
			CL_ParseSoundlist ();
			break;

		case svc_packetentities:
			CL_ParsePacketEntities (false);
			break;

		case svc_deltapacketentities:
			CL_ParsePacketEntities (true);
			break;

		case svc_maxspeed :
			cl.maxspeed[destsplit] = MSG_ReadFloat();
			break;

		case svc_entgravity :
			cl.entgravity[destsplit] = MSG_ReadFloat();
			break;

		case svc_setpause:
			cl.paused = MSG_ReadByte ();
			if (cl.paused)
				CDAudio_Pause ();
			else
				CDAudio_Resume ();
			break;

#ifdef PEXT_BULLETENS
		case svc_bulletentext:
			if (!(cls.fteprotocolextensions & PEXT_BULLETENS))
				Host_EndGame("PEXT_BULLETENS is meant to be disabled\n");
			Bul_ParseMessage();
			break;
#endif
#ifdef PEXT_LIGHTUPDATES
		case svc_lightnings:
			if (!(cls.fteprotocolextensions & PEXT_LIGHTUPDATES))
				Host_EndGame("PEXT_LIGHTUPDATES is meant to be disabled\n");
			CL_ParseProjectiles (cl_lightningindex);
			break;
#endif

		case svc_ftesetclientpersist:
			CL_ParseClientPersist();
			break;
#ifdef Q2BSPS
		case svc_setportalstate:
			i = MSG_ReadByte();
			j = MSG_ReadByte();
			i *= j & 127;
			j &= ~128;
			CMQ2_SetAreaPortalState(i, j!=0);
			break;
#endif

		case svc_showpic:
			SCR_ShowPic_Create();
			break;
		case svc_hidepic:
			SCR_ShowPic_Hide();
			break;
		case svc_movepic:
			SCR_ShowPic_Move();
			break;
		case svc_updatepic:
			SCR_ShowPic_Update();
			break;

		case svc_setattachment:
			CL_ParseAttachment();
			break;

		case svcqw_effect:
			CL_ParseEffect(false);
			break;
		case svcqw_effect2:
			CL_ParseEffect(true);
			break;
		}
	}
}

#ifdef Q2CLIENT
void CLQ2_ParseServerMessage (void)
{
	int			cmd;
	char		*s;
	int			i;
//	int			j;

	received_framecount = host_framecount;
	cl.last_servermessage = realtime;
	CL_ClearProjectiles ();

//
// if recording demos, copy the message out
//
	if (cl_shownet.value == 1)
		Con_TPrintf (TL_INT_SPACE,net_message.cursize);
	else if (cl_shownet.value == 2)
		Con_TPrintf (TLC_LINEBREAK_MINUS);


	CL_ParseClientdata ();

//
// parse the message
//
	while (1)
	{
		if (msg_badread)
		{
			Host_EndGame ("CL_ParseServerMessage: Bad server message");
			break;
		}

		cmd = MSG_ReadByte ();

		if (cmd == -1)
		{
			msg_readcount++;	// so the EOM showner has the right value
			SHOWNET("END OF MESSAGE");
			break;
		}

		SHOWNET(va("%i", cmd));
	
	// other commands
		switch (cmd)
		{
		default:
			Host_EndGame ("CL_ParseServerMessage: Illegible server message");
			return;

	//known to game
		case svcq2_muzzleflash:
			CLQ2_ParseMuzzleFlash();			
			break;
		case svcq2_muzzleflash2:
			CLQ2_ParseMuzzleFlash2();
			return;
		case svcq2_temp_entity:
			CLQ2_ParseTEnt();
			break;
		case svcq2_layout:
			s = MSG_ReadString ();
			Q_strncpyz (cl.q2layout, s, sizeof(cl.q2layout));
			UI_Q2LayoutChanged();
			break;
		case svcq2_inventory:
			Host_EndGame ("CL_ParseServerMessage: svcq2_inventory not implemented");
			return;

	// the rest are private to the client and server
		case svcq2_nop:			//6
			Host_EndGame ("CL_ParseServerMessage: svcq2_nop not implemented");
			return;
		case svcq2_disconnect:
			if (cls.state == ca_connected)
				Host_EndGame ("Server disconnected\n"
					"Server version may not be compatible");
			else
				Host_EndGame ("Server disconnected");
			return;
		case svcq2_reconnect:	//8
			Con_TPrintf (TLC_RECONNECTING);
			MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
			MSG_WriteString (&cls.netchan.message, "new");
			break;
		case svcq2_sound:		//9			// <see code>
			CLQ2_ParseStartSoundPacket();
			break;
		case svcq2_print:		//10			// [qbyte] id [string] null terminated string
			i = MSG_ReadByte ();
			s = MSG_ReadString ();
			if (i == PRINT_CHAT)
			{
				S_LocalSound ("misc/talk.wav");
				con_ormask = 0x8000;
				if (CL_ParseChat(s))
				{
					CL_ParsePrint(s, i);
					Con_TPrintf (TL_CSPECIALPRINT, getplayerchatcolour(s)%6+'1', s);
				}
			}
			else
			{
				CL_ParsePrint(s, i);
				Con_TPrintf (TL_ST, Translate(s));
			}
			con_ormask = 0;
			break;
		case svcq2_stufftext:	//11			// [string] stuffed into client's console buffer, should be \n terminated
			s = MSG_ReadString ();
			Con_DPrintf ("stufftext: %s\n", s);
			if (!strncmp(s, "precache", 8))	//big major hack. Q2 uses a command that q1 has as a cvar.
			{	//call the q2 precache function.
				CLQ2_Precache_f();
			}
			else
				Cbuf_AddText (s, RESTRICT_SERVER);	//don't let the local user cheat
			break;
		case svcq2_serverdata:	//12			// [long] protocol ...
			Cbuf_Execute ();		// make sure any stuffed commands are done
			CLQ2_ParseServerData ();
			break;
		case svcq2_configstring:	//13		// [short] [string]
			CLQ2_ParseConfigString();
			break;
		case svcq2_spawnbaseline://14		
			CLQ2_ParseBaseline();
			break;
		case svcq2_centerprint:	//15		// [string] to put in center of the screen
			SCR_CenterPrint (0, Translate(MSG_ReadString ()));
			break;
		case svcq2_download:		//16		// [short] size [size bytes]
			CL_ParseDownload();
			break;
		case svcq2_playerinfo:	//17			// variable
			Host_EndGame ("CL_ParseServerMessage: svcq2_playerinfo not implemented");
			return;
		case svcq2_packetentities://18			// [...]
			Host_EndGame ("CL_ParseServerMessage: svcq2_packetentities not implemented");
			return;
		case svcq2_deltapacketentities://19	// [...]
			Host_EndGame ("CL_ParseServerMessage: svcq2_deltapacketentities not implemented");
			return;
		case svcq2_frame:			//20 (the bastard to implement.)
			CLQ2_ParseFrame();
			break;
		}
	}
	CL_SetSolidEntities ();
}
#endif

#ifdef NQPROT
void CLNQ_ParseServerMessage (void)
{
	int			cmd;
	char		*s;
	int			i, j;

//	received_framecount = host_framecount;
//	cl.last_servermessage = realtime;
	CL_ClearProjectiles ();
	cl.fixangle = false;

//
// if recording demos, copy the message out
//
	if (cl_shownet.value == 1)
		Con_TPrintf (TL_INT_SPACE,net_message.cursize);
	else if (cl_shownet.value == 2)
		Con_TPrintf (TLC_LINEBREAK_MINUS);


	CL_ParseClientdata ();


	MSG_BeginReading ();
//
// parse the message
//
	while (1)
	{
		if (msg_badread)
		{
			Host_EndGame ("CL_ParseServerMessage: Bad server message");
			break;
		}

		cmd = MSG_ReadByte ();

		if (cmd == -1)
		{
			msg_readcount++;	// so the EOM showner has the right value
			SHOWNET("END OF MESSAGE");
			break;
		}

		if (cmd & 128)
		{
			SHOWNET("fast update");
			CLNQ_ParseEntity(cmd&127);
			continue;
		}

		SHOWNET2(svc_nqstrings[cmd>(sizeof(svc_nqstrings)/sizeof(char*))?0:cmd], cmd);
	
	// other commands
		switch (cmd)
		{
		default:
			Host_EndGame ("CLNQ_ParseServerMessage: Illegible server message (%i)", cmd);
			return;
			
		case svc_nop:
//			Con_Printf ("svc_nop\n");
			break;		

		case svc_print:
			s = MSG_ReadString ();
			if (*s == 1 || *s == 2)
			{
				if (CL_ParseChat(s+1))
				{
					CL_ParsePrint(s+1, 3);
					Con_TPrintf (TL_ST, Translate(s));
				}
			}
			else
			{
				CL_ParsePrint(s, 3);
				Con_TPrintf (TL_ST, Translate(s));
			}
			con_ormask = 0;
			break;

		case svc_disconnect:
//			CL_Disconnect();
			break;
			
		case svc_centerprint:
			SCR_CenterPrint (0, Translate(MSG_ReadString ()));
			break;
			
		case svc_stufftext:
			s = MSG_ReadString ();
			Con_DPrintf ("stufftext: %s\n", s);
			Cbuf_AddText (s, RESTRICT_SERVER);	//no cheating here...
			break;

		case svc_serverdata:
			Cbuf_Execute ();		// make sure any stuffed commands are done
			CLNQ_ParseServerData ();
			vid.recalc_refdef = true;	// leave full screen intermission			
			break;

		case 54://svc_precache:
			{
				int i = (unsigned short)MSG_ReadShort();
				char *s = MSG_ReadString();
				if (i < 32768)
				{
					if (i >= 1 && i < MAX_MODELS)
					{
						model_t *model;
						CL_CheckOrDownloadFile(s, true);
						model = Mod_ForName(s, i == 1);
						if (!model)
							Con_Printf("svc_precache: Mod_ForName(\"%s\") failed\n", s);
						cl.model_precache[i] = model;
						strcpy (cl.model_name[i], s);
					}
					else
						Con_Printf("svc_precache: model index %i outside range %i...%i\n", i, 1, MAX_MODELS);
				}
				else
				{
					i -= 32768;
					if (i >= 1 && i < MAX_SOUNDS)
					{
						sfx_t *sfx;
						CL_CheckOrDownloadFile(va("sounds/%s", s), true);
						sfx = S_PrecacheSound (s);
						if (!sfx)
							Con_Printf("svc_precache: S_PrecacheSound(\"%s\") failed\n", s);
						cl.sound_precache[i] = sfx;
						strcpy (cl.sound_name[i], s);
					}
					else
						Con_Printf("svc_precache: sound index %i outside range %i...%i\n", i, 1, MAX_SOUNDS);
				}
			}
			break;

		case svc_cdtrack:
			cl.cdtrack = MSG_ReadByte ();
			MSG_ReadByte ();

			CDAudio_Play ((qbyte)cl.cdtrack, true);			
			break;		

		case svc_setview:
			if (!cl.viewentity[0])
				cl.playernum[0] = (cl.viewentity[0] = MSG_ReadShort())-1;
			else
				cl.viewentity[0]=MSG_ReadShort();
			break;

		case svc_signonnum:
			i = MSG_ReadByte ();

			if (i <= cls.signon)
				Host_EndGame ("Received signon %i when at %i", i, cls.signon);
			cls.signon = i;
			CLNQ_SignonReply ();
			break;
		case svc_setpause:
			cl.paused = MSG_ReadByte ();
			if (cl.paused)
				CDAudio_Pause ();
			else
				CDAudio_Resume ();
			break;

		case svc_spawnstaticsound:
			CL_ParseStaticSound ();
			break;

		case svc_spawnstatic:
			CL_ParseStatic (1);
			break;
		
		case svc_spawnbaseline:
			i = MSG_ReadShort ();
			CL_ParseBaseline (&cl_baselines[i]);
			break;

		case svc_time:
			received_framecount = host_framecount;
			cl.last_servermessage = realtime;
			cl.gametime = MSG_ReadFloat();
			cl.gametimemark = realtime;
			if (nq_dp_protocol<5)
			{
				cl.validsequence = cls.netchan.incoming_sequence++;
//				cl.frames[(cls.netchan.incoming_sequence-1)&UPDATE_MASK].packet_entities = cl.frames[cls.netchan.incoming_sequence&UPDATE_MASK].packet_entities;
				cl.frames[cls.netchan.incoming_sequence&UPDATE_MASK].packet_entities.num_entities=0;
			}
			break;

		case svc_updatename:
			Sbar_Changed ();
			i = MSG_ReadByte ();
			strcpy(cl.players[i].name, MSG_ReadString());
			break;

		case svc_updatefrags:
			Sbar_Changed ();
			i = MSG_ReadByte ();
			cl.players[i].frags = MSG_ReadShort();
			break;
		case svc_updatecolors:
			{
			int a;
			Sbar_Changed ();
			i = MSG_ReadByte ();
			if (i >= MAX_SCOREBOARD)
				Host_Error ("CL_ParseServerMessage: svc_updatecolors > MAX_SCOREBOARD");
			a = MSG_ReadByte ();
			//FIXME:!!!!

			cl.players[i].topcolor = a&0x0f;
			cl.players[i].bottomcolor = (a&0xf0)>>4;

			if (cls.state == ca_active)
				Skin_Find (&cl.players[i]);

			Sbar_Changed ();
			CL_NewTranslation (i);
			}
			break;
		case svc_lightstyle:
			i = MSG_ReadByte ();
			if (i >= MAX_LIGHTSTYLES)
				Host_EndGame ("svc_lightstyle > MAX_LIGHTSTYLES");
#ifdef PEXT_LIGHTSTYLECOL
			cl_lightstyle[i].colour = 7;	//white
#endif
			Q_strncpyz (cl_lightstyle[i].map,  MSG_ReadString(), sizeof(cl_lightstyle[i].map));
			cl_lightstyle[i].length = Q_strlen(cl_lightstyle[i].map);
			break;

		case svc_updatestat:
			i = MSG_ReadByte ();			
			j = MSG_ReadLong ();
			CL_SetStat (0, i, j);
			break;
		case 51://svc_updatestat:
			i = MSG_ReadByte ();			
			j = MSG_ReadByte ();
			CL_SetStat (0, i, j);
			break;
		case svc_setangle:
			for (i=0 ; i<3 ; i++)
				cl.viewangles[0][i] = MSG_ReadAngle ();
//			cl.viewangles[PITCH] = cl.viewangles[ROLL] = 0;
			break;

		case svc_clientdata:
			i = MSG_ReadShort ();
			CLNQ_ParseClientdata (i);
			break;

		case svc_sound:
			CLNQ_ParseStartSoundPacket();
			break;

		case svc_temp_entity:
			CL_ParseTEnt (true);
			break;
		
		case svc_particle:
			NQ_R_ParseParticleEffect ();
			break;

		case svc_killedmonster:
			cl.stats[0][STAT_MONSTERS]++;
			break;

		case svc_foundsecret:
			cl.stats[0][STAT_SECRETS]++;
			break;

		case svc_intermission:
			cl.intermission = 1;
			cl.completed_time = cl.time;
			vid.recalc_refdef = true;	// go to full screen
			break;

		case svc_finale:
			cl.intermission = 2;
			cl.completed_time = cl.time;
			vid.recalc_refdef = true;	// go to full screen
			SCR_CenterPrint (0, MSG_ReadString ());			
			break;

		case svc_cutscene:
			cl.intermission = 3;
			cl.completed_time = cl.time;
			vid.recalc_refdef = true;	// go to full screen
			SCR_CenterPrint (0, MSG_ReadString ());
			break;

		case svc_sellscreen:	//pantsie
			Cmd_ExecuteString ("help 0", RESTRICT_RCON);
			break;

		case svc_damage:
			V_ParseDamage (0);
			break;

		case svcnq_effect:
			CL_ParseEffect(false);
			break;
		case svcnq_effect2:
			CL_ParseEffect(true);
			break;

		case 57://svc_entities
			if (cls.signon == 4 - 1)
			{	// first update is the final signon stage
				cls.signon = 4;
				CLNQ_SignonReply ();
			}	
			//well, it's really any protocol, but we're only going to support version 5.
			CLNQ_ParseDarkPlaces5Entities();
			break;
		}
	}
}
#endif

