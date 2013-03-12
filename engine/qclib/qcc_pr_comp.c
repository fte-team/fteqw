#ifndef MINIMAL

#include "qcc.h"
void QCC_PR_ParseAsm(void);

#define MEMBERFIELDNAME "__m%s"

#define STRCMP(s1,s2) (((*s1)!=(*s2)) || strcmp(s1+1,s2+1))	//saves about 2-6 out of 120 - expansion of idea from fastqcc
#define STRNCMP(s1,s2,l) (((*s1)!=(*s2)) || strncmp(s1+1,s2+1,l))	//pathetic saving here.

extern char *compilingfile;

int conditional;

//standard qcc keywords
#define keyword_do		1
#define keyword_return	1
#define keyword_if		1
#define keyword_else	1
#define keyword_local	1
#define keyword_while	1

//extended keywords.
pbool keyword_asm;
pbool keyword_break;
pbool keyword_case;
pbool keyword_class;
pbool keyword_optional;
pbool keyword_const;	//fixme
pbool keyword_continue;
pbool keyword_default;
pbool keyword_entity;	//for skipping the local
pbool keyword_float;	//for skipping the local
pbool keyword_for;
pbool keyword_goto;
pbool keyword_int;	//for skipping the local
pbool keyword_integer;	//for skipping the local
pbool keyword_state;
pbool keyword_string;	//for skipping the local
pbool keyword_struct;
pbool keyword_switch;
pbool keyword_thinktime;
pbool keyword_var;	//allow it to be initialised and set around the place.
pbool keyword_vector;	//for skipping the local


pbool keyword_enum;	//kinda like in c, but typedef not supported.
pbool keyword_enumflags;	//like enum, but doubles instead of adds 1.
pbool keyword_typedef;	//fixme
#define keyword_codesys		flag_acc	//reacc needs this (forces the resultant crc)
#define keyword_function	flag_acc	//reacc needs this (reacc has this on all functions, wierd eh?)
#define keyword_objdata		flag_acc	//reacc needs this (following defs are fields rather than globals, use var to disable)
#define keyword_object		flag_acc	//reacc needs this (an entity)
#define keyword_pfunc		flag_acc	//reacc needs this (pointer to function)
#define keyword_system		flag_acc	//reacc needs this (potatos)
#define keyword_real		flag_acc	//reacc needs this (a float)
#define keyword_exit		flag_acc	//emits an OP_DONE opcode.
#define keyword_external	flag_acc	//reacc needs this (a builtin)
pbool keyword_extern;	//function is external, don't error or warn if the body was not found
pbool keyword_shared;	//mark global to be copied over when progs changes (part of FTE_MULTIPROGS)
pbool keyword_noref;	//nowhere else references this, don't strip it.
pbool keyword_nosave;	//don't write the def to the output.
pbool keyword_union;	//you surly know what a union is!

#define keyword_not			1	//hexenc support needs this, and fteqcc can optimise without it, but it adds an extra token after the if, so it can cause no namespace conflicts

pbool keywords_coexist;		//don't disable a keyword simply because a var was made with the same name.
pbool output_parms;			//emit some PARMX fields. confuses decompilers.
pbool autoprototype;		//take two passes over the source code. First time round doesn't enter and functions or initialise variables.
pbool pr_subscopedlocals;	//causes locals to be valid ONLY within their statement block. (they simply can't be referenced by name outside of it)
pbool flag_ifstring;		//makes if (blah) equivelent to if (blah != "") which resolves some issues in multiprogs situations.
pbool flag_iffloat;			//use an op_if_f instruction instead of op_if so if(-0) evaluates to false.
pbool flag_acc;				//reacc like behaviour of src files (finds *.qc in start dir and compiles all in alphabetical order)
pbool flag_caseinsensative;	//symbols will be matched to an insensative case if the specified case doesn't exist. This should b usable for any mod
pbool flag_laxcasts;		//Allow lax casting. This'll produce loadsa warnings of course. But allows compilation of certain dodgy code.
pbool flag_hashonly;		//Allows use of only #constant for precompiler constants, allows certain preqcc using mods to compile
pbool flag_fasttrackarrays;	//Faster arrays, dynamically detected, activated only in supporting engines.
pbool flag_msvcstyle;		//MSVC style warnings, so msvc's ide works properly
pbool flag_assume_integer;	//5 - is that an integer or a float? qcc says float. but we support int too, so maybe we want that instead?
pbool flag_filetimes;
pbool flag_typeexplicit;	//no implicit type conversions, you must do the casts yourself.

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
pbool opt_locals_overlapping;	//make the local vars of all functions occupy the same globals.
pbool opt_logicops;				//don't make conditions enter functions if the return value will be discarded due to a previous value. (C style if statements)
pbool opt_vectorcalls;			//vectors can be packed into 3 floats, which can yield lower numpr_globals, but cost two more statements per call (only works for q1 calling conventions).
pbool opt_simplifiedifs;		//if (f != 0) -> if (f). if (f == 0) -> ifnot (f)
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
int optres_locals_overlapping;
int optres_logicops;

int optres_test1;
int optres_test2;

void *(*pHash_Get)(hashtable_t *table, const char *name);
void *(*pHash_GetNext)(hashtable_t *table, const char *name, void *old);
void *(*pHash_Add)(hashtable_t *table, const char *name, void *data, bucket_t *);

QCC_def_t *QCC_PR_DummyDef(QCC_type_t *type, char *name, QCC_def_t *scope, int arraysize, unsigned int ofs, int referable, unsigned int flags);
QCC_type_t *QCC_PR_FindType (QCC_type_t *type);
QCC_type_t *QCC_PR_PointerType (QCC_type_t *pointsto);
QCC_type_t *QCC_PR_FieldType (QCC_type_t *pointsto);
QCC_def_t *QCC_PR_Term (int exprflags);
QCC_def_t *QCC_PR_GenerateFunctionCall (QCC_def_t *func, QCC_def_t *arglist[], int argcount);
void QCC_Marshal_Locals(int firststatement, int laststatement);

void QCC_PR_ParseState (void);
pbool simplestore;
pbool expandedemptymacro;

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

pbool debug_armour_defined;

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

//#define ASSOC_RIGHT_RESULT ASSOC_RIGHT

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
 {7, "*=", "MULSTORE_VF",	6, ASSOC_RIGHT_RESULT,				&type_vector, &type_float, &type_vector},
 {7, "*=", "MULSTOREP_F",	6, ASSOC_RIGHT_RESULT,				&type_pointer, &type_float, &type_float},
 {7, "*=", "MULSTOREP_VF",	6, ASSOC_RIGHT_RESULT,				&type_pointer, &type_float, &type_vector},

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

 {7, "|=", "BITSET_F",						6,	ASSOC_RIGHT,	&type_float, &type_float, &type_float},
 {7, "|=", "BITSETP_F",						6,	ASSOC_RIGHT,	&type_pointer, &type_float, &type_float},
 {7, "&~=", "BITCLR_F",						6,	ASSOC_RIGHT,	&type_float, &type_float, &type_float},
 {7, "&~=", "BITCLRP_F",					6,	ASSOC_RIGHT,	&type_pointer, &type_float, &type_float},

 {7, "<RAND0>", "RAND0",					-1, ASSOC_LEFT,	&type_void, &type_void, &type_float},
 {7, "<RAND1>", "RAND1",					-1, ASSOC_LEFT,	&type_float, &type_void, &type_float},
 {7, "<RAND2>", "RAND2",					-1, ASSOC_LEFT,	&type_float, &type_float, &type_float},
 {7, "<RANDV0>", "RANDV0",					-1, ASSOC_LEFT,	&type_void, &type_void, &type_vector},
 {7, "<RANDV1>", "RANDV1",					-1, ASSOC_LEFT,	&type_vector, &type_void, &type_vector},
 {7, "<RANDV2>", "RANDV2",					-1, ASSOC_LEFT,	&type_vector, &type_vector, &type_vector},

 {7, "<SWITCH_F>", "SWITCH_F",				-1, ASSOC_RIGHT,	&type_void, NULL, &type_void},
 {7, "<SWITCH_V>", "SWITCH_V",				-1, ASSOC_RIGHT,	&type_void, NULL, &type_void},
 {7, "<SWITCH_S>", "SWITCH_S",				-1, ASSOC_RIGHT,	&type_void, NULL, &type_void},
 {7, "<SWITCH_E>", "SWITCH_E",				-1, ASSOC_RIGHT,	&type_void, NULL, &type_void},
 {7, "<SWITCH_FNC>", "SWITCH_FNC",			-1, ASSOC_RIGHT,	&type_void, NULL, &type_void},

 {7, "<CASE>", "CASE",						-1, ASSOC_RIGHT,	&type_void, NULL, &type_void},
 {7, "<CASERANGE>", "CASERANGE",			-1, ASSOC_RIGHT,	&type_void, &type_void, NULL},


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
 {7, "=",	"STORE_IF", 6, ASSOC_RIGHT,			&type_float, &type_integer, &type_integer},
 {7, "=",	"STORE_FI", 6, ASSOC_RIGHT,			&type_integer, &type_float, &type_float},

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

 {7, "^", "XOR_I", 3, ASSOC_LEFT,				&type_integer,	&type_integer, &type_integer},
 {7, ">>", "RSHIFT_I", 3, ASSOC_LEFT,			&type_integer,	&type_integer, &type_integer},
 {7, "<<", "LSHIFT_I", 3, ASSOC_LEFT,			&type_integer,	&type_integer, &type_integer},

										//var,		offset			return
 {7, "<ARRAY>", "GET_POINTER", -1, ASSOC_LEFT,	&type_float,		&type_integer, &type_pointer},
 {7, "<ARRAY>", "MUL4ADD_I", -1, ASSOC_LEFT,		&type_pointer,	&type_integer, &type_pointer},

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
 {7, "<LOADP_C>", "LOADP_C",	1, ASSOC_LEFT,	&type_string,	&type_float, &type_float},
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

{7, "&&", "AND_I", 7, ASSOC_LEFT,				&type_integer,	&type_integer,	&type_integer},
{7, "||", "OR_I", 7, ASSOC_LEFT,				&type_integer,	&type_integer,	&type_integer},
{7, "&&", "AND_IF", 7, ASSOC_LEFT,				&type_integer,	&type_float,	&type_integer},
{7, "||", "OR_IF", 7, ASSOC_LEFT,				&type_integer,	&type_float,	&type_integer},
{7, "&&", "AND_FI", 7, ASSOC_LEFT,				&type_float,	&type_integer,	&type_integer},
{7, "||", "OR_FI", 7, ASSOC_LEFT,				&type_float,	&type_integer,	&type_integer},
{7, "!=", "NE_IF", 5, ASSOC_LEFT,				&type_integer,	&type_float,	&type_integer},
{7, "!=", "NE_FI", 5, ASSOC_LEFT,				&type_float,	&type_float,	&type_integer},






{7, "<>",	"GSTOREP_I", -1, ASSOC_LEFT,			&type_float,	&type_float,	&type_float},
{7, "<>",	"GSTOREP_F", -1, ASSOC_LEFT,			&type_float,	&type_float,	&type_float},
{7, "<>",	"GSTOREP_ENT", -1, ASSOC_LEFT,			&type_float,	&type_float,	&type_float},
{7, "<>",	"GSTOREP_FLD", -1, ASSOC_LEFT,			&type_float,	&type_float,	&type_float},
{7, "<>",	"GSTOREP_S", -1, ASSOC_LEFT,			&type_float,	&type_float,	&type_float},
{7, "<>",	"GSTOREP_FNC", -1, ASSOC_LEFT,			&type_float,	&type_float,	&type_float},
{7, "<>",	"GSTOREP_V", -1, ASSOC_LEFT,			&type_float,	&type_float,	&type_float},

{7, "<>",	"GADDRESS", -1, ASSOC_LEFT,				&type_float,	&type_float,	&type_float},

{7, "<>",	"GLOAD_I",		-1, ASSOC_LEFT,				&type_float,	&type_float,	&type_float},
{7, "<>",	"GLOAD_F",		-1, ASSOC_LEFT,				&type_float,	&type_float,	&type_float},
{7, "<>",	"GLOAD_FLD",	-1, ASSOC_LEFT,				&type_float,	&type_float,	&type_float},
{7, "<>",	"GLOAD_ENT",	-1, ASSOC_LEFT,				&type_float,	&type_float,	&type_float},
{7, "<>",	"GLOAD_S",		-1, ASSOC_LEFT,				&type_float,	&type_float,	&type_float},
{7, "<>",	"GLOAD_FNC",	-1, ASSOC_LEFT,				&type_float,	&type_float,	&type_float},

{7, "<>",	"BOUNDCHECK",	-1, ASSOC_LEFT,				&type_integer,	NULL,	NULL},

{7, "<UNUSED>",	"UNUSED",		6,	ASSOC_RIGHT,				&type_void,	&type_void,	&type_void},
{7, "<PUSH>",	"PUSH",		-1, ASSOC_RIGHT,			&type_float,	&type_void,		&type_pointer},
{7, "<POP>",	"POP",		-1, ASSOC_RIGHT,			&type_float,	&type_void,		&type_void},

{7, "<SWITCH_I>", "SWITCH_I",				-1, ASSOC_LEFT,	&type_void, NULL, &type_void},
{7, "<>",	"GLOAD_S",		-1, ASSOC_LEFT,				&type_float,	&type_float,	&type_float},

{6, "<IF_F>",	"IF_F",		-1, ASSOC_RIGHT,				&type_float, NULL, &type_void},
{6, "<IFNOT_F>","IFNOT_F",	-1, ASSOC_RIGHT,				&type_float, NULL, &type_void},

/* emulated ops begin here */
 {7, "<>",	"OP_EMULATED",		-1, ASSOC_LEFT,				&type_float,	&type_float,	&type_float},


 {7, "|=", "BITSET_I",		6,	ASSOC_RIGHT,				&type_integer, &type_integer, &type_integer},
 {7, "|=", "BITSETP_I",		6,	ASSOC_RIGHT,				&type_pointer, &type_integer, &type_integer},


 {7, "*=", "MULSTORE_I",	6,	ASSOC_RIGHT_RESULT,		&type_integer, &type_integer, &type_integer},
 {7, "/=", "DIVSTORE_I",	6,	ASSOC_RIGHT_RESULT,		&type_integer, &type_integer, &type_integer},
 {7, "+=", "ADDSTORE_I",	6,	ASSOC_RIGHT_RESULT,		&type_integer, &type_integer, &type_integer},
 {7, "-=", "SUBSTORE_I",	6,	ASSOC_RIGHT_RESULT,		&type_integer, &type_integer, &type_integer},

 {7, "*=", "MULSTOREP_I",	6,	ASSOC_RIGHT_RESULT,		&type_pointer, &type_integer, &type_integer},
 {7, "/=", "DIVSTOREP_I",	6,	ASSOC_RIGHT_RESULT,		&type_pointer, &type_integer, &type_integer},
 {7, "+=", "ADDSTOREP_I",	6,	ASSOC_RIGHT_RESULT,		&type_pointer, &type_integer, &type_integer},
 {7, "-=", "SUBSTOREP_I",	6,	ASSOC_RIGHT_RESULT,		&type_pointer, &type_integer, &type_integer},

 {7, "*=", "MULSTORE_IF",	6,	ASSOC_RIGHT_RESULT,		&type_integer, &type_float, &type_float},
 {7, "*=", "MULSTOREP_IF",	6,	ASSOC_RIGHT_RESULT,		&type_intpointer, &type_float, &type_float},
 {7, "/=", "DIVSTORE_IF",	6,	ASSOC_RIGHT_RESULT,		&type_integer, &type_float, &type_float},
 {7, "/=", "DIVSTOREP_IF",	6,	ASSOC_RIGHT_RESULT,		&type_intpointer, &type_float, &type_float},
 {7, "+=", "ADDSTORE_IF",	6,	ASSOC_RIGHT_RESULT,		&type_integer, &type_float, &type_float},
 {7, "+=", "ADDSTOREP_IF",	6,	ASSOC_RIGHT_RESULT,		&type_intpointer, &type_float, &type_float},
 {7, "-=", "SUBSTORE_IF",	6,	ASSOC_RIGHT_RESULT,		&type_integer, &type_float, &type_float},
 {7, "-=", "SUBSTOREP_IF",	6,	ASSOC_RIGHT_RESULT,		&type_intpointer, &type_float, &type_float},

 {7, "*=", "MULSTORE_FI",	6,	ASSOC_RIGHT_RESULT,		&type_float, &type_integer, &type_float},
 {7, "*=", "MULSTOREP_FI",	6,	ASSOC_RIGHT_RESULT,		&type_floatpointer, &type_integer, &type_float},
 {7, "/=", "DIVSTORE_FI",	6,	ASSOC_RIGHT_RESULT,		&type_float, &type_integer, &type_float},
 {7, "/=", "DIVSTOREP_FI",	6,	ASSOC_RIGHT_RESULT,		&type_floatpointer, &type_integer, &type_float},
 {7, "+=", "ADDSTORE_FI",	6,	ASSOC_RIGHT_RESULT,		&type_float, &type_integer, &type_float},
 {7, "+=", "ADDSTOREP_FI",	6,	ASSOC_RIGHT_RESULT,		&type_floatpointer, &type_integer, &type_float},
 {7, "-=", "SUBSTORE_FI",	6,	ASSOC_RIGHT_RESULT,		&type_float, &type_integer, &type_float},
 {7, "-=", "SUBSTOREP_FI",	6,	ASSOC_RIGHT_RESULT,		&type_floatpointer, &type_integer, &type_float},

 {7, "*=", "MULSTORE_VI",	6, ASSOC_RIGHT_RESULT,		&type_vector, &type_integer, &type_vector},
 {7, "*=", "MULSTOREP_VI",	6, ASSOC_RIGHT_RESULT,		&type_pointer, &type_integer, &type_vector},

 {7, "=", "LOADA_STRUCT",	6, ASSOC_LEFT,				&type_float,	&type_integer, &type_float},

 {7, "=",	"STOREP_P",		6,	ASSOC_RIGHT,			&type_pointer,	&type_pointer,	&type_pointer},
 {7, "~",	"BINARYNOT_F", -1, ASSOC_LEFT,				&type_float,	&type_void, &type_float},
 {7, "~",	"BINARYNOT_I", -1, ASSOC_LEFT,				&type_integer,	&type_void, &type_integer},

 {0, NULL}
};


pbool OpAssignsToC(unsigned int op)
{
	// calls, switches and cases DON'T
	if(pr_opcodes[op].type_c == &type_void)
		return false;
	if(op >= OP_SWITCH_F && op <= OP_CALL8H)
		return false;
	if(op >= OP_RAND0 && op <= OP_RANDV2)
		return false;
	// they use a and b, but have 3 types
	// safety
	if(op >= OP_BITSET && op <= OP_BITCLRP)
		return false;
	/*if(op >= OP_STORE_I && op <= OP_STORE_FI)
	  return false; <- add STOREP_*?*/
	if(op == OP_STOREP_C || op == OP_LOADP_C)
		return false;
	if(op >= OP_MULSTORE_F && op <= OP_SUBSTOREP_V)
		return false;
	return true;
}
pbool OpAssignsToB(unsigned int op)
{
	if(op >= OP_BITSET && op <= OP_BITCLRP)
		return true;
	if(op >= OP_STORE_I && op <= OP_STORE_FI)
		return true;
	if(op == OP_STOREP_C || op == OP_LOADP_C)
		return true;
	if(op >= OP_MULSTORE_F && op <= OP_SUBSTOREP_V)
		return true;
	if(op >= OP_STORE_F && op <= OP_STOREP_FNC)
		return true;
	return false;
}
/*pbool OpAssignedTo(QCC_def_t *v, unsigned int op)
{
	if(OpAssignsToC(op))
	{
	}
	else if(OpAssignsToB(op))
	{
	}
	return false;
}
*/
#undef ASSOC_RIGHT_RESULT

#define	TOP_PRIORITY	7
#define FUNC_PRIORITY	1
#define UNARY_PRIORITY	1
#define	NOT_PRIORITY	5
//conditional and/or
#define CONDITION_PRIORITY 7


//this system cuts out 10/120
//these evaluate as top first.
QCC_opcode_t *opcodeprioritized[TOP_PRIORITY+1][128] =
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
		&pr_opcodes[OP_MUL_IV],
		&pr_opcodes[OP_MUL_VF],
		&pr_opcodes[OP_MUL_VI],
		&pr_opcodes[OP_MUL_I],
		&pr_opcodes[OP_MUL_FI],
		&pr_opcodes[OP_MUL_IF],

		&pr_opcodes[OP_DIV_F],
		&pr_opcodes[OP_DIV_I],
		&pr_opcodes[OP_DIV_FI],
		&pr_opcodes[OP_DIV_IF],
		&pr_opcodes[OP_DIV_VF],

		&pr_opcodes[OP_BITAND_F],
		&pr_opcodes[OP_BITAND_I],
		&pr_opcodes[OP_BITAND_IF],
		&pr_opcodes[OP_BITAND_FI],

		&pr_opcodes[OP_BITOR_F],
		&pr_opcodes[OP_BITOR_I],
		&pr_opcodes[OP_BITOR_IF],
		&pr_opcodes[OP_BITOR_FI],

		&pr_opcodes[OP_XOR_I],
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
		&pr_opcodes[OP_NE_IF],
		&pr_opcodes[OP_NE_FI],

		&pr_opcodes[OP_LE_F],
		&pr_opcodes[OP_LE_I],
		&pr_opcodes[OP_LE_IF],
		&pr_opcodes[OP_LE_FI],
		&pr_opcodes[OP_GE_F],
		&pr_opcodes[OP_GE_I],
		&pr_opcodes[OP_GE_IF],
		&pr_opcodes[OP_GE_FI],
		&pr_opcodes[OP_LT_F],
		&pr_opcodes[OP_LT_I],
		&pr_opcodes[OP_LT_IF],
		&pr_opcodes[OP_LT_FI],
		&pr_opcodes[OP_GT_F],
		&pr_opcodes[OP_GT_I],
		&pr_opcodes[OP_GT_IF],
		&pr_opcodes[OP_GT_FI],

		NULL
	}, {	//6
		&pr_opcodes[OP_STOREP_P],

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

		&pr_opcodes[OP_DIVSTORE_F],
		&pr_opcodes[OP_DIVSTORE_I],
		&pr_opcodes[OP_DIVSTORE_FI],
		&pr_opcodes[OP_DIVSTORE_IF],
		&pr_opcodes[OP_DIVSTOREP_F],
		&pr_opcodes[OP_DIVSTOREP_I],
		&pr_opcodes[OP_DIVSTOREP_IF],
		&pr_opcodes[OP_DIVSTOREP_FI],
		&pr_opcodes[OP_MULSTORE_F],
		&pr_opcodes[OP_MULSTORE_VF],
		&pr_opcodes[OP_MULSTORE_VI],
		&pr_opcodes[OP_MULSTORE_I],
		&pr_opcodes[OP_MULSTORE_IF],
		&pr_opcodes[OP_MULSTORE_FI],
		&pr_opcodes[OP_MULSTOREP_F],
		&pr_opcodes[OP_MULSTOREP_VF],
		&pr_opcodes[OP_MULSTOREP_VI],
		&pr_opcodes[OP_MULSTOREP_I],
		&pr_opcodes[OP_MULSTOREP_IF],
		&pr_opcodes[OP_MULSTOREP_FI],
		&pr_opcodes[OP_ADDSTORE_F],
		&pr_opcodes[OP_ADDSTORE_V],
		&pr_opcodes[OP_ADDSTORE_I],
		&pr_opcodes[OP_ADDSTORE_IF],
		&pr_opcodes[OP_ADDSTORE_FI],
		&pr_opcodes[OP_ADDSTOREP_F],
		&pr_opcodes[OP_ADDSTOREP_V],
		&pr_opcodes[OP_ADDSTOREP_I],
		&pr_opcodes[OP_ADDSTOREP_IF],
		&pr_opcodes[OP_ADDSTOREP_FI],
		&pr_opcodes[OP_SUBSTORE_F],
		&pr_opcodes[OP_SUBSTORE_V],
		&pr_opcodes[OP_SUBSTORE_I],
		&pr_opcodes[OP_SUBSTORE_IF],
		&pr_opcodes[OP_SUBSTORE_FI],
		&pr_opcodes[OP_SUBSTOREP_F],
		&pr_opcodes[OP_SUBSTOREP_V],
		&pr_opcodes[OP_SUBSTOREP_I],
		&pr_opcodes[OP_SUBSTOREP_IF],
		&pr_opcodes[OP_SUBSTOREP_FI],

		&pr_opcodes[OP_BITSET],
		&pr_opcodes[OP_BITSET_I],
//		&pr_opcodes[OP_BITSET_IF],
//		&pr_opcodes[OP_BITSET_FI],
		&pr_opcodes[OP_BITSETP],
		&pr_opcodes[OP_BITSETP_I],
//		&pr_opcodes[OP_BITSETP_IF],
//		&pr_opcodes[OP_BITSETP_FI],
		&pr_opcodes[OP_BITCLR],
		&pr_opcodes[OP_BITCLRP],

		NULL
	}, {	//7
		&pr_opcodes[OP_AND_F],
		&pr_opcodes[OP_AND_I],
		&pr_opcodes[OP_AND_IF],
		&pr_opcodes[OP_AND_FI],
		&pr_opcodes[OP_OR_F],
		&pr_opcodes[OP_OR_I],
		&pr_opcodes[OP_OR_IF],
		&pr_opcodes[OP_OR_FI],
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
	case QCF_QTEST:
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
	case QCF_FTEH2:
	case QCF_FTE:
	case QCF_FTEDEBUG:
		//no emulated opcodes
		if (num >= OP_NUMREALOPS)
			return false;
		return true;
	case QCF_DARKPLACES:
		//all id opcodes.
		if (num < OP_MULSTORE_F)
			return true;

		//no emulated opcodes
		if (num >= OP_NUMREALOPS)
			return false;

		//extended opcodes.
		//DPFIXME: this is a list of the extended opcodes. I was conservative regarding supported ones.
		//         at the time of writing, these are the ones that look like they'll work just fine in Blub\0's patch.
		//         the ones that looked too permissive with bounds checks, or would give false positives are disabled.
		//         if the DP guys want I can change them as desired.
		switch(num)
		{
		//maths and conditionals (simple opcodes that read from specific globals and write to a global)
		case OP_ADD_I:
		case OP_ADD_IF:
		case OP_ADD_FI:
		case OP_SUB_I:
		case OP_SUB_IF:
		case OP_SUB_FI:
		case OP_MUL_I:
		case OP_MUL_IF:
		case OP_MUL_FI:
		case OP_MUL_VI:
		case OP_DIV_VF:
		case OP_DIV_I:
		case OP_DIV_IF:
		case OP_DIV_FI:
		case OP_BITAND_I:
		case OP_BITOR_I:
		case OP_BITAND_IF:
		case OP_BITOR_IF:
		case OP_BITAND_FI:
		case OP_BITOR_FI:
		case OP_GE_I:
		case OP_LE_I:
		case OP_GT_I:
		case OP_LT_I:
		case OP_AND_I:
		case OP_OR_I:
		case OP_GE_IF:
		case OP_LE_IF:
		case OP_GT_IF:
		case OP_LT_IF:
		case OP_AND_IF:
		case OP_OR_IF:
		case OP_GE_FI:
		case OP_LE_FI:
		case OP_GT_FI:
		case OP_LT_FI:
		case OP_AND_FI:
		case OP_OR_FI:
		case OP_NOT_I:
		case OP_EQ_I:
		case OP_EQ_IF:
		case OP_EQ_FI:
		case OP_NE_I:
		case OP_NE_IF:
		case OP_NE_FI:
			return true;

		//stores into a pointer (generated from 'ent.field=XXX')
		case OP_STOREP_I:	//no worse than the other OP_STOREP_X functions
		//reads from an entity field
		case OP_LOAD_I:		//no worse than the other OP_LOAD_X functions.
		case OP_LOAD_P:
			return true;

		//stores into the globals array.
		//they can change any global dynamically, but thats no security risk.
		//fteqcc will not automatically generate these.
		//fteqw does not support them either.
		case OP_GSTOREP_I:
		case OP_GSTOREP_F:
		case OP_GSTOREP_ENT:
		case OP_GSTOREP_FLD:
		case OP_GSTOREP_S:
		case OP_GSTOREP_FNC:
		case OP_GSTOREP_V:
			return true;

		//this opcode looks weird
		case OP_GADDRESS://floatc = globals[inta + floatb] (fte does not support)
			return true;

		//fteqcc will not automatically generate these
		//fteqw does not support them either, for that matter.
		case OP_GLOAD_I://c = globals[inta]
		case OP_GLOAD_F://note: fte does not support these
		case OP_GLOAD_FLD:
		case OP_GLOAD_ENT:
		case OP_GLOAD_S:
		case OP_GLOAD_FNC:
			return true;
		case OP_GLOAD_V:
			return false;	//DPFIXME: this is commented out in the patch I was given a link to... because the opcode wasn't defined.

		//these are reportedly functional.
		case OP_CALL8H:
		case OP_CALL7H:
		case OP_CALL6H:
		case OP_CALL5H:
		case OP_CALL4H:
		case OP_CALL3H:
		case OP_CALL2H:
		case OP_CALL1H:
			return true;

		case OP_RAND0:
		case OP_RAND1:
		case OP_RAND2:
		case OP_RANDV0:
		case OP_RANDV1:
		case OP_RANDV2:
			return true;

		case OP_BITSET: // b |= a
		case OP_BITCLR: // b &= ~a
		case OP_BITSETP: // *b |= a
		case OP_BITCLRP: // *b &= ~a
			return false;	//FIXME: I do not fully follow the controversy over these.

		case OP_SWITCH_F:
		case OP_SWITCH_V:
		case OP_SWITCH_S:
		case OP_SWITCH_E:
		case OP_SWITCH_FNC:
		case OP_CASE:
		case OP_CASERANGE:
			return true;

		//assuming the pointers here are fine, the return values are a little strange.
		//but its fine
		case OP_ADDSTORE_F:
		case OP_ADDSTORE_V:
		case OP_ADDSTOREP_F: // e.f += f
		case OP_ADDSTOREP_V: // e.v += v
		case OP_SUBSTORE_F:
		case OP_SUBSTORE_V:
		case OP_SUBSTOREP_F: // e.f += f
		case OP_SUBSTOREP_V: // e.v += v
			return true;

		case OP_LOADA_I:
		case OP_LOADA_F:
		case OP_LOADA_FLD:
		case OP_LOADA_ENT:
		case OP_LOADA_S:
		case OP_LOADA_FNC:
		case OP_LOADA_V:
			return false;	//DPFIXME: DP does not bounds check these properly. I won't generate them.

		case OP_CONV_ITOF:
		case OP_CONV_FTOI:
			return true;	//these look fine.

		case OP_STOREP_C: // store a char in a string
			return false; //DPFIXME: dp's bounds check may give false positives with expected uses.

		case OP_MULSTORE_F:
		case OP_MULSTORE_VF:
		case OP_MULSTOREP_F:
		case OP_MULSTOREP_VF: // e.v *= f
		case OP_DIVSTORE_F:
		case OP_DIVSTOREP_F:
		case OP_STORE_IF:
		case OP_STORE_FI:
		case OP_STORE_P:
		case OP_STOREP_IF: // store a value to a pointer
		case OP_STOREP_FI:
		case OP_IFNOT_S:
		case OP_IF_S:
			return true;

		case OP_IFNOT_F:	//added, but not in dp yet
		case OP_IF_F:
			return false;

		case OP_CP_ITOF:
		case OP_CP_FTOI:
			return false;	//DPFIXME: These are not bounds checked at all.
		case OP_GLOBALADDRESS:
			return true;	//DPFIXME: DP will reject these pointers if they are ever used.
		case OP_POINTER_ADD:
			return true;	//just maths.

		case OP_ADD_SF: //(char*)c = (char*)a + (float)b
		case OP_SUB_S:  //(float)c = (char*)a - (char*)b
			return true;
		case OP_LOADP_C:        //load character from a string
			return false;	//DPFIXME: DP looks like it'll reject these or wrongly allow.

		case OP_LOADP_I:
		case OP_LOADP_F:
		case OP_LOADP_FLD:
		case OP_LOADP_ENT:
		case OP_LOADP_S:
		case OP_LOADP_FNC:
		case OP_LOADP_V:
			return true;

		case OP_XOR_I:
		case OP_RSHIFT_I:
		case OP_LSHIFT_I:
			return true;

		case OP_FETCH_GBL_F:
		case OP_FETCH_GBL_S:
		case OP_FETCH_GBL_E:
		case OP_FETCH_GBL_FNC:
		case OP_FETCH_GBL_V:
			return false;	//DPFIXME: DP will not bounds check this properly, it is too permissive.
		case OP_CSTATE:
		case OP_CWSTATE:
			return false;	//DP does not support this hexenc opcode.
		case OP_THINKTIME:
			return true;	//but it does support this one.

		default:			//anything I forgot to mention is new.
			return false;
		}
	}
	return false;
}

