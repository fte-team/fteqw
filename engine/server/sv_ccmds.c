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
#include "qwsvdef.h"

qboolean	sv_allow_cheats;

int fp_messages=4, fp_persecond=4, fp_secondsdead=10;
char fp_msg[255] = { 0 };
extern cvar_t cl_warncmd;
cvar_t sv_cheats = {"sv_cheats", "0", NULL, CVAR_LATCH};
	extern		redirect_t	sv_redirected;

extern cvar_t sv_public;


//generic helper function for naming players.
client_t *SV_GetClientForString(char *name, int *id)
{
	int i;
	char *s, *s2;
	char nicename[80];
	client_t *cl;

	int first=0;
	if (id && *id != -1)
		first = *id;

	if (!strcmp(name, "*"))	//match with all
	{
		for (i = first, cl = svs.clients+first; i < sv.allocated_client_slots; i++, cl++)
		{
			if (cl->state<=cs_zombie)
				continue;

			*id=i+1;
			return cl;
		}
		*id=sv.allocated_client_slots;
		return NULL;
	}

	//check to make sure it's all an int

	for (s = name; *s; s++)
	{
		if (*s < '0' || *s > '9')
			break;
	}

	//we got to the end of the string and found only numbers. - it's a uid.
	if (!*s)
	{
		int uid = Q_atoi(name);
		for (i = first, cl = svs.clients; i < sv.allocated_client_slots; i++, cl++)
		{
			if (cl->state<=cs_zombie)
				continue;
			if (cl->userid == uid)
			{
				*id=sv.allocated_client_slots;
				return cl;
			}
		}
		
		return NULL;
	}	

	for (i = first, cl = svs.clients+first; i < sv.allocated_client_slots; i++, cl++)
	{
		if (cl->state<=cs_zombie)
			continue;


		s = nicename;
		s2 = cl->name;
		while(*s2)
		{
			*s = *s2 & ~128;
			s2++;
			if (*s == '3')
				*s = 'e';
			else if (*s == '4')
				*s = 'a';
			else if (*s == '1' || *s == '7')
				*s = 'l';
			else if (*s >= 18 && *s < 27)
				*s = *s - 18 + '0';
			else if (*s >= 'A' && *s <= 'Z')
				*s = *s - 'A' + 'a';
			else if (*s<' ' || *s == '~')
				continue;

			s++;
		}
		*s = '\0';

		if (strstr(nicename, name))
		{
			*id=i+1;
			return cl;
		}
	}

	return NULL;
}

/*
===============================================================================

OPERATOR CONSOLE ONLY COMMANDS

These commands can only be entered from stdin or by a remote operator datagram
===============================================================================
*/

/*
====================
SV_SetMaster_f

Make a master server current
====================
*/
void SV_SetMaster_f (void)
{
	char	data[2];
	int		i;

	Cvar_Set(&sv_public, "1");	//go public.

	memset (&master_adr, 0, sizeof(master_adr));

	for (i=1 ; i<Cmd_Argc() ; i++)
	{
		if (!strcmp(Cmd_Argv(i), "none") || !NET_StringToAdr (Cmd_Argv(i), &master_adr[i-1]))
		{
			Con_TPrintf (STL_NOMASTERMODE);
			return;
		}
		if (master_adr[i-1].port == 0)
			master_adr[i-1].port = BigShort (27000);

		Con_TPrintf (STL_MASTERAT, NET_AdrToString (master_adr[i-1]));

		Con_TPrintf (STL_SENDINGPING);

		data[0] = A2A_PING;
		data[1] = 0;
		if (sv.state)
			NET_SendPacket (NS_SERVER, 2, data, master_adr[i-1]);
	}

	svs.last_heartbeat = -99999;
}


/*
==================
SV_Quit_f
==================
*/
void SV_Quit_f (void)
{
	SV_FinalMessage ("server shutdown\n");
	Con_TPrintf (STL_SHUTTINGDOWN);
	SV_Shutdown ();
	Sys_Quit ();
}

/*
============
SV_Logfile_f
============
*/
void SV_Logfile_f (void)
{
	char	name[MAX_OSPATH];

	if (con_debuglog)
	{
		Con_TPrintf (STL_LOGGINGOFF);
		con_debuglog = false;
		return;
	}

	sprintf (name, "%s/qconsole.log", com_gamedir);
	Con_TPrintf (STL_LOGGINGTO, name);
	con_debuglog = true;
}


/*
============
SV_Fraglogfile_f
============
*/
void SV_Fraglogfile_f (void)
{
	char	name[MAX_OSPATH];
	int		i;

	if (sv_fraglogfile)
	{
		Con_TPrintf (STL_FLOGGINGOFF);
		fclose (sv_fraglogfile);
		sv_fraglogfile = NULL;
		return;
	}

	// find an unused name
	for (i=0 ; i<1000 ; i++)
	{
		sprintf (name, "%s/frag_%i.log", com_gamedir, i);
		sv_fraglogfile = fopen (name, "r");
		if (!sv_fraglogfile)
		{	// can't read it, so create this one
			sv_fraglogfile = fopen (name, "w");
			if (!sv_fraglogfile)
				i=1000;	// give error
			break;
		}
		fclose (sv_fraglogfile);
	}
	if (i==1000)
	{
		Con_TPrintf (STL_FLOGGINGFAILED);
		sv_fraglogfile = NULL;
		return;
	}

	Con_TPrintf (STL_FLOGGINGTO, name);
}


