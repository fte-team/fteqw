#ifndef MINIMAL

#include "qcc.h"
void QCC_PR_ParseAsm(void);

#define MEMBERFIELDNAME "__m%s"

#define STRCMP(s1,s2) (((*s1)!=(*s2)) || strcmp(s1+1,s2+1))	//saves about 2-6 out of 120 - expansion of idea from fastqcc
#define STRNCMP(s1,s2,l) (((*s1)!=(*s2)) || strncmp(s1+1,s2+1,l))	//pathetic saving here.

extern char *compilingfile;

int conditional;

pbool keyword_var;
pbool keyword_thinktime;
pbool keyword_switch;
pbool keyword_for;
pbool keyword_case;
pbool keyword_default;
pbool keyword_do;
pbool keyword_asm;
pbool keyword_goto;
pbool keyword_break;
pbool keyword_continue;
pbool keyword_state;
pbool keyword_string;
pbool keyword_float;
pbool keyword_entity;
pbool keyword_vector;
pbool keyword_integer;
pbool keyword_int;
pbool keyword_const;
pbool keyword_class;

pbool keywords_coexist;		//don't disable a keyword simply because a var was made with the same name.
pbool output_parms;			//emit some PARMX fields. confuses decompilers.
pbool autoprototype;			//take two passes over the source code. First time round doesn't enter and functions or initialise variables.
pbool pr_subscopedlocals;	//causes locals to be valid ONLY within thier statement block. (they simply can't be referenced by name outside of it)
pbool flag_ifstring;

pbool opt_overlaptemps;		//reduce numpr_globals by reuse of temps. When they are not needed they are freed for reuse. The way this is implemented is better than frikqcc's. (This is the single most important optimisation)
pbool opt_assignments;		//STORE_F isn't used if an operation wrote to a temp.
pbool opt_shortenifnots;		//if(!var) is made an IF rather than NOT IFNOT
pbool opt_noduplicatestrings;	//brute force string check. time consuming but more effective than the equivelent in frikqcc.
pbool opt_constantarithmatic;	//3*5 appears as 15 instead of the extra statement.
pbool opt_nonvec_parms;			//store_f instead of store_v on function calls, where possible.
pbool opt_constant_names;		//take out the defs and name strings of constants.
pbool opt_constant_names_strings;//removes the defs of strings too. plays havok with multiprogs.
pbool opt_precache_file;			//remove the call, the parameters, everything.
pbool opt_filenames;				//strip filenames. hinders older decompilers.
pbool opt_unreferenced;			//strip defs that are not referenced.
pbool opt_function_names;		//strip out the names of builtin functions.
pbool opt_locals;				//strip out the names of locals and immediates.
pbool opt_dupconstdefs;			//float X = 5; and float Y = 5; occupy the same global with this.
pbool opt_return_only;			//RETURN; DONE; at the end of a function strips out the done statement if there is no way to get to it.
pbool opt_compound_jumps;		//jumps to jump statements jump to the final point.
pbool opt_stripfunctions;		//if a functions is only ever called directly or by exe, don't emit the def.
pbool opt_locals_marshalling;	//make the local vars of all functions occupy the same globals.
pbool opt_logicops;				//don't make conditions enter functions if the return value will be discarded due to a previous value. (C style if statements)
//bool opt_comexprremoval;

//these are the results of the opt_. The values are printed out when compilation is compleate, showing effectivness.
int optres_shortenifnots;
int optres_assignments;
int optres_overlaptemps;
int optres_noduplicatestrings;
int optres_constantarithmatic;
int optres_nonvec_parms;
int optres_constant_names;
int optres_constant_names_strings;
int optres_precache_file;
int optres_filenames;
int optres_unreferenced;
int optres_function_names;
int optres_locals;
int optres_dupconstdefs;
int optres_return_only;
int optres_compound_jumps;
//int optres_comexprremoval;
int optres_stripfunctions;
int optres_locals_marshalling;
int optres_logicops;

int optres_test1;
int optres_test2;


QCC_def_t *QCC_PR_DummyDef(QCC_type_t *type, char *name, QCC_def_t *scope, int arraysize, unsigned int ofs, int referable);
QCC_type_t *QCC_PR_NewType (char *name, int basictype);
QCC_type_t *QCC_PR_FindType (QCC_type_t *type);
QCC_type_t *QCC_PR_PointerType (QCC_type_t *pointsto);

void QCC_PR_ParseState (void);
pbool simplestore;

QCC_pr_info_t	pr;
//QCC_def_t		**pr_global_defs/*[MAX_REGS]*/;	// to find def for a global variable

//keeps track of how many funcs are called while parsing a statement
//int qcc_functioncalled;

//========================================

QCC_def_t		*pr_scope;		// the function being parsed, or NULL
QCC_type_t		*pr_classtype;
pbool	pr_dumpasm;
QCC_string_t	s_file, s_file2;			// filename for function definition

unsigned int			locals_start;		// for tracking local variables vs temps
unsigned int			locals_end;		// for tracking local variables vs temps

jmp_buf		pr_parse_abort;		// longjump with this on parse error

void QCC_PR_ParseDefs (char *classname);

pbool qcc_usefulstatement;

int max_breaks;
int max_continues;
int max_cases;
int num_continues;
int num_breaks;
int num_cases;
int *pr_breaks;
int *pr_continues;
int *pr_cases;
QCC_def_t **pr_casesdef;
QCC_def_t **pr_casesdef2;

typedef struct {
	int statementno;
	int lineno;
	char name[256];
} gotooperator_t;

int max_labels;
int max_gotos;
gotooperator_t *pr_labels;
gotooperator_t *pr_gotos;
int num_gotos;
int num_labels;

QCC_def_t *extra_parms[MAX_EXTRA_PARMS];

#define ASSOC_RIGHT_RESULT ASSOC_RIGHT

//========================================

//FIXME: modifiy list so most common GROUPS are first
//use look up table for value of first char and sort by first char and most common...?

//if true, effectivly {b=a; return a;}
QCC_opcode_t pr_opcodes[] =
{
 {6, "<DONE>", "DONE", -1, ASSOC_LEFT,			&type_void, &type_void, &type_void},

 {6, "*", "MUL_F",			3, ASSOC_LEFT,				&type_float, &type_float, &type_float},
 {6, "*", "MUL_V",			3, ASSOC_LEFT,				&type_vector, &type_vector, &type_float},
 {6, "*", "MUL_FV",			3, ASSOC_LEFT,				&type_float, &type_vector, &type_vector},
 {6, "*", "MUL_VF",			3, ASSOC_LEFT,				&type_vector, &type_float, &type_vector},

 {6, "/", "DIV_F",			3, ASSOC_LEFT,				&type_float, &type_float, &type_float},

 {6, "+", "ADD_F",			4, ASSOC_LEFT,				&type_float, &type_float, &type_float},
 {6, "+", "ADD_V",			4, ASSOC_LEFT,				&type_vector, &type_vector, &type_vector},

 {6, "-", "SUB_F",			4, ASSOC_LEFT,				&type_float, &type_float, &type_float},
 {6, "-", "SUB_V",			4, ASSOC_LEFT,				&type_vector, &type_vector, &type_vector},

 {6, "==", "EQ_F",			5, ASSOC_LEFT,				&type_float, &type_float, &type_float},
 {6, "==", "EQ_V",			5, ASSOC_LEFT,				&type_vector, &type_vector, &type_float},
 {6, "==", "EQ_S",			5, ASSOC_LEFT,				&type_string, &type_string, &type_float},
 {6, "==", "EQ_E",			5, ASSOC_LEFT,				&type_entity, &type_entity, &type_float},
 {6, "==", "EQ_FNC",		5, ASSOC_LEFT,				&type_function, &type_function, &type_float},
 
 {6, "!=", "NE_F",			5, ASSOC_LEFT,				&type_float, &type_float, &type_float},
 {6, "!=", "NE_V",			5, ASSOC_LEFT,				&type_vector, &type_vector, &type_float},
 {6, "!=", "NE_S",			5, ASSOC_LEFT,				&type_string, &type_string, &type_float},
 {6, "!=", "NE_E",			5, ASSOC_LEFT,				&type_entity, &type_entity, &type_float},
 {6, "!=", "NE_FNC",		5, ASSOC_LEFT,				&type_function, &type_function, &type_float},
 
 {6, "<=", "LE",			5, ASSOC_LEFT,					&type_float, &type_float, &type_float},
 {6, ">=", "GE",			5, ASSOC_LEFT,					&type_float, &type_float, &type_float},
 {6, "<", "LT",				5, ASSOC_LEFT,					&type_float, &type_float, &type_float},
 {6, ">", "GT",				5, ASSOC_LEFT,					&type_float, &type_float, &type_float},

 {6, ".", "INDIRECT_F",		1, ASSOC_LEFT,			&type_entity, &type_field, &type_float},
 {6, ".", "INDIRECT_V",		1, ASSOC_LEFT,			&type_entity, &type_field, &type_vector},
 {6, ".", "INDIRECT_S",		1, ASSOC_LEFT,			&type_entity, &type_field, &type_string},
 {6, ".", "INDIRECT_E",		1, ASSOC_LEFT,			&type_entity, &type_field, &type_entity},
 {6, ".", "INDIRECT_FI",	1, ASSOC_LEFT,			&type_entity, &type_field, &type_field},
 {6, ".", "INDIRECT_FU",	1, ASSOC_LEFT,			&type_entity, &type_field, &type_function},

 {6, ".", "ADDRESS",		1, ASSOC_LEFT,				&type_entity, &type_field, &type_pointer},

 {6, "=", "STORE_F",		6, ASSOC_RIGHT,				&type_float, &type_float, &type_float},
 {6, "=", "STORE_V",		6, ASSOC_RIGHT,				&type_vector, &type_vector, &type_vector},
 {6, "=", "STORE_S",		6, ASSOC_RIGHT,				&type_string, &type_string, &type_string},
 {6, "=", "STORE_ENT",		6, ASSOC_RIGHT,				&type_entity, &type_entity, &type_entity},
 {6, "=", "STORE_FLD",		6, ASSOC_RIGHT,				&type_field, &type_field, &type_field},
 {6, "=", "STORE_FNC",		6, ASSOC_RIGHT,				&type_function, &type_function, &type_function},

 {6, "=", "STOREP_F",		6, ASSOC_RIGHT,				&type_pointer, &type_float, &type_float},
 {6, "=", "STOREP_V",		6, ASSOC_RIGHT,				&type_pointer, &type_vector, &type_vector},
 {6, "=", "STOREP_S",		6, ASSOC_RIGHT,				&type_pointer, &type_string, &type_string},
 {6, "=", "STOREP_ENT",		6, ASSOC_RIGHT,			&type_pointer, &type_entity, &type_entity},
 {6, "=", "STOREP_FLD",		6, ASSOC_RIGHT,			&type_pointer, &type_field, &type_field},
 {6, "=", "STOREP_FNC",		6, ASSOC_RIGHT,			&type_pointer, &type_function, &type_function},

 {6, "<RETURN>", "RETURN",	-1, ASSOC_LEFT,		&type_float, &type_void, &type_void},
  
 {6, "!", "NOT_F",			-1, ASSOC_LEFT,				&type_float, &type_void, &type_float},
 {6, "!", "NOT_V",			-1, ASSOC_LEFT,				&type_vector, &type_void, &type_float},
 {6, "!", "NOT_S",			-1, ASSOC_LEFT,				&type_vector, &type_void, &type_float},
 {6, "!", "NOT_ENT",		-1, ASSOC_LEFT,				&type_entity, &type_void, &type_float},
 {6, "!", "NOT_FNC",		-1, ASSOC_LEFT,				&type_function, &type_void, &type_float},
  
  {6, "<IF>", "IF",			-1, ASSOC_RIGHT,				&type_float, NULL, &type_void},
  {6, "<IFNOT>", "IFNOT",	-1, ASSOC_RIGHT,			&type_float, NULL, &type_void},
  
// calls returns REG_RETURN
 {6, "<CALL0>", "CALL0",	-1, ASSOC_LEFT,			&type_function, &type_void, &type_void},
 {6, "<CALL1>", "CALL1",	-1, ASSOC_LEFT,			&type_function, &type_void, &type_void},
 {6, "<CALL2>", "CALL2",	-1, ASSOC_LEFT,			&type_function, &type_void, &type_void}, 
 {6, "<CALL3>", "CALL3",	-1, ASSOC_LEFT,			&type_function, &type_void, &type_void}, 
 {6, "<CALL4>", "CALL4",	-1, ASSOC_LEFT,			&type_function, &type_void, &type_void},
 {6, "<CALL5>", "CALL5",	-1, ASSOC_LEFT,			&type_function, &type_void, &type_void},
 {6, "<CALL6>", "CALL6",	-1, ASSOC_LEFT,			&type_function, &type_void, &type_void},
 {6, "<CALL7>", "CALL7",	-1, ASSOC_LEFT,			&type_function, &type_void, &type_void},
 {6, "<CALL8>", "CALL8",	-1, ASSOC_LEFT,			&type_function, &type_void, &type_void},
  
 {6, "<STATE>", "STATE",	-1, ASSOC_LEFT,			&type_float, &type_float, &type_void},
  
 {6, "<GOTO>", "GOTO",		-1, ASSOC_RIGHT,			NULL, &type_void, &type_void},
  
 {6, "&&", "AND",			7, ASSOC_LEFT,					&type_float,	&type_float, &type_float},
 {6, "||", "OR",			7, ASSOC_LEFT,					&type_float,	&type_float, &type_float},

 {6, "&", "BITAND",			3, ASSOC_LEFT,				&type_float, &type_float, &type_float},
 {6, "|", "BITOR",			3, ASSOC_LEFT,				&type_float, &type_float, &type_float},

 //version 6 are in normal progs.



//these are hexen2
 {7, "*=", "MULSTORE_F",	6, ASSOC_RIGHT_RESULT,				&type_float, &type_float, &type_float},
 {7, "*=", "MULSTORE_V",	6, ASSOC_RIGHT_RESULT,				&type_vector, &type_float, &type_vector},
 {7, "*=", "MULSTOREP_F",	6, ASSOC_RIGHT_RESULT,				&type_pointer, &type_float, &type_float},
 {7, "*=", "MULSTOREP_V",	6, ASSOC_RIGHT_RESULT,				&type_pointer, &type_float, &type_vector},

 {7, "/=", "DIVSTORE_F",	6, ASSOC_RIGHT_RESULT,				&type_float, &type_float, &type_float},
 {7, "/=", "DIVSTOREP_F",	6, ASSOC_RIGHT_RESULT,				&type_pointer, &type_float, &type_float},

 {7, "+=", "ADDSTORE_F",	6, ASSOC_RIGHT_RESULT,				&type_float, &type_float, &type_float},
 {7, "+=", "ADDSTORE_V",	6, ASSOC_RIGHT_RESULT,				&type_vector, &type_vector, &type_vector},
 {7, "+=", "ADDSTOREP_F",	6, ASSOC_RIGHT_RESULT,				&type_pointer, &type_float, &type_float},
 {7, "+=", "ADDSTOREP_V",	6, ASSOC_RIGHT_RESULT,				&type_pointer, &type_vector, &type_vector},

 {7, "-=", "SUBSTORE_F",	6, ASSOC_RIGHT_RESULT,				&type_float, &type_float, &type_float},
 {7, "-=", "SUBSTORE_V",	6, ASSOC_RIGHT_RESULT,				&type_vector, &type_vector, &type_vector},
 {7, "-=", "SUBSTOREP_F",	6, ASSOC_RIGHT_RESULT,				&type_pointer, &type_float, &type_float},
 {7, "-=", "SUBSTOREP_V",	6, ASSOC_RIGHT_RESULT,				&type_pointer, &type_vector, &type_vector},

 {7, "<FETCH_GBL_F>", "FETCH_GBL_F",		-1, ASSOC_LEFT,	&type_float, &type_float, &type_float},
 {7, "<FETCH_GBL_V>", "FETCH_GBL_V",		-1, ASSOC_LEFT,	&type_vector, &type_float, &type_vector},
 {7, "<FETCH_GBL_S>", "FETCH_GBL_S",		-1, ASSOC_LEFT,	&type_string, &type_float, &type_string},
 {7, "<FETCH_GBL_E>", "FETCH_GBL_E",		-1, ASSOC_LEFT,	&type_entity, &type_float, &type_entity},
 {7, "<FETCH_GBL_FNC>", "FETCH_GBL_FNC",	-1, ASSOC_LEFT,	&type_function, &type_float, &type_function},

 {7, "<CSTATE>", "CSTATE",					-1, ASSOC_LEFT,	&type_float, &type_float, &type_void},

 {7, "<CWSTATE>", "CWSTATE",				-1, ASSOC_LEFT,	&type_float, &type_float, &type_void},

 {7, "<THINKTIME>", "THINKTIME",			-1, ASSOC_LEFT,	&type_entity, &type_float, &type_void},

 {7, "(+)", "BITSET",						6,	ASSOC_RIGHT,	&type_float, &type_float, &type_float},
 {7, "(+)", "BITSETP",						6,	ASSOC_RIGHT,	&type_pointer, &type_float, &type_float},
 {7, "(-)", "BITCLR",						6,	ASSOC_RIGHT,	&type_float, &type_float, &type_float},
 {7, "(-)", "BITCLRP",						6,	ASSOC_RIGHT,	&type_pointer, &type_float, &type_float},

 {7, "<RAND0>", "RAND0",					-1, ASSOC_LEFT,	&type_void, &type_void, &type_float},
 {7, "<RAND1>", "RAND1",					-1, ASSOC_LEFT,	&type_float, &type_void, &type_float},
 {7, "<RAND2>", "RAND2",					-1, ASSOC_LEFT,	&type_float, &type_float, &type_float},
 {7, "<RANDV0>", "RANDV0",					-1, ASSOC_LEFT,	&type_void, &type_void, &type_vector},
 {7, "<RANDV1>", "RANDV1",					-1, ASSOC_LEFT,	&type_vector, &type_void, &type_vector},
 {7, "<RANDV2>", "RANDV2",					-1, ASSOC_LEFT,	&type_vector, &type_vector, &type_vector},

 {7, "<SWITCH_F>", "SWITCH_F",				-1, ASSOC_LEFT,	&type_void, NULL, &type_void},
 {7, "<SWITCH_V>", "SWITCH_V",				-1, ASSOC_LEFT,	&type_void, NULL, &type_void},
 {7, "<SWITCH_S>", "SWITCH_S",				-1, ASSOC_LEFT,	&type_void, NULL, &type_void},
 {7, "<SWITCH_E>", "SWITCH_E",				-1, ASSOC_LEFT,	&type_void, NULL, &type_void},
 {7, "<SWITCH_FNC>", "SWITCH_FNC",			-1, ASSOC_LEFT,	&type_void, NULL, &type_void},

 {7, "<CASE>", "CASE",						-1, ASSOC_LEFT,	&type_void, NULL, &type_void},
 {7, "<CASERANGE>", "CASERANGE",			-1, ASSOC_LEFT,	&type_void, &type_void, NULL},


//Later are additions by DMW.

 {7, "<CALL1H>", "CALL1H",	-1, ASSOC_LEFT,			&type_function, &type_vector, &type_void},
 {7, "<CALL2H>", "CALL2H",	-1, ASSOC_LEFT,			&type_function, &type_vector, &type_vector}, 
 {7, "<CALL3H>", "CALL3H",	-1, ASSOC_LEFT,			&type_function, &type_vector, &type_vector}, 
 {7, "<CALL4H>", "CALL4H",	-1, ASSOC_LEFT,			&type_function, &type_vector, &type_vector},
 {7, "<CALL5H>", "CALL5H",	-1, ASSOC_LEFT,			&type_function, &type_vector, &type_vector},
 {7, "<CALL6H>", "CALL6H",	-1, ASSOC_LEFT,			&type_function, &type_vector, &type_vector},
 {7, "<CALL7H>", "CALL7H",	-1, ASSOC_LEFT,			&type_function, &type_vector, &type_vector},
 {7, "<CALL8H>", "CALL8H",	-1, ASSOC_LEFT,			&type_function, &type_vector, &type_vector},

 {7, "=",	"STORE_I", 6, ASSOC_RIGHT,				&type_integer, &type_integer, &type_integer},
 {7, "=",	"STORE_IF", 6, ASSOC_RIGHT,			&type_integer, &type_float, &type_integer},
 {7, "=",	"STORE_FI", 6, ASSOC_RIGHT,			&type_float, &type_integer, &type_float},

 {7, "+", "ADD_I", 4, ASSOC_LEFT,				&type_integer, &type_integer, &type_integer},
 {7, "+", "ADD_FI", 4, ASSOC_LEFT,				&type_float, &type_integer, &type_float},
 {7, "+", "ADD_IF", 4, ASSOC_LEFT,				&type_integer, &type_float, &type_float},
 
 {7, "-", "SUB_I", 4, ASSOC_LEFT,				&type_integer, &type_integer, &type_integer},
 {7, "-", "SUB_FI", 4, ASSOC_LEFT,				&type_float, &type_integer, &type_float},
 {7, "-", "SUB_IF", 4, ASSOC_LEFT,				&type_integer, &type_float, &type_float},

 {7, "<CIF>", "C_ITOF", -1, ASSOC_LEFT,				&type_integer, &type_void, &type_float},
 {7, "<CFI>", "C_FTOI", -1, ASSOC_LEFT,				&type_float, &type_void, &type_integer},
 {7, "<CPIF>", "CP_ITOF", -1, ASSOC_LEFT,			&type_pointer, &type_integer, &type_float},
 {7, "<CPFI>", "CP_FTOI", -1, ASSOC_LEFT,			&type_pointer, &type_float, &type_integer},

 {7, ".", "INDIRECT", 1, ASSOC_LEFT,				&type_entity,	&type_field, &type_integer},
 {7, "=", "STOREP_I", 6, ASSOC_RIGHT,				&type_pointer,	&type_integer, &type_integer},
 {7, "=", "STOREP_IF", 6, ASSOC_RIGHT,				&type_pointer,	&type_float, &type_integer},
 {7, "=", "STOREP_FI", 6, ASSOC_RIGHT,				&type_pointer,	&type_integer, &type_float},

 {7, "&", "BITAND_I", 3, ASSOC_LEFT,				&type_integer,	&type_integer, &type_integer},
 {7, "|", "BITOR_I", 3, ASSOC_LEFT,				&type_integer,	&type_integer, &type_integer},

 {7, "*", "MUL_I", 3, ASSOC_LEFT,				&type_integer,	&type_integer, &type_integer},
 {7, "/", "DIV_I", 3, ASSOC_LEFT,				&type_integer,	&type_integer, &type_integer},
 {7, "==", "EQ_I", 5, ASSOC_LEFT,				&type_integer,	&type_integer, &type_integer},
 {7, "!=", "NE_I", 5, ASSOC_LEFT,				&type_integer,	&type_integer, &type_integer},

 {7, "<IFNOTS>", "IFNOTS", -1, ASSOC_RIGHT,		&type_string,	NULL, &type_void},
 {7, "<IFS>", "IFS", -1, ASSOC_RIGHT,				&type_string,	NULL, &type_void},

 {7, "!", "NOT_I", -1, ASSOC_LEFT,				&type_integer,	&type_void, &type_integer},

 {7, "/", "DIV_VF", 3, ASSOC_LEFT,				&type_vector,	&type_float, &type_float},

 {7, "^", "POWER_I", 3, ASSOC_LEFT,				&type_integer,	&type_integer, &type_integer},
 {7, ">>", "RSHIFT_I", 3, ASSOC_LEFT,			&type_integer,	&type_integer, &type_integer},
 {7, "<<", "LSHIFT_I", 3, ASSOC_LEFT,			&type_integer,	&type_integer, &type_integer},

										//var,		offset			return
 {7, "<ARRAY>", "GET_POINTER", -1, ASSOC_LEFT,	&type_float,		&type_integer, &type_pointer},
 {7, "<ARRAY>", "ARRAY_OFS", -1, ASSOC_LEFT,		&type_pointer,	&type_integer, &type_pointer},

 {7, "=", "LOADA_F", 6, ASSOC_LEFT,			&type_float,	&type_integer, &type_float},
 {7, "=", "LOADA_V", 6, ASSOC_LEFT,			&type_vector,	&type_integer, &type_vector},
 {7, "=", "LOADA_S", 6, ASSOC_LEFT,			&type_string,	&type_integer, &type_string},
 {7, "=", "LOADA_ENT", 6, ASSOC_LEFT,		&type_entity,	&type_integer, &type_entity},
 {7, "=", "LOADA_FLD", 6, ASSOC_LEFT,		&type_field,	&type_integer, &type_field},
 {7, "=", "LOADA_FNC", 6, ASSOC_LEFT,		&type_function,	&type_integer, &type_function},
 {7, "=", "LOADA_I", 6, ASSOC_LEFT,			&type_integer,	&type_integer, &type_integer},

 {7, "=", "STORE_P", 6, ASSOC_RIGHT,			&type_pointer,	&type_pointer, &type_void},
 {7, ".", "INDIRECT_P", 1, ASSOC_LEFT,			&type_entity,	&type_field, &type_pointer},

 {7, "=", "LOADP_F", 6, ASSOC_LEFT,			&type_pointer,	&type_integer, &type_float},
 {7, "=", "LOADP_V", 6, ASSOC_LEFT,			&type_pointer,	&type_integer, &type_vector},
 {7, "=", "LOADP_S", 6, ASSOC_LEFT,			&type_pointer,	&type_integer, &type_string},
 {7, "=", "LOADP_ENT", 6, ASSOC_LEFT,		&type_pointer,	&type_integer, &type_entity},
 {7, "=", "LOADP_FLD", 6, ASSOC_LEFT,		&type_pointer,	&type_integer, &type_field},
 {7, "=", "LOADP_FNC", 6, ASSOC_LEFT,		&type_pointer,	&type_integer, &type_function},
 {7, "=", "LOADP_I", 6, ASSOC_LEFT,			&type_pointer,	&type_integer, &type_integer},


 {7, "<=", "LE_I", 5, ASSOC_LEFT,					&type_integer, &type_integer, &type_integer},
 {7, ">=", "GE_I", 5, ASSOC_LEFT,					&type_integer, &type_integer, &type_integer},
 {7, "<", "LT_I", 5, ASSOC_LEFT,					&type_integer, &type_integer, &type_integer},
 {7, ">", "GT_I", 5, ASSOC_LEFT,					&type_integer, &type_integer, &type_integer},

 {7, "<=", "LE_IF", 5, ASSOC_LEFT,					&type_integer, &type_float, &type_integer},
 {7, ">=", "GE_IF", 5, ASSOC_LEFT,					&type_integer, &type_float, &type_integer},
 {7, "<", "LT_IF", 5, ASSOC_LEFT,					&type_integer, &type_float, &type_integer},
 {7, ">", "GT_IF", 5, ASSOC_LEFT,					&type_integer, &type_float, &type_integer},

 {7, "<=", "LE_FI", 5, ASSOC_LEFT,					&type_float, &type_integer, &type_integer},
 {7, ">=", "GE_FI", 5, ASSOC_LEFT,					&type_float, &type_integer, &type_integer},
 {7, "<", "LT_FI", 5, ASSOC_LEFT,					&type_float, &type_integer, &type_integer},
 {7, ">", "GT_FI", 5, ASSOC_LEFT,					&type_float, &type_integer, &type_integer},

 {7, "==", "EQ_IF", 5, ASSOC_LEFT,				&type_integer,	&type_float, &type_integer},
 {7, "==", "EQ_FI", 5, ASSOC_LEFT,				&type_float,	&type_integer, &type_float},

 	//-------------------------------------
	//string manipulation.
 {7, "+", "ADD_SF",	4, ASSOC_LEFT,				&type_string,	&type_float, &type_string},
 {7, "-", "SUB_S",	4, ASSOC_LEFT,				&type_string,	&type_string, &type_float},
 {7, "<STOREP_C>", "STOREP_C",	1, ASSOC_RIGHT,	&type_string,	&type_float, &type_float},
 {7, "<LOADP_C>", "LOADP_C",	1, ASSOC_LEFT,	&type_string,	&type_void, &type_float},
	//-------------------------------------



{7, "*", "MUL_IF", 5, ASSOC_LEFT,				&type_integer,	&type_float,	&type_integer},
{7, "*", "MUL_FI", 5, ASSOC_LEFT,				&type_float,	&type_integer,	&type_float},
{7, "*", "MUL_VI", 5, ASSOC_LEFT,				&type_vector,	&type_integer,	&type_vector},
{7, "*", "MUL_IV", 5, ASSOC_LEFT,				&type_integer,	&type_vector,	&type_vector},

{7, "/", "DIV_IF", 5, ASSOC_LEFT,				&type_integer,	&type_float,	&type_integer},
{7, "/", "DIV_FI", 5, ASSOC_LEFT,				&type_float,	&type_integer,	&type_float},

{7, "&", "BITAND_IF", 5, ASSOC_LEFT,			&type_integer,	&type_float,	&type_integer},
{7, "|", "BITOR_IF", 5, ASSOC_LEFT,				&type_integer,	&type_float,	&type_integer},
{7, "&", "BITAND_FI", 5, ASSOC_LEFT,			&type_float,	&type_integer,	&type_float},
{7, "|", "BITOR_FI", 5, ASSOC_LEFT,				&type_float,	&type_integer,	&type_float},

{7, "&&", "AND_I", 5, ASSOC_LEFT,				&type_integer,	&type_integer,	&type_integer},
{7, "||", "OR_I", 5, ASSOC_LEFT,				&type_integer,	&type_integer,	&type_integer},
{7, "&&", "AND_IF", 5, ASSOC_LEFT,				&type_integer,	&type_float,	&type_integer},
{7, "||", "OR_IF", 5, ASSOC_LEFT,				&type_integer,	&type_float,	&type_integer},
{7, "&&", "AND_FI", 5, ASSOC_LEFT,				&type_float,	&type_float,	&type_float},
{7, "||", "OR_FI", 5, ASSOC_LEFT,				&type_float,	&type_float,	&type_integer},
{7, "!=", "NE_IF", 5, ASSOC_LEFT,				&type_integer,	&type_float,	&type_integer},
{7, "!=", "NE_FI", 5, ASSOC_LEFT,				&type_integer,	&type_float,	&type_integer},

	




{7, "<>", "GSTOREP_I", -1, ASSOC_LEFT,				&type_float,	&type_float,	&type_float},
{7, "<>", "GSTOREP_F", -1, ASSOC_LEFT,				&type_float,	&type_float,	&type_float},
{7, "<>", "GSTOREP_ENT", -1, ASSOC_LEFT,				&type_float,	&type_float,	&type_float},
{7, "<>", "GSTOREP_FLD", -1, ASSOC_LEFT,				&type_float,	&type_float,	&type_float},
{7, "<>", "GSTOREP_S", -1, ASSOC_LEFT,				&type_float,	&type_float,	&type_float},
{7, "<>", "GSTORE_PFNC", -1, ASSOC_LEFT,				&type_float,	&type_float,	&type_float},
{7, "<>", "GSTOREP_V", -1, ASSOC_LEFT,				&type_float,	&type_float,	&type_float},

{7, "<>", "GADDRESS", -1, ASSOC_LEFT,				&type_float,	&type_float,	&type_float},

{7, "<>", "GLOAD_I", -1, ASSOC_LEFT,				&type_float,	&type_float,	&type_float},
{7, "<>", "GLOAD_F", -1, ASSOC_LEFT,				&type_float,	&type_float,	&type_float},
{7, "<>", "GLOAD_FLD", -1, ASSOC_LEFT,				&type_float,	&type_float,	&type_float},
{7, "<>", "GLOAD_ENT", -1, ASSOC_LEFT,				&type_float,	&type_float,	&type_float},
{7, "<>", "GLOAD_S", -1, ASSOC_LEFT,				&type_float,	&type_float,	&type_float},
{7, "<>", "GLOAD_FNC", -1, ASSOC_LEFT,				&type_float,	&type_float,	&type_float},

{7, "<>", "BOUNDCHECK", -1, ASSOC_LEFT,				&type_float,	&type_float,	&type_float},

{7, "=",	"STOREP_P", 6, ASSOC_RIGHT,				&type_pointer, &type_pointer, &type_void},

 {0, NULL}
};

#undef ASSOC_RIGHT_RESULT

#define	TOP_PRIORITY	7
#define	NOT_PRIORITY	5
//conditional and/or
#define CONDITION_PRIORITY 7


