#define COMPILER
#define PROGSUSED

//#define COMMONINLINES
//#define inline _inline

#include "cmdlib.h"
#include <setjmp.h>
/*
#include <stdio.h>
#include <conio.h>


#include "pr_comp.h"
*/

//this is for testing
#define WRITEASM

#ifdef __MINGW32_VERSION
#define MINGW
#endif

#define progfuncs qccprogfuncs
extern progfuncs_t *qccprogfuncs;

#ifndef _WIN32
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

void *qccHunkAlloc(size_t mem);
void qccClearHunk(void);

extern short   (*PRBigShort) (short l);
extern short   (*PRLittleShort) (short l);
extern int     (*PRBigLong) (int l);
extern int     (*PRLittleLong) (int l);
extern float   (*PRBigFloat) (float l);
extern float   (*PRLittleFloat) (float l);


#define	MAX_ERRORS		10

#define	MAX_NAME		64		// chars long

extern unsigned int MAX_REGS;

extern int	MAX_STRINGS;
extern int	MAX_GLOBALS;
extern int	MAX_FIELDS;
extern int	MAX_STATEMENTS;
extern int	MAX_FUNCTIONS;

#define	MAX_SOUNDS		1024	//convert to int?
#define MAX_TEXTURES	1024	//convert to int?
#define	MAX_MODELS		1024	//convert to int?
#define	MAX_FILES		1024	//convert to int?
#define	MAX_DATA_PATH	64

extern int MAX_CONSTANTS;
#define MAXCONSTANTLENGTH 64
#define MAXCONSTANTVALUELENGTH 1024
#define MAXCONSTANTPARAMLENGTH 32
#define MAXCONSTANTPARAMS 32

typedef enum {QCF_STANDARD, QCF_HEXEN2, QCF_DARKPLACES, QCF_FTE, QCF_FTEDEBUG, QCF_KK7} qcc_targetformat_t;
extern qcc_targetformat_t qcc_targetformat;


/*

TODO:

"stopped at 10 errors"

other pointer types for models and clients?

compact string heap?

always initialize all variables to something safe

the def->type->type arrangement is really silly.

return type checking

parm count type checking

immediate overflow checking

pass the first two parms in call->b and call->c

*/

