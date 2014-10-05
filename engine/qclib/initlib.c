#define PROGSUSED
#include "progsint.h"
#include <stdlib.h>
#define STRING_SPECMASK	0xc0000000	//
#define STRING_TEMP		0x80000000	//temp string, will be collected.
#define STRING_STATIC	0xc0000000	//pointer to non-qcvm string.
#define STRING_NORMAL_	0x00000000	//stringtable/mutable. should always be a fallthrough
#define STRING_NORMAL2_	0x40000000	//stringtable/mutable. should always be a fallthrough

typedef struct prmemb_s {
	struct prmemb_s *prev;
	int level;
} prmemb_t;
void *PRHunkAlloc(progfuncs_t *progfuncs, int ammount, char *name)
{
	prmemb_t *mem;
	ammount = sizeof(prmemb_t)+((ammount + 3)&~3);
	mem = progfuncs->funcs.parms->memalloc(ammount); 
	memset(mem, 0, ammount);
	mem->prev = memb;
	if (!memb)
		mem->level = 1;
	else
		mem->level = ((prmemb_t *)memb)->level+1;
	memb = mem;

	return ((char *)mem)+sizeof(prmemb_t);
}
void *PDECL QC_HunkAlloc(pubprogfuncs_t *ppf, int ammount, char *name)
{
	return PRHunkAlloc((progfuncs_t*)ppf, ammount, name);
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
		externs->memfree(omem);
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
		e = (edictrun_t *)(prinst.edicttable[i]);
		if (e && (char*)e->fields >= oldb && (char*)e->fields < oldb+oldlen)
			e->fields = ((char*)e->fields - oldb) + newb;
	}

	if (progfuncs->funcs.stringtable >= oldb && progfuncs->funcs.stringtable < oldb+oldlen)
		progfuncs->funcs.stringtable = (progfuncs->funcs.stringtable - oldb) + newb;

	for (i=0; i < maxprogs; i++)
	{
		if ((char*)prinst.progstate[i].globals >= oldb && (char*)prinst.progstate[i].globals < oldb+oldlen)
			prinst.progstate[i].globals = (float*)(((char*)prinst.progstate[i].globals - oldb) + newb);
		if (prinst.progstate[i].strings >= oldb && prinst.progstate[i].strings < oldb+oldlen)
			prinst.progstate[i].strings = (prinst.progstate[i].strings - oldb) + newb;
	}

	for (i = 0; i < prinst.numfields; i++)
	{
		if (prinst.field[i].name >= oldb && prinst.field[i].name < oldb+oldlen)
			prinst.field[i].name = (prinst.field[i].name - oldb) + newb;
	}

	externs->addressablerelocated(&progfuncs->funcs, oldb, newb, oldlen);
}

//for 64bit systems. :)
//addressable memory is memory available to the vm itself for writing.
//once allocated, it cannot be freed for the lifetime of the VM.
void *PRAddressableExtend(progfuncs_t *progfuncs, int ammount)
{
	ammount = (ammount + 4)&~3;	//round up to 4
	if (prinst.addressableused + ammount > prinst.addressablesize)
	{
		/*only do this if the caller states that it can cope with addressable-block relocations/resizes*/
		if (externs->addressablerelocated)
		{
#if defined(_WIN32) && !defined(WINRT)
			char *newblock;
		#if 0//def _DEBUG
			int oldtot = addressablesize;
		#endif
			int newsize = (prinst.addressableused + ammount + 4096) & ~(4096-1);
			newblock = VirtualAlloc (NULL, prinst.addressablesize, MEM_RESERVE, PAGE_NOACCESS);
			if (newblock)
			{
				VirtualAlloc (newblock, prinst.addressableused, MEM_COMMIT, PAGE_READWRITE);
				memcpy(newblock, prinst.addressablehunk, prinst.addressableused);
		#if 0//def _DEBUG
				VirtualAlloc (prinst.addressablehunk, oldtot, MEM_RESERVE, PAGE_NOACCESS);
		#else
				VirtualFree (prinst.addressablehunk, 0, MEM_RELEASE);
		#endif
				PRAddressableRelocate(progfuncs, prinst.addressablehunk, newblock, prinst.addressableused);
				prinst.addressablehunk = newblock;
				prinst.addressablesize = newsize;
			}
#else
			int newsize = (prinst.addressableused + ammount + 1024*1024) & ~(1024*1024-1);
			char *newblock = malloc(newsize);
			if (newblock)
			{
				PRAddressableRelocate(progfuncs, prinst.addressablehunk, newblock, prinst.addressableused);
				free(prinst.addressablehunk);
				prinst.addressablehunk = newblock;
				prinst.addressablesize = newsize;
			}
#endif
		}

		if (prinst.addressableused + ammount > prinst.addressablesize)
			Sys_Error("Not enough addressable memory for progs VM");
	}

	prinst.addressableused += ammount;
	progfuncs->funcs.stringtablesize = prinst.addressableused;

#if defined(_WIN32) && !defined(WINRT)
	if (!VirtualAlloc (prinst.addressablehunk, prinst.addressableused, MEM_COMMIT, PAGE_READWRITE))
		Sys_Error("VirtualAlloc failed. Blame windows.");
#endif

	return &prinst.addressablehunk[prinst.addressableused-ammount];
}


