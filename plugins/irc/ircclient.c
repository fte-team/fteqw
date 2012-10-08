//Released under the terms of the gpl as this file uses a bit of quake derived code. All sections of the like are marked as such
// changes name to while in channel
// mode command
// Spike can you implement nick tab completion. ~moodles
// need option for whois on receiving PM
// bug: setting channel to private, crashes fte when trying to join it.
// http://www.mirc.net/raws/
// http://www.ircle.com/reference/commands.shtml


#include "../plugin.h"
#include <time.h>
#include <ctype.h>

#define irccvars "IRC Console Variables"
vmcvar_t	irc_debug = {"irc_debug", "0", irccvars, 0};
vmcvar_t	irc_motd = {"irc_motd", "1", irccvars, 0};
vmcvar_t	irc_nick = {"irc_nick", "anonymous", irccvars, 0};
vmcvar_t	irc_altnick = {"irc_altnick", "unnamed", irccvars, 0};
vmcvar_t	irc_realname = {"irc_realname", "FTE IRC-Plugin http://www.fteqw.com", irccvars, 0};
vmcvar_t	irc_ident = {"irc_ident", "FTE", irccvars, 0};
vmcvar_t	irc_timestamp = {"irc_timestamp", "0", irccvars, 0};
#undef irccvars

vmcvar_t	*cvarlist[] ={
	&irc_debug,
	&irc_motd,
	&irc_nick,
	&irc_altnick,
	&irc_realname,
	&irc_ident,
	&irc_timestamp
};


char commandname[64]; // belongs to magic tokenizer
char subvar[9][1000]; // etghack
char casevar[9][1000]; //numbered_command
time_t seconds; // irc_connect
int irc_connecting = 0;
char servername[64]; // store server name
#define CURRENTCONSOLE "" // need to make this the current console
#define DEFAULTCONSOLE ""
#define RELEASE "__DATE__"

void (*Con_TrySubPrint)(char *subname, char *text);
void Con_FakeSubPrint(char *subname, char *text)
{
	Con_Print(text);
}
void Con_SubPrintf(char *subname, char *format, ...)
{
	va_list		argptr;
	static char		string[1024];
	char lwr[128];
	int i;

	va_start (argptr, format);
	vsnprintf (string, sizeof(string), format,argptr);
	va_end (argptr);

	if (format[0] == '^' && format[1] == '2')
	{
		//Cmd_AddText("say $\\", false);
		//Cmd_AddText(string+2, false);
		//Cmd_AddText("\n", false);
	}

	strlcpy(lwr, commandname, sizeof(lwr));
	for (i = strlen(lwr); *subname && i < sizeof(lwr)-2; i++, subname++)
	{
		if (*subname >= 'A' && *subname <= 'Z')
			lwr[i] = *subname - 'A' + 'a';
		else
			lwr[i] = *subname;
	}
	lwr[i] = '\0';
	Con_TrySubPrint(lwr, string);
}


//porting zone:


	#define COLOURGREEN	"^2"
	#define COLORWHITE "^7"
	#define COLOURWHITE "^7" // word
	#define COLOURRED "^1"
	#define COLOURYELLOW "^3"
	#define COLOURPURPLE "^5"
	#define COLOURBLUE "^4"
	#define COLOURINDIGO "^6"


	#define IRC_Malloc	malloc
	#define IRC_Free		free

	#define TL_NETGETPACKETERROR "NET_GetPacket Error %s\n"

	#define TOKENSIZE 1024
	char		com_token[TOKENSIZE];

	char *COM_Parse (char *data)	//this is taken out of quake
	{
		int		c;
		int		len;

		len = 0;
		com_token[0] = 0;

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
		}


	// handle quoted strings specially
		if (c == '\"')
		{
			data++;
			while (1)
			{
				if (len >= TOKENSIZE-1)
					return data;

				c = *data++;
				if (c=='\"' || !c)
				{
					com_token[len] = 0;
					return data;
				}
				com_token[len] = c;
				len++;
			}
		}

	// parse a regular word
		do
		{
			if (len >= TOKENSIZE-1)
				return data;

			com_token[len] = c;
			data++;
			len++;
			c = *data;
		} while (c>32);

		com_token[len] = 0;
		return data;
	}










//\r\n is used to end a line.
//meaning \0s are valid.
//but never used cos it breaks strings



#define IRC_MAXNICKLEN 32	//9 and a null term
#define IRC_MAXMSGLEN 512


char defaultuser[IRC_MAXNICKLEN+1] = "Unknown";


typedef struct {
	char server[64];
	int port;

	qhandle_t socket;

	char nick[IRC_MAXNICKLEN];
	char pwd[64];
	char realname[128];
	char hostname[128];
	char autochannels[128];
	int nickcycle;

	char defaultdest[IRC_MAXNICKLEN];//channel or nick

	char bufferedinmessage[IRC_MAXMSGLEN+1];	//there is a max size for protocol. (conveinient eh?) (and it's text format)
	int bufferedinammount;
} ircclient_t;
ircclient_t *ircclient;






void IRC_InitCvars(void)
{
	vmcvar_t *v;
	int i;
	for (v = cvarlist[0],i=0; i < sizeof(cvarlist)/sizeof(cvarlist[0]); v++, i++)
		v->handle = Cvar_Register(v->name, v->string, v->flags, v->group);
}

int IRC_CvarUpdate(void) // perhaps void instead?
{
	vmcvar_t *v;
	int i;
	for (v = cvarlist[0],i=0; i < sizeof(cvarlist)/sizeof(cvarlist[0]); v++, i++)
		v->modificationcount = Cvar_Update(v->handle, &v->modificationcount, v->string, &v->value);
	return 0;
}

