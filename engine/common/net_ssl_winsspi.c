#include "quakedef.h"
#if defined(HAVE_WINSSPI)

/*regarding HAVE_DTLS
DTLS1.0 is supported from win8 onwards
Its also meant to be supported from some RDP server patch on win7, but I can't get it to work.
I've given up for now.
*/

cvar_t *tls_ignorecertificateerrors;

#include "winquake.h"
#define SECURITY_WIN32
#include <security.h>
#include <sspi.h>
#include <schannel.h>

#define SP_PROT_TLS1_1_SERVER		0x00000100
#define SP_PROT_TLS1_1_CLIENT		0x00000200

#define SP_PROT_TLS1_2_SERVER		0x00000400
#define SP_PROT_TLS1_2_CLIENT		0x00000800

#define SP_PROT_DTLS_SERVER		0x00010000
#define SP_PROT_DTLS_CLIENT		0x00020000

//avoid the use of outdated/insecure protocols
//so no ssl2/ssl3
#define USE_PROT_SERVER (SP_PROT_TLS1_SERVER | SP_PROT_TLS1_1_SERVER | SP_PROT_TLS1_2_SERVER)
#define USE_PROT_CLIENT (SP_PROT_TLS1_CLIENT | SP_PROT_TLS1_1_CLIENT | SP_PROT_TLS1_2_CLIENT)

#define USE_PROT_DGRAM_SERVER (SP_PROT_DTLS_SERVER)
#define USE_PROT_DGRAM_CLIENT (SP_PROT_DTLS_CLIENT)

#ifndef szOID_RSA_SHA512RSA
#define szOID_RSA_SHA512RSA "1.2.840.113549.1.1.13"
#endif
#ifndef SCH_CRED_SNI_CREDENTIAL
#define SCH_CRED_SNI_CREDENTIAL 0x00080000
#endif

#define SEC_I_MESSAGE_FRAGMENT	0x00090364L
#define SEC_E_INVALID_PARAMETER	0x8009035DL


//hungarian ensures we hit no macros.
static struct
{
	dllhandle_t *lib;
	SECURITY_STATUS (WINAPI *pDecryptMessage)				(PCtxtHandle,PSecBufferDesc,ULONG,PULONG);
	SECURITY_STATUS (WINAPI *pEncryptMessage)				(PCtxtHandle,ULONG,PSecBufferDesc,ULONG);
	SECURITY_STATUS (WINAPI *pAcquireCredentialsHandleA)	(SEC_CHAR*,SEC_CHAR*,ULONG,PLUID,PVOID,SEC_GET_KEY_FN,PVOID,PCredHandle,PTimeStamp);
//	SECURITY_STATUS (WINAPI *pInitializeSecurityContextA)	(PCredHandle,PCtxtHandle,SEC_CHAR*,ULONG,ULONG,ULONG,PSecBufferDesc,ULONG,PCtxtHandle,PSecBufferDesc,PULONG,PTimeStamp);
	SECURITY_STATUS (WINAPI *pInitializeSecurityContextW)	(PCredHandle,PCtxtHandle,SEC_WCHAR*,ULONG,ULONG,ULONG,PSecBufferDesc,ULONG,PCtxtHandle,PSecBufferDesc,PULONG,PTimeStamp);
	SECURITY_STATUS (WINAPI *pAcceptSecurityContext)		(PCredHandle,PCtxtHandle,PSecBufferDesc,unsigned long,unsigned long,PCtxtHandle,PSecBufferDesc,unsigned long SEC_FAR *,PTimeStamp);
	SECURITY_STATUS (WINAPI *pCompleteAuthToken)			(PCtxtHandle,PSecBufferDesc);
	SECURITY_STATUS (WINAPI *pQueryContextAttributesA)		(PCtxtHandle,ULONG,PVOID);
	SECURITY_STATUS (WINAPI *pFreeCredentialsHandle)		(PCredHandle);
	SECURITY_STATUS (WINAPI *pDeleteSecurityContext)		(PCtxtHandle);
} secur;
static struct
{
	dllhandle_t *lib;
	BOOL (WINAPI *pCertGetCertificateChain)					(HCERTCHAINENGINE,PCCERT_CONTEXT,LPFILETIME,HCERTSTORE,PCERT_CHAIN_PARA,DWORD,LPVOID,PCCERT_CHAIN_CONTEXT*);
	BOOL (WINAPI *pCertVerifyCertificateChainPolicy)		(LPCSTR,PCCERT_CHAIN_CONTEXT,PCERT_CHAIN_POLICY_PARA,PCERT_CHAIN_POLICY_STATUS);
	void (WINAPI *pCertFreeCertificateChain)				(PCCERT_CHAIN_CONTEXT);
	DWORD (WINAPI *pCertNameToStrA)							(DWORD dwCertEncodingType, PCERT_NAME_BLOB pName, DWORD dwStrType, LPCSTR psz, DWORD csz);

	PCCERT_CONTEXT (WINAPI *pCertCreateSelfSignCertificate)	(HCRYPTPROV,PCERT_NAME_BLOB,DWORD,PCRYPT_KEY_PROV_INFO,PCRYPT_ALGORITHM_IDENTIFIER,PSYSTEMTIME,PSYSTEMTIME,PCERT_EXTENSIONS);
	BOOL (WINAPI *pCertStrToNameA)							(DWORD,LPCSTR,DWORD,void *,BYTE *,DWORD *,LPCSTR *);
} crypt;
void SSL_Init(void)
{
	dllfunction_t secur_functable[] =
	{
		{(void**)&secur.pDecryptMessage,				"DecryptMessage"},
		{(void**)&secur.pEncryptMessage,				"EncryptMessage"},
		{(void**)&secur.pAcquireCredentialsHandleA,		"AcquireCredentialsHandleA"},
//		{(void**)&secur.pInitializeSecurityContextA,	"InitializeSecurityContextA"},
		{(void**)&secur.pInitializeSecurityContextW,	"InitializeSecurityContextW"},
		{(void**)&secur.pAcceptSecurityContext,			"AcceptSecurityContext"},
		{(void**)&secur.pCompleteAuthToken,				"CompleteAuthToken"},
		{(void**)&secur.pQueryContextAttributesA,		"QueryContextAttributesA"},
		{(void**)&secur.pFreeCredentialsHandle,			"FreeCredentialsHandle"},
		{(void**)&secur.pDeleteSecurityContext,			"DeleteSecurityContext"},
		{NULL, NULL}
	};

	dllfunction_t crypt_functable[] =
	{
		{(void**)&crypt.pCertGetCertificateChain,			"CertGetCertificateChain"},
		{(void**)&crypt.pCertVerifyCertificateChainPolicy,	"CertVerifyCertificateChainPolicy"},
		{(void**)&crypt.pCertFreeCertificateChain,			"CertFreeCertificateChain"},
		{(void**)&crypt.pCertNameToStrA,					"CertNameToStrA"},
		{(void**)&crypt.pCertCreateSelfSignCertificate,		"CertCreateSelfSignCertificate"},
		{(void**)&crypt.pCertStrToNameA,					"CertStrToNameA"},
		{NULL, NULL}
	};

	tls_ignorecertificateerrors = Cvar_Get("tls_ignorecertificateerrors", "0", CVAR_NOTFROMSERVER, "TLS");

	if (!secur.lib)
		secur.lib = Sys_LoadLibrary("secur32.dll", secur_functable);
	if (!crypt.lib)
		crypt.lib = Sys_LoadLibrary("crypt32.dll", crypt_functable);
}
qboolean SSL_Inited(void)
{
	return !!secur.lib && !!crypt.lib;
}

#define MessageAttribute (ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY | ISC_REQ_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_MANUAL_CRED_VALIDATION) 

struct sslbuf
{
	size_t datasize;
	char *data;
	size_t avail;
};

typedef struct {
	vfsfile_t funcs;
	vfsfile_t *stream;

	wchar_t wpeername[256];
	qboolean datagram;
	enum
	{
		HS_ESTABLISHED,

		HS_ERROR,

		HS_STARTCLIENT,
		HS_CLIENT,

		HS_STARTSERVER,
		HS_SERVER
	} handshaking;

	struct sslbuf outraw;
	struct sslbuf outcrypt;
	struct sslbuf inraw;
	struct sslbuf incrypt;


	CredHandle cred;
	SecHandle sechnd;
	int headersize, footersize;
	char headerdata[1024], footerdata[1024];

#ifdef HAVE_DTLS
	void *cbctx;
	void (*transmit)(void *cbctx, qbyte *data, size_t datasize);
#endif
} sslfile_t;

static int SSPI_ExpandBuffer(struct sslbuf *buf, size_t bytes)
{
	if (bytes < buf->datasize)
		return buf->datasize;
	Z_ReallocElements((void**)&buf->data, &buf->datasize, bytes, 1);
	return bytes;
}

