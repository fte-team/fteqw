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

#include "qwsvdef.h"

#ifdef SQL
#include "win_mysql.h"
#endif

#define G_PROG G_FLOAT
#define Z_QC_TAG 2

#ifndef CLIENTONLY

#include "pr_common.h"

//okay, so these are a quick but easy hack
void ED_Print (struct progfuncs_s *progfuncs, struct edict_s *ed);
int PR_EnableEBFSBuiltin(char *name, int binum);

cvar_t	nomonsters = SCVAR("nomonsters", "0");
cvar_t	gamecfg = SCVAR("gamecfg", "0");
cvar_t	scratch1 = SCVAR("scratch1", "0");
cvar_t	scratch2 = SCVAR("scratch2", "0");
cvar_t	scratch3 = SCVAR("scratch3", "0");
cvar_t	scratch4 = SCVAR("scratch4", "0");
cvar_t	savedgamecfg = SCVARF("savedgamecfg", "0", CVAR_ARCHIVE);
cvar_t	saved1 = SCVARF("saved1", "0", CVAR_ARCHIVE);
cvar_t	saved2 = SCVARF("saved2", "0", CVAR_ARCHIVE);
cvar_t	saved3 = SCVARF("saved3", "0", CVAR_ARCHIVE);
cvar_t	saved4 = SCVARF("saved4", "0", CVAR_ARCHIVE);
cvar_t	temp1 = SCVARF("temp1", "0", CVAR_ARCHIVE);
cvar_t	noexit = SCVAR("noexit", "0");

cvar_t	pr_maxedicts = SCVARF("pr_maxedicts", "2048", CVAR_LATCH);
cvar_t	pr_imitatemvdsv = SCVARF("pr_imitatemvdsv", "0", CVAR_LATCH);
cvar_t	pr_fixbrokenqccarrays = SCVARF("pr_fixbrokenqccarrays", "1", CVAR_LATCH);

cvar_t	pr_no_playerphysics = SCVARF("pr_no_playerphysics", "0", CVAR_LATCH);

cvar_t	progs = SCVARF("progs", "", CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_NOTFROMSERVER);
cvar_t	qc_nonetaccess = SCVAR("qc_nonetaccess", "0");	//prevent write_... builtins from doing anything. This means we can run any mod, specific to any engine, on the condition that it also has a qw or nq crc.

cvar_t pr_overridebuiltins = SCVAR("pr_overridebuiltins", "1");

cvar_t pr_compatabilitytest = SCVARF("pr_compatabilitytest", "0", CVAR_LATCH);

cvar_t pr_ssqc_coreonerror = SCVAR("pr_coreonerror", "1");

cvar_t pr_droptofloorunits = SCVAR("pr_droptofloorunits", "");

cvar_t sv_gameplayfix_honest_tracelines = SCVAR("sv_gameplayfix_honest_tracelines", "1");
cvar_t sv_gameplayfix_blowupfallenzombies = SCVAR("sv_gameplayfix_blowupfallenzombies", "0");
extern cvar_t sv_gameplayfix_noairborncorpse;

cvar_t sv_addon[MAXADDONS];
char cvargroup_progs[] = "Progs variables";

#ifdef SQL
cvar_t sql_driver = SCVARF("sv_sql_driver", "mysql", CVAR_NOUNSAFEEXPAND);
cvar_t sql_host = SCVARF("sv_sql_host", "127.0.0.1", CVAR_NOUNSAFEEXPAND);
cvar_t sql_username = SCVARF("sv_sql_username", "", CVAR_NOUNSAFEEXPAND);
cvar_t sql_password = SCVARF("sv_sql_password", "", CVAR_NOUNSAFEEXPAND);
cvar_t sql_defaultdb = SCVARF("sv_sql_defaultdb", "", CVAR_NOUNSAFEEXPAND);

#define SQLCVAROPTIONS "SQL Defaults"
#endif

evalc_t evalc_idealpitch, evalc_pitch_speed;

int pr_teamfield;

void PR_ClearThreads(void);
void PR_fclose_progs(progfuncs_t*);
void PF_InitTempStrings(progfuncs_t *prinst);

#ifdef SQL
// SQL prototypes
void SQL_Init(void);
void SQL_KillServers(void);
void SQL_DeInit(void);
#endif

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

	qboolean obsolete;
} BuiltinList_t;
builtin_t pr_builtin[1024];
extern BuiltinList_t BuiltinList[];

struct {
	func_t ChatMessage;	//mvdsv parsing of 'say' commands
	func_t UserCmd;	//mvdsv
	func_t ConsoleCmd; //mvdsv
	func_t UserInfo_Changed;
	func_t localinfoChanged;

	func_t ParseClientCommand;	//KRIMZON_SV_PARSECLIENTCOMMAND
	func_t ParseConnectionlessPacket;	//FTE_QC_SENDPACKET

	func_t PausedTic;
	func_t ShouldPause;

	func_t RunClientCommand;	//EXT_CSQC_1

	func_t ClassChangeWeapon;//hexen2 support
} gfuncs;
func_t getplayerstat[MAX_CL_STATS];	//unnamed FTE extension
func_t getplayerstati[MAX_CL_STATS];//unnamed FTE extension
func_t SpectatorConnect;	//QW
func_t SpectatorThink;	//QW
func_t SpectatorDisconnect;	//QW

func_t SV_PlayerPhysicsQC;	//DP's DP_SV_PLAYERPHYSICS extension
func_t EndFrameQC;	//a common extension

qboolean pr_items2;	//hipnotic (or was it rogue?)

nqglobalvars_t realpr_nqglobal_struct;
nqglobalvars_t *pr_nqglobal_struct = &realpr_nqglobal_struct;

progfuncs_t *svprogfuncs;
progparms_t svprogparms;

progstype_t progstype;

void PR_RegisterFields(void);
void PR_ResetBuiltins(progstype_t type);

char *QC_ProgsNameForEnt(edict_t *ent)
{
	return "?";
}


int pr_edict_size;

void ED_Spawned (struct edict_s *ent, int loading)
{
#ifdef VM_Q1
	if (!ent->xv)
		ent->xv = (extentvars_t *)(ent->v+1);
#endif

	if (!loading || !ent->xv->Version)
	{
		ent->xv->dimension_see = 255;
		ent->xv->dimension_seen = 255;
		ent->xv->dimension_ghost = 0;
		ent->xv->dimension_solid = 255;
		ent->xv->dimension_hit = 255;

		ent->xv->Version = sv.csqcentversion[ent->entnum]+1;
		ent->xv->uniquespawnid = sv.csqcentversion[ent->entnum];
	}
}

pbool ED_CanFree (edict_t *ed)
{
	if (ed == sv.edicts)
	{
		if (developer.value)
		{
			Con_TPrintf(STL_CANTFREEWORLD);
			PR_StackTrace(svprogfuncs);
			*svprogfuncs->pr_trace = 1;
		}
		return false;
	}
	if (NUM_FOR_EDICT(svprogfuncs, ed) <= sv.allocated_client_slots)
	{
		Con_TPrintf(STL_CANTFREEPLAYERS);
		PR_StackTrace(svprogfuncs);
		*svprogfuncs->pr_trace = 1;
		return false;
	}
	SV_UnlinkEdict (ed);		// unlink from world bsp

	ed->v->model = 0;
	ed->v->takedamage = 0;
	ed->v->modelindex = 0;
	ed->v->colormap = 0;
	ed->v->skin = 0;
	ed->v->frame = 0;
	VectorClear (ed->v->origin);
	VectorClear (ed->v->angles);
	ed->v->nextthink = 0;
	ed->v->solid = 0;


	ed->v->classname = 0;

	if (pr_imitatemvdsv.value) 
	{
		ed->v->health = 0;
		ed->v->nextthink = -1;
		ed->v->impulse = 0;	//this is not true imitation, but it seems we need this line to get out of some ktpro infinate loops.
	}
	else
	{
		ed->v->think = 0;
	}

	ed->xv->SendEntity = 0;
	sv.csqcentversion[ed->entnum] = ed->xv->Version+1;

	return true;
}

void StateOp (progfuncs_t *prinst, float var, func_t func)
{
	stdentvars_t *vars = PROG_TO_EDICT(prinst, pr_global_struct->self)->v;
	if (progstype == PROG_H2)
		vars->nextthink = pr_global_struct->time+0.05;
	else
		vars->nextthink = pr_global_struct->time+0.1;
	vars->think = func;
	vars->frame = var;
}
void CStateOp (progfuncs_t *prinst, float startFrame, float endFrame, func_t currentfunc)
{
	stdentvars_t *vars = PROG_TO_EDICT(prinst, pr_global_struct->self)->v;

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
	stdentvars_t *vars = PROG_TO_EDICT(prinst, pr_global_struct->self)->v;

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
	stdentvars_t *vars = ed->v;
#ifdef PARANOID
	NUM_FOR_EDICT(ed); // Make sure it's in range
#endif
	vars->nextthink = pr_global_struct->time+var;
}

//int QCEditor (char *filename, int line, int nump, char **parms);
void QC_Clear(void);
builtin_t pr_builtin[];
extern int pr_numbuiltins;

int QCLibEditor(progfuncs_t *prinst, char *filename, int line, int nump, char **parms);
int QCEditor (progfuncs_t *prinst, char *filename, int line, int nump, char **parms)
{
#ifdef TEXTEDITOR
	static char oldfuncname[64];

	if (!parms)
		return QCLibEditor(prinst, filename, line, nump, parms);
	else
	{
		if (!nump && !strncmp(oldfuncname, *parms, sizeof(oldfuncname)))
		{
			Con_Printf("Executing %s: %s\n", *parms, filename);
			Q_strncpyz(oldfuncname, *parms, sizeof(oldfuncname));
		}
		return line;
	}
#else
	int i;
	char buffer[8192];
	char *r;
	vfsfile_t *f;

	SV_EndRedirect();

	if (line == -1)
		return -1;
	SV_EndRedirect();
	if (developer.value)
	{
		f = FS_OpenVFS(filename, "rb", FS_GAME);
	}
	else
		f = NULL;	//faster.
	if (!f)
	{
		Q_snprintfz(buffer, sizeof(buffer), "src/%s", filename);
		f = FS_OpenVFS(buffer, "rb", FS_GAME);
	}
	if (!f)
		Con_Printf("-%s - %i\n", filename, line);
	else
	{
		for (i = 0; i < line; i++)
		{
			VFS_GETS(f, buffer, sizeof(buffer));
		}
		if ((r = strchr(buffer, '\r')))
		{ r[0] = '\n';r[1]='\0';}
		Con_Printf("-%s", buffer);
		VFS_CLOSE(f);
	}
//PF_break(NULL);
	return line;
#endif
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
	svprogparms.edictsize = sizeof(edict_t);

	svprogparms.entspawn = ED_Spawned;//void (*entspawn) (struct edict_s *ent);	//ent has been spawned, but may not have all the extra variables (that may need to be set) set
	svprogparms.entcanfree = ED_CanFree;//bool (*entcanfree) (struct edict_s *ent);	//return true to stop ent from being freed
	svprogparms.stateop = StateOp;//void (*stateop) (float var, func_t func);
	svprogparms.cstateop = CStateOp;
	svprogparms.cwstateop = CWStateOp;
	svprogparms.thinktimeop = ThinkTimeOp;

	//used when loading a game
	svprogparms.builtinsfor = NULL;//builtin_t *(*builtinsfor) (int num);	//must return a pointer to the builtins that were used before the state was saved.
	svprogparms.loadcompleate = NULL;//void (*loadcompleate) (int edictsize);	//notification to reset any pointers.

	svprogparms.memalloc = PR_CB_Malloc;//void *(*memalloc) (int size);	//small string allocation	malloced and freed randomly
	svprogparms.memfree = PR_CB_Free;//void (*memfree) (void * mem);


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
	}
	PR_ClearThreads();
	PR_fclose_progs(svprogfuncs);

//	svs.numprogs = 0;

}

void PR_Deinit(void)
{
#ifdef SQL
	SQL_DeInit();
#endif

	PR_ClearThreads();
	if (svprogfuncs)
	{
		PR_fclose_progs(svprogfuncs);
		if (svprogfuncs->parms)
			CloseProgs(svprogfuncs);

		Z_FreeTags(Z_QC_TAG);
	}
	svprogfuncs=NULL;

	//clear out function pointers (so changing game modes cannot lead to confusions)
	memset(&gfuncs, 0, sizeof(gfuncs));
	memset(&getplayerstat, 0, sizeof(getplayerstat));
	memset(&getplayerstati, 0, sizeof(getplayerstati));
	SpectatorConnect = 0;
	SpectatorThink = 0;
	SpectatorDisconnect = 0;
}


#define QW_PROGHEADER_CRC	54730
#define NQ_PROGHEADER_CRC	5927
#define PREREL_PROGHEADER_CRC	26940	//prerelease
#define H2_PROGHEADER_CRC	38488	//basic hexen2
#define H2MP_PROGHEADER_CRC	26905	//hexen2 mission pack uses slightly different defs... *sigh*...
#define H2DEMO_PROGHEADER_CRC	14046	//I'm guessing this is from the original release or something

void PR_LoadGlabalStruct(void)
{
	static float writeonly;
	static float dimension_send_default;
	int i;
	int *v;
	nqglobalvars_t *pr_globals = pr_nqglobal_struct;
#define globalfloat(need,name) ((nqglobalvars_t*)pr_nqglobal_struct)->name = (float *)PR_FindGlobal(svprogfuncs, #name, 0);	if (need && !((nqglobalvars_t*)pr_globals)->name) SV_Error("Could not find \""#name"\" export in progs\n");
#define globalint(need,name) ((nqglobalvars_t*)pr_globals)->name = (int *)PR_FindGlobal(svprogfuncs, #name, 0);	if (need && !((nqglobalvars_t*)pr_globals)->name) SV_Error("Could not find export \""#name"\" in progs\n");
#define globalstring(need,name) ((nqglobalvars_t*)pr_globals)->name = (int *)PR_FindGlobal(svprogfuncs, #name, 0);	if (need && !((nqglobalvars_t*)pr_globals)->name) SV_Error("Could not find export \""#name"\" in progs\n");
#define globalvec(need,name) ((nqglobalvars_t*)pr_globals)->V_##name = (vec3_t *)PR_FindGlobal(svprogfuncs, #name, 0);	if (need && !((nqglobalvars_t*)pr_globals)->V_##name) SV_Error("Could not find export \""#name"\" in progs\n");
#define globalvec_(need,name) ((nqglobalvars_t*)pr_globals)->name = (vec3_t *)PR_FindGlobal(svprogfuncs, #name, 0);	if (need && !((nqglobalvars_t*)pr_globals)->name) SV_Error("Could not find export \""#name"\" in progs\n");
#define globalfunc(need,name) ((nqglobalvars_t*)pr_globals)->name = (func_t *)PR_FindGlobal(svprogfuncs, #name, 0);	if (need && !((nqglobalvars_t*)pr_globals)->name) SV_Error("Could not find export \""#name"\" in progs\n");
//			globalint(pad);
	globalint		(true, self);	//we need the qw ones, but any in standard quake and not quakeworld, we don't really care about.
	globalint		(true, other);
	globalint		(true, world);
	globalfloat		(true, time);
	globalfloat		(true, frametime);
	globalint		(false, newmis);	//not always in nq.
	globalfloat		(false, force_retouch);
	globalstring	(true, mapname);
	globalfloat		(false, deathmatch);
	globalfloat		(false, coop);
	globalfloat		(false, teamplay);
	globalfloat		(true, serverflags);
	globalfloat		(true, total_secrets);
	globalfloat		(true, total_monsters);
	globalfloat		(true, found_secrets);
	globalfloat		(true, killed_monsters);
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
	globalfloat		(false, trace_endcontents);
	globalfloat		(false, trace_surfaceflags);
	globalfloat		(false, cycle_wrapped);
	globalint		(false, msg_entity);
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

	
	globalfloat		(false, clientcommandframe);
	globalfloat		(false, input_timelength);
	globalvec_		(false, input_angles);
	globalvec_		(false, input_movevalues);
	globalfloat		(false, input_buttons);

	memset(&evalc_idealpitch, 0, sizeof(evalc_idealpitch));
	memset(&evalc_pitch_speed, 0, sizeof(evalc_pitch_speed));

	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		spawnparamglobals[i] = (float *)PR_FindGlobal(svprogfuncs, va("parm%i", i+1), 0);

	if (!((nqglobalvars_t*)pr_globals)->dimension_send)
	{	//make sure dimension send is always a valid pointer.
		((nqglobalvars_t*)pr_globals)->dimension_send = &dimension_send_default;
	}
	if (!((nqglobalvars_t*)pr_globals)->trace_endcontents)
	{	//make sure dimension send is always a valid pointer.
		((nqglobalvars_t*)pr_globals)->trace_endcontents = &writeonly;
	}
	if (!((nqglobalvars_t*)pr_globals)->trace_surfaceflags)
	{	//make sure dimension send is always a valid pointer.
		((nqglobalvars_t*)pr_globals)->trace_surfaceflags = &writeonly;
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

	gfuncs.ParseClientCommand = PR_FindFunction(svprogfuncs, "SV_ParseClientCommand", PR_ANY);
	gfuncs.ParseConnectionlessPacket = PR_FindFunction(svprogfuncs, "SV_ParseConnectionlessPacket", PR_ANY);

	gfuncs.UserInfo_Changed = PR_FindFunction(svprogfuncs, "UserInfo_Changed", PR_ANY);
	gfuncs.localinfoChanged = PR_FindFunction(svprogfuncs, "localinfoChanged", PR_ANY);
	gfuncs.ChatMessage = PR_FindFunction(svprogfuncs, "ChatMessage", PR_ANY);
	gfuncs.UserCmd = PR_FindFunction(svprogfuncs, "UserCmd", PR_ANY);
	gfuncs.ConsoleCmd = PR_FindFunction(svprogfuncs, "ConsoleCmd", PR_ANY);

	gfuncs.PausedTic = PR_FindFunction(svprogfuncs, "SV_PausedTic", PR_ANY);
	gfuncs.ShouldPause = PR_FindFunction(svprogfuncs, "SV_ShouldPause", PR_ANY);
	gfuncs.ClassChangeWeapon = PR_FindFunction(svprogfuncs, "ClassChangeWeapon", PR_ANY);
	gfuncs.RunClientCommand = PR_FindFunction(svprogfuncs, "SV_RunClientCommand", PR_ANY);

	if (pr_no_playerphysics.value)
		SV_PlayerPhysicsQC = 0;
	else
		SV_PlayerPhysicsQC = PR_FindFunction(svprogfuncs, "SV_PlayerPhysics", PR_ANY);
	EndFrameQC = PR_FindFunction (svprogfuncs, "EndFrame", PR_ANY);

	v = (int *)PR_globals(svprogfuncs, PR_CURRENT);
	QC_AddSharedVar(svprogfuncs, (int *)((nqglobalvars_t*)pr_nqglobal_struct)->self-v, 1);
	QC_AddSharedVar(svprogfuncs, (int *)((nqglobalvars_t*)pr_nqglobal_struct)->other-v, 1);
	QC_AddSharedVar(svprogfuncs, (int *)((nqglobalvars_t*)pr_nqglobal_struct)->time-v, 1);

	pr_items2 = !!PR_FindGlobal(svprogfuncs, "items2", 0);

	SV_ClearQCStats();

	if (progstype == PROG_H2)
	{
		SV_QCStatName(ev_float, "level", STAT_H2_LEVEL);
		SV_QCStatName(ev_float, "intelligence", STAT_H2_INTELLIGENCE);
		SV_QCStatName(ev_float, "wisdom", STAT_H2_WISDOM);
		SV_QCStatName(ev_float, "strength", STAT_H2_STRENGTH);
		SV_QCStatName(ev_float, "dexterity", STAT_H2_DEXTERITY);
		SV_QCStatName(ev_float, "bluemana", STAT_H2_BLUEMANA);
		SV_QCStatName(ev_float, "greenmana", STAT_H2_GREENMANA);
		SV_QCStatName(ev_float, "experience", STAT_H2_EXPERIENCE);
		SV_QCStatName(ev_float, "cnt_torch", STAT_H2_CNT_TORCH);
		SV_QCStatName(ev_float, "cnt_h_boost", STAT_H2_CNT_H_BOOST);
		SV_QCStatName(ev_float, "cnt_sh_boost", STAT_H2_CNT_SH_BOOST);
		SV_QCStatName(ev_float, "cnt_mana_boost", STAT_H2_CNT_MANA_BOOST);
		SV_QCStatName(ev_float, "cnt_teleport", STAT_H2_CNT_TELEPORT);
		SV_QCStatName(ev_float, "cnt_tome", STAT_H2_CNT_TOME);
		SV_QCStatName(ev_float, "cnt_summon", STAT_H2_CNT_SUMMON);
		SV_QCStatName(ev_float, "cnt_invisibility", STAT_H2_CNT_INVISIBILITY);
		SV_QCStatName(ev_float, "cnt_glyph", STAT_H2_CNT_GLYPH);
		SV_QCStatName(ev_float, "cnt_haste", STAT_H2_CNT_HASTE);
		SV_QCStatName(ev_float, "cnt_blast", STAT_H2_CNT_BLAST);
		SV_QCStatName(ev_float, "cnt_polymorph", STAT_H2_CNT_POLYMORPH);
		SV_QCStatName(ev_float, "cnt_flight", STAT_H2_CNT_FLIGHT);
		SV_QCStatName(ev_float, "cnt_cubeofforce", STAT_H2_CNT_CUBEOFFORCE);
		SV_QCStatName(ev_float, "cnt_invincibility", STAT_H2_CNT_INVINCIBILITY);
		SV_QCStatName(ev_float, "artifact_active", STAT_H2_ARTIFACT_ACTIVE);
		SV_QCStatName(ev_float, "artifact_low", STAT_H2_ARTIFACT_LOW);
		SV_QCStatName(ev_float, "movetype", STAT_H2_MOVETYPE);
		SV_QCStatName(ev_entity, "cameramode", STAT_H2_CAMERAMODE);
		SV_QCStatName(ev_float, "hasted", STAT_H2_HASTED);
		SV_QCStatName(ev_float, "inventory", STAT_H2_INVENTORY);
		SV_QCStatName(ev_float, "rings_active", STAT_H2_RINGS_ACTIVE);

		SV_QCStatName(ev_float, "rings_low", STAT_H2_RINGS_LOW);
		SV_QCStatName(ev_float, "armor_amulet", STAT_H2_ARMOUR2);
		SV_QCStatName(ev_float, "armor_bracer", STAT_H2_ARMOUR4);
		SV_QCStatName(ev_float, "armor_breastplate", STAT_H2_ARMOUR3);
		SV_QCStatName(ev_float, "armor_helmet", STAT_H2_ARMOUR1);
		SV_QCStatName(ev_float, "ring_flight", STAT_H2_FLIGHT_T);
		SV_QCStatName(ev_float, "ring_water", STAT_H2_WATER_T);
		SV_QCStatName(ev_float, "ring_turning", STAT_H2_TURNING_T);
		SV_QCStatName(ev_float, "ring_regeneration", STAT_H2_REGEN_T);
		SV_QCStatName(ev_string, "puzzle_inv1", STAT_H2_PUZZLE1);
		SV_QCStatName(ev_string, "puzzle_inv2", STAT_H2_PUZZLE2);
		SV_QCStatName(ev_string, "puzzle_inv3", STAT_H2_PUZZLE3);
		SV_QCStatName(ev_string, "puzzle_inv4", STAT_H2_PUZZLE4);
		SV_QCStatName(ev_string, "puzzle_inv5", STAT_H2_PUZZLE5);
		SV_QCStatName(ev_string, "puzzle_inv6", STAT_H2_PUZZLE6);
		SV_QCStatName(ev_string, "puzzle_inv7", STAT_H2_PUZZLE7);
		SV_QCStatName(ev_string, "puzzle_inv8", STAT_H2_PUZZLE8);
		SV_QCStatName(ev_float, "max_health", STAT_H2_MAXHEALTH);
		SV_QCStatName(ev_float, "max_mana", STAT_H2_MAXMANA);
		SV_QCStatName(ev_float, "flags", STAT_H2_FLAGS);
		SV_QCStatName(ev_float, "playerclass", STAT_H2_PLAYERCLASS);
	}
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
		num = PR_LoadProgs (svprogfuncs, name, QW_PROGHEADER_CRC, NULL, 0);
	else if (progstype == PROG_NQ)
		num = PR_LoadProgs (svprogfuncs, name, NQ_PROGHEADER_CRC, NULL, 0);
	else if (progstype == PROG_UNKNOWN)
		num = PR_LoadProgs (svprogfuncs, name, 0, NULL, 0);
	else //if (progstype == PROG_NONE)
	{
		progstype = PROG_QW;
		num = PR_LoadProgs (svprogfuncs, name, QW_PROGHEADER_CRC, NULL, 0);
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
				if (num == -1)
					num = PR_LoadProgs (svprogfuncs, name, H2DEMO_PROGHEADER_CRC, NULL, 0);
				if (num == -1)	//don't commit if bad.
				{
					progstype = PROG_PREREL;
					num = PR_LoadProgs (svprogfuncs, name, PREREL_PROGHEADER_CRC, NULL, 0);
					if (num == -1)
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
		}

		if (num != -1)
		{
			PR_LoadGlabalStruct();
			switch(progstype)
			{
			case PROG_QW:
				Con_DPrintf("Using QW progs\n");
				break;
			case PROG_NQ:
				Con_DPrintf("Using NQ progs\n");
				break;
			case PROG_H2:
				Con_DPrintf("Using H2 progs\n");
				break;
			case PROG_PREREL:
				Con_DPrintf("Using prerelease progs\n");
				break;
			default:
				Con_DPrintf("Using unknown progs, assuming NQ\n");
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
	{
		PF_InitTempStrings(svprogfuncs);
		PR_ResetBuiltins(progstype);
	}

	if ((f = PR_FindFunction (svprogfuncs, "VersionChat", num )))
	{
		pr_globals = PR_globals(svprogfuncs, num);
		G_FLOAT(OFS_PARM0) = build_number();
		PR_ExecuteProgram (svprogfuncs, f);

		fl = G_FLOAT(OFS_RETURN);
		if (fl < 0)
			SV_Error ("PR_LoadProgs: progs.dat is not compatible with EXE version");
		else if ((int) (fl) != (int) (build_number()))
			Con_DPrintf("Warning: Progs may not be fully compatible\n (%4.2f != %i)\n", fl, build_number());
	}

	if ((f = PR_FindFunction (svprogfuncs, "FTE_init", num )))
	{
		pr_globals = PR_globals(svprogfuncs, num);
		G_FLOAT(OFS_PARM0) = build_number();
		PR_ExecuteProgram (svprogfuncs, f);
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
		Q_SetProgsParms(false);
		PR_Configure(svprogfuncs, -1, MAX_PROGS);
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
	char *argv[64] = {"", "-src", "src", "-srcfile", "qwprogs.src"};

	if (Cmd_Argc()>2)
	{
		for (argc = 0; argc < Cmd_Argc(); argc++)
			argv[argc] = Cmd_Argv(argc);
	}
	else
	{
		//override the source name
		if (Cmd_Argc() == 2)
		{
			argv[4] = Cmd_Argv(1);
			argc = 5;
		}
		if (!FS_FLocateFile(va("%s/%s", argv[2], argv[4]), FSLFRT_IFFOUND, NULL))
		{
			//try the qc path
			argv[2] = "qc";
		}
		if (!FS_FLocateFile(va("%s/%s", argv[2], argv[4]), FSLFRT_IFFOUND, NULL))
		{
			//try the progs path (yeah... gah)
			argv[2] = "progs";
		}
		if (!FS_FLocateFile(va("%s/%s", argv[2], argv[4]), FSLFRT_IFFOUND, NULL))
		{
			//try the gamedir path
			argv[1] = argv[3];
			argv[2] = argv[4];
			argc -= 2;
		}
	}

	if (!svprogfuncs)
		Q_SetProgsParms(true);
	if (PR_StartCompile(svprogfuncs, argc, argv))
		while(PR_ContinueCompile(svprogfuncs));

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


	PR_Configure(svprogfuncs, -1, MAX_PROGS);
	PR_RegisterFields();
	PR_InitEnts(svprogfuncs, sv.max_edicts);

	pr_edict_size=svprogfuncs->load_ents(svprogfuncs, s, 0);


	PR_LoadGlabalStruct();

	pr_global_struct->time = sv.physicstime;


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

void PR_SSCoreDump_f(void)
{
	if (!svprogfuncs)
	{
		Con_Printf("Progs not running, you need to start a server first\n");
		return;
	}

	{
		int size = 1024*1024*8;
		char *buffer = BZ_Malloc(size);
		svprogfuncs->save_ents(svprogfuncs, buffer, &size, 3);
		COM_WriteFile("ssqccore.txt", buffer, size);
		BZ_Free(buffer);
	}
}

void PR_SVExtensionList_f(void);

/*
#ifdef _DEBUG
void QCLibTest(void)
{
	int size = 1024*1024*8;
	char *buffer = BZ_Malloc(size);
	svprogfuncs->save_ents(svprogfuncs, buffer, &size, 3);
	COM_WriteFile("ssqccore.txt", buffer, size);
	BZ_Free(buffer);

	PR_TestForWierdness(svprogfuncs);
}
#endif
*/
typedef char char32[32];
char32 sv_addonname[MAXADDONS];
void PR_Init(void)
{
	int i;

	PF_Common_RegisterCvars();

	Cmd_AddCommand ("breakpoint", PR_BreakPoint_f);
	Cmd_AddCommand ("decompile", PR_Decompile_f);
	Cmd_AddCommand ("compile", PR_Compile_f);
	Cmd_AddCommand ("applycompile", PR_ApplyCompilation_f);
	Cmd_AddCommand ("coredump_ssqc", PR_SSCoreDump_f);

	Cmd_AddCommand ("extensionlist_ssqc", PR_SVExtensionList_f);

/*
#ifdef _DEBUG
	Cmd_AddCommand ("svtestprogs", QCLibTest);
#endif
*/
	Cvar_Register(&pr_maxedicts, cvargroup_progs);
	Cvar_Register(&pr_imitatemvdsv, cvargroup_progs);
	Cvar_Register(&pr_fixbrokenqccarrays, cvargroup_progs);
	Cvar_Register(&pr_no_playerphysics, cvargroup_progs);

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

	Cvar_Register (&pr_ssqc_coreonerror, cvargroup_progs);

	Cvar_Register (&pr_droptofloorunits, cvargroup_progs);

	Cvar_Register (&sv_gameplayfix_honest_tracelines, cvargroup_progs);
	Cvar_Register (&sv_gameplayfix_blowupfallenzombies, cvargroup_progs);
	Cvar_Register (&sv_gameplayfix_noairborncorpse, cvargroup_progs);

#ifdef SQL
	SQL_Init();
#endif
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
	PR_Configure(svprogfuncs, -1, MAX_PROGS);

	PR_RegisterFields();

	num = svs.numprogs;
	svs.numprogs=0;

	d1 = COM_FDepthFile("progs.dat", true);
	d2 = COM_FDepthFile("qwprogs.dat", true);
	if (d2 != 0x7fffffff)
		d2 += (!deathmatch.value * 3);
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
		COM_DefaultExtension(addons, ".dat", sizeof(addons));
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
		SV_Error("Couldn't open or compile progs\n");

#ifdef SQL
	SQL_KillServers(); // TODO: is this the best placement for this?
#endif

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
						G_INT(OFS_PARM0) = (int)PR_TempString(svprogfuncs, as);
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

	switch (sv.worldmodel->fromgame)	//spawn functions for - spawn funcs still come from the first progs found.
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

	default:
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
						G_INT(OFS_PARM0) = (int)PR_TempString(svprogfuncs, as);
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
					G_INT(OFS_PARM0) = (int)PR_TempString(svprogfuncs, sv_addon[i2].string);
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
	if (sv.max_edicts > MAX_EDICTS)
		sv.max_edicts = MAX_EDICTS;
	pr_edict_size = PR_InitEnts(svprogfuncs, sv.max_edicts);
}

qboolean PR_QCChat(char *text, int say_type)
{
	globalvars_t *pr_globals;

	if (!gfuncs.ChatMessage || pr_imitatemvdsv.value >= 0)
		return false;

	pr_globals = PR_globals(svprogfuncs, PR_CURRENT);

	G_INT(OFS_PARM0) = (int)PR_SetString(svprogfuncs, text);
	G_FLOAT(OFS_PARM1) = say_type;
	PR_ExecuteProgram (svprogfuncs, gfuncs.ChatMessage);

	if (G_FLOAT(OFS_RETURN))
		return true;
	return false;
}

qboolean PR_GameCodePausedTic(float pausedtime)
{	//notications to the gamecode that the server is paused.
	globalvars_t *pr_globals;

	if (!svprogfuncs || !gfuncs.ShouldPause)
		return false;

	pr_globals = PR_globals(svprogfuncs, PR_CURRENT);

	G_FLOAT(OFS_PARM0) = pausedtime;
	PR_ExecuteProgram (svprogfuncs, gfuncs.PausedTic);

	if (G_FLOAT(OFS_RETURN))
		return true;
	return false;
}
qboolean PR_ShouldTogglePause(client_t *initiator, qboolean newpaused)
{
	globalvars_t *pr_globals;

	if (!svprogfuncs || !gfuncs.ShouldPause)
		return true;

	pr_globals = PR_globals(svprogfuncs, PR_CURRENT);

	if (initiator)
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, initiator->edict);
	else
		pr_global_struct->self = 0;
	G_FLOAT(OFS_PARM0) = newpaused;
	PR_ExecuteProgram (svprogfuncs, gfuncs.ShouldPause);

	return G_FLOAT(OFS_RETURN);
}

qboolean PR_GameCodePacket(char *s)
{
	globalvars_t *pr_globals;
	int i;
	client_t *cl;
	char adr[MAX_ADR_SIZE];

	if (!gfuncs.ParseConnectionlessPacket)
		return false;
	if (!svprogfuncs)
		return false;

	pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
	pr_global_struct->time = sv.physicstime;

	// check for packets from connected clients
	pr_global_struct->self = 0;
	for (i=0, cl=svs.clients ; i<MAX_CLIENTS ; i++,cl++)
	{
		if (cl->state == cs_free)
			continue;
		if (!NET_CompareAdr (net_from, cl->netchan.remote_address))
			continue;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, cl->edict);
		break;
	}



	G_INT(OFS_PARM0) = PR_TempString(svprogfuncs, NET_AdrToString (adr, sizeof(adr), net_from));

	G_INT(OFS_PARM1) = PR_TempString(svprogfuncs, s);
	PR_ExecuteProgram (svprogfuncs, gfuncs.ParseConnectionlessPacket);
	return G_FLOAT(OFS_RETURN);
}

qboolean PR_KrimzonParseCommand(char *s)
{
	globalvars_t *pr_globals;

#ifdef Q2SERVER
	if (ge)
		return false;
#endif
	if (!svprogfuncs)
		return false;

	if (gfuncs.ParseClientCommand)
	{	//the QC is expected to send it back to use via a builtin.

		pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
		pr_global_struct->time = sv.physicstime;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);

		G_INT(OFS_PARM0) = (int)PR_TempString(svprogfuncs, s);
		PR_ExecuteProgram (svprogfuncs, gfuncs.ParseClientCommand);
		return true;
	}

	return false;
}
qboolean PR_UserCmd(char *s)
{
	globalvars_t *pr_globals;
#ifdef Q2SERVER
	if (ge)
	{
		SV_BeginRedirect (RD_CLIENT, host_client->language);
		ge->ClientCommand(host_client->q2edict);
		SV_EndRedirect ();
		return true;	//the dll will convert it to chat.
	}
#endif
	if (!svprogfuncs)
		return false;

	if (gfuncs.ParseClientCommand)
	{	//the QC is expected to send it back to use via a builtin.

		pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
		pr_global_struct->time = sv.physicstime;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);

		G_INT(OFS_PARM0) = (int)PR_TempString(svprogfuncs, s);
		PR_ExecuteProgram (svprogfuncs, gfuncs.ParseClientCommand);
		return true;
	}

#ifdef VM_Q1
	if (svs.gametype == GT_Q1QVM)
	{
		pr_global_struct->time = sv.physicstime;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
		Q1QVM_ClientCommand();
		return true;	//qvm can print something if it wants
	}
#endif

	if (gfuncs.UserCmd && pr_imitatemvdsv.value >= 0)
	{	//we didn't recognise it. see if the mod does.

		//ktpro bug warning:
		//admin + judge. I don't know the exact rules behind this bug, so I just ban the entire command
		//I can't be arsed detecting ktpro specifically, so assume we're always running ktpro
		
		if (!strncmp(s, "admin", 5) || !strncmp(s, "judge", 5))
		{
			Con_Printf("Blocking potentially unsafe ktpro command: %s\n", s);
			return true;
		}

		pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
		pr_global_struct->time = sv.physicstime;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);

		G_INT(OFS_PARM0) = (int)PR_TempString(svprogfuncs, s);
		PR_ExecuteProgram (svprogfuncs, gfuncs.UserCmd);
		return !!G_FLOAT(OFS_RETURN);
	}

	return false;
}
qboolean PR_ConsoleCmd(void)
{
	globalvars_t *pr_globals;
	extern redirect_t sv_redirected;

	if (Cmd_ExecLevel < cmd_gamecodelevel.value)
		return false;

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

	if (svprogfuncs)
	{
		pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
		if (gfuncs.ConsoleCmd && pr_imitatemvdsv.value >= 0)
		{
			if (sv_redirected != RD_OBLIVION)
			{
				pr_global_struct->time = sv.physicstime;
				pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv.edicts);
			}

			PR_ExecuteProgram (svprogfuncs, gfuncs.ConsoleCmd);
			return (int) G_FLOAT(OFS_RETURN);
		}
	}

	return false;
}

