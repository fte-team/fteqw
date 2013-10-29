#include "xmpp.h"
#ifdef JINGLE
static struct c2c_s *JCL_JingleCreateSession(jclient_t *jcl, char *with, bresource_t *bres, qboolean creator, char *sid, int method, int mediatype)
{
	struct icestate_s *ice = NULL;
	struct c2c_s *c2c;
	char generatedname[64];
	char stunhost[256];

	if (!bres)
		return NULL;
	
	if (piceapi)
		ice = piceapi->ICE_Create(NULL, sid, with, method, mediatype);
	if (ice)
	{
		piceapi->ICE_Get(ice, "sid", generatedname, sizeof(generatedname));
		sid = generatedname;

		if (creator)
		{
			//note: the engine will ignore codecs it does not support.
			piceapi->ICE_Set(ice, "codec96", "speex@16000");	//wide
			piceapi->ICE_Set(ice, "codec97", "speex@8000");		//narrow
			piceapi->ICE_Set(ice, "codec98", "speex@32000");	//ultrawide
			piceapi->ICE_Set(ice, "codec99", "opus");
		}
	}
	else
		return NULL;	//no way to get the local ip otherwise, which means things won't work proper

	c2c = malloc(sizeof(*c2c) + strlen(sid));
	memset(c2c, 0, sizeof(*c2c));
	c2c->next = jcl->c2c;
	jcl->c2c = c2c;
	strcpy(c2c->sid, sid);

	c2c->peercaps = bres->caps;
	c2c->mediatype = mediatype;
	c2c->creator = creator;
	c2c->method = method;

	//query dns to see if there's a stunserver hosted by the same domain
	//nslookup -querytype=SRV _stun._udp.example.com
	//for msn, live.com has one, messanger.live.com has one, but messenger.live.com does NOT. seriously, the typo has more services.  wtf microsoft?
	//google doesn't provide a stun srv entry
	//facebook doesn't provide a stun srv entry
	Q_snprintf(stunhost, sizeof(stunhost), "_stun._udp.%s", jcl->domain);
	if (NET_DNSLookup_SRV(stunhost, stunhost, sizeof(stunhost)))
		piceapi->ICE_Set(ice, "stunip", stunhost);
	else
	{
		//there is no real way to query stun servers from the xmpp server.
		//while there is some extdisco extension (aka: the 'standard' way), it has some huge big fat do-not-implement message (and googletalk does not implement it).
		//google provide their own jingleinfo extension. Which also has some huge fat 'do-not-implement' message. hardcoding the address should provide equivelent longevity. :(
		//google also don't provide stun srv records.
		//so we're basically screwed if we want to work with the googletalk xmpp service long term.
		//more methods are best, I suppose, but I'm lazy.
		//yes, hardcoding means that other services might 'borrow' googles' stun servers.
		piceapi->ICE_Set(ice, "stunport", "19302");
		piceapi->ICE_Set(ice, "stunip", "stun.l.google.com");
	}

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


static void JCL_PopulateAudioDescription(xmltree_t *description, struct icestate_s *ice)
{
	xmltree_t *payload;
	int i;
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
			XML_AddParameter(payload, "name", "speex");
		}
		else if (!strcmp(codecname, "speex@16000"))
		{	//speex wideband
			payload = XML_CreateNode(description, "payload-type", "", "");
			XML_AddParameter(payload, "channels", "1");
			XML_AddParameter(payload, "clockrate", "16000");
			XML_AddParameter(payload, "id", argn+5);
			XML_AddParameter(payload, "name", "speex");
		}
		else if (!strcmp(codecname, "speex@32000"))
		{	//speex ultrawideband
			payload = XML_CreateNode(description, "payload-type", "", "");
			XML_AddParameter(payload, "channels", "1");
			XML_AddParameter(payload, "clockrate", "32000");
			XML_AddParameter(payload, "id", argn+5);
			XML_AddParameter(payload, "name", "speex");
		}
		else if (!strcmp(codecname, "opus"))
		{	//opus codec.
			payload = XML_CreateNode(description, "payload-type", "", "");
			XML_AddParameter(payload, "channels", "1");
			XML_AddParameter(payload, "id", argn+5);
			XML_AddParameter(payload, "name", "opus");
		}
	}
}

