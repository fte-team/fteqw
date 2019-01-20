
#include "qcc.h"
//#include "decomp.h"

//This file is derived from frikdec, but has some extra tweaks to make it more friendly with fte's compiler/opcodes (its not perfect though)
//FIXME: there's still a load of mallocs, so we don't allow this more than once per process.
//convert vector terms into floats when its revealed that they're using vector ops and not floats
//FIXME: fteqcc converts do {} while(1); into a goto -1; which is a really weeeird construct.

#if defined(_WIN32) || defined(__DJGPP__)
#include <malloc.h>
#else
#include <alloca.h>
#endif

#undef printf
#define printf GUIprintf

#define OP_MARK_END_DO		0x00010000	//do{
#define OP_MARK_END_ELSE	0x00000400	//}

#define MAX_REGS 65536
#define dstatement_t QCC_dstatement32_t
#define statements destatements
#define functions defunctions
#define strings destrings
static dstatement_t *statements;
static float *pr_globals;
static char		*strings;
static QCC_ddef32_t *globals;
static dfunction_t *functions;

static int ofs_return;
static int ofs_parms[MAX_PARMS];
static int ofs_size = 3;

static QCC_ddef_t		*globalofsdef[MAX_REGS];


//forward declarations.
QCC_ddef_t *GetField(const char *name);

#include <stdio.h>

/*int QC_snprintfz(char *buffer, size_t maxlen, const char *format, ...)
{
	int p;
	va_list		argptr;

	if (!maxlen)
		return -1;
		
	va_start (argptr, format);
	p = _vsnprintf (buffer, maxlen, format,argptr);
	va_end (argptr);
	buffer[maxlen-1] = 0;

	return p;
}*/

extern QCC_opcode_t pr_opcodes [];

int endofsystemfields;
int debug_offs = 0;
int assumeglobals = 0;	//unknown globals are assumed to be actual globals and NOT unlocked temps
int assumelocals = 0;	//unknown locals are assumed to be actual locals and NOT locked temps

vfile_t *Decompileofile;
vfile_t *Decompileprogssrc;
vfile_t *Decompileprofile;
char **DecompileProfiles;//[MAX_FUNCTIONS];
static char **rettypes;//[MAX_FUNCTIONS];

extern int quakeforgeremap[];

char *type_names[] =
{
	"void",
	"string",
	"float",
	"vector",
	"entity",
	"ev_field",
	"void()",
	"ev_pointer",
	"int",
	"__variant",
	"ev_struct",
	"ev_union",
	"ev_accessor",
	"ev_quat",
	"ev_uinteger"
};
const char *typetoname(QCC_type_t *type)
{
	return type->name;
}

const char *temp_type (int temp, dstatement_t *start, dfunction_t *df)
{
	int i;
	dstatement_t *stat;
	stat = start - 1;
	// determine the type of a temp

	while(stat > statements)
	{
		if (temp == stat->a)
			return typetoname(*pr_opcodes[stat->op].type_a);
		else if (temp == stat->b)
			return typetoname(*pr_opcodes[stat->op].type_b);
		else if (temp == stat->c)
			return typetoname(*pr_opcodes[stat->op].type_c);
		stat--;
	}

	// method 2
	// find a call to this function
	for (i = 0; i < numstatements; i++)
	{
		stat = &statements[i];

		if (stat->op >= OP_CALL0 && stat->op <= OP_CALL8 && ((eval_t *)&pr_globals[stat->a])->function == df - functions)
		{
			for(i++; i < numstatements; i++)
			{
				stat = &statements[i];
				if (ofs_return == stat->a && (*pr_opcodes[stat->op].type_a)->type != ev_void)
					return type_names[(*pr_opcodes[stat->op].type_a)->type];
				else if (ofs_return == stat->b && (*pr_opcodes[stat->op].type_b)->type != ev_void)
					return type_names[(*pr_opcodes[stat->op].type_b)->type];
				else if (stat->op == OP_DONE)
					break;
				else if (stat->op >= OP_CALL0 && stat->op <= OP_CALL8 && stat->a != df - functions)
					break;
			}
		}
	}

	printf("warning: Could not determine return type for %s\n", df->s_name + strings);

	return "float";

}

pbool IsConstant(QCC_ddef_t *def)
{

	int i;
	dstatement_t *d;

	if (def->type & DEF_SAVEGLOBAL)
		return false;

	if (pr_globals[def->ofs] == 0)
		return false;

	for (i = 1; i < numstatements; i++)
	{
		d = &statements[i];
		if (d->b == def->ofs)
		{
			if (pr_opcodes[d->op].associative == ASSOC_RIGHT)
			{
				if (d->op - OP_STORE_F < 6)
				{
					return false;
				}
			}
		}
	}
	return true;
}

const char *qcstring(int str)
{
	if ((unsigned)str >= strofs)
		return "";
	return strings+str;
}

char *type_name (QCC_ddef_t *def)
{
	QCC_ddef_t *j;

	switch(def->type&~DEF_SAVEGLOBAL)
	{
	case ev_field:
	case ev_pointer:
		j = GetField(def->s_name + strings);
		if (j)
			return qcva(".%s",type_names[j->type]);
		else
			return type_names[def->type&~DEF_SAVEGLOBAL];
	case ev_void:
	case ev_string:
	case ev_entity:
	case ev_vector:
	case ev_float:
		return type_names[def->type&~DEF_SAVEGLOBAL];
	case ev_function:
		return "void()";
	case ev_integer:
		return "int";
//	case ev_uinteger:
//		return "unsigned";
//	case ev_quat:
//		return "quat";
	default:
		return "float";
	}
};

extern int numstatements;

extern int numfunctions;

#define FILELISTSIZE 62



/*
===============
PR_String

Returns a string suitable for printing (no newlines, max 60 chars length)
===============
*/
const char *PR_String (const char *string)
{
	static char buf[80];
	char	*s;
	
	s = buf;
	*s++ = '"';
	while (string && *string)
	{
		if (s == buf + sizeof(buf) - 2)
			break;
		if (*string == '\n')
		{
			*s++ = '\\';
			*s++ = 'n';
		}
		else if (*string == '"')
		{
			*s++ = '\\';
			*s++ = '"';
		}
		else
			*s++ = *string;
		string++;
		if (s - buf > 60)
		{
			*s++ = '.';
			*s++ = '.';
			*s++ = '.';
			break;
		}
	}
	*s++ = '"';
	*s++ = 0;
	return buf;
}
/*
============
PR_ValueString

Returns a string describing *data in a type specific manner
=============
*/


static char *PR_ValueString (etype_t type, void *val)
{
	static char	line[8192];

	dfunction_t	*f;
	
	switch (type)
	{
	case ev_string:
		QC_snprintfz(line, sizeof(line), "%s", PR_String(strings + *(int *)val));
		break;
	case ev_entity:	
		QC_snprintfz(line, sizeof(line), "entity %i", *(int *)val);
		break;
	case ev_function:
		f = functions + *(int *)val;
		if (!f)
			QC_snprintfz(line, sizeof(line), "undefined function");
		else
			QC_snprintfz(line, sizeof(line), "%s()", strings + f->s_name);
		break;
	/*
	case ev_field:
		def = PR_DefForFieldOfs ( *(int *)val );
		sprintf (line, ".%s", def->name);
		break;
	*/
	case ev_void:
		QC_snprintfz(line, sizeof(line), "void");
		break;
	case ev_float:
		{
			unsigned int high = *(unsigned int*)val & 0xff000000;
			if (high == 0xff000000 || !high)
				//FIXME this is probably a string or something, but we don't really know what type it is.
				QC_snprintfz(line, sizeof(line), "(float)(__variant)%ii", *(int*)val);
			else
				QC_snprintfz(line, sizeof(line), "%5.1f", *(float *)val);
		}
		break;
	case ev_vector:
		QC_snprintfz(line, sizeof(line), "'%5.1f %5.1f %5.1f'", ((float *)val)[0], ((float *)val)[1], ((float *)val)[2]);
		break;
	case ev_pointer:
		QC_snprintfz(line, sizeof(line), "pointer");
		break;
	case ev_field:
		QC_snprintfz(line, sizeof(line), "<FIELD@%i>", *(int*)val);
		break;
	default:
		QC_snprintfz(line, sizeof(line), "bad type %i", type);
		break;
	}
	
	return line;
}


static char *filenames[] =
{
	"makevectors",		"defs.qc",
	"button_wait",		"buttons.qc",
	"anglemod",		"ai.qc",
	"boss_face",		"boss.qc",
	"info_intermission",	"client.qc",
	"CanDamage",	"combat.qc",
	"demon1_stand1",	"demon.qc",
	"dog_bite",		"dog.qc",
	"door_blocked",		"doors.qc",
	"Laser_Touch",		"enforcer.qc",
	"knight_attack",			"fight.qc",
	"f_stand1",	"fish.qc",
	"hknight_shot",	"hknight.qc",
	"SUB_regen",		"items.qc",
	"knight_stand1",	"knight.qc",
	"info_null",		"misc.qc",
	"monster_use",		"monsters.qc",
	"OgreGrenadeExplode",	"ogre.qc",
	"old_idle1",			"oldone.qc",
	"plat_spawn_inside_trigger", "plats.qc",
	"player_stand1",		"player.qc",
	"shal_stand",	"shalrath.qc",
	"sham_stand1",		"shambler.qc",
	"army_stand1",		"soldier.qc",
	"SUB_Null",			"subs.qc",
	"tbaby_stand1",		"tarbaby.qc",
	"trigger_reactivate",	"triggers.qc",
	"W_Precache",			"weapons.qc",
	"LaunchMissile",	"wizard.qc",
	"main",		"world.qc",
	"zombie_stand1",	"zombie.qc"
};