static int SSPI_CopyIntoBuffer(struct sslbuf *buf, const void *data, unsigned int bytes, qboolean expand)
{
	if (bytes > buf->datasize - buf->avail)
	{
		if (!expand || SSPI_ExpandBuffer(buf, buf->avail + bytes + 1024) < buf->avail + bytes)
			bytes = buf->datasize - buf->avail;
	}
	memcpy(buf->data + buf->avail, data, bytes);
	buf->avail += bytes;

	return bytes;
}

static void SSPI_Error(sslfile_t *f, char *error, ...)
{
	va_list         argptr;
	char             string[1024];
	va_start (argptr, error);
	vsnprintf (string,sizeof(string)-1, error,argptr);
	va_end (argptr);

	f->handshaking = HS_ERROR;
	if (*string)
		Sys_Printf("%s", string);
	if (f->stream)
		VFS_CLOSE(f->stream);

	secur.pDeleteSecurityContext(&f->sechnd);
	secur.pFreeCredentialsHandle(&f->cred);
	f->stream = NULL;
}

static void SSPI_TryFlushCryptOut(sslfile_t *f)
{
	int sent;
	if (f->outcrypt.avail)
	{
#ifdef HAVE_DTLS
		if (f->transmit)
		{
			f->transmit(f->cbctx, f->outcrypt.data, f->outcrypt.avail);
			f->outcrypt.avail = 0;
			return;
		}
#endif
		sent = VFS_WRITE(f->stream, f->outcrypt.data, f->outcrypt.avail);
	}
	else
		return;

	if (sent > 0)
	{
		memmove(f->outcrypt.data, f->outcrypt.data + sent, f->outcrypt.avail - sent);
		f->outcrypt.avail -= sent;
	}
}

static int SSPI_CheckNewInCrypt(sslfile_t *f)
{
	int newd;
	if (!f->stream)
		return -1;
	newd = VFS_READ(f->stream, f->incrypt.data+f->incrypt.avail, f->incrypt.datasize - f->incrypt.avail);
	if (newd < 0)
		return newd;
	else
		f->incrypt.avail += newd;

	return 0;
}

//convert inbound crypt->data
static void SSPI_Decode(sslfile_t *f)
{
	SECURITY_STATUS		ss;
	SecBufferDesc		BuffDesc;
	SecBuffer			SecBuff[4];
	ULONG				ulQop = 0;
	SecBuffer			*extra = NULL;
	int i;

	if (!f->incrypt.avail)
		return;

	BuffDesc.ulVersion    = SECBUFFER_VERSION;
	BuffDesc.cBuffers     = countof(SecBuff);
	BuffDesc.pBuffers     = SecBuff;

	SecBuff[0].BufferType = SECBUFFER_DATA;
	SecBuff[0].cbBuffer   = f->incrypt.avail;
	SecBuff[0].pvBuffer   = f->incrypt.data;
	
	SecBuff[1].BufferType = SECBUFFER_EMPTY;	//space for header
	SecBuff[2].BufferType = SECBUFFER_EMPTY;	//space for footer
	SecBuff[3].BufferType = SECBUFFER_EMPTY;	//space for extra marker

	ss = secur.pDecryptMessage(&f->sechnd, &BuffDesc, 0, &ulQop);

	if (ss < 0)
	{
		if (ss == SEC_E_INCOMPLETE_MESSAGE)
		{
			if (f->incrypt.avail == f->incrypt.datasize)
				SSPI_ExpandBuffer(&f->incrypt, f->incrypt.datasize+1024);
			return;	//no error if its incomplete, we can just get more data later on.
		}
		switch(ss)
		{
		case SEC_E_DECRYPT_FAILURE:	SSPI_Error(f, "DecryptMessage failed: SEC_E_DECRYPT_FAILURE\n", ss); break;
		case SEC_E_INVALID_HANDLE:	SSPI_Error(f, "DecryptMessage failed: SEC_E_INVALID_HANDLE\n"); break;
		default:					SSPI_Error(f, "DecryptMessage failed: %0#lx\n", ss); break;
		}
		return;
	}

	for (i = 0; i < BuffDesc.cBuffers; i++)
	{
		switch(SecBuff[i].BufferType)
		{
		case SECBUFFER_DATA:
			if (SSPI_CopyIntoBuffer(&f->inraw, SecBuff[i].pvBuffer, SecBuff[i].cbBuffer, true) != SecBuff[i].cbBuffer)
				SSPI_Error(f, "outraw buffer overflowed\n");
			break;
		case SECBUFFER_EXTRA:
			if (extra)
				SSPI_Error(f, "multiple extra buffers\n");
			extra = &SecBuff[i];
			break;
		case SECBUFFER_EMPTY:
		case SECBUFFER_MISSING:
		case SECBUFFER_STREAM_TRAILER:
		case SECBUFFER_STREAM_HEADER:
			break;
		default:
			SSPI_Error(f, "got unexpected buffer type\n");
			break;
		}
	}

	//retain the extra. if there's no extra then mark it so.
	if (extra)
	{
		memmove(f->incrypt.data, f->incrypt.data + (f->incrypt.avail - extra->cbBuffer), extra->cbBuffer);
		f->incrypt.avail = extra->cbBuffer;
	}
	else
		f->incrypt.avail = 0;
}

//convert outgoing data->crypt
static void SSPI_Encode(sslfile_t *f)
{
	SECURITY_STATUS		ss;
	SecBufferDesc		BuffDesc;
	SecBuffer			SecBuff[4];
	ULONG				ulQop = 0;

	if (f->outcrypt.avail)
	{
		SSPI_TryFlushCryptOut(f);
		if (f->outcrypt.avail)
			return;	//don't flood too much
	}


	//don't corrupt the handshake data.
	if (f->handshaking)
		return;

	if (!f->outraw.avail)
		return;

	BuffDesc.ulVersion    = SECBUFFER_VERSION;
	BuffDesc.cBuffers     = 4;
	BuffDesc.pBuffers     = SecBuff;

	SecBuff[0].BufferType = SECBUFFER_STREAM_HEADER;
	SecBuff[0].cbBuffer   = f->headersize;
	SecBuff[0].pvBuffer   = f->headerdata;
	
	SecBuff[1].BufferType = SECBUFFER_DATA;
	SecBuff[1].cbBuffer   = f->outraw.avail;
	SecBuff[1].pvBuffer   = f->outraw.data;

	SecBuff[2].BufferType = SECBUFFER_STREAM_TRAILER;
	SecBuff[2].cbBuffer   = f->footersize;
	SecBuff[2].pvBuffer   = f->footerdata;

	SecBuff[3].BufferType = SECBUFFER_EMPTY;
	SecBuff[3].cbBuffer   = 0;
	SecBuff[3].pvBuffer   = NULL;

	ss = secur.pEncryptMessage(&f->sechnd, ulQop, &BuffDesc, 0);

	if (ss < 0)
	{
		SSPI_Error(f, "EncryptMessage failed\n");
		return;
	}

	f->outraw.avail = 0;

	//fixme: these should be made non-fatal.
	if (SSPI_CopyIntoBuffer(&f->outcrypt, SecBuff[0].pvBuffer, SecBuff[0].cbBuffer, true) < SecBuff[0].cbBuffer)
	{
		SSPI_Error(f, "crypt buffer overflowed\n");
		return;
	}
	if (SSPI_CopyIntoBuffer(&f->outcrypt, SecBuff[1].pvBuffer, SecBuff[1].cbBuffer, true) < SecBuff[1].cbBuffer)
	{
		SSPI_Error(f, "crypt buffer overflowed\n");
		return;
	}
	if (SSPI_CopyIntoBuffer(&f->outcrypt, SecBuff[2].pvBuffer, SecBuff[2].cbBuffer, true) < SecBuff[2].cbBuffer)
	{
		SSPI_Error(f, "crypt buffer overflowed\n");
		return;
	}

	SSPI_TryFlushCryptOut(f);
}