void PR_ClientUserInfoChanged(char *name, char *oldivalue, char *newvalue)
{
	if (gfuncs.UserInfo_Changed && pr_imitatemvdsv.value >= 0)
	{
		globalvars_t *pr_globals;
		pr_globals = PR_globals(svprogfuncs, PR_CURRENT);

		pr_global_struct->time = sv.physicstime;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);

		G_INT(OFS_PARM0) = PR_TempString(svprogfuncs, name);
		G_INT(OFS_PARM1) = PR_TempString(svprogfuncs, oldivalue);
		G_INT(OFS_PARM2) = PR_TempString(svprogfuncs, newvalue);

		PR_ExecuteProgram (svprogfuncs, gfuncs.UserInfo_Changed);
	}
}

void PR_LocalInfoChanged(char *name, char *oldivalue, char *newvalue)
{
	if (gfuncs.localinfoChanged && sv.state && pr_imitatemvdsv.value >= 0)
	{
		globalvars_t *pr_globals;
		pr_globals = PR_globals(svprogfuncs, PR_CURRENT);

		pr_global_struct->time = sv.physicstime;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv.edicts);

		G_INT(OFS_PARM0) = PR_TempString(svprogfuncs, name);
		G_INT(OFS_PARM1) = PR_TempString(svprogfuncs, oldivalue);
		G_INT(OFS_PARM2) = PR_TempString(svprogfuncs, newvalue);

		PR_ExecuteProgram (svprogfuncs, gfuncs.localinfoChanged);
	}
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
		if ((qbyte *)svprogfuncs->filefromprogs(svprogfuncs, prnumforfile, filename, &com_filesize, NULL)==(qbyte *)-1)
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



//#define	RETURN_EDICT(pf, e) (((int *)pr_globals)[OFS_RETURN] = EDICT_TO_PROG(pf, e))
#define	RETURN_SSTRING(s) (((int *)pr_globals)[OFS_RETURN] = PR_SetString(prinst, s))	//static - exe will not change it.
#define	RETURN_TSTRING(s) (((int *)pr_globals)[OFS_RETURN] = PR_TempString(prinst, s))	//temp (static but cycle buffers)
#define	RETURN_CSTRING(s) (((int *)pr_globals)[OFS_RETURN] = PR_SetString(prinst, s))	//semi-permanant. (hash tables?)
#define	RETURN_PSTRING(s) (((int *)pr_globals)[OFS_RETURN] = PR_NewString(prinst, s, 0))	//permanant

/*
===============================================================================

						BUILT-IN FUNCTIONS

===============================================================================
*/






int externcallsdepth;
void PR_MoveParms(int progs1, int progs2);	//from 2 to 1


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
PF_objerror

Dumps out self, then an error message.  The program is aborted and self is
removed, but the level can continue.

objerror(value)
=================
*/
static void PF_objerror (progfuncs_t *prinst, struct globalvars_s *pr_globals)
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
		(*prinst->pr_trace) = 2;
	else
	{
		ED_Free (prinst, ed);

		prinst->AbortStack(prinst);

		PR_BIError (prinst, "Program error: %s", s);

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
static void PF_makevectors (progfuncs_t *prinst, struct globalvars_s *pr_globals)
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
	VectorCopy (org, e->v->origin);
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
		(*prinst->pr_trace) = 1;
		return;
	}
	min = G_VECTOR(OFS_PARM1);
	max = G_VECTOR(OFS_PARM2);
	VectorCopy (min, e->v->mins);
	VectorCopy (max, e->v->maxs);
	VectorSubtract (max, min, e->v->size);
	SV_LinkEdict (e, false);
}


/*
=================
PF_setmodel

setmodel(entity, model)
Also sets size, mins, and maxs for inline bmodels
=================
*/
void PF_setmodel_Internal (progfuncs_t *prinst, edict_t *e, char *m)
{
	int		i;
	model_t	*mod;

// check to see if model was properly precached
	if (!m || !*m)
		i = 0;
	else
	{
		for (i=1; sv.strings.model_precache[i] ; i++)
		{
			if (!strcmp(sv.strings.model_precache[i], m))
			{
#ifdef VM_Q1
				if (svs.gametype != GT_Q1QVM)
#endif
					m = sv.strings.model_precache[i];
				break;
			}
		}
		if (i==MAX_MODELS || !sv.strings.model_precache[i])
		{
			if (i!=MAX_MODELS)
			{
#ifdef VM_Q1
				if (svs.gametype == GT_Q1QVM)
					sv.strings.model_precache[i] = m;	//in a qvm, we expect the caller to have used a static location.
				else
#endif
					m = sv.strings.model_precache[i] = PR_AddString(prinst, m, 0);
				if (!strcmp(m + strlen(m) - 4, ".bsp"))
					sv.models[i] = Mod_FindName(m);
				Con_Printf("WARNING: SV_ModelIndex: model %s not precached\n", m);

				if (sv.state != ss_loading)
				{
					MSG_WriteByte(&sv.reliable_datagram, svcfte_precache);
					MSG_WriteShort(&sv.reliable_datagram, i);
					MSG_WriteString(&sv.reliable_datagram, m);
#ifdef NQPROT
					MSG_WriteByte(&sv.nqreliable_datagram, svcdp_precache);
					MSG_WriteShort(&sv.nqreliable_datagram, i);
					MSG_WriteString(&sv.nqreliable_datagram, m);
#endif
				}
			}
			else
			{
				PR_BIError (prinst, "no precache: %s\n", m);
				return;
			}
		}
	}

	e->v->model = PR_SetString(prinst, m);
	e->v->modelindex = i;

	// if it is an inline model, get the size information for it
	if (m[0] == '*' || (*m&&progstype == PROG_H2))
	{
		mod = Mod_ForName (m, false);
		if (mod)
		{
			VectorCopy (mod->mins, e->v->mins);
			VectorCopy (mod->maxs, e->v->maxs);
			VectorSubtract (mod->maxs, mod->mins, e->v->size);
			SV_LinkEdict (e, false);
		}

		return;
	}

	if (progstype == PROG_H2)
	{
		e->v->mins[0] = 0;
		e->v->mins[1] = 0;
		e->v->mins[2] = 0;

		e->v->maxs[0] = 0;
		e->v->maxs[1] = 0;
		e->v->maxs[2] = 0;

		VectorSubtract (e->v->maxs, e->v->mins, e->v->size);
	}
	else
	{
		if (progstype != PROG_QW)
		{	//also sets size.

			//nq dedicated servers load bsps and mdls
			//qw dedicated servers only load bsps (better)

			mod = sv.models[i];
			if (mod)
			{
				mod = Mod_ForName (m, false);
				if (mod)
				{
					VectorCopy (mod->mins, e->v->mins);
					VectorCopy (mod->maxs, e->v->maxs);
					VectorSubtract (mod->maxs, mod->mins, e->v->size);
					SV_LinkEdict (e, false);
				}
			}
			else
			{
				//it's an interesting fact that nq pretended that it's models were all +/- 16 (causing culling issues).
				//seing as dedicated servers don't want to load mdls,
				//imitate the behaviour of setting the size (which nq can only have as +/- 16)
				//hell, this works with quakerally so why not use it.
				e->v->mins[0] =
				e->v->mins[1] =
				e->v->mins[2] = -16;
				e->v->maxs[0] =
				e->v->maxs[1] =
				e->v->maxs[2] = 16;
				VectorSubtract (e->v->maxs, e->v->mins, e->v->size);
				SV_LinkEdict (e, false);
			}
		}
		else
		{
			if (sv.models[i])
			{
				mod = Mod_ForName (m, false);
				if (mod)
				{
					VectorCopy (mod->mins, e->v->mins);
					VectorCopy (mod->maxs, e->v->maxs);
					VectorSubtract (mod->maxs, mod->mins, e->v->size);
					SV_LinkEdict (e, false);
				}
			}
			//qw was fixed - it never sets the size of an alias model, mostly because it doesn't know it.
		}
	}
}

void PF_setmodel (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*e;
	char	*m;

	e = G_EDICT(prinst, OFS_PARM0);
	m = PR_GetStringOfs(prinst, OFS_PARM1);

	PF_setmodel_Internal(prinst, e, m);
}

void PF_set_puzzle_model (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{	//qc/hc lacks string manipulation.
	edict_t	*e;
	char *shortname;
	char fullname[MAX_QPATH];
	e = G_EDICT(prinst, OFS_PARM0);
	shortname = PR_GetStringOfs(prinst, OFS_PARM1);

	snprintf(fullname, sizeof(fullname)-1, "models/puzzle/%s.mdl", shortname);
	PF_setmodel_Internal(prinst, e, fullname);
}

static void PF_frameforname (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	unsigned int modelindex = G_FLOAT(OFS_PARM0);
	char *str = PF_VarString(prinst, 1, pr_globals);
	model_t *mod = (modelindex>= MAX_MODELS)?NULL:sv.models[modelindex];

	if (mod && Mod_FrameForName)
		G_FLOAT(OFS_RETURN) = Mod_FrameForName(mod, str);
	else
		G_FLOAT(OFS_RETURN) = -1;
}
static void PF_frameduration (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	unsigned int modelindex = G_FLOAT(OFS_PARM0);
	unsigned int framenum = G_FLOAT(OFS_PARM1);
	model_t *mod;

	if (modelindex >= MAX_MODELS)
		G_FLOAT(OFS_RETURN) = 0;
	else
	{
		mod = sv.models[modelindex];
		if (!mod)
			mod = sv.models[modelindex] = Mod_ForName(sv.strings.model_precache[modelindex], false);

		if (mod && Mod_GetFrameDuration)
			G_FLOAT(OFS_RETURN) = Mod_GetFrameDuration(mod, framenum);
		else
			G_FLOAT(OFS_RETURN) = 0;
	}
}
static void PF_skinforname (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
#ifndef SERVERONLY
	unsigned int modelindex = G_FLOAT(OFS_PARM0);
	char *str = PF_VarString(prinst, 1, pr_globals);
	model_t *mod = (modelindex>= MAX_MODELS)?NULL:sv.models[modelindex];


	if (mod && Mod_SkinForName)
		G_FLOAT(OFS_RETURN) = Mod_SkinForName(mod, str);
	else
#endif
		G_FLOAT(OFS_RETURN) = -1;
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

	if (progstype != PROG_QW)
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

//When a client is backbuffered, it's generally not a brilliant plan to send a bazillion stuffcmds. You have been warned.
//This handy function will let the mod know when it shouldn't send more. (use instead of a timer, and you'll never get clients overflowing. yay.)
void PF_isbackbuffered (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int			entnum;
	client_t	*client;

	entnum = G_EDICTNUM(prinst, OFS_PARM0);
	if (entnum < 1 || entnum > sv.allocated_client_slots)
	{
		Con_Printf ("PF_isbackbuffered: Not a client\n");
		return;
	}
	client = &svs.clients[entnum-1];

	G_FLOAT(OFS_RETURN) = client->num_backbuf>0;
}


/*
=================
PF_centerprint

single print to a specific client

centerprint(clientent, value)
=================
*/
void PF_centerprint_Internal (int entnum, char *s)
{
	client_t	*cl, *sp;
	int			slen;

	if (sv.demofile)
		return;

	if (entnum < 1 || entnum > sv.allocated_client_slots)
	{
		Con_TPrintf (STL_BADSPRINT);
		return;
	}

	cl = &svs.clients[entnum-1];
	slen = strlen(s);

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

		ClientReliableWrite_Begin (sp, svcfte_choosesplitclient, 4 + slen);
		ClientReliableWrite_Byte (sp, pnum);
		ClientReliableWrite_Byte (sp, svc_centerprint);
		ClientReliableWrite_String (sp, s);
	}
	else
	{
		ClientReliableWrite_Begin (cl, svc_centerprint, 2 + slen);
		ClientReliableWrite_String (cl, s);
	}

	if (sv.mvdrecording)
	{
		MVDWrite_Begin (dem_single, entnum - 1, 2 + slen);
		MSG_WriteByte ((sizebuf_t*)demo.dbuf, svc_centerprint);
		MSG_WriteString ((sizebuf_t*)demo.dbuf, s);
	}
}

void PF_centerprint (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char		*s;
	int			entnum;

	entnum = G_EDICTNUM(prinst, OFS_PARM0);
	s = PF_VarString(prinst, 1, pr_globals);
	PF_centerprint_Internal(entnum, s);
}

/*
=================
PF_vhlen

scalar vhlen(vector)
=================
*/
void PF_vhlen (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	*value1;
	float	newv;

	value1 = G_VECTOR(OFS_PARM0);

	newv = value1[0] * value1[0] + value1[1] * value1[1];
	newv = sqrt(newv);

	G_FLOAT(OFS_RETURN) = newv;
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
PF_particle

particle(origin, color, count)
=================
*/
static void PF_particle (progfuncs_t *prinst, globalvars_t *pr_globals)	//I said it was for compatability only.
{
	float		*org, *dir;
	float		color;
	float		count;
	int i, v;

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
/*	if (*prinst->callargc >= 5)
	{
	PARM4 = te_
	optional PARM5 = count
		MSG_WriteByte (&sv.multicast, svc_temp_entity);
		MSG_WriteByte (&sv.multicast, TE_BLOOD);
		MSG_WriteByte (&sv.multicast, count<10?1:(count+10)/20);
		MSG_WriteCoord (&sv.multicast, org[0]);
		MSG_WriteCoord (&sv.multicast, org[1]);
		MSG_WriteCoord (&sv.multicast, org[2]);
		SV_MulticastProtExt(org, MULTICAST_PVS, pr_global_struct->dimension_send, 0, PEXT_HEXEN2);
	}
	else
*/
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

void PF_te_blooddp (progfuncs_t *prinst, globalvars_t *pr_globals)
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
static void PF_particle2 (progfuncs_t *prinst, globalvars_t *pr_globals)
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

	MSG_WriteByte (&sv.multicast, svcfte_particle2);
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
static void PF_particle3 (progfuncs_t *prinst, globalvars_t *pr_globals)
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

	MSG_WriteByte (&sv.multicast, svcfte_particle3);
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
static void PF_particle4 (progfuncs_t *prinst, globalvars_t *pr_globals)
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

	MSG_WriteByte (&sv.multicast, svcfte_particle4);
	MSG_WriteCoord (&sv.multicast, org[0]);
	MSG_WriteCoord (&sv.multicast, org[1]);
	MSG_WriteCoord (&sv.multicast, org[2]);
	MSG_WriteByte (&sv.multicast, radius);

	MSG_WriteShort (&sv.multicast, color);
	MSG_WriteByte (&sv.multicast, count);
	MSG_WriteByte (&sv.multicast, effect);

	SV_MulticastProtExt (org, MULTICAST_PVS, pr_global_struct->dimension_send, PEXT_HEXEN2, 0);
}

void PF_h2particleexplosion(progfuncs_t *prinst, struct globalvars_s *pr_globals)
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
void PF_ambientsound_Internal (float *pos, char *samp, float vol, float attenuation)
{
	int			i, soundnum;

// check to see if samp was properly precached
	for (soundnum=1 ; *sv.strings.sound_precache[soundnum] ; soundnum++)
		if (!strcmp(sv.strings.sound_precache[soundnum],samp))
			break;

	if (!*sv.strings.sound_precache[soundnum])
	{
		Con_TPrintf (STL_NOPRECACHE, samp);
		return;
	}

	SV_FlushSignon();

// add an svc_spawnambient command to the level signon packet

	MSG_WriteByte (&sv.signon,svc_spawnstaticsound);
	for (i=0 ; i<3 ; i++)
		MSG_WriteCoord(&sv.signon, pos[i]);

	MSG_WriteByte (&sv.signon, soundnum);

	MSG_WriteByte (&sv.signon, vol*255);
	MSG_WriteByte (&sv.signon, attenuation*64);

}

void PF_ambientsound (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char		*samp;
	float		*pos;
	float 		vol, attenuation;

	pos = G_VECTOR (OFS_PARM0);
	samp = PR_GetStringOfs(prinst, OFS_PARM1);
	vol = G_FLOAT(OFS_PARM2);
	attenuation = G_FLOAT(OFS_PARM3);

	PF_ambientsound_Internal(pos, samp, vol, attenuation);
}

/*
=================
PF_sound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
already running on that entity/channel pair.

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

	SVQ1_StartSound (entity, channel, sample, volume, attenuation);
}

//an evil one from telejano.
void PF_LocalSound(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
#ifndef SERVERONLY
	sfx_t	*sfx;

	char * s = PR_GetStringOfs(prinst, OFS_PARM0);
	float chan = G_FLOAT(OFS_PARM1);
	float vol = G_FLOAT(OFS_PARM2);

	if (!isDedicated)
	{
		if ((sfx = S_PrecacheSound(s)))
			S_StartSound(cl.playernum[0], chan, sfx, cl.simorg[0], vol, 0.0);
	}
#endif
};

/*
=================
PF_traceline

Used for use tracing and shot targeting
Traces are blocked by bbox and exact bsp entityes, and also slide box entities
if the tryents flag is set.

traceline (vector1, vector2, tryents)
=================
*/
void PF_svtraceline (progfuncs_t *prinst, struct globalvars_s *pr_globals)
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

	savedhull = ent->xv->hull;
	ent->xv->hull = 0;
	trace = SV_Move (v1, mins, maxs, v2, nomonsters, ent);
	ent->xv->hull = savedhull;

	if (trace.startsolid)
		if (!sv_gameplayfix_honest_tracelines.value)
			trace.fraction = 1;

	pr_global_struct->trace_allsolid = trace.allsolid;
	pr_global_struct->trace_startsolid = trace.startsolid;
	pr_global_struct->trace_fraction = trace.fraction;
	pr_global_struct->trace_inwater = trace.inwater;
	pr_global_struct->trace_inopen = trace.inopen;
	pr_global_struct->trace_surfaceflags = trace.surface?trace.surface->flags:0;
	pr_global_struct->trace_endcontents = trace.contents;
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

static void PF_traceboxh2 (progfuncs_t *prinst, struct globalvars_s *pr_globals)
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

	savedhull = ent->xv->hull;
	ent->xv->hull = 0;
	trace = SV_Move (v1, mins, maxs, v2, nomonsters, ent);
	ent->xv->hull = savedhull;

	if (trace.startsolid)
		if (!sv_gameplayfix_honest_tracelines.value)
			trace.fraction = 1;

	pr_global_struct->trace_allsolid = trace.allsolid;
	pr_global_struct->trace_startsolid = trace.startsolid;
	pr_global_struct->trace_fraction = trace.fraction;
	pr_global_struct->trace_inwater = trace.inwater;
	pr_global_struct->trace_inopen = trace.inopen;
	pr_global_struct->trace_surfaceflags = trace.surface?trace.surface->flags:0;
	pr_global_struct->trace_endcontents = trace.contents;
	VectorCopy (trace.endpos, P_VEC(trace_endpos));
	VectorCopy (trace.plane.normal, P_VEC(trace_plane_normal));
	pr_global_struct->trace_plane_dist =  trace.plane.dist;
	if (trace.ent)
		pr_global_struct->trace_ent = EDICT_TO_PROG(prinst, trace.ent);
	else
		pr_global_struct->trace_ent = EDICT_TO_PROG(prinst, sv.edicts);
}

static void PF_traceboxdp (progfuncs_t *prinst, struct globalvars_s *pr_globals)
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

	savedhull = ent->xv->hull;
	ent->xv->hull = 0;
	trace = SV_Move (v1, mins, maxs, v2, nomonsters, ent);
	ent->xv->hull = savedhull;

	if (trace.startsolid)
		if (!sv_gameplayfix_honest_tracelines.value)
			trace.fraction = 1;

	pr_global_struct->trace_allsolid = trace.allsolid;
	pr_global_struct->trace_startsolid = trace.startsolid;
	pr_global_struct->trace_fraction = trace.fraction;
	pr_global_struct->trace_inwater = trace.inwater;
	pr_global_struct->trace_inopen = trace.inopen;
	pr_global_struct->trace_surfaceflags = trace.surface?trace.surface->flags:0;
	pr_global_struct->trace_endcontents = trace.contents;
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

extern trace_t SV_Trace_Toss (edict_t *ent, edict_t *ignore);
static void PF_TraceToss (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	trace_t	trace;
	edict_t	*ent;
	edict_t	*ignore;

	ent = G_EDICT(prinst, OFS_PARM0);
	if (ent == sv.edicts)
		Con_DPrintf("tracetoss: can not use world entity\n");
	ignore = G_EDICT(prinst, OFS_PARM1);

	trace = SV_Trace_Toss (ent, ignore);

	pr_global_struct->trace_allsolid = trace.allsolid;
	pr_global_struct->trace_startsolid = trace.startsolid;
	pr_global_struct->trace_fraction = trace.fraction;
	pr_global_struct->trace_inwater = trace.inwater;
	pr_global_struct->trace_inopen = trace.inopen;
	pr_global_struct->trace_surfaceflags = trace.surface?trace.surface->flags:0;
	pr_global_struct->trace_endcontents = trace.contents;
	VectorCopy (trace.endpos, pr_global_struct->V_trace_endpos);
	VectorCopy (trace.plane.normal, pr_global_struct->V_trace_plane_normal);
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
		if (ent->v->health <= 0)
			continue;
		if ((int)ent->v->flags & FL_NOTARGET)
			continue;

	// anything that is a client, or has a client as an enemy
		break;
	}

// get the PVS for the entity
	VectorAdd (ent->v->origin, ent->v->view_ofs, org);
	leaf = sv.worldmodel->funcs.LeafnumForPoint(sv.worldmodel, org);
	checkpvs = sv.worldmodel->funcs.LeafPVS (sv.worldmodel, leaf, checkpvsbuffer, sizeof(checkpvsbuffer));

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
int PF_checkclient_Internal (progfuncs_t *prinst)
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
	if (ent->isfree || ent->v->health <= 0)
	{
		return 0;
	}

// if current entity can't possibly see the check entity, return 0
	self = PROG_TO_EDICT(prinst, pr_global_struct->self);
	VectorAdd (self->v->origin, self->v->view_ofs, view);
	l = sv.worldmodel->funcs.LeafnumForPoint(sv.worldmodel, view)-1;
	if ( (l<0) || !(checkpvs[l>>3] & (1<<(l&7)) ) )
	{
c_notvis++;
		return 0;
	}

// might be able to see it
c_invis++;
	return sv.lastcheck;
}

