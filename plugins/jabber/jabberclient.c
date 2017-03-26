//Released under the terms of the gpl as this file uses a bit of quake derived code. All sections of the like are marked as such

/*
Network limitations:
	googletalk:
		username: same as gmail (foobar@gmail.com).
		FIXME: need to test foobar@googlemail.com
		auth mechanism: oauth2(tls+nontls) or plain(tls-only). no digests supported, so mitm can easily grab your password if they use certificate authority hackery, so DO NOT log in from work.
		oauth2: I've registered a clientid for use with googletalk's network, but the whole web-browser-is-required crap makes it near unusable. We'll try it if they omit a password.
		otherwise a complete implementation.
		other users appear unresponsive and permanently away. this is a wtf on google's part and not something I can trivially work around. these people are really offline but have previously used 'google hangouts', and google insist on the UI nightmare infecting other clients too.
		appears to hack avatar vcards into all presence messages, which is just an interesting thing to note as it seems to keep fucking up resulting in extra queries for avatar images.

	facebook:
		username: foobar@chat.facebook.com
		auth mechanism: digest-md5, x-facebook-platform.
		gateway implementation: no arbitary iq support (no invite/join/voice).
		no roster control
		completely untested. I've no interest in signing up to be tracked constantly (but somehow google is okay... go figure... I guess I'm just trying to avoid a double-whammy)
		oauth2: no idea where to register a clientid, or what the correct addresses are. a google search implies they don't do refresh tokens properly. sticking with digest-md5 should work.
		*should* work for chat.

	msn:
		username: foobar@messenger.live.com (NOT foobar@live.com - this will timeout)
		auth mechansim: x-messenger-oath ONLY
		non-standard unusable crap.
		uses incorrect certificates. any client that doesn't warn about that is buggy as fuck.
		probably doesn't have iq support, no idea, can't log in to test that
		requires annoying see-other-host redirection.
		no roster control
		stun servers are listed in srv records for live.com and messanger.live.com but not messenger.live.com. retards.
		oauth2: too lazy to register a clientid. stupid crap. I hate having to register everywhere.

	ejabberd:
		auth mechanism: digest-md5, scram-sha1, plain.
		complete implementation. no issues.
		may be lacking srv entries, depends on installation.
		may have self-signed certificate issues, depends on installation.

client compat:
	hangouts:
		UI nightmare infects the entire network and thus other clients also.
		voip not supported. does not advertise any extensions and thus no voip.
		no file transfer support.

	googletalk:
		impossible to download from google any more. completely unsupported.
		implements old version of jingle. voice calls appear to not work.
		does not support SI file transfer.
		not tested by me.

	pidgin:
		(linux) has issues with jingle+ice, and can easily be made to crash. voip uses speex. pidgin's ice seems vulnerable to dropped packets.
		(windows) doesn't support voice calls
		file transfer works.
		otherwise works.
*/

#include "../plugin.h"
#include <time.h>
#include "xmpp.h"

#ifdef DEFAULTDOMAIN
	#define EXAMPLEDOMAIN DEFAULTDOMAIN	//used in examples / default text field (but not otherwise assumed when omitted)
#else
	#define EXAMPLEDOMAIN "gmail.com"	//used in examples
#endif


#ifdef JINGLE
icefuncs_t *piceapi;
#endif
qboolean jclient_needreadconfig;
qboolean jclient_updatebuddylist;
jclient_t *jclient_action_cl;
buddy_t *jclient_action_buddy;
enum
{
	ACT_NONE,
	ACT_OAUTH,
	ACT_NEWACCOUNT,
	ACT_PASSWORD,
	ACT_ADDFRIEND,
	ACT_SETALIAS,
} jclient_action;

#define BUDDYLISTTITLE "Buddy List"



#define Q_strncpyz(o, i, l) do {strncpy(o, i, l-1);o[l-1]='\0';}while(0)

void (*Con_TrySubPrint)(const char *conname, const char *message);
void Fallback_ConPrint(const char *conname, const char *message)
{
	pCon_Print(message);
}

void XMPP_ConversationPrintf(const char *context, const char *title, char *format, ...);
void Con_SubPrintf(const char *subname, char *format, ...)
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

	static char *JCL_ParseOut (char *data, char *buf, int bufsize)	//GPL: this is taken out of quake
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


//GPL: ripped from quake
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

#if defined(_WIN32) && defined(HAVE_PACKET)
#include <windns.h>
static DNS_STATUS (WINAPI *pDnsQuery_UTF8) (PCSTR pszName, WORD wType, DWORD Options, PIP4_ARRAY aipServers, PDNS_RECORD *ppQueryResults, PVOID *pReserved);
static VOID (WINAPI *pDnsRecordListFree)(PDNS_RECORD pRecordList, DNS_FREE_TYPE FreeType);
static HMODULE dnsapi_lib;
qboolean NET_DNSLookup_SRV(char *host, char *out, int outlen)
{
	DNS_RECORD *result = NULL;
	if (!dnsapi_lib)
	{
		dnsapi_lib = LoadLibrary("dnsapi.dll");
		pDnsQuery_UTF8 = (void*)GetProcAddress(dnsapi_lib, "DnsQuery_UTF8");
		pDnsRecordListFree = (void*)GetProcAddress(dnsapi_lib, "DnsRecordListFree");
	}
	//win98?
	if (!pDnsQuery_UTF8 || !pDnsRecordListFree)
		return false;
	//do lookup
	pDnsQuery_UTF8(host, DNS_TYPE_SRV, DNS_QUERY_STANDARD, NULL, &result, NULL);
	if (result)
	{
		Q_snprintf(out, outlen, "[%s]:%i", result->Data.SRV.pNameTarget, result->Data.SRV.wPort);
		pDnsRecordListFree(result, DnsFreeRecordList);
		return true;
	}
	return false;
}
#elif defined(__unix__) || defined(ANDROID) || defined(__MACH__) || defined(__linux__)
#include <resolv.h>
#include <arpa/nameser.h>
qboolean NET_DNSLookup_SRV(char *host, char *out, int outlen)
{
	int questions;
	int answers;
	qbyte answer[512];
	qbyte dname[512];
	int len, i;
	qbyte *msg, *eom;

	len = res_query(host, C_IN, T_SRV, answer, sizeof(answer));
	if (len < 12)
	{
		Con_Printf("srv lookup failed for %s\n", host);
		return false;
	}

	eom = answer+len;

	questions = (answer[4]<<8) | answer[5];
	answers = (answer[6]<<8) | answer[7];
//	id @ 0
//	bits @ 2
//	questioncount@4
///	answer count@6
//	nameserver record count @8
//	additional record count @10

//	questions@12
//	answers@12+sizeof(questions)

	if (answers < 1)
		return false;

	msg = answer+12;

	while(questions --> 0)
	{
		dn_expand(answer, eom, msg, dname, sizeof(dname));
//		Con_Printf("Skip question %s\n", dname);
		i = dn_skipname(msg, eom);
		if (i <= 0)
			return false;
		msg += i;
		msg += 2;//query type
		msg += 2;//query class
	}

	while(answers --> 0)
	{
		i = dn_expand(answer, eom, msg, dname, sizeof(dname));
//		i = dn_skipname(msg, eom);
		msg += i;
		msg += 2;//query type
		msg += 2;//query class
		msg += 4;//ttl
		i = (msg[0]<<8) | msg[1];
		msg+=2;
		//noone tried to send the wrong type then, woo.
		if (!strcmp(dname, host))
		{
			int port;
			//we're not serving to other dns servers, and it seems they're already getting randomized, so just grab the first without rerandomizing.
			msg += 2;//priority
			msg += 2;//weight
			port = (msg[0]<<8) | msg[1];
			msg += 2;//port
			dn_expand(answer, eom, msg, dname, sizeof(dname));
			Q_snprintf(out, outlen, "[%s]:%i", dname, port);
//			Con_Printf("Resolved to %s\n", out);
			return true;
		}
		dn_expand(answer, eom, msg, out, outlen);
//		Con_Printf("Ignoring resolution to %s\n", out);
		msg += i;
	}

//type (2 octets)
//class (2 octets)
//TTL (4 octets)
//resource data length (2 octets)
//resource data (variable length)

	if (i < 0)
		return false;
	return true;
}
#else
qboolean NET_DNSLookup_SRV(char *host, char *out, int outlen)
{
	return false;
}
#endif


char base64[((4096+3)*4/3)+1];
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
	if (base64_len+4>=sizeof(base64)-1)
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

char *Base64_Finish(void)
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

	return base64;
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
//FIXME: we should be able to skip whitespace.
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



void XMPP_Menu_Password(jclient_t *acc);
void RenameConsole(char *totrim);
void JCL_Command(int accid, char *consolename);
void JCL_LoadConfig(void);
void JCL_WriteConfig(void);

struct {
	char *names;
	unsigned int cap;
} capnames[] =
{
	{"avatars", CAP_AVATARS},
	{"jingle_voice", CAP_VOICE},
	{"jingle_video", CAP_VIDEO},
	{"google_voice", CAP_GOOGLE_VOICE},
	{"quake_invite", CAP_GAMEINVITE},
	{"poke", CAP_POKE},
#ifdef FILETRANSFERS
	{"si_filetransfer", CAP_SIFT},
#endif
	{NULL}
};


qintptr_t JCL_ExecuteCommand(qintptr_t *args)
{
	char cmd[256];
	pCmd_Argv(0, cmd, sizeof(cmd));
	if (!strcmp(cmd, COMMANDPREFIX) || !strcmp(cmd, COMMANDPREFIX2) || !strcmp(cmd, COMMANDPREFIX3))
	{
		if (!args[0] || pCmd_Argc() == 1)
			JCL_Command(0, "");
		return true;
	}
	if (!strncmp(cmd, COMMANDPREFIX, strlen(COMMANDPREFIX)))
	{
		if (!args[0] || pCmd_Argc() == 1)
			JCL_Command(atoi(cmd+strlen(COMMANDPREFIX)), "");
		return true;
	}
	return false;
}

qintptr_t JCL_ConsoleLink(qintptr_t *args);
qintptr_t JCL_ConsoleLinkMouseOver(qintptr_t *args);
qintptr_t JCL_ConExecuteCommand(qintptr_t *args);

qintptr_t JCL_Frame(qintptr_t *args);
qintptr_t JCL_Shutdown(qintptr_t *args);

static qintptr_t QDECL JCL_UpdateVideo(qintptr_t *args)
{
	pvid.width = args[0];
	pvid.height = args[1];

	//FIXME: clear/reload images.

	return true;
}

qintptr_t Plug_Init(qintptr_t *args)
{
	jclient_needreadconfig = true;

	if (	Plug_Export("Tick", JCL_Frame) &&
		Plug_Export("Shutdown", JCL_Shutdown) &&
		Plug_Export("ExecuteCommand", JCL_ExecuteCommand))
	{
		if (!BUILTINISVALID(Net_SetTLSClient))
			Con_Printf("XMPP Plugin Loaded ^1without^7 TLS\n");	//most servers REQUIRE tls now
		else
			Con_Printf("XMPP Plugin Loaded. For help, use: ^[/"COMMANDPREFIX" /help^]\n");

		Plug_Export("UpdateVideo", JCL_UpdateVideo);
		Plug_Export("ConsoleLink", JCL_ConsoleLink);
		Plug_Export("ConsoleLinkMouseOver", JCL_ConsoleLinkMouseOver);

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

		pCmd_AddCommand(COMMANDPREFIX"0");
		pCmd_AddCommand(COMMANDPREFIX"1");
		pCmd_AddCommand(COMMANDPREFIX"2");
		pCmd_AddCommand(COMMANDPREFIX"3");
		pCmd_AddCommand(COMMANDPREFIX"4");
		pCmd_AddCommand(COMMANDPREFIX"5");
		pCmd_AddCommand(COMMANDPREFIX"6");
		pCmd_AddCommand(COMMANDPREFIX"7");

		//flags&1 == archive
		pCvar_Register("xmpp_nostatus",				"0", 0, "xmpp");
		pCvar_Register("xmpp_showstatusupdates",	"0", 0, "xmpp");
		pCvar_Register("xmpp_autoacceptjoins",		"0", 0, "xmpp");
		pCvar_Register("xmpp_autoacceptinvites",	"0", 0, "xmpp");
		pCvar_Register("xmpp_autoacceptvoice",		"0", 0, "xmpp");
		pCvar_Register("xmpp_debug",				"0", 0, "xmpp");

#ifdef JINGLE
		if (BUILTINISVALID(Plug_GetNativePointer))
			piceapi = pPlug_GetNativePointer(ICE_API_CURRENT);
#endif

		return 1;
	}
	else
		Con_Printf("JCL Client Plugin failed\n");
	return 0;
}










//\r\n is used to end a line.
//meaning \0s are valid.
//but never used cos it breaks strings

jclient_t *jclients[8];
int jclient_curtime;
int jclient_poketime;

typedef struct
{
	char *method;
	int (*sasl_initial)(jclient_t *jcl, char *buf, int bufsize);
	int (*sasl_challenge)(jclient_t *jcl, char *inbuf, int insize, char *outbuf, int outsize);
} saslmethod_t;

//#define OAUTH_CLIENT_ID_MSN "0"
#ifdef OAUTH_CLIENT_ID_MSN
static int sasl_plain_initial(jclient_t *jcl, char *buf, int bufsize)
{
	//"https://oauth.live.com/authorize?client_id=" OAUTH_CLIENT_ID_MSN "&scope=wl.messenger,wl.basic,wl.offline_access,wl.contacts_create,wl.share&response_type=token&redirect_uri=http://localhost/";
}
#endif

//\0username\0password
static int sasl_plain_initial(jclient_t *jcl, char *buf, int bufsize)
{
	int len = 0;

	if (jcl->issecure?jcl->allowauth_plaintls:jcl->allowauth_plainnontls)
	{
		if (!*jcl->password)
			return -2;

		//realm isn't specified
		buf[len++] = 0;
		memcpy(buf+len, jcl->username, strlen(jcl->username));
		len += strlen(jcl->username);
		buf[len++] = 0;
		memcpy(buf+len, jcl->password, strlen(jcl->password));
		len += strlen(jcl->password);

		return len;
	}
	return -1;
}


static int saslattr(char *out, int outlen, char *srcbuf, int srclen, char *arg)
{
	char *vn;
	char *s = srcbuf;
	char *e = s + srclen;
	char *vs;
	while(s < e)
	{
		while (s < e && *s == ',')
			s++;
		vn = s;
		s++;
		while (s < e && *s >= 'a' && *s <= 'z')
			s++;
		if (*s == '=')
		{
			vs = ++s;
			if (*s == '\"')
			{
				vs = ++s;
				while (s < e && *s != '\"')
					s++;
				outlen = s - vs;
				s++;
			}
			else
			{
				while (s < e && *s != ',')
					s++;
				outlen = s - vs;
			}

			if (!strncmp(vn, arg, strlen(arg)) && vn[strlen(arg)] == '=')
			{
				memcpy(out, vs, outlen);
				out[outlen] = 0;
				return outlen;
			}
		}
	}
	out[0] = 0;
	return 0;
}

static int sasl_digestmd5_initial(jclient_t *jcl, char *buf, int bufsize)
{
	if (jcl->allowauth_digestmd5)
	{
		if (!*jcl->password)
			return -2;

		//FIXME: randomize the cnonce and check the auth key
		//although really I'm not entirely sure what the point is.
		//if we just authenticated with a mitm attacker relay, we're screwed either way.
		strcpy(jcl->authnonce, "abcdefghijklmnopqrstuvwxyz");
		//nothing. server does the initial data.
		return 0;
	}
	return -1;
}
char *MD5_ToHex(char *input, int inputlen, char *ret, int retlen);
char *MD5_ToBinary(char *input, int inputlen, char *ret, int retlen);
static int sasl_digestmd5_challenge(jclient_t *jcl, char *in, int inlen, char *out, int outlen)
{
	char *username = jcl->username;
	char *password = jcl->password;
	char *cnonce = jcl->authnonce;
	char rspauth[512];
	char realm[512];
	char nonce[512];
	char qop[512];
	char charset[512];
	char algorithm[512];
	char X[512];
	char Y[33];
	char A1[512];
	char A2[512];
	char HA1[33];
	char HA2[33];
	char KD[512];
	char Z[33];
	char *nc = "00000001";
	char digesturi[512];
	char *authzid = "";

	saslattr(rspauth, sizeof(rspauth), in, inlen, "rspauth");
	if (*rspauth)
	{
		return 0;	//we don't actually send any data back, just an xml 'response' tag to tell the server that we accept it.
	}

	saslattr(realm, sizeof(realm), in, inlen, "realm");
	saslattr(nonce, sizeof(nonce), in, inlen, "nonce");
	saslattr(qop, sizeof(qop), in, inlen, "qop");
	saslattr(charset, sizeof(charset), in, inlen, "charset");
	saslattr(algorithm, sizeof(algorithm), in, inlen, "algorithm");

	if (!*realm)
		Q_strlcpy(realm, jcl->domain, sizeof(realm));
	Q_snprintf(digesturi, sizeof(digesturi), "xmpp/%s", realm);

	Q_snprintf(X, sizeof(X), "%s:%s:%s", username, realm, password);
	MD5_ToBinary(X, strlen(X), Y, sizeof(Y));
	memcpy(A1, Y, 16);
	if (*authzid)
		Q_snprintf(A1+16, sizeof(A1)-16, ":%s:%s:%s", nonce, cnonce, authzid);
	else
		Q_snprintf(A1+16, sizeof(A1)-16, ":%s:%s", nonce, cnonce);
	Q_snprintf(A2, sizeof(A2), "%s:%s", "AUTHENTICATE", digesturi);
	MD5_ToHex(A1, strlen(A1+16)+16, HA1, sizeof(HA1));
	MD5_ToHex(A2, strlen(A2), HA2, sizeof(HA2));
	Q_snprintf(KD, sizeof(KD), "%s:%s:%s:%s:%s:%s", HA1, nonce, nc, cnonce, qop, HA2);
	MD5_ToHex(KD, strlen(KD), Z, sizeof(Z));

	if (*authzid)
		Q_snprintf(out, outlen, "username=\"%s\",realm=\"%s\",nonce=\"%s\",cnonce=\"%s\",nc=\"%s\",qop=\"%s\",digest-uri=\"%s\",response=\"%s\",charset=\"%s\",authzid=\"%s\"",
					username, realm, nonce, cnonce, nc, qop, digesturi, Z, charset, authzid);
	else
		Q_snprintf(out, outlen, "username=\"%s\",realm=\"%s\",nonce=\"%s\",cnonce=\"%s\",nc=\"%s\",qop=\"%s\",digest-uri=\"%s\",response=\"%s\",charset=\"%s\"",
					username, realm, nonce, cnonce, nc, qop, digesturi, Z, charset);

	return strlen(out);
}

static int sasl_scramsha1_initial(jclient_t *jcl, char *buf, int bufsize)
{
	if (jcl->allowauth_scramsha1)
	{
		if (!*jcl->password)
			return -2;
		strcpy(jcl->authnonce, "abcdefghijklmnopqrstuvwxyz");	//FIXME: should be random, to validate that the server knows our password too

		Q_snprintf(buf, bufsize, "n,,n=%s,r=%s", jcl->username, jcl->authnonce);
		return strlen(buf);
	}
	return -1;
}

typedef struct
{
	int len;
	char buf[512];
} buf_t;
static void buf_cat(buf_t *buf, char *data, int len)
{
	memcpy(buf->buf + buf->len, data, len);
	buf->len += len;
	buf->buf[buf->len] = 0;
}
static void buf_cats(buf_t *buf, char *data)
{
	buf_cat(buf, data, strlen(data));
}
static void SHA1_Hi(char *out, char *password, int passwordlen, buf_t *salt, int times)
{
	char prev[20];
	int i, j;

	//first iteration is special
	buf_cat(salt, "\0\0\0\1", 4);
	SHA1_HMAC(prev, sizeof(prev), salt->buf, salt->len, password, passwordlen);
	memcpy(out, prev, sizeof(prev));
	
	//later iterations just use the previous iteration
	for (i = 1; i < times; i++)
	{
		SHA1_HMAC(prev, sizeof(prev), prev, sizeof(prev), password, passwordlen);

		for (j = 0; j < sizeof(prev); j++)
			out[j] ^= prev[j];
	}
}
static int sasl_scramsha1_challenge(jclient_t *jcl, char *in, int inlen, char *out, int outlen)
{
	//sasl SCRAM-SHA-1 challenge
	//send back the same 'r' attribute
	buf_t saslchal;
	int i;
	buf_t salt;
	buf_t csn;
	buf_t itr;
	buf_t final;
	buf_t sigkey;
	char salted_password[20];
	char proof[20];
	char clientkey[20];
	char storedkey[20];
	char clientsignature[20];
	char *username = jcl->username;
	char *password = jcl->password;

	saslchal.len = 0;
	buf_cat(&saslchal, in, inlen);
	
	//be warned, these CAN contain nulls.
	csn.len = saslattr(csn.buf, sizeof(csn.buf), saslchal.buf, saslchal.len, "r");
	salt.len = saslattr(salt.buf, sizeof(salt.buf), saslchal.buf, saslchal.len, "s");
	itr.len = saslattr(itr.buf, sizeof(itr.buf), saslchal.buf, saslchal.len, "i");

	salt.len = Base64_Decode(salt.buf, sizeof(salt.buf), salt.buf, salt.len);
	
	//FIXME: we should validate that csn is prefixed with our cnonce

	//this is the first part of the message we're about to send, with no proof.
	//c(channel) is mandatory but nulled and forms part of the hash
	final.len = 0;
	buf_cats(&final, "c=");
	Base64_Add("n,,", 3);
	Base64_Finish();
	buf_cat(&final, base64, strlen(base64));
	buf_cats(&final, ",r=");
	buf_cat(&final, csn.buf, csn.len);

	//our original message + ',' + challenge + ',' + the message we're about to send.
	sigkey.len = 0;
	buf_cats(&sigkey, "n=");
	buf_cats(&sigkey, username);
	buf_cats(&sigkey, ",r=");
	buf_cats(&sigkey, jcl->authnonce);
	buf_cats(&sigkey, ",");
	buf_cat(&sigkey, saslchal.buf, saslchal.len);
	buf_cats(&sigkey, ",");
	buf_cat(&sigkey, final.buf, final.len);

	SHA1_Hi(salted_password, password, strlen(password), &salt, atoi(itr.buf));
	SHA1_HMAC(clientkey, sizeof(clientkey), "Client Key", strlen("Client Key"), salted_password, sizeof(salted_password));
	SHA1(storedkey, sizeof(storedkey), clientkey, sizeof(clientkey));	//FIXME: switch the account's plain password to store this digest instead (with salt+itr).
	SHA1_HMAC(clientsignature, sizeof(clientsignature), sigkey.buf, sigkey.len, storedkey, sizeof(storedkey));

	for (i = 0; i < sizeof(proof); i++)
		proof[i] = clientkey[i] ^ clientsignature[i];

	Base64_Add(proof, sizeof(proof));
	Base64_Finish();


	//"c=biws,r=fyko+d2lbbFgONRv9qkxdawL3rfcNHYJY1ZVvWVs7j,p=v0X8v3Bz2T0CJGbJQyF0X+HI4Ts="
	Q_snprintf(out, outlen, "%s,p=%s", final.buf, base64);
	return strlen(out);
}