void IRC_Command(char *args);
qintptr_t IRC_ExecuteCommand(qintptr_t *args);
qintptr_t IRC_ConExecuteCommand(qintptr_t *args);
qintptr_t IRC_Frame(qintptr_t *args);

qintptr_t Plug_Init(qintptr_t *args)
{
	if (	Plug_Export("Tick", IRC_Frame) &&
		Plug_Export("ExecuteCommand", IRC_ExecuteCommand))
	{
		if (BUILTINISVALID(GetPluginName))
		{
			char *s;
			GetPluginName(0, commandname, sizeof(commandname));
			while((s = strchr(commandname, '/')))
			{	//strip off the leading slashes.
				memmove(commandname, s+1, strlen(s));
			}
		}
		else
			strlcpy(commandname, "irc", sizeof(commandname));

		Cmd_AddCommand(commandname);

		if (!Plug_Export("ConExecuteCommand", IRC_ConExecuteCommand))
		{
			Con_Print("IRC Client Plugin Loaded in single-console mode\n");
			Con_TrySubPrint = Con_FakeSubPrint;
		}
		else
		{
			Con_Print("IRC Client Plugin Loaded\n");
			Con_TrySubPrint = Con_SubPrint;
		}

		IRC_InitCvars();
		return true;
	}
	else
	{
		Con_Print("IRC Client Plugin failed\n");
	}
	return false;
}



qintptr_t IRC_ExecuteCommand(qintptr_t *args)
{
	char cmd[8];
	Cmd_Argv(0, cmd, sizeof(cmd));
	if (!strcmp(cmd, commandname))
	{
		IRC_Command(ircclient?ircclient->defaultdest:"");
		return true;
	}
	return false;
}
qintptr_t IRC_ConExecuteCommand(qintptr_t *args)
{
	char buffer[256];
	int cmdlen;
	Cmd_Argv(0, buffer, sizeof(buffer));
	if (!ircclient)
	{
		if (*buffer == '/')
			IRC_Command("");
		else
			Con_TrySubPrint(buffer, "You were disconnected\n");
		return true;
	}
	cmdlen = strlen(commandname);
	IRC_Command(buffer+cmdlen);
	return true;
}

void IRC_AddClientMessage(ircclient_t *irc, char *msg)
{
	char output[4096];

	strcpy(output, msg);
	strcat(output, "\n");

	Net_Send(irc->socket, output, strlen(output));	//FIXME: This needs rewriting to cope with errors.

	if (irc_debug.value == 1) { Con_SubPrintf(DEFAULTCONSOLE,COLOURYELLOW "<< %s \n",msg); }
}

ircclient_t *IRC_Connect(char *server, int defport)
{
	ircclient_t *irc;
	unsigned long _true = true;

	seconds = time (NULL); // when we connected
	irc_connecting = 1; //we are connecting.. so lets do the nickname stuff

	irc = IRC_Malloc(sizeof(ircclient_t));
	if (!irc)
		return NULL;

	memset(irc, 0, sizeof(ircclient_t));


	irc->socket = Net_TCPConnect(server, defport);	//port is only used if the url doesn't contain one. It's a default.

	//not yet blocking. So no frequent attempts please...
	//non blocking prevents connect from returning worthwhile sensible value.
	if ((qintptr_t)irc->socket < 0)
	{
		Con_Printf("IRC_OpenSocket: couldn't connect\n");
		IRC_Free(irc);
		return NULL;
	}

	strlcpy(irc->server, server, sizeof(irc->server));

	IRC_CvarUpdate();

	strcpy(irc->nick, irc_nick.string);
	strcpy(irc->realname, "anonymous");

	strcpy(irc->hostname, "anonymous");

	strcpy(irc->autochannels, "");

//	gethostname(irc->hostname, sizeof(irc->hostname));
//	irc->hostname[sizeof(irc->hostname)-1] = 0;

	return irc;
}
void IRC_SetPass(ircclient_t *irc, char *pass)
{
	if (pass != "")
		IRC_AddClientMessage(irc, va("PASS %s", pass));
}
void IRC_SetNick(ircclient_t *irc, char *nick)
{
	strlcpy(irc->nick, nick, sizeof(irc->nick)); // broken
	IRC_AddClientMessage(irc, va("NICK %s", irc->nick));
	irc->nickcycle=0;
}
void IRC_SetUser(ircclient_t *irc, char *user)
{
	IRC_CvarUpdate();

	IRC_AddClientMessage(irc, va("USER %s %s %s :%s", irc_ident.string, irc->hostname, irc->server, irc_realname.string));
}
void IRC_JoinChannel(ircclient_t *irc, char *channel, char *key) // i screwed up, its actually: <channel>{,<channel>} [<key>{,<key>}]
{
	if ( *channel != '#' )
		IRC_AddClientMessage(irc, va("JOIN #%s %s", channel,key));
	else
		IRC_AddClientMessage(irc, va("JOIN %s %s", channel,key));
}


