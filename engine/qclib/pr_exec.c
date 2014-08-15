#define PROGSUSED
#include "progsint.h"
//#include "editor.h"

#if __STDC_VERSION__ >= 199901L
	#define fte_restrict restrict
#elif defined(_MSC_VER)
	#define fte_restrict __restrict
#else
	#define fte_restrict
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
		printf ("%s ",  pr_opcodes[op].name);
		i = strlen(pr_opcodes[op].name);
		for ( ; i<10 ; i++)
			printf (" ");
	}
	else
#endif
		printf ("op%3i ", op);

	if (op == OP_IF_F || op == OP_IFNOT_F)
		printf ("%sbranch %i",PR_GlobalString(progfuncs, arg[0]),arg[1]);
	else if (op == OP_GOTO)
	{
		printf ("branch %i",arg[0]);
	}
	else if ( (unsigned)(op - OP_STORE_F) < 6)
	{
		printf ("%s",PR_GlobalString(progfuncs, arg[0]));
		printf ("%s", PR_GlobalStringNoContents(progfuncs, arg[1]));
	}
	else
	{
		if (arg[0])
			printf ("%s",PR_GlobalString(progfuncs, arg[0]));
		if (arg[1])
			printf ("%s",PR_GlobalString(progfuncs, arg[1]));
		if (arg[2])
			printf ("%s", PR_GlobalStringNoContents(progfuncs, arg[2]));
	}
	printf ("\n");
}

#ifdef _WIN32
static void VARGS QC_snprintfz (char *dest, size_t size, const char *fmt, ...)
{
	va_list args;
	va_start (args, fmt);
	vsnprintf (dest, size-1, fmt, args);
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
char *QC_ucase(char *str)
{
	static char s[1024];
	strcpy(s, str);
	str = s;

	while(*str)
	{
		if (*str >= 'a' && *str <= 'z')
			*str = *str - 'a' + 'A';
		str++;
	}
	return s;
}

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
			ed = PROG_TO_EDICT(progfuncs, ((eval_t *)&pr_globals[st16[st].a])->edict);
			ptr = (eval_t *)(((int *)edvars(ed)) + ((eval_t *)&pr_globals[st16[st].b])->_int + progfuncs->funcs.fieldadjust);

			cnfd = ED_FindField(progfuncs, "classname");
			if (cnfd)
			{
				string_t *v = (string_t *)((char *)edvars(ed) + cnfd->ofs*4);
				classname = PR_StringToNative(&progfuncs->funcs, *v);
			}
			else
				classname = "";
			if (*classname)
				fdef = ED_ClassFieldAtOfs(progfuncs, ((eval_t *)&pr_globals[st16[st].b])->_int, classname);
			else
				fdef = ED_FieldAtOfs(progfuncs, ((eval_t *)&pr_globals[st16[st].b])->_int);
			if (fdef)
				printf("    %s.%s: %s\n", ent->s_name+progfuncs->funcs.stringtable, fld->s_name+progfuncs->funcs.stringtable, PR_ValueString(progfuncs, fdef->type, ptr, false));
			else
				printf("    %s.%s: BAD FIELD DEF - %#x\n", ent->s_name+progfuncs->funcs.stringtable, fld->s_name+progfuncs->funcs.stringtable, ptr->_int);
		}
	}
}