/*

comments
--------
// comments discard text until the end of line
/ *  * / comments discard all enclosed text (spaced out on this line because this documentation is in a regular C comment block, and typing them in normally causes a parse error)

code structure
--------------
A definition is:
	<type> <name> [ = <immediate>] {, <name> [ = <immediate>] };


types
-----
simple types: void, float, vector, string, or entity
	float		width, height;
	string		name;
	entity		self, other;

vector types:
	vector		org;	// also creates org_x, org_y, and org_z float defs
	
	
A function type is specified as: 	simpletype ( type name {,type name} )
The names are ignored except when the function is initialized.	
	void()		think;
	entity()	FindTarget;
	void(vector destination, float speed, void() callback)	SUB_CalcMove;
	void(...)	dprint;		// variable argument builtin

A field type is specified as:  .type
	.vector		origin;
	.string		netname;
	.void()		think, touch, use;
	

names
-----
Names are a maximum of 64 characters, must begin with A-Z,a-z, or _, and can continue with those characters or 0-9.

There are two levels of scoping: global, and function.  The parameter list of a function and any vars declared inside a function with the "local" statement are only visible within that function, 


immediates
----------
Float immediates must begin with 0-9 or minus sign.  .5 is illegal.
	
A parsing ambiguity is present with negative constants. "a-5" will be parsed as "a", then "-5", causing an error.  Seperate the - from the digits with a space "a - 5" to get the proper behavior.
	12
	1.6
	0.5
	-100

Vector immediates are three float immediates enclosed in single quotes.
	'0 0 0'
	'20.5 -10 0.00001'
	
String immediates are characters enclosed in double quotes.  The string cannot contain explicit newlines, but the escape character \n can embed one.  The \" escape can be used to include a quote in the string.
	"maps/jrwiz1.bsp"
	"sound/nin/pain.wav"
	"ouch!\n"

Code immediates are statements enclosed in {} braces.
statement:
	{ <multiple statements> }
	<expression>;
	local <type> <name> [ = <immediate>] {, <name> [ = <immediate>] };
	return <expression>;
	if ( <expression> ) <statement> [ else <statement> ];
	while ( <expression> ) <statement>;
	do <statement> while ( <expression> );
	<function name> ( <function parms> );
	
expression:
	combiations of names and these operators with standard C precedence:
	"&&", "||", "<=", ">=","==", "!=", "!", "*", "/", "-", "+", "=", ".", "<", ">", "&", "|"
	Parenthesis can be used to alter order of operation.
	The & and | operations perform integral bit ops on floats
	
A built in function immediate is a number sign followed by an integer.
	#1
	#12


compilation
-----------
Source files are processed sequentially without dumping any state, so if a defs file is the first one processed, the definitions will be available to all other files.

The language is strongly typed and there are no casts.

Anything that is initialized is assumed to be constant, and will have immediates folded into it.  If you change the value, your program will malfunction.  All uninitialized globals will be saved to savegame files.

Functions cannot have more than eight parameters.

Error recovery during compilation is minimal.  It will skip to the next global definition, so you will never see more than one error at a time in a given function.  All compilation aborts after ten error messages.

Names can be defined multiple times until they are defined with an initialization, allowing functions to be prototyped before their definition.

void()	MyFunction;			// the prototype

void()	MyFunction =		// the initialization
{
	dprint ("we're here\n");
};


entities and fields
-------------------


execution
---------
Code execution is initiated by C code in quake from two main places:  the timed think routines for periodic control, and the touch function when two objects impact each other.

There are three global variables that are set before beginning code execution:
	entity	world;		// the server's world object, which holds all global
						// state for the server, like the deathmatch flags
						// and the body ques.
	entity	self;		// the entity the function is executing for
	entity	other;		// the other object in an impact, not used for thinks
	float	time;		// the current game time.  Note that because the
						// entities in the world are simulated sequentially,
						// time is NOT strictly increasing.  An impact late
						// in one entity's time slice may set time higher
						// than the think function of the next entity. 
						// The difference is limited to 0.1 seconds.
Execution is also caused by a few uncommon events, like the addition of a new client to an existing server.
	
There is a runnaway counter that stops a program if 100000 statements are executed, assuming it is in an infinite loop.

It is acceptable to change the system set global variables.  This is usually done to pose as another entity by changing self and calling a function.

The interpretation is fairly efficient, but it is still over an order of magnitude slower than compiled C code.  All time consuming operations should be made into built in functions.

A profile counter is kept for each function, and incremented for each interpreted instruction inside that function.  The "profile" console command in Quake will dump out the top 10 functions, then clear all the counters.  The "profile all" command will dump sorted stats for every function that has been executed.


afunc ( 4, bfunc(1,2,3));
will fail because there is a shared parameter marshaling area, which will cause the 1 from bfunc to overwrite the 4 already placed in parm0.  When a function is called, it copies the parms from the globals into it's privately scoped variables, so there is no collision when calling another function.

total = factorial(3) + factorial(4);
Will fail because the return value from functions is held in a single global area.  If this really gets on your nerves, tell me and I can work around it at a slight performance and space penalty by allocating a new register for the function call and copying it out.


built in functions
------------------
void(string text)	dprint;
Prints the string to the server console.

void(entity client, string text)	cprint;
Prints a message to a specific client.

void(string text)	bprint;
Broadcast prints a message to all clients on the current server.

entity()	spawn;
Returns a totally empty entity.  You can manually set everything up, or just set the origin and call one of the existing entity setup functions.

entity(entity start, .string field, string match) find;
Searches the server entity list beginning at start, looking for an entity that has entity.field = match.  To start at the beginning of the list, pass world.  World is returned when the end of the list is reached.

<FIXME: define all the other functions...>


gotchas
-------

The && and || operators DO NOT EARLY OUT like C!

Don't confuse single quoted vectors with double quoted strings

The function declaration syntax takes a little getting used to.

Don't forget the ; after the trailing brace of a function initialization.

Don't forget the "local" before defining local variables.

There are no ++ / -- operators, or operate/assign operators.

*/


#if 1
#include "hash.h"
extern hashtable_t compconstantstable;
extern hashtable_t globalstable, localstable;
#endif

#ifdef WRITEASM
FILE *asmfile;
#endif
//=============================================================================

// offsets are always multiplied by 4 before using
typedef unsigned int	gofs_t;				// offset in global data block
typedef struct QCC_function_s QCC_function_t;

#define	MAX_PARMS	8