#define MARKER 0xF1E3E3E7u
typedef struct
{
	unsigned int next;
	unsigned int prev;
	unsigned int size;
} qcmemfreeblock_t;
typedef struct
{
	unsigned int marker;
	unsigned int size;
} qcmemusedblock_t;
static void PF_fmem_unlink(progfuncs_t *pr, qcmemfreeblock_t *p)
{
	qcmemfreeblock_t *np;
	if (p->prev)
	{
		np = (qcmemfreeblock_t*)(pr->funcs.stringtable + p->prev);
		np->next = p->next;
	}
	else
		pr->inst.mfreelist = p->next;
	if (p->next)
	{
		np = (qcmemfreeblock_t*)(pr->funcs.stringtable + p->next);
		np->prev = p->prev;
	}
}
/*
static void PR_memvalidate (progfuncs_t *progfuncs)
{
	qcmemfreeblock_t *p;
	unsigned int b,l;

	b = prinst.mfreelist;
	l = 0;
	while (b)
	{
		if (b < 0 || b >= prinst.addressableused)
		{
			printf("PF_memalloc: memory corruption\n");
			PR_StackTrace(&progfuncs->funcs, false);
			return;
		}
		p = (qcmemfreeblock_t*)(progfuncs->funcs.stringtable + b);

		if (p->prev != l ||
			(p->next && p->next < b + p->size) ||
			p->next >= prinst.addressableused ||
			b + p->size >= prinst.addressableused ||
			p->prev >= b)
		{
			printf("PF_memalloc: memory corruption\n");
			PR_StackTrace(&progfuncs->funcs, false);
			return;
		}
		l = b;
		b = p->next;
	}
}
*/
static void *PDECL PR_memalloc (pubprogfuncs_t *ppf, unsigned int size)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	qcmemfreeblock_t *p, *np;
	qcmemusedblock_t *ub = NULL;
	unsigned int b,n;
	/*round size up*/
	size = (size+sizeof(qcmemusedblock_t) + 63) & ~63;

	b = prinst.mfreelist;
	while (b)
	{
		if (b < 0 || b+sizeof(qcmemfreeblock_t) >= prinst.addressableused)
		{
			printf("PF_memalloc: memory corruption\n");
			PR_StackTrace(&progfuncs->funcs, false);
			return NULL;
		}
		p = (qcmemfreeblock_t*)(progfuncs->funcs.stringtable + b);
		if (p->size >= size)
		{
			if ((p->next && p->next < b + p->size) ||
				p->next >= prinst.addressableused ||
				b + p->size >= prinst.addressableused ||
				p->prev >= b)
			{
				printf("PF_memalloc: memory corruption\n");
				PR_StackTrace(&progfuncs->funcs, false);
				return NULL;
			}

			ub = (qcmemusedblock_t*)p;
			if (p->size > size + 63)
			{
				/*make a new header just after it, with basically the same properties, and shift the important fields over*/
				n = b + size;
				np = (qcmemfreeblock_t*)(progfuncs->funcs.stringtable + b + size);
				np->prev = p->prev;
				np->next = p->next;
				np->size = p->size - size;
				if (np->prev)
				{
					p = (qcmemfreeblock_t*)(progfuncs->funcs.stringtable + np->prev);
					p->next = n;
				}
				else
					prinst.mfreelist = n;
				if (p->next)
				{
					p = (qcmemfreeblock_t*)(progfuncs->funcs.stringtable + np->next);
					p->prev = n;
				}
			}
			else
			{
				size = p->size; /*alloc the entire block*/
				/*unlink this entry*/
				PF_fmem_unlink(progfuncs, p);
			}
			break;
		}
		b = p->next;
	}

	/*assign more space*/
	if (!ub)
	{
		ub = PRAddressableExtend(progfuncs, size);
		if (!ub)
		{
			printf("PF_memalloc: memory exausted\n");
			PR_StackTrace(&progfuncs->funcs, false);
			return NULL;
		}
	}
	memset(ub, 0, size);
	ub->marker = MARKER;
	ub->size = size;

//	PR_memvalidate(progfuncs);

	return ub+1;
}
static void PDECL PR_memfree (pubprogfuncs_t *ppf, void *memptr)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	qcmemusedblock_t *ub;
	qcmemfreeblock_t *p, *np, *pp; 
	unsigned int pa, na;	//prev addr, next addr
	unsigned int size;
	unsigned int ptr = memptr?((char*)memptr - progfuncs->funcs.stringtable):0;

	/*freeing NULL is ignored*/
	if (!ptr)
		return;
