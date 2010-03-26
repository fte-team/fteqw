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

int PR_LeaveFunction (progfuncs_t *progfuncs);
int PR_EnterFunction (progfuncs_t *progfuncs, dfunction_t *f, int progsnum);

pbool PR_GenerateJit(progfuncs_t *progfuncs)
{
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
				EmitByte(0x31);EmitByte(0xc0);
				EmitByte(0xa3);EmitAdr(glob + OFS_RETURN+0);
				EmitByte(0xa3);EmitAdr(glob + OFS_RETURN+1);
				EmitByte(0xa3);EmitAdr(glob + OFS_RETURN+2);
			}
			else
			{
				//movl glob[A+0],eax
				EmitByte(0xa1);EmitAdr(glob + op[i].a+0);
				//movl glob[A+0],edx
				EmitByte(0x8b);EmitByte(0x0d);EmitAdr(glob + op[i].a+1);
				//movl glob[A+0],ecx
				EmitByte(0x8b);EmitByte(0x15);EmitAdr(glob + op[i].a+2);
				//movl eax, glob[OFS_RET+0]
				EmitByte(0xa3);EmitAdr(glob + OFS_RETURN+0);
				//movl edx, glob[OFS_RET+0]
				EmitByte(0x89);EmitByte(0x15);EmitAdr(glob + OFS_RETURN+1);
				//movl ecx, glob[OFS_RET+0]
				EmitByte(0x89);EmitByte(0x15);EmitAdr(glob + OFS_RETURN+2);
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
			EmitByte(0x74);EmitByte(0x09);
//			mov statementoffsets[%eax*4],%eax
			EmitByte(0x8b);EmitByte(0x04);EmitByte(0x85);EmitAdr(statementoffsets+1);
//			jmp eax
			EmitByte(0xff);EmitByte(0xe0);
//			returntoc:
//			ret
			EmitByte(0xc3);
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
			EmitByte(0xa1); EmitAdr(glob + op[i].a);
		//eax is now the func num

			//mov %eax,%ecx
			EmitByte(0x89); EmitByte(0xc1);
			//shr $24,%ecx
			EmitByte(0xc1); EmitByte(0xe9); EmitByte(0x18);
		//ecx is now the progs num for the new func

			//cmp %ecx,pr_typecurrent
			EmitByte(0x39); EmitByte(0x0d); EmitAdr(&pr_typecurrent);
			//je sameprogs
			EmitByte(0x74); EmitByte(0x3);
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
			EmitByte(0x7c);EmitByte(22);
	
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

			//mov prinst->continuestatement,%eax
			EmitByte(0xa1);EmitAdr(&prinst->continuestatement);
		//eax is now prinst->continuestatement

			//cmp $-1,%eax
			EmitByte(0x83);EmitByte(0xf8);EmitByte(0xff);
			//je donebuiltincall
			EmitByte(0x74);EmitByte(10+8);
			{
EmitByte(0xcc);
				//jmp statementoffsets[%eax*4]
				EmitByte(0xff);EmitByte(0x24);EmitByte(0x85);EmitAdr(statementoffsets+1);

				//mov $-1,prinst->continuestatement
				EmitByte(0xc7);EmitByte(0x05);EmitAdr(&prinst->continuestatement+1);Emit4Byte((unsigned int)-1);
			}
			//donebuiltincall:
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
			//je noteq
			EmitByte(0x74);EmitByte(0x0c);
			//movl 1.0f,glob[C]
			EmitByte(0xc7);EmitByte(0x05);EmitAdr(glob + op[i].c);EmitFloat(0.0f);
			//jmp end
			EmitByte(0xeb);EmitByte(0x0a);
			//noteq:
			//movl 0.0f,glob[C]
			EmitByte(0xc7);EmitByte(0x05);EmitAdr(glob + op[i].c);EmitFloat(1.0f);
			//end:
			break;

		case OP_STORE_F:
		case OP_STORE_S:
		case OP_STORE_ENT:
		case OP_STORE_FLD:
		case OP_STORE_FNC:
			//movl glob[A],eax
			EmitByte(0xa1);EmitAdr(glob + op[i].a);
			//movl eax,glob[B]
			EmitByte(0xa3);EmitAdr(glob + op[i].b);
			break;

		case OP_STORE_V:
			//movl glob[A+0],eax
			EmitByte(0xa1);EmitAdr(glob + op[i].a+0);
			//movl glob[A+1],edx
			EmitByte(0x8b);EmitByte(0x0d);EmitAdr(glob + op[i].a+1);
			//movl glob[A+2],ecx
			EmitByte(0x8b);EmitByte(0x15);EmitAdr(glob + op[i].a+2);

			//movl eax, glob[B+0]
			EmitByte(0xa3);EmitAdr(glob + op[i].b+0);
			//movl edx, glob[B+1]
			EmitByte(0x89);EmitByte(0x15);EmitAdr(glob + op[i].b+1);
			//movl ecx, glob[B+2]
			EmitByte(0x89);EmitByte(0x15);EmitAdr(glob + op[i].b+2);
			break;

		case OP_LOAD_F:
		case OP_LOAD_S:
		case OP_LOAD_ENT:
		case OP_LOAD_FLD:
		case OP_LOAD_FNC:
		case OP_LOAD_V:
		//a is the ent number, b is the field
		//c is the dest

			//movl glob[A+0],eax
			EmitByte(0xa1);EmitAdr(glob + op[i].a);
			//mov glob[B],ecx
			EmitByte(0x8b); EmitByte(0x0d);EmitAdr(glob + op[i].b);
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
			//mov edx,glob[C]
			EmitByte(0xa3);EmitAdr(glob + op[i].c);

			if (op[i].op == OP_LOAD_V)
			{
				//mov fieldajust+4(%edx,%ecx,4),%eax	//offset = progfuncs->fieldadjust
				EmitByte(0x8b); EmitByte(0x84); EmitByte(0x8a); Emit4Byte(4+progfuncs->fieldadjust*4);
				//mov edx,glob[C+1]
				EmitByte(0xa3);EmitAdr(glob + op[i].c+1);

				//mov fieldajust+8(%edx,%ecx,4),%eax	//offset = progfuncs->fieldadjust
				EmitByte(0x8b); EmitByte(0x84); EmitByte(0x8a); Emit4Byte(4+progfuncs->fieldadjust*4);
				//mov edx,glob[C+1]
				EmitByte(0xa3);EmitAdr(glob + op[i].c+2);
			}
			break;

		case OP_ADDRESS:
			//a is the ent number, b is the field
		//c is the dest

			//movl glob[A+0],eax
			EmitByte(0xa1);EmitAdr(glob + op[i].a);
			//mov glob[B],ecx
			EmitByte(0x8b); EmitByte(0x0d);EmitAdr(glob + op[i].b);
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
			//mov edx,glob[C]
			EmitByte(0xa3);EmitAdr(glob + op[i].c);
			break;

		case OP_STOREP_F:
		case OP_STOREP_S:
		case OP_STOREP_ENT:
		case OP_STOREP_FLD:
		case OP_STOREP_FNC:
			//movl glob[A],eax
			EmitByte(0xa1);EmitAdr(glob + op[i].a);
			//mov glob[B],ecx
			EmitByte(0x8b); EmitByte(0x0d);EmitAdr(glob + op[i].b);
			//mov %eax,(%ecx)
			EmitByte(0x89);EmitByte(0x01);
			break;

		case OP_STOREP_V:
			//mov glob[B],ecx
			EmitByte(0x8b); EmitByte(0x0d);EmitAdr(glob + op[i].b);
			//movl glob[A],eax
			EmitByte(0xa1);EmitAdr(glob + op[i].a+0);
			//mov %eax,0(%ecx)
			EmitByte(0x89);EmitByte(0x01);
			//movl glob[A],eax
			EmitByte(0xa1);EmitAdr(glob + op[i].a+0);
			//mov %eax,4(%ecx)
			EmitByte(0x89);EmitByte(0x41);EmitByte(0x04);
			//movl glob[A],eax
			EmitByte(0xa1);EmitAdr(glob + op[i].a+0);
			//mov %eax,8(%ecx)
			EmitByte(0x89);EmitByte(0x41);EmitByte(0x08);
			break;

		case OP_EQ_E:
		case OP_EQ_FNC:
			//integer equality
			//movl glob[A],%eax
			EmitByte(0xa1);EmitAdr(glob + op[i].a);
			//cmp glob[B],%eax
			EmitByte(0x3b); EmitByte(0x0f); EmitAdr(glob + op[i].b);
			//je 12
			EmitByte(0x74);EmitByte(0x0c);
			//mov 0.0f,glob[C]
			EmitByte(0xc7);EmitByte(0x05); EmitAdr(glob + op[i].a);EmitFloat(0.0f);
			//jmp 10
			EmitByte(0xeb);EmitByte(0x0a);
			//mov 1.0f,glob[C]
			EmitByte(0xc7);EmitByte(0x05); EmitAdr(glob + op[i].a);EmitFloat(1.0f);
			break;

		case OP_NE_E:
		case OP_NE_FNC:
			//integer equality
			//movl glob[A],%eax
			EmitByte(0xa1);EmitAdr(glob + op[i].a);
			//cmp glob[B],%eax
			EmitByte(0x3b); EmitByte(0x0f); EmitAdr(glob + op[i].b);
			//je 12
			EmitByte(0x74);EmitByte(0x0c);
			//mov 0.0f,glob[C]
			EmitByte(0xc7);EmitByte(0x05); EmitAdr(glob + op[i].a);EmitFloat(1.0f);
			//jmp 10
			EmitByte(0xeb);EmitByte(0x0a);
			//mov 1.0f,glob[C]
			EmitByte(0xc7);EmitByte(0x05); EmitAdr(glob + op[i].a);EmitFloat(0.0f);
			break;

		case OP_NOT_ENT:
		case OP_NOT_FNC:
			//cmp glob[B],%eax
			EmitByte(0x8c); EmitByte(0x3d); EmitAdr(glob + op[i].a);EmitByte(0x00);
			//je 12
			EmitByte(0x74);EmitByte(0x0c);
			//mov 0.0f,glob[C]
			EmitByte(0xc7);EmitByte(0x05); EmitAdr(glob + op[i].a);EmitFloat(0.0f);
			//jmp 10
			EmitByte(0xeb);EmitByte(0x0a);
			//mov 1.0f,glob[C]
			EmitByte(0xc7);EmitByte(0x05); EmitAdr(glob + op[i].c);EmitFloat(1.0f);
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
			//mov ta,%eax
			EmitByte(0xa1); EmitAdr(&ta);
			//and tb,%eax
			EmitByte(0x09); EmitByte(0x05);EmitAdr(&tb);
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
			//mov ta,%eax
			EmitByte(0xa1); EmitAdr(&ta);
			//and tb,%eax
			EmitByte(0x21); EmitByte(0x05);EmitAdr(&tb);
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
			//put a in ecx
			//put b in edi
			//mov a,%ecx
			EmitByte(0x8b); EmitByte(0x0d); EmitAdr(glob + op[i].a);
			//mov b,%edi
			EmitByte(0x8b); EmitByte(0x3d); EmitAdr(glob + op[i].b);

			//early out if they're equal
			//cmp %ecx,%edi
			EmitByte(0x39); EmitByte(0xd1);
			//je _true
			EmitByte(0x74); EmitByte(0x68);

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
				//je _true
				EmitByte(0x74); EmitByte(0x4b);
				//jmp _false
				EmitByte(0xeb); EmitByte(0x3d);

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
			}
			//bnot0:

			//push ecx
			EmitByte(0x51);
			//push progfuncs
			EmitByte(0x68); EmitAdr(progfuncs);
			//call PR_StringToNative
			EmitByte(0xe8); EmitFOffset(PR_StringToNative,4);
			//push %eax
			EmitByte(0x50);

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
			//je _true
			EmitByte(0x74); EmitByte(0x0c);
