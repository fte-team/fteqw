//qc execution code.
//we have two conditions.
//one allows us to debug and trace through our code, the other doesn't.

//hopefully, the compiler will do a great job at optimising this code for us, where required.
//if it dosn't, then bum.

//the general overhead should be reduced significantly, and I would be supprised if it did run slower.

//run away loops are checked for ONLY on gotos and function calls. This might give a poorer check, but it will run faster overall.

//Appears to work fine.

#if INTSIZE == 16
#define cont cont16
#define reeval reeval16
#define pr_statements pr_statements16
#define fakeop fakeop16
#define dstatement_t dstatement16_t
#define sofs signed short
#elif INTSIZE == 32
#define cont cont32
#define reeval reeval32
#define pr_statements pr_statements32
#define fakeop fakeop32
#define dstatement_t dstatement32_t
#define sofs signed int
#elif INTSIZE == 24
#error INTSIZE should be set to 32.
#else
#error Bad cont size
#endif

#define ENGINEPOINTER(p) ((char*)(p) - progfuncs->funcs.stringtable)
#define QCPOINTER(p) (eval_t *)(p->_int+progfuncs->funcs.stringtable)
#define QCPOINTERM(p) (eval_t *)((p)+progfuncs->funcs.stringtable)
#define QCPOINTERWRITEFAIL(p,sz) ((unsigned int)p->_int-1 >= prinst.addressableused-1-sz)	//disallows null writes
#define QCPOINTERREADFAIL(p,sz) ((unsigned int)p->_int >= prinst.addressableused-sz)		//permits null reads



#define QCFAULT return (pr_xstatement=(st-pr_statements)-1),PR_HandleFault

//rely upon just st
{
#ifdef DEBUGABLE
cont:	//last statement may have been a breakpoint
	s = st-pr_statements;
	s+=1;

	if (prinst.watch_ptr && prinst.watch_ptr->_int != prinst.watch_old._int)
	{
		//this will fire on the next instruction after the variable got changed.
		pr_xstatement = s;
		switch(prinst.watch_type)
		{
		case ev_float:
			printf("Watch point hit in %s, \"%s\" changed from %g to %g.\n", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name), prinst.watch_name, prinst.watch_old._float, prinst.watch_ptr->_float);
			break;
		case ev_vector:
			printf("Watch point hit in %s, \"%s\" changed from '%g %g %g' to '%g %g %g'.\n", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name), prinst.watch_name, prinst.watch_old._vector[0], prinst.watch_old._vector[1], prinst.watch_old._vector[2], prinst.watch_ptr->_vector[0], prinst.watch_ptr->_vector[1], prinst.watch_ptr->_vector[2]);
			break;
		default:
			printf("Watch point hit in %s, \"%s\" changed from %i to %i.\n", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name), prinst.watch_name, prinst.watch_old._int, prinst.watch_ptr->_int);
			break;
		case ev_entity:
			printf("Watch point hit in %s, \"%s\" changed from %i(%s) to %i(%s).\n", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name), prinst.watch_name, prinst.watch_old._int, PR_GetEdictClassname(progfuncs, prinst.watch_old._int), prinst.watch_ptr->_int, PR_GetEdictClassname(progfuncs, prinst.watch_ptr->_int));
			break;
		case ev_function:
		case ev_string:
			printf("Watch point hit in %s, \"%s\" now set to %s.\n", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name), prinst.watch_name, PR_ValueString(progfuncs, prinst.watch_type, prinst.watch_ptr, false));
			break;
		}
		prinst.watch_old = *prinst.watch_ptr;
//		prinst.watch_ptr = NULL;
		progfuncs->funcs.debug_trace=DEBUG_TRACE_INTO;	//this is what it's for

		s=ShowStep(progfuncs, s, "Watchpoint hit");
	}
	else if (progfuncs->funcs.debug_trace)
		s=ShowStep(progfuncs, s, NULL);
	st = pr_statements + s;
	pr_xfunction->profile+=1;

	op = (progfuncs->funcs.debug_trace?(st->op & ~0x8000):st->op);
reeval:
#else
	st++;
	op = st->op;
