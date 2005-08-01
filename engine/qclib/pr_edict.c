

#define PROGSUSED
struct edict_s;
#include "progsint.h"
//#include "crc.h"

/*int maxedicts;

evalc_t spawnflagscache;
*/

#ifdef _WIN32
//this is windows  all files are written with this endian standard. we do this to try to get a little more speed.
#define NOENDIAN
#endif


vec3_t vec3_origin;

//edictrun_t *sv_edicts;
//int sv_num_edicts;

//int				pr_edict_size;	// in bytes
//int				pr_max_edict_size;

//unsigned short		pr_crc;
const int		type_size[12] = {1,	//void
						sizeof(string_t)/4,	//string
						1,	//float
						3,	//vector
						1,	//entity
						1,	//field
						sizeof(func_t)/4,//function
						sizeof(void *)/4,//pointer
						1,	//integer
						1,	//fixme: how big should a variant be?
						0,	//ev_struct. variable sized.
						0	//ev_union. variable sized.
						};

fdef_t *ED_FieldAtOfs (progfuncs_t *progfuncs, unsigned int ofs);
pbool	ED_ParseEpair (progfuncs_t *progfuncs, void *base, ddefXX_t *key, char *s, int bits);



/*
#define	MAX_FIELD_LEN	64
#define GEFV_CACHESIZE	5

typedef struct {
	ddef_t	*pcache;
	char	field[MAX_FIELD_LEN];
} gefv_cache;

static gefv_cache	gefvCache[GEFV_CACHESIZE] = {{NULL, ""}, {NULL, ""}};
*/

/*
=================
ED_ClearEdict

Sets everything to NULL
=================
*/
void ED_ClearEdict (progfuncs_t *progfuncs, edictrun_t *e)
{
	int num = e->entnum;
	memset (e->fields, 0, fields_size);
	e->isfree = false;
	e->entnum = num;
}

/*
=================
ED_Alloc

Either finds a free edict, or allocates a new one.
Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
struct edict_s *ED_Alloc (progfuncs_t *progfuncs)
{
	int			i;
	edictrun_t		*e;

	for ( i=0 ; i<sv_num_edicts ; i++)
	{
		e = (edictrun_t*)EDICT_NUM(progfuncs, i);
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if (!e || (e->isfree && ( e->freetime < 2 || *externs->gametime - e->freetime > 0.5 ) ))
		{
			if (!e)
			{
				prinst->edicttable[i] = *(struct edict_s **)&e = (void*)memalloc(externs->edictsize);
				e->fields = PRAddressableAlloc(progfuncs, fields_size);
				e->entnum = i;
			}

			ED_ClearEdict (progfuncs, e);

			if (externs->entspawn)
				externs->entspawn((struct edict_s *) e);
			return (struct edict_s *)e;
		}
	}
	
	if (i >= maxedicts-1)	//try again, but use timed out ents.
	{
		for ( i=0 ; i<sv_num_edicts ; i++)
		{
			e = (edictrun_t*)EDICT_NUM(progfuncs, i);
			// the first couple seconds of server time can involve a lot of
			// freeing and allocating, so relax the replacement policy
			if (!e || (e->isfree))
			{
				if (!e)
				{
					prinst->edicttable[i] = *(struct edict_s **)&e = (void*)memalloc(externs->edictsize);
					e->fields = PRAddressableAlloc(progfuncs, fields_size);
					e->entnum = i;
				}

				ED_ClearEdict (progfuncs, e);

				if (externs->entspawn)
					externs->entspawn((struct edict_s *) e);
				return (struct edict_s *)e;
			}
		}

		if (i >= maxedicts-1)
		{
			int size;
			char *buf;
			buf = progfuncs->save_ents(progfuncs, NULL, &size, 0);
			progfuncs->parms->WriteFile("edalloc.dump", buf, size);
			Sys_Error ("ED_Alloc: no free edicts");
		}
	}
		
	sv_num_edicts++;
	e = (edictrun_t*)EDICT_NUM(progfuncs, i);

	if (!e)
	{
		prinst->edicttable[i] = *(struct edict_s **)&e = (void*)memalloc(externs->edictsize);
		e->fields = PRAddressableAlloc(progfuncs, fields_size);
		e->entnum = i;
	}

	ED_ClearEdict (progfuncs, e);

	if (externs->entspawn)
		externs->entspawn((struct edict_s *) e);
	return (struct edict_s *)e;
}

/*
=================
ED_Free

Marks the edict as free
FIXME: walk all entities and NULL out references to this entity
=================
*/
void ED_Free (progfuncs_t *progfuncs, struct edict_s *ed)
{
	edictrun_t *e = (edictrun_t *)ed;
//	SV_UnlinkEdict (ed);		// unlink from world bsp

	if (e->isfree)	//this happens on start.bsp where an onlyregistered trigger killtargets itself (when all of this sort die after 1 trigger anyway).
	{
		if (pr_depth)
			printf("Tried to free free entity within %s\n", pr_xfunction->s_name+progfuncs->stringtable);
		else
			printf("Engine tried to free free entity\n");
//		if (developer.value == 1)
//			pr_trace = true;
		return;
	}

	if (externs->entcanfree)
		if (!externs->entcanfree(ed))	//can stop an ent from being freed.
			return;

	e->isfree = true;
	e->freetime = (float)*externs->gametime;

/*
	ed->v.model = 0;
	ed->v.takedamage = 0;
	ed->v.modelindex = 0;
	ed->v.colormap = 0;
	ed->v.skin = 0;
	ed->v.frame = 0;
	VectorCopy (vec3_origin, ed->v.origin);
	VectorCopy (vec3_origin, ed->v.angles);
	ed->v.nextthink = -1;
	ed->v.solid = 0;
*/		
}

//===========================================================================

/*
============
ED_GlobalAtOfs
============
*/
ddef16_t *ED_GlobalAtOfs16 (progfuncs_t *progfuncs, int ofs)
{
	ddef16_t		*def;
	unsigned int			i;
	
	for (i=0 ; i<pr_progs->numglobaldefs ; i++)
	{
		def = &pr_globaldefs16[i];
		if (def->ofs == ofs)
			return def;
	}
	return NULL;
}
ddef32_t *ED_GlobalAtOfs32 (progfuncs_t *progfuncs, unsigned int ofs)
{
	ddef32_t		*def;
	unsigned int			i;
	
	for (i=0 ; i<pr_progs->numglobaldefs ; i++)
	{
		def = &pr_globaldefs32[i];
		if (def->ofs == ofs)
			return def;
	}
	return NULL;
}

/*
============
ED_FieldAtOfs
============
*/
fdef_t *ED_FieldAtOfs (progfuncs_t *progfuncs, unsigned int ofs)
{
//	ddef_t		*def;
	unsigned int			i;
	
	for (i=0 ; i<numfields ; i++)
	{
		if (field[i].ofs == ofs)
			return &field[i];
	}
	return NULL;
}
/*
============
ED_FindField
============
*/
fdef_t *ED_FindField (progfuncs_t *progfuncs, char *name)
{
	unsigned int			i;
	
	for (i=0 ; i<numfields ; i++)
	{		
		if (!strcmp(field[i].name, name) )
			return &field[i];
	}
	return NULL;
}


/*
============
ED_FindGlobal
============
*/
ddef16_t *ED_FindGlobal16 (progfuncs_t *progfuncs, char *name)
{
	ddef16_t		*def;
	unsigned int			i;
	
	for (i=1 ; i<pr_progs->numglobaldefs ; i++)
	{
		def = &pr_globaldefs16[i];
		if (!strcmp(def->s_name+progfuncs->stringtable,name) )
			return def;
	}
	return NULL;
}
ddef32_t *ED_FindGlobal32 (progfuncs_t *progfuncs, char *name)
{
	ddef32_t		*def;
	unsigned int			i;
	
	for (i=1 ; i<pr_progs->numglobaldefs ; i++)
	{
		def = &pr_globaldefs32[i];
		if (!strcmp(def->s_name+progfuncs->stringtable,name) )
			return def;
	}
	return NULL;
}

unsigned int ED_FindGlobalOfs (progfuncs_t *progfuncs, char *name)
{
	ddef16_t *d16;
	ddef32_t *d32;
	switch(current_progstate->intsize)
	{
	case 24:
	case 16:
		d16 = ED_FindGlobal16(progfuncs, name);
		return d16?d16->ofs:0;
	case 32:
		d32 = ED_FindGlobal32(progfuncs, name);
		return d32?d32->ofs:0;
	}
	Sys_Error("ED_FindGlobalOfs - bad intsize");
	return 0;
}

ddef16_t *ED_FindGlobalFromProgs16 (progfuncs_t *progfuncs, char *name, progsnum_t prnum)
{
	ddef16_t		*def;
	unsigned int			i;
	
	for (i=1 ; i<pr_progstate[prnum].progs->numglobaldefs ; i++)
	{
		def = &pr_progstate[prnum].globaldefs16[i];
		if (!strcmp(def->s_name+progfuncs->stringtable,name) )
			return def;
	}
	return NULL;
}
ddef32_t *ED_FindGlobalFromProgs32 (progfuncs_t *progfuncs, char *name, progsnum_t prnum)
{
	ddef32_t		*def;
	unsigned int			i;
	
	for (i=1 ; i<pr_progstate[prnum].progs->numglobaldefs ; i++)
	{
		def = &pr_progstate[prnum].globaldefs32[i];
		if (!strcmp(def->s_name+progfuncs->stringtable,name) )
			return def;
	}
	return NULL;
}

ddef16_t *ED_FindTypeGlobalFromProgs16 (progfuncs_t *progfuncs, char *name, progsnum_t prnum, int type)
{
	ddef16_t		*def;
	unsigned int			i;
	
	for (i=1 ; i<pr_progstate[prnum].progs->numglobaldefs ; i++)
	{
		def = &pr_progstate[prnum].globaldefs16[i];
		if (!strcmp(def->s_name+progfuncs->stringtable,name) )
		{
			if (pr_progstate[prnum].types)
			{
				if (pr_progstate[prnum].types[def->type&~DEF_SAVEGLOBAL].type != type)
					continue;
			}
			else if ((def->type&(~DEF_SAVEGLOBAL)) != type)
				continue;
			return def;
		}
	}
	return NULL;
}


ddef32_t *ED_FindTypeGlobalFromProgs32 (progfuncs_t *progfuncs, char *name, progsnum_t prnum, int type)
{
	ddef32_t		*def;
	unsigned int			i;
	
	for (i=1 ; i<pr_progstate[prnum].progs->numglobaldefs ; i++)
	{
		def = &pr_progstate[prnum].globaldefs32[i];
		if (!strcmp(def->s_name+progfuncs->stringtable,name) )
		{
			if (pr_progstate[prnum].types)
			{
				if (pr_progstate[prnum].types[def->type&~DEF_SAVEGLOBAL].type != type)
					continue;
			}
			else if ((def->type&(~DEF_SAVEGLOBAL)) != (unsigned)type)
				continue;
			return def;
		}
	}
	return NULL;
}

unsigned int *ED_FindGlobalOfsFromProgs (progfuncs_t *progfuncs, char *name, progsnum_t prnum, int type)
{
	ddef16_t		*def16;
	ddef32_t		*def32;
	static unsigned int pos;
	switch(pr_progstate[prnum].intsize)
	{
	case 16:
	case 24:
		def16 = ED_FindTypeGlobalFromProgs16(progfuncs, name, prnum, type);
		if (!def16)
			return NULL;
		pos = def16->ofs;
		return &pos;
	case 32:
		def32 = ED_FindTypeGlobalFromProgs32(progfuncs, name, prnum, type);
		if (!def32)
			return NULL;
		return &def32->ofs;		
	}
	Sys_Error("ED_FindGlobalOfsFromProgs - bad intsize");
	return 0;
}

