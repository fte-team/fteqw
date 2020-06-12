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

enum{false,true};


//builtins and builtin management.
void PF_puts (pubprogfuncs_t *prinst, struct globalvars_s *gvars)
{
	char *s;
	s = prinst->VarString(prinst, 0);

	printf("%s", s);
}

void PF_putv (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	printf("%f %f %f\n", G_FLOAT(OFS_PARM0+0), G_FLOAT(OFS_PARM0+1), G_FLOAT(OFS_PARM0+2));
}

void PF_putf (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	printf("%f\n", G_FLOAT(OFS_PARM0));
}

#ifdef _WIN32
	#define Q_snprintfz _snprintf
	#define Q_vsnprintf _vsnprintf
#else
	#define Q_snprintfz snprintf
	#define Q_vsnprintf vsnprintf
#endif
char	*va(char *format, ...)
{
	va_list		argptr;
	static char		string[1024];
	va_start (argptr, format);
	Q_vsnprintf (string, sizeof(string), format,argptr);
	va_end (argptr);
	return string;	
}

void QCBUILTIN PF_sprintf_internal (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals, const char *s, int firstarg, char *outbuf, int outbuflen)
{
	const char *s0;
	char *o = outbuf, *end = outbuf + outbuflen, *err;
	int width, precision, thisarg, flags;
	char formatbuf[16];
	char *f;
	int argpos = firstarg;
	int isfloat;
	static int dummyivec[3] = {0, 0, 0};
	static float dummyvec[3] = {0, 0, 0};

#define PRINTF_ALTERNATE 1
#define PRINTF_ZEROPAD 2
#define PRINTF_LEFT 4
#define PRINTF_SPACEPOSITIVE 8
#define PRINTF_SIGNPOSITIVE 16

	formatbuf[0] = '%';

#define GETARG_FLOAT(a) (((a)>=firstarg && (a)<prinst->callargc) ? (G_FLOAT(OFS_PARM0 + 3 * (a))) : 0)
#define GETARG_VECTOR(a) (((a)>=firstarg && (a)<prinst->callargc) ? (G_VECTOR(OFS_PARM0 + 3 * (a))) : dummyvec)
#define GETARG_INT(a) (((a)>=firstarg && (a)<prinst->callargc) ? (G_INT(OFS_PARM0 + 3 * (a))) : 0)
#define GETARG_INTVECTOR(a) (((a)>=firstarg && (a)<prinst->callargc) ? ((int*) G_VECTOR(OFS_PARM0 + 3 * (a))) : dummyivec)
#define GETARG_STRING(a) (((a)>=firstarg && (a)<prinst->callargc) ? (PR_GetStringOfs(prinst, OFS_PARM0 + 3 * (a))) : "")

	for(;;)
	{
		s0 = s;
		switch(*s)
		{
			case 0:
				goto finished;
			case '%':
				++s;

				if(*s == '%')
					goto verbatim;

				// complete directive format:
				// %3$*1$.*2$ld
				
				width = -1;
				precision = -1;
				thisarg = -1;
				flags = 0;
				isfloat = -1;

				// is number following?
				if(*s >= '0' && *s <= '9')
				{
					width = strtol(s, &err, 10);
					if(!err)
					{
						printf("PF_sprintf: bad format string: %s\n", s0);
						goto finished;
					}
					if(*err == '$')
					{
						thisarg = width + (firstarg-1);
						width = -1;
						s = err + 1;
					}
					else
					{
						if(*s == '0')
						{
							flags |= PRINTF_ZEROPAD;
							if(width == 0)
								width = -1; // it was just a flag
						}
						s = err;
					}
				}

				if(width < 0)
				{
					for(;;)
					{
						switch(*s)
						{
							case '#': flags |= PRINTF_ALTERNATE; break;
							case '0': flags |= PRINTF_ZEROPAD; break;
							case '-': flags |= PRINTF_LEFT; break;
							case ' ': flags |= PRINTF_SPACEPOSITIVE; break;
							case '+': flags |= PRINTF_SIGNPOSITIVE; break;
							default:
								goto noflags;
						}
						++s;
					}
noflags:
					if(*s == '*')
					{
						++s;
						if(*s >= '0' && *s <= '9')
						{
							width = strtol(s, &err, 10);
							if(!err || *err != '$')
							{
								printf("PF_sprintf: invalid format string: %s\n", s0);
								goto finished;
							}
							s = err + 1;
						}
						else
							width = argpos++;
						width = GETARG_FLOAT(width);
						if(width < 0)
						{
							flags |= PRINTF_LEFT;
							width = -width;
						}
					}
					else if(*s >= '0' && *s <= '9')
					{
						width = strtol(s, &err, 10);
						if(!err)
						{
							printf("PF_sprintf: invalid format string: %s\n", s0);
							goto finished;
						}
						s = err;
						if(width < 0)
						{
							flags |= PRINTF_LEFT;
							width = -width;
						}
					}
					// otherwise width stays -1
				}

				if(*s == '.')
				{
					++s;
					if(*s == '*')
					{
						++s;
						if(*s >= '0' && *s <= '9')
						{
							precision = strtol(s, &err, 10);
							if(!err || *err != '$')
							{
								printf("PF_sprintf: invalid format string: %s\n", s0);
								goto finished;
							}
							s = err + 1;
						}
						else
							precision = argpos++;
						precision = GETARG_FLOAT(precision);
					}
					else if(*s >= '0' && *s <= '9')
					{
						precision = strtol(s, &err, 10);
						if(!err)
						{
							printf("PF_sprintf: invalid format string: %s\n", s0);
							goto finished;
						}
						s = err;
					}
					else
					{
						printf("PF_sprintf: invalid format string: %s\n", s0);
						goto finished;
					}
				}

				for(;;)
				{
					switch(*s)
					{
						case 'h': isfloat = 1; break;
						case 'l': isfloat = 0; break;
						case 'L': isfloat = 0; break;
						case 'j': break;
						case 'z': break;
						case 't': break;
						default:
							goto nolength;
					}
					++s;
				}
nolength:

				// now s points to the final directive char and is no longer changed
				if (*s == 'p' || *s == 'P')
				{
					//%p is slightly different from %x.
					//always 8-bytes wide with 0 padding, always ints.
					flags |= PRINTF_ZEROPAD;
					if (width < 0) width = 8;
					if (isfloat < 0) isfloat = 0;
				}
				else if (*s == 'i')
				{
					//%i defaults to ints, not floats.
					if(isfloat < 0) isfloat = 0;
				}

				//assume floats, not ints.
				if(isfloat < 0)
					isfloat = 1;

				if(thisarg < 0)
					thisarg = argpos++;

				if(o < end - 1)
				{
					f = &formatbuf[1];
					if(*s != 's' && *s != 'c')
						if(flags & PRINTF_ALTERNATE) *f++ = '#';
					if(flags & PRINTF_ZEROPAD) *f++ = '0';
					if(flags & PRINTF_LEFT) *f++ = '-';
					if(flags & PRINTF_SPACEPOSITIVE) *f++ = ' ';
					if(flags & PRINTF_SIGNPOSITIVE) *f++ = '+';
					*f++ = '*';
					if(precision >= 0)
					{
						*f++ = '.';
						*f++ = '*';
					}
					if (*s == 'p')
						*f++ = 'x';
					else if (*s == 'P')
						*f++ = 'X';
					else
						*f++ = *s;
					*f++ = 0;

					if(width < 0) // not set
						width = 0;

					switch(*s)
					{
						case 'd': case 'i':
							if(precision < 0) // not set
								Q_snprintfz(o, end - o, formatbuf, width, (isfloat ? (int) GETARG_FLOAT(thisarg) : (int) GETARG_INT(thisarg)));
							else
								Q_snprintfz(o, end - o, formatbuf, width, precision, (isfloat ? (int) GETARG_FLOAT(thisarg) : (int) GETARG_INT(thisarg)));
							o += strlen(o);
							break;
						case 'o': case 'u': case 'x': case 'X': case 'p': case 'P':
							if(precision < 0) // not set
								Q_snprintfz(o, end - o, formatbuf, width, (isfloat ? (unsigned int) GETARG_FLOAT(thisarg) : (unsigned int) GETARG_INT(thisarg)));
							else
								Q_snprintfz(o, end - o, formatbuf, width, precision, (isfloat ? (unsigned int) GETARG_FLOAT(thisarg) : (unsigned int) GETARG_INT(thisarg)));
							o += strlen(o);
							break;
						case 'e': case 'E': case 'f': case 'F': case 'g': case 'G':
							if(precision < 0) // not set
								Q_snprintfz(o, end - o, formatbuf, width, (isfloat ? (double) GETARG_FLOAT(thisarg) : (double) GETARG_INT(thisarg)));
							else
								Q_snprintfz(o, end - o, formatbuf, width, precision, (isfloat ? (double) GETARG_FLOAT(thisarg) : (double) GETARG_INT(thisarg)));
							o += strlen(o);
							break;
						case 'v': case 'V':
							f[-2] += 'g' - 'v';
							if(precision < 0) // not set
								Q_snprintfz(o, end - o, va("%s %s %s", /* NESTED SPRINTF IS NESTED */ formatbuf, formatbuf, formatbuf),
									width, (isfloat ? (double) GETARG_VECTOR(thisarg)[0] : (double) GETARG_INTVECTOR(thisarg)[0]),
									width, (isfloat ? (double) GETARG_VECTOR(thisarg)[1] : (double) GETARG_INTVECTOR(thisarg)[1]),
									width, (isfloat ? (double) GETARG_VECTOR(thisarg)[2] : (double) GETARG_INTVECTOR(thisarg)[2])
								);
							else
								Q_snprintfz(o, end - o, va("%s %s %s", /* NESTED SPRINTF IS NESTED */ formatbuf, formatbuf, formatbuf),
									width, precision, (isfloat ? (double) GETARG_VECTOR(thisarg)[0] : (double) GETARG_INTVECTOR(thisarg)[0]),
									width, precision, (isfloat ? (double) GETARG_VECTOR(thisarg)[1] : (double) GETARG_INTVECTOR(thisarg)[1]),
									width, precision, (isfloat ? (double) GETARG_VECTOR(thisarg)[2] : (double) GETARG_INTVECTOR(thisarg)[2])
								);
							o += strlen(o);
							break;
						case 'c':
							//UTF-8-FIXME: figure it out yourself
//							if(flags & PRINTF_ALTERNATE)
							{
								if(precision < 0) // not set
									Q_snprintfz(o, end - o, formatbuf, width, (isfloat ? (unsigned int) GETARG_FLOAT(thisarg) : (unsigned int) GETARG_INT(thisarg)));
								else
									Q_snprintfz(o, end - o, formatbuf, width, precision, (isfloat ? (unsigned int) GETARG_FLOAT(thisarg) : (unsigned int) GETARG_INT(thisarg)));
								o += strlen(o);
							}
/*							else
							{
								unsigned int c = (isfloat ? (unsigned int) GETARG_FLOAT(thisarg) : (unsigned int) GETARG_INT(thisarg));
								char charbuf16[16];
								const char *buf = u8_encodech(c, NULL, charbuf16);
								if(!buf)
									buf = "";
								if(precision < 0) // not set
									precision = end - o - 1;
								o += u8_strpad(o, end - o, buf, (flags & PRINTF_LEFT) != 0, width, precision);
							}
*/							break;
						case 's':
							//UTF-8-FIXME: figure it out yourself
//							if(flags & PRINTF_ALTERNATE)
							{
								if(precision < 0) // not set
									Q_snprintfz(o, end - o, formatbuf, width, GETARG_STRING(thisarg));
								else
									Q_snprintfz(o, end - o, formatbuf, width, precision, GETARG_STRING(thisarg));
								o += strlen(o);
							}
/*							else
							{
								if(precision < 0) // not set
									precision = end - o - 1;
								o += u8_strpad(o, end - o, GETARG_STRING(thisarg), (flags & PRINTF_LEFT) != 0, width, precision);
							}
*/							break;
						default:
							printf("PF_sprintf: invalid format string: %s\n", s0);
							goto finished;
					}
				}
				++s;
				break;
			default:
verbatim:
				if(o < end - 1)
					*o++ = *s;
				s++;
				break;
		}
	}
finished:
	*o = 0;
}