//FIXME: parse fteextensions.qc instead, or something.
#define QW(x) //x,
static struct {
	int num;
	char *name;	//purly for readability.
	QCC_type_t **returns;
	QCC_type_t **params[8];
	char *text;
} builtins[]=
{
	{0,		NULL,				NULL, {NULL},																NULL},								
	{1,		"makevectors",		NULL, {&type_vector},														"void (vector ang)"},
	{2,		"setorigin",		NULL, {&type_entity, &type_vector},											"void (entity e, vector o)"},
	{3,		"setmodel",			NULL, {&type_entity, &type_string},											"void (entity e, string m)"},
	{4,		"setsize",			NULL, {&type_entity, &type_vector, &type_vector},							"void (entity e, vector min, vector max)"},
	{5,		NULL,				NULL, {NULL},																NULL},
	{6,		NULL,				NULL, {NULL},																"void ()"},
	{7,		"random",			NULL, {NULL},																"float ()"},
	{8,		"sound",			NULL, {&type_entity, &type_float, &type_string, &type_float, &type_float},	"void (entity e, float chan, string samp, float vol, float atten)"},
	{9,		"normalize",		&type_vector, {&type_vector},												"vector (vector v)"},
	{10,	"error",			NULL, {&type_string},														"void (string e)"},
	{11,	"objerror",			NULL, {&type_string},														"void (string e)"},
	{12,	"vlen",				&type_float, {&type_vector},												"float (vector v)"},
	{13,	"vectoyaw",			&type_float, {&type_vector},												"float (vector v)"},
	{14,	"spawn",			&type_entity, {NULL},														"entity ()"},
	{15,	"remove",			NULL, {&type_entity},														"void (entity e)"},
	{16,	"traceline",		NULL, {&type_vector, &type_vector, &type_float, &type_entity},				"void (vector v1, vector v2, float nomonsters, entity forent)"},
	{17,	NULL,				NULL, {NULL},																"entity ()"},
	{18,	"find",				&type_entity, {&type_entity, &type_field, &type_string},					"entity (entity start, .string fld, string match)"},
	{19,	"precache_sound",	NULL, {&type_string},														"string (string s)"},
	{20,	"precache_model",	NULL, {&type_string},														"string (string s)"},
	{21,	"stuffcmd",			NULL, {&type_entity, &type_string},											"void (entity client, string s)"},
	{22,	"findradius",		NULL, {&type_vector, &type_float},											"entity (vector org, float rad)"},
	{23,	"bprint",			NULL, {QW(&type_float) &type_string,&type_string,&type_string,&type_string,&type_string,&type_string,&type_string}, "void (...)"},
	{24,	"sprint",			NULL, {&type_entity, QW(&type_float) &type_string,&type_string,&type_string,&type_string,&type_string,&type_string}, "void (...)"},
	{25,	"dprint",			NULL, {&type_string,&type_string,&type_string,&type_string,&type_string,&type_string,&type_string,&type_string}, "void (...)"},
	{26,	"ftos",				&type_string, {&type_float},												"string (float f)"},
	{27,	"vtos",				&type_string, {&type_vector},												"string (vector v)"},
	{28,	"coredump",			NULL, {NULL},																"void ()"},
	{29,	"traceon",			NULL, {NULL},																"void ()"},
	{30,	"traceoff",			NULL, {NULL},																"void ()"},
	{31,	"eprint",			NULL, {&type_entity},														"void (entity e)"},
	{32,	"walkmove",			&type_float, {&type_float, &type_float},									"float (float yaw, float dist)"},
	{33,	NULL,				NULL, {NULL},																NULL},
	{34,	"droptofloor",		NULL, {&type_float, &type_float},											"float ()"},
	{35,	"lightstyle",		NULL, {&type_float, &type_string},											"void (float style, string value)"},
	{36,	"rint",				&type_float, {&type_vector},												"float (float v)"},
	{37,	"floor",			&type_float, {&type_vector},												"float (float v)"},
	{38,	"ceil",				&type_float, {&type_vector},												"float (float v)"},
	{39,	NULL,				NULL, {NULL},																NULL},
	{40,	"checkbottom",		&type_float, {&type_entity},												"float (entity e)"},
	{41,	"pointcontents",	&type_float, {&type_vector},												"float (vector v)"},
	{42,	NULL,				NULL, {NULL},																NULL},
	{43,	"fabs",				&type_float, {&type_float},													"float (float f)"},
	{44,	"aim",				NULL, {&type_entity, &type_float},											"vector (entity e, float speed)"},
	{45,	"cvar",				&type_string, {&type_string},												"float (string s)"},
	{46,	"localcmd",			NULL, {&type_string},														"void (string s)"},
	{47,	"nextent",			&type_entity, {&type_entity},												"entity (entity e)"},
	{48,	"",					NULL, {&type_vector, &type_vector, &type_float, &type_float},				"void (vector o, vector d, float color, float count)"},
	{49,	"changeyaw",		NULL, {NULL},																"void ()"},
	{50,	NULL,				NULL, {NULL},																NULL},
	{51,	"vectoangles",		&type_vector, {&type_vector},												"vector (vector v)"},
	{52,	"WriteByte",		NULL, {&type_float, &type_float},											"void (float to, float f)"},
	{53,	"WriteChar",		NULL, {&type_float, &type_float},											"void (float to, float f)"},
	{54,	"WriteShort",		NULL, {&type_float, &type_float},											"void (float to, float f)"},
	{55,	"WriteLong",		NULL, {&type_float, &type_float},											"void (float to, float f)"},
	{56,	"WriteCoord",		NULL, {&type_float, &type_float},											"void (float to, float f)"},
	{57,	"WriteAngle",		NULL, {&type_float, &type_float},											"void (float to, float f)"},
	{58,	"WriteString",		NULL, {&type_float, &type_string},											"void (float to, string s)"},
	{59,	"WriteEntity",		NULL, {&type_float, &type_entity},											"void (float to, entity s)"},
	{60,	NULL,				NULL, {NULL},																NULL},
	{61,	NULL,				NULL, {NULL},																NULL},
	{62,	NULL,				NULL, {NULL},																NULL},
	{63,	NULL,				NULL, {NULL},																NULL},
	{64,	NULL,				NULL, {NULL},																NULL},
	{65,	NULL,				NULL, {NULL},																NULL},
	{66,	NULL,				NULL, {NULL},																NULL},
	{67,	"movetogoal",		NULL, {&type_float},														"void (float step)"},
	{68,	"precache_file",	NULL, {&type_string},														"string (string s)"},
	{69,	"makestatic",		NULL, {&type_entity},														"void (entity e)"},
	{70,	"changelevel",		NULL, {&type_string},														"void (string s)"},
	{71,	NULL,				NULL, {NULL},																NULL},
	{72,	"cvar_set",			NULL, {&type_string, &type_string},											"void (string var, string val)"},
	{73,	"centerprint",		NULL, {&type_entity,&type_string,&type_string,&type_string,&type_string,&type_string,&type_string,&type_string},	"void (entity client, string s, ...)"},
	{74,	"ambientsound",		NULL, {&type_vector, &type_string, &type_float, &type_float},				"void (vector pos, string samp, float vol, float atten)"},
	{75,	"precache_model2",	NULL, {&type_string},														"string (string s)"},
	{76,	"precache_sound2",	NULL, {&type_string},														"string (string s)"},
	{77,	"precache_file2",	NULL, {&type_string},														"string (string s)"},
	{78,	"setspawnparms",	NULL, {&type_entity},														"void (entity e)"},

//quakeworld specific
	{79,	"logfrag",			NULL, {&type_entity, &type_entity},											"void(entity killer, entity killee)"},
	{80,	"infokey",			&type_string, {&type_entity, &type_string},									"string(entity e, string key)"},
	{81,	"stof",				&type_float, {&type_string},												"float(string s)"},
	{82,	"multicast",		NULL, {&type_vector, &type_float},											"void(vector where, float set)"},
/*
//these are mvdsv specific
	{83,	"executecmd",		NULL, {NULL},																NULL},
	{84,	"tokenize",			NULL, {&type_string},														NULL},
	{85,	"argc",				&type_float, {NULL},														"float()"},
	{86,	"argv",				&type_string, {&type_float},												"string(float f)"},
	{87,	"teamfield",		NULL, {NULL},																"void(.string fs)"},
	{88,	"substr",			&type_string, {&type_string, &type_float, &type_float},						"string(string, float, float)"},
	{89,	"strcat",			&type_string, {&type_string,&type_string,&type_string,&type_string,&type_string,&type_string,&type_string,&type_string},					"string (...)"},
	{90,	"strlen",			&type_float, {&type_string},												"float(string s)"},
	{91,	"str2byte",			&type_float, {&type_string},												"float(string s)"},
	{92,	NULL,				NULL, {NULL},																NULL},
	{93,	"newstr",			&type_string, {&type_string},												"string(...)"},
	{94,	"freestr",			NULL, {&type_string},														"void(string s)"},
	{95,	"conprint",			NULL, {NULL},																NULL},
	{96,	"readcmd",			&type_string, {&type_string},												"string(string cmd)"},
	{97,	"strcpy",			NULL, {NULL},																NULL},
	{98,	"strstr",			&type_string, {&type_string, &type_string},									"string(string str, string sub)"},
	{99,	"strncpy",			NULL, {NULL},																NULL},
	{100,	"log",				NULL, {NULL},																NULL},
	{101,	"redirectcmd",		NULL, {NULL},																NULL},
	{102,	"calltimeofday",	NULL, {NULL},																NULL},
	{103,	"forcedemoframe",	NULL, {NULL},																NULL},
*/

//some QSG extensions

	{83,	NULL,				NULL, {NULL},																NULL},
	{84,	NULL,				NULL, {NULL},																NULL},
	{85,	NULL,				NULL, {NULL},																NULL},
	{86,	NULL,				NULL, {NULL},																NULL},
	{87,	NULL,				NULL, {NULL},																NULL},
	{88,	NULL,				NULL, {NULL},																NULL},
	{89,	NULL,				NULL, {NULL},																NULL},
	{90,	"tracebox",		NULL, {&type_vector, &type_vector, &type_vector, &type_vector, &type_float, &type_entity},	"void(vector start, vector mins, vector maxs, vector end, float nomonsters, entity ent)"},
	{91,	"randomvec",	&type_vector, {NULL},															"vector()"},
	{92,	"getlight",		&type_vector, {&type_vector},													"vector(vector org)"},
	{93,	"registercvar",	&type_float, {&type_string, &type_string},										"float(string cvarname, string defaultvalue)"},
	{94,	"min",			&type_float, {&type_float,&type_float,&type_float,&type_float,&type_float,&type_float,&type_float,&type_float},"float(float a, float b, ...)"},
	{95,	"max",			&type_float, {&type_float,&type_float,&type_float,&type_float,&type_float,&type_float,&type_float,&type_float},"float(float a, float b, ...)"},
	{96,	"bound",		&type_float, {&type_float,&type_float,&type_float},								"float(float minimum, float val, float maximum)"},
	{97,	"pow",			&type_float, {&type_float,&type_float},											"float(float value, float exp)"},
	{98,	"findfloat",	&type_entity, {&type_entity,&type_field,&type_float},							"entity(entity start, .__variant fld, __variant match)"},

	{99,	"checkextension",&type_float, {&type_string},													"float(string extname)"},
	{100,	"builtin_find",	&type_float, {&type_string},													"float(string builtinname)"},
	{101,	"redirectcmd",	NULL, {&type_entity,&type_string},												"void(entity to, string str)"},
	{102,	"anglemod",		&type_float, {&type_float},														"float(float value)"},
	{103,	"cvar_string",	&type_string, {&type_string},													"string(string cvarname)"},

	{104,	"showpic",		NULL, {NULL},																"void(string slot, string picname, float x, float y, float zone, optional entity player)"},
	{105,	"hidepic",		NULL, {NULL},																"void(string slot, optional entity player)"},
	{106,	"movepic",		NULL, {NULL},																"void(string slot, float x, float y, float zone, optional entity player)"},
	{107,	"changepic",	NULL, {NULL},																"void(string slot, string picname, optional entity player)"},
	{108,	"showpicent",	NULL, {NULL},																"void(string slot, entity player)"},
	{109,	"hidepicent",	NULL, {NULL},																"void(string slot, entity player)"},

	{110,	"fopen",		&type_float, {&type_string,&type_float},									"float(string filename, float mode, optional float mmapminsize)"},
	{111,	"fclose",		NULL, {&type_float},														"void(float fhandle)"},
	{112,	"fgets",		&type_string, {&type_float,&type_string},									"string(float fhandle)"},
	{113,	"fputs",		NULL, {&type_float,&type_string},											"void(float fhandle, string s, optional string s2, optional string s3, optional string s4, optional string s5, optional string s6, optional string s7)"},
	{114,	"strlen",		&type_float, {&type_string},												"float(string s)"},
	{115,	"strcat",		&type_string, {&type_string,&type_string},									"string(string s1, optional string s2, optional string s3, optional string s4, optional string s5, optional string s6, optional string s7, optional string s8)"},
	{116,	"substring",	&type_string, {&type_string,&type_float,&type_float},						"string(string s, float start, float length)"},
	{117,	"stov",			&type_vector, {&type_string},												"vector(string s)"},
	{118,	"strzone",		&type_string, {&type_string},												"string(string s, ...)"},
	{119,	"strunzone",	NULL, {&type_string},														"void(string s)"},
};

char *DecompileValueString(etype_t type, void *val);
QCC_ddef_t *DecompileGetParameter(gofs_t ofs);
QCC_ddef_t *DecompileFindGlobal(const char *name);
char *DecompilePrintParameter(QCC_ddef_t * def);
QCC_ddef_t *DecompileFunctionGlobal(int funcnum);

char *ReadProgsCopyright(char *buf, size_t bufsize)
{
	char *copyright, *e;
	dprograms_t *progs = (dprograms_t*)buf;
	int lowest = progs->ofs_statements;
	lowest = min(lowest, progs->ofs_globaldefs);
	lowest = min(lowest, progs->ofs_fielddefs);
	lowest = min(lowest, progs->ofs_functions);
	lowest = min(lowest, progs->ofs_strings);
	lowest = min(lowest, progs->ofs_globals);
	lowest = min(lowest, progs->ofs_fielddefs);

	copyright = (char*)(progs+1);
	if (!strncmp("\r\n\r\n", copyright, 4))
	{
		copyright += 4;
		e = copyright+strlen(copyright)+1;
		if (e && !strncmp(e, "\r\n\r\n", 4))
		{
			if (e+4 <= buf+lowest)
			{
				return copyright;
			}

		}
	}
	return NULL;
}

