#define PROGSUSED
#include "progsint.h"
//#include "editor.h"

#if __STDC_VERSION__ >= 199901L
	#define fte_restrict restrict
#elif defined(_MSC_VER) && _MSC_VER >= 1400
	#define fte_restrict __restrict
#else
	#define fte_restrict
#endif

#if defined(_WIN32) || defined(__DJGPP__)
	#include <malloc.h>
#elif !defined(alloca)	//alloca.h isn't present on bsd (stdlib.h should define it to __builtin_alloca, and we can check for that here).
	#include <alloca.h>
#endif

#define HunkAlloc BADGDFG sdfhhsf FHS


#define Host_Error Sys_Error

// I put the following here to resolve "undefined reference to `__imp__vsnprintf'" with MinGW64 ~ Moodles
#if 0//def _WIN32
	#if (_MSC_VER >= 1400)
		//with MSVC 8, use MS extensions
		#define snprintf linuxlike_snprintf_vc8
		int VARGS linuxlike_snprintf_vc8(char *buffer, int size, const char *format, ...) LIKEPRINTF(3);
		#define vsnprintf(a, b, c, d) vsnprintf_s(a, b, _TRUNCATE, c, d)
	#else
		//msvc crap
		#define snprintf linuxlike_snprintf
		int VARGS linuxlike_snprintf(char *buffer, int size, const char *format, ...) LIKEPRINTF(3);
		#define vsnprintf linuxlike_vsnprintf
		int VARGS linuxlike_vsnprintf(char *buffer, int size, const char *format, va_list argptr);
	#endif
#endif


//cpu clock stuff (glorified rdtsc), for profile timing only
#if !defined(Sys_GetClock) && defined(_WIN32)
	//windows has some specific functions for this (traditionally wrapping rdtsc)
	//note: on some systems, you may need to force cpu affinity to a single core via task manager
	static prclocks_t Sys_GetClock(void)
	{
		LARGE_INTEGER li;
		QueryPerformanceCounter(&li);
		return li.QuadPart;
	}
	prclocks_t Sys_GetClockRate(void)
	{
		LARGE_INTEGER li;
		QueryPerformanceFrequency(&li);
		return li.QuadPart;
	}
	#define Sys_GetClock Sys_GetClock
#endif

#if 0//!defined(Sys_GetClock) && defined(__unix__)
	//linux/unix has some annoying abstraction and shows time in nanoseconds rather than cycles. lets hope we don't waste too much time  reading it.
	#include <unistd.h>
	#if defined(_POSIX_TIMERS) && _POSIX_TIMERS >= 0
		#include <time.h>
		#ifdef CLOCK_PROCESS_CPUTIME_ID
			static prclocks_t Sys_GetClock(void)
			{
				struct timespec c;
				clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &c);
				return (c.tv_sec*1000000000ull) + c.tv_nsec;
			}
			#define Sys_GetClock Sys_GetClock
			prclocks_t Sys_GetClockRate(void)
			{
				return 1000000000ull;
			}
		#endif
	#endif
#endif

#if !defined(Sys_GetClock) && defined(__unix__)
	#include <time.h>
	#define Sys_GetClock() clock()
	prclocks_t Sys_GetClockRate(void) { return CLOCKS_PER_SEC; }
#endif

#ifndef Sys_GetClock
	//other systems have no choice but to omit this feature in some way. this is just for profiling, so we can get away with stubs.
	#define Sys_GetClock() 0
	prclocks_t Sys_GetClockRate(void) { return 1; }
#endif

//=============================================================================

/*
=================
PR_PrintStatement
=================
*/
static void PR_PrintStatement (progfuncs_t *progfuncs, int statementnum)
{
	unsigned int op;
	unsigned int arg[3];

	switch(current_progstate->structtype)
	{
	default:
	case PST_DEFAULT:
	case PST_QTEST:
		op = ((dstatement16_t*)current_progstate->statements + statementnum)->op;
		arg[0] = ((dstatement16_t*)current_progstate->statements + statementnum)->a;
		arg[1] = ((dstatement16_t*)current_progstate->statements + statementnum)->b;
		arg[2] = ((dstatement16_t*)current_progstate->statements + statementnum)->c;
		break;
	case PST_KKQWSV:
	case PST_FTE32:
		op = ((dstatement32_t*)current_progstate->statements + statementnum)->op;
		arg[0] = ((dstatement32_t*)current_progstate->statements + statementnum)->a;
		arg[1] = ((dstatement32_t*)current_progstate->statements + statementnum)->b;
		arg[2] = ((dstatement32_t*)current_progstate->statements + statementnum)->c;
		break;
	}

#if !defined(MINIMAL) && !defined(OMIT_QCC)
	if ( (unsigned)op < OP_NUMOPS)
	{
		int i;
		externs->Printf ("%s ",  pr_opcodes[op].name);
		i = strlen(pr_opcodes[op].name);
		for ( ; i<10 ; i++)
			externs->Printf (" ");
	}
	else
#endif
		externs->Printf ("op%3i ", op);

	if (op == OP_IF_F || op == OP_IFNOT_F)
		externs->Printf ("%sbranch %i",PR_GlobalString(progfuncs, arg[0]),arg[1]);
	else if (op == OP_GOTO)
	{
		externs->Printf ("branch %i",arg[0]);
	}
	else if ( (unsigned)(op - OP_STORE_F) < 6)
	{
		externs->Printf ("%s",PR_GlobalString(progfuncs, arg[0]));
		externs->Printf ("%s", PR_GlobalStringNoContents(progfuncs, arg[1]));
	}
	else
	{
		if (arg[0])
			externs->Printf ("%s",PR_GlobalString(progfuncs, arg[0]));
		if (arg[1])
			externs->Printf ("%s",PR_GlobalString(progfuncs, arg[1]));
		if (arg[2])
			externs->Printf ("%s", PR_GlobalStringNoContents(progfuncs, arg[2]));
	}
	externs->Printf ("\n");
}

#ifdef _WIN32
static void VARGS QC_snprintfz (char *dest, size_t size, const char *fmt, ...)
{
	va_list args;
	va_start (args, fmt);
	_vsnprintf (dest, size-1, fmt, args);
	va_end (args);
	//make sure its terminated.
	dest[size-1] = 0;
}
#else
#define QC_snprintfz snprintf
#endif

void PDECL PR_GenerateStatementString (pubprogfuncs_t *ppf, int statementnum, char *out, int outlen)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	unsigned int op;
	unsigned int arg[3];

	*out = 0;
	outlen--;

	if ((unsigned)statementnum >= current_progstate->progs->numstatements)
		return;
	switch(current_progstate->structtype)
	{
	case PST_DEFAULT:
	case PST_QTEST:
		op = ((dstatement16_t*)current_progstate->statements + statementnum)->op;
		arg[0] = ((dstatement16_t*)current_progstate->statements + statementnum)->a;
		arg[1] = ((dstatement16_t*)current_progstate->statements + statementnum)->b;
		arg[2] = ((dstatement16_t*)current_progstate->statements + statementnum)->c;
		break;
	case PST_KKQWSV:
	case PST_FTE32:
		op = ((dstatement32_t*)current_progstate->statements + statementnum)->op;
		arg[0] = ((dstatement32_t*)current_progstate->statements + statementnum)->a;
		arg[1] = ((dstatement32_t*)current_progstate->statements + statementnum)->b;
		arg[2] = ((dstatement32_t*)current_progstate->statements + statementnum)->c;
		break;
	default:
		return;
	}
	op = op & ~0x8000;	//break points.

	if (current_progstate->linenums)
	{
		QC_snprintfz (out, outlen, "%3i: ", current_progstate->linenums[statementnum]);
		outlen -= strlen(out);
		out += strlen(out);
	}
	else
	{
		QC_snprintfz (out, outlen, "%3i: ", statementnum);
		outlen -= strlen(out);
		out += strlen(out);
	}

#if !defined(MINIMAL) && !defined(OMIT_QCC)
	if ( (unsigned)op < OP_NUMOPS)
	{
		QC_snprintfz (out, outlen, "%-12s ", pr_opcodes[op].opname);
		outlen -= strlen(out);
		out += strlen(out);
	}
	else
