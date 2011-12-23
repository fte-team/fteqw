#define PROGSUSED
#include "progsint.h"
#include <stdlib.h>

typedef struct prmemb_s {
	struct prmemb_s *prev;
	int level;
} prmemb_t;
void *PRHunkAlloc(progfuncs_t *progfuncs, int ammount)
{
	prmemb_t *mem;
	ammount = sizeof(prmemb_t)+((ammount + 3)&~3);
	mem = memalloc(ammount); 
	memset(mem, 0, ammount);
	mem->prev = memb;
	if (!memb)
		mem->level = 1;
	else
		mem->level = ((prmemb_t *)memb)->level+1;
	memb = mem;

	return ((char *)mem)+sizeof(prmemb_t);
}

int PRHunkMark(progfuncs_t *progfuncs)
{
	return ((prmemb_t *)memb)->level;
}
void PRHunkFree(progfuncs_t *progfuncs, int mark)
{
	prmemb_t *omem;
	while(memb)
	{
		if (memb->level <= mark)
			return;

		omem = memb;
		memb = memb->prev;
		memfree(omem);
	}
	return;
}

/*if we ran out of memory, the vm can allocate a new block, but doing so requires fixing up all sorts of pointers*/
void PRAddressableRelocate(progfuncs_t *progfuncs, char *oldb, char *newb, int oldlen)
{
	unsigned int i;
	edictrun_t *e;
	for (i=0 ; i<maxedicts; i++)
	{
		e = (edictrun_t *)(prinst->edicttable[i]);
		if (e && (char*)e->fields >= oldb && (char*)e->fields < oldb+oldlen)
			e->fields = ((char*)e->fields - oldb) + newb;
	}

	if (progfuncs->stringtable >= oldb && progfuncs->stringtable < oldb+oldlen)
		progfuncs->stringtable = (progfuncs->stringtable - oldb) + newb;

	for (i=0; i < maxprogs; i++)
	{
		if ((char*)prinst->progstate[i].globals >= oldb && (char*)prinst->progstate[i].globals < oldb+oldlen)
			prinst->progstate[i].globals = (float*)(((char*)prinst->progstate[i].globals - oldb) + newb);
		if (prinst->progstate[i].strings >= oldb && prinst->progstate[i].strings < oldb+oldlen)
			prinst->progstate[i].strings = (prinst->progstate[i].strings - oldb) + newb;
	}

	for (i = 0; i < numfields; i++)
	{
		if (field[i].name >= oldb && field[i].name < oldb+oldlen)
			field[i].name = (field[i].name - oldb) + newb;
	}

	externs->addressablerelocated(progfuncs, oldb, newb, oldlen);
}

//for 64bit systems. :)
//addressable memory is memory available to the vm itself for writing.
//once allocated, it cannot be freed for the lifetime of the VM.
void *PRAddressableAlloc(progfuncs_t *progfuncs, int ammount)
{
	ammount = (ammount + 4)&~3;	//round up to 4
	if (addressableused + ammount > addressablesize)
	{
		/*only do this if the caller states that it can cope with addressable-block relocations/resizes*/
		if (externs->addressablerelocated)
		{
#ifdef _WIN32
			char *newblock;
		#if 0//def _DEBUG
			int oldtot = addressablesize;
		#endif
			int newsize = (addressableused + ammount + 4096) & ~(4096-1);
			newblock = VirtualAlloc (NULL, addressablesize, MEM_RESERVE, PAGE_NOACCESS);
			if (newblock)
			{
				VirtualAlloc (newblock, addressableused, MEM_COMMIT, PAGE_READWRITE);
				memcpy(newblock, addressablehunk, addressableused);
		#if 0//def _DEBUG
				VirtualAlloc (addressablehunk, oldtot, MEM_RESERVE, PAGE_NOACCESS);
		#else
				VirtualFree (addressablehunk, 0, MEM_RELEASE);
		#endif
				PRAddressableRelocate(progfuncs, addressablehunk, newblock, addressableused);
				addressablehunk = newblock;
				addressablesize = newsize;
			}
#else
			char *newblock;
			int newsize = (addressableused + ammount + 1024*1024) & ~(1024*1024-1);
			newblock = realloc(newblock, addressablesize);
			if (newblock)
			{
				PRAddressableRelocate(progfuncs, addressablehunk, newblock, addressableused);
				addressablehunk = newblock;
				addressablesize = newsize;
			}
#endif
		}

		if (addressableused + ammount > addressablesize)
			Sys_Error("Not enough addressable memory for progs VM");
	}

	addressableused += ammount;

#ifdef _WIN32
	if (!VirtualAlloc (addressablehunk, addressableused, MEM_COMMIT, PAGE_READWRITE))
		Sys_Error("VirtualAlloc failed. Blame windows.");
#endif

	return &addressablehunk[addressableused-ammount];
}

