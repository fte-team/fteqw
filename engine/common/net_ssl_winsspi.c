#include "quakedef.h"
#if defined(_WIN32) && !defined(_SDL) && defined(HAVE_SSL)
#include <windows.h>
#define SECURITY_WIN32
#include <security.h>
#include <sspi.h>
#include <schannel.h>

//hungarian ensures we hit no macros.
static struct
{
	void *lib;
	SECURITY_STATUS (WINAPI *pDecryptMessage)				(PCtxtHandle,PSecBufferDesc,ULONG,PULONG);
	SECURITY_STATUS (WINAPI *pEncryptMessage)				(PCtxtHandle,ULONG,PSecBufferDesc,ULONG);
	SECURITY_STATUS (WINAPI *pAcquireCredentialsHandleA)	(SEC_CHAR*,SEC_CHAR*,ULONG,PLUID,PVOID,SEC_GET_KEY_FN,PVOID,PCredHandle,PTimeStamp);
	SECURITY_STATUS (WINAPI *pInitializeSecurityContextA)	(PCredHandle,PCtxtHandle,SEC_CHAR*,ULONG,ULONG,ULONG,PSecBufferDesc,ULONG,PCtxtHandle,PSecBufferDesc,PULONG,PTimeStamp);
	SECURITY_STATUS (WINAPI *pCompleteAuthToken)			(PCtxtHandle,PSecBufferDesc);
	SECURITY_STATUS (WINAPI *pQueryContextAttributesA)		(PCtxtHandle,ULONG,PVOID);
	SECURITY_STATUS (WINAPI *pFreeCredentialsHandle)		(PCredHandle);
	SECURITY_STATUS (WINAPI *pDeleteSecurityContext)		(PCtxtHandle);
} secur;
static struct
{
	void *lib;
	BOOL (WINAPI *pCertGetCertificateChain)					(HCERTCHAINENGINE,PCCERT_CONTEXT,LPFILETIME,HCERTSTORE,PCERT_CHAIN_PARA,DWORD,LPVOID,PCCERT_CHAIN_CONTEXT*);
	BOOL (WINAPI *pCertVerifyCertificateChainPolicy)		(LPCSTR,PCCERT_CHAIN_CONTEXT,PCERT_CHAIN_POLICY_PARA,PCERT_CHAIN_POLICY_STATUS);
	void (WINAPI *pCertFreeCertificateChain)				(PCCERT_CHAIN_CONTEXT);
} crypt;
static qboolean SSL_Init(void)
{
	dllfunction_t secur_functable[] =
	{
		{(void**)&secur.pDecryptMessage,				"DecryptMessage"},
		{(void**)&secur.pEncryptMessage,				"EncryptMessage"},
		{(void**)&secur.pAcquireCredentialsHandleA,		"AcquireCredentialsHandleA"},
		{(void**)&secur.pInitializeSecurityContextA,	"InitializeSecurityContextA"},
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
		{NULL, NULL}
	};
	
	if (!secur.lib)
		secur.lib = Sys_LoadLibrary("secur32.dll", secur_functable);
	if (!crypt.lib)
		crypt.lib = Sys_LoadLibrary("crypt32.dll", crypt_functable);

	return !!secur.lib && !!crypt.lib;
}

#define MessageAttribute (ISC_REQ_SEQUENCE_DETECT   | ISC_REQ_REPLAY_DETECT     | ISC_REQ_CONFIDENTIALITY   |  ISC_RET_EXTENDED_ERROR    | ISC_REQ_ALLOCATE_MEMORY   | ISC_REQ_STREAM | ISC_REQ_MANUAL_CRED_VALIDATION) 

struct sslbuf
{
	char data[8192];
	int avail;
	int newd;
};

