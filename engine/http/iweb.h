#ifndef IWEB_H__
#define IWEB_H__

#ifdef WEBSERVER

#ifdef WEBSVONLY

#include "quakedef.h"

#define Con_TPrintf IWebPrintf
#define IWebPrintf printf
#define com_gamedir "."	//current dir.


#define IWebMalloc(x) calloc(x, 1)
#define IWebRealloc(x, y) realloc(x, y)
#define IWebFree free
#endif

#define IWEBACC_READ	1
#define IWEBACC_WRITE	2
#define IWEBACC_FULL	4
struct sockaddr_in;
struct sockaddr;
struct sockaddr_qstorage;
int NetadrToSockadr (netadr_t *a, struct sockaddr_qstorage *s);

qboolean SV_AllowDownload (const char *name);


typedef qboolean iwboolean;

//it's not allowed to error.
#ifndef WEBSVONLY
void VARGS IWebDPrintf(char *fmt, ...) LIKEPRINTF(1);
void VARGS IWebPrintf(char *fmt, ...) LIKEPRINTF(1);
void VARGS IWebWarnPrintf(char *fmt, ...) LIKEPRINTF(1);
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


//char *COM_ParseOut (const char *data, char *out, int outlen);
//struct searchpath_s;
//void COM_EnumerateFiles (const char *match, int (*func)(const char *, int, void *, struct searchpath_s *), void *parm);


char *Q_strcpyline(char *out, const char *in, int maxlen);




iwboolean	FTP_StringToAdr (const char *s, qbyte ip[4], qbyte port[2]);

//server tick/control functions
iwboolean FTP_ServerRun(iwboolean ftpserverwanted, int port);
qboolean HTTP_ServerPoll(qboolean httpserverwanted, int port);

//server interface called from main server routines.
void IWebInit(void);
void IWebRun(void);
void IWebShutdown(void);
/*
qboolean FTP_Client_Command (char *cmd, void (*NotifyFunction)(vfsfile_t *file, char *localfile, qboolean sucess));
void IRC_Command(char *imsg);
void FTP_ClientThink (void);
void IRC_Frame(void);
*/
qboolean SV_POP3(qboolean activewanted);
qboolean SV_SMTP(qboolean activewanted);

#endif


#if 1
struct dl_download
{
	/*not used by anything in the download itself, useful for context*/
	unsigned int user_num;
	float user_float;
	void *user_ctx;
	int user_sequence;

	qboolean isquery;	//will not be displayed in the download/progress bar stuff.

#ifndef SERVERONLY
	qdownload_t qdownload;
#endif

	/*stream config*/
	char *url;	/*original url*/
	char redir[MAX_OSPATH];	/*current redirected url*/
	char localname[MAX_OSPATH]; /*leave empty for a temp file*/
	struct vfsfile_s *file;	/*downloaded to, if not already set when starting will open localname or a temp file*/

	char postmimetype[64];
	char *postdata;			/*if set, this is a post and not a get*/
	size_t postlen;

	/*stream status*/
	enum
	{
		DL_PENDING,		/*not started*/
		DL_FAILED,		/*gave up*/
		DL_RESOLVING,	/*resolving the host*/
		DL_QUERY,		/*sent query, waiting for response*/
		DL_ACTIVE,		/*receiving data*/
		DL_FINISHED		/*its complete*/
	} status;
	unsigned int	replycode;
	size_t			totalsize;	/*max size (can be 0 for unknown)*/
	size_t			completed; /*ammount correctly received so far*/

	size_t			sizelimit;

	/*internals*/
#ifdef MULTITHREAD
	qboolean threadenable;
	void *threadctx;
#endif
	void *ctx;	/*internal context, depending on http/ftp/etc protocol*/
	void (*abort) (struct dl_download *); /*cleans up the internal context*/
	qboolean (*poll) (struct dl_download *);

	/*not used internally by the backend, but used by HTTP_CL_Get/thread wrapper*/
	struct dl_download *next;
	qboolean (*notifystarted) (struct dl_download *dl, char *mimetype);	//mime can be null for some protocols, read dl->totalsize for size. false if the mime just isn't acceptable.
	void (*notifycomplete) (struct dl_download *dl);
};

vfsfile_t *VFSPIPE_Open(void);
void HTTP_CL_Think(void);
void HTTP_CL_Terminate(void);	//kills all active downloads
unsigned int HTTP_CL_GetActiveDownloads(void);
struct dl_download *HTTP_CL_Get(const char *url, const char *localfile, void (*NotifyFunction)(struct dl_download *dl));
struct dl_download *HTTP_CL_Put(const char *url, const char *mime, const char *data, size_t datalen, void (*NotifyFunction)(struct dl_download *dl));

struct dl_download *DL_Create(const char *url);
qboolean DL_CreateThread(struct dl_download *dl, vfsfile_t *file, void (*NotifyFunction)(struct dl_download *dl));
void DL_Close(struct dl_download *dl);
void DL_DeThread(void);
#endif

#endif
