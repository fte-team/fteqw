//Released under the terms of the gpl as this file uses a bit of quake derived code. All sections of the like are marked as such

#include "../plugin.h"

#define Q_strncpyz(o, i, l) do {strncpy(o, i, l-1);o[l-1]='\0';}while(0)

#define JCL_BUILD "1"


#define ARGNAMES ,sock
BUILTINR(int, Net_SetTLSClient, (qhandle_t sock));
#undef ARGNAMES


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

int JCL_ExecuteCommand(int *args)
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

int JCL_ConExecuteCommand(int *args);

int JCL_Frame(int *args);

int (*Con_TrySubPrint)(char *conname, char *message);

int Plug_Init(int *args)
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
			Con_TrySubPrint = Con_Print;
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


#define JCL_MAXMSGLEN 2048


typedef struct {
	char server[64];
	int port;

	qhandle_t socket;
	qhandle_t inlog;
	qhandle_t outlog;

	char bufferedinmessage[JCL_MAXMSGLEN+1];	//there is a max size for protocol. (conveinient eh?) (and it's text format)
	int bufferedinammount;

	char defaultdest[256];

	char domain[256];
	char username[256];
	char password[256];
	char resource[256];

	int tagdepth;
	int openbracket;
	int instreampos;

	qboolean noplain;
	qboolean issecure;
} jclient_t;
jclient_t *jclient;

int JCL_ConExecuteCommand(int *args)
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
	Con_Printf(COLOURYELLOW "<< %s \n",msg);
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
		if (Net_SetTLSClient(jclient->socket)<0)
		{
			Net_Close(jclient->socket);
			free(jclient);
			jclient = NULL;

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
	strlcpy(jcl->username, account, sizeof(jcl->username));
	strlcpy(jcl->domain, at+1, sizeof(jcl->domain));
	strlcpy(jcl->password, password, sizeof(jcl->password));

	strcpy(jcl->resource, "Quake");

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
	char body[2048];

	xmlparams_t *params;

	struct subtree_s *child;
	struct subtree_s *sibling;
} xmltree_t;

void XML_Destroy(xmltree_t *t);
xmltree_t *XML_Parse(char *buffer, int *startpos, int maxpos, qboolean headeronly)
{
	xmlparams_t *p;
	xmltree_t *child;
	xmltree_t *ret;
	int bodypos;
	int pos;
	char *tagend;
	char *tagstart;
	pos = *startpos;
	while (buffer[pos] >= '\0' && buffer[pos] <= ' ')
	{
		if (pos >= maxpos)
			break;
		pos++;
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
	strlcpy(ret->name, com_token, sizeof(ret->name));

	// FIXME:parse the parameters
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

			child = XML_Parse(buffer, &pos, maxpos, false);
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
			ret->body[bodypos++] = buffer[pos++];
	}
	ret->body[bodypos++] = '\0';

	*startpos = pos;

	return ret;
}