void PF_checkclient (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	RETURN_EDICT(prinst, EDICT_NUM(prinst, PF_checkclient_Internal(prinst)));
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
	int		slen;

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
			if (!strcmp(str, "color "))
				return;
			else
				str += 6;
		}
		// FIXME: this seems broken and color->teamname needs a common functions
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

	slen = strlen(str);

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

		ClientReliableWrite_Begin (sp, svcfte_choosesplitclient, 4 + slen);
		ClientReliableWrite_Byte (sp, pnum);
		ClientReliableWrite_Byte (sp, svc_stufftext);
		ClientReliableWrite_String (sp, str);
	}
	else
	{
		ClientReliableWrite_Begin (cl, svc_stufftext, 2+slen);
		ClientReliableWrite_String (cl, str);
	}

	if (sv.mvdrecording)
	{
		MVDWrite_Begin (dem_single, entnum - 1, 2 + slen);
		MSG_WriteByte ((sizebuf_t*)demo.dbuf, svc_stufftext);
		MSG_WriteString ((sizebuf_t*)demo.dbuf, str);
	}
}

//DP_QC_DROPCLIENT
void PF_dropclient (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int		entnum;
	client_t	*cl;

	entnum = G_EDICTNUM(prinst, OFS_PARM0);
	if (entnum < 1 || entnum > sv.allocated_client_slots)
		return;

	cl = &svs.clients[entnum-1];

	// so long and thanks for all the fish
	if (cl->netchan.remote_address.type == NA_LOOPBACK)
		return;	//don't drop the local client. It looks wrong.
	cl->drop = true;
	return;
}



//DP_SV_BOTCLIENT
//entity() spawnclient = #454;
void PF_spawnclient (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i;
	for (i = 0; i < sv.allocated_client_slots; i++)
	{
		if (!*svs.clients[i].name && !svs.clients[i].protocol && svs.clients[i].state == cs_free)
		{
			svs.clients[i].protocol = SCP_BAD;	//marker for bots
			svs.clients[i].state = cs_spawned;
			svs.clients[i].netchan.message.allowoverflow = true;
			svs.clients[i].netchan.message.maxsize = 0;
			svs.clients[i].datagram.allowoverflow = true;
			svs.clients[i].datagram.maxsize = 0;

			SV_SetUpClientEdict (&svs.clients[i], svs.clients[i].edict);

			RETURN_EDICT(prinst, svs.clients[i].edict);
			return;
		}
	}
	RETURN_EDICT(prinst, sv.edicts);
}

//DP_SV_BOTCLIENT
//float(entity client) clienttype = #455;
void PF_clienttype (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int entnum = G_EDICTNUM(prinst, OFS_PARM0);
	if (entnum < 1 || entnum > sv.allocated_client_slots)
	{
		G_FLOAT(OFS_RETURN) = 3;	//not a client slot
		return;
	}
	entnum--;
	if (svs.clients[entnum].state < cs_connected)
	{
		G_FLOAT(OFS_RETURN) = 0;	//disconnected
		return;
	}
	if (svs.clients[entnum].protocol == SCP_BAD)
		G_FLOAT(OFS_RETURN) = 2;	//an active, bot client.
	else
		G_FLOAT(OFS_RETURN) = 1;	//an active, not-bot client.
}

/*
=================
PF_cvar

float cvar (string)
=================
*/
static void PF_cvar (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*str;

	str = PR_GetStringOfs(prinst, OFS_PARM0);

	if (!strcmp(str, "pr_checkextension"))	//no console changing
		G_FLOAT(OFS_RETURN) = PR_EnableEBFSBuiltin("checkextension", 0);
	else if (!strcmp(str, "pr_builtin_find"))
		G_FLOAT(OFS_RETURN) = PR_EnableEBFSBuiltin("builtin_find", 0);
	else if (!strcmp(str, "pr_map_builtin"))
		G_FLOAT(OFS_RETURN) = PR_EnableEBFSBuiltin("map_builtin", 0);
	else if (!strcmp(str, "halflifebsp"))
		G_FLOAT(OFS_RETURN) = sv.worldmodel->fromgame == fg_halflife;
	else
	{
		cvar_t *cv = Cvar_FindVar(str);
		if (!cv)
		{
			//this little chunk is so cvars dp creates are created with meaningful values
			char *def = "";
			if (!strcmp(str, "sv_maxairspeed"))
				def = "30";
			else
				def = "";

			cv = Cvar_Get(str, def, 0, "QC variables");
			Con_Printf("^3Creating cvar %s\n", str);
		}
		G_FLOAT(OFS_RETURN) = cv->value;
	}
}

void PF_sv_getlight (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *point = G_VECTOR(OFS_PARM0);
	vec3_t diffuse, ambient, dir;
	if (sv.worldmodel && sv.worldmodel->funcs.LightPointValues)
	{
		sv.worldmodel->funcs.LightPointValues(sv.worldmodel, point, diffuse, ambient, dir);
		VectorMA(ambient, 0.5, diffuse, G_VECTOR(OFS_RETURN));
	}
	else
	{
		G_FLOAT(OFS_RETURN+0) = 128;
		G_FLOAT(OFS_RETURN+1) = 128;
		G_FLOAT(OFS_RETURN+2) = 128;
		return;
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
	rad = rad*rad;

	for (i=1 ; i<sv.num_edicts ; i++)
	{
		ent = EDICT_NUM(svprogfuncs, i);
		if (ent->isfree)
			continue;
		if (ent->v->solid == SOLID_NOT && (progstype != PROG_QW || !((int)ent->v->flags & FL_FINDABLE_NONSOLID)) && !sv_gameplayfix_blowupfallenzombies.value)
			continue;
		for (j=0 ; j<3 ; j++)
			eorg[j] = org[j] - (ent->v->origin[j] + (ent->v->mins[j] + ent->v->maxs[j])*0.5);
		if (DotProduct(eorg,eorg) > rad)
			continue;

		ent->v->chain = EDICT_TO_PROG(prinst, chain);
		chain = ent;
	}

	RETURN_EDICT(prinst, chain);
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


void PF_printf (progfuncs_t *prinst, struct globalvars_s *pr_globals)
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

void PF_printv (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char temp[256];

	sprintf (temp, "'%5.1f %5.1f %5.1f'", G_VECTOR(OFS_PARM1)[0], G_VECTOR(OFS_PARM1)[1], G_VECTOR(OFS_PARM1)[2]);

	Con_Printf (PR_GetStringOfs(prinst, OFS_PARM0),temp);
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

	if (ed->isfree)
	{
		ED_CanFree(ed);	//fake it
		if (developer.value)
		{
			Con_Printf("Tried removing free entity at:\n");
			PR_StackTrace(prinst);
		}
		return;	//yeah, alright, so this is hacky.
	}

	ED_Free (prinst, ed);
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

void PF_precache_sound_Internal (progfuncs_t *prinst, char *s)
{
	int		i;

	if (s[0] <= ' ')
	{
		PR_BIError (prinst, "PF_precache_sound: Bad string");
		return;
	}

	for (i=1 ; i<MAX_SOUNDS ; i++)
	{
		if (!*sv.strings.sound_precache[i])
		{
			strcpy(sv.strings.sound_precache[i], s);


			if (sv.state != ss_loading)
			{
				MSG_WriteByte(&sv.reliable_datagram, svcfte_precache);
				MSG_WriteShort(&sv.reliable_datagram, i+32768);
				MSG_WriteString(&sv.reliable_datagram, s);
#ifdef NQPROT
				MSG_WriteByte(&sv.nqreliable_datagram, svcdp_precache);
				MSG_WriteShort(&sv.nqreliable_datagram, i+32768);
				MSG_WriteString(&sv.nqreliable_datagram, s);
#endif
			}
			return;
		}
		if (!strcmp(sv.strings.sound_precache[i], s))
			return;
	}
	PR_BIError (prinst, "PF_precache_sound: overflow");
}
void PF_precache_sound (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*s;

	s = PR_GetStringOfs(prinst, OFS_PARM0);

	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);

	PF_precache_sound_Internal(prinst, s);
}

int PF_precache_model_Internal (progfuncs_t *prinst, char *s)
{
	int		i;

	if (s[0] <= ' ')
	{
		Con_Printf ("precache_model: empty string\n");
		return 0;
	}

	for (i=1 ; i<MAX_MODELS ; i++)
	{
		if (!sv.strings.model_precache[i])
		{
			if (strlen(s)>=MAX_QPATH-1)	//probably safest to keep this.
			{
				PR_BIError (prinst, "Precache name too long");
				return 0;
			}
#ifdef VM_Q1
			if (svs.gametype == GT_Q1QVM)
				sv.strings.model_precache[i] = s;
			else
#endif
				sv.strings.model_precache[i] = PR_AddString(prinst, s, 0);
			s = sv.strings.model_precache[i];
			if (!strcmp(s + strlen(s) - 4, ".bsp"))
				sv.models[i] = Mod_FindName(s);

			if (sv.state != ss_loading)
			{
				MSG_WriteByte(&sv.reliable_datagram, svcfte_precache);
				MSG_WriteShort(&sv.reliable_datagram, i);
				MSG_WriteString(&sv.reliable_datagram, s);
#ifdef NQPROT
				MSG_WriteByte(&sv.nqreliable_datagram, svcdp_precache);
				MSG_WriteShort(&sv.nqreliable_datagram, i);
				MSG_WriteString(&sv.nqreliable_datagram, s);
#endif
			}

			return i;
		}
		if (!strcmp(sv.strings.model_precache[i], s))
		{
			return i;
		}
	}
	PR_BIError (prinst, "PF_precache_model: overflow");
	return 0;
}
void PF_precache_model (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*s;

	s = PR_GetStringOfs(prinst, OFS_PARM0);
	
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);

	PF_precache_model_Internal(prinst, s);
}

void PF_precache_puzzle_model (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{	//qc/hc lacks string manipulation.
	char *shortname;
	char fullname[MAX_QPATH];
	shortname = PR_GetStringOfs(prinst, OFS_PARM0);
	snprintf(fullname, sizeof(fullname)-1, "models/puzzle/%s.mdl", shortname);

	PF_precache_model_Internal(prinst, fullname);
}

void PF_getmodelindex (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*s;

	s = PR_GetStringOfs(prinst, OFS_PARM0);
	G_INT(OFS_RETURN) = PF_precache_model_Internal(prinst, s);
}
void PF_precache_vwep_model (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i;
	char	*s;

	s = PR_GetStringOfs(prinst, OFS_PARM0);
	if (!*s || strchr(s, '\"') || strchr(s, ';') || strchr(s, '\t') || strchr(s, '\n'))
	{
		Con_Printf("PF_precache_vwep_model: bad string\n");
		G_FLOAT(OFS_RETURN) = 0;
	}
	else
	{
		for (i = 0; i < sizeof(sv.strings.vw_model_precache)/sizeof(sv.strings.vw_model_precache[0]); i++)
		{
			if (!sv.strings.vw_model_precache[i])
			{
				if (sv.state != ss_loading)
				{
					Con_Printf("PF_precache_vwep_model: not spawning\n");
					G_FLOAT(OFS_RETURN) = 0;
					return;
				}
#ifdef VM_Q1
				if (svs.gametype == GT_Q1QVM)
					sv.strings.vw_model_precache[i] = s;
				else
#endif
					sv.strings.vw_model_precache[i] = PR_AddString(prinst, s, 0);
				return;
			}
			if (!strcmp(sv.strings.vw_model_precache[i], s))
			{
				G_FLOAT(OFS_RETURN) = i;
				return;
			}
		}
		Con_Printf("PF_precache_vwep_model: overflow\n");
		G_FLOAT(OFS_RETURN) = 0;
	}
}

void PF_svcoredump (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int size = 1024*1024*8;
	char *buffer = BZ_Malloc(size);
	prinst->save_ents(prinst, buffer, &size, 3);
	COM_WriteFile("ssqccore.txt", buffer, size);
	BZ_Free(buffer);
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

	if ( !( (int)ent->v->flags & (FL_ONGROUND|FL_FLY|FL_SWIM) ) )
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

//	if (!dist)
//	{
//		G_FLOAT(OFS_RETURN) = !SV_TestEntityPosition(ent);
//	}
//	else if (!SV_TestEntityPosition(ent))
//	{
		G_FLOAT(OFS_RETURN) = SV_movestep(ent, move, true, false, settrace);
//		if (SV_TestEntityPosition(ent))
//			Con_Printf("Entity became stuck\n");
//	}


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

	VectorCopy (ent->v->origin, end);
	if (pr_droptofloorunits.value > 0)
		end[2] -= pr_droptofloorunits.value;
	else
		end[2] -= 256;

	VectorCopy (ent->v->origin, start);
	trace = SV_Move (start, ent->v->mins, ent->v->maxs, end, MOVE_NORMAL, ent);

	if (trace.fraction == 1 || trace.allsolid)
		G_FLOAT(OFS_RETURN) = 0;
	else
	{
		VectorCopy (trace.endpos, ent->v->origin);
		SV_LinkEdict (ent, false);
		ent->v->flags = (int)ent->v->flags | FL_ONGROUND;
		ent->v->groundentity = EDICT_TO_PROG(prinst, trace.ent);
		G_FLOAT(OFS_RETURN) = 1;
	}
}

void PF_applylightstyle(int style, char *val, int col)
{
	client_t	*client;
	int			j;


	if (style < 0 || style >= MAX_LIGHTSTYLES)
	{
		Con_Printf("WARNING: Bad lightstyle %i.\n", style);
		return;
	}
	if (strlen(val) > MAX_STYLESTRING-1)
		Con_Printf("WARNING: Style string is longer than standard (%i). Some clients could crash.\n", MAX_STYLESTRING-1);


// change the string in sv
	if (sv.strings.lightstyles[style])
		Z_Free(sv.strings.lightstyles[style]);
	sv.strings.lightstyles[style] = Z_Malloc(strlen(val)+1);
	strcpy(sv.strings.lightstyles[style], val);
//	sv.lightstyles[style] = val;
#ifdef PEXT_LIGHTSTYLECOL
	sv.strings.lightstylecolours[style] = col;
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
			if (style >= MAX_STANDARDLIGHTSTYLES)
				if (!*val)
					continue;
#ifdef PEXT_LIGHTSTYLECOL
			if ((client->fteprotocolextensions & PEXT_LIGHTSTYLECOL) && col!=7)
			{
				ClientReliableWrite_Begin (client, svcfte_lightstylecol, strlen(val)+4);
				ClientReliableWrite_Byte (client, style);
				ClientReliableWrite_Char (client, col);
				ClientReliableWrite_String (client, val);
			}
			else
			{
#endif
				ClientReliableWrite_Begin (client, svc_lightstyle, strlen(val)+3);
				ClientReliableWrite_Byte (client, style);
				ClientReliableWrite_String (client, val);
#ifdef PEXT_LIGHTSTYLECOL
			}
#endif
		}
	}
}

/*
===============
PF_lightstyle

void(float style, string value [, float colour]) lightstyle
===============
*/
void PF_lightstyle (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int		style;
	char	*val;

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

	PF_applylightstyle(style, val, col);
}

void PF_lightstylevalue (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int style;
	style = G_FLOAT(OFS_PARM0);
	if(style < 0 || style >= MAX_LIGHTSTYLES || !sv.strings.lightstyles[style])
	{
		G_FLOAT(OFS_RETURN) = 0;
		return;
	}
	G_FLOAT(OFS_RETURN) = *sv.strings.lightstyles[style] - 'a';
}

void PF_lightstylestatic (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int		style;
	float	num;
	char	*val;

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

	PF_applylightstyle(style, val, col);
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

//	cont = SV_Move(v, vec3_origin, vec3_origin, v, MOVE_NOMONSTERS, NULL).contents;
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
PF_aim

Pick a vector for the player to shoot along
vector aim(entity, missilespeed)
=============
*/
//cvar_t	sv_aim = {"sv_aim", "0.93"};
cvar_t	sv_aim = SCVAR("sv_aim", "2");
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

	VectorCopy (ent->v->origin, start);
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
	if (tr.ent && ((edict_t *)tr.ent)->v->takedamage == DAMAGE_AIM
	&& (!teamplay.value || ent->v->team <=0 || ent->v->team != ((edict_t *)tr.ent)->v->team) )
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
		if (check->v->takedamage != DAMAGE_AIM)
			continue;
		if (check == ent)
			continue;
		if (teamplay.value && ent->v->team > 0 && ent->v->team == check->v->team)
			continue;	// don't aim at teammate
		for (j=0 ; j<3 ; j++)
			end[j] = check->v->origin[j]
			+ 0.5*(check->v->mins[j] + check->v->maxs[j]);
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
		VectorSubtract (bestent->v->origin, ent->v->origin, dir);
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
	current = anglemod( ent->v->angles[1] );
	ideal = ent->v->ideal_yaw;
	speed = ent->v->yaw_speed;

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

	ent->v->angles[1] = anglemod (current + move);
}

//void() changepitch = #63;
static void PF_changepitch (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t		*ent;
	float		ideal, current, move, speed;
	eval_t *eval;

	ent = PROG_TO_EDICT(prinst, pr_global_struct->self);
	current = anglemod( ent->v->angles[1] );

	eval = prinst->GetEdictFieldValue(prinst, ent, "idealpitch", &evalc_idealpitch);
	if (eval)
		ideal = eval->_float;
	else
		ideal = 0;
	eval = prinst->GetEdictFieldValue(prinst, ent, "pitch_speed", &evalc_pitch_speed);
	if (eval)
		speed = eval->_float;
	else
		speed = 0;

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

	ent->v->angles[1] = anglemod (current + move);
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

sizebuf_t *QWWriteDest (int		dest)
{
	switch (dest)
	{
	case MSG_PRERELONE:
		{
		int entnum;
		entnum = PR_globals(svprogfuncs, PR_CURRENT)->param[0].i;
		return &svs.clients[entnum-1].netchan.message;
		}
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
			PR_BIError (svprogfuncs, "PF_Write_*: MSG_INIT can only be written in spawn functions");
			return NULL;
		}
		return &sv.signon;

	case MSG_MULTICAST:
		return &sv.multicast;

	default:
		PR_BIError (svprogfuncs, "WriteDest: bad destination");
		break;
	}

	return NULL;
}
#ifdef NQPROT
sizebuf_t *NQWriteDest (int dest)
{
	switch (dest)
	{
	case MSG_PRERELONE:
		{
		int entnum;
		entnum = PR_globals(svprogfuncs, PR_CURRENT)->param[0].i;
		return &svs.clients[entnum-1].netchan.message;
		}

	case MSG_BROADCAST:
		return &sv.nqdatagram;

	case MSG_ONE:
		SV_Error("Shouldn't be at MSG_ONE");
#if 0
		ent = PROG_TO_EDICT(pr_global_struct->msg_entity);
		entnum = NUM_FOR_EDICT(ent);
		if (entnum < 1 || entnum > MAX_CLIENTS)
		{
			PR_BIError (prinst, "WriteDest: not a client");
			return &sv.nqreliable_datagram;
		}
		return &svs.clients[entnum-1].netchan.message;
#endif

	case MSG_ALL:
		return &sv.nqreliable_datagram;

	case MSG_INIT:
		if (sv.state != ss_loading)
		{
			PR_BIError (svprogfuncs, "PF_Write_*: MSG_INIT can only be written in spawn functions");
			return NULL;
		}
		return &sv.signon;

	case MSG_MULTICAST:
		return &sv.nqmulticast;

	default:
		PR_BIError (svprogfuncs, "WriteDest: bad destination");
		break;
	}

	return NULL;
}
#endif

client_t *Write_GetClient(void)
{
	int		entnum;
	edict_t	*ent;

	ent = PROG_TO_EDICT(svprogfuncs, pr_global_struct->msg_entity);
	entnum = NUM_FOR_EDICT(svprogfuncs, ent);
	if (entnum < 1 || entnum > sv.allocated_client_slots)
		return NULL;//PR_RunError ("WriteDest: not a client");
	return &svs.clients[entnum-1];
}

extern sizebuf_t csqcmsgbuffer;
void PF_WriteByte (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (G_FLOAT(OFS_PARM0) == MSG_CSQC)
	{	//csqc buffers are always written.
		MSG_WriteByte(&csqcmsgbuffer, G_FLOAT(OFS_PARM1));
		return;
	}

	if (qc_nonetaccess.value || sv.demofile)
		return;

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
		MSG_WriteByte (QWWriteDest(G_FLOAT(OFS_PARM0)), G_FLOAT(OFS_PARM1));
#endif
}

void PF_WriteChar (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (G_FLOAT(OFS_PARM0) == MSG_CSQC)
	{	//csqc buffers are always written.
		MSG_WriteChar(&csqcmsgbuffer, G_FLOAT(OFS_PARM1));
		return;
	}

	if (qc_nonetaccess.value || sv.demofile)
		return;
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
		MSG_WriteChar (QWWriteDest(G_FLOAT(OFS_PARM0)), G_FLOAT(OFS_PARM1));
#endif
}

