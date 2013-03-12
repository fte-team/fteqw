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
void CL_Sbar_Callback(struct cvar_s *var, char *oldvalue);
void Name_Callback(struct cvar_s *var, char *oldvalue);

// we need to declare some mouse variables here, because the menu system
// references them even when on a unix system.

qboolean	noclip_anglehack;		// remnant from old quake


cvar_t	rcon_password = SCVARF("rcon_password", "", CVAR_NOUNSAFEEXPAND);

cvar_t	rcon_address = SCVARF("rcon_address", "", CVAR_NOUNSAFEEXPAND);

cvar_t	cl_timeout = SCVAR("cl_timeout", "60");

cvar_t	cl_shownet = CVARD("cl_shownet","0", "Debugging var. 0 shows nothing. 1 shows incoming packet sizes. 2 shows individual messages. 3 shows entities too.");	// can be 0, 1, or 2

cvar_t	cl_pure		= CVARD("cl_pure", "0", "If enabled, the filesystem will be restricted to allow only the content of the current server.");
cvar_t	cl_sbar		= CVARFC("cl_sbar", "0", CVAR_ARCHIVE, CL_Sbar_Callback);
cvar_t	cl_hudswap	= CVARF("cl_hudswap", "0", CVAR_ARCHIVE);
cvar_t	cl_maxfps	= CVARF("cl_maxfps", "500", CVAR_ARCHIVE);
cvar_t	cl_idlefps	= CVARFD("cl_idlefps", "0", CVAR_ARCHIVE, "This is the maximum framerate to attain while idle/paused.");
cvar_t	cl_yieldcpu = CVARFD("cl_yieldcpu", "0", CVAR_ARCHIVE, "Attempt to yield between frames. This can resolve issues with certain drivers and background software, but can mean less consistant frame times. Will reduce power consumption/heat generation so should be set on laptops or similar (over-hot/battery powered) devices.");
cvar_t	cl_nopext	= CVARF("cl_nopext", "0", CVAR_ARCHIVE);
cvar_t	cl_pext_mask = CVAR("cl_pext_mask", "0xffffffff");
cvar_t	cl_nolerp	= CVARD("cl_nolerp", "0", "Disables interpolation. If set, missiles/monsters will be smoother, but they may be more laggy. Does not affect players. A value of 2 means 'interpolate only in single-player/coop'.");
cvar_t	cl_nolerp_netquake = CVARD("cl_nolerp_netquake", "0", "Disables interpolation when connected to an NQ server. Does affect players, even the local player. You probably don't want to set this.");
cvar_t	hud_tracking_show = CVAR("hud_tracking_show", "1");

cvar_t	cl_defaultport		= CVARAFD("cl_defaultport", STRINGIFY(PORT_QWSERVER), "port", 0, "The default port to connect to servers.\nQW: "STRINGIFY(PORT_QWSERVER)", NQ: "STRINGIFY(PORT_NQSERVER)", Q2: "STRINGIFY(PORT_Q2SERVER)".");

cvar_t	cfg_save_name = CVARF("cfg_save_name", "fte", CVAR_ARCHIVE|CVAR_NOTFROMSERVER);

cvar_t	cl_splitscreen = CVARD("cl_splitscreen", "0", "Enables splitscreen support. See also: allow_splitscreen, in_rawinput*, the \"p\" command.");

cvar_t	lookspring = CVARF("lookspring","0", CVAR_ARCHIVE);
cvar_t	lookstrafe = CVARF("lookstrafe","0", CVAR_ARCHIVE);
cvar_t	sensitivity = CVARF("sensitivity","10", CVAR_ARCHIVE);

cvar_t cl_staticsounds = CVAR("cl_staticsounds", "1");

cvar_t	m_pitch = CVARF("m_pitch","0.022", CVAR_ARCHIVE);
cvar_t	m_yaw = CVARF("m_yaw","0.022", CVAR_ARCHIVE);
cvar_t	m_forward = CVARF("m_forward","1", CVAR_ARCHIVE);
cvar_t	m_side = CVARF("m_side","0.8", CVAR_ARCHIVE);

cvar_t	cl_lerp_players = CVARD("cl_lerp_players", "0", "Set this to make other players smoother, though it may increase effective latency. Affects only QuakeWorld.");
cvar_t	cl_predict_players = CVARD("cl_predict_players", "1", "Clear this cvar to see ents exactly how they are on the server.");
cvar_t	cl_predict_players_frac = CVARD("cl_predict_players_frac", "0.9", "How much of other players to predict. Values less than 1 will help minimize overruns.");
cvar_t	cl_solid_players = CVARD("cl_solid_players", "1", "Consider other players as solid for player prediction.");
cvar_t	cl_noblink = CVARD("cl_noblink", "0", "Disable the ^^b text blinking feature.");
cvar_t	cl_servername = CVARD("cl_servername", "none", "The hostname of the last server you connected to");
cvar_t	cl_serveraddress = CVARD("cl_serveraddress", "none", "The address of the last server you connected to");
cvar_t	qtvcl_forceversion1 = CVAR("qtvcl_forceversion1", "0");
cvar_t	qtvcl_eztvextensions = CVAR("qtvcl_eztvextensions", "0");

cvar_t cl_demospeed = CVARAF("cl_demospeed", "1", "demo_setspeed", 0);

cvar_t cl_loopbackprotocol = CVARD("cl_loopbackprotocol", "qw", "Which protocol to use for single-player/the internal client. Should be one of: qw, nqid, nq, fitz, dp6, dp7.");


cvar_t	cl_threadedphysics = CVAR("cl_threadedphysics", "0");

cvar_t  localid = SCVAR("localid", "");

cvar_t	r_drawflame = CVARD("r_drawflame", "1", "Set to -1 to disable ALL static entities. Set to 0 to disable only wall torches and standing flame. Set to 1 for everything drawn as normal.");

qboolean forcesaveprompt;
static qboolean allowremotecmd = true;

extern int			total_loading_size, current_loading_size, loading_stage;

//
// info mirrors
//
cvar_t	password = CVARAF("password",		"",	"pq_password", CVAR_USERINFO | CVAR_NOUNSAFEEXPAND); //this is parhaps slightly dodgy... added pq_password alias because baker seems to be using this for user accounts.
cvar_t	spectator = CVARF("spectator",		"",			CVAR_USERINFO);
cvar_t	name = CVARFC("name",				"unnamed",	CVAR_ARCHIVE | CVAR_USERINFO, Name_Callback);
cvar_t	team = CVARF("team",				"",			CVAR_ARCHIVE | CVAR_USERINFO);
cvar_t	skin = CVARF("skin",				"",			CVAR_ARCHIVE | CVAR_USERINFO);
cvar_t	model = CVARF("model",				"",			CVAR_ARCHIVE | CVAR_USERINFO);
cvar_t	topcolor = CVARF("topcolor",		"",			CVAR_ARCHIVE | CVAR_USERINFO);
cvar_t	bottomcolor = CVARF("bottomcolor",	"",			CVAR_ARCHIVE | CVAR_USERINFO);
cvar_t	rate = CVARFD("rate",				"10000"/*"6480"*/,		CVAR_ARCHIVE | CVAR_USERINFO, "A rough measure of the bandwidth to try to use while playing. Too high a value may result in 'buffer bloat'.");
cvar_t	drate = CVARFD("drate",				"100000",	CVAR_ARCHIVE | CVAR_USERINFO, "A rough measure of the bandwidth to try to use while downloading.");		// :)
cvar_t	noaim = CVARF("noaim",				"",			CVAR_ARCHIVE | CVAR_USERINFO);
cvar_t	msg = CVARFD("msg",					"1",		CVAR_ARCHIVE | CVAR_USERINFO, "Filter console prints/messages. Only functions on QuakeWorld servers. 0=pickup messages. 1=death messages. 2=critical messages. 3=chat.");
cvar_t	b_switch = CVARF("b_switch",		"",			CVAR_ARCHIVE | CVAR_USERINFO);
cvar_t	w_switch = CVARF("w_switch",		"",			CVAR_ARCHIVE | CVAR_USERINFO);
cvar_t	cl_nofake = CVARD("cl_nofake",		"2", "value 0: permits \\r chars in chat messages\nvalue 1: blocks all \\r chars\nvalue 2: allows \\r chars, but only from teammates");
cvar_t	cl_chatsound = CVAR("cl_chatsound","1");
cvar_t	cl_enemychatsound = CVAR("cl_enemychatsound", "misc/talk.wav");
cvar_t	cl_teamchatsound = CVAR("cl_teamchatsound", "misc/talk.wav");

cvar_t	r_torch			= CVARF("r_torch",	"0",	CVAR_CHEAT);
cvar_t	r_rocketlight	= CVARFC("r_rocketlight",	"1", CVAR_ARCHIVE, Cvar_Limiter_ZeroToOne_Callback);
cvar_t	r_lightflicker	= CVAR("r_lightflicker",	"1");
cvar_t	cl_r2g			= CVARF("cl_r2g",	"0", CVAR_ARCHIVE);
cvar_t	r_powerupglow	= CVAR("r_powerupglow", "1");
cvar_t	v_powerupshell	= CVARF("v_powerupshell", "0", CVAR_ARCHIVE);
cvar_t	cl_gibfilter	= CVARF("cl_gibfilter", "0", CVAR_ARCHIVE);
cvar_t	cl_deadbodyfilter	= CVAR("cl_deadbodyfilter", "0");

cvar_t  cl_gunx = SCVAR("cl_gunx", "0");
cvar_t  cl_guny = SCVAR("cl_guny", "0");
cvar_t  cl_gunz = SCVAR("cl_gunz", "0");

cvar_t  cl_gunanglex = SCVAR("cl_gunanglex", "0");
cvar_t  cl_gunangley = SCVAR("cl_gunangley", "0");
cvar_t  cl_gunanglez = SCVAR("cl_gunanglez", "0");

cvar_t	cl_download_csprogs = CVARFD("cl_download_csprogs", "1", CVAR_NOTFROMSERVER, "Download updated client gamecode if available.");
cvar_t	cl_download_redirection = CVARFD("cl_download_redirection", "2", CVAR_NOTFROMSERVER, "Follow download redirection to download packages instead of individual files. 2 allows redirection only to named packages files.");
cvar_t  cl_download_mapsrc = CVARD("cl_download_mapsrc", "", "Specifies an http location prefix for map downloads. EG: \"http://bigfoot.morphos-team.net/misc/quakemaps/\"");
cvar_t	cl_download_packages = CVARFD("cl_download_packages", "1", CVAR_NOTFROMSERVER, "0=Do not download packages simply because the server is using them. 1=Download and load packages as needed (does not affect games which do not use this package). 2=Do download and install permanently (use with caution!)");
cvar_t	requiredownloads = CVARFD("requiredownloads","1", CVAR_ARCHIVE, "0=join the game before downloads have even finished (might be laggy). 1=wait for all downloads to complete before joining.");

cvar_t	cl_muzzleflash = SCVAR("cl_muzzleflash", "1");

cvar_t	cl_item_bobbing = CVARF("cl_model_bobbing", "0", CVAR_ARCHIVE);
cvar_t	cl_countpendingpl = SCVAR("cl_countpendingpl", "0");