/*
==================
SV_SetPlayer

Sets host_client and sv_player to the player with idnum Cmd_Argv(1)
==================
*/
qboolean SV_SetPlayer (void)
{
	client_t	*cl;
	int			i;
	int			idnum;

	idnum = atoi(Cmd_Argv(1));

	for (i=0,cl=svs.clients ; i<MAX_CLIENTS ; i++,cl++)
	{
		if (!cl->state)
			continue;
		if (cl->userid == idnum)
		{
			host_client = cl;
			sv_player = host_client->edict;
			return true;
		}
	}
	Con_TPrintf (STL_USERIDNOTONSERVER, idnum);
	return false;
}


/*
==================
SV_God_f

Sets client to godmode
==================
*/
void SV_God_f (void)
{
	if (!sv_allow_cheats)
	{
		Con_TPrintf (STL_NEEDCHEATPARM);
		return;
	}

	if (!SV_SetPlayer ())
		return;

	sv_player->v.flags = (int)sv_player->v.flags ^ FL_GODMODE;
	if ((int)sv_player->v.flags & FL_GODMODE)
		SV_ClientTPrintf (host_client, PRINT_HIGH, STL_GODON);
	else
		SV_ClientTPrintf (host_client, PRINT_HIGH, STL_GODOFF);			
}


void SV_Noclip_f (void)
{
	if (!sv_allow_cheats)
	{
		Con_TPrintf (STL_NEEDCHEATPARM);
		return;
	}

	if (!SV_SetPlayer ())
		return;

	if (sv_player->v.movetype != MOVETYPE_NOCLIP)
	{
		sv_player->v.movetype = MOVETYPE_NOCLIP;
		SV_ClientTPrintf (host_client, PRINT_HIGH, STL_NOCLIPON);
	}
	else
	{
		sv_player->v.movetype = MOVETYPE_WALK;
		SV_ClientTPrintf (host_client, PRINT_HIGH, STL_NOCLIPOFF);
	}
}


/*
==================
SV_Give_f
==================
*/
void SV_Give_f (void)
{
	char	*t;
	int		v;
	
	if (!sv_allow_cheats)
	{
		Con_TPrintf (STL_NEEDCHEATPARM);
		return;
	}
	
	if (!SV_SetPlayer ())
		return;

	if (!svprogfuncs)
		return;

	t = Cmd_Argv(2);
	v = atoi (Cmd_Argv(3));
	
	switch (t[0])
	{
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		sv_player->v.items = (int)sv_player->v.items | IT_SHOTGUN<< (t[0] - '2');
		break;
	
	case 's':
		sv_player->v.ammo_shells = v;
		break;		
	case 'n':
		sv_player->v.ammo_nails = v;
		break;		
	case 'r':
		sv_player->v.ammo_rockets = v;
		break;		
	case 'h':
		sv_player->v.health = v;
		break;		
	case 'c':
		sv_player->v.ammo_cells = v;
		break;		
	}
}

int ShowMapList (char *name, int flags, void *parm)
{
	if (name[5] == 'b' && name[6] == '_')	//skip box models
		return true;
	Con_Printf("%s\n", name+5);
	return true;
}
void SV_MapList_f(void)
{
	COM_EnumerateFiles("maps/*.bsp", ShowMapList, NULL);
}

