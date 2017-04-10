//This file should be easily portable.
//The biggest strength of this plugin system is that ALL interactions are performed via
//named functions, this makes it *really* easy to port plugins from one engine to annother.

#include "quakedef.h"

#define GNUTLS_DYNAMIC 	//statically linking is bad, because that just dynamically links to a .so that probably won't exist.
						//on the other hand, it does validate that the function types are correct.

#ifdef HAVE_GNUTLS

#if defined(_WIN32) && !defined(MINGW)

#define GNUTLS_VERSION "2.12.23"
#define GNUTLS_SOPREFIX ""
#define GNUTLS_SONUM 26

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
typedef struct DSTRUCT* gnutls_certificate_credentials_t;
typedef gnutls_certificate_credentials_t gnutls_certificate_client_credentials_t;
typedef struct DSTRUCT* gnutls_anon_client_credentials_t;
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
typedef enum gnutls_connection_end { GNUTLS_SERVER=1, GNUTLS_CLIENT } gnutls_connection_end_t;
typedef enum gnutls_credentials_type { GNUTLS_CRD_CERTIFICATE=1, GNUTLS_CRD_ANON, GNUTLS_CRD_SRP } gnutls_credentials_type_t;
typedef enum gnutls_close_request { GNUTLS_SHUT_RDWR=0, GNUTLS_SHUT_WR=1 } gnutls_close_request_t;
typedef ssize_t (*gnutls_pull_func) (gnutls_transport_ptr_t, void *, size_t);
typedef ssize_t (*gnutls_push_func) (gnutls_transport_ptr_t, const void *, size_t);

#define GNUTLS_E_AGAIN -28
#define GNUTLS_E_CERTIFICATE_ERROR -43
#define GNUTLS_E_INTERRUPTED -52
#define GNUTLS_E_PREMATURE_TERMINATION -110

typedef enum
{
	GNUTLS_NAME_DNS = 1
} gnutls_server_name_type_t;

typedef int (VARGS gnutls_certificate_verify_function)(gnutls_session_t session);

#else
#include <gnutls/gnutls.h>
#define gnutls_connection_end_t unsigned int

	#if GNUTLS_VERSION_MAJOR < 3 || (GNUTLS_VERSION_MAJOR == 3 && GNUTLS_VERSION_MINOR < 3)
		#define GNUTLS_SONUM 26	//cygwin or something.
	#endif
	#if GNUTLS_VERSION_MAJOR == 3 && GNUTLS_VERSION_MINOR == 3
		#define GNUTLS_SOPREFIX "-deb0"	//not sure what this is about.
		#define GNUTLS_SONUM 28	//debian jessie
	#endif
	#if GNUTLS_VERSION_MAJOR == 3 && GNUTLS_VERSION_MINOR == 4
		#define GNUTLS_SONUM 30	//ubuntu 16.04
	#endif
	#if GNUTLS_VERSION_MAJOR == 3 && GNUTLS_VERSION_MINOR == 5
		#define GNUTLS_SONUM 30	//debian stretch
	#endif
	#if GNUTLS_VERSION_MAJOR == 3 && GNUTLS_VERSION_MINOR > 5
		#define GNUTLS_SONUM 30	//no idea what the future holds. maybe we'll be lucky...
	#endif
	#ifndef GNUTLS_SONUM
		#pragma message "GNUTLS version not recognised. Will probably not be loadable."
	#endif
	#ifndef GNUTLS_SOPREFIX
		#define GNUTLS_SOPREFIX
	#endif
#endif

#if GNUTLS_VERSION_MAJOR >= 3
	#define GNUTLS_HAVE_SYSTEMTRUST
#endif
#if GNUTLS_VERSION_MAJOR >= 4 || (GNUTLS_VERSION_MAJOR == 3 && (GNUTLS_VERSION_MINOR > 1 || (GNUTLS_VERSION_MINOR == 1 && GNUTLS_VERSION_PATCH >= 1)))
	#define GNUTLS_HAVE_VERIFY3
#endif


static int (VARGS *qgnutls_bye)(gnutls_session_t session, gnutls_close_request_t how);
static void (VARGS *qgnutls_perror)(int error);
static int (VARGS *qgnutls_handshake)(gnutls_session_t session);
static void (VARGS *qgnutls_transport_set_ptr)(gnutls_session_t session, gnutls_transport_ptr_t ptr);
static void (VARGS *qgnutls_transport_set_push_function)(gnutls_session_t session, gnutls_push_func push_func);
static void (VARGS *qgnutls_transport_set_pull_function)(gnutls_session_t session, gnutls_pull_func pull_func);
static void (VARGS *qgnutls_transport_set_errno)(gnutls_session_t session, int err);
static int (VARGS *qgnutls_error_is_fatal)(int error);
static int (VARGS *qgnutls_credentials_set)(gnutls_session_t, gnutls_credentials_type_t type, void* cred);
static int (VARGS *qgnutls_kx_set_priority)(gnutls_session_t session, const int*);
static int (VARGS *qgnutls_init)(gnutls_session_t * session, gnutls_connection_end_t con_end);
static int (VARGS *qgnutls_set_default_priority)(gnutls_session_t session);
static int (VARGS *qgnutls_certificate_allocate_credentials)(gnutls_certificate_credentials_t *sc);
static int (VARGS *qgnutls_certificate_type_set_priority)(gnutls_session_t session, const int*);
static int (VARGS *qgnutls_anon_allocate_client_credentials)(gnutls_anon_client_credentials_t *sc);
static int (VARGS *qgnutls_global_init)(void);
static ssize_t (VARGS *qgnutls_record_send)(gnutls_session_t session, const void *data, size_t sizeofdata);
static ssize_t (VARGS *qgnutls_record_recv)(gnutls_session_t session, void *data, size_t sizeofdata);

