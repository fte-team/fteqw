#include "bothdefs.h"

#ifdef IRCCLIENT

//Released under the terms of the gpl as this file uses a bit of quake derived code. All sections of the like are marked as such

//these should be in a header.
void IRC_Frame(void);
void IRC_Command(char *imsg);

//porting zone:
#ifdef TERMINALIRC	//windows console prog.

#define Q_strncpyz(o, i, l) do {strncpy(o, i, l-1);o[l-1]='\0';}while(0)

#pragma comment (lib, "wsock32.lib")
#include <winsock.h>

	#include <stdio.h>
	#include <conio.h>
	#include <malloc.h>
	#include <stdarg.h>
	enum {false, true};

	void main(void)
	{
		char buffer[1024];
		int len = 0;

		WSADATA wsadata;

		len = WSAStartup (MAKEWORD(1, 1), &wsadata);

		if (len)
		{
			printf ("Winsock initialization failed.");
			exit(len);
		}

		for(;;)
		{
			if (kbhit())
			{
				buffer[len] = getch();
				printf("%c", buffer[len]);
				if (buffer[len] == 8)
				{
					if (len)
					{
						printf(" %c", buffer[len]);
						len--;
					}
				}
				else if (buffer[len] == '\r')	//return was hit
				{
					buffer[len] = '\0';
					if (!strcmp(buffer, "/quitprog"))	//special case... he he he
					{
						WSACleanup();
						exit(0);
					}
					printf("\n");
					IRC_Command(buffer);
					len = 0;
				}
				else
					len++;
			}
			IRC_Frame();
			Sleep(100);
		}
	}

	char	*va(char *format, ...)	//stolen from quake
	{
		va_list		argptr;
		static char		string[1024];
		
		va_start (argptr, format);
		vsprintf (string, format,argptr);
		va_end (argptr);

		return string;	
	}

	#define dprintf		printf
	#define wprintf		printf
	#define twprintf	printf
	#define COLOURGREEN	""
	#define COMMANDPREFIX ""
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

#elif defined(QUAKE2)	//client
	#include "client.h"

	#define printf		Com_Printf
	#define dprintf		Com_DPrintf
	#define wprintf		Com_Printf
	#define COLOURGREEN	""
	#define COMMANDPREFIX "irc "
	#define playsound(s) S_StartLocalSound(s)

	#define IRC_Calloc	Z_Malloc
	#define IRC_Free		Z_Free

#define TL_NETGETPACKETERROR "NET_GetPacket Error %s\n"

#elif 1	//FTE based exe
	#include "../http/iweb.h"

	#define printf		IWebPrintf
	#define dprintf		IWebDPrintf
	#define wprintf		IWebWarnPrintf
	#define twprintf	Con_TPrintf
	#define COLOURGREEN	"^2"
	#define COMMANDPREFIX "irc "
	#define playsound(s) S_LocalSound(s)


	#define IRC_Malloc	IWebMalloc
	#define IRC_Free		IWebFree

#else
#error "Bad porting target"
#endif



//functions are geared towards windows because windows is fussiest.
//constants towards unix cos they are more proper

#ifdef _WIN32

#include <winsock.h>

#define EWOULDBLOCK	WSAEWOULDBLOCK
#define EMSGSIZE	WSAEMSGSIZE
#define ECONNRESET	WSAECONNRESET
#define ECONNABORTED	WSAECONNABORTED
#define ECONNREFUSED	WSAECONNREFUSED
#define EADDRNOTAVAIL	WSAEADDRNOTAVAIL

#define qerrno WSAGetLastError()
#else
#define qerrno errno

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <errno.h>

#include <unistd.h>

#ifdef sun
#include <sys/filio.h>
#endif

#ifdef NeXT
#include <libc.h>
#endif

#define closesocket close
#define ioctlsocket ioctl
#endif







int	IRC_StringToSockaddr (char *s, struct sockaddr *sadr)	//From Quake2
{
	struct hostent	*h;
	char	*colon;
	char	copy[128];
	
	memset (sadr, 0, sizeof(*sadr));

	((struct sockaddr_in *)sadr)->sin_family = AF_INET;
	
	((struct sockaddr_in *)sadr)->sin_port = 0;

	strcpy (copy, s);
	// strip off a trailing :port if present
	for (colon = copy ; *colon ; colon++)
		if (*colon == ':')
		{
			*colon = 0;
			((struct sockaddr_in *)sadr)->sin_port = htons((short)atoi(colon+1));	
		}
	
	if (copy[0] >= '0' && copy[0] <= '9')
	{
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = inet_addr(copy);
	}
	else
	{
		if (! (h = gethostbyname(copy)) )
			return 0;
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = *(int *)h->h_addr_list[0];
	}
	
	return true;
}





//\r\n is used to end a line.
//meaning \0s are valid.
//but never used cos it breaks strings



#define IRC_MAXNICKLEN 32	//9 and a null term
#define IRC_MAXMSGLEN 512


char defaultnick[IRC_MAXNICKLEN+1];


