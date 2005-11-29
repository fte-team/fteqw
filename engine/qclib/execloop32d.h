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
#define st st16
#define pr_statements pr_statements16
#define fakeop fakeop16
#define dstatement_t dstatement16_t
#define sofs signed short
#define uofs unsigned short
#elif INTSIZE == 32
#define cont cont32
#define reeval reeval32
#define st st32
#define pr_statements pr_statements32
#define fakeop fakeop32
#define dstatement_t dstatement32_t
#define sofs signed int
#define uofs unsigned int
#elif INTSIZE == 24
#error INTSIZE should be set to 32.
#else
#error Bad cont size
#endif

#define ENGINEPOINTER(p) ((char*)(p) - progfuncs->stringtable)
#define QCPOINTER(p) (eval_t *)(p->_int+progfuncs->stringtable)
#define QCPOINTERM(p) (eval_t *)((p)+progfuncs->stringtable)

//rely upon just st
{
#ifdef DEBUGABLE
cont:	//last statement may have been a breakpoint		
	s = st-pr_statements;
	s+=1;
	s=ShowStep(progfuncs, s);
	st = pr_statements + s;

reeval:
#else
	st++;
#endif

	switch (st->op)
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
		OPC->_vector[0] = OPA->_float * OPB->_vector[0];
		OPC->_vector[1] = OPA->_float * OPB->_vector[1];
		OPC->_vector[2] = OPA->_float * OPB->_vector[2];
		break;
	case OP_MUL_VF:
		OPC->_vector[0] = OPB->_float * OPA->_vector[0];
		OPC->_vector[1] = OPB->_float * OPA->_vector[1];
		OPC->_vector[2] = OPB->_float * OPA->_vector[2];
		break;

	case OP_DIV_F:
		OPC->_float = OPA->_float / OPB->_float;
		break;
	case OP_DIV_VF:
		OPC->_vector[0] = OPB->_float / OPA->_vector[0];
		OPC->_vector[1] = OPB->_float / OPA->_vector[1];
		OPC->_vector[2] = OPB->_float / OPA->_vector[2];
		break;

	case OP_BITAND:
		OPC->_float = (float)((int)OPA->_float & (int)OPB->_float);
		break;

	case OP_BITOR:
		OPC->_float = (float)((int)OPA->_float | (int)OPB->_float);
		break;


	case OP_GE:
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

	case OP_LE:
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

	case OP_GT:
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

	case OP_LT:
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

	case OP_AND:
		OPC->_float = (float)(OPA->_float && OPB->_float);
		break;
	case OP_OR:
		OPC->_float = (float)(OPA->_float || OPB->_float);
		break;

	case OP_NOT_F:
		OPC->_float = (float)(!OPA->_float);
		break;
	case OP_NOT_V:
		OPC->_float = (float)(!OPA->_vector[0] && !OPA->_vector[1] && !OPA->_vector[2]);
		break;
	case OP_NOT_S:
		OPC->_float = (float)(!(OPA->string) || !*(OPA->string+progfuncs->stringtable));
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
			if (!OPB->string || !*(OPB->string+progfuncs->stringtable))
				OPC->_float = true;
			else
				OPC->_float = false;
		}
		else if (!OPB->string)
		{
			if (!OPA->string || !*(OPA->string+progfuncs->stringtable))
				OPC->_float = true;
			else
				OPC->_float = false;
		}
		else
			OPC->_float = (float)(!strcmp(OPA->string+progfuncs->stringtable,OPB->string+progfuncs->stringtable));
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
			if (!OPB->string || !*(OPB->string+progfuncs->stringtable))
				OPC->_float = false;
			else
				OPC->_float = true;
		}
		else if (!OPB->string)
		{
			if (!OPA->string || !*(OPA->string+progfuncs->stringtable))
				OPC->_float = false;
			else
				OPC->_float = true;
		}
		else
			OPC->_float = (float)(strcmp(OPA->string+progfuncs->stringtable,OPB->string+progfuncs->stringtable));		
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
	case OP_STORE_I:
		OPB->_int = OPA->_int;
		break;
	case OP_STORE_F:
	case OP_STORE_ENT:
	case OP_STORE_FLD:		// integers
	case OP_STORE_S:
	case OP_STORE_FNC:		// pointers
		OPB->_int = OPA->_int;
		break;
	case OP_STORE_V:
		OPB->_vector[0] = OPA->_vector[0];
		OPB->_vector[1] = OPA->_vector[1];
		OPB->_vector[2] = OPA->_vector[2];
		break;

	//store a value to a pointer
	case OP_STOREP_IF:
		ptr = QCPOINTER(OPB);
		ptr->_float = (float)OPA->_int;
		break;
	case OP_STOREP_FI:
		ptr = QCPOINTER(OPB);
		ptr->_int = (int)OPA->_float;
		break;
	case OP_STOREP_I:
		ptr = QCPOINTER(OPB);
		ptr->_int = OPA->_int;
		break;
	case OP_STOREP_F:
	case OP_STOREP_ENT:
	case OP_STOREP_FLD:		// integers
	case OP_STOREP_S:
	case OP_STOREP_FNC:		// pointers
		ptr = QCPOINTER(OPB);
		ptr->_int = OPA->_int;
		break;
	case OP_STOREP_V:
		ptr = QCPOINTER(OPB);
		ptr->_vector[0] = OPA->_vector[0];
		ptr->_vector[1] = OPA->_vector[1];
		ptr->_vector[2] = OPA->_vector[2];
		break;

	case OP_STOREP_C:	//store character in a string
		ptr = QCPOINTER(OPB);
		*(unsigned char *)ptr = (char)OPA->_float;
		break;

	case OP_MULSTORE_F: // f *= f
		OPB->_float *= OPA->_float;
		break;
	case OP_MULSTORE_V: // v *= f
		OPB->_vector[0] *= OPA->_float;
		OPB->_vector[1] *= OPA->_float;
		OPB->_vector[2] *= OPA->_float;
		break;
	case OP_MULSTOREP_F: // e.f *= f
		ptr = QCPOINTER(OPB);
		OPC->_float = (ptr->_float *= OPA->_float);
		break;
	case OP_MULSTOREP_V: // e.v *= f
		ptr = QCPOINTER(OPB);
		OPC->_vector[0] = (ptr->_vector[0] *= OPA->_float);
		OPC->_vector[0] = (ptr->_vector[1] *= OPA->_float);
		OPC->_vector[0] = (ptr->_vector[2] *= OPA->_float);
		break;

	case OP_DIVSTORE_F: // f /= f
		OPB->_float /= OPA->_float;
		break;
	case OP_DIVSTOREP_F: // e.f /= f
		ptr = QCPOINTER(OPB);
		OPC->_float = (ptr->_float /= OPA->_float);
		break;

	case OP_ADDSTORE_F: // f += f
		OPB->_float += OPA->_float;
		break;
	case OP_ADDSTORE_V: // v += v
		OPB->_vector[0] += OPA->_vector[0];
		OPB->_vector[1] += OPA->_vector[1];
		OPB->_vector[2] += OPA->_vector[2];
		break;
	case OP_ADDSTOREP_F: // e.f += f
		ptr = QCPOINTER(OPB);
		OPC->_float = (ptr->_float += OPA->_float);
		break;
	case OP_ADDSTOREP_V: // e.v += v
		ptr = QCPOINTER(OPB);
		OPC->_vector[0] = (ptr->_vector[0] += OPA->_vector[0]);
		OPC->_vector[1] = (ptr->_vector[1] += OPA->_vector[1]);
		OPC->_vector[2] = (ptr->_vector[2] += OPA->_vector[2]);
		break;

	case OP_SUBSTORE_F: // f -= f
		OPB->_float -= OPA->_float;
		break;
	case OP_SUBSTORE_V: // v -= v
		OPB->_vector[0] -= OPA->_vector[0];
		OPB->_vector[1] -= OPA->_vector[1];
		OPB->_vector[2] -= OPA->_vector[2];
		break;
	case OP_SUBSTOREP_F: // e.f -= f
		ptr = QCPOINTER(OPB);
		OPC->_float = (ptr->_float -= OPA->_float);
		break;
	case OP_SUBSTOREP_V: // e.v -= v
		ptr = QCPOINTER(OPB);
		OPC->_vector[0] = (ptr->_vector[0] -= OPA->_vector[0]);
		OPC->_vector[1] = (ptr->_vector[1] -= OPA->_vector[1]);
		OPC->_vector[2] = (ptr->_vector[2] -= OPA->_vector[2]);
		break;


	//get a pointer to a field var
	case OP_ADDRESS:
		if ((unsigned)OPA->edict >= (unsigned)maxedicts)
			PR_RunError (progfuncs, "OP_ADDRESS references invalid entity in %s", progfuncs->stringtable + pr_xfunction->s_name);
		ed = PROG_TO_EDICT(progfuncs, OPA->edict);
