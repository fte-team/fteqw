//Released under the terms of the gpl as this file uses a bit of quake derived code. All sections of the like are marked as such

#include "../plugin.h"
#include <time.h>
#include "../../engine/common/netinc.h"
#include "xml.h"

//#define NOICE
#define VOIP_SPEEX

#ifdef VOIP_SPEEX
#define VOIP
#endif

#define DEFAULTDOMAIN ""
#define DEFAULTRESOURCE "Quake"
#define QUAKEMEDIATYPE "quake"
#define QUAKEMEDIAXMLNS "fteqw.com:netmedia"
#define DEFAULTICEMODE ICEM_ICE

icefuncs_t *piceapi;


#define Q_strncpyz(o, i, l) do {strncpy(o, i, l-1);o[l-1]='\0';}while(0)

#define JCL_BUILD "3"


#define ARGNAMES ,sock,certhostname
BUILTINR(int, Net_SetTLSClient, (qhandle_t sock, const char *certhostname));
#undef ARGNAMES

#define ARGNAMES ,funcname
BUILTINR(void *, Plug_GetNativePointer, (const char *funcname));
#undef ARGNAMES

void (*Con_TrySubPrint)(const char *conname, const char *message);
void Fallback_ConPrint(const char *conname, const char *message)
{
	pCon_Print(message);
}

void Con_SubPrintf(char *subname, char *format, ...)
{
	va_list		argptr;
	static char		string[1024];

	va_start (argptr, format);
	Q_vsnprintf (string, sizeof(string), format,argptr);
	va_end (argptr);

	Con_TrySubPrint(subname, string);
}


//porting zone:




	#define COLOURGREEN	"^2"
	#define COLORWHITE "^7"
	#define COLOURWHITE "^7" // word
	#define COLOURRED "^1"
	#define COLOURYELLOW "^3"
	#define COLOURPURPLE "^5"
	#define COMMANDPREFIX "xmpp"
	#define COMMANDPREFIX2 "jab"
	#define COMMANDPREFIX3 "jabbercl"
	#define playsound(s)


	#define TL_NETGETPACKETERROR "NET_GetPacket Error %s\n"

	static char *JCL_ParseOut (char *data, char *buf, int bufsize)	//this is taken out of quake
	{
		int		c;
		int		len;

		len = 0;
		buf[0] = 0;

		if (!data)
			return NULL;

	// skip whitespace
		while ( (c = *data) <= ' ')
		{
			if (c == 0)
				return NULL;			// end of file;
			data++;
		}

	// handle quoted strings specially
		if (c == '\"')
		{
			data++;
			while (1)
			{
				if (len >= bufsize-1)
					return data;

				c = *data++;
				if (c=='\"' || !c)
				{
					buf[len] = 0;
					return data;
				}
				buf[len] = c;
				len++;
			}
		}

	// parse a regular word
		do
		{
			if (len >= bufsize-1)
				return data;

			buf[len] = c;
			data++;
			len++;
			c = *data;
		} while (c>32);

		buf[len] = 0;
		return data;
	}


char *JCL_Info_ValueForKey (char *s, const char *key, char *valuebuf, int valuelen)
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
				*valuebuf='\0';
				return valuebuf;
			}
			*o++ = *s++;
			if (o+2 >= pkey+sizeof(pkey))	//hrm. hackers at work..
			{
				*valuebuf='\0';
				return valuebuf;
			}
		}
		*o = 0;
		s++;

		o = valuebuf;

		while (*s != '\\' && *s)
		{
			if (!*s)
			{
				*valuebuf='\0';
				return valuebuf;
			}
			*o++ = *s++;

			if (o+2 >= valuebuf+valuelen)	//hrm. hackers at work..
			{
				*valuebuf='\0';
				return valuebuf;
			}
		}
		*o = 0;

		if (!strcmp (key, pkey) )
			return valuebuf;

		if (!*s)
		{
			*valuebuf='\0';
			return valuebuf;
		}
		s++;
	}
}


void RenameConsole(char *totrim);
void JCL_Command(char *consolename);
void JCL_LoadConfig(void);
void JCL_WriteConfig(void);

qintptr_t JCL_ExecuteCommand(qintptr_t *args)
{
	char cmd[256];
	pCmd_Argv(0, cmd, sizeof(cmd));
	if (!strcmp(cmd, COMMANDPREFIX) || !strcmp(cmd, COMMANDPREFIX2) || !strcmp(cmd, COMMANDPREFIX3))
	{
		if (!args[0])
			JCL_Command("");
		return true;
	}
	return false;
}

qintptr_t JCL_ConsoleLink(qintptr_t *args);
qintptr_t JCL_ConExecuteCommand(qintptr_t *args);

qintptr_t JCL_Frame(qintptr_t *args);

qintptr_t Plug_Init(qintptr_t *args)
{
	if (	Plug_Export("Tick", JCL_Frame) &&
		Plug_Export("ExecuteCommand", JCL_ExecuteCommand))
	{
		CHECKBUILTIN(Net_SetTLSClient);
		if (!BUILTINISVALID(Net_SetTLSClient))
			Con_Printf("XMPP Plugin Loaded ^1without^7 TLS\n");
		else
			Con_Printf("XMPP Plugin Loaded. For help, use: ^[/"COMMANDPREFIX" /help^]\n");

		Plug_Export("ConsoleLink", JCL_ConsoleLink);

		if (!Plug_Export("ConExecuteCommand", JCL_ConExecuteCommand))
		{
			Con_Printf("XMPP plugin in single-console mode\n");
			Con_TrySubPrint = Fallback_ConPrint;
		}
		else
			Con_TrySubPrint = pCon_SubPrint;

		pCmd_AddCommand(COMMANDPREFIX);
		pCmd_AddCommand(COMMANDPREFIX2);
		pCmd_AddCommand(COMMANDPREFIX3);

		//flags&1 == archive
		pCvar_Register("xmpp_nostatus",				"0", 0, "xmpp");
		pCvar_Register("xmpp_autoacceptjoins",		"0", 0, "xmpp");
		pCvar_Register("xmpp_autoacceptinvites",	"0", 0, "xmpp");
		pCvar_Register("xmpp_autoacceptvoice",		"0", 0, "xmpp");
		pCvar_Register("xmpp_debug",				"0", 0, "xmpp");
		pCvar_Register("xmpp_disabletls",			"0", CVAR_NOTFROMSERVER, "xmpp");
		pCvar_Register("xmpp_allowplainauth",		"0", CVAR_NOTFROMSERVER, "xmpp");

		CHECKBUILTIN(Plug_GetNativePointer);
		if (BUILTINISVALID(Plug_GetNativePointer))
			piceapi = pPlug_GetNativePointer(ICE_API_CURRENT);

		JCL_LoadConfig();
		return 1;
	}
	else
		Con_Printf("JCL Client Plugin failed\n");
	return 0;
}










//\r\n is used to end a line.
//meaning \0s are valid.
//but never used cos it breaks strings


#define JCL_MAXMSGLEN 10000

#define CAP_QUERIED	1	//a query is pending or something.
#define CAP_VOICE	2	//supports voice
#define CAP_INVITE	4	//supports game invites.

typedef struct bresource_s
{
	char bstatus[128];	//basic status
	char fstatus[128];	//full status
	char server[256];
	int servertype;	//0=none, 1=already a client, 2=joinable

	unsigned int caps;

	struct bresource_s *next;

	char resource[1];
} bresource_t;
typedef struct buddy_s
{
	bresource_t *resources;
	bresource_t *defaultresource;	//this is the one that last replied
	int defaulttimestamp;
	qboolean friended;

	char name[256];

	struct buddy_s *next;
	char accountdomain[1];	//no resource on there
} buddy_t;
typedef struct jclient_s
{
	char server[64];
	int port;

	enum
	{
		JCL_DEAD,		//not connected. connection died or something.
		JCL_AUTHING,	//connected, but not able to send any info on it other than to auth
		JCL_ACTIVE		//we're connected, we got a buddy list and everything
	} status;
	unsigned int timeout;		//reconnect/ping timer

	qhandle_t socket;

	//we buffer output for times when the outgoing socket is full.
	//mostly this only happens at the start of the connection when the socket isn't actually open yet.
	char *outbuf;
	int outbufpos;
	int outbuflen;
	int outbufmax;

	char bufferedinmessage[JCL_MAXMSGLEN+1];	//servers are required to be able to handle messages no shorter than a specific size.
												//which means we need to be able to handle messages when they get to us.
												//servers can still handle larger messages if they choose, so this might not be enough.
	int bufferedinammount;

	char defaultdest[256];

	char domain[256];
	char username[256];
	char password[256];
	char resource[256];
	char jid[256];	//this is our full username@domain/resource string
	char localalias[256];//this is what's shown infront of outgoing messages. >> by default until we can get our name.

	int tagdepth;
	int openbracket;
	int instreampos;

	qboolean tlsconnect;	//use the old tls method on port 5223.
	qboolean connected;	//fully on server and authed and everything.
	qboolean issecure;	//tls enabled (either upgraded or initially)
	qboolean streamdebug;	//echo the stream to subconsoles

	qboolean preapproval;	//server supports presence preapproval

	char curquakeserver[2048];
	char defaultnamespace[2048];	//should be 'jabber:client' or blank (and spammy with all the extra xmlns attribs)

	struct iq_s
	{
		struct iq_s *next;
		char id[64];
		int timeout;
		qboolean (*callback) (struct jclient_s *jcl, struct subtree_s *tree, struct iq_s *iq);
		void *usrptr;
	} *pendingiqs;

	struct c2c_s
	{
		struct c2c_s *next;
		enum iceproto_e mediatype;
		enum icemode_e method;	//ICE_RAW or ICE_ICE. this is what the peer asked for. updated if we degrade it.
		qboolean accepted;	//connection is going
		qboolean creator;	//true if we're the creator.

		struct icestate_s *ice;
		char *peeraddr;
		int peerport;

		char *with;
		char sid[1];
	} *c2c;

	buddy_t *buddies;
} jclient_t;
jclient_t *jclient;
int jclient_curtime;

struct subtree_s;

void JCL_AddClientMessagef(jclient_t *jcl, char *fmt, ...);
qboolean JCL_FindBuddy(jclient_t *jcl, char *jid, buddy_t **buddy, bresource_t **bres);
void JCL_GeneratePresence(jclient_t *jcl, qboolean force);
struct iq_s *JCL_SendIQf(jclient_t *jcl, qboolean (*callback) (jclient_t *jcl, struct subtree_s *tree, struct iq_s *iq), char *iqtype, char *target, char *fmt, ...);
struct iq_s *JCL_SendIQNode(jclient_t *jcl, qboolean (*callback) (jclient_t *jcl, xmltree_t *tree, struct iq_s *iq), char *iqtype, char *target, xmltree_t *node, qboolean destroynode);

char *TrimResourceFromJid(char *jid)
{
	char *slash;
	slash = strchr(jid, '/');
	if (slash)
	{
		*slash = '\0';
		return slash+1;
	}
	return NULL;
}

static struct c2c_s *JCL_JingleCreateSession(jclient_t *jcl, char *with, qboolean creator, char *sid, int method, int mediatype)
{
	struct icestate_s *ice = NULL;
	struct c2c_s *c2c;
	char generatedname[64];
	
	if (piceapi)
		ice = piceapi->ICE_Create(NULL, sid, with, method, mediatype);
	if (ice)
	{
		piceapi->ICE_Get(ice, "sid", generatedname, sizeof(generatedname));
		sid = generatedname;

		piceapi->ICE_Set(ice, "codec96", "speex@8000");
		piceapi->ICE_Set(ice, "codec97", "speex@16000");
		piceapi->ICE_Set(ice, "codec98", "opus");
	}
	else
		return NULL;	//no way to get the local ip otherwise, which means things won't work proper

	c2c = malloc(sizeof(*c2c) + strlen(sid));
	memset(c2c, 0, sizeof(*c2c));
	c2c->next = jcl->c2c;
	jcl->c2c = c2c;
	strcpy(c2c->sid, sid);

	c2c->mediatype = mediatype;
	c2c->creator = creator;
	c2c->method = method;

	//FIXME: we need to query this from the server.
	//however, google don't implement the 'standard' way
	//the 'standard' way contains a huge big fat do-not-implement message.
	//and google's way equally says 'don't implement'... so...
	//as I kinda expect most users to use google's network anyway, I *hope* they won't mind too much. I doubt they'll get much traffic anyway. its just stun.
	piceapi->ICE_Set(ice, "stunport", "19302");
	piceapi->ICE_Set(ice, "stunip", "stun.l.google.com");