/*
============
ED_FindFunction
============
*/
dfunction_t *ED_FindFunction (progfuncs_t *progfuncs, char *name, progsnum_t *prnum, int fromprogs)
{
	dfunction_t		*func;
	unsigned int				i;
	char *sep;

	int pnum;

	if (prnum)
	{
		sep = strchr(name, ':');
		if (sep)
		{
			pnum = atoi(name);
			name = sep+1;
		}
		else
		{
			if (fromprogs>=0)
				pnum = fromprogs;
			else
				pnum = pr_typecurrent;
		}
		*prnum = pnum;
	}
	else
		pnum = pr_typecurrent;

	if (!pr_progstate[pnum].progs)
		return NULL;
	
	for (i=1 ; i<pr_progstate[pnum].progs->numfunctions ; i++)
	{
		func = &pr_progstate[pnum].functions[i];
		if (!strcmp(func->s_name+progfuncs->stringtable,name) )
			return func;
	}
	return NULL;
}

/*
============
PR_ValueString

Returns a string describing *data in a type specific manner
=============
*/
char *PR_ValueString (progfuncs_t *progfuncs, etype_t type, eval_t *val)
{
	static char	line[256];
	fdef_t			*fielddef;
	dfunction_t	*f;
	
#ifdef DEF_SAVEGLOBAL
	type &= ~DEF_SAVEGLOBAL;
#endif

	if (pr_types)
		type = pr_types[type].type;

	switch (type)
	{
	case ev_struct:
		sprintf (line, "struct");
		break;
	case ev_union:
		sprintf (line, "union");
		break;
	case ev_string:
		sprintf (line, "%s", val->string+progfuncs->stringtable);
		break;
	case ev_entity:	
		sprintf (line, "entity %i", NUM_FOR_EDICT(progfuncs, (struct edict_s *)PROG_TO_EDICT(progfuncs, val->edict)) );
		break;
	case ev_function:
		if (!val->function)
			sprintf (line, "NULL function");
		else
		{
			if ((val->function & 0xff000000)>>24 >= (unsigned)maxprogs || !pr_progstate[(val->function & 0xff000000)>>24].functions)
				sprintf (line, "Bad function");
			else
			{
				f = pr_progstate[(val->function & 0xff000000)>>24].functions + (val->function & ~0xff000000);
				sprintf (line, "%i:%s()", (val->function & 0xff000000)>>24, f->s_name+progfuncs->stringtable);
			}
		}
		break;
	case ev_field:
		fielddef = ED_FieldAtOfs (progfuncs,  val->_int );
		sprintf (line, ".%s (%i)", fielddef->name, val->_int);
		break;
	case ev_void:
		sprintf (line, "void type");
		break;
	case ev_float:
		sprintf (line, "%5.1f", val->_float);
		break;
	case ev_integer:
		sprintf (line, "%i", val->_int);
		break;
	case ev_vector:
		sprintf (line, "'%5.1f %5.1f %5.1f'", val->vector[0], val->vector[1], val->vector[2]);
		break;
	case ev_pointer:
		sprintf (line, "pointer");
		{
//			int entnum;
//			int valofs;
			if (val->_int == 0)
			{
				sprintf (line, "NULL pointer");
				break;
			}
		//FIXME: :/
			sprintf(line, "UNKNOWN");
//			entnum = ((qbyte *)val->edict - (qbyte *)sv_edicts) / pr_edict_size;
//			valofs = (int *)val->edict - (int *)edvars(EDICT_NUM(progfuncs, entnum));
//			fielddef = ED_FieldAtOfs (progfuncs, valofs );
//			if (!fielddef)
//				sprintf(line, "ent%i.%s", entnum, "UNKNOWN");
//			else
//				sprintf(line, "ent%i.%s", entnum, fielddef->s_name);
		}
		break;
	default:
		sprintf (line, "bad type %i", type);
		break;
	}
	
	return line;
}

/*
============
PR_UglyValueString

Returns a string describing *data in a type specific manner
Easier to parse than PR_ValueString
=============
*/
char *PR_UglyValueString (progfuncs_t *progfuncs, etype_t type, eval_t *val)
{
	static char	line[256];
	fdef_t		*fielddef;	
	dfunction_t	*f;
	int i, j;
	
#ifdef DEF_SAVEGLOBAL
	type &= ~DEF_SAVEGLOBAL;
#endif

	if (pr_types)
		type = pr_types[type].type;

	switch (type)
	{
	case ev_struct:
		sprintf (line, "structures cannot yet be saved");
		break;		
	case ev_union:
		sprintf (line, "unions cannot yet be saved");
		break;
	case ev_string:
		if ((unsigned)val->_int > (unsigned)addressableused)
			_snprintf (line, sizeof(line), "CORRUPT STRING");
		else
			_snprintf (line, sizeof(line), "%s", val->string+progfuncs->stringtable);
		break;
	case ev_entity:	
		sprintf (line, "%i", val->_int);
		break;
	case ev_function:
		i = (val->function & 0xff000000)>>24;	//progs number
		if (i > maxprogs || !pr_progstate[i].progs)
			sprintf (line, "BAD FUNCTION INDEX: %i", val->function);
		else
		{
			j = (val->function & ~0xff000000);	//function number
			if ((unsigned)j > pr_progstate[i].progs->numfunctions)
				sprintf(line, "%i:%s", i, "CORRUPT FUNCTION POINTER");
			else
			{
				f = pr_progstate[i].functions + j;
				sprintf (line, "%i:%s", i, f->s_name+progfuncs->stringtable);
			}
		}
		break;
	case ev_field:
		fielddef = ED_FieldAtOfs (progfuncs, val->_int );
		sprintf (line, "%s", fielddef->name);
		break;
	case ev_void:
		sprintf (line, "void");
		break;
	case ev_float:
		if (val->_float == (int)val->_float)
			sprintf (line, "%i", (int)val->_float);	//an attempt to cut down on the number of .000000 vars..
		else
			sprintf (line, "%f", val->_float);
		break;
	case ev_integer:
		sprintf (line, "%i", val->_int);
		break;
	case ev_vector:
		if (val->vector[0] == (int)val->vector[0] && val->vector[1] == (int)val->vector[1] && val->vector[2] == (int)val->vector[2])
			sprintf (line, "%i %i %i", (int)val->vector[0], (int)val->vector[1], (int)val->vector[2]);
		else
			sprintf (line, "%f %f %f", val->vector[0], val->vector[1], val->vector[2]);		
		break;
	default:
		sprintf (line, "bad type %i", type);
		break;
	}
	
	return line;
}

//compatable with Q1 (for savegames)
char *PR_UglyOldValueString (progfuncs_t *progfuncs, etype_t type, eval_t *val)
{
	static char	line[256];
	fdef_t		*fielddef;	
	dfunction_t	*f;
	
#ifdef DEF_SAVEGLOBAL
	type &= ~DEF_SAVEGLOBAL;
#endif

	if (pr_types)
		type = pr_types[type].type;

	switch (type)
	{
	case ev_struct:
		sprintf (line, "structures cannot yet be saved");
		break;		
	case ev_union:
		sprintf (line, "unions cannot yet be saved");
		break;
	case ev_string:
		sprintf (line, "%s", val->string+progfuncs->stringtable);
		break;
	case ev_entity:	
		sprintf (line, "%i", NUM_FOR_EDICT(progfuncs, (struct edict_s *)PROG_TO_EDICT(progfuncs, val->edict)));
		break;
	case ev_function:
		f = pr_progstate[(val->function & 0xff000000)>>24].functions + (val->function & ~0xff000000);
		sprintf (line, "%s", f->s_name+progfuncs->stringtable);
		break;
	case ev_field:
		fielddef = ED_FieldAtOfs (progfuncs, val->_int );
		sprintf (line, "%s", fielddef->name);
		break;
	case ev_void:
		sprintf (line, "void");
		break;
	case ev_float:
		if (val->_float == (int)val->_float)
			sprintf (line, "%i", (int)val->_float);	//an attempt to cut down on the number of .000000 vars..
		else
			sprintf (line, "%f", val->_float);
		break;
	case ev_integer:
		sprintf (line, "%i", val->_int);
		break;
	case ev_vector:
		if (val->vector[0] == (int)val->vector[0] && val->vector[1] == (int)val->vector[1] && val->vector[2] == (int)val->vector[2])
			sprintf (line, "%i %i %i", (int)val->vector[0], (int)val->vector[1], (int)val->vector[2]);
		else
			sprintf (line, "%f %f %f", val->vector[0], val->vector[1], val->vector[2]);
		break;		
		break;
	default:
		sprintf (line, "bad type %i", type);
		break;
	}
	
	return line;
}

char *PR_TypeString(progfuncs_t *progfuncs, etype_t type)
{
#ifdef DEF_SAVEGLOBAL
	type &= ~DEF_SAVEGLOBAL;
#endif

	if (pr_types)
		type = pr_types[type].type;

	switch (type)
	{
	case ev_struct:
		return "struct";		
	case ev_union:
		return "union";		
	case ev_string:
		return "string";
	case ev_entity:	
		return "entity";
	case ev_function:
		return "function";
	case ev_field:
		return "field";
	case ev_void:
		return "void";
	case ev_float:
		return "float";
	case ev_vector:
		return "vector";
	case ev_integer:
		return "integer";
	default:
		return "BAD TYPE";
	}	
}

/*
============
PR_GlobalString

Returns a string with a description and the contents of a global,
padded to 20 field width
============
*/
char *PR_GlobalString (progfuncs_t *progfuncs, int ofs)
{
	char	*s;
	int		i;
	ddef16_t	*def16;
	ddef32_t	*def32;
	void	*val;
	static char	line[128];

	switch (current_progstate->intsize)
	{
	case 16:
	case 24:
		val = (void *)&pr_globals[ofs];
		def16 = ED_GlobalAtOfs16(progfuncs, ofs);
		if (!def16)
			sprintf (line,"%i(?""?""?)", ofs);
		else
		{
			s = PR_ValueString (progfuncs, def16->type, val);
			sprintf (line,"%i(%s)%s", ofs, def16->s_name+progfuncs->stringtable, s);
		}
		
		i = strlen(line);
		for ( ; i<20 ; i++)
			strcat (line," ");
		strcat (line," ");
		return line;
	case 32:
		val = (void *)&pr_globals[ofs];
		def32 = ED_GlobalAtOfs32(progfuncs, ofs);
		if (!def32)
			sprintf (line,"%i(?""?""?)", ofs);
		else
		{
			s = PR_ValueString (progfuncs, def32->type, val);
			sprintf (line,"%i(%s)%s", ofs, def32->s_name+progfuncs->stringtable, s);
		}
		
		i = strlen(line);
		for ( ; i<20 ; i++)
			strcat (line," ");
		strcat (line," ");
		return line;
	}
	Sys_Error("Bad offset size in PR_GlobalString");
	return "";
}

char *PR_GlobalStringNoContents (progfuncs_t *progfuncs, int ofs)
{
	int		i;
	ddef16_t	*def16;
	ddef32_t	*def32;
	static char	line[128];

	switch (current_progstate->intsize)
	{
	case 16:
	case 24:
		def16 = ED_GlobalAtOfs16(progfuncs, ofs);
		if (!def16)
			sprintf (line,"%i(?""?""?)", ofs);
		else
			sprintf (line,"%i(%s)", ofs, def16->s_name+progfuncs->stringtable);
		break;
	case 32:
		def32 = ED_GlobalAtOfs32(progfuncs, ofs);
		if (!def32)
			sprintf (line,"%i(?""?""?)", ofs);
		else
			sprintf (line,"%i(%s)", ofs, def32->s_name+progfuncs->stringtable);
		break;
	default:
		Sys_Error("Bad offset size in PR_GlobalStringNoContents");
	}
	
	i = strlen(line);
	for ( ; i<20 ; i++)
		strcat (line," ");
	strcat (line," ");
		
	return line;
}