#ifdef PARANOID
		NUM_FOR_EDICT(ed);		// make sure it's in range
#endif
		if (ed->readonly)
		{
			pr_xstatement = st-pr_statements;
			PR_RunError (progfuncs, "assignment to read-only entity in %s", progfuncs->stringtable + pr_xfunction->s_name);
		}
		OPC->_int = ENGINEPOINTER((((int *)edvars(ed)) + OPB->_int + progfuncs->fieldadjust));
		break;

	//load a field to a value
	case OP_LOAD_I:
	case OP_LOAD_F:
	case OP_LOAD_FLD:
	case OP_LOAD_ENT:
	case OP_LOAD_S:
	case OP_LOAD_FNC:
		if ((unsigned)OPA->edict >= (unsigned)maxedicts)
			PR_RunError (progfuncs, "OP_LOAD references invalid entity in %s", progfuncs->stringtable + pr_xfunction->s_name);
		ed = PROG_TO_EDICT(progfuncs, OPA->edict);
#ifdef PARANOID
		NUM_FOR_EDICT(ed);		// make sure it's in range
#endif
		ptr = (eval_t *)(((int *)edvars(ed)) + OPB->_int + progfuncs->fieldadjust);
		OPC->_int = ptr->_int;
		break;

	case OP_LOAD_V:
		if ((unsigned)OPA->edict >= (unsigned)maxedicts)
			PR_RunError (progfuncs, "OP_LOAD_V references invalid entity in %s", progfuncs->stringtable + pr_xfunction->s_name);
		ed = PROG_TO_EDICT(progfuncs, OPA->edict);