#define EXPR_WARN_ABOVE_1 2
#define EXPR_DISALLOW_COMMA 4
#define EXPR_DISALLOW_ARRAYASSIGN 8
QCC_def_t *QCC_PR_Expression (int priority, int exprflags);
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
static int QCC_ShouldConvert(QCC_def_t *var, etype_t wanted)
{
	/*no conversion needed*/
	if (var->type->type == wanted)
		return 0;
	if (var->type->type == ev_integer && wanted == ev_function)
		return 0;
	if (var->type->type == ev_integer && wanted == ev_pointer)
		return 0;
	/*stuff needs converting*/
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

	/*impossible*/
	return -1;
}
QCC_def_t *QCC_SupplyConversion(QCC_def_t *var, etype_t wanted, pbool fatal)
{
	extern char *basictypenames[];
	int o;

	if (pr_classtype && var->type->type == ev_field && wanted != ev_field)
	{
		if (pr_classtype)
		{	//load self.var into a temp
			QCC_def_t *self;
			self = QCC_PR_GetDef(type_entity, "self", NULL, true, 0, false);
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

	if (o == 0) //type already matches
		return var;
	if (flag_typeexplicit)
		QCC_PR_ParseErrorPrintDef(ERR_TYPEMISMATCH, var, "Implicit type mismatch. Needed %s, got %s.", basictypenames[wanted], basictypenames[var->type->type]);
	if (o < 0)
	{
		if (fatal)
			QCC_PR_ParseErrorPrintDef(ERR_TYPEMISMATCH, var, "Implicit type mismatch. Needed %s, got %s.", basictypenames[wanted], basictypenames[var->type->type]);
		else
			return var;
	}

	return QCC_PR_Statement(&pr_opcodes[o], var, NULL, NULL);	//conversion return value
}
QCC_def_t *QCC_MakeTranslateStringConst(char *value);
QCC_def_t *QCC_MakeStringConst(char *value);
QCC_def_t *QCC_MakeFloatConst(float value);
QCC_def_t *QCC_MakeIntConst(int value);
QCC_def_t *QCC_MakeVectorConst(float a, float b, float c);

int tempsused;
//assistant functions. This can safly be bipassed with the old method for more complex things.
gofs_t QCC_GetFreeLocalOffsetSpace(unsigned int size)
{
	gofs_t ofs = locals_end;
	locals_end += size;
	return ofs;
}
gofs_t QCC_GetFreeTempOffsetSpace(unsigned int size)
{
	gofs_t ofs = FIRST_TEMP + tempsused;
	tempsused += size;
	return ofs;
}
gofs_t QCC_GetFreeGlobalOffsetSpace(unsigned int size)
{
	int ofs;

	ofs = numpr_globals;
	numpr_globals+=size;

	if (numpr_globals >= MAX_REGS)
	{
		if (!opt_overlaptemps || !opt_locals_overlapping)
			QCC_Error(ERR_TOOMANYGLOBALS, "numpr_globals exceeded MAX_REGS - you'll need to use more optimisations");
		else
			QCC_Error(ERR_TOOMANYGLOBALS, "numpr_globals exceeded MAX_REGS");
	}

	return ofs;
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
			QCC_Error(ERR_INTERNAL, "Internal error: temp has scope not equal to current scope");

		if (!t)
		{
			//allocate a new one
			t = qccHunkAlloc(sizeof(temp_t));
			t->size = type->size;
			t->next = functemps;
			functemps = t;

			t->ofs = QCC_GetFreeTempOffsetSpace(t->size);

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
		// we're not going to reallocate any temps so allocate permanently
		var_c->ofs = QCC_GetFreeTempOffsetSpace(type->size);
		numtemps+=type->size;
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

	if (def_ret.temp && def_ret.temp->used)
	{
		QCC_PR_ParseWarning(WARN_DEBUGGING, "Return value still in use in %s", pr_scope->name);
		def_ret.temp->used = false;
	}

	t = functemps;
	while(t)
	{
		if (t->used && !pr_error_count)	//don't print this after an error jump out.
			QCC_PR_ParseWarning(WARN_DEBUGGING, "Internal: temp(ofs %i) was not released in %s. This implies miscompilation.", t->ofs, pr_scope->name);
		t->used = false;
		t = t->next;
	}
}
#else
#define QCC_FreeTemps()
#endif
void QCC_PurgeTemps(void)
{
	functemps = NULL;
}

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

static void QCC_LockTemp(QCC_def_t *d)
{
	if (d->temp && d->temp->used)
		d->temp->scope = pr_scope;
}
static void QCC_ForceLockTempForOffset(int ofs)
{
	temp_t *t;
	for (t = functemps; t; t = t->next)
		if(t->ofs == ofs /* && t->used */)
			t->scope = pr_scope;
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
		if (pr_opcodes[st->op].type_a && st->a >= t->ofs && st->a < t->ofs + t->size)
		{
			if (!newofs)
			{
				newofs = QCC_GetFreeLocalOffsetSpace(t->size);
				numtemps+=t->size;

				def = QCC_PR_DummyDef(type_float, NULL, pr_scope, t->size, newofs, false, 0);
				def->nextlocal = pr.localvars;
#ifdef WRITEASM
				sprintf(buffer, "locked_%i", t->ofs);
				def->name = qccHunkAlloc(strlen(buffer)+1);
				strcpy(def->name, buffer);
#endif
				pr.localvars = def;
			}
			st->a = st->a - t->ofs + newofs;
		}
		if (pr_opcodes[st->op].type_b && st->b >= t->ofs && st->b < t->ofs + t->size)
		{
			if (!newofs)
			{
				newofs = QCC_GetFreeLocalOffsetSpace(t->size);
				numtemps+=t->size;

				def = QCC_PR_DummyDef(type_float, NULL, pr_scope, t->size, newofs, false, 0);
				def->nextlocal = pr.localvars;
#ifdef WRITEASM
				sprintf(buffer, "locked_%i", t->ofs);
				def->name = qccHunkAlloc(strlen(buffer)+1);
				strcpy(def->name, buffer);
#endif
				pr.localvars = def;
			}
			st->b = st->b - t->ofs + newofs;
		}
		if (pr_opcodes[st->op].type_c && st->c >= t->ofs && st->c < t->ofs + t->size)
		{
			if (!newofs)
			{
				newofs = QCC_GetFreeLocalOffsetSpace(t->size);
				numtemps+=t->size;

				def = QCC_PR_DummyDef(type_float, NULL, pr_scope, t->size, newofs, false, 0);
				def->nextlocal = pr.localvars;
#ifdef WRITEASM
				sprintf(buffer, "locked_%i", t->ofs);
				def->name = qccHunkAlloc(strlen(buffer)+1);
				strcpy(def->name, buffer);
#endif
				pr.localvars = def;
			}
			st->c = st->c - t->ofs + newofs;
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
		if (var->arraysize)
			fprintf(f, "local %s %s[%i];\n", TypeName(var->type), var->name, var->arraysize);
		else
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
				sprintf(message, "temp_%i_%c", t->ofs, 'x' + (ofs-t->ofs)%3);
			else
				sprintf(message, "temp_%i", t->ofs);
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

QCC_dstatement_t *QCC_PR_SimpleStatement( int op, int var_a, int var_b, int var_c, int force);

QCC_def_t *QCC_PR_Statement (QCC_opcode_t *op, QCC_def_t *var_a, QCC_def_t *var_b, QCC_dstatement_t **outstatement)
{
	QCC_dstatement_t	*statement;
	QCC_def_t			*var_c=NULL, *temp=NULL;

	if (outstatement == (QCC_dstatement_t **)0xffffffff)
		outstatement = NULL;
	else if (op->priority != -1 && op->priority != CONDITION_PRIORITY)
	{
		if (op->associative!=ASSOC_LEFT)
		{
			if (op->type_a != &type_pointer)
				var_b = QCC_SupplyConversion(var_b, (*op->type_a)->type, false);
		}
		else
		{
			if (var_a)
				var_a = QCC_SupplyConversion(var_a, (*op->type_a)->type, false);
			if (var_b)
				var_b = QCC_SupplyConversion(var_b, (*op->type_b)->type, false);
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
					QCC_PR_ParseWarning(0, "Implicit cast from '%s' to '%s'", var_a->type->name, var_b->type->name);
	}

	//maths operators
	if (opt_constantarithmatic)
	{
		if (var_a && var_a->constant)
		{
			if (var_b && var_b->constant)
			{
				//both are constants
				switch (op - pr_opcodes)	//improve some of the maths.
				{
				case OP_LOADA_F:
				case OP_LOADA_V:
				case OP_LOADA_S:
				case OP_LOADA_ENT:
				case OP_LOADA_FLD:
				case OP_LOADA_FNC:
				case OP_LOADA_I:
					{
						QCC_def_t *nd;
						nd = (void *)qccHunkAlloc (sizeof(QCC_def_t));
						memset (nd, 0, sizeof(QCC_def_t));
						nd->type = var_a->type;
						nd->ofs = var_a->ofs + G_INT(var_b->ofs);
						nd->temp = var_a->temp;
						nd->constant = true;
						nd->name = var_a->name;
						return nd;
					}
					break;
				case OP_BITOR_F:
					optres_constantarithmatic++;
					return QCC_MakeFloatConst((float)((int)G_FLOAT(var_a->ofs) | (int)G_FLOAT(var_b->ofs)));
				case OP_BITAND_F:
					optres_constantarithmatic++;
					return QCC_MakeFloatConst((float)((int)G_FLOAT(var_a->ofs) & (int)G_FLOAT(var_b->ofs)));
				case OP_MUL_F:
					optres_constantarithmatic++;
					return QCC_MakeFloatConst(G_FLOAT(var_a->ofs) * G_FLOAT(var_b->ofs));
				case OP_DIV_F:
					optres_constantarithmatic++;
					return QCC_MakeFloatConst(G_FLOAT(var_a->ofs) / G_FLOAT(var_b->ofs));
				case OP_ADD_F:
					optres_constantarithmatic++;
					return QCC_MakeFloatConst(G_FLOAT(var_a->ofs) + G_FLOAT(var_b->ofs));
				case OP_SUB_F:
					optres_constantarithmatic++;
					return QCC_MakeFloatConst(G_FLOAT(var_a->ofs) - G_FLOAT(var_b->ofs));

				case OP_BITOR_I:
					optres_constantarithmatic++;
					return QCC_MakeIntConst(G_INT(var_a->ofs) | G_INT(var_b->ofs));
				case OP_BITAND_I:
					optres_constantarithmatic++;
					return QCC_MakeIntConst(G_INT(var_a->ofs) & G_INT(var_b->ofs));
				case OP_MUL_I:
					optres_constantarithmatic++;
					return QCC_MakeIntConst(G_INT(var_a->ofs) * G_INT(var_b->ofs));
				case OP_DIV_I:
					optres_constantarithmatic++;
					if (G_INT(var_b->ofs) == 0)
					{
						QCC_PR_ParseWarning(WARN_CONSTANTCOMPARISON, "Division by constant 0");
						return QCC_MakeIntConst(0);
					}
					else
						return QCC_MakeIntConst(G_INT(var_a->ofs) / G_INT(var_b->ofs));
				case OP_ADD_I:
					optres_constantarithmatic++;
					return QCC_MakeIntConst(G_INT(var_a->ofs) + G_INT(var_b->ofs));
				case OP_SUB_I:
					optres_constantarithmatic++;
					return QCC_MakeIntConst(G_INT(var_a->ofs) - G_INT(var_b->ofs));

				case OP_ADD_IF:
					optres_constantarithmatic++;
					return QCC_MakeFloatConst(G_INT(var_a->ofs) + G_FLOAT(var_b->ofs));
				case OP_ADD_FI:
					optres_constantarithmatic++;
					return QCC_MakeFloatConst(G_FLOAT(var_a->ofs) + G_INT(var_b->ofs));

				case OP_AND_F:
					optres_constantarithmatic++;
					return QCC_MakeIntConst(G_INT(var_a->ofs) && G_INT(var_b->ofs));
				case OP_OR_F:
					optres_constantarithmatic++;
					return QCC_MakeIntConst(G_INT(var_a->ofs) || G_INT(var_b->ofs));
				case OP_MUL_V:	//mul_f is actually a dot-product
					optres_constantarithmatic++;
					return QCC_MakeFloatConst(	G_FLOAT(var_a->ofs) * G_FLOAT(var_b->ofs+0) +
												G_FLOAT(var_a->ofs) * G_FLOAT(var_b->ofs+1) +
												G_FLOAT(var_a->ofs) * G_FLOAT(var_b->ofs+2));
				case OP_MUL_FV:
					optres_constantarithmatic++;
					return QCC_MakeVectorConst(	G_FLOAT(var_a->ofs) * G_FLOAT(var_b->ofs+0),
												G_FLOAT(var_a->ofs) * G_FLOAT(var_b->ofs+1),
												G_FLOAT(var_a->ofs) * G_FLOAT(var_b->ofs+2));
				case OP_MUL_VF:
					optres_constantarithmatic++;
					return QCC_MakeVectorConst(	G_FLOAT(var_a->ofs+0) * G_FLOAT(var_b->ofs),
												G_FLOAT(var_a->ofs+1) * G_FLOAT(var_b->ofs),
												G_FLOAT(var_a->ofs+2) * G_FLOAT(var_b->ofs));
				case OP_ADD_V:
					optres_constantarithmatic++;
					return QCC_MakeVectorConst(	G_FLOAT(var_a->ofs+0) + G_FLOAT(var_b->ofs+0),
												G_FLOAT(var_a->ofs+1) + G_FLOAT(var_b->ofs+1),
												G_FLOAT(var_a->ofs+2) + G_FLOAT(var_b->ofs+2));
				case OP_SUB_V:
					optres_constantarithmatic++;
					return QCC_MakeVectorConst(	G_FLOAT(var_a->ofs+0) - G_FLOAT(var_b->ofs+0),
												G_FLOAT(var_a->ofs+1) - G_FLOAT(var_b->ofs+1),
												G_FLOAT(var_a->ofs+2) - G_FLOAT(var_b->ofs+2));
				}
			}
			else
			{
				//a is const, b is not
				switch (op - pr_opcodes)
				{
				case OP_CONV_FTOI:
					optres_constantarithmatic++;
					return QCC_MakeIntConst(G_FLOAT(var_a->ofs));
				case OP_CONV_ITOF:
					optres_constantarithmatic++;
					return QCC_MakeFloatConst(G_INT(var_a->ofs));
				case OP_BITOR_F:
				case OP_OR_F:
				case OP_ADD_F:
					if (G_FLOAT(var_a->ofs) == 0)
					{
						optres_constantarithmatic++;
						QCC_UnFreeTemp(var_b);
						return var_b;
					}
					break;
				case OP_MUL_F:
					if (G_FLOAT(var_a->ofs) == 1)
					{
						optres_constantarithmatic++;
						QCC_UnFreeTemp(var_b);
						return var_b;
					}
					break;
				case OP_BITAND_F:
				case OP_AND_F:
					if (G_FLOAT(var_a->ofs) != 0)
					{
						optres_constantarithmatic++;
						QCC_UnFreeTemp(var_b);
						return var_b;
					}
					break;

				case OP_BITOR_I:
				case OP_OR_I:
				case OP_ADD_I:
					if (G_INT(var_a->ofs) == 0)
					{
						optres_constantarithmatic++;
						QCC_UnFreeTemp(var_b);
						return var_b;
					}
					break;
				case OP_MUL_I:
					if (G_INT(var_a->ofs) == 1)
					{
						optres_constantarithmatic++;
						QCC_UnFreeTemp(var_b);
						return var_b;
					}
					break;
				case OP_BITAND_I:
				case OP_AND_I:
					if (G_INT(var_a->ofs) != 0)
					{
						optres_constantarithmatic++;
						QCC_UnFreeTemp(var_b);
						return var_b;
					}
					break;
				}
			}
		}
		else if (var_b && var_b->constant)
		{
			//b is const, a is not
			switch (op - pr_opcodes)
			{
			case OP_LOADA_F:
			case OP_LOADA_V:
			case OP_LOADA_S:
			case OP_LOADA_ENT:
			case OP_LOADA_FLD:
			case OP_LOADA_FNC:
			case OP_LOADA_I:
				{
					QCC_def_t *nd;
					nd = (void *)qccHunkAlloc (sizeof(QCC_def_t));
					memset (nd, 0, sizeof(QCC_def_t));
					nd->type = var_a->type;
					nd->ofs = var_a->ofs + G_INT(var_b->ofs);
					nd->temp = var_a->temp;
					nd->constant = false;
					nd->name = var_a->name;
					return nd;
				}
				break;
			case OP_BITOR_F:
			case OP_OR_F:
			case OP_SUB_F:
			case OP_ADD_F:
				if (G_FLOAT(var_b->ofs) == 0)
				{
					optres_constantarithmatic++;
					QCC_UnFreeTemp(var_a);
					return var_a;
				}
				break;
			case OP_DIV_F:
			case OP_MUL_F:
				if (G_FLOAT(var_b->ofs) == 1)
				{
					optres_constantarithmatic++;
					QCC_UnFreeTemp(var_a);
					return var_a;
				}
				break;
			//no bitand_f, I don't trust the casts
			case OP_AND_F:
				if (G_FLOAT(var_b->ofs) != 0)
				{
					optres_constantarithmatic++;
					QCC_UnFreeTemp(var_a);
					return var_a;
				}
				break;

			case OP_BITOR_I:
			case OP_OR_I:
			case OP_SUB_I:
			case OP_ADD_I:
				if (G_INT(var_b->ofs) == 0)
				{
					optres_constantarithmatic++;
					QCC_UnFreeTemp(var_a);
					return var_a;
				}
				break;
			case OP_DIV_I:
			case OP_MUL_I:
				if (G_INT(var_b->ofs) == 1)
				{
					optres_constantarithmatic++;
					QCC_UnFreeTemp(var_a);
					return var_a;
				}
				break;
			case OP_BITAND_I:
				if (G_INT(var_b->ofs) == 0xffffffff)
				{
					optres_constantarithmatic++;
					QCC_UnFreeTemp(var_a);
					return var_a;
				}
			case OP_AND_I:
				if (G_INT(var_b->ofs) == 0)
				{
					optres_constantarithmatic++;
					QCC_UnFreeTemp(var_a);
					return var_a;
				}
				break;
			}
		}
	}

	switch (op - pr_opcodes)
	{
	case OP_LOADA_F:
	case OP_LOADA_V:
	case OP_LOADA_S:
	case OP_LOADA_ENT:
	case OP_LOADA_FLD:
	case OP_LOADA_FNC:
	case OP_LOADA_I:
		break;
	case OP_AND_F:
		if (var_a->ofs == var_b->ofs)
			QCC_PR_ParseWarning(WARN_CONSTANTCOMPARISON, "Parameter offsets for && are the same");
		if (var_a->constant || var_b->constant)
			QCC_PR_ParseWarning(WARN_CONSTANTCOMPARISON, "Result of comparison is constant");
		break;
	case OP_OR_F:
		if (var_a->ofs == var_b->ofs)
			QCC_PR_ParseWarning(WARN_CONSTANTCOMPARISON, "Parameters for || are the same");
		if (var_a->constant || var_b->constant)
			QCC_PR_ParseWarning(WARN_CONSTANTCOMPARISON, "Result of comparison is constant");
		break;
	case OP_EQ_F:
	case OP_EQ_S:
	case OP_EQ_E:
	case OP_EQ_FNC:
//		if (opt_shortenifnots)
//			if (var_b->constant && ((int*)qcc_pr_globals)[var_b->ofs]==0)	// (a == 0) becomes (!a)
//				op = &pr_opcodes[(op - pr_opcodes) - OP_EQ_F + OP_NOT_F];
	case OP_EQ_V:

	case OP_NE_F:
	case OP_NE_V:
	case OP_NE_S:
	case OP_NE_E:
	case OP_NE_FNC:

	case OP_LE_F:
	case OP_GE_F:
	case OP_LT_F:
	case OP_GT_F:
		if ((var_a->constant && var_b->constant && !var_a->temp && !var_b->temp) || var_a->ofs == var_b->ofs)
			QCC_PR_ParseWarning(WARN_CONSTANTCOMPARISON, "Result of comparison is constant");
		break;
	case OP_IF_S:
	case OP_IFNOT_S:
	case OP_IF_F:
	case OP_IFNOT_F:
	case OP_IF_I:
	case OP_IFNOT_I:
//		if (var_a->type->type == ev_function && !var_a->temp)
//			QCC_PR_ParseWarning(WARN_CONSTANTCOMPARISON, "Result of comparison is constant");
		if (var_a->constant && !var_a->temp)
			QCC_PR_ParseWarning(WARN_CONSTANTCOMPARISON, "Result of comparison is constant");
		break;
	default:
		break;
	}

	if (numstatements)
	{	//optimise based on last statement.
		if (op - pr_opcodes == OP_IFNOT_I)
		{
			if (opt_shortenifnots && var_a && (statements[numstatements-1].op == OP_NOT_F || statements[numstatements-1].op == OP_NOT_FNC || statements[numstatements-1].op == OP_NOT_ENT))
			{
				if (statements[numstatements-1].c == var_a->ofs)
				{
					static QCC_def_t nvara;
					if (statements[numstatements-1].op == OP_NOT_F)
						op = &pr_opcodes[OP_IF_F];
					else
						op = &pr_opcodes[OP_IF_I];
					numstatements--;
					QCC_FreeTemp(var_a);
					memcpy(&nvara, var_a, sizeof(nvara));
					nvara.ofs = statements[numstatements].a;
					var_a = &nvara;

					optres_shortenifnots++;
				}
			}
		}
		else if (op - pr_opcodes == OP_IFNOT_F)
		{
			if (opt_shortenifnots && var_a && statements[numstatements-1].op == OP_NOT_F)
			{
				if (statements[numstatements-1].c == var_a->ofs)
				{
					static QCC_def_t nvara;
					op = &pr_opcodes[OP_IF_F];
					numstatements--;
					QCC_FreeTemp(var_a);
					memcpy(&nvara, var_a, sizeof(nvara));
					nvara.ofs = statements[numstatements].a;
					var_a = &nvara;

					optres_shortenifnots++;
				}
			}
		}
		else if (op - pr_opcodes == OP_IFNOT_S)
		{
			if (opt_shortenifnots && var_a && statements[numstatements-1].op == OP_NOT_S)
			{
				if (statements[numstatements-1].c == var_a->ofs)
				{
					static QCC_def_t nvara;
					op = &pr_opcodes[OP_IF_S];
					numstatements--;
					QCC_FreeTemp(var_a);
					memcpy(&nvara, var_a, sizeof(nvara));
					nvara.ofs = statements[numstatements].a;
					var_a = &nvara;

					optres_shortenifnots++;
				}
			}
		}
		else if (((unsigned) ((op - pr_opcodes) - OP_STORE_F) < 6) || (op-pr_opcodes) == OP_STORE_P)
		{
			// remove assignments if what should be assigned is the 3rd operand of the previous statement?
			// don't if it's a call, callH, switch or case
			// && var_a->ofs >RESERVED_OFS)
			if (OpAssignsToC(statements[numstatements-1].op) &&
			    opt_assignments && var_a && var_a->ofs == statements[numstatements-1].c)
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
		case OP_LOADA_STRUCT:
			/*emit this anyway. if it reaches runtime then you messed up.
			this is valid only if you do &foo[0]*/
//			QCC_PR_ParseWarning(0, "OP_LOADA_STRUCT: cannot emulate");
			break;

		case OP_ADD_I:
			{
				QCC_def_t *arg[2] = {var_a, var_b};
				numstatements--;
				def_parms[0].type = type_integer;
				def_parms[1].type = type_integer;
				var_c = QCC_PR_GenerateFunctionCall(QCC_PR_GetDef(type_function, "AddInt", NULL, true, 0, false), arg, 2);
				var_c->type = type_integer;
				return var_c;
			}
			break;
		case OP_SUB_I:
			{
				QCC_def_t *arg[2] = {var_a, var_b};
				numstatements--;
				def_parms[0].type = type_integer;
				def_parms[1].type = type_integer;
				var_c = QCC_PR_GenerateFunctionCall(QCC_PR_GetDef(type_function, "SubInt", NULL, true, 0, false), arg, 2);
				var_c->type = type_integer;
				return var_c;
			}
			break;
		case OP_MUL_I:
			{
				QCC_def_t *arg[2] = {var_a, var_b};
				numstatements--;
				def_parms[0].type = type_integer;
				def_parms[1].type = type_integer;
				var_c = QCC_PR_GenerateFunctionCall(QCC_PR_GetDef(type_function, "MulInt", NULL, true, 0, false), arg, 2);
				var_c->type = type_integer;
				return var_c;
			}
			break;
		case OP_DIV_I:
			{
				QCC_def_t *arg[2] = {var_a, var_b};
				numstatements--;
				def_parms[0].type = type_integer;
				def_parms[1].type = type_integer;
				var_c = QCC_PR_GenerateFunctionCall(QCC_PR_GetDef(type_function, "DivInt", NULL, true, 0, false), arg, 2);
				var_c->type = type_integer;
				return var_c;
			}
			break;

		case OP_CONV_ITOF:
		case OP_STORE_IF:
			op = pr_opcodes+OP_STORE_F;
			if (var_a->constant)
				var_a = QCC_MakeFloatConst(G_INT(var_a->ofs));
			else
			{
				numstatements--;
				def_parms[0].type = type_integer;
				var_a = QCC_PR_GenerateFunctionCall(QCC_PR_GetDef(type_function, "itof", NULL, true, 0, false), &var_a, 1);
				var_a->type = type_float;
				statement = &statements[numstatements];
				numstatements++;
			}
			break;
		case OP_CONV_FTOI:
		case OP_STORE_FI:
			op = pr_opcodes+OP_STORE_I;
			if (var_a->constant)
				var_a = QCC_MakeFloatConst(G_INT(var_a->ofs));
			else
			{
				numstatements--;
				def_parms[0].type = type_float;
				var_a = QCC_PR_GenerateFunctionCall(QCC_PR_GetDef(type_function, "ftoi", NULL, true, 0, false), &var_a, 1);
				var_a->type = type_integer;
				statement = &statements[numstatements];
				numstatements++;
			}
			break;
		case OP_STORE_I:
			op = pr_opcodes+OP_STORE_F;
			break;

		case OP_IF_S:
			var_c = QCC_PR_GetDef(type_string, "string_null", NULL, true, 0, false);
			numstatements--;
			var_a = QCC_PR_Statement(&pr_opcodes[OP_NE_S], var_a, var_c, NULL);
			statement = &statements[numstatements];
			numstatements++;

			QCC_FreeTemp(var_a);
			op = &pr_opcodes[OP_IF_I];
			break;

		case OP_IFNOT_S:
			var_c = QCC_PR_GetDef(type_string, "string_null", NULL, true, 0, false);
			numstatements--;
			var_a = QCC_PR_Statement(&pr_opcodes[OP_NE_S], var_a, var_c, NULL);
			statement = &statements[numstatements];
			numstatements++;

			QCC_FreeTemp(var_a);
			op = &pr_opcodes[OP_IFNOT_I];
			break;

		case OP_IF_F:
			var_c = QCC_MakeFloatConst(0);
			numstatements--;
			var_a = QCC_PR_Statement(&pr_opcodes[OP_NE_F], var_a, var_c, NULL);
			statement = &statements[numstatements];
			numstatements++;

			QCC_FreeTemp(var_a);
			op = &pr_opcodes[OP_IF_I];
			break;

		case OP_IFNOT_F:
			var_c = QCC_MakeFloatConst(0);
			numstatements--;
			var_a = QCC_PR_Statement(&pr_opcodes[OP_NE_F], var_a, var_c, NULL);
			statement = &statements[numstatements];
			numstatements++;

			QCC_FreeTemp(var_a);
			op = &pr_opcodes[OP_IFNOT_I];
			break;

		case OP_ADDSTORE_F:
			op = &pr_opcodes[OP_ADD_F];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;
		case OP_ADDSTORE_I:
			op = &pr_opcodes[OP_ADD_I];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;
		case OP_ADDSTORE_FI:
			op = &pr_opcodes[OP_ADD_FI];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;
		case OP_ADDSTORE_IF:
			op = &pr_opcodes[OP_ADD_IF];
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
		case OP_SUBSTORE_FI:
			op = &pr_opcodes[OP_SUB_FI];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;
		case OP_SUBSTORE_IF:
			op = &pr_opcodes[OP_SUB_IF];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;
		case OP_SUBSTORE_I:
			op = &pr_opcodes[OP_SUB_I];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;

		case OP_BINARYNOT_I:
			op = &pr_opcodes[OP_SUB_I];
			var_b = var_a;
			var_a = QCC_MakeIntConst(~0);
			break;
		case OP_BINARYNOT_F:
			op = &pr_opcodes[OP_SUB_F];
			var_b = var_a;
			var_a = QCC_MakeFloatConst(0xffffff);	//due to fpu precision, we only use 24 bits.
			break;

		case OP_DIVSTORE_F:
			op = &pr_opcodes[OP_DIV_F];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;
		case OP_DIVSTORE_FI:
			op = &pr_opcodes[OP_DIV_FI];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;
		case OP_DIVSTORE_IF:
			op = &pr_opcodes[OP_DIV_IF];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;
		case OP_DIVSTORE_I:
			op = &pr_opcodes[OP_DIV_I];
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
		case OP_MULSTORE_IF:
			op = &pr_opcodes[OP_MUL_IF];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;
		case OP_MULSTORE_FI:
			op = &pr_opcodes[OP_MUL_FI];
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

		case OP_MULSTORE_VF:
			op = &pr_opcodes[OP_MUL_VF];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;
		case OP_MULSTORE_VI:
			op = &pr_opcodes[OP_MUL_VI];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;

		case OP_BITSET_I:
			op = &pr_opcodes[OP_BITOR_I];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;
		case OP_BITSET:
			op = &pr_opcodes[OP_BITOR_F];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;

		case OP_STOREP_P:
			op = &pr_opcodes[OP_STOREP_I];
			break;

		case OP_BITCLR:
			//b = var, a = bit field.

			QCC_UnFreeTemp(var_a);
			QCC_UnFreeTemp(var_b);

			numstatements--;
			var_c = QCC_PR_Statement(&pr_opcodes[OP_BITAND_F], var_b, var_a, NULL);
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

		case OP_SUBSTOREP_FI:
		case OP_SUBSTOREP_IF:
		case OP_ADDSTOREP_FI:
		case OP_ADDSTOREP_IF:
		case OP_MULSTOREP_FI:
		case OP_MULSTOREP_IF:
		case OP_DIVSTOREP_FI:
		case OP_DIVSTOREP_IF:

		case OP_MULSTOREP_VF:
		case OP_MULSTOREP_VI:
		case OP_SUBSTOREP_V:
		case OP_ADDSTOREP_V:

		case OP_SUBSTOREP_F:
		case OP_SUBSTOREP_I:
		case OP_ADDSTOREP_I:
		case OP_ADDSTOREP_F:
		case OP_MULSTOREP_F:
		case OP_DIVSTOREP_F:
		case OP_BITSETP:
		case OP_BITSETP_I:
		case OP_BITCLRP:
//			QCC_PR_ParseWarning(0, "XSTOREP_F emulation is still experimental");
			QCC_UnFreeTemp(var_a);
			QCC_UnFreeTemp(var_b);
			//don't chain these... this expansion is not the same.
			{
				int st;
				int need_lock = false;
				if (var_b->temp)
				{
					for (st = numstatements-2; st>=0; st--)
					{
						if (statements[st].op == OP_ADDRESS)
							if (statements[st].c == var_b->ofs)
								break;

						if ((statements[st].op >= OP_CALL0 && statements[st].op <= OP_CALL8) || (statements[st].op >= OP_CALL1H && statements[st].op <= OP_CALL8H))
							need_lock = true;

						if (statements[st].c == var_b->ofs)
						{
							st = -1;
							break;
						}
					}
				}
				else
					st = -1;

				var_c = QCC_GetTemp(*op->type_c);
				if (st < 0)
				{
					/*generate new OP_LOADP instruction*/
					statement->op = ((*op->type_c)->type==ev_vector)?OP_LOADP_V:OP_LOADP_F;
					statement->a = var_b->ofs;
					statement->b = var_c->ofs;
					statement->c = 0;
				}
				else
				{
					/*it came from an OP_ADDRESS - st says the instruction*/
					if (need_lock)
					{
						QCC_ForceLockTempForOffset(statements[st].a);
						QCC_ForceLockTempForOffset(statements[st].b);
						QCC_LockTemp(var_c); /*that temp needs to be preserved over calls*/
					}

					/*generate new OP_ADDRESS instruction - FIXME: the arguments may have changed since the original instruction*/
					statement_linenums[statement-statements] = statement_linenums[st];
					statement->op = OP_ADDRESS;
					statement->a = statements[st].a;
					statement->b = statements[st].b;
					statement->c = statements[st].c;

					/*convert old one to an OP_LOAD*/
					statement_linenums[st] = pr_token_line_last;
					statements[st].op = ((*op->type_c)->type==ev_vector)?OP_LOAD_V:OP_LOAD_F;
					statements[st].a = statements[st].a;
					statements[st].b = statements[st].b;
					statements[st].c = var_c->ofs;
				}
			}

			statement = &statements[numstatements];
			numstatements++;

			statement_linenums[statement-statements] = pr_token_line_last;
			switch(op - pr_opcodes)
			{
			case OP_SUBSTOREP_V:
				statement->op = OP_SUB_V;
				break;
			case OP_ADDSTOREP_V:
				statement->op = OP_ADD_V;
				break;
			case OP_MULSTOREP_VF:
				statement->op = OP_MUL_VF;
				break;
			case OP_MULSTOREP_VI:
				statement->op = OP_MUL_VI;
				break;
			case OP_SUBSTOREP_F:
				statement->op = OP_SUB_F;
				break;
			case OP_SUBSTOREP_I:
				statement->op = OP_SUB_I;
				break;
			case OP_SUBSTOREP_IF:
				statement->op = OP_SUB_IF;
				break;
			case OP_SUBSTOREP_FI:
				statement->op = OP_SUB_FI;
				break;
			case OP_ADDSTOREP_IF:
				statement->op = OP_ADD_IF;
				break;
			case OP_ADDSTOREP_FI:
				statement->op = OP_ADD_FI;
				break;
			case OP_MULSTOREP_IF:
				statement->op = OP_MUL_IF;
				break;
			case OP_MULSTOREP_FI:
				statement->op = OP_MUL_FI;
				break;
			case OP_DIVSTOREP_IF:
				statement->op = OP_DIV_IF;
				break;
			case OP_DIVSTOREP_FI:
				statement->op = OP_DIV_FI;
				break;
			case OP_ADDSTOREP_F:
				statement->op = OP_ADD_F;
				break;
			case OP_ADDSTOREP_I:
				statement->op = OP_ADD_I;
				break;
			case OP_MULSTOREP_F:
				statement->op = OP_MUL_F;
				break;
			case OP_DIVSTOREP_F:
				statement->op = OP_DIV_F;
				break;
			case OP_BITSETP:
				statement->op = OP_BITOR_F;
				break;
			case OP_BITSETP_I:
				statement->op = OP_BITOR_I;
				break;
			case OP_BITCLRP:
				//float pointer float
				temp = QCC_GetTemp(type_float);
				statement->op = OP_BITAND_F;
				statement->a = var_c ? var_c->ofs : 0;
				statement->b = var_a ? var_a->ofs : 0;
				statement->c = temp->ofs;

				statement = &statements[numstatements];
				numstatements++;

				statement_linenums[statement-statements] = pr_token_line_last;
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

			op = &pr_opcodes[((*op->type_c)->type==ev_vector)?OP_STOREP_V:OP_STOREP_F];
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

	statement_linenums[statement-statements] = pr_token_line_last;
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

	if ((op - pr_opcodes >= OP_LOAD_F && op - pr_opcodes <= OP_LOAD_FNC) ||
		op - pr_opcodes == OP_LOAD_I)
	{
		if (var_b->constant == 2)
			var_c->constant = true;
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
QCC_dstatement_t *QCC_PR_SimpleStatement( int op, int var_a, int var_b, int var_c, int force)
{
	QCC_dstatement_t	*statement;

	if (!force && !QCC_OPCodeValid(pr_opcodes+op))
	{
		QCC_PR_ParseError(ERR_BADEXTENSION, "Opcode \"%s|%s\" not valid for target\n", pr_opcodes[op].name, pr_opcodes[op].opname);
	}

	statement_linenums[numstatements] = pr_token_line_last;
	statement = &statements[numstatements];

	numstatements++;
	statement->op = op;
	statement->a = var_a;
	statement->b = var_b;
	statement->c = var_c;
	return statement;
}

void QCC_PR_Statement3 ( QCC_opcode_t *op, QCC_def_t *var_a, QCC_def_t *var_b, QCC_def_t *var_c, int force)
{
	QCC_dstatement_t	*statement;

	if (!force && !QCC_OPCodeValid(op))
	{
//		outputversion = op->extension;
//		if (noextensions)
		QCC_PR_ParseError(ERR_BADEXTENSION, "Opcode \"%s|%s\" not valid for target\n", op->name, op->opname);
	}

	statement = &statements[numstatements];
	numstatements++;

	statement_linenums[statement-statements] = pr_token_line_last;
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
		cn = QCC_MakeFloatConst(pr_immediate._float);
		QCC_PR_Lex ();
		return cn;
	}
	if (pr_immediate_type == type_integer)
	{
		cn = QCC_MakeIntConst(pr_immediate._int);
		QCC_PR_Lex ();
		return cn;
	}

	if (pr_immediate_type == type_string)
	{
		char tmp[8192];
		strcpy(tmp, pr_immediate_string);

		for(;;)
		{
			QCC_PR_Lex ();
			if (pr_token_type == tt_immediate && pr_token_type == tt_immediate)
				strcat(tmp, pr_immediate_string);
			else
				break;
		} 

		cn = QCC_MakeStringConst(tmp);
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
	cn->ofs = QCC_GetFreeGlobalOffsetSpace(type_size[pr_immediate_type->type]);

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
	if (numsounds == QCC_MAX_SOUNDS)
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
	if (nummodels == QCC_MAX_MODELS)
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
	if (nummodels == QCC_MAX_MODELS)
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
	if (nummodels == QCC_MAX_MODELS)
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
	if (numfiles == QCC_MAX_FILES)
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
	if (numfiles == QCC_MAX_FILES)
		return;
//		QCC_Error ("PrecacheFile: numfiles == MAX_FILES");
	strcpy (precache_files[i], n);
	if (ch >= '1'  && ch <= '9')
		precache_files_block[i] = ch - '0';
	else
		precache_files_block[i] = 1;
	numfiles++;
}

QCC_def_t *QCC_PR_GenerateFunctionCall (QCC_def_t *func, QCC_def_t *arglist[], int argcount)	//warning, the func could have no name set if it's a field call.
{
	QCC_def_t		*d, *oldret, *oself;
	int			i;
	QCC_type_t		*t;
	int extraparms=false;
	int np;
	int laststatement = numstatements;

	int callconvention;
	QCC_dstatement_t *st;


	func->timescalled++;

	if (QCC_OPCodeValid(&pr_opcodes[OP_CALL1H]))
		callconvention = OP_CALL1H;	//FTE extended
	else
		callconvention = OP_CALL1;	//standard

	t = func->type;

	if (t->type == ev_variant)
	{
		t->aux_type = type_variant;
	}

	if (t->type != ev_function && t->type != ev_variant)
	{
		QCC_PR_ParseErrorPrintDef (ERR_NOTAFUNCTION, func, "not a function");
	}

// copy the arguments to the global parameter variables
	if (t->type == ev_variant)
	{
		extraparms = true;
		np = 0;
	}
	else if (t->num_parms < 0)
	{
		extraparms = true;
		np = (t->num_parms * -1) - 1;
	}
	else
		np = t->num_parms;

	if (strchr(func->name, ':') && laststatement && statements[laststatement-1].op == OP_LOAD_FNC && statements[laststatement-1].c == func->ofs)
	{	//we're entering OO code with a different self.
		//eg: other.touch(self)

		//FIXME: problems could occur with hexen2 calling conventions when parm0/1 is 'self'
		//thiscall. copy the right ent into 'self' (if it's not the same offset)
		d = QCC_PR_GetDef(type_entity, "self", NULL, true, 0, false);
		if (statements[laststatement-1].a != d->ofs)
		{
			oself = QCC_GetTemp(type_entity);
			//oself = self
			QCC_PR_SimpleStatement(OP_STORE_ENT, d->ofs, oself->ofs, 0, false);
			//self = other
			QCC_PR_SimpleStatement(OP_STORE_ENT, statements[laststatement-1].a, d->ofs, 0, false);

			//if the args refered to self, update them to refer to oself instead
			//(as self is now set to 'other')
			for (i = 0; i < argcount; i++)
			{
				if (arglist[i]->ofs == d->ofs)
				{
					arglist[i] = oself;
				}
			}
		}
		else
		{
			//it was self.func() anyway
			oself = NULL;
			d = NULL;
		}
	}
	else
	{	//regular func call
		oself = NULL;
		d = NULL;
	}

//	write the arguments (except for first two if hexenc)
	for (i = 0; i < argcount; i++)
	{
		if (i>=MAX_PARMS)
			d = extra_parms[i - MAX_PARMS];
		else
			d = &def_parms[i];

		if (callconvention == OP_CALL1H)
			if (i < 2)
			{
				//first two args are passed in the call opcode, so don't need to be copied
				arglist[i]->references++;
				d->references++;
				/*don't free these temps yet, free them after the return check*/
				continue;
			}

		if (arglist[i]->type->size == 3 || !opt_nonvec_parms)
			QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_STORE_V], arglist[i], d, (QCC_dstatement_t **)0xffffffff));
		else
		{
			d->type = arglist[i]->type;
			QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_STORE_F], arglist[i], d, (QCC_dstatement_t **)0xffffffff));
			optres_nonvec_parms++;
		}
	}

	//if the return value was in use, save it off now, so that it doesn't get clobbered
	if (def_ret.temp->used)
		oldret = QCC_GetTemp(def_ret.type);
	else
		oldret = NULL;

	/*can free temps used for arguments now*/
	if (callconvention == OP_CALL1H)
	{
		for (i = 0; i < argcount && i < 2; i++)
			QCC_FreeTemp(arglist[i]);
	}

	if (oldret && !def_ret.temp->used)
	{
		QCC_FreeTemp(oldret);
		oldret = NULL;
	}
	else if (def_ret.temp->used)
	{
		if (def_ret.type->size == 3)
			QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_V], &def_ret, oldret, (void*)0xffffffff));
		else
			QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_F], &def_ret, oldret, (void*)0xffffffff));
		QCC_UnFreeTemp(oldret);
		QCC_UnFreeTemp(&def_ret);
		QCC_PR_ParseWarning(WARN_FIXEDRETURNVALUECONFLICT, "Return value conflict - output is inefficient");
	}
	else
		oldret = NULL;

	//we dont need to lock the local containing the function index because its thrown away after the call anyway
	//(if a function is called in the argument list then it'll be locked as part of that call)
	QCC_FreeTemp(func);
	QCC_LockActiveTemps();	//any temps before are likly to be used with the return value.
	QCC_UnFreeTemp(func);

	//generate the call
	if (argcount>MAX_PARMS)
		QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[callconvention-1+MAX_PARMS], func, 0, (QCC_dstatement_t **)&st));
	else if (argcount)
		QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[callconvention-1+argcount], func, 0, (QCC_dstatement_t **)&st));
	else
		QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_CALL0], func, 0, (QCC_dstatement_t **)&st));

	if (callconvention == OP_CALL1H)
	{
		if (argcount)
		{
			st->b = arglist[0]->ofs;
//			QCC_FreeTemp(param[0]);
			if (argcount>1)
			{
				st->c = arglist[1]->ofs;
//				QCC_FreeTemp(param[1]);
			}
		}
	}

	//restore the class owner
	if (oself)
	{
		QCC_PR_SimpleStatement(OP_STORE_ENT, oself->ofs, d->ofs, 0, false);
		QCC_FreeTemp(oself);
	}

	if (oldret)
	{
		if (oldret->temp && !oldret->temp->used)
			QCC_PR_ParseWarning(0, "Ret was freed\n");

		//if we preserved the ofs_ret global, restore it here
		if (t->type == ev_variant)
		{
			d = QCC_GetTemp(type_variant);
			QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_STORE_F, &def_ret, d, (void*)0xffffffff));
		}
		else
		{
			d = QCC_GetTemp(t->aux_type);
			if (t->aux_type->size == 3)
				QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_STORE_V, &def_ret, d, (void*)0xffffffff));
			else
				QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_STORE_F, &def_ret, d, (void*)0xffffffff));
		}
		if (def_ret.type->size == 3)
			QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_STORE_V, oldret, &def_ret, (void*)0xffffffff));
		else
			QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_STORE_F, oldret, &def_ret, (void*)0xffffffff));
		QCC_FreeTemp(oldret);
		QCC_UnFreeTemp(&def_ret);
		QCC_UnFreeTemp(d);

		return d;
	}

	if (t->type == ev_variant)
		def_ret.type = type_variant;
	else
		def_ret.type = t->aux_type;
	if (def_ret.temp->used)
		QCC_PR_ParseWarning(WARN_FIXEDRETURNVALUECONFLICT, "Return value conflict - output is inefficient");
	def_ret.temp->used = true;

	return &def_ret;
}

