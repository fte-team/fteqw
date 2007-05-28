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

#include "qtv.h"

#define MAX_INFO_KEY 64

//I apologise for this if it breaks your formatting or anything
#define HELPSTRING "\
FTEQTV proxy commands: (build "__DATE__")\n\
----------------------\n\
connect, qtv, addserver\n\
  connect to a MVD stream (TCP)\n\
qtvlist\n\
  lists available streams on a proxy\n\
qw\n\
  connect to a server as a player (UDP)\n\
adddemo\n\
  play a demo from a MVD file\n\
port\n\
  UDP port for QuakeWorld client connections\n\
mvdport\n\
  specify TCP port for MVD broadcasting\n\
maxviewers, maxproxies\n\
  limit number of connections\n\
status, choke, late, talking, nobsp, reconnect, exec, password, master, hostname, record, stop, quit\n\
  other random commands\n\
\n"





char *Info_ValueForKey (char *s, const char *key, char *buffer, int buffersize)
{
	char	pkey[1024];
	char	*o;

	if (*s == '\\')
		s++;
	while (1)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
			{
				*buffer='\0';
				return buffer;
			}
			*o++ = *s++;
			if (o+2 >= pkey+sizeof(pkey))	//hrm. hackers at work..
			{
				*buffer='\0';
				return buffer;
			}
		}
		*o = 0;
		s++;

		o = buffer;

		while (*s != '\\' && *s)
		{
			if (!*s)
			{
				*buffer='\0';
				return buffer;
			}
			*o++ = *s++;

			if (o+2 >= buffer+buffersize)	//hrm. hackers at work..
			{
				*buffer='\0';
				return buffer;
			}
		}
		*o = 0;

		if (!strcmp (key, pkey) )
			return buffer;

		if (!*s)
		{
			*buffer='\0';
			return buffer;
		}
		s++;
	}
}

void Info_RemoveKey (char *s, const char *key)
{
	char	*start;
	char	pkey[1024];
	char	value[1024];
	char	*o;

	if (strstr (key, "\\"))
	{
//		printf ("Key has a slash\n");
		return;
	}

	while (1)
	{
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
		{
			strcpy (start, s);	// remove this part
			return;
		}

		if (!*s)
			return;
	}

}

void Info_SetValueForStarKey (char *s, const char *key, const char *value, int maxsize)
{
	char	newv[1024], *v;
	int		c;
#ifdef SERVERONLY
	extern cvar_t sv_highchars;
#endif

	if (strstr (key, "\\") || strstr (value, "\\") )
	{
//		printf ("Key has a slash\n");
		return;
	}

	if (strstr (key, "\"") || strstr (value, "\"") )
	{
//		printf ("Key has a quote\n");
		return;
	}

	if (strlen(key) >= MAX_INFO_KEY || strlen(value) >= MAX_INFO_KEY)
	{
//		printf ("Key or value is too long\n");
		return;
	}

	// this next line is kinda trippy
	if (*(v = Info_ValueForKey(s, key, newv, sizeof(newv))))
	{
		// key exists, make sure we have enough room for new value, if we don't,
		// don't change it!
		if (strlen(value) - strlen(v) + strlen(s) + 1 > maxsize)
		{
	//		Con_TPrintf (TL_INFOSTRINGTOOLONG);
			return;
		}
	}


	Info_RemoveKey (s, key);
	if (!value || !strlen(value))
		return;

	snprintf (newv, sizeof(newv)-1, "\\%s\\%s", key, value);

	if ((int)(strlen(newv) + strlen(s) + 1) > maxsize)
	{
//		printf ("info buffer is too small\n");
		return;
	}

	// only copy ascii values
	s += strlen(s);
	v = newv;
	while (*v)
	{
		c = (unsigned char)*v++;

//		c &= 127;		// strip high bits
		if (c > 13) // && c < 127)
			*s++ = c;
	}
	*s = 0;
}






#define DEFAULT_PUNCTUATION "(,{})(\':;=!><&|+"

char *COM_ParseToken (char *data, char *out, int outsize, const char *punctuation)
{
	int		c;
	int		len;

	if (!punctuation)
		punctuation = DEFAULT_PUNCTUATION;

	len = 0;
	out[0] = 0;

	if (!data)
		return NULL;

// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;			// end of file;
		data++;
	}