void PRAddressableFlush(progfuncs_t *progfuncs, int totalammount)
{
	addressableused = 0;
	if (totalammount < 0)	//flush
	{
		totalammount = addressablesize;
//		return;
	}

#ifdef _WIN32
	if (addressablehunk && addressablesize != totalammount)
	{
		VirtualFree(addressablehunk, 0, MEM_RELEASE);	//doesn't this look complicated? :p
		addressablehunk = NULL;
	}
	if (!addressablehunk)
		addressablehunk = VirtualAlloc (addressablehunk, totalammount, MEM_RESERVE, PAGE_NOACCESS);
#else
	if (addressablehunk)
		free(addressablehunk);
	addressablehunk = malloc(totalammount);	//linux will allocate-on-use anyway, which is handy.
//	memset(addressablehunk, 0xff, totalammount);
#endif
	if (!addressablehunk)
		Sys_Error("Out of memory\n");
	addressablesize = totalammount;
}

int PR_InitEnts(progfuncs_t *progfuncs, int max_ents)
{
	maxedicts = max_ents;

	sv_num_edicts = 0;

	max_fields_size = fields_size;

	prinst->edicttable = PRHunkAlloc(progfuncs, maxedicts*sizeof(struct edicts_s *));
	sv_edicts = PRHunkAlloc(progfuncs, externs->edictsize);
	prinst->edicttable[0] = sv_edicts;
	((edictrun_t*)prinst->edicttable[0])->fields = PRAddressableAlloc(progfuncs, max_fields_size);
	QC_ClearEdict(progfuncs, sv_edicts);
	sv_num_edicts = 1;

	if (externs->entspawn)
		externs->entspawn((struct edict_s *)sv_edicts, false);

	return max_fields_size;
}
edictrun_t tempedict;	//used as a safty buffer
float tempedictfields[2048];

void PR_Configure (progfuncs_t *progfuncs, int addressable_size, int max_progs)	//can be used to wipe all memory
{
	unsigned int i;
	edictrun_t *e;

	max_fields_size=0;
	fields_size = 0;
	progfuncs->stringtable = 0;
	QC_StartShares(progfuncs);
	QC_InitShares(progfuncs);

	for ( i=1 ; i<maxedicts; i++)
	{
		e = (edictrun_t *)(prinst->edicttable[i]);
		prinst->edicttable[i] = NULL;
//		e->entnum = i;
		if (e)
			memfree(e);
	}

	PRHunkFree(progfuncs, 0);	//clear mem - our hunk may not be a real hunk.
	if (addressable_size<0)
		addressable_size = 8*1024*1024;
	PRAddressableFlush(progfuncs, addressable_size);

	pr_progstate = PRHunkAlloc(progfuncs, sizeof(progstate_t) * max_progs);

/*		for(a = 0; a < max_progs; a++)
		{
			pr_progstate[a].progs = NULL;
		}		
*/
		
	maxprogs = max_progs;
	pr_typecurrent=-1;

	prinst->reorganisefields = false;

	maxedicts = 1;
	prinst->edicttable = &sv_edicts;
	sv_num_edicts = 1;	//set up a safty buffer so things won't go horribly wrong too often
	sv_edicts=(struct edict_s *)&tempedict;
	tempedict.readonly = true;
	tempedict.fields = tempedictfields;
	tempedict.isfree = false;
}



struct globalvars_s *PR_globals (progfuncs_t *progfuncs, progsnum_t pnum)
{
	if (pnum < 0)
	{
		if (!current_progstate)
		{
			static float fallback[RESERVED_OFS];
			return (struct globalvars_s *)fallback;	//err.. you've not loaded one yet.
		}
		return (struct globalvars_s *)current_progstate->globals;
	}
	return (struct globalvars_s *)pr_progstate[pnum].globals;
}

