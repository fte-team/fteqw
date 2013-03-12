// cmdlib.c

#include "qcc.h"
#include <ctype.h>
//#include <sys/time.h>

#define PATHSEPERATOR   '/'

#ifndef QCC
extern jmp_buf qcccompileerror;
#endif

#ifdef _WIN64
	#ifdef _SDL
		#define snprintf linuxlike_snprintf
		int VARGS linuxlike_snprintf(char *buffer, int size, const char *format, ...) LIKEPRINTF(3);
		#define vsnprintf linuxlike_vsnprintf
		int VARGS linuxlike_vsnprintf(char *buffer, int size, const char *format, va_list argptr);
		void *__imp__vsnprintf = vsnprintf;
	#endif
#endif

// set these before calling CheckParm
int myargc;
char **myargv;

char	qcc_token[1024];
int		qcc_eof;

const unsigned int		type_size[12] = {1,	//void
						sizeof(string_t)/4,	//string
						1,	//float
						3,	//vector
						1,	//entity
						1,	//field
						sizeof(func_t)/4,//function
						1,  //pointer (its an int index)
						1,	//integer
						3,	//fixme: how big should a variant be?
						0,	//ev_struct. variable sized.
						0	//ev_union. variable sized.
						};

/*
============================================================================

					BYTE ORDER FUNCTIONS

============================================================================
*/
short   (*PRBigShort) (short l);
short   (*PRLittleShort) (short l);
int     (*PRBigLong) (int l);
int     (*PRLittleLong) (int l);
float   (*PRBigFloat) (float l);
float   (*PRLittleFloat) (float l);


short   QCC_SwapShort (short l)
{
	qbyte    b1,b2;

	b1 = l&255;
	b2 = (l>>8)&255;

	return (b1<<8) + b2;
}

short   QCC_Short (short l)
{
	return l;
}


int    QCC_SwapLong (int l)
{
	qbyte    b1,b2,b3,b4;

	b1 = (qbyte)l;
	b2 = (qbyte)(l>>8);
	b3 = (qbyte)(l>>16);
	b4 = (qbyte)(l>>24);

	return ((int)b1<<24) + ((int)b2<<16) + ((int)b3<<8) + b4;
}

int    QCC_Long (int l)
{
	return l;
}


float	QCC_SwapFloat (float l)
{
	union {qbyte b[4]; float f;} in, out;

	in.f = l;
	out.b[0] = in.b[3];
	out.b[1] = in.b[2];
	out.b[2] = in.b[1];
	out.b[3] = in.b[0];

	return out.f;
}

float	QCC_Float (float l)
{
	return l;
}

void SetEndian(void)
{
	union {qbyte b[2]; unsigned short s;} ed;
	ed.s = 255;
	if (ed.b[0] == 255)
	{
		PRBigShort		= QCC_SwapShort;
		PRLittleShort	= QCC_Short;
		PRBigLong		= QCC_SwapLong;
		PRLittleLong	= QCC_Long;
		PRBigFloat		= QCC_SwapFloat;
		PRLittleFloat	= QCC_Float;
	}
	else
	{
		PRBigShort		= QCC_Short;
		PRLittleShort	= QCC_SwapShort;
		PRBigLong		= QCC_Long;
		PRLittleLong	= QCC_SwapLong;
		PRBigFloat		= QCC_Float;
		PRLittleFloat	= QCC_SwapFloat;
	}
}



#ifndef MINIMAL
/*
================
I_FloatTime
================
*/
/*
double I_FloatTime (void)
{
	struct timeval tp;
	struct timezone tzp;
	static int		secbase;

	gettimeofday(&tp, &tzp);

	if (!secbase)
	{
		secbase = tp.tv_sec;
		return tp.tv_usec/1000000.0;
	}

	return (tp.tv_sec - secbase) + tp.tv_usec/1000000.0;
}

  */