/*
=============
ED_Print

For debugging
=============
*/
void ED_Print (progfuncs_t *progfuncs, struct edict_s *ed)
{
	int		l;
	fdef_t	*d;
	int		*v;
	unsigned int		i;int j;
	char	*name;
	int		type;

	if (((edictrun_t *)ed)->isfree)
	{
		printf ("FREE\n");
		return;
	}

	printf("\nEDICT %i:\n", NUM_FOR_EDICT(progfuncs, (struct edict_s *)ed));
	for (i=1 ; i<numfields ; i++)
	{
		d = &field[i];
		name = d->name;
		l = strlen(name);
		if (l >= 2 && name[l-2] == '_')
			continue;	// skip _x, _y, _z vars
			
		v = (int *)((char *)edvars(ed) + d->ofs*4);

	// if the value is still all 0, skip the field
#ifdef DEF_SAVEGLOBAL
		type = d->type & ~DEF_SAVEGLOBAL;
#else
		type = d->type;
#endif
		
		for (j=0 ; j<type_size[type] ; j++)
			if (v[j])
				break;
		if (j == type_size[type])
			continue;
	
		printf ("%s",name);
		l = strlen (name);
		while (l++ < 15)
			printf (" ");

		printf ("%s\n", PR_ValueString(progfuncs, d->type, (eval_t *)v));		
	}
}

void ED_PrintNum (progfuncs_t *progfuncs, int ent)
{
	ED_Print (progfuncs, EDICT_NUM(progfuncs, ent));
}

/*
=============
ED_PrintEdicts

For debugging, prints all the entities in the current server
=============
*/
void ED_PrintEdicts (progfuncs_t *progfuncs)
{
	int		i;
	
	printf ("%i entities\n", sv_num_edicts);
	for (i=0 ; i<sv_num_edicts ; i++)
		ED_PrintNum (progfuncs, i);
}

/*
=============
ED_Count

For debugging
=============
*/
void ED_Count (progfuncs_t *progfuncs)
{
	int		i;
	edictrun_t	*ent;
	int		active, models, solid, step;

	active = models = solid = step = 0;
	for (i=0 ; i<sv_num_edicts ; i++)
	{
		ent = (edictrun_t *)EDICT_NUM(progfuncs, i);
		if (ent->isfree)
			continue;
		active++;
//		if (ent->v.solid)
//			solid++;
//		if (ent->v.model)
//			models++;
//		if (ent->v.movetype == MOVETYPE_STEP)
//			step++;
	}

	printf ("num_edicts:%3i\n", sv_num_edicts);
	printf ("active    :%3i\n", active);
//	Con_Printf ("view      :%3i\n", models);
//	Con_Printf ("touch     :%3i\n", solid);
//	Con_Printf ("step      :%3i\n", step);

}



//============================================================================


/*
=============
ED_NewString
=============
*/
char *ED_NewString (progfuncs_t *progfuncs, char *string, int minlength)
{
	char	*new, *new_p;
	int		i,l;

	minlength++;
	
	l = strlen(string) + 1;

	new = PRAddressableAlloc (progfuncs, l<minlength?minlength:l);
	new_p = new;

	for (i=0 ; i< l ; i++)
	{
		if (string[i] == '\\' && i < l-1 && string[i+1] != 0)
		{
			i++;
			if (string[i] == 'n')
				*new_p++ = '\n';
			else
				*new_p++ = '\\';
		}
		else
			*new_p++ = string[i];
	}
	
	return new;
}


/*
=============
ED_ParseEval

Can parse either fields or globals
returns false if error
=============
*/
pbool	ED_ParseEpair (progfuncs_t *progfuncs, void *base, ddefXX_t *key, char *s, int bits)
{
	int		i;
	char	string[128];
	fdef_t	*def;
	char	*v, *w;
	void	*d;
	string_t st;
	dfunction_t	*func;

	int type;
	
	switch(bits)
	{
	case 16:
		d = (void *)((int *)base + ((ddef16_t*)key)->ofs);

		if (pr_types)
			type = pr_types[((ddef16_t*)key)->type & ~DEF_SAVEGLOBAL].type;
		else
			type = ((ddef16_t*)key)->type & ~DEF_SAVEGLOBAL;
		break;
	case 32:
		d = (void *)((int *)base + ((ddef32_t*)key)->ofs);

		if (pr_types)
			type = pr_types[((ddef32_t*)key)->type & ~DEF_SAVEGLOBAL].type;
		else
			type = ((ddef32_t*)key)->type & ~DEF_SAVEGLOBAL;
		break;
	default:
		Sys_Error("Bad bits in ED_ParseEpair");
		d = 0;
	}
	
	switch (type)
	{
	case ev_string:
		st = ED_NewString (progfuncs, s, 0)-progfuncs->stringtable;
		*(string_t *)d = st;
		break;
		
	case ev_float:
		*(float *)d = (float)atof (s);
		break;

	case ev_integer:
		*(int *)d = atoi (s);
		break;
		
	case ev_vector:
		strcpy (string, s);
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
		
	case ev_entity:
		*(int *)d = atoi (s);
		break;

	case ev_field:
		def = ED_FindField (progfuncs, s);
		if (!def)
		{
			printf ("Can't find field %s\n", s);
			return false;
		}
		*(int *)d = def->ofs;
		break;

	case ev_function:
		if (s[1]==':'&&s[2]=='\0')
		{
			*(func_t *)d = 0;
			return true;
		}
		func = ED_FindFunction (progfuncs, s, &i, -1);
		if (!func)
		{
			printf ("Can't find function %s\n", s);
			return false;
		}
		*(func_t *)d = (func - pr_progstate[i].functions) | (i<<24);
		break;
		
	default:
		break;
	}
	return true;
}

/*
====================
ED_ParseEdict

Parses an edict out of the given string, returning the new position
ed should be a properly initialized empty edict.
Used for initial level load and for savegames.
====================
*/
#if 1
char *ED_ParseEdict (progfuncs_t *progfuncs, char *data, edictrun_t *ent)
{
	fdef_t		*key;
	pbool	init;
	char		keyname[256];
	int			n;

//	eval_t		*val;

	init = false;

// clear it
//	if (ent != (edictrun_t *)sv_edicts)	// hack
//		memset (ent+1, 0, pr_edict_size - sizeof(edictrun_t));

// go through all the dictionary pairs
	while (1)
	{	
	// parse key
		data = QCC_COM_Parse (data);
		if (qcc_token[0] == '}')
			break;
		if (!data)
			Sys_Error ("ED_ParseEntity: EOF without closing brace");
		
		strcpy (keyname, qcc_token);

		// another hack to fix heynames with trailing spaces
		n = strlen(keyname);
		while (n && keyname[n-1] == ' ')
		{
			keyname[n-1] = 0;
			n--;
		}

	// parse value	
		data = QCC_COM_Parse (data);
		if (!data)
			Sys_Error ("ED_ParseEntity: EOF without closing brace");

		if (qcc_token[0] == '}')
			Sys_Error ("ED_ParseEntity: closing brace without data");

		init = true;	

// keynames with a leading underscore are used for utility comments,
// and are immediately discarded by quake
		if (keyname[0] == '_')
			continue;
		
		key = ED_FindField (progfuncs, keyname);		
		if (!key)
		{
			if (!strcmp(keyname, "angle"))	//Quake anglehack - we've got to leave it in cos it doesn't work for quake otherwise, and this is a QuakeC lib!
			{
				if ((key = ED_FindField (progfuncs, "angles")))
				{
					sprintf (qcc_token, "0 %f 0", atof(qcc_token));	//change it from yaw to 3d angle
					goto cont;
				}
			}
			if (!strcmp(keyname, "light"))	//Quake lighthack - allows a field name and a classname to go by the same thing in the level editor
				if ((key = ED_FindField (progfuncs, "light_lev")))
					goto cont;
			printf ("'%s' is not a field\n", keyname);
			continue;
		}

cont:
		if (!ED_ParseEpair (progfuncs, ent->fields, (ddefXX_t*)key, qcc_token, 32))
			Sys_Error ("ED_ParseEdict: parse error on entities");
	}

	if (!init)
		ent->isfree = true;

	return data;
}
#endif

/*
================
ED_LoadFromFile

The entities are directly placed in the array, rather than allocated with
ED_Alloc, because otherwise an error loading the map would have entity
number references out of order.

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.

Used for both fresh maps and savegame loads.  A fresh map would also need
to call ED_CallSpawnFunctions () to let the objects initialize themselves.
================
*/

char *ED_WriteGlobals(progfuncs_t *progfuncs, char *buffer)	//switch first.
{
#define AddS(str) strcpy(buffer, str);buffer+=strlen(str);
	int		*v;
	ddef32_t		*def32;
	ddef16_t		*def16;
	unsigned int			i;
	int j;	
	char	*name;
	int			type;
	int curprogs = pr_typecurrent;
	int len;
	switch(current_progstate->intsize)
	{
	case 16:
	case 24:
		for (i=0 ; i<pr_progs->numglobaldefs ; i++)
		{
			def16 = &pr_globaldefs16[i];					
			name = def16->s_name + progfuncs->stringtable;
			len = strlen(name);
			if (!*name)
				continue;
			if (name[len-2] == '_' && (name[len-1] == 'x' || name[len-1] == 'y' || name[len-1] == 'z'))
				continue;	// skip _x, _y, _z vars (vector componants, which are saved as one vector not 3 floats)

			type = def16->type;

#ifdef DEF_SAVEGLOBAL
			if ( !(def16->type & DEF_SAVEGLOBAL) )
				continue;
			type &= ~DEF_SAVEGLOBAL;
#endif
			if (current_progstate->types)
				type = current_progstate->types[type].type;
			if (type == ev_function)
			{
				v = (int *)&current_progstate->globals[def16->ofs];
				if ((v[0]&0xff000000)>>24 == (unsigned)curprogs)	//same progs
				{
					if (!progfuncs->stringtable[current_progstate->functions[v[0]&0x00ffffff].s_name])
						continue;
					else if (!strcmp(current_progstate->functions[v[0]&0x00ffffff].s_name+ progfuncs->stringtable, name))	//names match. Assume function is at initial value.
						continue;
				}

				if (curprogs!=0)
				if ((v[0]&0xff000000)>>24 == 0)
					if (!ED_FindFunction(progfuncs, name, NULL, curprogs))	//defined as extern
					{
						if (!progfuncs->stringtable[pr_progstate[0].functions[v[0]&0x00ffffff].s_name])
							continue;
						else if (!strcmp(pr_progstate[0].functions[v[0]&0x00ffffff].s_name + progfuncs->stringtable, name))	//same name.
							continue;
					}

				//else function has been redirected externally.
				goto add16;
			}
			else if (type != ev_string	//anything other than these is not saved
			&& type != ev_float
			&& type != ev_integer
			&& type != ev_entity
			&& type != ev_vector)
				continue;

			v = (int *)&current_progstate->globals[def16->ofs];

			// make sure the value is not null, where there's no point in saving
			for (j=0 ; j<type_size[type] ; j++)
				if (v[j])
					break;
			if (j == type_size[type])
				continue;

 add16:
			AddS (qcva("\"%s\" ", name));
			AddS (qcva("\"%s\"\n", PR_UglyValueString(progfuncs, def16->type&~DEF_SAVEGLOBAL, (eval_t *)v)));
		}
		break;
	case 32:
		for (i=0 ; i<pr_progs->numglobaldefs ; i++)
		{
			def32 = &pr_globaldefs32[i];					
			name = def32->s_name + progfuncs->stringtable;
			if (name[strlen(name)-2] == '_')
				continue;	// skip _x, _y, _z vars (vector componants, which are saved as one vector not 3 floats)

			type = def32->type;

#ifdef DEF_SAVEGLOBAL
			if ( !(def32->type & DEF_SAVEGLOBAL) )
				continue;
			type &= ~DEF_SAVEGLOBAL;
#endif
			if (current_progstate->types)
				type = current_progstate->types[type].type;
			if (type == ev_function)
			{
				v = (int *)&current_progstate->globals[def32->ofs];
				if ((v[0]&0xff000000)>>24 == (unsigned)curprogs)	//same progs
					if (!strcmp(current_progstate->functions[v[0]&0x00ffffff].s_name+ progfuncs->stringtable, name))	//names match. Assume function is at initial value.
						continue;						

				if (curprogs!=0)
				if ((v[0]&0xff000000)>>24 == 0)
					if (!ED_FindFunction(progfuncs, name, NULL, curprogs))	//defined as extern
						if (!strcmp(pr_progstate[0].functions[v[0]&0x00ffffff].s_name+ progfuncs->stringtable, name))	//same name.
							continue;

				//else function has been redirected externally.
				goto add32;
			}
			else if (type != ev_string	//anything other than these is not saved
			&& type != ev_float
			&& type != ev_integer
			&& type != ev_entity
			&& type != ev_vector)
				continue;

			v = (int *)&current_progstate->globals[def32->ofs];

			// make sure the value is not null, where there's no point in saving
			for (j=0 ; j<type_size[type] ; j++)
				if (v[j])
					break;
			if (j == type_size[type])
				continue;
add32:
			AddS (qcva("\"%s\" ", name));
			AddS (qcva("\"%s\"\n", PR_UglyValueString(progfuncs, def32->type&~DEF_SAVEGLOBAL, (eval_t *)v)));
		}
		break;
	default:
		Sys_Error("Bad number of bits in SaveEnts");
	}

	return buffer;
}