/*
============
PR_ParseFunctionCall
============
*/
QCC_def_t *QCC_PR_ParseFunctionCall (QCC_def_t *func)	//warning, the func could have no name set if it's a field call.
{
	QCC_def_t		*e, *d, *old = {0}, *oself, *out; // warning: ‘old’ may be used uninitialized in this function
	int			arg;
	QCC_type_t		*t, *p;
	int extraparms=false;
	int np;

	int callconvention;

	QCC_def_t *param[MAX_PARMS+MAX_EXTRA_PARMS];

	func->timescalled++;

	if (QCC_OPCodeValid(&pr_opcodes[OP_CALL1H]))
		callconvention = OP_CALL1H;	//FTE extended
	else
		callconvention = OP_CALL1;	//standard

	t = func->type;

	if (t->type == ev_variant)
	{
		t->aux_type = type_variant;
	}

	if (t->type != ev_function && t->type != ev_variant)
	{
		QCC_PR_ParseErrorPrintDef (ERR_NOTAFUNCTION, func, "not a function");
	}

	if (!t->num_parms&&t->type != ev_variant)	//intrinsics. These base functions have variable arguments. I would check for (...) args too, but that might be used for extended builtin functionality. (this code wouldn't compile otherwise)
	{
		if (!strcmp(func->name, "sizeof"))
		{
			QCC_type_t *t;
			if (!func->initialized)
				func->initialized = 3;
			func->references++;
			t = QCC_PR_ParseType(false, true);
			if (t)
			{
				QCC_PR_Expect(")");
				return QCC_MakeIntConst(t->size * 4);
			}
			else
			{
				int sz;
				int oldstcount = numstatements;
				e = QCC_PR_Expression (TOP_PRIORITY, EXPR_DISALLOW_COMMA);
				//the term should not have side effects, or generate any actual statements.
				numstatements = oldstcount;
				QCC_PR_Expect(")");
				if (!e)
					QCC_PR_ParseErrorPrintDef (ERR_NOTAFUNCTION, func, "sizeof term not supported");
				if (!e->arraysize)
					sz = 1;
				else
					sz = e->arraysize;
				sz *= e->type->size;
				sz *= 4;
				QCC_FreeTemp(e);
				return QCC_MakeIntConst(sz);
			}
		}
		if (!strcmp(func->name, "_"))
		{
			if (!func->initialized)
				func->initialized = 3;
			func->references++;
			if (pr_token_type == tt_immediate && pr_immediate_type->type == ev_string)
			{
				d = QCC_MakeTranslateStringConst(pr_immediate_string);
				QCC_PR_Lex();
			}
			else
				QCC_PR_ParseErrorPrintDef (ERR_TYPEMISMATCHPARM, func, "_() intrinsic accepts only a string immediate", 1);
			QCC_PR_Expect(")");
			return d;
		}
		if (!strcmp(func->name, "random"))
		{
			old = NULL;
			if (!func->initialized)
				func->initialized = 3;
			func->references++;
			if (!QCC_PR_CheckToken(")"))
			{
				e = QCC_PR_Expression (TOP_PRIORITY, EXPR_DISALLOW_COMMA);
				if (e->type->type != ev_float)
					QCC_PR_ParseErrorPrintDef (ERR_TYPEMISMATCHPARM, func, "type mismatch on parm %i", 1);
				if (!QCC_PR_CheckToken(")"))
				{
					QCC_PR_Expect(",");
					d = QCC_PR_Expression (TOP_PRIORITY, EXPR_DISALLOW_COMMA);
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

			out = &def_ret;
			if (QCC_OPCodeValid(&pr_opcodes[OP_RAND0]))
			{
				if(qcc_targetformat != QCF_HEXEN2)
					out = QCC_GetTemp(type_float);
				else if (out->temp->used)
				{
					old = QCC_GetTemp(out->type);
					if (def_ret.type->size == 3)
						QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_V], out, old, NULL));
					else
						QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_F], out, old, NULL));
					QCC_UnFreeTemp(old);
					QCC_UnFreeTemp(out);
					QCC_PR_ParseWarning(WARN_FIXEDRETURNVALUECONFLICT, "Return value conflict - output is inefficient");
				}
				else
					old = NULL;

				if (e)
				{
					if (d)
						QCC_PR_SimpleStatement(OP_RAND2, e->ofs, d->ofs, out->ofs, false);
					else
						QCC_PR_SimpleStatement(OP_RAND1, e->ofs, 0, out->ofs, false);
				}
				else
					QCC_PR_SimpleStatement(OP_RAND0, 0, 0, out->ofs, false);
			}
			else
			{
				if (out->temp->used)
				{
					old = QCC_GetTemp(out->type);
					if (def_ret.type->size == 3)
						QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_V], out, old, NULL));
					else
						QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_F], out, old, NULL));
					QCC_UnFreeTemp(old);
					QCC_UnFreeTemp(out);
					QCC_PR_ParseWarning(WARN_FIXEDRETURNVALUECONFLICT, "Return value conflict - output is inefficient");
				}
				else
					old = NULL;

				if (e)
				{
					if (d)
					{
						QCC_dstatement_t *st;
						QCC_def_t *t;
						QCC_PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0, false);

						if ((!d->constant || !e->constant) && G_FLOAT(d->ofs) >= G_FLOAT(d->ofs))
						{
							t = QCC_PR_Statement(&pr_opcodes[OP_GT_F], d, e, NULL);
							QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_IFNOT_I], t, 0, &st));
							st->b = 3;

							t = QCC_PR_Statement(&pr_opcodes[OP_SUB_F], d, e, NULL);
							QCC_PR_SimpleStatement(OP_MUL_F, OFS_RETURN, t->ofs, OFS_RETURN, false);
							QCC_FreeTemp(t);
							QCC_PR_SimpleStatement(OP_ADD_F, OFS_RETURN, e->ofs, OFS_RETURN, false);

							QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_GOTO], 0, 0, &st));
							st->a = 3;
						}

						t = QCC_PR_Statement(&pr_opcodes[OP_SUB_F], e, d, NULL);
						QCC_PR_SimpleStatement(OP_MUL_F, OFS_RETURN, t->ofs, OFS_RETURN, false);
						QCC_FreeTemp(t);
						QCC_PR_SimpleStatement(OP_ADD_F, OFS_RETURN, d->ofs, OFS_RETURN, false);
					}
					else
					{
						QCC_PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0, false);
						QCC_PR_SimpleStatement(OP_MUL_F, OFS_RETURN, e->ofs, OFS_RETURN, false);
					}
				}
				else
					QCC_PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0, false);
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
				QCC_UnFreeTemp(d);
				QCC_UnFreeTemp(&def_ret);

				return d;
			}

			if (out == &def_ret)
			{
				if (out->temp->used)
					QCC_PR_ParseWarning(0, "Return value conflict - output is likly to be invalid");
				out->temp->used = true;
				out->type = type_float;
			}
			return out;

		}
		if (!strcmp(func->name, "randomv"))
		{
			out = NULL;
			if (!func->initialized)
				func->initialized = 3;
			func->references++;
			if (!QCC_PR_CheckToken(")"))
			{
				e = QCC_PR_Expression (TOP_PRIORITY, EXPR_DISALLOW_COMMA);
				if (e->type->type != ev_vector)
						QCC_PR_ParseErrorPrintDef (ERR_TYPEMISMATCHPARM, func, "type mismatch on parm %i", 1);
				if (!QCC_PR_CheckToken(")"))
				{
					QCC_PR_Expect(",");
					d = QCC_PR_Expression (TOP_PRIORITY, EXPR_DISALLOW_COMMA);
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

			if (QCC_OPCodeValid(&pr_opcodes[OP_RANDV0]))
			{
				if(def_ret.temp->used)
					out = QCC_GetTemp(type_vector);
				else
					out = &def_ret;

				if (e)
				{
					if (d)
						QCC_PR_SimpleStatement(OP_RANDV2, e->ofs, d->ofs, out->ofs, false);
					else
						QCC_PR_SimpleStatement(OP_RANDV1, e->ofs, 0, out->ofs, false);
				}
				else
					QCC_PR_SimpleStatement(OP_RANDV0, 0, 0, out->ofs, false);
			}
			else
			{
				if (def_ret.temp->used)
				{
					old = QCC_GetTemp(def_ret.type);
					if (def_ret.type->size == 3)
						QCC_PR_Statement(&pr_opcodes[OP_STORE_V], &def_ret, old, NULL);
					else
						QCC_PR_Statement(&pr_opcodes[OP_STORE_F], &def_ret, old, NULL);
					QCC_UnFreeTemp(old);
					QCC_UnFreeTemp(&def_ret);
					QCC_PR_ParseWarning(WARN_FIXEDRETURNVALUECONFLICT, "Return value conflict - output is inefficient");
				}
				else
					old = NULL;

				if (e)
				{
					if (d)
					{
						QCC_def_t *t;
						QCC_PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0, false);

						if ((!d->constant || !e->constant) && G_FLOAT(d->ofs) >= G_FLOAT(d->ofs))
						{
							t = QCC_GetTemp(type_float);
							QCC_PR_SimpleStatement(OP_GT_F, d->ofs+2, e->ofs+2, t->ofs, false);
							QCC_PR_SimpleStatement(OP_IFNOT_I, t->ofs, 3, 0, false);

							QCC_PR_SimpleStatement(OP_SUB_F, d->ofs+2, e->ofs+2, t->ofs, false);
							QCC_PR_SimpleStatement(OP_MUL_F, OFS_RETURN, t->ofs, OFS_RETURN+2, false);
							QCC_FreeTemp(t);
							QCC_PR_SimpleStatement(OP_ADD_F, OFS_RETURN, e->ofs+2, OFS_RETURN+2, false);

							QCC_PR_SimpleStatement(OP_GOTO, 3, 0, 0, false);
						}

						t = QCC_GetTemp(type_float);
						QCC_PR_SimpleStatement(OP_SUB_F, d->ofs+2, e->ofs+2, t->ofs, false);
						QCC_PR_SimpleStatement(OP_MUL_F, OFS_RETURN, t->ofs, OFS_RETURN+2, false);
						QCC_FreeTemp(t);
						QCC_PR_SimpleStatement(OP_ADD_F, OFS_RETURN, d->ofs+2, OFS_RETURN+2, false);



						QCC_PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0, false);

						if ((!d->constant || !e->constant) && G_FLOAT(d->ofs) >= G_FLOAT(d->ofs))
						{
							t = QCC_GetTemp(type_float);
							QCC_PR_SimpleStatement(OP_GT_F, d->ofs+1, e->ofs+1, t->ofs, false);
							QCC_PR_SimpleStatement(OP_IFNOT_I, t->ofs, 3, 0, false);

							QCC_PR_SimpleStatement(OP_SUB_F, d->ofs+1, e->ofs+1, t->ofs, false);
							QCC_PR_SimpleStatement(OP_MUL_F, OFS_RETURN, t->ofs, OFS_RETURN+1, false);
							QCC_FreeTemp(t);
							QCC_PR_SimpleStatement(OP_ADD_F, OFS_RETURN, e->ofs+1, OFS_RETURN+1, false);

							QCC_PR_SimpleStatement(OP_GOTO, 3, 0, 0, false);
						}

						t = QCC_GetTemp(type_float);
						QCC_PR_SimpleStatement(OP_SUB_F, d->ofs+1, e->ofs+1, t->ofs, false);
						QCC_PR_SimpleStatement(OP_MUL_F, OFS_RETURN, t->ofs, OFS_RETURN+1, false);
						QCC_FreeTemp(t);
						QCC_PR_SimpleStatement(OP_ADD_F, OFS_RETURN, d->ofs+1, OFS_RETURN+1, false);


						QCC_PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0, false);

						if ((!d->constant || !e->constant) && G_FLOAT(d->ofs) >= G_FLOAT(d->ofs))
						{
							t = QCC_GetTemp(type_float);
							QCC_PR_SimpleStatement(OP_GT_F, d->ofs, e->ofs, t->ofs, false);
							QCC_PR_SimpleStatement(OP_IFNOT_I, t->ofs, 3, 0, false);

							QCC_PR_SimpleStatement(OP_SUB_F, d->ofs, e->ofs, t->ofs, false);
							QCC_PR_SimpleStatement(OP_MUL_F, OFS_RETURN, t->ofs, OFS_RETURN, false);
							QCC_FreeTemp(t);
							QCC_PR_SimpleStatement(OP_ADD_F, OFS_RETURN, e->ofs, OFS_RETURN, false);

							QCC_PR_SimpleStatement(OP_GOTO, 3, 0, 0, false);
						}

						t = QCC_GetTemp(type_float);
						QCC_PR_SimpleStatement(OP_SUB_F, d->ofs, e->ofs, t->ofs, false);
						QCC_PR_SimpleStatement(OP_MUL_F, OFS_RETURN, t->ofs, OFS_RETURN, false);
						QCC_FreeTemp(t);
						QCC_PR_SimpleStatement(OP_ADD_F, OFS_RETURN, d->ofs, OFS_RETURN, false);
					}
					else
					{
						QCC_PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0, false);
						QCC_PR_SimpleStatement(OP_MUL_F, OFS_RETURN, e->ofs, OFS_RETURN+2, false);
						QCC_PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0, false);
						QCC_PR_SimpleStatement(OP_MUL_F, OFS_RETURN, e->ofs, OFS_RETURN+1, false);
						QCC_PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0, false);
						QCC_PR_SimpleStatement(OP_MUL_F, OFS_RETURN, e->ofs, OFS_RETURN, false);
					}
				}
				else
				{
					QCC_PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0, false);
					QCC_PR_SimpleStatement(OP_STORE_F, OFS_RETURN, OFS_RETURN+2, 0, false);
					QCC_PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0, false);
					QCC_PR_SimpleStatement(OP_STORE_F, OFS_RETURN, OFS_RETURN+1, 0, false);
					QCC_PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0, false);
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
				QCC_UnFreeTemp(d);
				QCC_UnFreeTemp(&def_ret);

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
			if (QCC_PR_CheckToken(")"))
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
			{
				old = QCC_GetTemp(def_ret.type);
				if (def_ret.type->size == 3)
					QCC_PR_Statement(&pr_opcodes[OP_STORE_V], &def_ret, old, NULL);
				else
					QCC_PR_Statement(&pr_opcodes[OP_STORE_F], &def_ret, old, NULL);
				QCC_UnFreeTemp(old);
				QCC_UnFreeTemp(&def_ret);
				QCC_PR_ParseWarning(WARN_FIXEDRETURNVALUECONFLICT, "Return value conflict - output is inefficient");
			}
			else
				old = NULL;
			/*
			if (def_ret.temp->used)
				QCC_PR_ParseWarning(0, "Return value conflict - output is likly to be invalid");
			def_ret.temp->used = true;
			*/

			if (rettype != type_entity)
			{
				char genfunc[2048];
				sprintf(genfunc, "Class*%s", rettype->name);
				func = QCC_PR_GetDef(type_function, genfunc, NULL, true, 0, false);
				func->references++;
			}
			QCC_PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0, false);

			if (old)
			{
				d = QCC_GetTemp(rettype);
				QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_STORE_ENT, &def_ret, d, NULL));
				if (def_ret.type->size == 3)
					QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_STORE_V, old, &def_ret, NULL));
				else
					QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_STORE_F, old, &def_ret, NULL));
				QCC_FreeTemp(old);
				QCC_UnFreeTemp(d);
				QCC_UnFreeTemp(&def_ret);

				return d;
			}

			def_ret.type = rettype;
			return &def_ret;
		}
		else if (!strcmp(func->name, "entnum") && !QCC_PR_CheckToken(")"))
		{
			//t = (a/%1) / (nextent(world)/%1)
			//a/%1 does a (int)entity to float conversion type thing
			if (!func->initialized)
				func->initialized = 3;
			func->references++;

			e = QCC_PR_Expression(TOP_PRIORITY, EXPR_DISALLOW_COMMA);
			QCC_PR_Expect(")");
			e = QCC_PR_Statement(&pr_opcodes[OP_DIV_F], e, QCC_MakeIntConst(1), (QCC_dstatement_t **)0xffffffff);

			d = QCC_PR_GetDef(NULL, "nextent", NULL, false, 0, false);
			if (!d)
				QCC_PR_ParseError(0, "the nextent builtin is not defined");
			QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_F], e, &def_parms[0], (QCC_dstatement_t **)0xffffffff));
			d = QCC_PR_Statement(&pr_opcodes[OP_CALL0], d, NULL, NULL);
			d = QCC_PR_Statement(&pr_opcodes[OP_DIV_F], d, QCC_MakeIntConst(1), (QCC_dstatement_t **)0xffffffff);

			e = QCC_PR_Statement(&pr_opcodes[OP_DIV_F], e, d, (QCC_dstatement_t **)0xffffffff);

			return e;
		}
	}	//so it's not an intrinsic.

	if (opt_precache_file)	//should we strip out all precache_file calls?
	{
		if (!strncmp(func->name,"precache_file", 13))
		{
			if (pr_token_type == tt_immediate && pr_immediate_type->type == ev_string && pr_scope && !strcmp(pr_scope->name, "main"))
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

// copy the arguments to the global parameter variables
	arg = 0;
	if (t->type == ev_variant)
	{
		extraparms = true;
		np = 0;
	}
	else if (t->num_parms < 0)
	{
		extraparms = true;
		np = (t->num_parms * -1) - 1;
	}
	else
		np = t->num_parms;

	//any temps referenced to build the parameters don't need to be locked.
	if (!QCC_PR_CheckToken(")"))
	{
		p = t->param;
		do
		{
			if (extraparms && arg >= MAX_PARMS)
				QCC_PR_ParseErrorPrintDef (ERR_TOOMANYPARAMETERSVARARGS, func, "More than %i parameters on varargs function", MAX_PARMS);
			else if (arg >= MAX_PARMS+MAX_EXTRA_PARMS)
				QCC_PR_ParseErrorPrintDef (ERR_TOOMANYTOTALPARAMETERS, func, "More than %i parameters", MAX_PARMS+MAX_EXTRA_PARMS);
			if (!extraparms && arg >= t->num_parms && !p)
			{
				QCC_PR_ParseWarning (WARN_TOOMANYPARAMETERSFORFUNC, "too many parameters");
				QCC_PR_ParsePrintDef(WARN_TOOMANYPARAMETERSFORFUNC, func);
			}


			//with vectorcalls, we store the vector into the args as individual floats
			//this allows better reuse of vector constants.
			//copy it into the offset now, because we can.
			if (opt_vectorcalls && pr_token_type == tt_immediate && pr_immediate_type == type_vector && arg < MAX_PARMS && (!def_parms[arg].temp || !def_parms[arg].temp->used))
			{
				e = &def_parms[arg];

				e->ofs = OFS_PARM0+0;
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_STORE_F], QCC_MakeFloatConst(pr_immediate.vector[0]), e, (QCC_dstatement_t **)0xffffffff));
				e->ofs = OFS_PARM0+1;
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_STORE_F], QCC_MakeFloatConst(pr_immediate.vector[1]), e, (QCC_dstatement_t **)0xffffffff));
				e->ofs = OFS_PARM0+2;
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_STORE_F], QCC_MakeFloatConst(pr_immediate.vector[2]), e, (QCC_dstatement_t **)0xffffffff));
				e->ofs = OFS_PARM0;
				e->type = type_vector;

				QCC_PR_Lex();
			}
			else
				e = QCC_PR_Expression (TOP_PRIORITY, EXPR_DISALLOW_COMMA);

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
					d->ofs = QCC_GetFreeGlobalOffsetSpace (3);
					extra_parms[arg - MAX_PARMS] = d;
				}
				d = extra_parms[arg - MAX_PARMS];
			}
			else
				d = &def_parms[arg];

			if (pr_classtype && e->type->type == ev_field && p->type != ev_field)
			{	//convert.
				oself = QCC_PR_GetDef(type_entity, "self", NULL, true, 0, false);
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
					else if ((p->type == ev_function || p->type == ev_field || p->type == ev_string || p->type == ev_pointer) && e->type->type == ev_integer && e->constant && !((int*)qcc_pr_globals)[e->ofs])
					{	//you're allowed to use int 0 to pass a null function/field/string/pointer
						//this is basically because __NULL__ is defined as 0i (int 0)
						//note that we don't allow passing 0.0f for null.
						//WARNING: field 0 is actually a valid field, and is commonly modelindex.
					}
					else if ((p->type == ev_vector) && e->type->type == ev_integer && e->constant && !((int*)qcc_pr_globals)[e->ofs])
					{
						//also allow it for vector types too, but make sure the entire vector is valid.
						e = QCC_MakeVectorConst(0, 0, 0);
					}
					else if (p->type != ev_variant && e->type->type != ev_variant)	//can cast to variant whatever happens
					{
						QCC_type_t *inh;
						for (inh = e->type->parentclass; inh; inh = inh->parentclass)
						{
							if (typecmp(inh, p))
								break;
						}
						if (!inh)
						{
							if (flag_laxcasts || (p->type == ev_function && e->type->type == ev_function))
							{
								QCC_PR_ParseWarning(WARN_LAXCAST, "type mismatch on parm %i - (%s should be %s)", arg+1, TypeName(e->type), TypeName(p));
								QCC_PR_ParsePrintDef(WARN_LAXCAST, func);
							}
							else
								QCC_PR_ParseErrorPrintDef (ERR_TYPEMISMATCHPARM, func, "type mismatch on parm %i - (%s should be %s)", arg+1, TypeName(e->type), TypeName(p));
						}
					}
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
		} while (QCC_PR_CheckToken (","));

		if (t->num_parms != -1 && arg < np)
			QCC_PR_ParseWarning (WARN_TOOFEWPARAMS, "too few parameters on call to %s", func->name);
		QCC_PR_Expect (")");
	}
	else if (np)
	{
		QCC_PR_ParseWarning (WARN_TOOFEWPARAMS, "%s: Too few parameters", func->name);
		QCC_PR_ParsePrintDef (WARN_TOOFEWPARAMS, func);
	}

	return QCC_PR_GenerateFunctionCall(func, param, arg);
}