//	PR_memvalidate(progfuncs);
	if (ptr < sizeof(qcmemusedblock_t) || ptr >= prinst.addressableused)
	{
		if (ptr < sizeof(qcmemusedblock_t) && !*(char*)memptr)
		{
			//the empty string is a point of contention. while we can detect it from fteqcc, its best to not give any special favours (other than nicer debugging, where possible)
			//we might not actually spot it from other qccs, so warning about it where possible is probably a very good thing.
			printf("PF_memfree: unable to free the non-null empty string constant at %x\n", ptr);
		}
		else
			printf("PF_memfree: pointer invalid - out of range (%x >= %x)\n", ptr, (unsigned int)prinst.addressableused);
		PR_StackTrace(&progfuncs->funcs, false);
		return;
	}

	ub = (qcmemusedblock_t*)(progfuncs->funcs.stringtable + ptr);
	ub--;
	ptr = (char*)ub - progfuncs->funcs.stringtable;
	if (ub->marker != MARKER || ub->size <= sizeof(*ub) || ptr + ub->size > (unsigned int)prinst.addressableused)
	{
		printf("PR_memfree: pointer lacks marker - double-freed?\n");
		PR_StackTrace(&progfuncs->funcs, false);
		return;
	}
	ub->marker = 0;
	size = ub->size;

	for (na = prinst.mfreelist, pa = 0; ;)
	{
		if (na < 0 || na >= prinst.addressableused)
		{
			printf("PF_memfree: memory corruption\n");
			PR_StackTrace(&progfuncs->funcs, false);
			return;
		}
		if (!na || na >= ptr)
		{
			np = (qcmemfreeblock_t*)(progfuncs->funcs.stringtable + pa);
			if (pa && pa+np->size>ptr)
			{
				printf("PF_memfree: double free\n");
				PR_StackTrace(&progfuncs->funcs, false);
				return;
			}

			/*generate the free block, now we know its proper values*/
			p = (qcmemfreeblock_t*)(progfuncs->funcs.stringtable + ptr);
			np = na?(qcmemfreeblock_t*)(progfuncs->funcs.stringtable + na):NULL;
			pp = pa?(qcmemfreeblock_t*)(progfuncs->funcs.stringtable + pa):NULL;

			p->prev = pa;
			p->next = na;
			p->size = size;

			/*update the next's previous*/
			if (na)
			{
				np->prev = ptr;
			
				/*extend this block and kill the next if they are adjacent*/
				if (p->next == ptr + size)
				{
					p->size += np->size; 
					PF_fmem_unlink(progfuncs, np);
				}
			}

			/*update the link to get here*/
			if (!pa)
				prinst.mfreelist = ptr;
			else
			{
				pp->next = ptr;

				/*we're adjacent to the previous block, so merge them by killing the newly freed region*/
				if (na && pa + np->size == ptr)
				{
					p->size += np->size;
					PF_fmem_unlink(progfuncs, np);
				}

			}
			break;
		}

		pa = na;
		p = (qcmemfreeblock_t*)(progfuncs->funcs.stringtable + pa);
		na = p->next;
	}

//	PR_memvalidate(progfuncs);
}

void PRAddressableFlush(progfuncs_t *progfuncs, size_t totalammount)
{
	prinst.addressableused = 0;
	if (totalammount <= 0)	//flush
	{
		totalammount = prinst.addressablesize;
//		return;
	}

#if defined(_WIN32) && !defined(WINRT)
	if (prinst.addressablehunk && prinst.addressablesize != totalammount)
	{
		VirtualFree(prinst.addressablehunk, 0, MEM_RELEASE);	//doesn't this look complicated? :p
		prinst.addressablehunk = NULL;
	}
	if (!prinst.addressablehunk)
		prinst.addressablehunk = VirtualAlloc (prinst.addressablehunk, totalammount, MEM_RESERVE, PAGE_NOACCESS);
#else
	if (prinst.addressablehunk && prinst.addressablesize != totalammount)
	{
		free(prinst.addressablehunk);
		prinst.addressablehunk = NULL;
	}
	if (!prinst.addressablehunk)
		prinst.addressablehunk = malloc(totalammount);	//linux will allocate-on-use anyway, which is handy.
//	memset(prinst.addressablehunk, 0xff, totalammount);
#endif
	if (!prinst.addressablehunk)
		Sys_Error("Out of memory\n");
	prinst.addressablesize = totalammount;
	progfuncs->funcs.stringtablemaxsize = totalammount;
}

int PDECL PR_InitEnts(pubprogfuncs_t *ppf, int max_ents)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	maxedicts = max_ents;

	sv_num_edicts = 0;