/*
======================
SV_Map_f

handle a 
map <mapname>
command from the console or progs.
======================
*/
void SV_Map_f (void)
{
	char	level[MAX_QPATH];
	char	spot[MAX_QPATH];
	char	expanded[MAX_QPATH];
	char	*nextserver;
	qboolean issamelevel = false;
	qboolean newunit = false;
	qboolean cinematic = false;
	int i;
	char *startspot;

	if (Cmd_Argc() != 2 && Cmd_Argc() != 3)
	{
		Con_TPrintf (STL_MAPCOMMANDUSAGE);
		return;
	}
	strcpy (level, Cmd_Argv(1));
	startspot = ((Cmd_Argc() == 2)?NULL:Cmd_Argv(2));

	nextserver = strchr(level, '+');
	if (nextserver)
	{
		*nextserver = '\0';
		nextserver++;
	}

	if (startspot)
	{
		strcpy(spot, startspot);
		startspot = spot;
	}
	else if ((startspot = strchr(level, '$')))
	{
		strcpy(spot, startspot+1);
		*startspot = '\0';
		startspot = spot;
	}
	else
		startspot = NULL;

	if (!strcmp(level, "."))	//restart current
	{		
		COM_StripExtension(COM_SkipPath(sv.modelname), level);
		issamelevel = true;

		Q_strncpyz(spot, Info_ValueForKey(svs.info, "*startspot"), sizeof(spot));
		startspot = spot;
	}

	COM_FlushFSCache();

	// check to make sure the level exists
	if (*level == '*')
	{
		memmove(level, level+1, strlen(level));
		newunit=true;
	}

	if (strlen(level) > 4 && !strcmp(level + strlen(level)-4, ".cin"))
	{
		cinematic = true;
	}
	else
	{
		sprintf (expanded, "maps/%s.bsp", level);
		if (!COM_FCheckExists (expanded))
		{
			Con_TPrintf (STL_CANTFINDMAP, expanded);
			return;
		}
	}

	if (sv.mvdrecording)
		SV_MVDStop_f();

#ifndef SERVERONLY
	if (!isDedicated)	//otherwise, info used on map loading isn't present
		Cmd_ExecuteString(va("fullserverinfo \"%s\"\n", svs.info), RESTRICT_LOCAL);

	if (!sv.state && cls.state)
		CL_Disconnect();
#endif

	SV_SaveSpawnparms (issamelevel);

	if (startspot && !issamelevel && !newunit)
		SV_SaveLevelCache(false);

	for (i=0 ; i<MAX_CLIENTS ; i++)	//we need to drop all q2 clients. We don't mix q1w with q2.
	{
		if (svs.clients[i].state>cs_connected)	//so that we don't send a datagram
			svs.clients[i].state=cs_connected;
	}

	SV_BroadcastCommand ("changing\n");
	SV_SendMessagesToAll ();

#ifndef SERVERONLY
	S_StopAllSounds (true);
	SCR_BeginLoadingPlaque();	
#endif

	if (newunit || !startspot || !SV_LoadLevelCache(level, startspot, false))
		SV_SpawnServer (level, startspot, false, cinematic);

	SV_BroadcastCommand ("reconnect\n");

	if (!issamelevel)
	{
		cvar_t *nsv;
		nsv = Cvar_Get("nextserver", "", 0, "");
		if (nextserver)
			Cvar_Set(nsv, nextserver);
		else
			Cvar_Set(nsv, "");
	}
}

void SV_KillServer_f(void)
{
	SV_UnspawnServer();
}


/*
==================
SV_Kick_f

Kick a user off of the server
==================
*/
void SV_Kick_f (void)
{	
	client_t	*cl;
	int clnum=-1;

	while((cl = SV_GetClientForString(Cmd_Argv(1), &clnum)))
	{
		SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTWASKICKED, cl->name);
		// print directly, because the dropped client won't get the
		// SV_BroadcastPrintf message
		SV_ClientTPrintf (cl, PRINT_HIGH, STL_YOUWEREKICKED);
		SV_DropClient (cl); 		
	}	

	if (clnum == -1)
		Con_TPrintf (STL_USERDOESNTEXIST, Cmd_Argv(1));
}

void SV_BanIP_f (void)
{
	client_t	*cl;
	int clnum=-1;

	while((cl = SV_GetClientForString(Cmd_Argv(1), &clnum)))
	if (cl)
	{
		bannedips_t *nb;
		nb = Z_Malloc(sizeof(bannedips_t));
		nb->next = svs.bannedips;
		nb->adr = cl->netchan.remote_address;
		if (*Cmd_Argv(2))	//explicit blocking of all ports of a client ip
			nb->adr.port = 0;
		svs.bannedips = nb;

		SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTWASBANNED, cl->name);
		// print directly, because the dropped client won't get the
		// SV_BroadcastPrintf message
		SV_ClientTPrintf (cl, PRINT_HIGH, STL_YOUWEREBANNED);
		SV_DropClient (cl);
	}

	if (clnum == -1)
		Con_TPrintf (STL_USERDOESNTEXIST, Cmd_Argv(1));
}

void SV_BanName_f (void)
{
	client_t	*cl;
	int clnum=-1;

	rankstats_t rs;


	while((cl = SV_GetClientForString(Cmd_Argv(1), &clnum)))
	{
		SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTWASBANNED, cl->name);
		// print directly, because the dropped client won't get the
		// SV_BroadcastPrintf message
		SV_ClientTPrintf (cl, PRINT_HIGH, STL_YOUWEREBANNED);
		if (cl->rankid)
		{
			if (Rank_GetPlayerStats(cl->rankid, &rs))
			{
				rs.flags1 |= RANK_BANNED;
				Rank_SetPlayerStats(cl->rankid, &rs);
			}
		}
		else
			Con_Printf("User is not using an account\n");
		SV_DropClient (cl); 		
	}

	if (clnum == -1)
		Con_TPrintf (STL_USERDOESNTEXIST, Cmd_Argv(1));
}

void SV_ForceName_f (void)
{
	client_t	*cl;
	int clnum=-1;
	int i;

	while((cl = SV_GetClientForString(Cmd_Argv(1), &clnum)))
	{
		Info_SetValueForKey(cl->userinfo, "name", Cmd_Argv(2), MAX_INFO_STRING);
		SV_ExtractFromUserinfo(cl);
		Q_strncpyz(cl->name, Cmd_Argv(2), sizeof(cl->name));
		i = cl - svs.clients;
		MSG_WriteByte (&sv.reliable_datagram, svc_setinfo);
		MSG_WriteByte (&sv.reliable_datagram, i);
		MSG_WriteString (&sv.reliable_datagram, "name");
		MSG_WriteString (&sv.reliable_datagram, cl->name);

		return;
	}

	if (clnum == -1)
		Con_TPrintf (STL_USERDOESNTEXIST, Cmd_Argv(1));
}

