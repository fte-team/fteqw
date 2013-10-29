#include "../plugin.h"

#include "xml.h"


//configuration

//#define NOICE	//if defined, we only do simple raw udp connections.
//#define FILETRANSFERS		//IBB only, speeds suck. autoaccept is forced on. no protection from mods stuffcmding sendfile commands. needs more extensive testing
#define QUAKECONNECT		//including quake ICE connections (depends upon jingle)
#define VOIP				//enables voice chat (depends upon jingle)
#define VOIP_LEGACY			//enables google-only voice chat compat. google have not kept up with the standardisation of jingle (aka: gingle).
//#define VOIP_LEGACY_ONLY	//disables jingle feature (advert+detection only)
#define JINGLE				//enables jingle signalling

#ifdef JINGLE
	#include "../../engine/common/netinc.h"
#else
	#undef VOIP
	#undef VOIP_LEGACY
#endif


#define JCL_BUILD "3"
#define DEFAULTDOMAIN ""
#define DEFAULTRESOURCE "Quake"
#define QUAKEMEDIATYPE "quake"
#define QUAKEMEDIAXMLNS "http://fteqw.com/protocol/quake"
#define DISCONODE		"http://fteqw.com/ftexmpp"	//some sort of client identifier
#define DEFAULTICEMODE ICEM_ICE



#define JCL_MAXMSGLEN 10000

//values are not on the wire or anything
#define CAP_QUERYING		(1<<0)	//we've sent a query and are waiting for the response.
#define CAP_QUERIED			(1<<1)	//feature capabilities are actually know.
#define CAP_QUERYFAILED		(1<<2)	//caps request failed due to bad hash or some such.
#define CAP_VOICE			(1<<3)	//supports voice

#define CAP_INVITE			(1<<4)	//supports game invites.
#define CAP_POKE			(1<<5)	//can be slapped.
#define CAP_SIFT			(1<<6)	//non-jingle file transfers

#define CAP_FUGOOG_VOICE	(1<<7)	//fuck you, google.
#define CAP_FUGOOG_SESSION	(1<<8)	//fuck you, google.

typedef struct bresource_s
{
	char bstatus[128];	//basic status
	char fstatus[128];	//full status
	char server[256];
	int servertype;	//0=none, 1=already a client, 2=joinable

	unsigned int caps;
	char *client_node;	//vendor name
	char *client_ver;	//cap hash value
	char *client_hash;	//cap hash method
	char *client_ext;	//deprecated. additionally queried

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
		char to[1];
	} *pendingiqs;

#ifdef JINGLE
	struct c2c_s
	{
		struct c2c_s *next;
		enum iceproto_e mediatype;
		enum icemode_e method;	//ICE_RAW or ICE_ICE. this is what the peer asked for. updated if we degrade it.
		qboolean accepted;	//connection is going
		qboolean creator;	//true if we're the creator.
		unsigned int peercaps;

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
		int sizedone;
		char *with;
		char md5hash[16];
		int privateid;
		char iqid[64];
		char sid[64];
		int blocksize;
		unsigned short seq;
		qhandle_t file;
		qhandle_t stream;
		qboolean begun;	//handshake
		qboolean eof;
		qboolean transmitting;	//we're offering
		qboolean allowed;	//if false, don't handshake the transfer

		enum
		{
			FT_IBB,			//in-band bytestreams
			FT_BYTESTREAM	//aka: relay
		} method;
	} *ft;
	int privateidseq;
#endif

	buddy_t *buddies;
} jclient_t;


#ifdef JINGLE
extern icefuncs_t *piceapi;
#endif


qboolean NET_DNSLookup_SRV(char *host, char *out, int outlen);

//xmpp functionality
struct iq_s *JCL_SendIQNode(jclient_t *jcl, qboolean (*callback) (jclient_t *jcl, xmltree_t *tree, struct iq_s *iq), char *iqtype, char *target, xmltree_t *node, qboolean destroynode);
void JCL_AddClientMessagef(jclient_t *jcl, char *fmt, ...);
qboolean JCL_FindBuddy(jclient_t *jcl, char *jid, buddy_t **buddy, bresource_t **bres);

//quake functionality
void JCL_GenLink(jclient_t *jcl, char *out, int outlen, char *action, char *context, char *contextres, char *sid, char *txtfmt, ...);
void Con_SubPrintf(char *subname, char *format, ...);

//jingle functions
void JCL_Join(jclient_t *jcl, char *target, char *sid, qboolean allow, int protocol);
void JCL_JingleTimeouts(jclient_t *jcl, qboolean killall);
//jingle iq message handlers
qboolean JCL_HandleGoogleSession(jclient_t *jcl, xmltree_t *tree, char *from, char *id);
qboolean JCL_ParseJingle(jclient_t *jcl, xmltree_t *tree, char *from, char *id);

void JCL_FT_AcceptFile(jclient_t *jcl, int fileid, qboolean accept);