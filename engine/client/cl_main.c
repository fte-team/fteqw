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
// cl_main.c  -- client main loop

#include "quakedef.h"
#include "winquake.h"
#include <sys/types.h>
#include "netinc.h"
#include "cl_master.h"
#include "cl_ignore.h"
#include "shader.h"
#include <ctype.h>
// callbacks
void QDECL CL_Sbar_Callback(struct cvar_s *var, char *oldvalue);
#ifdef NQPROT
void QDECL Name_Callback(struct cvar_s *var, char *oldvalue);
#else
#define Name_Callback NULL
#endif

static void CL_ForceStopDownload (qboolean finish);

// we need to declare some mouse variables here, because the menu system
// references them even when on a unix system.

qboolean	noclip_anglehack;		// remnant from old quake
int startuppending;

void Host_FinishLoading(void);


cvar_t	rcon_password = CVARF("rcon_password", "", CVAR_NOUNSAFEEXPAND);

cvar_t	rcon_address = CVARF("rcon_address", "", CVAR_NOUNSAFEEXPAND);

cvar_t	cl_timeout = CVAR("cl_timeout", "60");

cvar_t	cl_shownet = CVARD("cl_shownet","0", "Debugging var. 0 shows nothing. 1 shows incoming packet sizes. 2 shows individual messages. 3 shows entities too.");	// can be 0, 1, or 2

cvar_t	cl_pure		= CVARD("cl_pure", "0", "0=standard quake rules.\n1=clients should prefer files within packages present on the server.\n2=clients should use *only* files within packages present on the server.\nDue to quake 1.01/1.06 differences, a setting of 2 is only reliable with total conversions.\nIf sv_pure is set, the client will prefer the highest value set.");
cvar_t	cl_sbar		= CVARFC("cl_sbar", "0", CVAR_ARCHIVE, CL_Sbar_Callback);
cvar_t	cl_hudswap	= CVARF("cl_hudswap", "0", CVAR_ARCHIVE);
cvar_t	cl_maxfps	= CVARF("cl_maxfps", "500", CVAR_ARCHIVE);
cvar_t	cl_idlefps	= CVARFD("cl_idlefps", "0", CVAR_ARCHIVE, "This is the maximum framerate to attain while idle/paused/unfocused.");
cvar_t	cl_yieldcpu = CVARFD("cl_yieldcpu", "0", CVAR_ARCHIVE, "Attempt to yield between frames. This can resolve issues with certain drivers and background software, but can mean less consistant frame times. Will reduce power consumption/heat generation so should be set on laptops or similar (over-hot/battery powered) devices.");
cvar_t	cl_nopext	= CVARF("cl_nopext", "0", CVAR_ARCHIVE);
cvar_t	cl_pext_mask = CVAR("cl_pext_mask", "0xffffffff");
cvar_t	cl_nolerp	= CVARD("cl_nolerp", "0", "Disables interpolation. If set, missiles/monsters will be show exactly what was last received, which will be jerky. Does not affect players. A value of 2 means 'interpolate only in single-player/coop'.");
cvar_t	cl_nolerp_netquake = CVARD("cl_nolerp_netquake", "0", "Disables interpolation when connected to an NQ server. Does affect players, even the local player. You probably don't want to set this.");
cvar_t	*hud_tracking_show;
cvar_t	*hud_miniscores_show;
extern cvar_t net_compress;

cvar_t	cl_defaultport		= CVARAFD("cl_defaultport", STRINGIFY(PORT_QWSERVER), "port", 0, "The default port to connect to servers.\nQW: "STRINGIFY(PORT_QWSERVER)", NQ: "STRINGIFY(PORT_NQSERVER)", Q2: "STRINGIFY(PORT_Q2SERVER)".");

cvar_t	cfg_save_name = CVARFD("cfg_save_name", "fte", CVAR_ARCHIVE|CVAR_NOTFROMSERVER, "This is the config name that is saved by default when no argument is specified.");

cvar_t	cl_splitscreen = CVARD("cl_splitscreen", "0", "Enables splitscreen support. See also: allow_splitscreen, in_rawinput*, the \"p\" command.");

cvar_t	lookspring = CVARF("lookspring","0", CVAR_ARCHIVE);
cvar_t	lookstrafe = CVARF("lookstrafe","0", CVAR_ARCHIVE);
cvar_t	sensitivity = CVARF("sensitivity","10", CVAR_ARCHIVE);

cvar_t cl_staticsounds = CVARF("cl_staticsounds", "1", CVAR_ARCHIVE);

cvar_t	m_pitch = CVARF("m_pitch","0.022", CVAR_ARCHIVE);
cvar_t	m_yaw = CVARF("m_yaw","0.022", CVAR_ARCHIVE);
cvar_t	m_forward = CVARF("m_forward","1", CVAR_ARCHIVE);
cvar_t	m_side = CVARF("m_side","0.8", CVAR_ARCHIVE);

cvar_t	cl_lerp_players = CVARD("cl_lerp_players", "1", "Set this to make other players smoother, though it may increase effective latency. Affects only QuakeWorld.");
cvar_t	cl_predict_players			= CVARD("cl_predict_players", "1", "Clear this cvar to see ents exactly how they are on the server.");
cvar_t	cl_predict_players_frac		= CVARD("cl_predict_players_frac", "0.9", "How much of other players to predict. Values less than 1 will help minimize overruns.");
cvar_t	cl_predict_players_latency	= CVARD("cl_predict_players_latency", "1.0", "Push the player back according to your latency, to give a smooth consistent simulation of the server.");
cvar_t	cl_predict_players_nudge	= CVARD("cl_predict_players_nudge", "0.02", "An extra nudge of time, to cover video latency.");
cvar_t	cl_solid_players = CVARD("cl_solid_players", "1", "Consider other players as solid for player prediction.");
cvar_t	cl_noblink = CVARD("cl_noblink", "0", "Disable the ^^b text blinking feature.");
cvar_t	cl_servername = CVARD("cl_servername", "none", "The hostname of the last server you connected to");
cvar_t	cl_serveraddress = CVARD("cl_serveraddress", "none", "The address of the last server you connected to");
cvar_t	qtvcl_forceversion1 = CVAR("qtvcl_forceversion1", "0");
cvar_t	qtvcl_eztvextensions = CVAR("qtvcl_eztvextensions", "0");

cvar_t	record_flush = CVARD("record_flush", "0", "If set, explicitly flushes demo data to disk while recording. This may be inefficient, depending on how your operating system is configured.");
cvar_t cl_demospeed = CVARF("cl_demospeed", "1", 0);
cvar_t	cl_demoreel = CVARFD("cl_demoreel", "0", CVAR_SAVE, "When enabled, the engine will begin playing a demo loop on startup.");

cvar_t cl_loopbackprotocol = CVARD("cl_loopbackprotocol", "qw", "Which protocol to use for single-player/the internal client. Should be one of: qw, qwid, nqid, nq, fitz, bjp3, dp6, dp7, auto. If 'auto', will use qw protocols for qw mods, and nq protocols for nq mods.");


cvar_t	cl_threadedphysics = CVARD("cl_threadedphysics", "0", "When set, client input frames are generated and sent on a worker thread");

#ifdef QUAKESPYAPI
cvar_t  localid = SCVAR("localid", "");
static qboolean allowremotecmd = true;
#endif

cvar_t	r_drawflame = CVARD("r_drawflame", "1", "Set to -1 to disable ALL static entities. Set to 0 to disable only wall torches and standing flame. Set to 1 for everything drawn as normal.");

qboolean forcesaveprompt;

extern int			total_loading_size, current_loading_size, loading_stage;

//
// info mirrors
//
cvar_t	password	= CVARAF("password",	"",	"pq_password", CVAR_USERINFO | CVAR_NOUNSAFEEXPAND); //this is parhaps slightly dodgy... added pq_password alias because baker seems to be using this for user accounts.
cvar_t	spectator	= CVARF("spectator",	"",			CVAR_USERINFO);
cvar_t	name		= CVARFC("name",		"Player",	CVAR_ARCHIVE | CVAR_USERINFO, Name_Callback);
cvar_t	team		= CVARF("team",			"",			CVAR_ARCHIVE | CVAR_USERINFO);
cvar_t	skin		= CVARAF("skin",		"",			"_cl_playerskin", CVAR_ARCHIVE | CVAR_USERINFO);
cvar_t	model		= CVARAF("model",		"",			"_cl_playermodel", CVAR_ARCHIVE | CVAR_USERINFO);
cvar_t	topcolor	= CVARF("topcolor",		"",			CVAR_ARCHIVE | CVAR_USERINFO);
cvar_t	bottomcolor	= CVARF("bottomcolor",	"",			CVAR_ARCHIVE | CVAR_USERINFO);
cvar_t	rate		= CVARFD("rate",		"30000"/*"6480"*/,		CVAR_ARCHIVE | CVAR_USERINFO, "A rough measure of the bandwidth to try to use while playing. Too high a value may result in 'buffer bloat'.");
cvar_t	drate		= CVARFD("drate",		"3000000",	CVAR_ARCHIVE | CVAR_USERINFO, "A rough measure of the bandwidth to try to use while downloading (in bytes per second).");		// :)
cvar_t	noaim		= CVARF("noaim",		"",			CVAR_ARCHIVE | CVAR_USERINFO);
cvar_t	msg			= CVARFD("msg",			"1",		CVAR_ARCHIVE | CVAR_USERINFO, "Filter console prints/messages. Only functions on QuakeWorld servers. 0=pickup messages. 1=death messages. 2=critical messages. 3=chat.");
cvar_t	b_switch	= CVARF("b_switch",		"",			CVAR_ARCHIVE | CVAR_USERINFO);
cvar_t	w_switch	= CVARF("w_switch",		"",			CVAR_ARCHIVE | CVAR_USERINFO);
cvar_t	cl_nofake	= CVARD("cl_nofake",		"2", "value 0: permits \\r chars in chat messages\nvalue 1: blocks all \\r chars\nvalue 2: allows \\r chars, but only from teammates");
cvar_t	cl_chatsound	= CVAR("cl_chatsound","1");
cvar_t	cl_enemychatsound	= CVAR("cl_enemychatsound", "misc/talk.wav");
cvar_t	cl_teamchatsound	= CVAR("cl_teamchatsound", "misc/talk.wav");

cvar_t	r_torch					= CVARF("r_torch",	"0",	CVAR_CHEAT);
cvar_t	r_rocketlight			= CVARFC("r_rocketlight",	"1", CVAR_ARCHIVE, Cvar_Limiter_ZeroToOne_Callback);
cvar_t	r_lightflicker			= CVAR("r_lightflicker",	"1");
cvar_t	cl_r2g					= CVARFD("cl_r2g",	"0", CVAR_ARCHIVE, "Uses progs/grenade.mdl instead of progs/missile.mdl when 1.");
cvar_t	r_powerupglow			= CVAR("r_powerupglow", "1");
cvar_t	v_powerupshell			= CVARF("v_powerupshell", "0", CVAR_ARCHIVE);
cvar_t	cl_gibfilter			= CVARF("cl_gibfilter", "0", CVAR_ARCHIVE);
cvar_t	cl_deadbodyfilter		= CVARF("cl_deadbodyfilter", "0", CVAR_ARCHIVE);

cvar_t  cl_gunx					= CVAR("cl_gunx", "0");
cvar_t  cl_guny					= CVAR("cl_guny", "0");
cvar_t  cl_gunz					= CVAR("cl_gunz", "0");

cvar_t  cl_gunanglex			= CVAR("cl_gunanglex", "0");
cvar_t  cl_gunangley			= CVAR("cl_gunangley", "0");
cvar_t  cl_gunanglez			= CVAR("cl_gunanglez", "0");

cvar_t	cl_proxyaddr			= CVAR("cl_proxyaddr", "");
cvar_t	cl_sendguid				= CVARD("cl_sendguid", "0", "Send a randomly generated 'globally unique' id to servers, which can be used by servers for score rankings and stuff. Different servers will see different guids. Delete the 'qkey' file in order to appear as a different user.\nIf set to 2, all servers will see the same guid. Be warned that this can show other people the guid that you're using.");
cvar_t	cl_downloads			= CVARFD("cl_downloads", "1", CVAR_NOTFROMSERVER, "Allows you to block all automatic downloads.");
cvar_t	cl_download_csprogs		= CVARFD("cl_download_csprogs", "1", CVAR_NOTFROMSERVER, "Download updated client gamecode if available. Warning: If you clear this to avoid downloading vm code, you should also clear cl_download_packages.");
cvar_t	cl_download_redirection	= CVARFD("cl_download_redirection", "2", CVAR_NOTFROMSERVER, "Follow download redirection to download packages instead of individual files. Also allows the server to send nearly arbitary download commands.\n2: allows redirection only to named packages files (and demos/*.mvd), which is a bit safer.");
cvar_t  cl_download_mapsrc		= CVARFD("cl_download_mapsrc", "", CVAR_ARCHIVE, "Specifies an http location prefix for map downloads. EG: \"http://bigfoot.morphos-team.net/misc/quakemaps/\"");
cvar_t	cl_download_packages	= CVARFD("cl_download_packages", "1", CVAR_NOTFROMSERVER, "0=Do not download packages simply because the server is using them. 1=Download and load packages as needed (does not affect games which do not use this package). 2=Do download and install permanently (use with caution!)");
cvar_t	requiredownloads		= CVARFD("requiredownloads","1", CVAR_ARCHIVE, "0=join the game before downloads have even finished (might be laggy). 1=wait for all downloads to complete before joining.");

cvar_t	cl_muzzleflash			= CVAR("cl_muzzleflash", "1");

cvar_t	gl_simpleitems			= CVARF("gl_simpleitems", "0", CVAR_ARCHIVE);
cvar_t	cl_item_bobbing			= CVARF("cl_model_bobbing", "0", CVAR_ARCHIVE);
cvar_t	cl_countpendingpl		= CVARD("cl_countpendingpl", "0", "If set to 1, packet loss percentages will show packets still in transit as lost, even if they might still be received.");

cvar_t	cl_standardchat			= CVARFD("cl_standardchat", "0", CVAR_ARCHIVE, "Disables auto colour coding in chat messages.");
cvar_t	msg_filter				= CVARD("msg_filter", "0", "Filter out chat messages: 0=neither. 1=broadcast chat. 2=team chat. 3=all chat.");
cvar_t	msg_filter_frags		= CVARD("msg_filter_frags", "0", "Prevents frag messages from appearing on the console.");
cvar_t	msg_filter_pickups		= CVARD("msg_filter_pickups", "0", "Prevents pickup messages from appearing on the console. This would normally be filtered by 'msg 1', but nq servers cannot respect that (nor nq mods running in qw servers).");
cvar_t  cl_standardmsg			= CVARFD("cl_standardmsg", "0", CVAR_ARCHIVE, "Disables auto colour coding in console prints.");
cvar_t  cl_parsewhitetext		= CVARD("cl_parsewhitetext", "1", "When parsing chat messages, enable support for messages like: red{white}red");

cvar_t	cl_dlemptyterminate		= CVAR("cl_dlemptyterminate", "1");

cvar_t	host_mapname			= CVARAF("mapname", "",
									  "host_mapname", 0);

cvar_t	ruleset_allow_playercount			= CVAR("ruleset_allow_playercount", "1");
cvar_t	ruleset_allow_frj					= CVAR("ruleset_allow_frj", "1");
cvar_t	ruleset_allow_semicheats			= CVAR("ruleset_allow_semicheats", "1");
cvar_t	ruleset_allow_packet				= CVAR("ruleset_allow_packet", "1");
cvar_t	ruleset_allow_particle_lightning	= CVAR("ruleset_allow_particle_lightning", "1");
cvar_t	ruleset_allow_overlongsounds		= CVAR("ruleset_allow_overlong_sounds", "1");
cvar_t	ruleset_allow_larger_models			= CVAR("ruleset_allow_larger_models", "1");
cvar_t	ruleset_allow_modified_eyes			= CVAR("ruleset_allow_modified_eyes", "0");
cvar_t	ruleset_allow_sensitive_texture_replacements = CVAR("ruleset_allow_sensitive_texture_replacements", "1");
cvar_t	ruleset_allow_localvolume			= CVAR("ruleset_allow_localvolume", "1");
cvar_t  ruleset_allow_shaders				= CVARF("ruleset_allow_shaders", "1", CVAR_SHADERSYSTEM);
cvar_t  ruleset_allow_watervis				= CVARF("ruleset_allow_watervis", "1", CVAR_SHADERSYSTEM);
cvar_t  ruleset_allow_fbmodels				= CVARF("ruleset_allow_fbmodels", "0", CVAR_SHADERSYSTEM);

extern cvar_t cl_hightrack;
extern cvar_t	vid_renderer;

char cl_screengroup[] = "Screen options";
char cl_controlgroup[] = "client operation options";
char cl_inputgroup[] = "client input controls";
char cl_predictiongroup[] = "Client side prediction";


client_static_t	cls;
client_state_t	cl;

// alot of this should probably be dynamically allocated
entity_state_t	*cl_baselines;
static_entity_t *cl_static_entities;
unsigned int    cl_max_static_entities;
lightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
dlight_t		*cl_dlights;
unsigned int	cl_maxdlights; /*size of cl_dlights array*/

int cl_baselines_count;
int rtlights_first, rtlights_max;

// refresh list
// this is double buffered so the last frame
// can be scanned for oldorigins of trailing objects
int				cl_numvisedicts;
int				cl_maxvisedicts;
entity_t		*cl_visedicts;
int				cl_framecount;

scenetris_t		*cl_stris;
vecV_t			*fte_restrict cl_strisvertv;
vec4_t			*fte_restrict cl_strisvertc;
vec2_t			*fte_restrict cl_strisvertt;
index_t			*fte_restrict cl_strisidx;
unsigned int cl_numstrisidx;
unsigned int cl_maxstrisidx;
unsigned int cl_numstrisvert;
unsigned int cl_maxstrisvert;
unsigned int cl_numstris;
unsigned int cl_maxstris;

static struct
{
	qboolean		trying;
	qboolean		istransfer;		//ignore the user's desired server (don't change connect.adr).
	netadr_t		adr;			//address that we're trying to transfer to. FIXME: support multiple resolved addresses, eg both ::1 AND 127.0.0.1
#ifdef HAVE_DTLS
	enum
	{
		DTLS_DISABLE,
		DTLS_TRY,
		DTLS_REQUIRE,
		DTLS_ACTIVE
	} dtlsupgrade;
#endif
	int				mtu;
	unsigned int	compresscrc;
	int				protocol;		//nq/qw/q2/q3. guessed based upon server replies
	int				subprotocol;	//the monkeys are trying to eat me.
	unsigned int	fteext1;
	unsigned int	fteext2;
	int				qport;
	int				challenge;		//tracked as part of guesswork based upon what replies we get.
	double			time;			//for connection retransmits
	int				defaultport;
	int				tries;			//increased each try, every fourth trys nq connect packets.
	unsigned char	guid[64];
} connectinfo;

quakeparms_t host_parms;

qboolean	host_initialized;		// true if into command execution
qboolean	nomaster;

double		host_frametime;
double		realtime;				// without any filtering or bounding
double		oldrealtime;			// last frame run
int			host_framecount;

qbyte		*host_basepal;
qbyte		*h2playertranslations;

cvar_t	host_speeds = CVAR("host_speeds","0");		// set for running times
#ifdef CRAZYDEBUGGING
cvar_t	developer = CVAR("developer","1");
#else
cvar_t	developer = CVAR("developer","0");
#endif

int			fps_count;
qboolean	forcesaveprompt;

jmp_buf 	host_abort;

void Master_Connect_f (void);

char emodel_name[] =
	{ 'e' ^ 0xff, 'm' ^ 0xff, 'o' ^ 0xff, 'd' ^ 0xff, 'e' ^ 0xff, 'l' ^ 0xff, 0 };
char pmodel_name[] =
	{ 'p' ^ 0xff, 'm' ^ 0xff, 'o' ^ 0xff, 'd' ^ 0xff, 'e' ^ 0xff, 'l' ^ 0xff, 0 };
char prespawn_name[] =
	{ 'p'^0xff, 'r'^0xff, 'e'^0xff, 's'^0xff, 'p'^0xff, 'a'^0xff, 'w'^0xff, 'n'^0xff,
		' '^0xff, '%'^0xff, 'i'^0xff, ' '^0xff, '0'^0xff, ' '^0xff, '%'^0xff, 'i'^0xff, 0 };
char modellist_name[] =
	{ 'm'^0xff, 'o'^0xff, 'd'^0xff, 'e'^0xff, 'l'^0xff, 'l'^0xff, 'i'^0xff, 's'^0xff, 't'^0xff,
		' '^0xff, '%'^0xff, 'i'^0xff, ' '^0xff, '%'^0xff, 'i'^0xff, 0 };
char soundlist_name[] =
	{ 's'^0xff, 'o'^0xff, 'u'^0xff, 'n'^0xff, 'd'^0xff, 'l'^0xff, 'i'^0xff, 's'^0xff, 't'^0xff,
		' '^0xff, '%'^0xff, 'i'^0xff, ' '^0xff, '%'^0xff, 'i'^0xff, 0 };

void CL_UpdateWindowTitle(void)
{
	if (VID_SetWindowCaption)
	{
		if (cl.windowtitle)
		{	//gamecode wanted some explicit title.
			VID_SetWindowCaption(cl.windowtitle);
			return;
		}
		else
		{
			char title[2048];
			switch (cls.state)
			{
			default:
#ifndef CLIENTONLY
				if (sv.state)
					Q_snprintfz(title, sizeof(title), "%s: %s", fs_gamename.string, svs.name);
				else
#endif
				if (cls.demoplayback)
					Q_snprintfz(title, sizeof(title), "%s: %s", fs_gamename.string, cls.lastdemoname);
				else
					Q_snprintfz(title, sizeof(title), "%s: %s", fs_gamename.string, cls.servername);
				break;
			case ca_disconnected:
				if (CSQC_UnconnectedOkay(false))	//pure csqc mods can have a world model and yet be disconnected. we don't really know what the current map should be called though.
					Q_snprintfz(title, sizeof(title), "%s", fs_gamename.string);
				else
					Q_snprintfz(title, sizeof(title), "%s: disconnected", fs_gamename.string);
				break;
			}
			VID_SetWindowCaption(title);
		}
	}
}

void CL_MakeActive(char *gamename)
{
	extern int fs_finds;
	if (fs_finds)
	{
		Con_DPrintf("%i additional FS searches\n", fs_finds);
		fs_finds = 0;
	}
	cl.matchgametimestart = 0;
	cls.state = ca_active;

	//this might be expensive, don't count any of this as time spent *playing* the demo. this avoids skipping the first $LOADDURATION seconds.
	cl.stillloading = true;

	//kill sounds left over from the last map.
	S_Purge(true);

	//kill models left over from the last map.
	Mod_Purge(MP_MAPCHANGED);
	Image_Purge();

	//and reload shaders now if needed (this was blocked earlier)
	Shader_DoReload();

	SCR_EndLoadingPlaque();
	CL_UpdateWindowTitle();

	TP_ExecTrigger("f_begin", true);
	if (cls.demoplayback)
		TP_ExecTrigger("f_spawndemo", true);
	else
		TP_ExecTrigger("f_spawn", false);
}
/*
==================
CL_Quit_f
==================
*/
void CL_Quit_f (void)
{
	if (!host_initialized)
		return;

	if (forcesaveprompt && strcmp(Cmd_Argv(1), "force"))
	{
		forcesaveprompt = false;
		if (Cmd_Exists("menu_quit"))
		{
			Cmd_ExecuteString("menu_quit", RESTRICT_LOCAL);
			return;
		}
	}

	TP_ExecTrigger("f_quit", true);
	Cbuf_Execute();
/*
#ifndef CLIENTONLY
	if (!isDedicated)
#endif
	{
		M_Menu_Quit_f ();
		return;
	}*/
	Sys_Quit ();
}

void CL_ConnectToDarkPlaces(char *challenge, netadr_t *adr)
{
	char	data[2048];
	cls.fteprotocolextensions = 0;
	cls.fteprotocolextensions2 = 0;

	cls.resendinfo = false;

	connectinfo.time = realtime;	// for retransmit requests

	Q_snprintfz(data, sizeof(data), "%c%c%c%cconnect\\protocol\\darkplaces 3\\protocols\\DP7 DP6 DP5 RMQ FITZ NEHAHRABJP2 NEHAHRABJP NEHAHRABJP3 QUAKE\\challenge\\%s\\name\\%s", 255, 255, 255, 255, challenge, name.string);

	NET_SendPacket (NS_CLIENT, strlen(data), data, adr);

	cl.splitclients = 0;
}

#ifdef PROTOCOL_VERSION_FTE
void CL_SupportedFTEExtensions(int *pext1, int *pext2)
{
	unsigned int fteprotextsupported = 0;
	unsigned int fteprotextsupported2 = 0;

	fteprotextsupported = Net_PextMask(1, false);
	fteprotextsupported2 = Net_PextMask(2, false);

	fteprotextsupported &= strtoul(cl_pext_mask.string, NULL, 16);
//	fteprotextsupported2 &= strtoul(cl_pext2_mask.string, NULL, 16);

	if (cl_nopext.ival)
	{
		fteprotextsupported = 0;
		fteprotextsupported2 = 0;
	}

	*pext1 = fteprotextsupported;
	*pext2 = fteprotextsupported2;
}
#endif

char *CL_GUIDString(netadr_t *adr)
{
	static qbyte buf[2048];
	static int buflen;
	unsigned int digest[4];
	char serveraddr[256];
	void *blocks[2];
	int lens[2];

	if (!cl_sendguid.ival)
		return NULL;

	if (*connectinfo.guid && connectinfo.istransfer)
		return connectinfo.guid;

	if (!buflen)
	{
		vfsfile_t *f;
		f = FS_OpenVFS("qkey", "rb", FS_ROOT);
		if (f)
		{
			buflen = VFS_GETLEN(f);
			if (buflen > 2048)
				buflen = 2048;
			buflen = VFS_READ(f, buf, buflen);
			VFS_CLOSE(f);
		}
		if (buflen < 16)
		{
			buflen = sizeof(buf);
			if (!Sys_RandomBytes(buf, buflen))
			{
				int i;
				srand(time(NULL));
				for (i = 0; i < buflen; i++)
					buf[i] = rand() & 0xff;
			}
			f = FS_OpenVFS("qkey", "wb", FS_ROOT);
			if (f)
			{
				VFS_WRITE(f, buf, buflen);
				VFS_CLOSE(f);
			}
		}
	}

	if (cl_sendguid.ival == 2)
		*serveraddr = 0;
	else
		NET_AdrToString(serveraddr, sizeof(serveraddr), adr);

	blocks[0] = buf;lens[0] = buflen;
	blocks[1] = serveraddr;lens[1] = strlen(serveraddr);
	Com_BlocksChecksum(2, blocks, lens, (void*)digest);

	Q_snprintfz(connectinfo.guid, sizeof(connectinfo.guid), "%08x%08x%08x%08x", digest[0], digest[1], digest[2], digest[3]);
	return connectinfo.guid;
}

