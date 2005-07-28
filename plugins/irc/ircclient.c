//Released under the terms of the gpl as this file uses a bit of quake derived code. All sections of the like are marked as such

#include "../plugin.h"
#include <time.h>

#define Q_strncpyz(o, i, l) do {strncpy(o, i, l-1);o[l-1]='\0';}while(0)


void Con_SubPrintf(char *subname, char *format, ...)
{
	va_list		argptr;
	static char		string[1024];

	va_start (argptr, format);
	vsnprintf (string, sizeof(string), format,argptr);
	va_end (argptr);

	Con_SubPrint(subname, string);
}


//porting zone:


#define Q_strncpyz(o, i, l) do {strncpy(o, i, l-1);o[l-1]='\0';}while(0)




	#define COLOURGREEN	"^2"
	#define COLORWHITE "^7"
	#define COLOURWHITE "^7" // word
	#define COLOURRED "^1"
	#define COLOURYELLOW "^3"
	#define COLOURPURPLE "^5"
	#define COMMANDPREFIX "irc "
	#define playsound(s)


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






void IRC_Command(void);

int IRC_ExecuteCommand(int *args)
{
	char cmd[8];
	Cmd_Argv(0, cmd, sizeof(cmd));
	if (!strcmp(cmd, "irc"))
	{
		IRC_Command();
		return true;
	}
	return false;
}

int IRC_ConExecuteCommand(int *args);

int IRC_Frame(int *args);

int Plug_Init(int *args)
{
	if (	Plug_Export("Tick", IRC_Frame) &&
		Plug_Export("ExecuteCommand", IRC_ExecuteCommand) &&
		Plug_Export("ConExecuteCommand", IRC_ConExecuteCommand))
		Con_Print("IRC Client Plugin Loaded\n");
	else
		Con_Print("IRC Client Plugin failed\n");
	return 0;
}










//\r\n is used to end a line.
//meaning \0s are valid.
//but never used cos it breaks strings



#define IRC_MAXNICKLEN 32	//9 and a null term
#define IRC_MAXMSGLEN 512


char defaultnick[IRC_MAXNICKLEN+1];
char defaultuser[IRC_MAXNICKLEN+1] = "Unknown";


typedef struct {
	char server[64];
	int port;

	int socket;

	char nick[IRC_MAXNICKLEN];
	char pwd[64];
	char realname[128];
	char hostname[128];
	int nickcycle;

	char defaultdest[IRC_MAXNICKLEN];//channel or nick

	char bufferedinmessage[IRC_MAXMSGLEN+1];	//there is a max size for protocol. (conveinient eh?) (and it's text format)
	int bufferedinammount;
} ircclient_t;
ircclient_t *ircclient;

int IRC_ConExecuteCommand(int *args)
{
	if (!ircclient)
	{
		char buffer[256];
		Cmd_Argv(0, buffer, sizeof(buffer));
		Con_SubPrint(buffer, "You were disconnected\n");
		return true;
	}
	Cmd_Argv(0, ircclient->defaultdest, sizeof(ircclient->defaultdest));
	IRC_Command();
	return true;
}

void IRC_AddClientMessage(ircclient_t *irc, char *msg)
{
	char output[4096];

	strcpy(output, msg);
	strcat(output, "\n");

	Net_Send(irc->socket, output, strlen(output));	//FIXME: This needs rewriting to cope with errors.
	Con_SubPrintf("irc",COLOURYELLOW "<< %s \n",msg);
}

