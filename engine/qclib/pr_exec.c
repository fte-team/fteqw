#define PROGSUSED
#include "progsint.h"
//#include "editor.h"

#define HunkAlloc BADGDFG sdfhhsf FHS


#define Host_Error Sys_Error

// I put the following here to resolve "undefined reference to `__imp__vsnprintf'" with MinGW64 ~ Moodles
#ifdef _WIN32
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
	int		i;
	unsigned int op;
	unsigned int arg[3];

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
	}

#ifndef MINIMAL
	if ( (unsigned)op < OP_NUMOPS)
	{
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

void PR_GenerateStatementString (progfuncs_t *progfuncs, int statementnum, char *out, int outlen)
{
	int		i;
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

#ifndef MINIMAL
	if ( (unsigned)op < OP_NUMOPS)
	{
		QC_snprintfz (out, outlen, "%s", pr_opcodes[op].name);
		outlen -= strlen(out);
		out += strlen(out);
		QC_snprintfz (out, outlen, " ");
		outlen -= 1;
		out += 1;

		QC_snprintfz (out, outlen, "%s ",  pr_opcodes[op].name);
		i = strlen(pr_opcodes[op].name);
		for ( ; i<10 ; i++)
		{
			QC_snprintfz (out, outlen, " ");
			outlen -= 1;
			out += 1;
		}
	}
	else
#endif
	{
		QC_snprintfz (out, outlen, "op%3i ", op);
		outlen -= strlen(out);
		out += strlen(out);
	}

	if (op == OP_IF_F || op == OP_IFNOT_F)
	{
		QC_snprintfz (out, outlen, "%sbranch %i",PR_GlobalString(progfuncs, arg[0]),arg[1]);
		outlen -= strlen(out);
		out += strlen(out);
	}
	else if (op == OP_GOTO)
	{
		QC_snprintfz (out, outlen, "branch %i",arg[0]);
		outlen -= strlen(out);
		out += strlen(out);
	}
	else if ( (unsigned)(op - OP_STORE_F) < 6)
	{
		QC_snprintfz (out, outlen, "%s",PR_GlobalString(progfuncs, arg[0]));
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
			QC_snprintfz (out, outlen, "%s",PR_GlobalString(progfuncs, arg[0]));
			outlen -= strlen(out);
			out += strlen(out);
		}
		if (arg[1])
		{
			QC_snprintfz (out, outlen, "%s",PR_GlobalString(progfuncs, arg[1]));
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

void PR_StackTrace (progfuncs_t *progfuncs)
{
	dfunction_t	*f;
	int			i;
	int progs;

#ifdef STACKTRACE
	int arg;
	int *globalbase;
#endif
	progs = -1;

	if (pr_depth == 0)
	{
		printf ("<NO STACK>\n");
		return;
	}

#ifdef STACKTRACE
	globalbase = (int *)pr_globals + pr_xfunction->parm_start - pr_xfunction->locals;
#endif

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
			if (pr_stack[i].progsnum != progs)
			{
				progs = pr_stack[i].progsnum;

				printf ("<%s>\n", pr_progstate[progs].filename);
			}
			if (!f->s_file)
				printf ("stripped     : %s\n", f->s_name+progfuncs->stringtable);
			else
			{
				if (pr_progstate[progs].linenums)
					printf ("%12s %i : %s\n", f->s_file+progfuncs->stringtable, pr_progstate[progs].linenums[pr_stack[i].s], f->s_name+progfuncs->stringtable);
				else
					printf ("%12s : %s\n", f->s_file+progfuncs->stringtable, f->s_name+progfuncs->stringtable);
			}

#ifdef STACKTRACE

			for (arg = 0; arg < f->locals; arg++)
			{
				ddef16_t *local;
				local = ED_GlobalAtOfs16(progfuncs, f->parm_start+arg);
				if (!local)
				{
					printf("    ofs %i: %f : %i\n", f->parm_start+arg, *(float *)(globalbase - f->locals+arg), *(int *)(globalbase - f->locals+arg) );
				}
				else
				{
					printf("    %s: %s\n", local->s_name+progfuncs->stringtable, PR_ValueString(progfuncs, local->type, (eval_t*)(globalbase - f->locals+arg)));
					if (local->type == ev_vector)
						arg+=2;
				}
			}

			if (i == pr_depth)
				globalbase = localstack + localstack_used;
			else
				globalbase -= f->locals;
#endif
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
void VARGS PR_RunError (progfuncs_t *progfuncs, char *error, ...)
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
	PR_StackTrace (progfuncs);
	printf ("\n");

//editbadfile(pr_strings + pr_xfunction->s_file, -1);

//	pr_depth = 0;		// dump the stack so host_error can shutdown functions
//	prinst->exitdepth = 0;

	Abort ("%s", string);
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
void	PR_AbortStack			(progfuncs_t *progfuncs);
int ASMCALL PR_EnterFunction (progfuncs_t *progfuncs, dfunction_t *f, int progsnum)
{
	int		i, j, c, o;

	pr_stack[pr_depth].s = pr_xstatement;
	pr_stack[pr_depth].f = pr_xfunction;
	pr_stack[pr_depth].progsnum = progsnum;
	pr_stack[pr_depth].pushed = pr_spushed;
	pr_depth++;
	if (pr_depth == MAX_STACK_DEPTH)
	{
		pr_depth--;
		PR_StackTrace (progfuncs);

		printf ("stack overflow on call to %s\n", progfuncs->stringtable+f->s_name);

		//comment this out if you want the progs to try to continue anyway (could cause infinate loops)
		PR_AbortStack(progfuncs);
		Abort("Stack Overflow in %s\n", progfuncs->stringtable+f->s_name);
		return pr_xstatement;
	}

	localstack_used += pr_spushed;	//make sure the call doesn't hurt pushed pointers

// save off any locals that the new function steps on (to a side place, fromwhere they are restored on exit)
	c = f->locals;
	if (localstack_used + c > LOCALSTACK_SIZE)
	{
		localstack_used -= pr_spushed;
		pr_depth--;
		PR_RunError (progfuncs, "PR_ExecuteProgram: locals stack overflow\n");
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
		PR_RunError (progfuncs, "PR_ExecuteProgram: locals stack underflow\n");

	for (i=0 ; i < c ; i++)
		((int *)pr_globals)[pr_xfunction->parm_start + i] = localstack[localstack_used+i];

// up stack
	pr_depth--;
	PR_SwitchProgsParms(progfuncs, pr_stack[pr_depth].progsnum);
	pr_xfunction = pr_stack[pr_depth].f;
	pr_spushed = pr_stack[pr_depth].pushed;

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
			if (!strcmp(def16->s_name+progfuncs->stringtable, name))
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
			if (!strcmp(def32->s_name+progfuncs->stringtable, name))
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

char *COM_TrimString(char *str)
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
	eval_t *val = NULL;
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
		fdef = ED_FindField(progfuncs, COM_TrimString(c2));
		if (c)*c = '.';
		if (!fdef)
			return false;
		ed = PROG_TO_EDICT(progfuncs, val->_int);
		if (!ed)
			return false;
		val = (eval_t *) (((char *)ed->fields) + fdef->ofs*4);
		type = fdef->type;
	}
	*rettype = type;
	*result = val;
	return true;
}

pbool PR_SetWatchPoint(progfuncs_t *progfuncs, char *key)
{
	eval_t *val;
	eval_t fakeval;
	etype_t type;

	if (!key)
	{
		free(prinst->watch_name);
		prinst->watch_name = NULL;
		prinst->watch_ptr = NULL;
		prinst->watch_type = ev_void;
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

	free(prinst->watch_name);
	prinst->watch_name = strdup(key);
	prinst->watch_ptr = val;
	prinst->watch_old = *prinst->watch_ptr;
	prinst->watch_type = type;
	return true;
}

char *EvaluateDebugString(progfuncs_t *progfuncs, char *key)
{
	static char buf[256];
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
			*(string_t *)val = PR_StringToProgs(progfuncs, ED_NewString (progfuncs, assignment, 0));
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
				dfunction_t *func;
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
	strcpy(buf, PR_ValueString(progfuncs, type, val));

	return buf;
}

int debugstatement;
//int EditorHighlightLine(window_t *wnd, int line);
void SetExecutionToLine(progfuncs_t *progfuncs, int linenum)
{
	int pn = pr_typecurrent;
	int snum;
	dfunction_t *f = pr_xfunction;

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
	debugstatement = snum;
//	EditorHighlightLine(editwnd, pr_progstate[pn].linenums[snum]);
}

//0 clear. 1 set, 2 toggle, 3 check
int PR_ToggleBreakpoint(progfuncs_t *progfuncs, char *filename, int linenum, int flag)	//write alternate route to work by function name.
{
	int ret=0;
	unsigned int fl;
	unsigned int i;
	int pn = pr_typecurrent;
	dfunction_t *f;
	int op = 0; //warning about not being initialized before use

	for (pn = 0; (unsigned)pn < maxprogs; pn++)
	{
		if (!pr_progstate || !pr_progstate[pn].progs)
			continue;

		if (linenum)	//linenum is set means to set the breakpoint on a file and line
		{
			if (!pr_progstate[pn].linenums)
				continue;

			for (f = pr_progstate[pn].functions, fl = 0; fl < pr_progstate[pn].progs->numfunctions; f++, fl++)
			{
				if (!stricmp(f->s_file+progfuncs->stringtable, filename))
				{
					for (i = f->first_statement; ; i++)
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
				if (!strcmp(f->s_name+progfuncs->stringtable, filename))
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
	dfunction_t *f = pr_xfunction;

	if (!externs->useeditor)
	{
		PR_PrintStatement(progfuncs, statement);
		return statement;
	}

	if (f && externs->useeditor)
	{
		if (pr_progstate[pn].linenums)
		{
			if (lastline == pr_progstate[pn].linenums[statement] && lastfile == f->s_file+progfuncs->stringtable)
				return statement;	//no info/same line as last time

			lastline = pr_progstate[pn].linenums[statement];
		}
		else
			lastline = -1;
		lastfile = f->s_file+progfuncs->stringtable;

		lastline = externs->useeditor(progfuncs, lastfile, lastline, statement, 0, NULL);
		if (lastline < 0)
			return -lastline;
		if (!pr_progstate[pn].linenums)
			return statement;

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
		if (*(f->s_file+progfuncs->stringtable))	//if we can't get the filename, then it was stripped, and debugging it like this is useless
			if (externs->useeditor)
				externs->useeditor(progfuncs, f->s_file+progfuncs->stringtable, -1, 0, 0, NULL);
		return statement;
	}


	return statement;
}

//DMW: all pointer functions are modified to be absoloute pointers from NULL not sv_edicts
/*
====================
PR_ExecuteProgram
====================
*/
void PR_ExecuteCode (progfuncs_t *progfuncs, int s)
{
	eval_t	*t, *swtch=NULL;

	int swtchtype = 0; //warning about not being initialized before use
	dstatement16_t	*st16;
	dstatement32_t	*st32;
	dfunction_t	*newf;
	int		runaway;
	int		i;
	edictrun_t	*ed;
	eval_t	*ptr;

	float *glob;

	int fnum;

	if (prinst->watch_ptr && prinst->watch_ptr->_int != prinst->watch_old._int)
	{
		switch(prinst->watch_type)
		{
		case ev_float:
			printf("Watch point \"%s\" changed by engine from %g to %g.\n", prinst->watch_name, prinst->watch_old._float, prinst->watch_ptr->_float);
			break;
		default:
			printf("Watch point \"%s\" changed by engine from %i to %i.\n", prinst->watch_name, prinst->watch_old._int, prinst->watch_ptr->_int);
			break;
		case ev_function:
		case ev_string:
			printf("Watch point \"%s\" set by engine to %s.\n", prinst->watch_name, PR_ValueString(progfuncs, prinst->watch_type, prinst->watch_ptr));
			break;
		}
		prinst->watch_old = *prinst->watch_ptr;

		//we can't dump stack or anything, as we don't really know the stack frame that it happened in.

		//stop watching
		prinst->watch_ptr = NULL;
	}

	prinst->continuestatement = -1;
#ifdef QCJIT
	if (current_progstate->jit)
	{
		PR_EnterJIT(progfuncs, current_progstate->jit, s);
		return;
	}
#endif
	fnum = pr_xfunction - pr_functions;

	runaway = 100000000;

#define PRBOUNDSCHECK
#define RUNAWAYCHECK()							\
	if (!--runaway)								\
	{											\
		pr_xstatement = st-pr_statements;		\
		PR_StackTrace(progfuncs);				\
		printf ("runaway loop error\n");		\
		while(pr_depth > prinst->exitdepth)		\
			PR_LeaveFunction(progfuncs);		\
		pr_spushed = 0;							\
		return;									\
	}

#define OPA ((eval_t *)&glob[st->a])
#define OPB ((eval_t *)&glob[st->b])
#define OPC ((eval_t *)&glob[st->c])

restart:	//jumped to when the progs might have changed.
	glob = pr_globals;
	switch (current_progstate->structtype)
	{
	case PST_DEFAULT:
	case PST_QTEST:
#define INTSIZE 16
		st16 = &pr_statements16[s];
		while (pr_trace || prinst->watch_ptr)
		{
			#define DEBUGABLE
			#ifdef SEPARATEINCLUDES
				#include "execloop16d.h"
			#else
				#include "execloop.h"
			#endif
			#undef DEBUGABLE
		}

		while(1)
		{
			#include "execloop.h"
		}
#undef INTSIZE
		Sys_Error("PR_ExecuteProgram - should be unreachable");
		break;
	case PST_KKQWSV:
	case PST_FTE32:
#define INTSIZE 32
		st32 = &pr_statements32[s];
		while (pr_trace || prinst->watch_ptr)
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
		Sys_Error("PR_ExecuteProgram - should be unreachable");
		break;
	default:
		Sys_Error("PR_ExecuteProgram - bad structtype");
	}
}


void PR_ExecuteProgram (progfuncs_t *progfuncs, func_t fnum)
{
	dfunction_t	*f;
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
		printf("PR_ExecuteProgram: NULL function from exe\n");
//		Host_Error ("PR_ExecuteProgram: NULL function from exe");

//		PR_MoveParms(0, pr_typecurrent);
		PR_SwitchProgs(progfuncs, initial_progs);
		return;
	}

	oldexitdepth = prinst->exitdepth;

	f = &pr_functions[fnum & ~0xff000000];

	if (f->first_statement < 0)
	{	// negative statements are built in functions
		i = -f->first_statement;

		if (i < externs->numglobalbuiltins)
			(*externs->globalbuiltins[i]) (progfuncs, (struct globalvars_s *)current_progstate->globals);
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
			current_progstate->builtins [i] (progfuncs, (struct globalvars_s *)current_progstate->globals);
		}
		PR_SwitchProgsParms(progfuncs, initial_progs);
		return;
	}

	if (pr_trace)
		pr_trace--;

// make a stack frame
	prinst->exitdepth = pr_depth;


	s = PR_EnterFunction (progfuncs, f, initial_progs);

	tempdepth = prinst->numtempstringsstack;
	PR_ExecuteCode(progfuncs, s);


	PR_SwitchProgsParms(progfuncs, initial_progs);

	PR_FreeTemps(progfuncs, tempdepth);
	prinst->numtempstringsstack = tempdepth;

	prinst->exitdepth = oldexitdepth;
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

struct qcthread_s *PR_ForkStack(progfuncs_t *progfuncs)
{	//QC code can call builtins that call qc code.
	//to get around the problems of restoring the builtins we simply don't save the thread over the builtin.
	int i, l;
	int ed = prinst->exitdepth;
	int localsoffset, baselocalsoffset;
	qcthread_t *thread = memalloc(sizeof(qcthread_t));
	dfunction_t *f;

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

void PR_ResumeThread (progfuncs_t *progfuncs, struct qcthread_s *thread)
{
	dfunction_t	*f, *oldf;
	int		i,l,ls;
	progsnum_t initial_progs;
	int		oldexitdepth;

	int s;
	int tempdepth;

	progsnum_t prnum = thread->xprogs;
	int fnum = thread->xfunction;

	if (localstack_used + thread->lstackused > LOCALSTACK_SIZE)
		PR_RunError(progfuncs, "Too many locals on resumtion of QC thread\n");

	if (pr_depth + thread->fstackdepth > MAX_STACK_DEPTH)
		PR_RunError(progfuncs, "Too large stack on resumtion of QC thread\n");


	//do progs switching stuff as appropriate. (fteqw only)
	initial_progs = pr_typecurrent;
	PR_SwitchProgsParms(progfuncs, prnum);


	oldexitdepth = prinst->exitdepth;
	prinst->exitdepth = pr_depth;

	ls = 0;
	//add on the callstack.
	for (i = 0; i < thread->fstackdepth; i++)
	{
		if (pr_depth == prinst->exitdepth)
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
			f = &pr_functions[fnum];
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
		PR_RunError(progfuncs, "Thread stores incorrect locals count\n");


	f = &pr_functions[fnum];

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

	tempdepth = prinst->numtempstringsstack;
	PR_ExecuteCode(progfuncs, s);


	PR_SwitchProgsParms(progfuncs, initial_progs);
	PR_FreeTemps(progfuncs, tempdepth);
	prinst->numtempstringsstack = tempdepth;

	prinst->exitdepth = oldexitdepth;
	pr_xfunction = oldf;
}

void	PR_AbortStack			(progfuncs_t *progfuncs)
{
	while(pr_depth > prinst->exitdepth+1)
		PR_LeaveFunction(progfuncs);
	prinst->continuestatement = 0;
}

