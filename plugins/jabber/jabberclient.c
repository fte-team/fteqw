//Released under the terms of the gpl as this file uses a bit of quake derived code. All sections of the like are marked as such

/*
Network limitations:
	googletalk:
		username: same as gmail (foobar@gmail.com).
		FIXME: need to test foobar@googlemail.com
		auth mechanism: oauth2(tls+nontls) or plain(tls-only). no digests supported, so mitm can easily grab your password if they use certificate authority hackery, so DO NOT log in from work.
		oauth2: I've registered a clientid for use with googletalk's network, but the whole web-browser-is-required crap makes it near unusable. We'll try it if they omit a password.
		otherwise a complete implementation.

	facebook:
		username: foobar@chat.facebook.com
		auth mechanism: digest-md5, x-facebook-platform.
		gateway implementation: no arbitary iq support (no invite/join/voice).
		no roster control
		completely untested. I've no interest in signing up to be tracked constantly (but somehow google is okay... go figure... I guess I'm just trying to avoid a double-whammy)
		oauth2: no idea where to register a clientid, or what the correct addresses are. a google search implies they don't do refresh tokens properly. sticking with digest-md5 should work.

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

client compat:
	googletalk:
		implements old version of jingle. voice calls not compatible.
		not tested by me.

	pidgin:
		(linux) has issues with jingle+ice, and can be made to crash. voip uses speex. pidgin's ice seems vulnerable to dropped packets.
		(windows) doesn't support voice calls
		otherwise works.
*/

#include "../plugin.h"
#include <time.h>
#include "../../engine/common/netinc.h"
#include "xml.h"

//#define NOICE
#define VOIP_SPEEX
//#define FILETRANSFERS	//IBB only, speeds suck. autoaccept is forced on. no protection from mods stuffcmding sendfile commands. needs more extensive testing
#define JINGLE

#ifdef VOIP_SPEEX
#define VOIP
#endif

#define DEFAULTDOMAIN ""
#define DEFAULTRESOURCE "Quake"
#define QUAKEMEDIATYPE "quake"
#define QUAKEMEDIAXMLNS "fteqw.com:netmedia"
#define DEFAULTICEMODE ICEM_ICE

#ifdef JINGLE
icefuncs_t *piceapi;
#endif


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