ircclient_t *IRC_Connect(char *server, int defport)
{
	ircclient_t *irc;
	unsigned long _true = true;


	irc = IRC_Malloc(sizeof(ircclient_t));
	if (!irc)
		return NULL;

	memset(irc, 0, sizeof(ircclient_t));


	irc->socket = Net_TCPConnect(server, defport);	//port is only used if the url doesn't contain one. It's a default.

	//not yet blocking. So no frequent attempts please...
	//non blocking prevents connect from returning worthwhile sensible value.
	if (irc->socket < 0)
	{
		Con_Printf("IRC_OpenSocket: couldn't connect\n");
		IRC_Free(irc);
		return NULL;
	}

	Q_strncpyz(irc->server, server, sizeof(irc->server));
	strcpy(irc->nick, "anonymous");
	strcpy(irc->realname, "anonymous");

	strcpy(irc->hostname, "anonymous");

//	gethostname(irc->hostname, sizeof(irc->hostname));
//	irc->hostname[sizeof(irc->hostname)-1] = 0;


	return irc;
}
void IRC_SetPass(ircclient_t *irc, char *pass)
{
	if (pass != "") {	IRC_AddClientMessage(irc, va("PASS %s", pass)); }
}
void IRC_SetNick(ircclient_t *irc, char *nick)
{
	//strncpy(irc->nick, nick, sizeof(irc->nick)); // broken
	//irc->nick[sizeof(irc->nick)-1] = '\0'; // broken, lets just allow "anonymous" on.
	IRC_AddClientMessage(irc, va("NICK %s", irc->nick));
	irc->nickcycle=0;
}
void IRC_SetUser(ircclient_t *irc, char *user)
{
	IRC_AddClientMessage(irc, va("USER %s %s %s :%s", user, irc->hostname, irc->server, "Something"));
}

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
int IRC_ClientFrame(ircclient_t *irc)
{
	char prefix[64];
	int ret;
	char *nextmsg, *msg;

	char var[9][1000];
	char *temp;
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

	if (*msg == ':')	//we need to strip off the prefix
	{
		char *sp = strchr(msg, ' ');
		if (!sp)
		{
			Con_SubPrintf("irc", "Ignoring bad message\n%s\n", msg);
			memmove(irc->bufferedinmessage, nextmsg, irc->bufferedinammount - (msg-irc->bufferedinmessage));
			irc->bufferedinammount-=nextmsg-irc->bufferedinmessage;
			return IRC_CONTINUE;
		}

		if (sp-msg >= sizeof(prefix))
			Q_strncpyz(prefix, msg+1, sizeof(prefix));
		else
			Q_strncpyz(prefix, msg+1, sp-msg);

		msg = sp;
		while(*msg == ' ')
			msg++;
	}
	else
		strcpy(prefix, irc->server);

	if (!strncmp(var[1], "PING ", 5))
	{
		IRC_AddClientMessage(irc, va("PONG %s", var[2]));
	}
	else if (!strncmp(var[2], "PRIVMSG ", 7) || !strncmp(var[2], "NOTICE ", 6))	//no autoresponses to notice please, and any autoresponses should be in the form of a notice
	{
		char *exc = strchr(prefix, '!');
		char *col = strchr(msg+6, ':');
		char *end;
		char *to = msg + 7;

		//message takes the form :FROM PRIVMSG TO :MESSAGE

		playsound ("misc/talk.wav");

		if (!stricmp(var[4]+1, "\1VERSION\1"))
		{
			char *username;
			char delimiters[] = "!";

			username = strtok(var[1]+1, delimiters);

			IRC_AddClientMessage(irc, va("NOTICE %s :\1VERSION FTEQW-IRC-Plugin Build ?\1", username));
		}

		if (exc && col)
		{
			*col = '\0';
			col++;

			while(*to <= ' ' && *to)
				to++;
			for (end = to + strlen(to)-1; end >= to && *end <= ' '; end--)
				*end = '\0';
			if (!strcmp(to, irc->nick))
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
					Con_SubPrintf(to, "Ping from %s\n", prefix);	//from client
//					IRC_AddClientMessage(irc, va("PRIVMSG %s :\001PING %s\001\r\n", prefix, col+6));
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
		else Con_SubPrintf("irc", COLOURGREEN ":%s%s\n", prefix, msg);	//direct server message
	}
	else if (!strncmp(msg, "NICK ", 5))
	{
		char *exc = strchr(prefix, '!');
		char *col = strchr(msg+5, ':');
		if (exc && col)
		{
			*exc = '\0';
			//fixme: print this in all channels as appropriate.
			Con_SubPrintf("irc", COLOURGREEN "%s changes name to %s\n", prefix, col+1);
			Con_RenameSub(prefix, col+1);	//if we were pming to them, rename accordingly.
		}
		else Con_SubPrintf("irc", COLOURGREEN ":%s%s\n", prefix, msg+6);
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
				col = "irc";
			*exc = '\0';
			Con_SubPrintf(col, "%s leaves channel %s\n", prefix, col);
		}
		else Con_SubPrintf("irc", COLOURGREEN ":%sPART %s\n", prefix, msg+5);
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
		else Con_SubPrintf("irc", COLOURGREEN ":%sJOIN %s\n", prefix, msg+5);
	}
	else if ((!strncmp(msg, "001 ", 4)) || (!strncmp(msg, "002 ", 4)) || (!strncmp(msg, "003 ", 4)) || (!strncmp(msg, "004 ", 4)) || (!strncmp(msg, "005 ", 4))) // useless info on connect
	{
	}
	else if ((!strncmp(msg, "251 ", 4)) || (!strncmp(msg, "252 ", 4)) || (!strncmp(msg, "254 ", 4)) || (!strncmp(msg, "255 ", 4)) || (!strncmp(msg, "265 ", 4)) || (!strncmp(msg, "266 ", 4)) || (!strncmp(msg, "422 ", 4))) //useless info about local users, server users, connected servers, motd missing
	{
	}
	else if (!strncmp(msg, "311 ", 4)) // Whois
	{
		char *username = strtok(var[4], " ");
		char *ident = strtok(var[5], " ");
		char *address = strtok(var[6], " ");
		char *realname = var[8]+1;

		Con_SubPrintf("irc","WHOIS: <%s> (Ident: %s) (Address: %s) (Realname: %s) \n", username, ident, address, realname);
	}
	else if (!strncmp(msg, "312 ", 4))
	{
		char *username = strtok(var[4], " ");
		char *serverhostname = strtok(var[5], " ");
		char *servername = var[6]+1;

		Con_SubPrintf("irc","WHOIS: <%s> (Server: %s) (Server Name: %s) \n", username, serverhostname, servername);
	}
	else if (!strncmp(msg, "317 ", 4)) // seconds idle etc
	{
		char *username = strtok(var[4], " ");
		char *secondsidle = strtok(var[5], " ");
		char *signontime = strtok(var[6], " ");
		time_t t;
		const struct tm *tm;

		t=strtoul(signontime, 0, 0);
		tm=localtime(&t);

		//if (tm[strlen-2] = '\n') { tm[strlen-2] = '\0'; } // strip new line

		Con_SubPrintf("irc","WHOIS: <%s> (Idle Time: %s seconds) (Signon Time: %s) \n", username, secondsidle, asctime(tm));
	}
	else if (!strncmp(msg, "318 ", 4)) //end of whois
	{
	}
	else if (!strncmp(msg, "322 ", 4))	//channel listing
	{
		Con_SubPrintf("irc", "%s\n", msg);
	}
	else if ((!strncmp(msg, "372 ", 4)) || (!strncmp(msg, "375 ", 4)) || (!strncmp(msg, "376 ", 4)))
	{
	}
	else if (!strncmp(msg, "375 ", 4))
	{
		Con_SubPrintf("irc", "%s\n", msg);
	}
	else if (!strncmp(msg, "378 ", 4)) //kinda useless whois info
	{
	}
	else if (!strncmp(msg, "431 ", 4) ||	//nick not set
			 !strncmp(msg, "433 ", 4) ||	//nick already in use
			 !strncmp(msg, "436 ", 4))		//nick collision
	{
		if (irc->nickcycle >= 99)	//this is just silly.
			return IRC_KILL;

		if (!irc->nickcycle)	//sequentially try the next one up
			IRC_SetNick(irc, defaultnick);
		else
			IRC_SetNick(irc, va("%s%i", defaultnick, irc->nickcycle));

		irc->nickcycle++;
	}
	else if (!strncmp(msg, "432 ", 4))
	{
		Con_SubPrintf("irc", "Erroneous/invalid nickname given\n");
		return IRC_KILL;
	}
	else if (!strncmp(msg, "372 ", 4))
	{
		char *text = strstr(msg, ":-");
		if (text)
			Con_SubPrintf("irc", "%s\n", text+2);
		else
			Con_SubPrintf("irc", "%s\n", msg);
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
			chan = "irc";
		}

		Con_SubPrintf(chan, "Topic on channel %s is: "COLOURGREEN"%s\n", chan, topic);
	}
	else if (!strncmp(msg, "353 ", 4))	//the names of people on a channel
	{
		char *eq = strstr(msg, "=");
		char *str;
		if (eq)
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
		else
		{
			eq = "Corrupted_Message";
			str = NULL;
		}
		Con_SubPrint(eq, va("Users on channel %s:\n", eq));
		while (str)
		{
			str = COM_Parse(str);
			if (*com_token == '@')	//they're an operator
				Con_SubPrint(eq, va(COLOURGREEN"@"COLORWHITE"%s\n", com_token+1));
			else if (*com_token == '+')	//they've got voice
				Con_SubPrint(eq, va(COLOURGREEN"+"COLORWHITE"%s\n", com_token+1));
			else
				Con_SubPrint(eq, va(" %s\n", com_token));
		}
	}
	/*
	else if (!strncmp(msg, "401 ", 4))
	{
		dprintf("Nickname/channel does not exist\n", msg);
	}

  	else if (!strncmp(msg, "402 ", 4))
	{
		dprintf("No such server\n", msg);
	}
	else if (!strncmp(msg, "403 ", 4))
	{
		dprintf("No such channel\n", msg);
	}
	else if (!strncmp(msg, "404 ", 4))
	{
		dprintf("Cannot send to that channel\n", msg);
	}
	else if (!strncmp(msg, "405 ", 4))
	{
		dprintf("You may not join annother channel\n", msg);
	}
	else if (!strncmp(msg, "406 ", 4))
	{
		dprintf("Nickname does not exist\n", msg);
	}
	else if (!strncmp(msg, "407 ", 4))
	{
		dprintf("Too many targets. Try to specify a specific nickname.\n", msg);
	}

	else if (!strncmp(msg, "409 ", 4))
	{
		dprintf("No origin specified\n", msg);
	}
	else if (!strncmp(msg, "411 ", 4))
	{
		dprintf("No recipient given.\n", msg);
	}
	else if (!strncmp(msg, "412 ", 4))
	{
		dprintf("No text given\n", msg);
	}
	else if (!strncmp(msg, "413 ", 4))
	{
		dprintf("No top level domain specified\n", msg);
	}
	else if (!strncmp(msg, "414 ", 4))
	{
		dprintf("Wildcard in toplevel domain\n", msg);
	}
	else if (!strncmp(msg, "421 ", 4))
	{
		dprintf("Unknown command.\n", msg);
	}
	else if (!strncmp(msg, "422 ", 4))
	{
		dprintf("MOTD file is missing (awww)\n", msg);
	}
	else if (!strncmp(msg, "423 ", 4))
	{
		dprintf("No administrative info is available\n", msg);
	}
	else if (!strncmp(msg, "424 ", 4))
	{
		dprintf("Generic file error\n", msg);
	}
  	else if (!strncmp(msg, "441 ", 4))
	{
		dprintf("User isn't in that channel\n", msg);
	}
	else if (!strncmp(msg, "442 ", 4))
	{
		dprintf("You are not on that channel\n", msg);
	}
	else if (!strncmp(msg, "443 ", 4))
	{
		dprintf("The user is already on that channel\n", msg);
	}
	else if (!strncmp(msg, "444 ", 4))
	{
		dprintf("User not logged in\n", msg);
	}
	else if (!strncmp(msg, "445 ", 4))
	{
		dprintf("SUMMON has been disabled\n", msg);
	}
	else if (!strncmp(msg, "446 ", 4))
	{
		dprintf("USERS has been disabled\n", msg);
	}
	else if (!strncmp(msg, "451 ", 4))
	{
		dprintf("You have not registered\n", msg);
	}


        461     ERR_NEEDMOREPARAMS
                        "<command> :Not enough parameters"

                - Returned by the server by numerous commands to
                  indicate to the client that it didn't supply enough
                  parameters.

        462     ERR_ALREADYREGISTRED
                        ":You may not reregister"

                - Returned by the server to any link which tries to
                  change part of the registered details (such as
                  password or user details from second USER message).

        463     ERR_NOPERMFORHOST
                        ":Your host isn't among the privileged"

                - Returned to a client which attempts to register with
                  a server which does not been setup to allow
                  connections from the host the attempted connection
                  is tried.



--------------------------------------------------------------------------------
Page 47

        464     ERR_PASSWDMISMATCH
                        ":Password incorrect"

                - Returned to indicate a failed attempt at registering
                  a connection for which a password was required and
                  was either not given or incorrect.

        465     ERR_YOUREBANNEDCREEP
                        ":You are banned from this server"

                - Returned after an attempt to connect and register
                  yourself with a server which has been setup to
                  explicitly deny connections to you.

        467     ERR_KEYSET
                        "<channel> :Channel key already set"
        471     ERR_CHANNELISFULL
                        "<channel> :Cannot join channel (+l)"
        472     ERR_UNKNOWNMODE
                        "<char> :is unknown mode char to me"
        473     ERR_INVITEONLYCHAN
                        "<channel> :Cannot join channel (+i)"
        474     ERR_BANNEDFROMCHAN
                        "<channel> :Cannot join channel (+b)"
        475     ERR_BADCHANNELKEY
                        "<channel> :Cannot join channel (+k)"
        481     ERR_NOPRIVILEGES
                        ":Permission Denied- You're not an IRC operator"

                - Any command requiring operator privileges to operate
                  must return this error to indicate the attempt was
                  unsuccessful.

        482     ERR_CHANOPRIVSNEEDED
                        "<channel> :You're not channel operator"

                - Any command requiring 'chanop' privileges (such as
                  MODE messages) must return this error if the client
                  making the attempt is not a chanop on the specified
                  channel.

        483     ERR_CANTKILLSERVER
                        ":You cant kill a server!"

                - Any attempts to use the KILL command on a server
                  are to be refused and this error returned directly
                  to the client.



--------------------------------------------------------------------------------
Page 48

        491     ERR_NOOPERHOST
                        ":No O-lines for your host"

                - If a client sends an OPER message and the server has
                  not been configured to allow connections from the
                  client's host as an operator, this error must be
                  returned.

        501     ERR_UMODEUNKNOWNFLAG
                        ":Unknown MODE flag"

                - Returned by the server to indicate that a MODE
                  message was sent with a nickname parameter and that
                  the a mode flag sent was not recognized.

        502     ERR_USERSDONTMATCH
                        ":Cant change mode for other users"

                - Error sent to any user trying to view or change the
                  user mode for a user other than themselves.


6.2 Command responses.

        300     RPL_NONE
                        Dummy reply number. Not used.

        302     RPL_USERHOST
                        ":[<reply>{<space><reply>}]"

                - Reply format used by USERHOST to list replies to
                  the query list.  The reply string is composed as
                  follows:

                  <reply> ::= <nick>['*'] '=' <'+'|'-'><hostname>

The '*' indicates whether the client has registered as an Operator. The '-' or '+' characters represent whether the client has set an AWAY message or not respectively.


        303     RPL_ISON
                        ":[<nick> {<space><nick>}]"

                - Reply format used by ISON to list replies to the
                  query list.

        301     RPL_AWAY
                        "<nick> :<away message>"



--------------------------------------------------------------------------------
Page 49

        305     RPL_UNAWAY
                        ":You are no longer marked as being away"
        306     RPL_NOWAWAY
                        ":You have been marked as being away"

                - These replies are used with the AWAY command (if
                  allowed).  RPL_AWAY is sent to any client sending a
                  PRIVMSG to a client which is away.  RPL_AWAY is only
                  sent by the server to which the client is connected.
                  Replies RPL_UNAWAY and RPL_NOWAWAY are sent when the
                  client removes and sets an AWAY message.

        311     RPL_WHOISUSER
                        "<nick> <user> <host> * :<real name>"
        312     RPL_WHOISSERVER
                        "<nick> <server> :<server info>"
        313     RPL_WHOISOPERATOR
                        "<nick> :is an IRC operator"
        317     RPL_WHOISIDLE
                        "<nick> <integer> :seconds idle"
        318     RPL_ENDOFWHOIS
                        "<nick> :End of /WHOIS list"
        319     RPL_WHOISCHANNELS
                        "<nick> :{[@|+]<channel><space>}"

                - Replies 311 - 313, 317 - 319 are all replies
                  generated in response to a WHOIS message.  Given that
                  there are enough parameters present, the answering
                  server must either formulate a reply out of the above
                  numerics (if the query nick is found) or return an
                  error reply.  The '*' in RPL_WHOISUSER is there as
                  the literal character and not as a wild card.  For
                  each reply set, only RPL_WHOISCHANNELS may appear
                  more than once (for long lists of channel names).
                  The '@' and '+' characters next to the channel name
                  indicate whether a client is a channel operator or
                  has been granted permission to speak on a moderated
                  channel.  The RPL_ENDOFWHOIS reply is used to mark
                  the end of processing a WHOIS message.

        314     RPL_WHOWASUSER
                        "<nick> <user> <host> * :<real name>"
        369     RPL_ENDOFWHOWAS
                        "<nick> :End of WHOWAS"

                - When replying to a WHOWAS message, a server must use
                  the replies RPL_WHOWASUSER, RPL_WHOISSERVER or
                  ERR_WASNOSUCHNICK for each nickname in the presented



--------------------------------------------------------------------------------
Page 50
list. At the end of all reply batches, there must be RPL_ENDOFWHOWAS (even if there was only one reply and it was an error).


        321     RPL_LISTSTART
                        "Channel :Users  Name"
        322     RPL_LIST
                        "<channel> <# visible> :<topic>"
        323     RPL_LISTEND
                        ":End of /LIST"

                - Replies RPL_LISTSTART, RPL_LIST, RPL_LISTEND mark
                  the start, actual replies with data and end of the
                  server's response to a LIST command.  If there are
                  no channels available to return, only the start
                  and end reply must be sent.

        324     RPL_CHANNELMODEIS
                        "<channel> <mode> <mode params>"

        331     RPL_NOTOPIC
                        "<channel> :No topic is set"
        332     RPL_TOPIC
                        "<channel> :<topic>"

                - When sending a TOPIC message to determine the
                  channel topic, one of two replies is sent.  If
                  the topic is set, RPL_TOPIC is sent back else
                  RPL_NOTOPIC.

        341     RPL_INVITING
                        "<channel> <nick>"

                - Returned by the server to indicate that the
                  attempted INVITE message was successful and is
                  being passed onto the end client.

        342     RPL_SUMMONING
                        "<user> :Summoning user to IRC"

                - Returned by a server answering a SUMMON message to
                  indicate that it is summoning that user.

        351     RPL_VERSION
                        "<version>.<debuglevel> <server> :<comments>"

                - Reply by the server showing its version details.
                  The <version> is the version of the software being



--------------------------------------------------------------------------------
Page 51
used (including any patchlevel revisions) and the

                  <debuglevel> is used to indicate if the server is
                  running in "debug mode".

The "comments" field may contain any comments about the version or further version details.


        352     RPL_WHOREPLY
                        "<channel> <user> <host> <server> <nick> \
                         <H|G>[*][@|+] :<hopcount> <real name>"
        315     RPL_ENDOFWHO
                        "<name> :End of /WHO list"

                - The RPL_WHOREPLY and RPL_ENDOFWHO pair are used
                  to answer a WHO message.  The RPL_WHOREPLY is only
                  sent if there is an appropriate match to the WHO
                  query.  If there is a list of parameters supplied
                  with a WHO message, a RPL_ENDOFWHO must be sent
                  after processing each list item with <name> being
                  the item.

        353     RPL_NAMREPLY
                        "<channel> :[[@|+]<nick> [[@|+]<nick> [...]]]"
        366     RPL_ENDOFNAMES
                        "<channel> :End of /NAMES list"

                - To reply to a NAMES message, a reply pair consisting
                  of RPL_NAMREPLY and RPL_ENDOFNAMES is sent by the
                  server back to the client.  If there is no channel
                  found as in the query, then only RPL_ENDOFNAMES is
                  returned.  The exception to this is when a NAMES
                  message is sent with no parameters and all visible
                  channels and contents are sent back in a series of
                  RPL_NAMEREPLY messages with a RPL_ENDOFNAMES to mark
                  the end.

        364     RPL_LINKS
                        "<mask> <server> :<hopcount> <server info>"
        365     RPL_ENDOFLINKS
                        "<mask> :End of /LINKS list"

                - In replying to the LINKS message, a server must send
                  replies back using the RPL_LINKS numeric and mark the
                  end of the list using an RPL_ENDOFLINKS reply.

        367     RPL_BANLIST
                        "<channel> <banid>"
        368     RPL_ENDOFBANLIST



--------------------------------------------------------------------------------
Page 52
"<channel> :End of channel ban list"


                - When listing the active 'bans' for a given channel,
                  a server is required to send the list back using the
                  RPL_BANLIST and RPL_ENDOFBANLIST messages.  A separate
                  RPL_BANLIST is sent for each active banid.  After the
                  banids have been listed (or if none present) a
                  RPL_ENDOFBANLIST must be sent.

        371     RPL_INFO
                        ":<string>"
        374     RPL_ENDOFINFO
                        ":End of /INFO list"

                - A server responding to an INFO message is required to
                  send all its 'info' in a series of RPL_INFO messages
                  with a RPL_ENDOFINFO reply to indicate the end of the
                  replies.

        375     RPL_MOTDSTART
                        ":- <server> Message of the day - "
        376     RPL_ENDOFMOTD
                        ":End of /MOTD command"

                - When responding to the MOTD message and the MOTD file
                  is found, the file is displayed line by line, with
                  each line no longer than 80 characters, using
                  RPL_MOTD format replies.  These should be surrounded
                  by a RPL_MOTDSTART (before the RPL_MOTDs) and an
                  RPL_ENDOFMOTD (after).

        381     RPL_YOUREOPER
                        ":You are now an IRC operator"

                - RPL_YOUREOPER is sent back to a client which has
                  just successfully issued an OPER message and gained
                  operator status.

        382     RPL_REHASHING
                        "<config file> :Rehashing"

                - If the REHASH option is used and an operator sends
                  a REHASH message, an RPL_REHASHING is sent back to
                  the operator.

        391     RPL_TIME



--------------------------------------------------------------------------------
Page 53
"<server> :<string showing server's local time>"


                - When replying to the TIME message, a server must send
                  the reply using the RPL_TIME format above.  The string
                  showing the time need only contain the correct day and
                  time there.  There is no further requirement for the
                  time string.

        392     RPL_USERSSTART
                        ":UserID   Terminal  Host"
        393     RPL_USERS
                        ":%-8s %-9s %-8s"
        394     RPL_ENDOFUSERS
                        ":End of users"
        395     RPL_NOUSERS
                        ":Nobody logged in"

                - If the USERS message is handled by a server, the
                  replies RPL_USERSTART, RPL_USERS, RPL_ENDOFUSERS and
                  RPL_NOUSERS are used.  RPL_USERSSTART must be sent
                  first, following by either a sequence of RPL_USERS
                  or a single RPL_NOUSER.  Following this is
                  RPL_ENDOFUSERS.

        200     RPL_TRACELINK
                        "Link <version & debug level> <destination> \
                         <next server>"
        201     RPL_TRACECONNECTING
                        "Try. <class> <server>"
        202     RPL_TRACEHANDSHAKE
                        "H.S. <class> <server>"
        203     RPL_TRACEUNKNOWN
                        "???? <class> [<client IP address in dot form>]"
        204     RPL_TRACEOPERATOR
                        "Oper <class> <nick>"
        205     RPL_TRACEUSER
                        "User <class> <nick>"
        206     RPL_TRACESERVER
                        "Serv <class> <int>S <int>C <server> \
                         <nick!user|*!*>@<host|server>"
        208     RPL_TRACENEWTYPE
                        "<newtype> 0 <client name>"
        261     RPL_TRACELOG
                        "File <logfile> <debug level>"

                - The RPL_TRACE* are all returned by the server in
                  response to the TRACE message.  How many are
                  returned is dependent on the the TRACE message and



--------------------------------------------------------------------------------
Page 54
whether it was sent by an operator or not. There is no predefined order for which occurs first. Replies RPL_TRACEUNKNOWN, RPL_TRACECONNECTING and RPL_TRACEHANDSHAKE are all used for connections which have not been fully established and are either unknown, still attempting to connect or in the process of completing the 'server handshake'. RPL_TRACELINK is sent by any server which handles a TRACE message and has to pass it on to another server. The list of RPL_TRACELINKs sent in response to a TRACE command traversing the IRC network should reflect the actual connectivity of the servers themselves along that path.
RPL_TRACENEWTYPE is to be used for any connection which does not fit in the other categories but is being displayed anyway.


        211     RPL_STATSLINKINFO
                        "<linkname> <sendq> <sent messages> \
                         <sent bytes> <received messages> \
                         <received bytes> <time open>"
        212     RPL_STATSCOMMANDS
                        "<command> <count>"
        213     RPL_STATSCLINE
                        "C <host> * <name> <port> <class>"
        214     RPL_STATSNLINE
                        "N <host> * <name> <port> <class>"
        215     RPL_STATSILINE
                        "I <host> * <host> <port> <class>"
        216     RPL_STATSKLINE
                        "K <host> * <username> <port> <class>"
        218     RPL_STATSYLINE
                        "Y <class> <ping frequency> <connect \
                         frequency> <max sendq>"
        219     RPL_ENDOFSTATS
                        "<stats letter> :End of /STATS report"
        241     RPL_STATSLLINE
                        "L <hostmask> * <servername> <maxdepth>"
        242     RPL_STATSUPTIME
                        ":Server Up %d days %d:%02d:%02d"
        243     RPL_STATSOLINE
                        "O <hostmask> * <name>"
        244     RPL_STATSHLINE
                        "H <hostmask> * <servername>"

        221     RPL_UMODEIS
                        "<user mode string>"



--------------------------------------------------------------------------------
Page 55

                        - To answer a query about a client's own mode,
                          RPL_UMODEIS is sent back.

        251     RPL_LUSERCLIENT
                        ":There are <integer> users and <integer> \
                         invisible on <integer> servers"
        252     RPL_LUSEROP
                        "<integer> :operator(s) online"
        253     RPL_LUSERUNKNOWN
                        "<integer> :unknown connection(s)"
        254     RPL_LUSERCHANNELS
                        "<integer> :channels formed"
        255     RPL_LUSERME
                        ":I have <integer> clients and <integer> \
                          servers"

                        - In processing an LUSERS message, the server
                          sends a set of replies from RPL_LUSERCLIENT,
                          RPL_LUSEROP, RPL_USERUNKNOWN,
                          RPL_LUSERCHANNELS and RPL_LUSERME.  When
                          replying, a server must send back
                          RPL_LUSERCLIENT and RPL_LUSERME.  The other
                          replies are only sent back if a non-zero count
                          is found for them.

        256     RPL_ADMINME
                        "<server> :Administrative info"
        257     RPL_ADMINLOC1
                        ":<admin info>"
        258     RPL_ADMINLOC2
                        ":<admin info>"
        259     RPL_ADMINEMAIL
                        ":<admin info>"

                        - When replying to an ADMIN message, a server
                          is expected to use replies RLP_ADMINME
                          through to RPL_ADMINEMAIL and provide a text
                          message with each.  For RPL_ADMINLOC1 a
                          description of what city, state and country
                          the server is in is expected, followed by
                          details of the university and department
                          (RPL_ADMINLOC2) and finally the administrative
                          contact for the server (an email address here
                          is required) in RPL_ADMINEMAIL.



--------------------------------------------------------------------------------
Page 56

6.3 Reserved numerics.
These numerics are not described above since they fall into one of the following categories:


1 no longer in use;

2 reserved for future planned use;

3 in current use but are part of a non-generic 'feature' of
the current IRC server.

        209     RPL_TRACECLASS          217     RPL_STATSQLINE
        231     RPL_SERVICEINFO         232     RPL_ENDOFSERVICES
        233     RPL_SERVICE             234     RPL_SERVLIST
        235     RPL_SERVLISTEND
        316     RPL_WHOISCHANOP         361     RPL_KILLDONE
        362     RPL_CLOSING             363     RPL_CLOSEEND
        373     RPL_INFOSTART           384     RPL_MYPORTIS
        466     ERR_YOUWILLBEBANNED     476     ERR_BADCHANMASK
        492     ERR_NOSERVICEHOST


*/
	else
		Con_SubPrintf("irc", "%s\n", msg);

	memmove(irc->bufferedinmessage, nextmsg, irc->bufferedinammount - (msg-irc->bufferedinmessage));
	irc->bufferedinammount-=nextmsg-irc->bufferedinmessage;
	return IRC_CONTINUE;
}


