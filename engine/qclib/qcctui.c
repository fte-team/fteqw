#include "qcc.h"

#include <stdarg.h>
#include <stdio.h>

#if defined(__linux__) || defined(__unix__)
#include <unistd.h>
#endif

/*
==============
LoadFile
==============
*/
static void *QCC_ReadFile(const char *fname, unsigned char *(*buf_get)(void *ctx, size_t len), void *buf_ctx, size_t *out_size, pbool issourcefile)
//unsigned char *PDECL QCC_ReadFile (const char *fname, void *buffer, int len, size_t *sz)
{
	size_t len;
	FILE *f;
	char *buffer;

	f = fopen(fname, "rb");
	if (!f)
	{
		if (out_size)
			*out_size = 0;
		return NULL;
	}

	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (buf_get)
		buffer = buf_get(buf_ctx, len+1);
	else
		buffer = malloc(len+1);
	((char*)buffer)[len] = 0;
	if (len != fread(buffer, 1, len, f))
	{
		if (!buf_get)
			free(buffer);
		buffer = NULL;
	}
	fclose(f);

	if (out_size)
		*out_size = len;
	return buffer;
}
static int PDECL QCC_FileSize (const char *fname)
{
	long    length;
	FILE *f;
	f = fopen(fname, "rb");
	if (!f)
		return -1;
	fseek(f, 0, SEEK_END);
	length = ftell(f);
	fclose(f);

	return length;
}

static pbool PDECL QCC_WriteFile (const char *name, void *data, int len)
{
	long    length;
	FILE *f;
	f = fopen(name, "wb");
	if (!f)
		return false;
	length = fwrite(data, 1, len, f);
	fclose(f);

	if (length != len)
		return false;

	return true;
}

#undef printf
#undef Sys_Error

static void PDECL Sys_Error(const char *text, ...)
{
	va_list argptr;
	static char msg[2048];	

	va_start (argptr,text);
	QC_vsnprintf (msg,sizeof(msg)-1, text,argptr);
	va_end (argptr);

	QCC_Error(ERR_INTERNAL, "%s", msg);
}


static FILE *logfile;
static int logprintf(const char *format, ...)
{
	va_list		argptr;
	static char		string[1024];

	va_start (argptr, format);
#ifdef _WIN32
	_vsnprintf (string,sizeof(string)-1, format,argptr);
#else
	vsnprintf (string,sizeof(string), format,argptr);
#endif
	va_end (argptr);

	printf("%s", string);
//	fputs(string, stderr);
	if (logfile)
		fputs(string, logfile);

	return 0;
}

int main (int argc, char **argv)
{
	unsigned int i;
	pbool sucess;
	progexterns_t ext;
	progfuncs_t funcs;
	progfuncs = &funcs;
	memset(&funcs, 0, sizeof(funcs));
	funcs.funcs.parms = &ext;
	memset(&ext, 0, sizeof(progexterns_t));
	funcs.funcs.parms->ReadFile = QCC_ReadFile;
	funcs.funcs.parms->FileSize = QCC_FileSize;
	funcs.funcs.parms->WriteFile = QCC_WriteFile;
	funcs.funcs.parms->Printf = logprintf;
	funcs.funcs.parms->Sys_Error = Sys_Error;

#if defined(__linux__) || defined(__unix__)
	if (isatty(STDOUT_FILENO))
	{	//only use colours if its a tty, and not if we're redirected.
		col_none = "\e[0;m";			//reset to white
		col_error = "\e[0;31m";			//red
		col_symbol = "\e[0;32m";		//green
		col_warning = "\e[0;33m";		//yellow
		//col_ = "\e[0;34m";			//blue
		col_name = "\e[0;35m";			//magenta
		//col_ = "\e[0;36m";			//cyan
		col_location = "\e[0;1;37m";	//bright white
	}
#endif


#ifdef _WIN32
	logfile = fopen("fteqcc.log", "wt");
#endif
	if (logfile)
	{
		fputs("Args:", logfile);
		for (i = 0; i < argc; i++)
		{
			if (strchr(argv[i], ' '))
				fprintf(logfile, " \"%s\"", argv[i]);
			else
				fprintf(logfile, " %s", argv[i]);
		}
		fprintf(logfile, "\n");
	}
	sucess = CompileParams(&funcs, NULL, argc, argv);
	qccClearHunk();
	if (logfile)
		fclose(logfile);

#ifdef _WIN32
//	fgetc(stdin);	//wait for keypress
#endif
	return sucess?EXIT_SUCCESS:EXIT_FAILURE;
}
