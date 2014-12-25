#include "quakedef.h"
#if defined(HAVE_WINSSPI)
cvar_t *tls_ignorecertificateerrors;

#include "winquake.h"
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
	SECURITY_STATUS (WINAPI *pAcceptSecurityContext)		(PCredHandle,PCtxtHandle,PSecBufferDesc,unsigned long,unsigned long,PCtxtHandle,PSecBufferDesc,unsigned long SEC_FAR *,PTimeStamp);
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
	DWORD (WINAPI *pCertNameToStrA)							(DWORD dwCertEncodingType, PCERT_NAME_BLOB pName, DWORD dwStrType, LPTSTR psz, DWORD csz);

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
		{(void**)&secur.pInitializeSecurityContextA,	"InitializeSecurityContextA"},
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
	f->handshaking = HS_ERROR;
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
		if (ss == SEC_E_INCOMPLETE_MESSAGE)
			return;	//no error if its incomplete, we can just get more data later on.
		switch(ss)
		{
		case SEC_E_INVALID_HANDLE:	SSPI_Error(f, "DecryptMessage failed: SEC_E_INVALID_HANDLE\n"); break;
		default:					SSPI_Error(f, va("DecryptMessage failed: %0#x\n", ss)); break;
		}
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
		SSPI_Error(f, "EncryptMessage failed\n");
		return;
	}

	f->outraw.avail = 0;

	//fixme: these should be made non-fatal.
	if (SSPI_CopyIntoBuffer(&f->outcrypt, SecBuff[0].pvBuffer, SecBuff[0].cbBuffer) < SecBuff[0].cbBuffer)
	{
		SSPI_Error(f, "crypt buffer overflowed\n");
		return;
	}
	if (SSPI_CopyIntoBuffer(&f->outcrypt, SecBuff[1].pvBuffer, SecBuff[1].cbBuffer) < SecBuff[1].cbBuffer)
	{
		SSPI_Error(f, "crypt buffer overflowed\n");
		return;
	}
	if (SSPI_CopyIntoBuffer(&f->outcrypt, SecBuff[2].pvBuffer, SecBuff[2].cbBuffer) < SecBuff[2].cbBuffer)
	{
		SSPI_Error(f, "crypt buffer overflowed\n");
		return;
	}

	SSPI_TryFlushCryptOut(f);
}

