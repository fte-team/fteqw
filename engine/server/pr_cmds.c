/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

//okay, so these are a quick but easy hack
struct progfuncs_s;
struct edict_s;
int QC_RegisterFieldVar(struct progfuncs_s *progfuncs, unsigned int type, char *name, int requestedpos, int origionalofs);
void ED_Print (struct progfuncs_s *progfuncs, struct edict_s *ed);
int PR_EnableEBFSBuiltin(char *name);
void PR_CleanLogText_Init (void);

#include "qwsvdef.h"

cvar_t	nomonsters = {"nomonsters", "0"};
cvar_t	gamecfg = {"gamecfg", "0"};
cvar_t	scratch1 = {"scratch1", "0"};
cvar_t	scratch2 = {"scratch2", "0"};
cvar_t	scratch3 = {"scratch3", "0"};
cvar_t	scratch4 = {"scratch4", "0"};
cvar_t	savedgamecfg = {"savedgamecfg", "0", NULL, CVAR_ARCHIVE};
cvar_t	saved1 = {"saved1", "0", NULL, CVAR_ARCHIVE};
cvar_t	saved2 = {"saved2", "0", NULL, CVAR_ARCHIVE};
cvar_t	saved3 = {"saved3", "0", NULL, CVAR_ARCHIVE};
cvar_t	saved4 = {"saved4", "0", NULL, CVAR_ARCHIVE};
cvar_t	temp1 = {"temp1", "0", NULL, CVAR_ARCHIVE};
cvar_t	noexit = {"noexit", "0", NULL};

cvar_t	pr_maxedicts = {"pr_maxedicts", "512", NULL, CVAR_LATCH};
cvar_t	pr_imitatemvdsv = {"pr_imitatemvdsv", "0", NULL, CVAR_LATCH};
cvar_t	pr_fixbrokenqccarrays = {"pr_fixbrokenqccarrays", "0", NULL, CVAR_LATCH};

cvar_t	progs = {"progs", "", NULL, CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_NOTFROMSERVER};
cvar_t	qc_nonetaccess = {"qc_nonetaccess", "0"};	//prevent write_... builtins from doing anything. This means we can run any mod, specific to any engine, on the condition that it also has a qw or nq crc.

cvar_t pr_overridebuiltins = {"pr_overridebuiltins", "1"};

cvar_t pr_compatabilitytest = {"pr_compatabilitytest", "0", NULL, CVAR_LATCH};

cvar_t sv_addon[MAXADDONS];
char cvargroup_progs[] = "Progs variables";

int pr_teamfield;

void PR_ClearThreads(void);


typedef struct {
	//for func finding and swapping.
	char *name;
	//c function to call
	builtin_t bifunc;

	//most of the next two are the same, but they follow different family trees.
	//It depends on the progs type being loaded as to which is used.
	int nqnum;		//standard nq.
	int qwnum;		//standard qw.
	int h2num;		//standard hexen2
	int ebfsnum;	//extra functions, these exist ONLY after being checked for.
} BuiltinList_t;
builtin_t pr_builtin[500];
extern BuiltinList_t BuiltinList[];

qboolean pr_udc_exteffect_enabled;



func_t SpectatorConnect;
func_t SpectatorThink;
func_t SpectatorDisconnect;

func_t ChatMessage;

func_t getplayerstat[MAX_CL_STATS];
func_t getplayerstati[MAX_CL_STATS];

//mvdsv stuff
func_t mod_UserCmd, SV_ParseClientCommand;
func_t mod_ConsoleCmd;
func_t UserInfo_Changed;
func_t localinfoChanged;

func_t SV_PlayerPhysicsQC;	//DP's DP_SV_PLAYERPHYSICS extension
func_t EndFrameQC;

qboolean pr_items2;

nqglobalvars_t realpr_nqglobal_struct;
nqglobalvars_t *pr_nqglobal_struct = &realpr_nqglobal_struct;

progfuncs_t *svprogfuncs;
progparms_t svprogparms;

progstype_t progstype;

void PR_RegisterSVBuiltins(void);
void PR_RegisterFields(void);
void PR_ResetBuiltins(progstype_t type);

char *QC_ProgsNameForEnt(edict_t *ent)
{
	return "?";
}


int pr_edict_size;

int COM_FileSize(char *path);

pbool QC_WriteFile(char *name, void *data, int len)
{
	char buffer[256];
	sprintf(buffer, "%s", name);
	COM_WriteFile(buffer, data, len);
	return true;
}

void ED_Spawned (struct edict_s *ent)
{
	ent->v.dimension_see = 255;
	ent->v.dimension_seen = 255;
	ent->v.dimension_ghost = 0;
	ent->v.dimension_solid = 255;
	ent->v.dimension_hit = 255;
}

pbool ED_CanFree (edict_t *ed)
{
	if (ed == sv.edicts)
	{
		Con_TPrintf(STL_CANTFREEWORLD);
		pr_trace = 1;
		return false;
	}
	if (NUM_FOR_EDICT(svprogfuncs, ed) <= sv.allocated_client_slots)
	{
		Con_TPrintf(STL_CANTFREEPLAYERS);
		pr_trace = 1;
		return false;
	}
	SV_UnlinkEdict (ed);		// unlink from world bsp

	ed->v.model = 0;
	ed->v.takedamage = 0;
	ed->v.modelindex = 0;
	ed->v.colormap = 0;
	ed->v.skin = 0;
	ed->v.frame = 0;
	VectorCopy (vec3_origin, ed->v.origin);
	VectorCopy (vec3_origin, ed->v.angles);
	ed->v.nextthink = 0;
	ed->v.think = 0;
	ed->v.solid = 0;

	return true;
}

void StateOp (progfuncs_t *prinst, float var, func_t func)
{	
	entvars_t *vars = &PROG_TO_EDICT(prinst, pr_global_struct->self)->v;
	if (progstype == PROG_H2)
		vars->nextthink = pr_global_struct->time+0.05;
	else
		vars->nextthink = pr_global_struct->time+0.1;
	vars->think = func;
	vars->frame = var;
}
void CStateOp (progfuncs_t *prinst, float startFrame, float endFrame, func_t currentfunc)
{	
	entvars_t *vars = &PROG_TO_EDICT(prinst, pr_global_struct->self)->v;

	vars->nextthink = pr_global_struct->time+0.05;
	vars->think = currentfunc;
	pr_global_struct->cycle_wrapped = false;

	if(startFrame <= endFrame)
	{ // Increment
		if(vars->frame < startFrame || vars->frame > endFrame)
		{
			vars->frame = startFrame;
			return;
		}
		vars->frame++;
		if(vars->frame > endFrame)
		{
			pr_global_struct->cycle_wrapped = true;
			vars->frame = startFrame;
		}
		return;
	}
	// Decrement
	if(vars->frame > startFrame || vars->frame < endFrame)
	{
		vars->frame = startFrame;
		return;
	}
	vars->frame--;
	if(vars->frame < endFrame)
	{
		pr_global_struct->cycle_wrapped = true;
		vars->frame = startFrame;
	}
}
void CWStateOp (progfuncs_t *prinst, float startFrame, float endFrame, func_t currentfunc)
{	
	entvars_t *vars = &PROG_TO_EDICT(prinst, pr_global_struct->self)->v;

	vars->nextthink = pr_global_struct->time+0.05;
	vars->think = currentfunc;
	pr_global_struct->cycle_wrapped = false;

	if(startFrame <= endFrame)
	{ // Increment
		if(vars->weaponframe < startFrame || vars->weaponframe > endFrame)
		{
			vars->weaponframe = startFrame;
			return;
		}
		vars->weaponframe++;
		if(vars->weaponframe > endFrame)
		{
			pr_global_struct->cycle_wrapped = true;
			vars->weaponframe = startFrame;
		}
		return;
	}
	// Decrement
	if(vars->weaponframe > startFrame || vars->weaponframe < endFrame)
	{
		vars->weaponframe = startFrame;
		return;
	}
	vars->weaponframe--;
	if(vars->weaponframe < endFrame)
	{
		pr_global_struct->cycle_wrapped = true;
		vars->weaponframe = startFrame;
	}
}

void ThinkTimeOp (progfuncs_t *prinst, edict_t *ed, float var)
{	
	entvars_t *vars = &ed->v;
#ifdef PARANOID
	NUM_FOR_EDICT(ed); // Make sure it's in range
#endif
	vars->nextthink = pr_global_struct->time+var;
}

//int QCEditor (char *filename, int line, int nump, char **parms);
void QC_Clear(void);
builtin_t pr_builtin[];
extern int pr_numbuiltins;

//a little loop so we can keep track of used mem
void *VARGS PR_Malloc(int size)
{
	return BZ_Malloc(size);//Z_TagMalloc (size, 100);
}
void VARGS PR_Free(void *mem)
{
	BZ_Free(mem);
}
void PF_break (progfuncs_t *prinst, struct globalvars_s *pr_globals);
int QCLibEditor(char *filename, int line, int nump, char **parms);
int QCEditor (char *filename, int line, int nump, char **parms)
{
	int i;
	char buffer[8192];
	char *r;
	FILE *f;
#ifdef TEXTEDITOR
	return QCLibEditor(filename, line, nump, parms);
#endif
	if (line == -1)
		return -1;
	COM_FOpenFile(filename, &f);
	if (!f)
		Con_Printf("%s - %i\n", filename, line);
	else
	{
		for (i = 0; i < line; i++)
		{
			fgets(buffer, sizeof(buffer), f);
		}
		if ((r = strchr(buffer, '\r')))
		{ r[0] = '\n';r[1]='\0';}
		Con_Printf("%s", buffer);
		fclose(f);
	}
//PF_break(NULL);
	return line;
}

void Q_SetProgsParms(qboolean forcompiler)
{
	progstype = PROG_NONE;
	svprogparms.progsversion = PROGSTRUCT_VERSION;
	svprogparms.ReadFile = COM_LoadStackFile;//char *(*ReadFile) (char *fname, void *buffer, int *len);
	svprogparms.FileSize = COM_FileSize;//int (*FileSize) (char *fname);	//-1 if file does not exist
	svprogparms.WriteFile = QC_WriteFile;//bool (*WriteFile) (char *name, void *data, int len);
	svprogparms.printf = (void *)Con_Printf;//Con_Printf;//void (*printf) (char *, ...);
	svprogparms.Sys_Error = Sys_Error;
	svprogparms.Abort = SV_Error;
	svprogparms.edictsize = sizeof(edict_t)-sizeof(entvars_t);

	svprogparms.entspawn = ED_Spawned;//void (*entspawn) (struct edict_s *ent);	//ent has been spawned, but may not have all the extra variables (that may need to be set) set
	svprogparms.entcanfree = ED_CanFree;//bool (*entcanfree) (struct edict_s *ent);	//return true to stop ent from being freed
	svprogparms.stateop = StateOp;//void (*stateop) (float var, func_t func);
	svprogparms.cstateop = CStateOp;
	svprogparms.cwstateop = CWStateOp;
	svprogparms.thinktimeop = ThinkTimeOp;

	//used when loading a game
	svprogparms.builtinsfor = NULL;//builtin_t *(*builtinsfor) (int num);	//must return a pointer to the builtins that were used before the state was saved.
	svprogparms.loadcompleate = NULL;//void (*loadcompleate) (int edictsize);	//notification to reset any pointers.

	svprogparms.memalloc = PR_Malloc;//void *(*memalloc) (int size);	//small string allocation	malloced and freed randomly
	svprogparms.memfree = PR_Free;//void (*memfree) (void * mem);


	svprogparms.globalbuiltins = pr_builtin;//builtin_t *globalbuiltins;	//these are available to all progs
	svprogparms.numglobalbuiltins = pr_numbuiltins;

	svprogparms.autocompile = PR_COMPILECHANGED;//enum {PR_NOCOMPILE, PR_COMPILENEXIST, PR_COMPILECHANGED, PR_COMPILEALWAYS} autocompile;

	svprogparms.gametime = &sv.time;

	svprogparms.sv_edicts = &sv.edicts;
	svprogparms.sv_num_edicts = &sv.num_edicts;

	svprogparms.useeditor = QCEditor;//void (*useeditor) (char *filename, int line, int nump, char **parms);

	if (!svprogfuncs)
	{
		svprogfuncs = InitProgs(&svprogparms);
		PR_RegisterSVBuiltins();
	}
	PR_ClearThreads();

//	svs.numprogs = 0;

}

void PR_Deinit(void)
{
	PR_ClearThreads();
	if (svprogfuncs)
		CloseProgs(svprogfuncs);
	svprogfuncs=NULL;
}


#define NQ_PROGHEADER_CRC 5927
#define H2_PROGHEADER_CRC 38488
#define H2MP_PROGHEADER_CRC 26905

void PR_LoadGlabalStruct(void)
{
	static float dimension_send_default;
	int i;
	int *v;
	nqglobalvars_t *pr_globals = pr_nqglobal_struct;
#define globalfloat(need,name) ((nqglobalvars_t*)pr_nqglobal_struct)->name = (float *)PR_FindGlobal(svprogfuncs, #name, 0);	if (need && !((nqglobalvars_t*)pr_globals)->name) Sys_Error("Could not find export in progs \"%s\"\n", #name);
#define globalint(need,name) ((nqglobalvars_t*)pr_globals)->name = (int *)PR_FindGlobal(svprogfuncs, #name, 0);	if (need && !((nqglobalvars_t*)pr_globals)->name) Sys_Error("Could not find export in progs \"%s\"\n", #name "\n");
#define globalstring(need,name) ((nqglobalvars_t*)pr_globals)->name = (char **)PR_FindGlobal(svprogfuncs, #name, 0);	if (need && !((nqglobalvars_t*)pr_globals)->name) Sys_Error("Could not find export in progs \"%s\"\n", #name);
#define globalvec(need,name) ((nqglobalvars_t*)pr_globals)->V_##name = (vec3_t *)PR_FindGlobal(svprogfuncs, #name, 0);	if (need && !((nqglobalvars_t*)pr_globals)->V_##name) Sys_Error("Could not find export in progs \"%s\"\n", #name);
#define globalfunc(need,name) ((nqglobalvars_t*)pr_globals)->name = (func_t *)PR_FindGlobal(svprogfuncs, #name, 0);	if (need && !((nqglobalvars_t*)pr_globals)->name) Sys_Error("Could not find export in progs \"%s\"\n", #name);
//			globalint(pad);
	globalint		(true, self);	//we need the qw ones, but any in standard quake and not quakeworld, we don't really care about.
	globalint		(true, other);
	globalint		(true, world);
	globalfloat		(true, time);
	globalfloat		(true, frametime);
	globalint		(false, newmis);	//not always in nq.
	globalfloat		(true, force_retouch);
	globalstring	(true, mapname);
	globalfloat		(false, deathmatch);
	globalfloat		(false, coop);
	globalfloat		(false, teamplay);
	globalfloat		(true, serverflags);
	globalfloat		(true, total_secrets);
	globalfloat		(true, total_monsters);
	globalfloat		(true, found_secrets);
	globalfloat		(true, killed_monsters);
	globalfloat		(true, parm1);
	globalfloat		(true, parm2);
	globalfloat		(true, parm3);
	globalfloat		(true, parm4);
	globalfloat		(true, parm5);
	globalfloat		(true, parm6);
	globalfloat		(true, parm7);
	globalfloat		(true, parm8);
	globalfloat		(true, parm9);
	globalfloat		(true, parm10);
	globalfloat		(true, parm11);
	globalfloat		(true, parm12);
	globalfloat		(true, parm13);
	globalfloat		(true, parm14);
	globalfloat		(true, parm15);
	globalfloat		(true, parm16);
	globalvec		(true, v_forward);
	globalvec		(true, v_up);
	globalvec		(true, v_right);
	globalfloat		(true, trace_allsolid);
	globalfloat		(true, trace_startsolid);
	globalfloat		(true, trace_fraction);
	globalvec		(true, trace_endpos);
	globalvec		(true, trace_plane_normal);
	globalfloat		(true, trace_plane_dist);
	globalint		(true, trace_ent);
	globalfloat		(true, trace_inopen);
	globalfloat		(true, trace_inwater);
	globalint		(true, msg_entity);
	globalfunc		(false, main);
	globalfunc		(true, StartFrame);
	globalfunc		(true, PlayerPreThink);
	globalfunc		(true, PlayerPostThink);
	globalfunc		(true, ClientKill);
	globalfunc		(true, ClientConnect);
	globalfunc		(true, PutClientInServer);
	globalfunc		(true, ClientDisconnect);
	globalfunc		(false, SetNewParms);
	globalfunc		(false, SetChangeParms);
	globalfloat		(false, cycle_wrapped);

	globalfloat		(false, dimension_send);

	if (!((nqglobalvars_t*)pr_globals)->dimension_send)
	{
		((nqglobalvars_t*)pr_globals)->dimension_send = &dimension_send_default;
	}

	pr_global_struct->dimension_send = 255;

	pr_teamfield = 0;
	
	SpectatorConnect = PR_FindFunction(svprogfuncs, "SpectatorConnect", PR_ANY);
	SpectatorDisconnect = PR_FindFunction(svprogfuncs, "SpectatorDisconnect", PR_ANY);
	SpectatorThink = PR_FindFunction(svprogfuncs, "SpectatorThink", PR_ANY);
	for (i = 0; i < MAX_CL_STATS; i++)
	{
		getplayerstat[i] = PR_FindFunction(svprogfuncs, va("SetPlayerStat%i", i), PR_ANY);
		getplayerstati[i] = PR_FindFunction(svprogfuncs, va("SetPlayerStat%ii", i), PR_ANY);
	}

	SV_ParseClientCommand = PR_FindFunction(svprogfuncs, "SV_ParseClientCommand", PR_ANY);
	if (pr_imitatemvdsv.value >= 0)
	{
		mod_UserCmd = PR_FindFunction(svprogfuncs, "UserCmd", PR_ANY);
		mod_ConsoleCmd = PR_FindFunction(svprogfuncs, "ConsoleCmd", PR_ANY);
		UserInfo_Changed = PR_FindFunction(svprogfuncs, "UserInfo_Changed", PR_ANY);
		localinfoChanged = PR_FindFunction(svprogfuncs, "localinfoChanged", PR_ANY);
		ChatMessage = PR_FindFunction(svprogfuncs, "ChatMessage", PR_ANY);
	}
	else
	{
		mod_UserCmd			= 0;
		mod_ConsoleCmd		= 0;
		UserInfo_Changed	= 0;
		localinfoChanged	= 0;
		ChatMessage			= 0;
	}

	SV_PlayerPhysicsQC = PR_FindFunction(svprogfuncs, "SV_PlayerPhysics", PR_ANY);
	EndFrameQC = PR_FindFunction (svprogfuncs, "EndFrame", PR_ANY);

	v = (int *)PR_globals(svprogfuncs, PR_CURRENT);
	QC_AddSharedVar(svprogfuncs, (int *)((nqglobalvars_t*)pr_nqglobal_struct)->self-v, 1);
	QC_AddSharedVar(svprogfuncs, (int *)((nqglobalvars_t*)pr_nqglobal_struct)->other-v, 1);
	QC_AddSharedVar(svprogfuncs, (int *)((nqglobalvars_t*)pr_nqglobal_struct)->time-v, 1);

	pr_items2 = !!PR_FindGlobal(svprogfuncs, "items2", 0);

	PR_CleanLogText_Init();
}

progsnum_t AddProgs(char *name)
{
	float fl;
	func_t f;
	globalvars_t *pr_globals;
	progsnum_t num;
	int i;
	for (i = 0; i < svs.numprogs; i++)
	{
		if (!strcmp(svs.progsnames[i], name))
		{
			return svs.progsnum[i];
		}
	}

	if (svs.numprogs >= MAX_PROGS)
		return -1;

	svprogparms.autocompile = PR_COMPILECHANGED;

	if (progstype == PROG_QW)		
		num = PR_LoadProgs (svprogfuncs, name, PROGHEADER_CRC, NULL, 0);
	else if (progstype == PROG_NQ)
		num = PR_LoadProgs (svprogfuncs, name, NQ_PROGHEADER_CRC, NULL, 0);
	else if (progstype == PROG_UNKNOWN)
		num = PR_LoadProgs (svprogfuncs, name, 0, NULL, 0);
	else //if (progstype == PROG_NONE)
	{
		progstype = PROG_QW;
		num = PR_LoadProgs (svprogfuncs, name, PROGHEADER_CRC, NULL, 0);
		if (num == -1)
		{
			svprogparms.autocompile = PR_NOCOMPILE;
			progstype = PROG_NQ;
			num = PR_LoadProgs (svprogfuncs, name, NQ_PROGHEADER_CRC, NULL, 0);
			if (num == -1)
			{
				progstype = PROG_H2;
				num = PR_LoadProgs (svprogfuncs, name, H2_PROGHEADER_CRC, NULL, 0);
				if (num == -1)	//don't commit if bad.
					num = PR_LoadProgs (svprogfuncs, name, H2MP_PROGHEADER_CRC, NULL, 0);
				if (num == -1)	//don't commit if bad.
				{
					progstype = PROG_UNKNOWN;
					num = PR_LoadProgs (svprogfuncs, name, 0, NULL, 0);
					if (num == -1)	//don't commit if bad.
					{
						progstype = PROG_NONE;
					}
					else
						Cvar_Set(&qc_nonetaccess, "1");	//just in case
				}
			}
		}

		if (num != -1)
		{
			PR_LoadGlabalStruct();
			switch(progstype)
			{
			case PROG_QW:
				Con_Printf("Using QW progs\n");
				break;
			case PROG_NQ:
				Con_Printf("Using NQ progs\n");
				break;
			case PROG_H2:
				Con_Printf("Using H2 progs\n");
				break;
			default:
				Con_Printf("Using unknown progs, assuming NQ\n");
				break;
			}
		}
	}
	svprogparms.autocompile = PR_COMPILECHANGED;
	if (num == -1)
	{
		Con_Printf("Failed to load %s\n", name);
		return -1;
	}

	Con_Printf("Loaded %s\n", name);

	if (!svs.numprogs)
		PR_ResetBuiltins(progstype);

	if ((f = PR_FindFunction (svprogfuncs, "FTE_init", num )))
	{
		pr_globals = PR_globals(svprogfuncs, num);
		G_FLOAT(OFS_PARM0) = VERSION;
		PR_ExecuteProgram (svprogfuncs, f);

		fl = G_FLOAT(OFS_RETURN);
		if (fl < 0)
			SV_Error ("PR_LoadProgs: %s is not compatable with EXE version", name);
		else if ((int) (fl*1000) != (int) (VERSION*1000))
			Con_DPrintf("Warning: Progs may not be fully compatable\n (%4.2f != %4.2f)\n", fl, VERSION);
	}
	else
		Con_DPrintf("function \"float(float ver) FTE_init\" not found\n");

	if ((f = PR_FindFunction (svprogfuncs, "VersionChat", num )))
	{
		pr_globals = PR_globals(svprogfuncs, num);
		G_FLOAT(OFS_PARM0) = VERSION;
		PR_ExecuteProgram (svprogfuncs, f);

		fl = G_FLOAT(OFS_RETURN);
		if (fl < 0)
			SV_Error ("PR_LoadProgs: progs.dat is not compatable with EXE version");
		else if ((int) (fl*1000) != (int) (VERSION*1000))
			Con_DPrintf("Warning: Progs may not be fully compatable\n (%4.2f != %4.2f)\n", fl, VERSION);
	}

	strcpy(svs.progsnames[svs.numprogs], name);
	svs.progsnum[svs.numprogs] = num;
	svs.numprogs++;	

	return num;
}

void PR_Decompile_f(void)
{
	if (!svprogfuncs)
	{
		Con_Printf("Progs not running, you need to start a server first\n");
		return;
	}

	if (Cmd_Argc() == 1)
		svprogfuncs->Decompile(svprogfuncs, "qwprogs.dat");
	else
		svprogfuncs->Decompile(svprogfuncs, Cmd_Argv(1));
}
void PR_Compile_f(void)
{
	int argc=3;
	double time = Sys_DoubleTime();
	char *argv[] = {"", "-srcfile", "qwprogs.src"};

	if (Cmd_Argc() != 1)
		argv[2] = Cmd_Argv(1);

	if (!svprogfuncs)
		Q_SetProgsParms(true);
	if (svprogfuncs->PR_StartCompile(svprogfuncs, argc, argv))
		while(svprogfuncs->PR_ContinueCompile(svprogfuncs));

	time = Sys_DoubleTime() - time;

	Con_TPrintf(STL_COMPILEROVER, time);
}

void PR_ApplyCompilation_f (void)
{
	edict_t *ent;
	char *s;
	int len, i;
	if (sv.state < ss_active)
	{
		Con_Printf("Can't apply: Server isn't running or is still loading\n");
		return;
	}

	Con_Printf("Saving state\n");
	s = PR_SaveEnts(svprogfuncs, NULL, &len, 1);


	PR_Configure(svprogfuncs, NULL, -1, MAX_PROGS);
	PR_RegisterFields();
	PR_InitEnts(svprogfuncs, sv.max_edicts);
	
	pr_edict_size=svprogfuncs->load_ents(svprogfuncs, s, 0);
	

	PR_LoadGlabalStruct();

	pr_global_struct->time = sv.time;
	

	SV_ClearWorld ();

	for (i=0 ; i<sv.allocated_client_slots ; i++)
	{
		ent = EDICT_NUM(svprogfuncs, i+1);

		svs.clients[i].edict = ent;
	}

	ent = sv.edicts;
	for (i=0 ; i<sv.num_edicts ; i++)
	{
		ent = EDICT_NUM(svprogfuncs, i);
		if (ent->isfree)
			continue;

		SV_LinkEdict (ent, false);	// force retouch even for stationary
	}

	svprogfuncs->parms->memfree(s);
}

void PR_BreakPoint_f(void)
{
	int wasset;
	int isset;
	char *filename = Cmd_Argv(1);
	int line = atoi(Cmd_Argv(2));

	if (!svprogfuncs)
	{
		Con_Printf("Start the server first\n");
		return;
	}
	wasset = svprogfuncs->ToggleBreak(svprogfuncs, filename, line, 3);
	isset = svprogfuncs->ToggleBreak(svprogfuncs, filename, line, 2);

	if (wasset == isset)
		Con_Printf("Breakpoint was not valid\n");
	else if (isset)
		Con_Printf("Breakpoint has been set\n");
	else
		Con_Printf("Breakpoint has been cleared\n");

}

typedef char char32[32];
char32 sv_addonname[MAXADDONS];
void PR_Init(void)
{
	int i;	
	Cmd_AddCommand ("breakpoint", PR_BreakPoint_f);
	Cmd_AddCommand ("decompile", PR_Decompile_f);
	Cmd_AddCommand ("compile", PR_Compile_f);
	Cmd_AddCommand ("applycompile", PR_ApplyCompilation_f);
	
	Cvar_Register(&pr_maxedicts, cvargroup_progs);
	Cvar_Register(&pr_imitatemvdsv, cvargroup_progs);
	Cvar_Register(&pr_fixbrokenqccarrays, cvargroup_progs);

	for (i = 0; i < MAXADDONS; i++)
	{
		sprintf(sv_addonname[i], "addon%i", i);
		sv_addon[i].name = sv_addonname[i];
		sv_addon[i].string = "";
		sv_addon[i].flags = CVAR_NOTFROMSERVER;
		Cvar_Register(&sv_addon[i], cvargroup_progs);
	}

	Cvar_Register (&nomonsters, cvargroup_progs);
	Cvar_Register (&gamecfg, cvargroup_progs);
	Cvar_Register (&scratch1, cvargroup_progs);
	Cvar_Register (&scratch2, cvargroup_progs);
	Cvar_Register (&scratch3, cvargroup_progs);
	Cvar_Register (&scratch4, cvargroup_progs);
	Cvar_Register (&savedgamecfg, cvargroup_progs);
	Cvar_Register (&saved1, cvargroup_progs);
	Cvar_Register (&saved2, cvargroup_progs);
	Cvar_Register (&saved3, cvargroup_progs);
	Cvar_Register (&saved4, cvargroup_progs);
	Cvar_Register (&temp1, cvargroup_progs);
	Cvar_Register (&noexit, cvargroup_progs);

	Cvar_Register (&progs, cvargroup_progs);
	Cvar_Register (&pr_compatabilitytest, cvargroup_progs);

	Cvar_Register (&qc_nonetaccess, cvargroup_progs);
	Cvar_Register (&pr_overridebuiltins, cvargroup_progs);
}