#ifdef QCC
int QC_strncasecmp (const char *s1, const char *s2, int n)
{
	int             c1, c2;

	while (1)
	{
		c1 = *s1++;
		c2 = *s2++;

		if (!n--)
			return 0;               // strings are equal until end point

		if (c1 != c2)
		{
			if (c1 >= 'a' && c1 <= 'z')
				c1 -= ('a' - 'A');
			if (c2 >= 'a' && c2 <= 'z')
				c2 -= ('a' - 'A');
			if (c1 != c2)
				return -1;              // strings not equal
		}
		if (!c1)
			return 0;               // strings are equal
//              s1++;
//              s2++;
	}

	return -1;
}

int QC_strcasecmp (const char *s1, const char *s2)
{
	return QC_strncasecmp(s1, s2, 0x7fffffff);
}

#else
int QC_strncasecmp(const char *s1, const char *s2, int n);
int QC_strcasecmp (const char *s1, const char *s2)
{
	return QC_strncasecmp(s1, s2, 0x7fffffff);
}

#endif



#endif	//minimal
/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *QCC_COM_Parse (char *data)
{
	int		c;
	int		len;

	len = 0;
	qcc_token[0] = 0;

	if (!data)
		return NULL;

// skip whitespace
skipwhite:
	while ((c = *data) && qcc_iswhite(c))
		data++;
	if (!c)
	{
		qcc_eof = true;
		return NULL;
	}

// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

	// skip /* comments
	if (c=='/' && data[1] == '*')
	{
		while (data[1] && (data[0] != '*' || data[1] != '/'))
			data++;
		data+=2;
		goto skipwhite;
	}


// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		do
		{
			c = *data++;
			if (c=='\\' && *data == '\"')
				c = *data++;	//allow C-style string escapes
			else if (c=='\\' && *data == '\\')
				c = *data++;	// \ is now a special character so it needs to be marked up using itself
			else if (c=='\\' && *data == 'n')
			{					// and do new lines while we're at it.
				c = '\n';
				data++;
			}
			else if (c=='\"')
			{
				qcc_token[len] = 0;
				return data;
			}
			else if (c=='\0'||c=='\n')
			{
				qcc_token[len] = 0;
				return data;
			}
			if (len >= sizeof(qcc_token)-1)
				;
			else
				qcc_token[len] = c;
			len++;
		} while (1);
	}

// parse single characters
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c==':' || c==',')
	{
		qcc_token[len] = c;
		len++;
		qcc_token[len] = 0;
		return data+1;
	}

// parse a regular word
	do
	{
		if (len >= sizeof(qcc_token)-1)
			;
		else
			qcc_token[len++] = c;
		data++;
		c = *data;
		if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c==':' || c=='\"' || c==',')
			break;
	} while (c && !qcc_iswhite(c));

	qcc_token[len] = 0;
	return data;
}

//more C tokens...
char *QCC_COM_Parse2 (char *data)
{
	int		c;
	int		len;

	len = 0;
	qcc_token[0] = 0;

	if (!data)
		return NULL;

// skip whitespace
skipwhite:
	while ((c = *data) && qcc_iswhite(c))
		data++;
	if (!c)
	{
		qcc_eof = true;
		return NULL;
	}

// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}


// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		do
		{
			c = *data++;
			if (c=='\\' && *data == '\"')
				c = *data++;	//allow C-style string escapes
			else if (c=='\\' && *data == '\\')
				c = *data++;	// \ is now a special character so it needs to be marked up using itself
			else if (c=='\\' && *data == 'n')
			{					// and do new lines while we're at it.
				c = '\n';
				data++;
			}
			else if (c=='\"'||c=='\0')
			if (len >= sizeof(qcc_token)-1)
				;
			else
				qcc_token[len++] = c;
		} while (1);
	}

// parse numbers
	if (c >= '0' && c <= '9')
	{
		if (c == '0' && data[1] == 'x')
		{	//parse hex
			qcc_token[0] = '0';
			c='x';
			len=1;
			data++;
			for(;;)
			{	//parse regular number
				if (len >= sizeof(qcc_token)-1)
					;
				else
					qcc_token[len++] = c;
				data++;
				c = *data;
				if ((c<'0'|| c>'9') && (c<'a'||c>'f') && (c<'A'||c>'F') && c != '.')
					break;
			}

		}
		else
		{
			for(;;)
			{	//parse regular number
				if (len >= sizeof(qcc_token)-1)
					;
				else
					qcc_token[len++] = c;
				data++;
				c = *data;
				if ((c<'0'|| c>'9') && c != '.')
					break;
			}
		}

		qcc_token[len] = 0;
		return data;
	}