void PDECL PR_StackTrace (pubprogfuncs_t *ppf, int showlocals)
{
	progfuncs_t *progfuncs = (progfuncs_t *)ppf;
	const mfunction_t	*f;
	int			i;
	int progs;
	int arg;
	int *globalbase;
	progs = -1;

	if (pr_depth == 0)
	{
		printf ("<NO STACK>\n");
		return;
	}

	//point this to the function's locals
	globalbase = (int *)pr_globals + pr_xfunction->parm_start + pr_xfunction->locals;

	pr_stack[pr_depth].f = pr_xfunction;
	pr_stack[pr_depth].s = pr_xstatement;
	for (i=pr_depth ; i>0 ; i--)
	{
		f = pr_stack[i].f;

		if (!f)
		{
			printf ("<NO FUNCTION>\n");
		}
		else
		{
			globalbase -= f->locals;

			if (pr_stack[i].progsnum != progs)
			{
				progs = pr_stack[i].progsnum;

				printf ("<%s>\n", pr_progstate[progs].filename);
			}
			if (!f->s_file)
				printf ("stripped     : %s\n", f->s_name+progfuncs->funcs.stringtable);
			else
			{
				if (pr_progstate[progs].linenums)
					printf ("%12s:%i: %s\n", f->s_file+progfuncs->funcs.stringtable, pr_progstate[progs].linenums[pr_stack[i].s], f->s_name+progfuncs->funcs.stringtable);
				else
					printf ("%12s : %s\n", f->s_file+progfuncs->funcs.stringtable, f->s_name+progfuncs->funcs.stringtable);
			}

			//locals:0 = no locals
			//locals:1 = top only
			//locals:2 = ALL locals.
			if ((i == pr_depth && showlocals == 1) || showlocals >= 2)
			for (arg = 0; arg < f->locals; arg++)
			{
				ddef16_t *local;
				local = ED_GlobalAtOfs16(progfuncs, f->parm_start+arg);
				if (!local)
				{
					//printf("    ofs %i: %f : %i\n", f->parm_start+arg, *(float *)(globalbase - f->locals+arg), *(int *)(globalbase - f->locals+arg) );
				}
				else
				{
					printf("    %s: %s\n", local->s_name+progfuncs->funcs.stringtable, PR_ValueString(progfuncs, local->type, (eval_t*)(globalbase+arg), false));
					if (local->type == ev_vector)
						arg+=2;
				}
			}
			if (i == pr_depth)
			{	//scan for op_address/op_load instructions within the function
				PR_PrintRelevantLocals(progfuncs);
			}

			if (i == pr_depth)
				globalbase = localstack + localstack_used;
		}
	}
}

/*
============
PR_Profile_f

============
*/
/*
void PR_Profile_f (void)
{
	dfunction_t	*f, *best;
	int			max;
	int			num;
	unsigned int			i;

	num = 0;
	do
	{
		max = 0;
		best = NULL;
		for (i=0 ; i<pr_progs->numfunctions ; i++)
		{
			f = &pr_functions[i];
			if (f->profile > max && f->first_statement >=0)
			{
				max = f->profile;
				best = f;
			}
		}
		if (best)
		{
			if (num < 10)
				printf ("%7i %s\n", best->profile, best->s_name);
			num++;
			best->profile = 0;
		}
	} while (best);
}
*/


/*
============
PR_RunError

Aborts the currently executing function
============
*/
void VARGS PR_RunError (pubprogfuncs_t *progfuncs, char *error, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr,error);
	Q_vsnprintf (string,sizeof(string)-1, error,argptr);
	va_end (argptr);

//	{
//		void SV_EndRedirect (void);
//		SV_EndRedirect();
//	}

//	PR_PrintStatement (pr_statements + pr_xstatement);
	PR_StackTrace (progfuncs, true);
	progfuncs->parms->Printf ("\n");

//editbadfile(pr_strings + pr_xfunction->s_file, -1);

//	pr_depth = 0;		// dump the stack so host_error can shutdown functions
//	prinst->exitdepth = 0;

	progfuncs->parms->Abort ("%s", string);
}

pbool PR_RunWarning (pubprogfuncs_t *progfuncs, char *error, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr,error);
	Q_vsnprintf (string,sizeof(string)-1, error,argptr);
	va_end (argptr);

	progfuncs->parms->Printf ("%s, %s\n", string, ((progfuncs->pr_trace)?"ignoring":"enabling trace"));
	PR_StackTrace (progfuncs, false);

	if (progfuncs->pr_trace++ == 0)
		return true;
	return false;
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

	pr_stack[pr_depth].s = pr_xstatement;
	pr_stack[pr_depth].f = pr_xfunction;
	pr_stack[pr_depth].progsnum = progsnum;
	pr_stack[pr_depth].pushed = pr_spushed;
	if (prinst.profiling)
	{
		pr_stack[pr_depth].timestamp = Sys_GetClock();
	}
	pr_depth++;
	if (pr_depth == MAX_STACK_DEPTH)
	{
		pr_depth--;
		PR_StackTrace (&progfuncs->funcs, false);

		printf ("stack overflow on call to %s (depth %i)\n", progfuncs->funcs.stringtable+f->s_name, pr_depth);

		//comment this out if you want the progs to try to continue anyway (could cause infinate loops)
		PR_AbortStack(&progfuncs->funcs);
		externs->Abort("Stack Overflow in %s\n", progfuncs->funcs.stringtable+f->s_name);
		return pr_xstatement;
	}

	localstack_used += pr_spushed;	//make sure the call doesn't hurt pushed pointers