#if 0
	{
		int i;
		for (i = 0; i < prinst.numfields; i++)
		{
			printf("%s(%i) %i -> %i\n", prinst.field[i].name, prinst.field[i].type, prinst.field[i].progsofs, prinst.field[i].ofs);
		}
	}
#endif

	max_fields_size = fields_size;

	prinst.edicttable = PRHunkAlloc(progfuncs, maxedicts*sizeof(struct edicts_s *), "edicttable");
	sv_edicts = PRHunkAlloc(progfuncs, externs->edictsize, "edict0");
	prinst.edicttable[0] = sv_edicts;
	((edictrun_t*)prinst.edicttable[0])->fields = PRAddressableExtend(progfuncs, max_fields_size);
	QC_ClearEdict(&progfuncs->funcs, sv_edicts);
	sv_num_edicts = 1;

	if (externs->entspawn)
		externs->entspawn((struct edict_s *)sv_edicts, false);

	return max_fields_size;
}
edictrun_t tempedict;	//used as a safty buffer
static float tempedictfields[2048];

static void PDECL PR_Configure (pubprogfuncs_t *ppf, size_t addressable_size, int max_progs, pbool profiling)	//can be used to wipe all memory
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	unsigned int i;
	edictrun_t *e;

	max_fields_size=0;
	fields_size = 0;
	progfuncs->funcs.stringtable = 0;
	QC_StartShares(progfuncs);
	QC_InitShares(progfuncs);

	for ( i=1 ; i<maxedicts; i++)
	{
		e = (edictrun_t *)(prinst.edicttable[i]);
		prinst.edicttable[i] = NULL;
//		e->entnum = i;
		if (e)
			externs->memfree(e);
	}

	PRHunkFree(progfuncs, 0);	//clear mem - our hunk may not be a real hunk.
	if (addressable_size<0 || addressable_size == (size_t)-1)
	{
#if defined(_WIN64) && !defined(WINRT)
		addressable_size = 0x80000000;	//use of virtual address space rather than physical memory means we can just go crazy and use the max of 2gb.
#else
		addressable_size = 32*1024*1024;
#endif
	}
	if (addressable_size > 0x80000000)
		addressable_size = 0x80000000;
	PRAddressableFlush(progfuncs, addressable_size);

	pr_progstate = PRHunkAlloc(progfuncs, sizeof(progstate_t) * max_progs, "progstatetable");

/*		for(a = 0; a < max_progs; a++)
		{
			pr_progstate[a].progs = NULL;
		}		
*/
		
	maxprogs = max_progs;
	pr_typecurrent=-1;

	prinst.reorganisefields = false;

	prinst.profiling = profiling;
	maxedicts = 1;
	prinst.edicttable = &sv_edicts;
	sv_num_edicts = 1;	//set up a safty buffer so things won't go horribly wrong too often
	sv_edicts=(struct edict_s *)&tempedict;
	tempedict.readonly = true;
	tempedict.fields = tempedictfields;
	tempedict.isfree = false;
}



struct globalvars_s *PDECL PR_globals (pubprogfuncs_t *ppf, progsnum_t pnum)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
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

struct entvars_s *PDECL PR_entvars (pubprogfuncs_t *ppf, struct edict_s *ed)
{
//	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	if (((edictrun_t *)ed)->isfree)
		return NULL;

	return (struct entvars_s *)edvars(ed);
}

int PDECL PR_GetFuncArgCount(pubprogfuncs_t *ppf, func_t func)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;

	unsigned int pnum;
	unsigned int fnum;
	mfunction_t *f;

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

func_t PDECL PR_FindFunc(pubprogfuncs_t *ppf, const char *funcname, progsnum_t pnum)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	mfunction_t *f=NULL;
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

void PDECL QC_FindPrefixedGlobals(pubprogfuncs_t *ppf, int pnum, char *prefix, void (PDECL *found) (pubprogfuncs_t *progfuncs, char *name, union eval_s *val, etype_t type, void *ctx), void *ctx)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	unsigned int i;
	ddef16_t		*def16;
	ddef32_t		*def32;
	int len = strlen(prefix);

	if (pnum == PR_CURRENT)
		pnum = pr_typecurrent;
	if (pnum == PR_ANY)
	{
		for (pnum = 0; (unsigned)pnum < maxprogs; pnum++)
		{
			if (!pr_progstate[pnum].progs)
				continue;
			QC_FindPrefixedGlobals(ppf, pnum, prefix, found, ctx);
		}
		return;
	}

	if (!pr_progstate[pnum].progs)
		return;

	switch(pr_progstate[pnum].structtype)
	{
	case PST_DEFAULT:
	case PST_KKQWSV:
		for (i=1 ; i<pr_progstate[pnum].progs->numglobaldefs ; i++)
		{
			def16 = &pr_progstate[pnum].globaldefs16[i];
			if (!strncmp(def16->s_name+progfuncs->funcs.stringtable,prefix, len))
				found(&progfuncs->funcs, def16->s_name+progfuncs->funcs.stringtable, (eval_t *)&pr_progstate[pnum].globals[def16->ofs], def16->type, ctx);
		}
		break;
	case PST_QTEST:
	case PST_FTE32:
		for (i=1 ; i<pr_progstate[pnum].progs->numglobaldefs ; i++)
		{
			def32 = &pr_progstate[pnum].globaldefs32[i];
			if (!strncmp(def32->s_name+progfuncs->funcs.stringtable,prefix, len))
				found(&progfuncs->funcs, def32->s_name+progfuncs->funcs.stringtable, (eval_t *)&pr_progstate[pnum].globals[def32->ofs], def32->type, ctx);
		}
		break;
	}
}