/*

ATTN: Spike

# (just for reference) == Ctrl+K in mirc to put the color code symbol in

now to have a background color, you must specify a forground color first (#0,15)

, denotes end of forground color, and start of background color

irc colors work in many strange ways:

#0-#15 for forground color // the code currently converts to this one, which is not the "proper" irc way, read the next one to understand. Still need to support it, just not output as it.

#00-#15 for forground color (note #010 to #015 is not valid) --- this is the "proper" irc way, because I could say "#11+1=2" (which means I want 1+1=2 to appear black (1), but instead it will come out as indigo (11) and look like this: +1=2)

background examples: (note

#0,15 (white forground, light gray background)

#00,15 (white forground, light gray background) // proper way

#15,0 (white forground, light gray background)

#15,00 (white forground, light gray background) // proper way

I hope this makes sense to you, to be able to edit the IRC_FilterMircColours function ~ Moodles

*/
void IRC_FilterMircColours(char *msg)
{
	int i;
	int chars;
	while(*msg)
	{
		if (*msg == 3)
		{
			chars = 2;
			if (msg[1] >= '0' && msg[1] <= '9')
			{
				i = msg[1]- '0';
				if (msg[2] >= '0' && msg[2] <= '9')
				{
					i = i*10 + (msg[2]-'0');
					chars = 3;
				}
			}
			else
				i = msg[1];
			switch(i)
			{
			case 0:
				msg[1] = '7';	//white
				break;
			case 1:
				msg[1] = '0';	//black
				break;
			case 2:
				msg[1] = '4';	//darkblue
				break;
			case 3:
				msg[1] = '2';	//darkgreen
				break;
			case 4:
				msg[1] = '1';	//red
				break;
			case 5:
				msg[1] = '1';	//brown
				break;
			case 6:
				msg[1] = '5';	//purple
				break;
			case 7:
				msg[1] = '3';	//orange
				break;
			case 8:
				msg[1] = '3';	//yellow
				break;
			case 9:
				msg[1] = '2';	//lightgreen
				break;
			case 10:
				msg[1] = '6';	//darkcyan
				break;
			case 11:
				msg[1] = '6';	//lightcyan
				break;
			case 12:
				msg[1] = '4';	//lightblue
				break;
			case 13:
				msg[1] = '5';	//pink
				break;
			case 14:
				msg[1] = '7';	//grey
				break;
			case 15:
				msg[1] = '7';	//lightgrey
				break;
			default:
				msg++;
				continue;
			}
			*msg = '^';
			msg+=2;
			if (chars==3)
				memmove(msg, msg+1, strlen(msg));
			continue;
		}
		msg++;
	}
}

#define IRC_DONE 0
#define IRC_CONTINUE 1
#define IRC_KILL 2

void magic_tokenizer(int word,char *thestring)
{
	char *temp;
	int i = 1;

	strcpy(casevar[1],thestring);

	temp = strchr(casevar[1], ' ');

	while (i < 8)
	{
		i++;

		if (temp != NULL)
		{
			strcpy(casevar[i],temp+1);
		}
		else
		{
			strcpy(casevar[i], "");
		}

		temp=strchr(casevar[i], ' ');

	}

}

void magic_etghack(char *thestring)
{
	char *temp;
	int i = 1;

	strcpy(subvar[1],thestring);

	temp = strchr(subvar[1], ' ');

	while (i < 8)
	{
		i++;

		if (temp != NULL)
		{
			strcpy(subvar[i],temp+1);
		}
		else
		{
			strcpy(subvar[i], "");
		}

		temp=strchr(subvar[i], ' ');

	}

}


//==================================================