#ifdef _WIN32
#include "windns.h"
static DNS_STATUS (WINAPI *pDnsQuery_UTF8) (PCSTR pszName, WORD wType, DWORD Options, PIP4_ARRAY aipServers, PDNS_RECORD *ppQueryResults, PVOID *pReserved);
static VOID (WINAPI *pDnsRecordListFree)(PDNS_RECORD pRecordList, DNS_FREE_TYPE FreeType);
static HMODULE dnsapi_lib;
qboolean NET_DNSLookup_SRV(char *host, char *out, int outlen)
{
	HRESULT hr;
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
	hr = pDnsQuery_UTF8(host, DNS_TYPE_SRV, DNS_QUERY_STANDARD, NULL, &result, NULL);
	if (result)
	{
		Q_snprintf(out, outlen, "[%s]:%i", result->Data.SRV.pNameTarget, result->Data.SRV.wPort);
		pDnsRecordListFree(result, DnsFreeRecordList);
		return true;
	}
	return false;
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




void RenameConsole(char *totrim);
void JCL_Command(int accid, char *consolename);
void JCL_LoadConfig(void);
void JCL_WriteConfig(void);

qintptr_t JCL_ExecuteCommand(qintptr_t *args)
{
	char cmd[256];
	pCmd_Argv(0, cmd, sizeof(cmd));
	if (!strcmp(cmd, COMMANDPREFIX) || !strcmp(cmd, COMMANDPREFIX2) || !strcmp(cmd, COMMANDPREFIX3))
	{
		if (!args[0])
			JCL_Command(0, "");
		return true;
	}
	if (!strncmp(cmd, COMMANDPREFIX, strlen(COMMANDPREFIX)))
	{
		if (!args[0])
			JCL_Command(atoi(cmd+strlen(COMMANDPREFIX)), "");
		return true;
	}
	return false;
}

qintptr_t JCL_ConsoleLink(qintptr_t *args);
qintptr_t JCL_ConExecuteCommand(qintptr_t *args);

qintptr_t JCL_Frame(qintptr_t *args);
qintptr_t JCL_Shutdown(qintptr_t *args);

qintptr_t Plug_Init(qintptr_t *args)
{
	if (	Plug_Export("Tick", JCL_Frame) &&
		Plug_Export("Shutdown", JCL_Shutdown) &&
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
		pCvar_Register("xmpp_autoacceptjoins",		"0", 0, "xmpp");
		pCvar_Register("xmpp_autoacceptinvites",	"0", 0, "xmpp");
		pCvar_Register("xmpp_autoacceptvoice",		"0", 0, "xmpp");
		pCvar_Register("xmpp_debug",				"0", 0, "xmpp");

#ifdef JINGLE
		CHECKBUILTIN(Plug_GetNativePointer);
		if (BUILTINISVALID(Plug_GetNativePointer))
			piceapi = pPlug_GetNativePointer(ICE_API_CURRENT);
#endif

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
#define CAP_POKE	8	//can be slapped.
#define CAP_SIFT	16	//non-jingle file transfers

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
	qboolean chatroom;	//chatrooms are bizzare things that need special handling.

	char name[256];

	struct buddy_s *next;
	char accountdomain[1];	//no resource on there
} buddy_t;
typedef struct jclient_s
{
	int accountnum;	//a private id to track which client links are associated with

	char redirserveraddr[64];	//if empty, do an srv lookup.

	enum
	{
		JCL_INACTIVE,	//not trying to connect.
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

	//config info
	char serveraddr[64];	//if empty, do an srv lookup.
	int serverport;
	char domain[256];
	char username[256];
	char password[256];
	char resource[256];
	char certificatedomain[256];
	int forcetls;	//-1=off, 0=ifpossible, 1=fail if can't upgrade, 2=old-style tls
	qboolean allowauth_plainnontls;	//allow plain plain
	qboolean allowauth_plaintls;	//allow tls plain
	qboolean allowauth_digestmd5;	//allow digest-md5 auth
	qboolean allowauth_scramsha1;	//allow scram-sha-1 auth
	qboolean allowauth_oauth2;		//use oauth2 where possible
	
	char jid[256];	//this is our full username@domain/resource string
	char localalias[256];//this is what's shown infront of outgoing messages. >> by default until we can get our name.

	char authnonce[256];
	int authmode;

	int tagdepth;
	int openbracket;
	int instreampos;

	qboolean connected;	//fully on server and authed and everything.
	qboolean issecure;	//tls enabled (either upgraded or initially)
	qboolean streamdebug;	//echo the stream to subconsoles

	qboolean preapproval;	//server supports presence preapproval

	char curquakeserver[2048];
	char defaultnamespace[2048];	//should be 'jabber:client' or blank (and spammy with all the extra xmlns attribs)

	struct
	{
		char saslmethod[64];
		char obtainurl[256];
		char refreshurl[256];
		char clientid[256];
		char clientsecret[256];
		char *useraccount;
		char *scope;
		char *accesstoken;	//one-shot access token
		char *refreshtoken;	//long-term token that we can use to get new access tokens
		char *authtoken;	//short-term authorisation token, usable to get an access token (and a refresh token if we're lucky)
	} oauth2;

	struct iq_s
	{
		struct iq_s *next;
		char id[64];
		int timeout;
		qboolean (*callback) (struct jclient_s *jcl, struct subtree_s *tree, struct iq_s *iq);
		void *usrptr;
	} *pendingiqs;

#ifdef JINGLE
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
#endif

#ifdef FILETRANSFERS
	struct ft_s
	{
		struct ft_s *next;
		char fname[MAX_QPATH];
		int size;
		char *with;
		char md5hash[16];
		char sid[64];
		int blocksize;
		unsigned short seq;
		qhandle_t file;
		qboolean begun;
		qboolean transmitting;

		enum
		{
			FT_IBB,			//in-band bytestreams
			FT_BYTESTREAM	//aka: relay
		} method;
	} *ft;
#endif

	buddy_t *buddies;
} jclient_t;
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
	int l, i;
	buf_t salt;
	buf_t csn;
	buf_t itr;
	buf_t final;
	buf_t sigkey;
	char salted_password[20];
	char proof[20];
	char proof64[30];
	char clientkey[20];
	char storedkey[20];
	char clientsignature[20];
	char *username = jcl->username;
	char *password = jcl->password;

#if 0
	/*hack zone*/
	in = "r=fyko+d2lbbFgONRv9qkxdawL3rfcNHYJY1ZVvWVs7j,s=QSXCR+Q6sek8bf92,i=4096";
	inlen = strlen(in);
	strcpy(jcl->authnonce, "wvDh8bTUrSc=");//"fyko+d2lbbFgONRv9qkxdawL");
	username = "user";
	password = "pencil";
	//should result in "c=biws,r=fyko+d2lbbFgONRv9qkxdawL3rfcNHYJY1ZVvWVs7j,p=v0X8v3Bz2T0CJGbJQyF0X+HI4Ts="
#endif
	saslchal.len = 0;
	buf_cat(&saslchal, in, inlen);
	
	//be warned, these CAN contain nulls.
	csn.len = saslattr(csn.buf, sizeof(csn.buf), saslchal.buf, saslchal.len, "r");
	salt.len = saslattr(salt.buf, sizeof(salt.buf), saslchal.buf, saslchal.len, "s");
	itr.len = saslattr(itr.buf, sizeof(itr.buf), saslchal.buf, saslchal.len, "i");

	salt.len = Base64_Decode(salt.buf, sizeof(salt.buf), salt.buf, salt.len);
	
	//FIXME: should we validate that csn is prefixed with our cnonce?

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
	SHA1(storedkey, sizeof(storedkey), clientkey, sizeof(clientkey));
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
	char msg[4096];
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

		Con_Printf("Please visit ^[^4%s\\url\\%s^] and then enter:\n^[/"COMMANDPREFIX"%i /oa2token <TOKEN>^]\n", url, url, jcl->accountnum);

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
		XML_ConPrintTree(x, 1);
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
		XML_ConPrintTree(x, 1);

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
	return -1;
}

//in descending priority order
saslmethod_t saslmethods[] =
{
	{NULL,				sasl_oauth2_initial,		NULL},						//potentially avoids having to ask+store their password. a browser is required to obtain auth token for us.
	{"SCRAM-SHA-1",		sasl_scramsha1_initial,		sasl_scramsha1_challenge},	//lots of unreadable hashing
	{"DIGEST-MD5",		sasl_digestmd5_initial,		sasl_digestmd5_challenge},	//kinda silly
	{"PLAIN",			sasl_plain_initial,			NULL}						//realm\0username\0password
};

/*
pidgin's msn request
https://oauth.live.com/authorize?client_id=000000004C07035A&scope=wl.messenger,wl.basic,wl.offline_access,wl.contacts_create,wl.share&response_type=token&redirect_uri=http://pidgin.im/
*/


struct subtree_s;

void JCL_AddClientMessagef(jclient_t *jcl, char *fmt, ...);
qboolean JCL_FindBuddy(jclient_t *jcl, char *jid, buddy_t **buddy, bresource_t **bres);
void JCL_GeneratePresence(jclient_t *jcl, qboolean force);
struct iq_s *JCL_SendIQf(jclient_t *jcl, qboolean (*callback) (jclient_t *jcl, struct subtree_s *tree, struct iq_s *iq), char *iqtype, char *target, char *fmt, ...);
struct iq_s *JCL_SendIQNode(jclient_t *jcl, qboolean (*callback) (jclient_t *jcl, xmltree_t *tree, struct iq_s *iq), char *iqtype, char *target, xmltree_t *node, qboolean destroynode);
void JCL_CloseConnection(jclient_t *jcl, qboolean reconnect);
void JCL_JoinMUCChat(jclient_t *jcl, char *room, char *server, char *myhandle, char *password);

void JCL_GenLink(jclient_t *jcl, char *out, int outlen, char *action, char *context, char *contextres, char *sid, char *txtfmt, ...)
{
	va_list		argptr;
	qboolean textonly = false;
	*out = 0;
	if (!strchr(txtfmt, '%'))
	{
		Q_strlcpy(out, "bad link text", outlen);
		return;
	}
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

#ifdef JINGLE
static struct c2c_s *JCL_JingleCreateSession(jclient_t *jcl, char *with, qboolean creator, char *sid, int method, int mediatype)
{
	struct icestate_s *ice = NULL;
	struct c2c_s *c2c;
	char generatedname[64];
	char stunhost[256];
	
	if (piceapi)
		ice = piceapi->ICE_Create(NULL, sid, with, method, mediatype);
	if (ice)
	{
		piceapi->ICE_Get(ice, "sid", generatedname, sizeof(generatedname));
		sid = generatedname;

		//note: the engine will ignore codecs it does not support.
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
				c2c = JCL_JingleCreateSession(jcl, target, true, sid, DEFAULTICEMODE, ((protocol == ICEP_INVALID)?ICEP_QWCLIENT:protocol));
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

	//FIXME: if both people try to establish a connection to the other simultaneously, the higher session id is meant to be canceled, and the lower accepted automagically.

	c2c = JCL_JingleCreateSession(jcl, from, false,
		sid,
		strcmp(transportxmlns, "urn:xmpp:jingle:transports:raw-udp:1")?ICEM_ICE:ICEM_RAW,
		mt
		);
	if (!c2c)
		return false;

	
	JCL_FindBuddy(jcl, from, &b, NULL);

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

	JCL_JingleParsePeerPorts(jcl, c2c, inj, from);

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
			Con_SubPrintf(b->name, "Session Accepted!\n");
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
#endif

qintptr_t JCL_ConsoleLink(qintptr_t *args)
{
	jclient_t *jcl;
	char text[256];
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
#ifdef JINGLE
	else if (!strcmp(what, "jauth"))
	{
		JCL_Info_ValueForKey(link, "xmppsid", what, sizeof(what));
		if (jcl && jcl->status == JCL_ACTIVE)
			JCL_Join(jcl, who, what, true, ICEP_INVALID);
		return true;
	}
	else if (!strcmp(what, "jdeny"))
	{
		JCL_Info_ValueForKey(link, "xmppsid", what, sizeof(what));
		if (jcl && jcl->status == JCL_ACTIVE)
			JCL_Join(jcl, who, what, false, ICEP_INVALID);
		return true;
	}
	else if (!strcmp(what, "join"))
	{
		if (jcl && jcl->status == JCL_ACTIVE)
			JCL_Join(jcl, who, NULL, true, ICEP_QWCLIENT);
		return true;
	}
	else if (!strcmp(what, "invite"))
	{
		if (jcl && jcl->status == JCL_ACTIVE)
			JCL_Join(jcl, who, NULL, true, ICEP_QWSERVER);
		return true;
	}
	else if (!strcmp(what, "call"))
	{
		if (jcl && jcl->status == JCL_ACTIVE)
			JCL_Join(jcl, who, NULL, true, ICEP_VOICE);
		return true;
	}
#endif
	else if (!strcmp(what, "mucjoin"))
	{	//conference/chat join
		JCL_Info_ValueForKey(link, "xmppsid", what, sizeof(what));
		JCL_JoinMUCChat(jcl, who, NULL, NULL, what);
	}
	else if ((*who && !*what) || !strcmp(what, "msg"))
	{
		if (jcl)
		{
			char *f;
			buddy_t *b;
			bresource_t *br;

			JCL_FindBuddy(jcl, *who?who:jcl->defaultdest, &b, &br);
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
	jclient_t *jcl = jclients[0];

	if (!jcl)
	{
		char buffer[256];
		pCmd_Argv(0, buffer, sizeof(buffer));
		Con_SubPrintf(buffer, "You were disconnected\n");
		return true;
	}
	pCmd_Argv(0, consolename, sizeof(consolename));
	for (b = jcl->buddies; b; b = b->next)
	{
		if (!strcmp(b->name, consolename))
		{
			if (b->defaultresource)
				Q_snprintf(jcl->defaultdest, sizeof(jcl->defaultdest), "%s/%s", b->accountdomain, b->defaultresource->resource);
			else
				Q_snprintf(jcl->defaultdest, sizeof(jcl->defaultdest), "%s", b->accountdomain);
			break;
		}
	}
	JCL_Command(0, consolename);
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
			Con_SubPrintf("xmppout", COLOURYELLOW "%s\n", jcl->outbuf + jcl->outbufpos);
			jcl->outbuf[jcl->outbufpos+sent] = t;
		}

		jcl->outbufpos += sent;
		jcl->outbuflen -= sent;
	}
	else if (sent < 0)
		Con_Printf("Error sending\n");
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
			Con_Printf("XMPP: Trying to connect to %s (%s)\n", jcl->domain, srvserver);
			jcl->socket = pNet_TCPConnect(srvserver, jcl->serverport);	//port is should already be part of the srvserver name
		}
		else
		{
			Con_Printf("XMPP: Unable to determine server. Check internet connection.\n");
			return false;
		}
	}
	else
	{
		Con_Printf("XMPP: Trying to connect to %s\n", jcl->domain);
		jcl->socket = pNet_TCPConnect(serveraddr, jcl->serverport);	//port is only used if the url doesn't contain one. It's a default.
	}

	//not yet blocking. So no frequent attempts please...
	//non blocking prevents connect from returning worthwhile sensible value.
	if ((int)jcl->socket < 0)
	{
		Con_Printf("JCL_OpenSocket: couldn't connect\n");
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
	char oauthname[256];

	jcl = malloc(sizeof(jclient_t));
	if (!jcl)
		return NULL;

	memset(jcl, 0, sizeof(jclient_t));
	jcl->socket = -1;

	jcl->accountnum = atoi(XML_GetParameter(acc, "id", "1"));

	//make sure dependant properties are listed beneath their dependancies...
	jcl->forcetls = atoi(XML_GetChildBody(acc, "forcetls", "1"));
	jcl->streamdebug = !!atoi(XML_GetChildBody(acc, "streamdebug", "0"));
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

	return jcl;
}

jclient_t *JCL_Connect(int accnum, char *server, int forcetls, char *account, char *password)
{
	char srvserver[256];
	char gamename[64];
	jclient_t *jcl;
	char *domain;
	char *res;
	xmltree_t *x;

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
		domain = DEFAULTDOMAIN;
		if (domain && *domain)
			Con_Printf("XMPP: domain not specified, assuming %s\n", domain);
		else
		{
			Con_Printf("XMPP: domain not specified\n");
			return NULL;
		}
	}

	x = XML_CreateNode(NULL, "account", "", "");
	XML_AddParameteri(x, "id", accnum);
	XML_CreateNode(x, "server", "", server);
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

//FIXME: add flags to avoid creation
qboolean JCL_FindBuddy(jclient_t *jcl, char *jid, buddy_t **buddy, bresource_t **bres)
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
	char *s = XML_GenerateString(node, false);
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
			char myjid[512];
			Q_strlcpy(jcl->jid, c->body, sizeof(jcl->jid));
			JCL_GenLink(jcl, myjid, sizeof(myjid), NULL, jcl->jid, NULL, NULL, "%s", jcl->jid);
			Con_Printf("Bound to jid %s\n", jcl->jid, jcl->jid);
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
	#ifdef JINGLE
		"urn:xmpp:jingle:1",
		QUAKEMEDIAXMLNS,
		#ifdef VOIP
			"urn:xmpp:jingle:apps:rtp:1",
			"urn:xmpp:jingle:apps:rtp:audio",
		#endif
		//"urn:xmpp:jingle:apps:rtp:video",//we don't support rtp video chat
		"urn:xmpp:jingle:transports:raw-udp:1",
		#ifndef NOICE
			"urn:xmpp:jingle:transports:ice-udp:1",
		#endif
	#endif
	#ifndef Q3_VM
		"urn:xmpp:time",
	#endif
	"urn:xmpp:ping",	//FIXME: I'm not keen on this. I only added support to stop errors from pidgin when trying to debug.
	"urn:xmpp:attention:0",	//poke.

	//file transfer
	#ifdef FILETRANSFER
		"http://jabber.org/protocol/si",
		"http://jabber.org/protocol/si/profile/file-transfer",
		"http://jabber.org/protocol/ibb",
		//"http://jabber.org/protocol/bytestreams",
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
#ifdef FILETRANSFERS
		ot = XML_ChildOfTreeNS(tree, "http://jabber.org/protocol/ibb", "open", 0);
		if (ot)
		{
			struct ft_s *ft;
			char *sid = XML_GetParameter(ot, "sid", "");
			int blocksize = atoi(XML_GetParameter(ot, "block-size", "4096"));	//technically this is required.
			char *stanza = XML_GetParameter(ot, "stanza", "iq");
			for (ft = jcl->ft; ft; ft = ft->next)
			{
				if (!strcmp(ft->sid, sid))
				{
					if (!ft->begun && ft->transmitting == false)
					{
						if (blocksize > 65536 || strcmp(stanza, "iq"))
						{	//blocksize: MUST NOT be greater than 65535
							JCL_AddClientMessagef(jcl, 
									"<iq id='%s' to='%s' type='error'>"
										"<error type='modify'>"
											"<not-acceptable xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>"
										"</error>"
									"</iq>"
									, id, from);
						}
						else if (blocksize > 4096)
						{	//ask for smaller chunks
							JCL_AddClientMessagef(jcl, 
								"<iq id='%s' to='%s' type='error'>"
									"<error type='modify'>"
										"<resource-constraint xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>"
									"</error>"
								"</iq>"
								, id, from);
						}
						else
						{	//it looks okay
							pFS_Open(ft->fname, &ft->file, 2);
							ft->method = FT_IBB;
							ft->blocksize = blocksize;
							ft->begun = true;
							//if its okay...
							JCL_AddClientMessagef(jcl, "<iq id='%s' to='%s' type='result'/>", id, from);
						}
						return;
					}
				}
			}
		}
		ot = XML_ChildOfTreeNS(tree, "http://jabber.org/protocol/ibb", "close", 0);
		if (ot)
		{
			struct ft_s **link, *ft;
			char *sid = XML_GetParameter(ot, "sid", "");
			for (link = &jcl->ft; *link; link = &(*link)->next)
			{
				ft = *link;
				if (!strcmp(ft->sid, sid))
				{
					if (ft->begun && ft->method == FT_IBB)
					{
						int size;
						pFS_Close(ft->file);
						if (ft->transmitting)
						{
							Con_Printf("%s aborted transfer of \"%s\"\n", from, ft->fname);
						}
						else
						{
							size = pFS_Open(ft->fname, &ft->file, 1);
							pFS_Close(ft->file);
							if (size == ft->size)
								Con_Printf("Received file \"%s\" successfully\n", ft->fname);
							else
								Con_Printf("%s aborted transfer of \"%s\"\n", from, ft->fname);
						}
						*link = ft->next;
						free(ft);
						//if its okay...
						JCL_AddClientMessagef(jcl, "<iq id='%s' to='%s' type='result'/>", id, from);
						return;
					}
				}
			}
		}
		ot = XML_ChildOfTreeNS(tree, "http://jabber.org/protocol/ibb", "data", 0);
		if (ot)
		{
			char block[65536];
			char *sid = XML_GetParameter(ot, "sid", "");
			unsigned short seq = atoi(XML_GetParameter(ot, "seq", "0"));
			int blocksize;
			struct ft_s *ft;
			for (ft = jcl->ft; ft; ft = ft->next)
			{
				if (!strcmp(ft->sid, sid) && !ft->transmitting)
				{
					blocksize = Base64_Decode(block, sizeof(block), ot->body, strlen(ot->body));
					if (blocksize && blocksize <= ft->blocksize)
					{
						pFS_Write(ft->file, block, blocksize);
						JCL_AddClientMessagef(jcl, "<iq id='%s' to='%s' type='result'/>", id, from);
						return;
					}
					else
						Con_Printf("XMPP: Invalid blocksize in file transfer from %s\n", from);
					break;
				}
			}
		}

		ot = XML_ChildOfTreeNS(tree, "http://jabber.org/protocol/si", "si", 0);
		if (ot)
		{
			char *profile = XML_GetParameter(ot, "profile", "");
			unparsable = false;

			if (!strcmp(profile, "http://jabber.org/protocol/si/profile/file-transfer"))
			{
				char *s;
				xmltree_t *repiq, *repsi, *c;
				char *mimetype = XML_GetParameter(ot, "mime-type", "text/plain");
				char *sid = XML_GetParameter(ot, "id", "");
				xmltree_t *file = XML_ChildOfTreeNS(ot, "http://jabber.org/protocol/si/profile/file-transfer", "file", 0);
				char *fname = XML_GetParameter(file, "name", "file.txt");
				char *date = XML_GetParameter(file, "date", "");
				char *md5hash = XML_GetParameter(file, "hash", "");
				int fsize = strtoul(XML_GetParameter(file, "size", "0"), NULL, 0);
				char *desc = XML_GetChildBody(file, "desc", "");

				//file transfer offer
				struct ft_s *ft = malloc(sizeof(*ft));
				memset(ft, 0, sizeof(*ft));
				ft->next = jcl->ft;
				jcl->ft = ft;

				ft->transmitting = false;
				Q_strlcpy(ft->sid, sid, sizeof(ft->sid));
				Q_strlcpy(ft->fname, fname, sizeof(ft->sid));
				Base64_Decode(ft->md5hash, sizeof(ft->md5hash), md5hash, strlen(md5hash));
				ft->size = fsize;
				ft->file = -1;
//				ft->with = 
				ft->method = FT_IBB;
				ft->begun = false;

				Con_Printf("Receiving file \"%s\" from \"%s\" (%i bytes)\n", fname, from, fsize);
		
				//generate a response.
				//FIXME: we ought to delay response until after we prompt.
				repiq = XML_CreateNode(NULL, "iq", "", "");
				XML_AddParameter(repiq, "type", "result");
				XML_AddParameter(repiq, "to", from);
				XML_AddParameter(repiq, "id", id);
				repsi = XML_CreateNode(repiq, "si", "http://jabber.org/protocol/si", "");
				XML_CreateNode(repsi, "file", "http://jabber.org/protocol/si/profile/file-transfer", "");	//I don't really get the point of this.
				c = XML_CreateNode(repsi, "feature", "http://jabber.org/protocol/feature-neg", "");
				c = XML_CreateNode(c, "x", "jabber:x:data", "");
				XML_AddParameter(c, "type", "submit");
				c = XML_CreateNode(c, "field", "", "");
				XML_AddParameter(c, "var", "stream-method");
				if (ft->method == FT_IBB)
					c = XML_CreateNode(c, "value", "", "http://jabber.org/protocol/ibb");
				else if (ft->method == FT_BYTESTREAM)
					c = XML_CreateNode(c, "value", "", "http://jabber.org/protocol/bytestreams");

				s = XML_GenerateString(repiq, false);
				JCL_AddClientMessageString(jcl, s);
				free(s);
				XML_Destroy(repiq);
			}
			else
			{
				//profile not understood
				JCL_AddClientMessagef(jcl,
						"<iq type='error' to='%s' id='%s'>"
							"<error code='400' type='cancel'>"
								"<bad-request xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>"
								"<bad-profile xmlns='http://jabber.org/protocol/si'/>"
							"</error>"
						"</iq>", from, id);
			}
		}
#endif
		c = XML_ChildOfTree(tree, "query", 0);
		if (c && !strcmp(c->xmlns, "jabber:iq:roster"))
		{
			unparsable = false;
			JCL_RosterUpdate(jcl, c);
		}

#ifdef JINGLE
		c = XML_ChildOfTree(tree, "jingle", 0);
		if (c && !strcmp(c->xmlns, "urn:xmpp:jingle:1"))
		{
			unparsable = !JCL_ParseJingle(jcl, c, from, id);
		}
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
	char *type = XML_GetParameter(tree, "type", "normal");
	char *ctx;

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
			ctx = b->name;
			if (!strcmp(type, "groupchat"))
				f = br->resource;
			else if (b->chatroom)	//need to use the full resource for private chat within a room
			{
				ctx = f;
				f = br->resource;
			}
			else
			{
				f = b->name;
				b->defaultresource = br;
			}
		}

		if (f)
		{
			ot = XML_ChildOfTree(tree, "composing", 0);
			if (ot && !strcmp(ot->xmlns, "http://jabber.org/protocol/chatstates"))
			{
				unparsable = false;
				Con_SubPrintf(ctx, "%s is typing\r", f);
			}
			ot = XML_ChildOfTree(tree, "paused", 0);
			if (ot && !strcmp(ot->xmlns, "http://jabber.org/protocol/chatstates"))
			{
				unparsable = false;
				Con_SubPrintf(ctx, "%s has stopped typing\r", f);
			}
			ot = XML_ChildOfTree(tree, "inactive", 0);
			if (ot && !strcmp(ot->xmlns, "http://jabber.org/protocol/chatstates"))
			{
				unparsable = false;
				Con_SubPrintf(ctx, "\r", f);
			}
			ot = XML_ChildOfTree(tree, "active", 0);
			if (ot && !strcmp(ot->xmlns, "http://jabber.org/protocol/chatstates"))
			{
				unparsable = false;
				Con_SubPrintf(ctx, "\r", f);
			}
			ot = XML_ChildOfTree(tree, "gone", 0);
			if (ot && !strcmp(ot->xmlns, "http://jabber.org/protocol/chatstates"))
			{
				unparsable = false;
				Con_SubPrintf(ctx, "%s has gone away\r", f);
			}
		}

		ot = XML_ChildOfTree(tree, "attention", 0);
		if (ot)
		{
			if (jclient_poketime < jclient_curtime)	//throttle these.
			{
				jclient_poketime = jclient_curtime + 10*1000;
				Con_SubPrintf(ctx, "%s is an attention whore.\n", f);
				pCon_SetActive(ctx);
				if (BUILTINISVALID(LocalSound))
					pLocalSound("misc/talk.wav");
			}
		}
		
		ot = XML_ChildOfTree(tree, "subject", 0);
		if (ot && !strcmp(type, "groupchat"))
		{
			unparsable = false;
			Con_SubPrintf(ctx, "^2%s^7 has set the topic to: %s\n", f, ot->body);
		}


		ot = XML_ChildOfTreeNS(tree, "http://jabber.org/protocol/muc#user", "x", 0);
		if (ot && f && !strchr(f, '/'))
		{
			//this is an appaling extension protocol. we really have no way to know if someone's just making this shit up just to see our presence.
			//this message came from the groupchat server.
			xmltree_t *inv = XML_ChildOfTree(ot, "invite", 0);
			if (inv)
			{
				char *who = XML_GetParameter(inv, "from", "");
				char *reason = XML_GetChildBody(inv, "reason", NULL);
				char *password = XML_GetChildBody(ot, "password", 0);
				char link[512];
				buddy_t *b;
				JCL_FindBuddy(jcl, f, &b, NULL);
				if (b->chatroom)
					return;	//we already know about it. don't spam.
				JCL_GenLink(jcl, link, sizeof(link), "mucjoin", f, NULL, password, "%s", f);
				if (reason)
					Con_SubPrintf(ctx, "* ^2%s^7 has invited you to join %s: %s.\n", who, link, reason);
				else
					Con_SubPrintf(ctx, "* ^2%s^7 has invited you to join %s.\n", who, link);
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
				Con_SubPrintf(ctx, "* ^2%s^7 has invited you to join %s: %s.\n", f, link, reason);
			else
				Con_SubPrintf(ctx, "* ^2%s^7 has invited you to join %s.\n", f, link);
			return;	//ignore any body
		}

		ot = XML_ChildOfTree(tree, "body", 0);
		if (ot)
		{
			unparsable = false;
			if (f)
			{
				if (!strncmp(ot->body, "/me ", 4))
					Con_SubPrintf(ctx, "* ^2%s^7%s\n", f, ot->body+3);
				else
					Con_SubPrintf(ctx, "^2%s^7: %s\n", f, ot->body);
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
	xmltree_t *muc = XML_ChildOfTreeNS(tree, "http://jabber.org/protocol/muc#user", "x", 0);
	char *type = XML_GetParameter(tree, "type", "");
	char *serverip = NULL;
	char *servermap = NULL;
	char startconvo[512];
	char oldbstatus[128];
	char oldfstatus[128];

	if (quake && !strcmp(quake->xmlns, "fteqw.com:game"))
	{
		serverip = XML_GetParameter(quake, "serverip", NULL);
		servermap = XML_GetParameter(quake, "servermap", NULL);
	}

	if (type && !strcmp(type, "subscribe"))
	{
		char pauth[512], pdeny[512];
		JCL_GenLink(jcl, startconvo, sizeof(startconvo), NULL, from, NULL, NULL, "%s", from);
		JCL_GenLink(jcl, pauth, sizeof(pauth), "pauth", from, NULL, NULL, "%s", "Authorize");
		JCL_GenLink(jcl, pdeny, sizeof(pdeny), "pdeny", from, NULL, NULL, "%s", "Deny");
		Con_Printf("%s wants to be your friend! %s %s\n", startconvo, pauth, pdeny);
	}
	else if (type && !strcmp(type, "subscribed"))
	{
		JCL_GenLink(jcl, startconvo, sizeof(startconvo), NULL, from, NULL, NULL, "%s", from);
		Con_Printf("%s is now your friend!\n", startconvo);
	}
	else if (type && !strcmp(type, "unsubscribe"))
	{
		JCL_GenLink(jcl, startconvo, sizeof(startconvo), NULL, from, NULL, NULL, "%s", from);
		Con_Printf("%s has unfriended you\n", startconvo);
	}
	else if (type && !strcmp(type, "unsubscribed"))
	{
		JCL_GenLink(jcl, startconvo, sizeof(startconvo), NULL, from, NULL, NULL, "%s", from);
		Con_Printf("%s is no longer unfriended you\n", startconvo);
	}
	else
	{
		JCL_FindBuddy(jcl, from, &buddy, &bres);
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
				Q_strlcpy(bres->bstatus, (show && *show->body)?show->body:"present", sizeof(bres->bstatus));
				if (!(bres->caps & CAP_QUERIED))
				{
					bres->caps |= CAP_QUERIED;
					JCL_SendIQ(jcl, JCL_ClientDiscoInfo, "get", from, "<query xmlns='http://jabber.org/protocol/disco#info'/>");
				}
			}

			if (muc)
			{
				JCL_GenLink(jcl, startconvo, sizeof(startconvo), NULL, from, NULL, NULL, "%s", bres->resource);
				if (type && !strcmp(type, "unavailable"))
					Con_SubPrintf(buddy->name, "%s has left the conversation\n", bres->resource);
				else if (strcmp(oldbstatus, bres->bstatus))
					Con_SubPrintf(buddy->name, "%s is now %s\n", startconvo, bres->bstatus);
			}
			else
			{
				if (bres->servertype == 2)
				{
					char joinlink[512];
					JCL_GenLink(jcl, joinlink, sizeof(joinlink), "join", from, NULL, NULL, "Playing Quake - %s", bres->server);
					Con_Printf("%s is now %s\n", startconvo, joinlink);
				}
				else if (bres->servertype == 1)
					Con_Printf("%s is now ^[[Playing Quake - %s]\\observe\\%s^]\n", startconvo, bres->server, bres->server);
				else if (strcmp(oldbstatus, bres->bstatus) || strcmp(oldfstatus, bres->fstatus))
				{
					if (*bres->fstatus)
						Con_Printf("%s is now %s: %s\n", startconvo, bres->bstatus, bres->fstatus);
					else
						Con_Printf("%s is now %s\n", startconvo, bres->bstatus);
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

		if (jcl->streamdebug && tree)
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
			if ((!jcl->issecure) && BUILTINISVALID(Net_SetTLSClient) && XML_ChildOfTree(tree, "starttls", 0) != NULL && jcl->forcetls >= 0)
			{
				Con_Printf("XMPP: Attempting to switch to TLS\n");
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
				if (jcl->forcetls > 0 && !jcl->issecure)
				{
					Con_Printf("XMPP: Unable to switch to TLS.\n");
					XML_ConPrintTree(tree, 0);
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
							if (outlen != -1)
								break;
						}
					}
					if (outlen != -1)
						break;
				}

				if (outlen == -2)
				{
					XML_Destroy(tree);
            		return JCL_KILL;
				}

				if (outlen >= 0)
				{
					jcl->authmode = sm;
					Base64_Add(out, outlen);
					Base64_Finish();

					Con_Printf("XMPP: Authing with %s%s.\n", method, jcl->issecure?" over tls":" without encription");
					JCL_AddClientMessagef(jcl, "<auth xmlns='urn:ietf:params:xml:ns:xmpp-sasl' mechanism='%s'"
						    " auth:service='oauth2'"
							" xmlns:auth='http://www.google.com/talk/protocol/auth'"
							">%s</auth>", method, base64);
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
			Con_Printf("XMPP: unable to auth with server\n");
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
			Con_Printf("XMPP: proceed without TLS\n");
			XML_Destroy(tree);
			return JCL_KILL;
		}

		//when using srv records, the certificate must match the user's domain, rather than merely the hostname of the server.
		//if you want to match the hostname of the server, use (oldstyle) tlsconnect directly instead.
		if (pNet_SetTLSClient(jcl->socket, jcl->certificatedomain)<0)
		{
			Con_Printf("XMPP: failed to switch to TLS\n");
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
			Con_Printf("XMPP: Failure: %s\n", tree->child->name);
		else
			Con_Printf("XMPP: Unknown failure\n");
		XML_Destroy(tree);
		return JCL_KILL;
	}
	else if (!strcmp(tree->name, "error"))
	{
		ot = XML_ChildOfTree(tree, "see-other-host", 0);
		if (ot)
		{
			//msn needs this, apparently
			Q_strlcpy(jcl->redirserveraddr, ot->body, sizeof(jcl->redirserveraddr));
			JCL_CloseConnection(jcl, true);
			if (!JCL_Reconnect(jcl))
				return JCL_KILL;
			return JCL_CONTINUE;
		}
		else
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

	memmove(jcl->bufferedinmessage, jcl->bufferedinmessage+pos, jcl->bufferedinammount-pos);
	jcl->bufferedinammount -= pos;
	jcl->instreampos -= pos;

	if (unparsable)
	{
		Con_Printf("XMPP: Input corrupt, urecognised, or unusable. Disconnecting.\n");
		XML_ConPrintTree(tree, 0);
		XML_Destroy(tree);
		return JCL_KILL;
	}
	XML_Destroy(tree);
	return JCL_CONTINUE;
}

void JCL_CloseConnection(jclient_t *jcl, qboolean reconnect)
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
	int i;
	jclient_curtime = args[0];
	for (i = 0; i < sizeof(jclients)/sizeof(jclients[0]); i++)
	{
		jclient_t *jcl = jclients[i];
		if (jcl && jcl->status != JCL_INACTIVE)
		{
			int stat = JCL_CONTINUE;
#ifdef JINGLE
			JCL_JingleTimeouts(jcl, false);
#endif
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
				{
					JCL_CloseConnection(jcl, true);
					jcl->status = JCL_INACTIVE;
				}
				else if (stat == JCL_KILL)
					JCL_CloseConnection(jcl, true);
				else
					JCL_FlushOutgoing(jcl);
			}
		}
	}
	return 0;
}

void JCL_WriteConfig(void)
{
	xmltree_t *m, *n, *oauth2;
	int i;
	qhandle_t config;


	m = XML_CreateNode(NULL, "xmppaccounts", "", "");
	for (i = 0; i < sizeof(jclients) / sizeof(jclients[0]);  i++)
	{
		jclient_t *jcl = jclients[i];
		if (jcl)
		{
			char foo[64];
			n = XML_CreateNode(m, "account", "", "");
			XML_AddParameteri(n, "id", i);

			XML_CreateNode(n, "streamdebug", "", jcl->streamdebug?"1":"0");
			Q_snprintf(foo, sizeof(foo), "%i", jcl->forcetls);
			XML_CreateNode(n, "forcetls", "", foo);
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
	if (!jclients[0])
	{
		int len;
		qhandle_t config;
		char buf[8192];
		qboolean oldtls;
		len = pFS_Open("**plugconfig", &config, 1);
		if (config >= 0)
		{
			if (len >= sizeof(buf))
				len = sizeof(buf)-1;
			buf[len] = 0;
			pFS_Read(config, buf, len);
			pFS_Close(config);

			if (*buf != '<')
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
			JCL_CloseConnection(jcl, false);
	}
	return true;
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

	if ((r->caps & CAP_INVITE) && !r->servertype)
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
}
void JCL_PrintBuddyList(char *console, jclient_t *jcl, qboolean all)
{
	buddy_t *b;
	bresource_t *r;
	struct c2c_s *c2c;
	struct ft_s *ft;
	char convolink[512], actlink[512];
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
				JCL_GenLink(jcl, convolink, sizeof(convolink), NULL, b->accountdomain, NULL, NULL, "^s^7%s^r", b->name);
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
		JCL_FindBuddy(jcl, c2c->with, &b, &r);
		switch(c2c->mediatype)
		{
		case ICEP_VOICE:
			JCL_GenLink(jcl, convolink, sizeof(convolink), NULL, b->accountdomain, r->resource, NULL, "%s", b->name);
			JCL_GenLink(jcl, actlink, sizeof(actlink), "jdeny", c2c->with, NULL, c2c->sid, "%s", "Hang Up");
			Con_SubPrintf(console, "    %s: voice %s\n", convolink, actlink);
			break;
		case ICEP_QWSERVER:
			JCL_GenLink(jcl, convolink, sizeof(convolink), NULL, b->accountdomain, r->resource, NULL, "%s", b->name);
			JCL_GenLink(jcl, actlink, sizeof(actlink), "jdeny", c2c->with, NULL, c2c->sid, "%s", "Kick");
			Con_SubPrintf(console, "    %s: server %s\n", convolink, actlink);
			break;
		case ICEP_QWCLIENT:
			JCL_GenLink(jcl, convolink, sizeof(convolink), NULL, b->accountdomain, r->resource, NULL, "%s", b->name);
			JCL_GenLink(jcl, actlink, sizeof(actlink), "jdeny", c2c->with, NULL, c2c->sid, "%s", "Disconnect");
			Con_SubPrintf(console, "    %s: client %s\n", convolink, actlink);
			break;
		}
	}
#endif

#ifdef FILETRANSFERS
	if (jcl->ft)
		Con_SubPrintf(console, "Active file transfers:\n");
	for (ft = jcl->ft; ft; ft = ft->next)
	{
		JCL_FindBuddy(jcl, ft->with, &b, &r);
		JCL_GenLink(jcl, convolink, sizeof(convolink), NULL, b->accountdomain, r->resource, NULL, "%s", b->name);
		JCL_GenLink(jcl, actlink, sizeof(actlink), "ftdeny", ft->with, NULL, ft->sid, "%s", "Hang Up");
		Con_SubPrintf(console, "    %s: %s\n", convolink, ft->fname);
	}
#endif
}

void JCL_SendMessage(jclient_t *jcl, char *to, char *msg)
{
	char markup[1024];
	char *con;
	buddy_t *b;
	bresource_t *br;
	JCL_FindBuddy(jcl, to, &b, &br);
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
	}
	else
	{
		con = b->name;
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
		Con_SubPrintf(con, "* ^5%s^7"COLOURYELLOW"%s\n", ((!strcmp(jcl->localalias, ">>"))?"me":jcl->localalias), msg+3);
	else
		Con_SubPrintf(con, "^5%s^7: "COLOURYELLOW"%s\n", jcl->localalias, msg);
}
void JCL_AttentionMessage(jclient_t *jcl, char *to, char *msg)
{
	char fullto[256];
	buddy_t *b;
	bresource_t *br;
	xmltree_t *m;
	char *s;

	JCL_FindBuddy(jcl, to, &b, &br);
	if (!br)
		br = b->defaultresource;
	if (!br)
		br = b->resources;
	if (!br)
	{
		Con_SubPrintf(b->name, "User is not online\n");
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
			Con_SubPrintf(b->name, "*^5%s^7"COLOURYELLOW"%s\n", ((!strcmp(jcl->localalias, ">>"))?"me":jcl->localalias), msg+3);
		else
			Con_SubPrintf(b->name, "^5%s^7: "COLOURYELLOW"%s\n", jcl->localalias, msg);
	}
}

void JCL_ToJID(jclient_t *jcl, char *in, char *out, int outsize)
{
	//decompose links first
	if (in[0] == '^' && in[1] == '[')
	{
		char *sl;
		char *le;
		sl = in+2;
		sl = strchr(in, '\\');
		if (sl)
		{
			le = strstr(sl, "^]");
			if (le)
			{
				*le = 0;
				JCL_Info_ValueForKey(in, "xmpp", out, outsize);
				*le = '^';
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
				if (b->defaultresource)
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
	JCL_FindBuddy(jcl, roomserverhandle, &b, &r);
	b->chatroom = true;
	Q_snprintf(caps, sizeof(caps), "<c xmlns='http://jabber.org/protocol/caps' hash='sha-1' node='http://fteqw.com/ftexmppplugin' ver='%s'/>", buildcapshash());
	JCL_AddClientMessagef(jcl, "<presence to='%s'><x xmlns='http://jabber.org/protocol/muc'><password>%s</password></x>%s</presence>", roomserverhandle, password, caps);
}

#ifdef FILETRANSFERS
qboolean JCL_FT_IBBChunked(jclient_t *jcl, xmltree_t *x, struct iq_s *iq)
{
	char *from = XML_GetParameter(x, "from", "");
	struct ft_s *ft = iq->usrptr, **link, *v;
	for (link = &jcl->ft; (v=*link); link = &(*link)->next)
	{
		if (v == ft)
		{
			//its still valid
			if (x)
			{
				char rawbuf[4096];
				int sz;
				sz = pFS_Read(ft->file, rawbuf, ft->blocksize);
				Base64_Add(rawbuf, sz);
				Base64_Finish();

				if (sz && strlen(base64))
				{
					x = XML_CreateNode(NULL, "data", "http://jabber.org/protocol/ibb", base64);
					XML_AddParameteri(x, "seq", ft->seq++);
					XML_AddParameter(x, "sid", ft->sid);
					JCL_SendIQNode(jcl, JCL_FT_IBBChunked, "set", from, x, true)->usrptr = ft;
					return true;
				}
				//else eof
			}

			//errored or ended

			if (x)
				Con_Printf("Transfer of %s to %s completed\n", ft->fname, ft->with);
			else
				Con_Printf("%s aborted %s\n", ft->with, ft->fname);
			x = XML_CreateNode(NULL, "close", "http://jabber.org/protocol/ibb", "");
			XML_AddParameter(x, "sid", ft->sid);
			JCL_SendIQNode(jcl, NULL, "set", from, x, true)->usrptr = ft;

			//errored
			pFS_Close(ft->file);
			*link = ft->next;
			free(ft);
			return true;
		}
	}
	return false;
}
qboolean JCL_FT_IBBBegun(jclient_t *jcl, xmltree_t *x, struct iq_s *iq)
{
	struct ft_s *ft = iq->usrptr, **link, *v;
	for (link = &jcl->ft; (v=*link); link = &(*link)->next)
	{
		if (v == ft)
		{
			//its still valid
			if (!x)
			{
				Con_Printf("%s aborted %s\n", ft->with, ft->fname);
				//errored
				pFS_Close(ft->file);
				*link = ft->next;
				free(ft);
			}
			else
			{
				ft->begun = true;
				ft->method = FT_IBB;
				JCL_FT_IBBChunked(jcl, x, iq);
			}
			return true;
		}
	}
	return false;
}
qboolean JCL_FT_OfferAcked(jclient_t *jcl, xmltree_t *x, struct iq_s *iq)
{
	struct ft_s *ft = iq->usrptr, **link, *v;

	for (link = &jcl->ft; (v=*link); link = &(*link)->next)
	{
		if (v == ft)
		{
			//its still valid
			if (!x)
			{
				Con_Printf("%s doesn't want %s\n", ft->with, ft->fname);
				//errored
				pFS_Close(ft->file);
				*link = ft->next;
				free(ft);
			}
			else
			{
				xmltree_t *xo;
				Con_Printf("%s accepted %s\n", ft->with, ft->fname);
				xo = XML_CreateNode(NULL, "open", "http://jabber.org/protocol/ibb", "");
				XML_AddParameter(xo, "sid", ft->sid);
				XML_AddParameteri(xo, "block-size", ft->blocksize);
				//XML_AddParameter(xo, "stanza", "iq");

				JCL_SendIQNode(jcl, JCL_FT_IBBBegun, "set", XML_GetParameter(x, "from", ""), xo, true)->usrptr = ft;
			}
			return true;
		}
	}
	return false;
}
#endif

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

			if (jcl)
			{
				Con_TrySubPrint(console, "You are already connected\nPlease /quit first\n");
				return;
			}
			jcl = jclients[accid] = JCL_Connect(accid, arg[3], !strncmp(arg[0]+1, "tls", 3), arg[1], arg[2]);
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
				Con_Printf("eg for gmail: ^[/" COMMANDPREFIX " /connect myusername@gmail.com^] (using oauth2)\n");
				Con_Printf("eg for gmail: ^[/" COMMANDPREFIX " /connect myusername@gmail.com mypassword talk.google.com^] (warning: password will be saved locally in plain text)\n");
//				Con_Printf("eg for facebook: ^[/" COMMANDPREFIX " /connect myusername@chat.facebook.com mypassword chat.facebook.com^]\n");
//				Con_Printf("eg for msn: ^[/" COMMANDPREFIX " /connect myusername@messanger.live.com mypassword^]\n");
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
		else if (!strcmp(arg[0]+1, "quit"))
		{
			//disconnect from the xmpp server.
			JCL_CloseConnection(jcl, false);
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

			JCL_SendMessage(jcl, jcl->defaultdest, msg);
		}
		else if (!strcmp(arg[0]+1, "friend")) 
		{
			//FIXME: validate the name. deal with xml markup.

			//can also rename. We should probably read back the groups for the update.
			JCL_SendIQf(jcl, NULL, "set", NULL, "<query xmlns='jabber:iq:roster'><item jid='%s' name='%s'></item></query>", arg[1], arg[2]);

			//start looking for em
			JCL_AddClientMessagef(jcl, "<presence to='%s' type='subscribe'/>", arg[1]);

			//let em see us
			if (jcl->preapproval)
				JCL_AddClientMessagef(jcl, "<presence to='%s' type='subscribed'/>", arg[1]);
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
			JCL_ToJID(jcl, *arg[1]?arg[1]:console, nname, sizeof(nname));
			JCL_Join(jcl, nname, NULL, true, ICEP_QWCLIENT);
		}
		else if (!strcmp(arg[0]+1, "invite")) 
		{
			JCL_ToJID(jcl, *arg[1]?arg[1]:console, nname, sizeof(nname));
			JCL_Join(jcl, nname, NULL, true, ICEP_QWSERVER);
		}
		else if (!strcmp(arg[0]+1, "voice") || !strcmp(arg[0]+1, "call")) 
		{
			JCL_ToJID(jcl, *arg[1]?arg[1]:console, nname, sizeof(nname));
			JCL_Join(jcl, nname, NULL, true, ICEP_VOICE);
		}
		else if (!strcmp(arg[0]+1, "kick")) 
		{
			JCL_ToJID(jcl, *arg[1]?arg[1]:console, nname, sizeof(nname));
			JCL_Join(jcl, nname, NULL, false, ICEP_INVALID);
		}
		else if (!strcmp(arg[0]+1, "kick")) 
		{
			JCL_ToJID(jcl, *arg[1]?arg[1]:console, nname, sizeof(nname));
			JCL_Join(jcl, nname, NULL, false, ICEP_INVALID);
		}
#endif
#ifdef FILETRANSFERS
		else if (!strcmp(arg[0]+1, "sendfile"))
		{
			xmltree_t *xfile, *xsi, *c;
			char *fname = arg[1];
			//file transfer offer
			struct ft_s *ft;

			if (!*fname || strchr(fname, '*'))
			{
				Con_SubPrintf(console, "XMPP: /sendfile FILENAME [to]\n");
				return;
			}
			else
			{
				JCL_ToJID(jcl, *arg[2]?arg[2]:console, nname, sizeof(nname));

				Con_SubPrintf(console, "Offering %s to %s.\n", fname, nname);

				ft = malloc(sizeof(*ft));
				memset(ft, 0, sizeof(*ft));
				ft->next = jcl->ft;
				jcl->ft = ft;

				ft->transmitting = true;
				ft->blocksize = 4096;
				Q_strlcpy(ft->fname, fname, sizeof(ft->fname));
				Q_snprintf(ft->sid, sizeof(ft->sid), "%x%s", rand(), ft->fname);
				Q_strlcpy(ft->md5hash, "", sizeof(ft->md5hash));
				ft->size = pFS_Open(ft->fname, &ft->file, 1);
				ft->with = strdup(nname);
				ft->method = FT_IBB;
				ft->begun = false;

				//generate an offer.
				xsi = XML_CreateNode(NULL, "si", "http://jabber.org/protocol/si", "");
				XML_AddParameter(xsi, "profile", "http://jabber.org/protocol/si/profile/file-transfer");
				XML_AddParameter(xsi, "id", ft->sid);
				//XML_AddParameter(xsi, "mime-type", "text/plain");
				xfile = XML_CreateNode(xsi, "file", "http://jabber.org/protocol/si/profile/file-transfer", "");	//I don't really get the point of this.
				XML_AddParameter(xfile, "name", ft->fname);
				XML_AddParameteri(xfile, "size", ft->size);
				c = XML_CreateNode(xsi, "feature", "http://jabber.org/protocol/feature-neg", "");
				c = XML_CreateNode(c, "x", "jabber:x:data", "");
				XML_AddParameter(c, "type", "form");
				c = XML_CreateNode(c, "field", "", "");
				XML_AddParameter(c, "var", "stream-method");
				XML_AddParameter(c, "type", "listsingle");
				XML_CreateNode(XML_CreateNode(c, "option", "", ""), "value", "", "http://jabber.org/protocol/ibb");
	//			XML_CreateNode(XML_CreateNode(c, "option", "", ""), "value", "", "http://jabber.org/protocol/bytestreams");

				JCL_SendIQNode(jcl, JCL_FT_OfferAcked, "set", nname, xsi, true)->usrptr = ft;
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
			JCL_FindBuddy(jcl, roomserverhandle, &b, &r);
			b->chatroom = true;
			JCL_AddClientMessagef(jcl, "<presence to='%s' type='unavailable'/>", roomserverhandle);
			JCL_ForgetBuddy(jcl, b, NULL);
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
			JCL_ToJID(jcl, *arg[1]?arg[1]:console, nname, sizeof(nname));
			JCL_AttentionMessage(jcl, nname, msgtab[rand()%(sizeof(msgtab)/sizeof(msgtab[0]))]);
		}
		else if (!strcmp(arg[0]+1, "poke")) 
		{
			JCL_ToJID(jcl, *arg[1]?arg[1]:console, nname, sizeof(nname));
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
		if (jcl)
		{
			msg = imsg;

			if (!*msg)
			{
				if (!*console)
				{
					JCL_PrintBuddyList(console, jcl, false);
					//Con_TrySubPrint(console, "For help, type \"^[/" COMMANDPREFIX " /help^]\"\n");
				}
			}
			else
			{
				JCL_ToJID(jcl, *console?console:jcl->defaultdest, nname, sizeof(nname));
				JCL_SendMessage(jcl, nname, msg);
			}
		}
		else
		{
			Con_TrySubPrint(console, "Not connected. For help, type \"^[/" COMMANDPREFIX " /help^]\"\n");
		}
	}
}
