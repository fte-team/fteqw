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
/*	if (*(v = Info_ValueForKey(s, key)))
	{
		// key exists, make sure we have enough room for new value, if we don't,
		// don't change it!
		if (strlen(value) - strlen(v) + strlen(s) + 1 > maxsize)
		{
			Con_TPrintf (TL_INFOSTRINGTOOLONG);
			return;
		}
	}
*/

	Info_RemoveKey (s, key);
	if (!value || !strlen(value))
		return;

	snprintf (newv, sizeof(newv)-1, "\\%s\\%s", key, value);
	newv[sizeof(newv)-1] = '\0';

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
char *Cluster_Rcon_Dispatch(cluster_t *cluster, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
	if (!strcmp(arg[0], "hostname"))
	{
		strncpy(cluster->hostname, arg[1], sizeof(cluster->hostname)-1);
		return "hostname will change at start of next map\n";	//I'm too lazy to alter the serverinfo here.
	}
	else if (!strcmp(arg[0], "master"))
	{
		netadr_t addr;

		strncpy(cluster->master, arg[1], sizeof(cluster->master)-1);
		cluster->mastersendtime = cluster->curtime;

		if (NET_StringToAddr(arg[1], &addr))	//send a ping like a qw server does. this is kinda pointless of course.
			NET_SendPacket (cluster, cluster->qwdsocket, 1, "k", addr);

		return "Master server set.\n";
	}
	else if (!strcmp(arg[0], "port"))
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
			return "Opened udp port (all connected qw clients will time out)\n";
		}
		else
			return "Failed to open udp port\n";
	}
	else if (!strcmp(arg[0], "password"))
	{
		if (!localcommand)
			return "Rejecting rcon password change.\n";

		strncpy(cluster->password, arg[1], sizeof(cluster->password)-1);
		return "Password changed.\n";
	}
	else if (!strcmp(arg[0], "connect") || !strcmp(arg[0], "addserver"))
	{
		if (!*arg[1])
			return "connect requires an ip:port parameter\n";

		memmove(arg[1]+4, arg[1], ARG_LEN-5);
		strncpy(arg[1], "tcp:", 4);

		if (!QTV_NewServerConnection(cluster, arg[1], false))
			return "Failed to connect to server, connection aborted\n";
		return "Connection registered\n";
	}
	else if (!strcmp(arg[0], "demo") || !strcmp(arg[0], "adddemo") || !strcmp(arg[0], "addfile"))
	{
		if (!*arg[1])
			return "adddemo requires an filename parameter\n";

		if (!localcommand)
			if (*arg[1] == '\\' || *arg[1] == '/' || strstr(arg[1], "..") || arg[1][1] == ':')
				return "Absolute paths are prohibited.\n";

		memmove(arg[1]+5, arg[1], ARG_LEN-6);
		strncpy(arg[1], "file:", 5);

		if (!QTV_NewServerConnection(cluster, arg[1], false))
			return "Failed to connect to server, connection aborted\n";
		return "Connection registered\n";
	}
	else if (!strcmp(arg[0], "exec"))
	{
		FILE *f;
		char line[512], *res;

		if (!localcommand)
			if (*arg[1] == '\\' || *arg[1] == '/' || strstr(arg[1], "..") || arg[1][1] == ':')
				return "Absolute paths are prohibited.\n";

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
				res = Rcon_Command(cluster, NULL, line, buffer, sizeofbuffer, localcommand);
				Sys_Printf(cluster, "%s", res);
			}
			fclose(f);
			return "Execed\n";
		}
	}
	else if (!strcmp(arg[0], "status"))
	{
		buffer[0] = '\0';

		sprintf(buffer, "%i connections\n", cluster->numservers);

		strcat(buffer, "Options:\n");
		if (cluster->chokeonnotupdated)
			strcat(buffer, " Choke\n");
		if (cluster->lateforward)
			strcat(buffer, " Late forwarding\n");
		if (!cluster->notalking)
			strcat(buffer, " Talking allowed\n");
		if (cluster->nobsp)
			strcat(buffer, " No BSP loading\n");
		strcat(buffer, "\n");

		return buffer;
	}
	else if (!strcmp(arg[0], "choke"))
	{
		cluster->chokeonnotupdated = !!atoi(arg[1]);
		return "choke-until-update set\n";
	}
	else if (!strcmp(arg[0], "late"))
	{
		cluster->lateforward = !!atoi(arg[1]);
		return "late forwarding set\n";
	}
	else if (!strcmp(arg[0], "talking"))
	{
		cluster->notalking = !atoi(arg[1]);
		return "talking permissions set\n";
	}
	else if (!strcmp(arg[0], "nobsp"))
	{
		cluster->nobsp = !!atoi(arg[1]);
		return "nobsp will change at start of next map\n";
	}

	else if (!strcmp(arg[1], "maxviewers"))
	{
		cluster->maxviewers = atoi(arg[2]);
		return "maxviewers set\n";
	}
	else if (!strcmp(arg[1], "maxproxies"))
	{
		cluster->maxproxies = atoi(arg[2]);
		return "maxproxies set\n";
	}


	else if (!strcmp(arg[0], "ping"))
	{
		netadr_t addr;
		if (NET_StringToAddr(arg[1], &addr))
		{
			NET_SendPacket (cluster, cluster->qwdsocket, 1, "k", addr);
			return "pinged\n";
		}
		return "couldn't resolve\n";
	}
	else if (!strcmp(arg[0], "help"))
	{
		return "FTEQTV proxy\nValid commands: connect, addserver, adddemo, status, choke, late, talking, nobsp, exec, password, master, hostname, port, maxviewers, maxproxies\n";
	}

	else if (!strcmp(arg[0], "mvdport"))
	{
		return "mvdport requires a targeted server. Connect first.\n";
	}

	else if (!strcmp(arg[0], "record"))
	{
		return "record requires a targeted server\n";
	}

	else if (!strcmp(arg[0], "reconnect"))
	{
		return "reconnect requires a targeted server\n";
	}

	else if (!strcmp(arg[0], "stop"))
	{	//fixme
		return "stop requires a targeted server\n";
	}

	else if (!strcmp(arg[0], "echo"))
	{
		return "Poly wants a cracker.\n";
	}
	
	else if (!strcmp(arg[0], "quit"))
	{
		cluster->wanttoexit = true;
		return "Shutting down.\n";
	}
	else
		return NULL;
}