struct entvars_s *PR_entvars (progfuncs_t *progfuncs, struct edict_s *ed)
{
	if (((edictrun_t *)ed)->isfree)
		return NULL;

	return (struct entvars_s *)edvars(ed);
}

int PR_GetFuncArgCount(progfuncs_t *progfuncs, func_t func)
{
	unsigned int pnum;
	unsigned int fnum;
	dfunction_t *f;

	pnum = (func & 0xff000000)>>24;
	fnum = (func & 0x00ffffff);

	if (pnum >= (unsigned)maxprogs || !pr_progstate[pnum].functions)
		return -1;
	else if (fnum >= pr_progstate[pnum].progs->numfunctions)
		return -1;
	else
	{
		f = pr_progstate[pnum].functions + fnum;
		return f->numparms;
	}
}

func_t PR_FindFunc(progfuncs_t *progfuncs, char *funcname, progsnum_t pnum)
{
	dfunction_t *f=NULL;
	if (pnum == PR_ANY)
	{
		for (pnum = 0; (unsigned)pnum < maxprogs; pnum++)
		{
			if (!pr_progstate[pnum].progs)
				continue;
			f = ED_FindFunction(progfuncs, funcname, &pnum, pnum);
			if (f)
				break;
		}
	}
	else if (pnum == PR_ANYBACK)	//run backwards
	{
		for (pnum = maxprogs-1; pnum >= 0; pnum--)
		{
			if (!pr_progstate[pnum].progs)
				continue;
			f = ED_FindFunction(progfuncs, funcname, &pnum, pnum);
			if (f)
				break;
		}
	}
	else
		f = ED_FindFunction(progfuncs, funcname, &pnum, pnum);
	if (!f)
		return 0;

	{
	ddef16_t *var16;
	ddef32_t *var32;
	switch(pr_progstate[pnum].structtype)
	{
	case PST_KKQWSV:
	case PST_DEFAULT:
		var16 = ED_FindTypeGlobalFromProgs16(progfuncs, funcname, pnum, ev_function);	//we must make sure we actually have a function def - 'light' is defined as a field before it is defined as a function.
		if (!var16)
			return (f - pr_progstate[pnum].functions) | (pnum << 24);
		return *(int *)&pr_progstate[pnum].globals[var16->ofs];
	case PST_QTEST:
	case PST_FTE32:
		var32 = ED_FindTypeGlobalFromProgs32(progfuncs, funcname, pnum, ev_function);	//we must make sure we actually have a function def - 'light' is defined as a field before it is defined as a function.
		if (!var32)
			return (f - pr_progstate[pnum].functions) | (pnum << 24);
		return *(int *)&pr_progstate[pnum].globals[var32->ofs];	
	}
	Sys_Error("Error with def size (PR_FindFunc)");	
	}
	return 0;
}

void QC_FindPrefixedGlobals(progfuncs_t *progfuncs, char *prefix, void (*found) (progfuncs_t *progfuncs, char *name, union eval_s *val, etype_t type) )
{
	unsigned int i;
	ddef16_t		*def16;
	ddef32_t		*def32;
	int len = strlen(prefix);
	unsigned int pnum;

	for (pnum = 0; pnum < maxprogs; pnum++)
	{
		if (!pr_progstate[pnum].progs)
			continue;

		switch(pr_progstate[pnum].structtype)
		{
		case PST_DEFAULT:
		case PST_KKQWSV:
			for (i=1 ; i<pr_progstate[pnum].progs->numglobaldefs ; i++)
			{
				def16 = &pr_progstate[pnum].globaldefs16[i];
				if (!strncmp(def16->s_name+progfuncs->stringtable,prefix, len))
					found(progfuncs, def16->s_name+progfuncs->stringtable, (eval_t *)&pr_progstate[pnum].globals[def16->ofs], def16->type);
			}
			break;
		case PST_QTEST:
		case PST_FTE32:
			for (i=1 ; i<pr_progstate[pnum].progs->numglobaldefs ; i++)
			{
				def32 = &pr_progstate[pnum].globaldefs32[i];
				if (!strncmp(def32->s_name+progfuncs->stringtable,prefix, len))
					found(progfuncs, def32->s_name+progfuncs->stringtable, (eval_t *)&pr_progstate[pnum].globals[def32->ofs], def32->type);
			}
			break;
		}
	}
}