void URL_Split(char *url, char *proto, int protosize, char *host, int hostsize, char *res, int ressize)
{
	char *s;
	*proto = 0;
	*host = 0;
	*res = 0;
	s = strchr(url, ':');
	if (!s)
		return;
	protosize = min(protosize-1, s-url);
	memcpy(proto, url, protosize);
	proto[protosize] = 0;
	s++;
	if (s[0] == '/' && s[1] == '/')
		s+=2;
	url = s;
	s = strchr(url, '/');
	if (!s)
		s = url+strlen(url);
	hostsize = min(hostsize-1, s-url);
	memcpy(host, url, hostsize);
	host[hostsize] = 0;
	url = s;
	s = url+strlen(url);
	ressize = min(ressize-1, s-url);
	memcpy(res, url, ressize);
	res[ressize] = 0;
}
void Q_strlcat_urlencode(char *d, const char *s, int n)
{
	char hex[16] = "0123456789ABCDEF";
	int clen = strlen(d);
	d += clen;
	n -= clen;
	n--;
	if (s)
	while (*s)
	{
		if ((*s >= '0' && *s <= '9') ||
			(*s >= 'a' && *s <= 'z') ||
			(*s >= 'A' && *s <= 'Z') ||
			*s == '-' || *s == '_' || *s == '.')
		{
			if (!n)
				break;
			n--;
			*d++ = *s++;
		}
		else
		{
			if (n < 3)
				break;
			n -= 3;
			*d++ = '%';
			*d++ = hex[*s>>4];
			*d++ = hex[*s&15];
			s++;
		}
	}
	*d = 0;
}
static int sasl_oauth2_initial(jclient_t *jcl, char *buf, int bufsize)
{
	char proto[256];
	char host[256];
	char resource[256];
	int sock, l, rl=0;
	char result[8192];

	xmltree_t *x;

	if (*jcl->password)
		return -1;

	if (0)//*jcl->password)
	{
		char body[4096];
		char header[4096];

		URL_Split(jcl->oauth2.refreshurl, proto, sizeof(proto), host, sizeof(host), resource, sizeof(resource));

		*body = 0;
		Q_strlcat(body, "client_id=", sizeof(body));
		Q_strlcat_urlencode(body, jcl->oauth2.clientid, sizeof(body));
		Q_strlcat(body, "&client_secret=", sizeof(body));
		Q_strlcat_urlencode(body, jcl->oauth2.clientsecret, sizeof(body));
		Q_strlcat(body, "&", sizeof(body));

		Q_strlcat(body, "grant_type=password&username=", sizeof(body));
		Q_strlcat_urlencode(body, jcl->oauth2.useraccount, sizeof(body));
		Q_strlcat(body, "&password=", sizeof(body));
		Q_strlcat_urlencode(body, jcl->password, sizeof(body));

		Q_strlcat(body, "&response_type=code", sizeof(body));

		Q_strlcat(body, "&redirect_uri=", sizeof(body));
		Q_strlcat_urlencode(body, "urn:ietf:wg:oauth:2.0:oob", sizeof(body));

		Q_strlcat(body, "&scope=", sizeof(body));
		Q_strlcat_urlencode(body, jcl->oauth2.scope, sizeof(body));

		Q_snprintf(header, sizeof(header),
			"POST %s HTTP/1.1\r\n"
			"Host: %s\r\n"
			//"Authorization: Basic %s\r\n"
			"Content-length: %i\r\n"
			"Content-Type: application/x-www-form-urlencoded\r\n"
			"Connection: close\r\n"
			"\r\n",
				resource,
				host,
				strlen(body));

		sock = pNet_TCPConnect(host, 443);
		pNet_SetTLSClient(sock, host);
		pNet_Send(sock, header, strlen(header));
		pNet_Send(sock, body, strlen(body));
		while(1)
		{
			l = pNet_Recv(sock, result+rl, sizeof(result)-rl);
			if (l < 0)
				break;
			else
				rl += l;
		}
		pNet_Close(sock);
		result[rl] = 0;

		Con_Printf("Got %s\n", result);
	}
	
	//if we have nothing, load up a browser to ask for the first token
	if (!*jcl->oauth2.refreshtoken && !*jcl->oauth2.authtoken)
	{
		char url[4096];
		*url = 0;
		Q_strlcat(url, jcl->oauth2.obtainurl, sizeof(url));
		Q_strlcat(url, "?redirect_uri=", sizeof(url));
		Q_strlcat_urlencode(url, "urn:ietf:wg:oauth:2.0:oob", sizeof(url));
		Q_strlcat(url, "&%72esponse_type=code&client_id=", sizeof(url));	//%72 = r. fucking ezquake colour codes. works with firefox anyway. no idea if that's the server changing it to an r or not. :s
		Q_strlcat_urlencode(url, jcl->oauth2.clientid, sizeof(url));
		Q_strlcat(url, "&scope=", sizeof(url));
		Q_strlcat_urlencode(url, jcl->oauth2.scope, sizeof(url));
		Q_strlcat(url, "&access_type=offline", sizeof(url));
		Q_strlcat(url, "&login_hint=", sizeof(url));
		Q_strlcat_urlencode(url, jcl->oauth2.useraccount, sizeof(url));

		Con_Printf("Please visit ^[^4%s\\url\\%s^] and then enter:\n^[/"COMMANDPREFIX"%i /oa2token <TOKEN>^]\nNote: you can right-click the link to copy it to your browser, and you can use ctrl+v to paste the resulting auth token as part of the given command.\n", url, url, jcl->accountnum);

		//wait for user to act.
		return -2;
	}

	//refresh token is not known, try and get one
	if (!*jcl->oauth2.refreshtoken && *jcl->oauth2.authtoken)
	{
		xmltree_t *x;
		char body[4096];
		char header[4096];

		//send a refresh request

		*body = 0;
		Q_strlcat(body, "code=", sizeof(body));
		Q_strlcat_urlencode(body, jcl->oauth2.authtoken, sizeof(body));
		Q_strlcat(body, "&client_id=", sizeof(body));
		Q_strlcat_urlencode(body, jcl->oauth2.clientid, sizeof(body));
		Q_strlcat(body, "&client_secret=", sizeof(body));
		Q_strlcat_urlencode(body, jcl->oauth2.clientsecret, sizeof(body));
		Q_strlcat(body, "&redirect_uri=", sizeof(body));
		Q_strlcat_urlencode(body, "urn:ietf:wg:oauth:2.0:oob", sizeof(body));
		Q_strlcat(body, "&grant_type=", sizeof(body));
		Q_strlcat_urlencode(body, "authorization_code", sizeof(body));
		URL_Split(jcl->oauth2.refreshurl, proto, sizeof(proto), host, sizeof(host), resource, sizeof(resource));

		Q_snprintf(header, sizeof(header),
			"POST %s HTTP/1.1\r\n"
			"Host: %s\r\n"
			//"Authorization: Basic %s\r\n"
			"Content-length: %i\r\n"
			"Content-Type: application/x-www-form-urlencoded\r\n"
			"user-agent: fteqw-plugin-xmpp\r\n"
			"Connection: close\r\n"
			"\r\n",
			resource, host, strlen(body));

		Con_Printf("XMPP: Requesting access token\n");
		sock = pNet_TCPConnect(host, 443);
		pNet_SetTLSClient(sock, host);
		pNet_Send(sock, header, strlen(header));
		pNet_Send(sock, body, strlen(body));
		while(1)
		{
			l = pNet_Recv(sock, result+rl, sizeof(result)-rl);
			if (l < 0)
				break;
			else
				rl += l;
		}
		result[rl] = 0;
		pNet_Close(sock);

		//should contain something like:
		//{
		//"access_token" : "ya29.AHES6ZR-_Sx0UpexZdgqQwR8LFqTx-GFi-Zrq4nKrcLLA98N7g",
		//"token_type" : "Bearer",
		//"expires_in" : 3600
		//}

		l = strstr(result, "\r\n\r\n")-result;
		l+= 4;
		if (l < 0 || l > rl)
			l = rl;
		x = XML_FromJSON(NULL, "oauth2", result, &l, rl);
		XML_ConPrintTree(x, "", 1);
		free(jcl->oauth2.accesstoken);
		free(jcl->oauth2.refreshtoken);
		jcl->oauth2.accesstoken = strdup(XML_GetChildBody(x, "access_token", ""));
//		jcl->oauth2.token_type = strdup(XML_GetChildBody(x, "token_type", ""));
//		jcl->oauth2.expires_in = strdup(XML_GetChildBody(x, "expires_in", ""));
		jcl->oauth2.refreshtoken = strdup(XML_GetChildBody(x, "refresh_token", ""));

		//in theory, the auth token is no longer valid/needed
		free(jcl->oauth2.authtoken);
		jcl->oauth2.authtoken = strdup("");
	}

	//refresh our refresh token, obtaining a usable sign-in token at the same time.
	else if (!*jcl->oauth2.accesstoken)
	{
		char body[4096];
		char header[4096];
		char *newrefresh;

		//send a refresh request

		*body = 0;
		Q_strlcat(body, "client_id=", sizeof(body));
		Q_strlcat_urlencode(body, jcl->oauth2.clientid, sizeof(body));
		Q_strlcat(body, "&client_secret=", sizeof(body));
		Q_strlcat_urlencode(body, jcl->oauth2.clientsecret, sizeof(body));
		Q_strlcat(body, "&grant_type=", sizeof(body));
		Q_strlcat_urlencode(body, "refresh_token", sizeof(body));
		Q_strlcat(body, "&refresh_token=", sizeof(body));
		Q_strlcat_urlencode(body, jcl->oauth2.refreshtoken, sizeof(body));
		URL_Split(jcl->oauth2.refreshurl, proto, sizeof(proto), host, sizeof(host), resource, sizeof(resource));

		Q_snprintf(header, sizeof(header),
			"POST %s HTTP/1.1\r\n"
			"Host: %s\r\n"
			//"Authorization: Basic %s\r\n"
			"Content-length: %i\r\n"
			"Content-Type: application/x-www-form-urlencoded\r\n"
			"user-agent: fteqw-plugin-xmpp\r\n"
			"Connection: close\r\n"
			"\r\n",
			resource, host, strlen(body));

		Con_Printf("XMPP: Refreshing access token\n");
		sock = pNet_TCPConnect(host, 443);
		pNet_SetTLSClient(sock, host);
		pNet_Send(sock, header, strlen(header));
		pNet_Send(sock, body, strlen(body));
		while(1)
		{
			l = pNet_Recv(sock, result+rl, sizeof(result)-rl);
			if (l < 0)
				break;
			else
				rl += l;
		}
		pNet_Close(sock);
		result[rl] = 0;

		l = strstr(result, "\r\n\r\n")-result;
		l+= 4;
		if (l < 0 || l > rl)
			l = rl;
		x = XML_FromJSON(NULL, "oauth2", result, &l, rl);
//		XML_ConPrintTree(x, "", 1);

		newrefresh = XML_GetChildBody(x, "refresh_token", NULL);
		free(jcl->oauth2.accesstoken);
		jcl->oauth2.accesstoken = strdup(XML_GetChildBody(x, "access_token", ""));
		if (newrefresh || !*jcl->oauth2.accesstoken)
		{
			free(jcl->oauth2.refreshtoken);
			jcl->oauth2.refreshtoken = strdup(XML_GetChildBody(x, "refresh_token", ""));
		}
//		jcl->oauth2.token_type = strdup(XML_GetChildBody(x, "token_type", ""));
//		jcl->oauth2.expires_in = strdup(XML_GetChildBody(x, "expires_in", ""));

		//refresh token may mutate. follow the mutation.
	}
	else if (*jcl->oauth2.accesstoken)
		Con_Printf("XMPP: Using explicit access token\n");

	if (*jcl->oauth2.accesstoken)
	{
		int len = 0;
		if (*jcl->oauth2.useraccount)
		{
			//realm isn't specified
			buf[len++] = 0;
			memcpy(buf+len, jcl->oauth2.useraccount, strlen(jcl->oauth2.useraccount));
			len += strlen(jcl->oauth2.useraccount);
			buf[len++] = 0;
		}
		memcpy(buf+len, jcl->oauth2.accesstoken, strlen(jcl->oauth2.accesstoken));
		len += strlen(jcl->oauth2.accesstoken);

		Con_Printf("XMPP: Signing in\n");
		free(jcl->oauth2.accesstoken);
		jcl->oauth2.accesstoken = strdup("");
		return len;
	}

	//if the reply has a refresh token in it, clear out any password info
	return -2;
}

//in descending priority order
saslmethod_t saslmethods[] =
{
	{"SCRAM-SHA-1",		sasl_scramsha1_initial,		sasl_scramsha1_challenge},	//lots of unreadable hashing
	{"DIGEST-MD5",		sasl_digestmd5_initial,		sasl_digestmd5_challenge},	//kinda silly
	{"PLAIN",			sasl_plain_initial,			NULL},						//realm\0username\0password
	{NULL,				sasl_oauth2_initial,		NULL}						//potentially avoids having to ask+store their password. a browser is required to obtain auth token for us.
};

/*
pidgin's msn request
https://oauth.live.com/authorize?client_id=000000004C07035A&scope=wl.messenger,wl.basic,wl.offline_access,wl.contacts_create,wl.share&response_type=token&redirect_uri=http://pidgin.im/
*/


struct subtree_s;

void JCL_AddClientMessagef(jclient_t *jcl, char *fmt, ...);
qboolean JCL_FindBuddy(jclient_t *jcl, char *jid, buddy_t **buddy, bresource_t **bres, qboolean create);
void JCL_GeneratePresence(jclient_t *jcl, qboolean force);
struct iq_s *JCL_SendIQf(jclient_t *jcl, qboolean (*callback) (jclient_t *jcl, struct subtree_s *tree, struct iq_s *iq), char *iqtype, char *target, char *fmt, ...);
struct iq_s *JCL_SendIQNode(jclient_t *jcl, qboolean (*callback) (jclient_t *jcl, xmltree_t *tree, struct iq_s *iq), char *iqtype, char *target, xmltree_t *node, qboolean destroynode);
void JCL_CloseConnection(jclient_t *jcl, const char *reason, qboolean reconnect);
void JCL_JoinMUCChat(jclient_t *jcl, char *room, char *server, char *myhandle, char *password);
static qboolean JCL_BuddyVCardReply(jclient_t *jcl, xmltree_t *tree, struct iq_s *iq);