eval_t *PDECL PR_FindGlobal(pubprogfuncs_t *ppf, const char *globname, progsnum_t pnum, etype_t *type)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
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
			ev = PR_FindGlobal(&progfuncs->funcs, globname, i, type);
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

void PDECL SetGlobalEdict(pubprogfuncs_t *ppf, struct edict_s *ed, int ofs)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	((int*)pr_globals)[ofs] = EDICT_TO_PROG(progfuncs, ed);
}

char *PDECL PR_VarString (pubprogfuncs_t *ppf, int	first)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	int		i;
	static char out[1024];
	char *s;
	
	out[0] = 0;
	for (i=first ; i<progfuncs->funcs.callargc ; i++)
	{
		if (G_STRING(OFS_PARM0+i*3))
		{
			s=G_STRING((OFS_PARM0+i*3)) + progfuncs->funcs.stringtable;
			if (strlen(out) + strlen(s) + 1 >= sizeof(out))
				return out;
			strcat (out, s);
		}
	}
	return out;
}

int PDECL PR_QueryField (pubprogfuncs_t *ppf, unsigned int fieldoffset, etype_t *type, char **name, evalc_t *fieldcache)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
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

eval_t *PDECL QC_GetEdictFieldValue(pubprogfuncs_t *ppf, struct edict_s *ed, char *name, evalc_t *cache)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
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

struct edict_s *PDECL ProgsToEdict (pubprogfuncs_t *ppf, int progs)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	if ((unsigned)progs >= (unsigned)maxedicts)
	{
		printf("Bad entity index %i\n", progs);
		if (pr_depth)
		{
			PR_StackTrace (ppf, false);
//			progfuncs->funcs.pr_trace += 1;
		}
		progs = 0;
	}
	return (struct edict_s *)PROG_TO_EDICT(progfuncs.inst, progs);
}
int PDECL EdictToProgs (pubprogfuncs_t *ppf, struct edict_s *ed)
{
//	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	return EDICT_TO_PROG(progfuncs, ed);
}

string_t PDECL PR_StringToProgs			(pubprogfuncs_t *ppf, const char *str)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	char **ntable;
	int i, free=-1;

	if (!str)
		return 0;

	if (str >= progfuncs->funcs.stringtable && str < progfuncs->funcs.stringtable + prinst.addressableused)
		return str - progfuncs->funcs.stringtable;

	for (i = prinst.numallocedstrings-1; i >= 0; i--)
	{
		if (prinst.allocedstrings[i] == str)
			return (string_t)((unsigned int)i | STRING_STATIC);
		if (!prinst.allocedstrings[i])
			free = i;
	}

	if (free != -1)
	{
		i = free;
		prinst.allocedstrings[i] = (char*)str;
		return (string_t)((unsigned int)i | STRING_STATIC);
	}

	prinst.maxallocedstrings += 1024;
	ntable = progfuncs->funcs.parms->memalloc(sizeof(char*) * prinst.maxallocedstrings); 
	memcpy(ntable, prinst.allocedstrings, sizeof(char*) * prinst.numallocedstrings);
	memset(ntable + prinst.numallocedstrings, 0, sizeof(char*) * (prinst.maxallocedstrings - prinst.numallocedstrings));
	prinst.numallocedstrings = prinst.maxallocedstrings;
	if (prinst.allocedstrings)
		progfuncs->funcs.parms->memfree(prinst.allocedstrings);
	prinst.allocedstrings = ntable;

	for (i = prinst.numallocedstrings-1; i >= 0; i--)
	{
		if (!prinst.allocedstrings[i])
		{
			prinst.allocedstrings[i] = (char*)str;
			return (string_t)((unsigned int)i | STRING_STATIC);
		}
	}

	return 0;
}
//if ed is null, fld points to a global. if str_is_static, then s doesn't need its own memory allocated.
void PDECL PR_SetStringField(pubprogfuncs_t *progfuncs, struct edict_s *ed, string_t *fld, const char *str, pbool str_is_static)
{
	*fld = PR_StringToProgs(progfuncs, str);
}