void SV_CripplePlayer_f (void)
{
	client_t	*cl;
	int clnum=-1;

	int persist = *Cmd_Argv(2);

	while((cl = SV_GetClientForString(Cmd_Argv(1), &clnum)))
	{
		if (!cl->iscrippled)
		{
			if (persist)
			{
				cl->iscrippled = 2;
				SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTISCRIPPLEDPERMANENTLY, cl->name);
			}
			else
			{
				cl->iscrippled = true;
				SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTISCRIPPLED, cl->name);
			}
		}
		else
		{
			cl->iscrippled = false;
			SV_ClientTPrintf (cl, PRINT_HIGH, STL_YOUARNTCRIPPLED);
		}		
	}

	if (clnum == -1)
		Con_TPrintf (STL_USERDOESNTEXIST, Cmd_Argv(1));
}

void SV_Mute_f (void)
{
	client_t	*cl;
	int clnum=-1;

	int persist = *Cmd_Argv(2);

	while((cl = SV_GetClientForString(Cmd_Argv(1), &clnum)))
	{
		if (!cl->ismuted)
		{
			if (persist)
			{
				cl->ismuted = 2;
				SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTISMUTEDPERMANENTLY, cl->name);
			}
			else
			{
				cl->ismuted = true;
				SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTISMUTED, cl->name);
			}
		}
		else
		{
			cl->ismuted = false;
			SV_ClientTPrintf (cl, PRINT_HIGH, STL_YOUARNTMUTED);
		}
	}

	if (clnum == -1)
		Con_TPrintf (STL_USERDOESNTEXIST, Cmd_Argv(1));
}

void SV_Cuff_f (void)
{
	client_t	*cl;
	int clnum=-1;

	int persist = *Cmd_Argv(2);

	while((cl = SV_GetClientForString(Cmd_Argv(1), &clnum)))
	{
		if (!cl->iscuffed)
		{
			if (persist)
			{
				cl->iscuffed = 2;
				SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTISCUFFEDPERMANENTLY, cl->name);
			}
			else
			{
				cl->iscuffed = true;
				SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTISCUFFED, cl->name);
			}
		}
		else
		{
			cl->iscuffed = false;
			SV_ClientTPrintf (cl, PRINT_HIGH, STL_YOUARNTCUFFED);
		}
		return;
	}

	if (clnum == -1)
		Con_TPrintf (STL_USERDOESNTEXIST, Cmd_Argv(1));
}

void SV_StuffToClient_f(void)
{	//with this we emulate the progs 'stuffcmds' builtin

	client_t	*cl;

	int clnum=-1;
	char *clientname = Cmd_Argv(1);
	char *str;
	char *c;
	char *key;

	Cmd_ShiftArgs(1);
	if (!strcmp(Cmd_Argv(1), "bind"))
	{
		key = Z_Malloc(strlen(Cmd_Argv(2))+1);
		strcpy(key, Cmd_Argv(2));
		Cmd_ShiftArgs(2);
	}
	else
		key = NULL;
	str = Cmd_Args();

	while(*str <= ' ')	//strim leading spaces
	{
		if (!*str)
			break;
		str++;
	}

	//a list of safe, allowed commands. Allows any extention of this.
	if (strchr(str, '\n') || strchr(str, ';') || (
		strncmp(str, "setinfo", 7) &&
		strncmp(str, "quit", 4) &&
		strncmp(str, "gl_fb", 5) &&
		strncmp(str, "r_fb", 4) &&
		strncmp(str, "say", 3) &&	//note that the say parsing could be useful here.
		strncmp(str, "echo", 4) &&
		strncmp(str, "name", 4) &&
		strncmp(str, "skin", 4) &&
		strncmp(str, "color", 5) &&
		strncmp(str, "cmd", 3) &&
		strncmp(str, "fov", 3) &&
		strncmp(str, "connect", 7) &&
		strncmp(str, "rate", 4) &&
		strncmp(str, "cd", 2) &&
		strncmp(str, "easyrecord", 10) &&
		strncmp(str, "leftisright", 11) &&
		strncmp(str, "menu_", 5) &&
		strncmp(str, "r_fullbright", 12) &&
		strncmp(str, "toggleconsole", 13) &&
		strncmp(str, "v_i", 3) &&	//idlescale vars
		strncmp(str, "bf", 2) &&
		strncmp(str, "+", 1) &&
		strncmp(str, "-", 1) &&
		strncmp(str, "impulse", 7) &&
		1))
	{
		Con_Printf("You're not allowed to stuffcmd that\n");

		if (key)
			Z_Free(key);
		return;
	}

	while((cl = SV_GetClientForString(clientname, &clnum)))
	{
		if (cl->isq2client)
			ClientReliableWrite_Begin (cl, svcq2_stufftext, 3+strlen(str) + (key?strlen(key)+6:0));
		else
			ClientReliableWrite_Begin (cl, svc_stufftext, 3+strlen(str) + (key?strlen(key)+6:0));

		if (key)
		{
			for (c = "bind "; *c; c++)
				ClientReliableWrite_Byte (cl, *c);

			for (c = key; *c; c++)
				ClientReliableWrite_Byte (cl, *c);

			ClientReliableWrite_Byte (cl, ' ');
		}

		for (c = str; *c; c++)
			ClientReliableWrite_Byte (cl, *c);
		ClientReliableWrite_Byte (cl, '\n');
		ClientReliableWrite_Byte (cl, '\0');
	}

	if (key)
		Z_Free(key);
}