cvar_t	cl_standardchat = CVARFD("cl_standardchat", "0", CVAR_ARCHIVE, "Disables auto colour coding in chat messages.");
cvar_t	msg_filter = CVARD("msg_filter", "0", "Filter out chat messages: 0=neither. 1=broadcast chat. 2=team chat. 3=all chat.");
cvar_t  cl_standardmsg = CVARFD("cl_standardmsg", "0", CVAR_ARCHIVE, "Disables auto colour coding in console prints.");
cvar_t  cl_parsewhitetext = CVARD("cl_parsewhitetext", "1", "When parsing chat messages, enable support for messages like: red{white}red");

cvar_t	cl_dlemptyterminate = CVAR("cl_dlemptyterminate", "1");

cvar_t	host_mapname = CVARAF("mapname", "",
							  "host_mapname", 0);

cvar_t	ruleset_allow_playercount	= SCVAR("ruleset_allow_playercount", "1");
cvar_t	ruleset_allow_frj		= SCVAR("ruleset_allow_frj", "1");
cvar_t	ruleset_allow_semicheats		= SCVAR("ruleset_allow_semicheats", "1");
cvar_t	ruleset_allow_packet		= SCVAR("ruleset_allow_packet", "1");
cvar_t	ruleset_allow_particle_lightning	= SCVAR("ruleset_allow_particle_lightning", "1");
cvar_t	ruleset_allow_overlongsounds	= SCVAR("ruleset_allow_overlong_sounds", "1");
cvar_t	ruleset_allow_larger_models	= SCVAR("ruleset_allow_larger_models", "1");
cvar_t	ruleset_allow_modified_eyes = SCVAR("ruleset_allow_modified_eyes", "0");
cvar_t	ruleset_allow_sensative_texture_replacements = SCVAR("ruleset_allow_sensative_texture_replacements", "1");
cvar_t	ruleset_allow_localvolume	= SCVAR("ruleset_allow_localvolume", "1");
cvar_t  ruleset_allow_shaders	= SCVARF("ruleset_allow_shaders", "1", CVAR_SHADERSYSTEM);
cvar_t  ruleset_allow_fbmodels	= SCVARF("ruleset_allow_fbmodels", "1", CVAR_SHADERSYSTEM);

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
//lightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
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

scenetris_t		*cl_stris;
vecV_t			*cl_strisvertv;
vec4_t			*cl_strisvertc;
vec2_t			*cl_strisvertt;
index_t			*cl_strisidx;
unsigned int cl_numstrisidx;
unsigned int cl_maxstrisidx;
unsigned int cl_numstrisvert;
unsigned int cl_maxstrisvert;
unsigned int cl_numstris;
unsigned int cl_maxstris;

double			connect_time = -1;		// for connection retransmits
int				connect_defaultport = 0;
int				connect_tries = 0;	//increased each try, every fourth trys nq connect packets.

quakeparms_t host_parms;

qboolean	host_initialized;		// true if into command execution
qboolean	nomaster;

double		host_frametime;
double		realtime;				// without any filtering or bounding
double		oldrealtime;			// last frame run
int			host_framecount;

int			host_hunklevel;

qbyte		*host_basepal;
qbyte		*h2playertranslations;

cvar_t	host_speeds = SCVAR("host_speeds","0");		// set for running times
#ifdef CRAZYDEBUGGING
cvar_t	developer = SCVAR("developer","1");
#else
cvar_t	developer = SCVAR("developer","0");
#endif

int			fps_count;
qboolean	forcesaveprompt;

jmp_buf 	host_abort;

void Master_Connect_f (void);

float	server_version = 0;	// version of server we connected to

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
		switch (cls.state)
		{
		default:
#ifndef CLIENTONLY
			if (sv.state)
				VID_SetWindowCaption(va("%s %s: %s", DISTRIBUTION, fs_gamename.string, sv.name));
			else
#endif
				VID_SetWindowCaption(va("%s %s: %s", DISTRIBUTION, fs_gamename.string, cls.servername));
			break;
		case ca_disconnected:
			VID_SetWindowCaption(va("%s %s: disconnected", DISTRIBUTION, fs_gamename.string));
			break;
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
	cls.state = ca_active;
	S_Purge(true);
	CL_UpdateWindowTitle();

	SCR_EndLoadingPlaque();

	TP_ExecTrigger("f_spawn");
}
/*
==================
CL_Quit_f
==================
*/
void CL_Quit_f (void)
{
	if (forcesaveprompt)
	{
		forcesaveprompt =false;
		if (Cmd_Exists("menu_quit"))
		{
			Cmd_ExecuteString("menu_quit", RESTRICT_LOCAL);
			return;
		}
	}

	TP_ExecTrigger("f_quit");
	Cbuf_Execute();
/*
#ifndef CLIENTONLY
	if (!isDedicated)
#endif
	{
		M_Menu_Quit_f ();
		return;
	}*/
	CL_Disconnect ();
	Sys_Quit ();
}