typedef struct {
	vfsfile_t funcs;
	vfsfile_t *stream;

	wchar_t wpeername[256];
	enum
	{
		HS_ESTABLISHED,

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
} sslfile_t;

static int SSPI_CopyIntoBuffer(struct sslbuf *buf, const void *data, unsigned int bytes)
{
	if (bytes > sizeof(buf->data) - buf->avail)
		bytes = sizeof(buf->data) - buf->avail;
	memcpy(buf->data + buf->avail, data, bytes);
	buf->avail += bytes;

	return bytes;
}

static void SSPI_Error(sslfile_t *f, char *error)
{
	Sys_Printf("%s", error);
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
		sent = VFS_WRITE(f->stream, f->outcrypt.data, f->outcrypt.avail);
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
	newd = VFS_READ(f->stream, f->incrypt.data+f->incrypt.avail, sizeof(f->incrypt.data) - f->incrypt.avail);
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
	SecBuffer			*data = NULL;
	SecBuffer			*extra = NULL;
	int i;

	if (!f->incrypt.avail)
		return;

	BuffDesc.ulVersion    = SECBUFFER_VERSION;
	BuffDesc.cBuffers     = 4;
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
		SSPI_Error(f, "DecryptMessage failed");
		return;
	}

	for (i = 0; i < BuffDesc.cBuffers; i++)
	{
		if (SecBuff[i].BufferType == SECBUFFER_DATA && !data)
			data = &SecBuff[i];
		if (SecBuff[i].BufferType == SECBUFFER_EXTRA && !extra)
			extra = &SecBuff[i];
	}

	//copy the data out to the result, yay.
	if (data)
		SSPI_CopyIntoBuffer(&f->inraw, data->pvBuffer, data->cbBuffer);

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

	ss = secur.pEncryptMessage(&f->sechnd, ulQop, &BuffDesc, 0);

	if (ss < 0)
	{
		SSPI_Error(f, "EncryptMessage failed");
		return;
	}

	f->outraw.avail = 0;

	//fixme: these should be made non-fatal.
	if (SSPI_CopyIntoBuffer(&f->outcrypt, SecBuff[0].pvBuffer, SecBuff[0].cbBuffer) < SecBuff[0].cbBuffer)
	{
		SSPI_Error(f, "crypt buffer overflowed");
		return;
	}
	if (SSPI_CopyIntoBuffer(&f->outcrypt, SecBuff[1].pvBuffer, SecBuff[1].cbBuffer) < SecBuff[1].cbBuffer)
	{
		SSPI_Error(f, "crypt buffer overflowed");
		return;
	}
	if (SSPI_CopyIntoBuffer(&f->outcrypt, SecBuff[2].pvBuffer, SecBuff[2].cbBuffer) < SecBuff[2].cbBuffer)
	{
		SSPI_Error(f, "crypt buffer overflowed");
		return;
	}

	SSPI_TryFlushCryptOut(f);
}

static DWORD VerifyServerCertificate(PCCERT_CONTEXT pServerCert, PWSTR pwszServerName, DWORD dwCertFlags)
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
        Sys_Printf("Error 0x%x returned by CertGetCertificateChain!\n", Status);
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
			Sys_Printf("Error 0x%x returned by CertVerifyCertificateChainPolicy!\n", Status);
		}
		else
		{
			if (PolicyStatus.dwError)
			{
				char *err;
				Status = PolicyStatus.dwError;
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
					case CERT_E_CN_NO_MATCH:            err = "CERT_E_CN_NO_MATCH";             break;
					case CERT_E_WRONG_USAGE:            err = "CERT_E_WRONG_USAGE";             break;
					default:                            err = "(unknown)";                      break;
				}
				Sys_Printf("Error verifying certificate for %s: %s\n", pwszServerName, err);
			}
			else
				Status = SEC_E_OK;
		}
		crypt.pCertFreeCertificateChain(pChainContext);
	}

    return Status;
}