//these are known sites that use self-signed certificates, or are special enough that we don't trust corporate networks to hack in their own certificate authority for a proxy/mitm
//old static const qbyte triptohell_certdata[933] = "\x30\x82\x03\xa1\x30\x82\x02\x89\xa0\x03\x02\x01\x02\x02\x09\x00\x8b\xd0\x05\x63\x62\xd1\x6a\xe3\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x05\x05\x00\x30\x67\x31\x0b\x30\x09\x06\x03\x55\x04\x06\x13\x02\x42\x44\x31\x0c\x30\x0a\x06\x03\x55\x04\x08\x0c\x03\x42\x61\x64\x31\x0c\x30\x0a\x06\x03\x55\x04\x07\x0c\x03\x42\x61\x64\x31\x0c\x30\x0a\x06\x03\x55\x04\x0a\x0c\x03\x42\x61\x64\x31\x0c\x30\x0a\x06\x03\x55\x04\x0b\x0c\x03\x42\x61\x64\x31\x0c\x30\x0a\x06\x03\x55\x04\x03\x0c\x03\x42\x61\x64\x31\x12\x30\x10\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x09\x01\x16\x03\x42\x61\x64\x30\x1e\x17\x0d\x31\x34\x31\x32\x32\x34\x32\x32\x34\x32\x34\x37\x5a\x17\x0d\x32\x34\x31\x32\x32\x31\x32\x32\x34\x32\x34\x37\x5a\x30\x67\x31\x0b\x30\x09\x06\x03\x55\x04\x06\x13\x02\x42\x44\x31\x0c\x30\x0a\x06\x03\x55\x04\x08\x0c\x03\x42\x61\x64\x31\x0c\x30\x0a\x06\x03\x55\x04\x07\x0c\x03\x42\x61\x64\x31\x0c\x30\x0a\x06\x03\x55\x04\x0a\x0c\x03\x42\x61\x64\x31\x0c\x30\x0a\x06\x03\x55\x04\x0b\x0c\x03\x42\x61\x64\x31\x0c\x30\x0a\x06\x03\x55\x04\x03\x0c\x03\x42\x61\x64\x31\x12\x30\x10\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x09\x01\x16\x03\x42\x61\x64\x30\x82\x01\x22\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01\x05\x00\x03\x82\x01\x0f\x00\x30\x82\x01\x0a\x02\x82\x01\x01\x00\xaf\x10\x33\xfa\x39\xf5\xae\x2c\x91\x0e\x20\xe6\x3c\x5c\x7c\x1e\xeb\x16\x50\x2f\x05\x30\xfe\x67\xee\xa9\x00\x54\xd9\x4a\x86\xe6\xba\x80\xfb\x1a\x80\x08\x7e\x7b\x13\xe5\x1a\x18\xc9\xd4\x70\xbd\x5d\xc4\x38\xef\x64\xf1\x90\x2c\x53\x49\x93\x24\x36\x3e\x11\x59\x69\xa6\xdf\x37\xb2\x54\x82\x28\x3e\xdd\x30\x75\xa0\x18\xd8\xe1\xf5\x52\x73\x12\x5b\x37\x68\x1c\x59\xbd\x8c\x73\x66\x47\xbc\xcb\x9c\xfe\x38\x92\x8f\x74\xe9\xd1\x2f\x96\xd2\x5d\x6d\x11\x59\xb2\xdc\xbd\x8c\x37\x5b\x22\x76\x98\xe7\xbe\x08\xef\x1e\x99\xc4\xa9\x77\x2c\x9c\x0e\x08\x3c\x8e\xab\x97\x0c\x6a\xd7\x03\xab\xfd\x4a\x1e\x95\xb2\xc2\x9c\x3a\x16\x65\xd7\xaf\x45\x5f\x6e\xe7\xce\x51\xba\xa0\x60\x43\x0e\x07\xc5\x0b\x0a\x82\x05\x26\xc4\x92\x0a\x27\x5b\xfc\x57\x6c\xdf\xe2\x54\x8a\xef\x38\xf1\xf8\xc4\xf8\x51\x16\x27\x1f\x78\x89\x7c\x5b\xd7\x53\xcd\x9b\x54\x2a\xe6\x71\xee\xe4\x56\x2e\xa4\x09\x1a\x61\xf7\x0f\x97\x22\x94\xd7\xef\x21\x6c\xe6\x81\xfb\x54\x5f\x09\x92\xac\xd2\x7c\xab\xd5\xa9\x81\xf4\xc9\xb7\xd6\xbf\x68\xf8\x4f\xdc\xf3\x60\xa3\x3b\x29\x92\x9e\xdd\xa2\xa3\x02\x03\x01\x00\x01\xa3\x50\x30\x4e\x30\x1d\x06\x03\x55\x1d\x0e\x04\x16\x04\x14\x19\xed\xd0\x7b\x16\xaf\xb5\x0c\x9a\xe8\xd3\x46\x2e\x3c\x64\x29\xb6\xc1\x73\x5a\x30\x1f\x06\x03\x55\x1d\x23\x04\x18\x30\x16\x80\x14\x19\xed\xd0\x7b\x16\xaf\xb5\x0c\x9a\xe8\xd3\x46\x2e\x3c\x64\x29\xb6\xc1\x73\x5a\x30\x0c\x06\x03\x55\x1d\x13\x04\x05\x30\x03\x01\x01\xff\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x05\x05\x00\x03\x82\x01\x01\x00\x62\xa7\x26\xeb\xd4\x03\x29\x9c\x09\x33\x69\x7a\x9c\x65\x68\xec\x4c\xb9\x06\xeb\x1e\x51\x6f\x78\x20\xdc\xf6\x44\x5e\x06\x6e\x53\x87\x73\xe6\x14\x15\xb9\x17\x74\x67\xe0\x4e\x48\x38\xbc\x1c\xbd\xd0\xad\xd6\xbd\x8c\xf0\x3a\xe0\x13\x73\x19\xad\x8b\x79\x68\x67\x65\x9b\x7a\x4c\x81\xfb\xd9\x92\x77\x89\xb5\xb0\x53\xb0\xa5\xf7\x2d\x8e\x29\x60\x31\xd1\x9b\x2f\x63\x8a\x5f\x64\xc1\x61\xd5\xb7\xdf\x70\x3b\x2b\xf6\x1a\x96\xb9\xa7\x08\xca\x87\xa6\x8c\x60\xca\x6e\xd7\xee\xba\xef\x89\x0b\x93\xd5\xfd\xfc\x14\xba\xef\x27\xba\x90\x11\x90\xf7\x25\x70\xe7\x4e\xf4\x9c\x13\x27\xc1\xa7\x8e\xd9\x66\x43\x72\x20\x5b\xe1\x5c\x73\x74\xf5\x33\xf2\xa5\xf6\xe1\xd5\xac\xf3\x67\x5c\xe7\xd4\x0a\x8d\x91\x73\x03\x3e\x9d\xbc\x96\xc3\x0c\xdb\xd5\x77\x6e\x76\x44\x69\xaf\x24\x0f\x4f\x8b\x47\x36\x8b\xc3\xd6\x36\xdd\x26\x5a\x9c\xdd\x9c\x43\xee\x29\x43\xdd\x75\x2f\x19\x52\xfc\x1d\x24\x9c\x13\x29\x99\xa0\x6d\x7a\x95\xcc\xa0\x58\x86\xd8\xc5\xb9\xa3\xc2\x3d\x64\x1d\x85\x8a\xca\x53\x55\x8e\x9a\x6d\xc9\x91\x73\xf4\xe1\xe1\xa4\x9b\x76\xfc\x7f\x63\xc2\xb9\x23";
static const qbyte fte_triptohell_certdata[917] =	"\x30\x82\x03\x91\x30\x82\x02\x79\xa0\x03\x02\x01\x02\x02\x09\x00\xb5\x71\x47\x8d\x5e\x66\xf1\xd9\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0b\x05\x00\x30\x5f\x31\x0b\x30\x09\x06\x03\x55\x04\x06\x13\x02\x55\x53\x31\x11\x30\x0f\x06\x03\x55\x04\x08\x0c\x08\x4e\x65\x77\x20\x59\x6f\x72\x6b\x31\x11\x30\x0f\x06\x03\x55\x04\x07\x0c\x08\x4e\x65\x77\x20\x59\x6f\x72\x6b\x31\x0c\x30\x0a\x06\x03\x55\x04\x0a\x0c\x03\x46\x54\x45\x31\x1c\x30\x1a\x06\x03\x55\x04\x03\x0c\x13\x66\x74\x65\x2e\x74\x72\x69\x70\x74\x6f\x68\x65\x6c\x6c\x2e\x69\x6e\x66\x6f\x30\x1e\x17\x0d\x31\x34\x31\x32\x32\x35\x30\x30\x35\x38\x31\x34\x5a\x17\x0d\x31\x37\x30\x33\x30\x34\x30\x30\x35\x38\x31\x34\x5a\x30\x5f\x31\x0b\x30\x09\x06\x03\x55\x04\x06\x13\x02\x55\x53\x31\x11\x30\x0f\x06\x03\x55\x04\x08\x0c\x08\x4e\x65\x77\x20\x59\x6f\x72\x6b\x31\x11\x30\x0f\x06\x03\x55\x04\x07\x0c\x08\x4e\x65\x77\x20\x59\x6f\x72\x6b\x31\x0c\x30\x0a\x06\x03\x55\x04\x0a\x0c\x03\x46\x54\x45\x31\x1c\x30\x1a\x06\x03\x55\x04\x03\x0c\x13\x66\x74\x65\x2e\x74\x72\x69\x70\x74\x6f\x68\x65\x6c\x6c\x2e\x69\x6e\x66\x6f\x30\x82\x01\x22\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01\x05\x00\x03\x82\x01\x0f\x00\x30\x82\x01\x0a\x02\x82\x01\x01\x00\xdd\xb8\x7c\x69\x3d\x63\x95\xe3\x88\x15\xfd\xad\x93\x5e\x6b\x97\xfb\x74\xba\x1f\x83\x33\xe5\x8a\x8d\x8f\xb0\xbf\xf9\xd3\xa1\x2c\x65\x53\xa7\xef\xd3\x0f\xdc\x03\x60\x0a\x40\xef\xa8\xef\x3f\xb3\xd9\x8d\x31\x39\x12\x8a\xd8\x0e\x24\x8f\xe5\x58\x26\x86\x4c\x76\x6c\x59\x9a\xab\xea\x1c\x3d\xfb\x62\x62\xad\xaf\xd6\x00\x33\x76\x2d\xbb\xeb\xe8\xec\xb4\x76\x4f\xb0\xbe\xcf\xf0\x46\x94\x40\x02\x99\xd4\xb2\x71\x71\xd6\xf5\x1f\xc3\x4f\x1e\x1e\xb4\x0d\x82\x49\xc4\xa2\xdc\xae\x6f\x4e\x3a\xf9\x0e\xdd\xf4\xd2\x53\xe3\xe7\x7d\x58\x79\xf4\xce\x1f\x6c\xac\x81\x8c\x8c\xe1\x03\x5b\x22\x56\x92\x19\x4f\x74\xc0\x36\x41\xac\x1b\xfa\x9e\xf7\x2a\x0f\xd6\x4b\xcc\x9a\xca\x67\x87\xb7\x95\xdf\xb7\xd4\x7d\x8c\xcc\xa9\x25\xde\xdd\x8c\x1b\xd7\x32\xf2\x84\x25\x46\x7b\x10\x55\xf9\x80\xfd\x5d\xad\xab\xf9\x4c\x1f\xc0\xa5\xd1\x3f\x01\x86\x4d\xfa\x57\xab\x7a\x6d\xec\xf1\xdb\xf4\xad\xf2\x33\xcd\xa0\xed\xfe\x1b\x27\x55\x56\xba\x8c\x47\x70\x16\xd5\x75\x17\x8e\x80\xaa\x49\x5e\x93\x83\x1d\x6f\x1f\x2c\xf7\xa7\x64\xe6\x2e\x88\x8e\xff\x70\x5a\x41\x52\xae\x93\x02\x03\x01\x00\x01\xa3\x50\x30\x4e\x30\x1d\x06\x03\x55\x1d\x0e\x04\x16\x04\x14\x4e\x76\x4a\xce\x7b\x45\x14\x39\xeb\x9c\x28\x56\xb5\x7b\x8a\x18\x6f\x22\x17\x82\x30\x1f\x06\x03\x55\x1d\x23\x04\x18\x30\x16\x80\x14\x4e\x76\x4a\xce\x7b\x45\x14\x39\xeb\x9c\x28\x56\xb5\x7b\x8a\x18\x6f\x22\x17\x82\x30\x0c\x06\x03\x55\x1d\x13\x04\x05\x30\x03\x01\x01\xff\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0b\x05\x00\x03\x82\x01\x01\x00\x48\x22\x65\xed\x2e\xc5\xed\xbb\xe9\x40\x6c\x80\xc4\x63\x19\xd1\x00\xb4\x30\x34\x17\x7c\x7c\xbd\x1b\xc5\xa9\x43\x0c\x92\x6e\xd6\x2d\x11\x6c\x0d\xa6\xda\x30\xe9\xf7\x46\x7b\x01\xe4\x53\x23\xae\x88\xd1\xf2\xed\xca\x84\x06\x19\x97\xb9\x06\xfb\xda\xec\x72\x2d\x15\x20\xd2\x8f\x66\xad\xb5\xdd\x4b\x4f\xdf\x7e\xaf\xa3\x6c\x7f\x53\x32\x8f\xe2\x19\x5c\x44\x98\x86\x31\xee\xb4\x03\xe7\x27\xa1\x83\xab\xc3\xce\xb4\x9a\x01\xbe\x8c\x64\x2e\x2b\xe3\x4e\x55\xdf\x95\xeb\x16\x87\xbd\xfa\x11\xa2\x3e\x38\x92\x97\x36\xe9\x65\x60\xf3\xac\x68\x44\xb3\x51\x54\x3a\x42\xa8\x98\x9b\xee\x1b\x9e\x79\x6a\xaf\xc0\xbe\x41\xc4\xb1\x96\x42\xd9\x94\xef\x49\x5b\xbe\x2d\x04\xb9\xfb\x92\xbb\xdc\x0e\x29\xfd\xee\xa9\x68\x09\xf9\x9f\x69\x8b\x3d\xe1\x4b\xee\x24\xf9\xfe\x02\x3a\x0a\xb8\xcd\x6c\x07\x43\xa9\x4a\xe7\x03\x34\x2e\x72\xa7\x81\xaa\x40\xa9\x98\x5d\x97\xee\x2a\x99\xc6\x8f\xe8\x6f\x98\xa2\x85\xc9\x0d\x04\x19\x43\x6a\xd3\xc7\x15\x4c\x4b\xbc\xa5\xb8\x9f\x38\xf3\x43\x83\x0c\xef\x97\x6e\xa6\x20\xde\xc5\xd3\x1e\x3e\x5d\xcd\x58\x3d\x5c\x55\x7a\x90\x94";
static const qbyte triptohell_certdata[933] =		"\x30\x82\x03\xa1\x30\x82\x02\x89\xa0\x03\x02\x01\x02\x02\x09\x00\xea\xb7\x13\xcf\x55\xe5\xe8\x8c\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0b\x05\x00\x30\x67\x31\x0b\x30\x09\x06\x03\x55\x04\x06\x13\x02\x55\x53\x31\x11\x30\x0f\x06\x03\x55\x04\x08\x0c\x08\x4e\x65\x77\x20\x59\x6f\x72\x6b\x31\x11\x30\x0f\x06\x03\x55\x04\x07\x0c\x08\x4e\x65\x77\x20\x59\x6f\x72\x6b\x31\x18\x30\x16\x06\x03\x55\x04\x0a\x0c\x0f\x74\x72\x69\x70\x74\x6f\x68\x65\x6c\x6c\x2e\x69\x6e\x66\x6f\x31\x18\x30\x16\x06\x03\x55\x04\x03\x0c\x0f\x74\x72\x69\x70\x74\x6f\x68\x65\x6c\x6c\x2e\x69\x6e\x66\x6f\x30\x1e\x17\x0d\x31\x34\x31\x32\x32\x35\x30\x30\x35\x38\x33\x37\x5a\x17\x0d\x31\x37\x30\x33\x30\x34\x30\x30\x35\x38\x33\x37\x5a\x30\x67\x31\x0b\x30\x09\x06\x03\x55\x04\x06\x13\x02\x55\x53\x31\x11\x30\x0f\x06\x03\x55\x04\x08\x0c\x08\x4e\x65\x77\x20\x59\x6f\x72\x6b\x31\x11\x30\x0f\x06\x03\x55\x04\x07\x0c\x08\x4e\x65\x77\x20\x59\x6f\x72\x6b\x31\x18\x30\x16\x06\x03\x55\x04\x0a\x0c\x0f\x74\x72\x69\x70\x74\x6f\x68\x65\x6c\x6c\x2e\x69\x6e\x66\x6f\x31\x18\x30\x16\x06\x03\x55\x04\x03\x0c\x0f\x74\x72\x69\x70\x74\x6f\x68\x65\x6c\x6c\x2e\x69\x6e\x66\x6f\x30\x82\x01\x22\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01\x05\x00\x03\x82\x01\x0f\x00\x30\x82\x01\x0a\x02\x82\x01\x01\x00\xd8\x77\x62\xf6\x74\xa7\x75\xde\xda\x09\xae\x9e\x76\x7a\xc6\x2a\xcf\x9a\xbe\xc6\xb9\x6d\xe2\xca\x0f\x2d\x95\xb8\x89\x93\xf7\x50\x64\x92\x7d\x95\x34\xe4\x6e\xef\x52\x56\xef\x13\x9a\x3a\xae\x84\x5b\x57\x82\x04\x86\x74\xbd\x4e\x38\x32\x56\x00\xd6\x34\x9c\x23\xd6\x81\x8e\x29\x77\x45\x61\x20\xdf\x28\xf8\xe5\x61\x83\xec\xe6\xa0\x1a\x75\xa8\x3b\x53\x6f\xc4\x09\x61\x66\x3a\xf0\x81\xbf\x2c\xf5\x8e\xf1\xe2\x35\xe4\x24\x7f\x16\xcc\xce\x60\xa2\x42\x6e\xc2\x3a\x29\x75\x6c\x79\xb0\x99\x9c\xe2\xfe\x27\x32\xb6\xf7\x0d\x71\xfd\x62\x9d\x54\x7c\x40\xb2\xf5\xa0\xa4\x25\x31\x8d\x65\xfd\x3f\x3b\x9b\x7e\x84\x74\x17\x3c\x1f\xec\x50\xcf\x75\xb8\x5c\xca\xfc\x0f\xe8\x47\xd8\x64\xec\x5f\x6c\x45\x9a\x55\x49\x97\x3f\xcb\x49\x34\x71\x0a\x12\x13\xbc\x3d\x53\x81\x17\x9a\x92\x44\x91\x07\xc2\xef\x6d\x64\x86\x5d\xfd\x67\xd5\x99\x38\x95\x46\x74\x6d\xb6\xbf\x29\xc9\x5b\xac\xb1\x46\xd6\x9e\x57\x5c\x7b\x24\x91\xf4\x7c\xe4\x01\x31\x8c\xec\x79\x94\xb7\x3f\xd2\x93\x6d\xe2\x69\xbe\x61\x44\x2e\x8f\x1a\xdc\xa8\x97\xf5\x81\x8e\x0c\xe1\x00\xf2\x71\x51\xf3\x02\x03\x01\x00\x01\xa3\x50\x30\x4e\x30\x1d\x06\x03\x55\x1d\x0e\x04\x16\x04\x14\x18\xb2\x6b\x63\xcc\x17\x54\xf6\xf0\xb6\x9e\x62\xa4\x35\xcf\x47\x74\x13\x29\xbf\x30\x1f\x06\x03\x55\x1d\x23\x04\x18\x30\x16\x80\x14\x18\xb2\x6b\x63\xcc\x17\x54\xf6\xf0\xb6\x9e\x62\xa4\x35\xcf\x47\x74\x13\x29\xbf\x30\x0c\x06\x03\x55\x1d\x13\x04\x05\x30\x03\x01\x01\xff\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0b\x05\x00\x03\x82\x01\x01\x00\x7f\x24\x18\x8a\x79\xee\xf9\xeb\xed\x29\x1e\x21\x15\x8a\x53\xc9\xb7\xec\x30\xc4\x85\x9f\x45\x85\x26\x36\xb7\x07\xf3\xf1\xff\x3b\x89\x05\x0a\xd4\x30\x68\x31\x68\x33\xdd\xf6\x58\xa3\x85\x9f\x49\x50\x76\x9a\xc5\x79\x13\xe1\x4d\x67\x0c\xf3\x92\xf0\x1d\x02\x1f\xc4\x5c\xd4\xa1\x0c\x57\xdf\x46\x84\x43\x9f\xb0\xe2\x91\x62\xa8\xe0\x86\x0d\x47\xe1\xd9\x60\x01\xc4\xe0\xda\x6f\x06\x0a\xad\x38\xf3\x66\x68\xc5\xe2\x66\x3e\x47\x83\x65\x64\xcd\xff\xf3\xbb\xa7\xfa\x23\xf1\x82\x5e\x06\x6a\x91\x37\x51\xcd\xb9\x95\x20\x89\xff\xa1\x54\xb2\x76\xcf\x8e\xe1\xcd\x13\x93\x13\xd1\xda\x0d\x0d\xbc\x0f\xd5\x11\x26\xd6\xaf\x60\x0f\x4d\x8a\x4f\x28\xee\x6c\xf1\x99\xdc\xed\x16\xdc\x87\x26\xfd\x23\x8a\xb8\xb0\x20\x0e\xe2\x32\xf5\x8e\xb0\x65\x98\x13\xb8\x4b\x39\x7c\x8c\x98\xa2\x29\x75\x48\x3a\x89\xf9\x61\x77\x6c\x2d\x84\x41\x40\x17\xa6\x50\xc5\x09\x63\x10\xe7\x09\xd4\x5c\xdd\x0e\x71\x16\xaf\xb1\x32\xe4\xc0\xe6\xea\xfd\x26\x55\x07\x40\x95\x84\x48\x62\x04\x10\x92\xb2\xd9\x27\xfb\x8a\xf3\x7c\xe6\xfe\xd4\xfc\xa6\x33\x79\x01\x5c\xc3\x1f\x80\xa8\xf3";
static struct
{
	wchar_t *hostname;
	unsigned int datasize;
	const qbyte *data;
	//FIXME: include expiry information
	//FIXME: add alternative when one is about to expire
} knowncerts[] = {
	{L"triptohell.info", sizeof(triptohell_certdata), triptohell_certdata},
	{L"fte.triptohell.info", sizeof(fte_triptohell_certdata), fte_triptohell_certdata},
	{NULL}
};