	//copy out the interesting parameters
	c2c->with = strdup(with);
	c2c->ice = ice;
	return c2c;
}
static qboolean JCL_JingleAcceptAck(jclient_t *jcl, xmltree_t *tree, struct iq_s *iq)
{
	struct c2c_s *c2c;
	if (tree)
	{
		for (c2c = jcl->c2c; c2c; c2c = c2c->next)
		{
			if (c2c == iq->usrptr)
			{
				if (c2c->ice)
					piceapi->ICE_Set(c2c->ice, "state", STRINGIFY(ICE_CONNECTING));
			}
		}
	}
	return true;
}
/*
sends a jingle message to the peer.
action should be one of multiple things:
session-terminate	- totally not acceptable. this also closes the c2c
session-accept		- details are okay. this also begins ice polling (on iq ack, once we're sure the peer got our message)

(internally generated) transport-replace	- details are okay, except we want a different transport method.
*/
qboolean JCL_JingleSend(jclient_t *jcl, struct c2c_s *c2c, char *action)
{
	qboolean result;
	xmltree_t *jingle;
	struct icestate_s *ice = c2c->ice;
	qboolean wasaccept = false;
	int transportmode = ICEM_ICE;

	if (!ice)
		action = "session-terminate";

	jingle = XML_CreateNode(NULL, "jingle", "urn:xmpp:jingle:1", "");
	XML_AddParameter(jingle, "sid", c2c->sid);

	if (!strcmp(action, "session-initiate"))
	{	//these attributes are meant to only be present in initiate. for call forwarding etc. which we don't properly support.
		XML_AddParameter(jingle, "initiator", jcl->jid);
	}

	if (!strcmp(action, "session-terminate"))
	{
		struct c2c_s **link;
		for (link = &jcl->c2c; *link; link = &(*link)->next)
		{
			if (*link == c2c)
			{
				*link = c2c->next;
				break;
			}
		}
		if (c2c->ice)
			piceapi->ICE_Close(c2c->ice);

		result = false;
	}
	else
	{
		xmltree_t *content = XML_CreateNode(jingle, "content", "", "");

		result = true;

		if (!strcmp(action, "session-accept"))
		{
			if (c2c->method == transportmode)
			{
				XML_AddParameter(jingle, "responder", jcl->jid);
				c2c->accepted = wasaccept = true;
			}
			else
				action = "transport-replace";
		}

		{
			xmltree_t *description;
			xmltree_t *transport;
			if (transportmode == ICEM_RAW)
			{
				transport = XML_CreateNode(content, "transport", "urn:xmpp:jingle:transports:raw-udp:1", "");
				{
					xmltree_t *candidate;
					struct icecandinfo_s *b = NULL;
					struct icecandinfo_s *c;
					while ((c = piceapi->ICE_GetLCandidateInfo(ice)))
					{
						if (!b || b->priority < c->priority)
							b = c;
					}
					if (b)
					{
						candidate = XML_CreateNode(transport, "candidate", "", "");
						XML_AddParameter(candidate, "ip", b->addr);
						XML_AddParameteri(candidate, "port", b->port);
						XML_AddParameter(candidate, "id", b->candidateid);
						XML_AddParameteri(candidate, "generation", b->generation);
						XML_AddParameteri(candidate, "component", b->component);
					}
				}
			}
			else if (transportmode == ICEM_ICE)
			{
				char val[64];
				transport = XML_CreateNode(content, "transport", "urn:xmpp:jingle:transports:ice-udp:1", "");
				piceapi->ICE_Get(ice, "lufrag",  val, sizeof(val));
				XML_AddParameter(transport, "ufrag", val);
				piceapi->ICE_Get(ice, "lpwd",  val, sizeof(val));
				XML_AddParameter(transport, "pwd", val);
				{
					struct icecandinfo_s *c;
					while ((c = piceapi->ICE_GetLCandidateInfo(ice)))
					{
						char *ctypename[]={"host", "srflx", "prflx", "relay"};
						xmltree_t *candidate = XML_CreateNode(transport, "candidate", "", "");
						XML_AddParameter(candidate, "type", ctypename[c->type]);
						XML_AddParameter(candidate, "protocol", "udp");	//is this not just a little bit redundant? ice-udp? seriously?
						XML_AddParameteri(candidate, "priority", c->priority);
						XML_AddParameteri(candidate, "port", c->port);
						XML_AddParameteri(candidate, "network", c->network);
						XML_AddParameter(candidate, "ip", c->addr);
						XML_AddParameter(candidate, "id", c->candidateid);
						XML_AddParameteri(candidate, "generation", c->generation);
						XML_AddParameteri(candidate, "foundation", c->foundation);
						XML_AddParameteri(candidate, "component", c->component);
					}
				}
			}
#ifdef VOIP
			if (c2c->mediatype == ICEP_VOICE)
			{
				xmltree_t *payload;
				int i;

				XML_AddParameter(content, "senders", "both");
				XML_AddParameter(content, "name", "audio-session");
				XML_AddParameter(content, "creator", "initiator");

				description = XML_CreateNode(content, "description", "urn:xmpp:jingle:apps:rtp:1", "");
				XML_AddParameter(description, "media", "audio");

				for (i = 96; i <= 127; i++)
				{
					char codecname[64];
					char argn[64];
					Q_snprintf(argn, sizeof(argn), "codec%i", i);
					piceapi->ICE_Get(ice, argn,  codecname, sizeof(codecname));

					if (!strcmp(codecname, "speex@8000"))
					{	//speex narrowband
						payload = XML_CreateNode(description, "payload-type", "", "");
						XML_AddParameter(payload, "channels", "1");
						XML_AddParameter(payload, "clockrate", "8000");
						XML_AddParameter(payload, "id", argn+5);
						XML_AddParameter(payload, "name", "SPEEX");
					}
					else if (!strcmp(codecname, "speex@16000"))
					{	//speex wideband
						payload = XML_CreateNode(description, "payload-type", "", "");
						XML_AddParameter(payload, "channels", "1");
						XML_AddParameter(payload, "clockrate", "16000");
						XML_AddParameter(payload, "id", argn+5);
						XML_AddParameter(payload, "name", "SPEEX");
					}
					else if (!strcmp(codecname, "speex@32000"))
					{	//speex ultrawideband
						payload = XML_CreateNode(description, "payload-type", "", "");
						XML_AddParameter(payload, "channels", "1");
						XML_AddParameter(payload, "clockrate", "32000");
						XML_AddParameter(payload, "id", argn+5);
						XML_AddParameter(payload, "name", "SPEEX");
					}
					else if (!strcmp(codecname, "opus"))
					{	//opus codec.
						payload = XML_CreateNode(description, "payload-type", "", "");
						XML_AddParameter(payload, "channels", "1");
						XML_AddParameter(payload, "id", argn+5);
						XML_AddParameter(payload, "name", "OPUS");
					}
				}
			}
#endif
			else
			{
				description = XML_CreateNode(content, "description", QUAKEMEDIAXMLNS, "");
				XML_AddParameter(description, "media", QUAKEMEDIATYPE);
				if (c2c->mediatype == ICEP_QWSERVER)
					XML_AddParameter(description, "host", "me");
				else if (c2c->mediatype == ICEP_QWCLIENT)
					XML_AddParameter(description, "host", "you");
			}
		}
	}

	XML_AddParameter(jingle, "action", action);

//	Con_Printf("Sending Jingle:\n");
//	XML_ConPrintTree(jingle, 1);
	JCL_SendIQNode(jcl, wasaccept?JCL_JingleAcceptAck:NULL, "set", c2c->with, jingle, true)->usrptr = c2c;

	return result;
}

void JCL_JingleTimeouts(jclient_t *jcl, qboolean killall)
{
	struct c2c_s *c2c;
	for (c2c = jcl->c2c; c2c; c2c = c2c->next)
	{
		struct icecandinfo_s *lc;
		if (c2c->method == ICEM_ICE)
		{
			char bah[2];
			piceapi->ICE_Get(c2c->ice, "newlc", bah, sizeof(bah));
			if (atoi(bah))
			{
				Con_DPrintf("Sending updated local addresses\n");
				JCL_JingleSend(jcl, c2c, "transport-info");
			}
		}
	}
}

void JCL_Join(jclient_t *jcl, char *target, char *sid, qboolean allow, int protocol)
{
	struct c2c_s *c2c = NULL, **link;
	xmltree_t *jingle;
	struct icestate_s *ice;
	char autotarget[256];
	if (!jcl)
		return;

	if (!strchr(target, '/'))
	{	
		buddy_t *b;
		bresource_t *br;
		JCL_FindBuddy(jcl, target, &b, &br);
		if (!br)
			br = b->defaultresource;
		if (!br)
			br = b->resources;

		if (!br)
		{
			Con_Printf("User name not valid\n");
			return;
		}
		Q_snprintf(autotarget, sizeof(autotarget), "%s/%s", b->accountdomain, br->resource);
		target = autotarget;
	}

	for (link = &jcl->c2c; *link; link = &(*link)->next)
	{
		if (!strcmp((*link)->with, target) && (!sid || !strcmp((*link)->sid, sid)) && ((*link)->mediatype == protocol || protocol == ICEP_INVALID))
		{
			c2c = *link;
			break;
		}
	}
	if (allow)
	{
		if (!c2c)
		{
			if (!sid)
			{
				c2c = JCL_JingleCreateSession(jcl, target, true, sid, DEFAULTICEMODE, ((protocol == ICEP_INVALID)?ICEP_QWCLIENT:protocol));
				JCL_JingleSend(jcl, c2c, "session-initiate");

				Con_Printf("%s ^[[%s]\\xmpp\\%s^] ^[[Hang Up]\\xmppact\\jdeny\\xmpp\\%s\\xmppsid\\%s^].\n", protocol==ICEP_VOICE?"Calling":"Requesting session with", target, target, target, c2c->sid);
			}
			else
				Con_Printf("That session has expired.\n");
		}
		else if (c2c->creator)
		{
			//resend initiate if they've not acked it... I dunno...
			JCL_JingleSend(jcl, c2c, "session-initiate");
			Con_Printf("Restarting session with ^[[%s]\\xmpp\\%s^].\n", target, target);
		}
		else if (c2c->accepted)
			Con_Printf("That session was already accepted.\n");
		else
		{
			JCL_JingleSend(jcl, c2c, "session-accept");
			Con_Printf("Accepting session from ^[[%s]\\xmpp\\%s^].\n", target, target);
		}
	}
	else
	{
		if (c2c)
		{
			JCL_JingleSend(jcl, c2c, "session-terminate");
			Con_Printf("Terminating session with ^[[%s]\\xmpp\\%s^].\n", target, target);
		}
		else
			Con_Printf("That session has already expired.\n");
	}
}

