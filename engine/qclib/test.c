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

//the only function that is required externally. :/
void SV_EndRedirect (void) {}




//builtins and builtin management.
void PF_print (progfuncs_t *prinst, struct globalvars_s *gvars)
{
	char *s;
	s = prinst->VarString(prinst, 0);

	printf("%s", s);
}

builtin_t builtins[] = {
	PF_print,
	PF_print
};




//Called when the qc library has some sort of serious error.
void Sys_Abort(char *s, ...)
{	//quake handles this with a longjmp.
	printf("%s", s);
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

void runtest(void)
{
	progfuncs_t *pf;
	func_t func;
	progsnum_t pn;

	progparms_t ext;
	memset(&ext, 0, sizeof(ext));

	ext.progsversion = PROGSTRUCT_VERSION;
	ext.ReadFile = Sys_ReadFile;
	ext.FileSize= Sys_FileSize;
	ext.Abort = Sys_Abort;
	ext.printf = printf;

	pf = InitProgs(&ext);
	pf->Configure(pf, 1024*1024, 1);	//memory quantity of 1mb. Maximum progs loadable into the instance of 1
//If you support multiple progs types, you should tell the VM the offsets here, via RegisterFieldVar
	pn = pf->LoadProgs(pf, "testprogs.dat", 0, builtins, sizeof(builtins)/sizeof(builtins[0]));	//load the progs, don't care about the crc, and use those builtins.
	if (pn < 0)
		printf("Failed to load progs\n");
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
	CloseProgs(pf);
}


//Run a compiler and nothing else.
//Note that this could be done with an autocompile of PR_COMPILEALWAYS.
void compile(void)
{
	progfuncs_t *pf;

	progparms_t ext;

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

	memset(&ext, 0, sizeof(ext));
	ext.progsversion = PROGSTRUCT_VERSION;
	ext.ReadFile = Sys_ReadFile;
	ext.FileSize= Sys_FileSize;
	ext.WriteFile= Sys_WriteFile;
	ext.Abort = Sys_Abort;
	ext.printf = printf;

	pf = InitProgs(&ext);
	if (pf->StartCompile(pf, 0, NULL))
	{
		while(pf->ContinueCompile(pf) == 1)
			;
	}
	CloseProgs(pf);
}

int main(void)
{
	compile();
	runtest();

	return 0;
}