char *narrowen(char *out, size_t outlen, wchar_t *wide);
static DWORD VerifyKnownCertificates(DWORD status, wchar_t *domain, qbyte *data, size_t datasize, qboolean datagram)
{
	int i;
	if (datagram)
	{
		Con_Printf("FIXME: Ring of trust not yet implemented\n");
		if (status == CERT_E_UNTRUSTEDROOT)
		{
			Con_Printf("Allowing (probably) self-signed cert.\n");
			status = SEC_E_OK;
		}
		return status;
	}
	for (i = 0; knowncerts[i].hostname; i++)
	{
		if (!wcscmp(domain, knowncerts[i].hostname))
		{
#ifdef _DEBUG
			if (!knowncerts[i].data)
			{
				int j;
				Con_Printf("%ls cert %i bytes\n", domain, datasize);

				Con_Printf("\"", datasize);
				for (j = 0; j < datasize; j++)
					Con_Printf("\\x%02x", data[j]);
				Con_Printf("\"\n", datasize);

				Con_Printf("\n", datasize);
				for (j = 0; j < datasize; j++)
					Con_Printf("%c", data[j]);
				continue;
			}
#endif
			if (knowncerts[i].datasize == datasize && !memcmp(data, knowncerts[i].data, datasize))
			{	//what we know about matched
				if (status == CERT_E_UNTRUSTEDROOT)
					status = SEC_E_OK;
				break;
			}
			else
			{
				if (status != CERT_E_EXPIRED)
					Con_Printf("%ls has an unexpected certificate\n", domain);
				if (status == SEC_E_OK)	//we (think) we know better.
					status = TRUST_E_FAIL;
			}
		}
	}
	return status;
}

