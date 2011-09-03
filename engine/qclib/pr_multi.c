#define PROGSUSED
#include "progsint.h"

#define HunkAlloc BADGDFG sdfhhsf FHS

void PR_SetBuiltins(int type);
/*
progstate_t *pr_progstate;
progsnum_t pr_typecurrent;
int maxprogs;

progstate_t *current_progstate;
int numshares;

sharedvar_t *shares;	//shared globals, not including parms
int maxshares;
*/

pbool PR_SwitchProgs(progfuncs_t *progfuncs, progsnum_t type)
{	
	if ((unsigned)type >= maxprogs)
		PR_RunError(progfuncs, "QCLIB: Bad prog type - %i", type);
//		Sys_Error("Bad prog type - %i", type);

	if (pr_progstate[(unsigned)type].progs == NULL)	//we havn't loaded it yet, for some reason
		return false;	

	current_progstate = &pr_progstate[(unsigned)type];

	pr_typecurrent = type;

	return true;
}

void PR_MoveParms(progfuncs_t *progfuncs, progsnum_t progs1, progsnum_t progs2)	//from 2 to 1
{
	unsigned int a;
	progstate_t *p1;
	progstate_t *p2;

	if (progs1 == progs2)
		return;	//don't bother coping variables to themselves...

	p1 = &pr_progstate[(int)progs1];
	p2 = &pr_progstate[(int)progs2];

	if ((unsigned)progs1 >= maxprogs || !p1->globals)
		PR_RunError(progfuncs, "QCLIB: Bad prog type - %i", progs1);
	if ((unsigned)progs2 >= maxprogs || !p2->globals)
		PR_RunError(progfuncs, "QCLIB: Bad prog type - %i", progs2);

	//copy parms.
	for (a = 0; a < MAX_PARMS;a++)
	{
		*(int *)&p1->globals[OFS_PARM0+3*a  ] = *(int *)&p2->globals[OFS_PARM0+3*a  ];
		*(int *)&p1->globals[OFS_PARM0+3*a+1] = *(int *)&p2->globals[OFS_PARM0+3*a+1];
		*(int *)&p1->globals[OFS_PARM0+3*a+2] = *(int *)&p2->globals[OFS_PARM0+3*a+2];
	}
	p1->globals[OFS_RETURN] = p2->globals[OFS_RETURN];
	p1->globals[OFS_RETURN+1] = p2->globals[OFS_RETURN+1];
	p1->globals[OFS_RETURN+2] = p2->globals[OFS_RETURN+2];

	//move the vars defined as shared.
	for (a = 0; a < numshares; a++)//fixme: make offset per progs
	{
		memmove(&((int *)p1->globals)[shares[a].varofs], &((int *)p2->globals)[shares[a].varofs], shares[a].size*4);
/*		((int *)p1->globals)[shares[a].varofs] = ((int *)p2->globals)[shares[a].varofs];
		if (shares[a].size > 1)
		{
			((int *)p1->globals)[shares[a].varofs+1] = ((int *)p2->globals)[shares[a].varofs+1];
			if (shares[a].size > 2)
				((int *)p1->globals)[shares[a].varofs+2] = ((int *)p2->globals)[shares[a].varofs+2];
		}
*/
	}
}

progsnum_t PR_LoadProgs(progfuncs_t *progfuncs, char *s, int headercrc, builtin_t *builtins, int numbuiltins)
{
	unsigned int a;
	progsnum_t oldtype;
	oldtype = pr_typecurrent;	
	for (a = 0; a < maxprogs; a++)
	{
		if (pr_progstate[a].progs == NULL)
		{
			pr_typecurrent = a;
			current_progstate = &pr_progstate[a];
			if (PR_ReallyLoadProgs(progfuncs, s, headercrc, &pr_progstate[a], false))	//try and load it			
			{
				current_progstate->builtins = builtins;
				current_progstate->numbuiltins = numbuiltins;
				if (a <= progfuncs->numprogs)
					progfuncs->numprogs = a+1;

#ifdef QCJIT
				current_progstate->jit = PR_GenerateJit(progfuncs);
#endif
				if (oldtype>=0)
					PR_SwitchProgs(progfuncs, oldtype);
				return a;	//we could load it. Yay!
			}
			if (oldtype!=-1)
				PR_SwitchProgs(progfuncs, oldtype);
			return -1; // loading failed.
		}
	}
	PR_SwitchProgs(progfuncs, oldtype);
	return -1;
}