//this system cuts out 10/120
//these evaluate as top first.
QCC_opcode_t *opcodeprioritized[TOP_PRIORITY+1][64] =
{
	{	//don't use
/*		&pr_opcodes[OP_DONE],
		&pr_opcodes[OP_RETURN],

		&pr_opcodes[OP_NOT_F],
		&pr_opcodes[OP_NOT_V],
		&pr_opcodes[OP_NOT_S],
		&pr_opcodes[OP_NOT_ENT],
		&pr_opcodes[OP_NOT_FNC],

		&pr_opcodes[OP_IF],
		&pr_opcodes[OP_IFNOT],
		&pr_opcodes[OP_CALL0],
		&pr_opcodes[OP_CALL1],
		&pr_opcodes[OP_CALL2],
		&pr_opcodes[OP_CALL3],
		&pr_opcodes[OP_CALL4],
		&pr_opcodes[OP_CALL5],
		&pr_opcodes[OP_CALL6],
		&pr_opcodes[OP_CALL7],
		&pr_opcodes[OP_CALL8],
		&pr_opcodes[OP_STATE],
		&pr_opcodes[OP_GOTO],

		&pr_opcodes[OP_IFNOTS],
		&pr_opcodes[OP_IFS],

		&pr_opcodes[OP_NOT_I],
*/		NULL
	}, {	//1

		&pr_opcodes[OP_LOAD_F],
		&pr_opcodes[OP_LOAD_V],
		&pr_opcodes[OP_LOAD_S],
		&pr_opcodes[OP_LOAD_ENT],
		&pr_opcodes[OP_LOAD_FLD],
		&pr_opcodes[OP_LOAD_FNC],
		&pr_opcodes[OP_LOAD_I],
		&pr_opcodes[OP_LOAD_P],
		&pr_opcodes[OP_ADDRESS],
		NULL
	}, {	//2
/*	//conversion. don't use
		&pr_opcodes[OP_C_ITOF],
		&pr_opcodes[OP_C_FTOI],
		&pr_opcodes[OP_CP_ITOF],
		&pr_opcodes[OP_CP_FTOI],
*/		NULL
	}, {	//3
		&pr_opcodes[OP_MUL_F],
		&pr_opcodes[OP_MUL_V],
		&pr_opcodes[OP_MUL_FV],
		&pr_opcodes[OP_MUL_VF],
		&pr_opcodes[OP_MUL_I],

		&pr_opcodes[OP_DIV_F],
		&pr_opcodes[OP_DIV_I],
		&pr_opcodes[OP_DIV_VF],

		&pr_opcodes[OP_BITAND],
		&pr_opcodes[OP_BITAND_I],

		&pr_opcodes[OP_BITOR],
		&pr_opcodes[OP_BITOR_I],

		&pr_opcodes[OP_POWER_I],
		&pr_opcodes[OP_RSHIFT_I],
		&pr_opcodes[OP_LSHIFT_I],

		NULL
	}, {	//4

		&pr_opcodes[OP_ADD_F],
		&pr_opcodes[OP_ADD_V],
		&pr_opcodes[OP_ADD_I],
		&pr_opcodes[OP_ADD_FI],
		&pr_opcodes[OP_ADD_IF],
		&pr_opcodes[OP_ADD_SF],

		&pr_opcodes[OP_SUB_F],
		&pr_opcodes[OP_SUB_V],
		&pr_opcodes[OP_SUB_I],
		&pr_opcodes[OP_SUB_FI],
		&pr_opcodes[OP_SUB_IF],
		&pr_opcodes[OP_SUB_S],
		NULL
	}, {	//5

		&pr_opcodes[OP_EQ_F],
		&pr_opcodes[OP_EQ_V],
		&pr_opcodes[OP_EQ_S],
		&pr_opcodes[OP_EQ_E],
		&pr_opcodes[OP_EQ_FNC],
		&pr_opcodes[OP_EQ_I],
		&pr_opcodes[OP_EQ_IF],
		&pr_opcodes[OP_EQ_FI],
	
		&pr_opcodes[OP_NE_F],
		&pr_opcodes[OP_NE_V],
		&pr_opcodes[OP_NE_S],
		&pr_opcodes[OP_NE_E],
		&pr_opcodes[OP_NE_FNC],
		&pr_opcodes[OP_NE_I],
	
		&pr_opcodes[OP_LE],
		&pr_opcodes[OP_LE_I],
		&pr_opcodes[OP_LE_IF],
		&pr_opcodes[OP_LE_FI],
		&pr_opcodes[OP_GE],
		&pr_opcodes[OP_GE_I],
		&pr_opcodes[OP_GE_IF],
		&pr_opcodes[OP_GE_FI],
		&pr_opcodes[OP_LT],
		&pr_opcodes[OP_LT_I],
		&pr_opcodes[OP_LT_IF],
		&pr_opcodes[OP_LT_FI],
		&pr_opcodes[OP_GT],
		&pr_opcodes[OP_GT_I],
		&pr_opcodes[OP_GT_IF],
		&pr_opcodes[OP_GT_FI],

		NULL
	}, {	//6
		&pr_opcodes[OP_STORE_F],
		&pr_opcodes[OP_STORE_V],
		&pr_opcodes[OP_STORE_S],
		&pr_opcodes[OP_STORE_ENT],
		&pr_opcodes[OP_STORE_FLD],
		&pr_opcodes[OP_STORE_FNC],
		&pr_opcodes[OP_STORE_I],
		&pr_opcodes[OP_STORE_IF],
		&pr_opcodes[OP_STORE_FI],
		&pr_opcodes[OP_STORE_P],

		&pr_opcodes[OP_STOREP_F],
		&pr_opcodes[OP_STOREP_V],
		&pr_opcodes[OP_STOREP_S],
		&pr_opcodes[OP_STOREP_ENT],
		&pr_opcodes[OP_STOREP_FLD],
		&pr_opcodes[OP_STOREP_FNC],
		&pr_opcodes[OP_STOREP_I],
		&pr_opcodes[OP_STOREP_IF],
		&pr_opcodes[OP_STOREP_FI],
		&pr_opcodes[OP_STOREP_P],

		&pr_opcodes[OP_DIVSTORE_F],
		&pr_opcodes[OP_DIVSTOREP_F],
		&pr_opcodes[OP_MULSTORE_F],
		&pr_opcodes[OP_MULSTORE_V],
		&pr_opcodes[OP_MULSTOREP_F],
		&pr_opcodes[OP_MULSTOREP_V],
		&pr_opcodes[OP_ADDSTORE_F],
		&pr_opcodes[OP_ADDSTORE_V],
		&pr_opcodes[OP_ADDSTOREP_F],
		&pr_opcodes[OP_ADDSTOREP_V],
		&pr_opcodes[OP_SUBSTORE_F],
		&pr_opcodes[OP_SUBSTORE_V],
		&pr_opcodes[OP_SUBSTOREP_F],
		&pr_opcodes[OP_SUBSTOREP_V],

		&pr_opcodes[OP_BITSET],
		&pr_opcodes[OP_BITSETP],
		&pr_opcodes[OP_BITCLR],
		&pr_opcodes[OP_BITCLRP],
		NULL
	}, {	//7
		&pr_opcodes[OP_AND],
		&pr_opcodes[OP_OR],
		NULL
	}
};

pbool QCC_OPCodeValid(QCC_opcode_t *op)
{
	int num;
	num = op - pr_opcodes;
	switch(qcc_targetformat)
	{
	case QCF_STANDARD:
	case QCF_KK7:
		if (num < OP_MULSTORE_F)
			return true;
		return false;
	case QCF_HEXEN2:
		if (num >= OP_SWITCH_V && num <= OP_SWITCH_FNC)	//these were assigned numbers but were never actually implemtented in standard h2.
			return false;
//		if (num >= OP_MULSTORE_F && num <= OP_SUBSTOREP_V)
//			return false;
		if (num <= OP_CALL8H)	//CALLXH are fixed up. This is to provide more dynamic switching...??
			return true;
		return false;
	case QCF_FTE:
	case QCF_FTE32:
	case QCF_FTEDEBUG:
	case QCF_FTEDEBUG32:
		return true;
	}
	return false;
}

QCC_def_t *QCC_PR_Expression (int priority);
int QCC_AStatementJumpsTo(int targ, int first, int last);
pbool QCC_StatementIsAJump(int stnum, int notifdest);

temp_t *functemps;		//floats/strings/funcs/ents...

//===========================================================================


/*
============
PR_Statement

Emits a primitive statement, returning the var it places it's value in
============
*/
QCC_def_t *QCC_PR_Statement ( QCC_opcode_t *op, QCC_def_t *var_a, QCC_def_t *var_b, QCC_dstatement_t **outstatement);
int inline QCC_ShouldConvert(QCC_def_t *var, etype_t wanted)
{
	if (var->type->type == ev_integer && wanted == ev_function)
		return 0;
	if (var->type->type == ev_pointer && var->type->aux_type)
	{
		if (var->type->aux_type->type == ev_float && wanted == ev_integer)
			return OP_CP_FTOI;		

		if (var->type->aux_type->type == ev_integer && wanted == ev_float)
			return OP_CP_ITOF;
	}
	else
	{
		if (var->type->type == ev_float && wanted == ev_integer)
			return OP_CONV_FTOI;

		if (var->type->type == ev_integer && wanted == ev_float)
			return OP_CONV_ITOF;
	}

	return -1;
}
QCC_def_t *QCC_SupplyConversion(QCC_def_t *var, etype_t wanted)
{
	int o;

	if (pr_classtype && var->type->type == ev_field && wanted != ev_field)
	{
		if (pr_classtype)
		{	//load self.var into a temp
			QCC_def_t *self;
			self = QCC_PR_GetDef(type_entity, "self", NULL, true, 1);
			switch(wanted)
			{
			case ev_float:
				return QCC_PR_Statement(pr_opcodes+OP_LOAD_F, self, var, NULL);
			case ev_string:
				return QCC_PR_Statement(pr_opcodes+OP_LOAD_S, self, var, NULL);
			case ev_function:
				return QCC_PR_Statement(pr_opcodes+OP_LOAD_FNC, self, var, NULL);
			case ev_vector:
				return QCC_PR_Statement(pr_opcodes+OP_LOAD_V, self, var, NULL);
			case ev_entity:
				return QCC_PR_Statement(pr_opcodes+OP_LOAD_ENT, self, var, NULL);
			default:
				QCC_Error(ERR_INTERNAL, "Inexplicit field load failed, try explicit");
			}
		}
	}

	o = QCC_ShouldConvert(var, wanted);

	if (o <= 0)	//no conversion
		return var;
	

	return QCC_PR_Statement(&pr_opcodes[o], var, NULL, NULL);	//conversion return value
}
QCC_def_t *QCC_MakeStringDef(char *value);
QCC_def_t *QCC_MakeFloatDef(float value);
QCC_def_t *QCC_MakeIntDef(int value);

typedef struct freeoffset_s {
	struct freeoffset_s *next;
	gofs_t ofs;
	unsigned int size;
} freeoffset_t;

freeoffset_t *freeofs;

//assistant functions. This can safly be bipassed with the old method for more complex things.
gofs_t QCC_GetFreeOffsetSpace(unsigned int size)
{
	int ofs;
	if (opt_locals_marshalling)
	{
		freeoffset_t *fofs, *prev;
		for (fofs = freeofs, prev = NULL; fofs; fofs=fofs->next)
		{
			if (fofs->size == size)
			{
				if (prev)
					prev->next = fofs->next;
				else
					freeofs = fofs->next;

				return fofs->ofs;
			}
			prev = fofs;
		}
		for (fofs = freeofs, prev = NULL; fofs; fofs=fofs->next)
		{
			if (fofs->size > size)
			{
				fofs->size -= size;
				fofs->ofs += size;

				return fofs->ofs-size;
			}
			prev = fofs;
		}
	}
	
	ofs = numpr_globals;
	numpr_globals+=size;

	if (numpr_globals >= MAX_REGS)
	{
		if (!opt_overlaptemps || !opt_locals_marshalling)
			QCC_Error(ERR_TOOMANYGLOBALS, "numpr_globals exceeded MAX_REGS - you'll need to use more optimisations");
		else
			QCC_Error(ERR_TOOMANYGLOBALS, "numpr_globals exceeded MAX_REGS");
	}

	return ofs;
}

void QCC_FreeOffset(gofs_t ofs, unsigned int size)
{
	freeoffset_t *fofs;
	if (ofs+size == numpr_globals)
	{
		numpr_globals -= size;
		return;
	}

	for (fofs = freeofs; fofs; fofs=fofs->next)
	{
		if (fofs->ofs == ofs + size)
		{
			fofs->ofs -= size;
			fofs->size += size;
			return;
		}
		if (fofs->ofs+fofs->size == ofs)
		{
			fofs->size += size;
			return;
		}
	}

	fofs = qccHunkAlloc(sizeof(freeoffset_t));
	fofs->next = freeofs;
	fofs->ofs = ofs;
	fofs->size = size;

	freeofs = fofs;
	return;
}

static QCC_def_t *QCC_GetTemp(QCC_type_t *type)
{
//#define CRAZYTEMPOPTS //not worth it. saves 2 temps with hexen2 (without even touching numpr_globals)
	QCC_def_t *var_c;
	temp_t *t;
#ifdef CRAZYTEMPOPTS
	temp_t *best = NULL;
#endif

	var_c = (void *)qccHunkAlloc (sizeof(QCC_def_t));
	memset (var_c, 0, sizeof(QCC_def_t));		
	var_c->type = type;
	var_c->name = "temp";

	if (opt_overlaptemps)	//don't exceed. This lets us allocate a huge block, and still be able to compile smegging big funcs.
	{
		for (t = functemps; t; t = t->next)
		{
			if (!t->used && t->size == type->size)
			{
#ifdef CRAZYTEMPOPTS
				best = t;
				if (t->scope == pr_scope)
#endif
					break;
			}
		}
#ifdef CRAZYTEMPOPTS
		t = best;
#endif
		if (t && t->scope && t->scope != pr_scope)
			QCC_Error(ERR_INTERNAL, "Internal error temp has scope not equal to current scope");

		if (!t)
		{
			//allocate a new one
			t = qccHunkAlloc(sizeof(temp_t));
			t->size = type->size;
			t->next = functemps;
			functemps = t;
			
			t->ofs = QCC_GetFreeOffsetSpace(t->size);

			numtemps+=t->size;
		}
		else
			optres_overlaptemps+=t->size;
		//use a previous one.
		var_c->ofs = t->ofs;
		var_c->temp = t;
		t->lastfunc = pr_scope;
	}
	else
	{
		//allocate a new one
		t = qccHunkAlloc(sizeof(temp_t));
		t->size = type->size;

		t->next = functemps;
		functemps = t;

		t->ofs = QCC_GetFreeOffsetSpace(t->size);

		numtemps+=t->size;

		var_c->ofs = t->ofs;
		var_c->temp = t;
		t->lastfunc = pr_scope;
	}

	var_c->s_file = s_file;
	var_c->s_line = pr_source_line;

	if (var_c->temp)
		var_c->temp->used = true;

	return var_c;
}

//nothing else references this temp.
static void QCC_FreeTemp(QCC_def_t *t)
{
	if (t && t->temp)
		t->temp->used = false;
}

static void QCC_UnFreeTemp(QCC_def_t *t)
{
	if (t->temp)
		t->temp->used = true;
}

//We've just parsed a statement.
//We can gaurentee that any used temps are now not used.
#ifdef _DEBUG
static void QCC_FreeTemps(void)
{
	temp_t *t;

	t = functemps;
	while(t)
	{
		if (t->used && !pr_error_count)	//don't print this after an error jump out.
		{
			QCC_PR_ParseWarning(WARN_DEBUGGING, "Temp was used in %s", pr_scope->name);
			t->used = false;
		}
		t = t->next;
	}
}
#else
#define QCC_FreeTemps()
#endif

//temps that are still in use over a function call can be considered dodgy.
//we need to remap these to locally defined temps, on return from the function so we know we got them all.
static void QCC_LockActiveTemps(void)
{
	temp_t *t;

	t = functemps;
	while(t)
	{
		if (t->used)
			t->scope = pr_scope;
		t = t->next;
	}
	
}

static void QCC_RemapLockedTemp(temp_t *t, int firststatement, int laststatement)
{
#ifdef WRITEASM
	char buffer[128];
#endif

	QCC_def_t *def;
	int newofs;
	QCC_dstatement_t *st;
	int i;

	newofs = 0;
	for (i = firststatement, st = &statements[i]; i < laststatement; i++, st++)
	{
		if (pr_opcodes[st->op].type_a && st->a == t->ofs)
		{
			if (!newofs)
			{
				newofs = QCC_GetFreeOffsetSpace(t->size);
				numtemps+=t->size;

				def = QCC_PR_DummyDef(type_float, NULL, pr_scope, t->size, newofs, false);
				def->nextlocal = pr.localvars;
				def->constant = false;
#ifdef WRITEASM
				sprintf(buffer, "locked_%i", t->ofs);
				def->name = qccHunkAlloc(strlen(buffer)+1);
				strcpy(def->name, buffer);
#endif
				pr.localvars = def;
			}
			st->a = newofs;
		}
		if (pr_opcodes[st->op].type_b && st->b == t->ofs)
		{
			if (!newofs)
			{
				newofs = QCC_GetFreeOffsetSpace(t->size);
				numtemps+=t->size;

				def = QCC_PR_DummyDef(type_float, NULL, pr_scope, t->size, newofs, false);
				def->nextlocal = pr.localvars;
				def->constant = false;
#ifdef WRITEASM
				sprintf(buffer, "locked_%i", t->ofs);
				def->name = qccHunkAlloc(strlen(buffer)+1);
				strcpy(def->name, buffer);
#endif
				pr.localvars = def;
			}
			st->b = newofs;
		}
		if (pr_opcodes[st->op].type_c && st->c == t->ofs)
		{
			if (!newofs)
			{
				newofs = QCC_GetFreeOffsetSpace(t->size);
				numtemps+=t->size;

				def = QCC_PR_DummyDef(type_float, NULL, pr_scope, t->size, newofs, false);
				def->nextlocal = pr.localvars;
				def->constant = false;
#ifdef WRITEASM
				sprintf(buffer, "locked_%i", t->ofs);
				def->name = qccHunkAlloc(strlen(buffer)+1);
				strcpy(def->name, buffer);
#endif
				pr.localvars = def;
			}
			st->c = newofs;
		}
	}
}

static void QCC_RemapLockedTemps(int firststatement, int laststatement)
{
	temp_t *t;

	t = functemps;
	while(t)
	{
		if (t->scope)
		{
			QCC_RemapLockedTemp(t, firststatement, laststatement);
			t->scope = NULL;
			t->lastfunc = NULL;
		}
		t = t->next;
	}
}

static void QCC_fprintfLocals(FILE *f, gofs_t paramstart, gofs_t paramend)
{
	QCC_def_t	*var;
	temp_t *t;
	int i;

	for (var = pr.localvars; var; var = var->nextlocal)
	{
		if (var->ofs >= paramstart && var->ofs < paramend)
			continue;
		fprintf(f, "local %s %s;\n", TypeName(var->type), var->name);
	}

	for (t = functemps, i = 0; t; t = t->next, i++)
	{
		if (t->lastfunc == pr_scope)
		{
			fprintf(f, "local %s temp_%i;\n", (t->size == 1)?"float":"vector", i);
		}
	}
}

#ifdef WRITEASM
void QCC_WriteAsmFunction(QCC_def_t	*sc, unsigned int firststatement, gofs_t firstparm);
static const char *QCC_VarAtOffset(unsigned int ofs, unsigned int size)
{
	static char message[1024];
	QCC_def_t	*var;
	//check the temps
	temp_t *t;
	int i;

	for (t = functemps, i = 0; t; t = t->next, i++)
	{
		if (ofs >= t->ofs && ofs < t->ofs + t->size)
		{
			if (size < t->size)
				sprintf(message, "temp_%i_%c", i, 'x' + (ofs-t->ofs)%3);
			else
				sprintf(message, "temp_%i", i);
			return message;
		}
	}

	for (var = pr.localvars; var; var = var->nextlocal)
	{
		if (var->scope && var->scope != pr_scope)
			continue;	//this should be an error
		if (ofs >= var->ofs && ofs < var->ofs + var->type->size)
		{
			if (*var->name)
			{
				if (!STRCMP(var->name, "IMMEDIATE"))	//continue, don't get bogged down by multiple bits of code
					continue;
				if (size < var->type->size)
					sprintf(message, "%s_%c", var->name, 'x' + (ofs-var->ofs)%3);
				else
					sprintf(message, "%s", var->name);
				return message;
			}
		}
	}

	for (var = pr.def_head.next; var; var = var->next)
	{
		if (var->scope && var->scope != pr_scope)
			continue;

		if (ofs >= var->ofs && ofs < var->ofs + var->type->size)
		{
			if (*var->name)
			{
				if (!STRCMP(var->name, "IMMEDIATE"))
				{
					switch(var->type->type)
					{
					case ev_string:
						sprintf(message, "\"%.1020s\"", &strings[((int *)qcc_pr_globals)[var->ofs]]);
						return message;
					case ev_integer:
						sprintf(message, "%i", ((int *)qcc_pr_globals)[var->ofs]);
						return message;
					case ev_float:
						sprintf(message, "%f", qcc_pr_globals[var->ofs]);
						return message;
					case ev_vector:
						sprintf(message, "'%f %f %f'", qcc_pr_globals[var->ofs], qcc_pr_globals[var->ofs+1], qcc_pr_globals[var->ofs+2]);
						return message;
					default:
						sprintf(message, "IMMEDIATE");
						return message;
					}
				}
				if (size < var->type->size)
					sprintf(message, "%s_%c", var->name, 'x' + (ofs-var->ofs)%3);
				else
					sprintf(message, "%s", var->name);
				return message;
			}
		}
	}

	if (size >= 3)
	{
		if (ofs >= OFS_RETURN && ofs < OFS_PARM0)
			sprintf(message, "return");
		else if (ofs >= OFS_PARM0 && ofs < RESERVED_OFS)
			sprintf(message, "parm%i", (ofs-OFS_PARM0)/3);
		else
			sprintf(message, "offset_%i", ofs);
	}
	else
	{
		if (ofs >= OFS_RETURN && ofs < OFS_PARM0)
			sprintf(message, "return_%c", 'x' + ofs-OFS_RETURN);
		else if (ofs >= OFS_PARM0 && ofs < RESERVED_OFS)
			sprintf(message, "parm%i_%c", (ofs-OFS_PARM0)/3, 'x' + (ofs-OFS_PARM0)%3);
		else
			sprintf(message, "offset_%i", ofs);
	}
	return message;
}
#endif

QCC_def_t *QCC_PR_Statement ( QCC_opcode_t *op, QCC_def_t *var_a, QCC_def_t *var_b, QCC_dstatement_t **outstatement)
{
	QCC_dstatement_t	*statement;
	QCC_def_t			*var_c=NULL, *temp=NULL;

	if (outstatement == (QCC_dstatement_t **)0xffffffff)
		outstatement = NULL;
	else if (op->priority != -1)
	{
		if (op->associative!=ASSOC_LEFT)
		{
			if (op->type_a == &type_pointer)
				var_b = QCC_SupplyConversion(var_b, (*op->type_b)->type);
			else
				var_b = QCC_SupplyConversion(var_b, (*op->type_a)->type);
		}
		else
		{
			if (var_a)
				var_a = QCC_SupplyConversion(var_a, (*op->type_a)->type);
			if (var_b)
				var_b = QCC_SupplyConversion(var_b, (*op->type_b)->type);
//			if (op->type_a == &def_pointer)
//					var_a = QCC_SupplyConversion(var_a, (*op->type_b)->type);
//				else
//					var_a = QCC_SupplyConversion(var_a, (*op->type_a)->type);
//			}
//				//can't convert the left componant of an assignment operation
//			if (var_b && var_b->type && var_b->type != op->type_b->type)
//				var_b = QCC_SupplyConversion(var_b, op->type_b->type->type);			
		}
	}

	if (var_a)
	{
		var_a->references++;
		QCC_FreeTemp(var_a);
	}
	if (var_b)
	{
		var_b->references++;
		QCC_FreeTemp(var_b);
	}

	if (keyword_class && var_a && var_b)
	{
		if (var_a->type->type == ev_entity && var_b->type->type == ev_entity)
			if (var_a->type != var_b->type)
				if (strcmp(var_a->type->name, var_b->type->name))
					QCC_PR_ParseWarning(0, "Inexplict cast");
	}

	//maths operators
	if (opt_constantarithmatic && (var_a && var_a->constant) && (var_b && var_b->constant))
	{
		switch (op - pr_opcodes)	//improve some of the maths.
		{
		case OP_BITOR:
			optres_constantarithmatic++;
			return QCC_MakeFloatDef((float)((int)G_FLOAT(var_a->ofs) | (int)G_FLOAT(var_b->ofs)));
		case OP_BITAND:
			optres_constantarithmatic++;
			return QCC_MakeFloatDef((float)((int)G_FLOAT(var_a->ofs) & (int)G_FLOAT(var_b->ofs)));
		case OP_MUL_F:
			optres_constantarithmatic++;
			return QCC_MakeFloatDef(G_FLOAT(var_a->ofs) * G_FLOAT(var_b->ofs));
		case OP_DIV_F:
			optres_constantarithmatic++;
			return QCC_MakeFloatDef(G_FLOAT(var_a->ofs) / G_FLOAT(var_b->ofs));
		case OP_ADD_F:
			optres_constantarithmatic++;
			return QCC_MakeFloatDef(G_FLOAT(var_a->ofs) + G_FLOAT(var_b->ofs));
		case OP_SUB_F:
			optres_constantarithmatic++;
			return QCC_MakeFloatDef(G_FLOAT(var_a->ofs) - G_FLOAT(var_b->ofs));

		case OP_BITOR_I:
			optres_constantarithmatic++;
			return QCC_MakeIntDef(G_INT(var_a->ofs) | G_INT(var_b->ofs));
		case OP_BITAND_I:
			optres_constantarithmatic++;
			return QCC_MakeIntDef(G_INT(var_a->ofs) & G_INT(var_b->ofs));
		case OP_MUL_I:
			optres_constantarithmatic++;
			return QCC_MakeIntDef(G_INT(var_a->ofs) * G_INT(var_b->ofs));
		case OP_DIV_I:
			optres_constantarithmatic++;
			return QCC_MakeIntDef(G_INT(var_a->ofs) / G_INT(var_b->ofs));
		case OP_ADD_I:
			optres_constantarithmatic++;
			return QCC_MakeIntDef(G_INT(var_a->ofs) + G_INT(var_b->ofs));
		case OP_SUB_I:
			optres_constantarithmatic++;
			return QCC_MakeIntDef(G_INT(var_a->ofs) - G_INT(var_b->ofs));
		}
	}

	switch (op - pr_opcodes)
	{
	case OP_AND:
		if (var_a->ofs == var_b->ofs)
			QCC_PR_ParseWarning(0, "Parameter offsets for && are the same");
		if (var_a->constant || var_b->constant)
			QCC_PR_ParseWarning(0, "Result of comparison is constant");
		break;
	case OP_OR:
		if (var_a->ofs == var_b->ofs)
			QCC_PR_ParseWarning(0, "Parameters for || are the same");
		if (var_a->constant || var_b->constant)
			QCC_PR_ParseWarning(0, "Result of comparison is constant");
		break;
	case OP_EQ_F:
	case OP_EQ_V:
	case OP_EQ_S:
	case OP_EQ_E:
	case OP_EQ_FNC:

	case OP_NE_F:
	case OP_NE_V:
	case OP_NE_S:
	case OP_NE_E:
	case OP_NE_FNC:

	case OP_LE:
	case OP_GE:
	case OP_LT:
	case OP_GT:
		if ((var_a->constant && var_b->constant && !var_a->temp && !var_b->temp) || var_a->ofs == var_b->ofs)
			QCC_PR_ParseWarning(0, "Result of comparison is constant");
		break;
	case OP_IFS:
	case OP_IFNOTS:
	case OP_IF:
	case OP_IFNOT:
//		if (var_a->type->type == ev_function && !var_a->temp)
//			QCC_PR_ParseWarning(0, "Result of comparison is constant");
		if (var_a->constant && !var_a->temp)
			QCC_PR_ParseWarning(0, "Result of comparison is constant");
		break;
	default:
		break;
	}

	if (numstatements)
	{	//optimise based on last statement.
		if (op - pr_opcodes == OP_IFNOT)
		{
			if (opt_shortenifnots && var_a && (statements[numstatements-1].op == OP_NOT_F || statements[numstatements-1].op == OP_NOT_FNC || statements[numstatements-1].op == OP_NOT_ENT))
			{
				if (statements[numstatements-1].c == var_a->ofs)
				{
					static QCC_def_t nvara;
					op = &pr_opcodes[OP_IF];
					numstatements--;
					QCC_FreeTemp(var_a);
					memcpy(&nvara, var_a, sizeof(nvara));
					nvara.ofs = statements[numstatements].a;
					var_a = &nvara;

					optres_shortenifnots++;
				}
			}
		}
		else if (op - pr_opcodes == OP_IFNOTS)
		{
			if (opt_shortenifnots && var_a && statements[numstatements-1].op == OP_NOT_S)
			{
				if (statements[numstatements-1].c == var_a->ofs)
				{
					static QCC_def_t nvara;
					op = &pr_opcodes[OP_IFS];
					numstatements--;
					QCC_FreeTemp(var_a);
					memcpy(&nvara, var_a, sizeof(nvara));
					nvara.ofs = statements[numstatements].a;
					var_a = &nvara;

					optres_shortenifnots++;
				}
			}
		}
		else if (((unsigned) ((op - pr_opcodes) - OP_STORE_F) < 6))
		{
			if (opt_assignments && var_a && var_a->ofs == statements[numstatements-1].c)// && var_a->ofs >RESERVED_OFS)
			{
				if (var_a->type->type == var_b->type->type)
				{
					if (var_a->temp)
					{
						statement = &statements[numstatements-1];
						statement->c = var_b->ofs;

						if (var_a->type->type != var_b->type->type)
							QCC_PR_ParseWarning(0, "store type mismatch");
						var_b->references++;
						var_a->references--;
						QCC_FreeTemp(var_a);
						optres_assignments++;

						simplestore=true;

						QCC_UnFreeTemp(var_b);
						return var_b;
					}
				}
			}
		}
	}
	simplestore=false;
	
	statement = &statements[numstatements];
	numstatements++;

	if (!QCC_OPCodeValid(op))
	{
		switch(op - pr_opcodes)
		{
		case OP_IFS:
			var_c = QCC_PR_GetDef(type_string, "string_null", NULL, true, 1);
			numstatements--;
			var_a = QCC_PR_Statement(&pr_opcodes[OP_NE_S], var_a, var_c, NULL);
			statement = &statements[numstatements];
			numstatements++;

			QCC_FreeTemp(var_a);
			op = &pr_opcodes[OP_IF];
			break;

		case OP_IFNOTS:
			var_c = QCC_PR_GetDef(type_string, "string_null", NULL, true, 1);
			numstatements--;
			var_a = QCC_PR_Statement(&pr_opcodes[OP_NE_S], var_a, var_c, NULL);
			statement = &statements[numstatements];
			numstatements++;

			QCC_FreeTemp(var_a);
			op = &pr_opcodes[OP_IFNOT];
			break;

		case OP_ADDSTORE_F:
			op = &pr_opcodes[OP_ADD_F];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;

		case OP_SUBSTORE_F:
			op = &pr_opcodes[OP_SUB_F];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;

		case OP_DIVSTORE_F:
			op = &pr_opcodes[OP_DIV_F];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;

		case OP_MULSTORE_F:
			op = &pr_opcodes[OP_MUL_F];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;

		case OP_ADDSTORE_V:
			op = &pr_opcodes[OP_ADD_V];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;

		case OP_SUBSTORE_V:
			op = &pr_opcodes[OP_SUB_V];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;

		case OP_MULSTORE_V:
			op = &pr_opcodes[OP_MUL_V];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;

		case OP_BITSET:
			op = &pr_opcodes[OP_BITOR];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;

		case OP_BITCLR:
			//b = var, a = bit field.

			QCC_UnFreeTemp(var_a);
			QCC_UnFreeTemp(var_b);

			numstatements--;
			var_c = QCC_PR_Statement(&pr_opcodes[OP_BITAND], var_b, var_a, NULL);
			QCC_FreeTemp(var_c);
			statement = &statements[numstatements];
			numstatements++;
			
			QCC_FreeTemp(var_a);
			QCC_FreeTemp(var_b);

			op = &pr_opcodes[OP_SUB_F];
			var_a = var_b;
			var_b = var_c;
			var_c = var_a;
			break;

		case OP_SUBSTOREP_F:
		case OP_ADDSTOREP_F:
		case OP_MULSTOREP_F:
		case OP_DIVSTOREP_F:
		case OP_BITSETP:
		case OP_BITCLRP:
//			QCC_PR_ParseWarning(0, "XSTOREP_F emulation is still experimental");
			QCC_UnFreeTemp(var_a);
			QCC_UnFreeTemp(var_b);
			//don't chain these... this expansion is not the same.
			{
				int st;

				for (st = numstatements-2; st>=0; st--)
				{
					if (statements[st].op == OP_ADDRESS)
						if (statements[st].c == var_b->ofs)
							break;

					if (statements[st].c == var_b->ofs)
						QCC_PR_ParseWarning(0, "Temp-reuse may have broken your %s\n", pr_opcodes);
				}
				if (st < 0)
					QCC_PR_ParseError(ERR_INTERNAL, "XSTOREP_F couldn't find pointer generation");
				var_c = QCC_GetTemp(*op->type_c);

				statement_linenums[statement-statements] = pr_source_line;
				statement->op = OP_LOAD_F;
				statement->a = statements[st].a;
				statement->b = statements[st].b;
				statement->c = var_c->ofs;
			}

			statement = &statements[numstatements];
			numstatements++;

			statement_linenums[statement-statements] = pr_source_line;
			switch(op - pr_opcodes)
			{
			case OP_SUBSTOREP_F:
				statement->op = OP_SUB_F;
				break;
			case OP_ADDSTOREP_F:
				statement->op = OP_ADD_F;
				break;
			case OP_MULSTOREP_F:
				statement->op = OP_MUL_F;
				break;
			case OP_DIVSTOREP_F:
				statement->op = OP_DIV_F;
				break;
			case OP_BITSETP:
				statement->op = OP_BITOR;
				break;
			case OP_BITCLRP:
				//float pointer float
				temp = QCC_GetTemp(type_float);
				statement->op = OP_BITAND;
				statement->a = var_c ? var_c->ofs : 0;
				statement->b = var_a ? var_a->ofs : 0;
				statement->c = temp->ofs;

				statement = &statements[numstatements];
				numstatements++;

				statement_linenums[statement-statements] = pr_source_line;
				statement->op = OP_SUB_F;

				//t = c & i
				//c = c - t
				break;
			default:	//no way will this be hit...
				QCC_PR_ParseError(ERR_INTERNAL, "opcode invalid 3 times %i", op - pr_opcodes);
			}
			if (op - pr_opcodes == OP_BITCLRP)
			{
				statement->a = var_c ? var_c->ofs : 0;
				statement->b = temp ? temp->ofs : 0;
				statement->c = var_c->ofs;
				QCC_FreeTemp(temp);
				var_b = var_b;	//this is the ptr.
				QCC_FreeTemp(var_a);
				var_a = var_c;	//this is the value.
			}
			else
			{
				statement->a = var_c ? var_c->ofs : 0;
				statement->b = var_a ? var_a->ofs : 0;
				statement->c = var_c->ofs;
				var_b = var_b;	//this is the ptr.
				QCC_FreeTemp(var_a);
				var_a = var_c;	//this is the value.
			}

			op = &pr_opcodes[OP_STOREP_F];
			QCC_FreeTemp(var_c);
			var_c = NULL;
			QCC_FreeTemp(var_b);

			statement = &statements[numstatements];
			numstatements++;
			break;

		case OP_MULSTOREP_V:
		case OP_SUBSTOREP_V:
		case OP_ADDSTOREP_V:
//			QCC_PR_ParseWarning(0, "XSTOREP_V emulation is still experimental");
			QCC_UnFreeTemp(var_a);
			QCC_UnFreeTemp(var_b);
			//don't chain these... this expansion is not the same.
			{
				int st;
				for (st = numstatements-2; st>=0; st--)
				{
					if (statements[st].op == OP_ADDRESS)
						if (statements[st].c == var_b->ofs)
							break;
				}
				if (st < 0)
					QCC_PR_ParseError(ERR_INTERNAL, "XSTOREP_V couldn't find pointer generation");
				var_c = QCC_GetTemp(*op->type_c);

				statement_linenums[statement-statements] = pr_source_line;
				statement->op = OP_LOAD_V;
				statement->a = statements[st].a;
				statement->b = statements[st].b;
				statement->c = var_c ? var_c->ofs : 0;
			}

			statement = &statements[numstatements];
			numstatements++;

			statement_linenums[statement-statements] = pr_source_line;
			switch(op - pr_opcodes)
			{
			case OP_SUBSTOREP_V:
				statement->op = OP_SUB_V;
				break;
			case OP_ADDSTOREP_V:
				statement->op = OP_ADD_V;
				break;
			case OP_MULSTOREP_V:
				statement->op = OP_MUL_V;
				break;
			default:	//no way will this be hit...
				QCC_PR_ParseError(ERR_INTERNAL, "opcode invalid 3 times %i", op - pr_opcodes);
			}
			statement->a = var_a ? var_a->ofs : 0;
			statement->b = var_c ? var_c->ofs : 0;
			QCC_FreeTemp(var_c);
			var_c = QCC_GetTemp(*op->type_c);
			statement->c = var_c ? var_c->ofs : 0;

			var_b = var_b;	//this is the ptr.
			QCC_FreeTemp(var_a);
			var_a = var_c;	//this is the value.
			op = &pr_opcodes[OP_STOREP_V];


			
			
			QCC_FreeTemp(var_c);
			var_c = NULL;
			QCC_FreeTemp(var_b);

			statement = &statements[numstatements];
			numstatements++;
			break;
		default:
			QCC_PR_ParseError(ERR_BADEXTENSION, "Opcode \"%s|%s\" not valid for target", op->name, op->opname);
			break;
		}
	}

	if (outstatement)
		*outstatement = statement;
	
	statement_linenums[statement-statements] = pr_source_line;
	statement->op = op - pr_opcodes;
	statement->a = var_a ? var_a->ofs : 0;
	statement->b = var_b ? var_b->ofs : 0;
	if (var_c != NULL)
	{
		statement->c = var_c->ofs;
	}
	else if (op->type_c == &type_void || op->associative==ASSOC_RIGHT || op->type_c == NULL)
	{
		var_c = NULL;
		statement->c = 0;			// ifs, gotos, and assignments
									// don't need vars allocated
	}
	else
	{	// allocate result space
		var_c = QCC_GetTemp(*op->type_c);
		statement->c = var_c->ofs;
		if (op->type_b == &type_field)
		{
			var_c->name = var_b->name;
			var_c->s_file = var_b->s_file;
			var_c->s_line = var_b->s_line;
		}
	}

	if (!var_c)
	{
		if (var_a)
			QCC_UnFreeTemp(var_a);
		return var_a;
	}
	return var_c;
}