void JCL_JingleParsePeerPorts(jclient_t *jcl, struct c2c_s *c2c, xmltree_t *inj, char *from)
{
	xmltree_t *incontent = XML_ChildOfTree(inj, "content", 0);
	xmltree_t *intransport = XML_ChildOfTree(incontent, "transport", 0);
	xmltree_t *incandidate;
	struct icecandinfo_s rem;
	int i;

	if (strcmp(c2c->with, from) || strcmp(c2c->sid, XML_GetParameter(inj, "sid", "")))
	{
		Con_Printf("%s is trying to mess with our connections...\n", from);
		return;
	}

	if (!c2c->sid)
		return;

	if (!intransport)
		return;

	if (!c2c->ice)
		return;

	piceapi->ICE_Set(c2c->ice, "rufrag", XML_GetParameter(intransport, "ufrag", ""));
	piceapi->ICE_Set(c2c->ice, "rpwd", XML_GetParameter(intransport, "pwd", ""));

	for (i = 0; (incandidate = XML_ChildOfTree(intransport, "candidate", i)); i++)
	{
		char *s;
		memset(&rem, 0, sizeof(rem));
		Q_strlcpy(rem.addr, XML_GetParameter(incandidate, "ip", ""), sizeof(rem.addr));
		Q_strlcpy(rem.candidateid, XML_GetParameter(incandidate, "id", ""), sizeof(rem.candidateid));

		s = XML_GetParameter(incandidate, "type", "");
		if (s && !strcmp(s, "srflx"))
			rem.type = ICE_SRFLX;
		else if (s && !strcmp(s, "prflx"))
			rem.type = ICE_PRFLX;
		else if (s && !strcmp(s, "relay"))
			rem.type = ICE_RELAY;
		else
			rem.type = ICE_HOST;
		rem.port = atoi(XML_GetParameter(incandidate, "port", "0"));
		rem.priority = atoi(XML_GetParameter(incandidate, "priority", "0"));
		rem.network = atoi(XML_GetParameter(incandidate, "network", "0"));
		rem.generation = atoi(XML_GetParameter(incandidate, "generation", "0"));
		rem.foundation = atoi(XML_GetParameter(incandidate, "foundation", "0"));
		rem.component = atoi(XML_GetParameter(incandidate, "component", "0"));
		s = XML_GetParameter(incandidate, "protocol", "udp");
		if (s && !strcmp(s, "udp"))
			rem.transport = 0;
		else
			rem.transport = 0;
		piceapi->ICE_AddRCandidateInfo(c2c->ice, &rem);
	}
}
qboolean JCL_JingleHandleInitiate(jclient_t *jcl, xmltree_t *inj, char *from)
{
/*inj contains something like:
<jingle sid='purplea84196dc' responder='me@example.com/Quake' initiator='them@example.com/Quake' action='session-initiate' xmlns='urn:xmpp:jingle:1'>
  <content senders='both' name='audio-session' creator='initiator'>
    <transport ufrag='SES2' pwd='3XkwverVxJLy2lRXD1lOAb' xmlns='urn:xmpp:jingle:transports:ice-udp:1'>
      <candidate type='host' protocol='udp' priority='2013266431' port='53177' network='0' ip='192.168.0.182' id='purplea84196de' generation='0' foundation='1' component='1'/>
      <candidate type='host' protocol='udp' priority='2013266430' port='36480' network='0' ip='192.168.0.182' id='purplea84196dd' generation='0' foundation='1' component='2'/>
    </transport>
    <description media='audio' xmlns='urn:xmpp:jingle:apps:rtp:1'>
      <payload-type channels='1' clockrate='8000' id='104' name='SPEEX'/>
      <payload-type channels='1' clockrate='16000' id='103' name='SPEEX'/>
    </description>
  </content>
</jingle>
*/

	xmltree_t *incontent = XML_ChildOfTree(inj, "content", 0);
	xmltree_t *intransport = XML_ChildOfTree(incontent, "transport", 0);
	xmltree_t *indescription = XML_ChildOfTree(incontent, "description", 0);
	char *transportxmlns = intransport?intransport->xmlns:"";
	char *descriptionxmlns = indescription?indescription->xmlns:"";
	char *descriptionmedia = XML_GetParameter(indescription, "media", "");
	char *sid = XML_GetParameter(inj, "sid", "");

	xmltree_t *jingle;
	struct icestate_s *ice;
	qboolean accepted = false;
	enum icemode_e imode;
	char *response = "session-terminate";
	char *offer = "pwn you";
	char *autocvar = "xmpp_autoaccepthax";
	char *initiator;

	struct c2c_s *c2c = NULL;
	int mt = ICEP_INVALID;

	//FIXME: add support for session forwarding so that we might forward the connection to the real server. for now we just reject it.
	initiator = XML_GetParameter(inj, "initiator", "");
	if (strcmp(initiator, from))
		return false;

	if (incontent && !strcmp(descriptionmedia, QUAKEMEDIATYPE) && !strcmp(descriptionxmlns, QUAKEMEDIAXMLNS))
	{
		char *host = XML_GetParameter(indescription, "host", "you");
		if (!strcmp(host, "you"))
		{
			mt = ICEP_QWSERVER;
			offer = "join your game";
			autocvar = "xmpp_autoacceptjoins";
		}
		else if (!strcmp(host, "me"))
		{
			mt = ICEP_QWCLIENT;
			offer = "invite you to thier game";
			autocvar = "xmpp_autoacceptinvites";
		}
	}
	if (incontent && !strcmp(descriptionmedia, "audio") && !strcmp(descriptionxmlns, "urn:xmpp:jingle:apps:rtp:1"))
	{
		mt = ICEP_VOICE;
		offer = "have a natter with you";
		autocvar = "xmpp_autoacceptvoice";
	}
	if (mt == ICEP_INVALID)
		return false;

	//FIXME: if both people try to establish a connection to the other simultaneously, the higher session id is meant to be canceled, and the lower accepted automagically.

	c2c = JCL_JingleCreateSession(jcl, from, false,
		sid,
		strcmp(transportxmlns, "urn:xmpp:jingle:transports:raw-udp:1")?ICEM_ICE:ICEM_RAW,
		mt
		);
	if (!c2c)
		return false;

	if (c2c->mediatype == ICEP_VOICE)
	{
		qboolean okay = false;
		int i = 0;
		xmltree_t *payload;
		//chuck it at the engine and see what sticks. at least one must...
		while((payload = XML_ChildOfTree(indescription, "payload-type", i++)))
		{
			char *name = XML_GetParameter(payload, "name", "");
			char *clock = XML_GetParameter(payload, "clockrate", "");
			char *id = XML_GetParameter(payload, "id", "");
			char parm[64];
			char val[64];
			if (!strcmp(name, "SPEEX"))
			{
				Q_snprintf(parm, sizeof(parm), "codec%i", atoi(id));
				Q_snprintf(val, sizeof(val), "speex@%i", atoi(clock));
				okay |= piceapi->ICE_Set(c2c->ice, parm, val);
			}
			else if (!strcmp(name, "OPUS"))
			{
				Q_snprintf(parm, sizeof(parm), "codec%i", atoi(id));
				okay |= piceapi->ICE_Set(c2c->ice, parm, "opus");
			}
		}
		//don't do it if we couldn't successfully set any codecs, because the engine doesn't support the ones that were listed, or something.
		if (!okay)
		{
			JCL_JingleSend(jcl, c2c, "session-terminate");
			return false;
		}
	}

	JCL_JingleParsePeerPorts(jcl, c2c, inj, from);

	if (c2c->mediatype != ICEP_INVALID)
	{
		if (!pCvar_GetFloat(autocvar))
		{
			//show a prompt for it, send the reply when the user decides.
			Con_Printf(
					"^[[%s]\\xmpp\\%s^] wants to %s. "
					"^[[Authorise]\\xmppact\\jauth\\xmpp\\%s\\xmppsid\\%s^] "
					"^[[Deny]\\xmppact\\jdeny\\xmpp\\%s\\xmppsid\\%s^]\n",
				from, from, 
				offer,
				from, sid,
				from, sid);
			return true;
		}
		else
		{
			Con_Printf("Auto-accepting session from ^[[%s]\\xmpp\\%s^]\n", from, from);
			response = "session-accept";
		}
	}

	JCL_JingleSend(jcl, c2c, response);
	return true;
}

qboolean JCL_ParseJingle(jclient_t *jcl, xmltree_t *tree, char *from, char *id)
{
	char *action = XML_GetParameter(tree, "action", "");
	char *sid = XML_GetParameter(tree, "sid", "");

	struct c2c_s *c2c = NULL, **link;

	for (link = &jcl->c2c; *link; link = &(*link)->next)
	{
		if (!strcmp((*link)->sid, sid))
		{
			c2c = *link;
			if (!c2c->accepted)
				break;
		}
	}

	//validate sender
	if (c2c && strcmp(c2c->with, from))
	{
		Con_Printf("%s is trying to mess with our connections...\n", from);
		return false;
	}

	//FIXME: transport-info, transport-replace
	if (!strcmp(action, "session-terminate"))
	{
		xmltree_t *reason = XML_ChildOfTree(tree, "reason", 0);
		if (!c2c)
		{
			Con_Printf("Received session-terminate without an active session\n");
			return false;
		}

		if (reason && reason->child)
			Con_Printf("Session ended: %s\n", reason->child->name);
		else
			Con_Printf("Session ended\n");

		//unlink it
		for (link = &jcl->c2c; *link; link = &(*link)->next)
		{
			if (*link == c2c)
			{
				*link = c2c->next;
				break;
			}
		}

//		XML_ConPrintTree(tree, 0);

		if (c2c->ice)
			piceapi->ICE_Close(c2c->ice);
		free(c2c);
	}
	//content-accept
	//content-add
	//content-modify
	//content-reject
	//content-remove
	//description-info
	//security-info
	//
	else if (!strcmp(action, "transport-info"))
	{	//peer wants to add ports.
		if (c2c)
			JCL_JingleParsePeerPorts(jcl, c2c, tree, from);
		else
			Con_DPrintf("Received transport-info without an active session\n");
	}
//FIXME: we need to add support for this to downgrade to raw if someone tries calling through a SIP gateway
	else if (!strcmp(action, "transport-replace"))
	{
		if (c2c)
		{
			if (1)
				JCL_JingleSend(jcl, c2c, "transport-reject");
			else
			{
				JCL_JingleParsePeerPorts(jcl, c2c, tree, from);
				JCL_JingleSend(jcl, c2c, "transport-accept");
			}
		}
	}
	else if (!strcmp(action, "transport-reject"))
	{
		JCL_JingleSend(jcl, c2c, "session-terminate");
	}
	else if (!strcmp(action, "session-accept"))
	{
		if (!c2c)
		{
			Con_DPrintf("Unknown session acceptance\n");
			return false;
		}
		else if (!c2c->creator)
		{
			Con_DPrintf("Peer tried to accept a session that *they* created!\n");
			return false;
		}
		else if (c2c->accepted)
		{
			//pidgin is buggy and can dupe-accept sessions multiple times.
			Con_DPrintf("Duplicate session-accept from peer.\n");

			//XML_ConPrintTree(tree, 0);
			return false;
		}
		else
		{
			char *responder = XML_GetParameter(tree, "responder", from);
			if (strcmp(responder, from))
			{
				return false;
			}
			Con_Printf("Session Accepted!\n");
//			XML_ConPrintTree(tree, 0);

			JCL_JingleParsePeerPorts(jcl, c2c, tree, from);
			c2c->accepted = true;

			//if we didn't error out, the ICE stuff is meant to start sending handshakes/media as soon as the connection is accepted
			if (c2c->ice)
				piceapi->ICE_Set(c2c->ice, "state", STRINGIFY(ICE_CONNECTING));
		}
	}
	else if (!strcmp(action, "session-initiate"))
	{
//		Con_Printf("Peer initiating connection!\n");
//		XML_ConPrintTree(tree, 0);

		if (!JCL_JingleHandleInitiate(jcl, tree, from))
			return false;
	}
	else
	{
		Con_Printf("Unknown jingle action: %s\n", action);
//		XML_ConPrintTree(tree, 0);
	}

	JCL_AddClientMessagef(jcl,
		"<iq type='result' to='%s' id='%s' />", from, id);
	return true;
}


qintptr_t JCL_ConsoleLink(qintptr_t *args)
{
	char text[256];
	char link[256];
	char who[256];
	char what[256];
//	pCmd_Argv(0, text, sizeof(text));
	pCmd_Argv(1, link, sizeof(link));

	JCL_Info_ValueForKey(link, "xmpp", who, sizeof(who));
	JCL_Info_ValueForKey(link, "xmppact", what, sizeof(what));

	if (!*who && !*what)
		return false;

	if (!strcmp(what, "pauth"))
	{
		//we should friend them too.
		if (jclient && jclient->status == JCL_ACTIVE)
			JCL_AddClientMessagef(jclient, "<presence to='%s' type='subscribed'/>", who);
		return true;
	}
	else if (!strcmp(what, "pdeny"))
	{
		if (jclient && jclient->status == JCL_ACTIVE)
			JCL_AddClientMessagef(jclient, "<presence to='%s' type='unsubscribed'/>", who);
		return true;
	}
	else if (!strcmp(what, "jauth"))
	{
		JCL_Info_ValueForKey(link, "xmppsid", what, sizeof(what));
		if (jclient && jclient->status == JCL_ACTIVE)
			JCL_Join(jclient, who, what, true, ICEP_INVALID);
		return true;
	}
	else if (!strcmp(what, "jdeny"))
	{
		JCL_Info_ValueForKey(link, "xmppsid", what, sizeof(what));
		if (jclient && jclient->status == JCL_ACTIVE)
			JCL_Join(jclient, who, what, false, ICEP_INVALID);
		return true;
	}
	else if (!strcmp(what, "join"))
	{
		if (jclient && jclient->status == JCL_ACTIVE)
			JCL_Join(jclient, who, NULL, true, ICEP_QWCLIENT);
		return true;
	}
	else if (!strcmp(what, "invite"))
	{
		if (jclient && jclient->status == JCL_ACTIVE)
			JCL_Join(jclient, who, NULL, true, ICEP_QWSERVER);
		return true;
	}
	else if (!strcmp(what, "call"))
	{
		if (jclient && jclient->status == JCL_ACTIVE)
			JCL_Join(jclient, who, NULL, true, ICEP_VOICE);
		return true;
	}
	else if ((*who && !*what) || !strcmp(what, "msg"))
	{
		if (jclient)
		{
			char *f;
			buddy_t *b;
			bresource_t *br;

			JCL_FindBuddy(jclient, *who?who:jclient->defaultdest, &b, &br);
			f = b->name;
			b->defaultresource = br;

			if (BUILTINISVALID(Con_SubPrint))
				pCon_SubPrint(f, "");
			if (BUILTINISVALID(Con_SetActive))
				pCon_SetActive(f);
		}
		return true;
	}
	else
	{
		Con_Printf("Unsupported xmpp action (%s) in link\n", what);
	}

	return false;
}

qintptr_t JCL_ConExecuteCommand(qintptr_t *args)
{
	buddy_t *b;
	char consolename[256];
	if (!jclient)
	{
		char buffer[256];
		pCmd_Argv(0, buffer, sizeof(buffer));
		Con_SubPrintf(buffer, "You were disconnected\n");
		return true;
	}
	pCmd_Argv(0, consolename, sizeof(consolename));
	for (b = jclient->buddies; b; b = b->next)
	{
		if (!strcmp(b->name, consolename))
		{
			if (b->defaultresource)
				Q_snprintf(jclient->defaultdest, sizeof(jclient->defaultdest), "%s/%s", b->accountdomain, b->defaultresource->resource);
			else
				Q_snprintf(jclient->defaultdest, sizeof(jclient->defaultdest), "%s", b->accountdomain);
			break;
		}
	}
	JCL_Command(consolename);
	return true;
}