#endif

	switch (op)
	{
	case OP_ADD_F:
		OPC->_float = OPA->_float + OPB->_float;
		break;
	case OP_ADD_V:
		OPC->_vector[0] = OPA->_vector[0] + OPB->_vector[0];
		OPC->_vector[1] = OPA->_vector[1] + OPB->_vector[1];
		OPC->_vector[2] = OPA->_vector[2] + OPB->_vector[2];
		break;

	case OP_SUB_F:
		OPC->_float = OPA->_float - OPB->_float;
		break;
	case OP_SUB_V:
		OPC->_vector[0] = OPA->_vector[0] - OPB->_vector[0];
		OPC->_vector[1] = OPA->_vector[1] - OPB->_vector[1];
		OPC->_vector[2] = OPA->_vector[2] - OPB->_vector[2];
		break;

	case OP_MUL_F:
		OPC->_float = OPA->_float * OPB->_float;
		break;
	case OP_MUL_V:
		OPC->_float = OPA->_vector[0]*OPB->_vector[0]
				+ OPA->_vector[1]*OPB->_vector[1]
				+ OPA->_vector[2]*OPB->_vector[2];
		break;
	case OP_MUL_FV:
		tmpf = OPA->_float;
		OPC->_vector[0] = tmpf * OPB->_vector[0];
		OPC->_vector[1] = tmpf * OPB->_vector[1];
		OPC->_vector[2] = tmpf * OPB->_vector[2];
		break;
	case OP_MUL_VF:
		tmpf = OPB->_float;
		OPC->_vector[0] = tmpf * OPA->_vector[0];
		OPC->_vector[1] = tmpf * OPA->_vector[1];
		OPC->_vector[2] = tmpf * OPA->_vector[2];
		break;

	case OP_DIV_F:
/*		if (!OPB->_float)
		{
			pr_xstatement = st-pr_statements;
			printf ("Division by 0 in %s\n", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name));
			PR_StackTrace (&progfuncs->funcs);
		}
*/		OPC->_float = OPA->_float / OPB->_float;
		break;
	case OP_DIV_VF:
		tmpf = OPB->_float;
/*		if (!tmpf)
		{
			pr_xstatement = st-pr_statements;
			printf ("Division by 0 in %s\n", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name));
			PR_StackTrace (&progfuncs->funcs);
		}
*/
		OPC->_vector[0] = OPA->_vector[0] / tmpf;
		OPC->_vector[1] = OPA->_vector[1] / tmpf;
		OPC->_vector[2] = OPA->_vector[2] / tmpf;
		break;

	case OP_BITAND_F:
		OPC->_float = (float)((int)OPA->_float & (int)OPB->_float);
		break;

	case OP_BITOR_F:
		OPC->_float = (float)((int)OPA->_float | (int)OPB->_float);
		break;


	case OP_GE_F:
		OPC->_float = (float)(OPA->_float >= OPB->_float);
		break;
	case OP_GE_I:
		OPC->_int = (int)(OPA->_int >= OPB->_int);
		break;
	case OP_GE_IF:
		OPC->_float = (float)(OPA->_int >= OPB->_float);
		break;
	case OP_GE_FI:
		OPC->_float = (float)(OPA->_float >= OPB->_int);
		break;

	case OP_LE_F:
		OPC->_float = (float)(OPA->_float <= OPB->_float);
		break;
	case OP_LE_I:
		OPC->_int = (int)(OPA->_int <= OPB->_int);
		break;
	case OP_LE_IF:
		OPC->_float = (float)(OPA->_int <= OPB->_float);
		break;
	case OP_LE_FI:
		OPC->_float = (float)(OPA->_float <= OPB->_int);
		break;

	case OP_GT_F:
		OPC->_float = (float)(OPA->_float > OPB->_float);
		break;
	case OP_GT_I:
		OPC->_int = (int)(OPA->_int > OPB->_int);
		break;
	case OP_GT_IF:
		OPC->_float = (float)(OPA->_int > OPB->_float);
		break;
	case OP_GT_FI:
		OPC->_float = (float)(OPA->_float > OPB->_int);
		break;

	case OP_LT_F:
		OPC->_float = (float)(OPA->_float < OPB->_float);
		break;
	case OP_LT_I:
		OPC->_int = (int)(OPA->_int < OPB->_int);
		break;
	case OP_LT_IF:
		OPC->_float = (float)(OPA->_int < OPB->_float);
		break;
	case OP_LT_FI:
		OPC->_float = (float)(OPA->_float < OPB->_int);
		break;

	case OP_AND_F:
		OPC->_float = (float)(OPA->_float && OPB->_float);
		break;
	case OP_OR_F:
		OPC->_float = (float)(OPA->_float || OPB->_float);
		break;

	case OP_NOT_F:
		OPC->_float = (float)(!OPA->_float);
		break;
	case OP_NOT_V:
		OPC->_float = (float)(!OPA->_vector[0] && !OPA->_vector[1] && !OPA->_vector[2]);
		break;
	case OP_NOT_S:
		OPC->_float = (float)(!(OPA->string) || !*PR_StringToNative(&progfuncs->funcs, OPA->string));
		break;
	case OP_NOT_FNC:
		OPC->_float = (float)(!(OPA->function & ~0xff000000));
		break;
	case OP_NOT_ENT:
		OPC->_float = (float)(PROG_TO_EDICT(progfuncs, OPA->edict) == (edictrun_t *)sv_edicts);
		break;

	case OP_EQ_F:
		OPC->_float = (float)(OPA->_float == OPB->_float);
		break;
	case OP_EQ_IF:
		OPC->_float = (float)(OPA->_int == OPB->_float);
		break;
	case OP_EQ_FI:
		OPC->_float = (float)(OPA->_float == OPB->_int);
		break;


	case OP_EQ_V:
		OPC->_float = (float)((OPA->_vector[0] == OPB->_vector[0]) &&
					(OPA->_vector[1] == OPB->_vector[1]) &&
					(OPA->_vector[2] == OPB->_vector[2]));
		break;
	case OP_EQ_S:
		if (OPA->string==OPB->string)
			OPC->_float = true;
		else if (!OPA->string)
		{
			if (!OPB->string || !*PR_StringToNative(&progfuncs->funcs, OPB->string))
				OPC->_float = true;
			else
				OPC->_float = false;
		}
		else if (!OPB->string)
		{
			if (!OPA->string || !*PR_StringToNative(&progfuncs->funcs, OPA->string))
				OPC->_float = true;
			else
				OPC->_float = false;
		}
		else
			OPC->_float = (float)(!strcmp(PR_StringToNative(&progfuncs->funcs, OPA->string),PR_StringToNative(&progfuncs->funcs, OPB->string)));
		break;
	case OP_EQ_E:
		OPC->_float = (float)(OPA->_int == OPB->_int);
		break;
	case OP_EQ_FNC:
		OPC->_float = (float)(OPA->function == OPB->function);
		break;


	case OP_NE_F:
		OPC->_float = (float)(OPA->_float != OPB->_float);
		break;
	case OP_NE_V:
		OPC->_float = (float)((OPA->_vector[0] != OPB->_vector[0]) ||
					(OPA->_vector[1] != OPB->_vector[1]) ||
					(OPA->_vector[2] != OPB->_vector[2]));
		break;
	case OP_NE_S:
		if (OPA->string==OPB->string)
			OPC->_float = false;
		else if (!OPA->string)
		{
			if (!OPB->string || !*(PR_StringToNative(&progfuncs->funcs, OPB->string)))
				OPC->_float = false;
			else
				OPC->_float = true;
		}
		else if (!OPB->string)
		{
			if (!OPA->string || !*PR_StringToNative(&progfuncs->funcs, OPA->string))
				OPC->_float = false;
			else
				OPC->_float = true;
		}
		else
			OPC->_float = (float)(strcmp(PR_StringToNative(&progfuncs->funcs, OPA->string),PR_StringToNative(&progfuncs->funcs, OPB->string)));		
		break;
	case OP_NE_E:
		OPC->_float = (float)(OPA->_int != OPB->_int);
		break;
	case OP_NE_FNC:
		OPC->_float = (float)(OPA->function != OPB->function);
		break;

//==================
	case OP_STORE_IF:
		OPB->_float = (float)OPA->_int;
		break;
	case OP_STORE_FI:
		OPB->_int = (int)OPA->_float;
		break;
		
	case OP_STORE_F:
	case OP_STORE_ENT:
	case OP_STORE_FLD:		// integers
	case OP_STORE_S:
	case OP_STORE_I:
	case OP_STORE_FNC:		// pointers
	case OP_STORE_P:
		OPB->_int = OPA->_int;
		break;
	case OP_STORE_V:
		OPB->_vector[0] = OPA->_vector[0];
		OPB->_vector[1] = OPA->_vector[1];
		OPB->_vector[2] = OPA->_vector[2];
		break;

	//store a value to a pointer
	case OP_STOREP_IF:
		if (QCPOINTERWRITEFAIL(OPB, sizeof(float)))
		{
			if (OPB->_int == -1)
				break;
			QCFAULT(&progfuncs->funcs, "bad pointer write in %s", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name));
		}
		ptr = QCPOINTER(OPB);
		ptr->_float = (float)OPA->_int;
		break;
	case OP_STOREP_FI:
		if (QCPOINTERWRITEFAIL(OPB, sizeof(int)))
		{
			if (OPB->_int == -1)
				break;
			QCFAULT(&progfuncs->funcs, "bad pointer write in %s", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name));
		}
		ptr = QCPOINTER(OPB);
		ptr->_int = (int)OPA->_float;
		break;
	case OP_STOREP_I:
	case OP_STOREP_F:
	case OP_STOREP_ENT:
	case OP_STOREP_FLD:		// integers
	case OP_STOREP_S:
	case OP_STOREP_FNC:		// pointers
		if (QCPOINTERWRITEFAIL(OPB, sizeof(int)))
		{
			if (OPB->_int == -1)
				break;
			if (OPB->_int == 0)
				QCFAULT(&progfuncs->funcs, "bad pointer write in %s (null pointer)", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name));
			else
				QCFAULT(&progfuncs->funcs, "bad pointer write in %s (%x >= %x)", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name), OPB->_int, prinst.addressableused);
		}
		ptr = QCPOINTER(OPB);
		ptr->_int = OPA->_int;
		break;
	case OP_STOREP_V:
		if (QCPOINTERWRITEFAIL(OPB, sizeof(vec3_t)))
		{
			if (OPB->_int == -1)
				break;
			QCFAULT(&progfuncs->funcs, "bad pointer write in %s (%x >= %x)", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name), OPB->_int, prinst.addressableused);
		}
		ptr = QCPOINTER(OPB);
		ptr->_vector[0] = OPA->_vector[0];
		ptr->_vector[1] = OPA->_vector[1];
		ptr->_vector[2] = OPA->_vector[2];
		break;

	case OP_STOREP_C:	//store character in a string
		if (QCPOINTERWRITEFAIL(OPB, sizeof(char)))
		{
			QCFAULT(&progfuncs->funcs, "bad pointer write in %s (%x >= %x)", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name), OPB->_int, prinst.addressableused);
		}
		ptr = QCPOINTER(OPB);
		*(unsigned char *)ptr = (char)OPA->_float;
		break;

	//get a pointer to a field var
	case OP_ADDRESS:
		if ((unsigned)OPA->edict >= (unsigned)sv_num_edicts)
		{
			pr_xstatement = st-pr_statements;
			if (PR_RunWarning (&progfuncs->funcs, "OP_ADDRESS references invalid entity in %s\n", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name)))
			{
				st--;
				goto cont;
			}
			break;
		}
		ed = PROG_TO_EDICT(progfuncs, OPA->edict);