/*
============
QCC_PR_SimpleStatement

Emits a primitive statement, returning the var it places it's value in
============
*/
QCC_dstatement_t *QCC_PR_SimpleStatement( int op, int var_a, int var_b, int var_c)
{
	QCC_dstatement_t	*statement;

	if (!QCC_OPCodeValid(pr_opcodes+op))
	{
//		outputversion = op->extension;
//		if (noextensions)
		QCC_PR_ParseError(ERR_BADEXTENSION, "Opcode \"%s|%s\" not valid for target\n", pr_opcodes[op].name, pr_opcodes[op].opname);
	}

	statement_linenums[numstatements] = pr_source_line;
	statement = &statements[numstatements];

	numstatements++;
	statement->op = op;
	statement->a = var_a;
	statement->b = var_b;
	statement->c = var_c;
	return statement;
}

void QCC_PR_Statement3 ( QCC_opcode_t *op, QCC_def_t *var_a, QCC_def_t *var_b, QCC_def_t *var_c)
{
	QCC_dstatement_t	*statement;	

	if (!QCC_OPCodeValid(op))
	{
//		outputversion = op->extension;
//		if (noextensions)
		QCC_PR_ParseError(ERR_BADEXTENSION, "Opcode \"%s|%s\" not valid for target\n", op->name, op->opname);
	}

	statement = &statements[numstatements];	
	numstatements++;
	
	statement_linenums[statement-statements] = pr_source_line;
	statement->op = op - pr_opcodes;
	statement->a = var_a ? var_a->ofs : 0;
	statement->b = var_b ? var_b->ofs : 0;
	statement->c = var_c ? var_c->ofs : 0;
}

/*
============
PR_ParseImmediate

Looks for a preexisting constant
============
*/
QCC_def_t	*QCC_PR_ParseImmediate (void)
{
	QCC_def_t	*cn;

	if (pr_immediate_type == type_float)
	{
		cn = QCC_MakeFloatDef(pr_immediate._float);
		QCC_PR_Lex ();
		return cn;
	}
	if (pr_immediate_type == type_integer)
	{
		cn = QCC_MakeIntDef(pr_immediate._int);
		QCC_PR_Lex ();
		return cn;
	}

	if (pr_immediate_type == type_string)
	{
		cn = QCC_MakeStringDef(pr_immediate_string);
		QCC_PR_Lex ();
		return cn;
	}

// check for a constant with the same value
	for (cn=pr.def_head.next ; cn ; cn=cn->next)	//FIXME - hashtable.
	{
		if (!cn->initialized)
			continue;
		if (!cn->constant)
			continue;
		if (cn->type != pr_immediate_type)
			continue;
		if (pr_immediate_type == type_string)
		{
			if (!STRCMP(G_STRING(cn->ofs), pr_immediate_string) )
			{
				QCC_PR_Lex ();
				return cn;
			}
		}
		else if (pr_immediate_type == type_float)
		{
			if ( G_FLOAT(cn->ofs) == pr_immediate._float )
			{
				QCC_PR_Lex ();
				return cn;
			}
		}
		else if (pr_immediate_type == type_integer)
		{			
			if ( G_INT(cn->ofs) == pr_immediate._int )
			{
				QCC_PR_Lex ();
				return cn;
			}
		}
		else if	(pr_immediate_type == type_vector)
		{
			if ( ( G_FLOAT(cn->ofs) == pr_immediate.vector[0] )
			&& ( G_FLOAT(cn->ofs+1) == pr_immediate.vector[1] )
			&& ( G_FLOAT(cn->ofs+2) == pr_immediate.vector[2] ) )
			{
				QCC_PR_Lex ();
				return cn;
			}
		}
		else			
			QCC_PR_ParseError (ERR_BADIMMEDIATETYPE, "weird immediate type");
	}

// allocate a new one
	cn = (void *)qccHunkAlloc (sizeof(QCC_def_t));
	cn->next = NULL;
	pr.def_tail->next = cn;
	pr.def_tail = cn;

	cn->type = pr_immediate_type;
	cn->name = "IMMEDIATE";
	cn->constant = true;
	cn->initialized = 1;
	cn->scope = NULL;		// always share immediates

// copy the immediate to the global area
	cn->ofs = QCC_GetFreeOffsetSpace(type_size[pr_immediate_type->type]);

	if (pr_immediate_type == type_string)
		pr_immediate.string = QCC_CopyString (pr_immediate_string);
	
	memcpy (qcc_pr_globals + cn->ofs, &pr_immediate, 4*type_size[pr_immediate_type->type]);
	
	QCC_PR_Lex ();

	return cn;
}


void QCC_PrecacheSound (QCC_def_t *e, int ch)
{
	char	*n;
	int		i;

	if (e->type->type != ev_string)
		return;
	
	if (!e->ofs || e->temp || !e->constant)
		return;
	n = G_STRING(e->ofs);
	if (!*n)
		return;
	for (i=0 ; i<numsounds ; i++)
		if (!STRCMP(n, precache_sounds[i]))
			return;
	if (numsounds == MAX_SOUNDS)
		return;
//		QCC_Error ("PrecacheSound: numsounds == MAX_SOUNDS");
	strcpy (precache_sounds[i], n);
	if (ch >= '1'  && ch <= '9')
		precache_sounds_block[i] = ch - '0';
	else
		precache_sounds_block[i] = 1;
	numsounds++;
}

void QCC_PrecacheModel (QCC_def_t *e, int ch)
{
	char	*n;
	int		i;

	if (e->type->type != ev_string)
		return;
	
	if (!e->ofs || e->temp || !e->constant)
		return;	
	n = G_STRING(e->ofs);
	if (!*n)
		return;
	for (i=0 ; i<nummodels ; i++)
		if (!STRCMP(n, precache_models[i]))
		{
			if (!precache_models_block[i])
			{
				if (ch >= '1'  && ch <= '9')
					precache_models_block[i] = ch - '0';
				else
					precache_models_block[i] = 1;
			}
			return;
		}
	if (nummodels == MAX_MODELS)
		return;
//		QCC_Error ("PrecacheModels: nummodels == MAX_MODELS");
	strcpy (precache_models[i], n);
	if (ch >= '1'  && ch <= '9')
		precache_models_block[i] = ch - '0';
	else
		precache_models_block[i] = 1;
	nummodels++;
}

void QCC_SetModel (QCC_def_t *e)
{
	char	*n;
	int		i;

	if (e->type->type != ev_string)
		return;
	
	if (!e->ofs || e->temp || !e->constant)
		return;	
	n = G_STRING(e->ofs);
	if (!*n)
		return;
	for (i=0 ; i<nummodels ; i++)
		if (!STRCMP(n, precache_models[i]))
		{
			precache_models_used[i]++;
			return;
		}
	if (nummodels == MAX_MODELS)
		return;
	strcpy (precache_models[i], n);
	precache_models_block[i] = 0;
	precache_models_used[i]=1;
	nummodels++;
}

void QCC_PrecacheTexture (QCC_def_t *e, int ch)
{
	char	*n;
	int		i;

	if (e->type->type != ev_string)
		return;
	
	if (!e->ofs || e->temp || !e->constant)
		return;
	n = G_STRING(e->ofs);
	if (!*n)
		return;
	for (i=0 ; i<numtextures ; i++)
		if (!STRCMP(n, precache_textures[i]))
			return;
	if (nummodels == MAX_MODELS)
		return;
//		QCC_Error ("PrecacheTextures: numtextures == MAX_TEXTURES");
	strcpy (precache_textures[i], n);
	if (ch >= '1'  && ch <= '9')
		precache_textures_block[i] = ch - '0';
	else
		precache_textures_block[i] = 1;
	numtextures++;
}

void QCC_PrecacheFile (QCC_def_t *e, int ch)
{
	char	*n;
	int		i;

	if (e->type->type != ev_string)
		return;
	
	if (!e->ofs || e->temp || !e->constant)
		return;
	n = G_STRING(e->ofs);
	if (!*n)
		return;
	for (i=0 ; i<numfiles ; i++)
		if (!STRCMP(n, precache_files[i]))
			return;
	if (numfiles == MAX_FILES)
		return;
//		QCC_Error ("PrecacheFile: numfiles == MAX_FILES");
	strcpy (precache_files[i], n);
	if (ch >= '1'  && ch <= '9')
		precache_files_block[i] = ch - '0';
	else
		precache_files_block[i] = 1;
	numfiles++;
}

void QCC_PrecacheFileOptimised (char *n, int ch)
{
	int		i;

	for (i=0 ; i<numfiles ; i++)
		if (!STRCMP(n, precache_files[i]))
			return;
	if (numfiles == MAX_FILES)
		return;
//		QCC_Error ("PrecacheFile: numfiles == MAX_FILES");
	strcpy (precache_files[i], n);
	if (ch >= '1'  && ch <= '9')
		precache_files_block[i] = ch - '0';
	else
		precache_files_block[i] = 1;
	numfiles++;
}

/*
============
PR_ParseFunctionCall
============
*/
QCC_def_t *QCC_PR_ParseFunctionCall (QCC_def_t *func)	//warning, the func could have no name set if it's a field call.
{
	QCC_def_t		*e, *d, *old, *oself;
	int			arg, i;
	QCC_type_t		*t, *p;
	int extraparms=false;
	int np;
	int laststatement = numstatements;

	int callconvention;
	QCC_dstatement_t *st;

	QCC_def_t *param[MAX_PARMS+MAX_EXTRA_PARMS];

	func->timescalled++;

	if (QCC_OPCodeValid(&pr_opcodes[OP_CALL1H]))
		callconvention = OP_CALL1H;	//FTE extended
	else
		callconvention = OP_CALL1;	//standard

	t = func->type;

	if (t->type != ev_function)
	{
		QCC_PR_ParseErrorPrintDef (ERR_NOTAFUNCTION, func, "not a function");
	}

	if (!t->num_parms)	//intrinsics. These base functions have variable arguments. I would check for (...) args too, but that might be used for extended builtin functionality. (this code wouldn't compile otherwise)
	{
		if (!strcmp(func->name, "random"))
		{
			func->references++;
			if (!QCC_PR_Check(")"))
			{
				e = QCC_PR_Expression (TOP_PRIORITY);
				if (e->type->type != ev_float)
					QCC_PR_ParseErrorPrintDef (ERR_TYPEMISMATCHPARM, func, "type mismatch on parm %i", 1);
				if (!QCC_PR_Check(")"))
				{
					QCC_PR_Expect(",");
					d = QCC_PR_Expression (TOP_PRIORITY);
					if (d->type->type != ev_float)
						QCC_PR_ParseErrorPrintDef (ERR_TYPEMISMATCHPARM, func, "type mismatch on parm %i", 2);
					QCC_PR_Expect(")");
				}
				else
					d = NULL;
			}
			else
			{
				e = NULL;
				d = NULL;
			}


			if (def_ret.temp->used)
			{
				old = QCC_GetTemp(def_ret.type);
				if (def_ret.type->size == 3)
					QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_V], &def_ret, old, NULL));
				else
					QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_F], &def_ret, old, NULL));
				QCC_PR_ParseWarning(WARN_FIXEDRETURNVALUECONFLICT, "Return value conflict - output is inefficient");
			}
			else
				old = NULL;

			if (QCC_OPCodeValid(&pr_opcodes[OP_RAND0]))
			{
				if (e)
				{
					if (d)
						QCC_PR_SimpleStatement(OP_RAND2, e->ofs, d->ofs, OFS_RETURN);
					else
						QCC_PR_SimpleStatement(OP_RAND1, e->ofs, 0, OFS_RETURN);
				}
				else
					QCC_PR_SimpleStatement(OP_RAND0, 0, 0, OFS_RETURN);
			}
			else
			{
				if (e)
				{
					if (d)
					{
						QCC_dstatement_t *st;
						QCC_def_t *t;
						QCC_PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0);

						if ((!d->constant || !e->constant) && G_FLOAT(d->ofs) >= G_FLOAT(d->ofs))
						{
							t = QCC_PR_Statement(&pr_opcodes[OP_GT], d, e, NULL);
							QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_IFNOT], t, 0, &st));
							st->b = 3;

							t = QCC_PR_Statement(&pr_opcodes[OP_SUB_F], d, e, NULL);
							QCC_PR_SimpleStatement(OP_MUL_F, OFS_RETURN, t->ofs, OFS_RETURN);
							QCC_FreeTemp(t);
							QCC_PR_SimpleStatement(OP_ADD_F, OFS_RETURN, e->ofs, OFS_RETURN);

							QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_GOTO], 0, 0, &st));
							st->a = 3;
						}
						
						t = QCC_PR_Statement(&pr_opcodes[OP_SUB_F], e, d, NULL);
						QCC_PR_SimpleStatement(OP_MUL_F, OFS_RETURN, t->ofs, OFS_RETURN);
						QCC_FreeTemp(t);
						QCC_PR_SimpleStatement(OP_ADD_F, OFS_RETURN, d->ofs, OFS_RETURN);
					}
					else
					{
						QCC_PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0);
						QCC_PR_SimpleStatement(OP_MUL_F, OFS_RETURN, e->ofs, OFS_RETURN);
					}
				}
				else
					QCC_PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0);
			}

			if (e)
			{
				QCC_FreeTemp(e);
				e->references++;
			}
			if (d)
			{
				d->references++;
				QCC_FreeTemp(d);
			}

			if (old)
			{
				d = QCC_GetTemp(type_float);
				QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_STORE_F, &def_ret, d, NULL));
				if (def_ret.type->size == 3)
					QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_STORE_V, old, &def_ret, NULL));
				else
					QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_STORE_F, old, &def_ret, NULL));
				QCC_FreeTemp(old);

				return d;
			}

			if (def_ret.temp->used)
				QCC_PR_ParseWarning(0, "Return value conflict - output is likly to be invalid");
			def_ret.temp->used = true;
			def_ret.type = type_float;
			return &def_ret;

		}
		if (!strcmp(func->name, "randomv"))
		{
			func->references++;
			if (!QCC_PR_Check(")"))
			{
				e = QCC_PR_Expression (TOP_PRIORITY);
				if (e->type->type != ev_vector)
						QCC_PR_ParseErrorPrintDef (ERR_TYPEMISMATCHPARM, func, "type mismatch on parm %i", 1);
				if (!QCC_PR_Check(")"))
				{
					QCC_PR_Expect(",");
					d = QCC_PR_Expression (TOP_PRIORITY);
					if (d->type->type != ev_vector)
						QCC_PR_ParseErrorPrintDef (ERR_TYPEMISMATCHPARM, func, "type mismatch on parm %i", 2);
					QCC_PR_Expect(")");
				}
				else
					d = NULL;
			}
			else
			{
				e = NULL;
				d = NULL;
			}


			if (def_ret.temp->used)
			{
				old = QCC_GetTemp(def_ret.type);
				if (def_ret.type->size == 3)
					QCC_PR_Statement(&pr_opcodes[OP_STORE_V], &def_ret, old, NULL);
				else
					QCC_PR_Statement(&pr_opcodes[OP_STORE_F], &def_ret, old, NULL);
				QCC_PR_ParseWarning(WARN_FIXEDRETURNVALUECONFLICT, "Return value conflict - output is inefficient");
			}
			else
				old = NULL;

			if (QCC_OPCodeValid(&pr_opcodes[OP_RANDV0]))
			{
				if (e)
				{
					if (d)
						QCC_PR_SimpleStatement(OP_RANDV2, e->ofs, d->ofs, OFS_RETURN);
					else
						QCC_PR_SimpleStatement(OP_RANDV1, e->ofs, 0, OFS_RETURN);
				}
				else
					QCC_PR_SimpleStatement(OP_RANDV0, 0, 0, OFS_RETURN);
			}
			else
			{
				if (e)
				{
					if (d)
					{
						QCC_def_t *t;
						QCC_PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0);

						if ((!d->constant || !e->constant) && G_FLOAT(d->ofs) >= G_FLOAT(d->ofs))
						{
							t = QCC_GetTemp(type_float);
							QCC_PR_SimpleStatement(OP_GT, d->ofs+2, e->ofs+2, t->ofs);
							QCC_PR_SimpleStatement(OP_IFNOT, t->ofs, 3, 0);

							QCC_PR_SimpleStatement(OP_SUB_F, d->ofs+2, e->ofs+2, t->ofs);
							QCC_PR_SimpleStatement(OP_MUL_F, OFS_RETURN, t->ofs, OFS_RETURN+2);
							QCC_FreeTemp(t);
							QCC_PR_SimpleStatement(OP_ADD_F, OFS_RETURN, e->ofs+2, OFS_RETURN+2);

							QCC_PR_SimpleStatement(OP_GOTO, 3, 0, 0);
						}
						
						t = QCC_GetTemp(type_float);
						QCC_PR_SimpleStatement(OP_SUB_F, d->ofs+2, e->ofs+2, t->ofs);
						QCC_PR_SimpleStatement(OP_MUL_F, OFS_RETURN, t->ofs, OFS_RETURN+2);
						QCC_FreeTemp(t);
						QCC_PR_SimpleStatement(OP_ADD_F, OFS_RETURN, d->ofs+2, OFS_RETURN+2);

						
						
						QCC_PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0);

						if ((!d->constant || !e->constant) && G_FLOAT(d->ofs) >= G_FLOAT(d->ofs))
						{
							t = QCC_GetTemp(type_float);
							QCC_PR_SimpleStatement(OP_GT, d->ofs+1, e->ofs+1, t->ofs);
							QCC_PR_SimpleStatement(OP_IFNOT, t->ofs, 3, 0);

							QCC_PR_SimpleStatement(OP_SUB_F, d->ofs+1, e->ofs+1, t->ofs);
							QCC_PR_SimpleStatement(OP_MUL_F, OFS_RETURN, t->ofs, OFS_RETURN+1);
							QCC_FreeTemp(t);
							QCC_PR_SimpleStatement(OP_ADD_F, OFS_RETURN, e->ofs+1, OFS_RETURN+1);

							QCC_PR_SimpleStatement(OP_GOTO, 3, 0, 0);
						}
						
						t = QCC_GetTemp(type_float);
						QCC_PR_SimpleStatement(OP_SUB_F, d->ofs+1, e->ofs+1, t->ofs);
						QCC_PR_SimpleStatement(OP_MUL_F, OFS_RETURN, t->ofs, OFS_RETURN+1);
						QCC_FreeTemp(t);
						QCC_PR_SimpleStatement(OP_ADD_F, OFS_RETURN, d->ofs+1, OFS_RETURN+1);


						QCC_PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0);

						if ((!d->constant || !e->constant) && G_FLOAT(d->ofs) >= G_FLOAT(d->ofs))
						{
							t = QCC_GetTemp(type_float);
							QCC_PR_SimpleStatement(OP_GT, d->ofs, e->ofs, t->ofs);
							QCC_PR_SimpleStatement(OP_IFNOT, t->ofs, 3, 0);

							QCC_PR_SimpleStatement(OP_SUB_F, d->ofs, e->ofs, t->ofs);
							QCC_PR_SimpleStatement(OP_MUL_F, OFS_RETURN, t->ofs, OFS_RETURN);
							QCC_FreeTemp(t);
							QCC_PR_SimpleStatement(OP_ADD_F, OFS_RETURN, e->ofs, OFS_RETURN);

							QCC_PR_SimpleStatement(OP_GOTO, 3, 0, 0);
						}
						
						t = QCC_GetTemp(type_float);
						QCC_PR_SimpleStatement(OP_SUB_F, d->ofs, e->ofs, t->ofs);
						QCC_PR_SimpleStatement(OP_MUL_F, OFS_RETURN, t->ofs, OFS_RETURN);
						QCC_FreeTemp(t);
						QCC_PR_SimpleStatement(OP_ADD_F, OFS_RETURN, d->ofs, OFS_RETURN);
					}
					else
					{
						QCC_PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0);
						QCC_PR_SimpleStatement(OP_MUL_F, OFS_RETURN, e->ofs, OFS_RETURN+2);
						QCC_PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0);
						QCC_PR_SimpleStatement(OP_MUL_F, OFS_RETURN, e->ofs, OFS_RETURN+1);
						QCC_PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0);
						QCC_PR_SimpleStatement(OP_MUL_F, OFS_RETURN, e->ofs, OFS_RETURN);
					}
				}
				else
				{
					QCC_PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0);
					QCC_PR_SimpleStatement(OP_STORE_F, OFS_RETURN, OFS_RETURN+2, 0);
					QCC_PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0);
					QCC_PR_SimpleStatement(OP_STORE_F, OFS_RETURN, OFS_RETURN+1, 0);
					QCC_PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0);
				}
			}


			if (e)
			{
				QCC_FreeTemp(e);
				e->references++;
			}
			if (d)
			{
				d->references++;
				QCC_FreeTemp(d);
			}

			if (old)
			{
				d = QCC_GetTemp(type_vector);
				QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_STORE_V, &def_ret, d, NULL));
				if (def_ret.type->size == 3)
				{
					QCC_PR_Statement(pr_opcodes+OP_STORE_V, old, &def_ret, NULL);
				}
				else
				{
					QCC_PR_Statement(pr_opcodes+OP_STORE_F, old, &def_ret, NULL);
				}
				QCC_FreeTemp(old);

				return d;
			}

			if (def_ret.temp->used)
				QCC_PR_ParseWarning(0, "Return value conflict - output is likly to be invalid");
			def_ret.temp->used = true;
			def_ret.type = type_vector;
			return &def_ret;
		}
		else if (!strcmp(func->name, "spawn"))
		{
			QCC_type_t *rettype;
			if (QCC_PR_Check(")"))
			{
				rettype = type_entity;
			}
			else
			{
				rettype = QCC_TypeForName(QCC_PR_ParseName());
				if (!rettype || rettype->type != ev_entity)
					QCC_PR_ParseError(ERR_NOTANAME, "Spawn operator with undefined class");

				QCC_PR_Expect(")");
			}

			if (def_ret.temp->used)
				QCC_PR_ParseWarning(0, "Return value conflict - output is likly to be invalid");
			def_ret.temp->used = true;

			if (rettype != type_entity)
			{
				char genfunc[2048];
				sprintf(genfunc, "Class*%s", rettype->name);
				func = QCC_PR_GetDef(type_function, genfunc, NULL, true, 1);
				func->references++;
			}
			QCC_PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0);
			def_ret.type = rettype;
			return &def_ret;
		}
	}

	if (opt_precache_file)
	{
		if (!strncmp(func->name,"precache_file", 13))
		{
			if (pr_token_type == tt_immediate && pr_immediate_type->type == ev_string)
			{
				optres_precache_file += strlen(pr_immediate_string);
				QCC_PR_Lex();
				QCC_PR_Expect(")");
				QCC_PrecacheFileOptimised (pr_immediate_string, func->name[13]);
				def_ret.type = type_void;
				return &def_ret;
			}
		}
	}

	QCC_LockActiveTemps();	//any temps before are likly to be used with the return value.

	//any temps referenced to build the parameters don't need to be locked.

// copy the arguments to the global parameter variables
	arg = 0;
	if (t->num_parms < 0)
	{
		extraparms = true;
		np = (t->num_parms * -1) - 1;
	}
	else
		np = t->num_parms;

	
	if (!QCC_PR_Check(")"))
	{
		p = t->param;
		do
		{
			if (extraparms && arg >= MAX_PARMS)
				QCC_PR_ParseErrorPrintDef (ERR_TOOMANYPARAMETERSVARARGS, func, "More than %i parameters on varargs function", MAX_PARMS);
			else if (arg >= MAX_PARMS+MAX_EXTRA_PARMS)
				QCC_PR_ParseErrorPrintDef (ERR_TOOMANYTOTALPARAMETERS, func, "More than %i parameters", MAX_PARMS+MAX_EXTRA_PARMS);
			if (!extraparms && arg >= t->num_parms)
			{
				QCC_PR_ParseWarning (WARN_TOOMANYPARAMETERSFORFUNC, "too many parameters");
				QCC_PR_ParsePrintDef(WARN_TOOMANYPARAMETERSFORFUNC, func);
			}

			e = QCC_PR_Expression (TOP_PRIORITY);

			if (arg == 0 && func->name)
			{
			// save information for model and sound caching
				if (!strncmp(func->name,"precache_", 9))
				{
					if (!strncmp(func->name+9,"sound", 5))
						QCC_PrecacheSound (e, func->name[14]);
					else if (!strncmp(func->name+9,"model", 5))
						QCC_PrecacheModel (e, func->name[14]);
					else if (!strncmp(func->name+9,"texture", 7))
						QCC_PrecacheTexture (e, func->name[16]);
					else if (!strncmp(func->name+9,"file", 4))
						QCC_PrecacheFile (e, func->name[13]);
				}
			}

			if (arg>=MAX_PARMS)
			{
				if (!extra_parms[arg - MAX_PARMS])
				{
					d = (QCC_def_t *) qccHunkAlloc (sizeof(QCC_def_t));
					d->name = "extra parm";
					d->ofs = QCC_GetFreeOffsetSpace (3);
					extra_parms[arg - MAX_PARMS] = d;
				}
				d = extra_parms[arg - MAX_PARMS];
			}
			else
				d = &def_parms[arg];

			if (pr_classtype && e->type->type == ev_field && p->type != ev_field)
			{	//convert.
				oself = QCC_PR_GetDef(type_entity, "self", NULL, true, 1);
				switch(e->type->aux_type->type)
				{
				case ev_string:
					e = QCC_PR_Statement(pr_opcodes+OP_LOAD_S, oself, e, NULL);
					break;
				case ev_integer:
					e = QCC_PR_Statement(pr_opcodes+OP_LOAD_I, oself, e, NULL);
					break;
				case ev_float:
					e = QCC_PR_Statement(pr_opcodes+OP_LOAD_F, oself, e, NULL);
					break;
				case ev_function:
					e = QCC_PR_Statement(pr_opcodes+OP_LOAD_FNC, oself, e, NULL);
					break;
				case ev_vector:
					e = QCC_PR_Statement(pr_opcodes+OP_LOAD_V, oself, e, NULL);
					break;
				case ev_entity:
					e = QCC_PR_Statement(pr_opcodes+OP_LOAD_ENT, oself, e, NULL);
					break;
				default:
					QCC_Error(ERR_INTERNAL, "Bad member type. Try forced expansion");
				}
			}

			if (p)
			{
				if (typecmp(e->type, p))
				/*if (e->type->type != ev_integer && p->type != ev_function)
				if (e->type->type != ev_function && p->type != ev_integer)
				if ( e->type->type != p->type )*/
				{
					if (p->type == ev_integer && e->type->type == ev_float)	//convert float -> int... is this a constant?
						e = QCC_PR_Statement(pr_opcodes+OP_CONV_FTOI, e, NULL, NULL);
					else if (p->type == ev_float && e->type->type == ev_integer)	//convert float -> int... is this a constant?
						e = QCC_PR_Statement(pr_opcodes+OP_CONV_ITOF, e, NULL, NULL);
					else
						QCC_PR_ParseErrorPrintDef (ERR_TYPEMISMATCHPARM, func, "type mismatch on parm %i - (%s should be %s)", arg+1, TypeName(e->type), TypeName(p));
				}

				d->type = p;

				p=p->next;
			}
		// a vector copy will copy everything
			else
				d->type = type_void;

			if (arg == 1 && !STRCMP(func->name, "setmodel"))
			{
				QCC_SetModel(e);
			}

			param[arg] = e;
/*			if (e->type->size>1)
				QCC_PR_Statement (&pr_opcodes[OP_STORE_V], e, d, (QCC_dstatement_t **)0xffffffff);
			else
				QCC_PR_Statement (&pr_opcodes[OP_STORE_F], e, d, (QCC_dstatement_t **)0xffffffff);
				*/
			arg++;
		} while (QCC_PR_Check (","));

		if (t->num_parms != -1 && arg < np)
			QCC_PR_ParseWarning (WARN_TOOFEWPARAMS, "too few parameters on call to %s", func->name);
		QCC_PR_Expect (")");
	}
	else if (np)
	{
		QCC_PR_ParseWarning (WARN_TOOFEWPARAMS, "%s: Too few parameters", func->name);
		QCC_PR_ParsePrintDef (WARN_TOOFEWPARAMS, func);
	}

//	qcc_functioncalled++;
	for (i = 0; i < arg; i++)
	{
		if (i>=MAX_PARMS)
			d = extra_parms[i - MAX_PARMS];
		else
			d = &def_parms[i];

		if (callconvention == OP_CALL1H)
			if (i < 2)
			{
				param[i]->references++;
				d->references++;
				QCC_FreeTemp(param[i]);
				continue;
			}

		if (param[i]->type->size>1 || !opt_nonvec_parms)
			QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_STORE_V], param[i], d, (QCC_dstatement_t **)0xffffffff));
		else
		{
			d->type = param[i]->type;
			QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_STORE_F], param[i], d, (QCC_dstatement_t **)0xffffffff));
			optres_nonvec_parms++;
		}
	}

	if (def_ret.temp->used)
	{
		old = QCC_GetTemp(def_ret.type);
		if (def_ret.type->size == 3)
			QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_V], &def_ret, old, NULL));
		else
			QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_F], &def_ret, old, NULL));
		QCC_UnFreeTemp(old);
		QCC_UnFreeTemp(&def_ret);
		QCC_PR_ParseWarning(WARN_FIXEDRETURNVALUECONFLICT, "Return value conflict - output is inefficient");
	}
	else
		old = NULL;

	if (strchr(func->name, ':') && laststatement && statements[laststatement-1].op == OP_LOAD_FNC && statements[laststatement-1].c == func->ofs)
	{	//we're entering C++ code with a different self.

		//FIXME: problems could occur with hexen2 calling conventions when parm0/1 is 'self'
		//thiscall. copy the right ent into 'self' (if it's not the same offset)
		d = QCC_PR_GetDef(type_entity, "self", NULL, true, 1);
		if (statements[laststatement-1].a != d->ofs)
		{
			oself = QCC_GetTemp(type_entity);
			QCC_PR_SimpleStatement(OP_STORE_ENT, d->ofs, oself->ofs, 0);
			QCC_PR_SimpleStatement(OP_STORE_ENT, statements[laststatement-1].a, d->ofs, 0);

			if (callconvention == OP_CALL1H)	//other.function(self)
												//hexenc calling convention would mean that the
												//passed parameter is essentually (self=other),
												//so pass oself instead which won't be affected
			{
				QCC_def_t *temp;
				if (arg>=1 && param[0]->ofs == d->ofs)
				{
					temp = QCC_GetTemp(type_entity);
					QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_STORE_ENT, oself, temp, NULL));
					QCC_UnFreeTemp(temp);
					param[0] = temp;
				}
				if (arg>=2 && param[1]->ofs == d->ofs)
				{
					temp = QCC_GetTemp(type_entity);
					QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_STORE_ENT, oself, temp, NULL));
					QCC_UnFreeTemp(temp);
					param[1] = temp;
				}
			}
		}
		else
		{
			oself = NULL;
			d = NULL;
		}
	}
	else
	{
		oself = NULL;
		d = NULL;
	}

	if (arg>MAX_PARMS)
		QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[callconvention-1+MAX_PARMS], func, 0, (QCC_dstatement_t **)&st));
	else if (arg)
		QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[callconvention-1+arg], func, 0, (QCC_dstatement_t **)&st));
	else
		QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_CALL0], func, 0, (QCC_dstatement_t **)&st));

	if (callconvention == OP_CALL1H)
	{
		if (arg)
		{
			st->b = param[0]->ofs;
//			QCC_FreeTemp(param[0]);
			if (arg>1)
			{
				st->c = param[1]->ofs;
//				QCC_FreeTemp(param[1]);
			}
		}
	}
	if (oself)
		QCC_PR_SimpleStatement(OP_STORE_ENT, oself->ofs, d->ofs, 0);

	for(; arg; arg--)
	{
		QCC_FreeTemp(param[arg-1]);
	}

	if (old)
	{
		d = QCC_GetTemp(t->aux_type);
		if (t->aux_type->size == 3)
			QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_STORE_V, &def_ret, d, NULL));
		else
			QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_STORE_F, &def_ret, d, NULL));
		if (def_ret.type->size == 3)
			QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_STORE_V, old, &def_ret, NULL));
		else
			QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_STORE_F, old, &def_ret, NULL));
		QCC_FreeTemp(old);
		QCC_UnFreeTemp(&def_ret);
		QCC_UnFreeTemp(d);

		return d;
	}
	
	def_ret.type = t->aux_type;
	if (def_ret.temp->used)
		QCC_PR_ParseWarning(WARN_FIXEDRETURNVALUECONFLICT, "Return value conflict - output is inefficient");
	def_ret.temp->used = true;

	return &def_ret;
}