char *PDECL PR_RemoveProgsString				(pubprogfuncs_t *ppf, string_t str)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	char *ret;

	//input string is expected to be an allocated string
	//if its a temp, or a constant, just return NULL.
	if (((unsigned int)str & STRING_SPECMASK) == STRING_STATIC)
	{
		int i = str & ~STRING_SPECMASK;
		if (i >= prinst.numallocedstrings)
		{
			progfuncs->funcs.pr_trace = 1;
			return NULL;
		}
		if (prinst.allocedstrings[i])
		{
			ret = prinst.allocedstrings[i];
			prinst.allocedstrings[i] = NULL;	//remove it
			return ret;
		}
		else
		{
			progfuncs->funcs.pr_trace = 1;
			return NULL;	//urm, was freed...
		}
	}
	printf("invalid static string %x\n", str);
	progfuncs->funcs.pr_trace = 1;
	return NULL;
}

const char *ASMCALL PR_StringToNative				(pubprogfuncs_t *ppf, string_t str)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	if (((unsigned int)str & STRING_SPECMASK) == STRING_STATIC)
	{
		int i = str & ~STRING_SPECMASK;
		if (i >= prinst.numallocedstrings)
		{	
			if (!progfuncs->funcs.pr_trace)
			{
				printf("invalid string %x\n", str);
				progfuncs->funcs.pr_trace = 1;
				PR_StackTrace(&progfuncs->funcs, false);
			}
			return "";
		}
		if (prinst.allocedstrings[i])
			return prinst.allocedstrings[i];
		else
		{
			if (!progfuncs->funcs.pr_trace)
			{
				printf("invalid string %x\n", str);
				progfuncs->funcs.pr_trace = 1;
				PR_StackTrace(&progfuncs->funcs, false);
			}
			return "";	//urm, was freed...
		}
	}
	if (((unsigned int)str & STRING_SPECMASK) == STRING_TEMP)
	{
		int i = str & ~STRING_SPECMASK;
		if (i >= prinst.numtempstrings)
		{
			if (!progfuncs->funcs.pr_trace)
			{
				printf("invalid temp string %x\n", str);
				progfuncs->funcs.pr_trace = 1;
				PR_StackTrace(&progfuncs->funcs, false);
			}
			return "";
		}
		return prinst.tempstrings[i];
	}

	if ((unsigned int)str >= (unsigned int)prinst.addressableused)
	{
		if (!progfuncs->funcs.pr_trace)
		{
			printf("invalid string offset %x\n", str);
			progfuncs->funcs.pr_trace = 1;
			PR_StackTrace(&progfuncs->funcs, false);
		}
		return "";
	}
	return progfuncs->funcs.stringtable + str;
}


string_t PDECL PR_AllocTempString			(pubprogfuncs_t *ppf, const char *str)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	char **ntable;
	int newmax;
	int i;

	if (!str)
		return 0;

	if (prinst.numtempstrings == prinst.maxtempstrings)
	{
		newmax = prinst.maxtempstrings += 1024;
		prinst.maxtempstrings += 1024;
		ntable = progfuncs->funcs.parms->memalloc(sizeof(char*) * newmax);
		memcpy(ntable, prinst.tempstrings, sizeof(char*) * prinst.numtempstrings);
		prinst.maxtempstrings = newmax;
		if (prinst.tempstrings)
			progfuncs->funcs.parms->memfree(prinst.tempstrings);
		prinst.tempstrings = ntable;
	}

	i = prinst.numtempstrings;
	if (i == 0x10000000)
		return 0;

	prinst.numtempstrings++;

	prinst.tempstrings[i] = progfuncs->funcs.parms->memalloc(strlen(str)+1);
	strcpy(prinst.tempstrings[i], str);

	return (string_t)((unsigned int)i | STRING_TEMP);
}
string_t PDECL PR_AllocTempStringLen			(pubprogfuncs_t *ppf, char **str, unsigned int len)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	char **ntable;
	int newmax;
	int i;

	if (!str)
		return 0;

	if (prinst.numtempstrings == prinst.maxtempstrings)
	{
		newmax = prinst.maxtempstrings += 1024;
		prinst.maxtempstrings += 1024;
		ntable = progfuncs->funcs.parms->memalloc(sizeof(char*) * newmax);
		memcpy(ntable, prinst.tempstrings, sizeof(char*) * prinst.numtempstrings);
		prinst.maxtempstrings = newmax;
		if (prinst.tempstrings)
			progfuncs->funcs.parms->memfree(prinst.tempstrings);
		prinst.tempstrings = ntable;
	}

	i = prinst.numtempstrings;
	if (i == 0x10000000)
		return 0;

	prinst.numtempstrings++;

	prinst.tempstrings[i] = progfuncs->funcs.parms->memalloc(len);
	*str = prinst.tempstrings[i];

	return (string_t)((unsigned int)i | STRING_TEMP);
}