/*
================
SV_Status_f
================
*/
void SV_Status_f (void)
{
	int			i, j, l;
	client_t	*cl;
	float		cpu, avg, pak;
	char		*s;

	int columns = 80;

	if (sv_redirected != RD_OBLIVION && (sv_redirected != RD_NONE
#ifndef SERVERONLY
		|| (vid.width < 68*8 && qrenderer != QR_NONE)
#endif
		))
		columns = 40;

	if (!sv.state)
	{
		if (net_local_ipadr.type != NA_LOOPBACK)
			Con_Printf ("ip address       : %s\n",NET_AdrToString (net_local_ipadr));

		Con_Printf("Server is not running\n");
		return;
	}

	if (Cmd_Argc()>1)
		columns = atoi(Cmd_Argv(1));

	cpu = (svs.stats.latched_active+svs.stats.latched_idle);
	if (cpu)
		cpu = 100*svs.stats.latched_active/cpu;
	avg = 1000*svs.stats.latched_active / STATFRAMES;
	pak = (float)svs.stats.latched_packets/ STATFRAMES;

	if (net_local_ipadr.type != NA_LOOPBACK)
		Con_Printf ("ip address       : %s\n",NET_AdrToString (net_local_ipadr));
	Con_Printf ("cpu utilization  : %3i%%\n",(int)cpu);
	Con_Printf ("avg response time: %i ms\n",(int)avg);
	Con_Printf ("packets/frame    : %5.2f\n", pak);	//not relevent as a limit.
	
// min fps lat drp
	if (columns < 80)
	{
		// most remote clients are 40 columns
		//           0123456789012345678901234567890123456789
		Con_Printf ("name               userid frags\n");
        Con_Printf ("  address          rate ping drop\n");
		Con_Printf ("  ---------------- ---- ---- -----\n");
		for (i=0,cl=svs.clients ; i<MAX_CLIENTS ; i++,cl++)
		{
			if (!cl->state)
				continue;

			Con_Printf ("%-16.16s  ", cl->name);

			Con_Printf ("%6i %5i", cl->userid, (int)cl->old_frags);
			if (cl->spectator)
				Con_Printf(" (s)\n");
			else			
				Con_Printf("\n");

			s = NET_BaseAdrToString ( cl->netchan.remote_address);
			Con_Printf ("  %-16.16s", s);
			if (cl->state == cs_connected)
			{
				Con_Printf ("CONNECTING\n");
				continue;
			}
			if (cl->state == cs_zombie)
			{
				Con_Printf ("ZOMBIE\n");
				continue;
			}
			Con_Printf ("%4i %4i %5.2f\n"
				, (int)(1000*cl->netchan.frame_rate)
				, (int)SV_CalcPing (cl)
				, 100.0*cl->netchan.drop_count / cl->netchan.incoming_sequence);
		}
	}
	else
	{
		Con_Printf ("frags userid address         name            rate ping drop  qport dl%% dls\n");
		Con_Printf ("----- ------ --------------- --------------- ---- ---- ----- ----- --- ----\n");
		for (i=0,cl=svs.clients ; i<MAX_CLIENTS ; i++,cl++)
		{
			if (!cl->state)
				continue;
			Con_Printf ("%5i %6i ", (int)cl->old_frags,  cl->userid);

			if (cl->istobeloaded && cl->state == cs_zombie)
				s = "LoadZombie";
			else
				s = NET_BaseAdrToString ( cl->netchan.remote_address);
			Con_Printf ("%s", s);
			l = 16 - strlen(s);
			for (j=0 ; j<l ; j++)
				Con_Printf (" ");
			
			Con_Printf ("%s", cl->name);
			l = 16 - strlen(cl->name);
			for (j=0 ; j<l ; j++)
				Con_Printf (" ");
			if (cl->state == cs_connected)
			{
				Con_Printf ("CONNECTING           ");
			}
			else if (cl->state == cs_zombie)
			{
				Con_Printf ("ZOMBIE               ");
			}
			else
				Con_Printf ("%4i %4i %5.1f %4i"
				, (int)(1000*cl->netchan.frame_rate)
				, (int)SV_CalcPing (cl)
				, 100.0*cl->netchan.drop_count / cl->netchan.incoming_sequence
				, cl->netchan.qport);
			if (cl->download)
			{
				Con_Printf (" %3i %4i", (cl->downloadcount*100)/cl->downloadsize, cl->downloadsize/1024);
			}
			if (cl->spectator)
				Con_Printf(" (s)\n");
			else			
				Con_Printf("\n");

				
		}
	}
	Con_Printf ("\n");
}