int constchecks;
int varchecks;
int typechecks;
QCC_def_t *QCC_MakeIntDef(int value)
{
	QCC_def_t	*cn;
	
// check for a constant with the same value
	for (cn=pr.def_head.next ; cn ; cn=cn->next)
	{
		varchecks++;
		if (!cn->initialized)
			continue;
		if (!cn->constant)
			continue;
		constchecks++;
		if (cn->type != type_integer)
			continue;
		typechecks++;

		if ( G_INT(cn->ofs) == value )
		{				
			return cn;
		}	
	}

// allocate a new one
	cn = (void *)qccHunkAlloc (sizeof(QCC_def_t));
	cn->next = NULL;
	pr.def_tail->next = cn;
	pr.def_tail = cn;

	cn->type = type_integer;
	cn->name = "IMMEDIATE";
	cn->constant = true;
	cn->initialized = 1;
	cn->scope = NULL;		// always share immediates
	cn->arraysize = 1;

// copy the immediate to the global area
	cn->ofs = QCC_GetFreeOffsetSpace (type_size[type_integer->type]);
	
	G_INT(cn->ofs) = value;	
		

	return cn;
}

hashtable_t floatconstdefstable;
QCC_def_t *QCC_MakeFloatDef(float value)
{
	QCC_def_t	*cn;

	union {
		float f;
		int i;
	} fi;

	fi.f = value;

	cn = Hash_GetKey(&floatconstdefstable, fi.i);
	if (cn)
		return cn;

// allocate a new one
	cn = (void *)qccHunkAlloc (sizeof(QCC_def_t));
	cn->next = NULL;
	pr.def_tail->next = cn;
	pr.def_tail = cn;

	cn->type = type_float;
	cn->name = "IMMEDIATE";
	cn->constant = true;
	cn->initialized = 1;
	cn->scope = NULL;		// always share immediates
	cn->arraysize = 1;

// copy the immediate to the global area
	cn->ofs = QCC_GetFreeOffsetSpace (type_size[type_integer->type]);
	
	Hash_AddKey(&floatconstdefstable, fi.i, cn);
	
	G_FLOAT(cn->ofs) = value;	
		

	return cn;
}

hashtable_t stringconstdefstable;
QCC_def_t *QCC_MakeStringDef(char *value)
{
	QCC_def_t	*cn;
	int string;

	cn = Hash_Get(&stringconstdefstable, value);
	if (cn)
		return cn;

// allocate a new one
	cn = (void *)qccHunkAlloc (sizeof(QCC_def_t));
	cn->next = NULL;
	pr.def_tail->next = cn;
	pr.def_tail = cn;

	cn->type = type_string;
	cn->name = "IMMEDIATE";
	cn->constant = true;
	cn->initialized = 1;
	cn->scope = NULL;		// always share immediates
	cn->arraysize = 1;

// copy the immediate to the global area
	cn->ofs = QCC_GetFreeOffsetSpace (type_size[type_integer->type]);
	
	string = QCC_CopyString (value);

	Hash_Add(&stringconstdefstable, strings+string, cn);
	
	G_INT(cn->ofs) = string;	
		

	return cn;
}

QCC_type_t *QCC_PR_NewType (char *name, int basictype);
QCC_type_t *QCC_PointerTypeTo(QCC_type_t *type)
{
	QCC_type_t *newtype;
	newtype = QCC_PR_NewType("POINTER TYPE", ev_pointer);
	newtype->aux_type = type;
	return newtype;
}

int basictypefield[ev_union+1];
char *basictypenames[] = {
	"void",
	"string",
	"float",
	"vector",
	"entity",
	"field",
	"function",
	"pointer",
	"integer",
	"struct",
	"union"
};

QCC_def_t *QCC_MemberInParentClass(char *name, QCC_type_t *clas)
{	//if a member exists, return the member field (rather than mapped-to field)
	QCC_type_t *mt;
	QCC_def_t *def;
	int p, np;
	char membername[2048];

	if (!clas)
	{
		def = QCC_PR_GetDef(NULL, name, NULL, 0, 0);
		if (def && def->type->type == ev_field)	//the member existed as a normal entity field.
			return def;
		return NULL;
	}

	np = clas->num_parms;
	for (p = 0, mt = clas->param; p < np; p++, mt = mt->next)
	{
		if (strcmp(mt->name, name))
			continue;

		//the parent has it.

		sprintf(membername, "%s::"MEMBERFIELDNAME, clas->name, mt->name);
		def = QCC_PR_GetDef(NULL, membername, NULL, false, 0);
		return def;
	}

	return QCC_MemberInParentClass(name, clas->parentclass);
}

//create fields for the types, instanciate the members to the fields.
//we retouch the parents each time to guarentee polymorphism works.
//FIXME: virtual methods will not work properly. Need to trace down to see if a parent already defined it
void QCC_PR_EmitFieldsForMembers(QCC_type_t *clas)
{
	char membername[2048];
	int p, np, a;
	unsigned int o;
	QCC_type_t *mt, *ft;
	QCC_def_t *f, *m;
	if (clas->parentclass != type_entity)	//parents MUST have all thier fields set or inheritance would go crazy.
		QCC_PR_EmitFieldsForMembers(clas->parentclass);

	np = clas->num_parms;
	mt = clas->param;
	for (p = 0; p < np; p++, mt = mt->next)
	{
		sprintf(membername, "%s::"MEMBERFIELDNAME, clas->name, mt->name);
		m = QCC_PR_GetDef(NULL, membername, NULL, false, 0);

		f = QCC_MemberInParentClass(mt->name, clas->parentclass);
		if (f)
		{
			if (m->arraysize>1)
				QCC_Error(ERR_INTERNAL, "FTEQCC does not support overloaded arrays of members");
			a=0;
			for (o = 0; o < m->type->size; o++)
				((int *)qcc_pr_globals)[o+a*mt->size+m->ofs] = ((int *)qcc_pr_globals)[o+f->ofs];
			continue;
		}

		for (a = 0; a < m->arraysize; a++)
		{
			//we need the type in here so saved games can still work without saving ints as floats. (would be evil)
			ft = QCC_PR_NewType(basictypenames[mt->type], ev_field);
			ft->aux_type = QCC_PR_NewType(basictypenames[mt->type], mt->type);
			ft->aux_type->aux_type = type_void;
			ft = QCC_PR_FindType(ft);
			sprintf(membername, "__f_%s_%i", ft->name, ++basictypefield[mt->type]);
			f = QCC_PR_GetDef(ft, membername, NULL, true, 1);
		
			for (o = 0; o < m->type->size; o++)
				((int *)qcc_pr_globals)[o+a*mt->size+m->ofs] = ((int *)qcc_pr_globals)[o+f->ofs];

			f->references++;
		}
	}
}

void QCC_PR_EmitClassFunctionTable(QCC_type_t *clas, QCC_type_t *childclas, QCC_def_t *ed, QCC_def_t **constructor)
{	//go through clas, do the virtual thing only if the child class does not override.

	char membername[2048];
	QCC_type_t *type;
	QCC_type_t *oc;
	int p;

	QCC_def_t *point, *member;
	QCC_def_t *virt;

	if (clas->parentclass)
		QCC_PR_EmitClassFunctionTable(clas->parentclass, childclas, ed, constructor);

	type = clas->param;
	for (p = 0; p < clas->num_parms; p++, type = type->next)
	{
		for (oc = childclas; oc != clas; oc = oc->parentclass)
		{
			sprintf(membername, "%s::"MEMBERFIELDNAME, oc->name, type->name);
			if (QCC_PR_GetDef(NULL, membername, NULL, false, 0))
				break;	//a child class overrides.
		}
		if (oc != clas)
			continue;

		if (type->type == ev_function)	//FIXME: inheritance will not install all the member functions.
		{
			sprintf(membername, "%s::"MEMBERFIELDNAME, clas->name, type->name);
			member = QCC_PR_GetDef(NULL, membername, NULL, false, 1);
			if (!member)
			{
				QCC_PR_Warning(0, NULL, 0, "Member function %s was not defined", membername);
				continue;
			}
			if (!strcmp(type->name, clas->name))
			{
				*constructor = member;
			}
			point = QCC_PR_Statement(&pr_opcodes[OP_ADDRESS], ed, member, NULL);
			sprintf(membername, "%s::%s", clas->name, type->name);
			virt = QCC_PR_GetDef(type, membername, NULL, false, 1);
			QCC_PR_Statement(&pr_opcodes[OP_STOREP_FNC], virt, point, NULL);
		}
	}
}

//take all functions in the type, and parent types, and make sure the links all work properly.
void QCC_PR_EmitClassFromFunction(QCC_def_t *scope, char *tname)
{
	QCC_type_t *basetype;

	QCC_dfunction_t *df;

	QCC_def_t *virt;
	QCC_def_t *ed, *oself, *self;
	QCC_def_t *constructor = NULL;

//	int func;

	basetype = QCC_TypeForName(tname);
	if (!basetype)
		QCC_PR_ParseError(ERR_INTERNAL, "Type %s was not defined...", tname);

	pr_scope = scope;

	df = &functions[numfunctions];
	numfunctions++;

	df->s_file = 0;
	df->s_name = 0;
	df->first_statement = numstatements;
	df->parm_size[0] = 1;
	df->numparms = 1;
	df->parm_start = numpr_globals;

	G_FUNCTION(scope->ofs) = df - functions;

	//locals here...
	ed = QCC_PR_GetDef(type_entity, "ent", NULL, true, 1);

	virt = QCC_PR_GetDef(type_function, "spawn", NULL, false, 0);
	if (!virt)
		QCC_Error(ERR_INTERNAL, "spawn function was not defined\n");
	QCC_PR_SimpleStatement(OP_CALL0, virt->ofs, 0, 0);	//calling convention doesn't come into it.
	
	QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_ENT], &def_ret, ed, NULL));

	ed->references = 1;	//there may be no functions.


	QCC_PR_EmitClassFunctionTable(basetype, basetype, ed, &constructor);

	if (constructor)
	{	//self = ent;
		self = QCC_PR_GetDef(type_entity, "self", NULL, false, 0);
		oself = QCC_PR_GetDef(type_entity, "oself", scope, true, 1);
		QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_ENT], self, oself, NULL));
		QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_ENT], ed, self, NULL));	//return to our old self. boom boom.
		QCC_PR_SimpleStatement(OP_CALL0, constructor->ofs, 0, 0);
		QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_ENT], oself, self, NULL));
	}

	QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_RETURN], &def_ret, NULL, NULL));	//apparently we do actually have to return something. *sigh*...


	pr_scope = NULL;
	memset(basictypefield, 0, sizeof(basictypefield));
	QCC_PR_EmitFieldsForMembers(basetype);
	pr_scope = scope;
	QCC_WriteAsmFunction(scope, df->first_statement, df->parm_start);
	pr.localvars = NULL;


	locals_end = numpr_globals + basetype->size;
	df->locals = locals_end - df->parm_start;
}
/*
============
PR_ParseValue

Returns the global ofs for the current token
============
*/
QCC_def_t	*QCC_PR_ParseValue (QCC_type_t *assumeclass)
{
	QCC_def_t	*ao=NULL;	//arrayoffset
	QCC_def_t		*d, *nd, *od;
	char		*name;
	QCC_dstatement_t *st;
	int i;

	char membername[2048];
	
// if the token is an immediate, allocate a constant for it
	if (pr_token_type == tt_immediate)
		return QCC_PR_ParseImmediate ();

	name = QCC_PR_ParseName ();

	if (assumeclass && assumeclass->parentclass)	// 'testvar' becomes 'self::testvar'
	{	//try getting a member.
		QCC_type_t *type;
		type = assumeclass;
		d = NULL;
		while(type != type_entity && type)
		{
			sprintf(membername, "%s::"MEMBERFIELDNAME, type->name, name);
			od = d = QCC_PR_GetDef (NULL, membername, pr_scope, false, 0);
			if (d)
				break;

			type = type->parentclass;
		}
		if (!d)
			od = d = QCC_PR_GetDef (NULL, name, pr_scope, false, 0);
	}
	else

// look through the defs
	od = d = QCC_PR_GetDef (NULL, name, pr_scope, false, 0);
	
	if (!d)
	{
		if (	(!strcmp(name, "random" ))	||
				(!strcmp(name, "randomv"))	)	//intrinsics, any old function with no args will do.
			od = d = QCC_PR_GetDef (type_function, name, NULL, true, 1);
		else if (keyword_class && !strcmp(name, "this"))
		{
			if (!pr_classtype)
				QCC_PR_ParseError(ERR_NOTANAME, "Cannot use 'this' outside of an OO function\n");
			od = QCC_PR_GetDef(NULL, "self", NULL, true, 1);
			od = d = QCC_PR_DummyDef(pr_classtype, "this", pr_scope, 1, od->ofs, true);
		}
		else if (keyword_class && !strcmp(name, "super"))
		{
			if (!pr_classtype)
				QCC_PR_ParseError(ERR_NOTANAME, "Cannot use 'super' outside of an OO function\n");
			od = QCC_PR_GetDef(NULL, "self", NULL, true, 1);
			od = d = QCC_PR_DummyDef(pr_classtype, "super", pr_scope, 1, od->ofs, true);
		}
		else
		{
			od = d = QCC_PR_GetDef (type_float, name, pr_scope, true, 1);
			if (!d)
				QCC_PR_ParseError (ERR_UNKNOWNVALUE, "Unknown value \"%s\"", name);
			else
			{
				QCC_PR_ParseWarning (ERR_UNKNOWNVALUE, "Unknown value \"%s\", assuming float.", name);
			}
		}
	}

reloop:


//FIXME: Make this work with double arrays/2nd level structures.
//Should they just jump back to here?
	if (QCC_PR_Check("["))
	{
		QCC_type_t *newtype;
		if (ao)
		{
			numstatements--;	//remove the last statement			

			nd = QCC_PR_Expression (TOP_PRIORITY);
			QCC_PR_Expect("]");

			if (d->type->size != 1)	//we need to multiply it to find the offset.						
			{
				if (ao->type->type == ev_integer)
					nd = QCC_PR_Statement(&pr_opcodes[OP_MUL_I], nd, QCC_MakeIntDef(d->type->size), NULL);	//get add part
				else if (ao->type->type == ev_float)
					nd = QCC_PR_Statement(&pr_opcodes[OP_MUL_F], nd, QCC_MakeFloatDef((float)d->type->size), NULL);	//get add part
				else
				{
					QCC_PR_ParseError(ERR_BADARRAYINDEXTYPE, "Array offset is not of integer or float type");
					nd = NULL;
				}
			}

			if (nd->type->type == ao->type->type)
			{
				if (ao->type->type == ev_integer)
					ao = QCC_PR_Statement(&pr_opcodes[OP_ADD_I], ao, nd, NULL);	//get add part
				else if (ao->type->type == ev_float)
					ao = QCC_PR_Statement(&pr_opcodes[OP_ADD_F], ao, nd, NULL);	//get add part
				else
				{
					QCC_PR_ParseError(ERR_BADARRAYINDEXTYPE, "Array offset is not of integer or float type");
					nd = NULL;
				}
			}
			else
			{
				if (nd->type->type == ev_float)
					nd = QCC_PR_Statement (&pr_opcodes[OP_CONV_FTOI], nd, 0, NULL);
				ao = QCC_PR_Statement(&pr_opcodes[OP_ADD_I], ao, nd, NULL);	//get add part
			}

			newtype = d->type;
			d = od;
		}
		else
		{
			ao = QCC_PR_Expression (TOP_PRIORITY);
			QCC_PR_Expect("]");

			if (QCC_OPCodeValid(&pr_opcodes[OP_LOADA_F]) && d->type->size != 1)	//we need to multiply it to find the offset.
			{
				if (ao->type->type == ev_integer)
					ao = QCC_PR_Statement(&pr_opcodes[OP_MUL_I], ao, QCC_MakeIntDef(d->type->size), NULL);	//get add part
				else if (ao->type->type == ev_float)
					ao = QCC_PR_Statement(&pr_opcodes[OP_MUL_F], ao, QCC_MakeFloatDef((float)d->type->size), NULL);	//get add part
				else
				{
					nd = NULL;
					QCC_PR_ParseError(ERR_BADARRAYINDEXTYPE, "Array offset is not of integer or float type");
				}
			}

			newtype = d->type;
		}
		if (ao->type->type == ev_integer)
		{
			switch(newtype->type)
			{
			case ev_float:
				nd = QCC_PR_Statement(&pr_opcodes[OP_LOADA_F], d, ao, NULL);	//get pointer to precise def.
				break;
			case ev_string:
				if (d->arraysize <= 1)
				{
					nd = QCC_PR_Statement(&pr_opcodes[OP_LOADP_C], d, QCC_PR_Statement (&pr_opcodes[OP_CONV_ITOF], ao, 0, NULL), NULL);	//get pointer to precise def.
					newtype = nd->type;//don't be fooled
				}
				else
				{
					nd = QCC_PR_Statement(&pr_opcodes[OP_LOADA_S], d, ao, NULL);	//get pointer to precise def.
				}
				break;
			case ev_vector:
				nd = QCC_PR_Statement(&pr_opcodes[OP_LOADA_V], d, ao, NULL);	//get pointer to precise def.
				break;
			case ev_entity:
				nd = QCC_PR_Statement(&pr_opcodes[OP_LOADA_ENT], d, ao, NULL);	//get pointer to precise def.			
				break;
			case ev_field:
				nd = QCC_PR_Statement(&pr_opcodes[OP_LOADA_FLD], d, ao, NULL);	//get pointer to precise def.
				break;
			case ev_function:
				nd = QCC_PR_Statement(&pr_opcodes[OP_LOADA_FNC], d, ao, NULL);	//get pointer to precise def.
				nd->type = d->type;
				break;
			case ev_integer:
				nd = QCC_PR_Statement(&pr_opcodes[OP_LOADA_I], d, ao, NULL);	//get pointer to precise def.
				break;

			case ev_struct:
				nd = QCC_PR_Statement(&pr_opcodes[OP_LOADA_I], d, ao, NULL);	//get pointer to precise def.
				nd->type = d->type;
				break;
			default:
				QCC_PR_ParseError(ERR_NOVALIDOPCODES, "No op available. Try assembler");
				nd = NULL;
				break;
			}
			d=nd;
		}
		else if (ao->type->type == ev_float)
		{
			if (qcc_targetformat == QCF_HEXEN2)
			{	//hexen2 style retrieval, mixed with q1 style assignments...
				if (QCC_PR_Check("="))	//(hideous concept)
				{
					QCC_dstatement_t *st;
					QCC_def_t *funcretr;
					if (d->scope)
						QCC_PR_ParseError(0, "Scoped array without specific engine support");
					if (def_ret.temp->used && ao != &def_ret)
						QCC_PR_ParseWarning(0, "RETURN VALUE ALREADY IN USE");

					funcretr = QCC_PR_GetDef(type_function, qcva("ArraySet*%s", d->name), NULL, true, 1);
					nd = QCC_PR_Expression(TOP_PRIORITY);
					if (nd->type->type != d->type->type)
						QCC_PR_ParseErrorPrintDef(ERR_TYPEMISMATCH, d, "Type Mismatch on array assignment");

					QCC_PR_Statement (&pr_opcodes[OP_CALL2H], funcretr, 0, &st);
					st->a = ao->ofs;
					st->b = nd->ofs;
					QCC_FreeTemp(ao);
					QCC_FreeTemp(nd);
					qcc_usefulstatement = true;

					nd = &def_ret;
					d=nd;
					d->type = newtype;
					return d;
				}

				switch(newtype->type)
				{
				case ev_float:
					nd = QCC_PR_Statement(&pr_opcodes[OP_FETCH_GBL_F], d, ao, &st);	//get pointer to precise def.
					st->a = d->ofs;
					break;
				case ev_vector:
					nd = QCC_PR_Statement(&pr_opcodes[OP_FETCH_GBL_V], d, ao, &st);	//get pointer to precise def.
					st->a = d->ofs;
					break;
				case ev_string:
					nd = QCC_PR_Statement(&pr_opcodes[OP_FETCH_GBL_S], d, ao, &st);	//get pointer to precise def.
					st->a = d->ofs;
					break;
				case ev_entity:
					nd = QCC_PR_Statement(&pr_opcodes[OP_FETCH_GBL_E], d, ao, &st);	//get pointer to precise def.
					st->a = d->ofs;
					break;
				case ev_function:
					nd = QCC_PR_Statement(&pr_opcodes[OP_FETCH_GBL_FNC], d, ao, &st);	//get pointer to precise def.
					st->a = d->ofs;
					break;
				default:
					QCC_PR_ParseError(ERR_NOVALIDOPCODES, "No op available. Try assembler");
					nd = NULL;
					break;
				}
				QCC_FreeTemp(d);
				QCC_FreeTemp(ao);

				d=nd;
				d->type = newtype;
				return d;
			}
			else
			{
				if (!QCC_OPCodeValid(&pr_opcodes[OP_LOADA_F]))	//q1 compatable.
				{	//you didn't see this, okay?
					QCC_def_t *funcretr;
					if (d->scope)
						QCC_PR_ParseError(0, "Scoped array without specific engine support");
					if (def_ret.temp->used && ao != &def_ret)
						QCC_PR_ParseWarning(0, "RETURN VALUE ALREADY IN USE");

					def_parms[0].type = type_float;
					QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_STORE_F], ao, &def_parms[0], NULL));

					if (QCC_PR_Check("="))
					{
						funcretr = QCC_PR_GetDef(type_function, qcva("ArraySet*%s", d->name), NULL, true, 1);
						nd = QCC_PR_Expression(TOP_PRIORITY);
						if (nd->type->type != d->type->type)
							QCC_PR_ParseErrorPrintDef(ERR_TYPEMISMATCH, d, "Type Mismatch on array assignment");

						QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_STORE_V], nd, &def_parms[1], NULL));
						QCC_PR_Statement (&pr_opcodes[OP_CALL2], funcretr, 0, NULL);
						qcc_usefulstatement = true;
					}
					else
					{
						funcretr = QCC_PR_GetDef(type_function, qcva("ArrayGet*%s", d->name), NULL, true, 1);
						QCC_PR_Statement (&pr_opcodes[OP_CALL1], funcretr, 0, NULL);
					}

					nd = &def_ret;
					d=nd;
					d->type = newtype;
					return d;
				}
				else
				{
					switch(newtype->type)
					{
					case ev_pointer:
						if (d->arraysize>1)	//use the array
						{
							nd = QCC_PR_Statement(&pr_opcodes[OP_LOADA_I], d, QCC_PR_Statement (&pr_opcodes[OP_CONV_FTOI], ao, 0, NULL), NULL);	//get pointer to precise def.
							nd->type = d->type->aux_type;
						}
						else
						{	//dereference the pointer.
							switch(newtype->aux_type->type)
							{
							case ev_pointer:
								nd = QCC_PR_Statement(&pr_opcodes[OP_LOADP_I], d, QCC_PR_Statement (&pr_opcodes[OP_CONV_FTOI], ao, 0, NULL), NULL);	//get pointer to precise def.
								nd->type = d->type->aux_type;
								break;
							case ev_float:
								nd = QCC_PR_Statement(&pr_opcodes[OP_LOADP_F], d, QCC_PR_Statement (&pr_opcodes[OP_CONV_FTOI], ao, 0, NULL), NULL);	//get pointer to precise def.
								nd->type = d->type->aux_type;
								break;
							case ev_integer:
								nd = QCC_PR_Statement(&pr_opcodes[OP_LOADP_I], d, QCC_PR_Statement (&pr_opcodes[OP_CONV_FTOI], ao, 0, NULL), NULL);	//get pointer to precise def.
								nd->type = d->type->aux_type;
								break;
							default:
								QCC_PR_ParseError(ERR_NOVALIDOPCODES, "No op available. Try assembler");
								nd = NULL;
								break;
							}
						}
						break;

					case ev_float:
						nd = QCC_PR_Statement(&pr_opcodes[OP_LOADA_F], d, QCC_PR_Statement (&pr_opcodes[OP_CONV_FTOI], ao, 0, NULL), NULL);	//get pointer to precise def.
						break;
					case ev_string:
						if (d->arraysize <= 1)
						{
							nd = QCC_PR_Statement(&pr_opcodes[OP_LOADP_C], d, ao, NULL);	//get pointer to precise def.
							newtype = nd->type;//don't be fooled
						}
						else
							nd = QCC_PR_Statement(&pr_opcodes[OP_LOADA_S], d, QCC_PR_Statement (&pr_opcodes[OP_CONV_FTOI], ao, 0, NULL), NULL);	//get pointer to precise def.
						break;
					case ev_vector:
						nd = QCC_PR_Statement(&pr_opcodes[OP_LOADA_V], d, QCC_PR_Statement (&pr_opcodes[OP_CONV_FTOI], ao, 0, NULL), NULL);	//get pointer to precise def.
						break;
					case ev_entity:
						nd = QCC_PR_Statement(&pr_opcodes[OP_LOADA_ENT], d, QCC_PR_Statement (&pr_opcodes[OP_CONV_FTOI], ao, 0, NULL), NULL);	//get pointer to precise def.			
						break;
					case ev_field:
						nd = QCC_PR_Statement(&pr_opcodes[OP_LOADA_FLD], d, QCC_PR_Statement (&pr_opcodes[OP_CONV_FTOI], ao, 0, NULL), NULL);	//get pointer to precise def.
						break;
					case ev_function:
						nd = QCC_PR_Statement(&pr_opcodes[OP_LOADA_FNC], d, QCC_PR_Statement (&pr_opcodes[OP_CONV_FTOI], ao, 0, NULL), NULL);	//get pointer to precise def.
						nd->type = d->type;
						break;
					case ev_integer:
						nd = QCC_PR_Statement(&pr_opcodes[OP_LOADA_I], d, QCC_PR_Statement (&pr_opcodes[OP_CONV_FTOI], ao, 0, NULL), NULL);	//get pointer to precise def.
						break;

					case ev_struct:
						nd = QCC_PR_Statement(&pr_opcodes[OP_LOADA_I], d, QCC_PR_Statement (&pr_opcodes[OP_CONV_FTOI], ao, 0, NULL), NULL);	//get pointer to precise def.
						nd->type = d->type;
						break;
					default:
						QCC_PR_ParseError(ERR_NOVALIDOPCODES, "No op available. Try assembler");
						nd = NULL;
						break;
					}
				}
			}
			d=nd;
		}
		else
			QCC_PR_ParseError(ERR_BADARRAYINDEXTYPE, "Array offset is not of integer or float type");
		
		d->type = newtype;
		goto reloop;
	}


	i = d->type->type;
	if (i == ev_pointer)
	{
		int j;
		QCC_type_t *type;
		if (QCC_PR_Check(".") || QCC_PR_Check("->"))
		{
			for (i = d->type->num_parms, type = d->type+1; i; i--, type++)
			{
				if (QCC_PR_Check(type->name))
				{
					//give result
					if (ao)
					{
						numstatements--;	//remove the last statement
						d = od;

						nd = QCC_MakeIntDef(type->ofs);
						ao = QCC_PR_Statement(&pr_opcodes[OP_ADD_I], ao, nd, NULL);	//get add part						

						//so that we may offset it and readd it.
					}
					else
						ao = QCC_MakeIntDef(type->ofs);
					switch (type->type)
					{
					case ev_float:
						nd = QCC_PR_Statement(&pr_opcodes[OP_LOADP_F], d, ao, NULL);	//get pointer to precise def.
						break;
					case ev_string:
						nd = QCC_PR_Statement(&pr_opcodes[OP_LOADP_S], d, ao, NULL);	//get pointer to precise def.
						break;
					case ev_vector:
						nd = QCC_PR_Statement(&pr_opcodes[OP_LOADP_V], d, ao, NULL);	//get pointer to precise def.
						break;
					case ev_entity:
						nd = QCC_PR_Statement(&pr_opcodes[OP_LOADP_ENT], d, ao, NULL);	//get pointer to precise def.			
						break;
					case ev_field:
						nd = QCC_PR_Statement(&pr_opcodes[OP_LOADP_FLD], d, ao, NULL);	//get pointer to precise def.
						break;
					case ev_function:
						nd = QCC_PR_Statement(&pr_opcodes[OP_LOADP_FNC], d, ao, NULL);	//get pointer to precise def.
						nd->type = type;
						break;
					case ev_integer:
						nd = QCC_PR_Statement(&pr_opcodes[OP_LOADP_I], d, ao, NULL);	//get pointer to precise def.
						break;

//					case ev_struct:
						//no suitable op.
//						nd = QCC_PR_Statement(&pr_opcodes[OP_LOADP_I], d, ao, NULL);	//get pointer to precise def.
//						nd->type = type;
//						break;
					default:
						QCC_PR_ParseError(ERR_NOVALIDOPCODES, "No op available. Try assembler");
						nd = NULL;
						break;
					}					

					d=nd;
					break;
				}
				if (type->num_parms)
				{
					for (j = type->num_parms; j;j--)
						type++;
				}
			}			
			if (!i)
				QCC_PR_ParseError (ERR_MEMBERNOTVALID, "\"%s\" is not a member of \"%s\"", pr_token, od->type->name);

			goto reloop;
		}
	}
	else if (i == ev_struct || i == ev_union)
	{
		int j;
		QCC_type_t *type;
		if (QCC_PR_Check(".") || QCC_PR_Check("->"))
		{
			for (i = d->type->num_parms, type = d->type+1; i; i--, type++)
			{
				if (QCC_PR_Check(type->name))
				{
					//give result
					if (ao)
					{
						numstatements--;	//remove the last statement
						d = od;

						nd = QCC_MakeIntDef(type->ofs);
						ao = QCC_PR_Statement(&pr_opcodes[OP_ADD_I], ao, nd, NULL);	//get add part						

						//so that we may offset it and readd it.
					}
					else
						ao = QCC_MakeIntDef(type->ofs);
					switch (type->type)
					{
					case ev_float:
						nd = QCC_PR_Statement(&pr_opcodes[OP_LOADA_F], d, ao, NULL);	//get pointer to precise def.
						break;
					case ev_string:
						nd = QCC_PR_Statement(&pr_opcodes[OP_LOADA_S], d, ao, NULL);	//get pointer to precise def.
						break;
					case ev_vector:
						nd = QCC_PR_Statement(&pr_opcodes[OP_LOADA_V], d, ao, NULL);	//get pointer to precise def.
						break;
					case ev_entity:
						nd = QCC_PR_Statement(&pr_opcodes[OP_LOADA_ENT], d, ao, NULL);	//get pointer to precise def.			
						break;
					case ev_field:
						nd = QCC_PR_Statement(&pr_opcodes[OP_LOADA_FLD], d, ao, NULL);	//get pointer to precise def.
						break;
					case ev_function:
						nd = QCC_PR_Statement(&pr_opcodes[OP_LOADA_FNC], d, ao, NULL);	//get pointer to precise def.
						nd->type = type;
						break;
					case ev_integer:
						nd = QCC_PR_Statement(&pr_opcodes[OP_LOADA_I], d, ao, NULL);	//get pointer to precise def.
						break;

//					case ev_struct:
						//no suitable op.
//						nd = QCC_PR_Statement(&pr_opcodes[OP_LOADA_I], d, ao, NULL);	//get pointer to precise def.
//						nd->type = type;
//						break;
					default:
						QCC_PR_ParseError(ERR_NOVALIDOPCODES, "No op available. Try assembler");
						nd = NULL;
						break;
					}					

					d=nd;
					break;
				}
				if (type->num_parms)
				{
					for (j = type->num_parms; j;j--)
						type++;
				}
			}			
			if (!i)
				QCC_PR_ParseError (ERR_MEMBERNOTVALID, "\"%s\" is not a member of \"%s\"", pr_token, od->type->name);

			goto reloop;
		}
	}

/*	if (d->type->type == ev_pointer)
	{	//expand now, not in function call/maths parsing
		switch(d->type->aux_type->type)
		{
		case ev_string:
			d = QCC_PR_Statement(&pr_opcodes[OP_LOADP_S], d, NULL, NULL);
			break;
		case ev_float:
			d = QCC_PR_Statement(&pr_opcodes[OP_LOADP_F], d, NULL, NULL);
			break;
		}
	}
*/	
	if (!keyword_class)
		return d;

	if (d->type->parentclass||d->type->type == ev_entity)	//class
	{
		if (QCC_PR_Check(".") || QCC_PR_Check("->"))
		{
			QCC_def_t *field;
			if (QCC_PR_Check("("))
			{
				field = QCC_PR_Expression(TOP_PRIORITY);
				QCC_PR_Expect(")");
			}
			else
				field = QCC_PR_ParseValue(d->type);
			if (field->type->type == ev_field)
			{
				if (!field->type->aux_type)
				{
					QCC_PR_ParseWarning(ERR_INTERNAL, "Field with null aux_type");
					return QCC_PR_Statement(&pr_opcodes[OP_LOAD_FLD], d, field, NULL);
				}
				else
				{
					switch(field->type->aux_type->type)
					{
					default:
						QCC_PR_ParseError(ERR_INTERNAL, "Bad field type");
						return d;
					case ev_integer:
						return QCC_PR_Statement(&pr_opcodes[OP_LOAD_I], d, field, NULL);
					case ev_field:
						d = QCC_PR_Statement(&pr_opcodes[OP_LOAD_FLD], d, field, NULL);
						nd = (void *)qccHunkAlloc (sizeof(QCC_def_t));
						memset (nd, 0, sizeof(QCC_def_t));		
						nd->type = field->type->aux_type;
						nd->ofs = d->ofs;
						nd->temp = d->temp;
						nd->constant = false;
						nd->name = d->name;
						return nd;
					case ev_float:
						return QCC_PR_Statement(&pr_opcodes[OP_LOAD_F], d, field, NULL);
					case ev_string:
						return QCC_PR_Statement(&pr_opcodes[OP_LOAD_S], d, field, NULL);
					case ev_vector:
						return QCC_PR_Statement(&pr_opcodes[OP_LOAD_V], d, field, NULL);
					case ev_function:
						{	//complicated for a typecast
						d = QCC_PR_Statement(&pr_opcodes[OP_LOAD_FNC], d, field, NULL);
						nd = (void *)qccHunkAlloc (sizeof(QCC_def_t));
						memset (nd, 0, sizeof(QCC_def_t));		
						nd->type = field->type->aux_type;
						nd->ofs = d->ofs;
						nd->temp = d->temp;
						nd->constant = false;
						nd->name = d->name;
						return nd;

						}
					case ev_entity:
						return QCC_PR_Statement(&pr_opcodes[OP_LOAD_ENT], d, field, NULL);
					}
				}
			}
			else
				QCC_PR_IncludeChunk(".", false, NULL);
		}
	}	

	return d;
}


