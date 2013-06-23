//Released under the terms of the gpl as this file uses a bit of quake derived code. All sections of the like are marked as such

#include "../plugin.h"
#include <time.h>
#include "../../engine/common/netinc.h"
#include "xml.h"

#define NOICE

#define QUAKEMEDIATYPE "quake"
#define QUAKEMEDIAXMLNS "fteqw.com:netmedia"

struct icestate_s *(QDECL *pICE_Create)(void *module, char *conname, char *peername, enum icemode_e mode);	//doesn't start pinging anything.
struct icestate_s *(QDECL *pICE_Find)(void *module, char *conname);
void (QDECL *pICE_Begin)(struct icestate_s *con, char *stunip, int stunport);	//begins sending stun packets and stuff as required.
struct icecandidate_s *(QDECL *pICE_GetLCandidateInfo)(struct icestate_s *con);		//stuff that needs reporting to the peer.
void (QDECL *pICE_AddRCandidateInfo)(struct icestate_s *con, struct icecandidate_s *cand);		//stuff that came from the peer.
void (QDECL *pICE_Close)(struct icestate_s *con);	//bye then.


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
	Con_Print(message);
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

	char *COM_ParseOut (char *data, char *buf, int bufsize)	//this is taken out of quake
	{
		int		c;
		int		len;

		len = 0;
		buf[0] = 0;

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





void RenameConsole(char *totrim);
void JCL_Command(char *consolename);
void JCL_LoadConfig(void);
void JCL_WriteConfig(void);

qintptr_t JCL_ExecuteCommand(qintptr_t *args)
{
	char cmd[256];
	Cmd_Argv(0, cmd, sizeof(cmd));
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
			Con_Print("XMPP Plugin Loaded ^1without^7 TLS\n");
		else
			Con_Print("XMPP Plugin Loaded. For help, use: ^[/"COMMANDPREFIX" /help^]\n");

		Plug_Export("ConsoleLink", JCL_ConsoleLink);

		if (!Plug_Export("ConExecuteCommand", JCL_ConExecuteCommand))
		{
			Con_Printf("XMPP plugin in single-console mode\n");
			Con_TrySubPrint = Fallback_ConPrint;
		}
		else
			Con_TrySubPrint = Con_SubPrint;

		Cmd_AddCommand(COMMANDPREFIX);
		Cmd_AddCommand(COMMANDPREFIX2);
		Cmd_AddCommand(COMMANDPREFIX3);


		CHECKBUILTIN(Plug_GetNativePointer);
		if (BUILTINISVALID(Plug_GetNativePointer))
		{
			pICE_Create				= Plug_GetNativePointer("ICE_Create");
			pICE_Find				= Plug_GetNativePointer("ICE_Find");
			pICE_Begin				= Plug_GetNativePointer("ICE_Begin");
			pICE_GetLCandidateInfo	= Plug_GetNativePointer("ICE_GetLCandidateInfo");
			pICE_AddRCandidateInfo	= Plug_GetNativePointer("ICE_AddRCandidateInfo");
			pICE_Close				= Plug_GetNativePointer("ICE_Close");
		}


		JCL_LoadConfig();
		return 1;
	}
	else
		Con_Print("JCL Client Plugin failed\n");
	return 0;
}










//\r\n is used to end a line.
//meaning \0s are valid.
//but never used cos it breaks strings


#define JCL_MAXMSGLEN 10000

typedef struct bresource_s
{
	char bstatus[128];	//basic status
	char fstatus[128];	//full status
	char server[256];
	int servertype;	//0=none, 1=already a client, 2=joinable

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

	qhandle_t socket;

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

	int tagdepth;
	int openbracket;
	int instreampos;

	qboolean tlsconnect;	//the old tls method on port 5223.
	qboolean connected;	//fully on server and authed and everything.
	qboolean issecure;	//tls enabled
	qboolean streamdebug;	//echo the stream to subconsoles

	qboolean preapproval;

	char curquakeserver[2048];
	char defaultnamespace[2048];	//should be 'jabber:client' or blank (and spammy with all the extra xmlns attribs)

	struct iq_s {
		struct iq_s *next;
		char id[64];
		int timeout;
		qboolean (*callback) (struct jclient_s *jcl, struct subtree_s *tree);
	} *pendingiqs;

	struct c2c_s
	{
		struct c2c_s *next;
		char *sessionname;
		buddy_t *tob;
		bresource_t *tor;
	};

	buddy_t *buddies;
} jclient_t;
jclient_t *jclient;

struct subtree_s;

void JCL_AddClientMessagef(jclient_t *jcl, char *fmt, ...);
void JCL_GeneratePresence(qboolean force);
void JCL_SendIQf(jclient_t *jcl, qboolean (*callback) (jclient_t *jcl, struct subtree_s *tree), char *iqtype, char *target, char *fmt, ...);
void JCL_SendIQNode(jclient_t *jcl, qboolean (*callback) (jclient_t *jcl, xmltree_t *tree), char *iqtype, char *target, xmltree_t *node, qboolean destroynode);