void Q_InitProgs(void)
{
	int i, i2;
	func_t f, f2;
	globalvars_t *pr_globals;
	static char addons[2048];
	char *as, *a;
	int num = 0;
	progsnum_t prnum, oldprnum;
	int d1, d2;

	QC_Clear();

	Q_SetProgsParms(false);


// load progs to get entity field count
	PR_Configure(svprogfuncs, NULL, -1, MAX_PROGS);

	PR_RegisterFields();

	num = svs.numprogs;
	svs.numprogs=0;
	
	d1 = COM_FDepthFile("progs.dat", true);
	d2 = COM_FDepthFile("qwprogs.dat", true);
	if (d1 < d2)	//progs.dat is closer to the gamedir
		strcpy(addons, "progs.dat");
	else if (d1 > d2)	//qwprogs.dat is closest
	{
		strcpy(addons, "qwprogs.dat");
		d1 = d2;
	}
	//both are an equal distance - same path.
	else if (deathmatch.value && !COM_CheckParm("-game"))	//if deathmatch, default to qw
	{
		strcpy(addons, "qwprogs.dat");
		d1 = d2;
	}
	else					//single player/coop is better done with nq.
	{
		strcpy(addons, "progs.dat");
	}
							//if progs cvar is left blank and a q2 map is loaded, the server will use the q2 game dll.
							//if you do set a value here, q2 dll is not used.


	//hexen2 - maplist contains a list of maps that we need to use an alternate progs.dat for.
	d2 = COM_FDepthFile("maplist.txt", true);
	if (d2 <= d1)//Use it if the maplist.txt file is within a more or equal important gamedir.
	{
		int j, maps;
		char *f;

		f = COM_LoadTempFile("maplist.txt");
		f = COM_Parse(f);
		maps = atoi(com_token);
		for (j = 0; j < maps; j++)
		{
			f = COM_Parse(f);
			if (!Q_strcasecmp(sv.name, com_token))
			{
				f = COM_Parse(f);
				strcpy(addons, com_token);
				break;
			}
			f = strchr(f, '\n');	//skip to the end of the line.
		}
	}

	if (*progs.string && strlen(progs.string)<64 && *progs.string != '*')	//a * is a special case to not load a q2 dll.
	{
		Q_strncpyz(addons, progs.string, MAX_QPATH);
		COM_DefaultExtension(addons, ".dat");
	}
	oldprnum= AddProgs(addons);

	if (oldprnum < 0 && strcmp(addons, "qwprogs.dat"))
	{
#ifndef SERVERONLY
		if (SCR_UpdateScreen)
			SCR_UpdateScreen();
#endif
		oldprnum= AddProgs("qwprogs.dat");
	}

	if (oldprnum < 0 && strcmp(addons, "progs.dat"))
	{
#ifndef SERVERONLY
		if (SCR_UpdateScreen)
			SCR_UpdateScreen();
#endif
		oldprnum= AddProgs("progs.dat");
	}
	if (oldprnum < 0)
		Sys_Error("Couldn't open or compile progs\n");
		

	f = PR_FindFunction (svprogfuncs, "AddAddonProgs", oldprnum);	
/*	if (num)
	{
		//restore progs
		for (i = 1; i < num; i++)
		{
			if (f)
			{
				pr_globals = PR_globals(PR_CURRENT);
				G_SETSTRING(OFS_PARM0, svs.progsnames[i]);
				PR_ExecuteProgram (f);
			}
			else
			{
				prnum = AddProgs(svs.progsnames[i]);
				f2 = PR_FindFunction ( "init", prnum);

				if (f2)
				{
					pr_globals = PR_globals(PR_CURRENT);
					G_PROG(OFS_PARM0) = oldprnum;
					PR_ExecuteProgram(f2);					
				}
				oldprnum=prnum;
			}
		}
	}
*/
	//additional (always) progs
	as = NULL;
	a = COM_LoadStackFile("mod.gam", addons, 2048);

	if (a)
	{
		as = strstr(a, "extraqwprogs=");
		if (as)
		{
		for (a = as+13; *a; a++)
		{
			if (*a < ' ')
			{
				*a = '\0';
				break;
			}
		}
		a = (as+=13);
		}
	}
	if (as)
	{
		while(*a)
		{
			if (*a == ';')
			{
				*a = '\0';
				for (i = 0; i < svs.numprogs; i++)	//don't add if already added
				{
					if (!strcmp(svs.progsnames[i], as))
						break;
				}
				if (i == svs.numprogs)
				{
					if (f)
					{
						pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
						(string_t)G_INT(OFS_PARM0) = PR_SetString(svprogfuncs, as);
						PR_ExecuteProgram (svprogfuncs, f);
					}
					else
					{
						prnum = AddProgs(as);
						if (prnum>=0)
						{
							f2 = PR_FindFunction (svprogfuncs, "init", prnum);

							if (f2)
							{
								pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
								G_PROG(OFS_PARM0) = oldprnum;
								PR_ExecuteProgram(svprogfuncs, f2);
							}
							oldprnum=prnum;
						}
					}					
				}
				*a = ';';
				as = a+1;
			}
			a++;
		}
	}

	if (COM_FDepthFile("fteadd.dat", true)!=0x7fffffff)
	{
		prnum = AddProgs("fteadd.dat");
		if (prnum>=0)
		{
			f2 = PR_FindFunction (svprogfuncs, "init", prnum);

			if (f2)
			{
				pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
				G_PROG(OFS_PARM0) = oldprnum;
				PR_ExecuteProgram(svprogfuncs, f2);
			}
			oldprnum=prnum;
		}
	}
	prnum = 0;

	switch (sv.worldmodel->fromgame)
	{
	case fg_quake2:
		if (COM_FDepthFile("q2bsp.dat", true)!=0x7fffffff)
			prnum = AddProgs("q2bsp.dat");
		break;
	case fg_quake3:
		if (COM_FDepthFile("q3bsp.dat", true)!=0x7fffffff)
			prnum = AddProgs("q3bsp.dat");
		else if (COM_FDepthFile("q2bsp.dat", true)!=0x7fffffff)	//fallback
			prnum = AddProgs("q2bsp.dat");
		break;
	case fg_doom:
		if (COM_FDepthFile("doombsp.dat", true)!=0x7fffffff)
			prnum = AddProgs("doombsp.dat");
		break;
	case fg_halflife:
		if (COM_FDepthFile("hlbsp.dat", true)!=0x7fffffff)
			prnum = AddProgs("hlbsp.dat");
		break;
	}

	if (prnum>=0)
	{
		f2 = PR_FindFunction (svprogfuncs, "init", prnum);

		if (f2)
		{
			pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
			G_PROG(OFS_PARM0) = oldprnum;
			PR_ExecuteProgram(svprogfuncs, f2);
		}
		oldprnum=prnum;
	}

	//progs depended on by maps.
	a = as = COM_LoadStackFile(va("maps/%s.inf", sv.name), addons, 2048);
	if (a)
	{
		as = strstr(a, "qwprogs=");
		if (as)
		{
		for (a = as+11; *a; a++)
		{
			if (*a < ' ')
			{
				*a = '\0';
				break;
			}
		}
		a = (as+=11);
		}
	}
	if (as)
	{
		while(*a)
		{
			if (*a == ';')
			{
				*a = '\0';
				for (i = 0; i < svs.numprogs; i++)	//don't add if already added
				{
					if (!strcmp(svs.progsnames[i], as))
						break;
				}
				if (i == svs.numprogs)
				{
					if (f)
					{
						pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
						(string_t)G_INT(OFS_PARM0) = PR_SetString(svprogfuncs, as);
						PR_ExecuteProgram (svprogfuncs, f);					
					}
					else
					{
						prnum = AddProgs(as);
						if (prnum>=0)
						{
							f2 = PR_FindFunction (svprogfuncs, "init", prnum);

							if (f2)
							{
								pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
								G_PROG(OFS_PARM0) = oldprnum;
								PR_ExecuteProgram(svprogfuncs, f2);
							}
							oldprnum=prnum;
						}
					}					
				}
				*a = ';';
				as = a+1;
			}
			a++;
		}
	}

	//add any addons specified
	for (i2 = 0; i2 < MAXADDONS; i2++)
	{
		if (*sv_addon[i2].string)
		{
			for (i = 0; i < svs.numprogs; i++)	//don't add if already added
			{
				if (!strcmp(svs.progsnames[i], sv_addon[i2].string))
					break;
			}
			if (i == svs.numprogs)	//Not added yet. Add it.
			{					
				if (f)
				{
					pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
					(string_t)G_INT(OFS_PARM0) = PR_SetString(svprogfuncs, as);
					PR_ExecuteProgram (svprogfuncs, f);					
				}
				else
				{
					prnum = AddProgs(sv_addon[i2].string);
					if (prnum >= 0)
					{
						f2 = PR_FindFunction (svprogfuncs, "init", prnum);
						if (f2)
						{
							pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
							G_PROG(OFS_PARM0) = oldprnum;
							PR_ExecuteProgram(svprogfuncs, f2);
						}
						oldprnum=prnum;
					}

				}
			}
		}
	}

	sv.max_edicts = pr_maxedicts.value;
	pr_edict_size = PR_InitEnts(svprogfuncs, sv.max_edicts);
}

qboolean PR_QCChat(char *text, int say_type)
{
	globalvars_t *pr_globals = PR_globals(svprogfuncs, PR_CURRENT);

	if (!ChatMessage)
		return false;

	(string_t)G_INT(OFS_PARM0) = PR_SetString(svprogfuncs, text);
	G_FLOAT(OFS_PARM1) = say_type;
	PR_ExecuteProgram (svprogfuncs, ChatMessage);

	if (G_FLOAT(OFS_RETURN))
		return true;
	return false;
}

qboolean PR_UserCmd(char *s)
{
	globalvars_t *pr_globals;
#ifdef Q2SERVER
	if (ge)
	{
		ge->ClientCommand(host_client->q2edict);
		return true;	//the dll will convert in to chat.
	}
#endif

	SV_EndRedirect ();

	pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
	if (SV_ParseClientCommand)
	{	//this one is queryable, so it's the one that is proven to exist.
		//things won't mind if the other system doesn't exist.
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
		
		G_INT(OFS_PARM0) = (int)PR_SetString(svprogfuncs, s);
		PR_ExecuteProgram (svprogfuncs, SV_ParseClientCommand);
		return false;
	}
	if (mod_UserCmd)
	{
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
		
		G_INT(OFS_PARM0) = (int)PR_SetString(svprogfuncs, s);
		PR_ExecuteProgram (svprogfuncs, mod_UserCmd);
		return (int) G_FLOAT(OFS_RETURN);
	}

	return false;
}
qboolean PR_ConsoleCmd(void)
{
	globalvars_t *pr_globals;
	extern redirect_t sv_redirected;
#ifdef Q2SERVER
	if (ge)
	{	//server command
		if (!strcmp(Cmd_Argv(0), "sv"))
		{
			ge->ServerCommand();
			return true;
		}
		return false;
	}
#endif

	pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
	if (mod_ConsoleCmd)
	{
		if (sv_redirected != RD_OBLIVION)
		{
			pr_global_struct->time = sv.time;
			pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
		}
		
		PR_ExecuteProgram (svprogfuncs, mod_ConsoleCmd);
		return (int) G_FLOAT(OFS_RETURN);
	}

	return false;
}

void PR_ClientUserInfoChanged(char *name, char *oldivalue, char *newvalue)
{
	if (UserInfo_Changed)
	{
		globalvars_t *pr_globals;
		pr_globals = PR_globals(svprogfuncs, PR_CURRENT);

		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);

		G_INT(OFS_PARM0) = (int)PR_SetString(svprogfuncs, name);
		G_INT(OFS_PARM1) = (int)PR_SetString(svprogfuncs, oldivalue);
		G_INT(OFS_PARM2) = (int)PR_SetString(svprogfuncs, newvalue);

		PR_ExecuteProgram (svprogfuncs, UserInfo_Changed);
	}
}

void PR_LocalInfoChanged(char *name, char *oldivalue, char *newvalue)
{
	if (localinfoChanged && sv.state)
	{
		globalvars_t *pr_globals;
		pr_globals = PR_globals(svprogfuncs, PR_CURRENT);

		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv.edicts);

		G_INT(OFS_PARM0) = (int)PR_SetString(svprogfuncs, name);
		G_INT(OFS_PARM1) = (int)PR_SetString(svprogfuncs, oldivalue);
		G_INT(OFS_PARM2) = (int)PR_SetString(svprogfuncs, newvalue);

		PR_ExecuteProgram (svprogfuncs, localinfoChanged);
	}
}

void VARGS PR_BIError(char *format, ...)
{
	va_list		argptr;
	static char		string[2048];
	
	va_start (argptr, format);
	_vsnprintf (string,sizeof(string)-1, format,argptr);
	va_end (argptr);

	if (developer.value)
	{
		globalvars_t *pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
		Con_Printf("%s\n", string);
		pr_trace = 1;		
		G_INT(OFS_RETURN)=0;	//just in case it was a float and should be an ent...
		G_INT(OFS_RETURN+1)=0;
		G_INT(OFS_RETURN+2)=0;
	}
	else
		SV_Error("%s\n", string);
}

void QC_Clear(void)
{
}

int prnumforfile;
int PR_SizeOfFile(char *filename)
{
	extern int com_filesize;
//	int size;
	if (!svprogfuncs)
		return -1;
	prnumforfile=svs.numprogs-1;
	while(prnumforfile>=0)
	{
		if ((qbyte *)svprogfuncs->filefromprogs(svprogfuncs, prnumforfile, filename, &com_filesize, NULL)==(qbyte *)0xffffffff)
			return com_filesize;
		prnumforfile--;
	}
	return -1;
}
qbyte *PR_OpenFile(char *filename, qbyte *buffer)
{
	return svprogfuncs->filefromprogs(svprogfuncs, prnumforfile, filename, NULL, buffer);
}

char *Translate(char *message);

char *PF_VarString (progfuncs_t *prinst, int	first, globalvars_t *pr_globals)
{
	int		i;
	static char buffer[2][4096];
	static int bufnum;
	char *s, *out;

	out = buffer[(bufnum++)&1];
	
	out[0] = 0;
	for (i=first ; i<*prinst->callargc ; i++)
	{
		s = PR_GetStringOfs(prinst, OFS_PARM0+i*3);
		if (s)
		{
			strcat (out, Translate(s));

//#ifdef PARANOID
			if (strlen(out)+1 >= sizeof(buffer[0]))
				Sys_Error("VarString (builtin call ending with strings) exceeded maximum string length of %i chars", sizeof(buffer[0]));
//#endif
		}
	}
	return out;
}


//#define	RETURN_EDICT(pf, e) (((int *)pr_globals)[OFS_RETURN] = EDICT_TO_PROG(pf, e))
#define	RETURN_SSTRING(s) ((char *)((int *)pr_globals)[OFS_RETURN] = PR_SetString(prinst, s))	//static - exe will not change it.
#define	RETURN_TSTRING(s) ((char *)((int *)pr_globals)[OFS_RETURN] = PR_SetString(prinst, s))	//temp (static but cycle buffers?)
#define	RETURN_CSTRING(s) ((char *)((int *)pr_globals)[OFS_RETURN] = PR_SetString(prinst, s))	//semi-permanant. (hash tables?)
#define	RETURN_PSTRING(s) ((char *)((int *)pr_globals)[OFS_RETURN] = PR_NewString(prinst, s))	//permanant

/*
===============================================================================

						BUILT-IN FUNCTIONS

===============================================================================
*/






int externcallsdepth;
void PR_MoveParms(int progs1, int progs2);	//from 2 to 1
void PF_externcall (progfuncs_t *prinst, globalvars_t *pr_globals)	//this func calls a function in annother progs (by name)
{
	int progsnum;
	char *funcname;	
	int i;
	func_t f;

	progsnum = G_PROG(OFS_PARM0);
	funcname = PR_GetStringOfs(prinst, OFS_PARM1);

	f = PR_FindFunction(prinst, funcname, progsnum);
	if (f)
	{
		for (i = OFS_PARM0; i < OFS_PARM5; i+=3)
			VectorCopy(G_VECTOR(i+(2*3)), G_VECTOR(i));

		(pr_trace)++;	//continue debugging
		PR_ExecuteProgram(prinst, f);
	}
	else if (!f)
	{
		f = PR_FindFunction(prinst, "MissingFunc", progsnum);
		if (!f)
		{
			PR_BIError("Couldn't find function %s", funcname);
			return;
		}

		for (i = OFS_PARM0; i < OFS_PARM6; i+=3)
			VectorCopy(G_VECTOR(i+(1*3)), G_VECTOR(i));		
		G_INT(OFS_PARM0) = (int)funcname;

		(pr_trace)++;	//continue debugging
		PR_ExecuteProgram(prinst, f);
	}
}

//this func calls a function in annother progs
//it works in the same way as the above func, except that it calls by reference to a function, as opposed to by it's name
//used for entity function variables - not actually needed anymore
void PF_externrefcall (progfuncs_t *prinst, globalvars_t *pr_globals)
{
	int progsnum;
	func_t f;
	progsnum = G_PROG(OFS_PARM0);
	f = G_INT(OFS_PARM1);
	
	(pr_trace)++;	//continue debugging.
	PR_ExecuteProgram(prinst, f);
}

float PR_LoadAditionalProgs(char *s);
void PF_addprogs(progfuncs_t *prinst, globalvars_t *pr_globals)
{
	char *s = PR_GetStringOfs(prinst, OFS_PARM0);	
	if (!s || !*s)
	{
		G_PROG(OFS_RETURN)=-1;
		return;
	}
	G_PROG(OFS_RETURN) = AddProgs(s);
}

void PF_externvalue (progfuncs_t *prinst, globalvars_t *pr_globals)	//return a value in annother progs
{
	int n = G_PROG(OFS_PARM0);
	char *varname = PF_VarString(prinst, 1, pr_globals);
	eval_t *var;

	var = svprogfuncs->FindGlobal(prinst, varname, n);
	
	if (var)
		G_INT(OFS_RETURN) = var->_int;
	else
		G_INT(OFS_RETURN) = 0;
}

void PF_externset (progfuncs_t *prinst, globalvars_t *pr_globals)	//set a value in annother progs
{
	int n = G_PROG(OFS_PARM0);
	int v = G_INT(OFS_PARM1);
	char *varname = PF_VarString(prinst, 2, pr_globals);
	eval_t *var;

	if (n < 0)
	{
		for (n = 0; n < svs.numprogs; n++)
		{
			var = svprogfuncs->FindGlobal(prinst, varname, svs.progsnum[n]);
			if (var)
				var->_int = v;
		}
		return;
	}
	var = svprogfuncs->FindGlobal(prinst, varname, n);
	
	if (var)
	var->_int = v;
}

void PF_instr (progfuncs_t *prinst, globalvars_t *pr_globals)
{	
	char *sub;
	char *s1;
	char *s2;

	s1 = PR_GetStringOfs(prinst, OFS_PARM0);
	s2 = PF_VarString(prinst, 1, pr_globals);

	if (!s1 || !s2)
	{
		PR_BIError("Null string in \"instr\"\n");
		return;
	}

	sub = strstr(s1, s2);

	if (sub == NULL)
		RETURN_SSTRING("");
	else
		RETURN_SSTRING(sub);	//last as long as the origional string
}




/*
char *PF_VarString (int	first)
{
	int		i;
	static char out[256];
	
	out[0] = 0;
	for (i=first ; i<pr_argc ; i++)
	{
		strcat (out, G_STRING((OFS_PARM0+i*3)));
	}
	return out;
}
*/

/*
=================
PF_errror

This is a TERMINAL error, which will kill off the entire server.
Dumps self.

error(value)
=================
*/
void PF_break (progfuncs_t *prinst, struct globalvars_s *pr_globals);
void PF_error (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*s;
	
	s = PF_VarString(prinst, 0, pr_globals);
/*	Con_Printf ("======SERVER ERROR in %s:\n%s\n", PR_GetString(pr_xfunction->s_name) ,s);
	ed = PROG_TO_EDICT(pr_global_struct->self);
	ED_Print (ed);
*/	

	Con_Printf("%s", s);

	if (developer.value)
	{
//		SV_Error ("Program error: %s", s);
		PF_break(prinst, pr_globals);
		pr_trace = 2;
	}
	else
		SV_Error ("Program error: %s", s);
}

/*
=================
PF_objerror

Dumps out self, then an error message.  The program is aborted and self is
removed, but the level can continue.

objerror(value)
=================
*/
void PF_objerror (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*s;
	edict_t	*ed;
	
	s = PF_VarString(prinst, 0, pr_globals);
/*	Con_Printf ("======OBJECT ERROR in %s:\n%s\n", PR_GetString(pr_xfunction->s_name),s);
*/	ed = PROG_TO_EDICT(prinst, pr_global_struct->self);
/*	ED_Print (ed);
*/
	ED_Print(prinst, ed);
	Con_Printf("%s", s);

	if (developer.value)
		pr_trace = 2;
	else
	{
		ED_Free (svprogfuncs, ed);
	
		SV_Error ("Program error :%s", s);

		if (sv.time > 10)
			Cbuf_AddText("restart\n", RESTRICT_LOCAL);
	}
}



/*
==============
PF_makevectors

Writes new values for v_forward, v_up, and v_right based on angles
makevectors(vector)
==============
*/
void PF_makevectors (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	AngleVectors (G_VECTOR(OFS_PARM0), P_VEC(v_forward), P_VEC(v_right), P_VEC(v_up));
}

/*
=================
PF_setorigin

This is the only valid way to move an object without using the physics of the world (setting velocity and waiting).  Directly changing origin will not set internal links correctly, so clipping would be messed up.  This should be called when an object is spawned, and then only if it is teleported.

setorigin (entity, origin)
=================
*/
void PF_setorigin (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*e;
	float	*org;
	
	e = G_EDICT(prinst, OFS_PARM0);
	org = G_VECTOR(OFS_PARM1);
	VectorCopy (org, e->v.origin);
	SV_LinkEdict (e, false);
}


/*
=================
PF_setsize

the size box is rotated by the current angle

setsize (entity, minvector, maxvector)
=================
*/
void PF_setsize (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*e;
	float	*min, *max;
	
	e = G_EDICT(prinst, OFS_PARM0);
	if (e->isfree)
	{
		Con_TPrintf(STL_EDICTWASFREE, "setsize");
		pr_trace = 1;
		return;
	}
	min = G_VECTOR(OFS_PARM1);
	max = G_VECTOR(OFS_PARM2);
	VectorCopy (min, e->v.mins);
	VectorCopy (max, e->v.maxs);
	VectorSubtract (max, min, e->v.size);
	SV_LinkEdict (e, false);
}


/*
=================
PF_setmodel

setmodel(entity, model)
Also sets size, mins, and maxs for inline bmodels
=================
*/
void PF_setmodel (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*e;
	char	*m;
	int		i;
	model_t	*mod;

	e = G_EDICT(prinst, OFS_PARM0);
	m = PR_GetStringOfs(prinst, OFS_PARM1);

// check to see if model was properly precached
	if (!m || !*m)
		i = 0;
	else
	{
		for (i=1; *sv.model_precache[i] ; i++)
		{
			if (!strcmp(sv.model_precache[i], m))
			{
				m = sv.model_precache[i];
				break;
			}
		}
		if (i==MAX_MODELS || !*sv.model_precache[i])
		{
			if (i!=MAX_MODELS && sv.state == ss_loading)
			{
				Q_strncpyz(sv.model_precache[i], m, sizeof(sv.model_precache[i]));
				if (!strcmp(m + strlen(m) - 4, ".bsp"))
					sv.models[i] = Mod_FindName(sv.model_precache[i]);
				Con_Printf("WARNING: SV_ModelIndex: model %s not precached", m);
			}
			else
			{
				PR_BIError ("no precache: %s\n", m);
				return;
			}
		}
	}
		
	e->v.model = PR_SetString(prinst, sv.model_precache[i]);
	e->v.modelindex = i;

	// if it is an inline model, get the size information for it
	if (m[0] == '*' || (*m&&progstype == PROG_H2))
	{
		mod = Mod_ForName (m, true);
		VectorCopy (mod->mins, e->v.mins);
		VectorCopy (mod->maxs, e->v.maxs);
		VectorSubtract (mod->maxs, mod->mins, e->v.size);
		SV_LinkEdict (e, false);

		return;
	}

	if (progstype == PROG_H2)
	{
		e->v.mins[0] = 0;
		e->v.mins[1] = 0;
		e->v.mins[2] = 0;

		e->v.maxs[0] = 0;
		e->v.maxs[1] = 0;
		e->v.maxs[2] = 0;

		VectorSubtract (e->v.maxs, e->v.mins, e->v.size);
	}
	else
	{	
		if (progstype != PROG_QW)
		{	//also uses setsize.
			int len = strlen(m);
			if (m[len-4] == '.' && m[len-3] == 'b' && m[len-2] == 's' && m[len-1] == 'p')
				mod = Mod_ForName (m, false);
			else
				mod = NULL;
			if (mod)
			{
				VectorCopy (mod->mins, e->v.mins);
				VectorCopy (mod->maxs, e->v.maxs);
				VectorSubtract (mod->maxs, mod->mins, e->v.size);
				SV_LinkEdict (e, false);
			}
			else
			{
				VectorCopy (vec3_origin, e->v.mins);
				VectorCopy (vec3_origin, e->v.maxs);
				VectorSubtract (vec3_origin, vec3_origin, e->v.size);
				SV_LinkEdict (e, false);
			}
		}
		else
		{
			if (sv.models[i])
			{
				mod = Mod_ForName (m, true);
				VectorCopy (mod->mins, e->v.mins);
				VectorCopy (mod->maxs, e->v.maxs);
				VectorSubtract (mod->maxs, mod->mins, e->v.size);
				SV_LinkEdict (e, false);
			}
		}
	}
}