/*
=======================
CL_SendConnectPacket

called by CL_Connect_f and CL_CheckResend
======================
*/
void CL_SendConnectPacket (netadr_t *to, int mtu, 
#ifdef PROTOCOL_VERSION_FTE
						   int ftepext, int ftepext2,
#endif
						   int compressioncrc
						  /*, ...*/)	//dmw new parms
{
	extern cvar_t qport;
	netadr_t	addr;
	char	data[2048];
	char *info;
	double t1, t2;
#ifdef PROTOCOL_VERSION_FTE
	int fteprotextsupported=0;
	int fteprotextsupported2=0;
#endif
	char *a;

// JACK: Fixed bug where DNS lookups would cause two connects real fast
//       Now, adds lookup time to the connect time.
//		 Should I add it to realtime instead?!?!

	if (!connectinfo.trying)
		return;

	if (cl_nopext.ival)	//imagine it's an unenhanced server
	{
		compressioncrc = 0;
	}

#ifdef PROTOCOL_VERSION_FTE
#ifdef Q2CLIENT
	if (connectinfo.protocol == CP_QUAKE2)
	{
		fteprotextsupported = ftepext & (PEXT_MODELDBL|PEXT_SOUNDDBL|PEXT_SPLITSCREEN);
		fteprotextsupported2 = 0;
	}
	else
#endif
	{
		CL_SupportedFTEExtensions(&fteprotextsupported, &fteprotextsupported2);

		fteprotextsupported &= ftepext;
		fteprotextsupported2 &= ftepext2;

		if (connectinfo.protocol != CP_QUAKEWORLD)
		{
			fteprotextsupported = 0;
			fteprotextsupported2 = 0;
		}
	}

	connectinfo.fteext1 = fteprotextsupported;
	connectinfo.fteext2 = fteprotextsupported2;
#endif

	t1 = Sys_DoubleTime ();

	if (!to)
	{
		to = &addr;
		if (!NET_StringToAdr (cls.servername, PORT_QWSERVER, to))
		{
			Con_TPrintf ("Bad server address\n");
			connectinfo.trying = false;
			return;
		}
	}

	NET_AdrToString(data, sizeof(data), to);
	Cvar_ForceSet(&cl_serveraddress, data);
//	Info_SetValueForStarKey (cls.userinfo, "*ip", data, MAX_INFO_STRING);

	if (!NET_IsClientLegal(to))
	{
		Con_TPrintf ("Illegal server address\n");
		connectinfo.trying = false;
		return;
	}

	t2 = Sys_DoubleTime ();

	cls.resendinfo = false;

	connectinfo.time = realtime+t2-t1;	// for retransmit requests

	//fixme: we shouldn't cycle these so much
	connectinfo.qport = qport.value;
	Cvar_SetValue(&qport, (connectinfo.qport+1)&0xffff);

	if (connectinfo.protocol == CP_QUAKE2 && (connectinfo.subprotocol == PROTOCOL_VERSION_R1Q2 || connectinfo.subprotocol == PROTOCOL_VERSION_Q2PRO))
		connectinfo.qport &= 0xff;

#ifdef Q3CLIENT
	if (connectinfo.protocol == CP_QUAKE3)
	{	//q3 requires some very strange things.
		CLQ3_SendConnectPacket(to, connectinfo.challenge, connectinfo.qport);
		return;
	}
#endif

	Q_snprintfz(data, sizeof(data), "%c%c%c%cconnect", 255, 255, 255, 255);

	Q_strncatz(data, va(" %i %i %i", connectinfo.subprotocol, connectinfo.qport, connectinfo.challenge), sizeof(data));

	//userinfo0 has some twiddles for extensions from other qw engines.
	Q_strncatz(data, " \"", sizeof(data));
	//qwfwd proxy routing
	if ((a = strrchr(cls.servername, '@')))
	{
		*a = 0;
		Q_strncatz(data, va("\\prx\\%s", cls.servername), sizeof(data));
		*a = '@';
	}
	//the info itself
	Q_strncatz(data, cls.userinfo[0], sizeof(data));
	if (connectinfo.protocol == CP_QUAKEWORLD)	//zquake extension info.
		Q_strncatz(data, va("\\*z_ext\\%i", SUPPORTED_Z_EXTENSIONS), sizeof(data));

	Q_strncatz(data, "\"", sizeof(data));

	if (connectinfo.protocol == CP_QUAKE2 && connectinfo.subprotocol == PROTOCOL_VERSION_R1Q2)
		Q_strncatz(data, va(" %d %d", mtu, 1905), sizeof(data));	//mti, sub-sub-version
	else if (connectinfo.protocol == CP_QUAKE2 && connectinfo.subprotocol == PROTOCOL_VERSION_Q2PRO)
		Q_strncatz(data, va(" %d 0 0 %d", mtu, 1021), sizeof(data));	//mtu, netchan-fragmentation, zlib, sub-sub-version

	Q_strncatz(data, "\n", sizeof(data));

#ifdef PROTOCOL_VERSION_FTE
	if (ftepext)
		Q_strncatz(data, va("0x%x 0x%x\n", PROTOCOL_VERSION_FTE, fteprotextsupported), sizeof(data));
#endif
#ifdef PROTOCOL_VERSION_FTE2
	if (ftepext2)
		Q_strncatz(data, va("0x%x 0x%x\n", PROTOCOL_VERSION_FTE2, fteprotextsupported2), sizeof(data));
#endif

	{
		int ourmtu;
		if (to->type == NA_LOOPBACK)
			ourmtu = MAX_UDP_PACKET;
		else if (*net_mtu.string)
			ourmtu = net_mtu.ival;
		else
			ourmtu = 1440;	//a safe bet. servers have an unsafe bet by default
		if (ourmtu < 0)
			ourmtu = 0;
		if (mtu > ourmtu)
			mtu = ourmtu;
		connectinfo.mtu = mtu & ~7;

		if (connectinfo.mtu > 0)
			Q_strncatz(data, va("0x%x %i\n", PROTOCOL_VERSION_FRAGMENT, connectinfo.mtu), sizeof(data));
	}

#ifdef HUFFNETWORK
	if (compressioncrc && net_compress.ival && Huff_CompressionCRC(compressioncrc))
	{
		Q_strncatz(data, va("0x%x 0x%x\n", PROTOCOL_VERSION_HUFFMAN, LittleLong(compressioncrc)), sizeof(data));
		connectinfo.compresscrc = compressioncrc;
	}
	else
#endif
		connectinfo.compresscrc = 0;

	info = CL_GUIDString(to);
	if (info)
		Q_strncatz(data, va("0x%x \"%s\"\n", PROTOCOL_INFO_GUID, info), sizeof(data));

	NET_SendPacket (NS_CLIENT, strlen(data), data, to);
}

char *CL_TryingToConnect(void)
{
	if (!connectinfo.trying)
		return NULL;

	return cls.servername;
}

/*
=================
CL_CheckForResend

Resend a connect message if the last one has timed out

=================
*/
void CL_CheckForResend (void)
{
	char	data[2048];
	double t1, t2;
	int contype = 0;
	qboolean keeptrying = true;
	char *host;
	extern int	r_blockvidrestart;
	const char *lbp;

#ifndef CLIENTONLY
	if (!cls.state && (!connectinfo.trying || sv.state != ss_clustermode) && sv.state)
	{
#ifdef NQPROT
		qboolean proquakeangles = false;
#endif
#ifdef NETPREPARSE
		extern cvar_t dpcompat_nopreparse;
#endif
		memset(&connectinfo, 0, sizeof(connectinfo));
		Q_strncpyz (cls.servername, "internalserver", sizeof(cls.servername));
		Cvar_ForceSet(&cl_servername, cls.servername);
		if (!NET_StringToAdr(cls.servername, 0, &connectinfo.adr))
			return;	//erk?
		connectinfo.trying = true;
		connectinfo.istransfer = false;
		connectinfo.adr.prot = NP_DGRAM;

		NET_InitClient(true);

		cls.state = ca_disconnected;
		switch (svs.gametype)
		{
#ifdef Q3CLIENT
		case GT_QUAKE3:
			connectinfo.protocol = CP_QUAKE3;
			break;
#endif
#ifdef Q2CLIENT
		case GT_QUAKE2:
			connectinfo.protocol = CP_QUAKE2;
			connectinfo.subprotocol = PROTOCOL_VERSION_Q2;
			connectinfo.fteext1 = PEXT_MODELDBL|PEXT_SOUNDDBL|PEXT_SPLITSCREEN;
			connectinfo.fteext2 = 0;
			break;
#endif
		default:
			cl.movesequence = 0;
			lbp = cl_loopbackprotocol.string;
			if (!strcmp(lbp, "") || !strcmp(lbp, "qw") || progstype == PROG_H2)
			{	//qw with all supported extensions -default
				//for hexen2 we always force fte's native qw protocol. other protocols won't cut it.
				connectinfo.protocol = CP_QUAKEWORLD;
				connectinfo.subprotocol = PROTOCOL_VERSION_QW;
				connectinfo.fteext1 = Net_PextMask(1, false);
				connectinfo.fteext2 = Net_PextMask(2, false);
			}
			else if (!strcmp(lbp, "qwid") || !strcmp(cl_loopbackprotocol.string, "idqw"))
			{	//for recording .qwd files in any client
				connectinfo.protocol = CP_QUAKEWORLD;
				connectinfo.subprotocol = PROTOCOL_VERSION_QW;
				connectinfo.fteext1 = 0;
				connectinfo.fteext2 = 0;
			}
#ifdef Q3CLIENT
			else if (!strcmp(lbp, "q3"))
				cls.protocol = CP_QUAKE3;
#endif
#ifdef NQPROT
			else if (!strcmp(lbp, "random"))
			{	//for debugging.
				if (rand() & 1)
				{
					connectinfo.protocol = CP_NETQUAKE;
					connectinfo.subprotocol = CPNQ_FITZ666;
				}
				else
				{
					connectinfo.protocol = CP_QUAKEWORLD;
					connectinfo.subprotocol = PROTOCOL_VERSION_QW;
					connectinfo.fteext1 = Net_PextMask(1, false);
					connectinfo.fteext2 = Net_PextMask(2, false);
				}
			}
			else if (!strcmp(lbp, "fitz") || !strcmp(lbp, "666") || !strcmp(lbp, "999"))
			{	//we don't really distinguish between fitz and rmq protocols. we just use 999 with bigcoords and 666 othewise.
				connectinfo.protocol = CP_NETQUAKE;
				connectinfo.subprotocol = CPNQ_FITZ666;
			}
			else if (!strcmp(lbp, "bjp3") || !strcmp(lbp, "bjp"))
			{
				connectinfo.protocol = CP_NETQUAKE;
				connectinfo.subprotocol = CPNQ_BJP3;
			}
			else if (!strcmp(lbp, "nq"))
			{
				connectinfo.protocol = CP_NETQUAKE;
				connectinfo.subprotocol = CPNQ_ID;
				proquakeangles = true;
			}
			else if (!strcmp(lbp, "nqid") || !strcmp(lbp, "idnq"))
			{
				connectinfo.protocol = CP_NETQUAKE;
				connectinfo.subprotocol = CPNQ_ID;
			}
			else if (!strcmp(lbp, "dp6") || !strcmp(lbp, "dpp6"))
			{
				connectinfo.protocol = CP_NETQUAKE;
				connectinfo.subprotocol = CPNQ_DP6;
			}
			else if (!strcmp(lbp, "dp7") || !strcmp(lbp, "dpp7"))
			{
				connectinfo.protocol = CP_NETQUAKE;
				connectinfo.subprotocol = CPNQ_DP7;
			}
			else if (progstype != PROG_QW && progstype != PROG_H2)	//h2 depends on various extensions and doesn't really match either protocol, but we go for qw because that gives us all sorts of extensions.
			{
				connectinfo.protocol = CP_NETQUAKE;
				connectinfo.subprotocol = CPNQ_FITZ666;
				connectinfo.fteext1 = Net_PextMask(1, true);
				connectinfo.fteext2 = Net_PextMask(2, true);
			}
#endif
			else
			{	//protocol wasn't recognised, and we didn't take the nq fallback, so that must mean we're going for qw.
				connectinfo.protocol = CP_QUAKEWORLD;
				connectinfo.subprotocol = PROTOCOL_VERSION_QW;
				connectinfo.fteext1 = Net_PextMask(1, false);
				connectinfo.fteext2 = Net_PextMask(2, false);
			}

#ifdef NETPREPARSE
			if (dpcompat_nopreparse.ival)
#endif
			{
				//disabling preparsing with hexen2 is unsupported.
				if (progstype == PROG_H2)
					Con_Printf("dpcompat_nopreparse is unsupported with hexen2\n");
				else if (progstype == PROG_QW && cls.protocol != CP_QUAKEWORLD)
				{
					connectinfo.protocol = CP_QUAKEWORLD;
					connectinfo.subprotocol = PROTOCOL_VERSION_QW;
					connectinfo.fteext1 = Net_PextMask(1, false);
					connectinfo.fteext2 = Net_PextMask(2, false);
				}
				else if (progstype != PROG_QW && cls.protocol == CP_QUAKEWORLD)
				{
					connectinfo.protocol = CP_NETQUAKE;
					connectinfo.subprotocol = CPNQ_DP7;	//dpcompat_nopreparse is only really needed for DP mods that send unknowable svc_tempentity messages to the client.
				}
			}

			//make sure the protocol within demos is actually correct/sane
			if (cls.demorecording == DPB_QUAKEWORLD && cls.protocol != CP_QUAKEWORLD)
			{
				connectinfo.protocol = CP_QUAKEWORLD;
				connectinfo.subprotocol = PROTOCOL_VERSION_QW;
				connectinfo.fteext1 = Net_PextMask(1, false);
				connectinfo.fteext2 = Net_PextMask(2, false);
			}
#ifdef NQPROT
			else if (cls.demorecording == DPB_NETQUAKE && cls.protocol != CP_NETQUAKE)
			{
				connectinfo.protocol = CP_NETQUAKE;
				connectinfo.subprotocol = CPNQ_FITZ666;
				//FIXME: use pext.
			}
#endif
#ifdef Q2CLIENT
			else if (cls.demorecording == DPB_QUAKE2 && cls.protocol != CP_QUAKE2)
			{
				connectinfo.protocol = CP_QUAKE2;
				connectinfo.subprotocol = PROTOCOL_VERSION_Q2;
				connectinfo.fteext1 = PEXT_MODELDBL|PEXT_SOUNDDBL|PEXT_SPLITSCREEN;
				//FIXME: use pext.
			}
#endif
			break;
		}

		CL_FlushClientCommands();	//clear away all client->server clientcommands.

#ifdef NQPROT
		if (connectinfo.protocol == CP_NETQUAKE)
		{
			if (!NET_StringToAdr (cls.servername, connectinfo.defaultport, &connectinfo.adr))
			{
				Con_TPrintf ("Bad server address\n");
				connectinfo.trying = false;
				SCR_EndLoadingPlaque();
				return;
			}
			NET_AdrToString(data, sizeof(data), &connectinfo.adr);

			/*eat up the server's packets, to clear any lingering loopback packets (like disconnect commands... yes this might cause packetloss for other clients)*/
			while(NET_GetPacket (NS_SERVER, 0) >= 0)
			{
			}
			net_message.packing = SZ_RAWBYTES;
			net_message.cursize = 0;
			MSG_BeginReading(net_message.prim);

			if (connectinfo.subprotocol == CPNQ_ID && !proquakeangles)
			{
				net_from = connectinfo.adr;
				Cmd_TokenizeString (va("connect %i %i %i \"\\name\\unconnected\"", NQ_NETCHAN_VERSION, 0, SV_NewChallenge()), false, false);

				SVC_DirectConnect();
			}
			else if (connectinfo.subprotocol == CPNQ_BJP3)
			{
				net_from = connectinfo.adr;
				Cmd_TokenizeString (va("connect %i %i %i \"\\name\\unconnected\\mod\\%i\"", NQ_NETCHAN_VERSION, 0, SV_NewChallenge(), PROTOCOL_VERSION_BJP3), false, false);

				SVC_DirectConnect();
			}
			else if (connectinfo.subprotocol == CPNQ_FITZ666)
			{
				net_from = connectinfo.adr;
				Cmd_TokenizeString (va("connect %i %i %i \"\\name\\unconnected\\mod\\%i\"", NQ_NETCHAN_VERSION, 0, SV_NewChallenge(), PROTOCOL_VERSION_FITZ), false, false);

				SVC_DirectConnect();
			}
			else if (proquakeangles)
			{
				net_from = connectinfo.adr;
				Cmd_TokenizeString (va("connect %i %i %i \"\\name\\unconnected\\mod\\1\"", NQ_NETCHAN_VERSION, 0, SV_NewChallenge()), false, false);

				SVC_DirectConnect();
			}
			else
				CL_ConnectToDarkPlaces("", &connectinfo.adr);
//			connectinfo.trying = false;
		}
		else
#endif
		{
			if (!connectinfo.challenge)
				connectinfo.challenge = rand();
			CL_SendConnectPacket (NULL, 8192-16, connectinfo.fteext1, connectinfo.fteext2, false);
		}

		return;
	}
#endif

	if (!connectinfo.trying)
		return;
	if (startuppending || r_blockvidrestart)
		return;	//don't send connect requests until we've actually initialised fully. this isn't a huge issue, but makes the startup prints a little more sane.

	/*
#ifdef NQPROT
	if (connect_type)
	{
		if (!connect_time || !(realtime - connect_time < 5.0))
		{
			connect_time = realtime;
			NQ_BeginConnect(cls.servername);
			NQ_ContinueConnect(cls.servername);
		}
		else
			NQ_ContinueConnect(cls.servername);
		return;
	}
#endif
	*/
	if (connectinfo.time && realtime - connectinfo.time < 5.0)
		return;

	NET_InitClient(false);

	t1 = Sys_DoubleTime ();
	if (!connectinfo.istransfer)
	{
		host = strrchr(cls.servername, '@');
		if (host)
			host++;
		else
			host = cls.servername;

		if (!NET_StringToAdr (host, connectinfo.defaultport, &connectinfo.adr))
		{
			Con_TPrintf ("Bad server address\n");
			connectinfo.trying = false;
			SCR_EndLoadingPlaque();
			return;
		}

#ifdef HAVE_DTLS
		if (connectinfo.dtlsupgrade == DTLS_ACTIVE)
		{	//if we've already established a dtls connection, stick with it
			if (connectinfo.adr.prot == NP_DGRAM)
				connectinfo.adr.prot = NP_DTLS;
		}
		else if (connectinfo.adr.prot == NP_DTLS)
		{	//dtls connections start out with regular udp, and upgrade to dtls once its established that the server supports it.
			connectinfo.dtlsupgrade = DTLS_REQUIRE;
			connectinfo.adr.prot = NP_DGRAM;
		}
		else
		{
			//hostname didn't specify dtls. upgrade if we're allowed, but don't mandate it.
			//connectinfo.dtlsupgrade = DTLS_TRY;
		}
#endif
	}
	if (!NET_IsClientLegal(&connectinfo.adr))
	{
		Con_TPrintf ("Illegal server address\n");
		SCR_EndLoadingPlaque();
		connectinfo.trying = false;
		return;
	}

	CL_FlushClientCommands();

	t2 = Sys_DoubleTime ();

	connectinfo.time = realtime+t2-t1;	// for retransmit requests

	Cvar_ForceSet(&cl_servername, cls.servername);

#ifdef Q3CLIENT
	//Q3 clients send their cdkey to the q3 authorize server.
	//they send this packet with the challenge.
	//and the server will refuse the client if it hasn't sent it.
	CLQ3_SendAuthPacket(&connectinfo.adr);
#endif

	if (connectinfo.istransfer)
		Con_TPrintf ("Connecting to %s(%s)...\n", cls.servername, NET_AdrToString(data, sizeof(data), &connectinfo.adr));
	else
		Con_TPrintf ("Connecting to %s...\n", cls.servername);

	if (connectinfo.tries == 0)
		if (!NET_EnsureRoute(cls.sockets, "conn", cls.servername))
		{
			Con_Printf ("Unable to establish connection to %s\n", cls.servername);
			connectinfo.trying = false;
			SCR_EndLoadingPlaque();
			return;
		}

	contype |= 1; /*always try qw type connections*/
//	if ((connect_tries&3)==3) || (connect_defaultport==26000))
		contype |= 2; /*try nq connections periodically (or if its the default nq port)*/

	/*DP, QW, Q2, Q3*/
	if (contype & 1)
	{
		Q_snprintfz (data, sizeof(data), "%c%c%c%cgetchallenge\n", 255, 255, 255, 255);
		switch(NET_SendPacket (NS_CLIENT, strlen(data), data, &connectinfo.adr))
		{
		case NETERR_CLOGGED:	//temporary failure
		case NETERR_SENT:		//yay, works!
			break;
		default:
			keeptrying = false;
			break;
		}
	}
	/*NQ*/
#ifdef NQPROT
	if (contype & 2)
	{
		sizebuf_t sb;
		memset(&sb, 0, sizeof(sb));
		sb.data = data;
		sb.maxsize = sizeof(data);

		MSG_WriteLong(&sb, LongSwap(NETFLAG_CTL | (strlen(NQ_NETCHAN_GAMENAME)+7)));
		MSG_WriteByte(&sb, CCREQ_CONNECT);
		MSG_WriteString(&sb, NQ_NETCHAN_GAMENAME);
		MSG_WriteByte(&sb, NQ_NETCHAN_VERSION);

		/*NQ engines have a few extra bits on the end*/
		/*proquake servers wait for us to send them a packet before anything happens,
		  which means it corrects for our public port if our nat uses different public ports for different remote ports
		  thus all nq engines claim to be proquake
		*/

		MSG_WriteByte(&sb, 1); /*'mod'*/
		MSG_WriteByte(&sb, 34); /*'mod' version*/
		MSG_WriteByte(&sb, 0); /*flags*/
		MSG_WriteLong(&sb, strtoul(password.string, NULL, 0)); /*password*/

		/*FTE servers will detect this string and treat it as a qw challenge instead (if it allows qw clients), so protocol choice is deterministic*/
		if (contype & 1)
			MSG_WriteString(&sb, "getchallenge");

		*(int*)sb.data = LongSwap(NETFLAG_CTL | sb.cursize);
		switch(NET_SendPacket (NS_CLIENT, sb.cursize, sb.data, &connectinfo.adr))
		{
		case NETERR_CLOGGED:	//temporary failure
		case NETERR_SENT:		//yay, works!
			break;
		default:
			keeptrying = false;
			break;
		}
	}
#endif

	connectinfo.tries++;

	if (!keeptrying)
	{
		Con_TPrintf ("No route to host, giving up\n");
		connectinfo.trying = false;
		SCR_EndLoadingPlaque();
	}
}

void CL_BeginServerConnect(const char *host, int port, qboolean noproxy)
{
	if (strstr(host, "://") || !*cl_proxyaddr.string || noproxy)
		Q_strncpyz (cls.servername, host, sizeof(cls.servername));
	else
		Q_snprintfz(cls.servername, sizeof(cls.servername), "%s@%s", host, cl_proxyaddr.string);

	if (!port)
		port = cl_defaultport.value;
#ifdef HAVE_DTLS
	NET_DTLS_Disconnect(cls.sockets, &connectinfo.adr);
#endif
	memset(&connectinfo, 0, sizeof(connectinfo));
	connectinfo.trying = true;
	connectinfo.defaultport = port;
	connectinfo.protocol = CP_UNKNOWN;
	SCR_SetLoadingStage(LS_CONNECTION);
	CL_CheckForResend();
}

void CL_BeginServerReconnect(void)
{
#ifndef CLIENTONLY
	if (isDedicated)
	{
		Con_TPrintf ("Connect ignored - dedicated. set a renderer first\n");
		return;
	}
#endif
#ifdef HAVE_DTLS
	NET_DTLS_Disconnect(cls.sockets, &connectinfo.adr);
	connectinfo.dtlsupgrade = 0;
#endif
	connectinfo.trying = true;
	connectinfo.istransfer = false;
	connectinfo.time = 0;
}

void CL_Transfer_f(void)
{
	char oldguid[64];
	char	*server;
	if (Cmd_Argc() != 2)
	{
		Con_TPrintf ("usage: cl_transfer <server>\n");
		return;
	}

	server = Cmd_Argv (1);
	if (!*server)
	{
		//if they didn't specify a server, abort any active transfer/connection.
		connectinfo.trying = false;
		return;
	}

	Q_strncpyz(oldguid, connectinfo.guid, sizeof(oldguid));
	memset(&connectinfo, 0, sizeof(connectinfo));
	if (NET_StringToAdr(server, 0, &connectinfo.adr))
	{
		if (cls.state)
		{
			connectinfo.istransfer = true;
			Q_strncpyz(connectinfo.guid, oldguid, sizeof(oldguid));	//retain the same guid on transfers
		}
		connectinfo.trying = true;
		connectinfo.defaultport = cl_defaultport.value;
		connectinfo.protocol = CP_UNKNOWN;
		SCR_SetLoadingStage(LS_CONNECTION);
		CL_CheckForResend();
	}
	else
	{
		Con_Printf("cl_transfer: bad address\n");
	}
}
/*
================
CL_Connect_f

================
*/
void CL_Connect_f (void)
{
	char	*server;

	if (Cmd_Argc() != 2)
	{
		Con_TPrintf ("usage: connect <server>\n");
		return;
	}

	server = Cmd_Argv (1);

#ifndef CLIENTONLY
	if (sv.state == ss_clustermode)
		CL_Disconnect ();
	else
#endif
		CL_Disconnect_f ();

	CL_BeginServerConnect(server, 0, false);
}
#if defined(CL_MASTER)
static void CL_ConnectBestRoute_f (void)
{
	char	server[1024];
	int		proxies;
	int		directcost, chainedcost;
	if (Cmd_Argc() != 2)
	{
		Con_TPrintf ("usage: connectbr <server>\n");
		return;
	}

	proxies = Master_FindBestRoute(Cmd_Argv(1), server, sizeof(server), &directcost, &chainedcost);
	if (!*server)
	{
		Con_TPrintf ("Unable to route to server\n");
		return;
	}
	else if (proxies < 0)
		Con_TPrintf ("Routing database is not initialised, connecting directly\n");
	else if (!proxies)
		Con_TPrintf ("Routing table favours a direct connection\n");
	else if (proxies == 1)
		Con_TPrintf ("Routing table favours a single proxy (%ims vs %ims)\n", chainedcost, directcost);
	else
		Con_TPrintf ("Routing table favours chaining through %i proxies (%ims vs %ims)\n", proxies, chainedcost, directcost);

#ifndef CLIENTONLY
	if (sv.state == ss_clustermode)
		CL_Disconnect ();
	else
#endif
		CL_Disconnect_f ();
	CL_BeginServerConnect(server, 0, true);
}
#endif