//functions above this line allow connections to multiple servers.
//it is just the control functions that only allow one server.

int IRC_Frame(int *args)
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
			Con_SubPrintf("irc", "Disconnected from irc\n");
		}
	}
	return 0;
}

void IRC_Command(void)
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
				IRC_SetNick(ircclient, defaultnick);
				IRC_SetUser(ircclient, defaultuser);
			}
		}
		else if (!strcmp(com_token+1, "nick"))
		{
			msg = COM_Parse(msg);
			//Q_strncpyz(defaultnick, com_token, sizeof(defaultnick));
			//if (ircclient)
				//IRC_SetNick(ircclient, defaultnick);
				// the above 3 lines, don't work too well :(
			IRC_AddClientMessage(ircclient, va("nick %s", com_token));
		}
		else if (!strcmp(com_token+1, "user"))
		{
			msg = COM_Parse(msg);
			Q_strncpyz(defaultuser, com_token, sizeof(defaultuser));
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
		else if (!strcmp(com_token+1, "join"))
		{
			msg = COM_Parse(msg);
			IRC_AddClientMessage(ircclient, va("JOIN %s", com_token));
		}
		else if (!strcmp(com_token+1, "part") || !strcmp(com_token+1, "leave"))
		{
			msg = COM_Parse(msg);
			if (!*com_token)
				IRC_AddClientMessage(ircclient, va("PART %s", ircclient->defaultdest));
			else
				IRC_AddClientMessage(ircclient, va("PART %s", com_token));
		}
		else if (!strcmp(com_token+1, "msg"))
		{
			msg = COM_Parse(msg);
			IRC_AddClientMessage(ircclient, va("PRIVMSG %s :%s", com_token, msg));
			Con_SubPrintf(com_token, "%s: %s\n", ircclient->nick, msg);
		}
		else if (!strcmp(com_token+1, "quote"))
		{
			IRC_AddClientMessage(ircclient, va("%s", msg));
		}
		else if (!strcmp(com_token+1, "quit"))
		{
			IRC_AddClientMessage(ircclient, va("QUIT :FTE QuakeWorld IRC-Plugin http://fteqw.sf.net"));
		}
		else if (!strcmp(com_token+1, "whois"))
		{
			msg = COM_Parse(msg);
			IRC_AddClientMessage(ircclient, va("WHOIS :%s",com_token));
		}
		else if (!strcmp(com_token+1, "dest"))
		{
			msg = COM_Parse(msg);
			Q_strncpyz(ircclient->defaultdest, com_token, sizeof(ircclient->defaultdest));
		}
		else if (!strcmp(com_token+1, "ping"))
		{
			IRC_AddClientMessage(ircclient, va("PRIVMSG %s :\001PING%s\001", ircclient->defaultdest, msg));
		}
		else if (!strcmp(com_token+1, "me"))
		{
			if(*msg <= ' ' && *msg)
				msg++;
			IRC_AddClientMessage(ircclient, va("PRIVMSG %s :\001ACTION %s\001", ircclient->defaultdest, msg));
			Con_SubPrintf(ircclient->defaultdest, "***%s %s\n", ircclient->nick, msg);
		}
	}
	else
	{
		if (ircclient)
		{
			msg = imsg;
			IRC_AddClientMessage(ircclient, va("PRIVMSG %s :%s", ircclient->defaultdest, msg));
			Con_SubPrintf(ircclient->defaultdest, "%s: %s\n", ircclient->nick, msg);
		}
		else
			Con_Printf("Not connected\ntype \"" COMMANDPREFIX "/open IRCSERVER\" to connect\n");
	}
}