char *XML_ParameterOfTree(xmltree_t *t, char *paramname)
{
	xmlparams_t *p;
	for (p = t->params; p; p = p->next)
		if (!strcmp(p->name, paramname))
			return p->val;
	return NULL;
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
		Con_Printf("base64: %s\n", base64+base64_len-4);
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
	strlcpy(old, f, sizeof(old));
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
		Con_Printf("JCL: Remote host disconnected\n");
		return JCL_KILL;
	}
	if (ret < 0)
	{
		if (ret == N_WOULDBLOCK)
		{
			if (!jcl->bufferedinammount)	//if we are half way through a message, read any possible conjunctions.
				return JCL_DONE;	//remove
		}
		else
		{
			Con_Printf("JCL: socket error\n");
			return JCL_KILL;
		}
	}

	if (ret>0)
		jcl->bufferedinammount+=ret;

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
		tree = XML_Parse(jcl->bufferedinmessage, &pos, jcl->instreampos, true);
		while (tree && !strcmp(tree->name, "?xml"))
		{
			XML_Destroy(tree);
			tree = XML_Parse(jcl->bufferedinmessage, &pos, jcl->instreampos, true);
		}

		if (!tree)
		{
			Con_Printf("Not an xml stream\n");
			return JCL_KILL;
		}
		if (strcmp(tree->name, "stream:stream"))
		{
			Con_Printf("Not an xmpp stream\n");
			return JCL_KILL;
		}

		ot = tree;
		tree = tree->child;
		ot->child = NULL;
		Con_Printf("Discard\n");
		XML_ConPrintTree(ot, 0);
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
			if (jcl->tagdepth < 1)
			{
				Con_Printf("End of XML stream\n");
				return JCL_KILL;
			}
			return JCL_DONE;
		}

		pos = 0;
		tree = XML_Parse(jcl->bufferedinmessage, &pos, jcl->instreampos, false);

		if (!tree)
		{
//			Con_Printf("No input tree: %s", jcl->bufferedinmessage);
			return JCL_DONE;
		}
	}

	Con_Printf("read\n");
	XML_ConPrintTree(tree, 0);


	unparsable = true;
	if (!strcmp(tree->name, "stream:features"))
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
		}


		if (unparsable)
		{
			if ((!jclient->issecure) && BUILTINISVALID(Net_SetTLSClient) && XML_ChildOfTree(tree, "starttls", 0) != NULL)
			{
				JCL_AddClientMessageString(jcl, "<starttls xmlns='urn:ietf:params:xml:ns:xmpp-tls' />");
				unparsable = false;
			}
			else if ((ot=XML_ChildOfTree(tree, "mechanisms", 0)))
			{
				for(ot = ot->child; ot; ot = ot->sibling)
				{
					if (!strcmp(ot->body, "PLAIN"))
					{
						if (jclient->noplain && !jclient->issecure)	//probably don't send plain without tls.
						{
							//plain can still be read with man-in-the-middle attacks, of course, even with stl.
							Con_Printf("Ignoring auth \'%s\'\n", ot->body);
							continue;
						}
						Con_Printf("Authing with \'%s\'\n", ot->body);
						JCL_AddClientMessageString(jcl, "<auth xmlns='urn:ietf:params:xml:ns:xmpp-sasl' mechanism='PLAIN'>");
						Base64_Add(jclient->username, strlen(jcl->username));
						Base64_Add("@", 1);
						Base64_Add(jclient->domain, strlen(jcl->domain));
						Base64_Add("", 1);
						Base64_Add(jclient->username, strlen(jcl->username));
						Base64_Add("", 1);
						Base64_Add(jcl->password, strlen(jcl->password));
						Base64_Finish();
						JCL_AddClientMessageString(jcl, base64);
						JCL_AddClientMessageString(jcl, "</auth>");
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
			return JCL_KILL;
		}

		if (Net_SetTLSClient(jcl->socket)<0)
		{
			Con_Printf("JCL: failed to switch to TLS\n");
			return JCL_KILL;
		}
		jclient->issecure = true;

		JCL_AddClientMessageString(jcl,
			"<?xml version='1.0' ?>"
			"<stream:stream to='");
		JCL_AddClientMessageString(jcl, jcl->domain);
		JCL_AddClientMessageString(jcl, "' xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams' version='1.0'>");

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
		unparsable = false;

		id = XML_ParameterOfTree(tree, "id");
		from = XML_ParameterOfTree(tree, "from");
		to = XML_ParameterOfTree(tree, "to");

		f = XML_ParameterOfTree(tree, "type");
		if (f && !strcmp(f, "get"))
		{
			ot = XML_ChildOfTree(tree, "query", 0);
			if (ot)
			{
				f = XML_ParameterOfTree(tree, "xmlns");
				if (f && to && from && !strcmp(f, "jabber:iq:version"))
				{	//client->client version request
					JCL_AddClientMessageString(jcl, "<iq type='result' to='");
					JCL_AddClientMessageString(jcl, from);
					JCL_AddClientMessageString(jcl, "' from='");
					JCL_AddClientMessageString(jcl, to);
					JCL_AddClientMessageString(jcl, "' id='");
					JCL_AddClientMessageString(jcl, id);
					JCL_AddClientMessageString(jcl, "'>");

					JCL_AddClientMessageString(jcl,	"<query xmlns='jabber:iq:version'>"
										"<name>FTEQW Jabber Plugin</name>"
										"<version>"JCL_BUILD"</version>"
#ifdef Q3_VM
										"<os>QVM plugin</os>"
#endif
									"</query>");

					JCL_AddClientMessageString(jcl, "</iq>");
				}
			}
		}
		else
			Con_Print("Unrecognised iq type\n");
	}
	else if (!strcmp(tree->name, "message"))
	{
		unparsable = false;
		ot = XML_ChildOfTree(tree, "body", 0);
		if (ot)
		{
			f = XML_ParameterOfTree(tree, "from");
			if (f)
			{
				strlcpy(jcl->defaultdest, f, sizeof(jcl->defaultdest));
				RenameConsole(f);
				Con_SubPrintf(f, "%s: %s\n", f, ot->body);
			}
			else
				Con_Print(ot->body);

			LocalSound("misc/talk.wav");
		}
		else
			Con_Print("Received a message without a body\n");
	}
	else if (!strcmp(tree->name, "presence"))
	{
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

int JCL_Frame(int *args)
{
	int stat = JCL_CONTINUE;
	if (jclient)
	{
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
		strlcpy(arg[i], com_token, sizeof(arg[i]));
	}

	if (*arg[0] == '/')
	{
		if (!strcmp(arg[0]+1, "tlsopen") || !strcmp(arg[0]+1, "tlsconnect"))
		{
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
		}
		else if (!strcmp(arg[0]+1, "open") || !strcmp(arg[0]+1, "connect"))
		{
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
			strlcpy(jclient->defaultdest, arg[1], sizeof(jclient->defaultdest));
			msg = arg[2];

			JCL_AddClientMessageString(jclient, "<message to='");
			JCL_AddClientMessageString(jclient, jclient->defaultdest);
			JCL_AddClientMessageString(jclient, "'><body>");
			JCL_AddClientMessageString(jclient, msg);
			JCL_AddClientMessageString(jclient, "</body></message>");

			Con_SubPrintf(jclient->defaultdest, "%s: "COLOURYELLOW"%s\n", ">>", msg);
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
