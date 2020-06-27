//This file should be easily portable.
//The biggest strength of this plugin system is that ALL interactions are performed via
//named functions, this makes it *really* easy to port plugins from one engine to annother.

#include "quakedef.h"
#include "netinc.h"

#ifndef GNUTLS_STATIC
	#define GNUTLS_DYNAMIC 	//statically linking is bad, because that just dynamically links to a .so that probably won't exist.
							//on the other hand, it does validate that the function types are correct.
#endif

#ifdef HAVE_GNUTLS

#if defined(_WIN32) && !defined(MINGW) && 0

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
	#if GNUTLS_VERSION_MAJOR >= 3
		#include <gnutls/abstract.h>
	#endif
	#include <gnutls/x509.h>
	#if GNUTLS_VERSION_MAJOR >= 3 && defined(HAVE_DTLS)
		#include <gnutls/dtls.h>
	#else
		#undef HAVE_DTLS
	#endif
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
//static int (VARGS *qgnutls_kx_set_priority)(gnutls_session_t session, const int*);
static int (VARGS *qgnutls_init)(gnutls_session_t * session, gnutls_connection_end_t con_end);
static void (VARGS *qgnutls_deinit)(gnutls_session_t session);
static int (VARGS *qgnutls_set_default_priority)(gnutls_session_t session);
static int (VARGS *qgnutls_certificate_allocate_credentials)(gnutls_certificate_credentials_t *sc);
static int (VARGS *qgnutls_anon_allocate_client_credentials)(gnutls_anon_client_credentials_t *sc);
static int (VARGS *qgnutls_global_init)(void);
static ssize_t (VARGS *qgnutls_record_send)(gnutls_session_t session, const void *data, size_t sizeofdata);
static ssize_t (VARGS *qgnutls_record_recv)(gnutls_session_t session, void *data, size_t sizeofdata);

static void (VARGS *qgnutls_certificate_set_verify_function)(gnutls_certificate_credentials_t cred, gnutls_certificate_verify_function *func);
static void *(VARGS *qgnutls_session_get_ptr)(gnutls_session_t session);
static void (VARGS *qgnutls_session_set_ptr)(gnutls_session_t session, void *ptr);
int (VARGS *qgnutls_session_channel_binding)(gnutls_session_t session, gnutls_channel_binding_t cbtype, gnutls_datum_t * cb);
#ifdef GNUTLS_HAVE_SYSTEMTRUST
static int (VARGS *qgnutls_certificate_set_x509_system_trust)(gnutls_certificate_credentials_t cred);
#else
static int (VARGS *qgnutls_certificate_set_x509_trust_file)(gnutls_certificate_credentials_t cred, const char * cafile, gnutls_x509_crt_fmt_t type);
#endif
static int (VARGS *qgnutls_certificate_set_x509_key_file)(gnutls_certificate_credentials_t res, const char * certfile, const char * keyfile, gnutls_x509_crt_fmt_t type); 
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
static void		*(VARGS **qgnutls_malloc)(size_t);
static void (VARGS **qgnutls_free)(void * ptr);
static int (VARGS *qgnutls_server_name_set)(gnutls_session_t session, gnutls_server_name_type_t type, const void * name, size_t name_length); 

#ifdef HAVE_DTLS
static int (VARGS *qgnutls_key_generate)(gnutls_datum_t * key, unsigned int key_size);
static void (VARGS *qgnutls_transport_set_pull_timeout_function)(gnutls_session_t session, gnutls_pull_timeout_func func);
static int (VARGS *qgnutls_dtls_cookie_verify)(gnutls_datum_t * key, void *client_data, size_t client_data_size, void *_msg, size_t msg_size, gnutls_dtls_prestate_st * prestate);
static int (VARGS *qgnutls_dtls_cookie_send)(gnutls_datum_t * key, void *client_data, size_t client_data_size, gnutls_dtls_prestate_st * prestate, gnutls_transport_ptr_t ptr, gnutls_push_func push_func);
static void (VARGS *qgnutls_dtls_prestate_set)(gnutls_session_t session, gnutls_dtls_prestate_st * prestate);
static void (VARGS *qgnutls_dtls_set_mtu)(gnutls_session_t session, unsigned int mtu);
#endif

static unsigned int	(VARGS *qgnutls_sec_param_to_pk_bits)(gnutls_pk_algorithm_t algo, gnutls_sec_param_t param);
static int		(VARGS *qgnutls_x509_crt_init)(gnutls_x509_crt_t * cert);
static void		(VARGS *qgnutls_x509_crt_deinit)(gnutls_x509_crt_t cert);
static int		(VARGS *qgnutls_x509_crt_set_version)(gnutls_x509_crt_t crt, unsigned int version);
static int		(VARGS *qgnutls_x509_crt_set_activation_time)(gnutls_x509_crt_t cert, time_t act_time);
static int		(VARGS *qgnutls_x509_crt_set_expiration_time)(gnutls_x509_crt_t cert, time_t exp_time);
static int		(VARGS *qgnutls_x509_crt_set_serial)(gnutls_x509_crt_t cert, const void *serial, size_t serial_size);
static int		(VARGS *qgnutls_x509_crt_set_dn)(gnutls_x509_crt_t crt, const char *dn, const char **err);
static int		(VARGS *qgnutls_x509_crt_set_issuer_dn)(gnutls_x509_crt_t crt, const char *dn, const char **err);
static int		(VARGS *qgnutls_x509_crt_set_key)(gnutls_x509_crt_t crt, gnutls_x509_privkey_t key);
static int		(VARGS *qgnutls_x509_crt_export2)(gnutls_x509_crt_t cert, gnutls_x509_crt_fmt_t format, gnutls_datum_t * out);
static int		(VARGS *qgnutls_x509_crt_import)(gnutls_x509_crt_t cert, const gnutls_datum_t *data, gnutls_x509_crt_fmt_t format);
static int		(VARGS *qgnutls_x509_privkey_init)(gnutls_x509_privkey_t * key);
static void		(VARGS *qgnutls_x509_privkey_deinit)(gnutls_x509_privkey_t key);
static int		(VARGS *qgnutls_x509_privkey_generate)(gnutls_x509_privkey_t key, gnutls_pk_algorithm_t algo, unsigned int bits, unsigned int flags);
static int		(VARGS *qgnutls_x509_privkey_export2)(gnutls_x509_privkey_t key, gnutls_x509_crt_fmt_t format, gnutls_datum_t * out);
static int		(VARGS *qgnutls_x509_crt_privkey_sign)(gnutls_x509_crt_t crt, gnutls_x509_crt_t issuer, gnutls_privkey_t issuer_key, gnutls_digest_algorithm_t dig, unsigned int flags);
static int		(VARGS *qgnutls_privkey_init)(gnutls_privkey_t * key);
static void		(VARGS *qgnutls_privkey_deinit)(gnutls_privkey_t key);
static int		(VARGS *qgnutls_privkey_import_x509)(gnutls_privkey_t pkey, gnutls_x509_privkey_t key, unsigned int flags);
//static int		(VARGS *qgnutls_privkey_sign_hash2)(gnutls_privkey_t signer, gnutls_sign_algorithm_t algo, unsigned int flags, const gnutls_datum_t * hash_data, gnutls_datum_t * signature);
static int		(VARGS *qgnutls_privkey_sign_hash)(gnutls_privkey_t signer, gnutls_digest_algorithm_t hash_algo, unsigned int flags, const gnutls_datum_t * hash_data, gnutls_datum_t * signature);
static int		(VARGS *qgnutls_pubkey_init)(gnutls_pubkey_t * key);
static int		(VARGS *qgnutls_pubkey_import_x509)(gnutls_pubkey_t key, gnutls_x509_crt_t crt, unsigned int flags);
static int		(VARGS *qgnutls_pubkey_verify_hash2)(gnutls_pubkey_t key, gnutls_sign_algorithm_t algo, unsigned int flags, const gnutls_datum_t * hash, const gnutls_datum_t * signature);
static int		(VARGS *qgnutls_certificate_set_x509_key_mem)(gnutls_certificate_credentials_t res, const gnutls_datum_t * cert, const gnutls_datum_t * key, gnutls_x509_crt_fmt_t type);
static int		(VARGS *qgnutls_certificate_get_x509_key)(gnutls_certificate_credentials_t res, unsigned index, gnutls_x509_privkey_t *key);
static void		(VARGS *qgnutls_certificate_free_credentials)(gnutls_certificate_credentials_t sc);

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