#endif
	{
		QC_snprintfz (out, outlen, "op%3i ", op);
		outlen -= strlen(out);
		out += strlen(out);
	}

	if (op == OP_IF_F || op == OP_IFNOT_F || op == OP_IF_I || op == OP_IFNOT_I || op == OP_IF_S || op == OP_IFNOT_S)
	{
		QC_snprintfz (out, outlen, "%sbranch %i(%i)",PR_GlobalStringNoContents(progfuncs, arg[0]),(short)arg[1], statementnum+(short)arg[0]);
		outlen -= strlen(out);
		out += strlen(out);
	}
	else if (op == OP_GOTO)
	{
		QC_snprintfz (out, outlen, "branch %i(%i)",(short)arg[0], statementnum+(short)arg[0]);
		outlen -= strlen(out);
		out += strlen(out);
	}
	else if ( (unsigned)(op - OP_STORE_F) < 6)
	{
		QC_snprintfz (out, outlen, "%s",PR_GlobalStringNoContents(progfuncs, arg[0]));
		outlen -= strlen(out);
		out += strlen(out);
		QC_snprintfz (out, outlen, "%s", PR_GlobalStringNoContents(progfuncs, arg[1]));
		outlen -= strlen(out);
		out += strlen(out);
	}
	else
	{
		if (arg[0])
		{
			QC_snprintfz (out, outlen, "%s",PR_GlobalStringNoContents(progfuncs, arg[0]));
			outlen -= strlen(out);
			out += strlen(out);
		}
		if (arg[1])
		{
			QC_snprintfz (out, outlen, "%s",PR_GlobalStringNoContents(progfuncs, arg[1]));
			outlen -= strlen(out);
			out += strlen(out);
		}
		if (arg[2])
		{
			QC_snprintfz (out, outlen, "%s", PR_GlobalStringNoContents(progfuncs, arg[2]));
			outlen -= strlen(out);
			out += strlen(out);
		}
	}
	QC_snprintfz (out, outlen, "\n");
	outlen -= 1;
	out += 1;
}


/*
============
PR_StackTrace
============
*/
static void PDECL PR_PrintRelevantLocals(progfuncs_t *progfuncs)
{
	//scan for op_address/op_load instructions within the function
	int st, st2;
	int op;
	dstatement16_t *st16 = current_progstate->statements;
	int line;
	if (!current_progstate->linenums || current_progstate->structtype != PST_DEFAULT)
		return;

	line = current_progstate->linenums[pr_xstatement];
	for (st = pr_xfunction->first_statement; st16[st].op != OP_DONE; st++)
	{
		if (current_progstate->linenums[st] < line - 2 || current_progstate->linenums[st] > line + 2)
			continue;	//don't go crazy with this.
		op = st16[st].op & ~0x8000;
		if (op == OP_ADDRESS || (op >= OP_LOAD_F && op <= OP_LOAD_FNC) || op == OP_LOAD_I || op == OP_LOAD_P)
		{
			ddef16_t *ent = ED_GlobalAtOfs16(progfuncs, st16[st].a);
			ddef16_t *fld = ED_GlobalAtOfs16(progfuncs, st16[st].b);
			pbool skip = false;
			edictrun_t *ed;
			unsigned int entnum;
			eval_t *ptr;
			fdef_t *fdef;
			fdef_t *cnfd;
			const char *classname;
			if (!ent || !fld)
				continue;
			//all this extra code to avoid printing dupes...
			for (st2 = st-1; st2 >= pr_xfunction->first_statement; st2--)
			{
				if (current_progstate->linenums[st2] < line - 2 || current_progstate->linenums[st2] > line + 2)
					continue;
				op = st16[st2].op & ~0x8000;
				if (op == OP_ADDRESS || (op >= OP_LOAD_F && op <= OP_LOAD_FNC) || op == OP_LOAD_I || op == OP_LOAD_P)
					if (st16[st].a == st16[st2].a && st16[st].b == st16[st2].b)
					{
						skip = true;
						break;
					}
			}
			if (skip)
				continue;
			entnum = ((eval_t *)&pr_globals[st16[st].a])->edict;
			if (entnum >= sv_num_edicts)
			{
				classname = "INVALID";
				continue;
			}
			else
			{
				ed = PROG_TO_EDICT_PB(progfuncs, entnum);
				if ((unsigned int)((eval_t *)&pr_globals[st16[st].b])->_int*4u >= ed->fieldsize)
					continue;
				else
					ptr = (eval_t *)(((int *)edvars(ed)) + ((eval_t *)&pr_globals[st16[st].b])->_int + progfuncs->funcs.fieldadjust);

				cnfd = ED_FindField(progfuncs, "classname");
				if (cnfd)
				{
					string_t *v = (string_t *)((char *)edvars(ed) + cnfd->ofs*4);
					classname = PR_StringToNative(&progfuncs->funcs, *v);
				}
				else
					classname = "";
			}
			if (*classname)
				fdef = ED_ClassFieldAtOfs(progfuncs, ((eval_t *)&pr_globals[st16[st].b])->_int, classname);
			else
				fdef = ED_FieldAtOfs(progfuncs, ((eval_t *)&pr_globals[st16[st].b])->_int);
			if (fdef)
				externs->Printf("    %s.%s: %s\n", PR_StringToNative(&progfuncs->funcs, ent->s_name), PR_StringToNative(&progfuncs->funcs, fld->s_name), PR_ValueString(progfuncs, fdef->type, ptr, false));
			else
				externs->Printf("    %s.%s: BAD FIELD DEF - %#x\n", PR_StringToNative(&progfuncs->funcs, ent->s_name), PR_StringToNative(&progfuncs->funcs, fld->s_name), ptr->_int);
		}
	}
}

void PDECL PR_StackTrace (pubprogfuncs_t *ppf, int showlocals)
{
	progfuncs_t *progfuncs = (progfuncs_t *)ppf;
	const mfunction_t	*f;
	int prnum;
	int			i, st;
	int progs;
	int ofs;
	int *globalbase;
	int tracing = progfuncs->funcs.debug_trace;
	progs = -1;

	if (pr_depth == 0)
	{
		externs->Printf ("<NO STACK>\n");
		return;
	}

	progfuncs->funcs.debug_trace = -10;	//PR_StringToNative(+via PR_ValueString) has various error conditions that we want to mute instead of causing recursive errors.

	//point this to the function's locals
	globalbase = (int *)pr_globals + pr_xfunction->parm_start + pr_xfunction->locals;

	for (i=pr_depth ; i>0 ; i--)
	{
		if (i == pr_depth)
		{
			f = pr_xfunction;
			st = pr_xstatement;
			prnum = prinst.pr_typecurrent;
		}
		else
		{
			f = pr_stack[i].f;
			st = pr_stack[i].s;
			prnum = pr_stack[i].progsnum;
		}

		if (!f)
		{
			externs->Printf ("<NO FUNCTION>\n");
		}
		else
		{
			globalbase -= f->locals;

			if (prnum != progs)
			{
				progs = prnum;

				externs->DPrintf ("<%s>\n", pr_progstate[progs].filename);
			}
			if (!f->s_file)
				externs->Printf ("unknown-file : %s\n", PR_StringToNative(ppf, f->s_name));
			else
			{
				if (pr_progstate[progs].linenums)
					externs->Printf ("%12s:%i: %s\n", PR_StringToNative(ppf, f->s_file), pr_progstate[progs].linenums[st], PR_StringToNative(ppf, f->s_name));
				else
					externs->Printf ("%12s : %s+%i\n", PR_StringToNative(ppf, f->s_file), PR_StringToNative(ppf, f->s_name), st-f->first_statement);
			}

			//locals:0 = no locals
			//locals:1 = top only
			//locals:2 = ALL locals.
			if ((i == pr_depth && showlocals == 1) || showlocals >= 2)
			{
				for (ofs = 0; ofs < f->locals; ofs++)
				{
					ddef16_t *local;
					local = ED_GlobalAtOfs16(progfuncs, f->parm_start+ofs);
					if (!local)
					{
						int arg, aofs;
						for (arg = 0, aofs = 0; arg < f->numparms; arg++)
						{
							if (ofs >= aofs && ofs < aofs + f->parm_size[arg])
								break;
							aofs += f->parm_size[arg];
						}
						if (arg < f->numparms)
						{
							if (f->parm_size[arg] == 3)
							{	//looks like a vector. print it as such
								externs->Printf("    arg%i(%i): [%g, %g, %g]\n", arg, f->parm_start+ofs, *(float *)(globalbase+ofs), *(float *)(globalbase+ofs+1), *(float *)(globalbase+ofs+2));
								ofs += 2;
							}
							else
								externs->Printf("    arg%i(%i): %g===%i\n", arg, f->parm_start+ofs, *(float *)(globalbase+ofs), *(int *)(globalbase+ofs) );
						}
						else
						{
							externs->Printf("     unk(%i): %g===%i\n", f->parm_start+ofs, *(float *)(globalbase+ofs), *(int *)(globalbase+ofs) );
						}
					}
					else
					{
						externs->Printf("    %s: %s\n", PR_StringToNative(ppf, local->s_name), PR_ValueString(progfuncs, local->type, (eval_t*)(globalbase+ofs), false));
						if (local->type == ev_vector)
							ofs+=2;
					}
				}
			}
			if (i == pr_depth)
			{	//scan for op_address/op_load instructions within the function
				PR_PrintRelevantLocals(progfuncs);
			}

			if (i == pr_depth)
				globalbase = prinst.localstack + prinst.localstack_used;
		}
	}
	progfuncs->funcs.debug_trace = tracing;
}