char *ED_WriteEdict(progfuncs_t *progfuncs, edictrun_t *ed, char *buffer, pbool q1compatable)
{
	fdef_t	*d;

	int		*v;
	unsigned int		i;int j;
	char	*name;
	int		type;
	int len;

	for (i=0 ; i<numfields ; i++)
	{
		d = &field[i];
		name = d->name;
		len = strlen(name);
		if (len>4 && (name[len-2] == '_' && (name[len-1] == 'x' || name[len-1] == 'y' || name[len-1] == 'z')))
			continue;	// skip _x, _y, _z vars
			
		v = (int *)((char*)ed->fields + d->ofs*4);

	// if the value is still all 0, skip the field
#ifdef DEF_SAVEGLOBAL
		type = d->type & ~DEF_SAVEGLOBAL;
#else
		type = d->type;
#endif
	
		for (j=0 ; j<type_size[type] ; j++)
			if (v[j])
				break;
		if (j == type_size[type])
			continue;
	
		//add it to the file
		AddS (qcva("\"%s\" ",name));
		AddS (qcva("\"%s\"\n", (q1compatable?PR_UglyOldValueString:PR_UglyValueString)(progfuncs, d->type, (eval_t *)v)));
	}

	return buffer;
#undef AddS
}

char *SaveCallStack (progfuncs_t *progfuncs, char *s)
{
#define AddS(str) strcpy(s, str);s+=strlen(str);
	char buffer[8192];
	dfunction_t	*f;
	int			i;
	int progs;

	int arg;
	int *globalbase;

	progs = -1;

	if (pr_depth == 0)
	{
		AddS ("<NO STACK>\n");
		return s;
	}

	globalbase = (int *)pr_globals + pr_xfunction->parm_start + pr_xfunction->locals;

	pr_stack[pr_depth].f = pr_xfunction;
	for (i=pr_depth ; i>0 ; i--)
	{
		f = pr_stack[i].f;
		
		if (!f)
		{
			AddS ("<NO FUNCTION>\n");
		}
		else
		{
			if (pr_stack[i].progsnum != progs)
			{
				progs = pr_stack[i].progsnum;

				sprintf(buffer, "//%i %s\n", progs, pr_progstate[progs].filename);
				AddS (buffer);
			}
			if (!f->s_file)
				sprintf(buffer, "\t\"%i:%s\"\n", progs, f->s_name+progfuncs->stringtable);
			else
				sprintf(buffer, "\t\"%i:%s\" //%s\n", progs, f->s_name+progfuncs->stringtable, f->s_file+progfuncs->stringtable);
			AddS (buffer);

			AddS ("\t{\n");
			for (arg = 0; arg < f->locals; arg++)
			{
				ddef16_t *local;
				local = ED_GlobalAtOfs16(progfuncs, f->parm_start+arg);
				if (!local)
					sprintf(buffer, "\t\tofs%i %i // %f\n", f->parm_start+arg, *(int *)(globalbase - f->locals+arg), *(float *)(globalbase - f->locals+arg) );
				else
				{
//__try
//{
					if (local->type == ev_entity)
					{	//go safly.
						int n;
						sprintf(buffer, "\t\t\"%s\"\t\"entity INVALID POINTER\"\n", local->s_name+progfuncs->stringtable);
						for (n = 0; n < sv_num_edicts; n++)
						{
							if (prinst->edicttable[n] == (struct edict_s *)PROG_TO_EDICT(progfuncs, ((eval_t*)(globalbase - f->locals+arg))->edict))
							{
								sprintf(buffer, "\t\t\"%s\" \"entity %i\"\n", local->s_name+progfuncs->stringtable, n);
								break;
							}
						}
					}
					else
						sprintf(buffer, "\t\t\"%s\"\t\"%s\"\n", local->s_name+progfuncs->stringtable, PR_ValueString(progfuncs, local->type, (eval_t*)(globalbase - f->locals+arg)));
//}
//__except(EXCEPTION_EXECUTE_HANDLER)
//{
//	sprintf(buffer, "\t\t\"%s\" \"ILLEGAL POINTER\"\n", local->s_name+progfuncs->stringtable);
//}
					if (local->type == ev_vector)
						arg+=2;
				}
				AddS (buffer);
			}
			AddS ("\t}\n");

			if (i == pr_depth)
				globalbase = localstack + localstack_used - f->locals;
			else
				globalbase -= f->locals;
		}
	}
	return s;
#undef AddS
}

//there are two ways of saving everything.
//0 is to save just the entities.
//1 is to save the entites, and all the progs info so that all the variables are saved off, and it can be reloaded to exactly how it was (provided no files or data has been changed outside, like the progs.dat for example)
char *SaveEnts(progfuncs_t *progfuncs, char *mem, int *len, int alldata)
{
#define AddS(str) strcpy(s, str);s+=strlen(str);
	char *s, *os;
	int a;
	int oldprogs;
	
	if (mem)
	{
		os = s = mem;
	}
	else
		os = s = memalloc(5*1024*1024);

	if (alldata == 2)
	{	//special Q1 savegame compatability mode.
		//engine will need to store references to progs type and will need to preload the progs and inti the ents itself before loading.

		//Make sure there is only 1 progs loaded.
		for (a = 1; a < maxprogs; a++)
		{
			if (pr_progstate[a].progs)
				break;
		}
		if (!pr_progstate[0].progs || a != maxprogs)	//the state of the progs wasn't Q1 compatable.
		{
			memfree(os);
			return NULL;
		}

		//write the globals
		AddS ("{\n");

		oldprogs = pr_typecurrent;
		PR_SwitchProgs(progfuncs, 0);

		s = ED_WriteGlobals(progfuncs, s);

		PR_SwitchProgs(progfuncs, oldprogs);

		AddS ("}\n");


		//write the ents
		for (a = 0; a < sv_num_edicts; a++)
		{
			edictrun_t *ed = (edictrun_t *)EDICT_NUM(progfuncs, a);

			AddS ("{\n");

			if (!ed->isfree)
				s = ED_WriteEdict(progfuncs, ed, s, true);

			AddS ("}\n");
		}

		*len = s - os;
		return os;
	}

	if (alldata)
	{
		AddS("general {\n");
		AddS(qcva("\"maxprogs\" \"%i\"\n", maxprogs));
//		AddS(qcva("\"maxentities\" \"%i\"\n", maxedicts));
//		AddS(qcva("\"mem\" \"%i\"\n", hunksize));
//		AddS(qcva("\"crc\" \"%i\"\n", header_crc));
		AddS(qcva("\"numentities\" \"%i\"\n", sv_num_edicts));
		AddS("}\n");
		
		oldprogs = pr_typecurrent;

		for (a = 0; a < maxprogs; a++)
		{
			if (!pr_progstate[a].progs)
				continue;
			PR_SwitchProgs(progfuncs, a);
			{
				AddS (qcva("progs %i {\n", a));
				AddS (qcva("\"filename\" \"%s\"\n", pr_progstate[a].filename));
				AddS (qcva("\"crc\" \"%i\"\n", pr_progs->crc));
				AddS (qcva("\"numbuiltins\" \"%i\"\n", current_progstate->numbuiltins));
				AddS ("}\n");
			}
		}

		if (alldata == 3)
		{
			//include callstack
			AddS("stacktrace {\n");
			s = SaveCallStack(progfuncs, s);
			AddS("}\n");
		}

		for (a = 0; a < maxprogs; a++)	//I would mix, but external functions rely on other progs being loaded
		{
			if (!pr_progstate[a].progs)
				continue;

			AddS (qcva("globals %i {\n", a));

			PR_SwitchProgs(progfuncs, a);

			s = ED_WriteGlobals(progfuncs, s);

			AddS ("}\n");
		}
		PR_SwitchProgs(progfuncs, oldprogs);
	}
	for (a = 0; a < sv_num_edicts; a++)
	{
		edictrun_t *ed = (edictrun_t *)EDICT_NUM(progfuncs, a);

		if (ed->isfree)
			continue;

		AddS (qcva("entity %i{\n", a));

		s = ED_WriteEdict(progfuncs, ed, s, false);

		AddS ("}\n");
	}

	*len = s - os;
	return os;

#undef AddS
}

int header_crc;