/*
============
PR_Term
============
*/
QCC_def_t *QCC_PR_Term (void)
{
	QCC_def_t	*e, *e2;
	etype_t	t;
	if (pr_token_type == tt_punct)	//a little extra speed...
	{
		if (QCC_PR_Check("++"))	//supposedly. I'm unsure weather it works properly.
		{
			qcc_usefulstatement=true;
			e = QCC_PR_Term ();
			if (e->constant)
				QCC_PR_ParseWarning(WARN_ASSIGNMENTTOCONSTANT, "Assignment to constant %s", e->name);
			if (e->temp)
				QCC_PR_ParseWarning(WARN_ASSIGNMENTTOCONSTANT, "Hey! That's a temp! ++ operators cannot work on temps!");
			switch (e->type->type)
			{
			case ev_integer:
				QCC_PR_Statement3(&pr_opcodes[OP_ADD_I], e, QCC_MakeIntDef(1), e);
				break;
			case ev_float:
				QCC_PR_Statement3(&pr_opcodes[OP_ADD_F], e, QCC_MakeFloatDef(1), e);
				break;
			default:
				QCC_PR_ParseError(ERR_BADPLUSPLUSOPERATOR, "++ operator on unsupported type");
				break;
			}
			return e;
		}
		else if (QCC_PR_Check("--"))
		{
			qcc_usefulstatement=true;
			e = QCC_PR_Term ();
			if (e->constant)
				QCC_PR_ParseWarning(WARN_ASSIGNMENTTOCONSTANT, "Assignment to constant %s", e->name);
			if (e->temp)
				QCC_PR_ParseWarning(WARN_ASSIGNMENTTOCONSTANT, "Hey! That's a temp! -- operators cannot work on temps!");
			switch (e->type->type)
			{
			case ev_integer:
				QCC_PR_Statement3(&pr_opcodes[OP_SUB_I], e, QCC_MakeIntDef(1), e);
				break;
			case ev_float:
				QCC_PR_Statement3(&pr_opcodes[OP_SUB_F], e, QCC_MakeFloatDef(1), e);
				break;
			default:
				QCC_PR_ParseError(ERR_BADPLUSPLUSOPERATOR, "-- operator on unsupported type");
				break;
			}
			return e;
		}
		
		if (QCC_PR_Check ("!"))
		{
			e = QCC_PR_Expression (NOT_PRIORITY);
			t = e->type->type;
			if (t == ev_float)
				e2 = QCC_PR_Statement (&pr_opcodes[OP_NOT_F], e, 0, NULL);
			else if (t == ev_string)
				e2 = QCC_PR_Statement (&pr_opcodes[OP_NOT_S], e, 0, NULL);
			else if (t == ev_entity)
				e2 = QCC_PR_Statement (&pr_opcodes[OP_NOT_ENT], e, 0, NULL);
			else if (t == ev_vector)
				e2 = QCC_PR_Statement (&pr_opcodes[OP_NOT_V], e, 0, NULL);
			else if (t == ev_function)
				e2 = QCC_PR_Statement (&pr_opcodes[OP_NOT_FNC], e, 0, NULL);
			else if (t == ev_integer)
				e2 = QCC_PR_Statement (&pr_opcodes[OP_NOT_FNC], e, 0, NULL);	//functions are integer values too.
			else if (t == ev_pointer)
				e2 = QCC_PR_Statement (&pr_opcodes[OP_NOT_FNC], e, 0, NULL);	//Pointers are too.
			else
			{
				e2 = NULL;		// shut up compiler warning;
				QCC_PR_ParseError (ERR_BADNOTTYPE, "type mismatch for !");
			}
			return e2;
		}

		else if (QCC_PR_Check ("&"))
		{
			int st = numstatements;
			e = QCC_PR_Expression (NOT_PRIORITY);
			t = e->type->type;

			if (st != numstatements)
				//woo, something like ent.field?
			{
				if ((unsigned)(statements[numstatements-1].op - OP_LOAD_F) < 6 || statements[numstatements-1].op == OP_LOAD_I || statements[numstatements-1].op == OP_LOAD_P)
				{
					statements[numstatements-1].op = OP_ADDRESS;
					QCC_PR_ParseWarning(0, "debug: &ent.field");
					e->type = QCC_PR_PointerType(e->type);
					return e;
				}
				else	//this is a restriction that could be lifted, I just want to make sure that I got all the bits first.
				{
					QCC_PR_ParseError (ERR_BADNOTTYPE, "type mismatch for '&' Must be singular expression or field reference");
					return e;
				}
			}
//			QCC_PR_ParseWarning(0, "debug: &global");

			if (!QCC_OPCodeValid(&pr_opcodes[OP_GLOBALADDRESS]))
				QCC_PR_ParseError (ERR_BADEXTENSION, "Cannot use addressof operator ('&') on a global. Please use the FTE target.");

			e2 = QCC_PR_Statement (&pr_opcodes[OP_GLOBALADDRESS], e, 0, NULL);
			e2->type = QCC_PR_PointerType(e->type);
			return e2;
		}
		else if (QCC_PR_Check ("*"))
		{
			e = QCC_PR_Expression (NOT_PRIORITY);
			t = e->type->type;

			if (t != ev_pointer)
				QCC_PR_ParseError (ERR_BADNOTTYPE, "type mismatch for *");

			switch(e->type->aux_type->type)
			{
			case ev_float:
				e2 = QCC_PR_Statement (&pr_opcodes[OP_LOADP_F], e, 0, NULL);
				break;
			case ev_string:
				e2 = QCC_PR_Statement (&pr_opcodes[OP_LOADP_S], e, 0, NULL);
				break;
			case ev_vector:
				e2 = QCC_PR_Statement (&pr_opcodes[OP_LOADP_V], e, 0, NULL);
				break;
			case ev_entity:
				e2 = QCC_PR_Statement (&pr_opcodes[OP_LOADP_ENT], e, 0, NULL);
				break;
			case ev_field:
				e2 = QCC_PR_Statement (&pr_opcodes[OP_LOADP_FLD], e, 0, NULL);
				break;
			case ev_function:
				e2 = QCC_PR_Statement (&pr_opcodes[OP_LOADP_FLD], e, 0, NULL);
				break;
			case ev_integer:
				e2 = QCC_PR_Statement (&pr_opcodes[OP_LOADP_I], e, 0, NULL);
				break;
			case ev_pointer:
				e2 = QCC_PR_Statement (&pr_opcodes[OP_LOADP_I], e, 0, NULL);
				break;

			default:
				QCC_PR_ParseError (ERR_BADNOTTYPE, "type mismatch for * (unrecognised type)");
				break;
			}

			e2->type = e->type->aux_type;
			return e2;
		}
		else if (QCC_PR_Check ("-"))
		{
			e = QCC_PR_Expression (NOT_PRIORITY);

			switch(e->type->type)
			{
			case ev_float:
				e2 = QCC_PR_Statement (&pr_opcodes[OP_SUB_F], QCC_MakeFloatDef(0), e, NULL);
				break;
			case ev_integer:
				e2 = QCC_PR_Statement (&pr_opcodes[OP_SUB_I], QCC_MakeIntDef(0), e, NULL);
				break;
			default:
				QCC_PR_ParseError (ERR_BADNOTTYPE, "type mismatch for -");
				break;
			}
			return e2;
		}
		else if (QCC_PR_Check ("+"))
		{
			e = QCC_PR_Expression (NOT_PRIORITY);

			switch(e->type->type)
			{
			case ev_float:
				e2 = QCC_PR_Statement (&pr_opcodes[OP_ADD_F], QCC_MakeFloatDef(0), e, NULL);
				break;
			case ev_integer:
				e2 = QCC_PR_Statement (&pr_opcodes[OP_ADD_I], QCC_MakeIntDef(0), e, NULL);
				break;
			default:
				QCC_PR_ParseError (ERR_BADNOTTYPE, "type mismatch for +");
				break;
			}
			return e2;
		}
		
		if (QCC_PR_Check ("("))
		{
			if (keyword_float && QCC_PR_Check("float"))	//check for type casts
			{
				QCC_PR_Expect (")");
				e = QCC_PR_Term();
				if (e->type->type == ev_float)
					return e;
				else if (e->type->type == ev_integer)
					return QCC_PR_Statement (&pr_opcodes[OP_CONV_ITOF], e, 0, NULL);
				else if (e->type->type == ev_function)
					return e;
	//			else
	//				QCC_PR_ParseError ("invalid typecast");

				QCC_PR_ParseWarning (0, "Not all vars make sence as floats");

				e2 = (void *)qccHunkAlloc (sizeof(QCC_def_t));
				memset (e2, 0, sizeof(QCC_def_t));		
				e2->type = type_float;
				e2->ofs = e->ofs;
				e2->constant = true;
				e2->temp = e->temp;
				return e2;
			}
			else if (keyword_class && QCC_PR_Check("class"))
			{
				QCC_type_t *classtype = QCC_TypeForName(QCC_PR_ParseName());
				if (!classtype)
					QCC_PR_ParseError(ERR_NOTANAME, "Class not defined for cast");

				QCC_PR_Expect (")");
				e = QCC_PR_Term();
				e2 = (void *)qccHunkAlloc (sizeof(QCC_def_t));
				memset (e2, 0, sizeof(QCC_def_t));		
				e2->type = classtype;
				e2->ofs = e->ofs;
				e2->constant = true;
				e2->temp = e->temp;
				return e2;
			}
			else if (keyword_integer && QCC_PR_Check("integer"))	//check for type casts
			{
				QCC_PR_Expect (")");
				e = QCC_PR_Term();
				if (e->type->type == ev_integer)
					return e;
				else if (e->type->type == ev_float)
					return QCC_PR_Statement (&pr_opcodes[OP_CONV_FTOI], e, 0, NULL);
				else
					QCC_PR_ParseError (ERR_BADTYPECAST, "invalid typecast");
			}
			else if (keyword_int && QCC_PR_Check("int"))	//check for type casts
			{
				QCC_PR_Expect (")");
				e = QCC_PR_Term();
				if (e->type->type == ev_integer)
					return e;
				else if (e->type->type == ev_float)
					return QCC_PR_Statement (&pr_opcodes[OP_CONV_FTOI], e, 0, NULL);
				else
					QCC_PR_ParseError (ERR_BADTYPECAST, "invalid typecast");
			}
			else
			{
				pbool oldcond = conditional;
				conditional = conditional?2:0;
				e =	QCC_PR_Expression (TOP_PRIORITY);
				QCC_PR_Expect (")");
				conditional = oldcond;
			}
			return e;
		}
	}
	return QCC_PR_ParseValue (pr_classtype);
}


int QCC_canConv(QCC_def_t *from, etype_t to)
{
	if (from->type->type == to)
		return 0;

	if (from->type->type == ev_vector && to == ev_float)
		return 4;

	if (pr_classtype)
	{
		if (from->type->type == ev_field)
		{
			if (from->type->aux_type->type == to)
				return 1;
		}
	}
	
/*	if (from->type->type == ev_pointer && from->type->aux_type->type == to)
		return 1;

	if (QCC_ShouldConvert(from, to)>=0)
		return 1;
*/
	if (from->type->type == ev_integer && to == ev_function)
		return 1;

	return -100;
}
/*
==============
PR_Expression
==============
*/

QCC_def_t *QCC_PR_Expression (int priority)
{
	QCC_dstatement32_t	*st;
	QCC_opcode_t	*op, *oldop;

	QCC_opcode_t *bestop;
	int numconversions, c;

	int opnum;

	QCC_def_t		*e, *e2;
	etype_t		type_a, type_b, type_c;

	if (priority == 0)
		return QCC_PR_Term ();

	e = QCC_PR_Expression (priority-1);

	while (1)
	{
		if (priority == 1 && QCC_PR_Check ("(") )
		{
			qcc_usefulstatement=true;
			return QCC_PR_ParseFunctionCall (e);
		}

		opnum=0;

		if (pr_token_type == tt_immediate)
		{
			if (pr_immediate_type->type == ev_float)
				if (pr_immediate._float < 0)	//hehehe... was a minus all along...
				{
					QCC_PR_IncludeChunk(pr_token, true, NULL);
					strcpy(pr_token, "+");//two negatives would make a positive.
					pr_token_type = tt_punct;
				}
		}

		if (pr_token_type != tt_punct)
		{
			QCC_PR_ParseWarning(WARN_UNEXPECTEDPUNCT, "Expected punctuation");
		}

		//go straight for the correct priority.
		for (op = opcodeprioritized[priority][opnum]; op; op = opcodeprioritized[priority][++opnum])
//		for (op=pr_opcodes ; op->name ; op++)
		{
//			if (op->priority != priority)
//				continue;
			if (!QCC_PR_Check (op->name))
				continue;
			st = NULL;
			if ( op->associative!=ASSOC_LEFT )
			{
			// if last statement is an indirect, change it to an address of
				if (!simplestore && ((unsigned)(statements[numstatements-1].op - OP_LOAD_F) < 6 || statements[numstatements-1].op == OP_LOAD_I || statements[numstatements-1].op == OP_LOAD_P) && statements[numstatements-1].c == e->ofs)
				{
					qcc_usefulstatement=true;
					statements[numstatements-1].op = OP_ADDRESS;
					type_pointer->aux_type->type = e->type->type;
					e->type = type_pointer;
				}
				//if last statement retrieved a value, switch it to retrieve a usable pointer.
				if ( !simplestore && (unsigned)(statements[numstatements-1].op - OP_LOADA_F) < 7)// || statements[numstatements-1].op == OP_LOADA_C)
				{
					statements[numstatements-1].op = OP_GLOBALADDRESS;
					type_pointer->aux_type->type = e->type->type;
					e->type = type_pointer;
				}
				if ( !simplestore && (unsigned)(statements[numstatements-1].op - OP_LOADP_F) < 7)
				{
					statements[numstatements-1].op = OP_ADD_I;
				}
				if ( !simplestore && statements[numstatements-1].op == OP_LOADP_C && e->ofs == statements[numstatements-1].c)
				{
					statements[numstatements-1].op = OP_ADD_SF;
					e->type = type_string;

					//now we want to make sure that string = float can't work without it being a dereferenced pointer. (we don't want to allow storep_c without dereferece)
					e2 = QCC_PR_Expression (priority);
					if (e2->type->type == ev_float)
						op = &pr_opcodes[OP_STOREP_C];
				}
				else
					e2 = QCC_PR_Expression (priority);
			}
			else
			{
				if (op->priority == 7 && opt_logicops)
				{
					optres_logicops++;
					st = &statements[numstatements];
					if (*op->name == '&')	//statement 3 because we don't want to optimise this into if from not ifnot
						QCC_PR_Statement3(&pr_opcodes[OP_IFNOT], e, NULL, NULL);
					else
						QCC_PR_Statement3(&pr_opcodes[OP_IF], e, NULL, NULL);
				}

				e2 = QCC_PR_Expression (priority-1);
			}

		// type check
			type_a = e->type->type;
			type_b = e2->type->type;

//			if (type_a == ev_pointer && type_b == ev_pointer)
//				QCC_PR_ParseWarning(0, "Debug: pointer op pointer");

			if (op->name[0] == '.')// field access gets type from field
			{
				if (e2->type->aux_type)
					type_c = e2->type->aux_type->type;
				else
					type_c = -1;	// not a field
			}
			else
				type_c = ev_void;
				
			oldop = op;
			bestop = NULL;
			numconversions = 32767;			
			while (op)
			{
				if (!(type_c != ev_void && type_c != (*op->type_c)->type))
				{
					if (!STRCMP (op->name , oldop->name))	//matches
					{
						//return values are never converted - what to?
	//					if (type_c != ev_void && type_c != op->type_c->type->type)
	//					{
	//						op++;
	//						continue;
	//					}

						if (op->associative!=ASSOC_LEFT)
						{//assignment
							if (op->type_a == &type_pointer)	//ent var
							{
								if (e->type->type != ev_pointer)
									c = -200;	//don't cast to a pointer.
								else if ((*op->type_c)->type == ev_void && op->type_b == &type_pointer && e2->type->type == ev_pointer)
									c = 0;	//generic pointer... fixme: is this safe? make sure both sides are equivelent
								else if (e->type->aux_type->type != (*op->type_b)->type)	//if e isn't a pointer to a type_b
									c = -200;	//don't let the conversion work
								else
									c = QCC_canConv(e2, (*op->type_c)->type);
							}
							else
							{
								c=QCC_canConv(e2, (*op->type_b)->type);
								if (type_a != (*op->type_a)->type)	//in this case, a is the final assigned value
									c = -300;	//don't use this op, as we must not change var b's type
							}
						}
						else
						{
							if (op->type_a == &type_pointer)	//ent var
							{
								if (e2->type->type != ev_pointer || e2->type->aux_type->type != (*op->type_b)->type)	//if e isn't a pointer to a type_b
									c = -200;	//don't let the conversion work
								else
									c = 0;
							}
							else
							{
								c=QCC_canConv(e, (*op->type_a)->type);
								c+=QCC_canConv(e2, (*op->type_b)->type);
							}
						}

						if (c>=0 && c < numconversions)
						{
							bestop = op;
							numconversions=c;
							if (c == 0)//can't get less conversions than 0...
								break;
						}
					}				
					else
						break;
				}
				op = opcodeprioritized[priority][++opnum];
			}
			if (bestop == NULL)
			{
				if (oldop->priority == CONDITION_PRIORITY)
					op = oldop;
				else
				{
					if (e->type->type == ev_pointer)
						QCC_PR_ParseError (ERR_TYPEMISMATCH, "type mismatch for %s (%s and %s)", oldop->name, e->type->name, e2->type->name);
					else
						QCC_PR_ParseError (ERR_TYPEMISMATCH, "type mismatch for %s (%s and %s)", oldop->name, e->type->name, e2->type->name);
				}
			}
			else
			{
				if (numconversions>3)
					QCC_PR_ParseWarning(WARN_IMPLICITCONVERSION, "Implicit conversion");
				op = bestop;
			}

//			if (type_a == ev_pointer && type_b != e->type->aux_type->type)
//				QCC_PR_ParseError ("type mismatch for %s", op->name);

			if (st)
				st->b = &statements[numstatements] - st;


			if (op->associative!=ASSOC_LEFT)
			{
				qcc_usefulstatement = true;
				if (e->constant || e->ofs < OFS_PARM0)
				{
					QCC_PR_ParseWarning(WARN_ASSIGNMENTTOCONSTANT, "Assignment to constant %s", e->name);
					QCC_PR_ParsePrintDef(WARN_ASSIGNMENTTOCONSTANT, e);
#ifndef QCC
					editbadfile(strings+s_file, pr_source_line);
#endif
				}
				if (conditional&1)
					QCC_PR_ParseWarning(WARN_ASSIGNMENTINCONDITIONAL, "Assignment in conditional");

				e = QCC_PR_Statement (op, e2, e, NULL);
			}
			else
				e = QCC_PR_Statement (op, e, e2, NULL);
			
			if (type_c != ev_void/* && type_c != ev_string*/)	// field access gets type from field
				e->type = e2->type->aux_type;
			
			break;
		}
		if (!op)
		{
			if (e == NULL)
				QCC_PR_ParseError(ERR_INTERNAL, "e == null");


			if (!STRCMP(pr_token, "++"))
			{
				//if the last statement was an ent.float (or something)
				if (((unsigned)(statements[numstatements-1].op - OP_LOAD_F) < 6 || statements[numstatements-1].op == OP_LOAD_I) && statements[numstatements-1].c == e->ofs)
				{	//we have our load.
					QCC_def_t		*e3;
//the only inefficiency here is with an extra temp (we can't reuse the origional)
//this is not a problem, as the optimise temps or locals marshalling can clean these up for us
					qcc_usefulstatement=true;
//load
//add to temp
//store temp to offset
//return origional loaded (which is not at the same offset as the pointer we store to)
					e2 = QCC_GetTemp(type_float);
					e3 = QCC_GetTemp(type_pointer);
					QCC_PR_SimpleStatement(OP_ADDRESS, statements[numstatements-1].a, statements[numstatements-1].b, e3->ofs);
					if (e->type->type == ev_float)
					{
						QCC_PR_Statement3(&pr_opcodes[OP_ADD_F], e, QCC_MakeFloatDef(1), e2);
						QCC_PR_Statement3(&pr_opcodes[OP_STOREP_F], e2, e3, NULL);
					}
					else if (e->type->type == ev_integer)
					{
						QCC_PR_Statement3(&pr_opcodes[OP_ADD_I], e, QCC_MakeIntDef(1), e2);
						QCC_PR_Statement3(&pr_opcodes[OP_STOREP_I], e2, e3, NULL);
					}
					else
					{
						QCC_PR_ParseError(ERR_PARSEERRORS, "-- suffix operator results in nonstandard behaviour. Use -=1 or prefix form instead");
						QCC_PR_IncludeChunk("-=1", false, NULL);
					}
					QCC_FreeTemp(e2);
					QCC_FreeTemp(e3);
				}
				else if (e->type->type == ev_float)
				{
//copy to temp
//add to origional
//return temp (which == origional)
					QCC_PR_ParseWarning(WARN_INEFFICIENTPLUSPLUS, "++ suffix operator results in inefficient behaviour. Use +=1 or prefix form instead");
					qcc_usefulstatement=true;

					e2 = QCC_GetTemp(type_float);
					QCC_PR_Statement3(&pr_opcodes[OP_STORE_F], e, e2, NULL);
					QCC_PR_Statement3(&pr_opcodes[OP_ADD_F], e, QCC_MakeFloatDef(1), e);
					QCC_FreeTemp(e);
					e = e2;
				}
				else if (e->type->type == ev_integer)
				{
					QCC_PR_ParseWarning(WARN_INEFFICIENTPLUSPLUS, "++ suffix operator results in inefficient behaviour. Use +=1 or prefix form instead");
					qcc_usefulstatement=true;

					e2 = QCC_GetTemp(type_integer);
					QCC_PR_Statement3(&pr_opcodes[OP_STORE_I], e, e2, NULL);
					QCC_PR_Statement3(&pr_opcodes[OP_ADD_I], e, QCC_MakeIntDef(1), e);
					QCC_FreeTemp(e);
					e = e2;
				}
				else
				{
					QCC_PR_ParseWarning(WARN_NOTSTANDARDBEHAVIOUR, "++ suffix operator results in nonstandard behaviour. Use +=1 or prefix form instead");
					QCC_PR_IncludeChunk("+=1", false, NULL);
				}
				QCC_PR_Lex();
			}
			else if (!STRCMP(pr_token, "--"))
			{
				if (((unsigned)(statements[numstatements-1].op - OP_LOAD_F) < 6 || statements[numstatements-1].op == OP_LOAD_I) && statements[numstatements-1].c == e->ofs)
				{	//we have our load.
					QCC_def_t		*e3;
//load
//add to temp
//store temp to offset
//return origional loaded (which is not at the same offset as the pointer we store to)
					e2 = QCC_GetTemp(type_float);
					e3 = QCC_GetTemp(type_pointer);
					QCC_PR_SimpleStatement(OP_ADDRESS, statements[numstatements-1].a, statements[numstatements-1].b, e3->ofs);
					if (e->type->type == ev_float)
					{
						QCC_PR_Statement3(&pr_opcodes[OP_SUB_F], e, QCC_MakeFloatDef(1), e2);
						QCC_PR_Statement3(&pr_opcodes[OP_STOREP_F], e2, e3, NULL);
					}
					else if (e->type->type == ev_integer)
					{
						QCC_PR_Statement3(&pr_opcodes[OP_SUB_I], e, QCC_MakeIntDef(1), e2);
						QCC_PR_Statement3(&pr_opcodes[OP_STOREP_I], e2, e3, NULL);
					}
					else
					{
						QCC_PR_ParseError(ERR_PARSEERRORS, "-- suffix operator results in nonstandard behaviour. Use -=1 or prefix form instead");
						QCC_PR_IncludeChunk("-=1", false, NULL);
					}
					QCC_FreeTemp(e2);
					QCC_FreeTemp(e3);
				}
				else if (e->type->type == ev_float)
				{
					QCC_PR_ParseWarning(WARN_INEFFICIENTPLUSPLUS, "-- suffix operator results in inefficient behaviour. Use -=1 or prefix form instead");
					qcc_usefulstatement=true;

					e2 = QCC_GetTemp(type_float);
					QCC_PR_Statement3(&pr_opcodes[OP_STORE_F], e, e2, NULL);
					QCC_PR_Statement3(&pr_opcodes[OP_SUB_F], e, QCC_MakeFloatDef(1), e);
					QCC_FreeTemp(e);
					e = e2;
				}
				else if (e->type->type == ev_integer)
				{
					QCC_PR_ParseWarning(WARN_INEFFICIENTPLUSPLUS, "-- suffix operator results in inefficient behaviour. Use -=1 or prefix form instead");
					qcc_usefulstatement=true;

					e2 = QCC_GetTemp(type_integer);
					QCC_PR_Statement3(&pr_opcodes[OP_STORE_I], e, e2, NULL);
					QCC_PR_Statement3(&pr_opcodes[OP_SUB_I], e, QCC_MakeIntDef(1), e);
					QCC_FreeTemp(e);
					e = e2;
				}
				else
				{
					QCC_PR_ParseWarning(WARN_NOTSTANDARDBEHAVIOUR, "-- suffix operator results in nonstandard behaviour. Use -=1 or prefix form instead");
					QCC_PR_IncludeChunk("-=1", false, NULL);
				}
				QCC_PR_Lex();
			}
			break;	// next token isn't at this priority level
		}
	}
	if (e == NULL)
		QCC_PR_ParseError(ERR_INTERNAL, "e == null");
	return e;
}

void QCC_PR_GotoStatement (QCC_dstatement_t *patch2, char *labelname)
{
	if (num_gotos >= max_gotos)
	{
		max_gotos += 8;
		pr_gotos = realloc(pr_gotos, sizeof(*pr_gotos)*max_gotos);
	}

	strncpy(pr_gotos[num_gotos].name, labelname, sizeof(pr_gotos[num_gotos].name) -1);
	pr_gotos[num_gotos].lineno = pr_source_line;
	pr_gotos[num_gotos].statementno = patch2 - statements;

	num_gotos++;
}

/*
============
PR_ParseStatement

============
*/
void QCC_PR_ParseStatement (void)
{
	int continues;
	int breaks;
	int cases;
	int i;
	QCC_def_t				*e, *e2;
	QCC_dstatement_t		*patch1, *patch2, *patch3;

	if (QCC_PR_Check ("{"))
	{
		e = pr.localvars;
		while (!QCC_PR_Check("}"))
			QCC_PR_ParseStatement ();

		if (pr_subscopedlocals)
		{
			for	(e2 = pr.localvars; e2 != e; e2 = e2->nextlocal)
			{
				Hash_RemoveData(&localstable, e2->name, e2);
			}
		}
		return;
	}
	
	if (QCC_PR_Check("return"))
	{
		/*if (pr_classtype)
		{
			e = QCC_PR_GetDef(NULL, "__oself", pr_scope, false, 0);
			e2 = QCC_PR_GetDef(NULL, "self", NULL, false, 0);
			QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_ENT], e, QCC_PR_DummyDef(pr_classtype, "self", pr_scope, 1, e2->ofs, false), NULL));
		}*/

		if (QCC_PR_Check (";"))
		{
			if (pr_scope->type->aux_type->type != ev_void)
				QCC_PR_ParseWarning(WARN_MISSINGRETURNVALUE, "\'%s\' should return %s", pr_scope->name, pr_scope->type->aux_type->name);
			if (opt_return_only)
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_DONE], 0, 0, NULL));
			else
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_RETURN], 0, 0, NULL));
			return;
		}
		e = QCC_PR_Expression (TOP_PRIORITY);
		QCC_PR_Expect (";");
		if (pr_scope->type->aux_type->type != e->type->type)
			QCC_PR_ParseWarning(WARN_WRONGRETURNTYPE, "\'%s\' returned %s, expected %s", pr_scope->name, e->type->name, pr_scope->type->aux_type->name);
		QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_RETURN], e, 0, NULL));
		return;		
	}
	
	if (QCC_PR_Check("while"))
	{
		continues = num_continues;
		breaks = num_breaks;

		QCC_PR_Expect ("(");
		patch2 = &statements[numstatements];
		conditional = 1;
		e = QCC_PR_Expression (TOP_PRIORITY);
		conditional = 0;
		if (((e->constant && !e->temp) || !STRCMP(e->name, "IMMEDIATE")) && opt_compound_jumps)
		{
			optres_compound_jumps++;
			if (!G_INT(e->ofs))
			{
				QCC_PR_ParseWarning(0, "while(0)?");
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_GOTO], 0, 0, &patch1));
			}
			else
			{
				patch1 = NULL;
			}
		}
		else
		{
			if (e->constant && !e->temp)
			{
				if (!G_FLOAT(e->ofs))
					QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_GOTO], 0, 0, &patch1));
				else
					patch1 = NULL;
			}
			else if (e->type == type_string)	//special case, as strings are now pointers, not offsets from string table
			{
				QCC_PR_ParseWarning(0, "while (string) can result in bizzare behaviour");
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IFNOTS], e, 0, &patch1));
			}
			else
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IFNOT], e, 0, &patch1));
		}
		QCC_PR_Expect (")");	//after the line number is noted..
		QCC_PR_ParseStatement ();
		QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_GOTO], NULL, 0, &patch3));
		patch3->a = patch2 - patch3;
		if (patch1)
		{
			if (patch1->op == OP_GOTO)
				patch1->a = &statements[numstatements] - patch1;
			else
				patch1->b = &statements[numstatements] - patch1;
		}

		if (breaks != num_breaks)
		{
			for(i = breaks; i < num_breaks; i++)
			{
				patch1 = &statements[pr_breaks[i]];
				statements[pr_breaks[i]].a = &statements[numstatements] - patch1;	//jump to after the return-to-top goto
			}
			num_breaks = breaks;
		}
		if (continues != num_continues)
		{
			for(i = continues; i < num_continues; i++)
			{
				patch1 = &statements[pr_continues[i]];
				statements[pr_continues[i]].a = patch2 - patch1;	//jump back to top
			}
			num_continues = continues;
		}
		return;
	}
	if (keyword_for && QCC_PR_Check("for"))
	{
		int old_numstatements;
		int numtemp, i;

		int					linenum[32];
		QCC_dstatement_t		temp[sizeof(linenum)/sizeof(linenum[0])];

		continues = num_continues;
		breaks = num_breaks;

		QCC_PR_Expect("(");
		if (!QCC_PR_Check(";"))
		{
			do
			{
				QCC_FreeTemp(QCC_PR_Expression(TOP_PRIORITY));
			} while (QCC_PR_Check(","));
			QCC_PR_Expect(";");
		}

		patch2 = &statements[numstatements];
		if (!QCC_PR_Check(";"))
		{
			conditional = 1;
			e = QCC_PR_Expression(TOP_PRIORITY);
			while (QCC_PR_Check(","))	//logicops, string ops?
			{
				e = QCC_PR_Statement(pr_opcodes+OP_AND, e, QCC_PR_Expression(TOP_PRIORITY), NULL);
			}
			conditional = 0;
			QCC_PR_Expect(";");
		}
		else
			e = NULL;

		if (!QCC_PR_Check(")"))
		{
			old_numstatements = numstatements;
			do
			{
				QCC_FreeTemp(QCC_PR_Expression(TOP_PRIORITY));
			} while (QCC_PR_Check(","));

			numtemp = numstatements - old_numstatements;
			if (numtemp > sizeof(linenum)/sizeof(linenum[0]))
				QCC_PR_ParseError(ERR_TOOCOMPLEX, "Update expression too large");
			numstatements = old_numstatements;
			for (i = 0 ; i < numtemp ; i++)
			{
				linenum[i] = statement_linenums[numstatements + i];
				temp[i] = statements[numstatements + i];
			}

			QCC_PR_Expect(")");
		}
		else
			numtemp = 0;

		if (e)
			QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_IFNOT], e, 0, &patch1));
		else
			patch1 = NULL;
		if (!QCC_PR_Check(";"))
			QCC_PR_ParseStatement();	//don't give the hanging ';' warning.
		patch3 = &statements[numstatements];
		for (i = 0 ; i < numtemp ; i++)
		{
			statement_linenums[numstatements] = linenum[i];
			statements[numstatements++] = temp[i];
		}
		QCC_PR_SimpleStatement(OP_GOTO, patch2 - &statements[numstatements], 0, 0);
		if (patch1)
			patch1->b = &statements[numstatements] - patch1;

		if (breaks != num_breaks)
		{
			for(i = breaks; i < num_breaks; i++)
			{	
				patch1 = &statements[pr_breaks[i]];
				statements[pr_breaks[i]].a = &statements[numstatements] - patch1;
			}
			num_breaks = breaks;
		}
		if (continues != num_continues)
		{
			for(i = continues; i < num_continues; i++)
			{
				patch1 = &statements[pr_continues[i]];
				statements[pr_continues[i]].a = patch3 - patch1;
			}
			num_continues = continues;
		}

		return;
	}
	if (keyword_do && QCC_PR_Check("do"))
	{
		continues = num_continues;
		breaks = num_breaks;

		patch1 = &statements[numstatements];
		QCC_PR_ParseStatement ();
		QCC_PR_Expect ("while");
		QCC_PR_Expect ("(");
		conditional = 1;
		e = QCC_PR_Expression (TOP_PRIORITY);
		conditional = 0;

		if (e->constant && !e->temp)
		{
			if (G_FLOAT(e->ofs))
			{
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_GOTO], NULL, 0, &patch2));
				patch2->a = patch1 - patch2;
			}
		}
		else
		{
			if (e->type == type_string && flag_ifstring)
			{
				QCC_PR_ParseWarning(WARN_IFSTRING_USED, "do {} while(string) can result in bizzare behaviour");
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IFS], e, NULL, &patch2));
			}
			else
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IF], e, NULL, &patch2));

			patch2->b = patch1 - patch2;
		}

		QCC_PR_Expect (")");
		QCC_PR_Expect (";");

		if (breaks != num_breaks)
		{
			for(i = breaks; i < num_breaks; i++)
			{
				patch2 = &statements[pr_breaks[i]];
				statements[pr_breaks[i]].a = &statements[numstatements] - patch2;
			}
			num_breaks = breaks;
		}
		if (continues != num_continues)
		{
			for(i = continues; i < num_continues; i++)
			{
				patch2 = &statements[pr_continues[i]];
				statements[pr_continues[i]].a = patch1 - patch2;
			}
			num_continues = continues;
		}

		return;
	}
	
	if (QCC_PR_Check("local"))
	{
//		if (locals_end != numpr_globals)	//is this breaking because of locals?
//			QCC_PR_ParseWarning("local vars after temp vars\n");
		QCC_PR_ParseDefs (NULL);
		locals_end = numpr_globals;
		return;
	}

	if (pr_token_type == tt_name)
	if ((keyword_var && !STRCMP ("var", pr_token)) ||
		(keyword_string && !STRCMP ("string", pr_token)) ||
		(keyword_float && !STRCMP ("float", pr_token)) ||
		(keyword_entity && !STRCMP ("entity", pr_token)) ||
		(keyword_vector && !STRCMP ("vector", pr_token)) ||
		(keyword_integer && !STRCMP ("integer", pr_token)) ||
		(keyword_int && !STRCMP ("int", pr_token)) ||
		(keyword_class && !STRCMP ("class", pr_token)) ||
		(keyword_const && !STRCMP ("const", pr_token)))
	{
//		if (locals_end != numpr_globals)	//is this breaking because of locals?
//			QCC_PR_ParseWarning("local vars after temp vars\n");
		QCC_PR_ParseDefs (NULL);
		locals_end = numpr_globals;
		return;
	}

	if (keyword_state && QCC_PR_Check("state"))
	{
		QCC_PR_Expect("[");
		QCC_PR_ParseState();
		QCC_PR_Expect(";");
		return;
	}
	
	if (QCC_PR_Check("if"))
	{
		if (QCC_PR_Check("not"))
		{
			QCC_PR_Expect ("(");
			conditional = 1;
			e = QCC_PR_Expression (TOP_PRIORITY);
			conditional = 0;

			if (e->type == type_string && flag_ifstring)	//special case, as strings are now pointers, not offsets from string table
			{
				QCC_PR_ParseWarning(WARN_IFSTRING_USED, "if not(string) can result in bizzare behaviour");
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IFS], e, 0, &patch1));
			}
			else
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IF], e, 0, &patch1));
		}
		else
		{
			QCC_PR_Expect ("(");
			conditional = 1;
			e = QCC_PR_Expression (TOP_PRIORITY);
			conditional = 0;

			if (e->type == type_string && flag_ifstring)	//special case, as strings are now pointers, not offsets from string table
			{
				QCC_PR_ParseWarning(WARN_IFSTRING_USED, "if (string) can result in bizzare behaviour");
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IFNOTS], e, 0, &patch1));
			}
			else
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IFNOT], e, 0, &patch1));
		}

		QCC_PR_Expect (")");	//close bracket is after we save the statement to mem (so debugger does not show the if statement as being on the line after

		QCC_PR_ParseStatement ();

		if (QCC_PR_Check ("else"))
		{
			int lastwasreturn;
			lastwasreturn = statements[numstatements-1].op == OP_RETURN || statements[numstatements-1].op == OP_DONE;

			//nothing jumped to it, so it's not a problem!
			if (lastwasreturn && opt_compound_jumps && !QCC_AStatementJumpsTo(numstatements, patch1-statements, numstatements))
			{
//				QCC_PR_ParseWarning(0, "optimised the else");
				optres_compound_jumps++;
				patch1->b = &statements[numstatements] - patch1;
				QCC_PR_ParseStatement ();
			}
			else
			{
//				QCC_PR_ParseWarning(0, "using the else");
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_GOTO], 0, 0, &patch2));
				patch1->b = &statements[numstatements] - patch1;
				QCC_PR_ParseStatement ();
				patch2->a = &statements[numstatements] - patch2;
			}
		}
		else
			patch1->b = &statements[numstatements] - patch1;

		return;
	}
	if (keyword_switch && QCC_PR_Check("switch"))
	{
		int op;
		int hcstyle;
		int defaultcase = -1;
		temp_t *et;
		int oldst;

		breaks = num_breaks;
		cases = num_cases;


		QCC_PR_Expect ("(");

		conditional = 1;
		e = QCC_PR_Expression (TOP_PRIORITY);
		conditional = 0;

		if (e == &def_ret)
		{	//copy it out, so our hack just below doesn't crash us
			if (e->type->type == ev_vector)
				e = QCC_PR_Statement(pr_opcodes+OP_STORE_V, QCC_GetTemp(type_vector), e, NULL);
			else
				e = QCC_PR_Statement(pr_opcodes+OP_STORE_F, QCC_GetTemp(type_float), e, NULL);
		}
		et = e->temp;
		e->temp = NULL;	//so noone frees it until we finish this loop

		//expands

		//switch (CONDITION)
		//{
		//case 1:
		//	break;
		//case 2:
		//default:
		//	break;
		//}
		
		//to

		// x = CONDITION, goto start
		// l1:
		//	goto end
		// l2:
		// def:
		//	goto end
		//	goto end			P1
		// start:
		//	if (x == 1) goto l1;
		//	if (x == 2) goto l2;
		//	goto def
		// end:

		//x is emitted in an opcode, stored as a register that we cannot access later.
		//it should be possible to nest these.
		
		switch(e->type->type)
		{
		case ev_float:
			op = OP_SWITCH_F;
			break;
		case ev_entity:	//whu???
			op = OP_SWITCH_E;
			break;
		case ev_vector:
			op = OP_SWITCH_V;
			break;
		case ev_string:
			op = OP_SWITCH_S;
			break;
		case ev_function:
			op = OP_SWITCH_FNC;
			break;
		default:	//err hmm.
			op = 0;
			break;
		}

		if (op)
			hcstyle = QCC_OPCodeValid(&pr_opcodes[op]);
		else
			hcstyle = false;


		if (hcstyle)
			QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[op], e, 0, &patch1));
		else
			QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_GOTO], e, 0, &patch1));

		QCC_PR_Expect (")");	//close bracket is after we save the statement to mem (so debugger does not show the if statement as being on the line after
		
		oldst = numstatements;
		QCC_PR_ParseStatement ();

		//this is so that a missing goto at the end of your switch doesn't end up in the jumptable again
		if (oldst == numstatements || !QCC_StatementIsAJump(numstatements-1, numstatements-1))
		{
			QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_GOTO], 0, 0, &patch2));	//the P1 statement/the theyforgotthebreak statement.