#ifdef PARANOID
		NUM_FOR_EDICT(ed);		// make sure it's in range
#endif
		if (!ed || ed->readonly)
		{

			//boot it over to the debugger
			{
				ddef16_t *d16;
				fdef_t *f;
				d16 = ED_GlobalAtOfs16(progfuncs, st->a);
				f = ED_FieldAtOfs(progfuncs, OPB->_int + progfuncs->funcs.fieldadjust);
				pr_xstatement = st-pr_statements;
				if (PR_RunWarning(&progfuncs->funcs, "assignment to read-only entity %i in %s (%s.%s)\n", OPA->edict, PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name), d16?PR_StringToNative(&progfuncs->funcs, d16->s_name):NULL, f?f->name:NULL))
				{
					st--;
					goto cont;
				}
				OPC->_int = ~0;
				break;
			}
		}

//Whilst the next block would technically be correct, we don't use it as it breaks too many quake mods.
//		if (ed->isfree)
//		{
//			pr_xstatement = st-pr_statements;
//			PR_RunError (progfuncs, "assignment to free entitiy in %s", PR_StringToNative(progfuncs, pr_xfunction->s_name));
//		}
		OPC->_int = ENGINEPOINTER((((int *)edvars(ed)) + OPB->_int + progfuncs->funcs.fieldadjust));
		break;

	//load a field to a value
	case OP_LOAD_P:
	case OP_LOAD_I:
	case OP_LOAD_F:
	case OP_LOAD_FLD:
	case OP_LOAD_ENT:
	case OP_LOAD_S:
	case OP_LOAD_FNC:
		if ((unsigned)OPA->edict >= (unsigned)sv_num_edicts)
		{
			pr_xstatement = st-pr_statements;
			if (PR_RunWarning (&progfuncs->funcs, "OP_LOAD references invalid entity %i in %s\n", OPA->edict, PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name)))
			{
				st--;
				goto cont;
			}
			OPC->_int = 0;
			break;
		}
		ed = PROG_TO_EDICT(progfuncs, OPA->edict);
#ifdef PARANOID
		NUM_FOR_EDICT(ed);		// make sure it's in range
#endif
		ptr = (eval_t *)(((int *)edvars(ed)) + OPB->_int + progfuncs->funcs.fieldadjust);
		OPC->_int = ptr->_int;
		break;

	case OP_LOAD_V:
		if ((unsigned)OPA->edict >= (unsigned)sv_num_edicts)
		{
			pr_xstatement = st-pr_statements;
			if (PR_RunWarning (&progfuncs->funcs, "OP_LOAD_V references invalid entity %i in %s\n", OPA->edict, PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name)))
			{
				st--;
				goto cont;
			}
			OPC->_vector[0] = 0;
			OPC->_vector[1] = 0;
			OPC->_vector[2] = 0;
			break;
		}
		ed = PROG_TO_EDICT(progfuncs, OPA->edict);
#ifdef PARANOID
		NUM_FOR_EDICT(ed);		// make sure it's in range