// parse words
	else if ((c>= 'a' && c <= 'z') || (c>= 'A' && c <= 'Z') || c == '_')
	{
		do
		{
			if (len >= sizeof(qcc_token)-1)
				;
			else
				qcc_token[len++] = c;
			data++;
			c = *data;
		} while ((c>= 'a' && c <= 'z') || (c>= 'A' && c <= 'Z') || c == '_');

		qcc_token[len] = 0;
		return data;
	}
	else
	{
		qcc_token[len] = c;
		len++;
		qcc_token[len] = 0;
		return data+1;
	}
}

char *VARGS qcva (char *text, ...)
{
	va_list argptr;
	static char msg[2048];

	va_start (argptr,text);
	QC_vsnprintf (msg,sizeof(msg)-1, text,argptr);
	va_end (argptr);

	return msg;
}


#ifndef MINIMAL

char *QC_strupr (char *start)
{
	char	*in;
	in = start;
	while (*in)
	{
		*in = toupper(*in);
		in++;
	}
	return start;
}

char *QC_strlower (char *start)
{
	char	*in;
	in = start;
	while (*in)
	{
		*in = tolower(*in);
		in++;
	}
	return start;
}


/*
=============================================================================

						MISC FUNCTIONS

=============================================================================
*/

/*
=================
Error

For abnormal program terminations
=================
*/
void VARGS QCC_Error (int errortype, const char *error, ...)
{
	extern int numsourcefiles;
	va_list argptr;
	char msg[2048];

	va_start (argptr,error);
	QC_vsnprintf (msg,sizeof(msg)-1, error,argptr);
	va_end (argptr);

	printf ("\n************ ERROR ************\n%s\n", msg);


	editbadfile(strings+s_file, pr_source_line);

	numsourcefiles = 0;

#ifndef QCC
	longjmp(qcccompileerror, 1);
#else
	print ("Press any key\n");
	getch();
#endif
	exit (1);
}


/*
=================
CheckParm

Checks for the given parameter in the program's command line arguments
Returns the argument number (1 to argc-1) or 0 if not present
=================
*/
int QCC_CheckParm (char *check)
{
	int             i;

	for (i = 1;i<myargc;i++)
	{
		if ( !QC_strcasecmp(check, myargv[i]) )
			return i;
	}

	return 0;
}

/*


#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifdef QCC
int SafeOpenWrite (char *filename)
{
	int     handle;

	umask (0);

	handle = open(filename,O_WRONLY | O_CREAT | O_TRUNC | O_BINARY
	, 0666);

	if (handle == -1)
		QCC_Error ("Error opening %s: %s",filename,strerror(errno));

	return handle;
}
#endif

int SafeOpenRead (char *filename)
{
	int     handle;

	handle = open(filename,O_RDONLY | O_BINARY);

	if (handle == -1)
		QCC_Error ("Error opening %s: %s",filename,strerror(errno));

	return handle;
}


void SafeRead (int handle, void *buffer, long count)
{
	if (read (handle,buffer,count) != count)
		QCC_Error ("File read failure");
}

#ifdef QCC
void SafeWrite (int handle, void *buffer, long count)
{
	if (write (handle,buffer,count) != count)
		QCC_Error ("File write failure");
}
#endif


void *SafeMalloc (long size)
{
	void *ptr;

	ptr = (void *)Hunk_Alloc (size);

	if (!ptr)
		QCC_Error ("Malloc failure for %lu bytes",size);

	return ptr;
}

*/



void DefaultExtension (char *path, char *extension)
{
	char    *src;
//
// if path doesn't have a .EXT, append extension
// (extension should include the .)
//
	src = path + strlen(path) - 1;

	while (*src != PATHSEPERATOR && src != path)
	{
		if (*src == '.')
			return;                 // it has an extension
		src--;
	}

	strcat (path, extension);
}