//			QCC_PR_ParseWarning(0, "emitted goto");
		}
		else
		{
			patch2 = NULL;
//			QCC_PR_ParseWarning(0, "No goto");
		}

		if (hcstyle)
			patch1->b = &statements[numstatements] - patch1;	//the goto start part
		else
			patch1->a = &statements[numstatements] - patch1;	//the goto start part

		for (i = cases; i < num_cases; i++)
		{
			if (!pr_casesdef[i])
			{
				if (defaultcase >= 0)
					QCC_PR_ParseError(ERR_MULTIPLEDEFAULTS, "Duplicated default case");
				defaultcase = i;
			}
			else
			{
				if (pr_casesdef[i]->type->type != e->type->type)
					QCC_PR_ParseWarning(WARN_SWITCHTYPEMISMATCH, "switch case type mismatch");
				if (pr_casesdef2[i])
				{
					if (pr_casesdef2[i]->type->type != e->type->type)
						QCC_PR_ParseWarning(WARN_SWITCHTYPEMISMATCH, "switch caserange type mismatch");

					if (hcstyle)
					{
						QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_CASERANGE], pr_casesdef[i], pr_casesdef2[i], &patch3));
						patch3->c = &statements[pr_cases[i]] - patch3;
					}
					else
					{
						QCC_def_t *e3;
						e2 = QCC_PR_Statement (&pr_opcodes[OP_GE], e, pr_casesdef[i], NULL);
						e3 = QCC_PR_Statement (&pr_opcodes[OP_LE], e, pr_casesdef2[i], NULL);
						e2 = QCC_PR_Statement (&pr_opcodes[OP_AND], e2, e3, NULL);
						QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IF], e2, 0, &patch3));
						patch3->b = &statements[pr_cases[i]] - patch3;
					}
				}
				else
				{
					if (hcstyle)
					{
						QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_CASE], pr_casesdef[i], 0, &patch3));
						patch3->b = &statements[pr_cases[i]] - patch3;
					}
					else
					{
						if (!pr_casesdef[i]->constant || G_INT(pr_casesdef[i]->ofs))
						{
							switch(e->type->type)
							{
							case ev_float:
								e2 = QCC_PR_Statement (&pr_opcodes[OP_EQ_F], e, pr_casesdef[i], NULL);
								break;
							case ev_entity:	//whu???
								e2 = QCC_PR_Statement (&pr_opcodes[OP_EQ_E], e, pr_casesdef[i], &patch1);
								break;
							case ev_vector:
								e2 = QCC_PR_Statement (&pr_opcodes[OP_EQ_V], e, pr_casesdef[i], &patch1);
								break;
							case ev_string:
								e2 = QCC_PR_Statement (&pr_opcodes[OP_EQ_S], e, pr_casesdef[i], &patch1);
								break;
							case ev_function:
								e2 = QCC_PR_Statement (&pr_opcodes[OP_EQ_FNC], e, pr_casesdef[i], &patch1);
								break;
							case ev_field:
								e2 = QCC_PR_Statement (&pr_opcodes[OP_EQ_FNC], e, pr_casesdef[i], &patch1);
								break;
							case ev_integer:
								e2 = QCC_PR_Statement (&pr_opcodes[OP_EQ_I], e, pr_casesdef[i], &patch1);
								break;
							default:
								QCC_PR_ParseError(ERR_BADSWITCHTYPE, "Bad switch type");
								e2 = NULL;
								break;
							}
							QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IF], e2, 0, &patch3));
						}
						else
						{
							if (e->type->type == ev_string)
								QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IFNOTS], e, 0, &patch3));
							else
								QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IFNOT], e, 0, &patch3));
						}
						patch3->b = &statements[pr_cases[i]] - patch3;
					}
				}
			}	
		}
		if (defaultcase>=0)
		{
			QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_GOTO], 0, 0, &patch3));
			patch3->a = &statements[pr_cases[defaultcase]] - patch3;
		}

		num_cases = cases;


		patch3 = &statements[numstatements];
		if (patch2)
			patch2->a = patch3 - patch2;	//set P1 jump

		if (breaks != num_breaks)
		{
			for(i = breaks; i < num_breaks; i++)
			{
				patch2 = &statements[pr_breaks[i]];
				patch2->a = patch3 - patch2;
			}
			num_breaks = breaks;
		}

		e->temp = et;
		QCC_FreeTemp(e);
		return;
	}

	if (keyword_asm && QCC_PR_Check("asm"))
	{
		if (QCC_PR_Check("{"))
		{
			while (!QCC_PR_Check("}"))
				QCC_PR_ParseAsm ();
		}
		else
			QCC_PR_ParseAsm ();
		return;
	}

	if (QCC_PR_Check(":"))
	{
		if (pr_token_type != tt_name)
		{
			QCC_PR_ParseError(ERR_BADLABELNAME, "invalid label name \"%s\"", pr_token);
			return;
		}

		for (i = 0; i < num_labels; i++)
			if (!STRNCMP(pr_labels[i].name, pr_token, sizeof(pr_labels[num_labels].name) -1))
			{
				QCC_PR_ParseWarning(WARN_DUPLICATELABEL, "Duplicate label %s", pr_token);
				QCC_PR_Lex();
				return;
			}

		if (num_labels >= max_labels)
		{
			max_labels += 8;
			pr_labels = realloc(pr_labels, sizeof(*pr_labels)*max_labels);
		}

		strncpy(pr_labels[num_labels].name, pr_token, sizeof(pr_labels[num_labels].name) -1);
		pr_labels[num_labels].lineno = pr_source_line;
		pr_labels[num_labels].statementno = numstatements;

		num_labels++;

//		QCC_PR_ParseWarning("Gotos are evil");
		QCC_PR_Lex();
		return;
	}
	if (keyword_goto && QCC_PR_Check("goto"))
	{
		if (pr_token_type != tt_name)
		{
			QCC_PR_ParseError(ERR_NOLABEL, "invalid label name \"%s\"", pr_token);
			return;
		}

		QCC_PR_Statement (&pr_opcodes[OP_GOTO], 0, 0, &patch2);

		QCC_PR_GotoStatement (patch2, pr_token);

//		QCC_PR_ParseWarning("Gotos are evil");
		QCC_PR_Lex();
		QCC_PR_Expect(";");
		return;
	}

	if (keyword_break && QCC_PR_Check("break"))
	{
		if (!STRCMP ("(", pr_token))
		{	//make sure it wasn't a call to the break function.
			QCC_PR_IncludeChunk("break(", true, NULL);
			QCC_PR_Lex();	//so it sees the break.
		}
		else
		{
			if (num_breaks >= max_breaks)
			{
				max_breaks += 8;
				pr_breaks = realloc(pr_breaks, sizeof(*pr_breaks)*max_breaks);
			}
			pr_breaks[num_breaks] = numstatements;
			QCC_PR_Statement (&pr_opcodes[OP_GOTO], 0, 0, NULL);
			num_breaks++;
			QCC_PR_Expect(";");
			return;
		}
	}
	if (keyword_continue && QCC_PR_Check("continue"))
	{
		if (num_continues >= max_continues)
		{
			max_continues += 8;
			pr_continues = realloc(pr_continues, sizeof(*pr_continues)*max_continues);
		}
		pr_continues[num_continues] = numstatements;
		QCC_PR_Statement (&pr_opcodes[OP_GOTO], 0, 0, NULL);
		num_continues++;
		QCC_PR_Expect(";");
		return;
	}
	if (keyword_case && QCC_PR_Check("case"))
	{
		if (num_cases >= max_cases)
		{
			max_cases += 8;
			pr_cases = realloc(pr_cases, sizeof(*pr_cases)*max_cases);
			pr_casesdef = realloc(pr_casesdef, sizeof(*pr_casesdef)*max_cases);
			pr_casesdef2 = realloc(pr_casesdef2, sizeof(*pr_casesdef2)*max_cases);
		}
		pr_cases[num_cases] = numstatements;
		pr_casesdef[num_cases] = QCC_PR_Expression (TOP_PRIORITY);
		if (QCC_PR_Check(".."))
		{
			pr_casesdef2[num_cases] = QCC_PR_Expression (TOP_PRIORITY);
			if (pr_casesdef[num_cases]->constant && pr_casesdef2[num_cases]->constant &&
				!pr_casesdef[num_cases]->temp && !pr_casesdef2[num_cases]->temp)
				if (G_FLOAT(pr_casesdef[num_cases]->ofs) >= G_FLOAT(pr_casesdef2[num_cases]->ofs))
					QCC_PR_ParseError(ERR_CASENOTIMMEDIATE, "Caserange statement uses backwards range\n");
		}
		else
			pr_casesdef2[num_cases] = NULL;

		if (numstatements != pr_cases[num_cases])
			QCC_PR_ParseError(ERR_CASENOTIMMEDIATE, "Case statements may not use formulas\n");
		num_cases++;
		QCC_PR_Expect(":");
		return;
	}
	if (keyword_default && QCC_PR_Check("default"))
	{
		if (num_cases >= max_cases)
		{
			max_cases += 8;
			pr_cases = realloc(pr_cases, sizeof(*pr_cases)*max_cases);
			pr_casesdef = realloc(pr_casesdef, sizeof(*pr_casesdef)*max_cases);
			pr_casesdef2 = realloc(pr_casesdef2, sizeof(*pr_casesdef2)*max_cases);
		}
		pr_cases[num_cases] = numstatements;
		pr_casesdef[num_cases] = NULL;
		pr_casesdef2[num_cases] = NULL;
		num_cases++;
		QCC_PR_Expect(":");
		return;
	}

	if (keyword_thinktime && QCC_PR_Check("thinktime"))
	{
		QCC_def_t *nextthink;
		QCC_def_t *time;
		e = QCC_PR_Expression (TOP_PRIORITY);
		QCC_PR_Expect(":");
		e2 = QCC_PR_Expression (TOP_PRIORITY);
		if (e->type->type != ev_entity || e2->type->type != ev_float)
			QCC_PR_ParseError(ERR_THINKTIMETYPEMISMATCH, "thinktime type mismatch");

		if (QCC_OPCodeValid(&pr_opcodes[OP_THINKTIME]))
			QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_THINKTIME], e, e2, NULL));
		else
		{
			nextthink = QCC_PR_GetDef(NULL, "nextthink", NULL, false, 0);
			if (!nextthink)
				QCC_PR_ParseError (ERR_UNKNOWNVALUE, "Unknown value \"%s\"", "nextthink");
			time = QCC_PR_GetDef(type_float, "time", NULL, false, 0);
			if (!time)
				QCC_PR_ParseError (ERR_UNKNOWNVALUE, "Unknown value \"%s\"", "time");
			nextthink = QCC_PR_Statement(&pr_opcodes[OP_ADDRESS], e, nextthink, NULL);
			time = QCC_PR_Statement(&pr_opcodes[OP_ADD_F], time, e2, NULL);
			QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STOREP_F], time, nextthink, NULL));
		}
		QCC_PR_Expect(";");
		return;
	}
	if (QCC_PR_Check(";"))
	{
		QCC_PR_ParseWarning(WARN_POINTLESSSTATEMENT, "Hanging ';'");
		return;
	}

//	qcc_functioncalled=0;

	qcc_usefulstatement = false;
	e = QCC_PR_Expression (TOP_PRIORITY);
	QCC_PR_Expect (";");

	if (e->type->type != ev_void && !qcc_usefulstatement)
		QCC_PR_ParseWarning(WARN_POINTLESSSTATEMENT, "Effectless statement");

	QCC_FreeTemp(e);

//	qcc_functioncalled=false;
}


/*
==============
PR_ParseState

States are special functions made for convenience.  They automatically
set frame, nextthink (implicitly), and think (allowing forward definitions).

// void() name = [framenum, nextthink] {code}
// expands to:
// function void name ()
// {
//		self.frame=framenum;
//		self.nextthink = time + 0.1;
//		self.think = nextthink
//		<code>
// };
==============
*/
void QCC_PR_ParseState (void)
{
	char	*name;
	QCC_def_t	*s1, *def, *sc = pr_scope;
	char f;

	f = *pr_token;
	if (QCC_PR_Check("++") || QCC_PR_Check("--"))
	{
		s1 = QCC_PR_ParseImmediate ();
		QCC_PR_Expect("..");
		def = QCC_PR_ParseImmediate ();
		QCC_PR_Expect ("]");

		if (s1->type->type != ev_float || def->type->type != ev_float)
			QCC_PR_ParseError(ERR_STATETYPEMISMATCH, "state type mismatch");
	

		if (QCC_OPCodeValid(&pr_opcodes[OP_CSTATE]))
			QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_CSTATE], s1, def, NULL));
		else
		{
			QCC_def_t *t1, *t2;
			QCC_def_t *framef, *frame;
			QCC_def_t *self;
			QCC_def_t *cycle_wrapped;
			self = QCC_PR_GetDef(type_entity, "self", NULL, false, 0);
			framef = QCC_PR_GetDef(NULL, "frame", NULL, false, 0);
			cycle_wrapped = QCC_PR_GetDef(type_float, "cycle_wrapped", NULL, false, 0);

			frame = QCC_PR_Statement(&pr_opcodes[OP_LOAD_F], self, framef, NULL);
			QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_F], QCC_MakeFloatDef(0), cycle_wrapped, NULL));

			//make sure the frame is within the bounds given.
			t1 = QCC_PR_Statement(&pr_opcodes[OP_LT], frame, s1, NULL);
			QCC_UnFreeTemp(frame);
			t2 = QCC_PR_Statement(&pr_opcodes[OP_GT], frame, def, NULL);
			QCC_UnFreeTemp(frame);
			t1 = QCC_PR_Statement(&pr_opcodes[OP_OR], t1, t2, NULL);
			QCC_PR_SimpleStatement(OP_IFNOT, t1->ofs, 2, 0);
			QCC_FreeTemp(t1);
				QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_F], s1, frame, NULL));
			  QCC_PR_SimpleStatement(OP_GOTO, t1->ofs, 13, 0);

			t1 = QCC_PR_Statement(&pr_opcodes[OP_GE], def, s1, NULL);
			QCC_PR_SimpleStatement(OP_IFNOT, t1->ofs, 7, 0);
			QCC_FreeTemp(t1);	//this block is the 'it's in a forwards direction'
				QCC_PR_SimpleStatement(OP_ADD_F, frame->ofs, QCC_MakeFloatDef(1)->ofs, frame->ofs);
				t1 = QCC_PR_Statement(&pr_opcodes[OP_GT], frame, def, NULL);
				QCC_PR_SimpleStatement(OP_IFNOT, t1->ofs,2, 0);
				QCC_FreeTemp(t1);
					QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_F], s1, frame, NULL));
					QCC_UnFreeTemp(frame);
					QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_F], QCC_MakeFloatDef(1), cycle_wrapped, NULL));

			QCC_PR_SimpleStatement(OP_GOTO, 6, 0, 0);
				//reverse animation.
				QCC_PR_SimpleStatement(OP_SUB_F, frame->ofs, QCC_MakeFloatDef(1)->ofs, frame->ofs);
				t1 = QCC_PR_Statement(&pr_opcodes[OP_LT], frame, s1, NULL);
				QCC_PR_SimpleStatement(OP_IFNOT, t1->ofs,2, 0);
				QCC_FreeTemp(t1);
					QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_F], def, frame, NULL));
					QCC_UnFreeTemp(frame);
					QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_F], QCC_MakeFloatDef(1), cycle_wrapped, NULL));
	
			//self.frame = frame happens with the normal state opcode.
			QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_STATE], frame, pr_scope, NULL));
			QCC_FreeTemp(frame);
		}
		return;
	}
	
	if (pr_token_type != tt_immediate || pr_immediate_type != type_float)
		QCC_PR_ParseError (ERR_STATETYPEMISMATCH, "state frame must be a number");
	s1 = QCC_PR_ParseImmediate ();
	
	QCC_PR_Check (",");

	name = QCC_PR_ParseName ();
	pr_scope = NULL;
	def = QCC_PR_GetDef (type_function, name, NULL, true, 1);
	pr_scope = sc;
		
	QCC_PR_Expect ("]");
	
	QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_STATE], s1, def, NULL));
}

void QCC_PR_ParseAsm(void)
{
	QCC_dstatement_t *patch1;
	int op, p;
	QCC_def_t *a, *b, *c;

	if (QCC_PR_Check("local"))
	{
		QCC_PR_ParseDefs (NULL);
		locals_end = numpr_globals;
		return;
	}

	for (op = 0; op < OP_NUMOPS; op++)
	{
		if (!STRCMP(pr_token, pr_opcodes[op].opname))
		{
			QCC_PR_Lex();
			if (pr_opcodes[op].priority==-1 && pr_opcodes[op].associative!=ASSOC_LEFT)
			{
				if (pr_opcodes[op].type_a==NULL)
				{
					patch1 = &statements[numstatements];

					QCC_PR_Statement3(&pr_opcodes[op], NULL, NULL, NULL);

					if (pr_token_type == tt_name)
					{
						QCC_PR_GotoStatement(patch1, QCC_PR_ParseName());
					}
					else
					{
						p = (int)pr_immediate._float;
						patch1->a = (int)p;
					}

					QCC_PR_Lex();
				}
				else if (pr_opcodes[op].type_b==NULL)
				{
					patch1 = &statements[numstatements];

					a = QCC_PR_ParseValue(pr_classtype);
					QCC_PR_Statement3(&pr_opcodes[op], a, NULL, NULL);

					if (pr_token_type == tt_name)
					{
						QCC_PR_GotoStatement(patch1, QCC_PR_ParseName());
					}
					else
					{
						p = (int)pr_immediate._float;
						patch1->b = (int)p;
					}

					QCC_PR_Lex();
				}
				else
				{
					patch1 = &statements[numstatements];

					a = QCC_PR_ParseValue(pr_classtype);
					b = QCC_PR_ParseValue(pr_classtype);
					QCC_PR_Statement3(&pr_opcodes[op], a, b, NULL);

					if (pr_token_type == tt_name)
					{
						QCC_PR_GotoStatement(patch1, QCC_PR_ParseName());
					}
					else
					{
						p = (int)pr_immediate._float;
						patch1->c = (int)p;
					}

					QCC_PR_Lex();
				}
			}
			else
			{				
				if (pr_opcodes[op].type_a != &type_void)
					a = QCC_PR_ParseValue(pr_classtype);
				else
					a=NULL;
				if (pr_opcodes[op].type_b != &type_void)
					b = QCC_PR_ParseValue(pr_classtype);
				else
					b=NULL;
				if (pr_opcodes[op].associative==ASSOC_LEFT && pr_opcodes[op].type_c != &type_void)
					c = QCC_PR_ParseValue(pr_classtype);
				else
					c=NULL;

				QCC_PR_Statement3(&pr_opcodes[op], a, b, c);
			}
			
			QCC_PR_Expect(";");
			return;
		}
	}
	QCC_PR_ParseError(ERR_BADOPCODE, "Bad op code name %s", pr_token);
}

pbool QCC_FuncJumpsTo(int first, int last, int statement)
{
	int st;
	for (st = first; st < last; st++)
	{
		if (pr_opcodes[statements[st].op].type_a == NULL)
		{
			if (st + (signed)statements[st].a == statement)
			{
				if (st != first)
				{
					if (statements[st-1].op == OP_RETURN)
						continue;
					if (statements[st-1].op == OP_DONE)
						continue;
					return true;
				}
			}
		}
		if (pr_opcodes[statements[st].op].type_b == NULL)
		{
			if (st + (signed)statements[st].b == statement)
			{
				if (st != first)
				{
					if (statements[st-1].op == OP_RETURN)
						continue;
					if (statements[st-1].op == OP_DONE)
						continue;
					return true;
				}
			}
		}
		if (pr_opcodes[statements[st].op].type_c == NULL)
		{
			if (st + (signed)statements[st].c == statement)
			{
				if (st != first)
				{
					if (statements[st-1].op == OP_RETURN)
						continue;
					if (statements[st-1].op == OP_DONE)
						continue;
					return true;
				}
			}
		}
	}
	return false;
}

pbool QCC_FuncJumpsToRange(int first, int last, int firstr, int lastr)
{
	int st;
	for (st = first; st < last; st++)
	{
		if (pr_opcodes[statements[st].op].type_a == NULL)
		{
			if (st + (signed)statements[st].a >= firstr && st + (signed)statements[st].a <= lastr)
			{
				if (st != first)
				{
					if (statements[st-1].op == OP_RETURN)
						continue;
					if (statements[st-1].op == OP_DONE)
						continue;
					return true;
				}
			}
		}
		if (pr_opcodes[statements[st].op].type_b == NULL)
		{
			if (st + (signed)statements[st].b >= firstr && st + (signed)statements[st].b <= lastr)
			{
				if (st != first)
				{
					if (statements[st-1].op == OP_RETURN)
						continue;
					if (statements[st-1].op == OP_DONE)
						continue;
					return true;
				}
			}
		}
		if (pr_opcodes[statements[st].op].type_c == NULL)
		{
			if (st + (signed)statements[st].c >= firstr && st + (signed)statements[st].c <= lastr)
			{
				if (st != first)
				{
					if (statements[st-1].op == OP_RETURN)
						continue;
					if (statements[st-1].op == OP_DONE)
						continue;
					return true;
				}
			}
		}
	}
	return false;
}

#if 0
void QCC_CompoundJumps(int first, int last)
{
	//jumps to jumps are reordered so they become jumps to the final target.
	int statement;
	int st;
	for (st = first; st < last; st++)
	{
		if (pr_opcodes[statements[st].op].type_a == NULL)
		{
			statement = st + (signed)statements[st].a;
			if (statements[statement].op == OP_RETURN || statements[statement].op == OP_DONE)
			{	//goto leads to return. Copy the command out to remove the goto.
				statements[st].op = statements[statement].op;
				statements[st].a = statements[statement].a;
				statements[st].b = statements[statement].b;
				statements[st].c = statements[statement].c;
				optres_compound_jumps++;
			}
			while (statements[statement].op == OP_GOTO)
			{
				statements[st].a = statement+statements[statement].a - st;
				statement = st + (signed)statements[st].a;
				optres_compound_jumps++;
			}
		}
		if (pr_opcodes[statements[st].op].type_b == NULL)
		{
			statement = st + (signed)statements[st].b;
			while (statements[statement].op == OP_GOTO)
			{
				statements[st].b = statement+statements[statement].a - st;
				statement = st + (signed)statements[st].b;
				optres_compound_jumps++;
			}
		}
		if (pr_opcodes[statements[st].op].type_c == NULL)
		{
			statement = st + (signed)statements[st].c;
			while (statements[statement].op == OP_GOTO)
			{
				statements[st].c = statement+statements[statement].a - st;
				statement = st + (signed)statements[st].c;
				optres_compound_jumps++;
			}
		}
	}
}
#else
void QCC_CompoundJumps(int first, int last)
{
	//jumps to jumps are reordered so they become jumps to the final target.
	int statement;
	int st;
	int infloop;
	for (st = first; st < last; st++)
	{
		if (pr_opcodes[statements[st].op].type_a == NULL)
		{
			statement = st + (signed)statements[st].a;
			if (statements[statement].op == OP_RETURN || statements[statement].op == OP_DONE)
			{	//goto leads to return. Copy the command out to remove the goto.
				statements[st].op = statements[statement].op;
				statements[st].a = statements[statement].a;
				statements[st].b = statements[statement].b;
				statements[st].c = statements[statement].c;
				optres_compound_jumps++;
			}
			infloop = 1000;
			while (statements[statement].op == OP_GOTO)
			{
				if (!infloop--)
				{
					QCC_PR_ParseWarning(0, "Infinate loop detected");
					break;
				}
				statements[st].a = (statement+statements[statement].a - st);
				statement = st + (signed)statements[st].a;
				optres_compound_jumps++;
			}
		}
		if (pr_opcodes[statements[st].op].type_b == NULL)
		{
			statement = st + (signed)statements[st].b;
			infloop = 1000;
			while (statements[statement].op == OP_GOTO)
			{
				if (!infloop--)
				{
					QCC_PR_ParseWarning(0, "Infinate loop detected");
					break;
				}
				statements[st].b = (statement+statements[statement].a - st);
				statement = st + (signed)statements[st].b;
				optres_compound_jumps++;
			}
		}
		if (pr_opcodes[statements[st].op].type_c == NULL)
		{
			statement = st + (signed)statements[st].c;
			infloop = 1000;
			while (statements[statement].op == OP_GOTO)
			{
				if (!infloop--)
				{
					QCC_PR_ParseWarning(0, "Infinate loop detected");
					break;
				}
				statements[st].c = (statement+statements[statement].a - st);
				statement = st + (signed)statements[st].c;
				optres_compound_jumps++;
			}
		}
	}
}
#endif

void QCC_CheckForDeadAndMissingReturns(int first, int last, int rettype)
{
	int st, st2;

	if (statements[last-1].op == OP_DONE)
		last--;	//don't want the done
	
	if (rettype != ev_void)
		if (statements[last-1].op != OP_RETURN)
		{
			if (statements[last-1].op != OP_GOTO || (signed)statements[last-1].a > 0)
			{
				QCC_PR_ParseWarning(WARN_MISSINGRETURN, "%s: not all control paths return a value", pr_scope->name );
				return;
			}
		}

	for (st = first; st < last; st++)
	{
		if (statements[st].op == OP_RETURN || statements[st].op == OP_GOTO)
		{
			st++;
			if (st == last)
				continue;	//erm... end of function doesn't count as unreachable.

			if (!opt_compound_jumps)
			{	//we can ignore single statements like these without compound jumps (compound jumps correctly removes all).
				if (statements[st].op == OP_GOTO)	//inefficient compiler, we can ignore this.
					continue;
				if (statements[st].op == OP_DONE)	//inefficient compiler, we can ignore this.
					continue;
				if (statements[st].op == OP_RETURN)	//inefficient compiler, we can ignore this.
					continue;
			}

			//make sure something goes to just after this return.
			for (st2 = first; st2 < last; st2++)
			{
				if (pr_opcodes[statements[st2].op].type_a == NULL)
				{
					if (st2 + (signed)statements[st2].a == st)
						break;
				}
				if (pr_opcodes[statements[st2].op].type_b == NULL)
				{
					if (st2 + (signed)statements[st2].b == st)
						break;
				}
				if (pr_opcodes[statements[st2].op].type_c == NULL)
				{
					if (st2 + (signed)statements[st2].c == st)
						break;
				}
			}
			if (st2 == last)
			{
				QCC_PR_ParseWarning(WARN_UNREACHABLECODE, "%s: contains unreachable code", pr_scope->name );
			}
			continue;
		}
		if (rettype != ev_void)
		{
			if (pr_opcodes[statements[st].op].type_a == NULL)
			{
				if (st + (signed)statements[st].a == last)
				{
					QCC_PR_ParseWarning(WARN_MISSINGRETURN, "%s: not all control paths return a value", pr_scope->name );
					return;
				}
			}
			if (pr_opcodes[statements[st].op].type_b == NULL)
			{
				if (st + (signed)statements[st].b == last)
				{
					QCC_PR_ParseWarning(WARN_MISSINGRETURN, "%s: not all control paths return a value", pr_scope->name );
					return;
				}
			}
			if (pr_opcodes[statements[st].op].type_c == NULL)
			{
				if (st + (signed)statements[st].c == last)
				{
					QCC_PR_ParseWarning(WARN_MISSINGRETURN, "%s: not all control paths return a value", pr_scope->name );
					return;
				}
			}
		}
	}
}

pbool QCC_StatementIsAJump(int stnum, int notifdest)	//only the unconditionals.
{
	if (statements[stnum].op == OP_RETURN)
		return true;
	if (statements[stnum].op == OP_DONE)
		return true;
	if (statements[stnum].op == OP_GOTO)
		if ((int)statements[stnum].a != notifdest)
			return true;
	return false;
}

