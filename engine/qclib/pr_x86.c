/*
when I say JIT, I mean load time, not execution time.

notes:
	qc jump offsets are all constants. we have no variable offset jumps (other than function calls/returns)
	field remapping... fields are in place, and cannot be adjusted. if a field is not set to 0, its assumed to be a constant.

optimisations:
	none at the moment...
	instructions need to be chained. stuff that writes to C should be cacheable, etc. maybe we don't even need to do the write to C
	it should also be possible to fold in eq+ifnot, so none of this silly storeing of floats in equality tests

	eax - tmp
	ebx - prinst->edicttable
	ecx	- tmp
	edx - tmp
	esi -
	edi - tmp (because its preserved by subfunctions
	ebp -

  to use gas to provide binary opcodes:
  vim -N blob.s && as blob.s && objdump.exe -d a.out
*/

#define PROGSUSED
#include "progsint.h"

#ifdef QCJIT

static float ta, tb, nullfloat=0;

unsigned int *statementjumps;	//[MAX_STATEMENTS*2]
unsigned char **statementoffsets; //[MAX_STATEMENTS]
unsigned int numjumps;
unsigned char *code;
unsigned int codesize;
unsigned int jitstatements;

void EmitByte(unsigned char byte)
{
	code[codesize++] = byte;
}
void Emit4Byte(unsigned int value)
{
	code[codesize++] = (value>> 0)&0xff;
	code[codesize++] = (value>> 8)&0xff;
	code[codesize++] = (value>>16)&0xff;
	code[codesize++] = (value>>24)&0xff;
}
void EmitAdr(void *value)
{
	Emit4Byte((unsigned int)value);
}
void EmitFloat(float value)
{
	union {float f; unsigned int i;} u;
	u.f = value;
	Emit4Byte(u.i);
}
void Emit2Byte(unsigned short value)
{
	code[codesize++] = (value>> 0)&0xff;
	code[codesize++] = (value>> 8)&0xff;
}

void EmitFOffset(void *func, int bias)
{
	union {void *f; unsigned int i;} u;
	u.f = func;
	u.i -= (unsigned int)&code[codesize+bias];
	Emit4Byte(u.i);
}

void Emit4ByteJump(int statementnum, int offset)
{
	statementjumps[numjumps++] = codesize;
	statementjumps[numjumps++] = statementnum;
	statementjumps[numjumps++] = offset;

	//the offset is filled in later
	codesize += 4;
}

enum
{
	REG_EAX,
	REG_ECX,
	REG_EDX,
	REG_EBX,
	REG_ESP,
	REG_EBP,
	REG_ESI,
	REG_EDI
};
#define XOR(sr,dr) EmitByte(0x31);EmitByte(0xc0 | (sr<<3) | dr);
#define CLEARREG(reg) XOR(reg,reg)
#define LOADREG(addr, reg) if (reg == REG_EAX) {EmitByte(0xa1);} else {EmitByte(0x8b); EmitByte((reg<<3) | 0x05);} EmitAdr(addr);
#define STOREREG(reg, addr) if (reg == REG_EAX) {EmitByte(0xa3);} else {EmitByte(0x89); EmitByte((reg<<3) | 0x05);} EmitAdr(addr);
#define STOREF(f, addr) EmitByte(0xc7);EmitByte(0x05); EmitAdr(addr);EmitFloat(f);
#define STOREI(f, addr) EmitByte(0xc7);EmitByte(0x05); EmitAdr(addr);EmitFloat(f);
#define SETREGI(val,reg) EmitByte(0xbe);EmitByte(val);EmitByte(val>>8);EmitByte(val>>16);EmitByte(val>>24);

void *LocalLoc(void)
{
	return &code[codesize];
}
void *LocalJmp(int cond)
{
	if (cond == OP_GOTO)
		EmitByte(0xeb);	//jmp
	else if (cond == OP_LE)
		EmitByte(0x7e);	//jle
	else if (cond == OP_GE)
		EmitByte(0x7d);	//jge
	else if (cond == OP_LT)
		EmitByte(0x7c);	//jl
	else if (cond == OP_GT)
		EmitByte(0x7f);	//jg
	else if ((cond >= OP_NE_F && cond <= OP_NE_FNC) || cond == OP_NE_I)
		EmitByte(0x75);	//jne
	else if ((cond >= OP_EQ_F && cond <= OP_EQ_FNC) || cond == OP_EQ_I)
		EmitByte(0x74);	//je
#if defined(DEBUG) && defined(_WIN32)
	else
	{
		OutputDebugString("oh noes!\n");
		return NULL;
	}
#endif

	EmitByte(0);

	return LocalLoc();
}
void LocalJmpLoc(void *jmp, void *loc)
{
	int offs;
	unsigned char *a = jmp;
	offs = (char *)loc - (char *)jmp;
#if defined(DEBUG) && defined(_WIN32)
	if (offs > 127 || offs <= -128)
	{
		OutputDebugStringA("bad jump\n");
		a[-2] = 0xcd;
		a[-1] = 0xcc;
		return;
	}
#endif
	a[-1] = offs;
}