void JCL_GenLink(jclient_t *jcl, char *out, int outlen, char *action, char *context, char *contextres, char *sid, char *txtfmt, ...)
{
	va_list		argptr;
	qboolean textonly = false;
	*out = 0;
	if (!strchr(txtfmt, '%'))
	{	//protect against potential bugs and exploits.
		Q_strlcpy(out, "bad link text", outlen);
		return;
	}
	//FIXME: validate that there is no \\ markup within any section that would break the link.
	//FIXME: validate that ^[ and ^] are not used, as that would also mess things up. add markup for every ^?
	if (textonly)
	{
		va_start(argptr, txtfmt);
		Q_strlcat(out, "[", outlen);
		Q_vsnprintf(out, outlen, txtfmt, argptr);
		Q_strlcat(out, "]", outlen);
		va_end(argptr);
		return;
	}
	Q_strlcat(out, "^[[", outlen);
	va_start(argptr, txtfmt);
	Q_vsnprintf(out+3, outlen-3, txtfmt, argptr);
	va_end(argptr);
	Q_strlcat(out, "]", outlen);

	if (jcl && jcl->accountnum)
	{
		char acc[32];
		Q_snprintf(acc, sizeof(acc), "%i", jcl->accountnum);
		Q_strlcat(out, "\\xmppacc\\", outlen);
		Q_strlcat(out, acc, outlen);
	}

	if (action)
	{
		Q_strlcat(out, "\\xmppact\\", outlen);
		Q_strlcat(out, action, outlen);
	}
	if (context)
	{
		Q_strlcat(out, "\\xmpp\\", outlen);
		Q_strlcat(out, context, outlen);
		if (contextres)
		{
			Q_strlcat(out, "/", outlen);
			Q_strlcat(out, contextres, outlen);
		}
	}
	if (sid)
	{
		Q_strlcat(out, "\\xmppsid\\", outlen);
		Q_strlcat(out, sid, outlen);
	}
	Q_strlcat(out, "^]", outlen);
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

qintptr_t JCL_ConsoleLinkMouseOver(qintptr_t *args)
{
	jclient_t *jcl;
//	char text[256];
	char link[256];
	char who[256];
	char what[256];
	char which[256];
	char *actiontext;
	int i;
	buddy_t *b, *me = NULL;
	bresource_t *br;
	float x = *(float*)&args[0];
	float y = *(float*)&args[1];

//	pCmd_Argv(0, text, sizeof(text));
	pCmd_Argv(1, link, sizeof(link));

	JCL_Info_ValueForKey(link, "xmpp", who, sizeof(who));
	JCL_Info_ValueForKey(link, "xmppact", what, sizeof(what));
	JCL_Info_ValueForKey(link, "xmppacc", which, sizeof(which));

	if (!*who)
		return false;

	i = atoi(which);
	i = bound(0, i, sizeof(jclients)/sizeof(jclients[0]));
	jcl = jclients[i];

	x += 16;

	if (!jcl)
		return false;

	if (jcl->status != JCL_ACTIVE)
	{
		pDraw_String(x, y, "^&C0You are currently offline");
		return true;
	}

	if (!strcmp(what, "pauth"))
		actiontext = "Befriend";
	else if (!strcmp(what, "pdeny"))
		actiontext = "Decline";
#ifdef FILETRANSFERS
	else if (!strcmp(what, "fauth") && (jcl->enabledcapabilities & CAP_SIFT))
		actiontext = "Receive";
	else if (!strcmp(what, "fdeny") && (jcl->enabledcapabilities & CAP_SIFT))
		actiontext = "Decline";
#endif
#ifdef JINGLE
	else if (!strcmp(what, "jauth") && (jcl->enabledcapabilities & (CAP_GAMEINVITE|CAP_VOICE|CAP_VIDEO|CAP_GOOGLE_VOICE)))
		actiontext = "Answer";
	else if (!strcmp(what, "jdeny") && (jcl->enabledcapabilities & (CAP_GAMEINVITE|CAP_VOICE|CAP_VIDEO|CAP_GOOGLE_VOICE)))
		actiontext = "Hang Up";
	else if (!strcmp(what, "join") && (jcl->enabledcapabilities & CAP_GAMEINVITE))
		actiontext = "Join Game";
	else if (!strcmp(what, "invite") && (jcl->enabledcapabilities & CAP_GAMEINVITE))
		actiontext = "Invite To Game";
	else if (!strcmp(what, "call") && (jcl->enabledcapabilities & (CAP_VOICE|CAP_GOOGLE_VOICE)))
		actiontext = "Call";
	else if (!strcmp(what, "vidcall") && (jcl->enabledcapabilities & CAP_VIDEO))
		actiontext = "Video Call";
#endif
	else if (!strcmp(what, "mucjoin"))
		actiontext = "Join Chat:";
	else if ((*who && !*what) || !strcmp(what, "msg"))
		actiontext = "Chat With";
	else
		return false;

	JCL_FindBuddy(jcl, who, &b, &br, false);
	if (!b)
		return false;
	JCL_FindBuddy(jcl, jcl->jid, &me, NULL, true);

	if ((jcl->enabledcapabilities & CAP_AVATARS) && BUILTINISVALID(Draw_LoadImageData))
	{
		if (b->vcardphotochanged && b->friended && !jcl->avatarupdate)
		{
			b->vcardphotochanged = false;
			Con_Printf("Querying %s's photo\n", b->accountdomain);
			jcl->avatarupdate = JCL_SendIQf(jcl, JCL_BuddyVCardReply, "get", b->accountdomain, "<vCard xmlns='vcard-temp'/>");
		}
		if (b->image)
		{
			//xep-0153: The image height and width SHOULD be between thirty-two (32) and ninety-six (96) pixels; the recommended size is sixty-four (64) pixels high and sixty-four (64) pixels wide.
			//96 just feels far too large for a game that was origionally running at a resolution of 320*200.
			//FIXME: we should proably respect the image's aspect ratio...
#define IMGSIZE 96/2
			pDraw_Image (x, y, IMGSIZE, IMGSIZE, 0, 0, 1, 1, b->image);
			x += IMGSIZE+8;
		}
	}

	pDraw_String(x, y, va("^&F0%s ^2%s", actiontext, b->name));
	y+=8;
	pDraw_String(x, y, va("^&F0%s", b->accountdomain));
	y+=8;
	if (br)
	{
		pDraw_String(x, y, va("^&F0  %s", br->resource));
		y+=8;
	}
	if (b == me)
		pDraw_String(x, y, "^&90" "You");
	else if (!b->friended)
		pDraw_String(x, y, "^&C0" "Unknown");
	y+=8;

	return true;
}
qintptr_t JCL_ConsoleLink(qintptr_t *args)
{
	jclient_t *jcl;
	char link[256];
	char who[256];
	char what[256];
	char which[256];
	int i;
//	pCmd_Argv(0, text, sizeof(text));
	pCmd_Argv(1, link, sizeof(link));

	JCL_Info_ValueForKey(link, "xmpp", who, sizeof(who));
	JCL_Info_ValueForKey(link, "xmppact", what, sizeof(what));
	JCL_Info_ValueForKey(link, "xmppacc", which, sizeof(which));

	if (!*who && !*what)
		return false;

	i = atoi(which);
	i = bound(0, i, sizeof(jclients)/sizeof(jclients[0]));
	jcl = jclients[i];

	jclient_updatebuddylist = true;

	if (!strcmp(what, "pauth"))
	{
		//we should friend them too.
		if (jcl && jcl->status == JCL_ACTIVE)
			JCL_AddClientMessagef(jcl, "<presence to='%s' type='subscribed'/>", who);
		return true;
	}
	else if (!strcmp(what, "pdeny"))
	{
		if (jcl && jcl->status == JCL_ACTIVE)
			JCL_AddClientMessagef(jcl, "<presence to='%s' type='unsubscribed'/>", who);
		return true;
	}
#ifdef FILETRANSFERS
	else if (!strcmp(what, "fauth") && (jcl->enabledcapabilities & CAP_SIFT))
	{
		JCL_Info_ValueForKey(link, "xmppsid", what, sizeof(what));
		if (jcl && jcl->status == JCL_ACTIVE)
			XMPP_FT_AcceptFile(jcl, atoi(what), true);
		return true;
	}
	else if (!strcmp(what, "fdeny") && (jcl->enabledcapabilities & CAP_SIFT))
	{
		JCL_Info_ValueForKey(link, "xmppsid", what, sizeof(what));
		if (jcl && jcl->status == JCL_ACTIVE)
			XMPP_FT_AcceptFile(jcl, atoi(what), false);
		return true;
	}
#endif
#ifdef JINGLE
	//jauth/jdeny are used to accept/cancel all jingle/gingle content types.
	else if (!strcmp(what, "jauth") && (jcl->enabledcapabilities & (CAP_VOICE|CAP_VIDEO|CAP_GAMEINVITE|CAP_GOOGLE_VOICE)))
	{
		JCL_Info_ValueForKey(link, "xmppsid", what, sizeof(what));
		if (jcl && jcl->status == JCL_ACTIVE)
			JCL_Join(jcl, who, what, true, ICEP_INVALID);
		return true;
	}
	else if (!strcmp(what, "jdeny") && (jcl->enabledcapabilities & (CAP_VOICE|CAP_VIDEO|CAP_GAMEINVITE|CAP_GOOGLE_VOICE)))
	{
		JCL_Info_ValueForKey(link, "xmppsid", what, sizeof(what));
		if (jcl && jcl->status == JCL_ACTIVE)
			JCL_Join(jcl, who, what, false, ICEP_INVALID);
		return true;
	}
	else if (!strcmp(what, "join") && (jcl->enabledcapabilities & CAP_GAMEINVITE))
	{
		if (jcl && jcl->status == JCL_ACTIVE)
			JCL_Join(jcl, who, NULL, true, ICEP_QWCLIENT);
		return true;
	}
	else if (!strcmp(what, "invite") && (jcl->enabledcapabilities & CAP_GAMEINVITE))
	{
		if (jcl && jcl->status == JCL_ACTIVE)
			JCL_Join(jcl, who, NULL, true, ICEP_QWSERVER);
		return true;
	}
	else if (!strcmp(what, "call") && (jcl->enabledcapabilities & (CAP_VOICE|CAP_GOOGLE_VOICE)))
	{
		if (jcl && jcl->status == JCL_ACTIVE)
			JCL_Join(jcl, who, NULL, true, ICEP_VOICE);
		return true;
	}
	else if (!strcmp(what, "vidcall") && (jcl->enabledcapabilities & (CAP_VIDEO)))
	{
		if (jcl && jcl->status == JCL_ACTIVE)
			JCL_Join(jcl, who, NULL, true, ICEP_VIDEO);
		return true;
	}
#endif
	else if (!strcmp(what, "mucjoin"))
	{	//conference/chat join
		if (jcl)
		{
			JCL_Info_ValueForKey(link, "xmppsid", what, sizeof(what));
			JCL_JoinMUCChat(jcl, who, NULL, NULL, what);
		}
	}
	else if (!strcmp(what, "addfriend"))
	{
		if (jcl)
		{
			pCon_SetConsoleFloat(BUDDYLISTTITLE, "linebuffered", true);
			pCon_SetConsoleString(BUDDYLISTTITLE, "footer", "Please enter your friend's account name");
			jclient_action_cl = jcl;
			jclient_action_buddy = NULL;
			jclient_action = ACT_ADDFRIEND;
		}
	}
	else if (!strcmp(what, "connect"))
	{
		if (jcl && (jcl->status == JCL_INACTIVE || jcl->status == JCL_DEAD))
		{
			jcl->status = JCL_DEAD;	//flag it as still trying to connect.
			jcl->timeout = jclient_curtime;	//do it now
		}
	}
	else if (!strcmp(what, "disconnect"))
	{
		if (jcl)
		{
			if (jcl->status == JCL_INACTIVE)
				JCL_CloseConnection(jcl, "", false);
			else
			{
				JCL_CloseConnection(jcl, "", true);
				jcl->status = JCL_INACTIVE;
			}
		}
	}
	else if (!strcmp(what, "newaccount"))
	{
		pCon_SetConsoleFloat(BUDDYLISTTITLE, "linebuffered", true);
		pCon_SetConsoleString(BUDDYLISTTITLE, "footer", "Please enter your XMPP account name\neg: example@"EXAMPLEDOMAIN);
		jclient_action_cl = jcl;
		jclient_action_buddy = NULL;
		jclient_action = ACT_NEWACCOUNT;
	}
	else if (!strcmp(what, "buddyopts"))
	{
		char footer[2048];
		char chatlink[512];
		char unfriend[512];
		char realias[512];
		buddy_t *b = NULL;
		JCL_FindBuddy(jcl, *who?who:jcl->defaultdest, &b, NULL, false);
		if (b)
		{
			JCL_GenLink(jcl, chatlink, sizeof(chatlink), NULL, b->accountdomain, NULL, NULL, "Chat with %s", b->name);
			JCL_GenLink(jcl, unfriend, sizeof(unfriend), NULL, b->accountdomain, NULL, NULL, "Unfriend %s", b->name);
			JCL_GenLink(jcl, realias, sizeof(realias), NULL, b->accountdomain, NULL, NULL, "Set alias for %s", b->name);
			Q_snprintf(footer, sizeof(footer), "\n%s\n%s\n%s", chatlink, unfriend, realias);

			pCon_SetConsoleString(BUDDYLISTTITLE, "footer", footer);
		}
	}
	else if ((*who && !*what) || !strcmp(what, "msg"))
	{
		if (jcl)
		{
			char *f;
			buddy_t *b = NULL;
			bresource_t *br = NULL;
			JCL_FindBuddy(jcl, *who?who:jcl->defaultdest, &b, &br, true);
			if (b)
			{
				f = b->accountdomain;
				b->defaultresource = br;

				if (BUILTINISVALID(Con_SetConsoleFloat))
				{
					pCon_SetConsoleString(f, "title", b->name);
					pCon_SetConsoleFloat(f, "iswindow", true);
					pCon_SetConsoleFloat(f, "forceutf8", true);
					pCon_SetConsoleFloat(f, "wnd_w", 256);
					pCon_SetConsoleFloat(f, "wnd_h", 320);
				}
				else if (BUILTINISVALID(Con_SubPrint))
					pCon_SubPrint(f, "");
				if (BUILTINISVALID(Con_SetActive))
					pCon_SetActive(f);
			}
		}
		return true;
	}
	else
	{
		Con_Printf("Unsupported xmpp action (%s) in link\n", what);
	}

	return false;
}

void JCL_ToJID(jclient_t *jcl, const char *in, char *out, int outsize, qboolean assumeresource)
{
	//decompose links first
	if (in[0] == '^' && in[1] == '[')
	{
		const char *sl;
		const char *le;
		sl = in+2;
		sl = strchr(in, '\\');
		if (sl)
		{
			le = strstr(sl, "^]");
			if (le)
			{
				char info[512];
				if (le-sl < 512)
				{
					memcpy(info, sl, le-sl);
					info[le-sl] = 0;
					JCL_Info_ValueForKey(info, "xmpp", out, outsize);
				}
				return;
			}
		}
	}

	if (!strchr(in, '@') && jcl)
	{
		//no @? probably its an alias, but could also be a server/domain perhaps. not sure we care. you'll just have to rename your friend.
		//check to see if we can find a friend going by that name
		//fixme: allow resources to make it through here
		buddy_t *b;
		for (b = jcl->buddies; b; b = b->next)
		{
			if (!strcasecmp(b->name, in))
			{
				if (b->defaultresource && assumeresource)
					Q_snprintf(out, outsize, "%s/%s", b->accountdomain, b->defaultresource->resource);
				else
					Q_strlcpy(out, b->accountdomain, outsize);
				return;
			}
		}
	}
	
	if (assumeresource)
	{
		buddy_t *b;
		for (b = jcl->buddies; b; b = b->next)
		{
			if (!strcasecmp(b->accountdomain, in))
			{
				if (b->defaultresource && assumeresource)
					Q_snprintf(out, outsize, "%s/%s", b->accountdomain, b->defaultresource->resource);
				else
					Q_strlcpy(out, b->accountdomain, outsize);
				return;
			}
		}
	}

	//a regular jabber account name
	Q_strlcpy(out, in, outsize);
}

void XMPP_AddFriend(jclient_t *jcl, const char *account, const char *nick)
{
	char jid[256];
	//FIXME: validate the name. deal with xml markup.
	//try and make sense of the name given
	JCL_ToJID(jcl, account, jid, sizeof(jid), false);
	if (!strchr(jid, '@'))
		Con_Printf("Missing @ character. Trying anyway, but this will be assumed to be a server rather than a user.\n");

	//can also rename. We should probably read back the groups for the update.
	JCL_SendIQf(jcl, NULL, "set", NULL, "<query xmlns='jabber:iq:roster'><item jid='%s' name='%s'></item></query>", jid, nick);

	//start looking for em
	JCL_AddClientMessagef(jcl, "<presence to='%s' type='subscribe'/>", jid);

	//let em see us
	if (jcl->preapproval)
		JCL_AddClientMessagef(jcl, "<presence to='%s' type='subscribed'/>", jid);
}
jclient_t *JCL_Connect(int accnum, char *server, int forcetls, char *account, char *password);
qintptr_t JCL_ConExecuteCommand(qintptr_t *args)
{
	buddy_t *b;
	char consolename[256];
	jclient_t *jcl;
	int i;

	pCmd_Argv(0, consolename, sizeof(consolename));

	if (!strcmp(consolename, BUDDYLISTTITLE))
	{
		char args[512];
		pCmd_Args(args, sizeof(args));
		pCon_SetConsoleFloat(BUDDYLISTTITLE, "linebuffered", false);
		pCon_SetConsoleString(BUDDYLISTTITLE, "footer", "");
		jclient_updatebuddylist = true;

		//FIXME: validate that the client is still active.
		switch(jclient_action)
		{
		case ACT_NONE:
			break;
		case ACT_OAUTH:
			jcl = jclient_action_cl;
			if (jcl)
			{
				free(jcl->oauth2.authtoken);
				jcl->oauth2.authtoken = strdup(args);
				if (jcl->status == JCL_INACTIVE)
					jcl->status = JCL_DEAD;
			}
			break;
		case ACT_NEWACCOUNT:
			if (!*args)
				break;	//they didn't enter anything! oh well.
			for (i = 0; i < sizeof(jclients)/sizeof(jclients[0]); i++)
			{
				if (jclients[i])
					continue;
				jclients[i] = JCL_Connect(i, "", BUILTINISVALID(Net_SetTLSClient)?1:0, args, "");
				break;
			}
			if (i == sizeof(jclients)/sizeof(jclients[0]))
				pCon_SetConsoleString(BUDDYLISTTITLE, "footer", "Too many accounts open");
			else if (!jclients[i])
				pCon_SetConsoleString(BUDDYLISTTITLE, "footer", "Unable to create account");
			else
			{	//now ask for a password instantly. because oauth2 is basically unusable.
				jclients[i]->status = JCL_INACTIVE;
				pCon_SetConsoleFloat(BUDDYLISTTITLE, "linebuffered", true);
				pCon_SetConsoleString(BUDDYLISTTITLE, "footer", "Please enter password");
				jclient_action_cl = jclients[i];
				jclient_action_buddy = NULL;
				jclient_action = ACT_PASSWORD;
				return true;
			}
			break;
		case ACT_PASSWORD:
			if (*args)
				Q_strncpyz(jclient_action_cl->password, args, sizeof(jclient_action_cl->password));
			if (jclient_action_cl->status == JCL_INACTIVE)
				jclient_action_cl->status = JCL_DEAD;
			jclient_action = ACT_NONE;
			return 2;	//ask to not store in history.
		case ACT_ADDFRIEND:
			if (*args)
				XMPP_AddFriend(jclient_action_cl, args, "");
			break;
		case ACT_SETALIAS:
			Q_strncpyz(jclient_action_buddy->name, args, sizeof(jclient_action_buddy->name));
			JCL_SendIQf(jclient_action_cl, NULL, "set", NULL, "<query xmlns='jabber:iq:roster'><item jid='%s' name='%s'></item></query>", jclient_action_buddy->accountdomain, jclient_action_buddy->name);
			break;
		}
		jclient_action = ACT_NONE;
		return true;
	}

	for (i = 0; i < sizeof(jclients) / sizeof(jclients[0]); i++)
	{
		jcl = jclients[i];
		if (!jcl)
			continue;
		for (b = jcl->buddies; b; b = b->next)
		{
			if (!strcmp(b->accountdomain, consolename))
			{
				if (b->defaultresource)
					Q_snprintf(jcl->defaultdest, sizeof(jcl->defaultdest), "%s/%s", b->accountdomain, b->defaultresource->resource);
				else
					Q_snprintf(jcl->defaultdest, sizeof(jcl->defaultdest), "%s", b->accountdomain);
				JCL_Command(i, consolename);
				return true;
			}
		}
	}
	for (i = 0; i < sizeof(jclients) / sizeof(jclients[0]); i++)
	{
		jcl = jclients[i];
		if (!jcl)
			continue;
		JCL_Command(i, consolename);
		return true;
	}

	Con_SubPrintf(consolename, "You were disconnected\n");
	return true;
}

void JCL_FlushOutgoing(jclient_t *jcl)
{
	int sent;
	if (!jcl || !jcl->outbuflen || jcl->socket == -1)
		return;

	sent = pNet_Send(jcl->socket, jcl->outbuf + jcl->outbufpos, jcl->outbuflen);	//FIXME: This needs rewriting to cope with errors.
	if (sent > 0)
	{
		//and print it on some subconsole if we're debugging
		if (jcl->streamdebug)
		{
			char t = jcl->outbuf[jcl->outbufpos+sent];
			jcl->outbuf[jcl->outbufpos+sent] = 0;
			XMPP_ConversationPrintf("xmppout", "xmppout", COLOURYELLOW "%s\n", jcl->outbuf + jcl->outbufpos);
			jcl->outbuf[jcl->outbufpos+sent] = t;
		}

		jcl->outbufpos += sent;
		jcl->outbuflen -= sent;
	}
	else if (sent < 0)
		Con_Printf("XMPP: Error sending\n");
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
	char body[4096];

	va_start (argptr, fmt);
	Q_vsnprintf (body, sizeof(body), fmt, argptr);
	va_end (argptr);

	JCL_AddClientMessageString(jcl, body);
}
qboolean JCL_Reconnect(jclient_t *jcl)
{
	char *serveraddr;
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
	jcl->authmode = -1;

	if (*jcl->redirserveraddr)
		serveraddr = jcl->redirserveraddr;
	else
		serveraddr = jcl->serveraddr;

	if (!*serveraddr)
	{
		//jcl->tlsconnect requires an explicit hostname, so should not be able to take this path.
		char srv[256];
		char srvserver[256];
		Q_snprintf(srv, sizeof(srv), "_xmpp-client._tcp.%s", jcl->domain);
		if (NET_DNSLookup_SRV(srv, srvserver, sizeof(srvserver)))
		{
			Con_DPrintf("XMPP: Trying to connect to %s (%s)\n", jcl->domain, srvserver);
			jcl->socket = pNet_TCPConnect(srvserver, jcl->serverport);	//port is should already be part of the srvserver name
		}
		else
		{
			Q_strncpyz(jcl->errormsg, "Unable to determine service", sizeof(jcl->errormsg));
			return false;
		}
	}
	else
	{
		Con_DPrintf("XMPP: Trying to connect to %s\n", jcl->domain);
		jcl->socket = pNet_TCPConnect(serveraddr, jcl->serverport);	//port is only used if the url doesn't contain one. It's a default.
	}

	//not yet blocking. So no frequent attempts please...
	//non blocking prevents connect from returning worthwhile sensible value.
	if ((int)jcl->socket < 0)
	{
		Q_strncpyz(jcl->errormsg, "Unable to connect", sizeof(jcl->errormsg));
		return false;
	}

	jcl->issecure = false;
	if (jcl->forcetls==2)
		if (pNet_SetTLSClient(jcl->socket, jcl->certificatedomain)>=0)
			jcl->issecure = true;

	jcl->status = JCL_AUTHING;

	JCL_AddClientMessageString(jcl,
		"<?xml version='1.0' ?>"
		"<stream:stream to='");
	JCL_AddClientMessageString(jcl, jcl->domain);
	JCL_AddClientMessageString(jcl, "' xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams' version='1.0'>");

	jclient_updatebuddylist = true;

	return true;
}

jclient_t *JCL_ConnectXML(xmltree_t *acc)
{
	//this is a list of xmpp networks that use oauth2 for which we have a clientid registered. this allows us to get the right oauth defaults.
	struct 
	{
		char *domain;
		char *saslmethod;
		char *obtainurl;
		char *refreshurl;
		char *revokeurl;
		char *scope;
		char *clientid;
		char *clientsecret;
		//char *signouturl;
		//char *clientregistrationurl;
	} *oa, dfltoauth[] = {
		{
			"gmail.com",
			"X-OAUTH2",
			"https://accounts.google.com/o/oauth2/auth",	//authorize
			"https://accounts.google.com/o/oauth2/token",	//refresh
			"https://accounts.google.com/o/oauth2/revoke",	//revoke
			"https://www.googleapis.com/auth/googletalk",	//scope
			"1060926168015.apps.googleusercontent.com",		//clientid
			"mptCRRTE5I626npsnoZ_RqoG"						//client secrit. there really is no securing this. I'll just have to avoid any pay-for google apis. *shrug*
			//"https://accounts.google.com/IssuedAuthSubTokens"
			//"https://code.google.com/apis/console"
		},
/*
		{
			"messenger.live.com",
			"X-MESSENGER-OAUTH2",
			"https://oauth.live.com/authorize",
			"https://oauth.live.com/token",
			"",	//FIXME fill in revoke url
			"wl.messenger,wl.basic,wl.offline_access,wl.contacts_create,wl.share",	//no idea what's actually needed.
			"",	//client-id - none registered. go register it yourself.
			""	//client-secret - client not registered, go do it yourself.
			//""
			//"https://manage.dev.live.com/"
		},
*/
/*
		{
			"messenger.live.com",
			"X-FACEBOOK-PLATFORM",
			"", //FIXME fill in	obtain url
			"",	//FIXME fill in	refresh url
			"",	//FIXME fill in revoke url
			"",	//FIXME: fill in scope
			"",	//client-id - none registered. go register it yourself.
			""	//client-secret - client not registered, go do it yourself.
			//""
			//""
		},
*/
		{
			NULL,
			"",
			"",
			"",
			"",
			"",
			"",
			""
		}
	};
	jclient_t *jcl;
	xmltree_t *oauth2;
	xmltree_t *features;
	char oauthname[256];
	struct buddyinfo_s *bi;
	int bn;

	jcl = malloc(sizeof(jclient_t));
	if (!jcl)
		return NULL;

	memset(jcl, 0, sizeof(jclient_t));
	jcl->socket = -1;

	jcl->enabledcapabilities = CAP_DEFAULTENABLEDCAPS;

	jcl->accountnum = atoi(XML_GetParameter(acc, "id", "1"));

	//make sure dependant properties are listed beneath their dependancies...
	jcl->forcetls = atoi(XML_GetChildBody(acc, "forcetls", "1"));
	jcl->streamdebug = atoi(XML_GetChildBody(acc, "streamdebug", "0"));
	jcl->streamdebug = bound(0, jcl->streamdebug, 2);
	Q_strlcpy(jcl->serveraddr, XML_GetChildBody(acc, "serveraddr", ""), sizeof(jcl->serveraddr));
	jcl->serverport = atoi(XML_GetChildBody(acc, "serverport", (jcl->forcetls==2)?"5223":"5222"));
	Q_strlcpy(jcl->username, XML_GetChildBody(acc, "username", "user"), sizeof(jcl->username));
	Q_strlcpy(jcl->domain, XML_GetChildBody(acc, "domain", "localhost"), sizeof(jcl->domain));
	Q_strlcpy(jcl->resource, XML_GetChildBody(acc, "resource", ""), sizeof(jcl->resource));

	//half these networks seem to have weird domains. especially microsoft.
	if (strchr(jcl->username, '@'))
		Q_strlcpy(oauthname, jcl->username, sizeof(oauthname));
	else
		Q_snprintf(oauthname, sizeof(oauthname), "%s@%s", jcl->username, jcl->domain);
	for (oa = dfltoauth; oa->domain; oa++)
	{
		if (!strcmp(oa->domain, jcl->domain))
			break;
	}
	oauth2 = XML_ChildOfTree(acc, "oauth2", 0);
	Q_strlcpy(jcl->oauth2.saslmethod, XML_GetParameter(oauth2, "method", oa->saslmethod), sizeof(jcl->oauth2.saslmethod));
	Q_strlcpy(jcl->oauth2.obtainurl, XML_GetChildBody(oauth2, "obtain-url", oa->obtainurl), sizeof(jcl->oauth2.obtainurl));
	Q_strlcpy(jcl->oauth2.refreshurl, XML_GetChildBody(oauth2, "refresh-url", oa->refreshurl), sizeof(jcl->oauth2.refreshurl));
	Q_strlcpy(jcl->oauth2.clientid, XML_GetChildBody(oauth2, "client-id", oa->clientid), sizeof(jcl->oauth2.clientid));
	Q_strlcpy(jcl->oauth2.clientsecret, XML_GetChildBody(oauth2, "client-secret", oa->clientsecret), sizeof(jcl->oauth2.clientsecret));
	jcl->oauth2.scope = strdup(XML_GetChildBody(oauth2, "scope", oa->scope));
	jcl->oauth2.useraccount = strdup(XML_GetChildBody(oauth2, "authname", oauthname));
	jcl->oauth2.authtoken = strdup(XML_GetChildBody(oauth2, "auth-token", ""));
	jcl->oauth2.refreshtoken = strdup(XML_GetChildBody(oauth2, "refresh-token", ""));
	jcl->oauth2.accesstoken = strdup(XML_GetChildBody(oauth2, "access-token", ""));

	Q_strlcpy(jcl->password, XML_GetChildBody(acc, "password", ""), sizeof(jcl->password));

	if (!*jcl->resource)
	{	//the default resource matches the game that they're trying to play.
		char gamename[64], *res, *o;
		if (pCvar_GetString("fs_gamename", gamename, sizeof(gamename)))
		{
			//strip out any weird chars (namely whitespace)
			for (o = gamename, res = gamename; *res; )
			{
				if (*res == ' ' || *res == '\t')
					res++;
				else
					*o++ = *res++;
			}
			if (!*gamename)
				Q_strlcpy(jcl->resource, "FTE", sizeof(jcl->resource));
			else
				Q_strlcpy(jcl->resource, gamename, sizeof(jcl->resource));
		}
	}
	Q_strlcpy(jcl->certificatedomain, XML_GetChildBody(acc, "certificatedomain", jcl->domain), sizeof(jcl->certificatedomain));
	jcl->status = atoi(XML_GetChildBody(acc, "inactive", "0"))?JCL_INACTIVE:JCL_DEAD;
	jcl->allowauth_plainnontls	= atoi(XML_GetChildBody(acc, "allowauth_plain_nontls",	"0"));
	jcl->allowauth_plaintls		= atoi(XML_GetChildBody(acc, "allowauth_plain_tls",		"1"));	//required 1 for googletalk, otherwise I'd set it to 0.
	jcl->allowauth_digestmd5	= atoi(XML_GetChildBody(acc, "allowauth_digest_md5",	"1"));
	jcl->allowauth_scramsha1	= atoi(XML_GetChildBody(acc, "allowauth_scram_sha_1",	"1"));
	jcl->allowauth_oauth2		= atoi(XML_GetChildBody(acc, "allowauth_oauth2",		jcl->oauth2.saslmethod?"1":"0"));

	jcl->savepassword	= atoi(XML_GetChildBody(acc, "savepassword",	"0"));

	features = XML_ChildOfTree(acc, "features", 0);
	if (features && XML_GetParameter(features, "ver", JCL_BUILD))
	{
		char *val;
		int j;
		for (j = 0; capnames[j].names; j++)
		{
			val = XML_GetChildBody(features, capnames[j].names, NULL);
			if (val)
			{
				if (atoi(val))
					jcl->enabledcapabilities |= capnames[j].cap;
				else
					jcl->enabledcapabilities &= ~capnames[j].cap;
			}
		}
	}


	features = XML_ChildOfTree(acc, "buddyinfo", 0);
	for (bn=0; ; bn++)
	{
		xmltree_t *b = XML_ChildOfTree(features, "buddy", bn);
		if (b)
		{
			char *buddyid = XML_GetParameter(b, "name", JCL_BUILD);
			char *buddyimage = XML_GetChildBody(b, "image", NULL);
			char *buddyimagemime = XML_GetChildBody(b, "imagemime", NULL);
			char *buddyimagehash = XML_GetChildBody(b, "imagehash", NULL);

			bi = malloc(sizeof(*bi) + strlen(buddyid));
			strcpy(bi->accountdomain, buddyid);
			bi->image = buddyimage?strdup(buddyimage):NULL;
			bi->imagemime = buddyimagemime?strdup(buddyimagemime):NULL;
			bi->imagehash = buddyimagehash?strdup(buddyimagehash):NULL;
			bi->next = jcl->buddyinfo;
			jcl->buddyinfo = bi;
		}
		else
			break;
	}

	return jcl;
}

jclient_t *JCL_Connect(int accnum, char *server, int forcetls, char *account, char *password)
{
	char gamename[64];
	jclient_t *jcl;
	char *domain;
	char *res;
	xmltree_t *x;

	res = TrimResourceFromJid(account);
	if (!res || !*res)
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

	if (forcetls>0)
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
#ifdef DEFAULTDOMAIN
		domain = DEFAULTDOMAIN;
		Con_Printf("XMPP: domain not specified, assuming %s\n", domain);
#else
		Con_Printf("XMPP: domain not specified\n");
		return NULL;
#endif
	}

	x = XML_CreateNode(NULL, "account", "", "");
	XML_AddParameteri(x, "id", accnum);
	XML_CreateNode(x, "serveraddr", "", server);
	XML_CreateNode(x, "username", "", account);
	XML_CreateNode(x, "domain", "", domain);
	XML_CreateNode(x, "resource", "", res);
	XML_CreateNode(x, "password", "", password);
	jcl = JCL_ConnectXML(x);
	XML_Destroy(x);
	return jcl;
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

struct stringprep_range
{
	unsigned int cp_min;
	unsigned int cp_max;
	unsigned int remap[2];
};
struct stringprep_range stringprep_A1[] =
{
	{0x0234,0x024F},
	{0x02AE,0x02AF},
	{0x02EF,0x02FF},
	{0x0350,0x035F},
	{0x0370,0x0373},
	{0x0376,0x0379},
	{0x037B,0x037D},
	{0x037F,0x0383},
	{0x038B},
	{0x038D},
	{0x03A2},
	{0x03CF},
	{0x03F7,0x03FF},
	{0x0487},
	{0x04CF},
	{0x04F6,0x04F7},
	{0x04FA,0x04FF},
	{0x0510,0x0530},
	{0x0557,0x0558},
	{0x0560},
	{0x0588},
	{0x058B,0x0590},
	{0x05A2},
	{0x05BA},
	{0x05C5,0x05CF},
	{0x05EB,0x05EF},
	{0x05F5,0x060B},
	{0x0600,~0},	//FIXME rest of A.1 (utf)
};
struct stringprep_range stringprep_B1[] =
{
	{0x00AD},
	{0x034F},
	{0x1806},
	{0x180B},
	{0x180C},
	{0x180D},
	{0x200B},
	{0x200C},
	{0x200D},
	{0x2060},
	{0xFE00},
	{0xFE01},
	{0xFE02},
	{0xFE03},
	{0xFE04},
	{0xFE05},
	{0xFE06},
	{0xFE07},
	{0xFE08},
	{0xFE09},
	{0xFE0A},
	{0xFE0B},
	{0xFE0C},
	{0xFE0D},
	{0xFE0E},
	{0xFE0F},
	{0xFEFF},
};
struct stringprep_range stringprep_B2[] =
{
	{0x0041, 0x005A,	{0x0061}},
	{0x00B5, 0x00B5,	{0x03BC}},
	{0x00C0, 0x00DE,	{0x00E0}},
	{0x00DF, 0,			{0x0073, 0x0073}},
};
struct stringprep_range stringprep_C1[] =
{
	//C1.1
	{0x0020},
	//C2.2
	{0x0020},
	{0x00A0},
	{0x1680},
	//FIXME... utf
};
struct stringprep_range stringprep_C2[] =
{
	//C.2.1
	{0x0000, 0x001F},
	{0x007F, 0x007F},
	//C.2.1
	{0x0080, 0x009F},			//C.2.2
	//FIXME... utf
	{0x06DD, ~0},
};
struct stringprep_range stringprep_C3[] =
{
	{0xE000, 0xF8FF},
	{0xF0000, 0xFFFFD},
	{0x100000, 0x10FFFD},
};
struct stringprep_range stringprep_C4[] =
{
	{0xFDD0, 0xFDEF},
	{0xFFFE, 0xFFFF},
	{0x1FFFE, 0x1FFFF},
	{0x2FFFE, 0x2FFFF},
	{0x3FFFE, 0x3FFFF},
	{0x4FFFE, 0x4FFFF},
	{0x5FFFE, 0x5FFFF},
	{0x6FFFE, 0x6FFFF},
	{0x7FFFE, 0x7FFFF},
	{0x8FFFE, 0x8FFFF},
	{0x9FFFE, 0x9FFFF},
	{0xAFFFE, 0xAFFFF},
	{0xBFFFE, 0xBFFFF},
	{0xCFFFE, 0xCFFFF},
	{0xDFFFE, 0xDFFFF},
	{0xEFFFE, 0xEFFFF},
	{0xFFFFE, 0xFFFFF},
	{0x10FFFE, 0x10FFFF},
};
struct stringprep_range stringprep_C5[] =
{
	{0xD800, 0xDFFF},
};
struct stringprep_range stringprep_C6[] =
{
	{0xFFF9},
	{0xFFFA},
	{0xFFFB},
	{0xFFFC},
	{0xFFFD},
};
struct stringprep_range stringprep_C7[] =
{
	{0x2FF0, 0x2FFB},
};
struct stringprep_range stringprep_C8[] =
{
	{0x0340},
	{0x0341},
	{0x200E},
	{0x200F},
	{0x202A},
	{0x202B},
	{0x202C},
	{0x202D},
	{0x202E},
	{0x206A},
	{0x206B},
	{0x206C},
	{0x206D},
	{0x206E},
	{0x206F},
};
struct stringprep_range stringprep_C9[] =
{
	{0xE0001},
	{0xE0001},
	{0xE0020, 0xE007F},
};

static struct stringprep_range *StringPrep_InRange(unsigned int codepoint, struct stringprep_range *range, size_t slots)
{
	int i;
	for (i = 0; i < slots; i++)
	{
		if (codepoint < range[i].cp_min)
			return NULL;	//early out
		if (range[i].cp_max)
		{
			if (codepoint < range[i].cp_max)
				return &range[i];
		}
		else
		{
			if (codepoint == range[i].cp_min)
				return &range[i];
		}
	}
	return NULL;
}
#define StringPrep_InRange(cp,r) StringPrep_InRange(cp,r,sizeof(r)/sizeof(r[0]))

static qboolean JCL_NamePrep(const char *in, size_t insize, char *out, size_t outsize)
{
	int j;
	unsigned int offset;
	unsigned int codepoint;
	struct stringprep_range *mapping;
	struct stringprep_range nomap;
	outsize--;
	while (insize --> 0)
	{
		codepoint = *in++;
		if (codepoint >= 0x80)
			return false;	//FIXME: utf-8 decode.

		if (StringPrep_InRange(codepoint, stringprep_A1)) return false;	//no unassigned codepoints

		mapping = StringPrep_InRange(codepoint, stringprep_B1);
		if (!mapping)
			mapping = StringPrep_InRange(codepoint, stringprep_B2);
		if (!mapping)
		{	//no mapping, its probably fine.
			nomap.cp_min = codepoint;
			nomap.cp_max = codepoint;
			nomap.remap[0] = codepoint;
			nomap.remap[1] = 0;
			mapping = &nomap;
		}
		offset = codepoint - mapping->cp_min;

		for (j = 0; j < sizeof(mapping->remap)/sizeof(mapping->remap[0]) && mapping->remap[j]; j++)
		{
			codepoint = mapping->remap[j] + offset;
			if (StringPrep_InRange(codepoint, stringprep_C1)) return false;
			if (StringPrep_InRange(codepoint, stringprep_C2)) return false;
			if (StringPrep_InRange(codepoint, stringprep_C3)) return false;
			if (StringPrep_InRange(codepoint, stringprep_C4)) return false;
			if (StringPrep_InRange(codepoint, stringprep_C5)) return false;
			if (StringPrep_InRange(codepoint, stringprep_C6)) return false;
			if (StringPrep_InRange(codepoint, stringprep_C7)) return false;
			if (StringPrep_InRange(codepoint, stringprep_C8)) return false;
			if (StringPrep_InRange(codepoint, stringprep_C9)) return false;

			//FIXME: utf-8 encode
			if (outsize < 1)
				return false;
			outsize--;
			*out++ = codepoint;
		}
	}
	*out = 0;
	return true;
}

static qboolean JCL_NameResourcePrep(const char *in, char *nout, size_t noutsize, char **res)
{
	char *resstart = strchr(in, '/');
	if (resstart)
	{
		*res = resstart+1;
		//FIXME: resprep the resource.
		if (!JCL_NamePrep(in, resstart-in, nout, noutsize))
			return false;
	}
	else
	{
		*res = NULL;
		if (!JCL_NamePrep(in, strlen(in), nout, noutsize))
			return false;
	}
	return true;
}

//FIXME: add flags to avoid creation
qboolean JCL_FindBuddy(jclient_t *jcl, char *jid, buddy_t **buddy, bresource_t **bres, qboolean create)
{
	char name[256];
	char *res;
	buddy_t *b;
	bresource_t *r = NULL;
	if (!jid)
	{
		if (buddy)
			*buddy = NULL;
		if (bres)
			*bres = NULL;
		return false;
	}

	if (!JCL_NameResourcePrep(jid, name, sizeof(name), &res))
		return false;	//nameprep failed.

	for (b = jcl->buddies; b; b = b->next)
	{
		if (!strcmp(b->accountdomain, name))
			break;
	}
	if (!b && create)
	{
		b = malloc(sizeof(*b) + strlen(name));
		memset(b, 0, sizeof(*b));
		b->next = jcl->buddies;
		jcl->buddies = b;
//		b->vcardphotochanged = true;	//don't know what it is, query their photo as needed. google sucks, and things stop working.
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
		if (!r && create)
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

	if (bres)
		return *bres != NULL;
	return *buddy != NULL;
}

void JCL_IQTimeouts(jclient_t *jcl)
{
	struct iq_s *iq, **link;
	for (link = &jcl->pendingiqs; *link; )
	{
		iq = *link;
		if (iq->timeout < jclient_curtime)
		{
			iq = *link;
			*link = iq->next;
			if (iq->callback)
			{
				Con_DPrintf("IQ timeout with %s\n", iq->to);
				iq->callback(jcl, NULL, iq);
			}
			free(iq);
		}
		else
			link = &(*link)->next;
	}
}
struct iq_s *JCL_SendIQ(jclient_t *jcl, qboolean (*callback) (jclient_t *jcl, xmltree_t *tree, struct iq_s *iq), char *iqtype, char *target, char *body)
{
	struct iq_s *iq;

	if (!target)
		target = "";

	iq = malloc(sizeof(*iq) + strlen(target));
	iq->next = jcl->pendingiqs;
	jcl->pendingiqs = iq;
	Q_snprintf(iq->id, sizeof(iq->id), "%i", rand());
	iq->callback = callback;
	iq->timeout = jclient_curtime + 30*1000;
	strcpy(iq->to, target);

	if (*target)
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
	char *s = XML_GenerateString(node, false);
	n = JCL_SendIQ(jcl, callback, iqtype, target, s);
	free(s);
	if (destroynode)
		XML_Destroy(node);
	return n;
}

qboolean XMPP_NewGoogleMailsReply(jclient_t *jcl, xmltree_t *tree, struct iq_s *iq)
{
	int i, j;
	xmltree_t *mailbox = XML_ChildOfTreeNS(tree, "google:mail:notify", "mailbox", 0);
//	char *url = XML_GetParameter(mailbox, "url", "");
//	int totalmatched = atoi(XML_GetParameter(mailbox, "total-matched", "0"));
//	char *resulttime = XML_GetParameter(mailbox, "result-time", "");
	xmltree_t *mailthread;
	xmltree_t *senders, *sender;
	char *subject;
	char *sendername;
	char *cleanup;
	char *mailurl;

	for (i = 0; ; i++)
	{
		mailthread = XML_ChildOfTree(mailbox, "mail-thread-info", i);
		if (!mailthread)
			break;

//		tid = XML_GetParameter(mailthread, "tid", "");
		mailurl = XML_GetParameter(mailthread, "url", "");
//		participation = XML_GetParameter(mailthread, "participation", "");
//		messages = XML_GetParameter(mailthread, "messages", "");
//		date = XML_GetParameter(mailthread, "date", "");
//		labels = XML_GetChildBody(mailthread, "labels", "");
		subject = XML_GetChildBody(mailthread, "subject", "");
		if (!*subject)
			subject = XML_GetChildBody(mailthread, "snippet", "<NO SUBJECT>");

		senders = XML_ChildOfTree(mailthread, "senders", 0);
		for (j = 0; ; j++)
		{
			sender = XML_ChildOfTree(senders, "sender", j);
			if (!sender)
				break;
//			address = XML_GetParameter(sender, "address", "");
			sendername = XML_GetParameter(sender, "name", "");
			if (!*sendername)
				sendername = XML_GetParameter(sender, "address", "");
//			originator = XML_GetParameter(sender, "originator", "");
			if (atoi(XML_GetParameter(sender, "unread", "1")))
			{
				//we trust the server to not feed us gibberish like \r or \n chars.
				//however, other chars may be problematic and could break/hack the link markup.
				for (cleanup = sendername; (cleanup = strchr(cleanup, '^')) != NULL; )
					*cleanup = ' ';
				for (cleanup = sendername; (cleanup = strchr(cleanup, '\\')) != NULL; )
					*cleanup = '/';
				for (cleanup = subject; (cleanup = strchr(cleanup, '^')) != NULL; )
					*cleanup = ' ';
				for (cleanup = subject; (cleanup = strchr(cleanup, '\\')) != NULL; )
					*cleanup = '/';
				for (cleanup = mailurl; (cleanup = strstr(cleanup, "^]")) != NULL; )
					*cleanup = '_';	//FIXME: %5E
				for (cleanup = mailurl; (cleanup = strchr(cleanup, '\\')) != NULL; )
					*cleanup = '/';	//FIXME: %5C
				Con_Printf("^[^4New spam from %s: %s\\url\\%s^]\n", sendername, subject, mailurl);
			}
		}
	}
	return true;
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
		JCL_FindBuddy(jcl, jid, &buddy, NULL, true);

		if (*name)
			Q_strlcpy(buddy->name, name, sizeof(buddy->name));
		else
			buddy->vcardphotochanged = true;	//try to query their actual name
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
			char myjid[512];
			Q_strlcpy(jcl->jid, c->body, sizeof(jcl->jid));
			JCL_GenLink(jcl, myjid, sizeof(myjid), NULL, jcl->jid, NULL, NULL, "%s", jcl->jid);
			Con_DPrintf("Bound to jid %s\n", jcl->jid);
			return true;
		}
	}
	return false;
}
static qboolean JCL_BuddyVCardReply(jclient_t *jcl, xmltree_t *tree, struct iq_s *iq)
{
	char myself[256];
	char photodata[65536];
	xmltree_t *vc, *photo, *photobinval;
	const char *nickname;
	const char *photomime;

	buddy_t *b = NULL;
	char *from = iq->to;

	if (!*from)
	{
		Q_snprintf(myself, sizeof(myself), "%s@%s", jcl->username, jcl->domain);
		from = myself;
	}

	if (jcl->avatarupdate == iq)
	{
		if (!tree)
			Con_DPrintf("timeout for %s's photo\n", iq->to);
		else
			Con_DPrintf("Response for %s's photo\n", iq->to);
		jcl->avatarupdate = NULL;
	}

	JCL_FindBuddy(jcl, from, &b, NULL, true);
	if (!b)
	{
		Con_DPrintf("unknown vcard from %s\n", from);
		return false;
	}

	jclient_updatebuddylist = true;	//make sure any new info is displayed properly.

	vc = XML_ChildOfTree(tree, "vCard", 0);
	if (!vc)
		Con_DPrintf("invalid vcard from %s\n", from);
	else
	{
		if (!strcmp(b->name, b->accountdomain))
		{
			nickname = XML_GetChildBody(vc, "NICKNAME", NULL);
			if (!nickname)
				nickname = XML_GetChildBody(vc, "FN", NULL);
			if (nickname)
				Q_strlcpy(b->name, nickname, sizeof(b->name));
		}

		photo = XML_ChildOfTree(vc, "PHOTO", 0);
		photobinval = XML_ChildOfTree(photo, "BINVAL", 0);

		if ((jcl->enabledcapabilities & CAP_AVATARS) && BUILTINISVALID(Draw_LoadImageData))
		{
			if (photobinval)
			{
				struct buddyinfo_s *bi;
				unsigned int photosize = Base64_Decode(photodata, sizeof(photodata), photobinval->body, strlen(photobinval->body));
				photomime = XML_GetChildBody(photo, "TYPE", "");
				//xep-0153: If the <TYPE/> is something other than image/gif, image/jpeg, or image/png, it SHOULD be ignored.
				if (strcmp(photomime, "image/png") && strcmp(photomime, "image/jpeg") && strcmp(photomime, "image/gif"))
					photomime = "";
				b->image = pDraw_LoadImageData(va("xmpp/%s.png", b->accountdomain), photomime, photodata, photosize);
				Con_DPrintf("vcard photo updated from %s\n", from);

				for (bi = jcl->buddyinfo; bi; bi = bi->next)
				{
					if (!strcmp(bi->accountdomain, b->accountdomain))
						break;
				}
				if (!bi)
				{
					bi = malloc(sizeof(*bi)+strlen(b->accountdomain));
					memset(bi, 0, sizeof(*bi));
					strcpy(bi->accountdomain, b->accountdomain);
					bi->next = jcl->buddyinfo;
					jcl->buddyinfo = bi;
				}

				if (bi)
				{
					char *hex = "0123456789abcdef";
					char hash[20];
					char hasha[41];
					int i, o;
					free(bi->image);
					free(bi->imagehash);
					free(bi->imagemime);
					SHA1(hash, sizeof(hash), photodata, photosize);
					for (i = 0, o = 0; i < sizeof(hash); i++)
					{
						hasha[o++] = hex[(hash[i]>>4) & 0xf];
						hasha[o++] = hex[(hash[i]>>0) & 0xf];
					}
					hasha[o] = 0;
					bi->imagehash = strdup(hasha);
					bi->image = strdup(photobinval->body);
					bi->imagemime = strdup(photomime);
				}
			}
			else
			{
				b->image = pDraw_LoadImageData(va("xmpp/%s.png", b->accountdomain), "", NULL, 0);
				Con_DPrintf("vcard photo invalidated from %s\n", from);
			}
		}
		else
			Con_DPrintf("vcard photo ignored from %s\n", from);
	}

	return true;
}
static qboolean JCL_MyVCardReply(jclient_t *jcl, xmltree_t *tree, struct iq_s *iq)
{
	char photodata[65536];
	char digest[20];
	xmltree_t *vc, *fn, *nickname, *photo, *photobinval;

	//hack the from parametmer so it looks legit
	Q_snprintf(photodata, sizeof(photodata), "%s@%s", jcl->username, jcl->domain);

	//make sure our image is loaded etc
	JCL_BuddyVCardReply(jcl, tree, iq);

	vc = XML_ChildOfTree(tree, "vCard", 0);
	fn = XML_ChildOfTree(vc, "FN", 0);
	nickname = XML_ChildOfTree(vc, "NICKNAME", 0);

	photo = XML_ChildOfTree(vc, "PHOTO", 0);
	photobinval = XML_ChildOfTree(photo, "BINVAL", 0);
	if (!tree || !photobinval)
	{
		//server doesn't support vcards?
		if (jcl->vcardphotohashstatus != VCP_NONE)
		{
			jcl->vcardphotohashstatus = VCP_NONE;
			jcl->vcardphotohashchanged = true;
			*jcl->vcardphotohash = 0;
		}
	}
	else
	{
		int photosize = Base64_Decode(photodata, sizeof(photodata), photobinval->body, strlen(photobinval->body));
		SHA1(digest, sizeof(digest), photodata, photosize);
		if (jcl->vcardphotohashstatus != VCP_KNOWN || memcmp(jcl->vcardphotohash, digest, sizeof(jcl->vcardphotohash)))
		{
			memcpy(jcl->vcardphotohash, digest, sizeof(jcl->vcardphotohash));
			jcl->vcardphotohashchanged = true;
			jcl->vcardphotohashstatus = VCP_KNOWN;
		}
	}

	if (nickname && *nickname->body)
		Q_strlcpy(jcl->localalias, nickname->body, sizeof(jcl->localalias));
	else if (fn && *fn->body)
		Q_strlcpy(jcl->localalias, fn->body, sizeof(jcl->localalias));
	return true;
}
static qboolean JCL_ServerFeatureReply(jclient_t *jcl, xmltree_t *tree, struct iq_s *iq)
{
	xmltree_t *query = XML_ChildOfTreeNS(tree, "http://jabber.org/protocol/disco#info", "query", 0);
	xmltree_t *feature;
	char *featurename;
	int f;
	qboolean gmail = false;

	if (!query)
		return false;

	for (f = 0; ; f++)
	{
		feature = XML_ChildOfTree(query, "feature", f);
		if (!feature)
			break;
		featurename = XML_GetParameter(feature, "var", "");
		if (!strcmp(featurename, "google:mail:notify"))
			gmail = true;
		else
		{
			Con_DPrintf("Server supports feature %s\n", featurename);
		}
	}

	if (gmail)
		JCL_SendIQf(jcl, XMPP_NewGoogleMailsReply, "get", NULL, "<query xmlns='google:mail:notify'/>");
	
	return true;
}
static qboolean JCL_SessionReply(jclient_t *jcl, xmltree_t *tree, struct iq_s *iq)
{
	JCL_SendIQf(jcl, JCL_RosterReply, "get", NULL, "<query xmlns='jabber:iq:roster'/>");
	JCL_SendIQf(jcl, JCL_MyVCardReply, "get", NULL, "<vCard xmlns='vcard-temp'/>");
	JCL_SendIQf(jcl, JCL_ServerFeatureReply, "get", jcl->domain, "<query xmlns='http://jabber.org/protocol/disco#info'/>");
	return true;
}