void numbered_command(int comm,char *msg,ircclient_t *irc) // move vars up 1 more than debug says
{
	magic_tokenizer(0,msg);

	switch (comm)
	{
	case 001:
	case 002:
	case 003:
	case 004:
	case 005:
	{
		irc_connecting = 0; // ok we are connected

		Con_SubPrintf(DEFAULTCONSOLE, COLOURYELLOW "SERVER STATS: %s\n",casevar[3]);
		return;
	}
	case 250:
	case 251:
	case 252:
	case 253:
	case 254:
	case 255:
	case 265:
	case 266:
	{
		Con_SubPrintf(DEFAULTCONSOLE, COLOURYELLOW "SERVER STATS: %s\n",casevar[3]);
		return;
	}
	case 301: /* #define RPL_AWAY             301 */
	{
		char *username = strtok(casevar[3], " ");
		char *awaymessage = casevar[4]+1;

		Con_SubPrintf(DEFAULTCONSOLE,"WHOIS: <%s> (Away Message: %s)\n",username,awaymessage);
		return;
	}
	case 305: /* RPL_UNAWAY */
	case 306: /* RPL_NOWAWAY */
	{
		char *away = casevar[3]+1;

		Con_SubPrintf(CURRENTCONSOLE,"%s\n",away);
		return;
	}
	case 311: /* #define RPL_WHOISUSER        311 */
	{
		char *username = strtok(casevar[3], " ");
		char *ident = strtok(casevar[4], " ");
		char *address = strtok(casevar[5], " ");
		char *realname = casevar[7]+1;

		Con_SubPrintf(DEFAULTCONSOLE,"WHOIS: <%s> (Ident: %s) (Address: %s) (Realname: %s) \n", username, ident, address, realname);
		return;
	}
	case 312: /* #define RPL_WHOISSERVER      312 */ //seems to be /whowas also
	{
		char *username = strtok(casevar[3], " ");
		char *serverhostname = strtok(casevar[4], " ");
		char *servername = casevar[5]+1;

		Con_SubPrintf(DEFAULTCONSOLE,"WHOIS: <%s> (Server: %s) (Server Name: %s) \n", username, serverhostname, servername);
		return;
	}
	case 313: /* RPL_WHOISOPERATOR */
	{
		char *username = strtok(casevar[3], " ");
		char *isoperator = casevar[4]+1;

		Con_SubPrintf(DEFAULTCONSOLE,"WHOIS: <%s> (%s)\n", username,isoperator);

		return;
	}
	case 317: /* #define RPL_WHOISIDLE        317 */
	{
		char *username = strtok(casevar[3], " ");
		char *secondsidle = strtok(casevar[4], " ");
		char *signontime = strtok(casevar[5], " ");
		time_t t;
		const struct tm *tm;
		char buffer[100];

		t=strtoul(signontime, 0, 0);
		tm=localtime(&t);

		strftime (buffer, 100, "%a %b %d %H:%M:%S", tm);

		Con_SubPrintf(DEFAULTCONSOLE,"WHOIS: <%s> (Idle Time: %s seconds) (Signon Time: %s) \n", username, secondsidle, buffer);
		return;
	}
	case 318: /* #define RPL_ENDOFWHOIS       318 */
	{
		char *endofwhois = casevar[4]+1;

		Con_SubPrintf(DEFAULTCONSOLE,"WHOIS: %s\n", endofwhois);

		return;
	}
	case 319: /* #define RPL_WHOISCHANNELS    319 */
	{
		char *username = strtok(casevar[3], " ");
		char *channels = casevar[4]+1;

		Con_SubPrintf(DEFAULTCONSOLE,"WHOIS: <%s> (Channels: %s)\n",username,channels); // need to remove the space from the end of channels
		return;
	}
	case 321:
	{
		Con_SubPrintf("list", "Start /LIST\n");

		return;
	}
	case 322: /* #define RPL_LIST             322 */
	{
		char *channel = strtok(casevar[3], " ");
		char *users = strtok(casevar[4], " ");
		char *topic = casevar[5]+1;

		Con_SubPrintf("list", "^1Channel:^7 %s ^1Users:^7 %s ^1Topic:^7 %s\n\n", channel,users,topic);
		return;
	}
	case 323:
	{
		char *endoflist = casevar[3]+1;

		Con_SubPrintf("list", "%s\n",endoflist);

		return;
	}
	case 366:
	{
		char *channel = strtok(casevar[3], " ");
		char *endofnameslist = casevar[4]+1;

		Con_SubPrintf(channel,"%s\n",endofnameslist);
		return;
	}
	case 372:
	case 375:
	case 376:
	{
		char *motdmessage = casevar[3]+1;

		IRC_CvarUpdate();

		if (irc_motd.value == 2)
			Con_SubPrintf(DEFAULTCONSOLE, "MOTD: %s\n", motdmessage);
		else if (irc_motd.value)
			Con_SubPrintf(DEFAULTCONSOLE, "%s\n", motdmessage);

		if (*irc->autochannels)
			IRC_JoinChannel(ircclient,irc->autochannels,""); // note to self... "" needs to be the channel key.. so autochannels needs a recoded

		return;
	}
	case 378:
	{
		Con_SubPrintf(DEFAULTCONSOLE, "%s\n", msg);
		return;
	}
	case 401:
	case 403:
	case 404:
	case 405:
	{
		char *username = strtok(casevar[3], " ");
		char *error = casevar[4]+1;

		Con_SubPrintf(DEFAULTCONSOLE, COLOURRED "ERROR <%s>: %s\n",username,error);
		return;
	}
	case 432: /* #define ERR_ERRONEUSNICKNAME 432 */
	{
		Con_SubPrintf(DEFAULTCONSOLE, "Erroneous/invalid nickname given\n");
		return;
	}
	case 433: /* #define ERR_NICKNAMEINUSE    433 */
	case 438:
	case 453:
	{
		char *nickname = strtok(casevar[4], " ");
		char *badnickname = ":Nickname";
		char *seedednick;

		if ( !strcasecmp(nickname,badnickname) ) // bug with ircd, the nickname actually shifts position.
		{
			nickname = strtok(casevar[3], " ");
		}

		IRC_CvarUpdate();

		Con_SubPrintf(DEFAULTCONSOLE, COLOURRED "ERROR: <%s> is already in use.\n",nickname);

		if ( !strcmp(nickname,irc_nick.string) && (irc_connecting == 1) )
		{
			IRC_SetNick(irc, irc_altnick.string);
		}
		else if ( !strcmp(nickname,irc_altnick.string) && (irc_connecting == 1) )
		{
			Con_SubPrintf(DEFAULTCONSOLE, COLOURRED "ERROR: <%s> AND <%s> both in use. Attempting generic nickname.\n",irc_nick.string,irc_altnick.string);
			seedednick = va("FTE%i",rand());

			IRC_SetNick(irc, seedednick);

		}
		else
		{
			if (irc_connecting == 1)
			{
				seedednick = va("FTE%i",rand());
				IRC_SetNick(irc, seedednick);
			}
		}

		return;
	}
	case 471: /* ERR_CHANNELISFULL */
	{
		char *channel = strtok(casevar[3], " ");
		char *error = casevar[4]+1;

		Con_SubPrintf(DEFAULTCONSOLE, COLOURRED "ERROR: <%s>: %s (Channel is full and has reached user limit)\n",channel,error);
		return;
	}
	case 472: /* ERR_UNKNOWNMODE */
	{
		char *mode = strtok(casevar[3], " ");
		char *error = casevar[4]+1;

		Con_SubPrintf(DEFAULTCONSOLE, COLOURRED "ERROR: <%s>: %s (Unknown mode)\n",mode,error);
		return;
	}
	case 473: /* ERR_INVITEONLYCHAN */
	{
		char *channel = strtok(casevar[3], " ");
		char *error = casevar[4]+1;

		Con_SubPrintf(DEFAULTCONSOLE, COLOURRED "ERROR: <%s>: %s (Invite only)\n",channel,error);
		return;
	}
	case 474: /* ERR_BANNEDFROMCHAN */
	{
		char *channel = strtok(casevar[3], " ");
		char *error = casevar[4]+1;

		Con_SubPrintf(DEFAULTCONSOLE, COLOURRED "ERROR: <%s>: %s (You are banned)\n",channel,error);
		return;
	}
	case 475: /* ERR_BADCHANNELKEY */
	{
		char *channel = strtok(casevar[3], " ");
		char *error = casevar[4]+1;

		Con_SubPrintf(DEFAULTCONSOLE, COLOURRED "ERROR: <%s>: %s (Need the correct channel key. Example: /join %s bananas)\n",channel,error,channel);
		return;
	}
	case 482: /* ERR_CHANOPRIVSNEEDED */
	{
		char *channel = strtok(casevar[3], " ");
		char *error = casevar[4]+1;

		Con_SubPrintf(DEFAULTCONSOLE, COLOURRED "ERROR: <%s>: %s (Need +o or @ status)\n",channel,error,channel);
		return;
	}
	}

	Con_SubPrintf(DEFAULTCONSOLE, "%s\n", msg); // if no raw number exists, print the thing
}