void JCL_FlushOutgoing(jclient_t *jcl)
{
	int sent;
	if (!jcl || !jcl->outbuflen)
		return;

	sent = pNet_Send(jcl->socket, jcl->outbuf + jcl->outbufpos, jcl->outbuflen);	//FIXME: This needs rewriting to cope with errors.
	if (sent > 0)
	{
		//and print it on some subconsole if we're debugging
		if (jcl->streamdebug)
		{
			char t = jcl->outbuf[jcl->outbufpos+sent];
			jcl->outbuf[jcl->outbufpos+sent] = 0;
			Con_SubPrintf("xmppout", COLOURYELLOW "%s\n", jcl->outbuf + jcl->outbufpos);
			jcl->outbuf[jcl->outbufpos+sent] = t;
		}

		jcl->outbufpos += sent;
		jcl->outbuflen -= sent;
	}
//	else
//		Con_Printf("Unable to send anything\n");
}
void JCL_AddClientMessage(jclient_t *jcl, char *msg, int datalen)
{
	//handle overflows
	if (jcl->outbufpos+jcl->outbuflen+datalen > jcl->outbufmax)
	{
		if (jcl->outbuflen+datalen <= jcl->outbufmax)
		{
			//can get away with just moving the data
			memmove(jcl->outbuf, jcl->outbuf + jcl->outbufpos, jcl->outbuflen);
			jcl->outbufpos = 0;
		}
		else
		{
			//need to expand the buffer.
			int newmax = (jcl->outbuflen+datalen)*2;
			char *newbuf;

			if (newmax < jcl->outbuflen)
				newbuf = NULL;	//eep... some special kind of evil overflow.
			else
				newbuf = malloc(newmax+1);

			if (newbuf)
			{
				memcpy(newbuf, jcl->outbuf + jcl->outbufpos, jcl->outbuflen);
				jcl->outbufmax = newmax;
				jcl->outbufpos = 0;
				jcl->outbuf = newbuf;
			}
			else
				datalen = 0;	//eep!
		}
	}
	//and write our data to it
	memcpy(jcl->outbuf + jcl->outbufpos + jcl->outbuflen, msg, datalen);
	jcl->outbuflen += datalen;

	//try and flush it now
	JCL_FlushOutgoing(jcl);
}
void JCL_AddClientMessageString(jclient_t *jcl, char *msg)
{
	JCL_AddClientMessage(jcl, msg, strlen(msg));
}
void JCL_AddClientMessagef(jclient_t *jcl, char *fmt, ...)
{
	va_list		argptr;
	char body[2048];

	va_start (argptr, fmt);
	Q_vsnprintf (body, sizeof(body), fmt, argptr);
	va_end (argptr);

	JCL_AddClientMessageString(jcl, body);
}
qboolean JCL_Reconnect(jclient_t *jcl)
{
	//destroy any data that never got sent
	free(jcl->outbuf);
	jcl->outbuf = NULL;
	jcl->outbuflen = 0;
	jcl->outbufpos = 0;
	jcl->outbufmax = 0;
	jcl->instreampos = 0;
	jcl->bufferedinammount = 0;
	jcl->tagdepth = 0;
	Q_strlcpy(jcl->localalias, ">>", sizeof(jcl->localalias));


	Con_Printf("XMPP: Trying to connect to %s\n", jcl->domain);
	jcl->socket = pNet_TCPConnect(jcl->server, jcl->tlsconnect?5223:5222);	//port is only used if the url doesn't contain one. It's a default.

	//not yet blocking. So no frequent attempts please...
	//non blocking prevents connect from returning worthwhile sensible value.
	if ((int)jcl->socket < 0)
	{
		Con_Printf("JCL_OpenSocket: couldn't connect\n");
		return false;
	}

	jcl->issecure = false;
	if (jcl->tlsconnect)
		if (pNet_SetTLSClient(jcl->socket, jcl->server)>=0)
			jcl->issecure = true;

	jcl->status = JCL_AUTHING;

	JCL_AddClientMessageString(jcl,
		"<?xml version='1.0' ?>"
		"<stream:stream to='");
	JCL_AddClientMessageString(jcl, jcl->domain);
	JCL_AddClientMessageString(jcl, "' xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams' version='1.0'>");

	return true;
}
jclient_t *JCL_Connect(char *server, qboolean usesecure, char *account, char *password)
{
	char gamename[64];
	jclient_t *jcl;
	char *domain;
	char *res;

	res = TrimResourceFromJid(account);
	if (!res)
	{
		//the default resource matches the game that they're trying to play.
		if (pCvar_GetString("fs_gamename", gamename, sizeof(gamename)))
		{
			//strip out any weird chars (namely whitespace)
			char *o;
			for (o = gamename, res = gamename; *res; )
			{
				if (*res == ' ' || *res == '\t')
					res++;
				else
					*o++ = *res++;
			}
			res = gamename;
		}
	}

	if (usesecure)
	{
		if (!BUILTINISVALID(Net_SetTLSClient))
		{
			Con_Printf("XMPP: TLS is not supported\n");
			return NULL;
		}
	}

	domain = strchr(account, '@');
	if (domain)
	{
		*domain = '\0';
		domain++;
	}
	else
	{
		domain = DEFAULTDOMAIN;
		if (domain && *domain)
			Con_Printf("XMPP: domain not specified, assuming %s\n", domain);
		else
		{
			Con_Printf("XMPP: domain not specified\n");
			return NULL;
		}
	}

	jcl = malloc(sizeof(jclient_t));
	if (!jcl)
		return NULL;

	memset(jcl, 0, sizeof(jclient_t));

	jcl->tlsconnect = usesecure;
	jcl->streamdebug = !!pCvar_GetFloat("xmpp_debug");

	Q_strlcpy(jcl->server, server, sizeof(jcl->server));
	Q_strlcpy(jcl->username, account, sizeof(jcl->username));
	Q_strlcpy(jcl->domain, domain, sizeof(jcl->domain));
	Q_strlcpy(jcl->password, password, sizeof(jcl->password));
	Q_strlcpy(jcl->resource, (res&&*res)?res:"FTE", sizeof(jcl->password));

	if (!JCL_Reconnect(jcl))
	{
		free(jcl);
		jcl = NULL;
	}

	return jcl;
}

char base64[512+1];
unsigned int base64_len;	//current output length
unsigned int base64_cur;	//current pending value
unsigned int base64_bits;//current pending bits
char Base64_From64(int byt)
{
	if (byt >= 0 && byt < 26)
		return 'A' + byt - 0;
	if (byt >= 26 && byt < 52)
		return 'a' + byt - 26;
	if (byt >= 52 && byt < 62)
		return '0' + byt - 52;
	if (byt == 62)
		return '+';
	if (byt == 63)
		return '/';
	return '!';
}
void Base64_Byte(unsigned int byt)
{
	if (base64_len+8>=sizeof(base64)-1)
		return;
	base64_cur |= byt<<(16-	base64_bits);//first byte fills highest bits
	base64_bits += 8;
	if (base64_bits == 24)
	{
		base64[base64_len++] = Base64_From64((base64_cur>>18)&63);
		base64[base64_len++] = Base64_From64((base64_cur>>12)&63);
		base64[base64_len++] = Base64_From64((base64_cur>>6)&63);
		base64[base64_len++] = Base64_From64((base64_cur>>0)&63);
		base64[base64_len] = '\0';
//		Con_Printf("base64: %s\n", base64+base64_len-4);
		base64_bits = 0;
		base64_cur = 0;
	}
}

void Base64_Add(char *s, int len)
{
	unsigned char *us = (unsigned char *)s;
	while(len-->0)
		Base64_Byte(*us++);
}

void Base64_Finish(void)
{
	//output is always a multiple of four

	//0(0)->0(0)
	//1(8)->2(12)
	//2(16)->3(18)
	//3(24)->4(24)

	if (base64_bits != 0)
	{
		base64[base64_len++]=Base64_From64((base64_cur>>18)&63);
		base64[base64_len++]=Base64_From64((base64_cur>>12)&63);
		if (base64_bits == 8)
		{
			base64[base64_len++]= '=';
			base64[base64_len++]= '=';
		}
		else
		{
			base64[base64_len++]=Base64_From64((base64_cur>>6)&63);
			if (base64_bits == 16)
				base64[base64_len++]= '=';
			else
				base64[base64_len++]=Base64_From64((base64_cur>>0)&63);
		}
	}
	base64[base64_len++] = '\0';

	base64_len = 0; //for next time (use strlen)
	base64_bits = 0;
	base64_cur = 0;
}

//decode a base64 byte to a 0-63 value. Cannot cope with =.
static int Base64_DecodeByte(char byt)
{
    if (byt >= 'A' && byt <= 'Z')
        return (byt-'A') + 0;
    if (byt >= 'a' && byt <= 'z')
        return (byt-'a') + 26;
    if (byt >= '0' && byt <= '9')
        return (byt-'0') + 52;
    if (byt == '+')
        return 62;
    if (byt == '/')
        return 63;
    return -1;
}
int Base64_Decode(char *out, int outlen, char *src, int srclen)
{
	int len = 0;
	int result;

	//4 input chars give 3 output chars
	while(srclen >= 4)
	{
		if (len+3 > outlen)
			break;
		result = Base64_DecodeByte(src[0])<<18;
		result |= Base64_DecodeByte(src[1])<<12;
		out[len++] = (result>>16)&0xff;
		if (src[2] != '=')
		{
			result |= Base64_DecodeByte(src[2])<<6;
			out[len++] = (result>>8)&0xff;
			if (src[3] != '=')
			{
				result |= Base64_DecodeByte(src[3])<<0;
				out[len++] = (result>>0)&0xff;
			}
		}
		if (result & 0xff000000)
			return 0;	//some kind of invalid char

		src += 4;
		srclen -= 4;
	}

	//some kind of error
	if (srclen)
		return 0;
	
	return len;
}

void JCL_ForgetBuddyResource(jclient_t *jcl, buddy_t *buddy, bresource_t *bres)
{
	bresource_t **link;
	bresource_t *r;
	for (link = &buddy->resources; *link; )
	{
		r = *link;
		if (!bres || bres == r)
		{
			*link = r->next;
			free(r);
			if (bres)
				break;
		}
		else
			link = &r->next;
	}
}
void JCL_ForgetBuddy(jclient_t *jcl, buddy_t *buddy, bresource_t *bres)
{
	buddy_t **link;
	buddy_t *b;
	for (link = &jcl->buddies; *link; )
	{
		b = *link;
		if (!buddy || buddy == b)
		{
			*link = b->next;
			JCL_ForgetBuddyResource(jcl, b, bres);
			free(b);
			if (buddy)
				break;
		}
		else
			link = &b->next;
	}
}

qboolean JCL_FindBuddy(jclient_t *jcl, char *jid, buddy_t **buddy, bresource_t **bres)
{
	char name[256];
	char *res;
	buddy_t *b;
	bresource_t *r = NULL;

	Q_strlcpy(name, jid, sizeof(name));
	res = TrimResourceFromJid(name);

	for (b = jcl->buddies; b; b = b->next)
	{
		if (!strcmp(b->accountdomain, name))
			break;
	}
	if (!b)
	{
		b = malloc(sizeof(*b) + strlen(name));
		memset(b, 0, sizeof(*b));
		b->next = jcl->buddies;
		jcl->buddies = b;
		strcpy(b->accountdomain, name);
		Q_strlcpy(b->name, name, sizeof(b->name));	//default
	}
	*buddy = b;
	if (res && bres)
	{
		for (r = b->resources; r; r = r->next)
		{
			if (!strcmp(r->resource, res))
				break;
		}
		if (!r)
		{
			r = malloc(sizeof(*r) + strlen(res));
			memset(r, 0, sizeof(*r));
			r->next = b->resources;
			b->resources = r;
			strcpy(r->resource, res);
		}
		*bres = r;
	}
	else if (bres)
		*bres = NULL;
	return false;
}

struct iq_s *JCL_SendIQ(jclient_t *jcl, qboolean (*callback) (jclient_t *jcl, xmltree_t *tree, struct iq_s *iq), char *iqtype, char *target, char *body)
{
	struct iq_s *iq;
		
	iq = malloc(sizeof(*iq));
	iq->next = jcl->pendingiqs;
	jcl->pendingiqs = iq;
	Q_snprintf(iq->id, sizeof(iq->id), "%i", rand());
	iq->callback = callback;

	if (target)
	{
		if (*jcl->jid)
			JCL_AddClientMessagef(jcl, "<iq type='%s' id='%s' from='%s' to='%s'>", iqtype, iq->id, jcl->jid, target);
		else
			JCL_AddClientMessagef(jcl, "<iq type='%s' id='%s' to='%s'>", iqtype, iq->id, target);
	}
	else
	{
		if (*jcl->jid)
			JCL_AddClientMessagef(jcl, "<iq type='%s' id='%s' from='%s'>", iqtype, iq->id, jcl->jid);
		else
			JCL_AddClientMessagef(jcl, "<iq type='%s' id='%s'>", iqtype, iq->id);
	}
	JCL_AddClientMessageString(jcl, body);
	JCL_AddClientMessageString(jcl, "</iq>");
	return iq;
}
struct iq_s *JCL_SendIQf(jclient_t *jcl, qboolean (*callback) (jclient_t *jcl, xmltree_t *tree, struct iq_s *iq), char *iqtype, char *target, char *fmt, ...)
{
	va_list		argptr;
	char body[2048];

	va_start (argptr, fmt);
	Q_vsnprintf (body, sizeof(body), fmt, argptr);
	va_end (argptr);

	return JCL_SendIQ(jcl, callback, iqtype, target, body);
}
struct iq_s *JCL_SendIQNode(jclient_t *jcl, qboolean (*callback) (jclient_t *jcl, xmltree_t *tree, struct iq_s *iq), char *iqtype, char *target, xmltree_t *node, qboolean destroynode)
{
	struct iq_s *n;
	char *s = XML_GenerateString(node);
	n = JCL_SendIQ(jcl, callback, iqtype, target, s);
	free(s);
	if (destroynode)
		XML_Destroy(node);
	return n;
}
static void JCL_RosterUpdate(jclient_t *jcl, xmltree_t *listp)
{
	xmltree_t *i;
	buddy_t *buddy;
	int cnum = 0;
	while ((i = XML_ChildOfTree(listp, "item", cnum++)))
	{
		char *name = XML_GetParameter(i, "name", "");
		char *jid = XML_GetParameter(i, "jid", "");
//		char *sub = XML_GetParameter(i, "subscription", "");
		JCL_FindBuddy(jcl, jid, &buddy, NULL);

		if (*name)
			Q_strlcpy(buddy->name, name, sizeof(buddy->name));
		buddy->friended = true;
	}
}
static qboolean JCL_RosterReply(jclient_t *jcl, xmltree_t *tree, struct iq_s *iq)
{
	xmltree_t *c;
	//we're probably connected once we've had this reply.
	jcl->status = JCL_ACTIVE;
	JCL_GeneratePresence(jcl, true);
	c = XML_ChildOfTree(tree, "query", 0);
	if (c)
	{
		JCL_RosterUpdate(jcl, c);
		return true;
	}
	return false;
}

