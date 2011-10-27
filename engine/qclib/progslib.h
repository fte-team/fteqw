
#ifndef PROGSLIB_H
#define PROGSLIB_H
#ifdef _MSC_VER
	#define VARGS __cdecl
#endif
#if (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1))
	#define LIKEPRINTF(x) __attribute__((format(printf,x,x+1)))
#endif
#ifndef LIKEPRINTF
	#define LIKEPRINTF(x)
#endif
#ifndef VARGS
	#define VARGS
#endif

#if defined(_M_IX86) || defined(__i386__)
//#define QCJIT
#endif

#ifdef QCJIT
#define ASMCALL VARGS
#else
#define ASMCALL
#endif
#define QCBUILTIN ASMCALL


struct edict_s;
struct entvars_s;
struct globalvars_s;
struct qcthread_s;
typedef struct progfuncs_s progfuncs_t;
typedef void (ASMCALL *builtin_t) (progfuncs_t *prinst, struct globalvars_s *gvars);

//used by progs engine. All nulls is reset.
typedef struct {
	char *varname;
	struct fdef_s *ofs32;

	int spare[2];
} evalc_t;
#define sizeofevalc sizeof(evalc_t)
typedef enum {ev_void, ev_string, ev_float, ev_vector, ev_entity, ev_field, ev_function, ev_pointer, ev_integer, ev_variant, ev_struct, ev_union} etype_t;

//the number of pointers to variables (as opposed to functions - those are fine) in these structures is excessive.
//Many of the functions are also obsolete.
struct progfuncs_s {
	int progsversion;	//PROGSTRUCT_VERSION


	void	(*Configure)				(progfuncs_t *prinst, int addressablesize, int max_progs);		//configure buffers and memory. Used to reset and must be called first. Flushes a running VM.
	progsnum_t	(*LoadProgs)			(progfuncs_t *prinst, char *s, int headercrc, builtin_t *builtins, int numbuiltins);	//load a progs
	int		(*InitEnts)					(progfuncs_t *prinst, int max_ents);	//returns size of edicts for use with nextedict macro
	void	(*ExecuteProgram)			(progfuncs_t *prinst, func_t fnum);	//start execution
	pbool	(*SwitchProgs)				(progfuncs_t *prinst, progsnum_t num);	//switch to a different progs - this should be obsolete.
	struct globalvars_s	*(*globals)		(progfuncs_t *prinst, progsnum_t num);	//get the globals of a progs
	struct entvars_s	*(*entvars)		(progfuncs_t *prinst, struct edict_s *ent);	//return a pointer to the entvars of an ent. can be achieved via the edict_t structure instead, so obsolete.

	void	(VARGS *RunError)			(progfuncs_t *prinst, char *msg, ...) LIKEPRINTF(2);		//builtins call this to say there was a problem
	void	(*PrintEdict)				(progfuncs_t *prinst, struct edict_s *ed);	//get a listing of all vars on an edict (sent back via 'print')

	struct edict_s	*(*EntAlloc)		(progfuncs_t *prinst);
	void	(*EntFree)					(progfuncs_t *prinst, struct edict_s *ed);

	struct edict_s	*(*EDICT_NUM)		(progfuncs_t *prinst, unsigned int n);		//get the nth edict
	unsigned int		(*NUM_FOR_EDICT)			(progfuncs_t *prinst, struct edict_s *e);	//so you can find out what that 'n' will be

	void	(*SetGlobalEdict)			(progfuncs_t *prinst, struct edict_s *ed, int ofs);	//set a global to an edict (partially obsolete)

	char	*(*VarString)				(progfuncs_t *prinst, int	first);	//returns a string made up of multiple arguments

	struct progstate_s **progstate;	//internal to the library.

	func_t	(*FindFunction)				(progfuncs_t *prinst, char *funcname, progsnum_t num);

	int		(*StartCompile)				(progfuncs_t *prinst, int argv, char **argc);	//1 if can compile, 0 if failed to compile
	int		(*ContinueCompile)			(progfuncs_t *prinst);	//2 if finished, 1 if more to go, 0 if failed

	char	*(*filefromprogs)			(progfuncs_t *prinst, progsnum_t prnum, char *fname, int *size, char *buffer);	//reveals encoded/added files from already loaded progs
	char	*(*filefromnewprogs)		(progfuncs_t *prinst, char *prname, char *fname, int *size, char *buffer);	//reveals encoded/added files from a progs on the disk somewhere

	char	*(*save_ents)				(progfuncs_t *prinst, char *buf, int *size, int mode);	//dump the entire progs info into one big self allocated string
	int		(*load_ents)				(progfuncs_t *prinst, char *s, float killonspawnflags);	//restore the entire progs state (or just add some more ents) (returns edicts ize)