eval_t *PR_FindGlobal(progfuncs_t *progfuncs, char *globname, progsnum_t pnum, etype_t *type)
{
	unsigned int i;
	ddef16_t *var16;
	ddef32_t *var32;
	if (pnum == PR_CURRENT)
		pnum = pr_typecurrent;
	if (pnum == PR_ANY)
	{
		eval_t *ev;
		for (i = 0; i < maxprogs; i++)
		{
			if (!pr_progstate[i].progs)
				continue;
			ev = PR_FindGlobal(progfuncs, globname, i, type);
			if (ev)
				return ev;
		}
		return NULL;
	}
	if (pnum < 0 || (unsigned)pnum >= maxprogs || !pr_progstate[pnum].progs)
		return NULL;
	switch(pr_progstate[pnum].structtype)
	{
	case PST_DEFAULT:
	case PST_KKQWSV:
		if (!(var16 = ED_FindGlobalFromProgs16(progfuncs, globname, pnum)))
			return NULL;

		if (type)
			*type = var16->type;
		return (eval_t *)&pr_progstate[pnum].globals[var16->ofs];
	case PST_QTEST:
	case PST_FTE32:
		if (!(var32 = ED_FindGlobalFromProgs32(progfuncs, globname, pnum)))
			return NULL;

		if (type)
			*type = var32->type;
		return (eval_t *)&pr_progstate[pnum].globals[var32->ofs];
	}
	Sys_Error("Error with def size (PR_FindGlobal)");
	return NULL;
}

void SetGlobalEdict(progfuncs_t *progfuncs, struct edict_s *ed, int ofs)
{
	((int*)pr_globals)[ofs] = EDICT_TO_PROG(progfuncs, ed);
}

char *PR_VarString (progfuncs_t *progfuncs, int	first)
{
	int		i;
	static char out[1024];
	char *s;
	
	out[0] = 0;
	for (i=first ; i<pr_argc ; i++)
	{
		if (G_STRING(OFS_PARM0+i*3))
		{
			s=G_STRING((OFS_PARM0+i*3)) + progfuncs->stringtable;
			if (strlen(out) + strlen(s) + 1 >= sizeof(out))
				return out;
			strcat (out, s);
		}
	}
	return out;
}

int PR_QueryField (progfuncs_t *progfuncs, unsigned int fieldoffset, etype_t *type, char **name, evalc_t *fieldcache)
{
	fdef_t *var;
	var = ED_FieldAtOfs(progfuncs, fieldoffset);
	if (!var)
		return false;

	if (type)
		*type = var->type & ~(DEF_SAVEGLOBAL|DEF_SHARED);
	if (name)
		*name = var->name;
	if (fieldcache)
	{
		fieldcache->ofs32 = var;
		fieldcache->varname = var->name;
	}
		
	return true;
}

eval_t *GetEdictFieldValue(progfuncs_t *progfuncs, struct edict_s *ed, char *name, evalc_t *cache)
{
	fdef_t *var;
	if (!cache)
	{
		var = ED_FindField(progfuncs, name);
		if (!var)
			return NULL;
		return (eval_t *) &(((int*)(((edictrun_t*)ed)->fields))[var->ofs]);
	}
	if (!cache->varname)
	{
		cache->varname = name;
		var = ED_FindField(progfuncs, name);		
		if (!var)
		{
			cache->ofs32 = NULL;
			return NULL;
		}
		cache->ofs32 = var;
		cache->varname = var->name;
		if (!ed)
			return (void*)~0;	//something not null
		return (eval_t *) &(((int*)(((edictrun_t*)ed)->fields))[var->ofs]);
	}
	if (cache->ofs32 == NULL)
		return NULL;
	return (eval_t *) &(((int*)(((edictrun_t*)ed)->fields))[cache->ofs32->ofs]);
}

struct edict_s *ProgsToEdict (progfuncs_t *progfuncs, int progs)
{
	if ((unsigned)progs >= (unsigned)maxedicts)
	{
		printf("Bad entity index %i\n", progs);
		progs = 0;
	}
	return (struct edict_s *)PROG_TO_EDICT(progfuncs, progs);
}
int EdictToProgs (progfuncs_t *progfuncs, struct edict_s *ed)
{
	return EDICT_TO_PROG(progfuncs, ed);
}