static qboolean JCL_BindReply(jclient_t *jcl, xmltree_t *tree, struct iq_s *iq)
{
	xmltree_t *c;
	c = XML_ChildOfTree(tree, "bind", 0);
	if (c)
	{
		c = XML_ChildOfTree(c, "jid", 0);
		if (c)
		{
			Q_strlcpy(jcl->jid, c->body, sizeof(jcl->jid));
			Con_Printf("Bound to jid ^[[%s]\\xmpp\\%s^]\n", jcl->jid, jcl->jid);
			return true;
		}
	}
	return false;
}
static qboolean JCL_VCardReply(jclient_t *jcl, xmltree_t *tree, struct iq_s *iq)
{
	xmltree_t *vc, *fn, *nickname;
	vc = XML_ChildOfTree(tree, "vCard", 0);
	fn = XML_ChildOfTree(vc, "FN", 0);
	nickname = XML_ChildOfTree(vc, "NICKNAME", 0);

	if (nickname && *nickname->body)
		Q_strlcpy(jcl->localalias, nickname->body, sizeof(jcl->localalias));
	else if (fn && *fn->body)
		Q_strlcpy(jcl->localalias, fn->body, sizeof(jcl->localalias));
	return true;
}
static qboolean JCL_SessionReply(jclient_t *jcl, xmltree_t *tree, struct iq_s *iq)
{
	JCL_SendIQf(jcl, JCL_RosterReply, "get", NULL, "<query xmlns='jabber:iq:roster'/>");
	JCL_SendIQf(jcl, JCL_VCardReply, "get", NULL, "<vCard xmlns='vcard-temp'/>");
	return true;
}


static char *caps[] =
{
#if 1
	"http://jabber.org/protocol/caps",
	"http://jabber.org/protocol/disco#info",
//	"http://jabber.org/protocol/disco#items",

//	"http://www.google.com/xmpp/protocol/camera/v1",
//	"http://www.google.com/xmpp/protocol/session",
//	"http://www.google.com/xmpp/protocol/voice/v1",
//	"http://www.google.com/xmpp/protocol/video/v1",

	"jabber:iq:version", 
	"urn:xmpp:jingle:1",
	QUAKEMEDIAXMLNS,
#ifdef VOIP
	"urn:xmpp:jingle:apps:rtp:1",
	"urn:xmpp:jingle:apps:rtp:audio",
#endif
//	"urn:xmpp:jingle:apps:rtp:video",//we don't support rtp video chat
	"urn:xmpp:jingle:transports:raw-udp:1",
#ifndef NOICE
	"urn:xmpp:jingle:transports:ice-udp:1",
#endif
#ifndef Q3_VM
	"urn:xmpp:time",
#endif
	"urn:xmpp:ping",	//FIXME: I'm not keen on this. I only added support to stop errors from pidgin when trying to debug.

#else
	//for testing, this is the list of features pidgin supports (which is the other client I'm testing against).

	"jabber:iq:last",
	"jabber:iq:oob",
	"urn:xmpp:time",
	"jabber:iq:version",
	"jabber:x:conference",
	"http://jabber.org/protocol/bytestreams",
	"http://jabber.org/protocol/caps",
	"http://jabber.org/protocol/chatstates",
	"http://jabber.org/protocol/disco#info",
	"http://jabber.org/protocol/disco#items",
	"http://jabber.org/protocol/muc",
	"http://jabber.org/protocol/muc#user",
	"http://jabber.org/protocol/si",
//	"http://jabber.org/protocol/si/profile/file-transfer",
	"http://jabber.org/protocol/xhtml-im",
	"urn:xmpp:ping",
	"urn:xmpp:attention:0",
	"urn:xmpp:bob",
	"urn:xmpp:jingle:1",
	"http://www.google.com/xmpp/protocol/session",
	"http://www.google.com/xmpp/protocol/voice/v1",
	"http://www.google.com/xmpp/protocol/video/v1",
	"http://www.google.com/xmpp/protocol/camera/v1",
	"urn:xmpp:jingle:apps:rtp:1",
	"urn:xmpp:jingle:apps:rtp:audio",
	"urn:xmpp:jingle:apps:rtp:video",
	"urn:xmpp:jingle:transports:raw-udp:1",
	"urn:xmpp:jingle:transports:ice-udp:1",
	"urn:xmpp:avatar:metadata",
	"urn:xmpp:avatar:data",
	"urn:xmpp:avatar:metadata+notify",
	"http://jabber.org/protocol/mood",
	"http://jabber.org/protocol/mood+notify",
	"http://jabber.org/protocol/tune",
	"http://jabber.org/protocol/tune+notify",
	"http://jabber.org/protocol/nick",
	"http://jabber.org/protocol/nick+notify",
	"http://jabber.org/protocol/ibb",
#endif
	NULL
};
static void buildcaps(char *out, int outlen)
{
	int i;
	Q_strncpyz(out, "<identity category='client' type='pc' name='FTEQW'/>", outlen);

	for (i = 0; caps[i]; i++)
	{
		Q_strlcat(out, "<feature var='", outlen);
		Q_strlcat(out, caps[i], outlen);
		Q_strlcat(out, "'/>", outlen);
	}
}
static int qsortcaps(const void *va, const void *vb)
{
	char *a = *(char**)va;
	char *b = *(char**)vb;
	return strcmp(a, b);
}
int SHA1(char *digest, int maxdigestsize, char *string, int stringlen);
char *buildcapshash(void)
{
	int i, l;
	char out[8192];
	int outlen = sizeof(out);
	unsigned char digest[64];
	Q_strlcpy(out, "client/pc//FTEQW<", outlen);
	qsort(caps, sizeof(caps)/sizeof(caps[0]) - 1, sizeof(char*), qsortcaps); 
	for (i = 0; caps[i]; i++)
	{
		Q_strlcat(out, caps[i], outlen);
		Q_strlcat(out, "<", outlen);
	}
	l = SHA1(digest, sizeof(digest), out, strlen(out));
	for (i = 0; i < l; i++)
		Base64_Byte(digest[i]);
	Base64_Finish();
	return base64;
}

void JCL_ParseIQ(jclient_t *jcl, xmltree_t *tree)
{
	qboolean unparsable = true;
	char *from;
//	char *to;
	char *id;
	char *f;
	xmltree_t *ot;

	//FIXME: block from people who we don't know.

	id = XML_GetParameter(tree, "id", "");
	from = XML_GetParameter(tree, "from", "");
//	to = XML_GetParameter(tree, "to", "");

	f = XML_GetParameter(tree, "type", "");
	if (!strcmp(f, "get"))
	{
		ot = XML_ChildOfTree(tree, "query", 0);
		if (ot)
		{
			if (from && !strcmp(ot->xmlns, "http://jabber.org/protocol/disco#info"))
			{	//http://xmpp.org/extensions/xep-0030.html
				char msg[2048];
				char *hash;
				unparsable = false;

				buildcaps(msg, sizeof(msg));
				hash = buildcapshash();

				JCL_AddClientMessagef(jcl,
						"<iq type='result' to='%s' id='%s'>"
							"<query xmlns='http://jabber.org/protocol/disco#info' node='http://fteqw.com/ftexmppplug#%s'>"
								"%s"
							"</query>"
						"</iq>", from, id, hash, msg);
			}
			else if (from && !strcmp(ot->xmlns, "jabber:iq:version"))
			{	//client->client version request
				char msg[2048];
				unparsable = false;

				Q_snprintf(msg, sizeof(msg),
						"<iq type='result' to='%s' id='%s'>"
							"<query xmlns='jabber:iq:version'>"
								"<name>FTEQW XMPP</name>"
								"<version>V"JCL_BUILD"</version>"
#ifdef Q3_VM
								"<os>QVM plugin</os>"
#else
								//don't specify the os otherwise, as it gives away required base addresses etc for exploits
#endif
							"</query>"
						"</iq>", from, id);

				JCL_AddClientMessageString(jcl, msg);
			}
			else if (from && !strcmp(ot->xmlns, "jabber:iq:last"))
			{
				unparsable = false;
				JCL_AddClientMessagef(jcl,
						"<iq type='error' to='%s' id='%s'>"
							"<error type='cancel'>"
								"<service-unavailable xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>"
							"</error>"
						"</iq>", from, id);
			}
/*			else if (from && !strcmp(ot->xmlns, "jabber:iq:last"))
			{	//http://xmpp.org/extensions/xep-0012.html
				char msg[2048];
				int idletime = 0;
				unparsable = false;

				//last activity
				Q_snprintf(msg, sizeof(msg),
						"<iq type='result' to='%s' id='%s'>"
							"<query xmlns='jabber:iq:last' seconds='%i'/>"
						"</iq>", from, id, idletime);
				
				JCL_AddClientMessageString(jcl, msg);
			}
*/
		}
#ifndef Q3_VM
		ot = XML_ChildOfTree(tree, "time", 0);
		if (ot && !strcmp(ot->xmlns, "urn:xmpp:time"))
		{	//http://xmpp.org/extensions/xep-0202.html
			char msg[2048];
			char tz[256];
			char timestamp[256];
			struct tm * timeinfo;
			int tzh, tzm;
			time_t rawtime;
			time (&rawtime);
			timeinfo = localtime(&rawtime);
			tzh = timeinfo->tm_hour;
			tzm = timeinfo->tm_min;
			timeinfo = gmtime (&rawtime);
			tzh -= timeinfo->tm_hour;
			tzm -= timeinfo->tm_min;
			Q_snprintf(tz, sizeof(tz), "%+i:%i", tzh, tzm);
			strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", timeinfo);
			unparsable = false;
			//strftime
			Q_snprintf(msg, sizeof(msg),
					"<iq type='result' to='%s' id='%s'>"
						"<time xmlns='urn:xmpp:time'>"
							"<tzo>%s</tzo>"
							"<utc>%s</utc>"
						"</time>"
					"</iq>", from, id, tz, timestamp);
			JCL_AddClientMessageString(jcl, msg);
		}
#endif

		ot = XML_ChildOfTree(tree, "ping", 0);
		if (ot && !strcmp(ot->xmlns, "urn:xmpp:ping"))
		{
			JCL_AddClientMessagef(jcl, "<iq type='result' to='%s' id='%s' />", from, id);
		}
			
		if (unparsable)
		{	//unsupported stuff
			char msg[2048];
			unparsable = false;

			Con_Printf("Unsupported iq get\n");
			XML_ConPrintTree(tree, 0);

			//tell them OH NOES, instead of requiring some timeout.
			Q_snprintf(msg, sizeof(msg),
					"<iq type='error' to='%s' id='%s'>"
						"<error type='cancel'>"
							"<service-unavailable xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>"
						"</error>"
					"</iq>", from, id);
			JCL_AddClientMessageString(jcl, msg);
		}
	}
	else if (!strcmp(f, "set"))
	{
		xmltree_t *c;
		
		c = XML_ChildOfTree(tree, "query", 0);
		if (c && !strcmp(c->xmlns, "jabber:iq:roster"))
		{
			unparsable = false;
			JCL_RosterUpdate(jcl, c);
		}

		c = XML_ChildOfTree(tree, "jingle", 0);
		if (c && !strcmp(c->xmlns, "urn:xmpp:jingle:1"))
		{
			unparsable = !JCL_ParseJingle(jcl, c, from, id);
		}

		if (unparsable)
		{
			char msg[2048];
			//tell them OH NOES, instead of requiring some timeout.
			Q_snprintf(msg, sizeof(msg),
					"<iq type='error' to='%s' id='%s'>"
						"<error type='cancel'>"
							"<service-unavailable xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>"
						"</error>"
					"</iq>", from, id);
			JCL_AddClientMessageString(jcl, msg);
			unparsable = false;
		}
	}
	else if (!strcmp(f, "result") || !strcmp(f, "error"))
	{
		char *id = XML_GetParameter(tree, "id", "");
		struct iq_s **link, *iq;
		unparsable = false;
		for (link = &jcl->pendingiqs; *link; link = &(*link)->next)
		{
			iq = *link;
			if (!strcmp(iq->id, id))
				break;
		}
		if (*link)
		{
			iq = *link;
			*link = iq->next;

			if (iq->callback)
			{
				if (!iq->callback(jcl, !strcmp(f, "error")?NULL:tree, iq))
				{
					Con_Printf("Invalid iq result\n");
					XML_ConPrintTree(tree, 0);
				}
			}
			free(iq);
		}
		else
		{
			Con_Printf("Unrecognised iq result\n");
			XML_ConPrintTree(tree, 0);
		}
	}
	
	if (unparsable)
	{
		unparsable = false;
		Con_Printf("Unrecognised iq type\n");
		XML_ConPrintTree(tree, 0);
	}
}
void JCL_ParseMessage(jclient_t *jcl, xmltree_t *tree)
{
	xmltree_t *ot;
	qboolean unparsable = true;
	char *f = XML_GetParameter(tree, "from", NULL);

	if (f && !strcmp(f, jcl->jid))
		unparsable = false;
	else
	{
		if (f)
		{
			buddy_t *b;
			bresource_t *br;
			Q_strlcpy(jcl->defaultdest, f, sizeof(jcl->defaultdest));

			JCL_FindBuddy(jcl, f, &b, &br);
			f = b->name;
			b->defaultresource = br;
		}

		if (f)
		{
			ot = XML_ChildOfTree(tree, "composing", 0);
			if (ot && !strcmp(ot->xmlns, "http://jabber.org/protocol/chatstates"))
			{
				unparsable = false;
				Con_SubPrintf(f, "%s is typing\r", f);
			}
			ot = XML_ChildOfTree(tree, "paused", 0);
			if (ot && !strcmp(ot->xmlns, "http://jabber.org/protocol/chatstates"))
			{
				unparsable = false;
				Con_SubPrintf(f, "%s has stopped typing\r", f);
			}
			ot = XML_ChildOfTree(tree, "inactive", 0);
			if (ot && !strcmp(ot->xmlns, "http://jabber.org/protocol/chatstates"))
			{
				unparsable = false;
				Con_SubPrintf(f, "\r", f);
			}
			ot = XML_ChildOfTree(tree, "active", 0);
			if (ot && !strcmp(ot->xmlns, "http://jabber.org/protocol/chatstates"))
			{
				unparsable = false;
				Con_SubPrintf(f, "\r", f);
			}
			ot = XML_ChildOfTree(tree, "gone", 0);
			if (ot && !strcmp(ot->xmlns, "http://jabber.org/protocol/chatstates"))
			{
				unparsable = false;
				Con_SubPrintf(f, "%s has gone away\r", f);
			}
		}

		ot = XML_ChildOfTree(tree, "body", 0);
		if (ot)
		{
			unparsable = false;
			if (f)
			{
				if (!strncmp(ot->body, "/me ", 4))
					Con_SubPrintf(f, "*^2%s^7%s\n", f, ot->body+3);
				else
					Con_SubPrintf(f, "^2%s^7: %s\n", f, ot->body);
			}
			else
				Con_Printf("NOTICE: %s\n", ot->body);

			if (BUILTINISVALID(LocalSound))
				pLocalSound("misc/talk.wav");
		}

		if (unparsable)
		{
			unparsable = false;
			if (jcl->streamdebug)
			{
				Con_Printf("Received a message without a body\n");
				XML_ConPrintTree(tree, 0);
			}
		}
	}
}

