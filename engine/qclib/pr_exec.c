#define PROGSUSED
#include "progsint.h"
//#include "editor.h"

#define HunkAlloc BADGDFG sdfhhsf FHS


#define Host_Error Sys_Error


//=============================================================================

/*
=================
PR_PrintStatement
=================
*/
void PR_PrintStatement (progfuncs_t *progfuncs, dstatement16_t *s)
{
	int		i;
printf("PR_PrintStatement is unsupported\n");
return;
	if ( (unsigned)s->op < OP_NUMOPS)
	{
		printf ("%s ",  pr_opcodes[s->op].name);
		i = strlen(pr_opcodes[s->op].name);
		for ( ; i<10 ; i++)
			printf (" ");
	}
		
	if (s->op == OP_IF || s->op == OP_IFNOT)
		printf ("%sbranch %i",PR_GlobalString(progfuncs, s->a),s->b);
	else if (s->op == OP_GOTO)
	{
		printf ("branch %i",s->a);
	}
	else if ( (unsigned)(s->op - OP_STORE_F) < 6)
	{
		printf ("%s",PR_GlobalString(progfuncs, s->a));
		printf ("%s", PR_GlobalStringNoContents(progfuncs, s->b));
	}
	else
	{
		if (s->a)
			printf ("%s",PR_GlobalString(progfuncs, s->a));
		if (s->b)
			printf ("%s",PR_GlobalString(progfuncs, s->b));
		if (s->c)
			printf ("%s", PR_GlobalStringNoContents(progfuncs, s->c));
	}
	printf ("\n");
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
			if (!*f->s_file)
				printf ("stripped     : %s\n", f->s_name);
			else
				printf ("%12s : %s\n", f->s_file, f->s_name);

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
					printf("    %s: %s\n", local->s_name, PR_ValueString(progfuncs, local->type, (eval_t*)(globalbase - f->locals+arg)));
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

	{
		void SV_EndRedirect (void);
		SV_EndRedirect();
	}

//	PR_PrintStatement (pr_statements + pr_xstatement);
	PR_StackTrace (progfuncs);
	printf ("\n");

//editbadfile(pr_strings + pr_xfunction->s_file, -1);
	
	pr_depth = 0;		// dump the stack so host_error can shutdown functions	
	prinst->exitdepth = 0;

	Abort (string);
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
int PR_EnterFunction (progfuncs_t *progfuncs, dfunction_t *f, int progsnum)
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

		printf ("stack overflow on call to %s\n", f->s_name);

		//comment this out if you want the progs to try to continue anyway (could cause infinate loops)
		Abort("Stack Overflow\n");

		PR_AbortStack(progfuncs);
		return pr_xstatement;
	}

	localstack_used += pr_spushed;	//make sure the call doesn't hurt pushed pointers