static DWORD VerifyServerCertificate(PCCERT_CONTEXT pServerCert, PWSTR pwszServerName, DWORD dwCertFlags, qboolean datagram)
{
	HTTPSPolicyCallbackData polHttps;
	CERT_CHAIN_POLICY_PARA PolicyPara;
	CERT_CHAIN_POLICY_STATUS PolicyStatus;
	CERT_CHAIN_PARA ChainPara;
	PCCERT_CHAIN_CONTEXT pChainContext;
	DWORD Status;
	LPSTR rgszUsages[]		=
	{
		szOID_PKIX_KP_SERVER_AUTH,
		szOID_SERVER_GATED_CRYPTO,
		szOID_SGC_NETSCAPE
	};
	DWORD cUsages			=	sizeof(rgszUsages) / sizeof(LPSTR);

	if(pServerCert == NULL)
		return SEC_E_WRONG_PRINCIPAL;
	if(!*pwszServerName)
		return SEC_E_WRONG_PRINCIPAL;

	// Build certificate chain.
	memset(&ChainPara, 0, sizeof(ChainPara));
	ChainPara.cbSize = sizeof(ChainPara);
	ChainPara.RequestedUsage.dwType = USAGE_MATCH_TYPE_OR;
	ChainPara.RequestedUsage.Usage.cUsageIdentifier = cUsages;
	ChainPara.RequestedUsage.Usage.rgpszUsageIdentifier = rgszUsages;

	if (!crypt.pCertGetCertificateChain(NULL, pServerCert, NULL, pServerCert->hCertStore, &ChainPara, 0, NULL, &pChainContext))
	{
		Status = GetLastError();
		Sys_Printf("Error %#lx returned by CertGetCertificateChain!\n", Status);
	}
	else
	{
		// Validate certificate chain.
		memset(&polHttps, 0, sizeof(HTTPSPolicyCallbackData));
		polHttps.cbStruct = sizeof(HTTPSPolicyCallbackData);
		polHttps.dwAuthType = AUTHTYPE_SERVER;
		polHttps.fdwChecks = dwCertFlags;
		polHttps.pwszServerName = pwszServerName;

		memset(&PolicyPara, 0, sizeof(PolicyPara));
		PolicyPara.cbSize = sizeof(PolicyPara);
		PolicyPara.pvExtraPolicyPara = &polHttps;

		memset(&PolicyStatus, 0, sizeof(PolicyStatus));
		PolicyStatus.cbSize = sizeof(PolicyStatus);

		if (!crypt.pCertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL, pChainContext, &PolicyPara, &PolicyStatus))
		{
			Status = GetLastError();
			Sys_Printf("Error %#lx returned by CertVerifyCertificateChainPolicy!\n", Status);
		}
		else
		{
			Status = VerifyKnownCertificates(PolicyStatus.dwError, pwszServerName, pServerCert->pbCertEncoded, pServerCert->cbCertEncoded, datagram);
			if (Status)
			{
				char fmsg[512];
				char *err;
				switch (Status)
				{
					case CERT_E_EXPIRED:                err = "CERT_E_EXPIRED";                 break;
					case CERT_E_VALIDITYPERIODNESTING:  err = "CERT_E_VALIDITYPERIODNESTING";   break;
					case CERT_E_ROLE:                   err = "CERT_E_ROLE";                    break;
					case CERT_E_PATHLENCONST:           err = "CERT_E_PATHLENCONST";            break;
					case CERT_E_CRITICAL:               err = "CERT_E_CRITICAL";                break;
					case CERT_E_PURPOSE:                err = "CERT_E_PURPOSE";                 break;
					case CERT_E_ISSUERCHAINING:         err = "CERT_E_ISSUERCHAINING";          break;
					case CERT_E_MALFORMED:              err = "CERT_E_MALFORMED";               break;
					case CERT_E_UNTRUSTEDROOT:          err = "CERT_E_UNTRUSTEDROOT";           break;
					case CERT_E_CHAINING:               err = "CERT_E_CHAINING";                break;
					case TRUST_E_FAIL:                  err = "TRUST_E_FAIL";                   break;
					case CERT_E_REVOKED:                err = "CERT_E_REVOKED";                 break;
					case CERT_E_UNTRUSTEDTESTROOT:      err = "CERT_E_UNTRUSTEDTESTROOT";       break;
					case CERT_E_REVOCATION_FAILURE:     err = "CERT_E_REVOCATION_FAILURE";      break;
					case CERT_E_CN_NO_MATCH:            
						err = fmsg;
						Q_strncpyz(fmsg, "Certificate is for ", sizeof(fmsg));
						crypt.pCertNameToStrA(X509_ASN_ENCODING, &pServerCert->pCertInfo->Subject, 0, fmsg+strlen(fmsg), sizeof(fmsg)-strlen(fmsg));
						break;
					case CERT_E_WRONG_USAGE:            err = "CERT_E_WRONG_USAGE";             break;
					default:                            err = "(unknown)";                      break;
				}
				Con_Printf("Error verifying certificate for '%ls': %s\n", pwszServerName, err);

				if (tls_ignorecertificateerrors->ival)
				{
					Con_Printf("pretending it didn't happen... (tls_ignorecertificateerrors is set)\n");
					Status = SEC_E_OK;
				}
			}
			else
				Status = SEC_E_OK;
		}
		crypt.pCertFreeCertificateChain(pChainContext);
	}

	return Status;
}
static PCCERT_CONTEXT SSPI_GetServerCertificate(void)
{
	static PCCERT_CONTEXT ret;
	char *issuertext = "CN=127.0.0.1, O=\"FTE QuakeWorld\", OU=Testing, C=TR";
	CERT_NAME_BLOB issuerblob;

	CRYPT_ALGORITHM_IDENTIFIER sigalg;
	SYSTEMTIME expiredate;

	if (ret)
		return ret;

	memset(&sigalg, 0, sizeof(sigalg));
	sigalg.pszObjId = szOID_RSA_SHA512RSA;

	GetSystemTime(&expiredate);
	expiredate.wYear += 2;	//2 years hence. woo


	memset(&issuerblob, 0, sizeof(issuerblob));
	crypt.pCertStrToNameA(X509_ASN_ENCODING, issuertext, CERT_X500_NAME_STR, NULL, issuerblob.pbData, &issuerblob.cbData, NULL);
	issuerblob.pbData = Z_Malloc(issuerblob.cbData);
	crypt.pCertStrToNameA(X509_ASN_ENCODING, issuertext, CERT_X500_NAME_STR, NULL, issuerblob.pbData, &issuerblob.cbData, NULL);

	ret = crypt.pCertCreateSelfSignCertificate(
			0,
			&issuerblob,
			0,
			NULL,
			&sigalg,
			NULL,
			&expiredate,
			NULL
		);
	if (!ret)
	{	//try and downgrade the signature algo if it failed.
		sigalg.pszObjId = szOID_RSA_SHA1RSA;
		ret = crypt.pCertCreateSelfSignCertificate(
			0,
			&issuerblob,
			0,
			NULL,
			&sigalg,
			NULL,
			&expiredate,
			NULL
		);
	}

	Z_Free(issuerblob.pbData);
	return ret;
}