/*
sends a jingle message to the peer.
action should be one of multiple things:
session-terminate	- totally not acceptable. this also closes the c2c
session-accept		- details are okay. this also begins ice polling (on iq ack, once we're sure the peer got our message)

(internally generated) transport-replace	- details are okay, except we want a different transport method.
*/
static qboolean JCL_JingleSend(jclient_t *jcl, struct c2c_s *c2c, char *action)
{
	qboolean result;
	xmltree_t *jingle;
	struct icestate_s *ice = c2c->ice;
	qboolean wasaccept = false;
	int transportmode = ICEM_ICE;
	int cap = c2c->peercaps;

	if (!ice)
		action = "session-terminate";

#ifdef VOIP_LEGACY
	if ((cap & CAP_FUGOOG_VOICE) && (cap & CAP_FUGOOG_SESSION) && !(cap & CAP_VOICE))
	{
		//legacy crap for google compatibility.
		if (!strcmp(action, "session-initiate"))
		{
			xmltree_t *session = XML_CreateNode(NULL, "session", "http://www.google.com/session", "");
			xmltree_t *description = XML_CreateNode(session, "description", "http://www.google.com/session/phone", "");

			XML_AddParameter(session, "id", c2c->sid);
			XML_AddParameter(session, "initiator", jcl->jid);
			XML_AddParameter(session, "type", "initiate");
			JCL_PopulateAudioDescription(description, ice);

			JCL_SendIQNode(jcl, NULL, "set", c2c->with, session, true);
		}
		else if (!strcmp(action, "session-accept"))
		{
			xmltree_t *session = XML_CreateNode(NULL, "session", "http://www.google.com/session", "");
			xmltree_t *description = XML_CreateNode(session, "description", "http://www.google.com/session/phone", "");

			XML_AddParameter(session, "id", c2c->sid);
			XML_AddParameter(session, "initiator", c2c->with);
			XML_AddParameter(session, "type", "accept");
			JCL_PopulateAudioDescription(description, ice);

			JCL_SendIQNode(jcl, JCL_JingleAcceptAck, "set", c2c->with, session, true)->usrptr = c2c;
			c2c->accepted = true;
		}
		else if (!strcmp(action, "transport-info"))
		{
			struct icecandinfo_s *c;
			while ((c = piceapi->ICE_GetLCandidateInfo(ice)))
			{
				xmltree_t *session = XML_CreateNode(NULL, "session", "http://www.google.com/session", "");

				XML_AddParameter(session, "id", c2c->sid);
				XML_AddParameter(session, "initiator", c2c->creator?jcl->jid:c2c->with);
				XML_AddParameter(session, "type", "candidates");

				//one per message, apparently
				if (c)
				{
					char *ctypename[]={"local", "stun", "stun", "relay"};
					char *pref[]={"1", "0.9", "0.5", "0.2"};
					xmltree_t *candidate = XML_CreateNode(session, "candidate", "", "");
					char pwd[64];
					char uname[64];

					piceapi->ICE_Get(ice, "lufrag",  uname, sizeof(uname));
					piceapi->ICE_Get(ice, "lpwd",  pwd, sizeof(pwd));

					XML_AddParameter(candidate, "address", c->addr);
					XML_AddParameteri(candidate, "port", c->port);
					XML_AddParameter(candidate, "name", (c->component==1)?"rtp":"rtcp");
					XML_AddParameter(candidate, "username", uname);
					XML_AddParameter(candidate, "password", pwd);
					XML_AddParameter(candidate, "preference", pref[c->type]);	//FIXME: c->priority
					XML_AddParameter(candidate, "protocol", "udp");
					XML_AddParameter(candidate, "type", ctypename[c->type]);
					XML_AddParameteri(candidate, "generation", c->generation);
					XML_AddParameteri(candidate, "network", c->network);
				}

				JCL_SendIQNode(jcl, NULL, "set", c2c->with, session, true);
			}
		}
		else if (!strcmp(action, "session-terminate"))
		{
			struct c2c_s **link;
			xmltree_t *session = XML_CreateNode(NULL, "session", "http://www.google.com/session", "");

			XML_AddParameter(session, "id", c2c->sid);
			XML_AddParameter(session, "initiator", c2c->creator?jcl->jid:c2c->with);
			XML_AddParameter(session, "type", "terminate");
			JCL_SendIQNode(jcl, NULL, "set", c2c->with, session, true);

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

			return false;
		}
		else
		{
			return false;
		}
		return true;
	}
#endif

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

				JCL_PopulateAudioDescription(description, ice);
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
	buddy_t *b;
	bresource_t *br;
	if (!jcl)
		return;

	JCL_FindBuddy(jcl, target, &b, &br);
	if (!br)
		br = b->defaultresource;
	if (!br)
		br = b->resources;

	if (!strchr(target, '/'))
	{
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
				char convolink[512], hanguplink[512];
				c2c = JCL_JingleCreateSession(jcl, target, br, true, sid, DEFAULTICEMODE, ((protocol == ICEP_INVALID)?ICEP_QWCLIENT:protocol));
				JCL_JingleSend(jcl, c2c, "session-initiate");

				JCL_GenLink(jcl, convolink, sizeof(convolink), NULL, target, NULL, NULL, "%s", target);
				JCL_GenLink(jcl, hanguplink, sizeof(hanguplink), "jdeny", target, NULL, c2c->sid, "%s", "Hang Up");
				Con_SubPrintf(b->name, "%s %s %s.\n",  protocol==ICEP_VOICE?"Calling":"Requesting session with", convolink, hanguplink);
			}
			else
				Con_SubPrintf(b->name, "That session has expired.\n");
		}
		else if (c2c->creator)
		{
			char convolink[512];
			//resend initiate if they've not acked it... I dunno...
			JCL_JingleSend(jcl, c2c, "session-initiate");
			JCL_GenLink(jcl, convolink, sizeof(convolink), NULL, target, NULL, NULL, "%s", target);
			Con_SubPrintf(b->name, "Restarting session with %s.\n", convolink);
		}
		else if (c2c->accepted)
			Con_SubPrintf(b->name, "That session was already accepted.\n");
		else
		{
			char convolink[512];
			JCL_JingleSend(jcl, c2c, "session-accept");
			JCL_GenLink(jcl, convolink, sizeof(convolink), NULL, target, NULL, NULL, "%s", target);
			Con_SubPrintf(b->name, "Accepting session from %s.\n", convolink);
		}
	}
	else
	{
		if (c2c)
		{
			char convolink[512];
			JCL_JingleSend(jcl, c2c, "session-terminate");
			JCL_GenLink(jcl, convolink, sizeof(convolink), NULL, target, NULL, NULL, "%s", target);
			Con_SubPrintf(b->name, "Terminating session with %s.\n", convolink);
		}
		else
			Con_SubPrintf(b->name, "That session has already expired.\n");
	}
}