void DefaultPath (char *path, char *basepath)
{
	char    temp[128];

	if (path[0] == PATHSEPERATOR)
		return;                   // absolute path location
	strcpy (temp,path);
	strcpy (path,basepath);
	strcat (path,temp);
}


void    StripFilename (char *path)
{
	int             length;

	length = strlen(path)-1;
	while (length > 0 && path[length] != PATHSEPERATOR)
		length--;
	path[length] = 0;
}

/*
====================
Extract file parts
====================
*/
void ExtractFilePath (char *path, char *dest)
{
	char    *src;

	src = path + strlen(path) - 1;

//
// back up until a \ or the start
//
	while (src != path && *(src-1) != PATHSEPERATOR)
		src--;

	memcpy (dest, path, src-path);
	dest[src-path] = 0;
}

void ExtractFileBase (char *path, char *dest)
{
	char    *src;

	src = path + strlen(path) - 1;

//
// back up until a \ or the start
//
	while (src != path && *(src-1) != PATHSEPERATOR)
		src--;

	while (*src && *src != '.')
	{
		*dest++ = *src++;
	}
	*dest = 0;
}

void ExtractFileExtension (char *path, char *dest)
{
	char    *src;

	src = path + strlen(path) - 1;

//
// back up until a . or the start
//
	while (src != path && *(src-1) != '.')
		src--;
	if (src == path)
	{
		*dest = 0;	// no extension
		return;
	}

	strcpy (dest,src);
}


/*
==============
ParseNum / ParseHex
==============
*/
long ParseHex (char *hex)
{
	char    *str;
	long    num;

	num = 0;
	str = hex;

	while (*str)
	{
		num <<= 4;
		if (*str >= '0' && *str <= '9')
			num += *str-'0';
		else if (*str >= 'a' && *str <= 'f')
			num += 10 + *str-'a';
		else if (*str >= 'A' && *str <= 'F')
			num += 10 + *str-'A';
		else
			QCC_Error (ERR_BADHEX, "Bad hex number: %s",hex);
		str++;
	}

	return num;
}


long ParseNum (char *str)
{
	if (str[0] == '$')
		return ParseHex (str+1);
	if (str[0] == '0' && str[1] == 'x')
		return ParseHex (str+2);
	return atol (str);
}








//buffer size and max size are different. buffer is bigger.

#define MAXQCCFILES 3
struct {
	char name[64];
	char *buff;
//	int buffismalloc;
	int buffsize;
	int ofs;
	int maxofs;
} qccfile[MAXQCCFILES];
int SafeOpenWrite (char *filename, int maxsize)
{
	int i;
	for (i = 0; i < MAXQCCFILES; i++)
	{
		if (!qccfile[i].buff)
		{
			strcpy(qccfile[i].name, filename);
			qccfile[i].buffsize = maxsize;
			qccfile[i].maxofs = 0;
			qccfile[i].ofs = 0;
//			if (maxsize > 8192)
//				qccfile[i].buffismalloc = 1;
//			else
//				qccfile[i].buffismalloc = 0;
//			if (qccfile[i].buffismalloc)
				qccfile[i].buff = malloc(qccfile[i].buffsize);
//			else
//				qccfile[i].buff = memalloc(qccfile[i].buffsize);
			return i;
		}
	}
	QCC_Error(ERR_TOOMANYOPENFILES, "Too many open files on file %s", filename);
	return -1;
}