typedef struct QCC_type_s
{
	etype_t			type;

	struct QCC_type_s	*parentclass;	//type_entity...
	struct QCC_type_s	*next;
// function types are more complex
	struct QCC_type_s	*aux_type;	// return type or field type
	struct QCC_type_s	*param;
	int				num_parms;	// -1 = variable args
//	struct QCC_type_s	*parm_types[MAX_PARMS];	// only [num_parms] allocated	

	unsigned int ofs;	//inside a structure.
	unsigned int size;
	char *name;
} QCC_type_t;
int typecmp(QCC_type_t *a, QCC_type_t *b);

typedef struct temp_s {
	gofs_t ofs;
	struct QCC_def_s	*scope;
#ifdef WRITEASM
	struct QCC_def_s	*lastfunc;
#endif
	struct temp_s	*next;
	pbool used;
	unsigned int size;
} temp_t;

//not written
typedef struct QCC_def_s
{
	QCC_type_t		*type;
	char		*name;
	struct QCC_def_s	*next;
	struct QCC_def_s	*nextlocal;	//provides a chain of local variables for the opt_locals_marshalling optimisation.
	gofs_t		ofs;
	struct QCC_def_s	*scope;		// function the var was defined in, or NULL
	int			initialized;	// 1 when a declaration included "= immediate"
	int			constant;		// 1 says we can use the value over and over again

	int references;
	int timescalled;	//part of the opt_stripfunctions optimisation.

	int s_file;
	int s_line;

	int arraysize;
	pbool shared;
	pbool saved;
	pbool isstatic;

	temp_t *temp;
} QCC_def_t;

//============================================================================

// pr_loc.h -- program local defs


//=============================================================================
extern char QCC_copyright[1024];
extern char QCC_Packname[5][128];
extern int QCC_packid;

typedef union QCC_eval_s
{
	QCC_string_t			string;
	float				_float;
	float				vector[3];
	func_t				function;
	int					_int;
	union QCC_eval_s		*ptr;
} QCC_eval_t;

const extern	unsigned int		type_size[];
//extern	QCC_def_t	*def_for_type[9];

extern	QCC_type_t	*type_void, *type_string, *type_float, *type_vector, *type_entity, *type_field, *type_function, *type_pointer, *type_integer, *type_variant, *type_floatfield;

struct QCC_function_s
{
	int					builtin;	// if non 0, call an internal function
	int					code;		// first statement
	char				*file;		// source file with definition
	int					file_line;
	struct QCC_def_s		*def;
	unsigned int		parm_ofs[MAX_PARMS];	// always contiguous, right?
};


//
// output generated by prog parsing
//
typedef struct
{
	char		*memory;
	int			max_memory;
	int			current_memory;
	QCC_type_t		*types;
	
	QCC_def_t		def_head;		// unused head of linked list
	QCC_def_t		*def_tail;		// add new defs after this and move it
	QCC_def_t		*localvars;		// chain of variables which need to be pushed and stuff.
	
	int			size_fields;
} QCC_pr_info_t;

extern	QCC_pr_info_t	pr;


typedef struct
{
	char name[MAXCONSTANTLENGTH];
	char value[MAXCONSTANTVALUELENGTH];
	char params[MAXCONSTANTPARAMS][MAXCONSTANTPARAMLENGTH];
	int numparams;
	pbool used;
	pbool inside;

	int namelen;
} CompilerConstant_t;
extern CompilerConstant_t *CompilerConstant;

//============================================================================

extern	pbool	pr_dumpasm;

//extern	QCC_def_t		**pr_global_defs;	// to find def for a global variable

typedef enum {
tt_eof,			// end of file reached
tt_name, 		// an alphanumeric name token
tt_punct, 		// code punctuation
tt_immediate,	// string, float, vector
} token_type_t;

extern	char		pr_token[8192];
extern	token_type_t	pr_token_type;
extern	QCC_type_t		*pr_immediate_type;
extern	QCC_eval_t		pr_immediate;

