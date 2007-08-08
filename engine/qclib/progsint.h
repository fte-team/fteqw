#ifdef WIN32

	#ifndef AVAIL_ZLIB
		#ifdef _MSC_VER
			//#define AVAIL_ZLIB
		#endif
	#endif

	#include <windows.h>

	enum{false, true};
#else
	#include <stdarg.h>
	#include <math.h>

	#include <stdlib.h>
	#include <setjmp.h>
	#include <string.h>
	#include <ctype.h>

	#ifndef __declspec
		#define __declspec(mode)
	#endif

	typedef enum{false, true} boolean;
//#define _inline inline
#endif
typedef unsigned char qbyte;
#include <stdio.h>

#define DLL_PROG
#ifndef PROGSUSED
#define PROGSUSED
#endif

extern int maxedicts;
extern int maxprogs;
extern int hunksize;

#include "progtype.h"
#include "progslib.h"

//extern progfuncs_t *progfuncs;

#define prinst progfuncs->prinst
#define externs progfuncs->parms

#include "pr_comp.h"

#include "qcd.h"

typedef struct
{
	int			targetflags;	//weather we need to mark the progs as a newer version
	char		*name;
	char		*opname;
	int		priority;
	enum {ASSOC_LEFT, ASSOC_RIGHT, ASSOC_RIGHT_RESULT}			associative;
	struct QCC_type_s		**type_a, **type_b, **type_c;
} QCC_opcode_t;
extern	QCC_opcode_t	pr_opcodes[];		// sized by initialization




#ifdef _MSC_VER
#define Q_vsnprintf _vsnprintf
#else
#define Q_vsnprintf vsnprintf
#endif


#define sv_num_edicts (*externs->sv_num_edicts)
#define sv_edicts (*externs->sv_edicts)

#define printf externs->printf
#define Sys_Error externs->Sys_Error
#define Abort externs->Abort

#define memalloc externs->memalloc
#define memfree externs->memfree

int PRHunkMark(progfuncs_t *progfuncs);
void PRHunkFree(progfuncs_t *progfuncs, int mark);
void *PRHunkAlloc(progfuncs_t *progfuncs, int size);
void *PRAddressableAlloc(progfuncs_t *progfuncs, int ammount);

//void *HunkAlloc (int size);
char *VARGS qcva (char *text, ...);
void QC_InitShares(progfuncs_t *progfuncs);
void QC_StartShares(progfuncs_t *progfuncs);
void QC_AddSharedVar(progfuncs_t *progfuncs, int num, int type);
void QC_AddSharedFieldVar(progfuncs_t *progfuncs, int num, char *stringtable);
int QC_RegisterFieldVar(progfuncs_t *progfuncs, unsigned int type, char *name, int requestedpos, int originalofs);
pbool Decompile(progfuncs_t *progfuncs, char *fname);
int PR_ToggleBreakpoint(progfuncs_t *progfuncs, char *filename, int linenum, int flag);
void    StripExtension (char *path);


#define edvars(ed) (((edictrun_t*)ed)->fields)	//pointer to the field vars, given an edict


void SetEndian(void);
extern short   (*PRBigShort) (short l);
extern short   (*PRLittleShort) (short l);
extern int     (*PRBigLong) (int l);
extern int     (*PRLittleLong) (int l);
extern float   (*PRBigFloat) (float l);
extern float   (*PRLittleFloat) (float l);



/*
#ifndef COMPILER
typedef union eval_s
{
	string_t		string;
	float			_float;
	float			vector[3];
	func_t			function;
	int				_int;
	int				edict;
	progsnum_t		prog;	//so it can easily be changed
} eval_t;
#endif
*/


#define	MAX_ENT_LEAFS	16
typedef struct edictrun_s
{
	pbool	isfree;

	float		freetime;			// realtime when the object was freed
	unsigned int entnum;
	pbool	readonly;	//causes error when QC tries writing to it. (quake's world entity)
	void	*fields;

// other fields from progs come immediately after
} edictrun_t;
#define	EDICT_FROM_AREA(l) STRUCT_FROM_LINK(l,edictrun_t,area)