void PR_ShiftParms(progfuncs_t *progfuncs, int amount)
{
	int a;
	for (a = 0; a < MAX_PARMS - amount;a++)
		*(int *)&pr_globals[OFS_PARM0+3*a] = *(int *)&pr_globals[OFS_PARM0+3*(amount+a)];
}

//forget a progs
void PR_Clear(progfuncs_t *progfuncs)
{
	unsigned int a;
	for (a = 0; a < maxprogs; a++)
	{
#ifdef QCJIT
		if (pr_progstate[a].jit)
			PR_CloseJit(pr_progstate[a].jit);
#endif
		pr_progstate[a].progs = NULL;
	}
}



void QC_StartShares(progfuncs_t *progfuncs)
{
	numshares = 0;
	maxshares = 32;
	if (shares)
		memfree(shares);
	shares = memalloc(sizeof(sharedvar_t)*maxshares);
}
void QC_AddSharedVar(progfuncs_t *progfuncs, int start, int size)	//fixme: make offset per progs and optional
{
	int ofs;
	unsigned int a;

	if (numshares >= maxshares)
	{
		void *buf;
		buf = shares;
		maxshares += 16;		
		shares = memalloc(sizeof(sharedvar_t)*maxshares);

		memcpy(shares, buf, sizeof(sharedvar_t)*numshares);

		memfree(buf);
	}
	ofs = start;
	for (a = 0; a < numshares; a++)
	{
		if (shares[a].varofs+shares[a].size == ofs)
		{
			shares[a].size += size;	//expand size.
			return;
		}
		if (shares[a].varofs == start)
			return;
	}


	shares[numshares].varofs = start;
	shares[numshares].size = size;
	numshares++;
}


//void ShowWatch(void);

void QC_InitShares(progfuncs_t *progfuncs)
{
//	ShowWatch();
	if (!field)	//don't make it so we will just need to remalloc everything
	{
		maxfields = 64;
		field = memalloc(sizeof(fdef_t) * maxfields);
	}

	numfields = 0;
	progfuncs->fieldadjust = 0;
}

void QC_FlushProgsOffsets(progfuncs_t *progfuncs)
{	//sets the fields up for loading a new progs.
	//fields are matched by name to other progs
	//not by offset
	unsigned int i;
	for (i = 0; i < numfields; i++)
		field[i].progsofs = -1;
}


//called if a global is defined as a field
//returns offset.

//vectors must be added before any of their corresponding _x/y/z vars
//in this way, even screwed up progs work.

//requestedpos is the offset the engine WILL put it at.
//origionaloffs is used to track matching field offsets. fields with the same progs offset overlap

