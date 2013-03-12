#include "qcc.h"

#include <stdarg.h>
#include <stdio.h>

/*
==============
LoadFile
==============
*/
unsigned char *PDECL QCC_ReadFile (const char *fname, void *buffer, int len)
{
	long    length;
	FILE *f;
	f = fopen(fname, "rb");
	if (!f)
		return NULL;
	length = fread(buffer, 1, len, f);
	fclose(f);

	if (length != len)
		return NULL;

	return buffer;
}
int PDECL QCC_FileSize (const char *fname)
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

pbool PDECL QCC_WriteFile (const char *name, void *data, int len)
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

void PDECL Sys_Error(const char *text, ...)
{
	va_list argptr;
	static char msg[2048];	

	va_start (argptr,text);
	QC_vsnprintf (msg,sizeof(msg)-1, text,argptr);
	va_end (argptr);

	QCC_Error(ERR_INTERNAL, "%s", msg);
}


FILE *logfile;
int logprintf(const char *format, ...)
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
	if (logfile)
		fputs(string, logfile);

	return 0;
}

int main (int argc, char **argv)
{
	int sucess;
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
	logfile = fopen("fteqcc.log", "wt");
	sucess = CompileParams(&funcs, true, argc, argv);
	qccClearHunk();
	if (logfile)
		fclose(logfile);

#ifdef _WIN32
//	fgetc(stdin);	//wait for keypress
#endif
	return !sucess;
}