string_t PR_StringToProgs			(progfuncs_t *progfuncs, char *str)
{
	char **ntable;
	int i, free=-1;

	if (!str)
		return 0;

	if (str-progfuncs->stringtable < addressableused)
		return str - progfuncs->stringtable;

	for (i = prinst->numallocedstrings-1; i >= 0; i--)
	{
		if (prinst->allocedstrings[i] == str)
			return (string_t)((unsigned int)i | 0x80000000);
		if (!prinst->allocedstrings[i])
			free = i;
	}

	if (free != -1)
	{
		i = free;
		prinst->allocedstrings[i] = str;
		return (string_t)((unsigned int)i | 0x80000000);
	}

	prinst->maxallocedstrings += 1024;
	ntable = memalloc(sizeof(char*) * prinst->maxallocedstrings); 
	memcpy(ntable, prinst->allocedstrings, sizeof(char*) * prinst->numallocedstrings);
	memset(ntable + prinst->numallocedstrings, 0, sizeof(char*) * (prinst->maxallocedstrings - prinst->numallocedstrings));
	prinst->numallocedstrings = prinst->maxallocedstrings;
	if (prinst->allocedstrings)
		memfree(prinst->allocedstrings);
	prinst->allocedstrings = ntable;

	for (i = prinst->numallocedstrings-1; i >= 0; i--)
	{
		if (!prinst->allocedstrings[i])
		{
			prinst->allocedstrings[i] = str;
			return (string_t)((unsigned int)i | 0x80000000);
		}
	}

	return 0;
}

char *PR_RemoveProgsString				(progfuncs_t *progfuncs, string_t str)
{
	char *ret;

	//input string is expected to be an allocated string
	//if its a temp, or a constant, just return NULL.
	if ((unsigned int)str & 0xc0000000)
	{
		if ((unsigned int)str & 0x80000000)
		{
			int i = str & ~0x80000000;
			if (i >= prinst->numallocedstrings)
			{
				pr_trace = 1;
				return NULL;
			}
			if (prinst->allocedstrings[i])
			{
				ret = prinst->allocedstrings[i];
				prinst->allocedstrings[i] = NULL;	//remove it
				return ret;
			}
			else
			{
				pr_trace = 1;
				return NULL;	//urm, was freed...
			}
		}
	}
	pr_trace = 1;
	return NULL;
}

char *ASMCALL PR_StringToNative				(progfuncs_t *progfuncs, string_t str)
{
	if ((unsigned int)str & 0xc0000000)
	{
		if ((unsigned int)str & 0x80000000)
		{
			int i = str & ~0x80000000;
			if (i >= prinst->numallocedstrings)
			{
				printf("invalid string %x\n", str);
				PR_StackTrace(progfuncs);
				pr_trace = 1;
				return "";
			}
			if (prinst->allocedstrings[i])
				return prinst->allocedstrings[i];
			else
			{
				printf("invalid string %x\n", str);
				PR_StackTrace(progfuncs);
				pr_trace = 1;
				return "";	//urm, was freed...
			}
		}
		if ((unsigned int)str & 0x40000000)
		{
			int i = str & ~0x40000000;
			if (i >= prinst->numtempstrings)
			{
				printf("invalid temp string %x\n", str);
				PR_StackTrace(progfuncs);
				pr_trace = 1;
				return "";
			}
			return prinst->tempstrings[i];
		}
	}

	if (str >= addressableused)
	{
		printf("invalid string offset %x\n", str);
		PR_StackTrace(progfuncs);
		pr_trace = 1;
		return "";
	}
	return progfuncs->stringtable + str;
}