// save off any locals that the new function steps on (to a side place, fromwhere they are restored on exit)
	c = f->locals;
	if (localstack_used + c > LOCALSTACK_SIZE)
	{
		localstack_used -= pr_spushed;
		pr_depth--;
		PR_RunError (&progfuncs->funcs, "PR_ExecuteProgram: locals stack overflow\n");
	}

	for (i=0 ; i < c ; i++)
		localstack[localstack_used+i] = ((int *)pr_globals)[f->parm_start + i];
	localstack_used += c;

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

	if (pr_depth <= 0)
		Sys_Error ("prog stack underflow");

// restore locals from the stack
	c = pr_xfunction->locals;
	localstack_used -= c;
	if (localstack_used < 0)
		PR_RunError (&progfuncs->funcs, "PR_ExecuteProgram: locals stack underflow\n");

	for (i=0 ; i < c ; i++)
		((int *)pr_globals)[pr_xfunction->parm_start + i] = localstack[localstack_used+i];

// up stack
	pr_depth--;

	PR_SwitchProgsParms(progfuncs, pr_stack[pr_depth].progsnum);
	pr_spushed = pr_stack[pr_depth].pushed;

	if (prinst.profiling)
	{
		unsigned long long cycles;
		cycles = Sys_GetClock() - pr_stack[pr_depth].timestamp;
		pr_xfunction->profiletime += cycles;
		pr_xfunction = pr_stack[pr_depth].f;
		if (pr_depth)
			pr_xfunction->profilechildtime += cycles;
	}
	else
		pr_xfunction = pr_stack[pr_depth].f;

	localstack_used -= pr_spushed;
	return pr_stack[pr_depth].s;
}

ddef32_t *ED_FindLocalOrGlobal(progfuncs_t *progfuncs, char *name, eval_t **val)
{
	static ddef32_t def;
	ddef32_t *def32;
	ddef16_t *def16;
	int i;

	if (pr_typecurrent < 0)
		return NULL;

	switch (pr_progstate[pr_typecurrent].structtype)
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
				*val = (eval_t *)&pr_progstate[pr_typecurrent].globals[pr_xfunction->parm_start+i];

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
				*val = (eval_t *)&pr_progstate[pr_typecurrent].globals[pr_xfunction->parm_start+i];

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

	*val = (eval_t *)&pr_progstate[pr_typecurrent].globals[def32->ofs];
	return &def;
}

static char *COM_TrimString(char *str)
{
	int i;
	static char buffer[256];
	while (*str <= ' ' && *str>'\0')
		str++;

	for (i = 0; i < 255; i++)
	{
		if (*str <= ' ')
			break;
		buffer[i] = *str++;
	}
	buffer[i] = '\0';
	return buffer;
}

pbool LocateDebugTerm(progfuncs_t *progfuncs, char *key, eval_t **result, etype_t *rettype, eval_t *store)
{
	ddef32_t *def;
	fdef_t *fdef;
	int fofs;
	eval_t *val = NULL, *fval=NULL;
	char *c, *c2;
	etype_t type = ev_void;
	struct edictrun_s *ed;

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
			c2 = COM_TrimString(c2);
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


		ed = PROG_TO_EDICT(progfuncs, val->_int);
		if (!ed)
			return false;
		if (fofs < 0 || fofs >= max_fields_size)
			return false;
		val = (eval_t *) (((char *)ed->fields) + fofs*4);
	}
	*rettype = type;
	*result = val;
	return true;
}