static void (VARGS *qgnutls_certificate_set_verify_function)(gnutls_certificate_credentials_t cred, gnutls_certificate_verify_function *func);
static void *(VARGS *qgnutls_session_get_ptr)(gnutls_session_t session);
static void (VARGS *qgnutls_session_set_ptr)(gnutls_session_t session, void *ptr);
#ifdef GNUTLS_HAVE_SYSTEMTRUST
static int (VARGS *qgnutls_certificate_set_x509_system_trust)(gnutls_certificate_credentials_t cred);
#else
static int (VARGS *qgnutls_certificate_set_x509_trust_file)(gnutls_certificate_credentials_t cred, const char * cafile, gnutls_x509_crt_fmt_t type);
#endif
#ifdef GNUTLS_HAVE_VERIFY3
static int (VARGS *qgnutls_certificate_verify_peers3)(gnutls_session_t session, const char* hostname, unsigned int * status);
static int (VARGS *qgnutls_certificate_verification_status_print)(unsigned int status, gnutls_certificate_type_t type, gnutls_datum_t * out, unsigned int flags);
#else
static int (VARGS *qgnutls_certificate_verify_peers2)(gnutls_session_t session, unsigned int * status);
static int (VARGS *qgnutls_x509_crt_check_hostname)(gnutls_x509_crt_t cert, const char * hostname);
static int (VARGS *qgnutls_x509_crt_init)(gnutls_x509_crt_t * cert); 
static int (VARGS *qgnutls_x509_crt_import)(gnutls_x509_crt_t cert, const gnutls_datum_t *data, gnutls_x509_crt_fmt_t format);
#endif
static const gnutls_datum_t *(VARGS *qgnutls_certificate_get_peers)(gnutls_session_t session, unsigned int * list_size);
static gnutls_certificate_type_t (VARGS *qgnutls_certificate_type_get)(gnutls_session_t session);
static void (VARGS *qgnutls_free)(void * ptr);
static int (VARGS *qgnutls_server_name_set)(gnutls_session_t session, gnutls_server_name_type_t type, const void * name, size_t name_length); 