//==================================================

int IRC_ClientFrame(ircclient_t *irc)
{
	char prefix[64];
	int ret;
	char *nextmsg, *msg;
	char *raw;
	char *temp;
	char temp2[4096];
	char var[9][1000];

	int i = 1;

	ret = Net_Recv(irc->socket, irc->bufferedinmessage+irc->bufferedinammount, sizeof(irc->bufferedinmessage)-1 - irc->bufferedinammount);
	if (ret == 0)
		return IRC_KILL;
	if (ret < 0)
	{
		if (ret == N_WOULDBLOCK)
		{
			if (!irc->bufferedinammount)	//if we are half way through a message, read any possible conjunctions.
				return IRC_DONE;	//remove
		}
		else
			return IRC_KILL;
	}

	if (ret>0)
		irc->bufferedinammount+=ret;
	irc->bufferedinmessage[irc->bufferedinammount] = '\0';
	nextmsg = strstr(irc->bufferedinmessage, "\r\n");
	if (!nextmsg)
		return IRC_DONE;

	*nextmsg = '\0';
	nextmsg+=2;

	msg = irc->bufferedinmessage;

	strcpy(var[1],msg);

	temp = strchr(var[1], ' ');

	while (i < 8)
	{
		i++;

		if (temp != NULL)
		{
			strcpy(var[i],temp+1);
		}
		else
		{
			strcpy(var[i], "");
		}

		temp=strchr(var[i], ' ');

	}

	strcpy(temp2,var[2]);

	raw = strtok(temp2, " ");

	IRC_CvarUpdate(); // is this the right place for it?

	raw = strtok(var[2], " ");

	if (irc_debug.value == 1) { Con_SubPrintf(DEFAULTCONSOLE,COLOURRED "!!!!! ^11: %s ^22: %s ^33: %s ^44: %s ^55: %s ^66: %s ^77: %s ^88: %s\n",var[1],var[2],var[3],var[4],var[5],var[6],var[7],var[8]); }

	if (*msg == ':')	//we need to strip off the prefix
	{
		char *sp = strchr(msg, ' ');
		if (!sp)
		{
			Con_SubPrintf(DEFAULTCONSOLE, "Ignoring bad message\n%s\n", msg);
			memmove(irc->bufferedinmessage, nextmsg, irc->bufferedinammount - (msg-irc->bufferedinmessage));
			irc->bufferedinammount-=nextmsg-irc->bufferedinmessage;
			return IRC_CONTINUE;
		}

		if (sp-msg >= sizeof(prefix))
			strlcpy(prefix, msg+1, sizeof(prefix));
		else
			strlcpy(prefix, msg+1, sp-msg);

		msg = sp;
		while(*msg == ' ')
			msg++;
	}
	else
		strcpy(prefix, irc->server);

	if (!strncmp(var[1], "NOTICE AUTH ", 12))
	{
		Con_SubPrintf(DEFAULTCONSOLE, COLOURGREEN "SERVER NOTICE: %s\n", var[3]+1);
	}
	else if (!strncmp(var[1], "PING ", 5))
	{
		IRC_AddClientMessage(irc, va("PONG %s", var[2]));
	}
	else if (!strncmp(var[2], "NOTICE ", 6))
	{
		char *exc = strchr(prefix, '!');
		char *col = strchr(msg+6, ':');
		char *end;
		char *to = msg + 7;
		char *servernotice = var[4]+1;
		char *etghack;

		if (!strncmp(var[4]+1, "\1", 1))
		{
			char delimiters[] = "!";
			char *username = strtok(var[1]+1, delimiters);
			char *ctcpreplytype = strtok(var[4]+2, " ");
			char *ctcpreply = var[5];

			Con_SubPrintf(DEFAULTCONSOLE,"<CTCP Reply> %s FROM %s: %s\n",ctcpreplytype,username,ctcpreply); // need to remove the last char on the end of ctcpreply
		}
		else if (exc && col)
		{
			*col = '\0';
			col++;

			while(*to <= ' ' && *to)
				to++;
			for (end = to + strlen(to)-1; end >= to && *end <= ' '; end--)
				*end = '\0';
			if (!strcmp(to, irc_nick.string))
				to = prefix;	//This was directed straight at us.
								//So change the 'to', to the 'from'.

			for (end = to; *end; end++)
			{
				if (*end >= 'A' && *end <= 'Z')
					*end = *end + 'a' - 'A';
			}

			*exc = '\0';
			if (!strncmp(col, "\001", 1))
			{
				end = strchr(col+1, '\001');
				if (end)
					*end = '\0';
				if (!strncmp(col+1, "ACTION ", 7))
				{
					IRC_FilterMircColours(col+8);
					Con_SubPrintf(to, COLOURGREEN "***%s "COLORWHITE"%s\n", prefix, col+8);	//from client
				}
			}
			else
			{
				IRC_FilterMircColours(col);
				Con_SubPrintf(DEFAULTCONSOLE, COLOURGREEN "NOTICE: -%s- %s\n", prefix, col);	//from client
			}
		}
		else
		{

		etghack = strtok(var[1],"\n");

		Con_SubPrintf(DEFAULTCONSOLE, COLOURGREEN "SERVER NOTICE: <%s> %s\n", prefix, etghack);

		strcpy(servername,prefix);

		while (1)
		{
			etghack = strtok(NULL, "\n");

			if (etghack == NULL)
			{
				break;
				break;
			}

			magic_etghack(etghack);

			if (atoi(subvar[2]) != 0)
				numbered_command(atoi(subvar[2]),etghack,ircclient);
			else
				Con_SubPrintf(DEFAULTCONSOLE, COLOURGREEN "SERVER NOTICE: <%s> %s\n", prefix, subvar[4]);

		}
	}

	}
	else if (!strncmp(var[2], "PRIVMSG ", 7))	//no autoresponses to notice please, and any autoresponses should be in the form of a notice
	{
		char *exc = strchr(prefix, '!');
		char *col = strchr(msg+6, ':');
		char *end;
		char *to = msg + 7;

		//message takes the form :FROM PRIVMSG TO :MESSAGE

		if (BUILTINISVALID(LocalSound))
			LocalSound ("misc/talk.wav");

		if ((!stricmp(var[4]+1, "\1VERSION\1")) && (!strncmp(var[2], "PRIVMSG ", 7)))
		{
			char *username;
			char delimiters[] = "!";

			username = strtok(var[1]+1, delimiters);

			IRC_AddClientMessage(irc, va("NOTICE %s :\1VERSION FTEQW-IRC-Plugin Release: %s", username, RELEASE));
		}
		else if ((!stricmp(var[4]+1, "\1TIME\1")) && (!strncmp(var[2], "PRIVMSG ", 7)))
		{
			char delimiters[] = "!";
			char *username = strtok(var[1], delimiters);
			time_t t;
			const struct tm *tm;
			char buffer[100];

			time(&t);
			tm=localtime(&t);

			strftime (buffer, 100, "%a %b %d %H:%M:%S", tm);

			IRC_AddClientMessage(irc, va("NOTICE %s :\1TIME %s\1", username, buffer));
		}
		else if (exc && col)
		{
			*col = '\0';
			col++;

			while(*to <= ' ' && *to)
				to++;
			for (end = to + strlen(to)-1; end >= to && *end <= ' '; end--)
				*end = '\0';
			if (!strcmp(to, irc_nick.string))
				to = prefix;	//This was directed straight at us.
								//So change the 'to', to the 'from'.

			for (end = to; *end; end++)
			{
				if (*end >= 'A' && *end <= 'Z')
					*end = *end + 'a' - 'A';
			}

			*exc = '\0';
			if (!strncmp(col, "\001", 1))
			{
				end = strchr(col+1, '\001');
				if (end)
					*end = '\0';
				if (!strncmp(col+1, "ACTION ", 7))
				{
					IRC_FilterMircColours(col+8);
					Con_SubPrintf(to, COLOURGREEN "***%s "COLORWHITE"%s\n", prefix, col+8);	//from client
				}
				else if (!strncmp(col+1, "PING ", 5))
				{
					time_t currentseconds;

					currentseconds = time (NULL);

					Con_SubPrintf(to, "Ping from %s\n", prefix);	//from client
					IRC_AddClientMessage(irc, va("NOTICE %s :\001PING %i\001\r\n", prefix, currentseconds));
				}
				else
				{
					if (end)//put it back on. might as well.
						*end = '\001';
					Con_SubPrintf(to, COLOURGREEN "%s: "COLORWHITE"%s\n", prefix, col);	//from client
				}
			}
			else
			{
				IRC_FilterMircColours(col);
				Con_SubPrintf(to, COLOURGREEN "%s: %s\n", prefix, col);	//from client
			}
		}
		else Con_SubPrintf(DEFAULTCONSOLE, COLOURGREEN "SERVER: <%s> %s\n", prefix, msg);	//direct server message
	}
	else if (!strncmp(var[2], "MODE ", 5))
	{
		char *username = strtok(var[1]+1, "! ");
		char *mode = strtok(var[4], " ");
		char *target = strtok(var[5], " ");
		char channel[100];

		if (!strncmp(var[3], "#", 1))
		{
			strcpy(channel,strtok(var[3], " "));
		}
		else
		{
			strcpy(channel,DEFAULTCONSOLE);
		}

		if ((!strncmp(mode+1,"o", 1)) || (!strncmp(mode+1,"v",1))) // ops or voice
		{
			Con_SubPrintf(channel,COLOURGREEN "%s sets mode %s on %s\n",username,mode,target);
		}
		else
		{
			Con_SubPrintf(channel,COLOURGREEN "%s sets mode %s\n",username,mode);
		}

	}
	else if (!strncmp(var[2], "KICK ", 5))
	{
		char *username = strtok(var[1]+1, "!");
		char *channel = strtok(var[3], " ");
		char *target = strtok(var[4], " ");
		char *reason = var[5]+1;

		Con_SubPrintf(channel,COLOURGREEN "%s was kicked from %s Reason: '%s' by %s\n",target,channel,reason,username);
	}
	else if (!strncmp(msg, "NICK ", 5))
	{
		char *exc = strchr(prefix, '!');
		char *col = strchr(msg+5, ':');
		if (exc && col)
		{
			*exc = '\0';
			//fixme: print this in all channels as appropriate.
			Con_SubPrintf(DEFAULTCONSOLE, COLOURGREEN "%s changes name to %s\n", prefix, col+1);
			if (BUILTINISVALID(Con_RenameSub))
				Con_RenameSub(prefix, col+1);	//if we were pming to them, rename accordingly.
		}
		else Con_SubPrintf(DEFAULTCONSOLE, COLOURGREEN ":%s%s\n", prefix, msg+6);
	}
	else if (!strncmp(msg, "PART ", 5))
	{
		char *exc = strchr(prefix, '!');
		char *col = strchr(msg+5, ':');
		if (!col)
			col = msg+5;
		else col+=1;
		if (exc)
		{
			if (!col)
				col = DEFAULTCONSOLE;
			*exc = '\0';
			Con_SubPrintf(col, "%s leaves channel %s\n", prefix, col);
		}
		else Con_SubPrintf(DEFAULTCONSOLE, COLOURGREEN ":%sPART %s\n", prefix, msg+5);
	}
	else if (!strncmp(msg, "JOIN ", 5))
	{
		char *exc = strchr(prefix, '!');
		char *col = strchr(msg+5, ':');
		if (exc && col)
		{
			*exc = '\0';
			Con_SubPrintf(col+1, COLOURGREEN "%s joins channel %s\n", prefix, col+1);
		}
		else Con_SubPrintf(DEFAULTCONSOLE, COLOURGREEN ":%sJOIN %s\n", prefix, msg+5);
	}
	else if (!strncmp(msg, "372 ", 4))
	{
		char *text = strstr(msg, ":-");
		if (text)
			Con_SubPrintf(DEFAULTCONSOLE, "%s\n", text+2);
		else
			Con_SubPrintf(DEFAULTCONSOLE, "%s\n", msg);
	}
	else if (!strncmp(msg, "331 ", 4) ||//no topic
			 !strncmp(msg, "332 ", 4))	//the topic
	{
		char *topic;
		char *chan;
		topic = COM_Parse(msg);
		topic = COM_Parse(topic);
		topic = COM_Parse(topic);
		while(*topic == ' ')
			topic++;
		if (*topic == ':')
		{
			topic++;
			chan = com_token;
		}
		else
		{
			topic = "No topic";
			chan = DEFAULTCONSOLE;
		}

		Con_SubPrintf(chan, "Topic on channel %s is: "COLOURGREEN"%s\n", chan, topic);
	}
	else if (!strncmp(msg, "353 ", 4))	//the names of people on a channel
	{
		char *eq = strstr(msg, "="); // BAD SPIKE!! = is normal channel :(
		char *eq2 = strstr(msg, "@"); // @ means the channel is +s (secret)
		char *eq3 = strstr(msg, "*"); // * means the channel is +p (private) rather redundant...
		char *channeltype = strtok(var[4], " ");
		char *channel = strtok(var[5], " ");
		char *str;


		int secret = 0;
		int privatechan = 0;
		if ( !strcmp(channeltype,"=") )
		{
			char *end;
			eq++;
			str = strstr(eq, ":");
			while(*eq == ' ')
				eq++;
			for (end = eq; *end>' '&&*end !=':'; end++)
				;
			*end = '\0';
			str++;
		}
		//else if (eq2)
		else if ( !strcmp(channeltype,"@") )
		{
			char *end;

			secret = 1;

			eq2++;
			str = strstr(eq2, ":");
			while(*eq2 == ' ')
				eq2++;
			for (end = eq2; *end>' '&&*end !=':'; end++)
				;
			*end = '\0';
			str++;
		}
		else if ( !strcmp(channeltype,"*") )
		{
			char *end;

			privatechan = 1;

			eq3++;
			str = strstr(eq3, ":");
			while(*eq3 == ' ')
				eq3++;
			for (end = eq3; *end>' '&&*end !=':'; end++)
				;
			*end = '\0';
			str++;
		}
		else
		{
			eq = "Corrupted_Message";
			str = NULL;
		}
		Con_SubPrintf(channel, va("Users on channel %s:\n", channel));
		while (str)
		{
			str = COM_Parse(str);
			if (*com_token == '@')	//they're an operator
				Con_SubPrintf(channel, COLOURGREEN"@"COLORWHITE"%s\n", com_token+1);
			else if (*com_token == '%')	//they've got half-op
				Con_SubPrintf(channel, COLOURGREEN"%"COLORWHITE"%s\n", com_token+1);
			else if (*com_token == '+')	//they've got voice
				Con_SubPrintf(channel, COLOURGREEN"+"COLORWHITE"%s\n", com_token+1);
			else
				Con_SubPrintf(channel, " %s\n", com_token);
		}
		if (secret == 1)
		{
			Con_SubPrintf(channel, "%s is secret (+s)\n",channel);
		}
		else if (privatechan == 1)
		{
			Con_SubPrintf(channel, "%s is private (+p)\n",channel);
		}

	}
	// would be great to convert the above to work better
	else if (atoi(raw) != 0)
	{
		char *rawparameter = strtok(var[4], " ");
		char *rawmessage = var[5];
		char *wholerawmessage = var[4];

		numbered_command(atoi(raw),msg,ircclient);

		IRC_CvarUpdate();

		if (irc_debug.value == 1) { Con_SubPrintf(DEFAULTCONSOLE, "%s\n", msg); }
	}
	else
		Con_SubPrintf(DEFAULTCONSOLE, "%s\n", msg);

	memmove(irc->bufferedinmessage, nextmsg, irc->bufferedinammount - (msg-irc->bufferedinmessage));
	irc->bufferedinammount-=nextmsg-irc->bufferedinmessage;
	return IRC_CONTINUE;
}

