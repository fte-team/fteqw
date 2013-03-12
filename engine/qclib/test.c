//This is basically a sample program.
//It deomnstrates the code required to get qclib up and running.
//This code does not demonstrate entities, however.
//It does demonstrate the built in qc compiler, and does demonstrate a globals-only progs interface.
//It also demonstrates basic builtin(s).



#include "progtype.h"
#include "progslib.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>




//builtins and builtin management.
void PF_prints (pubprogfuncs_t *prinst, struct globalvars_s *gvars)
{
	char *s;
	s = prinst->VarString(prinst, 0);

	printf("%s", s);
}

void PF_printv (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	printf("%f %f %f\n", G_FLOAT(OFS_PARM0+0), G_FLOAT(OFS_PARM0+1), G_FLOAT(OFS_PARM0+2));
}

void PF_printf (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	printf("%f\n", G_FLOAT(OFS_PARM0));
}


void PF_bad (pubprogfuncs_t *prinst, struct globalvars_s *gvars)
{
	printf("bad builtin\n");
}

builtin_t builtins[] = {
	PF_bad,
	PF_prints,
	PF_printv,
	PF_printf
};




//Called when the qc library has some sort of serious error.
void Sys_Abort(char *s, ...)
{	//quake handles this with a longjmp.
	va_list ap;
	va_start(ap, s);
	vprintf(s, ap);
	va_end(ap);
	exit(1);
}
//Called when the library has something to say.
//Kinda required for the compiler...
//Not really that useful for the normal vm.
int Sys_Printf(char *s, ...)
{	//look up quake's va function to find out how to deal with variable arguments properly.
	return printf("%s", s);
}

#include <stdio.h>
//copy file into buffer. note that the buffer will have been sized to fit the file (obtained via FileSize)
unsigned char *Sys_ReadFile (char *fname, void *buffer, int buflen)
{
	int len;
	FILE *f;
	if (!strncmp(fname, "src/", 4))
		fname+=4;	//skip the src part
	f = fopen(fname, "rb");
	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	if (buflen < len)
		return NULL;
	fseek(f, 0, SEEK_SET);
	fread(buffer, 1, len, f);
	fclose(f);
	return buffer;
}
//Finds the size of a file.
int Sys_FileSize (char *fname)
{
	int len;
	FILE *f;
	if (!strncmp(fname, "src/", 4))
		fname+=4;	//skip the src part
	f = fopen(fname, "rb");
	if (!f)
		return -1;
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fclose(f);
	return len;
}
//Writes a file.
pbool Sys_WriteFile (char *fname, void *data, int len)
{
	FILE *f;
	f = fopen(fname, "wb");
	if (!f)
		return 0;
	fwrite(data, 1, len, f);
	fclose(f);
	return 1;
}

void runtest(char *progsname)
{
	pubprogfuncs_t *pf;
	func_t func;
	progsnum_t pn;

	progparms_t ext;
	memset(&ext, 0, sizeof(ext));

	ext.progsversion = PROGSTRUCT_VERSION;
	ext.ReadFile = Sys_ReadFile;
	ext.FileSize= Sys_FileSize;
	ext.Abort = Sys_Abort;
	ext.Printf = printf;

	ext.numglobalbuiltins = sizeof(builtins)/sizeof(builtins[0]);
	ext.globalbuiltins = builtins;

	pf = InitProgs(&ext);
	pf->Configure(pf, 1024*1024, 1);	//memory quantity of 1mb. Maximum progs loadable into the instance of 1
//If you support multiple progs types, you should tell the VM the offsets here, via RegisterFieldVar
	pn = pf->LoadProgs(pf, progsname, 0, NULL, 0);	//load the progs, don't care about the crc, and use those builtins.
	if (pn < 0)
		printf("test: Failed to load progs \"%s\"\n", progsname);
	else
	{
//allocate qc-acessable strings here for 64bit cpus. (allocate via AddString, tempstringbase is a holding area not used by the actual vm)
//you can call functions before InitEnts if you want. it's not really advised for anything except naming additional progs. This sample only allows one max.

		pf->InitEnts(pf, 10);		//Now we know how many fields required, we can say how many maximum ents we want to allow. 10 in this case. This can be huge without too many problems.

//now it's safe to ED_Alloc.

		func = pf->FindFunction(pf, "main", PR_ANY);	//find the function 'main' in the first progs that has it.
		if (!func)
			printf("Couldn't find function\n");
		else
			pf->ExecuteProgram(pf, func);			//call the function
	}
	pf->CloseProgs(pf);
}


//Run a compiler and nothing else.
//Note that this could be done with an autocompile of PR_COMPILEALWAYS.
void compile(int argc, char **argv)
{
	pubprogfuncs_t *pf;

	progparms_t ext;

	if (0)
	{
		char *testsrcfile =	//newstyle progs.src must start with a #.
						//it's newstyle to avoid using multiple source files.
				 	"#pragma PROGS_DAT \"testprogs.dat\"\r\n"
					"//INTERMEDIATE FILE - EDIT TEST.C INSTEAD\r\n"
					"\r\n"
					"void(...) print = #1;\r\n"
					"void() main =\r\n"
					"{\r\n"
					"	print(\"hello world\\n\");\r\n"
					"};\r\n";

		//so that the file exists. We could insert it via the callbacks instead
		Sys_WriteFile("progs.src", testsrcfile, strlen(testsrcfile));
	}

	memset(&ext, 0, sizeof(ext));
	ext.progsversion = PROGSTRUCT_VERSION;
	ext.ReadFile = Sys_ReadFile;
	ext.FileSize= Sys_FileSize;
	ext.WriteFile= Sys_WriteFile;
	ext.Abort = Sys_Abort;
	ext.Printf = printf;

	pf = InitProgs(&ext);
	if (pf->StartCompile)
	{
		if (pf->StartCompile(pf, argc, argv))
		{
			while(pf->ContinueCompile(pf) == 1)
				;
		}
	}
	else
		printf("no compiler in this qcvm build\n");
	pf->CloseProgs(pf);
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("Invalid arguments!\nPlease run as, for example:\n%s testprogs.dat -srcfile progs.src\nThe first argument is the name of the progs.dat to run, the remaining arguments are the qcc args to use", argv[0]);
		return 0;
	}

	compile(argc-1, argv+1);
	runtest(argv[1]);

	return 0;
}
