//Released under the terms of the gpl as this file uses a bit of quake derived code. All sections of the like are marked as such

#include "../plugin.h"
#include <time.h>

#define Q_strncpyz(o, i, l) do {strncpy(o, i, l-1);o[l-1]='\0';}while(0)

#define JCL_BUILD "2"


#define ARGNAMES ,sock,certhostname
BUILTINR(int, Net_SetTLSClient, (qhandle_t sock, const char *certhostname));
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
	#define COMMANDPREFIX "jabbercl"
	#define playsound(s)


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






void JCL_Command(void);

qintptr_t JCL_ExecuteCommand(qintptr_t *args)
{
	char cmd[256];
	Cmd_Argv(0, cmd, sizeof(cmd));
	if (!strcmp(cmd, COMMANDPREFIX))
	{
		JCL_Command();
		return true;
	}
	return false;
}

qintptr_t JCL_ConExecuteCommand(qintptr_t *args);

qintptr_t JCL_Frame(qintptr_t *args);

qintptr_t Plug_Init(qintptr_t *args)
{
	if (	Plug_Export("Tick", JCL_Frame) &&
		Plug_Export("ExecuteCommand", JCL_ExecuteCommand))
	{
		CHECKBUILTIN(Net_SetTLSClient);
		if (!BUILTINISVALID(Net_SetTLSClient))
			Con_Print("Jabber Client Plugin Loaded ^1without^7 TLS\n");
		else
			Con_Print("Jabber Client Plugin Loaded with TLS\n");

		if (!Plug_Export("ConExecuteCommand", JCL_ConExecuteCommand))
		{
			Con_Printf("Jabber client plugin in single-console mode\n");
			Con_TrySubPrint = Fallback_ConPrint;
		}
		else
			Con_TrySubPrint = Con_SubPrint;

		Cmd_AddCommand(COMMANDPREFIX);
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


typedef struct {
	char server[64];
	int port;

	qhandle_t socket;
	qhandle_t inlog;
	qhandle_t outlog;

	char bufferedinmessage[JCL_MAXMSGLEN+1];	//servers are required to be able to handle messages no shorter than a specific size.
												//which means we need to be able to handle messages when they get to us.
												//servers can still handle larger messages if they choose, so this might not be enough.
	int bufferedinammount;

	char defaultdest[256];

	char domain[256];
	char username[256];
	char password[256];
	char resource[256];

	int tagdepth;
	int openbracket;
	int instreampos;

	qboolean connected;	//fully on server and authed and everything.
	qboolean noplain;	//block plain-text password exposure
	qboolean issecure;	//tls enabled

	char curquakeserver[2048];
	char defaultnamespace[2048];	//should be 'jabber:client' or blank (and spammy with all the extra xmlns attribs)
} jclient_t;
jclient_t *jclient;

qintptr_t JCL_ConExecuteCommand(qintptr_t *args)
{
	if (!jclient)
	{
		char buffer[256];
		Cmd_Argv(0, buffer, sizeof(buffer));
		Con_SubPrint(buffer, "You were disconnected\n");
		return true;
	}
	Cmd_Argv(0, jclient->defaultdest, sizeof(jclient->defaultdest));
	JCL_Command();
	return true;
}

void JCL_AddClientMessage(jclient_t *jcl, char *msg, int datalen)
{
	Net_Send(jcl->socket, msg, datalen);	//FIXME: This needs rewriting to cope with errors.
	Con_SubPrintf("xmppout", COLOURYELLOW "%s \n",msg);
}
void JCL_AddClientMessageString(jclient_t *jcl, char *msg)
{
	JCL_AddClientMessage(jcl, msg, strlen(msg));
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

	jcl->noplain = true;

	*at = '\0';
	Q_strlcpy(jcl->username, account, sizeof(jcl->username));
	Q_strlcpy(jcl->domain, at+1, sizeof(jcl->domain));
	Q_strlcpy(jcl->password, password, sizeof(jcl->password));

	Q_strlcpy(jcl->resource, "Quake", sizeof(jcl->password));

	Con_Printf("Trying to connect\n");
	JCL_AddClientMessageString(jcl,
		"<?xml version='1.0' ?>"
		"<stream:stream to='");
	JCL_AddClientMessageString(jcl, jcl->domain);
	JCL_AddClientMessageString(jcl, "' xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams' version='1.0'>");

	return jcl;
}

typedef struct xmlparams_s {
	char name[64];
	char val[256];

	struct xmlparams_s *next;
} xmlparams_t;

typedef struct subtree_s {
	char name[64];
	char xmlns[64];			//namespace of the element
	char xmlns_dflt[64];	//default namespace of children
	char body[2048];

	xmlparams_t *params;

	struct subtree_s *child;
	struct subtree_s *sibling;
} xmltree_t;

void XML_Destroy(xmltree_t *t);

char *XML_ParameterOfTree(xmltree_t *t, char *paramname)
{
	xmlparams_t *p;
	for (p = t->params; p; p = p->next)
		if (!strcmp(p->name, paramname))
			return p->val;
	return NULL;
}

//fixme: we should accept+parse the default namespace
xmltree_t *XML_Parse(char *buffer, int *startpos, int maxpos, qboolean headeronly, char *defaultnamespace)
{
	xmlparams_t *p;
	xmltree_t *child;
	xmltree_t *ret;
	int bodypos;
	int pos;
	char *tagend;
	char *tagstart;
	char *ns;
	pos = *startpos;
	while (buffer[pos] >= '\0' && buffer[pos] <= ' ')
	{
		if (pos >= maxpos)
			break;
		pos++;
	}

	if (pos == maxpos)
	{
		*startpos = pos;
		return NULL;	//nothing anyway.
	}

	//expect a <

	if (buffer[pos] != '<')
	{
		Con_Printf("Missing open bracket\n");
		return NULL;	//should never happen
	}

	if (buffer[pos+1] == '/')
	{
		Con_Printf("Unexpected close tag.\n");
		return NULL;	//err, terminating a parent tag
	}

	tagend = strchr(buffer+pos, '>');
	if (!tagend)
	{
		Con_Printf("Missing close bracket\n");
		return NULL;	//should never happen
	}
	*tagend = '\0';
	tagend++;


	//assume no nulls in the tag header.

	tagstart = buffer+pos+1;
	tagstart = COM_Parse(tagstart);
	if (!tagstart)
	{
		Con_Printf("EOF on tag name\n");
		return NULL;
	}

	pos = tagend - buffer;

	ret = malloc(sizeof(xmltree_t));
	memset(ret, 0, sizeof(*ret));

	ns = strchr(com_token, ':');
	if (ns)
	{
		*ns = 0;
		ns++;

		memcpy(ret->xmlns, "xmlns:", 6);
		Q_strlcpy(ret->xmlns+6, com_token, sizeof(ret->xmlns)-6);
		Q_strlcpy(ret->name, ns, sizeof(ret->name));
	}
	else
	{
		Q_strlcpy(ret->xmlns, "xmlns", sizeof(ret->xmlns));
		Q_strlcpy(ret->name, com_token, sizeof(ret->name));
	}

	while(*tagstart)
	{
		int nlen;


		while(*tagstart <= ' ' && *tagstart)
			tagstart++;	//skip whitespace (note that we know there is a null terminator before the end of the buffer)

		if (!*tagstart)
			break;

		p = malloc(sizeof(xmlparams_t));
		nlen = 0;
		while (nlen < sizeof(p->name)-2)
		{
			if(*tagstart <= ' ')
				break;

			if (*tagstart == '=')
				break;
			p->name[nlen++] = *tagstart++;
		}
		p->name[nlen++] = '\0';

		while(*tagstart <= ' ' && *tagstart)
			tagstart++;	//skip whitespace (note that we know there is a null terminator before the end of the buffer)

		if (*tagstart != '=')
			continue;
		tagstart++;

		while(*tagstart <= ' ' && *tagstart)
			tagstart++;	//skip whitespace (note that we know there is a null terminator before the end of the buffer)

		nlen = 0;
		if (*tagstart == '\'')
		{
			tagstart++;
			while (*tagstart && nlen < sizeof(p->name)-2)
			{
				if(*tagstart == '\'')
					break;

				p->val[nlen++] = *tagstart++;
			}
			tagstart++;
			p->val[nlen++] = '\0';
		}
		else if (*tagstart == '\"')
		{
			tagstart++;
			while (*tagstart && nlen < sizeof(p->name)-2)
			{
				if(*tagstart == '\"')
					break;

				p->val[nlen++] = *tagstart++;
			}
			tagstart++;
			p->val[nlen++] = '\0';
		}
		else
		{
			while (*tagstart && nlen < sizeof(p->name)-2)
			{
				if(*tagstart <= ' ')
					break;

				p->val[nlen++] = *tagstart++;
			}
			p->val[nlen++] = '\0';
		}
		p->next = ret->params;
		ret->params = p;
	}

	ns = XML_ParameterOfTree(ret, ret->xmlns);
	Q_strlcpy(ret->xmlns, ns?ns:"", sizeof(ret->xmlns));

	ns = XML_ParameterOfTree(ret, "xmlns");
	Q_strlcpy(ret->xmlns_dflt, ns?ns:defaultnamespace, sizeof(ret->xmlns_dflt));

	tagend[-1] = '>';

	if (tagend[-2] == '/')
	{	//no body
		*startpos = pos;
		return ret;
	}
	if (ret->name[0] == '?')
	{
		//no body either
		if (tagend[-2] == '?')
		{
			*startpos = pos;
			return ret;
		}
	}

	if (headeronly)
	{
		*startpos = pos;
		return ret;
	}

	//does it have a body, or is it child tags?

	bodypos = 0;
	while(1)
	{
		if (pos == maxpos)
		{	//malformed
			Con_Printf("tree is malfored\n");
			XML_Destroy(ret);
			return NULL;
		}

		if (buffer[pos] == '<')
		{
			if (buffer[pos+1] == '/')
			{	//the end of this block
				//FIXME: check name

				tagend = strchr(buffer+pos, '>');
				if (!tagend)
				{
					Con_Printf("No close tag\n");
					XML_Destroy(ret);
					return NULL;	//should never happen
				}
				tagend++;
				pos = tagend - buffer;
				break;
			}

			child = XML_Parse(buffer, &pos, maxpos, false, ret->xmlns_dflt);
			if (!child)
			{
				Con_Printf("Child block is unparsable\n");
				XML_Destroy(ret);
				return NULL;
			}
			child->sibling = ret->child;
			ret->child = child;
		}
		else 
		{
			char c = buffer[pos++];
			if (bodypos < sizeof(ret->body)-1)
				ret->body[bodypos++] = c;
		}
	}
	ret->body[bodypos++] = '\0';

	*startpos = pos;

	return ret;
}

void XML_Destroy(xmltree_t *t)
{
	xmlparams_t *p, *np;

	if (t->child)
		XML_Destroy(t->child);
	if (t->sibling)
		XML_Destroy(t->sibling);

	for (p = t->params; p; p = np)
	{
		np = p->next;
		free(p);
	}
	free(t);
}

xmltree_t *XML_ChildOfTree(xmltree_t *t, char *name, int childnum)
{
	for (t = t->child; t; t = t->sibling)
	{
		if (!strcmp(t->name, name))
		{
			if (childnum-- == 0)
				return t;
		}
	}
	return NULL;
}

void XML_ConPrintTree(xmltree_t *t, int indent)
{
	xmltree_t *c;
	xmlparams_t *p;
	int i;
	for (i = 0; i < indent; i++) Con_Printf(" ");

	Con_Printf("<%s", t->name);
	for (p = t->params; p; p = p->next)
		Con_Printf(" %s='%s'", p->name, p->val);

	if (t->child)
	{
		Con_Printf(">\n");
		for (c = t->child; c; c = c->sibling)
			XML_ConPrintTree(c , indent+2);
		for (i = 0; i < indent; i++) Con_Printf(" ");
		Con_Printf("</%s>\n", t->name);
	}
	else if (*t->body)
		Con_Printf(">%s</%s>\n", t->body, t->name);
	else
		Con_Printf("/>\n");
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


void RenameConsole(char *f)
{
	//note that this function has a sideeffect

	//if I send a message to blah@blah.com, and they reply, the reply comes from blah@blah.com/resource
	//so, if we rename the old console before printing, we don't spawn random extra consoles.
	char old[256];
	char *slash;
	Q_strlcpy(old, f, sizeof(old));
	slash = strchr(f, '/');
	if (slash)
	{
		*slash = '\0';
		Con_RenameSub(f, old);
	}
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
		Con_TrySubPrint("xmppin", jcl->bufferedinmessage+jcl->bufferedinammount-ret);
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
			JCL_AddClientMessageString(jcl, "<iq type='set' id='H_0'><bind xmlns='urn:ietf:params:xml:ns:xmpp-bind'><resource>");
			JCL_AddClientMessageString(jcl, jcl->resource);
			JCL_AddClientMessageString(jcl, "</resource></bind></iq>");
		}
		if ((ot=XML_ChildOfTree(tree, "session", 0)))
		{
			unparsable = false;
			JCL_AddClientMessageString(jcl, "<iq type='set' id='H_1'><session xmlns='urn:ietf:params:xml:ns:xmpp-session'/></iq>");
			JCL_AddClientMessageString(jcl, "<presence/>");
			jcl->connected = true;

			JCL_AddClientMessageString(jcl, "<iq type='get' to='gmail.com' id='H_2'><query xmlns='http://jabber.org/protocol/disco#info'/></iq>");
		}


		if (unparsable)
		{
			if ((!jclient->issecure) && BUILTINISVALID(Net_SetTLSClient) && XML_ChildOfTree(tree, "starttls", 0) != NULL)
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
						if (jclient->noplain && !jclient->issecure)	//probably don't send plain without tls.
						{
							//plain can still be read with man-in-the-middle attacks, of course, even with stl.
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

		id = XML_ParameterOfTree(tree, "id");
		from = XML_ParameterOfTree(tree, "from");
		to = XML_ParameterOfTree(tree, "to");

		f = XML_ParameterOfTree(tree, "type");
		if (f && !strcmp(f, "get"))
		{
			ot = XML_ChildOfTree(tree, "query", 0);
			if (ot)
			{
				if (from && !strcmp(ot->xmlns, "http://jabber.org/protocol/disco#info"))
				{	//http://xmpp.org/extensions/xep-0030.html
					char msg[2048];
					int idletime = 0;
					unparsable = false;

					Q_snprintf(msg, sizeof(msg),
							"<iq type='result' to='%s' id='%s'>"
								"<query xmlns='http://jabber.org/protocol/disco#info'>"
								    "<identity category='client' type='pc' name='FTEQW'/>"
									"<feature var='jabber:iq:version'/>"
#ifndef Q3_VM
									"<feature var='urn:xmpp:time'/>"
#endif			
								"</query>"
							"</iq>", from, id, idletime);
					
					JCL_AddClientMessageString(jcl, msg);
				}
				else if (from && !strcmp(ot->xmlns, "jabber:iq:version"))
				{	//client->client version request
					char msg[2048];
					unparsable = false;

					Q_snprintf(msg, sizeof(msg),
							"<iq type='result' to='%s' id='%s'>"
								"<query xmlns='jabber:iq:version'>"
									"<name>FTEQW Jabber Plugin</name>"
									"<version>V"JCL_BUILD"</version>"
#ifdef Q3_VM
									"<os>QVM plugin</os>"
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
				int tzs;
				time_t rawtime;
				time (&rawtime);
				timeinfo = gmtime (&rawtime);
				tzs = _timezone;
				tzs *= -1;
				Q_snprintf(tz, sizeof(tz), "%+i:%i", tzs/(60*60), abs(tzs/60) % 60);
				strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", timeinfo);
				unparsable = false;
				//strftime
				Q_snprintf(msg, sizeof(msg),
						"<iq type='result' to='%s' id='%s'>"
							"<time xmlns='urn:xmpp:time'>"
								"<tzo>+00:00</tzo>"
								"<utc>2006-12-19T17:58:35Z</utc>"
							"</time>"
						"</iq>", from, id, tz, timestamp);
				JCL_AddClientMessageString(jcl, msg);
			}
#endif
				
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
		else if (f && !strcmp(f, "result"))
		{
			xmltree_t *c, *v;
			unparsable = false;
			c = XML_ChildOfTree(tree, "bind", 0);
			if (c)
			{
				v = XML_ChildOfTree(c, "jid", 0);
				if (v)
					Con_Printf("Bound to jid \"%s\"\n", v->body);
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
		f = XML_ParameterOfTree(tree, "from");

		if (f)
		{
			Q_strlcpy(jcl->defaultdest, f, sizeof(jcl->defaultdest));
			RenameConsole(f);
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
				Con_SubPrintf(f, "%s has become inactive\r", f);
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

			LocalSound("misc/talk.wav");
		}

		if (unparsable)
		{
			unparsable = false;
			Con_Print("Received a message without a body\n");
			XML_ConPrintTree(tree, 0);
		}
	}
	else if (!strcmp(tree->name, "presence"))
	{
		char *from = XML_ParameterOfTree(tree, "from");
		xmltree_t *show = XML_ChildOfTree(tree, "show", 0);
		xmltree_t *status = XML_ChildOfTree(tree, "status", 0);
		xmltree_t *quake = XML_ChildOfTree(tree, "quake", 0);
		xmltree_t *serverip = NULL;

		if (quake && !strcmp(quake->xmlns, "fteqw.com:game"))
			serverip = XML_ChildOfTree(quake, "serverip", 0);

		if (serverip && *serverip->body)
			Con_Printf("%s is now playing quake! ^[JOIN THEM!\\join\\%s^]\n", from, serverip->body);
		else if (status && *status->body)
			Con_Printf("%s is now %s: %s\n", from, show?show->body:"present", status->body);
		else
			Con_Printf("%s is now %s\n", from, show?show->body:"present");

		//we should keep a list of the people that we know of.
		unparsable = false;
	}
	else
		Con_Printf("JCL unrecognised stanza: %s\n", tree->name);

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

//functions above this line allow connections to multiple servers.
//it is just the control functions that only allow one server.

qintptr_t JCL_Frame(qintptr_t *args)
{
	int stat = JCL_CONTINUE;
	if (jclient)
	{
		if (jclient->connected)
		{
			int dummystat;
			char serveraddr[1024*16];
			//get the last server address
			if (!Cvar_GetString("cl_serveraddress", serveraddr, sizeof(serveraddr)))
				serveraddr[0] = 0;
			//if we can't get any stats, its because we're not actually on the server.
			if (!CL_GetStats(0, &dummystat, 1))
				serveraddr[0] = 0;

			if (strcmp(jclient->curquakeserver, serveraddr))
			{
				char msg[1024];
				Q_strlcpy(jclient->curquakeserver, serveraddr, sizeof(jclient->curquakeserver));
				if (!*jclient->curquakeserver)
					Q_strlcpy(msg, "<presence/>", sizeof(msg));
				else
					Q_snprintf(msg, sizeof(msg), 
						"<presence>"
							"<quake xmlns='fteqw.com:game'>"
								 "<serverip>sha1-hash-of-image</serverip>"
							"</quake>"
						"</presence>"
						);
				JCL_AddClientMessageString(jclient, msg);
			}
		}

		while(stat == JCL_CONTINUE)
			stat = JCL_ClientFrame(jclient);
		if (stat == JCL_KILL)
		{
			JCL_CloseConnection(jclient);

			jclient = NULL;
		}
	}
	return 0;
}

void JCL_Command(void)
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
		msg = COM_Parse(msg);
		Q_strlcpy(arg[i], com_token, sizeof(arg[i]));
	}

	if (*arg[0] == '/')
	{
		if (!strcmp(arg[0]+1, "tlsopen") || !strcmp(arg[0]+1, "tlsconnect"))
		{
			if (!*arg[1])
			{
				Con_Printf("tlsopen [server] [account] [password]\n");
				return;
			}

			if (jclient)
			{
				Con_Printf("You are already connected\nPlease /quit first\n");
				return;
			}
			if (!*arg[1])
			{
				Con_Printf("%s <server[:port]> <account[@domain]> <password>\n", arg[0]+1);
				return;
			}
			jclient = JCL_Connect(arg[1], 5223, true, arg[2], arg[3]);
			if (!jclient)
			{
				Con_Printf("Connect failed\n");
				return;
			}
		}
		else if (!strcmp(arg[0]+1, "open") || !strcmp(arg[0]+1, "connect"))
		{
			if (!*arg[1])
			{
				Con_Printf("open [server] [account] [password]\n");
				return;
			}

			if (jclient)
			{
				Con_Printf("You are already connected\nPlease /quit first\n");
				return;
			}
			if (!*arg[1])
			{
				Con_Printf("%s <server[:port]> <account[@domain]> <password>\n", arg[0]+1);
				return;
			}
			jclient = JCL_Connect(arg[1], 5222, false, arg[2], arg[3]);
			if (!jclient)
			{
				Con_Printf("Connect failed\n");
				return;
			}
		}
		else if (!jclient)
		{
			Con_Printf("You are not connected. Cannot %s\n", arg[0]);
		}
		else if (!strcmp(arg[0]+1, "quit"))
		{
			JCL_CloseConnection(jclient);
			jclient = NULL;
		}
		else if (!strcmp(arg[0]+1, "msg"))
		{
			Q_strlcpy(jclient->defaultdest, arg[1], sizeof(jclient->defaultdest));
			msg = arg[2];

			JCL_AddClientMessageString(jclient, "<message to='");
			JCL_AddClientMessageString(jclient, jclient->defaultdest);
			JCL_AddClientMessageString(jclient, "'><body>");
			JCL_AddClientMessageString(jclient, msg);
			JCL_AddClientMessageString(jclient, "</body></message>");

			Con_SubPrintf(jclient->defaultdest, "%s: "COLOURYELLOW"%s\n", ">>", msg);
		}
		else if (!strcmp(arg[0]+1, "raw"))
		{
			JCL_AddClientMessageString(jclient, arg[1]);
		}
		else
			Con_Printf("Unrecognised command: %s\n", arg[0]);
	}
	else
	{
		if (jclient)
		{
			msg = imsg;
			JCL_AddClientMessageString(jclient, "<message to='");
			JCL_AddClientMessageString(jclient, jclient->defaultdest);
			JCL_AddClientMessageString(jclient, "'><body>");
			JCL_AddClientMessageString(jclient, msg);
			JCL_AddClientMessageString(jclient, "</body></message>");

			Con_SubPrintf(jclient->defaultdest, "%s: "COLOURYELLOW"%s\n", ">>", msg);
		}
		else
		{
			Con_Printf("Not connected\ntype \"" COMMANDPREFIX " /connect JABBERSERVER USERNAME@DOMAIN PASSWORD\" to connect\n");
			if (BUILTINISVALID(Net_SetTLSClient))
				Con_Printf("eg: " COMMANDPREFIX " /tlsconnect talk.google.com myusername@gmail.com mypassword\n");
		}
	}
}