static void JCL_JingleParsePeerPorts(jclient_t *jcl, struct c2c_s *c2c, xmltree_t *inj, char *from, char *sid)
{
	xmltree_t *incontent = XML_ChildOfTree(inj, "content", 0);
	xmltree_t *intransport = XML_ChildOfTree(incontent, "transport", 0);
	xmltree_t *incandidate;
	struct icecandinfo_s rem;
	int i;

	if (!c2c->sid)
		return;

	if (!intransport)
		return;

	if (!c2c->ice)
		return;

	if (strcmp(c2c->with, from) || strcmp(c2c->sid, sid))
	{
		Con_Printf("%s is trying to mess with our connections...\n", from);
		return;
	}

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
#ifdef VOIP_LEGACY
static void JCL_JingleParsePeerPorts_GoogleSession(jclient_t *jcl, struct c2c_s *c2c, xmltree_t *inj, char *from, char *sid)
{
	xmltree_t *incandidate;
	struct icecandinfo_s rem;
	int i;

	if (strcmp(c2c->with, from) || strcmp(c2c->sid, sid))
	{
		Con_Printf("%s is trying to mess with our connections...\n", from);
		return;
	}

	if (!c2c->sid)
		return;

	if (!c2c->ice)
		return;

	for (i = 0; (incandidate = XML_ChildOfTree(inj, "candidate", i)); i++)
	{
		char *s;
		memset(&rem, 0, sizeof(rem));
		Q_strlcpy(rem.addr, XML_GetParameter(incandidate, "address", ""), sizeof(rem.addr));
//		Q_strlcpy(rem.candidateid, XML_GetParameter(incandidate, "id", ""), sizeof(rem.candidateid));

		s = XML_GetParameter(incandidate, "type", "");
		if (s && !strcmp(s, "stun"))
			rem.type = ICE_SRFLX;
		else if (s && !strcmp(s, "stun"))
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
		s = XML_GetParameter(incandidate, "name", "rtp");
		if (strcmp(s, "rtp"))
			rem.component = 1;
		else if (strcmp(s, "rtcp"))
			rem.component = 2;
		else
			rem.component = 0;
		s = XML_GetParameter(incandidate, "protocol", "udp");
		if (s && !strcmp(s, "udp"))
			rem.transport = 0;
		else
			rem.transport = 0;
		piceapi->ICE_AddRCandidateInfo(c2c->ice, &rem);
	}
}
static qboolean JCL_JingleHandleInitiate_GoogleSession(jclient_t *jcl, xmltree_t *inj, char *from)
{
	xmltree_t *indescription = XML_ChildOfTree(inj, "description", 0);
	char *descriptionxmlns = indescription?indescription->xmlns:"";
	char *sid = XML_GetParameter(inj, "id", "");

	xmltree_t *jingle;
	struct icestate_s *ice;
	qboolean accepted = false;
	enum icemode_e imode;
	char *response = "terminate";
	char *offer = "pwn you";
	char *autocvar = "xmpp_autoaccepthax";
	char *initiator;

	struct c2c_s *c2c = NULL;
	int mt = ICEP_INVALID;
	buddy_t *b;
	bresource_t *br;

	//FIXME: add support for session forwarding so that we might forward the connection to the real server. for now we just reject it.
	initiator = XML_GetParameter(inj, "initiator", "");
	if (strcmp(initiator, from))
		return false;

	if (indescription && !strcmp(descriptionxmlns, "http://www.google.com/session/phone"))
	{
		mt = ICEP_VOICE;
		offer = "is trying to call you";
		autocvar = "xmpp_autoacceptvoice";
	}
	if (mt == ICEP_INVALID)
		return false;

	JCL_FindBuddy(jcl, from, &b, &br);

	//FIXME: if both people try to establish a connection to the other simultaneously, the higher session id is meant to be canceled, and the lower accepted automagically.

	c2c = JCL_JingleCreateSession(jcl, from, br, false,
		sid,
		ICEM_ICE,
		mt
		);
	if (!c2c)
		return false;
	c2c->peercaps = CAP_FUGOOG_VOICE|CAP_FUGOOG_SESSION;

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
			//note: the engine will ignore codecs it does not support, returning false.
			if (!stricmp(name, "SPEEX"))
			{
				Q_snprintf(parm, sizeof(parm), "codec%i", atoi(id));
				Q_snprintf(val, sizeof(val), "speex@%i", atoi(clock));
				okay |= piceapi->ICE_Set(c2c->ice, parm, val);
			}
			else if (!stricmp(name, "OPUS"))
			{
				Q_snprintf(parm, sizeof(parm), "codec%i", atoi(id));
				okay |= piceapi->ICE_Set(c2c->ice, parm, "opus");
			}
		}
		//don't do it if we couldn't successfully set any codecs, because the engine doesn't support the ones that were listed, or something.
		//we really ought to give a reason, but we're rude.
		if (!okay)
		{
			char convolink[512];
			JCL_JingleSend(jcl, c2c, "session-terminate");
			JCL_GenLink(jcl, convolink, sizeof(convolink), NULL, from, NULL, NULL, "%s", b->name);
			Con_SubPrintf(b->name, "%s does not support any compatible audio codecs, and is unable to call you.\n", convolink);
			return false;
		}
	}

	if (c2c->mediatype != ICEP_INVALID)
	{
		char convolink[512];
		JCL_GenLink(jcl, convolink, sizeof(convolink), NULL, from, NULL, NULL, "%s", b->name);
		if (!pCvar_GetFloat(autocvar))
		{
			char authlink[512];
			char denylink[512];
			JCL_GenLink(jcl, authlink, sizeof(authlink), "jauth", from, NULL, sid, "%s", "Accept");
			JCL_GenLink(jcl, denylink, sizeof(denylink), "jdeny", from, NULL, sid, "%s", "Reject");

			//show a prompt for it, send the reply when the user decides.
			Con_SubPrintf(b->name,
					"%s %s. %s %s\n", convolink, offer, authlink, denylink);
			pCon_SetActive(b->name);
			return true;
		}
		else
		{
			Con_SubPrintf(b->name, "Auto-accepting session from %s\n", convolink);
			response = "session-accept";
		}
	}

	JCL_JingleSend(jcl, c2c, response);
	return true;
}
#endif
static qboolean JCL_JingleHandleInitiate(jclient_t *jcl, xmltree_t *inj, char *from)
{
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
	buddy_t *b;
	bresource_t *br;

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
			offer = "wants to join your game";
			autocvar = "xmpp_autoacceptjoins";
		}
		else if (!strcmp(host, "me"))
		{
			mt = ICEP_QWCLIENT;
			offer = "wants to invite you to thier game";
			autocvar = "xmpp_autoacceptinvites";
		}
	}
	if (incontent && !strcmp(descriptionmedia, "audio") && !strcmp(descriptionxmlns, "urn:xmpp:jingle:apps:rtp:1"))
	{
		mt = ICEP_VOICE;
		offer = "is trying to call you";
		autocvar = "xmpp_autoacceptvoice";
	}
	if (mt == ICEP_INVALID)
		return false;

	JCL_FindBuddy(jcl, from, &b, &br);

	//FIXME: if both people try to establish a connection to the other simultaneously, the higher session id is meant to be canceled, and the lower accepted automagically.

	c2c = JCL_JingleCreateSession(jcl, from, br, false,
		sid,
		strcmp(transportxmlns, "urn:xmpp:jingle:transports:raw-udp:1")?ICEM_ICE:ICEM_RAW,
		mt
		);
	if (!c2c)
		return false;

	c2c->peercaps &= ~(CAP_FUGOOG_SESSION|CAP_FUGOOG_VOICE);

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
			//note: the engine will ignore codecs it does not support, returning false.
			if (!stricmp(name, "SPEEX"))
			{
				Q_snprintf(parm, sizeof(parm), "codec%i", atoi(id));
				Q_snprintf(val, sizeof(val), "speex@%i", atoi(clock));
				okay |= piceapi->ICE_Set(c2c->ice, parm, val);
			}
			else if (!stricmp(name, "OPUS"))
			{
				Q_snprintf(parm, sizeof(parm), "codec%i", atoi(id));
				okay |= piceapi->ICE_Set(c2c->ice, parm, "opus");
			}
		}
		//don't do it if we couldn't successfully set any codecs, because the engine doesn't support the ones that were listed, or something.
		//we really ought to give a reason, but we're rude.
		if (!okay)
		{
			char convolink[512];
			JCL_JingleSend(jcl, c2c, "session-terminate");
			JCL_GenLink(jcl, convolink, sizeof(convolink), NULL, from, NULL, NULL, "%s", b->name);
			Con_SubPrintf(b->name, "%s does not support any compatible audio codecs, and is unable to call you.\n", convolink);
			return false;
		}
	}

	JCL_JingleParsePeerPorts(jcl, c2c, inj, from, sid);

	if (c2c->mediatype != ICEP_INVALID)
	{
		char convolink[512];
		JCL_GenLink(jcl, convolink, sizeof(convolink), NULL, from, NULL, NULL, "%s", b->name);
		if (!pCvar_GetFloat(autocvar))
		{
			char authlink[512];
			char denylink[512];
			JCL_GenLink(jcl, authlink, sizeof(authlink), "jauth", from, NULL, sid, "%s", "Accept");
			JCL_GenLink(jcl, denylink, sizeof(denylink), "jdeny", from, NULL, sid, "%s", "Reject");

			//show a prompt for it, send the reply when the user decides.
			Con_SubPrintf(b->name,
					"%s %s. %s %s\n", convolink, offer, authlink, denylink);
			pCon_SetActive(b->name);
			return true;
		}
		else
		{
			Con_SubPrintf(b->name, "Auto-accepting session from %s\n", convolink);
			response = "session-accept";
		}
	}

	JCL_JingleSend(jcl, c2c, response);
	return true;
}

