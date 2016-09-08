//compile routines

#include "qcc.h"
#undef progfuncs

char errorfile[128];
int errorline;

progfuncs_t *qccprogfuncs;

#include <setjmp.h>

extern int qcc_compileactive;
jmp_buf qcccompileerror;
char qcc_gamedir[128];
void QCC_PR_ResetErrorScope(void);



#if defined(MINIMAL) || defined(OMIT_QCC)

#else

int qccalloced;
int qcchunksize;
char *qcchunk;
void *qccHunkAlloc(size_t mem)
{
	mem = (mem + 7)&~7;

	qccalloced+=mem;
	if (qccalloced > qcchunksize)
		QCC_Error(ERR_INTERNAL, "Compile hunk was filled");

	memset(qcchunk+qccalloced-mem, 0, mem);
	return qcchunk+qccalloced-mem;
}
void qccClearHunk(void)
{
	if (qcchunk)
	{
		free(qcchunk);
		qcchunk=NULL;
	}
}
int qccpersisthunk;
void PostCompile(void)
{
	if (!qccpersisthunk)
		qccClearHunk();
	QCC_PR_CloseProcessor();
	QCC_Cleanup();

	if (asmfile)
	{
		fclose(asmfile);
		asmfile = NULL;
	}
}
pbool PreCompile(void)
{
	QCC_PR_ResetErrorScope();

	qccClearHunk();
	strcpy(qcc_gamedir, "");
	if (sizeof(void*) > 4)
		qcchunk = malloc(qcchunksize=512*1024*1024);
	else
		qcchunk = malloc(qcchunksize=256*1024*1024);
	while(!qcchunk && qcchunksize > 8*1024*1024)
	{
		qcchunksize /= 2;
		qcchunk = malloc(qcchunksize);
	}
	qccalloced=0;

	return !!qcchunk;
}

pbool QCC_main (int argc, char **argv);
void QCC_ContinueCompile(void);
void QCC_FinishCompile(void);

int comp_nump;char **comp_parms;
//void Editor(char *fname, int line, int numparms, char **compileparms);
pbool CompileParams(progfuncs_t *progfuncs, void(*cb)(void), int nump, char **parms)
{
	comp_nump = nump;
	comp_parms = parms;
	*errorfile = '\0';
	qccprogfuncs = progfuncs;
	if (setjmp(qcccompileerror))
	{
		PostCompile();
		if (*errorfile)
		{
			printf("Error in %s on line %i\n", errorfile, errorline);
		}
		return false;
	}

	if (!QCC_main(nump, parms))
		return false;

	while(qcc_compileactive)
	{
		if (cb)
			cb();

		QCC_ContinueCompile();
	}

	PostCompile();

	return true;
}
int PDECL Comp_Begin(pubprogfuncs_t *progfuncs, int nump, char **parms)
{
	comp_nump = nump;
	comp_parms = parms;
	qccprogfuncs = (progfuncs_t*)progfuncs;
	*errorfile = '\0';
	if (setjmp(qcccompileerror))
	{
		PostCompile();
		return false;
	}

	if (!QCC_main(nump, parms))
		return false;

	return true;
}
int PDECL Comp_Continue(pubprogfuncs_t *progfuncs)
{	
	qccprogfuncs = (progfuncs_t *)progfuncs;
	if (setjmp(qcccompileerror))
	{
		PostCompile();
		return false;
	}

	if (qcc_compileactive)
		QCC_ContinueCompile();
	else
	{
		PostCompile();

		return false;
	}

	return true;
}
#endif
pbool CompileFile(progfuncs_t *progfuncs, const char *filename)
{	
#if defined(MINIMAL) || defined(OMIT_QCC)
	return false;
#else
	char srcfile[32];
	char newname[32];
	static char *p[5];
	int parms;
	char *s, *s2;
	
	p[0] = NULL;
	parms = 1;
	
	strcpy(newname, filename);
	s = newname;
	if (strchr(s+1, '/'))
	{
		while(1)
		{
			s2 = strchr(s+1, '/');
			if (!s2)
			{
				*s = '\0';
				break;
			}
			s = s2;
		}
		p[parms] = "-src";
		p[parms+1] = newname;
		parms+=2;	
		
		strcpy(srcfile, s+1);
		srcfile[strlen(srcfile)-4] = '\0';
		strcat(srcfile, ".src");

		if (externs->FileSize(qcva("%s/%s", newname, srcfile))>0)
		{
			p[parms] = "-srcfile";
			p[parms+1] = srcfile;
			parms+=2;
		}
	}
	else
	{
		p[parms] = "-srcfile";
		p[parms+1] = newname;
		newname[strlen(newname)-4] = '\0';
		strcat(newname, ".src");
		parms+=2;
	}
//	p[2][strlen(p[2])-4] = '\0';
//	strcat(p[2], "/");

	while (!CompileParams(progfuncs, NULL, parms, p))
	{
		return false;
	}
	return true;
#endif
}

int QC_strncasecmp(const char *s1, const char *s2, int n)
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
	}
	
	return -1;
}

void editbadfile(const char *fname, int line)
{
	if (!*errorfile)
	{
		strcpy(errorfile, fname);
		errorline = line;
	}
}