//_false:
			//mov 0.0f,c
			EmitByte(0xc7); EmitByte(0x05); EmitAdr(glob + op[i].c); EmitFloat((op[i].op == OP_NE_S)?1.0f:0.0f);
			//jmp done
			EmitByte(0xeb); EmitByte(0x0a);
//_true:
			//mov 1.0f,c
			EmitByte(0xc7); EmitByte(0x05); EmitAdr(glob + op[i].c); EmitFloat((op[i].op == OP_NE_S)?0.0f:1.0f);
//_done:
			break;

		case OP_NOT_S:
			//mov A,%eax
			EmitByte(0xa1);EmitAdr(glob + op[i].a);
			//cmp $0,%eax
			EmitByte(0x83); EmitByte(0xf8); EmitByte(0x00);
			//je _true
			EmitByte(0x74); EmitByte(0x1f);
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
			//je _true
			EmitByte(0x74); EmitByte(0x0c);
//_false:
			//mov 0.0f,c
			EmitByte(0xc7); EmitByte(0x05); EmitAdr(glob + op[i].c); EmitFloat(0.0f);
			//jmp done
			EmitByte(0xeb); EmitByte(0x0a);
//_true:
			//mov 1.0f,c
			EmitByte(0xc7); EmitByte(0x05); EmitAdr(glob + op[i].c); EmitFloat(1.0f);