char *Server_Rcon_Dispatch(sv_t *qtv, char *arg[MAX_ARGS], char *buffer, int sizeofbuffer, qboolean localcommand)
{
#define TOKENIZE_PUNCTUATION ""


	buffer[0] = '\0';
	if (!strcmp(arg[0], "status"))
	{
		sprintf(buffer, "%i connections\n", qtv->cluster->numservers);

		strcat(buffer, "\n");
		strcat(buffer, "Selected server: ");
		strcat(buffer, qtv->server);
		strcat(buffer, "\n");
		if (qtv->file)
			strcat(buffer, "Playing from file\n");
		if (qtv->sourcesock != INVALID_SOCKET)
			strcat(buffer, "Connected\n");
		if (qtv->parsingconnectiondata)
			strcat(buffer, "Waiting for gamestate\n");

		if (qtv->listenmvd != INVALID_SOCKET)
		{
			strcat(buffer, "Listening for proxies (");
			sprintf(arg[0], "%i", qtv->tcplistenportnum);
			strcat(buffer, arg[0]);
			strcat(buffer, ")\n");
		}

		if (qtv->bsp)
		{
			strcat(buffer, "BSP (");
			strcat(buffer, qtv->mapname);
			strcat(buffer, ") is loaded\n");
		}

		strcat(buffer, "Options:\n");
		if (qtv->cluster->chokeonnotupdated)
			strcat(buffer, " Choke\n");
		if (qtv->cluster->lateforward)
			strcat(buffer, " Late forwarding\n");
		if (!qtv->cluster->notalking)
			strcat(buffer, " Talking allowed\n");
		if (qtv->cluster->nobsp)
			strcat(buffer, " No BSP loading\n");
		strcat(buffer, "\n");

		return buffer;
	}

	else if (!strcmp(arg[0], "connect"))
	{
		if (!*arg[1])
			return "connect requires an ip:port parameter\n";

		if (QTV_Connect(qtv, arg[1]))
			return "Connected, waiting for data\n";
		else
			return "Failed (will keep trying)\n";
	}
	else if (!strcmp(arg[0], "file") || !strcmp(arg[0], "play") || !strcmp(arg[0], "playdemo"))
	{
		if (!*arg[1])
			return "file requires a filename on the proxy's machine\n";

		if (!localcommand)
			if (*arg[1] == '\\' || *arg[1] == '/' || strstr(arg[1], "..") || arg[1][1] == ':')
				return "Absolute paths are prohibited.\n";

		memmove(arg[1]+5, arg[1], ARG_LEN-6);
		strncpy(arg[1], "file:", 5);
		if (QTV_Connect(qtv, arg[1]))
			return "File opened successfully\n";
		else
			return "Failed (will keep trying)\n";
	}
	else if (!strcmp(arg[0], "record"))
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
	else if (!strcmp(arg[0], "stop"))
	{
		if (Net_StopFileProxy(qtv))
			return "stopped\n";
		else
			return "not recording to disk\n";
	}

	else if (!strcmp(arg[0], "reconnect"))
	{
		if (QTV_Connect(qtv, qtv->server))
			return "Reconnected\n";
		else
			return "Failed to reconnect (will keep trying)\n";
	}

	else if (!strcmp(arg[0], "mvdport"))
	{
		int news;
		int newp = atoi(arg[1]);

		if (!newp)
		{
			if (qtv->listenmvd != INVALID_SOCKET)
			{
				closesocket(qtv->listenmvd);
				qtv->listenmvd = INVALID_SOCKET;
				qtv->tcplistenportnum = 0;

				return "mvd port is now closed\n";
			}
			return "Already closed\n";
		}
		else
		{
			news = Net_MVDListen(newp);

			if (news != INVALID_SOCKET)
			{
				closesocket(qtv->listenmvd);
				qtv->listenmvd = news;
				qtv->tcplistenportnum = newp;
				return "Opened tcp port\n";
			}
			else
				return "Failed to open tcp port\n";
		}
	}

	else if (!strcmp(arg[0], "exec"))
	{
		FILE *f;
		char line[512], *res;

		if (!localcommand)
			if (*arg[1] == '\\' || *arg[1] == '/' || strstr(arg[1], "..") || arg[1][1] == ':')
				return "Absolute paths are prohibited.\n";

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
				res = Rcon_Command(qtv->cluster, qtv, line, buffer, sizeofbuffer, localcommand);
				Sys_Printf(qtv->cluster, "%s", res);
			}
			fclose(f);
			return "Execed\n";
		}
	}

	else
	{
		return NULL;
	}
}

char *Rcon_Command(cluster_t *cluster, sv_t *qtv, char *command, char *buffer, int sizeofbuffer, qboolean localcommand)
{
#define TOKENIZE_PUNCTUATION ""

	int i;
	char arg[MAX_ARGS][ARG_LEN];
	char *argptrs[MAX_ARGS];
	char *result;

	for (i = 0; i < MAX_ARGS; i++)
	{
		command = COM_ParseToken(command, arg[i], ARG_LEN, TOKENIZE_PUNCTUATION);
		argptrs[i] = arg[i];
	}

	if (qtv)
	{	//if there is a specific connection targetted
		result = Server_Rcon_Dispatch(qtv, argptrs, buffer, sizeofbuffer, localcommand);
		if (result)
			return result;
	}
	else if (cluster->numservers == 1)
	{	//if it's a single-connection proxy
		result = Server_Rcon_Dispatch(cluster->servers, argptrs, buffer, sizeofbuffer, localcommand);
		if (result)
			return result;
	}

	result = Cluster_Rcon_Dispatch(cluster, argptrs, buffer, sizeofbuffer, localcommand);
	if (result)
		return result;

	snprintf(buffer, sizeofbuffer, "Command \"%s\" not recognised.\n", arg[0]);
	return buffer;
}