/*
==================
SV_ConSay_f
==================
*/
void SV_ConSay_f(void)
{
	client_t *client;
	int		j;
	char	*p;
	char	text[1024];

	if (Cmd_Argc () < 2)
		return;

	Q_strcpy (text, "console: ");
	p = Cmd_Args();

	if (*p == '"')
	{
		p++;
		p[Q_strlen(p)-1] = 0;
	}

	Q_strcat(text, p);

	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++)
	{
		if (client->state == cs_free)
			continue;
		SV_ClientPrintf(client, PRINT_CHAT, "%s\n", text);
	}

	if (sv.mvdrecording)
	{
		MVDWrite_Begin (dem_all, 0, strlen(text)+3);
		MSG_WriteByte ((sizebuf_t*)demo.dbuf, svc_print);
		MSG_WriteByte ((sizebuf_t*)demo.dbuf, PRINT_CHAT);
		MSG_WriteString ((sizebuf_t*)demo.dbuf, text);
	}
}

void SV_ConSayOne_f (void)
{
	char	text[2048];
	client_t	*to;
	int i;
	char *s;
	int clnum=-1;

	if (Cmd_Argc () < 3)
		return;

	while((to = SV_GetClientForString(Cmd_Argv(1), &clnum)))
	{
		Q_strcpy (text, "{console}: ");

		for (i = 2; ; i++)
		{
			s = Cmd_Argv(i);
			if (!*s)
				break;

			if (strlen(text) + strlen(s) + 2 >= sizeof(text)-1)
				break;
			strcat(text, " ");
			strcat(text, s);
		}
		strcat(text, "\n");
		SV_ClientPrintf(to, PRINT_CHAT, "%s", text);
	}
	if (!clnum)
		Con_TPrintf(STL_USERDOESNTEXIST, Cmd_Argv(1));
}

/*
==================
SV_Heartbeat_f
==================
*/
void SV_Heartbeat_f (void)
{
	svs.last_heartbeat = -9999;
}

void SV_SendServerInfoChange(char *key, char *value)
{
	if (!sv.state)
		return;
#ifdef Q2SERVER
	if (ge)
		return;	//FIXME!!!
#endif

	MSG_WriteByte (&sv.reliable_datagram, svc_serverinfo);
	MSG_WriteString (&sv.reliable_datagram, key);
	MSG_WriteString (&sv.reliable_datagram, value);
}

/*
===========
SV_Serverinfo_f

  Examine or change the serverinfo string
===========
*/
char *CopyString(char *s);
void SV_Serverinfo_f (void)
{
	cvar_t	*var;
	char value[512];
	int i;

	if (Cmd_Argc() == 1)
	{
		Con_TPrintf (STL_SERVERINFOSETTINGS);
		Info_Print (svs.info);
		return;
	}

	if (Cmd_Argc() < 3)
	{
		Con_TPrintf (STL_SERVERINFOSYNTAX);
		return;
	}

	if (Cmd_Argv(1)[0] == '*')
	{
		Con_TPrintf (TL_STARKEYPROTECTED);
		return;
	}
	Q_strncpyz(value, Cmd_Argv(2), sizeof(value));
	value[sizeof(value)-1] = '\0';
	for (i = 3; i < Cmd_Argc(); i++)
	{
		strncat(value, " ", sizeof(value)-1);
		strncat(value, Cmd_Argv(i), sizeof(value)-1);
	}

	Info_SetValueForKey (svs.info, Cmd_Argv(1), value, MAX_SERVERINFO_STRING);

	// if this is a cvar, change it too	
	var = Cvar_FindVar (Cmd_Argv(1));
	if (var)
	{
		Cvar_Set(var, value);
/*		Z_Free (var->string);	// free the old value string	
		var->string = CopyString (value);
		var->value = Q_atof (var->string);
*/	}

	SV_SendServerInfoChange(Cmd_Argv(1), value);
}


/*
===========
SV_Serverinfo_f

  Examine or change the serverinfo string
===========
*/
char *CopyString(char *s);
void SV_Localinfo_f (void)
{
	char *old;

	if (Cmd_Argc() == 1)
	{
		Con_TPrintf (STL_LOCALINFOSETTINGS);
		Info_Print (localinfo);
		return;
	}

	if (Cmd_Argc() != 3)
	{
		Con_TPrintf (STL_LOCALINFOSYNTAX);
		return;
	}

	if (Cmd_Argv(1)[0] == '*')
	{
		Con_TPrintf (TL_STARKEYPROTECTED);
		return;
	}
	old = Info_ValueForKey(localinfo, Cmd_Argv(1));
	Info_SetValueForKey (localinfo, Cmd_Argv(1), Cmd_Argv(2), MAX_LOCALINFO_STRING);

	PR_LocalInfoChanged(Cmd_Argv(1), old, Cmd_Argv(2));
}


/*
===========
SV_User_f

Examine a users info strings
===========
*/
void SV_User_f (void)
{
	if (Cmd_Argc() != 2)
	{
		Con_TPrintf (STL_USERINFOSYNTAX);
		return;
	}

	if (!SV_SetPlayer ())
		return;

	Info_Print (host_client->userinfo);
}

/*
================
SV_Floodport_f

Sets the gamedir and path to a different directory.
================
*/