int Comp_Begin(progfuncs_t *progfuncs, int nump, char **parms);
int Comp_Continue(progfuncs_t *progfuncs);

char *EvaluateDebugString(progfuncs_t *progfuncs, char *key);
char *SaveEnts(progfuncs_t *progfuncs, char *mem, int *size, int mode);
int LoadEnts(progfuncs_t *progfuncs, char *file, float killonspawnflags);
char *SaveEnt (progfuncs_t *progfuncs, char *buf, int *size, struct edict_s *ed);
struct edict_s *RestoreEnt (progfuncs_t *progfuncs, char *buf, int *size, struct edict_s *ed);
char *PF_VarString (int	first);
void PR_StackTrace (progfuncs_t *progfuncs);

extern int noextensions;

#ifndef COMPILER
typedef struct progstate_s
{
	dprograms_t		*progs;
	dfunction_t		*functions;
	char			*strings;
	union {
		ddefXX_t		*globaldefs;
		ddef16_t		*globaldefs16;
		ddef32_t		*globaldefs32;
	};
	union {
		ddefXX_t		*fielddefs;
		ddef16_t		*fielddefs16;
		ddef32_t		*fielddefs32;
	};
	void	*statements;
//	void			*global_struct;
	float			*globals;			// same as pr_global_struct

	typeinfo_t	*types;

	int				edict_size;	// in bytes

	char			filename[128];

	builtin_t	*builtins;
	int		numbuiltins;

	int *linenums;	//debug versions only

	int intsize;	//16 for standard (more limiting) versions
} progstate_t;

typedef struct extensionbuiltin_s {
	char *name;
	builtin_t func;
	struct extensionbuiltin_s *prev;
} extensionbuiltin_t;

//============================================================================


#define pr_progs			current_progstate->progs
#define	pr_functions		current_progstate->functions
#define	pr_strings			current_progstate->strings
#define	pr_globaldefs16		((ddef16_t*)current_progstate->globaldefs)
#define	pr_globaldefs32		((ddef32_t*)current_progstate->globaldefs)
#define	pr_fielddefs16		((ddef16_t*)current_progstate->fielddefs)
#define	pr_fielddefs32		((ddef32_t*)current_progstate->fielddefs)
#define	pr_statements16		((dstatement16_t*)current_progstate->statements)
#define	pr_statements32		((dstatement32_t*)current_progstate->statements)
//#define	pr_global_struct	current_progstate->global_struct
#define pr_globals			current_progstate->globals
#define pr_linenums			current_progstate->linenums
#define pr_types			current_progstate->types



//============================================================================

void PR_Init (void);

void PR_ExecuteProgram (progfuncs_t *progfuncs, func_t fnum);
int PR_LoadProgs(progfuncs_t *progfncs, char *s, int headercrc, builtin_t *builtins, int numbuiltins);
int PR_ReallyLoadProgs (progfuncs_t *progfuncs, char *filename, int headercrc, progstate_t *progstate, pbool complain);

void *PRHunkAlloc(progfuncs_t *progfuncs, int ammount);

void PR_Profile_f (void);

struct edict_s *ED_Alloc (progfuncs_t *progfuncs);
void ED_Free (progfuncs_t *progfuncs, struct edict_s *ed);

char *ED_NewString (progfuncs_t *progfuncs, char *string, int minlength);
// returns a copy of the string allocated from the server's string heap

void ED_Print (progfuncs_t *progfuncs, struct edict_s *ed);
//void ED_Write (FILE *f, edictrun_t *ed);
char *ED_ParseEdict (progfuncs_t *progfuncs, char *data, edictrun_t *ent);

//void ED_WriteGlobals (FILE *f);
void ED_ParseGlobals (char *data);

//void ED_LoadFromFile (char *data);

//define EDICT_NUM(n) ((edict_t *)(sv.edicts+ (n)*pr_edict_size))
//define NUM_FOR_EDICT(e) (((byte *)(e) - sv.edicts)/pr_edict_size)

struct edict_s *EDICT_NUM(progfuncs_t *progfuncs, unsigned int n);
unsigned int NUM_FOR_EDICT(progfuncs_t *progfuncs, struct edict_s *e);