static void CL_Join_f (void)
{
	char	*server;

	if (Cmd_Argc() != 2)
	{
		if (cls.state)
		{	//Hmm. This server sucks.
			if ((cls.z_ext & Z_EXT_JOIN_OBSERVE) || cls.protocol != CP_QUAKEWORLD)
				Cmd_ForwardToServer();
			else
				Cbuf_AddText("\nspectator 0;reconnect\n", RESTRICT_LOCAL);
			return;
		}
		Con_Printf ("join requires a connection or servername/ip\n");
		return;
	}

	server = Cmd_Argv (1);

	CL_Disconnect_f ();

	Cvar_Set(&spectator, "0");

	CL_BeginServerConnect(server, 0, false);
}

void CL_Observe_f (void)
{
	char	*server;

	if (Cmd_Argc() != 2)
	{
		if (cls.state)
		{
			if ((cls.z_ext & Z_EXT_JOIN_OBSERVE) || cls.protocol != CP_QUAKEWORLD)
				Cmd_ForwardToServer();
			else	//Hmm. This server sucks.
				Cbuf_AddText("\nspectator 1;reconnect\n", RESTRICT_LOCAL);
			return;
		}
		Con_Printf ("observe requires a connection or servername/ip\n");
		return;
	}

	server = Cmd_Argv (1);

	CL_Disconnect_f ();

	Cvar_Set(&spectator, "1");

	CL_BeginServerConnect(server, 0, false);
}

#ifdef NQPROT
void CLNQ_Connect_f (void)
{
	char	*server;

	if (Cmd_Argc() != 2)
	{
		Con_TPrintf ("usage: connect <server>\n");
		return;
	}

	server = Cmd_Argv (1);

	CL_Disconnect_f ();

	CL_BeginServerConnect(server, 26000, true);
}
#endif
 
#ifdef IRCCONNECT
void CL_IRCConnect_f (void)
{
	CL_Disconnect_f ();

	if (FTENET_AddToCollection(cls.sockets, "TCP", Cmd_Argv(2), NA_IP, NP_IRC, false))
	{
		char *server;
		server = Cmd_Argv (1);

		CL_BeginServerConnect(va("irc://%s", server), 0, true);
	}
}
#endif

#ifdef TCPCONNECT
void CL_TCPConnect_f (void)
{
	if (!Q_strcasecmp(Cmd_Argv(0), "tlsconnect"))
		Cbuf_InsertText(va("connect tls://%s", Cmd_Argv(1)), Cmd_ExecLevel, true);
	else
		Cbuf_InsertText(va("connect tcp://%s", Cmd_Argv(1)), Cmd_ExecLevel, true);
}
#endif

/*
=====================
CL_Rcon_f

  Send the rest of the command line over as
  an unconnected command.
=====================
*/
void CL_Rcon_f (void)
{
	char	message[1024];
	char	*password;
	int		i;
	netadr_t	to;

	i = 1;
	password = rcon_password.string;
	if (!*password)	//FIXME: this is strange...
	{
		if (Cmd_Argc() < 3)
		{
			Con_TPrintf ("'rcon_password' is not set.\n");
			Con_TPrintf("usage: rcon (password) <command>\n");
			return;
		}
		password = Cmd_Argv(1);
		i = 2;
	}
	else
	{
		if (Cmd_Argc() < 2)
		{
			Con_Printf("usage: rcon <command>\n");
			return;
		}
	}

	message[0] = 255;
	message[1] = 255;
	message[2] = 255;
	message[3] = 255;
	message[4] = 0;

	Q_strncatz (message, "rcon ", sizeof(message));
	Q_strncatz (message, password, sizeof(message));
	Q_strncatz (message, " ", sizeof(message));

	for ( ; i<Cmd_Argc() ; i++)
	{
		Q_strncatz (message, Cmd_Argv(i), sizeof(message));
		Q_strncatz (message, " ", sizeof(message));
	}

	if (cls.state >= ca_connected)
		to = cls.netchan.remote_address;
	else
	{
		if (!strlen(rcon_address.string))
		{
			Con_TPrintf ("You must either be connected,\nor set the 'rcon_address' cvar\nto issue rcon commands\n");

			return;
		}
		if (!NET_StringToAdr (rcon_address.string, PORT_QWSERVER, &to))
		{
			Con_Printf("Unable to resolve target address\n");
			return;
		}
	}

	NET_SendPacket (NS_CLIENT, strlen(message)+1, message, &to);
}

void CL_BlendFog(fogstate_t *result, fogstate_t *oldf, float time, fogstate_t *newf)
{
	float nfrac;
	if (time >= newf->time)
		nfrac = 1;
	else if (time < oldf->time)
		nfrac = 0;
	else
		nfrac = (time - oldf->time) / (newf->time - oldf->time);

	FloatInterpolate(oldf->alpha, nfrac, newf->alpha, result->alpha);
	FloatInterpolate(oldf->depthbias, nfrac, newf->depthbias, result->depthbias);
	FloatInterpolate(oldf->density, nfrac, newf->density, result->density);	//this should be non-linear, but that sort of maths is annoying.
	VectorInterpolate(oldf->colour, nfrac, newf->colour, result->colour);

	result->time = time;
}
void CL_ResetFog(int ftype)
{
	//blend from the current state, not the old state. this means things work properly if we've not reached the new state yet.
	CL_BlendFog(&cl.oldfog[ftype], &cl.oldfog[ftype], realtime, &cl.fog[ftype]);

	//reset the new state to defaults, to be filled in by the caller.
	memset(&cl.fog[ftype], 0, sizeof(cl.fog[ftype]));
	cl.fog[ftype].time = realtime;
	cl.fog[ftype].density = 0;
	cl.fog[ftype].colour[0] = 0.3;
	cl.fog[ftype].colour[1] = 0.3;
	cl.fog[ftype].colour[2] = 0.3;
	cl.fog[ftype].alpha = 1;
	cl.fog[ftype].depthbias = 0;
	/*
	cl.fog[ftype].end = 16384;
	cl.fog[ftype].height = 1<<30;
	cl.fog[ftype].fadedepth = 128;
	*/
}

/*
=====================
CL_ClearState

=====================
*/
void CL_ClearState (void)
{
	extern cvar_t cfg_save_auto;
	int			i, j;
#ifndef CLIENTONLY
#define serverrunning (sv.state != ss_dead)
#define tolocalserver NET_IsLoopBackAddress(&cls.netchan.remote_address)
#else
#define serverrunning false
#define tolocalserver false
#define SV_UnspawnServer()
#endif

	CL_UpdateWindowTitle();

	CL_AllowIndependantSendCmd(false);	//model stuff could be a problem.

	S_StopAllSounds (true);
	S_UntouchAll();
	S_ResetFailedLoad();

	Cvar_ApplyLatches(CVAR_SERVEROVERRIDE);

	Con_DPrintf ("Clearing memory\n");
	if (!serverrunning || !tolocalserver)
	{
#ifndef CLIENTONLY
		if (serverrunning && sv.state != ss_clustermode)
			SV_UnspawnServer();
#endif
		Mod_ClearAll ();
		r_regsequence++;

		Cvar_ApplyLatches(CVAR_LATCH);
	}

	CL_ClearParseState();
	CL_ClearTEnts();
	CL_ClearCustomTEnts();
	Surf_ClearLightmaps();
#ifdef HEXEN2
	T_FreeInfoStrings();
#endif
	SCR_ShowPic_ClearAll(false);

	if (cl.playerview[0].playernum == -1)
	{	//left over from q2 connect.
		Media_StopFilm(true);
	}

	for (i = 0; i < UPDATE_BACKUP; i++)
	{
		if (cl.inframes[i].packet_entities.entities)
		{
			Z_Free(cl.inframes[i].packet_entities.entities);
			cl.inframes[i].packet_entities.entities = NULL;
		}
	}

	if (cl.lerpents)
		BZ_Free(cl.lerpents);
	if (cl.particle_ssprecaches)
	{
		for (i = 0; i < MAX_SSPARTICLESPRE; i++)
			if (cl.particle_ssname[i])
				free(cl.particle_ssname[i]);
	}
	if (cl.particle_csprecaches)
	{
		for (i = 0; i < MAX_CSPARTICLESPRE; i++)
			if (cl.particle_csname[i])
				free(cl.particle_csname[i]);
	}
#ifdef Q2CLIENT
	for (i = 0; i < Q2MAX_IMAGES; i++)
		if (cl.image_name[i])
			BZ_Free(cl.image_name[i]);
	for (i = 0; i < Q2MAX_ITEMS; i++)
		if (cl.item_name[i])
			BZ_Free(cl.item_name[i]);
#endif

	while (cl.itemtimers)
	{
		struct itemtimer_s *t = cl.itemtimers;
		cl.itemtimers = t->next;
		Z_Free(t);
	}

	{
		downloadlist_t *next;
		while(cl.downloadlist)
		{
			next = cl.downloadlist->next;
			Z_Free(cl.downloadlist);
			cl.downloadlist = next;
		}
		while(cl.faileddownloads)
		{
			next = cl.faileddownloads->next;
			Z_Free(cl.faileddownloads);
			cl.faileddownloads = next;
		}
	}

#ifdef Q2CLIENT
	for (i = 0; i < countof(cl.configstring_general); i++)
	{
		if (cl.configstring_general)
			Z_Free(cl.configstring_general[i]);
	}
#endif

	for (i = 0; i < MAX_SPLITS; i++)
	{
		for (j = 0; j < MAX_CL_STATS; j++)
			if (cl.playerview[i].statsstr[j])
				Z_Free(cl.playerview[i].statsstr[j]);
	}

	Z_Free(cl.windowtitle);

// wipe the entire cl structure
	memset (&cl, 0, sizeof(cl));

	CL_ResetFog(0);
	CL_ResetFog(1);

	cl.allocated_client_slots = QWMAX_CLIENTS;
#ifndef CLIENTONLY
	//FIXME: we should just set it to 0 to make sure its set up properly elsewhere.
	if (sv.state)
		cl.allocated_client_slots = sv.allocated_client_slots;
#endif

	SZ_Clear (&cls.netchan.message);

	r_worldentity.model = NULL;

// clear other arrays
//	memset (cl_dlights, 0, sizeof(cl_dlights));
	memset (cl_lightstyle, 0, sizeof(cl_lightstyle));
	for (i = 0; i < MAX_LIGHTSTYLES; i++)
		R_UpdateLightStyle(i, NULL, 1, 1, 1);

	rtlights_first = rtlights_max = RTL_FIRST;

	for (i = 0; i < MAX_SPLITS; i++)
	{
		VectorSet(cl.playerview[i].gravitydir, 0, 0, -1);
		cl.playerview[i].viewheight = DEFAULT_VIEWHEIGHT;
		cl.playerview[i].maxspeed = 320;
		cl.playerview[i].entgravity = 1;

		cl.playerview[i].chatstate = atoi(Info_ValueForKey(cls.userinfo[i], "chat"));
	}
#ifdef QUAKESTATS
	for (i = 0; i < MAX_CLIENTS; i++)	//in case some server doesn't support it
		cl.players[i].stats[STAT_VIEWHEIGHT] = cl.players[i].statsf[STAT_VIEWHEIGHT] = DEFAULT_VIEWHEIGHT;
#endif
	cl.minpitch = -70;
	cl.maxpitch = 80;

	cl.oldgametime = 0;
	cl.gametime = 0;
	cl.gametimemark = 0;
	cl.splitclients = 1;
	cl.autotrack_hint = -1;
	cl.autotrack_killer = -1;

	if (cfg_save_auto.ival && Cvar_UnsavedArchive())
		Cmd_ExecuteString("cfg_save\n", RESTRICT_LOCAL);
}

/*
=====================
CL_Disconnect

Sends a disconnect message to the server
This is also called on Host_Error, so it shouldn't cause any errors
=====================
*/
void CL_Disconnect (void)
{
	qbyte	final[12];
	int i;

	connectinfo.trying = false;

	SCR_SetLoadingStage(0);

	Cvar_ApplyLatches(CVAR_SERVEROVERRIDE);

// stop sounds (especially looping!)
	S_StopAllSounds (true);
#ifdef VM_CG
	CG_Stop();
#endif
#ifdef CSQC_DAT
	CSQC_Shutdown();
#endif
	// if running a local server, shut it down
	if (cls.demoplayback != DPB_NONE)
		CL_StopPlayback ();
	else if (cls.state != ca_disconnected)
	{
		if (cls.demorecording)
			CL_Stop_f ();

		switch(cls.protocol)
		{
		case CP_NETQUAKE:
#ifdef NQPROT
			final[0] = clc_disconnect;
			final[1] = clc_stringcmd;
			strcpy (final+2, "drop");
			Netchan_Transmit (&cls.netchan, strlen(final)+1, final, 250000);
			Netchan_Transmit (&cls.netchan, strlen(final)+1, final, 250000);
			Netchan_Transmit (&cls.netchan, strlen(final)+1, final, 250000);
#endif
			break;
		case CP_PLUGIN:
			break;
		case CP_QUAKE2:
#ifdef Q2CLIENT
			final[0] = clcq2_stringcmd;
			strcpy (final+1, "disconnect");
			Netchan_Transmit (&cls.netchan, strlen(final)+1, final, 2500);
			Netchan_Transmit (&cls.netchan, strlen(final)+1, final, 2500);
			Netchan_Transmit (&cls.netchan, strlen(final)+1, final, 2500);
#endif
			break;
		case CP_QUAKE3:
			break;
		case CP_QUAKEWORLD:
			final[0] = clc_stringcmd;
			strcpy (final+1, "drop");
			Netchan_Transmit (&cls.netchan, strlen(final)+1, final, 2500);
			Netchan_Transmit (&cls.netchan, strlen(final)+1, final, 2500);
			Netchan_Transmit (&cls.netchan, strlen(final)+1, final, 2500);
			break;
		case CP_UNKNOWN:
			break;
		}

		cls.state = ca_disconnected;
		cls.protocol = CP_UNKNOWN;

		cls.demoplayback = DPB_NONE;
		cls.demorecording = cls.timedemo = false;

#ifndef CLIENTONLY
	//running a server, and it's our own
		if (serverrunning && !tolocalserver && sv.state != ss_clustermode)
			SV_UnspawnServer();
#endif
	}
	Cam_Reset();

	if (cl.worldmodel)
	{
		Mod_ClearAll();
		cl.worldmodel = NULL;
	}

	CL_Parse_Disconnected();

	COM_FlushTempoaryPacks();

	r_worldentity.model = NULL;
	for (i = 0; i < cl.splitclients; i++)
		cl.playerview[i].spectator = 0;
	cl.sendprespawn = false;
	cl.intermissionmode = IM_NONE;
	cl.oldgametime = 0;

	memset(&r_refdef, 0, sizeof(r_refdef));

#ifdef NQPROT
	cls.signon=0;
#endif
	CL_StopUpload();

	CL_FlushClientCommands();

#ifndef CLIENTONLY
	if (!isDedicated)
#endif
	{
		SCR_EndLoadingPlaque();
		V_ClearCShifts();
	}

	cl.servercount = 0;
	cls.findtrack = false;
	cls.realserverip.type = NA_INVALID;

	Validation_DelatchRulesets();

#ifdef TCPCONNECT
	//disconnects it, without disconnecting the others.
	FTENET_AddToCollection(cls.sockets, "conn", NULL, NA_INVALID, NP_DGRAM);
#endif

	Cvar_ForceSet(&cl_servername, "none");

	CL_ClearState();

	FS_PureMode(0, NULL, NULL, NULL, NULL, 0);

	Alias_WipeStuffedAliases();

	//now start up the csqc/menu module again.
//	(void)CSQC_UnconnectedInit();
}

#undef serverrunning
#undef tolocalserver

void CL_Disconnect_f (void)
{
#ifndef CLIENTONLY
	if (sv.state)
		SV_UnspawnServer();
#endif

	CL_Disconnect ();

	connectinfo.trying = false;

	NET_CloseClient();

	(void)CSQC_UnconnectedInit();
}

/*
====================
CL_User_f

user <name or userid>

Dump userdata / masterdata for a user
====================
*/
void CL_User_f (void)
{
	int		uid;
	int		i;
	qboolean found = false;

#ifndef CLIENTONLY
	if (sv.state)
	{
		SV_User_f();
		return;
	}
#endif

	if (Cmd_Argc() != 2)
	{
		Con_TPrintf ("Usage: user <username / userid>\n");
		return;
	}

	uid = atoi(Cmd_Argv(1));

	for (i=0 ; i<MAX_CLIENTS ; i++)
	{
		if (!cl.players[i].name[0])
			continue;
		if (cl.players[i].userid == uid
		|| !strcmp(cl.players[i].name, Cmd_Argv(1)) )
		{
			if (!cl.players[i].userinfovalid)
				Con_Printf("name: %s\ncolour %i %i\nping: %i\n", cl.players[i].name, cl.players[i].rbottomcolor, cl.players[i].rtopcolor, cl.players[i].ping);
			else
				Info_Print (cl.players[i].userinfo, "");
			found = true;
		}
	}
	if (!found)
		Con_TPrintf ("User not in server.\n");
}

/*
====================
CL_Users_f

Dump userids for all current players
====================
*/
void CL_Users_f (void)
{
	int		i;
	int		c;

	c = 0;
	Con_TPrintf ("userid frags name\n");
	Con_TPrintf ("------ ----- ----\n");
	for (i=0 ; i<MAX_CLIENTS ; i++)
	{
		if (cl.players[i].name[0])
		{
			Con_TPrintf ("%6i %4i ^[%s\\player\\%i^]\n", cl.players[i].userid, cl.players[i].frags, cl.players[i].name, i);
			c++;
		}
	}

	Con_TPrintf ("%i total users\n", c);
}

int CL_ParseColour(char *colt)
{
	int col;
	if (!strncmp(colt, "0x", 2))
		col = 0xff000000|strtoul(colt+2, NULL, 16);
	else
	{
		col = atoi(colt);
		col &= 15;
		if (col > 13)
			col = 13;
	}
	return col;
}

void CL_Color_f (void)
{
	// just for quake compatability...
	int		top, bottom;
	char	num[16];
	int  pnum = CL_TargettedSplit(true);

	qboolean server_owns_colour;

	char *t;
	char *b;

	if (Cmd_Argc() == 1)
	{
		char *t = Info_ValueForKey (cls.userinfo[pnum], "topcolor");
		char *b = Info_ValueForKey (cls.userinfo[pnum], "bottomcolor");
		if (!*t)
			t = "0";
		if (!*b)
			b = "0";
		Con_TPrintf ("\"color\" is \"%s %s\"\n", t, b);
		Con_TPrintf ("usage: color <0xRRGGBB> [0xRRGGBB]\n");
		return;
	}

	if (Cmd_FromGamecode())
		server_owns_colour = true;
	else
		server_owns_colour = false;


	t = Cmd_Argv(1);
	b = (Cmd_Argc()==2)?t:Cmd_Argv(2);
	if (!strcmp(t, "-1"))
		t = Info_ValueForKey (cls.userinfo[pnum], "topcolor");
	top = CL_ParseColour(t);
	if (!strcmp(b, "-1"))
		b = Info_ValueForKey (cls.userinfo[pnum], "bottomcolor");
	bottom = CL_ParseColour(b);

	Q_snprintfz (num, sizeof(num), (top&0xff000000)?"0x%06x":"%i", top & 0xffffff);
	if (top == 0)
		*num = '\0';
	if (Cmd_ExecLevel>RESTRICT_SERVER) //colour command came from server for a split client
		Cbuf_AddText(va("p%i cmd setinfo topcolor \"%s\"\n", Cmd_ExecLevel-RESTRICT_SERVER-1, num), Cmd_ExecLevel);
//	else if (server_owns_colour)
//		Cvar_LockFromServer(&topcolor, num);
	else
		CL_SetInfo(pnum, "topcolor", num);
	Q_snprintfz (num, sizeof(num), (bottom&0xff000000)?"0x%06x":"%i", bottom & 0xffffff);
	if (bottom == 0)
		*num = '\0';
	if (Cmd_ExecLevel>RESTRICT_SERVER) //colour command came from server for a split client
		Cbuf_AddText(va("p%i cmd setinfo bottomcolor \"%s\"\n", Cmd_ExecLevel-RESTRICT_SERVER-1, num), Cmd_ExecLevel);
	else if (server_owns_colour)
		Cvar_LockFromServer(&bottomcolor, num);
	else
		CL_SetInfo (pnum, "bottomcolor", num);
#ifdef NQPROT
	if (cls.protocol == CP_NETQUAKE)
		Cmd_ForwardToServer();
#endif
}

qboolean CL_CheckDLFile(const char *filename);
void CL_PakDownloads(int mode)
{
	/*
	mode=0 no downloads (forced to 1 for pure)
	mode=1 archived names so local stuff is not poluted
	mode=2 downloaded packages will always be present. Use With Caution.
	mode&4 download even packages that are not referenced.
	*/
	char local[256];
	char *pname;
	char *s = cl.serverpakcrcs;
	int i;

	if (!cl.serverpakschanged || !mode)
		return;

	Cmd_TokenizeString(cl.serverpaknames, false, false);
	for (i = 0; i < Cmd_Argc(); i++)
	{
		s = COM_Parse(s);
		pname = Cmd_Argv(i);

		//'*' prefix means 'referenced'. so if the server isn't using any files from it, don't bother downloading it.
		if (*pname == '*')
			pname++;
		else if (!(mode & 4))
			continue;

		if ((mode&3) != 2)
		{
			/*if we already have such a file, this is a no-op*/
			if (CL_CheckDLFile(va("package/%s", pname)))
				continue;
			if (!FS_GenCachedPakName(pname, com_token, local, sizeof(local)))
				continue;
		}
		else
			Q_strncpyz(local, pname, sizeof(local));
		CL_CheckOrEnqueDownloadFile(pname, local, DLLF_NONGAME);
	}
}

void CL_CheckServerPacks(void)
{
	static int oldpure;
	int pure = atof(Info_ValueForKey(cl.serverinfo, "sv_pure"));
	if (pure < cl_pure.ival)
		pure = cl_pure.ival;
	pure = bound(0, pure, 2);

	if (!*cl.serverpakcrcs || cls.demoplayback)
		pure = 0;

	if (pure != oldpure || cl.serverpakschanged)
	{
		CL_PakDownloads((pure && !cl_download_packages.ival)?1:cl_download_packages.ival);
		FS_PureMode(pure, cl.serverpaknames, cl.serverpakcrcs, NULL, NULL, cls.challenge);

		if (pure)
		{
			/*when enabling pure, kill cached models/sounds/etc*/
			Cache_Flush();
			/*make sure cheating lamas can't use old shaders from a different srver*/
			Shader_NeedReload(true);
		}
	}
	oldpure = pure;
	cl.serverpakschanged = false;
}

void CL_CheckServerInfo(void)
{
	char *s;
	unsigned int allowed;
#ifdef QUAKESTATS
	int oldstate;
#endif
#ifndef CLIENTONLY
	extern cvar_t sv_cheats;
#endif
	int oldteamplay;
	qboolean spectating = true;
	int i;
	
	//spectator 2 = spectator-with-scores, considered to be players. this means we don't want to allow spec cheats while they're inactive, because that would be weird.
	for (i = 0; i < cl.splitclients; i++)
		if (cl.playerview[i].spectator != 1)
			spectating = false;

	oldteamplay = cl.teamplay;
	cl.teamplay = atoi(Info_ValueForKey(cl.serverinfo, "teamplay"));
	cls.deathmatch = cl.deathmatch = atoi(Info_ValueForKey(cl.serverinfo, "deathmatch"));

	cls.allow_cheats = false;
	cls.allow_semicheats=true;
	cls.allow_watervis=false;
	cls.allow_skyboxes=false;
	cls.allow_fbskins = 1;
//	cls.allow_fbskins = 0;
//	cls.allow_overbrightlight;


	cls.allow_csqc = atoi(Info_ValueForKey(cl.serverinfo, "anycsqc")) || *Info_ValueForKey(cl.serverinfo, "*csprogs");
	cl.csqcdebug = atoi(Info_ValueForKey(cl.serverinfo, "*csqcdebug"));

	if (spectating || cls.demoplayback || atoi(Info_ValueForKey(cl.serverinfo, "watervis")))
		cls.allow_watervis=true;

	if (spectating || cls.demoplayback || atoi(Info_ValueForKey(cl.serverinfo, "allow_skybox")) || atoi(Info_ValueForKey(cl.serverinfo, "allow_skyboxes")))
		cls.allow_skyboxes=true;	//mostly obsolete.

	s = Info_ValueForKey(cl.serverinfo, "fbskins");
	if (*s)
		cls.allow_fbskins = atof(s);
	else if (cl.teamfortress)
		cls.allow_fbskins = 0;
	else
		cls.allow_fbskins = 1;

	s = Info_ValueForKey(cl.serverinfo, "*cheats");
	if (spectating || cls.demoplayback || !stricmp(s, "on"))
		cls.allow_cheats = true;

#ifndef CLIENTONLY
	//allow cheats in single player regardless of sv_cheats.
	//(also directly read the sv_cheats cvar to avoid issues with nq protocols that don't support serverinfo.
	if ((sv.state == ss_active && sv.allocated_client_slots == 1) || sv_cheats.ival)
		cls.allow_cheats = true;
#endif

	s = Info_ValueForKey(cl.serverinfo, "strict");
	if ((!spectating && !cls.demoplayback && *s && strcmp(s, "0")) || !ruleset_allow_semicheats.ival)
	{
		cls.allow_semicheats = false;
		cls.allow_cheats	= false;
	}

	cls.z_ext = atoi(Info_ValueForKey(cl.serverinfo, "*z_ext"));

#ifdef NQPROT
	if (cls.protocol == CP_NETQUAKE && CPNQ_IS_DP)
	{
		//movevars come from stats.
	}
	else
#endif
	{
		cls.maxfps = atof(Info_ValueForKey(cl.serverinfo, "maxfps"));
		if (cls.maxfps < 20)
			cls.maxfps = 72;

		// movement vars for prediction
		cl.bunnyspeedcap = Q_atof(Info_ValueForKey(cl.serverinfo, "pm_bunnyspeedcap"));
		movevars.slidefix = (Q_atof(Info_ValueForKey(cl.serverinfo, "pm_slidefix")) != 0);
		movevars.airstep = (Q_atof(Info_ValueForKey(cl.serverinfo, "pm_airstep")) != 0);
		movevars.walljump = (Q_atof(Info_ValueForKey(cl.serverinfo, "pm_walljump")));
		movevars.ktjump = Q_atof(Info_ValueForKey(cl.serverinfo, "pm_ktjump"));
		s = Info_ValueForKey(cl.serverinfo, "pm_stepheight");
		movevars.stepheight = *s?Q_atof(s):PM_DEFAULTSTEPHEIGHT;
		s = Info_ValueForKey(cl.serverinfo, "pm_watersinkspeed");
		movevars.watersinkspeed = *s?Q_atof(s):60;
		s = Info_ValueForKey(cl.serverinfo, "pm_flyfriction");
		movevars.flyfriction = *s?Q_atof(s):4;
	}

	// Initialize cl.maxpitch & cl.minpitch
	if (cls.protocol == CP_QUAKEWORLD || cls.protocol == CP_NETQUAKE)
	{
		s = (cls.z_ext & Z_EXT_PITCHLIMITS) ? Info_ValueForKey (cl.serverinfo, "maxpitch") : "";
		cl.maxpitch = *s ? Q_atof(s) : 80.0f;
		s = (cls.z_ext & Z_EXT_PITCHLIMITS) ? Info_ValueForKey (cl.serverinfo, "minpitch") : "";
		cl.minpitch = *s ? Q_atof(s) : -70.0f;

		if (cls.protocol == CP_NETQUAKE)
		{	//proquake likes spamming us with fixangles
			//should be about 0.5/65536, but there's some precision issues with such small numbers around 80, so we need to bias it more than we ought
			cl.maxpitch -= 1.0/2048;
		}
	}
	else
	{
		cl.maxpitch = 90;
		cl.minpitch = -90;
	}
	//bound it, such that we never end up looking slightly more back than forwards
	//FIXME: we should probably tweak our movement code instead.
	cl.maxpitch = bound(-89.9, cl.maxpitch, 89.9);
	cl.minpitch = bound(-89.9, cl.minpitch, 89.9);

	cl.hexen2pickups = atoi(Info_ValueForKey(cl.serverinfo, "sv_pupglow"));

	allowed = atoi(Info_ValueForKey(cl.serverinfo, "allow"));
	if (allowed & 1)
		cls.allow_watervis = true;
//	if (allowed & 2)
//		cls.allow_rearview = true;
	if (allowed & 4)
		cls.allow_skyboxes = true;
//	if (allowed & 8)
//		cls.allow_mirrors = true;
	//16
	//32
//	if (allowed & 128)
//		cls.allow_postproc = true;
//	if (allowed & 256)
//		cls.allow_lightmapgamma = true;
	if (allowed & 512)
		cls.allow_cheats = true;

	if (cls.allow_semicheats)
		cls.allow_anyparticles = true;
	else
		cls.allow_anyparticles = false;


	if (spectating || cls.demoplayback)
		cl.fpd = 0;
	else
		cl.fpd = atoi(Info_ValueForKey(cl.serverinfo, "fpd"));

	cl.gamespeed = atof(Info_ValueForKey(cl.serverinfo, "*gamespeed"))/100.f;
	if (cl.gamespeed < 0.1)
		cl.gamespeed = 1;

#ifdef QUAKESTATS
	s = Info_ValueForKey(cl.serverinfo, "status");
	oldstate = cl.matchstate;
	if (!stricmp(s, "standby"))
		cl.matchstate = MATCH_STANDBY;
	else if (!stricmp(s, "countdown"))
		cl.matchstate = MATCH_COUNTDOWN;
	else
	{
		float time = strtod(s, &s);
		if (!strcmp(s, " min left") || !strcmp(s, " mins left"))
			time *= 60;
		else if (!strcmp(s, " sec left") || !strcmp(s, " secs left"))
			time *= 1;
		else if (!strcmp(s, " hour left") || !strcmp(s, " hours left"))
			time *= 60*60;
		else
			time = -1;

		if (time >= 0)
		{
			//always update it. this is to try to cope with overtime.
			oldstate = cl.matchstate = MATCH_INPROGRESS;
			cl.matchgametimestart = cl.gametime + time - 60*atof(Info_ValueForKey(cl.serverinfo, "timelimit"));
		}
		else
		{
			if (*s && cl.matchstate == MATCH_INPROGRESS)
				Con_DPrintf("Match state changed to unknown meaning: %s\n", s);
			else
				cl.matchstate = MATCH_DONTKNOW;	//don't revert from inprogress to don't know
		}
	}
	if (oldstate != cl.matchstate)
		cl.matchgametimestart = cl.gametime;
#endif

	CL_CheckServerPacks();

	Cvar_ForceCheatVars(cls.allow_semicheats, cls.allow_cheats);
	Validation_Apply_Ruleset();

	if (oldteamplay != cl.teamplay)
		Skin_FlushPlayers();
}