void PF_WriteShort (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (G_FLOAT(OFS_PARM0) == MSG_CSQC)
	{	//csqc buffers are always written.
		MSG_WriteShort(&csqcmsgbuffer, G_FLOAT(OFS_PARM1));
		return;
	}

	if (qc_nonetaccess.value || sv.demofile)
		return;

	if (progstype == PROG_NQ || progstype == PROG_H2)
	{
		NPP_NQWriteShort(G_FLOAT(OFS_PARM0), (short)(int)G_FLOAT(OFS_PARM1));
		return;
	}
#ifdef NQPROT
	else
	{
		NPP_QWWriteShort(G_FLOAT(OFS_PARM0), (short)(int)G_FLOAT(OFS_PARM1));
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
		MSG_WriteShort (QWWriteDest(G_FLOAT(OFS_PARM0)), G_FLOAT(OFS_PARM1));
#endif
}

void PF_WriteLong (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (G_FLOAT(OFS_PARM0) == MSG_CSQC)
	{	//csqc buffers are always written.
		MSG_WriteLong(&csqcmsgbuffer, G_FLOAT(OFS_PARM1));
		return;
	}

	if (qc_nonetaccess.value || sv.demofile)
		return;

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
		MSG_WriteLong (QWWriteDest(G_FLOAT(OFS_PARM0)), G_FLOAT(OFS_PARM1));
#endif
}

void PF_WriteAngle (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (G_FLOAT(OFS_PARM0) == MSG_CSQC)
	{	//csqc buffers are always written.
		MSG_WriteAngle(&csqcmsgbuffer, G_FLOAT(OFS_PARM1));
		return;
	}

	if (qc_nonetaccess.value || sv.demofile)
		return;
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
		MSG_WriteAngle (QWWriteDest(G_FLOAT(OFS_PARM0)), G_FLOAT(OFS_PARM1));
#endif
}

void PF_WriteCoord (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (G_FLOAT(OFS_PARM0) == MSG_CSQC)
	{	//csqc buffers are always written.
		MSG_WriteCoord(&csqcmsgbuffer, G_FLOAT(OFS_PARM1));
		return;
	}

	if (qc_nonetaccess.value || sv.demofile)
		return;

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
		MSG_WriteCoord (QWWriteDest(G_FLOAT(OFS_PARM0)), G_FLOAT(OFS_PARM1));
#endif
}

void PF_WriteString_Internal (int target, char *str)
{
	if (target == MSG_CSQC)
	{	//csqc buffers are always written.
		MSG_WriteString(&csqcmsgbuffer, str);
		return;
	}

	if (qc_nonetaccess.value || sv.demofile)
		return;

	if (progstype == PROG_NQ || progstype == PROG_H2)
	{
		NPP_NQWriteString(target, str);
		return;
	}
#ifdef NQPROT
	else
	{
		NPP_QWWriteString(target, str);
		return;
	}
#else
	if (target == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		if (!cl)
			return;
		ClientReliableCheckBlock(cl, 1+strlen(str));
		ClientReliableWrite_String(cl, str);
	}
	else
		MSG_WriteString (QWWriteDest(target), str);
#endif
}

void PF_WriteString (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *str = PF_VarString(prinst, 1, pr_globals);
	PF_WriteString_Internal(G_FLOAT(OFS_PARM0), str);
}


void PF_WriteEntity (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (G_FLOAT(OFS_PARM0) == MSG_CSQC)
	{	//csqc buffers are always written.
		MSG_WriteShort(&csqcmsgbuffer, G_EDICTNUM(prinst, OFS_PARM1));
		return;
	}

	if (qc_nonetaccess.value || sv.demofile)
		return;

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
		MSG_WriteShort (QWWriteDest(G_FLOAT(OFS_PARM0)), G_EDICTNUM(prinst, OFS_PARM1));
#endif
}

//small wrapper function.
//void(float target, string str, ...) WriteString2 = #33;
void PF_WriteString2 (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int old;
	char *str;

	if (G_FLOAT(OFS_PARM0) != MSG_CSQC && (qc_nonetaccess.value || sv.demofile))
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

void PF_Single_WriteByte (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteByte(MSG_PRERELONE, (qbyte)G_FLOAT(OFS_PARM1));
}
void PF_Single_WriteChar (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteChar(MSG_PRERELONE, (char)G_FLOAT(OFS_PARM1));
}
void PF_Single_WriteShort (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteShort(MSG_PRERELONE, (short)G_FLOAT(OFS_PARM1));
}
void PF_Single_WriteLong (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteLong(MSG_PRERELONE, G_FLOAT(OFS_PARM1));
}
void PF_Single_WriteAngle (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteAngle(MSG_PRERELONE, G_FLOAT(OFS_PARM1));
}
void PF_Single_WriteCoord (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteCoord(MSG_PRERELONE, G_FLOAT(OFS_PARM1));
}
void PF_Single_WriteString (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteString(MSG_PRERELONE, PF_VarString(prinst, 1, pr_globals));
}
void PF_Single_WriteEntity (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteEntity(MSG_PRERELONE, (short)G_EDICTNUM(prinst, OFS_PARM1));
}

void PF_Broadcast_WriteByte (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteByte(MSG_BROADCAST, (qbyte)G_FLOAT(OFS_PARM0));
}
void PF_Broadcast_WriteChar (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteChar(MSG_BROADCAST, (char)G_FLOAT(OFS_PARM0));
}
void PF_Broadcast_WriteShort (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteShort(MSG_BROADCAST, (short)G_FLOAT(OFS_PARM0));
}
void PF_Broadcast_WriteLong (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteLong(MSG_BROADCAST, G_FLOAT(OFS_PARM0));
}
void PF_Broadcast_WriteAngle (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteAngle(MSG_BROADCAST, G_FLOAT(OFS_PARM0));
}
void PF_Broadcast_WriteCoord (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteCoord(MSG_BROADCAST, G_FLOAT(OFS_PARM0));
}
void PF_Broadcast_WriteString (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteString(MSG_BROADCAST, PF_VarString(prinst, 0, pr_globals));
}
void PF_Broadcast_WriteEntity (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteEntity(MSG_BROADCAST, (short)G_EDICTNUM(prinst, OFS_PARM0));
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
#ifdef NQPROT
		MSG_WriteByte (&sv.nqmulticast, type);
#endif
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

/*
void PF_tempentity (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), G_FLOAT(OFS_PARM1), 1);
}
*/

//=============================================================================

int SV_ModelIndex (char *name);

void PF_makestatic (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*ent;
	int		mdlindex, i;
	entity_state_t *state;

	ent = G_EDICT(prinst, OFS_PARM0);

	SV_FlushSignon ();

	mdlindex = SV_ModelIndex(PR_GetString(prinst, ent->v->model));

	if (ent->xv->drawflags || ent->xv->alpha || mdlindex > 255 || ent->v->frame > 255 || ent->xv->scale || ent->xv->abslight)
	{
		if (sv.numextrastatics==sizeof(sv.extendedstatics)/sizeof(sv.extendedstatics[0]))
			return;	//fail the whole makestatic thing.

		state = &sv.extendedstatics[sv.numextrastatics++];
		memset(state, 0, sizeof(*state));
		state->number = sv.numextrastatics;
		state->flags = 0;
		VectorCopy (ent->v->origin, state->origin);
		VectorCopy (ent->v->angles, state->angles);
		state->modelindex = mdlindex;//ent->v->modelindex;
		state->frame = ent->v->frame;
		state->colormap = ent->v->colormap;
		state->skinnum = ent->v->skin;
		state->effects = ent->v->effects;
		state->hexen2flags = ent->xv->drawflags;
		state->abslight = (int)(ent->xv->abslight*255) & 255;
		state->trans = ent->xv->alpha*255;
		if (!ent->xv->alpha)
			state->trans = 255;
		state->fatness = ent->xv->fatness;
		state->scale = ent->xv->scale*16.0;
		if (!ent->xv->scale)
			state->scale = 1*16;

		if (progstype != PROG_QW)	//don't send extra nq effects to a qw client.
			state->effects &= EF_BRIGHTLIGHT | EF_DIMLIGHT;
	}
	else
	{
		MSG_WriteByte (&sv.signon,svc_spawnstatic);

		MSG_WriteByte (&sv.signon, mdlindex&255);

		MSG_WriteByte (&sv.signon, ent->v->frame);
		MSG_WriteByte (&sv.signon, (int)ent->v->colormap);
		MSG_WriteByte (&sv.signon, (int)ent->v->skin);
		for (i=0 ; i<3 ; i++)
		{
			MSG_WriteCoord(&sv.signon, ent->v->origin[i]);
			MSG_WriteAngle(&sv.signon, ent->v->angles[i]);
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
		PR_BIError (prinst, "Entity is not a client");
		return;
	}

	// copy spawn parms out of the client_t
	client = svs.clients + (i-1);

	for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
		if (spawnparamglobals[i])
			*spawnparamglobals[i] = client->spawn_parms[i];
}

/*
==============
PF_changelevel
==============
*/
void PF_changelevel (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*s, *spot;

// make sure we don't issue two changelevels (unless the last one failed)
	if (sv.mapchangelocked)
		return;
	sv.mapchangelocked = true;

	if (*svprogfuncs->callargc == 2)
	{
		s = PR_GetStringOfs(prinst, OFS_PARM0);
		spot = PR_GetStringOfs(prinst, OFS_PARM1);
		Cbuf_AddText (va("\nchangelevel %s %s\n",s, spot), RESTRICT_LOCAL);
	}
	else
	{
		s = PR_GetStringOfs(prinst, OFS_PARM0);
		Cbuf_AddText (va("\nmap %s\n",s), RESTRICT_LOCAL);
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
	if (e1 != e2)	//don't get a point for suicide.
		svs.clients[e1-1].kills += 1;
	svs.clients[e2-1].deaths += 1;
#endif

	s = va("\\%s\\%s\\\n",svs.clients[e1-1].name, svs.clients[e2-1].name);

	SZ_Print (&svs.log[svs.logsequence&1], s);
	if (sv_fraglogfile)
	{
		VFS_WRITE(sv_fraglogfile, s, strlen(s));
		VFS_FLUSH (sv_fraglogfile);
	}
}


/*
==============
PF_infokey

string(entity e, string key) infokey
==============
*/
char *PF_infokey_Internal (int entnum, char *key)
{
	char	*value;
	char ov[256];
	char adr[MAX_ADR_SIZE];

	if (entnum == 0)
	{
		if (pr_imitatemvdsv.value && !strcmp(key, "*version"))
			value = "2.40";
		else
		{
			if ((value = Info_ValueForKey (svs.info, key)) == NULL || !*value)
				value = Info_ValueForKey(localinfo, key);
		}
	}
	else if (entnum <= MAX_CLIENTS)
	{
		value = ov;
		if (!strcmp(key, "ip") || !strcmp(key, "realip"))	//note: FTE doesn't support mvdsv's realip stuff, so pretend that we do if the mod asks
			value = strcpy(ov, NET_BaseAdrToString (adr, sizeof(adr), svs.clients[entnum-1].netchan.remote_address));
		else if (!strcmp(key, "ping"))
			sprintf(ov, "%d", SV_CalcPing (&svs.clients[entnum-1]));
		else if (!strcmp(key, "*userid"))
			sprintf(ov, "%d", svs.clients[entnum-1].userid);
		else if (!strcmp(key, "download"))
			sprintf(ov, "%d", svs.clients[entnum-1].download != NULL ? (int)(100*svs.clients[entnum-1].downloadcount/svs.clients[entnum-1].downloadsize) : -1);
//		else if (!strcmp(key, "login"))	//mvdsv
//			value = "";
		else if (!strcmp(key, "trustlevel"))	//info for progs.
		{
#ifdef SVRANKING
			rankstats_t rs;
			if (!svs.clients[entnum-1].rankid)
				value = "";
			else if (Rank_GetPlayerStats(svs.clients[entnum-1].rankid, &rs))
				sprintf(ov, "%d", rs.trustlevel);
			else
#endif
				value = "";
		}
		else
			value = Info_ValueForKey (svs.clients[entnum-1].userinfo, key);
	} else
		value = "";

	return value;
}

void PF_infokey (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*e;
	int		e1;
	char	*value;
	char	*key;

	e = G_EDICT(prinst, OFS_PARM0);
	e1 = NUM_FOR_EDICT(prinst, e);
	key = PR_GetStringOfs(prinst, OFS_PARM1);

	value = PF_infokey_Internal (e1, key);

	G_INT(OFS_RETURN) = PR_TempString(prinst, value);
}


/*
==============
PF_multicast

void(vector where, float set) multicast
==============
*/
void PF_multicast (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	*o;
	int		to;

	o = G_VECTOR(OFS_PARM0);
	to = G_FLOAT(OFS_PARM1);

	SV_Multicast (o, to);
}


static void PF_Fixme (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
int i;
qboolean printedheader = false;

	SV_EndRedirect();

	for (i = 0; BuiltinList[i].bifunc; i++)
	{
		if (BuiltinList[i].ebfsnum == prinst->lastcalledbuiltinnumber)
		{
			if (!printedheader)
			{
				Con_Printf( "\n"
							"Mod forgot to ensure support for builtin %i\n"
							"Please consult the extensionlist_ssqc command.\n"
							"Possible builtins:\n", prinst->lastcalledbuiltinnumber);
				printedheader = true;
			}
			Con_Printf("%s\n", BuiltinList[i].name);
		}
	}

	Con_Printf("\n");

	if (progstype == PROG_QW)
		prinst->RunError(prinst, "\nBuiltin %i not implemented.\nMods designed for mvdsv may need pr_imitatemvdsv to be enabled.", prinst->lastcalledbuiltinnumber);
	else
		prinst->RunError(prinst, "\nBuiltin %i not implemented.\nMod is not compatible.", prinst->lastcalledbuiltinnumber);
	PR_BIError (prinst, "builtin not implemented");
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

	if (sv_fraglogfile)
	{
		VFS_WRITE(sv_fraglogfile, s, strlen(s));
		VFS_FLUSH(sv_fraglogfile);
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
	s = Z_TagMalloc(len+8, Z_QC_TAG);
	((int *)s)[0] = PRSTR;
	((int *)s)[1] = len;

	strcpy(s+8, in);

	RETURN_SSTRING(s+8);
}

void PF_strcatp(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *buf = PR_GetStringOfs(prinst, OFS_PARM0); char *add = PR_GetStringOfs(prinst, OFS_PARM1);
	int wantedlen = G_FLOAT(OFS_PARM2);
	int len;
	if (((int *)(buf-8))[0] != PRSTR)
	{
		Con_Printf("QC tried to add to a non allocated string\n");
		(*prinst->pr_trace) = 1;
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
		*s=*string|CON_HIGHCHARSMASK;
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
			ClientReliableWrite_Byte(client, svcfte_bulletentext);
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




















// FTE SQL functions
#ifdef SQL
#define SQL_CONNECT_STRUCTPARAMS 2
#define SQL_CONNECT_PARAMS 4

typedef enum 
{
	SQLDRV_MYSQL,
//	SQLDRV_SQLITE, NOT IN YET
	SQLDRV_INVALID
} sqldrv_t;

typedef struct queryrequest_s
{
	int num; // query number reference
	qboolean persistant; // persistant query
	struct queryrequest_s *next; // next request in queue
	int callback; // callback function reference
	int selfent; // self entity on call
	float selfid; // self entity id on call
	int otherent; // other entity on call
	float otherid; // other entity id on call
	char query[1]; // query to run (struct hack)
} queryrequest_t;

typedef struct queryresult_s
{
	struct queryrequest_s *request; // corresponding request
	struct queryresult_s *next; // next result in queue
	int rows; // rows contained in single result set
	int columns; // fields
	qboolean eof; // end of query reached
	MYSQL_RES *result; // result set from mysql
//	char **resultset; // stored result set from partial fetch
	char error[1]; // error string, "" if none (struct hack)
} queryresult_t;

typedef struct sqlserver_s
{
	void *thread; // worker thread for server
	MYSQL *mysql; // mysql server
	volatile qboolean active; // set to false to kill thread
	void *requestcondv; // lock and conditional variable for queue read/write
	void *resultlock; // mutex for queue read/write
	int querynum; // next reference number for queries
	queryrequest_t *requests; // query requests queue
	queryrequest_t *requestslast; // query requests queue last link
	queryresult_t *results; // query results queue
	queryresult_t *resultslast; // query results queue last link
	queryresult_t *currentresult; // current called result
	queryresult_t *persistresults; // list of persistant results
	queryresult_t *serverresult; // server error results
	char **connectparams; // connect parameters (0 = host, 1 = user, 2 = pass, 3 = defaultdb)
} sqlserver_t;

void SQL_PushResult(sqlserver_t *server, queryresult_t *qres)
{
	Sys_LockMutex(server->resultlock);
	qres->next = NULL;
	if (!server->resultslast)
		server->results = server->resultslast = qres;
	else
		server->resultslast = server->resultslast->next = qres;
	Sys_UnlockMutex(server->resultlock);
}

queryresult_t *SQL_PullResult(sqlserver_t *server)
{
	queryresult_t *qres;
	Sys_LockMutex(server->resultlock);
	qres = server->results;
	if (qres)
	{
		server->results = qres->next;
		if (!server->results)
			server->resultslast = NULL;
	}
	Sys_UnlockMutex(server->resultlock);

	return qres;
}

void SQL_PushRequest(sqlserver_t *server, queryrequest_t *qreq)
{
	Sys_LockConditional(server->requestcondv);
	qreq->next = NULL;
	if (!server->requestslast)
		server->requests = server->requestslast = qreq;
	else
		server->requestslast = server->requestslast->next = qreq;
	Sys_UnlockConditional(server->requestcondv);
}

queryrequest_t *SQL_PullRequest(sqlserver_t *server, qboolean lock)
{
	queryrequest_t *qreq;
	if (lock)
		Sys_LockConditional(server->requestcondv);
	qreq = server->requests;
	if (qreq)
	{
		server->requests = qreq->next;
		if (!server->requests)
			server->requestslast = NULL;
	}
	Sys_UnlockConditional(server->requestcondv);

	return qreq;
}

sqlserver_t **sqlservers;
int sqlservercount;
qboolean sqlavailable;

int sql_serverworker(void *sref)
{
	sqlserver_t *server = (sqlserver_t *)sref;
	char *error = NULL;
	my_bool reconnect = 1;
	int tinit, i;
	qboolean needlock = false;

	if (tinit = mysql_thread_init())
		error = "MYSQL thread init failed";
	else if (!(server->mysql = mysql_init(NULL)))
		error = "MYSQL init failed";
	else if (mysql_options(server->mysql, MYSQL_OPT_RECONNECT, &reconnect))
		error = "MYSQL reconnect options set failed";
	else
	{	
		int port = 0;
		char *colon;

		colon = strchr(server->connectparams[0], ':');
		if (colon)
		{
			*colon = '\0';
			port = atoi(colon + 1);
		}

		if (!(server->mysql = mysql_real_connect(server->mysql, server->connectparams[0], server->connectparams[1], server->connectparams[2], server->connectparams[3], port, 0, 0)))
		error = "MYSQL initial connect attempt failed";

		if (colon)
			*colon = ':';
	}

	for (i = SQL_CONNECT_STRUCTPARAMS; i < SQL_CONNECT_PARAMS; i++)
		Z_Free(server->connectparams[i]);

	BZ_Realloc(server->connectparams, sizeof(char *) * SQL_CONNECT_STRUCTPARAMS);

	if (error)
		server->active = false;

	while (server->active)
	{	
		Sys_LockConditional(server->requestcondv);
		if (!server->requests) // this is needed for thread startup and to catch any "lost" changes
		Sys_ConditionWait(server->requestcondv);
		needlock = false; // so we don't try to relock first round

		while (1)
		{
			queryrequest_t *qreq = NULL;
			queryresult_t *qres;
			const char *qerror = NULL;
			MYSQL_RES *mysqlres = NULL;
			int rows = -1;
			int columns = -1;
			int qesize = 0;

			if (!(qreq = SQL_PullRequest(server, needlock)))
				break;

			// pullrequest makes sure our condition is unlocked but we'll need
			// a lock next round
			needlock = true;

			// perform the query and fill out the result structure
			if (mysql_query(server->mysql, qreq->query))
				qerror = mysql_error(server->mysql);
			else // query succeeded
			{
				mysqlres = mysql_store_result(server->mysql);
				if (mysqlres) // result set returned
				{
					rows = mysql_num_rows(mysqlres);
					columns = mysql_num_fields(mysqlres);
				}
				else if (mysql_field_count(server->mysql) == 0) // no result set
				{
					rows = mysql_affected_rows(server->mysql);
					if (rows < 0)
						rows = 0;
					columns = 0;
				}
				else // error
					qerror = mysql_error(server->mysql);
			}

			if (qerror)
				qesize = Q_strlen(qerror);
			qres = (queryresult_t *)ZF_Malloc(sizeof(queryresult_t) + qesize);
			if (qres)
			{
				if (qerror)
					Q_strncpy(qres->error, qerror, qesize);
				qres->result = mysqlres;
				qres->rows = rows;
				qres->columns = columns;
				qres->request = qreq;
				qres->eof = true; // store result has no more rows to read afterwards
				qreq->next = NULL;

				SQL_PushResult(server, qres);
			}
			else // we're screwed here so bomb out
			{
				server->active = false;
				error = "MALLOC ERROR! Unable to allocate query result!";
				break;
			}
		}
	}

	if (server->mysql)
		mysql_close(server->mysql);

	// if we have a server error we still need to put it on the queue
	if (error)
	{ 
		int esize = Q_strlen(error);
		queryresult_t *qres = (queryresult_t *)Z_Malloc(sizeof(queryresult_t) + esize);
		if (qres)
		{ // hopefully the mysql_close gained us some memory otherwise we're pretty screwed
			qres->rows = qres->columns = -1;
			Q_strncpy(qres->error, error, esize);

			SQL_PushResult(server, qres);
		}
	}

	if (!tinit)
		mysql_thread_end();

	return 0;
}

sqlserver_t *SQL_GetServer (int serveridx, qboolean inactives)
{
	if (serveridx < 0 || serveridx >= sqlservercount)
		return NULL;
	if (!inactives && sqlservers[serveridx]->active == false)
		return NULL;
	return sqlservers[serveridx];
}

queryresult_t *SQL_GetQueryResult (sqlserver_t *server, int queryidx)
{
	queryresult_t *qres;

	qres = server->currentresult;
	if (qres && qres->request && qres->request->num == queryidx)
		return qres;

	for (qres = server->persistresults; qres; qres = qres->next)
		if (qres->request && qres->request->num == queryidx)
			return qres;

	return NULL;
}

void PF_sqlconnect (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int serverref;
	char *paramstr[SQL_CONNECT_PARAMS];
	int paramsize[SQL_CONNECT_PARAMS];
	sqlserver_t *server;
	int i, tsize;
	char *driver;
	int drvchoice;

	if (!sqlavailable)
	{
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}

	// check and fit connection parameters
	for (i = 0; i < SQL_CONNECT_PARAMS; i++)
	{
		if (*svprogfuncs->callargc <= (i + 1))
			paramstr[i] = "";
		else
			paramstr[i] = PR_GetStringOfs(prinst, OFS_PARM0+i*3);
	}

	if (!paramstr[0][0])
		paramstr[0] = sql_host.string;
	if (!paramstr[1][0])
		paramstr[1] = sql_username.string;
	if (!paramstr[2][0])
		paramstr[2] = sql_password.string;
	if (!paramstr[3][0])
		paramstr[3] = sql_defaultdb.string;

	for (i = 0; i < SQL_CONNECT_PARAMS; i++)
		paramsize[i] = Q_strlen(paramstr[i]);

	// verify/switch driver choice
	if (*svprogfuncs->callargc > (SQL_CONNECT_PARAMS + 1))
		driver = PR_GetStringOfs(prinst, OFS_PARM0 + SQL_CONNECT_PARAMS * 3);
	else
		driver = "";

	if (!driver[0])
		driver = sql_driver.string;

	if (Q_strcasecmp(driver, "mysql") == 0)
		drvchoice = SQLDRV_MYSQL;
	else // invalid driver choice so we bomb out
	{ 
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}

	// alloc or realloc sql servers array
	if (sqlservers == NULL)
	{
		serverref = 0;
		sqlservercount = 1;
		sqlservers = (sqlserver_t **)BZ_Malloc(sizeof(sqlserver_t *));
	}
	else
	{
		serverref = sqlservercount;
		sqlservercount++;
		sqlservers = (sqlserver_t **)BZ_Realloc(sqlservers, sizeof(sqlserver_t *) * sqlservercount);
	}

	// assemble server structure
	tsize = 0;
	for (i = 0; i < SQL_CONNECT_STRUCTPARAMS; i++)
		tsize += paramsize[i] + 1;	// allocate extra space for host and user only

	server = (sqlserver_t *)Z_Malloc(sizeof(sqlserver_t) + tsize);
	server->connectparams = BZ_Malloc(sizeof(char *) * SQL_CONNECT_PARAMS);

	tsize = 0;
	for (i = 0; i < SQL_CONNECT_STRUCTPARAMS; i++)
	{
		server->connectparams[i] = ((char *)(server + 1)) + tsize;
		Q_strncpy(server->connectparams[i], paramstr[i], paramsize[i]);
		// string should be null-terminated due to Z_Malloc
		tsize += paramsize[i] + 1;
	}
	for (i = SQL_CONNECT_STRUCTPARAMS; i < SQL_CONNECT_PARAMS; i++)
	{
		server->connectparams[i] = Z_Malloc(sizeof(char) * (paramsize[i] + 1));
		Q_strncpy(server->connectparams[i], paramstr[i], paramsize[i]);
		// string should be null-terminated due to Z_Malloc
	}

	sqlservers[serverref] = server;

	server->querynum = 1;
	server->active = true;
	server->requestcondv = Sys_CreateConditional();
	server->resultlock = Sys_CreateMutex();

	if (!server->requestcondv || !server->resultlock)
	{
		if (server->requestcondv)
			Sys_DestroyConditional(server->requestcondv);
		if (server->resultlock)
			Sys_DestroyMutex(server->resultlock);
		Z_Free(server);
		sqlservercount--;
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}

	server->thread = Sys_CreateThread(sql_serverworker, (void *)server, 1024);
	
	if (!server->thread)
	{
		Z_Free(server);
		sqlservercount--;
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}
	
	G_FLOAT(OFS_RETURN) = serverref;
}

void PF_sqldisconnect (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	sqlserver_t *server;

	if (sqlavailable)
	{
		server = SQL_GetServer(G_FLOAT(OFS_PARM0), false);
		if (server)
		{
			server->active = false;

			// force the threads to reiterate requests and hopefully terminate
			Sys_ConditionBroadcast(server->requestcondv);
			return;
		}
	}
}

void PF_sqlopenquery (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int callfunc = G_INT(OFS_PARM1);
	int querytype = G_FLOAT(OFS_PARM2);
	char *querystr = PF_VarString(prinst, 3, pr_globals);
	int qsize = Q_strlen(querystr);
	queryrequest_t *qreq;
	sqlserver_t *server;
	int querynum;

	if (sqlavailable)
	{
		server = SQL_GetServer(G_FLOAT(OFS_PARM0), false);
		if (server)
		{
			qreq = (queryrequest_t *)ZF_Malloc(sizeof(queryrequest_t) + qsize);
			if (qreq)
			{
				qreq->persistant = (querytype == 1);
				qreq->callback = callfunc;

				// save self and other references
				if (PROG_TO_EDICT(prinst, pr_global_struct->self)->isfree)
					qreq->selfent = pr_global_struct->world;
				else
					qreq->selfent = pr_global_struct->self;
				qreq->selfid = PROG_TO_EDICT(prinst, qreq->selfent)->xv->uniquespawnid;
				if (PROG_TO_EDICT(prinst, pr_global_struct->other)->isfree)
					qreq->otherent = pr_global_struct->world;
				else
					qreq->otherent = pr_global_struct->other;
				qreq->otherid = PROG_TO_EDICT(prinst, qreq->otherent)->xv->uniquespawnid;

				querynum = qreq->num = server->querynum;
				// prevent the reference num from getting too big to prevent FP problems
				if (++server->querynum > 1000000)
					server->querynum = 1; 
				
				Q_strncpy(qreq->query, querystr, qsize);

				SQL_PushRequest(server, qreq);
				Sys_ConditionSignal(server->requestcondv);

				G_FLOAT(OFS_RETURN) = querynum;
				return;
			}
		}
	}
	// else we failed so return the error
	G_FLOAT(OFS_RETURN) = -1;
}

void SQL_DeallocResult(queryresult_t *qres)
{
	// deallocate current result
	if (qres->result)
		mysql_free_result(qres->result);
	if (qres->request)
		Z_Free(qres->request);

	Z_Free(qres);
}

void SQL_ClosePersistantResult(sqlserver_t *server, queryresult_t *qres)
{
	queryresult_t *prev, *cur;

	prev = server->persistresults;
	if (prev == qres)
	{
		server->persistresults = prev->next;
		SQL_DeallocResult(prev);
		return;
	}

	for (cur = prev->next; cur; prev = cur, cur = prev->next)
	{
		if (cur == qres)
		{
			prev = cur->next;
			SQL_DeallocResult(cur);
			return;
		}
	}
}

void SQL_CloseResult(sqlserver_t *server, queryresult_t *qres)
{
	if (!qres)
		return;
	if (qres == server->currentresult)
	{
		SQL_DeallocResult(server->currentresult);
		server->currentresult = NULL;
		return;
	}
	// else we have a persistant query
	SQL_ClosePersistantResult(server, qres);
}

void SQL_CloseAllResults(sqlserver_t *server)
{
	queryresult_t *oldqres, *qres;

	// close orphaned results (we assume the lock is active or non-existant at this point)
	qres = server->results;
	while (qres)
	{
		oldqres = qres;
		qres = qres->next;
		SQL_DeallocResult(oldqres);
	}
	// close current
	if (server->currentresult)
	{
		SQL_DeallocResult(server->currentresult);
		server->currentresult = NULL;
	}
	// close persistant results
	qres = server->persistresults;
	while (qres)
	{
		oldqres = qres;
		qres = qres->next;
		SQL_DeallocResult(oldqres);
	}
	server->persistresults = NULL;
	// close server result
	if (server->serverresult)
	{
		SQL_DeallocResult(server->serverresult);
		server->serverresult = NULL;
	}
}

void PF_sqlclosequery (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	sqlserver_t *server;
	queryresult_t *qres;
	
	if (sqlavailable)
	{
		server = SQL_GetServer(G_FLOAT(OFS_PARM0), false);
		if (server)
		{
			qres = SQL_GetQueryResult(server, G_FLOAT(OFS_PARM1));
			if (qres) 
			{
				// TODO: partial resultset logic not implemented yet
				SQL_CloseResult(server, qres);
				return;
			}
		}
	}
	// else nothing to close
}

char *SQL_ReadField (sqlserver_t *server, queryresult_t *qres, int row, int col, qboolean fields)
{
	if (!qres->result) // TODO: partial resultset logic not implemented yet
		return NULL;
	else
	{ // store_result query
		if (qres->rows < row || qres->columns < col || col < 0)
			return NULL;

		if (row < 0)
		{ // fetch field name
			if (fields) // but only if we asked for them
			{
				MYSQL_FIELD *field;

				field = mysql_fetch_field_direct(qres->result, col);

				if (!field)
					return NULL;
				else
					return field->name;
			}
			else
				return NULL;
		}
		else
		{ // fetch data
			MYSQL_ROW sqlrow;

			mysql_data_seek(qres->result, row);
			sqlrow = mysql_fetch_row(qres->result);
			if (!sqlrow || !sqlrow[col])
				return NULL;
			else
				return sqlrow[col];
		}
	}
}

void PF_sqlreadfield (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	sqlserver_t *server;
	queryresult_t *qres;
	char *data;

	if (sqlavailable)
	{	
		server = SQL_GetServer(G_FLOAT(OFS_PARM0), false);
		if (server)
		{
			qres = SQL_GetQueryResult(server, G_FLOAT(OFS_PARM1));
			if (qres)
			{
				data = SQL_ReadField(server, qres, G_FLOAT(OFS_PARM2), G_FLOAT(OFS_PARM3), true);
				if (data)
				{
					RETURN_TSTRING(data);
					return;
				}
			}
		}
	}
	// else we failed to get anything
	G_INT(OFS_RETURN) = 0;
}

void PF_sqlreadfloat (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	sqlserver_t *server;
	queryresult_t *qres;
	char *data;

	if (sqlavailable)
	{	
		server = SQL_GetServer(G_FLOAT(OFS_PARM0), false);
		if (server)
		{
			qres = SQL_GetQueryResult(server, G_FLOAT(OFS_PARM1));
			if (qres)
			{
				data = SQL_ReadField(server, qres, G_FLOAT(OFS_PARM2), G_FLOAT(OFS_PARM3), true);
				if (data)
				{
					G_FLOAT(OFS_RETURN) = Q_atof(data);
					return;
				}
			}
		}
	}
	// else we failed to get anything
	G_FLOAT(OFS_RETURN) = 0;
}


void PF_sqlerror (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	sqlserver_t *server;
	int serverref = G_FLOAT(OFS_PARM0);

	if (sqlavailable)
	{
		server = SQL_GetServer(G_FLOAT(OFS_PARM0), true);
		if (server)
		{
			if (*svprogfuncs->callargc == 2)
			{ // query-specific error request
				if (server->active) // didn't check this earlier so check it now
				{ 
					queryresult_t *qres = SQL_GetQueryResult(server, G_FLOAT(OFS_PARM1));
					if (qres)
					{
						RETURN_TSTRING(qres->error);
						return;
					}
				}
			}
			else if (server->serverresult)
			{ // server-specific error request
				RETURN_TSTRING(server->serverresult->error);
				return;
			}
		}
	}
	// else we didn't get a server or query
	RETURN_TSTRING("");
}

void PF_sqlescape (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	sqlserver_t *server;
	char *toescape;
	char escaped[4096];

	if (sqlavailable)
	{
		server = SQL_GetServer(G_FLOAT(OFS_PARM0), false);
		if (server)
		{
			toescape = PR_GetStringOfs(prinst, OFS_PARM1);
			if (toescape)
			{
				mysql_real_escape_string(server->mysql, escaped, toescape, strlen(toescape));

				RETURN_TSTRING(escaped);
				return;
			}
		}
	}
	// else invalid string or server reference
	RETURN_TSTRING("");
}

void PF_sqlversion (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	sqlserver_t *server;
	
	if (sqlavailable)
	{
		server = SQL_GetServer(G_FLOAT(OFS_PARM0), false);
		if (server)
		{
			RETURN_TSTRING(va("mysql: %s", mysql_get_client_info()));
			return;
		}
	}
	// else invalid string or server reference
	RETURN_TSTRING("");
}

// SQL related commands
void SQL_Status_f(void)
{
	int i;

	Con_Printf("%i connections\n", sqlservercount);
	for (i = 0; i < sqlservercount; i++)
	{
		int reqnum = 0;
		int resnum = 0;
		queryrequest_t *qreq;
		queryresult_t *qres;

		sqlserver_t *server = sqlservers[i];

		Sys_LockMutex(server->resultlock);
		Sys_LockConditional(server->requestcondv);
		for (qreq = server->requests; qreq; qreq = qreq->next)
			reqnum++;
		for (qres = server->results; qres; qres = qres->next)
			resnum++;

		Con_Printf("#%i %s@%s: %s\n",
			i,
			server->connectparams[1],
			server->connectparams[0],
			server->active ? "active" : "inactive");

		if (reqnum)
		{
			Con_Printf ("- %i requests\n");
			for (qreq = server->requests; qreq; qreq = qreq->next)
			{
				Con_Printf ("  query #%i: %s\n",
					qreq->num,
					qreq->query);
				// TODO: function lookup?
			}
		}

		if (resnum)
		{
			Con_Printf ("- %i results\n");
			for (qres = server->results; qres; qres = qres->next)
			{
				Con_Printf ("  * %i rows, %i columns", 
					qres->rows,
					qres->columns);
				if (qres->error[0])
					Con_Printf(", error %s\n", qres->error);
				else
					Con_Printf("\n");
				// TODO: request info?
			}
		}

		if (server->serverresult)
			Con_Printf ("server result: error %s\n", server->serverresult->error);

		// TODO: list all requests, results here
		Sys_UnlockMutex(server->resultlock);
		Sys_UnlockConditional(server->requestcondv);
	}
}

void SQL_Kill_f (void)
{
	sqlserver_t *server;

	if (Cmd_Argc() < 2)
	{
		Con_Printf ("Syntax: %s serverid\n", Cmd_Argv(0));
		return;
	}

	server = SQL_GetServer(atoi(Cmd_Argv(1)), false);
	if (server)
	{
		server->active = false;
		Sys_ConditionBroadcast(server->requestcondv);
		return;
	}
}

void SQL_Killall_f (void)
{
	SQL_KillServers();
}

// SQL cycle logic
void SQL_Cycle (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i;

	for (i = 0; i < sqlservercount; i++)
	{
		sqlserver_t *server = sqlservers[i];
		queryresult_t *qres;

		while (qres = SQL_PullResult(server))
		{
			qres->next = NULL;
			if (qres->request && qres->request->callback)
			{
				if (server->active)
				{ // only process results to callback if server is active
					edict_t *ent;

					server->currentresult = qres;
					G_FLOAT(OFS_PARM0) = i;
					G_FLOAT(OFS_PARM1) = qres->request->num;
					G_FLOAT(OFS_PARM2) = qres->rows;
					G_FLOAT(OFS_PARM3) = qres->columns;
					G_FLOAT(OFS_PARM4) = qres->eof;

					// recall self and other references
					ent = PROG_TO_EDICT(prinst, qres->request->selfent);
					if (ent->isfree || ent->xv->uniquespawnid != qres->request->selfid)
						pr_global_struct->self = pr_global_struct->world;
					else
						pr_global_struct->self = qres->request->selfent;
					ent = PROG_TO_EDICT(prinst, qres->request->otherent);
					if (ent->isfree || ent->xv->uniquespawnid != qres->request->otherid)
						pr_global_struct->other = pr_global_struct->world;
					else
						pr_global_struct->other = qres->request->otherent;

					PR_ExecuteProgram(prinst, qres->request->callback);

					if (qres->eof)
					{
						if (server->currentresult)
						{
							if (server->currentresult->request && server->currentresult->request->persistant)
							{
								// move into persistant list
								server->currentresult->next = server->persistresults;
								server->persistresults = server->currentresult;
							}
							else // just close the query
								SQL_CloseResult(server, server->currentresult);
						}
					}
					// TODO: else we move a request back into the queue?
				}
			}
			else // error or server-only result
			{
				if (server->serverresult)
					Z_Free(server->serverresult);
				server->serverresult = qres;
			}
		}
		server->currentresult = NULL;
	}
}

void PR_SQLCycle(void)
{
	globalvars_t *pr_globals;

	if (!sqlavailable || !svprogfuncs)
		return;

	pr_globals = PR_globals(svprogfuncs, PR_CURRENT);

	SQL_Cycle(svprogfuncs, pr_globals);
}

void SQL_MYSQLInit(void)
{
#ifdef WIN32
	if (!mysql_dll_init())
	{
		Con_Printf("mysqlclient.dll didn't load\n");
		return;
	}
#endif

	if (mysql_thread_safe())
	{
		if (!mysql_library_init(0, NULL, NULL))
		{
			Con_Printf("MYSQL backend loaded\n");
			sqlavailable = true;
			return;
		}
		else
			Con_Printf("MYSQL library init failed!\n");
	}
	else
		Con_Printf("MYSQL client is not thread safe!\n");

	mysql_dll_close();
	sqlavailable = false;
}

void SQL_Init(void)
{
	Cmd_AddCommand ("sqlstatus", SQL_Status_f);
	Cmd_AddCommand ("sqlkill", SQL_Kill_f);
	Cmd_AddCommand ("sqlkillall", SQL_Killall_f);

	Cvar_Register(&sql_driver, SQLCVAROPTIONS);
	Cvar_Register(&sql_host, SQLCVAROPTIONS);
	Cvar_Register(&sql_username, SQLCVAROPTIONS);
	Cvar_Register(&sql_password, SQLCVAROPTIONS);
	Cvar_Register(&sql_defaultdb, SQLCVAROPTIONS);

	SQL_MYSQLInit();
}

void SQL_KillServers(void)
{
	int i;
	for (i = 0; i < sqlservercount; i++)
	{
		sqlserver_t *server = sqlservers[i];
		queryrequest_t *qreq, *oldqreq;

		server->active = false; // set thread to kill itself
		Sys_ConditionBroadcast(server->requestcondv); // force condition check
		Sys_WaitOnThread(server->thread); // wait on thread to die

		// server resource deallocation (TODO: should this be done in the thread itself?)
		Sys_DestroyConditional(server->requestcondv);
		Sys_DestroyMutex(server->resultlock);
		
		// close orphaned requests
		qreq = server->requests;
		while (qreq)
		{
			oldqreq = qreq;
			qreq = qreq->next;
			Z_Free(oldqreq);
		}

		SQL_CloseAllResults(server);

		// the alloc'ed connect params should get deallocated by the thread
		if (server->connectparams)
			BZ_Free(server->connectparams);

		Z_Free(server);
	}
	if (sqlservers)
		Z_Free(sqlservers);
	sqlservers = NULL;
	sqlservercount = 0;
}

void SQL_DeInit(void)
{
	sqlavailable = false;

	SQL_KillServers();

	mysql_library_end();

	mysql_dll_close();
}
#endif






int PR_EnableEBFSBuiltin(char *name, int binum)
{
	int i;
	for (i = 0;BuiltinList[i].name;i++)
	{
		if (!strcmp(BuiltinList[i].name, name))
		{
			if (!binum)
				binum = BuiltinList[i].ebfsnum;
			if (!pr_overridebuiltins.value)
			{
				if (pr_builtin[binum] != NULL && pr_builtin[binum] != PF_Fixme)
				{
					if (pr_builtin[binum] == BuiltinList[i].bifunc)	//it is already this function.
						return binum;

					return 0;	//already used... ?
				}
			}

			pr_builtin[binum] = BuiltinList[i].bifunc;

			return binum;
		}
	}

	return 0;	//not known
}



lh_extension_t *checkfteextensioncl(int mask, char *name)	//true if the cient extension mask matches an extension name
{
	int i;
	for (i = 0; i < 32; i++)
	{
		if (mask & (1<<i))	//suported
		{
			if (QSG_Extensions[i].name)	//some were removed
				if (!stricmp(name, QSG_Extensions[i].name))	//name matches
					return &QSG_Extensions[i];
		}
	}
	return NULL;
}

lh_extension_t *checkfteextensionsv(char *name)	//true if the server supports an protocol extension.
{
	int i;

	for (i = 0; i < 32; i++)
	{
		if (svs.fteprotocolextensions & (1<<i))
		{
			if (QSG_Extensions[i].name)	//some were removed
				if (!stricmp(name, QSG_Extensions[i].name))	//name matches
					return &QSG_Extensions[i];
		}
	}
	return NULL;
}

lh_extension_t *checkextension(char *name)
{
	int i;
	for (i = 32; i < QSG_Extensions_count; i++)
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
			if (!PR_EnableEBFSBuiltin(ext->builtinnames[i], 0))
			{
				Con_Printf("Failed to initialise builtin \"%s\" for extension \"%s\"", ext->builtinnames[i], s);
				return;	//whoops, we failed.
			}
		}

		if (ext->enabled)
			*ext->enabled = true;

		G_FLOAT(OFS_RETURN) = true;
		Con_DPrintf("Extension %s is supported\n", s);
	}
	else
		G_FLOAT(OFS_RETURN) = false;
}


void PF_builtinsupported (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *s = PR_GetStringOfs(prinst, OFS_PARM0);

	G_FLOAT(OFS_RETURN) = PR_EnableEBFSBuiltin(s, 0);
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
	char dest[4096];
	char *s;
	int start, len, l;

	s = PR_GetStringOfs(prinst, OFS_PARM0);
	start = (int) G_FLOAT(OFS_PARM1);
	len = (int) G_FLOAT(OFS_PARM2);
	l = strlen(s);

	if (start >= l || len<=0 || !*s)
	{
		RETURN_TSTRING("");
		return;
	}

	s += start;
	l -= start;

	if (len > l + 1)
		len = l + 1;

	if (len > sizeof(dest)-1)
		len = sizeof(dest)-1;

	Q_strncpyz(dest, s, len + 1);

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
	extern int sv_redirectedlang;
	redirect_t old;
	int oldl;

	s = PR_GetStringOfs(prinst, OFS_PARM0);

	Cbuf_Execute();
	Cbuf_AddText (s, RESTRICT_LOCAL);

	old = sv_redirected;
	oldl = sv_redirectedlang;
	if (old != RD_NONE)
		SV_EndRedirect();

	SV_BeginRedirect(RD_OBLIVION, LANGDEFAULT);
	Cbuf_Execute();
	Q_strncpyz(output, outputbuf, sizeof(output));
	SV_EndRedirect();

	if (old != RD_NONE)
		SV_BeginRedirect(old, oldl);

Con_Printf("PF_readcmd: %s\n%s", s, output);
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
		G_INT(OFS_PARM6) = (int)PR_TempString(prinst, date.str);

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

void PF_MVDSV_strcpy (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *src = PR_GetStringOfs(prinst, OFS_PARM1);
	char *dest = PR_GetStringOfs(prinst, OFS_PARM0);
	int *ident;
	ident = (int *)(dest-8);

/*
	if (*ident != PRSTR)
	{
		Con_Printf("PF_strcpy: not an allocated string\n");
		return;
	}
	if (ident[1] < strlen(src)+1)
	{
		Con_Printf("PF_strcpy: allocated string is not big enough.\n");
		return;
	}
*/
	strcpy(dest, src);
}

/*
=================
PF_strncpy

void strcpy(string dst, string src, float count)
FIXME: check for null pointers first?
=================
*/

void PF_MVDSV_strncpy (progfuncs_t *prinst, struct globalvars_s *pr_globals)
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

char readable2[256] = 
{
	'.', '_', '_', '_', '_', '.', '_', '_', 
	'_', '_', '\n', '_', '\n', '>', '.', '.', 
	'[', ']', '0', '1', '2', '3', '4', '5', 
	'6', '7', '8', '9', '.', '_', '_', '_', 
	' ', '!', '\"', '#', '$', '%', '&', '\'', 
	'(', ')', '*', '+', ',', '-', '.', '/', 
	'0', '1', '2', '3', '4', '5', '6', '7', 
	'8', '9', ':', ';', '<', '=', '>', '?', 
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 
	'X', 'Y', 'Z', '[', '\\', ']', '^', '_', 
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 
	'x', 'y', 'z', '{', '|', '}', '~', '_', 
	'_', '_', '_', '_', '_', '.', '_', '_', 
	'_', '_', '_', '_', '_', '>', '.', '.', 
	'[', ']', '0', '1', '2', '3', '4', '5', 
	'6', '7', '8', '9', '.', '_', '_', '_', 
	' ', '!', '\"', '#', '$', '%', '&', '\'', 
	'(', ')', '*', '+', ',', '-', '.', '/', 
	'0', '1', '2', '3', '4', '5', '6', '7', 
	'8', '9', ':', ';', '<', '=', '>', '?', 
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 
	'X', 'Y', 'Z', '[', '\\', ']', '^', '_', 
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 
	'x', 'y', 'z', '{', '|', '}', '~', '_'
};

void PR_CleanText(unsigned char *text)
{
	for ( ; *text; text++)
		*text = readable2[*text];
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
	vfsfile_t *file;

	snprintf(name, MAX_OSPATH, "%s.log", PR_GetStringOfs(prinst, OFS_PARM0));
	text = PF_VarString(prinst, 2, pr_globals);
	PR_CleanText(text);

	file = FS_OpenVFS(name, "ab", FS_GAME);
	if (file == NULL)
	{
		Sys_Printf("coldn't open log file %s\n", name);
	}
	else
	{
		VFS_WRITE(file, text, strlen(text));
		VFS_CLOSE (file);
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
static void PF_copyentity (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t *in, *out;

	in = G_EDICT(prinst, OFS_PARM0);
	out = G_EDICT(prinst, OFS_PARM1);

	memcpy(out->v, in->v, pr_edict_size);
	SV_LinkEdict(out, false);
}


//EXTENSION: DP_QC_FINDCHAIN

//entity(string field, string match) findchain = #402
//chained search for strings in entity fields
void PF_sv_findchain (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i, f;
	char *s;
	string_t t;
	edict_t *ent, *chain;

	chain = (edict_t *) *prinst->parms->sv_edicts;

	f = G_INT(OFS_PARM0)+prinst->fieldadjust;
	s = PR_GetStringOfs(prinst, OFS_PARM1);

	for (i = 1; i < *prinst->parms->sv_num_edicts; i++)
	{
		ent = EDICT_NUM(prinst, i);
		if (ent->isfree)
			continue;
		t = *(string_t *)&((float*)ent->v)[f];
		if (!t)
			continue;
		if (strcmp(PR_GetString(prinst, t), s))
			continue;

		ent->v->chain = EDICT_TO_PROG(prinst, chain);
		chain = ent;
	}

	RETURN_EDICT(prinst, chain);
}

//EXTENSION: DP_QC_FINDCHAINFLOAT

//entity(string field, float match) findchainfloat = #403
//chained search for float, int, and entity reference fields
void PF_sv_findchainfloat (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i, f;
	float s;
	edict_t	*ent, *chain;

	chain = (edict_t *) *prinst->parms->sv_edicts;

	f = G_INT(OFS_PARM0)+prinst->fieldadjust;
	s = G_FLOAT(OFS_PARM1);

	for (i = 1; i < *prinst->parms->sv_num_edicts; i++)
	{
		ent = EDICT_NUM(prinst, i);
		if (ent->isfree)
			continue;
		if (((float *)ent->v)[f] != s)
			continue;

		ent->v->chain = EDICT_TO_PROG(prinst, chain);
		chain = ent;
	}

	RETURN_EDICT(prinst, chain);
}

//EXTENSION: DP_QC_FINDCHAINFLAGS

//entity(string field, float match) findchainflags = #450
//chained search for float, int, and entity reference fields
void PF_sv_findchainflags (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i, f;
	int s;
	edict_t	*ent, *chain;

	chain = (edict_t *) *prinst->parms->sv_edicts;

	f = G_INT(OFS_PARM0)+prinst->fieldadjust;
	s = G_FLOAT(OFS_PARM1);

	for (i = 1; i < *prinst->parms->sv_num_edicts; i++)
	{
		ent = EDICT_NUM(prinst, i);
		if (ent->isfree)
			continue;
		if (!((int)((float *)ent->v)[f] & s))
			continue;

		ent->v->chain = EDICT_TO_PROG(prinst, chain);
		chain = ent;
	}

	RETURN_EDICT(prinst, chain);
}

//EXTENSION: DP_QC_VECTORVECTORS

//void(vector dir) vectorvectors = #432
//Writes new values for v_forward, v_up, and v_right based on the given forward vector
static void PF_vectorvectors (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	VectorCopy(G_VECTOR(OFS_PARM0), P_VEC(v_forward));
	VectorNormalize(P_VEC(v_forward));
	VectorVectors(P_VEC(v_forward), P_VEC(v_right), P_VEC(v_up));
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
	{
		PR_BIError(prinst, "PF_clientcommand: entity is not a client");
		return;
	}

	temp_client = host_client;
	host_client = &svs.clients[i];
	if (host_client->state == cs_connected || host_client->state == cs_spawned)
	{
		SV_ExecuteUserCommand (PF_VarString(prinst, 1, pr_globals), true);
	}
	else
		Con_Printf("PF_clientcommand: client is not active\n");
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

	if ((Start<End&&(Ent->v->frame < Start || Ent->v->frame > End))||
		(Start>End&&(Ent->v->frame > Start || Ent->v->frame < End)))
	{ // Didn't start in the range
		Ent->v->frame = Start;
		Result = 0;
	}
	else if(Ent->v->frame == End)
	{  // Wrapping
		Ent->v->frame = Start;
		Result = 1;
	}
	else if(End>Start)
	{  // Regular Advance
		Ent->v->frame++;
		if (Ent->v->frame == End)
			Result = 2;
		else
			Result = 0;
	}
	else if(End<Start)
	{  // Reverse Advance
		Ent->v->frame--;
		if (Ent->v->frame == End)
			Result = 2;
		else
			Result = 0;
	}
	else
	{
		Ent->v->frame=End;
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

	if (Ent->v->frame > Start || Ent->v->frame < End)
	{ // Didn't start in the range
		Ent->v->frame = Start;
		Result = 0;
	}
	else if(Ent->v->frame == End)
	{  // Wrapping
		Ent->v->frame = Start;
		Result = 1;
	}
	else
	{  // Regular Advance
		Ent->v->frame--;
		if (Ent->v->frame == End) Result = 2;
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

	if ((endframe > startframe && (ent->v->weaponframe > endframe || ent->v->weaponframe < startframe)) ||
	(endframe < startframe && (ent->v->weaponframe < endframe || ent->v->weaponframe > startframe)) )
	{
		ent->v->weaponframe=startframe;
		state = WF_CYCLE_STARTED;
	}
	else if(ent->v->weaponframe==endframe)
	{
		ent->v->weaponframe=startframe;
		state = WF_CYCLE_WRAPPED;
	}
	else
	{
		if (startframe > endframe)
			ent->v->weaponframe = ent->v->weaponframe - 1;
		else if (startframe < endframe)
			ent->v->weaponframe = ent->v->weaponframe + 1;

		if (ent->v->weaponframe==endframe)
			state = WF_LAST_FRAME;
		else
			state = WF_NORMAL_ADVANCE;
	}

	G_FLOAT(OFS_RETURN) = state;
}

void PR_SetPlayerClass(client_t *cl, int classnum, qboolean fromqc)
{
	char		temp[16];
	if (classnum < 1)
		return;	//reject it (it would crash the (standard hexen2) mod)
	if (classnum > 5)
		return;
	if (cl->playerclass != classnum)
	{
		cl->edict->xv->playerclass = classnum;
		cl->playerclass = classnum;

		sprintf(temp,"%i",(int)classnum);
		Info_SetValueForKey (cl->userinfo, "cl_playerclass", temp, sizeof(cl->userinfo));

		if (!fromqc)
		{
			cl->sendinfo = true;
			if (cl->state == cs_spawned && gfuncs.ClassChangeWeapon)
			{
				pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, cl->edict);
				PR_ExecuteProgram (svprogfuncs, gfuncs.ClassChangeWeapon);
			}
		}
	}
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

	e->xv->playerclass = NewClass;
	client->playerclass = NewClass;

	sprintf(temp,"%d",(int)NewClass);
	Info_SetValueForKey (client->userinfo, "playerclass", temp, sizeof(client->userinfo));
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

	if (G_FLOAT(OFS_PARM0) == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		if (!cl)
			return;
		ClientReliableWrite_Begin (cl, svc_centerprint, 4 + strlen(s));
		if (*s)
		{
			ClientReliableWrite_Byte (cl, '/');
			ClientReliableWrite_Byte (cl, 'P');
		}
		ClientReliableWrite_String (cl, s);
	}
	else
	{
		MSG_WriteByte (QWWriteDest(G_FLOAT(OFS_PARM0)), svc_centerprint);
		if (*s)
		{
			MSG_WriteByte (QWWriteDest(G_FLOAT(OFS_PARM0)), '/');
			MSG_WriteByte (QWWriteDest(G_FLOAT(OFS_PARM0)), 'P');
		}
		MSG_WriteString (QWWriteDest(G_FLOAT(OFS_PARM0)), s);
#ifdef NQPROT
		MSG_WriteByte (NQWriteDest(G_FLOAT(OFS_PARM0)), svc_centerprint);
		if (*s)
		{
			MSG_WriteByte (NQWriteDest(G_FLOAT(OFS_PARM0)), '/');
			MSG_WriteByte (NQWriteDest(G_FLOAT(OFS_PARM0)), 'P');
		}
		MSG_WriteString (NQWriteDest(G_FLOAT(OFS_PARM0)), s);
#endif
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

	AngleVectors(actor->v->angles, old_forward, old_right, P_VEC(v_up));

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

	actor->v->angles[0] = dot*pitch;
	actor->v->angles[2] = (1-fabs(dot))*pitch*mod;
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
		PR_BIError (prinst, "PF_RegisterTEnt: Registration can only be done in spawn functions");
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

	MSG_WriteByte(&sv.multicast, svcfte_customtempent);
	MSG_WriteByte(&sv.multicast, type);
	MSG_WriteCoord(&sv.multicast, org[0]);
	MSG_WriteCoord(&sv.multicast, org[1]);
	MSG_WriteCoord(&sv.multicast, org[2]);

	type = sv.customtents[type].netstyle;
	arg = 2;
	if (type & CTE_ISBEAM)
	{
		MSG_WriteCoord(&sv.multicast, G_VECTOR(OFS_PARM0+arg*3)[0]);
		MSG_WriteCoord(&sv.multicast, G_VECTOR(OFS_PARM0+arg*3)[1]);
		MSG_WriteCoord(&sv.multicast, G_VECTOR(OFS_PARM0+arg*3)[2]);
		arg++;
	}
	else
	{
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
	}
	if (arg != *prinst->callargc)
		Con_Printf("PF_CusromTEnt: bad number of arguments for particle type\n");

	SV_MulticastProtExt (org, MULTICAST_PVS, pr_global_struct->dimension_send, PEXT_CUSTOMTEMPEFFECTS, 0);	//now send the new multicast to all that will.
}

//float(string effectname) particleeffectnum (EXT_CSQC)
void PF_sv_particleeffectnum(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
#pragma message("PF_sv_particleeffectnum: which effect index values to use?")
	char *efname = PR_GetStringOfs(prinst, OFS_PARM0);
	G_FLOAT(OFS_RETURN) = COM_Effectinfo_ForName(efname);
}
//void(float effectnum, entity ent, vector start, vector end) trailparticles (EXT_CSQC),
void PF_sv_trailparticles(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
#ifdef PEXT_CSQC
#pragma message("PF_sv_trailparticles: first two parameters differ from dp, but comply with spec")
	int efnum = G_FLOAT(OFS_PARM0);
	int ednum = G_EDICTNUM(prinst, OFS_PARM1);
	float *start = G_VECTOR(OFS_PARM2);
	float *end = G_VECTOR(OFS_PARM3);

	MSG_WriteByte(&sv.multicast, svcfte_trailparticles);
	MSG_WriteShort(&sv.multicast, ednum);
	MSG_WriteShort(&sv.multicast, efnum);
	MSG_WriteCoord(&sv.multicast, start[0]);
	MSG_WriteCoord(&sv.multicast, start[1]);
	MSG_WriteCoord(&sv.multicast, start[2]);
	MSG_WriteCoord(&sv.multicast, end[0]);
	MSG_WriteCoord(&sv.multicast, end[1]);
	MSG_WriteCoord(&sv.multicast, end[2]);

	MSG_WriteByte(&sv.nqmulticast, svcdp_trailparticles);
	MSG_WriteShort(&sv.nqmulticast, ednum);
	MSG_WriteShort(&sv.nqmulticast, efnum);
	MSG_WriteCoord(&sv.nqmulticast, start[0]);
	MSG_WriteCoord(&sv.nqmulticast, start[1]);
	MSG_WriteCoord(&sv.nqmulticast, start[2]);
	MSG_WriteCoord(&sv.nqmulticast, end[0]);
	MSG_WriteCoord(&sv.nqmulticast, end[1]);
	MSG_WriteCoord(&sv.nqmulticast, end[2]);

	SV_MulticastProtExt(start, MULTICAST_PHS, ~0, PEXT_CSQC, 0);
#endif
}
//void(float effectnum, vector origin [, vector dir, float count]) pointparticles (EXT_CSQC)
void PF_sv_pointparticles(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
#ifdef PEXT_CSQC
	int efnum = G_FLOAT(OFS_PARM0);
	float *org = G_VECTOR(OFS_PARM1);
	float *vel = G_VECTOR(OFS_PARM2);
	int count = G_FLOAT(OFS_PARM3);

	if (count > 65535)
		count = 65535;

	if (count == 1 && DotProduct(org, org) == 0)
	{
		MSG_WriteByte(&sv.multicast, svcfte_pointparticles1);
		MSG_WriteShort(&sv.multicast, efnum);
		MSG_WriteCoord(&sv.multicast, org[0]);
		MSG_WriteCoord(&sv.multicast, org[1]);
		MSG_WriteCoord(&sv.multicast, org[2]);

		MSG_WriteByte(&sv.nqmulticast, svcdp_pointparticles1);
		MSG_WriteShort(&sv.nqmulticast, efnum);
		MSG_WriteCoord(&sv.nqmulticast, org[0]);
		MSG_WriteCoord(&sv.nqmulticast, org[1]);
		MSG_WriteCoord(&sv.nqmulticast, org[2]);
	}
	else
	{
		MSG_WriteByte(&sv.multicast, svcfte_pointparticles);
		MSG_WriteShort(&sv.multicast, efnum);
		MSG_WriteCoord(&sv.multicast, org[0]);
		MSG_WriteCoord(&sv.multicast, org[1]);
		MSG_WriteCoord(&sv.multicast, org[2]);
		MSG_WriteCoord(&sv.multicast, vel[0]);
		MSG_WriteCoord(&sv.multicast, vel[1]);
		MSG_WriteCoord(&sv.multicast, vel[2]);
		MSG_WriteShort(&sv.multicast, count);

		MSG_WriteByte(&sv.nqmulticast, svcdp_pointparticles);
		MSG_WriteShort(&sv.nqmulticast, efnum);
		MSG_WriteCoord(&sv.nqmulticast, org[0]);
		MSG_WriteCoord(&sv.nqmulticast, org[1]);
		MSG_WriteCoord(&sv.nqmulticast, org[2]);
		MSG_WriteCoord(&sv.nqmulticast, vel[0]);
		MSG_WriteCoord(&sv.nqmulticast, vel[1]);
		MSG_WriteCoord(&sv.nqmulticast, vel[2]);
		MSG_WriteShort(&sv.nqmulticast, count);
	}
	SV_MulticastProtExt(org, MULTICAST_PHS, ~0, PEXT_CSQC, 0);
#endif
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
		{	//not time yet, reform original list.
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
//DP_TE_QUADEFFECTS1
void PF_te_gunshotquad(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int count;
	if (*svprogfuncs->callargc >= 2)
		count = G_FLOAT(OFS_PARM1);
	else
		count = 1;
	SV_point_tempentity(G_VECTOR(OFS_PARM0), DPTE_GUNSHOTQUAD, count);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(vector org) te_spike = #419;
void PF_te_spike(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TE_SPIKE, 1);
}

//DP_TE_QUADEFFECTS1
void PF_te_spikequad(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), DPTE_SPIKEQUAD, 1);
}

// FTE_TE_STANDARDEFFECTBUILTINS
void PF_te_lightningblood(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TE_LIGHTNINGBLOOD, 1);
}

// FTE_TE_STANDARDEFFECTBUILTINS
void PF_te_bloodqw(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int count;
	if (*svprogfuncs->callargc >= 2)
		count = G_FLOAT(OFS_PARM1);
	else
		count = 1;
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TE_BLOOD, count);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(vector org) te_superspike = #420;
void PF_te_superspike(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TE_SUPERSPIKE, 1);
}
//DP_TE_QUADEFFECTS1
void PF_te_superspikequad(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), DPTE_SUPERSPIKEQUAD, 1);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(vector org) te_explosion = #421;
void PF_te_explosion(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TE_EXPLOSION, 1);
}
//DP_TE_QUADEFFECTS1
void PF_te_explosionquad(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), DPTE_EXPLOSIONQUAD, 1);
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

//DP_TE_SPARK
void PF_te_spark(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);

	if (G_FLOAT(OFS_PARM2) < 1)
		return;

	MSG_WriteByte(&sv.multicast, svc_temp_entity);
	MSG_WriteByte(&sv.multicast, DPTE_SPARK);
	// origin
	MSG_WriteCoord(&sv.multicast, org[0]);
	MSG_WriteCoord(&sv.multicast, org[1]);
	MSG_WriteCoord(&sv.multicast, org[2]);
	// velocity
	MSG_WriteByte(&sv.multicast, bound(-128, (int) G_VECTOR(OFS_PARM1)[0], 127));
	MSG_WriteByte(&sv.multicast, bound(-128, (int) G_VECTOR(OFS_PARM1)[1], 127));
	MSG_WriteByte(&sv.multicast, bound(-128, (int) G_VECTOR(OFS_PARM1)[2], 127));
	// count
	MSG_WriteByte(&sv.multicast, bound(0, (int) G_FLOAT(OFS_PARM2), 255));

//the nq version
#ifdef NQPROT
	MSG_WriteByte(&sv.nqmulticast, svc_temp_entity);
	MSG_WriteByte(&sv.nqmulticast, DPTE_SPARK);
	// origin
	MSG_WriteCoord(&sv.nqmulticast, org[0]);
	MSG_WriteCoord(&sv.nqmulticast, org[1]);
	MSG_WriteCoord(&sv.nqmulticast, org[2]);
	// velocity
	MSG_WriteByte(&sv.nqmulticast, bound(-128, (int) G_VECTOR(OFS_PARM1)[0], 127));
	MSG_WriteByte(&sv.nqmulticast, bound(-128, (int) G_VECTOR(OFS_PARM1)[1], 127));
	MSG_WriteByte(&sv.nqmulticast, bound(-128, (int) G_VECTOR(OFS_PARM1)[2], 127));
	// count
	MSG_WriteByte(&sv.nqmulticast, bound(0, (int) G_FLOAT(OFS_PARM2), 255));
#endif
	SV_Multicast(org, MULTICAST_PVS);
}

// #416 void(vector org) te_smallflash (DP_TE_SMALLFLASH)
void PF_te_smallflash(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);
	SV_point_tempentity(org, DPTE_SMALLFLASH, 0);
}

// #417 void(vector org, float radius, float lifetime, vector color) te_customflash (DP_TE_CUSTOMFLASH)
void PF_te_customflash(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);

	if (G_FLOAT(OFS_PARM1) < 8 || G_FLOAT(OFS_PARM2) < (1.0 / 256.0))
		return;
	MSG_WriteByte(&sv.multicast, svc_temp_entity);
	MSG_WriteByte(&sv.multicast, DPTE_CUSTOMFLASH);
	// origin
	MSG_WriteCoord(&sv.multicast, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteCoord(&sv.multicast, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteCoord(&sv.multicast, G_VECTOR(OFS_PARM0)[2]);
	// radius
	MSG_WriteByte(&sv.multicast, bound(0, G_FLOAT(OFS_PARM1) / 8 - 1, 255));
	// lifetime
	MSG_WriteByte(&sv.multicast, bound(0, G_FLOAT(OFS_PARM2) / 256 - 1, 255));
	// color
	MSG_WriteByte(&sv.multicast, bound(0, G_VECTOR(OFS_PARM3)[0] * 255, 255));
	MSG_WriteByte(&sv.multicast, bound(0, G_VECTOR(OFS_PARM3)[1] * 255, 255));
	MSG_WriteByte(&sv.multicast, bound(0, G_VECTOR(OFS_PARM3)[2] * 255, 255));

#ifdef NQPROT
	MSG_WriteByte(&sv.nqmulticast, svc_temp_entity);
	MSG_WriteByte(&sv.nqmulticast, DPTE_CUSTOMFLASH);
	// origin
	MSG_WriteCoord(&sv.nqmulticast, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteCoord(&sv.nqmulticast, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteCoord(&sv.nqmulticast, G_VECTOR(OFS_PARM0)[2]);
	// radius
	MSG_WriteByte(&sv.nqmulticast, bound(0, G_FLOAT(OFS_PARM1) / 8 - 1, 255));
	// lifetime
	MSG_WriteByte(&sv.nqmulticast, bound(0, G_FLOAT(OFS_PARM2) / 256 - 1, 255));
	// color
	MSG_WriteByte(&sv.nqmulticast, bound(0, G_VECTOR(OFS_PARM3)[0] * 255, 255));
	MSG_WriteByte(&sv.nqmulticast, bound(0, G_VECTOR(OFS_PARM3)[1] * 255, 255));
	MSG_WriteByte(&sv.nqmulticast, bound(0, G_VECTOR(OFS_PARM3)[2] * 255, 255));
#endif
	SV_Multicast(org, MULTICAST_PVS);
}

//#408 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color, float gravityflag, float randomveljitter) te_particlecube (DP_TE_PARTICLECUBE)
void PF_te_particlecube(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *min = G_VECTOR(OFS_PARM0);
	float *max = G_VECTOR(OFS_PARM1);
	float *dir = G_VECTOR(OFS_PARM2);
	float count = G_FLOAT(OFS_PARM3);
	float color = G_FLOAT(OFS_PARM4);
	float gravityflag = G_FLOAT(OFS_PARM5);
	float randomveljitter = G_FLOAT(OFS_PARM6);

	vec3_t org;

	if (count < 1)
		return;

	// [vector] min [vector] max [vector] dir [short] count [byte] color [byte] gravity [coord] randomvel

	MSG_WriteByte (&sv.multicast, svc_temp_entity);
	MSG_WriteByte (&sv.multicast, DPTE_PARTICLECUBE);
	MSG_WriteCoord(&sv.multicast, min[0]);
	MSG_WriteCoord(&sv.multicast, min[1]);
	MSG_WriteCoord(&sv.multicast, min[2]);
	MSG_WriteCoord(&sv.multicast, max[0]);
	MSG_WriteCoord(&sv.multicast, max[1]);
	MSG_WriteCoord(&sv.multicast, max[2]);
	MSG_WriteCoord(&sv.multicast, dir[0]);
	MSG_WriteCoord(&sv.multicast, dir[1]);
	MSG_WriteCoord(&sv.multicast, dir[2]);
	MSG_WriteShort(&sv.multicast, bound(0, count, 65535));
	MSG_WriteByte (&sv.multicast, bound(0, color, 255));
	MSG_WriteByte (&sv.multicast, (int)gravityflag!=0);
	MSG_WriteCoord(&sv.multicast, randomveljitter);

#ifdef NQPROT
	MSG_WriteByte (&sv.nqmulticast, svc_temp_entity);
	MSG_WriteByte (&sv.nqmulticast, DPTE_PARTICLECUBE);
	MSG_WriteCoord(&sv.nqmulticast, min[0]);
	MSG_WriteCoord(&sv.nqmulticast, min[1]);
	MSG_WriteCoord(&sv.nqmulticast, min[2]);
	MSG_WriteCoord(&sv.nqmulticast, max[0]);
	MSG_WriteCoord(&sv.nqmulticast, max[1]);
	MSG_WriteCoord(&sv.nqmulticast, max[2]);
	MSG_WriteCoord(&sv.nqmulticast, dir[0]);
	MSG_WriteCoord(&sv.nqmulticast, dir[1]);
	MSG_WriteCoord(&sv.nqmulticast, dir[2]);
	MSG_WriteShort(&sv.nqmulticast, bound(0, count, 65535));
	MSG_WriteByte (&sv.nqmulticast, bound(0, color, 255));
	MSG_WriteByte (&sv.nqmulticast, (int)gravityflag!=0);
	MSG_WriteCoord(&sv.nqmulticast, randomveljitter);
#endif

	VectorAdd(min, max, org);
	VectorScale(org, 0.5, org);
	SV_Multicast(org, MULTICAST_PVS);
}

void PF_te_explosionrgb(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);
	float *colour = G_VECTOR(OFS_PARM0);

	MSG_WriteByte(&sv.multicast, svc_temp_entity);
	MSG_WriteByte(&sv.multicast, DPTE_EXPLOSIONRGB);
	// origin
	MSG_WriteCoord(&sv.multicast, org[0]);
	MSG_WriteCoord(&sv.multicast, org[1]);
	MSG_WriteCoord(&sv.multicast, org[2]);
	// color
	MSG_WriteByte(&sv.multicast, bound(0, (int) (colour[0] * 255), 255));
	MSG_WriteByte(&sv.multicast, bound(0, (int) (colour[1] * 255), 255));
	MSG_WriteByte(&sv.multicast, bound(0, (int) (colour[2] * 255), 255));
#ifdef NQPROT
	MSG_WriteByte(&sv.nqmulticast, svc_temp_entity);
	MSG_WriteByte(&sv.nqmulticast, DPTE_EXPLOSIONRGB);
	// origin
	MSG_WriteCoord(&sv.nqmulticast, org[0]);
	MSG_WriteCoord(&sv.nqmulticast, org[1]);
	MSG_WriteCoord(&sv.nqmulticast, org[2]);
	// color
	MSG_WriteByte(&sv.nqmulticast, bound(0, (int) (colour[0] * 255), 255));
	MSG_WriteByte(&sv.nqmulticast, bound(0, (int) (colour[1] * 255), 255));
	MSG_WriteByte(&sv.nqmulticast, bound(0, (int) (colour[2] * 255), 255));
#endif
	SV_Multicast(org, MULTICAST_PVS);
}

void PF_te_particlerain(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *min = G_VECTOR(OFS_PARM0);
	float *max = G_VECTOR(OFS_PARM1);
	float *velocity = G_VECTOR(OFS_PARM2);
	float count = G_FLOAT(OFS_PARM3);
	float colour = G_FLOAT(OFS_PARM4);

	if (count < 1)
		return;

	MSG_WriteByte(&sv.multicast, svc_temp_entity);
	MSG_WriteByte(&sv.multicast, DPTE_PARTICLERAIN);
	// min
	MSG_WriteCoord(&sv.multicast, min[0]);
	MSG_WriteCoord(&sv.multicast, min[1]);
	MSG_WriteCoord(&sv.multicast, min[2]);
	// max
	MSG_WriteCoord(&sv.multicast, max[0]);
	MSG_WriteCoord(&sv.multicast, max[1]);
	MSG_WriteCoord(&sv.multicast, max[2]);
	// velocity
	MSG_WriteCoord(&sv.multicast, velocity[0]);
	MSG_WriteCoord(&sv.multicast, velocity[1]);
	MSG_WriteCoord(&sv.multicast, velocity[2]);
	// count
	MSG_WriteShort(&sv.multicast, max(count, 65535));
	// colour
	MSG_WriteByte(&sv.multicast, colour);

#ifdef NQPROT
	MSG_WriteByte(&sv.nqmulticast, svc_temp_entity);
	MSG_WriteByte(&sv.nqmulticast, DPTE_PARTICLERAIN);
	// min
	MSG_WriteCoord(&sv.nqmulticast, min[0]);
	MSG_WriteCoord(&sv.nqmulticast, min[1]);
	MSG_WriteCoord(&sv.nqmulticast, min[2]);
	// max
	MSG_WriteCoord(&sv.nqmulticast, max[0]);
	MSG_WriteCoord(&sv.nqmulticast, max[1]);
	MSG_WriteCoord(&sv.nqmulticast, max[2]);
	// velocity
	MSG_WriteCoord(&sv.nqmulticast, velocity[0]);
	MSG_WriteCoord(&sv.nqmulticast, velocity[1]);
	MSG_WriteCoord(&sv.nqmulticast, velocity[2]);
	// count
	MSG_WriteShort(&sv.nqmulticast, max(count, 65535));
	// colour
	MSG_WriteByte(&sv.nqmulticast, colour);
#endif

	SV_Multicast(NULL, MULTICAST_ALL);
}

void PF_te_particlesnow(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *min = G_VECTOR(OFS_PARM0);
	float *max = G_VECTOR(OFS_PARM1);
	float *velocity = G_VECTOR(OFS_PARM2);
	float count = G_FLOAT(OFS_PARM3);
	float colour = G_FLOAT(OFS_PARM4);

	if (count < 1)
		return;

	MSG_WriteByte(&sv.multicast, svc_temp_entity);
	MSG_WriteByte(&sv.multicast, DPTE_PARTICLESNOW);
	// min
	MSG_WriteCoord(&sv.multicast, min[0]);
	MSG_WriteCoord(&sv.multicast, min[1]);
	MSG_WriteCoord(&sv.multicast, min[2]);
	// max
	MSG_WriteCoord(&sv.multicast, max[0]);
	MSG_WriteCoord(&sv.multicast, max[1]);
	MSG_WriteCoord(&sv.multicast, max[2]);
	// velocity
	MSG_WriteCoord(&sv.multicast, velocity[0]);
	MSG_WriteCoord(&sv.multicast, velocity[1]);
	MSG_WriteCoord(&sv.multicast, velocity[2]);
	// count
	MSG_WriteShort(&sv.multicast, max(count, 65535));
	// colour
	MSG_WriteByte(&sv.multicast, colour);

#ifdef NQPROT
	MSG_WriteByte(&sv.nqmulticast, svc_temp_entity);
	MSG_WriteByte(&sv.nqmulticast, DPTE_PARTICLESNOW);
	// min
	MSG_WriteCoord(&sv.nqmulticast, min[0]);
	MSG_WriteCoord(&sv.nqmulticast, min[1]);
	MSG_WriteCoord(&sv.nqmulticast, min[2]);
	// max
	MSG_WriteCoord(&sv.nqmulticast, max[0]);
	MSG_WriteCoord(&sv.nqmulticast, max[1]);
	MSG_WriteCoord(&sv.nqmulticast, max[2]);
	// velocity
	MSG_WriteCoord(&sv.nqmulticast, velocity[0]);
	MSG_WriteCoord(&sv.nqmulticast, velocity[1]);
	MSG_WriteCoord(&sv.nqmulticast, velocity[2]);
	// count
	MSG_WriteShort(&sv.nqmulticast, max(count, 65535));
	// colour
	MSG_WriteByte(&sv.nqmulticast, colour);
#endif

	SV_Multicast(NULL, MULTICAST_ALL);
}

// #406 void(vector mincorner, vector maxcorner, float explosionspeed, float howmany) te_bloodshower (DP_TE_BLOODSHOWER)
void PF_te_bloodshower(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	// [vector] min [vector] max [coord] explosionspeed [short] count
	float *min = G_VECTOR(OFS_PARM0);
	float *max = G_VECTOR(OFS_PARM1);
	float speed = G_FLOAT(OFS_PARM2);
	float count = G_FLOAT(OFS_PARM3);
	vec3_t org;

	MSG_WriteByte(&sv.multicast, svc_temp_entity);
	MSG_WriteByte(&sv.multicast, DPTE_BLOODSHOWER);
	// min
	MSG_WriteCoord(&sv.multicast, min[0]);
	MSG_WriteCoord(&sv.multicast, min[1]);
	MSG_WriteCoord(&sv.multicast, min[2]);
	// max
	MSG_WriteCoord(&sv.multicast, max[0]);
	MSG_WriteCoord(&sv.multicast, max[1]);
	MSG_WriteCoord(&sv.multicast, max[2]);
	// speed
	MSG_WriteCoord(&sv.multicast, speed);
	// count
	MSG_WriteShort(&sv.multicast, bound(0, count, 65535));

#ifdef NQPROT
	MSG_WriteByte(&sv.nqmulticast, svc_temp_entity);
	MSG_WriteByte(&sv.nqmulticast, DPTE_BLOODSHOWER);
	// min
	MSG_WriteCoord(&sv.nqmulticast, min[0]);
	MSG_WriteCoord(&sv.nqmulticast, min[1]);
	MSG_WriteCoord(&sv.nqmulticast, min[2]);
	// max
	MSG_WriteCoord(&sv.nqmulticast, max[0]);
	MSG_WriteCoord(&sv.nqmulticast, max[1]);
	MSG_WriteCoord(&sv.nqmulticast, max[2]);
	// speed
	MSG_WriteCoord(&sv.nqmulticast, speed);
	// count
	MSG_WriteShort(&sv.nqmulticast, bound(0, count, 65535));
#endif

	VectorAdd(min, max, org);
	VectorScale(org, 0.5, org);
	SV_Multicast(org, MULTICAST_PVS);	//fixme: should this be phs instead?
}

//DP_SV_EFFECT
//void(vector org, string modelname, float startframe, float endframe, float framerate) effect = #404;
static void PF_effect(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);
	char *name = PR_GetStringOfs(prinst, OFS_PARM1);
	float startframe = G_FLOAT(OFS_PARM2);
	float endframe = G_FLOAT(OFS_PARM3);
	float framerate = G_FLOAT(OFS_PARM4);
	int index = SV_ModelIndex(name);

	if (startframe>255 || index>255)
	{
		MSG_WriteByte (&sv.multicast, svcfte_effect2);
		MSG_WriteCoord (&sv.multicast, org[0]);
		MSG_WriteCoord (&sv.multicast, org[1]);
		MSG_WriteCoord (&sv.multicast, org[2]);
		MSG_WriteShort (&sv.multicast, index);
		MSG_WriteShort (&sv.multicast, startframe);
		MSG_WriteByte (&sv.multicast, endframe);
		MSG_WriteByte (&sv.multicast, framerate);

#ifdef NQPROT
		MSG_WriteByte (&sv.nqmulticast, svcnq_effect2);
		MSG_WriteCoord (&sv.nqmulticast, org[0]);
		MSG_WriteCoord (&sv.nqmulticast, org[1]);
		MSG_WriteCoord (&sv.nqmulticast, org[2]);
		MSG_WriteShort (&sv.nqmulticast, index);
		MSG_WriteShort (&sv.nqmulticast, startframe);
		MSG_WriteByte (&sv.nqmulticast, endframe);
		MSG_WriteByte (&sv.nqmulticast, framerate);
#endif
	}
	else
	{
		MSG_WriteByte (&sv.multicast, svcfte_effect);
		MSG_WriteCoord (&sv.multicast, org[0]);
		MSG_WriteCoord (&sv.multicast, org[1]);
		MSG_WriteCoord (&sv.multicast, org[2]);
		MSG_WriteByte (&sv.multicast, index);
		MSG_WriteByte (&sv.multicast, startframe);
		MSG_WriteByte (&sv.multicast, endframe);
		MSG_WriteByte (&sv.multicast, framerate);

#ifdef NQPROT
		MSG_WriteByte (&sv.nqmulticast, svcnq_effect);
		MSG_WriteCoord (&sv.nqmulticast, org[0]);
		MSG_WriteCoord (&sv.nqmulticast, org[1]);
		MSG_WriteCoord (&sv.nqmulticast, org[2]);
		MSG_WriteByte (&sv.nqmulticast, index);
		MSG_WriteByte (&sv.nqmulticast, startframe);
		MSG_WriteByte (&sv.nqmulticast, endframe);
		MSG_WriteByte (&sv.nqmulticast, framerate);
#endif
	}

	SV_Multicast(org, MULTICAST_PVS);
}

//DP_TE_PLASMABURN
//void(vector org) te_plasmaburn = #433;
void PF_te_plasmaburn(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);
	SV_point_tempentity(org, 75, 0);
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
		Info_SetValueForStarKey(svs.clients[e1-1].userinfo, key, value, sizeof(svs.clients[e1-1].userinfo));


		SV_ExtractFromUserinfo (&svs.clients[e1-1]);

		MSG_WriteByte (&sv.reliable_datagram, svc_setinfo);
		MSG_WriteByte (&sv.reliable_datagram, e1-1);
		MSG_WriteString (&sv.reliable_datagram, key);
		MSG_WriteString (&sv.reliable_datagram, Info_ValueForKey(svs.clients[e1-1].userinfo, key));

		if (!strcmp(key, "*spectator"))
			svs.clients[e1-1].spectator = !!atoi(value);

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
	client->edict->v->team = (i & 15) + 1;
#ifdef NQPROT
	MSG_WriteByte (&sv.nqreliable_datagram, svc_updatecolors);
	MSG_WriteByte (&sv.nqreliable_datagram, entnum - 1);
	MSG_WriteByte (&sv.nqreliable_datagram, i);
#endif
	sprintf(number, "%i", i>>4);
	if (!strcmp(number, Info_ValueForKey(client->userinfo, "topcolor")))
	{
		Info_SetValueForKey(client->userinfo, "topcolor", number, sizeof(client->userinfo));
		MSG_WriteByte (&sv.reliable_datagram, svc_setinfo);
		MSG_WriteByte (&sv.reliable_datagram, entnum-1);
		MSG_WriteString (&sv.reliable_datagram, "topcolor");
		MSG_WriteString (&sv.reliable_datagram, number);
	}

	sprintf(number, "%i", i&15);
	if (!strcmp(number, Info_ValueForKey(client->userinfo, "bottomcolor")))
	{
		Info_SetValueForKey(client->userinfo, "bottomcolor", number, sizeof(client->userinfo));
		MSG_WriteByte (&sv.reliable_datagram, svc_setinfo);
		MSG_WriteByte (&sv.reliable_datagram, entnum-1);
		MSG_WriteString (&sv.reliable_datagram, "bottomcolor");
		MSG_WriteString (&sv.reliable_datagram, number);
	}
	SV_ExtractFromUserinfo (client);
}


static client_t *DirectSplit(client_t *cl, int svc, int svclen)
{
	if (cl->controller)
	{	//this is a slave client.
		//find the right number and send.
		client_t *sp;
		int pnum = 0;
		for (sp = cl->controller; sp; sp = sp->controlled)
		{
			if (sp == cl)
				break;
			pnum++;
		}
		sp = cl->controller;

		ClientReliableWrite_Begin (sp, svcfte_choosesplitclient, 2+svclen);
		ClientReliableWrite_Byte (sp, pnum);
		ClientReliableWrite_Byte (sp, svc);
		return sp;
	}
	else
	{
		ClientReliableWrite_Begin (cl, svc, svclen);
		return cl;
	}
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
	char *slot	= PR_GetStringOfs(prinst, OFS_PARM0);
	char *picname = PR_GetStringOfs(prinst, OFS_PARM1);
	float x		= G_FLOAT(OFS_PARM2);
	float y		= G_FLOAT(OFS_PARM3);
	float zone	= G_FLOAT(OFS_PARM4);
	int entnum;

	client_t *cl;

	ParamNegateFix( &x, &y, zone );

	if (*prinst->callargc==6)
	{	//to a single client
		entnum = G_EDICTNUM(prinst, OFS_PARM5)-1;
		if (entnum < 0 || entnum >= sv.allocated_client_slots)
			PR_RunError (prinst, "WriteDest: not a client");

		if (!(svs.clients[entnum].fteprotocolextensions & PEXT_SHOWPIC))
			return;	//need an extension for this. duh.

		cl = DirectSplit(&svs.clients[entnum], svcfte_showpic, 8 + strlen(slot)+strlen(picname));
		ClientReliableWrite_Byte(cl, zone);
		ClientReliableWrite_String(cl, slot);
		ClientReliableWrite_String(cl, picname);
		ClientReliableWrite_Short(cl, x);
		ClientReliableWrite_Short(cl, y);
	}
	else
	{
		*prinst->callargc = 6;
		for (entnum = 0; entnum < sv.allocated_client_slots; entnum++)
		{
			G_INT(OFS_PARM5) = EDICT_TO_PROG(prinst, EDICT_NUM(prinst, entnum+1));
			PF_ShowPic(prinst, pr_globals);
		}
	}
};

void PF_HidePic(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	client_t *cl;
	char *slot	= PR_GetStringOfs(prinst, OFS_PARM0);
	int entnum;

	if (*prinst->callargc==2)
	{	//to a single client
		entnum = G_EDICTNUM(prinst, OFS_PARM1)-1;
		if (entnum < 0 || entnum >= sv.allocated_client_slots)
			PR_RunError (prinst, "WriteDest: not a client");

		if (!(svs.clients[entnum].fteprotocolextensions & PEXT_SHOWPIC))
			return;	//need an extension for this. duh.

		cl = DirectSplit(&svs.clients[entnum], svcfte_hidepic, 2 + strlen(slot));
		ClientReliableWrite_String(cl, slot);
	}
	else
	{
		*prinst->callargc = 2;
		for (entnum = 0; entnum < sv.allocated_client_slots; entnum++)
		{
			G_INT(OFS_PARM1) = EDICT_TO_PROG(prinst, EDICT_NUM(prinst, entnum+1));
			PF_HidePic(prinst, pr_globals);
		}
	}
};


void PF_MovePic(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *slot	= PR_GetStringOfs(prinst, OFS_PARM0);
	float x		= G_FLOAT(OFS_PARM1);
	float y		= G_FLOAT(OFS_PARM2);
	float zone	= G_FLOAT(OFS_PARM3);
	int entnum;
	client_t *cl;

	ParamNegateFix( &x, &y, zone );

	if (*prinst->callargc==5)
	{	//to a single client
		entnum = G_EDICTNUM(prinst, OFS_PARM4)-1;
		if (entnum < 0 || entnum >= sv.allocated_client_slots)
			PR_RunError (prinst, "WriteDest: not a client");

		if (!(svs.clients[entnum].fteprotocolextensions & PEXT_SHOWPIC))
			return;	//need an extension for this. duh.

		cl = DirectSplit(&svs.clients[entnum], svcfte_movepic, 6 + strlen(slot));
		ClientReliableWrite_String(cl, slot);
		ClientReliableWrite_Byte(cl, zone);
		ClientReliableWrite_Short(cl, x);
		ClientReliableWrite_Short(cl, y);
	}
	else
	{
		*prinst->callargc = 5;
		for (entnum = 0; entnum < sv.allocated_client_slots; entnum++)
		{
			G_INT(OFS_PARM4) = EDICT_TO_PROG(prinst, EDICT_NUM(prinst, entnum+1));
			PF_MovePic(prinst, pr_globals);
		}
	}
};

void PF_ChangePic(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *slot	= PR_GetStringOfs(prinst, OFS_PARM0);
	char *newpic= PR_GetStringOfs(prinst, OFS_PARM1);
	int entnum;
	client_t *cl;

	if (*prinst->callargc==3)
	{	//to a single client
		entnum = G_EDICTNUM(prinst, OFS_PARM2)-1;
		if (entnum < 0 || entnum >= sv.allocated_client_slots)
			PR_RunError (prinst, "WriteDest: not a client");

		if (!(svs.clients[entnum].fteprotocolextensions & PEXT_SHOWPIC))
			return;	//need an extension for this. duh.

		cl = DirectSplit(&svs.clients[entnum], svcfte_updatepic, 3 + strlen(slot)+strlen(newpic));
		ClientReliableWrite_String(cl, slot);
		ClientReliableWrite_String(cl, newpic);
	}
	else
	{
		*prinst->callargc = 3;
		for (entnum = 0; entnum < sv.allocated_client_slots; entnum++)
		{
			G_INT(OFS_PARM2) = EDICT_TO_PROG(prinst, EDICT_NUM(prinst, entnum+1));
			PF_ChangePic(prinst, pr_globals);
		}
	}
}





#if 0
typedef struct {
	int			ident;
	int			version;

	char		name[MAX_QPATH];

	int			flags;	//Does anyone know what these are?

	int			numFrames;
	int			numTags;
	int			numSurfaces;

	int			numSkins;

	int			ofsFrames;
	int			ofsTags;
	int			ofsSurfaces;
	int			ofsEnd;
} md3Header_t;
typedef struct {
	char name[MAX_QPATH];
	vec3_t org;
	float ang[3][3];
} md3tag_t;

typedef struct zymlump_s
{
	int start;
	int length;
} zymlump_t;
typedef struct zymtype1header_s
{
	char id[12]; // "ZYMOTICMODEL", length 12, no termination
	int type; // 0 (vertex morph) 1 (skeletal pose) or 2 (skeletal scripted)
	int filesize; // size of entire model file
	float mins[3], maxs[3], radius; // for clipping uses
	int numverts;
	int numtris;
	int numsurfaces;
	int numbones; // this may be zero in the vertex morph format (undecided)
	int numscenes; // 0 in skeletal scripted models

// skeletal pose header
	// lump offsets are relative to the file
	zymlump_t lump_scenes; // zymscene_t scene[numscenes]; // name and other information for each scene (see zymscene struct)
	zymlump_t lump_poses; // float pose[numposes][numbones][6]; // animation data
	zymlump_t lump_bones; // zymbone_t bone[numbones];
	zymlump_t lump_vertbonecounts; // int vertbonecounts[numvertices]; // how many bones influence each vertex (separate mainly to make this compress better)
	zymlump_t lump_verts; // zymvertex_t vert[numvertices]; // see vertex struct
	zymlump_t lump_texcoords; // float texcoords[numvertices][2];
	zymlump_t lump_render; // int renderlist[rendersize]; // sorted by shader with run lengths (int count), shaders are sequentially used, each run can be used with glDrawElements (each triangle is 3 int indices)
	zymlump_t lump_surfnames; // char shadername[numsurfaces][32]; // shaders used on this model
	zymlump_t lump_trizone; // byte trizone[numtris]; // see trizone explanation
} zymtype1header_t;
typedef struct zymbone_s
{
	char name[32];
	int flags;
	int parent; // parent bone number
} zymbone_t;
#endif
int SV_TagForName(int modelindex, char *tagname)
{
#if 1
	model_t *model = sv.models[modelindex];
	if (!model)
		model = Mod_ForName(sv.strings.model_precache[modelindex], false);
	if (!model)
		return 0;

	return Mod_TagNumForName(model, tagname);
#else
	int i;
	unsigned int *file;

	file = (void*)COM_LoadTempFile(sv.model_precache[modelindex]);
	if (!file)
	{
		Con_Printf("setattachment: \"%s\" is missing\n", sv.model_precache[modelindex]);
		return 0;
	}

	if (*file == MD3_IDENT)
	{
		md3Header_t *md3 = (md3Header_t*)file;
		md3tag_t *tag;

		tag = (md3tag_t*)((char*)md3 + md3->ofsTags);

		for (i = 0;i < md3->numTags;i++)
		{
			if (!strcmp(tagname, tag[i].name))
			{
				return i + 1;
			}
		}
	}
	else if (!strncmp((char*)file, "ZYMOTICMODEL", 12) && BigLong(file[3]) == 1)
	{
		zymtype1header_t *zym = (zymtype1header_t*)file;
		zymbone_t *tag;

		tag = (zymbone_t*)((char*)zym + BigLong(zym->lump_bones.start));

		for (i = BigLong(zym->numbones)-1;i >=0;i--)
		{
			if (!strcmp(tagname, tag[i].name))
			{
				return i + 1;
			}
		}
	}
	else
		Con_DPrintf("setattachment: %s not supported\n", sv.model_precache[modelindex]);
	return 0;
#endif
}

void PF_setattachment(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t *e = G_EDICT(prinst, OFS_PARM0);
	edict_t *tagentity = G_EDICT(prinst, OFS_PARM1);
	char *tagname = PR_GetStringOfs(prinst, OFS_PARM2);

	int modelindex;

	int tagidx;

	tagidx = 0;

	if (tagentity != sv.edicts && tagname && tagname[0])
	{
		modelindex = (int)tagentity->v->modelindex;
		if (modelindex > 0 && modelindex < MAX_MODELS)
		{
			if (!sv.models[modelindex])
				sv.models[modelindex] = Mod_ForName(sv.strings.model_precache[modelindex], false);
			if (sv.models[modelindex])
			{
				tagidx = SV_TagForName(modelindex, tagname);
				if (tagidx == 0)
					Con_DPrintf("setattachment(edict %i, edict %i, string \"%s\"): tried to find tag named \"%s\" on entity %i (model \"%s\") but could not find it\n", NUM_FOR_EDICT(prinst, e), NUM_FOR_EDICT(prinst, tagentity), tagname, tagname, NUM_FOR_EDICT(prinst, tagentity), sv.models[modelindex]->name);
			}
			else
				Con_DPrintf("setattachment(edict %i, edict %i, string \"%s\"): Couldn't load model %s\n", NUM_FOR_EDICT(prinst, e), NUM_FOR_EDICT(prinst, tagentity), tagname, sv.modelname[modelindex]);
		}
		else
			Con_DPrintf("setattachment(edict %i, edict %i, string \"%s\"): tried to find tag named \"%s\" on entity %i but it has no model\n", NUM_FOR_EDICT(prinst, e), NUM_FOR_EDICT(prinst, tagentity), tagname, tagname, NUM_FOR_EDICT(prinst, tagentity));

	}

	e->xv->tag_entity = EDICT_TO_PROG(prinst,tagentity);
	e->xv->tag_index = tagidx;
}

// #451 float(entity ent, string tagname) gettagindex (DP_MD3_TAGSINFO)
void PF_gettagindex(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t *e = G_EDICT(prinst, OFS_PARM0);
	char *tagname = PR_GetStringOfs(prinst, OFS_PARM1);

	int modelindex;

	int tagidx;

	tagidx = 0;

	if (tagname && tagname[0])
	{
		modelindex = (int)e->v->modelindex;
		if (modelindex > 0 && modelindex < MAX_MODELS && sv.strings.model_precache[modelindex])
		{
			tagidx = SV_TagForName(modelindex, tagname);
			if (tagidx == 0)
				Con_DPrintf("PF_gettagindex(edict %i, edict %i, string \"%s\"): tried to find tag named \"%s\" on entity %i (model \"%s\") but could not find it\n", NUM_FOR_EDICT(prinst, e), NUM_FOR_EDICT(prinst, e), tagname, tagname, NUM_FOR_EDICT(prinst, e), sv.models[modelindex]->name);
		}
		else
			Con_DPrintf("PF_gettagindex(edict %i, edict %i, string \"%s\"): tried to find tag named \"%s\" on entity %i but it has no model\n", NUM_FOR_EDICT(prinst, e), NUM_FOR_EDICT(prinst, e), tagname, tagname, NUM_FOR_EDICT(prinst, e));

	}

	G_FLOAT(OFS_RETURN) = tagidx;
}

static void EdictToTransform(edict_t *ed, float *trans)
{
	AngleVectors(ed->v->angles, trans+0, trans+4, trans+8);
	VectorInverse(trans+4);

	trans[3] = ed->v->origin[0];
	trans[7] = ed->v->origin[1];
	trans[11] = ed->v->origin[2];
}

// #452 vector(entity ent, float tagindex) gettaginfo (DP_MD3_TAGSINFO)
void PF_sv_gettaginfo(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	framestate_t fstate;
	float transtag[12];
	float transent[12];
	float result[12];
	edict_t *ent = G_EDICT(prinst, OFS_PARM0);
	int tagnum = G_FLOAT(OFS_PARM1);
	model_t *model = sv.models[(int)ent->v->modelindex];

	float *origin = G_VECTOR(OFS_RETURN);
	float *axis[3];
	axis[0] = P_VEC(v_forward);
	axis[1] = P_VEC(v_up);
	axis[2] = P_VEC(v_right);

	if (!model)
		model = Mod_FindName(sv.strings.model_precache[(int)ent->v->modelindex]);

	memset(&fstate, 0, sizeof(fstate));
	fstate.g[FS_REG].frame[0] = fstate.g[FS_REG].frame[0] = ent->v->frame;

	if (!Mod_GetTag(model, tagnum, &fstate, transtag))
	{
		return;
	}

#pragma message("This function doesn't honour attachments")
	EdictToTransform(ent, transent);
	R_ConcatTransforms((void*)transent, (void*)transtag, (void*)result);

	origin[0] = result[3];
	origin[1] = result[7];
	origin[2] = result[11];
	VectorCopy((result+0), axis[0]);
	VectorCopy((result+4), axis[1]);
	VectorCopy((result+8), axis[2]);
}

//the first implementation of this function was (float type, float num, string name)
//it is now float num, float type, .field
//EXT_CSQC_1
void PF_clientstat(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
#if 0 //this is the old code
	char *name = PF_VarString(prinst, 2, pr_globals);
	SV_QCStatName(G_FLOAT(OFS_PARM0), name, G_FLOAT(OFS_PARM1));
#else
	SV_QCStatFieldIdx(G_FLOAT(OFS_PARM1), G_INT(OFS_PARM2)+prinst->fieldadjust, G_FLOAT(OFS_PARM0));
#endif
}
//EXT_CSQC_1
//void(float num, float type, string name) globalstat
void PF_globalstat(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *name = PF_VarString(prinst, 2, pr_globals);
#if 0 //this is the old code
	SV_QCStatName(G_FLOAT(OFS_PARM0), name, G_FLOAT(OFS_PARM1));
#else
	SV_QCStatGlobal(G_FLOAT(OFS_PARM1), name, G_FLOAT(OFS_PARM0));
#endif
}

//EXT_CSQC_1
void PF_runclientphys(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{	
	unsigned int i, n;
	extern vec3_t player_maxs, player_mins;
	extern qbyte playertouch[];
	unsigned int msecs;
	edict_t *ent = G_EDICT(prinst, OFS_PARM0);
	edict_t *touched;
	if (pr_nqglobal_struct->clientcommandframe)
		pmove.sequence = *pr_nqglobal_struct->clientcommandframe;
	else
		pmove.sequence = 0;
	if (host_client && host_client->edict == ent)
		pmove.pm_type = SV_PMTypeForClient(host_client);
	else
		pmove.pm_type = PM_NORMAL;

	pmove.jump_msec = 0;//(cls.z_ext & Z_EXT_PM_TYPE) ? 0 : from->jump_msec;

	pmove.jump_held = ((int)ent->xv->pmove_flags)&PMF_JUMP_HELD;
	pmove.waterjumptime = ent->v->teleport_time;

//set up the movement command
	msecs = pr_global_struct->input_timelength*1000 + 0.5f;
	//precision inaccuracies. :(
#define ANGLE2SHORT(x) (x) * (65536/360.0)
	pmove.cmd.angles[0] = ANGLE2SHORT((pr_global_struct->input_angles)[0]);
	pmove.cmd.angles[1] = ANGLE2SHORT((pr_global_struct->input_angles)[1]);
	pmove.cmd.angles[2] = ANGLE2SHORT((pr_global_struct->input_angles)[2]);
	VectorCopy(pr_global_struct->input_angles, pmove.angles);

	pmove.cmd.forwardmove = (pr_global_struct->input_movevalues)[0];
	pmove.cmd.sidemove = (pr_global_struct->input_movevalues)[1];
	pmove.cmd.upmove = (pr_global_struct->input_movevalues)[2];
	pmove.cmd.buttons = pr_global_struct->input_buttons;

	VectorCopy(ent->v->origin, pmove.origin);
	VectorCopy(ent->v->velocity, pmove.velocity);
	VectorCopy(ent->v->maxs, player_maxs);
	VectorCopy(ent->v->mins, player_mins);
	pmove.hullnum = SV_HullNumForPlayer(ent->xv->hull, ent->v->mins, ent->v->maxs);

	pmove.numphysent = 1;
	pmove.physents[0].model = sv.worldmodel;

	for (i=0 ; i<3 ; i++)
	{
		extern vec3_t	pmove_mins, pmove_maxs;
		pmove_mins[i] = pmove.origin[i] - 256;
		pmove_maxs[i] = pmove.origin[i] + 256;
	}
	AddLinksToPmove(ent, sv_areanodes);
//	AddAllEntsToPmove();


	while(msecs)	//break up longer commands
	{
		pmove.cmd.msec = msecs;
		if (pmove.cmd.msec > 50)
			pmove.cmd.msec = 50;
		msecs -= pmove.cmd.msec;
		PM_PlayerMove(1);
	}

	ent->xv->pmove_flags = 0;
	ent->xv->pmove_flags += ((int)pmove.jump_held?PMF_JUMP_HELD:0);
	ent->xv->pmove_flags += ((int)pmove.onladder?PMF_LADDER:0);
	ent->v->teleport_time = pmove.waterjumptime;
	VectorCopy(pmove.origin, ent->v->origin);
	VectorCopy(pmove.velocity, ent->v->velocity);



	ent->v->waterlevel = pmove.waterlevel;

	if (pmove.watertype & FTECONTENTS_SOLID)
		ent->v->watertype = Q1CONTENTS_SOLID;
	else if (pmove.watertype & FTECONTENTS_SKY)
		ent->v->watertype = Q1CONTENTS_SKY;
	else if (pmove.watertype & FTECONTENTS_LAVA)
		ent->v->watertype = Q1CONTENTS_LAVA;
	else if (pmove.watertype & FTECONTENTS_SLIME)
		ent->v->watertype = Q1CONTENTS_SLIME;
	else if (pmove.watertype & FTECONTENTS_WATER)
		ent->v->watertype = Q1CONTENTS_WATER;
	else
		ent->v->watertype = Q1CONTENTS_EMPTY;

	if (pmove.onground)
	{
		ent->v->flags = (int)sv_player->v->flags | FL_ONGROUND;
		ent->v->groundentity = EDICT_TO_PROG(svprogfuncs, EDICT_NUM(svprogfuncs, pmove.physents[pmove.groundent].info));
	}
	else
		ent->v->flags = (int)ent->v->flags & ~FL_ONGROUND;


	SV_LinkEdict(ent, true);
	for (i=0 ; i<pmove.numtouch ; i++)
	{
		if (pmove.physents[pmove.touchindex[i]].notouch)
			continue;
		n = pmove.physents[pmove.touchindex[i]].info;
		touched = EDICT_NUM(svprogfuncs, n);
		if (!ent->v->touch || (playertouch[n/8]&(1<<(n%8))))
			continue;

		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, touched);
		pr_global_struct->other = EDICT_TO_PROG(svprogfuncs, ent);
		pr_global_struct->time = sv.time;
#ifdef VM_Q1
		if (svs.gametype == GT_Q1QVM)
			Q1QVM_Touch();
		else
#endif
			PR_ExecuteProgram (svprogfuncs, touched->v->touch);
		playertouch[n/8] |= 1 << (n%8);
	}
}


//EXT_CSQC_1 (called when a movement command is received. runs full acceleration + movement)
qboolean SV_RunFullQCMovement(client_t *client, usercmd_t *ucmd)
{
	if (gfuncs.RunClientCommand)
	{
#ifdef SVCHAT
		if (SV_ChatMove(ucmd->impulse))
		{
			ucmd->buttons = 0;
			ucmd->impulse = 0;
			ucmd->forwardmove = ucmd->sidemove = ucmd->upmove = 0;
		}
#endif

		if (!sv_player->v->fixangle)
		{
			sv_player->v->v_angle[0] = SHORT2ANGLE(ucmd->angles[0]);
			sv_player->v->v_angle[1] = SHORT2ANGLE(ucmd->angles[1]);
			sv_player->v->v_angle[2] = SHORT2ANGLE(ucmd->angles[2]);
		}

		if (progstype == PROG_H2)
			sv_player->xv->light_level = 128;	//hmm... HACK!!!

		sv_player->v->button0 = ucmd->buttons & 1;
		sv_player->v->button2 = (ucmd->buttons >> 1) & 1;
	// DP_INPUTBUTTONS
		sv_player->xv->button3 = ((ucmd->buttons >> 2) & 1);
		sv_player->xv->button4 = ((ucmd->buttons >> 3) & 1);
		sv_player->xv->button5 = ((ucmd->buttons >> 4) & 1);
		sv_player->xv->button6 = ((ucmd->buttons >> 5) & 1);
		sv_player->xv->button7 = ((ucmd->buttons >> 6) & 1);
		sv_player->xv->button8 = ((ucmd->buttons >> 7) & 1);
		if (ucmd->impulse && SV_FiltureImpulse(ucmd->impulse, host_client->trustlevel))
			sv_player->v->impulse = ucmd->impulse;

		if (host_client->iscuffed)
		{
			sv_player->v->impulse = 0;
			sv_player->v->button0 = 0;
		}

		if (host_client->state && host_client->protocol != SCP_BAD)
		{
			sv_player->xv->movement[0] = ucmd->forwardmove * host_frametime;
			sv_player->xv->movement[1] = ucmd->sidemove * host_frametime;
			sv_player->xv->movement[2] = ucmd->upmove * host_frametime;
		}

		SV_CheckVelocity(sv_player);

	//
	// angles
	// show 1/3 the pitch angle and all the roll angle
		if (sv_player->v->health > 0)
		{
			if (!sv_player->v->fixangle)
			{
				sv_player->v->angles[PITCH] = -sv_player->v->v_angle[PITCH]/3;
				sv_player->v->angles[YAW] = sv_player->v->v_angle[YAW];
			}
			sv_player->v->angles[ROLL] =
				V_CalcRoll (sv_player->v->angles, sv_player->v->velocity)*4;
		}

		//prethink should be consistant with what the engine normally does
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, client->edict);
		PR_ExecuteProgram (svprogfuncs, pr_global_struct->PlayerPreThink);
		SV_RunThink (client->edict);










		pr_global_struct->input_timelength = ucmd->msec/1000.0f;
	//precision inaccuracies. :(
#define ANGLE2SHORT(x) (x) * (65536/360.0)
		(pr_global_struct->input_angles)[0] = SHORT2ANGLE(ucmd->angles[0]);
		(pr_global_struct->input_angles)[1] = SHORT2ANGLE(ucmd->angles[1]);
		(pr_global_struct->input_angles)[2] = SHORT2ANGLE(ucmd->angles[2]);

		(pr_global_struct->input_movevalues)[0] = ucmd->forwardmove;
		(pr_global_struct->input_movevalues)[1] = ucmd->sidemove;
		(pr_global_struct->input_movevalues)[2] = ucmd->upmove;
		pr_global_struct->input_buttons = ucmd->buttons;
//		pr_global_struct->input_impulse = ucmd->impulse;

		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, client->edict);
		PR_ExecuteProgram(svprogfuncs, gfuncs.RunClientCommand);
 		return true;
	}
	return false;
}

//DP_QC_GETSURFACE
// #434 float(entity e, float s) getsurfacenumpoints (DP_QC_GETSURFACE)
void PF_getsurfacenumpoints(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	unsigned int surfnum;
	model_t *model;
	int modelindex;
	edict_t *ent;

	ent = G_EDICT(prinst, OFS_PARM0);
	surfnum = G_FLOAT(OFS_PARM1);

	modelindex = ent->v->modelindex;
	if (modelindex > 0 && modelindex < MAX_MODELS)
		model = sv.models[(int)ent->v->modelindex];
	else
		model = NULL;

	if (!model || model->type != mod_brush || surfnum >= model->numsurfaces)
		G_FLOAT(OFS_RETURN) = 0;
	else
		G_FLOAT(OFS_RETURN) = model->surfaces[surfnum].mesh->numvertexes;
}
// #435 vector(entity e, float s, float n) getsurfacepoint (DP_QC_GETSURFACE)
void PF_getsurfacepoint(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	unsigned int surfnum, pointnum;
	model_t *model;
	int modelindex;
	edict_t *ent;

	ent = G_EDICT(prinst, OFS_PARM0);
	surfnum = G_FLOAT(OFS_PARM1);
	pointnum = G_FLOAT(OFS_PARM2);

	modelindex = ent->v->modelindex;
	if (modelindex > 0 && modelindex < MAX_MODELS)
		model = sv.models[(int)ent->v->modelindex];
	else
		model = NULL;

	if (!model || model->type != mod_brush || surfnum >= model->numsurfaces)
	{
		G_FLOAT(OFS_RETURN+0) = 0;
		G_FLOAT(OFS_RETURN+1) = 0;
		G_FLOAT(OFS_RETURN+2) = 0;
	}
	else
	{
		G_FLOAT(OFS_RETURN+0) = model->surfaces[surfnum].mesh->xyz_array[pointnum][2];
		G_FLOAT(OFS_RETURN+1) = model->surfaces[surfnum].mesh->xyz_array[pointnum][2];
		G_FLOAT(OFS_RETURN+2) = model->surfaces[surfnum].mesh->xyz_array[pointnum][2];
	}
}
// #436 vector(entity e, float s) getsurfacenormal (DP_QC_GETSURFACE)
void PF_getsurfacenormal(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	unsigned int surfnum, pointnum;
	model_t *model;
	int modelindex;
	edict_t *ent;

	ent = G_EDICT(prinst, OFS_PARM0);
	surfnum = G_FLOAT(OFS_PARM1);
	pointnum = G_FLOAT(OFS_PARM2);

	modelindex = ent->v->modelindex;
	if (modelindex > 0 && modelindex < MAX_MODELS)
		model = sv.models[(int)ent->v->modelindex];
	else
		model = NULL;

	if (!model || model->type != mod_brush || surfnum >= model->numsurfaces)
	{
		G_FLOAT(OFS_RETURN+0) = 0;
		G_FLOAT(OFS_RETURN+1) = 0;
		G_FLOAT(OFS_RETURN+2) = 0;
	}
	else
	{
		G_FLOAT(OFS_RETURN+0) = model->surfaces[surfnum].plane->normal[0];
		G_FLOAT(OFS_RETURN+1) = model->surfaces[surfnum].plane->normal[1];
		G_FLOAT(OFS_RETURN+2) = model->surfaces[surfnum].plane->normal[2];
		if (model->surfaces[surfnum].flags & SURF_PLANEBACK)
			VectorInverse(G_VECTOR(OFS_RETURN));
	}
}
// #437 string(entity e, float s) getsurfacetexture (DP_QC_GETSURFACE)
void PF_getsurfacetexture(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	model_t *model;
	edict_t *ent;
	msurface_t *surf;
	int modelindex;
	int surfnum;

	ent = G_EDICT(prinst, OFS_PARM0);
	surfnum = G_FLOAT(OFS_PARM1);

	modelindex = ent->v->modelindex;
	if (modelindex > 0 && modelindex < MAX_MODELS)
		model = sv.models[(int)ent->v->modelindex];
	else
		model = NULL;

	G_INT(OFS_RETURN) = 0;
	if (!model || model->type != mod_brush)
		return;

	if (surfnum < 0 || surfnum > model->numsurfaces)
		return;

	surf = &model->surfaces[surfnum];
	G_INT(OFS_RETURN) = PR_TempString(prinst, surf->texinfo->texture->name);
}
// #438 float(entity e, vector p) getsurfacenearpoint (DP_QC_GETSURFACE)
void PF_getsurfacenearpoint(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	model_t *model;
	edict_t *ent;
	msurface_t *surf;
	int i;
	float planedist;
	float *point;
	int modelindex;

	vec3_t edgedir;
	vec3_t edgenormal;
	mvertex_t *v1, *v2;
	int edge;
	int e;

	ent = G_EDICT(prinst, OFS_PARM0);
	point = G_VECTOR(OFS_PARM1);

	G_FLOAT(OFS_RETURN) = -1;

	modelindex = ent->v->modelindex;
	if (modelindex > 0 && modelindex < MAX_MODELS)
		model = sv.models[(int)ent->v->modelindex];
	else
		model = NULL;

	if (!model || model->type != mod_brush)
		return;

	if (model->fromgame != fg_quake)
		return;


	surf = model->surfaces;
	for (i = model->numsurfaces; i; i--, surf++)
	{
		if (surf->flags & SURF_PLANEBACK)
			planedist = -DotProduct(point, surf->plane->normal);
		else
			planedist = DotProduct(point, surf->plane->normal);

		if (planedist*planedist < 8*8)
		{	//within a specific range
			//make sure it's within the poly
			for (e = surf->firstedge+surf->numedges, edge = model->surfedges[surf->firstedge]; e > surf->firstedge; e--, edge++)
			{
				if (edge < 0)
				{
					v1 = &model->vertexes[model->edges[-edge].v[0]];
					v2 = &model->vertexes[model->edges[-edge].v[1]];
				}
				else
				{
					v2 = &model->vertexes[model->edges[edge].v[0]];
					v1 = &model->vertexes[model->edges[edge].v[1]];
				}

				if (surf->flags & SURF_PLANEBACK)
				{
					VectorSubtract(v1->position, v2->position, edgedir);
					CrossProduct(edgedir, surf->plane->normal, edgenormal);
					if (DotProduct(edgenormal, v1->position) > DotProduct(edgenormal, point))
						break;
				}
				else
				{
					VectorSubtract(v1->position, v2->position, edgedir);
					CrossProduct(edgedir, surf->plane->normal, edgenormal);
					if (DotProduct(edgenormal, v1->position) < DotProduct(edgenormal, point))
						break;
				}
			}
			if (e == surf->firstedge)
			{
				G_FLOAT(OFS_RETURN) = i;
				break;
			}
		}
	}
}
// #439 vector(entity e, float s, vector p) getsurfaceclippedpoint (DP_QC_GETSURFACE)
void PF_getsurfaceclippedpoint(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
}

qbyte qcpvs[(MAX_MAP_LEAFS+7)/8];
//#240 float(vector viewpos, entity viewee) checkpvs (FTE_QC_CHECKPVS)
//note: this requires a correctly setorigined entity.
void PF_checkpvs(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *viewpos = G_VECTOR(OFS_PARM0);
	edict_t *ent = G_EDICT(prinst, OFS_PARM1);

	//FIXME: Make all alternatives of FatPVS not recalulate the pvs.
	//and yeah, this is overkill what with the whole fat thing and all.
	sv.worldmodel->funcs.FatPVS(sv.worldmodel, viewpos, qcpvs, sizeof(qcpvs), false);

	G_FLOAT(OFS_RETURN) = sv.worldmodel->funcs.EdictInFatPVS(sv.worldmodel, ent, qcpvs);
}

//entity(string match [, float matchnum]) matchclient = #241;
void PF_matchclient(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int clnum=-1;
	char *name = PR_GetStringOfs(prinst, OFS_PARM0);
	int matchnum = G_FLOAT(OFS_PARM1);
	client_t *cl;

	if (*prinst->callargc < 2)
	{
		cl = SV_GetClientForString(name, &clnum);
		if (!cl)
			G_INT(OFS_RETURN) = 0;
		else
			G_INT(OFS_RETURN) = (cl - svs.clients) + 1;

		if ((cl = SV_GetClientForString(name, &clnum)))
			G_INT(OFS_RETURN) = 0;	//prevent multiple matches.
		return;
	}

	while((cl = SV_GetClientForString(name, &clnum)))
	{
		if (!matchnum)
		{	//this is the one that matches
			G_INT(OFS_RETURN) = (cl - svs.clients) + 1;
			return;
		}
		matchnum--;
	}

	G_INT(OFS_RETURN) = 0;	//world
}

void PF_SendPacket(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	netadr_t to;
	char *address = PR_GetStringOfs(prinst, OFS_PARM0);
	char *contents = PF_VarString(prinst, 1, pr_globals);

	NET_StringToAdr(address, &to);
	NET_SendPacket(NS_SERVER, strlen(contents), contents, to);
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
	{"traceline",		PF_svtraceline,		16,		16,		16},	// float(vector v1, vector v2, float tryents) traceline = #16;
	{"checkclient",		PF_checkclient,		17,		17,		17},	// entity() clientlist					= #17;
	{"find",			PF_FindString,		18,		18,		18},	// entity(entity start, .string fld, string match) find = #18;
	{"precache_sound",	PF_precache_sound,	19,		19,		19},	// void(string s) precache_sound		= #19;
	{"precache_model",	PF_precache_model,	20,		20,		20},	// void(string s) precache_model		= #20;
	{"stuffcmd",		PF_stuffcmd,		21,		21,		21},	// void(entity client, string s)stuffcmd = #21;
	{"findradius",		PF_findradius,		22,		22,		22},	// entity(vector org, float rad) findradius = #22;
	{"bprint",			PF_bprint,			23,		23,		23},	// void(string s) bprint				= #23;
//FIXME: distinguish between qw and nq parameters here?
	{"sprint",			PF_sprint,			24,		24,		24},	// void(entity client, string s) sprint = #24;
	{"dprint",			PF_dprint,			25,		0,		25},	// void(string s) dprint				= #25;
	{"print",			PF_print,			0,		25,		0},		// void(string s) print				= #25;
	{"ftos",			PF_ftos,			26,		26,		26},	// void(string s) ftos				= #26;
	{"vtos",			PF_vtos,			27,		27,		27},	// void(string s) vtos				= #27;
	{"coredump",		PF_coredump,		28,		28,		28},	//28
	{"traceon",			PF_traceon,			29,		29,		29},	//29
	{"traceoff",		PF_traceoff,		30,		30,		30},	//30
	{"eprint",			PF_eprint,			31,		31,		31},	//31 // void(entity e) debug print an entire entity
	{"walkmove",		PF_walkmove,		32,		32,		32},	//32 // float(float yaw, float dist) walkmove
	{"tracearea",		PF_traceboxh2,		0,		0,		33},	//33 //
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

	{"swritebyte",		PF_Single_WriteByte},	//52
	{"swritechar",		PF_Single_WriteChar},	//53
	{"swriteshort",		PF_Single_WriteShort},	//54
	{"swritelong",		PF_Single_WriteLong},	//55
	{"swritecoord",		PF_Single_WriteCoord},	//56
	{"swriteangle",		PF_Single_WriteAngle},	//57
	{"swritestring",	PF_Single_WriteString},	//58
	{"swriteentity",	PF_Single_WriteEntity},

	{"bwritebyte",		PF_Broadcast_WriteByte},	//59
	{"bwritechar",		PF_Broadcast_WriteChar},	//60
	{"bwriteshort",		PF_Broadcast_WriteShort},	//61
	{"bwritelong",		PF_Broadcast_WriteLong},	//62
	{"bwritecoord",		PF_Broadcast_WriteCoord},	//63
	{"bwriteangle",		PF_Broadcast_WriteAngle},	//64
	{"bwritestring",	PF_Broadcast_WriteString},	//65
	{"bwriteentity",	PF_Broadcast_WriteEntity},	//66


	{"printfloat",		PF_printf,			0,		0,		60},	//60

	{"sin",				PF_Sin,				0,		0,		62,		60},	//60
	{"cos",				PF_Cos,				0,		0,		61,		61},	//61
	{"sqrt",			PF_Sqrt,			0,		0,		84,		62},	//62

	{"AdvanceFrame",	PF_AdvanceFrame,	0,		0,		63,		0},
	{"printvec",		PF_printv,			0,		0,		64,		0},	//64
	{"RewindFrame",		PF_RewindFrame,		0,		0,		65,		0},
	{"particleexplosion",PF_h2particleexplosion,0,	0,		81,		0},
	{"movestep",		PF_movestep,		0,		0,		82,		0},
	{"advanceweaponframe",PF_advanceweaponframe,0,	0,		83,		0},

	{"setclass",		PF_setclass,		0,		0,		66,		0},

	{"changepitch",		PF_changepitch,		0,		0,		0,		63},
	{"tracetoss",		PF_TraceToss,		0,		0,		0,		64},
	{"etos",			PF_etos,			0,		0,		0,		65},

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
	{"logfrag",			PF_logfrag,			0,		79,		0,		79},	//79

// Tomaz - QuakeC String Manipulation Begin
	{"tq_zone",			PF_dupstring,		0,		0,		0,		79, true},	//79
	{"tq_unzone",		PF_forgetstring,	0,		0,		0,		80, true},	//80
	{"tq_stof",			PF_stof,			0,		0,		0,		81, true},	//81
	{"tq_strcat",		PF_strcat,			0,		0,		0,		82, true},	//82
	{"tq_substring",	PF_substring,		0,		0,		0,		83, true},	//83
	{"tq_stof",			PF_stof,			0,		0,		0,		84, true},	//84
	{"tq_stov",			PF_stov,			0,		0,		0,		85, true},	//85
// Tomaz - QuakeC String Manipulation End

// Tomaz - QuakeC File System Begin (new mods use frik_file instead)
	{"tq_fopen",		PF_fopen,			0,		0,		0,		86, true},// #86 float(string filename, float mode) fopen (QSG_FILE)
	{"tq_fclose",		PF_fclose,			0,		0,		0,		87, true},// #87 void(float fhandle) fclose (QSG_FILE)
	{"tq_fgets",		PF_fgets,			0,		0,		0,		88, true},// #88 string(float fhandle) fgets (QSG_FILE)
	{"tq_fputs",		PF_fputs,			0,		0,		0,		89, true},// #89 void(float fhandle, string s) fputs (QSG_FILE)
// Tomaz - QuakeC File System End

	{"rain_go",			PF_rain_go,			0,		0,		80,		0},	//80

	{"infokey",			PF_infokey,			0,		80,		0,		80},	//80
	{"stof",			PF_stof,			0,		81,		0,		81},	//81
	{"multicast",		PF_multicast,		0,		82,		0,		0},	//82



//mvdsv (don't require ebfs usage in qw)
	{"executecommand",	PF_ExecuteCommand,	0,		0,		0,		83, true},	//83		//void() exec;   please don't use.
	{"mvdtokenize",		PF_Tokenize, 		0,		0,		0,		84, true},	//84			//void(string str) tokanize;
	{"mvdargc",			PF_ArgC,			0,		0,		0,		85, true},	//85			//float() argc;
	{"mvdargv",			PF_ArgV,			0,		0,		0,		86, true},	//86			//string(float num) argv;

//mvd commands
//some of these are a little iffy.
//we support them for mvdsv compatability but some of them look very hacky.
//these ones are not honoured with numbers, but can be used via the proper means.
	{"teamfield",		PF_teamfield,		0,		0,		0,		87, true},
	{"substr",			PF_substr,			0,		0,		0,		88, true},
	{"mvdstrcat",		PF_strcat,			0,		0,		0,		89, true},
	{"mvdstrlen",		PF_strlen,			0,		0,		0,		90, true},
	{"str2byte",		PF_str2byte,		0,		0,		0,		91, true},
	{"str2short",		PF_str2short,		0,		0,		0,		92, true},
	{"mvdnewstr",		PF_newstring,		0,		0,		0,		93, true},
	{"mvdfreestr",		PF_forgetstring,	0,		0,		0,		94, true},
	{"conprint",		PF_conprint,		0,		0,		0,		95, true},
	{"readcmd",			PF_readcmd,			0,		0,		0,		96, true},
	{"mvdstrcpy",		PF_MVDSV_strcpy,	0,		0,		0,		97, true},
	{"strstr",			PF_strstr,			0,		0,		0,		98, true},
	{"mvdstrncpy",		PF_MVDSV_strncpy,	0,		0,		0,		99, true},
	{"log",				PF_log,				0,		0,		0,		100, true},
//	{"redirectcmd",		PF_redirectcmd,		0,		0,		0,		101, true},
	{"mvdcalltimeofday",PF_calltimeofday,	0,		0,		0,		102, true},
	{"forcedemoframe",	PF_forcedemoframe,	0,		0,		0,		103, true},
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
	{"stopsound",		PF_StopSound,		0,		0,		106,	0},

	{"precache_model4",	PF_precache_model,	0,		0,		116,	0},//please don't use...
	{"precache_sound4",	PF_precache_sound,	0,		0,		117,	0},

	{"tracebox",		PF_traceboxdp,		0,		0,		0,		90},

	{"randomvec",		PF_randomvector,	0,		0,		0,		91},
	{"getlight",		PF_sv_getlight,		0,		0,		0,		92},// #92 vector(vector org) getlight (DP_QC_GETLIGHT),
	{"registercvar",	PF_registercvar,	0,		0,		0,		93},
	{"min",				PF_min,				0,		0,		0,		94},// #94 float(float a, floats) min (DP_QC_MINMAXBOUND)
	{"max",				PF_max,				0,		0,		0,		95},	// #95 float(float a, floats) max (DP_QC_MINMAXBOUND)
	{"bound",			PF_bound,			0,		0,		0,		96},	// #96 float(float minimum, float val, float maximum) bound (DP_QC_MINMAXBOUND)
	{"pow",				PF_pow,				0,		0,		0,		97},
	{"tj_cvar_string",	PF_cvar_string,		0,		0,		0,		97, true},	//telejano
//DP_QC_FINDFLOAT
	{"findfloat",		PF_FindFloat,		0,		0,		0,		98},	// #98 entity(entity start, float fld, float match) findfloat (DP_QC_FINDFLOAT)

	{"checkextension",	PF_checkextension,	99,		99,		0,		99},	// #99	//darkplaces system - query a string to see if the mod supports X Y and Z.
	{"builtin_find",	PF_builtinsupported,100,	100,	0,		100},	// #100	//per builtin system.
	{"anglemod",		PF_anglemod,		0,		0,		0,		102},
	{"qsg_cvar_string",	PF_cvar_string,		0,		0,		0,		103},

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
	{"getmodelindex",	PF_getmodelindex,	0,		0,		0,		200},
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

//EXT_DIMENSION_PLANES
	{"bitshift",		PF_bitshift,		0,		0,		0,		218},

//I guess this should go under DP_TE_STANDARDEFFECTBUILTINS...
	{"te_lightningblood",PF_te_lightningblood,	0,	0,		0,		219},// #219 te_lightningblood

	{"map_builtin",		PF_builtinsupported,0,		0,		0,		220},	//like #100 - takes 2 args. arg0 is builtinname, 1 is number to map to.

//FTE_STRINGS
	{"strstrofs",		PF_strstrofs,		0,		0,		0,		221},
	{"str2chr",			PF_str2chr,			0,		0,		0,		222},
	{"chr2str",			PF_chr2str,			0,		0,		0,		223},
	{"strconv",			PF_strconv,			0,		0,		0,		224},
	{"strpad",			PF_strpad,			0,		0,		0,		225},	//will be moved
	{"infoadd",			PF_infoadd,			0,		0,		0,		226},
	{"infoget",			PF_infoget,			0,		0,		0,		227},
	{"strncmp",			PF_strncmp,			0,		0,		0,		228},
	{"strcasecmp",		PF_strcasecmp,		0,		0,		0,		229},
	{"strncasecmp",		PF_strncasecmp,		0,		0,		0,		230},
//END FTE_STRINGS

//FTE_CALLTIMEOFDAY
	{"calltimeofday",	PF_calltimeofday,	0,		0,		0,		231},

//EXT_CSQC
	{"clientstat",		PF_clientstat,		0,		0,		0,		232},	//EXT_CSQC
	{"globalstat",		PF_globalstat,		0,		0,		0,		233},	//EXT_CSQC_1 actually
//END EXT_CSQC
	{"isbackbuffered",	PF_isbackbuffered,	0,		0,		0,		234},
	{"skinforname",		PF_skinforname,		0,		0,		0,		237},		// #237
	{"te_bloodqw",		PF_te_bloodqw,		0,		0,		0,		239},

	{"checkpvs",		PF_checkpvs,		0,		0,		0,		240},
	{"matchclientname",	PF_matchclient,		0,		0,		0,		241},
	{"sendpacket",		PF_SendPacket,		0,		0,		0,		242},	//void(string dest, string content) sendpacket = #242; (FTE_QC_SENDPACKET)
#ifdef PEXT_BULLETENS
	{"bulleten",		PF_bulleten,		0,		0,		0,		243},
#endif

#ifdef SQL
	{"sqlconnect",		PF_sqlconnect,		0,		0,		0,		250},	// #250 float([string host], [string user], [string pass], [string defaultdb], [string driver]) sqlconnect (FTE_SQL)
	{"sqldisconnect",	PF_sqldisconnect,	0,		0,		0,		251},	// #251 void(float serveridx) sqldisconnect (FTE_SQL)
	{"sqlopenquery",	PF_sqlopenquery,	0,		0,		0,		252},	// #252 float(float serveridx, void(float serveridx, float queryidx, float rows, float columns, float eof) callback, float querytype, string query) sqlopenquery (FTE_SQL)
	{"sqlclosequery",	PF_sqlclosequery,	0,		0,		0,		253},	// #253 void(float serveridx, float queryidx) sqlclosequery (FTE_SQL)
	{"sqlreadfield",	PF_sqlreadfield,	0,		0,		0,		254},	// #254 string(float serveridx, float queryidx, float row, float column) sqlreadfield (FTE_SQL)
	{"sqlerror",		PF_sqlerror,		0,		0,		0,		255},	// #255 string(float serveridx, [float queryidx]) sqlerror (FTE_SQL)
	{"sqlescape",		PF_sqlescape,		0,		0,		0,		256},	// #256 string(float serveridx, string data) sqlescape (FTE_SQL)
	{"sqlversion",		PF_sqlversion,		0,		0,		0,		257},	// #257 string(float serveridx) sqlversion (FTE_SQL)
	{"sqlreadfloat",	PF_sqlreadfloat,	0,		0,		0,		258},	// #258 float(float serveridx, float queryidx, float row, float column) sqlreadfloat (FTE_SQL)
#endif
	{"stoi",			PF_stoi,			0,		0,		0,		259},
	{"itos",			PF_itos,			0,		0,		0,		260},
	{"stoh",			PF_stoh,			0,		0,		0,		261},
	{"htos",			PF_htos,			0,		0,		0,		262},

	{"frameforname",	PF_frameforname,	0,		0,		0,		276},//void(float modidx, string framename) frameforname = #276 (FTE_CSQC_SKELETONOBJECTS)
	{"frameduration",	PF_frameduration,	0,		0,		0,		277},//float(float modidx, float framenum) frameduration = #277 (FTE_CSQC_SKELETONOBJECTS)


//EXT_CSQC
//	{"setmodelindex",	PF_sv_SetModelIndex,0,		0,		0,		333},	// #333 void(entity e, float mdlindex) setmodelindex (EXT_CSQC)
//	{"modelnameforindex",PF_sv_ModelnameForIndex,0,	0,		0,		334},	// #334 string(float mdlindex) modelnameforindex (EXT_CSQC)

	{"particleeffectnum",PF_sv_particleeffectnum,0,	0,		0,		335},	// #335 float(string effectname) particleeffectnum (EXT_CSQC)
	{"trailparticles",	PF_sv_trailparticles,0,		0,		0,		336},	// #336 void(entity ent, float effectnum, vector start, vector end) trailparticles (EXT_CSQC),
	{"pointparticles",	PF_sv_pointparticles,0,		0,		0,		337},	// #337 void(float effectnum, vector origin [, vector dir, float count]) pointparticles (EXT_CSQC)

//	{"cprint",			PF_sv_cprint,		0,		0,		0,		338},	// #338 void(string s) cprint (EXT_CSQC)
	{"print",			PF_print,		0,		0,		0,		339},	// #339 void(string s) print (EXT_CSQC)

	{"runclientphys",	PF_runclientphys,	0,		0,		0,		347},

//	{"runningserver",	PF_sv_runningserver,0,		0,		0,		350},	// #350 float() isserver (EXT_CSQC)
//	{"registercommand",	PF_sv_registercommand,0,	0,		0,		352},	// #352 void(string cmdname) registercommand (EXT_CSQC)
	{"wasfreed",		PF_WasFreed,		0,		0,		0,		353},	// #353 float(entity ent) wasfreed (EXT_CSQC) (should be availabe on server too)
//	{"serverkey",		PF_sv_serverkey,	0,		0,		0,		354},	// #354 string(string key) serverkey;
//END EXT_CSQC

//end fte extras

//DP extras

//DP_QC_COPYENTITY
	{"copyentity",		PF_copyentity,		0,		0,		0,		400},// #400 void(entity from, entity to) copyentity (DP_QC_COPYENTITY)
//DP_SV_SETCOLOR
	{"setcolors",		PF_setcolors,		0,		0,		0,		401},// #401 void(entity from, entity to) setcolors
//DP_QC_FINDCHAIN
	{"findchain",		PF_sv_findchain,		0,		0,		0,		402},// #402 entity(string field, string match) findchain (DP_QC_FINDCHAIN)
//DP_QC_FINDCHAINFLOAT
	{"findchainfloat",	PF_sv_findchainfloat,	0,		0,		0,		403},// #403 entity(float fld, float match) findchainfloat (DP_QC_FINDCHAINFLOAT)
//DP_SV_EFFECT
	{"effect",			PF_effect,			0,		0,		0,		404},// #404 void(vector org, string modelname, float startframe, float endframe, float framerate) effect (DP_SV_EFFECT)
//DP_TE_BLOOD
	{"te_blood",		PF_te_blooddp,		0,		0,		0,		405},// #405 te_blood
//DP_TE_BLOODSHOWER
	{"te_bloodshower",	PF_te_bloodshower,	0,		0,		0,		406},// #406 void(vector mincorner, vector maxcorner, float explosionspeed, float howmany) te_bloodshower (DP_TE_BLOODSHOWER)
//DP_TE_EXPLOSIONRGB
	{"te_explosionrgb",	PF_te_explosionrgb,	0,		0,		0,		407},// #407 void(vector org, vector color) te_explosionrgb (DP_TE_EXPLOSIONRGB)
//DP_TE_PARTICLECUBE
	{"te_particlecube",	PF_te_particlecube,	0,		0,		0,		408},// #408 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color, float gravityflag, float randomveljitter) te_particlecube (DP_TE_PARTICLECUBE)
//DP_TE_PARTICLERAIN
	{"te_particlerain",	PF_te_particlerain,	0,		0,		0,		409},// #409 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color) te_particlerain (DP_TE_PARTICLERAIN)
//DP_TE_PARTICLESNOW
	{"te_particlesnow",	PF_te_particlesnow,	0,		0,		0,		410},// #410 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color) te_particlesnow (DP_TE_PARTICLESNOW)
//DP_TE_SPARK
	{"te_spark",		PF_te_spark,		0,		0,		0,		411},// #411 void(vector org, vector vel, float howmany) te_spark (DP_TE_SPARK)
//DP_TE_QUADEFFECTS1
	{"te_gunshotquad",	PF_te_gunshotquad,	0,		0,		0,		412},// #412 void(vector org) te_gunshotquad (DP_TE_QUADEFFECTS1)
	{"te_spikequad",	PF_te_spikequad,	0,		0,		0,		413},// #413 void(vector org) te_spikequad (DP_TE_QUADEFFECTS1)
	{"te_superspikequad",PF_te_superspikequad,0,	0,		0,		414},// #414 void(vector org) te_superspikequad (DP_TE_QUADEFFECTS1)
	{"te_explosionquad",PF_te_explosionquad,0,		0,		0,		415},// #415 void(vector org) te_explosionquad (DP_TE_QUADEFFECTS1)
//DP_TE_SMALLFLASH
	{"te_smallflash",	PF_te_smallflash,	0,		0,		0,		416},// #416 void(vector org) te_smallflash (DP_TE_SMALLFLASH)
//DP_TE_CUSTOMFLASH
	{"te_customflash",	PF_te_customflash,	0,		0,		0,		417},// #417 void(vector org, float radius, float lifetime, vector color) te_customflash (DP_TE_CUSTOMFLASH)

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

	{"te_plasmaburn",	PF_te_plasmaburn,	0,		0,		0,		433},// #433 void(vector org) te_plasmaburn (DP_TE_PLASMABURN)

	{"getsurfacenumpoints",PF_getsurfacenumpoints,0,0,		0,		434},// #434 float(entity e, float s) getsurfacenumpoints (DP_QC_GETSURFACE)
	{"getsurfacepoint",PF_getsurfacepoint,	0,		0,		0,		435},// #435 vector(entity e, float s, float n) getsurfacepoint (DP_QC_GETSURFACE)
	{"getsurfacenormal",PF_getsurfacenormal,0,		0,		0,		436},// #436 vector(entity e, float s) getsurfacenormal (DP_QC_GETSURFACE)
//	{"getsurfacetexture",PF_getsurfacetexture,0,	0,		0,		437},// #437 string(entity e, float s) getsurfacetexture (DP_QC_GETSURFACE)
	{"getsurfacenearpoint",PF_getsurfacenearpoint,0,0,		0,		438},// #438 float(entity e, vector p) getsurfacenearpoint (DP_QC_GETSURFACE)
	{"getsurfaceclippedpoint",PF_getsurfaceclippedpoint,0,0,0,		439},// #439 vector(entity e, float s, vector p) getsurfaceclippedpoint (DP_QC_GETSURFACE)

//KRIMZON_SV_PARSECLIENTCOMMAND
	{"clientcommand",	PF_clientcommand,	0,		0,		0,		440},// #440 void(entity e, string s) clientcommand (KRIMZON_SV_PARSECLIENTCOMMAND)
	{"tokenize",		PF_Tokenize,		0,		0,		0,		441},// #441 float(string s) tokenize (KRIMZON_SV_PARSECLIENTCOMMAND)
	{"argv",			PF_ArgV,			0,		0,		0,		442},// #442 string(float n) argv (KRIMZON_SV_PARSECLIENTCOMMAND

//DP_GFX_QUAKE3MODELTAGS
	{"setattachment",	PF_setattachment,	0,		0,		0,		443},// #443 void(entity e, entity tagentity, string tagname) setattachment (DP_GFX_QUAKE3MODELTAGS)

	{"search_begin",	PF_search_begin,	0,		0,		0,		444},
	{"search_end",		PF_search_end,		0,		0,		0,		445},
	{"search_getsize",	PF_search_getsize,	0,		0,		0,		446},
	{"search_getfilename", PF_search_getfilename,0,	0,		0,		447},
//DP_QC_CVAR_STRING
	{"dp_cvar_string",	PF_cvar_string,		0,		0,		0,		448},// #448 string(float n) cvar_string

//DP_QC_FINDFLAGS
	{"findflags",		PF_FindFlags,		0,		0,		0,		449},// #449 entity(entity start, .entity fld, float match) findflags
//DP_QC_FINDCHAINFLAGS
	{"findchainflags",	PF_sv_findchainflags,	0,		0,		0,		450},// #450 entity(.float fld, float match) findchainflags
//DP_MD3_TAGSINFO
	{"gettagindex",		PF_gettagindex,		0,		0,		0,		451},// #451 float(entity ent, string tagname) gettagindex (DP_MD3_TAGSINFO)
	{"gettaginfo",		PF_sv_gettaginfo,		0,		0,		0,		452},// #452 vector(entity ent, float tagindex) gettaginfo (DP_MD3_TAGSINFO)
//DP_SV_BOTCLIENT
	{"dropclient",		PF_dropclient,		0,		0,		0,		453},// #453 void(entity player) dropclient

	{"spawnclient",		PF_spawnclient,		0,		0,		0,		454},	//entity() spawnclient = #454;
	{"clienttype",		PF_clienttype,		0,		0,		0,		455},	//float(entity client) clienttype = #455;

	{"WriteUnterminatedString",PF_WriteString2,0,	0,		0,		456},	//writestring but without the null terminator. makes things a little nicer.

//DP_TE_FLAMEJET
//	{"te_flamejet",		PF_te_flamejet,		0,		0,		0,		457},	// #457 void(vector org, vector vel, float howmany) te_flamejet

	//no 458 documented.

//DP_QC_EDICT_NUM
	{"edict_num",		PF_edict_for_num,	0,		0,		0,		459},	// #459 entity(float entnum) edict_num

//DP_QC_STRINGBUFFERS
	{"buf_create",		PF_buf_create,		0,		0,		0,		460},	// #460 float() buf_create
	{"buf_del",			PF_buf_del,			0,		0,		0,		461},	// #461 void(float bufhandle) buf_del
	{"buf_getsize",		PF_buf_getsize,		0,		0,		0,		462},	// #462 float(float bufhandle) buf_getsize
	{"buf_copy",		PF_buf_copy,		0,		0,		0,		463},	// #463 void(float bufhandle_from, float bufhandle_to) buf_copy
	{"buf_sort",		PF_buf_sort,		0,		0,		0,		464},	// #464 void(float bufhandle, float sortpower, float backward) buf_sort
	{"buf_implode",		PF_buf_implode,		0,		0,		0,		465},	// #465 string(float bufhandle, string glue) buf_implode
	{"bufstr_get",		PF_bufstr_get,		0,		0,		0,		466},	// #466 string(float bufhandle, float string_index) bufstr_get
	{"bufstr_set",		PF_bufstr_set,		0,		0,		0,		467},	// #467 void(float bufhandle, float string_index, string str) bufstr_set
	{"bufstr_add",		PF_bufstr_add,		0,		0,		0,		468},	// #468 float(float bufhandle, string str, float order) bufstr_add
	{"bufstr_free",		PF_bufstr_free,		0,		0,		0,		469},	// #469 void(float bufhandle, float string_index) bufstr_free

	//no 470 documented

//DP_QC_ASINACOSATANATAN2TAN
	{"asin",			PF_asin,			0,		0,		0,		471},	// #471 float(float s) asin
	{"acos",			PF_acos,			0,		0,		0,		472},	// #472 float(float c) acos
	{"atan",			PF_atan,			0,		0,		0,		473},	// #473 float(float t) atan
	{"atan2",			PF_atan2,			0,		0,		0,		474},	// #474 float(float c, float s) atan2
	{"tan",				PF_tan,				0,		0,		0,		475},	// #475 float(float a) tan


//DP_QC_STRINGCOLORFUNCTIONS
	{"strlennocol",		PF_strlennocol,		0,		0,		0,		476},	// #476 float(string s) strlennocol
	{"strdecolorize",	PF_strdecolorize,	0,		0,		0,		477},	// #477 string(string s) strdecolorize

//DP_QC_STRFTIME
	{"strftime",		PF_strftime,		0,		0,		0,		478},	// #478 string(float uselocaltime, string format, ...) strftime

//DP_QC_TOKENIZEBYSEPARATOR
	{"tokenizebyseparator",PF_tokenizebyseparator,0,0,		0,		479},	// #479 float(string s, string separator1, ...) tokenizebyseparator

//DP_QC_STRING_CASE_FUNCTIONS
	{"strtolower",		PF_strtolower,		0,		0,		0,		480},	// #476 string(string s) strtolower
	{"strtoupper",		PF_strtoupper,		0,		0,		0,		481},	// #476 string(string s) strlennocol

//DP_QC_CVAR_DEFSTRING
	{"cvar_defstring",	PF_cvar_defstring,	0,		0,		0,		482},	// #482 string(string s) cvar_defstring

//DP_SV_POINTSOUND
	{"pointsound",		PF_pointsound,		0,		0,		0,		483},	// #483 void(vector origin, string sample, float volume, float attenuation) pointsound

//DP_QC_STRREPLACE
	{"strreplace",		PF_strreplace,		0,		0,		0,		484},	// #484 string(string search, string replace, string subject) strreplace
	{"strireplace",		PF_strireplace,		0,		0,		0,		485},	// #485 string(string search, string replace, string subject) strireplace


//DP_QC_GETSURFACEPOINTATTRIBUTE
	{"getsurfacepointattribute",PF_getsurfacepointattribute,0,0,0,	486},	// #486vector(entity e, float s, float n, float a) getsurfacepointattribute

//DP_GECKO_SUPPORT
	{"gecko_create",	PF_gecko_create,	0,		0,		0,		487},	// #487 float(string name) gecko_create( string name )
	{"gecko_destroy",	PF_gecko_destroy,	0,		0,		0,		488},	// #488 void(string name) gecko_destroy( string name )
	{"gecko_navigate",	PF_gecko_navigate,	0,		0,		0,		489},	// #489 void(string name) gecko_navigate( string name, string URI )
	{"gecko_keyevent",	PF_gecko_keyevent,	0,		0,		0,		490},	// #490 float(string name) gecko_keyevent( string name, float key, float eventtype )
	{"gecko_mousemove",	PF_gecko_mousemove,	0,		0,		0,		491},	// #491 void gecko_mousemove( string name, float x, float y )
	{"gecko_resize",	PF_gecko_resize,	0,		0,		0,		492},	// #492 void gecko_resize( string name, float w, float h )
	{"gecko_get_texture_extent",PF_gecko_get_texture_extent,0,0,0,	493},	// #493 vector gecko_get_texture_extent( string name )

//DP_QC_CRC16
	{"crc16",			PF_crc16,			0,		0,		0,		494},	// #494 float(float caseinsensitive, string s, ...) crc16

//DP_QC_CVAR_TYPE
	{"cvar_type",		PF_cvar_type,		0,		0,		0,		495},	// #495 float(string name) cvar_type

//DP_QC_ENTITYDATA
	{"numentityfields",	PF_numentityfields,	0,		0,		0,		496},	// #496 float() numentityfields
	{"entityfieldname",	PF_entityfieldname,	0,		0,		0,		497},	// #497 string(float fieldnum) entityfieldname
	{"entityfieldtype",	PF_entityfieldtype,	0,		0,		0,		498},	// #498 float(float fieldnum) entityfieldtype
	{"getentityfieldstring",PF_getentityfieldstring,0,0,	0,		499},	// #499 string(float fieldnum, entity ent) getentityfieldstring
	{"putentityfieldstring",PF_putentityfieldstring,0,0,	0,		500},	// #500 float(float fieldnum, entity ent, string s) putentityfieldstring

//DP_SV_WRITEPICTURE
	{"WritePicture",	PF_WritePicture,	0,		0,		0,		501},	// #501 void(float to, string s, float sz) WritePicture

	//no 502 documented

//DP_QC_WHICHPACK
	{"whichpack",		PF_whichpack,		0,		0,		0,		503},	// #503 string(string filename) whichpack

//DP_QC_URI_ESCAPE
	{"uri_escape",		PF_uri_escape,		0,		0,		0,		510},	// #510 string(string in) uri_escape
	{"uri_unescape",	PF_uri_unescape,	0,		0,		0,		511},	// #511 string(string in) uri_unescape = #511;

//DP_QC_NUM_FOR_EDICT
	{"num_for_edict",	PF_num_for_edict,	0,		0,		0,		512},	// #512 float(entity ent) num_for_edict

//DP_QC_URI_GET
	{"uri_get",			PF_uri_get,			0,		0,		0,		513},	// #513 float(string uril, float id) uri_get

	//no 504
//end dp extras

	{"precache_vwep_model",PF_precache_vwep_model,0,0,		0,		532},	// #532 float(string mname) precache_vwep_model

	//don't exceed sizeof(pr_builtin)/sizeof(pr_builtin[0]) (currently 1024) without modifing the size of pr_builtin

	{NULL}
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

	if (type == PROG_PREREL)
	{
		pr_builtin[52] = PF_Single_WriteByte;
		pr_builtin[53] = PF_Single_WriteChar;
		pr_builtin[54] = PF_Single_WriteShort;
		pr_builtin[55] = PF_Single_WriteLong;
		pr_builtin[56] = PF_Single_WriteCoord;
		pr_builtin[57] = PF_Single_WriteAngle;
		pr_builtin[58] = PF_Single_WriteString;
		//lack of writeentity is intentional (prerel doesn't have it.

		pr_builtin[59] = PF_Broadcast_WriteByte;
		pr_builtin[60] = PF_Broadcast_WriteChar;
		pr_builtin[61] = PF_Broadcast_WriteShort;
		pr_builtin[62] = PF_Broadcast_WriteLong;
		pr_builtin[63] = PF_Broadcast_WriteCoord;
		pr_builtin[64] = PF_Broadcast_WriteAngle;
		pr_builtin[65] = PF_Broadcast_WriteString;
		pr_builtin[66] = PF_Broadcast_WriteEntity;
	}

	if (!pr_compatabilitytest.value)
	{
		for (i = 0; BuiltinList[i].name; i++)
		{
			if (BuiltinList[i].ebfsnum && !BuiltinList[i].obsolete)
				builtincount[BuiltinList[i].ebfsnum]++;
		}
		for (i = 0; BuiltinList[i].name; i++)
		{
			if (BuiltinList[i].ebfsnum)
			{
				if (pr_builtin[BuiltinList[i].ebfsnum] == PF_Fixme && builtincount[BuiltinList[i].ebfsnum] == (BuiltinList[i].obsolete?0:1))
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
				Con_Printf("Failed to map builtin %s to %i specified in fte_bimap.dat\n", com_token, binum);
		}
	}


	for (i = 0; i < QSG_Extensions_count; i++)
	{
		if (QSG_Extensions[i].enabled)
			*QSG_Extensions[i].enabled = false;
	}

	if (type == PROG_QW && pr_imitatemvdsv.value>0)	//pretend to be mvdsv for a bit.
	{
		if (PR_EnableEBFSBuiltin("teamfield",		87) != 87 ||
			PR_EnableEBFSBuiltin("substr",			88) != 88 ||
			PR_EnableEBFSBuiltin("mvdstrcat",		89) != 89 ||
			PR_EnableEBFSBuiltin("mvdstrlen",		90) != 90 ||
			PR_EnableEBFSBuiltin("str2byte",		91) != 91 ||
			PR_EnableEBFSBuiltin("str2short",		92) != 92 ||
			PR_EnableEBFSBuiltin("mvdnewstr",		93) != 93 ||
			PR_EnableEBFSBuiltin("mvdfreestr",		94) != 94 ||
			PR_EnableEBFSBuiltin("conprint",		95) != 95 ||
			PR_EnableEBFSBuiltin("readcmd",			96) != 96 ||
			PR_EnableEBFSBuiltin("mvdstrcpy",		97) != 97 ||
			PR_EnableEBFSBuiltin("strstr",			98) != 98 ||
			PR_EnableEBFSBuiltin("mvdstrncpy",		99) != 99 ||
			PR_EnableEBFSBuiltin("log",				100)!= 100 ||
//			PR_EnableEBFSBuiltin("redirectcmd",		101)!= 101 ||
			PR_EnableEBFSBuiltin("mvdcalltimeofday",102)!= 102 ||
			PR_EnableEBFSBuiltin("forcedemoframe",	103)!= 103)
			Con_Printf("Failed to register all MVDSV builtins\n");
		else
			Con_Printf("Be aware that MVDSV does not follow standards. Please encourage mod developers to not require pr_imitatemvdsv to be set.\n");
	}
}

void PR_SVExtensionList_f(void)
{
	int i;
	int ebi;
	int bi;
	lh_extension_t *extlist;

#define SHOW_ACTIVEEXT 1
#define SHOW_ACTIVEBI 2
#define SHOW_NOTSUPPORTEDEXT 4
#define SHOW_NOTACTIVEEXT 8
#define SHOW_NOTACTIVEBI 16

	int showflags = atoi(Cmd_Argv(1));
	if (!showflags)
		showflags = SHOW_ACTIVEEXT|SHOW_NOTACTIVEEXT;

	if (showflags & (SHOW_ACTIVEBI|SHOW_NOTACTIVEBI))
	for (i = 0; BuiltinList[i].name; i++)
	{
		if (!BuiltinList[i].ebfsnum)
			continue;	//a reserved builtin.
		if (BuiltinList[i].bifunc == PF_Fixme)
			Con_Printf("^1%s:%i needs to be added\n", BuiltinList[i].name, BuiltinList[i].ebfsnum);
		else if (pr_builtin[BuiltinList[i].ebfsnum] == BuiltinList[i].bifunc)
		{
			if (showflags & SHOW_ACTIVEBI)
				Con_Printf("%s is active on %i\n", BuiltinList[i].name, BuiltinList[i].ebfsnum);
		}
		else
		{
			if (showflags & SHOW_NOTACTIVEBI)
				Con_Printf("^4%s is NOT active (%i)\n", BuiltinList[i].name, BuiltinList[i].ebfsnum);
		}
	}

	if (showflags & (SHOW_NOTSUPPORTEDEXT|SHOW_NOTACTIVEEXT|SHOW_ACTIVEEXT))
	{
		extlist = QSG_Extensions;

		for (i = 0; i < QSG_Extensions_count; i++)
		{
			if (!extlist[i].name)
				continue;

			if (i < 32)
			{
				if (!(svs.fteprotocolextensions & (1<<i)))
				{
					if (showflags & SHOW_NOTSUPPORTEDEXT)
						Con_Printf("^4protocol %s is not supported\n", extlist[i].name);
					continue;
				}
			}

			for (ebi = 0; ebi < extlist[i].numbuiltins; ebi++)
			{
				for (bi = 0; BuiltinList[bi].name; bi++)
				{
					if (!strcmp(BuiltinList[bi].name, extlist[i].builtinnames[ebi]))
						break;
				}

				if (!BuiltinList[bi].name)
				{
					if (showflags & SHOW_NOTSUPPORTEDEXT)
						Con_Printf("^4%s is not supported\n", extlist[i].name);
					break;
				}
				if (pr_builtin[BuiltinList[bi].ebfsnum] != BuiltinList[bi].bifunc)
				{
					if (pr_builtin[BuiltinList[bi].ebfsnum] == PF_Fixme)
					{
						if (showflags & SHOW_NOTACTIVEEXT)
							Con_Printf("^4%s is not currently active (builtin: %s#%i)\n", extlist[i].name, BuiltinList[bi].name, BuiltinList[bi].ebfsnum);
					}
					else
					{
						if (showflags & SHOW_NOTACTIVEEXT)
							Con_Printf("^4%s was overridden (builtin: %s#%i)\n", extlist[i].name, BuiltinList[bi].name, BuiltinList[bi].ebfsnum);
					}
					break;
				}
			}
			if (ebi == extlist[i].numbuiltins)
			{
				if (showflags & SHOW_ACTIVEEXT)
				{
					if (!extlist[i].numbuiltins)
						Con_Printf("%s is supported\n", extlist[i].name);
					else
						Con_Printf("%s is currently active\n", extlist[i].name);
				}
			}
		}
	}
}

builtin_t *pr_builtins = pr_builtin;
int pr_numbuiltins = sizeof(pr_builtin)/sizeof(pr_builtin[0]);

void PR_RegisterFields(void)	//it's just easier to do it this way.
{
#define fieldfloat(name) PR_RegisterFieldVar(svprogfuncs, ev_float, #name, (int)&((stdentvars_t*)0)->name, -1)
#define fieldvector(name) PR_RegisterFieldVar(svprogfuncs, ev_vector, #name, (int)&((stdentvars_t*)0)->name, -1)
#define fieldentity(name) PR_RegisterFieldVar(svprogfuncs, ev_entity, #name, (int)&((stdentvars_t*)0)->name, -1)
#define fieldstring(name) PR_RegisterFieldVar(svprogfuncs, ev_string, #name, (int)&((stdentvars_t*)0)->name, -1)
#define fieldfunction(name) PR_RegisterFieldVar(svprogfuncs, ev_function, #name, (int)&((stdentvars_t*)0)->name, -1)

#ifdef VM_Q1
#define fieldxfloat(name) PR_RegisterFieldVar(svprogfuncs, ev_float, #name, sizeof(stdentvars_t) + (int)&((extentvars_t*)0)->name, -1)
#define fieldxvector(name) PR_RegisterFieldVar(svprogfuncs, ev_vector, #name, sizeof(stdentvars_t) + (int)&((extentvars_t*)0)->name, -1)
#define fieldxentity(name) PR_RegisterFieldVar(svprogfuncs, ev_entity, #name, sizeof(stdentvars_t) + (int)&((extentvars_t*)0)->name, -1)
#define fieldxstring(name) PR_RegisterFieldVar(svprogfuncs, ev_string, #name, sizeof(stdentvars_t) + (int)&((extentvars_t*)0)->name, -1)
#define fieldxfunction(name) PR_RegisterFieldVar(svprogfuncs, ev_function, #name, sizeof(stdentvars_t) + (int)&((extentvars_t*)0)->name, -1)
#else
#define fieldxfloat fieldfloat
#define fieldxvector fieldvector
#define fieldxentity fieldentity
#define fieldxstring fieldstring
#define fieldxfunction fieldfunction
#endif

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
	fieldxfloat(button3);
	fieldxfloat(button4);
	fieldxfloat(button5);
	fieldxfloat(button6);
	fieldxfloat(button7);
	fieldxfloat(button8);
	fieldxfloat(gravity);		//standard extension
	fieldxfloat(maxspeed);	//standard extension
	fieldxfloat(items2);	//standard nq
	fieldxvector(punchangle);//standard nq
	fieldxfloat(scale);
	fieldxfloat(alpha);
	fieldxfloat(fatness);
	fieldxentity(view2);
	fieldxvector(movement);
	fieldxfloat(pmove_flags);
	fieldxfloat(vw_index);

	//dp extra fields
	fieldxentity(nodrawtoclient);
	fieldxentity(drawonlytoclient);
	fieldxentity(viewmodelforclient);
	fieldxentity(exteriormodeltoclient);

	fieldxfloat(viewzoom);

	fieldxentity(tag_entity);
	fieldxfloat(tag_index);

	fieldxfloat(glow_size);
	fieldxfloat(glow_color);
	fieldxfloat(glow_trail);

	fieldxvector(colormod);

//	if (progstype == PROG_H2)
	{
		fieldxvector(color);
	}
	fieldxfloat(light_lev);
	fieldxfloat(style);
	fieldxfloat(pflags);

	fieldxfloat(clientcolors);

//hexen 2 stuff
	fieldxfloat(playerclass);
	fieldxfloat(hull);
	fieldxfloat(hasted);

	fieldxfloat(light_level);
	fieldxfloat(abslight);
	fieldxfloat(drawflags);
	fieldxentity(movechain);
	fieldxfunction(chainmoved);

	//QSG_DIMENSION_PLANES
	fieldxfloat(dimension_see);
	fieldxfloat(dimension_seen);
	fieldxfloat(dimension_ghost);
	fieldxfloat(dimension_ghost_alpha);
	fieldxfloat(dimension_solid);
	fieldxfloat(dimension_hit);


	// EXT_CSQC
	fieldxfunction(SendEntity);
	fieldxfloat(SendFlags);
	fieldxfloat(Version);
	fieldxfloat(pvsflags);

	// FTE_ENT_UNIQUESPAWNID
	fieldxfloat(uniquespawnid);

	fieldxfunction(customizeentityforclient);

	//Tell the qc library to split the entity fields each side.
	//the fields above become < 0, the remaining fields specified by the qc stay where the mod specified, as far as possible (with addons at least).
	//this means that custom array offsets still work in mods like ktpro.
	if (pr_fixbrokenqccarrays.value)
		PR_RegisterFieldVar(svprogfuncs, 0, NULL, 0,0);
}


#endif