	char	*(*saveent)					(progfuncs_t *prinst, char *buf, int *size, struct edict_s *ed);	//will save just one entities vars
	struct edict_s	*(*restoreent)		(progfuncs_t *prinst, char *buf, int *size, struct edict_s *ed);	//will restore the entity that had it's values saved (can use NULL for ed)

	union eval_s	*(*FindGlobal)		(progfuncs_t *prinst, char *name, progsnum_t num, etype_t *type);	//find a pointer to the globals value
	char	*(*AddString)				(progfuncs_t *prinst, char *val, int minlength);	//dump a string into the progs memory (for setting globals and whatnot)
	void	*(*Tempmem)					(progfuncs_t *prinst, int ammount, char *whatfor);	//grab some mem for as long as the progs stays loaded

	union eval_s	*(*GetEdictFieldValue)	(progfuncs_t *prinst, struct edict_s *ent, char *name, evalc_t *s); //get an entityvar (cache it) and return the possible values
	struct edict_s	*(*ProgsToEdict)	(progfuncs_t *prinst, int progs);	//edicts are stored as ints and need to be adjusted
	int		(*EdictToProgs)				(progfuncs_t *prinst, struct edict_s *ed);		//edicts are stored as ints and need to be adjusted

	char	*(*EvaluateDebugString)		(progfuncs_t *prinst, char *key);	//evaluate a string and return it's value (according to current progs) (expands edict vars)

	int		*pr_trace;	//start calling the editor for each line executed	

	void	(*StackTrace)				(progfuncs_t *prinst);
	
	int		(*ToggleBreak)				(progfuncs_t *prinst, char *filename, int linenum, int mode);

	int		numprogs;

	struct	progexterns_s *parms;	//these are the initial parms, they may be changed

	pbool	(*Decompile)				(progfuncs_t *prinst, char *fname);


	struct prinst_s	*prinst;	//internal variables. Leave alone.

	int		*callargc;	//number of args of built-in call
	void	(*RegisterBuiltin)			(progfuncs_t *prinst, char *, builtin_t);

	char *stringtable;	//qc strings are all relative. add to a qc string. this is required for support of frikqcc progs that strip string immediates.
	int fieldadjust;	//FrikQCC style arrays can cause problems due to field remapping. This causes us to leave gaps but offsets identical.

	struct qcthread_s *(*Fork)			(progfuncs_t *prinst);	//returns a pointer to a thread which can be resumed via RunThread.
	void	(*RunThread)				(progfuncs_t *prinst, struct qcthread_s *thread);
	void	(*AbortStack)				(progfuncs_t *prinst);	//annigilates the current stack, positioning on a return statement. It is expected that this is only used via a builtin!

	int lastcalledbuiltinnumber;			//useful with non-implemented opcodes.

	int (*RegisterFieldVar)				(progfuncs_t *prinst, unsigned int type, char *name, signed long requestedpos, signed long originalofs);

	char	*tempstringbase;				//for engine's use. Store your base tempstring pointer here.
	int		tempstringnum;			//for engine's use.

	string_t (*TempString)				(progfuncs_t *prinst, char *str);

	string_t (*StringToProgs)			(progfuncs_t *prinst, char *str);
	char *(ASMCALL *StringToNative)				(progfuncs_t *prinst, string_t str);
	int stringtablesize;

	int (*QueryField)					(progfuncs_t *prinst, unsigned int fieldoffset, etype_t *type, char **name, evalc_t *fieldcache);	//find info on a field definition at an offset

	void (*EntClear)					(progfuncs_t *progfuncs, struct edict_s *e);
	void (*FindPrefixGlobals)			(progfuncs_t *progfuncs, char *prefix, void (*found) (progfuncs_t *progfuncs, char *name, union eval_s *val, etype_t type) );

	void *(*AddressableAlloc)			(progfuncs_t *progfuncs, int ammount); /*returns memory within the qc block, use stringtoprogs to get a usable qc pointer/string*/
};