//these are known sites that use self-signed certificates, or are special enough that we don't trust corporate networks to hack in their own certificate authority for a proxy/mitm
static struct
{
	wchar_t *hostname;
	unsigned int datasize;
	qbyte *data;
	//FIXME: include expiry information
	//FIXME: add alternative when one is about to expire
} knowncerts[] = {
	{L"triptohell.info", 933, "\x30\x82\x03\xa1\x30\x82\x02\x89\xa0\x03\x02\x01\x02\x02\x09\x00\x8b\xd0\x05\x63\x62\xd1\x6a\xe3\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x05\x05\x00\x30\x67\x31\x0b\x30\x09\x06\x03\x55\x04\x06\x13\x02\x42\x44\x31\x0c\x30\x0a\x06\x03\x55\x04\x08\x0c\x03\x42\x61\x64\x31\x0c\x30\x0a\x06\x03\x55\x04\x07\x0c\x03\x42\x61\x64\x31\x0c\x30\x0a\x06\x03\x55\x04\x0a\x0c\x03\x42\x61\x64\x31\x0c\x30\x0a\x06\x03\x55\x04\x0b\x0c\x03\x42\x61\x64\x31\x0c\x30\x0a\x06\x03\x55\x04\x03\x0c\x03\x42\x61\x64\x31\x12\x30\x10\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x09\x01\x16\x03\x42\x61\x64\x30\x1e\x17\x0d\x31\x34\x31\x32\x32\x34\x32\x32\x34\x32\x34\x37\x5a\x17\x0d\x32\x34\x31\x32\x32\x31\x32\x32\x34\x32\x34\x37\x5a\x30\x67\x31\x0b\x30\x09\x06\x03\x55\x04\x06\x13\x02\x42\x44\x31\x0c\x30\x0a\x06\x03\x55\x04\x08\x0c\x03\x42\x61\x64\x31\x0c\x30\x0a\x06\x03\x55\x04\x07\x0c\x03\x42\x61\x64\x31\x0c\x30\x0a\x06\x03\x55\x04\x0a\x0c\x03\x42\x61\x64\x31\x0c\x30\x0a\x06\x03\x55\x04\x0b\x0c\x03\x42\x61\x64\x31\x0c\x30\x0a\x06\x03\x55\x04\x03\x0c\x03\x42\x61\x64\x31\x12\x30\x10\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x09\x01\x16\x03\x42\x61\x64\x30\x82\x01\x22\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01\x05\x00\x03\x82\x01\x0f\x00\x30\x82\x01\x0a\x02\x82\x01\x01\x00\xaf\x10\x33\xfa\x39\xf5\xae\x2c\x91\x0e\x20\xe6\x3c\x5c\x7c\x1e\xeb\x16\x50\x2f\x05\x30\xfe\x67\xee\xa9\x00\x54\xd9\x4a\x86\xe6\xba\x80\xfb\x1a\x80\x08\x7e\x7b\x13\xe5\x1a\x18\xc9\xd4\x70\xbd\x5d\xc4\x38\xef\x64\xf1\x90\x2c\x53\x49\x93\x24\x36\x3e\x11\x59\x69\xa6\xdf\x37\xb2\x54\x82\x28\x3e\xdd\x30\x75\xa0\x18\xd8\xe1\xf5\x52\x73\x12\x5b\x37\x68\x1c\x59\xbd\x8c\x73\x66\x47\xbc\xcb\x9c\xfe\x38\x92\x8f\x74\xe9\xd1\x2f\x96\xd2\x5d\x6d\x11\x59\xb2\xdc\xbd\x8c\x37\x5b\x22\x76\x98\xe7\xbe\x08\xef\x1e\x99\xc4\xa9\x77\x2c\x9c\x0e\x08\x3c\x8e\xab\x97\x0c\x6a\xd7\x03\xab\xfd\x4a\x1e\x95\xb2\xc2\x9c\x3a\x16\x65\xd7\xaf\x45\x5f\x6e\xe7\xce\x51\xba\xa0\x60\x43\x0e\x07\xc5\x0b\x0a\x82\x05\x26\xc4\x92\x0a\x27\x5b\xfc\x57\x6c\xdf\xe2\x54\x8a\xef\x38\xf1\xf8\xc4\xf8\x51\x16\x27\x1f\x78\x89\x7c\x5b\xd7\x53\xcd\x9b\x54\x2a\xe6\x71\xee\xe4\x56\x2e\xa4\x09\x1a\x61\xf7\x0f\x97\x22\x94\xd7\xef\x21\x6c\xe6\x81\xfb\x54\x5f\x09\x92\xac\xd2\x7c\xab\xd5\xa9\x81\xf4\xc9\xb7\xd6\xbf\x68\xf8\x4f\xdc\xf3\x60\xa3\x3b\x29\x92\x9e\xdd\xa2\xa3\x02\x03\x01\x00\x01\xa3\x50\x30\x4e\x30\x1d\x06\x03\x55\x1d\x0e\x04\x16\x04\x14\x19\xed\xd0\x7b\x16\xaf\xb5\x0c\x9a\xe8\xd3\x46\x2e\x3c\x64\x29\xb6\xc1\x73\x5a\x30\x1f\x06\x03\x55\x1d\x23\x04\x18\x30\x16\x80\x14\x19\xed\xd0\x7b\x16\xaf\xb5\x0c\x9a\xe8\xd3\x46\x2e\x3c\x64\x29\xb6\xc1\x73\x5a\x30\x0c\x06\x03\x55\x1d\x13\x04\x05\x30\x03\x01\x01\xff\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x05\x05\x00\x03\x82\x01\x01\x00\x62\xa7\x26\xeb\xd4\x03\x29\x9c\x09\x33\x69\x7a\x9c\x65\x68\xec\x4c\xb9\x06\xeb\x1e\x51\x6f\x78\x20\xdc\xf6\x44\x5e\x06\x6e\x53\x87\x73\xe6\x14\x15\xb9\x17\x74\x67\xe0\x4e\x48\x38\xbc\x1c\xbd\xd0\xad\xd6\xbd\x8c\xf0\x3a\xe0\x13\x73\x19\xad\x8b\x79\x68\x67\x65\x9b\x7a\x4c\x81\xfb\xd9\x92\x77\x89\xb5\xb0\x53\xb0\xa5\xf7\x2d\x8e\x29\x60\x31\xd1\x9b\x2f\x63\x8a\x5f\x64\xc1\x61\xd5\xb7\xdf\x70\x3b\x2b\xf6\x1a\x96\xb9\xa7\x08\xca\x87\xa6\x8c\x60\xca\x6e\xd7\xee\xba\xef\x89\x0b\x93\xd5\xfd\xfc\x14\xba\xef\x27\xba\x90\x11\x90\xf7\x25\x70\xe7\x4e\xf4\x9c\x13\x27\xc1\xa7\x8e\xd9\x66\x43\x72\x20\x5b\xe1\x5c\x73\x74\xf5\x33\xf2\xa5\xf6\xe1\xd5\xac\xf3\x67\x5c\xe7\xd4\x0a\x8d\x91\x73\x03\x3e\x9d\xbc\x96\xc3\x0c\xdb\xd5\x77\x6e\x76\x44\x69\xaf\x24\x0f\x4f\x8b\x47\x36\x8b\xc3\xd6\x36\xdd\x26\x5a\x9c\xdd\x9c\x43\xee\x29\x43\xdd\x75\x2f\x19\x52\xfc\x1d\x24\x9c\x13\x29\x99\xa0\x6d\x7a\x95\xcc\xa0\x58\x86\xd8\xc5\xb9\xa3\xc2\x3d\x64\x1d\x85\x8a\xca\x53\x55\x8e\x9a\x6d\xc9\x91\x73\xf4\xe1\xe1\xa4\x9b\x76\xfc\x7f\x63\xc2\xb9\x23"},
	{NULL}
};