void PR_FreeTemps			(progfuncs_t *progfuncs, int depth)
{
	int i;
	if (depth > prinst.numtempstrings)
	{
		Sys_Error("QC Temp stack inverted\n");
		return;
	}
	for (i = depth; i < prinst.numtempstrings; i++)
	{
		externs->memfree(prinst.tempstrings[i]);
	}

	prinst.numtempstrings = depth;
}
pbool PDECL PR_DumpProfiles (pubprogfuncs_t *ppf)
{
	progfuncs_t *progfuncs = (progfuncs_t*)ppf;
	struct progstate_s *ps;
	unsigned int i, f, j, s;
	unsigned long long cpufrequency;
	struct
	{
		char *fname;
		int profile;
		unsigned long long profiletime;
	} *sorted, t;
	if (!prinst.profiling)
	{
		printf("Enabling profiling\n");
		prinst.profiling = true;
		return true;
	}

	cpufrequency = Sys_GetClockRate();

	for (i = 0; i < maxprogs; i++)
	{
		ps = &pr_progstate[i];
		if (ps->progs == NULL)	//we havn't loaded it yet, for some reason
			continue;	

		printf("%s:\n", ps->filename);
		sorted = malloc(sizeof(*sorted) * ps->progs->numfunctions);
		//pull out the functions in order to sort them
		for (s = 0, f = 0; f < ps->progs->numfunctions; f++)
		{
			if (!ps->functions[f].profile)
				continue;
			sorted[s].fname = ps->functions[f].s_name+progfuncs->funcs.stringtable;
			sorted[s].profile = ps->functions[f].profile;
			sorted[s].profiletime = ps->functions[f].profiletime - ps->functions[f].profilechildtime;
			ps->functions[f].profile = 0;
			ps->functions[f].profiletime = 0;
			s++;
		}

		// good 'ol bubble sort
		for (f = 0; f < s; f++)
			for (j = f; j < s; j++)
				if (sorted[f].profiletime > sorted[j].profiletime)
				{
					t = sorted[f];
					sorted[f] = sorted[j];
					sorted[j] = t;
				}

		//print it out
		for (f = 0; f < s; f++)
			printf("%s: %u %g\n", sorted[f].fname, sorted[f].profile, (float)(((double)sorted[f].profiletime) / cpufrequency));
		free(sorted);
	}
	return true;
}

static void PDECL PR_CloseProgs(pubprogfuncs_t *ppf);

static void PDECL RegisterBuiltin(pubprogfuncs_t *progfncs, char *name, builtin_t func);

pubprogfuncs_t deffuncs = {
	PROGSTRUCT_VERSION,
	PR_CloseProgs,
	PR_Configure,
	PR_LoadProgs,
	PR_InitEnts,
	PR_ExecuteProgram,
	PR_globals,
	PR_entvars,
	PR_RunError,
	ED_Print,
	ED_Alloc,
	ED_Free,

	QC_EDICT_NUM,
	QC_NUM_FOR_EDICT,


	SetGlobalEdict,

	PR_VarString,

	NULL,	//progstate
	PR_FindFunc,
#if defined(MINIMAL) || defined(OMIT_QCC)
	NULL,
	NULL,
#else
	Comp_Begin,
	Comp_Continue,
#endif

	filefromprogs,
	NULL,//filefromnewprogs,

	ED_Print,
	PR_SaveEnts,
	PR_LoadEnts,

	PR_SaveEnt,
	PR_RestoreEnt,

	PR_FindGlobal,
	ED_NewString,
	QC_HunkAlloc,

	QC_GetEdictFieldValue,
	ProgsToEdict,
	EdictToProgs,

	PR_EvaluateDebugString,

	0,//trace
	PR_StackTrace,

	PR_ToggleBreakpoint,
	0,	//numprogs
	NULL,	//parms
#if 1//defined(MINIMAL) || defined(OMIT_QCC)
	NULL,	//decompile
#else
	QC_Decompile,
#endif
	0,	//callargc
	RegisterBuiltin,

	0,	//string table(pointer base address)
	0,		//string table size
	0,	//max size
	0,	//field adjust(aditional field offset)

	PR_ForkStack,
	PR_ResumeThread,
	PR_AbortStack,

	0,	//called builtin number

	QC_RegisterFieldVar,

	NULL,	//user tempstringbase
	0,		//user tempstringnum

	PR_AllocTempString,

	PR_StringToProgs,
	PR_StringToNative,
	PR_QueryField,
	QC_ClearEdict,
	QC_FindPrefixedGlobals,
	PR_memalloc,
	PR_AllocTempStringLen,
	PR_memfree,
	PR_SetWatchPoint,

	QC_AddSharedVar,
	QC_AddSharedFieldVar,
	PR_RemoveProgsString,
	PR_GetFuncArgCount,
	PR_GenerateStatementString,
	ED_FieldInfo,
	PR_UglyValueString,
	ED_ParseEval,
	PR_SetStringField,
	PR_DumpProfiles
};
static int PDECL qclib_null_printf(const char *s, ...)
{
	return 0;
}
static void *PDECL qclib_malloc(int size)
{
	return malloc(size);
}
static void PDECL qclib_free(void *ptr)
{
	free(ptr);
}
#ifdef FTE_TARGET_WEB
#define printf NULL	//should be some null wrapper instead
#endif