//if 'general' block is found, this is a compleate state, otherwise, we should spawn entities like
int LoadEnts(progfuncs_t *progfuncs, char *file, float killonspawnflags)
{
	eval_t *fulldata;	//this is part of FTE_FULLSPAWNDATA
	char *datastart;
	
	eval_t *var;

	char filename[128];
	int num;	
	int numbuiltins;	
	edictrun_t *ed=NULL;
	ddef16_t *d16;
	ddef32_t *d32;
	func_t CheckSpawn=0;

	extern edictrun_t tempedict;

	int crc = 1;
	int entsize = 0;
	int numents = 0;

	pbool resethunk=0;
	pbool isloadgame;
	if (!strncmp(file, "loadgame", 8))
	{
		isloadgame = true;
		numents = -1;
		file+=8;
		fulldata = NULL;
	}
	else
	{
		isloadgame = false;

		if (pr_typecurrent>=0)
			num = ED_FindGlobalOfs(progfuncs, "__fullspawndata");
		else
			num = 0;
		if (num)
			fulldata = (eval_t *)((int *)pr_globals + num);
		else
			fulldata = NULL;
	}

	while(1)
	{
		datastart = file;
		file = QCC_COM_Parse(file);
		if (file == NULL)
			break;	//finished reading file
		else if (!strcmp(qcc_token, "entity"))
		{
			if (entsize == 0 && resethunk)	//edicts have not yet been initialized, and this is a compleate load (memsize has been set)
			{
				entsize = PR_InitEnts(progfuncs, maxedicts);
//				sv_num_edicts = numents;

				for (num = 0; num < numents; num++)
				{
					ed = (edictrun_t *)EDICT_NUM(progfuncs, num);

					if (!ed)
					{
						prinst->edicttable[num] = *(struct edict_s **)&ed = (void*)memalloc(externs->edictsize);
						ed->fields = PRAddressableAlloc(progfuncs, fields_size);
						ed->entnum = num;
						ed->isfree = true;
					}
				}
			}

			file = QCC_COM_Parse(file);
			num = atoi(qcc_token);
			file = QCC_COM_Parse(file);
			if (qcc_token[0] != '{')
				Sys_Error("Progs loading found %s, not '{'", qcc_token);
			if (!resethunk)
				ed = (edictrun_t *)ED_Alloc(progfuncs);
			else
			{
				ed = (edictrun_t *)EDICT_NUM(progfuncs, num);

				if (!ed)
				{
					Sys_Error("Edict was not allocated\n");
					prinst->edicttable[num] = *(struct edict_s **)&ed = (void*)memalloc(externs->edictsize);
					ed->fields = PRAddressableAlloc(progfuncs, fields_size);
					ED_ClearEdict(progfuncs, ed);
					ed->entnum = num;
				}
			}
			ed->isfree = false;
			file = ED_ParseEdict(progfuncs, file, ed);

			if (killonspawnflags)
			{
				var = GetEdictFieldValue (progfuncs, (struct edict_s *)&ed, "spawnflags", &spawnflagscache);
				if (var)
				{
					if ((int)var->_float & (int)killonspawnflags)
					{
						ed->isfree = true;
						continue;
					}
				}
			}

			if (!resethunk)
			{
				dfunction_t *f;
				if ((var = GetEdictFieldValue (progfuncs, (struct edict_s *)ed, "classname", NULL)))
				{
					f = ED_FindFunction(progfuncs, var->string + progfuncs->stringtable, NULL, -1);
					if (f)
					{
						var = (eval_t *)((int *)pr_globals + ED_FindGlobalOfs(progfuncs, "self"));
						var->edict = EDICT_TO_PROG(progfuncs, ed);
						PR_ExecuteProgram(progfuncs, f-pr_functions);
					}
				}
			}
		}
		else if (!strcmp(qcc_token, "progs"))
		{
			file = QCC_COM_Parse(file);
			num = atoi(qcc_token);
			file = QCC_COM_Parse(file);			
			if (qcc_token[0] != '{')
				Sys_Error("Progs loading found %s, not '{'", qcc_token);

			
			filename[0] = '\0';		
			header_crc = 0;		
			numbuiltins = 0;

			while(1)
			{
				file = QCC_COM_Parse(file);	//read the key
				if (!file)
					Sys_Error("EOF in progs block");

				if (!strcmp("filename", qcc_token))	//check key get and save values
					{file = QCC_COM_Parse(file); strcpy(filename, qcc_token);}
				else if (!strcmp("crc", qcc_token))
					{file = QCC_COM_Parse(file); header_crc = atoi(qcc_token);}
				else if (!strcmp("numbuiltins", qcc_token))
					{file = QCC_COM_Parse(file); numbuiltins = atoi(qcc_token);}
				else if (qcc_token[0] == '}')	//end of block
					break;
				else
					Sys_Error("Bad key \"%s\" in progs block", qcc_token);
			}

			PR_ReallyLoadProgs(progfuncs, filename, header_crc, &pr_progstate[num], true);
			if (!externs->builtinsfor)
			{
			//	Sys_Error("Couldn't reset the builtin functions");
				current_progstate->builtins = NULL;	//these are specific, we assume the global ones were set via pr_configure
				current_progstate->numbuiltins = 0;
			}
			else
			{
				current_progstate->builtins = externs->builtinsfor(num, header_crc);
				current_progstate->numbuiltins = numbuiltins;
			}
		}
		else if (!strcmp(qcc_token, "globals"))
		{
			if (entsize == 0 && resethunk)	//by the time we parse some globals, we MUST have loaded all progs
			{
				entsize = PR_InitEnts(progfuncs, maxedicts);
//				sv_num_edicts = numents;

				for (num = 0; num < numents; num++)
				{
					ed = (edictrun_t *)EDICT_NUM(progfuncs, num);

					if (!ed)
					{
						prinst->edicttable[num] = *(struct edict_s **)&ed = (void*)memalloc(externs->edictsize);
						ed->fields = PRAddressableAlloc(progfuncs, fields_size);
						ED_ClearEdict(progfuncs, ed);
						ed->entnum = num;
						ed->isfree = true;
					}
				}
			}

			file = QCC_COM_Parse(file);
			num = atoi(qcc_token);

			file = QCC_COM_Parse(file);
			if (qcc_token[0] != '{')
				Sys_Error("Globals loading found \'%s\', not '{'", qcc_token);
			
			PR_SwitchProgs(progfuncs, num);
			while (1)
			{				
				file = QCC_COM_Parse(file);
				if (qcc_token[0] == '}')
					break;
				else if (!qcc_token[0] || !file)
					Sys_Error("EOF when parsing global values");

				switch(current_progstate->intsize)
				{
				case 16:
				case 24:
					if (!(d16 = ED_FindGlobal16(progfuncs, qcc_token)))
					{
						file = QCC_COM_Parse(file);
						printf("global value %s not found", qcc_token);
					}
					else
					{
						file = QCC_COM_Parse(file);
						ED_ParseEpair(progfuncs, pr_globals, (ddefXX_t*)d16, qcc_token, 16);
					}
					break;
				case 32:
					if (!(d32 = ED_FindGlobal32(progfuncs, qcc_token)))
					{
						file = QCC_COM_Parse(file);
						printf("global value %s not found", qcc_token);
					}
					else
					{
						file = QCC_COM_Parse(file);
						ED_ParseEpair(progfuncs, pr_globals, (ddefXX_t*)d32, qcc_token, 32);
					}
					break;
				default:
					Sys_Error("Bad intsize in LoadEnts");
				}
			}

//			file = QCC_COM_Parse(file);
//			if (com_token[0] != '}')
//				Sys_Error("Progs loading found %s, not '}'", qcc_token);			
		}		
		else if (!strcmp(qcc_token, "general"))
		{
			QC_StartShares(progfuncs);
//			QC_InitShares();	//forget stuff
//			pr_edict_size = 0;
			max_fields_size=0;

			file = QCC_COM_Parse(file);
			if (qcc_token[0] != '{')
				Sys_Error("Progs loading found %s, not '{'", qcc_token);

			while(1)
			{
				file = QCC_COM_Parse(file);	//read the key
				if (!file)
					Sys_Error("EOF in general block");

				if (!strcmp("maxprogs", qcc_token))	//check key get and save values
					{file = QCC_COM_Parse(file); maxprogs = atoi(qcc_token);}
//				else if (!strcmp("maxentities", com_token))
//					{file = QCC_COM_Parse(file); maxedicts = atoi(qcc_token);}
//				else if (!strcmp("mem", com_token))
//					{file = QCC_COM_Parse(file); memsize = atoi(qcc_token);}
//				else if (!strcmp("crc", com_token))
//					{file = QCC_COM_Parse(file); crc = atoi(qcc_token);}
				else if (!strcmp("numentities", qcc_token))
					{file = QCC_COM_Parse(file); numents = atoi(qcc_token);}
				else if (qcc_token[0] == '}')	//end of block
					break;
				else
					Sys_Error("Bad key \"%s\" in general block", qcc_token);
			}
			
			PRAddressableFlush(progfuncs, -1);
			resethunk=true;

			pr_progstate = PRHunkAlloc(progfuncs, sizeof(progstate_t) * maxprogs);
			pr_typecurrent=0;

			sv_num_edicts = 1;	//set up a safty buffer so things won't go horribly wrong too often
			sv_edicts=(struct edict_s *)&tempedict;
			prinst->edicttable = &sv_edicts;


			sv_num_edicts = numents;	//should be fine

//			PR_Configure(crc, NULL, memsize, maxedicts, maxprogs);
		}		
		else if (!strcmp(qcc_token, "{"))
		{
			if (isloadgame)
			{
				if (numents == -1)	//globals
				{
					while (1)
					{				
						file = QCC_COM_Parse(file);
						if (qcc_token[0] == '}')
							break;
						else if (!qcc_token[0] || !file)
							Sys_Error("EOF when parsing global values");

						switch(current_progstate->intsize)
						{
						case 16:
						case 24:
							if (!(d16 = ED_FindGlobal16(progfuncs, qcc_token)))
							{
								file = QCC_COM_Parse(file);
								printf("global value %s not found", qcc_token);
							}
							else
							{
								file = QCC_COM_Parse(file);
								ED_ParseEpair(progfuncs, pr_globals, (ddefXX_t*)d16, qcc_token, 16);
							}
							break;
						case 32:
							if (!(d32 = ED_FindGlobal32(progfuncs, qcc_token)))
							{
								file = QCC_COM_Parse(file);
								printf("global value %s not found", qcc_token);
							}
							else
							{
								file = QCC_COM_Parse(file);
								ED_ParseEpair(progfuncs, pr_globals, (ddefXX_t*)d32, qcc_token, 32);
							}
							break;
						default:
							Sys_Error("Bad intsize in LoadEnts");
						}
					}
				}
				else
				{
					ed = (edictrun_t *)EDICT_NUM(progfuncs, numents);
					if (!ed)
					{
						prinst->edicttable[numents] = *(struct edict_s **)&ed = (void*)memalloc(externs->edictsize);
						ed->fields = PRAddressableAlloc(progfuncs, fields_size);
						ED_ClearEdict(progfuncs, ed);
						ed->entnum = numents;
						ed->isfree = true;
					}

					sv_num_edicts = numents;
					ed->isfree = false;
					file = ED_ParseEdict (progfuncs, file, ed);
				}
				numents++;
				continue;
			}

			if (entsize == 0 && resethunk)	//edicts have not yet been initialized, and this is a compleate load (memsize has been set)
			{
				entsize = PR_InitEnts(progfuncs, maxedicts);
//				sv_num_edicts = numents;

				for (num = 0; num < numents; num++)
				{
					ed = (edictrun_t *)EDICT_NUM(progfuncs, num);

					if (!ed)
					{
						prinst->edicttable[num] = *(struct edict_s **)&ed = (void*)memalloc(externs->edictsize);
						ed->fields = PRAddressableAlloc(progfuncs, fields_size);
						ED_ClearEdict(progfuncs, ed);
						ed->entnum = num;
						ed->isfree = true;
					}
				}
			}

			if (!ed)	//first entity
				ed = (edictrun_t *)EDICT_NUM(progfuncs, 0);
			else
				ed = (edictrun_t *)ED_Alloc(progfuncs);
			ed->isfree = false;
			file = ED_ParseEdict(progfuncs, file, ed);

			if (killonspawnflags)
			{
				var = GetEdictFieldValue (progfuncs, (struct edict_s *)ed, "spawnflags", &spawnflagscache);
				if (var)
				{
					if ((int)var->_float & (int)killonspawnflags)
					{
						ed->isfree = true;
						ed->freetime = 0;
						continue;
					}
				}
			}

			if (!resethunk)
			{				
				func_t f;
				if (!CheckSpawn)
					CheckSpawn = PR_FindFunc(progfuncs, "CheckSpawn", -2);

				var = GetEdictFieldValue (progfuncs, (struct edict_s *)ed, "classname", NULL);
				if (!var || !var->string || !*(var->string+progfuncs->stringtable))
				{
					printf("No classname\n");
					ED_Free(progfuncs, (struct edict_s *)ed);
				}
				else
				{
					eval_t *selfvar;

					//added by request of Mercury.
					if (fulldata)	//this is a vital part of HL map support!!!
					{	//essentually, it passes the ent's spawn info to the ent.
						char *nl;	//otherwise it sees only the named fields of 
						char *spawndata;//a standard quake ent.
						spawndata = PRHunkAlloc(progfuncs, file - datastart +1);
						strncpy(spawndata, datastart, file - datastart);
						spawndata[file - datastart] = '\0';
						for (nl = spawndata; *nl; nl++)
							if (*nl == '\n')
								*nl = '\t';
						fulldata->string = spawndata - progfuncs->stringtable;
					}

					selfvar = (eval_t *)((int *)pr_globals + ED_FindGlobalOfs(progfuncs, "self"));
					selfvar->edict = EDICT_TO_PROG(progfuncs, ed);

					f = PR_FindFunc(progfuncs, var->string+progfuncs->stringtable, PR_ANYBACK);
					if (f)
					{
						if (CheckSpawn)
						{
							G_INT(OFS_PARM0) = f;
							PR_ExecuteProgram(progfuncs, CheckSpawn);
							//call the spawn func or remove.
						}
						else
							PR_ExecuteProgram(progfuncs, f);
					}
					else if (CheckSpawn)
					{
						G_INT(OFS_PARM0) = 0;
						PR_ExecuteProgram(progfuncs, CheckSpawn);
						//the mod is responsible for freeing unrecognised ents.
					}
					else
					{
						printf("Couldn't find spawn function %s\n", var->string+progfuncs->stringtable);
						ED_Free(progfuncs, (struct edict_s *)ed);
					}
				}
			}
		}
		else
			Sys_Error("Command %s not recognised", qcc_token);
	}
	if (resethunk)
	{
		header_crc = crc;	
		if (externs->loadcompleate)
			externs->loadcompleate(entsize);

		sv_num_edicts = numents;
	}

	if (resethunk)
	{
		return entsize;
	}
	else
		return max_fields_size;
}