// skip // comments
	if (c=='/')
	{
		if (data[1] == '/')
		{
			while (*data && *data != '\n')
				data++;
			goto skipwhite;
		}
		else if (data[1] == '*')
		{
			data+=2;
			while (*data && (*data != '*' || data[1] != '/'))
				data++;
			data+=2;
			goto skipwhite;
		}
	}


// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			if (len >= outsize-1)
			{
				out[len] = '\0';
				return data;
			}
			c = *data++;
			if (c=='\"' || !c)
			{
				out[len] = 0;
				return data;
			}
			out[len] = c;
			len++;
		}
	}

// parse single characters
	if (strchr(punctuation, c))
	{
		out[len] = c;
		len++;
		out[len] = 0;
		return data+1;
	}

// parse a regular word
	do
	{
		if (len >= outsize-1)
			break;
		out[len] = c;
		data++;
		len++;
		c = *data;
		if (strchr(punctuation, c))
			break;
	} while (c>32);

	out[len] = 0;
	return data;
}





#define MAX_ARGS 8
#define ARG_LEN 512
typedef char *(*dispatchrconcommand_t)(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand);

char *Cmd_Hostname(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	if (!*arg[1])
	{
		buffer[0] = 0;
		if (*cluster->hostname)
			snprintf(buffer, sizeofbuffer, "Current hostname is %s\n", cluster->hostname);
		else
			return "No master server is currently set.\n";
		return buffer;
	}
	strncpy(cluster->hostname, arg[1], sizeof(cluster->hostname)-1);

	snprintf(buffer, sizeofbuffer, "hostname set to \"%s\"\n", cluster->hostname);
	return buffer;
}
char *Cmd_Master(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	netadr_t addr;

	if (!*arg[1])
	{
		if (*cluster->master)
			return "Subscribed to a master server (use '-' to clear)\n";
		else
			return "No master server is currently set.\n";
	}

	if (!strcmp(arg[1], "-"))
	{
		strncpy(cluster->master, "", sizeof(cluster->master)-1);
		return "Master server cleared\n";
	}

	if (!NET_StringToAddr(arg[1], &addr, 27000))	//send a ping like a qw server does. this is kinda pointless of course.
	{
		return "Couldn't resolve address\n";
	}

	strncpy(cluster->master, arg[1], sizeof(cluster->master)-1);
	cluster->mastersendtime = cluster->curtime;

	if (cluster->qwdsocket != INVALID_SOCKET)
		NET_SendPacket (cluster, cluster->qwdsocket, 1, "k", addr);
	return "Master server set.\n";
}



char *Cmd_UDPPort(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	int news;
	int newp = atoi(arg[1]);
	news = QW_InitUDPSocket(newp);

	if (news != INVALID_SOCKET)
	{
		cluster->mastersendtime = cluster->curtime;
		closesocket(cluster->qwdsocket);
		cluster->qwdsocket = news;
		cluster->qwlistenportnum = newp;

		snprintf(buffer, sizeofbuffer, "Opened udp port %i (all connected qw clients will time out)\n", newp);
	}
	else
		snprintf(buffer, sizeofbuffer, "Failed to open udp port %i\n", newp);
	return buffer;
}
char *Cmd_AdminPassword(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	if (!localcommand)
		return "Rejecting remote password change.\n";

	if (!*arg[1])
	{
		if (*cluster->adminpassword)
			return "An admin password is currently set\n";
		else
			return "No admin passsword is currently set\n";
	}

	strncpy(cluster->adminpassword, arg[1], sizeof(cluster->adminpassword)-1);
	return "Password changed.\n";
}