//defs incase following structure is not passed.
struct edict_s *safesv_edicts;
int safesv_num_edicts;
double safetime=0;

progexterns_t defexterns = {
	PROGSTRUCT_VERSION,		

	NULL, //char *(*ReadFile) (char *fname, void *buffer, int len);
	NULL, //int (*FileSize) (char *fname);	//-1 if file does not exist
	NULL, //bool (*WriteFile) (char *name, void *data, int len);
	qclib_null_printf, //void (*printf) (char *, ...);
	(void*)exit, //void (*Sys_Error) (char *, ...);
	NULL, //void (*Abort) (char *, ...);
	NULL,

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

	qclib_malloc, //void *(*memalloc) (int size);	//small string allocation	malloced and freed randomly by the executor. (use memalloc if you want)
	qclib_free, //void (*memfree) (void * mem);

	NULL, //int (*useeditor) (char *filename, int line, int nump, char **parms);
	NULL,	//relocated

	NULL, //builtin_t *globalbuiltins;	//these are available to all progs
	0, //int numglobalbuiltins;

	PR_NOCOMPILE,

	&safetime, //double *gametime;

	&safesv_edicts, //struct edict_s **sv_edicts;
	&safesv_num_edicts, //int *sv_num_edicts;
	sizeof(edictrun_t), //int edictsize;	//size of edict_t
};

//progfuncs_t *progfuncs = NULL;
#undef memfree
#undef prinst
#undef extensionbuiltin
#undef field
#undef shares
#undef maxedicts
#undef sv_num_edicts

static void PDECL PR_CloseProgs(pubprogfuncs_t *ppf)
{
//	extensionbuiltin_t *eb;
	void (VARGS *f) (void *);
	progfuncs_t *inst = (progfuncs_t*)ppf;

	unsigned int i;
	edictrun_t *e;

	f = inst->funcs.parms->memfree;

	for ( i=1 ; i<inst->inst.maxedicts; i++)
	{
		e = (edictrun_t *)(inst->inst.edicttable[i]);
		inst->inst.edicttable[i] = NULL;
		if (e)
		{
//			e->entnum = i;
			f(e);
		}
	}

	PRHunkFree(inst, 0);

#if defined(_WIN32) && !defined(WINRT)
	VirtualFree(inst->inst.addressablehunk, 0, MEM_RELEASE);	//doesn't this look complicated? :p
#else
	free(inst->inst.addressablehunk);
#endif

	if (inst->inst.allocedstrings)
		f(inst->inst.allocedstrings);
	inst->inst.allocedstrings = NULL;
	if (inst->inst.tempstrings)
		f(inst->inst.tempstrings);
	inst->inst.tempstrings = NULL;

	free(inst->inst.watch_name);


/*
	while(inst->prinst.extensionbuiltin)
	{
		eb = inst->prinst.extensionbuiltin->prev;
		f(inst->prinst.extensionbuiltin);
		inst->prinst.extensionbuiltin = eb;
	}
*/
	if (inst->inst.field)
		f(inst->inst.field);
	if (inst->inst.shares)
		f(inst->inst.shares);	//free memory
	f(inst);
}

static void PDECL RegisterBuiltin(pubprogfuncs_t *progfuncs, char *name, builtin_t func)
{
/*
	extensionbuiltin_t *eb;
	eb = memalloc(sizeof(extensionbuiltin_t));
	eb->prev = progfuncs->prinst.extensionbuiltin;
	progfuncs->prinst.extensionbuiltin = eb;
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
pubprogfuncs_t * PDECL InitProgs(progexterns_t *ext)
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
#undef pr_progstate
#undef pr_argc
	funcs = ext->memalloc(sizeof(progfuncs_t));	
	memcpy(&funcs->funcs, &deffuncs, sizeof(pubprogfuncs_t));
	memset(&funcs->inst, 0, sizeof(funcs->inst));

	funcs->funcs.progstate = &funcs->inst.progstate;

	funcs->funcs.parms = ext;

	SetEndian();
	
	return &funcs->funcs;
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