char *narrowen(char *out, size_t outlen, wchar_t *wide);
static DWORD VerifyKnownCertificates(DWORD status, wchar_t *domain, qbyte *data, size_t datasize)
{
	int i;
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
				continue;
			}
#endif
			if (knowncerts[i].datasize != datasize || memcmp(data, knowncerts[i].data, datasize))
			{
				if (status != CERT_E_EXPIRED)
					Con_Printf("%ls has an unexpected certificate\n", domain);
				if (status == SEC_E_OK)
					status = TRUST_E_FAIL;
			}
			else
			{
				if (status == CERT_E_UNTRUSTEDROOT)
					status = SEC_E_OK;
				break;
			}
		}
	}
	return status;
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
        Sys_Printf("Error 0x%x returned by CertGetCertificateChain!\n", (unsigned int)Status);
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
			Sys_Printf("Error 0x%x returned by CertVerifyCertificateChainPolicy!\n", (unsigned int)Status);
		}
		else
		{
			Status = VerifyKnownCertificates(PolicyStatus.dwError, pwszServerName, pServerCert->pbCertEncoded, pServerCert->cbCertEncoded);
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
	sigalg.pszObjId = szOID_RSA_SHA1RSA;

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
	SchannelCred.grbitEnabledProtocols = (SP_PROT_TLS1|SP_PROT_SSL3) & SP_PROT_SERVERS;
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
	SecBuffer			OutSecBuff;
	SecBufferDesc		InBuffDesc;
	SecBuffer			InSecBuff[2];
	ULONG				ContextAttributes;
	SCHANNEL_CRED SchannelCred;

	if (f->outcrypt.avail)
	{
		//don't let things build up too much
		SSPI_TryFlushCryptOut(f);
		if (f->outcrypt.avail)
			return;
	}

	//FIXME: skip this if we've had no new data since last time

	OutBuffDesc.ulVersion = SECBUFFER_VERSION;
	OutBuffDesc.cBuffers  = 1;
	OutBuffDesc.pBuffers  = &OutSecBuff;

	OutSecBuff.cbBuffer   = sizeof(f->outcrypt.data) - f->outcrypt.avail;
	OutSecBuff.BufferType = SECBUFFER_TOKEN;
	OutSecBuff.pvBuffer   = f->outcrypt.data + f->outcrypt.avail;

	if (f->handshaking == HS_ERROR)
		return;	//gave up.
	else if (f->handshaking == HS_STARTCLIENT)
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
	else if (f->handshaking == HS_CLIENT)
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
	else if (f->handshaking == HS_STARTSERVER || f->handshaking == HS_SERVER)
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

		ss = secur.pAcceptSecurityContext(&f->cred, (f->handshaking==HS_SERVER)?&f->sechnd:NULL, &InBuffDesc, ASC_REQ_ALLOCATE_MEMORY|ASC_REQ_STREAM|ASC_REQ_CONFIDENTIALITY, SECURITY_NATIVE_DREP, &f->sechnd, &OutBuffDesc, &ContextAttributes, &Lifetime); 

		if (ss == SEC_E_INCOMPLETE_MESSAGE)
			return;
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
		default:						SSPI_Error(f, va("InitializeSecurityContext failed: %#x\n", ss));				break;
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
				if (VerifyServerCertificate(remotecert, f->wpeername, 0))
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

		SSPI_Encode(f);
	}
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
	Z_Free(f);
	return success;
}

#include <wchar.h>
vfsfile_t *FS_OpenSSL(const char *hostname, vfsfile_t *source, qboolean server)
{
	sslfile_t *newf;
	int i = 0;
	int err;
	unsigned int c;

	if (!source || !SSL_Inited())
	{
		VFS_CLOSE(source);
		return NULL;
	}
	if (!hostname)
		hostname = "";

/*
	if (server)	//unsupported
	{
		VFS_CLOSE(source);
		return NULL;
	}
*/

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

	if (server)
		SSPI_GenServerCredentials(newf);

	return &newf->funcs;
}
#endif