//#define	NEXT_EDICT(e) ((edictrun_t *)( (byte *)e + pr_edict_size))

#define	EDICT_TO_PROG(pf, e) (((edictrun_t*)e)->entnum)
#define PROG_TO_EDICT(pf, e) ((struct edictrun_s *)prinst->edicttable[e])

//============================================================================

#define	G_FLOAT(o) (pr_globals[o])
#define	G_FLOAT2(o) (pr_globals[OFS_PARM0 + o*3])
#define	G_INT(o) (*(int *)&pr_globals[o])
#define	G_EDICT(o) ((edict_t *)((qbyte *)sv_edicts+ *(int *)&pr_globals[o]))
#define G_EDICTNUM(o) NUM_FOR_EDICT(G_EDICT(o))
#define	G_VECTOR(o) (&pr_globals[o])
#define	G_STRING(o) (*(string_t *)&pr_globals[o])
#define G_STRING2(o) ((char*)*(string_t *)&pr_globals[o])
#define	GQ_STRING(o) (*(QCC_string_t *)&pr_globals[o])
#define GQ_STRING2(o) ((char*)*(QCC_string_t *)&pr_globals[o])
#define	G_FUNCTION(o) (*(func_t *)&pr_globals[o])
#define G_PROG(o) (*(progsnum_t *)&pr_globals[o])	//simply so it's nice and easy to change...

#define	RETURN_EDICT(e) (((int *)pr_globals)[OFS_RETURN] = EDICT_TO_PROG(e))

#define	E_FLOAT(e,o) (((float*)&e->v)[o])
#define	E_INT(e,o) (*(int *)&((float*)&e->v)[o])
#define	E_VECTOR(e,o) (&((float*)&e->v)[o])
#define	E_STRING(e,o) (*(string_t *)&((float*)(e+1))[o])

const extern	unsigned int		type_size[];


extern	unsigned short		pr_crc;

void VARGS PR_RunError (progfuncs_t *progfuncs, char *error, ...);

void ED_PrintEdicts (progfuncs_t *progfuncs);
void ED_PrintNum (progfuncs_t *progfuncs, int ent);


pbool PR_SwitchProgs(progfuncs_t *progfuncs, progsnum_t type);
void PR_MoveParms(progfuncs_t *progfuncs, progsnum_t progs1, progsnum_t progs2);




eval_t *GetEdictFieldValue(progfuncs_t *progfuncs, struct edict_s *ed, char *name, evalc_t *cache);

#endif




#ifndef COMPILER

//this is windows - all files are written with this endian standard
//optimisation
//leave undefined if in doubt over os.
#ifdef _WIN32
#define NOENDIAN
#endif


typedef struct {
	int varofs;
	int size;
} sharedvar_t;
typedef struct
{
	int				s;
	dfunction_t		*f;
	int				progsnum;
	int pushed;
} prstack_t;



//pr_multi.c
void PR_SetBuiltins(int type);

#define var(type, name) type name
#define vars(type, name, size) type name[size]