static qboolean Init_GNUTLS(void)
{
#ifdef GNUTLS_HAVE_SYSTEMTRUST
	#define GNUTLS_TRUSTFUNCS GNUTLS_FUNC(gnutls_certificate_set_x509_system_trust)
#else
	#define GNUTLS_TRUSTFUNCS GNUTLS_FUNC(gnutls_certificate_set_x509_trust_file)
#endif
#ifdef GNUTLS_HAVE_VERIFY3
	#define GNUTLS_VERIFYFUNCS \
		GNUTLS_FUNC(gnutls_certificate_verify_peers3) \
		GNUTLS_FUNC(gnutls_certificate_verification_status_print)
#else
	#define GNUTLS_VERIFYFUNCS \
		GNUTLS_FUNC(gnutls_certificate_verify_peers2) \
		GNUTLS_FUNC(gnutls_x509_crt_check_hostname) \
		GNUTLS_FUNC(gnutls_x509_crt_init) \
		GNUTLS_FUNC(gnutls_x509_crt_import) \
		GNUTLS_FUNC(gnutls_certificate_get_peers)
#endif


#define GNUTLS_FUNCS \
	GNUTLS_FUNC(gnutls_bye)	\
	GNUTLS_FUNC(gnutls_perror)	\
	GNUTLS_FUNC(gnutls_handshake)	\
	GNUTLS_FUNC(gnutls_transport_set_ptr)	\
	GNUTLS_FUNC(gnutls_transport_set_push_function)	\
	GNUTLS_FUNC(gnutls_transport_set_pull_function)	\
	GNUTLS_FUNC(gnutls_transport_set_errno)	\
	GNUTLS_FUNC(gnutls_error_is_fatal)	\
	GNUTLS_FUNC(gnutls_certificate_type_set_priority)	\
	GNUTLS_FUNC(gnutls_credentials_set)	\
	GNUTLS_FUNC(gnutls_kx_set_priority)	\
	GNUTLS_FUNC(gnutls_init)	\
	GNUTLS_FUNC(gnutls_set_default_priority)	\
	GNUTLS_FUNC(gnutls_certificate_allocate_credentials)	\
	GNUTLS_FUNC(gnutls_anon_allocate_client_credentials)	\
	GNUTLS_FUNC(gnutls_global_init)	\
	GNUTLS_FUNC(gnutls_record_send)	\
	GNUTLS_FUNC(gnutls_record_recv)	\
	GNUTLS_FUNC(gnutls_certificate_set_verify_function)	\
	GNUTLS_FUNC(gnutls_session_get_ptr)	\
	GNUTLS_FUNC(gnutls_session_set_ptr)	\
	GNUTLS_TRUSTFUNCS	\
	GNUTLS_VERIFYFUNCS	\
	GNUTLS_FUNC(gnutls_certificate_type_get)	\
	GNUTLS_FUNC(gnutls_free)	\
	GNUTLS_FUNC(gnutls_server_name_set)	\

#ifdef GNUTLS_DYNAMIC
	dllhandle_t *hmod;

	dllfunction_t functable[] =
	{
//#define GNUTLS_FUNC(nam) {(void**)&q##nam, #nam},
//		GNUTLS_FUNCS
//#undef GNUTLS_FUNC
		{(void**)&qgnutls_bye, "gnutls_bye"},
		{(void**)&qgnutls_perror, "gnutls_perror"},
		{(void**)&qgnutls_handshake, "gnutls_handshake"},
		{(void**)&qgnutls_transport_set_ptr, "gnutls_transport_set_ptr"},
		{(void**)&qgnutls_transport_set_push_function, "gnutls_transport_set_push_function"},
		{(void**)&qgnutls_transport_set_pull_function, "gnutls_transport_set_pull_function"},
		{(void**)&qgnutls_transport_set_errno, "gnutls_transport_set_errno"},
		{(void**)&qgnutls_error_is_fatal, "gnutls_error_is_fatal"},
		{(void**)&qgnutls_certificate_type_set_priority, "gnutls_certificate_type_set_priority"},
		{(void**)&qgnutls_credentials_set, "gnutls_credentials_set"},
		{(void**)&qgnutls_kx_set_priority, "gnutls_kx_set_priority"},
		{(void**)&qgnutls_init, "gnutls_init"},
		{(void**)&qgnutls_set_default_priority, "gnutls_set_default_priority"},
		{(void**)&qgnutls_certificate_allocate_credentials, "gnutls_certificate_allocate_credentials"},
		{(void**)&qgnutls_anon_allocate_client_credentials, "gnutls_anon_allocate_client_credentials"},
		{(void**)&qgnutls_global_init, "gnutls_global_init"},
		{(void**)&qgnutls_record_send, "gnutls_record_send"},
		{(void**)&qgnutls_record_recv, "gnutls_record_recv"},

		{(void**)&qgnutls_certificate_set_verify_function, "gnutls_certificate_set_verify_function"},
		{(void**)&qgnutls_session_get_ptr, "gnutls_session_get_ptr"},
		{(void**)&qgnutls_session_set_ptr, "gnutls_session_set_ptr"},
#ifdef GNUTLS_HAVE_SYSTEMTRUST
		{(void**)&qgnutls_certificate_set_x509_system_trust, "gnutls_certificate_set_x509_system_trust"},
#else
		{(void**)&qgnutls_certificate_set_x509_trust_file, "gnutls_certificate_set_x509_trust_file"},
#endif
#ifdef GNUTLS_HAVE_VERIFY3
		{(void**)&qgnutls_certificate_verify_peers3, "gnutls_certificate_verify_peers3"},
		{(void**)&qgnutls_certificate_verification_status_print, "gnutls_certificate_verification_status_print"},
#else
		{(void**)&qgnutls_certificate_verify_peers2, "gnutls_certificate_verify_peers2"},
		{(void**)&qgnutls_x509_crt_init, "gnutls_x509_crt_init"},
		{(void**)&qgnutls_x509_crt_import, "gnutls_x509_crt_import"},
		{(void**)&qgnutls_x509_crt_check_hostname, "gnutls_x509_crt_check_hostname"},
#endif
		{(void**)&qgnutls_certificate_get_peers, "gnutls_certificate_get_peers"},
		{(void**)&qgnutls_certificate_type_get, "gnutls_certificate_type_get"},
		{(void**)&qgnutls_free, "gnutls_free"},
		{(void**)&qgnutls_server_name_set, "gnutls_server_name_set"},
		{NULL, NULL}
	};
	
#ifdef GNUTLS_SONUM
	#ifdef __CYGWIN__
		hmod = Sys_LoadLibrary("cyggnutls"GNUTLS_SOPREFIX"-"STRINGIFY(GNUTLS_SONUM)".dll", functable);
	#else
		hmod = Sys_LoadLibrary("libgnutls"GNUTLS_SOPREFIX".so."STRINGIFY(GNUTLS_SONUM), functable);
	#endif
#else
	hmod = Sys_LoadLibrary("libgnutls"GNUTLS_SOPREFIX".so", functable);	//hope and pray
#endif
	if (!hmod)
		return false;
#else
	#define GNUTLS_FUNC(name) q##name = name;
			GNUTLS_FUNCS
	#undef GNUTLS_FUNC
#endif
	return true;
}

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
	qboolean datagram;

	struct sslbuf outplain;
	struct sslbuf outcrypt;
	struct sslbuf inplain;
	struct sslbuf incrypt;
} gnutlsfile_t;

