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
#ifdef _WIN32
#include "winsock.h"
#else
#include <netinet/in.h>
#endif

#if defined(_WIN32) && !defined(MINGW) && defined(RGLQUAKE)
#define WINAVI
#endif

#include <ctype.h>

//imap exports
extern void IMAP_CreateConnection(char *servername, char *username, char *password);
extern cvar_t imap_checkfrequency;
extern void IMAP_Think (void);

//pop3
extern void POP3_CreateConnection(char *servername, char *username, char *password);
extern cvar_t pop3_checkfrequency;
extern void POP3_Think (void);


// we need to declare some mouse variables here, because the menu system
// references them even when on a unix system.

qboolean	noclip_anglehack;		// remnant from old quake


cvar_t	rcon_password = {"rcon_password", ""};

cvar_t	rcon_address = {"rcon_address", ""};

cvar_t	cl_timeout = {"cl_timeout", "60"};

cvar_t	cl_shownet = {"cl_shownet","0"};	// can be 0, 1, or 2

cvar_t	cl_sbar		= {"cl_sbar", "0", NULL, CVAR_ARCHIVE};
cvar_t	cl_hudswap	= {"cl_hudswap", "0", NULL, CVAR_ARCHIVE};
cvar_t	cl_maxfps	= {"cl_maxfps", "-1", NULL, CVAR_ARCHIVE};
cvar_t	cl_nopext	= {"cl_nopext", "0", NULL, CVAR_ARCHIVE};

cvar_t	cfg_save_name = {"cfg_save_name", "fteconfig", NULL, CVAR_ARCHIVE};

cvar_t	cl_splitscreen = {"cl_splitscreen", "0"};

cvar_t	lookspring = {"lookspring","0", NULL, CVAR_ARCHIVE};
cvar_t	lookstrafe = {"lookstrafe","0", NULL, CVAR_ARCHIVE};
cvar_t	sensitivity = {"sensitivity","3", NULL, CVAR_ARCHIVE};

cvar_t	m_pitch = {"m_pitch","0.022", NULL, CVAR_ARCHIVE};
cvar_t	m_yaw = {"m_yaw","0.022"};
cvar_t	m_forward = {"m_forward","1"};
cvar_t	m_side = {"m_side","0.8"};

cvar_t	entlatency = {"entlatency", "20"};
cvar_t	cl_predict_players = {"cl_predict_players", "1"};
cvar_t	cl_predict_players2 = {"cl_predict_players2", "1"};
cvar_t	cl_solid_players = {"cl_solid_players", "1"};

cvar_t  localid = {"localid", ""};

static qboolean allowremotecmd = true;

//
// info mirrors
//
cvar_t	password = {"password",			"",			NULL, CVAR_USERINFO};	//this is parhaps slightly dodgy...
cvar_t	spectator = {"spectator",		"",			NULL, CVAR_USERINFO};
cvar_t	name = {"name",					"unnamed",	NULL, CVAR_ARCHIVE | CVAR_USERINFO};
cvar_t	team = {"team",					"",			NULL, CVAR_ARCHIVE | CVAR_USERINFO};
cvar_t	skin = {"skin",					"",			NULL, CVAR_ARCHIVE | CVAR_USERINFO};
cvar_t	topcolor = {"topcolor",			"",			NULL, CVAR_ARCHIVE | CVAR_USERINFO};
cvar_t	bottomcolor = {"bottomcolor",	"",			NULL, CVAR_ARCHIVE | CVAR_USERINFO};
cvar_t	rate = {"rate",					"2500",		NULL, CVAR_ARCHIVE | CVAR_USERINFO};
cvar_t	noaim = {"noaim",				"",			NULL, CVAR_ARCHIVE | CVAR_USERINFO};
cvar_t	msg = {"msg",					"1",		NULL, CVAR_ARCHIVE | CVAR_USERINFO};

cvar_t	cl_item_bobbing = {"cl_model_bobbing", "0"};

cvar_t	requiredownloads = {"requiredownloads","1", NULL, CVAR_ARCHIVE};
cvar_t	cl_standardchat = {"cl_standardchat", "0"};

cvar_t	host_mapname = {"host_mapname", ""};

extern cvar_t cl_hightrack;

char cl_screengroup[] = "Screen options";
char cl_controlgroup[] = "client operation options";
char cl_inputgroup[] = "client input controls";
char cl_predictiongroup[] = "Client side prediction";


client_static_t	cls;
client_state_t	cl;

entity_state_t	cl_baselines[MAX_EDICTS];
efrag_t			cl_efrags[MAX_EFRAGS];
entity_t		cl_static_entities[MAX_STATIC_ENTITIES];
lightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
//lightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
dlight_t		cl_dlights[MAX_DLIGHTS];

// refresh list
// this is double buffered so the last frame
// can be scanned for oldorigins of trailing objects
int				cl_numvisedicts, cl_oldnumvisedicts;
entity_t		*cl_visedicts, *cl_oldvisedicts;
entity_t		cl_visedicts_list[2][MAX_VISEDICTS];

double			connect_time = -1;		// for connection retransmits
int				connect_type = 0;

quakeparms_t host_parms;

qboolean	host_initialized;		// true if into command execution
qboolean	nomaster;

double		host_frametime;
double		realtime;				// without any filtering or bounding
double		oldrealtime;			// last frame run
int			host_framecount;

int			host_hunklevel;

qbyte		*host_basepal;
qbyte		*host_colormap;

cvar_t	host_speeds = {"host_speeds","0"};			// set for running times
cvar_t	show_fps = {"show_fps","0"};			// set for running times
cvar_t	developer = {"developer","0"};

int			fps_count;

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

/*
==================
CL_Quit_f
==================
*/
void CL_Quit_f (void)
{
	if (!isDedicated)
	{
		M_Menu_Quit_f ();
		return;
	}
	CL_Disconnect ();
	Sys_Quit ();
}

/*
=======================
CL_Version_f
======================
*/
void CL_Version_f (void)
{
#ifdef VERSION3PART
#ifdef DISTRIBUTION
	Con_TPrintf (TLC_VERSIONST3, DISTRIBUTION, VERSION, build_number());
#else
	Con_TPrintf (TCL_VERSION3, VERSION, build_number());
#endif
#else
#ifdef DISTRIBUTION
	Con_TPrintf (TLC_VERSIONST2, DISTRIBUTION, VERSION, build_number());
#else
	Con_TPrintf (TLC_VERSION2, VERSION, build_number());
#endif
#endif
	Con_TPrintf (TL_EXEDATETIME, __DATE__, __TIME__);
}