int DecompileReadData(char *srcfilename, char *buf, size_t bufsize)
{
	dprograms_t progs;
	int i, j;
	void *p;
	char name[1024];
	QCC_ddef_t *fd;

	int stsz = 16, defsz=16;
//	int quakeforge = false;
	
	memcpy(&progs, buf, sizeof(progs));

	if (progs.version == PROG_QTESTVERSION)
	{
		defsz = -32;	//ddefs_t is 32bit
		stsz = -16;	//statements is mostly 16bit. there's some line numbers in there too.
	}
	else if (progs.version == PROG_VERSION)
		stsz = defsz = 16;
	else if (progs.version == 7)
	{
		if (progs.secondaryversion == PROG_SECONDARYVERSION16)
		{
			//regular 16bit progs, just an extended instruction set probably.		
			stsz = defsz = 16;
		}
		else if (progs.secondaryversion == PROG_SECONDARYVERSION32)
		{
			//32bit fte progs. everything is 32bit.
			stsz = defsz = 32;
		}
		else
		{
			//progs is kk7 (certain QW TF mods). defs are 16bit but statements are 32bit. so this is unusable for saved games.
			stsz = 32;
			defsz = 16;	//gah! fucked!
		}
	}
	else
	{
		stsz = defsz = 16;
//		quakeforge = true;
	}

	strings = buf + progs.ofs_strings;
	strofs = progs.numstrings;

	numstatements = progs.numstatements;

//	if (numstatements > MAX_STATEMENTS)
//		Sys_Error("Too many statements");
	if (stsz == 32)
		statements = (dstatement32_t*)(buf+progs.ofs_statements);
	else if (stsz == 16)
	{	//need to expand the statements to 32bit.
		const dstatement16_t	*statements6 = (const dstatement16_t*)(buf+progs.ofs_statements);
		statements = malloc(numstatements * sizeof(*statements));
		for (i = 0; i < numstatements; i++)
		{
			statements[i].op = statements6[i].op;

			if (statements[i].op == OP_GOTO)
				statements[i].a = (signed short)statements6[i].a;
			else
				statements[i].a = (unsigned short)statements6[i].a;

			if (statements[i].op == OP_IF_I || statements[i].op == OP_IFNOT_I ||
				statements[i].op == OP_IF_F || statements[i].op == OP_IFNOT_F ||
				statements[i].op == OP_IF_S || statements[i].op == OP_IFNOT_S)
				statements[i].b = (signed short)statements6[i].b;
			else
				statements[i].b = (unsigned short)statements6[i].b;

			statements[i].c = (unsigned short)statements6[i].c;
		}
	}
	else if (stsz == -16)
	{
		const qtest_statement_t	*statements3 = (const qtest_statement_t*)(buf+progs.ofs_statements);
		statements = malloc(numstatements * sizeof(*statements));
		for (i = 0; i < numstatements; i++)
		{
			statements[i].op = statements3[i].op;

			if (statements[i].op == OP_GOTO)
				statements[i].a = (signed short)statements3[i].a;
			else
				statements[i].a = (unsigned short)statements3[i].a;

			if (statements[i].op == OP_IF_I || statements[i].op == OP_IFNOT_I ||
				statements[i].op == OP_IF_F || statements[i].op == OP_IFNOT_F ||
				statements[i].op == OP_IF_S || statements[i].op == OP_IFNOT_S)
				statements[i].b = (signed short)statements3[i].b;
			else
				statements[i].b = (unsigned short)statements3[i].b;

			statements[i].c = (unsigned short)statements3[i].c;
		}
	}
	else
		Sys_Error("Unrecognised progs version");

	numfunctions = progs.numfunctions;
	functions = (dfunction_t*)(buf+progs.ofs_functions);
	DecompileProfiles = calloc(numfunctions, sizeof(*DecompileProfiles));
	rettypes = calloc(numfunctions, sizeof(*rettypes));

	numglobaldefs = progs.numglobaldefs;
	numfielddefs = progs.numfielddefs;
	if (defsz == 16)
	{
		const QCC_ddef16_t	*gd16 = (const QCC_ddef16_t*)(buf+progs.ofs_globaldefs);
		globals = malloc(numglobaldefs * sizeof(*globals));
		for (i = 0; i < numglobaldefs; i++)
		{
			globals[i].ofs = gd16[i].ofs;
			globals[i].s_name = gd16[i].s_name;
			globals[i].type = gd16[i].type;
		}


		gd16 = (const QCC_ddef16_t*)(buf+progs.ofs_fielddefs);
		fields = malloc(numfielddefs * sizeof(*fields));
		for (i = 0; i < numfielddefs; i++)
		{
			fields[i].ofs = gd16[i].ofs;
			fields[i].s_name = gd16[i].s_name;
			fields[i].type = gd16[i].type;
		}
	}
	else if (defsz == -32)
	{
		const qtest_function_t *funcin = (const qtest_function_t*)(buf+progs.ofs_functions);
		const qtest_def_t	*gdqt = (const qtest_def_t*)(buf+progs.ofs_globaldefs);
		globals = malloc(numglobaldefs * sizeof(*globals));
		for (i = 0; i < numglobaldefs; i++)
		{
			globals[i].ofs = gdqt[i].ofs;
			globals[i].s_name = gdqt[i].s_name;
			globals[i].type = gdqt[i].type;
		}


		gdqt = (const qtest_def_t*)(buf+progs.ofs_fielddefs);
		fields = malloc(numfielddefs * sizeof(*fields));
		for (i = 0; i < numfielddefs; i++)
		{
			fields[i].ofs = gdqt[i].ofs;
			fields[i].s_name = gdqt[i].s_name;
			fields[i].type = gdqt[i].type;
		}

		functions = malloc(numfunctions * sizeof(*functions));
		for (i = 0; i < numfunctions; i++)
		{
			functions[i].first_statement = funcin[i].first_statement;	// negative numbers are builtins
			functions[i].parm_start = funcin[i].parm_start;
			functions[i].locals = funcin[i].locals;				// total ints of parms + locals

			functions[i].profile = funcin[i].profile;		// runtime

			functions[i].s_name = funcin[i].s_name;
			functions[i].s_file = funcin[i].s_file;			// source file defined in

			functions[i].numparms = funcin[i].numparms;
			for (j = 0; j < MAX_PARMS; j++)
				functions[i].parm_size[j] = funcin[i].parm_size[j];
		}
	}
	else
	{
		globals = (QCC_ddef32_t*)(buf+progs.ofs_globaldefs);
		fields = (QCC_ddef32_t*)(buf+progs.ofs_fielddefs);
	}

	pr_globals = (float*)(buf+progs.ofs_globals);
	numpr_globals = progs.numglobals;

	printf("Decompiling...\n");
	printf("Read Data from %s:\n", srcfilename);
	printf("Total Size is %6i\n", bufsize);
	printf("Version Code is %i\n", progs.version);
	printf("CRC is %i\n", progs.crc);
	printf("%6i strofs\n", strofs);
	printf("%6i numstatements\n", numstatements);
	printf("%6i numfunctions\n", numfunctions);
	printf("%6i numglobaldefs\n", numglobaldefs);
	printf("%6i numfielddefs\n", numfielddefs);
	printf("%6i numpr_globals\n", numpr_globals);
	printf("----------------------\n");

	if (numpr_globals > MAX_REGS)
	{
		printf("fatal error: progs exceeds a limit\n");
		exit(1);
	}

	ofs_return = OFS_RETURN;
	for (i = 0; i < 8; i++)
		ofs_parms[i] = OFS_PARM0 + i * 3;
	ofs_size = 3;

/*
	if (quakeforge)
	{
		int typeremap[] = {ev_void, ev_string, ev_float, ev_vector, ev_entity, ev_field, ev_function, ev_pointer, ev_quat, ev_integer, ev_uinteger};
		for (i = 1; i < numglobaldefs; i++)
		{
			globals[i].type = (globals[i].type & DEF_SAVEGLOBGAL) | typeremap[globals[i].type&~DEF_SAVEGLOBGAL];
		}
		for (i = 1; i < numfielddefs; i++)
		{
			fields[i].type = (fields[i].type & DEF_SAVEGLOBGAL) | typeremap[fields[i].type&~DEF_SAVEGLOBGAL];
		}

		for (i = 1; i < numstatements; i++)
		{
			if (statements[i].op >= OP_H2_FIRST)// && statements[i].op <= OP_H2_FIRST+sizeof(quakeforgeremap)/sizeof(quakeforgeremap[0]))
				statements[i].op = quakeforgeremap[statements[i].op-OP_H2_FIRST];
		}

		fd = DecompileFindGlobal(".zero");
		if (fd)
			fd->ofs = -1;

		fd = DecompileFindGlobal(".return");
		if (fd)
		{
			ofs_return = fd->ofs;
			fd->ofs = -1;
		}

		for (i = 0; i < 8; i++)
		{
			QC_snprintfz(name, sizeof(name), ".param_%i", i);
			fd = DecompileFindGlobal(name);
			if (fd)
			{
				ofs_parms[i] = fd->ofs;
				fd->ofs = -1;
			}
		}

		fd = DecompileFindGlobal(".param_size");
		if (fd)
			ofs_size = ((int*)pr_globals)[fd->ofs];
	}
*/

	//fix up the globaldefs
	for (i = 1; i < numglobaldefs; i++)
	{
		if (globals[i].ofs < RESERVED_OFS)
			globals[i].ofs += numpr_globals;
	}
// fix up the functions
	for (i = 1; i < numfunctions; i++)
	{
		if ((unsigned)functions[i].s_name >= (unsigned)strofs || strlen(functions[i].s_name + strings) <= 0)
		{
			fd = DecompileFunctionGlobal(i);
			if (fd)
			{
				functions[i].s_name = fd->s_name;
				continue;
			}
			QC_snprintfz(name, sizeof(name), "function%i", i);
			name[strlen(name)] = 0;
			p = malloc(strlen(name + 1));
			strcpy(p, name);
			functions[i].s_name = (char *)p - strings;
		}
		if (functions[i].first_statement > 0 && !functions[i].locals && functions[i].numparms)
		{	//vanilla qcc apparently had a bug
			for (j = 0; j < functions[i].numparms; j++)
				functions[i].locals += functions[i].parm_size[j];
		}
	}

	return progs.version;
}

int 
DecompileGetFunctionIdxByName(const char *name)
{

	int i;

	for (i = 1; i < numfunctions; i++)
		if (!strcmp(name, strings + functions[i].s_name)) 
		{
			return i;
		}
	return 0;
}

const etype_t DecompileGetFieldTypeByDef(QCC_ddef_t *def)
{
	int i;
	int ofs = ((int*)pr_globals)[def->ofs];

	for (i = 1; i < numfielddefs; i++)
		if (fields[i].ofs == ofs) 
		{
			if (!strcmp(strings+def->s_name, strings+fields[i].s_name))
				return fields[i].type;
		}
	return ev_void;
}
const char *DecompileGetFieldNameIdxByFinalOffset(int ofs)
{
	int i;

	for (i = 1; i < numfielddefs; i++)
		if (fields[i].ofs == ofs) 
		{
			return fields[i].s_name+strings;
		}
	return "UNKNOWN FIELD";
}
void DecompileGetFieldNameIdxByFinalOffset2(char *out, size_t outsize, int ofs)
{
	int i;

	for (i = 1; i < numfielddefs; i++)
	{
		if (fields[i].ofs == ofs) 
		{
			QC_snprintfz(out, outsize, "%s", fields[i].s_name+strings);
			return;
		}
		else if (fields[i].type == ev_vector && fields[i].ofs+1 == ofs)
		{
			QC_snprintfz(out, outsize, "%s_y", fields[i].s_name+strings);
			return;
		}
		else if (fields[i].type == ev_vector && fields[i].ofs+2 == ofs)
		{
			QC_snprintfz(out, outsize, "%s_z", fields[i].s_name+strings);
			return;
		}
	}
	QC_snprintfz(out, outsize, "<FIELD@%i>", ofs);
}

int 
DecompileAlreadySeen(char *fname, vfile_t **rfile)
{
	int ret = 1;

	vfile_t *file;

	file = QCC_FindVFile(fname);
	if (!file)
	{
		ret = 0;
		if (rfile)
		{
			char *header = "//Decompiled code. Please respect the original copyright.\n";
			*rfile = QCC_AddVFile(fname, header, strlen(header));
			AddSourceFile("progs.src",	fname);
		}
	}
	else if (rfile)
		*rfile = file;

	return ret;
}

char *DecompileReturnType(dfunction_t *df);

char *DecompileAgressiveType(dfunction_t *df, dstatement_t *last, gofs_t ofs)
{
	QCC_ddef_t *par;
	par = DecompileGetParameter(ofs);
	if (par)	//single = intended
	{
		return type_name(par);
	}

	if (ofs == ofs_return && ((last->op >= OP_CALL0 && last->op <= OP_CALL8) || (last->op >= OP_CALL1H && last->op <= OP_CALL8H)))
	{	//offset is a return value, go look at the called function's return type.
		return DecompileReturnType(functions + ((int*)pr_globals)[last->a]);
	}

	while(last >= &statements[df->first_statement])
	{
		if (last->c == ofs && 
			pr_opcodes[last->op].associative == ASSOC_LEFT &&
			pr_opcodes[last->op].priorityclass)
		{
			//previous was an operation into the temp
			return type_names[(*pr_opcodes[last->op].type_c)->type];


	//					sprintf(fname, "%s ", temp_type6(rds->a, rds, df));
		}
		last--;
	}

	return NULL;	//got to start of function... shouldn't really happen.
}

char *DecompileReturnType(dfunction_t *df)
{
	dstatement_t *ds;
	unsigned short dom;
	pbool foundret = false;
	static int recursion;
	char *ret = NULL;	//return null if we don't know.
	int couldbeastring = true;

	if (df->first_statement <= 0)
	{
		if (df->first_statement > -(int)(sizeof(builtins)/sizeof(builtins[0])))
			if (builtins[-df->first_statement].returns)
				return type_names[(*builtins[-df->first_statement].returns)->type];

		return "void";	//no returns statements found
	}

	if (rettypes[df - functions])
		return rettypes[df - functions];

	recursion++;

	ds = statements + df->first_statement;

	/*
	 * find a return statement, to determine the result type 
	 */

	while (1) 
	{
		dom = (ds->op) % OP_MARK_END_ELSE;
		if (!dom)
			break;
//		if (dom == OPQF_RETURN_V)
//			break;
		if (dom == OP_RETURN)
		{
			if (ds->a != 0)	//some code is buggy.
			{
				foundret = true;

				if (recursion < 10)
				{
					ret = DecompileAgressiveType(df, ds-1, ds->a);
					if (ret)
						break;
				}

				if (((int*)pr_globals)[ds->a] < 0 && ((int*)pr_globals)[ds->a] >= strofs)
					couldbeastring = false;	//definatly not
				else
				{
					char buf[64];
					QC_snprintfz(buf, sizeof(buf), "%f", pr_globals[ds->a]);
					if (strcmp(buf, "0.000000"))
						couldbeastring = false;	//doesn't fit the profile
				}
			}

		}
		ds++;
	}
	recursion--;

	if (foundret)
	{
		if (!ret)
		{
			if (couldbeastring)
				ret = "string /*WARNING: could not determine return type*/";
			else
				ret = "float /*WARNING: could not determine return type*/";
		}
	}
	else
		ret = "void";	//no returns statements found

	rettypes[df - functions] = ret;
	return ret;
}