void CL_ConnectToDarkPlaces(char *challenge, netadr_t adr)
{
	char	data[2048];
	cls.fteprotocolextensions = 0;
	cls.fteprotocolextensions2 = 0;

	cls.resendinfo = false;

	connect_time = realtime;	// for retransmit requests

	Q_snprintfz(data, sizeof(data), "%c%c%c%cconnect\\protocol\\darkplaces 3\\challenge\\%s", 255, 255, 255, 255, challenge);

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

char *CL_GUIDString(netadr_t adr)
{
	static qbyte buf[2048];
	static int buflen;
	unsigned int digest[4];
	char serveraddr[256];
	void *blocks[2];
	int lens[2];
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

	NET_AdrToString(serveraddr, sizeof(serveraddr), adr);

	blocks[0] = buf;lens[0] = buflen;
	blocks[1] = serveraddr;lens[1] = strlen(serveraddr);
	Com_BlocksChecksum(2, blocks, lens, (void*)digest);

	return va("%08x%08x%08x%08x", digest[0], digest[1], digest[2], digest[3]);
}

/*
=======================
CL_SendConnectPacket

called by CL_Connect_f and CL_CheckResend
======================
*/
void CL_SendConnectPacket (int mtu, 
#ifdef PROTOCOL_VERSION_FTE
						   int ftepext, int ftepext2,
#endif
						   int compressioncrc
						  /*, ...*/)	//dmw new parms
{
	extern cvar_t qport;
	netadr_t	adr;
	char	data[2048];
	char *info;
	double t1, t2;
#ifdef PROTOCOL_VERSION_FTE
	int fteprotextsupported=0;
	int fteprotextsupported2=0;
#endif
	int clients;
	int c;

// JACK: Fixed bug where DNS lookups would cause two connects real fast
//       Now, adds lookup time to the connect time.
//		 Should I add it to realtime instead?!?!

	if (cls.state != ca_disconnected)
		return;

	if (cl_nopext.ival)	//imagine it's an unenhanced server
	{
		compressioncrc = 0;
	}

#ifdef PROTOCOL_VERSION_FTE
	CL_SupportedFTEExtensions(&fteprotextsupported, &fteprotextsupported2);

	fteprotextsupported &= ftepext;
	fteprotextsupported2 &= ftepext2;

#ifdef Q2CLIENT
	if (cls.protocol != CP_QUAKEWORLD)
		fteprotextsupported = 0;
#endif

	cls.fteprotocolextensions = fteprotextsupported;
	cls.fteprotocolextensions2 = fteprotextsupported2;
#endif

	t1 = Sys_DoubleTime ();

	if (!NET_StringToAdr (cls.servername, PORT_QWSERVER, &adr))
	{
		Con_TPrintf (TLC_BADSERVERADDRESS);
		connect_time = -1;
		return;
	}

	NET_AdrToString(data, sizeof(data), adr);
	Cvar_ForceSet(&cl_serveraddress, data);

	if (!NET_IsClientLegal(&adr))
	{
		Con_TPrintf (TLC_ILLEGALSERVERADDRESS);
		connect_time = -1;
		return;
	}

	t2 = Sys_DoubleTime ();

	cls.resendinfo = false;

	connect_time = realtime+t2-t1;	// for retransmit requests

	cls.qport = qport.value;
	Cvar_SetValue(&qport, (cls.qport+1)&0xffff);

//	Info_SetValueForStarKey (cls.userinfo, "*ip", NET_AdrToString(adr), MAX_INFO_STRING);

	clients = 1;
	if (cl_splitscreen.value && (fteprotextsupported & PEXT_SPLITSCREEN))
	{
//		if (adr.type == NA_LOOPBACK)
			clients = cl_splitscreen.value+1;
//		else
//			Con_Printf("Split screens are still under development\n");
	}

	if (clients < 1)
		clients = 1;
	if (clients > MAX_SPLITS)
		clients = MAX_SPLITS;

#ifdef Q2CLIENT
	if (cls.protocol == CP_QUAKE2)	//sorry - too lazy.
		clients = 1;
#endif

#ifdef Q3CLIENT
	if (cls.protocol == CP_QUAKE3)
	{	//q3 requires some very strange things.
		CLQ3_SendConnectPacket(adr);
		return;
	}
#endif

	Q_snprintfz(data, sizeof(data), "%c%c%c%cconnect", 255, 255, 255, 255);

	if (clients>1)	//splitscreen 'connect' command specifies the number of userinfos sent.
		Q_strncatz(data, va("%i", clients), sizeof(data));

#ifdef Q2CLIENT
	if (cls.protocol == CP_QUAKE2)
		Q_strncatz(data, va(" %i", PROTOCOL_VERSION_Q2), sizeof(data));
	else
#endif
		Q_strncatz(data, va(" %i", PROTOCOL_VERSION_QW), sizeof(data));


	Q_strncatz(data, va(" %i %i", cls.qport, cls.challenge), sizeof(data));

	//userinfo 0 + zquake extension info.
	if (cls.protocol == CP_QUAKEWORLD)
		Q_strncatz(data, va(" \"%s\\*z_ext\\%i\"", cls.userinfo[0], SUPPORTED_Z_EXTENSIONS), sizeof(data));
	else
		Q_strncatz(data, va(" \"%s\"", cls.userinfo[0]), sizeof(data));
	for (c = 1; c < clients; c++)
	{
		Q_strncatz(data, va(" \"%s\"", cls.userinfo[c]), sizeof(data));
	}

	Q_strncatz(data, "\n", sizeof(data));

#ifdef PROTOCOL_VERSION_FTE
	if (ftepext)
		Q_strncatz(data, va("0x%x 0x%x\n", PROTOCOL_VERSION_FTE, fteprotextsupported), sizeof(data));
#endif
#ifdef PROTOCOL_VERSION_FTE2
	if (ftepext2)
		Q_strncatz(data, va("0x%x 0x%x\n", PROTOCOL_VERSION_FTE2, fteprotextsupported2), sizeof(data));
#endif

	if (mtu > 0)
	{
		if (adr.type == NA_LOOPBACK)
			mtu = MAX_UDP_PACKET;
		else if (net_mtu.ival > 64 && mtu > net_mtu.ival)
			mtu = net_mtu.ival;
		mtu &= ~7;
		Q_strncatz(data, va("0x%x %i\n", PROTOCOL_VERSION_FRAGMENT, mtu), sizeof(data));
		cls.netchan.fragmentsize = mtu;
	}
	else
		cls.netchan.fragmentsize = 0;

#ifdef HUFFNETWORK
	if (compressioncrc && Huff_CompressionCRC(compressioncrc))
	{
		Q_strncatz(data, va("0x%x 0x%x\n", PROTOCOL_VERSION_HUFFMAN, LittleLong(compressioncrc)), sizeof(data));
		cls.netchan.compress = true;
	}
	else
#endif
		cls.netchan.compress = false;

	info = CL_GUIDString(adr);
	if (info)
		Q_strncatz(data, va("0x%x \"%s\"\n", PROTOCOL_INFO_GUID, info), sizeof(data));

	NET_SendPacket (NS_CLIENT, strlen(data), data, adr);

	cl.splitclients = 0;
}

char *CL_TryingToConnect(void)
{
	if (connect_time == -1)
		return NULL;
	if (cls.state != ca_disconnected)
		return NULL;

	return cls.servername;
}

#ifndef CLIENTONLY
int SV_NewChallenge (void);
client_t *SVC_DirectConnect(void);
#endif

/*
=================
CL_CheckForResend

Resend a connect message if the last one has timed out

=================
*/
void CL_CheckForResend (void)
{
	netadr_t	adr;
	char	data[2048];
	double t1, t2;
	int contype = 0;

#ifndef CLIENTONLY
	if (!cls.state && sv.state)
	{
		Q_strncpyz (cls.servername, "internalserver", sizeof(cls.servername));
		Cvar_ForceSet(&cl_servername, cls.servername);

		cls.state = ca_disconnected;
		switch (svs.gametype)
		{
#ifdef Q3CLIENT
		case GT_QUAKE3:
			cls.protocol = CP_QUAKE3;
			break;
#endif
#ifdef Q2CLIENT
		case GT_QUAKE2:
			cls.protocol = CP_QUAKE2;
			break;
#endif
		default:
			cl.movesequence = 0;
			if (!strcmp(cl_loopbackprotocol.string, "qw"))
				cls.protocol = CP_QUAKEWORLD;
			else if (!strcmp(cl_loopbackprotocol.string, "fitz"))	//actually proquake, because we might as well use the extra angles
			{
				cls.protocol = CP_NETQUAKE;
				cls.protocol_nq = CPNQ_FITZ666;
			}
			else if (!strcmp(cl_loopbackprotocol.string, "nq"))	//actually proquake, because we might as well use the extra angles
			{
				cls.protocol = CP_NETQUAKE;
				cls.protocol_nq = CPNQ_PROQUAKE3_4;
			}
			else if (!strcmp(cl_loopbackprotocol.string, "nqid"))
			{
				cls.protocol = CP_NETQUAKE;
				cls.protocol_nq = CPNQ_ID;
			}
			else if (!strcmp(cl_loopbackprotocol.string, "q3"))
				cls.protocol = CP_QUAKE3;
			else if (!strcmp(cl_loopbackprotocol.string, "dp6"))
			{
				cls.protocol = CP_NETQUAKE;
				cls.protocol_nq = CPNQ_DP7;
			}
			else if (!strcmp(cl_loopbackprotocol.string, "dp7"))
			{
				cls.protocol = CP_NETQUAKE;
				cls.protocol_nq = CPNQ_DP7;
			}
			else if (progstype == PROG_QW)
				cls.protocol = CP_QUAKEWORLD;
			else
				cls.protocol = CP_NETQUAKE;
			break;
		}

		CL_FlushClientCommands();	//clear away all client->server clientcommands.

		if (cls.protocol == CP_NETQUAKE)
		{
			if (!NET_StringToAdr (cls.servername, connect_defaultport, &adr))
			{
				Con_TPrintf (TLC_BADSERVERADDRESS);
				connect_time = -1;
				SCR_EndLoadingPlaque();
				return;
			}
			NET_AdrToString(data, sizeof(data), adr);

			/*eat up the server's packets, to clear any lingering loopback packets*/
			while(NET_GetPacket (NS_SERVER, 0) >= 0)
			{
			}
			net_message.packing = SZ_RAWBYTES;
			net_message.cursize = 0;

			if (cls.protocol_nq == CPNQ_ID)
			{
				net_from = adr;
				Cmd_TokenizeString (va("connect %i %i %i \"\\name\\unconnected\"", NET_PROTOCOL_VERSION, 0, SV_NewChallenge()), false, false);

				SVC_DirectConnect();
			}
			else if (cls.protocol_nq == CPNQ_FITZ666)
			{
				net_from = adr;
				Cmd_TokenizeString (va("connect %i %i %i \"\\name\\unconnected\\mod\\666\"", NET_PROTOCOL_VERSION, 0, SV_NewChallenge()), false, false);

				SVC_DirectConnect();
			}
			else if (cls.protocol_nq == CPNQ_PROQUAKE3_4)
			{
				net_from = adr;
				Cmd_TokenizeString (va("connect %i %i %i \"\\name\\unconnected\\mod\\1\"", NET_PROTOCOL_VERSION, 0, SV_NewChallenge()), false, false);

				SVC_DirectConnect();
			}
			else
				CL_ConnectToDarkPlaces("", adr);
		}
		else
			CL_SendConnectPacket (8192-16, Net_PextMask(1, false), Net_PextMask(2, false), false);
		return;
	}
#endif

	if (connect_time == -1)
		return;
	if (cls.state != ca_disconnected)
		return;
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
	if (connect_time && realtime - connect_time < 5.0)
		return;

	t1 = Sys_DoubleTime ();
	if (!NET_StringToAdr (cls.servername, connect_defaultport, &adr))
	{
		Con_TPrintf (TLC_BADSERVERADDRESS);
		connect_time = -1;
		SCR_EndLoadingPlaque();
		return;
	}
	if (!NET_IsClientLegal(&adr))
	{
		Con_TPrintf (TLC_ILLEGALSERVERADDRESS);
		SCR_EndLoadingPlaque();
		connect_time = -1;
		return;
	}

	t2 = Sys_DoubleTime ();

	connect_time = realtime+t2-t1;	// for retransmit requests

	Cvar_ForceSet(&cl_servername, cls.servername);

#ifdef Q3CLIENT
	//Q3 clients send their cdkey to the q3 authorize server.
	//they send this packet with the challenge.
	//and the server will refuse the client if it hasn't sent it.
	CLQ3_SendAuthPacket(adr);
#endif

	Con_TPrintf (TLC_CONNECTINGTO, cls.servername);

	if (connect_tries == 0)
		if (!NET_EnsureRoute(cls.sockets, "conn", cls.servername, false))
		{
			Con_Printf ("Unable to establish connection to %s\n", cls.servername);
			return;
		}

	contype |= 1; /*always try qw type connections*/
//	if ((connect_tries&3)==3) || (connect_defaultport==26000))
		contype |= 2; /*try nq connections periodically (or if its the default nq port)*/

	/*DP, QW, Q2, Q3*/
	if (contype & 1)
	{
		Q_snprintfz (data, sizeof(data), "%c%c%c%cgetchallenge\n", 255, 255, 255, 255);
		NET_SendPacket (NS_CLIENT, strlen(data), data, adr);
	}
	/*NQ*/
#ifdef NQPROT
	if (contype & 2)
	{
		sizebuf_t sb;
		memset(&sb, 0, sizeof(sb));
		sb.data = data;
		sb.maxsize = sizeof(data);

		MSG_WriteLong(&sb, LongSwap(NETFLAG_CTL | (strlen(NET_GAMENAME_NQ)+7)));
		MSG_WriteByte(&sb, CCREQ_CONNECT);
		MSG_WriteString(&sb, NET_GAMENAME_NQ);
		MSG_WriteByte(&sb, NET_PROTOCOL_VERSION);

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
		MSG_WriteString(&sb, "getchallenge");

		*(int*)sb.data = LongSwap(NETFLAG_CTL | sb.cursize);
		NET_SendPacket (NS_CLIENT, sb.cursize, sb.data, adr);
	}
#endif

	connect_tries++;
}

void CL_BeginServerConnect(int port)
{
	if (!port)
		port = cl_defaultport.value;
	SCR_SetLoadingStage(LS_CONNECTION);
	connect_time = 0;
	connect_defaultport = port;
	connect_tries = 0;
	CL_CheckForResend();
}

void CL_BeginServerReconnect(void)
{
#ifndef CLIENTONLY
	if (isDedicated)
	{
		Con_TPrintf (TLC_DEDICATEDCANNOTCONNECT);
		return;
	}
#endif
	connect_time = 0;
	CL_CheckForResend();
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
		Con_TPrintf (TLC_SYNTAX_CONNECT);
		return;
	}

	server = Cmd_Argv (1);

	CL_Disconnect_f ();

	Q_strncpyz (cls.servername, server, sizeof(cls.servername));
	CL_BeginServerConnect(0);
}

void CL_Join_f (void)
{
	char	*server;

	if (Cmd_Argc() != 2)
	{
		if (cls.state)
		{	//Hmm. This server sucks.
			if (cls.z_ext & Z_EXT_JOIN_OBSERVE)
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

	Q_strncpyz (cls.servername, server, sizeof(cls.servername));
	CL_BeginServerConnect(0);
}

void CL_Observe_f (void)
{
	char	*server;

	if (Cmd_Argc() != 2)
	{
		if (cls.state)
		{	//Hmm. This server sucks.
			if (cls.z_ext & Z_EXT_JOIN_OBSERVE)
				Cmd_ForwardToServer();
			else
				Cbuf_AddText("\nspectator 1;reconnect\n", RESTRICT_LOCAL);
			return;
		}
		Con_Printf ("observe requires a connection or servername/ip\n");
		return;
	}

	server = Cmd_Argv (1);

	CL_Disconnect_f ();

	Cvar_Set(&spectator, "1");

	Q_strncpyz (cls.servername, server, sizeof(cls.servername));
	CL_BeginServerConnect(0);
}

#ifdef NQPROT
void CLNQ_Connect_f (void)
{
	char	*server;

	if (Cmd_Argc() != 2)
	{
		Con_TPrintf (TLC_SYNTAX_CONNECT);
		return;
	}

	server = Cmd_Argv (1);

	CL_Disconnect_f ();

	Q_strncpyz (cls.servername, server, sizeof(cls.servername));
	CL_BeginServerConnect(26000);
}
#endif

#ifdef IRCCONNECT
void CL_IRCConnect_f (void)
{
	CL_Disconnect_f ();

	if (FTENET_AddToCollection(cls.sockets, "TCP", Cmd_Argv(2), FTENET_IRCConnect_EstablishConnection, false))
	{
		char *server;
		server = Cmd_Argv (1);

		strcpy(cls.servername, "irc://");
		Q_strncpyz (cls.servername+6, server, sizeof(cls.servername)-6);
		CL_BeginServerConnect(0);
	}
}
#endif

#ifdef TCPCONNECT
void CL_TCPConnect_f (void)
{
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
			Con_TPrintf (TLC_NORCONPASSWORD);
			Con_Printf("usage: rcon (password) <command>\n");
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
			Con_TPrintf (TLC_NORCONDEST);

			return;
		}
		NET_StringToAdr (rcon_address.string, PORT_QWSERVER, &to);
	}

	NET_SendPacket (NS_CLIENT, strlen(message)+1, message
		, to);
}