int constchecks;
int varchecks;
int typechecks;
QCC_def_t *QCC_MakeIntConst(int value)
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
	cn->arraysize = 0;

	if (!value)
		G_INT(cn->ofs) = 0;
	else
	{
	// copy the immediate to the global area
		cn->ofs = QCC_GetFreeGlobalOffsetSpace (type_size[type_integer->type]);

		G_INT(cn->ofs) = value;
	}


	return cn;
}

QCC_def_t *QCC_MakeVectorConst(float a, float b, float c)
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
		if (cn->type != type_vector)
			continue;
		typechecks++;

		if ( G_FLOAT(cn->ofs+0) == a &&
			G_FLOAT(cn->ofs+1) == b &&
			G_FLOAT(cn->ofs+2) == c)
		{
			return cn;
		}
	}

// allocate a new one
	cn = (void *)qccHunkAlloc (sizeof(QCC_def_t));
	cn->next = NULL;
	pr.def_tail->next = cn;
	pr.def_tail = cn;

	cn->type = type_vector;
	cn->name = "IMMEDIATE";
	cn->constant = true;
	cn->initialized = 1;
	cn->scope = NULL;		// always share immediates
	cn->arraysize = 0;

// copy the immediate to the global area
	cn->ofs = QCC_GetFreeGlobalOffsetSpace (type_size[type_vector->type]);

	G_FLOAT(cn->ofs+0) = a;
	G_FLOAT(cn->ofs+1) = b;
	G_FLOAT(cn->ofs+2) = c;

	return cn;
}

extern hashtable_t floatconstdefstable;
QCC_def_t *QCC_MakeFloatConst(float value)
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

	cn->s_file = s_file;
	cn->s_line = pr_source_line;
	cn->type = type_float;
	cn->name = "IMMEDIATE";
	cn->constant = true;
	cn->initialized = 1;
	cn->scope = NULL;		// always share immediates
	cn->arraysize = 0;

// copy the immediate to the global area
	cn->ofs = QCC_GetFreeGlobalOffsetSpace (type_size[type_integer->type]);

	Hash_AddKey(&floatconstdefstable, fi.i, cn, qccHunkAlloc(sizeof(bucket_t)));

	G_FLOAT(cn->ofs) = value;


	return cn;
}

extern hashtable_t stringconstdefstable, stringconstdefstable_trans;
int dotranslate_count;
static QCC_def_t *QCC_MakeStringConstInternal(char *value, pbool translate)
{
	QCC_def_t	*cn;
	int string;

	cn = pHash_Get(translate?&stringconstdefstable_trans:&stringconstdefstable, value);
	if (cn)
		return cn;

// allocate a new one
	if(translate)
	{
		char buf[64];
		sprintf(buf, "dotranslate_%i", ++dotranslate_count);
		cn = (void *)qccHunkAlloc (sizeof(QCC_def_t) + strlen(buf)+1);
		cn->name = (char*)(cn+1);
		strcpy(cn->name, buf);
	}
	else
	{
		cn = (void *)qccHunkAlloc (sizeof(QCC_def_t));
		cn->name = "IMMEDIATE";
	}
	cn->next = NULL;
	pr.def_tail->next = cn;
	pr.def_tail = cn;

	cn->type = type_string;
	cn->constant = true;
	cn->initialized = 1;
	cn->scope = NULL;		// always share immediates
	cn->arraysize = 0;

// copy the immediate to the global area
	cn->ofs = QCC_GetFreeGlobalOffsetSpace (type_size[type_integer->type]);

	string = QCC_CopyString (value);

	pHash_Add(translate?&stringconstdefstable_trans:&stringconstdefstable, strings+string, cn, qccHunkAlloc(sizeof(bucket_t)));

	G_INT(cn->ofs) = string;

	return cn;
}

QCC_def_t *QCC_MakeStringConst(char *value)
{
	return QCC_MakeStringConstInternal(value, false);
}
QCC_def_t *QCC_MakeTranslateStringConst(char *value)
{
	return QCC_MakeStringConstInternal(value, true);
}

QCC_type_t *QCC_PointerTypeTo(QCC_type_t *type)
{
	QCC_type_t *newtype;
	newtype = QCC_PR_NewType("ptr", ev_pointer, false);
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
		def = QCC_PR_GetDef(NULL, name, NULL, 0, 0, false);
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
		def = QCC_PR_GetDef(NULL, membername, NULL, false, 0, false);
		return def;
	}

	return QCC_MemberInParentClass(name, clas->parentclass);
}

//create fields for the types, instanciate the members to the fields.
//we retouch the parents each time to guarentee polymorphism works.
//FIXME: virtual methods will not work properly. Need to trace down to see if a parent already defined it
void QCC_PR_EmitFieldsForMembers(QCC_type_t *clas)
{
//we created fields for each class when we defined the actual classes.
//we need to go through each member and match it to the offset of it's parent class, if overloaded, or create a new field if not..

//basictypefield is cleared before we do this
//we emit the parent's fields first (every time), thus ensuring that we don't reuse parent fields on a child class.

	char membername[2048];
	int p, np, a;
	unsigned int o;
	QCC_type_t *mt, *ft;
	QCC_def_t *f, *m;
	if (clas->parentclass != type_entity)	//parents MUST have all their fields set or inheritance would go crazy.
		QCC_PR_EmitFieldsForMembers(clas->parentclass);

	np = clas->num_parms;
	mt = clas->param;
	for (p = 0; p < np; p++, mt = mt->next)
	{
		sprintf(membername, "%s::"MEMBERFIELDNAME, clas->name, mt->name);
		m = QCC_PR_GetDef(NULL, membername, NULL, false, 0, false);

		f = QCC_MemberInParentClass(mt->name, clas->parentclass);
		if (f)
		{
			if (m->arraysize)
				QCC_Error(ERR_INTERNAL, "FTEQCC does not support overloaded arrays of members");
			a=0;
			for (o = 0; o < m->type->size; o++)
				((int *)qcc_pr_globals)[o+a*mt->size+m->ofs] = ((int *)qcc_pr_globals)[o+f->ofs];
			continue;
		}

		for (a = 0; a < (m->arraysize?m->arraysize:1); a++)
		{
			/*if it was already set, don't go recursive and generate 500 fields for a one-member class that was inheritted from 500 times*/
			if (((int *)qcc_pr_globals)[0+a*mt->size+m->ofs])
				continue;

			//we need the type in here so saved games can still work without saving ints as floats. (would be evil)
			ft = QCC_PR_NewType(basictypenames[mt->type], ev_field, false);
			ft->aux_type = QCC_PR_NewType(basictypenames[mt->type], mt->type, false);
			ft->aux_type->aux_type = type_void;
			ft->size = ft->aux_type->size;
			ft = QCC_PR_FindType(ft);
			sprintf(membername, "__f_%s_%i", ft->name, ++basictypefield[mt->type]);
			f = QCC_PR_GetDef(ft, membername, NULL, true, 0, true);

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
			if (QCC_PR_GetDef(NULL, membername, NULL, false, 0, false))
				break;	//a child class overrides.
		}
		if (oc != clas)
			continue;

		if (type->type == ev_function)	//FIXME: inheritance will not install all the member functions.
		{
			sprintf(membername, "%s::"MEMBERFIELDNAME, clas->name, type->name);
			member = QCC_PR_GetDef(NULL, membername, NULL, false, 0, false);
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
			virt = QCC_PR_GetDef(type, membername, NULL, false, 0, false);
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

	if (numfunctions >= MAX_FUNCTIONS)
		QCC_Error(ERR_INTERNAL, "Too many function defs");

	pr_scope = NULL;
	memset(basictypefield, 0, sizeof(basictypefield));
	QCC_PR_EmitFieldsForMembers(basetype);




	pr_scope = scope;

	df = &functions[numfunctions];
	numfunctions++;

	df->s_file = 0;
	df->s_name = 0;
	df->first_statement = numstatements;
	df->parm_size[0] = 1;
	df->numparms = 0;
	locals_start = locals_end = FIRST_LOCAL;

	G_FUNCTION(scope->ofs) = df - functions;

	//locals here...
	ed = QCC_PR_GetDef(type_entity, "ent", pr_scope, true, 0, false);

	virt = QCC_PR_GetDef(type_function, "spawn", NULL, false, 0, false);
	if (!virt)
		QCC_Error(ERR_INTERNAL, "spawn function was not defined\n");
	QCC_PR_SimpleStatement(OP_CALL0, virt->ofs, 0, 0, false);	//calling convention doesn't come into it.

	QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_ENT], &def_ret, ed, NULL));

	ed->references = 1;	//there may be no functions.


	QCC_PR_EmitClassFunctionTable(basetype, basetype, ed, &constructor);

	if (constructor)
	{	//self = ent;
		self = QCC_PR_GetDef(type_entity, "self", NULL, false, 0, false);
		oself = QCC_PR_GetDef(type_entity, "oself", scope, true, 0, false);
		QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_ENT], self, oself, NULL));
		QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_ENT], ed, self, NULL));	//return to our old self. boom boom.
		QCC_PR_SimpleStatement(OP_CALL0, constructor->ofs, 0, 0, false);
		QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_ENT], oself, self, NULL));
	}

	QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_RETURN], ed, NULL, NULL));	//apparently we do actually have to return something. *sigh*...
	QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_DONE], NULL, NULL, NULL));



	QCC_WriteAsmFunction(scope, df->first_statement, df->parm_start);

	QCC_Marshal_Locals(df->first_statement, numstatements);
	df->parm_start = locals_start;
	df->numparms = locals_end - locals_start;
}

QCC_def_t *QCC_PR_ParseArrayPointer (QCC_def_t *d, pbool allowarrayassign)
{
	QCC_type_t *t;
	QCC_def_t *idx;
	QCC_def_t *tmp;
	QCC_dstatement_t *st;
	pbool allowarray;

	t = d->type;
	idx = NULL;
	while(1)
	{
		allowarray = false;
		if (idx)
			allowarray = t->arraysize>0 ||
						(t->type == ev_vector);
		else if (!idx)
		{
			allowarray = d->arraysize ||
						(d->type->type == ev_pointer) ||
						(d->type->type == ev_string) ||
						(d->type->type == ev_vector);
		}

		if (allowarray && QCC_PR_CheckToken("["))
		{
			tmp = QCC_PR_Expression (TOP_PRIORITY, 0);
			QCC_PR_Expect("]");

			/*if its a pointer that got dereferenced, follow the type*/
			if (!idx && t->type == ev_pointer && !d->arraysize)
				t = t->aux_type;

			if (!idx && d->type->type == ev_pointer)
			{
				/*no bounds checks on pointer dereferences*/
			}
			else if (!idx && d->type->type == ev_string && !d->arraysize)
			{
				/*automatic runtime bounds checks on strings, I'm not going to check this too much...*/
			}
			else if ((!idx && d->type->type == ev_vector && !d->arraysize) || (idx && t->type == ev_vector && !t->arraysize))
			{
				if (tmp->constant)
				{
					unsigned int i;
					if (tmp->type->type == ev_integer)
						i = G_INT(tmp->ofs);
					else if (tmp->type->type == ev_float)
						i = G_FLOAT(tmp->ofs);
					if (i < 0 || i >= 3)
						QCC_PR_ParseErrorPrintDef(0, d, "(vector) array index out of bounds");
				}
				else if (QCC_OPCodeValid(&pr_opcodes[OP_BOUNDCHECK]))
				{
					tmp = QCC_SupplyConversion(tmp, ev_integer, true);
					QCC_PR_SimpleStatement (OP_BOUNDCHECK, tmp->ofs, 3, 0, false);
				}
				t = type_float;
			}
			else if (!((!idx)?d->arraysize:t->arraysize))
			{
				QCC_PR_ParseErrorPrintDef(0, d, "array index on non-array");
			}
			else if (tmp->constant)
			{
				unsigned int i;
				if (tmp->type->type == ev_integer)
					i = G_INT(tmp->ofs);
				else if (tmp->type->type == ev_float)
					i = G_FLOAT(tmp->ofs);
				if (i < 0 || i >= ((!idx)?d->arraysize:t->arraysize))
					QCC_PR_ParseErrorPrintDef(0, d, "(constant) array index out of bounds");
			}
			else
			{
				if (QCC_OPCodeValid(&pr_opcodes[OP_BOUNDCHECK]))
				{
					tmp = QCC_SupplyConversion(tmp, ev_integer, true);
					QCC_PR_SimpleStatement (OP_BOUNDCHECK, tmp->ofs, ((!idx)?d->arraysize:t->arraysize), 0, false);
				}
			}

			if (t->size != 1) /*don't multiply by type size if the instruction/emulation will do that instead*/
			{
				if (tmp->type->type == ev_float)
					tmp = QCC_PR_Statement(&pr_opcodes[OP_MUL_F], tmp, QCC_MakeFloatConst(t->size), NULL);
				else
					tmp = QCC_PR_Statement(&pr_opcodes[OP_MUL_I], tmp, QCC_MakeIntConst(t->size), NULL);
			}

			/*calc the new index*/
			if (idx && idx->type->type == ev_float && tmp->type->type == ev_float)
				idx = QCC_PR_Statement(&pr_opcodes[OP_ADD_F], idx, QCC_SupplyConversion(tmp, ev_float, true), NULL);
			else if (idx)
				idx = QCC_PR_Statement(&pr_opcodes[OP_ADD_I], idx, QCC_SupplyConversion(tmp, ev_integer, true), NULL);
			else
				idx = tmp;
		}
		else if ((t->type == ev_pointer || t->type == ev_struct || t->type == ev_union) && (QCC_PR_CheckToken(".") || QCC_PR_CheckToken("->")))
		{
			if (!idx && t->type == ev_pointer && !d->arraysize)
				t = t->aux_type;

			for (t = t->param; t; t = t->next)
			{
				if (QCC_PR_CheckName(t->name))
				{
					break;
				}
			
			}
			if (!t)
				QCC_PR_ParseError(0, "%s is not a member", pr_token);

			if (QCC_OPCodeValid(&pr_opcodes[OP_ADD_I]))
			{
				tmp = QCC_MakeIntConst(t->ofs);
				if (idx)
					idx = QCC_PR_Statement(&pr_opcodes[OP_ADD_I], idx, tmp, NULL);
				else
					idx = tmp;
			}
			else
			{
				tmp = QCC_MakeFloatConst(t->ofs);
				if (idx)
					idx = QCC_PR_Statement(&pr_opcodes[OP_ADD_F], idx, tmp, NULL);
				else
					idx = tmp;
			}
		}
		else
			break;
	}

	if (idx)
	{
		if (d->type->type == ev_pointer)
		{
			switch (t->type)
			{
			case ev_pointer:
				d = QCC_PR_Statement(&pr_opcodes[OP_LOADP_I], d, QCC_SupplyConversion(idx, ev_integer, true), NULL);
				break;
			case ev_float:
				d = QCC_PR_Statement(&pr_opcodes[OP_LOADP_F], d, QCC_SupplyConversion(idx, ev_integer, true), NULL);
				break;
			case ev_integer:
				d = QCC_PR_Statement(&pr_opcodes[OP_LOADP_I], d, QCC_SupplyConversion(idx, ev_integer, true), NULL);
				break;
			case ev_string:
				d = QCC_PR_Statement(&pr_opcodes[OP_LOADP_S], d, QCC_SupplyConversion(idx, ev_integer, true), NULL);
				break;
			case ev_vector:
				d = QCC_PR_Statement(&pr_opcodes[OP_LOADP_V], d, QCC_SupplyConversion(idx, ev_integer, true), NULL);
				break;
			case ev_entity:
				d = QCC_PR_Statement(&pr_opcodes[OP_LOADP_ENT], d, QCC_SupplyConversion(idx, ev_integer, true), NULL);
				break;
			case ev_field:
				d = QCC_PR_Statement(&pr_opcodes[OP_LOADP_FLD], d, QCC_SupplyConversion(idx, ev_integer, true), NULL);
				break;
			case ev_function:
				d = QCC_PR_Statement(&pr_opcodes[OP_LOADP_FNC], d, QCC_SupplyConversion(idx, ev_integer, true), NULL);
				break;
			case ev_struct:
			case ev_union:
				d = QCC_PR_Statement(&pr_opcodes[OP_LOADP_I], d, QCC_SupplyConversion(idx, ev_integer, true), NULL);
				break;

			default:
				QCC_PR_ParseError(ERR_NOVALIDOPCODES, "No op available. Try assembler");
			}
			d->type = t;
		}
		else if (d->type->type == ev_string && d->arraysize == 0)
		{
			d = QCC_PR_Statement(&pr_opcodes[OP_LOADP_C], d, QCC_SupplyConversion(idx, ev_float, true), NULL);
		}
		else if (d->type->type == ev_vector && d->arraysize == 0)
		{
			d = QCC_PR_Statement(&pr_opcodes[OP_LOADA_F], d, QCC_SupplyConversion(idx, ev_integer, true), (QCC_dstatement_t **)0xffffffff);
			d->type = type_float;
		}
		else if (QCC_OPCodeValid(&pr_opcodes[OP_LOADA_F]))
		{
			/*don't care about assignments. the code can convert an OP_LOADA_F to an OP_ADDRESS on assign*/
			/*source type is a struct, or its an array, or something that can otherwise be directly accessed*/
			switch(t->type)
			{
			case ev_pointer:
				d = QCC_PR_Statement(&pr_opcodes[OP_LOADA_I], d, QCC_SupplyConversion(idx, ev_integer, true), (QCC_dstatement_t **)0xffffffff);
				break;
			case ev_float:
				d = QCC_PR_Statement(&pr_opcodes[OP_LOADA_F], d, QCC_SupplyConversion(idx, ev_integer, true), (QCC_dstatement_t **)0xffffffff);
				break;
			case ev_integer:
				d = QCC_PR_Statement(&pr_opcodes[OP_LOADA_I], d, QCC_SupplyConversion(idx, ev_integer, true), (QCC_dstatement_t **)0xffffffff);
				break;
			case ev_string:
				d = QCC_PR_Statement(&pr_opcodes[OP_LOADA_S], d, QCC_SupplyConversion(idx, ev_integer, true), (QCC_dstatement_t **)0xffffffff);
				break;
			case ev_vector:
				d = QCC_PR_Statement(&pr_opcodes[OP_LOADA_V], d, QCC_SupplyConversion(idx, ev_integer, true), (QCC_dstatement_t **)0xffffffff);
				break;
			case ev_entity:
				d = QCC_PR_Statement(&pr_opcodes[OP_LOADA_ENT], d, QCC_SupplyConversion(idx, ev_integer, true), (QCC_dstatement_t **)0xffffffff);
				break;
			case ev_field:
				d = QCC_PR_Statement(&pr_opcodes[OP_LOADA_FLD], d, QCC_SupplyConversion(idx, ev_integer, true), (QCC_dstatement_t **)0xffffffff);
				break;
			case ev_function:
				d = QCC_PR_Statement(&pr_opcodes[OP_LOADA_FNC], d, QCC_SupplyConversion(idx, ev_integer, true), (QCC_dstatement_t **)0xffffffff);
				break;
			case ev_struct:
			case ev_union:
				//FIXME...
				d = QCC_PR_Statement(&pr_opcodes[OP_LOADA_STRUCT], d, QCC_SupplyConversion(idx, ev_integer, true), (QCC_dstatement_t **)0xffffffff);
				break;
			default:
				QCC_PR_ParseError(ERR_NOVALIDOPCODES, "No op available. Try assembler");
			}
			d->type = t;
		}
		else if (idx->constant)
		{
			int cidx;
			idx = QCC_SupplyConversion(idx, ev_integer, true);
			cidx = G_INT(idx->ofs);

			d->references++;
			tmp = (void *)qccHunkAlloc (sizeof(QCC_def_t));
			memcpy (tmp, d, sizeof(QCC_def_t));
			tmp->arraysize = 0;
			tmp->ofs = d->ofs + cidx;
			d = tmp;

			//d can be assigned to freely
		}
		else if (allowarrayassign && QCC_PR_CheckToken("="))
		{
			/*if its assigned to, generate a functioncall to do the store*/
			QCC_def_t *args[2], *funcretr, *rhs;

			d->references++;
			funcretr = QCC_PR_GetDef(type_function, qcva("ArraySet*%s", d->name), NULL, true, 0, false);

			rhs = QCC_PR_Expression(TOP_PRIORITY, 0);
			if (rhs->type->type != t->type)
				QCC_PR_ParseErrorPrintDef(ERR_TYPEMISMATCH, d, "Type Mismatch on array assignment");

			args[0] = QCC_SupplyConversion(idx, ev_float, true);
			args[1] = rhs;
			qcc_usefulstatement=true;
			d = QCC_PR_GenerateFunctionCall(funcretr, args, 2);
			d->type = t;

			return d;
		}
		else if (QCC_OPCodeValid(&pr_opcodes[OP_FETCH_GBL_F]))
		{
			if (!d->arraysize)
				QCC_PR_ParseErrorPrintDef(ERR_TYPEMISMATCH, d, "array lookup on non-array");

			if (d->temp)
				QCC_PR_ParseErrorPrintDef(ERR_TYPEMISMATCH, d, "array lookup on a temp");

			/*hexen2 format has opcodes to read arrays (but has no way to write)*/
			switch(t->type)
			{
			case ev_field:
			case ev_pointer:
			case ev_integer:
				d = QCC_PR_Statement(&pr_opcodes[OP_FETCH_GBL_F], d, QCC_SupplyConversion(idx, ev_float, true), &st);	//get pointer to precise def.
				d->type = t;
				break;
			case ev_float:
				d = QCC_PR_Statement(&pr_opcodes[OP_FETCH_GBL_F], d, QCC_SupplyConversion(idx, ev_float, true), &st);	//get pointer to precise def.
//				st->a = d->ofs;
				break;
			case ev_vector:
				idx = QCC_PR_Statement(&pr_opcodes[OP_DIV_F], QCC_SupplyConversion(idx, ev_float, true), QCC_MakeFloatConst(3), NULL);
				d = QCC_PR_Statement(&pr_opcodes[OP_FETCH_GBL_V], d, idx, &st);	//get pointer to precise def.
//				st->a = d->ofs;
				break;
			case ev_string:
				d = QCC_PR_Statement(&pr_opcodes[OP_FETCH_GBL_S], d, QCC_SupplyConversion(idx, ev_float, true), &st);	//get pointer to precise def.
//				st->a = d->ofs;
				break;
			case ev_entity:
				d = QCC_PR_Statement(&pr_opcodes[OP_FETCH_GBL_E], d, QCC_SupplyConversion(idx, ev_float, true), &st);	//get pointer to precise def.
//				st->a = d->ofs;
				break;
			case ev_function:
				d = QCC_PR_Statement(&pr_opcodes[OP_FETCH_GBL_FNC], d, QCC_SupplyConversion(idx, ev_float, true), &st);	//get pointer to precise def.
//				st->a = d->ofs;
				break;
			default:
				QCC_PR_ParseError(ERR_NOVALIDOPCODES, "No op available. Try assembler");
				d = NULL;
				break;
			}
			d->type = t;
		}
		else
		{
			/*emulate the array access using a function call to do the read for us*/
			QCC_def_t *args[1], *funcretr;

			d->references++;

			/*make sure the function type that we're calling exists*/

			if (d->type->type == ev_vector)
			{
				def_parms[0].type = type_vector;
				funcretr = QCC_PR_GetDef(type_function, qcva("ArrayGet*%s", d->name), NULL, true, 0, false);

				args[0] = QCC_PR_Statement(&pr_opcodes[OP_DIV_F], QCC_SupplyConversion(idx, ev_float, true), QCC_MakeFloatConst(3), NULL);
				d = QCC_PR_GenerateFunctionCall(funcretr, args, 1);
				d->type = t;
			}
			else
			{
				def_parms[0].type = type_float;
				funcretr = QCC_PR_GetDef(type_function, qcva("ArrayGet*%s", d->name), NULL, true, 0, false);

				if (t->size > 1)
				{
					QCC_def_t *r;
					unsigned int i;
					int old_op = opt_assignments;
					d = QCC_GetTemp(t);
					idx = QCC_SupplyConversion(idx, ev_float, true);

					for (i = 0; i < t->size; i++)
					{
						if (i)
							args[0] = QCC_PR_Statement(&pr_opcodes[OP_ADD_F], idx, QCC_MakeFloatConst(i), (QCC_dstatement_t **)0xffffffff);
						else
						{
							args[0] = idx;
							opt_assignments = false;
						}
						r = QCC_PR_GenerateFunctionCall(funcretr, args, 1);
						opt_assignments = old_op;
						QCC_UnFreeTemp(idx);
						QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_F], r, d, (QCC_dstatement_t **)0xffffffff));
						d->ofs++;
					}
					QCC_FreeTemp(idx);
					d->ofs -= i;
					QCC_UnFreeTemp(d);
				}
				else
				{
					args[0] = QCC_SupplyConversion(idx, ev_float, true);
					d = QCC_PR_GenerateFunctionCall(funcretr, args, 1);
				}
				d->type = t;
			}
		}

		/*parse recursively*/
		d = QCC_PR_ParseArrayPointer(d, allowarrayassign);
	}

	return d;
}