void DecompileCalcProfiles(void)
{

	int i, ps;
	gofs_t j;
	char *knew;
	static char fname[512];
	static char line[512];
	dfunction_t *df;
	QCC_ddef_t *par;

	for (i = 1; i < numfunctions; i++)
	{

		df = functions + i;
		fname[0] = '\0';
		line[0] = '\0';
		DecompileProfiles[i] = NULL;

		if (df->first_statement <= 0) 
		{
			if (-df->first_statement <= sizeof(builtins)/sizeof(builtins[0]) && builtins[-df->first_statement].text)
				QC_snprintfz(fname, sizeof(fname), "%s %s", builtins[-df->first_statement].text, strings + functions[i].s_name);
			else
			{
				QC_snprintfz(fname, sizeof(fname), "__variant(...) %s", strings + functions[i].s_name);
				printf("warning: unknown builtin %s\n", strings + functions[i].s_name);
			}
		}
		else
		{
			char *rettype;

			rettype = DecompileReturnType(df);
			if (!rettype)
			{	//but we do know that it's not void
				rettype = "float /*WARNING: could not determine return type*/";
			}
			strcpy(fname, rettype);
			strcat(fname, "(");

			/*
			 * determine overall parameter size 
			 */

			for (j = 0, ps = 0; j < df->numparms; j++)
				ps += df->parm_size[j];

			if (ps > 0) 
			{
				int p;
				for (p = 0, j = df->parm_start; j < (df->parm_start) + ps; p++)
				{
					line[0] = '\0';
					par = DecompileGetParameter(j);
					if (!par)
						par = DecompileGetParameter((short)j);

					if (!par)
					{
						//Error("Error - No parameter names with offset %i.", j);
//						printf("No parameter names with offset %i\n", j);
						if (p<8)
							j += df->parm_size[p];
						else
							j += 1;
						if (p<8&&df->parm_size[p] == 3)
						{
							if (j < (df->parm_start) + ps)
								QC_snprintfz(line, sizeof(line), "vector par%i, ", p);
							else
								QC_snprintfz(line, sizeof(line), "vector par%i", p);
						}
						else
						{
							if (j < (df->parm_start) + ps)
								QC_snprintfz(line, sizeof(line), "__variant par%i, ", p);
							else
								QC_snprintfz(line, sizeof(line), "__variant par%i", p);
						}
					}
					else
					{
						if (par->type == ev_vector)
							j += 2;
						j++;
						if (j < (df->parm_start) + ps)
						{
							QC_snprintfz(line, sizeof(line), "%s, ", DecompilePrintParameter(par));
						}
						else
						{
							QC_snprintfz(line, sizeof(line), "%s", DecompilePrintParameter(par));
						}
					}
					strcat(fname, line);
				}
			}
			strcat(fname, ") ");
			line[0] = '\0';
			QC_snprintfz(line, sizeof(line), strings + functions[i].s_name);
			strcat(fname, line);

		}

		knew = (char *)malloc(strlen(fname) + 1);
		strcpy(knew, fname);
		DecompileProfiles[i] = knew;
	}

}

QCC_ddef_t *GlobalAtOffset(dfunction_t *df, gofs_t ofs)
{
	QCC_ddef_t *def;
	int i, j;

	def = globalofsdef[ofs];
	if (def)
		return def;

	for (i = 0; i < numglobaldefs; i++)
	{
		def = &globals[i];

		if (def->ofs == ofs)
		{

			/*if (!strings[def->s_name])
			{
				char line[16];
				char *buf;

				sprintf(line, "_s_%i", def->ofs);	//globals, which are defined after the locals of the function they are first used in...
				buf = malloc(strlen(line)+1);		//must be static variables, but we can't handle them very well
				strcpy(buf, line);
				def->s_name = buf - strings;
			}*/
			globalofsdef[ofs] = def;
			return def;
		}
	}

	if (ofs >= df->parm_start && ofs < df->parm_start + df->locals)
	{
		static QCC_ddef_t parm[8];
		static char *parmnames[] = {"par0","par1","par2","par3","par4","par5","par6","par7"};
		int parmofs = ofs - df->parm_start;
		for (i = 0; i < df->numparms && i < 8; i++)
		{
			if (parmofs < df->parm_size[i])
			{
				parm[i].ofs = ofs - parmofs;
				parm[i].s_name = parmnames[i]-strings;
				parm[i].type = ev_void;

				ofs = parm[i].ofs;
				for (j = 0; j < numglobaldefs; j++)
				{
					def = &globals[j];
					if (def->ofs == ofs)
					{
						char line[256], *buf;
						sprintf(line, "%s_%c", strings+def->s_name, 'x'+parmofs);	//globals, which are defined after the locals of the function they are first used in...
						def = malloc(sizeof(*def)+strlen(line)+1);		//must be static variables, but we can't handle them very well
						buf = (char*)(def+1);
						strcpy(buf, line);
						def->s_name = buf - strings;
						def->type = ev_float;
						return def;
					}
				}

				return &parm[i];
			}
			parmofs -= df->parm_size[i];
		}
		//moo
	}
	//FIXME: if its within the current function's bounds, its:
	//	within param list: argument
	//	never written: immediate
	//	optimised: a local / locked temp.
	//	vanilla qcc: always a temp (other locals will be named)
	//elsewhere:
	//	if its assigned to somewhere, then its a temp
	//	otherwise its a const.

	return NULL;
}

char *DecompileGlobal(dfunction_t *df, gofs_t ofs, QCC_type_t * req_t)
{
	int i;
	QCC_ddef_t *def;
	static char line[8192];
	char *res;

	line[0] = '\0';

	/*if (req_t == &def_short)
	{
		QC_snprintfz(line, sizeof(line), "%ii", ofs);
		res = (char *)malloc(strlen(line) + 1);
		strcpy(res, line);
		return res;
	}*/

	def = GlobalAtOffset(df, ofs);

	if (def)
	{
		const char *defname = qcstring(def->s_name);

		if (!strcmp(defname, "IMMEDIATE") || !strcmp(defname, ".imm") || !def->s_name)
		{
			etype_t ty;
			if (!req_t)
				ty = def->type;
			else
			{
				ty = (etype_t)(req_t->type);
				if (!ty)
					ty = def->type;
			}
			QC_snprintfz(line, sizeof(line), "%s", DecompileValueString(ty, &pr_globals[def->ofs]));
		}
		else 
		{
			if (!*defname)
			{
				char line[16];
				char *buf;
				QCC_ddef_t *parent;
				if (ofs >= df->parm_start && ofs < df->parm_start + df->locals)
					goto lookslikealocal;
				else if ((parent = GlobalAtOffset(df, ofs-1)) && parent->type == ev_vector)
				{	// _y
					QC_snprintfz(line, sizeof(line), "%s_y", strings+parent->s_name);	//globals, which are defined after the locals of the function they are first used in...
					buf = malloc(strlen(line)+1);		//must be static variables, but we can't handle them very well
					strcpy(buf, line);
					def->s_name = buf - strings;
				}
				else if ((parent = GlobalAtOffset(df, ofs-2)) && parent->type == ev_vector)
				{	// _z
					QC_snprintfz(line, sizeof(line), "%s_z", strings+parent->s_name);	//globals, which are defined after the locals of the function they are first used in...
					buf = malloc(strlen(line)+1);		//must be static variables, but we can't handle them very well
					strcpy(buf, line);
					def->s_name = buf - strings;
				}
				else
				{
					QC_snprintfz(line, sizeof(line), "_sloc_%i", def->ofs);	//globals, which are defined after the locals of the function they are first used in...
					buf = malloc(strlen(line)+1);		//must be static variables, but we can't handle them very well
					strcpy(buf, line);
					def->s_name = buf - strings;
				}
			}

			QC_snprintfz(line, sizeof(line), "%s", strings + def->s_name);
			if (def->type == ev_field && req_t == type_field && req_t->aux_type == type_float && DecompileGetFieldTypeByDef(def) == ev_vector)
				strcat(line, "_x");
			else if (def->type == ev_vector && req_t == type_float)
				strcat(line, "_x");

		}
		res = (char *)malloc(strlen(line) + 1);
		strcpy(res, line);

		return res;
	}

	if (ofs >= df->parm_start && ofs < df->parm_start + df->locals)
	{
		int parmofs;
lookslikealocal:
		QC_snprintfz(line, sizeof(line), "local_%i", ofs);
		for (i = 0, parmofs = ofs - df->parm_start; i < df->numparms && i < 8; i++)
		{
			if (parmofs < df->parm_size[i])
			{
				if (parmofs)
					QC_snprintfz(line, sizeof(line), "par%i_%c", i, 'x'+parmofs);
				else
					QC_snprintfz(line, sizeof(line), "par%i", i);
				break;
			}
			parmofs -= df->parm_size[i];
		}
		if (!assumelocals && i == df->numparms)
			return NULL;	//we don't know what this is. assume its a temp

		res = (char *)malloc(strlen(line) + 1);
		strcpy(res, line);
		return res;
	}

	if (assumeglobals)
	{	//unknown globals are normally assumed to be temps
		if (ofs >= ofs_parms[7]+ofs_size)
		{
			QC_snprintfz(line, sizeof(line), "tmp_%i", ofs);
			res = (char *)malloc(strlen(line) + 1);
			strcpy(res, line);

			return res;
		}
	}

	return NULL;
}

static struct
{
	char *text;
	QCC_type_t *type;
} IMMEDIATES[MAX_REGS];
gofs_t DecompileScaleIndex(dfunction_t *df, gofs_t ofs)
{
	gofs_t nofs = 0;

	/*if (ofs > ofs_parms[7]+ofs_size)
		nofs = ofs - df->parm_start + ofs_parms[7]+ofs_size;
	else*/
		nofs = ofs;

	if ((nofs < 0) || (nofs > MAX_REGS - 1))
	{
		printf("Fatal Error - Index (%i) out of bounds.\n", nofs);
		return 0;
		exit(1);
	}

	return nofs;
}

void DecompileImmediate_Free(void)
{
	int i;
	for (i = 0; i < MAX_REGS; i++)
	{
		if (IMMEDIATES[i].text) 
		{
			free(IMMEDIATES[i].text);
			IMMEDIATES[i].text = NULL;
		}
	}
}
void DecompileImmediate_Insert(dfunction_t *df, gofs_t ofs, char *knew, QCC_type_t *type)
{
	QCC_ddef_t *d;
	int nofs;

	nofs = DecompileScaleIndex(df, ofs);

	if (IMMEDIATES[nofs].text)
	{
//		fprintf(Decompileofile, "/*WARNING: Discarding \"%s\"/", IMMEDIATES[nofs]);
		free(IMMEDIATES[nofs].text);
		IMMEDIATES[nofs].text = NULL;
	}


	d = GlobalAtOffset(df, ofs);
	if (d && d->s_name)// && strcmp(strings+d->s_name, "IMMEDIATE"))
	{	//every operator has a src (or two) and a dest.
		//many compilers optimise by using the dest of a maths/logic operator to store to a local/global
		//they then skip off the storeopcode.
		//without this, we would never see these stores.
		IMMEDIATES[nofs].text = NULL;
		IMMEDIATES[nofs].type = NULL;

		QCC_CatVFile(Decompileofile, "%s = %s;\n", strings + d->s_name, knew);
	}
	else
	{
		IMMEDIATES[nofs].text = (char *)malloc(strlen(knew) + 1);
		strcpy(IMMEDIATES[nofs].text, knew);
		IMMEDIATES[nofs].type = type;
	}
}

void FloatToString(char *out, size_t outsize, float f)
{
	char *e;
	QC_snprintfz(out, outsize, "%f", f);

	//trim any trailing decimals
	e = strchr(out, '.');
	if (e)
	{
		e = e+strlen(e);
		while (e > out && e[-1] == '0')
			e--;
		if (e > out && e[-1] == '.')
			e--;
		*e = 0;
	}
}

