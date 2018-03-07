#ifndef NETINC_INCLUDED
#define NETINC_INCLUDED


#ifndef HAVE_PACKET

#ifndef _XBOX
	struct sockaddr
	{
		short  sa_family;
	};

	#define ntohs BigShort
	#define htons BigShort
	#define htonl BigLong
	#define ntohl BigLong
#endif
/*	struct sockaddr_in
	{
		short  sin_family;
		unsigned short	sin_port;
		in_addr sin_addr;
	};*/
	#define AF_UNSPEC 0
//	#define AF_INET 1

	/*NaCl engines cannot host servers. Regular FTE servers can use the same listening tcpconnect socket to host a websocket connection*/

	#define AF_WEBSOCK 342

	struct sockaddr_websocket
	{
		short  sws_family;
		char url[64];
	};



#elif defined(_WIN32)
	#ifdef _XBOX
		#include <xtl.h>
		#include <WinSockX.h>
	#else
		#ifdef _MSC_VER
			#define USEIPX
		#endif
		#define WIN32_LEAN_AND_MEAN
		#include <windows.h>
		#include <winsock2.h>
		#ifdef USEIPX
			#include "wsipx.h"
		#endif
		#include <ws2tcpip.h>
	#endif
	#include <errno.h>
	#if !defined(IPPROTO_IPV6) && !defined(_XBOX)
		/*for msvc6*/
		#define	IPPROTO_IPV6 41
		
		#ifndef EAI_NONAME		
			#define EAI_NONAME 8
		#endif

		struct ip6_scope_id
		{
			union
			{
				struct
				{
					u_long  Zone : 28;
					u_long  Level : 4;
				};
				u_long  Value;
			};
		};

		#if !defined(in_addr6)
			struct in6_addr
			{
				u_char	s6_addr[16];	/* IPv6 address */
			};
			#define sockaddr_in6 sockaddr_in6_fixed /*earlier versions of msvc have a sockaddr_in6 which does _not_ match windows, so this *must* be redefined for any non-final msvc releases or it won't work at all*/
			typedef struct sockaddr_in6
			{
				short  sin6_family;
				u_short  sin6_port;
				u_long  sin6_flowinfo;
				struct in6_addr  sin6_addr;
				union
				{
					u_long  sin6_scope_id;
					struct ip6_scope_id  sin6_scope_struct; 
				};
			};
			struct addrinfo
			{
			  int ai_flags;
			  int ai_family;
			  int ai_socktype;
			  int ai_protocol;
			  size_t ai_addrlen;
			  char* ai_canonname;
			  struct sockaddr * ai_addr;
			  struct addrinfo * ai_next;
			};
		#endif
	#endif
	#ifndef IPV6_V6ONLY
		#define IPV6_V6ONLY 27
	#endif

	#define HAVE_IPV4
	#ifdef IPPROTO_IPV6
			#define HAVE_IPV6
	#endif
#else
	#include <sys/time.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <netdb.h>
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

	#if defined(__MORPHOS__) && !defined(ixemul)
		#define closesocket CloseSocket
		#define ioctlsocket IoctlSocket
	#else
		#define closesocket close
		#define ioctlsocket ioctl
	#endif

	#if defined(AF_INET6) && !defined(IPPROTO_IPV6)
		#define IPPROTO_IPV6 IPPROTO_IPV6	//fte often just checks to see if IPPROTO_IPV6 is defined or not, which doesn't work if its an enum value or somesuch...
	#endif

	#if defined(AF_INET)
		#define HAVE_IPV4
	#endif
	#ifdef IPPROTO_IPV6
		#define HAVE_IPV6
	#endif

	#define SOCKET int
#endif

#if defined(_WIN32)
	#define neterrno() WSAGetLastError()
	//this madness is because winsock defines its own errors instead of using system error codes.
	//*AND* microsoft then went and defined names for all the unix ones too... with different values! oh the insanity of it all!
	#define NET_EWOULDBLOCK		WSAEWOULDBLOCK
	#define NET_EINPROGRESS		WSAEINPROGRESS
	#define NET_EMSGSIZE		WSAEMSGSIZE
	#define NET_ECONNRESET		WSAECONNRESET
	#define NET_ECONNABORTED	WSAECONNABORTED
	#define NET_ECONNREFUSED	WSAECONNREFUSED
	#define NET_ETIMEDOUT		WSAETIMEDOUT
	#define NET_ENOTCONN		WSAENOTCONN
	#define NET_EACCES			WSAEACCES
	#define NET_EADDRNOTAVAIL	WSAEADDRNOTAVAIL
	#define NET_ENETUNREACH		WSAENETUNREACH
	#define NET_EAFNOSUPPORT	WSAEAFNOSUPPORT