pbool PDECL PR_SetWatchPoint(pubprogfuncs_t *ppf, char *key)
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
		printf("Unable to evaluate watch term \"%s\"\n", key);
		return false;
	}
	if (val == &fakeval)
	{
		printf("Do you like watching paint dry?\n");
		return false;
	}
	if (type == ev_vector)
	{
		printf("Unable to watch vectors. Watching the x field instead.\n");
		type = ev_float;
	}

	free(prinst.watch_name);
	prinst.watch_name = strdup(key);
	prinst.watch_ptr = val;
	prinst.watch_old = *prinst.watch_ptr;
	prinst.watch_type = type;
	return true;
}

char *PDECL PR_EvaluateDebugString(pubprogfuncs_t *ppf, char *key)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	static char buf[8192];
	fdef_t *fdef;
	eval_t *val;
	char *assignment;
	etype_t type;
	eval_t fakeval;

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
		assignment++;
		while(*assignment == ' ')
			assignment++;
		switch (type&~DEF_SAVEGLOBAL)
		{
		case ev_string:
			*(string_t *)val = PR_StringToProgs(&progfuncs->funcs, ED_NewString (&progfuncs->funcs, assignment, 0, true));
			break;

		case ev_float:
			if (assignment[0] == '0' && (assignment[1] == 'x' || assignment[1] == 'X'))
				*(float*)val = strtoul(assignment, NULL, 0);
			else
				*(float *)val = (float)atof (assignment);
			break;

		case ev_integer:
			*(int *)val = atoi (assignment);
			break;

/*		case ev_vector:
			strcpy (string, assignment);
			v = string;
			w = string;
			for (i=0 ; i<3 ; i++)
			{
				while (*v && *v != ' ')
					v++;
				*v = 0;
				((float *)d)[i] = (float)atof (w);
				w = v = v+1;
			}
			break;
*/
		case ev_entity:
			if (!EDICT_NUM(progfuncs, atoi (assignment)))
				return "(invalid entity)";
			*(int *)val = EDICT_TO_PROG(progfuncs, EDICT_NUM(progfuncs, atoi (assignment)));
			break;

		case ev_field:
			fdef = ED_FindField (progfuncs, assignment);
			if (!fdef)
			{
				size_t l,nl = strlen(assignment);
				strcpy(buf, "Can't find field ");
				l = strlen(buf);
				if (nl > sizeof(buf)-l-2)
					nl = sizeof(buf)-l-2;
				memcpy(buf+l, assignment, nl);
				assignment[l+nl+0] = '\n';
				assignment[l+nl+1] = 0;
				return buf;
			}
			*(int *)val = G_INT(fdef->ofs);
			break;

		case ev_function:
			{
				mfunction_t *func;
				int i;
				int progsnum = -1;
				char *s = assignment;
				if (s[0] && s[1] == ':')
				{
					progsnum = atoi(s);
					s+=2;
				}
				else if (s[0] && s[1] && s[2] == ':')
				{
					progsnum = atoi(s);
					s+=3;
				}

				func = ED_FindFunction (progfuncs, s, &i, progsnum);
				if (!func)
				{
					size_t l,nl = strlen(s);

					assignment[-1] = '=';

					strcpy(buf, "Can't find field ");
					l = strlen(buf);
					if (nl > sizeof(buf)-l-2)
						nl = sizeof(buf)-l-2;
					memcpy(buf+l, assignment, nl);
					assignment[l+nl+0] = '\n';
					assignment[l+nl+1] = 0;
					return buf;
				}
				*(func_t *)val = (func - pr_progstate[i].functions) | (i<<24);
			}
			break;

		default:
			break;

		}
		assignment[-1] = '=';
	}
	QC_snprintfz(buf, sizeof(buf), "%s", PR_ValueString(progfuncs, type, val, true));

	return buf;
}