char *DecompileImmediate_Get(dfunction_t *df, gofs_t ofs, QCC_type_t *req_t)
{
	char *res;
 
	gofs_t nofs;

	nofs = DecompileScaleIndex(df, ofs);
  //  printf("DecompileImmediate - Index scale: %i -> %i.\n", ofs, nofs);

	// insert at nofs 
	if (IMMEDIATES[nofs].text)
	{

	//	printf("DecompileImmediate - Reading \"%s\" at index %i.\n", IMMEDIATES[nofs], nofs);

		if (IMMEDIATES[nofs].type == type_vector && req_t == type_float)
		{
			res = (char *)malloc(strlen(IMMEDIATES[nofs].text) + 4);
			if (strchr(IMMEDIATES[nofs].text, '('))
				sprintf(res, "%s[0]", IMMEDIATES[nofs].text);
			else
				sprintf(res, "%s_x", IMMEDIATES[nofs].text);
		}
		else
		{
			res = (char *)malloc(strlen(IMMEDIATES[nofs].text) + 1);
			strcpy(res, IMMEDIATES[nofs].text);
		}

		return res;
	}
	else
	{	//you are now entering the hack zone.
		char temp[8192];
		
		switch(req_t?req_t->type:-1)
		{
		case ev_void:	//for lack of any better ideas.
		case ev_float:
			//denormalised floats need special handling.
			if ((0x7fffffff&*(int*)&pr_globals[ofs]) >= 1 && (0x7fffffff&*(int*)&pr_globals[ofs]) < 0x00800000)
			{
				QC_snprintfz(temp, sizeof(temp), "((float)(__variant)%ii)", *(int*)&pr_globals[ofs]);

//				if (req_t && *(int*)&pr_globals[ofs] >= 1 && *(int*)&pr_globals[ofs] < strofs)
//					;	//failure to break means we'll print out a trailing /*string*/
//				else
					break;
			}
			else
			{
				FloatToString(temp, sizeof(temp), pr_globals[ofs]);
				break;
			}
		case ev_string:
			{
				const char *in;
				char *out;
				if (((int*)pr_globals)[ofs] < 0 || ((int*)pr_globals)[ofs] > strofs)
				{
					printf("Hey! That's not a string! error in %s\n", strings + df->s_name);
					QC_snprintfz(temp, sizeof(temp), "%f", pr_globals[ofs]);
					break;
				}
				in = &strings[((int*)pr_globals)[ofs]];
				out = temp;
				if (req_t->type != ev_string)
				{
					QC_snprintfz(temp, sizeof(temp), "/*%i*/", ((int*)pr_globals)[ofs]);
					out += strlen(out);
				}

				*out++ =  '\"';
				while (*in)
				{
					if (*in == '\"')
					{
						*out++ = '\\';
						*out++ = '\"';
						in++;
					}
					else if (*in == '\n')
					{
						*out++ = '\\';
						*out++ = 'n';
						in++;
					}
					else if (*in == '\\')
					{
						*out++ = '\\';
						*out++ = '\\';
						in++;
					}
					else if (*in == '\r')
					{
						*out++ = '\\';
						*out++ = 'r';
						in++;
					}
					else if (*in == '\a')
					{
						*out++ = '\\';
						*out++ = 'a';
						in++;
					}
					else if (*in == '\b')
					{
						*out++ = '\\';
						*out++ = 'b';
						in++;
					}
					else if (*in == '\f')
					{
						*out++ = '\\';
						*out++ = 'f';
						in++;
					}
					else if (*in == '\t')
					{
						*out++ = '\\';
						*out++ = 't';
						in++;
					}
					else if (*in == '\v')
					{
						*out++ = '\\';
						*out++ = 'v';
						in++;
					}
					else
						*out++ = *in++;
				}
				*out++ =  '\"';
				*out++ =  '\0';
			}
			break;
		case ev_vector:
			{
				char x[64], y[64], z[64];
				FloatToString(x, sizeof(x), pr_globals[ofs+0]);
				FloatToString(y, sizeof(y), pr_globals[ofs+1]);
				FloatToString(z, sizeof(z), pr_globals[ofs+2]);
				QC_snprintfz(temp, sizeof(temp), "\'%s %s %s\'", x, y, z);
			}
			break;
//		case ev_quat:
//			QC_snprintfz(temp, sizeof(temp), "\'%f %f %f %f\'", pr_globals[ofs],pr_globals[ofs+1],pr_globals[ofs+2],pr_globals[ofs+3]);
//			break;
		case ev_integer:
			QC_snprintfz(temp, sizeof(temp), "%ii", ((int*)pr_globals)[ofs]);
			break;
//		case ev_uinteger:
//			QC_snprintfz(temp, sizeof(temp), "%uu", ((int*)pr_globals)[ofs]);
//			break;
		case ev_pointer:
			QC_snprintfz(temp, sizeof(temp), "(__variant*)0x%xi", ((int*)pr_globals)[ofs]);
			break;
		case ev_function:
			if (!((int*)pr_globals)[ofs])
				QC_snprintfz(temp, sizeof(temp), "__NULL__/*func*/");
			else if (((int*)pr_globals)[ofs] > 0 && ((int*)pr_globals)[ofs] < numfunctions && functions[((int*)pr_globals)[ofs]].s_name>0)
				QC_snprintfz(temp, sizeof(temp), "%s/*immediate*/", strings+functions[((int*)pr_globals)[ofs]].s_name);
			else
				QC_snprintfz(temp, sizeof(temp), "((__variant(...))%i)", ((int*)pr_globals)[ofs]);
			break;
		case ev_entity:
			if (!pr_globals[ofs])
				QC_snprintfz(temp, sizeof(temp), "((entity)__NULL__)");
			else
				QC_snprintfz(temp, sizeof(temp), "(entity)%i", ((int*)pr_globals)[ofs]);
			break;
		case ev_field:
			if (!pr_globals[ofs])
				QC_snprintfz(temp, sizeof(temp), "((.void)__NULL__)");
			else
				QC_snprintfz(temp, sizeof(temp), "/*field %s*/%i", DecompileGetFieldNameIdxByFinalOffset(((int*)pr_globals)[ofs]), ((int*)pr_globals)[ofs]);
			break;
		default:
			QC_snprintfz(temp, sizeof(temp), "FIXME");
			break;
		}

		res = (char *)malloc(strlen(temp) + 1);
		strcpy(res, temp);

		return res;

	}
	return NULL;
}

char *DecompileGet(dfunction_t *df, gofs_t ofs, QCC_type_t *req_t)
{
	char *farg1;
	/*if (req_t == &def_short)
	{
		char temp[16];
		QC_snprintfz(temp, sizeof(temp), "%i", ofs);
		return strdup(temp);
	}*/
	farg1 = NULL;

	farg1 = DecompileGlobal(df, ofs, req_t);

	if (farg1 == NULL)
		farg1 = DecompileImmediate_Get(df, ofs, req_t);

	return farg1;
}

void DecompilePrintStatement(dstatement_t *s);

void DecompileIndent(int c)
{
	int i;

	if (c < 0)
		c = 0;

	for (i = 0; i < c; i++) 
	{
		QCC_CatVFile(Decompileofile, "\t");
	}
}

void DecompileOpcode(dfunction_t *df, int a, int b, int c, char *opcode, QCC_type_t *typ1, QCC_type_t *typ2, QCC_type_t *typ3, int usebrackets, int *indent)
{
	static char line[512];
	char *arg1, *arg2, *arg3;
	arg1 = DecompileGet(df, a, typ1);
	arg2 = DecompileGet(df, b, typ2);
	arg3 = DecompileGlobal(df, c, typ3);

	if (arg3)
	{
		DecompileIndent(*indent);
		if (usebrackets)
			QCC_CatVFile(Decompileofile, "%s = %s %s %s;\n", arg3, arg1, opcode, arg2);
		else
			QCC_CatVFile(Decompileofile, "%s = %s%s%s;\n", arg3, arg1, opcode, arg2);
	}
	else
	{
		if (usebrackets)
			QC_snprintfz(line, sizeof(line), "(%s %s %s)", arg1, opcode, arg2);
		else
			QC_snprintfz(line, sizeof(line), "%s%s%s", arg1, opcode, arg2);
		DecompileImmediate_Insert(df, c, line, typ3);
	}
}