#elif defined(__MORPHOS__) && !defined(ixemul)
	#define neterrno() Errno()
#else
	#define neterrno() errno
#endif

#ifndef NET_EWOULDBLOCK
	//assume unix codes instead, so our prefix still works.
	#define NET_EWOULDBLOCK		EWOULDBLOCK
	#define NET_EINPROGRESS		EINPROGRESS
	#define NET_EMSGSIZE		EMSGSIZE
	#define NET_ECONNRESET		ECONNRESET
	#define NET_ECONNABORTED	ECONNABORTED
	#define NET_ECONNREFUSED	ECONNREFUSED
	#define NET_ETIMEDOUT		ETIMEDOUT
	#define NET_ENOTCONN		ENOTCONN
	#define NET_EACCES			EACCES
	#define NET_EADDRNOTAVAIL	EADDRNOTAVAIL
	#define NET_ENETUNREACH		ENETUNREACH
	#define NET_EAFNOSUPPORT	EAFNOSUPPORT
#endif

#ifndef INVALID_SOCKET
	#define INVALID_SOCKET -1
#endif

#ifndef INADDR_LOOPBACK
	#define INADDR_LOOPBACK 0x7f000001
#endif

#if defined(FTE_TARGET_WEB)
	#undef IPPROTO_IPV6
#endif

#if 1//def SUPPORT_ICE
struct icecandinfo_s
{
	char candidateid[64];
	char addr[64];		//v4/v6/fqdn. fqdn should prefer ipv6
	int port;
	int transport;		//0=udp. other values not supported
	int foundation;		//to figure out...
	int component;		//1-based. allows rtp+rtcp in a single ICE... we only support one.
	int priority;		//some random value...
	enum
	{
		ICE_HOST=0,
		ICE_SRFLX=1,	//Server Reflexive (from stun, etc)
		ICE_PRFLX=2,	//Peer Reflexive 
		ICE_RELAY=3,
	} type;				//says what sort of proxy is used.
	char reladdr[64];	//when proxied, this is our local info
	int relport;
	int generation;		//for ice restarts. starts at 0.
	int network;		//which network device this comes from.
};
enum iceproto_e
{
	ICEP_INVALID,	//not allowed..
	ICEP_QWSERVER,	//we're server side
	ICEP_QWCLIENT,	//we're client side
	ICEP_VOICE,		//speex. requires client.
	ICEP_VIDEO		//err... REALLY?!?!?
};
enum icemode_e
{
	ICEM_RAW,	//not actually interactive beyond a simple handshake.
	ICEM_ICE		//rfc5245. meant to be able to holepunch, but not implemented properly yet.
};
enum icestate_e
{
	ICE_INACTIVE,	//idle.
	ICE_FAILED,
	ICE_CONNECTING,	//exchanging pings.
	ICE_CONNECTED	//media is flowing, supposedly. sending keepalives.
};
struct icestate_s;
#define ICE_API_CURRENT "Internet Connectivity Establishment 0.0"
typedef struct
{
	struct icestate_s *(QDECL *ICE_Create)(void *module, const char *conname, const char *peername, enum icemode_e mode, enum iceproto_e proto);	//doesn't start pinging anything.
	qboolean (QDECL *ICE_Set)(struct icestate_s *con, const char *prop, const char *value);
	qboolean (QDECL *ICE_Get)(struct icestate_s *con, const char *prop, char *value, size_t valuesize);
	struct icecandinfo_s *(QDECL *ICE_GetLCandidateInfo)(struct icestate_s *con);		//retrieves candidates that need reporting to the peer.
	void (QDECL *ICE_AddRCandidateInfo)(struct icestate_s *con, struct icecandinfo_s *cand);		//stuff that came from the peer.
	void (QDECL *ICE_Close)(struct icestate_s *con);	//bye then.
	void (QDECL *ICE_CloseModule)(void *module);	//closes all unclosed connections, with warning.
//	struct icestate_s *(QDECL *ICE_Find)(void *module, const char *conname);
	qboolean (QDECL *ICE_GetLCandidateSDP)(struct icestate_s *con, char *out, size_t valuesize);		//retrieves candidates that need reporting to the peer.
} icefuncs_t;
extern icefuncs_t iceapi;
#endif

//address flags
#define ADDR_NATPMP		(1u<<0)
#define ADDR_UPNPIGP	(1u<<1)
#define ADDR_REFLEX		(1u<<2)	//as reported by some external server.