void JCL_Join(jclient_t *jcl, char *target)
{
	char *s;
	xmltree_t *jingle;
	struct icestate_s *ice;
	if (!jcl)
		return;

#ifdef NOICE
	ice = pICE_Create(NULL, NULL, target, ICE_RAW);
#else
	ice = pICE_Create(NULL, NULL, target, ICE_ICE);
#endif
	if (!ice)
	{
		Con_Printf("Unable to connect to %s (dedicated servers cannot initiate connections)\n", target);
		return;
	}
	//FIXME: record the session name

	jingle = XML_CreateNode(NULL, "jingle", "urn:xmpp:jingle:1", "");
	XML_AddParameter(jingle, "sid", ice->conname);
	XML_AddParameter(jingle, "responder", target);
	XML_AddParameter(jingle, "initiator", jcl->jid);
	XML_AddParameter(jingle, "action", "session-initiate");
	{
		xmltree_t *content = XML_CreateNode(jingle, "content", "", "");
		XML_AddParameter(content, "senders", "both");
		XML_AddParameter(content, "name", "some-old-quake-game");
		XML_AddParameter(content, "creator", "initiator");
		{
			xmltree_t *description;
			xmltree_t *transport;
			if (ice->mode == ICE_RAW)
			{
				transport = XML_CreateNode(content, "transport", "urn:xmpp:jingle:transports:raw-udp:1", "");
				{
					xmltree_t *candidate;
					struct icecandidate_s *b = NULL;
					struct icecandidate_s *c;
					while ((c = pICE_GetLCandidateInfo(ice)))
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
			else if (ice->mode == ICE_ICE)
			{
				transport = XML_CreateNode(content, "transport", "urn:xmpp:jingle:transports:ice-udp:1", "");
				XML_AddParameter(transport, "ufrag", ice->lfrag);
				XML_AddParameter(transport, "pwd", ice->lpwd);
				{
					struct icecandidate_s *c;
					while ((c = pICE_GetLCandidateInfo(ice)))
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
			description = XML_CreateNode(content, "description", QUAKEMEDIAXMLNS, "");
			XML_AddParameter(description, "media", QUAKEMEDIATYPE);
			{
				/*
				xmltree_t *candidate = XML_CreateNode(description, "payload-type", "", "");
				XML_AddParameter(candidate, "channels", "1");
				XML_AddParameter(candidate, "clockrate", "8000");
				XML_AddParameter(candidate, "id", "104");
				XML_AddParameter(candidate, "name", "SPEEX");
				*/
			}
		}
	}
	Con_Printf("Sending connection start:\n");
	XML_ConPrintTree(jingle, 0);
	JCL_SendIQNode(jcl, NULL, "set", target, jingle, true);
}

void JCL_JingleParsePeerPorts(jclient_t *jcl, xmltree_t *inj, char *from)
{
	xmltree_t *incontent = XML_ChildOfTree(inj, "content", 0);
	xmltree_t *intransport = XML_ChildOfTree(incontent, "transport", 0);
	xmltree_t *incandidate;
	struct icestate_s *ice;
	struct icecandidate_s rem;
	int i;

	ice = pICE_Find(NULL, XML_GetParameter(inj, "sid", ""));
	if (ice && strcmp(ice->friendlyname, from))
	{
		Con_Printf("%s is trying to mess with our connections...\n", from);
		return;
	}

	for (i = 0; incandidate = XML_ChildOfTree(intransport, "candidate", i); i++)
	{
		char *s;
		memset(&rem, 0, sizeof(rem));
		rem.addr = XML_GetParameter(incandidate, "ip", "");
		rem.candidateid = XML_GetParameter(incandidate, "id", "");

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
		pICE_AddRCandidateInfo(ice, &rem);
	}
}
struct icestate_s *JCL_JingleHandleInitiate(jclient_t *jcl, xmltree_t *inj, char *from)
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

	xmltree_t *jingle;
	struct icestate_s *ice;
	qboolean accepted = false;
	enum icemode_e imode;

	imode = strcmp(transportxmlns, "urn:xmpp:jingle:transports:raw-udp:1")?ICE_ICE:ICE_RAW;

	if (!incontent || strcmp(descriptionmedia, QUAKEMEDIATYPE) || strcmp(descriptionxmlns, QUAKEMEDIAXMLNS))
	{
		//decline it
		ice = NULL;
	}
	else
		ice = pICE_Create(NULL, XML_GetParameter(inj, "sid", ""), from, imode);

	jingle = XML_CreateNode(NULL, "jingle", "urn:xmpp:jingle:1", "");
	XML_AddParameter(jingle, "sid", ice->conname);
	XML_AddParameter(jingle, "responder", XML_GetParameter(inj, "responder", ""));
	XML_AddParameter(jingle, "initiator", XML_GetParameter(inj, "initiator", ""));
	if (!ice)
		XML_AddParameter(jingle, "action", "session-terminate");
	else
	{
		xmltree_t *content = XML_CreateNode(jingle, "content", "", "");
#ifdef NOICE
		if (imode != ICE_RAW)
		{
			char buf[256];
			XML_AddParameter(jingle, "action", "transport-replace");
			Q_snprintf(buf, sizeof(buf), "raw-%s", XML_GetParameter(incontent, "name", ""));
			XML_AddParameter(content, "name", buf);
		}
		else
#endif
		{
			XML_AddParameter(jingle, "action", "session-accept");
			XML_AddParameter(content, "name", XML_GetParameter(incontent, "name", ""));
			accepted = true;
		}
		XML_AddParameter(content, "senders", "both");
		XML_AddParameter(content, "creator", "initiator");
		{
			xmltree_t *description;
			xmltree_t *transport;
			transport = XML_CreateNode(content, "transport", transportxmlns, "");
			if (imode == ICE_RAW)
			{
				//raw-udp can send only one candidate
				xmltree_t *candidate;
				struct icecandidate_s *b = NULL;
				struct icecandidate_s *c;
				while ((c = pICE_GetLCandidateInfo(ice)))
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
			else if (imode == ICE_ICE)
			{
				//ice can send multiple candidates
				struct icecandidate_s *c;
				XML_AddParameter(transport, "ufrag", ice->lfrag);
				XML_AddParameter(transport, "pwd", ice->lpwd);
				while ((c = pICE_GetLCandidateInfo(ice)))
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
			else
			{
				//egads! can't cope with that.
			}
			description = XML_CreateNode(content, "description", QUAKEMEDIAXMLNS, "");
			XML_AddParameter(description, "media", QUAKEMEDIATYPE);
			{
				/*
				xmltree_t *candidate = XML_CreateNode(description, "payload-type", "", "");
				XML_AddParameter(candidate, "channels", "1");
				XML_AddParameter(candidate, "clockrate", "8000");
				XML_AddParameter(candidate, "id", "104");
				XML_AddParameter(candidate, "name", "SPEEX");
				*/
			}
		}
	}
	if (!ice)
		Con_Printf("Sending reject:\n");
	else
		Con_Printf("Sending accept:\n");
	XML_ConPrintTree(jingle, 0);
	JCL_SendIQNode(jcl, NULL, "set", from, jingle, true);

	if (ice)
		JCL_JingleParsePeerPorts(jcl, inj, from);

	//if we didn't error out, the ICE stuff is meant to start sending handshakes/media as soon as the connection is accepted
	if (ice && accepted)
		pICE_Begin(ice, NULL, 0);
	return ice;
}

void JCL_ParseJingle(jclient_t *jcl, xmltree_t *tree, char *from, char *id)
{
	char *action = XML_GetParameter(tree, "action", "");
	char *initiator = XML_GetParameter(tree, "initiator", "");
	char *responder = XML_GetParameter(tree, "responder", "");
	char *sid = XML_GetParameter(tree, "sid", "");

	//validate sender
	struct icestate_s *ice = pICE_Find(NULL, sid);
	if (ice && strcmp(ice->friendlyname, from))
	{
		Con_Printf("%s is trying to mess with our connections...\n", from);
		return;
	}

	if (!strcmp(action, "session-terminate"))
	{
		if (ice)
			pICE_Close(ice);

		Con_Printf("Session ended\n");
		XML_ConPrintTree(tree, 0);
	}
	else if (!strcmp(action, "session-accept"))
	{
		if (!ice)
		{
			Con_Printf("Cannot accept a session that was never created\n");
		}
		else
		{
			Con_Printf("Session Accepted!\n");
			XML_ConPrintTree(tree, 0);

			if (ice)
				JCL_JingleParsePeerPorts(jcl, tree, from);

			//if we didn't error out, the ICE stuff is meant to start sending handshakes/media as soon as the connection is accepted
			if (ice)
				pICE_Begin(ice, NULL, 0);
		}
	}
	else if (!strcmp(action, "session-initiate"))
	{
		Con_Printf("Peer initiating connection!\n");
		XML_ConPrintTree(tree, 0);

		ice = JCL_JingleHandleInitiate(jcl, tree, from);
	}
	else
	{
		Con_Printf("Unknown jingle action: %s\n", action);
		XML_ConPrintTree(tree, 0);
	}

	JCL_AddClientMessagef(jcl,
		"<iq type='result' to='%s' id='%s' />", from, id);
}


qintptr_t JCL_ConsoleLink(qintptr_t *args)
{
	char text[256];
	char link[256];
	Cmd_Argv(0, text, sizeof(text));
	Cmd_Argv(1, link, sizeof(link));

	if (!strncmp(link, "\\xmppauth\\", 6))
	{
		//we should friend them too.
		if (jclient)
			JCL_AddClientMessagef(jclient, "<presence to='%s' type='subscribed'/>", link+10);
		return true;
	}
	if (!strncmp(link, "\\xmppdeny\\", 6))
	{
		if (jclient)
			JCL_AddClientMessagef(jclient, "<presence to='%s' type='unsubscribed'/>", link+10);
		return true;
	}

	if (!strncmp(link, "\\xmppjoin\\", 6))
	{
		JCL_Join(jclient, link+10);
		return false;
	}
	if (!strncmp(link, "\\xmpp\\", 6))
	{
		if (jclient)
		{
			char *f;
			buddy_t *b;
			bresource_t *br;

			JCL_FindBuddy(jclient, link+6, &b, &br);
			f = b->name;
			b->defaultresource = br;

			if (BUILTINISVALID(Con_SubPrint))
				Con_SubPrint(f, "");
			if (BUILTINISVALID(Con_SetActive))
				Con_SetActive(f);
		}
		return true;
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
		Cmd_Argv(0, buffer, sizeof(buffer));
		Con_SubPrint(buffer, "You were disconnected\n");
		return true;
	}
	Cmd_Argv(0, consolename, sizeof(consolename));
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

	sent = Net_Send(jcl->socket, jcl->outbuf + jcl->outbufpos, jcl->outbuflen);	//FIXME: This needs rewriting to cope with errors.
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
	else
		Con_Printf("Unable to send anything\n");
}
void JCL_AddClientMessage(jclient_t *jcl, char *msg, int datalen)
{
	int sent;

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
	struct iq_s *iq;
		
	va_start (argptr, fmt);
	vsnprintf (body, sizeof(body), fmt, argptr);
	va_end (argptr);

	JCL_AddClientMessageString(jcl, body);
}
jclient_t *JCL_Connect(char *server, int defport, qboolean usesecure, char *account, char *password)
{
	jclient_t *jcl;
	char *at;

	if (usesecure)
	{
		if (!BUILTINISVALID(Net_SetTLSClient))
		{
			Con_Printf("JCL_OpenSocket: TLS is not supported\n");
			return NULL;
		}
	}

	at = strchr(account, '@');
	if (!at)
		return NULL;


	jcl = malloc(sizeof(jclient_t));
	if (!jcl)
		return NULL;

	memset(jcl, 0, sizeof(jclient_t));


	jcl->socket = Net_TCPConnect(server, defport);	//port is only used if the url doesn't contain one. It's a default.

	//not yet blocking. So no frequent attempts please...
	//non blocking prevents connect from returning worthwhile sensible value.
	if ((int)jcl->socket < 0)
	{
		Con_Printf("JCL_OpenSocket: couldn't connect\n");
		free(jcl);
		return NULL;
	}

	if (usesecure)
	{
		if (Net_SetTLSClient(jcl->socket, server)<0)
		{
			Net_Close(jcl->socket);
			free(jcl);
			jcl = NULL;

			return NULL;
		}
		jcl->issecure = true;
	}
	else
		jcl->issecure = false;


//	gethostname(jcl->hostname, sizeof(jcl->hostname));
//	jcl->hostname[sizeof(jcl->hostname)-1] = 0;

	jcl->tlsconnect = usesecure;
	jcl->streamdebug = !!Cvar_GetFloat("xmpp_debug");

	*at = '\0';
	Q_strlcpy(jcl->server, server, sizeof(jcl->server));
	Q_strlcpy(jcl->username, account, sizeof(jcl->username));
	Q_strlcpy(jcl->domain, at+1, sizeof(jcl->domain));
	Q_strlcpy(jcl->password, password, sizeof(jcl->password));

	Q_strlcpy(jcl->resource, "Quake", sizeof(jcl->password));

	Con_Printf("Trying to connect to %s\n", at+1);
	JCL_AddClientMessageString(jcl,
		"<?xml version='1.0' ?>"
		"<stream:stream to='");
	JCL_AddClientMessageString(jcl, jcl->domain);
	JCL_AddClientMessageString(jcl, "' xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams' version='1.0'>");

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
	int i;
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

void JCL_SendIQ(jclient_t *jcl, qboolean (*callback) (jclient_t *jcl, xmltree_t *tree), char *iqtype, char *target, char *body)
{
	struct iq_s *iq;
		
	iq = malloc(sizeof(*iq));
	iq->next = jcl->pendingiqs;
	jcl->pendingiqs = iq;
	sprintf(iq->id, "%i", rand());
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
}
void JCL_SendIQf(jclient_t *jcl, qboolean (*callback) (jclient_t *jcl, xmltree_t *tree), char *iqtype, char *target, char *fmt, ...)
{
	va_list		argptr;
	char body[2048];

	va_start (argptr, fmt);
	vsnprintf (body, sizeof(body), fmt, argptr);
	va_end (argptr);

	JCL_SendIQ(jcl, callback, iqtype, target, body);
}
void JCL_SendIQNode(jclient_t *jcl, qboolean (*callback) (jclient_t *jcl, xmltree_t *tree), char *iqtype, char *target, xmltree_t *node, qboolean destroynode)
{
	char *s = XML_GenerateString(node);
	JCL_SendIQ(jcl, callback, iqtype, target, s);
	free(s);
	if (destroynode)
		XML_Destroy(node);
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
		char *sub = XML_GetParameter(i, "subscription", "");
		JCL_FindBuddy(jcl, jid, &buddy, NULL);

		if (*name)
			Q_strlcpy(buddy->name, name, sizeof(buddy->name));
		buddy->friended = true;
	}
}
static qboolean JCL_RosterReply(jclient_t *jcl, xmltree_t *tree)
{
	xmltree_t *c, *i;
	c = XML_ChildOfTree(tree, "query", 0);
	if (c)
	{
		JCL_RosterUpdate(jcl, c);
		return true;
	}
	JCL_GeneratePresence(true);
	return false;
}

static qboolean JCL_BindReply(jclient_t *jcl, xmltree_t *tree)
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
static qboolean JCL_SessionReply(jclient_t *jcl, xmltree_t *tree)
{
	JCL_SendIQf(jcl, JCL_RosterReply, "get", NULL, "<query xmlns='jabber:iq:roster'/>");
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
//	"urn:xmpp:jingle:apps:rtp:1",	//we don't support rtp video/voice chat
//	"urn:xmpp:jingle:apps:rtp:audio",//we don't support rtp voice chat
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

#define JCL_DONE 0
#define JCL_CONTINUE 1
#define JCL_KILL 2
int JCL_ClientFrame(jclient_t *jcl)
{
	int pos;
	xmltree_t *tree, *ot;
	char *f;
	int ret;
	qboolean unparsable;

	int olddepth;

	ret = Net_Recv(jcl->socket, jcl->bufferedinmessage+jcl->bufferedinammount, sizeof(jcl->bufferedinmessage)-1 - jcl->bufferedinammount);
	if (ret == 0)
	{
		if (!jcl->bufferedinammount)	//if we are half way through a message, read any possible conjunctions.
			return JCL_DONE;	//remove
	}
	if (ret < 0)
	{
		Con_Printf("JCL: socket error\n");
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
			if ((!jclient->issecure) && BUILTINISVALID(Net_SetTLSClient) && XML_ChildOfTree(tree, "starttls", 0) != NULL && !Cvar_GetFloat("xmpp_disabletls"))
			{
				Con_Printf("Attempting to switch to TLS\n");
				JCL_AddClientMessageString(jcl, "<starttls xmlns='urn:ietf:params:xml:ns:xmpp-tls' />");
				unparsable = false;
			}
			else if ((ot=XML_ChildOfTree(tree, "mechanisms", 0)))
			{
				for(ot = ot->child; ot; ot = ot->sibling)
				{
					if (!strcmp(ot->body, "PLAIN"))
					{
						char msg[2048];
						if (!jclient->issecure && !Cvar_GetFloat("xmpp_allowplainauth"))	//probably don't send plain without tls.
						{
							//plain can still be read with man-in-the-middle attacks, of course, even with tls if the certificate is spoofed.
							Con_Printf("Ignoring auth \'%s\'\n", ot->body);
							continue;
						}
						Con_Printf("Authing with \'%s\'%s\n", ot->body, jclient->issecure?" over tls/ssl":"");
						
//						Base64_Add(jclient->username, strlen(jcl->username));
//						Base64_Add("@", 1);
//						Base64_Add(jclient->domain, strlen(jcl->domain));
						Base64_Add("", 1);
						Base64_Add(jclient->username, strlen(jcl->username));
						Base64_Add("", 1);
						Base64_Add(jcl->password, strlen(jcl->password));
						Base64_Finish();
						Q_snprintf(msg, sizeof(msg), "<auth xmlns='urn:ietf:params:xml:ns:xmpp-sasl' mechanism='PLAIN'>%s</auth>", base64);
						JCL_AddClientMessageString(jcl, msg);
//						JCL_AddClientMessageString(jcl, "<auth xmlns='urn:ietf:params:xml:ns:xmpp-sasl' mechanism='PLAIN'>");
//						JCL_AddClientMessageString(jcl, base64);
//						JCL_AddClientMessageString(jcl, "</auth>");
						unparsable = false;
						break;
					}
					else
						Con_Printf("Unable to use auth method \'%s\'\n", ot->body);
				}
				if (!ot)
				{
					Con_Printf("JCL: No suitable auth methods\n");
					unparsable = true;
				}
			}
			else	//we cannot auth, no suitable method.
			{
				Con_Printf("JCL: Neither SASL or TLS are usable\n");
				unparsable = true;
			}
		}
	}
	else if (!strcmp(tree->name, "proceed"))
	{
		//switch to TLS, if we can

		//Restart everything, basically.
		jcl->bufferedinammount = 0;
		jcl->instreampos = 0;
		jcl->tagdepth = 0;

		if (!BUILTINISVALID(Net_SetTLSClient))
		{
			Con_Printf("JCL: proceed without TLS\n");
			XML_Destroy(tree);
			return JCL_KILL;
		}

		if (Net_SetTLSClient(jcl->socket, jcl->domain)<0)
		{
			Con_Printf("JCL: failed to switch to TLS\n");
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
	else if (!strcmp(tree->name, "stream:error"))
	{
	}
	else if (!strcmp(tree->name, "failure"))
	{
		if (tree->child)
			Con_Printf("JCL: Failure: %s\n", tree->child->name);
		else
			Con_Printf("JCL: Unknown failure\n");
		XML_Destroy(tree);
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
		char *from;
		char *to;
		char *id;

		id = XML_GetParameter(tree, "id", "");
		from = XML_GetParameter(tree, "from", "");
		to = XML_GetParameter(tree, "to", "");

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
					int idletime = 0;
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
/*				else if (from && !strcmp(ot->xmlns, "jabber:iq:last"))
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
				int idletime = 0;
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

				Con_Print("Unsupported iq get\n");
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
				JCL_ParseJingle(jcl, c, from, id);
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
					if (!iq->callback(jcl, !strcmp(f, "error")?NULL:tree))
					{
						Con_Print("Invalid iq result\n");
						XML_ConPrintTree(tree, 0);
					}
				}
				free(iq);
			}
			else
			{
				Con_Print("Unrecognised iq result\n");
				XML_ConPrintTree(tree, 0);
			}
		}
		
		if (unparsable)
		{
			unparsable = false;
			Con_Print("Unrecognised iq type\n");
			XML_ConPrintTree(tree, 0);
		}
	}
	else if (!strcmp(tree->name, "message"))
	{
		f = XML_GetParameter(tree, "from", NULL);

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
					Con_SubPrintf(f, "%s: %s\n", f, ot->body);
				else
					Con_Print(ot->body);

				if (BUILTINISVALID(LocalSound))
					LocalSound("misc/talk.wav");
			}

			if (unparsable)
			{
				unparsable = false;
				if (jcl->streamdebug)
				{
					Con_Print("Received a message without a body\n");
					XML_ConPrintTree(tree, 0);
				}
			}
		}
	}
	else if (!strcmp(tree->name, "presence"))
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
		char *server;

		if (quake && !strcmp(quake->xmlns, "fteqw.com:game"))
		{
			serverip = XML_GetParameter(quake, "serverip", NULL);
			servermap = XML_GetParameter(quake, "servermap", NULL);
		}

		if (type && !strcmp(type, "subscribe"))
		{
			Con_Printf("^[[%s]\\xmpp\\%s^] wants to be your friend! ^[[Authorize]\\xmppauth\\%s^] ^[[Deny]\\xmppdeny\\%s^]\n", from, from, from, from);
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
					Q_strlcpy(bres->bstatus, "offline", sizeof(bres->bstatus));
				else
					Q_strlcpy(bres->bstatus, (show && *show->body)?show->body:"present", sizeof(bres->bstatus));

				if (bres->servertype == 2)
					Con_Printf("^[[%s]\\xmpp\\%s^] is now ^[[Playing Quake - %s]\\xmppjoin\\%s^]\n", buddy->name, from, bres->server, from);
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
		Con_Printf("JCL: Input corrupt, urecognised, or unusable. Disconnecting.");
		return JCL_KILL;
	}

	return JCL_CONTINUE;
}

void JCL_CloseConnection(jclient_t *jcl)
{
	Con_Printf("JCL: Disconnected from %s@%s\n", jcl->username, jcl->domain);
	JCL_AddClientMessageString(jcl, "</stream:stream>");
	Net_Close(jcl->socket);
	free(jcl);
}

//can be polled for server address updates
void JCL_GeneratePresence(qboolean force)
{
	int dummystat;
	char serveraddr[1024*16];
	char servermap[1024*16];
	//get the last server address

	serveraddr[0] = 0;
	servermap[0] = 0;

	if (!Cvar_GetFloat("xmpp_nostatus"))
	{
		if (Cvar_GetFloat("sv.state"))
		{
			Cvar_GetString("sv.mapname", servermap, sizeof(servermap));
		}
		else
		{
			if (!Cvar_GetString("cl_serveraddress", serveraddr, sizeof(serveraddr)))
				serveraddr[0] = 0;
			if (BUILTINISVALID(CL_GetStats))
			{
				//if we can't get any stats, its because we're not actually on the server.
				if (!CL_GetStats(0, &dummystat, 1))
					serveraddr[0] = 0;
			}
		}
	}

	if (force || strcmp(jclient->curquakeserver, *servermap?servermap:serveraddr))
	{
		char caps[256];
		char *caphash;
		Q_strlcpy(jclient->curquakeserver, *servermap?servermap:serveraddr, sizeof(jclient->curquakeserver));

		//note: ext='voice-v1 camera-v1 video-v1' is some legacy nonsense, and is required for voice calls with googletalk clients or something stupid like that
		Q_snprintf(caps, sizeof(caps), "<c xmlns='http://jabber.org/protocol/caps' hash='sha-1' node='http://fteqw.com/ftexmppplugin' ver='%s'/>", buildcapshash());

		if (!*jclient->curquakeserver)
			JCL_AddClientMessagef(jclient,
					"<presence>"
						"%s"
					"</presence>", caps);
		else if (*servermap)	//if we're running a server, say so
			JCL_AddClientMessagef(jclient, 
						"<presence>"
							"<quake xmlns='fteqw.com:game' servermap='%s'/>"
							"%s"
						"</presence>"
						, servermap, caps);
		else	//if we're connected to a server, say so
			JCL_AddClientMessagef(jclient, 
						"<presence>"
							"<quake xmlns='fteqw.com:game' serverip='%s' />"
							"%s"
						"</presence>"
				,jclient->curquakeserver, caps);
	}
}

//functions above this line allow connections to multiple servers.
//it is just the control functions that only allow one server.

qintptr_t JCL_Frame(qintptr_t *args)
{
	int stat = JCL_CONTINUE;
	if (jclient)
	{
		if (jclient->connected)
		{
			JCL_GeneratePresence(false);
		}

		while(stat == JCL_CONTINUE)
			stat = JCL_ClientFrame(jclient);
		if (stat == JCL_KILL)
		{
			JCL_CloseConnection(jclient);

			jclient = NULL;
		}

		JCL_FlushOutgoing(jclient);
	}
	return 0;
}

void JCL_WriteConfig(void)
{
	if (jclient->connected)
	{
		qhandle_t config;
		FS_Open("**plugconfig", &config, 2);
		if (config >= 0)
		{
			char buffer[8192];
			Q_snprintf(buffer, sizeof(buffer), "%i \"%s\" \"%s@%s\" \"%s\"\n",
				jclient->tlsconnect, jclient->server, jclient->username, jclient->domain, jclient->password);
			FS_Write(config, buffer, strlen(buffer));
			FS_Close(config);
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
		len = FS_Open("**plugconfig", &config, 1);
		if (config >= 0)
		{
			if (len >= sizeof(buf))
				len = sizeof(buf)-1;
			buf[len] = 0;
			FS_Read(config, buf, len);
			FS_Close(config);

			line = COM_ParseOut(line, tls, sizeof(tls));
			line = COM_ParseOut(line, server, sizeof(server));
			line = COM_ParseOut(line, account, sizeof(account));
			line = COM_ParseOut(line, password, sizeof(password));

			oldtls = atoi(tls);

			jclient = JCL_Connect(server, oldtls?5223:5222, oldtls, account, password);
		}
	}
}
void JCL_PrintBuddyList(char *console, jclient_t *jcl, qboolean all)
{
	buddy_t *b;
	bresource_t *r;
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
				if (r->servertype == 2)
					Con_SubPrintf(console, "    ^[[%s]\\xmpp\\%s/%s^]: ^[[Playing Quake - %s]\\xmppjoin\\%s/%s^]\n", r->resource, b->accountdomain, r->resource, r->server, b->accountdomain, r->resource);
				else if (r->servertype)
					Con_SubPrintf(console, "    ^[[%s]\\xmpp\\%s/%s^]: ^[[Playing Quake - %s]\\observe\\%s^]\n", r->resource, b->accountdomain, r->resource, r->server, r->server);
				else if (*r->fstatus)
					Con_SubPrintf(console, "    ^[[%s]\\xmpp\\%s/%s^]: %s - %s\n", r->resource, b->accountdomain, r->resource, r->bstatus, r->fstatus);
				else
					Con_SubPrintf(console, "    ^[[%s]\\xmpp\\%s/%s^]: %s\n", r->resource, b->accountdomain, r->resource, r->bstatus);
			}
		}
		else	//only one resource
		{
			r = b->resources;
			if (!strcmp(r->server, "-"))
				Con_SubPrintf(console, "^[[%s]\\xmpp\\%s/%s^]: ^[[Playing Quake]\\xmppjoin\\%s/%s^]\n", b->name, b->accountdomain, r->resource, b->accountdomain, r->resource);
			else if (*r->server)
				Con_SubPrintf(console, "^[[%s]\\xmpp\\%s/%s^]: ^[[Playing Quake - %s]\\observe\\%s^]\n", b->name, b->accountdomain, r->resource, r->server, r->server);
			else if (*r->fstatus)
				Con_SubPrintf(console, "^[[%s]\\xmpp\\%s/%s^]: %s - %s\n", b->name, b->accountdomain, r->resource, r->bstatus, r->fstatus);
			else
				Con_SubPrintf(console, "^[[%s]\\xmpp\\%s/%s^]: %s\n", b->name, b->accountdomain, r->resource, r->bstatus);
		}
	}
}

void JCL_SendMessage(jclient_t *jcl, char *to, char *msg)
{
	char markup[256];
	char *d;
	buddy_t *b;
	bresource_t *br;
	JCL_FindBuddy(jcl, to, &b, &br);
	if (br)
		JCL_AddClientMessagef(jcl, "<message to='%s/%s'><body>", b->accountdomain, br->resource);
	else
		JCL_AddClientMessagef(jcl, "<message to='%s'><body>", b->accountdomain);
	JCL_AddClientMessage(jcl, markup, XML_Markup(msg, markup, sizeof(markup)) - markup);
	JCL_AddClientMessageString(jcl, "</body></message>");
	Con_SubPrintf(b->name, "%s: "COLOURYELLOW"%s\n", ">>", msg);
}


void JCL_Command(char *console)
{
	char imsg[8192];
	char arg[6][256];
	char *msg;
	int i;

	Cmd_Args(imsg, sizeof(imsg));

	msg = imsg;
	for (i = 0; i < 6; i++)
	{
		if (!msg)
			continue;
		msg = COM_ParseOut(msg, arg[i], sizeof(arg[i]));
	}

	if (*arg[0] == '/')
	{
		if (!strcmp(arg[0]+1, "tlsopen") || !strcmp(arg[0]+1, "tlsconnect"))
		{	//tlsconnect is 'old'.
			if (!*arg[1])
			{
				Con_TrySubPrint(console, "tlsopen [server] [account] [password]\n");
				return;
			}

			if (jclient)
			{
				Con_TrySubPrint(console, "You are already connected\nPlease /quit first\n");
				return;
			}
			if (!*arg[1])
			{
				Con_SubPrintf(console, "%s <server[:port]> <account[@domain]> <password>\n", arg[0]+1);
				return;
			}
			jclient = JCL_Connect(arg[1], 5223, true, arg[2], arg[3]);
			if (!jclient)
			{
				Con_TrySubPrint(console, "Connect failed\n");
				return;
			}
		}
		else if (!strcmp(arg[0]+1, "open") || !strcmp(arg[0]+1, "connect"))
		{
			if (!*arg[1])
			{
				Con_TrySubPrint(console, "open [server] [account] [password]\n");
				return;
			}

			if (jclient)
			{
				Con_TrySubPrint(console, "You are already connected\nPlease /quit first\n");
				return;
			}
			if (!*arg[1])
			{
				Con_SubPrintf(console, "%s <server[:port]> <account[@domain]> <password>\n", arg[0]+1);
				return;
			}
			jclient = JCL_Connect(arg[1], 5222, false, arg[2], arg[3]);
			if (!jclient)
			{
				Con_TrySubPrint(console, "Connect failed\n");
				return;
			}
		}
		else if (!strcmp(arg[0]+1, "help")) 
		{
			Con_TrySubPrint(console, "^[/" COMMANDPREFIX " /tlsconnect XMPPSERVER USERNAME@DOMAIN PASSWORD^]\n");
			if (BUILTINISVALID(Net_SetTLSClient))
				Con_Printf("for example: ^[/" COMMANDPREFIX " /tlsconnect talk.google.com myusername@gmail.com mypassword^]\n"
				"Note that this info will be used the next time you start quake.\n");

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
			Con_TrySubPrint(console, 	"^[/" COMMANDPREFIX " /msg ACCOUNTNAME your message goes here^]\n"
										"Sends a message to the named person. If given a resource postfix, your message will be sent only to that resource.\n");
			Con_TrySubPrint(console, 	"If no arguments, will print out your friends list. If no /command is used, the arguments will be sent as a message to the person you last sent a message to.\n");
		}
		else if (!jclient)
		{
			Con_SubPrintf(console, "You are not connected. Cannot %s\n", arg[0]);
		}
		else if (!strcmp(arg[0]+1, "quit"))
		{
			//disconnect from the xmpp server.
			JCL_CloseConnection(jclient);
			jclient = NULL;
		}
		else if (!strcmp(arg[0]+1, "blist"))
		{
			//print out a full list of everyone, even those offline.
			JCL_PrintBuddyList(console, jclient, true);
		}
		else if (!strcmp(arg[0]+1, "clear"))
		{
			//just clears the current console.
			if (*console)
			{
				Con_Destroy(console);
				Con_TrySubPrint(console, "");
				Con_SetActive(console);
			}
			else
				Cmd_AddText("\nclear\n", true);
		}
		else if (!strcmp(arg[0]+1, "msg"))
		{
			//FIXME: validate the dest. deal with xml markup in dest.
			buddy_t *b;
			bresource_t *br;
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
			JCL_Join(jclient, arg[1]);
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
			buddy_t *b;
			bresource_t *br;
			msg = imsg;

			if (!*msg)
			{
				if (!*console)
				{
					JCL_PrintBuddyList(console, jclient, false);
					Con_TrySubPrint(console, "For help, type \"^[/" COMMANDPREFIX " /help^]\"\n");
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
