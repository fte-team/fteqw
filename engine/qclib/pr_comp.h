// this file is shared by the execution and compiler

/*i'm part way through making this work
I've given up now that I can't work out a way to load pointers.
Setting them should be fine.
*/
#ifndef __PR_COMP_H__
#define __PR_COMP_H__


/*this distinction is made as the execution uses c pointers while compiler uses pointers from the start of the string table of the current progs*/
#ifdef COMPILER
typedef int QCC_string_t;
#else
//typedef char *string_t;
#endif

//typedef enum {ev_void, ev_string, ev_float, ev_vector, ev_entity, ev_field, ev_function, ev_pointer, ev_integer, ev_struct, ev_union} etype_t;
//				0			1		2			3			4		5			6				7			8		9			10

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


enum {
	OP_DONE,	//0
	OP_MUL_F,
	OP_MUL_V,
	OP_MUL_FV,
	OP_MUL_VF,
	OP_DIV_F,
	OP_ADD_F,
	OP_ADD_V,
	OP_SUB_F,
	OP_SUB_V,
	
	OP_EQ_F,	//10
	OP_EQ_V,
	OP_EQ_S,
	OP_EQ_E,
	OP_EQ_FNC,
	
	OP_NE_F,
	OP_NE_V,
	OP_NE_S,
	OP_NE_E,
	OP_NE_FNC,
	
	OP_LE,	//20
	OP_GE,
	OP_LT,
	OP_GT,

	OP_LOAD_F,
	OP_LOAD_V,
	OP_LOAD_S,
	OP_LOAD_ENT,
	OP_LOAD_FLD,
	OP_LOAD_FNC,

	OP_ADDRESS,	//30

	OP_STORE_F,
	OP_STORE_V,
	OP_STORE_S,
	OP_STORE_ENT,
	OP_STORE_FLD,
	OP_STORE_FNC,

	OP_STOREP_F,
	OP_STOREP_V,
	OP_STOREP_S,
	OP_STOREP_ENT,	//40
	OP_STOREP_FLD,
	OP_STOREP_FNC,

	OP_RETURN,
	OP_NOT_F,
	OP_NOT_V,
	OP_NOT_S,
	OP_NOT_ENT,
	OP_NOT_FNC,
	OP_IF,
	OP_IFNOT,		//50
	OP_CALL0,		//careful... hexen2 and q1 have different calling conventions
	OP_CALL1,		//remap hexen2 calls to OP_CALL2H
	OP_CALL2,
	OP_CALL3,
	OP_CALL4,
	OP_CALL5,
	OP_CALL6,
	OP_CALL7,
	OP_CALL8,
	OP_STATE,		//60
	OP_GOTO,
	OP_AND,
	OP_OR,
	
	OP_BITAND,
	OP_BITOR,

	
	//these following ones are Hexen 2 constants.
	
	OP_MULSTORE_F,
	OP_MULSTORE_V,
	OP_MULSTOREP_F,
	OP_MULSTOREP_V,

	OP_DIVSTORE_F,	//70
	OP_DIVSTOREP_F,

	OP_ADDSTORE_F,
	OP_ADDSTORE_V,
	OP_ADDSTOREP_F,
	OP_ADDSTOREP_V,

	OP_SUBSTORE_F,
	OP_SUBSTORE_V,
	OP_SUBSTOREP_F,
	OP_SUBSTOREP_V,

	OP_FETCH_GBL_F,	//80
	OP_FETCH_GBL_V,
	OP_FETCH_GBL_S,
	OP_FETCH_GBL_E,
	OP_FETCH_GBL_FNC,

	OP_CSTATE,
	OP_CWSTATE,

	OP_THINKTIME,

	OP_BITSET,
	OP_BITSETP,
	OP_BITCLR,		//90
	OP_BITCLRP,

	OP_RAND0,
	OP_RAND1,
	OP_RAND2,
	OP_RANDV0,
	OP_RANDV1,
	OP_RANDV2,

	OP_SWITCH_F,
	OP_SWITCH_V,
	OP_SWITCH_S,	//100
	OP_SWITCH_E,
	OP_SWITCH_FNC,

	OP_CASE,
	OP_CASERANGE,





	//the rest are added
	//mostly they are various different ways of adding two vars with conversions.

	OP_CALL1H,
	OP_CALL2H,
	OP_CALL3H,
	OP_CALL4H,
	OP_CALL5H,
	OP_CALL6H,		//110
	OP_CALL7H,
	OP_CALL8H,