/*
============
PR_ParseValue

Returns the global ofs for the current token
============
*/
QCC_def_t	*QCC_PR_ParseValue (QCC_type_t *assumeclass, pbool allowarrayassign)
{
	QCC_def_t		*d, *od, *tmp;
	QCC_type_t		*t;
	char		*name;

	char membername[2048];

// if the token is an immediate, allocate a constant for it
	if (pr_token_type == tt_immediate)
		return QCC_PR_ParseImmediate ();

	if (QCC_PR_CheckToken("["))
	{
		//originally used for reacc - taking the form of [5 84 2]
		//we redefine it to include statements - [a+b, c, 3+(d*2)]
		//and to not need the 2nd/3rd parts if you're lazy - [5] or [5,6] - FIXME: should we accept 1-d vector? or is that too risky with arrays and weird error messages?
		//note the addition of commas.
		//if we're parsing reacc code, we will still accept [(a+b) c (3+(d*2))], as QCC_PR_Term contains the () handling. We do also allow optional commas.
		QCC_def_t *x,*y,*z;
		if (flag_acc)
		{
			x = QCC_PR_Term(EXPR_DISALLOW_COMMA);
			QCC_PR_CheckToken(",");
			y = QCC_PR_Term(EXPR_DISALLOW_COMMA);
			QCC_PR_CheckToken(",");
			z = QCC_PR_Term(EXPR_DISALLOW_COMMA);
		}
		else
		{
			x = QCC_PR_Expression(TOP_PRIORITY, EXPR_DISALLOW_COMMA);

			if (QCC_PR_CheckToken(","))
				y = QCC_PR_Expression(TOP_PRIORITY, EXPR_DISALLOW_COMMA);
			else
				y = QCC_MakeFloatConst(0);

			if (QCC_PR_CheckToken(","))
				z = QCC_PR_Expression(TOP_PRIORITY, EXPR_DISALLOW_COMMA);
			else
				z = QCC_MakeFloatConst(0);
		}

		QCC_PR_Expect("]");

		if ((x->type->type != ev_float && x->type->type != ev_integer) ||
			(y->type->type != ev_float && y->type->type != ev_integer) ||
			(z->type->type != ev_float && z->type->type != ev_integer))
		{
			QCC_PR_ParseError(ERR_TYPEMISMATCH, "Argument not a single numeric value in vector constructor");
			return QCC_MakeVectorConst(0, 0, 0);
		}

		//return a constant if we can.
		if (x->constant && y->constant && z->constant)
		{
			d = QCC_MakeVectorConst(
				(x->type->type==ev_float)?G_FLOAT(x->ofs):G_INT(x->ofs),
				(y->type->type==ev_float)?G_FLOAT(y->ofs):G_INT(y->ofs),
				(z->type->type==ev_float)?G_FLOAT(z->ofs):G_INT(z->ofs));
			QCC_FreeTemp(x);
			QCC_FreeTemp(y);
			QCC_FreeTemp(z);
			return d;
		}

		//pack the variables into a vector
		d = QCC_GetTemp(type_vector);
		d->type = type_float;
		if (x->type->type == ev_float)
			QCC_PR_Statement(pr_opcodes + OP_STORE_F, x, d, (QCC_dstatement_t **)0xffffffff);
		else
			QCC_PR_Statement(pr_opcodes+OP_CONV_ITOF, x, d, (QCC_dstatement_t **)0xffffffff);
		d->ofs++;
		if (y->type->type == ev_float)
			QCC_PR_Statement(pr_opcodes + OP_STORE_F, y, d, (QCC_dstatement_t **)0xffffffff);
		else
			QCC_PR_Statement(pr_opcodes+OP_CONV_ITOF, y, d, (QCC_dstatement_t **)0xffffffff);
		d->ofs++;
		if (z->type->type == ev_float)
			QCC_PR_Statement(pr_opcodes + OP_STORE_F, z, d, (QCC_dstatement_t **)0xffffffff);
		else
			QCC_PR_Statement(pr_opcodes+OP_CONV_ITOF, z, d, (QCC_dstatement_t **)0xffffffff);
		d->ofs++;
		d->ofs -= 3;
		d->type = type_vector;

		QCC_FreeTemp(x);
		QCC_FreeTemp(y);
		QCC_FreeTemp(z);
		return d;
	}
	name = QCC_PR_ParseName ();

	if (assumeclass && assumeclass->parentclass)	// 'testvar' becomes 'self::testvar'
	{	//try getting a member.
		QCC_type_t *type;
		type = assumeclass;
		d = NULL;
		while(type != type_entity && type)
		{
			sprintf(membername, "%s::"MEMBERFIELDNAME, type->name, name);
			d = QCC_PR_GetDef (NULL, membername, pr_scope, false, 0, false);
			if (d)
				break;

			type = type->parentclass;
		}
		if (!d)
			d = QCC_PR_GetDef (NULL, name, pr_scope, false, 0, false);
	}
	else
	{
		// look through the defs
		d = QCC_PR_GetDef (NULL, name, pr_scope, false, 0, false);
	}

	if (!d)
	{
		if (	(!strcmp(name, "random" ))	||
				(!strcmp(name, "randomv"))	||
				(!strcmp(name, "sizeof"))	||
				(!strcmp(name, "entnum"))	||
				(!strcmp(name, "_")))	//intrinsics, any old function with no args will do.
		{
			d = QCC_PR_GetDef (type_function, name, NULL, true, 0, false);
			d->initialized = 0;
		}
		else if (keyword_class && !strcmp(name, "this"))
		{
			if (!pr_classtype)
				QCC_PR_ParseError(ERR_NOTANAME, "Cannot use 'this' outside of an OO function\n");
			od = QCC_PR_GetDef(NULL, "self", NULL, true, 0, false);
			d = QCC_PR_DummyDef(pr_classtype, "this", pr_scope, 0, od->ofs, true, GDF_CONST);
		}
		else if (keyword_class && !strcmp(name, "super"))
		{
			if (!pr_classtype)
				QCC_PR_ParseError(ERR_NOTANAME, "Cannot use 'super' outside of an OO function\n");
			od = QCC_PR_GetDef(NULL, "self", NULL, true, 0, false);
			d = QCC_PR_DummyDef(pr_classtype, "super", pr_scope, 0, od->ofs, true, GDF_CONST);
		}
		else
		{
			d = QCC_PR_GetDef (type_variant, name, pr_scope, true, 0, false);
			if (!d)
				QCC_PR_ParseError (ERR_UNKNOWNVALUE, "Unknown value \"%s\"", name);
			else
			{
				QCC_PR_ParseWarning (ERR_UNKNOWNVALUE, "Unknown value \"%s\".", name);
			}
		}
	}

	d = QCC_PR_ParseArrayPointer(d, allowarrayassign);

	t = d->type;
	if (keyword_class && t->type == ev_entity && t->parentclass && (QCC_PR_CheckToken(".") || QCC_PR_CheckToken("->")))
	{
		QCC_def_t *field;
		if (QCC_PR_CheckToken("("))
		{
			field = QCC_PR_Expression(TOP_PRIORITY, 0);
			QCC_PR_Expect(")");
		}
		else
			field = QCC_PR_ParseValue(d->type, false);
		if (field->type->type == ev_field)
		{
			if (!field->type->aux_type)
			{
				QCC_PR_ParseWarning(ERR_INTERNAL, "Field with null aux_type");
				d = QCC_PR_Statement(&pr_opcodes[OP_LOAD_FLD], d, field, NULL);
			}
			else
			{
				switch(field->type->aux_type->type)
				{
				default:
					QCC_PR_ParseError(ERR_INTERNAL, "Bad field type");
					break;
				case ev_integer:
					d = QCC_PR_Statement(&pr_opcodes[OP_LOAD_I], d, field, NULL);
					break;
				case ev_field:
					d = QCC_PR_Statement(&pr_opcodes[OP_LOAD_FLD], d, field, NULL);
					tmp = (void *)qccHunkAlloc (sizeof(QCC_def_t));
					memset (tmp, 0, sizeof(QCC_def_t));
					tmp->type = field->type->aux_type;
					tmp->ofs = d->ofs;
					tmp->temp = d->temp;
					tmp->constant = false;
					tmp->name = d->name;
					d = tmp;
					break;
				case ev_float:
					d = QCC_PR_Statement(&pr_opcodes[OP_LOAD_F], d, field, NULL);
					break;
				case ev_string:
					d = QCC_PR_Statement(&pr_opcodes[OP_LOAD_S], d, field, NULL);
					break;
				case ev_vector:
					d = QCC_PR_Statement(&pr_opcodes[OP_LOAD_V], d, field, NULL);
					break;
				case ev_function:
					d = QCC_PR_Statement(&pr_opcodes[OP_LOAD_FNC], d, field, NULL);
					tmp = (void *)qccHunkAlloc (sizeof(QCC_def_t));
					memset (tmp, 0, sizeof(QCC_def_t));
					tmp->type = field->type->aux_type;
					tmp->ofs = d->ofs;
					tmp->temp = d->temp;
					tmp->constant = false;
					tmp->name = d->name;
					d = tmp;
					break;
				case ev_entity:
					d = QCC_PR_Statement(&pr_opcodes[OP_LOAD_ENT], d, field, NULL);
					break;
				}
			}
		}
		else
			QCC_PR_IncludeChunk(".", false, NULL);
	}

	return d;
}


/*
============
PR_Term
============
*/
QCC_def_t *QCC_PR_Term (int exprflags)
{
	QCC_def_t	*e, *e2;
	etype_t	t;
	if (pr_token_type == tt_punct)	//a little extra speed...
	{
		if (QCC_PR_CheckToken("++"))
		{
			qcc_usefulstatement=true;
			e = QCC_PR_Term (0);
			if (e->constant)
				QCC_PR_ParseWarning(WARN_ASSIGNMENTTOCONSTANT, "Assignment to constant %s", e->name);
			if (e->temp)
				QCC_PR_ParseWarning(WARN_ASSIGNMENTTOCONSTANT, "Hey! That's a temp! ++ operators cannot work on temps!");
			switch (e->type->type)
			{
			case ev_integer:
				QCC_PR_Statement3(&pr_opcodes[OP_ADD_I], e, QCC_MakeIntConst(1), e, false);
				break;
			case ev_float:
				QCC_PR_Statement3(&pr_opcodes[OP_ADD_F], e, QCC_MakeFloatConst(1), e, false);
				break;
			default:
				QCC_PR_ParseError(ERR_BADPLUSPLUSOPERATOR, "++ operator on unsupported type");
				break;
			}
			return e;
		}
		else if (QCC_PR_CheckToken("--"))
		{
			qcc_usefulstatement=true;
			e = QCC_PR_Term (0);
			if (e->constant)
				QCC_PR_ParseWarning(WARN_ASSIGNMENTTOCONSTANT, "Assignment to constant %s", e->name);
			if (e->temp)
				QCC_PR_ParseWarning(WARN_ASSIGNMENTTOCONSTANT, "Hey! That's a temp! -- operators cannot work on temps!");
			switch (e->type->type)
			{
			case ev_integer:
				QCC_PR_Statement3(&pr_opcodes[OP_SUB_I], e, QCC_MakeIntConst(1), e, false);
				break;
			case ev_float:
				QCC_PR_Statement3(&pr_opcodes[OP_SUB_F], e, QCC_MakeFloatConst(1), e, false);
				break;
			default:
				QCC_PR_ParseError(ERR_BADPLUSPLUSOPERATOR, "-- operator on unsupported type");
				break;
			}
			return e;
		}

		if (QCC_PR_CheckToken ("!"))
		{
			e = QCC_PR_Expression (NOT_PRIORITY, EXPR_DISALLOW_COMMA|EXPR_WARN_ABOVE_1);
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
		else if (QCC_PR_CheckToken ("~"))
		{
			e = QCC_PR_Expression (NOT_PRIORITY, EXPR_DISALLOW_COMMA|EXPR_WARN_ABOVE_1);
			t = e->type->type;
			if (t == ev_float)
				e2 = QCC_PR_Statement (&pr_opcodes[OP_BINARYNOT_F], e, 0, NULL);
			else if (t == ev_integer)
				e2 = QCC_PR_Statement (&pr_opcodes[OP_BINARYNOT_I], e, 0, NULL);	//functions are integer values too.
			else
			{
				e2 = NULL;		// shut up compiler warning;
				QCC_PR_ParseError (ERR_BADNOTTYPE, "type mismatch for binary not");
			}
			return e2;
		}
		else if (QCC_PR_CheckToken ("&"))
		{
			int st = numstatements;
			e = QCC_PR_Expression (UNARY_PRIORITY, EXPR_DISALLOW_COMMA);
			t = e->type->type;

			if (st != numstatements)
				//woo, something like ent.field?
			{
				if ((OP_LOAD_F <= statements[numstatements-1].op && statements[numstatements-1].op <= OP_LOAD_FNC) || statements[numstatements-1].op == OP_LOAD_I || statements[numstatements-1].op == OP_LOAD_P)
				{
					statements[numstatements-1].op = OP_ADDRESS;
					e->type = QCC_PR_PointerType(e->type);
					return e;
				}
				else if (OP_LOADA_F <= statements[numstatements-1].op && statements[numstatements-1].op <= OP_LOADA_I)
				{
					statements[numstatements-1].op = OP_GLOBALADDRESS;
					e->type = QCC_PR_PointerType(e->type);
					return e;
				}
				else if (OP_LOADP_F <= statements[numstatements-1].op && statements[numstatements-1].op <= OP_LOADP_I)
				{
					statements[numstatements-1].op = OP_POINTER_ADD;
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
		else if (QCC_PR_CheckToken ("*"))
		{
			e = QCC_PR_Expression (UNARY_PRIORITY, EXPR_DISALLOW_COMMA);
			t = e->type->type;

			if (t == ev_string)
				e2 = QCC_PR_Statement(&pr_opcodes[OP_LOADP_C], e, QCC_MakeFloatConst(0), NULL);
			else if (t == ev_pointer)
			{
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
					e2 = NULL;
					break;
				}
				e2->type = e->type->aux_type;
			}
			else
				QCC_PR_ParseError (ERR_BADNOTTYPE, "type mismatch for *");
			return e2;
		}
		else if (QCC_PR_CheckToken ("-"))
		{
			e = QCC_PR_Expression (UNARY_PRIORITY, EXPR_DISALLOW_COMMA);

			switch(e->type->type)
			{
			case ev_float:
				e2 = QCC_PR_Statement (&pr_opcodes[OP_SUB_F], QCC_MakeFloatConst(0), e, NULL);
				break;
			case ev_vector:
				e2 = QCC_PR_Statement (&pr_opcodes[OP_SUB_V], QCC_MakeVectorConst(0, 0, 0), e, NULL);
				break;
			case ev_integer:
				e2 = QCC_PR_Statement (&pr_opcodes[OP_SUB_I], QCC_MakeIntConst(0), e, NULL);
				break;
			default:
				QCC_PR_ParseError (ERR_BADNOTTYPE, "type mismatch for -");
				e2 = NULL;
				break;
			}
			return e2;
		}
		else if (QCC_PR_CheckToken ("+"))
		{
			e = QCC_PR_Expression (UNARY_PRIORITY, EXPR_DISALLOW_COMMA);

			switch(e->type->type)
			{
			case ev_float:
				e2 = e;
				break;
			case ev_vector:
				e2 = e;
				break;
			case ev_integer:
				e2 = e;
				break;
			default:
				QCC_PR_ParseError (ERR_BADNOTTYPE, "type mismatch for +");
				e2 = NULL;
				break;
			}
			return e2;
		}

		if (QCC_PR_CheckToken ("("))
		{
			QCC_type_t *newtype;
			newtype = QCC_PR_ParseType(false, true);
			if (newtype)
			{
				QCC_PR_Expect (")");
				e = QCC_PR_Expression (UNARY_PRIORITY, EXPR_DISALLOW_COMMA);

				/*you may cast from a type to itself*/
				if (!typecmp(e->type, newtype))
				{
				}
				/*you may cast from const 0 to any type of same size for free (from either int or float for simplicity)*/
				else if (newtype->size == e->type->size && (e->type->type == ev_integer || e->type->type == ev_float) && e->constant && !G_INT(e->ofs))
				{
					//direct cast
					e2 = (void *)qccHunkAlloc (sizeof(QCC_def_t));
					memset (e2, 0, sizeof(QCC_def_t));

					e2->type = newtype;
					e2->ofs = e->ofs;
					e2->constant = true;
					e2->temp = e->temp;
					return e2;
				}
				/*cast from int->float will convert*/
				else if (newtype->type == ev_float && e->type->type == ev_integer)
					return QCC_PR_Statement (&pr_opcodes[OP_CONV_ITOF], e, 0, NULL);
				/*cast from float->int will convert*/
				else if (newtype->type == ev_integer && e->type->type == ev_float)
					return QCC_PR_Statement (&pr_opcodes[OP_CONV_FTOI], e, 0, NULL);
				/*you may freely cast between pointers (and ints, as this is explicit) (strings count as pointers - WARNING: some strings may not be expressable as pointers)*/
				else if (
					//pointers
					((newtype->type == ev_pointer || newtype->type == ev_string || newtype->type == ev_integer) && (e->type->type == ev_pointer || e->type->type == ev_string || e->type->type == ev_integer))
					//ents/classs
					|| (newtype->type == ev_entity && e->type->type == ev_entity)
					//variants are fine too
					|| (newtype->type == ev_variant || e->type->type == ev_variant)
					)
				{
					//direct cast
					e2 = (void *)qccHunkAlloc (sizeof(QCC_def_t));
					memset (e2, 0, sizeof(QCC_def_t));

					e2->type = newtype;
					e2->ofs = e->ofs;
					e2->constant = true;
					e2->temp = e->temp;
					return e2;
				}
				else
					QCC_PR_ParseError(0, "Bad type cast\n");
			}
/*			else if (QCC_PR_CheckToken("*"))
			{
				QCC_PR_Expect (")");
				e = QCC_PR_Term();
				e2 = (void *)qccHunkAlloc (sizeof(QCC_def_t));
				memset (e2, 0, sizeof(QCC_def_t));
				e2->type = type_pointer;
				e2->ofs = e->ofs;
				e2->constant = true;
				e2->temp = e->temp;
				return e2;
			}
			else if (QCC_PR_CheckKeyword(keyword_float, "float"))	//check for type casts
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
			else if (QCC_PR_CheckKeyword(keyword_class, "class"))
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
			else if (QCC_PR_CheckKeyword(keyword_integer, "integer"))	//check for type casts
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
			else if (QCC_PR_CheckKeyword(keyword_int, "int"))	//check for type casts
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
*/			else
			{
				pbool oldcond = conditional;
				conditional = conditional?2:0;
				e =	QCC_PR_Expression (TOP_PRIORITY, 0);
				QCC_PR_Expect (")");
				conditional = oldcond;
			}
			return e;
		}
	}
	return QCC_PR_ParseValue (pr_classtype, !(exprflags&EXPR_DISALLOW_ARRAYASSIGN));
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

	if (from->type->type == ev_variant)
		return 3;

/*	if (from->type->type == ev_pointer && from->type->aux_type->type == to)
		return 1;

	if (QCC_ShouldConvert(from, to)>=0)
		return 1;
*/
//	if (from->type->type == ev_integer && to == ev_function)
//		return 1;

	if (from->constant && from->arraysize == 0 && from->type->size == ((to==ev_vector)?3:1) && (from->type->type == ev_integer || from->type->type == ev_float) && !G_INT(from->ofs))
		return 2;

	return -100;
}
/*
==============
PR_Expression
==============
*/

QCC_def_t *QCC_PR_Expression (int priority, int exprflags)
{
	QCC_dstatement32_t	*st;
	QCC_opcode_t	*op, *oldop;

	QCC_opcode_t *bestop;
	int numconversions, c;

	int opnum;

	QCC_def_t		*e, *e2;
	etype_t		type_a, type_b, type_c;

	if (priority == 0)
		return QCC_PR_Term (exprflags);

	e = QCC_PR_Expression (priority-1, exprflags);

	while (1)
	{
		if (priority == FUNC_PRIORITY)
		{
			if (QCC_PR_CheckToken ("(") )
			{
				qcc_usefulstatement=true;
				e = QCC_PR_ParseFunctionCall (e);
			}
			if (QCC_PR_CheckToken ("?"))
			{
				QCC_dstatement32_t *fromj, *elsej;
				QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_IFNOT_I], e, NULL, &fromj));
				e = QCC_PR_Expression(TOP_PRIORITY, 0);
				e2 = QCC_GetTemp(e->type);
				QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[(e2->type->size>=3)?OP_STORE_V:OP_STORE_F], e, e2, (QCC_dstatement_t **)0xffffffff));
				//e2 can be stomped upon until its reused anyway
				QCC_UnFreeTemp(e2);

				QCC_PR_Expect(":");
				QCC_PR_Statement(&pr_opcodes[OP_GOTO], NULL, NULL, &elsej);
				fromj->b = &statements[numstatements] - fromj;
				e = QCC_PR_Expression(TOP_PRIORITY, 0);

				if (typecmp(e->type, e2->type) != 0)
					QCC_PR_ParseError(0, "Ternary operator with mismatching types\n");
				QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[(e2->type->size>=3)?OP_STORE_V:OP_STORE_F], e, e2, (QCC_dstatement_t **)0xffffffff));
				QCC_UnFreeTemp(e2);

				elsej->a = &statements[numstatements] - elsej;
				return e2;
			}
		}

		opnum=0;

		if (pr_token_type == tt_immediate)
		{
			if ((pr_immediate_type->type == ev_float && pr_immediate._float < 0) ||
				(pr_immediate_type->type == ev_integer && pr_immediate._int < 0))	//hehehe... was a minus all along...
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
			if (!QCC_PR_CheckToken (op->name))
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
				if ( !simplestore && (unsigned)(statements[numstatements-1].op - OP_LOADP_F) < 7 && statements[numstatements-1].c == e->ofs)
				{
					if (!statements[numstatements-1].b)
					{
						//if the loadp has no offset, remove the instruction and convert the dest of this instruction directly to the pointer's load address
						//this kills the add 0.
						e->ofs = statements[numstatements-1].a;
						numstatements--;
					}
					else
					{
						statements[numstatements-1].op = OP_POINTER_ADD;
					}
					if (e->type != type_pointer)
					{
						type_pointer->aux_type->type = e->type->type;
						e->type = type_pointer;
					}
				}
				if ( !simplestore && statements[numstatements-1].op == OP_LOADP_C && e->ofs == statements[numstatements-1].c)
				{
					statements[numstatements-1].op = OP_ADD_SF;
					e->type = type_string;

					//now we want to make sure that string = float can't work without it being a dereferenced pointer. (we don't want to allow storep_c without dereferece)
					e2 = QCC_PR_Expression (priority, exprflags);
					if (e2->type->type == ev_float)
						op = &pr_opcodes[OP_STOREP_C];
				}
				else
					e2 = QCC_PR_Expression (priority, exprflags);
			}
			else
			{
				if (op->priority == 7 && opt_logicops)
				{
					optres_logicops++;
					st = &statements[numstatements];
					if (*op->name == '&')	//statement 3 because we don't want to optimise this into if from not ifnot
						QCC_PR_Statement3(&pr_opcodes[OP_IFNOT_I], e, NULL, NULL, false);
					else
						QCC_PR_Statement3(&pr_opcodes[OP_IF_I], e, NULL, NULL, false);
				}

				e2 = QCC_PR_Expression (priority-1, exprflags | EXPR_DISALLOW_ARRAYASSIGN);
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
								/*FIXME: I don't like this code*/
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
								else if ((*op->type_a)->type == ev_pointer && e->type->aux_type->type != (*op->type_a)->aux_type->type)
									c = -300;	//don't use this op if its a pointer to a different type
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
					if (flag_laxcasts)
					{
						op = oldop;
						QCC_PR_ParseWarning(WARN_LAXCAST, "type mismatch for %s (%s and %s)", oldop->name, e->type->name, e2->type->name);
					}
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
					if (e->type->type == ev_function)
					{
						QCC_PR_ParseWarning(WARN_ASSIGNMENTTOCONSTANTFUNC, "Assignment to function %s", e->name);
						QCC_PR_ParsePrintDef(WARN_ASSIGNMENTTOCONSTANTFUNC, e);
					}
					else
					{
						QCC_PR_ParseWarning(WARN_ASSIGNMENTTOCONSTANT, "Assignment to constant %s", e->name);
						QCC_PR_ParsePrintDef(WARN_ASSIGNMENTTOCONSTANT, e);
					}
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

			if (priority > 1 && exprflags & EXPR_WARN_ABOVE_1)
				QCC_PR_ParseWarning(0, "You may wish to add brackets after that ! operator");

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
//the only inefficiency here is with an extra temp (we can't reuse the original)
//this is not a problem, as the optimise temps or locals marshalling can clean these up for us
					qcc_usefulstatement=true;
//load
//add to temp
//store temp to offset
//return original loaded (which is not at the same offset as the pointer we store to)
					e2 = QCC_GetTemp(type_float);
					e3 = QCC_GetTemp(type_pointer);
					QCC_PR_SimpleStatement(OP_ADDRESS, statements[numstatements-1].a, statements[numstatements-1].b, e3->ofs, false);
					if (e->type->type == ev_float)
					{
						QCC_PR_Statement3(&pr_opcodes[OP_ADD_F], e, QCC_MakeFloatConst(1), e2, false);
						QCC_PR_Statement3(&pr_opcodes[OP_STOREP_F], e2, e3, NULL, false);
					}
					else if (e->type->type == ev_integer)
					{
						QCC_PR_Statement3(&pr_opcodes[OP_ADD_I], e, QCC_MakeIntConst(1), e2, false);
						QCC_PR_Statement3(&pr_opcodes[OP_STOREP_I], e2, e3, NULL, false);
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
//add to original
//return temp (which == original)
					QCC_PR_ParseWarning(WARN_INEFFICIENTPLUSPLUS, "++ suffix operator results in inefficient behaviour. Use +=1 or prefix form instead");
					qcc_usefulstatement=true;

					e2 = QCC_GetTemp(type_float);
					QCC_PR_Statement3(&pr_opcodes[OP_STORE_F], e, e2, NULL, false);
					QCC_PR_Statement3(&pr_opcodes[OP_ADD_F], e, QCC_MakeFloatConst(1), e, false);
					QCC_FreeTemp(e);
					e = e2;
				}
				else if (e->type->type == ev_integer)
				{
					QCC_PR_ParseWarning(WARN_INEFFICIENTPLUSPLUS, "++ suffix operator results in inefficient behaviour. Use +=1 or prefix form instead");
					qcc_usefulstatement=true;

					e2 = QCC_GetTemp(type_integer);
					QCC_PR_Statement3(&pr_opcodes[OP_STORE_I], e, e2, NULL, false);
					QCC_PR_Statement3(&pr_opcodes[OP_ADD_I], e, QCC_MakeIntConst(1), e, false);
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
//return original loaded (which is not at the same offset as the pointer we store to)
					e2 = QCC_GetTemp(type_float);
					e3 = QCC_GetTemp(type_pointer);
					QCC_PR_SimpleStatement(OP_ADDRESS, statements[numstatements-1].a, statements[numstatements-1].b, e3->ofs, false);
					if (e->type->type == ev_float)
					{
						QCC_PR_Statement3(&pr_opcodes[OP_SUB_F], e, QCC_MakeFloatConst(1), e2, false);
						QCC_PR_Statement3(&pr_opcodes[OP_STOREP_F], e2, e3, NULL, false);
					}
					else if (e->type->type == ev_integer)
					{
						QCC_PR_Statement3(&pr_opcodes[OP_SUB_I], e, QCC_MakeIntConst(1), e2, false);
						QCC_PR_Statement3(&pr_opcodes[OP_STOREP_I], e2, e3, NULL, false);
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
					QCC_PR_Statement3(&pr_opcodes[OP_STORE_F], e, e2, NULL, false);
					QCC_PR_Statement3(&pr_opcodes[OP_SUB_F], e, QCC_MakeFloatConst(1), e, false);
					QCC_FreeTemp(e);
					e = e2;
				}
				else if (e->type->type == ev_integer)
				{
					QCC_PR_ParseWarning(WARN_INEFFICIENTPLUSPLUS, "-- suffix operator results in inefficient behaviour. Use -=1 or prefix form instead");
					qcc_usefulstatement=true;

					e2 = QCC_GetTemp(type_integer);
					QCC_PR_Statement3(&pr_opcodes[OP_STORE_I], e, e2, NULL, false);
					QCC_PR_Statement3(&pr_opcodes[OP_SUB_I], e, QCC_MakeIntConst(1), e, false);
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

	if (!(exprflags&EXPR_DISALLOW_COMMA) && priority == TOP_PRIORITY && QCC_PR_CheckToken (","))
	{
		if (!qcc_usefulstatement)
			QCC_PR_ParseWarning(WARN_POINTLESSSTATEMENT, "Effectless statement");

		QCC_FreeTemp(e);
		qcc_usefulstatement = false;
		e = QCC_PR_Expression(TOP_PRIORITY, exprflags);
	}

	return e;
}

int QCC_PR_IntConstExpr(void)
{
	QCC_def_t *def = QCC_PR_Expression(TOP_PRIORITY, 0);
	if (def->constant)
	{
		def->references++;
		if (def->type->type == ev_integer)
			return G_INT(def->ofs);
		if (def->type->type == ev_float)
		{
			int i = G_FLOAT(def->ofs);
			if ((float)i == G_FLOAT(def->ofs))
				return i;
		}
	}
	QCC_PR_ParseError(ERR_NOTACONSTANT, "Value is not an integer constant");
	return true;
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

pbool QCC_PR_StatementBlocksMatch(QCC_dstatement_t *p1, int p1count, QCC_dstatement_t *p2, int p2count)
{
	if (p1count != p2count)
		return false;

	while(p1count>0)
	{
		if (p1->op != p2->op)
			return false;
		if (p1->a != p2->a)
			return false;
		if (p1->b != p2->b)
			return false;
		if (p1->c != p2->c)
			return false;
		p1++;
		p2++;
		p1count--;
	}

	return true;
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
	int statementstart = pr_source_line;

	if (QCC_PR_CheckToken ("{"))
	{
		e = pr.localvars;
		while (!QCC_PR_CheckToken("}"))
			QCC_PR_ParseStatement ();

		if (pr_subscopedlocals)
		{
			for (e2 = pr.localvars; e2 != e; e2 = e2->nextlocal)
			{
				if (!e2->subscoped_away)
				{
					Hash_RemoveData(&localstable, e2->name, e2);
					e2->subscoped_away = true;
				}
			}
		}
		return;
	}

	if (QCC_PR_CheckKeyword(keyword_return, "return"))
	{
		/*if (pr_classtype)
		{
			e = QCC_PR_GetDef(NULL, "__oself", pr_scope, false, 0);
			e2 = QCC_PR_GetDef(NULL, "self", NULL, false, 0);
			QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_ENT], e, QCC_PR_DummyDef(pr_classtype, "self", pr_scope, 0, e2->ofs, false), NULL));
		}*/

		if (QCC_PR_CheckToken (";"))
		{
			if (pr_scope->type->aux_type->type != ev_void)
				QCC_PR_ParseWarning(WARN_MISSINGRETURNVALUE, "\'%s\' should return %s", pr_scope->name, pr_scope->type->aux_type->name);
			if (opt_return_only)
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_DONE], 0, 0, NULL));
			else
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_RETURN], 0, 0, NULL));
			return;
		}
		e = QCC_PR_Expression (TOP_PRIORITY, 0);
		e2 = QCC_SupplyConversion(e, pr_scope->type->aux_type->type, true);
		if (e != e2)
		{
			QCC_PR_ParseWarning(WARN_CORRECTEDRETURNTYPE, "\'%s\' returned %s, expected %s, conversion supplied", pr_scope->name, e->type->name, pr_scope->type->aux_type->name);
			e = e2;
		}
		QCC_PR_Expect (";");
		if (pr_scope->type->aux_type->type != e->type->type)
			QCC_PR_ParseWarning(WARN_WRONGRETURNTYPE, "\'%s\' returned %s, expected %s", pr_scope->name, e->type->name, pr_scope->type->aux_type->name);
		QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_RETURN], e, 0, NULL));
		return;
	}
	if (QCC_PR_CheckKeyword(keyword_exit, "exit"))
	{
		QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_DONE], 0, 0, NULL));
		QCC_PR_Expect (";");
		return;
	}

	if (QCC_PR_CheckKeyword(keyword_while, "while"))
	{
		continues = num_continues;
		breaks = num_breaks;

		QCC_PR_Expect ("(");
		patch2 = &statements[numstatements];
		conditional = 1;
		e = QCC_PR_Expression (TOP_PRIORITY, 0);
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
			else if (!typecmp( e->type, type_string) && flag_ifstring)	//special case, as strings are now pointers, not offsets from string table
			{
				QCC_PR_ParseWarning(WARN_IFSTRING_USED, "while(string) can result in bizzare behaviour");
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IFNOT_S], e, 0, &patch1));
			}
			else if (!typecmp( e->type, type_float) && (flag_iffloat||QCC_OPCodeValid(&pr_opcodes[OP_IFNOT_F])))	//special case, as negative 0 is also zero
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IFNOT_F], e, 0, &patch1));
			else
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IFNOT_I], e, 0, &patch1));
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
	if (QCC_PR_CheckKeyword(keyword_for, "for"))
	{
		int old_numstatements;
		int numtemp, i;

		int					linenum[32];
		QCC_dstatement_t		temp[sizeof(linenum)/sizeof(linenum[0])];

		continues = num_continues;
		breaks = num_breaks;

		QCC_PR_Expect("(");
		if (!QCC_PR_CheckToken(";"))
		{
			QCC_FreeTemp(QCC_PR_Expression(TOP_PRIORITY, 0));
			QCC_PR_Expect(";");
		}

		patch2 = &statements[numstatements];
		if (!QCC_PR_CheckToken(";"))
		{
			conditional = 1;
			e = QCC_PR_Expression(TOP_PRIORITY, 0);
			conditional = 0;
			QCC_PR_Expect(";");
		}
		else
			e = NULL;

		if (!QCC_PR_CheckToken(")"))
		{
			old_numstatements = numstatements;
			QCC_FreeTemp(QCC_PR_Expression(TOP_PRIORITY, 0));

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
			QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_IFNOT_I], e, 0, &patch1));
		else
			patch1 = NULL;
		if (!QCC_PR_CheckToken(";"))
			QCC_PR_ParseStatement();	//don't give the hanging ';' warning.
		patch3 = &statements[numstatements];
		for (i = 0 ; i < numtemp ; i++)
		{
			statement_linenums[numstatements] = linenum[i];
			statements[numstatements++] = temp[i];
		}
		QCC_PR_SimpleStatement(OP_GOTO, patch2 - &statements[numstatements], 0, 0, false);
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
	if (QCC_PR_CheckKeyword(keyword_do, "do"))
	{
		continues = num_continues;
		breaks = num_breaks;

		patch1 = &statements[numstatements];
		QCC_PR_ParseStatement ();
		QCC_PR_Expect ("while");
		QCC_PR_Expect ("(");
		conditional = 1;
		e = QCC_PR_Expression (TOP_PRIORITY, 0);
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
			if (!typecmp( e->type, type_string) && flag_ifstring)
			{
				QCC_PR_ParseWarning(WARN_IFSTRING_USED, "do {} while(string) can result in bizzare behaviour");
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IF_S], e, NULL, &patch2));
			}
			else if (!typecmp( e->type, type_float) && (flag_iffloat||QCC_OPCodeValid(&pr_opcodes[OP_IFNOT_F])))
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IF_F], e, NULL, &patch2));
			else
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IF_I], e, NULL, &patch2));

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

	if (QCC_PR_CheckKeyword(keyword_local, "local"))
	{
		QCC_type_t *functionsclasstype = pr_classtype;
//		if (locals_end != numpr_globals)	//is this breaking because of locals?
//			QCC_PR_ParseWarning("local vars after temp vars\n");
		QCC_PR_ParseDefs (NULL);
		pr_classtype = functionsclasstype;
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
		QCC_PR_ParseDefs (NULL);
		return;
	}

	if (QCC_PR_CheckKeyword(keyword_state, "state"))
	{
		QCC_PR_Expect("[");
		QCC_PR_ParseState();
		QCC_PR_Expect(";");
		return;
	}
	if (QCC_PR_CheckToken("#"))
	{
		char *name;
		float frame = pr_immediate._float;
		QCC_PR_Lex();
		name = QCC_PR_ParseName();
		QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_STATE], QCC_MakeFloatConst(frame), QCC_PR_GetDef(type_function, name, NULL, false, 0, false), NULL));
		QCC_PR_Expect(";");
		return;
	}

	if (QCC_PR_CheckKeyword(keyword_if, "if"))
	{
		pbool negate = QCC_PR_CheckKeyword(keyword_not, "not");

		QCC_PR_Expect ("(");
		conditional = 1;
		e = QCC_PR_Expression (TOP_PRIORITY, 0);
		conditional = 0;

//		negate = negate != 0;

		if (negate)
		{
			if (!typecmp( e->type, type_string) && flag_ifstring)
			{
				QCC_PR_ParseWarning(WARN_IFSTRING_USED, "if not(string) can result in bizzare behaviour");
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IF_S], e, 0, &patch1));
			}
			else if (!typecmp( e->type, type_float) && (flag_iffloat||QCC_OPCodeValid(&pr_opcodes[OP_IFNOT_F])))
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IF_F], e, 0, &patch1));
			else
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IF_I], e, 0, &patch1));
		}
		else
		{
			if (!typecmp( e->type, type_string) && flag_ifstring)
			{
				QCC_PR_ParseWarning(WARN_IFSTRING_USED, "if (string) can result in bizzare behaviour");
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IFNOT_S], e, 0, &patch1));
			}
			else if (!typecmp( e->type, type_float) && (flag_iffloat||QCC_OPCodeValid(&pr_opcodes[OP_IFNOT_F])))
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IFNOT_F], e, 0, &patch1));
			else
				QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IFNOT_I], e, 0, &patch1));
		}

		QCC_PR_Expect (")");	//close bracket is after we save the statement to mem (so debugger does not show the if statement as being on the line after

		QCC_PR_ParseStatement ();

		if (QCC_PR_CheckKeyword (keyword_else, "else"))
		{
			int lastwasreturn;
			lastwasreturn = statements[numstatements-1].op == OP_RETURN || statements[numstatements-1].op == OP_DONE ||
				statements[numstatements-1].op == OP_GOTO;

			//the last statement of the if was a return, so we don't need the goto at the end
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

				if (QCC_PR_StatementBlocksMatch(patch1+1, patch2-patch1, patch2+1, &statements[numstatements] - patch2))
					QCC_PR_ParseWarning(0, "Two identical blocks each side of an else");
			}
		}
		else
			patch1->b = &statements[numstatements] - patch1;

		return;
	}
	if (QCC_PR_CheckKeyword(keyword_switch, "switch"))
	{
		int op;
		int hcstyle;
		int defaultcase = -1;
		temp_t *et;
		int oldst;
		QCC_type_t *switchtype;

		breaks = num_breaks;
		cases = num_cases;


		QCC_PR_Expect ("(");

		conditional = 1;
		e = QCC_PR_Expression (TOP_PRIORITY, 0);
		conditional = 0;

		if (e == &def_ret)
		{	//copy it out, so our hack just below doesn't crash us
/*			if (e->type->type == ev_vector)
				e = QCC_PR_Statement(pr_opcodes+OP_STORE_V, e, QCC_GetTemp(type_vector), NULL);
			else
				e = QCC_PR_Statement(pr_opcodes+OP_STORE_F, e, QCC_GetTemp(type_float), NULL);

			if (e == &def_ret)	//this shouldn't be happening
				QCC_Error(ERR_INTERNAL, "internal error: switch: e == &def_ret");
*/
			et = NULL;
		}
		else
		{
			et = e->temp;
			e->temp = NULL;	//so noone frees it until we finish this loop
		}

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

		switchtype = e->type;
		switch(switchtype->type)
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

		if (e == &def_ret)
			e->type = switchtype;	//set it back to the type it was actually meant to be.

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
				{
					if (e->type->type == ev_integer && pr_casesdef[i]->type->type == ev_float)
						pr_casesdef[i] = QCC_MakeIntConst((int)qcc_pr_globals[pr_casesdef[i]->ofs]);
					else
						QCC_PR_ParseWarning(WARN_SWITCHTYPEMISMATCH, "switch case type mismatch");
				}
				if (pr_casesdef2[i])
				{
					if (pr_casesdef2[i]->type->type != e->type->type)
					{
						if (e->type->type == ev_integer && pr_casesdef[i]->type->type == ev_float)
							pr_casesdef2[i] = QCC_MakeIntConst((int)qcc_pr_globals[pr_casesdef2[i]->ofs]);
						else
							QCC_PR_ParseWarning(WARN_SWITCHTYPEMISMATCH, "switch caserange type mismatch");
					}

					if (hcstyle)
					{
						QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_CASERANGE], pr_casesdef[i], pr_casesdef2[i], &patch3));
						patch3->c = &statements[pr_cases[i]] - patch3;
					}
					else
					{
						QCC_def_t *e3;

						if (e->type->type == ev_float)
						{
							e2 = QCC_PR_Statement (&pr_opcodes[OP_GE_F], e, pr_casesdef[i], NULL);
							e3 = QCC_PR_Statement (&pr_opcodes[OP_LE_F], e, pr_casesdef2[i], NULL);
							e2 = QCC_PR_Statement (&pr_opcodes[OP_AND_F], e2, e3, NULL);
							QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IF_I], e2, 0, &patch3));
							patch3->b = &statements[pr_cases[i]] - patch3;
						}
						else if (e->type->type == ev_integer)
						{
							e2 = QCC_PR_Statement (&pr_opcodes[OP_GE_I], e, pr_casesdef[i], NULL);
							e3 = QCC_PR_Statement (&pr_opcodes[OP_LE_I], e, pr_casesdef2[i], NULL);
							e2 = QCC_PR_Statement (&pr_opcodes[OP_AND_I], e2, e3, NULL);
							QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IF_I], e2, 0, &patch3));
							patch3->b = &statements[pr_cases[i]] - patch3;
						}
						else
							QCC_PR_ParseWarning(WARN_SWITCHTYPEMISMATCH, "switch caserange MUST be a float or integer");
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
							QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IF_I], e2, 0, &patch3));
						}
						else
						{
							if (e->type->type == ev_string)
								QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IFNOT_S], e, 0, &patch3));
							else if (e->type->type == ev_float)
								QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IFNOT_F], e, 0, &patch3));
							else
								QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_IFNOT_I], e, 0, &patch3));
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

		if (et)
		{
			e->temp = et;
			QCC_FreeTemp(e);
		}
		return;
	}

	if (QCC_PR_CheckKeyword(keyword_asm, "asm"))
	{
		if (QCC_PR_CheckToken("{"))
		{
			while (!QCC_PR_CheckToken("}"))
				QCC_PR_ParseAsm ();
		}
		else
			QCC_PR_ParseAsm ();
		return;
	}

	if (QCC_PR_CheckToken(":"))
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
	if (QCC_PR_CheckKeyword(keyword_goto, "goto"))
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

	if (QCC_PR_CheckKeyword(keyword_break, "break"))
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
	if (QCC_PR_CheckKeyword(keyword_continue, "continue"))
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
	if (QCC_PR_CheckKeyword(keyword_case, "case"))
	{
		if (num_cases >= max_cases)
		{
			max_cases += 8;
			pr_cases = realloc(pr_cases, sizeof(*pr_cases)*max_cases);
			pr_casesdef = realloc(pr_casesdef, sizeof(*pr_casesdef)*max_cases);
			pr_casesdef2 = realloc(pr_casesdef2, sizeof(*pr_casesdef2)*max_cases);
		}
		pr_cases[num_cases] = numstatements;
		pr_casesdef[num_cases] = QCC_PR_Expression (TOP_PRIORITY, EXPR_DISALLOW_COMMA);
		if (QCC_PR_CheckToken(".."))
		{
			pr_casesdef2[num_cases] = QCC_PR_Expression (TOP_PRIORITY, EXPR_DISALLOW_COMMA);
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
	if (QCC_PR_CheckKeyword(keyword_default, "default"))
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

	if (QCC_PR_CheckKeyword(keyword_thinktime, "thinktime"))
	{
		QCC_def_t *nextthink;
		QCC_def_t *time;
		e = QCC_PR_Expression (TOP_PRIORITY, 0);
		QCC_PR_Expect(":");
		e2 = QCC_PR_Expression (TOP_PRIORITY, 0);
		if (e->type->type != ev_entity || e2->type->type != ev_float)
			QCC_PR_ParseError(ERR_THINKTIMETYPEMISMATCH, "thinktime type mismatch");

		if (QCC_OPCodeValid(&pr_opcodes[OP_THINKTIME]))
			QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_THINKTIME], e, e2, NULL));
		else
		{
			nextthink = QCC_PR_GetDef(NULL, "nextthink", NULL, false, 0, false);
			if (!nextthink)
				QCC_PR_ParseError (ERR_UNKNOWNVALUE, "Unknown value \"%s\"", "nextthink");
			time = QCC_PR_GetDef(type_float, "time", NULL, false, 0, false);
			if (!time)
				QCC_PR_ParseError (ERR_UNKNOWNVALUE, "Unknown value \"%s\"", "time");
			nextthink = QCC_PR_Statement(&pr_opcodes[OP_ADDRESS], e, nextthink, NULL);
			time = QCC_PR_Statement(&pr_opcodes[OP_ADD_F], time, e2, NULL);
			QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STOREP_F], time, nextthink, NULL));
		}
		QCC_PR_Expect(";");
		return;
	}
	if (QCC_PR_CheckToken(";"))
	{
		int osl = pr_source_line;
		pr_source_line = statementstart;
		if (!expandedemptymacro)
			QCC_PR_ParseWarning(WARN_POINTLESSSTATEMENT, "Hanging ';'");
		pr_source_line = osl;
		return;
	}

