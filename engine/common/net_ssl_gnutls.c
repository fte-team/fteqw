//This file should be easily portable.
//The biggest strength of this plugin system is that ALL interactions are performed via
//named functions, this makes it *really* easy to port plugins from one engine to annother.

#include "quakedef.h"
 
#ifdef HAVE_GNUTLS

#if 1//defined(_WIN32) && !defined(MINGW)

#ifdef _MSC_VER
#if SIZE_MAX == ULONG_MAX
#define ssize_t long
#else
#define ssize_t int
#endif
#endif

//lets rip stuff out of the header and supply a seperate dll.
//gnutls is huge.
//also this helps get around the whole msvc/mingw thing.

struct DSTRUCT;
typedef struct DSTRUCT* gnutls_certificate_credentials;
typedef gnutls_certificate_credentials gnutls_certificate_client_credentials;
typedef struct DSTRUCT* gnutls_anon_client_credentials;
struct gnutls_session_int;
typedef struct gnutls_session_int* gnutls_session_t;
typedef void * gnutls_transport_ptr_t;
struct gnutls_x509_crt_int;
typedef struct gnutls_x509_crt_int *gnutls_x509_crt_t;
typedef struct
{
	unsigned char *data;
	unsigned int size;
} gnutls_datum_t;

typedef enum gnutls_kx_algorithm { GNUTLS_KX_RSA=1, GNUTLS_KX_DHE_DSS,
	GNUTLS_KX_DHE_RSA, GNUTLS_KX_ANON_DH, GNUTLS_KX_SRP,
	GNUTLS_KX_RSA_EXPORT, GNUTLS_KX_SRP_RSA, GNUTLS_KX_SRP_DSS
} gnutls_kx_algorithm;
typedef enum {
    GNUTLS_CRT_UNKNOWN = 0,
    GNUTLS_CRT_X509 = 1,
    GNUTLS_CRT_OPENPGP = 2,
    GNUTLS_CRT_RAW = 3
} gnutls_certificate_type_t;
typedef enum {
    GNUTLS_X509_FMT_DER = 0,
    GNUTLS_X509_FMT_PEM = 1
} gnutls_x509_crt_fmt_t;
typedef enum
{
	GNUTLS_CERT_INVALID = 1<<1,
	GNUTLS_CERT_REVOKED = 1<<5,
	GNUTLS_CERT_SIGNER_NOT_FOUND = 1<<6,
	GNUTLS_CERT_SIGNER_NOT_CA = 1<<7,
	GNUTLS_CERT_INSECURE_ALGORITHM = 1<<8,
	GNUTLS_CERT_NOT_ACTIVATED = 1<<9,
	GNUTLS_CERT_EXPIRED = 1<<10,
	GNUTLS_CERT_SIGNATURE_FAILURE = 1<<11,
	GNUTLS_CERT_REVOCATION_DATA_SUPERSEDED = 1<<12,
	GNUTLS_CERT_UNEXPECTED_OWNER = 1<<14,
	GNUTLS_CERT_REVOCATION_DATA_ISSUED_IN_FUTURE = 1<<15,
	GNUTLS_CERT_SIGNER_CONSTRAINTS_FAILURE = 1<<16,
	GNUTLS_CERT_MISMATCH = 1<<17,
} gnutls_certificate_status_t;
typedef enum gnutls_connection_end { GNUTLS_SERVER=1, GNUTLS_CLIENT } gnutls_connection_end;
typedef enum gnutls_credentials_type { GNUTLS_CRD_CERTIFICATE=1, GNUTLS_CRD_ANON, GNUTLS_CRD_SRP } gnutls_credentials_type;
typedef enum gnutls_close_request { GNUTLS_SHUT_RDWR=0, GNUTLS_SHUT_WR=1 } gnutls_close_request;
typedef ssize_t (*gnutls_pull_func) (gnutls_transport_ptr_t, void *, size_t);
typedef ssize_t (*gnutls_push_func) (gnutls_transport_ptr_t, const void *, size_t);

#define GNUTLS_E_AGAIN -28
#define GNUTLS_E_CERTIFICATE_ERROR -43
#define GNUTLS_E_INTERRUPTED -52