#ifdef PARANOID
		NUM_FOR_EDICT(ed);		// make sure it's in range
#endif
		ptr = (eval_t *)(((int *)edvars(ed)) + OPB->_int + progfuncs->fieldadjust);
		OPC->_vector[0] = ptr->_vector[0];
		OPC->_vector[1] = ptr->_vector[1];
		OPC->_vector[2] = ptr->_vector[2];
		break;	
		
//==================

	case OP_IFNOTS:
		RUNAWAYCHECK();
		if (!OPA->string || !OPA->string[progfuncs->stringtable])
			st += (sofs)st->b - 1;	// offset the s++
		break;

	case OP_IFNOT:
		RUNAWAYCHECK();
		if (!OPA->_int)
			st += (sofs)st->b - 1;	// offset the s++
		break;

	case OP_IFS:
		RUNAWAYCHECK();
		if (OPA->string && OPA->string[progfuncs->stringtable])
			st += (sofs)st->b - 1;	// offset the s++
		break;
		
	case OP_IF:
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
		RUNAWAYCHECK();
		pr_xstatement = st-pr_statements;


		if (st->op > OP_CALL8)
			pr_argc = st->op - (OP_CALL1H-1);
		else
			pr_argc = st->op - OP_CALL0;
		fnum = OPA->function;
		if ((fnum & ~0xff000000)==0)
		{
			pr_trace++;
			printf("NULL function from qc (%s).\n", progfuncs->stringtable + pr_xfunction->s_name);
#ifndef DEBUGABLE
			goto cont;
#endif
			break;
		}
/*
{
	static char buffer[1024*1024*8];
	int size = sizeof buffer;
		progfuncs->save_ents(progfuncs, buffer, &size, 0);
}*/


		p=pr_typecurrent;
//about to switch. needs caching.

		//if it's an external call, switch now (before any function pointers are used)
		PR_MoveParms(progfuncs, (fnum & 0xff000000)>>24, p);
		PR_SwitchProgs(progfuncs, (fnum & 0xff000000)>>24);

		newf = &pr_functions[fnum & ~0xff000000];

		if (newf->first_statement < 0)
		{	// negative statements are built in functions

if (pr_typecurrent != 0)
{
	PR_MoveParms(progfuncs, 0, pr_typecurrent);
	PR_SwitchProgs(progfuncs, 0);
}
			i = -newf->first_statement;
//			p = pr_typecurrent;
			progfuncs->lastcalledbuiltinnumber = i;
			if (i < externs->numglobalbuiltins)
			{
				(*externs->globalbuiltins[i]) (progfuncs, (struct globalvars_s *)current_progstate->globals);
				if (prinst->continuestatement!=-1)
				{
					st=&pr_statements[prinst->continuestatement];
					prinst->continuestatement=-1;
					break;
				}
			}
			else
			{
				i -= externs->numglobalbuiltins;
				if (i >= current_progstate->numbuiltins)
				{
//					if (newf->first_statement == -0x7fffffff)
//						((builtin_t)newf->profile) (progfuncs, (struct globalvars_s *)current_progstate->globals);
//					else
						PR_RunError (progfuncs, "Bad builtin call number - %i", -newf->first_statement);
				}
				else
					current_progstate->builtins [i] (progfuncs, (struct globalvars_s *)current_progstate->globals);
			}
			PR_MoveParms(progfuncs, p, pr_typecurrent);
//			memcpy(&pr_progstate[p].globals[OFS_RETURN], &current_progstate->globals[OFS_RETURN], sizeof(vec3_t));
			PR_SwitchProgs(progfuncs, (progsnum_t)p);

//#ifndef DEBUGABLE	//decide weather non debugger wants to start debugging.
			s = st-pr_statements;
			goto restart;
//#endif
//			break;
		}
