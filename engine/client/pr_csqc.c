#include "quakedef.h"

#ifdef CSQC_DAT

typedef struct menuedict_s
{
	qboolean	isfree;
	float		freetime; // sv.time when the object was freed
	int			entnum;
	qboolean	readonly;	//world
} csqcedict_t;

#define	RETURN_SSTRING(s) (*(char **)&((int *)pr_globals)[OFS_RETURN] = PR_SetString(prinst, s))	//static - exe will not change it.
char *PF_TempStr(void);

int csqcentsize;

//pr_cmds.c builtins that need to be moved to a common.
void VARGS PR_BIError(progfuncs_t *progfuncs, char *format, ...);
void PF_cvar_string (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_cvar_set (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_dprint (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_error (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_rint (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_floor (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_ceil (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_Tokenize  (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_ArgV  (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_FindString (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_FindFloat (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_nextent (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_randomvec (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_Sin (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_Cos (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_Sqrt (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_bound (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_strlen(progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_strcat (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_ftos (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_fabs (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_vtos (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_etos (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_stof (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_mod (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_substring (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_stov (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_dupstring(progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_forgetstring(progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_Spawn (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_min (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_max (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_registercvar (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_pow (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_chr2str (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_localcmd (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_random (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_fopen (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_fclose (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_fputs (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_fgets (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_normalize (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_vlen (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_vectoyaw (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_vectoangles (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_findchain (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_findchainfloat (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_coredump (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_traceon (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_traceoff (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_eprint (progfuncs_t *prinst, struct globalvars_s *pr_globals);

void PF_fclose_progs (progfuncs_t *prinst);
char *PF_VarString (progfuncs_t *prinst, int	first, struct globalvars_s *pr_globals);

static void PF_cvar (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	cvar_t	*var;
	char	*str;

	str = PR_GetStringOfs(prinst, OFS_PARM0);
	{
		var = Cvar_Get(str, "", 0, "csqc cvars");
		if (var)
			G_FLOAT(OFS_RETURN) = var->value;
		else
			G_FLOAT(OFS_RETURN) = 0;
	}
}

//too specific to the prinst's builtins.
static void PF_Fixme (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Con_Printf("\n");

	prinst->PR_RunError(prinst, "\nBuiltin %i not implemented.\nMenu is not compatable.", prinst->lastcalledbuiltinnumber);
	PR_BIError (prinst, "bulitin not implemented");
}



static void PF_makevectors (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
//	AngleVectors (G_VECTOR(OFS_PARM0), CSQC_VEC(v_forward), CSQC_VEC(v_right), CSQC_VEC(v_up));
}



//warning: functions that depend on globals are bad, mkay?
builtin_t csqc_builtins[] = {
//0
	PF_Fixme,
	PF_makevectors,
	PF_Fixme, //PF_setorigin
	PF_Fixme, //PF_setmodel
	PF_Fixme, //PF_setsize
	PF_Fixme,
	PF_Fixme, //PF_break,
	PF_random,
	PF_Fixme, //PF_sound,
	PF_normalize,
//10
	PF_error,
	PF_Fixme, //PF_objerror,
	PF_vlen,
	PF_vectoyaw,
	PF_Spawn,
	PF_Fixme, //PF_Remove,
	PF_Fixme, //PF_traceline,
	PF_Fixme, //PF_checkclient,
	PF_FindString,
	PF_Fixme, //PF_precache_sound,
//20
	PF_Fixme, //PF_precache_model,
	PF_Fixme, //PF_stuffcmd,
	PF_Fixme, //PF_findradius,
	PF_Fixme, //PF_bprint,
	PF_Fixme, //PF_sprint,
	PF_dprint,
	PF_ftos,
	PF_vtos,
	PF_coredump,
	PF_traceon,
//30
	PF_traceoff,
	PF_eprint,
	PF_Fixme, //PF_walkmove,
	PF_Fixme,
PF_Fixme, //PF_droptofloor,
PF_Fixme, //PF_lightstyle,
PF_rint,
PF_floor,
PF_ceil,
PF_Fixme,
//40
PF_Fixme, //PF_checkbottom,
PF_Fixme, //PF_pointcontents,
PF_Fixme,
PF_fabs,
PF_Fixme, //PF_aim,	hehehe...
PF_cvar,
PF_localcmd,
PF_nextent,
PF_Fixme, //PF_particle,
PF_Fixme, //PF_changeyaw,
//50
PF_Fixme,
PF_vectoangles,

PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
//60
PF_Fixme,

PF_Sin,
PF_Cos,
PF_Sqrt,

PF_Fixme,

PF_Fixme,
PF_Fixme,
SV_MoveToGoal,
PF_Fixme, //PF_precache_file,
PF_Fixme, //PF_makestatic,
//70
PF_Fixme, //PF_changelevel,
PF_Fixme,
PF_cvar_set,
PF_Fixme, //PF_centerprint,
PF_Fixme, //PF_ambientsound,

PF_Fixme, //PF_precache_model,
PF_Fixme, //PF_precache_sound,
PF_Fixme, //PF_precache_file,
PF_Fixme, //PF_setspawnparms,
PF_Fixme, //PF_logfrag,
//80
PF_Fixme, //PF_infokey,
PF_stof,
PF_Fixme, //PF_multicast,
PF_Fixme,
PF_Fixme,

PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
//90
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,

PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
//100
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,

PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
//110
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,

PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,



PF_Fixme};
int csqc_numbuiltins = sizeof(csqc_builtins)/sizeof(csqc_builtins[0]);





jmp_buf csqc_abort;
int incsqcprogs;
progfuncs_t *csqcprogs;
progparms_t csqcprogparms;
csqcedict_t *csqc_edicts;
int num_csqc_edicts;

func_t csqc_init_function;
func_t csqc_shutdown_function;
func_t csqc_draw_function;
func_t csqc_keydown_function;
func_t csqc_keyup_function;
func_t csqc_toggle_function;

float *csqc_time;




int COM_FileSize(char *path);
pbool QC_WriteFile(char *name, void *data, int len);
void *VARGS PR_Malloc(int size);	//these functions should be tracked by the library reliably, so there should be no need to track them ourselves.
void VARGS PR_Free(void *mem);

//Any menu builtin error or anything like that will come here.
void VARGS CSQC_Abort (char *format, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr, format);
	_vsnprintf (string,sizeof(string)-1, format,argptr);
	va_end (argptr);

	Con_Printf("CSQC_Abort: %s\nShutting down csqc\n", string);


{
	static char buffer[1024*1024*8];
	int size = sizeof buffer;
	csqcprogs->save_ents(csqcprogs, buffer, &size, 3);
	COM_WriteFile("csqccore.txt", buffer, size);
}

	Host_EndGame("csqc error");
}

double  csqctime;
void CSQC_Init (void)
{
	if (!qrenderer)
	{
		return;
	}


	csqcprogparms.progsversion = PROGSTRUCT_VERSION;
	csqcprogparms.ReadFile = COM_LoadStackFile;//char *(*ReadFile) (char *fname, void *buffer, int *len);
	csqcprogparms.FileSize = COM_FileSize;//int (*FileSize) (char *fname);	//-1 if file does not exist
	csqcprogparms.WriteFile = QC_WriteFile;//bool (*WriteFile) (char *name, void *data, int len);
	csqcprogparms.printf = (void *)Con_Printf;//Con_Printf;//void (*printf) (char *, ...);
	csqcprogparms.Sys_Error = Sys_Error;
	csqcprogparms.Abort = CSQC_Abort;
	csqcprogparms.edictsize = sizeof(csqcedict_t);

	csqcprogparms.entspawn = NULL;//void (*entspawn) (struct edict_s *ent);	//ent has been spawned, but may not have all the extra variables (that may need to be set) set
	csqcprogparms.entcanfree = NULL;//bool (*entcanfree) (struct edict_s *ent);	//return true to stop ent from being freed
	csqcprogparms.stateop = NULL;//StateOp;//void (*stateop) (float var, func_t func);
	csqcprogparms.cstateop = NULL;//CStateOp;
	csqcprogparms.cwstateop = NULL;//CWStateOp;
	csqcprogparms.thinktimeop = NULL;//ThinkTimeOp;

	//used when loading a game
	csqcprogparms.builtinsfor = NULL;//builtin_t *(*builtinsfor) (int num);	//must return a pointer to the builtins that were used before the state was saved.
	csqcprogparms.loadcompleate = NULL;//void (*loadcompleate) (int edictsize);	//notification to reset any pointers.

	csqcprogparms.memalloc = PR_Malloc;//void *(*memalloc) (int size);	//small string allocation	malloced and freed randomly
	csqcprogparms.memfree = PR_Free;//void (*memfree) (void * mem);


	csqcprogparms.globalbuiltins = csqc_builtins;//builtin_t *globalbuiltins;	//these are available to all progs
	csqcprogparms.numglobalbuiltins = csqc_numbuiltins;

	csqcprogparms.autocompile = PR_NOCOMPILE;//enum {PR_NOCOMPILE, PR_COMPILENEXIST, PR_COMPILECHANGED, PR_COMPILEALWAYS} autocompile;

	csqcprogparms.gametime = &csqctime;

	csqcprogparms.sv_edicts = (edict_t **)&csqc_edicts;
	csqcprogparms.sv_num_edicts = &num_csqc_edicts;

	csqcprogparms.useeditor = NULL;//sorry... QCEditor;//void (*useeditor) (char *filename, int line, int nump, char **parms);

	csqctime = Sys_DoubleTime();
	if (!csqcprogs)
	{
		csqcprogs = InitProgs(&csqcprogparms);
		PR_Configure(csqcprogs, NULL, -1, 1);
		if (PR_LoadProgs(csqcprogs, "qwprogs.dat", 54730, NULL, 0) < 0) //no per-progs builtins.
		{
			//failed to load or something
			M_Init_Internal();
			return;
		}
		if (setjmp(csqc_abort))
		{
			M_Init_Internal();
			return;
		}
		incsqcprogs++;

		csqc_time = (float*)PR_FindGlobal(csqcprogs, "time", 0);
		if (csqc_time)
			*csqc_time = Sys_DoubleTime();

		csqcentsize = PR_InitEnts(csqcprogs, 3072);


		//'world' edict
		EDICT_NUM(csqcprogs, 0)->readonly = true;
		EDICT_NUM(csqcprogs, 0)->isfree = false;


		csqc_init_function		= PR_FindFunction(csqcprogs, "csqc_init",		PR_ANY);
		csqc_shutdown_function	= PR_FindFunction(csqcprogs, "csqc_shutdown",	PR_ANY);
		csqc_draw_function		= PR_FindFunction(csqcprogs, "csqc_draw",		PR_ANY);
		csqc_keydown_function	= PR_FindFunction(csqcprogs, "csqc_keydown",	PR_ANY);
		csqc_keyup_function		= PR_FindFunction(csqcprogs, "csqc_keyup",		PR_ANY);
		csqc_toggle_function	= PR_FindFunction(csqcprogs, "csqc_toggle",		PR_ANY);

		if (csqc_init_function)
			PR_ExecuteProgram(csqcprogs, csqc_init_function);
		incsqcprogs--;
	}
}

qboolean CSQC_DrawView(void)
{
	if (!csqcprogs || !csqc_draw_function)
		return false;

	incsqcprogs++;

	PR_ExecuteProgram(csqcprogs, csqc_draw_function);

	incsqcprogs--;
	return true;
}

#endif