extern pbool keyword_asm;
extern pbool keyword_break;
extern pbool keyword_case;
extern pbool keyword_class;
extern pbool keyword_const;
extern pbool keyword_continue;
extern pbool keyword_default;
extern pbool keyword_do;
extern pbool keyword_entity;
extern pbool keyword_float;
extern pbool keyword_for;
extern pbool keyword_goto;
extern pbool keyword_int;
extern pbool keyword_integer;
extern pbool keyword_state;
extern pbool keyword_string;
extern pbool keyword_struct;
extern pbool keyword_switch;
extern pbool keyword_thinktime;
extern pbool keyword_var;
extern pbool keyword_vector;
extern pbool keyword_union;
extern pbool keyword_enum;	//kinda like in c, but typedef not supported.
extern pbool keyword_enumflags;	//like enum, but doubles instead of adds 1.
extern pbool keyword_typedef;	//fixme
extern pbool keyword_extern;	//function is external, don't error or warn if the body was not found
extern pbool keyword_shared;	//mark global to be copied over when progs changes (part of FTE_MULTIPROGS)
extern pbool keyword_noref;	//nowhere else references this, don't strip it.
extern pbool keyword_nosave;	//don't write the def to the output.
extern pbool keyword_union;	//you surly know what a union is!


extern pbool keywords_coexist;
extern pbool output_parms;
extern pbool autoprototype;
extern pbool flag_ifstring;
extern pbool flag_acc;
extern pbool flag_caseinsensative;
extern pbool flag_laxcasts;
extern pbool flag_hashonly;
extern pbool flag_fasttrackarrays;

extern pbool opt_overlaptemps;
extern pbool opt_shortenifnots;
extern pbool opt_noduplicatestrings;
extern pbool opt_constantarithmatic;
extern pbool opt_nonvec_parms;
extern pbool opt_constant_names;
extern pbool opt_precache_file;
extern pbool opt_filenames;
extern pbool opt_assignments;
extern pbool opt_unreferenced;
extern pbool opt_function_names;
extern pbool opt_locals;
extern pbool opt_dupconstdefs;
extern pbool opt_constant_names_strings;
extern pbool opt_return_only;
extern pbool opt_compound_jumps;
//extern pbool opt_comexprremoval;
extern pbool opt_stripfunctions;
extern pbool opt_locals_marshalling;
extern pbool opt_logicops;
extern pbool opt_vectorcalls;

extern int optres_shortenifnots;
extern int optres_overlaptemps;
extern int optres_noduplicatestrings;
extern int optres_constantarithmatic;
extern int optres_nonvec_parms;
extern int optres_constant_names;
extern int optres_precache_file;
extern int optres_filenames;
extern int optres_assignments;
extern int optres_unreferenced;
extern int optres_function_names;
extern int optres_locals;
extern int optres_dupconstdefs;
extern int optres_constant_names_strings;
extern int optres_return_only;
extern int optres_compound_jumps;
//extern int optres_comexprremoval;
extern int optres_stripfunctions;
extern int optres_locals_marshalling;
extern int optres_logicops;

pbool CompileParams(progfuncs_t *progfuncs, int doall, int nump, char **parms);

void QCC_PR_PrintStatement (QCC_dstatement_t *s);

void QCC_PR_Lex (void);
// reads the next token into pr_token and classifies its type

QCC_type_t *QCC_PR_NewType (char *name, int basictype);
QCC_type_t *QCC_PR_ParseType (int newtype); extern pbool type_inlinefunction;
QCC_type_t *QCC_TypeForName(char *name);
QCC_type_t *QCC_PR_ParseFunctionType (int newtype, QCC_type_t *returntype);
QCC_type_t *QCC_PR_ParseFunctionTypeReacc (int newtype, QCC_type_t *returntype);
char *QCC_PR_ParseName (void);
CompilerConstant_t *QCC_PR_DefineName(char *name);

void QCC_RemapOffsets(unsigned int firststatement, unsigned int laststatement, unsigned int min, unsigned int max, unsigned int newmin);

#ifndef COMMONINLINES
pbool QCC_PR_CheckToken (char *string);
pbool QCC_PR_CheckName (char *string);
void QCC_PR_Expect (char *string);
pbool QCC_PR_CheckKeyword(int keywordenabled, char *string);
#endif
void VARGS QCC_PR_ParseError (int errortype, char *error, ...);
void VARGS QCC_PR_ParseWarning (int warningtype, char *error, ...);
void VARGS QCC_PR_Warning (int type, char *file, int line, char *error, ...);
void QCC_PR_ParsePrintDef (int warningtype, QCC_def_t *def);
void VARGS QCC_PR_ParseErrorPrintDef (int errortype, QCC_def_t *def, char *error, ...);

int QCC_WarningForName(char *name);