#endif
		ptr = (eval_t *)(((int *)edvars(ed)) + OPB->_int + progfuncs->funcs.fieldadjust);
		OPC->_vector[0] = ptr->_vector[0];
		OPC->_vector[1] = ptr->_vector[1];
		OPC->_vector[2] = ptr->_vector[2];
		break;	
		
//==================

	case OP_IFNOT_S:
		RUNAWAYCHECK();
		if (!OPA->string || !PR_StringToNative(&progfuncs->funcs, OPA->string))
			st += (sofs)st->b - 1;	// offset the s++
		break;

	case OP_IFNOT_F:
		RUNAWAYCHECK();
		if (!OPA->_float)
			st += (sofs)st->b - 1;	// offset the s++
		break;

	case OP_IFNOT_I:
		RUNAWAYCHECK();
		if (!OPA->_int)
			st += (sofs)st->b - 1;	// offset the s++
		break;

	case OP_IF_S:
		RUNAWAYCHECK();
		if (OPA->string && PR_StringToNative(&progfuncs->funcs, OPA->string))
			st += (sofs)st->b - 1;	// offset the s++
		break;

	case OP_IF_F:
		RUNAWAYCHECK();
		if (OPA->_float)
			st += (sofs)st->b - 1;	// offset the s++
		break;

	case OP_IF_I:
		RUNAWAYCHECK();
		if (OPA->_int)
			st += (sofs)st->b - 1;	// offset the s++
		break;
		
	case OP_GOTO:
		RUNAWAYCHECK();
		st += (sofs)st->a - 1;	// offset the s++
		break;

	case OP_CALL8H:
	case OP_CALL7H:
	case OP_CALL6H:
	case OP_CALL5H:
	case OP_CALL4H:
	case OP_CALL3H:
	case OP_CALL2H:
		G_VECTOR(OFS_PARM1)[0] = OPC->_vector[0];
		G_VECTOR(OFS_PARM1)[1] = OPC->_vector[1];
		G_VECTOR(OFS_PARM1)[2] = OPC->_vector[2];
	case OP_CALL1H:
		G_VECTOR(OFS_PARM0)[0] = OPB->_vector[0];
		G_VECTOR(OFS_PARM0)[1] = OPB->_vector[1];
		G_VECTOR(OFS_PARM0)[2] = OPB->_vector[2];

	case OP_CALL8:
	case OP_CALL7:
	case OP_CALL6:
	case OP_CALL5:
	case OP_CALL4:
	case OP_CALL3:
	case OP_CALL2:
	case OP_CALL1:
	case OP_CALL0:
		{
			int callerprogs;
			int newpr;
			unsigned int fnum;
			RUNAWAYCHECK();
			pr_xstatement = st-pr_statements;

			if (op > OP_CALL8)
				progfuncs->funcs.callargc = op - (OP_CALL1H-1);
			else
				progfuncs->funcs.callargc = op - OP_CALL0;
			fnum = OPA->function;

			glob = NULL;	//try to derestrict it.

			callerprogs=pr_typecurrent;			//so we can revert to the right caller.
			newpr = (fnum & 0xff000000)>>24;	//this is the progs index of the callee
			fnum &= ~0xff000000;				//the callee's function index.

			//if it's an external call, switch now (before any function pointers are used)
			if (!PR_SwitchProgsParms(progfuncs, newpr) || !fnum || fnum > pr_progs->numfunctions)
			{
				char *msg = fnum?"OP_CALL references invalid function in %s\n":"NULL function from qc (inside %s).\n";
				PR_SwitchProgsParms(progfuncs, callerprogs);

				glob = pr_globals;
				if (!progfuncs->funcs.debug_trace)
					QCFAULT(&progfuncs->funcs, msg, PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name));

				//skip the instruction if they just try stepping over it anyway.
				PR_StackTrace(&progfuncs->funcs, 0);
				printf(msg, PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name));

				pr_globals[OFS_RETURN] = 0;
				pr_globals[OFS_RETURN+1] = 0;
				pr_globals[OFS_RETURN+2] = 0;
				break;
			}

			newf = &pr_cp_functions[fnum & ~0xff000000];

			if (newf->first_statement <= 0)
			{	// negative statements are built in functions
				/*calling a builtin in another progs may affect that other progs' globals instead, is the theory anyway, so args and stuff need to move over*/
				if (pr_typecurrent != 0)
				{
					//builtins quite hackily refer to only a single global.
					//for builtins to affect the globals of other progs, we need to first switch to the progs that it will affect, so they'll be correct when we switch back
					PR_SwitchProgsParms(progfuncs, 0);
				}
				i = -newf->first_statement;
	//			p = pr_typecurrent;
				if (i < externs->numglobalbuiltins)
				{
#ifndef QCGC
					prinst.numtempstringsstack = prinst.numtempstrings;
#endif
					(*externs->globalbuiltins[i]) (&progfuncs->funcs, (struct globalvars_s *)current_progstate->globals);
					if (prinst.continuestatement!=-1)
					{
						st=&pr_statements[prinst.continuestatement];
						prinst.continuestatement=-1;
						glob = pr_globals;
						break;
					}
				}
				else
				{
//					if (newf->first_statement == -0x7fffffff)
//						((builtin_t)newf->profile) (progfuncs, (struct globalvars_s *)current_progstate->globals);
//					else
						PR_RunError (&progfuncs->funcs, "Bad builtin call number - %i", -newf->first_statement);
				}
	//			memcpy(&pr_progstate[p].globals[OFS_RETURN], &current_progstate->globals[OFS_RETURN], sizeof(vec3_t));
				PR_SwitchProgsParms(progfuncs, (progsnum_t)callerprogs);

				//decide weather non debugger wants to start debugging.
				s = st-pr_statements;
				return s;
			}
	//		PR_SwitchProgsParms((OPA->function & 0xff000000)>>24);
			s = PR_EnterFunction (progfuncs, newf, callerprogs);
			st = &pr_statements[s];
		}
		
		//resume at the new statement, which might be in a different progs
		return s;
//		break;

	case OP_DONE:
	case OP_RETURN:

		RUNAWAYCHECK();

		glob[OFS_RETURN] = glob[st->a];
		glob[OFS_RETURN+1] = glob[st->a+1];
		glob[OFS_RETURN+2] = glob[st->a+2];