typedef struct progexterns_s {
	int progsversion;	//PROGSTRUCT_VERSION

	unsigned char *(*ReadFile) (const char *fname, void *buffer, int len);
	int (*FileSize) (const char *fname);	//-1 if file does not exist
	pbool (*WriteFile) (const char *name, void *data, int len);
	int (VARGS *printf) (const char *, ...) LIKEPRINTF(1);
	void (VARGS *Sys_Error) (const char *, ...) LIKEPRINTF(1);
	void (VARGS *Abort) (char *, ...) LIKEPRINTF(1);
	int edictsize;	//size of edict_t

	void (*entspawn) (struct edict_s *ent, int loading);	//ent has been spawned, but may not have all the extra variables (that may need to be set) set
	pbool (*entcanfree) (struct edict_s *ent);	//return true to stop ent from being freed
	void (ASMCALL *stateop) (progfuncs_t *prinst, float var, func_t func);	//what to do on qc's state opcode.
	void (ASMCALL *cstateop) (progfuncs_t *prinst, float vara, float varb, func_t currentfunc);		//a hexen2 opcode.
	void (ASMCALL *cwstateop) (progfuncs_t *prinst, float vara, float varb, func_t currentfunc);	//a hexen2 opcode.
	void (ASMCALL *thinktimeop) (progfuncs_t *prinst, struct edict_s *ent, float varb);			//a hexen2 opcode.


	//used when loading a game
	builtin_t *(*builtinsfor) (int num, int headercrc);	//must return a pointer to the builtins that were used before the state was saved.
	void (*loadcompleate) (int edictsize);	//notification to reset any pointers.
	pbool (*badfield)(progfuncs_t *prinst, struct edict_s *ent, const char *keyname, const char *value);	//called for any fields that are not registered

	void *(VARGS *memalloc) (int size);	//small string allocation	malloced and freed randomly by the executor. (use malloc if you want)
	void (VARGS *memfree) (void * mem);


	builtin_t *globalbuiltins;	//these are available to all progs
	int numglobalbuiltins;

	enum {PR_NOCOMPILE, PR_COMPILENEXIST, PR_COMPILEEXISTANDCHANGED, PR_COMPILECHANGED, PR_COMPILEALWAYS, PR_COMPILEIGNORE} autocompile;

	double *gametime;	//used to prevent the vm from reusing an entity faster than 2 secs.

	struct edict_s **sv_edicts;	//pointer to the engine's reference to world.
	unsigned int *sv_num_edicts;		//pointer to the engine's edict count.

	int (*useeditor) (progfuncs_t *prinst, char *filename, int line, int nump, char **parms);	//called on syntax errors or step-by-step debugging.
	void (*addressablerelocated) (progfuncs_t *progfuncs, char *oldb, char *newb, int oldlen);	//called when the progs memory was resized. you must fix up all pointers to globals, strings, fields, addressable blocks.

	void *user;	/*contains the owner's world reference in FTE*/
} progparms_t, progexterns_t;

//FIXMEs
void QC_AddSharedVar(progfuncs_t *progfuncs, int start, int size);
void QC_AddSharedFieldVar(progfuncs_t *progfuncs, int num, char *relstringtable);
void ED_Print(progfuncs_t *progfuncs, struct edict_s *ed);
char *PR_RemoveProgsString(progfuncs_t *progfuncs, string_t str);
int PR_GetFuncArgCount(progfuncs_t *progfuncs, func_t func);

#if defined(QCLIBDLL_EXPORTS)
__declspec(dllexport)
#endif
progfuncs_t * InitProgs(progparms_t *ext);
#if defined(QCLIBDLL_EXPORTS)
__declspec(dllexport)
#endif
void CloseProgs(progfuncs_t *inst);

#ifndef COMPILER
typedef union eval_s
{
	string_t		string;
	float			_float;
	float			_vector[3];
	func_t			function;
	int				_int;
	int				edict;
	float		prog;	//so it can easily be changed
} eval_t;
#endif

#define PR_CURRENT	-1
#define PR_ANY	-2	//not always valid. Use for finding funcs
#define PR_ANYBACK -3
#define PROGSTRUCT_VERSION 2


#ifndef DLL_PROG
#define PR_Configure(pf, memsize, max_progs)				(*pf->Configure)			(pf, memsize, max_progs)
#define PR_LoadProgs(pf, s, headercrc, builtins, numb)		(*pf->LoadProgs)			(pf, s, headercrc, builtins, numb)
#define PR_InitEnts(pf, maxents)							(*pf->InitEnts)				(pf, maxents)
#define PR_ExecuteProgram(pf, fnum)							(*pf->ExecuteProgram)		(pf, fnum)
#define PR_SwitchProgs(pf, num)								(*pf->SwitchProgs)			(pf, num)
#define PR_globals(pf, num)									(*pf->globals)				(pf, num)
#define PR_entvars(pf, ent)									(*pf->entvars)				(pf, ent)

#define PR_RegisterFieldVar(pf,type,name,reqofs,qcofs)		(*pf->RegisterFieldVar)		(pf,type,name,reqofs,qcofs)

#define ED_Alloc(pf)										(*pf->EntAlloc)				(pf)
#define ED_Free(pf, ed)										(*pf->EntFree)				(pf, ed)
#define ED_Clear(pf, ed)									(*pf->EntClear)				(pf, ed)