//note: we probably suffer from progs with renamed system globals.
int QC_RegisterFieldVar(progfuncs_t *progfuncs, unsigned int type, char *name, signed long engineofs, signed long progsofs)
{
//	progstate_t *p;
//	int pnum;
	unsigned int i;
	int namelen;
	int ofs;

	int fnum;

	if (!name)	//engine can use this to offset all progs fields
	{			//which fixes constant field offsets (some ktpro arrays)
		progfuncs->fieldadjust = fields_size/4;
		return 0;
	}


	prinst->reorganisefields = true;

	//look for an existing match
	for (i = 0; i < numfields; i++)
	{		
		if (!strcmp(name, field[i].name))
		{
			if (field[i].type != type)
			{
				printf("Field type mismatch on \"%s\". %i != %i\n", name, field[i].type, type);
				continue;
			}
			if (!progfuncs->fieldadjust && engineofs>=0)
				if ((unsigned)engineofs/4 != field[i].ofs)
					Sys_Error("Field %s at wrong offset", name);

			if (field[i].progsofs == -1)
				field[i].progsofs = progsofs;
//			printf("Dupfield %s %i -> %i\n", name, field[i].progsofs,field[i].ofs);
			return field[i].ofs-progfuncs->fieldadjust;	//got a match
		}
	}

	if (numfields+1>maxfields)
	{
		fdef_t *nf;
		i = maxfields;
		maxfields += 32;
		nf = memalloc(sizeof(fdef_t) * maxfields);
		memcpy(nf, field, sizeof(fdef_t) * i);
		memfree(field);
		field = nf;
	}

	//try to add a new one
	fnum = numfields;
	numfields++;
	field[fnum].name = name;	
	if (type == ev_vector)
	{
		char *n;		
		namelen = strlen(name)+5;	

		n=PRHunkAlloc(progfuncs, namelen);
		sprintf(n, "%s_x", name);
		ofs = QC_RegisterFieldVar(progfuncs, ev_float, n, engineofs, progsofs);
		field[fnum].ofs = ofs+progfuncs->fieldadjust;

		n=PRHunkAlloc(progfuncs, namelen);
		sprintf(n, "%s_y", name);
		QC_RegisterFieldVar(progfuncs, ev_float, n, (engineofs==-1)?-1:(engineofs+4), (progsofs==-1)?-1:progsofs+1);

		n=PRHunkAlloc(progfuncs, namelen);
		sprintf(n, "%s_z", name);
		QC_RegisterFieldVar(progfuncs, ev_float, n, (engineofs==-1)?-1:(engineofs+8), (progsofs==-1)?-1:progsofs+2);
	}
	else if (engineofs >= 0)
	{	//the engine is setting up a list of required field indexes.

		//paranoid checking of the offset.
	/*	for (i = 0; i < numfields-1; i++)
		{
			if (field[i].ofs == ((unsigned)engineofs)/4)
			{
				if (type == ev_float && field[i].type == ev_vector)	//check names
				{
					if (strncmp(field[i].name, name, strlen(field[i].name)))
						Sys_Error("Duplicated offset");
				}
				else
					Sys_Error("Duplicated offset");
			}
		}*/
		if (engineofs&3)
			Sys_Error("field %s is %i&3", name, (int)engineofs);
		field[fnum].ofs = ofs = engineofs/4;
	}
	else
	{	//we just found a new fieldname inside a progs
		field[fnum].ofs = ofs = fields_size/4;	//add on the end

		//if the progs field offset matches annother offset in the same progs, make it match up with the earlier one.
		if (progsofs>=0)
		{
			for (i = 0; i < numfields-1; i++)
			{
				if (field[i].progsofs == (unsigned)progsofs)
				{
//					printf("found union field %s %i -> %i\n", field[i].name, field[i].progsofs, field[i].ofs);
					field[fnum].ofs = ofs = field[i].ofs;
					break;
				}
			}
		}
	}
//	if (type != ev_vector)
		if (fields_size < (ofs+type_size[type])*4)
			fields_size = (ofs+type_size[type])*4;

	if (max_fields_size && fields_size > max_fields_size)
		Sys_Error("Allocated too many additional fields after ents were inited.");
	field[fnum].type = type;

	field[fnum].progsofs = progsofs;

//	printf("Field %s %i -> %i\n", name, field[fnum].progsofs,field[fnum].ofs);
	
	//we've finished setting the structure	
	return ofs - progfuncs->fieldadjust;
}