/*
{
	static char buffer[1024*1024*8];
	int size = sizeof buffer;
		progfuncs->save_ents(progfuncs, buffer, &size, 0);
}
*/
		s = PR_LeaveFunction (progfuncs);
		st = &pr_statements[s];		
		if (pr_depth == prinst.exitdepth)
		{		
			return -1;		// all done
		}
		return s;
//		break;

	case OP_STATE:
		externs->stateop(&progfuncs->funcs, OPA->_float, OPB->function);
		break;

	case OP_ADD_I:		
		OPC->_int = OPA->_int + OPB->_int;
		break;
	case OP_ADD_FI:
		OPC->_float = OPA->_float + (float)OPB->_int;
		break;
	case OP_ADD_IF:
		OPC->_float = (float)OPA->_int + OPB->_float;
		break;
  
	case OP_SUB_I:
		OPC->_int = OPA->_int - OPB->_int;
		break;
	case OP_SUB_FI:
		OPC->_float = OPA->_float - (float)OPB->_int;
		break;
	case OP_SUB_IF:
		OPC->_float = (float)OPA->_int - OPB->_float;
		break;

	case OP_CONV_ITOF:
		OPC->_float = (float)OPA->_int;
		break;
	case OP_CONV_FTOI:
		OPC->_int = (int)OPA->_float;
		break;

	case OP_CP_ITOF:
		ptr = (eval_t *)(((qbyte *)sv_edicts) + OPA->_int);
		OPC->_float = (float)ptr->_int;
		break;

	case OP_CP_FTOI:
		ptr = (eval_t *)(((qbyte *)sv_edicts) + OPA->_int);
		OPC->_int = (int)ptr->_float;
		break;

	case OP_BITAND_I:
		OPC->_int = (OPA->_int & OPB->_int);
		break;
	
	case OP_BITOR_I:
		OPC->_int = (OPA->_int | OPB->_int);
		break;

	case OP_MUL_I:		
		OPC->_int = OPA->_int * OPB->_int;
		break;
	case OP_DIV_I:
		if (OPB->_int == 0)	//no division by zero allowed...
			OPC->_int = 0;
		else
			OPC->_int = OPA->_int / OPB->_int;
		break;
	case OP_EQ_I:
		OPC->_int = (OPA->_int == OPB->_int);
		break;
	case OP_NE_I:
		OPC->_int = (OPA->_int != OPB->_int);
		break;
	

	//array/structure reading/writing.
	case OP_GLOBALADDRESS:
		OPC->_int = ENGINEPOINTER(&OPA->_int + OPB->_int); /*pointer arithmatic*/
		break;
	case OP_ADD_PIW:	//pointer to 32 bit (remember to *3 for vectors)
		OPC->_int = OPA->_int + OPB->_int*sizeof(float);
		break;

	case OP_LOADA_I:
	case OP_LOADA_F:
	case OP_LOADA_FLD:
	case OP_LOADA_ENT:
	case OP_LOADA_S:
	case OP_LOADA_FNC:
		ptr = (eval_t *)(&OPA->_int + OPB->_int); /*pointer arithmatic*/
		OPC->_int = ptr->_int;
		break;

	case OP_LOADA_V:
		ptr = (eval_t *)(&OPA->_int + OPB->_int);
		OPC->_vector[0] = ptr->_vector[0];
		OPC->_vector[1] = ptr->_vector[1];
		OPC->_vector[2] = ptr->_vector[2];
		break;



	case OP_ADD_SF:	//(char*)c = (char*)a + (float)b
		OPC->_int = OPA->_int + (int)OPB->_float;
		break;
	case OP_SUB_S:	//(float)c = (char*)a - (char*)b
		OPC->_int = OPA->_int - OPB->_int;
		break;
	case OP_LOADP_C:	//load character from a string/pointer
		i = (unsigned int)OPA->_int + (unsigned int)OPB->_float;
		if ((unsigned int)i > prinst.addressableused-sizeof(char))
		{
			i = (unsigned int)OPB->_float;
			ptr = (eval_t*)PR_StringToNative(&progfuncs->funcs, OPA->_int);
			if ((size_t)i > strlen((char*)ptr))
			{
				pr_xstatement = st-pr_statements;
				PR_RunError (&progfuncs->funcs, "bad pointer read in %s (%i bytes into %s)", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name), i, ptr);
			}
			ptr = (eval_t*)((char*)ptr + i);
		}
		else 
			ptr = QCPOINTERM(i);
		OPC->_float = *(unsigned char *)ptr;
		break;
	case OP_LOADP_I:
	case OP_LOADP_F:
	case OP_LOADP_FLD:
	case OP_LOADP_ENT:
	case OP_LOADP_S:
	case OP_LOADP_FNC:
		i = OPA->_int + OPB->_int*4;
		if ((unsigned int)i > prinst.addressableused-sizeof(int))
		{
			if (i == -1)
			{
				OPC->_int = 0;
				break;
			}
			pr_xstatement = st-pr_statements;
			PR_RunError (&progfuncs->funcs, "bad pointer read in %s (from %#x)", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name), i);
		}
		ptr = QCPOINTERM(i);
		OPC->_int = ptr->_int;
		break;

	case OP_LOADP_V:
		i = OPA->_int + OPB->_int*4;	//NOTE: inconsistant!
		if ((unsigned int)i > prinst.addressableused-sizeof(vec3_t))
		{
			if (i == -1)
			{
				OPC->_int = 0;
				break;
			}
			pr_xstatement = st-pr_statements;
			PR_RunError (&progfuncs->funcs, "bad pointer read in %s (from %#x)", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name), i);
		}
		ptr = QCPOINTERM(i);
		OPC->_vector[0] = ptr->_vector[0];
		OPC->_vector[1] = ptr->_vector[1];
		OPC->_vector[2] = ptr->_vector[2];
		break;

	case OP_BITXOR_I:
		OPC->_int = OPA->_int ^ OPB->_int;
		break;
	case OP_RSHIFT_I:
		OPC->_int = OPA->_int >> OPB->_int;
		break;
	case OP_LSHIFT_I:
		OPC->_int = OPA->_int << OPB->_int;
		break;

	//hexen2 arrays contain a prefix global set to (arraysize-1) inserted before the actual array data
	//for vectors, this prefix is the number of vectors rather than the number of globals. this can cause issues with using OP_FETCH_GBL_V within structs.
	case OP_FETCH_GBL_F:
	case OP_FETCH_GBL_S:
	case OP_FETCH_GBL_E:
	case OP_FETCH_GBL_FNC:
		i = OPB->_float;
		if((unsigned)i > (unsigned)((eval_t *)&glob[st->a-1])->_int)
		{
			pr_xstatement = st-pr_statements;
			PR_RunError(&progfuncs->funcs, "array index out of bounds: %s[%d] (max %d)", PR_GlobalStringNoContents(progfuncs, st->a), i, ((eval_t *)&glob[st->a-1])->_int);
		}
		OPC->_int = ((eval_t *)&glob[st->a + i])->_int;
		break;
	case OP_FETCH_GBL_V:
		i = OPB->_float;
		if((unsigned)i > (unsigned)((eval_t *)&glob[st->a-1])->_int)
		{
			pr_xstatement = st-pr_statements;
			PR_RunError(&progfuncs->funcs, "array index out of bounds: %s[%d]", PR_GlobalStringNoContents(progfuncs, st->a), i);
		}
		ptr = (eval_t *)&glob[st->a + i*3];
		OPC->_vector[0] = ptr->_vector[0];
		OPC->_vector[1] = ptr->_vector[1];
		OPC->_vector[2] = ptr->_vector[2];
		break;

	case OP_CSTATE:
		externs->cstateop(&progfuncs->funcs, OPA->_float, OPB->_float, pr_xfunction - pr_cp_functions);
		break;

	case OP_CWSTATE:
		externs->cwstateop(&progfuncs->funcs, OPA->_float, OPB->_float, pr_xfunction - pr_cp_functions);
		break;

	case OP_THINKTIME:
		externs->thinktimeop(&progfuncs->funcs, (struct edict_s *)PROG_TO_EDICT(progfuncs, OPA->edict), OPB->_float);
		break;

	case OP_MULSTORE_F:
		/*OPC->_float = */OPB->_float *= OPA->_float;
		break;
	case OP_MULSTORE_VF:
		tmpf = OPA->_float;	//don't break on vec*=vec_x;
		/*OPC->_vector[0] = */OPB->_vector[0] *= tmpf;
		/*OPC->_vector[1] = */OPB->_vector[1] *= tmpf;
		/*OPC->_vector[2] = */OPB->_vector[2] *= tmpf;
		break;
	case OP_MULSTOREP_F:
		if (QCPOINTERWRITEFAIL(OPB, sizeof(float)))
		{
			pr_xstatement = st-pr_statements;
			PR_RunError (&progfuncs->funcs, "bad pointer write in %s (%x >= %x)", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name), OPB->_int, prinst.addressableused);
		}
		ptr = QCPOINTER(OPB);
		OPC->_float = ptr->_float *= OPA->_float;
		break;
	case OP_MULSTOREP_VF:
		if (QCPOINTERWRITEFAIL(OPB, sizeof(float)))
		{
			pr_xstatement = st-pr_statements;
			PR_RunError (&progfuncs->funcs, "bad pointer write in %s (%x >= %x)", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name), OPB->_int, prinst.addressableused);
		}
		ptr = QCPOINTER(OPB);
		tmpf = OPA->_float;	//don't break on vec*=vec_x;
		OPC->_vector[0] = ptr->_vector[0] *= tmpf;
		OPC->_vector[1] = ptr->_vector[1] *= tmpf;
		OPC->_vector[2] = ptr->_vector[2] *= tmpf;
		break;
	case OP_DIVSTORE_F:
		/*OPC->_float = */OPB->_float /= OPA->_float;
		break;
	case OP_DIVSTOREP_F:
		if (QCPOINTERWRITEFAIL(OPB, sizeof(float)))
		{
			pr_xstatement = st-pr_statements;
			PR_RunError (&progfuncs->funcs, "bad pointer write in %s (%x >= %x)", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name), OPB->_int, prinst.addressableused);
		}
		ptr = QCPOINTER(OPB);
		OPC->_float = ptr->_float /= OPA->_float;
		break;
	case OP_ADDSTORE_F:
		/*OPC->_float = */OPB->_float += OPA->_float;
		break;
	case OP_ADDSTORE_V:
		/*OPC->_vector[0] =*/ OPB->_vector[0] += OPA->_vector[0];
		/*OPC->_vector[1] =*/ OPB->_vector[1] += OPA->_vector[1];
		/*OPC->_vector[2] =*/ OPB->_vector[2] += OPA->_vector[2];
		break;
	case OP_ADDSTOREP_F:
		if (QCPOINTERWRITEFAIL(OPB, sizeof(float)))
		{
			pr_xstatement = st-pr_statements;
			PR_RunError (&progfuncs->funcs, "bad pointer write in %s (%x >= %x)", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name), OPB->_int, prinst.addressableused);
		}
		ptr = QCPOINTER(OPB);
		OPC->_float = ptr->_float += OPA->_float;
		break;
	case OP_ADDSTOREP_V:
		if (QCPOINTERWRITEFAIL(OPB, sizeof(float)))
		{
			pr_xstatement = st-pr_statements;
			PR_RunError (&progfuncs->funcs, "bad pointer write in %s (%x >= %x)", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name), OPB->_int, prinst.addressableused);
		}
		ptr = QCPOINTER(OPB);
		OPC->_vector[0] = ptr->_vector[0] += OPA->_vector[0];
		OPC->_vector[1] = ptr->_vector[1] += OPA->_vector[1];
		OPC->_vector[2] = ptr->_vector[2] += OPA->_vector[2];
		break;
	case OP_SUBSTORE_F:
		/*OPC->_float = */OPB->_float -= OPA->_float;
		break;
	case OP_SUBSTORE_V:
		/*OPC->_vector[0] = */OPB->_vector[0] -= OPA->_vector[0];
		/*OPC->_vector[1] = */OPB->_vector[1] -= OPA->_vector[1];
		/*OPC->_vector[2] = */OPB->_vector[2] -= OPA->_vector[2];
		break;
	case OP_SUBSTOREP_F:
		if (QCPOINTERWRITEFAIL(OPB, sizeof(float)))
		{
			pr_xstatement = st-pr_statements;
			PR_RunError (&progfuncs->funcs, "bad pointer write in %s (%x >= %x)", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name), OPB->_int, prinst.addressableused);
		}
		ptr = QCPOINTER(OPB);
		OPC->_float = ptr->_float -= OPA->_float;
		break;
	case OP_SUBSTOREP_V:
		if (QCPOINTERWRITEFAIL(OPB, sizeof(float)))
		{
			pr_xstatement = st-pr_statements;
			PR_RunError (&progfuncs->funcs, "bad pointer write in %s (%x >= %x)", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name), OPB->_int, prinst.addressableused);
		}
		ptr = QCPOINTER(OPB);
		OPC->_vector[0] = ptr->_vector[0] -= OPA->_vector[0];
		OPC->_vector[1] = ptr->_vector[1] -= OPA->_vector[1];
		OPC->_vector[2] = ptr->_vector[2] -= OPA->_vector[2];
		break;
	case OP_BITSETSTORE_F:
		OPB->_float = (int)OPB->_float | (int)OPA->_float;
		break;
	case OP_BITSETSTOREP_F:
		if (QCPOINTERWRITEFAIL(OPB, sizeof(float)))
		{
			pr_xstatement = st-pr_statements;
			PR_RunError (&progfuncs->funcs, "bad pointer write in %s (%x >= %x)", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name), OPB->_int, prinst.addressableused);
		}
		ptr = QCPOINTER(OPB);
		ptr->_float = (int)ptr->_float | (int)OPA->_float;
		break;
	case OP_BITCLRSTORE_F:
		OPB->_float = (int)OPB->_float & ~(int)OPA->_float;
		break;
	case OP_BITCLRSTOREP_F:
		if (QCPOINTERWRITEFAIL(OPB, sizeof(float)))
		{
			pr_xstatement = st-pr_statements;
			PR_RunError (&progfuncs->funcs, "bad pointer write in %s (%x >= %x)", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name), OPB->_int, prinst.addressableused);
		}
		ptr = QCPOINTER(OPB);
		ptr->_float = (int)ptr->_float & ~(int)OPA->_float;
		break;

	//for scaler randoms, prevent the random value from ever reaching 1
	//this avoids issues when array[random()*array.length]
	case OP_RAND0:
		OPC->_float = (rand ()&0x7fff) / ((float)0x8000);
		break;
	case OP_RAND1:
		OPC->_float = (rand ()&0x7fff) / ((float)0x8000)*OPA->_float;
		break;
	case OP_RAND2:	//backwards range shouldn't matter (except that it is b that is never reached, rather than the higher of the two)
		OPC->_float = OPA->_float + (rand ()&0x7fff) / ((float)0x8000)*(OPB->_float-OPA->_float);
		break;
	//random vectors DO result in 0 to 1 inclusive, to try to ensure a more balanced range
	case OP_RANDV0:
		OPC->_vector[0] = (rand ()&0x7fff) / ((float)0x7fff);
		OPC->_vector[1] = (rand ()&0x7fff) / ((float)0x7fff);
		OPC->_vector[2] = (rand ()&0x7fff) / ((float)0x7fff);
		break;
	case OP_RANDV1:
		OPC->_vector[0] = (rand ()&0x7fff) / ((float)0x7fff)*OPA->_vector[0];
		OPC->_vector[1] = (rand ()&0x7fff) / ((float)0x7fff)*OPA->_vector[1];
		OPC->_vector[2] = (rand ()&0x7fff) / ((float)0x7fff)*OPA->_vector[2];
		break;
	case OP_RANDV2:	//backwards range shouldn't matter
		OPC->_vector[0] = OPA->_vector[0] + (rand ()&0x7fff) / ((float)0x7fff)*(OPB->_vector[0]-OPA->_vector[0]);
		OPC->_vector[1] = OPA->_vector[1] + (rand ()&0x7fff) / ((float)0x7fff)*(OPB->_vector[1]-OPA->_vector[1]);
		OPC->_vector[2] = OPA->_vector[2] + (rand ()&0x7fff) / ((float)0x7fff)*(OPB->_vector[2]-OPA->_vector[2]);
		break;

	case OP_SWITCH_F:
	case OP_SWITCH_V:
	case OP_SWITCH_S:
	case OP_SWITCH_E:
	case OP_SWITCH_FNC:
		//the case opcodes depend upon the preceding switch.
		//otherwise the switch itself is much like a goto
		//don't embed the case/caserange checks directly into the switch so that custom caseranges can be potentially be implemented with hybrid emulation.
		switchcomparison = op - OP_SWITCH_F;
		switchref = OPA;
		RUNAWAYCHECK();
		st += (sofs)st->b - 1;	// offset the s++
		break;
	case OP_CASE:
		//if the comparison is true, jump (back up) to the relevent code block
		if (casecmp[switchcomparison](progfuncs, switchref, OPA))
		{
			RUNAWAYCHECK();
			st += (sofs)st->b-1; // -1 to offset the s++
		}
		break;
	case OP_CASERANGE:
		//if the comparison is true, jump (back up) to the relevent code block
		if (casecmprange[switchcomparison](progfuncs, switchref, OPA, OPC))
		{
			RUNAWAYCHECK();
			st += (sofs)st->c-1; // -1 to offset the s++
		}
		break;








	case OP_BITAND_IF:
		OPC->_int = (OPA->_int & (int)OPB->_float);
		break;
	case OP_BITOR_IF:
		OPC->_int = (OPA->_int | (int)OPB->_float);
		break;
	case OP_BITAND_FI:
		OPC->_int = ((int)OPA->_float & OPB->_int);
		break;
	case OP_BITOR_FI:
		OPC->_int = ((int)OPA->_float | OPB->_int);
		break;

	case OP_MUL_IF:
		OPC->_float = (OPA->_int * OPB->_float);
		break;
	case OP_MUL_FI:
		OPC->_float = (OPA->_float * OPB->_int);
		break;

	case OP_MUL_VI:
		tmpi = OPB->_int;
		OPC->_vector[0] = OPA->_vector[0] * tmpi;
		OPC->_vector[1] = OPA->_vector[1] * tmpi;
		OPC->_vector[2] = OPA->_vector[2] * tmpi;
		break;
	case OP_MUL_IV:
		tmpi = OPA->_int;
		OPC->_vector[0] = tmpi * OPB->_vector[0];
		OPC->_vector[1] = tmpi * OPB->_vector[1];
		OPC->_vector[2] = tmpi * OPB->_vector[2];
		break;

	case OP_DIV_IF:
		OPC->_float = (OPA->_int / OPB->_float);
		break;
	case OP_DIV_FI:
		OPC->_float = (OPA->_float / OPB->_int);
		break;

	case OP_AND_I:
		OPC->_int = (OPA->_int && OPB->_int);
		break;
	case OP_OR_I:
		OPC->_int = (OPA->_int || OPB->_int);
		break;

	case OP_AND_IF:
		OPC->_int = (OPA->_int && OPB->_float);
		break;
	case OP_OR_IF:
		OPC->_int = (OPA->_int || OPB->_float);
		break;

	case OP_AND_FI:
		OPC->_int = (OPA->_float && OPB->_int);
		break;
	case OP_OR_FI:
		OPC->_int = (OPA->_float || OPB->_int);
		break;

	case OP_NOT_I:
		OPC->_int = !OPA->_int;
		break;

	case OP_NE_IF:
		OPC->_int = (OPA->_int != OPB->_float);
		break;
	case OP_NE_FI:
		OPC->_int = (OPA->_float != OPB->_int);
		break;

	case OP_GADDRESS: //return glob[aint+bfloat]
		//this instruction is not implemented due to the weirdness of it.
		//its theoretically a more powerful load... but untyped?
		//or is it meant to be an LEA instruction (that could simply be switched with OP_GLOAD_I)
		pr_xstatement = st-pr_statements;
		PR_RunError (&progfuncs->funcs, "OP_GADDRESS not implemented (found in %s)", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name));
		break;
	case OP_GLOAD_I:
	case OP_GLOAD_F:
	case OP_GLOAD_FLD:
	case OP_GLOAD_ENT:
	case OP_GLOAD_S:
	case OP_GLOAD_FNC:
		if (OPA->_int < 0 || OPA->_int*4 >= current_progstate->globals_size)
		{
			pr_xstatement = st-pr_statements;
			PR_RunError (&progfuncs->funcs, "bad indexed global read in %s (%x >= %x)", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name), OPA->_int, current_progstate->globals_size);
		}
		ptr = ((eval_t *)&glob[OPA->_int]);
		OPC->_int = ptr->_int;
		break;
	case OP_GLOAD_V:
		if (OPA->_int < 0 || (OPA->_int+2)*4 >= current_progstate->globals_size)
		{
			pr_xstatement = st-pr_statements;
			PR_RunError (&progfuncs->funcs, "bad indexed global read in %s (%x >= %x)", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name), OPA->_int, current_progstate->globals_size);
		}
		ptr = ((eval_t *)&glob[OPA->_int]);
		OPC->_vector[0] = ptr->_vector[0];
		OPC->_vector[1] = ptr->_vector[1];
		OPC->_vector[2] = ptr->_vector[2];
		break;
	case OP_GSTOREP_I:
	case OP_GSTOREP_F:
	case OP_GSTOREP_ENT:
	case OP_GSTOREP_FLD:
	case OP_GSTOREP_S:
	case OP_GSTOREP_FNC:
		if (OPB->_int < 0 || OPB->_int*4 >= current_progstate->globals_size)
		{
			pr_xstatement = st-pr_statements;
			PR_RunError (&progfuncs->funcs, "bad indexed global write in %s (%x >= %x)", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name), OPB->_int, prinst.addressableused);
		}
		ptr = ((eval_t *)&glob[OPB->_int]);
		ptr->_int = OPA->_int;
		break;
	case OP_GSTOREP_V:
		if (OPB->_int < 0 || (OPB->_int+2)*4 >= current_progstate->globals_size)
		{
			pr_xstatement = st-pr_statements;
			PR_RunError (&progfuncs->funcs, "bad indexed global write in %s (%x >= %x)", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name), OPB->_int, prinst.addressableused);
		}
		ptr = ((eval_t *)&glob[OPB->_int]);
		ptr->_vector[0] = OPA->_vector[0];
		ptr->_vector[1] = OPA->_vector[1];
		ptr->_vector[2] = OPA->_vector[2];
		break;

	case OP_BOUNDCHECK:
		if ((unsigned int)OPA->_int < (unsigned int)st->c || (unsigned int)OPA->_int >= (unsigned int)st->b)
		{
			printf("Progs boundcheck failed. Value is %i. Must be between %u and %u\n", OPA->_int, st->c, st->b);
			QCFAULT(&progfuncs->funcs, "Progs boundcheck failed. Value is %i. Must be between %u and %u\n", OPA->_int, st->c, st->b);
/*			s=ShowStepf(progfuncs, st - pr_statements, "Progs boundcheck failed. Value is %i. Must be between %u and %u\n", OPA->_int, st->c, st->b);
			if (st == pr_statements + s)
				PR_RunError(&progfuncs->funcs, "unable to resume boundcheck");
			st = pr_statements + s;
			return s;
*/		}
		break;