qboolean JCL_ClientDiscoInfo(jclient_t *jcl, xmltree_t *tree, struct iq_s *iq)
{
	xmltree_t *query = XML_ChildOfTree(tree, "query", 0);
	xmltree_t *feature;
	char *var;
	int i = 0;
	unsigned int caps = 0;
	qboolean rtp = false;
	qboolean rtpaudio = false;
	qboolean quake = false;
	qboolean ice = false;
	qboolean raw = false;
	qboolean jingle = false;
	buddy_t *b;
	bresource_t *r;
	JCL_FindBuddy(jcl, XML_GetParameter(tree, "from", ""), &b, &r);
	while((feature = XML_ChildOfTree(query, "feature", i++)))
	{
		var = XML_GetParameter(feature, "var", "");
		//check ones we recognise.
		if (!strcmp(var, QUAKEMEDIAXMLNS))
			quake = true;
		if (!strcmp(var, "urn:xmpp:jingle:apps:rtp:audio"))
			rtpaudio = true;
		if (!strcmp(var, "urn:xmpp:jingle:apps:rtp:1"))
			rtp = true;		//kinda implied, but ensures version is okay
		if (!strcmp(var, "urn:xmpp:jingle:transports:ice-udp:1"))
			ice = true;
		if (!strcmp(var, "urn:xmpp:jingle:transports:raw-udp:1"))
			raw = true;
		if (!strcmp(var, "urn:xmpp:jingle:1"))
			jingle = true;	//kinda implied, but ensures version is okay
	}
	if ((ice||raw) && jingle)
	{
		if (rtpaudio && rtp)
			caps |= CAP_VOICE;
		if (quake)
			caps |= CAP_INVITE;
	}

	if (b && r)
		r->caps = (r->caps & CAP_QUERIED) | caps;
	return true;
}
void JCL_ParsePresence(jclient_t *jcl, xmltree_t *tree)
{
	buddy_t *buddy;
	bresource_t *bres;

	char *from = XML_GetParameter(tree, "from", "");
	xmltree_t *show = XML_ChildOfTree(tree, "show", 0);
	xmltree_t *status = XML_ChildOfTree(tree, "status", 0);
	xmltree_t *quake = XML_ChildOfTree(tree, "quake", 0);
	char *type = XML_GetParameter(tree, "type", "");
	char *serverip = NULL;
	char *servermap = NULL;

	if (quake && !strcmp(quake->xmlns, "fteqw.com:game"))
	{
		serverip = XML_GetParameter(quake, "serverip", NULL);
		servermap = XML_GetParameter(quake, "servermap", NULL);
	}

	if (type && !strcmp(type, "subscribe"))
	{
		Con_Printf("^[[%s]\\xmpp\\%s^] wants to be your friend! ^[[Authorize]\\xmpp\\%s\\xmppact\\pauth^] ^[[Deny]\\xmpp\\%s\\xmppact\\pdeny^]\n", from, from, from, from);
	}
	else if (type && !strcmp(type, "subscribed"))
	{
		Con_Printf("^[[%s]\\xmpp\\%s^] is now your friend!\n", from, from, from, from);
	}
	else if (type && !strcmp(type, "unsubscribe"))
	{
		Con_Printf("^[[%s]\\xmpp\\%s^] has unfriended you\n", from, from);
	}
	else if (type && !strcmp(type, "unsubscribed"))
	{
		Con_Printf("^[[%s]\\xmpp\\%s^] is no longer unfriended you\n", from, from);
	}
	else
	{
		JCL_FindBuddy(jcl, from, &buddy, &bres);

		if (bres)
		{
			if (servermap)
			{
				bres->servertype = 2;
				Q_strlcpy(bres->server, servermap, sizeof(bres->server));
			}
			else if (serverip)
			{
				bres->servertype = 1;
				Q_strlcpy(bres->server, serverip, sizeof(bres->server));
			}
			else
			{
				bres->servertype = 0;
				Q_strlcpy(bres->server, "", sizeof(bres->server));
			}
			Q_strlcpy(bres->fstatus, (status && *status->body)?status->body:"", sizeof(bres->fstatus));
			if (!tree->child)
			{
				Q_strlcpy(bres->bstatus, "offline", sizeof(bres->bstatus));
				bres->caps = 0;
			}
			else
			{
				Q_strlcpy(bres->bstatus, (show && *show->body)?show->body:"present", sizeof(bres->bstatus));
				if (!(bres->caps & CAP_QUERIED))
				{
					bres->caps |= CAP_QUERIED;
					JCL_SendIQ(jcl, JCL_ClientDiscoInfo, "get", from, "<query xmlns='http://jabber.org/protocol/disco#info'/>");
				}
			}

			if (bres->servertype == 2)
				Con_Printf("^[[%s]\\xmpp\\%s^] is now ^[[Playing Quake - %s]\\xmpp\\%s\\xmppact\\join^]\n", buddy->name, from, bres->server, from);
			else if (bres->servertype == 1)
				Con_Printf("^[[%s]\\xmpp\\%s^] is now ^[[Playing Quake - %s]\\observe\\%s^]\n", buddy->name, from, bres->server, bres->server);
			else if (*bres->fstatus)
				Con_Printf("^[[%s]\\xmpp\\%s^] is now %s: %s\n", buddy->name, from, bres->bstatus, bres->fstatus);
			else
				Con_Printf("^[[%s]\\xmpp\\%s^] is now %s\n", buddy->name, from, bres->bstatus);

			if (!tree->child)
			{
				//remove this buddy resource
			}
		}
		else
		{
			Con_Printf("Weird presence:\n");
			XML_ConPrintTree(tree, 0);
		}
	}
}

