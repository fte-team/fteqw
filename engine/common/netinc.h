
#ifndef NACL
#define HAVE_IPV4	//says we can set and receive AF_INET ipv4 udp packets.
#define HAVE_TCP	//says we can use tcp too (either ipv4 or ipv6)
#define HAVE_PACKET	//if we have the socket api at all...
#endif

#ifdef FTE_TARGET_WEB
#undef HAVE_PACKET	//no udp packet interface.
#endif

#ifdef NACL

	struct sockaddr
	{
		short  sa_family;
	};
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
	#ifdef _MSC_VER
		#define USEIPX
	#endif
	#define WIN32_LEAN_AND_MEAN
	#define byte winbyte
	#include <windows.h>
	#include <winsock2.h>
//	#include "winquake.h"
	#ifdef USEIPX
		#include "wsipx.h"
	#endif
	#include <ws2tcpip.h>
	#include <errno.h>
	#ifndef IPPROTO_IPV6
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

		#if !(_MSC_VER >= 1500)
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

	#ifdef EADDRNOTAVAIL
		#undef EADDRNOTAVAIL
	#endif
	#ifdef EAFNOSUPPORT
		#undef EAFNOSUPPORT
	#endif
	#ifdef ECONNABORTED
		#undef ECONNABORTED
	#endif
	#ifdef ECONNREFUSED
		#undef ECONNREFUSED
	#endif
	#ifdef ECONNREFUSED
		#undef ECONNREFUSED
	#endif
	#ifdef EMSGSIZE
		#undef EMSGSIZE
	#endif
	#ifdef EWOULDBLOCK
		#undef EWOULDBLOCK
	#endif
	#ifdef EACCES
		#undef EACCES
	#endif

	#define EWOULDBLOCK		WSAEWOULDBLOCK
	#define EMSGSIZE		WSAEMSGSIZE
	#define ECONNRESET		WSAECONNRESET
	#define ECONNABORTED	WSAECONNABORTED
	#define ECONNREFUSED	WSAECONNREFUSED
	#define EACCES			WSAEACCES
	#define EADDRNOTAVAIL	WSAEADDRNOTAVAIL
	#define EAFNOSUPPORT	WSAEAFNOSUPPORT

#else
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <netdb.h>
	#include <sys/ioctl.h>
	#include <sys/uio.h>
	#include <sys/time.h>
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
		#define IPPROTO_IPV6 IPPROTO_IPV6
	#endif

	#define SOCKET int
#endif

#if defined(_WIN32)
	#define qerrno WSAGetLastError()
#elif defined(__MORPHOS__) && !defined(ixemul)
	#define qerrno Errno()
#else
	#define qerrno errno
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
struct icecandidate_s
{
	struct icecandidate_s *next;
	char *candidateid;
	char *addr;		//v4/v6/fqdn. fqdn should prefer ipv6
	int port;
	int transport;		//0=udp. other values not supported
	int foundation;		//to figure out...
	int component;		//1-based. allows rtp+rtcp in a single ICE... we only support one.
	int priority;		//some random value...
	enum
	{
		ICE_HOST=0,
		ICE_SRFLX=1,
		ICE_PRFLX=2,
		ICE_RELAY=3,
	} type;				//says what sort of proxy is used.
	char *reladdr;		//when proxied, this is our local info
	int relport;
	int generation;		//for ice restarts. starts at 0.
	int network;		//which network device this comes from.

	qboolean dirty;
};
struct icestate_s
{
	struct icestate_s *next;
	void *module;

	int netsrc;
	enum icemode_e
	{
		ICE_RAW,	//not actually interactive beyond a simple handshake.
		ICE_ICE		//rfc5245. meant to be able to holepunch, but not implemented properly yet.
	} mode;
	char *conname;		//internal id.
	char *friendlyname;	//who you're talking to.
	char *stunserver;//where to get our public ip from.
	int stunport;

	struct icecandidate_s *lc;
	char *lpwd;
	char *lfrag;

	struct icecandidate_s *rc;
	char *rpwd;
	char *rfrag;
};
struct icestate_s *QDECL ICE_Create(void *module, char *conname, char *peername, enum icemode_e mode);	//doesn't start pinging anything.
struct icestate_s *QDECL ICE_Find(void *module, char *conname);
void QDECL ICE_Begin(struct icestate_s *con, char *stunip, int stunport);	//begins sending stun packets and stuff as required. data flows automagically. caller should poll ICE_GetLCandidateInfo periodically to pick up new candidates that need to be reported to the peer.
struct icecandidate_s *QDECL ICE_GetLCandidateInfo(struct icestate_s *con);		//retrieves candidates that need reporting to the peer.
void QDECL ICE_AddRCandidateInfo(struct icestate_s *con, struct icecandidate_s *cand);		//stuff that came from the peer.
void QDECL ICE_Close(struct icestate_s *con);	//bye then.
void QDECL ICE_CloseModule(void *module);	//closes all unclosed connections, with warning.
#endif