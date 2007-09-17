#ifdef WEBSERVER

#ifndef IWEB_H__
#define IWEB_H__


#ifdef WEBSVONLY

typedef unsigned char qbyte;
typedef enum {false, true} qboolean;
typedef enum {NA_INVALID, NA_LOOPBACK, NA_IP, NA_IPX, NA_BROADCAST_IP, NA_BROADCAST_IPX} netadrtype_t;
typedef struct
{
	netadrtype_t	type;

	qbyte	ip[4];
	qbyte	ipx[10];

	unsigned short	port;
} netadr_t;
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>

#ifdef _WIN32
#include <winsock.h>
#endif


#define PORT_ANY 0
void Sys_Error(char *fmt, ...);
int COM_CheckParm(char *parm);
int com_argc;
char **com_argv;

#define Con_TPrintf IWebPrintf
#define TL_NETBINDINTERFACE			"binding to %s"
#define TL_CONNECTIONLOSTORABORTED	"connection lost or aborted"
#define TL_NETGETPACKETERROR		"get packet error"

#define IWebPrintf printf
#define com_gamedir "."	//current dir.


#define IWebMalloc(x) calloc(x, 1)
#define IWebRealloc(x, y) realloc(x, y)
#define IWebFree free

#define MAX_OSPATH 1024

#else

#ifndef QUAKEDEF_H__

#include "quakedef.h"

#else
//#include <netinet/in.h>
#endif
#endif

#ifdef _WIN32
#include "winquake.h"
#endif

#define IWEBACC_READ	1
#define IWEBACC_WRITE	2
#define IWEBACC_FULL	4
struct sockaddr_in;
struct sockaddr;
struct sockaddr_qstorage;
int NetadrToSockadr (netadr_t *a, struct sockaddr_qstorage *s);

qboolean SV_AllowDownload (char *name);


typedef qboolean iwboolean;

#ifndef _WIN32
#define INVALID_SOCKET ~0
#endif

//it's not allowed to error.
#ifndef WEBSVONLY
void VARGS IWebDPrintf(char *fmt, ...);
void VARGS IWebPrintf(char *fmt, ...);
void VARGS IWebWarnPrintf(char *fmt, ...);
#endif

typedef struct {
	float gentime;	//useful for generating a new file (if too old, removes reference)
	int references;	//freed if 0
	char *data;
	int len;
} IWeb_FileGen_t;

#ifndef WEBSVONLY
void *IWebMalloc(int size);
void *IWebRealloc(void *old, int size);
void IWebFree(void *mem);
#define IWebFree	Z_Free
#endif

int IWebAuthorize(char *name, char *password);
iwboolean IWebAllowUpLoad(char *fname, char *uname);

vfsfile_t *IWebGenerateFile(char *name, char *content, int contentlength);



char *COM_ParseOut (char *data, char *out, int outlen);
void COM_EnumerateFiles (char *match, int (*func)(char *, int, void *), void *parm);


char *Q_strcpyline(char *out, char *in, int maxlen);




iwboolean	FTP_StringToAdr (const char *s, qbyte ip[4], qbyte port[2]);

//server tick/control functions
iwboolean FTP_ServerRun(iwboolean ftpserverwanted, int port);
qboolean HTTP_ServerPoll(qboolean httpserverwanted, int port);

void HTTP_CL_Think(void);
qboolean HTTP_CL_Get(char *url, char *localfile, void (*NotifyFunction)(char *localfile, qboolean sucess));

//server interface called from main server routines.
void IWebInit(void);
void IWebRun(void);
void IWebShutdown(void);

qboolean FTP_Client_Command (char *cmd, void (*NotifyFunction)(char *localfile, qboolean sucess));
void IRC_Command(char *imsg);
void FTP_ClientThink (void);
void IRC_Frame(void);

qboolean SV_POP3(qboolean activewanted);
qboolean SV_SMTP(qboolean activewanted);

#endif

#endif