void PF_printf (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char outbuf[4096];
	PF_sprintf_internal(prinst, pr_globals, PR_GetStringOfs(prinst, OFS_PARM0), 1, outbuf, sizeof(outbuf));
	printf("%s", outbuf);
}

void PF_spawn (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	struct edict_s	*ed;
	ed = ED_Alloc(prinst, false, 0);
	pr_globals = PR_globals(prinst, PR_CURRENT);
	RETURN_EDICT(prinst, ed);
}

void PF_bad (pubprogfuncs_t *prinst, struct globalvars_s *gvars)
{
	printf("bad builtin\n");
}

builtin_t builtins[] = {
	PF_bad,
	PF_puts,
	PF_putv,
	PF_putf,
	PF_printf,
	PF_spawn
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
void *PDECL Sys_ReadFile(const char *fname, unsigned char *(PDECL *buf_get)(void *ctx, size_t len), void *buf_ctx, size_t *out_size, pbool issourcefile)
{
	void *buffer;
	int len;
	FILE *f;
	if (!strncmp(fname, "src/", 4))
		fname+=4;	//skip the src part
	f = fopen(fname, "rb");
	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	len = ftell(f);

	buffer = buf_get(buf_ctx, len);
	fseek(f, 0, SEEK_SET);
	fread(buffer, 1, len, f);
	fclose(f);

	*out_size = len;
	return buffer;
}
//Finds the size of a file.
int Sys_FileSize (const char *fname)
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
pbool Sys_WriteFile (const char *fname, void *data, int len)
{
	FILE *f;
	f = fopen(fname, "wb");
	if (!f)
		return 0;
	fwrite(data, 1, len, f);
	fclose(f);
	return 1;
}

void runtest(const char *progsname)
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
	pf->Configure(pf, 1024*1024, 1, false);	//memory quantity of 1mb. Maximum progs loadable into the instance of 1
//If you support multiple progs types, you should tell the VM the offsets here, via RegisterFieldVar
	pn = pf->LoadProgs(pf, progsname);	//load the progs.
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
	pf->Shutdown(pf);
}

//Run a compiler and nothing else.
//Note that this could be done with an autocompile of PR_COMPILEALWAYS.
void compile(int argc, const char **argv)
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
		else
			printf("compilation failed to start\n");
	}
	else
		printf("no compiler in this qcvm build\n");
	pf->Shutdown(pf);
}

int main(int argc, const char **argv)
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