/*
=======================
CL_SendConnectPacket

called by CL_Connect_f and CL_CheckResend
======================
*/
void CL_SendConnectPacket (
#ifdef PROTOCOL_VERSION_FTE
						   int ftepext,
#endif
						   int compressioncrc
						  /*, ...*/)	//dmw new parms
{	
	netadr_t	adr;
	char	data[2048];
	char playerinfo2[MAX_INFO_STRING];
	double t1, t2;
#ifdef PROTOCOL_VERSION_FTE
	int fteprotextsupported=0;
#endif
	int clients;
	int c;

// JACK: Fixed bug where DNS lookups would cause two connects real fast
//       Now, adds lookup time to the connect time.
//		 Should I add it to realtime instead?!?!

	if (cls.state != ca_disconnected)
		return;

	if (cl_nopext.value)	//imagine it's an unenhanced server
	{
		ftepext = 0;
		compressioncrc = 0;
	}

#ifdef PROTOCOL_VERSION_FTE
#ifdef PEXT_SCALE	//dmw - protocol extensions
	fteprotextsupported |= PEXT_SCALE;
#endif
#ifdef PEXT_LIGHTSTYLECOL
	fteprotextsupported |= PEXT_LIGHTSTYLECOL;
#endif
#ifdef PEXT_TRANS
	fteprotextsupported |= PEXT_TRANS;
#endif
#ifdef PEXT_VIEW2	
	fteprotextsupported |= PEXT_VIEW2;
#endif
#ifdef PEXT_BULLETENS
	fteprotextsupported |= PEXT_BULLETENS;
#endif
#ifdef PEXT_ZLIBDL
	fteprotextsupported |= PEXT_ZLIBDL;
#endif
#ifdef PEXT_LIGHTUPDATES
	fteprotextsupported |= PEXT_LIGHTUPDATES;
#endif
#ifdef PEXT_FATNESS
	fteprotextsupported |= PEXT_FATNESS;
#endif
#ifdef PEXT_HLBSP
	fteprotextsupported |= PEXT_HLBSP;
#endif

#ifdef PEXT_Q2BSP
	fteprotextsupported |= PEXT_Q2BSP;
#endif
#ifdef PEXT_Q3BSP
	fteprotextsupported |= PEXT_Q3BSP;
#endif

#ifdef PEXT_TE_BULLET
	fteprotextsupported |= PEXT_TE_BULLET;
#endif
#ifdef PEXT_HULLSIZE
	fteprotextsupported |= PEXT_HULLSIZE;
#endif
#ifdef PEXT_SETVIEW
	fteprotextsupported |= PEXT_SETVIEW;
#endif
#ifdef PEXT_MODELDBL
	fteprotextsupported |= PEXT_MODELDBL;
#endif
#ifdef PEXT_VWEAP
	fteprotextsupported |= PEXT_VWEAP;
#endif
#ifdef PEXT_ORIGINDBL
	fteprotextsupported |= PEXT_ORIGINDBL;
#endif
	fteprotextsupported |= PEXT_SPAWNSTATIC2;
	fteprotextsupported |= PEXT_SEEF1;
	fteprotextsupported |= PEXT_SPLITSCREEN;
	fteprotextsupported |= PEXT_HEXEN2;
	fteprotextsupported |= PEXT_CUSTOMTEMPEFFECTS;
	fteprotextsupported |= PEXT_256PACKETENTITIES;
//	fteprotextsupported |= PEXT_64PLAYERS;
	fteprotextsupported |= PEXT_SHOWPIC;

	fteprotextsupported &= ftepext;

	cls.fteprotocolextensions = fteprotextsupported;
#endif

	t1 = Sys_DoubleTime ();

	if (
#ifdef NQPROT
			!cls.netcon &&
#endif
			!NET_StringToAdr (cls.servername, &adr))
	{
		Con_TPrintf (TLC_BADSERVERADDRESS);
		connect_time = -1;
		return;
	}

	if (!NET_IsClientLegal(&adr))
	{
		Con_TPrintf (TLC_ILLEGALSERVERADDRESS);
		connect_time = -1;
		return;
	}

	if (adr.port == 0)
		adr.port = BigShort (27500);
	t2 = Sys_DoubleTime ();

	cls.resendinfo = false;

	connect_time = realtime+t2-t1;	// for retransmit requests

	cls.qport = Cvar_VariableValue("qport");

//	Info_SetValueForStarKey (cls.userinfo, "*ip", NET_AdrToString(adr), MAX_INFO_STRING);

	Q_strncpyz(playerinfo2, cls.userinfo, sizeof(playerinfo2)-1);
	Info_SetValueForStarKey (playerinfo2, "name", "Second player", MAX_INFO_STRING);

	clients = 1;
	if (cl_splitscreen.value)
	{
		if (adr.type == NA_LOOPBACK)
			clients = cl_splitscreen.value+1;
		else
			Con_Printf("Split screens are still under development\n");
	}

	if (clients < 1)
		clients = 1;
	if (clients > MAX_SPLITS)
		clients = MAX_SPLITS;

#ifdef Q2CLIENT
	if (cls.q2server)	//sorry - too lazy.
		clients = 1;
#endif

	sprintf(data, "%c%c%c%cconnect", 255, 255, 255, 255);
	if (clients>1)	//splitscreen 'connect' command specifies the number of userinfos sent.
		strcat(data, va("%i", clients));
	
#ifdef Q2CLIENT
	if (cls.q2server)
		strcat(data, va(" %i", PROTOCOL_VERSION_Q2));
	else
#endif
		strcat(data, va(" %i", PROTOCOL_VERSION));


	strcat(data, va(" %i %i", cls.qport, cls.challenge));

	//userinfo 0 + zquake extension info.
	strcat(data, va(" \"%s\\*z_ext\\%i\"", cls.userinfo, SUPPORTED_EXTENSIONS));
	for (c = 1; c < clients; c++)
	{
		Info_SetValueForStarKey (playerinfo2, "name", va("%s%i", name.string, c+1), MAX_INFO_STRING);
		strcat(data, va(" \"%s\"", playerinfo2, SUPPORTED_EXTENSIONS));
	}

	strcat(data, "\n");

#ifdef PROTOCOL_VERSION_FTE
	if (ftepext)
		strcat(data, va("0x%x 0x%x\n", PROTOCOL_VERSION_FTE, fteprotextsupported));
#endif

#ifdef HUFFNETWORK
	if (compressioncrc && Huff_CompressionCRC(compressioncrc))
	{
		strcat(data, va("0x%x 0x%x\n", (('H'<<0) + ('U'<<8) + ('F'<<16) + ('F' << 24)), compressioncrc));
		cls.netchan.compress = true;
	}
	else
#endif
		cls.netchan.compress = false;
#ifdef NQPROT
	if (cls.netcon)
	{
		sizebuf_t msg;
		msg.allowoverflow = false;
		msg.cursize = strlen(data);
		msg.data = data;
		msg.maxsize = sizeof(data);
		msg.overflowed = false;
		NET_SendMessage(cls.netcon, &msg);
	}
	else
#endif
		NET_SendPacket (NS_CLIENT, strlen(data), data, adr);

	cl.splitclients = 0;
}

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

#ifndef CLIENTONLY
	if (!cls.state && sv.state)
	{
		Q_strncpyz (cls.servername, "internalserver", sizeof(cls.servername));
		cls.state = ca_disconnected;
#ifdef Q2CLIENT
		if (!svprogfuncs)
			cls.q2server = true;
		else
			cls.q2server = false;
#endif
			
		CL_SendConnectPacket (svs.fteprotocolextensions, false);
		return;
	}
#endif

	if (connect_time == -1)
		return;
	if (cls.state != ca_disconnected)
		return;
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
	if (connect_time && realtime - connect_time < 5.0)
		return;

	t1 = Sys_DoubleTime ();
	if (!NET_StringToAdr (cls.servername, &adr))
	{
		Con_TPrintf (TLC_BADSERVERADDRESS);
		connect_time = -1;
		return;
	}
	if (!NET_IsClientLegal(&adr))
	{
		Con_TPrintf (TLC_ILLEGALSERVERADDRESS);
		connect_time = -1;
		return;
	}

	if (adr.port == 0)
		adr.port = BigShort (27500);
	t2 = Sys_DoubleTime ();

	connect_time = realtime+t2-t1;	// for retransmit requests

	Con_TPrintf (TLC_CONNECTINGTO, cls.servername);
	sprintf (data, "%c%c%c%cgetchallenge\n", 255, 255, 255, 255);
	NET_SendPacket (NS_CLIENT, strlen(data), data, adr);
}