#ifdef HAVE_DTLS
#define GNUTLS_DTLS_STUFF \
		GNUTLS_FUNC(gnutls_key_generate) \
		GNUTLS_FUNC(gnutls_transport_set_pull_timeout_function) \
		GNUTLS_FUNC(gnutls_dtls_cookie_verify) \
		GNUTLS_FUNC(gnutls_dtls_cookie_send) \
		GNUTLS_FUNC(gnutls_dtls_prestate_set) \
		GNUTLS_FUNC(gnutls_dtls_set_mtu)
#else
	#define GNUTLS_DTLS_STUFF
#endif


#define GNUTLS_X509_STUFF \
	GNUTLS_FUNC(gnutls_sec_param_to_pk_bits)	\
	GNUTLS_FUNC(gnutls_x509_crt_init)	\
	GNUTLS_FUNC(gnutls_x509_crt_deinit)	\
	GNUTLS_FUNC(gnutls_x509_crt_set_version)	\
	GNUTLS_FUNC(gnutls_x509_crt_set_activation_time)	\
	GNUTLS_FUNC(gnutls_x509_crt_set_expiration_time)	\
	GNUTLS_FUNC(gnutls_x509_crt_set_serial)	\
	GNUTLS_FUNC(gnutls_x509_crt_set_dn)	\
	GNUTLS_FUNC(gnutls_x509_crt_set_issuer_dn)	\
	GNUTLS_FUNC(gnutls_x509_crt_set_key)	\
	GNUTLS_FUNC(gnutls_x509_crt_export2)	\
	GNUTLS_FUNC(gnutls_x509_privkey_init)	\
	GNUTLS_FUNC(gnutls_x509_privkey_deinit)	\
	GNUTLS_FUNC(gnutls_x509_privkey_generate)	\
	GNUTLS_FUNC(gnutls_x509_privkey_export2)	\
	GNUTLS_FUNC(gnutls_x509_crt_privkey_sign)	\
	GNUTLS_FUNC(gnutls_privkey_init)	\
	GNUTLS_FUNC(gnutls_privkey_deinit)	\
	GNUTLS_FUNC(gnutls_privkey_import_x509)	\
	GNUTLS_FUNC(gnutls_certificate_set_x509_key_mem)



#define GNUTLS_FUNCS \
	GNUTLS_FUNC(gnutls_bye)	\
	GNUTLS_FUNC(gnutls_perror)	\
	GNUTLS_FUNC(gnutls_handshake)	\
	GNUTLS_FUNC(gnutls_transport_set_ptr)	\
	GNUTLS_FUNC(gnutls_transport_set_push_function)	\
	GNUTLS_FUNC(gnutls_transport_set_pull_function)	\
	GNUTLS_FUNC(gnutls_transport_set_errno)	\
	GNUTLS_FUNC(gnutls_error_is_fatal)	\
	GNUTLS_FUNC(gnutls_credentials_set)	\
	GNUTLS_FUNC(gnutls_init)	\
	GNUTLS_FUNC(gnutls_deinit)	\
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
	GNUTLS_FUNC(gnutls_certificate_set_x509_key_file)	\
	GNUTLS_VERIFYFUNCS	\
	GNUTLS_FUNC(gnutls_certificate_type_get)	\
	GNUTLS_FUNC(gnutls_free)	\
	GNUTLS_FUNC(gnutls_server_name_set)	\
	GNUTLS_DTLS_STUFF	\
	GNUTLS_X509_STUFF

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
		{(void**)&qgnutls_credentials_set, "gnutls_credentials_set"},
//		{(void**)&qgnutls_kx_set_priority, "gnutls_kx_set_priority"},
		{(void**)&qgnutls_init, "gnutls_init"},
		{(void**)&qgnutls_deinit, "gnutls_deinit"},
		{(void**)&qgnutls_set_default_priority, "gnutls_set_default_priority"},
		{(void**)&qgnutls_certificate_allocate_credentials, "gnutls_certificate_allocate_credentials"},
		{(void**)&qgnutls_anon_allocate_client_credentials, "gnutls_anon_allocate_client_credentials"},
		{(void**)&qgnutls_global_init, "gnutls_global_init"},
		{(void**)&qgnutls_record_send, "gnutls_record_send"},
		{(void**)&qgnutls_record_recv, "gnutls_record_recv"},

		{(void**)&qgnutls_certificate_set_verify_function, "gnutls_certificate_set_verify_function"},
		{(void**)&qgnutls_session_get_ptr, "gnutls_session_get_ptr"},
		{(void**)&qgnutls_session_set_ptr, "gnutls_session_set_ptr"},
		{(void**)&qgnutls_session_channel_binding, "gnutls_session_channel_binding"},
#ifdef GNUTLS_HAVE_SYSTEMTRUST
		{(void**)&qgnutls_certificate_set_x509_system_trust, "gnutls_certificate_set_x509_system_trust"},
#else
		{(void**)&qgnutls_certificate_set_x509_trust_file, "gnutls_certificate_set_x509_trust_file"},