/*
=====================
CL_ClearState

=====================
*/
void CL_ClearState (void)
{
	int			i;
#ifndef CLIENTONLY
#define serverrunning (sv.state != ss_dead)
#define tolocalserver NET_IsLoopBackAddress(cls.netchan.remote_address)
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
		if (serverrunning)
			SV_UnspawnServer();
		Mod_ClearAll ();

		if (host_hunklevel)	// FIXME: check this...
			Hunk_FreeToLowMark (host_hunklevel);

		Cvar_ApplyLatches(CVAR_LATCH);
	}

	CL_ClearParseState();
	CL_ClearTEnts();
	CL_ClearCustomTEnts();
	Surf_ClearLightmaps();
	T_FreeInfoStrings();
	SCR_ShowPic_Clear();

	if (cl.playernum[0] == -1)
	{	//left over from q2 connect.
		Media_PlayFilm("");
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

// wipe the entire cl structure
	memset (&cl, 0, sizeof(cl));

	cl.fog_density = 0;
	cl.fog_colour[0] = 0.3;
	cl.fog_colour[1] = 0.3;
	cl.fog_colour[2] = 0.3;

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
		cl_lightstyle[i].colour = 7;

	rtlights_first = rtlights_max = RTL_FIRST;

	for (i = 0; i < MAX_SPLITS; i++)
	{
		VectorSet(cl.playerview[i].gravitydir, 0, 0, -1);
		cl.viewheight[i] = DEFAULT_VIEWHEIGHT;
	}
	cl.minpitch = -70;
	cl.maxpitch = 80;

	cl.oldgametime = 0;
	cl.gametime = 0;
	cl.gametimemark = 0;
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

	connect_time = -1;
	connect_tries = 0;

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
		if (serverrunning && !tolocalserver)
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
	cl.spectator = 0;
	cl.sendprespawn = false;
	cl.intermission = 0;
	cl.oldgametime = 0;

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
	FTENET_AddToCollection(cls.sockets, "TCP", NULL, NULL, false);
#endif

	Cvar_ForceSet(&cl_servername, "none");

	CL_ClearState();

	//now start up the csqc/menu module again.
	CSQC_UnconnectedInit();
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

	Alias_WipeStuffedAliaes();
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
		Con_TPrintf (TLC_SYNTAX_USER);
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
			if (cls.protocol == CP_NETQUAKE)
				Con_Printf("name: %s\ncolour %i %i\nping: %i\n", cl.players[i].name, cl.players[i].rbottomcolor, cl.players[i].rtopcolor, cl.players[i].ping);
			else
				Info_Print (cl.players[i].userinfo);
			found = true;
		}
	}
	if (!found)
		Con_TPrintf (TLC_USER_NOUSER);
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
	Con_TPrintf (TLC_USERBANNER);
	Con_TPrintf (TLC_USERBANNER2);
	for (i=0 ; i<MAX_CLIENTS ; i++)
	{
		if (cl.players[i].name[0])
		{
			Con_TPrintf (TLC_USERLINE, cl.players[i].userid, cl.players[i].frags, cl.players[i].name);
			c++;
		}
	}

	Con_TPrintf (TLC_USERTOTAL, c);
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

	if (Cmd_Argc() == 1)
	{
		Con_TPrintf (TLC_COLOURCURRENT,
			Info_ValueForKey (cls.userinfo[pnum], "topcolor"),
			Info_ValueForKey (cls.userinfo[pnum], "bottomcolor") );
		Con_TPrintf (TLC_SYNTAX_COLOUR);
		return;
	}

	if (Cmd_FromGamecode())
		server_owns_colour = true;
	else
		server_owns_colour = false;


	if (Cmd_Argc() == 2)
		top = bottom = CL_ParseColour(Cmd_Argv(1));
	else
	{
		top = CL_ParseColour(Cmd_Argv(1));
		bottom = CL_ParseColour(Cmd_Argv(2));
	}

	Q_snprintfz (num, sizeof(num), (top&0xff000000)?"%#08x":"%i", top & 0xffffff);
	if (top == 0)
		*num = '\0';
	if (Cmd_ExecLevel>RESTRICT_SERVER) //colour command came from server for a split client
		Cbuf_AddText(va("cmd %i setinfo topcolor \"%s\"\n", Cmd_ExecLevel-RESTRICT_SERVER-1, num), Cmd_ExecLevel);
//	else if (server_owns_colour)
//		Cvar_LockFromServer(&topcolor, num);
	else
		Cvar_Set (&topcolor, num);
	Q_snprintfz (num, sizeof(num), (bottom&0xff000000)?"%#08x":"%i", bottom & 0xffffff);
	if (bottom == 0)
		*num = '\0';
	if (Cmd_ExecLevel>RESTRICT_SERVER) //colour command came from server for a split client
		Cbuf_AddText(va("cmd %i setinfo bottomcolor \"%s\"\n", Cmd_ExecLevel-RESTRICT_SERVER-1, num), Cmd_ExecLevel);
	else if (server_owns_colour)
		Cvar_LockFromServer(&bottomcolor, num);
	else
		Cvar_Set (&bottomcolor, num);
#ifdef NQPROT
	if (cls.protocol == CP_NETQUAKE)
		Cmd_ForwardToServer();
#endif
}

void FS_GenCachedPakName(char *pname, char *crc, char *local, int llen);
qboolean CL_CheckDLFile(char *filename);
void CL_PakDownloads(int mode)
{
	/*
	mode=0 no downloads (forced to 1 for pure)
	mode=1 archived names so local stuff is not poluted
	mode=2 downloaded packages will always be present. Use With Caution.
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
		else
			continue;

		if (mode != 2)
		{
			/*if we already have such a file, this is a no-op*/
			if (CL_CheckDLFile(va("package/%s", pname)))
				continue;
			FS_GenCachedPakName(pname, com_token, local, sizeof(local));
		}
		else
			Q_strncpyz(local, pname, sizeof(local));
		CL_CheckOrEnqueDownloadFile(pname, local, DLLF_NONGAME);
	}
}

void CL_CheckServerPacks(void)
{
	static qboolean oldpure;
	qboolean pure = cl_pure.ival || atoi(Info_ValueForKey(cl.serverinfo, "sv_pure"));

	if (pure != oldpure || cl.serverpakschanged)
	{
		if (pure)
		{
			CL_PakDownloads((!cl_download_packages.ival)?1:cl_download_packages.ival);
			FS_ForceToPure(cl.serverpaknames, cl.serverpakcrcs, cls.challenge);

			/*when enabling pure, kill cached models/sounds/etc*/
			Cache_Flush();
			/*make sure cheating lamas can't use old shaders from a different srver*/
			Shader_NeedReload(true);
		}
		else
		{
			CL_PakDownloads(cl_download_packages.ival);
			FS_ImpurePacks(cl.serverpaknames, cl.serverpakcrcs);
		}
	}
	oldpure = pure;
	cl.serverpakschanged = false;
}