//_done:
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

			//jcc _true
			if (op[i].op == OP_LE)
				EmitByte(0x7e);	//jle
			else if (op[i].op == OP_GE)
				EmitByte(0x7d);	//jge
			else if (op[i].op == OP_LT)
				EmitByte(0x7c);	//jl
			else if (op[i].op == OP_GT)
				EmitByte(0x7f);	//jg
			else if (op[i].op == OP_NE_F)
				EmitByte(0x75);	//jne
			else
				EmitByte(0x74);	//je
			EmitByte(0x0c);
//_false:
			//mov 0.0f,c
			EmitByte(0xc7); EmitByte(0x05); EmitAdr(glob + op[i].c); EmitFloat(0.0f);
			//jmp done
			EmitByte(0xeb); EmitByte(0x0a);
//_true:
			//mov 1.0f,c
			EmitByte(0xc7); EmitByte(0x05); EmitAdr(glob + op[i].c); EmitFloat(1.0f);
//_done:
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
#if 0
		case OP_NOT_V:
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


		case OP_EQ_V:
			EmitByte(0xcd);EmitByte(op[i].op);
			printf("QCJIT: instruction %i is not implemented\n", op[i].op);
			break;

		case OP_NE_V:
			EmitByte(0xcd);EmitByte(op[i].op);
			printf("QCJIT: instruction %i is not implemented\n", op[i].op);
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