//QccMain.c must be changed if this is changed.
enum {
	WARN_DEBUGGING,
	WARN_ERROR,
	WARN_NOTREFERENCED,
	WARN_NOTREFERENCEDCONST,
	WARN_CONFLICTINGRETURNS,
	WARN_TOOFEWPARAMS,
	WARN_TOOMANYPARAMS,
	WARN_UNEXPECTEDPUNCT,
	WARN_ASSIGNMENTTOCONSTANT,
	WARN_ASSIGNMENTTOCONSTANTFUNC,
	WARN_MISSINGRETURNVALUE,
	WARN_WRONGRETURNTYPE,
	WARN_CORRECTEDRETURNTYPE,
	WARN_POINTLESSSTATEMENT,
	WARN_MISSINGRETURN,
	WARN_DUPLICATEDEFINITION,
	WARN_UNDEFNOTDEFINED,
	WARN_PRECOMPILERMESSAGE,
	WARN_TOOMANYPARAMETERSFORFUNC,
	WARN_STRINGTOOLONG,
	WARN_BADTARGET,
	WARN_BADPRAGMA,
	WARN_HANGINGSLASHR,
	WARN_NOTDEFINED,
	WARN_NOTCONSTANT,
	WARN_SWITCHTYPEMISMATCH,
	WARN_CONFLICTINGUNIONMEMBER,
	WARN_KEYWORDDISABLED,
	WARN_ENUMFLAGS_NOTINTEGER,
	WARN_ENUMFLAGS_NOTBINARY,
	WARN_CASEINSENSATIVEFRAMEMACRO,
	WARN_DUPLICATELABEL,
	WARN_DUPLICATEMACRO,
	WARN_ASSIGNMENTINCONDITIONAL,
	WARN_MACROINSTRING,
	WARN_BADPARAMS,
	WARN_IMPLICITCONVERSION,
	WARN_FIXEDRETURNVALUECONFLICT,
	WARN_EXTRAPRECACHE,
	WARN_NOTPRECACHED,
	WARN_DEADCODE,
	WARN_UNREACHABLECODE,
	WARN_NOTSTANDARDBEHAVIOUR,
	WARN_INEFFICIENTPLUSPLUS,
	WARN_DUPLICATEPRECOMPILER,
	WARN_IDENTICALPRECOMPILER,
	WARN_FTE_SPECIFIC,	//extension that only FTEQCC will have a clue about.
	WARN_EXTENSION_USED,	//extension that frikqcc also understands
	WARN_IFSTRING_USED,
	WARN_LAXCAST,	//some errors become this with a compiler flag
	WARN_UNDESIRABLECONVENTION,
	WARN_SAMENAMEASGLOBAL,
	WARN_CONSTANTCOMPARISON,
	WARN_UNSAFEFUNCTIONRETURNTYPE,

	ERR_PARSEERRORS,	//caused by qcc_pr_parseerror being called.

	//these are definatly my fault...
	ERR_INTERNAL,
	ERR_TOOCOMPLEX,
	ERR_BADOPCODE,
	ERR_TOOMANYSTATEMENTS,
	ERR_TOOMANYSTRINGS,
	ERR_BADTARGETSWITCH,
	ERR_TOOMANYTYPES,
	ERR_TOOMANYPAKFILES,
	ERR_PRECOMPILERCONSTANTTOOLONG,
	ERR_MACROTOOMANYPARMS,
	ERR_CONSTANTTOOLONG,
	ERR_TOOMANYFRAMEMACROS,

	//limitations, some are imposed by compiler, some arn't.
	ERR_TOOMANYGLOBALS,
	ERR_TOOMANYGOTOS,
	ERR_TOOMANYBREAKS,
	ERR_TOOMANYCONTINUES,
	ERR_TOOMANYCASES,
	ERR_TOOMANYLABELS,
	ERR_TOOMANYOPENFILES,
	ERR_TOOMANYPARAMETERSVARARGS,
	ERR_TOOMANYTOTALPARAMETERS,