//	qcc_functioncalled=0;

	qcc_usefulstatement = false;
	e = QCC_PR_Expression (TOP_PRIORITY, 0);
	expandedemptymacro = false;
	QCC_PR_Expect (";");

	if (e->type->type != ev_void && !qcc_usefulstatement)
	{
		int osl = pr_source_line;
		pr_source_line = statementstart;
		QCC_PR_ParseWarning(WARN_POINTLESSSTATEMENT, "Effectless statement");
		pr_source_line = osl;
	}

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
	if (QCC_PR_CheckToken("++") || QCC_PR_CheckToken("--"))
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
			temp_t *ftemp;

			self = QCC_PR_GetDef(type_entity, "self", NULL, false, 0, false);
			framef = QCC_PR_GetDef(NULL, "frame", NULL, false, 0, false);
			cycle_wrapped = QCC_PR_GetDef(type_float, "cycle_wrapped", NULL, false, 0, false);

			frame = QCC_PR_Statement(&pr_opcodes[OP_LOAD_F], self, framef, NULL);
			if (cycle_wrapped)
				QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_F], QCC_MakeFloatConst(0), cycle_wrapped, NULL));
			QCC_UnFreeTemp(frame);

			//make sure the frame is within the bounds given.
			ftemp = frame->temp;
			frame->temp = NULL;
			t1 = QCC_PR_Statement(&pr_opcodes[OP_LT_F], frame, s1, NULL);
			t2 = QCC_PR_Statement(&pr_opcodes[OP_GT_F], frame, def, NULL);
			t1 = QCC_PR_Statement(&pr_opcodes[OP_OR_F], t1, t2, NULL);
			QCC_PR_SimpleStatement(OP_IFNOT_I, t1->ofs, 2, 0, false);
			QCC_FreeTemp(t1);
				QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_F], s1, frame, NULL));
			  QCC_PR_SimpleStatement(OP_GOTO, t1->ofs, 13, 0, false);

			t1 = QCC_PR_Statement(&pr_opcodes[OP_GE_F], def, s1, NULL);
			QCC_PR_SimpleStatement(OP_IFNOT_I, t1->ofs, 7, 0, false);
			QCC_FreeTemp(t1);	//this block is the 'it's in a forwards direction'
				QCC_PR_SimpleStatement(OP_ADD_F, frame->ofs, QCC_MakeFloatConst(1)->ofs, frame->ofs, false);
				t1 = QCC_PR_Statement(&pr_opcodes[OP_GT_F], frame, def, NULL);
				QCC_PR_SimpleStatement(OP_IFNOT_I, t1->ofs,2, 0, false);
				QCC_FreeTemp(t1);
					QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_F], s1, frame, NULL));
					QCC_UnFreeTemp(frame);
					if (cycle_wrapped)
						QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_F], QCC_MakeFloatConst(1), cycle_wrapped, NULL));

			QCC_PR_SimpleStatement(OP_GOTO, 6, 0, 0, false);
				//reverse animation.
				QCC_PR_SimpleStatement(OP_SUB_F, frame->ofs, QCC_MakeFloatConst(1)->ofs, frame->ofs, false);
				t1 = QCC_PR_Statement(&pr_opcodes[OP_LT_F], frame, s1, NULL);
				QCC_PR_SimpleStatement(OP_IFNOT_I, t1->ofs,2, 0, false);
				QCC_FreeTemp(t1);
					QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_F], def, frame, NULL));
					QCC_UnFreeTemp(frame);
					if (cycle_wrapped)
						QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_F], QCC_MakeFloatConst(1), cycle_wrapped, NULL));

			//self.frame = frame happens with the normal state opcode.
			QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_STATE], frame, pr_scope, NULL));

			frame->temp = ftemp;
			QCC_FreeTemp(frame);
		}
		return;
	}

	if (pr_token_type != tt_immediate || pr_immediate_type != type_float)
		QCC_PR_ParseError (ERR_STATETYPEMISMATCH, "state frame must be a number");
	s1 = QCC_PR_ParseImmediate ();

	QCC_PR_CheckToken (",");

	name = QCC_PR_ParseName ();
	pr_scope = NULL;
	def = QCC_PR_GetDef (type_function, name, NULL, true, 0, false);
	pr_scope = sc;

	QCC_PR_Expect ("]");

	QCC_FreeTemp(QCC_PR_Statement (&pr_opcodes[OP_STATE], s1, def, NULL));
}

void QCC_PR_ParseAsm(void)
{
	QCC_dstatement_t *patch1;
	int op, p;
	QCC_def_t *a, *b, *c;

	if (QCC_PR_CheckKeyword(keyword_local, "local"))
	{
		QCC_PR_ParseDefs (NULL);
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

					QCC_PR_Statement3(&pr_opcodes[op], NULL, NULL, NULL, true);

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

					a = QCC_PR_ParseValue(pr_classtype, false);
					QCC_PR_Statement3(&pr_opcodes[op], a, NULL, NULL, true);

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

					a = QCC_PR_ParseValue(pr_classtype, false);
					b = QCC_PR_ParseValue(pr_classtype, false);
					QCC_PR_Statement3(&pr_opcodes[op], a, b, NULL, true);

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
					a = QCC_PR_ParseValue(pr_classtype, false);
				else
					a=NULL;
				if (pr_opcodes[op].type_b != &type_void)
					b = QCC_PR_ParseValue(pr_classtype, false);
				else
					b=NULL;
				if (pr_opcodes[op].associative==ASSOC_LEFT && pr_opcodes[op].type_c != &type_void)
					c = QCC_PR_ParseValue(pr_classtype, false);
				else
					c=NULL;

				QCC_PR_Statement3(&pr_opcodes[op], a, b, c, true);
			}

			QCC_PR_Expect(";");
			return;
		}
	}
	QCC_PR_ParseError(ERR_BADOPCODE, "Bad op code name %s", pr_token);
}

static pbool QCC_FuncJumpsTo(int first, int last, int statement)
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

static pbool QCC_FuncJumpsToRange(int first, int last, int firstr, int lastr)
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
				if (pr_opcodes[statements[st2].op].associative == ASSOC_RIGHT)
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
			}
			if (st2 == last)
			{
				QCC_PR_ParseWarning(WARN_UNREACHABLECODE, "%s: contains unreachable code", pr_scope->name );
			}
			continue;
		}
		if (rettype != ev_void && pr_opcodes[statements[st].op].associative == ASSOC_RIGHT)
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

#define OpAssignsToA(op) false

//follow branches (by recursing).
//stop on first read(error, return statement) or write(no error, return -1)
//end-of-block returns 0, done/return/goto returns -2
int QCC_CheckOneUninitialised(int firststatement, int laststatement, int min, int max)
{
	int ret;
	int i;
	QCC_dstatement32_t *st;

	for (i = firststatement; i < laststatement; i++)
	{
		st = &statements[i];

		if (st->op == OP_DONE || st->op == OP_RETURN)
		{
			if (st->a >= min && st->a < max)
				return i;
			return -2;
		}

//		this code catches gotos, but can cause issues with while statements.
//		if (st->op == OP_GOTO && (int)st->a < 1)
//			return -2;

		if (pr_opcodes[st->op].type_a)
		{
			if (st->a >= min && st->a < max)
			{
				if (OpAssignsToA(st->op))
					return -1;

				return i;
			}
		}
		else if (pr_opcodes[st->op].associative == ASSOC_RIGHT && (int)st->a > 0)
		{
			int jump = i + (int)st->a;
			ret = QCC_CheckOneUninitialised(i + 1, jump, min, max);
			if (ret > 0)
				return ret;
			i = jump-1;
		}

		if (pr_opcodes[st->op].type_b)
		{
			if (st->b >= min && st->b < max)
			{
				if (OpAssignsToB(st->op))
					return -1;

				return i;
			}
		}
		else if (pr_opcodes[st->op].associative == ASSOC_RIGHT && (int)st->b > 0)
		{
			int jump = i + (int)st->b;
			//check if there's an else.
			st = &statements[jump-1];
			if (st->op == OP_GOTO && (int)st->a > 0)
			{
				int jump2 = jump-1 + st->a;
				int rett = QCC_CheckOneUninitialised(i + 1, jump - 1, min, max);
				if (rett > 0)
					return rett;
				ret = QCC_CheckOneUninitialised(jump, jump2, min, max);
				if (ret > 0)
					return ret;
				if (rett < 0 && ret < 0)
					return (rett == ret)?ret:-1;	//inited or aborted in both, don't need to continue along this branch
				i = jump2-1;
			}
			else
			{
				ret = QCC_CheckOneUninitialised(i + 1, jump, min, max);
				if (ret > 0)
					return ret;
				i = jump-1;
			}
			continue;
		}

		if (pr_opcodes[st->op].type_c && st->c >= min && st->c < max)
		{
			if (OpAssignsToC(st->op))
				return -1;

			return i;
		}
		else if (pr_opcodes[st->op].associative == ASSOC_RIGHT && (int)st->c > 0)
		{
			int jump = i + (int)st->c;
			ret = QCC_CheckOneUninitialised(i + 1, jump, min, max);
			if (ret > 0)
				return ret;
			i = jump-1;

			continue;
		}
	}

	return 0;
}

pbool QCC_CheckUninitialised(int firststatement, int laststatement)
{
	QCC_def_t *local;
	int i;
	int min,max;
	pbool result = false;
	int paramend = FIRST_LOCAL;
	QCC_type_t *type = pr_scope->type;
	int err;

	for (type = pr_scope->type, i = type->num_parms, type = type->param; i > 0; i--, type = type->next)
		paramend += type->size;

	for (local = pr.localvars; local; local = local->nextlocal)
	{
		if (local->constant)
			continue;	//will get some other warning, so we don't care.
		if (local->ofs < paramend)
			continue;
		min = local->ofs;
		max = local->ofs + (local->type->size * (local->arraysize?local->arraysize:1));
		err = QCC_CheckOneUninitialised(firststatement, laststatement, min, max);
		if (err > 0)
		{
			QCC_PR_Warning(WARN_UNINITIALIZED, strings+s_file, statement_linenums[err], "Potentially uninitialised variable %s", local->name);
			result = true;
//			break;
		}
	}

	return result;
}

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

void QCC_Marshal_Locals(int firststatement, int laststatement)
{
	QCC_def_t *local;
	pbool error = false;
	int localsused = locals_end - locals_start;

	if (!pr.localvars)	//nothing to marshal
	{
		locals_start = numpr_globals;
		locals_end = numpr_globals;
		return;
	}

	if (!opt_locals_overlapping)
	{
		if (qccwarningaction[WARN_UNINITIALIZED])
			QCC_CheckUninitialised(firststatement, laststatement);
		error = true;	//always use the legacy behaviour
	}
	else if (QCC_CheckUninitialised(firststatement, laststatement))
	{
		error = true;
//		QCC_PR_Note(ERR_INTERNAL, strings+s_file, pr_source_line, "Not overlapping locals from %s due to uninitialised locals", pr_scope->name);
	}
	else
	{
		//make sure we're allowed to marshall this function's locals
		for (local = pr.localvars; local; local = local->nextlocal)
		{
			//FIXME: check for uninitialised locals.
			//these matter when the function goes recursive (and locals marshalling counts as recursive every time).
			if (((int*)qcc_pr_globals)[local->ofs])
			{
				QCC_PR_Note(ERR_INTERNAL, strings+local->s_file, local->s_line, "Marshaling non-const initialised %s", local->name);
				error = true;
			}

			if (local->constant)
			{
				QCC_PR_Note(ERR_INTERNAL, strings+local->s_file, local->s_line, "Marshaling const %s", local->name);
				error = true;
			}
		}
	}

	if (error)
	{
		//move all the locals into a vanilla-style single block, per function.
		locals_start = QCC_GetFreeGlobalOffsetSpace(localsused);
		locals_end = locals_start + localsused;

		QCC_RemapOffsets(firststatement, laststatement, FIRST_LOCAL, FIRST_LOCAL+localsused, locals_start);
		memcpy(qcc_pr_globals+locals_start, qcc_pr_globals+FIRST_LOCAL, localsused*sizeof(float));
		memset(qcc_pr_globals+FIRST_LOCAL, 0, localsused*sizeof(float));
		for (local = pr.localvars; local; local = local->nextlocal)
		{
			if (local->ofs >= FIRST_LOCAL)
				local->ofs = local->ofs - FIRST_LOCAL + locals_start;
		}
		for (local = pr.def_tail; local; local = local->next)
		{
			if (local->scope != pr_scope)
				break;

			if (local->ofs >= FIRST_LOCAL)
				local->ofs = local->ofs - FIRST_LOCAL + locals_start;
		}
	}
	else
	{
//		QCC_PR_Note(ERR_INTERNAL, strings+s_file, pr_source_line, "Overlapping %s", pr_scope->name);
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
			{
				fprintf(asmfile, "\t%s", QCC_VarAtOffset(statements[i].a, (*pr_opcodes[statements[i].op].type_a)->size));
				fprintf(asmfile, "/*%i*/", statements[i].a);
			}
			else
				fprintf(asmfile, "\t%i", statements[i].a);
			if (pr_opcodes[statements[i].op].type_b != &type_void)
			{
				if (pr_opcodes[statements[i].op].type_b)
				{
					fprintf(asmfile, ",\t%s", QCC_VarAtOffset(statements[i].b, (*pr_opcodes[statements[i].op].type_b)->size));
					fprintf(asmfile, "/*%i*/", statements[i].b);
				}
				else
					fprintf(asmfile, ",\t%i", statements[i].b);
				if (pr_opcodes[statements[i].op].type_c != &type_void && pr_opcodes[statements[i].op].associative==ASSOC_LEFT)
				{
					if (pr_opcodes[statements[i].op].type_c)
					{
						fprintf(asmfile, ",\t%s", QCC_VarAtOffset(statements[i].c, (*pr_opcodes[statements[i].op].type_c)->size));
						fprintf(asmfile, "/*%i*/", statements[i].c);
					}
					else
						fprintf(asmfile, ",\t%i", statements[i].c);
				}
			}
			else
			{
				if (pr_opcodes[statements[i].op].type_c != &type_void)
				{
					if (pr_opcodes[statements[i].op].type_c)
					{
						fprintf(asmfile, ",\t%s", QCC_VarAtOffset(statements[i].c, (*pr_opcodes[statements[i].op].type_c)->size));
						fprintf(asmfile, "/*%i*/", statements[i].c);
					}
					else
						fprintf(asmfile, ",\t%i", statements[i].c);
				}
			}
		}
		fprintf(asmfile, "; /*%i*/\n", statement_linenums[i]);
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

	conditional = 0;

	expandedemptymacro = false;

	f = (void *)qccHunkAlloc (sizeof(QCC_function_t));

//
// check for builtin function definition #1, #2, etc
//
// hexenC has void name() : 2;
	if (QCC_PR_CheckToken ("#") || QCC_PR_CheckToken (":"))
	{
		int binum = 0;
		if (pr_token_type == tt_immediate
		&& pr_immediate_type == type_float
		&& pr_immediate._float == (int)pr_immediate._float)
			binum = (int)pr_immediate._float;
		else if (pr_token_type == tt_immediate && pr_immediate_type == type_integer)
			binum = pr_immediate._int;
		else
			QCC_PR_ParseError (ERR_BADBUILTINIMMEDIATE, "Bad builtin immediate");
		f->builtin = binum;
		QCC_PR_Lex ();

		locals_start = locals_end = OFS_PARM0; //hmm...
		return f;
	}
	if (QCC_PR_CheckKeyword(keyword_external, "external"))
	{	//reacc style builtin
		if (pr_token_type != tt_immediate
		|| pr_immediate_type != type_float
		|| pr_immediate._float != (int)pr_immediate._float)
			QCC_PR_ParseError (ERR_BADBUILTINIMMEDIATE, "Bad builtin immediate");
		f->builtin = (int)-pr_immediate._float;
		QCC_PR_Lex ();
		QCC_PR_Expect(";");

		locals_start = locals_end = OFS_PARM0; //hmm...
		return f;
	}

	if (type->num_parms < 0)
		QCC_PR_ParseError (ERR_FUNCTIONWITHVARGS, "QC function with variable arguments and function body");

	f->builtin = 0;
//
// define the parms
//

	locals_start = locals_end = FIRST_LOCAL;

	parm = type->param;
	for (i=0 ; i<type->num_parms ; i++)
	{
		if (!*pr_parm_names[i])
			QCC_PR_ParseError(ERR_PARAMWITHNONAME, "Parameter is not named");
		defs[i] = QCC_PR_GetDef (parm, pr_parm_names[i], pr_scope, true, 0, false);

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

	f->code = numstatements;

	if (type->num_parms > MAX_PARMS)
	{
		for (i = MAX_PARMS; i < type->num_parms; i++)
		{
			if (!extra_parms[i - MAX_PARMS])
			{
				e2 = (QCC_def_t *) qccHunkAlloc (sizeof(QCC_def_t));
				e2->name = "extra parm";
				e2->ofs = QCC_GetFreeGlobalOffsetSpace(3);
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
		e = QCC_PR_GetDef(pr_classtype, "__oself", pr_scope, true, 0);
		e2 = QCC_PR_GetDef(type_entity, "self", NULL, true, 0);
		QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_ENT], QCC_PR_DummyDef(pr_classtype, "self", pr_scope, 0, e2->ofs, false), e, NULL));
	}*/

//
// check for a state opcode
//
	if (QCC_PR_CheckToken ("["))
		QCC_PR_ParseState ();

	if (QCC_PR_CheckKeyword (keyword_asm, "asm"))
	{
		QCC_PR_Expect ("{");
		while (!QCC_PR_CheckToken("}"))
			QCC_PR_ParseAsm ();
	}
	else
	{
		if (QCC_PR_CheckKeyword (keyword_var, "var"))	//reacc support
		{	//parse lots of locals
			char *name;
			do {
				name = QCC_PR_ParseName();
				QCC_PR_Expect(":");
				e2 = QCC_PR_GetDef(QCC_PR_ParseType(false, false), name, pr_scope, true, 0, false);
				QCC_PR_Expect(";");
			} while(!QCC_PR_CheckToken("{"));
		}
		else
			QCC_PR_Expect ("{");
//
// parse regular statements
//
		while (!QCC_PR_CheckToken("}"))
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
			QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_ENT], e, QCC_PR_DummyDef(pr_classtype, "self", pr_scope, 0, e2->ofs, false), NULL));
		}*/

		QCC_PR_Statement (pr_opcodes, 0,0, NULL);
	}
	else
		optres_return_only++;

	QCC_CheckForDeadAndMissingReturns(f->code, numstatements, type->aux_type->type);