void ResizeBuf(int hand, int newsize)
{
//	int wasmal = qccfile[hand].buffismalloc;
	char *nb;

	if (qccfile[hand].buffsize >= newsize)
		return;	//already big enough

//	if (newsize > 8192)
//	{
//		qccfile[hand].buffismalloc = true;
		nb = malloc(newsize);
//	}
//	else
//	{
//		qccfile[hand].buffismalloc = false;
//		nb = memalloc(newsize);
//	}

	memcpy(nb, qccfile[hand].buff, qccfile[hand].maxofs);
//	if (wasmal)
		free(qccfile[hand].buff);
//	else
//		externs->memfree(qccfile[hand].buff);
	qccfile[hand].buff = nb;
	qccfile[hand].buffsize = newsize;
}
void SafeWrite(int hand, void *buf, long count)
{
	if (qccfile[hand].ofs +count >= qccfile[hand].buffsize)
		ResizeBuf(hand, qccfile[hand].ofs + count+(64*1024));

	memcpy(&qccfile[hand].buff[qccfile[hand].ofs], buf, count);
	qccfile[hand].ofs+=count;
	if (qccfile[hand].ofs > qccfile[hand].maxofs)
		qccfile[hand].maxofs = qccfile[hand].ofs;
}
int SafeSeek(int hand, int ofs, int mode)
{
	if (mode == SEEK_CUR)
		return qccfile[hand].ofs;
	else
	{
		ResizeBuf(hand, ofs+1024);
		qccfile[hand].ofs = ofs;
		if (qccfile[hand].ofs > qccfile[hand].maxofs)
			qccfile[hand].maxofs = qccfile[hand].ofs;
		return 0;
	}
}
void SafeClose(int hand)
{
	externs->WriteFile(qccfile[hand].name, qccfile[hand].buff, qccfile[hand].maxofs);
//	if (qccfile[hand].buffismalloc)
		free(qccfile[hand].buff);
//	else
//		externs->memfree(qccfile[hand].buff);
	qccfile[hand].buff = NULL;
}

qcc_cachedsourcefile_t *qcc_sourcefile;

enum
{
	UTF16LE,
	UTF16BE,
	UTF32LE,
	UTF32BE,
};
//read utf-16 chars and output the 'native' utf-8.
//we don't expect essays written in code, so we don't need much actual support for utf-8.
static char *decodeUTF(int type, unsigned char *inputf, unsigned int inbytes, int *outlen)
{
	char *utf8, *start;
	unsigned int inc;
	unsigned int chars, i;
	int w, maxperchar;
	switch(type)
	{
	case UTF16LE:
		w = 2;
		maxperchar = 3;
		break;
	case UTF16BE:
		w = 2;
		maxperchar = 3;
		break;
	case UTF32LE:
		w = 4;
		maxperchar = 4;	//we adhere to RFC3629 and clamp to U+10FFFF, which is only 4 bytes.
		break;
	case UTF32BE:
		w = 4;
		maxperchar = 4;
		break;
	}
	chars = inbytes / w;
	utf8 = start = qccHunkAlloc(chars * maxperchar + 2);
	for (i = 0; i < chars; i++)
	{
		switch(type)
		{
		case UTF16LE:
			inc = *inputf++;
			inc|= (*inputf++)<<8;
			break;
		case UTF16BE:
			inc = (*inputf++)<<8;
			inc|= *inputf++;
			break;
		case UTF32LE:
			inc = *inputf++;
			inc|= (*inputf++)<<8;
			inc|= (*inputf++)<<16;
			inc|= (*inputf++)<<24;
			break;
		case UTF32BE:
			inc = (*inputf++)<<24;
			inc|= (*inputf++)<<16;
			inc|= (*inputf++)<<8;
			inc|= *inputf++;
			break;
		}
		if (inc > 0x10FFFF)
			inc = 0xFFFD;

		if (inc <= 127)
			*utf8++ = inc;
		else if (inc <= 0x7ff)
		{
			*utf8++ = ((inc>>6) & 0x1f) | 0xc0;
			*utf8++ = ((inc>>0) & 0x3f) | 0x80;
		}
		else if (inc <= 0xffff)
		{
			*utf8++ = ((inc>>12) & 0xf) | 0xe0;
			*utf8++ = ((inc>>6) & 0x3f) | 0x80;
			*utf8++ = ((inc>>0) & 0x3f) | 0x80;
		}
		else if (inc <= 0x1fffff)
		{
			*utf8++ = ((inc>>18) & 0x07) | 0xf0;
			*utf8++ = ((inc>>12) & 0x3f) | 0x80;
			*utf8++ = ((inc>> 6) & 0x3f) | 0x80;
			*utf8++ = ((inc>> 0) & 0x3f) | 0x80;
		}
		else
		{
			inc = 0xFFFD;
			*utf8++ = ((inc>>12) & 0xf) | 0xe0;
			*utf8++ = ((inc>>6) & 0x3f) | 0x80;
			*utf8++ = ((inc>>0) & 0x3f) | 0x80;
		}
	}
	*outlen = utf8 - start;
	return start;
}