static void SSPI_Handshake (sslfile_t *f)
{
	SECURITY_STATUS   ss;
	TimeStamp         Lifetime;
	SecBufferDesc     OutBuffDesc;
	SecBuffer         OutSecBuff;
	SecBufferDesc     InBuffDesc;
	SecBuffer         InSecBuff[2];
	ULONG             ContextAttributes;
	SCHANNEL_CRED SchannelCred;

	if (f->outcrypt.avail)
	{
		//don't let things build up too much
		SSPI_TryFlushCryptOut(f);
		if (f->outcrypt.avail)
			return;
	}

	OutBuffDesc.ulVersion = SECBUFFER_VERSION;
	OutBuffDesc.cBuffers  = 1;
	OutBuffDesc.pBuffers  = &OutSecBuff;

	OutSecBuff.cbBuffer   = sizeof(f->outcrypt.data) - f->outcrypt.avail;
	OutSecBuff.BufferType = SECBUFFER_TOKEN;
	OutSecBuff.pvBuffer   = f->outcrypt.data + f->outcrypt.avail;

	if (f->handshaking == HS_STARTCLIENT)
	{
		//no input data yet.
		f->handshaking = HS_CLIENT;

		memset(&SchannelCred, 0, sizeof(SchannelCred));
		SchannelCred.dwVersion = SCHANNEL_CRED_VERSION;
		SchannelCred.grbitEnabledProtocols = SP_PROT_TLS1 | SP_PROT_SSL3;
		SchannelCred.dwFlags |= SCH_CRED_NO_DEFAULT_CREDS;	/*don't use windows login info or anything*/

		ss = secur.pAcquireCredentialsHandleA (NULL, UNISP_NAME_A, SECPKG_CRED_OUTBOUND, NULL, &SchannelCred, NULL, NULL, &f->cred, &Lifetime);
		if (ss < 0)
		{
			SSPI_Error(f, "AcquireCredentialsHandle failed\n");
			return;
		}

		ss = secur.pInitializeSecurityContextA (&f->cred, NULL, NULL, MessageAttribute, 0, SECURITY_NATIVE_DREP, NULL, 0, &f->sechnd, &OutBuffDesc, &ContextAttributes, &Lifetime);
	}
	else
	{
		//only if we actually have data.
		if (!f->incrypt.avail)
			return;

		InBuffDesc.ulVersion = SECBUFFER_VERSION;
		InBuffDesc.cBuffers  = 2;
		InBuffDesc.pBuffers  = InSecBuff;

		InSecBuff[0].BufferType = SECBUFFER_TOKEN;
		InSecBuff[0].cbBuffer   = f->incrypt.avail;
		InSecBuff[0].pvBuffer   = f->incrypt.data;

		InSecBuff[1].BufferType = SECBUFFER_EMPTY;
		InSecBuff[1].pvBuffer   = NULL;
		InSecBuff[1].cbBuffer   = 0;

		ss = secur.pInitializeSecurityContextA (&f->cred, &f->sechnd, NULL, MessageAttribute, 0, SECURITY_NATIVE_DREP, &InBuffDesc, 0, &f->sechnd, &OutBuffDesc, &ContextAttributes, &Lifetime);

		if (ss == SEC_E_INCOMPLETE_MESSAGE)
			return;

		//any extra data should still remain for the next time around. this might be more handshake data or payload data.
		if (InSecBuff[1].BufferType == SECBUFFER_EXTRA)
		{
			memmove(f->incrypt.data, f->incrypt.data + (f->incrypt.avail - InSecBuff[1].cbBuffer), InSecBuff[1].cbBuffer);
			f->incrypt.avail = InSecBuff[1].cbBuffer;
		}
		else f->incrypt.avail = 0;
	}

	if (ss == SEC_I_INCOMPLETE_CREDENTIALS)
	{
		SSPI_Error(f, "server requires credentials\n");
		return;
	}

	if (ss < 0)  
	{
		SSPI_Error(f, "InitializeSecurityContext failed\n");
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

	if (SSPI_CopyIntoBuffer(&f->outcrypt, OutSecBuff.pvBuffer, OutSecBuff.cbBuffer) < OutSecBuff.cbBuffer)
	{
		SSPI_Error(f, "crypt overflow\n");
		return;
	}

	//send early, send often.
	SSPI_TryFlushCryptOut(f);

	//its all okay and established if we get this far.
	if (ss == SEC_E_OK)
	{
		SecPkgContext_StreamSizes strsizes;
		CERT_CONTEXT *remotecert;

		secur.pQueryContextAttributesA(&f->sechnd, SECPKG_ATTR_STREAM_SIZES, &strsizes);
		f->headersize = strsizes.cbHeader;
		f->footersize = strsizes.cbTrailer;
		f->handshaking = HS_ESTABLISHED;

		if (*f->wpeername)
		{
			ss = secur.pQueryContextAttributesA(&f->sechnd, SECPKG_ATTR_REMOTE_CERT_CONTEXT, &remotecert);
			if (ss != SEC_E_OK)
			{
				SSPI_Error(f, "unable to read server's certificate\n");
				return;
			}
			if (VerifyServerCertificate(remotecert, f->wpeername, 0))
				SSPI_Error(f, "Error validating certificante");
		}
		else
			Sys_Printf("SSL/TLS Server name not specified, skipping verification\n");


		SSPI_Encode(f);
	}
}

static int SSPI_ReadBytes (struct vfsfile_s *file, void *buffer, int bytestoread)
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
static int SSPI_WriteBytes (struct vfsfile_s *file, const void *buffer, int bytestowrite)
{
	sslfile_t *f = (sslfile_t *)file;

	bytestowrite = SSPI_CopyIntoBuffer(&f->outraw, buffer, bytestowrite);

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
static qboolean SSPI_Seek (struct vfsfile_s *file, unsigned long pos)
{
	SSPI_Error((sslfile_t*)file, "unable to seek on streams");
	return false;
}
static unsigned long SSPI_Tell (struct vfsfile_s *file)
{
	SSPI_Error((sslfile_t*)file, "unable to seek on streams");
	return 0;
}
static unsigned long SSPI_GetLen (struct vfsfile_s *file)
{
	return 0;
}
static void SSPI_Close (struct vfsfile_s *file)
{
	SSPI_Error((sslfile_t*)file, "");
	Z_Free(file);
}

#include <wchar.h>
vfsfile_t *FS_OpenSSL(const char *hostname, vfsfile_t *source, qboolean server)
{
	sslfile_t *newf;
	int i = 0;
	int err;
	unsigned int c;

	if (!source || !SSL_Init())
	{
		VFS_CLOSE(source);
		return NULL;
	}
	if (!hostname)
		hostname = "";

	if (server)	//unsupported
	{
		VFS_CLOSE(source);
		return NULL;
	}

	newf = Z_Malloc(sizeof(*newf));
	while(*hostname)
	{
		c = utf8_decode(&err, hostname, (void*)&hostname);
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
	newf->funcs.seekingisabadplan = true;

	return &newf->funcs;
}
#endif