static int (VARGS *gnutls_bye)(gnutls_session_t session, gnutls_close_request how);
static void (VARGS *gnutls_perror)(int error);
static int (VARGS *gnutls_handshake)(gnutls_session_t session);
static void (VARGS *gnutls_transport_set_ptr)(gnutls_session_t session, gnutls_transport_ptr_t ptr);
static void (VARGS *gnutls_transport_set_push_function)(gnutls_session_t session, gnutls_push_func push_func);
static void (VARGS *gnutls_transport_set_pull_function)(gnutls_session_t session, gnutls_pull_func pull_func);
static void (VARGS *gnutls_transport_set_errno)(gnutls_session_t session, int err);
static int (VARGS *gnutls_error_is_fatal)(int error);
static int (VARGS *gnutls_credentials_set)(gnutls_session_t, gnutls_credentials_type type, void* cred);
static int (VARGS *gnutls_kx_set_priority)(gnutls_session_t session, const int*);
static int (VARGS *gnutls_init)(gnutls_session_t * session, gnutls_connection_end con_end);
static int (VARGS *gnutls_set_default_priority)(gnutls_session_t session);
static int (VARGS *gnutls_certificate_allocate_credentials)(gnutls_certificate_credentials *sc);
static int (VARGS *gnutls_certificate_type_set_priority)(gnutls_session_t session, const int*);
static int (VARGS *gnutls_anon_allocate_client_credentials)(gnutls_anon_client_credentials *sc);
static int (VARGS *gnutls_global_init)(void);
static int (VARGS *gnutls_record_send)(gnutls_session_t session, const void *data, size_t sizeofdata);
static int (VARGS *gnutls_record_recv)(gnutls_session_t session, void *data, size_t sizeofdata);

static int (VARGS *gnutls_certificate_set_verify_function)();
static void *(VARGS *gnutls_session_get_ptr)(gnutls_session_t session);
static void (VARGS *gnutls_session_set_ptr)(gnutls_session_t session, void *ptr);
#if GNUTLS_VERSION_3_0_0_PLUS
static int (VARGS *gnutls_certificate_set_x509_system_trust)(gnutls_certificate_credentials cred);
#else
static int (VARGS *gnutls_certificate_set_x509_trust_file)(gnutls_certificate_credentials cred, const char * cafile, gnutls_x509_crt_fmt_t type);
#endif
#if GNUTLS_VERSION_3_1_4_PLUS
static int (VARGS *gnutls_certificate_verify_peers3)(gnutls_session_t session, const char* hostname, unsigned int * status);
static int (VARGS *gnutls_certificate_verification_status_print)(unsigned int status, gnutls_certificate_type_t type, gnutls_datum_t * out, unsigned int flags);
#else
static int (VARGS *gnutls_certificate_verify_peers2)(gnutls_session_t session, unsigned int * status);
static int (VARGS *gnutls_x509_crt_check_hostname)(gnutls_x509_crt_t cert, const char * hostname);
static int (VARGS *gnutls_x509_crt_init)(gnutls_x509_crt_t * cert); 
static int (VARGS *gnutls_x509_crt_import)(gnutls_x509_crt_t cert, const gnutls_datum_t *data, gnutls_x509_crt_fmt_t format);
static const gnutls_datum_t *(VARGS *gnutls_certificate_get_peers)(gnutls_session_t session, unsigned int * list_size);
#endif
static gnutls_certificate_type_t (VARGS *gnutls_certificate_type_get)(gnutls_session_t session);
static void (VARGS *gnutls_free)(void * ptr);