typedef struct prinst_s {

	char **tempstrings;
	int maxtempstrings;
	int numtempstrings;
	int numtempstringsstack;

	char **allocedstrings;
	int maxallocedstrings;
	int numallocedstrings;

var(progstate_t *, pr_progstate);
#define pr_progstate prinst->pr_progstate

var(progsnum_t, pr_typecurrent);
#define pr_typecurrent prinst->pr_typecurrent
var(unsigned int, maxprogs);
#define maxprogs prinst->maxprogs

var(progstate_t *,current_progstate);
#define current_progstate prinst->current_progstate

var(unsigned int, numshares);
#define numshares prinst->numshares
var(sharedvar_t *,shares);	//shared globals, not including parms
#define shares prinst->shares
var(unsigned int, maxshares);
#define maxshares prinst->maxshares

var(struct prmemb_s     *, memblocks);
#define memb prinst->memblocks

var(unsigned int, maxfields);
#define maxfields prinst->maxfields
var(unsigned int, numfields);
#define numfields prinst->numfields
var(fdef_t*, field);	//biggest size
#define field prinst->field

int reorganisefields;


//pr_exec.c
#define	MAX_STACK_DEPTH		64
vars(prstack_t, pr_stack, MAX_STACK_DEPTH);
#define pr_stack prinst->pr_stack
var(int, pr_depth);
#define pr_depth prinst->pr_depth
var(int, spushed);
#define pr_spushed prinst->spushed

#define	LOCALSTACK_SIZE		4096
vars(int, localstack, LOCALSTACK_SIZE);
#define localstack prinst->localstack
var(int, localstack_used);
#define localstack_used prinst->localstack_used

var(int, continuestatement);
var(int, exitdepth);

var(int, pr_trace);
#define pr_trace prinst->pr_trace
var(dfunction_t	*, pr_xfunction);
#define pr_xfunction prinst->pr_xfunction
var(int, pr_xstatement);
#define pr_xstatement prinst->pr_xstatement

var(int, pr_argc);
#define pr_argc prinst->pr_argc

//pr_edict.c

var(unsigned int, maxedicts);
#define maxedicts prinst->maxedicts

var(evalc_t, spawnflagscache);
#define spawnflagscache prinst->spawnflagscache




var(unsigned int, fields_size);	// in bytes
#define fields_size prinst->fields_size
var(unsigned int, max_fields_size);
#define max_fields_size prinst->max_fields_size


//initlib.c
var(char *, addressablehunk);
#define addressablehunk prinst->addressablehunk
var(int, addressableused);
#define addressableused prinst->addressableused
var(int, addressablesize);
#define addressablesize prinst->addressablesize


//var(extensionbuiltin_t *, extensionbuiltin);
//#define extensionbuiltin prinst->extensionbuiltin

	struct edict_s **edicttable;
} prinst_t;
extern vec3_t vec3_origin;

eval_t *PR_FindGlobal(progfuncs_t *prfuncs, char *globname, progsnum_t pnum);
ddef16_t *ED_FindTypeGlobalFromProgs16 (progfuncs_t *progfuncs, char *name, progsnum_t prnum, int type);
ddef32_t *ED_FindTypeGlobalFromProgs32 (progfuncs_t *progfuncs, char *name, progsnum_t prnum, int type);
ddef16_t *ED_FindGlobalFromProgs16 (progfuncs_t *progfuncs, char *name, progsnum_t prnum);
ddef32_t *ED_FindGlobalFromProgs32 (progfuncs_t *progfuncs, char *name, progsnum_t prnum);
fdef_t *ED_FindField (progfuncs_t *progfuncs, char *name);
dfunction_t *ED_FindFunction (progfuncs_t *progfuncs, char *name, progsnum_t *pnum, progsnum_t fromprogs);
func_t PR_FindFunc(progfuncs_t *progfncs, char *funcname, progsnum_t pnum);
void PR_Configure (progfuncs_t *progfncs, int addressable_size, int max_progs);
int PR_InitEnts(progfuncs_t *progfncs, int maxents);
char *PR_ValueString (progfuncs_t *progfuncs, etype_t type, eval_t *val);
void ED_ClearEdict (progfuncs_t *progfuncs, edictrun_t *e);
void PRAddressableFlush(progfuncs_t *progfuncs, int totalammount);
void QC_FlushProgsOffsets(progfuncs_t *progfuncs);

ddef16_t *ED_GlobalAtOfs16 (progfuncs_t *progfuncs, int ofs);
ddef16_t *ED_FindGlobal16 (progfuncs_t *progfuncs, char *name);
ddef32_t *ED_FindGlobal32 (progfuncs_t *progfuncs, char *name);
ddef32_t *ED_GlobalAtOfs32 (progfuncs_t *progfuncs, unsigned int ofs);

string_t PR_StringToProgs			(progfuncs_t *inst, char *str);
char *PR_StringToNative				(progfuncs_t *inst, string_t str);

void PR_FreeTemps			(progfuncs_t *progfuncs, int depth);

char *PR_GlobalString (progfuncs_t *progfuncs, int ofs);
char *PR_GlobalStringNoContents (progfuncs_t *progfuncs, int ofs);

pbool CompileFile(progfuncs_t *progfuncs, char *filename);

char *QCC_COM_Parse (char *data);
extern char	qcc_token[1024];
#endif
