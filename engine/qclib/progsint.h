#ifdef _WIN32
	#ifndef _CRT_SECURE_NO_WARNINGS
		#define _CRT_SECURE_NO_WARNINGS
	#endif
	#define _CRT_NONSTDC_NO_WARNINGS
	#ifndef _CRT_SECURE_NO_DEPRECATE
		#define _CRT_SECURE_NO_DEPRECATE
	#endif
	#ifndef _CRT_NONSTDC_NO_DEPRECATE
		#define _CRT_NONSTDC_NO_DEPRECATE
	#endif
	#ifndef AVAIL_ZLIB
		#ifdef _MSC_VER
			//#define AVAIL_ZLIB
		#endif
	#endif

	#include <windows.h>
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
//#define _inline inline
#endif
typedef unsigned char qbyte;
#include <stdio.h>

#define DLL_PROG
#ifndef PROGSUSED
#define PROGSUSED
#endif

#define false 0
#define true 1

#include "progtype.h"
#include "progslib.h"

#include "pr_comp.h"

#ifdef _MSC_VER
#pragma warning(disable : 4244)
#pragma warning(disable : 4267)
#endif

//extern progfuncs_t *progfuncs;
typedef struct sharedvar_s
{
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

typedef struct prinst_s
 {
	char **tempstrings;
	int maxtempstrings;
	int numtempstrings;
	int numtempstringsstack;

	char **allocedstrings;
	int maxallocedstrings;
	int numallocedstrings;

	struct progstate_s * progstate;
#define pr_progstate prinst.progstate

	progsnum_t pr_typecurrent;
#define pr_typecurrent prinst.pr_typecurrent
	unsigned int maxprogs;
#define maxprogs prinst.maxprogs

	struct progstate_s *current_progstate;
#define current_progstate prinst.current_progstate

	char * watch_name;
	eval_t * watch_ptr;
	eval_t watch_old;
	etype_t watch_type;

	unsigned int numshares;
#define numshares prinst.numshares
	sharedvar_t *shares;	//shared globals, not including parms
#define shares prinst.shares
	unsigned int maxshares;
#define maxshares prinst.maxshares

	struct prmemb_s     *memblocks;
#define memb prinst.memblocks

	unsigned int maxfields;
	unsigned int numfields;
	fdef_t *field;	//biggest size

int reorganisefields;


//pr_exec.c
#define	MAX_STACK_DEPTH		64
	prstack_t pr_stack[MAX_STACK_DEPTH];
#define pr_stack prinst.pr_stack
	int pr_depth;
#define pr_depth prinst.pr_depth
	int spushed;
#define pr_spushed prinst.spushed

#define	LOCALSTACK_SIZE		4096
	int localstack[LOCALSTACK_SIZE];
#define localstack prinst.localstack
	int localstack_used;
#define localstack_used prinst.localstack_used

	int debugstatement;
	int continuestatement;
	int exitdepth;

	dfunction_t	*pr_xfunction;
#define pr_xfunction prinst.pr_xfunction
	int pr_xstatement;
#define pr_xstatement prinst.pr_xstatement

//pr_edict.c

	unsigned int maxedicts;
#define maxedicts prinst.maxedicts

	evalc_t spawnflagscache;
#define spawnflagscache prinst.spawnflagscache




	unsigned int fields_size;	// in bytes
#define fields_size prinst.fields_size
	unsigned int max_fields_size;
#define max_fields_size prinst.max_fields_size


//initlib.c
	int mfreelist;
	char * addressablehunk;
	size_t addressableused;
	size_t addressablesize;

	struct edict_s **edicttable;
} prinst_t;

typedef struct progfuncs_s
{
	struct pubprogfuncs_s funcs;
	struct prinst_s	inst;	//private fields. Leave alone.
} progfuncs_t;

#define prinst progfuncs->inst
#define externs progfuncs->funcs.parms

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

#define printf externs->Printf
#define Sys_Error externs->Sys_Error

int PRHunkMark(progfuncs_t *progfuncs);
void PRHunkFree(progfuncs_t *progfuncs, int mark);
void *PRHunkAlloc(progfuncs_t *progfuncs, int size, char *name);
void *PRAddressableExtend(progfuncs_t *progfuncs, int ammount);

#ifdef printf
#undef LIKEPRINTF
#define LIKEPRINTF(x)
#endif

//void *HunkAlloc (int size);
char *VARGS qcva (char *text, ...) LIKEPRINTF(1);
void QC_InitShares(progfuncs_t *progfuncs);
void QC_StartShares(progfuncs_t *progfuncs);
void PDECL QC_AddSharedVar(pubprogfuncs_t *progfuncs, int num, int type);
void PDECL QC_AddSharedFieldVar(pubprogfuncs_t *progfuncs, int num, char *stringtable);
int PDECL QC_RegisterFieldVar(pubprogfuncs_t *progfuncs, unsigned int type, char *name, signed long requestedpos, signed long originalofs);
pbool PDECL QC_Decompile(pubprogfuncs_t *progfuncs, char *fname);
int PDECL PR_ToggleBreakpoint(pubprogfuncs_t *progfuncs, char *filename, int linenum, int flag);
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

typedef struct edictrun_s
{
	pbool	isfree;

	float		freetime;			// realtime when the object was freed
	unsigned int entnum;
	pbool	readonly;	//causes error when QC tries writing to it. (quake's world entity)
	void	*fields;

// other fields from progs come immediately after
} edictrun_t;


int PDECL Comp_Begin(pubprogfuncs_t *progfuncs, int nump, char **parms);
int PDECL Comp_Continue(pubprogfuncs_t *progfuncs);

pbool PDECL PR_SetWatchPoint(pubprogfuncs_t *progfuncs, char *key);
char *PDECL PR_EvaluateDebugString(pubprogfuncs_t *progfuncs, char *key);
char *PDECL PR_SaveEnts(pubprogfuncs_t *progfuncs, char *mem, int *size, int mode);
int PDECL PR_LoadEnts(pubprogfuncs_t *progfuncs, char *file, float killonspawnflags);
char *PDECL PR_SaveEnt (pubprogfuncs_t *progfuncs, char *buf, int *size, struct edict_s *ed);
struct edict_s *PDECL PR_RestoreEnt (pubprogfuncs_t *progfuncs, char *buf, int *size, struct edict_s *ed);
void PDECL PR_StackTrace (pubprogfuncs_t *progfuncs);

extern int noextensions;

typedef enum
{
	PST_DEFAULT, //16
	PST_FTE32, //32
	PST_KKQWSV, //24
	PST_QTEST,
} progstructtype_t;

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
	int				globals_size;	// in bytes

	typeinfo_t	*types;

	int				edict_size;	// in bytes

	char			filename[128];

	builtin_t	*builtins;
	int		numbuiltins;

	int *linenums;	//debug versions only

	progstructtype_t structtype;

#ifdef QCJIT
	struct jitstate *jit;
#endif
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

void PDECL PR_ExecuteProgram (pubprogfuncs_t *progfuncs, func_t fnum);
int PDECL PR_LoadProgs(pubprogfuncs_t *progfncs, char *s, int headercrc, builtin_t *builtins, int numbuiltins);
int PR_ReallyLoadProgs (progfuncs_t *progfuncs, char *filename, int headercrc, progstate_t *progstate, pbool complain);

void *PRHunkAlloc(progfuncs_t *progfuncs, int ammount, char *name);

void PR_Profile_f (void);

struct edict_s *PDECL ED_Alloc (pubprogfuncs_t *progfuncs);
void PDECL ED_Free (pubprogfuncs_t *progfuncs, struct edict_s *ed);

char *PDECL ED_NewString (pubprogfuncs_t *progfuncs, char *string, int minlength);
// returns a copy of the string allocated from the server's string heap

void PDECL ED_Print (pubprogfuncs_t *progfuncs, struct edict_s *ed);
//void ED_Write (FILE *f, edictrun_t *ed);
char *ED_ParseEdict (progfuncs_t *progfuncs, char *data, edictrun_t *ent);

//void ED_WriteGlobals (FILE *f);
void ED_ParseGlobals (char *data);

//void ED_LoadFromFile (char *data);

//define EDICT_NUM(n) ((edict_t *)(sv.edicts+ (n)*pr_edict_size))
//define NUM_FOR_EDICT(e) (((byte *)(e) - sv.edicts)/pr_edict_size)

struct edict_s *PDECL QC_EDICT_NUM(pubprogfuncs_t *progfuncs, unsigned int n);
unsigned int PDECL QC_NUM_FOR_EDICT(pubprogfuncs_t *progfuncs, struct edict_s *e);

#define EDICT_NUM(pf, num)	QC_EDICT_NUM(&pf->funcs,num)
#define NUM_FOR_EDICT(pf, e) QC_NUM_FOR_EDICT(&pf->funcs,e)

//#define	NEXT_EDICT(e) ((edictrun_t *)( (byte *)e + pr_edict_size))

#define	EDICT_TO_PROG(pf, e) (((edictrun_t*)e)->entnum)
#define PROG_TO_EDICT(pf, e) ((struct edictrun_s *)prinst.edicttable[e])

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
#define G_PROG(o) G_FLOAT(o)	//simply so it's nice and easy to change...

#define	RETURN_EDICT(e) (((int *)pr_globals)[OFS_RETURN] = EDICT_TO_PROG(e))

#define	E_FLOAT(e,o) (((float*)&e->v)[o])
#define	E_INT(e,o) (*(int *)&((float*)&e->v)[o])
#define	E_VECTOR(e,o) (&((float*)&e->v)[o])
#define	E_STRING(e,o) (*(string_t *)&((float*)(e+1))[o])

const extern	unsigned int		type_size[];


extern	unsigned short		pr_crc;

void VARGS PR_RunError (pubprogfuncs_t *progfuncs, char *error, ...) LIKEPRINTF(2);

void ED_PrintEdicts (progfuncs_t *progfuncs);
void ED_PrintNum (progfuncs_t *progfuncs, int ent);


pbool PR_SwitchProgs(progfuncs_t *progfuncs, progsnum_t type);
pbool PR_SwitchProgsParms(progfuncs_t *progfuncs, progsnum_t newprogs);




eval_t *PDECL QC_GetEdictFieldValue(pubprogfuncs_t *progfuncs, struct edict_s *ed, char *name, evalc_t *cache);
void PDECL PR_GenerateStatementString (pubprogfuncs_t *progfuncs, int statementnum, char *out, int outlen);
fdef_t *PDECL ED_FieldInfo (pubprogfuncs_t *progfuncs, unsigned int *count);
char *PDECL PR_UglyValueString (pubprogfuncs_t *progfuncs, etype_t type, eval_t *val);
pbool	PDECL ED_ParseEval (pubprogfuncs_t *progfuncs, eval_t *eval, int type, char *s);

#endif




#ifndef COMPILER

//this is windows - all files are written with this endian standard
//optimisation
//leave undefined if in doubt over os.
#ifdef _WIN32
#define NOENDIAN
#endif




//pr_multi.c
void PR_SetBuiltins(int type);

extern vec3_t vec3_origin;

struct qcthread_s *PDECL PR_ForkStack	(pubprogfuncs_t *progfuncs);
void PDECL PR_ResumeThread			(pubprogfuncs_t *progfuncs, struct qcthread_s *thread);
void	PDECL PR_AbortStack			(pubprogfuncs_t *progfuncs);

eval_t *PDECL PR_FindGlobal(pubprogfuncs_t *prfuncs, char *globname, progsnum_t pnum, etype_t *type);
ddef16_t *ED_FindTypeGlobalFromProgs16 (progfuncs_t *progfuncs, char *name, progsnum_t prnum, int type);
ddef32_t *ED_FindTypeGlobalFromProgs32 (progfuncs_t *progfuncs, char *name, progsnum_t prnum, int type);
ddef16_t *ED_FindGlobalFromProgs16 (progfuncs_t *progfuncs, char *name, progsnum_t prnum);
ddef32_t *ED_FindGlobalFromProgs32 (progfuncs_t *progfuncs, char *name, progsnum_t prnum);
fdef_t *ED_FindField (progfuncs_t *progfuncs, char *name);
fdef_t *ED_FieldAtOfs (progfuncs_t *progfuncs, unsigned int ofs);
dfunction_t *ED_FindFunction (progfuncs_t *progfuncs, char *name, progsnum_t *pnum, progsnum_t fromprogs);
func_t PDECL PR_FindFunc(pubprogfuncs_t *progfncs, char *funcname, progsnum_t pnum);
void PDECL PR_Configure (pubprogfuncs_t *progfncs, size_t addressable_size, int max_progs);
int PDECL PR_InitEnts(pubprogfuncs_t *progfncs, int maxents);
char *PR_ValueString (progfuncs_t *progfuncs, etype_t type, eval_t *val);
void PDECL QC_ClearEdict (pubprogfuncs_t *progfuncs, struct edict_s *ed);
void PRAddressableFlush(progfuncs_t *progfuncs, size_t totalammount);
void QC_FlushProgsOffsets(progfuncs_t *progfuncs);

ddef16_t *ED_GlobalAtOfs16 (progfuncs_t *progfuncs, int ofs);
ddef16_t *ED_FindGlobal16 (progfuncs_t *progfuncs, char *name);
ddef32_t *ED_FindGlobal32 (progfuncs_t *progfuncs, char *name);
ddef32_t *ED_GlobalAtOfs32 (progfuncs_t *progfuncs, unsigned int ofs);

string_t PDECL PR_StringToProgs			(pubprogfuncs_t *inst, char *str);
char *ASMCALL PR_StringToNative				(pubprogfuncs_t *inst, string_t str);

void PR_FreeTemps			(progfuncs_t *progfuncs, int depth);

char *PR_GlobalString (progfuncs_t *progfuncs, int ofs);
char *PR_GlobalStringNoContents (progfuncs_t *progfuncs, int ofs);

pbool CompileFile(progfuncs_t *progfuncs, char *filename);

struct jitstate;
struct jitstate *PR_GenerateJit(progfuncs_t *progfuncs);
void PR_EnterJIT(progfuncs_t *progfuncs, struct jitstate *jitstate, int statement);
void PR_CloseJit(struct jitstate *jit);

char *QCC_COM_Parse (char *data);
extern char	qcc_token[1024];
#endif