long	QCC_LoadFile (char *filename, void **bufferptr)
{
	char *mem;
	int len;
	len = externs->FileSize(filename);
	if (len < 0)
	{
		QCC_Error(ERR_COULDNTOPENFILE, "Couldn't open file %s", filename);
//		if (!externs->Abort)
			return -1;
//		externs->Abort("failed to find file %s", filename);
	}
	mem = qccHunkAlloc(sizeof(qcc_cachedsourcefile_t) + len+2);

	((qcc_cachedsourcefile_t*)mem)->next = qcc_sourcefile;
	qcc_sourcefile = (qcc_cachedsourcefile_t*)mem;
	mem += sizeof(qcc_cachedsourcefile_t);

	externs->ReadFile(filename, mem, len+2);

	if (len >= 4 && mem[0] == '\xff' && mem[1] == '\xfe' && mem[2] == '\x00' && mem[3] == '\x00')
		mem = decodeUTF(UTF32LE, (unsigned char*)mem+4, len-4, &len);
	else if (len >= 4 && mem[0] == '\x00' && mem[1] == '\x00' && mem[2] == '\xfe' && mem[3] == '\xff')
		mem = decodeUTF(UTF32BE, (unsigned char*)mem+4, len-4, &len);
	else if (len >= 2 && mem[0] == '\xff' && mem[1] == '\xfe')
		mem = decodeUTF(UTF16LE, (unsigned char*)mem+2, len-2, &len);
	else if (len >= 2 && mem[0] == '\xfe' && mem[1] == '\xff')
		mem = decodeUTF(UTF16BE, (unsigned char*)mem+2, len-2, &len);
	//utf-8 BOM, for compat with broken text editors (like windows notepad).
	else if (len >= 3 && mem[0] == '\xef' && mem[1] == '\xbb' && mem[2] == '\xbf')
	{
		mem += 3;
		len -= 3;
	}
	//actual utf-8 handling is somewhat up to the engine. the qcc can only ensure that utf8 works in symbol names etc.
	//its only in strings where it actually makes a difference, and the interpretation of those is basically entirely up to the engine.
	//that said, we could insert a utf-8 BOM into ones with utf-8 chars, but that would mess up a lot of builtins+mods, so we won't.

	mem[len] = '\n';
	mem[len+1] = '\0';

	strcpy(qcc_sourcefile->filename, filename);
	qcc_sourcefile->size = len;
	qcc_sourcefile->file = mem;
	qcc_sourcefile->type = FT_CODE;

	*bufferptr=mem;

	return len;
}
void	QCC_AddFile (char *filename)
{
	char *mem;
	int len;
	len = externs->FileSize(filename);
	if (len < 0)
		externs->Abort("failed to find file %s", filename);
	mem = qccHunkAlloc(sizeof(qcc_cachedsourcefile_t) + len+1);

	((qcc_cachedsourcefile_t*)mem)->next = qcc_sourcefile;
	qcc_sourcefile = (qcc_cachedsourcefile_t*)mem;
	qcc_sourcefile->size = len;
	mem += sizeof(qcc_cachedsourcefile_t);
	strcpy(qcc_sourcefile->filename, filename);
	qcc_sourcefile->file = mem;
	qcc_sourcefile->type = FT_DATA;

	externs->ReadFile(filename, mem, len+1);
	mem[len] = '\0';
}
void *FS_ReadToMem(char *filename, void *mem, int *len)
{
	if (!mem)
	{
		*len = externs->FileSize(filename);
		mem = externs->memalloc(*len);
	}
	return externs->ReadFile(filename, mem, *len);
}

void FS_CloseFromMem(void *mem)
{
	externs->memfree(mem);
}


#endif

void    StripExtension (char *path)
{
	int             length;

	length = strlen(path)-1;
	while (length > 0 && path[length] != '.')
	{
		length--;
		if (path[length] == '/')
			return;		// no extension
	}
	if (length)
		path[length] = 0;
}