static struct
{
	char *name;
	unsigned int withcap;
} caps[] =
{
#if 1
	{"http://jabber.org/protocol/caps"},
	{"http://jabber.org/protocol/disco#info"},
//	{"http://jabber.org/protocol/disco#items"},

	{"jabber:iq:version"},
	#ifdef JINGLE
		{"urn:xmpp:jingle:1", CAP_GAMEINVITE|CAP_VOICE|CAP_VIDEO},
		{QUAKEMEDIAXMLNS, CAP_GAMEINVITE},
		#ifdef VOIP
			#ifdef VOIP_LEGACY
				{"http://www.google.com/xmpp/protocol/session", CAP_GOOGLE_VOICE},	//so google's non-standard clients can chat with us
				{"http://www.google.com/xmpp/protocol/voice/v1", CAP_GOOGLE_VOICE}, //so google's non-standard clients can chat with us
				{"http://www.google.com/xmpp/protocol/camera/v1", CAP_GOOGLE_VOICE},	//can send video
//				{"http://www.google.com/xmpp/protocol/video/v1", CAP_GOOGLE_VOICE},	//can receive video
			#endif
			#ifndef VOIP_LEGACY_ONLY
				{"urn:xmpp:jingle:apps:rtp:1", CAP_VOICE|CAP_VIDEO},
				{"urn:xmpp:jingle:apps:rtp:audio", CAP_VOICE},
				{"urn:xmpp:jingle:apps:rtp:video", CAP_VIDEO},
			#endif
		#endif
		//"urn:xmpp:jingle:apps:rtp:video",//we don't support rtp video chat
		{"urn:xmpp:jingle:transports:raw-udp:1", CAP_GAMEINVITE|CAP_VOICE|CAP_VIDEO},
		#ifndef NOICE
			{"urn:xmpp:jingle:transports:ice-udp:1", CAP_GAMEINVITE|CAP_VOICE|CAP_VIDEO},
		#endif
	#endif
	#ifndef Q3_VM
		{"urn:xmpp:time"},
	#endif
	{"urn:xmpp:ping"},	//FIXME: I'm not keen on this. I only added support to stop errors from pidgin when trying to debug.
	{"urn:xmpp:attention:0"},	//poke.

	//file transfer
	#ifdef FILETRANSFERS
		{"http://jabber.org/protocol/si", CAP_SIFT},
		{"http://jabber.org/protocol/si/profile/file-transfer", CAP_SIFT},
		{"http://jabber.org/protocol/ibb", CAP_SIFT},
		{"http://jabber.org/protocol/bytestreams", CAP_SIFT},
	#endif
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
	"http://jabber.org/protocol/si/profile/file-transfer",
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
	{NULL}
};
static void buildcaps(jclient_t *jcl, char *out, int outlen)
{
	int i;
	Q_strncpyz(out, "<identity category='client' type='pc' name='FTEQW'/>", outlen);

	for (i = 0; caps[i].name; i++)
	{
		if (!(caps[i].withcap & jcl->enabledcapabilities))
			continue;
		Q_strlcat(out, "<feature var='", outlen);
		Q_strlcat(out, caps[i].name, outlen);
		Q_strlcat(out, "'/>", outlen);
	}
}
static int qsortcaps(const void *va, const void *vb)
{
	char *a = *(char**)va;
	char *b = *(char**)vb;
	return strcmp(a, b);
}
char *buildcapshash(jclient_t *jcl)
{
	int i, l;
	char out[8192];
	int outlen = sizeof(out);
	unsigned char digest[64];
	Q_strlcpy(out, "client/pc//FTEQW<", outlen);
	qsort(caps, sizeof(caps)/sizeof(caps[0]) - 1, sizeof(caps[0]), qsortcaps); 
	for (i = 0; caps[i].name; i++)
	{
		if (!(caps[i].withcap & jcl->enabledcapabilities))
			continue;
		Q_strlcat(out, caps[i].name, outlen);
		Q_strlcat(out, "<", outlen);
	}
	l = SHA1(digest, sizeof(digest), out, strlen(out));
	for (i = 0; i < l; i++)
		Base64_Byte(digest[i]);
	Base64_Finish();
	return base64;
}

