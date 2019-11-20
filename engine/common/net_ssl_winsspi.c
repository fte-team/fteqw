#include "quakedef.h"
#if defined(HAVE_WINSSPI)

/*regarding HAVE_DTLS
DTLS1.0 is supported from win8 onwards
Its also meant to be supported from some RDP server patch on win7, but I can't get it to work.
I've given up for now.
*/

#include "winquake.h"
#include "netinc.h"
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

#ifndef SEC_I_MESSAGE_FRAGMENT
#define SEC_I_MESSAGE_FRAGMENT	0x00090364L
#endif
#ifndef SEC_E_INVALID_PARAMETER
#define SEC_E_INVALID_PARAMETER	0x8009035DL
#endif


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
	neterr_t (*transmit)(void *cbctx, const qbyte *data, size_t datasize);
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

char *narrowen(char *out, size_t outlen, wchar_t *wide);
static DWORD VerifyKnownCertificates(DWORD status, wchar_t *domain, qbyte *data, size_t datasize, qboolean datagram)
{
	size_t knownsize;
	void *knowncert;
	char realdomain[256];
	if (datagram)
	{
		if (status == CERT_E_UNTRUSTEDROOT || SUCCEEDED(status))
		{
#ifndef SERVERONLY
			if (CertLog_ConnectOkay(narrowen(realdomain, sizeof(realdomain), domain), data, datasize))
				status = SEC_E_OK;
			else
#endif
				status = TRUST_E_FAIL;
		}
		return status;
	}

	narrowen(realdomain, sizeof(realdomain), domain);
	knowncert = TLS_GetKnownCertificate(realdomain, &knownsize);
	if (knowncert)
	{
		if (knownsize == datasize && !memcmp(data, knowncert, datasize))
		{	//what we know about matched
			if (status == CERT_E_UNTRUSTEDROOT || status == CERT_E_EXPIRED)
				status = SEC_E_OK;
		}
		else
		{
			if (status != CERT_E_EXPIRED)
				Con_Printf("%ls has an unexpected certificate\n", domain);
			if (status == SEC_E_OK)	//we (think) we know better.
				status = TRUST_E_FAIL;
		}
		BZ_Free(knowncert);
	}

#ifndef SERVERONLY
	//self-signed and expired certs are understandable in many situations.
	//prompt and cache (although this connection attempt will fail).
	if (status == CERT_E_UNTRUSTEDROOT || status == CERT_E_UNTRUSTEDTESTROOT || status == CERT_E_EXPIRED)
		if (CertLog_ConnectOkay(realdomain, data, datasize))
			return SEC_E_OK;
#endif

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
				Con_Printf(CON_ERROR "Error verifying certificate for '%ls': %s\n", pwszServerName, err);

				if (tls_ignorecertificateerrors.ival)
				{
					Con_Printf(CON_WARNING "pretending it didn't happen... (tls_ignorecertificateerrors is set)\n");
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
vfsfile_t *SSPI_OpenVFS(const char *servername, vfsfile_t *source, qboolean server)
{
	sslfile_t *newf;
	int i = 0;
	int err;
	unsigned int c;
//	const char *localname;
	const char *peername;

	if (!source || !SSL_Inited())
	{
		if (source)
			VFS_CLOSE(source);
		return NULL;
	}
	if (server)
	{
//		localname = servername;
		peername = "";
	}
	else
	{
//		localname = "";
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

#ifndef SECPKG_ATTR_UNIQUE_BINDINGS
#define SECPKG_ATTR_UNIQUE_BINDINGS 25
typedef struct _SecPkgContext_Bindings
{
	unsigned long        BindingsLength;
	SEC_CHANNEL_BINDINGS *Bindings;
} SecPkgContext_Bindings, *PSecPkgContext_Bindings;
#endif
int SSPI_GetChannelBinding(vfsfile_t *vf, qbyte *binddata, size_t *bindsize)
{
	int ret;
	sslfile_t *f = (sslfile_t*)vf;
	SecPkgContext_Bindings bindings;
	if (vf->Close != SSPI_Close)
		return -2;	//not one of ours.

	bindings.BindingsLength = 0;
	bindings.Bindings = NULL;
	ret = 0;
	switch(secur.pQueryContextAttributesA(&f->sechnd, SECPKG_ATTR_UNIQUE_BINDINGS, &bindings))
	{
	case SEC_E_OK:
		if (bindings.Bindings->cbApplicationDataLength <= *bindsize)
		{
			//will contain 'tls-unique:BINARYDATA'
			*bindsize = bindings.Bindings->cbApplicationDataLength-11;
			memcpy(binddata, ((unsigned char*) bindings.Bindings) + bindings.Bindings->dwApplicationDataOffset+11, bindings.Bindings->cbApplicationDataLength-11);
			ret = 1;
		}
		//FIXME: leak
		//secur.pFreeContextBuffer(bindings.Bindings);
		break;
	case SEC_E_UNSUPPORTED_FUNCTION:
		ret = -1;	//schannel doesn't support it. too old an OS, I guess.
		break;
	default:
		break;
	}
	return ret;
}

#include "netinc.h"
#if 0
struct fakedtls_s
{
	void *cbctx;
	neterr_t(*push)(void *cbctx, const qbyte *data, size_t datasize);
};
static void *FAKEDTLS_CreateContext(const char *remotehost, void *cbctx, neterr_t(*push)(void *cbctx, const qbyte *data, size_t datasize), qboolean isserver)
{
	struct fakedtls_s *ctx = Z_Malloc(sizeof(*ctx));
	ctx->cbctx = cbctx;
	ctx->push = push;
	return ctx;
}
static void FAKEDTLS_DestroyContext(void *vctx)
{
	Z_Free(vctx);
}
static neterr_t FAKEDTLS_Transmit(void *vctx, const qbyte *data, size_t datasize)
{
	struct fakedtls_s *ctx = vctx;
	neterr_t r;
	*(int*)data ^= 0xdeadbeef;
	r = ctx->push(ctx->cbctx, data, datasize);
	*(int*)data ^= 0xdeadbeef;
	return r;
}
static neterr_t FAKEDTLS_Received(void *ctx, qbyte *data, size_t datasize)
{
	*(int*)data ^= 0xdeadbeef;
	return NETERR_SENT;
}
static neterr_t FAKEDTLS_Timeouts(void *ctx)
{
//	fakedtls_s *f = (fakedtls_s *)ctx;
	return NETERR_SENT;
}
static const dtlsfuncs_t dtlsfuncs_fakedtls =
{
	FAKEDTLS_CreateContext,
	FAKEDTLS_DestroyContext,
	FAKEDTLS_Transmit,
	FAKEDTLS_Received,
	FAKEDTLS_Timeouts,
};
const dtlsfuncs_t *FAKEDTLS_InitServer(void)
{
	return &dtlsfuncs_fakedtls;
}
const dtlsfuncs_t *FAKEDTLS_InitClient(void)
{
	return &dtlsfuncs_fakedtls;
}
#elif defined(HAVE_DTLS)
static void *SSPI_DTLS_CreateContext(const char *remotehost, void *cbctx, neterr_t(*push)(void *cbctx, const qbyte *data, size_t datasize), qboolean isserver)
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

static void SSPI_DTLS_DestroyContext(void *vctx)
{
	SSPI_Close(vctx);
}


static neterr_t SSPI_DTLS_Transmit(void *ctx, const qbyte *data, size_t datasize)
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

static neterr_t SSPI_DTLS_Received(void *ctx, qbyte *data, size_t datasize)
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
static neterr_t SSPI_DTLS_Timeouts(void *ctx)
{
	sslfile_t *f = (sslfile_t *)ctx;
	if (f->handshaking)
	{
//		SSPI_Handshake(f);
		return NETERR_CLOGGED;
	}
	return NETERR_SENT;
}

static const dtlsfuncs_t dtlsfuncs_schannel =
{
	SSPI_DTLS_CreateContext,
	SSPI_DTLS_DestroyContext,
	SSPI_DTLS_Transmit,
	SSPI_DTLS_Received,
	SSPI_DTLS_Timeouts,
};
const dtlsfuncs_t *SSPI_DTLS_InitServer(void)
{
	//FIXME: at this point, schannel is still returning errors when I try acting as a server.
	//so just block any attempt to use this as a server.
	//clients don't need/get certs.
	return NULL;
}
const dtlsfuncs_t *SSPI_DTLS_InitClient(void)
{
	return &dtlsfuncs_schannel;
}
#endif

#endif