	//these are probably yours, or qcc being fussy.
	ERR_BADEXTENSION,
	ERR_BADIMMEDIATETYPE,
	ERR_NOOUTPUT,
	ERR_NOTAFUNCTION,
	ERR_FUNCTIONWITHVARGS,
	ERR_BADHEX,
	ERR_UNKNOWNPUCTUATION,
	ERR_EXPECTED,
	ERR_NOTANAME,
	ERR_NAMETOOLONG,
	ERR_NOFUNC,
	ERR_COULDNTOPENFILE,
	ERR_NOTFUNCTIONTYPE,
	ERR_TOOFEWPARAMS,
	ERR_TOOMANYPARAMS,
	ERR_CONSTANTNOTDEFINED,
	ERR_BADFRAMEMACRO,
	ERR_TYPEMISMATCH,
	ERR_TYPEMISMATCHREDEC,
	ERR_TYPEMISMATCHPARM,
	ERR_TYPEMISMATCHARRAYSIZE,
	ERR_UNEXPECTEDPUNCTUATION,
	ERR_NOTACONSTANT,
	ERR_REDECLARATION,
	ERR_INITIALISEDLOCALFUNCTION,
	ERR_NOTDEFINED,
	ERR_ARRAYNEEDSSIZE,
	ERR_ARRAYNEEDSBRACES,
	ERR_TOOMANYINITIALISERS,
	ERR_TYPEINVALIDINSTRUCT,
	ERR_NOSHAREDLOCALS,
	ERR_TYPEWITHNONAME,
	ERR_BADARRAYSIZE,
	ERR_NONAME,
	ERR_SHAREDINITIALISED,
	ERR_UNKNOWNVALUE,
	ERR_BADARRAYINDEXTYPE,
	ERR_NOVALIDOPCODES,
	ERR_MEMBERNOTVALID,
	ERR_BADPLUSPLUSOPERATOR,
	ERR_BADNOTTYPE,
	ERR_BADTYPECAST,
	ERR_MULTIPLEDEFAULTS,
	ERR_CASENOTIMMEDIATE,
	ERR_BADSWITCHTYPE,
	ERR_BADLABELNAME,
	ERR_NOLABEL,
	ERR_THINKTIMETYPEMISMATCH,
	ERR_STATETYPEMISMATCH,
	ERR_BADBUILTINIMMEDIATE,
	ERR_PARAMWITHNONAME,
	ERR_BADPARAMORDER,
	ERR_ILLEGALCONTINUES,
	ERR_ILLEGALBREAKS,
	ERR_ILLEGALCASES,
	ERR_NOTANUMBER,
	ERR_WRONGSUBTYPE,
	ERR_EOF,
	ERR_NOPRECOMPILERIF,
	ERR_NOENDIF,
	ERR_HASHERROR,
	ERR_NOTATYPE,
	ERR_TOOMANYPACKFILES,
	ERR_INVALIDVECTORIMMEDIATE,
	ERR_INVALIDSTRINGIMMEDIATE,
	ERR_BADCHARACTERCODE,
	ERR_BADPARMS,

	WARN_MAX
};

#define FLAG_KILLSDEBUGGERS	1
#define FLAG_ASDEFAULT		2
#define FLAG_SETINGUI		4
#define FLAG_HIDDENINGUI	8
#define FLAG_MIDCOMPILE		16	//option can be changed mid-compile with the special pragma
typedef struct {
	pbool *enabled;
	char *abbrev;
	int optimisationlevel;
	int flags;	//1: kills debuggers. 2: applied as default.
	char *fullname;
	char *description;
	void *guiinfo;
} optimisations_t;
extern optimisations_t optimisations[];

typedef struct {
	pbool *enabled;
	int flags;	//2 applied as default
	char *abbrev;
	char *fullname;
	char *description;
	void *guiinfo;
} compiler_flag_t;
extern compiler_flag_t compiler_flag[];

extern pbool qccwarningdisabled[WARN_MAX];

extern	jmp_buf		pr_parse_abort;		// longjump with this on parse error
extern	int			pr_source_line;
extern	char		*pr_file_p;

void *QCC_PR_Malloc (int size);


#define	OFS_NULL		0
#define	OFS_RETURN		1
#define	OFS_PARM0		4		// leave 3 ofs for each parm to hold vectors
#define	OFS_PARM1		7
#define	OFS_PARM2		10
#define	OFS_PARM3		13
#define	OFS_PARM4		16
#define	RESERVED_OFS	28


extern	QCC_def_t	*pr_scope;
extern	int		pr_error_count, pr_warning_count;

void QCC_PR_NewLine (pbool incomment);
QCC_def_t *QCC_PR_GetDef (QCC_type_t *type, char *name, QCC_def_t *scope, pbool allocate, int arraysize, pbool saved);

void QCC_PR_PrintDefs (void);

void QCC_PR_SkipToSemicolon (void);