#endif
		{(void**)&qgnutls_certificate_set_x509_key_file, "gnutls_certificate_set_x509_key_file"},
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
		{(void**)&qgnutls_malloc, "gnutls_malloc"},
		{(void**)&qgnutls_free, "gnutls_free"},
		{(void**)&qgnutls_server_name_set, "gnutls_server_name_set"},

#ifdef HAVE_DTLS
		{(void**)&qgnutls_key_generate, "gnutls_key_generate"},
		{(void**)&qgnutls_transport_set_pull_timeout_function, "gnutls_transport_set_pull_timeout_function"},
		{(void**)&qgnutls_dtls_cookie_verify, "gnutls_dtls_cookie_verify"},
		{(void**)&qgnutls_dtls_cookie_send, "gnutls_dtls_cookie_send"},
		{(void**)&qgnutls_dtls_prestate_set, "gnutls_dtls_prestate_set"},
		{(void**)&qgnutls_dtls_set_mtu, "gnutls_dtls_set_mtu"},
#endif

		{(void**)&qgnutls_sec_param_to_pk_bits, "gnutls_sec_param_to_pk_bits"},
		{(void**)&qgnutls_x509_crt_init, "gnutls_x509_crt_init"},
		{(void**)&qgnutls_x509_crt_deinit, "gnutls_x509_crt_deinit"},
		{(void**)&qgnutls_x509_crt_set_version, "gnutls_x509_crt_set_version"},
		{(void**)&qgnutls_x509_crt_set_activation_time, "gnutls_x509_crt_set_activation_time"},
		{(void**)&qgnutls_x509_crt_set_expiration_time, "gnutls_x509_crt_set_expiration_time"},
		{(void**)&qgnutls_x509_crt_set_serial, "gnutls_x509_crt_set_serial"},
		{(void**)&qgnutls_x509_crt_set_dn, "gnutls_x509_crt_set_dn"},
		{(void**)&qgnutls_x509_crt_set_issuer_dn, "gnutls_x509_crt_set_issuer_dn"},
		{(void**)&qgnutls_x509_crt_set_key, "gnutls_x509_crt_set_key"},
		{(void**)&qgnutls_x509_crt_export2, "gnutls_x509_crt_export2"},
		{(void**)&qgnutls_x509_privkey_init, "gnutls_x509_privkey_init"},
		{(void**)&qgnutls_x509_privkey_deinit, "gnutls_x509_privkey_deinit"},
		{(void**)&qgnutls_x509_privkey_generate, "gnutls_x509_privkey_generate"},
		{(void**)&qgnutls_x509_privkey_export2, "gnutls_x509_privkey_export2"},
		{(void**)&qgnutls_x509_crt_privkey_sign, "gnutls_x509_crt_privkey_sign"},
		{(void**)&qgnutls_privkey_init, "gnutls_privkey_init"},
		{(void**)&qgnutls_privkey_deinit, "gnutls_privkey_deinit"},
		{(void**)&qgnutls_privkey_import_x509, "gnutls_privkey_import_x509"},
		{(void**)&qgnutls_certificate_set_x509_key_mem, "gnutls_certificate_set_x509_key_mem"},

		{(void**)&qgnutls_certificate_get_x509_key, "gnutls_certificate_get_x509_key"},
		{(void**)&qgnutls_certificate_free_credentials, "gnutls_certificate_free_credentials"},
		{(void**)&qgnutls_pubkey_init, "gnutls_pubkey_init"},
		{(void**)&qgnutls_pubkey_import_x509, "gnutls_pubkey_import_x509"},
		{(void**)&qgnutls_privkey_sign_hash, "gnutls_privkey_sign_hash"},
		{(void**)&qgnutls_pubkey_verify_hash2, "gnutls_pubkey_verify_hash2"},
		{(void**)&qgnutls_x509_crt_import, "gnutls_x509_crt_import"},
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

typedef struct 
{
	vfsfile_t funcs;
	vfsfile_t *stream;

	char certname[512];
	gnutls_session_t session;

	qboolean handshaking;
	qboolean datagram;

	qboolean challenging;	//not sure this is actually needed, but hey.
	void *cbctx;
	neterr_t(*cbpush)(void *cbctx, const qbyte *data, size_t datasize);
	qbyte *readdata;
	size_t readsize;
#ifdef HAVE_DTLS
	gnutls_dtls_prestate_st prestate;
#endif
//	int mtu;
} gnutlsfile_t;

#define CAFILE "/etc/ssl/certs/ca-certificates.crt"

static void SSL_Close(vfsfile_t *vfs)
{
	gnutlsfile_t *file = (void*)vfs;

	file->handshaking = true;	//so further attempts to use it will fail.

	if (file->session)
	{
		qgnutls_bye (file->session, file->datagram?GNUTLS_SHUT_WR:GNUTLS_SHUT_RDWR);
		qgnutls_deinit(file->session);
		file->session = NULL;
	}
}
static qboolean QDECL SSL_CloseFile(vfsfile_t *vfs)
{
	gnutlsfile_t *file = (void*)vfs;
	SSL_Close(vfs);
	if (file->stream)
	{
		VFS_CLOSE(file->stream);
		file->stream = NULL;
	}
	Z_Free(vfs);
	return true;
}

static qboolean SSL_CheckUserTrust(gnutls_session_t session, gnutlsfile_t *file, int *errorcode)
{
#ifdef HAVE_CLIENT
	//when using dtls, we expect self-signed certs and persistent trust.
	if (file->datagram)
	{
		qbyte *certdata;
		size_t certsize;
		unsigned int certcount, j;
		const gnutls_datum_t *const certlist = qgnutls_certificate_get_peers(session, &certcount);
		for (certsize = 0, j = 0; j < certcount; j++)
			certsize += certlist[j].size;
		certdata = malloc(certsize);
		for (certsize = 0, j = 0; j < certcount; j++)
		{
			memcpy(certdata+certsize, certlist[j].data, certlist[j].size);
			certsize += certlist[j].size;
		}
		if (CertLog_ConnectOkay(file->certname, certdata, certsize))
			*errorcode = 0;	//user has previously authorised it.
		else
			*errorcode = GNUTLS_E_CERTIFICATE_ERROR;	//user didn't trust it yet
		free(certdata);
		return true;
	}
#endif

	return false;
}