void DecompileDecompileStatement(dfunction_t * df, dstatement_t * s, int *indent)
{
	static char line[8192];
	static char fnam[512];
	char *arg1, *arg2, *arg3;
	int nargs, i, j;
	dstatement_t *t;
	unsigned int dom, doc, ifc, tom;
	QCC_type_t *typ1, *typ2, *typ3;
	QCC_ddef_t *par;
	dstatement_t *k;
	int dum;


	arg1 = arg2 = arg3 = NULL;

	line[0] = '\0';
	fnam[0] = '\0';

	dom = s->op;

	doc = dom / OP_MARK_END_DO;
	ifc = (dom % OP_MARK_END_DO) / OP_MARK_END_ELSE;

	// use program flow information 

	for (i = 0; i < ifc; i++) 
	{
		(*indent)--;
		DecompileIndent(*indent);
		QCC_CatVFile(Decompileofile, "}\n");//FrikaC style modification
	}
	for (i = 0; i < doc; i++) 
	{
		DecompileIndent(*indent);
		QCC_CatVFile(Decompileofile, "do\n");
		DecompileIndent(*indent);
		QCC_CatVFile(Decompileofile, "{\n");
		(*indent)++;
	}

	/*
	 * remove all program flow information 
	 */
	s->op %= OP_MARK_END_ELSE;
	typ1 = pr_opcodes[s->op].type_a?*pr_opcodes[s->op].type_a:NULL;
	typ2 = pr_opcodes[s->op].type_b?*pr_opcodes[s->op].type_b:NULL;
	typ3 = pr_opcodes[s->op].type_c?*pr_opcodes[s->op].type_c:NULL;

	/*
	 * printf("DecompileDecompileStatement - decompiling %i (%i):\n",(int)(s - statements),dom);
	 * DecompilePrintStatement (s);  
	 */
	/*
	 * states are handled at top level 
	 */
	if (s->op == OP_DONE)
	{

	}
	else if (s->op == OP_BOUNDCHECK)
	{
		/*these are auto-generated as a sideeffect. currently there is no syntax to explicitly use one (other than asm), but we don't want to polute the code when they're autogenerated, so ditch them all*/
	}
	else if (s->op == OP_STATE) 
	{

		par = DecompileGetParameter(s->a);
		if (!par)
		{
			printf("Error - Can't determine frame number.\n");
			exit(1);
		}
		arg2 = DecompileGet(df, s->b, NULL);
		if (!arg2)
		{
			printf("Error - No state parameter with offset %i.\n", s->b);
			exit(1);
		}
		DecompileIndent(*indent);
		QCC_CatVFile(Decompileofile, "state [ %s, %s ];\n", DecompileValueString((etype_t)(par->type), &pr_globals[par->ofs]), arg2);

	//	free(arg2);
	}
	else if (s->op == OP_RETURN/* || s->op == OPQF_RETURN_V*/)
	{
		DecompileIndent(*indent);
		QCC_CatVFile(Decompileofile, "return");

		if (s->a)
		{
			QCC_CatVFile(Decompileofile, " ");
			arg1 = DecompileGet(df, s->a, type_void);	//FIXME: we should know the proper type better than this.
			QCC_CatVFile(Decompileofile, "(%s)", arg1);
		}
		QCC_CatVFile(Decompileofile, ";\n");

	}
	else if (pr_opcodes[s->op].flags & OPF_STD)
	{
		DecompileOpcode(df, s->a, s->b, s->c, pr_opcodes[s->op].name, typ1, typ2, typ3, true, indent);
	}
	else if ((pr_opcodes[s->op].flags & OPF_LOADPTR) || OP_GLOBALADDRESS == s->op)
	{
		arg1 = DecompileGet(df, s->a, typ1);
		arg2 = DecompileGet(df, s->b, typ2);
		arg3 = DecompileGlobal(df, s->c, typ3);

		if (arg3)
		{
			DecompileIndent(*indent);
			if (s->b)
				QCC_CatVFile(Decompileofile, "%s = &%s[%s];\n", arg3, arg1, arg2);
			else
				QCC_CatVFile(Decompileofile, "%s = &%s;\n", arg3, arg1);
		}
		else
		{
			if (s->b)
				QC_snprintfz(line, sizeof(line), "%s[%s]", arg1, arg2);
			else
				QC_snprintfz(line, sizeof(line), "%s", arg1);
			DecompileImmediate_Insert(df, s->c, line, typ3);
		}
	}
	else if ((OP_LOAD_F <= s->op && s->op <= OP_ADDRESS) || s->op == OP_LOAD_P || s->op == OP_LOAD_I)
	{
		if (s->op == OP_ADDRESS)
		{
			QCC_ddef_t *def = GlobalAtOffset(df, s->b);
			if (def && DecompileGetFieldTypeByDef(def) == ev_vector)
				typ3 = type_vector;
		}

		type_field->aux_type = typ3;
		DecompileOpcode(df, s->a, s->b, s->c, ".", typ1, typ2, typ3, false, indent);
		type_field->aux_type = NULL;
	}
	else if ((OP_LOADA_F <= s->op && s->op <= OP_LOADA_I))// || (OPQF_LOADBI_F <= s->op && s->op <= OPQF_LOADBI_P))
	{
		static char line[512];
		char *arg1, *arg2, *arg3;
		arg1 = DecompileGet(df, s->a, typ1);
		arg2 = DecompileGet(df, s->b, typ2);
		arg3 = DecompileGlobal(df, s->c, typ3);

		if (arg3) 
		{
			DecompileIndent(*indent);
			QCC_CatVFile(Decompileofile, "%s = %s[%s];\n", arg3, arg1, arg2);
		}
		else
		{
			QC_snprintfz(line, sizeof(line), "%s[%s]", arg1, arg2);
			DecompileImmediate_Insert(df, s->c, line, typ3);
		}
	}
	else if (pr_opcodes[s->op].flags & OPF_STORE)
	{
		QCC_type_t *parmtype=NULL;
		if (s->b >= ofs_parms[0] && s->b < ofs_parms[7]+ofs_size)
		{	//okay, so typ1 might not be what the store type says it should be.
			k = s+1;
			while(k->op%OP_MARK_END_ELSE)
			{
				if ((k->op >= OP_CALL0 && k->op <= OP_CALL8) || (k->op >= OP_CALL1H && k->op <= OP_CALL8H))
				{
					//well, this is it.
					int fn;
					int pn;
					dfunction_t *cf;
					QCC_ddef_t *def;
					fn = ((int*)pr_globals)[k->a];
					cf = &functions[fn];
					if (cf->first_statement<=0 && cf->first_statement > -(signed)(sizeof(builtins)/sizeof(builtins[0])))	//builtins don't have this info.
					{
						QCC_type_t **p = builtins[-cf->first_statement].params[(s->b-ofs_parms[0])/ofs_size];
						parmtype = p?*p:NULL;
					}
					else
					{
						fn = cf->parm_start;
						for (pn = 0; pn < (s->b-ofs_parms[0])/ofs_size; pn++)
							fn += cf->parm_size[pn];

						def = DecompileGetParameter(fn);
						if (def)
						{
							switch(def->type)
							{
							case ev_float:
								parmtype = type_float;
								break;
							case ev_string:
								parmtype = type_string;
								break;
							case ev_vector:
								parmtype = type_vector;
								break;
							case ev_entity:
								parmtype = type_entity;
								break;
							case ev_function:
								parmtype = type_function;
								break;
							case ev_integer:
								parmtype = type_integer;
								break;
//							case ev_uinteger:
//								parmtype = type_uinteger;
//								break;
//							case ev_quat:
//								parmtype = type_quat;
//								break;
							default:
//								parmtype = type_float;
								break;
							}
						}
					}
					break;
				}
				else if (OP_STORE_F <= s->op && s->op <= OP_STORE_FNC)
				{
					if (k->b < s->b)	//whoops... older QCCs can nest things awkwardly.
						break;
				}
				k++;
			}
		}

		if (parmtype)
			arg1 = DecompileGet(df, s->a, parmtype);
		else
			arg1 = DecompileGet(df, s->a, typ1);
		arg3 = DecompileGlobal(df, s->b, typ2);

		if (arg3)
		{
			DecompileIndent(*indent);
			QCC_CatVFile(Decompileofile, "%s %s %s;\n", arg3, pr_opcodes[s->op].name, arg1);
		}
		else
		{
			QC_snprintfz(line, sizeof(line), "%s", arg1);
			DecompileImmediate_Insert(df, s->b, line, NULL);
		}

	}
	else if (pr_opcodes[s->op].flags & OPF_STOREPTR)
	{
		arg1 = DecompileGet(df, s->a, typ2);
		//FIXME: we need to deal with ref types and other crazyness, so we know whether we need to add * or *& or if we can skip that completely
		arg2 = DecompileGet(df, s->b, typ2);

		DecompileIndent(*indent);
		QCC_CatVFile(Decompileofile, "%s %s %s;\n", arg2, pr_opcodes[s->op].name, arg1);

	}
	else if (OP_CONV_FTOI == s->op)
	{
		arg1 = DecompileGet(df, s->a, typ1);
		QC_snprintfz(line, sizeof(line), "(int)%s", arg1);
		DecompileImmediate_Insert(df, s->c, line, type_integer);
	}
	else if (OP_CONV_ITOF == s->op)
	{
		arg1 = DecompileGet(df, s->a, typ1);
		QC_snprintfz(line, sizeof(line), "(float)%s", arg1);
		DecompileImmediate_Insert(df, s->c, line, type_float);
	}
	else if (OP_RAND0 == s->op)
	{
		DecompileImmediate_Insert(df, ofs_return, "random()", type_float);
	}
	else if (OP_RAND1 == s->op)
	{
		arg1 = DecompileGet(df, s->a, typ1);
		QC_snprintfz(line, sizeof(line), "random(%s)", arg1);
		DecompileImmediate_Insert(df, ofs_return, line, type_float);
	}
	else if (OP_RAND2 == s->op)
	{
		arg1 = DecompileGet(df, s->a, typ1);
		arg2 = DecompileGet(df, s->b, typ2);
		QC_snprintfz(line, sizeof(line), "random(%s, %s)", arg1, arg2);
		DecompileImmediate_Insert(df, ofs_return, line, type_float);
	}
	else if (OP_NOT_F <= s->op && s->op <= OP_NOT_FNC)
	{

		arg1 = DecompileGet(df, s->a, typ1);
		QC_snprintfz(line, sizeof(line), "!%s", arg1);
		DecompileImmediate_Insert(df, s->c, line, type_float);

	}
	else if ((OP_CALL0 <= s->op && s->op <= OP_CALL8) || (OP_CALL1H <= s->op && s->op <= OP_CALL8H))
	{
		if (OP_CALL1H <= s->op && s->op <= OP_CALL8H)
			nargs = (s->op - OP_CALL1H) + 1;
		else
			nargs = s->op - OP_CALL0;

		arg1 = DecompileGet(df, s->a, type_function);
		QC_snprintfz(line, sizeof(line), "%s(", arg1);
		QC_snprintfz(fnam, sizeof(fnam), "%s", arg1);

		for (i = 0; i < nargs; i++)
		{

			typ1 = NULL;

			if (i == 0 && OP_CALL1H <= s->op && s->op <= OP_CALL8H)
				j = s->b;
			else if (i == 1 && OP_CALL1H <= s->op && s->op <= OP_CALL8H)
				j = s->c;
			else
				j = ofs_parms[i];

			if (arg1)
			free(arg1);

			arg1 = DecompileGet(df, (gofs_t)j, typ1);
			strcat(line, arg1);

			if (i < nargs - 1)
				strcat(line, ", ");//frikqcc modified
		}

		strcat(line, ")");
		DecompileImmediate_Insert(df, ofs_return, line, NULL);

	/*
	 * if ( ( ( (s+1)->a != 1) && ( (s+1)->b != 1) && 
	 * ( (s+2)->a != 1) && ( (s+2)->b != 1) ) || 
	 * ( ((s+1)->op) % OP_MARK_END_ELSE == OP_CALL0 ) ) {
	 * DecompileIndent(*indent);
	 * fprintf(Decompileofile,"%s;\n",line);
	 * }
	 */

		if ((((s + 1)->a != ofs_return) && ((s + 1)->b != ofs_return) &&
			((s + 2)->a != ofs_return) && ((s + 2)->b != ofs_return)) ||
			((((s + 1)->op) % OP_MARK_END_ELSE == OP_CALL0) && ((((s + 2)->a != ofs_return)) || ((s + 2)->b != ofs_return))))
		{
			DecompileIndent(*indent);
			QCC_CatVFile(Decompileofile, "%s;\n", line);
		}
	}
	else if (s->op == OP_IF_I || s->op == OP_IFNOT_I ||
			 s->op == OP_IF_F || s->op == OP_IFNOT_F ||
			 s->op == OP_IF_S || s->op == OP_IFNOT_S/* || s->op == OPQF_IFA || s->op == OPQF_IFB || s->op == OPQF_IFAE || s->op == OPQF_IFBE*/)
	{

		arg1 = DecompileGet(df, s->a, type_float);	//FIXME: this isn't quite accurate...
		arg2 = DecompileGlobal(df, s->a, NULL);

		if (s->op == OP_IFNOT_I || s->op == OP_IFNOT_F || s->op == OP_IFNOT_S)
		{
	lameifnot:
			if ((signed int)s->b < 1)
			{
//				if (arg1)
//					free(arg1);
//				if (arg2)
//					free(arg2);
//				if (arg3)
//					free(arg3);

				return;


				printf("Found a negative IFNOT jump.\n");
				exit(1);
			}

			/*
			 * get instruction right before the target 
			 */
			t = s + (signed int)s->b - 1;
			tom = t->op % OP_MARK_END_ELSE;

			if (tom != OP_GOTO)
			{

				/*
				 * pure if 
				 */
				DecompileIndent(*indent);
				QCC_CatVFile(Decompileofile, "if (%s)\n", arg1);//FrikaC modified
				DecompileIndent(*indent);
				QCC_CatVFile(Decompileofile, "{\n");

				(*indent)++;

			}
			else
			{
			
				if ((signed int)t->a > 0)
				{
					/*
					 * ite 
					 */

					DecompileIndent(*indent);
					QCC_CatVFile(Decompileofile, "if (%s)\n", arg1);//FrikaC modified
					DecompileIndent(*indent);
					QCC_CatVFile(Decompileofile, "{\n");

					(*indent)++;

				}
				else
				{


					if (((signed int)t->a + (signed int)s->b) > 1)
					{
						/*
						 * pure if  
						 */

						DecompileIndent(*indent);
						QCC_CatVFile(Decompileofile, "if (%s)\n", arg1);//FrikaC modified
						DecompileIndent(*indent);
						QCC_CatVFile(Decompileofile, "{\n");
						(*indent)++;
					}
					else
					{

						dum = 1;
						for (k = t + (signed int)(t->a); k < s; k++)
						{
							tom = k->op % OP_MARK_END_ELSE;
							if (tom == OP_GOTO ||
								tom == OP_IF_I || tom == OP_IFNOT_I ||
								tom == OP_IF_F || tom == OP_IFNOT_F ||
								tom == OP_IF_S || tom == OP_IFNOT_S)
								dum = 0;
						}
						if (dum) 
						{

							DecompileIndent(*indent);
							QCC_CatVFile(Decompileofile, "while (%s)\n", arg1);
							DecompileIndent(*indent); //FrikaC
							QCC_CatVFile(Decompileofile, "{\n");
							(*indent)++;
						}
						else
						{

							DecompileIndent(*indent);


							QCC_CatVFile(Decompileofile, "if (%s)\n", arg1);//FrikaC modified
							DecompileIndent(*indent);
							QCC_CatVFile(Decompileofile, "{\n");
							(*indent)++;
						}
					}
				}
			}
		}
		/*else if (s->op == OPQF_IFA)
		{
			char *t = arg1;
			arg1 = malloc(strlen(arg1)+8);
			sprintf(arg1, "(%s) <= 0", t);
			free(t);
			goto lameifnot;
		}
		else if (s->op == OPQF_IFAE)
		{
			char *t = arg1;
			arg1 = malloc(strlen(arg1)+7);
			sprintf(arg1, "(%s) < 0", t);
			free(t);
			goto lameifnot;
		}
		else if (s->op == OPQF_IFB)
		{
			char *t = arg1;
			arg1 = malloc(strlen(arg1)+8);
			sprintf(arg1, "(%s) >= 0", t);
			free(t);
			goto lameifnot;
		}
		else if (s->op == OPQF_IFBE)
		{
			char *t = arg1;
			arg1 = malloc(strlen(arg1)+7);
			sprintf(arg1, "(%s) > 0", t);
			free(t);
			goto lameifnot;
		}*/
		else
		{
			if ((signed int)s->b>0)
			{
				char *t = arg1;
				//if (!...)

				arg1 = malloc(strlen(arg1)+2);
				sprintf(arg1, "!%s", t);
				free(t);
				goto lameifnot;
			}
			else
			{
			/*
			 * do ... while 
			 */

				(*indent)--;
				QCC_CatVFile(Decompileofile, "\n");
				DecompileIndent(*indent);
				QCC_CatVFile(Decompileofile, "} while (%s);\n", arg1);
			}

		}

	}
	else if (s->op == OPD_GOTO_FORSTART)
	{
		DecompileIndent(*indent);
		QCC_CatVFile(Decompileofile, "do_tail\n", (s-statements) + (signed int)s->a);
		DecompileIndent(*indent);
		QCC_CatVFile(Decompileofile, "{\n");
		(*indent)++;
	}
	else if (s->op == OPD_GOTO_WHILE1)
	{
		(*indent)--;
		DecompileIndent(*indent);
		QCC_CatVFile(Decompileofile, "} while(1);\n");
	}
	else if (s->op == OP_GOTO)
	{

		if ((signed int)s->a > 0)
		{
			/*
			 * else 
			 */
			(*indent)--;
			DecompileIndent(*indent);
			QCC_CatVFile(Decompileofile, "}\n");
			DecompileIndent(*indent);
			QCC_CatVFile(Decompileofile, "else\n");
			DecompileIndent(*indent);
			QCC_CatVFile(Decompileofile, "{\n");
			(*indent)++;

		}
		else
		{
			/*
			 * while 
			 */
			(*indent)--;

			DecompileIndent(*indent);
			QCC_CatVFile(Decompileofile, "}\n");

		}

	}
	else
	{
		int op = s->op%OP_MARK_END_ELSE;
		if (op <= OP_BITOR_F && pr_opcodes[s->op].opname)
			printf("warning: Unknown usage of OP_%s", pr_opcodes[s->op].opname);
		else
		{
			DecompileIndent(*indent);
			QCC_CatVFile(Decompileofile, "[OP_%s", pr_opcodes[op].opname);
			if (s->a)
				QCC_CatVFile(Decompileofile, ", %s", DecompileGet(df, s->a, typ1));
			if (s->b)
				QCC_CatVFile(Decompileofile, ", %s", DecompileGet(df, s->b, typ1));
			if (s->c)
				QCC_CatVFile(Decompileofile, ", %s", DecompileGet(df, s->c, typ1));
			QCC_CatVFile(Decompileofile, "]\n");
			printf("warning: Unknown opcode %i in %s\n", op, strings + df->s_name);
		}

	}


//    printf("DecompileDecompileStatement - Current line is \"%s\"\n", line);


	if (arg1)
		free(arg1);
	if (arg2)
		free(arg2);
	if (arg3)
		free(arg3);

	return;
}

pbool DecompileDecompileFunction(dfunction_t * df, dstatement_t *altdone)
{
	dstatement_t *ds;
	int indent;


	// Initialize 
 
	DecompileImmediate_Free();

	indent = 1;

	ds = statements + df->first_statement;
	if(ds->op == OP_STATE)
		ds++;
	while (1) 
	{
		if (ds == altdone)
		{
			//decompile the dummy done, cos we can
			DecompileDecompileStatement(df, statements, &indent);
			break;
		}
		DecompileDecompileStatement(df, ds, &indent);
		if (!ds->op)
			break;
		ds++;
	}

	if (indent != 1)
	{
		printf("warning: Indentation structure corrupt (in func %s)\n", strings+df->s_name);
		return false;
	}
	return true;
}

char *DecompileString(int qcstring)
{
	static char buf[8192];
	char *s;
	int c = 1;
	const char *string = strings+qcstring;
	if (qcstring < 0 || qcstring >= strofs)
		return "Invalid String";

	s = buf;
	*s++ = '"';
	while (string && *string)
	{
		if (c == sizeof(buf) - 2)
			break;
		if (*string == '\n')
		{
			*s++ = '\\';
			*s++ = 'n';
			c++;
		}
		else if (*string == '"')
		{
			*s++ = '\\';
			*s++ = '"';
			c++;
		}
		else
		{
			*s++ = *string;
			c++;
		}
		string++;
		if (c > (int)(sizeof(buf) - 10))
		{
			*s++ = '.';
			*s++ = '.';
			*s++ = '.';
			c += 3;
			break;
		}
	}
	*s++ = '"';
	*s++ = 0;
	return buf;
}