//xep-0115 1.4+
//xep-0153
char *buildcapsvcardpresence(jclient_t *jcl, char *caps, size_t sizeofcaps)
{
	char *vcard;
	char *voiceext = "";	//xep-0115 1.0
#ifdef VOIP_LEGACY
	if (jcl->enabledcapabilities & CAP_GOOGLE_VOICE)
	{
		if (jcl->enabledcapabilities & CAP_VIDEO)
			voiceext = " ext='voice-v1 camera-v1 video-v1'";
		else
			voiceext = " ext='voice-v1'";
	}
#endif

	Q_snprintf(caps, sizeofcaps,
		"<c xmlns='http://jabber.org/protocol/caps'"
		" hash='sha-1'"
		" node='"DISCONODE"'"
		" ver='%s'"
		"%s/>"
		, buildcapshash(jcl), voiceext);

	//xep-0153
	vcard = caps+strlen(caps);
	sizeofcaps -= strlen(caps);
	if (jcl->vcardphotohashstatus == VCP_NONE)
	{
		//let other people know that we don't have one. yay. pointless. whatever.
		Q_snprintf(vcard, sizeofcaps,
				"<x xmlns='vcard-temp:x:update'><photo/></x>");
	}
	else if (jcl->vcardphotohashstatus == VCP_KNOWN)
	{
		unsigned char *hex = "0123456789abcdef";
		char inhex[41];
		int i, o;
		for (i = 0, o = 0; i < sizeof(jcl->vcardphotohash); i++)
		{
			inhex[o++] = hex[(jcl->vcardphotohash[i]>>4) & 0xf];
			inhex[o++] = hex[(jcl->vcardphotohash[i]>>0) & 0xf];
		}
		inhex[o] = 0;

		//if we know the vcard hash, we must tell other people what it is or if its changed, etc.
		Q_snprintf(vcard, sizeofcaps,
			"<x xmlns='vcard-temp:x:update'><photo>%s</photo></x>", inhex);
	}
	else
	{
		//always include a vcard update tag.
		//this says that we won't corrupt other resource's vcard.
		//note that googletalk seems to hack the current vcard hash anyway. don't test this feature on that network.
		Q_snprintf(vcard, sizeofcaps,
			"<x xmlns='vcard-temp:x:update'/>");
	}
	return caps;
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
	from = XML_GetParameter(tree, "from", jcl->domain);
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
				char hashednode[1024];
				char *node = XML_GetParameter(ot, "node", NULL);
				unparsable = false;

				buildcaps(jcl, msg, sizeof(msg));
				Q_snprintf(hashednode, sizeof(hashednode),DISCONODE"#%s", buildcapshash(jcl));

				if (!node || !strcmp(node, hashednode) || !strcmp(node, DISCONODE"#") )
				{
					JCL_AddClientMessagef(jcl,
							"<iq type='result' to='%s' id='%s'>"
								"<query xmlns='http://jabber.org/protocol/disco#info' node='%s'>"
									"%s"
								"</query>"
							"</iq>", from, id, node?node:hashednode, msg);
				}
#ifdef VOIP_LEGACY
				else if (!strcmp(node, DISCONODE"#voice-v1") && (jcl->enabledcapabilities & CAP_GOOGLE_VOICE))
				{
					JCL_AddClientMessagef(jcl,
							"<iq type='result' to='%s' id='%s'>"
								"<query xmlns='http://jabber.org/protocol/disco#info' node='%s'>"
									"<feature var='http://www.google.com/xmpp/protocol/voice/v1'/>"
								"</query>"
							"</iq>", from, id, node, msg);
				}
				else if (!strcmp(node, DISCONODE"#camera-v1") && (jcl->enabledcapabilities & CAP_GOOGLE_VOICE))
				{
					JCL_AddClientMessagef(jcl,
							"<iq type='result' to='%s' id='%s'>"
								"<query xmlns='http://jabber.org/protocol/disco#info' node='%s'>"
									"<feature var='http://www.google.com/xmpp/protocol/camera/v1'/>"
								"</query>"
							"</iq>", from, id, node, msg);
				}
				else if (!strcmp(node, DISCONODE"#video-v1") && (jcl->enabledcapabilities & CAP_GOOGLE_VOICE))
				{
					JCL_AddClientMessagef(jcl,
							"<iq type='result' to='%s' id='%s'>"
								"<query xmlns='http://jabber.org/protocol/disco#info' node='%s'>"
									"<feature var='http://www.google.com/xmpp/protocol/video/v1'/>"
								"</query>"
							"</iq>", from, id, node, msg);
				}
#endif
				else
				{
					JCL_AddClientMessagef(jcl,
							"<iq type='error' to='%s' id='%s'>"
								"<error code='404' type='cancel'>"
									"<item-not-found/>"
								"</error>"
							"</iq>", from, id);
				}
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
			XML_ConPrintTree(tree, "", 0);

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

#ifdef FILETRANSFERS
		if (XMPP_FT_ParseIQSet(jcl, from, id, tree))
			return;
#endif

		c = XML_ChildOfTree(tree, "query", 0);
		if (c && !strcmp(c->xmlns, "jabber:iq:roster") && !strcmp(from, jcl->domain))
		{
			unparsable = false;
			JCL_RosterUpdate(jcl, c);
		}

		//google-specific - new mail notifications.
		c = XML_ChildOfTree(tree, "new-mail", 0);
		if (c && !strcmp(c->xmlns, "google:mail:notify") && !strcmp(from, jcl->domain))
		{
			JCL_AddClientMessagef(jcl, "<iq type='result' to='%s' id='%s' />", from, id);
			JCL_SendIQf(jcl, XMPP_NewGoogleMailsReply, "get", "", "<query xmlns='google:mail:notify'/>");
			return;
		}

#ifdef JINGLE
		c = XML_ChildOfTreeNS(tree, "urn:xmpp:jingle:1", "jingle", 0);
		if (c && (jcl->enabledcapabilities & (CAP_GAMEINVITE|CAP_VOICE|CAP_VIDEO)))
			unparsable = !JCL_ParseJingle(jcl, c, from, id);
#ifdef VOIP_LEGACY
		c = XML_ChildOfTreeNS(tree, "http://www.google.com/session", "session", 0);
		if (c && (jcl->enabledcapabilities & (CAP_GOOGLE_VOICE)))
			unparsable = !JCL_HandleGoogleSession(jcl, c, from, id);
#endif
#endif



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
		char *from = XML_GetParameter(tree, "from", jcl->domain);
		unparsable = false;
		for (link = &jcl->pendingiqs; *link; link = &(*link)->next)
		{
			iq = *link;
			if (!strcmp(iq->id, id) && (!strcmp(iq->to, from) || !*iq->to))
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
					XML_ConPrintTree(tree, "", 0);
				}
			}
			free(iq);
		}
		else
		{
			Con_Printf("Unrecognised iq result from %s\n", from);
//			XML_ConPrintTree(tree, "", 0);
		}
	}
	
	if (unparsable)
	{
		unparsable = false;
		Con_Printf("Unrecognised iq type\n");
		XML_ConPrintTree(tree, "", 0);
	}
}
void XMPP_ConversationPrintf(const char *context, const char *title, char *format, ...)
{
	va_list		argptr;
	static char		string[1024];

	va_start (argptr, format);
	Q_vsnprintf (string, sizeof(string), format,argptr);
	va_end (argptr);

	if (*context && BUILTINISVALID(Con_GetConsoleFloat) && pCon_GetConsoleFloat(context, "iswindow") < true)
	{
		pCon_SetConsoleFloat(context, "iswindow", true);
		pCon_SetConsoleFloat(context, "forceutf8", true);
		pCon_SetConsoleFloat(context, "wnd_w", 256);
		pCon_SetConsoleFloat(context, "wnd_h", 320);
		pCon_SetConsoleString(context, "title", title);
	}
	Con_TrySubPrint(context, string);
}
void JCL_ParseMessage(jclient_t *jcl, xmltree_t *tree)
{
	xmltree_t *ot;
	qboolean unparsable = true;
	char *f = XML_GetParameter(tree, "from", jcl->domain);
	char *type = XML_GetParameter(tree, "type", "normal");
	char *ctx = f;

	if (!strcmp(f, jcl->jid))
		unparsable = false;
	else
	{
		if (f)
		{
			buddy_t *b = NULL;
			bresource_t *br = NULL;
			Q_strlcpy(jcl->defaultdest, f, sizeof(jcl->defaultdest));

			JCL_FindBuddy(jcl, f, &b, &br, true);
			if (b)
			{
				ctx = b->accountdomain;
				if (!strcmp(type, "groupchat"))
					f = br->resource;
				else if (b->chatroom)
				{
					ctx = f;	//one server with multiple rooms requires that we retain resource info.
					f = br->resource;
				}
				else
				{
					f = b->name;
					if (br)
						b->defaultresource = br;
				}
			}
		}

		if (!strcmp(type, "error"))
		{
			char *reason = NULL;
			ot = XML_ChildOfTree(tree, "error", 0);
			if (ot->child)
				reason = ot->child->name;
			if (XML_ChildOfTree(ot, "remote-server-not-found", 0))		reason = "Remote Server Not Found";
			if (XML_ChildOfTree(ot, "bad-request", 0))					reason = "Bad Request";
			if (XML_ChildOfTree(ot, "conflict", 0))						reason = "Conflict Error";
			if (XML_ChildOfTree(ot, "feature-not-implemented", 0))		reason = "feature-not-implemented";
			if (XML_ChildOfTree(ot, "forbidden", 0))					reason = "forbidden";
			if (XML_ChildOfTree(ot, "gone", 0))							reason = "'gone' Error";
			if (XML_ChildOfTree(ot, "internal-server-error", 0))		reason = "internal-server-error";
			if (XML_ChildOfTree(ot, "item-not-found", 0))				reason = "item-not-found";
			if (XML_ChildOfTree(ot, "jid-malformed", 0))				reason = "jid-malformed";
			if (XML_ChildOfTree(ot, "not-acceptable", 0))				reason = "not-acceptable";
			if (XML_ChildOfTree(ot, "not-allowed", 0))					reason = "not-allowed";
			if (XML_ChildOfTree(ot, "not-authorized", 0))				reason = "not-authorized";
			if (XML_ChildOfTree(ot, "policy-violation", 0))				reason = "policy-violation";
			if (XML_ChildOfTree(ot, "recipient-unavailable", 0))		reason = "recipient-unavailable";
			if (XML_ChildOfTree(ot, "redirect", 0))						reason = "'redirect' Error";
			if (XML_ChildOfTree(ot, "registration-required", 0))		reason = "registration-required";
			if (XML_ChildOfTree(ot, "remote-server-not-found", 0))		reason = "remote-server-not-found";
			if (XML_ChildOfTree(ot, "remote-server-timeout", 0))		reason = "remote-server-timeout";
			if (XML_ChildOfTree(ot, "resource-constraint", 0))			reason = "resource-constraint";
			if (XML_ChildOfTree(ot, "service-unavailable", 0))			reason = "service-unavailable";
			if (XML_ChildOfTree(ot, "subscription-required", 0))		reason = "subscription-required";
			if (XML_ChildOfTree(ot, "undefined-condition", 0))			reason = "undefined-condition";
			if (XML_ChildOfTree(ot, "unexpected-request", 0))			reason = "unexpected-request";

			ot = XML_ChildOfTree(tree, "body", 0);
			if (ot)
			{
				unparsable = false;
				if (reason)
					XMPP_ConversationPrintf(ctx, f, "^1Error: %s (%s): ", reason, f);
				else
					XMPP_ConversationPrintf(ctx, f, "^1error sending message to %s: ", f);
				if (f)
				{
					if (!strncmp(ot->body, "/me ", 4))
						XMPP_ConversationPrintf(ctx, f, "* ^2%s^7%s\n", ((!strcmp(jcl->localalias, ">>"))?"me":jcl->localalias), ot->body+3);
					else
						XMPP_ConversationPrintf(ctx, f, "%s\n", ot->body);
				}
			}
			else
				XMPP_ConversationPrintf(ctx, f, "error sending message to %s\r", f);
			return;
		}

		if (f)
		{
			ot = XML_ChildOfTree(tree, "composing", 0);
			if (ot && !strcmp(ot->xmlns, "http://jabber.org/protocol/chatstates"))
			{
				unparsable = false;
				XMPP_ConversationPrintf(ctx, f, "%s is typing\r", f);
			}
			ot = XML_ChildOfTree(tree, "paused", 0);
			if (ot && !strcmp(ot->xmlns, "http://jabber.org/protocol/chatstates"))
			{
				unparsable = false;
				XMPP_ConversationPrintf(ctx, f, "%s has stopped typing\r", f);
			}
			ot = XML_ChildOfTree(tree, "inactive", 0);
			if (ot && !strcmp(ot->xmlns, "http://jabber.org/protocol/chatstates"))
			{
				unparsable = false;
				XMPP_ConversationPrintf(ctx, f, "\r", f);
			}
			ot = XML_ChildOfTree(tree, "active", 0);
			if (ot && !strcmp(ot->xmlns, "http://jabber.org/protocol/chatstates"))
			{
				unparsable = false;
				XMPP_ConversationPrintf(ctx, f, "\r", f);
			}
			ot = XML_ChildOfTree(tree, "gone", 0);
			if (ot && !strcmp(ot->xmlns, "http://jabber.org/protocol/chatstates"))
			{
				unparsable = false;
				XMPP_ConversationPrintf(ctx, f, "%s has gone away\r", f);
			}
		}

		ot = XML_ChildOfTree(tree, "attention", 0);
		if (ot)
		{
			if (jclient_poketime < jclient_curtime)	//throttle these.
			{
				jclient_poketime = jclient_curtime + 10*1000;
				XMPP_ConversationPrintf(ctx, f, "%s is an attention whore.\n", f);
				if (BUILTINISVALID(Con_SetActive))
					pCon_SetActive(ctx);
				if (BUILTINISVALID(LocalSound))
					pLocalSound("misc/talk.wav");
			}
		}
		
		ot = XML_ChildOfTree(tree, "subject", 0);
		if (ot && !strcmp(type, "groupchat"))
		{
			unparsable = false;
			XMPP_ConversationPrintf(ctx, f, "^2%s^7 has set the topic to: %s\n", f, ot->body);
		}


		ot = XML_ChildOfTreeNS(tree, "http://jabber.org/protocol/muc#user", "x", 0);
		if (ot && f && !strchr(f, '/'))
		{
			//this is an appaling extension protocol. we really have no way to know if someone's just making this shit up just to see our presence.
			//this message came from the groupchat server.
			xmltree_t *inv = XML_ChildOfTree(ot, "invite", 0);
			if (inv)
			{
				char *who = XML_GetParameter(inv, "from", jcl->domain);
				char *reason = XML_GetChildBody(inv, "reason", NULL);
				char *password = XML_GetChildBody(ot, "password", 0);
				char link[512];
				buddy_t *b;
				if (JCL_FindBuddy(jcl, f, &b, NULL, true))
				{
					if (b->chatroom)
						return;	//we already know about it. don't spam.
					JCL_GenLink(jcl, link, sizeof(link), "mucjoin", f, NULL, password, "%s", f);
//					ctx = who;
					if (reason)
						XMPP_ConversationPrintf(ctx, f, "* ^2%s^7 has invited you to join %s: %s.\n", who, link, reason);
					else
						XMPP_ConversationPrintf(ctx, f, "* ^2%s^7 has invited you to join %s.\n", who, link);
					if (BUILTINISVALID(Con_SetActive))
						pCon_SetActive(ctx);
				}
				return; //ignore any body/jabber:x:conference
			}
		}

		ot = XML_ChildOfTreeNS(tree, "jabber:x:conference", "x", 0);
		if (ot)
		{
			char link[512];
			char *chatjit = XML_GetParameter(ot, "jid", "");
			char *reason = XML_GetParameter(ot, "reason", NULL);
			char *password = XML_GetParameter(ot, "password", NULL);
			unparsable = false;

			JCL_GenLink(jcl, link, sizeof(link), "mucjoin", chatjit, NULL, password, "%s", chatjit);
			if (reason)
				XMPP_ConversationPrintf(ctx, f, "* ^2%s^7 has invited you to join %s: %s.\n", f, link, reason);
			else
				XMPP_ConversationPrintf(ctx, f, "* ^2%s^7 has invited you to join %s.\n", f, link);
			if (BUILTINISVALID(Con_SetActive))
				pCon_SetActive(ctx);
			return;	//ignore any body
		}

		ot = XML_ChildOfTree(tree, "body", 0);
		if (ot)
		{
			unparsable = false;
			if (f)
			{
				if (!strncmp(ot->body, "/me ", 4))
					XMPP_ConversationPrintf(ctx, f, "* ^2%s^7%s\n", f, ot->body+3);
				else
					XMPP_ConversationPrintf(ctx, f, "^2%s^7: %s\n", f, ot->body);
			}
			else
				XMPP_ConversationPrintf(ctx, f, "NOTICE: %s\n", ot->body);

			if (BUILTINISVALID(LocalSound))
				pLocalSound("misc/talk.wav");
		}

		if (unparsable)
		{
			unparsable = false;
			if (jcl->streamdebug)
			{
				XMPP_ConversationPrintf(ctx, f, "Received a message without a body\n");
				XML_ConPrintTree(tree, "", 0);
			}
		}
	}
}

unsigned int JCL_ParseCaps(jclient_t *jcl, char *account, char *resource, xmltree_t *query)
{
	xmltree_t *feature;
	unsigned int caps = 0;
	qboolean rtp = false;
	qboolean rtpaudio = false;
	qboolean rtpvideo = false;
	qboolean quake = false;
	qboolean ice = false;
	qboolean raw = false;
	qboolean jingle = false;
	qboolean si = false;
	qboolean sift = false;
	qboolean ibb = false;
	int i = 0;
	char *var;
//	XML_ConPrintTree(query, 0);
	while((feature = XML_ChildOfTree(query, "feature", i++)))
	{
		var = XML_GetParameter(feature, "var", "");
		//check ones we recognise.
//		Con_Printf("%s/%s: %s\n", account, resource, var);
		if (!strcmp(var, QUAKEMEDIAXMLNS))
			quake = true;
#ifndef VOIP_LEGACY_ONLY
		if (!strcmp(var, "urn:xmpp:jingle:apps:rtp:audio"))
			rtpaudio = true;
		if (!strcmp(var, "urn:xmpp:jingle:apps:rtp:video"))
			rtpvideo = true;
#endif
		if (!strcmp(var, "urn:xmpp:jingle:apps:rtp:1"))
			rtp = true;		//kinda implied, but ensures version is okay
		if (!strcmp(var, "urn:xmpp:jingle:transports:ice-udp:1"))
			ice = true;
		if (!strcmp(var, "urn:xmpp:jingle:transports:raw-udp:1"))
			raw = true;
		if (!strcmp(var, "urn:xmpp:jingle:1"))
			jingle = true;	//kinda implied, but ensures version is okay

#ifdef VOIP_LEGACY
		if (!strcmp(var, "http://www.google.com/xmpp/protocol/voice/v1"))
			caps |= CAP_GOOGLE_VOICE;	//legacy crap, so we can call google's official clients, which do not follow current xmpp standards.
#endif

		if (!strcmp(var, "http://jabber.org/protocol/si"))
			si = true;
		if (!strcmp(var, "http://jabber.org/protocol/si/profile/file-transfer"))
			sift = true;
		if (!strcmp(var, "http://jabber.org/protocol/ibb"))
			ibb = true;
	}

	if ((ice||raw) && jingle)
	{
		if (rtpaudio && rtp)
			caps |= CAP_VOICE;
		if (rtpvideo && rtp)
			caps |= CAP_VIDEO;
		if (quake)
			caps |= CAP_GAMEINVITE;
	}
	if (si && sift && ibb)
		caps |= CAP_SIFT;

	caps &= jcl->enabledcapabilities;

	return caps;
}