static int QDECL SSL_CheckCert(gnutls_session_t session)
{
	gnutlsfile_t *file = qgnutls_session_get_ptr (session);
	unsigned int certstatus;
	qboolean preverified = false;
	int errcode = GNUTLS_E_CERTIFICATE_ERROR;

	size_t knownsize;
	qbyte *knowndata = TLS_GetKnownCertificate(file->certname, &knownsize);

	if (knowndata)
	{
		unsigned int certcount, j;
		const gnutls_datum_t *const certlist = qgnutls_certificate_get_peers(session, &certcount);
		if (!certlist || !certcount)
		{
			BZ_Free(knowndata);
			return GNUTLS_E_CERTIFICATE_ERROR;
		}
		else
		{
			size_t offset = 0;

			for (j = 0; j < certcount; offset += certlist[j++].size)
			{
				if (certlist[j].size+offset > knownsize)
					break;	//overflow...
				if (memcmp(certlist[j].data, knowndata+offset, certlist[j].size))
					break;
			}

			if (j && j == certcount && offset == knownsize)
				preverified = true;
			else
			{
#ifdef _DEBUG
				for (j = 0, offset = 0; j < certcount; j++)
					offset += certlist[j].size;
				Con_Printf("%s cert %zu bytes (chain %u)\n", file->certname, offset, certcount);
				Con_Printf("/*%s*/\"", file->certname);
				for (j = 0; file->certname[j]; j++)
					Con_Printf("\\x%02x", file->certname[j]^0xff);
				Con_Printf("\\xff");
				Con_Printf("\\x%02x\\x%02x", (unsigned)offset&0xff, ((unsigned)offset>>8)&0xff);
				for (j = 0; j < certcount; j++)
				{
					unsigned char *data = certlist[j].data;
					unsigned int datasize = certlist[j].size, k;
					for (k = 0; k < datasize; k++)
						Con_Printf("\\x%02x", data[k]^0xff);
				}
				Con_Printf("\",\n\n");
#endif
				Con_Printf(CON_ERROR "%s: Reported certificate does not match known certificate. Possible MITM attack, alternatively just an outdated client.\n", file->certname);
				BZ_Free(knowndata);
				return GNUTLS_E_CERTIFICATE_ERROR;
			}
		}
		BZ_Free(knowndata);
	}

#ifdef GNUTLS_HAVE_VERIFY3
	if (qgnutls_certificate_verify_peers3(session, file->certname, &certstatus) >= 0)
	{
		{
			gnutls_datum_t out;
			gnutls_certificate_type_t type;

			if (preverified && (certstatus&~GNUTLS_CERT_EXPIRED) == (GNUTLS_CERT_INVALID|GNUTLS_CERT_SIGNER_NOT_FOUND))
				return 0;
			if (certstatus == 0)
				return SSL_CheckUserTrust(session, file, 0);
			if (certstatus == (GNUTLS_CERT_INVALID|GNUTLS_CERT_SIGNER_NOT_FOUND))
			{
				if (SSL_CheckUserTrust(session, file, &errcode))
					return errcode;
			}

			type = qgnutls_certificate_type_get (session);
			if (qgnutls_certificate_verification_status_print(certstatus, type, &out, 0) >= 0)
			{
				Con_Printf(CON_ERROR "%s: %s (%x)\n", file->certname, out.data, certstatus);
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
				if (preverified && (certstatus&~GNUTLS_CERT_EXPIRED) == (GNUTLS_CERT_INVALID|GNUTLS_CERT_SIGNER_NOT_FOUND))
					return 0;
				if (certstatus == 0)
					return SSL_CheckUserTrust(session, file, 0);
				if (certstatus == (GNUTLS_CERT_INVALID|GNUTLS_CERT_SIGNER_NOT_FOUND) && SSL_CheckUserTrust(session, file, GNUTLS_E_CERTIFICATE_ERROR))
					return 0;

				if (certstatus & GNUTLS_CERT_SIGNER_NOT_FOUND)
					Con_Printf(CON_ERROR "%s: Certificate authority is not recognised\n", file->certname);
				else if (certstatus & GNUTLS_CERT_INSECURE_ALGORITHM)
					Con_Printf(CON_ERROR "%s: Certificate uses insecure algorithm\n", file->certname);
				else if (certstatus & (GNUTLS_CERT_REVOCATION_DATA_ISSUED_IN_FUTURE|GNUTLS_CERT_REVOCATION_DATA_SUPERSEDED|GNUTLS_CERT_EXPIRED|GNUTLS_CERT_REVOKED|GNUTLS_CERT_NOT_ACTIVATED))
					Con_Printf(CON_ERROR "%s: Certificate has expired or was revoked or not yet valid\n", file->certname);
				else if (certstatus & GNUTLS_CERT_SIGNATURE_FAILURE)
					Con_Printf(CON_ERROR "%s: Certificate signature failure\n", file->certname);
				else
					Con_Printf(CON_ERROR "%s: Certificate error\n", file->certname);
#endif
				if (tls_ignorecertificateerrors.ival)
				{
					Con_Printf(CON_ERROR "%s: Ignoring certificate errors (tls_ignorecertificateerrors is %i)\n", file->certname, tls_ignorecertificateerrors.ival);
					return 0;
				}
			}
			else
				Con_DPrintf(CON_ERROR "%s: certificate is for a different domain\n", file->certname);
		}
	}

	Con_DPrintf(CON_ERROR "%s: rejecting certificate\n", file->certname);
	return errcode;
}