char *DecompileValueString(etype_t type, void *val)
{
	static char line[8192];

	line[0] = '\0';

	switch (type)
	{
		case ev_string:
			QC_snprintfz(line, sizeof(line), "%s", DecompileString(*(int *)val));
			break;
		case ev_void:
			QC_snprintfz(line, sizeof(line), "void");
			break;
		case ev_float:
			if (*(float *)val > 999999 || *(float *)val < -999999) // ugh
				QC_snprintfz(line, sizeof(line), "%.f", *(float *)val);
			else if ((!(*(int*)val & 0x7f800000) || (*(int*)val & 0x7f800000)==0x7f800000) && (*(int*)val & 0x7fffffff))
				QC_snprintfz(line, sizeof(line), "%%%i", *(int*)val);
			else if ((*(float *)val < 0.001) && (*(float *)val > 0))
				QC_snprintfz(line, sizeof(line), "%.6f", *(float *)val);
			else			
				QC_snprintfz(line, sizeof(line), "%g", *(float *)val);
			break;
		case ev_vector:
			QC_snprintfz(line, sizeof(line), "'%g %g %g'", ((float *)val)[0], ((float *)val)[1], ((float *)val)[2]);
			break;
//		case ev_quat:
//			QC_snprintfz(line, sizeof(line), "'%g %g %g %g'", ((float *)val)[0], ((float *)val)[1], ((float *)val)[2], ((float *)val)[3]);
//			break;
		case ev_field:
			DecompileGetFieldNameIdxByFinalOffset2(line, sizeof(line), *(int *)val);
			break;
		case ev_entity:
			QC_snprintfz(line, sizeof(line), "(entity)%ii", *(int *)val);
			break;
		case ev_integer:
			QC_snprintfz(line, sizeof(line), "%ii", *(int *)val);
			break;
//		case ev_uinteger:
//			QC_snprintfz(line, sizeof(line), "%uu", *(int *)val);
//			break;
		case ev_pointer:
			QC_snprintfz(line, sizeof(line), "(__variant*)0x%xi", *(int *)val);
			break;
		case ev_function:
			if (*(int *)val>0 && *(int *)val<numfunctions)
				QC_snprintfz(line, sizeof(line), "(/*func 0x%x*/%s)", *(int *)val, strings+functions[*(int *)val].s_name);
			else
				QC_snprintfz(line, sizeof(line), "((void())0x%xi)", *(int *)val);
			break;
		default:
			QC_snprintfz(line, sizeof(line), "bad type %i", type);
			break;
	}

	return line;
}

char *DecompilePrintParameter(QCC_ddef_t * def)
{
	static char line[128];
	static char debug[128];

	line[0] = '0';

	if (debug_offs)
	{
		QC_snprintfz(debug, sizeof(debug), " /*@%i*/", def->ofs);
	}
	else
		*debug = 0;

	if (!strings[def->s_name])	//null string...
	{
		QC_snprintfz(line, sizeof(line), "%s _p_%i%s", type_name(def), def->ofs, debug);
	}
	else if (!strcmp(strings + def->s_name, "IMMEDIATE") || !strcmp(strings + def->s_name, ".imm"))
	{
		QC_snprintfz(line, sizeof(line), "%s%s", DecompileValueString((etype_t)(def->type), &pr_globals[def->ofs]), debug);
	}
	else
	{
		QC_snprintfz(line, sizeof(line), "%s %s%s", type_name(def), strings + def->s_name, debug);
	}
	return line;
}

//we only work with prior fields.
const char *GetMatchingField(QCC_ddef_t *field)
{
	int i;
	QCC_ddef_t *def;
	int ld, lf;

	def = NULL;

	for (i = 0; i < numglobaldefs; i++)
	{
		def = &globals[i];

		if ((def->type&~DEF_SAVEGLOBAL) == ev_field)
		{
			if (((int*)pr_globals)[def->ofs] == field->ofs)
			{
				if (!strcmp(strings+def->s_name, strings+field->s_name))
					break;	//found ourself, give up.
				lf = strlen(strings + field->s_name);
				ld = strlen(strings + def->s_name);
				if (lf - 2 == ld)
				{
					if ((strings + field->s_name)[lf-2] == '_')
						if (!strncmp(strings + field->s_name, strings + def->s_name, ld))
							break;
				}
				return def->s_name+strings;
			}
		}
	}

	return NULL;
}

QCC_ddef_t *GetField(const char *name)
{
	int i;
	QCC_ddef_t *d;
	if (!*name)
		return NULL;

	if (*name == '.')
		name++;	//some idiot _intentionally_ decided to fuck shit up. go them!

	for (i = 0; i < numfielddefs; i++)
	{
		d = &fields[i];

		if (!strcmp(strings + d->s_name, name))
			return d;
	}
	return NULL;
}
QCC_ddef_t *DecompileGetParameter(gofs_t ofs)
{
	int i;
	QCC_ddef_t *def;

	def = NULL;

	for (i = 0; i < numglobaldefs; i++)
	{
		def = &globals[i];

		if (def->ofs == ofs)
		{
			return def;
		}
	}

	return NULL;
}
QCC_ddef_t *DecompileFindGlobal(const char *findname)
{
	int i;
	QCC_ddef_t *def;
	const char *defname;

	def = NULL;

	for (i = 0; i < numglobaldefs; i++)
	{
		def = &globals[i];
		defname = strings + def->s_name;

		if (!strcmp(findname, defname))
		{
			return def;
		}
	}

	return NULL;
}

QCC_ddef_t *DecompileFunctionGlobal(int funcnum)
{
	int i;
	QCC_ddef_t *def;

	def = NULL;

	for (i = 0; i < numglobaldefs; i++)
	{
		def = &globals[i];

		if (def->type == ev_function)
		{
			if (((int*)pr_globals)[def->ofs] == funcnum)
			{
				return def;
			}
		}
	}

	return NULL;
}

void DecompilePreceedingGlobals(int start, int end, const char *name)
{
	QCC_ddef_t *par;
	int j;
	QCC_ddef_t *ef;
	static char line[8192];

	const char *matchingfield;

	//print globals leading up to the function.
	for (j = start; j < end; j++)
	{

		par = DecompileGetParameter((gofs_t)j);

		if (par)
		{
			if (par->type & DEF_SAVEGLOBAL)
				par->type -= DEF_SAVEGLOBAL;

			if (par->type == ev_function)
			{
				if (strcmp(strings + par->s_name, "IMMEDIATE") && strcmp(strings + par->s_name, ".imm"))
				{
					if (strcmp(strings + par->s_name, name))
					{
						int f = ((int*)pr_globals)[par->ofs];
						//DecompileGetFunctionIdxByName(strings + par->s_name);
						if (f && strcmp(strings+functions[f].s_name, strings + par->s_name))
						{
							char *s = strrchr(DecompileProfiles[f], ' ');
							//happens with void() func = otherfunc;
							//such functions thus don't have their own type+body
							*s = 0;
							QCC_CatVFile(Decompileofile, "var %s %s = %s;\n", DecompileProfiles[f], strings + par->s_name, s+1);
							*s = ' ';
						}
						else
							QCC_CatVFile(Decompileofile, "%s;\n", DecompileProfiles[f]);
					}
				}
			}
			else if (par->type != ev_pointer)
			{
				if (strcmp(strings + par->s_name, "IMMEDIATE") && strcmp(strings + par->s_name, ".imm") && par->s_name)
				{

					if (par->type == ev_field)
					{

						ef = GetField(strings + par->s_name);

						if (!ef)
						{
							QCC_CatVFile(Decompileofile, "var .unknowntype %s;\n", strings + par->s_name);
							printf("Fatal Error: Could not locate a field named \"%s\"\n", strings + par->s_name);
						}
						else
						{
							//if (ef->type == ev_vector)
							//	j += 2;

							matchingfield = GetMatchingField(ef);

#ifndef DONT_USE_DIRTY_TRICKS	//could try scanning for an op_address+op_storep_fnc pair
							if ((ef->type == ev_function) && !strcmp(strings + ef->s_name, "th_pain"))
							{
								QCC_CatVFile(Decompileofile, ".void(entity attacker, float damage) th_pain;\n");
							}
							else
#endif
							{
								if (matchingfield)
									QCC_CatVFile(Decompileofile, "var .%s %s = %s;\n", type_name(ef), strings + ef->s_name, matchingfield);
								else
									QCC_CatVFile(Decompileofile, ".%s %s;\n", type_name(ef), strings + ef->s_name);

//								fprintf(Decompileofile, "//%i %i %i %i\n", ef->ofs, ((int*)pr_globals)[ef->ofs], par->ofs, ((int*)pr_globals)[par->ofs]);
							}
						}
					}
					else
					{

						if (par->type == ev_vector)
							j += 2;

						if (par->type == ev_entity || par->type == ev_void)
						{

							QCC_CatVFile(Decompileofile, "%s %s;\n", type_name(par), strings + par->s_name);

						}
						else
						{

							line[0] = '\0';
							QC_snprintfz(line, sizeof(line), "%s", DecompileValueString((etype_t)(par->type), &pr_globals[par->ofs]));

							if (IsConstant(par))
							{
								QCC_CatVFile(Decompileofile, "%s %s    = %s;\n", type_name(par), strings + par->s_name, line);
							}
							else
							{
								if (pr_globals[par->ofs] != 0)
									QCC_CatVFile(Decompileofile, "%s %s /* = %s */;\n", type_name(par), strings + par->s_name, line);
								else
									QCC_CatVFile(Decompileofile, "%s %s;\n", type_name(par), strings + par->s_name, line);
							}
						}
					}
				}
			}
		}
	}
}
void DecompileFunction(const char *name, int *lastglobal)
{
	int i, findex, ps;
	dstatement_t *ds, *ts, *altdone;
	dfunction_t *df;
	QCC_ddef_t *par;
	char *arg2;
	unsigned short dom, tom;
	int j, start, end;
	static char line[8192];
	dstatement_t *k;
	int dum;
	size_t startpos;



	for (i = 1; i < numfunctions; i++)
		if (!strcmp(name, strings + functions[i].s_name))
			break;
	if (i == numfunctions)
	{
		printf("Fatal Error: No function named \"%s\"\n", name);
		exit(1);
	}
	df = functions + i;
	altdone = statements + numstatements;
	for (j = i+1; j < numfunctions; j++)
	{
		if (functions[j].first_statement <= 0)
			continue;
		altdone = statements + functions[j].first_statement;
		break;
	}

	findex = i;

	start = *lastglobal;
//	if (dfpred->first_statement <= 0 && df->first_statement > 0)
//		start -= 1;
	end = df->parm_start;
	if (!end)
	{
		par = DecompileFindGlobal(name);
		if (par)
			end = par - globals;
	}
	*lastglobal = max(*lastglobal, end + df->locals);
	DecompilePreceedingGlobals(start, end, name);

	/*
	 * Check ''local globals''  
	 */

	if (df->first_statement <= 0)
	{

		QCC_CatVFile(Decompileofile, "%s", DecompileProfiles[findex]);
		QCC_CatVFile(Decompileofile, " = #%i; \n", -df->first_statement);

		return;
	}
	ds = statements + df->first_statement;

	while (1)
	{

		dom = (ds->op) % OP_MARK_END_ELSE;

		if (!dom || ds == altdone)
			break;
		else if (dom == OP_GOTO)
		{
			// check for i-t-e 
			if ((signed int)ds->a > 0)
			{
				ts = ds + (signed int)ds->a;
				ts->op += OP_MARK_END_ELSE;	// mark the end of a if/ite construct 
			}
			else
			{
				ts = ds + (signed int)ds->a;
				//if its a negative goto then it should normally be the end of a while{} loop. if we can't find the while statement itself, then its an infinite loop
				for (k = (ts); k < ds; k++)
				{
					tom = k->op % OP_MARK_END_ELSE;
					if (tom == OP_IF_I || tom == OP_IFNOT_I ||
						tom == OP_IF_F || tom == OP_IFNOT_F ||
						tom == OP_IF_S || tom == OP_IFNOT_S)
					{
						if (k + (signed int)k->b == ds+1)
							break;
					}
				}
				if (k == ds)
				{
					ds->op += OPD_GOTO_WHILE1-OP_GOTO;
					ts->op += OP_MARK_END_DO;
				}
			}
		}
		else if (dom == OP_IFNOT_I || dom == OP_IFNOT_F || dom == OP_IFNOT_S)
		{
			// check for pure if 

			ts = ds + (signed int)ds->b;
			tom = (ts - 1)->op % OP_MARK_END_ELSE;

			if (tom != OP_GOTO)
				ts->op += OP_MARK_END_ELSE;	// mark the end of a if construct 
			else if ((signed int)(ts - 1)->a < 0)
			{
				if (((signed int)(ts - 1)->a + (signed int)ds->b) > 1)
				{
					// pure if
					ts->op += OP_MARK_END_ELSE;	// mark the end of a if/ite construct 
				}
				else
				{

					dum = 1;
					for (k = (ts - 1) + (signed int)((ts - 1)->a); k < ds; k++)
					{
						tom = k->op % OP_MARK_END_ELSE;
						if (tom == OP_GOTO ||
							tom == OP_IF_I || tom == OP_IFNOT_I ||
							tom == OP_IF_F || tom == OP_IFNOT_F ||
							tom == OP_IF_S || tom == OP_IFNOT_S)
							dum = 0;
					}
					if (!dum)
					{
						// pure if  
						ts->op += OP_MARK_END_ELSE;	// mark the end of a if/ite construct 
					}
				}
			}
		}
		else if (dom == OP_IF_I || dom == OP_IF_F || dom == OP_IF_S)
		{
			if ((signed int)ds->b<1)
			{
				ts = ds + (signed int)ds->b;
				//this is some kind of loop, either a while or for.

				//if the statement before the 'do' is a forwards goto, and it jumps to within the loop (instead of after), then we have to assume that it is a for loop and not a loop inside an else block.
				if ((ts-1)->op%OP_MARK_END_ELSE == OP_GOTO && (signed int)(ts-1)->a > 0 && (ts-1)+(signed int)(ts-1)->a <= ds)
				{
					(--ts)->op += OPD_GOTO_FORSTART - OP_GOTO;
					//because it was earlier, we need to unmark that goto's target as an end_else
					ts = ts + (signed int)ts->a;
					ts->op -= OP_MARK_END_ELSE;
				}
				else
					ts->op += OP_MARK_END_DO;	// mark the start of a do construct 
			}
			else
			{
				ts = ds + ds->b;
				if ((ts-1)->op%OP_MARK_END_ELSE != OP_GOTO)
					ts->op += OP_MARK_END_ELSE;	// mark the end of an if construct 
				else if ((signed int)(ts - 1)->a < 0)
				{
					if (((signed int)(ts - 1)->a + (signed int)ds->b) > 1)
					{
						// pure if
						ts->op += OP_MARK_END_ELSE;	// mark the end of a if/ite construct 
					}
					else
					{

						dum = 1;
						for (k = (ts - 1) + (signed int)((ts - 1)->a); k < ds; k++)
						{
							tom = k->op % OP_MARK_END_ELSE;
							if (tom == OP_GOTO ||
								tom == OP_IF_I || tom == OP_IFNOT_I ||
								tom == OP_IF_F || tom == OP_IFNOT_F ||
								tom == OP_IF_S || tom == OP_IFNOT_S)
								dum = 0;
						}
						if (!dum)
						{
							// pure if  
							ts->op += OP_MARK_END_ELSE;	// mark the end of a if/ite construct 
						}
					}
				}
			}
		}
		ds++;
	}

	/*
	 * print the prototype 
	 */
	QCC_CatVFile(Decompileofile, "\n%s", DecompileProfiles[findex]);

	// handle state functions 

	ds = statements + df->first_statement;

	if (ds->op == OP_STATE)
	{

		par = DecompileGetParameter(ds->a);
		if (!par)
		{
			static QCC_ddef_t pars;
			//must be a global (gotta be a float), create the def as needed
			pars.ofs = ds->a;
			pars.s_name = "IMMEDIATE"-strings;
			pars.type = ev_float;
			par = &pars;
//			printf("Fatal Error - Can't determine frame number.");
//			exit(1);
		}

		arg2 = DecompileGet(df, ds->b, NULL);
		if (!arg2)
		{
			printf("Fatal Error - No state parameter with offset %i.", ds->b);
			exit(1);
		}

		QCC_CatVFile(Decompileofile, " = [ %s, %s ]", DecompileValueString((etype_t)(par->type), &pr_globals[par->ofs]), arg2);

		free(arg2);

	}
	else
	{
		QCC_CatVFile(Decompileofile, " =");
	}
	QCC_CatVFile(Decompileofile, "\n{\n");

	startpos = Decompileofile->size;

/*
	fprintf(Decompileprofile, "%s", DecompileProfiles[findex]);
	fprintf(Decompileprofile, ") %s;\n", name);
*/

	/*
	 * calculate the parameter size 
	 */

	for (j = 0, ps = 0; j < df->numparms; j++)
	{
		par = DecompileGetParameter((gofs_t)(df->parm_start + ps));

		if (par)
		{
			if (!strings[par->s_name])
			{
				QC_snprintfz(line, sizeof(line), "_p_%i", par->ofs);
				arg2 = malloc(strlen(line)+1);
				strcpy(arg2, line);
				par->s_name = arg2 - strings;
			}
		}

		ps += df->parm_size[j];
	}

	/*
	 * print the locals 
	 */

	if (df->locals > 0)
	{

		if ((df->parm_start) + df->locals - 1 >= (df->parm_start) + ps)
		{

			for (i = df->parm_start + ps; i < (df->parm_start) + df->locals; i++) 
			{

				par = DecompileGetParameter((gofs_t)i);

				if (!par)
				{
					// temps, or stripped...
					continue;
				}
				else
				{
					if (!strcmp(strings + par->s_name, "IMMEDIATE") || !strcmp(strings + par->s_name, ".imm"))
						continue; // immediates don't belong

					if (!strings[par->s_name])
					{
						QC_snprintfz(line, sizeof(line), "_l_%i", par->ofs);
						arg2 = malloc(strlen(line)+1);
						strcpy(arg2, line);
						par->s_name = arg2 - strings;
					}

					if (par->type == ev_function)
					{
						printf("Warning Fields and functions must be global\n");
					}
					else
					{
						if (((int*)pr_globals)[par->ofs])
							QCC_CatVFile(Decompileofile, "\tlocal %s = %s;\n", DecompilePrintParameter(par), DecompileValueString(par->type, &pr_globals[par->ofs]));
						else
							QCC_CatVFile(Decompileofile, "\tlocal %s;\n", DecompilePrintParameter(par));
					}
					if (par->type == ev_vector)
						i += 2;
				}
			}

			QCC_CatVFile(Decompileofile, "\n");

		}
	}
	/*
	 * do the hard work 
	 */

	if (!DecompileDecompileFunction(df, altdone))
	{
		QCC_InsertVFile(Decompileofile, startpos, "#error Corrupt Function: %s\n#if 0\n", strings+df->s_name);
		QCC_CatVFile(Decompileofile, "#endif\n");
	}

	QCC_CatVFile(Decompileofile, "};\n");
}