void JCL_CheckClientCaps(jclient_t *jcl, buddy_t *buddy, bresource_t *bres);
qboolean JCL_ClientDiscoInfo(jclient_t *jcl, xmltree_t *tree, struct iq_s *iq)
{
	xmltree_t *query = XML_ChildOfTree(tree, "query", 0);
	unsigned int caps = 0;
	buddy_t *b, *ob;
	bresource_t *r, *or;
	if (!JCL_FindBuddy(jcl, iq->to, &b, &r, true))
		return false;

	if (!query)
	{
		caps &= ~(CAP_QUERYING|CAP_QUERIED);
		caps |= CAP_QUERYFAILED;
	}
	else
	{
//		XML_ConPrintTree(tree, 0);
		caps = JCL_ParseCaps(jcl, b->accountdomain, r->resource, query);
	}

	if (b && r)
	{
		if (!(caps & CAP_QUERYFAILED))
		{
//			Con_Printf("%s/%s caps = %x\n", b->accountdomain, r->resource, caps);
			if (!(r->caps & CAP_QUERIED))
				r->caps = CAP_QUERIED;	//reset it
			r->caps |= caps;

			//as we got a valid response, make sure other resources running the same software get the same caps
			for (ob = jcl->buddies; ob; ob = ob->next)
			{
				for (or = ob->resources; or; or = or->next)
				{
					//ignore evil clients.
					if (r == or || (or->caps & (CAP_QUERYFAILED|CAP_QUERIED|CAP_QUERYING)) || !or->client_node)
						continue;

					//all resources with the same details then get the same caps flags.
					if (!strcmp(r->client_node, or->client_node) && !strcmp(r->client_ver, or->client_ver) && !strcmp(r->client_hash, or->client_hash) && !strcmp(r->client_ext, or->client_ext))
					{
//						Con_Printf("%s/%s matches (updated) %s/%s (%s#%s)\n", ob->accountdomain, or->resource, b->accountdomain, r->resource, r->client_node, r->client_ver);
						or->caps = r->caps;
					}
				}
			}
		}
		else
		{
			if (!(r->caps & (CAP_QUERIED|CAP_QUERYFAILED)))
			{
				r->caps = CAP_QUERYFAILED;
				//as the response is invalid, we need to ensure that other resources that claim to use the same software are still queried anyway.
				//(this is needed in case the one that we asked was spoofing)
				for (ob = jcl->buddies; ob; ob = ob->next)
				{
					for (or = ob->resources; or; or = or->next)
					{
						//ignore evil clients.
						if (r == or || (or->caps & (CAP_QUERYFAILED|CAP_QUERIED|CAP_QUERYING)) || !or->client_node)
							continue;

						//all resources with the same details then get the same caps flags.
						if (r->client_node && !strcmp(r->client_node, or->client_node) && !strcmp(r->client_ver, or->client_ver) && !strcmp(r->client_hash, or->client_hash) && !strcmp(r->client_ext, or->client_ext))
						{
							JCL_CheckClientCaps(jcl, ob, or);
							return true;
						}
					}
				}
			}
		}
	}
	return true;
}
void JCL_CheckClientCaps(jclient_t *jcl, buddy_t *buddy, bresource_t *bres)
{
	buddy_t *b;
	bresource_t *r;
	char extname[64], *ext;

	//ignore it if we're already asking them...
	if (bres->caps & (CAP_QUERYING|CAP_QUERIED|CAP_QUERYFAILED))
		return;

	bres->buggycaps = 0;

#ifdef VOIP_LEGACY
	if (!bres->client_node || !bres->client_hash || !*bres->client_hash)
	{
		//one of google's nodes. ONLY google get this fucked up evil hack because they're the only ones that are arrogant enough to not bother to query what that 'ext' actually means - and then to not even bother to tell other clients.
		//every other client is expected to have its act together and not fuck up like this.
		if (bres->client_node && (!!strstr(bres->client_node, "google.com") || !!strstr(bres->client_node, "android.com")))
		{
			for (ext = bres->client_ext; ext; )
			{
				ext = JCL_ParseOut(ext, extname, sizeof(extname));
				if (googlefuckedup)
				{
					//work around repeated bugs in google's various clients.
					if (!strcmp(extname, "voice-v1"))
						bres->buggycaps |= CAP_GOOGLE_VOICE;
				}
			}
		}
	}
#endif

	//look for another resource that we might already know about that is the same client+ver and thus has the same caps.
	if (bres->client_node)
		for (b = jcl->buddies; b; b = b->next)
		{
			for (r = b->resources; r; r = r->next)
			{
				if (r == bres)
					continue;
				//ignore evil clients.
				if ((r->caps & CAP_QUERYFAILED) || !r->client_node)
					continue;

				if (!strcmp(r->client_node, bres->client_node) && !strcmp(r->client_ver, bres->client_ver) && !strcmp(r->client_hash, bres->client_hash) && !strcmp(r->client_ext, bres->client_ext))
				{
					if (r->caps & CAP_QUERIED)
					{
						bres->caps = r->caps;
//						Con_Printf("%s/%s matches (known) %s/%s (%s#%s)\n", buddy->accountdomain, bres->resource, b->accountdomain, r->resource, r->client_node, r->client_ver);
						return;
					}
					if (r->caps & CAP_QUERYING)
					{
						//we're already asking one client with the same software. don't ask for dupes.
						bres->caps = 0;
//						Con_Printf("%s/%s matches (pending) %s/%s (%s#%s)\n", buddy->accountdomain, bres->resource, b->accountdomain, r->resource, r->client_node, r->client_ver);
						return;
					}
				}
			}
		}

//	Con_Printf("%s/%s querying (%s#%s)\n", buddy->accountdomain, bres->resource, bres->client_node, bres->client_ver);
	//okay, this is the first time we've seen that software version. ask it what it supports, and hope we get a valid response...
	if (!bres->client_node || !bres->client_hash || !*bres->client_hash)
	{
		//if we cannot cache the result, don't bother asking.
		//this saves googletalk bugging out on us.
		bres->caps = CAP_QUERIED;
	}
	else
	{
		bres->caps = CAP_QUERYING;

		//ask for info about each extension too. which should only be used if the specified version isn't a hash.
		if (bres->client_hash && !*bres->client_hash)
		{
			for (ext = bres->client_ext; ext; )
			{
				ext = JCL_ParseOut(ext, extname, sizeof(extname));
				if (*extname)
					JCL_SendIQf(jcl, JCL_ClientDiscoInfo, "get", va("%s/%s", buddy->accountdomain, bres->resource), "<query xmlns='http://jabber.org/protocol/disco#info' node='%s#%s'/>", bres->client_node, extname);
			}
		}
		JCL_SendIQf(jcl, JCL_ClientDiscoInfo, "get", va("%s/%s", buddy->accountdomain, bres->resource), "<query xmlns='http://jabber.org/protocol/disco#info' node='%s#%s'/>", bres->client_node, bres->client_ver);
	}
}
void JCL_ParsePresence(jclient_t *jcl, xmltree_t *tree)
{
	buddy_t *buddy;
	bresource_t *bres;

	char *from = XML_GetParameter(tree, "from", jcl->domain);
	xmltree_t *show = XML_ChildOfTree(tree, "show", 0);
	xmltree_t *status = XML_ChildOfTree(tree, "status", 0);
	xmltree_t *quake = XML_ChildOfTree(tree, "quake", 0);
	xmltree_t *muc = XML_ChildOfTreeNS(tree, "http://jabber.org/protocol/muc#user", "x", 0);
	xmltree_t *caps = XML_ChildOfTreeNS(tree, "http://jabber.org/protocol/caps", "c", 0);
	char *type = XML_GetParameter(tree, "type", "");
	char *serverip = NULL;
	char *servermap = NULL;
	char startconvo[512];
	char oldbstatus[128];
	char oldfstatus[128];

	if (quake && !strcmp(quake->xmlns, "fteqw.com:game") && (jcl->enabledcapabilities & CAP_GAMEINVITE))
	{
		serverip = XML_GetParameter(quake, "serverip", NULL);
		servermap = XML_GetParameter(quake, "servermap", NULL);
	}

	jclient_updatebuddylist = true;

	if (type && !strcmp(type, "subscribe"))
	{
		//they want us to let them see us.
		char pauth[512], pdeny[512];
		JCL_GenLink(jcl, startconvo, sizeof(startconvo), NULL, from, NULL, NULL, "%s", from);
		JCL_GenLink(jcl, pauth, sizeof(pauth), "pauth", from, NULL, NULL, "%s", "Authorize");
		JCL_GenLink(jcl, pdeny, sizeof(pdeny), "pdeny", from, NULL, NULL, "%s", "Deny");
		Con_Printf("%s wants to be your friend! %s %s\n", startconvo, pauth, pdeny);
	}
	else if (type && !strcmp(type, "subscribed"))
	{
		//they allowed us to add them.
		JCL_GenLink(jcl, startconvo, sizeof(startconvo), NULL, from, NULL, NULL, "%s", from);
		Con_Printf("%s is now your friend!\n", startconvo);
	}
	else if (type && !strcmp(type, "unsubscribe"))
	{
		//they removed us.
		JCL_GenLink(jcl, startconvo, sizeof(startconvo), NULL, from, NULL, NULL, "%s", from);
		Con_Printf("%s has unfriended you\n", startconvo);
	}
	else if (type && !strcmp(type, "unsubscribed"))
	{
		//we removed them.
		JCL_GenLink(jcl, startconvo, sizeof(startconvo), NULL, from, NULL, NULL, "%s", from);
		Con_Printf("%s is no longer your friend\n", startconvo);
	}
	else
	{
		JCL_FindBuddy(jcl, from, &buddy, &bres, true);
		if (!bres)
		{
			JCL_FindBuddy(jcl, va("%s/", from), &buddy, &bres, true);
		}
		JCL_GenLink(jcl, startconvo, sizeof(startconvo), NULL, from, NULL, NULL, "%s", buddy->name);

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

			Q_strlcpy(oldbstatus, bres->bstatus, sizeof(oldbstatus));
			Q_strlcpy(oldfstatus, bres->fstatus, sizeof(oldfstatus));

			Q_strlcpy(bres->fstatus, (status && *status->body)?status->body:"", sizeof(bres->fstatus));
			if (!tree->child)
			{
				Q_strlcpy(bres->bstatus, "offline", sizeof(bres->bstatus));
				bres->caps = 0;
			}
			else
			{
				xmltree_t *vcu;
				char *photohash;
				buddy_t *me;
				vcu = XML_ChildOfTreeNS(tree, "vcard-temp:x:update", "x", 0);
				photohash = XML_GetChildBody(vcu, "photo", buddy->vcardphotohash);
				if (strcmp(buddy->vcardphotohash, photohash))
				{
					Con_DPrintf("%s changed their photo from \"%s\" to \"%s\"\n", from, buddy->vcardphotohash, photohash);

					Q_strlcpy(buddy->vcardphotohash, photohash, sizeof(buddy->vcardphotohash));
					buddy->vcardphotochanged = true;
				}

				JCL_FindBuddy(jcl, jcl->jid, &me, NULL, true);
				if (buddy == me)
				{
					if (strcmp(buddy->vcardphotohash, jcl->vcardphotohash))
					{	//if one of your other resources changed its image, we need to retrieve it so we can tell other clients about it if the other resource goes away
						jcl->vcardphotohashstatus = VCP_UNKNOWN;
						JCL_SendIQf(jcl, JCL_MyVCardReply, "get", NULL, "<vCard xmlns='vcard-temp'/>");
					}
				}

				Q_strlcpy(bres->bstatus, (show && *show->body)?show->body:"present", sizeof(bres->bstatus));

				if (caps)
				{
					char *ext = XML_GetParameter(caps, "ext", "");	//deprecated
					char *ver = XML_GetParameter(caps, "ver", "");
					char *node = XML_GetParameter(caps, "node", "");
					char *hash = XML_GetParameter(caps, "hash", "");

					if (!bres->client_hash || strcmp(ext, bres->client_ext) || strcmp(hash, bres->client_hash) || strcmp(node, bres->client_node) || strcmp(ver, bres->client_ver))
					{
						bres->caps &= ~(CAP_QUERIED|CAP_QUERYING|CAP_QUERYFAILED);	//no idea what the new caps are. 
						free(bres->client_ext);
						free(bres->client_hash);
						free(bres->client_node);
						free(bres->client_ver);
						bres->client_ext = strdup(ext);
						bres->client_hash = strdup(hash);
						bres->client_node = strdup(node);
						bres->client_ver = strdup(ver);
					}
				}
				JCL_CheckClientCaps(jcl, buddy, bres);
			}

			if (muc)
			{
				JCL_GenLink(jcl, startconvo, sizeof(startconvo), NULL, from, NULL, NULL, "%s", bres->resource);
				if (type && !strcmp(type, "unavailable"))
					XMPP_ConversationPrintf(buddy->name, buddy->name, "%s has left the conversation\n", bres->resource);
				else if (strcmp(oldbstatus, bres->bstatus))
					XMPP_ConversationPrintf(buddy->name, buddy->name, "%s is now %s\n", startconvo, bres->bstatus);
			}
			else
			{
				char *conv = buddy->accountdomain;
				char *title = buddy->name;

				//if we're not currently talking with them, put the status update into the main console instead (which will probably then get dropped).
				if (!BUILTINISVALID(Con_GetConsoleFloat) || pCon_GetConsoleFloat(conv, "iswindow") != true)
					conv = "";

				if (bres->servertype == 2)
				{
					char joinlink[512];
					JCL_GenLink(jcl, joinlink, sizeof(joinlink), "join", from, NULL, NULL, "Playing Quake - %s", bres->server);
					XMPP_ConversationPrintf(conv, title, "%s is now %s\n", startconvo, joinlink);
				}
				else if (bres->servertype == 1)
					XMPP_ConversationPrintf(conv, title, "%s is now ^[[Playing Quake - %s]\\observe\\%s^]\n", startconvo, bres->server, bres->server);
				else if ((pCvar_GetFloat("xmpp_showstatusupdates")||*conv) && (strcmp(oldbstatus, bres->bstatus) || strcmp(oldfstatus, bres->fstatus)))
				{
					if (*bres->fstatus)
						XMPP_ConversationPrintf(conv, title, "%s is now %s: %s\n", startconvo, bres->bstatus, bres->fstatus);
					else
						XMPP_ConversationPrintf(conv, title, "%s is now %s\n", startconvo, bres->bstatus);
				}
			}

			if (type && !strcmp(type, "unavailable"))
			{
				//remove this buddy resource
			}
		}
		else
		{
			Con_Printf("Weird presence:\n");
			XML_ConPrintTree(tree, "", 0);
		}
	}
}

#define JCL_DONE 0			//no more data available for now.
#define JCL_CONTINUE 1		//more data needs parsing.
#define JCL_KILL 2			//some error, needs reconnecting.
#define JCL_NUKEFROMORBIT 3	//permanent error (or logged on from elsewhere)
int JCL_ClientFrame(jclient_t *jcl, char **error)
{
	int pos;
	xmltree_t *tree, *ot;
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
		*error = "Socket Error";
		if (jcl->socket != -1)
			pNet_Close(jcl->socket);
		jcl->socket = -1;
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

		if (jcl->streamdebug == 2)
		{
			char t = jcl->bufferedinmessage[pos];
			jcl->bufferedinmessage[pos] = 0;
			XMPP_ConversationPrintf("xmppin", "xmppin", jcl->bufferedinmessage);
			if (tree)
				XMPP_ConversationPrintf("xmppin", "xmppin", "\n");
			jcl->bufferedinmessage[pos] = t;
		}

		if (!tree)
		{
			*error = "Not an xml stream";
			return JCL_KILL;
		}
		if (strcmp(tree->name, "stream") || strcmp(tree->xmlns, "http://etherx.jabber.org/streams"))
		{
			*error = "Not an xmpp stream";
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
				*error = "End of XML stream";
				return JCL_KILL;
			}
			return JCL_DONE;
		}

		pos = 0;
		tree = XML_Parse(jcl->bufferedinmessage, &pos, jcl->instreampos, false, jcl->defaultnamespace);

		if (jcl->streamdebug == 2 && tree)
		{
			char t = jcl->bufferedinmessage[pos];
			jcl->bufferedinmessage[pos] = 0;
			XMPP_ConversationPrintf("xmppin", "xmppin", jcl->bufferedinmessage);
			XMPP_ConversationPrintf("xmppin", "xmppin", "\n");
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

	if (jcl->streamdebug == 1)
	{
		XMPP_ConversationPrintf("xmppin", "xmppin", "");
		XML_ConPrintTree(tree, "xmppin", 0);
	}

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
		}


		if (unparsable)
		{
			if ((!jcl->issecure) && BUILTINISVALID(Net_SetTLSClient) && XML_ChildOfTree(tree, "starttls", 0) != NULL && jcl->forcetls >= 0)
			{
				Con_DPrintf("XMPP: Attempting to switch to TLS\n");
				JCL_AddClientMessageString(jcl, "<starttls xmlns='urn:ietf:params:xml:ns:xmpp-tls' />");
				unparsable = false;
			}
			else if ((ot=XML_ChildOfTree(tree, "mechanisms", 0)))
			{
				xmltree_t *m;
				int sm;
				char out[512];
				char *method = NULL;
				int outlen = -1;
				qboolean needpass = false;
				if (jcl->forcetls > 0 && !jcl->issecure)
				{
					if (BUILTINISVALID(Net_SetTLSClient))
					{
						*error = "Unable to switch to TLS. You are probably being man-in-the-middle attacked.";
						XML_ConPrintTree(tree, "", 0);
					}
					else
					{
						*error = "Unable to switch to TLS. Your engine does not provide the feature. You can use the xmpp /autoconnect command to register your account instead, as this does not enforce the use of tls (but does still use it in case your future engine versions support it).";
					}
					XML_Destroy(tree);
					return JCL_KILL;
				}
				for (sm = 0; sm < sizeof(saslmethods)/sizeof(saslmethods[0]); sm++)
				{
					method = saslmethods[sm].method?saslmethods[sm].method:jcl->oauth2.saslmethod;
					if (!*method)
						continue;
					for (m = ot->child; m; m = m->sibling)
					{
						if (!strcmp(m->body, method))
						{
							outlen = saslmethods[sm].sasl_initial(jcl, out, sizeof(out));
							if (outlen >= 0)
								break;
							if (outlen == -2)
								needpass = true;
						}
					}
					if (outlen >= 0)
						break;
				}

				if (outlen < 0)
				{
					XML_Destroy(tree);
					//can't authenticate for some reason
					if (needpass)
						XMPP_Menu_Password(jcl);
					*error = "Password needed";
					return JCL_NUKEFROMORBIT;
				}

				if (outlen >= 0)
				{
					jcl->authmode = sm;
					Base64_Add(out, outlen);
					Base64_Finish();

					Con_DPrintf("XMPP: Authing with %s%s.\n", method, jcl->issecure?" over tls":" without encription");
					JCL_AddClientMessagef(jcl, "<auth xmlns='urn:ietf:params:xml:ns:xmpp-sasl' mechanism='%s'"
						    " auth:service='oauth2'"
							" xmlns:auth='http://www.google.com/talk/protocol/auth'"
							">%s</auth>", method, base64);
					unparsable = false;
				}
				else
				{
					*error = "No suitable auth methods";
					XML_ConPrintTree(tree, "", 0);
					XML_Destroy(tree);
					return JCL_KILL;
				}
			}
			else	//we cannot auth, no suitable method.
			{
				*error = "Neither SASL or TLS are usable";
				XML_Destroy(tree);
				return JCL_KILL;
			}
		}
	}
	else if (!strcmp(tree->name, "challenge") && !strcmp(tree->xmlns, "urn:ietf:params:xml:ns:xmpp-sasl") && jcl->authmode >= 0)
	{
		char in[512];
		int inlen;
		char out[512];
		int outlen;
		inlen = Base64_Decode(in, sizeof(in), tree->body, strlen(tree->body));
		outlen = saslmethods[jcl->authmode].sasl_challenge(jcl, in, inlen, out, sizeof(out));
		if (outlen < 0)
		{
			*error = "Unable to auth with server";
			XML_Destroy(tree);
			return JCL_KILL;
		}
		Base64_Add(out, outlen);
		Base64_Finish();
		JCL_AddClientMessagef(jcl, "<response xmlns='urn:ietf:params:xml:ns:xmpp-sasl'>%s</response>", base64);
		unparsable = false;
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
			*error = "proceed without TLS";
			XML_Destroy(tree);
			return JCL_KILL;
		}

		//when using srv records, the certificate must match the user's domain, rather than merely the hostname of the server.
		//if you want to match the hostname of the server, use (oldstyle) tlsconnect directly instead.
		if (pNet_SetTLSClient(jcl->socket, jcl->certificatedomain)<0)
		{
			*error = "failed to switch to TLS";
			XML_Destroy(tree);
			return JCL_KILL;
		}
		if (!*jcl->certificatedomain)
			Con_Printf("XMPP: WARNING: Connecting via TLS without validating certificate\n");
		jcl->issecure = true;

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
			*error = va("Failure: %s\n", tree->child->name);
		else
			*error = "Unknown failure";
		XML_Destroy(tree);
		return JCL_KILL;
	}
	else if (!strcmp(tree->name, "error"))
	{
		xmltree_t *condition;
		condition = XML_ChildOfTree(tree, "success", 0);
		if (!condition)
			condition = XML_ChildOfTree(tree, "see-other-host", 0);
		if (!condition)
			condition = XML_ChildOfTree(tree, "invalid-xml", 0);

		if (condition && !strcmp(condition->name, "see-other-host"))
		{
			//msn needs this, apparently
			Q_strlcpy(jcl->redirserveraddr, condition->body, sizeof(jcl->redirserveraddr));
			JCL_CloseConnection(jcl, "Redirecting", true);
			if (!JCL_Reconnect(jcl))
			{
				*error = "Unable to redirect";
				return JCL_KILL;
			}
			return JCL_CONTINUE;
		}
		else if (condition && !strcmp(condition->name, "success"))
		{
			*error = "error: success";
			unparsable = false;
		}
		else
		{
			ot = XML_ChildOfTree(tree, "text", 0);
			if (ot)
				*error = va("error: %s", ot->body);
			else if (condition)
				*error = va("error: %s", condition->name);
			else
				*error = "Unknown error";

			ot = XML_ChildOfTree(tree, "conflict", 0);
			XML_Destroy(tree);

			if (ot)
				return JCL_NUKEFROMORBIT;
			else
				return JCL_KILL;
		}
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
		XML_ConPrintTree(tree, "", 0);
	}

	memmove(jcl->bufferedinmessage, jcl->bufferedinmessage+pos, jcl->bufferedinammount-pos);
	jcl->bufferedinammount -= pos;
	jcl->instreampos -= pos;

	if (unparsable)
	{
		*error = "Input corrupt, urecognised, or unusable.";
		XML_ConPrintTree(tree, "", 0);
		XML_Destroy(tree);
		return JCL_KILL;
	}
	XML_Destroy(tree);
	return JCL_CONTINUE;
}