#define JCL_DONE 0			//no more data available for now.
#define JCL_CONTINUE 1		//more data needs parsing.
#define JCL_KILL 2			//some error, needs reconnecting.
#define JCL_NUKEFROMORBIT 3	//permanent error (or logged on from elsewhere)
int JCL_ClientFrame(jclient_t *jcl)
{
	int pos;
	xmltree_t *tree, *ot;
	char *f;
	int ret;
	qboolean unparsable;

	int olddepth;

	ret = pNet_Recv(jcl->socket, jcl->bufferedinmessage+jcl->bufferedinammount, sizeof(jcl->bufferedinmessage)-1 - jcl->bufferedinammount);
	if (ret == 0)
	{
		if (!jcl->bufferedinammount)	//if we are half way through a message, read any possible conjunctions.
			return JCL_DONE;	//nothing more this frame
	}
	if (ret < 0)
	{
		Con_Printf("XMPP: socket error\n");
		return JCL_KILL;
	}

	if (ret>0)
	{
		jcl->bufferedinammount+=ret;
		jcl->bufferedinmessage[jcl->bufferedinammount] = 0;
	}

	olddepth = jcl->tagdepth;

	//we never end parsing in the middle of a < >
	//this means we can filter out the <? ?>, <!-- --> and < /> stuff properly
	for (pos = jcl->instreampos; pos < jcl->bufferedinammount; pos++)
	{
		if (jcl->bufferedinmessage[pos] == '<')
		{
			jcl->instreampos = pos;
		}
		else if (jcl->bufferedinmessage[pos] == '>')
		{
			if (pos < 1)
				break;	//erm...

			if (jcl->bufferedinmessage[pos-1] != '/')	//<blah/> is a tag without a body
			{
				if (jcl->bufferedinmessage[jcl->instreampos+1] != '?')	//<? blah ?> is a tag without a body
				{
					if (jcl->bufferedinmessage[pos-1] != '?')
					{
						if (jcl->bufferedinmessage[jcl->instreampos+1] == '/')	//</blah> is the end of a tag with a body
							jcl->tagdepth--;
						else
							jcl->tagdepth++;			//<blah> is the start of a tag with a body
					}
				}
			}

			jcl->instreampos=pos+1;
		}
	}

	if (jcl->tagdepth == 1 && olddepth == 0)
	{	//first bit of info

		pos = 0;
		tree = XML_Parse(jcl->bufferedinmessage, &pos, jcl->instreampos, true, "");
		while (tree && !strcmp(tree->name, "?xml"))
		{
			XML_Destroy(tree);
			tree = XML_Parse(jcl->bufferedinmessage, &pos, jcl->instreampos, true, "");
		}

		if (jcl->streamdebug)
		{
			char t = jcl->bufferedinmessage[pos];
			jcl->bufferedinmessage[pos] = 0;
			Con_TrySubPrint("xmppin", jcl->bufferedinmessage);
			if (tree)
				Con_TrySubPrint("xmppin", "\n");
			jcl->bufferedinmessage[pos] = t;
		}

		if (!tree)
		{
			Con_Printf("Not an xml stream\n");
			return JCL_KILL;
		}
		if (strcmp(tree->name, "stream") || strcmp(tree->xmlns, "http://etherx.jabber.org/streams"))
		{
			Con_Printf("Not an xmpp stream\n");
			return JCL_KILL;
		}
		Q_strlcpy(jcl->defaultnamespace, tree->xmlns_dflt, sizeof(jcl->defaultnamespace));

		ot = tree;
		tree = tree->child;
		ot->child = NULL;

//		Con_Printf("Discard\n");
//		XML_ConPrintTree(ot, 0);
		XML_Destroy(ot);

		if (!tree)
		{
			memmove(jcl->bufferedinmessage, jcl->bufferedinmessage+pos, jcl->bufferedinammount - (pos));
			jcl->bufferedinammount-=pos;
			jcl->instreampos-=pos;

			return JCL_DONE;
		}
	}
	else
	{
		if (jcl->tagdepth != 1)
		{
			if (jcl->tagdepth < 1 && jcl->bufferedinammount==jcl->instreampos)
			{
				Con_Printf("End of XML stream\n");
				return JCL_KILL;
			}
			return JCL_DONE;
		}

		pos = 0;
		tree = XML_Parse(jcl->bufferedinmessage, &pos, jcl->instreampos, false, jcl->defaultnamespace);

		if (jcl->streamdebug)
		{
			char t = jcl->bufferedinmessage[pos];
			jcl->bufferedinmessage[pos] = 0;
			Con_TrySubPrint("xmppin", jcl->bufferedinmessage);
			Con_TrySubPrint("xmppin", "\n");
			jcl->bufferedinmessage[pos] = t;
		}

		if (!tree)
		{
//			Con_Printf("No input tree: %s", jcl->bufferedinmessage);
			return JCL_DONE;
		}
	}

//	Con_Printf("read\n");
//	XML_ConPrintTree(tree, 0);

	jcl->timeout = jclient_curtime + 60*1000;

	unparsable = true;
	if (!strcmp(tree->name, "features"))
	{
		if ((ot=XML_ChildOfTree(tree, "bind", 0)))
		{
			unparsable = false;
			JCL_SendIQf(jcl, JCL_BindReply, "set", NULL, "<bind xmlns='urn:ietf:params:xml:ns:xmpp-bind'><resource>%s</resource></bind>", jcl->resource);
		}
		if ((ot=XML_ChildOfTree(tree, "session", 0)))
		{
			unparsable = false;
			JCL_SendIQf(jcl, JCL_SessionReply, "set", NULL, "<session xmlns='urn:ietf:params:xml:ns:xmpp-session'/>");
			jcl->connected = true;

			JCL_WriteConfig();

//			JCL_AddClientMessageString(jcl, "<iq type='get' to='gmail.com' id='H_2'><query xmlns='http://jabber.org/protocol/disco#info'/></iq>");
		}


		if (unparsable)
		{
			if ((!jclient->issecure) && BUILTINISVALID(Net_SetTLSClient) && XML_ChildOfTree(tree, "starttls", 0) != NULL && !pCvar_GetFloat("xmpp_disabletls"))
			{
				Con_Printf("Attempting to switch to TLS\n");
				JCL_AddClientMessageString(jcl, "<starttls xmlns='urn:ietf:params:xml:ns:xmpp-tls' />");
				unparsable = false;
			}
			else if ((ot=XML_ChildOfTree(tree, "mechanisms", 0)))
			{
				qboolean canplain = false;
//				qboolean canmd5 = false;
//				qboolean canscramsha1 = false;
//				qboolean canxoath2 = false;

				for(ot = ot->child; ot; ot = ot->sibling)
				{
					if (!strcmp(ot->body, "PLAIN"))
						canplain = true;
//					else if (!strcmp(ot->body, "SCRAM-SHA-1"))
//						cansha1 = true;
//					else if (!strcmp(ot->body, "DIGEST-MD5"))
//						canmd5 = true;
//					else if (!strcmp(ot->body, "X-OAUTH2"))
//						canxoath2 = true;
//					else
//						Con_Printf("Unknown auth method \'%s\'\n", ot->body);
				}
/*
				if (canscramsha1)
				{
					Con_Printf("Using scram-sha-1%s\n", jclient->issecure?" over tls/ssl":"");
					strcpy(jcl->authnonce, "abcdefghijklmnopqrstuvwxyz");	//FIXME: should be random
					Base64_Add("n,,n=", 5);
					Base64_Add(jclient->username, strlen(jcl->username));
					Base64_Add(",r=", 3);
					Base64_Add(jcl->authnonce, strlen(jcl->authnonce));	//must be random ascii.
					Base64_Finish();
					JCL_AddClientMessagef(jcl, "<auth xmlns='urn:ietf:params:xml:ns:xmpp-sasl' mechanism='SCRAM-SHA-1'>%s</auth>", base64);
					unparsable = false;
				}
				else
*/
				if (canplain && (jclient->issecure || pCvar_GetFloat("xmpp_allowplainauth")))
				{
					//plain can still be read with man-in-the-middle attacks, of course, even with tls if the certificate is spoofed, so this should always be the lowest priority.
					//we just hope that the tls certificate cannot be spoofed.
					Con_Printf("Using plain auth%s\n", jclient->issecure?" over tls/ssl":"");
					
					Base64_Add("", 1);
					Base64_Add(jclient->username, strlen(jcl->username));
					Base64_Add("", 1);
					Base64_Add(jcl->password, strlen(jcl->password));
					Base64_Finish();
					JCL_AddClientMessagef(jcl, "<auth xmlns='urn:ietf:params:xml:ns:xmpp-sasl' mechanism='PLAIN'>%s</auth>", base64);
					unparsable = false;
				}
				else
				{
					Con_Printf("XMPP: No suitable auth methods. Unable to connect.\n");
					XML_ConPrintTree(tree, 0);
					XML_Destroy(tree);
            		return JCL_KILL;
				}
			}
			else	//we cannot auth, no suitable method.
			{
				Con_Printf("XMPP: Neither SASL or TLS are usable\n");
				XML_Destroy(tree);
            	return JCL_KILL;
			}
		}
	}
/*
	else if (!strcmp(tree->name, "challenge") && !strcmp(tree->xmlns, "urn:ietf:params:xml:ns:xmpp-sasl"))
	{
		//sasl SCRAM-SHA-1 challenge
		//send back the same 'r' attribute
		buf saslchal;
		int l, i;
		buf salt;
		buf csn;
		buf itr;
		buf final;
		buf sigkey;
		char salted_password[20];
		char proof[20];
		char proof64[30];
		char clientkey[20];
		char storedkey[20];

		void hmacsha1(char *out, char *key, int keysize, char *data, int datalen);
		void Hi(char *out, char *password, buf salt, int i);

		saslchal.len = Base64_Decode(saslchal.buf, sizeof(saslchal.buf), tree->body, strlen(tree->body));
		//be warned, these CAN contain nulls.
		csn = saslattr(&saslchal, 'r');
		salt = saslattr(&saslchal, 's');
		itr = saslattr(&saslchal, 'i');
		

		//this is the first part of the message we're about to send, with no proof.
		//c(channel) is mandatory but nulled and forms part of the hash
		final.len = 0;
		buf_cat(&final, "c=", 2);
		Base64_Add("n,,", 3));
		Base64_Finish();
		final.cat(&final, base64, strlen(base64));
		final.cat(&final, "r=", 2);
		final.cat(&final, csn.buf, csn.len);

		//our original message + ',' + challenge + ',' + the message we're about to send.
		sigkey.len = 0;
		buf_cat(&sigkey, "n,,n=", 5);
		buf_cat(&sigkey, jcl->username, strlen(jcl->username));
		buf_cat(&sigkey, "r=", 2);
		buf_cat(&sigkey, jcl->authnonce, strlen(jcl->authnonce));
		buf_cat(&sigkey, ",", 1);
		buf_cat(&sigkey, saslchal.buf, saslchal.len);
		buf_cat(&sigkey, ",", 1);
		buf_cat(&sigkey, final.buf, final.len);

		Hi(salted_password, password, salt, atoi(itr));
		hmacsha1(clientkey, salted_password, sizeof(salted_password), "Client Key", strlen("Client Key"));
		storedkey = sha1(clientkey, sizeof(clientkey));
		hmacsha1(clientsignature, storedkey, sizeof(storedkey), sigkey.buf, sigkey.len);

		for (i = 0; i < sizeof(proof); i++)
			proof[i] = clientkey[i] ^ clientsignature[i];

		Base64_Add(proof, sizeof(proof));
		Base64_Finish();
		strcpy(proof64, base64);
		Base64_Add(final, buflen(final));
		Base64_Add(",p=", 3);
		Base64_Add(proof64, strlen(proof64));
		Base64_Finish();
		JCL_AddClientMessagef(jcl, "<response xmlns='urn:ietf:params:xml:ns:xmpp-sasl'>%s</response>", base64);
	}
*/
	else if (!strcmp(tree->name, "proceed"))
	{
		//switch to TLS, if we can

		//Restart everything, basically.
		jcl->bufferedinammount = 0;
		jcl->instreampos = 0;
		jcl->tagdepth = 0;

		if (!BUILTINISVALID(Net_SetTLSClient))
		{
			Con_Printf("XMPP: proceed without TLS\n");
			XML_Destroy(tree);
			return JCL_KILL;
		}

		if (pNet_SetTLSClient(jcl->socket, jcl->domain)<0)
		{
			Con_Printf("XMPP: failed to switch to TLS\n");
			XML_Destroy(tree);
			return JCL_KILL;
		}
		jclient->issecure = true;

		JCL_AddClientMessageString(jcl,
			"<?xml version='1.0' ?>"
			"<stream:stream to='");
		JCL_AddClientMessageString(jcl, jcl->domain);
		JCL_AddClientMessageString(jcl, "' xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams' version='1.0'>");

		XML_Destroy(tree);
		return JCL_DONE;
	}
	else if (!strcmp(tree->name, "failure"))
	{
		if (tree->child)
			Con_Printf("XMPP: Failure: %s\n", tree->child->name);
		else
			Con_Printf("XMPP: Unknown failure\n");
		XML_Destroy(tree);
		return JCL_KILL;
	}
	else if (!strcmp(tree->name, "error"))
	{
		ot = XML_ChildOfTree(tree, "text", 0);
		if (ot)
			Con_Printf("XMPP: %s\n", ot->body);
		else
			Con_Printf("XMPP: Unknown error\n");

		ot = XML_ChildOfTree(tree, "conflict", 0);
		XML_Destroy(tree);

		if (ot)
			return JCL_NUKEFROMORBIT;
		else
			return JCL_KILL;
	}
	else if (!strcmp(tree->name, "success"))
	{
		//Restart everything, basically, AGAIN! (third time lucky?)
		jcl->bufferedinammount = 0;
		jcl->instreampos = 0;
		jcl->tagdepth = 0;

		JCL_AddClientMessageString(jcl,
			"<?xml version='1.0' ?>"
			"<stream:stream to='");
		JCL_AddClientMessageString(jcl, jcl->domain);
		JCL_AddClientMessageString(jcl, "' xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams' version='1.0'>");

		return JCL_DONE;
	}
	else if (!strcmp(tree->name, "iq"))
	{
		JCL_ParseIQ(jcl, tree);
		unparsable = false;
	}
	else if (!strcmp(tree->name, "message"))
	{
		JCL_ParseMessage(jcl, tree);
		unparsable = false;
	}
	else if (!strcmp(tree->name, "presence"))
	{
		JCL_ParsePresence(jcl, tree);
		//we should keep a list of the people that we know of.
		unparsable = false;
	}
	else
	{
		Con_Printf("JCL unrecognised stanza: %s\n", tree->name);
		XML_ConPrintTree(tree, 0);
	}

	XML_Destroy(tree);

	memmove(jcl->bufferedinmessage, jcl->bufferedinmessage+pos, jcl->bufferedinammount-pos);
	jcl->bufferedinammount -= pos;
	jcl->instreampos -= pos;

	if (unparsable)
	{
		Con_Printf("XMPP: Input corrupt, urecognised, or unusable. Disconnecting.\n");
		return JCL_KILL;
	}

	return JCL_CONTINUE;
}

void JCL_CloseConnection(jclient_t *jcl, qboolean reconnect)
{
	//send our signoff to the server, if we're still alive.
	Con_Printf("XMPP: Disconnected from %s@%s\n", jcl->username, jcl->domain);

	if (jcl->status == JCL_ACTIVE)
		JCL_AddClientMessageString(jcl, "<presence type='unavailable'/>");
	JCL_AddClientMessageString(jcl, "</stream:stream>");
	JCL_FlushOutgoing(jcl);

	//forget all our friends.
	JCL_ForgetBuddy(jcl, NULL, NULL);

	//destroy any data that never got sent
	free(jcl->outbuf);
	jcl->outbuf = NULL;
	jcl->outbuflen = 0;
	jcl->outbufpos = 0;
	jcl->outbufmax = 0;

	pNet_Close(jcl->socket);
	jcl->socket = -1;
	jcl->status = JCL_DEAD;

	jcl->timeout = jclient_curtime + 30*1000;	//wait 30 secs before reconnecting, to avoid flood-prot-protection issues.

	if (!reconnect)
	{
		free(jcl);
		if (jclient == jcl)
			jclient = NULL;
	}
}

//can be polled for server address updates
void JCL_GeneratePresence(jclient_t *jcl, qboolean force)
{
	int dummystat;
	char serveraddr[1024*16];
	char servermap[1024*16];
	//get the last server address

	serveraddr[0] = 0;
	servermap[0] = 0;

	if (!pCvar_GetFloat("xmpp_nostatus"))
	{
		if (pCvar_GetFloat("sv.state"))
		{
			pCvar_GetString("sv.mapname", servermap, sizeof(servermap));
		}
		else
		{
			if (!pCvar_GetString("cl_serveraddress", serveraddr, sizeof(serveraddr)))
				serveraddr[0] = 0;
			if (BUILTINISVALID(CL_GetStats))
			{
				//if we can't get any stats, its because we're not actually on the server.
				if (!pCL_GetStats(0, &dummystat, 1))
					serveraddr[0] = 0;
			}
		}
	}

	if (force || strcmp(jcl->curquakeserver, *servermap?servermap:serveraddr))
	{
		char caps[256];
		Q_strlcpy(jcl->curquakeserver, *servermap?servermap:serveraddr, sizeof(jcl->curquakeserver));
		Con_DPrintf("Sending presence %s\n", jcl->curquakeserver);
		//note: ext='voice-v1 camera-v1 video-v1' is some legacy nonsense, and is required for voice calls with googletalk clients or something stupid like that
		Q_snprintf(caps, sizeof(caps), "<c xmlns='http://jabber.org/protocol/caps' hash='sha-1' node='http://fteqw.com/ftexmppplugin' ver='%s'/>", buildcapshash());

		if (!*jcl->curquakeserver)
			JCL_AddClientMessagef(jcl,
					"<presence>"
						"%s"
					"</presence>", caps);
		else if (*servermap)	//if we're running a server, say so
			JCL_AddClientMessagef(jcl, 
						"<presence>"
							"<quake xmlns='fteqw.com:game' servermap='%s'/>"
							"%s"
						"</presence>"
						, servermap, caps);
		else	//if we're connected to a server, say so
			JCL_AddClientMessagef(jcl, 
						"<presence>"
							"<quake xmlns='fteqw.com:game' serverip='%s' />"
							"%s"
						"</presence>"
				,jcl->curquakeserver, caps);
	}
}

//functions above this line allow connections to multiple servers.
//it is just the control functions that only allow one server.

qintptr_t JCL_Frame(qintptr_t *args)
{
	int stat = JCL_CONTINUE;
	jclient_t *jcl = jclient;

	jclient_curtime = args[0];
	if (jcl)
	{
		JCL_JingleTimeouts(jcl, false);
		if (jcl->status == JCL_DEAD)
		{
			if (jclient_curtime > jcl->timeout)
			{
				JCL_Reconnect(jcl);
				jcl->timeout = jclient_curtime + 60*1000;
			}
		}
		else
		{
			if (jcl->connected)
				JCL_GeneratePresence(jcl, false);
			while(stat == JCL_CONTINUE)
				stat = JCL_ClientFrame(jcl);
			if (stat == JCL_NUKEFROMORBIT)
				JCL_CloseConnection(jcl, false);
			else if (stat == JCL_KILL)
				JCL_CloseConnection(jcl, true);
			else
				JCL_FlushOutgoing(jcl);
		}
	}
	return 0;
}