	OP_STORE_I,
	OP_STORE_IF,
	OP_STORE_FI,
	
	OP_ADD_I,
	OP_ADD_FI,
	OP_ADD_IF,		//110
  
	OP_SUB_I,
	OP_SUB_FI,
	OP_SUB_IF,

	OP_CONV_ITOF,
	OP_CONV_FTOI,
	OP_CP_ITOF,
	OP_CP_FTOI,
	OP_LOAD_I,
	OP_STOREP_I,
	OP_STOREP_IF,	//120
	OP_STOREP_FI,

	OP_BITAND_I,
	OP_BITOR_I,

	OP_MUL_I,
	OP_DIV_I,
	OP_EQ_I,
	OP_NE_I,

	OP_IFNOTS,
	OP_IFS,

	OP_NOT_I,		//130

	OP_DIV_VF,

	OP_POWER_I,
	OP_RSHIFT_I,
	OP_LSHIFT_I,

	OP_GLOBALADDRESS,
	OP_POINTER_ADD,	//32 bit pointers

	OP_LOADA_F,
	OP_LOADA_V,	
	OP_LOADA_S,
	OP_LOADA_ENT,	//140
	OP_LOADA_FLD,		
	OP_LOADA_FNC,
	OP_LOADA_I,

	OP_STORE_P,
	OP_LOAD_P,

	OP_LOADP_F,
	OP_LOADP_V,	
	OP_LOADP_S,
	OP_LOADP_ENT,
	OP_LOADP_FLD,	//150
	OP_LOADP_FNC,
	OP_LOADP_I,

	OP_LE_I,
	OP_GE_I,
	OP_LT_I,
	OP_GT_I,

	OP_LE_IF,
	OP_GE_IF,
	OP_LT_IF,
	OP_GT_IF,		//160

	OP_LE_FI,
	OP_GE_FI,
	OP_LT_FI,
	OP_GT_FI,

	OP_EQ_IF,
	OP_EQ_FI,

	//-------------------------------------
	//string manipulation.
	OP_ADD_SF,	//(char*)c = (char*)a + (float)b
	OP_SUB_S,	//(float)c = (char*)a - (char*)b
	OP_STOREP_C,//(float)c = *(char*)b = (float)a
	OP_LOADP_C,	//(float)c = *(char*)					//170
	//-------------------------------------


	OP_MUL_IF,
	OP_MUL_FI,
	OP_MUL_VI,
	OP_MUL_IV,
	OP_DIV_IF,
	OP_DIV_FI,
	OP_BITAND_IF,
	OP_BITOR_IF,
	OP_BITAND_FI,
	OP_BITOR_FI,		//180
	OP_AND_I,
	OP_OR_I,
	OP_AND_IF,
	OP_OR_IF,
	OP_AND_FI,
	OP_OR_FI,
	OP_NE_IF,
	OP_NE_FI,

//erm... FTEQCC doesn't make use of these... These are for DP.
	OP_GSTOREP_I,
	OP_GSTOREP_F,		//190
	OP_GSTOREP_ENT,
	OP_GSTOREP_FLD,		// integers
	OP_GSTOREP_S,
	OP_GSTOREP_FNC,		// pointers
	OP_GSTOREP_V,
	OP_GADDRESS,
	OP_GLOAD_I,
	OP_GLOAD_F,
	OP_GLOAD_FLD,
	OP_GLOAD_ENT,		//200
	OP_GLOAD_S,
	OP_GLOAD_FNC,
	OP_BOUNDCHECK,

//back to ones that we do use.
	OP_STOREP_P,
	OP_PUSH,	//push 4octets onto the local-stack (which is ALWAYS poped on function return). Returns a pointer.
	OP_POP,		//pop those ones that were pushed (don't over do it). Needs assembler.

	OP_NUMOPS
};


#ifndef COMPILER
typedef struct statement16_s
{
	unsigned short	op;
	unsigned short	a,b,c;
} dstatement16_t;
typedef struct statement32_s
{
	unsigned int	op;
	unsigned int	a,b,c;
} dstatement32_t;
#else
typedef struct QCC_statement16_s
{
	unsigned short	op;
	unsigned short	a,b,c;
} QCC_dstatement16_t;
typedef struct QCC_statement32_s
{
	unsigned int	op;
	unsigned int	a,b,c;
} QCC_dstatement32_t;
#define QCC_dstatement_t QCC_dstatement32_t
#endif

