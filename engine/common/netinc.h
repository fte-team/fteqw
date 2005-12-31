
#ifdef _WIN32

	#define EWOULDBLOCK	WSAEWOULDBLOCK
	#define EMSGSIZE	WSAEMSGSIZE
	#define ECONNRESET	WSAECONNRESET
	#define ECONNABORTED	WSAECONNABORTED
	#define ECONNREFUSED	WSAECONNREFUSED
	#define EADDRNOTAVAIL	WSAEADDRNOTAVAIL
	#define EAFNOSUPPORT	WSAEAFNOSUPPORT

	#ifdef _MSC_VER
		#define USEIPX
	#endif
	#include "winquake.h"
	#ifdef USEIPX
		#include "wsipx.h"
	#endif
	#ifdef IPPROTO_IPV6
		#include <ws2tcpip.h>
	#endif
#else
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