string_t PR_AllocTempString			(progfuncs_t *progfuncs, char *str)
{
	char **ntable;
	int newmax;
	int i;

	if (!str)
		return 0;

	if (prinst->numtempstrings == prinst->maxtempstrings)
	{
		newmax = prinst->maxtempstrings += 1024;
		prinst->maxtempstrings += 1024;
		ntable = memalloc(sizeof(char*) * newmax);
		memcpy(ntable, prinst->tempstrings, sizeof(char*) * prinst->numtempstrings);
		prinst->maxtempstrings = newmax;
		if (prinst->tempstrings)
			memfree(prinst->tempstrings);
		prinst->tempstrings = ntable;
	}

	i = prinst->numtempstrings;
	if (i == 0x10000000)
		return 0;

	prinst->numtempstrings++;

	prinst->tempstrings[i] = memalloc(strlen(str)+1);
	strcpy(prinst->tempstrings[i], str);

	return (string_t)((unsigned int)i | 0x40000000);
}

void PR_FreeTemps			(progfuncs_t *progfuncs, int depth)
{
	int i;
	if (depth > prinst->numtempstrings)
	{
		Sys_Error("QC Temp stack inverted\n");
		return;
	}
	for (i = depth; i < prinst->numtempstrings; i++)
	{
		memfree(prinst->tempstrings[i]);
	}

	prinst->numtempstrings = depth;
}


struct qcthread_s *PR_ForkStack	(progfuncs_t *progfuncs);
void PR_ResumeThread			(progfuncs_t *progfuncs, struct qcthread_s *thread);
void	PR_AbortStack			(progfuncs_t *progfuncs);


void RegisterBuiltin(progfuncs_t *progfncs, char *name, builtin_t func);

progfuncs_t deffuncs = {
	PROGSTRUCT_VERSION,
	PR_Configure,
	PR_LoadProgs,
	PR_InitEnts,
	PR_ExecuteProgram,
	PR_SwitchProgs,
	PR_globals,
	PR_entvars,
	PR_RunError,
	ED_Print,
	ED_Alloc,
	ED_Free,

	EDICT_NUM,
	NUM_FOR_EDICT,


	SetGlobalEdict,

	PR_VarString,

	NULL,
	PR_FindFunc,
#ifdef MINIMAL
	NULL,
	NULL,
#else
	Comp_Begin,
	Comp_Continue,
#endif

	filefromprogs,
	NULL,//filefromnewprogs,

	SaveEnts,
	LoadEnts,

	SaveEnt,
	RestoreEnt,

	PR_FindGlobal,
	ED_NewString,
	(void*)PRHunkAlloc,

	GetEdictFieldValue,
	ProgsToEdict,
	EdictToProgs,

	EvaluateDebugString,

	NULL,
	PR_StackTrace,

	PR_ToggleBreakpoint,
	0,
	NULL,
#ifdef MINIMAL
	NULL,
#else
	Decompile,
#endif
	NULL,
	NULL,
	RegisterBuiltin,

	0,
	0,

	PR_ForkStack,
	PR_ResumeThread,
	PR_AbortStack,

	0,

	QC_RegisterFieldVar,

	0,
	0,

	PR_AllocTempString,

	PR_StringToProgs,
	PR_StringToNative,
	0,
	PR_QueryField,
	QC_ClearEdict,
	QC_FindPrefixedGlobals,
	PRAddressableAlloc
};
#undef printf

//defs incase following structure is not passed.
struct edict_s *safesv_edicts;
int safesv_num_edicts;
double safetime=0;

progexterns_t defexterns = {
	PROGSTRUCT_VERSION,		

	NULL, //char *(*ReadFile) (char *fname, void *buffer, int len);
	NULL, //int (*FileSize) (char *fname);	//-1 if file does not exist
	NULL, //bool (*WriteFile) (char *name, void *data, int len);
	printf, //void (*printf) (char *, ...);
	(void*)exit, //void (*Sys_Error) (char *, ...);
	NULL, //void (*Abort) (char *, ...);
	sizeof(edictrun_t), //int edictsize;	//size of edict_t

	NULL, //void (*entspawn) (struct edict_s *ent);	//ent has been spawned, but may not have all the extra variables (that may need to be set) set
	NULL, //bool (*entcanfree) (struct edict_s *ent);	//return true to stop ent from being freed
	NULL, //void (*stateop) (float var, func_t func);
	NULL,
	NULL,
	NULL,

	//used when loading a game
	NULL, //builtin_t *(*builtinsfor) (int num);	//must return a pointer to the builtins that were used before the state was saved.
	NULL, //void (*loadcompleate) (int edictsize);	//notification to reset any pointers.
	NULL,

	(void*)malloc, //void *(*memalloc) (int size);	//small string allocation	malloced and freed randomly by the executor. (use memalloc if you want)
	free, //void (*memfree) (void * mem);


	NULL, //builtin_t *globalbuiltins;	//these are available to all progs
	0, //int numglobalbuiltins;

	PR_NOCOMPILE,

	&safetime, //double *gametime;

	&safesv_edicts, //struct edict_s **sv_edicts;
	&safesv_num_edicts, //int *sv_num_edicts;

	NULL, //int (*useeditor) (char *filename, int line, int nump, char **parms);
};