//return 1 to read data.
//-1 for error
//0 for not ready
static int SSL_DoHandshake(gnutlsfile_t *file)
{
	int err;
	//session was previously closed = error
	if (!file->session)
	{
		//Sys_Printf("null session\n");
		return -1;
	}

	err = qgnutls_handshake (file->session);
	if (err < 0)
	{	//non-fatal errors can just handshake again the next time the caller checks to see if there's any data yet
		//(e_again or e_intr)
		if (!qgnutls_error_is_fatal(err))
			return 0;
		if (developer.ival)
		{
			if (err == GNUTLS_E_FATAL_ALERT_RECEIVED)
				;	//peer doesn't like us.
			else
				//we didn't like the peer.
				qgnutls_perror (err);
		}

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

	if (!bytestoread)	//gnutls doesn't like this.
		return -1;

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
			Con_Printf("TLS Read Warning %i (bufsize %i)\n", read, bytestoread);
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
			Con_DPrintf("TLS Send Error %i (%i bytes)\n", written, bytestowrite);
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
//	Sys_Printf("SSL_Push: %u\n", size);
	int done = VFS_WRITE(file->stream, data, size);
	if (done <= 0)
	{
		qgnutls_transport_set_errno(file->session, (done==0)?EAGAIN:ECONNRESET);
		return -1;
	}
	return done;
}
static ssize_t SSL_Pull(gnutls_transport_ptr_t p, void *data, size_t size)
{
	gnutlsfile_t *file = p;
//	Sys_Printf("SSL_Pull: %u\n", size);
	int done = VFS_READ(file->stream, data, size);
	if (done <= 0)
	{
		//use ECONNRESET instead of returning eof.
		qgnutls_transport_set_errno(file->session, (done==0)?EAGAIN:ECONNRESET);
		return -1;
	}
	return done;
}

static ssize_t DTLS_Push(gnutls_transport_ptr_t p, const void *data, size_t size)
{
	gnutlsfile_t *file = p;

	neterr_t ne = file->cbpush(file->cbctx, data, size);

//	Sys_Printf("DTLS_Push: %u, err=%i\n", (unsigned)size, (int)ne);

	switch(ne)
	{
	case NETERR_CLOGGED:
		qgnutls_transport_set_errno(file->session, EAGAIN);
		return -1;
	case NETERR_MTU:
		qgnutls_transport_set_errno(file->session, EMSGSIZE);
		return -1;
	case NETERR_DISCONNECTED:
		qgnutls_transport_set_errno(file->session, EPERM);
		return -1;
	default:
		qgnutls_transport_set_errno(file->session, 0);
		return size;
	}
}
static ssize_t DTLS_Pull(gnutls_transport_ptr_t p, void *data, size_t size)
{
	gnutlsfile_t *file = p;

//	Sys_Printf("DTLS_Pull: %u of %u\n", size, file->readsize);

	if (!file->readsize)
	{	//no data left
//		Sys_Printf("DTLS_Pull: EAGAIN\n");
		qgnutls_transport_set_errno(file->session, EAGAIN);
		return -1;
	}
	else if (file->readsize > size)
	{	//buffer passed is smaller than available data
//		Sys_Printf("DTLS_Pull: EMSGSIZE\n");
		memcpy(data, file->readdata, size);
		file->readsize = 0;
		qgnutls_transport_set_errno(file->session, EMSGSIZE);
		return -1;
	}
	else
	{	//buffer is big enough to read it all
		size = file->readsize;
		file->readsize = 0;
//		Sys_Printf("DTLS_Pull: reading %i\n", size);
		memcpy(data, file->readdata, size);
		qgnutls_transport_set_errno(file->session, 0);
		return size;
	}
}
#ifdef HAVE_DTLS
static int DTLS_Pull_Timeout(gnutls_transport_ptr_t p, unsigned int timeout)
{	//gnutls (pointlessly) requires this function for dtls.
	gnutlsfile_t *f = p;
//	Sys_Printf("DTLS_Pull_Timeout %i, %i\n", timeout, f->readsize);
	return f->readsize>0?1:0;
}
#endif

#ifdef USE_ANON
static gnutls_anon_client_credentials_t anoncred[2];
#else
static gnutls_certificate_credentials_t xcred[2];
#endif
#ifdef HAVE_DTLS
static gnutls_datum_t cookie_key;
#endif

vfsfile_t *SSL_OpenPrivKey(char *nativename, size_t nativesize)
{
#define privname "privkey.pem"
	vfsfile_t *privf;
	const char *mode = nativename?"wb":"rb";
	int i = COM_CheckParm("-privkey");
	if (i++)
	{
		if (nativename)
			Q_strncpyz(nativename, com_argv[i], nativesize);
		privf = FS_OpenVFS(com_argv[i], mode, FS_SYSTEM);
	}
	else
	{
		if (nativename)
			if (!FS_NativePath(privname, FS_ROOT, nativename, nativesize))
				return NULL;

		privf = FS_OpenVFS(privname, mode, FS_ROOT);
	}
	return privf;
#undef privname
}
vfsfile_t *SSL_OpenPubKey(char *nativename, size_t nativesize)
{
#define pubname "cert.pem"
	vfsfile_t *pubf;
	const char *mode = nativename?"wb":"rb";
	int i = COM_CheckParm("-pubkey");
	if (i++)
	{
		if (nativename)
			Q_strncpyz(nativename, com_argv[i], nativesize);
		pubf = FS_OpenVFS(com_argv[i], mode, FS_SYSTEM);
	}
	else
	{
		if (nativename)
			if (!FS_NativePath(pubname, FS_ROOT, nativename, nativesize))
				return NULL;
		pubf = FS_OpenVFS(pubname, mode, FS_ROOT);
	}
	return pubf;
#undef pubname
}

static qboolean SSL_LoadPrivateCert(gnutls_certificate_credentials_t cred)
{
	int ret = -1;
	gnutls_datum_t priv, pub;
	vfsfile_t *privf = SSL_OpenPrivKey(NULL, 0);
	vfsfile_t *pubf = SSL_OpenPubKey(NULL, 0);
	const char *hostname = NULL;

	int i = COM_CheckParm("-certhost");
	if (i)
		hostname = com_argv[i+1];

	memset(&priv, 0, sizeof(priv));
	memset(&pub, 0, sizeof(pub));

	if ((!privf || !pubf) && hostname)
	{	//not found? generate a new one.
		//FIXME: how to deal with race conditions with multiple servers on the same host?
		//delay till the first connection? we at least write both files at the sameish time.
		//even so they might get different certs the first time the server(s) run.
		//TODO: implement a lockfile
		gnutls_x509_privkey_t key;
		gnutls_x509_crt_t cert;
		char serial[64];
		const char *errstr;
		gnutls_pk_algorithm_t privalgo = GNUTLS_PK_RSA;

		if (privf)VFS_CLOSE(privf);privf=NULL;
		if (pubf)VFS_CLOSE(pubf);pubf=NULL;

		Con_Printf("Generating new GNUTLS key+cert...\n");

		qgnutls_x509_privkey_init(&key);
		ret = qgnutls_x509_privkey_generate(key, privalgo, qgnutls_sec_param_to_pk_bits(privalgo, GNUTLS_SEC_PARAM_HIGH), 0);
		if (ret < 0)
			Con_Printf("gnutls_x509_privkey_generate failed: %i\n", ret);
		ret = qgnutls_x509_privkey_export2(key, GNUTLS_X509_FMT_PEM, &priv);
		if (ret < 0)
			Con_Printf("gnutls_x509_privkey_export2 failed: %i\n", ret);

		//stoopid browsers insisting that serial numbers are different even on throw-away self-signed certs.
		//we should probably just go and make our own root ca/master. post it a cert and get a signed one (with sequential serial) back or something.
		//we'll probably want something like that for client certs anyway, for stat tracking.
		Q_snprintfz(serial, sizeof(serial), "%u", (unsigned)time(NULL));

		qgnutls_x509_crt_init(&cert);
		qgnutls_x509_crt_set_version(cert, 1);
		qgnutls_x509_crt_set_activation_time(cert, time(NULL)-1);
		qgnutls_x509_crt_set_expiration_time(cert, time(NULL)+(time_t)10*365*24*60*60);
		qgnutls_x509_crt_set_serial(cert, serial, strlen(serial));
		if (!hostname)
			/*qgnutls_x509_crt_set_key_usage(cert, GNUTLS_KEY_DIGITAL_SIGNATURE)*/;
		else
		{
			if (qgnutls_x509_crt_set_dn(cert, va("CN=%s", hostname), &errstr) < 0)
				Con_Printf("gnutls_x509_crt_set_dn failed: %s\n", errstr);
			if (qgnutls_x509_crt_set_issuer_dn(cert, va("CN=%s", hostname), &errstr) < 0)
				Con_Printf("gnutls_x509_crt_set_issuer_dn failed: %s\n", errstr);
//			qgnutls_x509_crt_set_key_usage(cert, GNUTLS_KEY_KEY_ENCIPHERMENT|GNUTLS_KEY_DATA_ENCIPHERMENT|);
		}
		qgnutls_x509_crt_set_key(cert, key);

		/*sign it with our private key*/
		{
			gnutls_privkey_t akey;
			qgnutls_privkey_init(&akey);
			qgnutls_privkey_import_x509(akey, key, GNUTLS_PRIVKEY_IMPORT_COPY);
			ret = qgnutls_x509_crt_privkey_sign(cert, cert, akey, GNUTLS_DIG_SHA256, 0);
			if (ret < 0)
				Con_Printf("gnutls_x509_crt_privkey_sign failed: %i\n", ret);
			qgnutls_privkey_deinit(akey);
		}
		ret = qgnutls_x509_crt_export2(cert, GNUTLS_X509_FMT_PEM, &pub);
		qgnutls_x509_crt_deinit(cert);
		qgnutls_x509_privkey_deinit(key);
		if (ret < 0)
			Con_Printf("gnutls_x509_crt_export2 failed: %i\n", ret);

		if (priv.size && pub.size)
		{
			char fullname[MAX_OSPATH];
			privf = SSL_OpenPrivKey(fullname, sizeof(fullname));
			if (privf)
			{
				VFS_WRITE(privf, priv.data, priv.size);
				VFS_CLOSE(privf);
				Con_Printf("Wrote %s\n", fullname);
			}
//			memset(priv.data, 0, priv.size);
			(*qgnutls_free)(priv.data);
			memset(&priv, 0, sizeof(priv));

			pubf = SSL_OpenPubKey(fullname, sizeof(fullname));
			if (pubf)
			{
				VFS_WRITE(pubf, pub.data, pub.size);
				VFS_CLOSE(pubf);
				Con_Printf("Wrote %s\n", fullname);
			}
			(*qgnutls_free)(pub.data);
			memset(&pub, 0, sizeof(pub));

			privf = SSL_OpenPrivKey(NULL, 0);
			pubf = SSL_OpenPubKey(NULL, 0);

			Con_Printf("Certificate generated\n");
		}
	}

	if (privf && pubf)
	{
		//read the two files now
		priv.size = VFS_GETLEN(privf);
		priv.data = (*qgnutls_malloc)(priv.size+1);
		if (priv.size != VFS_READ(privf, priv.data, priv.size))
			priv.size = 0;
		priv.data[priv.size] = 0;

		pub.size = VFS_GETLEN(pubf);
		pub.data = (*qgnutls_malloc)(pub.size+1);
		if (pub.size != VFS_READ(pubf, pub.data, pub.size))
			pub.size = 0;
		pub.data[pub.size] = 0;
	}

	//FIXME: extend the expiration time if its old?

	if (priv.size && pub.size)
	{	//submit them to gnutls
		ret = qgnutls_certificate_set_x509_key_mem(cred, &pub, &priv, GNUTLS_X509_FMT_PEM);
		if (ret < 0)
			Con_Printf("gnutls_certificate_set_x509_key_mem failed: %i\n", ret);
	}
	else
		Con_Printf("Unable to read/generate cert ('-certhost HOSTNAME' commandline arguments to autogenerate one)\n");

	memset(priv.data, 0, priv.size);//just in case. FIXME: we didn't scrub the filesystem code. libc has its own caches etc. lets hope that noone comes up with some way to scrape memory remotely (although if they can inject code then we've lost either way so w/e)
	if (priv.data)
		(*qgnutls_free)(priv.data);
	if (pub.data)
		(*qgnutls_free)(pub.data);

	return ret>=0;
}

qboolean SSL_InitGlobal(qboolean isserver)
{
	static int initstatus[2];
	isserver = !!isserver;
	if (COM_CheckParm("-notls"))
		return false;
#ifdef LOADERTHREAD
	if (com_resourcemutex)
		Sys_LockMutex(com_resourcemutex);
#endif
	if (!initstatus[isserver])
	{
		if (!Init_GNUTLS())
		{
#ifdef LOADERTHREAD
			if (com_resourcemutex)
				Sys_UnlockMutex(com_resourcemutex);
#endif
			Con_Printf("GnuTLS "GNUTLS_VERSION" library not available.\n");
			return false;
		}
		initstatus[isserver] = true;
		qgnutls_global_init ();

#ifdef HAVE_DTLS
		if (isserver)
			qgnutls_key_generate(&cookie_key, GNUTLS_COOKIE_KEY_SIZE);
#endif


#ifdef USE_ANON
		qgnutls_anon_allocate_client_credentials (&anoncred[isserver]);
#else

		qgnutls_certificate_allocate_credentials (&xcred[isserver]);

#ifdef GNUTLS_HAVE_SYSTEMTRUST
		qgnutls_certificate_set_x509_system_trust (xcred[isserver]);
#else
		qgnutls_certificate_set_x509_trust_file (xcred[isserver], CAFILE, GNUTLS_X509_FMT_PEM);
#endif

#ifdef LOADERTHREAD
		if (com_resourcemutex)
			Sys_UnlockMutex(com_resourcemutex);
#endif
		if (isserver)
		{
#if 1
			if (!SSL_LoadPrivateCert(xcred[isserver]))
				initstatus[isserver] = -1;
#else
			int ret = -1;
			char keyfile[MAX_OSPATH];
			char certfile[MAX_OSPATH];
			*keyfile = *certfile = 0;
			if (FS_NativePath("key.pem", FS_ROOT, keyfile, sizeof(keyfile)))
				if (FS_NativePath("cert.pem", FS_ROOT, certfile, sizeof(certfile)))
					ret = qgnutls_certificate_set_x509_key_file(xcred[isserver], certfile, keyfile, GNUTLS_X509_FMT_PEM);
			if (ret < 0)
			{
				Con_Printf("No certificate or key was found in %s and %s\n", certfile, keyfile);
				initstatus[isserver] = -1;
			}
#endif
		}
		else
			qgnutls_certificate_set_verify_function (xcred[isserver], SSL_CheckCert);
#endif
	}
	else
	{
#ifdef LOADERTHREAD
		if (com_resourcemutex)
			Sys_UnlockMutex(com_resourcemutex);
#endif
	}

	if (initstatus[isserver] < 0)
		return false;
	return true;
}
static qboolean SSL_InitConnection(gnutlsfile_t *newf, qboolean isserver, qboolean datagram)
{
	// Initialize TLS session
	qgnutls_init (&newf->session, GNUTLS_NONBLOCK|(isserver?GNUTLS_SERVER:GNUTLS_CLIENT)|(datagram?GNUTLS_DATAGRAM:0));

	if (!isserver)
		qgnutls_server_name_set(newf->session, GNUTLS_NAME_DNS, newf->certname, strlen(newf->certname));
	/*else
	{
		size_t size = sizeof(newf->certname);
		unsigned int type = GNUTLS_NAME_DNS;
		int err;
		err=qgnutls_server_name_get(newf->session, newf->certname, &size, &type, 0);
		if (err!=GNUTLS_E_SUCCESS)
			*newf->certname = 0;
	}*/

	qgnutls_session_set_ptr(newf->session, newf);

#ifdef USE_ANON
	//qgnutls_kx_set_priority (newf->session, kx_prio);
	qgnutls_credentials_set (newf->session, GNUTLS_CRD_ANON, anoncred[isserver]);
#else
//#if GNUTLS_VERSION_MAJOR >= 3
	//gnutls_priority_set_direct();
//#else
	//qgnutls_certificate_type_set_priority (newf->session, cert_type_priority);
//#endif
	qgnutls_credentials_set (newf->session, GNUTLS_CRD_CERTIFICATE, xcred[isserver]);
#endif
	// Use default priorities
	qgnutls_set_default_priority (newf->session);

	// tell gnutls how to send/receive data
	qgnutls_transport_set_ptr (newf->session, newf);
	qgnutls_transport_set_push_function(newf->session, datagram?DTLS_Push:SSL_Push);
	//qgnutls_transport_set_vec_push_function(newf->session, SSL_PushV);
	qgnutls_transport_set_pull_function(newf->session, datagram?DTLS_Pull:SSL_Pull);
#ifdef HAVE_DTLS
	if (datagram)
		qgnutls_transport_set_pull_timeout_function(newf->session, DTLS_Pull_Timeout);
#endif

//	if (isserver)	//don't bother to auth any client certs
//		qgnutls_certificate_server_set_request(newf->session, GNUTLS_CERT_IGNORE);

	newf->handshaking = true;

	return true;
}

vfsfile_t *GNUTLS_OpenVFS(const char *hostname, vfsfile_t *source, qboolean isserver)
{
	gnutlsfile_t *newf;

	if (!source)
		return NULL;

	if (!SSL_InitGlobal(isserver))
		newf = NULL;
	else
		newf = Z_Malloc(sizeof(*newf));
	if (!newf)
	{
		return NULL;
	}
	newf->funcs.Close = SSL_CloseFile;
	newf->funcs.Flush = NULL;
	newf->funcs.GetLen = SSL_GetLen;
	newf->funcs.ReadBytes = SSL_Read;
	newf->funcs.WriteBytes = SSL_Write;
	newf->funcs.Seek = SSL_Seek;
	newf->funcs.Tell = SSL_Tell;
	newf->funcs.seekstyle = SS_UNSEEKABLE;

	if (hostname)
		Q_strncpyz(newf->certname, hostname, sizeof(newf->certname));
	else
		Q_strncpyz(newf->certname, "", sizeof(newf->certname));

	if (!SSL_InitConnection(newf, isserver, false))
	{
		VFS_CLOSE(&newf->funcs);
		return NULL;
	}
	newf->stream = source;

	return &newf->funcs;
}

int GNUTLS_GetChannelBinding(vfsfile_t *vf, qbyte *binddata, size_t *bindsize)
{
	gnutls_datum_t cb;
	gnutlsfile_t *f = (gnutlsfile_t*)vf;
	if (vf->Close != SSL_CloseFile)
		return -1;	//err, not a tls connection.

	if (qgnutls_session_channel_binding(f->session, GNUTLS_CB_TLS_UNIQUE, &cb))
	{	//error of some kind
		//if the error is because of the other side not supporting it, then we should return 0 here.
		return -1;
	}
	else
	{
		if (cb.size > *bindsize)
			return 0;	//overflow
		*bindsize = cb.size;
		memcpy(binddata, cb.data, cb.size);
		return 1;
	}
}

//generates a signed blob
int GNUTLS_GenerateSignature(qbyte *hashdata, size_t hashsize, qbyte *signdata, size_t signsizemax)
{
	gnutls_datum_t hash = {hashdata, hashsize};
	gnutls_datum_t sign = {NULL, 0};

	gnutls_certificate_credentials_t cred;
	if (Init_GNUTLS())
	{
		qgnutls_certificate_allocate_credentials (&cred);
		if (SSL_LoadPrivateCert(cred))
		{
			gnutls_x509_privkey_t xkey;
			gnutls_privkey_t privkey;
			qgnutls_privkey_init(&privkey);
			qgnutls_certificate_get_x509_key(cred, 0, &xkey);
			qgnutls_privkey_import_x509(privkey, xkey, 0);

			qgnutls_privkey_sign_hash(privkey, GNUTLS_DIG_SHA512, 0, &hash, &sign);
			qgnutls_privkey_deinit(privkey);
		}
		else
			sign.size = 0;
		qgnutls_certificate_free_credentials(cred);
	}
	else
		Con_Printf("Unable to init gnutls\n");
	memcpy(signdata, sign.data, sign.size);
	return sign.size;
}

//windows equivelent https://docs.microsoft.com/en-us/windows/win32/seccrypto/example-c-program-signing-a-hash-and-verifying-the-hash-signature
enum hashvalidation_e GNUTLS_VerifyHash(qbyte *hashdata, size_t hashsize, const char *authority, qbyte *signdata, size_t signsize)
{
	gnutls_datum_t hash = {hashdata, hashsize};
	gnutls_datum_t sign = {signdata, signsize};
	int r;

	gnutls_datum_t rawcert;
#if 1
	size_t sz;
	gnutls_pubkey_t pubkey;
	gnutls_x509_crt_t cert;

	rawcert.data = Auth_GetKnownCertificate(authority, &sz);
	if (!rawcert.data)
		return VH_AUTHORITY_UNKNOWN;
	if (!Init_GNUTLS())
		return VH_UNSUPPORTED;
	rawcert.size = sz;

	qgnutls_pubkey_init(&pubkey);
	qgnutls_x509_crt_init(&cert);
	qgnutls_x509_crt_import(cert, &rawcert, GNUTLS_X509_FMT_PEM);

	qgnutls_pubkey_import_x509(pubkey, cert, 0);
#else
	qgnutls_pubkey_import(pubkey, rawcert, GNUTLS_X509_FMT_PEM);
#endif

	r = qgnutls_pubkey_verify_hash2(pubkey, GNUTLS_SIGN_RSA_SHA512, 0, &hash, &sign);
	if (r < 0)
	{
		if (r == GNUTLS_E_PK_SIG_VERIFY_FAILED)
		{
			Con_Printf("GNUTLS_VerifyHash: GNUTLS_E_PK_SIG_VERIFY_FAILED!\n");
			return VH_INCORRECT;
		}
		else if (r == GNUTLS_E_INSUFFICIENT_SECURITY)
		{
			Con_Printf("GNUTLS_VerifyHash: GNUTLS_E_INSUFFICIENT_SECURITY\n");
			return VH_AUTHORITY_UNKNOWN;	//should probably be incorrect or something, but oh well
		}
		return VH_INCORRECT;
	}
	else
		return VH_CORRECT;
}

#ifdef HAVE_DTLS

static void GNUDTLS_DestroyContext(void *ctx)
{
	SSL_Close(ctx);
}
static void *GNUDTLS_CreateContext(const char *remotehost, void *cbctx, neterr_t(*push)(void *cbctx, const qbyte *data, size_t datasize), qboolean isserver)
{
	gnutlsfile_t *newf;

	if (!SSL_InitGlobal(isserver))
		newf = NULL;
	else
		newf = Z_Malloc(sizeof(*newf));
	if (!newf)
		return NULL;
	newf->datagram = true;
	newf->cbctx = cbctx;
	newf->cbpush = push;
	newf->challenging = isserver;

//	Sys_Printf("DTLS_CreateContext: server=%i\n", isserver);

	Q_strncpyz(newf->certname, remotehost?remotehost:"", sizeof(newf->certname));

	if (!SSL_InitConnection(newf, isserver, true))
	{
		SSL_Close(&newf->funcs);
		return NULL;
	}

	return newf;
}

static neterr_t GNUDTLS_Transmit(void *ctx, const qbyte *data, size_t datasize)
{
	int ret;
	gnutlsfile_t *f = (gnutlsfile_t *)ctx;
//	Sys_Printf("DTLS_Transmit: %u\n", datasize);
//	Sys_Printf("%su\n", data);

	if (f->challenging)
		return NETERR_CLOGGED;
	if (f->handshaking)
	{
		ret = SSL_DoHandshake(f);
		if (!ret)
			return NETERR_CLOGGED;
		if (ret < 0)
			return NETERR_DISCONNECTED;
	}

	ret = qgnutls_record_send(f->session, data, datasize);
	if (ret < 0)
	{
		if (ret == GNUTLS_E_LARGE_PACKET)
			return NETERR_MTU;
//Sys_Error("qgnutls_record_send returned %i\n", ret);

		if (qgnutls_error_is_fatal(ret))
			return NETERR_DISCONNECTED;
		return NETERR_CLOGGED;
	}
	return NETERR_SENT;
}

static neterr_t GNUDTLS_Received(void *ctx, qbyte *data, size_t datasize)
{
	int cli_addr = 0xdeadbeef;
	int ret;
	gnutlsfile_t *f = (gnutlsfile_t *)ctx;

//Sys_Printf("DTLS_Received: %u\n", datasize);

	if (f->challenging)
	{
		memset(&f->prestate, 0, sizeof(f->prestate));
		ret = qgnutls_dtls_cookie_verify(&cookie_key,
				&cli_addr, sizeof(cli_addr),
				data, datasize,
				&f->prestate);

		if (ret < 0)
		{
//Sys_Printf("Sending cookie\n");
			qgnutls_dtls_cookie_send(&cookie_key,
					&cli_addr, sizeof(cli_addr),
					&f->prestate,
					(gnutls_transport_ptr_t)f, DTLS_Push);
			return NETERR_CLOGGED;
		}
//Sys_Printf("Got correct cookie\n");
		f->challenging = false;

		qgnutls_dtls_prestate_set(f->session, &f->prestate);
		qgnutls_dtls_set_mtu(f->session, 1440);

//		qgnutls_transport_set_push_function(f->session, DTLS_Push);
//		qgnutls_transport_set_pull_function(f->session, DTLS_Pull);
		f->handshaking = true;
	}

	f->readdata = data;
	f->readsize = datasize;

	if (f->handshaking)
	{
		ret = SSL_DoHandshake(f);
		if (ret <= 0)
			f->readsize = 0;
		if (!ret)
			return NETERR_CLOGGED;
		if (ret < 0)
			return NETERR_DISCONNECTED;
	}

	ret = qgnutls_record_recv(f->session, net_message_buffer, sizeof(net_message_buffer));
//Sys_Printf("DTLS_Received returned %i of %i\n", ret, f->readsize);
	f->readsize = 0;
	if (ret <= 0)
	{
		if (!ret)
		{
//			Sys_Printf("DTLS_Received peer terminated connection\n");
			return NETERR_DISCONNECTED;
		}
		if (qgnutls_error_is_fatal(ret))
		{
//			Sys_Printf("DTLS_Received fail error\n");
			return NETERR_DISCONNECTED;
		}
//		Sys_Printf("DTLS_Received temp error\n");
		return NETERR_CLOGGED;
	}
	net_message.cursize = ret;
	data[ret] = 0;
//	Sys_Printf("DTLS_Received returned %s\n", data);
	return NETERR_SENT;
}

static neterr_t GNUDTLS_Timeouts(void *ctx)
{
	gnutlsfile_t *f = (gnutlsfile_t *)ctx;
	int ret;
	if (f->challenging)
		return NETERR_CLOGGED;
	if (f->handshaking)
	{
		f->readsize = 0;
		ret = SSL_DoHandshake(f);
		f->readsize = 0;
		if (!ret)
			return NETERR_CLOGGED;
		if (ret < 0)
			return NETERR_DISCONNECTED;

//		Sys_Printf("handshaking over?\n");
	}
	return NETERR_SENT;
}

static const dtlsfuncs_t dtlsfuncs_gnutls =
{
	GNUDTLS_CreateContext,
	GNUDTLS_DestroyContext,
	GNUDTLS_Transmit,
	GNUDTLS_Received,
	GNUDTLS_Timeouts,
};
const dtlsfuncs_t *GNUDTLS_InitServer(void)
{
	if (!SSL_InitGlobal(true))
		return NULL;	//unable to init a server certificate. don't allow dtls to init.
	return &dtlsfuncs_gnutls;
}
const dtlsfuncs_t *GNUDTLS_InitClient(void)
{
	return &dtlsfuncs_gnutls;
}
#endif

#else
#warning "GNUTLS version is too old (3.0+ required). Please clean and then recompile with CFLAGS=-DNO_GNUTLS"

qboolean SSL_InitGlobal(qboolean isserver) {return false;}
vfsfile_t *FS_OpenSSL(const char *hostname, vfsfile_t *source, qboolean isserver) {return NULL;}
int GNUTLS_GetChannelBinding(vfsfile_t *vf, qbyte *binddata, size_t *bindsize) {return -1;}
const dtlsfuncs_t *GNUDTLS_InitClient(void) {return NULL;}
const dtlsfuncs_t *GNUDTLS_InitServer(void) {return NULL;}
#endif
#endif