int QCC_AStatementJumpsTo(int targ, int first, int last)
{
	int st;
	for (st = first; st < last; st++)
	{
		if (pr_opcodes[statements[st].op].type_a == NULL)
		{
			if (st + (signed)statements[st].a == targ && statements[st].a)
			{
				return true;
			}
		}
		if (pr_opcodes[statements[st].op].type_b == NULL)
		{
			if (st + (signed)statements[st].b == targ)
			{
				return true;
			}
		}
		if (pr_opcodes[statements[st].op].type_c == NULL)
		{
			if (st + (signed)statements[st].c == targ)
			{
				return true;
			}
		}
	}

	for (st = 0; st < num_labels; st++)	//assume it's used.
	{
		if (pr_labels[st].statementno == targ)
			return true;
	}


	return false;
}
/*
//goes through statements, if it sees a matching statement earlier, it'll strim out the current.
void QCC_CommonSubExpressionRemoval(int first, int last)
{
	int cur;	//the current
	int prev;	//the earlier statement
	for (cur = last-1; cur >= first; cur--)
	{
		if (pr_opcodes[statements[cur].op].priority == -1)
			continue;
		for (prev = cur-1; prev >= first; prev--)
		{
			if (statements[prev].op >= OP_CALL0 && statements[prev].op <= OP_CALL8)
			{
				optres_test1++;
				break;
			}
			if (statements[prev].op >= OP_CALL1H && statements[prev].op <= OP_CALL8H)
			{
				optres_test1++;
				break;
			}
			if (pr_opcodes[statements[prev].op].right_associative)
			{	//make sure no changes to var_a occur.
				if (statements[prev].b == statements[cur].a)
				{
					optres_test2++;
					break;
				}
				if (statements[prev].b == statements[cur].b && !pr_opcodes[statements[cur].op].right_associative)
				{
					optres_test2++;
					break;
				}
			}
			else
			{
				if (statements[prev].c == statements[cur].a)
				{
					optres_test2++;
					break;
				}
				if (statements[prev].c == statements[cur].b && !pr_opcodes[statements[cur].op].right_associative)
				{
					optres_test2++;
					break;
				}
			}

			if (statements[prev].op == statements[cur].op)
				if (statements[prev].a == statements[cur].a)
					if (statements[prev].b == statements[cur].b)
						if (statements[prev].c == statements[cur].c)
						{
							if (!QCC_FuncJumpsToRange(first, last, prev, cur))
							{
								statements[cur].op = OP_STORE_F;
								statements[cur].a = 28;
								statements[cur].b = 28;
								optres_comexprremoval++;
							}
							else
								optres_test1++;
							break;
						}
		}
	}
}
*/

void QCC_RemapOffsets(unsigned int firststatement, unsigned int laststatement, unsigned int min, unsigned int max, unsigned int newmin)
{
	QCC_dstatement_t *st;
	unsigned int i;

	for (i = firststatement, st = &statements[i]; i < laststatement; i++, st++)
	{
		if (pr_opcodes[st->op].type_a && st->a >= min && st->a < max)
			st->a = st->a - min + newmin;
		if (pr_opcodes[st->op].type_b && st->b >= min && st->b < max)
			st->b = st->b - min + newmin;
		if (pr_opcodes[st->op].type_c && st->c >= min && st->c < max)
			st->c = st->c - min + newmin;
	}
}

void QCC_Marshal_Locals(int first, int laststatement)
{
	QCC_def_t *local;
	unsigned int newofs;

	if (!opt_overlaptemps)	//clear these after each function. we arn't overlapping them so why do we need to keep track of them?
		functemps = NULL;

	if (!pr.localvars)	//nothing to marshal
	{
		locals_start = numpr_globals;
		locals_end = numpr_globals;
		return;
	}

	if (!opt_locals_marshalling)
	{
		pr.localvars = NULL;
		return;
	}

	//initial backwards bounds.
	locals_start = MAX_REGS;
	locals_end = 0;

	newofs = MAX_REGS;	//this is a handy place to put it. :)

	//the params need to be in the order that they were allocated
	//so we allocate in a backwards order.
	for (local = pr.localvars; local; local = local->nextlocal)
	{
		if (local->constant)
			continue;

		newofs += local->type->size*local->arraysize;
		if (local->arraysize>1)
			newofs++;
	}

	locals_start = MAX_REGS;
	locals_end = newofs;

	
	optres_locals_marshalling+=newofs-MAX_REGS;

	for (local = pr.localvars; local; local = local->nextlocal)
	{
		if (local->constant)
			continue;

		newofs -= local->type->size*local->arraysize;
		if (local->arraysize>1)
			newofs--;

		QCC_RemapOffsets(first, laststatement, local->ofs, local->ofs+local->type->size*local->arraysize, newofs);
		QCC_FreeOffset(local->ofs, local->type->size*local->arraysize);

		local->ofs = newofs;
	}


	pr.localvars = NULL;
}

#ifdef WRITEASM
void QCC_WriteAsmFunction(QCC_def_t	*sc, unsigned int firststatement, gofs_t firstparm)
{
	unsigned int			i;
	unsigned int p;
	gofs_t o;
	QCC_type_t *type;
	QCC_def_t *param;

	if (!asmfile)
		return;

	type = sc->type;
	fprintf(asmfile, "%s(", TypeName(type->aux_type));
	p = type->num_parms;
	for (o = firstparm, i = 0, type = type->param; i < p; i++, type = type->next)
	{
		if (i)
			fprintf(asmfile, ", ");

		for (param = pr.localvars; param; param = param->nextlocal)
		{
			if (param->ofs == o)
				break;
		}
		if (param)
			fprintf(asmfile, "%s %s", TypeName(type), param->name);
		else
			fprintf(asmfile, "%s", TypeName(type));

		o += type->size;
	}

	fprintf(asmfile, ") %s = asm\n{\n", sc->name);

	QCC_fprintfLocals(asmfile, firstparm, o);

	for (i = firststatement; i < (unsigned int)numstatements; i++)
	{
		fprintf(asmfile, "\t%s", pr_opcodes[statements[i].op].opname);
		if (pr_opcodes[statements[i].op].type_a != &type_void)
		{
			if (strlen(pr_opcodes[statements[i].op].opname)<6)
				fprintf(asmfile, "\t");
			if (pr_opcodes[statements[i].op].type_a)
				fprintf(asmfile, "\t%s", QCC_VarAtOffset(statements[i].a, (*pr_opcodes[statements[i].op].type_a)->size));
			else
				fprintf(asmfile, "\t%i", statements[i].a);
			if (pr_opcodes[statements[i].op].type_b != &type_void)
			{
				if (pr_opcodes[statements[i].op].type_b)
					fprintf(asmfile, ",\t%s", QCC_VarAtOffset(statements[i].b, (*pr_opcodes[statements[i].op].type_b)->size));
				else
					fprintf(asmfile, ",\t%i", statements[i].b);
				if (pr_opcodes[statements[i].op].type_c != &type_void && pr_opcodes[statements[i].op].associative==ASSOC_LEFT)
				{
					if (pr_opcodes[statements[i].op].type_c)
						fprintf(asmfile, ",\t%s", QCC_VarAtOffset(statements[i].c, (*pr_opcodes[statements[i].op].type_c)->size));
					else
						fprintf(asmfile, ",\t%i", statements[i].c);
				}
			}
			else
			{
				if (pr_opcodes[statements[i].op].type_c != &type_void)
				{
					if (pr_opcodes[statements[i].op].type_c)
						fprintf(asmfile, ",\t%s", QCC_VarAtOffset(statements[i].c, (*pr_opcodes[statements[i].op].type_c)->size));
					else
						fprintf(asmfile, ",\t%i", statements[i].c);
				}
			}
		}
		fprintf(asmfile, ";\n");
	}

	fprintf(asmfile, "}\n\n");
}
#endif

/*
============
PR_ParseImmediateStatements

Parse a function body
============
*/
QCC_function_t *QCC_PR_ParseImmediateStatements (QCC_type_t *type)
{
	int			i;
	QCC_function_t	*f;
	QCC_def_t		*defs[MAX_PARMS+MAX_EXTRA_PARMS], *e2;

	QCC_type_t *parm;
	pbool needsdone=false;
	freeoffset_t *oldfofs;

	conditional = 0;

	
	f = (void *)qccHunkAlloc (sizeof(QCC_function_t));

//
// check for builtin function definition #1, #2, etc
//
// hexenC has void name() : 2;
	if (QCC_PR_Check ("#") || QCC_PR_Check (":"))
	{
		if (pr_token_type != tt_immediate
		|| pr_immediate_type != type_float
		|| pr_immediate._float != (int)pr_immediate._float)
			QCC_PR_ParseError (ERR_BADBUILTINIMMEDIATE, "Bad builtin immediate");
		f->builtin = (int)pr_immediate._float;
		QCC_PR_Lex ();

		locals_start = locals_end = OFS_PARM0; //hmm...
		return f;
	}

	if (type->num_parms < 0)
		QCC_PR_ParseError (ERR_FUNCTIONWITHVARGS, "QC function with variable arguments and function body");
	
	f->builtin = 0;
//
// define the parms
//

	locals_start = locals_end = numpr_globals;

	oldfofs = freeofs;
	freeofs = NULL;

	parm = type->param;
	for (i=0 ; i<type->num_parms ; i++)
	{
		if (!*pr_parm_names[i])
			QCC_PR_ParseError(ERR_PARAMWITHNONAME, "Parameter is not named");
		defs[i] = QCC_PR_GetDef (parm, pr_parm_names[i], pr_scope, true, 1);

		defs[i]->references++;
		if (i < MAX_PARMS)
		{
			f->parm_ofs[i] = defs[i]->ofs;
			if (i > 0 && f->parm_ofs[i] < f->parm_ofs[i-1])
				QCC_Error (ERR_BADPARAMORDER, "bad parm order");
			if (i > 0 && f->parm_ofs[i] != f->parm_ofs[i-1]+defs[i-1]->type->size)
				QCC_Error (ERR_BADPARAMORDER, "parms not packed");
		}
		parm = parm->next;
	}

	if (type->num_parms)
		locals_start = locals_end = defs[0]->ofs;

	freeofs = oldfofs;

	f->code = numstatements;

	if (type->num_parms > MAX_PARMS)
	{
		for (i = MAX_PARMS; i < type->num_parms; i++)
		{
			if (!extra_parms[i - MAX_PARMS])
			{
				e2 = (QCC_def_t *) qccHunkAlloc (sizeof(QCC_def_t));
				e2->name = "extra parm";
				e2->ofs = QCC_GetFreeOffsetSpace(3);
				extra_parms[i - MAX_PARMS] = e2;
			}
			extra_parms[i - MAX_PARMS]->type = defs[i]->type;
			if (defs[i]->type->type != ev_vector)
				QCC_PR_Statement (&pr_opcodes[OP_STORE_F], extra_parms[i - MAX_PARMS], defs[i], NULL);
			else
				QCC_PR_Statement (&pr_opcodes[OP_STORE_V], extra_parms[i - MAX_PARMS], defs[i], NULL);
		}
	}

	QCC_RemapLockedTemps(-1, -1);

	/*if (pr_classtype)
	{
		QCC_def_t *e, *e2;
		e = QCC_PR_GetDef(pr_classtype, "__oself", pr_scope, true, 1);
		e2 = QCC_PR_GetDef(type_entity, "self", NULL, true, 1);
		QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_ENT], QCC_PR_DummyDef(pr_classtype, "self", pr_scope, 1, e2->ofs, false), e, NULL));
	}*/

//
// check for a state opcode
//
	if (QCC_PR_Check ("["))
		QCC_PR_ParseState ();

	if (QCC_PR_Check ("asm"))
	{
		QCC_PR_Expect ("{");
		while (!QCC_PR_Check("}"))
			QCC_PR_ParseAsm ();
	}
	else
	{
//
// parse regular statements
//
		QCC_PR_Expect ("{");

		while (!QCC_PR_Check("}"))
		{
			QCC_PR_ParseStatement ();
			QCC_FreeTemps();
		}
	}
	QCC_FreeTemps();

	// this is cheap
//	if (type->aux_type->type)
//		if (statements[numstatements - 1].op != OP_RETURN)
//			QCC_PR_ParseWarning(WARN_MISSINGRETURN, "%s: not all control paths return a value", pr_scope->name );

	if (f->code == numstatements)
		needsdone = true;
	else if (statements[numstatements - 1].op != OP_RETURN && statements[numstatements - 1].op != OP_DONE)
		needsdone = true;

	if (num_gotos)
	{
		int j;
		for (i = 0; i < num_gotos; i++)
		{
			for (j = 0; j < num_labels; j++)
			{
				if (!strcmp(pr_gotos[i].name, pr_labels[j].name))
				{
					if (!pr_opcodes[statements[pr_gotos[i].statementno].op].type_a)
						statements[pr_gotos[i].statementno].a += pr_labels[j].statementno - pr_gotos[i].statementno;
					else if (!pr_opcodes[statements[pr_gotos[i].statementno].op].type_b)
						statements[pr_gotos[i].statementno].b += pr_labels[j].statementno - pr_gotos[i].statementno;
					else
						statements[pr_gotos[i].statementno].c += pr_labels[j].statementno - pr_gotos[i].statementno;
					break;
				}
			}
			if (j == num_labels)
			{
				num_gotos = 0;
				QCC_PR_ParseError(ERR_NOLABEL, "Goto statement with no matching label \"%s\"", pr_gotos[i].name);
			}
		}
		num_gotos = 0;
	}

	if (opt_return_only && !needsdone)
		needsdone = QCC_FuncJumpsTo(f->code, numstatements, numstatements);

	// emit an end of statements opcode
	if (!opt_return_only || needsdone)
	{
		/*if (pr_classtype)
		{
			QCC_def_t *e, *e2;
			e = QCC_PR_GetDef(NULL, "__oself", pr_scope, false, 0);
			e2 = QCC_PR_GetDef(NULL, "self", NULL, false, 0);
			QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_ENT], e, QCC_PR_DummyDef(pr_classtype, "self", pr_scope, 1, e2->ofs, false), NULL));
		}*/

		QCC_PR_Statement (pr_opcodes, 0,0, NULL);
	}
	else
		optres_return_only++;

	QCC_CheckForDeadAndMissingReturns(f->code, numstatements, type->aux_type->type);

	if (opt_compound_jumps)
		QCC_CompoundJumps(f->code, numstatements);
//	if (opt_comexprremoval)
//		QCC_CommonSubExpressionRemoval(f->code, numstatements);


	QCC_RemapLockedTemps(f->code, numstatements);
	locals_end = numpr_globals;

	QCC_WriteAsmFunction(pr_scope, f->code, locals_start);

	QCC_Marshal_Locals(f->code, numstatements);

	if (num_labels)
		num_labels = 0;


	if (num_continues)
	{
		num_continues=0;
		QCC_PR_ParseError(ERR_ILLEGALCONTINUES, "%s: function contains illegal continues\n", pr_scope->name);
	}
	if (num_breaks)
	{
		num_breaks=0;
		QCC_PR_ParseError(ERR_ILLEGALBREAKS, "%s: function contains illegal breaks\n", pr_scope->name);
	}
	if (num_cases)
	{
		num_cases = 0;
		QCC_PR_ParseError(ERR_ILLEGALCASES, "%s: function contains illegal cases\n", pr_scope->name);
	}

	return f;
}

void QCC_PR_ArrayRecurseDivideRegular(QCC_def_t *array, QCC_def_t *index, int min, int max)
{
	QCC_dstatement_t *st;
	QCC_def_t *eq;
	if (min == max || min+1 == max)
	{
		eq = QCC_PR_Statement(pr_opcodes+OP_LT, index, QCC_MakeFloatDef(min+0.5f), NULL);
		QCC_UnFreeTemp(index);
		QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_IFNOT, eq, 0, &st));
		st->b = 2;
		QCC_PR_Statement(pr_opcodes+OP_RETURN, 0, 0, &st);
		st->a = array->ofs + min*array->type->size;
	}
	else
	{
		int mid = min + (max-min)/2;

		if (max-min>4)
		{
			eq = QCC_PR_Statement(pr_opcodes+OP_LT, index, QCC_MakeFloatDef(mid+0.5f), NULL);
			QCC_UnFreeTemp(index);
			QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_IFNOT, eq, 0, &st));
		}
		else
			st = NULL;
		QCC_PR_ArrayRecurseDivideRegular(array, index, min, mid);
		if (st)
			st->b = numstatements - (st-statements);
		QCC_PR_ArrayRecurseDivideRegular(array, index, mid, max);
	}
}

//the idea here is that we return a vector, the caller then figures out the extra 3rd.
//This is useful when we have a load of indexes.
void QCC_PR_ArrayRecurseDivideUsingVectors(QCC_def_t *array, QCC_def_t *index, int min, int max)
{
	QCC_dstatement_t *st;
	QCC_def_t *eq;
	if (min == max || min+1 == max)
	{
		eq = QCC_PR_Statement(pr_opcodes+OP_LT, index, QCC_MakeFloatDef(min+0.5f), NULL);
		QCC_UnFreeTemp(index);
		QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_IFNOT, eq, 0, &st));
		st->b = 2;
		QCC_PR_Statement(pr_opcodes+OP_RETURN, 0, 0, &st);
		st->a = array->ofs + min*3;
	}
	else
	{
		int mid = min + (max-min)/2;

		if (max-min>4)
		{
			eq = QCC_PR_Statement(pr_opcodes+OP_LT, index, QCC_MakeFloatDef(mid+0.5f), NULL);
			QCC_UnFreeTemp(index);
			QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_IFNOT, eq, 0, &st));
		}
		else
			st = NULL;
		QCC_PR_ArrayRecurseDivideUsingVectors(array, index, min, mid);
		if (st)
			st->b = numstatements - (st-statements);
		QCC_PR_ArrayRecurseDivideUsingVectors(array, index, mid, max);
	}
}

//returns a vector overlapping the result needed.
QCC_def_t *QCC_PR_EmitArrayGetVector(QCC_def_t *array)
{
	QCC_dfunction_t *df;
	QCC_def_t *temp, *index, *func;

	func = QCC_PR_GetDef(type_function, qcva("ArrayGetVec*%s", array->name), NULL, true, 1);

	pr_scope = func;

	df = &functions[numfunctions];
	numfunctions++;

	df->s_file = 0;
	df->s_name = QCC_CopyString(func->name);
	df->first_statement = numstatements;
	df->parm_size[0] = 1;
	df->numparms = 1;
	df->parm_start = numpr_globals;
	index = QCC_PR_GetDef(type_float, "index___", func, true, 1);
	index->references++;
	temp = QCC_PR_GetDef(type_float, "div3___", func, true, 1);
	locals_end = numpr_globals;
	df->locals = locals_end - df->parm_start;
	QCC_PR_Statement3(pr_opcodes+OP_DIV_F, index, QCC_MakeFloatDef(3), temp);
	QCC_PR_Statement3(pr_opcodes+OP_BITAND, temp, temp, temp);//round down to int

	QCC_PR_ArrayRecurseDivideUsingVectors(array, temp, 0, (array->arraysize+2)/3);	//round up

	QCC_PR_Statement(pr_opcodes+OP_RETURN, QCC_MakeFloatDef(0), 0, NULL);	//err... we didn't find it, give up.
	QCC_PR_Statement(pr_opcodes+OP_DONE, 0, 0, NULL);	//err... we didn't find it, give up.

	G_FUNCTION(func->ofs) = df - functions;
	func->initialized = 1;
	return func;
}

void QCC_PR_EmitArrayGetFunction(QCC_def_t *scope, char *arrayname)
{
	QCC_def_t *vectortrick;
	QCC_dfunction_t *df;
	QCC_def_t *def, *index;

	QCC_dstatement_t *st;
	QCC_def_t *eq;

	def = QCC_PR_GetDef(NULL, arrayname, NULL, false, 0);

	if (def->arraysize >= 15 && def->type->size == 1)
	{
		vectortrick = QCC_PR_EmitArrayGetVector(def);
	}
	else
		vectortrick = NULL;

	pr_scope = scope;

	df = &functions[numfunctions];
	numfunctions++;

	df->s_file = 0;
	df->s_name = QCC_CopyString(scope->name);
	df->first_statement = numstatements;
	df->parm_size[0] = 1;
	df->numparms = 1;
	df->parm_start = numpr_globals;
	index = QCC_PR_GetDef(type_float, "indexg___", def, true, 1);

	G_FUNCTION(scope->ofs) = df - functions;

	if (vectortrick)
	{
		QCC_def_t *div3, *intdiv3, *ret;

		//okay, we've got a function to retrieve the var as part of a vector.
		//we need to work out which part, x/y/z that it's stored in.
		//0,1,2 = i - ((int)i/3 *) 3;

		div3 = QCC_PR_GetDef(type_float, "div3___", def, true, 1);
		intdiv3 = QCC_PR_GetDef(type_float, "intdiv3___", def, true, 1);

		eq = QCC_PR_Statement(pr_opcodes+OP_GE, index, QCC_MakeFloatDef((float)def->arraysize), NULL);	//escape clause - should call some sort of error function instead.. that'd rule!
		QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_IFNOT, eq, 0, &st));
		st->b = 2;
		QCC_PR_Statement(pr_opcodes+OP_RETURN, QCC_MakeFloatDef(0), 0, &st);

		div3->references++;
		QCC_PR_Statement3(pr_opcodes+OP_BITAND, index, index, index);
		QCC_PR_Statement3(pr_opcodes+OP_DIV_F, index, QCC_MakeFloatDef(3), div3);
		QCC_PR_Statement3(pr_opcodes+OP_BITAND, div3, div3, intdiv3);

		QCC_PR_Statement3(pr_opcodes+OP_STORE_F, index, &def_parms[0], NULL);
		QCC_PR_Statement3(pr_opcodes+OP_CALL1, vectortrick, NULL, NULL);
		vectortrick->references++;
		ret = QCC_PR_GetDef(type_vector, "vec__", pr_scope, true, 1);
		ret->references+=4;
		QCC_PR_Statement3(pr_opcodes+OP_STORE_V, &def_ret, ret, NULL);

		div3 = QCC_PR_Statement(pr_opcodes+OP_MUL_F, intdiv3, QCC_MakeFloatDef(3), NULL);
		QCC_PR_Statement3(pr_opcodes+OP_SUB_F, index, div3, index);
		QCC_FreeTemp(div3);

		eq = QCC_PR_Statement(pr_opcodes+OP_LT, index, QCC_MakeFloatDef(0+0.5f), NULL);
		QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_IFNOT, eq, 0, &st));
		st->b = 2;
		QCC_PR_Statement(pr_opcodes+OP_RETURN, 0, 0, &st);
		st->a = ret->ofs + 0;

		eq = QCC_PR_Statement(pr_opcodes+OP_LT, index, QCC_MakeFloatDef(1+0.5f), NULL);
		QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_IFNOT, eq, 0, &st));
		st->b = 2;
		QCC_PR_Statement(pr_opcodes+OP_RETURN, 0, 0, &st);
		st->a = ret->ofs + 1;

		eq = QCC_PR_Statement(pr_opcodes+OP_LT, index, QCC_MakeFloatDef(2+0.5), NULL);
		QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_IFNOT, eq, 0, &st));
		st->b = 2;
		QCC_PR_Statement(pr_opcodes+OP_RETURN, 0, 0, &st);
		st->a = ret->ofs + 2;
		QCC_FreeTemp(ret);
		QCC_FreeTemp(index);
	}
	else
	{
		QCC_PR_Statement3(pr_opcodes+OP_BITAND, index, index, index);
		QCC_PR_ArrayRecurseDivideRegular(def, index, 0, def->arraysize);
	}

	QCC_PR_Statement(pr_opcodes+OP_RETURN, QCC_MakeFloatDef(0), 0, NULL);

	QCC_PR_Statement(pr_opcodes+OP_DONE, 0, 0, NULL);

	locals_end = numpr_globals;
	df->locals = locals_end - df->parm_start;


	QCC_WriteAsmFunction(pr_scope, df->first_statement, df->parm_start);

	QCC_FreeTemps();
}

void QCC_PR_ArraySetRecurseDivide(QCC_def_t *array, QCC_def_t *index, QCC_def_t *value, int min, int max)
{
	QCC_dstatement_t *st;
	QCC_def_t *eq;
	if (min == max || min+1 == max)
	{
		eq = QCC_PR_Statement(pr_opcodes+OP_EQ_F, index, QCC_MakeFloatDef((float)min), NULL);
		QCC_UnFreeTemp(index);
		QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_IFNOT, eq, 0, &st));
		st->b = 3;
		if (array->type->size == 3)
			QCC_PR_Statement(pr_opcodes+OP_STORE_V, value, array, &st);
		else
			QCC_PR_Statement(pr_opcodes+OP_STORE_F, value, array, &st);
		st->b = array->ofs + min*array->type->size;
		QCC_PR_Statement(pr_opcodes+OP_RETURN, 0, 0, &st);
	}
	else
	{
		int mid = min + (max-min)/2;

		if (max-min>4)
		{
			eq = QCC_PR_Statement(pr_opcodes+OP_LT, index, QCC_MakeFloatDef((float)mid), NULL);
			QCC_UnFreeTemp(index);
			QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_IFNOT, eq, 0, &st));
		}
		else
			st = NULL;
		QCC_PR_ArraySetRecurseDivide(array, index, value, min, mid);
		if (st)
			st->b = numstatements - (st-statements);
		QCC_PR_ArraySetRecurseDivide(array, index, value, mid, max);
	}
}

void QCC_PR_EmitArraySetFunction(QCC_def_t *scope, char *arrayname)
{
	QCC_dfunction_t *df;
	QCC_def_t *def, *index, *value;

	def = QCC_PR_GetDef(NULL, arrayname, NULL, false, 0);
	pr_scope = scope;

	df = &functions[numfunctions];
	numfunctions++;

	df->s_file = 0;
	df->s_name = QCC_CopyString(scope->name);
	df->first_statement = numstatements;
	df->parm_size[0] = 1;
	df->parm_size[1] = def->type->size;
	df->numparms = 2;
	df->parm_start = numpr_globals;
	index = QCC_PR_GetDef(type_float, "indexs___", def, true, 1);
	value = QCC_PR_GetDef(def->type, "value___", def, true, 1);
	locals_end = numpr_globals;
	df->locals = locals_end - df->parm_start;

	G_FUNCTION(scope->ofs) = df - functions;

	QCC_PR_Statement3(pr_opcodes+OP_BITAND, index, index, index);
	QCC_PR_ArraySetRecurseDivide(def, index, value, 0, def->arraysize);

	QCC_PR_Statement(pr_opcodes+OP_DONE, 0, 0, NULL);



	QCC_WriteAsmFunction(pr_scope, df->first_statement, df->parm_start);

	QCC_FreeTemps();
}

//register a def, and all of it's sub parts.
//only the main def is of use to the compiler.
//the subparts are emitted to the compiler and allow correct saving/loading
//be careful with fields, this doesn't allocated space, so will it allocate fields. It only creates defs at specified offsets.
QCC_def_t *QCC_PR_DummyDef(QCC_type_t *type, char *name, QCC_def_t *scope, int arraysize, unsigned int ofs, int referable)
{
	char array[64];
	char newname[256];
	int a, i;
	QCC_def_t *def, *first=NULL;

#define KEYWORD(x) if (!STRCMP(name, #x) && keyword_##x) {if (keyword_##x)QCC_PR_ParseWarning(WARN_KEYWORDDISABLED, "\""#x"\" keyword used as variable name%s", keywords_coexist?" - coexisting":" - disabling");keyword_##x=keywords_coexist;}
	if (name)
	{
		KEYWORD(var);
		KEYWORD(thinktime);
		KEYWORD(for);
		KEYWORD(switch);
		KEYWORD(case);
		KEYWORD(default);
		KEYWORD(do);
		KEYWORD(goto);
		if (type->type != ev_function)
			KEYWORD(break);
		KEYWORD(continue);
		KEYWORD(state);
		KEYWORD(string);
		if (qcc_targetformat != QCF_HEXEN2)
			KEYWORD(float);	//hmm... hexen2 requires this...
		KEYWORD(entity);
		KEYWORD(vector);
		KEYWORD(const);
		KEYWORD(asm);
	}

	for (a = 0; a < arraysize; a++)
	{
		if (a == 0)
			*array = '\0';
		else
			sprintf(array, "[%i]", a);

		if (name)
			sprintf(newname, "%s%s", name, array);
		else
			*newname = *"";

		// allocate a new def
		def = (void *)qccHunkAlloc (sizeof(QCC_def_t));
		memset (def, 0, sizeof(*def));
		def->next = NULL;
		def->arraysize = arraysize;
		if (name)
		{
			pr.def_tail->next = def;
			pr.def_tail = def;
		}

		if (a > 0)
			def->references++;

		def->s_line = pr_source_line;
		def->s_file = s_file;

		def->name = (void *)qccHunkAlloc (strlen(newname)+1);
		strcpy (def->name, newname);
		def->type = type;

		def->scope = scope;	

	//	if (arraysize>1)
			def->constant = true;

		if (ofs + type->size*a >= MAX_REGS)
			QCC_Error(ERR_TOOMANYGLOBALS, "MAX_REGS is too small");
		def->ofs = ofs + type->size*a;
		if (!first)
			first = def;

//	printf("Emited %s\n", newname);

		if (type->type == ev_struct)
		{
			int partnum;
			QCC_type_t *parttype;
			parttype = type->param;				
			for (partnum = 0; partnum < type->num_parms; partnum++)
			{
				switch (parttype->type)
				{
				case ev_vector:
					sprintf(newname, "%s%s.%s", name, array, parttype->name);
					QCC_PR_DummyDef(parttype, newname, scope, 1, ofs + type->size*a + parttype->ofs, false);

					sprintf(newname, "%s%s.%s_x", name, array, parttype->name);
					QCC_PR_DummyDef(type_float, newname, scope, 1, ofs + type->size*a + parttype->ofs, false);
					sprintf(newname, "%s%s.%s_y", name, array, parttype->name);
					QCC_PR_DummyDef(type_float, newname, scope, 1, ofs + type->size*a + parttype->ofs+1, false);
					sprintf(newname, "%s%s.%s_z", name, array, parttype->name);
					QCC_PR_DummyDef(type_float, newname, scope, 1, ofs + type->size*a + parttype->ofs+2, false);
					break;

				case ev_float:
				case ev_string:
				case ev_entity:
				case ev_field:				
				case ev_pointer:
				case ev_integer:
				case ev_struct:
				case ev_union:
					sprintf(newname, "%s%s.%s", name, array, parttype->name);
					QCC_PR_DummyDef(parttype, newname, scope, 1, ofs + type->size*a + parttype->ofs, false);
					break;

				case ev_function:
					sprintf(newname, "%s%s.%s", name, array, parttype->name);
					QCC_PR_DummyDef(parttype, newname, scope, 1, ofs + type->size*a +parttype->ofs, false)->initialized = true;
					for (i = parttype->num_parms; i>0; i--)
						parttype=parttype->next;
					break;
				case ev_void:
					break;
				}
				parttype=parttype->next;
			}			
		}
		else if (type->type == ev_vector)
		{	//do the vector thing.
			sprintf(newname, "%s%s_x", name, array);
			QCC_PR_DummyDef(type_float, newname, scope, 1, ofs + type->size*a+0, referable);
			sprintf(newname, "%s%s_y", name, array);
			QCC_PR_DummyDef(type_float, newname, scope, 1, ofs + type->size*a+1, referable);
			sprintf(newname, "%s%s_z", name, array);
			QCC_PR_DummyDef(type_float, newname, scope, 1, ofs + type->size*a+2, referable);
		}
		else if (type->type == ev_field)
		{
			if (type->aux_type->type == ev_vector)
			{
				//do the vector thing.
				sprintf(newname, "%s%s_x", name, array);
				QCC_PR_DummyDef(type_floatfield, newname, scope, 1, ofs + type->size*a+0, referable);
				sprintf(newname, "%s%s_y", name, array);
				QCC_PR_DummyDef(type_floatfield, newname, scope, 1, ofs + type->size*a+1, referable);
				sprintf(newname, "%s%s_z", name, array);
				QCC_PR_DummyDef(type_floatfield, newname, scope, 1, ofs + type->size*a+2, referable);
			}
		}
	}

	if (referable)
	{
		if (!Hash_Get(&globalstable, "end_sys_fields"))
			first->references++;	//anything above needs to be left in, and so warning about not using it is just going to pee people off.
		if (arraysize <= 1)
			first->constant = false;
		if (scope)
			Hash_Add(&localstable, first->name, first);
		else
			Hash_Add(&globalstable, first->name, first);

		if (!scope && asmfile)
			fprintf(asmfile, "%s %s;\n", TypeName(first->type), first->name);
	}

	return first;
}

/*
============
PR_GetDef

If type is NULL, it will match any type
If allocate is true, a new def will be allocated if it can't be found
============
*/