void JCL_CloseConnection(jclient_t *jcl, const char *reason, qboolean reconnect)
{
	//send our signoff to the server, if we're still alive.
	if (jcl->status != JCL_DEAD && jcl->status != JCL_INACTIVE)
		Con_Printf("XMPP: Disconnected from %s@%s\n", jcl->username, jcl->domain);

	if (jcl->status == JCL_ACTIVE)
		JCL_AddClientMessageString(jcl, "<presence type='unavailable'/>");
	if (jcl->status != JCL_DEAD && jcl->status != JCL_INACTIVE)
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

	if (jcl->socket != -1)
		pNet_Close(jcl->socket);
	jcl->socket = -1;
	jcl->status = JCL_DEAD;
	Q_strncpyz(jcl->errormsg, reason, sizeof(jcl->errormsg));

	jcl->timeout = jclient_curtime + 30*1000;	//wait 30 secs before reconnecting, to avoid flood-prot-protection issues.

	if (!reconnect)
	{
		int i;
		free(jcl);
		for (i = 0; i < sizeof(jclients)/sizeof(jclients[0]); i++)
		{
			if (jclients[i] == jcl)
				jclients[i] = NULL;
		}
	}

	jclient_updatebuddylist = true;
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

	if (force || jcl->vcardphotohashchanged || strcmp(jcl->curquakeserver, *servermap?servermap:serveraddr))
	{
		char *prior = "<priority>24</priority>";
		char caps[512];
		jcl->vcardphotohashchanged = false;
		Q_strlcpy(jcl->curquakeserver, *servermap?servermap:serveraddr, sizeof(jcl->curquakeserver));
		Con_DPrintf("Sending presence %s\n", jcl->curquakeserver);

		buildcapsvcardpresence(jcl, caps, sizeof(caps));

		if (!*jcl->curquakeserver)
			JCL_AddClientMessagef(jcl,
					"<presence>"
						"%s"
						"%s"
					"</presence>", prior, caps);
		else if (*servermap)	//if we're running a server, say so
			JCL_AddClientMessagef(jcl, 
						"<presence>"
							"%s"
							"<quake xmlns='fteqw.com:game' servermap='%s'/>"
							"%s"
						"</presence>"
						, prior, servermap, caps);
		else	//if we're connected to a server, say so
			JCL_AddClientMessagef(jcl, 
						"<presence>"
							"%s"
							"<quake xmlns='fteqw.com:game' serverip='%s' />"
							"%s"
						"</presence>"
				, prior, jcl->curquakeserver, caps);
	}
}

static void JCL_PrintBuddyStatus(char *console, jclient_t *jcl, buddy_t *b, bresource_t *r)
{
	if (r->servertype == 2)
	{
		char joinlink[512];
		JCL_GenLink(jcl, joinlink, sizeof(joinlink), "join", b->accountdomain, r->resource, NULL, "Playing Quake - %s", r->server);
		Con_SubPrintf(console, "%s", joinlink);
	}
	else if (r->servertype)
		Con_SubPrintf(console, "^[[Playing Quake - %s]\\observe\\%s^]", r->server, r->server);
	else if (*r->fstatus)
		Con_SubPrintf(console, "%s - %s", r->bstatus, r->fstatus);
	else
		Con_SubPrintf(console, "%s", r->bstatus);

	if ((r->caps & CAP_GAMEINVITE) && !r->servertype)
	{
		char invitelink[512];
		JCL_GenLink(jcl, invitelink, sizeof(invitelink), "invite", b->accountdomain, r->resource, NULL, "%s", "Invite");
		Con_SubPrintf(console, " %s", invitelink);
	}
	if (r->caps & CAP_VOICE)
	{
		char calllink[512];
		JCL_GenLink(jcl, calllink, sizeof(calllink), "call", b->accountdomain, r->resource, NULL, "%s", "Call");
		Con_SubPrintf(console, " %s", calllink);
	}
	else if ((r->caps|r->buggycaps) & CAP_GOOGLE_VOICE)
	{
		char calllink[512];
		JCL_GenLink(jcl, calllink, sizeof(calllink), "call", b->accountdomain, r->resource, NULL, "%s", "Call");
		Con_SubPrintf(console, " %s", calllink);
	}
}
static void JCL_RegenerateBuddyList(qboolean force)
{
	const char *console = BUDDYLISTTITLE;
	jclient_t *jcl;
	buddy_t *b, *me;
	bresource_t *r;
	int i, j;
	char convolink[512];

	buddy_t *sortlist[256];
	int buds;

	jclient_updatebuddylist = false;

	if (!BUILTINISVALID(Con_GetConsoleFloat))
		return;

	//only redraw the window if it actually exists. if they closed it, then don't mess things up.
	if (!force && pCon_GetConsoleFloat(console, "iswindow") <= 0)
		return;

	if (pCon_GetConsoleFloat(console, "iswindow") != true)
	{
		pCon_SetConsoleFloat(console, "iswindow", true);
		pCon_SetConsoleFloat(console, "forceutf8", true);
		pCon_SetConsoleFloat(console, "linebuffered", false);
		pCon_SetConsoleFloat(console, "wnd_x", pvid.width - 256);
		pCon_SetConsoleFloat(console, "wnd_y", true);
		pCon_SetConsoleFloat(console, "wnd_w", 256);
		pCon_SetConsoleFloat(console, "wnd_h", pvid.height);
	}
	pCon_SetConsoleFloat(console, "linecount", 0);	//clear it
	if (force)
		pCon_SetActive(console);

	for (i = 0; i < sizeof(jclients)/sizeof(jclients[0]); i++)
	{
		jcl = jclients[i];
		if (!jcl)
			continue;
		if (*jcl->localalias && *jcl->localalias != '>')
			Con_SubPrintf(console, "\n^3%s\n", jcl->localalias);
		else
			Con_SubPrintf(console, "\n^3%s@%s\n", jcl->username, jcl->domain);
		if (jcl->status == JCL_INACTIVE)
			Con_SubPrintf(console, "Not connected.\n", jcl->accountnum);
		else if (jcl->status == JCL_DEAD)
			Con_SubPrintf(console, "%s.\n", *jcl->errormsg?jcl->errormsg:"Connect failed", jcl->accountnum);
		else if (jcl->status == JCL_AUTHING)
			Con_SubPrintf(console, "Connecting... Please wait.\n");
		
		if (jcl->status == JCL_INACTIVE)
			JCL_GenLink(jcl, convolink, sizeof(convolink), "forgetacc", NULL, NULL, NULL, "%s", "Forget Account");
		else if (jcl->status == JCL_DEAD)
			JCL_GenLink(jcl, convolink, sizeof(convolink), "disconnect", NULL, NULL, NULL, "%s", "Disable");
		else
			JCL_GenLink(jcl, convolink, sizeof(convolink), "disconnect", NULL, NULL, NULL, "%s", "Disconnect");
		Con_SubPrintf(console, "%s", convolink);
		if (jcl->status == JCL_INACTIVE)
		{
			JCL_GenLink(jcl, convolink, sizeof(convolink), "connect", NULL, NULL, NULL, "%s", "Connect");
			Con_SubPrintf(console, " %s", convolink);
		}
		else if (jcl->status == JCL_DEAD)
		{
			JCL_GenLink(jcl, convolink, sizeof(convolink), "connect", NULL, NULL, NULL, "%s", "Reconnect");
			Con_SubPrintf(console, " %s", convolink);
		}
		if (jcl->status != JCL_ACTIVE)
			Con_SubPrintf(console, "\n");
		else
		{
			qboolean youarealoner = true;

			JCL_GenLink(jcl, convolink, sizeof(convolink), "addfriend", NULL, NULL, NULL, "%s", "Add Friend");
			Con_SubPrintf(console, " %s\n", convolink);

			JCL_FindBuddy(jcl, jcl->jid, &me, NULL, true);

			for (b = jcl->buddies, buds = 0; b && buds < sizeof(sortlist)/sizeof(sortlist[0]); b = b->next)
			{
				if (b == me)
					continue;
				if (!b->resources)	//can't be online.
					continue;
				if (b->chatroom)
					continue;		//these don't count

				for (j = buds; j > 0; j--)
				{
					if (strcasecmp(sortlist[j-1]->name, b->name) >= 0)
						break;
					sortlist[j] = sortlist[j-1];
				}
				buds++;
				sortlist[j] = b;
			}
			Con_SubPrintf(console, "\n");

			while(buds --> 0)
			{
				bresource_t *gameres = NULL;
				bresource_t *voiceres = NULL;
				bresource_t *chatres = NULL;
				struct c2c_s *c2c;

				b = sortlist[buds];
				for (r = b->resources; r; r = r->next)
				{
					if (!strcmp(r->bstatus, "offline"))
						continue;

					if ((r->caps & CAP_VOICE) && (!voiceres || r->priority > voiceres->priority))
						voiceres = r;
					if ((r->caps & CAP_GAMEINVITE) && (!gameres || r->priority > gameres->priority))
						gameres = r;
					if (!chatres || r->priority > chatres->priority)
						chatres = r;
				}
				if (b->defaultresource)
				{
					r = b->defaultresource;
					if (r->caps & CAP_VOICE)
						voiceres = r;
					if (r->caps & CAP_GAMEINVITE)
						gameres = r;
					chatres = r;
				}
				if (chatres)
				{
					youarealoner = false;

					if (b->vcardphotochanged && b->friended)
					{
						struct buddyinfo_s *bi;
						for (bi = jcl->buddyinfo; bi; bi = bi->next)
						{
							if (!strcmp(bi->accountdomain, b->accountdomain))
								break;
						}
						if (bi && !strcmp(b->vcardphotohash, bi->imagehash))
						{
							char photodata[65536];
							unsigned int photosize = bi->image?Base64_Decode(photodata, sizeof(photodata), bi->image, strlen(bi->image)):0;
							b->image = pDraw_LoadImageData(va("xmpp/%s.png", b->accountdomain), bi->imagemime, photodata, photosize);
							b->vcardphotochanged = false;
						}
						else if (!*b->vcardphotohash)
						{	//this buddy has no photo, so don't bother querying it
							b->vcardphotochanged = false;
							b->image = 0;
						}
						else if (jcl->avatarupdate == NULL)
						{
							b->vcardphotochanged = false;
							Con_DPrintf("Querying %s's photo\n", b->accountdomain);
							jcl->avatarupdate = JCL_SendIQf(jcl, JCL_BuddyVCardReply, "get", b->accountdomain, "<vCard xmlns='vcard-temp'/>");
						}
					}

					Q_snprintf(convolink, sizeof(convolink), "^[%s\\xmppacc\\%i\\xmpp\\%s^]", b->name, jcl->accountnum, b->accountdomain);

					if (!b->image)
						Con_SubPrintf(console, "^[\\img\\gfx/menudot1.lmp\\w\\32\\h\\32^]");
					else
						Con_SubPrintf(console, "^[\\img\\xmpp/%s.png\\w\\32\\h\\32^]", b->accountdomain);
					Con_SubPrintf(console, "%s", convolink);

					if (*chatres->fstatus)
						Con_SubPrintf(console, "\v  %s", chatres->fstatus);

					JCL_GenLink(jcl, convolink, sizeof(convolink), "buddyopts", b->accountdomain, NULL, NULL, "^h%s^h", "Options");
					Con_SubPrintf(console, "\v  %s    %s", chatres->bstatus, convolink);

					for (c2c = jcl->c2c; c2c; c2c = c2c->next)
					{
						buddy_t *peer = NULL;
						qboolean voice = false, video = false, server = false;
						int c;
						JCL_FindBuddy(jcl, c2c->with, &peer, NULL, true);
						if (peer == b)
						{
							for (c = 0; c < c2c->contents; c++)
							{
								switch(c2c->content[c].mediatype)
								{
								case ICEP_INVALID:					break;
								case ICEP_VOICE:	voice = true; 	break;
								case ICEP_VIDEO:	video = true; 	break;
								case ICEP_QWSERVER: server = true; 	break;
								case ICEP_QWCLIENT: /*client = true;*/ 	break;
								}
							}
							if (server)
							{
								JCL_GenLink(jcl, convolink, sizeof(convolink), "jdeny", c2c->with, NULL, c2c->sid, "%s", "Kick");
								gameres = NULL;
							}
							else if (video || voice)
							{
								JCL_GenLink(jcl, convolink, sizeof(convolink), "jdeny", c2c->with, NULL, c2c->sid, "%s", "Hang Up");
								voiceres = NULL;
							}
							else /*if (client)*/
							{
								JCL_GenLink(jcl, convolink, sizeof(convolink), "jdeny", c2c->with, NULL, c2c->sid, "%s", "Disconnect");
								gameres = NULL;
							}
							Con_SubPrintf(console, " %s", convolink);
						}
					}
					if (gameres)
					{
						if (*gameres->server)
							JCL_GenLink(jcl, convolink, sizeof(convolink), "join", b->accountdomain, gameres->resource, NULL, "Playing Quake - %s", gameres->server);
						else
							JCL_GenLink(jcl, convolink, sizeof(convolink), "invite", b->accountdomain, gameres->resource, NULL, "%s", "Invite");
						Con_SubPrintf(console, " %s", convolink);
					}
					if (voiceres)
					{
						JCL_GenLink(jcl, convolink, sizeof(convolink), "call", b->accountdomain, voiceres->resource, NULL, "%s", "Call");
						Con_SubPrintf(console, " %s", convolink);
					}
					Con_SubPrintf(console, "\n");
				}
			}
			if (youarealoner)
				Con_SubPrintf(console, "    You have no friends\n");
		}
	}

	for (i = 0; i < sizeof(jclients)/sizeof(jclients[0]); i++)
		if (!jclients[i])
		{
			JCL_GenLink(NULL, convolink, sizeof(convolink), "newaccount", NULL, NULL, NULL, "%s", "Open New Account");
			Con_SubPrintf(console, "\n%s\n", convolink);
			break;
		}
}

static void JCL_PrintBuddyList(char *console, jclient_t *jcl, qboolean all)
{
	buddy_t *b;
	bresource_t *r;
	struct c2c_s *c2c;
	struct ft_s *ft;
	char convolink[512], actlink[512];
	int c;

	if (!jcl->buddies)
		Con_SubPrintf(console, "You have no friends\n");
	for (b = jcl->buddies; b; b = b->next)
	{	
		//if we don't actually know them, don't list them.
		if (!b->friended && !b->chatroom)
			continue;

		if (!b->resources)	//offline
		{
			if (all)
			{
				JCL_GenLink(jcl, convolink, sizeof(convolink), NULL, b->accountdomain, NULL, NULL, "%s", b->name);
				Con_SubPrintf(console, "%s: offline\n", convolink);
			}
		}
		else if (b->resources->next)
		{	//multiple potential resources
			JCL_GenLink(jcl, convolink, sizeof(convolink), NULL, b->accountdomain, NULL, NULL, "%s", b->name);
			Con_SubPrintf(console, "%s:\n", convolink);
			for (r = b->resources; r; r = r->next)
			{
				JCL_GenLink(jcl, convolink, sizeof(convolink), NULL, b->accountdomain, r->resource, NULL, "%s", r->resource);
				Con_SubPrintf(console, "    %s: ", convolink);
				JCL_PrintBuddyStatus(console, jcl, b, r);
				Con_SubPrintf(console, "\n");
			}
		}
		else	//only one resource
		{
			r = b->resources;
			JCL_GenLink(jcl, convolink, sizeof(convolink), NULL, b->accountdomain, r->resource, NULL, "%s", b->name);
			Con_SubPrintf(console, "%s: ", convolink);
			JCL_PrintBuddyStatus(console, jcl, b, r);
			Con_SubPrintf(console, "\n");
		}
	}

#ifdef JINGLE
	if (jcl->c2c)
		Con_SubPrintf(console, "Active sessions:\n");
	for (c2c = jcl->c2c; c2c; c2c = c2c->next)
	{
		qboolean voice = false, video = false, server = false, client = false;
		JCL_FindBuddy(jcl, c2c->with, &b, &r, true);
		for (c = 0; c < c2c->contents; c++)
		{
			switch(c2c->content[c].mediatype)
			{
			case ICEP_INVALID:					break;
			case ICEP_VOICE:	voice = true; 	break;
			case ICEP_VIDEO:	video = true; 	break;
			case ICEP_QWSERVER: server = true; 	break;
			case ICEP_QWCLIENT: client = true; 	break;
			}
		}
		JCL_GenLink(jcl, convolink, sizeof(convolink), NULL, b->accountdomain, r->resource, NULL, "%s", b->name);
		Con_SubPrintf(console, "    %s: ", convolink);
		if (video)
			Con_SubPrintf(console, "video ");
		if (voice)
			Con_SubPrintf(console, "voice ");
		if (server)
			Con_SubPrintf(console, "server ");
		if (client)
			Con_SubPrintf(console, "client ");
		if (server)
			JCL_GenLink(jcl, actlink, sizeof(actlink), "jdeny", c2c->with, NULL, c2c->sid, "%s", "Kick");
		else if (video || voice)
			JCL_GenLink(jcl, actlink, sizeof(actlink), "jdeny", c2c->with, NULL, c2c->sid, "%s", "Hang Up");
		else
			JCL_GenLink(jcl, actlink, sizeof(actlink), "jdeny", c2c->with, NULL, c2c->sid, "%s", "Disconnect");
		Con_SubPrintf(console, "%s\n", actlink);
	}
#endif

#ifdef FILETRANSFERS
	if (jcl->ft)
		Con_SubPrintf(console, "Active file transfers:\n");
	for (ft = jcl->ft; ft; ft = ft->next)
	{
		JCL_FindBuddy(jcl, ft->with, &b, &r, true);
		JCL_GenLink(jcl, convolink, sizeof(convolink), NULL, b->accountdomain, r->resource, NULL, "%s", b->name);
		JCL_GenLink(jcl, actlink, sizeof(actlink), "fdeny", ft->with, NULL, ft->sid, "%s", "Cancel");
		Con_SubPrintf(console, "    %s: %s\n", convolink, ft->fname);
	}
#endif
}

//functions above this line allow connections to multiple servers.
//it is just the control functions that only allow one server.

qintptr_t JCL_Frame(qintptr_t *args)
{
	int i;
	jclient_curtime = args[0];
	if (jclient_needreadconfig)
	{
		JCL_LoadConfig();
		JCL_RegenerateBuddyList(false);
	}
	if (jclient_updatebuddylist)
		JCL_RegenerateBuddyList(false);

	for (i = 0; i < sizeof(jclients)/sizeof(jclients[0]); i++)
	{
		jclient_t *jcl = jclients[i];
		if (jcl && jcl->status != JCL_INACTIVE)
		{
			int stat = JCL_CONTINUE;
			if (jcl->status == JCL_DEAD)
			{
				if (jclient_curtime > jcl->timeout)
				{
					JCL_Reconnect(jcl);
					jcl->timeout = jclient_curtime + 60*1000;
					jclient_updatebuddylist = true;
				}
			}
			else
			{
				char *error = "";
				if (jcl->connected)
					JCL_GeneratePresence(jcl, false);
				while(stat == JCL_CONTINUE)
					stat = JCL_ClientFrame(jcl, &error);
				if (stat == JCL_NUKEFROMORBIT)
				{
					JCL_CloseConnection(jcl, error, true);
					jcl->status = JCL_INACTIVE;
				}
				else if (stat == JCL_KILL)
					JCL_CloseConnection(jcl, error, true);
				else
					JCL_FlushOutgoing(jcl);

				if (stat == JCL_DONE)
				{
					XMPP_FT_Frame(jcl);

					if (jclient_curtime > jcl->timeout)
					{
						//client needs to send something valid as a keep-alive so routers don't silently forget mappings.
						JCL_SendIQf(jcl, NULL, "get", NULL, "<ping xmlns='urn:xmpp:ping'/>");
						jcl->timeout = jclient_curtime + 60*1000;
					}
				}
			}

#ifdef JINGLE
			JCL_JingleTimeouts(jcl, false);
#endif
			JCL_IQTimeouts(jcl);
		}
	}
	return 0;
}

void JCL_WriteConfig(void)
{
	xmltree_t *m, *n, *oauth2, *features;
	int i, j;
	qhandle_t config;
	struct buddyinfo_s *bi;

	//don't write the config if we're meant to be reading it. avoid wiping it if we're killed fast.
	if (jclient_needreadconfig)
		return;


	m = XML_CreateNode(NULL, "xmppaccounts", "", "");
	for (i = 0; i < sizeof(jclients) / sizeof(jclients[0]);  i++)
	{
		jclient_t *jcl = jclients[i];
		if (jcl)
		{
			char foo[64];
			n = XML_CreateNode(m, "account", "", "");
			XML_AddParameteri(n, "id", i);

			Q_snprintf(foo, sizeof(foo), "%i", jcl->streamdebug);
			XML_CreateNode(n, "streamdebug", "", foo);
			Q_snprintf(foo, sizeof(foo), "%i", jcl->forcetls);
			XML_CreateNode(n, "forcetls", "", foo);
			XML_CreateNode(n, "savepassword", "", jcl->savepassword?"1":"0");
			XML_CreateNode(n, "allowauth_plain_nontls", "", jcl->allowauth_plainnontls?"1":"0");
			XML_CreateNode(n, "allowauth_plain_tls", "", jcl->allowauth_plaintls?"1":"0");
			XML_CreateNode(n, "allowauth_digest_md5", "", jcl->allowauth_digestmd5?"1":"0");
			XML_CreateNode(n, "allowauth_scram_sha_1", "", jcl->allowauth_scramsha1?"1":"0");

			if (*jcl->oauth2.saslmethod)
			{
				XML_CreateNode(n, "allowauth_oauth2", "", jcl->allowauth_oauth2?"1":"0");
				oauth2 = XML_CreateNode(n, "oauth2", "", "");
				XML_AddParameter(oauth2, "method", jcl->oauth2.saslmethod);
				XML_CreateNode(oauth2, "obtain-url", "", jcl->oauth2.obtainurl);
				XML_CreateNode(oauth2, "refresh-url", "", jcl->oauth2.refreshurl);
				XML_CreateNode(oauth2, "client-id", "", jcl->oauth2.clientid);
				XML_CreateNode(oauth2, "client-secret", "", jcl->oauth2.clientsecret);
				XML_CreateNode(oauth2, "scope", "", jcl->oauth2.scope);
				XML_CreateNode(oauth2, "auth-token", "", jcl->oauth2.authtoken);
				XML_CreateNode(oauth2, "refresh-token", "", jcl->oauth2.refreshtoken);
				XML_CreateNode(oauth2, "access-token", "", jcl->oauth2.accesstoken);
			}

			XML_CreateNode(n, "username", "", jcl->username);
			XML_CreateNode(n, "domain", "", jcl->domain);
			XML_CreateNode(n, "resource", "", jcl->resource);
			if (!*jcl->oauth2.saslmethod || !jcl->allowauth_oauth2 || *jcl->password)	//avoid writing password lest we encourage someone to supply it when its not useful
				XML_CreateNode(n, "password", "", jcl->password);	//FIXME: should we base64 this just to obscure it?
			XML_CreateNode(n, "serveraddr", "", jcl->serveraddr);
			Q_snprintf(foo, sizeof(foo), "%i", jcl->serverport);
			XML_CreateNode(n, "serverport", "", foo);
			XML_CreateNode(n, "certificatedomain", "", jcl->certificatedomain);
			XML_CreateNode(n, "inactive", "", jcl->status == JCL_INACTIVE?"1":"0");

			features = XML_CreateNode(n, "features", "", "");
			XML_AddParameter(features, "ver", JCL_BUILD);
			for (j = 0; capnames[j].names; j++)
			{
				XML_CreateNode(features, capnames[j].names, "", (jcl->enabledcapabilities & capnames[j].cap)?"1":"0");
			}

			features = XML_CreateNode(n, "buddyinfo", "", "");
			for (bi = jcl->buddyinfo; bi; bi = bi->next)
			{
				xmltree_t *b = XML_CreateNode(features, "buddy", "", "");
				XML_AddParameter(b, "name", bi->accountdomain);

				if (bi->image)
					XML_CreateNode(b, "image", "", bi->image);
				if (bi->imagemime)
					XML_CreateNode(b, "imagemime", "", bi->imagemime);
				if (bi->imagehash)
					XML_CreateNode(b, "imagehash", "", bi->imagehash);
			}

			//FIXME: client disco info
		}
	}

	pFS_Open("**plugconfig", &config, 2);
	if (config >= 0)
	{
		char *s = XML_GenerateString(m, true);
		pFS_Write(config, s, strlen(s));
		free(s);

		pFS_Close(config);
	}
	XML_Destroy(m);
}
void JCL_LoadConfig(void)
{
	jclient_needreadconfig = false;
	if (!jclients[0])
	{
		int len;
		qhandle_t config;
		char *buf;
		qboolean oldtls;
		len = pFS_Open("**plugconfig", &config, 1);
		if (len >= 0)
		{
			buf = malloc(len+1);
			buf[len] = 0;
			pFS_Read(config, buf, len);
			pFS_Close(config);

			if (len && *buf != '<')
			{//legacy code, to be removed
				char *line = buf;
				char tls[256];
				char server[256];
				char account[256];
				char password[256];
				line = JCL_ParseOut(line, tls, sizeof(tls));
				line = JCL_ParseOut(line, server, sizeof(server));
				line = JCL_ParseOut(line, account, sizeof(account));
				line = JCL_ParseOut(line, password, sizeof(password));

				oldtls = atoi(tls);

				Con_Printf("Legacy config: %s (%i)\n", buf, len);
				jclients[0] = JCL_Connect(0, server, oldtls, account, password);
			}
			else
			{
				xmltree_t *accs;
				int start = 0;
				accs = XML_Parse(buf, &start, len, false, "");
				if (accs)
				{
					int i;
					xmltree_t *acc;
					for (i = 0; (acc = XML_ChildOfTree(accs, "account", i)); i++)
					{
						int id = atoi(XML_GetParameter(acc, "id", "0"));
						if (id < 0 || id >= sizeof(jclients) / sizeof(jclients[0]) || jclients[id])
							continue;

						jclients[id] = JCL_ConnectXML(acc);
					}
					XML_Destroy(accs);
				}
			}
			free(buf);
		}
	}
}