static qboolean Init_GNUTLS(void)
{
	dllhandle_t hmod;

	dllfunction_t functable[] =
	{
		{(void**)&gnutls_bye, "gnutls_bye"},
		{(void**)&gnutls_perror, "gnutls_perror"},
		{(void**)&gnutls_handshake, "gnutls_handshake"},
		{(void**)&gnutls_transport_set_ptr, "gnutls_transport_set_ptr"},
		{(void**)&gnutls_transport_set_push_function, "gnutls_transport_set_push_function"},
		{(void**)&gnutls_transport_set_pull_function, "gnutls_transport_set_pull_function"},
		{(void**)&gnutls_transport_set_errno, "gnutls_transport_set_errno"},
		{(void**)&gnutls_error_is_fatal, "gnutls_error_is_fatal"},
		{(void**)&gnutls_certificate_type_set_priority, "gnutls_certificate_type_set_priority"},
		{(void**)&gnutls_credentials_set, "gnutls_credentials_set"},
		{(void**)&gnutls_kx_set_priority, "gnutls_kx_set_priority"},
		{(void**)&gnutls_init, "gnutls_init"},
		{(void**)&gnutls_set_default_priority, "gnutls_set_default_priority"},
		{(void**)&gnutls_certificate_allocate_credentials, "gnutls_certificate_allocate_credentials"},
		{(void**)&gnutls_anon_allocate_client_credentials, "gnutls_anon_allocate_client_credentials"},
		{(void**)&gnutls_global_init, "gnutls_global_init"},
		{(void**)&gnutls_record_send, "gnutls_record_send"},
		{(void**)&gnutls_record_recv, "gnutls_record_recv"},

		{(void**)&gnutls_certificate_set_verify_function, "gnutls_certificate_set_verify_function"},
		{(void**)&gnutls_session_get_ptr, "gnutls_session_get_ptr"},
		{(void**)&gnutls_session_set_ptr, "gnutls_session_set_ptr"},
#if GNUTLS_VERSION_3_0_0_PLUS
		{(void**)&gnutls_certificate_set_x509_system_trust, "gnutls_certificate_set_x509_system_trust"},
#else
		{(void**)&gnutls_certificate_set_x509_trust_file, "gnutls_certificate_set_x509_trust_file"},
#endif
#if GNUTLS_VERSION_3_1_4_PLUS
		{(void**)&gnutls_certificate_verify_peers3, "gnutls_certificate_verify_peers3"},
		{(void**)&gnutls_certificate_verification_status_print, "gnutls_certificate_verification_status_print"},
#else
		{(void**)&gnutls_certificate_verify_peers2, "gnutls_certificate_verify_peers2"},
		{(void**)&gnutls_x509_crt_init, "gnutls_x509_crt_init"},
		{(void**)&gnutls_x509_crt_import, "gnutls_x509_crt_import"},
		{(void**)&gnutls_certificate_get_peers, "gnutls_certificate_get_peers"},
		{(void**)&gnutls_x509_crt_check_hostname, "gnutls_x509_crt_check_hostname"},
#endif
		{(void**)&gnutls_certificate_type_get, "gnutls_certificate_type_get"},
		{(void**)&gnutls_free, "gnutls_free"},
		{NULL, NULL}
	};
	
#ifdef __CYGWIN__
	hmod = Sys_LoadLibrary("cyggnutls-26.dll", functable);
#else
	hmod = Sys_LoadLibrary("libgnutls.so.26", functable);
#endif
	if (!hmod)
		return false;
	return true;
}

#else
#include <gnutls/gnutls.h>
static qboolean Init_GNUTLS(void) {return true;}
#endif

struct sslbuf
{
	char data[8192];
	int avail;
};
typedef struct 
{
	vfsfile_t funcs;
	vfsfile_t *stream;

	char certname[512];
	gnutls_session_t session;

	qboolean handshaking;

	struct sslbuf outplain;
	struct sslbuf outcrypt;
	struct sslbuf inplain;
	struct sslbuf incrypt;
} gnutlsfile_t;

#define CAFILE "/etc/ssl/certs/ca-certificates.crt"