static void SSPI_GenServerCredentials(sslfile_t *f)
{
	SECURITY_STATUS   ss;
	TimeStamp         Lifetime;
	SCHANNEL_CRED SchannelCred;
	PCCERT_CONTEXT cred;

	memset(&SchannelCred, 0, sizeof(SchannelCred));
	SchannelCred.dwVersion = SCHANNEL_CRED_VERSION;
	SchannelCred.grbitEnabledProtocols = f->datagram?USE_PROT_DGRAM_SERVER:USE_PROT_SERVER;
	SchannelCred.dwFlags |= SCH_CRED_NO_SYSTEM_MAPPER|SCH_CRED_DISABLE_RECONNECTS;	/*don't use windows login info or anything*/

	cred = SSPI_GetServerCertificate();
	SchannelCred.cCreds = 1;
	SchannelCred.paCred = &cred;

	if (!cred)
	{
		SSPI_Error(f, "Unable to load/generate certificate\n");
		return;
	}

	ss = secur.pAcquireCredentialsHandleA (NULL, UNISP_NAME_A, SECPKG_CRED_INBOUND, NULL, &SchannelCred, NULL, NULL, &f->cred, &Lifetime);
	if (ss < 0)
	{
		SSPI_Error(f, "AcquireCredentialsHandle failed\n");
		return;
	}
}