#define CAFILE "/etc/ssl/certs/ca-certificates.crt"

static qboolean QDECL SSL_Close(vfsfile_t *vfs)
{
	gnutlsfile_t *file = (void*)vfs;

	file->handshaking = true;

	if (file->session)
		qgnutls_bye (file->session, file->datagram?GNUTLS_SHUT_WR:GNUTLS_SHUT_RDWR);
	file->session = NULL;
	if (file->stream)
		VFS_CLOSE(file->stream);
	file->stream = NULL;
	return true;
}

static const qbyte fte_triptohell_certdata[917] = "\x30\x82\x03\x91\x30\x82\x02\x79\xa0\x03\x02\x01\x02\x02\x09\x00\xb5\x71\x47\x8d\x5e\x66\xf1\xd9\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0b\x05\x00\x30\x5f\x31\x0b\x30\x09\x06\x03\x55\x04\x06\x13\x02\x55\x53\x31\x11\x30\x0f\x06\x03\x55\x04\x08\x0c\x08\x4e\x65\x77\x20\x59\x6f\x72\x6b\x31\x11\x30\x0f\x06\x03\x55\x04\x07\x0c\x08\x4e\x65\x77\x20\x59\x6f\x72\x6b\x31\x0c\x30\x0a\x06\x03\x55\x04\x0a\x0c\x03\x46\x54\x45\x31\x1c\x30\x1a\x06\x03\x55\x04\x03\x0c\x13\x66\x74\x65\x2e\x74\x72\x69\x70\x74\x6f\x68\x65\x6c\x6c\x2e\x69\x6e\x66\x6f\x30\x1e\x17\x0d\x31\x34\x31\x32\x32\x35\x30\x30\x35\x38\x31\x34\x5a\x17\x0d\x31\x37\x30\x33\x30\x34\x30\x30\x35\x38\x31\x34\x5a\x30\x5f\x31\x0b\x30\x09\x06\x03\x55\x04\x06\x13\x02\x55\x53\x31\x11\x30\x0f\x06\x03\x55\x04\x08\x0c\x08\x4e\x65\x77\x20\x59\x6f\x72\x6b\x31\x11\x30\x0f\x06\x03\x55\x04\x07\x0c\x08\x4e\x65\x77\x20\x59\x6f\x72\x6b\x31\x0c\x30\x0a\x06\x03\x55\x04\x0a\x0c\x03\x46\x54\x45\x31\x1c\x30\x1a\x06\x03\x55\x04\x03\x0c\x13\x66\x74\x65\x2e\x74\x72\x69\x70\x74\x6f\x68\x65\x6c\x6c\x2e\x69\x6e\x66\x6f\x30\x82\x01\x22\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01\x05\x00\x03\x82\x01\x0f\x00\x30\x82\x01\x0a\x02\x82\x01\x01\x00\xdd\xb8\x7c\x69\x3d\x63\x95\xe3\x88\x15\xfd\xad\x93\x5e\x6b\x97\xfb\x74\xba\x1f\x83\x33\xe5\x8a\x8d\x8f\xb0\xbf\xf9\xd3\xa1\x2c\x65\x53\xa7\xef\xd3\x0f\xdc\x03\x60\x0a\x40\xef\xa8\xef\x3f\xb3\xd9\x8d\x31\x39\x12\x8a\xd8\x0e\x24\x8f\xe5\x58\x26\x86\x4c\x76\x6c\x59\x9a\xab\xea\x1c\x3d\xfb\x62\x62\xad\xaf\xd6\x00\x33\x76\x2d\xbb\xeb\xe8\xec\xb4\x76\x4f\xb0\xbe\xcf\xf0\x46\x94\x40\x02\x99\xd4\xb2\x71\x71\xd6\xf5\x1f\xc3\x4f\x1e\x1e\xb4\x0d\x82\x49\xc4\xa2\xdc\xae\x6f\x4e\x3a\xf9\x0e\xdd\xf4\xd2\x53\xe3\xe7\x7d\x58\x79\xf4\xce\x1f\x6c\xac\x81\x8c\x8c\xe1\x03\x5b\x22\x56\x92\x19\x4f\x74\xc0\x36\x41\xac\x1b\xfa\x9e\xf7\x2a\x0f\xd6\x4b\xcc\x9a\xca\x67\x87\xb7\x95\xdf\xb7\xd4\x7d\x8c\xcc\xa9\x25\xde\xdd\x8c\x1b\xd7\x32\xf2\x84\x25\x46\x7b\x10\x55\xf9\x80\xfd\x5d\xad\xab\xf9\x4c\x1f\xc0\xa5\xd1\x3f\x01\x86\x4d\xfa\x57\xab\x7a\x6d\xec\xf1\xdb\xf4\xad\xf2\x33\xcd\xa0\xed\xfe\x1b\x27\x55\x56\xba\x8c\x47\x70\x16\xd5\x75\x17\x8e\x80\xaa\x49\x5e\x93\x83\x1d\x6f\x1f\x2c\xf7\xa7\x64\xe6\x2e\x88\x8e\xff\x70\x5a\x41\x52\xae\x93\x02\x03\x01\x00\x01\xa3\x50\x30\x4e\x30\x1d\x06\x03\x55\x1d\x0e\x04\x16\x04\x14\x4e\x76\x4a\xce\x7b\x45\x14\x39\xeb\x9c\x28\x56\xb5\x7b\x8a\x18\x6f\x22\x17\x82\x30\x1f\x06\x03\x55\x1d\x23\x04\x18\x30\x16\x80\x14\x4e\x76\x4a\xce\x7b\x45\x14\x39\xeb\x9c\x28\x56\xb5\x7b\x8a\x18\x6f\x22\x17\x82\x30\x0c\x06\x03\x55\x1d\x13\x04\x05\x30\x03\x01\x01\xff\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0b\x05\x00\x03\x82\x01\x01\x00\x48\x22\x65\xed\x2e\xc5\xed\xbb\xe9\x40\x6c\x80\xc4\x63\x19\xd1\x00\xb4\x30\x34\x17\x7c\x7c\xbd\x1b\xc5\xa9\x43\x0c\x92\x6e\xd6\x2d\x11\x6c\x0d\xa6\xda\x30\xe9\xf7\x46\x7b\x01\xe4\x53\x23\xae\x88\xd1\xf2\xed\xca\x84\x06\x19\x97\xb9\x06\xfb\xda\xec\x72\x2d\x15\x20\xd2\x8f\x66\xad\xb5\xdd\x4b\x4f\xdf\x7e\xaf\xa3\x6c\x7f\x53\x32\x8f\xe2\x19\x5c\x44\x98\x86\x31\xee\xb4\x03\xe7\x27\xa1\x83\xab\xc3\xce\xb4\x9a\x01\xbe\x8c\x64\x2e\x2b\xe3\x4e\x55\xdf\x95\xeb\x16\x87\xbd\xfa\x11\xa2\x3e\x38\x92\x97\x36\xe9\x65\x60\xf3\xac\x68\x44\xb3\x51\x54\x3a\x42\xa8\x98\x9b\xee\x1b\x9e\x79\x6a\xaf\xc0\xbe\x41\xc4\xb1\x96\x42\xd9\x94\xef\x49\x5b\xbe\x2d\x04\xb9\xfb\x92\xbb\xdc\x0e\x29\xfd\xee\xa9\x68\x09\xf9\x9f\x69\x8b\x3d\xe1\x4b\xee\x24\xf9\xfe\x02\x3a\x0a\xb8\xcd\x6c\x07\x43\xa9\x4a\xe7\x03\x34\x2e\x72\xa7\x81\xaa\x40\xa9\x98\x5d\x97\xee\x2a\x99\xc6\x8f\xe8\x6f\x98\xa2\x85\xc9\x0d\x04\x19\x43\x6a\xd3\xc7\x15\x4c\x4b\xbc\xa5\xb8\x9f\x38\xf3\x43\x83\x0c\xef\x97\x6e\xa6\x20\xde\xc5\xd3\x1e\x3e\x5d\xcd\x58\x3d\x5c\x55\x7a\x90\x94";
static const qbyte triptohell_certdata[933] = "\x30\x82\x03\xa1\x30\x82\x02\x89\xa0\x03\x02\x01\x02\x02\x09\x00\xea\xb7\x13\xcf\x55\xe5\xe8\x8c\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0b\x05\x00\x30\x67\x31\x0b\x30\x09\x06\x03\x55\x04\x06\x13\x02\x55\x53\x31\x11\x30\x0f\x06\x03\x55\x04\x08\x0c\x08\x4e\x65\x77\x20\x59\x6f\x72\x6b\x31\x11\x30\x0f\x06\x03\x55\x04\x07\x0c\x08\x4e\x65\x77\x20\x59\x6f\x72\x6b\x31\x18\x30\x16\x06\x03\x55\x04\x0a\x0c\x0f\x74\x72\x69\x70\x74\x6f\x68\x65\x6c\x6c\x2e\x69\x6e\x66\x6f\x31\x18\x30\x16\x06\x03\x55\x04\x03\x0c\x0f\x74\x72\x69\x70\x74\x6f\x68\x65\x6c\x6c\x2e\x69\x6e\x66\x6f\x30\x1e\x17\x0d\x31\x34\x31\x32\x32\x35\x30\x30\x35\x38\x33\x37\x5a\x17\x0d\x31\x37\x30\x33\x30\x34\x30\x30\x35\x38\x33\x37\x5a\x30\x67\x31\x0b\x30\x09\x06\x03\x55\x04\x06\x13\x02\x55\x53\x31\x11\x30\x0f\x06\x03\x55\x04\x08\x0c\x08\x4e\x65\x77\x20\x59\x6f\x72\x6b\x31\x11\x30\x0f\x06\x03\x55\x04\x07\x0c\x08\x4e\x65\x77\x20\x59\x6f\x72\x6b\x31\x18\x30\x16\x06\x03\x55\x04\x0a\x0c\x0f\x74\x72\x69\x70\x74\x6f\x68\x65\x6c\x6c\x2e\x69\x6e\x66\x6f\x31\x18\x30\x16\x06\x03\x55\x04\x03\x0c\x0f\x74\x72\x69\x70\x74\x6f\x68\x65\x6c\x6c\x2e\x69\x6e\x66\x6f\x30\x82\x01\x22\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01\x05\x00\x03\x82\x01\x0f\x00\x30\x82\x01\x0a\x02\x82\x01\x01\x00\xd8\x77\x62\xf6\x74\xa7\x75\xde\xda\x09\xae\x9e\x76\x7a\xc6\x2a\xcf\x9a\xbe\xc6\xb9\x6d\xe2\xca\x0f\x2d\x95\xb8\x89\x93\xf7\x50\x64\x92\x7d\x95\x34\xe4\x6e\xef\x52\x56\xef\x13\x9a\x3a\xae\x84\x5b\x57\x82\x04\x86\x74\xbd\x4e\x38\x32\x56\x00\xd6\x34\x9c\x23\xd6\x81\x8e\x29\x77\x45\x61\x20\xdf\x28\xf8\xe5\x61\x83\xec\xe6\xa0\x1a\x75\xa8\x3b\x53\x6f\xc4\x09\x61\x66\x3a\xf0\x81\xbf\x2c\xf5\x8e\xf1\xe2\x35\xe4\x24\x7f\x16\xcc\xce\x60\xa2\x42\x6e\xc2\x3a\x29\x75\x6c\x79\xb0\x99\x9c\xe2\xfe\x27\x32\xb6\xf7\x0d\x71\xfd\x62\x9d\x54\x7c\x40\xb2\xf5\xa0\xa4\x25\x31\x8d\x65\xfd\x3f\x3b\x9b\x7e\x84\x74\x17\x3c\x1f\xec\x50\xcf\x75\xb8\x5c\xca\xfc\x0f\xe8\x47\xd8\x64\xec\x5f\x6c\x45\x9a\x55\x49\x97\x3f\xcb\x49\x34\x71\x0a\x12\x13\xbc\x3d\x53\x81\x17\x9a\x92\x44\x91\x07\xc2\xef\x6d\x64\x86\x5d\xfd\x67\xd5\x99\x38\x95\x46\x74\x6d\xb6\xbf\x29\xc9\x5b\xac\xb1\x46\xd6\x9e\x57\x5c\x7b\x24\x91\xf4\x7c\xe4\x01\x31\x8c\xec\x79\x94\xb7\x3f\xd2\x93\x6d\xe2\x69\xbe\x61\x44\x2e\x8f\x1a\xdc\xa8\x97\xf5\x81\x8e\x0c\xe1\x00\xf2\x71\x51\xf3\x02\x03\x01\x00\x01\xa3\x50\x30\x4e\x30\x1d\x06\x03\x55\x1d\x0e\x04\x16\x04\x14\x18\xb2\x6b\x63\xcc\x17\x54\xf6\xf0\xb6\x9e\x62\xa4\x35\xcf\x47\x74\x13\x29\xbf\x30\x1f\x06\x03\x55\x1d\x23\x04\x18\x30\x16\x80\x14\x18\xb2\x6b\x63\xcc\x17\x54\xf6\xf0\xb6\x9e\x62\xa4\x35\xcf\x47\x74\x13\x29\xbf\x30\x0c\x06\x03\x55\x1d\x13\x04\x05\x30\x03\x01\x01\xff\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0b\x05\x00\x03\x82\x01\x01\x00\x7f\x24\x18\x8a\x79\xee\xf9\xeb\xed\x29\x1e\x21\x15\x8a\x53\xc9\xb7\xec\x30\xc4\x85\x9f\x45\x85\x26\x36\xb7\x07\xf3\xf1\xff\x3b\x89\x05\x0a\xd4\x30\x68\x31\x68\x33\xdd\xf6\x58\xa3\x85\x9f\x49\x50\x76\x9a\xc5\x79\x13\xe1\x4d\x67\x0c\xf3\x92\xf0\x1d\x02\x1f\xc4\x5c\xd4\xa1\x0c\x57\xdf\x46\x84\x43\x9f\xb0\xe2\x91\x62\xa8\xe0\x86\x0d\x47\xe1\xd9\x60\x01\xc4\xe0\xda\x6f\x06\x0a\xad\x38\xf3\x66\x68\xc5\xe2\x66\x3e\x47\x83\x65\x64\xcd\xff\xf3\xbb\xa7\xfa\x23\xf1\x82\x5e\x06\x6a\x91\x37\x51\xcd\xb9\x95\x20\x89\xff\xa1\x54\xb2\x76\xcf\x8e\xe1\xcd\x13\x93\x13\xd1\xda\x0d\x0d\xbc\x0f\xd5\x11\x26\xd6\xaf\x60\x0f\x4d\x8a\x4f\x28\xee\x6c\xf1\x99\xdc\xed\x16\xdc\x87\x26\xfd\x23\x8a\xb8\xb0\x20\x0e\xe2\x32\xf5\x8e\xb0\x65\x98\x13\xb8\x4b\x39\x7c\x8c\x98\xa2\x29\x75\x48\x3a\x89\xf9\x61\x77\x6c\x2d\x84\x41\x40\x17\xa6\x50\xc5\x09\x63\x10\xe7\x09\xd4\x5c\xdd\x0e\x71\x16\xaf\xb1\x32\xe4\xc0\xe6\xea\xfd\x26\x55\x07\x40\x95\x84\x48\x62\x04\x10\x92\xb2\xd9\x27\xfb\x8a\xf3\x7c\xe6\xfe\xd4\xfc\xa6\x33\x79\x01\x5c\xc3\x1f\x80\xa8\xf3";
static struct
{
	char *hostname;
	unsigned int datasize;
	const qbyte *data;
} knowncerts[] = {
	{"triptohell.info", sizeof(triptohell_certdata), triptohell_certdata},
	{"fte.triptohell.info", sizeof(fte_triptohell_certdata), fte_triptohell_certdata},
};
static int QDECL SSL_CheckCert(gnutls_session_t session)
{
	gnutlsfile_t *file = qgnutls_session_get_ptr (session);
	unsigned int certstatus;
	cvar_t *tls_ignorecertificateerrors;
	qboolean preverified = false;

	size_t i;
	for (i = 0; i < countof(knowncerts); i++)
	{
		if (!strcmp(knowncerts[i].hostname, file->certname))
		{
			unsigned int certcount, j;
			const gnutls_datum_t *const certlist = qgnutls_certificate_get_peers(session, &certcount);
			if (certlist && certcount)
			{
				size_t offset = 0;

				for (j = 0; j < certcount; offset += certlist[j++].size)
				{
					if (certlist[j].size+offset > knowncerts[i].datasize)
						break;	//overflow...
					if (memcmp(certlist[j].data, knowncerts[i].data+offset, certlist[j].size))
						break;
				}

				if (j && j == certcount && offset == knowncerts[i].datasize)
					preverified = true;
				else
				{
#ifdef _DEBUG
					for (j = 0, offset = 0; j < certcount; j++)
						offset += certlist[j].size;
					Con_Printf("%s cert %zu bytes (chain %u)\n", file->certname, offset, certcount);
					Con_Printf("\"");

					for (j = 0; j < certcount; j++)
					{
						unsigned char *data = certlist[j].data;
						unsigned int datasize = certlist[j].size, k;
						for (k = 0; k < datasize; k++)
							Con_Printf("\\x%02x", data[k]);
					}
					Con_Printf("\"\n\n");
#endif
					Con_Printf(CON_ERROR "%s: Reported certificate does not match known certificate. Possible MITM attack, alternatively just an outdated client.\n", file->certname);
					return GNUTLS_E_CERTIFICATE_ERROR;
				}
			}
			break;
		}
	}

#ifdef GNUTLS_HAVE_VERIFY3
	if (qgnutls_certificate_verify_peers3(session, file->certname, &certstatus) >= 0)
	{
		{
			gnutls_datum_t out;
			gnutls_certificate_type_t type;

			if (preverified && certstatus == (GNUTLS_CERT_INVALID|GNUTLS_CERT_SIGNER_NOT_FOUND))
				return 0;
			if (certstatus == 0)
				return 0;

			type = qgnutls_certificate_type_get (session);
			if (qgnutls_certificate_verification_status_print(certstatus, type, &out, 0) >= 0)
			{
				Con_Printf("%s: %s (%x)\n", file->certname, out.data, certstatus);
//looks like its static anyway.				qgnutls_free(out.data);

#else
	if (qgnutls_certificate_verify_peers2(session, &certstatus) >= 0)
	{
		int certslen;
		//grab the certificate
		const gnutls_datum_t *const certlist = qgnutls_certificate_get_peers(session, &certslen);
		if (certlist && certslen)
		{
			//and make sure the hostname on it actually makes sense.
			gnutls_x509_crt_t cert;
			qgnutls_x509_crt_init(&cert);
			qgnutls_x509_crt_import(cert, certlist, GNUTLS_X509_FMT_DER);
			if (qgnutls_x509_crt_check_hostname(cert, file->certname))
			{
				if (preverified && certstatus == (GNUTLS_CERT_INVALID|GNUTLS_CERT_SIGNER_NOT_FOUND))
					return 0;
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
			else
				Con_DPrintf("%s: certificate is for a different domain\n", file->certname);
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

	err = qgnutls_handshake (file->session);
	if (err < 0)
	{	//non-fatal errors can just handshake again the next time the caller checks to see if there's any data yet
		//(e_again or e_intr)
		if (!qgnutls_error_is_fatal(err))
			return 0;

		//certificate errors etc
//		qgnutls_perror (err);

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

	read = qgnutls_record_recv(file->session, buffer, bytestoread);
	if (read < 0)
	{
		if (read == GNUTLS_E_PREMATURE_TERMINATION)
		{
			Con_Printf("TLS Premature Termination from %s\n", file->certname);
			return -1;
		}
		else if (read == GNUTLS_E_REHANDSHAKE)
		{
			file->handshaking = false;//gnutls_safe_renegotiation_status();
			//if false, 'recommended' to send an GNUTLS_A_NO_RENEGOTIATION alert, no idea how.
		}
		else if (!qgnutls_error_is_fatal(read))
			return 0;	//caller is expected to try again later, no real need to loop here, just in case it repeats (eg E_AGAIN)
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

	written = qgnutls_record_send(file->session, buffer, bytestowrite);
	if (written < 0)
	{
		if (!qgnutls_error_is_fatal(written))
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
		qgnutls_transport_set_errno(file->session, EAGAIN);
		return -1;
	}
	qgnutls_transport_set_errno(file->session, done<0?errno:0);
	if (done < 0)
		return 0;
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
		qgnutls_transport_set_errno(file->session, EAGAIN);
		return -1;
	}
	qgnutls_transport_set_errno(file->session, 0);
	return total;
}*/
static ssize_t SSL_Pull(gnutls_transport_ptr_t p, void *data, size_t size)
{
	gnutlsfile_t *file = p;
	int done = VFS_READ(file->stream, data, size);
	if (!done)
	{
		qgnutls_transport_set_errno(file->session, EAGAIN);
		return -1;
	}
	qgnutls_transport_set_errno(file->session, done<0?errno:0);
	if (done < 0)
	{
		return 0;
	}
	return done;
}

vfsfile_t *FS_OpenSSL(const char *hostname, vfsfile_t *source, qboolean server, qboolean datagram)
{
	gnutlsfile_t *newf;
	qboolean anon = false;
	
	static gnutls_anon_client_credentials_t anoncred;
	static gnutls_certificate_credentials_t xcred;

//	long _false = false;
//	long _true = true;

	/* Need to enable anonymous KX specifically. */
	const int kx_prio[] = {GNUTLS_KX_ANON_DH, 0};
	const int cert_type_priority[3] = {GNUTLS_CRT_X509, 0};

	if (!source)
		return NULL;

	if (datagram)
	{
		VFS_CLOSE(source);
		return NULL;
	}

#ifdef GNUTLS_DATAGRAM
	if (datagram)
		return NULL;
#endif

	{
		static qboolean needinit = true;
		if (needinit)
		{
			if (!Init_GNUTLS())
			{
				Con_Printf("GnuTLS "GNUTLS_VERSION" library not available.\n");
				VFS_CLOSE(source);
				return NULL;
			}
			qgnutls_global_init ();

			qgnutls_anon_allocate_client_credentials (&anoncred);
			qgnutls_certificate_allocate_credentials (&xcred);

#ifdef GNUTLS_HAVE_SYSTEMTRUST
			qgnutls_certificate_set_x509_system_trust (xcred);
#else
			qgnutls_certificate_set_x509_trust_file (xcred, CAFILE, GNUTLS_X509_FMT_PEM);
#endif
			qgnutls_certificate_set_verify_function (xcred, SSL_CheckCert);

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
	qgnutls_init (&newf->session, GNUTLS_CLIENT/*|(datagram?GNUTLS_DATAGRAM:0)*/);

	qgnutls_server_name_set(newf->session, GNUTLS_NAME_DNS, newf->certname, strlen(newf->certname));

	qgnutls_session_set_ptr(newf->session, newf);

	// Use default priorities
	qgnutls_set_default_priority (newf->session);
	if (anon)
	{
		qgnutls_kx_set_priority (newf->session, kx_prio);
		qgnutls_credentials_set (newf->session, GNUTLS_CRD_ANON, anoncred);
	}
	else
	{
//#if GNUTLS_VERSION_MAJOR >= 3
		//gnutls_priority_set_direct();
//#else
		qgnutls_certificate_type_set_priority (newf->session, cert_type_priority);
//#endif
		qgnutls_credentials_set (newf->session, GNUTLS_CRD_CERTIFICATE, xcred);
	}

	// tell gnutls how to send/receive data
	qgnutls_transport_set_ptr (newf->session, newf);
	qgnutls_transport_set_push_function(newf->session, SSL_Push);
	//qgnutls_transport_set_vec_push_function(newf->session, SSL_PushV);
	qgnutls_transport_set_pull_function(newf->session, SSL_Pull);
	//qgnutls_transport_set_pull_timeout_function(newf->session, NULL);

	newf->handshaking = true;

	return &newf->funcs;
}

/*
vfsfile_t *FS_OpenSSL(const char *hostname, vfsfile_t *source, qboolean server, qboolean datagram)
{
	if (source)
		VFS_CLOSE(source);
	return NULL;
}
*/
#endif