void FixupJumps(void)
{
	unsigned int j;
	unsigned char *codesrc;
	unsigned char *codedst;
	unsigned int offset;

	unsigned int v;

	for (j = 0; j < numjumps;)
	{
		v = statementjumps[j++];
		codesrc = &code[v];

		v = statementjumps[j++];
		codedst = statementoffsets[v];

		v = statementjumps[j++];
		offset = (int)(codedst - (codesrc-v));	//3rd term because the jump is relative to the instruction start, not the instruction's offset

		codesrc[0] = (offset>> 0)&0xff;
		codesrc[1] = (offset>> 8)&0xff;
		codesrc[2] = (offset>>16)&0xff;
		codesrc[3] = (offset>>24)&0xff;
	}
}

int ASMCALL PR_LeaveFunction (progfuncs_t *progfuncs);
int ASMCALL PR_EnterFunction (progfuncs_t *progfuncs, dfunction_t *f, int progsnum);

pbool PR_GenerateJit(progfuncs_t *progfuncs)
{
	void *j0, *l0;
	void *j1, *l1;
	void *j2, *l2;
	unsigned int i;
	dstatement16_t *op = (dstatement16_t*)current_progstate->statements;
	unsigned int numstatements = current_progstate->progs->numstatements;
	int *glob = (int*)current_progstate->globals;

	if (current_progstate->numbuiltins)
		return false;

	jitstatements = numstatements;

	statementjumps = malloc(numstatements*12);
	statementoffsets = malloc(numstatements*4);
	code = malloc(numstatements*500);

	numjumps = 0;
	codesize = 0;



	for (i = 0; i < numstatements; i++)
	{
		statementoffsets[i] = &code[codesize];

		SETREGI(op[i].op, REG_ESI);
		switch(op[i].op)
		{
		//jumps
		case OP_IF:
			//integer compare
			//if a, goto b

			//cmpl $0,glob[A]
			EmitByte(0x83);EmitByte(0x3d);EmitAdr(glob + op[i].a);EmitByte(0x0);
			//jnz B
			EmitByte(0x0f);EmitByte(0x85);Emit4ByteJump(i + (signed short)op[i].b, -4);
			break;

		case OP_IFNOT:
			//integer compare
			//if !a, goto b

			//cmpl $0,glob[A]
			EmitByte(0x83);EmitByte(0x3d);EmitAdr(glob + op[i].a);EmitByte(0x0);
			//jz B
			EmitByte(0x0f);EmitByte(0x84);Emit4ByteJump(i + (signed short)op[i].b, -4);
			break;

		case OP_GOTO:
			EmitByte(0xE9);Emit4ByteJump(i + (signed short)op[i].a, -4);
			break;
			
		//function returns
		case OP_DONE:
		case OP_RETURN:
			//done and return are the same

			//part 1: store A into OFS_RETURN

			if (!op[i].a)
			{
				//assumption: anything that returns address 0 is a void or zero return.
				//thus clear eax and copy that to the return vector.
				CLEARREG(REG_EAX);
				STOREREG(REG_EAX, glob + OFS_RETURN+0);
				STOREREG(REG_EAX, glob + OFS_RETURN+1);
				STOREREG(REG_EAX, glob + OFS_RETURN+2);
			}
			else
			{
				LOADREG(glob + op[i].a+0, REG_EAX);
				LOADREG(glob + op[i].a+1, REG_EDX);
				LOADREG(glob + op[i].a+2, REG_ECX);
				STOREREG(REG_EAX, glob + OFS_RETURN+0);
				STOREREG(REG_EDX, glob + OFS_RETURN+1);
				STOREREG(REG_ECX, glob + OFS_RETURN+2);
			}
			
			//call leavefunction to get the return address
			
//			pushl progfuncs
			EmitByte(0x68);EmitAdr(progfuncs);
//			call PR_LeaveFunction
			EmitByte(0xe8);EmitFOffset(PR_LeaveFunction, 4);
//			add $4,%esp
			EmitByte(0x83);EmitByte(0xc4);EmitByte(0x04);
//			movl pr_depth,%edx
			EmitByte(0x8b);EmitByte(0x15);EmitAdr(&pr_depth);
//			cmp prinst->exitdepth,%edx
			EmitByte(0x3b);EmitByte(0x15);EmitAdr(&prinst->exitdepth);
//			je returntoc
			j1 = LocalJmp(OP_EQ_E);
//				mov statementoffsets[%eax*4],%eax
				EmitByte(0x8b);EmitByte(0x04);EmitByte(0x85);EmitAdr(statementoffsets+1);
//				jmp *eax
				EmitByte(0xff);EmitByte(0xe0);
//			returntoc:
			l1 = LocalLoc();
//			ret
			EmitByte(0xc3);

			LocalJmpLoc(j1,l1);
			break;

		//function calls
		case OP_CALL0:
		case OP_CALL1:
		case OP_CALL2:
		case OP_CALL3:
		case OP_CALL4:
		case OP_CALL5:
		case OP_CALL6:
		case OP_CALL7:
		case OP_CALL8:
		//save the state in place the rest of the engine can cope with
			//movl $i, pr_xstatement
			EmitByte(0xc7);EmitByte(0x05);EmitAdr(&pr_xstatement);Emit4Byte(i);
			//movl $(op[i].op-OP_CALL0), pr_argc
			EmitByte(0xc7);EmitByte(0x05);EmitAdr(&pr_argc);Emit4Byte(op[i].op-OP_CALL0);

		//figure out who we're calling, and what that involves
			//%eax = glob[A]
			LOADREG(glob + op[i].a, REG_EAX);
		//eax is now the func num

			//mov %eax,%ecx
			EmitByte(0x89); EmitByte(0xc1);
			//shr $24,%ecx
			EmitByte(0xc1); EmitByte(0xe9); EmitByte(0x18);
		//ecx is now the progs num for the new func

			//cmp %ecx,pr_typecurrent
			EmitByte(0x39); EmitByte(0x0d); EmitAdr(&pr_typecurrent);
			//je sameprogs
			j1 = LocalJmp(OP_EQ_I);
			{
				//can't handle switching progs

				//FIXME: recurse though PR_ExecuteProgram
				//push eax
				//push progfuncs
				//call PR_ExecuteProgram
				//add $8,%esp
				//remember to change the je above

				//err... exit depth? no idea
				EmitByte(0xcd);EmitByte(op[i].op);	//int $X


				//ret
				EmitByte(0xc3);
			}
			//sameprogs:
			l1 = LocalLoc();
			LocalJmpLoc(j1,l1);

			//andl $0x00ffffff, %eax
			EmitByte(0x25);Emit4Byte(0x00ffffff);
			
			//mov $sizeof(dfunction_t),%edx
			EmitByte(0xba);Emit4Byte(sizeof(dfunction_t));
			//mul %edx
			EmitByte(0xf7); EmitByte(0xe2);
			//add pr_functions,%eax
			EmitByte(0x05); EmitAdr(pr_functions);

		//eax is now the dfunction_t to be called
		//edx is clobbered.

			//mov (%eax),%edx
			EmitByte(0x8b);EmitByte(0x10);
		//edx is now the first statement number
			//cmp $0,%edx
			EmitByte(0x83);EmitByte(0xfa);EmitByte(0x00);
			//jl isabuiltin
			j1 = LocalJmp(OP_LE);
			{
				//push %ecx
				EmitByte(0x51);
				//push %eax
				EmitByte(0x50);
				//pushl progfuncs
				EmitByte(0x68);EmitAdr(progfuncs);
				//call PR_EnterFunction
				EmitByte(0xe8);EmitFOffset(PR_EnterFunction, 4);
				//sub $12,%esp
				EmitByte(0x83);EmitByte(0xc4);EmitByte(0xc);
		//eax is now the next statement number (first of the new function, usually equal to ecx, but not always)

				//jmp statementoffsets[%eax*4]
				EmitByte(0xff);EmitByte(0x24);EmitByte(0x85);EmitAdr(statementoffsets+1);
			}
			//isabuiltin:
			l1 = LocalLoc();
			LocalJmpLoc(j1,l1);

			//push current_progstate->globals
			EmitByte(0x68);EmitAdr(current_progstate->globals);
			//push progfuncs
			EmitByte(0x68);EmitAdr(progfuncs);
			//neg %edx
			EmitByte(0xf7);EmitByte(0xda);
			//call externs->globalbuiltins[%edx,4]
//FIXME: make sure this dereferences
			EmitByte(0xff);EmitByte(0x14);EmitByte(0x95);EmitAdr(externs->globalbuiltins);
			//add $8,%esp
			EmitByte(0x83);EmitByte(0xc4);EmitByte(0x8);

		//but that builtin might have been Abort()

			LOADREG(&prinst->continuestatement, REG_EAX);
			//cmp $-1,%eax
			EmitByte(0x83);EmitByte(0xf8);EmitByte(0xff);
			//je donebuiltincall
			j1 = LocalJmp(OP_EQ_I);
			{
				//mov $-1,prinst->continuestatement
				EmitByte(0xc7);EmitByte(0x05);EmitAdr(&prinst->continuestatement);Emit4Byte((unsigned int)-1);

				//jmp statementoffsets[%eax*4]
				EmitByte(0xff);EmitByte(0x24);EmitByte(0x85);EmitAdr(statementoffsets);
			}
			//donebuiltincall:
			l1 = LocalLoc();
			LocalJmpLoc(j1,l1);
			break;

		case OP_MUL_F:
			//flds glob[A]
			EmitByte(0xd9);EmitByte(0x05);EmitAdr(glob + op[i].a);
			//fmuls glob[B]
			EmitByte(0xd8);EmitByte(0x0d);EmitAdr(glob + op[i].b);
			//fstps glob[C]
			EmitByte(0xd9);EmitByte(0x1d);EmitAdr(glob + op[i].c);
			break;
		case OP_DIV_F:
			//flds glob[A]
			EmitByte(0xd9);EmitByte(0x05);EmitAdr(glob + op[i].a);
			//fdivs glob[B]
			EmitByte(0xd8);EmitByte(0x35);EmitAdr(glob + op[i].b);
			//fstps glob[C]
			EmitByte(0xd9);EmitByte(0x1d);EmitAdr(glob + op[i].c);
			break;
		case OP_ADD_F:
			//flds glob[A]
			EmitByte(0xd9);EmitByte(0x05);EmitAdr(glob + op[i].a);
			//fadds glob[B]
			EmitByte(0xd8);EmitByte(0x05);EmitAdr(glob + op[i].b);
			//fstps glob[C]
			EmitByte(0xd9);EmitByte(0x1d);EmitAdr(glob + op[i].c);
			break;
		case OP_SUB_F:
			//flds glob[A]
			EmitByte(0xd9);EmitByte(0x05);EmitAdr(glob + op[i].a);
			//fsubs glob[B]
			EmitByte(0xd8);EmitByte(0x25);EmitAdr(glob + op[i].b);
			//fstps glob[C]
			EmitByte(0xd9);EmitByte(0x1d);EmitAdr(glob + op[i].c);
			break;

		case OP_NOT_F:
			//flds glob[A]
			EmitByte(0xd9);EmitByte(0x05);EmitAdr(glob + op[i].a);
			//fldz
			EmitByte(0xd9);EmitByte(0xee);
			//fnstsw %ax
			EmitByte(0xdf);EmitByte(0xe0);
			//testb 0x40,%ah
			EmitByte(0xf6);EmitByte(0xc4);EmitByte(0x40);
			
			j1 = LocalJmp(OP_NE_F);
			{
				STOREF(1.0f, glob + op[i].c);
				j2 = LocalJmp(OP_GOTO);
			}
			{
				//noteq:
				l1 = LocalLoc();
				STOREF(0.0f, glob + op[i].c);
			}
			//end:
			l2 = LocalLoc();
			LocalJmpLoc(j1,l1);
			LocalJmpLoc(j2,l2);
			break;

		case OP_STORE_F:
		case OP_STORE_S:
		case OP_STORE_ENT:
		case OP_STORE_FLD:
		case OP_STORE_FNC:
			LOADREG(glob + op[i].a, REG_EAX);
			STOREREG(REG_EAX, glob + op[i].b);
			break;

		case OP_STORE_V:
			LOADREG(glob + op[i].a+0, REG_EAX);
			LOADREG(glob + op[i].a+1, REG_EDX);
			LOADREG(glob + op[i].a+2, REG_ECX);
			STOREREG(REG_EAX, glob + op[i].b+0);
			STOREREG(REG_EDX, glob + op[i].b+1);
			STOREREG(REG_ECX, glob + op[i].b+2);
			break;

		case OP_LOAD_F:
		case OP_LOAD_S:
		case OP_LOAD_ENT:
		case OP_LOAD_FLD:
		case OP_LOAD_FNC:
		case OP_LOAD_V:
		//a is the ent number, b is the field
		//c is the dest

			LOADREG(glob + op[i].a, REG_EAX);
			LOADREG(glob + op[i].b, REG_ECX);

		//FIXME: bound eax (ent number)
		//FIXME: bound ecx (field index)
			//mov (ebx,eax,4).%eax
			EmitByte(0x8b); EmitByte(0x04); EmitByte(0x83);
		//eax is now an edictrun_t
			//mov fields(,%eax,4),%edx
			EmitByte(0x8b);EmitByte(0x50);EmitByte((int)&((edictrun_t*)NULL)->fields);
		//edx is now the field array for that ent

			//mov fieldajust(%edx,%ecx,4),%eax	//offset = progfuncs->fieldadjust
			EmitByte(0x8b); EmitByte(0x84); EmitByte(0x8a); Emit4Byte(progfuncs->fieldadjust*4);

			STOREREG(REG_EAX, glob + op[i].c)

			if (op[i].op == OP_LOAD_V)
			{
				//mov fieldajust+4(%edx,%ecx,4),%eax	//offset = progfuncs->fieldadjust
				EmitByte(0x8b); EmitByte(0x84); EmitByte(0x8a); Emit4Byte(4+progfuncs->fieldadjust*4);
				STOREREG(REG_EAX, glob + op[i].c+1)

				//mov fieldajust+8(%edx,%ecx,4),%eax	//offset = progfuncs->fieldadjust
				EmitByte(0x8b); EmitByte(0x84); EmitByte(0x8a); Emit4Byte(8+progfuncs->fieldadjust*4);
				STOREREG(REG_EAX, glob + op[i].c+2)
			}
			break;

		case OP_ADDRESS:
			//a is the ent number, b is the field
		//c is the dest

			LOADREG(glob + op[i].a, REG_EAX);
			LOADREG(glob + op[i].b, REG_ECX);

		//FIXME: bound eax (ent number)
		//FIXME: bound ecx (field index)
			//mov (ebx,eax,4).%eax
			EmitByte(0x8b); EmitByte(0x04); EmitByte(0x83);
		//eax is now an edictrun_t
			//mov fields(,%eax,4),%edx
			EmitByte(0x8b);EmitByte(0x50);EmitByte((int)&((edictrun_t*)NULL)->fields);
		//edx is now the field array for that ent
			//mov fieldajust(%edx,%ecx,4),%eax	//offset = progfuncs->fieldadjust
			//EmitByte(0x8d); EmitByte(0x84); EmitByte(0x8a); EmitByte(progfuncs->fieldadjust*4);
			EmitByte(0x8d); EmitByte(0x84); EmitByte(0x8a); Emit4Byte(progfuncs->fieldadjust*4);
			STOREREG(REG_EAX, glob + op[i].c);
			break;

		case OP_STOREP_F:
		case OP_STOREP_S:
		case OP_STOREP_ENT:
		case OP_STOREP_FLD:
		case OP_STOREP_FNC:
			LOADREG(glob + op[i].a, REG_EAX);
			LOADREG(glob + op[i].b, REG_ECX);
			//mov %eax,(%ecx)
			EmitByte(0x89);EmitByte(0x01);
			break;

		case OP_STOREP_V:
			LOADREG(glob + op[i].b, REG_ECX);

			LOADREG(glob + op[i].a+0, REG_EAX);
			//mov %eax,0(%ecx)
			EmitByte(0x89);EmitByte(0x01);

			LOADREG(glob + op[i].a+1, REG_EAX);
			//mov %eax,4(%ecx)
			EmitByte(0x89);EmitByte(0x41);EmitByte(0x04);

			LOADREG(glob + op[i].a+2, REG_EAX);
			//mov %eax,8(%ecx)
			EmitByte(0x89);EmitByte(0x41);EmitByte(0x08);
			break;

		case OP_NE_I:
		case OP_NE_E:
		case OP_NE_FNC:
		case OP_EQ_I:
		case OP_EQ_E:
		case OP_EQ_FNC:
			//integer equality
			LOADREG(glob + op[i].a, REG_EAX);

			//cmp glob[B],%eax
			EmitByte(0x3b); EmitByte(0x04); EmitByte(0x25); EmitAdr(glob + op[i].b);
			j1 = LocalJmp(op[i].op);
			{
				STOREF(1.0f, glob + op[i].c);
				j2 = LocalJmp(OP_GOTO);
			}
			{
				l1 = LocalLoc();
				STOREF(0.0f, glob + op[i].c);
			}
			l2 = LocalLoc();
			LocalJmpLoc(j1,l1);
			LocalJmpLoc(j2,l2);
			break;

		case OP_NOT_I:
		case OP_NOT_ENT:
		case OP_NOT_FNC:
			//cmp glob[B],$0
			EmitByte(0x83); EmitByte(0x3d); EmitAdr(glob + op[i].a); EmitByte(0x00); 
			j1 = LocalJmp(OP_NE_I);
			{
				STOREF(1.0f, glob + op[i].c);
				j2 = LocalJmp(OP_GOTO);
			}
			{
				l1 = LocalLoc();
				STOREF(0.0f, glob + op[i].c);
			}
			l2 = LocalLoc();
			LocalJmpLoc(j1,l1);
			LocalJmpLoc(j2,l2);
			break;

		case OP_BITOR:	//floats...
			//flds glob[A]
			EmitByte(0xd9); EmitByte(0x05);EmitAdr(glob + op[i].a);
			//flds glob[B]
			EmitByte(0xd9); EmitByte(0x05);EmitAdr(glob + op[i].b);
			//fistp tb
			EmitByte(0xdf); EmitByte(0x1d);EmitAdr(&tb);
			//fistp ta
			EmitByte(0xdf); EmitByte(0x1d);EmitAdr(&ta);
			LOADREG(&ta, REG_EAX)
			//or tb,%eax
			EmitByte(0x09); EmitByte(0x05);EmitAdr(&tb);
			STOREREG(REG_EAX, &tb)
			//fild tb
			EmitByte(0xdf); EmitByte(0x05);EmitAdr(&tb);
			//fstps glob[C]
			EmitByte(0xd9); EmitByte(0x1d);EmitAdr(glob + op[i].c);
			break;

		case OP_BITAND:
			//flds glob[A]
			EmitByte(0xd9); EmitByte(0x05);EmitAdr(glob + op[i].a);
			//flds glob[B]
			EmitByte(0xd9); EmitByte(0x05);EmitAdr(glob + op[i].b);
			//fistp tb
			EmitByte(0xdf); EmitByte(0x1d);EmitAdr(&tb);
			//fistp ta
			EmitByte(0xdf); EmitByte(0x1d);EmitAdr(&ta);
			/*two args are now at ta and tb*/
			LOADREG(&ta, REG_EAX)
			//and tb,%eax
			EmitByte(0x21); EmitByte(0x05);EmitAdr(&tb);
			STOREREG(REG_EAX, &tb)
			/*we just wrote the int value to tb, convert that to a float and store it at c*/
			//fild tb
			EmitByte(0xdf); EmitByte(0x05);EmitAdr(&tb);
			//fstps glob[C]
			EmitByte(0xd9); EmitByte(0x1d);EmitAdr(glob + op[i].c);
			break;

		case OP_AND:
			//test floats properly, so we don't get confused with -0.0

			//flds	glob[A]
			EmitByte(0xd9); EmitByte(0x05); EmitAdr(glob + op[i].a);
			//fcomps	nullfloat
			EmitByte(0xd8); EmitByte(0x1d); EmitAdr(&nullfloat);
			//fnstsw	%ax
			EmitByte(0xdf); EmitByte(0xe0);
			//test	$0x40,%ah
			EmitByte(0xf6); EmitByte(0xc4);EmitByte(0x40);
			//je onefalse
			EmitByte(0x75); EmitByte(0x1f);

			//flds	glob[B]
			EmitByte(0xd9); EmitByte(0x05); EmitAdr(glob + op[i].b);
			//fcomps	nullfloat
			EmitByte(0xd8); EmitByte(0x1d); EmitAdr(&nullfloat);
			//fnstsw	%ax
			EmitByte(0xdf); EmitByte(0xe0);
			//test	$0x40,%ah
			EmitByte(0xf6); EmitByte(0xc4);EmitByte(0x40);
			//jne onefalse
			EmitByte(0x75); EmitByte(0x0c);

			//mov float0,glob[C]
			EmitByte(0xc7); EmitByte(0x05); EmitAdr(glob + op[i].c); EmitFloat(1.0f);
			//jmp done
			EmitByte(0xeb); EmitByte(0x0a);

			//onefalse:
			//mov float1,glob[C]
			EmitByte(0xc7); EmitByte(0x05); EmitAdr(glob + op[i].c); EmitFloat(0.0f);
			//done:
			break;
		case OP_OR:
			//test floats properly, so we don't get confused with -0.0

			//flds	glob[A]
			EmitByte(0xd9); EmitByte(0x05); EmitAdr(glob + op[i].a);
			//fcomps	nullfloat
			EmitByte(0xd8); EmitByte(0x1d); EmitAdr(&nullfloat);
			//fnstsw	%ax
			EmitByte(0xdf); EmitByte(0xe0);
			//test	$0x40,%ah
			EmitByte(0xf6); EmitByte(0xc4);EmitByte(0x40);
			//je onetrue
			EmitByte(0x74); EmitByte(0x1f);

			//flds	glob[B]
			EmitByte(0xd9); EmitByte(0x05); EmitAdr(glob + op[i].b);
			//fcomps	nullfloat
			EmitByte(0xd8); EmitByte(0x1d); EmitAdr(&nullfloat);
			//fnstsw	%ax
			EmitByte(0xdf); EmitByte(0xe0);
			//test	$0x40,%ah
			EmitByte(0xf6); EmitByte(0xc4);EmitByte(0x40);
			//je onetrue
			EmitByte(0x74); EmitByte(0x0c);

			//mov float0,glob[C]
			EmitByte(0xc7); EmitByte(0x05); EmitAdr(glob + op[i].c); EmitFloat(0.0f);
			//jmp done
			EmitByte(0xeb); EmitByte(0x0a);

			//onetrue:
			//mov float1,glob[C]
			EmitByte(0xc7); EmitByte(0x05); EmitAdr(glob + op[i].c); EmitFloat(1.0f);
			//done:
			break;

		case OP_EQ_S:
		case OP_NE_S:
			{
				void *j0b, *j1b, *j1c;
			//put a in ecx
			LOADREG(glob + op[i].a, REG_ECX);
			//put b in edi
			LOADREG(glob + op[i].b, REG_EDI);
/*
			//early out if they're equal
			//cmp %ecx,%edi
			EmitByte(0x39); EmitByte(0xc0 | (REG_EDI<<3) | REG_ECX);
			j1c = LocalJmp(OP_EQ_S);

			//if a is 0, check if b is ""
			//jecxz ais0
			EmitByte(0xe3); EmitByte(0x1a);

			//if b is 0, check if a is ""
  			//cmp $0,%edi
			EmitByte(0x83); EmitByte(0xff); EmitByte(0x00);
			//jne bnot0
			EmitByte(0x75); EmitByte(0x2a);
			{
				//push a
				EmitByte(0x51);
				//push progfuncs
				EmitByte(0x68); EmitAdr(progfuncs);
				//call PR_StringToNative
				EmitByte(0xe8); EmitFOffset(PR_StringToNative,4);
				//add $8,%esp
				EmitByte(0x83); EmitByte(0xc4); EmitByte(0x08);
				//cmpb $0,(%eax)
				EmitByte(0x80); EmitByte(0x38); EmitByte(0x00);
				j1b = LocalJmp(OP_EQ_S);
				j0b = LocalJmp(OP_GOTO);
			}

			//ais0:
			{
				//push edi
				EmitByte(0x57);
				//push progfuncs
				EmitByte(0x68); EmitAdr(progfuncs);
				//call PR_StringToNative
				EmitByte(0xe8); EmitFOffset(PR_StringToNative,4);
				//add $8,%esp
				EmitByte(0x83); EmitByte(0xc4); EmitByte(0x08);
				//cmpb $0,(%eax)
				EmitByte(0x80); EmitByte(0x38); EmitByte(0x00);
				//je _true
				EmitByte(0x74); EmitByte(0x36);
				//jmp _false
				EmitByte(0xeb); EmitByte(0x28);
			}
			//bnot0:
*/
LOADREG(glob + op[i].a, REG_ECX);
			//push ecx
			EmitByte(0x51);
			//push progfuncs
			EmitByte(0x68); EmitAdr(progfuncs);
			//call PR_StringToNative
			EmitByte(0xe8); EmitFOffset(PR_StringToNative,4);
			//push %eax
			EmitByte(0x50);

LOADREG(glob + op[i].b, REG_EDI);
			//push %edi
			EmitByte(0x57);
			//push progfuncs
			EmitByte(0x68); EmitAdr(progfuncs);
			//call PR_StringToNative
			EmitByte(0xe8); EmitFOffset(PR_StringToNative,4);
			//add $8,%esp
			EmitByte(0x83); EmitByte(0xc4); EmitByte(0x08);


			//push %eax
			EmitByte(0x50);
			//call strcmp
			EmitByte(0xe8); EmitFOffset(strcmp,4);
			//add $16,%esp
			EmitByte(0x83); EmitByte(0xc4); EmitByte(0x10);

			//cmp $0,%eax
			EmitByte(0x83); EmitByte(0xf8); EmitByte(0x00);
			j1 = LocalJmp(OP_EQ_S);
			{
				l0 = LocalLoc();
				STOREF((op[i].op == OP_NE_S)?1.0f:0.0f, glob + op[i].c);
				j2 = LocalJmp(OP_GOTO);
			}
			{
				l1 = LocalLoc();
				STOREF((op[i].op == OP_NE_S)?0.0f:1.0f, glob + op[i].c);
			}
			l2 = LocalLoc();

//			LocalJmpLoc(j0b, l0);
			LocalJmpLoc(j1, l1);
//			LocalJmpLoc(j1b, l1);
			LocalJmpLoc(j2, l2);
			}
			break;

		case OP_NOT_S:
			LOADREG(glob + op[i].a, REG_EAX)

			//cmp $0,%eax
			EmitByte(0x83); EmitByte(0xf8); EmitByte(0x00);
			j2 = LocalJmp(OP_EQ_S);

			//push %eax
			EmitByte(0x50);
			//push progfuncs
			EmitByte(0x68); EmitAdr(progfuncs);
			//call PR_StringToNative
			EmitByte(0xe8); EmitFOffset(PR_StringToNative,4);
			//add $8,%esp
			EmitByte(0x83); EmitByte(0xc4); EmitByte(0x08);

			//cmpb $0,(%eax)
			EmitByte(0x80); EmitByte(0x38); EmitByte(0x00);
			j1 = LocalJmp(OP_EQ_S);
			{
				STOREF(0.0f, glob + op[i].c);
				j0 = LocalJmp(OP_GOTO);
			}
			{
				l1 = LocalLoc();
				STOREF(1.0f, glob + op[i].c);
			}
			l2 = LocalLoc();
			LocalJmpLoc(j2, l1);
			LocalJmpLoc(j1, l1);
			LocalJmpLoc(j0, l2);
			break;

		case OP_ADD_V:
			//flds glob[A]
			EmitByte(0xd9);EmitByte(0x05);EmitAdr(glob + op[i].a+0);
			//fadds glob[B]
			EmitByte(0xd8);EmitByte(0x05);EmitAdr(glob + op[i].b+0);
			//fstps glob[C]
			EmitByte(0xd9);EmitByte(0x1d);EmitAdr(glob + op[i].c+0);

			//flds glob[A]
			EmitByte(0xd9);EmitByte(0x05);EmitAdr(glob + op[i].a+1);
			//fadds glob[B]
			EmitByte(0xd8);EmitByte(0x05);EmitAdr(glob + op[i].b+1);
			//fstps glob[C]
			EmitByte(0xd9);EmitByte(0x1d);EmitAdr(glob + op[i].c+1);

			//flds glob[A]
			EmitByte(0xd9);EmitByte(0x05);EmitAdr(glob + op[i].a+2);
			//fadds glob[B]
			EmitByte(0xd8);EmitByte(0x05);EmitAdr(glob + op[i].b+2);
			//fstps glob[C]
			EmitByte(0xd9);EmitByte(0x1d);EmitAdr(glob + op[i].c+2);
			break;
		case OP_SUB_V:
			//flds glob[A]
			EmitByte(0xd9);EmitByte(0x05);EmitAdr(glob + op[i].a+0);
			//fsubs glob[B]
			EmitByte(0xd8);EmitByte(0x25);EmitAdr(glob + op[i].b+0);
			//fstps glob[C]
			EmitByte(0xd9);EmitByte(0x1d);EmitAdr(glob + op[i].c+0);

			//flds glob[A]
			EmitByte(0xd9);EmitByte(0x05);EmitAdr(glob + op[i].a+1);
			//fsubs glob[B]
			EmitByte(0xd8);EmitByte(0x25);EmitAdr(glob + op[i].b+1);
			//fstps glob[C]
			EmitByte(0xd9);EmitByte(0x1d);EmitAdr(glob + op[i].c+1);

			//flds glob[A]
			EmitByte(0xd9);EmitByte(0x05);EmitAdr(glob + op[i].a+2);
			//fsubs glob[B]
			EmitByte(0xd8);EmitByte(0x25);EmitAdr(glob + op[i].b+2);
			//fstps glob[C]
			EmitByte(0xd9);EmitByte(0x1d);EmitAdr(glob + op[i].c+2);
			break;

		case OP_MUL_V:
			//this is actually a dotproduct
			//flds glob[A]
			EmitByte(0xd9);EmitByte(0x05);EmitAdr(glob + op[i].a+0);
			//fmuls glob[B]
			EmitByte(0xd8);EmitByte(0x0d);EmitAdr(glob + op[i].b+0);

			//flds glob[A]
			EmitByte(0xd9);EmitByte(0x05);EmitAdr(glob + op[i].a+1);
			//fmuls glob[B]
			EmitByte(0xd8);EmitByte(0x0d);EmitAdr(glob + op[i].b+1);

			//flds glob[A]
			EmitByte(0xd9);EmitByte(0x05);EmitAdr(glob + op[i].a+2);
			//fmuls glob[B]
			EmitByte(0xd8);EmitByte(0x0d);EmitAdr(glob + op[i].b+2);

			//faddp
			EmitByte(0xde);EmitByte(0xc1);
			//faddp
			EmitByte(0xde);EmitByte(0xc1);

			//fstps glob[C]
			EmitByte(0xd9);EmitByte(0x1d);EmitAdr(glob + op[i].c);
			break;

		case OP_EQ_F:
		case OP_NE_F:
		case OP_LE:
		case OP_GE:
		case OP_LT:
		case OP_GT:
			//flds glob[A]
			EmitByte(0xd9);EmitByte(0x05);EmitAdr(glob + op[i].a);
			//flds glob[B]
			EmitByte(0xd9);EmitByte(0x05);EmitAdr(glob + op[i].b);
			//fcomip %st(1),%st
			EmitByte(0xdf);EmitByte(0xe9);
			//fstp %st(0)	(aka: pop)
			EmitByte(0xdd);EmitByte(0xd8);

			j1 = LocalJmp(op[i].op);
			{
				STOREF(0.0f, glob + op[i].c);
				j2 = LocalJmp(OP_GOTO);
			}
			{
				l1 = LocalLoc();
				STOREF(1.0f, glob + op[i].c);
			}
			l2 = LocalLoc();
			LocalJmpLoc(j1,l1);
			LocalJmpLoc(j2,l2);
			break;

		case OP_MUL_FV:
		case OP_MUL_VF:
			//
			{
				int v;
				int f;
				if (op[i].op == OP_MUL_FV)
				{
					f = op[i].a;
					v = op[i].b;
				}
				else
				{
					v = op[i].a;
					f = op[i].b;
				}

				//flds glob[F]
				EmitByte(0xd9);EmitByte(0x05);EmitAdr(glob + f);

				//flds glob[V0]
				EmitByte(0xd8);EmitByte(0x0d);EmitAdr(glob + v+0);
				//fmul st(1)
				EmitByte(0xd8);EmitByte(0xc9);
				//fstps glob[C]
				EmitByte(0xd9);EmitByte(0x1d);EmitAdr(glob + op[i].c+0);

				//flds glob[V0]
				EmitByte(0xd8);EmitByte(0x0d);EmitAdr(glob + v+1);
				//fmul st(1)
				EmitByte(0xd8);EmitByte(0xc9);
				//fstps glob[C]
				EmitByte(0xd9);EmitByte(0x1d);EmitAdr(glob + op[i].c+1);

				//flds glob[V0]
				EmitByte(0xd8);EmitByte(0x0d);EmitAdr(glob + v+2);
				//fmul st(1)
				EmitByte(0xd8);EmitByte(0xc9);
				//fstps glob[C]
				EmitByte(0xd9);EmitByte(0x1d);EmitAdr(glob + op[i].c+2);

				//fstp %st(0)	(aka: pop)
				EmitByte(0xdd);EmitByte(0xd8);
			}
			break;

		case OP_STATE:
			//externs->stateop(progfuncs, OPA->_float, OPB->function);
			//push b
			EmitByte(0xff);EmitByte(0x35);EmitAdr(glob + op[i].b);
			//push a
			EmitByte(0xff);EmitByte(0x35);EmitAdr(glob + op[i].a);
			//push $progfuncs
			EmitByte(0x68); EmitAdr(progfuncs);
			//call externs->stateop
			EmitByte(0xe8); EmitFOffset(externs->stateop, 4);
			//add $12,%esp
			EmitByte(0x83); EmitByte(0xc4); EmitByte(0x0c);
			break;
#if 1
/*		case OP_NOT_V:
			//flds 0
			//flds glob[A+0]
			//fcomip %st(1),%st
			//jne _true
			//flds glob[A+1]
			//fcomip %st(1),%st
			//jne _true
			//flds glob[A+1]
			//fcomip %st(1),%st
			//jne _true
			//mov 1,C
			//jmp done
			//_true:
			//mov 0,C
			//done:
			break;
*/
		case OP_NE_V:
		case OP_EQ_V:
			//flds glob[A]
			EmitByte(0xd9);EmitByte(0x05);EmitAdr(glob + op[i].a+0);
			//flds glob[B]
			EmitByte(0xd9);EmitByte(0x05);EmitAdr(glob + op[i].b+0);
			//fcomip %st(1),%st
			EmitByte(0xdf);EmitByte(0xe9);
			//fstp %st(0)	(aka: pop)
			EmitByte(0xdd);EmitByte(0xd8);

			//jncc _true
			if (op[i].op == OP_NE_V)
				EmitByte(0x74);	//je
			else
				EmitByte(0x75);	//jne
			EmitByte(0x0c);
//_false0:
			//mov 0.0f,c
			EmitByte(0xc7); EmitByte(0x05); EmitAdr(glob + op[i].c); EmitFloat(1.0f);
			//jmp done
			EmitByte(0xeb); EmitByte(0x0a);


//_true:
			//mov 1.0f,c
			EmitByte(0xc7); EmitByte(0x05); EmitAdr(glob + op[i].c); EmitFloat(0.0f);
//_done:
			break;

		case OP_NOT_V:
			EmitByte(0xcd);EmitByte(op[i].op);
			printf("QCJIT: instruction %i is not implemented\n", op[i].op);
			break;
#endif
		default:
			printf("QCJIT: Extended instruction set %i is not supported, not using jit.\n", op[i].op);


			free(statementjumps);	//[MAX_STATEMENTS]
			free(statementoffsets); //[MAX_STATEMENTS]
			free(code);
			statementoffsets = NULL;
			return false;
		}
	}

	FixupJumps();

#ifdef _WIN32
	{
		DWORD old;

		//this memory is on the heap.
		//this means that we must maintain read/write protection, or libc will crash us
		VirtualProtect(code, codesize, PAGE_EXECUTE_READWRITE, &old);
	}
#endif

//	externs->WriteFile("jit.x86", code, codesize);

	return true;
}

void PR_EnterJIT(progfuncs_t *progfuncs, int statement)
{
#ifdef __GNUC__
	//call, it clobbers pretty much everything.
	asm("call *%0" :: "r"(statementoffsets[statement+1]),"b"(prinst->edicttable):"cc","memory","eax","ecx","edx");
#elif defined(_MSC_VER)
	void *entry = statementoffsets[statement+1];
	void *edicttable = prinst->edicttable;
	__asm {
		pushad
		mov eax,entry
		mov ebx,edicttable
		call eax
		popad
	}
#else
	#error "Sorry, no idea how to enter assembler safely for your compiler"
#endif
}
#endif