static void QDECL SSL_Close(vfsfile_t *vfs)
{
	gnutlsfile_t *file = (void*)vfs;

	file->handshaking = true;

	if (file->session)
		gnutls_bye (file->session, GNUTLS_SHUT_RDWR);
	file->session = NULL;
	if (file->stream)
		VFS_CLOSE(file->stream);
	file->stream = NULL;
}
static int QDECL SSL_CheckCert(gnutls_session_t session)
{
	gnutlsfile_t *file = gnutls_session_get_ptr (session);
	unsigned int certstatus;
	cvar_t *tls_ignorecertificateerrors;

#if GNUTLS_VERSION_3_1_4_PLUS
	if (gnutls_certificate_verify_peers3(session, file->certname, &certstatus) >= 0)
	{
		{
			gnutls_datum_t out;
			gnutls_certificate_type_t type;
			if (certstatus == 0)
				return 0;

			type = gnutls_certificate_type_get (session);
			if (gnutls_certificate_verification_status_print(certstatus, type, &out, 0) >= 0)
			{
				Con_Printf("%s: %s\n", file->certname, out.data);
				gnutls_free(out.data);

#else
	if (gnutls_certificate_verify_peers2(session, &certstatus) >= 0)
	{
		int certslen;
		//grab the certificate
		const gnutls_datum_t *const certlist = gnutls_certificate_get_peers(session, &certslen);
		if (certlist && certslen)
		{
			//and make sure the hostname on it actually makes sense.
			gnutls_x509_crt_t cert;
			gnutls_x509_crt_init(&cert);
			gnutls_x509_crt_import(cert, certlist, GNUTLS_X509_FMT_DER);
			if (gnutls_x509_crt_check_hostname(cert, file->certname))
			{
				if (certstatus == 0)
					return 0;

				if (certstatus & GNUTLS_CERT_SIGNER_NOT_FOUND)
					Con_Printf("%s: Certificate authority is not recognised\n", file->certname);
				else if (certstatus & GNUTLS_CERT_INSECURE_ALGORITHM)
					Con_Printf("%s: Certificate uses insecure algorithm\n", file->certname);
				else if (certstatus & (GNUTLS_CERT_REVOCATION_DATA_ISSUED_IN_FUTURE|GNUTLS_CERT_REVOCATION_DATA_SUPERSEDED|GNUTLS_CERT_EXPIRED|GNUTLS_CERT_REVOKED|GNUTLS_CERT_NOT_ACTIVATED))
					Con_Printf("%s: Certificate has expired or was revoked or not yet valid\n", file->certname);
				else if (certstatus & GNUTLS_CERT_SIGNATURE_FAILURE)
					Con_Printf("%s: Certificate signature failure\n", file->certname);
				else
					Con_Printf("%s: Certificate error\n", file->certname);
#endif
				tls_ignorecertificateerrors = Cvar_Get("tls_ignorecertificateerrors", "0", CVAR_NOTFROMSERVER, "TLS");
				if (tls_ignorecertificateerrors && tls_ignorecertificateerrors->ival)
				{
					Con_Printf("%s: Ignoring certificate errors (tls_ignorecertificateerrors is %i)\n", file->certname, tls_ignorecertificateerrors->ival);
					return 0;
				}
			}
		}
	}

	Con_DPrintf("%s: rejecting certificate\n", file->certname);
	return GNUTLS_E_CERTIFICATE_ERROR;
}

//return 1 to read data.
//-1 or 0 for error or not ready
int SSL_DoHandshake(gnutlsfile_t *file)
{
	int err;
	//session was previously closed = error
	if (!file->session)
		return -1;

	err = gnutls_handshake (file->session);
	if (err < 0)
	{	//non-fatal errors can just handshake again the next time the caller checks to see if there's any data yet
		//(e_again or e_intr)
		if (!gnutls_error_is_fatal(err))
			return 0;

		//certificate errors etc
//		gnutls_perror (err);

		SSL_Close(&file->funcs);
//		Con_Printf("%s: abort\n", file->certname);
		return -1;
	}
	file->handshaking = false;
	return 1;
}

static int QDECL SSL_Read(struct vfsfile_s *f, void *buffer, int bytestoread)
{
	gnutlsfile_t *file = (void*)f;
	int read;

	if (file->handshaking)
	{
		read = SSL_DoHandshake(file);
		if (read <= 0)
			return read;
	}

	read = gnutls_record_recv(file->session, buffer, bytestoread);
	if (read < 0)
	{
		if (!gnutls_error_is_fatal(read))
			return 0;
		else
		{
			Con_Printf("TLS Read Error %i (bufsize %i)\n", read, bytestoread);
			return -1;
		}
	}
	else if (read == 0)
		return -1;	//closed by remote connection.
	return read;
}
static int QDECL SSL_Write(struct vfsfile_s *f, const void *buffer, int bytestowrite)
{
	gnutlsfile_t *file = (void*)f;
	int written;

	if (file->handshaking)
	{
		written = SSL_DoHandshake(file);
		if (written <= 0)
			return written;
	}

	written = gnutls_record_send(file->session, buffer, bytestowrite);
	if (written < 0)
	{
		if (!gnutls_error_is_fatal(written))
			return 0;
		else
		{
			Con_Printf("TLS Send Error %i (%i bytes)\n", written, bytestowrite);
			return -1;
		}
	}
	else if (written == 0)
		return -1;	//closed by remote connection.
	return written;
}
static qboolean QDECL SSL_Seek (struct vfsfile_s *file, qofs_t pos)
{
	return false;
}
static qofs_t QDECL SSL_Tell (struct vfsfile_s *file)
{
	return 0;
}
static qofs_t QDECL SSL_GetLen (struct vfsfile_s *file)
{
	return 0;
}


#include <errno.h>

/*functions for gnutls to call when it wants to send data*/
static ssize_t SSL_Push(gnutls_transport_ptr_t p, const void *data, size_t size)
{
	gnutlsfile_t *file = p;
	int done = VFS_WRITE(file->stream, data, size);
	if (!done)
	{
		gnutls_transport_set_errno(file->session, EAGAIN);
		return -1;
	}
	if (done < 0)
		return 0;
	gnutls_transport_set_errno(file->session, done<0?errno:0);
	return done;
}
/*static ssize_t SSL_PushV(gnutls_transport_ptr_t p, giovec_t *iov, int iovcnt)
{
	int i;
	ssize_t written;
	ssize_t total;
	gnutlsfile_t *file = p;
	for (i = 0; i < iovcnt; i++)
	{
		written = SSL_Push(file, iov[i].iov_base, iov[i].iov_len);
		if (written <= 0)
			break;
		total += written;
		if (written < iov[i].iov_len)
			break;
	}
	if (!total)
	{
		gnutls_transport_set_errno(file->session, EAGAIN);
		return -1;
	}
	gnutls_transport_set_errno(file->session, 0);
	return total;
}*/
static ssize_t SSL_Pull(gnutls_transport_ptr_t p, void *data, size_t size)
{
    gnutlsfile_t *file = p;
    int done = VFS_READ(file->stream, data, size);
    if (!done)
    {
        gnutls_transport_set_errno(file->session, EAGAIN);
        return -1;
    }
	if (done < 0)
	{
		return 0;
	}
	gnutls_transport_set_errno(file->session, done<0?errno:0);
    return done;
}

vfsfile_t *FS_OpenSSL(const char *hostname, vfsfile_t *source, qboolean server)
{
	gnutlsfile_t *newf;
	qboolean anon = false;
	
	static gnutls_anon_client_credentials anoncred;
	static gnutls_certificate_credentials xcred;

//	long _false = false;
//	long _true = true;

  /* Need to enable anonymous KX specifically. */
	const int kx_prio[] = {GNUTLS_KX_ANON_DH, 0};
	const int cert_type_priority[3] = {GNUTLS_CRT_X509, 0};


	{
		static qboolean needinit = true;
		if (needinit)
		{
			if (!Init_GNUTLS())
			{
				Con_Printf("GnuTLS library not available.\n");
				VFS_CLOSE(source);
				return NULL;
			}
			gnutls_global_init ();

			gnutls_anon_allocate_client_credentials (&anoncred);
			gnutls_certificate_allocate_credentials (&xcred);

#ifdef GNUTLS_VERSION_3_0_0_PLUS
			gnutls_certificate_set_x509_system_trust (xcred);
#else
			gnutls_certificate_set_x509_trust_file (xcred, CAFILE, GNUTLS_X509_FMT_PEM);
#endif
			gnutls_certificate_set_verify_function (xcred, SSL_CheckCert);

			needinit = false;
		}
	}

	newf = Z_Malloc(sizeof(*newf));
	if (!newf)
	{
		VFS_CLOSE(source);
		return NULL;
	}
	newf->stream = source;
	newf->funcs.Close = SSL_Close;
	newf->funcs.Flush = NULL;
	newf->funcs.GetLen = SSL_GetLen;
	newf->funcs.ReadBytes = SSL_Read;
	newf->funcs.WriteBytes = SSL_Write;
	newf->funcs.Seek = SSL_Seek;
	newf->funcs.Tell = SSL_Tell;
	newf->funcs.seekingisabadplan = true;

	Q_strncpyz(newf->certname, hostname, sizeof(newf->certname));

	// Initialize TLS session
	gnutls_init (&newf->session, GNUTLS_CLIENT);

	gnutls_session_set_ptr(newf->session, newf);

	// Use default priorities
	gnutls_set_default_priority (newf->session);
	if (anon)
	{
		gnutls_kx_set_priority (newf->session, kx_prio);
		gnutls_credentials_set (newf->session, GNUTLS_CRD_ANON, anoncred);
	}
	else
	{
		gnutls_certificate_type_set_priority (newf->session, cert_type_priority);
		gnutls_credentials_set (newf->session, GNUTLS_CRD_CERTIFICATE, xcred);
	}

	// tell gnutls how to send/receive data
	gnutls_transport_set_ptr (newf->session, newf);
	gnutls_transport_set_push_function(newf->session, SSL_Push);
	//gnutls_transport_set_vec_push_function(newf->session, SSL_PushV);
	gnutls_transport_set_pull_function(newf->session, SSL_Pull);
	//gnutls_transport_set_pull_timeout_function(newf->session, NULL);

	newf->handshaking = true;

	return &newf->funcs;
}
#endif