//int EditorHighlightLine(window_t *wnd, int line);
void SetExecutionToLine(progfuncs_t *progfuncs, int linenum)
{
	int pn = pr_typecurrent;
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

//0 clear. 1 set, 2 toggle, 3 check
int PDECL PR_ToggleBreakpoint(pubprogfuncs_t *ppf, char *filename, int linenum, int flag)	//write alternate route to work by function name.
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	int ret=0;
	unsigned int fl;
	unsigned int i;
	int pn = pr_typecurrent;
	mfunction_t *f;
	int op = 0; //warning about not being initialized before use

	for (pn = 0; (unsigned)pn < maxprogs; pn++)
	{
		if (!pr_progstate || !pr_progstate[pn].progs)
			continue;

		if (linenum)	//linenum is set means to set the breakpoint on a file and line
		{
			if (!pr_progstate[pn].linenums)
				continue;

			//we need to use the function table in order to set breakpoints in the right file.
			for (f = pr_progstate[pn].functions, fl = 0; fl < pr_progstate[pn].progs->numfunctions; f++, fl++)
			{
				if (!stricmp(f->s_file+progfuncs->funcs.stringtable, filename))
				{
					for (i = f->first_statement; i < pr_progstate[pn].progs->numstatements; i++)
					{
						if (pr_progstate[pn].linenums[i] >= linenum)
						{
							fl = pr_progstate[pn].linenums[i];
							for (; ; i++)
							{
								if ((unsigned int)pr_progstate[pn].linenums[i] > fl)
									break;

								switch(pr_progstate[pn].structtype)
								{
								case PST_DEFAULT:
								case PST_QTEST:
									op = ((dstatement16_t*)pr_progstate[pn].statements + i)->op;
									break;
								case PST_KKQWSV:
								case PST_FTE32:
									op = ((dstatement32_t*)pr_progstate[pn].statements + i)->op;
									break;
								default:
									Sys_Error("Bad structtype");
									op = 0;
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
								switch(pr_progstate[pn].structtype)
								{
								case PST_DEFAULT:
								case PST_QTEST:
									((dstatement16_t*)pr_progstate[pn].statements + i)->op = op;
									break;
								case PST_KKQWSV:
								case PST_FTE32:
									((dstatement32_t*)pr_progstate[pn].statements + i)->op = op;
									break;
								default:
									Sys_Error("Bad structtype");
									op = 0;
								}
							}
							goto cont;
						}
					}
				}
			}
		}
		else	//set the breakpoint on the first statement of the function specified.
		{
			for (f = pr_progstate[pn].functions, fl = 0; fl < pr_progstate[pn].progs->numfunctions; f++, fl++)
			{
				if (!strcmp(f->s_name+progfuncs->funcs.stringtable, filename))
				{
					i = f->first_statement;
					switch(pr_progstate[pn].structtype)
					{
					case PST_DEFAULT:
					case PST_QTEST:
						op = ((dstatement16_t*)pr_progstate[pn].statements + i)->op;
						break;
					case PST_KKQWSV:
					case PST_FTE32:
						op = ((dstatement32_t*)pr_progstate[pn].statements + i)->op;
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
					switch(pr_progstate[pn].structtype)
					{
					case PST_DEFAULT:
					case PST_QTEST:
						((dstatement16_t*)pr_progstate[pn].statements + i)->op = op;
						break;
					case PST_KKQWSV:
					case PST_FTE32:
						((dstatement32_t*)pr_progstate[pn].statements + i)->op = op;
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

int ShowStep(progfuncs_t *progfuncs, int statement)
{
//	return statement;
//	texture realcursortex;
static int lastline = 0;
static char *lastfile = 0;

	int pn = pr_typecurrent;
	int i;
	const mfunction_t *f = pr_xfunction;
	pr_xstatement = statement;

	if (!externs->useeditor)
	{
		PR_PrintStatement(progfuncs, statement);
		return statement;
	}

	if (f && externs->useeditor)
	{
		if (pr_progstate[pn].linenums)
		{
			if (lastline == pr_progstate[pn].linenums[statement] && lastfile == f->s_file+progfuncs->funcs.stringtable)
				return statement;	//no info/same line as last time

			lastline = pr_progstate[pn].linenums[statement];
		}
		else
			lastline = -1;
		lastfile = f->s_file+progfuncs->funcs.stringtable;

		lastline = externs->useeditor(&progfuncs->funcs, lastfile, lastline, statement, 0, NULL);
		if (!pr_progstate[pn].linenums)
			return statement;
		if (lastline <= 0)
			return -lastline;

		if (pr_progstate[pn].linenums[statement] != lastline)
		{
			for (i = f->first_statement; ; i++)
			{
				if (lastline == pr_progstate[pn].linenums[i])
				{
					return i;
				}
				else if (lastline <= pr_progstate[pn].linenums[i])
				{
					return statement;
				}
			}
		}
	}
	else if (f)	//annoying.
	{
		if (*(f->s_file+progfuncs->funcs.stringtable))	//if we can't get the filename, then it was stripped, and debugging it like this is useless
			if (externs->useeditor)
				externs->useeditor(&progfuncs->funcs, f->s_file+progfuncs->funcs.stringtable, -1, 0, 0, NULL);
		return statement;
	}


	return statement;
}

#define RUNAWAYCHECK()							\
	if (!--*runaway)								\
	{											\
		pr_xstatement = st-pr_statements;		\
		PR_RunError (&progfuncs->funcs, "runaway loop error\n");\
		PR_StackTrace(&progfuncs->funcs,false);	\
		printf ("runaway loop error\n");		\
		while(pr_depth > prinst.exitdepth)		\
			PR_LeaveFunction(progfuncs);		\
		pr_spushed = 0;							\
		return -1;								\
	}

static int PR_ExecuteCode16 (progfuncs_t *fte_restrict progfuncs, int s, int *fte_restrict runaway)
{
	eval_t	*t, *swtch=NULL;

	int swtchtype = 0; //warning about not being initialized before use
	const dstatement16_t	*fte_restrict st;
	mfunction_t	*fte_restrict newf;
	int		i;
	edictrun_t	*ed;
	eval_t	*ptr;

	float *fte_restrict glob = pr_globals;
	float tmpf;
	int tmpi;

#define OPA ((eval_t *)&glob[st->a])
#define OPB ((eval_t *)&glob[st->b])
#define OPC ((eval_t *)&glob[st->c])

#define INTSIZE 16
	st = &pr_statements16[s];
	while (progfuncs->funcs.pr_trace || prinst.watch_ptr || prinst.profiling)
	{
#ifdef FTE_TARGET_WEB
		//this can generate huge functions, so disable it on systems that can't realiably cope with such things (IE initiates an unwanted denial-of-service attack when pointed our javascript, and firefox prints a warning too)
		pr_xstatement = st-pr_statements;
		PR_RunError (&progfuncs->funcs, "This platform does not support QC debugging.\n");
		PR_StackTrace(&progfuncs->funcs);
		return -1;
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
	eval_t	*t, *swtch=NULL;

	int swtchtype = 0; //warning about not being initialized before use
	const dstatement32_t	*fte_restrict st;
	mfunction_t	*fte_restrict newf;
	int		i;
	edictrun_t	*ed;
	eval_t	*ptr;

	float *fte_restrict glob = pr_globals;
	float tmpf;
	int tmpi;

#define OPA ((eval_t *)&glob[st->a])
#define OPB ((eval_t *)&glob[st->b])
#define OPC ((eval_t *)&glob[st->c])

#define INTSIZE 32
	st = &pr_statements32[s];
	while (progfuncs->funcs.pr_trace || prinst.watch_ptr || prinst.profiling)
	{
#ifdef FTE_TARGET_WEB
		//this can generate huge functions, so disable it on systems that can't realiably cope with such things (IE initiates an unwanted denial-of-service attack when pointed our javascript, and firefox prints a warning too)
		pr_xstatement = st-pr_statements;
		PR_RunError (&progfuncs->funcs, "This platform does not support QC debugging.\n");
		PR_StackTrace(&progfuncs->funcs, false);
		return -1;
#else
		#define DEBUGABLE
		#ifdef SEPARATEINCLUDES
			#include "execloop32d.h"
		#else
			#include "execloop.h"
		#endif
		#undef DEBUGABLE
#endif
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
			printf("Watch point \"%s\" changed by engine from %g to %g.\n", prinst.watch_name, prinst.watch_old._float, prinst.watch_ptr->_float);
			break;
		default:
			printf("Watch point \"%s\" changed by engine from %i to %i.\n", prinst.watch_name, prinst.watch_old._int, prinst.watch_ptr->_int);
			break;
		case ev_function:
		case ev_string:
			printf("Watch point \"%s\" set by engine to %s.\n", prinst.watch_name, PR_ValueString(progfuncs, prinst.watch_type, prinst.watch_ptr, false));
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

	int tempdepth;

	unsigned int newprogs = (fnum & 0xff000000)>>24;

	initial_progs = pr_typecurrent;
	if (newprogs != initial_progs)
	{
		if (newprogs >= maxprogs || !&pr_progstate[newprogs].globals)	//can happen with hexen2...
		{
			printf("PR_ExecuteProgram: tried branching into invalid progs\n");
			return;
		}
		PR_SwitchProgsParms(progfuncs, newprogs);
	}

	if (!(fnum & ~0xff000000) || (signed)(fnum & ~0xff000000) >= pr_progs->numfunctions)
	{
//		if (pr_global_struct->self)
//			ED_Print (PROG_TO_EDICT(pr_global_struct->self));
#ifdef __GNUC__
		printf("PR_ExecuteProgram: NULL function from exe (address %p)\n", __builtin_return_address(0));
#else
		printf("PR_ExecuteProgram: NULL function from exe\n");
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
			i -= externs->numglobalbuiltins;
			if (i > current_progstate->numbuiltins)
			{
				printf ("Bad builtin call number %i (from exe)\n", -f->first_statement);
			//	PR_MoveParms(p, pr_typecurrent);
				PR_SwitchProgs(progfuncs, initial_progs);
				return;
			}
			current_progstate->builtins [i] (&progfuncs->funcs, (struct globalvars_s *)current_progstate->globals);
		}
		PR_SwitchProgsParms(progfuncs, initial_progs);
		return;
	}

	if (progfuncs->funcs.pr_trace)
		progfuncs->funcs.pr_trace--;

// make a stack frame
	prinst.exitdepth = pr_depth;


	s = PR_EnterFunction (progfuncs, f, initial_progs);

	tempdepth = prinst.numtempstringsstack;
	PR_ExecuteCode(progfuncs, s);


	PR_SwitchProgsParms(progfuncs, initial_progs);

	PR_FreeTemps(progfuncs, tempdepth);
	prinst.numtempstringsstack = tempdepth;

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
			((int *)pr_globals)[f->parm_start + l] = localstack[localsoffset+l];	//copy the old value into the globals (so the older functions have the correct locals.
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
	thread->xfunction = pr_xfunction - pr_progstate[pr_typecurrent].functions;
	thread->xprogs = pr_typecurrent;

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
	int tempdepth;

	progsnum_t prnum = thread->xprogs;
	int fnum = thread->xfunction;

	if (localstack_used + thread->lstackused > LOCALSTACK_SIZE)
		PR_RunError(&progfuncs->funcs, "Too many locals on resumtion of QC thread\n");

	if (pr_depth + thread->fstackdepth > MAX_STACK_DEPTH)
		PR_RunError(&progfuncs->funcs, "Too large stack on resumtion of QC thread\n");


	//do progs switching stuff as appropriate. (fteqw only)
	initial_progs = pr_typecurrent;
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
			localstack[localstack_used++] = ((int *)pr_globals)[f->parm_start + l];
			((int *)pr_globals)[f->parm_start + l] = thread->lstack[ls++];
		}

		pr_depth++;
	}

	if (ls != thread->lstackused)
		PR_RunError(&progfuncs->funcs, "Thread stores incorrect locals count\n");


	f = &pr_cp_functions[fnum];

//	thread->lstackused -= f->locals;	//the current function is the odd one out.

	//add on the locals stack
	memcpy(localstack+localstack_used, thread->lstack, sizeof(int)*thread->lstackused);
	localstack_used += thread->lstackused;

	//bung the locals of the current function on the stack.
//	for (i=0 ; i < f->locals ; i++)
//		((int *)pr_globals)[f->parm_start + i] = 0xff00ff00;//thread->lstack[thread->lstackused+i];


//	PR_EnterFunction (progfuncs, f, initial_progs);
	oldf = pr_xfunction;
	pr_xfunction = f;
	s = thread->xstatement;

	tempdepth = prinst.numtempstringsstack;
	PR_ExecuteCode(progfuncs, s);


	PR_SwitchProgsParms(progfuncs, initial_progs);
	PR_FreeTemps(progfuncs, tempdepth);
	prinst.numtempstringsstack = tempdepth;

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