typedef struct {
	char server[64];
	int port;

	int socket;

	char nick[IRC_MAXNICKLEN];
	char pwd[64];
	char realname[128];
	char hostname[128];

	char defaultdest[IRC_MAXNICKLEN];//channel or nick

	char bufferedinmessage[IRC_MAXMSGLEN+1];	//there is a max size for protocol. (conveinient eh?) (and it's text format)
	int bufferedinammount;
} ircclient_t;

ircclient_t *ircclient;
void IRC_AddClientMessage(ircclient_t *irc, char *msg)
{
	send(irc->socket, msg, strlen(msg), 0);	//FIXME: This needs rewriting to cope with errors.
}

ircclient_t *IRC_Connect(char *server, int port)
{
	ircclient_t *irc;
	unsigned long _true = true;
	struct sockaddr_in	from;


	irc = IRC_Malloc(sizeof(ircclient_t));
	if (!irc)
		return NULL;
	
	memset(irc, 0, sizeof(ircclient_t));

	

	if ((irc->socket = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		wprintf ("IRC_TCP_OpenSocket: socket: %s\n", strerror(qerrno));
		return NULL;
	}

	IRC_StringToSockaddr(server, (struct sockaddr *)&from);

	//not yet blocking. So no frequent attempts please...
	//non blocking prevents connect from returning worthwhile sensible value.
	if (connect(irc->socket, (struct sockaddr *)&from, sizeof(from)) == -1)
	{
		wprintf ("IRC_TCP_OpenSocket: connect: %i %s\n", qerrno, strerror(qerrno));
		closesocket(irc->socket);		
		IRC_Free(irc);
		return NULL;
	}
	
	if (ioctlsocket (irc->socket, FIONBIO, &_true) == -1)	//now make it non blocking.
	{
		wprintf ("IRC_TCP_OpenSocket: ioctl FIONBIO: %s\n", strerror(qerrno));
		return NULL;
	}

	Q_strncpyz(irc->server, server, sizeof(irc->server));
	strcpy(irc->nick, "anonymous");
	strcpy(irc->realname, "anonymous");

	gethostname(irc->hostname, sizeof(irc->hostname));
	irc->hostname[sizeof(irc->hostname)-1] = 0;


	return irc;
}
void IRC_SetPass(ircclient_t *irc, char *pass)
{
	IRC_AddClientMessage(irc, va("PASS %s\r\n", pass));
}
void IRC_SetNick(ircclient_t *irc, char *nick)
{
	IRC_AddClientMessage(irc, va("NICK %s\r\n", nick));
}
void IRC_SetUser(ircclient_t *irc, char *user)
{
	IRC_AddClientMessage(irc, va("USER %s %s %s :%s\r\n", user, irc->hostname, irc->server, "Something"));
}

#define IRC_DONE 0
#define IRC_CONTINUE 1
#define IRC_KILL 2
int IRC_ClientFrame(ircclient_t *irc)
{
	char prefix[64];
	int ret;
	char *nextmsg, *msg;
	ret = recv(irc->socket, irc->bufferedinmessage+irc->bufferedinammount, sizeof(irc->bufferedinmessage)-1 - irc->bufferedinammount, 0);		
	if (ret == 0)
		return IRC_KILL;
	if (ret == -1)
	{
		if (qerrno == EWOULDBLOCK)
		{
			if (!irc->bufferedinammount)	//if we are half way through a message, read any possible conjunctions.
				return IRC_DONE;	//remove
		}
		else
		{
			if (qerrno == ECONNABORTED || qerrno == ECONNRESET)
				return IRC_KILL;

			twprintf (TL_NETGETPACKETERROR, strerror(qerrno));			
			return IRC_KILL;
		}
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

	if (*msg == ':')	//we need to strip off the prefix
	{
		char *sp = strchr(msg, ' ');
		if (!sp)
		{
			dprintf("Ignoring bad message\n%s\n", msg);
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

	if (!strncmp(msg, "PING ", 5))
	{
		dprintf("%s\n", msg);
		IRC_AddClientMessage(irc, va("PONG %s\r\n", msg+5));
	}
	else if (!strncmp(msg, "PRIVMSG ", 7) || !strncmp(msg, "NOTICE ", 6))	//no autoresponses to notice please, and any autoresponses should be in the form of a notice
	{
		char *exc = strchr(prefix, '!');
		char *col = strchr(msg+6, ':');

		playsound ("misc/talk.wav");

		if (exc && col)
		{
			*exc = '\0';
			printf(COLOURGREEN "%s: %s\n", prefix, col+1);	//from client
		}
		else dprintf(COLOURGREEN ":%s%s\n", prefix, msg);	//direct server message
	}
	else if (!strncmp(msg, "NICK ", 5))
	{
		char *exc = strchr(prefix, '!');
		char *col = strchr(msg+6, ':');
		if (exc && col)
		{
			*exc = '\0';
			printf(COLOURGREEN "%s changes name to %s\n", prefix, col+1);
		}
		else dprintf(COLOURGREEN ":%s%s\n", prefix, msg+6);
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
			*exc = '\0';
			printf(COLOURGREEN "%s leaves channel %s\n", prefix, col);
		}
		else dprintf(COLOURGREEN ":%sPART %s\n", prefix, msg+5);
	}
	else if (!strncmp(msg, "JOIN ", 5))
	{
		char *exc = strchr(prefix, '!');
		char *col = strchr(msg+5, ':');
		if (exc && col)
		{
			*exc = '\0';
			printf(COLOURGREEN "%s joins channel %s\n", prefix, col+1);
		}
		else dprintf(COLOURGREEN ":%sJOIN %s\n", prefix, msg+5);
	}
	else if (!strncmp(msg, "322 ", 4))	//channel listing
	{
		dprintf("%s\n", msg);
	}
	else if (!strncmp(msg, "375 ", 4))
	{
		dprintf("%s\n", msg);
	}
	else if (!strncmp(msg, "431 ", 4))
	{
		dprintf("You need to specify a nickname\n");
	}
	else if (!strncmp(msg, "432 ", 4))
	{
		dprintf("Erroneous/invalid nickname given\n");
	}
	else if (!strncmp(msg, "433 ", 4))
	{
		dprintf("Nickname is already in use by someone\n");
	}
  	else if (!strncmp(msg, "436 ", 4))
	{
		dprintf("Nickname collision. You will probably need to reconnect\n");
	}
	else if (!strncmp(msg, "372 ", 4))
	{
		char *text = strstr(msg, ":-");
		if (text)
			dprintf("%s\n", text+2);
		else
			dprintf("%s\n", msg);
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
		dprintf("%s\n", msg);

	memmove(irc->bufferedinmessage, nextmsg, irc->bufferedinammount - (msg-irc->bufferedinmessage));
	irc->bufferedinammount-=nextmsg-irc->bufferedinmessage;
	return IRC_CONTINUE;
}


//functions above this line allow connections to multiple servers.
//it is just the control functions that only allow one server.

void IRC_Frame(void)
{
	int stat = IRC_CONTINUE;
	if (ircclient)
	{
		while(stat == IRC_CONTINUE)
			stat = IRC_ClientFrame(ircclient);
		if (stat == IRC_KILL)
		{
			closesocket(ircclient->socket);
			IRC_Free(ircclient);
			ircclient = NULL;
			Con_Printf("Disconnected from irc\n");
		}
	}
}

void IRC_Command(char *imsg)
{
	char *msg;
	msg = COM_Parse(imsg);

	if (*com_token == '/')
	{
		if (!strcmp(com_token+1, "open") || !strcmp(com_token+1, "connect"))
		{
			if (ircclient)
			{
				printf("You are already connected\nPlease /quit first");
				return;
			}
			msg = COM_Parse(msg);
			ircclient = IRC_Connect(com_token, 8080);
			if (ircclient)
			{
				char username[512] = "Unknown User";
#ifdef _WIN32
				DWORD blen = sizeof(username)-1;
#endif

				printf("Trying to connect\n");
#ifdef _WIN32
				GetUserName(username, &blen);
#endif
				IRC_SetPass(ircclient, "");
				IRC_SetNick(ircclient, defaultnick);
				IRC_SetUser(ircclient, username);
			}
		}
		else if (!strcmp(com_token+1, "nick"))
		{
			msg = COM_Parse(msg);
			Q_strncpyz(defaultnick, com_token, sizeof(defaultnick));
			if (ircclient)
				IRC_SetNick(ircclient, defaultnick);
		}
		else if (!strcmp(com_token+1, "list"))
		{
			if (ircclient)
				IRC_AddClientMessage(ircclient, "LIST\r\n");
			else
				printf("Not connected\n");
		}
		else if (!strcmp(com_token+1, "join"))
		{
			msg = COM_Parse(msg);
			if (ircclient)
				IRC_AddClientMessage(ircclient, va("JOIN %s\r\n", com_token));
			else
				printf("Not connected\n");
		}
		else if (!strcmp(com_token+1, "part") || !strcmp(com_token+1, "leave"))
		{
			msg = COM_Parse(msg);
			if (ircclient)
				IRC_AddClientMessage(ircclient, va("PART %s\r\n", com_token));
			else
				printf("Not connected\n");
		}
		else if (!strcmp(com_token+1, "msg"))
		{
			msg = COM_Parse(msg);
			if (ircclient)
				IRC_AddClientMessage(ircclient, va("PRIVMSG %s :%s\r\n", com_token, msg));
			else
				printf("Not connected\n");
		}
		else if (!strcmp(com_token+1, "quote"))
		{
			if (ircclient)
				IRC_AddClientMessage(ircclient, va("%s\r\n", msg));
			else
				printf("Not connected\n");
		}
		else if (!strcmp(com_token+1, "quit"))
		{
			if (ircclient)
				IRC_AddClientMessage(ircclient, va("QUIT\r\n"));
			else
				printf("Not connected\n");
		}
	}
	else
	{
		if (ircclient)
		{
			IRC_AddClientMessage(ircclient, va("PRIVMSG %s :%s\r\n", imsg));
		}
		else
			printf("Not connected\ntype \"" COMMANDPREFIX "/open IRCSERVER\" to connect\n");
	}
}

#endif