/*
============================================================================
PR_ExecuteProgram

The interpretation main loop
============================================================================
*/

/*
====================
PR_EnterFunction

Returns the new program statement counter
====================
*/
int ASMCALL PR_EnterFunction (progfuncs_t *progfuncs, mfunction_t *f, int progsnum)
{
	int		i, j, c, o;
	prstack_t *st;

	if (pr_depth == MAX_STACK_DEPTH)
	{
		PR_StackTrace (&progfuncs->funcs, false);

		externs->Printf ("stack overflow on call to %s (depth %i)\n", progfuncs->funcs.stringtable+f->s_name, pr_depth);

		//comment this out if you want the progs to try to continue anyway (could cause infinate loops)
		PR_AbortStack(&progfuncs->funcs);
		externs->Abort("Stack Overflow in %s\n", progfuncs->funcs.stringtable+f->s_name);
		return pr_xstatement;
	}
	st = &pr_stack[pr_depth++];
	st->s = pr_xstatement;
	st->f = pr_xfunction;
	st->progsnum = progsnum;
	st->pushed = prinst.spushed;
	st->stepping = progfuncs->funcs.debug_trace;
	if (progfuncs->funcs.debug_trace == DEBUG_TRACE_OVER)
		progfuncs->funcs.debug_trace = DEBUG_TRACE_OFF;
	if (prinst.profiling)
	{
		st->timestamp = Sys_GetClock();
	}

	prinst.localstack_used += prinst.spushed;	//make sure the call doesn't hurt pushed pointers

// save off any locals that the new function steps on (to a side place, fromwhere they are restored on exit)
	c = f->locals;
	if (prinst.localstack_used + c > LOCALSTACK_SIZE)
	{
		prinst.localstack_used -= prinst.spushed;
		pr_depth--;
		PR_RunError (&progfuncs->funcs, "PR_ExecuteProgram: locals stack overflow\n");
	}

	for (i=0 ; i < c ; i++)
		prinst.localstack[prinst.localstack_used+i] = ((int *)pr_globals)[f->parm_start + i];
	prinst.localstack_used += c;

// copy parameters (set initial values)
	o = f->parm_start;
	for (i=0 ; i<f->numparms ; i++)
	{
		for (j=0 ; j<f->parm_size[i] ; j++)
		{
			((int *)pr_globals)[o] = ((int *)pr_globals)[OFS_PARM0+i*3+j];
			o++;
		}
	}

	pr_xfunction = f;
	return f->first_statement - 1;	// offset the s++
}

/*
====================
PR_LeaveFunction
====================
*/
int ASMCALL PR_LeaveFunction (progfuncs_t *progfuncs)
{
	int		i, c;
	prstack_t *st;

	if (pr_depth <= 0)
		Sys_Error ("prog stack underflow");

	// up stack
	st = &pr_stack[--pr_depth];

// restore locals from the stack
	c = pr_xfunction->locals;
	prinst.localstack_used -= c;
	if (prinst.localstack_used < 0)
		PR_RunError (&progfuncs->funcs, "PR_ExecuteProgram: locals stack underflow\n");

	for (i=0 ; i < c ; i++)
		((int *)pr_globals)[pr_xfunction->parm_start + i] = prinst.localstack[prinst.localstack_used+i];

	PR_SwitchProgsParms(progfuncs, st->progsnum);
	prinst.spushed = st->pushed;

	if (!progfuncs->funcs.debug_trace)
		progfuncs->funcs.debug_trace = st->stepping;

	if (prinst.profiling)
	{
		prclocks_t cycles;
		cycles = Sys_GetClock() - st->timestamp;
		if (cycles > prinst.profilingalert)
			externs->Printf("QC call to %s took over a second\n", PR_StringToNative(&progfuncs->funcs,pr_xfunction->s_name));
		pr_xfunction->profiletime += cycles;
		pr_xfunction = st->f;
		if (pr_depth)
			pr_xfunction->profilechildtime += cycles;
	}
	else
		pr_xfunction = st->f;

	prinst.localstack_used -= prinst.spushed;
	return st->s;
}

ddef32_t *ED_FindLocalOrGlobal(progfuncs_t *progfuncs, const char *name, eval_t **val)
{
	static ddef32_t def;
	ddef32_t *def32;
	ddef16_t *def16;
	int i;
	progstate_t *cp = current_progstate;

	if (!cp)
		return NULL;

	switch (cp->structtype)
	{
	case PST_DEFAULT:
	case PST_KKQWSV:
		//this gets parms fine, but not locals
		if (pr_xfunction)
		for (i = 0; i < pr_xfunction->locals; i++)
		{
			def16 = ED_GlobalAtOfs16(progfuncs, pr_xfunction->parm_start+i);
			if (!def16)
				continue;
			if (!strcmp(def16->s_name+progfuncs->funcs.stringtable, name))
			{
				*val = (eval_t *)&cp->globals[pr_xfunction->parm_start+i];

				//we need something like this for functions that are not the top layer
	//			*val = (eval_t *)&localstack[localstack_used-pr_xfunction->numparms*4];
				def.ofs = def16->ofs;
				def.s_name = def16->s_name;
				def.type = def16->type;
				return &def;
			}
		}
		def16 = ED_FindGlobal16(progfuncs, name);
		if (!def16)
			return NULL;
		def.ofs = def16->ofs;
		def.type = def16->type;
		def.s_name = def16->s_name;
		def32 = &def;
		break;
	case PST_QTEST:
	case PST_FTE32:
		//this gets parms fine, but not locals
		if (pr_xfunction)
		for (i = 0; i < pr_xfunction->numparms; i++)
		{
			def32 = ED_GlobalAtOfs32(progfuncs, pr_xfunction->parm_start+i);
			if (!def32)
				continue;
			if (!strcmp(def32->s_name+progfuncs->funcs.stringtable, name))
			{
				*val = (eval_t *)&cp->globals[pr_xfunction->parm_start+i];

				//we need something like this for functions that are not the top layer
	//			*val = (eval_t *)&localstack[localstack_used-pr_xfunction->numparms*4];
				return def32;
			}
		}
		def32 = ED_FindGlobal32(progfuncs, name);
		if (!def32)
			return NULL;
		break;
	default:
		Sys_Error("Bad struct type in ED_FindLocalOrGlobal");
		def32 = NULL;
	}

	*val = (eval_t *)&cp->globals[def32->ofs];
	return &def;
}

static char *COM_TrimString(const char *str, char *buffer, int buffersize)
{
	int i;
	while (*str <= ' ' && *str>'\0')
		str++;

	for (i = 0; i < buffersize-1; i++)
	{
		if (*str <= ' ')
			break;
		buffer[i] = *str++;
	}
	buffer[i] = '\0';
	return buffer;
}

pbool LocateDebugTerm(progfuncs_t *progfuncs, const char *key, eval_t **result, etype_t *rettype, eval_t *store)
{
	ddef32_t *def;
	fdef_t *fdef;
	int fofs;
	eval_t *val = NULL, *fval=NULL;
	char *c, *c2;
	etype_t type = ev_void;
	struct edictrun_s *ed;
//	etype_t ptrtype = ev_void;

	c = strchr(key, '.');
	if (c) *c = '\0';
	def = ED_FindLocalOrGlobal(progfuncs, key, &val);
	if (!def)
	{
		if (*key == '\'')
		{
			type = ev_vector;
			val = store;
			val->_vector[0] = 0;
			val->_vector[1] = 0;
			val->_vector[2] = 0;
		}
		else if (*key == '\"')
		{
			type = ev_string;
			val = store;
			val->string = 0;
		}
		else if (atoi(key))
		{
			type = ev_entity;
			val = store;
			val->edict = atoi(key);
		}
	}
	else
		type = def->type;

	if (c) *c = '.';
	if (!val)
	{
		return false;
	}

	//go through ent vars
	c = strchr(key, '.');
	while(c)
	{
		c2 = c+1;
		c = strchr(c2, '.');
		type = type &~DEF_SAVEGLOBAL;
		if (current_progstate && current_progstate->types)
			type = current_progstate->types[type].type;
		if (type != ev_entity)
			return false;
		if (c)*c = '\0';

		fdef = ED_FindField(progfuncs, c2);
		if (!fdef)
		{
			char trimmed[256];
			c2 = COM_TrimString(c2, trimmed, sizeof(trimmed));
			def = ED_FindLocalOrGlobal(progfuncs, c2, &fval);
			if (def && def->type == ev_field)
			{
				fofs = fval->_int + progfuncs->funcs.fieldadjust;
				fdef = ED_FieldAtOfs(progfuncs, fofs);
			}
		}

		if (c)*c = '.';
		if (!fdef)
			return false;
		fofs = fdef->ofs;
		type = fdef->type;

		if ((unsigned int)val->_int >= prinst.maxedicts)
			ed = NULL;
		else
			ed = PROG_TO_EDICT_PB(progfuncs, val->_int);
		if (!ed)
			return false;
		if (fofs < 0 || fofs >= (int)prinst.max_fields_size)
			return false;
		val = (eval_t *) (((char *)ed->fields) + fofs*4);
	}
	*rettype = type;
	*result = val;
	return true;
}