char *Cmd_QTVList(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	if (!*arg[1])
		return "connect requires an ip:port parameter\n";

	memmove(arg[1]+4, arg[1], ARG_LEN-5);
	strncpy(arg[1], "tcp:", 4);

	qtv = QTV_NewServerConnection(cluster, arg[1], arg[2], false, false, false, true);
	if (!qtv)
		return "Failed to connect to server, connection aborted\n";
	return "Querying proxy\n";
}
char *Cmd_QTVDemoList(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	if (!*arg[1])
		return "connect requires an ip:port parameter\n";

	memmove(arg[1]+4, arg[1], ARG_LEN-5);
	strncpy(arg[1], "tcp:", 4);

	qtv = QTV_NewServerConnection(cluster, arg[1], arg[2], false, false, false, 2);
	if (!qtv)
		return "Failed to connect to server, connection aborted\n";
	return "Querying proxy\n";
}
char *Cmd_QTVConnect(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	if (!*arg[1])
		return "connect requires an ip:port parameter\n";

	memmove(arg[1]+4, arg[1], ARG_LEN-5);
	strncpy(arg[1], "tcp:", 4);

	if (!QTV_NewServerConnection(cluster, arg[1], arg[2], false, false, false, false))
		return "Failed to connect to server, connection aborted\n";

	snprintf(buffer, sizeofbuffer, "Source registered \"%s\"\n", arg[1]);
	return buffer;
}
char *Cmd_QWConnect(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	if (!*arg[1])
		return "connect requires an ip:port parameter\n";

	memmove(arg[1]+4, arg[1], ARG_LEN-5);
	strncpy(arg[1], "udp:", 4);

	if (!QTV_NewServerConnection(cluster, arg[1], arg[2], false, false, false, false))
		return "Failed to connect to server, connection aborted\n";

	snprintf(buffer, sizeofbuffer, "Source registered \"%s\"\n", arg[1]);
	return buffer;
}
char *Cmd_MVDConnect(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	if (!*arg[1])
		return "adddemo requires an filename parameter\n";

	if (!localcommand)
		if (*arg[1] == '\\' || *arg[1] == '/' || strstr(arg[1], "..") || arg[1][1] == ':')
			return "Absolute paths are prohibited.\n";

	memmove(arg[1]+5, arg[1], ARG_LEN-6);
	strncpy(arg[1], "file:", 5);

	if (!QTV_NewServerConnection(cluster, arg[1], arg[2], false, false, false, false))
		return "Failed to connect to server, connection aborted\n";

	snprintf(buffer, sizeofbuffer, "Source registered \"%s\"\n", arg[1]);
	return buffer;
}
char *Cmd_Exec(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	FILE *f;
	char line[512], *res, *l;

	if (!localcommand)
	{
		if (*arg[1] == '\\' || *arg[1] == '/' || strstr(arg[1], "..") || arg[1][1] == ':')
			return "Absolute paths are prohibited.\n";
		if (!strncmp(arg[1], "usercfg/", 8))	//this is how we stop users from execing a 50gb pk3..
			return "Remote-execed configs must be in the usercfg directory\n";
	}

	f = fopen(arg[1], "rt");
	if (!f)
	{
		snprintf(buffer, sizeofbuffer, "Couldn't exec \"%s\"\n", arg[1]);
		return buffer;
	}
	else
	{
		while(fgets(line, sizeof(line)-1, f))
		{
			l = line;
			while(*(unsigned char*)l <= ' ' && *l)
				l++;
			if (*l && l[0] != '/' && l[1] != '/')
			{
				res = Rcon_Command(cluster, qtv, l, buffer, sizeofbuffer, localcommand);
				Sys_Printf(cluster, "%s", res);	//this is perhaps wrong.
			}
		}
		fclose(f);

		snprintf(buffer, sizeofbuffer, "Execed \"%s\"\n", arg[1]);
		return buffer;
	}
}

void catbuffer(char *buffer, int bufsize, char *format, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr, format);
	vsnprintf (string,sizeof(string)-1, format,argptr);
	va_end (argptr);

	Q_strncatz(buffer, string, bufsize);
}

char *Cmd_Say(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	int i;
	viewer_t *v;
	char message[8192];
	message[0] = '\0';
	for (i = 1; i < MAX_ARGS; i++)
		catbuffer(message, sizeof(message)-1, "%s%s", i==1?"":" ", arg[i]);

	for (v = cluster->viewers; v; v = v->next)
	{
		if (v->server == qtv || !qtv)
			QW_PrintfToViewer(v, "proxy: %s\n", message);
	}

	buffer[0] = '\0';
	catbuffer(buffer, sizeofbuffer, "proxy: %s\n", message);
	return buffer;
}