extern pbool safedecomp;
int fake_name;
char synth_name[1024]; // fake name part2

pbool TrySynthName(const char *first)
{
	int i;

	// try to figure out the filename
	// based on the first function in the file
	for (i=0; i < FILELISTSIZE; i+=2)
	{
		if (!strcmp(filenames[i], first))
		{
			QC_snprintfz(synth_name, sizeof(synth_name), filenames[i + 1]);
			return true;
		}
	}
	return false;
}

void DecompileDecompileFunctions(const char *origcopyright)
{
	int i;
	unsigned int o;
	dfunction_t *d;
	pbool bogusname;
	vfile_t *f;
	char fname[512];
	int lastglob = 1;
	QCC_ddef_t *def;

	DecompileCalcProfiles();

	AddSourceFile(NULL, "progs.src");
	Decompileprogssrc = QCC_AddVFile("progs.src", NULL, 0);
	if (!Decompileprogssrc)
	{
		printf("Fatal Error - Could not open \"progs.src\" for output.\n");
		exit(1);
	}

	QCC_CatVFile(Decompileprogssrc, "./progs.dat\n\n");

	QCC_CatVFile(Decompileprogssrc, "#pragma flag enable lax //remove this line once you've fixed up any decompiler bugs...\n");
	if (origcopyright)
		QCC_CatVFile(Decompileprogssrc, "//#pragma copyright \"%s\"\n", origcopyright);
	QCC_CatVFile(Decompileprogssrc, "\n", origcopyright);

	def = DecompileFindGlobal("end_sys_fields");
	lastglob = def?def->ofs+1:1;
	if (lastglob != 1)
	{
		QC_snprintfz(synth_name, sizeof(synth_name), "sysdefs.qc");
		QC_snprintfz(fname, sizeof(fname), synth_name);
		if (!DecompileAlreadySeen(fname, &f))
		{
			printf("decompiling %s\n", fname);
			compilecb();
			QCC_CatVFile(Decompileprogssrc, "%s\n", fname);
		}
		if (!f)
		{
			printf("Fatal Error - Could not open \"%s\" for output.\n", fname);
			exit(1);
		}
		Decompileofile = f;

		DecompilePreceedingGlobals(1, lastglob, "");
	}

	for (i = 1; i < numfunctions; i++)
	{
		d = &functions[i];

		fname[0] = '\0';
		if (d->s_file <= strofs && d->s_file >= 0)
			sprintf(fname, "%s", strings + d->s_file);
		// FrikaC -- not sure if this is cool or what?
		bogusname = false;
		if (strlen(fname) <= 0)
			bogusname = true;
		else for (o = 0; o < strlen(fname); o++)
		{
			if ((fname[o] < 'a' || fname[o] > 'z') && 
			  (fname[o] < '0' || fname[o] > '9') &&
			  (fname[o] <'A' || fname[o] > 'Z') &&
			  (fname[o] != '.' && fname[o] != '!' && fname[o] != '_'))
			{
				if (fname[o] == '/')
					fname[o] = '.';
				else if (fname[o] == '\\')
					fname[o] = '.';
				else
				{
						bogusname = true;
						break;
				}
			}
		}

		if (bogusname)
		{
			if (*fname && !DecompileAlreadySeen(fname, NULL))
			{
				synth_name[0] = 0;
			}
			if(!TrySynthName(qcva("%s", strings + d->s_name)) && !synth_name[0])
				QC_snprintfz(synth_name, sizeof(synth_name), "frik%i.qc", fake_name++);

			QC_snprintfz(fname, sizeof(fname), synth_name);
		}
		else
			synth_name[0] = 0;



		if (!DecompileAlreadySeen(fname, &f))
		{
			printf("decompiling %s\n", fname);
			compilecb();
			QCC_CatVFile(Decompileprogssrc, "%s\n", fname);
		}
		if (!f)
		{
			printf("Fatal Error - Could not open \"%s\" for output.\n", fname);
			exit(1);
		}
		Decompileofile = f;
		DecompileFunction(strings + d->s_name, &lastglob);
	}
}

void DecompileProgsDat(char *name, void *buf, size_t bufsize)
{
	char *c = ReadProgsCopyright(buf, bufsize);
	if (c)
		printf("Copyright: %s\n", c);

	PreCompile();
	pHash_Get = &Hash_Get;
	pHash_GetNext = &Hash_GetNext;
	pHash_Add = &Hash_Add;
	pHash_RemoveData = &Hash_RemoveData;
	Hash_InitTable(&typedeftable, 1024, qccHunkAlloc(Hash_BytesForBuckets(1024)));

	maxtypeinfos = 64;
	qcc_typeinfo = (void *)malloc(sizeof(QCC_type_t)*maxtypeinfos);
	numtypeinfos = 0;

	type_void = QCC_PR_NewType("void", ev_void, true);
	type_string = QCC_PR_NewType("string", ev_string, true);
	type_float = QCC_PR_NewType("float", ev_float, true);
	type_vector = QCC_PR_NewType("vector", ev_vector, true);
	type_entity = QCC_PR_NewType("entity", ev_entity, true);
	type_field = QCC_PR_NewType("__field", ev_field, false);
	type_function = QCC_PR_NewType("__function", ev_function, false);
	type_function->aux_type = type_void;
	type_pointer = QCC_PR_NewType("__pointer", ev_pointer, false);
	type_integer = QCC_PR_NewType("__integer", ev_integer, true);
	type_variant = QCC_PR_NewType("variant", ev_variant, true);
	type_variant = QCC_PR_NewType("__variant", ev_variant, true);

	DecompileReadData(name, buf, bufsize);
	DecompileDecompileFunctions(c);

	printf("Done.\n");
}

char *DecompileGlobalStringNoContents(gofs_t ofs)
{
	int i;
	QCC_ddef_t *def;
	static char line[128];

	line[0] = '0';
	QC_snprintfz(line, sizeof(line), "%i(??""?)", ofs);

	for (i = 0; i < numglobaldefs; i++)
	{
		def = &globals[i];

		if (def->ofs == ofs)
		{
			line[0] = '0';
			QC_snprintfz(line, sizeof(line), "%i(%s)", def->ofs, strings + def->s_name);
			break;
		}
	}

	i = strlen(line);
	for (; i < 16; i++)
		strcat(line, " ");
	strcat(line, " ");

	return line;
}

char *DecompileGlobalString(gofs_t ofs)
{
	char *s;
	int i;
	QCC_ddef_t *def;
	static char line[128];

	line[0] = '0';
	QC_snprintfz(line, sizeof(line), "%i(??""?)", ofs);

	for (i = 0; i < numglobaldefs; i++)
	{
		def = &globals[i];

		if (def->ofs == ofs)
		{

			line[0] = '0';
			if (!strcmp(strings + def->s_name, "IMMEDIATE") || !strcmp(strings + def->s_name, ".imm"))
			{
				s = PR_ValueString((etype_t)(def->type), &pr_globals[ofs]);
				QC_snprintfz(line, sizeof(line), "%i(%s)", def->ofs, s);
			}
			else
				QC_snprintfz(line, sizeof(line), "%i(%s)", def->ofs, strings + def->s_name);
		}
	}

	i = strlen(line);
	for (; i < 16; i++)
		strcat(line, " ");
	strcat(line, " ");

	return line;
}

void DecompilePrintStatement(dstatement_t * s)
{
	int i;

	printf("%4i : %s ", (int)(s - statements), pr_opcodes[s->op].opname);
	i = strlen(pr_opcodes[s->op].opname);
	for (; i < 10; i++)
		printf(" ");

	if (s->op == OP_IF_I || s->op == OP_IFNOT_I ||
		s->op == OP_IF_F || s->op == OP_IFNOT_F ||
		s->op == OP_IF_S || s->op == OP_IFNOT_S)
		printf("%sbranch %i", DecompileGlobalString(s->a), s->b);
	else if (s->op == OP_GOTO)
	{
		printf("branch %i", s->a);
	}
	else if ((unsigned)(s->op - OP_STORE_F) < 6)
	{
		printf("%s", DecompileGlobalString(s->a));
		printf("%s", DecompileGlobalStringNoContents(s->b));
	}
	else
	{
		if (s->a)
			printf("%s", DecompileGlobalString(s->a));
		if (s->b)
			printf("%s", DecompileGlobalString(s->b));
		if (s->c)
			printf("%s", DecompileGlobalStringNoContents(s->c));
	}
	printf("\n");
}

void DecompilePrintFunction(char *name)
{
	int i;
	dstatement_t *ds;
	dfunction_t *df;

	for (i = 0; i < numfunctions; i++)
		if (!strcmp(name, strings + functions[i].s_name))
			break;
	if (i == numfunctions)
	{
		printf("Fatal Error: No function names \"%s\"\n", name);
		exit(1);
	}
	df = functions + i;

	printf("Statements for %s:\n", name);
	ds = statements + df->first_statement;
	while (1)
	{
		DecompilePrintStatement(ds);

		if (!ds->op)
			break;
		ds++;
	}
}