//		PR_MoveParms((OPA->function & 0xff000000)>>24, pr_typecurrent);
//		PR_SwitchProgs((OPA->function & 0xff000000)>>24);
		s = PR_EnterFunction (progfuncs, newf, p);
		st = &pr_statements[s];
		
		goto restart;
//		break;

	case OP_DONE:
	case OP_RETURN:

		RUNAWAYCHECK();

		pr_globals[OFS_RETURN] = pr_globals[st->a];
		pr_globals[OFS_RETURN+1] = pr_globals[st->a+1];
		pr_globals[OFS_RETURN+2] = pr_globals[st->a+2];
/*
{
	static char buffer[1024*1024*8];
	int size = sizeof buffer;
		progfuncs->save_ents(progfuncs, buffer, &size, 0);
}
*/
		s = PR_LeaveFunction (progfuncs);
		st = &pr_statements[s];		
		if (pr_depth == prinst->exitdepth)
		{		
			return;		// all done
		}
		goto restart;
//		break;

	case OP_STATE:
		externs->stateop(progfuncs, OPA->_float, OPB->function);
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
	

	//array/structure reading/riting.
	case OP_GLOBALADDRESS:
		OPC->_int = ENGINEPOINTER(&OPA->_int + OPB->_int);
		break;
	case OP_POINTER_ADD:	//pointer to 32 bit (remember to *3 for vectors)
		OPC->_int = OPA->_int + OPB->_int*4;
		break;

	case OP_LOADA_I:
	case OP_LOADA_F:
	case OP_LOADA_FLD:
	case OP_LOADA_ENT:
	case OP_LOADA_S:
	case OP_LOADA_FNC:
		ptr = (eval_t *)(&OPA->_int + OPB->_int);
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
	case OP_LOADP_C:	//load character from a string
		ptr = QCPOINTERM(OPA->_int + (int)OPB->_float);
		OPC->_float = *(unsigned char *)ptr;
		break;
	case OP_LOADP_I:
	case OP_LOADP_F:
	case OP_LOADP_FLD:
	case OP_LOADP_ENT:
	case OP_LOADP_S:
	case OP_LOADP_FNC:
		ptr = QCPOINTERM(OPA->_int + OPB->_int);
		OPC->_int = ptr->_int;
		break;

	case OP_LOADP_V:
		ptr = QCPOINTERM(OPA->_int + OPB->_int);
		OPC->_vector[0] = ptr->_vector[0];
		OPC->_vector[1] = ptr->_vector[1];
		OPC->_vector[2] = ptr->_vector[2];
		break;

	case OP_POWER_I:
		OPC->_int = OPA->_int ^ OPB->_int;
		break;
	case OP_RSHIFT_I:
		OPC->_int = OPA->_int >> OPB->_int;
		break;
	case OP_LSHIFT_I:
		OPC->_int = OPA->_int << OPB->_int;
		break;


	case OP_FETCH_GBL_F:
	case OP_FETCH_GBL_S:
	case OP_FETCH_GBL_E:
	case OP_FETCH_GBL_FNC:
		i = (int)OPB->_float;
		if(i < 0 || i > G_INT((uofs)st->a - 1))
		{
			PR_RunError(progfuncs, "array index out of bounds: %s[%d]", PR_GlobalStringNoContents(progfuncs, st->a), i);
		}
		t = (eval_t *)&pr_globals[(uofs)st->a + i];
		OPC->_int = t->_int;
		break;
	case OP_FETCH_GBL_V:
		i = (int)OPB->_float;
		if(i < 0 || i > G_INT((uofs)st->a - 1))
		{
			PR_RunError(progfuncs, "array index out of bounds: %s[%d]", PR_GlobalStringNoContents(progfuncs, st->a), i);
		}
		t = (eval_t *)&pr_globals[(uofs)st->a
			+((int)OPB->_float)*3];
		OPC->_vector[0] = t->_vector[0];
		OPC->_vector[1] = t->_vector[1];
		OPC->_vector[2] = t->_vector[2];
		break;

	case OP_CSTATE:
		externs->cstateop(progfuncs, OPA->_float, OPB->_float, fnum);
		break;

	case OP_CWSTATE:
		externs->cwstateop(progfuncs, OPA->_float, OPB->_float, fnum);
		break;

	case OP_THINKTIME:
		externs->thinktimeop(progfuncs, (struct edict_s *)PROG_TO_EDICT(progfuncs, OPA->edict), OPB->_float);
		break;


	case OP_BITSET: // b (+) a
		OPB->_float = (float)((int)OPB->_float | (int)OPA->_float);
		break;
	case OP_BITSETP: // .b (+) a
		ptr = QCPOINTER(OPB);
		ptr->_float = (float)((int)ptr->_float | (int)OPA->_float);
		break;
	case OP_BITCLR: // b (-) a
		OPB->_float = (float)((int)OPB->_float & ~((int)OPA->_float));
		break;
	case OP_BITCLRP: // .b (-) a
		ptr = QCPOINTER(OPB);
		ptr->_float = (float)((int)ptr->_float & ~((int)OPA->_float));
		break;

	case OP_RAND0:
		G_FLOAT(OFS_RETURN) = (rand()&0x7fff)/((float)0x7fff);
		break;
	case OP_RAND1:
		G_FLOAT(OFS_RETURN) = (rand()&0x7fff)/((float)0x7fff)*OPA->_float;
		break;
	case OP_RAND2:
		if(OPA->_float < OPB->_float)
		{
			G_FLOAT(OFS_RETURN) = OPA->_float+((rand()&0x7fff)/((float)0x7fff)
				*(OPB->_float-OPA->_float));
		}
		else
		{
			G_FLOAT(OFS_RETURN) = OPB->_float+((rand()&0x7fff)/((float)0x7fff)
				*(OPA->_float-OPB->_float));
		}
		break;
	case OP_RANDV0:
		G_FLOAT(OFS_RETURN+0) = (rand()&0x7fff)/((float)0x7fff);
		G_FLOAT(OFS_RETURN+1) = (rand()&0x7fff)/((float)0x7fff);
		G_FLOAT(OFS_RETURN+2) = (rand()&0x7fff)/((float)0x7fff);
		break;
	case OP_RANDV1:
		G_FLOAT(OFS_RETURN+0) = (rand()&0x7fff)/((float)0x7fff)*OPA->_vector[0];
		G_FLOAT(OFS_RETURN+1) = (rand()&0x7fff)/((float)0x7fff)*OPA->_vector[1];
		G_FLOAT(OFS_RETURN+2) = (rand()&0x7fff)/((float)0x7fff)*OPA->_vector[2];
		break;
	case OP_RANDV2:
		for(i = 0; i < 3; i++)
		{
			if(OPA->_vector[i] < OPB->_vector[i])
			{
				G_FLOAT(OFS_RETURN+i) = OPA->_vector[i]+((rand()&0x7fff)/((float)0x7fff)
					*(OPB->_vector[i]-OPA->_vector[i]));
			}
			else
			{
				G_FLOAT(OFS_RETURN+i) = OPB->_vector[i]+(rand()*(1.0f/RAND_MAX)
					*(OPA->_vector[i]-OPB->_vector[i]));
			}
		}
		break;


	case OP_SWITCH_F:
	case OP_SWITCH_V:
	case OP_SWITCH_S:
	case OP_SWITCH_E:
	case OP_SWITCH_FNC:
		swtch = OPA;
		swtchtype = st->op;
		RUNAWAYCHECK();
		st += (sofs)st->b - 1;	// offset the st++
		break;
	case OP_CASE:
		switch(swtchtype)
		{
		case OP_SWITCH_F:
			if (swtch->_float == OPA->_float)
			{
				RUNAWAYCHECK();
				st += (sofs)st->b-1; // -1 to offset the s++
			}
			break;
		case OP_SWITCH_E:
		case OP_SWITCH_FNC:
			if (swtch->_int == OPA->_int)
			{
				RUNAWAYCHECK();
				st += (sofs)st->b-1; // -1 to offset the s++
			}
			break;
		case OP_SWITCH_S:
			if (swtch->_int == OPA->_int)
			{
				RUNAWAYCHECK();
				st += (sofs)st->b-1; // -1 to offset the s++
			}
			if ((!swtch->_int && progfuncs->stringtable[OPA->string]) || (!OPA->_int && progfuncs->stringtable[swtch->string]))	//one is null (cannot be not both).
				break;
			if (!strcmp(progfuncs->stringtable+swtch->string, progfuncs->stringtable+OPA->string))
			{
				RUNAWAYCHECK();
				st += (sofs)st->b-1; // -1 to offset the s++
			}
			break;
		case OP_SWITCH_V:
			if (swtch->_vector[0] == OPA->_vector[0] && swtch->_vector[1] == OPA->_vector[1] && swtch->_vector[2] == OPA->_vector[2])
			{
				RUNAWAYCHECK();
				st += (sofs)st->b-1; // -1 to offset the s++
			}
			break;
		default:
			PR_RunError (progfuncs, "OP_CASE with bad/missing OP_SWITCH %i", swtchtype);
			break;
		}
		break;
	case OP_CASERANGE:
		switch(swtchtype)
		{
		case OP_SWITCH_F:
			if (swtch->_float >= OPA->_float && swtch->_float <= OPB->_float)
			{
				RUNAWAYCHECK();
				st += (sofs)st->c-1; // -1 to offset the s++
			}
			break;
		default:
			PR_RunError (progfuncs, "OP_CASERANGE with bad/missing OP_SWITCH %i", swtchtype);
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
		OPC->_vector[0] = OPA->_vector[0] * OPB->_int;
		OPC->_vector[1] = OPA->_vector[0] * OPB->_int;
		OPC->_vector[2] = OPA->_vector[0] * OPB->_int;
		break;
	case OP_MUL_IV:
		OPC->_vector[0] = OPB->_int * OPA->_vector[0];
		OPC->_vector[1] = OPB->_int * OPA->_vector[1];
		OPC->_vector[2] = OPB->_int * OPA->_vector[2];
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

	case OP_GSTOREP_I:
	case OP_GSTOREP_F:
	case OP_GSTOREP_ENT:
	case OP_GSTOREP_FLD:		// integers
	case OP_GSTOREP_S:
	case OP_GSTOREP_FNC:		// pointers
	case OP_GSTOREP_V:
	case OP_GADDRESS:
	case OP_GLOAD_I:
	case OP_GLOAD_F:
	case OP_GLOAD_FLD:
	case OP_GLOAD_ENT:
	case OP_GLOAD_S:
	case OP_GLOAD_FNC:
		pr_xstatement = st-pr_statements;
		PR_RunError(progfuncs, "Extra opcode not implemented\n");
		break;

	case OP_BOUNDCHECK:
		if ((unsigned int)OPA->_int < (unsigned int)st->c || (unsigned int)OPA->_int >= (unsigned int)st->b)
		{
			pr_xstatement = st-pr_statements;
			PR_RunError(progfuncs, "Progs boundcheck failed. Value is %i.", OPA->_int);
		}
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
		if (st->op & 0x8000)	//break point!
		{
			pr_xstatement = s = st-pr_statements;

			printf("Break point hit in %s.\n", pr_xfunction->s_name+progfuncs->stringtable);
			if (pr_trace<1)
				pr_trace=1;	//this is what it's for

			s = ShowStep(progfuncs, s);
			st = &pr_statements[s];	//let the user move execution
			pr_xstatement = s = st-pr_statements;

#if 0	//fakeop stuff - not practical, the rest of the code is more optimised, st needs to point at the correct statement
			memcpy(&fakeop, st, sizeof(dstatement_t));	//don't hit the new statement as a break point, cos it's probably the same one.
			fakeop.op &= ~0x8000;
			st = &fakeop;	//a little remapping...
#else
			st->op &= ~0x8000;	//just remove the breakpoint and go around again, but this time in the debugger.
#endif

			goto reeval;	//reexecute
		}
		pr_xstatement = st-pr_statements;
		PR_RunError (progfuncs, "Bad opcode %i", st->op);
	}
}


#undef cont
#undef reeval
#undef st
#undef pr_statements
#undef fakeop
#undef dstatement_t
#undef sofs
#undef uofs

#undef ENGINEPOINTER
#undef QCPOINTER
#undef QCPOINTERM