//these should be the same except the string type
#ifndef COMPILER
typedef struct ddef16_s
{
	unsigned short	type;		// if DEF_SAVEGLOBAL bit is set
								// the variable needs to be saved in savegames
	unsigned short	ofs;
	string_t		s_name;
} ddef16_t;

typedef struct ddef32_s
{
	unsigned int	type;		// if DEF_SAVEGLOBAL bit is set
								// the variable needs to be saved in savegames
	unsigned int	ofs;
	string_t		s_name;
} ddef32_t;

typedef struct fdef_s
{
	unsigned int	type;		// if DEF_SAVEGLOBAL bit is set
								// the variable needs to be saved in savegames
	unsigned int	ofs;
	unsigned int	progsofs;	//used at loading time, so maching field offsets (unions/members) are positioned at the same runtime offset.
	char *		name;
} fdef_t;

typedef void *ddefXX_t;
#else
typedef struct QCC_ddef16_s
{
	unsigned short	type;		// if DEF_SAVEGLOBAL bit is set
								// the variable needs to be saved in savegames
	unsigned short	ofs;
	QCC_string_t		s_name;
} QCC_ddef16_t;

typedef struct QCC_ddef32_s
{
	unsigned int	type;		// if DEF_SAVEGLOBAL bit is set
								// the variable needs to be saved in savegames
	unsigned int	ofs;
	QCC_string_t		s_name;
} QCC_ddef32_t;

#define QCC_ddef_t QCC_ddef32_t
#endif

#define	DEF_SAVEGLOBAL 		(1<<15)
#define	DEF_SHARED 		(1<<14)

#define	MAX_PARMS	8

#ifndef COMPILER
typedef struct
{
	int		first_statement;	// negative numbers are builtins
	int		parm_start;
	int		locals;				// total ints of parms + locals
	
	int		profile;		// runtime
	
	string_t	s_name;
	string_t	s_file;			// source file defined in
	
	int		numparms;
	qbyte	parm_size[MAX_PARMS];
} dfunction_t;
#else
typedef struct
{
	unsigned int		first_statement;	// negative numbers are builtins
	unsigned int		parm_start;
	int		locals;				// total ints of parms + locals
	
	int		profile;		// runtime
	
	QCC_string_t	s_name;
	QCC_string_t	s_file;			// source file defined in
	
	int		numparms;
	qbyte	parm_size[MAX_PARMS];
} QCC_dfunction_t;
#endif


#define	PROG_VERSION	6
#define PROG_KKQWSVVERSION 7
#define	PROG_EXTENDEDVERSION	7
#define PROG_SECONDARYVERSION16 (*(int*)"1FTE" ^ *(int*)"PROG")	//something unlikly and still meaningful (to me)
#define PROG_SECONDARYVERSION32 (*(int*)"1FTE" ^ *(int*)"32B ")	//something unlikly and still meaningful (to me)
typedef struct
{
	int		version;
	int		crc;			// check of header file
	
	unsigned int		ofs_statements;	//comp 1
	unsigned int		numstatements;	// statement 0 is an error

	unsigned int		ofs_globaldefs;	//comp 2
	unsigned int		numglobaldefs;
	
	unsigned int		ofs_fielddefs;	//comp 4
	unsigned int		numfielddefs;
	
	unsigned int		ofs_functions;	//comp 8
	unsigned int		numfunctions;	// function 0 is an empty
	
	unsigned int		ofs_strings;	//comp 16
	unsigned int		numstrings;		// first string is a null string

	unsigned int		ofs_globals;	//comp 32
	unsigned int		numglobals;
	
	unsigned int		entityfields;

	//debug / version 7 extensions
	unsigned int		ofsfiles;	//non list format. no comp
	unsigned int		ofslinenums;	//numstatements big	//comp 64
	unsigned int		ofsbodylessfuncs;	//no comp
	unsigned int		numbodylessfuncs;

	unsigned int	ofs_types;	//comp 128
	unsigned int	numtypes;
	unsigned int	blockscompressed;

	int	secondaryversion;	//Constant - to say that any version 7 progs are actually ours, not someone else's alterations.
} dprograms_t;
#define standard_dprograms_t_size ((int)&((dprograms_t*)NULL)->ofsfiles)

#endif





typedef struct {
	char filename[128];
	int size;
	int compsize;
	int compmethod;
	int ofs;
} includeddatafile_t;




typedef struct typeinfo_s
{
	etype_t	type;

	int		next;
	int		aux_type;
	int		num_parms;

	int		ofs;	//inside a structure.
	int		size;
	char	*name;
} typeinfo_t;