/*	case OP_PUSH:
		OPC->_int = ENGINEPOINTER(&localstack[localstack_used+pr_spushed]);
		pr_spushed += OPA->_int;
		if (pr_spushed + localstack_used >= LOCALSTACK_SIZE)
		{
			pr_spushed = 0;
			pr_xstatement = st-pr_statements;
			PR_RunError(progfuncs, "Progs pushed too much");
		}
		break;
	case OP_POP:
		pr_spushed -= OPA->_int;
		if (pr_spushed < 0)
		{
			pr_spushed = 0;
			pr_xstatement = st-pr_statements;
			PR_RunError(progfuncs, "Progs poped more than it pushed");
		}
		break;
*/
	default:					
		if (op & 0x8000)	//break point!
		{
			op &= ~0x8000;
			s = st-pr_statements;
			if (pr_xstatement != s)
			{
				pr_xstatement = s;
				printf("Break point hit in %s.\n", PR_StringToNative(&progfuncs->funcs, pr_xfunction->s_name));
				s = ShowStep(progfuncs, s, NULL);
				st = &pr_statements[s];	//let the user move execution
				pr_xstatement = s = st-pr_statements;
				op = st->op & ~0x8000;
			}
			goto reeval;	//reexecute
		}
		pr_xstatement = st-pr_statements;
		PR_RunError (&progfuncs->funcs, "Bad opcode %i", st->op);
	}
}


#undef cont
#undef reeval
#undef st
#undef pr_statements
#undef fakeop
#undef dstatement_t
#undef sofs
#undef OPCODE

#undef ENGINEPOINTER
#undef QCPOINTER
#undef QCPOINTERM