static void SSPI_Handshake (sslfile_t *f)
{
	SECURITY_STATUS		ss;
	TimeStamp			Lifetime;
	SecBufferDesc		OutBuffDesc;
	SecBuffer			OutSecBuff[8];
	SecBufferDesc		InBuffDesc;
	SecBuffer			InSecBuff[8];
	ULONG				ContextAttributes;
	SCHANNEL_CRED SchannelCred;
	int i;
	qboolean retries = 5;

//	char buf1[128];
//	char buf2[128];

retry:

	if (f->outcrypt.avail)
	{
		//don't let things build up too much
		SSPI_TryFlushCryptOut(f);
		if (f->outcrypt.avail)
			return;
	}

	//FIXME: skip this if we've had no new data since last time

	OutBuffDesc.ulVersion = SECBUFFER_VERSION;
	OutBuffDesc.cBuffers  = countof(OutSecBuff);
	OutBuffDesc.pBuffers  = OutSecBuff;

	OutSecBuff[0].BufferType = SECBUFFER_TOKEN;
	OutSecBuff[0].cbBuffer   = f->outcrypt.datasize - f->outcrypt.avail;
	OutSecBuff[0].pvBuffer   = f->outcrypt.data + f->outcrypt.avail;

	for (i = 0; i < OutBuffDesc.cBuffers; i++)
	{
		OutSecBuff[i].BufferType = SECBUFFER_EMPTY;
		OutSecBuff[i].pvBuffer   = NULL;
		OutSecBuff[i].cbBuffer   = 0;
	}

	if (f->handshaking == HS_ERROR)
		return;	//gave up.
	else if (f->handshaking == HS_STARTCLIENT)
	{
		//no input data yet.
		f->handshaking = HS_CLIENT;

		memset(&SchannelCred, 0, sizeof(SchannelCred));
		SchannelCred.dwVersion = SCHANNEL_CRED_VERSION;
		SchannelCred.grbitEnabledProtocols = f->datagram?USE_PROT_DGRAM_CLIENT:USE_PROT_CLIENT;
		SchannelCred.dwFlags |= SCH_CRED_SNI_CREDENTIAL | SCH_CRED_NO_DEFAULT_CREDS;	/*don't use windows login info or anything*/

		ss = secur.pAcquireCredentialsHandleA (NULL, UNISP_NAME_A, SECPKG_CRED_OUTBOUND, NULL, &SchannelCred, NULL, NULL, &f->cred, &Lifetime);
		if (ss < 0)
		{
			SSPI_Error(f, "AcquireCredentialsHandle failed\n");
			return;
		}

		ss = secur.pInitializeSecurityContextW (&f->cred, NULL, f->wpeername, MessageAttribute|(f->datagram?ISC_REQ_DATAGRAM:ISC_REQ_STREAM), 0, SECURITY_NATIVE_DREP, NULL, 0, &f->sechnd, &OutBuffDesc, &ContextAttributes, &Lifetime);
	}
	else if (f->handshaking == HS_CLIENT)
	{
		//only if we actually have data.
		if (!f->incrypt.avail && !f->datagram)
			return;

		InBuffDesc.ulVersion = SECBUFFER_VERSION;
		InBuffDesc.cBuffers  = 4;
		InBuffDesc.pBuffers  = InSecBuff;

		i = 0;

		if (f->incrypt.avail)
		{
			InSecBuff[i].BufferType = SECBUFFER_TOKEN;
			InSecBuff[i].cbBuffer   = f->incrypt.avail;
			InSecBuff[i].pvBuffer   = f->incrypt.data;
			i++;
		}

		for (; i < InBuffDesc.cBuffers; i++)
		{
			InSecBuff[i].BufferType = SECBUFFER_EMPTY;
			InSecBuff[i].pvBuffer   = NULL;
			InSecBuff[i].cbBuffer   = 0;
		}

		ss = secur.pInitializeSecurityContextW (&f->cred, &f->sechnd, NULL, MessageAttribute|(f->datagram?ISC_REQ_DATAGRAM:ISC_REQ_STREAM), 0, SECURITY_NETWORK_DREP, &InBuffDesc, 0, &f->sechnd, &OutBuffDesc, &ContextAttributes, &Lifetime);

		if (ss == SEC_E_INCOMPLETE_MESSAGE)
		{
//			Con_Printf("SEC_E_INCOMPLETE_MESSAGE\n");
			if (!f->datagram && f->incrypt.avail == f->incrypt.datasize)
				SSPI_ExpandBuffer(&f->incrypt, f->incrypt.datasize+1024);
			return;
		}
		else if (ss == SEC_E_INVALID_TOKEN)
		{
//			Con_Printf("SEC_E_INVALID_TOKEN\n");
			if (f->datagram)
				return;	//our udp protocol may have non-dtls packets mixed in. besides, we don't want to die from spoofed packets.
		}
//		else if (ss == SEC_I_MESSAGE_FRAGMENT)
//			Con_Printf("SEC_I_MESSAGE_FRAGMENT\n");
//		else if (ss == SEC_I_CONTINUE_NEEDED)
//			Con_Printf("SEC_I_CONTINUE_NEEDED\n");
//		else
//			Con_Printf("InitializeSecurityContextA %x\n", ss);


		//any extra data should still remain for the next time around. this might be more handshake data or payload data.
		if (InSecBuff[1].BufferType == SECBUFFER_EXTRA)
		{
			memmove(f->incrypt.data, f->incrypt.data + (f->incrypt.avail - InSecBuff[1].cbBuffer), InSecBuff[1].cbBuffer);
			f->incrypt.avail = InSecBuff[1].cbBuffer;
		}
		else f->incrypt.avail = 0;
	}
	else if (f->handshaking == HS_STARTSERVER || f->handshaking == HS_SERVER)
	{
		//only if we actually have data.
		if (!f->incrypt.avail)
			return;

		InBuffDesc.ulVersion = SECBUFFER_VERSION;
		InBuffDesc.cBuffers  = countof(InSecBuff);
		InBuffDesc.pBuffers  = InSecBuff;
		i = 0;

		if (f->incrypt.avail)
		{
			InSecBuff[i].BufferType = SECBUFFER_TOKEN;
			InSecBuff[i].cbBuffer   = f->incrypt.avail;
			InSecBuff[i].pvBuffer   = f->incrypt.data;
			i++;
		}

		for (; i < InBuffDesc.cBuffers; i++)
		{
			InSecBuff[i].BufferType = SECBUFFER_EMPTY;
			InSecBuff[i].pvBuffer   = NULL;
			InSecBuff[i].cbBuffer   = 0;
		}

		i = 1;
		OutSecBuff[i++].BufferType = SECBUFFER_EXTRA;
		OutSecBuff[i++].BufferType = 17/*SECBUFFER_ALERT*/;

#define ServerMessageAttribute (ASC_REQ_SEQUENCE_DETECT | ASC_REQ_REPLAY_DETECT | ASC_REQ_CONFIDENTIALITY /*| ASC_REQ_EXTENDED_ERROR*/ | ASC_REQ_ALLOCATE_MEMORY)

		ss = secur.pAcceptSecurityContext(&f->cred, (f->handshaking==HS_SERVER)?&f->sechnd:NULL, &InBuffDesc,
								ServerMessageAttribute|(f->datagram?ASC_REQ_DATAGRAM:ASC_REQ_STREAM), SECURITY_NETWORK_DREP, &f->sechnd,
								&OutBuffDesc, &ContextAttributes, NULL); 
		if (ss == SEC_E_INVALID_TOKEN)
		{
//			Con_Printf("SEC_E_INVALID_TOKEN\n");
			if (f->datagram)
				return;
		}
		else if (ss == SEC_E_INCOMPLETE_MESSAGE)
		{
//			Con_Printf("SEC_E_INCOMPLETE_MESSAGE\n");
			if (!f->datagram && f->incrypt.avail == f->incrypt.datasize)
				SSPI_ExpandBuffer(&f->incrypt, f->incrypt.datasize+1024);
			return;
		}
//		else
//			Con_Printf("InitializeSecurityContextA %x\n", ss);
		f->handshaking = HS_SERVER;

		//any extra data should still remain for the next time around. this might be more handshake data or payload data.
		if (InSecBuff[1].BufferType == SECBUFFER_EXTRA)
		{
			memmove(f->incrypt.data, f->incrypt.data + (f->incrypt.avail - InSecBuff[1].cbBuffer), InSecBuff[1].cbBuffer);
			f->incrypt.avail = InSecBuff[1].cbBuffer;
		}
		else f->incrypt.avail = 0;
	}
	else
		return;
	

	if (ss == SEC_I_INCOMPLETE_CREDENTIALS)
	{
		SSPI_Error(f, "server requires credentials\n");
		return;
	}

	if (ss < 0)  
	{
		switch(ss)
		{
		case SEC_E_ALGORITHM_MISMATCH:	SSPI_Error(f, "InitializeSecurityContext failed: SEC_E_ALGORITHM_MISMATCH\n");	break;
		case SEC_E_INVALID_HANDLE:		SSPI_Error(f, "InitializeSecurityContext failed: SEC_E_INVALID_HANDLE\n");		break;
		case SEC_E_ILLEGAL_MESSAGE:		SSPI_Error(f, "InitializeSecurityContext failed: SEC_E_ILLEGAL_MESSAGE\n");		break;
		case SEC_E_INVALID_TOKEN:		SSPI_Error(f, "InitializeSecurityContext failed: SEC_E_INVALID_TOKEN\n");		break;
		case SEC_E_INVALID_PARAMETER:	SSPI_Error(f, "InitializeSecurityContext failed: SEC_E_INVALID_PARAMETER\n");	break;
		default:						SSPI_Error(f, "InitializeSecurityContext failed: %lx\n", (long)ss);				break;
		}
		return;
	}

	if ((SEC_I_COMPLETE_NEEDED == ss) || (SEC_I_COMPLETE_AND_CONTINUE == ss))  
	{
		ss = secur.pCompleteAuthToken (&f->sechnd, &OutBuffDesc);
		if (ss < 0)  
		{
			SSPI_Error(f, "CompleteAuthToken failed\n");
			return;
		}
	}

	//its all okay and established if we get this far.
	if (ss == SEC_E_OK)
	{
		SecPkgContext_StreamSizes strsizes;
		CERT_CONTEXT *remotecert;

		secur.pQueryContextAttributesA(&f->sechnd, SECPKG_ATTR_STREAM_SIZES, &strsizes);
		f->headersize = strsizes.cbHeader;
		f->footersize = strsizes.cbTrailer;
		if (f->handshaking != HS_SERVER)
		{	//server takes an annonymous client. client expects a proper certificate.
			if (*f->wpeername)
			{
				ss = secur.pQueryContextAttributesA(&f->sechnd, SECPKG_ATTR_REMOTE_CERT_CONTEXT, &remotecert);
				if (ss != SEC_E_OK)
				{
					f->handshaking = HS_ERROR;
					SSPI_Error(f, "unable to read server's certificate\n");
					return;
				}
				if (VerifyServerCertificate(remotecert, f->wpeername, 0, f->datagram))
				{
					f->handshaking = HS_ERROR;
					SSPI_Error(f, "Error validating certificante\n");
					return;
				}
			}
			else
				Sys_Printf("SSL/TLS Server name not specified, skipping verification\n");
		}

		f->handshaking = HS_ESTABLISHED;
	}

	//send early, send often.
#ifdef HAVE_DTLS
	if (f->transmit)
	{
		for (i = 0; i < OutBuffDesc.cBuffers; i++)
			if (OutSecBuff[i].BufferType == SECBUFFER_TOKEN && OutSecBuff[i].cbBuffer)
				f->transmit(f->cbctx, OutSecBuff[i].pvBuffer, OutSecBuff[i].cbBuffer);
	}
	else
#endif
	{
		i = 0;
		if (SSPI_CopyIntoBuffer(&f->outcrypt, OutSecBuff[i].pvBuffer, OutSecBuff[i].cbBuffer, true) < OutSecBuff[i].cbBuffer)
		{
			SSPI_Error(f, "crypt overflow\n");
			return;
		}
		SSPI_TryFlushCryptOut(f);
	}

	if (f->handshaking == HS_ESTABLISHED)
		SSPI_Encode(f);
	else if (ss == SEC_I_MESSAGE_FRAGMENT)	//looks like we can connect faster if we loop when we get this result.
		if (retries --> 0)
			goto retry;
}