pbool PDECL PR_SetWatchPoint(pubprogfuncs_t *ppf, const char *key)
{
	progfuncs_t *progfuncs = (progfuncs_t *)ppf;
	eval_t *val;
	eval_t fakeval;
	etype_t type;

	if (!key)
	{
		free(prinst.watch_name);
		prinst.watch_name = NULL;
		prinst.watch_ptr = NULL;
		prinst.watch_type = ev_void;
		return false;
	}
	if (!LocateDebugTerm(progfuncs, key, &val, &type, &fakeval))
	{
		externs->Printf("Unable to evaluate watch term \"%s\"\n", key);
		return false;
	}
	if (val == &fakeval)
	{
		externs->Printf("Do you like watching paint dry?\n");
		return false;
	}
	if (type == ev_vector)
	{
		externs->Printf("Unable to watch vectors. Watching the x field instead.\n");
		type = ev_float;
	}

	free(prinst.watch_name);
	prinst.watch_name = strdup(key);
	prinst.watch_ptr = val;
	prinst.watch_old = *prinst.watch_ptr;
	prinst.watch_type = type &~ DEF_SAVEGLOBAL;
	return true;
}

static const char *PR_ParseCast(const char *key, etype_t *t, pbool *isptr)
{
	extern char *basictypenames[];
	int type;
	*t = ev_void;
	*isptr = false;
	while(*key == ' ')
		key++;
	if (*key == '(')
	{
		key++;
		for (type = 0; type < 10; type++)
		{
			if (!strncmp(key, basictypenames[type], strlen(basictypenames[type])))
			{
				key += strlen(basictypenames[type]);
				while(*key == ' ')
					key++;
				if (*key == '*')
				{
					*isptr = true;
					key++;
				}
				*t = type;
				break;
			}
		}
		if (type == 10)
			return NULL;

		while(*key == ' ')
			key++;
		if (*key++ != ')')
			return NULL;
	}
	return key;
}
char *PDECL PR_EvaluateDebugString(pubprogfuncs_t *ppf, const char *key)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	static char buf[8192];
	fdef_t *fdef;
	eval_t *val;
	char *assignment;
	etype_t type;
	eval_t fakeval;
	extern char *basictypenames[];

	if (*key == '*')
	{
		int ptr;
		eval_t v;
		etype_t cast;
		pbool isptr;
		type = ev_void;
		key = PR_ParseCast(key+1, &cast, &isptr);
		if (!key || !isptr)
			return "(unable to evaluate)";
		if (*key == '&')
		{
			if (!LocateDebugTerm(progfuncs, key+1, &val, &type, &fakeval) && val != &fakeval)
				return "(unable to evaluate)";
			v._int = (char*)val - progfuncs->funcs.stringtable;
			val = &v;
			type = ev_pointer;
		}
		else
		{
			if (!LocateDebugTerm(progfuncs, key, &val, &type, &fakeval) && val != &fakeval)
				return "(unable to evaluate)";
		}
		if (type == ev_integer || type == ev_string || type == ev_pointer)
			ptr = val->_int;
		else if (type == ev_float)
			ptr = val->_float;
		else
			return "(unable to evaluate)";
		return PR_ValueString(progfuncs, cast, (eval_t*)(progfuncs->funcs.stringtable + ptr), true);
	}
	if (*key == '&')
	{
		if (!LocateDebugTerm(progfuncs, key+1, &val, &type, &fakeval) && val != &fakeval)
			return "(unable to evaluate)";
		QC_snprintfz(buf, sizeof(buf), "(%s*)%#x", ((type>=10)?"???":basictypenames[type]), (unsigned int)((char*)val - progfuncs->funcs.stringtable));
		return buf;
	}

	assignment = strchr(key, '=');
	if (assignment)
		*assignment = '\0';

	if (!LocateDebugTerm(progfuncs, key, &val, &type, &fakeval))
		return "(unable to evaluate)";

		/*
	c = strchr(key, '.');
	if (c) *c = '\0';
	def = ED_FindLocalOrGlobal(progfuncs, key, &val);
	if (!def)
	{
		if (atoi(key))
		{
			def = &fakedef;
			def->ofs = 0;
			def->type = ev_entity;
			val = &fakeval;
			val->edict = atoi(key);
		}
	}
	if (c) *c = '.';
	if (!def)
	{
		return "(Bad string)";
	}
	type = def->type;

	//go through ent vars
	c = strchr(key, '.');
	while(c)
	{
		c2 = c+1;
		c = strchr(c2, '.');
		type = type &~DEF_SAVEGLOBAL;
		if (current_progstate && current_progstate->types)
			type = current_progstate->types[type].type;
		if (type != ev_entity)
			return "'.' without entity";
		if (c)*c = '\0';
		fdef = ED_FindField(progfuncs, COM_TrimString(c2));
		if (c)*c = '.';
		if (!fdef)
			return "(Bad string)";
		ed = PROG_TO_EDICT(progfuncs, val->_int);
		if (!ed)
			return "(Invalid Entity)";
		val = (eval_t *) (((char *)ed->fields) + fdef->ofs*4);
		type = fdef->type;
	}
*/
	if (assignment)
	{
		char *str = assignment+1;
		while(*str == ' ')
			str++;
		switch (type&~DEF_SAVEGLOBAL)
		{
		case ev_string:
#ifdef QCGC
			*(string_t *)val = PR_AllocTempString(&progfuncs->funcs, str);
#else
			*(string_t *)val = PR_StringToProgs(&progfuncs->funcs, ED_NewString (&progfuncs->funcs, assignment, 0, true));
#endif
			break;

		case ev_float:
			if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
				*(float*)val = strtoul(str, NULL, 0);
			else
				*(float *)val = (float)atof (str);
			break;

		case ev_integer:
			*(int *)val = atoi (str);
			break;

		case ev_vector:
			{
				int i;
				if (*str == '\'')
					str++;
				for (i = 0; i < 3; i++)
				{
					while(*str == ' ' || *str == '\t')
						str++;
					((float *)val)[i] = strtod(str, &str);
				}
				while(*str == ' ' || *str == '\t')
					str++;
				if (*str == '\'')
					str++;
			}
			break;

		case ev_entity:
			if (!EDICT_NUM(progfuncs, atoi (str)))
				return "(invalid entity)";
			*(int *)val = EDICT_TO_PROG(progfuncs, EDICT_NUM(progfuncs, atoi (str)));
			break;

		case ev_field:
			fdef = ED_FindField (progfuncs, str);
			if (!fdef)
			{
				size_t l,nl = strlen(str);

				*assignment = '=';

				strcpy(buf, "Can't find field ");
				l = strlen(buf);
				if (nl > sizeof(buf)-l-2)
					nl = sizeof(buf)-l-2;
				memcpy(buf+l, str, nl);
				buf[l+nl+1] = 0;
				return buf;
			}
			*(int *)val = G_INT(fdef->ofs);
			break;

		case ev_function:
			{
				mfunction_t *func;
				int i;
				int progsnum = -1;
				if (str[0] && str[1] == ':')
				{
					progsnum = atoi(str);
					str+=2;
				}
				else if (str[0] && str[1] && str[2] == ':')
				{
					progsnum = atoi(str);
					str+=3;
				}

				func = ED_FindFunction (progfuncs, str, &i, progsnum);
				if (!func)
				{
					size_t l,nl = strlen(str);

					*assignment = '=';

					strcpy(buf, "Can't find field ");
					l = strlen(buf);
					if (nl > sizeof(buf)-l-2)
						nl = sizeof(buf)-l-2;
					memcpy(buf+l, str, nl);
					buf[l+nl+1] = 0;
					return buf;
				}
				*(func_t *)val = (func - pr_progstate[i].functions) | (i<<24);
			}
			break;

		default:
			break;

		}
		*assignment = '=';
	}
	QC_snprintfz(buf, sizeof(buf), "%s", PR_ValueString(progfuncs, type, val, true));

	return buf;
}