char *Cmd_Status(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{

	buffer[0] = '\0';

	catbuffer(buffer, sizeofbuffer, "%i sources\n", cluster->numservers);
	catbuffer(buffer, sizeofbuffer, "%i viewers\n", cluster->numviewers);
	catbuffer(buffer, sizeofbuffer, "%i proxies\n", cluster->numproxies);

	catbuffer(buffer, sizeofbuffer, "Options:\n");
	catbuffer(buffer, sizeofbuffer, " Hostname %s\n", cluster->hostname);
	
	if (cluster->chokeonnotupdated)
		catbuffer(buffer, sizeofbuffer, " Choke\n");
	if (cluster->lateforward)
		catbuffer(buffer, sizeofbuffer, " Late forwarding\n");
	if (!cluster->notalking)
		catbuffer(buffer, sizeofbuffer, " Talking allowed\n");
	if (cluster->nobsp)
		catbuffer(buffer, sizeofbuffer, " No BSP loading\n");
	if (cluster->tcpsocket != INVALID_SOCKET)
	{
		catbuffer(buffer, sizeofbuffer, " tcp port %i\n", cluster->tcplistenportnum);
	}
	if (cluster->tcpsocket != INVALID_SOCKET)
	{
		catbuffer(buffer, sizeofbuffer, " udp port %i\n", cluster->qwlistenportnum);
	}
	catbuffer(buffer, sizeofbuffer, " user connections are %sallowed\n", cluster->nouserconnects?"":"NOT ");
	catbuffer(buffer, sizeofbuffer, "\n");


	if (qtv)
	{
		catbuffer(buffer, sizeofbuffer, "Selected server: %s\n", qtv->server);
		if (qtv->sourcefile)
			catbuffer(buffer, sizeofbuffer, "Playing from file\n");
		if (qtv->sourcesock != INVALID_SOCKET)
			catbuffer(buffer, sizeofbuffer, "Connected\n");
		if (qtv->parsingqtvheader || qtv->parsingconnectiondata)
			catbuffer(buffer, sizeofbuffer, "Waiting for gamestate\n");
		if (qtv->usequkeworldprotocols)
		{
			catbuffer(buffer, sizeofbuffer, "QuakeWorld protocols\n");
			if (qtv->controller)
			{
				catbuffer(buffer, sizeofbuffer, "Controlled by %s\n", qtv->controller->name);
			}
		}
		else if (qtv->sourcesock == INVALID_SOCKET && !qtv->sourcefile)
			catbuffer(buffer, sizeofbuffer, "Connection not established\n");

		if (*qtv->modellist[1].name)
		{
			catbuffer(buffer, sizeofbuffer, "Map name %s\n", qtv->modellist[1].name);
		}
		if (*qtv->connectpassword)
			catbuffer(buffer, sizeofbuffer, "Using a password\n");

		if (qtv->disconnectwhennooneiswatching)
			catbuffer(buffer, sizeofbuffer, "Stream is temporary\n");

/*		if (qtv->tcpsocket != INVALID_SOCKET)
		{
			catbuffer(buffer, sizeofbuffer, "Listening for proxies (%i)\n", qtv->tcplistenportnum);
		}
*/

		if (qtv->bsp)
		{
			catbuffer(buffer, sizeofbuffer, "BSP (%s) is loaded\n", qtv->mapname);
		}
	}

	return buffer;
}
char *Cmd_Choke(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	if (!*arg[1])
	{
		if (cluster->chokeonnotupdated)
			return "proxy will not interpolate packets\n";
		else
			return "proxy will smooth action at the expense of extra packets\n";
	}
	cluster->chokeonnotupdated = !!atoi(arg[1]);
	return "choke-until-update set\n";
}
char *Cmd_Late(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	if (!*arg[1])
	{
		if (cluster->lateforward)
			return "forwarded streams will be artificially delayed\n";
		else
			return "forwarded streams are forwarded immediatly\n";
	}
	cluster->lateforward = !!atoi(arg[1]);
	return "late forwarding set\n";
}
char *Cmd_Talking(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	if (!*arg[1])
	{
		if (cluster->notalking)
			return "viewers may not talk\n";
		else
			return "viewers may talk freely\n";
	}
	cluster->notalking = !atoi(arg[1]);
	return "talking permissions set\n";
}
char *Cmd_NoBSP(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	if (!*arg[1])
	{
		if (cluster->nobsp)
			return "no bsps will be loaded\n";
		else
			return "attempting to load bsp files\n";
	}
	cluster->nobsp = !!atoi(arg[1]);
	return "nobsp will change at start of next map\n";
}

char *Cmd_MaxViewers(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	if (!*arg[1])
	{
		buffer[0] = '\0';
		if (cluster->maxviewers)
			snprintf(buffer, sizeofbuffer, "maxviewers is currently %i\n", cluster->maxviewers);
		else
			return "maxviewers is currently unlimited\n";
		return buffer;
	}
	cluster->maxviewers = atoi(arg[1]);
	return "maxviewers set\n";
}
char *Cmd_AllowNQ(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	if (!*arg[1])
	{
		buffer[0] = '\0';
		snprintf(buffer, sizeofbuffer, "allownq is currently %i\n", cluster->allownqclients);
		return buffer;
	}
	cluster->allownqclients = !!atoi(arg[1]);
	return "allownq set\n";
}
char *Cmd_MaxProxies(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	if (!*arg[1])
	{
		buffer[0] = '\0';
		if (cluster->maxproxies)
			snprintf(buffer, sizeofbuffer, "maxproxies is currently %i\n", cluster->maxproxies);
		else
			return "maxproxies is currently unlimited\n";
		return buffer;
	}
	cluster->maxproxies = atoi(arg[1]);
	return "maxproxies set\n";
}


char *Cmd_Ping(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	netadr_t addr;
	if (NET_StringToAddr(arg[1], &addr, 27500))
	{
		NET_SendPacket (cluster, cluster->qwdsocket, 1, "k", addr);
		return "pinged\n";
	}
	return "couldn't resolve\n";
}

char *Cmd_Help(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	return HELPSTRING;
}

char *Cmd_Echo(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	return "Poly wants a cracker.\n";
}
	
char *Cmd_Quit(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	if (!localcommand)
		return "Remote shutdown refused.\n";
	cluster->wanttoexit = true;
	return "Shutting down.\n";
}

















char *Cmd_Streams(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	catbuffer(buffer, sizeofbuffer, "Streams:\n");

	for (qtv = cluster->servers; qtv; qtv = qtv->next)
	{
		catbuffer(buffer, sizeofbuffer, "%i: %s\n", qtv->streamid, qtv->server);
	}
	return buffer;
}




char *Cmd_Disconnect(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	QTV_Shutdown(qtv);
	return "Disconnected\n";
}

char *Cmd_Record(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	if (!*arg[1])
		return "record requires a filename on the proxy's machine\n";

	if (!localcommand)
		if (*arg[1] == '\\' || *arg[1] == '/' || strstr(arg[1], "..") || arg[1][1] == ':')
			return "Absolute paths are prohibited.\n";

	if (Net_FileProxy(qtv, arg[1]))
		return "Recording to disk\n";
	else
		return "Failed to open file\n";
}
char *Cmd_Stop(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	if (Net_StopFileProxy(qtv))
		return "stopped\n";
	else
		return "not recording to disk\n";
}

char *Cmd_Reconnect(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	if (qtv->disconnectwhennooneiswatching == 2)
		return "Stream is a reverse connection (command rejected)\n";
	else if (QTV_Connect(qtv, qtv->server))
		return "Reconnected\n";
	else
		return "Failed to reconnect (will keep trying)\n";
}

char *Cmd_MVDPort(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	int news;
	int newp = atoi(arg[1]);

	if (!newp)
	{
		if (cluster->tcpsocket != INVALID_SOCKET)
		{
			closesocket(cluster->tcpsocket);
			cluster->tcpsocket = INVALID_SOCKET;
			cluster->tcplistenportnum = 0;

			return "mvd port is now closed\n";
		}
		return "Already closed\n";
	}
	else
	{
		news = Net_MVDListen(newp);

		if (news != INVALID_SOCKET)
		{
			if (cluster->tcpsocket != INVALID_SOCKET)
				closesocket(cluster->tcpsocket);
			cluster->tcpsocket = news;
			cluster->tcplistenportnum = newp;

			snprintf(buffer, sizeofbuffer, "Opened tcp port %i\n", newp);
		}
		else
			snprintf(buffer, sizeofbuffer, "Failed to open tcp port %i\n", newp);
		return buffer;
	}
}

#ifdef VIEWER
char *Cmd_Watch(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	if (!localcommand)
		return "watch is not permitted remotly\n";

	if (cluster->viewserver == qtv)
	{
		cluster->viewserver = NULL;
		return "Stopped watching\n";
	}

	cluster->viewserver = qtv;

	return "Watching\n";
}
#endif

char *Cmd_Commands(cluster_t *cluster, sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	return "fixme\n";
}

typedef struct rconcommands_s {
	char *name;
	qboolean serverspecific;	//works within a qtv context
	qboolean clusterspecific;	//works without a qtv context (ignores context)
	dispatchrconcommand_t func;
} rconcommands_t;

const rconcommands_t rconcommands[] =
{
	{"exec",		1, 1, Cmd_Exec},
	{"status",		1, 1, Cmd_Status},
	{"say",			1, 1, Cmd_Say},

	{"help",		0, 1, Cmd_Help},
	{"commands",	0, 1, Cmd_Commands},
	{"hostname",	0, 1, Cmd_Hostname},
	{"master",		0, 1, Cmd_Master},
	{"udpport",		0, 1, Cmd_UDPPort},
	 {"port",		0, 1, Cmd_UDPPort},
	{"adminpassword",0, 1, Cmd_AdminPassword},
	 {"rconpassword",0, 1, Cmd_AdminPassword},
	{"qtvlist",		0, 1, Cmd_QTVList},
	{"qtvdemolist",	0, 1, Cmd_QTVDemoList},
	{"qtv",			0, 1, Cmd_QTVConnect},
	 {"addserver",	0, 1, Cmd_QTVConnect},
	 {"connect",	0, 1, Cmd_QTVConnect},
	{"qw",			0, 1, Cmd_QWConnect},
	 {"observe",	0, 1, Cmd_QWConnect},
	{"demo",		0, 1, Cmd_MVDConnect},
	 {"playdemo",	0, 1, Cmd_MVDConnect},
	{"choke",		0, 1, Cmd_Choke},
	{"late",		0, 1, Cmd_Late},
	{"talking",		0, 1, Cmd_Talking},
	{"nobsp",		0, 1, Cmd_NoBSP},
	{"maxviewers",	0, 1, Cmd_MaxViewers},
	{"maxproxies",	0, 1, Cmd_MaxProxies},
	{"ping",		0, 1, Cmd_Ping},
	{"reconnect",	0, 1, Cmd_Reconnect},
	{"echo",		0, 1, Cmd_Echo},
	{"quit",		0, 1, Cmd_Quit},
	{"streams",		0, 1, Cmd_Streams},
	{"allownq",		0, 1, Cmd_AllowNQ},



	{"disconnect",	1, 0, Cmd_Disconnect},
	{"record",		1, 0, Cmd_Record},
	{"stop",		1, 0, Cmd_Stop},
	{"tcpport",		0, 1, Cmd_MVDPort},
	 {"mvdport",	0, 1, Cmd_MVDPort},

#ifdef VIEWER
	{"watch",		1, 0, Cmd_Watch},
#endif
	 
	{NULL}
};

char *Rcon_Command(cluster_t *cluster, sv_t *qtv, char *command, char *buffer, int sizeofbuffer, qboolean localcommand)
{
#define TOKENIZE_PUNCTUATION ""

	int i;
	char arg[MAX_ARGS][ARG_LEN];
	char *argptrs[MAX_ARGS];
	char *sid;

	for (sid = command; *sid; sid = sid++)
	{
		if (*sid == ':')
			break;
		if (*sid < '0' || *sid > '9')
			break;
	}
	if (*sid == ':')
	{
		i = atoi(command);
		command = sid+1;

		for (qtv = cluster->servers; qtv; qtv = qtv->next)
			if (qtv->streamid == i)
				break;
	}



	for (i = 0; i < MAX_ARGS; i++)
	{
		command = COM_ParseToken(command, arg[i], ARG_LEN, TOKENIZE_PUNCTUATION);
		argptrs[i] = arg[i];
	}

	if (!qtv && cluster->numservers==1)
		qtv = cluster->servers;

	buffer[0] = 0;

	if (qtv)
	{	//if there is a specific connection targetted

		for (i = 0; rconcommands[i].name; i++)
		{
			if (rconcommands[i].serverspecific)
				if (!strcmp(rconcommands[i].name, argptrs[0]))
					return rconcommands[i].func(cluster, qtv, argptrs, buffer, sizeofbuffer, localcommand);
		}
	}

	for (i = 0; rconcommands[i].name; i++)
	{
		if (!strcmp(rconcommands[i].name, argptrs[0]))
		{
			if (rconcommands[i].clusterspecific)
				return rconcommands[i].func(cluster, NULL, argptrs, buffer, sizeofbuffer, localcommand);
			else if (rconcommands[i].serverspecific)
			{
				snprintf(buffer, sizeofbuffer, "Command \"%s\" requires a targeted server.\n", arg[0]);
				return buffer;
			}
		}
	}


	snprintf(buffer, sizeofbuffer, "Command \"%s\" not recognised.\n", arg[0]);
	return buffer;
}