void CL_CheckServerInfo(void)
{
	char *s;
	unsigned int allowed;
	int oldstate;
	int oldteamplay;

	oldteamplay = cl.teamplay;
	cl.teamplay = atoi(Info_ValueForKey(cl.serverinfo, "teamplay"));
	cl.deathmatch = atoi(Info_ValueForKey(cl.serverinfo, "deathmatch"));

	cls.allow_cheats = false;
	cls.allow_semicheats=true;
	cls.allow_rearview=false;
	cls.allow_watervis=false;
	cls.allow_skyboxes=false;
	cls.allow_mirrors=false;
	cls.allow_luma=false;
	cls.allow_postproc=false;
	cls.allow_fbskins = 1;
//	cls.allow_fbskins = 0;
//	cls.allow_overbrightlight;
	if (cl.spectator || cls.demoplayback || atoi(Info_ValueForKey(cl.serverinfo, "rearview")))
		cls.allow_rearview=true;

	if (cl.spectator || cls.demoplayback || atoi(Info_ValueForKey(cl.serverinfo, "watervis")))
		cls.allow_watervis=true;

	if (cl.spectator || cls.demoplayback || atoi(Info_ValueForKey(cl.serverinfo, "allow_skybox")) || atoi(Info_ValueForKey(cl.serverinfo, "allow_skyboxes")))
		cls.allow_skyboxes=true;

	if (cl.spectator || cls.demoplayback || atoi(Info_ValueForKey(cl.serverinfo, "mirrors")))
		cls.allow_mirrors=true;

	s = Info_ValueForKey(cl.serverinfo, "allow_luma");
	if (cl.spectator || cls.demoplayback || !*s || atoi(s))
		cls.allow_luma=true;

	if (cl.spectator || cls.demoplayback || atoi(Info_ValueForKey(cl.serverinfo, "allow_lmgamma")))
		cls.allow_lightmapgamma=true;

	s = Info_ValueForKey(cl.serverinfo, "allow_fish");
	if (cl.spectator || cls.demoplayback || !*s || atoi(s))
		cls.allow_postproc=true;
	s = Info_ValueForKey(cl.serverinfo, "allow_postproc");
	if (cl.spectator || cls.demoplayback || !*s || atoi(s))
		cls.allow_postproc=true;

	s = Info_ValueForKey(cl.serverinfo, "fbskins");
	if (*s)
		cls.allow_fbskins = atof(s);
	else if (cl.teamfortress)
		cls.allow_fbskins = 0;

	s = Info_ValueForKey(cl.serverinfo, "*cheats");
	if (cl.spectator || cls.demoplayback || !stricmp(s, "on"))
		cls.allow_cheats = true;

	s = Info_ValueForKey(cl.serverinfo, "strict");
	if ((!cl.spectator && !cls.demoplayback && *s && strcmp(s, "0")) || !ruleset_allow_semicheats.ival)
	{
		cls.allow_semicheats = false;
		cls.allow_cheats	= false;
	}

	cls.maxfps = atof(Info_ValueForKey(cl.serverinfo, "maxfps"));
	if (cls.maxfps < 20)
		cls.maxfps = 72;

	cls.deathmatch = atoi(Info_ValueForKey(cl.serverinfo, "deathmatch"));

	cls.z_ext = atoi(Info_ValueForKey(cl.serverinfo, "*z_ext"));

	// movement vars for prediction
	cl.bunnyspeedcap = Q_atof(Info_ValueForKey(cl.serverinfo, "pm_bunnyspeedcap"));
	movevars.slidefix = (Q_atof(Info_ValueForKey(cl.serverinfo, "pm_slidefix")) != 0);
	movevars.airstep = (Q_atof(Info_ValueForKey(cl.serverinfo, "pm_airstep")) != 0);
	movevars.walljump = (Q_atof(Info_ValueForKey(cl.serverinfo, "pm_walljump")));
	movevars.ktjump = Q_atof(Info_ValueForKey(cl.serverinfo, "pm_ktjump"));
	s = Info_ValueForKey(cl.serverinfo, "pm_stepheight");
	if (*s)
		movevars.stepheight = Q_atof(s);
	else
		movevars.stepheight = PM_DEFAULTSTEPHEIGHT;

	// Initialize cl.maxpitch & cl.minpitch
	if (cls.protocol == CP_QUAKEWORLD || cls.protocol == CP_NETQUAKE)
	{
		s = (cls.z_ext & Z_EXT_PITCHLIMITS) ? Info_ValueForKey (cl.serverinfo, "maxpitch") : "";
		cl.maxpitch = *s ? Q_atof(s) : 80.0f;
		s = (cls.z_ext & Z_EXT_PITCHLIMITS) ? Info_ValueForKey (cl.serverinfo, "minpitch") : "";
		cl.minpitch = *s ? Q_atof(s) : -70.0f;
	}
	else
	{
		cl.maxpitch = 89.9;
		cl.minpitch = -89.9;
	}

	cl.hexen2pickups = atoi(Info_ValueForKey(cl.serverinfo, "sv_pupglow"));

	allowed = atoi(Info_ValueForKey(cl.serverinfo, "allow"));
	if (allowed & 1)
		cls.allow_watervis = true;
	if (allowed & 2)
		cls.allow_rearview = true;
	if (allowed & 4)
		cls.allow_skyboxes = true;
	if (allowed & 8)
		cls.allow_mirrors = true;
	//16
	if (allowed & 32)
		cls.allow_luma = true;
	if (allowed & 128)
		cls.allow_postproc = true;
	if (allowed & 256)
		cls.allow_lightmapgamma = true;
	if (allowed & 512)
		cls.allow_cheats = true;

	if (cls.allow_semicheats)
		cls.allow_anyparticles = true;
	else
		cls.allow_anyparticles = false;


	if (cl.spectator || cls.demoplayback)
		cl.fpd = 0;
	else
		cl.fpd = atoi(Info_ValueForKey(cl.serverinfo, "fpd"));

	cl.gamespeed = atof(Info_ValueForKey(cl.serverinfo, "*gamespeed"))/100.f;
	if (cl.gamespeed < 0.1)
		cl.gamespeed = 1;

	s = Info_ValueForKey(cl.serverinfo, "status");
	oldstate = cl.matchstate;
	if (!stricmp(s, "standby"))
		cl.matchstate = MATCH_STANDBY;
	else if (!stricmp(s, "countdown"))
		cl.matchstate = MATCH_COUNTDOWN;
	else
		cl.matchstate = MATCH_DONTKNOW;
	if (oldstate != cl.matchstate)
		cl.matchgametime = 0;

	CL_CheckServerPacks();

	Cvar_ForceCheatVars(cls.allow_semicheats, cls.allow_cheats);
	Validation_Apply_Ruleset();

	if (oldteamplay != cl.teamplay)
		Skin_FlushPlayers();
}
/*
==================
CL_FullServerinfo_f

Sent by server just after the svc_serverdata
==================
*/
void CL_FullServerinfo_f (void)
{
	char *p;
	float v;

	if (!Cmd_FromGamecode())
	{
		Con_Printf("Hey! fullserverinfo is meant to come from the server!\n");
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Con_TPrintf (TLC_SYNTAX_FULLSERVERINFO);
		return;
	}

	Q_strncpyz (cl.serverinfo, Cmd_Argv(1), sizeof(cl.serverinfo));

	if ((p = Info_ValueForKey(cl.serverinfo, "*version")) && *p) {
		v = Q_atof(p);
		if (v) {
			if (!server_version)
				Con_TPrintf (TLC_SERVER_VERSION, v);
			server_version = v;
		}
	}
	CL_CheckServerInfo();

	cl.csqcdebug = atoi(Info_ValueForKey(cl.serverinfo, "*csqcdebug"));
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
		Con_TPrintf (TLC_SYNTAX_FULLINFO);
		return;
	}

	s = Cmd_Argv(1);
	if (*s == '\\')
		s++;
	while (*s)
	{
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (!*s)
		{
			Con_TPrintf (TL_KEYHASNOVALUE);
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
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
#ifdef Q2CLIENT
		if (cls.protocol == CP_QUAKE2 || cls.protocol == CP_QUAKE3)
			cls.resendinfo = true;
		else
#endif
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
		Info_Print (cls.userinfo[pnum]);
		return;
	}
	if (Cmd_Argc() != 3)
	{
		Con_TPrintf (TLC_SYNTAX_SETINFO);
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
		Con_TPrintf (TL_STARKEYPROTECTED);
		return;
	}


	CL_SetInfo(pnum, Cmd_Argv(1), Cmd_Argv(2));
}