QCC_def_t *QCC_PR_GetDef (QCC_type_t *type, char *name, QCC_def_t *scope, pbool allocate, int arraysize)
{
	int ofs;
	QCC_def_t		*def;
//	char element[MAX_NAME];
	unsigned int i;

	if (scope)
	{
		def = Hash_Get(&localstable, name);

		while(def)
		{
			if ( def->scope && def->scope != scope)
			{
				def = Hash_GetNext(&localstable, name, def);
				continue;		// in a different function
			}

			if (type && typecmp(def->type, type))
				QCC_PR_ParseError (ERR_TYPEMISMATCHREDEC, "Type mismatch on redeclaration of %s. %s, should be %s",name, TypeName(type), TypeName(def->type));
			if (def->arraysize != arraysize && arraysize)
				QCC_PR_ParseError (ERR_TYPEMISMATCHARRAYSIZE, "Array sizes for redecleration of %s do not match",name);
			if (allocate && scope)
			{
				QCC_PR_ParseWarning (WARN_DUPLICATEDEFINITION, "%s duplicate definition ignored", name);
				QCC_PR_ParsePrintDef(WARN_DUPLICATEDEFINITION, def);
//				if (!scope)
//					QCC_PR_ParsePrintDef(def);
			}
			return def;
		}
	}


	def = Hash_Get(&globalstable, name);

	while(def)
	{
		if ( def->scope && def->scope != scope)
		{
			def = Hash_GetNext(&globalstable, name, def);
			continue;		// in a different function
		}

		if (type && typecmp(def->type, type))
		{
			if (!pr_scope)
				QCC_PR_ParseError (ERR_TYPEMISMATCHREDEC, "Type mismatch on redeclaration of %s. %s, should be %s",name, TypeName(type), TypeName(def->type));
		}
		if (def->arraysize != arraysize && arraysize)
			QCC_PR_ParseError (ERR_TYPEMISMATCHARRAYSIZE, "Array sizes for redecleration of %s do not match",name);
		if (allocate && scope)
		{
			if (pr_scope)
			{	//warn? or would that be pointless?
				def = Hash_GetNext(&globalstable, name, def);
				continue;		// in a different function
			}

			QCC_PR_ParseWarning (WARN_DUPLICATEDEFINITION, "%s duplicate definition ignored", name);
			QCC_PR_ParsePrintDef(WARN_DUPLICATEDEFINITION, def);
//			if (!scope)
//				QCC_PR_ParsePrintDef(def);
		}
		return def;
	}

	if (!allocate)
		return NULL;
	if (arraysize < 1)
	{
		QCC_PR_ParseError (ERR_ARRAYNEEDSSIZE, "First declaration of array %s with no size",name);
	}

	if (scope)
	{
		if (QCC_PR_GetDef(type, name, NULL, false, arraysize))
			QCC_PR_ParseWarning(0, "Local \"%s\" defined with name of a global", name);
	}

	ofs = numpr_globals;
	if (arraysize > 1)
	{	//write the array size
		ofs = QCC_GetFreeOffsetSpace(1 + (type->size	* arraysize));

		((int *)qcc_pr_globals)[ofs] = arraysize-1;	//An array needs the size written first. This is a hexen2 opcode thing.
		ofs++;
	}
	else
		ofs = QCC_GetFreeOffsetSpace(type->size	* arraysize);

	def = QCC_PR_DummyDef(type, name, scope, arraysize, ofs, true);

	//fix up fields.
	if (type->type == ev_field && allocate != 2)
	{
		for (i = 0; i < type->size*arraysize; i++)	//make arrays of fields work.
			*(int *)&qcc_pr_globals[def->ofs+i] = pr.size_fields+i;

		pr.size_fields += i;
	}

	if (scope)
	{
		def->nextlocal = pr.localvars;
		pr.localvars = def;
	}

	return def;
}

QCC_def_t *QCC_PR_DummyFieldDef(QCC_type_t *type, char *name, QCC_def_t *scope, int arraysize, unsigned int *fieldofs)
{
	char array[64];
	char newname[256];
	int a, parms;
	QCC_def_t *def, *first=NULL;
	unsigned int maxfield, startfield;
	QCC_type_t *ftype;
	pbool isunion;
	startfield = *fieldofs;
	maxfield = startfield;

	for (a = 0; a < arraysize; a++)
	{
		if (a == 0)
			*array = '\0';
		else
			sprintf(array, "[%i]", a);

		if (*name)
		{
			sprintf(newname, "%s%s", name, array);

			// allocate a new def
			def = (void *)qccHunkAlloc (sizeof(QCC_def_t));
			memset (def, 0, sizeof(*def));
			def->next = NULL;
			def->arraysize = arraysize;

			pr.def_tail->next = def;
			pr.def_tail = def;

			def->s_line = pr_source_line;
			def->s_file = s_file;

			def->name = (void *)qccHunkAlloc (strlen(newname)+1);
			strcpy (def->name, newname);
			def->type = type;

			def->scope = scope;	

			def->ofs = QCC_GetFreeOffsetSpace(1);
			((int *)qcc_pr_globals)[def->ofs] = *fieldofs;
			*fieldofs++;
			if (!first)
				first = def;
		}
		else
		{
			def=NULL;
		}

//	printf("Emited %s\n", newname);

		if ((type)->type == ev_struct||(type)->type == ev_union)
		{
			int partnum;
			QCC_type_t *parttype;
			if (def)
				def->references++;
			parttype = (type)->param;
			isunion = ((type)->type == ev_union);
			for (partnum = 0, parms = (type)->num_parms; partnum < parms; partnum++)
			{
				switch (parttype->type)
				{
				case ev_union:
				case ev_struct:
					if (*name)
						sprintf(newname, "%s%s.%s", name, array, parttype->name);
					else
						sprintf(newname, "%s%s", parttype->name, array);
					def = QCC_PR_DummyFieldDef(parttype, newname, scope, 1, fieldofs);
					break;
				case ev_float:
				case ev_string:
				case ev_vector:
				case ev_entity:
				case ev_field:
				case ev_pointer:
				case ev_integer:
					if (*name)
						sprintf(newname, "%s%s.%s", name, array, parttype->name);
					else
						sprintf(newname, "%s%s", parttype->name, array);
					ftype = QCC_PR_NewType("FIELD TYPE", ev_field);
					ftype->aux_type = parttype;
					if (parttype->type == ev_vector)
						ftype->size = parttype->size;	//vector fields create a _y and _z too, so we need this still.
					def = QCC_PR_GetDef(NULL, newname, scope, false, 1);
					if (!def)
					{
						def = QCC_PR_GetDef(ftype, newname, scope, true, 1);
					}
					else
					{
						QCC_PR_ParseWarning(WARN_CONFLICTINGUNIONMEMBER, "conflicting offsets for union/struct expansion of %s. Ignoring new def.", newname);
						QCC_PR_ParsePrintDef(WARN_CONFLICTINGUNIONMEMBER, def);
					}
					break;

				case ev_function:
					if (*name)
						sprintf(newname, "%s%s.%s", name, array, parttype->name);
					else
						sprintf(newname, "%s%s", parttype->name, array);
					ftype = QCC_PR_NewType("FIELD TYPE", ev_field);
					ftype->aux_type = parttype;
					def = QCC_PR_GetDef(ftype, newname, scope, true, 1);
					def->initialized = true;
					((int *)qcc_pr_globals)[def->ofs] = *fieldofs;
					*fieldofs += parttype->size;
					break;
				case ev_void:
					break;
				}
				if (*fieldofs > maxfield)
					maxfield = *fieldofs;
				if (isunion)
					*fieldofs = startfield;

				type = parttype;
				parttype=parttype->next;
			}			
		}
	}

	*fieldofs = maxfield;	//final size of the union.
	return first;
}



void QCC_PR_ExpandUnionToFields(QCC_type_t *type, int *fields)
{
	QCC_type_t *pass = type->aux_type;
	QCC_PR_DummyFieldDef(pass, "", pr_scope, 1, fields);
}

/*
================
PR_ParseDefs

Called at the outer layer and when a local statement is hit
================
*/
void QCC_PR_ParseDefs (char *classname)
{
	char		*name;
	QCC_type_t		*type, *parm;
	QCC_def_t		*def, *d;
	QCC_function_t	*f;
	QCC_dfunction_t	*df;
	int			i;
	pbool shared=false;
	pbool externfnc=false;
	pbool constant = true;
	pbool noref = false;
	pbool nosave = false;
	pbool allocatenew = true;
	int ispointer;
	gofs_t oldglobals;
	int arraysize;

	if (QCC_PR_Check("enum"))
	{
		float v = 0;
		QCC_PR_Expect("{");
		i = 0;
		d = NULL;
		while(1)
		{
			name = QCC_PR_ParseName();
			if (QCC_PR_Check("="))
			{
				if (pr_token_type != tt_immediate && pr_immediate_type->type != ev_float)
				{
					def = QCC_PR_GetDef(NULL, QCC_PR_ParseName(), NULL, false, 0);
					if (def)
					{
						if (!def->constant)
							QCC_PR_ParseError(ERR_NOTANUMBER, "enum - %s is not a constant", def->name);
						else
							v = G_FLOAT(def->ofs);
					}
					else
						QCC_PR_ParseError(ERR_NOTANUMBER, "enum - not a number");
				}
				else
				{
					v = pr_immediate._float;
					QCC_PR_Lex();
				}
			}
			def = QCC_PR_GetDef(type_float, name, pr_scope, true, 1);
			def->constant = true;
			G_FLOAT(def->ofs) = v;
			v++;

			if (QCC_PR_Check("}"))
				break;
			QCC_PR_Expect(",");
		}
		QCC_PR_Expect(";");
		return;
	}

	if (QCC_PR_Check("enumflags"))
	{
		float v = 1;
		int bits;
		QCC_PR_Expect("{");
		i = 0;
		d = NULL;
		while(1)
		{
			name = QCC_PR_ParseName();
			if (QCC_PR_Check("="))
			{
				if (pr_token_type != tt_immediate && pr_immediate_type->type != ev_float)
				{
					def = QCC_PR_GetDef(NULL, QCC_PR_ParseName(), NULL, false, 0);
					if (def)
					{
						if (!def->constant)
							QCC_PR_ParseError(ERR_NOTANUMBER, "enumflags - %s is not a constant", def->name);
						else
							v = G_FLOAT(def->ofs);
					}
					else
						QCC_PR_ParseError(ERR_NOTANUMBER, "enumflags - not a number");
				}
				else
				{
					v = pr_immediate._float;
					QCC_PR_Lex();
				}

				bits = 0;
				i = (int)v;
				if (i != v)
					QCC_PR_ParseWarning(WARN_ENUMFLAGS_NOTINTEGER, "enumflags - %f not an integer", v);
				else
				{
					while(i)
					{
						if (((i>>1)<<1) != i)
							bits++;
						i>>=1;
					}
					if (bits != 1)
						QCC_PR_ParseWarning(WARN_ENUMFLAGS_NOTBINARY, "enumflags - value %i not a single bit", (int)v);
				}
			}
			def = QCC_PR_GetDef(type_float, name, pr_scope, true, 1);
			def->constant = true;
			G_FLOAT(def->ofs) = v;
			v*=2;

			if (QCC_PR_Check("}"))
				break;
			QCC_PR_Expect(",");
		}
		QCC_PR_Expect(";");
		return;
	}

	if (QCC_PR_Check ("typedef"))
	{
		type = QCC_PR_ParseType(true);
		if (!type)
		{
			QCC_PR_ParseError(ERR_NOTANAME, "typedef found unexpected tokens");
		}
		type->name = QCC_CopyString(pr_token)+strings;
		QCC_PR_Lex();
		QCC_PR_Expect(";");
		return;
	}

	while(1)
	{
		if (QCC_PR_Check("extern"))
			externfnc=true;
		else if (QCC_PR_Check("shared"))
		{
			shared=true;
			if (pr_scope)
				QCC_PR_ParseError (ERR_NOSHAREDLOCALS, "Cannot have shared locals");
		}
		else if (QCC_PR_Check("const"))
			constant = true;
		else if (QCC_PR_Check("var"))
			constant = false;
		else if (QCC_PR_Check("noref"))
			noref=true;
		else if (QCC_PR_Check("nosave"))
			nosave = true;
		else
			break;
	}

	type = QCC_PR_ParseType (false);
	if (type == NULL)	//ignore
		return;

	if (externfnc && type->type != ev_function)
	{
		printf ("Only functions may be defined as external (yet)\n");
		externfnc=false;
	}


//	if (pr_scope && (type->type == ev_field) )
//		QCC_PR_ParseError ("Fields must be global");

	do
	{
		if (QCC_PR_Check ("*"))
		{
			ispointer = 1;
			while(QCC_PR_Check ("*"))
				ispointer++;
			name = QCC_PR_ParseName ();
		}
		else if (QCC_PR_Check (";"))
		{
			if (type->type == ev_field && (type->aux_type->type == ev_union || type->aux_type->type == ev_struct))
			{
				QCC_PR_ExpandUnionToFields(type, &pr.size_fields);
				return;
			}
//			if (type->type == ev_union)
//			{
//				return;
//			}
			QCC_PR_ParseError (ERR_TYPEWITHNONAME, "type with no name");
			name = NULL;
			ispointer = false;
		}
		else
		{
			name = QCC_PR_ParseName ();
			ispointer = false;
		}

		if (QCC_PR_Check("::") && !classname)
		{
			classname = name;
			name = QCC_PR_ParseName();
		}

//check for an array
		
		if ( QCC_PR_Check ("[") )
		{
			if (pr_immediate_type->type == ev_integer)
				arraysize = pr_immediate._int;
			else if (pr_immediate_type->type == ev_float && (float)(int)pr_immediate._float == pr_immediate._float)
				arraysize = (int)pr_immediate._float;
			else
			{
				QCC_PR_ParseError (ERR_BADARRAYSIZE, "Definition of array (%s) size is not of a numerical value", name);
				arraysize=0;	//grrr...
			}
			QCC_PR_Lex();
			QCC_PR_Expect("]");
		}
		else
			arraysize = 1;

		if (QCC_PR_Check("("))
			type = QCC_PR_ParseFunctionType(false, type);

		if (classname)
		{
			char *membername = name;
			name = qccHunkAlloc(strlen(classname) + strlen(name) + 3);
			sprintf(name, "%s::"MEMBERFIELDNAME, classname, membername);
			if (!QCC_PR_GetDef(NULL, name, NULL, false, 0))
				QCC_PR_ParseError(ERR_NOTANAME, "%s %s is not a member of class %s\n", TypeName(type), membername, classname);
			sprintf(name, "%s::%s", classname, membername);

			pr_classtype = QCC_TypeForName(classname);
			if (!pr_classtype || !pr_classtype->parentclass)
				QCC_PR_ParseError(ERR_NOTANAME, "%s is not a class\n", classname);
		}
		else
			pr_classtype = NULL;

		oldglobals = numpr_globals;

		if (ispointer)
		{
			parm = type;
			while(ispointer)
			{
				ispointer--;
				parm = QCC_PointerTypeTo(parm);
			}

			def = QCC_PR_GetDef (parm, name, pr_scope, allocatenew, arraysize);
		}
		else
			def = QCC_PR_GetDef (type, name, pr_scope, allocatenew, arraysize);

		if (!def)
			QCC_PR_ParseError(ERR_NOTANAME, "%s is not part of class %s", name, classname);

		if (noref)
			def->references++;
		if (nosave)
			def->saved = false;
		else def->saved = true;

		if (!def->initialized && shared)	//shared count as initiialised
		{	
			def->shared = shared;
			def->initialized = true;
		}
		if (externfnc)
			def->initialized = 2;

// check for an initialization
		if (type->type == ev_function && (pr_scope || !constant))
		{
			if ( QCC_PR_Check ("=") )
			{
				if (def->arraysize>1)
					goto lazyfunctiondeclaration;
				QCC_PR_ParseError (ERR_INITIALISEDLOCALFUNCTION, "local functions may only be used as pointers");
			}

			arraysize = def->arraysize;
			d = def;	//apply to ALL elements
			while(arraysize--)
			{
				d->initialized = 1;	//fake function
				G_FUNCTION(d->ofs) = 0;
				d = d->next;
			}

			continue;
		}

		if ( QCC_PR_Check ("=") || ((type->type == ev_function) && (pr_token[0] == '{' || pr_token[0] == '[' || pr_token[0] == ':')))	//this is an initialisation (or a function)
		{
			if (def->shared)
				QCC_PR_ParseError (ERR_SHAREDINITIALISED, "shared values may not be assigned an initial value", name);
			if (def->initialized == 1)
			{
				if (def->type->type == ev_function)
				{
					i = G_FUNCTION(def->ofs);
					df = &functions[i];
					QCC_PR_ParseErrorPrintDef (ERR_REDECLARATION, def, "%s redeclared, prev instance is in %s", name, strings+df->s_file);
				}
				else
					QCC_PR_ParseErrorPrintDef(ERR_REDECLARATION, def, "%s redeclared", name);
			}

			if (autoprototype)
			{	//ignore the code and stuff
				if (QCC_PR_Check("["))
				{
					while (!QCC_PR_Check("]"))
					{
						if (pr_token_type == tt_eof)
							break;
						QCC_PR_Lex();
					}
				}
				if (QCC_PR_Check("{"))
				{
					int blev = 1;
					//balance out the { and }
					while(blev)
					{
						if (pr_token_type == tt_eof)
							break;
						if (QCC_PR_Check("{"))
							blev++;
						else if (QCC_PR_Check("}"))
							blev--;
						else
							QCC_PR_Lex();	//ignore it.
					}
				}
				else
				{
					QCC_PR_Check("#");
					QCC_PR_Lex();
				}
				continue;
			}

			if (pr_token_type == tt_name)
			{
				unsigned int i;

				if (def->arraysize>1)
					QCC_PR_ParseError(ERR_ARRAYNEEDSBRACES, "Array initialisation requires curly braces");

				d = QCC_PR_GetDef(NULL, pr_token, pr_scope, false, 0);
				if (!d)
					QCC_PR_ParseError(ERR_NOTDEFINED, "%s was not defined\n", name);
				if (typecmp(def->type, d->type))
					QCC_PR_ParseError (ERR_BADIMMEDIATETYPE, "wrong immediate type for %s", name);


				for (i = 0; i < d->type->size; i++)
					G_INT(def->ofs) = G_INT(d->ofs);
				QCC_PR_Lex();
				continue;
			}
	
			else if (type->type == ev_function)
			{
lazyfunctiondeclaration:
				def->constant = constant;
				if (QCC_PR_Check("0"))
				{
					def->constant = 0;
					def->initialized = 1;	//fake function
					G_FUNCTION(def->ofs) = 0;
					continue;
				}

				if (arraysize>1)
				{
					int i;					
					def->initialized = 1;	//fake function
					QCC_PR_Expect ("{");
					i = 0;
					do
					{
						name = QCC_PR_ParseName ();

						d = QCC_PR_GetDef (NULL, name, pr_scope, false, 0);
						if (!d)
							QCC_PR_ParseError(ERR_NOTDEFINED, "%s was not defined", name);
						else
						{
							if (!d->initialized)
								QCC_PR_ParseWarning(WARN_NOTDEFINED, "initialisation of function arrays must be placed after the body of all functions used (%s)", name);
							G_FUNCTION(def->ofs+i) = G_FUNCTION(d->ofs);
						}

						i++;
					} while(QCC_PR_Check(","));

					arraysize = def->arraysize;
					d = def;	//apply to ALL elements
					while(arraysize--)
					{
						d->initialized = 1;	//fake function
						d = d->next;
					}

					QCC_PR_Expect("}");
					if (i > def->arraysize)
						QCC_PR_ParseError(ERR_TOOMANYINITIALISERS, "Too many initializers");
					continue;
				}

				def->references++;
				pr_scope = def;
				f = QCC_PR_ParseImmediateStatements (type);
				pr_scope = NULL;
				def->initialized = 1;
				G_FUNCTION(def->ofs) = numfunctions;
				f->def = def;
//				if (pr_dumpasm)
//					PR_PrintFunction (def);

		// fill in the dfunction
				df = &functions[numfunctions];
				numfunctions++;
				if (f->builtin)
					df->first_statement = -f->builtin;
				else
					df->first_statement = f->code;

				if (f->builtin && opt_function_names)
					optres_function_names += strlen(f->def->name);
				else
					df->s_name = QCC_CopyString (f->def->name);
				df->s_file = s_file2;
				df->numparms =  f->def->type->num_parms;
				df->locals = locals_end - locals_start;
				df->parm_start = locals_start;
				for (i=0,parm = type->param ; i<df->numparms ; i++, parm = parm->next)
				{
					df->parm_size[i] = parm->size;
				}
				
				continue;
			}

			else if (type->type == ev_struct)
			{
				int arraypart, partnum;
				QCC_type_t *parttype;
				def->initialized = 1;
				def->constant = constant;
//				if (constant)
//					QCC_PR_ParseError("const used on a struct isn't useful");
				
				//FIXME: should do this recursivly
				QCC_PR_Expect("{");
				for (arraypart = 0; arraypart < arraysize; arraypart++)
				{
					parttype = type->param;
					QCC_PR_Expect("{");
					for (partnum = 0; partnum < type->num_parms; partnum++)
					{
						switch (parttype->type)
						{
						case ev_float:
						case ev_integer:
						case ev_vector:
							if (pr_token_type == tt_punct)
							{
								if (QCC_PR_Check("{"))
								{
									QCC_PR_Expect("}");
								}
								else
									QCC_PR_ParseError(ERR_UNEXPECTEDPUNCTUATION, "Unexpected punctuation");

							}
							else if (pr_token_type == tt_immediate)
							{
								if (pr_immediate_type->type == ev_float && parttype->type == ev_integer)
									G_INT(def->ofs + arraypart*type->size + parttype->ofs) = (int)pr_immediate._float;
								else if (pr_immediate_type->type != parttype->type)
									QCC_PR_ParseError (ERR_BADIMMEDIATETYPE, "wrong immediate subtype for %s.%s", def->name, parttype->name);
								else
									memcpy (qcc_pr_globals + def->ofs + arraypart*type->size + parttype->ofs, &pr_immediate, 4*type_size[pr_immediate_type->type]);
							}
							else if (pr_token_type == tt_name)
							{
								d = QCC_PR_GetDef(NULL, pr_token, pr_scope, false, 0);
								if (!d)
									QCC_PR_ParseError(ERR_NOTDEFINED, "%s was not defined\n", pr_token);
								else if (d->type->type != parttype->type)
									QCC_PR_ParseError (ERR_WRONGSUBTYPE, "wrong subtype for %s.%s", def->name, parttype->name);
								else if (!d->constant)
									QCC_PR_ParseError(ERR_NOTACONSTANT, "%s isn't a constant\n", pr_token);

								memcpy (qcc_pr_globals + def->ofs + arraypart*type->size + parttype->ofs, qcc_pr_globals + d->ofs, 4*d->type->size);
							}
							else
								QCC_PR_ParseError (ERR_BADIMMEDIATETYPE, "wrong immediate subtype for %s.%s", def->name, parttype->name);
							QCC_PR_Lex ();

							break;
						case ev_string:
							if (pr_token_type == tt_punct)
							{
								if (QCC_PR_Check("{"))
								{
									unsigned int i;
									for (i = 0; i < parttype->size; i++)
									{
/*										//the executor defines strings as true c strings, but reads in index from string table.
										//structures can hide these strings.
										d = (void *)qccHunkAlloc (sizeof(QCC_def_t));
										d->next = NULL;
										pr.def_tail->next = d;
										pr.def_tail = d;

										d->type = parttype;
										d->name = "STRUCTIMMEDIATE";
										d->constant = constant;
										d->initialized = 1;
										d->scope = NULL;

										d->ofs = def->ofs+arraypart*type->size+parttype->ofs+i;
*/
										G_INT(def->ofs+arraypart*type->size+parttype->ofs+i) = QCC_CopyString(pr_immediate_string);
										QCC_PR_Lex ();

										if (!QCC_PR_Check(","))
										{
											i++;
											break;
										}
									}
									for (; i < parttype->size; i++)
									{
/*										//the executor defines strings as true c strings, but reads in index from string table.
										//structures can hide these strings.
										d = (void *)qccHunkAlloc (sizeof(QCC_def_t));
										d->next = NULL;
										pr.def_tail->next = d;
										pr.def_tail = d;

										d->type = parttype;
										d->name = "STRUCTIMMEDIATE";
										d->constant = constant;
										d->initialized = 1;
										d->scope = NULL;

										d->ofs = def->ofs+arraypart*type->size+parttype->ofs+i;
*/
										G_INT(def->ofs+arraypart*type->size+parttype->ofs+i) = 0;										
									}
									QCC_PR_Expect("}");
								}
								else
									QCC_PR_ParseError(ERR_UNEXPECTEDPUNCTUATION, "Unexpected punctuation");
							}
							else
							{
/*								//the executor defines strings as true c strings, but reads in index from string table.
								//structures can hide these strings.
								d = (void *)qccHunkAlloc (sizeof(QCC_def_t));
								d->next = NULL;
								pr.def_tail->next = d;
								pr.def_tail = d;

								d->type = parttype;
								d->name = "STRUCTIMMEDIATE";
								d->constant = constant;
								d->initialized = 1;
								d->scope = NULL;

								d->ofs = def->ofs+arraypart*type->size+parttype->ofs;
*/
								G_INT(def->ofs+arraypart*type->size+parttype->ofs) = QCC_CopyString(pr_immediate_string);
								QCC_PR_Lex ();
							}
							break;
						case ev_function:
							if (pr_token_type == tt_immediate)
							{
								if (pr_immediate._int != 0)
									QCC_PR_ParseError(ERR_NOTFUNCTIONTYPE, "Expected function name or NULL");
								G_FUNCTION(def->ofs+arraypart*type->size+parttype->ofs) = 0;
								QCC_PR_Lex();
							}
							else
							{
								name = QCC_PR_ParseName ();

								d = QCC_PR_GetDef (NULL, name, pr_scope, false, 0);
								if (!d)
									QCC_PR_ParseError(ERR_NOTDEFINED, "%s was not defined\n", name);
								else
									G_FUNCTION(def->ofs+arraypart*type->size+parttype->ofs) = G_FUNCTION(d->ofs);
							}
							break;
						default:
							QCC_PR_ParseError(ERR_TYPEINVALIDINSTRUCT, "type %i not valid in a struct", parttype->type);							
							QCC_PR_Lex();
							break;
						}
						if (!QCC_PR_Check(","))
							break;

						parttype=parttype->next;
					}
					QCC_PR_Expect("}");
					if (!QCC_PR_Check(","))
						break;
				}
				QCC_PR_Expect("}");
				continue;
			}

			else if (type->type == ev_integer)	//handle these differently, because they may need conversions
			{
				def->constant = constant;
				def->initialized = 1;
				memcpy (qcc_pr_globals + def->ofs, &pr_immediate, 4*type_size[pr_immediate_type->type]);
				QCC_PR_Lex ();

				if (pr_immediate_type->type == ev_float)
					G_INT(def->ofs) = (int)pr_immediate._float;
				else if (pr_immediate_type->type != ev_integer)
					QCC_PR_ParseError (ERR_BADIMMEDIATETYPE, "wrong immediate type for %s", name);

				continue;
			}
			else if (type->type == ev_string)
			{
				if (arraysize>=1 && QCC_PR_Check("{"))
				{
					int i;
					for (i = 0; i < arraysize; i++)
					{
						//the executor defines strings as true c strings, but reads in index from string table.
						//structures can hide these strings.
						if (i != 0)	//not for the first entry - already a string def for that
						{
							d = (void *)qccHunkAlloc (sizeof(QCC_def_t));
							d->next = NULL;
							pr.def_tail->next = d;
							pr.def_tail = d;

							d->type = type_string;
							d->name = "IMMEDIATE";
							d->constant = constant;
							d->initialized = 1;
							d->scope = NULL;

							d->ofs = def->ofs+i;
							if (d->ofs >= MAX_REGS)
								QCC_Error(ERR_TOOMANYGLOBALS, "MAX_REGS is too small");
						}

						(((int *)qcc_pr_globals)[def->ofs+i]) = QCC_CopyString(pr_immediate_string);
						QCC_PR_Lex ();

						if (!QCC_PR_Check(","))
							break;
					}
					QCC_PR_Expect("}");

					continue;
				}
				else if (arraysize<=1)
				{
					def->constant = constant;
					def->initialized = 1;
					(((int *)qcc_pr_globals)[def->ofs]) = QCC_CopyString(pr_immediate_string);
					QCC_PR_Lex ();

					if (pr_immediate_type->type == ev_float)
						G_INT(def->ofs) = (int)pr_immediate._float;
					else if (pr_immediate_type->type != ev_string)
						QCC_PR_ParseError (ERR_BADIMMEDIATETYPE, "wrong immediate type for %s", name);

					continue;
				}
				else
					QCC_PR_ParseError(ERR_ARRAYNEEDSBRACES, "Array initialisation requires curly brasces");
			}
			else if (type->type == ev_float)
			{
				if (arraysize>=1 && QCC_PR_Check("{"))
				{
					int i;
					for (i = 0; i < arraysize; i++)
					{
						if (pr_immediate_type->type != ev_float)
							QCC_PR_ParseError (ERR_BADIMMEDIATETYPE, "wrong immediate type for %s", name);
						(((float *)qcc_pr_globals)[def->ofs+i]) = pr_immediate._float;
						QCC_PR_Lex ();

						if (!QCC_PR_Check(","))
							break;
					}
					QCC_PR_Expect("}");

					continue;
				}
				else if (arraysize<=1)
				{
					def->constant = constant;
					def->initialized = 1;

					if (pr_immediate_type->type != ev_float)
						QCC_PR_ParseError (ERR_BADIMMEDIATETYPE, "wrong immediate type for %s", name);

					if (constant && opt_dupconstdefs)
					{
						if (def->ofs == oldglobals)
						{
							if (Hash_GetKey(&floatconstdefstable, *(int*)&pr_immediate._float))
								optres_dupconstdefs++;
							QCC_FreeOffset(def->ofs, def->type->size);
							d = QCC_MakeFloatDef(pr_immediate._float);
							d->references++;
							def->ofs = d->ofs;
							QCC_PR_Lex();
							continue;
						}
					}

					(((float *)qcc_pr_globals)[def->ofs]) = pr_immediate._float;
					QCC_PR_Lex ();

					continue;
				}
				else
					QCC_PR_ParseError(ERR_ARRAYNEEDSBRACES, "Array initialisation requires curly brasces");
			}
			else if (type->type == ev_vector)
			{
				if (arraysize>=1 && QCC_PR_Check("{"))
				{
					int i;
					for (i = 0; i < arraysize; i++)
					{
						if (pr_immediate_type->type != ev_vector)
							QCC_PR_ParseError (ERR_BADIMMEDIATETYPE, "wrong immediate type for %s", name);
						(((float *)qcc_pr_globals)[def->ofs+i*3+0]) = pr_immediate.vector[0];
						(((float *)qcc_pr_globals)[def->ofs+i*3+1]) = pr_immediate.vector[1];
						(((float *)qcc_pr_globals)[def->ofs+i*3+2]) = pr_immediate.vector[2];
						QCC_PR_Lex ();

						if (!QCC_PR_Check(","))
							break;
					}
					QCC_PR_Expect("}");

					continue;
				}
				else if (arraysize<=1)
				{
					def->constant = constant;
					def->initialized = 1;
					(((float *)qcc_pr_globals)[def->ofs+0]) = pr_immediate.vector[0];
					(((float *)qcc_pr_globals)[def->ofs+1]) = pr_immediate.vector[1];
					(((float *)qcc_pr_globals)[def->ofs+2]) = pr_immediate.vector[2];
					QCC_PR_Lex ();

					if (pr_immediate_type->type != ev_vector)
						QCC_PR_ParseError (ERR_BADIMMEDIATETYPE, "wrong immediate type for %s", name);

					continue;
				}
				else
					QCC_PR_ParseError(ERR_ARRAYNEEDSBRACES, "Array initialisation requires curly brasces");
			}
			else if (pr_token_type == tt_name)
			{
//				if (pr_scope)//create a new instance, emit a copy op
//				{
//					QCC_PR_ParseError ("name defined for local : %s", name);
//				}
//				else
				{
					d = QCC_PR_GetDef (NULL, pr_token, pr_scope, false, 0);
					if (!d)
						QCC_PR_ParseError (ERR_NOTDEFINED, "initialisation name not defined : %s", pr_token);
					if (!d->constant)
					{
						QCC_PR_ParseWarning (WARN_NOTCONSTANT, "initialisation name not a constant : %s", pr_token);
						QCC_PR_ParsePrintDef(WARN_NOTCONSTANT, d);
					}
					memcpy (def, d, sizeof(*d));
					def->name = name;
					def->initialized = true;
				}
				constant = true;
			}
			else if (pr_token_type != tt_immediate)
				QCC_PR_ParseError (ERR_BADIMMEDIATETYPE, "not an immediate for %s - %s", name, pr_token);
			else if (pr_immediate_type->type != type->type)
				QCC_PR_ParseError (ERR_BADIMMEDIATETYPE, "wrong immediate type for %s - %s", name, pr_token);
			else
				memcpy (qcc_pr_globals + def->ofs, &pr_immediate, 4*type_size[pr_immediate_type->type]);
	
			def->constant = constant;
			def->initialized = true;
			QCC_PR_Lex ();
		}
		
	} while (QCC_PR_Check (","));

	if (type->type == ev_function)
		QCC_PR_Check (";");
	else
		QCC_PR_Expect (";");
}

/*
============
PR_CompileFile

compiles the 0 terminated text, adding defintions to the pr structure
============
*/
pbool	QCC_PR_CompileFile (char *string, char *filename)
{	
	if (!pr.memory)
		QCC_Error (ERR_INTERNAL, "PR_CompileFile: Didn't clear");

	QCC_PR_ClearGrabMacros ();	// clear the frame macros

	compilingfile = filename;
		
	if (opt_filenames)
	{
		optres_filenames += strlen(filename);
		pr_file_p = qccHunkAlloc(strlen(filename)+1);
		strcpy(pr_file_p, filename);
		s_file = pr_file_p - strings;
		s_file2 = 0;
	}
	else
	{
		s_file = s_file2 = QCC_CopyString (filename);
	}
	pr_file_p = string;

	pr_source_line = 0;
	
	QCC_PR_NewLine (false);

	QCC_PR_Lex ();	// read first token

	while (pr_token_type != tt_eof)
	{
		if (setjmp(pr_parse_abort))
		{
			if (++pr_error_count > MAX_ERRORS)
				return false;
			QCC_PR_SkipToSemicolon ();
			if (pr_token_type == tt_eof)
				return false;		
		}

		pr_scope = NULL;	// outside all functions
		
		QCC_PR_ParseDefs (NULL);
	}
	
	return (pr_error_count == 0);
}

pbool QCC_Include(char *filename)
{
	char *newfile;
	char fname[512];
	char *opr_file_p;
	QCC_string_t os_file, os_file2;
	int opr_source_line;
	char *ocompilingfile;
	struct qcc_includechunk_s *oldcurrentchunk;
	extern struct qcc_includechunk_s *currentchunk;

	extern char qccmsourcedir[];

	ocompilingfile = compilingfile;
	os_file = s_file;
	os_file2 = s_file2;
	opr_source_line = pr_source_line;
	opr_file_p = pr_file_p;
	oldcurrentchunk = currentchunk;

	if (*filename == '/')
		strcpy(fname, filename+1);
	else
	{
		strcpy(fname, qccmsourcedir);
		strcat(fname, filename);
	}
	QCC_LoadFile(fname, (void*)&newfile);
	currentchunk = NULL;
	pr_file_p = newfile;
	QCC_PR_CompileFile(newfile, fname);
	currentchunk = oldcurrentchunk;

	compilingfile = ocompilingfile;
	s_file = os_file;
	s_file2 = os_file2;
	pr_source_line = opr_source_line;
	pr_file_p = opr_file_p;

//	QCC_PR_IncludeChunk(newfile, false, fname);

	return true;
}

#endif