//int EditorHighlightLine(window_t *wnd, int line);
void SetExecutionToLine(progfuncs_t *progfuncs, int linenum)
{
	int pn = prinst.pr_typecurrent;
	int snum;
	const mfunction_t *f = pr_xfunction;

	switch(current_progstate->structtype)
	{
	case PST_DEFAULT:
	case PST_QTEST:
		for (snum = f->first_statement; pr_progstate[pn].linenums[snum] < linenum; snum++)
		{
			if (pr_statements16[snum].op == OP_DONE)
				return;
		}
		break;
	case PST_KKQWSV:
	case PST_FTE32:
		for (snum = f->first_statement; pr_progstate[pn].linenums[snum] < linenum; snum++)
		{
			if (pr_statements32[snum].op == OP_DONE)
				return;
		}
		break;
	default:
		Sys_Error("Bad struct type");
		snum = 0;
	}
	prinst.debugstatement = snum;
//	EditorHighlightLine(editwnd, pr_progstate[pn].linenums[snum]);
}

struct sortedfunc_s
{
	int firststatement;
	int firstline;
};
int PDECL PR_SortBreakFunctions(const void *va, const void *vb)
{
	const struct sortedfunc_s *a = va;
	const struct sortedfunc_s *b = vb;
	if (a->firstline == b->firstline)
		return 0;
	return a->firstline > b->firstline;
}

//0 clear. 1 set, 2 toggle, 3 check
int PDECL PR_ToggleBreakpoint(pubprogfuncs_t *ppf, const char *filename, int linenum, int flag)	//write alternate route to work by function name.
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	int ret=0;
	unsigned int fl, stline;
	unsigned int i, j;

	progstate_t *cp;
	mfunction_t *f;
	int op = 0; //warning about not being initialized before use

	if (!pr_progstate)
		return ret;

	for (j = 0; j < prinst.maxprogs; j++)
	{
		cp = &pr_progstate[j];
		if (!cp->progs)
			continue;

		if (linenum)	//linenum is set means to set the breakpoint on a file and line
		{
			struct sortedfunc_s *sortedstatements;
			int numfilefunctions = 0;
			if (!cp->linenums)
				continue;
			sortedstatements = alloca(cp->progs->numfunctions * sizeof(*sortedstatements));

			//we need to use the function table in order to set breakpoints in the right file.
			for (f = cp->functions, fl = 0; fl < cp->progs->numfunctions; f++, fl++)
			{
				const char *fncfile = f->s_file+progfuncs->funcs.stringtable;
				if (fncfile[0] == '.' && fncfile[1] == '/')
					fncfile+=2;
				if (!stricmp(fncfile, filename))
				{
					sortedstatements[numfilefunctions].firststatement = f->first_statement;
					if (f->first_statement < 0 || f->first_statement >= (int)cp->progs->numstatements)
						sortedstatements[numfilefunctions].firstline = 0;
					else
						sortedstatements[numfilefunctions].firstline = cp->linenums[f->first_statement];
					numfilefunctions++;
				}
			}
			f = NULL;
			qsort(sortedstatements, numfilefunctions, sizeof(*sortedstatements), PR_SortBreakFunctions);

			//our functions are now in terms of ascending line numbers.
			for (fl = 0; fl < numfilefunctions; fl++)
			{
				for (i = sortedstatements[fl].firststatement; i < cp->progs->numstatements; i++)
				{
					if (cp->linenums[i] >= linenum)
					{
						stline = cp->linenums[i];
						for (; ; i++)
						{
							if ((unsigned int)cp->linenums[i] != stline)
								break;

							switch(cp->structtype)
							{
							case PST_DEFAULT:
							case PST_QTEST:
								op = ((dstatement16_t*)cp->statements + i)->op;
								break;
							case PST_KKQWSV:
							case PST_FTE32:
								op = ((dstatement32_t*)cp->statements + i)->op;
								break;
							default:
								Sys_Error("Bad structtype");
								op = 0;
							}
							switch (flag)
							{
							default:
								if (op & OP_BIT_BREAKPOINT)
								{
									op &= ~OP_BIT_BREAKPOINT;
									ret = false;
									flag = 0;
								}
								else
								{
									op |= OP_BIT_BREAKPOINT;
									ret = true;
									flag = 1;
								}
								break;
							case 0:
								op &= ~OP_BIT_BREAKPOINT;
								ret = false;
								break;
							case 1:
								op |= OP_BIT_BREAKPOINT;
								ret = true;
								break;
							case 3:
								if (op & OP_BIT_BREAKPOINT)
									return true;
							}
							switch(cp->structtype)
							{
							case PST_DEFAULT:
							case PST_QTEST:
								((dstatement16_t*)cp->statements + i)->op = op;
								break;
							case PST_KKQWSV:
							case PST_FTE32:
								((dstatement32_t*)cp->statements + i)->op = op;
								break;
							default:
								Sys_Error("Bad structtype");
								op = 0;
							}
							if (ret)	//if its set, only set one breakpoint statement, not all of them.
								break;

							if ((op & ~OP_BIT_BREAKPOINT) == OP_DONE)
								break;	//give up when we see the function's done.
						}
						goto cont;	//next progs
					}
				}
			}
		}
		else	//set the breakpoint on the first statement of the function specified.
		{
			for (f = cp->functions, fl = 0; fl < cp->progs->numfunctions; f++, fl++)
			{
				if (!strcmp(f->s_name+progfuncs->funcs.stringtable, filename))
				{
					i = f->first_statement;
					switch(cp->structtype)
					{
					case PST_DEFAULT:
					case PST_QTEST:
						op = ((dstatement16_t*)cp->statements + i)->op;
						break;
					case PST_KKQWSV:
					case PST_FTE32:
						op = ((dstatement32_t*)cp->statements + i)->op;
						break;
					default:
						Sys_Error("Bad structtype");
					}
					switch (flag)
					{
					default:
						if (op & 0x8000)
						{
							op &= ~0x8000;
							ret = false;
							flag = 0;
						}
						else
						{
							op |= 0x8000;
							ret = true;
							flag = 1;
						}
						break;
					case 0:
						op &= ~0x8000;
						ret = false;
						break;
					case 1:
						op |= 0x8000;
						ret = true;
						break;
					case 3:
						if (op & 0x8000)
							return true;
					}
					switch(cp->structtype)
					{
					case PST_DEFAULT:
					case PST_QTEST:
						((dstatement16_t*)cp->statements + i)->op = op;
						break;
					case PST_KKQWSV:
					case PST_FTE32:
						((dstatement32_t*)cp->statements + i)->op = op;
						break;
					default:
						Sys_Error("Bad structtype");
					}
					break;
				}
			}
		}
cont:
		continue;
	}

	return ret;
}