static qboolean JCL_JingleHandleSessionTerminate(jclient_t *jcl, xmltree_t *tree, struct c2c_s *c2c, struct c2c_s **link, buddy_t *b)
{
	xmltree_t *reason = XML_ChildOfTree(tree, "reason", 0);
	if (!c2c)
	{
		Con_Printf("Received session-terminate without an active session\n");
		return false;
	}

	if (reason && reason->child)
		Con_SubPrintf(b->name, "Session ended: %s\n", reason->child->name);
	else
		Con_SubPrintf(b->name, "Session ended\n");

	//unlink it
	for (link = &jcl->c2c; *link; link = &(*link)->next)
	{
		if (*link == c2c)
		{
			*link = c2c->next;
			break;
		}
	}

//	XML_ConPrintTree(tree, 0);

	if (c2c->ice)
		piceapi->ICE_Close(c2c->ice);
	free(c2c);
	return true;
}
static qboolean JCL_JingleHandleSessionAccept(jclient_t *jcl, xmltree_t *tree, char *from, struct c2c_s *c2c, buddy_t *b)
{
	//peer accepted our session
	//make sure it actually was ours, and not theirs. sneaky sneaky.
	//will generally contain some port info.
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
		Con_SubPrintf(b->name, "Session Accepted!\n");
//			XML_ConPrintTree(tree, 0);

		JCL_JingleParsePeerPorts(jcl, c2c, tree, from, XML_GetParameter(tree, "sid", ""));
		c2c->accepted = true;

		//if we didn't error out, the ICE stuff is meant to start sending handshakes/media as soon as the connection is accepted
		if (c2c->ice)
			piceapi->ICE_Set(c2c->ice, "state", STRINGIFY(ICE_CONNECTING));
	}
	return true;
}
#ifdef VOIP_LEGACY
qboolean JCL_HandleGoogleSession(jclient_t *jcl, xmltree_t *tree, char *from, char *id)
{
	char *sid = XML_GetParameter(tree, "id", "");
	char *type = XML_GetParameter(tree, "type", "");

	struct c2c_s *c2c = NULL, **link;
	buddy_t *b;

	//validate+find sender
	for (link = &jcl->c2c; *link; link = &(*link)->next)
	{
		if (!strcmp((*link)->sid, sid))
		{
			c2c = *link;
			if (!c2c->accepted)
				break;
		}
	}
	JCL_FindBuddy(jcl, from, &b, NULL);
	if (c2c && strcmp(c2c->with, from))
	{
		Con_Printf("%s is trying to mess with our connections...\n", from);
		return false;
	}

	XML_ConPrintTree(tree, 0);

	if (!strcmp(type, "accept"))
	{
		if (!JCL_JingleHandleSessionAccept(jcl, tree, from, c2c, b))
			return false;
	}
	else if (!strcmp(type, "initiate"))
	{
		if (!JCL_JingleHandleInitiate_GoogleSession(jcl, tree, from))
			return false;
	}
	else if (!strcmp(type, "terminate"))
	{
		JCL_JingleHandleSessionTerminate(jcl, tree, c2c, link, b);
	}
	else if (!strcmp(type, "candidates"))
	{
		if (!c2c)
			return false;
		JCL_JingleParsePeerPorts_GoogleSession(jcl, c2c, tree, from, sid);
	}
	else
	{
		Con_Printf("Unknown google session action: %s\n", type);
//		XML_ConPrintTree(tree, 0);
		return false;
	}

	JCL_AddClientMessagef(jcl,
		"<iq type='result' to='%s' id='%s' />", from, id);
	return true;
}
#endif
enum
{
	JE_ACKNOWLEDGE,
	JE_UNSUPPORTED,
	JE_OUTOFORDER,
	JE_TIEBREAK,
	JE_UNKNOWNSESSION,
	JE_UNSUPPORTEDINFO
};
void JCL_JingleError(jclient_t *jcl, xmltree_t *tree, char *from, char *id, int type)
{
	switch(type)
	{
	case JE_ACKNOWLEDGE:
		JCL_AddClientMessagef(jcl,
			"<iq type='result' to='%s' id='%s' />", from, id);
		break;
	case JE_UNSUPPORTED:
		JCL_AddClientMessagef(jcl,
				"<iq id='%s' to='%s' type='error'>"
				  "<error type='cancel'>"
					"<bad-request xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>"
				  "</error>"
				"</iq>", id, from);
		break;
	case JE_UNKNOWNSESSION:
		JCL_AddClientMessagef(jcl,
				"<iq id='%s' to='%s' type='error'>"
				  "<error type='modify'>"
					"<item-not-found xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>"
					"<unknown-session xmlns='urn:xmpp:jingle:errors:1'/>"
				  "</error>"
				"</iq>", id, from);
		break;
	case JE_UNSUPPORTEDINFO:
		JCL_AddClientMessagef(jcl,
				"<iq id='%s' to='%s' type='error'>"
				  "<error type='modify'>"
					"<feature-not-implemented xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>"
					"<unsupported-info xmlns='urn:xmpp:jingle:errors:1'/>"
				  "</error>"
				"</iq>", id, from);
		break;
	}
}
qboolean JCL_ParseJingle(jclient_t *jcl, xmltree_t *tree, char *from, char *id)
{
	char *action = XML_GetParameter(tree, "action", "");
	char *sid = XML_GetParameter(tree, "sid", "");

	struct c2c_s *c2c = NULL, **link;
	buddy_t *b;

	for (link = &jcl->c2c; *link; link = &(*link)->next)
	{
		if (!strcmp((*link)->sid, sid))
		{
			c2c = *link;
			if (!c2c->accepted)
				break;
		}
	}

	JCL_FindBuddy(jcl, from, &b, NULL);

	//validate sender
	if (c2c && strcmp(c2c->with, from))
	{
		Con_Printf("%s is trying to mess with our connections...\n", from);
		return false;
	}

	//FIXME: transport-info, transport-replace
	if (!strcmp(action, "session-terminate"))
	{
		if (c2c)
		{
			JCL_JingleError(jcl, tree, from, id, JE_ACKNOWLEDGE);
			JCL_JingleHandleSessionTerminate(jcl, tree, c2c, link, b);
		}
		else
			JCL_JingleError(jcl, tree, from, id, JE_UNKNOWNSESSION);
	}
	else if (!strcmp(action, "content-accept"))
	{
		//response from content-add
		if (c2c)
			JCL_JingleError(jcl, tree, from, id, JE_UNSUPPORTED);
		else
			JCL_JingleError(jcl, tree, from, id, JE_UNKNOWNSESSION);
	}
	else if (!strcmp(action, "content-add"))
	{
		//FIXME: must send content-reject
		if (c2c)
			JCL_JingleError(jcl, tree, from, id, JE_UNSUPPORTED);
		else
			JCL_JingleError(jcl, tree, from, id, JE_UNKNOWNSESSION);
	}
	else if (!strcmp(action, "content-modify"))
	{
		//send an error to reject it
		JCL_JingleError(jcl, tree, from, id, JE_UNSUPPORTED);
	}
	else if (!strcmp(action, "content-reject"))
	{
		//response from content-add. an error until we actually generate content-adds...
		if (c2c)
			JCL_JingleError(jcl, tree, from, id, JE_UNSUPPORTED);
		else
			JCL_JingleError(jcl, tree, from, id, JE_UNKNOWNSESSION);
	}
	else if (!strcmp(action, "content-remove"))
	{
		if (c2c)
			JCL_JingleError(jcl, tree, from, id, JE_UNSUPPORTED);
		else
			JCL_JingleError(jcl, tree, from, id, JE_UNKNOWNSESSION);
	}
	else if (!strcmp(action, "description-info"))
	{
		//The description-info action is used to send informational hints about parameters related to the application type, such as the suggested height and width of a video display area or suggested configuration for an audio stream.
		//just ack and ignore it
		if (c2c)
			JCL_JingleError(jcl, tree, from, id, JE_UNSUPPORTED);
		else
			JCL_JingleError(jcl, tree, from, id, JE_UNKNOWNSESSION);
	}
	else if (!strcmp(action, "security-info"))
	{
		//The security-info action is used to send information related to establishment or maintenance of security preconditions.
		//no security mechanisms supported...
		if (c2c)
			JCL_JingleError(jcl, tree, from, id, JE_UNSUPPORTED);
		else
			JCL_JingleError(jcl, tree, from, id, JE_UNKNOWNSESSION);
	}
	else if (!strcmp(action, "session-info"))
	{
		if (tree->child)
			JCL_JingleError(jcl, tree, from, id, JE_UNSUPPORTEDINFO);
		else
			JCL_JingleError(jcl, tree, from, id, JE_ACKNOWLEDGE);	//serves as a ping.
	}

	else if (!strcmp(action, "transport-info"))
	{	//peer wants to add ports.
		if (c2c)
		{
			JCL_JingleError(jcl, tree, from, id, JE_ACKNOWLEDGE);
			JCL_JingleParsePeerPorts(jcl, c2c, tree, from, sid);
		}
		else
			JCL_JingleError(jcl, tree, from, id, JE_UNKNOWNSESSION);
	}
//FIXME: we need to add support for this to downgrade to raw if someone tries calling through a SIP gateway
	else if (!strcmp(action, "transport-replace"))
	{
		if (c2c)
		{
			if (1)
			{
				JCL_JingleError(jcl, tree, from, id, JE_ACKNOWLEDGE);
				JCL_JingleSend(jcl, c2c, "transport-reject");
			}
			else
			{
				JCL_JingleError(jcl, tree, from, id, JE_ACKNOWLEDGE);
				JCL_JingleParsePeerPorts(jcl, c2c, tree, from, sid);
				JCL_JingleSend(jcl, c2c, "transport-accept");
			}
		}
		else
			JCL_JingleError(jcl, tree, from, id, JE_UNKNOWNSESSION);
	}
	else if (!strcmp(action, "transport-reject"))
	{
		JCL_JingleError(jcl, tree, from, id, JE_ACKNOWLEDGE);
		JCL_JingleSend(jcl, c2c, "session-terminate");
	}
	else if (!strcmp(action, "session-accept"))
	{
		if (c2c)
		{
			//response from a message we sent.
			if (JCL_JingleHandleSessionAccept(jcl, tree, from, c2c, b))
				JCL_JingleError(jcl, tree, from, id, JE_ACKNOWLEDGE);
			else
				JCL_JingleError(jcl, tree, from, id, JE_OUTOFORDER);
		}
		else
			JCL_JingleError(jcl, tree, from, id, JE_UNKNOWNSESSION);
	}
	else if (!strcmp(action, "session-initiate"))
	{
//		Con_Printf("Peer initiating connection!\n");
//		XML_ConPrintTree(tree, 0);

		JCL_JingleError(jcl, tree, from, id, JE_ACKNOWLEDGE);

		JCL_JingleHandleInitiate(jcl, tree, from);
	}
	else
	{
		JCL_JingleError(jcl, tree, from, id, JE_UNSUPPORTED);
	}
	return true;
}
#endif