void CL_BeginServerConnect(void)
{
#ifdef NQPROT
	if (cls.netcon)
	{
		NET_Close(cls.netcon);
		cls.netcon = cls.netchan.qsocket = NULL;
	}
#endif
	connect_time = 0;
	connect_type=0;
	CL_CheckForResend();
}
#ifdef NQPROT
void CLNQ_BeginServerConnect(void)
{
	if (cls.netcon)
	{
		NET_Close(cls.netcon);
		cls.netcon = NULL;
	}
	connect_time = 0;
	connect_type=1;
	CL_CheckForResend();
}
#endif
void CL_BeginServerReconnect(void)
{
	if (isDedicated)
	{
		Con_TPrintf (TLC_DEDICATEDCANNOTCONNECT);
		return;
	}
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
	CL_BeginServerConnect();
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
	CLNQ_BeginServerConnect();	
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
	int		i;
	netadr_t	to;

	if (!rcon_password.string)	//FIXME: this is strange...
	{
		Con_TPrintf (TLC_NORCONPASSWORD);
		return;
	}

	message[0] = 255;
	message[1] = 255;
	message[2] = 255;
	message[3] = 255;
	message[4] = 0;

	strcat (message, "rcon ");

	strcat (message, rcon_password.string);
	strcat (message, " ");

	for (i=1 ; i<Cmd_Argc() ; i++)
	{
		strcat (message, Cmd_Argv(i));
		strcat (message, " ");
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
		NET_StringToAdr (rcon_address.string, &to);
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

	S_StopAllSounds (true);

	Cvar_ApplyLatches(CVAR_SERVEROVERRIDE);

	Con_DPrintf ("Clearing memory\n");	
#ifdef PEXT_BULLETENS
	WipeBulletenTextures ();
#endif
	if (!serverrunning || !tolocalserver)
	{
		if (serverrunning)
			SV_UnspawnServer();
		D_FlushCaches ();
		Mod_ClearAll ();
	
		if (host_hunklevel)	// FIXME: check this...			
			Hunk_FreeToLowMark (host_hunklevel);

		Cvar_ApplyLatches(CVAR_LATCH);
	}

	CL_ClearTEnts ();
	CL_ClearCustomTEnts();
	SCR_ShowPic_Clear();

	if (cl.playernum[0] == -1)
	{	//left over from q2 connect.
		Media_PlayFilm("");
	}

// wipe the entire cl structure
	memset (&cl, 0, sizeof(cl));

	SZ_Clear (&cls.netchan.message);

// clear other arrays	
	memset (cl_efrags, 0, sizeof(cl_efrags));
	memset (cl_dlights, 0, sizeof(cl_dlights));
	memset (cl_lightstyle, 0, sizeof(cl_lightstyle));

	memset (cl_baselines, 0, sizeof(cl_baselines));

//
// allocate the efrags and chain together into a free list
//
	cl.free_efrags = cl_efrags;
	for (i=0 ; i<MAX_EFRAGS-1 ; i++)
		cl.free_efrags[i].entnext = &cl.free_efrags[i+1];
	cl.free_efrags[i].entnext = NULL;

	for (i = 0; i < MAX_SPLITS; i++)
		cl.viewheight[i] = DEFAULT_VIEWHEIGHT;
	cl.minpitch = -70;
	cl.maxpitch = 80;
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
	qbyte	final[10];

	connect_time = -1;

	Cvar_ApplyLatches(CVAR_SERVEROVERRIDE);

#ifdef _WIN32
	SetWindowText (mainwindow, "FTE QuakeWorld: disconnected");
#endif

// stop sounds (especially looping!)
	S_StopAllSounds (true);
	
// if running a local server, shut it down
	if (cls.demoplayback != DPB_NONE)
		CL_StopPlayback ();
	else if (cls.state != ca_disconnected)
	{
		if (cls.demorecording)
			CL_Stop_f ();

#ifdef NQPROT
		if (cls.netcon)
		{
			sizebuf_t msg;
			final[0] = clc_disconnect;			
			msg.data = final;
			msg.cursize = 1;
			msg.maxsize = 10;
			NET_SendMessage(cls.netcon, &msg);
			NET_SendMessage(cls.netcon, &msg);
			NET_SendMessage(cls.netcon, &msg);
		}
		else
#endif
		{
#ifdef Q2CLIENT
			if (cls.q2server)
			{
				final[0] = clcq2_stringcmd;
				strcpy (final+1, "disconnect");
			}
			else
#endif
			{
				final[0] = clc_stringcmd;
				strcpy (final+1, "drop");
			}
			Netchan_Transmit (&cls.netchan, strlen(final)+1, final);
			Netchan_Transmit (&cls.netchan, strlen(final)+1, final);
			Netchan_Transmit (&cls.netchan, strlen(final)+1, final);
		}

		cls.state = ca_disconnected;

		cls.demoplayback = DPB_NONE;
		cls.demorecording = cls.timedemo = false;

#ifndef CLIENTONLY
	//running a server, and it's our own
		if (serverrunning && !tolocalserver)SV_UnspawnServer();
#endif
	}
	Cam_Reset();

	if (cl.worldmodel)
	{	
#if defined(RUNTIMELIGHTING) && defined(RGLQUAKE)
		extern model_t *lightmodel;
		lightmodel = NULL;
#endif

		cl.worldmodel->needload=true;
		cl.worldmodel=NULL;
	}

	if (cls.downloadqw)
	{
		fclose(cls.downloadqw);
		cls.downloadqw = NULL;
		if (cls.downloadmethod == DL_QW)
			cls.downloadmethod = DL_NONE;
	}
	if (!cls.downloadmethod)
		*cls.downloadname = '\0';
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

	COM_FlushTempoaryPacks();

#ifdef NQPROT
	cls.signon=0;
	NET_Close(cls.netcon);
	cls.netcon = NULL;
#endif
	CL_StopUpload();

	if (!isDedicated)
		SCR_EndLoadingPlaque();
}

#undef serverrunning
#undef tolocalserver

void CL_Disconnect_f (void)
{
	CL_Disconnect ();

#ifndef CLIENTONLY
	if (sv.state)
		SV_UnspawnServer();
#endif

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

#ifndef CLIENTONLY
	if (isDedicated)
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
			Info_Print (cl.players[i].userinfo);
			return;
		}
	}
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

void CL_Color_f (void)
{
	// just for quake compatability...
	int		top, bottom;
	char	num[16];

	qboolean server_owns_colour;

	if (Cmd_Argc() == 1)
	{
		Con_TPrintf (TLC_COLOURCURRENT,
			Info_ValueForKey (cls.userinfo, "topcolor"),
			Info_ValueForKey (cls.userinfo, "bottomcolor") );
		Con_TPrintf (TLC_SYNTAX_COLOUR);
		return;
	}

	if (Cmd_FromServer())
		server_owns_colour = true;
	else
		server_owns_colour = false;


	if (Cmd_Argc() == 2)
		top = bottom = atoi(Cmd_Argv(1));
	else
	{
		top = atoi(Cmd_Argv(1));
		bottom = atoi(Cmd_Argv(2));
	}
	
	top &= 15;
	if (top > 13)
		top = 13;
	bottom &= 15;
	if (bottom > 13)
		bottom = 13;
	
	sprintf (num, "%i", top);
	if (top == 0)
		*num = '\0';
	if (Cmd_ExecLevel>RESTRICT_SERVER) //colour command came from server for a split client
		Cbuf_AddText(va("cmd %i setinfo topcolor %i\n", Cmd_ExecLevel-RESTRICT_SERVER-1, top), Cmd_ExecLevel);
//	else if (server_owns_colour)
//		Cvar_LockFromServer(&topcolor, num);
	else
		Cvar_Set (&topcolor, num);
	sprintf (num, "%i", bottom);
	if (bottom == 0)
		*num = '\0';
	if (Cmd_ExecLevel>RESTRICT_SERVER) //colour command came from server for a split client
		Cbuf_AddText(va("cmd %i setinfo bottomcolor %i\n", Cmd_ExecLevel-RESTRICT_SERVER-1, bottom), Cmd_ExecLevel);
	else if (server_owns_colour)
		Cvar_LockFromServer(&bottomcolor, num);
	else
		Cvar_Set (&bottomcolor, num);
#ifdef NQPROT
	if (cls.netcon)
		Cmd_ForwardToServer();
#endif
}


