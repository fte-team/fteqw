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
		printf ("Key has a slash\n");
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
		printf ("Key has a slash\n");
		return;
	}

	if (strstr (key, "\"") || strstr (value, "\"") )
	{
		printf ("Key has a quote\n");
		return;
	}

	if (strlen(key) >= MAX_INFO_KEY || strlen(value) >= MAX_INFO_KEY)
	{
		printf ("Key or value is too long\n");
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

	_snprintf (newv, sizeof(newv)-1, "\\%s\\%s", key, value);
	newv[sizeof(newv)-1] = '\0';

	if ((int)(strlen(newv) + strlen(s) + 1) > maxsize)
	{
		printf ("info buffer is too small\n");
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

char *Rcon_Command(sv_t *qtv, char *command, char *buffer, int sizeofbuffer, qboolean localcommand)
{
#define TOKENIZE_PUNCTUATION ""

	int i;
#define MAX_ARGS 8
#define ARG_LEN 512
	char arg[MAX_ARGS][ARG_LEN];

	for (i = 0; i < MAX_ARGS; i++)
		command = COM_ParseToken(command, arg[i], ARG_LEN, TOKENIZE_PUNCTUATION);

	buffer[0] = '\0';
	if (!strcmp(arg[0], "status"))
	{
		strcat(buffer, "\n");
		strcat(buffer, "Server: ");
		strcat(buffer, qtv->server);
		strcat(buffer, "\n");
		if (qtv->file)
			strcat(buffer, "Playing from file\n");
		if (qtv->sourcesock != INVALID_SOCKET)
			strcat(buffer, "Connected\n");
		if (qtv->parsingconnectiondata)
			strcat(buffer, "Waiting for gamestate\n");

		if (qtv->listenmvd != INVALID_SOCKET)
			strcat(buffer, "Listening for proxies\n");
		if (qtv->qwdsocket != INVALID_SOCKET)
			strcat(buffer, "Listening for qwcl\n");

		if (qtv->bsp)
		{
			strcat(buffer, "BSP (");
			strcat(buffer, qtv->mapname);
			strcat(buffer, ") is loaded\n");
		}

		strcat(buffer, "Options:\n");
		if (qtv->chokeonnotupdated)
			strcat(buffer, " Choke\n");
		if (qtv->lateforward)
			strcat(buffer, " Late forwarding\n");
		if (!qtv->notalking)
			strcat(buffer, " Talking allowed\n");
		strcat(buffer, "\n");

		return buffer;
	}
	else if (!strcmp(arg[0], "option"))
	{
		if (!*arg[1])
			return "option X Y\nWhere X is choke/late/talking/hostname/nobsp and Y is (mostly) 0/1\n";

		if (!strcmp(arg[1], "choke"))
			qtv->chokeonnotupdated = !!atoi(arg[2]);
		else if (!strcmp(arg[1], "late"))
			qtv->lateforward = !!atoi(arg[2]);
		else if (!strcmp(arg[1], "talking"))
			qtv->notalking = !atoi(arg[2]);
		else if (!strcmp(arg[1], "nobsp"))
		{
			qtv->nobsp = !!atoi(arg[2]);
			return "nobsp will change at start of next map\n";
		}
		else if (!strcmp(arg[1], "hostname"))
		{
			strncpy(qtv->hostname, arg[2], sizeof(qtv->hostname)-1);
			return "hostname will change at start of next map\n";	//I'm too lazy to alter the serverinfo here.
		}
		else if (!strcmp(arg[1], "master"))
		{
			strncpy(qtv->master, arg[2], sizeof(qtv->master)-1);
			qtv->mastersendtime = qtv->curtime;
		}
		else
			return "Option not recognised\n";
		return "Set\n";
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
	else if (!strcmp(arg[0], "file"))
	{
		if (!*arg[1])
			return "file requires a filename on the proxy's machine\n";

		if (!localcommand)
			if (*arg[1] == '\\' || *arg[1] == '/' || strstr(arg[1], "..") || arg[1][1] == ':')
				return "Absolute paths are prohibited.\n";

		memmove(arg[1]+5, arg[1], sizeof(arg[1])-6);
		strncpy(arg[1], "file:", 5);
		if (QTV_Connect(qtv, arg[1]))
			return "File opened successfully\n";
		else
			return "Failed (will keep trying)\n";
	}
	else if (!strcmp(arg[0], "help"))
	{
		return "FTEQTV proxy\nValid commands: connect, file, status, option\n";
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

	else if (!strcmp(arg[0], "ping"))
	{
		netadr_t addr;
		if (NET_StringToAddr(arg[1], &addr))
		{
			NET_SendPacket (qtv->qwdsocket, 1, "k", addr);
			return "pinged\n";
		}
		return "couldn't resolve\n";
	}

	else if (!strcmp(arg[0], "port"))
	{
		int news;
		int newp = atoi(arg[1]);
		news = QW_InitUDPSocket(newp);

		if (news != INVALID_SOCKET)
		{
			qtv->mastersendtime = qtv->curtime;
			closesocket(qtv->qwdsocket);
			qtv->qwdsocket = news;
			qtv->qwlistenportnum = newp;
			return "Opened udp port\n";
		}
		else
			return "Failed to open udp port\n";
	}

	else if (!strcmp(arg[0], "password"))
	{
		if (!localcommand)
			return "Rejecting rcon password change.\n";

		strncpy(qtv->password, arg[1], sizeof(qtv->password)-1);
		return "Password changed.\n";
	}
	else
		return "Unrecognised command.\n";

}