#define MAX_EXTRA_PARMS 128
#ifdef MAX_EXTRA_PARMS
extern	char		pr_parm_names[MAX_PARMS+MAX_EXTRA_PARMS][MAX_NAME];
extern QCC_def_t *extra_parms[MAX_EXTRA_PARMS];
#else
extern	char		pr_parm_names[MAX_PARMS][MAX_NAME];
#endif
extern	pbool	pr_trace;

#define	G_FLOAT(o) (qcc_pr_globals[o])
#define	G_INT(o) (*(int *)&qcc_pr_globals[o])
#define	G_VECTOR(o) (&qcc_pr_globals[o])
#define	G_STRING(o) (strings + *(QCC_string_t *)&qcc_pr_globals[o])
#define	G_FUNCTION(o) (*(func_t *)&qcc_pr_globals[o])

char *QCC_PR_ValueString (etype_t type, void *val);

void QCC_PR_ClearGrabMacros (void);

pbool	QCC_PR_CompileFile (char *string, char *filename);
void QCC_PR_ResetErrorScope(void);

extern	pbool	pr_dumpasm;

extern	QCC_string_t	s_file;			// filename for function definition

extern	QCC_def_t	def_ret, def_parms[MAX_PARMS];

void QCC_PR_EmitArrayGetFunction(QCC_def_t *scope, char *arrayname);
void QCC_PR_EmitArraySetFunction(QCC_def_t *scope, char *arrayname);
void QCC_PR_EmitClassFromFunction(QCC_def_t *scope, char *tname);

//=============================================================================

extern char	pr_immediate_string[8192];

extern float		*qcc_pr_globals;
extern unsigned int	numpr_globals;

extern char		*strings;
extern int			strofs;

extern QCC_dstatement_t	*statements;
extern int			numstatements;
extern int			*statement_linenums;

extern QCC_dfunction_t	*functions;
extern int			numfunctions;

extern QCC_ddef_t		*qcc_globals;
extern int			numglobaldefs;

extern QCC_def_t		*activetemps;

extern QCC_ddef_t		*fields;
extern int			numfielddefs;

extern QCC_type_t *qcc_typeinfo;
extern int numtypeinfos;
extern int maxtypeinfos;

extern int ForcedCRC;
extern pbool defaultstatic;

extern int *qcc_tempofs;
extern int max_temps;
//extern int qcc_functioncalled;	//unuse temps if this is true - don't want to reuse the same space.

extern int tempsstart;
extern int numtemps;

typedef char PATHSTRING[MAX_DATA_PATH];

PATHSTRING		*precache_sounds;
int			*precache_sounds_block;
int			*precache_sounds_used;
int			numsounds;

PATHSTRING		*precache_textures;
int			*precache_textures_block;
int			numtextures;

PATHSTRING		*precache_models;
int			*precache_models_block;
int			*precache_models_used;
int			nummodels;

PATHSTRING		*precache_files;
int			*precache_files_block;
int			numfiles;

int	QCC_CopyString (char *str);




typedef struct qcc_cachedsourcefile_s {
	char filename[128];
	int size;
	char *file;
	enum{FT_CODE, FT_DATA} type;	//quakec source file or not.
	struct qcc_cachedsourcefile_s *next;
} qcc_cachedsourcefile_t;
extern qcc_cachedsourcefile_t *qcc_sourcefile;





#ifdef COMMONINLINES
static bool inline QCC_PR_CheckToken (char *string)
{
	if (pr_token_type != tt_punct)
		return false;

	if (STRCMP (string, pr_token))
		return false;

	QCC_PR_Lex ();
	return true;
}

static void inline QCC_PR_Expect (char *string)
{
	if (strcmp (string, pr_token))
		QCC_PR_ParseError ("expected %s, found %s",string, pr_token);
	QCC_PR_Lex ();
}
#endif

void editbadfile(char *fname, int line);
char *TypeName(QCC_type_t *type);
void QCC_PR_IncludeChunk (char *data, pbool duplicate, char *filename);
void QCC_PR_IncludeChunkEx(char *data, pbool duplicate, char *filename, CompilerConstant_t *cnst);
pbool QCC_PR_UnInclude(void);
extern void *(*pHash_Get)(hashtable_t *table, char *name);
extern void *(*pHash_GetNext)(hashtable_t *table, char *name, void *old);
extern void *(*pHash_Add)(hashtable_t *table, char *name, void *data, bucket_t *);