int ShowStep(progfuncs_t *progfuncs, int statement, char *fault, pbool fatal)
{
//FIXME: statics are evil, but at least the lastfile pointer check _should_ isolate different vms.
static unsigned int lastline = 0;
static unsigned int ignorestatement = 0;
static const char *lastfile = NULL;
	const char *file = NULL;

	int pn = prinst.pr_typecurrent;
	int i;
	const mfunction_t *f = pr_xfunction;
	int faultline;
	int debugaction;
	pr_xstatement = statement;

	if (!externs->useeditor)
	{
		PR_PrintStatement(progfuncs, statement);
		if (fatal)
			progfuncs->funcs.debug_trace = DEBUG_TRACE_ABORT;
		return statement;
	}

	if (f)
	{
		for(;;)	//for DEBUG_TRACE_NORESUME handling
		{
			file = PR_StringToNative(&progfuncs->funcs, f->s_file);
			if (pr_progstate[pn].linenums)
			{
				if (lastline == pr_progstate[pn].linenums[statement] && lastfile == file && statement == ignorestatement && !fault)
				{
					ignorestatement++;
					return statement;	//no info/same line as last time
				}

				lastline = pr_progstate[pn].linenums[statement];
			}
			else
				lastline = -1;
			lastfile = file;

			faultline = lastline;
			debugaction = externs->useeditor(&progfuncs->funcs, lastfile, ((lastline!=-1)?&lastline:NULL), &statement, f->first_statement, fault, fatal);

//			if (pn != prinst.pr_typecurrent)

			//if they changed the line to execute, we need to find a statement that is on that line
			if (lastline && faultline != lastline)
			if (pr_progstate[pn].linenums)
			{
				switch(pr_progstate[pn].structtype)
				{
				case PST_FTE32:
				case PST_KKQWSV:
					{
						dstatement32_t *st = pr_progstate[pn].statements;
						unsigned int *lnos = pr_progstate[pn].linenums;
						for (i = f->first_statement; ; i++)
						{
							if (lastline == lnos[i])
							{
								statement = i;
								break;
							}
							else if (lastline <= lnos[i])
								break;
							else if (st[i].op == OP_DONE)
								break;
						}
					}
					break;
				case PST_DEFAULT:
				case PST_QTEST:
					{
						dstatement16_t *st = pr_progstate[pn].statements;
						unsigned int *lnos = pr_progstate[pn].linenums;
						for (i = f->first_statement; ; i++)
						{
							if (lastline == lnos[i])
							{
								statement = i;
								break;
							}
							else if (lastline <= lnos[i])
								break;
							else if (st[i].op == OP_DONE)
								break;
						}
					}
				}
			}

			if (debugaction == DEBUG_TRACE_NORESUME)
				continue;
			else if(debugaction == DEBUG_TRACE_ABORT)
				progfuncs->funcs.parms->Abort ("%s", fault?fault:"Debugger Abort");
			else if (debugaction == DEBUG_TRACE_OFF)
			{
				//if we're resuming, don't hit any lingering step-over triggers
				progfuncs->funcs.debug_trace = DEBUG_TRACE_OFF;
				for (i = 0; i < pr_depth; i++)
					pr_stack[pr_depth-1].stepping = DEBUG_TRACE_OFF;
			}
			else if (debugaction == DEBUG_TRACE_OUT)
			{
				//clear tracing for now, but ensure that it'll be reactivated once we reach the caller (if from qc)
				progfuncs->funcs.debug_trace = DEBUG_TRACE_OFF;
				if (pr_depth)
					pr_stack[pr_depth-1].stepping = DEBUG_TRACE_INTO;
			}
			else	//some other debug action. maybe resume.
				progfuncs->funcs.debug_trace = debugaction;
			break;
		}
	}

	ignorestatement = statement+1;
	return statement;
}

//called by the qcvm when executing some statement that cannot be execed.
int PR_HandleFault (pubprogfuncs_t *ppf, char *error, ...)
{
	progfuncs_t *progfuncs = (progfuncs_t *)ppf;
	va_list		argptr;
	char		string[1024];
	int resumestatement;

	va_start (argptr,error);
	Q_vsnprintf (string,sizeof(string)-1, error,argptr);
	va_end (argptr);

	PR_StackTrace (ppf, true);
	ppf->parms->Printf ("%s\n", string);

	resumestatement = ShowStep(progfuncs, pr_xstatement, string, true);

	if (resumestatement == 0)
	{
		PR_AbortStack(ppf);
		return prinst.continuestatement;
//		ppf->parms->Abort ("%s", string);
	}
	return resumestatement;
}

/*
============
PR_RunError

Aborts the currently executing function
============
*/
void VARGS PR_RunError (pubprogfuncs_t *progfuncs, const char *error, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr,error);
	Q_vsnprintf (string,sizeof(string)-1, error,argptr);
	va_end (argptr);

//	PR_PrintStatement (pr_statements + pr_xstatement);
	PR_StackTrace (progfuncs, true);
	progfuncs->parms->Printf ("\n");

//editbadfile(pr_strings + pr_xfunction->s_file, -1);

	progfuncs->parms->Abort ("%s", string);
}

pbool PR_RunWarning (pubprogfuncs_t *ppf, char *error, ...)
{
	progfuncs_t *progfuncs = (progfuncs_t *)ppf;
	va_list		argptr;
	char		string[1024];

	va_start (argptr,error);
	Q_vsnprintf (string,sizeof(string)-1, error,argptr);
	va_end (argptr);

	progfuncs->funcs.parms->Printf ("%s", string);
	if (pr_depth != 0)
		PR_StackTrace (ppf, false);

	if (progfuncs->funcs.debug_trace == 0)
	{
		progfuncs->funcs.debug_trace = DEBUG_TRACE_INTO;
		return true;
	}
	return false;
}
static pbool PR_ExecRunWarning (pubprogfuncs_t *ppf, int xstatement, char *error, ...)
{
	progfuncs_t *progfuncs = (progfuncs_t *)ppf;
	va_list		argptr;
	char		string[1024];

	pr_xstatement = xstatement;

	va_start (argptr,error);
	Q_vsnprintf (string,sizeof(string)-1, error,argptr);
	va_end (argptr);

	progfuncs->funcs.parms->Printf ("%s", string);
	if (pr_depth != 0)
		PR_StackTrace (ppf, false);

	if (progfuncs->funcs.debug_trace == 0)
	{
		pr_xstatement = ShowStep(progfuncs, xstatement, string, false);
		return true;
	}
	return false;
}

//For debugging. Assumes classname field exists.
const char *PR_GetEdictClassname(progfuncs_t *progfuncs, unsigned int edict)
{
	fdef_t *cnfd = ED_FindField(progfuncs, "classname");
	if (cnfd && edict < prinst.maxedicts)
	{
		string_t *v = (string_t *)((char *)edvars(PROG_TO_EDICT_PB(progfuncs, edict)) + cnfd->ofs*4);
		return PR_StringToNative(&progfuncs->funcs, *v);
	}
	return "";
}


static pbool casecmp_f(progfuncs_t *progfuncs, eval_t *ref, eval_t *val)	{return ref->_float == val->_float;}
static pbool casecmp_i(progfuncs_t *progfuncs, eval_t *ref, eval_t *val)	{return ref->_int == val->_int;}
static pbool casecmp_v(progfuncs_t *progfuncs, eval_t *ref, eval_t *val)	{return ref->_vector[0] == val->_vector[0] && 
																					ref->_vector[1] == val->_vector[1] &&
																					ref->_vector[2] == val->_vector[2];}
static pbool casecmp_s(progfuncs_t *progfuncs, eval_t *ref, eval_t *val)	{	const char *refs = PR_StringToNative(&progfuncs->funcs, ref->string);
																				const char *vals = PR_StringToNative(&progfuncs->funcs, val->string);
																				return !strcmp(refs, vals);}
static pbool casecmprange_f(progfuncs_t *progfuncs, eval_t *ref, eval_t *min, eval_t *max)	{return ref->_float >= min->_float && ref->_float <= max->_float;}
static pbool casecmprange_i(progfuncs_t *progfuncs, eval_t *ref, eval_t *min, eval_t *max)	{return ref->_int >= min->_int && ref->_int <= max->_int;}
static pbool casecmprange_v(progfuncs_t *progfuncs, eval_t *ref, eval_t *min, eval_t *max)	{return ref->_vector[0] >= min->_vector[0] && ref->_vector[0] <= max->_vector[0] &&
																									ref->_vector[1] >= min->_vector[1] && ref->_vector[1] <= max->_vector[1] &&
																									ref->_vector[2] >= min->_vector[2] && ref->_vector[2] <= max->_vector[2];}
static pbool casecmprange_bad(progfuncs_t *progfuncs, eval_t *ref, eval_t *min, eval_t *max){	PR_RunError (&progfuncs->funcs, "OP_CASERANGE type not supported");//BUG: pr_xstatement will not be correct.
																								return false;}
typedef pbool (*casecmp_t)(progfuncs_t *progfuncs, eval_t *ref, eval_t *val);
typedef pbool (*casecmprange_t)(progfuncs_t *progfuncs, eval_t *ref, eval_t *min, eval_t *max);
static casecmp_t casecmp[] =
{
	casecmp_f,	//float
	casecmp_v,	//vector
	casecmp_s,	//string
	casecmp_i,	//ent
	casecmp_i	//func
	//pointer, field, int, etc are emulated with func or something. I dunno
};
static casecmprange_t casecmprange[] =
{
	casecmprange_f,	//float
	casecmprange_v,	//vector - I'm using a bbox, not really sure what it should be
	casecmprange_bad,	//string - should it use stof? string ranges don't relly make sense, at all.
	casecmprange_i,	//ent - doesn't really make sense, but as ints/pointers/fields/etc might be emulated with this, allow it anyway, as an int type.
	casecmprange_i	//func
};

#define RUNAWAYCHECK()							\
	if (!--*runaway)								\
	{											\
		pr_xstatement = st-pr_statements;		\
		PR_RunError (&progfuncs->funcs, "runaway loop error\n");\
		PR_StackTrace(&progfuncs->funcs,false);	\
		externs->Printf ("runaway loop error\n");		\
		while(pr_depth > prinst.exitdepth)		\
			PR_LeaveFunction(progfuncs);		\
		prinst.spushed = 0;							\
		return -1;								\
	}