//	if (opt_comexprremoval)
//		QCC_CommonSubExpressionRemoval(f->code, numstatements);


	QCC_RemapLockedTemps(f->code, numstatements);
	QCC_WriteAsmFunction(pr_scope, f->code, locals_start);

	QCC_Marshal_Locals(f->code, numstatements);

	if (opt_compound_jumps)
		QCC_CompoundJumps(f->code, numstatements);

	if (num_labels)
		num_labels = 0;


	if (num_continues)
	{
		num_continues=0;
		QCC_PR_ParseError(ERR_ILLEGALCONTINUES, "%s: function contains illegal continues", pr_scope->name);
	}
	if (num_breaks)
	{
		num_breaks=0;
		QCC_PR_ParseError(ERR_ILLEGALBREAKS, "%s: function contains illegal breaks", pr_scope->name);
	}
	if (num_cases)
	{
		num_cases = 0;
		QCC_PR_ParseError(ERR_ILLEGALCASES, "%s: function contains illegal cases", pr_scope->name);
	}

	return f;
}

void QCC_PR_ArrayRecurseDivideRegular(QCC_def_t *array, QCC_def_t *index, int min, int max)
{
	QCC_dstatement_t *st;
	QCC_def_t *eq;
	int stride;

	if (array->type->type == ev_vector)
		stride = 3;
	else
		stride = 1;	//struct arrays should be 1, so that every element can be accessed...

	if (min == max || min+1 == max)
	{
		eq = QCC_PR_Statement(pr_opcodes+OP_LT_F, index, QCC_MakeFloatConst(min+0.5f), NULL);
		QCC_UnFreeTemp(index);
		QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_IFNOT_I, eq, 0, &st));
		st->b = 2;
		QCC_PR_Statement(pr_opcodes+OP_RETURN, 0, 0, &st);
		st->a = array->ofs + min*stride;
	}
	else
	{
		int mid = min + (max-min)/2;

		if (max-min>4)
		{
			eq = QCC_PR_Statement(pr_opcodes+OP_LT_F, index, QCC_MakeFloatConst(mid+0.5f), NULL);
			QCC_UnFreeTemp(index);
			QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_IFNOT_I, eq, 0, &st));
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
		eq = QCC_PR_Statement(pr_opcodes+OP_LT_F, index, QCC_MakeFloatConst(min+0.5f), NULL);
		QCC_UnFreeTemp(index);
		QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_IFNOT_I, eq, 0, &st));
		st->b = 2;
		QCC_PR_Statement(pr_opcodes+OP_RETURN, 0, 0, &st);
		st->a = array->ofs + min*3;
	}
	else
	{
		int mid = min + (max-min)/2;

		if (max-min>4)
		{
			eq = QCC_PR_Statement(pr_opcodes+OP_LT_F, index, QCC_MakeFloatConst(mid+0.5f), NULL);
			QCC_UnFreeTemp(index);
			QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_IFNOT_I, eq, 0, &st));
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

	int numslots;

	//array shouldn't ever be a vector array
	numslots = array->arraysize*array->type->size;
	numslots = (numslots+2)/3;

	func = QCC_PR_GetDef(type_function, qcva("ArrayGetVec*%s", array->name), NULL, true, 0, false);

	pr_scope = func;

	if (numfunctions >= MAX_FUNCTIONS)
		QCC_Error(ERR_INTERNAL, "Too many function defs");

	df = &functions[numfunctions];
	numfunctions++;

	df->s_file = 0;
	df->s_name = QCC_CopyString(func->name);
	df->first_statement = numstatements;
	df->parm_size[0] = 1;
	df->numparms = 1;
	locals_start = locals_end = FIRST_LOCAL;
	index = QCC_PR_GetDef(type_float, "index___", func, true, 0, false);
	index->references++;
	temp = QCC_PR_GetDef(type_float, "div3___", func, true, 0, false);
	QCC_PR_Statement3(pr_opcodes+OP_DIV_F, index, QCC_MakeFloatConst(3), temp, false);
	QCC_PR_Statement3(pr_opcodes+OP_BITAND_F, temp, temp, temp, false);//round down to int

	QCC_PR_ArrayRecurseDivideUsingVectors(array, temp, 0, numslots);

	QCC_PR_Statement(pr_opcodes+OP_RETURN, QCC_MakeFloatConst(0), 0, NULL);	//err... we didn't find it, give up.
	QCC_PR_Statement(pr_opcodes+OP_DONE, 0, 0, NULL);	//err... we didn't find it, give up.

	G_FUNCTION(func->ofs) = df - functions;
	func->initialized = 1;

	QCC_WriteAsmFunction(pr_scope, df->first_statement, df->parm_start);
	QCC_Marshal_Locals(df->first_statement, numstatements);
	QCC_FreeTemps();
	df->parm_start = locals_start;
	df->numparms = locals_end - locals_start;
	return func;
}

void QCC_PR_EmitArrayGetFunction(QCC_def_t *scope, char *arrayname)
{
	QCC_def_t *vectortrick;
	QCC_dfunction_t *df;
	QCC_def_t *def, *index;

	QCC_dstatement_t *st;
	QCC_def_t *eq;

	QCC_def_t *fasttrackpossible;
	int numslots;

	if (flag_fasttrackarrays)
		fasttrackpossible = QCC_PR_GetDef(type_float, "__ext__fasttrackarrays", NULL, true, 0, false);
	else
		fasttrackpossible = NULL;

	def = QCC_PR_GetDef(NULL, arrayname, NULL, false, 0, false);

	if (def->type->type == ev_vector)
		numslots = def->arraysize;
	else
		numslots = def->arraysize*def->type->size;

	if (numslots >= 15 && def->type->type != ev_vector)
		vectortrick = QCC_PR_EmitArrayGetVector(def);
	else
		vectortrick = NULL;

	pr_scope = scope;

	if (numfunctions >= MAX_FUNCTIONS)
		QCC_Error(ERR_INTERNAL, "Too many function defs");

	df = &functions[numfunctions];
	numfunctions++;

	df->s_file = 0;
	df->s_name = QCC_CopyString(scope->name);
	df->first_statement = numstatements;
	df->parm_size[0] = 1;
	df->numparms = 1;
	locals_start = locals_end = FIRST_LOCAL;
	index = QCC_PR_GetDef(type_float, "indexg___", def, true, 0, false);

	G_FUNCTION(scope->ofs) = df - functions;

	if (fasttrackpossible)
	{
		QCC_PR_Statement(pr_opcodes+OP_IFNOT_I, fasttrackpossible, NULL, &st);
		//fetch_gbl takes: (float size, variant array[]), float index, variant pos
		//note that the array size is coded into the globals, one index before the array.

		if (def->type->type == ev_vector)
			QCC_PR_Statement3(&pr_opcodes[OP_FETCH_GBL_V], def, index, &def_ret, true);
		else
			QCC_PR_Statement3(&pr_opcodes[OP_FETCH_GBL_F], def, index, &def_ret, true);

		QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_RETURN], &def_ret, NULL, NULL));

		//finish the jump
		st->b = &statements[numstatements] - st;
	}

	if (vectortrick)
	{
		QCC_def_t *div3, *intdiv3, *ret;

		//okay, we've got a function to retrieve the var as part of a vector.
		//we need to work out which part, x/y/z that it's stored in.
		//0,1,2 = i - ((int)i/3 *) 3;

		div3 = QCC_PR_GetDef(type_float, "div3___", def, true, 0, false);
		intdiv3 = QCC_PR_GetDef(type_float, "intdiv3___", def, true, 0, false);

		eq = QCC_PR_Statement(pr_opcodes+OP_GE_F, index, QCC_MakeFloatConst((float)numslots), NULL);	//escape clause - should call some sort of error function instead.. that'd rule!
		QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_IFNOT_I, eq, 0, &st));
		st->b = 2;
		QCC_PR_Statement(pr_opcodes+OP_RETURN, QCC_MakeFloatConst(0), 0, &st);

		div3->references++;
		QCC_PR_Statement3(pr_opcodes+OP_BITAND_F, index, index, index, false);
		QCC_PR_Statement3(pr_opcodes+OP_DIV_F, index, QCC_MakeFloatConst(3), div3, false);
		QCC_PR_Statement3(pr_opcodes+OP_BITAND_F, div3, div3, intdiv3, false);

		QCC_PR_Statement3(pr_opcodes+OP_STORE_F, index, &def_parms[0], NULL, false);
		QCC_PR_Statement3(pr_opcodes+OP_CALL1, vectortrick, NULL, NULL, false);
		vectortrick->references++;
		ret = QCC_PR_GetDef(type_vector, "vec__", pr_scope, true, 0, false);
		ret->references+=4;
		QCC_PR_Statement3(pr_opcodes+OP_STORE_V, &def_ret, ret, NULL, false);
		QCC_FreeTemp(&def_ret);

		div3 = QCC_PR_Statement(pr_opcodes+OP_MUL_F, intdiv3, QCC_MakeFloatConst(3), NULL);
		QCC_PR_Statement3(pr_opcodes+OP_SUB_F, index, div3, index, false);
		QCC_FreeTemp(div3);

		eq = QCC_PR_Statement(pr_opcodes+OP_LT_F, index, QCC_MakeFloatConst(0+0.5f), NULL);
		QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_IFNOT_I, eq, 0, &st));
		st->b = 2;
		QCC_PR_Statement(pr_opcodes+OP_RETURN, 0, 0, &st);
		st->a = ret->ofs + 0;

		eq = QCC_PR_Statement(pr_opcodes+OP_LT_F, index, QCC_MakeFloatConst(1+0.5f), NULL);
		QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_IFNOT_I, eq, 0, &st));
		st->b = 2;
		QCC_PR_Statement(pr_opcodes+OP_RETURN, 0, 0, &st);
		st->a = ret->ofs + 1;

		eq = QCC_PR_Statement(pr_opcodes+OP_LT_F, index, QCC_MakeFloatConst(2+0.5), NULL);
		QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_IFNOT_I, eq, 0, &st));
		st->b = 2;
		QCC_PR_Statement(pr_opcodes+OP_RETURN, 0, 0, &st);
		st->a = ret->ofs + 2;
		QCC_FreeTemp(ret);
		QCC_FreeTemp(index);
	}
	else
	{
		QCC_PR_Statement3(pr_opcodes+OP_BITAND_F, index, index, index, false);
		QCC_PR_ArrayRecurseDivideRegular(def, index, 0, numslots);
	}

	QCC_PR_Statement(pr_opcodes+OP_RETURN, QCC_MakeFloatConst(0), 0, NULL);

	QCC_PR_Statement(pr_opcodes+OP_DONE, 0, 0, NULL);

	QCC_WriteAsmFunction(pr_scope, df->first_statement, df->parm_start);
	QCC_Marshal_Locals(df->first_statement, numstatements);
	QCC_FreeTemps();
	df->parm_start = locals_start;
	df->numparms = locals_end - locals_start;
}

void QCC_PR_ArraySetRecurseDivide(QCC_def_t *array, QCC_def_t *index, QCC_def_t *value, int min, int max)
{
	QCC_dstatement_t *st;
	QCC_def_t *eq;
	int stride;

	if (array->type->type == ev_vector)
		stride = 3;
	else
		stride = 1;	//struct arrays should be 1, so that every element can be accessed...

	if (min == max || min+1 == max)
	{
		eq = QCC_PR_Statement(pr_opcodes+OP_EQ_F, index, QCC_MakeFloatConst((float)min), NULL);
		QCC_UnFreeTemp(index);
		QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_IFNOT_I, eq, 0, &st));
		st->b = 3;
		if (stride == 3)
			QCC_PR_Statement(pr_opcodes+OP_STORE_V, value, array, &st);
		else
			QCC_PR_Statement(pr_opcodes+OP_STORE_F, value, array, &st);
		st->b = array->ofs + min*stride;
		QCC_PR_Statement(pr_opcodes+OP_RETURN, 0, 0, &st);
	}
	else
	{
		int mid = min + (max-min)/2;

		if (max-min>4)
		{
			eq = QCC_PR_Statement(pr_opcodes+OP_LT_F, index, QCC_MakeFloatConst((float)mid), NULL);
			QCC_UnFreeTemp(index);
			QCC_FreeTemp(QCC_PR_Statement(pr_opcodes+OP_IFNOT_I, eq, 0, &st));
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

	QCC_def_t *fasttrackpossible;
	int numslots;

	if (flag_fasttrackarrays)
		fasttrackpossible = QCC_PR_GetDef(type_float, "__ext__fasttrackarrays", NULL, true, 0, false);
	else
		fasttrackpossible = NULL;

	def = QCC_PR_GetDef(NULL, arrayname, NULL, false, 0, false);
	pr_scope = scope;

	if (def->type->type == ev_vector)
		numslots = def->arraysize;
	else
		numslots = def->arraysize*def->type->size;

	if (numfunctions >= MAX_FUNCTIONS)
		QCC_Error(ERR_INTERNAL, "Too many function defs");

	df = &functions[numfunctions];
	numfunctions++;

	df->s_file = 0;
	df->s_name = QCC_CopyString(scope->name);
	df->first_statement = numstatements;
	df->parm_size[0] = 1;
	df->parm_size[1] = def->type->size;
	df->numparms = 2;
	locals_start = locals_end = FIRST_LOCAL;
	index = QCC_PR_GetDef(type_float, "indexs___", def, true, 0, false);
	value = QCC_PR_GetDef(def->type, "value___", def, true, 0, false);

	G_FUNCTION(scope->ofs) = df - functions;

	if (fasttrackpossible)
	{
		QCC_dstatement_t *st;

		QCC_PR_Statement(pr_opcodes+OP_IFNOT_I, fasttrackpossible, NULL, &st);
		//note that the array size is coded into the globals, one index before the array.

		QCC_PR_Statement3(&pr_opcodes[OP_CONV_FTOI], index, NULL, index, true);	//address stuff is integer based, but standard qc (which this accelerates in supported engines) only supports floats
		QCC_PR_SimpleStatement (OP_BOUNDCHECK, index->ofs, ((int*)qcc_pr_globals)[def->ofs-1]+1, 0, true);//annoy the programmer. :p
		if (def->type->size != 1)//shift it upwards for larger types
			QCC_PR_Statement3(&pr_opcodes[OP_MUL_I], index, QCC_MakeIntConst(def->type->size), index, true);
		QCC_PR_Statement3(&pr_opcodes[OP_GLOBALADDRESS], def, index, index, true);	//comes with built in add
		if (def->type->size >= 3)
			QCC_PR_Statement3(&pr_opcodes[OP_STOREP_V], value, index, NULL, true);	//*b = a
		else
			QCC_PR_Statement3(&pr_opcodes[OP_STOREP_F], value, index, NULL, true);
		QCC_PR_Statement(&pr_opcodes[OP_RETURN], value, NULL, NULL);

		//finish the jump
		st->b = &statements[numstatements] - st;
	}

	QCC_PR_Statement3(pr_opcodes+OP_BITAND_F, index, index, index, false);
	QCC_PR_ArraySetRecurseDivide(def, index, value, 0, numslots);

	QCC_PR_Statement(pr_opcodes+OP_DONE, 0, 0, NULL);



	QCC_WriteAsmFunction(pr_scope, df->first_statement, df->parm_start);
	QCC_Marshal_Locals(df->first_statement, numstatements);
	QCC_FreeTemps();
	df->parm_start = locals_start;
	df->numparms = locals_end - locals_start;
}

//register a def, and all of it's sub parts.
//only the main def is of use to the compiler.
//the subparts are emitted to the compiler and allow correct saving/loading
//be careful with fields, this doesn't allocated space, so will it allocate fields. It only creates defs at specified offsets.
QCC_def_t *QCC_PR_DummyDef(QCC_type_t *type, char *name, QCC_def_t *scope, int arraysize, unsigned int ofs, int referable, unsigned int flags)
{
	char array[64];
	char newname[256];
	int a;
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

	for (a = 0; a < (arraysize?arraysize:1); a++)
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
		def->arraysize = a?0:arraysize;
		if (name)
		{
			pr.def_tail->next = def;
			pr.def_tail = def;
		}

		if (a > 0)
			def->references++;

		def->s_line = pr_source_line;
		def->s_file = s_file;
		if (a)
			def->initialized = 1;

		def->name = (void *)qccHunkAlloc (strlen(newname)+1);
		strcpy (def->name, newname);
		def->type = type;

		def->scope = scope;
		def->saved = !!(flags & GDF_SAVED);
		def->constant = !!(flags & GDF_CONST);
		def->isstatic = !!(flags & GDF_STATIC);

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
					QCC_PR_DummyDef(parttype, newname, scope, 0, ofs + type->size*a + parttype->ofs, false, flags | GDF_CONST);

					sprintf(newname, "%s%s.%s_x", name, array, parttype->name);
					QCC_PR_DummyDef(type_float, newname, scope, 0, ofs + type->size*a + parttype->ofs, false, flags | GDF_CONST);
					sprintf(newname, "%s%s.%s_y", name, array, parttype->name);
					QCC_PR_DummyDef(type_float, newname, scope, 0, ofs + type->size*a + parttype->ofs+1, false, flags | GDF_CONST);
					sprintf(newname, "%s%s.%s_z", name, array, parttype->name);
					QCC_PR_DummyDef(type_float, newname, scope, 0, ofs + type->size*a + parttype->ofs+2, false, flags | GDF_CONST);
					break;

				case ev_float:
				case ev_string:
				case ev_entity:
				case ev_field:
				case ev_pointer:
				case ev_integer:
				case ev_struct:
				case ev_union:
				case ev_variant:	//for lack of any better alternative
					sprintf(newname, "%s%s.%s", name, array, parttype->name);
					QCC_PR_DummyDef(parttype, newname, scope, 0, ofs + type->size*a + parttype->ofs, false, flags);
					break;

				case ev_function:
					sprintf(newname, "%s%s.%s", name, array, parttype->name);
					QCC_PR_DummyDef(parttype, newname, scope, 0, ofs + type->size*a +parttype->ofs, false, flags)->initialized = true;
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
			QCC_PR_DummyDef(type_float, newname, scope, 0, ofs + type->size*a+0, referable, flags | GDF_CONST);
			sprintf(newname, "%s%s_y", name, array);
			QCC_PR_DummyDef(type_float, newname, scope, 0, ofs + type->size*a+1, referable, flags | GDF_CONST);
			sprintf(newname, "%s%s_z", name, array);
			QCC_PR_DummyDef(type_float, newname, scope, 0, ofs + type->size*a+2, referable, flags | GDF_CONST);
		}
		else if (type->type == ev_field)
		{
			if (type->aux_type->type == ev_vector)
			{
				//do the vector thing.
				sprintf(newname, "%s%s_x", name, array);
				QCC_PR_DummyDef(type_floatfield, newname, scope, 0, ofs + type->size*a+0, referable, flags | GDF_CONST);
				sprintf(newname, "%s%s_y", name, array);
				QCC_PR_DummyDef(type_floatfield, newname, scope, 0, ofs + type->size*a+1, referable, flags | GDF_CONST);
				sprintf(newname, "%s%s_z", name, array);
				QCC_PR_DummyDef(type_floatfield, newname, scope, 0, ofs + type->size*a+2, referable, flags | GDF_CONST);
			}
		}
		first->deftail = pr.def_tail;
	}

	if (referable)
	{
		if (!pHash_Get(&globalstable, "end_sys_fields"))
			first->references++;	//anything above needs to be left in, and so warning about not using it is just going to pee people off.
		if (!arraysize && first->type->type != ev_field)
			first->constant = false;
		if (scope)
			pHash_Add(&localstable, first->name, first, qccHunkAlloc(sizeof(bucket_t)));
		else
			pHash_Add(&globalstable, first->name, first, qccHunkAlloc(sizeof(bucket_t)));

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
If arraysize=0, its not an array and has 1 element.
If arraysize>0, its an array and requires array notation
If arraysize<0, its an array with undefined size - GetDef will fail if its not already allocated.
============
*/

QCC_def_t *QCC_PR_GetDef (QCC_type_t *type, char *name, QCC_def_t *scope, pbool allocate, int arraysize, unsigned int flags)
{
	int ofs;
	QCC_def_t		*def;
//	char element[MAX_NAME];
	QCC_def_t *foundstatic = NULL;

	if (!allocate)
		arraysize = -1;

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
				QCC_PR_ParseErrorPrintDef (ERR_TYPEMISMATCHREDEC, def, "Type mismatch on redeclaration of %s. %s, should be %s",name, TypeName(type), TypeName(def->type));
			if (def->arraysize != arraysize && arraysize>=0)
				QCC_PR_ParseErrorPrintDef (ERR_TYPEMISMATCHARRAYSIZE, def, "Array sizes for redecleration of %s do not match",name);
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

		if (def->isstatic && strcmp(strings+def->s_file, strings+s_file))
		{	//warn? or would that be pointless?
			foundstatic = def;
			def = Hash_GetNext(&globalstable, name, def);
			continue;		// in a different function
		}

		if (type && typecmp(def->type, type))
		{
			if (!pr_scope)
			{
				if (typecmp_lax(def->type, type))
				{
					//unequal even when we're lax
					QCC_PR_ParseError (ERR_TYPEMISMATCHREDEC, "Type mismatch on redeclaration of %s. %s, should be %s",name, TypeName(type), TypeName(def->type));
				}
				else
					QCC_PR_ParseWarning (WARN_LAXCAST, "Optional arguments differ on redeclaration of %s. %s, should be %s",name, TypeName(type), TypeName(def->type));
			}
		}
		if (def->arraysize != arraysize && arraysize>=0)
			QCC_PR_ParseErrorPrintDef(ERR_TYPEMISMATCHARRAYSIZE, def, "Array sizes for redecleration of %s do not match",name);
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

	if (pHash_Get != &Hash_Get && !allocate)	//do we want to try case insensative too?
	{
		if (scope)
		{
			def = pHash_Get(&localstable, name);

			while(def)
			{
				if ( def->scope && def->scope != scope)
				{
					def = pHash_GetNext(&localstable, name, def);
					continue;		// in a different function
				}

				if (type && typecmp(def->type, type))
					QCC_PR_ParseError (ERR_TYPEMISMATCHREDEC, "Type mismatch on redeclaration of %s. %s, should be %s",name, TypeName(type), TypeName(def->type));
				if (def->arraysize != arraysize && arraysize>=0)
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


		def = pHash_Get(&globalstable, name);

		while(def)
		{
			if ( def->scope && def->scope != scope)
			{
				def = pHash_GetNext(&globalstable, name, def);
				continue;		// in a different function
			}

			if (def->isstatic && def->s_file != s_file)
			{	//warn? or would that be pointless?
				foundstatic = def;
				def = Hash_GetNext(&globalstable, name, def);
				continue;		// in a different function
			}

			if (type && typecmp(def->type, type))
			{
				if (!pr_scope)
				{
					if (typecmp_lax(def->type, type))
					{
						//unequal even when we're lax
						QCC_PR_ParseError (ERR_TYPEMISMATCHREDEC, "Type mismatch on redeclaration of %s. %s, should be %s",name, TypeName(type), TypeName(def->type));
					}
					else
						QCC_PR_ParseWarning (WARN_LAXCAST, "Optional arguments differ on redeclaration of %s. %s, should be %s",name, TypeName(type), TypeName(def->type));
				}
			}
			if (def->arraysize != arraysize && arraysize>=0)
				QCC_PR_ParseError (ERR_TYPEMISMATCHARRAYSIZE, "Array sizes for redecleration of %s do not match",name);
			if (allocate && scope)
			{
				if (pr_scope)
				{	//warn? or would that be pointless?
					def = pHash_GetNext(&globalstable, name, def);
					continue;		// in a different function
				}

				QCC_PR_ParseWarning (WARN_DUPLICATEDEFINITION, "%s duplicate definition ignored", name);
				QCC_PR_ParsePrintDef(WARN_DUPLICATEDEFINITION, def);
	//			if (!scope)
	//				QCC_PR_ParsePrintDef(def);
			}
			return def;
		}
	}

	if (foundstatic && !allocate)
	{
		QCC_PR_ParseWarning (WARN_DUPLICATEDEFINITION, "%s defined static", name);
		QCC_PR_ParsePrintDef(WARN_DUPLICATEDEFINITION, foundstatic);
	}

	if (!allocate)
		return NULL;

	if (arraysize < 0)
	{
		QCC_PR_ParseError (ERR_ARRAYNEEDSSIZE, "First declaration of array %s with no size",name);
	}

	if (scope)
	{
		def = QCC_PR_GetDef(type, name, NULL, false, arraysize, false);
		if (def)
		{
			QCC_PR_ParseWarning(WARN_SAMENAMEASGLOBAL, "Local \"%s\" defined with name of a global", name);
			QCC_PR_ParsePrintDef(WARN_SAMENAMEASGLOBAL, def);
		}
	}

	ofs = numpr_globals;
	if (arraysize)
	{	//write the array size
		if (scope && !(flags & (GDF_CONST|GDF_STATIC)))
			ofs = QCC_GetFreeLocalOffsetSpace(1 + (type->size	* arraysize));
		else
			ofs = QCC_GetFreeGlobalOffsetSpace(1 + (type->size	* arraysize));

		//An array needs the size written first. This is a hexen2 opcode thing.
		//note that for struct emulation, union and struct arrays, the size is the total size of global slots, rather than array elements
		if (type->type == ev_vector)
			((int *)qcc_pr_globals)[ofs] = arraysize-1;
		else
			((int *)qcc_pr_globals)[ofs] = (arraysize*type->size)-1;
		ofs++;
	}
	else if (scope && !(flags & (GDF_CONST|GDF_STATIC)))
		ofs = QCC_GetFreeLocalOffsetSpace(type->size);
	else
		ofs = QCC_GetFreeGlobalOffsetSpace(type->size);

	def = QCC_PR_DummyDef(type, name, scope, arraysize, ofs, true, flags);

	if (scope && !(flags & (GDF_CONST|GDF_STATIC)))
	{
		def->nextlocal = pr.localvars;
		pr.localvars = def;
	}

	return def;
}

QCC_def_t *QCC_PR_DummyFieldDef(QCC_type_t *type, char *name, QCC_def_t *scope, int arraysize, unsigned int *fieldofs, pbool saved)
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

	for (a = 0; a < (arraysize?arraysize:1); a++)
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

			def->ofs = scope?QCC_GetFreeLocalOffsetSpace(1):QCC_GetFreeGlobalOffsetSpace(1);
			((int *)qcc_pr_globals)[def->ofs] = *fieldofs;
			fieldofs++;
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
					def = QCC_PR_DummyFieldDef(parttype, newname, scope, 1, fieldofs, saved);
					break;
				case ev_float:
				case ev_string:
				case ev_vector:
				case ev_entity:
				case ev_field:
				case ev_pointer:
				case ev_integer:
				case ev_variant:
					if (*name)
						sprintf(newname, "%s%s.%s", name, array, parttype->name);
					else
						sprintf(newname, "%s%s", parttype->name, array);
					ftype = QCC_PR_NewType("FIELD TYPE", ev_field, false);
					ftype->aux_type = parttype;
					if (parttype->type == ev_vector)
						ftype->size = parttype->size;	//vector fields create a _y and _z too, so we need this still.
					def = QCC_PR_GetDef(NULL, newname, scope, false, 0, saved);
					if (!def)
					{
						def = QCC_PR_GetDef(ftype, newname, scope, true, 0, saved);
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
					ftype = QCC_PR_NewType("FIELD TYPE", ev_field, false);
					ftype->aux_type = parttype;
					def = QCC_PR_GetDef(ftype, newname, scope, true, 0, saved);
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
	QCC_PR_DummyFieldDef(pass, "", pr_scope, 1, fields, true);
}


void QCC_PR_ParseInitializerType(int arraysize, QCC_def_t *def, QCC_type_t *type, int offset)
{
	QCC_def_t *tmp;
	int i;

	if (arraysize)
	{
		//arrays go recursive
		QCC_PR_Expect("{");
		for (i = 0; i < arraysize; i++)
		{
			QCC_PR_ParseInitializerType(0, def, type, offset + i*type->size);
			if (!QCC_PR_CheckToken(","))
				break;
		}
		QCC_PR_Expect("}");
	}
	else
	{
		if (type->type == ev_function && pr_token_type == tt_punct)
		{
			/*begin function special case*/
			QCC_def_t *parentfunc = pr_scope;
			QCC_function_t *f;
			QCC_dfunction_t	*df;
			QCC_type_t *parm;

			tmp = NULL;

			def->references++;
			pr_scope = def;
			if (QCC_PR_CheckToken ("#") || QCC_PR_CheckToken (":"))
			{
				int binum = 0;
				if (pr_token_type == tt_immediate
				&& pr_immediate_type == type_float
				&& pr_immediate._float == (int)pr_immediate._float)
					binum = (int)pr_immediate._float;
				else if (pr_token_type == tt_immediate && pr_immediate_type == type_integer)
					binum = pr_immediate._int;
				else
					QCC_PR_ParseError (ERR_BADBUILTINIMMEDIATE, "Bad builtin immediate");
				QCC_PR_Lex();

				if (def->initialized)
				for (i = 0; i < numfunctions; i++)
				{
					if (functions[i].first_statement == -binum)
					{
						tmp = QCC_MakeIntConst(i);
						break;
					}
				}

				if (!tmp)
				{
					f = (void *)qccHunkAlloc (sizeof(QCC_function_t));
					f->builtin = binum;

					locals_start = locals_end = OFS_PARM0; //hmm...
				}
				else
					f = NULL;
			}
			else
				f = QCC_PR_ParseImmediateStatements (type);
			if (!tmp)
			{
				pr_scope = parentfunc;
				tmp = QCC_MakeIntConst(numfunctions);
				f->def = def;

				if (numfunctions >= MAX_FUNCTIONS)
					QCC_Error(ERR_INTERNAL, "Too many function defs");

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
				/*end function special case*/
			}
		}
		else if (type->type == ev_string && QCC_PR_CheckName("_"))
		{
			char trname[128];
			QCC_PR_Expect("(");
			if (pr_token_type != tt_immediate || pr_immediate_type->type != ev_string)
				QCC_PR_ParseError(0, "_() intrinsic accepts only a string immediate");
			tmp = QCC_MakeStringConst(pr_immediate_string);
			QCC_PR_Lex();
			QCC_PR_Expect(")");

			if (!pr_scope || def->constant)
			{
				QCC_def_t *dt;
				sprintf(trname, "dotranslate_%i", ++dotranslate_count);
				dt = QCC_PR_DummyDef(type_string, trname, pr_scope, 0, offset, true, GDF_CONST);
				dt->references = 1;
				dt->constant = 1;
				dt->initialized = 1;
			}
		}
		else if (type->type == ev_struct || type->type == ev_union)
		{
			//structs go recursive
			QCC_type_t *parttype;
			int partnum;
			int parms;
			pbool isunion;
			QCC_PR_Expect("{");

			isunion = ((type)->type == ev_union);
			for (partnum = 0, parttype = (type)->param, parms = (type)->num_parms; partnum < parms; partnum++, parttype = parttype->next)
			{
				QCC_PR_ParseInitializerType(parttype->arraysize, def, parttype, offset + parttype->ofs);
				if (isunion || !QCC_PR_CheckToken(","))
					break;
			}
			QCC_PR_Expect("}");
			return;
		}
		else
		{
			tmp = QCC_PR_Expression(TOP_PRIORITY, EXPR_DISALLOW_COMMA);
			if (typecmp(type, tmp->type))
			{
				/*you can cast from const 0 to anything*/
				if (tmp->type->size == type->size && (tmp->type->type == ev_integer || tmp->type->type == ev_float) && tmp->constant && !G_INT(tmp->ofs))
				{
				}
				/*cast from int->float will convert*/
				else if (type->type == ev_float && tmp->type->type == ev_integer)
					tmp = QCC_PR_Statement (&pr_opcodes[OP_CONV_ITOF], tmp, 0, NULL);
				/*cast from float->int will convert*/
				else if (type->type == ev_integer && tmp->type->type == ev_float)
					tmp = QCC_PR_Statement (&pr_opcodes[OP_CONV_FTOI], tmp, 0, NULL);
				else
					QCC_PR_ParseErrorPrintDef (ERR_BADIMMEDIATETYPE, def, "wrong initializer type");
			}
		}

		if (!pr_scope || def->constant)
		{
			if (!tmp->constant)
				QCC_PR_ParseErrorPrintDef (ERR_BADIMMEDIATETYPE, def, "initializer is not constant");

			if (def->initialized && def->initialized != 3)
			{
				for (i = 0; (unsigned)i < type->size; i++)
					if (G_INT(offset+i) != G_INT(tmp->ofs+i))
						QCC_PR_ParseErrorPrintDef (ERR_REDECLARATION, def, "incompatible redeclaration");
			}
			else
			{
				for (i = 0; (unsigned)i < type->size; i++)
					G_INT(offset+i) = G_INT(tmp->ofs+i);
			}
		}
		else
		{
			QCC_def_t lhs, rhs;
			if (def->initialized)
				QCC_PR_ParseErrorPrintDef (ERR_REDECLARATION, def, "%s initialised twice", def->name);

			memset(&lhs, 0, sizeof(lhs));
			memset(&rhs, 0, sizeof(rhs));
			def->references++;
			for (i = 0; (unsigned)i < type->size; )
			{
				lhs.ofs = offset+i;
				tmp->references++;
				rhs.ofs = tmp->ofs+i;

				if (type->size - i >= 3)
				{
					rhs.type = lhs.type = type_vector;
					QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_V], &rhs, &lhs, NULL));
					i+=3;
				}
				else
				{
					rhs.type = lhs.type = type_float;
					QCC_FreeTemp(QCC_PR_Statement(&pr_opcodes[OP_STORE_F], &rhs, &lhs, NULL));
					i++;
				}
			}
		}
		QCC_FreeTemp(tmp);
	}
}