/*
==================
CL_FullInfo_f

Allow clients to change userinfo
==================
*/
void CL_FullInfo_f (void)
{
	char	key[512];
	char	value[512];
	char	*o;
	char	*s;
	int pnum = CL_TargettedSplit(true);

	if (Cmd_Argc() != 2)
	{
		Con_TPrintf ("fullinfo <complete info string>\n");
		return;
	}

	s = Cmd_Argv(1);
	if (*s == '\\')
		s++;
	while (*s)
	{
		o = key;
		while (*s && *s != '\\' && o < key + sizeof(key))
			*o++ = *s++;
		if (o == key + sizeof(key))
		{
			Con_Printf ("key length too long\n");
			return;
		}
		*o = 0;

		if (!*s)
		{
			Con_Printf ("key %s has no value\n", key);
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\' && o < value + sizeof(value))
			*o++ = *s++;
		if (o == value + sizeof(value))
		{
			Con_Printf ("value length too long\n");
			return;
		}
		*o = 0;

		if (*s)
			s++;

		if (!stricmp(key, pmodel_name) || !stricmp(key, emodel_name))
			continue;

		Info_SetValueForKey (cls.userinfo[pnum], key, value, sizeof(cls.userinfo[pnum]));
	}
}

void CL_SetInfo (int pnum, char *key, char *value)
{
	cvar_t *var;
	if (!pnum)
	{
		var = Cvar_FindVar(key);
		if (var && (var->flags & CVAR_USERINFO))
		{	//get the cvar code to set it. the server might have locked it.
			Cvar_Set(var, value);
			return;
		}
	}

	Info_SetValueForStarKey (cls.userinfo[pnum], key, value, sizeof(cls.userinfo[pnum]));
	if (cls.state >= ca_connected && !cls.demoplayback)
	{
		if (pnum >= cl.splitclients)
			return;

#ifdef Q2CLIENT
		if ((cls.protocol == CP_QUAKE2 || cls.protocol == CP_QUAKE3) && !cls.fteprotocolextensions)
			cls.resendinfo = true;
		else
#endif
			if (cls.protocol != CP_NETQUAKE || (cls.fteprotocolextensions & PEXT_BIGUSERINFOS))
		{
			if (pnum)
				CL_SendClientCommand(true, "%i setinfo %s \"%s\"", pnum+1, key, value);
			else
				CL_SendClientCommand(true, "setinfo %s \"%s\"", key, value);
		}
	}
}
/*
==================
CL_SetInfo_f

Allow clients to change userinfo
==================
*/
void CL_SetInfo_f (void)
{
	cvar_t *var;
	int pnum = CL_TargettedSplit(true);
	if (Cmd_Argc() == 1)
	{
		Info_Print (cls.userinfo[pnum], "");
		return;
	}
	if (Cmd_Argc() != 3)
	{
		Con_TPrintf ("usage: setinfo [ <key> <value> ]\n");
		return;
	}
	if (!stricmp(Cmd_Argv(1), pmodel_name) || !strcmp(Cmd_Argv(1), emodel_name))
		return;

	if (Cmd_Argv(1)[0] == '*')
	{
		int i;
		if (!strcmp(Cmd_Argv(1), "*"))
			if (!strcmp(Cmd_Argv(2), ""))
			{	//clear it out
				char *k;
				for(i=0;;)
				{
					k = Info_KeyForNumber(cls.userinfo[pnum], i);
					if (!*k)
						break;	//no more.
					else if (*k == '*')
						i++;	//can't remove * keys
					else if ((var = Cvar_FindVar(k)) && var->flags&CVAR_USERINFO)
						i++;	//this one is a cvar.
					else
						Info_RemoveKey(cls.userinfo[pnum], k);	//we can remove this one though, so yay.
				}

				return;
			}
		Con_Printf ("Can't set * keys\n");
		return;
	}


	CL_SetInfo(pnum, Cmd_Argv(1), Cmd_Argv(2));
}

void CL_SaveInfo(vfsfile_t *f)
{
	int i;
	for (i = 0; i < MAX_SPLITS; i++)
	{
		VFS_WRITE(f, "\n", 1);
		if (i)
		{
			VFS_WRITE(f, va("p%i setinfo * \"\"\n", i+1), 16);
			Info_WriteToFile(f, cls.userinfo[i],  va("p%i setinfo", i+1), 0);
		}
		else
		{
			VFS_WRITE(f, "setinfo * \"\"\n", 13);
			Info_WriteToFile(f, cls.userinfo[i], "setinfo", CVAR_USERINFO);
		}
	}
}

/*
====================
CL_Packet_f

packet <destination> <contents>

Contents allows \n escape character
====================
*/
void CL_Packet_f (void)
{
	char	send[2048];
	int		i, l;
	char	*in, *out;
	netadr_t	adr;

	if (Cmd_Argc() != 3)
	{
		Con_TPrintf ("usage: packet <destination> <contents>\n");
		return;
	}

	if (!NET_StringToAdr (Cmd_Argv(1), PORT_QWSERVER, &adr))
	{
		Con_Printf ("Bad address: %s\n", Cmd_Argv(1));
		return;
	}


	if (Cmd_FromGamecode())	//some mvd servers stuffcmd a packet command which lets them know which ip the client is from.
	{						//unfortunatly, 50% of servers are badly configured.
		if (cls.demoplayback)
		{
			Con_DPrintf ("Not sending realip packet from demo\n");
			return;
		}

		if (adr.type == NA_IP)
			if ((adr.address.ip[0] == 127 && adr.address.ip[1] == 0 && adr.address.ip[2] == 0 && adr.address.ip[3] == 1) ||
				(adr.address.ip[0] == 0   && adr.address.ip[1] == 0 && adr.address.ip[2] == 0 && adr.address.ip[3] == 0))
			{
				adr.address.ip[0] = cls.netchan.remote_address.address.ip[0];
				adr.address.ip[1] = cls.netchan.remote_address.address.ip[1];
				adr.address.ip[2] = cls.netchan.remote_address.address.ip[2];
				adr.address.ip[3] = cls.netchan.remote_address.address.ip[3];
				adr.port = cls.netchan.remote_address.port;
				Con_Printf (CON_WARNING "Server is broken. Trying to send to server instead.\n");
			}

		cls.realserverip = adr;
		Con_DPrintf ("Sending realip packet\n");
	}
	else if (!ruleset_allow_packet.ival)
	{
		Con_Printf("Sorry, the %s command is disallowed\n", Cmd_Argv(0));
		return;
	}
	cls.lastarbiatarypackettime = Sys_DoubleTime();	//prevent the packet command from causing a reconnect on badly configured mvdsv servers.

	in = Cmd_Argv(2);
	out = send+4;
	send[0] = send[1] = send[2] = send[3] = 0xff;

	l = strlen (in);
	for (i=0 ; i<l ; i++)
	{
		if (in[i] == '\\' && in[i+1] == 'n')
		{
			*out++ = '\n';
			i++;
		}
		else if (in[i] == '\\' && in[i+1] == '\\')
		{
			*out++ = '\\';
			i++;
		}
		else if (in[i] == '\\' && in[i+1] == 'r')
		{
			*out++ = '\r';
			i++;
		}
		else if (in[i] == '\\' && in[i+1] == '\"')
		{
			*out++ = '\"';
			i++;
		}
		else if (in[i] == '\\' && in[i+1] == '0')
		{
			*out++ = '\0';
			i++;
		}
		else
			*out++ = in[i];
	}
	*out = 0;

	NET_SendPacket (NS_CLIENT, out-send, send, &adr);

	if (Cmd_FromGamecode())
	{
		//realip
		char *temp = Z_Malloc(strlen(in)+1);
		strcpy(temp, in);
		Cmd_TokenizeString(temp, false, false);
		cls.realip_ident = atoi(Cmd_Argv(2));
		Z_Free(temp);
	}
}


/*
=====================
CL_NextDemo

Called to play the next demo in the demo loop
=====================
*/
void CL_NextDemo (void)
{
	char	str[1024];

	if (cls.demonum < 0)
		return;		// don't play demos

	if (!cls.demos[cls.demonum][0] || cls.demonum >= MAX_DEMOS)
	{
		cls.demonum = 0;
		if (!cls.demos[cls.demonum][0])
		{
//			Con_Printf ("No demos listed with startdemos\n");
			cls.demonum = -1;
			return;
		}
	}

	if (!strcmp(cls.demos[cls.demonum], "quit"))
		Q_snprintfz (str, sizeof(str), "quit\n");
	else
		Q_snprintfz (str, sizeof(str), "playdemo %s\n", cls.demos[cls.demonum]);
	Cbuf_InsertText (str, RESTRICT_LOCAL, false);
	cls.demonum++;

	if (!cls.state)
		cls.state = ca_demostart;
}

/*
===============================================================================

DEMO LOOP CONTROL

===============================================================================
*/


/*
==================
CL_Startdemos_f
==================
*/
void CL_Startdemos_f (void)
{
	int		i, c;

	c = Cmd_Argc() - 1;
	if (c > MAX_DEMOS)
	{
		Con_Printf ("Max %i demos in demoloop\n", MAX_DEMOS);
		c = MAX_DEMOS;
	}
	Con_DPrintf ("%i demo(s) in loop\n", c);

	for (i=1 ; i<c+1 ; i++)
		Q_strncpyz (cls.demos[i-1], Cmd_Argv(i), sizeof(cls.demos[0]));

	cls.demonum = -1;
	//don't start it here - we might have been given a +connect or whatever argument.
}


/*
==================
CL_Demos_f

Return to looping demos
==================
*/
void CL_Demos_f (void)
{
	if (cls.demonum == -1)
		cls.demonum = 1;
	CL_Disconnect_f ();
	CL_NextDemo ();
}

/*
==================
CL_Stopdemo_f

stop demo
==================
*/
void CL_Stopdemo_f (void)
{
	if (cls.demoplayback == DPB_NONE)
		return;
	CL_StopPlayback ();
	CL_Disconnect ();
}



/*
=================
CL_Changing_f

Just sent as a hint to the client that they should
drop to full console
=================
*/
void CL_Changing_f (void)
{
	char *mapname = Cmd_Argv(1);
	if (cls.download && cls.download->method <= DL_QWPENDING)  // don't change when downloading
		return;

	cls.demoseeking = false;	//don't seek over it

	if (*mapname)
		SCR_ImageName(mapname);
	else
		SCR_BeginLoadingPlaque();

	S_StopAllSounds (true);
	cl.intermissionmode = IM_NONE;
	if (cls.state)
	{
		cls.state = ca_connected;	// not active anymore, but not disconnected
		Con_TPrintf ("\nChanging map...\n");
	}
	else
		Con_Printf("Changing while not connected\n");

#ifdef NQPROT
	cls.signon=0;
#endif
}


/*
=================
CL_Reconnect_f

The server is changing levels
=================
*/
void CL_Reconnect_f (void)
{
	if (cls.download && cls.download->method <= DL_QWPENDING)  // don't change when downloading
		return;
#ifdef NQPROT
	if (cls.protocol == CP_NETQUAKE && Cmd_IsInsecure())
	{
		CL_Changing_f();
		return;
	}
#endif
	S_StopAllSounds (true);

	if (cls.state == ca_connected)
	{
		Con_TPrintf ("reconnecting...\n");
		CL_SendClientCommand(true, "new");
		return;
	}

	if (!*cls.servername)
	{
		Con_TPrintf ("No server to reconnect to...\n");
		return;
	}

	CL_Disconnect();
	CL_BeginServerReconnect();
}

/*
=================
CL_ConnectionlessPacket

Responses to broadcasts, etc
=================
*/
void CL_ConnectionlessPacket (void)
{
	char	*s;
	int		c;
	char	adr[MAX_ADR_SIZE];

	MSG_BeginReading (msg_nullnetprim);
	MSG_ReadLong ();        // skip the -1

	Cmd_TokenizeString(net_message.data+4, false, false);

	if (net_message.cursize == sizeof(net_message_buffer))
		net_message.data[sizeof(net_message_buffer)-1] = '\0';
	else
		net_message.data[net_message.cursize] = '\0';

#ifdef PLUGINS
	if (Plug_ConnectionlessClientPacket(net_message.data+4, net_message.cursize-4))
		return;
#endif

	c = MSG_ReadByte ();

	// ping from somewhere
	if (c == A2A_PING)
	{
		char	data[256];
		int len;

		if (cls.realserverip.type == NA_INVALID)
			return;	//not done a realip yet

		if (NET_CompareBaseAdr(&cls.realserverip, &net_from) == false)
			return;	//only reply if it came from the real server's ip.

		data[0] = 0xff;
		data[1] = 0xff;
		data[2] = 0xff;
		data[3] = 0xff;
		data[4] = A2A_ACK;
		data[5] = 0;

		//ack needs two parameters to work with realip properly.
		//firstly it needs an auth message, so it can't be spoofed.
		//secondly, it needs a copy of the realip ident, so you can't report a different player's client (you would need access to their ip).
		data[5] = ' ';
		Q_snprintfz(data+6, sizeof(data)-6, "%i %i", atoi(MSG_ReadString()), cls.realip_ident);
		len = strlen(data);

		NET_SendPacket (NS_CLIENT, len, &data, &net_from);
		return;
	}

	if (c == A2C_PRINT)
	{
		if (!strncmp(net_message.data+msg_readcount, "\\chunk", 6))
		{
			if (NET_CompareBaseAdr(&cls.netchan.remote_address, &net_from) == false)
				if (cls.realserverip.type == NA_INVALID || NET_CompareBaseAdr(&cls.realserverip, &net_from) == false)
					return;	//only use it if it came from the real server's ip (this breaks on proxies).

			MSG_ReadLong();
			MSG_ReadChar();
			MSG_ReadChar();

			if (CL_ParseOOBDownload())
			{
				if (msg_readcount != net_message.cursize)
				{
					Con_Printf ("junk on the end of the packet\n");
					CL_Disconnect_f();
				}
				cls.netchan.last_received = realtime;	//in case there's some virus scanner running on the server making it stall... for instance...
			}
			return;
		}
	}

	if (cls.demoplayback == DPB_NONE && net_from.type != NA_LOOPBACK)
		Con_Printf ("%s: ", NET_AdrToString (adr, sizeof(adr), &net_from));
//	Con_DPrintf ("%s", net_message.data + 4);

	if (c == 'f')	//using 'f' as a prefix so that I don't need lots of hacks
	{
		s = MSG_ReadStringLine ();
		if (!strcmp(s, "redir"))
		{
			netadr_t adr;
			char *data = MSG_ReadStringLine();
			Con_TPrintf ("redirect to %s\n", data);
			if (NET_StringToAdr(data, PORT_QWSERVER, &adr))
			{
				data = "\xff\xff\xff\xffgetchallenge\n";

				if (NET_CompareAdr(&connectinfo.adr, &net_from))
				{
					connectinfo.istransfer = true;
					connectinfo.adr = adr;
					NET_SendPacket (NS_CLIENT, strlen(data), data, &adr);
				}
			}
			return;
		}
		else if (!strcmp(s, "reject"))
		{	//generic rejection. stop trying.
			char *data = MSG_ReadStringLine();
			Con_Printf ("reject\n%s\n", data);
			if (NET_CompareAdr(&connectinfo.adr, &net_from))
				connectinfo.trying = false;
			return;
		}
		else if (!strcmp(s, "badname"))
		{	//rejected purely because of player name
			if (NET_CompareAdr(&connectinfo.adr, &net_from))
				connectinfo.trying = false;
		}
		else if (!strcmp(s, "badaccount"))
		{	//rejected because username or password is wrong
			if (NET_CompareAdr(&connectinfo.adr, &net_from))
				connectinfo.trying = false;
		}
		
		Con_Printf ("f%s\n", s);
		return;
	}

	if (c == S2C_CHALLENGE)
	{
		static unsigned int lasttime = 0xdeadbeef;
		static netadr_t lastadr;
		unsigned int curtime = Sys_Milliseconds();
		unsigned long pext = 0, pext2 = 0, huffcrc=0, mtu=0;
#ifdef HAVE_DTLS
		qboolean candtls = false;
#endif

		s = MSG_ReadString ();
		COM_Parse(s);

#ifdef Q3CLIENT
		if (!strcmp(com_token, "onnectResponse"))
		{
			connectinfo.protocol = CP_QUAKE3;
			goto client_connect;
		}
#endif
#ifdef Q2CLIENT
		if (!strcmp(com_token, "lient_connect"))
		{
			connectinfo.protocol = CP_QUAKE2;
			goto client_connect;
		}
#endif

		Con_TPrintf ("challenge\n");

		if (!strcmp(com_token, "hallengeResponse"))
		{
			/*Quake3*/
#ifdef Q3CLIENT
			if (connectinfo.protocol == CP_QUAKE3 || connectinfo.protocol == CP_UNKNOWN)
			{
				/*throttle*/
				if (curtime - lasttime < 500)
					return;
				lasttime = curtime;

				connectinfo.protocol = CP_QUAKE3;
				connectinfo.challenge = atoi(s+17);
				CL_SendConnectPacket (&net_from, 0, 0, 0, 0/*, ...*/);
			}
			else
			{
				Con_Printf("\nChallenge from another protocol, ignoring Q3 challenge\n");
				return;
			}
			return;
#else
			Con_Printf("\nUnable to connect to Quake3\n");
			return;
#endif
		}
		else if (!strcmp(com_token, "hallenge"))
		{
			/*Quake2 or Darkplaces*/
			char *s2;

			for (s2 = s+9; *s2; s2++)
			{
				if ((*s2 < '0' || *s2 > '9') && *s2 != '-')
					break;
			}
			if (!strncmp(s2, "FTE", 3) || !strncmp(s2, "QW", 2))
			{	//hack to work around NQ+QW+DP servers that reply with both qw and dp challenge requests.
				//we DON'T want to treat it as a dp server. because then we end up with nq-based protocols.
				return;
			}
			else if (*s2 && *s2 != ' ')
			{//and if it's not, we're unlikly to be compatible with whatever it is that's talking at us.
#ifdef NQPROT
				if (connectinfo.protocol == CP_NETQUAKE || connectinfo.protocol == CP_UNKNOWN)
				{
					/*throttle*/
					if (curtime - lasttime < 500)
						return;
					lasttime = curtime;

					connectinfo.protocol = CP_NETQUAKE;
					CL_ConnectToDarkPlaces(s+9, &net_from);
				}
				else
					Con_Printf("\nChallenge from another protocol, ignoring DP challenge\n");
#else
				Con_Printf("\nUnable connect to DarkPlaces\n");
#endif
				return;
			}

#ifdef Q2CLIENT
			if (connectinfo.protocol == CP_QUAKE2 || connectinfo.protocol == CP_UNKNOWN)
			{
				connectinfo.protocol = CP_QUAKE2;
				connectinfo.subprotocol = PROTOCOL_VERSION_Q2;
			}
			else
			{
				Con_Printf("\nChallenge from another protocol, ignoring Q2 challenge\n");
				return;
			}
#else
			Con_Printf("\nUnable to connect to Quake2\n");
			return;
#endif
			s+=9;
		}

		/*no idea, assume a QuakeWorld challenge response ('c' packet)*/

		else if (connectinfo.protocol == CP_QUAKEWORLD || connectinfo.protocol == CP_UNKNOWN)
		{
			connectinfo.protocol = CP_QUAKEWORLD;
			connectinfo.subprotocol = PROTOCOL_VERSION_QW;
		}
		else
		{
			Con_Printf("\nChallenge from another protocol, ignoring QW challenge\n");
			return;
		}

		/*throttle connect requests*/
		if (curtime - lasttime < 500 && NET_CompareAdr(&net_from, &lastadr))
			return;
		lasttime = curtime;
		lastadr = net_from;

		s = COM_Parse(s);
		connectinfo.challenge = atoi(com_token);

		while((s = COM_Parse(s)))
		{
			if (connectinfo.protocol == CP_QUAKE2 && !strncmp(com_token, "p=", 2))
			{
				char *p = com_token+2;
				do
				{
					switch(strtoul(p, &p, 0))
					{
					case PROTOCOL_VERSION_R1Q2:
#ifdef AVAIL_ZLIB		//r1q2 will typically send us compressed data, which is a problem if we can't handle that (q2pro has a way to disable it).
						if (connectinfo.subprotocol < PROTOCOL_VERSION_R1Q2)
							connectinfo.subprotocol = PROTOCOL_VERSION_R1Q2;
#endif
						break;
					case PROTOCOL_VERSION_Q2PRO:
						if (connectinfo.subprotocol < PROTOCOL_VERSION_Q2PRO)
							connectinfo.subprotocol = PROTOCOL_VERSION_Q2PRO;
						break;
					}
				} while (*p++ == ',');
			}
		}

		for(;;)
		{
			int cmd = MSG_ReadLong ();
			if (msg_badread)
				break;
			if (cmd == PROTOCOL_VERSION_VARLENGTH)
			{
				int len = MSG_ReadLong();
				if (len < 0 || len > 8192)
					break;
				c = MSG_ReadLong();/*ident*/
				switch(c)
				{
				default:
					MSG_ReadSkip(len); /*payload*/
					break;
				}
			}
			else
			{
				unsigned int l = MSG_ReadLong();
				switch(cmd)
				{
				case PROTOCOL_VERSION_FTE:			pext = l;		break;
				case PROTOCOL_VERSION_FTE2:			pext2 = l;		break;
				case PROTOCOL_VERSION_FRAGMENT:		mtu = l;		break;
#ifdef HAVE_DTLS
				case PROTOCOL_VERSION_DTLSUPGRADE:	candtls = l;	break;	//0:not enabled. 1:use if you want. 2:require it.
#endif
#ifdef HUFFNETWORK
				case PROTOCOL_VERSION_HUFFMAN:		huffcrc = l;	break;
#endif
				default:
					break;
				}
			}
		}

#ifdef HAVE_DTLS
		if (candtls && connectinfo.adr.prot == NP_DGRAM && (connectinfo.dtlsupgrade || candtls > 1))
		{
			//c2s getchallenge
			//s2c c%u\0DTLS=0
			//c2s dtlsconnect %u
			//s2c dtlsopened
			//c2s DTLS(getchallenge)
			//DTLS(etc)

			//server says it can do tls.
			char *pkt = va("%c%c%c%cdtlsconnect %i", 255, 255, 255, 255, connectinfo.challenge);
			NET_SendPacket (NS_CLIENT, strlen(pkt), pkt, &net_from);
			return;
		}
		if (connectinfo.dtlsupgrade == DTLS_REQUIRE)
		{
			connectinfo.trying = false;
			Con_Printf("Server does not support/allow dtls. not connecting.\n");
			return;
		}
#endif

		CL_SendConnectPacket (&net_from, mtu, pext, pext2, huffcrc/*, ...*/);
		return;
	}
#ifdef Q2CLIENT
	if (connectinfo.protocol == CP_QUAKE2)
	{
		char *nl;
		msg_readcount--;
		c = msg_readcount;
		s = MSG_ReadString ();
		nl = strchr(s, '\n');
		if (nl)
		{
			msg_readcount = c + nl-s + 1;
			msg_badread = false;
			*nl = '\0';
		}

		if (!strcmp(s, "print"))
		{
			Con_TPrintf ("print\n");

			s = MSG_ReadString ();
			Con_Printf ("%s", s);
			return;
		}
		else if (!strcmp(s, "client_connect"))
		{
			connectinfo.protocol = CP_QUAKE2;
			goto client_connect;
		}
		else if (!strcmp(s, "disconnect"))
		{
			if (NET_CompareAdr(&net_from, &cls.netchan.remote_address))
			{
				Con_Printf ("disconnect\n");
				CL_Disconnect_f();
				return;
			}
			else
			{
				Con_Printf("Ignoring random disconnect command\n");
				return;
			}
		}
		else
		{
			Con_TPrintf ("unknown connectionless packet for q2:  %s\n", s);
			msg_readcount = c;
			c = MSG_ReadByte();
		}
	}
#endif

#ifdef NQPROT
	if (c == 'a')
	{
		s = MSG_ReadString ();
		COM_Parse(s);
		if (!strcmp(com_token, "ccept"))
		{
			/*this is a DP server... but we don't know which version nor nq protocol*/
			Con_Printf ("accept\n");
			if (cls.state == ca_connected)
				return;	//we're already connected. don't do it again!

			if (!NET_CompareAdr(&connectinfo.adr, &net_from))
			{
				//if (net_from.type != NA_LOOPBACK)
				Con_TPrintf ("ignoring connection\n");
				return;
			}

			Validation_Apply_Ruleset();
			Netchan_Setup(NS_CLIENT, &cls.netchan, &net_from, connectinfo.qport);
			CL_ParseEstablished();
			Con_DPrintf ("CL_EstablishConnection: connected to %s\n", cls.servername);

			cls.netchan.isnqprotocol = true;
			cls.protocol = CP_NETQUAKE;
			cls.protocol_nq = CPNQ_ID;	//assume vanilla protocol until we know better.
			cls.proquake_angles_hack = false;
			cls.challenge = connectinfo.challenge;
			connectinfo.trying = false;

			cls.demonum = -1;			// not in the demo loop now
			cls.state = ca_connected;


			SCR_BeginLoadingPlaque();
			return;
		}
	}

	if (c == 'i')
	{
		if (!strncmp(net_message.data+4, "infoResponse\n", 13))
		{
			Con_TPrintf ("infoResponse\n");
			Info_Print(net_message.data+17, "");
			return;
		}
	}
	if (c == 'g')
	{
		if (!strncmp(net_message.data+4, "getserversResponse", 18))
		{
			qbyte *b = net_message.data+4+18;
			Con_TPrintf ("getserversResponse\n");
			while (b+7 <= net_message.data+net_message.cursize)
			{
				if (*b == '\\')
				{
					b+=1;
					Con_Printf("%u.%u.%u.%u:%u\n", b[0], b[1], b[2], b[3], b[5]|(b[4]<<8));
					b+=6;
				}
				else if (*b == '/')
				{
					b+=1;
					Con_Printf("[%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x]:%u\n", (b[0]<<8)|b[1], (b[2]<<8)|b[3], (b[4]<<8)|b[5], (b[6]<<8)|b[7], (b[8]<<8)|b[9], (b[10]<<8)|b[11], (b[12]<<8)|b[13], (b[14]<<8)|b[15], b[17]|(b[16]<<8));
					b+=18;
				}
			}
			return;
		}
	}
#endif

	if (c == 'd'/*M2C_MASTER_REPLY*/)
	{
		s = MSG_ReadString ();
		COM_Parse(s);
		if (!strcmp(com_token, "tlsopened"))
		{	//server is letting us know that its now listening for a dtls handshake.
#ifdef HAVE_DTLS
			Con_Printf ("dtlsopened\n");
			if (!NET_CompareAdr(&connectinfo.adr, &net_from))
				return;

			if (NET_DTLS_Create(cls.sockets, &net_from))
			{
				connectinfo.dtlsupgrade = DTLS_ACTIVE;
				connectinfo.adr.prot = NP_DTLS;
			}
			else
			{
				if (connectinfo.dtlsupgrade == DTLS_TRY)
					connectinfo.dtlsupgrade = DTLS_DISABLE;
				Con_Printf ("unable to establish dtls route\n");
			}
#else
			Con_Printf ("dtlsopened (unsupported)\n");
#endif
		}
		else if (*s != '\n')
		{	//qw master server list response
			Con_Printf ("server ip list\n");
		}
		else
		{
			Con_Printf ("d\n");
			if (cls.demoplayback != DPB_NONE)
			{
				Con_Printf("Disconnect\n");
				CL_Disconnect_f();
			}
		}
		return;
	}

	if (c == S2C_CONNECTION)
	{
		connectinfo.protocol = CP_QUAKEWORLD;
		connectinfo.subprotocol = PROTOCOL_VERSION_QW;
#if defined(Q2CLIENT) || defined(Q3CLIENT)
client_connect:	//fixme: make function
#endif
		if (net_from.type == NA_INVALID)
			return;	//I've found a qizmo demo that contains one of these. its best left ignored.

		if (!NET_CompareAdr(&connectinfo.adr, &net_from))
		{
			if (net_from.type != NA_LOOPBACK)
				Con_TPrintf ("ignoring connection\n");
			return;
		}
		if (net_from.type != NA_LOOPBACK)
		{
			Con_TPrintf ("connection\n");

#ifndef CLIENTONLY
			if (sv.state)
				SV_UnspawnServer();
#endif
		}

		if (cls.state >= ca_connected)
		{
			if (!NET_CompareAdr(&cls.netchan.remote_address, &net_from))
			{
#ifndef CLIENTONLY
				if (sv.state != ss_clustermode)
#endif
					CL_Disconnect ();
			}
			else
			{
				if (cls.demoplayback == DPB_NONE)
					Con_TPrintf ("Dup connect received.  Ignored.\n");
				return;
			}
		}
		connectinfo.trying = false;
		cl.splitclients = 0;
		cls.protocol = connectinfo.protocol;
		cls.proquake_angles_hack = false;
		cls.fteprotocolextensions = connectinfo.fteext1;
		cls.fteprotocolextensions2 = connectinfo.fteext2;
		cls.challenge = connectinfo.challenge;
		Netchan_Setup (NS_CLIENT, &cls.netchan, &net_from, connectinfo.qport);
		if (cls.protocol == CP_QUAKE2)
		{
			cls.protocol_q2 = connectinfo.subprotocol;
			if (cls.protocol_q2 == PROTOCOL_VERSION_R1Q2 || cls.protocol_q2 == PROTOCOL_VERSION_Q2PRO)
				cls.netchan.qportsize = 1;
		}
		cls.netchan.fragmentsize = connectinfo.mtu;
		if (connectinfo.mtu >= 64)
			cls.netchan.message.maxsize = sizeof(cls.netchan.message_buf);
#ifdef HUFFNETWORK
		cls.netchan.compresstable = Huff_CompressionCRC(connectinfo.compresscrc);
#else
		cls.netchan.compresstable = NULL;
#endif
		CL_ParseEstablished();
#ifdef Q3CLIENT
		if (cls.protocol != CP_QUAKE3)
#endif
			CL_SendClientCommand(true, "new");
		cls.state = ca_connected;
		if (cls.netchan.remote_address.type != NA_LOOPBACK)
		{
			if (cls.netchan.remote_address.prot == NP_DTLS || cls.netchan.remote_address.prot == NP_TLS || cls.netchan.remote_address.prot == NP_WSS)
				Con_TPrintf ("Connected (encrypted).\n");
			else
				Con_TPrintf ("Connected (plain-text).\n");
		}
#ifdef QUAKESPYAPI
		allowremotecmd = false; // localid required now for remote cmds
#endif

		total_loading_size = 100;
		current_loading_size = 0;
		SCR_SetLoadingStage(LS_CLIENT);

		Validation_Apply_Ruleset();

		CL_WriteSetDemoMessage();

		return;
	}
#ifdef QUAKESPYAPI
	// remote command from gui front end
	if (c == A2C_CLIENT_COMMAND)	//man I hate this.
	{
		char	cmdtext[2048];

		if (net_from.type == NA_INVALID || net_from.type != net_local_cl_ipadr.type || net_from.type != NA_IP
			|| ((*(unsigned *)net_from.address.ip != *(unsigned *)net_local_cl_ipadr.address.ip) && (*(unsigned *)net_from.address.ip != htonl(INADDR_LOOPBACK))))
		{
			Con_TPrintf ("Command packet from remote host.  Ignored.\n");
			return;
		}
#if defined(_WIN32) && !defined(WINRT)
		ShowWindow (mainwindow, SW_RESTORE);
		SetForegroundWindow (mainwindow);
#endif
		s = MSG_ReadString ();

		Con_TPrintf ("client command: %s\n", s);

		Q_strncpyz(cmdtext, s, sizeof(cmdtext));

		s = MSG_ReadString ();

		while (*s && isspace(*s))
			s++;
		while (*s && isspace(s[strlen(s) - 1]))
			s[strlen(s) - 1] = 0;

		if (!allowremotecmd && (!*localid.string || strcmp(localid.string, s)))
		{
			if (!*localid.string)
			{
				Con_TPrintf ("^&C0Command packet received from local host, but no localid has been set.  You may need to upgrade your server browser.\n");
				return;
			}
			Con_TPrintf ("^&C0Invalid localid on command packet received from local host. \n|%s| != |%s|\nYou may need to reload your server browser and game.\n",
				s, localid.string);
			Cvar_Set(&localid, "");
			return;
		}

		Cbuf_AddText (cmdtext, RESTRICT_SERVER);
		allowremotecmd = false;
		return;
	}
#endif
	// print command from somewhere
	if (c == 'p')
	{
		if (!strncmp(net_message.data+4, "print\n", 6))
		{
			Con_TPrintf ("print\n");
			Con_Printf ("%s", net_message.data+10);
			return;
		}
	}
	if (c == A2C_PRINT)
	{
		Con_TPrintf ("print\n");

		s = MSG_ReadString ();
		Con_Printf ("%s", s);
		return;
	}
	if (c == 'r')//dp's reject
	{
		s = MSG_ReadString ();
		Con_Printf("r%s\n", s);
		return;
	}

//happens in demos
	if (c == svc_disconnect && cls.demoplayback != DPB_NONE && net_from.type == NA_INVALID)
	{
		Host_EndGame ("End of Demo");
		return;
	}

	Con_TPrintf ("unknown connectionless packet:  %c\n", c);
}

#ifdef NQPROT
void CLNQ_ConnectionlessPacket(void)
{
	char *s;
	int length;
	unsigned short port;

	MSG_BeginReading (msg_nullnetprim);
	length = LongSwap(MSG_ReadLong ());
	if (!(length & NETFLAG_CTL))
		return;	//not an nq control packet.
	length &= NETFLAG_LENGTH_MASK;
	if (length != net_message.cursize)
		return;	//not an nq packet.

	switch(MSG_ReadByte())
	{
	case CCREP_ACCEPT:
		connectinfo.trying = false;
		if (cls.state >= ca_connected)
		{
			if (cls.demoplayback == DPB_NONE)
				Con_TPrintf ("Dup connect received.  Ignored.\n");
			return;
		}
		port = htons((unsigned short)MSG_ReadLong());
		//this is the port that we're meant to respond to.

		if (port)
		{
			char buf[256];
			net_from.port = port;
			Con_DPrintf("redirecting to port %s\n", NET_AdrToString(buf, sizeof(buf), &net_from));
		}

		cls.proquake_angles_hack = false;
		cls.protocol_nq = CPNQ_ID;
		if (MSG_ReadByte() == 1)	//a proquake server adds a little extra info
		{
			int ver = MSG_ReadByte();
			Con_DPrintf("ProQuake server %i.%i\n", ver/10, ver%10);

//			if (ver >= 34)
			cls.proquake_angles_hack = true;
			if (MSG_ReadByte() == 1)
			{
				//its a 'pure' server.
				Con_Printf("pure ProQuake server\n");
				return;
			}
		}

		Validation_Apply_Ruleset();

		cls.fteprotocolextensions = connectinfo.fteext1;
		cls.fteprotocolextensions2 = connectinfo.fteext2;
		Netchan_Setup (NS_CLIENT, &cls.netchan, &net_from, connectinfo.qport);
		CL_ParseEstablished();
		cls.netchan.isnqprotocol = true;
		cls.netchan.compresstable = NULL;
		cls.protocol = CP_NETQUAKE;
		cls.state = ca_connected;
		Con_TPrintf ("Connected.\n");

		total_loading_size = 100;
		current_loading_size = 0;
		SCR_SetLoadingStage(LS_CLIENT);

#ifdef QUAKESPYAPI
		allowremotecmd = false; // localid required now for remote cmds
#endif

		//send a dummy packet.
		//this makes our local nat think we initialised the conversation, so that we can receive the.
		Netchan_Transmit(&cls.netchan, 1, "\x01", 2500);
		return;

	case CCREP_REJECT:
		s = MSG_ReadString();
		Con_Printf("Connect failed\n%s\n", s);
		return;
	}
}
#endif

void CL_MVDUpdateSpectator (void);
void CL_WriteDemoMessage (sizebuf_t *msg, int payloadoffset);
/*
=================
CL_ReadPackets
=================
*/
void CL_ReadPackets (void)
{
	char	adr[MAX_ADR_SIZE];

	if (!qrenderer)
		return;

//	while (NET_GetPacket ())
	for(;;)
	{
		if (!CL_GetMessage())
#ifndef HAVE_DTLS
			break;
#else
		{
			NET_DTLS_Timeouts(cls.sockets);
			break;
		}

		if (*(int *)net_message.data != -1)
			if (NET_DTLS_Decode(cls.sockets))
				if (!net_message.cursize)
					continue;
#endif

#ifdef NQPROT
		if (cls.demoplayback == DPB_NETQUAKE)
		{
			MSG_BeginReading (cls.netchan.netprim);
			cls.netchan.last_received = realtime;
			CLNQ_ParseServerMessage ();

			if (!cls.demoplayback)
				CL_NextDemo();
			continue;
		}
#endif
#ifdef Q2CLIENT
		if (cls.demoplayback == DPB_QUAKE2)
		{
			MSG_BeginReading (cls.netchan.netprim);
			cls.netchan.last_received = realtime;
			CLQ2_ParseServerMessage ();
			continue;
		}
#endif
		//
		// remote command packet
		//
		if (*(int *)net_message.data == -1)
		{
			CL_ConnectionlessPacket ();
			continue;
		}

		if (net_message.cursize < 6 && (cls.demoplayback != DPB_MVD && cls.demoplayback != DPB_EZTV)) //MVDs don't have the whole sequence header thing going on
		{
			Con_TPrintf ("%s: Runt packet\n", NET_AdrToString(adr, sizeof(adr), &net_from));
			continue;
		}

		if (cls.state == ca_disconnected)
		{	//connect to nq servers, but don't get confused with sequenced packets.
			if (NET_WasSpecialPacket(NS_CLIENT))
				continue;
#ifdef NQPROT
			CLNQ_ConnectionlessPacket ();
#endif
			continue;	//ignore it. We arn't connected.
		}

		//
		// packet from server
		//
		if (!cls.demoplayback &&
			!NET_CompareAdr (&net_from, &cls.netchan.remote_address))
		{
			if (NET_WasSpecialPacket(NS_CLIENT))
				continue;
			Con_DPrintf ("%s:sequenced packet from wrong server\n"
				,NET_AdrToString(adr, sizeof(adr), &net_from));
			continue;
		}

		switch(cls.protocol)
		{
		case CP_NETQUAKE:
#ifdef NQPROT
			if(NQNetChan_Process(&cls.netchan))
			{
				MSG_ChangePrimitives(cls.netchan.netprim);
				CL_WriteDemoMessage (&net_message, msg_readcount);
				CLNQ_ParseServerMessage ();
			}
#endif
			break;
		case CP_PLUGIN:
			break;
		case CP_QUAKE2:
#ifdef Q2CLIENT
			if (!Netchan_Process(&cls.netchan))
				continue;		// wasn't accepted for some reason
			CLQ2_ParseServerMessage ();
			break;
#endif
		case CP_QUAKE3:
#ifdef Q3CLIENT
			CLQ3_ParseServerMessage();
#endif
			break;
		case CP_QUAKEWORLD:
			if (cls.demoplayback == DPB_MVD || cls.demoplayback == DPB_EZTV)
			{
				MSG_BeginReading(cls.netchan.netprim);
				cls.netchan.last_received = realtime;
				cls.netchan.outgoing_sequence = cls.netchan.incoming_sequence;
			}
			else if (!Netchan_Process(&cls.netchan))
				continue;		// wasn't accepted for some reason

			CL_WriteDemoMessage (&net_message, msg_readcount);

			if (cls.netchan.incoming_sequence > cls.netchan.outgoing_sequence)
			{	//server should not be responding to packets we have not sent yet
				Con_DPrintf("Server is from the future! (%i packets)\n", cls.netchan.incoming_sequence - cls.netchan.outgoing_sequence);
				cls.netchan.outgoing_sequence = cls.netchan.incoming_sequence;
			}
			MSG_ChangePrimitives(cls.netchan.netprim);
			CLQW_ParseServerMessage ();
			break;
		case CP_UNKNOWN:
			break;
		}

//		if (cls.demoplayback && cls.state >= ca_active && !CL_DemoBehind())
//			return;
	}

	//
	// check timeout
	//
	if (cls.state >= ca_connected
	 && realtime - cls.netchan.last_received > cl_timeout.value && !cls.demoplayback)
	{
#ifndef CLIENTONLY
		/*don't timeout when we're the actual server*/
		if (!sv.state)
#endif
		{
			Con_TPrintf ("\nServer connection timed out.\n");
			CL_Disconnect ();
			return;
		}
	}

	if (cls.demoplayback == DPB_MVD || cls.demoplayback == DPB_EZTV)
	{
		CL_MVDUpdateSpectator();
	}
}

//=============================================================================

qboolean CL_AllowArbitaryDownload(char *oldname, char *localfile)
{
	int allow;
	//never allow certain (native code) arbitary downloads.
	if (!Q_strncasecmp(localfile, "game", 4) ||	//q2-ey things
		!Q_strcasecmp(localfile, "progs.dat") || !Q_strcasecmp(localfile, "menu.dat") || !Q_strcasecmp(localfile, "csprogs.dat") || !Q_strcasecmp(localfile, "qwprogs.dat") || //overriding gamecode is bad (csqc should be dlcached)
		strstr(localfile, "\\") || strstr(localfile, "..") || strstr(localfile, "./") || strstr(localfile, ":") || strstr(localfile, "//") ||	//certain path patterns are just bad
		Q_strcasestr(localfile, ".qvm") || Q_strcasestr(localfile, ".dll") || Q_strcasestr(localfile, ".so") || Q_strcasestr(localfile, ".dylib"))	//disallow any native code
	{	//yes, I know the user can use a different progs from the one that is specified. If you leave it blank there will be no problem. (server isn't allowed to stuff progs cvar)
		Con_Printf("Ignoring arbitary download to \"%s\" due to possible security risk\n", localfile);
		return false;
	}
	allow = cl_download_redirection.ival;
	if (allow == 2)
	{
		char ext[8];
		COM_FileExtension(localfile, ext, sizeof(ext));
		if (!strncmp(localfile, "demos/", 6) && (!Q_strcasecmp(ext, "mvd") || !Q_strcasecmp(ext, "gz")))
			return true;	//mvdsv popularised the server sending 'download demo/foobar.mvd' in response to 'download demonum/5' aka 'cmd dl #'
		else if (!strncmp(localfile, "package/", 8) && (!Q_strcasecmp(ext, "pak") || !Q_strcasecmp(ext, "pk3") || !Q_strcasecmp(ext, "pk4")))
			return true;	//packages, woo.
							//fixme: we should probably try using package/$gamedir/foo.pak if we get redirected to that.
		else
		{
			Con_Printf("Ignoring non-package download redirection to \"%s\"\n", localfile);
			return false;
		}
	}
	if (allow)
		return true;
	Con_Printf("Ignoring download redirection to \"%s\". This server may require you to set cl_download_redirection to 2.\n", localfile);
	return false;
}

/*
=====================
CL_Download_f
=====================
*/
void CL_Download_f (void)
{
//	char *p, *q;
	char *url = Cmd_Argv(1);
	char *localname = Cmd_Argv(2);

#ifdef WEBCLIENT
	if (!strnicmp(url, "http://", 7) || !strnicmp(url, "https://", 8) || !strnicmp(url, "ftp://", 6))
	{
		if (Cmd_IsInsecure())
			return;
		if (!*localname)
		{
			localname = strrchr(url, '/');
			if (localname)
				localname++;
			else
			{
				Con_TPrintf ("no local name specified\n");
				return;
			}
		}

		HTTP_CL_Get(url, localname, NULL);//"test.txt");
		return;
	}
#endif

	if (!strnicmp(url, "qw://", 5) || !strnicmp(url, "q2://", 5))
	{
		url += 5;
	}

	if (!*localname)
		localname = url;

	if ((cls.state == ca_disconnected || cls.demoplayback) && cls.demoplayback != DPB_EZTV)
	{
		Con_TPrintf ("Must be connected.\n");
		return;
	}

	if (cls.netchan.remote_address.type == NA_LOOPBACK)
	{
		Con_TPrintf ("Must be connected.\n");
		return;
	}

	if (Cmd_Argc() != 2 && Cmd_Argc() != 3)
	{
		Con_TPrintf ("Usage: download <datafile> <localname>\n");
		return;
	}

	if (Cmd_IsInsecure())	//mark server specified downloads.
	{
		if (cls.download && cls.download->method == DL_QWPENDING)
			DL_Abort(cls.download, QDL_FAILED);

		//don't let gamecode order us to download random junk
		if (!CL_AllowArbitaryDownload(NULL, localname))
			return;

		CL_CheckOrEnqueDownloadFile(url, localname, DLLF_REQUIRED|DLLF_VERBOSE);
		return;
	}

	CL_EnqueDownload(url, localname, DLLF_USEREXPLICIT|DLLF_IGNOREFAILED|DLLF_REQUIRED|DLLF_OVERWRITE|DLLF_VERBOSE);
}

void CL_DownloadSize_f(void)
{
	downloadlist_t *dl;
	char *rname;
	char *size;
	char *redirection;

	//if this is a demo.. urm?
	//ignore it. This saves any spam.
	if (cls.demoplayback)
		return;

	rname = Cmd_Argv(1);
	size = Cmd_Argv(2);
	if (!strcmp(size, "e"))
	{
		Con_Printf("Download of \"%s\" failed. Not found.\n", rname);
		CL_DownloadFailed(rname, NULL);
	}
	else if (!strcmp(size, "p"))
	{
		if (cls.download && stricmp(cls.download->remotename, rname))
		{
			Con_Printf("Download of \"%s\" failed. Not allowed.\n", rname);
			CL_DownloadFailed(rname, NULL);
		}
	}
	else if (!strcmp(size, "r"))
	{	//'download this file instead'
		redirection = Cmd_Argv(3);

		if (!CL_AllowArbitaryDownload(rname, redirection))
			return;

		dl = CL_DownloadFailed(rname, NULL);
		Con_DPrintf("Download of \"%s\" redirected to \"%s\".\n", rname, redirection);
		CL_CheckOrEnqueDownloadFile(redirection, NULL, dl->flags);
	}
	else
	{
		for (dl = cl.downloadlist; dl; dl = dl->next)
		{
			if (!strcmp(dl->rname, rname))
			{
				dl->size = strtoul(size, NULL, 0);
				dl->flags &= ~DLLF_SIZEUNKNOWN;
				return;
			}
		}
	}
}

void CL_FinishDownload(char *filename, char *tempname);
static void CL_ForceStopDownload (qboolean finish)
{
	qdownload_t *dl = cls.download;
	if (Cmd_IsInsecure())
	{
		Con_Printf(CON_WARNING "Execution from server rejected for %s\n", Cmd_Argv(0));
		return;
	}
	if (!dl)
		return;

	if (!dl->file)
	{
		if (dl->method == DL_QWPENDING)
			finish = false;
		else
		{
			Con_Printf("No files downloading by QW protocol\n");
			return;
		}
	}

	if (finish)
		DL_Abort(dl, QDL_COMPLETED);
	else
		DL_Abort(dl, QDL_FAILED);

	// get another file if needed
	CL_RequestNextDownload ();
}
void CL_SkipDownload_f (void)
{
	CL_ForceStopDownload(false);
}
void CL_FinishDownload_f (void)
{
	CL_ForceStopDownload(true);
}

#if defined(_WIN32) && !defined(WINRT) && !defined(_XBOX)
#include "winquake.h"
/*
=================
CL_Minimize_f
=================
*/
void CL_Windows_f (void)
{
	if (!mainwindow)
	{
		Con_Printf("Cannot comply\n");
		return;
	}
//	if (modestate == MS_WINDOWED)
//		ShowWindow(mainwindow, SW_MINIMIZE);
//	else
		SendMessage(mainwindow, WM_SYSKEYUP, VK_TAB, 1 | (0x0F << 16) | (1<<29));
}
#endif

#ifndef CLIENTONLY
void CL_ServerInfo_f(void)
{
	if (!sv.state && cls.state && Cmd_Argc() < 2)
	{
		if (cls.demoplayback || cls.protocol != CP_QUAKEWORLD)
		{
			Info_Print (cl.serverinfo, "");
		}
		else
			Cmd_ForwardToServer ();
	}
	else
	{
		SV_Serverinfo_f();	//allow it to be set... (whoops)
	}
}
#endif

#ifdef FTPCLIENT
void CL_FTP_f(void)
{
	FTP_Client_Command(Cmd_Args(), NULL);
}
#endif

//fixme: make a cvar
void CL_Fog_f(void)
{
	int ftype = Q_strcasecmp(Cmd_Argv(0), "fog");
	if ((cl.fog_locked && !Cmd_FromGamecode()) || Cmd_Argc() <= 1)
	{
		if (Cmd_ExecLevel != RESTRICT_INSECURE)
			Con_Printf("Current fog %f (r:%f g:%f b:%f, a:%f bias:%f)\n", cl.fog[ftype].density, cl.fog[ftype].colour[0], cl.fog[ftype].colour[1], cl.fog[ftype].colour[2], cl.fog[ftype].alpha, cl.fog[ftype].depthbias);
	}
	else
	{
		CL_ResetFog(ftype);

		switch(Cmd_Argc())
		{
		case 1:
			break;
		case 2:
			cl.fog[ftype].density = atof(Cmd_Argv(1));
			break;
		case 3:
			cl.fog[ftype].density = atof(Cmd_Argv(1));
			cl.fog[ftype].colour[0] = cl.fog[ftype].colour[1] = cl.fog[ftype].colour[2] = atof(Cmd_Argv(2));
			break;
		case 4:
			cl.fog[ftype].density = 0.05;	//make something up for vauge compat with fitzquake, so it doesn't get the default of 0
			cl.fog[ftype].colour[0] = atof(Cmd_Argv(1));
			cl.fog[ftype].colour[1] = atof(Cmd_Argv(2));
			cl.fog[ftype].colour[2] = atof(Cmd_Argv(3));
			break;
		case 5:
		default:
			cl.fog[ftype].density = atof(Cmd_Argv(1));
			cl.fog[ftype].colour[0] = atof(Cmd_Argv(2));
			cl.fog[ftype].colour[1] = atof(Cmd_Argv(3));
			cl.fog[ftype].colour[2] = atof(Cmd_Argv(4));
			break;
		}

		if (cls.state == ca_active)
			cl.fog[ftype].time += 1;

		//fitz:
		//if (Cmd_Argc() >= 6) cl.fog_time += atof(Cmd_Argv(5));
		//dp:
		if (Cmd_Argc() >= 6) cl.fog[ftype].alpha = atof(Cmd_Argv(5));
		if (Cmd_Argc() >= 7) cl.fog[ftype].depthbias = atof(Cmd_Argv(6));
		//if (Cmd_Argc() >= 8) cl.fog.end = atof(Cmd_Argv(7));
		//if (Cmd_Argc() >= 9) cl.fog.height = atof(Cmd_Argv(8));
		//if (Cmd_Argc() >= 10) cl.fog.fadedepth = atof(Cmd_Argv(9));

		if (Cmd_FromGamecode())
			cl.fog_locked = !!cl.fog[ftype].density;
	}
}

void CL_CrashMeEndgame_f(void)
{
	Host_EndGame("crashme!");
}

void CL_Status_f(void)
{
	char adr[128];
	float pi, po, bi, bo;
	NET_PrintAddresses(cls.sockets);
	if (NET_GetRates(cls.sockets, &pi, &po, &bi, &bo))
		Con_Printf("packets,bytes/sec: in: %g %g  out: %g %g\n", pi, bi, po, bo);	//not relevent as a limit.

	if (cls.state)
	{
		Con_Printf("Server address: %s\n", NET_AdrToString(adr, sizeof(adr), &cls.netchan.remote_address));	//not relevent as a limit.

		switch(cls.protocol)
		{
		default:
		case CP_UNKNOWN:
			Con_Printf("Unknown protocol\n");
			break;
		case CP_QUAKEWORLD:
			Con_Printf("QuakeWorld-based protocol\n");
			break;
	#ifdef NQPROT
		case CP_NETQUAKE:
			switch(cls.protocol_nq)
			{
			case CPNQ_ID:
				Con_Printf("NetQuake-based protocol\n");
				if (cls.proquake_angles_hack)
					Con_Printf("With ProQuake's extended angles\n");
				break;
			case CPNQ_NEHAHRA:
				Con_Printf("Nehahra protocol\n");
				break;
			case CPNQ_BJP1:
				Con_Printf("BJP1 protocol\n");
				break;
			case CPNQ_BJP2:
				Con_Printf("BJP2 protocol\n");
				break;
			case CPNQ_BJP3:
				Con_Printf("BJP3 protocol\n");
				break;
			case CPNQ_FITZ666:
				Con_Printf("FitzQuake-based protocol\n");
				break;
			case CPNQ_DP5:
				Con_Printf("DPP5 protocol\n");
				break;
			case CPNQ_DP6:
				Con_Printf("DPP6 protocol\n");
				break;
			case CPNQ_DP7:
				Con_Printf("DPP7 protocol\n");
				break;
			}
			break;
	#endif
	#ifdef Q2CLIENT
		case CP_QUAKE2:
			Con_Printf("Quake2-based protocol\n");
			if (cls.protocol_q2 && cls.protocol_q2 < PROTOCOL_VERSION_Q2)
				Con_Printf("\toutdated protocol version\n");
			else switch (cls.protocol_q2)
			{
			case PROTOCOL_VERSION_Q2:
				Con_Printf("\tStandard Quake2\n");
				break;
			case PROTOCOL_VERSION_R1Q2:
				Con_Printf("\tR1Q2\n");
				break;
			case PROTOCOL_VERSION_Q2PRO:
				Con_Printf("\tQ2Pro\n");
				break;
			}
			break;
	#endif
	#ifdef Q3CLIENT
		case CP_QUAKE3:
			Con_Printf("Quake3-based protocol\n");
			break;
	#endif
	#ifdef PLUGINS
		case CP_PLUGIN:
			Con_Printf("external protocol\n");
			break;
	#endif
		}

		//just show the more interesting extensions.
		if (cls.fteprotocolextensions & PEXT_FLOATCOORDS)
			Con_Printf("\textended coords\n");
		if (cls.fteprotocolextensions & PEXT_SPLITSCREEN)
			Con_Printf("\tsplit screen\n");
#ifdef PEXT_CSQC
		if (cls.fteprotocolextensions & PEXT_CSQC)
			Con_Printf("\tcsqc info\n");
#endif
		if (cls.fteprotocolextensions2 & PEXT2_VOICECHAT)
			Con_Printf("\tvoice chat\n");
		if (cls.fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS)
			Con_Printf("\treplacement deltas\n");
	}
}

void CL_Demo_SetSpeed_f(void)
{
	char *s = Cmd_Argv(1);
	if (s)
	{
		float f = atof(s)/100;
		Cvar_SetValue(&cl_demospeed, f);
	}
	else
		Con_Printf("demo playback speed %g%%\n", cl_demospeed.value * 100);
}

void CL_Skygroup_f(void);
/*
=================
CL_Init
=================
*/
void CL_Init (void)
{
	extern void CL_Say_f (void);
	extern void CL_SayMe_f (void);
	extern void CL_SayTeam_f (void);
	extern	cvar_t		baseskin;
	extern	cvar_t		noskins;
	char *ver;

	cls.state = ca_disconnected;

#ifdef SVNREVISION
	if (strcmp(STRINGIFY(SVNREVISION), "-"))
		ver = va("%s v%i.%02i %s", DISTRIBUTION, FTE_VER_MAJOR, FTE_VER_MINOR, STRINGIFY(SVNREVISION));
	else
#endif
		ver = va("%s v%i.%02i", DISTRIBUTION, FTE_VER_MAJOR, FTE_VER_MINOR);

	Info_SetValueForStarKey (cls.userinfo[0], "*ver", ver, sizeof(cls.userinfo[0]));

	InitValidation();

	CL_InitInput ();
	CL_InitTEnts ();
	CL_InitPrediction ();
	CL_InitCam ();
	CL_InitDlights();
	PM_Init ();
	TP_Init();

//
// register our commands
//
	CLSCR_Init();
#ifdef MENU_DAT
	MP_RegisterCvarsAndCmds();
#endif
#ifdef CSQC_DAT
	CSQC_RegisterCvarsAndThings();
#endif
	Cvar_Register (&host_speeds, cl_controlgroup);
	Cvar_Register (&developer, cl_controlgroup);

	Cvar_Register (&cfg_save_name, cl_controlgroup);

	Cvar_Register (&cl_proxyaddr, cl_controlgroup);
	Cvar_Register (&cl_sendguid, cl_controlgroup);
	Cvar_Register (&cl_defaultport, cl_controlgroup);
	Cvar_Register (&cl_servername, cl_controlgroup);
	Cvar_Register (&cl_serveraddress, cl_controlgroup);
	Cvar_Register (&cl_demospeed, "Demo playback");
	Cmd_AddCommand("demo_setspeed", CL_Demo_SetSpeed_f);
	Cvar_Register (&cl_upspeed, cl_inputgroup);
	Cvar_Register (&cl_forwardspeed, cl_inputgroup);
	Cvar_Register (&cl_backspeed, cl_inputgroup);
	Cvar_Register (&cl_sidespeed, cl_inputgroup);
	Cvar_Register (&cl_movespeedkey, cl_inputgroup);
	Cvar_Register (&cl_yawspeed, cl_inputgroup);
	Cvar_Register (&cl_pitchspeed, cl_inputgroup);
	Cvar_Register (&cl_anglespeedkey, cl_inputgroup);
	Cvar_Register (&cl_shownet,	cl_screengroup);
	Cvar_Register (&cl_sbar,	cl_screengroup);
	Cvar_Register (&cl_pure,	cl_screengroup);
	Cvar_Register (&cl_hudswap,	cl_screengroup);
	Cvar_Register (&cl_maxfps,	cl_screengroup);
	Cvar_Register (&cl_idlefps, cl_screengroup);
	Cvar_Register (&cl_yieldcpu, cl_screengroup);
	Cvar_Register (&cl_timeout, cl_controlgroup);
	Cvar_Register (&lookspring, cl_inputgroup);
	Cvar_Register (&lookstrafe, cl_inputgroup);
	Cvar_Register (&sensitivity, cl_inputgroup);

	Cvar_Register (&m_pitch, cl_inputgroup);
	Cvar_Register (&m_yaw, cl_inputgroup);
	Cvar_Register (&m_forward, cl_inputgroup);
	Cvar_Register (&m_side, cl_inputgroup);

	Cvar_Register (&rcon_password,	cl_controlgroup);
	Cvar_Register (&rcon_address,	cl_controlgroup);

	Cvar_Register (&cl_lerp_players, cl_controlgroup);
	Cvar_Register (&cl_predict_players,	cl_predictiongroup);
	Cvar_Register (&cl_predict_players_frac,	cl_predictiongroup);
	Cvar_Register (&cl_predict_players_latency,	cl_predictiongroup);
	Cvar_Register (&cl_predict_players_nudge,	cl_predictiongroup);
	Cvar_Register (&cl_solid_players,	cl_predictiongroup);

#ifdef QUAKESPYAPI
	Cvar_Register (&localid,	cl_controlgroup);
#endif

	Cvar_Register (&cl_muzzleflash, cl_controlgroup);

	Cvar_Register (&baseskin,	"Teamplay");
	Cvar_Register (&noskins,	"Teamplay");
	Cvar_Register (&cl_noblink,	"Console controls");	//for lack of a better group

	Cvar_Register (&cl_item_bobbing, "Item effects");
	Cvar_Register (&gl_simpleitems, "Item effects");

	Cvar_Register (&cl_staticsounds, "Item effects");

	Cvar_Register (&r_torch, "Item effects");
	Cvar_Register (&r_rocketlight, "Item effects");
	Cvar_Register (&r_lightflicker, "Item effects");
	Cvar_Register (&cl_r2g, "Item effects");
	Cvar_Register (&r_powerupglow, "Item effects");
	Cvar_Register (&v_powerupshell, "Item effects");

	Cvar_Register (&cl_gibfilter, "Item effects");
	Cvar_Register (&cl_deadbodyfilter, "Item effects");

	Cvar_Register (&cl_nolerp, "Item effects");
	Cvar_Register (&cl_nolerp_netquake, "Item effects");

	Cvar_Register (&r_drawflame, "Item effects");

	Cvar_Register (&cl_downloads, cl_controlgroup);
	Cvar_Register (&cl_download_csprogs, cl_controlgroup);
	Cvar_Register (&cl_download_redirection, cl_controlgroup);
	Cvar_Register (&cl_download_packages, cl_controlgroup);

	//
	// info mirrors
	//
	Cvar_Register (&name,						cl_controlgroup);
	Cvar_Register (&password,					cl_controlgroup);
	Cvar_Register (&spectator,					cl_controlgroup);
	Cvar_Register (&skin,						cl_controlgroup);
	Cvar_Register (&model,						cl_controlgroup);
	Cvar_Register (&team,						cl_controlgroup);
	Cvar_Register (&topcolor,					cl_controlgroup);
	Cvar_Register (&bottomcolor,				cl_controlgroup);
	Cvar_Register (&rate,						cl_controlgroup);
	Cvar_Register (&drate,						cl_controlgroup);
	Cvar_Register (&msg,						cl_controlgroup);
	Cvar_Register (&noaim,						cl_controlgroup);
	Cvar_Register (&b_switch,					cl_controlgroup);
	Cvar_Register (&w_switch,					cl_controlgroup);

	Cvar_Register (&cl_demoreel,				cl_controlgroup);
	Cvar_Register (&record_flush,					cl_controlgroup);

	Cvar_Register (&cl_nofake,					cl_controlgroup);
	Cvar_Register (&cl_chatsound,					cl_controlgroup);
	Cvar_Register (&cl_enemychatsound,				cl_controlgroup);
	Cvar_Register (&cl_teamchatsound,				cl_controlgroup);

	Cvar_Register (&requiredownloads,				cl_controlgroup);
	Cvar_Register (&cl_standardchat,				cl_controlgroup);
	Cvar_Register (&msg_filter,						cl_controlgroup);
	Cvar_Register (&msg_filter_frags,				cl_controlgroup);
	Cvar_Register (&cl_standardmsg,					cl_controlgroup);
	Cvar_Register (&cl_parsewhitetext,				cl_controlgroup);
	Cvar_Register (&cl_nopext,						cl_controlgroup);
	Cvar_Register (&cl_pext_mask,					cl_controlgroup);

	Cvar_Register (&cl_splitscreen,					cl_controlgroup);

	Cvar_Register (&host_mapname,					"Scripting");

#ifndef SERVERONLY
	Cvar_Register (&cl_loopbackprotocol,				cl_controlgroup);
#endif
	Cvar_Register (&cl_countpendingpl,				cl_controlgroup);
	Cvar_Register (&cl_threadedphysics,				cl_controlgroup);
	hud_tracking_show = Cvar_Get("hud_tracking_show", "1", 0, "statusbar");
	hud_miniscores_show = Cvar_Get("hud_miniscores_show", "1", 0, "statusbar");
	Cvar_Register (&cl_download_mapsrc,				cl_controlgroup);

	Cvar_Register (&cl_dlemptyterminate,				cl_controlgroup);

	Cvar_Register (&cl_gunx,					cl_controlgroup);
	Cvar_Register (&cl_guny,					cl_controlgroup);
	Cvar_Register (&cl_gunz,					cl_controlgroup);

	Cvar_Register (&cl_gunanglex,					cl_controlgroup);
	Cvar_Register (&cl_gunangley,					cl_controlgroup);
	Cvar_Register (&cl_gunanglez,					cl_controlgroup);

	Cvar_Register (&ruleset_allow_playercount,			cl_controlgroup);
	Cvar_Register (&ruleset_allow_frj,				cl_controlgroup);
	Cvar_Register (&ruleset_allow_semicheats,			cl_controlgroup);
	Cvar_Register (&ruleset_allow_packet,				cl_controlgroup);
	Cvar_Register (&ruleset_allow_particle_lightning,		cl_controlgroup);
	Cvar_Register (&ruleset_allow_overlongsounds,			cl_controlgroup);
	Cvar_Register (&ruleset_allow_larger_models,			cl_controlgroup);
	Cvar_Register (&ruleset_allow_modified_eyes,			cl_controlgroup);
	Cvar_Register (&ruleset_allow_sensitive_texture_replacements,	cl_controlgroup);
	Cvar_Register (&ruleset_allow_localvolume,			cl_controlgroup);
	Cvar_Register (&ruleset_allow_shaders,			cl_controlgroup);
	Cvar_Register (&ruleset_allow_watervis,			cl_controlgroup);
	Cvar_Register (&ruleset_allow_fbmodels,			cl_controlgroup);

	Cvar_Register (&qtvcl_forceversion1,				cl_controlgroup);
	Cvar_Register (&qtvcl_eztvextensions,				cl_controlgroup);
#ifdef FTPCLIENT
	Cmd_AddCommand ("ftp", CL_FTP_f);
#endif

	Cmd_AddCommandD ("changing", CL_Changing_f, "Part of network protocols. This command should not be used manually.");
	Cmd_AddCommand ("disconnect", CL_Disconnect_f);
	Cmd_AddCommandAD ("record", CL_Record_f, CL_DemoList_c, NULL);
	Cmd_AddCommandAD ("rerecord", CL_ReRecord_f, CL_DemoList_c, "Reconnects to the previous/current server, but starts recording a clean demo.");
	Cmd_AddCommandD ("stop", CL_Stop_f, "Stop demo recording.");
	Cmd_AddCommandAD ("playdemo", CL_PlayDemo_f, CL_DemoList_c, NULL);
	Cmd_AddCommand ("qtvplay", CL_QTVPlay_f);
	Cmd_AddCommand ("qtvlist", CL_QTVList_f);
	Cmd_AddCommand ("qtvdemos", CL_QTVDemos_f);
	Cmd_AddCommandD ("demo_jump", CL_DemoJump_f, "Jump to a specified time in a demo. Prefix with a + or - for a relative offset. Seeking backwards will restart the demo and the fast forward, which can take some time in long demos.");
	Cmd_AddCommandAD ("timedemo", CL_TimeDemo_f, CL_DemoList_c, NULL);
	Cmd_AddCommand ("crashme_endgame", CL_CrashMeEndgame_f);

	Cmd_AddCommandD ("showpic", SCR_ShowPic_Script_f, 	"showpic <imagename> <placename> <x> <y> <zone> [width] [height] [touchcommand]\nDisplays an image onscreen, that potentially has a key binding attached to it when clicked/touched.\nzone should be one of: TL, TR, BL, BR, MM, TM, BM, ML, MR. This serves as an extra offset to move the image around the screen without any foreknowledge of the screen resolution.");
	Cmd_AddCommandD ("showpic_removeall", SCR_ShowPic_Remove_f, 	"removes any pictures inserted with the showpic command.");

	Cmd_AddCommand ("startdemos", CL_Startdemos_f);
	Cmd_AddCommand ("demos", CL_Demos_f);
	Cmd_AddCommand ("stopdemo", CL_Stopdemo_f);

	Cmd_AddCommand ("skins", Skin_Skins_f);
	Cmd_AddCommand ("allskins", Skin_AllSkins_f);

	Cmd_AddCommand ("cl_status", CL_Status_f);
	Cmd_AddCommandD ("quit", CL_Quit_f, "Use this command when you get angry. Does not save any cvars. Use cfg_save to save settings, or use the menu for a prompt.");

#if defined(CL_MASTER)
	Cmd_AddCommandD ("connectbr", CL_ConnectBestRoute_f, "connect address:port\nConnect to a qw server using the best route we can detect.");
#endif
	Cmd_AddCommandD ("connect", CL_Connect_f, "connect scheme://address:port\nConnect to a server. "
#if defined(FTE_TARGET_WEB)
		"Use a scheme of ws:// or wss:// to connect via using websockets."
#else
		"Use a scheme of tcp:// or tls:// to connect via non-udp protocols."
#endif
#if defined(NQPROT) || defined(Q2CLIENT) || defined(Q3CLIENT)
		"\nDefault port is port "STRINGIFY(PORT_QWSERVER)"."
#ifdef NQPROT
		" NQ:"STRINGIFY(PORT_NQSERVER)"."
#endif
		" QW:"STRINGIFY(PORT_QWSERVER)"."
#ifdef Q2CLIENT
		" Q2:"STRINGIFY(PORT_Q2SERVER)"."
#endif
#ifdef Q3CLIENT
		" Q3:"STRINGIFY(PORT_Q3SERVER)"."
#endif
#endif
		);
	Cmd_AddCommandD ("cl_transfer", CL_Transfer_f, "Connect to a different server, disconnecting from the current server only when the new server replies.");
#ifdef TCPCONNECT
	Cmd_AddCommandD ("tcpconnect", CL_TCPConnect_f, "Connect to a server using the tcp:// prefix");
#endif
#ifdef IRCCONNECT
	Cmd_AddCommand ("ircconnect", CL_IRCConnect_f);
#endif
#ifdef NQPROT
	Cmd_AddCommandD ("nqconnect", CLNQ_Connect_f, "Connects to the specified server, defaulting to port "STRINGIFY(PORT_NQSERVER)".");
#endif
	Cmd_AddCommand ("reconnect", CL_Reconnect_f);
	Cmd_AddCommand ("join", CL_Join_f);
	Cmd_AddCommand ("observe", CL_Observe_f);

	Cmd_AddCommand ("rcon", CL_Rcon_f);
	Cmd_AddCommand ("packet", CL_Packet_f);
	Cmd_AddCommand ("user", CL_User_f);
	Cmd_AddCommand ("users", CL_Users_f);

	Cmd_AddCommand ("setinfo", CL_SetInfo_f);
	Cmd_AddCommand ("fullinfo", CL_FullInfo_f);

	Cmd_AddCommand ("color", CL_Color_f);
	Cmd_AddCommand ("download", CL_Download_f);
	Cmd_AddCommand ("dlsize", CL_DownloadSize_f);

	Cmd_AddCommand ("nextul", CL_NextUpload);
	Cmd_AddCommand ("stopul", CL_StopUpload);

	Cmd_AddCommand ("skipdl", CL_SkipDownload_f);
	Cmd_AddCommand ("finishdl", CL_FinishDownload_f);

//
// forward to server commands
//
	Cmd_AddCommand ("god", NULL);	//cheats
	Cmd_AddCommand ("give", NULL);
	Cmd_AddCommand ("noclip", NULL);
	Cmd_AddCommand ("6dof", NULL);
	Cmd_AddCommand ("spiderpig", NULL);
	Cmd_AddCommand ("fly", NULL);
	Cmd_AddCommand ("setpos", NULL);
	Cmd_AddCommand ("notarget", NULL);

	Cmd_AddCommand ("topten", NULL);

	Cmd_AddCommandD ("fog", CL_Fog_f, "fog <density> <red> <green> <blue> <alpha> <depthbias>");
	Cmd_AddCommandD ("waterfog", CL_Fog_f, "waterfog <density> <red> <green> <blue> <alpha> <depthbias>");
	Cmd_AddCommand ("kill", NULL);
	Cmd_AddCommand ("pause", NULL);
	Cmd_AddCommand ("say", CL_Say_f);
	Cmd_AddCommand ("me", CL_SayMe_f);
	Cmd_AddCommand ("sayone", CL_Say_f);
	Cmd_AddCommand ("say_team", CL_SayTeam_f);
#ifdef CLIENTONLY
	Cmd_AddCommand ("serverinfo", NULL);
#else
	Cmd_AddCommand ("serverinfo", CL_ServerInfo_f);
#endif

	Cmd_AddCommand ("skygroup", CL_Skygroup_f);
//
//  Windows commands
//
#if defined(_WIN32) && !defined(WINRT) && !defined(_XBOX)
	Cmd_AddCommand ("windows", CL_Windows_f);
#endif

	Ignore_Init();
#ifdef QUAKEHUD
	Stats_Init();
#endif
	CL_ClearState();	//make sure the cl.* fields are set properly if there's no ssqc or whatever.
}


/*
================
Host_EndGame

Call this to drop to a console without exiting the qwcl
================
*/
NORETURN void VARGS Host_EndGame (char *message, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr,message);
	vsnprintf (string,sizeof(string)-1, message,argptr);
	va_end (argptr);

	COM_AssertMainThread(string);

	SCR_EndLoadingPlaque();

	Con_TPrintf ("^&C0Host_EndGame: %s\n", string);
	Con_Printf ("\n");

	SCR_EndLoadingPlaque();

	CL_Disconnect ();

	SV_UnspawnServer();
	connectinfo.trying = false;

	Cvar_Set(&cl_shownet, "0");

	longjmp (host_abort, 1);
}