#if defined(FTE_TARGET_WEB) || defined(SIMPLE_QCVM)
static int PR_NoDebugVM(progfuncs_t *fte_restrict progfuncs)
{
	char stack[4*1024];
	size_t ofs;
	strcpy(stack, "This platform does not support QC debugging\nStack Trace:");
	ofs = strlen(stack);
	PR_SaveCallStack (progfuncs, stack, &ofs, sizeof(stack));
	PR_RunError (&progfuncs->funcs, stack);
	return -1;
}
#endif

static int PR_ExecuteCode16 (progfuncs_t *fte_restrict progfuncs, int s, int *fte_restrict runaway)
{
	unsigned int switchcomparison = 0;
	const dstatement16_t	*fte_restrict st;
	mfunction_t	*fte_restrict newf;
	int		i;
	edictrun_t	*ed;
	eval_t	*ptr;

	float *fte_restrict glob = pr_globals;
	float tmpf;
	int tmpi;
	unsigned short op;

	eval_t	*switchref = (eval_t*)glob;
	unsigned int num_edicts = sv_num_edicts;

#define OPA ((eval_t *)&glob[st->a])
#define OPB ((eval_t *)&glob[st->b])
#define OPC ((eval_t *)&glob[st->c])

#define INTSIZE 16
	st = &pr_statements16[s];
	while (progfuncs->funcs.debug_trace || prinst.watch_ptr || prinst.profiling)
	{
#if defined(FTE_TARGET_WEB) || defined(SIMPLE_QCVM)
		reeval16:
		//this can generate huge functions, so disable it on systems that can't realiably cope with such things (IE initiates an unwanted denial-of-service attack when pointed our javascript, and firefox prints a warning too)
		pr_xstatement = st-pr_statements16;
		return PR_NoDebugVM(progfuncs);
#else
		#define DEBUGABLE
		#ifdef SEPARATEINCLUDES
			#include "execloop16d.h"
		#else
			#include "execloop.h"
		#endif
		#undef DEBUGABLE
#endif
	}

	while(1)
	{
		#include "execloop.h"
	}
#undef INTSIZE
}

static int PR_ExecuteCode32 (progfuncs_t *fte_restrict progfuncs, int s, int *fte_restrict runaway)
{
#if defined(FTE_TARGET_WEB) ||defined(SIMPLE_QCVM)
	//this can generate huge functions, so disable it on systems that can't realiably cope with such things (IE initiates an unwanted denial-of-service attack when pointed our javascript, and firefox prints a warning too)
	pr_xstatement = s;
	PR_RunError (&progfuncs->funcs, "32bit qc statement support was disabled for this platform.\n");
	PR_StackTrace(&progfuncs->funcs, false);
	return -1;
#else

	unsigned int switchcomparison = 0;
	const dstatement32_t	*fte_restrict st;
	mfunction_t	*fte_restrict newf;
	int		i;
	edictrun_t	*ed;
	eval_t	*ptr;

	float *fte_restrict glob = pr_globals;
	float tmpf;
	int tmpi;
	eval_t	*switchref = (eval_t*)glob;
	unsigned int num_edicts = sv_num_edicts;

	unsigned int op;

#define OPA ((eval_t *)&glob[st->a])
#define OPB ((eval_t *)&glob[st->b])
#define OPC ((eval_t *)&glob[st->c])

#define INTSIZE 32
	st = &pr_statements32[s];
	while (progfuncs->funcs.debug_trace || prinst.watch_ptr || prinst.profiling)
	{
		#define DEBUGABLE
		#ifdef SEPARATEINCLUDES
			#include "execloop32d.h"
		#else
			#include "execloop.h"
		#endif
		#undef DEBUGABLE
	}

	while(1)
	{
		#ifdef SEPARATEINCLUDES
			#include "execloop32.h"
		#else
			#include "execloop.h"
		#endif
	}
#undef INTSIZE
#endif
}

/*
====================
PR_ExecuteProgram
====================
*/
static void PR_ExecuteCode (progfuncs_t *progfuncs, int s)
{
	int		runaway;

	if (prinst.watch_ptr && prinst.watch_ptr->_int != prinst.watch_old._int)
	{
		switch(prinst.watch_type)
		{
		case ev_float:
			externs->Printf("Watch point \"%s\" changed by engine from %g to %g.\n", prinst.watch_name, prinst.watch_old._float, prinst.watch_ptr->_float);
			break;
		case ev_vector:
			externs->Printf("Watch point \"%s\" changed by engine from '%g %g %g' to '%g %g %g'.\n", prinst.watch_name, prinst.watch_old._vector[0], prinst.watch_old._vector[1], prinst.watch_old._vector[2], prinst.watch_ptr->_vector[0], prinst.watch_ptr->_vector[1], prinst.watch_ptr->_vector[2]);
			break;
		default:
			externs->Printf("Watch point \"%s\" changed by engine from %i to %i.\n", prinst.watch_name, prinst.watch_old._int, prinst.watch_ptr->_int);
			break;
		case ev_entity:
			externs->Printf("Watch point \"%s\" changed by engine from %i(%s) to %i(%s).\n", prinst.watch_name, prinst.watch_old._int, PR_GetEdictClassname(progfuncs, prinst.watch_old._int), prinst.watch_ptr->_int, PR_GetEdictClassname(progfuncs, prinst.watch_ptr->_int));
			break;
		case ev_function:
		case ev_string:
			externs->Printf("Watch point \"%s\" set by engine to %s.\n", prinst.watch_name, PR_ValueString(progfuncs, prinst.watch_type, prinst.watch_ptr, false));
			break;
		}
		prinst.watch_old = *prinst.watch_ptr;

		//we can't dump stack or anything, as we don't really know the stack frame that it happened in.

		//stop watching
//		prinst->watch_ptr = NULL;
	}

	prinst.continuestatement = -1;
#ifdef QCJIT
	if (current_progstate->jit)
	{
		PR_EnterJIT(progfuncs, current_progstate->jit, s);
		return;
	}
#endif

	runaway = 100000000;

	for(;;)
	{
		switch (current_progstate->structtype)
		{
		case PST_DEFAULT:
		case PST_QTEST:
			s = PR_ExecuteCode16(progfuncs, s, &runaway);
			if (s == -1)
				return;
			continue;
		case PST_KKQWSV:
		case PST_FTE32:
			s = PR_ExecuteCode32(progfuncs, s, &runaway);
			if (s == -1)
				return;
			continue;
		default:
			Sys_Error("PR_ExecuteProgram - bad structtype");
		}
	}
}


void PDECL PR_ExecuteProgram (pubprogfuncs_t *ppf, func_t fnum)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	mfunction_t	*f;
	int		i;
	unsigned int initial_progs;
	int		oldexitdepth;

	int s;

#ifndef QCGC
	int tempdepth;
#endif

	unsigned int newprogs = (fnum & 0xff000000)>>24;

	initial_progs = prinst.pr_typecurrent;
	if (newprogs != initial_progs)
	{
		if (newprogs >= prinst.maxprogs || !pr_progstate[newprogs].globals)	//can happen with hexen2...
		{
			externs->Printf("PR_ExecuteProgram: tried branching into invalid progs (%#x)\n", fnum);
			return;
		}
		PR_SwitchProgsParms(progfuncs, newprogs);
	}

	if (!(fnum & ~0xff000000) || (signed)(fnum & ~0xff000000) >= pr_progs->numfunctions)
	{
//		if (pr_global_struct->self)
//			ED_Print (PROG_TO_EDICT(pr_global_struct->self));
#if defined(__GNUC__) && !defined(FTE_TARGET_WEB) && !defined(NACL)
		externs->Printf("PR_ExecuteProgram: NULL function from exe (address %p)\n", __builtin_return_address(0));
#else
		externs->Printf("PR_ExecuteProgram: NULL function from exe\n");
#endif
//		Host_Error ("PR_ExecuteProgram: NULL function from exe");

//		PR_MoveParms(0, pr_typecurrent);
		PR_SwitchProgs(progfuncs, initial_progs);
		return;
	}

	oldexitdepth = prinst.exitdepth;

	f = &pr_cp_functions[fnum & ~0xff000000];

	if (f->first_statement < 0)
	{	// negative statements are built in functions
		i = -f->first_statement;

		if (i < externs->numglobalbuiltins)
			(*externs->globalbuiltins[i]) (&progfuncs->funcs, (struct globalvars_s *)current_progstate->globals);
		else
		{
			externs->Printf ("Bad builtin call number %i (from exe)\n", -f->first_statement);
		//	PR_MoveParms(p, pr_typecurrent);
			PR_SwitchProgs(progfuncs, initial_progs);
		}
		PR_SwitchProgsParms(progfuncs, initial_progs);
		return;
	}

	//forget about any tracing if its active. control returning to the engine should not look like its calling some random function.
	progfuncs->funcs.debug_trace = DEBUG_TRACE_OFF;