#define AddS(str) strcpy(s, str);s+=strlen(str);

char *SaveEnt (progfuncs_t *progfuncs, char *buf, int *size, struct edict_s *ed)
{
	fdef_t	*d;
	int		*v;
	unsigned int		i;int j;
	char	*name;
	int		type;	

	char *s = buf;

//	if (ed->free)
//		continue;

	AddS ("{\n");


	for (i=0 ; i<numfields ; i++)
	{
		d = &field[i];
		name = d->name;
		if (name[strlen(name)-2] == '_')
			continue;	// skip _x, _y, _z vars
			
		v = (int*)((edictrun_t*)ed)->fields + d->ofs;

	// if the value is still all 0, skip the field
		type = d->type & ~DEF_SAVEGLOBAL;
		for (j=0 ; j<type_size[type] ; j++)
			if (v[j])
				break;
		if (j == type_size[type])
			continue;
	
		//add it to the file
		AddS (qcva("\"%s\" ",name));
		AddS (qcva("\"%s\"\n", PR_UglyValueString(progfuncs, d->type, (eval_t *)v)));
	}

	AddS ("}\n");

	*size = s - buf;

	return buf;
}
struct edict_s *RestoreEnt (progfuncs_t *progfuncs, char *buf, int *size, struct edict_s *ed)
{
	edictrun_t *ent;
	char *start = buf;

	buf = QCC_COM_Parse(buf);	//read the key
	if (!buf || !*qcc_token)
		return NULL;

	if (strcmp(qcc_token, "{"))
		Sys_Error("Restore Ent with no opening brace");

	if (!ed)
		ent = (edictrun_t *)ED_Alloc(progfuncs);
	else
		ent = (edictrun_t *)ed;
	ent->isfree = false;

	buf = ED_ParseEdict(progfuncs, buf, ent);

	*size = buf - start;

	return (struct edict_s *)ent;
}

#define Host_Error Sys_Error