void SV_Floodprot_f (void)
{
	int arg1, arg2, arg3;
	
	if (Cmd_Argc() == 1)
	{
		if (fp_messages) {
			Con_TPrintf (STL_FLOODPROTSETTINGS, 
				fp_messages, fp_persecond, fp_secondsdead);
			return;
		} else
			Con_TPrintf (STL_FLOODPROTNOTON);
	}

	if (Cmd_Argc() != 4)
	{
		Con_TPrintf (STL_FLOODPROTSYNTAX);
		return;
	}

	arg1 = atoi(Cmd_Argv(1));
	arg2 = atoi(Cmd_Argv(2));
	arg3 = atoi(Cmd_Argv(3));

	if (arg1<=0 || arg2 <= 0 || arg3<=0) {
		Con_TPrintf (STL_NONEGATIVEVALUES);
		return;
	}
	
	if (arg1 > 10) {
		Con_TPrintf (STL_TRACK10PLUSSMESSAGES);
		return;
	}

	fp_messages	= arg1;
	fp_persecond = arg2;
	fp_secondsdead = arg3;
}

void SV_Floodprotmsg_f (void)
{
	if (Cmd_Argc() == 1) {
		Con_TPrintf(STL_FLOODPROTCURRENTMESSAGE, fp_msg);
		return;
	} else if (Cmd_Argc() != 2) {
		Con_TPrintf(STL_FLOODPROTMESSAGESYNTAX);
		return;
	}
	sprintf(fp_msg, "%s", Cmd_Argv(1));
}
  

/*
================
SV_Gamedir

Sets the fake *gamedir to a different directory.
================
*/
void SV_Gamedir (void)
{
	char			*dir;

	if (Cmd_Argc() == 1)
	{
		Con_TPrintf (STL_CURRENTGAMEDIR, Info_ValueForKey (svs.info, "*gamedir"));
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Con_TPrintf (STL_SVGAMEDIRUSAGE);
		return;
	}

	dir = Cmd_Argv(1);

	if (strstr(dir, "..") || strstr(dir, "/")
		|| strstr(dir, "\\") || strstr(dir, ":") )
	{
		Con_TPrintf (STL_GAMEDIRCANTBEPATH);
		return;
	}

	Info_SetValueForStarKey (svs.info, "*gamedir", dir, MAX_SERVERINFO_STRING);
}

/*
================
SV_Gamedir_f

Sets the gamedir and path to a different directory.
================
*/
void SV_Gamedir_f (void)
{
	char			*dir;

	if (Cmd_Argc() == 1)
	{
		Con_TPrintf (STL_CURRENTGAMEDIR, com_gamedir);
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Con_TPrintf (STL_GAMEDIRUSAGE);
		return;
	}

	dir = Cmd_Argv(1);

	if (strstr(dir, "..") || strstr(dir, "/")
		|| strstr(dir, "\\") || strstr(dir, ":") )
	{
		Con_TPrintf (STL_GAMEDIRCANTBEPATH);
		return;
	}

	COM_Gamedir (dir);
	Info_SetValueForStarKey (svs.info, "*gamedir", dir, MAX_SERVERINFO_STRING);
}


extern char	gamedirfile[MAX_OSPATH];
/*
================
SV_Snap
================
*/
void SV_Snap (int uid)
{
	client_t *cl;
	char		pcxname[80]; 
	char		checkname[MAX_OSPATH];
	int			i;

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
	{
		if (!cl->state)
			continue;
		if (cl->userid == uid)
			break;
	}
	if (i >= MAX_CLIENTS) {
		Con_TPrintf (STL_USERDOESNTEXIST);
		return;
	}

	sprintf(pcxname, "%d-00.pcx", uid);

	sprintf(checkname, "%s/snap", gamedirfile);
	Sys_mkdir(gamedirfile);
	Sys_mkdir(checkname);
		
	for (i=0 ; i<=99 ; i++) 
	{ 
		pcxname[strlen(pcxname) - 6] = i/10 + '0'; 
		pcxname[strlen(pcxname) - 5] = i%10 + '0'; 
		sprintf (checkname, "%s/snap/%s", gamedirfile, pcxname);
		if (Sys_FileTime(checkname) == -1)
			break;	// file doesn't exist
	} 
	if (i==100) 
	{
		Con_TPrintf (STL_SNAPTOOMANYFILES);
		return;
	}
	strcpy(cl->uploadfn, checkname);

	memcpy(&cl->snap_from, &net_from, sizeof(net_from));
	if (sv_redirected != RD_NONE)
		cl->remote_snap = true;
	else
		cl->remote_snap = false;

	ClientReliableWrite_Begin (cl, svc_stufftext, 24);
	ClientReliableWrite_String (cl, "cmd snap");
	Con_TPrintf (STL_SNAPREQUEST, uid);
}

/*
================
SV_Snap_f
================
*/
void SV_Snap_f (void)
{
	int			uid;

	if (Cmd_Argc() != 2)
	{
		Con_TPrintf (STL_SNAPUSAGE);
		return;
	}

	uid = atoi(Cmd_Argv(1));

	SV_Snap(uid);
}