#define PR_LoadEnts(pf, s, kf)								(*pf->load_ents)			(pf, s, kf)
#define PR_SaveEnts(pf, buf, size, mode)					(*pf->save_ents)			(pf, buf, size, mode)

#define EDICT_NUM(pf, num)									(*pf->EDICT_NUM)			(pf, num)
#define NUM_FOR_EDICT(pf, e)								(*pf->NUM_FOR_EDICT)		(pf, e)
#define SetGlobalEdict(pf, ed, ofs)							(*pf->SetGlobalEdict)		(pf, ed, ofs)
#define PR_VarString(pf,first)								(*pf->VarString)			(pf,first)

#define PR_StartCompile(pf,argc,argv)						(*pf->StartCompile)			(pf,argc,argv)
#define PR_ContinueCompile(pf)								(*pf->ContinueCompile)		(pf)

#define PR_StackTrace(pf)									(*pf->StackTrace)			(pf)
#define PR_AbortStack(pf)									(*pf->AbortStack)			(pf)

#define PR_RunError(pf,str)									(*pf->RunError)				(pf,str)

#define PR_PrintEdict(pf,ed)								(*pf->PrintEdict)			(pf, ed)

#define PR_FindFunction(pf, name, num)						(*pf->FindFunction)			(pf, name, num)
#define PR_FindGlobal(pf, name, progs, type)				(*pf->FindGlobal)			(pf, name, progs, type)
#define PR_AddString(pf, ed, len)							(*pf->AddString)			(pf, ed, len)
#define PR_Alloc(pf,size,whatfor)							(*pf->Tempmem)				(pf, size, whatfor)
#define PR_AddressableAlloc(pf,size)						(*pf->AddressableAlloc)		(pf, size)

#define PROG_TO_EDICT(pf, ed)								(*pf->ProgsToEdict)			(pf, ed)
#define EDICT_TO_PROG(pf, ed)								(*pf->EdictToProgs)			(pf, (struct edict_s*)ed)

#define PR_RegisterBuiltin(pf, name, func)					(*pf->RegisterBuiltin)		(pf, name, func)

#define PR_GetString(pf,s)									(*pf->StringToNative)		(pf, s)
#define PR_GetStringOfs(pf,o)								(*pf->StringToNative)		(pf, G_INT(o))
#define PR_SetString(pf, s)									(*pf->StringToProgs)		(pf, s)

#define NEXT_EDICT(pf,o)		EDICT_NUM(pf, NUM_FOR_EDICT(pf, o)+1)
#define	RETURN_EDICT(pf, e) (((int *)pr_globals)[OFS_RETURN] = EDICT_TO_PROG(pf, e))


//builtin funcs (which operate on globals)
//To use these outside of builtins, you will likly have to use the 'globals' method.
#define	G_FLOAT(o) (((float *)pr_globals)[o])
#define	G_FLOAT2(o) (((float *)pr_globals)[OFS_PARM0 + o*3])
#define	G_INT(o) (((int *)pr_globals)[o])
#define	G_EDICT(pf, o) PROG_TO_EDICT(pf, G_INT(o)) //((edict_t *)((char *) sv.edicts+ *(int *)&((float *)pr_globals)[o]))
#define G_EDICTNUM(pf, o) NUM_FOR_EDICT(pf, G_EDICT(pf, o))
#define	G_VECTOR(o) (&((float *)pr_globals)[o])
#define	G_FUNCTION(o) (*(func_t *)&((float *)pr_globals)[o])

/*
#define PR_GetString(p,s) (s?s + p->stringtable:"")
#define PR_GetStringOfs(p,o) (G_INT(o)?G_INT(o) + p->stringtable:"")
#define PR_SetStringOfs(p,o,s) (G_INT(o) = s - p->stringtable)
*/
//#define PR_SetString(p, s) ((s&&*s)?(s - p->stringtable):0)
#define PR_NewString(p, s, l) PR_SetString(p, PR_AddString(p, s, l))
/**/

#define ev_prog ev_integer

#define E_STRING(o) (char *)(((int *)((char *)ed) + progparms.edictsize)[o])

//#define pr_global_struct pr_globals

#endif


#define	OFS_NULL		0
#define	OFS_RETURN		1
#define	OFS_PARM0		4		// leave 3 ofs for each parm to hold vectors
#define	OFS_PARM1		7
#define	OFS_PARM2		10
#define	OFS_PARM3		13
#define	OFS_PARM4		16
#define	OFS_PARM5		19
#define	OFS_PARM6		22
#define	OFS_PARM7		25
#define	RESERVED_OFS	28


#undef edict_t
#undef globalvars_t

#endif //PROGSLIB_H