//called if a global is defined as a field
void QC_AddSharedFieldVar(progfuncs_t *progfuncs, int num, char *stringtable)
{
//	progstate_t *p;
//	int pnum;
	unsigned int i, o;

	char *s;

	//look for an existing match not needed, cos we look a little later too.
	/*
	for (i = 0; i < numfields; i++)
	{		
		if (!strcmp(pr_globaldefs[num].s_name, field[i].s_name))
		{
			//really we should look for a field def

			*(int *)&pr_globals[pr_globaldefs[num].ofs] = field[i].ofs;	//got a match

			return;
		}
	}
	*/
	
	switch(current_progstate->structtype)
	{
	case PST_KKQWSV:
	case PST_DEFAULT:
		for (i=1 ; i<pr_progs->numfielddefs; i++)
		{
			if (!strcmp(pr_fielddefs16[i].s_name+stringtable, pr_globaldefs16[num].s_name+stringtable))
			{
//				int old = *(int *)&pr_globals[pr_globaldefs16[num].ofs];
				*(int *)&pr_globals[pr_globaldefs16[num].ofs] = QC_RegisterFieldVar(progfuncs, pr_fielddefs16[i].type, pr_globaldefs16[num].s_name+stringtable, -1, *(int *)&pr_globals[pr_globaldefs16[num].ofs]);

//				printf("Field %s %i -> %i\n", pr_globaldefs16[num].s_name+stringtable, old, *(int *)&pr_globals[pr_globaldefs16[num].ofs]);
				return;
			}
		}

		s = pr_globaldefs16[num].s_name+stringtable;

		for (i = 0; i < numfields; i++)
		{
			o = field[i].progsofs;
			if (o == *(unsigned int *)&pr_globals[pr_globaldefs16[num].ofs])
			{
//				int old = *(int *)&pr_globals[pr_globaldefs16[num].ofs];
				*(int *)&pr_globals[pr_globaldefs16[num].ofs] = field[i].ofs-progfuncs->fieldadjust;
//				printf("Field %s %i -> %i\n", pr_globaldefs16[num].s_name+stringtable, old, *(int *)&pr_globals[pr_globaldefs16[num].ofs]);
				return;
			}
		}

		//oh well, must be a parameter.
//		if (*(int *)&pr_globals[pr_globaldefs16[num].ofs])
//			Sys_Error("QCLIB: Global field var with no matching field \"%s\", from offset %i", pr_globaldefs16[num].s_name+stringtable, *(int *)&pr_globals[pr_globaldefs16[num].ofs]);
		return;
	case PST_FTE32:
	case PST_QTEST:
		for (i=1 ; i<pr_progs->numfielddefs; i++)
		{
			if (!strcmp(pr_fielddefs32[i].s_name+stringtable, pr_globaldefs32[num].s_name+stringtable))
			{
				*(int *)&pr_globals[pr_globaldefs32[num].ofs] = QC_RegisterFieldVar(progfuncs, pr_fielddefs32[i].type, pr_globaldefs32[num].s_name+stringtable, -1, *(int *)&pr_globals[pr_globaldefs32[num].ofs]);
				return;
			}
		}

		s = pr_globaldefs32[num].s_name+stringtable;

		for (i = 0; i < numfields; i++)
		{
			o = field[i].progsofs;
			if (o == *(unsigned int *)&pr_globals[pr_globaldefs32[num].ofs])
			{
				*(int *)&pr_globals[pr_globaldefs32[num].ofs] = field[i].ofs-progfuncs->fieldadjust;
				return;
			}
		}

		//oh well, must be a parameter.
		if (*(int *)&pr_globals[pr_globaldefs32[num].ofs])
			Sys_Error("QCLIB: Global field var with no matching field \"%s\", from offset %i", pr_globaldefs32[num].s_name+stringtable, *(int *)&pr_globals[pr_globaldefs32[num].ofs]);
		return;
	default:
		Sys_Error("Bad bits");
		break;
	}
	Sys_Error("Should be unreachable");	
}