void CL_CheckServerInfo(void)
{
	char *s;
	unsigned int allowed;

	cl.teamplay = atoi(Info_ValueForKey(cl.serverinfo, "teamplay"));
	cl.deathmatch = atoi(Info_ValueForKey(cl.serverinfo, "deathmatch"));

	cls.allow_cheats = false;
	cls.allow_semicheats=true;
	cls.allow_rearview=false;
	cls.allow_watervis=false;
	cls.allow_skyboxes=false;
	cls.allow_mirrors=false;
	cls.allow_shaders=false;
	cls.allow_luma=true;
	cls.allow_bump=false;
#ifdef FISH
	cls.allow_fish=false;
#endif
	cls.allow_fbskins = 0;
//	cls.allow_overbrightlight;
	if (atoi(Info_ValueForKey(cl.serverinfo, "rearview")))
		cls.allow_rearview=true;

	if (atoi(Info_ValueForKey(cl.serverinfo, "watervis")))
		cls.allow_watervis=true;

	if (atoi(Info_ValueForKey(cl.serverinfo, "allow_skybox")) || atoi(Info_ValueForKey(cl.serverinfo, "allow_skyboxes")))
		cls.allow_skyboxes=true;

	if (atoi(Info_ValueForKey(cl.serverinfo, "mirrors")))
		cls.allow_mirrors=true;

	if (atoi(Info_ValueForKey(cl.serverinfo, "allow_shaders")))
		cls.allow_shaders=true;

	if (!atoi(Info_ValueForKey(cl.serverinfo, "allow_luma")))
		cls.allow_luma=false;

	if (atoi(Info_ValueForKey(cl.serverinfo, "allow_lmgamma")))
		cls.allow_lightmapgamma=true;

	s = Info_ValueForKey(cl.serverinfo, "allow_bump");
	if (atoi(s) || !*s)	//admin doesn't care.
		cls.allow_bump=true;
#ifdef FISH
	if (atoi(Info_ValueForKey(cl.serverinfo, "allow_fish")))
		cls.allow_fish=true;			
#endif

	s = Info_ValueForKey(cl.serverinfo, "fbskins");
	if (*s)
		cls.allow_fbskins = atof(s);
	else
		cls.allow_fbskins = 0.3;

	s = Info_ValueForKey(cl.serverinfo, "*cheats");
	if (!stricmp(s, "on"))
		cls.allow_cheats = true;

	s = Info_ValueForKey(cl.serverinfo, "strict");
	if (*s && strcmp(s, "0"))
	{
		cls.allow_semicheats = false;
		cls.allow_cheats	= false;
	}


	cls.maxfps = atof(Info_ValueForKey(cl.serverinfo, "maxfps"));

	if (!atoi(Info_ValueForKey(cl.serverinfo, "deathmatch")))
		cls.gamemode = GAME_COOP;
	else
		cls.gamemode = GAME_DEATHMATCH;

	cls.z_ext = atoi(Info_ValueForKey(cl.serverinfo, "*z_ext"));

	// movement vars for prediction
	cl.bunnyspeedcap = Q_atof(Info_ValueForKey(cl.serverinfo, "pm_bunnyspeedcap"));
	movevars.slidefix = (Q_atof(Info_ValueForKey(cl.serverinfo, "pm_slidefix")) != 0);
	movevars.airstep = (Q_atof(Info_ValueForKey(cl.serverinfo, "pm_airstep")) != 0);
	movevars.ktjump = Q_atof(Info_ValueForKey(cl.serverinfo, "pm_ktjump"));

	// Initialize cl.maxpitch & cl.minpitch
	s = (cls.z_ext & Z_EXT_PITCHLIMITS) ? Info_ValueForKey (cl.serverinfo, "maxpitch") : "";
	cl.maxpitch = *s ? Q_atof(s) : 80.0f;
	s = (cls.z_ext & Z_EXT_PITCHLIMITS) ? Info_ValueForKey (cl.serverinfo, "minpitch") : "";
	cl.minpitch = *s ? Q_atof(s) : -70.0f;

	allowed = atoi(Info_ValueForKey(cl.serverinfo, "allow"));
	if (allowed & 1)
		cls.allow_watervis = true;
	if (allowed & 2)
		cls.allow_rearview = true;
	if (allowed & 4)
		cls.allow_skyboxes = true;
	if (allowed & 8)
		cls.allow_mirrors = true;
	if (allowed & 16)
		cls.allow_shaders = true;
	if (allowed & 32)
		cls.allow_luma = true;
	if (allowed & 64)
		cls.allow_bump = true;
#ifdef FISH
	if (allowed & 128)
		cls.allow_fish = true;
#endif
	if (allowed & 256)
		cls.allow_lightmapgamma = true;
	if (allowed & 512)
		cls.allow_cheats = true;

	if (cls.allow_semicheats)
		cls.allow_anyparticles = true;
	else
		cls.allow_anyparticles = false;

	Cvar_ForceCheatVars(cls.allow_semicheats, cls.allow_cheats);
	
}
/*
==================
CL_FullServerinfo_f

Sent by server when serverinfo changes
==================
*/
void CL_FullServerinfo_f (void)
{
	char *p;
	float v;

	if (Cmd_Argc() != 2)
	{
		Con_TPrintf (TLC_SYNTAX_FULLSERVERINFO);
		return;
	}

	strcpy (cl.serverinfo, Cmd_Argv(1));

	if ((p = Info_ValueForKey(cl.serverinfo, "*version")) && *p) {
		v = Q_atof(p);
		if (v) {
			if (!server_version)
				Con_TPrintf (TLC_SERVER_VERSION, v);
			server_version = v;
		}
	}
	CL_CheckServerInfo();
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

		Info_SetValueForKey (cls.userinfo, key, value, MAX_INFO_STRING);
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
	if (Cmd_Argc() == 1)
	{
		Info_Print (cls.userinfo);
		return;
	}
	if (Cmd_Argc() != 3)
	{
		Con_TPrintf (TLC_SYNTAX_SETINFO);
		return;
	}
	if (!stricmp(Cmd_Argv(1), pmodel_name) || !strcmp(Cmd_Argv(1), emodel_name))
		return;

	var = Cvar_FindVar(Cmd_Argv(1));
	if (var && (var->flags & CVAR_USERINFO))
	{	//get the cvar code to set it. the server might have locked it.
		Cvar_Set(var, Cmd_Argv(2));
		return;
	}

	Info_SetValueForKey (cls.userinfo, Cmd_Argv(1), Cmd_Argv(2), MAX_INFO_STRING);
	if (cls.state >= ca_connected)
	{
#ifdef Q2CLIENT
		if (cls.q2server)
		{
			MSG_WriteByte (&cls.netchan.message, clcq2_userinfo);
			MSG_WriteString (&cls.netchan.message, cls.userinfo);
		}
		else
#endif
			Cmd_ForwardToServer ();
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

	if (!NET_StringToAdr (Cmd_Argv(1), &adr))
	{
		Con_TPrintf (TLC_BADADDRESS);
		return;
	}

	if (Cmd_FromServer())
	{
		if (adr.type == NA_IP)
			if (adr.ip[0] == 127)
			if (adr.ip[1] == 0)
			if (adr.ip[2] == 0)
			if (adr.ip[3] == 1)
			{
				Con_Printf ("^b^1Server is broken. Ignoring 'realip' packet request\n");
				return;
			}
	}

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
		else
			*out++ = in[i];
	}
	*out = 0;

	NET_SendPacket (NS_CLIENT, out-send, send, adr);
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

	sprintf (str,"playdemo %s\n", cls.demos[cls.demonum]);
	Cbuf_InsertText (str, RESTRICT_LOCAL);
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
	Con_Printf ("%i demo(s) in loop\n", c);

	for (i=1 ; i<c+1 ; i++)
		Q_strncpyz (cls.demos[i-1], Cmd_Argv(i), sizeof(cls.demos[0]));

	if (!sv.state && cls.demonum != -1 && cls.demoplayback==DPB_NONE && !media_filmtype)
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
	if (cls.downloadqw)  // don't change when downloading
		return;

	SCR_BeginLoadingPlaque();

	S_StopAllSounds (true);
	cl.intermission = 0;
	cls.state = ca_connected;	// not active anymore, but not disconnected
	Con_TPrintf (TLC_CHANGINGMAP);

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
	if (cls.netcon)
	{
		CL_Changing_f();
		return;
	}
#endif
	S_StopAllSounds (true);

	if (cls.state == ca_connected)
	{
		Con_TPrintf (TLC_RECONNECTING);
		MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");
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

    MSG_BeginReading ();
    MSG_ReadLong ();        // skip the -1

	c = MSG_ReadByte ();

	if (cls.demoplayback == DPB_NONE)
		Con_TPrintf (TL_ST_COLON, NET_AdrToString (net_from));
//	Con_DPrintf ("%s", net_message.data + 5);

	if (c == S2C_CHALLENGE) {
		unsigned long pext = 0, huffcrc=0;
		Con_TPrintf (TLC_S2C_CHALLENGE);

		s = MSG_ReadString ();
		COM_Parse(s);
#ifdef Q2CLIENT
		if (!strcmp(com_token, "hallenge"))
		{
			cls.q2server = true;
			s+=9;
		}
		else if (!strcmp(com_token, "lient_connect"))
		{
			goto client_connect;
		}
		else
			cls.q2server = false;
#endif
		cls.challenge = atoi(s);

		for(;;)
		{
			c = MSG_ReadLong ();
			if (msg_badread)
				break;
			if (c == PROTOCOL_VERSION_FTE)
				pext = MSG_ReadLong ();
#ifdef HUFFNETWORK
			else if (c == (('H'<<0) + ('U'<<8) + ('F'<<16) + ('F' << 24)))
				huffcrc = MSG_ReadLong ();
#endif
			//else if (c == PROTOCOL_VERSION_...)
			else
				MSG_ReadLong ();
		}
		CL_SendConnectPacket (pext, huffcrc/*, ...*/);
		return;
	}
#ifdef Q2CLIENT
	if (cls.q2server)
	{
		char *nl;
		msg_readcount--;
		c = msg_readcount;
		s = MSG_ReadString ();
		nl = strchr(s, '\n');
		if (nl)
		{
			msg_readcount = c + nl-s + 1;
			*nl = '\0';
		}

		if (!strcmp(s, "print"))
		{
			Con_TPrintf (TLC_A2C_PRINT);

			s = MSG_ReadString ();
			Con_Print (s);
			return;
		}
		else if (!strcmp(s, "client_connect"))
		{
			goto client_connect;
		}
		else
		{
			Con_TPrintf (TLC_Q2CONLESSPACKET_UNKNOWN, s);
			msg_readcount = c;
			c = MSG_ReadByte();
		}
	}
#endif
		
	if (c == S2C_CONNECTION)
	{
		int compress;
#ifdef Q2CLIENT
client_connect:	//fixme: make function
#endif
		Con_TPrintf (TLC_GOTCONNECTION);
		if (cls.state >= ca_connected)
		{
			if (cls.demoplayback == DPB_NONE)
				Con_TPrintf (TLC_DUPCONNECTION);
			return;
		}
		compress = cls.netchan.compress;
		Netchan_Setup (NS_CLIENT, &cls.netchan, net_from, cls.qport);
		cls.netchan.compress = compress;
#ifdef NQPROT
		cls.netchan.qsocket = cls.netcon;
#endif
		MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");
		cls.state = ca_connected;
		Con_TPrintf (TLC_CONNECTED);
		allowremotecmd = false; // localid required now for remote cmds
		return;
	}
	// remote command from gui front end
	if (c == A2C_CLIENT_COMMAND)
	{
		char	cmdtext[2048];

		Con_TPrintf (TLC_CONLESS_CONCMD);
		if (net_from.type != net_local_ipadr.type
			|| ((*(unsigned *)net_from.ip != *(unsigned *)net_local_ipadr.ip) && (*(unsigned *)net_from.ip != htonl(INADDR_LOOPBACK))))
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
				Con_TPrintf (TL_LINEBREAK_EQUALS);
				Con_TPrintf (TLC_LOCALID_NOTSET);
				Con_TPrintf (TL_LINEBREAK_EQUALS);
				return;
			}
			Con_TPrintf (TL_LINEBREAK_EQUALS);
			Con_TPrintf (TLC_LOCALID_BAD,
				s, localid.string);
			Con_TPrintf (TL_LINEBREAK_EQUALS);
			Cvar_Set(&localid, "");
			return;
		}

		Cbuf_AddText (cmdtext, RESTRICT_LOCAL);
		allowremotecmd = false;
		return;
	}
	// print command from somewhere
	if (c == A2C_PRINT)
	{
		Con_TPrintf (TLC_A2C_PRINT);

		s = MSG_ReadString ();
		Con_Print (s);
		return;
	}

	// ping from somewhere
	if (c == A2A_PING)
	{
		char	data[6];

		Con_TPrintf (TLC_A2A_PING);

		data[0] = 0xff;
		data[1] = 0xff;
		data[2] = 0xff;
		data[3] = 0xff;
		data[4] = A2A_ACK;
		data[5] = 0;
		
		NET_SendPacket (NS_CLIENT, 6, &data, net_from);
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


/*
=================
CL_ReadPackets
=================
*/
void CL_ReadPackets (void)
{
//	while (NET_GetPacket ())
	while (CL_GetMessage())
	{
#ifdef NQPROT
		if (cls.demoplayback == DPB_NETQUAKE)
		{
			cls.netchan.last_received = realtime;
			CLNQ_ParseServerMessage ();
			continue;
		}
#endif
#ifdef Q2CLIENT
		if (cls.demoplayback == DPB_QUAKE2)
		{
			MSG_BeginReading ();
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

		if (net_message.cursize < 8 && cls.demoplayback != DPB_MVD) //MVDs don't have the whole sequence header thing going on
		{
			Con_TPrintf (TL_RUNTPACKET,NET_AdrToString(net_from));
			continue;
		}

		//
		// packet from server
		//
		if (!cls.demoplayback && 
			!NET_CompareAdr (net_from, cls.netchan.remote_address))
		{
			Con_DPrintf ("%s:sequenced packet without connection\n"
				,NET_AdrToString(net_from));
			continue;
		}
		if (cls.state == ca_disconnected)
			continue;	//ignore it. We arn't connected.

		if (cls.demoplayback == DPB_MVD)
			MSG_BeginReading();
		else if (!Netchan_Process(&cls.netchan))
			continue;		// wasn't accepted for some reason

#ifdef Q2CLIENT
		if (cls.q2server)
			CLQ2_ParseServerMessage ();
		else
#endif
			CL_ParseServerMessage ();

//		if (cls.demoplayback && cls.state >= ca_active && !CL_DemoBehind())
//			return;
	}
#ifdef NQPROT
	while(CLNQ_GetMessage()>0)
	{
		//allow qw protocol over nq transports
		if (cls.netcon->qwprotocol)
		{			
			if (*(int *)net_message.data == -1)
			{
				CL_ConnectionlessPacket ();
				continue;
			}

			if (net_message.cursize < 8)
			{
				Con_TPrintf (TL_RUNTPACKET,NET_AdrToString(net_from));
				continue;
			}

			if (cls.state == ca_disconnected)
				continue;	//ignore it. We arn't connected.
			
			memcpy(&cls.netchan.remote_address, &net_from, sizeof(net_from));
			if (!Netchan_Process(&cls.netchan))			
				continue;		// wasn't accepted for some reason
//			MSG_BeginReading();
//			MSG_ReadLong();
//			MSG_ReadLong();

#ifdef Q2CLIENT
			if (cls.q2server)
				CLQ2_ParseServerMessage ();
			else
#endif
				CL_ParseServerMessage ();
		}
		else
		{
			cls.netchan.last_received = realtime;
			CLNQ_ParseServerMessage ();
		}
	}
#endif

	//
	// check timeout
	//
	if (cls.state >= ca_connected
	 && realtime - cls.netchan.last_received > cl_timeout.value)
	{
		Con_TPrintf (TLC_SERVERTIMEOUT);
		CL_Disconnect ();
		return;
	}
	
	if (cls.demoplayback == DPB_MVD)
		MVD_Interpolate();
}

//=============================================================================

/*
=====================
CL_Download_f
=====================
*/
void CL_Download_f (void)
{
	char *p, *q, *url;

	url = Cmd_Argv(1);

#ifdef WEBCLIENT
	if (!strnicmp(url, "http://", 7) || !strnicmp(url, "ftp://", 6))
	{
		if (Cmd_FromServer())
			return;
		HTTP_CL_Get(url, Cmd_Argv(2));//"test.txt");
		return;
	}
#endif

	if (!strnicmp(url, "qw://", 5) || !strnicmp(url, "q2://", 5))
	{
		url += 5;
	}

	if (cls.state == ca_disconnected || cls.demoplayback)
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

	if (Cmd_FromServer())	//mark server specified downloads.
	{
		if (!strncmp(url, "game", 4) || !strcmp(url, "progs.dat") || !strcmp(url, "qwprogs.dat") || strstr(url, ".."))
		{	//yes, I know the user can use a different progs from the one that is specified. If you leave it blank there will be no problem. (server isn't allowed to stuff progs cvar)
			Con_Printf("Ignoring stuffed download of \"%s\" due to possible security risk\n", url);
			return;
		}

		cls.downloadtype = dl_singlestuffed;

		CL_CheckOrDownloadFile(url, false);
		return;
	}
	else
	{
		cls.downloadtype = dl_single;
	}

	

	_snprintf (cls.downloadname, sizeof(cls.downloadname), "%s/%s", com_gamedir, url);

	p = cls.downloadname;
	for (;;)
	{
		if ((q = strchr(p, '/')) != NULL)
		{
			*q = 0;
			Sys_mkdir(cls.downloadname);
			*q = '/';
			p = q + 1;
		} else
			break;
	}

	strcpy(cls.downloadtempname, cls.downloadname);
//	cls.downloadqw = fopen (cls.downloadname, "wb");
	cls.downloadmethod = DL_QW;


	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	SZ_Print (&cls.netchan.message, va("download %s\n",url));
}

#ifdef _WINDOWS
#include <windows.h>
/*
=================
CL_Minimize_f
=================
*/
void CL_Windows_f (void) {
//	if (modestate == MS_WINDOWED)
//		ShowWindow(mainwindow, SW_MINIMIZE);
//	else
		SendMessage(mainwindow, WM_SYSKEYUP, VK_TAB, 1 | (0x0F << 16) | (1<<29));
}
#endif

#ifndef CLIENTONLY
void CL_ServerInfo_f(void)
{
	if (!sv.state && cls.state)
		Cmd_ForwardToServer ();
	else
	{
		SV_Serverinfo_f();	//allow it to be set... (whoops)
	}
}
#endif


#ifdef WEBCLIENT
void CL_FTP_f(void)
{	
	FTP_Client_Command(Cmd_Args()); 
}
void CL_IRC_f(void)
{	
	IRC_Command(Cmd_Args()); 
}
#endif
#ifdef EMAILCLIENT
void CL_IMAPPoll_f(void)
{	
	IMAP_CreateConnection(Cmd_Argv(1), Cmd_Argv(2), Cmd_Argv(3)); 
}

void CL_POP3Poll_f(void)
{	
	POP3_CreateConnection(Cmd_Argv(1), Cmd_Argv(2), Cmd_Argv(3)); 
}
#endif

/*
=================
CL_Init
=================
*/
void CL_Init (void)
{
	extern void CL_Say_f (void);
	extern	cvar_t		baseskin;
	extern	cvar_t		noskins;
	char st[80];

	cls.state = ca_disconnected;

#ifdef VERSION3PART
	sprintf (st, "%s %4.3f-%04d", DISTRIBUTION, VERSION, build_number());
#else
	sprintf (st, "%s %4.2f-%04d", DISTRIBUTION, VERSION, build_number());
#endif
	sprintf (st, "%s", DISTRIBUTION);
	Info_SetValueForStarKey (cls.userinfo, "*ver", st, MAX_INFO_STRING);

	InitValidation();
#ifdef CLPROGS
	CLPR_Init();
#endif

	CL_InitInput ();
	CL_InitTEnts ();
	CL_InitPrediction ();
	CL_InitCam ();
	PM_Init ();
	TP_Init();
	
//
// register our commands
//
	Cvar_Register (&show_fps, cl_screengroup);
	Cvar_Register (&host_speeds, cl_controlgroup);
	Cvar_Register (&developer, cl_controlgroup);

	Cvar_Register (&cfg_save_name, cl_controlgroup);

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
	Cvar_Register (&cl_hudswap,	cl_screengroup);
	Cvar_Register (&cl_maxfps,	cl_screengroup);
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

	Cvar_Register (&entlatency,	cl_predictiongroup);
	Cvar_Register (&cl_predict_players2,	cl_predictiongroup);
	Cvar_Register (&cl_predict_players,	cl_predictiongroup);
	Cvar_Register (&cl_solid_players,	cl_predictiongroup);

	Cvar_Register (&localid,	cl_controlgroup);

	Cvar_Register (&baseskin,	"Teamplay");
	Cvar_Register (&noskins,	"Teamplay");

	Cvar_Register (&cl_item_bobbing, "Item effects");

	//
	// info mirrors
	//
	Cvar_Register (&name,	cl_controlgroup);
	Cvar_Register (&password,	cl_controlgroup);
	Cvar_Register (&spectator,	cl_controlgroup);
	Cvar_Register (&skin,	cl_controlgroup);
	Cvar_Register (&team,	cl_controlgroup);
	Cvar_Register (&topcolor,	cl_controlgroup);
	Cvar_Register (&bottomcolor,	cl_controlgroup);
	Cvar_Register (&rate,	cl_controlgroup);
	Cvar_Register (&msg,	cl_controlgroup);
	Cvar_Register (&noaim,	cl_controlgroup);

	Cvar_Register (&requiredownloads,	cl_controlgroup);
	Cvar_Register (&cl_standardchat,	cl_controlgroup);
	Cvar_Register (&cl_nopext, cl_controlgroup);
	Cvar_Register (&cl_splitscreen, cl_controlgroup);

	Cvar_Register (&host_mapname,		"Scripting");

#ifdef IRCCLIENT
	Cmd_AddCommand ("irc", CL_IRC_f);
#endif
#ifdef WEBCLIENT
	Cmd_AddCommand ("ftp", CL_FTP_f);
#endif

#ifdef EMAILCLIENT
	Cvar_Register (&imap_checkfrequency,	"Email notifications");
	Cmd_AddCommand ("imapaccount", CL_IMAPPoll_f);

	Cvar_Register (&pop3_checkfrequency,	"Email notifications");
	Cmd_AddCommand ("pop3account", CL_POP3Poll_f);
#endif

	Cmd_AddCommand ("version", CL_Version_f);

	Cmd_AddCommand ("changing", CL_Changing_f);
	Cmd_AddCommand ("disconnect", CL_Disconnect_f);
	Cmd_AddCommand ("record", CL_Record_f);
	Cmd_AddCommand ("rerecord", CL_ReRecord_f);
	Cmd_AddCommand ("stop", CL_Stop_f);
	Cmd_AddCommand ("playdemo", CL_PlayDemo_f);
	Cmd_AddCommand ("timedemo", CL_TimeDemo_f);

	Cmd_AddCommand ("startdemos", CL_Startdemos_f);
	Cmd_AddCommand ("demos", CL_Demos_f);
	Cmd_AddCommand ("stopdemo", CL_Stopdemo_f);

	Cmd_AddCommand ("skins", Skin_Skins_f);
	Cmd_AddCommand ("allskins", Skin_AllSkins_f);

	Cmd_AddCommand ("quit", CL_Quit_f);

	Cmd_AddCommand ("connect", CL_Connect_f);
#ifdef NQPROT
	Cmd_AddCommand ("nqconnect", CLNQ_Connect_f);
#endif
	Cmd_AddCommand ("reconnect", CL_Reconnect_f);

	Cmd_AddCommand ("rcon", CL_Rcon_f);
	Cmd_AddCommand ("packet", CL_Packet_f);
	Cmd_AddCommand ("user", CL_User_f);
	Cmd_AddCommand ("users", CL_Users_f);

	Cmd_AddCommand ("setinfo", CL_SetInfo_f);
	Cmd_AddCommand ("fullinfo", CL_FullInfo_f);
	Cmd_AddCommand ("fullserverinfo", CL_FullServerinfo_f);

	Cmd_AddCommand ("color", CL_Color_f);
	Cmd_AddCommand ("download", CL_Download_f);

	Cmd_AddCommand ("nextul", CL_NextUpload);
	Cmd_AddCommand ("stopul", CL_StopUpload);

//
// forward to server commands
//
	Cmd_AddCommand ("god", NULL);	//cheats
	Cmd_AddCommand ("give", NULL);
	Cmd_AddCommand ("noclip", NULL);
	Cmd_AddCommand ("fly", NULL);
	Cmd_AddCommand ("setpos", NULL);

	Cmd_AddCommand ("topten", NULL);

	Cmd_AddCommand ("kill", NULL);
	Cmd_AddCommand ("pause", NULL);
	Cmd_AddCommand ("say", CL_Say_f);
	Cmd_AddCommand ("sayone", CL_Say_f);
	Cmd_AddCommand ("say_team", CL_Say_f);
#ifdef CLIENTONLY
	Cmd_AddCommand ("serverinfo", NULL);
#else
	Cmd_AddCommand ("serverinfo", CL_ServerInfo_f);
#endif

//
//  Windows commands
//
#ifdef _WINDOWS
	Cmd_AddCommand ("windows", CL_Windows_f);
#endif
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
	_vsnprintf (string,sizeof(string)-1, message,argptr);
	va_end (argptr);
	Con_TPrintf (TL_NL);
	Con_TPrintf (TL_LINEBREAK_EQUALS);
	Con_TPrintf (TLC_CLIENTCON_ERROR_ENDGAME,string);
	Con_TPrintf (TL_LINEBREAK_EQUALS);
	Con_TPrintf (TL_NL);

	SCR_EndLoadingPlaque();
	
	CL_Disconnect ();

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
	_vsnprintf (string,sizeof(string)-1, error,argptr);
	va_end (argptr);
	Con_TPrintf (TLC_HOSTFATALERROR,string);
	
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
	FILE	*f;

	if (host_initialized && cfg_save_name.string && *cfg_save_name.string)
	{
		if (strchr(cfg_save_name.string, '.'))
		{
			Con_TPrintf (TLC_CONFIGCFG_WRITEFAILED);
			return;
		}

		f = fopen (va("%s/%s.cfg",com_gamedir, cfg_save_name.string), "w");
		if (!f)
		{
			Con_TPrintf (TLC_CONFIGCFG_WRITEFAILED);
			return;
		}
		
		Key_WriteBindings (f);
		Cvar_WriteVariables (f, false);

		fclose (f);
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
===================
CL_FilterTime

Returns false if the time is too short to run a frame
===================
*/
#define bound(n,v,x) v<n?n:(v>x?x:v)
qboolean CL_Net_FilterTime (double time);
qboolean CL_FilterTime (double time)
{
	float fps;
//	float fpscap;

	if (cls.timedemo) 
		return true;

//	if (cls.demoplayback)
//	{
		if (!cl_maxfps.value)
			return true;
		if (cl_maxfps.value < 0)
			return CL_Net_FilterTime(time*1000); 
		fps = max (30.0, cl_maxfps.value);
/*	}
	else
	{
		fpscap = cls.maxfps ? max (30.0, cls.maxfps) : 0x7fff;
	
		if (cl_maxfps.value)
			fps = bound (10.0, cl_maxfps.value, fpscap);
		else
		{
//			if (com_serveractive)
//				fps = fpscap;
//			else
				fps = bound (30.0, rate.value/80.0, fpscap);
		}
	}
*/
	if (time < 1.0 / fps)
		return false;

	return true;
}



/*
==================
Host_Frame

Runs all active servers
==================
*/
#if defined(WINAVI) && !defined(NOMEDIA)
extern float recordavi_frametime;
extern qboolean recordingdemo;
#endif

int		nopacketcount;
void SNDDMA_SetUnderWater(qboolean underwater);
void Host_Frame (float time)
{
	static double		time1 = 0;
	static double		time2 = 0;
	static double		time3 = 0;
	int			pass1, pass2, pass3;
//	float fps;
	if (setjmp (host_abort) )
		return;			// something bad happened, or the server disconnected

#if defined(WINAVI) && !defined(NOMEDIA)
	if (cls.demoplayback && recordingdemo)
		time = recordavi_frametime;
#endif

#ifndef CLIENTONLY
	SV_Frame(time);
#endif

#ifdef WEBCLIENT
	FTP_ClientThink();
	HTTP_CL_Think();
#endif
#ifdef IRCCLIENT
	IRC_Frame();
#endif
#ifdef EMAILCLIENT
	IMAP_Think();
	POP3_Think();
#endif

	// decide the simulation time
	realtime += time;
	if (oldrealtime > realtime)
		oldrealtime = 0;


#if defined(NQPROT) || defined(Q2CLIENT)
#if defined(NQPROT) && defined(Q2CLIENT)
	if (cls.q2server || cls.demoplayback == DPB_NETQUAKE)
#elif defined(NQPROT)
	if (cls.demoplayback == DPB_NETQUAKE)
#elif defined(Q2CLIENT)
	if (cls.q2server)
#endif
		cl.time += time;
#endif



#ifdef VOICECHAT
	CLVC_Poll();
#endif

/*
	if (cl_maxfps.value)
		fps = cl_maxfps.value;//max(30.0, min(cl_maxfps.value, 72.0));
	else
		fps = max(30.0, min(rate.value/80.0, 72.0));

	if (!cls.timedemo && realtime - oldrealtime < 1.0/fps)
		return;			// framerate is too high

	*/
	if (!CL_FilterTime(realtime - oldrealtime))
	{
		Mod_Think();
		return;
	}

	host_frametime = realtime - oldrealtime;
	oldrealtime = realtime;
//	if (host_frametime > 0.2)
//		host_frametime = 0.2;
		
	// get new key events
	Sys_SendKeyEvents ();

	// allow mice or other external controllers to add commands
	IN_Commands ();

	// process console commands
	Cbuf_Execute ();

	if (isDedicated)	//someone changed it.
		return;

#ifdef NQPROT
	NET_Poll();
#endif

	if (cls.downloadtype == dl_none && !*cls.downloadname && cl.downloadlist)
	{
CL_RequestNextDownload();
	}

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
		CL_SendCmd ();

		if (cl.worldmodel)
		{
			//work out which packet entities are solid
			CL_SetSolidEntities ();

			// Set up prediction for other players
			CL_SetUpPlayerPrediction(false);

			// do client side motion prediction
			CL_PredictMove ();

			// Set up prediction for other players
			CL_SetUpPlayerPrediction(true);

			// build a refresh entity list
			CL_EmitEntities ();
		}
	}

	// update video
	if (host_speeds.value)
		time1 = Sys_DoubleTime ();

	if (SCR_UpdateScreen)
	{
		extern mleaf_t	*r_viewleaf;
		SCR_UpdateScreen ();
		if (cls.state >= ca_active && r_viewleaf)
			SNDDMA_SetUnderWater(r_viewleaf->contents <= Q1CONTENTS_WATER);
		else
			SNDDMA_SetUnderWater(false);
	}

	if (host_speeds.value)
		time2 = Sys_DoubleTime ();
		
	// update audio
	if (cls.state == ca_active)
	{
		S_Update (r_origin, vpn, vright, vup);
		CL_DecayLights ();
	}
	else
		S_Update (vec3_origin, vec3_origin, vec3_origin, vec3_origin);

	CDAudio_Update();

	if (host_speeds.value)
	{
		pass1 = (time1 - time3)*1000;
		time3 = Sys_DoubleTime ();
		pass2 = (time2 - time1)*1000;
		pass3 = (time3 - time2)*1000;
		Con_TPrintf (TLC_HOSTSPEEDSOUTPUT,
					pass1+pass2+pass3, pass1, pass2, pass3);
	}

	host_framecount++;
	fps_count++;

	IN_Commands ();

	// process console commands
	Cbuf_Execute ();

#ifdef NQPROT
	NET_Poll();
#endif

	if (cls.downloadtype == dl_none && !*cls.downloadname && cl.downloadlist)
	{
CL_RequestNextDownload();
	}
}

static void simple_crypt(char *buf, int len)
{
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

//============================================================================


/*
====================
Host_Init
====================
*/
void Host_Init (quakeparms_t *parms)
{
	extern cvar_t	vid_renderer;
	COM_InitArgv (parms->argc, parms->argv);

	Sys_mkdir("qw");

	if (COM_CheckParm ("-minmemory"))
		parms->memsize = MINIMUM_MEMORY;

	host_parms = *parms;

	if (parms->memsize < MINIMUM_MEMORY)
		Sys_Error ("Only %4.1f megs of memory reported, can't execute game", parms->memsize / (float)0x100000);

	Memory_Init (parms->membase, parms->memsize);
	Cbuf_Init ();
	Cmd_Init ();

	V_Init ();

	COM_Init ();
#ifdef Q2BSPS
	CM_Init();
#endif

	Host_FixupModelNames();


	NET_Init ();
	NET_InitClient ();

	Netchan_Init ();

	Renderer_Init();

//	W_LoadWadFile ("gfx.wad");
	Key_Init ();
	Con_Init ();	
	M_Init ();	
 
#ifdef __linux__
	IN_Init ();
	CDAudio_Init ();
//	VID_Init (host_basepal);
//	Draw_Init ();
//	SCR_Init ();
//	R_Init ();

	S_Init ();
	
	cls.state = ca_disconnected;
	Sbar_Init ();
	CL_Init ();
#else
	S_Init ();

	cls.state = ca_disconnected;
	CDAudio_Init ();
	Sbar_Init ();
	CL_Init ();
	IN_Init ();
#endif
	TranslateInit();
#ifndef CLIENTONLY
	SV_Init(parms);
#endif
#ifdef TEXTEDITOR
	Editor_Init();
#endif

	//	Con_Printf ("Exe: "__TIME__" "__DATE__"\n");
	Con_TPrintf (TL_HEAPSIZE, parms->memsize/ (1024*1024.0));

	Cbuf_AddText ("+mlook\n", RESTRICT_LOCAL);		//fixme: this is bulky, only exec one of these.
	Cbuf_AddText ("exec default.cfg\n", RESTRICT_LOCAL);
	Cbuf_AddText ("exec config.cfg\n", RESTRICT_LOCAL);	//don't get confused with q2.
	Cbuf_AddText ("exec quake.rc\n", RESTRICT_LOCAL);
	Cbuf_AddText ("exec hexen.rc\n", RESTRICT_LOCAL);
	Cbuf_AddText ("exec fte.cfg\n", RESTRICT_LOCAL);
	Cbuf_AddText ("cl_warncmd 1\n", RESTRICT_LOCAL);

	Hunk_AllocName (0, "-HOST_HUNKLEVEL-");
	host_hunklevel = Hunk_LowMark ();

	R_SetRenderer(QR_NONE);//set the mod stuff...

	host_initialized = true;

	if (!setjmp (host_abort) )
		Cbuf_Execute ();	//if the server initialisation causes a problem, give it a place to abort to
#ifndef NOMEDIA
	if (!cls.demofile && !cls.state && !media_filmtype)
	{
		if (COM_FDepthFile("video/idlogo.roq", true) > COM_FDepthFile("video/idlog.cin", true))
			Media_PlayFilm("video/idlog.cin");
		else
			Media_PlayFilm("video/idlogo.roq");	
	}
#endif
	if (!qrenderer && *vid_renderer.string)
	{
		Cmd_ExecuteString("vid_restart\n", RESTRICT_LOCAL);
	}

	if (!qrenderer)
	{
		Cmd_ExecuteString("vid_restart\n", RESTRICT_LOCAL);
	}

	UI_Init();


Con_TPrintf (TL_NL);
#ifdef VERSION3PART
	Con_TPrintf (TCL_VERSION3, VERSION, build_number());
#else
	Con_TPrintf (TLC_VERSION2, VERSION, build_number());
#endif
Con_TPrintf (TL_NL);

	Con_TPrintf (TLC_QUAKEWORLD_INITED);

	Con_Printf("This program is free software; you can redistribute it and/or "
				"modify it under the terms of the GNU General Public License "
				"as published by the Free Software Foundation; either version 2 "
				"of the License, or (at your option) any later version."
				"\n"
				"This program is distributed in the hope that it will be useful, "
				"but WITHOUT ANY WARRANTY; without even the implied warranty of "
				"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. "
				"\n"
				"See the GNU General Public License for more details.\n");
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
	static qboolean isdown = false;
	
	if (isdown)
	{
		Sys_Printf ("recursive shutdown\n");
		return;
	}
	isdown = true;

	UI_Stop();

	Host_WriteConfiguration (); 
		
	CDAudio_Shutdown ();
	S_Shutdown();
	IN_Shutdown ();
	if (VID_DeInit)
		VID_DeInit();
#ifndef CLIENT_ONLY
	SV_Shutdown();
#else
	NET_Shutdown ();
#endif

	Cvar_Shutdown();
	Validation_FlushFileList();
}