#define FTENET_ADDRTYPES 2
typedef struct ftenet_generic_connection_s {
	char name[MAX_QPATH];

	int (*GetLocalAddresses)(struct ftenet_generic_connection_s *con, unsigned int *adrflags, netadr_t *addresses, int maxaddresses);
	qboolean (*ChangeLocalAddress)(struct ftenet_generic_connection_s *con, netadr_t *newadr);
	qboolean (*GetPacket)(struct ftenet_generic_connection_s *con);
	neterr_t (*SendPacket)(struct ftenet_generic_connection_s *con, int length, const void *data, netadr_t *to);
	void (*Close)(struct ftenet_generic_connection_s *con);
#ifdef HAVE_PACKET
	int (*SetFDSets) (struct ftenet_generic_connection_s *con, fd_set *readfdset, fd_set *writefdset);	/*set for connections which have multiple sockets (ie: listening tcp connections)*/
#endif
	void (*PrintStatus)(struct ftenet_generic_connection_s *con);

	netadrtype_t addrtype[FTENET_ADDRTYPES];
	qboolean islisten;
#ifdef HAVE_PACKET
	SOCKET thesocket;
#else
	int thesocket;
#endif
} ftenet_generic_connection_t;

#ifdef HAVE_DTLS
typedef struct dtlsfuncs_s
{
	void *(*CreateContext)(const char *remotehost, void *cbctx, neterr_t(*push)(void *cbctx, const qbyte *data, size_t datasize), qboolean isserver);	//if remotehost is null then their certificate will not be validated.
	void (*DestroyContext)(void *ctx);
	neterr_t (*Transmit)(void *ctx, const qbyte *data, size_t datasize);
	neterr_t (*Received)(void *ctx, qbyte *data, size_t datasize);
	neterr_t (*Timeouts)(void *ctx);
} dtlsfuncs_t;
const dtlsfuncs_t *DTLS_InitServer(void);
const dtlsfuncs_t *DTLS_InitClient(void);
#ifdef HAVE_WINSSPI
	const dtlsfuncs_t *SSPI_DTLS_InitServer(void);	//returns NULL if there's no cert available.
	const dtlsfuncs_t *SSPI_DTLS_InitClient(void);	//should always return something, if implemented.
#endif
#ifdef HAVE_GNUTLS
	const dtlsfuncs_t *GNUDTLS_InitServer(void);	//returns NULL if there's no cert available.
	const dtlsfuncs_t *GNUDTLS_InitClient(void);	//should always return something, if implemented.
#endif
#endif



#define MAX_CONNECTIONS 8
typedef struct ftenet_connections_s
{
	qboolean islisten;
	unsigned int packetsin;
	unsigned int packetsout;
	unsigned int bytesin;
	unsigned int bytesout;
	unsigned int timemark;
	float packetsinrate;
	float packetsoutrate;
	float bytesinrate;
	float bytesoutrate;
	ftenet_generic_connection_t *conn[MAX_CONNECTIONS];

#ifdef HAVE_DTLS
	struct dtlspeer_s *dtls;	//linked list. linked lists are shit, but at least it keeps pointers valid when things are resized.
	const dtlsfuncs_t *dtlsfuncs;
#endif

	struct ftenet_delayed_packet_s
	{
		unsigned int sendtime;	//in terms of Sys_Milliseconds()
		struct ftenet_delayed_packet_s *next;
		netadr_t dest;
		size_t cursize;
		qbyte data[1];
	} *delayed_packets;
} ftenet_connections_t;

void ICE_Tick(void);
qboolean ICE_WasStun(netsrc_t netsrc);
void QDECL ICE_AddLCandidateConn(ftenet_connections_t *col, netadr_t *addr, int type);
void QDECL ICE_AddLCandidateInfo(struct icestate_s *con, netadr_t *adr, int adrno, int type);

ftenet_connections_t *FTENET_CreateCollection(qboolean listen);
void FTENET_CloseCollection(ftenet_connections_t *col);
qboolean FTENET_AddToCollection(struct ftenet_connections_s *col, const char *name, const char *address, netadrtype_t addrtype, netproto_t addrprot);
int NET_EnumerateAddresses(ftenet_connections_t *collection, struct ftenet_generic_connection_s **con, unsigned int *adrflags, netadr_t *addresses, int maxaddresses);

vfsfile_t *FS_OpenSSL(const char *hostname, vfsfile_t *source, qboolean server);
int TLS_GetChannelBinding(vfsfile_t *stream, qbyte *data, size_t *datasize);	//datasize should be preinitialised to the max length allowed. -1 for not implemented. 0 for peer problems. 1 for success
#ifdef HAVE_PACKET
vfsfile_t *FS_OpenTCPSocket(SOCKET socket, qboolean conpending, const char *peername);	//conpending allows us to reject any writes until the connection has succeeded
#endif
vfsfile_t *FS_OpenTCP(const char *name, int defaultport);

#ifndef SOCK_CLOEXEC
#define SOCK_CLOEXEC 0
#endif

#endif //NETINC_INCLUDED