void JCL_WriteConfig(void)
{
	if (jclient->connected)
	{
		qhandle_t config;
		pFS_Open("**plugconfig", &config, 2);
		if (config >= 0)
		{
			char buffer[8192];
			Q_snprintf(buffer, sizeof(buffer), "%i \"%s\" \"%s@%s\" \"%s\"\n",
				jclient->tlsconnect, jclient->server, jclient->username, jclient->domain, jclient->password);
			pFS_Write(config, buffer, strlen(buffer));
			pFS_Close(config);
		}
	}
}
void JCL_LoadConfig(void)
{
	if (!jclient)
	{
		int len;
		qhandle_t config;
		char buf[8192];
		char tls[256];
		char server[256];
		char account[256];
		char password[256];
		char *line = buf;
		qboolean oldtls;
		len = pFS_Open("**plugconfig", &config, 1);
		if (config >= 0)
		{
			if (len >= sizeof(buf))
				len = sizeof(buf)-1;
			buf[len] = 0;
			pFS_Read(config, buf, len);
			pFS_Close(config);

			line = JCL_ParseOut(line, tls, sizeof(tls));
			line = JCL_ParseOut(line, server, sizeof(server));
			line = JCL_ParseOut(line, account, sizeof(account));
			line = JCL_ParseOut(line, password, sizeof(password));

			oldtls = atoi(tls);

			jclient = JCL_Connect(server, oldtls, account, password);
		}
	}
}
static void JCL_PrintBuddyStatus(char *console, buddy_t *b, bresource_t *r)
{
	if (r->servertype == 2)
		Con_SubPrintf(console, "^[[Playing Quake - %s]\\xmpp\\%s/%s\\xmppact\\join^]", r->server, b->accountdomain, r->resource);
	else if (r->servertype)
		Con_SubPrintf(console, "^[[Playing Quake - %s]\\observe\\%s^]", r->server, r->server);
	else if (*r->fstatus)
		Con_SubPrintf(console, "%s - %s", r->bstatus, r->fstatus);
	else
		Con_SubPrintf(console, "%s", r->bstatus);

	if ((r->caps & CAP_INVITE) && !r->servertype)
		Con_SubPrintf(console, " ^[[Invite]\\xmpp\\%s/%s\\xmppact\\invite^]", b->accountdomain, r->resource);
	if (r->caps & CAP_VOICE)
		Con_SubPrintf(console, " ^[[Call]\\xmpp\\%s/%s\\xmppact\\call^]",  b->accountdomain, r->resource);
}
void JCL_PrintBuddyList(char *console, jclient_t *jcl, qboolean all)
{
	buddy_t *b;
	bresource_t *r;
	struct c2c_s *c2c;
	if (!jcl->buddies)
		Con_SubPrintf(console, "You have no friends\n");
	for (b = jcl->buddies; b; b = b->next)
	{
		//if we don't actually know them, don't list them.
		if (!b->friended)
			continue;

		if (!b->resources)	//offline
		{
			if (all)
				Con_SubPrintf(console, "^[^7[%s]\\xmpp\\%s^]: offline\n", b->name, b->accountdomain);
		}
		else if (b->resources->next)
		{	//multiple potential resources
			Con_SubPrintf(console, "^[[%s]\\xmpp\\%s^]\n", b->name, b->accountdomain);
			for (r = b->resources; r; r = r->next)
			{
				Con_SubPrintf(console, "    ^[[%s]\\xmpp\\%s/%s^]: ", r->resource, b->accountdomain, r->resource);
				JCL_PrintBuddyStatus(console, b, r);
				Con_SubPrintf(console, "\n");
			}
		}
		else	//only one resource
		{
			r = b->resources;
			Con_SubPrintf(console, "^[[%s]\\xmpp\\%s/%s^]: ", b->name, b->accountdomain, r->resource);
			JCL_PrintBuddyStatus(console, b, r);
			Con_SubPrintf(console, "\n");
		}
	}

	if (jcl->c2c)
		Con_SubPrintf(console, "Active sessions:\n");
	for (c2c = jcl->c2c; c2c; c2c = c2c->next)
	{
		JCL_FindBuddy(jcl, c2c->with, &b, &r);
		switch(c2c->mediatype)
		{
		case ICEP_VOICE:
			Con_SubPrintf(console, "    ^[[%s]\\xmpp\\%s/%s^]: voice ^[[Hang Up]\\xmppact\\jdeny\\xmpp\\%s\\xmppsid\\%s^]\n", b->name, b->accountdomain, r->resource, c2c->with, c2c->sid);
			break;
		case ICEP_QWSERVER:
			Con_SubPrintf(console, "    ^[[%s]\\xmpp\\%s/%s^]: server ^[[Kick]\\xmppact\\jdeny\\xmpp\\%s\\xmppsid\\%s^]\n", b->name, b->accountdomain, r->resource, c2c->with, c2c->sid);
			break;
		case ICEP_QWCLIENT:
			Con_SubPrintf(console, "    ^[[%s]\\xmpp\\%s/%s^]: client ^[[Disconnect]\\xmppact\\jdeny\\xmpp\\%s\\xmppsid\\%s^]\n", b->name, b->accountdomain, r->resource, c2c->with, c2c->sid);
			break;
		}
	}
}

void JCL_SendMessage(jclient_t *jcl, char *to, char *msg)
{
	char markup[1024];
	buddy_t *b;
	bresource_t *br;
	JCL_FindBuddy(jcl, to, &b, &br);
	if (br)
		JCL_AddClientMessagef(jcl, "<message to='%s/%s'><body>", b->accountdomain, br->resource);
	else
		JCL_AddClientMessagef(jcl, "<message to='%s'><body>", b->accountdomain);
	JCL_AddClientMessage(jcl, markup, XML_Markup(msg, markup, sizeof(markup)) - markup);
	JCL_AddClientMessageString(jcl, "</body></message>");
	if (!strncmp(msg, "/me ", 4))
		Con_SubPrintf(b->name, "*^5%s^7"COLOURYELLOW"%s\n", ((!strcmp(jcl->localalias, ">>"))?"me":jcl->localalias), msg+3);
	else
		Con_SubPrintf(b->name, "^5%s^7: "COLOURYELLOW"%s\n", jcl->localalias, msg);
}


void JCL_Command(char *console)
{
	char imsg[8192];
	char arg[6][256];
	char *msg;
	int i;

	pCmd_Args(imsg, sizeof(imsg));

	msg = imsg;
	for (i = 0; i < 6; i++)
	{
		if (!msg)
			continue;
		msg = JCL_ParseOut(msg, arg[i], sizeof(arg[i]));
	}

	if (arg[0][0] == '/' && arg[0][1] != '/' && strcmp(arg[0]+1, "me"))
	{
		if (!strcmp(arg[0]+1, "open") || !strcmp(arg[0]+1, "connect") || !strcmp(arg[0]+1, "tlsopen") || !strcmp(arg[0]+1, "tlsconnect"))
		{	//tlsconnect is 'old'.
			if (!*arg[1])
			{
				Con_SubPrintf(console, "%s <account@domain/resource> <password> <server>\n", arg[0]+1);
				return;
			}

			if (jclient)
			{
				Con_TrySubPrint(console, "You are already connected\nPlease /quit first\n");
				return;
			}
			jclient = JCL_Connect(arg[3], !strncmp(arg[0]+1, "tls", 3), arg[1], arg[2]);
			if (!jclient)
			{
				Con_TrySubPrint(console, "Connect failed\n");
				return;
			}
		}
		else if (!strcmp(arg[0]+1, "help")) 
		{
			Con_TrySubPrint(console, "^[/" COMMANDPREFIX " /connect XMPPSERVER USERNAME@DOMAIN/RESOURCE PASSWORD^]\n");
			if (BUILTINISVALID(Net_SetTLSClient))
			{
				Con_Printf("eg for gmail: ^[/" COMMANDPREFIX " /connect myusername@gmail.com mypassword talk.google.com^]\n");
				Con_Printf("eg for facebook: ^[/" COMMANDPREFIX " /connect myusername@chat.facebook.com mypassword chat.facebook.com^]\n");
			}
			Con_Printf("Note that this info will be used the next time you start quake.\n");

			//small note:
			//for the account 'me@example.com' the server to connect to can be displayed with:
			//nslookup -querytype=SRV _xmpp-client._tcp.example.com
			//srv resolving seems to be non-standard on each system, I don't like having to special case things.
			Con_TrySubPrint(console, 	"^[/" COMMANDPREFIX " /help^]\n"
										"This text...\n");
			Con_TrySubPrint(console, 	"^[/" COMMANDPREFIX " /raw <XML STANZAS/>^]\n"
										"For debug hackery.\n");
			Con_TrySubPrint(console, 	"^[/" COMMANDPREFIX " /friend accountname friendlyname^]\n"
										"Befriends accountname, and shows them in your various lists using the friendly name. Can also be used to rename friends.\n");
			Con_TrySubPrint(console,	"^[/" COMMANDPREFIX " /unfriend accountname^]\n"
										"Ostracise your new best enemy. You will no longer see them and they won't be able to contact you.\n");
			Con_TrySubPrint(console, 	"^[/" COMMANDPREFIX " /blist^]\n"
										"Show all your friends! Names are clickable and will begin conversations.\n");
			Con_TrySubPrint(console, 	"^[/" COMMANDPREFIX " /quit^]\n"
										"Disconnect from the XMPP server, noone will be able to hear you scream.\n");
			Con_TrySubPrint(console, 	"^[/" COMMANDPREFIX " /join accountname^]\n"
										"Joins your friends game (they will be prompted).\n");
			Con_TrySubPrint(console, 	"^[/" COMMANDPREFIX " /invite accountname^]\n"
										"Invite someone to join your game (they will be prompted).\n");
			Con_TrySubPrint(console, 	"^[/" COMMANDPREFIX " /voice accountname^]\n"
										"Begin a bi-directional peer-to-peer voice conversation with someone (they will be prompted).\n");
			Con_TrySubPrint(console, 	"^[/" COMMANDPREFIX " /msg ACCOUNTNAME your message goes here^]\n"
										"Sends a message to the named person. If given a resource postfix, your message will be sent only to that resource.\n");
			Con_TrySubPrint(console, 	"If no arguments, will print out your friends list. If no /command is used, the arguments will be sent as a message to the person you last sent a message to.\n");
		}
		else if (!strcmp(arg[0]+1, "clear"))
		{
			//just clears the current console.
			if (*console)
			{
				pCon_Destroy(console);
				Con_SubPrintf(console, "");
				pCon_SetActive(console);
			}
			else
				pCmd_AddText("\nclear\n", true);
		}
		else if (!jclient)
		{
			Con_SubPrintf(console, "No account specified. Cannot %s\n", arg[0]);
		}
		else if (!strcmp(arg[0]+1, "quit"))
		{
			//disconnect from the xmpp server.
			JCL_CloseConnection(jclient, false);
		}
		else if (jclient->status != JCL_ACTIVE)
		{
			Con_SubPrintf(console, "You are not authed. Please wait.\n", arg[0]);
		}
		else if (!strcmp(arg[0]+1, "blist"))
		{
			//print out a full list of everyone, even those offline.
			JCL_PrintBuddyList(console, jclient, true);
		}
		else if (!strcmp(arg[0]+1, "msg"))
		{
			//FIXME: validate the dest. deal with xml markup in dest.
			Q_strlcpy(jclient->defaultdest, arg[1], sizeof(jclient->defaultdest));
			msg = arg[2];

			JCL_SendMessage(jclient, jclient->defaultdest, msg);
		}
		else if (!strcmp(arg[0]+1, "friend")) 
		{
			//FIXME: validate the name. deal with xml markup.

			//can also rename. We should probably read back the groups for the update.
			JCL_SendIQf(jclient, NULL, "set", NULL, "<query xmlns='jabber:iq:roster'><item jid='%s' name='%s'></item></query>", arg[1], arg[2]);

			//start looking for em
			JCL_AddClientMessagef(jclient, "<presence to='%s' type='subscribe'/>", arg[1]);

			//let em see us
			if (jclient->preapproval)
				JCL_AddClientMessagef(jclient, "<presence to='%s' type='subscribed'/>", arg[1]);
		}
		else if (!strcmp(arg[0]+1, "unfriend")) 
		{
			//FIXME: validate the name. deal with xml markup.

			//hide from em
			JCL_AddClientMessagef(jclient, "<presence to='%s' type='unsubscribed'/>", arg[1]);

			//stop looking for em
			JCL_AddClientMessagef(jclient, "<presence to='%s' type='unsubscribe'/>", arg[1]);

			//stop listing em
			JCL_SendIQf(jclient, NULL, "set", NULL, "<query xmlns='jabber:iq:roster'><item jid='%s' subscription='remove' /></query>", arg[1]);
		}
		else if (!strcmp(arg[0]+1, "join")) 
		{
			JCL_Join(jclient, *arg[1]?arg[1]:console, NULL, true, ICEP_QWCLIENT);
		}
		else if (!strcmp(arg[0]+1, "invite")) 
		{
			JCL_Join(jclient, *arg[1]?arg[1]:console, NULL, true, ICEP_QWSERVER);
		}
		else if (!strcmp(arg[0]+1, "voice") || !strcmp(arg[0]+1, "call")) 
		{
			JCL_Join(jclient, *arg[1]?arg[1]:console, NULL, true, ICEP_VOICE);
		}
		else if (!strcmp(arg[0]+1, "kick")) 
		{
			JCL_Join(jclient, *arg[1]?arg[1]:console, NULL, false, ICEP_INVALID);
		}
		else if (!strcmp(arg[0]+1, "raw")) 
		{
			jclient->streamdebug = true;
			JCL_AddClientMessageString(jclient, arg[1]);
		}
		else
			Con_SubPrintf(console, "Unrecognised command: %s\n", arg[0]);
	}
	else
	{
		if (jclient)
		{
			msg = imsg;

			if (!*msg)
			{
				if (!*console)
				{
					JCL_PrintBuddyList(console, jclient, false);
					//Con_TrySubPrint(console, "For help, type \"^[/" COMMANDPREFIX " /help^]\"\n");
				}
			}
			else
			{
				JCL_SendMessage(jclient, jclient->defaultdest, msg);
			}
		}
		else
		{
			Con_TrySubPrint(console, "Not connected. For help, type \"^[/" COMMANDPREFIX " /help^]\"\n");
		}
	}
}