void CL_SaveInfo(vfsfile_t *f)
{
	int i;
	VFS_WRITE(f, "\n", 1);
	for (i = 0; i < MAX_SPLITS; i++)
	{
		if (i)
			VFS_WRITE(f, va("p%i setinfo * \"\"\n", i+1), 16);
		else
			VFS_WRITE(f, "setinfo * \"\"\n", 13);
		Info_WriteToFile(f, cls.userinfo[i], "setinfo", CVAR_USERINFO);
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
		Con_TPrintf (TLC_PACKET_SYNTAX);
		return;
	}

	if (!NET_StringToAdr (Cmd_Argv(1), PORT_QWSERVER, &adr))
	{
		Con_Printf ("Bad address: %s\n", Cmd_Argv(1));
		return;
	}


	if (Cmd_FromGamecode())	//some mvd servers stuffcmd a packet command which lets them know which ip the client is from.
	{						//unfortunatly, 50% of servers are badly configured.
		if (adr.type == NA_IP)
			if (adr.address.ip[0] == 127)
			if (adr.address.ip[1] == 0)
			if (adr.address.ip[2] == 0)
			if (adr.address.ip[3] == 1)
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

	NET_SendPacket (NS_CLIENT, out-send, send, adr);

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

	if (cls.demonum == -1)
		return;		// don't play demos

	if (!cls.demos[cls.demonum][0] || cls.demonum == MAX_DEMOS)
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

	if (
#ifndef CLIENTONLY
		!sv.state &&
#endif
		cls.demonum != -1 && cls.demoplayback==DPB_NONE && !Media_PlayingFullScreen() && COM_CheckParm("-demos"))
	{
		cls.demonum = 0;
		CL_NextDemo ();
	}
	else
		cls.demonum = -1;
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
	if (cls.downloadqw)  // don't change when downloading
		return;

	if (*mapname)
		SCR_ImageName(mapname);
	else
		SCR_BeginLoadingPlaque();

	S_StopAllSounds (true);
	cl.intermission = 0;
	if (cls.state)
	{
		cls.state = ca_connected;	// not active anymore, but not disconnected
		Con_TPrintf (TLC_CHANGINGMAP);
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
	if (cls.downloadqw)  // don't change when downloading
		return;
#ifdef NQPROT
	if (cls.protocol == CP_NETQUAKE && Cmd_FromGamecode())
	{
		CL_Changing_f();
		return;
	}
#endif
	S_StopAllSounds (true);

	if (cls.state == ca_connected)
	{
		Con_TPrintf (TLC_RECONNECTING);
		CL_SendClientCommand(true, "new");
		return;
	}

	if (!*cls.servername)
	{
		Con_TPrintf (TLC_RECONNECT_NOSERVER);
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

		if (NET_CompareBaseAdr(cls.realserverip, net_from) == false)
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

		NET_SendPacket (NS_CLIENT, len, &data, net_from);
		return;
	}

	if (c == A2C_PRINT)
	{
		if (!strncmp(net_message.data+msg_readcount, "\\chunk", 6))
		{
			if (NET_CompareBaseAdr(cls.netchan.remote_address, net_from) == false)
				if (cls.realserverip.type == NA_INVALID || NET_CompareBaseAdr(cls.realserverip, net_from) == false)
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
			}
			return;
		}
	}

	if (cls.demoplayback == DPB_NONE && net_from.type != NA_LOOPBACK)
		Con_TPrintf (TL_ST_COLON, NET_AdrToString (adr, sizeof(adr), net_from));
//	Con_DPrintf ("%s", net_message.data + 4);

	if (c == S2C_CHALLENGE)
	{
		static unsigned int lasttime = 0xdeadbeef;
		unsigned int curtime = Sys_Milliseconds();
		unsigned long pext = 0, pext2 = 0, huffcrc=0, mtu=0;
		Con_TPrintf (TLC_S2C_CHALLENGE);

		s = MSG_ReadString ();
		COM_Parse(s);
		if (!strcmp(com_token, "hallengeResponse"))
		{
			/*Quake3*/
#ifdef Q3CLIENT
			if (cls.protocol == CP_QUAKE3 || cls.protocol == CP_UNKNOWN)
			{
				/*throttle*/
				if (curtime - lasttime < 500)
					return;
				lasttime = curtime;

				cls.protocol = CP_QUAKE3;
				cls.challenge = atoi(s+17);
				CL_SendConnectPacket (0, 0, 0, 0/*, ...*/);
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
			if (*s2 && *s2 != ' ')
			{//and if it's not, we're unlikly to be compatible with whatever it is that's talking at us.
#ifdef NQPROT
				if (cls.protocol == CP_NETQUAKE || cls.protocol == CP_UNKNOWN)
				{
					/*throttle*/
					if (curtime - lasttime < 500)
						return;
					lasttime = curtime;

					cls.protocol = CP_NETQUAKE;
					CL_ConnectToDarkPlaces(s+9, net_from);
				}
				else
					Con_Printf("\nChallenge from another protocol, ignoring DP challenge\n");
#else
				Con_Printf("\nUnable connect to DarkPlaces\n");
#endif
				return;
			}

#ifdef Q2CLIENT
			if (cls.protocol == CP_QUAKE2 || cls.protocol == CP_UNKNOWN)
				cls.protocol = CP_QUAKE2;
			else
			{
				Con_Printf("\nChallenge from another protocol, ignoring Q2 challenge\n");
				return;
			}
#else
			Con_Printf("\nUnable to connect to Quake2\n");
#endif
			s+=9;
		}
#ifdef Q3CLIENT
		else if (!strcmp(com_token, "onnectResponse"))
		{
			goto client_connect;
		}
#endif
#ifdef Q2CLIENT
		else if (!strcmp(com_token, "lient_connect"))
		{
			goto client_connect;
		}
#endif

		/*no idea, assume a QuakeWorld challenge response ('c' packet)*/

		else if (cls.protocol == CP_QUAKEWORLD || cls.protocol == CP_UNKNOWN)
			cls.protocol = CP_QUAKEWORLD;
		else
		{
			Con_Printf("\nChallenge from another protocol, ignoring QW challenge\n");
			return;
		}

		/*throttle*/
		if (curtime - lasttime < 500)
			return;
		lasttime = curtime;

		cls.challenge = atoi(s);

		for(;;)
		{
			c = MSG_ReadLong ();
			if (msg_badread)
				break;
			if (c == PROTOCOL_VERSION_FTE)
				pext = MSG_ReadLong ();
			else if (c == PROTOCOL_VERSION_FTE2)
				pext2 = MSG_ReadLong ();
			else if (c == PROTOCOL_VERSION_FRAGMENT)
				mtu = MSG_ReadLong ();
			else if (c == PROTOCOL_VERSION_VARLENGTH)
			{
				int len = MSG_ReadLong();
				if (len < 0 || len > 8192)
					break;
				c = MSG_ReadLong();/*ident*/
				MSG_ReadSkip(len); /*payload*/
			}
#ifdef HUFFNETWORK
			else if (c == PROTOCOL_VERSION_HUFFMAN)
				huffcrc = MSG_ReadLong ();
#endif
			//else if (c == PROTOCOL_VERSION_...)
			else
				MSG_ReadLong ();
		}
		CL_SendConnectPacket (mtu, pext, pext2, huffcrc/*, ...*/);
		return;
	}
#ifdef Q2CLIENT
	if (cls.protocol == CP_QUAKE2)
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
			Con_TPrintf (TLC_A2C_PRINT);

			s = MSG_ReadString ();
			Con_Printf ("%s", s);
			return;
		}
		else if (!strcmp(s, "client_connect"))
		{
			goto client_connect;
		}
		else if (!strcmp(s, "disconnect"))
		{
			if (NET_CompareAdr(net_from, cls.netchan.remote_address))
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
			Con_TPrintf (TLC_Q2CONLESSPACKET_UNKNOWN, s);
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
			Con_Printf ("accept\n");
			Validation_Apply_Ruleset();
			Netchan_Setup(NS_CLIENT, &cls.netchan, net_from, cls.qport);
			CL_ParseEstablished();
			Con_DPrintf ("CL_EstablishConnection: connected to %s\n", cls.servername);

			/*this is a DP server... but we don't know which version*/
			cls.netchan.isnqprotocol = true;
			cls.protocol = CP_NETQUAKE;
			cls.protocol_nq = CPNQ_ID;

			cls.demonum = -1;			// not in the demo loop now
			cls.state = ca_connected;


			SCR_BeginLoadingPlaque();
			return;
		}
	}
#endif

	if (c == 'd')	//note - this conflicts with qw masters, our browser uses a different socket.
	{
		Con_Printf ("d\n");
		if (cls.demoplayback != DPB_NONE)
		{
			Con_Printf("Disconnect\n");
			CL_Disconnect_f();
		}
		return;
	}

	if (c == S2C_CONNECTION)
	{
		int compress;
		int mtu;
#ifdef Q2CLIENT
client_connect:	//fixme: make function
#endif
		if (net_from.type != NA_LOOPBACK)
			Con_TPrintf (TLC_GOTCONNECTION);
		if (cls.state >= ca_connected)
		{
			if (cls.demoplayback == DPB_NONE)
				Con_TPrintf (TLC_DUPCONNECTION);
			return;
		}
		compress = cls.netchan.compress;
		mtu = cls.netchan.fragmentsize;
		Netchan_Setup (NS_CLIENT, &cls.netchan, net_from, cls.qport);
		cls.netchan.fragmentsize = mtu;
		cls.netchan.compress = compress;
		CL_ParseEstablished();
#ifdef Q3CLIENT
		if (cls.protocol != CP_QUAKE3)
#endif
			CL_SendClientCommand(true, "new");
		cls.state = ca_connected;
		if (cls.netchan.remote_address.type != NA_LOOPBACK)
			Con_TPrintf (TLC_CONNECTED);
		allowremotecmd = false; // localid required now for remote cmds

		total_loading_size = 100;
		current_loading_size = 0;
		SCR_SetLoadingStage(LS_CLIENT);

		Validation_Apply_Ruleset();

		return;
	}
	// remote command from gui front end
	if (c == A2C_CLIENT_COMMAND)	//man I hate this.
	{
		char	cmdtext[2048];

		Con_TPrintf (TLC_CONLESS_CONCMD);
		if (net_from.type != net_local_cl_ipadr.type
			|| ((*(unsigned *)net_from.address.ip != *(unsigned *)net_local_cl_ipadr.address.ip) && (*(unsigned *)net_from.address.ip != htonl(INADDR_LOOPBACK))))
		{
			Con_TPrintf (TLC_CMDFROMREMOTE);
			return;
		}
#ifdef _WIN32
		ShowWindow (mainwindow, SW_RESTORE);
		SetForegroundWindow (mainwindow);
#endif
		s = MSG_ReadString ();

		Q_strncpyz(cmdtext, s, sizeof(cmdtext));

		s = MSG_ReadString ();

		while (*s && isspace(*s))
			s++;
		while (*s && isspace(s[strlen(s) - 1]))
			s[strlen(s) - 1] = 0;

		if (!allowremotecmd && (!*localid.string || strcmp(localid.string, s))) {
			if (!*localid.string) {
				Con_TPrintf (TLC_LOCALID_NOTSET);
				return;
			}
			Con_TPrintf (TLC_LOCALID_BAD,
				s, localid.string);
			Cvar_Set(&localid, "");
			return;
		}

		Cbuf_AddText (cmdtext, RESTRICT_SERVER);
		allowremotecmd = false;
		return;
	}
	// print command from somewhere
	if (c == 'p')
	{
		if (!strncmp(net_message.data+4, "print\n", 6))
		{
			Con_TPrintf (TLC_A2C_PRINT);
			Con_Printf ("%s", net_message.data+10);
			return;
		}
	}
	if (c == A2C_PRINT)
	{
		Con_TPrintf (TLC_A2C_PRINT);

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
	if (c == svc_disconnect && cls.demoplayback != DPB_NONE)
	{
		Host_EndGame ("End of Demo");
		return;
	}

	Con_TPrintf (TLC_CONLESSPACKET_UNKNOWN, c);
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
		if (cls.state >= ca_connected)
		{
			if (cls.demoplayback == DPB_NONE)
				Con_TPrintf (TLC_DUPCONNECTION);
			return;
		}
		port = htons((unsigned short)MSG_ReadLong());
		//this is the port that we're meant to respond to.

		if (port)
			net_from.port = port;

		cls.protocol_nq = CPNQ_ID;
		if (MSG_ReadByte() == 1)	//a proquake server adds a little extra info
		{
			int ver = MSG_ReadByte();
			Con_DPrintf("ProQuake server %i.%i\n", ver/10, ver%10);

//			if (ver >= 34)
			cls.protocol_nq = CPNQ_PROQUAKE3_4;
			if (MSG_ReadByte() == 1)
			{
				//its a 'pure' server.
				Con_Printf("pure ProQuake server\n");
				return;
			}
		}

		Validation_Apply_Ruleset();

		Netchan_Setup (NS_CLIENT, &cls.netchan, net_from, cls.qport);
		CL_ParseEstablished();
		cls.netchan.isnqprotocol = true;
		cls.netchan.compress = 0;
		cls.protocol = CP_NETQUAKE;
		cls.state = ca_connected;
		Con_TPrintf (TLC_CONNECTED);

		total_loading_size = 100;
		current_loading_size = 0;
		SCR_SetLoadingStage(LS_CLIENT);

		allowremotecmd = false; // localid required now for remote cmds

		//send a dummy packet.
		//this makes our local nat think we initialised the conversation.
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
			break;

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
			Con_TPrintf (TL_RUNTPACKET,NET_AdrToString(adr, sizeof(adr), net_from));
			continue;
		}

		if (cls.state == ca_disconnected)
		{	//connect to nq servers, but don't get confused with sequenced packets.
#ifdef NQPROT
			CLNQ_ConnectionlessPacket ();
#endif
			continue;	//ignore it. We arn't connected.
		}

		//
		// packet from server
		//
		if (!cls.demoplayback &&
			!NET_CompareAdr (net_from, cls.netchan.remote_address))
		{
			Con_DPrintf ("%s:sequenced packet from wrong server\n"
				,NET_AdrToString(adr, sizeof(adr), net_from));
			continue;
		}

		switch(cls.protocol)
		{
		case CP_NETQUAKE:
#ifdef NQPROT
			switch(NQNetChan_Process(&cls.netchan))
			{
			case NQP_ERROR:
				break;
			case NQP_DATAGRAM://datagram
//				cls.netchan.incoming_sequence = cls.netchan.outgoing_sequence - 3;
			case NQP_RELIABLE://reliable
				MSG_ChangePrimitives(cls.netchan.netprim);
				CLNQ_ParseServerMessage ();
				break;
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
			Con_TPrintf (TLC_SERVERTIMEOUT);
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

/*
=====================
CL_Download_f
=====================
*/
void CL_Download_f (void)
{
//	char *p, *q;
	char *url;

	url = Cmd_Argv(1);

#ifdef WEBCLIENT
	if (!strnicmp(url, "http://", 7) || !strnicmp(url, "ftp://", 6))
	{
		if (Cmd_IsInsecure())
			return;
		HTTP_CL_Get(url, Cmd_Argv(2), NULL);//"test.txt");
		return;
	}
#endif

	if (!strnicmp(url, "qw://", 5) || !strnicmp(url, "q2://", 5))
	{
		url += 5;
	}

	if ((cls.state == ca_disconnected || cls.demoplayback) && cls.demoplayback != DPB_EZTV)
	{
		Con_TPrintf (TLC_CONNECTFIRST);
		return;
	}

	if (cls.netchan.remote_address.type == NA_LOOPBACK)
	{
		Con_TPrintf (TLC_CONNECTFIRST);
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Con_TPrintf (TLC_SYNTAX_DOWNLOAD);
		return;
	}

	if (Cmd_IsInsecure())	//mark server specified downloads.
	{
		if (!strnicmp(url, "game", 4) || !stricmp(url, "progs.dat") || !stricmp(url, "menu.dat") || !stricmp(url, "csprogs.dat") || !stricmp(url, "qwprogs.dat") || strstr(url, "..") || strstr(url, ".qvm") || strstr(url, ".dll") || strstr(url, ".so"))
		{	//yes, I know the user can use a different progs from the one that is specified. If you leave it blank there will be no problem. (server isn't allowed to stuff progs cvar)
			Con_Printf("Ignoring stuffed download of \"%s\" due to possible security risk\n", url);
			return;
		}

		CL_CheckOrEnqueDownloadFile(url, url, DLLF_REQUIRED|DLLF_VERBOSE);
		return;
	}

	CL_EnqueDownload(url, url, DLLF_IGNOREFAILED|DLLF_REQUIRED|DLLF_OVERWRITE|DLLF_VERBOSE);
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
		CL_DownloadFailed(rname, false);
	}
	else if (!strcmp(size, "p"))
	{
		if (stricmp(cls.downloadremotename, rname))
		{
			Con_Printf("Download of \"%s\" failed. Not allowed.\n", rname);
			CL_DownloadFailed(rname, false);
		}
	}
	else if (!strcmp(size, "r"))
	{	//'download this file instead'
		int allow = cl_download_redirection.ival;
		redirection = Cmd_Argv(3);

		dl = CL_DownloadFailed(rname, false);

		if (allow == 2)
		{
			char *ext = COM_FileExtension(redirection);
			if (!strcmp(ext, "pak") || !strcmp(ext, "pk3") || !strcmp(ext, "pk4"))
				allow = true;
			else
				allow = false;
		}
		if (allow)
		{
			Con_DPrintf("Download of \"%s\" redirected to \"%s\".\n", rname, redirection);
			CL_CheckOrEnqueDownloadFile(redirection, NULL, dl->flags);
		}
		else
			Con_Printf("Download of \"%s\" redirected to \"%s\". Prevented by cl_download_redirection.\n", rname, redirection);
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
void CL_ForceStopDownload (qboolean finish)
{
	if (Cmd_IsInsecure())
	{
		Con_Printf(CON_WARNING "Execution from server rejected for %s\n", Cmd_Argv(0));
		return;
	}

	if (!cls.downloadqw)
	{
		Con_Printf("No files downloading by QW protocol\n");
		return;
	}

	VFS_CLOSE (cls.downloadqw);
	cls.downloadqw = NULL;

	if (finish)
		CL_DownloadFinished();
	else
	{
		char *tempname;

		if (*cls.downloadtempname)
			tempname = cls.downloadtempname;
		else
			tempname = cls.downloadlocalname;

		if (strncmp(tempname,"skins/",6))
			FS_Remove(tempname, FS_GAME);
		else
			FS_Remove(tempname+6, FS_SKINS);
	}
	*cls.downloadlocalname = '\0';
	*cls.downloadremotename = '\0';
	cls.downloadpercent = 0;

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

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
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
			Info_Print (cl.serverinfo);
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

/*
#ifdef WEBCLIENT
void CL_FTP_f(void)
{
	FTP_Client_Command(Cmd_Args(), NULL);
}
#endif
*/

void CL_Fog_f(void)
{
	if (cl.fog_locked && !Cmd_FromGamecode())
		Con_Printf("Current fog %f (r:%f g:%f b:%f)\n", cl.fog_density, cl.fog_colour[0], cl.fog_colour[1], cl.fog_colour[2]);
	else if (Cmd_Argc() <= 1)
	{
		Con_Printf("Current fog %f (r:%f g:%f b:%f)\n", cl.fog_density, cl.fog_colour[0], cl.fog_colour[1], cl.fog_colour[2]);
	}
	else
	{
		cl.fog_density = atof(Cmd_Argv(1));
		if (Cmd_Argc() >= 5)
		{
			cl.fog_colour[0] = atof(Cmd_Argv(2));
			cl.fog_colour[1] = atof(Cmd_Argv(3));
			cl.fog_colour[2] = atof(Cmd_Argv(4));
		}

		if (Cmd_FromGamecode())
			cl.fog_locked = !!cl.fog_density;
	}
}

void CL_Skygroup_f(void);
void SCR_ShowPic_Script_f(void);
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
	Info_SetValueForStarKey (cls.userinfo[1], "*ss", "1", sizeof(cls.userinfo[1]));
	Info_SetValueForStarKey (cls.userinfo[2], "*ss", "1", sizeof(cls.userinfo[2]));
	Info_SetValueForStarKey (cls.userinfo[3], "*ss", "1", sizeof(cls.userinfo[3]));

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

	Cvar_Register (&cl_defaultport, cl_controlgroup);
	Cvar_Register (&cl_servername, cl_controlgroup);
	Cvar_Register (&cl_serveraddress, cl_controlgroup);
	Cvar_Register (&cl_demospeed, "Demo playback");
	Cvar_Register (&cl_warncmd, "Warnings");
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
	Cvar_Register (&cl_solid_players,	cl_predictiongroup);

	Cvar_Register (&localid,	cl_controlgroup);

	Cvar_Register (&cl_muzzleflash, cl_controlgroup);

	Cvar_Register (&baseskin,	"Teamplay");
	Cvar_Register (&noskins,	"Teamplay");
	Cvar_Register (&cl_noblink,	"Console controls");	//for lack of a better group

	Cvar_Register (&cl_item_bobbing, "Item effects");

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

	Cvar_Register (&cl_nofake,					cl_controlgroup);
	Cvar_Register (&cl_chatsound,					cl_controlgroup);
	Cvar_Register (&cl_enemychatsound,				cl_controlgroup);
	Cvar_Register (&cl_teamchatsound,				cl_controlgroup);

	Cvar_Register (&requiredownloads,				cl_controlgroup);
	Cvar_Register (&cl_standardchat,				cl_controlgroup);
	Cvar_Register (&msg_filter,					cl_controlgroup);
	Cvar_Register (&cl_standardmsg,					cl_controlgroup);
	Cvar_Register (&cl_parsewhitetext,				cl_controlgroup);
	Cvar_Register (&cl_nopext,					cl_controlgroup);
	Cvar_Register (&cl_pext_mask,					cl_controlgroup);

	Cvar_Register (&cl_splitscreen,					cl_controlgroup);

	Cvar_Register (&host_mapname,					"Scripting");

#ifndef SERVERONLY
	Cvar_Register (&cl_loopbackprotocol,				cl_controlgroup);
#endif
	Cvar_Register (&cl_countpendingpl,				cl_controlgroup);
	Cvar_Register (&cl_threadedphysics,				cl_controlgroup);
	Cvar_Register (&hud_tracking_show,				"statusbar");
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
	Cvar_Register (&ruleset_allow_sensative_texture_replacements,	cl_controlgroup);
	Cvar_Register (&ruleset_allow_localvolume,			cl_controlgroup);
	Cvar_Register (&ruleset_allow_shaders,			cl_controlgroup);
	Cvar_Register (&ruleset_allow_fbmodels,			cl_controlgroup);

	Cvar_Register (&qtvcl_forceversion1,				cl_controlgroup);
	Cvar_Register (&qtvcl_eztvextensions,				cl_controlgroup);
//#ifdef WEBCLIENT
//	Cmd_AddCommand ("ftp", CL_FTP_f);
//#endif

	Cmd_AddCommand ("changing", CL_Changing_f);
	Cmd_AddCommand ("disconnect", CL_Disconnect_f);
	Cmd_AddCommand ("record", CL_Record_f);
	Cmd_AddCommand ("rerecord", CL_ReRecord_f);
	Cmd_AddCommand ("stop", CL_Stop_f);
	Cmd_AddCommand ("playdemo", CL_PlayDemo_f);
	Cmd_AddCommand ("qtvplay", CL_QTVPlay_f);
	Cmd_AddCommand ("qtvlist", CL_QTVList_f);
	Cmd_AddCommand ("qtvdemos", CL_QTVDemos_f);
	Cmd_AddCommand ("demo_jump", CL_DemoJump_f);
	Cmd_AddCommand ("timedemo", CL_TimeDemo_f);

	Cmd_AddCommand ("showpic", SCR_ShowPic_Script_f);

	Cmd_AddCommand ("startdemos", CL_Startdemos_f);
	Cmd_AddCommand ("demos", CL_Demos_f);
	Cmd_AddCommand ("stopdemo", CL_Stopdemo_f);

	Cmd_AddCommand ("skins", Skin_Skins_f);
	Cmd_AddCommand ("allskins", Skin_AllSkins_f);

	Cmd_AddCommand ("quit", CL_Quit_f);

	Cmd_AddCommand ("connect", CL_Connect_f);
#ifdef TCPCONNECT
	Cmd_AddCommand ("tcpconnect", CL_TCPConnect_f);
#endif
#ifdef IRCCONNECT
	Cmd_AddCommand ("ircconnect", CL_IRCConnect_f);
#endif
#ifdef NQPROT
	Cmd_AddCommand ("nqconnect", CLNQ_Connect_f);
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
	Cmd_AddCommand ("fullserverinfo", CL_FullServerinfo_f);

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
	Cmd_AddCommand ("notarget", NULL);
	Cmd_AddCommand ("fly", NULL);
	Cmd_AddCommand ("setpos", NULL);

	Cmd_AddCommand ("topten", NULL);

	Cmd_AddCommand ("fog", CL_Fog_f);
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
#ifdef _WINDOWS
	Cmd_AddCommand ("windows", CL_Windows_f);
#endif

	Ignore_Init();
}


/*
================
Host_EndGame

Call this to drop to a console without exiting the qwcl
================
*/
void VARGS Host_EndGame (char *message, ...)
{
	va_list		argptr;
	char		string[1024];

	SCR_EndLoadingPlaque();

	va_start (argptr,message);
	vsnprintf (string,sizeof(string)-1, message,argptr);
	va_end (argptr);
	Con_TPrintf (TLC_CLIENTCON_ERROR_ENDGAME, string);
	Con_TPrintf (TL_NL);

	SCR_EndLoadingPlaque();

	CL_Disconnect ();

	SV_UnspawnServer();

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
	Con_TPrintf (TLC_HOSTFATALERROR, string);

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
			Con_TPrintf (TLC_CONFIGCFG_WRITEFAILED);
			return;
		}

		Q_snprintfz(savename, sizeof(savename), "%s.cfg", cfg_save_name.string);

		f = FS_OpenVFS(savename, "wb", FS_GAMEONLY);
		if (!f)
		{
			Con_TPrintf (TLC_CONFIGCFG_WRITEFAILED);
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

/*
==================
Host_Frame

Runs all active servers
==================
*/
extern cvar_t cl_netfps;
extern cvar_t cl_sparemsec;

int		nopacketcount;
void SNDDMA_SetUnderWater(qboolean underwater);
double Host_Frame (double time)
{
	static double		time1 = 0;
	static double		time2 = 0;
	static double		time3 = 0;
	int			pass1, pass2, pass3;
//	float fps;
	double realframetime, newrealtime;
	static double spare;
	float maxfps;
	qboolean maxfpsignoreserver;
	qboolean idle;

	RSpeedLocals();

	if (setjmp (host_abort) )
	{
		return 0;			// something bad happened, or the server disconnected
	}

	newrealtime = Media_TweekCaptureFrameTime(realtime, time);

	realframetime = time = newrealtime - realtime;
	realtime = newrealtime;

	if (oldrealtime > realtime)
		oldrealtime = 0;

//	if (cls.demoplayback && cl_demospeed.value>0)
//		realframetime *= cl_demospeed.value; // this probably screws up other timings

	if (cl.gamespeed<0.1)
		cl.gamespeed = 1;
	time *= cl.gamespeed;

#ifdef WEBCLIENT
//	FTP_ClientThink();
	HTTP_CL_Think();
#endif

#ifdef PLUGINS
	Plug_Tick();
#endif

	if (cl.paused)
		cl.gametimemark += time;

	idle = (cls.state == ca_disconnected) || 
#ifdef VM_UI
		UI_MenuState() != 0 || 
#endif
		key_dest == key_menu || 
		key_dest == key_editor ||
		cl.paused;
	// TODO: check if minimized or unfocused

	if (idle && cl_idlefps.value > 0)
	{
		double idlesec = 1.0 / cl_idlefps.value;
		if (idlesec > 0.1)
			idlesec = 0.1; // limit to at least 10 fps
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

	if (maxfps > 0 
#if !defined(NOMEDIA)
		&& Media_Capturing() != 2
#endif
		)
	{
		realtime += spare/1000;	//don't use it all!
		spare = CL_FilterTime((realtime - oldrealtime)*1000, maxfps, maxfpsignoreserver);
		if (!spare)
			return cl_yieldcpu.ival ? (1.0 / maxfps - (realtime - oldrealtime)) : 0;
		if (spare < 0 || cls.state < ca_onserver)
			spare = 0;	//uncapped.
		if (spare > cl_sparemsec.ival)
			spare = cl_sparemsec.ival;

		realtime -= spare/1000;	//don't use it all!
	}
	else
		spare = 0;

	host_frametime = (realtime - oldrealtime)*cl.gamespeed;
	if (!cl.paused)
	{
		cl.matchgametime += host_frametime;
	}
	oldrealtime = realtime;

	CL_ProgressDemoTime();


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

	CL_AllowIndependantSendCmd(false);

	// fetch results from server
	CL_ReadPackets ();

	// send intentions now
	// resend a connection request if necessary
	if (cls.state == ca_disconnected)
	{
		IN_Move(NULL, 0);
		CL_CheckForResend ();
	}
	else
	{
		CL_SendCmd (cl.gamespeed?host_frametime/cl.gamespeed:host_frametime, true);

		if (cls.state == ca_onserver && cl.validsequence && cl.worldmodel)
		{	// first update is the final signon stage
			CL_MakeActive("QuakeWorld");
		}
	}
	CL_AllowIndependantSendCmd(true);

	RSpeedEnd(RSPEED_PROTOCOL);

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

	if (SCR_UpdateScreen)
	{
		extern mleaf_t	*r_viewleaf;
		extern cvar_t scr_chatmodecvar;

		if (scr_chatmodecvar.ival && !cl.intermission)
			scr_chatmode = (cl.spectator&&cl.splitclients<2&&cls.state == ca_active)?2:1;
		else
			scr_chatmode = 0;

		SCR_UpdateScreen ();
		if (cls.state >= ca_active && r_viewleaf)
			S_SetUnderWater(r_viewleaf->contents <= Q1CONTENTS_WATER);
		else
			S_SetUnderWater(false);
	}

	if (host_speeds.ival)
		time2 = Sys_DoubleTime ();

	// update audio
#ifdef CSQC_DAT
	if (!CSQC_SettingListener())
#endif
	{
		if (cls.state == ca_active)
		{
			if (cls.protocol != CP_QUAKE3)
				S_UpdateListener (r_origin, vpn, vright, vup);
		}
		else
			S_UpdateListener (vec3_origin, vec3_origin, vec3_origin, vec3_origin);

		S_Update ();
	}

	CDAudio_Update();

	if (host_speeds.ival)
	{
		pass1 = (time1 - time3)*1000;
		time3 = Sys_DoubleTime ();
		pass2 = (time2 - time1)*1000;
		pass3 = (time3 - time2)*1000;
		Con_TPrintf (TLC_HOSTSPEEDSOUTPUT,
					pass1+pass2+pass3, pass1, pass2, pass3);
	}


	IN_Commands ();

	// process console commands
	Cbuf_Execute ();

	CL_RequestNextDownload();


	CL_QTVPoll();

	TP_UpdateAutoStatus();

	fps_count++;
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
	buffer = COM_LoadTempFile("q3key");
	if (buffer)	//a cdkey is meant to be 16 chars
	{
		cvar_t *var;
		char *chr;
		for (chr = buffer; *chr; chr++)
		{
			if (*(unsigned char*)chr < ' ')
			{
				*chr = '\0';	//don't get more than one line.
				break;
			}
		}
		var = Cvar_Get("cl_cdkey", buffer, CVAR_LATCH|CVAR_NOUNSAFEEXPAND, "Q3 compatability");
	}
}
#endif

//============================================================================


void CL_StartCinematicOrMenu(void)
{
	//start up the ui now we have a renderer
#ifdef VM_UI
	UI_Start();
#endif

	Con_TPrintf (TLC_QUAKEWORLD_INITED, fs_gamename.string);

	//there might be some console command or somesuch waiting for the renderer to begin (demos or map command or whatever all need model support).
	realtime+=1;
	Cbuf_Execute ();	//server may have been waiting for the renderer

	//and any startup cinematics
#ifndef NOMEDIA
	if (!cls.demoinfile && !cls.state && !Media_PlayingFullScreen())
	{
		int ol_depth;
		int idcin_depth;
		int idroq_depth;

		idcin_depth = COM_FDepthFile("video/idlog.cin", true);	//q2
		idroq_depth = COM_FDepthFile("video/idlogo.roq", true);	//q2
		ol_depth = COM_FDepthFile("video/openinglogos.roq", true);	//jk2

		if (ol_depth != 0x7fffffff && (ol_depth <= idroq_depth || ol_depth <= idcin_depth))
			Media_PlayFilm("video/openinglogos.roq");
		else if (idroq_depth != 0x7fffffff && idroq_depth <= idcin_depth)
			Media_PlayFilm("video/idlogo.roq");
		else if (idcin_depth != 0x7fffffff)
			Media_PlayFilm("video/idlog.cin");
	}
#endif

	if (!cls.demoinfile && !*cls.servername && !Media_Playing())
	{
#ifndef CLIENTONLY
		if (!sv.state)
#endif
		{
			if (qrenderer > QR_NONE)
				M_ToggleMenu_f();
			//Con_ForceActiveNow();
		}
	}
}

//note that this does NOT include commandline.
void CL_ExecInitialConfigs(void)
{
	int qrc, hrc, def, i;

	Cbuf_AddText ("cl_warncmd 0\n", RESTRICT_LOCAL);

	//who should we imitate?
	qrc = COM_FDepthFile("quake.rc", true);	//q1
	hrc = COM_FDepthFile("hexen.rc", true);	//h2
	def = COM_FDepthFile("default.cfg", true);	//q2/q3

	if (qrc <= def && qrc <= hrc && qrc!=0x7fffffff)
		Cbuf_AddText ("exec quake.rc\n", RESTRICT_LOCAL);
	else if (hrc <= def && hrc!=0x7fffffff)
		Cbuf_AddText ("exec hexen.rc\n", RESTRICT_LOCAL);
	else
	{	//they didn't give us an rc file!
		Cbuf_AddText ("bind ~ toggleconsole\n", RESTRICT_LOCAL);	//we expect default.cfg to not exist. :(
		Cbuf_AddText ("exec default.cfg\n", RESTRICT_LOCAL);
		if (COM_FCheckExists ("config.cfg"))
			Cbuf_AddText ("exec config.cfg\n", RESTRICT_LOCAL);
		if (COM_FCheckExists ("q3config.cfg"))
			Cbuf_AddText ("exec q3config.cfg\n", RESTRICT_LOCAL);
		Cbuf_AddText ("exec autoexec.cfg\n", RESTRICT_LOCAL);
	}
	Cbuf_AddText ("exec fte.cfg\n", RESTRICT_LOCAL);

	if (COM_FCheckExists ("frontend.cfg"))
		Cbuf_AddText ("exec frontend.cfg\n", RESTRICT_LOCAL);
	Cbuf_AddText ("cl_warncmd 1\n", RESTRICT_LOCAL);	//and then it's allowed to start moaning.

	{
		extern cvar_t com_parseutf8;
		com_parseutf8.ival = com_parseutf8.value;
	}

	Cbuf_Execute ();	//if the server initialisation causes a problem, give it a place to abort to


	//assuming they didn't use any waits in their config (fools)
	//the configs should be fully loaded.
	//so convert the backwards compable commandline parameters in cvar sets.

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
//	Cbuf_Execute ();	//if the server initialisation causes a problem, give it a place to abort to
}





/*
====================
Host_Init
====================
*/
void Host_Init (quakeparms_t *parms)
{
	extern cvar_t com_parseutf8;
	com_parseutf8.ival = 1;	//enable utf8 parsing even before cvars are registered.

	COM_InitArgv (parms->argc, parms->argv);

	if (setjmp (host_abort) )
		Sys_Error("Host_Init: An error occured. Try the -condebug commandline parameter\n");

	if (COM_CheckParm ("-minmemory"))
		parms->memsize = MINIMUM_MEMORY;

	host_parms = *parms;

	if (parms->memsize < MINIMUM_MEMORY)
		Sys_Error ("Only %4.1f megs of memory reported, can't execute game", parms->memsize / (float)0x100000);

	Cvar_Init();
	Memory_Init (parms->membase, parms->memsize);

	/*memory is working, its safe to printf*/
	Con_Init ();

	Sys_Init();

	COM_ParsePlusSets();
	Cbuf_Init ();
	Cmd_Init ();
	V_Init ();
	COM_Init ();
#ifdef Q2BSPS
	CM_Init();
#endif
#ifdef TERRAIN
	Terr_Init();
#endif
	Host_FixupModelNames();

	NET_Init ();
	NET_InitClient ();
	Netchan_Init ();
	Renderer_Init();

//	W_LoadWadFile ("gfx.wad");
	Key_Init ();
	M_Init ();
	IN_Init ();
	S_Init ();
	cls.state = ca_disconnected;
	CDAudio_Init ();
	Sbar_Init ();
	CL_Init ();
#if defined(CSQC_DAT) || defined(MENU_DAT)
	PF_Common_RegisterCvars();
#endif

	TranslateInit();
#ifndef CLIENTONLY
	SV_Init(parms);
#endif
#ifdef TEXTEDITOR
	Editor_Init();
#endif

#ifdef PLUGINS
	Plug_Init();
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
	//Con_TPrintf (TL_HEAPSIZE, parms->memsize/ (1024*1024.0));

	Hunk_AllocName (0, "-HOST_HUNKLEVEL-");
	host_hunklevel = Hunk_LowMark ();

	R_SetRenderer(NULL);//set the renderer stuff to unset...

	host_initialized = true;
	forcesaveprompt = false;

	Sys_SendKeyEvents();


	//the engine is technically initialised at this point, except for the renderer. now we exec configs and bring up the renderer
	//anything that needs models cannot be run yet, but it should be safe to allow console commands etc.
	//if we get a map command, we'll just stick it on the end of the console command buffer.

	Con_History_Load();

	CL_ExecInitialConfigs();

	if (CL_CheckBootDownloads())
	{
		Cmd_StuffCmds();
		Cbuf_Execute ();
	}

Con_TPrintf (TL_NL);
	Con_Printf ("%s", version_string());
Con_TPrintf (TL_NL);

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

	Renderer_Start();

	CL_StartCinematicOrMenu();
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
	{
		Sys_Printf ("recursive shutdown\n");
		return;
	}
	host_initialized = false;

	Plug_Shutdown();

	//disconnect server/client/etc
	CL_Disconnect_f();

#ifdef VM_UI
	UI_Stop();
#endif

//	Host_WriteConfiguration ();

	CDAudio_Shutdown ();
	S_Shutdown();
	IN_Shutdown ();
	R_ShutdownRenderer();
#ifdef CL_MASTER
	MasterInfo_Shutdown();
#endif
	CL_FreeDlights();
	CL_FreeVisEdicts();
	M_Shutdown();
#ifndef CLIENTONLY
	SV_Shutdown();
#else
	NET_Shutdown ();
#endif

	Cvar_Shutdown();
	Validation_FlushFileList();

	Cmd_Shutdown();
	Key_Unbindall_f();
	Con_Shutdown();
	Memory_DeInit();

#ifndef CLIENTONLY
	memset(&sv, 0, sizeof(sv));
	memset(&svs, 0, sizeof(svs));
#endif
	Sys_Shutdown();

	FS_Shutdown();
}

#ifdef CLIENTONLY
void SV_EndRedirect (void)
{
}
#endif