void QCC_PR_ParseInitializerDef(QCC_def_t *def)
{
	QCC_PR_ParseInitializerType(def->arraysize, def, def->type, def->ofs);
	if (!def->initialized)
		def->initialized = 1;
}

int accglobalsblock;	//0 = error, 1 = var, 2 = function, 3 = objdata
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
	int			i = 0; // warning: ‘i’ may be used uninitialized in this function
	pbool shared=false;
	pbool isstatic=defaultstatic;
	pbool externfnc=false;
	pbool isconstant = false;
	pbool isvar = false;
	pbool noref = defaultnoref;
	pbool nosave = false;
	pbool allocatenew = true;
	pbool inlinefunction = false;
	int arraysize;
	unsigned int gd_flags;

	while (QCC_PR_CheckToken(";"))
		;

	if (QCC_PR_CheckKeyword(keyword_enum, "enum"))
	{
		if (QCC_PR_CheckKeyword(keyword_integer, "integer") || QCC_PR_CheckKeyword(keyword_int, "int"))
		{
			int iv = 0;
			QCC_PR_Expect("{");
			i = 0;
			d = NULL;
			while(1)
			{
				name = QCC_PR_ParseName();
				if (QCC_PR_CheckToken("="))
				{
					if (pr_token_type != tt_immediate && pr_immediate_type->type != ev_integer)
					{
						def = QCC_PR_GetDef(NULL, QCC_PR_ParseName(), NULL, false, 0, false);
						if (def)
						{
							if (!def->constant)
								QCC_PR_ParseError(ERR_NOTANUMBER, "enum - %s is not a constant", def->name);
							else
								iv = G_INT(def->ofs);
						}
						else
							QCC_PR_ParseError(ERR_NOTANUMBER, "enum - not a number");
					}
					else
					{
						iv = pr_immediate._int;
						QCC_PR_Lex();
					}
				}
				def = QCC_MakeIntConst(iv);
				pHash_Add(&globalstable, name, def, qccHunkAlloc(sizeof(bucket_t)));
				iv++;

				if (QCC_PR_CheckToken("}"))
					break;
				QCC_PR_Expect(",");
				if (QCC_PR_CheckToken("}"))
					break; // accept trailing comma
			}
		}
		else
		{
			float fv = 0;
			QCC_PR_CheckKeyword(keyword_float, "float");
			QCC_PR_Expect("{");
			i = 0;
			d = NULL;
			while(1)
			{
				name = QCC_PR_ParseName();
				if (QCC_PR_CheckToken("="))
				{
					if (pr_token_type != tt_immediate && pr_immediate_type->type != ev_float)
					{
						def = QCC_PR_GetDef(NULL, QCC_PR_ParseName(), NULL, false, 0, false);
						if (def)
						{
							if (!def->constant)
								QCC_PR_ParseError(ERR_NOTANUMBER, "enum - %s is not a constant", def->name);
							else
								fv = G_FLOAT(def->ofs);
						}
						else
							QCC_PR_ParseError(ERR_NOTANUMBER, "enum - not a number");
					}
					else
					{
						fv = pr_immediate._float;
						QCC_PR_Lex();
					}
				}
				def = QCC_MakeFloatConst(fv);
				pHash_Add(&globalstable, name, def, qccHunkAlloc(sizeof(bucket_t)));
				fv++;

				if (QCC_PR_CheckToken("}"))
					break;
				QCC_PR_Expect(",");
				if (QCC_PR_CheckToken("}"))
					break; // accept trailing comma
			}
		}
		QCC_PR_Expect(";");
		return;
	}

	if (QCC_PR_CheckKeyword(keyword_enumflags, "enumflags"))
	{
		int bits;

		if (QCC_PR_CheckKeyword(keyword_integer, "integer") || QCC_PR_CheckKeyword(keyword_int, "int"))
		{
			int iv = 1;
			QCC_PR_Expect("{");
			i = 0;
			d = NULL;
			while(1)
			{
				name = QCC_PR_ParseName();
				if (QCC_PR_CheckToken("="))
				{
					if (pr_token_type != tt_immediate && pr_immediate_type->type != ev_integer)
					{
						def = QCC_PR_GetDef(NULL, QCC_PR_ParseName(), NULL, false, 0, false);
						if (def)
						{
							if (!def->constant)
								QCC_PR_ParseError(ERR_NOTANUMBER, "enumflags - %s is not a constant", def->name);
							else
								iv = G_INT(def->ofs);
						}
						else
							QCC_PR_ParseError(ERR_NOTANUMBER, "enumflags - not a number");
					}
					else
					{
						iv = pr_immediate._int;
						QCC_PR_Lex();
					}
				}

				bits = 0;
				i = (int)iv;
				if (i != iv)
					QCC_PR_ParseWarning(WARN_ENUMFLAGS_NOTINTEGER, "enumflags - %f not an integer", iv);
				else
				{
					while(i)
					{
						if (((i>>1)<<1) != i)
							bits++;
						i>>=1;
					}
					if (bits > 1)
						QCC_PR_ParseWarning(WARN_ENUMFLAGS_NOTBINARY, "enumflags - value %i not a single bit", (int)iv);
				}

				def = QCC_MakeIntConst(iv);
				pHash_Add(&globalstable, name, def, qccHunkAlloc(sizeof(bucket_t)));

				iv*=2;

				if (QCC_PR_CheckToken("}"))
					break;
				QCC_PR_Expect(",");
			}
		}
		else
		{
			float fv = 1;
			QCC_PR_CheckKeyword(keyword_float, "float");

			QCC_PR_Expect("{");
			i = 0;
			d = NULL;
			while(1)
			{
				name = QCC_PR_ParseName();
				if (QCC_PR_CheckToken("="))
				{
					if (pr_token_type != tt_immediate && pr_immediate_type->type != ev_float)
					{
						def = QCC_PR_GetDef(NULL, QCC_PR_ParseName(), NULL, false, 0, false);
						if (def)
						{
							if (!def->constant)
								QCC_PR_ParseError(ERR_NOTANUMBER, "enumflags - %s is not a constant", def->name);
							else
								fv = G_FLOAT(def->ofs);
						}
						else
							QCC_PR_ParseError(ERR_NOTANUMBER, "enumflags - not a number");
					}
					else
					{
						fv = pr_immediate._float;
						QCC_PR_Lex();
					}
				}

				bits = 0;
				i = (int)fv;
				if (i != fv)
					QCC_PR_ParseWarning(WARN_ENUMFLAGS_NOTINTEGER, "enumflags - %f not an integer", fv);
				else
				{
					while(i)
					{
						if (((i>>1)<<1) != i)
							bits++;
						i>>=1;
					}
					if (bits > 1)
						QCC_PR_ParseWarning(WARN_ENUMFLAGS_NOTBINARY, "enumflags - value %i not a single bit", (int)fv);
				}

				def = QCC_MakeFloatConst(fv);
				pHash_Add(&globalstable, name, def, qccHunkAlloc(sizeof(bucket_t)));

				fv*=2;

				if (QCC_PR_CheckToken("}"))
					break;
				QCC_PR_Expect(",");
			}
		}
		QCC_PR_Expect(";");
		return;
	}

	if (QCC_PR_CheckKeyword (keyword_typedef, "typedef"))
	{
		type = QCC_PR_ParseType(true, false);
		if (!type)
		{
			QCC_PR_ParseError(ERR_NOTANAME, "typedef found unexpected tokens");
		}
		type->name = QCC_CopyString(pr_token)+strings;
		type->typedefed = true;
		QCC_PR_Lex();
		QCC_PR_Expect(";");
		return;
	}

	if (flag_acc)
	{
		char *oldp;
		if (QCC_PR_CheckKeyword (keyword_codesys, "CodeSys"))	//reacc support.
		{
			if (ForcedCRC)
				QCC_PR_ParseError(ERR_BADEXTENSION, "progs crc was already specified - only one is allowed");
			ForcedCRC = (int)pr_immediate._float;
			QCC_PR_Lex();
			QCC_PR_Expect(";");
			return;
		}

		oldp = pr_file_p;
		if (QCC_PR_CheckKeyword (keyword_var, "var"))	//reacc support.
		{
			if (accglobalsblock == 3)
			{
				if (!QCC_PR_GetDef(type_void, "end_sys_fields", NULL, false, 0, false))
					QCC_PR_GetDef(type_void, "end_sys_fields", NULL, true, 0, false);
			}

			QCC_PR_ParseName();
			if (QCC_PR_CheckToken(":"))
				accglobalsblock = 1;
			pr_file_p = oldp;
			QCC_PR_Lex();
		}

		if (QCC_PR_CheckKeyword (keyword_function, "function"))	//reacc support.
		{
			accglobalsblock = 2;
		}
		if (QCC_PR_CheckKeyword (keyword_objdata, "objdata"))	//reacc support.
		{
			if (accglobalsblock == 3)
			{
				if (!QCC_PR_GetDef(type_void, "end_sys_fields", NULL, false, 0, false))
					QCC_PR_GetDef(type_void, "end_sys_fields", NULL, true, 0, false);
			}
			else
				if (!QCC_PR_GetDef(type_void, "end_sys_globals", NULL, false, 0, false))
					QCC_PR_GetDef(type_void, "end_sys_globals", NULL, true, 0, false);
			accglobalsblock = 3;
		}
	}

	if (!pr_scope)
	switch(accglobalsblock)//reacc support.
	{
	case 1:
		{
		char *oldp = pr_file_p;
		name = QCC_PR_ParseName();
		if (!QCC_PR_CheckToken(":"))	//nope, it wasn't!
		{
			QCC_PR_IncludeChunk(name, true, NULL);
			QCC_PR_Lex();
			QCC_PR_UnInclude();
			pr_file_p = oldp;
			break;
		}
		if (QCC_PR_CheckKeyword(keyword_object, "object"))
			QCC_PR_GetDef(type_entity, name, NULL, true, 0, true);
		else if (QCC_PR_CheckKeyword(keyword_string, "string"))
			QCC_PR_GetDef(type_string, name, NULL, true, 0, true);
		else if (QCC_PR_CheckKeyword(keyword_real, "real"))
		{
			def = QCC_PR_GetDef(type_float, name, NULL, true, 0, true);
			if (QCC_PR_CheckToken("="))
			{
				G_FLOAT(def->ofs) = pr_immediate._float;
				QCC_PR_Lex();
			}
		}
		else if (QCC_PR_CheckKeyword(keyword_vector, "vector"))
		{
			def = QCC_PR_GetDef(type_vector, name, NULL, true, 0, true);
			if (QCC_PR_CheckToken("="))
			{
				QCC_PR_Expect("[");
				G_FLOAT(def->ofs+0) = pr_immediate._float;
				QCC_PR_Lex();
				G_FLOAT(def->ofs+1) = pr_immediate._float;
				QCC_PR_Lex();
				G_FLOAT(def->ofs+2) = pr_immediate._float;
				QCC_PR_Lex();
				QCC_PR_Expect("]");
			}
		}
		else if (QCC_PR_CheckKeyword(keyword_pfunc, "pfunc"))
			QCC_PR_GetDef(type_function, name, NULL, true, 0, true);
		else
			QCC_PR_ParseError(ERR_BADNOTTYPE, "Bad type\n");
		QCC_PR_Expect (";");

		if (QCC_PR_CheckKeyword (keyword_system, "system"))
			QCC_PR_Expect (";");
		return;
		}
	case 2:
		name = QCC_PR_ParseName();
		QCC_PR_GetDef(type_function, name, NULL, true, 1, true);
		QCC_PR_CheckToken (";");
		return;
	case 3:
		{
		char *oldp = pr_file_p;
		name = QCC_PR_ParseName();
		if (!QCC_PR_CheckToken(":"))	//nope, it wasn't!
		{
			QCC_PR_IncludeChunk(name, true, NULL);
			QCC_PR_Lex();
			QCC_PR_UnInclude();
			pr_file_p = oldp;
			break;
		}
		if (QCC_PR_CheckKeyword(keyword_object, "object"))
			QCC_PR_GetDef(QCC_PR_FieldType(type_entity), name, NULL, true, 0, true);
		else if (QCC_PR_CheckKeyword(keyword_string, "string"))
			QCC_PR_GetDef(QCC_PR_FieldType(type_string), name, NULL, true, 0, true);
		else if (QCC_PR_CheckKeyword(keyword_real, "real"))
			QCC_PR_GetDef(QCC_PR_FieldType(type_float), name, NULL, true, 0, true);
		else if (QCC_PR_CheckKeyword(keyword_vector, "vector"))
			QCC_PR_GetDef(QCC_PR_FieldType(type_vector), name, NULL, true, 0, true);
		else if (QCC_PR_CheckKeyword(keyword_pfunc, "pfunc"))
			QCC_PR_GetDef(QCC_PR_FieldType(type_function), name, NULL, true, 0, true);
		else
			QCC_PR_ParseError(ERR_BADNOTTYPE, "Bad type\n");
		QCC_PR_Expect (";");
		return;
		}
	}

	while(1)
	{
		if (QCC_PR_CheckKeyword(keyword_extern, "extern"))
			externfnc=true;
		else if (QCC_PR_CheckKeyword(keyword_shared, "shared"))
		{
			shared=true;
			if (pr_scope)
				QCC_PR_ParseError (ERR_NOSHAREDLOCALS, "Cannot have shared locals");
		}
		else if (QCC_PR_CheckKeyword(keyword_const, "const"))
			isconstant = true;
		else if (QCC_PR_CheckKeyword(keyword_var, "var"))
			isvar = true;
		else if (!pr_scope && QCC_PR_CheckKeyword(keyword_var, "static"))
			isstatic = true;
		else if (!pr_scope && QCC_PR_CheckKeyword(keyword_var, "nonstatic"))
			isstatic = false;
		else if (QCC_PR_CheckKeyword(keyword_noref, "noref"))
			noref=true;
		else if (QCC_PR_CheckKeyword(keyword_nosave, "nosave"))
			nosave = true;
		else
			break;
	}

	type = QCC_PR_ParseType (false, false);
	if (type == NULL)	//ignore
		return;

	inlinefunction = type_inlinefunction;

	if (externfnc && type->type != ev_function)
	{
		printf ("Only functions may be defined as external (yet)\n");
		externfnc=false;
	}

	if (!pr_scope && QCC_PR_CheckKeyword(keyword_function, "function"))	//reacc support.
	{
		name = QCC_PR_ParseName ();
		QCC_PR_Expect("(");
		type = QCC_PR_ParseFunctionTypeReacc(false, type);
		QCC_PR_Expect(";");
		def = QCC_PR_GetDef (type, name, NULL, true, 0, false);

		if (autoprototype)
		{	//ignore the code and stuff

			if (QCC_PR_CheckKeyword(keyword_external, "external"))
			{	//builtin
				QCC_PR_Lex();
				QCC_PR_Expect(";");
			}
			else
			{
				int blev = 1;

				while (!QCC_PR_CheckToken("{"))	//skip over the locals.
				{
					if (pr_token_type == tt_eof)
					{
						QCC_PR_ParseError(0, "Unexpected EOF");
						break;
					}
					QCC_PR_Lex();
				}

				//balance out the { and }
				while(blev)
				{
					if (pr_token_type == tt_eof)
						break;
					if (QCC_PR_CheckToken("{"))
						blev++;
					else if (QCC_PR_CheckToken("}"))
						blev--;
					else
						QCC_PR_Lex();	//ignore it.
				}
			}
			return;
		}

		def->references++;

		pr_scope = def;
		f = QCC_PR_ParseImmediateStatements (type);
		pr_scope = NULL;
		def->initialized = 1;
		def->isstatic = isstatic;
		G_FUNCTION(def->ofs) = numfunctions;
		f->def = def;
//				if (pr_dumpasm)
//					PR_PrintFunction (def);

		if (numfunctions >= MAX_FUNCTIONS)
			QCC_Error(ERR_INTERNAL, "Too many function defs");

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

		return;
	}

//	if (pr_scope && (type->type == ev_field) )
//		QCC_PR_ParseError ("Fields must be global");

	do
	{		
		if (QCC_PR_CheckToken (";"))
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
		}
		else
		{
			name = QCC_PR_ParseName ();
		}

		if (QCC_PR_CheckToken("::") && !classname)
		{
			classname = name;
			name = QCC_PR_ParseName();
		}

//check for an array

		if ( QCC_PR_CheckToken ("[") )
		{
			char *oldprfile = pr_file_p;
			int depth;
			arraysize = 0;
			if (QCC_PR_CheckToken("]"))
			{
				QCC_PR_Expect("=");
				QCC_PR_Expect("{");
				arraysize++;
				depth = 1;
				while(1)
				{
					if(pr_token_type == tt_eof)
					{
						QCC_PR_ParseError (ERR_EOF, "EOF inside definition of %s", name);
						break;
					}
					else if (depth == 1 && QCC_PR_CheckToken(","))
						arraysize++;
					else if (QCC_PR_CheckToken("{") || QCC_PR_CheckToken("["))
						depth++;
					else if (QCC_PR_CheckToken("}") || QCC_PR_CheckToken("]"))
					{
						depth--;
						if (depth == 0)
							break;
					}
					else
						QCC_PR_Lex();
				}
				pr_file_p = oldprfile;
				QCC_PR_Lex();
			}
			else
			{
				arraysize = QCC_PR_IntConstExpr();
				QCC_PR_Expect("]");
			}

			if (arraysize < 1)
			{
				QCC_PR_ParseError (ERR_BADARRAYSIZE, "Definition of array (%s) size is not of a numerical value", name);
				arraysize=1;	//grrr...
			}
		}
		else
			arraysize = 0;

		if (QCC_PR_CheckToken("("))
		{
			if (inlinefunction)
				QCC_PR_ParseWarning(WARN_UNSAFEFUNCTIONRETURNTYPE, "Function returning function. Is this what you meant? (suggestion: use typedefs)");
			inlinefunction = false;
			type = QCC_PR_ParseFunctionType(false, type);
		}

		if (classname)
		{
			char *membername = name;
			name = qccHunkAlloc(strlen(classname) + strlen(name) + 3);
			sprintf(name, "%s::"MEMBERFIELDNAME, classname, membername);
			if (!QCC_PR_GetDef(NULL, name, NULL, false, 0, false))
				QCC_PR_ParseError(ERR_NOTANAME, "%s %s is not a member of class %s\n", TypeName(type), membername, classname);
			sprintf(name, "%s::%s", classname, membername);

			pr_classtype = QCC_TypeForName(classname);
			if (!pr_classtype || !pr_classtype->parentclass)
				QCC_PR_ParseError(ERR_NOTANAME, "%s is not a class\n", classname);
		}
		else
			pr_classtype = NULL;

		gd_flags = 0;
		if (isstatic)
			gd_flags |= GDF_STATIC;
		if (isconstant)
			gd_flags |= GDF_CONST;
		if (!nosave)
			gd_flags |= GDF_SAVED;

		def = QCC_PR_GetDef (type, name, pr_scope, allocatenew, arraysize, gd_flags);

		if (!def)
			QCC_PR_ParseError(ERR_NOTANAME, "%s is not part of class %s", name, classname);

		if (noref)
		{
			if (type->type == ev_function && !def->initialized)
				def->initialized = 3;
			def->references++;
		}

		if (!def->initialized && shared)	//shared count as initiialised
		{
			def->shared = shared;
			def->initialized = true;
		}
		if (externfnc)
			def->initialized = 2;

		if (isstatic)
		{
			if (!strcmp(strings+def->s_file, strings+s_file))
				def->isstatic = isstatic;
			else //if (type->type != ev_function && defaultstatic)	//functions don't quite consitiute a definition
				QCC_PR_ParseErrorPrintDef (ERR_REDECLARATION, def, "can't redefine non-static as static");
		}

// check for an initialization
		if (type->type == ev_function && (pr_scope))
		{
			if ( QCC_PR_CheckToken ("=") )
			{
				QCC_PR_ParseError (ERR_INITIALISEDLOCALFUNCTION, "local functions may not be initialised");
			}

			d = def;
			while (d != def->deftail)
			{
				d = d->next;
				d->initialized = 1;	//fake function
				G_FUNCTION(d->ofs) = 0;
			}

			continue;
		}

		if (type->type == ev_field && QCC_PR_CheckName ("alias"))
		{
			QCC_PR_ParseError(ERR_INTERNAL, "FTEQCC does not support this variant of decompiled hexenc\nPlease obtain the original version released by Raven Software instead.");
			name = QCC_PR_ParseName();
		}
		else if ( QCC_PR_CheckToken ("=") || ((type->type == ev_function) && (pr_token[0] == '{' || pr_token[0] == '[' || pr_token[0] == ':')))	//this is an initialisation (or a function)
		{
			if (def->shared)
				QCC_PR_ParseError (ERR_SHAREDINITIALISED, "shared values may not be assigned an initial value", name);
			if (def->initialized == 1)
			{
//				if (def->type->type == ev_function)
//				{
//					i = G_FUNCTION(def->ofs);
//					df = &functions[i];
//					QCC_PR_ParseErrorPrintDef (ERR_REDECLARATION, def, "%s redeclared, prev instance is in %s", name, strings+df->s_file);
//				}
//				else
//					QCC_PR_ParseErrorPrintDef(ERR_REDECLARATION, def, "%s redeclared", name);
			}

			if (autoprototype)
			{	//ignore the code and stuff
				if (QCC_PR_CheckToken("["))
				{
					while (!QCC_PR_CheckToken("]"))
					{
						if (pr_token_type == tt_eof)
							break;
						QCC_PR_Lex();
					}
				}
				if (QCC_PR_CheckToken("{"))
				{
					int blev = 1;
					//balance out the { and }
					while(blev)
					{
						if (pr_token_type == tt_eof)
							break;
						if (QCC_PR_CheckToken("{"))
							blev++;
						else if (QCC_PR_CheckToken("}"))
							blev--;
						else
							QCC_PR_Lex();	//ignore it.
					}
				}
				else
				{
					QCC_PR_CheckToken("#");
					QCC_PR_Lex();
				}
				continue;
			}

			def->constant = (isconstant || (!isvar && !pr_scope));
			QCC_PR_ParseInitializerDef(def);
		}
		else
		{
			if (type->type == ev_function && isvar)
			{
				isconstant = !isvar;
				def->initialized = 1;
			}

			if (type->type == ev_field)
			{
				//fields are const by default, even when not initialised (as they are initialised behind the scenes)
				if (isconstant)
					def->constant = 2;	//special flag on fields, 2, makes the pointer obtained from them also constant.
				else if (isvar || (pr_scope && !isstatic))
					def->constant = 0;
				else
					def->constant = 1;

				if (def->constant)
				{
					unsigned int i;
					for (i = 0; i < type->size*(arraysize?arraysize:1); i++)	//make arrays of fields work.
						*(int *)&qcc_pr_globals[def->ofs+i] = pr.size_fields+i;

					pr.size_fields += i;
				}
			}
			else
				def->constant = isconstant;
		}

		d = def;
		while (d != def->deftail)
		{
			d = d->next;
			d->constant = def->constant;
			d->initialized = def->initialized;
		}
	} while (QCC_PR_CheckToken (","));

	if (type->type == ev_function)
		QCC_PR_CheckToken (";");
	else
	{
		if (!QCC_PR_CheckToken (";"))
			QCC_PR_ParseWarning(WARN_UNDESIRABLECONVENTION, "Missing semicolon at end of definition");
	}
}

/*
============
PR_CompileFile

compiles the 0 terminated text, adding defintions to the pr structure
============
*/
pbool	QCC_PR_CompileFile (char *string, char *filename)
{
	jmp_buf oldjb;
	if (!pr.memory)
		QCC_Error (ERR_INTERNAL, "PR_CompileFile: Didn't clear");

	QCC_PR_ClearGrabMacros (true);	// clear the frame macros

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

	memcpy(&oldjb, &pr_parse_abort, sizeof(oldjb));

	if( setjmp( pr_parse_abort ) ) {
		// dont count it as error
	} else {
		//clock up the first line
		QCC_PR_NewLine (false);

		QCC_PR_Lex ();	// read first token
	}

	while (pr_token_type != tt_eof)
	{
		if (setjmp(pr_parse_abort))
		{
			if (++pr_error_count > MAX_ERRORS)
			{
				memcpy(&pr_parse_abort, &oldjb, sizeof(oldjb));
				return false;
			}
			num_continues = 0;
			num_breaks = 0;
			num_cases = 0;
			QCC_PR_SkipToSemicolon ();
			if (pr_token_type == tt_eof)
			{
				memcpy(&pr_parse_abort, &oldjb, sizeof(oldjb));
				return false;
			}
		}

		pr_scope = NULL;	// outside all functions

		QCC_PR_ParseDefs (NULL);
	}
	memcpy(&pr_parse_abort, &oldjb, sizeof(oldjb));

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

	ocompilingfile = compilingfile;
	os_file = s_file;
	os_file2 = s_file2;
	opr_source_line = pr_source_line;
	opr_file_p = pr_file_p;
	oldcurrentchunk = currentchunk;

	strcpy(fname, filename);
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