/*
================
Host_Error

This shuts down the client and exits qwcl
================
*/
void VARGS Host_Error (char *error, ...)
{
	va_list		argptr;
	char		string[1024];
	static	qboolean inerror = false;

	if (inerror)
		Sys_Error ("Host_Error: recursively entered");
	inerror = true;

	va_start (argptr,error);
	vsnprintf (string,sizeof(string)-1, error,argptr);
	va_end (argptr);
	COM_AssertMainThread(string);
	Con_TPrintf ("Host_Error: %s\n", string);

	CL_Disconnect ();
	cls.demonum = -1;

	inerror = false;

// FIXME
	Sys_Error ("Host_Error: %s\n",string);
}


/*
===============
Host_WriteConfiguration

Writes key bindings and archived cvars to config.cfg
===============
*/
void Host_WriteConfiguration (void)
{
	vfsfile_t	*f;
	char savename[MAX_OSPATH];
	char sysname[MAX_OSPATH];

	if (host_initialized && cfg_save_name.string && *cfg_save_name.string)
	{
		if (strchr(cfg_save_name.string, '.'))
		{
			Con_TPrintf (CON_ERROR "Couldn't write config.cfg.\n");
			return;
		}

		Q_snprintfz(savename, sizeof(savename), "%s.cfg", cfg_save_name.string);

		f = FS_OpenVFS(savename, "wb", FS_GAMEONLY);
		if (!f)
		{
			Con_TPrintf (CON_ERROR "Couldn't write config.cfg.\n");
			return;
		}

		Key_WriteBindings (f);
		Cvar_WriteVariables (f, false);

		VFS_CLOSE (f);

		FS_NativePath(savename, FS_GAMEONLY, sysname, sizeof(sysname));
		Con_Printf("Wrote %s\n", savename);
	}
}


