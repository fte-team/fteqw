#include "progsint.h"
//#include "qcc.h"

#ifndef NO_ZLIB
#define AVAIL_ZLIB
#endif

#ifdef AVAIL_ZLIB
#ifdef _WIN32
#define ZEXPORT VARGS
#include <zlib.h>

#ifdef _WIN64
//# pragma comment (lib, "../libs/zlib64.lib") 
#else
//# pragma comment (lib, "../libs/zlib.lib") 
#endif
#else
#include <zlib.h>
#endif
#endif

pbool QC_decodeMethodSupported(int method)
{
	if (method == 0)
		return true;
	if (method == 1)
		return true;
	if (method == 2)
	{
#ifdef AVAIL_ZLIB
		return false;
#endif
	}
	return false;
}

char *QC_decode(progfuncs_t *progfuncs, int complen, int len, int method, char *info, char *buffer)
{
	int i;
	if (method == 0)	//copy
	{
		if (complen != len) Sys_Error("lengths do not match");
		memcpy(buffer, info, len);		
	}
	else if (method == 1)	//xor encryption
	{
		if (complen != len) Sys_Error("lengths do not match");
		for (i = 0; i < len; i++)
			buffer[i] = info[i] ^ 0xA5;		
	}
	else if (method == 2)	//compression (ZLIB)
	{
#ifdef AVAIL_ZLIB
		z_stream strm = {
			info,
			complen,
			0,

			buffer,
			len,
			0,

			NULL,
			NULL,

			NULL,
			NULL,
			NULL,

			Z_BINARY,
			0,
			0
		};

		inflateInit(&strm);
		if (Z_STREAM_END != inflate(&strm, Z_FINISH))	//decompress it in one go.
			Sys_Error("Failed block decompression\n");
		inflateEnd(&strm);
#endif
	}
	//add your decryption/decompression routine here.
	else
		Sys_Error("Bad file encryption routine\n");


	return buffer;
}

#if !defined(MINIMAL) && !defined(OMIT_QCC)
int QC_encodecrc(int len, char *in)
{
#ifdef AVAIL_ZLIB
	return crc32(0, in, len);
#else
	return 0;
#endif
}
void SafeWrite(int hand, void *buf, long count);
int SafeSeek(int hand, int ofs, int mode);
//we are allowed to trash our input here.
int QC_encode(progfuncs_t *progfuncs, int len, int method, char *in, int handle)
{
	int i;
	if (method == 0) //copy, allows a lame pass-through.
	{		
		SafeWrite(handle, in, len);
		return len;
	}
	else if (method == 1)	//xor encryption, not secure. maybe useful for the string table.
	{
		for (i = 0; i < len; i++)
			in[i] = in[i] ^ 0xA5;
		SafeWrite(handle, in, len);
		return len;
	}
	else if (method == 2 || method == 8)	//compression (ZLIB)
	{
#ifdef AVAIL_ZLIB
		char out[8192];

		z_stream strm = {
			in,
			len,
			0,

			out,
			sizeof(out),
			0,

			NULL,
			NULL,

			NULL,
			NULL,
			NULL,

			Z_BINARY,
			0,
			0
		};
		i=0;

		if (method == 8)
			deflateInit2(&strm, 9, Z_DEFLATED, -MAX_WBITS, 9, Z_DEFAULT_STRATEGY);		//zip deflate compression
		else
			deflateInit(&strm, Z_BEST_COMPRESSION);	//zlib compression
		while(deflate(&strm, Z_FINISH) == Z_OK)
		{
			SafeWrite(handle, out, sizeof(out) - strm.avail_out);	//compress in chunks of 8192. Saves having to allocate a huge-mega-big buffer
			i+=sizeof(out) - strm.avail_out;
			strm.next_out = out;
			strm.avail_out = sizeof(out);
		}
		SafeWrite(handle, out, sizeof(out) - strm.avail_out);
		i+=sizeof(out) - strm.avail_out;
		deflateEnd(&strm);
		return i;
#endif
		Sys_Error("ZLIB compression not supported in this build");
		return 0;
	}
	//add your compression/decryption routine here.
	else
	{
		Sys_Error("Wierd method");
		return 0;
	}
}
#endif

char *PDECL filefromprogs(pubprogfuncs_t *ppf, progsnum_t prnum, char *fname, size_t *size, char *buffer)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	int num;
	includeddatafile_t *s;
	if (size)
		*size = 0;
	if (!pr_progstate[prnum].progs)
		return NULL;
	if (pr_progstate[prnum].progs->version != PROG_EXTENDEDVERSION)
		return NULL;
	if (!pr_progstate[prnum].progs->secondaryversion != PROG_SECONDARYVERSION16 &&
		!pr_progstate[prnum].progs->secondaryversion != PROG_SECONDARYVERSION32)
		return NULL;

	num = *(int*)((char *)pr_progstate[prnum].progs + pr_progstate[prnum].progs->ofsfiles);
	s = (includeddatafile_t *)((char *)pr_progstate[prnum].progs + pr_progstate[prnum].progs->ofsfiles+4);	
	while(num>0)
	{
		if (!strcmp(s->filename, fname))
		{
			if (size)
				*size = s->size;
			if (!buffer)
				return NULL;
			return QC_decode(progfuncs, s->compsize, s->size, s->compmethod, (char *)pr_progstate[prnum].progs+s->ofs, buffer);
		}

		s++;
		num--;
	}	

	if (size)
		*size = 0;
	return NULL;
}

/*
char *filefromnewprogs(progfuncs_t *progfuncs, char *prname, char *fname, int *size, char *buffer)
{
	int num;
	includeddatafile_t *s;	
	progstate_t progs;
	if (!PR_ReallyLoadProgs(progfuncs, prname, -1, &progs, false))
	{
		if (size)
			*size = 0;
		return NULL;
	}

	if (progs.progs->version < PROG_EXTENDEDVERSION)
		return NULL;
	if (!progs.progs->ofsfiles)
		return NULL;

	num = *(int*)((char *)progs.progs + progs.progs->ofsfiles);
	s = (includeddatafile_t *)((char *)progs.progs + progs.progs->ofsfiles+4);	
	while(num>0)
	{
		if (!strcmp(s->filename, fname))
		{
			if (size)
				*size = s->size;
			if (!buffer)
				return (char *)0xffffffff;
			return QC_decode(progfuncs, s->compsize, s->size, s->compmethod, (char *)progs.progs+s->ofs, buffer);
		}

		s++;
		num--;
	}	

	if (size)
		*size = 0;
	return NULL;
}
*/