// save off any locals that the new function steps on (to a side place, fromwhere they are restored on exit)
	c = f->locals;
	if (localstack_used + c > LOCALSTACK_SIZE)
		PR_RunError (progfuncs, "PR_ExecuteProgram: locals stack overflow\n");

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
int PR_LeaveFunction (progfuncs_t *progfuncs)
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
	PR_MoveParms(progfuncs, pr_stack[pr_depth].progsnum, pr_typecurrent);
	PR_SwitchProgs(progfuncs, pr_stack[pr_depth].progsnum);
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

	switch (pr_progstate[pr_typecurrent].intsize)
	{
	case 16:
	case 24:
		//this gets parms fine, but not locals
		if (pr_xfunction)
		for (i = 0; i < pr_xfunction->numparms; i++)
		{
			def16 = ED_GlobalAtOfs16(progfuncs, pr_xfunction->parm_start+i);
			if (!def16)
				continue;
			if (!strcmp(def16->s_name, name))
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
	case 32:
		//this gets parms fine, but not locals
		if (pr_xfunction)
		for (i = 0; i < pr_xfunction->numparms; i++)
		{
			def32 = ED_GlobalAtOfs32(progfuncs, pr_xfunction->parm_start+i);
			if (!def32)
				continue;
			if (!strcmp(def32->s_name, name))
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
		Sys_Error("Bad int size in ED_FindLocalOrGlobal");
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

char *EvaluateDebugString(progfuncs_t *progfuncs, char *key)
{
	static char buf[256];
	char *c, *c2;
	ddef32_t *def;
	fdef_t *fdef;
	eval_t *val;
	char *assignment;
	int type;

	assignment = strchr(key, '=');
	if (assignment)
		*assignment = '\0';

	c = strchr(key, '.');
	if (c) *c = '\0';
	def = ED_FindLocalOrGlobal(progfuncs, key, &val);	
	if (c) *c = '.';
	if (!def)
	{		
		return "(Bad string)";
	}	
		//go through ent vars

	c = strchr(key, '.');	
	while(c)
	{
		c2 = c+1;
		c = strchr(c2, '.');
		type = def->type &~DEF_SAVEGLOBAL;
		if (current_progstate->types)
			type = current_progstate->types[type].type;
		if (type != ev_entity)
			return "'.' without entity";
		if (c)*c = '\0';
		fdef = ED_FindField(progfuncs, COM_TrimString(c2));
		if (c)*c = '.';
		if (!fdef)
			return "(Bad string)";
		val = (eval_t *) (((char *)PROG_TO_EDICT(val->_int) + externs->edictsize) + fdef->ofs*4);		
		def->type = fdef->type;
	}
	
	if (assignment)
	{
		assignment++;
		switch (def->type&~DEF_SAVEGLOBAL)
		{
		case ev_string:
			*(string_t *)val = ED_NewString (progfuncs, assignment)-progfuncs->stringtable;
			break;
			
		case ev_float:
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
			*(int *)val = EDICT_TO_PROG(EDICT_NUM(progfuncs, atoi (assignment)));
			break;

		case ev_field:
			fdef = ED_FindField (progfuncs, assignment);
			if (!fdef)
			{
				sprintf(buf, "Can't find field %s\n", assignment);
				return buf;
			}
			*(int *)val = G_INT(fdef->ofs);
			break;
/*
		case ev_function:
			if (s[1]==':'&&s[2]=='\0')
			{
				*(func_t *)val = 0;
				return true;
			}
			func = ED_FindFunction (assignment, &i, -1);
			if (!func)
			{
				printf ("Can't find function %s\n", assignment);
				return false;
			}
			*(func_t *)val = (func - pr_progstate[i].functions) | (i<<24);
			break;
*/
		default:
			break;

		}
	}
	strcpy(buf, PR_ValueString(progfuncs, def->type, val));

	return buf;
}

int debugstatement;
//int EditorHighlightLine(window_t *wnd, int line);
void SetExecutionToLine(progfuncs_t *progfuncs, int linenum)
{
	int pn = pr_typecurrent;
	int snum;
	dfunction_t *f = pr_xfunction;

	switch(current_progstate->intsize)
	{
	case 16:
		for (snum = f->first_statement; pr_progstate[pn].linenums[snum] < linenum; snum++) 
		{
			if (pr_statements16[snum].op == OP_DONE)
				return;
		}
		break;
	case 24:
	case 32:
		for (snum = f->first_statement; pr_progstate[pn].linenums[snum] < linenum; snum++) 
		{
			if (pr_statements32[snum].op == OP_DONE)
				return;
		}
		break;
	default:
		Sys_Error("Bad intsize");
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
	int op;

	for (pn = 0; pn < maxprogs; pn++)
	{
		if (!pr_progstate || !pr_progstate[pn].progs)
			continue;

		if (linenum)	//linenum is set means to set the breakpoint on a file and line
		{
			if (!pr_progstate[pn].linenums)
				continue;

			for (f = pr_progstate[pn].functions, fl = 0; fl < pr_progstate[pn].progs->numfunctions; f++, fl++)
			{
				if (!stricmp(f->s_file, filename))
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

								switch(pr_progstate[pn].intsize)
								{
								case 16:
									op = ((dstatement16_t*)pr_progstate[pn].statements + i)->op;
									break;
								case 24:
								case 32:
									op = ((dstatement32_t*)pr_progstate[pn].statements + i)->op;
									break;
								default:
									Sys_Error("Bad intsize");
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
								switch(pr_progstate[pn].intsize)
								{
								case 16:
									((dstatement16_t*)pr_progstate[pn].statements + i)->op = op;
									break;
								case 24:
								case 32:
									((dstatement32_t*)pr_progstate[pn].statements + i)->op = op;
									break;
								default:
									Sys_Error("Bad intsize");
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
				if (!strcmp(f->s_name, filename))
				{
					i = f->first_statement;
					switch(pr_progstate[pn].intsize)
					{
					case 16:
						op = ((dstatement16_t*)pr_progstate[pn].statements + i)->op;
						break;
					case 24:
					case 32:
						op = ((dstatement32_t*)pr_progstate[pn].statements + i)->op;
						break;
					default:
						Sys_Error("Bad intsize");
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
					switch(pr_progstate[pn].intsize)
					{
					case 16:
						((dstatement16_t*)pr_progstate[pn].statements + i)->op = op;
						break;
					case 24:
					case 32:
						((dstatement32_t*)pr_progstate[pn].statements + i)->op = op;
						break;
					default:
						Sys_Error("Bad intsize");
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
static char *lastfile = NULL;

	int pn = pr_typecurrent;
	int i;
	dfunction_t *f = pr_xfunction;	

	if (f && pr_progstate[pn].linenums && externs->useeditor)
	{
		if (lastline == pr_progstate[pn].linenums[statement] && lastfile == f->s_file)
			return statement;	//no info/same line as last time

		lastline = pr_progstate[pn].linenums[statement];
		lastfile = f->s_file;

		lastline = externs->useeditor(lastfile, lastline, 0, NULL);

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
		if (externs->useeditor)
			externs->useeditor(f->s_file, -1, 0, &f->s_name);
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

	int swtchtype;
	dstatement16_t	*st16;
	dstatement32_t	*st32;
	dfunction_t	*newf;
	int		runaway;
	int		i;
	int p;
	edictrun_t	*ed;
	eval_t	*ptr;

	float *glob;

	int fnum = pr_xfunction - pr_functions;

	runaway = 1000000;

	prinst->continuestatement = -1;

#define PRBOUNDSCHECK
#define RUNAWAYCHECK()							\
	if (!--runaway)								\
	{											\
		pr_xstatement = st-pr_statements;		\
		PR_StackTrace(progfuncs);				\
		printf ("runaway loop error");			\
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
	switch (current_progstate->intsize)
	{
	case 16:
#define INTSIZE 16
		st16 = &pr_statements16[s];
		while (pr_trace)
		{
			#define DEBUGABLE
			#include "execloop16d.h"
			#undef DEBUGABLE
		}
		
		while(1)
		{
			#include "execloop.h"
		}	
#undef INTSIZE
		Sys_Error("PR_ExecuteProgram - should be unreachable");
		break;
	case 24:
	case 32:
#define INTSIZE 32
		st32 = &pr_statements32[s];
		while (pr_trace)
		{
			#define DEBUGABLE
			#include "execloop32d.h"
			#undef DEBUGABLE
		}
		
		while(1)
		{
			#include "execloop32.h"
		}
#undef INTSIZE	
		Sys_Error("PR_ExecuteProgram - should be unreachable");
		break;
	default:
		Sys_Error("PR_ExecuteProgram - bad intsize");
	}
}


void PR_ExecuteProgram (progfuncs_t *progfuncs, func_t fnum)
{
	dfunction_t	*f;
	int		i;
	progsnum_t initial_progs;
	int		oldexitdepth;

	int s;

	int newprogs = (fnum & 0xff000000)>>24;

	initial_progs = pr_typecurrent;
	if (newprogs != initial_progs)
	{
		if (newprogs >= maxprogs || !&pr_progstate[newprogs].globals)	//can happen with hexen2...
		{
			printf("PR_ExecuteProgram: tried branching into invalid progs\n");
			return;
		}
		PR_MoveParms(progfuncs, newprogs, pr_typecurrent);
		PR_SwitchProgs(progfuncs, newprogs);
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
		PR_MoveParms(progfuncs, initial_progs, pr_typecurrent);
		PR_SwitchProgs(progfuncs, initial_progs);
		return;
	}

	if (pr_trace)
		pr_trace--;

// make a stack frame
	prinst->exitdepth = pr_depth;

	s = PR_EnterFunction (progfuncs, f, initial_progs);

	PR_ExecuteCode(progfuncs, s);


	PR_MoveParms(progfuncs, initial_progs, pr_typecurrent);
	PR_SwitchProgs(progfuncs, initial_progs);

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

	progsnum_t prnum = thread->xprogs;
	int fnum = thread->xfunction;

	if (localstack_used + thread->lstackused > LOCALSTACK_SIZE)
		PR_RunError(progfuncs, "Too many locals on resumtion of QC thread\n");

	if (pr_depth + thread->fstackdepth > MAX_STACK_DEPTH)
		PR_RunError(progfuncs, "Too large stack on resumtion of QC thread\n");


	//do progs switching stuff as appropriate. (fteqw only)
	initial_progs = pr_typecurrent;
	PR_MoveParms(progfuncs, prnum, pr_typecurrent);
	PR_SwitchProgs(progfuncs, prnum);


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

	PR_ExecuteCode(progfuncs, s);


	PR_MoveParms(progfuncs, initial_progs, pr_typecurrent);
	PR_SwitchProgs(progfuncs, initial_progs);

	prinst->exitdepth = oldexitdepth;
	pr_xfunction = oldf;
}

void	PR_AbortStack			(progfuncs_t *progfuncs)
{
	while(pr_depth > prinst->exitdepth+1)
		PR_LeaveFunction(progfuncs);
	prinst->continuestatement = 0;
}