//============================================================================

#if 0
/*
==================
Host_SimulationTime

This determines if enough time has passed to run a simulation frame
==================
*/
qboolean Host_SimulationTime(float time)
{
	float fps;

	if (oldrealtime > realtime)
		oldrealtime = 0;

	if (cl_maxfps.value)
		fps = max(30.0, min(cl_maxfps.value, 72.0));
	else
		fps = max(30.0, min(rate.value/80.0, 72.0));

	if (!cls.timedemo && (realtime + time) - oldrealtime < 1.0/fps)
		return false;			// framerate is too high
	return true;
}
#endif

void Host_RunFileNotify(struct dl_download *dl)
{
	if (dl->file)
	{
		Host_RunFile(dl->url, strlen(dl->url), dl->file);
		dl->file = NULL;
	}
}

#include "fs.h"
#define HRF_OVERWRITE	(1<<0)
#define HRF_NOOVERWRITE	(1<<1)
//						(1<<2)
#define HRF_ABORT		(1<<3)

#define HRF_OPENED		(1<<4)
#define HRF_DOWNLOADED	(1<<5)	//file was actually downloaded, and not from the local system
#define HRF_WAITING		(1<<6)	//file looks important enough that we should wait for it to start to download or something before we try doing other stuff.
//						(1<<7)

#define HRF_DEMO_MVD	(1<<8)
#define HRF_DEMO_QWD	(1<<9)
#define HRF_DEMO_DM2	(1<<10)
#define HRF_DEMO_DEM	(1<<11)

#define HRF_QTVINFO		(1<<12)
#define HRF_MANIFEST	(1<<13)
#define HRF_BSP			(1<<14)
#define HRF_PACKAGE		(1<<15)	//pak or pk3 that should be installed.
#define	HRF_ARCHIVE		(1<<16)	//zip - treated as a multiple-file 'installer'
#define HRF_MODEL		(1<<17)
#define HRF_CONFIG		(1<<18)	//exec it on the console...

#define HRF_ACTION (HRF_OVERWRITE|HRF_NOOVERWRITE|HRF_ABORT)
#define HRF_DEMO		(HRF_DEMO_MVD|HRF_DEMO_QWD|HRF_DEMO_DM2|HRF_DEMO_DEM)
#define HRF_FILETYPES	(HRF_DEMO|HRF_QTVINFO|HRF_MANIFEST|HRF_BSP|HRF_PACKAGE|HRF_ARCHIVE|HRF_MODEL|HRF_CONFIG)
typedef struct {
	unsigned int flags;
	struct dl_download *dl;
	vfsfile_t *srcfile;
	vfsfile_t *dstfile;
	char fname[1];	//system path or url.
} hrf_t;

extern int waitingformanifest;
void Host_DoRunFile(hrf_t *f);
void CL_PlayDemoStream(vfsfile_t *file, char *filename, qboolean issyspath, int demotype, float bufferdelay);
void CL_ParseQTVDescriptor(vfsfile_t *f, const char *name);
qboolean FS_PathURLCache(char *url, char *path, size_t pathsize);