//return true if pr_progs needs recompiling (source files have changed)
pbool PR_TestRecompile(progfuncs_t *progfuncs)
{
	int newsize;
	int num, found=0, lost=0, changed=0;
	includeddatafile_t *s;
	if (!pr_progs->ofsfiles)
		return false;
	
	num = *(int*)((char *)pr_progs + pr_progs->ofsfiles);
	s = (includeddatafile_t *)((char *)pr_progs + pr_progs->ofsfiles+4);	
	while(num>0)
	{
		newsize = externs->FileSize(s->filename);
		if (newsize == -1)	//ignore now missing files. - the referencer must have changed...
			lost++;
		else if (s->size != newsize)	//file
			changed++;
		else
			found++;

		s++;
		num--;
	}
	if (lost > found+changed)
		return false;
	if (changed)
		return true;
	return false;
}
/*
#ifdef _DEBUG
//this is for debugging.
//I'm using this to detect incorrect string types while converting 32bit string pointers with bias to bound indexes.
void PR_TestForWierdness(progfuncs_t *progfuncs)
{
	unsigned int i;
	int e;
	edictrun_t *ed;
	for (i = 0; i < pr_progs->numglobaldefs; i++)
	{
		if ((pr_globaldefs16[i].type&~(DEF_SHARED|DEF_SAVEGLOBAL)) == ev_string)
		{
			if (G_INT(pr_globaldefs16[i].ofs) < 0 || G_INT(pr_globaldefs16[i].ofs) >= addressableused)
				printf("String type irregularity on \"%s\" \"%s\"\n", pr_globaldefs16[i].s_name+progfuncs->stringtable, G_INT(pr_globaldefs16[i].ofs)+progfuncs->stringtable);
		}
	}

	for (i = 0; i < numfields; i++)
	{
		if ((field[i].type&~(DEF_SHARED|DEF_SAVEGLOBAL)) == ev_string)
		{
			for (e = 0; e < sv_num_edicts; e++)
			{
				ed = (edictrun_t*)EDICT_NUM(progfuncs, e);
				if (ed->isfree)
					continue;
				if (((int *)ed->fields)[field[i].ofs] < 0 || ((int *)ed->fields)[field[i].ofs] >= addressableused)
					printf("String type irregularity \"%s\" \"%s\"\n", field[i].name, ((int *)ed->fields)[field[i].ofs]+progfuncs->stringtable);
			}
		}
	}
}
#endif
*/
char *decode(int complen, int len, int method, char *info, char *buffer);
/*
===============
PR_LoadProgs
===============
*/
int PR_ReallyLoadProgs (progfuncs_t *progfuncs, char *filename, int headercrc, progstate_t *progstate, pbool complain)
{
	unsigned int		i, type;
//	extensionbuiltin_t *eb;
//	float	fl;
	int len;
//	int num;
//	dfunction_t *f, *f2;
	ddef16_t *d16;	
	ddef32_t *d32;
	int *d2;
	eval_t *eval;
	char *s;
	int progstype;
	int trysleft = 2;
//	bool qfhack = false;
	pbool isfriked = false;	//all imediate values were stripped, which causes problems with strings.
	pbool hexencalling = false;	//hexen style calling convention. The opcodes themselves are used as part of passing the arguments;
	ddef16_t *gd16, *fld16;
	float *glob;
	dfunction_t *fnc;
	dstatement16_t *st16;

	int hmark=0xffffffff;

	int reorg = prinst->reorganisefields;

	int stringadjust;

	current_progstate = progstate;	

	strcpy(current_progstate->filename, filename);
		

// flush the non-C variable lookup cache
//	for (i=0 ; i<GEFV_CACHESIZE ; i++)
//		gefvCache[i].field[0] = 0;

	memset(&spawnflagscache, 0, sizeof(evalc_t));

	if (externs->autocompile == PR_COMPILEALWAYS)	//always compile before loading
	{
		printf("Forcing compile of progs %s\n", filename);
		if (!CompileFile(progfuncs, filename))
			return false;
	}

//	CRC_Init (&pr_crc);

retry:
	if ((len=externs->FileSize(filename))<=0)
	{
		if (externs->autocompile == PR_COMPILENEXIST || externs->autocompile == PR_COMPILECHANGED)	//compile if file is not found (if 2, we have already tried, so don't bother)
		{
			if (hmark==0xffffffff)	//first try
			{
				printf("couldn't open progs %s. Attempting to compile.\n", filename);
				CompileFile(progfuncs, filename);
			}
			if ((len=externs->FileSize(filename))<0)
			{
				printf("Couldn't find or compile file %s\n", filename);
				return false;
			}
		}
		else if (externs->autocompile == PR_COMPILEIGNORE)
			return false;
		else
		{
			printf("Couldn't find file %s\n", filename);
			return false;
		}
	}

	hmark = PRHunkMark(progfuncs);
	pr_progs = PRHunkAlloc(progfuncs, len+1);
	if (!externs->ReadFile(filename, pr_progs, len+1))
	{
		if (!complain)
			return false;
		printf("Failed to open %s", filename);
		return false;
	}

//	for (i=0 ; i<len ; i++)
//		CRC_ProcessByte (&pr_crc, ((byte *)pr_progs)[i]);

// byte swap the header
#ifndef NOENDIAN
	for (i=0 ; i<sizeof(*pr_progs)/4 ; i++)
		((int *)pr_progs)[i] = PRLittleLong ( ((int *)pr_progs)[i] );
#endif

	if (pr_progs->version == PROG_VERSION)
	{
//		printf("Opening standard progs file \"%s\"\n", filename);
		current_progstate->intsize = 16;
	}
	else if (pr_progs->version == PROG_EXTENDEDVERSION)
	{
		if (pr_progs->secondaryversion == PROG_SECONDARYVERSION16)
		{
//			printf("Opening 16bit fte progs file \"%s\"\n", filename);
			current_progstate->intsize = 16;
		}
		else if (pr_progs->secondaryversion == PROG_SECONDARYVERSION32)
		{
//			printf("Opening 32bit fte progs file \"%s\"\n", filename);
			current_progstate->intsize = 32;
		}
		else
		{
//			printf("Opening KK7 progs file \"%s\"\n", filename);
			current_progstate->intsize = 24;	//KK progs. Yuck. Disabling saving would be a VERY good idea.
			pr_progs->version = PROG_VERSION;	//not fte.
		}
/*		else
		{
			printf ("Progs extensions are not compatable\nTry recompiling with the FTE compiler\n");
			HunkFree(hmark);
			pr_progs=NULL;
			return false;
		}
*/	}
	else
	{
		printf ("%s has wrong version number (%i should be %i)\n", filename, pr_progs->version, PROG_VERSION);
		PRHunkFree(progfuncs, hmark);
		pr_progs=NULL;
		return false;
	}

//progs contains enough info for use to recompile it.
	if (trysleft && (externs->autocompile == PR_COMPILECHANGED  || externs->autocompile == PR_COMPILEEXISTANDCHANGED) && pr_progs->version == PROG_EXTENDEDVERSION)
	{
		if (PR_TestRecompile(progfuncs))
		{
			printf("Source file has changed\nRecompiling.\n");
			if (CompileFile(progfuncs, filename))
			{
				PRHunkFree(progfuncs, hmark);
				pr_progs=NULL;

				trysleft--;
				goto retry;
			}
		}
	}
	if (!trysleft)	//the progs exists, let's just be happy about it.
		printf("Progs is out of date and uncompilable\n");

	if (headercrc != -1 && headercrc != 0)
	if (pr_progs->crc != headercrc && pr_progs->crc != 0)	//This shouldn't affect us. However, it does adjust expectations and usage of builtins.
	{
//		printf ("%s system vars have been modified, progdefs.h is out of date\n", filename);
		PRHunkFree(progfuncs, hmark);
		pr_progs=NULL;
		return false;
	}

	fnc = pr_functions = (dfunction_t *)((qbyte *)pr_progs + pr_progs->ofs_functions);
	pr_strings = ((char *)pr_progs + pr_progs->ofs_strings);
	current_progstate->globaldefs = *(void**)&gd16 = (void *)((qbyte *)pr_progs + pr_progs->ofs_globaldefs);
	current_progstate->fielddefs = *(void**)&fld16 = (void *)((qbyte *)pr_progs + pr_progs->ofs_fielddefs);
	current_progstate->statements = (void *)((qbyte *)pr_progs + pr_progs->ofs_statements);

	glob = pr_globals = (void *)((qbyte *)pr_progs + pr_progs->ofs_globals);

	pr_linenums=NULL;
	pr_types=NULL;
	if (pr_progs->version == PROG_EXTENDEDVERSION)
	{
		if (pr_progs->ofslinenums)
			pr_linenums = (int *)((qbyte *)pr_progs + pr_progs->ofslinenums);
		if (pr_progs->ofs_types)
			pr_types = (typeinfo_t *)((qbyte *)pr_progs + pr_progs->ofs_types);

		//start decompressing stuff...
		if (pr_progs->blockscompressed & 1)	//statements
		{
			switch(current_progstate->intsize)
			{
			case 16:
				len=sizeof(dstatement16_t)*pr_progs->numstatements;
				break;
			case 32:
				len=sizeof(dstatement32_t)*pr_progs->numstatements;
				break;
			default:
				Sys_Error("Bad intsize");
			}
			s = PRHunkAlloc(progfuncs, len);
			QC_decode(progfuncs, PRLittleLong(*(int *)pr_statements16), len, 2, (char *)(((int *)pr_statements16)+1), s);

			current_progstate->statements = (dstatement16_t *)s;
		}
		if (pr_progs->blockscompressed & 2)	//global defs
		{
			switch(current_progstate->intsize)
			{
			case 16:
				len=sizeof(ddef16_t)*pr_progs->numglobaldefs;
				break;
			case 32:
				len=sizeof(ddef32_t)*pr_progs->numglobaldefs;
				break;
			default:
				Sys_Error("Bad intsize");
			}			
			s = PRHunkAlloc(progfuncs, len);
			QC_decode(progfuncs, PRLittleLong(*(int *)pr_globaldefs16), len, 2, (char *)(((int *)pr_globaldefs16)+1), s);

			gd16 = *(ddef16_t**)&current_progstate->globaldefs = (ddef16_t *)s;
		}
		if (pr_progs->blockscompressed & 4)	//fields
		{
			switch(current_progstate->intsize)
			{
			case 16:
				len=sizeof(ddef16_t)*pr_progs->numglobaldefs;
				break;
			case 32:
				len=sizeof(ddef32_t)*pr_progs->numglobaldefs;
				break;
			default:
				Sys_Error("Bad intsize");
			}			
			s = PRHunkAlloc(progfuncs, len);
			QC_decode(progfuncs, PRLittleLong(*(int *)pr_fielddefs16), len, 2, (char *)(((int *)pr_fielddefs16)+1), s);

			*(ddef16_t**)&current_progstate->fielddefs = (ddef16_t *)s;
		}
		if (pr_progs->blockscompressed & 8)	//functions
		{
			len=sizeof(dfunction_t)*pr_progs->numfunctions;
			s = PRHunkAlloc(progfuncs, len);
			QC_decode(progfuncs, PRLittleLong(*(int *)pr_functions), len, 2, (char *)(((int *)pr_functions)+1), s);

			fnc = pr_functions = (dfunction_t *)s;
		}
		if (pr_progs->blockscompressed & 16)	//string table
		{
			len=sizeof(char)*pr_progs->numstrings;
			s = PRHunkAlloc(progfuncs, len);
			QC_decode(progfuncs, PRLittleLong(*(int *)pr_strings), len, 2, (char *)(((int *)pr_strings)+1), s);

			pr_strings = (char *)s;
		}
		if (pr_progs->blockscompressed & 32)	//globals
		{
			len=sizeof(float)*pr_progs->numglobals;
			s = PRHunkAlloc(progfuncs, len);
			QC_decode(progfuncs, PRLittleLong(*(int *)pr_globals), len, 2, (char *)(((int *)pr_globals)+1), s);

			glob = pr_globals = (float *)s;
		}
		if (pr_linenums && pr_progs->blockscompressed & 64)	//line numbers
		{
			len=sizeof(int)*pr_progs->numstatements;
			s = PRHunkAlloc(progfuncs, len);
			QC_decode(progfuncs, PRLittleLong(*(int *)pr_linenums), len, 2, (char *)(((int *)pr_linenums)+1), s);

			pr_linenums = (int *)s;			
		}
		if (pr_types && pr_progs->blockscompressed & 128)	//types
		{			
			len=sizeof(typeinfo_t)*pr_progs->numtypes;
			s = PRHunkAlloc(progfuncs, len);
			QC_decode(progfuncs, PRLittleLong(*(int *)pr_types), len, 2, (char *)(((int *)pr_types)+1), s);

			pr_types = (typeinfo_t *)s;
		}
	}

	len=sizeof(char)*pr_progs->numstrings;
	s = PRAddressableAlloc(progfuncs, len);
	memcpy(s, pr_strings, len);
	pr_strings = (char *)s;

	len=sizeof(float)*pr_progs->numglobals;
	s = PRAddressableAlloc(progfuncs, len);
	memcpy(s, pr_globals, len);
	glob = pr_globals = (float *)s;

	if (progfuncs->stringtable)
		stringadjust = pr_strings - progfuncs->stringtable;
	else
		stringadjust = 0;

	if (!pr_linenums)
	{
		unsigned int lnotype = *(unsigned int*)"LNOF";
		unsigned int version = 1;
		int ohm;
		unsigned int *file;
		char lnoname[128];
		ohm = PRHunkMark(progfuncs);

		strcpy(lnoname, filename);
		StripExtension(lnoname);
		strcat(lnoname, ".lno");
		if ((len=externs->FileSize(lnoname))>0)
		{
			file = PRHunkAlloc(progfuncs, len+1);
			if (externs->ReadFile(lnoname, file, len+1))
			{
				if (	file[0] != lnotype
					||	file[1] != version
					||	file[2] != pr_progs->numglobaldefs
					||	file[3] != pr_progs->numglobals
					||	file[4] != pr_progs->numfielddefs
					||	file[5] != pr_progs->numstatements
					)
				{
					PRHunkFree(progfuncs, ohm);	//whoops: old progs or incompatable
				}
				else
					pr_linenums = file + 6;
			}
		}
	}

	pr_functions = fnc;
//	pr_strings = ((char *)pr_progs + pr_progs->ofs_strings);
	gd16 = *(ddef16_t**)&current_progstate->globaldefs = (ddef16_t *)((qbyte *)pr_progs + pr_progs->ofs_globaldefs);
	fld16 = (ddef16_t *)((qbyte *)pr_progs + pr_progs->ofs_fielddefs);
//	pr_statements16 = (dstatement16_t *)((qbyte *)pr_progs + pr_progs->ofs_statements);
	pr_globals = glob;
	st16 = pr_statements16;
#undef pr_globals
#undef pr_globaldefs16
#undef pr_functions
#undef pr_statements16
#undef pr_fielddefs16


	current_progstate->edict_size = pr_progs->entityfields * 4 + externs->edictsize;

// byte swap the lumps
	for (i=0 ; i<pr_progs->numfunctions; i++)
	{
#ifndef NOENDIAN
		fnc[i].first_statement	= PRLittleLong (fnc[i].first_statement);
		fnc[i].parm_start	= PRLittleLong (fnc[i].parm_start);
		fnc[i].s_name	= (string_t)PRLittleLong ((long)fnc[i].s_name);
		fnc[i].s_file	= (string_t)PRLittleLong ((long)fnc[i].s_file);
		fnc[i].numparms	= PRLittleLong (fnc[i].numparms);
		fnc[i].locals	= PRLittleLong (fnc[i].locals);
#endif
/*		if (!strncmp(fnc[i].s_name+pr_strings, "ext_", 4))
		{
			for (eb = extensionbuiltin; eb; eb = eb->prev)
			{
				if (*eb->name == '_')
				{
					if (!strncmp(fnc[i].s_name+pr_strings+4, eb->name+1, strlen(eb->name+1)))
					{
						fnc[i].first_statement = -0x7fffffff;
						*(void**)&fnc[i].profile = (void*)eb->func;
						break;
					}
				}
				else if (!strcmp(fnc[i].s_name+4, eb->name))
				{
					fnc[i].first_statement = -0x7fffffff;
					*(void**)&fnc[i].profile = (void*)eb->func;
					break;
				}
			}
		}
*/
		fnc[i].s_name += stringadjust;
		fnc[i].s_file += stringadjust;
	}
	
	//actual global values
#ifndef NOENDIAN
	for (i=0 ; i<pr_progs->numglobals ; i++)
		((int *)glob)[i] = PRLittleLong (((int *)glob)[i]);
#endif	

	if (pr_types)
	{			
		for (i=0 ; i<pr_progs->numtypes ; i++)
		{	
#ifndef NOENDIAN
			pr_types[i].type = PRLittleLong(current_progstate->types[i].type);
			pr_types[i].next = PRLittleLong(current_progstate->types[i].next);
			pr_types[i].aux_type = PRLittleLong(current_progstate->types[i].aux_type);
			pr_types[i].num_parms = PRLittleLong(current_progstate->types[i].num_parms);
			pr_types[i].ofs = PRLittleLong(current_progstate->types[i].ofs);
			pr_types[i].size = PRLittleLong(current_progstate->types[i].size);
			pr_types[i].name = (string_t)PRLittleLong((long)current_progstate->types[i].name);
#endif
			pr_types[i].name += stringadjust;
		}
	}

	if (reorg)
		reorg = (headercrc != -1);

	switch(current_progstate->intsize)
	{
	case 24:
	case 16:
		//byteswap the globals and fix name offsets
		for (i=0 ; i<pr_progs->numglobaldefs ; i++)
		{
#ifndef NOENDIAN
			gd16[i].type = PRLittleShort (gd16[i].type);
			gd16[i].ofs = PRLittleShort (gd16[i].ofs);
			gd16[i].s_name = (string_t)PRLittleLong ((long)gd16[i].s_name);
#endif
			gd16[i].s_name += stringadjust;
		}

		//byteswap fields and fix name offets. Also register the fields (which will result in some offset adjustments in the globals segment).
		for (i=0 ; i<pr_progs->numfielddefs ; i++)
		{
#ifndef NOENDIAN
			fld16[i].type = PRLittleShort (fld16[i].type);
			fld16[i].ofs = PRLittleShort (fld16[i].ofs);
			fld16[i].s_name = (string_t)PRLittleLong ((long)fld16[i].s_name);
#endif
			if (reorg)
			{
				if (pr_types)
					type = pr_types[fld16[i].type & ~(DEF_SHARED|DEF_SAVEGLOBAL)].type;
				else
					type = fld16[i].type & ~(DEF_SHARED|DEF_SAVEGLOBAL);

				if (progfuncs->fieldadjust)	//we need to make sure all fields appear in thier original place.
					QC_RegisterFieldVar(progfuncs, type, fld16[i].s_name+pr_strings, 4*(fld16[i].ofs+progfuncs->fieldadjust), -1);
				else if (type == ev_vector)	//emit vector vars early, so thier fields cannot be alocated before the vector itself. (useful against scramblers)
				{
					QC_RegisterFieldVar(progfuncs, type, fld16[i].s_name+pr_strings, -1, -1);
				}
			}
			fld16[i].s_name += stringadjust;
		}
		break;
	case 32:
		for (i=0 ; i<pr_progs->numglobaldefs ; i++)
		{
#ifndef NOENDIAN
			pr_globaldefs32[i].type = PRLittleLong (pr_globaldefs32[i].type);
			pr_globaldefs32[i].ofs = PRLittleLong (pr_globaldefs32[i].ofs);
			pr_globaldefs32[i].s_name = (string_t)PRLittleLong ((long)pr_globaldefs32[i].s_name);
#endif
			pr_globaldefs32[i].s_name += stringadjust;
		}

		for (i=0 ; i<pr_progs->numfielddefs ; i++)
		{
	#ifndef NOENDIAN
			pr_fielddefs32[i].type = PRLittleLong (pr_fielddefs32[i].type);
			pr_fielddefs32[i].ofs = PRLittleLong (pr_fielddefs32[i].ofs);
			pr_fielddefs32[i].s_name = (string_t)PRLittleLong ((long)pr_fielddefs32[i].s_name);
	#endif

			if (reorg)
			{
				if (pr_types)
					type = pr_types[pr_fielddefs32[i].type & ~(DEF_SHARED|DEF_SAVEGLOBAL)].type;
				else
					type = pr_fielddefs32[i].type & ~(DEF_SHARED|DEF_SAVEGLOBAL);
				if (type == ev_vector)
					QC_RegisterFieldVar(progfuncs, type, pr_fielddefs32[i].s_name+pr_strings, -1, -1);
			}
			pr_fielddefs32[i].s_name += stringadjust;
		}	
		break;
	default:
		Sys_Error("Bad int size");
	}

//ifstring fixes arn't performed anymore. 
//the following switch just fixes endian and hexen2 calling conventions (by using different opcodes).
	switch(current_progstate->intsize)
	{
	case 16:
		for (i=0 ; i<pr_progs->numstatements ; i++)
		{
#ifndef NOENDIAN
			st16[i].op = PRLittleShort(st16[i].op);
			st16[i].a = PRLittleShort(st16[i].a);
			st16[i].b = PRLittleShort(st16[i].b);
			st16[i].c = PRLittleShort(st16[i].c);
#endif
			if (st16[i].op >= OP_CALL1 && st16[i].op <= OP_CALL8)
			{
				if (st16[i].b)
					hexencalling = true;

			}
		}
		if (hexencalling)
		{
			for (i=0 ; i<pr_progs->numstatements ; i++)
			{
				if (st16[i].op >= OP_CALL1 && st16[i].op <= OP_CALL8)
					st16[i].op += OP_CALL1H - OP_CALL1;
			}
		}
		break;
	
	case 24:	//24 sucks. Guess why.
		for (i=0 ; i<pr_progs->numstatements ; i++)
		{
#ifndef NOENDIAN
			pr_statements32[i].op = PRLittleLong(pr_statements32[i].op);
			pr_statements32[i].a = PRLittleLong(pr_statements32[i].a);
			pr_statements32[i].b = PRLittleLong(pr_statements32[i].b);
			pr_statements32[i].c = PRLittleLong(pr_statements32[i].c);
#endif
			if (pr_statements32[i].op >= OP_CALL1 && pr_statements32[i].op <= OP_CALL8)
			{
				if (pr_statements32[i].b)
					hexencalling = true;

			}
		}
		if (hexencalling)
		{
			for (i=0 ; i<pr_progs->numstatements ; i++)
			{
				if (pr_statements32[i].op >= OP_CALL1 && pr_statements32[i].op <= OP_CALL8)
					pr_statements32[i].op += OP_CALL1H - OP_CALL1;
			}
		}
		break;
	case 32:
		for (i=0 ; i<pr_progs->numstatements ; i++)
		{
#ifndef NOENDIAN
			pr_statements32[i].op = PRLittleLong(pr_statements32[i].op);
			pr_statements32[i].a = PRLittleLong(pr_statements32[i].a);
			pr_statements32[i].b = PRLittleLong(pr_statements32[i].b);
			pr_statements32[i].c = PRLittleLong(pr_statements32[i].c);
#endif
			if (pr_statements32[i].op >= OP_CALL1 && pr_statements32[i].op <= OP_CALL8)
			{
				if (pr_statements32[i].b)
					hexencalling = true;

			}
		}
		if (hexencalling)
		{
			for (i=0 ; i<pr_progs->numstatements ; i++)
			{
				if (pr_statements32[i].op >= OP_CALL1 && pr_statements32[i].op <= OP_CALL8)
					pr_statements32[i].op += OP_CALL1H - OP_CALL1;
			}
		}
		break;
	}


	if (headercrc == -1)
	{
		isfriked = true;
		if (current_progstate->intsize != 16)
			Sys_Error("Decompiling a bigprogs");
		return true;
	}

	progstype = current_progstate-pr_progstate;

//	QC_StartShares(progfuncs);

	isfriked = true;
	if (!pr_typecurrent)	//progs 0 always acts as string stripped.
		isfriked = -1;		//partly to avoid some bad progs.

//	len = 0;
	switch(current_progstate->intsize)
	{
	case 24:
	case 16:
		for (i=0 ; i<pr_progs->numglobaldefs ; i++)
		{
			if (pr_types)
				type = pr_types[gd16[i].type & ~(DEF_SHARED|DEF_SAVEGLOBAL)].type;
			else
				type = gd16[i].type & ~(DEF_SHARED|DEF_SAVEGLOBAL);

			if (gd16[i].type & DEF_SHARED)
			{
				gd16[i].type &= ~DEF_SHARED;
				if (pr_types)
					QC_AddSharedVar(progfuncs, gd16[i].ofs, pr_types[gd16[i].type & ~(DEF_SHARED|DEF_SAVEGLOBAL)].size);
				else
					QC_AddSharedVar(progfuncs, gd16[i].ofs, type_size[type]);
			}
			switch(type)
			{
			case ev_field:
				QC_AddSharedFieldVar(progfuncs, i, pr_strings - stringadjust);
				break;
			case ev_string:
				if (((unsigned int *)glob)[gd16[i].ofs]>=progstate->progs->numstrings)
					printf("Insane value\n");
				else if (isfriked != -1)
				{
					if (pr_strings[((int *)glob)[gd16[i].ofs]])	//quakec uses string tables. 0 must remain null, or 'if (s)' can break.
					{
						((int *)glob)[gd16[i].ofs] += stringadjust;
						isfriked = false;
					}
					else
						((int *)glob)[gd16[i].ofs] = 0;
				}
				break;
			case ev_function:
				if (((int *)glob)[gd16[i].ofs])	//don't change null funcs
				{
//					if (fnc[((int *)glob)[gd16[i].ofs]].first_statement>=0)	//this is a hack. Make all builtins switch to the main progs first. Allows builtin funcs to cache vars from just the main progs.
						((int *)glob)[gd16[i].ofs] |= progstype << 24;
				}
				break;
			}
		}

		if (pr_progs->version == PROG_EXTENDEDVERSION && pr_progs->numbodylessfuncs)
		{
			s = &((char *)pr_progs)[pr_progs->ofsbodylessfuncs];
			for (i = 0; i < pr_progs->numbodylessfuncs; i++)
			{
				d16 = ED_FindGlobal16(progfuncs, s);
				if (!d16)
					Sys_Error("Progs requires \"%s\" the external function \"%s\", but the definition was stripped", filename, s);

				((int *)glob)[d16->ofs] = PR_FindFunc(progfuncs, s, PR_ANY);
				if (!((int *)glob)[d16->ofs])
					Sys_Error("Runtime-linked function %s was not found in primary progs (loading %s)", s, filename);
				/*
				d2 = ED_FindGlobalOfsFromProgs(progfuncs, s, 0, ev_function);
				if (!d2)
					Sys_Error("Runtime-linked function %s was not found in primary progs (loading %s)", s, filename);
				((int *)glob)[d16->ofs] = (*(func_t *)&pr_progstate[0].globals[*d2]);
				*/
				s+=strlen(s)+1;
			}
		}
		break;
	case 32:
		for (i=0 ; i<pr_progs->numglobaldefs ; i++)
		{
			if (pr_types)
				type = pr_types[pr_globaldefs32[i].type & ~(DEF_SHARED|DEF_SAVEGLOBAL)].type;
			else
				type = pr_globaldefs32[i].type & ~(DEF_SHARED|DEF_SAVEGLOBAL);

			if (pr_globaldefs32[i].type & DEF_SHARED)
			{
				pr_globaldefs32[i].type &= ~DEF_SHARED;
				if (pr_types)
					QC_AddSharedVar(progfuncs, pr_globaldefs32[i].ofs, pr_types[pr_globaldefs32[i].type & ~(DEF_SHARED|DEF_SAVEGLOBAL)].size);
				else
					QC_AddSharedVar(progfuncs, pr_globaldefs32[i].ofs, type_size[type]);
			}
			switch(type)
			{
			case ev_field:
				QC_AddSharedFieldVar(progfuncs, i, pr_strings - stringadjust);
				break;
			case ev_string:
				if (pr_strings[((int *)glob)[pr_globaldefs32[i].ofs]])	//quakec uses string tables. 0 must remain null, or 'if (s)' can break.
				{
					((int *)glob)[pr_globaldefs32[i].ofs] += stringadjust;
					isfriked = false;
				}
				break;
			case ev_function:
				if (((int *)glob)[pr_globaldefs32[i].ofs])	//don't change null funcs
					((int *)glob)[pr_globaldefs32[i].ofs] |= progstype << 24;
				break;
			}
		}

		if (pr_progs->version == PROG_EXTENDEDVERSION && pr_progs->numbodylessfuncs)
		{
			s = &((char *)pr_progs)[pr_progs->ofsbodylessfuncs];
			for (i = 0; i < pr_progs->numbodylessfuncs; i++)
			{
				d32 = ED_FindGlobal32(progfuncs, s);
				d2 = ED_FindGlobalOfsFromProgs(progfuncs, s, 0, ev_function);
				if (!d2)
					Sys_Error("Runtime-linked function %s was not found in existing progs", s);
				if (!d32)
					Sys_Error("Couldn't find def for \"%s\"", s);
				((int *)glob)[d32->ofs] = (*(func_t *)&pr_progstate[0].globals[*d2]);

				s+=strlen(s)+1;
			}
		}
		break;
	default:
		Sys_Error("Bad int size");
	}

	if ((isfriked && pr_typecurrent))	//friked progs only allow one file.
	{
		printf("You are trying to load a string-stripped progs as an addon.\nThis behaviour is not supported. Try removing some optimizations.");
		PRHunkFree(progfuncs, hmark);
		pr_progs=NULL;
		return false;
	}

	pr_strings+=stringadjust;
	if (!progfuncs->stringtable)
		progfuncs->stringtable = pr_strings;

	eval = PR_FindGlobal(progfuncs, "thisprogs", progstype);		
	if (eval)
		eval->prog = progstype;


	return true;
}



struct edict_s *EDICT_NUM(progfuncs_t *progfuncs, int n)
{
	if (n < 0 || n >= maxedicts)
		Sys_Error ("QCLIB: EDICT_NUM: bad number %i", n);

	return prinst->edicttable[n];
}

int NUM_FOR_EDICT(progfuncs_t *progfuncs, struct edict_s *e)
{
	edictrun_t *er = (edictrun_t*)e;
	if (er->entnum < 0 || er->entnum >= maxedicts)
		Sys_Error ("QCLIB: NUM_FOR_EDICT: bad pointer (%i)", e);
	return er->entnum;
}