void PF_set_puzzle_model (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{	//qc/hc lacks string manipulation.
	char *shortname;
	char fullname[MAX_QPATH];
	shortname = PR_GetStringOfs(prinst, OFS_PARM1);
	_snprintf(fullname, sizeof(fullname)-1, "models/puzzle/%s.mdl", shortname);
	G_INT(OFS_PARM1) = (int)(fullname - prinst->stringtable);
	PF_setmodel (prinst, pr_globals);
	G_INT(OFS_PARM1) = (int)(shortname - prinst->stringtable);	//piece of mind.
}

/*
=================
PF_bprint

broadcast print to everyone on server

bprint(value)
=================
*/
void PF_bprint (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char		*s;
	int			level;

	if (sv.demofile)
		return;
	
	if (progstype == PROG_NQ)
	{
		level = PRINT_HIGH;

		s = PF_VarString(prinst, 0, pr_globals);
	}
	else
	{
		level = G_FLOAT(OFS_PARM0);

		s = PF_VarString(prinst, 1, pr_globals);
	}
	SV_BroadcastPrintf (level, "%s", s);
}

/*
=================
PF_sprint

single print to a specific client

sprint(clientent, value)
=================
*/
void PF_sprint (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char		*s;
	client_t	*client;
	int			entnum;
	int			level;
	
	if (sv.demofile)
		return;

	entnum = G_EDICTNUM(prinst, OFS_PARM0);

	if (progstype == PROG_NQ || progstype == PROG_H2)
	{
		level = PRINT_HIGH;

		s = PF_VarString(prinst, 1, pr_globals);
	}
	else
	{
		level = G_FLOAT(OFS_PARM1);

		s = PF_VarString(prinst, 2, pr_globals);
	}
	
	if (entnum < 1 || entnum > sv.allocated_client_slots)
	{
		Con_TPrintf (STL_BADSPRINT);
		return;
	}
		
	client = &svs.clients[entnum-1];
	
	SV_ClientPrintf (client, level, "%s", s);
}


/*
=================
PF_centerprint

single print to a specific client

centerprint(clientent, value)
=================
*/
void PF_centerprint (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char		*s;
	int			entnum;
	client_t	*cl, *sp;

	if (sv.demofile)
		return;
	
	entnum = G_EDICTNUM(prinst, OFS_PARM0);
	s = PF_VarString(prinst, 1, pr_globals);
	
	if (entnum < 1 || entnum > sv.allocated_client_slots)
	{
		Con_TPrintf (STL_BADSPRINT);
		return;
	}
		
	cl = &svs.clients[entnum-1];

	if (cl->controller)
	{	//this is a slave client.
		//find the right number and send.
		int pnum = 0;
		for (sp = cl->controller; sp; sp = sp->controlled)
		{
			if (sp == cl)
				break;
			pnum++;
		}
		sp = cl->controller;

		ClientReliableWrite_Begin (sp, svc_choosesplitclient, 4 + strlen(s));
		ClientReliableWrite_Byte (sp, pnum);
		ClientReliableWrite_Byte (sp, svc_centerprint);
		ClientReliableWrite_String (sp, s);
	}
	else
	{
		ClientReliableWrite_Begin (cl, svc_centerprint, 2 + strlen(s));
		ClientReliableWrite_String (cl, s);
	}
}


/*
=================
PF_normalize

vector normalize(vector)
=================
*/
void PF_normalize (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	*value1;
	vec3_t	newvalue;
	float	new;
	
	value1 = G_VECTOR(OFS_PARM0);

	new = value1[0] * value1[0] + value1[1] * value1[1] + value1[2]*value1[2];
	new = sqrt(new);
	
	if (new == 0)
		newvalue[0] = newvalue[1] = newvalue[2] = 0;
	else
	{
		new = 1/new;
		newvalue[0] = value1[0] * new;
		newvalue[1] = value1[1] * new;
		newvalue[2] = value1[2] * new;
	}
	
	VectorCopy (newvalue, G_VECTOR(OFS_RETURN));	
}

/*
=================
PF_vlen

scalar vlen(vector)
=================
*/
void PF_vlen (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	*value1;
	float	new;
	
	value1 = G_VECTOR(OFS_PARM0);

	new = value1[0] * value1[0] + value1[1] * value1[1] + value1[2]*value1[2];
	new = sqrt(new);
	
	G_FLOAT(OFS_RETURN) = new;
}

/*
=================
PF_vlen

scalar vhlen(vector)
=================
*/
void PF_vhlen (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	*value1;
	float	new;
	
	value1 = G_VECTOR(OFS_PARM0);

	new = value1[0] * value1[0] + value1[1] * value1[1];
	new = sqrt(new);
	
	G_FLOAT(OFS_RETURN) = new;
}

/*
=================
PF_vectoyaw

float vectoyaw(vector)
=================
*/
void PF_vectoyaw (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	*value1;
	float	yaw;
	
	value1 = G_VECTOR(OFS_PARM0);

	if (value1[1] == 0 && value1[0] == 0)
		yaw = 0;
	else
	{
		yaw = (int) (atan2(value1[1], value1[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;
	}

	G_FLOAT(OFS_RETURN) = yaw;
}

void PF_anglemod (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float v = G_FLOAT(OFS_PARM0);

	while (v >= 360)
		v = v - 360;
	while (v < 0)
		v = v + 360;

	G_FLOAT(OFS_RETURN) = v;
}


/*
=================
PF_vectoangles

vector vectoangles(vector)
=================
*/
void PF_vectoangles (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	*value1;
	float	forward;
	float	yaw, pitch;
	
	value1 = G_VECTOR(OFS_PARM0);

	if (value1[1] == 0 && value1[0] == 0)
	{
		yaw = 0;
		if (value1[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		yaw = (int) (atan2(value1[1], value1[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;

		forward = sqrt (value1[0]*value1[0] + value1[1]*value1[1]);
		pitch = (int) (atan2(value1[2], forward) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}

	G_FLOAT(OFS_RETURN+0) = pitch;
	G_FLOAT(OFS_RETURN+1) = yaw;
	G_FLOAT(OFS_RETURN+2) = 0;
}


static	long	predictablerandx = 1;

void predictablesrand(unsigned int x)
{
	predictablerandx = x;
}
int predictablerandgetseed(void)
{
	return predictablerandx;
}


int predictablerand(void)
{
	return(((predictablerandx = predictablerandx*1103515245 + 12345)>>16) & 077777);
}

/*
=================
PF_Random

Returns a number from 0<= num < 1

random()
=================
*/
void PF_random (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float		num;

	if (svs.demorecording || svs.demoplayback)
		num = (predictablerand ()&0x7fff) / ((float)0x7fff);
	else
		num = (rand ()&0x7fff) / ((float)0x7fff);
	
	G_FLOAT(OFS_RETURN) = num;
}

/*
=================
PF_particle

particle(origin, color, count)
=================
*/
void PF_particle (progfuncs_t *prinst, globalvars_t *pr_globals)	//I said it was for compatability only.
{
	float		*org, *dir;
	float		color;
	float		count;
	int i, v;

	return;
			
	org = G_VECTOR(OFS_PARM0);
	dir = G_VECTOR(OFS_PARM1);
	color = G_FLOAT(OFS_PARM2);
	count = G_FLOAT(OFS_PARM3);
#ifdef NQPROT
	MSG_WriteByte (&sv.nqmulticast, svc_particle);
	MSG_WriteCoord (&sv.nqmulticast, org[0]);
	MSG_WriteCoord (&sv.nqmulticast, org[1]);
	MSG_WriteCoord (&sv.nqmulticast, org[2]);
	for (i=0 ; i<3 ; i++)
	{
		v = dir[i]*16;
		if (v > 127)
			v = 127;
		else if (v < -128)
			v = -128;
		MSG_WriteChar (&sv.nqmulticast, v);
	}
	MSG_WriteByte (&sv.nqmulticast, count*20);
	MSG_WriteByte (&sv.nqmulticast, color);
#endif
	//for qw users (and not fte)
	if (color == 73)
	{
		MSG_WriteByte (&sv.multicast, svc_temp_entity);
		MSG_WriteByte (&sv.multicast, TE_BLOOD);
		MSG_WriteByte (&sv.multicast, count<10?1:(count+10)/20);
		MSG_WriteCoord (&sv.multicast, org[0]);
		MSG_WriteCoord (&sv.multicast, org[1]);
		MSG_WriteCoord (&sv.multicast, org[2]);
		SV_MulticastProtExt(org, MULTICAST_PVS, pr_global_struct->dimension_send, 0, PEXT_HEXEN2);
	}
	else if (color == 225)
	{
		MSG_WriteByte (&sv.multicast, svc_temp_entity);
		MSG_WriteByte (&sv.multicast, TE_LIGHTNINGBLOOD);		
		MSG_WriteCoord (&sv.multicast, org[0]);
		MSG_WriteCoord (&sv.multicast, org[1]);
		MSG_WriteCoord (&sv.multicast, org[2]);		
		SV_MulticastProtExt(org, MULTICAST_PVS, pr_global_struct->dimension_send, 0, PEXT_HEXEN2);
	}
	//now we can start fte svc_particle stuff..
	MSG_WriteByte (&sv.multicast, svc_particle);
	MSG_WriteCoord (&sv.multicast, org[0]);
	MSG_WriteCoord (&sv.multicast, org[1]);
	MSG_WriteCoord (&sv.multicast, org[2]);
	for (i=0 ; i<3 ; i++)
	{
		v = dir[i]*16;
		if (v > 127)
			v = 127;
		else if (v < -128)
			v = -128;
		MSG_WriteChar (&sv.multicast, v);
	}
	MSG_WriteByte (&sv.multicast, count);
	MSG_WriteByte (&sv.multicast, color);
	SV_MulticastProtExt(org, MULTICAST_PVS, pr_global_struct->dimension_send, PEXT_HEXEN2, 0);
}

void PF_te_blood (progfuncs_t *prinst, globalvars_t *pr_globals)
{
	float count;
	float *org, *dir;
#ifdef NQPROT
	int i, v;
#endif
	org = G_VECTOR(OFS_PARM0);
	dir = G_VECTOR(OFS_PARM1);
	count = G_FLOAT(OFS_PARM2);

#ifdef NQPROT
	MSG_WriteByte (&sv.nqmulticast, svc_particle);
	MSG_WriteCoord (&sv.nqmulticast, org[0]);
	MSG_WriteCoord (&sv.nqmulticast, org[1]);
	MSG_WriteCoord (&sv.nqmulticast, org[2]);
	for (i=0 ; i<3 ; i++)
	{
		v = dir[i]*16;
		if (v > 127)
			v = 127;
		else if (v < -128)
			v = -128;
		MSG_WriteChar (&sv.nqmulticast, v);
	}
	MSG_WriteByte (&sv.nqmulticast, count);
	MSG_WriteByte (&sv.nqmulticast, 73);
#endif

	MSG_WriteByte (&sv.multicast, svc_temp_entity);
	MSG_WriteByte (&sv.multicast, TE_BLOOD);
	MSG_WriteByte (&sv.multicast, count<10?1:(count+10)/20);
	MSG_WriteCoord (&sv.multicast, org[0]);
	MSG_WriteCoord (&sv.multicast, org[1]);
	MSG_WriteCoord (&sv.multicast, org[2]);
	SV_Multicast(org, MULTICAST_PVS);
}

/*
=================
PF_particle2 - hexen2

particle(origin, dmin, dmax, color, effect, count)
=================
*/
void PF_particle2 (progfuncs_t *prinst, globalvars_t *pr_globals)
{
	float		*org, *dmin, *dmax;
	float		color;
	float		count;
	float    effect;

	org = G_VECTOR(OFS_PARM0);
	dmin = G_VECTOR(OFS_PARM1);
	dmax = G_VECTOR(OFS_PARM2);
	color = G_FLOAT(OFS_PARM3);
	effect = G_FLOAT(OFS_PARM4);
	count = G_FLOAT(OFS_PARM5);

	MSG_WriteByte (&sv.multicast, svc_particle2);
	MSG_WriteCoord (&sv.multicast, org[0]);
	MSG_WriteCoord (&sv.multicast, org[1]);
	MSG_WriteCoord (&sv.multicast, org[2]);
	MSG_WriteFloat (&sv.multicast, dmin[0]);
	MSG_WriteFloat (&sv.multicast, dmin[1]);
	MSG_WriteFloat (&sv.multicast, dmin[2]);
	MSG_WriteFloat (&sv.multicast, dmax[0]);
	MSG_WriteFloat (&sv.multicast, dmax[1]);
	MSG_WriteFloat (&sv.multicast, dmax[2]);

	MSG_WriteShort (&sv.multicast, color);
	MSG_WriteByte (&sv.multicast, count);
	MSG_WriteByte (&sv.multicast, effect);

	SV_MulticastProtExt (org, MULTICAST_PVS, pr_global_struct->dimension_send, PEXT_HEXEN2, 0);
}


/*
=================
PF_particle3 - hexen2

particle(origin, box, color, effect, count)
=================
*/
void PF_particle3 (progfuncs_t *prinst, globalvars_t *pr_globals)
{
	float		*org, *box;
	float		color;
	float		count;
	float    effect;

	org = G_VECTOR(OFS_PARM0);
	box = G_VECTOR(OFS_PARM1);
	color = G_FLOAT(OFS_PARM2);
	effect = G_FLOAT(OFS_PARM3);
	count = G_FLOAT(OFS_PARM4);

	MSG_WriteByte (&sv.multicast, svc_particle3);
	MSG_WriteCoord (&sv.multicast, org[0]);
	MSG_WriteCoord (&sv.multicast, org[1]);
	MSG_WriteCoord (&sv.multicast, org[2]);
	MSG_WriteByte (&sv.multicast, box[0]);
	MSG_WriteByte (&sv.multicast, box[1]);
	MSG_WriteByte (&sv.multicast, box[2]);

	MSG_WriteShort (&sv.multicast, color);
	MSG_WriteByte (&sv.multicast, count);
	MSG_WriteByte (&sv.multicast, effect);

	SV_MulticastProtExt (org, MULTICAST_PVS, pr_global_struct->dimension_send, PEXT_HEXEN2, 0);
}

/*
=================
PF_particle4 - hexen2

particle(origin, radius, color, effect, count)
=================
*/
void PF_particle4 (progfuncs_t *prinst, globalvars_t *pr_globals)
{
	float		*org;
	float		radius;
	float		color;
	float		count;
	float    effect;

	org = G_VECTOR(OFS_PARM0);
	radius = G_FLOAT(OFS_PARM1);
	color = G_FLOAT(OFS_PARM2);
	effect = G_FLOAT(OFS_PARM3);
	count = G_FLOAT(OFS_PARM4);

	MSG_WriteByte (&sv.multicast, svc_particle4);
	MSG_WriteCoord (&sv.multicast, org[0]);
	MSG_WriteCoord (&sv.multicast, org[1]);
	MSG_WriteCoord (&sv.multicast, org[2]);
	MSG_WriteByte (&sv.multicast, radius);

	MSG_WriteShort (&sv.multicast, color);
	MSG_WriteByte (&sv.multicast, count);
	MSG_WriteByte (&sv.multicast, effect);

	SV_MulticastProtExt (org, MULTICAST_PVS, pr_global_struct->dimension_send, PEXT_HEXEN2, 0);
}

void PF_particleexplosion(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org;
	int color,radius,counter;

	org = G_VECTOR(OFS_PARM0);
	color = G_FLOAT(OFS_PARM1);
	radius = G_FLOAT(OFS_PARM2);
	counter = G_FLOAT(OFS_PARM3);
/*
	MSG_WriteByte(&sv.datagram, svc_particle_explosion);
	MSG_WriteCoord(&sv.datagram, org[0]);
	MSG_WriteCoord(&sv.datagram, org[1]);
	MSG_WriteCoord(&sv.datagram, org[2]);
	MSG_WriteShort(&sv.datagram, color);
	MSG_WriteShort(&sv.datagram, radius);
	MSG_WriteShort(&sv.datagram, counter);
*/
}

/*
=================
PF_ambientsound

=================
*/
void PF_ambientsound (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{	
	char		*samp;
	float		*pos;
	float 		vol, attenuation;
	int			i, soundnum;

	pos = G_VECTOR (OFS_PARM0);			
	samp = PR_GetStringOfs(prinst, OFS_PARM1);
	vol = G_FLOAT(OFS_PARM2);
	attenuation = G_FLOAT(OFS_PARM3);
	
// check to see if samp was properly precached
	for (soundnum=1 ; *sv.sound_precache[soundnum] ; soundnum++)
		if (!strcmp(sv.sound_precache[soundnum],samp))
			break;
			
	if (!*sv.sound_precache[soundnum])
	{
		Con_TPrintf (STL_NOPRECACHE, samp);
		return;
	}

// add an svc_spawnambient command to the level signon packet

	MSG_WriteByte (&sv.signon,svc_spawnstaticsound);
	for (i=0 ; i<3 ; i++)
		MSG_WriteCoord(&sv.signon, pos[i]);

	MSG_WriteByte (&sv.signon, soundnum);

	MSG_WriteByte (&sv.signon, vol*255);
	MSG_WriteByte (&sv.signon, attenuation*64);

}

/*
=================
PF_sound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
allready running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.

=================
*/
void PF_sound (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char		*sample;
	int			channel;
	edict_t		*entity;
	int 		volume;
	float attenuation;
		
	entity = G_EDICT(prinst, OFS_PARM0);
	channel = G_FLOAT(OFS_PARM1);
	sample = PR_GetStringOfs(prinst, OFS_PARM2);
	volume = G_FLOAT(OFS_PARM3) * 255;
	attenuation = G_FLOAT(OFS_PARM4);
	
	if (volume < 0)	//erm...
		return;

	if (volume > 255)
		volume = 255;

	SV_StartSound (entity, channel, sample, volume, attenuation);
}

//an evil one from telejano.
void PF_LocalSound(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
#ifndef SERVERONLY
	sfx_t	*sfx;

	char * s = PR_GetStringOfs(prinst, OFS_PARM0);
	float chan = G_FLOAT(OFS_PARM1);
	float vol = G_FLOAT(OFS_PARM2);

	if (sfx = S_PrecacheSound(s))
		S_StartSound(cl.playernum[0], chan, sfx, cl.simorg[0], vol, 0.0);
#endif
};

/*
=================
PF_break

break()
=================
*/
void PF_break (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
#ifdef SERVERONLY	//new break code
	char *s;

	//I would like some sort of network activity here,
	//but I don't want to mess up the sequence and stuff
	//It should be possible, but would mean that I would
	//need to alter the client, or rewrite a bit of the server..

	if (pr_globals)
		Con_TPrintf(STL_BREAKSTATEMENT);
	else if (developer.value!=2)
		return;	//non developers cann't step.
	for(;;)
	{
		s=Sys_ConsoleInput();		
		if (s)
		{
			if (!*s)
				break;
			else
				Con_Printf("%s\n", svprogfuncs->EvaluateDebugString(svprogfuncs, s));			
		}
	}
#elif defined(TEXTEDITOR)
	(pr_trace)++;
#else	//old break code
Con_Printf ("break statement\n");
*(int *)-4 = 0;	// dump to debugger
//	PR_RunError ("break statement");
#endif
}

/*
=================
PF_traceline

Used for use tracing and shot targeting
Traces are blocked by bbox and exact bsp entityes, and also slide box entities
if the tryents flag is set.

traceline (vector1, vector2, tryents)
=================
*/
void PF_traceline (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	*v1, *v2, *mins, *maxs;
	trace_t	trace;
	int		nomonsters;
	edict_t	*ent;
	int savedhull;

	v1 = G_VECTOR(OFS_PARM0);
	v2 = G_VECTOR(OFS_PARM1);
	nomonsters = G_FLOAT(OFS_PARM2);
	ent = G_EDICT(prinst, OFS_PARM3);

	if (*svprogfuncs->callargc == 6)
	{
		mins = G_VECTOR(OFS_PARM4);
		maxs = G_VECTOR(OFS_PARM5);
	}
	else
	{
		mins = vec3_origin;
		maxs = vec3_origin;
	}

	savedhull = ent->v.hull;
	ent->v.hull = 0;
	trace = SV_Move (v1, mins, maxs, v2, nomonsters, ent);
	ent->v.hull = savedhull;

	pr_global_struct->trace_allsolid = trace.allsolid;
	pr_global_struct->trace_startsolid = trace.startsolid;
	pr_global_struct->trace_fraction = trace.fraction;
	pr_global_struct->trace_inwater = trace.inwater;
	pr_global_struct->trace_inopen = trace.inopen;
//	if (trace.fraction != 1)
//		VectorMA (trace.endpos, 4, trace.plane.normal, P_VEC(trace_endpos));
//	else
		VectorCopy (trace.endpos, P_VEC(trace_endpos));
	VectorCopy (trace.plane.normal, P_VEC(trace_plane_normal));
	pr_global_struct->trace_plane_dist =  trace.plane.dist;	
	if (trace.ent)
		pr_global_struct->trace_ent = EDICT_TO_PROG(prinst, trace.ent);
	else
		pr_global_struct->trace_ent = EDICT_TO_PROG(prinst, sv.edicts);
}

void PF_traceboxh2 (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	*v1, *v2, *mins, *maxs;
	trace_t	trace;
	int		nomonsters;
	edict_t	*ent;
	int savedhull;

	v1 = G_VECTOR(OFS_PARM0);
	v2 = G_VECTOR(OFS_PARM1);
	mins = G_VECTOR(OFS_PARM2);
	maxs = G_VECTOR(OFS_PARM3);
	nomonsters = G_FLOAT(OFS_PARM4);
	ent = G_EDICT(prinst, OFS_PARM5);

	savedhull = ent->v.hull;
	ent->v.hull = 0;
	trace = SV_Move (v1, mins, maxs, v2, nomonsters, ent);
	ent->v.hull = savedhull;

	pr_global_struct->trace_allsolid = trace.allsolid;
	pr_global_struct->trace_startsolid = trace.startsolid;
	pr_global_struct->trace_fraction = trace.fraction;
	pr_global_struct->trace_inwater = trace.inwater;
	pr_global_struct->trace_inopen = trace.inopen;
//	if (trace.fraction != 1)
//		VectorMA (trace.endpos, 4, trace.plane.normal, P_VEC(trace_endpos));
//	else
		VectorCopy (trace.endpos, P_VEC(trace_endpos));
	VectorCopy (trace.plane.normal, P_VEC(trace_plane_normal));
	pr_global_struct->trace_plane_dist =  trace.plane.dist;	
	if (trace.ent)
		pr_global_struct->trace_ent = EDICT_TO_PROG(prinst, trace.ent);
	else
		pr_global_struct->trace_ent = EDICT_TO_PROG(prinst, sv.edicts);
}

void PF_traceboxdp (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	*v1, *v2, *mins, *maxs;
	trace_t	trace;
	int		nomonsters;
	edict_t	*ent;
	int savedhull;

	v1 = G_VECTOR(OFS_PARM0);
	mins = G_VECTOR(OFS_PARM1);
	maxs = G_VECTOR(OFS_PARM2);
	v2 = G_VECTOR(OFS_PARM3);
	nomonsters = G_FLOAT(OFS_PARM4);
	ent = G_EDICT(prinst, OFS_PARM5);

	savedhull = ent->v.hull;
	ent->v.hull = 0;
	trace = SV_Move (v1, mins, maxs, v2, nomonsters, ent);
	ent->v.hull = savedhull;

	pr_global_struct->trace_allsolid = trace.allsolid;
	pr_global_struct->trace_startsolid = trace.startsolid;
	pr_global_struct->trace_fraction = trace.fraction;
	pr_global_struct->trace_inwater = trace.inwater;
	pr_global_struct->trace_inopen = trace.inopen;
//	if (trace.fraction != 1)
//		VectorMA (trace.endpos, 4, trace.plane.normal, P_VEC(trace_endpos));
//	else
		VectorCopy (trace.endpos, P_VEC(trace_endpos));
	VectorCopy (trace.plane.normal, P_VEC(trace_plane_normal));
	pr_global_struct->trace_plane_dist =  trace.plane.dist;	
	if (trace.ent)
		pr_global_struct->trace_ent = EDICT_TO_PROG(prinst, trace.ent);
	else
		pr_global_struct->trace_ent = EDICT_TO_PROG(prinst, sv.edicts);
}

/*
=================
PF_checkpos

Returns true if the given entity can move to the given position from it's
current position by walking or rolling.
FIXME: make work...
scalar checkpos (entity, vector)
=================
*/
void PF_checkpos (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
}

//============================================================================

qbyte	checkpvsbuffer[MAX_MAP_LEAFS/8];
qbyte	*checkpvs;

int PF_newcheckclient (progfuncs_t *prinst, int check)
{
	int		i;
//	qbyte	*pvs;
	edict_t	*ent;
	int		leaf;
	vec3_t	org;

// cycle to the next one

	if (check < 1)
		check = 1;
	if (check > sv.allocated_client_slots)
		check = sv.allocated_client_slots;

	if (check == sv.allocated_client_slots)
		i = 1;
	else
		i = check + 1;

	for ( ;  ; i++)
	{
		if (i == sv.allocated_client_slots+1)
			i = 1;

		ent = EDICT_NUM(prinst, i);

		if (i == check)
			break;	// didn't find anything else

		if (ent->isfree)
			continue;
		if (ent->v.health <= 0)
			continue;
		if ((int)ent->v.flags & FL_NOTARGET)
			continue;

	// anything that is a client, or has a client as an enemy
		break;
	}

// get the PVS for the entity
	VectorAdd (ent->v.origin, ent->v.view_ofs, org);
	leaf = sv.worldmodel->funcs.LeafForPoint(org, sv.worldmodel);
	checkpvs = sv.worldmodel->funcs.LeafPVS (leaf, sv.worldmodel, checkpvsbuffer);

	return i;
}

/*
=================
PF_checkclient

Returns a client (or object that has a client enemy) that would be a
valid target.

If there are more than one valid options, they are cycled each frame

If (self.origin + self.viewofs) is not in the PVS of the current target,
it is not returned at all.

name checkclient ()
=================
*/
#define	MAX_CHECK	16
int c_invis, c_notvis;
void PF_checkclient (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*ent, *self;
	int		l;
	vec3_t	view;
	
// find a new check if on a new frame
	if (sv.time - sv.lastchecktime >= 0.1)
	{
		sv.lastcheck = PF_newcheckclient (prinst, sv.lastcheck);
		sv.lastchecktime = sv.time;
	}

// return check if it might be visible	
	ent = EDICT_NUM(prinst, sv.lastcheck);
	if (ent->isfree || ent->v.health <= 0)
	{
		RETURN_EDICT(prinst, sv.edicts);
		return;
	}

// if current entity can't possibly see the check entity, return 0
	self = PROG_TO_EDICT(prinst, pr_global_struct->self);
	VectorAdd (self->v.origin, self->v.view_ofs, view);
	l = sv.worldmodel->funcs.LeafForPoint(view, sv.worldmodel)-1;
	if ( (l<0) || !(checkpvs[l>>3] & (1<<(l&7)) ) )
	{
c_notvis++;
		RETURN_EDICT(prinst, sv.edicts);
		return;
	}

// might be able to see it
c_invis++;
	RETURN_EDICT(prinst, ent);
}

//============================================================================


/*
=================
PF_stuffcmd

Sends text over to the client's execution buffer

stuffcmd (clientent, value)
=================
*/
void PF_stuffcmd (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int		entnum;
	char	*str;
	client_t	*cl;
	static qboolean expectingcolour;
	
	entnum = G_EDICTNUM(prinst, OFS_PARM0);
	if (entnum < 1 || entnum > sv.allocated_client_slots)
		return;
//		PR_BIError ("Parm 0 not a client");
	str = PR_GetStringOfs(prinst, OFS_PARM1);

	cl = &svs.clients[entnum-1];

	if (strcmp(str, "disconnect\n") == 0)
	{
		// so long and thanks for all the fish
		if (cl->netchan.remote_address.type == NA_LOOPBACK)
			return;	//don't drop the local client. It looks wrong.
		cl->drop = true;
		return;
	}

	if (progstype != PROG_QW)
	{
		if (!strncmp(str, "color ", 6))	//okay, so this is a hack, but it fixes the qw scoreboard
		{
			expectingcolour = true;
			return;
		}
		if (expectingcolour)
		{
			int team = atoi(str);
			char *tname;

			expectingcolour = false;

			switch(team)
			{
			case 4:		tname = "red";	break;
			case 13:	tname = "blue";	break;
			default:	tname = va("t%i", team);	break;	//good job our va has multiple buffers
			}

			ClientReliableWrite_Begin (cl, svc_stufftext, 2+strlen("team XXXXXX\n"));
			ClientReliableWrite_String (cl, va("team %s\n", tname));

			ClientReliableWrite_Begin (cl, svc_stufftext, 2+strlen("color "));
			ClientReliableWrite_String (cl, "color ");
		}
	}

	if (cl->controller)
	{	//this is a slave client.
		//find the right number and send.
		int pnum = 0;
		client_t *sp;
		for (sp = cl->controller; sp; sp = sp->controlled)
		{
			if (sp == cl)
				break;
			pnum++;
		}
		sp = cl->controller;

		ClientReliableWrite_Begin (sp, svc_choosesplitclient, 4 + strlen(str));
		ClientReliableWrite_Byte (sp, pnum);
		ClientReliableWrite_Byte (sp, svc_stufftext);
		ClientReliableWrite_String (sp, str);
	}
	else
	{
		ClientReliableWrite_Begin (cl, svc_stufftext, 2+strlen(str));
		ClientReliableWrite_String (cl, str);
	}
}

/*
=================
PF_localcmd

Sends text over to the client's execution buffer

localcmd (string)
=================
*/
void PF_localcmd (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*str;
	
	str = PR_GetStringOfs(prinst, OFS_PARM0);	
	if (!strcmp(str, "host_framerate 0\n"))
		Cbuf_AddText ("sv_mintic 0\n", RESTRICT_RCON);	//hmm... do this better...
	else
		Cbuf_AddText (str, RESTRICT_RCON);
}

/*
=================
PF_cvar

float cvar (string)
=================
*/
void PF_cvar (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*str;
	
	str = PR_GetStringOfs(prinst, OFS_PARM0);

	if (!strcmp(str, "pr_checkextension"))	//no console changing
		G_FLOAT(OFS_RETURN) = PR_EnableEBFSBuiltin("checkextension");
	else if (!strcmp(str, "pr_builtin_find"))
		G_FLOAT(OFS_RETURN) = PR_EnableEBFSBuiltin("builtin_find");
	else if (!strcmp(str, "halflifebsp"))
		G_FLOAT(OFS_RETURN) = sv.worldmodel->fromgame == fg_halflife;
	else
		G_FLOAT(OFS_RETURN) = Cvar_VariableValue (str);
}

void PF_cvar_string (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*str;
	
	str = PR_GetStringOfs(prinst, OFS_PARM0);
	
	RETURN_CSTRING(Cvar_VariableString (str));
}

/*
=================
PF_cvar_set

float cvar (string)
=================
*/
void PF_cvar_set (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*var_name, *val;
	cvar_t *var;
	
	var_name = PR_GetStringOfs(prinst, OFS_PARM0);
	val = PR_GetStringOfs(prinst, OFS_PARM1);

	var = Cvar_FindVar(var_name);
	if (!var)
		Con_Printf("PF_cvar_set: variable %s not found\n", var_name);
	else
		Cvar_Set (var, val);
}

void PF_cvar_setf (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*var_name;
	float	val;
	cvar_t *var;
	
	var_name = PR_GetStringOfs(prinst, OFS_PARM0);
	val = G_FLOAT(OFS_PARM1);

	var = Cvar_FindVar(var_name);
	if (!var)
		Con_Printf("PF_cvar_set: variable %s not found\n", var_name);
	else
		Cvar_SetValue (var, val);
}

/*
=================
PF_registercvar

float registercvar (string name, string value)
=================
*/
void PF_registercvar (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *name, *value;
	value = PR_GetStringOfs(prinst, OFS_PARM0); 

	if (Cvar_FindVar(value))
		G_FLOAT(OFS_RETURN) = 0;
	else
	{
		name = BZ_Malloc(strlen(value)+1);
		strcpy(name, value);
		if (*prinst->callargc > 1)
			value = PR_GetStringOfs(prinst, OFS_PARM1);
		else
			value = "";

	// archive?
		if (Cvar_Get(name, value, CVAR_USERCREATED, "QC created vars"))
			G_FLOAT(OFS_RETURN) = 1;
		else
			G_FLOAT(OFS_RETURN) = 0;
	}
}

/*
=================
PF_findradius

Returns a chain of entities that have origins within a spherical area

findradius (origin, radius)
=================
*/
void PF_findradius (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*ent, *chain;
	float	rad;
	float	*org;
	vec3_t	eorg;
	int		i, j;

	chain = (edict_t *)sv.edicts;

	org = G_VECTOR(OFS_PARM0);
	rad = G_FLOAT(OFS_PARM1);

	for (i=1 ; i<sv.num_edicts ; i++)
	{
		ent = EDICT_NUM(svprogfuncs, i);
		if (ent->isfree)
			continue;
		if (ent->v.solid == SOLID_NOT)
			continue;
		for (j=0 ; j<3 ; j++)
			eorg[j] = org[j] - (ent->v.origin[j] + (ent->v.mins[j] + ent->v.maxs[j])*0.5);			
		if (Length(eorg) > rad)
			continue;
			
		ent->v.chain = EDICT_TO_PROG(prinst, chain);
		chain = ent;
	}

	RETURN_EDICT(prinst, chain);
}


/*
=========
PF_dprint
=========
*/
void PF_dprint (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Con_Printf ("%s",PF_VarString(prinst, 0, pr_globals));
}

/*
=========
PF_conprint
=========
*/
void PF_conprint (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Sys_Printf ("%s",PF_VarString(prinst, 0, pr_globals));
}


void PF_dprintf (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char temp[256];
	float	v;

	v = G_FLOAT(OFS_PARM1);

	if (v == (int)v)
		sprintf (temp, "%d",(int)v);
	else
		sprintf (temp, "%5.1f",v);

	Con_Printf (PR_GetStringOfs(prinst, OFS_PARM0),temp);
}

void PF_dprintv (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char temp[256];

	sprintf (temp, "'%5.1f %5.1f %5.1f'", G_VECTOR(OFS_PARM1)[0], G_VECTOR(OFS_PARM1)[1], G_VECTOR(OFS_PARM1)[2]);

	Con_Printf (PR_GetStringOfs(prinst, OFS_PARM0),temp);
}

#define MAX_TEMPSTRS	16
#define MAXTEMPBUFFERLEN	1024
static char *PF_TempStr(void)
{
	static char	pr_string_temparr[MAX_TEMPSTRS][MAXTEMPBUFFERLEN];
	static int tempbuffernum;
	if (tempbuffernum == MAX_TEMPSTRS)
		tempbuffernum = 0;
	return pr_string_temparr[tempbuffernum++];
}

void PF_ftos (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	v;
	char *pr_string_temp = PF_TempStr();
	v = G_FLOAT(OFS_PARM0);
	
	if (v == (int)v)
		sprintf (pr_string_temp, "%d",(int)v);
	else
		sprintf (pr_string_temp, "%5.1f",v);
	RETURN_TSTRING(pr_string_temp);
}
void PF_ftosp(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	v;
	int num;
	char *pr_string_temp = PF_TempStr();
	v = G_FLOAT(OFS_PARM0);
	num = G_FLOAT(OFS_PARM1);

	switch(num)
	{
	case 1:
		sprintf(pr_string_temp, "%1f", v);
		break;
	case 2:
		sprintf(pr_string_temp, "%2f", v);
		break;
	case 3:
		sprintf(pr_string_temp, "%3f", v);
		break;
	case 4:
		sprintf(pr_string_temp, "%4f", v);
		break;
	default:
		sprintf(pr_string_temp, "%f", v);
		break;
	}

	RETURN_TSTRING(pr_string_temp);
}

void PF_fabs (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	v;
	v = G_FLOAT(OFS_PARM0);
	G_FLOAT(OFS_RETURN) = fabs(v);
}

void PF_vtos (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *pr_string_temp = PF_TempStr();
	sprintf (pr_string_temp, "'%5.1f %5.1f %5.1f'", G_VECTOR(OFS_PARM0)[0], G_VECTOR(OFS_PARM0)[1], G_VECTOR(OFS_PARM0)[2]);
	RETURN_TSTRING(pr_string_temp);
}

void PF_Spawn (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*ed;
	ed = ED_Alloc(prinst);
	RETURN_EDICT(prinst, ed);
}

void PF_spawn_temp (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*ed;
	ed = ED_Alloc(prinst);
	RETURN_EDICT(prinst, ed);
}

void PF_Remove (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*ed;
	
	ed = G_EDICT(prinst, OFS_PARM0);

	if (ed->isfree && progstype == PROG_H2)
	{
		Con_DPrintf("Tried removing free entity\n");
		return;	//yeah, alright, so this is hacky.
	}

	ED_Free (prinst, ed);
}


// entity (entity start, .string field, string match) find = #5;
void PF_Find (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int		e;	
	int		f;
	char	*s, *t;
	edict_t	*ed;
	
	e = G_EDICTNUM(prinst, OFS_PARM0);
	f = G_INT(OFS_PARM1)+prinst->fieldadjust;
	s = PR_GetStringOfs(prinst, OFS_PARM2);
	if (!s)
	{
		PR_BIError ("PF_Find: bad search string");
		return;
	}
		
	for (e++ ; e < sv.num_edicts ; e++)
	{
		ed = EDICT_NUM(prinst, e);
		if (ed->isfree)
			continue;
		t = *(string_t *)&((float*)&ed->v)[f];
//		t = E_STRING(f);
		if (!t)
			continue;
		if (!strcmp(t+prinst->stringtable,s))
		{
			RETURN_EDICT(prinst, ed);
			return;
		}
	}
	
	RETURN_EDICT(prinst, sv.edicts);
}
/*
void PR_CheckEmptyString (char *s)
{
	if (s[0] <= ' ')
		PR_RunError ("Bad string");
}
*/
void PF_precache_file (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{	// precache_file is only used to copy files with qcc, it does nothing
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
}

void PF_precache_sound (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*s;
	int		i;
	
	if (sv.state != ss_loading)
	{
		PR_BIError ("PF_Precache_*: Precache can only be done in spawn functions");
		return;
	}

	s = PR_GetStringOfs(prinst, OFS_PARM0);
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);

	if (s[0] <= ' ')
	{
		PR_BIError ("Bad string");	
		return;
	}
	
	for (i=1 ; i<MAX_SOUNDS ; i++)
	{
		if (!*sv.sound_precache[i])
		{
			strcpy(sv.sound_precache[i], s);
			return;
		}
		if (!strcmp(sv.sound_precache[i], s))
			return;
	}
	PR_BIError ("PF_precache_sound: overflow");
}

void PF_precache_model (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*s;
	int		i;
	
	if (sv.state != ss_loading)
	{
		PR_BIError ("PF_Precache_*: Precache can only be done in spawn functions");
		G_FLOAT(OFS_RETURN) = 1;
		return;
	}
		
	s = PR_GetStringOfs(prinst, OFS_PARM0);
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);

	if (s[0] <= ' ')
	{
		PR_BIError ("Bad string");
		return;
	}

	for (i=1 ; i<MAX_MODELS ; i++)
	{
		if (!*sv.model_precache[i])
		{
			if (strlen(s)>=sizeof(sv.model_precache[i])-1)
				PR_BIError ("Precache name too long");
			strcpy(sv.model_precache[i], s);
			if (!strcmp(s + strlen(s) - 4, ".bsp"))
				sv.models[i] = Mod_FindName(sv.model_precache[i]);

			return;
		}
		if (!strcmp(sv.model_precache[i], s))
		{
			return;
		}
	}
	PR_BIError ("PF_precache_model: overflow");
}

void PF_precache_puzzle_model (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{	//qc/hc lacks string manipulation.
	char *shortname;
	char fullname[MAX_QPATH];
	shortname = PR_GetStringOfs(prinst, OFS_PARM0);
	_snprintf(fullname, sizeof(fullname)-1, "models/puzzle/%s.mdl", shortname);
	G_INT(OFS_PARM0) = (int)(fullname - prinst->stringtable);
	PF_precache_model (prinst, pr_globals);
	G_INT(OFS_PARM0) = (int)(shortname - prinst->stringtable);	//piece of mind.
}

void PF_WeapIndex (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*s;
	int		i;

	s = PR_GetStringOfs(prinst, OFS_PARM0);

	G_FLOAT(OFS_RETURN) = 1;

	if (s[0] <= ' ')
	{
		PR_BIError ("Bad string");
		return;
	}

	for (i=1 ; i<MAX_MODELS ; i++)
	{
		if (!*sv.model_precache[i])
		{
			if (sv.state != ss_loading)	//allow it to be used to find a model too.
			{
				PR_BIError ("PF_Precache_*: Precache can only be done in spawn functions");
				return;
			}

			strcpy(sv.model_precache[i], s);
			if (!strcmp(s + strlen(s) - 4, ".bsp"))
				sv.models[i] = Mod_FindName(sv.model_precache[i]);

			G_FLOAT(OFS_RETURN) = i;
			return;
		}
		if (!strcmp(sv.model_precache[i], s))
		{
			G_FLOAT(OFS_RETURN) = i;
			return;
		}
	}
	PR_BIError ("PF_precache_model: overflow");
}


void PF_coredump (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
//	ED_PrintEdicts ();
}

void PF_traceon (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	pr_trace = true;
}

void PF_traceoff (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	pr_trace = false;
}

void PF_eprint (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char buffer[8192];
	char *buf;
	int size = sizeof(buffer);
	buf = prinst->saveent(prinst, buffer, &size, G_EDICT(prinst, OFS_PARM0));
	Con_Printf("Entity %i:\n%s\n", G_EDICTNUM(prinst, OFS_PARM0), buf);
}

/*
===============
PF_walkmove

float(float yaw, float dist) walkmove
===============
*/
void PF_walkmove (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*ent;
	float	yaw, dist;
	vec3_t	move;
//	dfunction_t	*oldf;
	int 	oldself;
	struct globalvars_s *settrace;
	
	ent = PROG_TO_EDICT(prinst, pr_global_struct->self);
	yaw = G_FLOAT(OFS_PARM0);
	dist = G_FLOAT(OFS_PARM1);
	if (*svprogfuncs->callargc >= 3 && G_FLOAT(OFS_PARM2))
		settrace = pr_globals;
	else
		settrace = NULL;
	
	if ( !( (int)ent->v.flags & (FL_ONGROUND|FL_FLY|FL_SWIM) ) )
	{
		G_FLOAT(OFS_RETURN) = 0;
		return;
	}

	yaw = yaw*M_PI*2 / 360;
	
	move[0] = cos(yaw)*dist;
	move[1] = sin(yaw)*dist;
	move[2] = 0;

// save program state, because SV_movestep may call other progs
//	oldf = pr_xfunction;
	oldself = pr_global_struct->self;
	
	G_FLOAT(OFS_RETURN) = SV_movestep(ent, move, true, false, settrace);
	
	
// restore program state
//	pr_xfunction = oldf;
	pr_global_struct->self = oldself;
}

/*
===============
PF_droptofloor

void() droptofloor
===============
*/
void PF_droptofloor (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t		*ent;
	vec3_t		end;
	vec3_t		start;
	trace_t		trace;
	
	ent = PROG_TO_EDICT(prinst, pr_global_struct->self);

	VectorCopy (ent->v.origin, end);
	end[2] -= 512;

	VectorCopy (ent->v.origin, start);
	trace = SV_Move (start, ent->v.mins, ent->v.maxs, end, false, ent);

	if (trace.fraction == 1 || trace.allsolid)
		G_FLOAT(OFS_RETURN) = 0;
	else
	{
		VectorCopy (trace.endpos, ent->v.origin);
		SV_LinkEdict (ent, false);
//		ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
		ent->v.groundentity = EDICT_TO_PROG(prinst, trace.ent);
		G_FLOAT(OFS_RETURN) = 1;
	}
}

/*
===============
PF_lightstyle

void(float style, string value) lightstyle
===============
*/
void PF_lightstyle (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int		style;
	char	*val;
	client_t	*client;
	int			j;

#ifdef PEXT_LIGHTSTYLECOL
	int col;
	if (*svprogfuncs->callargc >= 3)
	{
		col = G_FLOAT(OFS_PARM2);
		if (IS_NAN(col) || !col || col > 0x111)
			col = 7;
	}
	else col = 7;
#endif
	
	style = G_FLOAT(OFS_PARM0);
	val = PR_GetStringOfs(prinst, OFS_PARM1);

	if (style < 0 || style >= MAX_LIGHTSTYLES)
	{
		Con_Printf("WARNING: Bad lightstyle %i.\n", style);
		return;
	}
	if (strlen(val) > MAX_STYLESTRING-1)
		Con_Printf("WARNING: Style string is longer than standard (%i). Some clients could crash.\n", MAX_STYLESTRING-1);


// change the string in sv
	if (sv.lightstyles[style])
		Z_Free(sv.lightstyles[style]);
	sv.lightstyles[style] = Z_Malloc(strlen(val)+1);
	strcpy(sv.lightstyles[style], val);
//	sv.lightstyles[style] = val;
#ifdef PEXT_LIGHTSTYLECOL
	sv.lightstylecolours[style] = col;
#endif
	
// send message to all clients on this server
	if (sv.state != ss_active)
		return;
	
	for (j=0, client = svs.clients ; j<sv.allocated_client_slots ; j++, client++)
	{
		if (client->controller)
			continue;
		if ( client->state == cs_spawned )
		{
#ifdef PEXT_LIGHTSTYLECOL
			if (client->fteprotocolextensions & PEXT_LIGHTSTYLECOL && col!=7)
			{
				ClientReliableWrite_Begin (client, svc_lightstylecol, strlen(val)+4);
				ClientReliableWrite_Char (client, style);
				ClientReliableWrite_Char (client, col);
				ClientReliableWrite_String (client, val);
			}
			else
			{
#endif
				ClientReliableWrite_Begin (client, svc_lightstyle, strlen(val)+3);
				ClientReliableWrite_Char (client, style);
				ClientReliableWrite_String (client, val);
#ifdef PEXT_LIGHTSTYLECOL			
			}
#endif
		}
	}
}

void PF_lightstylevalue (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int style;
	style = G_FLOAT(OFS_PARM0);
	if(style < 0 || style >= MAX_LIGHTSTYLES || !sv.lightstyles[style])
	{
		G_FLOAT(OFS_RETURN) = 0;
		return;
	}
	G_FLOAT(OFS_RETURN) = *sv.lightstyles[style] - 'a';
}

void PF_lightstylestatic (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int		style;
	float	num;
	char	*val;
	client_t	*client;
	int			j;

	static char *styleDefs[] =
	{
		"a", "b", "c", "d", "e", "f", "g",
		"h", "i", "j", "k", "l", "m", "n",
		"o", "p", "q", "r", "s", "t", "u",
		"v", "w", "x", "y", "z"
	};

#ifdef PEXT_LIGHTSTYLECOL
	int col;
	if (*svprogfuncs->callargc >= 3)
	{
		col = G_FLOAT(OFS_PARM2);
		if (IS_NAN(col) || !col || col > 0x111)
			col = 7;
	}
	else col = 7;
#endif
	
	style = G_FLOAT(OFS_PARM0);
	num = G_FLOAT(OFS_PARM1);
	if (num < 0)
		num = 0;
	else if (num >= 'z'-'a')
		num = 'z'-'a'-1;
	val = styleDefs[(int)num];


	if (style < 0 || style >= MAX_LIGHTSTYLES)
	{
		Con_Printf("WARNING: Bad lightstyle %i.\n", style);
		return;
	}
	if (strlen(val) > MAX_STYLESTRING-1)
		Con_Printf("WARNING: Style string is longer than standard (%i). Some clients could crash.\n", MAX_STYLESTRING-1);


// change the string in sv
	if (sv.lightstyles[style])
		Z_Free(sv.lightstyles[style]);
	sv.lightstyles[style] = Z_Malloc(strlen(val)+1);
	strcpy(sv.lightstyles[style], val);
//	sv.lightstyles[style] = val;
#ifdef PEXT_LIGHTSTYLECOL
	sv.lightstylecolours[style] = col;
#endif
	
// send message to all clients on this server
	if (sv.state != ss_active)
		return;
	
	for (j=0, client = svs.clients ; j<sv.allocated_client_slots ; j++, client++)
	{
		if (client->controller)
			continue;

		if ( client->state == cs_spawned )
		{
#ifdef PEXT_LIGHTSTYLECOL
			if (client->fteprotocolextensions & PEXT_LIGHTSTYLECOL && col!=7)
			{
				ClientReliableWrite_Begin (client, svc_lightstylecol, strlen(val)+4);
				ClientReliableWrite_Char (client, style);
				ClientReliableWrite_Char (client, col);
				ClientReliableWrite_String (client, val);
			}
			else
			{
#endif
				ClientReliableWrite_Begin (client, svc_lightstyle, strlen(val)+3);
				ClientReliableWrite_Char (client, style);
				ClientReliableWrite_String (client, val);
#ifdef PEXT_LIGHTSTYLECOL			
			}
#endif
		}
	}
}

void PF_rint (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	f;
	f = G_FLOAT(OFS_PARM0);
	if (f > 0)
		G_FLOAT(OFS_RETURN) = (int)(f + 0.5);
	else
		G_FLOAT(OFS_RETURN) = (int)(f - 0.5);
}
void PF_floor (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = floor(G_FLOAT(OFS_PARM0));
}
void PF_ceil (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = ceil(G_FLOAT(OFS_PARM0));
}


/*
=============
PF_checkbottom
=============
*/
void PF_checkbottom (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*ent;
	
	ent = G_EDICT(prinst, OFS_PARM0);

	G_FLOAT(OFS_RETURN) = SV_CheckBottom (ent);
}

/*
=============
PF_pointcontents
=============
*/
void PF_pointcontents (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	*v;
	int cont;
	
	v = G_VECTOR(OFS_PARM0);

	cont = SV_PointContents (v);
	if (cont & FTECONTENTS_SOLID)
		G_FLOAT(OFS_RETURN) = Q1CONTENTS_SOLID;
	else if (cont & FTECONTENTS_SKY)
		G_FLOAT(OFS_RETURN) = Q1CONTENTS_SKY;
	else if (cont & FTECONTENTS_LAVA)
		G_FLOAT(OFS_RETURN) = Q1CONTENTS_LAVA;
	else if (cont & FTECONTENTS_SLIME)
		G_FLOAT(OFS_RETURN) = Q1CONTENTS_SLIME;
	else if (cont & FTECONTENTS_WATER)
		G_FLOAT(OFS_RETURN) = Q1CONTENTS_WATER;
	else
		G_FLOAT(OFS_RETURN) = Q1CONTENTS_EMPTY;
}

/*
=============
PF_nextent

entity nextent(entity)
=============
*/
void PF_nextent (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int		i;
	edict_t	*ent;
	
	i = G_EDICTNUM(prinst, OFS_PARM0);
	while (1)
	{
		i++;
		if (i == sv.num_edicts)
		{
			RETURN_EDICT(prinst, sv.edicts);
			return;
		}
/*		if (i <= MAX_CLIENTS)
		{
			if (!svs.clients[i-1].state)
				continue;
		}*/
		ent = EDICT_NUM(prinst, i);
		if (!ent->isfree)
		{
			RETURN_EDICT(prinst, ent);
			return;
		}
	}
}

/*
=============
PF_aim

Pick a vector for the player to shoot along
vector aim(entity, missilespeed)
=============
*/
//cvar_t	sv_aim = {"sv_aim", "0.93"};
cvar_t	sv_aim = {"sv_aim", "2"};
void PF_aim (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*ent, *check, *bestent;
	vec3_t	start, dir, end, bestdir;
	int		i, j;
	trace_t	tr;
	float	dist, bestdist;
	float	speed;
	char	*noaim;

	ent = G_EDICT(prinst, OFS_PARM0);
	speed = G_FLOAT(OFS_PARM1);

	VectorCopy (ent->v.origin, start);
	start[2] += 20;

// noaim option
	i = NUM_FOR_EDICT(prinst, ent);
	if (i>0 && i<sv.allocated_client_slots)
	{
		noaim = Info_ValueForKey (svs.clients[i-1].userinfo, "noaim");
		if (atoi(noaim) > 0)
		{			
			VectorCopy (P_VEC(v_forward), G_VECTOR(OFS_RETURN));
			return;
		}
	}

// try sending a trace straight
	VectorCopy (P_VEC(v_forward), dir);
	VectorMA (start, 2048, dir, end);
	tr = SV_Move (start, vec3_origin, vec3_origin, end, false, ent);
	if (tr.ent && tr.ent->v.takedamage == DAMAGE_AIM
	&& (!teamplay.value || ent->v.team <=0 || ent->v.team != tr.ent->v.team) )
	{
		VectorCopy (P_VEC(v_forward), G_VECTOR(OFS_RETURN));
		return;
	}


// try all possible entities
	VectorCopy (dir, bestdir);
	bestdist = sv_aim.value;
	bestent = NULL;

	for (i=1 ; i<sv.num_edicts ; i++ )
	{
		check = EDICT_NUM(prinst, i);
		if (check->v.takedamage != DAMAGE_AIM)
			continue;
		if (check == ent)
			continue;
		if (teamplay.value && ent->v.team > 0 && ent->v.team == check->v.team)
			continue;	// don't aim at teammate
		for (j=0 ; j<3 ; j++)
			end[j] = check->v.origin[j]
			+ 0.5*(check->v.mins[j] + check->v.maxs[j]);
		VectorSubtract (end, start, dir);
		VectorNormalize (dir);
		dist = DotProduct (dir, P_VEC(v_forward));
		if (dist < bestdist)
			continue;	// to far to turn
		tr = SV_Move (start, vec3_origin, vec3_origin, end, false, ent);
		if (tr.ent == check)
		{	// can shoot at this one
			bestdist = dist;
			bestent = check;
		}
	}
	
	if (bestent)
	{
		VectorSubtract (bestent->v.origin, ent->v.origin, dir);
		dist = DotProduct (dir, P_VEC(v_forward));
		VectorScale (P_VEC(v_forward), dist, end);
		end[2] = dir[2];
		VectorNormalize (end);
		VectorCopy (end, G_VECTOR(OFS_RETURN));	
	}
	else
	{
		VectorCopy (bestdir, G_VECTOR(OFS_RETURN));
	}
}

/*
==============
PF_changeyaw

This was a major timewaster in progs, so it was converted to C
==============
*/
void PF_changeyaw (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t		*ent;
	float		ideal, current, move, speed;
	
	ent = PROG_TO_EDICT(prinst, pr_global_struct->self);
	current = anglemod( ent->v.angles[1] );
	ideal = ent->v.ideal_yaw;
	speed = ent->v.yaw_speed;
	
	if (current == ideal)
		return;
	move = ideal - current;
	if (ideal > current)
	{
		if (move >= 180)
			move = move - 360;
	}
	else
	{
		if (move <= -180)
			move = move + 360;
	}
	if (move > 0)
	{
		if (move > speed)
			move = speed;
	}
	else
	{
		if (move < -speed)
			move = -speed;
	}
	
	ent->v.angles[1] = anglemod (current + move);
}

/*
===============================================================================

MESSAGE WRITING

===============================================================================
*/

#define	MSG_BROADCAST	0		// unreliable to all
#define	MSG_ONE			1		// reliable to one (msg_entity)
#define	MSG_ALL			2		// reliable to all
#define	MSG_INIT		3		// write to the init string
#define	MSG_MULTICAST	4		// for multicast()

sizebuf_t *WriteDest (int		dest)
{
	switch (dest)
	{
	case MSG_BROADCAST:
		return &sv.datagram;
	
	case MSG_ONE:
		SV_Error("Shouldn't be at MSG_ONE");
#if 0
		ent = PROG_TO_EDICT(pr_global_struct->msg_entity);
		entnum = NUM_FOR_EDICT(ent);
		if (entnum < 1 || entnum > MAX_CLIENTS)
		{
			PR_BIError ("WriteDest: not a client");
			return &sv.reliable_datagram;
		}
		return &svs.clients[entnum-1].netchan.message;
#endif
		
	case MSG_ALL:
		return &sv.reliable_datagram;
	
	case MSG_INIT:
		if (sv.state != ss_loading)
		{
			PR_BIError ("PF_Write_*: MSG_INIT can only be written in spawn functions");
			return NULL;
		}
		return &sv.signon;

	case MSG_MULTICAST:
		return &sv.multicast;

	default:
		PR_BIError ("WriteDest: bad destination");
		break;
	}
	
	return NULL;
}
#ifdef NQPROT
sizebuf_t *NQWriteDest (int dest)
{
	switch (dest)
	{
	case MSG_BROADCAST:
		return &sv.nqdatagram;
	
	case MSG_ONE:
		SV_Error("Shouldn't be at MSG_ONE");
#if 0
		ent = PROG_TO_EDICT(pr_global_struct->msg_entity);
		entnum = NUM_FOR_EDICT(ent);
		if (entnum < 1 || entnum > MAX_CLIENTS)
		{
			PR_BIError ("WriteDest: not a client");
			return &sv.nqreliable_datagram;
		}
		return &svs.clients[entnum-1].netchan.message;
#endif
		
	case MSG_ALL:
		return &sv.nqreliable_datagram;
	
	case MSG_INIT:
		if (sv.state != ss_loading)
		{
			PR_BIError ("PF_Write_*: MSG_INIT can only be written in spawn functions");
			return NULL;
		}
		return &sv.signon;

	case MSG_MULTICAST:
		return &sv.nqmulticast;

	default:
		PR_BIError ("WriteDest: bad destination");
		break;
	}
	
	return NULL;
}
#endif

struct globalvars_s *pr_netglob;
progfuncs_t *pr_netprogfuncs;
client_t *Write_GetClient(void)
{
	int		entnum;
	edict_t	*ent;

	struct globalvars_s *pr_globals = pr_netglob;

	ent = PROG_TO_EDICT(pr_netprogfuncs, pr_global_struct->msg_entity);
	entnum = NUM_FOR_EDICT(pr_netprogfuncs, ent);
	if (entnum < 1 || entnum > sv.allocated_client_slots)
		return NULL;//PR_RunError ("WriteDest: not a client");
	return &svs.clients[entnum-1];
}

void PF_WriteByte (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (qc_nonetaccess.value || sv.demofile)
		return;

	pr_netprogfuncs = prinst;
	pr_netglob = pr_globals;
	if (progstype == PROG_NQ || progstype == PROG_H2)
	{
		NPP_NQWriteByte(G_FLOAT(OFS_PARM0), (qbyte)G_FLOAT(OFS_PARM1));
		return;
	}
#ifdef NQPROT
	else
	{
		NPP_QWWriteByte(G_FLOAT(OFS_PARM0), (qbyte)G_FLOAT(OFS_PARM1));
		return;
	}
#else
	else if (G_FLOAT(OFS_PARM0) == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		if (!cl)
			return;
		ClientReliableCheckBlock(cl, 1);
		ClientReliableWrite_Byte(cl, G_FLOAT(OFS_PARM1));
	}
	else
		MSG_WriteByte (WriteDest(G_FLOAT(OFS_PARM0)), G_FLOAT(OFS_PARM1));
#endif
}

void PF_WriteChar (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (qc_nonetaccess.value || sv.demofile)
		return;
	pr_netprogfuncs = prinst;
	pr_netglob = pr_globals;
	if (progstype == PROG_NQ || progstype == PROG_H2)
	{
		NPP_NQWriteChar(G_FLOAT(OFS_PARM0), (char)G_FLOAT(OFS_PARM1));
		return;
	}
#ifdef NQPROT
	else
	{
		NPP_QWWriteChar(G_FLOAT(OFS_PARM0), (char)G_FLOAT(OFS_PARM1));
		return;
	}
#else
	if (G_FLOAT(OFS_PARM0) == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		if (!cl)
			return;
		ClientReliableCheckBlock(cl, 1);
		ClientReliableWrite_Char(cl, G_FLOAT(OFS_PARM1));
	}
	else
		MSG_WriteChar (WriteDest(G_FLOAT(OFS_PARM0)), G_FLOAT(OFS_PARM1));
#endif
}

void PF_WriteShort (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (qc_nonetaccess.value || sv.demofile)
		return;

	pr_netprogfuncs = prinst;
	pr_netglob = pr_globals;
	if (progstype == PROG_NQ || progstype == PROG_H2)
	{
		NPP_NQWriteShort(G_FLOAT(OFS_PARM0), (short)G_FLOAT(OFS_PARM1));
		return;
	}
#ifdef NQPROT
	else
	{
		NPP_QWWriteShort(G_FLOAT(OFS_PARM0), (short)G_FLOAT(OFS_PARM1));
		return;
	}
#else
	if (G_FLOAT(OFS_PARM0) == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		if (!cl)
			return;
		ClientReliableCheckBlock(cl, 2);
		ClientReliableWrite_Short(cl, G_FLOAT(OFS_PARM1));
	}
	else
		MSG_WriteShort (WriteDest(G_FLOAT(OFS_PARM0)), G_FLOAT(OFS_PARM1));
#endif
}

void PF_WriteLong (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (qc_nonetaccess.value || sv.demofile)
		return;

	pr_netprogfuncs = prinst;
	pr_netglob = pr_globals;
	if (progstype == PROG_NQ || progstype == PROG_H2)
	{
		NPP_NQWriteLong(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
		return;
	}
#ifdef NQPROT
	else
	{
		NPP_QWWriteLong(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
		return;
	}
#else
	if (G_FLOAT(OFS_PARM0) == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		if (!cl)
			return;
		ClientReliableCheckBlock(cl, 4);
		ClientReliableWrite_Long(cl, G_FLOAT(OFS_PARM1));
	}
	else
		MSG_WriteLong (WriteDest(G_FLOAT(OFS_PARM0)), G_FLOAT(OFS_PARM1));
#endif
}

void PF_WriteAngle (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (qc_nonetaccess.value || sv.demofile)
		return;
	pr_netprogfuncs = prinst;
	pr_netglob = pr_globals;
	if (progstype == PROG_NQ || progstype == PROG_H2)
	{
		NPP_NQWriteAngle(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
		return;
	}
#ifdef NQPROT
	else
	{
		NPP_QWWriteAngle(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
		return;
	}
#else
	if (G_FLOAT(OFS_PARM0) == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		if (!cl)
			return;
		ClientReliableCheckBlock(cl, 1);
		ClientReliableWrite_Angle(cl, G_FLOAT(OFS_PARM1));
	}
	else
		MSG_WriteAngle (WriteDest(G_FLOAT(OFS_PARM0)), G_FLOAT(OFS_PARM1));
#endif
}

void PF_WriteCoord (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (qc_nonetaccess.value || sv.demofile)
		return;

	pr_netprogfuncs = prinst;
	pr_netglob = pr_globals;
	if (progstype == PROG_NQ || progstype == PROG_H2)
	{
		NPP_NQWriteCoord(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
		return;
	}
#ifdef NQPROT
	else
	{
		NPP_QWWriteCoord(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
		return;
	}
#else
	if (G_FLOAT(OFS_PARM0) == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		if (!cl)
			return;
		ClientReliableCheckBlock(cl, 2);
		ClientReliableWrite_Coord(cl, G_FLOAT(OFS_PARM1));
	}
	else
		MSG_WriteCoord (WriteDest(G_FLOAT(OFS_PARM0)), G_FLOAT(OFS_PARM1));
#endif
}

void PF_WriteString (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *str;
	if (qc_nonetaccess.value || sv.demofile)
		return;
	str = PF_VarString(prinst, 1, pr_globals);

	pr_netprogfuncs = prinst;
	pr_netglob = pr_globals;
	if (progstype == PROG_NQ || progstype == PROG_H2)
	{
		NPP_NQWriteString(G_FLOAT(OFS_PARM0), str);
		return;
	}
#ifdef NQPROT
	else
	{
		NPP_QWWriteString(G_FLOAT(OFS_PARM0), str);
		return;
	}
#else
	if (G_FLOAT(OFS_PARM0) == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		if (!cl)
			return;
		ClientReliableCheckBlock(cl, 1+strlen(str));
		ClientReliableWrite_String(cl, str);
	}
	else
		MSG_WriteString (WriteDest(G_FLOAT(OFS_PARM0)), str);
#endif
}


void PF_WriteEntity (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (qc_nonetaccess.value || sv.demofile)
		return;

	pr_netprogfuncs = prinst;
	pr_netglob = pr_globals;
	if (progstype == PROG_NQ || progstype == PROG_H2)
	{
		NPP_NQWriteEntity(G_FLOAT(OFS_PARM0), (short)G_EDICTNUM(prinst, OFS_PARM1));
		return;
	}
#ifdef NQPROT
	else
	{
		NPP_QWWriteEntity(G_FLOAT(OFS_PARM0), (short)G_EDICTNUM(prinst, OFS_PARM1));
		return;
	}
#else
	if (G_FLOAT(OFS_PARM0) == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		if (!cl)
			return;
		ClientReliableCheckBlock(cl, 2);
		ClientReliableWrite_Short(cl, G_EDICTNUM(prinst, OFS_PARM1));
	}
	else
		MSG_WriteShort (WriteDest(G_FLOAT(OFS_PARM0)), G_EDICTNUM(prinst, OFS_PARM1));
#endif
}

//small wrapper function.
//void(float target, string str, ...) WriteString2 = #33;
void PF_WriteString2 (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int old;
	char *str;
	if (qc_nonetaccess.value || sv.demofile)
		return;

	str = PF_VarString(prinst, 1, pr_globals);

	old = G_FLOAT(OFS_PARM1);
	while(*str)
	{
		G_FLOAT(OFS_PARM1) = *str++;
		PF_WriteChar(prinst, pr_globals);
	}
	G_FLOAT(OFS_PARM1) = old;
}


//======================================================

//copes with any qw point entities.
void SV_point_tempentity (vec3_t o, int type, int count)	//count (usually 1) is available for some tent types.
{
	int split=0;	

	if (sv.demofile)
		return;
		

	if (type > TE_SUPERBULLET)	//pick a new effect, cos this one we don't know about.
		type = TE_SPIKE;

//this is for lamers with old (or unsupported) clients
	MSG_WriteByte (&sv.multicast, svc_temp_entity);
#ifdef NQPROT
	MSG_WriteByte (&sv.nqmulticast, svc_temp_entity);
#endif
	switch(type)
	{
	case TE_BULLET:
		MSG_WriteByte (&sv.multicast, TE_SPIKE);
#ifdef NQPROT
		MSG_WriteByte (&sv.nqmulticast, TE_SPIKE);
#endif
		type = TE_BULLET;
		split = PEXT_TE_BULLET;
		break;
	case TE_SUPERBULLET:
		MSG_WriteByte (&sv.multicast, TE_SUPERSPIKE);
#ifdef NQPROT
		MSG_WriteByte (&sv.nqmulticast, TE_SUPERSPIKE);
#endif
		type = TE_SUPERBULLET;
		split = PEXT_TE_BULLET;
		break;
	case TE_BLOOD:
	case TE_GUNSHOT:
		MSG_WriteByte (&sv.multicast, type);
		MSG_WriteByte (&sv.multicast, count);
#ifdef NQPROT
		MSG_WriteByte (&sv.nqmulticast, type);	//nq doesn't have a count.
#endif
		break;
	case TE_LIGHTNING1:
	case TE_LIGHTNING2:
	case TE_LIGHTNING3:
		SV_Error("SV_point_tempentity - type is a beam\n");
	default:
		MSG_WriteByte (&sv.multicast, type);
	}
	MSG_WriteCoord (&sv.multicast, o[0]);
	MSG_WriteCoord (&sv.multicast, o[1]);
	MSG_WriteCoord (&sv.multicast, o[2]);
#ifdef NQPROT
	MSG_WriteCoord (&sv.nqmulticast, o[0]);
	MSG_WriteCoord (&sv.nqmulticast, o[1]);
	MSG_WriteCoord (&sv.nqmulticast, o[2]);
#endif
	if (type == TE_BLOOD || type == TE_LIGHTNINGBLOOD)
	{
#ifdef NQPROT
		sv.nqmulticast.cursize = 0;	//don't send a te_blood or lightningblood to an nq client - they'll die horribly.

		//send a particle instead
		MSG_WriteByte (&sv.nqmulticast, svc_particle);
		MSG_WriteCoord (&sv.nqmulticast, o[0]);
		MSG_WriteCoord (&sv.nqmulticast, o[1]);
		MSG_WriteCoord (&sv.nqmulticast, o[2]);
		//no direction.
		MSG_WriteChar (&sv.nqmulticast, 0);
		MSG_WriteChar (&sv.nqmulticast, 0);
		MSG_WriteChar (&sv.nqmulticast, 0);
		MSG_WriteByte (&sv.nqmulticast, count*20);
		if (type == TE_BLOOD)
			MSG_WriteByte (&sv.nqmulticast, 73);
		else
			MSG_WriteByte (&sv.nqmulticast, 225);
#endif
	}

	SV_MulticastProtExt (o, MULTICAST_PHS, pr_global_struct->dimension_send, split, 0);
	
	if (!split)	//don't bother sending again.
		return;

//this is for cool people (not nq users)
	MSG_WriteByte (&sv.multicast, svc_temp_entity);
	MSG_WriteByte (&sv.multicast, type);
	MSG_WriteCoord (&sv.multicast, o[0]);
	MSG_WriteCoord (&sv.multicast, o[1]);
	MSG_WriteCoord (&sv.multicast, o[2]);

	SV_MulticastProtExt (o, MULTICAST_PHS, pr_global_struct->dimension_send, 0, split);
}

void SV_beam_tempentity (int ownerent, vec3_t start, vec3_t end, int type)
{
	MSG_WriteByte (&sv.multicast, svc_temp_entity);
	MSG_WriteByte (&sv.multicast, type);
	MSG_WriteShort (&sv.multicast, ownerent);
	MSG_WriteCoord (&sv.multicast, start[0]);
	MSG_WriteCoord (&sv.multicast, start[1]);
	MSG_WriteCoord (&sv.multicast, start[2]);
	MSG_WriteCoord (&sv.multicast, end[0]);
	MSG_WriteCoord (&sv.multicast, end[1]);
	MSG_WriteCoord (&sv.multicast, end[2]);
#ifdef NQPROT
	if (type == TE_LIGHTNING2 && ownerent<0)	//special handling for TE_BEAM (don't do TE_RAILGUN - it's a tomaz extension)
	{
		MSG_WriteByte (&sv.nqmulticast, svc_temp_entity);
		MSG_WriteByte (&sv.nqmulticast, NQTE_BEAM);
		MSG_WriteShort (&sv.nqmulticast, -1-ownerent);
		MSG_WriteCoord (&sv.nqmulticast, start[0]);
		MSG_WriteCoord (&sv.nqmulticast, start[1]);
		MSG_WriteCoord (&sv.nqmulticast, start[2]);
		MSG_WriteCoord (&sv.nqmulticast, end[0]);
		MSG_WriteCoord (&sv.nqmulticast, end[1]);
		MSG_WriteCoord (&sv.nqmulticast, end[2]);
	}
	else
	{
		MSG_WriteByte (&sv.nqmulticast, svc_temp_entity);
		MSG_WriteByte (&sv.nqmulticast, type);
		MSG_WriteShort (&sv.nqmulticast, ownerent);
		MSG_WriteCoord (&sv.nqmulticast, start[0]);
		MSG_WriteCoord (&sv.nqmulticast, start[1]);
		MSG_WriteCoord (&sv.nqmulticast, start[2]);
		MSG_WriteCoord (&sv.nqmulticast, end[0]);
		MSG_WriteCoord (&sv.nqmulticast, end[1]);
		MSG_WriteCoord (&sv.nqmulticast, end[2]);
	}
#endif
	SV_MulticastProtExt (start, MULTICAST_PHS, pr_global_struct->dimension_send, 0, 0);
}

void PF_tempentity (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), G_FLOAT(OFS_PARM1), 1);
}

//=============================================================================

int SV_ModelIndex (char *name);

void PF_makestatic (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*ent;
	int		mdlindex, i;
	entity_state_t *state;
	
	ent = G_EDICT(prinst, OFS_PARM0);

	SV_FlushSignon ();

	mdlindex = SV_ModelIndex(PR_GetString(prinst, ent->v.model));

	if (ent->v.drawflags || ent->v.alpha || mdlindex > 255 || ent->v.frame > 255 || ent->v.scale || ent->v.abslight)
	{
		if (sv.numextrastatics==sizeof(sv.extendedstatics)/sizeof(sv.extendedstatics[0]))
			return;	//fail the whole makestatic thing.

		state = &sv.extendedstatics[sv.numextrastatics++];
		memset(state, 0, sizeof(*state));
		state->number = sv.numextrastatics;
		state->flags = 0;
		VectorCopy (ent->v.origin, state->origin);
		VectorCopy (ent->v.angles, state->angles);
		state->modelindex = mdlindex;//ent->v.modelindex;
		state->frame = ent->v.frame;
		state->colormap = ent->v.colormap;
		state->skinnum = ent->v.skin;
		state->effects = ent->v.effects;
		state->drawflags = ent->v.drawflags;
		state->abslight = (int)(ent->v.abslight*255) & 255;
		state->trans = ent->v.alpha;
		if (!state->trans)
			state->trans = 1;
		state->fatness = ent->v.fatness;
		state->scale = ent->v.scale;

		if (progstype != PROG_QW)	//don't send extra nq effects to a qw client.
			state->effects &= EF_BRIGHTLIGHT | EF_DIMLIGHT;
	}
	else
	{
		MSG_WriteByte (&sv.signon,svc_spawnstatic);

		MSG_WriteByte (&sv.signon, mdlindex&255);

		MSG_WriteByte (&sv.signon, ent->v.frame);
		MSG_WriteByte (&sv.signon, (int)ent->v.colormap);
		MSG_WriteByte (&sv.signon, (int)ent->v.skin);
		for (i=0 ; i<3 ; i++)
		{
			MSG_WriteCoord(&sv.signon, ent->v.origin[i]);
			MSG_WriteAngle(&sv.signon, ent->v.angles[i]);
		}
	}

// throw the entity away now
	ED_Free (svprogfuncs, ent);
}

//=============================================================================

/*
==============
PF_setspawnparms
==============
*/
void PF_setspawnparms (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*ent;
	int		i;
	client_t	*client;

	ent = G_EDICT(prinst, OFS_PARM0);
	i = NUM_FOR_EDICT(prinst, ent);
	if (i < 1 || i > sv.allocated_client_slots)
	{
		PR_BIError ("Entity is not a client");
		return;
	}

	// copy spawn parms out of the client_t
	client = svs.clients + (i-1);

	for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
		(&pr_global_struct->parm1)[i] = client->spawn_parms[i];
}

/*
==============
PF_changelevel
==============
*/
void PF_changelevel (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*s, *spot;
	static	int	last_spawncount;

// make sure we don't issue two changelevels
	if (svs.spawncount == last_spawncount)
		return;
	last_spawncount = svs.spawncount;

	if (*svprogfuncs->callargc == 2)
	{
		s = PR_GetStringOfs(prinst, OFS_PARM0);
		spot = PR_GetStringOfs(prinst, OFS_PARM1);
		Cbuf_AddText (va("changelevel %s %s\n",s, spot), RESTRICT_LOCAL);
	}
	else
	{
		s = PR_GetStringOfs(prinst, OFS_PARM0);
		Cbuf_AddText (va("map %s\n",s), RESTRICT_LOCAL);
	}
}


/*
==============
PF_logfrag

logfrag (killer, killee)
==============
*/
void PF_logfrag (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*ent1, *ent2;
	int		e1, e2;
	char	*s;

	ent1 = G_EDICT(prinst, OFS_PARM0);
	ent2 = G_EDICT(prinst, OFS_PARM1);

	e1 = NUM_FOR_EDICT(prinst, ent1);
	e2 = NUM_FOR_EDICT(prinst, ent2);
	
	if (e1 < 1 || e1 > sv.allocated_client_slots
	|| e2 < 1 || e2 > sv.allocated_client_slots)
		return;

#ifdef SVRANKING
	if (e1 != e2)	//don't get a point for suiside.
		svs.clients[e1-1].kills += 1;
	svs.clients[e2-1].deaths += 1;
#endif
	
	s = va("\\%s\\%s\\\n",svs.clients[e1-1].name, svs.clients[e2-1].name);

	SZ_Print (&svs.log[svs.logsequence&1], s);
	if (sv_fraglogfile) {
		fprintf (sv_fraglogfile, s);
		fflush (sv_fraglogfile);
	}
}


/*
==============
PF_infokey

string(entity e, string key) infokey
==============
*/
void PF_infokey (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*e;
	int		e1;
	char	*value;
	char	*key;
	char ov[256];
	char *dest;

	e = G_EDICT(prinst, OFS_PARM0);
	e1 = NUM_FOR_EDICT(prinst, e);
	key = PR_GetStringOfs(prinst, OFS_PARM1);

	if (e1 == 0)
	{
		if ((value = Info_ValueForKey (svs.info, key)) == NULL || !*value)
			value = Info_ValueForKey(localinfo, key);
	}
	else if (e1 <= MAX_CLIENTS)
	{
		if (!strcmp(key, "ip"))
			value = strcpy(ov, NET_BaseAdrToString (svs.clients[e1-1].netchan.remote_address));
		else if (!strcmp(key, "ping"))
		{
			int ping = SV_CalcPing (&svs.clients[e1-1]);
			sprintf(ov, "%d", ping);
			value = ov;
		}
		else if (!strcmp(key, "trustlevel"))	//info for progs.
		{
			rankstats_t rs;
			if (!svs.clients[e1-1].rankid)
				value = "";
			else if (Rank_GetPlayerStats(svs.clients[e1-1].rankid, &rs))
			{
				sprintf(ov, "%d", rs.trustlevel);
				value = ov;
			}
			else
				value = "";
		}
		else
			value = Info_ValueForKey (svs.clients[e1-1].userinfo, key);
	} else
		value = "";

	dest = PF_TempStr();
	strcpy(dest, value);
	RETURN_CSTRING(dest);
}

/*
==============
PF_stof

float(string s) stof
==============
*/
void PF_stof (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*s;

	s = PR_GetStringOfs(prinst, OFS_PARM0);

	G_FLOAT(OFS_RETURN) = atof(s);
}

void PF_Sin (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = sin(G_FLOAT(OFS_PARM0));
}
void PF_Cos (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = cos(G_FLOAT(OFS_PARM0));
}
void PF_Sqrt (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = sqrt(G_FLOAT(OFS_PARM0));
}
void PF_pow (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = pow(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
}



/*
==============
PF_multicast

void(vector where, float set) multicast
==============
*/
void PF_multicast (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
#if !defined(NQPROT) && !defined(PREPARSE)	//these make multicasts issue when message is compleate, making the multicast builtin useless.
	float	*o;
	int		to;

	o = G_VECTOR(OFS_PARM0);
	to = G_FLOAT(OFS_PARM1);

	SV_Multicast (o, to);
#endif
}


void PF_Fixme (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	progfuncs_t *progfuncs = prinst;
int i;
qboolean printedheader = false;

	SV_EndRedirect();

	for (i = 0; BuiltinList[i].bifunc; i++)
	{
		if (BuiltinList[i].ebfsnum == prinst->lastcalledbuiltinnumber)
		{
			if (!printedheader)
			{
				Con_Printf("\nMod forgot to ensure support for builtin %i\nPossible builtins:\n", prinst->lastcalledbuiltinnumber);
				printedheader = true;
			}
			Con_Printf("%s\n", BuiltinList[i].name);
		}
	}

	Con_Printf("\n");

	if (progstype == PROG_QW)
		PR_RunError(prinst, "\nBuiltin %i not implemented.\nMods designed for mvdsv may need pr_imitatemvdsv to be enabled.", prinst->lastcalledbuiltinnumber);
	else
		PR_RunError(prinst, "\nBuiltin %i not implemented.\nMod is not compatable.", prinst->lastcalledbuiltinnumber);
	PR_BIError ("bulitin not implemented");
}

void PF_Ignore(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_INT(OFS_RETURN) = 0;
}

/*
==============
PF_logfrag

logfrag (killer, killee)
==============
*/
void PF_logstring (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*s;

	s = PF_VarString(prinst, 0, pr_globals);
	
	if (sv_fraglogfile) {
		fprintf (sv_fraglogfile, s);
		fflush (sv_fraglogfile);
	}
}
#define PRSTR	0xa6ffb3d7
void PF_newstring(progfuncs_t *prinst, struct globalvars_s *pr_globals)	//mvdsv
{
	char *s;
	int len;

	char *in = PR_GetStringOfs(prinst, OFS_PARM0);

	len = strlen(in)+1;
	if (*prinst->callargc == 2 && G_FLOAT(OFS_PARM1) > len)
		len = G_FLOAT(OFS_PARM1);
	s = Z_Malloc(len+8);
	((int *)s)[0] = PRSTR;
	((int *)s)[1] = len;

	strcpy(s+8, in);

	RETURN_SSTRING(s+8);
}
void PF_dupstring(progfuncs_t *prinst, struct globalvars_s *pr_globals)	//frik_file
{
	char *s, *in;
	int len;
	in = PF_VarString(prinst, 0, pr_globals);
	len = strlen(in)+1;
	s = Z_Malloc(len+8);
	((int *)s)[0] = PRSTR;
	((int *)s)[1] = len;
	strcpy(s+8, in);
	RETURN_SSTRING(s+8);
}

void PF_forgetstring(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *s=PR_GetStringOfs(prinst, OFS_PARM0);
	s-=8;
	if (((int *)s)[0] != PRSTR)
	{
		Con_Printf("QC tried to free a non allocated string\n");
		pr_trace = 1;
		return;
	}
	Z_Free(s);
}
void PF_strlen(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = strlen(PR_GetStringOfs(prinst, OFS_PARM0));
}

void PF_strcatp(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *buf = PR_GetStringOfs(prinst, OFS_PARM0); char *add = PR_GetStringOfs(prinst, OFS_PARM1);
	int wantedlen = G_FLOAT(OFS_PARM2);
	int len;
	if (((int *)(buf-8))[0] != PRSTR)
	{
		Con_Printf("QC tried to add to a non allocated string\n");
		pr_trace = 1;
		G_FLOAT(OFS_RETURN) = 0;
		return;
	}
	len = strlen(add);
	buf+=strlen(buf);
	strcat(buf, add);
	buf+=len;
	while(len++ < wantedlen)
		*buf++ = ' ';
	*buf = '\0';

	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
}

void PF_redstring(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *string = PR_GetStringOfs(prinst, OFS_PARM0), *s;
	static char buf[1024];

	for (s = buf; *string; s++, string++)
		*s=*string|128;	
	*s = '\0';

	RETURN_TSTRING(buf);
}

#ifdef PEXT_BULLETENS
void PF_bulleten (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int j;
	client_t  *client;
	char *msg = PF_VarString(prinst, 1, pr_globals);
	int board = G_FLOAT(OFS_PARM0);
	int msglen = strlen(msg);

	// send the data to all relevent clients
	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++)
	{
		if (client->state < cs_connected)	//so a call can be used in client connected.
			continue;

		if (client->fteprotocolextensions & PEXT_BULLETENS)
		{
			ClientReliableCheckBlock(client, msglen+1);
			ClientReliableWrite_Byte(client, svc_bulletentext);
			ClientReliableWrite_Byte(client, board);
			ClientReliableWrite_String(client, msg);
		}
	}
}
#endif

#ifdef SVCHAT
void SV_Chat(char *filename, float starttag, edict_t *edict);
void PF_chat (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_Chat(PR_GetStringOfs(prinst, OFS_PARM0), G_FLOAT(OFS_PARM1), G_EDICT(prinst, OFS_PARM2));
}
#endif









//FRIK_FILE extensions.



//returns a section of a string as a tempstring
void PF_substring (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i, start, length;
	char *s;
	char *string = PF_TempStr();

	s = PR_GetStringOfs(prinst, OFS_PARM0);
	start = G_FLOAT(OFS_PARM1);
	length = G_FLOAT(OFS_PARM2);

	if (length >= sizeof(string))
		length = sizeof(string)-1;

	for (i = 0; i < start && *s; i++, s++)
		;

	for (i = 0; *s && i < length; i++, s++)
		string[i] = *s;
	string[i] = 0;

	RETURN_SSTRING(string);
}


//vector(string s) stov = #117
//returns vector value from a string
void PF_stov (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i;
	char *s;
	float *out;

	s = PF_VarString(prinst, 0, pr_globals);
	out = G_VECTOR(OFS_RETURN);
	out[0] = out[1] = out[2] = 0;

	if (*s == '\'')
		s++;

	for (i = 0; i < 3; i++)
	{
		while (*s == ' ' || *s == '\t')
			s++;
		out[i] = atof (s);
		if (!out[i] && *s != '-' && *s != '+' && (*s < '0' || *s > '9'))
			break; // not a number
		while (*s && *s != ' ' && *s !='\t' && *s != '\'')
			s++;
		if (*s == '\'')
			break;
	}
}



#define MAX_QC_FILES 8

typedef struct {
	char name[256];
	char *data;
	int bufferlen;
	int len;
	int ofs;
	int accessmode;
	progfuncs_t *prinst;
} pf_fopen_files_t;
pf_fopen_files_t pf_fopen_files[MAX_QC_FILES];

void PF_fopen (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *name = PR_GetStringOfs(prinst, OFS_PARM0);
	int fmode = G_FLOAT(OFS_PARM1);
	int i;

	for (i = 0; i < MAX_QC_FILES; i++)
		if (!pf_fopen_files[i].data)
			break;

	if (i == MAX_QC_FILES)	//too many already open
	{
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}

	if (name[1] == ':' ||	//dos filename absolute path specified - reject.
		*name == '\\' || *name == '/' ||	//absolute path was given - reject
		strstr(name, ".."))	//someone tried to be cleaver.
	{
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}

	pf_fopen_files[i].prinst = prinst;

	Q_strncpyz(pf_fopen_files[i].name, name, sizeof(pf_fopen_files[i].name));

	pf_fopen_files[i].accessmode = fmode;
	switch (fmode)
	{
	case 0:	//read
		pf_fopen_files[i].data = COM_LoadMallocFile(name);
		pf_fopen_files[i].bufferlen = pf_fopen_files[i].len = com_filesize;
		pf_fopen_files[i].ofs = 0;
		if (pf_fopen_files[i].data)
			G_FLOAT(OFS_RETURN) = i;
		else
			G_FLOAT(OFS_RETURN) = -1;
		break;
	case 1:	//append
		pf_fopen_files[i].data = COM_LoadMallocFile(name);
		pf_fopen_files[i].ofs = pf_fopen_files[i].bufferlen = pf_fopen_files[i].len = com_filesize;
		if (pf_fopen_files[i].data)
		{
			G_FLOAT(OFS_RETURN) = i;
			break;
		}
		//fall through
	case 2:	//write
		pf_fopen_files[i].bufferlen = 8192;
		pf_fopen_files[i].data = BZ_Malloc(pf_fopen_files[i].bufferlen);
		pf_fopen_files[i].len = 0;
		pf_fopen_files[i].ofs = 0;
		G_FLOAT(OFS_RETURN) = i;
		break;
	default: //bad
		G_FLOAT(OFS_RETURN) = -1;
		break;
	}
}

void PF_fclose (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int fnum = G_FLOAT(OFS_PARM0);
	if (fnum < 0 || fnum >= MAX_QC_FILES)
		return;	//out of range

	if (!pf_fopen_files[fnum].data)
		return;	//not open

	if (pf_fopen_files[fnum].prinst != prinst)
		return;	//this just isn't ours.

	switch(pf_fopen_files[fnum].accessmode)
	{
	case 0:
		BZ_Free(pf_fopen_files[fnum].data);
		break;
	case 1:
	case 2:
		COM_WriteFile(pf_fopen_files[fnum].name, pf_fopen_files[fnum].data, pf_fopen_files[fnum].len);
		BZ_Free(pf_fopen_files[fnum].data);
		break;
	}
	pf_fopen_files[fnum].data = NULL;
}

void PF_fgets (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char c, *s, *o, *max;
	int fnum = G_FLOAT(OFS_PARM0);
	char *pr_string_temp = PF_TempStr();

	*pr_string_temp = '\0';
	RETURN_SSTRING(pr_string_temp);
	if (fnum < 0 || fnum >= MAX_QC_FILES)
		return;	//out of range

	if (!pf_fopen_files[fnum].data)
		return;	//not open

	if (pf_fopen_files[fnum].prinst != prinst)
		return;	//this just isn't ours.

	//read up to the next \n, ignoring any \rs.
	o = pr_string_temp;
	max = o + sizeof(pr_string_temp)-1;
	s = pf_fopen_files[fnum].data+pf_fopen_files[fnum].ofs;
	while(*s)
	{
		c = *s++;
		if (c == '\n')
			break;
		if (c == '\r')
			continue;

		if (o == max)
			break;
		*o++ = c;
	}
	*o = '\0';

	pf_fopen_files[fnum].ofs = s - pf_fopen_files[fnum].data;

	RETURN_SSTRING(pr_string_temp);
}

void PF_fputs (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int fnum = G_FLOAT(OFS_PARM0);
	char *msg = PF_VarString(prinst, 1, pr_globals);
	int len = strlen(msg);
	if (fnum < 0 || fnum >= MAX_QC_FILES)
		return;	//out of range

	if (!pf_fopen_files[fnum].data)
		return;	//not open

	if (pf_fopen_files[fnum].prinst != prinst)
		return;	//this just isn't ours.

	if (pf_fopen_files[fnum].bufferlen < pf_fopen_files[fnum].ofs + len)
	{
		char *newbuf;
		pf_fopen_files[fnum].bufferlen = pf_fopen_files[fnum].bufferlen*2 + len;
		newbuf = BZF_Malloc(pf_fopen_files[fnum].bufferlen);
		memcpy(newbuf, pf_fopen_files[fnum].data, pf_fopen_files[fnum].len);
		BZ_Free(pf_fopen_files[fnum].data);
		pf_fopen_files[fnum].data = newbuf;
	}

	memcpy(pf_fopen_files[fnum].data + pf_fopen_files[fnum].ofs, msg, len);
	if (pf_fopen_files[fnum].len < pf_fopen_files[fnum].ofs + len)
		pf_fopen_files[fnum].len = pf_fopen_files[fnum].ofs + len;
	pf_fopen_files[fnum].ofs+=len;
}

void PF_fcloseall (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i;
	for (i = 0; i < MAX_QC_FILES; i++)
	{
		G_FLOAT(OFS_PARM0) = i;
		PF_fclose(prinst, pr_globals);
	}
}
















//the lh extension system asks for a name for the extension.
//the ebfs version is a function that returns a builtin number.
//thus lh's system requires various builtins to exist at specific numbers.
typedef struct lh_extension_s {
	char *name;
	int numbuiltins;
	qboolean *enabled;
	char *builtins[14];	//extend freely
} lh_extension_t;



lh_extension_t QSG_Extensions[] = {
	{"QW_ENGINE"},	//warning: interpretation of .skin on players can be dodgy, as can some other QW features that differ from NQ.
	{"DP_HALFLIFE_MAP_CVAR"},
	{"FTE_MULTIPROGS"},	//multiprogs functions are available.
	{"FTE_SOLID_LADDER"},	//part of a worthy hl implementation. Allows a simple trigger to remove effects of gravity (solid 20)
	{"FRIK_FILE",						11, NULL, {"stof", "fopen","fclose","fgets","fputs","strlen","strcat","substring","stov","strzone","strunzone"}},
	{"DP_REGISTERCVAR",					1,	NULL, {"registercvar"}},

	{"DP_QC_SINCOSSQRTPOW",				4,	NULL, {"sin", "cos", "sqrt", "pow"}},

	{"DP_TE_BLOOD",						1,	NULL, {"te_blood"}},
	{"QSG_CVARSTRING",					1,	NULL, {"cvar_string"}},
#ifndef NOMEDIA
	{"FTE_MEDIA_AVI"},	//playfilm supports avi files.
	{"FTE_MEDIA_ROQ"},	//playfilm command supports q3 roq files
	{"FTE_MEDIA_CIN"},	//playfilm command supports q2 cin files.
#endif
	{"QWE_MVD_RECORD"},	//Quakeworld extended get the credit for this one. (mvdsv)
	{"FTE_MVD_PLAYBACK"},
	{"DP_SPRITE32"},				//hmm... is it legal to advertise this one?
	{"DP_QC_TRACE_MOVETYPES"},		//this one is just a lame excuse to add annother extension...
	{"DP_QC_TRACEBOX",					1,	NULL,	{"tracebox"}},
	{"DP_MOVETYPEBOUNCEMISSILE"},	//I added the code with hexen2 support.
	{"DP_SV_NODRAWTOCLIENT"},		//I prefer my older system. Guess I might as well remove that older system at some point.
	{"DP_SV_DRAWONLYTOCLIENT"},
	{"DP_EF_FULLBRIGHT"},			//Rerouted to hexen2 support.
	{"DP_EF_BLUE"},					//hah!! This is QuakeWorld!!!
	{"DP_EF_RED"},
	{"DP_QC_MINMAXBOUND",				3,	NULL, {"min", "max", "bound"}},
	{"DP_QC_RANDOMVEC",					1,	NULL, {"randomvec"}},
	{"DP_QC_COPYENTITY",				1,	NULL, {"copyentity"}},
	{"KRIMZON_SV_PARSECLIENTCOMMAND",	3,	NULL, {"clientcommand", "tokenize", "argv"}},	//very very similar to the mvdsv system.
	{"DP_QC_VECTORVECTORS",				1,	NULL, {"vectorvectors"}},
	{"DP_QC_FINDFLOAT",					1,	NULL, {"findfloat"}},
	{"DP_QC_FINDCHAIN",					1,	NULL, {"findchain"}},
	{"DP_QC_FINDCHAINFLOAT",			1,	NULL, {"findchainfloat"}},
	{"TEI_MD3_MODEL"},
//	{"TQ_RAILTRAIL"},	//client supports it, server can't filter it, but can currently send it.
	{"DP_QUAKE2_MODEL"},
	{"DP_QUAKE3_MODEL"},
	{"DP_QC_ETOS",						1,	NULL, {"etos"}},
	{"DP_SV_PLAYERPHYSICS"},
	{"DP_EXTRA_TEMPSTRING"},	//ftos returns 16 temp buffers.

	{"DP_TE_STANDARDEFFECTBUILTINS",	14,	NULL, { "te_gunshot", "te_spike", "te_superspike", "te_explosion", "te_tarexplosion",
													"te_wizspike", "te_knightspike", "te_lavasplash", "te_teleport", "te_explosion2",
													"te_lightning1", "te_lightning2", "te_lightning3", "te_beam"}},	//should we include QW ones?...

	{"ZQ_MOVETYPE_NOCLIP"},
	{"ZQ_MOVETYPE_FLY"},
	{"ZQ_MOVETYPE_NONE"},

	{"EXT_DIMENSION_VISIBILITY"},
	{"EXT_DIMENSION_PHYSICS"},
	{"EXT_DIMENSION_GHOST"},
	{"EXT_BITSHIFT",					1,	NULL, {"bitshift"}},

	{"FTE_FORCEINFOKEY",				1,	NULL, {"forceinfokey"}},
	{"FTE_MULTITHREADED",				3,	NULL, {"sleep", "fork", "abort"}},
#ifdef SVCHAT
	{"FTE_NPCCHAT",						1,	NULL, {"chat"}},	//server looks at chat files. It automagically branches through calling qc functions as requested.
#endif
	{"DP_SV_SETCOLOR"}
};

//some of these are overkill yes, but they are all derived from the fteextensions flags and document the underlaying protocol available.
//(which is why there are two lists of extensions here)
lh_extension_t FTE_Protocol_Extensions[] =
{
	{"FTE_PEXT_SETVIEW"},		//nq setview works.
	{"DP_ENT_SCALE"},			//entities may be rescaled
	{"FTE_PEXT_LIGHTSTYLECOL"},	//lightstyles may have colours.
	{"DP_ENT_ALPHA"},			//transparent entites
	{"FTE_PEXT_VIEW2"},		//secondary view.
	{"FTE_PEXT_BULLETENS"},	//bulleten boards (scrolling text on walls)
#ifdef PEXT_ZLIBDL
	{"FTE_PEXT_ZLIBDL"},		//supposed download optimisation (unimportant to qc)
#else
	{NULL},
#endif
#ifdef PEXT_LIGHTUPDATES
	{"FTE_PEXT_LIGHTUPDATES"},	//zap.mdl is sent as a nail packet.
#else
	{NULL},
#endif
	{"FTE_PEXT_FATNESS"},		//entities may be expanded along thier vertex normals
	{"DP_HALFLIFE_MAP"},		//entitiy can visit a hl bsp
	{"FTE_PEXT_TE_BULLET"},	//additional particle effect. Like TE_SPIKE and TE_SUPERSPIKE
	{"FTE_PEXT_HULLSIZE"},	//means we can tell a client to go to crouching hull
	{"FTE_PEXT_MODELDBL"},	//max of 512 models
	{"FTE_PEXT_ENTITYDBL"},	//max of 1024 ents
	{"FTE_PEXT_ENTITYDBL2"},	//max of 2048 ents
#ifdef PEXT_ORIGINDBL
	{"FTE_PEXT_ORIGINDBL"},	//-8k to +8k map size.
#else
	{NULL},
#endif
	{"FTE_PEXT_VWEAP"},
#ifdef Q2BSPS
	{"FTE_PEXT_Q2BSP"},		//supports q2 maps. No bugs are apparent.
#else
	{NULL},
#endif
#ifdef Q3BSPS
	{"FTE_PEXT_Q3BSP"},		//quake3 bsp support. dp probably has an equivelent, but this is queryable per client.
#else
	{NULL},
#endif
	{"UDC_EXTEFFECT",					0,	&pr_udc_exteffect_enabled},		//hmm. crap.
	{NULL},	//splitscreen - not queryable.
	{"FTE_HEXEN2"},				//client can use hexen2 maps. server can use hexen2 progs
	{"FTE_PEXT_SPAWNSTATIC"},	//means that static entities can have alpha/scale and anything else the engine supports on normal ents. (Added for >256 models, while still being compatable - previous system failed with -1 skins)
	{"FTE_PEXT_CUSTOMTENTS",					2,	NULL, {"RegisterTempEnt", "CustomTempEnt"}},
/*not supported yet*/	{"FTE_PEXT_256PACKETENTITIES"},	//client is able to receive unlimited packet entities (server caps itself to 256 to prevent insanity).
	{"TEI_SHOWLMP2",					6,	NULL, {"showpic", "hidepic", "movepic", "changepic", "showpicent", "hidepicent"}}	//telejano doesn't actually export the moveent/changeent (we don't want to either cos it would stop frik_file stuff being autoregistered)
};


int PR_EnableEBFSBuiltin(char *name)
{
	int i;
	for (i = 0;BuiltinList[i].name;i++)
	{
		if (!strcmp(BuiltinList[i].name, name))
		{
			if (!pr_overridebuiltins.value)
			{
				if (pr_builtin[BuiltinList[i].ebfsnum] != NULL && pr_builtin[BuiltinList[i].ebfsnum] != PF_Fixme)
				{
					if (pr_builtin[BuiltinList[i].ebfsnum] == BuiltinList[i].bifunc)	//it is already this function.
						return BuiltinList[i].ebfsnum;

					return 0;	//already used... ?
				}
			}

			pr_builtin[BuiltinList[i].ebfsnum] = BuiltinList[i].bifunc;
			
			return BuiltinList[i].ebfsnum;
		}
	}

	return 0;	//not known
}



lh_extension_t *checkfteextensioncl(int mask, char *name)	//true if the cient extension mask matches an extension name
{
	int i;
	unsigned int m = 1;
	for (i = 0; i < sizeof(FTE_Protocol_Extensions)/sizeof(lh_extension_t); i++)
	{
		if (mask & m)	//suported
		{
			if (FTE_Protocol_Extensions[i].name)	//some were removed
				if (!stricmp(name, FTE_Protocol_Extensions[i].name))	//name matches
					return &FTE_Protocol_Extensions[i];
		}

		m=m<<2;
	}
	return NULL;
}

lh_extension_t *checkfteextensionsv(char *name)	//true if the server supports an protocol extension.
{
	int i;

	for (i = 0; i < sizeof(FTE_Protocol_Extensions)/sizeof(lh_extension_t); i++)
	{
		if (FTE_Protocol_Extensions[i].name)	//some were removed
			if (!stricmp(name, FTE_Protocol_Extensions[i].name))	//name matches
				return &FTE_Protocol_Extensions[i];
	}
	return NULL;
}

lh_extension_t *checkextension(char *name)
{
	int i;
	for (i = 0; i < sizeof(QSG_Extensions)/sizeof(lh_extension_t); i++)
	{
		if (!QSG_Extensions[i].name)
			continue;
		if (!stricmp(name, QSG_Extensions[i].name))
			return &QSG_Extensions[i];
	}
	return NULL;
}

/*
=================
PF_checkextension

returns true if the extension is supported by the server

checkextension(string extensionname, [entity client])
=================
*/
void PF_checkextension (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	lh_extension_t *ext = NULL;
	char *s = PR_GetStringOfs(prinst, OFS_PARM0);

	ext = checkextension(s);
	if (!ext)
	{
		if (*svprogfuncs->callargc == 2)
		{
			int clnum = NUM_FOR_EDICT(prinst, G_EDICT(prinst, OFS_PARM1));
			if (clnum >= 1 && clnum <= MAX_CLIENTS)	//valid client as second parameter
			{
				ext = checkfteextensioncl(svs.clients[clnum-1].fteprotocolextensions, s);
			}
			else if (clnum == 0)
				ext = checkfteextensionsv(s);
			else
			{
			//ent wasn't valid
				Con_Printf("PF_CheckExtension with invalid client number");
			}
		}
		else
		{
			ext = checkfteextensionsv(s);
		}
	}

	if (ext)
	{
		int i;
		G_FLOAT(OFS_RETURN) = false;
		for (i = 0; i < ext->numbuiltins; i++)
		{
			if (!PR_EnableEBFSBuiltin(ext->builtins[i]))
			{
				Con_Printf("Failed to initialise builtin \"%s\" for extension \"%s\"", ext->builtins[i], s);
				return;	//whoops, we failed.
			}
		}

		if (ext->enabled)
			*ext->enabled = true;

		G_FLOAT(OFS_RETURN) = true;
	}
	else
		G_FLOAT(OFS_RETURN) = false;
}


void PF_builtinsupported (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *s = PR_GetStringOfs(prinst, OFS_PARM0);
	
	G_FLOAT(OFS_RETURN) = PR_EnableEBFSBuiltin(s);
}




//mvdsv builtins.
void PF_ExecuteCommand  (progfuncs_t *prinst, struct globalvars_s *pr_globals)	//83		//void() exec;
{
	int old_other, old_self; // mod_consolecmd will be executed, so we need to store this

	old_self = pr_global_struct->self;
	old_other = pr_global_struct->other;

	Cbuf_Execute();

	pr_global_struct->self = old_self;
	pr_global_struct->other = old_other;
}
void PF_Tokenize  (progfuncs_t *prinst, struct globalvars_s *pr_globals)			//84			//void(string str) tokanize;
{
	Cmd_TokenizeString(PR_GetStringOfs(prinst, OFS_PARM0));
	G_FLOAT(OFS_RETURN) = Cmd_Argc();
}
void PF_ArgC  (progfuncs_t *prinst, struct globalvars_s *pr_globals)				//85			//float() argc;
{
	G_FLOAT(OFS_RETURN) = Cmd_Argc();
}
void PF_ArgV  (progfuncs_t *prinst, struct globalvars_s *pr_globals)				//86			//string(float num) argv;
{
	char *dest = PF_TempStr();
	strcpy(dest, Cmd_Argv(G_FLOAT(OFS_PARM0)));
	RETURN_CSTRING(dest);
}

/*
=================
PF_teamfield

string teamfield(.string field)
=================
*/

void PF_teamfield (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	pr_teamfield = G_INT(OFS_PARM0)+prinst->fieldadjust;
}

/*
=================
PF_substr

string substr(string str, float start, float len)
=================
*/

void PF_substr (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *dest = PF_TempStr();
	char *s;
	int start, len, l;

	s = PR_GetStringOfs(prinst, OFS_PARM0);
	start = (int) G_FLOAT(OFS_PARM1);
	len = (int) G_FLOAT(OFS_PARM2);
	l = strlen(s);

	if (start >= l || !len || !*s)
	{
		RETURN_TSTRING("");
		return;
	}

	s += start;
	l -= start;

	if (len > l + 1)
		len = l + 1;

	Q_strncpyz(dest, s, len + 1);

	RETURN_TSTRING(dest);
}

/*
=================
PF_strcat

string strcat(string str1, string str2)
=================
*/

void PF_strcat (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *dest = PF_TempStr();
	char *src = PF_VarString(prinst, 0, pr_globals);
	strncpy(dest, src, MAXTEMPBUFFERLEN);
	RETURN_TSTRING(dest);
}

/*
=================
PF_str2byte

float str2byte (string str)
=================
*/

void PF_str2byte (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = (float) *PR_GetStringOfs(prinst, OFS_PARM0);
}

/*
=================
PF_str2short

float str2short (string str)
=================
*/

void PF_str2short (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = (float) LittleShort(*(short*)PR_GetStringOfs(prinst, OFS_PARM0));
}

/*
=================
PF_readcmd

string readmcmd (string str)
=================
*/

void PF_readcmd (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *s;
	static char output[8000];
	extern char outputbuf[];
	extern redirect_t sv_redirected;
	redirect_t old;

	s = PR_GetStringOfs(prinst, OFS_PARM0);

	Cbuf_Execute();
	Cbuf_AddText (s, RESTRICT_LOCAL);

	old = sv_redirected;
	if (old != RD_NONE)
		SV_EndRedirect();

	SV_BeginRedirect(RD_OBLIVION);
	Cbuf_Execute();
	Q_strncpyz(output, outputbuf, sizeof(output));
	SV_EndRedirect();

	if (old != RD_NONE)
		SV_BeginRedirect(old);


	G_INT(OFS_RETURN) = (int)PR_SetString(prinst, output);
}

/*
=================
PF_redirectcmd

void redirectcmd (entity to, string str)
=================
*/
/*
void PF_redirectcmd (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *s;
	int entnum;
	extern redirect_t sv_redirected;

	if (sv_redirected)
		return;

	entnum = G_EDICTNUM(OFS_PARM0);
	if (entnum < 1 || entnum > MAX_CLIENTS)
		PR_RunError ("Parm 0 not a client");

	s = G_STRING(OFS_PARM1);

	Cbuf_AddText (s);

	SV_BeginRedirect(RD_MOD + entnum);
	Cbuf_Execute();
	SV_EndRedirect();
}*/

void PF_calltimeofday (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	date_t date;
	func_t f;

	f = PR_FindFunction(svprogfuncs, "timeofday", PR_ANY);
	if (f)
	{
		SV_TimeOfDay(&date);

		G_FLOAT(OFS_PARM0) = (float)date.sec;
		G_FLOAT(OFS_PARM1) = (float)date.min;
		G_FLOAT(OFS_PARM2) = (float)date.hour;
		G_FLOAT(OFS_PARM3) = (float)date.day;
		G_FLOAT(OFS_PARM4) = (float)date.mon;
		G_FLOAT(OFS_PARM5) = (float)date.year;
		G_INT(OFS_PARM6) = (int)PR_SetString(prinst, date.str);

		PR_ExecuteProgram(prinst, f);
	}

}

/*
=================
PF_forcedemoframe

void PF_forcedemoframe(float now)
Forces demo frame
if argument 'now' is set, frame is written instantly
=================
*/

void PF_forcedemoframe (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	demo.forceFrame = 1;
//	if (G_FLOAT(OFS_PARM0) == 1)
//        SV_SendDemoMessage();
}


/*
=================
PF_strcpy

void strcpy(string dst, string src)
FIXME: check for null pointers first?
=================
*/

void PF_strcpy (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *src = PR_GetStringOfs(prinst, OFS_PARM1);
	char *dest = PR_GetStringOfs(prinst, OFS_PARM0);
	int *ident;
	ident = (int *)(dest-8);
	if (*ident != PRSTR)
	{
		Con_Printf("PF_strcpy: not an allocated string\n");
		return;
	}
	if (ident[0] < strlen(src)+1)
	{
		Con_Printf("PF_strcpy: allocated string is not big enough.\n");
		return;
	}
	strcpy(dest, src);
}

/*
=================
PF_strncpy

void strcpy(string dst, string src, float count)
FIXME: check for null pointers first?
=================
*/

void PF_strncpy (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	strncpy(PR_GetStringOfs(prinst, OFS_PARM0), PR_GetStringOfs(prinst, OFS_PARM1), (int) G_FLOAT(OFS_PARM2));
}


/*
=================
PF_strstr

string strstr(string str, string sub)
=================
*/

void PF_strstr (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *str, *sub, *p;

	str = PR_GetStringOfs(prinst, OFS_PARM0);
	sub = PR_GetStringOfs(prinst, OFS_PARM1);

	if ((p = strstr(str, sub)) == NULL)
	{
		G_INT(OFS_RETURN) = 0;
		return;
	}
	
	RETURN_TSTRING(p);
}

/*
====================
SV_CleanName_Init

sets chararcter table to translate quake texts to more friendly texts
====================
*/

char chartbl2[256];

void PR_CleanLogText_Init (void)
{
	int i;

	for (i = 0; i < 256; i++)
		chartbl2[i] = (i&127) < 32 ? ' ' : i&127;

	chartbl2[13] = 13;
	chartbl2[10] = 10;
	// special cases

	// numbers
	for (i = 18; i < 28; i++)
		chartbl2[i] = chartbl2[i + 128] = i + 30;

	// brackets
	chartbl2[29] = chartbl2[29 + 128] = chartbl2[128] = '(';
	chartbl2[31] = chartbl2[31 + 128] = chartbl2[130] = ')';
	chartbl2[16] = chartbl2[16 + 128]= '[';
	chartbl2[17] = chartbl2[17 + 128] = ']';

	// hash
	for (i = 1; i < 10; i++) // 5 redefined as '.'
		chartbl2[i] = chartbl2[i + 128] = '#';

	chartbl2[11] = chartbl2[11 + 128] = '#';

	// dot
	chartbl2[5] = chartbl2[14] = chartbl2[15] = chartbl2[28] = chartbl2[46] = '.';
	chartbl2[5 + 128] = chartbl2[14 + 128] = chartbl2[15 + 128] = chartbl2[28 + 128] = chartbl2[46 + 128] = '.';

	// left arrow
	chartbl2[127] = '>';

	// right arrow
	chartbl2[141] = '<';

	// '='
	chartbl2[30] = chartbl2[129] = chartbl2[30 + 128] = '=';

	// whitespaces
	chartbl2[12] = chartbl2[12 + 128] = chartbl2[138] = ' ';

	chartbl2[33] = chartbl2[33 + 128]= '!';
}

void PR_CleanText(unsigned char *text)
{
	for ( ; *text; text++)
		*text = chartbl2[*text];
}

/*
================
PF_log

void log(string name, float console, string text)
=================
*/

void PF_log(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char name[MAX_OSPATH], *text;
	FILE *file;

	_snprintf(name, MAX_OSPATH, "%s/%s.log", com_gamedir, PR_GetStringOfs(prinst, OFS_PARM0));
	text = PF_VarString(prinst, 2, pr_globals);
	PR_CleanText(text);

	if ((file = fopen(name, "a")) == NULL)
	{
		Sys_Printf("coldn't open log file %s\n", name);
	}
	else
	{
		fprintf (file, text);
		fflush (file);
		fclose(file);
	}

	if (G_FLOAT(OFS_PARM1))
		Con_Printf("%s", text);
	
}


#ifdef Q2BSPS
void PF_OpenPortal	(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (sv.worldmodel->fromgame == fg_quake2)
	{
		int i, portal	= G_FLOAT(OFS_PARM0);
		int state	= G_FLOAT(OFS_PARM1)!=0;
		client_t *client;
		for (client = svs.clients, i = 0; i < sv.allocated_client_slots; i++, client++)
		{
			if (client->state >= cs_connected)
			{
				ClientReliableWrite_Begin(client, svc_setportalstate, 3);
				if (state)
					ClientReliableWrite_Short(client, portal | (state<<15));
				else
					ClientReliableWrite_Short(client, portal);
			}
		}
		CMQ2_SetAreaPortalState(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
	}
}
#endif


//EXTENSION: DP_QC_COPYENTITY

//void(entity from, entity to) copyentity = #400
//copies data from one entity to another
void PF_copyentity (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t *in, *out;

	in = G_EDICT(prinst, OFS_PARM0);
	out = G_EDICT(prinst, OFS_PARM1);

	memcpy(&out->v, &in->v, pr_edict_size-svprogparms.edictsize);
}

//EXTENSION: DP_QC_ETOS

//string(entity ent) etos = #65
void PF_etos (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *s;
	s = PF_TempStr();
	sprintf (s, "entity %i", G_EDICTNUM(prinst, OFS_PARM0));
	G_INT(OFS_RETURN) = (int)PR_SetString(prinst, s);
}

//EXTENSION: DP_QC_FINDCHAIN

//entity(string field, string match) findchain = #402
//chained search for strings in entity fields
void PF_findchain (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i, f;
	char *s, *t;
	edict_t *ent, *chain;

	chain = (edict_t *) sv.edicts;

	f = G_INT(OFS_PARM0)+prinst->fieldadjust;
	s = PR_GetStringOfs(prinst, OFS_PARM1);

	for (i = 1; i < sv.num_edicts; i++)
	{
		ent = EDICT_NUM(svprogfuncs, i);
		if (ent->isfree)
			continue;
		t = *(string_t *)&((float*)&ent->v)[f];
		if (!t)
			continue;
		if (strcmp(t, s))
			continue;

		ent->v.chain = EDICT_TO_PROG(prinst, chain);
		chain = ent;
	}

	RETURN_EDICT(prinst, chain);
}

//EXTENSION: DP_QC_FINDCHAINFLOAT

//entity(string field, float match) findchainfloat = #403
//chained search for float, int, and entity reference fields
void PF_findchainfloat (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i, f;
	float s;
	edict_t	*ent, *chain;

	chain = (edict_t *) sv.edicts;

	f = G_INT(OFS_PARM0)+prinst->fieldadjust;
	s = G_FLOAT(OFS_PARM1);

	for (i = 1; i < sv.num_edicts; i++)
	{
		ent = EDICT_NUM(svprogfuncs, i);
		if (ent->isfree)
			continue;
		if (((float *)&ent->v)[f] != s)
			continue;

		ent->v.chain = EDICT_TO_PROG(prinst, chain);
		chain = ent;
	}

	RETURN_EDICT(prinst, chain);
}

//EXTENSION: DP_QC_FINDFLOAT

//entity(entity start, float fld, float match) findfloat = #98
void PF_FindFloat (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int e, f;
	float s;
	edict_t *ed;

	e = G_EDICTNUM(prinst, OFS_PARM0);
	f = G_INT(OFS_PARM1)+prinst->fieldadjust;
	s = G_FLOAT(OFS_PARM2);

	for (e++; e < sv.num_edicts; e++)
	{
		ed = EDICT_NUM(prinst, e);
		if (ed->isfree)
			continue;
		if (((float *)&ed->v)[f] == s)
		{
			RETURN_EDICT(prinst, ed);
			return;
		}
	}

	RETURN_EDICT(prinst, sv.edicts);
}




//EXTENSION: DP_QC_RANDOMVEC

//vector() randomvec = #91
void PF_randomvec (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	vec3_t temp;
	do
	{
		temp[0] = (rand() & 32767) * (2.0 / 32767.0) - 1.0;
		temp[1] = (rand() & 32767) * (2.0 / 32767.0) - 1.0;
		temp[2] = (rand() & 32767) * (2.0 / 32767.0) - 1.0;
	} while (DotProduct(temp, temp) >= 1);
	VectorCopy (temp, G_VECTOR(OFS_RETURN));
}

//EXTENSION: DP_QC_VECTORVECTORS

//void(vector dir) vectorvectors = #432
//Writes new values for v_forward, v_up, and v_right based on the given forward vector
void PF_vectorvectors (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	VectorCopy(G_VECTOR(OFS_PARM0), P_VEC(v_forward));
	VectorNormalize(P_VEC(v_forward));
	VectorVectors(P_VEC(v_forward), P_VEC(v_right), P_VEC(v_up));
}


//EXTENSION: DP_QC_MINMAXBOUND

//float(float a, floats) min = #94
void PF_min (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i;
	float f;

	if (*prinst->callargc == 2)
	{
		G_FLOAT(OFS_RETURN) = min(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
	}
	else if (*prinst->callargc >= 3)
	{
		f = G_FLOAT(OFS_PARM0);
		for (i = 1; i < *prinst->callargc; i++)
		{
			if (G_FLOAT((OFS_PARM0 + i * 3)) < f)
				f = G_FLOAT((OFS_PARM0 + i * 3));
		}
		G_FLOAT(OFS_RETURN) = f;
	}
	else
		PR_BIError("PF_min: must supply at least 2 floats\n");
}

//float(float a, floats) max = #95
void PF_max (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i;
	float f;

	if (*prinst->callargc == 2)
	{
		G_FLOAT(OFS_RETURN) = max(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
	}
	else if (*prinst->callargc >= 3)
	{
		f = G_FLOAT(OFS_PARM0);
		for (i = 1; i < *prinst->callargc; i++) {
			if (G_FLOAT((OFS_PARM0 + i * 3)) > f)
				f = G_FLOAT((OFS_PARM0 + i * 3));
		}
		G_FLOAT(OFS_RETURN) = f;
	}
	else
	{
		PR_BIError("PF_min: must supply at least 2 floats\n");
	}
}

//float(float minimum, float val, float maximum) bound = #96
void PF_bound (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (G_FLOAT(OFS_PARM1) > G_FLOAT(OFS_PARM2))
		G_FLOAT(OFS_RETURN) = G_FLOAT(OFS_PARM2);
	else if (G_FLOAT(OFS_PARM1) < G_FLOAT(OFS_PARM0))
		G_FLOAT(OFS_RETURN) = G_FLOAT(OFS_PARM0);
	else
		G_FLOAT(OFS_RETURN) = G_FLOAT(OFS_PARM1);
}

//EXTENSION: KRIMZON_SV_PARSECLIENTCOMMAND

//void(entity e, string s) clientcommand = #440
//executes a command string as if it came from the specified client
void PF_clientcommand (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	client_t *temp_client;
	int i;

	//find client for this entity
	i = NUM_FOR_EDICT(prinst, G_EDICT(prinst, OFS_PARM0)) - 1;
	if (i < 0 || i >= sv.allocated_client_slots)
		PR_BIError("PF_clientcommand: entity is not a client");

	temp_client = host_client;
	host_client = &svs.clients[i];
	if (host_client->state == cs_connected || host_client->state == cs_spawned)
		SV_ExecuteUserCommand (PF_VarString(prinst, OFS_PARM1, pr_globals), true);
	host_client = temp_client;
	if (host_client)
		sv_player = host_client->edict;
}







void PF_AdvanceFrame(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t *Ent;
	float Start,End,Result;
	
	Ent = PROG_TO_EDICT(prinst, pr_global_struct->self);
	Start = G_FLOAT(OFS_PARM0);
	End = G_FLOAT(OFS_PARM1);

	if ((Start<End&&(Ent->v.frame < Start || Ent->v.frame > End))||
		(Start>End&&(Ent->v.frame > Start || Ent->v.frame < End)))
	{ // Didn't start in the range
		Ent->v.frame = Start;
		Result = 0;
	}
	else if(Ent->v.frame == End)
	{  // Wrapping
		Ent->v.frame = Start;
		Result = 1;
	}
	else if(End>Start)
	{  // Regular Advance
		Ent->v.frame++;
		if (Ent->v.frame == End) 
			Result = 2;
		else 
			Result = 0;
	}
	else if(End<Start)
	{  // Reverse Advance
		Ent->v.frame--;
		if (Ent->v.frame == End)
			Result = 2;
		else
			Result = 0;
	}
	else
	{
		Ent->v.frame=End;
		Result = 1;
	}

	G_FLOAT(OFS_RETURN) = Result;
}

void PF_RewindFrame(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t *Ent;
	float Start,End,Result;
	
	Ent = PROG_TO_EDICT(prinst, pr_global_struct->self);
	Start = G_FLOAT(OFS_PARM0);
	End = G_FLOAT(OFS_PARM1);

	if (Ent->v.frame > Start || Ent->v.frame < End)
	{ // Didn't start in the range
		Ent->v.frame = Start;
		Result = 0;
	}
	else if(Ent->v.frame == End)
	{  // Wrapping
		Ent->v.frame = Start;
		Result = 1;
	}
	else
	{  // Regular Advance
		Ent->v.frame--;
		if (Ent->v.frame == End) Result = 2;
		else Result = 0;
	}

	G_FLOAT(OFS_RETURN) = Result;
}

#define WF_NORMAL_ADVANCE 0
#define WF_CYCLE_STARTED 1
#define WF_CYCLE_WRAPPED 2
#define WF_LAST_FRAME 3

void PF_advanceweaponframe (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t *ent;
	float startframe,endframe;
	float state;

	ent = PROG_TO_EDICT(prinst, pr_global_struct->self);
	startframe = G_FLOAT(OFS_PARM0);
	endframe = G_FLOAT(OFS_PARM1);

	if ((endframe > startframe && (ent->v.weaponframe > endframe || ent->v.weaponframe < startframe)) ||
	(endframe < startframe && (ent->v.weaponframe < endframe || ent->v.weaponframe > startframe)) )
	{
		ent->v.weaponframe=startframe;
		state = WF_CYCLE_STARTED;
	}
	else if(ent->v.weaponframe==endframe)
	{			  
		ent->v.weaponframe=startframe;
		state = WF_CYCLE_WRAPPED;
	}
	else
	{
		if (startframe > endframe)
			ent->v.weaponframe = ent->v.weaponframe - 1;
		else if (startframe < endframe)
			ent->v.weaponframe = ent->v.weaponframe + 1;

		if (ent->v.weaponframe==endframe)
			state = WF_LAST_FRAME;
		else 
			state = WF_NORMAL_ADVANCE;
	}

	G_FLOAT(OFS_RETURN) = state;
}


void PF_setclass (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float		NewClass;
	int			entnum;
	edict_t		*e;
	client_t	*client;
//	client_t	*old;
	char		temp[1024];
	
	entnum = G_EDICTNUM(prinst, OFS_PARM0);
	e = G_EDICT(prinst, OFS_PARM0);
	NewClass = G_FLOAT(OFS_PARM1);
	
	if (entnum < 1 || entnum > sv.allocated_client_slots)
	{
		Con_Printf ("tried to change class of a non-client\n");
		return;
	}

	client = &svs.clients[entnum-1];

	e->v.playerclass = NewClass;
	client->playerclass = NewClass;

	sprintf(temp,"%d",(int)NewClass);
	Info_SetValueForKey (client->userinfo, "playerclass", temp, MAX_INFO_STRING);
	client->sendinfo = true;
}

void PF_v_factor(progfuncs_t *prinst, struct globalvars_s *pr_globals)
// returns (v_right * factor_x) + (v_forward * factor_y) + (v_up * factor_z)
{
	float *range;
	vec3_t result;

	range = G_VECTOR(OFS_PARM0);

	result[0] = (P_VEC(v_right)[0] * range[0]) +
				(P_VEC(v_forward)[0] * range[1]) +
				(P_VEC(v_up)[0] * range[2]);

	result[1] = (P_VEC(v_right)[1] * range[0]) +
				(P_VEC(v_forward)[1] * range[1]) +
				(P_VEC(v_up)[1] * range[2]);

	result[2] = (P_VEC(v_right)[2] * range[0]) +
				(P_VEC(v_forward)[2] * range[1]) +
				(P_VEC(v_up)[2] * range[2]);

	VectorCopy (result, G_VECTOR(OFS_RETURN));
}

void PF_v_factorrange(progfuncs_t *prinst, struct globalvars_s *pr_globals)
// returns (v_right * factor_x) + (v_forward * factor_y) + (v_up * factor_z)
{
	float num,*minv,*maxv;
	vec3_t result,r2;

	minv = G_VECTOR(OFS_PARM0);
	maxv = G_VECTOR(OFS_PARM1);

	num = (rand ()&0x7fff) / ((float)0x7fff);
	result[0] = ((maxv[0]-minv[0]) * num) + minv[0];
	num = (rand ()&0x7fff) / ((float)0x7fff);
	result[1] = ((maxv[1]-minv[1]) * num) + minv[1];
	num = (rand ()&0x7fff) / ((float)0x7fff);
	result[2] = ((maxv[2]-minv[2]) * num) + minv[2];

	r2[0] = (P_VEC(v_right)[0] * result[0]) +
			(P_VEC(v_forward)[0] * result[1]) +
			(P_VEC(v_up)[0] * result[2]);

	r2[1] = (P_VEC(v_right)[1] * result[0]) +
			(P_VEC(v_forward)[1] * result[1]) +
			(P_VEC(v_up)[1] * result[2]);

	r2[2] = (P_VEC(v_right)[2] * result[0]) +
			(P_VEC(v_forward)[2] * result[1]) +
			(P_VEC(v_up)[2] * result[2]);

	VectorCopy (r2, G_VECTOR(OFS_RETURN));
}

char *T_GetString(int num);
void PF_plaque_draw(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char		*s;

	if (sv.demofile)
		return;
	
	if (G_FLOAT(OFS_PARM1) == 0)
		s = "";
	else
		s = T_GetString(G_FLOAT(OFS_PARM1)-1);

	pr_netprogfuncs = prinst;
	pr_netglob = pr_globals;

	if (G_FLOAT(OFS_PARM0) == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		if (!cl)
			return;
		ClientReliableWrite_Begin (cl, svc_centerprint, 2 + strlen(s));
		ClientReliableWrite_String (cl, s);
	}
	else
	{
		MSG_WriteByte (WriteDest(G_FLOAT(OFS_PARM0)), svc_centerprint);
		MSG_WriteString (WriteDest(G_FLOAT(OFS_PARM0)), s);
	}
}

void PF_movestep (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	vec3_t v;
	edict_t	*ent;
	int 	oldself;
	qboolean set_trace;

	ent = PROG_TO_EDICT(prinst, pr_global_struct->self);

	v[0] = G_FLOAT(OFS_PARM0);
	v[1] = G_FLOAT(OFS_PARM1);
	v[2] = G_FLOAT(OFS_PARM2);
	set_trace = G_FLOAT(OFS_PARM3);

// save program state, because SV_movestep may call other progs
	oldself = pr_global_struct->self;

	G_INT(OFS_RETURN) = SV_movestep (ent, v, false, true, set_trace?pr_globals:NULL);

// restore program state
	pr_global_struct->self = oldself;
}

void PF_concatv(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *in,*range;
	vec3_t result;

	in = G_VECTOR(OFS_PARM0);
	range = G_VECTOR(OFS_PARM1);

	VectorCopy (in, result);
	if (result[0] < -range[0]) result[0] = -range[0];
	if (result[0] > range[0]) result[0] = range[0];
	if (result[1] < -range[1]) result[1] = -range[1];
	if (result[1] > range[1]) result[1] = range[1];
	if (result[2] < -range[2]) result[2] = -range[2];
	if (result[2] > range[2]) result[2] = range[2];

	VectorCopy (result, G_VECTOR(OFS_RETURN));
}

void PF_matchAngleToSlope(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*actor;
	vec3_t v_forward, old_forward, old_right, new_angles2 = { 0, 0, 0 };
	float pitch, mod, dot;

	// OFS_PARM0 is used by PF_vectoangles below
	actor = G_EDICT(prinst, OFS_PARM1);

	AngleVectors(actor->v.angles, old_forward, old_right, P_VEC(v_up));

	PF_vectoangles(prinst, pr_globals);

	pitch = G_FLOAT(OFS_RETURN) - 90;

	new_angles2[1] = G_FLOAT(OFS_RETURN+1);

	AngleVectors(new_angles2, v_forward, P_VEC(v_right), P_VEC(v_up));

	mod = DotProduct(v_forward, old_right);

	if(mod<0)
		mod=1;
	else
		mod=-1;

	dot = DotProduct(v_forward, old_forward);

	actor->v.angles[0] = dot*pitch;
	actor->v.angles[2] = (1-fabs(dot))*pitch*mod;
}

void PF_starteffect(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Con_DPrintf("Start effect %i\n", (int)G_FLOAT(OFS_PARM0));
}

void PF_endeffect(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Con_DPrintf("Stop effect %i\n", (int)G_FLOAT(OFS_PARM0));
}

void PF_rain_go(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
}

void PF_StopSound(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
}

void PF_getstring(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *s = T_GetString(G_FLOAT(OFS_PARM0)-1);
	RETURN_PSTRING(s);
}

void PF_RegisterTEnt(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int arg;
	int i;
	int nettype = G_FLOAT(OFS_PARM0);
	char *effectname = PR_GetStringOfs(prinst, OFS_PARM1);
	if (sv.state != ss_loading)
	{
		PR_BIError ("PF_RegisterTEnt: Registration can only be done in spawn functions");
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}

	for (i = 0; i < 255; i++)
	{
		if (!*sv.customtents[i].particleeffecttype)
			break;
		if (!strcmp(effectname, sv.customtents[i].particleeffecttype))
			break;
	}
	if (i == 255)
	{
		Con_Printf("Too many custom effects\n");
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}

	Q_strncpyz(sv.customtents[i].particleeffecttype, effectname, sizeof(sv.customtents[i].particleeffecttype));
	sv.customtents[i].netstyle = nettype;

	arg = 2;
	if (nettype & CTE_STAINS)
	{
		VectorCopy(G_VECTOR(OFS_PARM0+arg*3), sv.customtents[i].stain);
		sv.customtents[i].radius = G_FLOAT(OFS_PARM1+arg*3);
		arg += 2;
	}
	if (nettype & CTE_GLOWS)
	{
		sv.customtents[i].dlightrgb[0] = G_FLOAT(OFS_PARM0+arg*3+0)*255;
		sv.customtents[i].dlightrgb[1] = G_FLOAT(OFS_PARM0+arg*3+1)*255;
		sv.customtents[i].dlightrgb[2] = G_FLOAT(OFS_PARM0+arg*3+2)*255;
		sv.customtents[i].dlightradius = G_FLOAT(OFS_PARM1+arg*3)/4;
		sv.customtents[i].dlighttime = G_FLOAT(OFS_PARM2+arg*3)*16;
		arg += 3;
		if (nettype & CTE_CHANNELFADE)
		{
			sv.customtents[i].dlightcfade[0] = G_FLOAT(OFS_PARM0+arg*3+0)*64;
			sv.customtents[i].dlightcfade[1] = G_FLOAT(OFS_PARM0+arg*3+1)*64;
			sv.customtents[i].dlightcfade[2] = G_FLOAT(OFS_PARM0+arg*3+2)*64;
			arg++; // we're out now..
		}
	}

	if (arg != *prinst->callargc)
		Con_Printf("Bad argument count\n");


	G_FLOAT(OFS_RETURN) = i;
}

void PF_CustomTEnt(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int type;
	int arg;
	float *org = G_VECTOR(OFS_PARM1);

	if (sv.multicast.cursize)
		SV_MulticastProtExt (org, MULTICAST_PVS, pr_global_struct->dimension_send, 0, PEXT_CUSTOMTEMPEFFECTS);	//do a multicast with the current buffer to all players who won't get the new effect.

	type = G_FLOAT(OFS_PARM0);
	if (type < 0 || type >= 255)
		return;

	MSG_WriteByte(&sv.multicast, svc_customtempent);
	MSG_WriteByte(&sv.multicast, type);
	MSG_WriteCoord(&sv.multicast, org[0]);
	MSG_WriteCoord(&sv.multicast, org[1]);
	MSG_WriteCoord(&sv.multicast, org[2]);

	type = sv.customtents[type].netstyle;
	arg = 2;
	if (type & CTE_CUSTOMCOUNT)
	{
		MSG_WriteByte(&sv.multicast, G_FLOAT(OFS_PARM0+arg*3));
		arg++;
	}
	if (type & CTE_CUSTOMDIRECTION)
	{
		MSG_WriteDir(&sv.multicast, G_VECTOR(OFS_PARM0+arg*3));
		arg++;
	}
	if (arg != *prinst->callargc)
		Con_Printf("PF_CusromTEnt: bad number of arguments for particle type\n");

	SV_MulticastProtExt (org, MULTICAST_PVS, pr_global_struct->dimension_send, PEXT_CUSTOMTEMPEFFECTS, 0);	//now send the new multicast to all that will.
}

void PF_Abort(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	svprogfuncs->AbortStack(svprogfuncs);
}

typedef struct qcstate_s {
	float resumetime;
	struct qcthread_s *thread;
	int self;
	int other;

	struct qcstate_s *next;
} qcstate_t;

qcstate_t *qcthreads;
void PR_RunThreads(void)
{
	struct globalvars_s *pr_globals;

	qcstate_t *state = qcthreads, *next;
	qcthreads = NULL;
	while(state)
	{
		next = state->next;

		if (state->resumetime > sv.time)
		{	//not time yet, reform origional list.
			state->next = qcthreads;
			qcthreads = state;
		}
		else
		{	//call it and forget it ever happened. The Sleep biltin will recreate if needed.
			pr_globals = PR_globals(svprogfuncs, PR_CURRENT);

			pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, EDICT_NUM(svprogfuncs, state->self));
			pr_global_struct->other = EDICT_TO_PROG(svprogfuncs, EDICT_NUM(svprogfuncs, state->other));
			G_FLOAT(OFS_RETURN) = 1;

			svprogfuncs->RunThread(svprogfuncs, state->thread);
			svprogfuncs->parms->memfree(state->thread);
			svprogfuncs->parms->memfree(state);
		}

		state = next;
	}
}

void PR_ClearThreads(void)
{
	qcstate_t *state = qcthreads, *next;
	qcthreads = NULL;
	while(state)
	{
		next = state->next;

		svprogfuncs->parms->memfree(state->thread);
		svprogfuncs->parms->memfree(state);

		state = next;
	}
}

void PF_Sleep(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	qcstate_t *state;
	struct qcthread_s *thread;
	float sleeptime;

	sleeptime = G_FLOAT(OFS_PARM0);

	thread = svprogfuncs->Fork(svprogfuncs);

	state = svprogfuncs->parms->memalloc(sizeof(qcstate_t));
	state->next = qcthreads;
	qcthreads = state;
	state->resumetime = sv.time + sleeptime;
	state->self = NUM_FOR_EDICT(svprogfuncs, PROG_TO_EDICT(svprogfuncs, pr_global_struct->self));
	state->other = NUM_FOR_EDICT(svprogfuncs, PROG_TO_EDICT(svprogfuncs, pr_global_struct->other));
	state->thread = thread;

	svprogfuncs->AbortStack(svprogfuncs);
}

void PF_Fork(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	qcstate_t *state;
	struct qcthread_s *thread;


	thread = svprogfuncs->Fork(svprogfuncs);

	state = svprogfuncs->parms->memalloc(sizeof(qcstate_t));
	state->next = qcthreads;
	qcthreads = state;
	state->resumetime = sv.time;
	state->self = NUM_FOR_EDICT(svprogfuncs, PROG_TO_EDICT(svprogfuncs, pr_global_struct->self));
	state->other = NUM_FOR_EDICT(svprogfuncs, PROG_TO_EDICT(svprogfuncs, pr_global_struct->other));
	state->thread = thread;
	
	PR_RunThreads();

	G_FLOAT(OFS_RETURN) = 0;
}


//QSG_DIMENSION_PLANES 
//helper function
//float(float number, float quantity) bitshift = #218;
void PF_bitshift(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int bitmask;
	int shift;

	bitmask = G_FLOAT(OFS_PARM0);
	shift = G_FLOAT(OFS_PARM1);

	if (shift < 0)
		bitmask >>= shift;
	else
		bitmask <<= shift;

	G_FLOAT(OFS_RETURN) = bitmask;
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(vector org) te_gunshot = #418;
void PF_te_gunshot(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int count;
	if (*svprogfuncs->callargc >= 2)
		count = G_FLOAT(OFS_PARM1);
	else
		count = 1;
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TE_GUNSHOT, count);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(vector org) te_spike = #419;
void PF_te_spike(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TE_SPIKE, 1);
}

void PF_te_lightningblood(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TE_LIGHTNINGBLOOD, 1);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(vector org) te_superspike = #420;
void PF_te_superspike(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TE_SUPERSPIKE, 1);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(vector org) te_explosion = #421;
void PF_te_explosion(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TE_EXPLOSION, 1);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(vector org) te_tarexplosion = #422;
void PF_te_tarexplosion(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TE_TAREXPLOSION, 1);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(vector org) te_wizspike = #423;
void PF_te_wizspike(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TE_WIZSPIKE, 1);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(vector org) te_knightspike = #424;
void PF_te_knightspike(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TE_KNIGHTSPIKE, 1);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(vector org) te_lavasplash = #425;
void PF_te_lavasplash(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TE_LAVASPLASH, 1);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(vector org) te_teleport = #426;
void PF_te_teleport(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TE_TELEPORT, 1);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(vector org, float color) te_explosion2 = #427;
void PF_te_explosion2(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	//FIXME: QW doesn't support TE_EXPLOSION2...
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TE_EXPLOSION, 1);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(entity own, vector start, vector end) te_lightning1 = #428;
void PF_te_lightning1(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_beam_tempentity(G_EDICTNUM(prinst, OFS_PARM0), G_VECTOR(OFS_PARM1), G_VECTOR(OFS_PARM2), TE_LIGHTNING1);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(entity own, vector start, vector end) te_lightning2 = #429;
void PF_te_lightning2(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_beam_tempentity(G_EDICTNUM(prinst, OFS_PARM0), G_VECTOR(OFS_PARM1), G_VECTOR(OFS_PARM2), TE_LIGHTNING2);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(entity own, vector start, vector end) te_lightning3 = #430;
void PF_te_lightning3(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_beam_tempentity(G_EDICTNUM(prinst, OFS_PARM0), G_VECTOR(OFS_PARM1), G_VECTOR(OFS_PARM2), TE_LIGHTNING3);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(entity own, vector start, vector end) te_beam = #431;
void PF_te_beam(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_beam_tempentity(-1 -G_EDICTNUM(prinst, OFS_PARM0), G_VECTOR(OFS_PARM1), G_VECTOR(OFS_PARM2), TE_LIGHTNING2);
}



void PF_ForceInfoKey(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*e;
	int		e1;
	char	*value;
	char	*key;

	e = G_EDICT(prinst, OFS_PARM0);
	e1 = NUM_FOR_EDICT(prinst, e);
	key = PR_GetStringOfs(prinst, OFS_PARM1);
	value = PR_GetStringOfs(prinst, OFS_PARM2);

	G_FLOAT(OFS_RETURN) = 0;

	if (e1 == 0)
	{	//localinfo
		Info_SetValueForKey (localinfo, key, value, MAX_LOCALINFO_STRING);
		G_FLOAT(OFS_RETURN) = 2;
	}
	else if (e1 <= sv.allocated_client_slots)
	{	//woo. we found a client.
		Info_SetValueForKey(svs.clients[e1-1].userinfo, key, value, MAX_INFO_STRING);


		SV_ExtractFromUserinfo (&svs.clients[e1-1]);

		MSG_WriteByte (&sv.reliable_datagram, svc_setinfo);
		MSG_WriteByte (&sv.reliable_datagram, e1-1);
		MSG_WriteString (&sv.reliable_datagram, key);
		MSG_WriteString (&sv.reliable_datagram, Info_ValueForKey(svs.clients[e1-1].userinfo, key));

		G_FLOAT(OFS_RETURN) = 1;
	}
	else
		Con_DPrintf("PF_ForceInfoKey: not world or client\n");
}

/*
=================
PF_setcolors

sets the color of a client and broadcasts the update to all connected clients

setcolors(clientent, value)
=================
*/

//from lh
void PF_setcolors (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	client_t	*client;
	int			entnum, i;
	char number[8];

	entnum = G_EDICTNUM(prinst, OFS_PARM0);
	i = G_FLOAT(OFS_PARM1);

	if (entnum < 1 || entnum > MAX_CLIENTS)
	{
		Con_Printf ("tried to setcolor a non-client\n");
		return;
	}

	client = &svs.clients[entnum-1];
	client->edict->v.team = (i & 15) + 1;
#ifdef NQPROT
	MSG_WriteByte (&sv.nqreliable_datagram, svc_updatecolors);
	MSG_WriteByte (&sv.nqreliable_datagram, entnum - 1);
	MSG_WriteByte (&sv.nqreliable_datagram, i);
#endif
	sprintf(number, "%i", i>>4);
	if (!strcmp(number, Info_ValueForKey(client->userinfo, "topcolor")))
	{
		Info_SetValueForKey(client->userinfo, "topcolor", number, MAX_INFO_STRING);
		MSG_WriteByte (&sv.reliable_datagram, svc_setinfo);
		MSG_WriteByte (&sv.reliable_datagram, entnum-1);
		MSG_WriteString (&sv.reliable_datagram, "topcolor");
		MSG_WriteString (&sv.reliable_datagram, number);
	}

	sprintf(number, "%i", i&15);
	if (!strcmp(number, Info_ValueForKey(client->userinfo, "bottomcolor")))
	{
		Info_SetValueForKey(client->userinfo, "bottomcolor", number, MAX_INFO_STRING);
		MSG_WriteByte (&sv.reliable_datagram, svc_setinfo);
		MSG_WriteByte (&sv.reliable_datagram, entnum-1);
		MSG_WriteString (&sv.reliable_datagram, "bottomcolor");
		MSG_WriteString (&sv.reliable_datagram, number);
	}
	SV_ExtractFromUserinfo (client);
}


static void ParamNegateFix ( float * xx, float * yy, int Zone )
{
	float x,y;
	x = xx[0];
	y = yy[0];

	if (Zone == SL_ORG_CC || SL_ORG_CW == Zone || SL_ORG_CE == Zone )
		y = y + 8000;

	if (Zone == SL_ORG_CC || SL_ORG_CN == Zone || SL_ORG_CS == Zone  )
		x = x + 8000;

	xx[0] = x;
	yy[0] = y;
}
void PF_ShowPic(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	progfuncs_t *progfuncs = prinst;
	char *slot	= PR_GetStringOfs(prinst, OFS_PARM0);
	char *picname = PR_GetStringOfs(prinst, OFS_PARM1);
	float x		= G_FLOAT(OFS_PARM2);
	float y		= G_FLOAT(OFS_PARM3);
	float zone	= G_FLOAT(OFS_PARM4);
	int entnum;

	ParamNegateFix( &x, &y, zone );

	if (*prinst->callargc==6)
	{	//to a single client
		entnum = G_EDICTNUM(prinst, OFS_PARM5)-1;
		if (entnum < 0 || entnum >= sv.allocated_client_slots)
			PR_RunError (prinst, "WriteDest: not a client");

		if (!(svs.clients[entnum].fteprotocolextensions & PEXT_SHOWPIC))
			return;	//need an extension for this. duh.

		ClientReliableWrite_Begin(&svs.clients[entnum], svc_showpic, 8 + strlen(slot)+strlen(picname));
		ClientReliableWrite_Byte(&svs.clients[entnum], zone);
		ClientReliableWrite_String(&svs.clients[entnum], slot);
		ClientReliableWrite_String(&svs.clients[entnum], picname);
		ClientReliableWrite_Short(&svs.clients[entnum], x);
		ClientReliableWrite_Short(&svs.clients[entnum], y);
	}
	else
	{
		//multicast instead of broadcast - 1: selective on the extensions. 2: reliable. 3: cleaner.
		MSG_WriteByte  (&sv.multicast, svc_showpic);
		MSG_WriteByte  (&sv.multicast, zone);//zone
		MSG_WriteString(&sv.multicast, slot);//label
		MSG_WriteString(&sv.multicast, picname);//picname
		MSG_WriteShort (&sv.multicast, x);
		MSG_WriteShort (&sv.multicast, y);

		SV_MulticastProtExt(vec3_origin, MULTICAST_ALL_R, FULLDIMENSIONMASK, PEXT_SHOWPIC, 0);
	}
};

void PF_HidePic(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	progfuncs_t *progfuncs = prinst;
	char *slot	= PR_GetStringOfs(prinst, OFS_PARM0);
	int entnum;

	if (*prinst->callargc==2)
	{	//to a single client
		entnum = G_EDICTNUM(prinst, OFS_PARM1)-1;
		if (entnum < 0 || entnum >= sv.allocated_client_slots)
			PR_RunError (prinst, "WriteDest: not a client");

		if (!(svs.clients[entnum].fteprotocolextensions & PEXT_SHOWPIC))
			return;	//need an extension for this. duh.

		ClientReliableWrite_Begin(&svs.clients[entnum], svc_hidepic, 2 + strlen(slot));
		ClientReliableWrite_String(&svs.clients[entnum], slot);
	}
	else
	{
		//easier to multicast
		MSG_WriteByte  (&sv.multicast, svc_hidepic);
		MSG_WriteString(&sv.multicast, slot);//lmp label

		SV_MulticastProtExt(vec3_origin, MULTICAST_ALL_R, FULLDIMENSIONMASK, PEXT_SHOWPIC, 0);
	}
};


void PF_MovePic(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	progfuncs_t *progfuncs = prinst;
	char *slot	= PR_GetStringOfs(prinst, OFS_PARM0);
	float x		= G_FLOAT(OFS_PARM1);
	float y		= G_FLOAT(OFS_PARM2);
	float zone	= G_FLOAT(OFS_PARM3);
	int entnum;

	ParamNegateFix( &x, &y, zone );

	if (*prinst->callargc==5)
	{	//to a single client
		entnum = G_EDICTNUM(prinst, OFS_PARM4)-1;
		if (entnum < 0 || entnum >= sv.allocated_client_slots)
			PR_RunError (prinst, "WriteDest: not a client");

		if (!(svs.clients[entnum].fteprotocolextensions & PEXT_SHOWPIC))
			return;	//need an extension for this. duh.

		ClientReliableWrite_Begin(&svs.clients[entnum], svc_movepic, 6 + strlen(slot));
		ClientReliableWrite_String(&svs.clients[entnum], slot);
		ClientReliableWrite_Byte(&svs.clients[entnum], zone);
		ClientReliableWrite_Short(&svs.clients[entnum], x);
		ClientReliableWrite_Short(&svs.clients[entnum], y);
	}
	else
	{
		//easier to multicast
		MSG_WriteByte  (&sv.multicast, svc_movepic);
		MSG_WriteString(&sv.multicast, slot);//lmp label
		MSG_WriteByte  (&sv.multicast, zone);
		MSG_WriteShort (&sv.multicast, x);
		MSG_WriteShort (&sv.multicast, y);

		SV_MulticastProtExt(vec3_origin, MULTICAST_ALL_R, FULLDIMENSIONMASK, PEXT_SHOWPIC, 0);
	}
};

void PF_ChangePic(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	progfuncs_t *progfuncs = prinst;
	char *slot	= PR_GetStringOfs(prinst, OFS_PARM0);
	char *newpic= PR_GetStringOfs(prinst, OFS_PARM1);
	int entnum;

	if (*prinst->callargc==3)
	{	//to a single client
		entnum = G_EDICTNUM(prinst, OFS_PARM2)-1;
		if (entnum < 0 || entnum >= sv.allocated_client_slots)
			PR_RunError (prinst, "WriteDest: not a client");

		if (!(svs.clients[entnum].fteprotocolextensions & PEXT_SHOWPIC))
			return;	//need an extension for this. duh.

		ClientReliableWrite_Begin(&svs.clients[entnum], svc_updatepic, 3 + strlen(slot)+strlen(newpic));
		ClientReliableWrite_String(&svs.clients[entnum], slot);
		ClientReliableWrite_String(&svs.clients[entnum], newpic);
	}
	else
	{
		MSG_WriteByte  (&sv.multicast, svc_updatepic);
		MSG_WriteString(&sv.multicast, slot);
		MSG_WriteString(&sv.multicast, newpic);

		SV_MulticastProtExt(vec3_origin, MULTICAST_ALL_R, FULLDIMENSIONMASK, PEXT_SHOWPIC, 0);
	}
}



BuiltinList_t BuiltinList[] = {				//nq	qw		h2		ebfs
	{"fixme",			PF_Fixme,			0,		0,		0},
	{"ignore",			PF_Ignore,			0,		0,		0},
	{"makevectors",		PF_makevectors,		1,		1,		1},	// void(entity e)	makevectors 		= #1;
	{"setorigin",		PF_setorigin,		2,		2,		2},	// void(entity e, vector o) setorigin	= #2;
	{"setmodel",		PF_setmodel,		3,		3,		3},	// void(entity e, string m) setmodel	= #3;
	{"setsize",			PF_setsize,			4,		4,		4},	// void(entity e, vector min, vector max) setsize = #4;
//	{"qtest_setabssize",NULL,				5},
	{"lightstylestatic",PF_lightstylestatic,0,		0,		5,		5},
	{"break",			PF_break,			6,		6,		6},	// void() break						= #6;
	{"random",			PF_random,			7,		7,		7},	// float() random						= #7;
	{"sound",			PF_sound,			8,		8,		8},	// void(entity e, float chan, string samp) sound = #8;
	{"normalize",		PF_normalize,		9,		9,		9},	// vector(vector v) normalize			= #9;
	{"error",			PF_error,			10,		10,		10},	// void(string e) error				= #10;
	{"objerror",		PF_objerror,		11,		11,		11},	// void(string e) objerror				= #11;
	{"vlen",			PF_vlen,			12,		12,		12},	// float(vector v) vlen				= #12;
	{"vectoyaw",		PF_vectoyaw,		13,		13,		13},	// float(vector v) vectoyaw		= #13;
	{"spawn",			PF_Spawn,			14,		14,		14},	// entity() spawn						= #14;
	{"remove",			PF_Remove,			15,		15,		15},	// void(entity e) remove				= #15;
	{"traceline",		PF_traceline,		16,		16,		16},	// float(vector v1, vector v2, float tryents) traceline = #16;
	{"checkclient",		PF_checkclient,		17,		17,		17},	// entity() clientlist					= #17;
	{"find",			PF_Find,			18,		18,		18},	// entity(entity start, .string fld, string match) find = #18;
	{"precache_sound",	PF_precache_sound,	19,		19,		19},	// void(string s) precache_sound		= #19;
	{"precache_model",	PF_precache_model,	20,		20,		20},	// void(string s) precache_model		= #20;
	{"stuffcmd",		PF_stuffcmd,		21,		21,		21},	// void(entity client, string s)stuffcmd = #21;
	{"findradius",		PF_findradius,		22,		22,		22},	// entity(vector org, float rad) findradius = #22;
	{"bprint",			PF_bprint,			23,		23,		23},	// void(string s) bprint				= #23;
//FIXME: distinguish between qw and nq parameters here?
	{"sprint",			PF_sprint,			24,		24,		24},	// void(entity client, string s) sprint = #24;
	{"dprint",			PF_dprint,			25,		25,		25},	// void(string s) dprint				= #25;
	{"ftos",			PF_ftos,			26,		26,		26},	// void(string s) ftos				= #26;
	{"vtos",			PF_vtos,			27,		27,		27},	// void(string s) vtos				= #27;
	{"coredump",		PF_coredump,		28,		28,		28},	//28
	{"traceon",			PF_traceon,			29,		29,		29},	//29
	{"traceoff",		PF_traceoff,		30,		30,		30},	//30
	{"eprint",			PF_eprint,			31,		31,		31},	//31 // void(entity e) debug print an entire entity
	{"walkmove",		PF_walkmove,		32,		32,		32},	//32 // float(float yaw, float dist) walkmove
	{"tracearea",		PF_traceboxh2,		0,		0,		33},	//33 // 
	{"writestring2",	PF_WriteString2,	0,		0,		0,		33},	//writestring but without the null terminator. makes things a little nicer.
//	{"qtest_flymove",	NULL,	33},	//vector
	{"droptofloor",		PF_droptofloor,		34,		34,		34},	//34
	{"lightstyle",		PF_lightstyle,		35,		35,		35},	//35
	{"rint",			PF_rint,			36,		36,		36},	//36
	{"floor",			PF_floor,			37,		37,		37},	//37
	{"ceil",			PF_ceil,			38,		38,		38},	//38
//	{"qtest_canreach",	NULL,				39},
	{"checkbottom",		PF_checkbottom,		40,		40,		40},	//40
	{"pointcontents",	PF_pointcontents,	41,		41,		41},	//41
//	{"qtest_stopsound",	NULL,				42},
	{"fabs",			PF_fabs,			43,		43,		43},	//43
	{"aim",				PF_aim,				44,		44,		44},	//44
	{"cvar",			PF_cvar,			45,		45,		45},	//45
	{"localcmd",		PF_localcmd,		46,		46,		46},	//46
	{"nextent",			PF_nextent,			47,		47,		47},	//47
	{"particle",		PF_particle,		48,		0,		48,		48},		//48 nq readded. This isn't present in QW protocol (fte added it back).
	{"changeyaw",		PF_changeyaw,		49,		49,		49},	//49
//	{"qtest_precacheitem", NULL,			50},	//1 1
	{"vhlen",			PF_vhlen,			0,		0,		50},	//49
	{"vectoangles",		PF_vectoangles,		51,		51,		51},	//51

	{"writebyte",		PF_WriteByte,		52,		52,		52},	//52
	{"writechar",		PF_WriteChar,		53,		53,		53},	//53
	{"writeshort",		PF_WriteShort,		54,		54,		54},	//54
	{"writelong",		PF_WriteLong,		55,		55,		55},	//55
	{"writecoord",		PF_WriteCoord,		56,		56,		56},	//56
	{"writeangle",		PF_WriteAngle,		57,		57,		57},	//57
	{"writestring",		PF_WriteString,		58,		58,		58},	//58
	{"writeentity",		PF_WriteEntity,		59,		59,		59},	//59

	{"dprintf",			PF_dprintf,			0,		0,		60},	//60

	{"sin",				PF_Sin,				0,		0,		62,		60},	//60
	{"cos",				PF_Cos,				0,		0,		61,		61},	//61
	{"sqrt",			PF_Sqrt,			0,		0,		84,		62},	//62

	{"AdvanceFrame",	PF_AdvanceFrame,	0,		0,		63,		0},
	{"dprintv",			PF_dprintv,			0,		0,		64,		0},	//64
	{"RewindFrame",		PF_RewindFrame,		0,		0,		65,		0},
	{"particleexplosion",PF_particleexplosion,0,	0,		81,		0},
	{"movestep",		PF_movestep,		0,		0,		82,		0},
	{"advanceweaponframe",PF_advanceweaponframe,0,	0,		83,		0},


	{"setclass",		PF_setclass,		0,		0,		66,		0},

	{"movetogoal",		SV_MoveToGoal,		67,		67,		67},	//67
	{"precache_file",	PF_precache_file,	68,		68,		68},	//68
	{"makestatic",		PF_makestatic,		69,		69,		69},	//69

	{"changelevel",		PF_changelevel,		70,		70,		70},	//70
	{"lightstylevalue",	PF_lightstylevalue,	0,		0,		71},	//70

	{"cvar_set",		PF_cvar_set,		72,		72,		72},	//72
	{"centerprint",		PF_centerprint,		73,		73,		73},	//73

	{"ambientsound",	PF_ambientsound,	74,		74,		74},	//74

	{"precache_model2",	PF_precache_model,	75,		75,		75},	//75
	{"precache_sound2",	PF_precache_sound,	76,		76,		76},	//76	// precache_sound2 is different only for qcc
	{"precache_file2",	PF_precache_file,	77,		77,		0},	//77

	{"setspawnparms",	PF_setspawnparms,	78,		78,		78},	//78
	{"plaque_draw",		PF_plaque_draw,		0,		0,		79},	//79
	{"logfrag",			PF_logfrag,			0,		79,		0},	//79

// Tomaz - QuakeC String Manipulation Begin
	{"tq_zone",			PF_dupstring,		0,		0,		0,		79},	//79
	{"tq_unzone",		PF_forgetstring,	0,		0,		0,		80},	//80
	//stof
	{"tq_strcat",		PF_strcat,			0,		0,		0,		82},	//82
	{"tq_substring",	PF_substring,		0,		0,		0,		83},	//83
	{"tq_stof",			PF_stof,			0,		0,		0,		84},	//84
	{"tq_stov",			PF_stov,			0,		0,		0,		85},	//85
// Tomaz - QuakeC String Manipulation End

// Tomaz - QuakeC File System Begin (new mods use frik_file instead)
	{"tq_fopen",		PF_fopen,			0,		0,		0,		86},// #86 float(string filename, float mode) fopen (QSG_FILE)
	{"tq_fclose",		PF_fclose,			0,		0,		0,		87},// #87 void(float fhandle) fclose (QSG_FILE)
	{"tq_fgets",		PF_fgets,			0,		0,		0,		88},// #88 string(float fhandle) fgets (QSG_FILE)
	{"tq_fputs",		PF_fputs,			0,		0,		0,		89},// #89 void(float fhandle, string s) fputs (QSG_FILE)
// Tomaz - QuakeC File System End

	{"rain_go",			PF_rain_go,			0,		0,		80},	//80

	{"infokey",			PF_infokey,			0,		80,		0,		80},	//80
	{"stof",			PF_stof,			0,		81,		0,		81},	//81
	{"multicast",		PF_multicast,		0,		82,		0,		0},	//82



//mvdsv (don't require ebfs usage in qw)
	{"executecommand",	PF_ExecuteCommand,	0,		83,		0,		83},	//83		//void() exec;   please don't use.
	{"mvdtokenize",		PF_Tokenize, 		0,		84,		0,		84},	//84			//void(string str) tokanize;
	{"mvdargc",			PF_ArgC,			0,		85,		0,		85},	//85			//float() argc;
	{"mvdargv",			PF_ArgV,			0,		86,		0,		86},	//86			//string(float num) argv;

//mvd command
	{"teamfield",		PF_teamfield,		0,		0,		0,		87},
	{"substr",			PF_substr,			0,		0,		0,		88},
	{"mvdstrcat",			PF_strcat,			0,		0,		0,		89},
	{"mvdstrlen",			PF_strlen,			0,		0,		0,		90},
	{"str2byte",		PF_str2byte,		0,		0,		0,		91},
	{"str2short",		PF_str2short,		0,		0,		0,		92},
	{"newstr",			PF_newstring,		0,		0,		0,		93},
	{"freestr",			PF_forgetstring,	0,		0,		0,		94},
	{"conprint",		PF_conprint,		0,		0,		0,		95},
	{"readcmd",			PF_readcmd,			0,		0,		0,		96},
	{"strcpy",			PF_strcpy,			0,		0,		0,		97},
	{"strstr",			PF_strstr,			0,		0,		0,		98},
	{"strncpy",			PF_strncpy,			0,		0,		0,		99},
	{"log",				PF_log,				0,		0,		0,		100},
//	{"redirectcmd",		PF_redirectcmd,		0,		0,		0,		101},
	{"calltimeofday",	PF_calltimeofday,	0,		0,		0,		102},
	{"forcedemoframe",	PF_forcedemoframe,	0,		0,		0,		103},
//end of mvdsv

	{"setpuzzlemodel",	PF_set_puzzle_model,0,		0,		87,		0},
	{"starteffect",		PF_starteffect,		0,		0,		88,		0},	//FIXME
	{"endeffect",		PF_endeffect,		0,		0,		89,		0},	//FIXME
	{"getstring",		PF_getstring,		0,		0,		92,		0},	//FIXME
	{"spawntemp",		PF_spawn_temp,		0,		0,		93,		0},

	{"v_factor",		PF_v_factor,		0,		0,		94,		0},
	{"v_factorrange",	PF_v_factorrange,	0,		0,		95,		0},

	{"precache_puzzle_model", PF_precache_puzzle_model,	0,		0,		90,		0},
	{"concatv",			PF_concatv,			0,		0,		91,		0},
	{"precache_sound3",	PF_precache_sound,	0,		0,		96,		0},
	{"precache_model3",	PF_precache_model,	0,		0,		97,		0},//please don't use...
	{"matchangletoslope",PF_matchAngleToSlope,0,	0,		99,		0},

	{"precache_sound4",	PF_precache_sound,	0,		0,		101,	0},
	{"precache_model4",	PF_precache_model,	0,		0,		102,	0},
	{"precache_file4",	PF_precache_file,	0,		0,		103,	0},
	{"stopsound4",		PF_StopSound,		0,		0,		106,	0},

	{"precache_model4",	PF_precache_model,	0,		0,		116,	0},//please don't use...
	{"precache_sound4",	PF_precache_sound,	0,		0,		117,	0},

	{"tracebox",		PF_traceboxdp,		0,		0,		0,		90},

	{"randomvec",		PF_randomvec,		0,		0,		0,		91},

	{"registercvar",	PF_registercvar,	0,		0,		0,		93},
	{"min",				PF_min,				0,		0,		0,		94},// #94 float(float a, floats) min (DP_QC_MINMAXBOUND)
	{"max",				PF_max,				0,		0,		0,		95},	// #95 float(float a, floats) max (DP_QC_MINMAXBOUND)
	{"bound",			PF_bound,			0,		0,		0,		96},	// #96 float(float minimum, float val, float maximum) bound (DP_QC_MINMAXBOUND)
	{"pow",				PF_pow,				0,		0,		0,		97},
	{"tj_cvar_string",	PF_cvar_string,		0,		0,		0,		97},	//telejano
//DP_QC_FINDFLOAT
	{"findfloat",		PF_FindFloat,		0,		0,		0,		98},	// #98 entity(entity start, float fld, float match) findfloat (DP_QC_FINDFLOAT)

	{"checkextension",	PF_checkextension,	99,		99,		0,		99},	// #99	//darkplaces system - query a string to see if the mod supports X Y and Z.
	{"builtin_find",	PF_builtinsupported,100,	100,	0,		100},	// #100	//per builtin system.
	{"anglemod",		PF_anglemod,		0,		0,		0,		102},
	{"cvar_string",		PF_cvar_string,		0,		0,		0,		103},

//TEI_SHOWLMP2
	{"showpic",			PF_ShowPic,			0,		0,		0,		104},
	{"hidepic",			PF_HidePic,			0,		0,		0,		105},
	{"movepic",			PF_MovePic,			0,		0,		0,		106},
	{"changepic",		PF_ChangePic,		0,		0,		0,		107},
	{"showpicent",		PF_ShowPic,			0,		0,		0,		108},
	{"hidepicent",		PF_HidePic,			0,		0,		0,		109},
//End TEU_SHOWLMP2

//frik file
	{"fopen",			PF_fopen,			0,		0,		0,		110},// #110 float(string filename, float mode) fopen (FRIK_FILE)
	{"fclose",			PF_fclose,			0,		0,		0,		111},// #111 void(float fhandle) fclose (FRIK_FILE)
	{"fgets",			PF_fgets,			0,		0,		0,		112},// #112 string(float fhandle) fgets (FRIK_FILE)
	{"fputs",			PF_fputs,			0,		0,		0,		113},// #113 void(float fhandle, string s) fputs (FRIK_FILE)
	{"strlen",			PF_strlen,			0,		0,		0,		114},// #114 float(string s) strlen (FRIK_FILE)
	{"strcat",			PF_strcat,			0,		0,		0,		115},// #115 string(string s1, string s2) strcat (FRIK_FILE)
	{"substring",		PF_substring,		0,		0,		0,		116},// #116 string(string s, float start, float length) substring (FRIK_FILE)
	{"stov",			PF_stov,			0,		0,		0,		117},// #117 vector(string s) stov (FRIK_FILE)
	{"strzone",			PF_dupstring,		0,		0,		0,		118},// #118 string(string s) strzone (FRIK_FILE)
	{"strunzone",		PF_forgetstring,	0,		0,		0,		119},// #119 string(string s) strunzone (FRIK_FILE)
//end frikfile

//these are telejano's
	{"cvar_setf",		PF_cvar_setf,		0,		0,		0,		176},// #176 void(string cvar, float val) cvar_setf
	{"localsound",		PF_LocalSound,		0,		0,		0,		177},//	#177
//end telejano

//fte extras
	{"getmodelindex",	PF_WeapIndex,		0,		0,		0,		200},
	{"externcall",		PF_externcall,		0,		0,		0,		201},
	{"addprogs",		PF_addprogs,		0,		0,		0,		202},
	{"externvalue",		PF_externvalue,		0,		0,		0,		203},
	{"externset",		PF_externset,		0,		0,		0,		204},
	{"externrefcall",	PF_externrefcall,	0,		0,		0,		205},
	{"instr",			PF_instr,			0,		0,		0,		206},
#ifdef Q2BSPS
	{"openportal",		PF_OpenPortal,		0,		0,		0,		207},
#endif

	{"RegisterTempEnt", PF_RegisterTEnt,	0,		0,		0,		208},
	{"CustomTempEnt",	PF_CustomTEnt,		0,		0,		0,		209},
	{"fork",			PF_Fork,			0,		0,		0,		210},
	{"abort",			PF_Abort,			0,		0,		0,		211},
	{"sleep",			PF_Sleep,			0,		0,		0,		212},
	{"forceinfokey",	PF_ForceInfoKey,	0,		0,		0,		213},
#ifdef SVCHAT
	{"chat",			PF_chat,			0,		0,		0,		214},// #214 void(string filename, float starttag, entity edict) SV_Chat (FTE_NPCCHAT)
#endif
//FTE_PEXT_HEXEN2
	{"particle2",		PF_particle2,		0,		0,		42,		215},
	{"particle3",		PF_particle3,		0,		0,		85,		216},
	{"particle4",		PF_particle4,		0,		0,		86,		217},

//QSG_DIMENSION_PLANES
	{"bitshift",		PF_bitshift,		0,		0,		0,		218},

//I guess this should go under DP_TE_STANDARDEFFECTBUILTINS...
	{"te_lightningblood",PF_te_lightningblood,	0,	0,		0,		219},// #219 te_lightningblood
//end fte extras

//DP extras
//DP_QC_COPYENTITY
	{"copyentity",		PF_copyentity,		0,		0,		0,		400},// #400 void(entity from, entity to) copyentity (DP_QC_COPYENTITY)
//DP_SV_SETCOLOR
	{"setcolors",		PF_setcolors,		0,		0,		0,		401},// #401 void(entity from, entity to) setcolors
//DP_QC_FINDCHAIN
	{"findchain",		PF_findchain,		0,		0,		0,		402},// #402 entity(string field, string match) findchain (DP_QC_FINDCHAIN)
//DP_QC_FINDCHAINFLOAT
	{"findfloatchain",	PF_findchainfloat,	0,		0,		0,		403},// #403 entity(float fld, float match) findchainfloat (DP_QC_FINDCHAINFLOAT)
	{"te_blood",		PF_te_blood,		0,		0,		0,		405},// #405 te_blood
//DP_TE_STANDARDEFFECTBUILTINS
	{"te_gunshot",		PF_te_gunshot,		0,		0,		0,		418},// #418 te_gunshot
	{"te_spike",		PF_te_spike,		0,		0,		0,		419},// #419 te_spike
	{"te_superspike",	PF_te_superspike,	0,		0,		0,		420},// #420 te_superspike
	{"te_explosion",	PF_te_explosion,	0,		0,		0,		421},// #421 te_explosion
	{"te_tarexplosion",	PF_te_tarexplosion,	0,		0,		0,		422},// #422 te_tarexplosion
	{"te_wizspike",		PF_te_wizspike,		0,		0,		0,		423},// #423 te_wizspike
	{"te_knightspike",	PF_te_knightspike,	0,		0,		0,		424},// #424 te_knightspike
	{"te_lavasplash",	PF_te_lavasplash,	0,		0,		0,		425},// #425 te_lavasplash
	{"te_teleport",		PF_te_teleport,		0,		0,		0,		426},// #426 te_teleport
	{"te_explosion2",	PF_te_explosion2,	0,		0,		0,		427},// #427 te_explosion2
	{"te_lightning1",	PF_te_lightning1,	0,		0,		0,		428},// #428 te_lightning1
	{"te_lightning2",	PF_te_lightning2,	0,		0,		0,		429},// #429 te_lightning2
	{"te_lightning3",	PF_te_lightning3,	0,		0,		0,		430},// #430 te_lightning3
	{"te_beam",			PF_te_beam,			0,		0,		0,		431},// #431 te_beam
//DP_QC_VECTORVECTORS
	{"vectorvectors",	PF_vectorvectors,	0,		0,		0,		432},// #432 void(vector dir) vectorvectors (DP_QC_VECTORVECTORS)
//KRIMZON_SV_PARSECLIENTCOMMAND
	{"clientcommand",	PF_clientcommand,	0,		0,		0,		440},// #440 void(entity e, string s) clientcommand (KRIMZON_SV_PARSECLIENTCOMMAND)
	{"tokenize",		PF_Tokenize,		0,		0,		0,		441},// #441 float(string s) tokenize (KRIMZON_SV_PARSECLIENTCOMMAND)
	{"argv",			PF_ArgV,			0,		0,		0,		442},// #442 string(float n) argv (KRIMZON_SV_PARSECLIENTCOMMAND
//end other peoples extras
	{NULL}

	//don't exceed 500 without modifing the size of pr_builtin
};

void PR_ResetBuiltins(progstype_t type)	//fix all nulls to PF_FIXME and add any extras that have a big number.
{
	int i;

	int builtincount[sizeof(pr_builtin)/sizeof(pr_builtin[0])];

	memset(pr_builtin, 0, sizeof(pr_builtin));

	if (type == PROG_QW)
	{
		for (i = 0; BuiltinList[i].name; i++)
		{
			if (BuiltinList[i].qwnum)
			{
				if (pr_builtin[BuiltinList[i].qwnum])
					Sys_Error("Cannot assign builtin %s, already taken\n", BuiltinList[i].name);
				pr_builtin[BuiltinList[i].qwnum] = BuiltinList[i].bifunc;
			}
		}
	}
	else if (type == PROG_H2)
	{
		for (i = 0; BuiltinList[i].name; i++)
		{
			if (BuiltinList[i].h2num)
			{
				if (pr_builtin[BuiltinList[i].h2num])
					Sys_Error("Cannot assign builtin %s, already taken\n", BuiltinList[i].name);
				pr_builtin[BuiltinList[i].h2num] = BuiltinList[i].bifunc;
			}
		}
	}
	else
	{
		for (i = 0; BuiltinList[i].name; i++)
		{
			if (BuiltinList[i].nqnum)
			{
				if (pr_builtin[BuiltinList[i].nqnum])
					Sys_Error("Cannot assign builtin %s, already taken\n", BuiltinList[i].name);
				pr_builtin[BuiltinList[i].nqnum] = BuiltinList[i].bifunc;
			}
		}
	}

	memset(builtincount, 0, sizeof(builtincount));

	for (i = 0; i < pr_numbuiltins; i++)	//clean up nulls
	{
		if (!pr_builtin[i])
		{
			pr_builtin[i] = PF_Fixme;
		}
		else
			builtincount[i]=100;
	}

	if (!pr_compatabilitytest.value)
	{
		for (i = 0; BuiltinList[i].name; i++)
		{
			builtincount[BuiltinList[i].ebfsnum]++;
		}
		for (i = 0; BuiltinList[i].name; i++)
		{
			if (BuiltinList[i].ebfsnum)
			{
				if (pr_builtin[BuiltinList[i].ebfsnum] == PF_Fixme && builtincount[BuiltinList[i].ebfsnum] == 1)
				{
					pr_builtin[BuiltinList[i].ebfsnum] = BuiltinList[i].bifunc;
					Con_DPrintf("Enabled %s\n", BuiltinList[i].name);
				}
				else if (pr_builtin[i] != BuiltinList[i].bifunc)
					Con_DPrintf("Not enabled %s\n", BuiltinList[i].name);
			}
		}
	}

	{
		char *builtinmap;
		int binum;
		builtinmap = COM_LoadTempFile("fte_bimap.txt");
		while(1)
		{
			builtinmap = COM_Parse(builtinmap);
			if (!builtinmap)
				break;
			binum = atoi(com_token);
			builtinmap = COM_Parse(builtinmap);

			for (i = 0; BuiltinList[i].name; i++)
			{
				if (!strcmp(BuiltinList[i].name, com_token))
				{
					pr_builtin[binum] = BuiltinList[i].bifunc;
					break;
				}
			}
			if (!BuiltinList[i].name)
				Con_Printf("Failed to map builtin %s to %i specified in fte_bimap.dat\n");
		}
	}


	for (i = 0; i < sizeof(QSG_Extensions)/sizeof(QSG_Extensions[0]); i++)
	{
		if (QSG_Extensions[i].enabled)
			*QSG_Extensions[i].enabled = false;
	}

	if (type == PROG_QW && pr_imitatemvdsv.value>0)	//pretend to be mvdsv for a bit.
	{
		PR_EnableEBFSBuiltin("teamfield");
		PR_EnableEBFSBuiltin("substr");
		PR_EnableEBFSBuiltin("mvdstrcat");
		PR_EnableEBFSBuiltin("mvdstrlen");
		PR_EnableEBFSBuiltin("str2byte");
		PR_EnableEBFSBuiltin("str2short");
		PR_EnableEBFSBuiltin("newstr");
		PR_EnableEBFSBuiltin("freestr");
		PR_EnableEBFSBuiltin("conprint");
		PR_EnableEBFSBuiltin("readcmd");
		PR_EnableEBFSBuiltin("strcpy");
		PR_EnableEBFSBuiltin("strstr");
		PR_EnableEBFSBuiltin("strncpy");
		PR_EnableEBFSBuiltin("log");
		PR_EnableEBFSBuiltin("redirectcmd");
		PR_EnableEBFSBuiltin("calltimeofday");
		PR_EnableEBFSBuiltin("forcedemoframe");
	}
}

builtin_t *pr_builtins = pr_builtin;
int pr_numbuiltins = sizeof(pr_builtin)/sizeof(pr_builtin[0]);


void PR_RegisterSVBuiltins(void)
{/*
	PR_RegisterBuiltin(svprogfuncs, "getmodelindex", &PF_WeapIndex);
	PR_RegisterBuiltin(svprogfuncs, "tracebox", &PF_traceline);
#ifdef Q2BSPS
	PR_RegisterBuiltin(svprogfuncs, "SetAreaPortalState", &PF_OpenPortal);
#endif
	PR_RegisterBuiltin(svprogfuncs, "logtext", &PF_logstring);

	PR_RegisterBuiltin(svprogfuncs, "newstring", &PF_newstring);
	PR_RegisterBuiltin(svprogfuncs, "forgetstring", &PF_forgetstring);
	PR_RegisterBuiltin(svprogfuncs, "strlen", &PF_strlen);
	PR_RegisterBuiltin(svprogfuncs, "strcat", &PF_strcat);
	PR_RegisterBuiltin(svprogfuncs, "strcatp", &PF_strcatp);
	PR_RegisterBuiltin(svprogfuncs, "ftosp", &PF_ftosp);
	PR_RegisterBuiltin(svprogfuncs, "redstring", &PF_redstring);

#ifdef USEBULLETENS
	PR_RegisterBuiltin(svprogfuncs, "bulleten", &PF_bulleten);
#endif
#ifdef SVCHAT
	PR_RegisterBuiltin(svprogfuncs, "chat", &PF_chat);
#endif

	PR_RegisterBuiltin(svprogfuncs, "cvar_string", &PF_cvar_string);


	PR_RegisterBuiltin(svprogfuncs, "_externcall", &PF_externcall);
	PR_RegisterBuiltin(svprogfuncs, "_addprogs", &PF_addprogs);
	PR_RegisterBuiltin(svprogfuncs, "_externvalue", &PF_externvalue);
	PR_RegisterBuiltin(svprogfuncs, "_externset", &PF_externset);
//	PR_RegisterBuiltin(svprogfuncs, "_externrefcall", &PF_externrefcall);
	PR_RegisterBuiltin(svprogfuncs, "instr", &PF_instr);
	PR_RegisterBuiltin(svprogfuncs, "temppointentity", &PF_tempentity);

//these are for nq progs
	PR_RegisterBuiltin(svprogfuncs, "logfrag", &PF_logfrag);
	PR_RegisterBuiltin(svprogfuncs, "infokey", &PF_infokey);
	PR_RegisterBuiltin(svprogfuncs, "stof", &PF_stof);*/
}

void PR_RegisterFields(void)	//it's just easier to do it this way.
{
#define fieldfloat(name) QC_RegisterFieldVar(svprogfuncs, ev_float, #name, (int)&((edict_t*)0)->v.name - (int)&((edict_t*)0)->v, -1)
#define fieldvector(name) QC_RegisterFieldVar(svprogfuncs, ev_vector, #name, (int)&((edict_t*)0)->v.name - (int)&((edict_t*)0)->v, -1)
#define fieldentity(name) QC_RegisterFieldVar(svprogfuncs, ev_entity, #name, (int)&((edict_t*)0)->v.name - (int)&((edict_t*)0)->v, -1)
#define fieldstring(name) QC_RegisterFieldVar(svprogfuncs, ev_string, #name, (int)&((edict_t*)0)->v.name - (int)&((edict_t*)0)->v, -1)
#define fieldfunction(name) QC_RegisterFieldVar(svprogfuncs, ev_function, #name, (int)&((edict_t*)0)->v.name - (int)&((edict_t*)0)->v, -1)

	fieldfloat(modelindex);
	fieldvector(absmin);
	fieldvector(absmax);
	fieldfloat(ltime);
	fieldfloat(lastruntime);
	fieldfloat(movetype);
	fieldfloat(solid);
	fieldvector(origin);
	fieldvector(oldorigin);
	fieldvector(velocity);
	fieldvector(angles);
	fieldvector(avelocity);
	fieldstring(classname);
	fieldstring(model);
	fieldfloat(frame);
	fieldfloat(skin);
	fieldfloat(effects);
	fieldvector(mins);
	fieldvector(maxs);
	fieldvector(size);
	fieldfunction(touch);
	fieldfunction(use);
	fieldfunction(think);
	fieldfunction(blocked);
	fieldfloat(nextthink);
	fieldentity(groundentity);
	fieldfloat(health);
	fieldfloat(frags);
	fieldfloat(weapon);
	fieldstring(weaponmodel);
	fieldfloat(weaponframe);
	fieldfloat(currentammo);
	fieldfloat(ammo_shells);
	fieldfloat(ammo_nails);
	fieldfloat(ammo_rockets);
	fieldfloat(ammo_cells);
	fieldfloat(items);
	fieldfloat(takedamage);
	fieldentity(chain);
	fieldfloat(deadflag);
	fieldvector(view_ofs);
	fieldfloat(button0);
	fieldfloat(button1);
	fieldfloat(button2);
	fieldfloat(button3);
	fieldfloat(button4);
	fieldfloat(button5);
	fieldfloat(button6);
	fieldfloat(button7);
	fieldfloat(button8);
	fieldfloat(impulse);
	fieldfloat(fixangle);
	fieldvector(v_angle);
	fieldstring(netname);
	fieldentity(enemy);
	fieldfloat(flags);
	fieldfloat(colormap);
	fieldfloat(team);
	fieldfloat(max_health);
	fieldfloat(teleport_time);
	fieldfloat(armortype);
	fieldfloat(armorvalue);
	fieldfloat(waterlevel);
	fieldfloat(watertype);
	fieldfloat(ideal_yaw);
	fieldfloat(yaw_speed);
	fieldentity(aiment);
	fieldentity(goalentity);
	fieldfloat(spawnflags);
	fieldstring(target);
	fieldstring(targetname);
	fieldfloat(dmg_take);
	fieldfloat(dmg_save);
	fieldentity(dmg_inflictor);
	fieldentity(owner);
	fieldvector(movedir);
	fieldfloat(sounds);
	fieldstring(noise);
	fieldstring(noise1);
	fieldstring(noise2);
	fieldstring(noise3);

//the rest are extras. (not in header)
	fieldfloat(gravity);		//standard extension
	fieldfloat(maxspeed);	//standard extension
	fieldfloat(items2);	//standard nq
	fieldfloat(scale);
	//fieldfloat(transparency);
	fieldfloat(alpha);
	fieldfloat(fatness);
	fieldentity(view2);
	fieldfloat(sendflags);
	fieldvector(movement);
	QC_RegisterFieldVar(svprogfuncs, ev_float, "buttonforward", (int)&((edict_t*)0)->v.movement[0] - (int)&((edict_t*)0)->v, -1);
	QC_RegisterFieldVar(svprogfuncs, ev_float, "buttonright", (int)&((edict_t*)0)->v.movement[1] - (int)&((edict_t*)0)->v, -1);
	QC_RegisterFieldVar(svprogfuncs, ev_float, "buttonup", (int)&((edict_t*)0)->v.movement[2] - (int)&((edict_t*)0)->v, -1);
	fieldfloat(fteflags);
	fieldfloat(vweapmodelindex);

	//dp extra fields
	fieldentity(nodrawtoclient);
	fieldentity(drawonlytoclient);

	//UDC_EXTEFFECT... yuckie
	QC_RegisterFieldVar(svprogfuncs, ev_float, "fieldcolor", (int)&((edict_t*)0)->v.seefcolour - (int)&((edict_t*)0)->v, -1);
	QC_RegisterFieldVar(svprogfuncs, ev_float, "fieldsizex", (int)&((edict_t*)0)->v.seefsizex - (int)&((edict_t*)0)->v, -1);
	QC_RegisterFieldVar(svprogfuncs, ev_float, "fieldsizey", (int)&((edict_t*)0)->v.seefsizey - (int)&((edict_t*)0)->v, -1);
	QC_RegisterFieldVar(svprogfuncs, ev_float, "fieldsizez", (int)&((edict_t*)0)->v.seefsizez - (int)&((edict_t*)0)->v, -1);
	QC_RegisterFieldVar(svprogfuncs, ev_float, "fieldoffset", (int)&((edict_t*)0)->v.seefoffset - (int)&((edict_t*)0)->v, -1);

//hexen 2 stuff
	fieldfloat(playerclass);
	fieldfloat(hull);

//stats - only a couple...
	fieldfloat(level);
	fieldfloat(intelligence);
	fieldfloat(experience);
	fieldfloat(wisdom);
	fieldfloat(strength);
	fieldfloat(dexterity);
	fieldfloat(bluemana);
	fieldfloat(greenmana);
	fieldfloat(max_mana);
	fieldfloat(experiance);
	fieldfloat(artifact_active);
	fieldfloat(artifact_low);
	fieldentity(cameramode);
	fieldfloat(rings_active);
	fieldfloat(rings_low);
	fieldfloat(armor_amulet);
	fieldfloat(armor_bracer);
	fieldfloat(armor_breastplate);
	fieldfloat(armor_helmet);
	fieldfloat(ring_flight);
	fieldfloat(ring_water);
	fieldfloat(ring_turning);
	fieldfloat(ring_regeneration);

	fieldstring(puzzle_inv1);
	fieldstring(puzzle_inv2);
	fieldstring(puzzle_inv3);
	fieldstring(puzzle_inv4);
	fieldstring(puzzle_inv5);
	fieldstring(puzzle_inv6);
	fieldstring(puzzle_inv7);
	fieldstring(puzzle_inv8);

	fieldfloat(hasted);
	fieldfloat(inventory);
	fieldfloat(cnt_torch);
	fieldfloat(cnt_h_boost);
	fieldfloat(cnt_sh_boost);
	fieldfloat(cnt_mana_boost);
	fieldfloat(cnt_teleport);
	fieldfloat(cnt_tome);
	fieldfloat(cnt_summon);
	fieldfloat(cnt_invisibility);
	fieldfloat(cnt_glyph);
	fieldfloat(cnt_haste);
	fieldfloat(cnt_blast);
	fieldfloat(cnt_polymorph);
	fieldfloat(cnt_flight);
	fieldfloat(cnt_cubeofforce);
	fieldfloat(cnt_invincibility);
//end of stats.

	fieldfloat(light_level);
	fieldfloat(abslight);
	fieldfloat(drawflags);
	fieldentity(movechain);
	fieldfunction(chainmoved);

	//QSG_DIMENSION_PLANES
	fieldfloat(dimension_see);
	fieldfloat(dimension_seen);
	fieldfloat(dimension_ghost);
	fieldfloat(dimension_ghost_alpha);
	fieldfloat(dimension_solid);
	fieldfloat(dimension_hit);

	if (pr_fixbrokenqccarrays.value)
		QC_RegisterFieldVar(svprogfuncs, 0, NULL, 0,0);
}