/*
================
SV_Snap
================
*/
void SV_SnapAll_f (void)
{
	client_t *cl;
	int			i;

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
	{
		if (cl->state < cs_connected || cl->spectator)
			continue;
		SV_Snap(cl->userid);
	}
}

float timer;
float lasttimer;
int ticsleft;
float timerinterval;
int timerlevel;
cvar_t *timercommand;
void SV_CheckTimer(void)
{
	float ctime = Sys_DoubleTime();
//	if (ctime < lasttimer) //new map? (shouldn't happen)
//		timer = ctime+5;	//trigger in a few secs
	lasttimer = ctime;

	if (ticsleft)
	{
		if (timer < ctime)
		{
			timer += timerinterval;
			if (ticsleft > 0)
				ticsleft--;

			if (timercommand)
			{
				Cbuf_AddText(timercommand->string, timerlevel);
				Cbuf_AddText("\n", timerlevel);
			}
		}
	}
}

void SV_SetTimer_f(void)
{
	int count;
	float interval;
	char *command;

	if (Cmd_Argc() < 2)
	{
		Con_Printf("%s count interval command\n", Cmd_Argv(0));
		return;
	}

	count = atoi(Cmd_Argv(1));
	interval = atof(Cmd_Argv(2));

	if (!count && Cmd_Argc() == 2)
	{
		ticsleft = 0;
		return;
	}

	if (interval <= 0 || (count <= 0 && count != -1))	//makes sure the args are right. :)
	{
		Con_Printf("%s count interval command\n", Cmd_Argv(0));
		return;
	}

	Cmd_ShiftArgs(2);	//strip the two vars
	command = Cmd_Args();

	timercommand = Cvar_Get("sv_timer", "", CVAR_NOSET, NULL);
	Cvar_ForceSet(timercommand, command);

	timer = Sys_DoubleTime() + interval;
	ticsleft = count;
	timerinterval = interval;

	timerlevel = Cmd_ExecLevel;
}

void SV_SendGameCommand_f(void)
{
#ifdef Q2SERVER
	if (ge)
	{
		ge->ServerCommand();
	}
	else
#endif
		Con_Printf("This command requires a Q2 sever\n");
}

/*
==================
SV_InitOperatorCommands
==================
*/
void SV_InitOperatorCommands (void)
{
#ifndef SERVERONLY
	if (isDedicated)
#endif
	{
		Cmd_AddCommand ("quit", SV_Quit_f);
		Cmd_AddCommand ("say", SV_ConSay_f);
		Cmd_AddCommand ("sayone", SV_ConSayOne_f);
		Cmd_AddCommand ("serverinfo", SV_Serverinfo_f);	//commands that conflict with client commands.
		Cmd_AddCommand ("user", SV_User_f);

		Cmd_AddCommand ("god", SV_God_f);
		Cmd_AddCommand ("give", SV_Give_f);
		Cmd_AddCommand ("noclip", SV_Noclip_f);
	}

	Cvar_Register(&sv_cheats, "Server Permissions");
	if (COM_CheckParm ("-cheats"))
	{
		Cvar_Set(&sv_cheats, "1");
	}

	Cmd_AddCommand ("logfile", SV_Logfile_f);
	Cmd_AddCommand ("fraglogfile", SV_Fraglogfile_f);

	Cmd_AddCommand ("snap", SV_Snap_f);
	Cmd_AddCommand ("snapall", SV_SnapAll_f);
	Cmd_AddCommand ("kick", SV_Kick_f);
	Cmd_AddCommand ("mute", SV_Mute_f);
	Cmd_AddCommand ("cuff", SV_Cuff_f);
	Cmd_AddCommand ("renameclient", SV_ForceName_f);
	Cmd_AddCommand ("cripple", SV_CripplePlayer_f);
	Cmd_AddCommand ("banname", SV_BanName_f);
	Cmd_AddCommand ("banip", SV_BanIP_f);
//	Cmd_AddCommand ("ban", SV_BanName_f);
	Cmd_AddCommand ("status", SV_Status_f);

	Cmd_AddCommand ("sv", SV_SendGameCommand_f);

	Cmd_AddCommand ("killserver", SV_KillServer_f);
	Cmd_AddCommand ("map", SV_Map_f);
	Cmd_AddCommand ("gamemap", SV_Map_f);
	Cmd_AddCommand ("changelevel", SV_Map_f);
	Cmd_AddCommand ("listmaps", SV_MapList_f);
	Cmd_AddCommand ("setmaster", SV_SetMaster_f);

	Cmd_AddCommand ("heartbeat", SV_Heartbeat_f);

	Cmd_AddCommand ("localinfo", SV_Localinfo_f);	
	Cmd_AddCommand ("gamedir", SV_Gamedir_f);
	Cmd_AddCommand ("sv_gamedir", SV_Gamedir);
	Cmd_AddCommand ("floodprot", SV_Floodprot_f);
	Cmd_AddCommand ("floodprotmsg", SV_Floodprotmsg_f);
	Cmd_AddCommand ("sv_settimer", SV_SetTimer_f);
	Cmd_AddCommand ("stuffcmd", SV_StuffToClient_f);

	cl_warncmd.value = 1;
}