//guesses the file type based upon its file extension. mdl/md3/iqm distinctions are not important, so we can usually get away with this in the context of quake.
unsigned int Host_GuessFileType(const char *mimetype, const char *filename)
{
	if (mimetype)
	{
		if (!strcmp(mimetype, "application/x-qtv"))	//what uses this?
			return HRF_QTVINFO;
		else if (!strcmp(mimetype, "text/x-quaketvident"))
			return HRF_QTVINFO;
		else if (!strcmp(mimetype, "application/x-fteplugin"))
			return HRF_MANIFEST;
		else if (!strcmp(mimetype, "application/x-ftemanifest"))
			return HRF_MANIFEST;
		else if (!strcmp(mimetype, "application/x-multiviewdemo"))
			return HRF_DEMO_MVD;
		else if (!strcmp(mimetype, "application/zip"))
			return HRF_ARCHIVE;
//		else if (!strcmp(mimetype, "application/x-ftebsp"))
//			return HRF_BSP;
//		else if (!strcmp(mimetype, "application/x-ftepackage"))
//			return HRF_PACKAGE;
	}

	if (filename)
	{	//find the query or location part of the url, so we can ignore extra stuff.
		struct
		{
			unsigned int type;
			const char *ext;
		} exts[] =
		{
			//demo formats
			{HRF_DEMO_QWD, "qwd"},
			{HRF_DEMO_QWD, "qwd.gz"},
			{HRF_DEMO_MVD, "mvd"},
			{HRF_DEMO_MVD, "mvd.gz"},
			{HRF_DEMO_DM2, "dm2"},
			{HRF_DEMO_DM2, "dm2.gz"},
			{HRF_DEMO_DEM, "dem"},
			{HRF_DEMO_DEM, "dem.gz"},
			{HRF_QTVINFO, "qtv"},
			//other stuff
			{HRF_MANIFEST, "fmf"},
			{HRF_BSP, "bsp"},
			{HRF_BSP, "map"},
			{HRF_CONFIG, "cfg"},
			{HRF_CONFIG, "rc"},
			{HRF_PACKAGE, "pak"},
			{HRF_PACKAGE, "pk3"},
			{HRF_PACKAGE, "pk4"},
			{HRF_PACKAGE, "wad"},
			{HRF_ARCHIVE, "zip"},
			//model formats
			{HRF_MODEL, "mdl"},
			{HRF_MODEL, "md2"},
			{HRF_MODEL, "md3"},
			{HRF_MODEL, "iqm"},
			{HRF_MODEL, "vvm"},
			{HRF_MODEL, "psk"},
			{HRF_MODEL, "zym"},
			{HRF_MODEL, "dpm"},
			//sprites
			{HRF_MODEL, "spr"},
			{HRF_MODEL, "spr2"},
			//static stuff
			{HRF_MODEL, "obj"},
			{HRF_MODEL, "lwo"},
			{HRF_MODEL, "ase"},
		};
		size_t i;
		const char *ext;
		const char *stop = filename+strlen(filename);
		const char *tag = strchr(filename, '?');
		if (tag && tag < stop)
			stop = tag;
		tag = strchr(filename, '#');
		if (tag && tag < stop)
			stop = tag;

		ext = COM_GetFileExtension(filename, stop);
		if (!Q_strstopcasecmp(ext, stop, ".php"))	//deal with extra extensions the easy way
			ext = COM_GetFileExtension(filename, stop=ext);
		if (!Q_strstopcasecmp(ext, stop, ".gz"))	//deal with extra extensions the easy way
			ext = COM_GetFileExtension(filename, ext);
		if (*ext == '.')
			ext++;

		for (i = 0; i < countof(exts); i++)
			if (!Q_strstopcasecmp(ext, stop, exts[i].ext))
				return exts[i].type;
	}
	return 0;
}

void Host_RunFileDownloaded(struct dl_download *dl)
{
	hrf_t *f = dl->user_ctx;
	if(!f)	//download was previously cancelled.
		return;
	if (dl->status == DL_FAILED)
	{
		f->flags |= HRF_ABORT;
		f->srcfile = NULL;
	}
	else
	{
		if (f->srcfile)	//erk?
			VFS_CLOSE(f->srcfile);
		f->flags |= HRF_OPENED;
		f->srcfile = dl->file;
		dl->file = NULL;
	}

	Host_DoRunFile(f);
}
qboolean Host_BeginFileDownload(struct dl_download *dl, char *mimetype)
{
	qboolean result = false;
	//at this point the file is still downloading, so don't copy it out just yet.
	hrf_t *f = dl->user_ctx;

	if (f->flags & HRF_WAITING)
	{
		f->flags &= ~HRF_WAITING;
		waitingformanifest--;
	}

	if (!(f->flags & HRF_FILETYPES))
	{
		f->flags |= Host_GuessFileType(mimetype, f->fname);
		if (!(f->flags & HRF_FILETYPES))
		{
			if (mimetype)
				Con_Printf("mime type \"%s\" and file extension of \"%s\" not recognised\n", mimetype, f->fname);
			else
				Con_Printf("file extension of \"%s\" not recognised\n", f->fname);
			//file type not guessable from extension either.
			f->flags |= HRF_ABORT;
			Host_DoRunFile(f);
			return false;
		}

		if ((f->flags & HRF_MANIFEST) && !(f->flags & HRF_WAITING))
		{
			f->flags |= HRF_WAITING;
			waitingformanifest++;
		}
	}

	//seeking means we can rewind
	if (f->flags & HRF_DEMO_QWD)
		CL_PlayDemoStream((dl->file = VFSPIPE_Open(2, true)), f->fname, true, DPB_QUAKEWORLD, 0);
	else if (f->flags & HRF_DEMO_MVD)
		CL_PlayDemoStream((dl->file = VFSPIPE_Open(2, true)), f->fname, true, DPB_MVD, 0);
#ifdef Q2CLIENT
	else if (f->flags & HRF_DEMO_DM2)
		CL_PlayDemoStream((dl->file = VFSPIPE_Open(2, true)), f->fname, true, DPB_QUAKE2, 0);
#endif
#ifdef NQPROT
	else if (f->flags & HRF_DEMO_DEM)
	{	//fixme: the demo code can't handle the cd track with streamed/missing-so-far writes.
		dl->file = VFSPIPE_Open(1, true);	//make sure the reader will be seekable, so we can rewind.
//		CL_PlayDemoStream((dl->file = VFSPIPE_Open(2, true)), f->fname, DPB_NETQUAKE, 0);
	}
#endif
	else if (f->flags & (HRF_MANIFEST | HRF_QTVINFO))
	{
		//just use a pipe instead of a temp file, working around an issue with temp files on android
		dl->file = VFSPIPE_Open(1, false);
		return true;
	}
	else if (f->flags & HRF_ARCHIVE)
	{
		char cachename[MAX_QPATH];
		if (!FS_PathURLCache(f->fname, cachename, sizeof(cachename)))
			return false;
		f->srcfile = FS_OpenVFS(cachename, "rb", FS_ROOT);
		if (f->srcfile)
		{
			f->flags |= HRF_OPENED;
			Host_DoRunFile(f);
			return false;
		}
		FS_CreatePath(cachename, FS_ROOT);
		dl->file = FS_OpenVFS(cachename, "wb", FS_ROOT);
		if (dl->file)
			return true;	//okay, continue downloading.
	}
	else if (f->flags & HRF_DEMO)
		Con_Printf("%s: format not supported\n", f->fname);	//demos that are not supported in this build for one reason or another
	else
		return true;

	//demos stream, so we want to continue the http download, but we don't want to do anything with the result.
	if (f->flags & HRF_DEMO)
		result = true;
	else
	{
		f->flags |= HRF_ABORT;
		Host_DoRunFile(f);
	}
	return result;
}
void Host_RunFilePrompted(void *ctx, int button)
{
	hrf_t *f = ctx;
	switch(button)
	{
	case 0:
		f->flags |= HRF_OVERWRITE;
		break;
	case 1:
		f->flags |= HRF_NOOVERWRITE;
		break;
	default:
		f->flags |= HRF_ABORT;
		break;
	}
	Host_DoRunFile(f);
}

#ifdef WEBCLIENT
static qboolean isurl(char *url)
{
#ifdef FTE_TARGET_WEB
	return true;	//assume EVERYTHING is a url, because the local filesystem is pointless.
#endif
	return /*!strncmp(url, "data:", 5) || */!strncmp(url, "http://", 7) || !strncmp(url, "https://", 8);
}
#endif

qboolean FS_FixupGamedirForExternalFile(char *input, char *filename, size_t fnamelen);

void Host_DoRunFile(hrf_t *f)
{
	char qname[MAX_QPATH];
	char displayname[MAX_QPATH];
	char loadcommand[MAX_OSPATH];
	qboolean isnew = false;
	qboolean haschanged = false;

	if (f->flags & HRF_WAITING)
	{
		f->flags &= ~HRF_WAITING;
		waitingformanifest--;
	}
	
	if (f->flags & HRF_ABORT)
	{
done:
		if (f->flags & HRF_WAITING)
			waitingformanifest--;

		if (f->srcfile)
			VFS_CLOSE(f->srcfile);
		if (f->dstfile)
			VFS_CLOSE(f->dstfile);
		Z_Free(f);
		return;
	}

	if (!(f->flags & HRF_FILETYPES))
	{
#ifdef WEBCLIENT
		if (isurl(f->fname) && !f->srcfile)
		{
			if (!(f->flags & HRF_OPENED))
			{
				struct dl_download *dl;
				f->flags |= HRF_OPENED;
				dl = HTTP_CL_Get(f->fname, NULL, Host_RunFileDownloaded);
				if (dl)
				{
					f->flags |= HRF_DOWNLOADED;
					dl->notifystarted = Host_BeginFileDownload;
					dl->user_ctx = f;

					if (!(f->flags & HRF_WAITING))
					{
						f->flags |= HRF_WAITING;
						waitingformanifest++;
					}
					return;
				}
			}
		}
#endif

		f->flags |= Host_GuessFileType(NULL, f->fname);
		
		//if we still don't know what it is, give up.
		if (!(f->flags & HRF_FILETYPES))
		{
			Con_Printf("Host_DoRunFile: unknown filetype\n");
			goto done;
		}

		if (f->flags & HRF_MANIFEST)
		{
			if (!(f->flags & HRF_WAITING))
			{
				f->flags |= HRF_WAITING;
				waitingformanifest++;
			}
		}
	}

	if (f->flags & HRF_DEMO)
	{
		//play directly via system path, no prompts needed
		FS_FixupGamedirForExternalFile(f->fname, loadcommand, sizeof(loadcommand));
		Cbuf_AddText(va("playdemo \"%s\"\n", loadcommand), RESTRICT_LOCAL);

		goto done;
	}
	else if (f->flags & HRF_BSP)
	{
		char shortname[MAX_QPATH];
		COM_StripExtension(COM_SkipPath(f->fname), shortname, sizeof(shortname));
		if (FS_FixupGamedirForExternalFile(f->fname, qname, sizeof(qname)) && !Q_strncasecmp(qname, "maps/", 5))
		{
			COM_StripExtension(qname+5, loadcommand, sizeof(loadcommand));
			Cbuf_AddText(va("map \"%s\"\n", loadcommand), RESTRICT_LOCAL);
			goto done;
		}

		snprintf(loadcommand, sizeof(loadcommand), "map \"%s\"\n", shortname);
		snprintf(displayname, sizeof(displayname), "map: %s", shortname);
		snprintf(qname, sizeof(qname), "maps/%s.bsp", shortname);
	}
	else if (f->flags & HRF_PACKAGE)
	{
		char *shortname;
		shortname = COM_SkipPath(f->fname);
		snprintf(qname, sizeof(qname), "%s", shortname);
		snprintf(loadcommand, sizeof(loadcommand), "fs_restart\n");
		snprintf(displayname, sizeof(displayname), "package: %s", shortname);
	}
	else if (f->flags & HRF_MANIFEST)
	{
		if (f->flags & HRF_OPENED)
		{
			if (f->srcfile)
			{
				ftemanifest_t *man;
				int len = VFS_GETLEN(f->srcfile);
				int foo;
				char *fdata = BZ_Malloc(len+1);
				foo = VFS_READ(f->srcfile, fdata, len);
				fdata[len] = 0;
				if (foo != len || !len)
				{
					Con_Printf("Host_DoRunFile: unable to read file properly\n");
					BZ_Free(fdata);
				}
				else
				{
					host_parms.manifest = Z_StrDup(fdata);
					man = FS_Manifest_Parse(NULL, fdata);
					if (man)
					{
						if (!man->updateurl)
							man->updateurl = Z_StrDup(f->fname);
//						if (f->flags & HRF_DOWNLOADED)
						man->blockupdate = true;
						BZ_Free(fdata);
						PM_Shutdown();
						FS_ChangeGame(man, true, true);
					}
					else
					{
						Con_Printf("Manifest file %s does not appear valid\n", f->fname);
						BZ_Free(fdata);
					}
				}

				goto done;
			}
		}
	}
	else if (f->flags & HRF_MODEL)
	{
		if (!FS_FixupGamedirForExternalFile(f->fname, loadcommand, sizeof(loadcommand)))
			Con_Printf("%s is not within the current gamedir\n", f->fname);
		else
			Cbuf_AddText(va("modelviewer \"%s\"\n", loadcommand), RESTRICT_LOCAL);
		goto done;
	}
	else if (f->flags & HRF_ARCHIVE)
	{
		char cachename[MAX_QPATH];
		struct gamepacks packagespaths[2];
		if (f->srcfile)
			VFS_CLOSE(f->srcfile);
		f->srcfile = NULL;

		memset(packagespaths, 0, sizeof(packagespaths));
		packagespaths[0].url = f->fname;
		packagespaths[0].path = cachename;
		if (FS_PathURLCache(f->fname, cachename, sizeof(cachename)))
		{
			COM_Gamedir("", packagespaths);
		}
		goto done;
	}
	else if (f->flags & HRF_CONFIG)
	{
		if (!(f->flags & HRF_ACTION))
		{
			Key_Dest_Remove(kdm_console);
			M_Menu_Prompt(Host_RunFilePrompted, f, va("Exec %s?\n", COM_SkipPath(f->fname)), "Yes", NULL, "Cancel");
			return;
		}
		if (f->flags & HRF_OPENED)
		{
			size_t len = VFS_GETLEN(f->srcfile);
			char *fdata = BZ_Malloc(len+2);
			if (fdata)
			{
				VFS_READ(f->srcfile, fdata, len);
				fdata[len++] = '\n';
				fdata[len] = 0;
				Cbuf_AddText(fdata, RESTRICT_INSECURE);
				BZ_Free(fdata);
			}
			goto done;
		}
	}
	else if (!(f->flags & HRF_QTVINFO))
	{
		Con_Printf("Host_DoRunFile: filetype not handled\n");
		goto done;
	}

	//at this point we need the file to have been opened.
	if (!(f->flags & HRF_OPENED))
	{
		f->flags |= HRF_OPENED;
		if (!f->srcfile)
		{
#ifdef WEBCLIENT
			if (isurl(f->fname))
			{
				struct dl_download *dl = HTTP_CL_Get(f->fname, NULL, Host_RunFileDownloaded);
				if (dl)
				{
					dl->notifystarted = Host_BeginFileDownload;
					dl->user_ctx = f;
					return;
				}
			}
#endif
			f->srcfile = VFSOS_Open(f->fname, "rb");	//input file is a system path, or something.
		}
	}

	if (!f->srcfile)
	{
		Con_Printf("Unable to open %s\n", f->fname);
		goto done;
	}

	if (f->flags & HRF_MANIFEST)
	{
		Host_DoRunFile(f);
		return;
	}

	if (f->flags & HRF_QTVINFO)
	{
		//pass the file object to the qtv code instead of trying to install it.
		CL_ParseQTVDescriptor(f->srcfile, f->fname);
		f->srcfile = NULL;

		goto done;
	}

	VFS_SEEK(f->srcfile, 0);

	f->dstfile = FS_OpenVFS(qname, "rb", FS_GAME);
	if (f->dstfile)
	{
		//do a real diff.
		if (f->srcfile->seekstyle == SS_UNSEEKABLE || VFS_GETLEN(f->srcfile) != VFS_GETLEN(f->dstfile))
		{
			//if we can't seek, or the sizes differ, just assume that the file is modified.
			haschanged = true;
		}
		else
		{
			int len = VFS_GETLEN(f->srcfile);
			char sbuf[8192], dbuf[8192];
			if (len > sizeof(sbuf))
				len = sizeof(sbuf);
			VFS_READ(f->srcfile, sbuf, len);
			VFS_READ(f->dstfile, dbuf, len);
			haschanged = memcmp(sbuf, dbuf, len);
			VFS_SEEK(f->srcfile, 0);
		}
		VFS_CLOSE(f->dstfile);
		f->dstfile = NULL;
	}
	else
		isnew = true;

	if (haschanged)
	{
		if (!(f->flags & HRF_ACTION))
		{
			Key_Dest_Remove(kdm_console);
			M_Menu_Prompt(Host_RunFilePrompted, f, va("File already exists.\nWhat would you like to do?\n%s\n", displayname), "Overwrite", "Run old", "Cancel");
			return;
		}
	}
	else if (isnew)
	{
		if (!(f->flags & HRF_ACTION))
		{
			Key_Dest_Remove(kdm_console);
			M_Menu_Prompt(Host_RunFilePrompted, f, va("File appears new.\nWould you like to install\n%s\n", displayname), "Install!", "", "Cancel");
			return;
		}
	}

	if (f->flags & HRF_OVERWRITE)
	{
		char buffer[8192];
		int len;
		f->dstfile = FS_OpenVFS(qname, "wb", FS_GAMEONLY);
		if (f->dstfile)
		{
#ifdef FTE_TARGET_WEB
			VFS_SEEK(f->dstfile, VFS_GETLEN(f->srcfile));
			VFS_WRITE(f->dstfile, "zomg", 0);	//hack to ensure the file is there, avoiding excessive copies.
			VFS_SEEK(f->dstfile, 0);
#endif
			while(1)
			{
				len = VFS_READ(f->srcfile, buffer, sizeof(buffer));
				if (len <= 0)
					break;
				VFS_WRITE(f->dstfile, buffer, len);
			}
			VFS_CLOSE(f->dstfile);
			f->dstfile = NULL;
		}
	}

	Cbuf_AddText(loadcommand, RESTRICT_LOCAL);

	f->flags |= HRF_ABORT;
	Host_DoRunFile(f);
	return;
}

//only valid once the host has been initialised, as it needs a working filesystem.
//if file is specified, takes full ownership of said file, including destruction.
qboolean Host_RunFile(const char *fname, int nlen, vfsfile_t *file)
{
	hrf_t *f;
#if defined(_WIN32) && !defined(FTE_SDL) && !defined(WINRT) && !defined(_XBOX)
	//win32 file urls are basically fucked, so defer to the windows api.
	char utf8[MAX_OSPATH*3];
	if (nlen >= 7 && !strncmp(fname, "file://", 7))
	{
		qboolean Sys_ResolveFileURL(const char *inurl, int inlen, char *out, int outlen);
		if (!Sys_ResolveFileURL(fname, nlen, utf8, sizeof(utf8)))
		{
			Con_Printf("Cannot resolve file url\n");
			return false;
		}
		fname = utf8;
		nlen = strlen(fname);
	}
	else
#elif !defined(FTE_TARGET_WEB)
	//unix file urls are fairly consistant.
	if (nlen >= 8 && !strncmp(fname, "file:///", 8))
	{
		fname += 7;
		nlen -= 7;
	}
	else
#endif
		if (nlen >= 5 && !strncmp(fname, "qw://", 5))
	{	//this is also implemented by ezquake, so be careful here...
		//"qw://[stream@]host[:port]/COMMAND" join, spectate, qtvplay
		char *t, *cmd;
		const char *url;
		char buffer[8192];
		t = Z_Malloc(nlen+1);
		memcpy(t, fname, nlen);
		t[nlen] = 0;
		url = t+5;

		for (cmd = t+5; *cmd; cmd++)
		{
			if (*cmd == '/')
			{
				*cmd++ = 0;
				break;
			}
		}

		//quote the url safely.
		url = COM_QuotedString(url, buffer, sizeof(buffer), false);

		//now figure out what the command actually was
		if (!Q_strcasecmp(cmd, "join"))
			Cbuf_AddText(va("join %s\n", url), RESTRICT_LOCAL);
		else if (!Q_strcasecmp(cmd, "spectate") || !strcmp(cmd, "observe"))
			Cbuf_AddText(va("observe %s\n", url), RESTRICT_LOCAL);
		else if (!Q_strcasecmp(cmd, "qtvplay"))
			Cbuf_AddText(va("qtvplay %s\n", url), RESTRICT_LOCAL);
		else if (!*cmd || !Q_strcasecmp(cmd, "connect"))
			Cbuf_AddText(va("connect %s\n", url), RESTRICT_LOCAL);
		else
			Con_Printf("Unknown url command: %s\n", cmd);

		if(file)
			VFS_CLOSE(file);
		Z_Free(t);
		return true;
	}

	f = Z_Malloc(sizeof(*f) + nlen);
	memcpy(f->fname, fname, nlen);
	f->fname[nlen] = 0;
	f->srcfile = file;
	if (file)
		f->flags |= HRF_OPENED;

	Con_Printf("Opening external file: %s\n", f->fname);

	Host_DoRunFile(f);
	return true;
}

void CL_UpdateHeadAngles(void)
{
/*FIXME: no idea what I'm doing with this. lets just not break anything for now
	//identity, for now
	vec3_t headchange[3] =
	{
		{1,0,0},
		{0,1,0},
		{0,0,1}
	};
	vec3_t tmp[3], tmp2[3];
	playerview_t *pv = &cl.playerview[0];

	tmp2[0][0] = 0;
	tmp2[0][1] = host_frametime*90;
	tmp2[0][2] = 0;
	AngleVectorsFLU(tmp2[0], headchange[0], headchange[1], headchange[2]);

	switch(cl_headmode.ival)
	{
	case 3: //head angles change both
		R_ConcatRotations(headchange, r_refdef.headaxis, tmp);
		break;
	case 2:	//head changes are entirely relative to the 'view' angle
		R_ConcatRotations(headchange, r_refdef.headaxis, tmp);
		memcpy(r_refdef.headaxis, tmp, sizeof(r_refdef.headaxis));
		break;
	case 1:	//head changes change the view angle directly.

		AngleVectorsFLU(pv->viewangles, tmp[0], tmp[1], tmp[2]);
		R_ConcatRotations(headchange, tmp, tmp2);
		VectorAngles(tmp2[0], tmp2[2], pv->viewangles);
		pv->viewangles[0] *= r_meshpitch.value;

		//fall through
	default:
	case 0:	//off
		VectorSet(r_refdef.headaxis[0], 1, 0, 0);
		VectorSet(r_refdef.headaxis[1], 0, 1, 0);
		VectorSet(r_refdef.headaxis[2], 0, 0, 1);
		break;
	}
	*/
	VectorSet(r_refdef.headaxis[0], 1, 0, 0);
	VectorSet(r_refdef.headaxis[1], 0, 1, 0);
	VectorSet(r_refdef.headaxis[2], 0, 0, 1);
}

/*
==================
Host_Frame

Runs all active servers
==================
*/
extern cvar_t cl_netfps;
extern cvar_t cl_sparemsec;