//progfuncs_t *progfuncs = NULL;
#undef memfree
#undef prinst
#undef extensionbuiltin
#undef field
#undef shares
#undef sv_num_edicts


#ifdef QCLIBDLL_EXPORTS
__declspec(dllexport)
#endif 
void CloseProgs(progfuncs_t *inst)
{
//	extensionbuiltin_t *eb;
	void (VARGS *f) (void *);

	unsigned int i;
	edictrun_t *e;

	f = inst->parms->memfree;

	for ( i=1 ; i<inst->maxedicts; i++)
	{
		e = (edictrun_t *)(inst->prinst->edicttable[i]);
		inst->prinst->edicttable[i] = NULL;
		if (e)
		{
//			e->entnum = i;
			f(e);
		}
	}

	PRHunkFree(inst, 0);

#ifdef _WIN32
	VirtualFree(inst->addressablehunk, 0, MEM_RELEASE);	//doesn't this look complicated? :p
#else
	free(inst->addressablehunk);
#endif

	if (inst->prinst->allocedstrings)
		f(inst->prinst->allocedstrings);
	inst->prinst->allocedstrings = NULL;
	if (inst->prinst->tempstrings)
		f(inst->prinst->tempstrings);
	inst->prinst->tempstrings = NULL;


/*
	while(inst->prinst->extensionbuiltin)
	{
		eb = inst->prinst->extensionbuiltin->prev;
		f(inst->prinst->extensionbuiltin);
		inst->prinst->extensionbuiltin = eb;
	}
*/
	if (inst->prinst->field)
		f(inst->prinst->field);
	if (inst->prinst->shares)
		f(inst->prinst->shares);	//free memory
	f(inst->prinst);
	f(inst);
}

void RegisterBuiltin(progfuncs_t *progfuncs, char *name, builtin_t func)
{
/*
	extensionbuiltin_t *eb;
	eb = memalloc(sizeof(extensionbuiltin_t));
	eb->prev = progfuncs->prinst->extensionbuiltin;
	progfuncs->prinst->extensionbuiltin = eb;
	eb->name = name;
	eb->func = func;
*/
}

#ifndef WIN32
#define QCLIBINT	//don't use dllspecifications
#endif

#if defined(QCLIBDLL_EXPORTS)
__declspec(dllexport)
#endif
progfuncs_t * InitProgs(progexterns_t *ext)
{	
	progfuncs_t *funcs;

	if (!ext)
		ext = &defexterns;
	else
	{
		int i;
		if (ext->progsversion > PROGSTRUCT_VERSION)
			return NULL;

		for (i=0;i<sizeof(progexterns_t); i+=4)	//make sure there are no items left out.
			if (!*(int *)((char *)ext+i))
				*(int *)((char *)ext+i) = *(int *)((char *)&defexterns+i);		
	}	
#undef memalloc
#undef pr_trace
	funcs = ext->memalloc(sizeof(progfuncs_t));	
	memcpy(funcs, &deffuncs, sizeof(progfuncs_t));

	funcs->prinst = ext->memalloc(sizeof(prinst_t));
	memset(funcs->prinst,0, sizeof(prinst_t));

	funcs->pr_trace = &funcs->prinst->pr_trace;
	funcs->progstate = &funcs->pr_progstate;
	funcs->callargc = &funcs->pr_argc;

	funcs->parms = ext;

	SetEndian();
	
	return funcs;
}
















#ifdef QCC
void main (int argc, char **argv)
{
	progexterns_t ext;

	progfuncs_t *funcs;
	funcs = InitProgs(&ext);
	if (funcs->PR_StartCompile(argc, argv))
		while(funcs->PR_ContinueCompile());
}
#endif