//functions above this line allow connections to multiple servers.
//it is just the control functions that only allow one server.

qintptr_t IRC_Frame(qintptr_t *args)
{
	int stat = IRC_CONTINUE;
	if (ircclient)
	{
		while(stat == IRC_CONTINUE)
			stat = IRC_ClientFrame(ircclient);
		if (stat == IRC_KILL)
		{
			Net_Close(ircclient->socket);
			IRC_Free(ircclient);
			ircclient = NULL;
			Con_SubPrintf(DEFAULTCONSOLE, "Disconnected from irc\n");
		}
	}
	return 0;
}

void IRC_Command(char *dest)
{
	char imsg[8192];
	char *msg;

	Cmd_Args(imsg, sizeof(imsg));

	msg = COM_Parse(imsg);

	if (*com_token == '/')
	{
		if (!strcmp(com_token+1, "open") || !strcmp(com_token+1, "connect"))
		{
			if (ircclient)
			{
				Con_Printf("You are already connected\nPlease /quit first\n");
				return;
			}
			msg = COM_Parse(msg);
			ircclient = IRC_Connect(com_token, 6667);
			if (ircclient)
			{
				Con_Printf("Trying to connect\n");
				IRC_SetPass(ircclient, "");

				msg = COM_Parse(msg);
				strlcpy(ircclient->autochannels, com_token, sizeof(ircclient->autochannels));

				msg = COM_Parse(msg);
				if (*com_token)
					IRC_SetNick(ircclient, com_token);
				else
					IRC_SetNick(ircclient, ircclient->nick);

				IRC_SetUser(ircclient, defaultuser);

			}
		}
		else if (!strcmp(com_token+1, "nick"))
		{
			msg = COM_Parse(msg);
			if (!ircclient)	//not yet connected.
				Cvar_SetString(irc_nick.name, com_token);
			else
				IRC_SetNick(ircclient, com_token);
		}
		else if (!strcmp(com_token+1, "user"))
		{
			msg = COM_Parse(msg);
			strlcpy(defaultuser, com_token, sizeof(defaultuser));
			if (ircclient)
				IRC_SetUser(ircclient, defaultuser);
		}
		else if (!ircclient)
		{
			Con_Printf("Not connected, please connect to an irc server first.\n");
		}

		//ALL other commands require you to be connected.
		else if (!strcmp(com_token+1, "list"))
		{
			IRC_AddClientMessage(ircclient, "LIST");
		}
		else if ( !strcmp(com_token+1, "join") || !strcmp(com_token+1, "j") )
		{
			char *channelkey;

			msg = COM_Parse(msg);

			channelkey = strtok(imsg," ");
			channelkey = strtok(NULL," ");
			channelkey = strtok(NULL," ");

			IRC_JoinChannel(ircclient,com_token,channelkey);

		}
		else if (!strcmp(com_token+1, "part") || !strcmp(com_token+1, "leave")) // need to implement leave reason
		{
			msg = COM_Parse(msg);
			if (!*com_token)
				IRC_AddClientMessage(ircclient, va("PART %s", dest));
			else
				IRC_AddClientMessage(ircclient, va("PART %s", com_token));
		}
		else if (!strcmp(com_token+1, "msg"))
		{
			msg = COM_Parse(msg);
			if (!msg)
				return;
			IRC_AddClientMessage(ircclient, va("PRIVMSG %s :%s", com_token, msg+1));
			Con_SubPrintf(com_token, "%s: %s\n", ircclient->nick, msg);
		}
		else if (!strcmp(com_token+1, "quote") || !strcmp(com_token+1, "raw"))
		{
			IRC_AddClientMessage(ircclient, va("%s", msg));
		}
		else if (!strcmp(com_token+1, "quit") || !strcmp(com_token+1, "disconnect"))
		{
			msg = COM_Parse(msg);
			if (*com_token)
				IRC_AddClientMessage(ircclient, va("QUIT :%s", com_token));
			else
				IRC_AddClientMessage(ircclient, va("QUIT :FTE QuakeWorld IRC-Plugin Release: %s http://www.fteqw.com/plugins/", RELEASE));
		}
		else if (!strcmp(com_token+1, "whois"))
		{
			msg = COM_Parse(msg);
			IRC_AddClientMessage(ircclient, va("WHOIS :%s",com_token));
		}
		else if (!strcmp(com_token+1, "away"))
		{
			if ( strlen(msg) > 1 )
				IRC_AddClientMessage(ircclient, va("AWAY :%s",msg+1));
			else
				IRC_AddClientMessage(ircclient, va("AWAY :"));
		}
		else if (!strcmp(com_token+1, "motd"))
		{
			IRC_AddClientMessage(ircclient, "MOTD");
		}
		else if (!strcmp(com_token+1, "ctcp"))
		{
			msg = COM_Parse(msg);
			IRC_AddClientMessage(ircclient, va("PRIVMSG %s :\1%s\1",com_token,msg+1));
		}
		else if (!strcmp(com_token+1, "dest"))
		{
			msg = COM_Parse(msg);
			strlcpy(ircclient->defaultdest, com_token, sizeof(ircclient->defaultdest));
		}
		else if (!strcmp(com_token+1, "ping"))
		{
			if (!*dest)
				Con_SubPrintf(DEFAULTCONSOLE, "No channel joined. Try /join #<channel>\n");
			else
				IRC_AddClientMessage(ircclient, va("PRIVMSG %s :\001PING%s\001", dest, msg));
		}
		else if (!strcmp(com_token+1, "notice"))
		{
			msg = COM_Parse(msg);
			IRC_AddClientMessage(ircclient, va("NOTICE %s :%s",com_token, msg+1));
		}
		else if (!strcmp(com_token+1, "me"))
		{
			if (!*dest)
				Con_SubPrintf(DEFAULTCONSOLE, "No channel joined. Try /join #<channel>\n");
			else
			{
				if(*msg <= ' ' && *msg)
					msg++;
				IRC_AddClientMessage(ircclient, va("PRIVMSG %s :\001ACTION %s\001", dest, msg));
				Con_SubPrintf(ircclient->defaultdest, "***%s %s\n", ircclient->nick, msg);
			}
		}
	}
	else
	{
		if (ircclient)
		{
			if (!*dest)
			{
				Con_SubPrintf(DEFAULTCONSOLE, "No channel joined. Try /join #<channel>\n");
			}
			else
			{
				msg = imsg;
				IRC_AddClientMessage(ircclient, va("PRIVMSG %s :%s", dest, msg));
				Con_SubPrintf(dest, "%s: %s\n", ircclient->nick, msg);
			}
		}
		else
			Con_Printf("Not connected\ntype \"%s /open IRCSERVER [#channel1[,#channel2[,...]]] [nick]\" to connect\n", commandname);
	}
}