void CL_StartCinematicOrMenu(void);
int		nopacketcount;
void SNDDMA_SetUnderWater(qboolean underwater);
double Host_Frame (double time)
{
	static double		time0 = 0;
	static double		time1 = 0;
	static double		time2 = 0;
	static double		time3 = 0;
	int			pass0, pass1, pass2, pass3, i;
//	float fps;
	double newrealtime;
	static double spare;
	float maxfps;
	qboolean maxfpsignoreserver;
	qboolean idle;
	extern int r_blockvidrestart;
	static qboolean hadwork;

	RSpeedLocals();

	if (setjmp (host_abort) )
	{
		return 0;			// something bad happened, or the server disconnected
	}

	newrealtime = Media_TweekCaptureFrameTime(realtime, time);

	time = newrealtime - realtime;
	realtime = newrealtime;

	if (oldrealtime > realtime)
		oldrealtime = realtime;

	if (cl.gamespeed<0.1)
		cl.gamespeed = 1;
	time *= cl.gamespeed;

#ifdef WEBCLIENT
//	FTP_ClientThink();
	HTTP_CL_Think();
#endif

	if (r_blockvidrestart)
	{
		if (waitingformanifest)
		{
			COM_MainThreadWork();
			return 0.1;
		}
		Host_FinishLoading();
		return 0;
	}
	if (startuppending)
		CL_StartCinematicOrMenu();

#ifdef PLUGINS
	Plug_Tick();
#endif
	NET_Tick();

	if (cl.paused)
		cl.gametimemark += time;

	idle = (cls.state == ca_disconnected) || 
#ifdef VM_UI
		UI_MenuState() != 0 || 
#endif
		Key_Dest_Has(kdm_gmenu) || 
		Key_Dest_Has(kdm_emenu) || 
		Key_Dest_Has(kdm_editor) ||
		!vid.activeapp ||
		cl.paused
		;
	// TODO: check if minimized or unfocused

	//read packets early and always, so we don't have stuff waiting for reception quite so often.
	//should smooth out a few things, and increase download speeds.
	if (!cls.timedemo)
		CL_ReadPackets ();

	if (idle && cl_idlefps.value > 0)
	{
		double idlesec = 1.0 / cl_idlefps.value;
		if (idlesec > 0.1)
			idlesec = 0.1; // limit to at least 10 fps
#ifdef HAVE_MEDIA_ENCODER
		if (Media_Capturing())
			idlesec = 0;
#endif
		if ((realtime - oldrealtime) < idlesec)
		{
#ifndef CLIENTONLY
			if (sv.state)
			{
				RSpeedRemark();
				SV_Frame();
				RSpeedEnd(RSPEED_SERVER);
			}
#endif
			while(COM_DoWork(0, false))
				;
			return idlesec - (realtime - oldrealtime);
		}
	}

/*
	if (cl_maxfps.value)
		fps = cl_maxfps.value;//max(30.0, min(cl_maxfps.value, 72.0));
	else
		fps = max(30.0, min(rate.value/80.0, 72.0));

	if (!cls.timedemo && realtime - oldrealtime < 1.0/fps)
		return;			// framerate is too high

	*/
	Mod_Think();	//think even on idle (which means small walls and a fast cpu can get more surfaces done.

#ifndef CLIENTONLY
	if (sv.state && cls.state != ca_active)
	{
		maxfpsignoreserver = false;
		maxfps = 0;
	}
	else
#endif
		if ((cl_netfps.value>0 || cls.demoplayback || cl_threadedphysics.ival))
	{	//limit the fps freely, and expect the netfps to cope.
		maxfpsignoreserver = true;
		maxfps = cl_maxfps.ival;
	}
	else
	{
		maxfpsignoreserver = false;
		maxfps = (cl_maxfps.ival>0||cls.protocol!=CP_QUAKEWORLD)?cl_maxfps.value:((cl_netfps.value>0)?cl_netfps.value:cls.maxfps);
		/*gets buggy at times longer than 250ms (and 0/negative, obviously)*/
		if (maxfps < 4)
			maxfps = 4;
	}

	if (vid.isminimized && (maxfps <= 0 || maxfps > 10))
		maxfps = 10;

	if (maxfps > 0 
#ifdef HAVE_MEDIA_ENCODER
		&& Media_Capturing() != 2
#endif
		)
	{
//		realtime += spare/1000;	//don't use it all!
		double newspare = CL_FilterTime((spare/1000 + realtime - oldrealtime)*1000, maxfps, 1.5, maxfpsignoreserver);
		if (!newspare)
		{
			while(COM_DoWork(0, false))
				;
			return (cl_yieldcpu.ival || vid.isminimized)? (1.0 / maxfps - (realtime - oldrealtime)) : 0;
		}
		if (spare < 0 || cls.state < ca_onserver)
			spare = 0;	//uncapped.
		if (spare > cl_sparemsec.ival)
			spare = cl_sparemsec.ival;
		spare = newspare;

//		realtime -= spare/1000;	//don't use it all!
	}
	else
		spare = 0;

	host_frametime = (realtime - oldrealtime)*cl.gamespeed;
	oldrealtime = realtime;

	if (cls.demoplayback && !cl.stillloading)
	{
		qboolean haswork = cl.sendprespawn || COM_HasWork();
		if (!hadwork && !haswork)
			CL_ProgressDemoTime();
		hadwork = haswork;
	}
	cl.stillloading = cl.sendprespawn
#ifdef LOADERTHREAD
		|| (cls.state < ca_active && worker_flush.ival && COM_HasWork())
#endif
		;
	COM_MainThreadWork();


#if defined(Q2CLIENT)
	if (cls.protocol == CP_QUAKE2)
		cl.time += host_frametime;
#endif


//	if (host_frametime > 0.2)
//		host_frametime = 0.2;

	// get new key events
	Sys_SendKeyEvents ();

	// allow mice or other external controllers to add commands
	IN_Commands ();

	// process console commands
	Cbuf_Execute ();

#ifndef CLIENTONLY
	if (isDedicated)	//someone changed it.
		return 0;
#endif

	cls.framecount++;

	RSpeedRemark();

	CL_UseIndepPhysics(!!cl_threadedphysics.ival);

	cl.do_lerp_players = cl_lerp_players.ival || (cls.demoplayback==DPB_MVD || cls.demoplayback == DPB_EZTV) || (cls.demoplayback && !cl_nolerp.ival && !cls.timedemo);
	CL_AllowIndependantSendCmd(false);

	// fetch results from server
	CL_ReadPackets ();

	// send intentions now
	// resend a connection request if necessary
	if (cls.state == ca_disconnected)
	{
		CL_SendCmd (host_frametime, true);
//		IN_Move(NULL, 0, time);
		CL_CheckForResend ();

#ifdef VOICECHAT
		S_Voip_Transmit(0, NULL);
#endif
	}
	else
	{
		if (connectinfo.trying)
			CL_CheckForResend ();
		CL_SendCmd (cl.gamespeed?host_frametime/cl.gamespeed:host_frametime, true);

		if (cls.state == ca_onserver && cl.validsequence && cl.worldmodel)
		{	// first update is the final signon stage
			if (cls.protocol == CP_NETQUAKE)
			{
				//nq can send 'frames' without any entities before we're on the server, leading to short periods where the local player's position is not known. this is bad. so be more cautious with nq. this might break csqc.
				CL_TransitionEntities();
				if (cl.currentpackentities->num_entities || cl.currentpackentities->servertime
#ifdef CSQC_DAT
					|| (cls.fteprotocolextensions & PEXT_CSQC)
#endif
					)
					CL_MakeActive("Quake");
			}
			else
				CL_MakeActive("QuakeWorld");
		}
	}
	CL_AllowIndependantSendCmd(true);

	RSpeedEnd(RSPEED_PROTOCOL);

	if (host_speeds.ival)
		time0 = Sys_DoubleTime ();

#ifndef CLIENTONLY
	if (sv.state)
	{
		float ohft = host_frametime;
		RSpeedRemark();
		SV_Frame();
		RSpeedEnd(RSPEED_SERVER);
		host_frametime = ohft;
//		if (cls.protocol != CP_QUAKE3 && cls.protocol != CP_QUAKE2)
//			CL_ReadPackets ();	//q3's cgame cannot cope with input commands with the same time as the most recent snapshot value
	}
#endif
	CL_CalcClientTime();

	// update video
	if (host_speeds.ival)
		time1 = Sys_DoubleTime ();

	for (i = 0; i < MAX_SPLITS; i++)
	{
		cl.playerview[i].audio.defaulted = true;
		cl.playerview[i].audio.entnum = cl.playerview[i].viewentity;
		VectorClear(cl.playerview[i].audio.origin);
		VectorSet(cl.playerview[i].audio.forward, 1, 0, 0);
		VectorSet(cl.playerview[i].audio.right, 0, 1, 0);
		VectorSet(cl.playerview[i].audio.up, 0, 0, 1);
		cl.playerview[i].audio.reverbtype = 0;
		VectorClear(cl.playerview[i].audio.velocity);
	}

	if (SCR_UpdateScreen && !vid.isminimized)
	{
		extern cvar_t scr_chatmodecvar, r_stereo_method;

		if (scr_chatmodecvar.ival && cl.intermissionmode == IM_NONE)
			scr_chatmode = (cl.playerview[0].spectator&&cl.splitclients<2&&cls.state == ca_active)?2:1;
		else
			scr_chatmode = 0;

		r_refdef.stereomethod = r_stereo_method.ival;
#ifdef FTE_TARGET_WEB
		if (emscriptenfte_getvrframedata())
			r_refdef.stereomethod = STEREO_WEBVR;
#endif
		CL_UpdateHeadAngles();

		{
			RSpeedMark();
			if (SCR_UpdateScreen())
				fps_count++;
			if (R2D_Flush)
				Sys_Error("update didn't flush 2d cache\n");
			RSpeedEnd(RSPEED_TOTALREFRESH);
		}
	}
	else
		fps_count++;

	if (host_speeds.ival)
		time2 = Sys_DoubleTime ();

	// update audio
	for (i = 0 ; i < MAX_SPLITS; i++)
	{
		playerview_t *pv = &cl.playerview[cl.splitclients?i % cl.splitclients:0];
		S_UpdateListener (i, pv->audio.entnum, pv->audio.origin, pv->audio.forward, pv->audio.right, pv->audio.up, pv->audio.reverbtype, pv->audio.velocity);
	}

	S_Update ();

	CDAudio_Update();

	if (host_speeds.ival)
	{
		pass0 = (time0 - time3)*1000000;
		time3 = Sys_DoubleTime ();
		pass1 = (time1 - time0)*1000000;
		pass2 = (time2 - time1)*1000000;
		pass3 = (time3 - time2)*1000000;
		Con_Printf ("%4i tot %4i idle %4i server %4i gfx %4i snd\n",
					pass0+pass1+pass2+pass3, pass0, pass1, pass2, pass3);
	}


	IN_Commands ();

	// process console commands
	Cbuf_Execute ();

	CL_RequestNextDownload();


	CL_QTVPoll();

	TP_UpdateAutoStatus();

	host_framecount++;
	return 0;
}

static void simple_crypt(char *buf, int len)
{
	if (!(*buf & 128))
		return;
	while (len--)
		*buf++ ^= 0xff;
}

void Host_FixupModelNames(void)
{
	simple_crypt(emodel_name, sizeof(emodel_name) - 1);
	simple_crypt(pmodel_name, sizeof(pmodel_name) - 1);
	simple_crypt(prespawn_name,  sizeof(prespawn_name)  - 1);
	simple_crypt(modellist_name, sizeof(modellist_name) - 1);
	simple_crypt(soundlist_name, sizeof(soundlist_name) - 1);
}



#ifdef Q3CLIENT
void CL_ReadCDKey(void)
{	//q3 cdkey
	//you don't need one, just use a server without sv_strictauth set to 0.
	char *buffer;
	buffer = COM_LoadTempFile("q3key", NULL);
	if (buffer)	//a cdkey is meant to be 16 chars
	{
		char *chr;
		for (chr = buffer; *chr; chr++)
		{
			if (*(unsigned char*)chr < ' ')
			{
				*chr = '\0';	//don't get more than one line.
				break;
			}
		}
		Cvar_Get("cl_cdkey", buffer, CVAR_LATCH|CVAR_NOUNSAFEEXPAND, "Q3 compatability");
	}
}
#endif

//============================================================================

void CL_StartCinematicOrMenu(void)
{
	COM_MainThreadWork();

	if (com_installer && FS_DownloadingPackage())
	{
		startuppending = true;
		return;
	}
	if (cls.download)
	{
		startuppending = true;
		return;
	}
	if (startuppending)
	{
		if (startuppending == 2)	//installer finished.
			Cbuf_AddText("\nfs_restart\nvid_restart\n", RESTRICT_LOCAL);
		startuppending = false;
		Key_Dest_Remove(kdm_console);	//make sure console doesn't stay up weirdly.
	}

	//start up the ui now we have a renderer
#ifdef VM_UI
	UI_Start();
#endif

#ifdef MENU_DAT
	Cbuf_AddText("menu_restart\n", RESTRICT_LOCAL);
#endif

	Con_TPrintf ("^Ue080^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081 %s %sInitialized ^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue082\n", *fs_gamename.string?fs_gamename.string:"Nothing", com_installer?"Installer ":"");

	//there might be some console command or somesuch waiting for the renderer to begin (demos or map command or whatever all need model support).
	realtime+=1;
	Cbuf_Execute ();	//server may have been waiting for the renderer

	Con_ClearNotify();

	TP_ExecTrigger("f_startup", true);
	Cbuf_Execute ();

	if (com_installer)
	{
		com_installer = false;
#if 0
		Key_Dest_Remove(kdm_console);	//make sure console doesn't stay up weirdly.
		M_Menu_Installer();
		return;
#endif
	}

	//and any startup cinematics
#ifdef HAVE_MEDIA_DECODER
#ifndef CLIENTONLY
	if (!sv.state)
#endif
	if (!cls.demoinfile && !cls.state && !*cls.servername && !Media_PlayingFullScreen())
	{
		int ol_depth;
		int idcin_depth;
		int idroq_depth;

		idcin_depth = COM_FDepthFile("video/idlog.cin", true);	//q2
		idroq_depth = COM_FDepthFile("video/idlogo.roq", true);	//q3
		ol_depth = COM_FDepthFile("video/openinglogos.roq", true);	//jk2

		if (ol_depth != 0x7fffffff && (ol_depth <= idroq_depth || ol_depth <= idcin_depth))
			Media_PlayFilm("video/openinglogos.roq", true);
		else if (idroq_depth != 0x7fffffff && idroq_depth <= idcin_depth)
			Media_PlayFilm("video/idlogo.roq", true);
		else if (idcin_depth != 0x7fffffff)
			Media_PlayFilm("video/idlog.cin", true);

#ifndef NOLEGACY
		//and for fun (blame spirit):
		if (COM_FCheckExists("data/local/video/New_Bliz640x480.bik"))
			Media_PlayFilm("av:data/local/video/New_Bliz640x480.bik", true);
		if (COM_FCheckExists("data/local/video/BlizNorth640x480.bik"))
			Media_PlayFilm("av:data/local/video/BlizNorth640x480.bik", true);
		if (COM_FCheckExists("data/local/video/eng/d2intro640x292.bik"))
			Media_PlayFilm("av:data/local/video/eng/d2intro640x292.bik", true);
		if (COM_FCheckExists("Data/Local/Video/ENG/D2x_Intro_640x292.bik"))
			Media_PlayFilm("av:Data/Local/Video/ENG/D2x_Intro_640x292.bik", true);
#endif
	}
#endif

	if (!cls.demoinfile && !cls.state && !*cls.servername && !Media_PlayingFullScreen())
	{
#ifndef CLIENTONLY
		if (!sv.state)
#endif
		{
			if (qrenderer > QR_NONE && !Key_Dest_Has(kdm_emenu))
			{
#ifndef NOBUILTINMENUS
				if (!cls.state && !Key_Dest_Has(kdm_emenu) && !*FS_GetGamedir(false))
					M_Menu_Mods_f();
#endif
				if (!cls.state && !Key_Dest_Has(kdm_emenu) && cl_demoreel.ival)
				{
					cls.demonum = 0;
					CL_NextDemo();
				}
				if (!cls.state && !Key_Dest_Has(kdm_emenu))
					//if we're (now) meant to be using csqc for menus, make sure that its running.
					if (!CSQC_UnconnectedInit())
						M_ToggleMenu_f();
			}
			//Con_ForceActiveNow();
		}
	}
}

void CL_ArgumentOverrides(void)
{
	int i;
	if (COM_CheckParm ("-window") || COM_CheckParm ("-startwindowed"))
		Cvar_Set(Cvar_FindVar("vid_fullscreen"), "0");
	if (COM_CheckParm ("-fullscreen"))
		Cvar_Set(Cvar_FindVar("vid_fullscreen"), "1");

	if ((i = COM_CheckParm ("-width")))	//width on it's own also sets height
	{
		Cvar_Set(Cvar_FindVar("vid_width"), com_argv[i+1]);
		Cvar_SetValue(Cvar_FindVar("vid_height"), (atoi(com_argv[i+1])/4)*3);
	}
	if ((i = COM_CheckParm ("-height")))
		Cvar_Set(Cvar_FindVar("vid_height"), com_argv[i+1]);

	if ((i = COM_CheckParm ("-conwidth")))	//width on it's own also sets height
	{
		Cvar_Set(Cvar_FindVar("vid_conwidth"), com_argv[i+1]);
		Cvar_SetValue(Cvar_FindVar("vid_conheight"), (atoi(com_argv[i+1])/4)*3);
	}
	if ((i = COM_CheckParm ("-conheight")))
		Cvar_Set(Cvar_FindVar("vid_conheight"), com_argv[i+1]);

	if ((i = COM_CheckParm ("-bpp")))
		Cvar_Set(Cvar_FindVar("vid_bpp"), com_argv[i+1]);

	if (COM_CheckParm ("-current"))
		Cvar_Set(Cvar_FindVar("vid_desktopsettings"), "1");

	if (COM_CheckParm("-condebug"))
		Cvar_Set(Cvar_FindVar("log_enable"), "1");

	if ((i = COM_CheckParm ("-particles")))
		Cvar_Set(Cvar_FindVar("r_part_maxparticles"), com_argv[i+1]);
}

//note that this does NOT include commandline.
void CL_ExecInitialConfigs(char *resetcommand)
{
#ifdef QUAKESTATS
	int qrc, hrc;
#endif
	int def;

	Cbuf_Execute ();	//make sure any pending console commands are done with. mostly, anyway...
	
	Cbuf_AddText("unbindall\nshowpic_removeall\n", RESTRICT_LOCAL);
	Cbuf_AddText("bind volup \"inc volume 0.1\"\n", RESTRICT_LOCAL);
	Cbuf_AddText("bind voldown \"inc volume -0.1\"\n", RESTRICT_LOCAL);
	Cbuf_AddText("alias restart_ents \"changelevel . .\"\n",RESTRICT_LOCAL);
	Cbuf_AddText("alias restart \"changelevel .\"\n",RESTRICT_LOCAL);
	Cbuf_AddText("alias startmap_sp \"map start\"\n", RESTRICT_LOCAL);
	Cbuf_AddText("cl_warncmd 0\n", RESTRICT_LOCAL);
	Cbuf_AddText("cvar_purgedefaults\n", RESTRICT_LOCAL);	//reset cvar defaults to their engine-specified values. the tail end of 'exec default.cfg' will update non-cheat defaults to mod-specified values.
	Cbuf_AddText("cvarreset *\n", RESTRICT_LOCAL);			//reset all cvars to their current (engine) defaults
#ifndef CLIENTONLY
	Cbuf_AddText(va("sv_gamedir \"%s\"\n", FS_GetGamedir(true)), RESTRICT_LOCAL);
#endif
	Cbuf_AddText(resetcommand, RESTRICT_LOCAL);
	Cbuf_AddText("\n", RESTRICT_LOCAL);
	COM_ParsePlusSets(true);

	def = COM_FDepthFile("default.cfg", true);	//q2/q3/tc
#ifdef QUAKETC
	Cbuf_AddText ("exec default.cfg\n", RESTRICT_LOCAL);
	if (COM_FDepthFile ("config.cfg", true) <= def)
		Cbuf_AddText ("exec config.cfg\n", RESTRICT_LOCAL);
	if (COM_FCheckExists ("autoexec.cfg"))
		Cbuf_AddText ("exec autoexec.cfg\n", RESTRICT_LOCAL);
#else
	//who should we imitate?
	qrc = COM_FDepthFile("quake.rc", true);	//q1
	hrc = COM_FDepthFile("hexen.rc", true);	//h2

	if (qrc <= def && qrc <= hrc && qrc!=0x7fffffff)
	{
		Cbuf_AddText ("exec quake.rc\n", RESTRICT_LOCAL);
		def = qrc;
	}
	else if (hrc <= def && hrc!=0x7fffffff)
	{
		Cbuf_AddText ("exec hexen.rc\n", RESTRICT_LOCAL);
		def = hrc;
	}
	else
	{	//they didn't give us an rc file!
		int cfg = COM_FDepthFile ("config.cfg", true);
		int q3cfg = COM_FDepthFile ("q3config.cfg", true);
	//	Cbuf_AddText ("bind ` toggleconsole\n", RESTRICT_LOCAL);	//in case default.cfg does not exist. :(
		if (def!=0x7fffffff)
			Cbuf_AddText ("exec default.cfg\n", RESTRICT_LOCAL);
		if (cfg <= def && cfg!=0x7fffffff)
			Cbuf_AddText ("exec config.cfg\n", RESTRICT_LOCAL);
		if (q3cfg <= def && q3cfg!=0x7fffffff)
			Cbuf_AddText ("exec q3config.cfg\n", RESTRICT_LOCAL);
		if (def!=0x7fffffff)
			Cbuf_AddText ("exec autoexec.cfg\n", RESTRICT_LOCAL);
	}
	qrc = COM_FDepthFile("fte.cfg", true);
	if (qrc != 0x7fffffff)
	{
		if (qrc <= def)	//don't use it if we're running a mod with a default.cfg that is in a stronger path than fte.cfg, as this indicates that fte.cfg is from fte/ and not $currentmod/.
			Cbuf_AddText ("exec fte.cfg\n", RESTRICT_LOCAL);
		else
			Cbuf_AddText ("echo skipping fte.cfg from wrong gamedir\n", RESTRICT_LOCAL);
	}
#endif
#ifdef QUAKESPYAPI
	if (COM_FCheckExists ("frontend.cfg"))
		Cbuf_AddText ("exec frontend.cfg\n", RESTRICT_LOCAL);
#endif
	Cbuf_AddText ("cl_warncmd 1\n", RESTRICT_LOCAL);	//and then it's allowed to start moaning.
	COM_ParsePlusSets(true);

	com_parseutf8.ival = com_parseutf8.value;

	//if the renderer is already up and running, be prepared to reload content to match the new conback/font/etc
	if (qrenderer != QR_NONE)
		Cbuf_AddText ("vid_reload\n", RESTRICT_LOCAL);
//	if (Key_Dest_Has(kdm_menu))
//		Cbuf_AddText ("closemenu\ntogglemenu\n", RESTRICT_LOCAL);	//make sure the menu has the right content loaded.

	Cbuf_Execute ();	//if the server initialisation causes a problem, give it a place to abort to

	//assuming they didn't use any waits in their config (fools)
	//the configs should be fully loaded.
	//so convert the backwards compable commandline parameters in cvar sets.
	CL_ArgumentOverrides();
#ifndef CLIENTONLY
	SV_ArgumentOverrides();
#endif

	//and disable the 'you have unsaved stuff' prompt.
	Cvar_Saved();
}



void Host_FinishLoading(void)
{
	extern int	r_blockvidrestart;
	if (r_blockvidrestart == true)
	{
		//1 means we need to init the filesystem

		//the filesystem has retrieved its manifest, but might still be waiting for paks to finish downloading.

		//make sure the filesystem has some default if no manifest was loaded.
		FS_ChangeGame(NULL, true, true);

		if (waitingformanifest)
			return;

		Con_History_Load();

		Cmd_StuffCmds();
		Cbuf_Execute ();

		CL_ArgumentOverrides();
	#ifndef CLIENTONLY
		SV_ArgumentOverrides();
	#endif

		Con_Printf ("\n%s\n", version_string());

		Con_DPrintf("This program is free software; you can redistribute it and/or "
					"modify it under the terms of the GNU General Public License "
					"as published by the Free Software Foundation; either version 2 "
					"of the License, or (at your option) any later version."
					"\n"
					"This program is distributed in the hope that it will be useful, "
					"but WITHOUT ANY WARRANTY; without even the implied warranty of "
					"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. "
					"\n"
					"See the GNU General Public License for more details.\n");

	#if defined(_WIN32) && !defined(FTE_SDL) && !defined(_XBOX) && defined(WEBCLIENT)
		if (Sys_RunInstaller())
			Sys_Quit();
	#endif

		r_blockvidrestart = 2;

		Menu_Download_Update();

		IPLog_Merge_File("iplog.txt");
		IPLog_Merge_File("iplog.dat");	//legacy crap, for compat with proquake
	}

	if (PM_IsApplying(true))
		return;

#ifdef ANDROID
	//android needs to wait a bit longer before it's allowed to init its video properly.
	extern int sys_glesversion;
	if (!sys_glesversion)
		return;
#endif

	if (r_blockvidrestart == 2)
	{	//2 is part of the initial startup
		Renderer_Start();
		CL_StartCinematicOrMenu();
	}
	else	//3 flags for a renderer restart
		Renderer_Start();
}

/*
====================
Host_Init
====================
*/
void Host_Init (quakeparms_t *parms)
{
	char engineupdated[MAX_OSPATH];
	int man;

	com_parseutf8.ival = 1;	//enable utf8 parsing even before cvars are registered.

	COM_InitArgv (parms->argc, parms->argv);

	if (setjmp (host_abort) )
		Sys_Error("Host_Init: An error occured. Try the -condebug commandline parameter\n");


	host_parms = *parms;

	Cvar_Init();
	Memory_Init ();

	/*memory is working, its safe to printf*/
	Con_Init ();

	Sys_Init();

	COM_ParsePlusSets(false);
	Cbuf_Init ();
	Cmd_Init ();
	COM_Init ();

	//we have enough of the filesystem inited now that we can read the package list and figure out which engine was last installed.
	if (PM_FindUpdatedEngine(engineupdated, sizeof(engineupdated)))
	{
		PM_Shutdown();	//will restart later as needed, but we need to be sure that no files are open or anything.
		if (Sys_EngineWasUpdated(engineupdated))
		{
			COM_Shutdown();
			Cmd_Shutdown();
			Sys_Shutdown();
			Con_Shutdown();
			Memory_DeInit();
			Cvar_Shutdown();
			Sys_Quit();
			return;
		}
	}
	V_Init ();
	NET_Init ();

#ifdef PLUGINS
	Plug_Initialise(false);
#endif

#ifdef Q2BSPS
	CM_Init();
#endif
#ifdef TERRAIN
	Terr_Init();
#endif
	Host_FixupModelNames();

	Netchan_Init ();
	Renderer_Init();
	Mod_Init(true);

#if defined(CSQC_DAT) || defined(MENU_DAT)
	PF_Common_RegisterCvars();
#endif
#ifndef CLIENTONLY
	SV_Init(parms);
#endif

//	W_LoadWadFile ("gfx.wad");
	Key_Init ();
	M_Init ();
	IN_Init ();
	S_Init ();
	cls.state = ca_disconnected;
	CDAudio_Init ();
	Sbar_Init ();
	CL_Init ();

#ifdef TEXTEDITOR
	Editor_Init();
#endif

#ifdef PLUGINS
	Plug_Initialise(true);
#endif
#ifdef VM_UI
	UI_Init();
#endif

#ifdef CL_MASTER
	Master_SetupSockets();
#endif

#ifdef Q3CLIENT
	CL_ReadCDKey();
#endif

	//	Con_Printf ("Exe: "__TIME__" "__DATE__"\n");
	//Con_Printf ("%4.1f megs RAM available.\n", parms->memsize/ (1024*1024.0));

	R_SetRenderer(NULL);//set the renderer stuff to unset...

	Cvar_ParseWatches();
	host_initialized = true;
	forcesaveprompt = false;

	Sys_SendKeyEvents();

	//the engine is fully running, except the file system may be nulled out waiting for a manifest to download.

	man = COM_CheckParm("-manifest");
	if (man && man < com_argc-1 && com_argv[man+1])
		Host_RunFile(com_argv[man+1], strlen(com_argv[man+1]), NULL);
}

/*
===============
Host_Shutdown

FIXME: this is a callback from Sys_Quit and Sys_Error.  It would be better
to run quit through here before the final handoff to the sys code.
===============
*/
void Host_Shutdown(void)
{
	if (!host_initialized)
		return;
	host_initialized = false;

#ifdef WEBCLIENT
	HTTP_CL_Terminate();
#endif

	//disconnect server/client/etc
	CL_Disconnect_f();

	M_Shutdown(true);

#ifdef CSQC_DAT
	CSQC_Shutdown();
#endif

#ifdef VM_UI
	UI_Stop();
#endif

	S_Shutdown(true);
	CDAudio_Shutdown ();
	IN_Shutdown ();
	R_ShutdownRenderer(true);

#ifdef PLUGINS
	Plug_Shutdown(false);
#endif

//	Host_WriteConfiguration ();
#ifdef CL_MASTER
	MasterInfo_Shutdown();
#endif
	CL_FreeDlights();
	CL_FreeVisEdicts();
	Mod_Shutdown(true);
	Wads_Flush();
	Con_History_Save();	//do this outside of the console code so that the filesystem is still running at this point but still allowing the filesystem to make console prints (you might not see them, but they should be visible to sys_printf still, for debugging).
#ifndef CLIENTONLY
	SV_Shutdown();
#else
	Log_ShutDown();
	NET_Shutdown ();
	FS_Shutdown();
#endif
#ifdef QUAKEHUD
	Stats_Clear();
#endif
#ifdef Q3CLIENT
	VMQ3_FlushStringHandles();
#endif

	COM_DestroyWorkerThread();

	P_ShutdownParticleSystem();
	Cvar_Shutdown();
	Validation_FlushFileList();

	Cmd_Shutdown();
	PM_Shutdown();
	Key_Unbindall_f();

#ifdef PLUGINS
	Plug_Shutdown(true);
#endif

	Con_Shutdown();
	COM_BiDi_Shutdown();
	Memory_DeInit();

#ifndef CLIENTONLY
	SV_WipeServerState();
	memset(&svs, 0, sizeof(svs));
#endif
	Sys_Shutdown();
}

#ifdef CLIENTONLY
void SV_EndRedirect (void)
{
}
#endif