// make a stack frame
	prinst.exitdepth = pr_depth;


	s = PR_EnterFunction (progfuncs, f, initial_progs);

#ifndef QCGC
	tempdepth = prinst.numtempstringsstack;
#endif
	PR_ExecuteCode(progfuncs, s);


	PR_SwitchProgsParms(progfuncs, initial_progs);

#ifndef QCGC
	PR_FreeTemps(progfuncs, tempdepth);
	prinst.numtempstringsstack = tempdepth;
#else
	if (!pr_depth)
		PR_RunGC(progfuncs);
#endif

	prinst.exitdepth = oldexitdepth;
}






typedef struct {
	int fnum;
	int progsnum;
	int statement;
} qcthreadstack_t;
typedef struct qcthread_s {
	int fstackdepth;
	qcthreadstack_t fstack[MAX_STACK_DEPTH];
	int lstackused;
	int lstack[LOCALSTACK_SIZE];
	int xstatement;
	int xfunction;
	progsnum_t xprogs;
} qcthread_t;

struct qcthread_s *PDECL PR_ForkStack(pubprogfuncs_t *ppf)
{	//QC code can call builtins that call qc code.
	//to get around the problems of restoring the builtins we simply don't save the thread over the builtin.
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	int i, l;
	int ed = prinst.exitdepth;
	int localsoffset, baselocalsoffset;
	qcthread_t *thread = externs->memalloc(sizeof(qcthread_t));
	const mfunction_t *f;

	//copy out the functions stack.
	for (i = 0,localsoffset=0; i < ed; i++)
	{
		if (i+1 == pr_depth)
			f = pr_xfunction;
		else
			f = pr_stack[i+1].f;
		localsoffset += f->locals;	//this is where it crashes
	}
	baselocalsoffset = localsoffset;
	for (i = ed; i < pr_depth; i++)
	{
		thread->fstack[i-ed].fnum = pr_stack[i].f - pr_progstate[pr_stack[i].progsnum].functions;
		thread->fstack[i-ed].progsnum = pr_stack[i].progsnum;
		thread->fstack[i-ed].statement = pr_stack[i].s;

		if (i+1 == pr_depth)
			f = pr_xfunction;
		else
			f = pr_stack[i+1].f;
		localsoffset += f->locals;
	}
	thread->fstackdepth = pr_depth - ed;

	for (i = pr_depth - 1; i >= ed ; i--)
	{
		if (i+1 == pr_depth)
			f = pr_xfunction;
		else
			f = pr_stack[i+1].f;
		localsoffset -= f->locals;
		for (l = 0; l < f->locals; l++)
		{
			thread->lstack[localsoffset-baselocalsoffset + l ] = ((int *)pr_globals)[f->parm_start + l];
			((int *)pr_globals)[f->parm_start + l] = prinst.localstack[localsoffset+l];	//copy the old value into the globals (so the older functions have the correct locals.
		}
	}

	for (i = ed; i < pr_depth ; i++)	//we need to get the locals back to how they were.
	{
		if (i+1 == pr_depth)
			f = pr_xfunction;
		else
			f = pr_stack[i+1].f;

		for (l = 0; l < f->locals; l++)
		{
			((int *)pr_globals)[f->parm_start + l] = thread->lstack[localsoffset-baselocalsoffset + l];
		}
		localsoffset += f->locals;
	}
	thread->lstackused = localsoffset - baselocalsoffset;

	thread->xstatement = pr_xstatement;
	thread->xfunction = pr_xfunction - current_progstate->functions;
	thread->xprogs = prinst.pr_typecurrent;

	return thread;
}

void PDECL PR_ResumeThread (pubprogfuncs_t *ppf, struct qcthread_s *thread)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	mfunction_t	*f, *oldf;
	int		i,l,ls;
	progsnum_t initial_progs;
	int		oldexitdepth;

	int s;
#ifndef QCGC
	int tempdepth;
#endif

	progsnum_t prnum = thread->xprogs;
	int fnum = thread->xfunction;

	if (prinst.localstack_used + thread->lstackused > LOCALSTACK_SIZE)
		PR_RunError(&progfuncs->funcs, "Too many locals on resumtion of QC thread\n");

	if (pr_depth + thread->fstackdepth > MAX_STACK_DEPTH)
		PR_RunError(&progfuncs->funcs, "Too large stack on resumtion of QC thread\n");


	//do progs switching stuff as appropriate. (fteqw only)
	initial_progs = prinst.pr_typecurrent;
	PR_SwitchProgsParms(progfuncs, prnum);


	oldexitdepth = prinst.exitdepth;
	prinst.exitdepth = pr_depth;

	ls = 0;
	//add on the callstack.
	for (i = 0; i < thread->fstackdepth; i++)
	{
		if (pr_depth == prinst.exitdepth)
		{
			pr_stack[pr_depth].f = pr_xfunction;
			pr_stack[pr_depth].s = pr_xstatement;
			pr_stack[pr_depth].progsnum = initial_progs;
		}
		else
		{
			pr_stack[pr_depth].progsnum = thread->fstack[i].progsnum;
			pr_stack[pr_depth].f = pr_progstate[thread->fstack[i].progsnum].functions + thread->fstack[i].fnum;
			pr_stack[pr_depth].s = thread->fstack[i].statement;
		}

		if (i+1 == thread->fstackdepth)
			f = &pr_cp_functions[fnum];
		else
			f = pr_progstate[thread->fstack[i+1].progsnum].functions + thread->fstack[i+1].fnum;
		for (l = 0; l < f->locals; l++)
		{
			prinst.localstack[prinst.localstack_used++] = ((int *)pr_globals)[f->parm_start + l];
			((int *)pr_globals)[f->parm_start + l] = thread->lstack[ls++];
		}

		pr_depth++;
	}

	if (ls != thread->lstackused)
		PR_RunError(&progfuncs->funcs, "Thread stores incorrect locals count\n");


	f = &pr_cp_functions[fnum];

//	thread->lstackused -= f->locals;	//the current function is the odd one out.

	//add on the locals stack
	memcpy(prinst.localstack+prinst.localstack_used, thread->lstack, sizeof(int)*thread->lstackused);
	prinst.localstack_used += thread->lstackused;

	//bung the locals of the current function on the stack.
//	for (i=0 ; i < f->locals ; i++)
//		((int *)pr_globals)[f->parm_start + i] = 0xff00ff00;//thread->lstack[thread->lstackused+i];


//	PR_EnterFunction (progfuncs, f, initial_progs);
	oldf = pr_xfunction;
	pr_xfunction = f;
	s = thread->xstatement;

#ifndef QCGC
	tempdepth = prinst.numtempstringsstack;
#endif
	PR_ExecuteCode(progfuncs, s);


	PR_SwitchProgsParms(progfuncs, initial_progs);
#ifndef QCGC
	PR_FreeTemps(progfuncs, tempdepth);
	prinst.numtempstringsstack = tempdepth;
#endif

	prinst.exitdepth = oldexitdepth;
	pr_xfunction = oldf;
}

void	PDECL PR_AbortStack			(pubprogfuncs_t *ppf)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	while(pr_depth > prinst.exitdepth+1)
		PR_LeaveFunction(progfuncs);
	prinst.continuestatement = 0;
}

pbool	PDECL PR_GetBuiltinCallInfo	(pubprogfuncs_t *ppf, int *builtinnum, char *function, size_t sizeoffunction)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	int st = pr_xstatement;
	int op;
	int a;
	const char *fname;
	
	switch (current_progstate->structtype)
	{
	case PST_DEFAULT:
	case PST_QTEST:
		op = pr_statements16[st].op;
		a = pr_statements16[st].a;
		break;
	case PST_KKQWSV:
	case PST_FTE32:
		op = pr_statements32[st].op;
		a = pr_statements32[st].a;
		break;
	default:
		op = OP_DONE;
		a = 0;
		break;
	}

	*builtinnum = 0;
	*function = 0;
	if ((op >= OP_CALL0 && op <= OP_CALL8) || (op >= OP_CALL1H && op <= OP_CALL8H))
	{
		a = ((eval_t *)&pr_globals[a])->function;

		*builtinnum = -current_progstate->functions[a].first_statement;
		fname = PR_StringToNative(ppf, current_progstate->functions[a].s_name);
		strncpy(function, fname, sizeoffunction-1);
		function[sizeoffunction-1] = 0;
		return true;
	}
	return false;
}