//on shutdown, write config and close connections.
qintptr_t JCL_Shutdown(qintptr_t *args)
{
	jclient_t *jcl;
	int i;
	JCL_WriteConfig();
	for (i = 0; i < sizeof(jclients)/sizeof(jclients[0]); i++)
	{
		jcl = jclients[i];
		if (jcl)
			JCL_CloseConnection(jcl, "", false);
	}

//	if (_CrtDumpMemoryLeaks())
//		OutputDebugStringA("Leaks detected\n");
	return true;
}

void JCL_SendMessage(jclient_t *jcl, char *to, char *msg)
{
	char markup[1024];
	char *con, *title;
	buddy_t *b;
	bresource_t *br;
	JCL_FindBuddy(jcl, to, &b, &br, true);
	if (!b)
	{
		Con_Printf("Can't find buddy \"%s\"\n", to);
		return;
	}
	if (b->chatroom)
	{
		if (br)
		{
			JCL_AddClientMessagef(jcl, "<message to='%s/%s' type='chat'><body>", b->accountdomain, br->resource);
			con = to;
		}
		else
		{
			JCL_AddClientMessagef(jcl, "<message to='%s' type='groupchat'><body>", b->accountdomain);
			con = b->name;
		}
		title = con;
	}
	else
	{
		title = b->name;
		con = b->accountdomain;
		if (!br)
			br = b->defaultresource;
		if (br)
			JCL_AddClientMessagef(jcl, "<message to='%s/%s'><body>", b->accountdomain, br->resource);
		else
			JCL_AddClientMessagef(jcl, "<message to='%s'><body>", b->accountdomain);
	}
	JCL_AddClientMessage(jcl, markup, XML_Markup(msg, markup, sizeof(markup)) - markup);
	JCL_AddClientMessageString(jcl, "</body></message>");
	if (b->chatroom && !br)
		return;
	if (!strncmp(msg, "/me ", 4))
		XMPP_ConversationPrintf(con, title, "* ^5%s^7"COLOURYELLOW"%s\n", ((!strcmp(jcl->localalias, ">>"))?"me":jcl->localalias), msg+3);
	else
		XMPP_ConversationPrintf(con, title, "^5%s^7: "COLOURYELLOW"%s\n", jcl->localalias, msg);
}
void JCL_AttentionMessage(jclient_t *jcl, char *to, char *msg)
{
	char fullto[256];
	buddy_t *b = NULL;
	bresource_t *br = NULL;
	xmltree_t *m;
	char *s;

	JCL_FindBuddy(jcl, to, &b, &br, true);
	if (!b)
		return;
	if (!br)
		br = b->defaultresource;
	if (!br)
		br = b->resources;
	if (!br)
	{
		Con_SubPrintf(b->accountdomain, "User is not online\n");
		return;
	}
	Q_snprintf(fullto, sizeof(fullto), "%s/%s", b->accountdomain, br->resource);

	m = XML_CreateNode(NULL, "message", "", "");
	XML_AddParameter(m, "to", fullto);
//	XML_AddParameter(m, "type", "headline");

	XML_CreateNode(m, "attention", "urn:xmpp:attention:0", "");
	if (msg)
		XML_CreateNode(m, "body", "", msg);

	s = XML_GenerateString(m, false);
	JCL_AddClientMessageString(jcl, s);
	free(s);
	XML_Destroy(m);

	if (msg)
	{
		if (!strncmp(msg, "/me ", 4))
			Con_SubPrintf(b->accountdomain, "*^5%s^7"COLOURYELLOW"%s\n", ((!strcmp(jcl->localalias, ">>"))?"me":jcl->localalias), msg+3);
		else
			Con_SubPrintf(b->accountdomain, "^5%s^7: "COLOURYELLOW"%s\n", jcl->localalias, msg);
	}
}

//server may be null, in which case its expected to be folded into room.
void JCL_JoinMUCChat(jclient_t *jcl, char *room, char *server, char *myhandle, char *password)
{
	char caps[512];
	char roomserverhandle[512];
	buddy_t *b;
	bresource_t *r;
	if (!myhandle)
		myhandle = jcl->username;
	if (server)
		Q_snprintf(roomserverhandle, sizeof(roomserverhandle), "%s@%s/%s", room, server, myhandle);
	else
		Q_snprintf(roomserverhandle, sizeof(roomserverhandle), "%s/%s", room, myhandle);
	if (!JCL_FindBuddy(jcl, roomserverhandle, &b, &r, true))
		return;
	b->chatroom = true;
	buildcapsvcardpresence(jcl, caps, sizeof(caps));
	JCL_AddClientMessagef(jcl, "<presence to='%s'><x xmlns='http://jabber.org/protocol/muc'><password>%s</password></x>%s</presence>", roomserverhandle, password, caps);
}

void XMPP_Menu_Password(jclient_t *acc)
{
//	int y;

	if (!jclient_action)
	{
		JCL_RegenerateBuddyList(true);
		pCon_SetConsoleFloat(BUDDYLISTTITLE, "linebuffered", true);
		jclient_action_cl = acc;
		jclient_action_buddy = NULL;
		jclient_action = ACT_PASSWORD;
	}

	/*
	pCmd_AddText("conmenu\n"
				"{\n"
					"menuclear\n"
					"if (option == \"SignIn\")\n"
					"{\n"
					COMMANDPREFIX" /set savepassword $_t1\n"
					COMMANDPREFIX" /password ${0}\n"
					"}\n"
				"}\n", false);

	y = 36;
	pCmd_AddText(va("menutext 48 %i \"^sXMPP Sign In\"\n", y), false); y+=16;
	pCmd_AddText(va("menutext 48 %i \"^sPlease provide your password for\"\n", y), false); y+=16;
	pCmd_AddText(va("menueditpriv 48 %i \"%s@%s\" \"example\"\n", y, acc->username, acc->domain), false);y+=16;
	pCmd_AddText(va("set _t1 0\nmenucheck 48 %i \"Save Password\" _t1 1\n", y), false); y+=16;
	pCmd_AddText(va("menutext 48 %i \"Sign In\" SignIn\n", y), false);
	pCmd_AddText(va("menutext 256 %i \"Cancel\" cancel\n", y), false);
	*/
}
void XMPP_Menu_Connect(void)
{
	int y;
	pCmd_AddText("conmenu\n"
				"{\n"
					"menuclear\n"
					"if (option == \"SignIn\")\n"
					"{\n"COMMANDPREFIX" /connect ${0}@${1}/${2}\n}\n"
				"}\n", false);

	y = 36;
	pCmd_AddText(va("menutext 48 %i \"^sXMPP Sign In\"\n", y), false); y+=16;
	pCmd_AddText(va("menueditpriv 48 %i \"Username\" \"example\"\n", y), false);y+=16;
	pCmd_AddText(va("menueditpriv 48 %i \"Domain\" \""EXAMPLEDOMAIN"\"\n", y), false);y+=16;
	pCmd_AddText(va("menueditpriv 48 %i \"Resource\" \"\"\n", y), false);y+=32;
	pCmd_AddText(va("menutext 48 %i \"Sign In\" SignIn\n", y), false);
	pCmd_AddText(va("menutext 256 %i \"Cancel\" cancel\n", y), false);
}

void JCL_Command(int accid, char *console)
{
	char imsg[8192];
	char arg[6][1024];
	char *msg;
	int i;
	char nname[256];
	jclient_t *jcl;

	if (accid < 0 || accid >= sizeof(jclients)/sizeof(jclients[0]))
		return;
	jcl = jclients[accid];

	pCmd_Args(imsg, sizeof(imsg));

	msg = imsg;
	for (i = 0; i < 6; i++)
	{
		if (!msg)
			*arg[i] = 0;
		else
			msg = JCL_ParseOut(msg, arg[i], sizeof(arg[i]));
	}

	if (arg[0][0] == '/' && arg[0][1] != '/' && strcmp(arg[0]+1, "me"))
	{
		if (!strcmp(arg[0]+1, "open") || !strcmp(arg[0]+1, "connect") || !strcmp(arg[0]+1, "autoopen") || !strcmp(arg[0]+1, "autoconnect") || !strcmp(arg[0]+1, "plainopen") || !strcmp(arg[0]+1, "plainconnect") || !strcmp(arg[0]+1, "tlsopen") || !strcmp(arg[0]+1, "tlsconnect"))
		{	//tlsconnect is 'old'.
			int tls;
			if (!*arg[1])
			{
				XMPP_Menu_Connect();
				Con_SubPrintf(console, "%s <account@domain/resource> <password> <server>\n", arg[0]+1);
				return;
			}

			if (jcl)
			{
				Con_TrySubPrint(console, "You are already connected\nPlease /quit first\n");
				return;
			}
			if (!strncmp(arg[0]+1, "tls", 3))
				tls = 2;	//old initial-tls connect
			else if (!strncmp(arg[0]+1, "plain", 5))
				tls = -1;	//don't bother with tls. at all.
			else if (!strncmp(arg[0]+1, "auto", 4))
				tls = 0;	//use tls if its available, but don't really care otherwise.
			else
				tls = 1;	//require tls
			jcl = jclients[accid] = JCL_Connect(accid, arg[3], tls, arg[1], arg[2]);
			if (!jclients[accid])
			{
				Con_TrySubPrint(console, "Connect failed\n");
				return;
			}
		}
		else if (!strcmp(arg[0]+1, "help")) 
		{
			Con_TrySubPrint(console, "^[/" COMMANDPREFIX " /connect USERNAME@DOMAIN/RESOURCE [PASSWORD] [XMPPSERVER]^]\n");
			if (BUILTINISVALID(Net_SetTLSClient))
			{
				Con_TrySubPrint(console, "eg for gmail: ^[/" COMMANDPREFIX " /connect myusername@gmail.com^] (using oauth2)\n");
				Con_TrySubPrint(console, "eg for gmail: ^[/" COMMANDPREFIX " /connect myusername@gmail.com mypassword^] (warning: password will be saved locally in plain text)\n");
//				Con_TrySubPrint(console, "eg for facebook: ^[/" COMMANDPREFIX " /connect myusername@chat.facebook.com mypassword chat.facebook.com^]\n");
//				Con_TrySubPrint(console, "eg for msn: ^[/" COMMANDPREFIX " /connect myusername@messanger.live.com mypassword^]\n");
			}
			else
			{	//if we don't have tls support, we can still connect to google with oauth2
				//no idea about other networks.
				//however, the regular 'connect' command will insist on tls, so make sure the helpful command displayed is different
				Con_TrySubPrint(console, "eg for gmail: ^[/" COMMANDPREFIX " /autoconnect myusername@gmail.com^] (using oauth2)\n");
			}
			Con_TrySubPrint(console, "Note that this info will be used the next time you start quake.\n");

			//small note:
			//for the account 'me@example.com' the server to connect to can be displayed with:
			//nslookup -querytype=SRV _xmpp-client._tcp.example.com
			//srv resolving seems to be non-standard on each system, I don't like having to special case things.
			Con_TrySubPrint(console, 	"^[/" COMMANDPREFIX " /help^]\n"
										"This text...\n");
			Con_TrySubPrint(console, 	"^[/" COMMANDPREFIX " /raw <XML STANZAS/>^]\n"
										"For debug hackery.\n");
			Con_TrySubPrint(console, 	"^[/" COMMANDPREFIX " /friend accountname@domain friendlyname^]\n"
										"Befriends accountname, and shows them in your various lists using the friendly name. Can also be used to rename friends.\n");
			Con_TrySubPrint(console,	"^[/" COMMANDPREFIX " /unfriend accountname@domain^]\n"
										"Ostracise your new best enemy. You will no longer see them and they won't be able to contact you.\n");
			Con_TrySubPrint(console, 	"^[/" COMMANDPREFIX " /blist^]\n"
										"Show all your friends! Names are clickable and will begin conversations.\n");
			Con_TrySubPrint(console, 	"^[/" COMMANDPREFIX " /quit^]\n"
										"Disconnect from the XMPP server, noone will be able to hear you scream.\n");
			Con_TrySubPrint(console, 	"^[/" COMMANDPREFIX " /join accountname@domain^]\n"
										"Joins your friends game (they will be prompted).\n");
			Con_TrySubPrint(console, 	"^[/" COMMANDPREFIX " /invite accountname@domain^]\n"
										"Invite someone to join your game (they will be prompted).\n");
			Con_TrySubPrint(console, 	"^[/" COMMANDPREFIX " /voice accountname@domain^]\n"
										"Begin a bi-directional peer-to-peer voice conversation with someone (they will be prompted).\n");
			Con_TrySubPrint(console, 	"^[/" COMMANDPREFIX " /msg ACCOUNTNAME@domain \"your message goes here\"^]\n"
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
				if (BUILTINISVALID(Con_SetActive))
					pCon_SetActive(console);
			}
			else
				pCmd_AddText("\nclear\n", true);
		}
		else if (!jcl)
		{
			Con_SubPrintf(console, "No account specified. Cannot %s\n", arg[0]);
		}
		else if (!strcmp(arg[0]+1, "oa2token"))
		{
			free(jcl->oauth2.authtoken);
			jcl->oauth2.authtoken = strdup(arg[1]);
			if (jcl->status == JCL_INACTIVE)
				jcl->status = JCL_DEAD;
		}
		else if (!strcmp(arg[0]+1, "set"))
		{
			if (!strcmp(arg[1], "savepassword"))
				jcl->savepassword = atoi(arg[2]);
			else if (!strcmp(arg[1], "avatars"))
				jcl->enabledcapabilities = (jcl->enabledcapabilities & ~CAP_AVATARS) | (atoi(arg[2])?CAP_AVATARS:0);
			else if (!strcmp(arg[1], "debug"))
				jcl->streamdebug = atoi(arg[2]);
			else if (!strcmp(arg[1], "resource"))
				Q_strlcpy(jcl->resource, arg[2], sizeof(jcl->resource));
			else
				Con_SubPrintf(console, "Sorry, setting not recognised.\n", arg[0]);
		}
		else if (!strcmp(arg[0]+1, "password"))
		{
			Q_strncpyz(jcl->password, arg[1], sizeof(jcl->password));
			if (jcl->status == JCL_INACTIVE)
				jcl->status = JCL_DEAD;
		}
		else if (!strcmp(arg[0]+1, "quit"))
		{
			//disconnect from the xmpp server.
			JCL_CloseConnection(jcl, "/quit", false);
		}
		else if (jcl->status != JCL_ACTIVE)
		{
			Con_SubPrintf(console, "You are not authed. Please wait.\n", arg[0]);
		}
		else if (!strcmp(arg[0]+1, "blist"))
		{
			//print out a full list of everyone, even those offline.
			JCL_PrintBuddyList(console, jcl, true);
		}
		else if (!strcmp(arg[0]+1, "msg"))
		{
			//FIXME: validate the dest. deal with xml markup in dest.
			Q_strlcpy(jcl->defaultdest, arg[1], sizeof(jcl->defaultdest));
			msg = arg[2];

			//reparse the commands, so we get all trailing text
			msg = imsg;
			msg = JCL_ParseOut(msg, arg[0], sizeof(arg[0]));
			msg = JCL_ParseOut(msg, arg[1], sizeof(arg[1]));
			while(*msg == ' ')
				msg++;

			JCL_SendMessage(jcl, jcl->defaultdest, msg);
		}
		else if (!strcmp(arg[0]+1, "friend")) 
		{
			XMPP_AddFriend(jcl, arg[1], arg[2]);
		}
		else if (!strcmp(arg[0]+1, "unfriend")) 
		{
			//FIXME: validate the name. deal with xml markup.

			//hide from em
			JCL_AddClientMessagef(jcl, "<presence to='%s' type='unsubscribed'/>", arg[1]);

			//stop looking for em
			JCL_AddClientMessagef(jcl, "<presence to='%s' type='unsubscribe'/>", arg[1]);

			//stop listing em
			JCL_SendIQf(jcl, NULL, "set", NULL, "<query xmlns='jabber:iq:roster'><item jid='%s' subscription='remove' /></query>", arg[1]);
		}
#ifdef JINGLE
		else if (!strcmp(arg[0]+1, "join")) 
		{
			JCL_ToJID(jcl, *arg[1]?arg[1]:console, nname, sizeof(nname), true);
			JCL_Join(jcl, nname, NULL, true, ICEP_QWCLIENT);
		}
		else if (!strcmp(arg[0]+1, "invite")) 
		{
			JCL_ToJID(jcl, *arg[1]?arg[1]:console, nname, sizeof(nname), true);
			JCL_Join(jcl, nname, NULL, true, ICEP_QWSERVER);
		}
		else if (!strcmp(arg[0]+1, "voice") || !strcmp(arg[0]+1, "call")) 
		{
			JCL_ToJID(jcl, *arg[1]?arg[1]:console, nname, sizeof(nname), true);
			JCL_Join(jcl, nname, NULL, true, ICEP_VOICE);
		}
		else if (!strcmp(arg[0]+1, "kick")) 
		{
			JCL_ToJID(jcl, *arg[1]?arg[1]:console, nname, sizeof(nname), true);
			JCL_Join(jcl, nname, NULL, false, ICEP_INVALID);
		}
#endif
#ifdef FILETRANSFERS
		else if (!strcmp(arg[0]+1, "sendfile"))
		{
			char *fname = arg[1];
		
			if (!(jcl->enabledcapabilities & CAP_SIFT))
			{
				Con_SubPrintf(console, "XMPP: file transfers are not enabled for this account. Edit your config.\n");
				return;
			}

			if (!*fname || strchr(fname, '*'))
			{
				Con_SubPrintf(console, "XMPP: /sendfile FILENAME [to]\n");
				return;
			}
			else
			{
				JCL_ToJID(jcl, *arg[2]?arg[2]:console, nname, sizeof(nname), true);

				XMPP_FT_SendFile(jcl, console, nname, fname);
			}
		}
#endif
		else if (!strcmp(arg[0]+1, "joinchatroom") || !strcmp(arg[0]+1, "muc") || !strcmp(arg[0]+1, "joinmuc")) 
		{
			JCL_JoinMUCChat(jcl, arg[1], arg[2], arg[3], arg[4]);
		}
		else if (!strcmp(arg[0]+1, "leavechatroom") || !strcmp(arg[0]+1, "leavemuc")) 
		{
			char roomserverhandle[512];
			buddy_t *b;
			bresource_t *r;
			Q_snprintf(roomserverhandle, sizeof(roomserverhandle), "%s@%s/%s", arg[1], arg[2], arg[3]);
			if (JCL_FindBuddy(jcl, roomserverhandle, &b, &r, false))
			{
				b->chatroom = true;
				JCL_AddClientMessagef(jcl, "<presence to='%s' type='unavailable'/>", roomserverhandle);
				JCL_ForgetBuddy(jcl, b, NULL);
			}
		}
		else if (!strcmp(arg[0]+1, "slap")) 
		{
			char *msgtab[] =
			{
				"/me slaps you around a bit with a large trout",
				"/me slaps you around a bit with a large tunafish",
				"/me slaps you around a bit with a slimy hagfish",
				"/me slaps a large trout around a bit with your face",
				"/me gets eaten by a rabid shark while trying to slap you with it",
				"/me gets crushed under the weight of a large whale",
				"/me searches for a fresh fish, but gets crabs instead",
				"/me searches for a fish, but there are no more fish in the sea",
				"/me tickles you around a bit with a large fish finger",
				"/me goes to order cod and chips. brb",
				"/me goes to watch some monty python"
			};
			JCL_ToJID(jcl, *arg[1]?arg[1]:console, nname, sizeof(nname), true);
			JCL_AttentionMessage(jcl, nname, msgtab[rand()%(sizeof(msgtab)/sizeof(msgtab[0]))]);
		}
		else if (!strcmp(arg[0]+1, "poke")) 
		{
			JCL_ToJID(jcl, *arg[1]?arg[1]:console, nname, sizeof(nname), true);
			JCL_AttentionMessage(jcl, nname, NULL);
		}
		else if (!strcmp(arg[0]+1, "raw")) 
		{
			//parse it first, so noone ever generates invalid xml and gets kicked... too obviously.
			int pos = 0;
			int maxpos = strlen(arg[1]);
			xmltree_t *t;
			while (pos != maxpos)
			{
				t = XML_Parse(arg[1], &pos, maxpos, false, "");
				if (t)
					XML_Destroy(t);
				else
					break;
			}
			if (pos == maxpos)
			{
				jcl->streamdebug = true;
				JCL_AddClientMessageString(jcl, arg[1]);
			}
			else
				Con_Printf("XML not well formed\n");
		}
		else
			Con_SubPrintf(console, "Unrecognised command: %s\n", arg[0]);
	}
	else
	{
		msg = imsg;
		if (jcl && jcl->status == JCL_ACTIVE)
		{
			if (!*msg)
			{
				if (!*console)
				{
					if (BUILTINISVALID(Con_GetConsoleFloat))
						JCL_RegenerateBuddyList(true);
					else
						JCL_PrintBuddyList(console, jcl, false);
					//Con_TrySubPrint(console, "For help, type \"^[/" COMMANDPREFIX " /help^]\"\n");
				}
			}
			else
			{
				JCL_ToJID(jcl, *console?console:jcl->defaultdest, nname, sizeof(nname), true);
				JCL_SendMessage(jcl, nname, msg);
			}
		}
		else
		{
			if (!*msg && BUILTINISVALID(Con_GetConsoleFloat))
				JCL_RegenerateBuddyList(true);
			else
				Con_TrySubPrint(console, "Not connected. For help, type \"^[/" COMMANDPREFIX " /help^]\"\n");
		}
	}
}