static int QDECL SSPI_ReadBytes (struct vfsfile_s *file, void *buffer, int bytestoread)
{
	sslfile_t *f = (sslfile_t *)file;
	int err = SSPI_CheckNewInCrypt(f);

	if (f->handshaking)
	{
		SSPI_Handshake(f);
		return err;
	}

	SSPI_Encode(f);

	SSPI_Decode(f);

	bytestoread = min(bytestoread, f->inraw.avail);
	if (bytestoread)
	{
		memcpy(buffer, f->inraw.data, bytestoread);
		f->inraw.avail -= bytestoread;
		memmove(f->inraw.data, f->inraw.data + bytestoread, f->inraw.avail);
	}
	else
	{
		if (err)
			return err;
	}
	return bytestoread;
}
static int QDECL SSPI_WriteBytes (struct vfsfile_s *file, const void *buffer, int bytestowrite)
{
	sslfile_t *f = (sslfile_t *)file;

	//don't endlessly accept data faster than we can push it out.
	//we'll buffer a little, but don't go overboard
	if (f->outcrypt.avail > 8192)
		return false;

	bytestowrite = SSPI_CopyIntoBuffer(&f->outraw, buffer, bytestowrite, false);

	if (f->handshaking)
	{
		SSPI_CheckNewInCrypt(f);	//make sure its ticking over
		SSPI_Handshake(f);
	}
	else
	{
		SSPI_Encode(f);
	}

	return bytestowrite;
}
static qboolean QDECL SSPI_Seek (struct vfsfile_s *file, qofs_t pos)
{
	SSPI_Error((sslfile_t*)file, "unable to seek on streams\n");
	return false;
}
static qofs_t QDECL SSPI_Tell (struct vfsfile_s *file)
{
	SSPI_Error((sslfile_t*)file, "unable to seek on streams\n");
	return 0;
}
static qofs_t QDECL SSPI_GetLen (struct vfsfile_s *file)
{
	return 0;
}
static qboolean QDECL SSPI_Close (struct vfsfile_s *file)
{
	sslfile_t *f = (sslfile_t *)file;
	qboolean success = f->stream != NULL;
	SSPI_Error(f, "");

	Z_Free(f->outraw.data);
	Z_Free(f->outcrypt.data);
	Z_Free(f->inraw.data);
	Z_Free(f->incrypt.data);

	Z_Free(f);
	return success;
}

#include <wchar.h>
vfsfile_t *FS_OpenSSL(const char *servername, vfsfile_t *source, qboolean server)
{
	sslfile_t *newf;
	int i = 0;
	int err;
	unsigned int c;
	const char *localname, *peername;

	if (!source || !SSL_Inited())
	{
		if (source)
			VFS_CLOSE(source);
		return NULL;
	}
	if (server)
	{
		localname = servername;
		peername = "";
	}
	else
	{
		localname = "";
		peername = servername;
	}

/*
	if (server)	//unsupported
	{
		VFS_CLOSE(source);
		return NULL;
	}
*/

	newf = Z_Malloc(sizeof(*newf));
	while(*peername)
	{
		c = utf8_decode(&err, peername, (void*)&peername);
		if (c > WCHAR_MAX)
			err = true;	//no 16bit surrogates. they're evil.
		else if (i == sizeof(newf->wpeername)/sizeof(newf->wpeername[0]) - 1)
			err = true; //no space to store it
		else
			newf->wpeername[i++] = c;
		if (err)
		{
			Z_Free(newf);
			VFS_CLOSE(source);
			return NULL;
		}
	}
	newf->wpeername[i] = 0;

	newf->handshaking = server?HS_STARTSERVER:HS_STARTCLIENT;
	newf->stream = source;
	newf->funcs.Close = SSPI_Close;
	newf->funcs.Flush = NULL;
	newf->funcs.GetLen = SSPI_GetLen;
	newf->funcs.ReadBytes = SSPI_ReadBytes;
	newf->funcs.Seek = SSPI_Seek;
	newf->funcs.Tell = SSPI_Tell;
	newf->funcs.WriteBytes = SSPI_WriteBytes;
	newf->funcs.seekstyle = SS_UNSEEKABLE;

	SSPI_ExpandBuffer(&newf->outraw,	8192);
	SSPI_ExpandBuffer(&newf->outcrypt,	8192);
	SSPI_ExpandBuffer(&newf->inraw,		8192);
	SSPI_ExpandBuffer(&newf->incrypt,	8192);

	if (server)
		SSPI_GenServerCredentials(newf);

	return &newf->funcs;
}


#if 0
struct nulldtls_s
{
	void *cbctx;
	neterr_t(*push)(void *cbctx, const qbyte *data, size_t datasize);
};
void *DTLS_CreateContext(void *cbctx, neterr_t(*push)(void *cbctx, const qbyte *data, size_t datasize), qboolean isserver)
{
	struct nulldtls_s *ctx = Z_Malloc(sizeof(*ctx));
	ctx->cbctx = cbctx;
	ctx->push = push;
	return ctx;
}
qboolean DTLS_HasServerCertificate(void)
{
	//FIXME: at this point, schannel is still returning errors when I try acting as a server.
	//so just block any attempt to use this as a server.
	//clients don't need certs!
	return false;
}
neterr_t DTLS_Transmit(void *vctx, const qbyte *data, size_t datasize)
{
	struct nulldtls_s *ctx = vctx;
	neterr_t r;
	*(int*)data ^= 0xdeadbeef;
	r = ctx->push(ctx->cbctx, data, datasize);
	*(int*)data ^= 0xdeadbeef;
	return r;
}
neterr_t DTLS_Received(void *ctx, qbyte *data, size_t datasize)
{
	*(int*)data ^= 0xdeadbeef;
	return NETERR_SENT;
}
#elif defined(HAVE_DTLS)
void *DTLS_CreateContext(char *remotehost, void *cbctx, neterr_t(*push)(void *cbctx, const qbyte *data, size_t datasize), qboolean isserver)
{
	int i = 0;
	sslfile_t *ctx;
	if (!SSL_Inited())
		return NULL;

	ctx = Z_Malloc(sizeof(*ctx));
	ctx->datagram = true;
	ctx->handshaking = isserver?HS_STARTSERVER:HS_STARTCLIENT;
	ctx->cbctx = cbctx;
	ctx->transmit = push;

	while(*remotehost)
	{
		int err;
		int c = utf8_decode(&err, remotehost, (void*)&remotehost);
		if (c > WCHAR_MAX)
			err = true;	//no 16bit surrogates. they're evil.
		else if (i == sizeof(ctx->wpeername)/sizeof(ctx->wpeername[0]) - 1)
			err = true; //no space to store it
		else
			ctx->wpeername[i++] = c;
		if (err)
		{
			Z_Free(ctx);
			return NULL;
		}
	}
	ctx->wpeername[i] = 0;

	SSPI_ExpandBuffer(&ctx->outraw,		8192);
	SSPI_ExpandBuffer(&ctx->outcrypt,	65536);
	SSPI_ExpandBuffer(&ctx->inraw,		8192);
	SSPI_ExpandBuffer(&ctx->incrypt,	65536);

	if (isserver)
		SSPI_GenServerCredentials(ctx);
	else
		SSPI_Handshake(ctx);	//begin the initial handshake now
	return ctx;
}

void DTLS_DestroyContext(void *vctx)
{
	SSPI_Close(vctx);
}


neterr_t DTLS_Transmit(void *ctx, const qbyte *data, size_t datasize)
{
	int ret;
	sslfile_t *f = (sslfile_t *)ctx;

//Con_Printf("DTLS_Transmit: %i\n", datasize);

	//sspi likes writing over the source data. make sure nothing is hurt by copying it out first.
	f->outraw.avail = 0;
	SSPI_CopyIntoBuffer(&f->outraw, data, datasize, true);

	if (f->handshaking)
	{
		SSPI_Handshake(f);

		if (f->handshaking == HS_ERROR)
			ret = NETERR_DISCONNECTED;
		ret = NETERR_CLOGGED;	//not ready yet
	}
	else
	{
		SSPI_Encode(f);
		ret = NETERR_SENT;
	}

	return ret;
}

neterr_t DTLS_Received(void *ctx, qbyte *data, size_t datasize)
{
	int ret;
	sslfile_t *f = (sslfile_t *)ctx;

//Con_Printf("DTLS_Received: %i\n", datasize);

	f->incrypt.data = data;
	f->incrypt.avail = f->incrypt.datasize = datasize;

	if (f->handshaking)
	{
		SSPI_Handshake(f);
		ret = NETERR_CLOGGED;	//not ready yet

		if (f->handshaking == HS_ERROR)
			ret = NETERR_DISCONNECTED;
	}
	else
	{
		SSPI_Decode(f);
		ret = NETERR_SENT;

		memcpy(net_message_buffer, f->inraw.data, f->inraw.avail);
		net_message.cursize = f->inraw.avail;
		f->inraw.avail = 0;

		net_message_buffer[net_message.cursize] = 0;
//		Con_Printf("returning %i bytes: %s\n", net_message.cursize, net_message_buffer);
	}
	f->incrypt.data = NULL;
	return ret;
}
neterr_t DTLS_Timeouts(void *ctx)
{
	sslfile_t *f = (sslfile_t *)ctx;
	if (f->handshaking)
	{
//		SSPI_Handshake(f);
		return NETERR_CLOGGED;
	}
	return NETERR_SENT;
}
#endif

#endif
