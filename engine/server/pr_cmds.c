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

#include "quakedef.h"

#ifdef SQL
#include "sv_sql.h"
#endif

#ifndef SQL
#define PF_sqlconnect		PF_Fixme
#define PF_sqldisconnect	PF_Fixme
#define PF_sqlopenquery		PF_Fixme
#define PF_sqlclosequery	PF_Fixme
#define PF_sqlreadfield		PF_Fixme
#define PF_sqlerror			PF_Fixme
#define PF_sqlescape		PF_Fixme
#define PF_sqlversion		PF_Fixme
#define PF_sqlreadfloat		PF_Fixme
#define PF_sqlreadblob		PF_Fixme
#define PF_sqlescapeblob	PF_Fixme
#endif

#ifndef CLIENTONLY

#include "pr_common.h"

//okay, so these are a quick but easy hack
int PR_EnableEBFSBuiltin(const char *name, int binum);
int PR_CSQC_BuiltinValid(const char *name, int num);

/*cvars for the gamecode only*/
cvar_t	nomonsters = CVAR("nomonsters", "0");
cvar_t	gamecfg = CVAR("gamecfg", "0");
cvar_t	scratch1 = CVAR("scratch1", "0");
cvar_t	scratch2 = CVAR("scratch2", "0");
cvar_t	scratch3 = CVAR("scratch3", "0");
cvar_t	scratch4 = CVAR("scratch4", "0");
cvar_t	savedgamecfg = CVARF("savedgamecfg", "0", CVAR_ARCHIVE);
cvar_t	saved1 = CVARF("saved1", "0", CVAR_ARCHIVE);
cvar_t	saved2 = CVARF("saved2", "0", CVAR_ARCHIVE);
cvar_t	saved3 = CVARF("saved3", "0", CVAR_ARCHIVE);
cvar_t	saved4 = CVARF("saved4", "0", CVAR_ARCHIVE);
cvar_t	temp1 = CVARF("temp1", "0", CVAR_ARCHIVE);
cvar_t	noexit = CVAR("noexit", "0");
extern cvar_t sv_specprint;

cvar_t	pr_ssqc_memsize = CVARD("pr_ssqc_memsize", "-1", "The ammount of memory available to the QC vm. This has a theoretical maximum of 1gb, but that value can only really be used in 64bit builds. -1 will attempt to use some conservative default, but you may need to increase it. Consider also clearing pr_fixbrokenqccarrays if you need to change this cvar.");

/*cvars purely for compat with others*/
cvar_t	pr_imitatemvdsv = CVARFD("pr_imitatemvdsv", "0", CVAR_LATCH, "Enables mvdsv-specific builtins, and fakes identifiers so that mods made for mvdsv can run properly and with the full feature set.");

/*compat with frikqcc's arrays (ensures that unknown fields are at the same offsets*/
cvar_t	pr_fixbrokenqccarrays = CVARFD("pr_fixbrokenqccarrays", "0", CVAR_LATCH, "When set, ensures that fields are not relocated unless remapped, working around stripped/immediate field offsets. This results in higher memory usage, but may be needed for compat with certain mods, namely ktpro.");

/*other stuff*/
cvar_t	pr_maxedicts = CVARAFD("pr_maxedicts", "32768", "max_edicts", CVAR_LATCH, "Maximum number of entities spawnable on the map at once. Low values will crash the server on some maps/mods. High values will result in excessive memory useage (see pr_ssqc_memsize). Illegible server messages may occur with old/other clients above 32k. FTE's network protocols have a maximum at a little over 4 million. Please don't ever make a mod that actually uses that many...");

#ifdef NOLEGACY
cvar_t	pr_no_playerphysics = CVARFD("pr_no_playerphysics", "1", CVAR_LATCH, "Prevents support of the 'SV_PlayerPhysics' QC function. This allows servers to prevent needless breakage of player prediction.");
#else
cvar_t	pr_no_playerphysics = CVARFD("pr_no_playerphysics", "0", CVAR_LATCH, "Prevents support of the 'SV_PlayerPhysics' QC function. This allows servers to prevent needless breakage of player prediction.");
#endif
cvar_t	pr_no_parsecommand = CVARFD("pr_no_parsecommand", "0", 0, "Provides a way around invalid mod usage of SV_ParseClientCommand, eg xonotic.");

extern cvar_t pr_sourcedir;
cvar_t	pr_ssqc_progs = CVARAF("progs", "", "sv_progs", CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_NOTFROMSERVER);
cvar_t	pr_nonetaccess = CVARD("pr_nonetaccess", "0", "Block all direct access to network buffers (the writebyte builtin and friends will ignore the call).");	//prevent write_... builtins from doing anything. This means we can run any mod, specific to any engine, on the condition that it also has a qw or nq crc.

cvar_t pr_overridebuiltins = CVAR("pr_overridebuiltins", "1");

cvar_t pr_compatabilitytest = CVARFD("pr_compatabilitytest", "0", CVAR_LATCH, "Only enables builtins if the extension they are part of was queried.");

cvar_t pr_ssqc_coreonerror = CVAR("pr_coreonerror", "1");

cvar_t sv_gameplayfix_honest_tracelines = CVAR("sv_gameplayfix_honest_tracelines", "1");
#ifdef NOLEGACY
cvar_t sv_gameplayfix_setmodelrealbox = CVARD("sv_gameplayfix_setmodelrealbox", "1", "Vanilla setmodel will setsize the entity to a hardcoded size for non-bsp models. This cvar will always use the real size of the model instead, but will require that the server actually loads the model.");
#else
cvar_t sv_gameplayfix_setmodelrealbox = CVARD("sv_gameplayfix_setmodelrealbox", "0", "Vanilla setmodel will setsize the entity to a hardcoded size for non-bsp models. This cvar will always use the real size of the model instead, but will require that the server actually loads the model.");
#endif
cvar_t sv_gameplayfix_setmodelsize_qw = CVARD("sv_gameplayfix_setmodelsize_qw", "0", "The setmodel builtin will act as a setsize for QuakeWorld mods also.");
cvar_t dpcompat_nopreparse = CVARD("dpcompat_nopreparse", "0", "Xonotic uses svc_tempentity with unknowable lengths mixed with other data that needs to be translated. This cvar disables any attempt to translate or pre-parse network messages, including disabling nq/qw cross compatibility. NOTE: because preparsing will be disabled, messages might not get backbuffered correctly if too much reliable data is written.");
extern cvar_t sv_listen_dp;

cvar_t sv_addon[MAXADDONS];
char cvargroup_progs[] = "Progs variables";

evalc_t evalc_idealpitch, evalc_pitch_speed;

qboolean ssqc_deprecated_warned;
int pr_teamfield;
unsigned int h2infoplaque[2];	/*hexen2 stat*/

static void PRSV_ClearThreads(void);
void PR_fclose_progs(pubprogfuncs_t*);
void PF_InitTempStrings(pubprogfuncs_t *prinst);
static int PDECL PR_SSQC_MapNamedBuiltin(pubprogfuncs_t *progfuncs, int headercrc, const char *builtinname);
static void QCBUILTIN PF_Fixme (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals);

void PR_DumpPlatform_f(void);

typedef struct qcstate_s
{
	float resumetime;
	qboolean waiting;
	struct qcthread_s *thread;
	int self;
	int selfid;
	int other;
	int otherid;
	float returnval;

	struct qcstate_s *next;
} qcstate_t;
qcstate_t *qcthreads;

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

	char *prototype;
	char *biglongdesc;
	qboolean obsolete;
} BuiltinList_t;
builtin_t pr_builtin[1024];

struct {
	func_t ChatMessage;	//mvdsv parsing of 'say' commands
	func_t UserCmd;	//mvdsv
	func_t ConsoleCmd; //mvdsv
	func_t UserInfo_Changed;
	func_t localinfoChanged;

	func_t ParseClusterEvent;	//FTE_SV_CLUSTER
	func_t ParseClientCommand;	//KRIMZON_SV_PARSECLIENTCOMMAND
	func_t ParseConnectionlessPacket;	//FTE_QC_SENDPACKET

	func_t PausedTic;
	func_t ShouldPause;

	func_t RunClientCommand;	//EXT_CSQC_1

#ifdef HEXEN2
	func_t ClassChangeWeapon;//hexen2 support
#endif
	func_t AddDebugPolygons;
	func_t CheckRejectConnection;
} gfuncs;
func_t SpectatorConnect;	//QW
func_t SpectatorThink;	//QW
func_t SpectatorDisconnect;	//QW

func_t SV_PlayerPhysicsQC;	//DP's DP_SV_PLAYERPHYSICS extension
func_t EndFrameQC;	//a common extension

globalptrs_t realpr_global_ptrs;
globalptrs_t *pr_global_ptrs = &realpr_global_ptrs;

pubprogfuncs_t *svprogfuncs;
progparms_t svprogparms;

progstype_t progstype;

void PR_RegisterFields(void);
void PR_ResetBuiltins(progstype_t type);

static qcstate_t *PR_CreateThread(pubprogfuncs_t *prinst, float retval, float resumetime, qboolean wait)
{
	qcstate_t *state;

	state = prinst->parms->memalloc(sizeof(qcstate_t));
	state->next = qcthreads;
	qcthreads = state;
	state->resumetime = resumetime;
	state->self = NUM_FOR_EDICT(prinst, PROG_TO_EDICT(prinst, pr_global_struct->self));
	state->selfid = PROG_TO_EDICT(prinst, state->self)->xv->uniquespawnid;
	state->other = NUM_FOR_EDICT(prinst, PROG_TO_EDICT(prinst, pr_global_struct->other));
	state->otherid = PROG_TO_EDICT(prinst, state->other)->xv->uniquespawnid;
	state->thread = prinst->Fork(prinst);
	state->waiting = wait;
	state->returnval = retval;
	return state;
}

void PDECL ED_Spawned (struct edict_s *ent, int loading)
{
#ifdef VM_Q1
	if (!ent->xv)
		ent->xv = (extentvars_t *)(ent->v+1);
#endif

	if (!loading || !ent->xv->uniquespawnid)
	{
		ent->xv->dimension_see = pr_global_struct->dimension_default;
		ent->xv->dimension_seen = pr_global_struct->dimension_default;
		ent->xv->dimension_ghost = 0;
		ent->xv->dimension_solid = pr_global_struct->dimension_default;
		ent->xv->dimension_hit = pr_global_struct->dimension_default;
#ifdef HEXEN2
		if (progstype != PROG_H2)
			ent->xv->drawflags = SCALE_ORIGIN_ORIGIN;	//if not running hexen2, default the scale origin to the actual origin.
#endif

#ifndef NOLEGACY
		ent->xv->Version = sv.csqcentversion[ent->entnum];
#endif
		ent->xv->uniquespawnid = sv.csqcentversion[ent->entnum];

		if (!ent->baseline.number)
		{	//make sure it has a valid baseline
			extern entity_state_t nullentitystate;
			memcpy(&ent->baseline, &nullentitystate, sizeof(ent->baseline));
			ent->baseline.number = ent->entnum;
		}
	}
}

pbool PDECL ED_CanFree (edict_t *ed)
{
	if (ed == (edict_t*)sv.world.edicts)
	{
		if (developer.value)
			PR_RunWarning(svprogfuncs, "cannot free world entity\n");
		return false;
	}
	if (NUM_FOR_EDICT(svprogfuncs, ed) <= sv.allocated_client_slots)
	{
		PR_RunWarning(svprogfuncs, "cannot free player entities\n");
		return false;
	}
	World_UnlinkEdict ((wedict_t*)ed);		// unlink from world bsp

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
	ed->xv->pvsflags = 0;

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
	sv.csqcentversion[ed->entnum] += 1;

#ifdef USERBE
	if (sv.world.rbe)
	{
		sv.world.rbe->RemoveFromEntity(&sv.world, (wedict_t*)ed);
		sv.world.rbe->RemoveJointFromEntity(&sv.world, (wedict_t*)ed);
	}
#endif


	return true;
}

static void ASMCALL StateOp (pubprogfuncs_t *prinst, float var, func_t func)
{
	stdentvars_t *vars = PROG_TO_EDICT(prinst, pr_global_struct->self)->v;
	if (progstype == PROG_H2)
		vars->nextthink = pr_global_struct->time+0.05;
	else
		vars->nextthink = pr_global_struct->time+0.1;
	vars->think = func;
	vars->frame = var;
}
#ifdef HEXEN2
static void ASMCALL CStateOp (pubprogfuncs_t *prinst, float first, float last, func_t currentfunc)
{
	float min, max;
	float step;
	wedict_t *e = PROG_TO_WEDICT(prinst, pr_global_struct->self);
	float frame = e->v->frame;

	if (progstype == PROG_H2)
		e->v->nextthink = pr_global_struct->time+0.05;
	else
		e->v->nextthink = pr_global_struct->time+0.1;
	e->v->think = currentfunc;

	if (pr_global_ptrs->cycle_wrapped)
		pr_global_struct->cycle_wrapped = false;

	if (first > last)
	{	//going backwards
		min = last;
		max = first;
		step = -1.0;
	}
	else
	{	//forwards
		min = first;
		max = last;
		step = 1.0;
	}
	if (frame < min || frame > max)
		frame = first;	//started out of range, must have been a different animation
	else
	{
		frame += step;
		if (frame < min || frame > max)
		{	//became out of range, must have wrapped
			if (pr_global_ptrs->cycle_wrapped)
				pr_global_struct->cycle_wrapped = true;
			frame = first;
		}
	}
	e->v->frame = frame;
}
static void ASMCALL CWStateOp (pubprogfuncs_t *prinst, float first, float last, func_t currentfunc)
{
	float min, max;
	float step;
	wedict_t *e = PROG_TO_WEDICT(prinst, pr_global_struct->self);
	float frame = e->v->weaponframe;

	if (progstype == PROG_H2)
		e->v->nextthink = pr_global_struct->time+0.05;
	else
		e->v->nextthink = pr_global_struct->time+0.1;
	e->v->think = currentfunc;

	if (pr_global_ptrs->cycle_wrapped)
		pr_global_struct->cycle_wrapped = false;

	if (first > last)
	{	//going backwards
		min = last;
		max = first;
		step = -1.0;
	}
	else
	{	//forwards
		min = first;
		max = last;
		step = 1.0;
	}
	if (frame < min || frame > max)
		frame = first;	//started out of range, must have been a different animation
	else
	{
		frame += step;
		if (frame < min || frame > max)
		{	//became out of range, must have wrapped
			if (pr_global_ptrs->cycle_wrapped)
				pr_global_struct->cycle_wrapped = true;
			frame = first;
		}
	}
	e->v->weaponframe = frame;
}

static void ASMCALL ThinkTimeOp (pubprogfuncs_t *prinst, edict_t *ed, float var)
{
	stdentvars_t *vars = ed->v;
	vars->nextthink = pr_global_struct->time+var;
}
#endif

static int SV_ParticlePrecache_Add(const char *pname);
static pbool PDECL SV_BadField(pubprogfuncs_t *inst, edict_t *foo, const char *keyname, const char *value)
{
#ifdef HEXEN2
	/*Worldspawn only fields...*/
	if (NUM_FOR_EDICT(inst, foo) == 0)
	{
		/*hexen2 midi - just mute it, we don't support it*/
		if (!stricmp(keyname, "MIDI"))
		{
			Q_strncpyz(sv.h2miditrack, value, sizeof(sv.h2miditrack));
			return true;
		}
		/*hexen2 does cd tracks slightly differently too, so there's consistancy for you*/
		if (!stricmp(keyname, "CD"))
		{
			sv.h2cdtrack = atoi(value);
			return true;
		}
	}
#endif

	if (!strcmp(keyname, "traileffect"))
	{
		foo->xv->traileffectnum = SV_ParticlePrecache_Add(value);
		return true;
	}
	if (!strcmp(keyname, "emiteffect"))
	{
		foo->xv->emiteffectnum = SV_ParticlePrecache_Add(value);
		return true;
	}

	if (!strcmp(keyname, "sky") || !strcmp(keyname, "fog"))
		return true;	//these things are handled in the client, so don't warn if they're used.

	//don't spam warnings about missing fields if we failed to load the progs.
	if (!svs.numprogs)
		return true;

	return false;
}

void PR_SV_FillWorldGlobals(world_t *w)
{
	w->g.self = pr_global_ptrs->self;
	w->g.other = pr_global_ptrs->other;
	w->g.force_retouch = pr_global_ptrs->force_retouch;
	w->g.physics_mode = pr_global_ptrs->physics_mode;
	w->g.frametime = pr_global_ptrs->frametime;
	w->g.newmis = pr_global_ptrs->newmis;
	w->g.time = pr_global_ptrs->time;
	w->g.v_forward = *pr_global_ptrs->v_forward;
	w->g.v_right = *pr_global_ptrs->v_right;
	w->g.v_up = *pr_global_ptrs->v_up;
	w->g.defaultgravitydir = *pr_global_ptrs->global_gravitydir;
}

static void PDECL PR_SSQC_Relocated(pubprogfuncs_t *pr, char *oldb, char *newb, int oldlen)
{
#ifdef VM_Q1
	edict_t *ent;
#endif
	int i;
	union {
		globalptrs_t *g;
		char **c;
	} b;
	b.g = pr_global_ptrs;
	for (i = 0; i < sizeof(*b.g)/sizeof(*b.c); i++)
	{
		if (b.c[i] >= oldb && b.c[i] < oldb+oldlen)
			b.c[i] += newb - oldb;
	}
	PR_SV_FillWorldGlobals(&sv.world);

#ifdef VM_Q1
	for (i = 0; i < sv.world.num_edicts; i++)
	{
		ent = EDICT_NUM(pr, i);
		if ((char*)ent->xv >= oldb && (char*)ent->xv < oldb+oldlen)
			ent->xv = (extentvars_t*)((char*)ent->xv - oldb + newb);
	}
#endif

	for (i = 0; sv.strings.model_precache[i]; i++)
	{
		if (sv.strings.model_precache[i] >= oldb && sv.strings.model_precache[i] < oldb+oldlen)
			sv.strings.model_precache[i] += newb - oldb;
	}
	for (i = 0; i < svs.allocated_client_slots; i++)
	{
		if (svs.clients[i].name >= oldb && svs.clients[i].name < oldb+oldlen)
			svs.clients[i].name += newb - oldb;
		if (svs.clients[i].team >= oldb && svs.clients[i].team < oldb+oldlen)
			svs.clients[i].team += newb - oldb;
	}
}

//int QCEditor (char *filename, int line, int nump, char **parms);
void QC_Clear(void);
builtin_t pr_builtin[];
extern int pr_numbuiltins;

model_t *QDECL SVPR_GetCModel(world_t *w, int modelindex)
{
	if ((unsigned int)modelindex < MAX_PRECACHE_MODELS)
	{
		model_t *mod;
		if (!sv.models[modelindex] && sv.strings.model_precache[modelindex])
			sv.models[modelindex] = Mod_ForName(Mod_FixName(sv.strings.model_precache[modelindex], sv.modelname), MLV_WARN);
		mod = sv.models[modelindex];
		if (mod && mod->loadstate != MLS_LOADED)
		{
			if (mod->loadstate == MLS_LOADING)
				COM_WorkerPartialSync(mod, &mod->loadstate, MLS_LOADING);
			if (mod->loadstate != MLS_LOADED)
				mod = NULL;	//gah, it failed!
		}
		return mod;
	}
	else
		return NULL;
}
static void QDECL SVPR_Get_FrameState(world_t *w, wedict_t *ent, framestate_t *fstate)
{
	memset(fstate, 0, sizeof(*fstate));
	fstate->g[FS_REG].frame[0] = ent->v->frame;
	fstate->g[FS_REG].frametime[0] = ent->xv->frame1time;
	fstate->g[FS_REG].lerpweight[0] = 1;
	fstate->g[FS_REG].endbone = 0x7fffffff;

	fstate->g[FST_BASE].frame[0] = ent->xv->baseframe;
	fstate->g[FST_BASE].frametime[0] = ent->xv->/*base*/frame1time;
	fstate->g[FST_BASE].lerpweight[0] = 1;
	fstate->g[FST_BASE].endbone = ent->xv->basebone;

#if defined(SKELETALOBJECTS) || defined(RAGDOLL)
	if (ent->xv->skeletonindex)
		skel_lookup(w, ent->xv->skeletonindex, fstate);
#endif
}

static void QDECL SVPR_Event_Touch(world_t *w, wedict_t *s, wedict_t *o)
{
	int oself = pr_global_struct->self;
	int oother = pr_global_struct->other;

	pr_global_struct->self = EDICT_TO_PROG(w->progs, s);
	pr_global_struct->other = EDICT_TO_PROG(w->progs, o);
	pr_global_struct->time = w->physicstime;
	PR_ExecuteProgram (w->progs, s->v->touch);

	pr_global_struct->self = oself;
	pr_global_struct->other = oother;
}

static void QDECL SVPR_Event_Think(world_t *w, wedict_t *s)
{
	pr_global_struct->self = EDICT_TO_PROG(w->progs, s);
	pr_global_struct->other = EDICT_TO_PROG(w->progs, w->edicts);
	if (!s->v->think)
		Con_Printf("SSQC entity \"%s\" has nextthink with no think function\n", PR_GetString(w->progs, s->v->classname));
	else
		PR_ExecuteProgram (w->progs, s->v->think);
}

static qboolean QDECL SVPR_Event_ContentsTransition(world_t *w, wedict_t *ent, int oldwatertype, int newwatertype)
{
	if (ent->xv->contentstransition)
	{
		void *pr_globals = PR_globals(w->progs, PR_CURRENT);
		pr_global_struct->self = EDICT_TO_PROG(w->progs, ent);
		pr_global_struct->time = w->physicstime;
		G_FLOAT(OFS_PARM0) = oldwatertype;
		G_FLOAT(OFS_PARM1) = newwatertype;
		PR_ExecuteProgram (w->progs, ent->xv->contentstransition);
		return true;
	}
	return false;	//do legacy behaviour
}

#define QW_PROGHEADER_CRC	54730
#define NQ_PROGHEADER_CRC	5927
#define PREREL_PROGHEADER_CRC	26940	//prerelease
#define TENEBRAE_PROGHEADER_CRC	32401	//tenebrae
#define H2_PROGHEADER_CRC	38488	//basic hexen2
#define H2MP_PROGHEADER_CRC	26905	//hexen2 mission pack uses slightly different defs... *sigh*...
#define H2DEMO_PROGHEADER_CRC	14046	//I'm guessing this is from the original release or something

pbool PDECL PR_SSQC_CheckHeaderCrc(pubprogfuncs_t *inst, progsnum_t idx, int crc)
{
	progstype_t modtype;
	if (crc == QW_PROGHEADER_CRC)
		modtype = PROG_QW;
	else if (crc == NQ_PROGHEADER_CRC)
		modtype = PROG_NQ;
#ifdef HEXEN2
	else if (crc == H2_PROGHEADER_CRC || crc == H2MP_PROGHEADER_CRC || crc == H2DEMO_PROGHEADER_CRC)
		modtype = PROG_H2;
#endif
	else if (crc == PREREL_PROGHEADER_CRC)
		modtype = PROG_PREREL;
	else if (crc == TENEBRAE_PROGHEADER_CRC)
		modtype = PROG_TENEBRAE;
	else
		modtype = PROG_UNKNOWN;

	//if we didn't see one yet, use the one that just got loaded.
	if (progstype == PROG_NONE)
		progstype = modtype;
	//if the new one differs from the main module, reject it, unless it has crc 0, which we'll allow as a universal mutator (good luck guessing the correct arguments, but hey).
	if (progstype != modtype && crc != 0)
		return false;
	return true;
}
void Q_SetProgsParms(qboolean forcompiler)
{
	progstype = PROG_NONE;
	svprogparms.progsversion = PROGSTRUCT_VERSION;
	svprogparms.ReadFile = COM_LoadStackFile;//char *(*ReadFile) (char *fname, void *buffer, int *len);
	svprogparms.FileSize = COM_FileSize;//int (*FileSize) (char *fname);	//-1 if file does not exist
	svprogparms.WriteFile = QC_WriteFile;//bool (*WriteFile) (char *name, void *data, int len);
	svprogparms.Printf = PR_Printf;//Con_Printf;//void (*printf) (char *, ...);
	svprogparms.DPrintf = PR_DPrintf;//Con_Printf;//void (*printf) (char *, ...);
	svprogparms.CheckHeaderCrc = PR_SSQC_CheckHeaderCrc;
	svprogparms.Sys_Error = Sys_Error;
	svprogparms.Abort = SV_Error;
	svprogparms.edictsize = sizeof(edict_t);

	svprogparms.entspawn = ED_Spawned;//void (*entspawn) (struct edict_s *ent);	//ent has been spawned, but may not have all the extra variables (that may need to be set) set
	svprogparms.entcanfree = ED_CanFree;//bool (*entcanfree) (struct edict_s *ent);	//return true to stop ent from being freed
	svprogparms.stateop = StateOp;//void (*stateop) (float var, func_t func);
#ifdef HEXEN2
	svprogparms.cstateop = CStateOp;
	svprogparms.cwstateop = CWStateOp;
	svprogparms.thinktimeop = ThinkTimeOp;
#endif

	svprogparms.MapNamedBuiltin = PR_SSQC_MapNamedBuiltin;
	svprogparms.loadcompleate = NULL;//void (*loadcompleate) (int edictsize);	//notification to reset any pointers.
	svprogparms.badfield = SV_BadField;

	svprogparms.memalloc = PR_CB_Malloc;//void *(*memalloc) (int size);	//small string allocation	malloced and freed randomly
	svprogparms.memfree = PR_CB_Free;//void (*memfree) (void * mem);


	svprogparms.globalbuiltins = pr_builtin;//builtin_t *globalbuiltins;	//these are available to all progs
	svprogparms.numglobalbuiltins = pr_numbuiltins;

	svprogparms.autocompile = PR_COMPILEIGNORE;//PR_COMPILECHANGED;//enum {PR_NOCOMPILE, PR_COMPILENEXIST, PR_COMPILECHANGED, PR_COMPILEALWAYS} autocompile;

	svprogparms.gametime = &sv.time;

	svprogparms.sv_edicts = (edict_t**)&sv.world.edicts;
	svprogparms.sv_num_edicts = &sv.world.num_edicts;

	svprogparms.useeditor = QCEditor;

	//until its properly tested
	if (pr_ssqc_memsize.ival == -2)
		svprogparms.addressablerelocated = PR_SSQC_Relocated;

	svprogparms.user = &sv.world;
	if (!svprogfuncs)
	{
		sv.world.progs = svprogfuncs = InitProgs(&svprogparms);
	}
	sv.world.Event_Touch = SVPR_Event_Touch;
	sv.world.Event_Think = SVPR_Event_Think;
	sv.world.Event_Sound = SVQ1_StartSound;
	sv.world.Event_ContentsTransition = SVPR_Event_ContentsTransition;
	sv.world.Get_CModel = SVPR_GetCModel;
	sv.world.Get_FrameState = SVPR_Get_FrameState;
	PRSV_ClearThreads();
	PR_fclose_progs(svprogfuncs);

//	svs.numprogs = 0;

}

void PR_Deinit(void)
{
	int i;

	PRSV_ClearThreads();
#ifdef VM_Q1
	Q1QVM_Shutdown();
#endif
	if (svprogfuncs)
	{
		for (i = 0; i < svs.allocated_client_slots; i++)
		{
			if (svs.clients[i].name != svs.clients[i].namebuf)
				Q_strncpyz(svs.clients[i].namebuf, svs.clients[i].name, sizeof(svs.clients[i].namebuf));
			svs.clients[i].name = svs.clients[i].namebuf;
			svs.clients[i].team = svs.clients[i].teambuf;
		}

		PR_Common_Shutdown(svprogfuncs, false);
		World_Destroy(&sv.world);
		if (svprogfuncs->CloseProgs)
			svprogfuncs->CloseProgs(svprogfuncs);
		sv.world.progs = NULL;
		svprogfuncs=NULL;

		for (i = 0; i < MAX_LIGHTSTYLES; i++)
		{
			BZ_Free((void*)sv.strings.lightstyles[i]);
			sv.strings.lightstyles[i] = NULL;
		}
	}

	World_Destroy(&sv.world);

#ifdef SQL
	SQL_KillServers();
#endif

	//clear out function pointers (so changing game modes cannot lead to confusions)
	memset(&gfuncs, 0, sizeof(gfuncs));
	SpectatorConnect = 0;
	SpectatorThink = 0;
	SpectatorDisconnect = 0;
}

void PR_Shutdown(void)
{
	PR_Deinit();
#ifdef SQL
	SQL_DeInit();
#endif
}

void PR_LoadGlabalStruct(qboolean muted)
{
	static float svphysicsmode = 2;
	static float writeonly;
	static int writeonly_int;
	static float dimension_send_default;
	static float dimension_default = 255;
	static float zero_default;
	static float input_buttons_default;
	static float input_timelength_default;
	static float input_impulse_default;
	static vec3_t input_angles_default;
	static vec3_t input_movevalues_default;
	static vec3_t global_gravitydir_default = {0,0,-1};
	int i;
	int *v;
	globalptrs_t *pr_globals = pr_global_ptrs;
#define globalfloat(need,name)	(pr_globals)->name = (float *)PR_FindGlobal(svprogfuncs, #name, 0, NULL);	if (need && !(pr_globals)->name)	{static float fallback##name; (pr_globals)->name = &fallback##name; if (!muted) Con_DPrintf("Could not find \""#name"\" export in progs\n");}
#define globalint(need,name)	(pr_globals)->name = (int *)PR_FindGlobal(svprogfuncs, #name, 0, NULL);		if (need && !(pr_globals)->name)	{static int fallback##name; (pr_globals)->name = &fallback##name; if (!muted) Con_DPrintf("Could not find \""#name"\" export in progs\n");}
#define globalstring(need,name)	(pr_globals)->name = (int *)PR_FindGlobal(svprogfuncs, #name, 0, NULL);		if (need && !(pr_globals)->name)	{static string_t fallback##name; (pr_globals)->name = &fallback##name; if (!muted) Con_DPrintf("Could not find \""#name"\" export in progs\n");}
#define globalvec(need,name)	(pr_globals)->name = (vec3_t *)PR_FindGlobal(svprogfuncs, #name, 0, NULL);	if (need && !(pr_globals)->name)	{static vec3_t fallback##name; (pr_globals)->name = &fallback##name; if (!muted) Con_DPrintf("Could not find \""#name"\" export in progs\n");}
#define globalfunc(need,name)	(pr_globals)->name = (func_t *)PR_FindGlobal(svprogfuncs, #name, 0, NULL);	if (!(pr_globals)->name)			{static func_t stripped##name; stripped##name = PR_FindFunction(svprogfuncs, #name, 0); if (stripped##name) (pr_globals)->name = &stripped##name; else if (need && !muted) Con_DPrintf("Could not find function \""#name"\" in progs\n"); }
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
	globalfloat		(false, serverflags);
	globalfloat		(false, total_secrets);
	globalfloat		(false, total_monsters);
	globalfloat		(false, found_secrets);
	globalfloat		(false, killed_monsters);
	globalvec		(true, v_forward);
	globalvec		(true, v_up);
	globalvec		(true, v_right);
	globalfloat		(false, trace_allsolid);
	globalfloat		(false, trace_startsolid);
	globalfloat		(false, trace_fraction);
	globalvec		(false, trace_endpos);
	globalvec		(false, trace_plane_normal);
	globalfloat		(false, trace_plane_dist);
	globalint		(true, trace_ent);
	globalfloat		(false, trace_inopen);
	globalfloat		(false, trace_inwater);
	globalfloat		(false, trace_endcontentsf);
	globalint		(false, trace_endcontentsi);
	globalfloat		(false, trace_surfaceflagsf);
	globalint		(false, trace_surfaceflagsi);
	globalstring	(false, trace_surfacename);
	globalint		(false, trace_brush_id);
	globalint		(false, trace_brush_faceid);
	globalint		(false, trace_surface_id);
	globalint		(false, trace_bone_id);
	globalint		(false, trace_triangle_id);
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
	globalfloat		(false, dimension_default);


	globalfloat		(false, clientcommandframe);
	globalfloat		(false, input_timelength);
	globalfloat		(false, input_impulse);
	globalvec		(false, input_angles);
	globalvec		(false, input_movevalues);
	globalfloat		(false, input_buttons);
	globalint		(false, serverid);
	globalvec		(false, global_gravitydir);

	memset(&evalc_idealpitch, 0, sizeof(evalc_idealpitch));
	memset(&evalc_pitch_speed, 0, sizeof(evalc_pitch_speed));

	if (pr_global_ptrs->serverid)
		*pr_global_ptrs->serverid = svs.clusterserverid;
	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		pr_global_ptrs->spawnparamglobals[i] = (float *)PR_FindGlobal(svprogfuncs, va("parm%i", i+1), 0, NULL);

#define ensureglobal(name,var) if (!(pr_globals)->name) (pr_globals)->name = &var;

	if (!(pr_globals)->trace_surfaceflagsf && !(pr_globals)->trace_surfaceflagsi)
	{
		etype_t etype;
		eval_t *v = PR_FindGlobal(svprogfuncs, "trace_surfaceflags", 0, &etype);
		if (etype == ev_float)
			(pr_globals)->trace_surfaceflagsf = (float*)v;
		else if (etype == ev_integer)
			(pr_globals)->trace_surfaceflagsi = (int*)v;
	}
	if (!(pr_globals)->trace_endcontentsf && !(pr_globals)->trace_endcontentsi)
	{
		etype_t etype;
		eval_t *v = PR_FindGlobal(svprogfuncs, "trace_endcontents", 0, &etype);
		if (etype == ev_float)
			(pr_globals)->trace_endcontentsf = (float*)v;
		else if (etype == ev_integer)
			(pr_globals)->trace_endcontentsi = (int*)v;
	}

	// make sure these entries are always valid pointers
	ensureglobal(dimension_send, dimension_send_default);
	ensureglobal(dimension_default, dimension_default);
	ensureglobal(trace_endcontentsf, writeonly);
	ensureglobal(trace_endcontentsi, writeonly_int);
	ensureglobal(trace_surfaceflagsf, writeonly);
	ensureglobal(trace_surfaceflagsi, writeonly_int);
	ensureglobal(trace_brush_id, writeonly_int);
	ensureglobal(trace_brush_faceid, writeonly_int);
	ensureglobal(trace_surface_id, writeonly_int);
	ensureglobal(trace_bone_id, writeonly_int);
	ensureglobal(trace_triangle_id, writeonly_int);

	ensureglobal(input_timelength, input_timelength_default);
	ensureglobal(input_impulse, input_impulse_default);
	ensureglobal(input_angles, input_angles_default);
	ensureglobal(input_movevalues, input_movevalues_default);
	ensureglobal(input_buttons, input_buttons_default);
	ensureglobal(global_gravitydir, global_gravitydir_default);

	// qtest renames and missing variables
	if (!(pr_globals)->trace_plane_normal)
	{
		(pr_globals)->trace_plane_normal = (vec3_t *)PR_FindGlobal(svprogfuncs, "trace_normal", 0, NULL);
		if (!(pr_globals)->trace_plane_normal)
		{
			static vec3_t fallback_trace_plane_normal;
			(pr_globals)->trace_plane_normal = &fallback_trace_plane_normal;
			if (!muted)
				Con_DPrintf("Could not find trace_plane_normal export in progs\n");
		}
	}
	if (!(pr_globals)->trace_endpos)
	{
		(pr_globals)->trace_endpos = (vec3_t *)PR_FindGlobal(svprogfuncs, "trace_impact", 0, NULL);
		if (!(pr_globals)->trace_endpos)
		{
			static vec3_t fallback_trace_endpos;
			(pr_globals)->trace_endpos = &fallback_trace_endpos;
			if (!muted)
				Con_DPrintf("Could not find trace_endpos export in progs\n");
		}
	}
	if (!(pr_globals)->trace_fraction)
	{
		(pr_globals)->trace_fraction = (float *)PR_FindGlobal(svprogfuncs, "trace_frac", 0, NULL);
		if (!(pr_globals)->trace_fraction)
		{
			static float fallback_trace_fraction;
			(pr_globals)->trace_fraction = &fallback_trace_fraction;
			if (!muted)
				Con_DPrintf("Could not find trace_fraction export in progs\n");
		}
	}
	ensureglobal(serverflags, zero_default);
	ensureglobal(total_secrets, zero_default);
	ensureglobal(total_monsters, zero_default);
	ensureglobal(found_secrets, zero_default);
	ensureglobal(killed_monsters, zero_default);
	ensureglobal(trace_allsolid, writeonly);
	ensureglobal(trace_startsolid, writeonly);
	ensureglobal(trace_plane_dist, writeonly);
	ensureglobal(trace_inopen, writeonly);
	ensureglobal(trace_inwater, writeonly);
	ensureglobal(physics_mode, svphysicsmode);

	//this can be a map start or a loadgame. don't hurt stuff.
	if (!pr_global_struct->dimension_send)
		pr_global_struct->dimension_send = pr_global_struct->dimension_default;
/*
	pr_global_struct->serverflags = 0;
	pr_global_struct->total_secrets = 0;
	pr_global_struct->total_monsters = 0;
	pr_global_struct->found_secrets = 0;
	pr_global_struct->killed_monsters = 0;
*/
	pr_teamfield = 0;

	SpectatorConnect = PR_FindFunction(svprogfuncs, "SpectatorConnect", PR_ANY);
	SpectatorDisconnect = PR_FindFunction(svprogfuncs, "SpectatorDisconnect", PR_ANY);
	SpectatorThink = PR_FindFunction(svprogfuncs, "SpectatorThink", PR_ANY);

	gfuncs.ParseClusterEvent = PR_FindFunction(svprogfuncs, "SV_ParseClusterEvent", PR_ANY);
	gfuncs.ParseClientCommand = PR_FindFunction(svprogfuncs, "SV_ParseClientCommand", PR_ANY);
	gfuncs.ParseConnectionlessPacket = PR_FindFunction(svprogfuncs, "SV_ParseConnectionlessPacket", PR_ANY);

	gfuncs.UserInfo_Changed = PR_FindFunction(svprogfuncs, "UserInfo_Changed", PR_ANY);
	gfuncs.localinfoChanged = PR_FindFunction(svprogfuncs, "localinfoChanged", PR_ANY);
	gfuncs.ChatMessage = PR_FindFunction(svprogfuncs, "ChatMessage", PR_ANY);
	gfuncs.UserCmd = PR_FindFunction(svprogfuncs, "UserCmd", PR_ANY);
	gfuncs.ConsoleCmd = PR_FindFunction(svprogfuncs, "ConsoleCmd", PR_ANY);

	gfuncs.PausedTic = PR_FindFunction(svprogfuncs, "SV_PausedTic", PR_ANY);
	gfuncs.ShouldPause = PR_FindFunction(svprogfuncs, "SV_ShouldPause", PR_ANY);
#ifdef HEXEN2
	gfuncs.ClassChangeWeapon = PR_FindFunction(svprogfuncs, "ClassChangeWeapon", PR_ANY);
#endif
	gfuncs.RunClientCommand = PR_FindFunction(svprogfuncs, "SV_RunClientCommand", PR_ANY);
	gfuncs.AddDebugPolygons = PR_FindFunction(svprogfuncs, "SV_AddDebugPolygons", PR_ANY);
	gfuncs.CheckRejectConnection = PR_FindFunction(svprogfuncs, "SV_CheckRejectConnection", PR_ANY);

	if (pr_no_playerphysics.ival)
		SV_PlayerPhysicsQC = 0;
	else
		SV_PlayerPhysicsQC = PR_FindFunction(svprogfuncs, "SV_PlayerPhysics", PR_ANY);
	EndFrameQC = PR_FindFunction (svprogfuncs, "EndFrame", PR_ANY);

	v = (int *)PR_globals(svprogfuncs, PR_CURRENT);
	svprogfuncs->AddSharedVar(svprogfuncs, (int *)(pr_global_ptrs)->self-v, 1);
	svprogfuncs->AddSharedVar(svprogfuncs, (int *)(pr_global_ptrs)->other-v, 1);
	svprogfuncs->AddSharedVar(svprogfuncs, (int *)(pr_global_ptrs)->time-v, 1);

	//test the global rather than the field - fte internally always has the field.
	sv.haveitems2 = !!PR_FindGlobal(svprogfuncs, "items2", 0, NULL);

	SV_ClearQCStats();

	PR_SV_FillWorldGlobals(&sv.world);

#ifdef HEXEN2
	/*Hexen2 has lots of extra stats, which I don't want special support for, so list them here and send them as for csqc*/
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
		SV_QCStatName(ev_float, "movetype", STAT_H2_MOVETYPE);		/*normally used to change the roll when flying*/
		SV_QCStatName(ev_entity, "cameramode", STAT_H2_CAMERAMODE);	/*locks view in place when set*/
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
		SV_QCStatName(ev_float, "flags", STAT_H2_FLAGS);				/*to show the special abilities on the sbar*/

		SV_QCStatPtr(ev_integer, &h2infoplaque[0], STAT_H2_OBJECTIVE1);
		SV_QCStatPtr(ev_integer, &h2infoplaque[1], STAT_H2_OBJECTIVE2);
	}
#endif
}

progsnum_t AddProgs(const char *name)
{
	float fl;
	func_t f;
	globalvars_t *pr_globals;
	progsnum_t num;
	flocation_t loc;
	char gamedir[MAX_QPATH];
	int i;
	if (strlen(name) >= sizeof(svs.progsnames[0]))
		return -1;

	for (i = 0; i < svs.numprogs; i++)
	{
		if (!strcmp(svs.progsnames[i], name))
		{
			return svs.progsnum[i];
		}
	}

	if (svs.numprogs >= MAX_PROGS)
		return -1;

//	svprogparms.autocompile = PR_COMPILECHANGED;

	num = PR_LoadProgs (svprogfuncs, name);
	sv.world.usesolidcorpse = (progstype != PROG_H2);
	if (!i && num != -1)
	{
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
			Con_DPrintf("Using unknown progs\n");
			break;
		}
	}
//	svprogparms.autocompile = PR_COMPILECHANGED;
	if (num == -1)
	{
		Con_Printf("Failed to load %s\n", name);
		return -1;
	}

	if (num == 0)
		PR_LoadGlabalStruct(false);

	*gamedir = 0;
	if (FS_FLocateFile(name, FSLF_IFFOUND, &loc))
	{
		Q_strncpyz(gamedir, loc.search->purepath, sizeof(gamedir));
		*COM_SkipPath(gamedir) = 0;
	}
	Con_TPrintf("Loaded progs %s%s\n", gamedir, name);

	PR_ProgsAdded(svprogfuncs, num, name);

	if (!svs.numprogs)
	{
		PF_InitTempStrings(svprogfuncs);
		PR_ResetBuiltins(progstype);
	}

	if ((f = PR_FindFunction (svprogfuncs, "VersionChat", num )))
	{
		pr_globals = PR_globals(svprogfuncs, num);
		G_FLOAT(OFS_PARM0) = version_number();
		PR_ExecuteProgram (svprogfuncs, f);

		fl = G_FLOAT(OFS_RETURN);
		if (fl < 0)
			SV_Error ("PR_LoadProgs: progs.dat is not compatible with EXE version");
		else if ((int) (fl) != (int) (version_number()))
			Con_DPrintf("Warning: Progs may not be fully compatible\n (%4.2f != %i)\n", fl, version_number());
	}

	if ((f = PR_FindFunction (svprogfuncs, "FTE_init", num )))
	{
		pr_globals = PR_globals(svprogfuncs, num);
		G_FLOAT(OFS_PARM0) = version_number();
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
		PR_Configure(svprogfuncs, pr_ssqc_memsize.ival, MAX_PROGS, 0);
	}


	if (Cmd_Argc() == 1)
		svprogfuncs->Decompile(svprogfuncs, "qwprogs.dat");
	else
		svprogfuncs->Decompile(svprogfuncs, Cmd_Argv(1));
}
void PR_Compile_f(void)
{
	qboolean killondone = false;
	int argc=3;
	double time = Sys_DoubleTime();
	char *argv[64] = {"", "-src", pr_sourcedir.string, "-srcfile", "progs.src"};

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
		if (!FS_FLocateFile(va("%s/%s", argv[2], argv[4]), FSLF_IFFOUND, NULL))
		{
			//try the qc path
			argv[2] = "qc";
		}
		if (!FS_FLocateFile(va("%s/%s", argv[2], argv[4]), FSLF_IFFOUND, NULL))
		{
			//try the progs path (yeah... gah)
			argv[2] = "progs";
		}
		if (!FS_FLocateFile(va("%s/%s", argv[2], argv[4]), FSLF_IFFOUND, NULL))
		{
			//try the gamedir path
			argv[1] = argv[3];
			argv[2] = argv[4];
			argc -= 2;
		}
	}

	if (!svprogfuncs)
	{
		Q_SetProgsParms(true);
		killondone = true;
	}
	if (svprogfuncs->StartCompile && PR_StartCompile(svprogfuncs, argc, argv))
		while(PR_ContinueCompile(svprogfuncs));

	if (killondone)
		PR_Deinit();

	time = Sys_DoubleTime() - time;

	Con_TPrintf("Compile took %f secs\n", time);
}

void PR_ApplyCompilation_f (void)
{
	edict_t *ent;
	char *s;
	size_t len;
	int i;
	if (sv.state < ss_active)
	{
		Con_Printf("Can't apply: Server isn't running or is still loading\n");
		return;
	}

	Con_Printf("Saving state\n");
	s = PR_SaveEnts(svprogfuncs, NULL, &len, 0, 1);


	PR_Configure(svprogfuncs, pr_ssqc_memsize.ival, MAX_PROGS, pr_enable_profiling.ival);
	PR_RegisterFields();
	sv.world.edict_size=PR_InitEnts(svprogfuncs, sv.world.max_edicts);

	sv.world.edict_size=svprogfuncs->load_ents(svprogfuncs, s, NULL, NULL);


	PR_LoadGlabalStruct(false);

	pr_global_struct->time = sv.world.physicstime;

	for (i=0 ; i<sv.allocated_client_slots ; i++)
	{
		ent = EDICT_NUM(svprogfuncs, i+1);

		svs.clients[i].edict = ent;
	}

	World_ClearWorld (&sv.world, true);

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

//	Cvar_Set(Cvar_FindVar("debugger"), "1");
}
void PR_WatchPoint_f(void)
{
	char *variable = Cmd_Argv(1);
	int oldself;
	if (!*variable)
		variable = NULL;

	if (!svprogfuncs)
	{
		Con_Printf("Start the server first\n");
		return;
	}
	oldself = pr_global_struct->self;
	if (oldself == 0)
	{	//if self is world, set it to something sensible.
		int i;
		for (i = 0; i < sv.allocated_client_slots; i++)
		{
			if (svs.clients[i].state && svs.clients[i].netchan.remote_address.type == NA_LOOPBACK)
			{
				//always use first local client, if available.
				pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, svs.clients[i].edict);
				break;
			}
			//failing that, just use the first client.
			if (svs.clients[i].state == cs_spawned && !pr_global_struct->self)
				pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, svs.clients[i].edict);
		}
	}
	if (svprogfuncs->SetWatchPoint(svprogfuncs, variable))
		Con_Printf("Watchpoint set\n");
	else
		Con_Printf("Watchpoint cleared\n");
	pr_global_struct->self = oldself;

//	Cvar_Set(Cvar_FindVar("debugger"), "1");
}

static void PR_SSProfile_f(void)
{
	if (svprogfuncs && svprogfuncs->DumpProfile)
		if (!svprogfuncs->DumpProfile(svprogfuncs, !atof(Cmd_Argv(1))))
			Con_Printf("Enabled ssqc profiling. Re-execute %s to see the results.\n", Cmd_Argv(0));
}

static void PR_SSPoke_f(void)
{
	if (!SV_MayCheat())
		Con_TPrintf ("Please set sv_cheats 1 and restart the map first.\n");
	else if (svprogfuncs && svprogfuncs->EvaluateDebugString)
		Con_TPrintf("Result: %s\n", svprogfuncs->EvaluateDebugString(svprogfuncs, Cmd_Args()));
	else
		Con_TPrintf ("not supported.\n");
}

void PR_SSCoreDump_f(void)
{
	if (!svprogfuncs)
	{
		Con_Printf("Progs not running, you need to start a server first\n");
		return;
	}

	{
		size_t size = 1024*1024*8;
		char *buffer = BZ_Malloc(size);
		svprogfuncs->save_ents(svprogfuncs, buffer, &size, size, 3);
		COM_WriteFile("ssqccore.txt", FS_GAMEONLY, buffer, size);
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

	Cmd_AddCommand ("breakpoint", PR_BreakPoint_f);
	Cmd_AddCommand ("watchpoint", PR_WatchPoint_f);
	Cmd_AddCommand ("watchpoint_ssqc", PR_WatchPoint_f);
	Cmd_AddCommand ("decompile", PR_Decompile_f);
	Cmd_AddCommand ("compile", PR_Compile_f);
	Cmd_AddCommand ("applycompile", PR_ApplyCompilation_f);
	Cmd_AddCommand ("coredump_ssqc", PR_SSCoreDump_f);
	Cmd_AddCommand ("poke_ssqc", PR_SSPoke_f);
	Cmd_AddCommandD ("profile_ssqc", PR_SSProfile_f, "Displays how much time has been spent in various QC functions since this command was last used.\nIf pr_enable_profiling is set, profiling will be enabled automatically, and can be used to list spawn functions.\nAdd an arg with value 1 if you wish to avoid purging timing information.");

	Cmd_AddCommand ("extensionlist_ssqc", PR_SVExtensionList_f);
	Cmd_AddCommand ("pr_dumpplatform", PR_DumpPlatform_f);

/*
#ifdef _DEBUG
	Cmd_AddCommand ("svtestprogs", QCLibTest);
#endif
*/
	Cvar_Register(&pr_imitatemvdsv, cvargroup_progs);
	Cvar_Register(&pr_fixbrokenqccarrays, cvargroup_progs);

	Cvar_Register(&pr_maxedicts, cvargroup_progs);
	Cvar_Register(&pr_no_playerphysics, cvargroup_progs);
	Cvar_Register(&pr_no_parsecommand, cvargroup_progs);

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

	Cvar_Register (&pr_ssqc_progs, cvargroup_progs);
	Cvar_Register (&pr_compatabilitytest, cvargroup_progs);

	Cvar_Register (&dpcompat_nopreparse, cvargroup_progs);
	Cvar_Register (&pr_nonetaccess, cvargroup_progs);
	Cvar_Register (&pr_overridebuiltins, cvargroup_progs);

	Cvar_Register (&pr_ssqc_coreonerror, cvargroup_progs);
	Cvar_Register (&pr_ssqc_memsize, cvargroup_progs);

	Cvar_Register (&sv_gameplayfix_honest_tracelines, cvargroup_progs);
	Cvar_Register (&sv_gameplayfix_setmodelrealbox, cvargroup_progs);
	Cvar_Register (&sv_gameplayfix_setmodelsize_qw, cvargroup_progs);

#ifdef SQL
	SQL_Init();
#endif
}

void SVQ1_CvarChanged(cvar_t *var)
{
	if (svprogfuncs)
	{
		PR_AutoCvar(svprogfuncs, var);
	}
}

static void QCBUILTIN PF_precache_model (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals);
static void QCBUILTIN PF_setmodel (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals);
void QCBUILTIN PF_makestatic (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals);
static void PR_FallbackSpawn_Misc_Model(pubprogfuncs_t *progfuncs, edict_t *self)
{
	void *pr_globals;
	eval_t *val;

	if (!self->v->model && (val = progfuncs->GetEdictFieldValue(progfuncs, self, "mdl", ev_string, NULL)))
		self->v->model = val->string;
	if (!*PR_GetString(progfuncs, self->v->model)) //must have a model, because otherwise various things will assume its not valid at all.
		progfuncs->SetStringField(progfuncs, self, &self->v->model, "*null", true);

	if (self->v->angles[1] < 0)	//mimic AD. shame there's no avelocity clientside.
		self->v->angles[1] = (rand()*(360.0f/RAND_MAX));

	//make sure the model is precached, to avoid errors.
	pr_globals = PR_globals(progfuncs, PR_CURRENT);
	G_INT(OFS_PARM0) = self->v->model;
	PF_precache_model(progfuncs, pr_globals);

	pr_globals = PR_globals(progfuncs, PR_CURRENT);
	G_INT(OFS_PARM0) = EDICT_TO_PROG(progfuncs, self);
	G_INT(OFS_PARM1) = self->v->model;
	PF_setmodel(progfuncs, pr_globals);

	//and lets just call makestatic instead of worrying if it'll interfere with the rest of the qc.
	pr_globals = PR_globals(progfuncs, PR_CURRENT);
	G_INT(OFS_PARM0) = EDICT_TO_PROG(progfuncs, self);
	PF_makestatic(progfuncs, pr_globals);
}
struct spawnents_s
{
	int killonspawnflags;
	eval_t *fulldata;
	func_t CheckSpawn;
	const char *spawnwarned[32];
};
static void PDECL PR_DoSpawnInitialEntity(pubprogfuncs_t *progfuncs, struct edict_s *ed, void *vctx, const char *start, const char *end)
{
	struct spawnents_s *ctx = vctx;
	const char *eclassname;
	func_t f;
	char spawnfuncname[256];

	if ((int)ed->v->spawnflags & ctx->killonspawnflags)
	{
		ED_Free(progfuncs, (struct edict_s *)ed);
		return;
	}

	if (!ctx->CheckSpawn)
		ctx->CheckSpawn = PR_FindFunction(progfuncs, "CheckSpawn", -2);

	eclassname = PR_GetString(progfuncs, ed->v->classname);
	if (!*eclassname)
	{
		printf("No classname\n");
		ED_Free(progfuncs, ed);
	}
	else
	{
		//added by request of Mercury.
		if (ctx->fulldata)	//this is a vital part of HL map support!!!
		{	//essentually, it passes the ent's spawn info to the ent.
			char *spawndata;//a standard quake ent.
#ifdef QCGC
			const char *in;
			ctx->fulldata->string = progfuncs->AllocTempString(progfuncs, &spawndata, end-start+1);
			for (in = start; in < end; )
			{
				char c = *in++;
				if (c == '\n')
					*spawndata++ = '\t';
				else
					*spawndata++ = c;
			}
			*spawndata = 0;
#else
			char *nl;	//otherwise it sees only the named fields of
			spawndata = PRHunkAlloc(progfuncs, file - datastart +1, "fullspawndata");
			strncpy(spawndata, datastart, file - datastart);
			spawndata[file - datastart] = '\0';
			for (nl = spawndata; *nl; nl++)
				if (*nl == '\n')
					*nl = '\t';
			ctx->fulldata->string = PR_StringToProgs(&progfuncs->funcs, spawndata);
#endif
		}

		*sv.world.g.self = EDICT_TO_PROG(progfuncs, ed);

		//DP_SV_SPAWNFUNC_PREFIX support
		Q_snprintfz(spawnfuncname, sizeof(spawnfuncname), "spawnfunc_%s", eclassname);
		f = PR_FindFunction(progfuncs, spawnfuncname, PR_ANYBACK);
		if (!f)
			f = PR_FindFunction(progfuncs, eclassname, PR_ANYBACK);
		if (f)
		{
			if (ctx->CheckSpawn)
			{
				void *pr_globals = PR_globals(progfuncs, PR_CURRENT);
				G_INT(OFS_PARM0) = f;
				PR_ExecuteProgram(progfuncs, ctx->CheckSpawn);
				//call the spawn func or remove.
			}
			else
				PR_ExecuteProgram(progfuncs, f);
		}
		else if (ctx->CheckSpawn)
		{
			void *pr_globals = PR_globals(progfuncs, PR_CURRENT);
			G_INT(OFS_PARM0) = 0;
			PR_ExecuteProgram(progfuncs, ctx->CheckSpawn);
			//the mod is responsible for freeing unrecognised ents.
		}
		else if (!strcmp(eclassname, "misc_model"))
			PR_FallbackSpawn_Misc_Model(progfuncs, ed);
		else
		{
			//only warn on the first occurence of the classname, don't spam.
			int i;
			if (svs.numprogs)
				for (i = 0; i < countof(ctx->spawnwarned); i++)
				{
					if (!ctx->spawnwarned[i])
					{
						printf("Couldn't find spawn function for %s\n", eclassname);
						ctx->spawnwarned[i] = eclassname;
						break;
					}
					else if (!strcmp(ctx->spawnwarned[i], eclassname))
						break;
				}
			ED_Free(progfuncs, ed);
		}
	}
}
void PR_SpawnInitialEntities(const char *file)
{
	extern cvar_t skill;
	struct spawnents_s ctx;
	memset(&ctx, 0, sizeof(ctx));

#ifdef HEXEN2
	if (progstype == PROG_H2)
	{
		extern cvar_t coop;
		if (deathmatch.value)
			ctx.killonspawnflags = SPAWNFLAG_NOT_H2DEATHMATCH;
		else if (coop.value)
			ctx.killonspawnflags = SPAWNFLAG_NOT_H2COOP;
		else
		{
			cvar_t *cl_playerclass = Cvar_Get("cl_playerclass", "0", CVAR_USERINFO, 0);
			ctx.killonspawnflags = SPAWNFLAG_NOT_H2SINGLE;

			if (cl_playerclass && cl_playerclass->ival == 1)
				ctx.killonspawnflags |= SPAWNFLAG_NOT_H2PALADIN;
			else if (cl_playerclass && cl_playerclass->ival == 2)
				ctx.killonspawnflags |= SPAWNFLAG_NOT_H2CLERIC;
			else if (cl_playerclass && cl_playerclass->ival == 3)
				ctx.killonspawnflags |= SPAWNFLAG_NOT_H2NECROMANCER;
			else if (cl_playerclass && cl_playerclass->ival == 4)
				ctx.killonspawnflags |= SPAWNFLAG_NOT_H2THEIF;
			else if (cl_playerclass && cl_playerclass->ival == 5)
				ctx.killonspawnflags |= SPAWNFLAG_NOT_H2NECROMANCER;	/*yes, I know.,. makes no sense*/
		}
		if (skill.value < 0.5)
			ctx.killonspawnflags |= SPAWNFLAG_NOT_H2EASY;
		else if (skill.value > 1.5)
			ctx.killonspawnflags |= SPAWNFLAG_NOT_H2HARD;
		else
			ctx.killonspawnflags |= SPAWNFLAG_NOT_H2MEDIUM;

		//don't filter based on player class. we're lame and don't have any real concept of player classes.
	}
	else
#endif
		if (!deathmatch.value)	//decide if we are to inhibit single player game ents instead
	{
		if (skill.value < 0.5)
			ctx.killonspawnflags = SPAWNFLAG_NOT_EASY;
		else if (skill.value > 1.5)
			ctx.killonspawnflags = SPAWNFLAG_NOT_HARD;
		else
			ctx.killonspawnflags = SPAWNFLAG_NOT_MEDIUM;
	}
	else
		ctx.killonspawnflags = SPAWNFLAG_NOT_DEATHMATCH;

	ctx.fulldata = PR_FindGlobal(svprogfuncs, "__fullspawndata", PR_ANY, NULL);

	if (svprogfuncs)
		sv.world.edict_size = PR_LoadEnts(svprogfuncs, file, &ctx, PR_DoSpawnInitialEntity);
	else
		sv.world.edict_size = 0;
}

void SV_RegisterH2CustomTents(void);
void Q_InitProgs(void)
{
	int i, i2;
	func_t f, f2;
	globalvars_t *pr_globals;
	static char addons[2048];
	char *as, *a;
	progsnum_t prnum, oldprnum=-1;
	int d1, d2;

	ssqc_deprecated_warned = false;

	QC_Clear();

	Q_SetProgsParms(false);

	memset(pr_builtin, 0, sizeof(pr_builtin));

// load progs to get entity field count
	PR_Configure(svprogfuncs, pr_ssqc_memsize.ival, MAX_PROGS, pr_enable_profiling.ival);

	PR_RegisterFields();

	svs.numprogs=0;

	d1 = FS_FLocateFile("progs.dat",	FSLF_DONTREFERENCE|FSLF_DEEPONFAILURE|FSLF_IGNOREBASEDEPTH, NULL);
	d2 = FS_FLocateFile("qwprogs.dat",	FSLF_DONTREFERENCE|FSLF_DEEPONFAILURE|FSLF_IGNOREBASEDEPTH, NULL);
	//FIXME id1/progs.dat vs qw/qwprogs.dat - these should be considered to have the same priority.
	if (d1 < d2)	//progs.dat is closer to the gamedir
		strcpy(addons, "progs.dat");
	else if (d1 > d2)	//qwprogs.dat is closest
	{
		strcpy(addons, "qwprogs.dat");
		d1 = d2;
	}
	//both are an equal depth - same path.
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

		f = COM_LoadTempFile("maplist.txt", NULL);
		f = COM_Parse(f);
		maps = atoi(com_token);
		for (j = 0; j < maps; j++)
		{
			f = COM_Parse(f);
			if (!Q_strcasecmp(svs.name, com_token))
			{
				f = COM_Parse(f);
				strcpy(addons, com_token);
				break;
			}
			f = strchr(f, '\n');	//skip to the end of the line.
		}
	}

	/*if pr_ssqc_progs cvar is set, override the default*/
	if (*pr_ssqc_progs.string && strlen(pr_ssqc_progs.string)<64 && *pr_ssqc_progs.string != '*')	//a * is a special case to not load a q2 dll.
	{
		Q_strncpyz(addons, pr_ssqc_progs.string, MAX_QPATH);
		COM_DefaultExtension(addons, ".dat", sizeof(addons));
	}
	oldprnum= AddProgs(addons);

	/*try to load qwprogs.dat if we didn't manage to load one yet*/
	if (oldprnum < 0 && strcmp(addons, "qwprogs.dat"))
	{
#ifndef SERVERONLY
		if (SCR_UpdateScreen)
			SCR_UpdateScreen();
#endif
		oldprnum= AddProgs("qwprogs.dat");
	}

	/*try to load qwprogs.dat if we didn't manage to load one yet*/
	if (oldprnum < 0 && strcmp(addons, "progs.dat"))
	{
#ifndef SERVERONLY
		if (SCR_UpdateScreen)
			SCR_UpdateScreen();
#endif
		oldprnum= AddProgs("progs.dat");
	}
	if (oldprnum < 0)
	{
		PR_LoadGlabalStruct(true);
//		SV_Error("Couldn't open or compile progs\n");
		Con_Printf(CON_ERROR"Running without gamecode\n");
	}

#ifdef SQL
	SQL_KillServers(); // TODO: is this the best placement for this?
#endif

	if (oldprnum >= 0)
		f = PR_FindFunction (svprogfuncs, "AddAddonProgs", oldprnum);
	else
		f = 0;
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
	a = COM_LoadStackFile("mod.gam", addons, 2048, NULL);

	if (a)
	{
		if (progstype == PROG_QW)
			as = strstr(a, "extraqwprogs=");
		else
			as = strstr(a, "extraprogs=");
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

	switch (sv.world.worldmodel->fromgame)	//spawn functions for - spawn funcs still come from the first progs found.
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
	a = as = COM_LoadStackFile(va("maps/%s.inf", svs.name), addons, sizeof(addons), NULL);
	if (a)
	{
		if (progstype == PROG_QW)
			as = strstr(a, "qwprogs=");
		else
			as = strstr(a, "progs=");
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

//	svprogfuncs->ToggleBreak(svprogfuncs, "", 0, 2);
//	svprogfuncs->SetWatchPoint(svprogfuncs, "");

	sv.world.edict_size = PR_InitEnts(svprogfuncs, sv.world.max_edicts);

	if (progstype == PROG_QW)
		sv.world.defaultgravityscale = 0;
	else
		sv.world.defaultgravityscale = 1;
#ifdef HEXEN2
	SV_RegisterH2CustomTents();
#endif

	World_RBE_Start(&sv.world);
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

	if (!svprogfuncs || !gfuncs.PausedTic)
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
	pr_global_struct->time = sv.world.physicstime;

	// check for packets from connected clients
	pr_global_struct->self = 0;
	for (i=0, cl=svs.clients ; i<sv.allocated_client_slots ; i++,cl++)
	{
		if (cl->state == cs_free)
			continue;
		if (!NET_CompareAdr (&net_from, &cl->netchan.remote_address))
			continue;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, cl->edict);
		break;
	}



	G_INT(OFS_PARM0) = PR_TempString(svprogfuncs, NET_AdrToString (adr, sizeof(adr), &net_from));

	G_INT(OFS_PARM1) = PR_TempString(svprogfuncs, s);
	PR_ExecuteProgram (svprogfuncs, gfuncs.ParseConnectionlessPacket);
	return G_FLOAT(OFS_RETURN);
}

qboolean PR_ParseClusterEvent(char *dest, char *source, char *cmd, char *info)
{
	globalvars_t *pr_globals;

	if (svprogfuncs && gfuncs.ParseClusterEvent)
	{
		pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
		pr_global_struct->time = sv.world.physicstime;
		pr_global_struct->self = 0;

		G_INT(OFS_PARM0) = (int)PR_TempString(svprogfuncs, dest);
		G_INT(OFS_PARM1) = (int)PR_TempString(svprogfuncs, source);
		G_INT(OFS_PARM2) = (int)PR_TempString(svprogfuncs, cmd);
		G_INT(OFS_PARM3) = (int)PR_TempString(svprogfuncs, info);
		PR_ExecuteProgram (svprogfuncs, gfuncs.ParseClusterEvent);
		return true;
	}

	return false;
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

	/*some people are irresponsible*/
	if (pr_no_parsecommand.ival)
		return false;

	if (gfuncs.ParseClientCommand)
	{	//the QC is expected to send it back to use via a builtin.

		pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
		pr_global_struct->time = sv.world.physicstime;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);

		G_INT(OFS_PARM0) = (int)PR_TempString(svprogfuncs, s);
		PR_ExecuteProgram (svprogfuncs, gfuncs.ParseClientCommand);
		return true;
	}

	return false;
}
int tokenizeqc(char *str, qboolean dpfuckage);
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
		pr_global_struct->time = sv.world.physicstime;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);

		G_INT(OFS_PARM0) = (int)PR_TempString(svprogfuncs, s);
		PR_ExecuteProgram (svprogfuncs, gfuncs.ParseClientCommand);
		return true;
	}

#ifdef VM_Q1
	if (svs.gametype == GT_Q1QVM)
	{
		pr_global_struct->time = sv.world.physicstime;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
		return Q1QVM_ClientCommand();
	}
#endif

	if (gfuncs.UserCmd && pr_imitatemvdsv.value >= 0)
	{	//we didn't recognise it. see if the mod does.
		const char *arg0;
		//ktpro bug warning:
		//admin + judge. I don't know the exact rules behind this bug, so I just ban the entire command
		//I can't be arsed detecting ktpro specifically, so assume we're always running ktpro

		pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
		pr_global_struct->time = sv.world.physicstime;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);

		tokenizeqc(s, true);

		//make sure we use the same logic that the qc will use. specifically that we check for " and leading spaces etc
		G_FLOAT(OFS_PARM0) = 0;
		PF_ArgV(svprogfuncs, pr_globals);
		arg0 = PR_GetStringOfs(svprogfuncs, OFS_RETURN);
		if (!strcmp(arg0, "admin") || !strcmp(arg0, "judge"))
		{
			Con_Printf("Blocking potentially unsafe ktpro command: %s\n", s);
			return true;
		}

		G_INT(OFS_PARM0) = (int)PR_TempString(svprogfuncs, s);
		PR_ExecuteProgram (svprogfuncs, gfuncs.UserCmd);
		return !!G_FLOAT(OFS_RETURN);
	}

	return false;
}
qboolean PR_ConsoleCmd(const char *command)
{
	globalvars_t *pr_globals;
	extern redirect_t sv_redirected;

	if (Cmd_ExecLevel < cmd_gamecodelevel.value)
		return false;

	if (svprogfuncs)
	{
		pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
		if (gfuncs.ConsoleCmd)
		{
			if (sv_redirected != RD_OBLIVION)
			{
				pr_global_struct->time = sv.world.physicstime;
				pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv.world.edicts);
			}

			G_INT(OFS_PARM0) = PR_TempString(svprogfuncs, command);
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

		pr_global_struct->time = sv.world.physicstime;
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

		pr_global_struct->time = sv.world.physicstime;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv.world.edicts);

		G_INT(OFS_PARM0) = PR_TempString(svprogfuncs, name);
		G_INT(OFS_PARM1) = PR_TempString(svprogfuncs, oldivalue);
		G_INT(OFS_PARM2) = PR_TempString(svprogfuncs, newvalue);

		PR_ExecuteProgram (svprogfuncs, gfuncs.localinfoChanged);
	}
}

void QC_Clear(void)
{
}


//#define	RETURN_EDICT(pf, e) (((int *)pr_globals)[OFS_RETURN] = EDICT_TO_PROG(pf, e))
#define	RETURN_SSTRING(s) (((int *)pr_globals)[OFS_RETURN] = PR_SetString(prinst, s))	//static - exe will not change it.
#define	RETURN_TSTRING(s) (((int *)pr_globals)[OFS_RETURN] = PR_TempString(prinst, s))	//temp (static but cycle buffers)
#define	RETURN_CSTRING(s) (((int *)pr_globals)[OFS_RETURN] = PR_SetString(prinst, s))	//semi-permanant. (hash tables?)
#define	RETURN_PSTRING(s) (((int *)pr_globals)[OFS_RETURN] = PR_NewString(prinst, s))	//permanant

/*
===============================================================================

						BUILT-IN FUNCTIONS

===============================================================================
*/


static void SV_Effect(vec3_t org, int mdlidx, int startframe, int endframe, int framerate)
{
	if (startframe>255 || mdlidx>255)
	{
		MSG_WriteByte (&sv.multicast, svcfte_effect2);
		MSG_WriteCoord (&sv.multicast, org[0]);
		MSG_WriteCoord (&sv.multicast, org[1]);
		MSG_WriteCoord (&sv.multicast, org[2]);
		MSG_WriteShort (&sv.multicast, mdlidx);
		MSG_WriteShort (&sv.multicast, startframe);
		MSG_WriteByte (&sv.multicast, endframe);
		MSG_WriteByte (&sv.multicast, framerate);

#ifdef NQPROT
		MSG_WriteByte (&sv.nqmulticast, svcnq_effect2);
		MSG_WriteCoord (&sv.nqmulticast, org[0]);
		MSG_WriteCoord (&sv.nqmulticast, org[1]);
		MSG_WriteCoord (&sv.nqmulticast, org[2]);
		MSG_WriteShort (&sv.nqmulticast, mdlidx);
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
		MSG_WriteByte (&sv.multicast, mdlidx);
		MSG_WriteByte (&sv.multicast, startframe);
		MSG_WriteByte (&sv.multicast, endframe);
		MSG_WriteByte (&sv.multicast, framerate);

#ifdef NQPROT
		MSG_WriteByte (&sv.nqmulticast, svcnq_effect);
		MSG_WriteCoord (&sv.nqmulticast, org[0]);
		MSG_WriteCoord (&sv.nqmulticast, org[1]);
		MSG_WriteCoord (&sv.nqmulticast, org[2]);
		MSG_WriteByte (&sv.nqmulticast, mdlidx);
		MSG_WriteByte (&sv.nqmulticast, startframe);
		MSG_WriteByte (&sv.nqmulticast, endframe);
		MSG_WriteByte (&sv.nqmulticast, framerate);
#endif
	}

	SV_MulticastProtExt (org, MULTICAST_PVS, pr_global_struct->dimension_send, 0, 0);
}

#ifdef HEXEN2
static int SV_CustomTEnt_Register(char *effectname, int nettype, float *stain_rgb, float stain_radius, float *dl_rgb, float dl_radius, float dl_time, float *dl_fade)
{
	int i;
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
		return -1;
	}

	Q_strncpyz(sv.customtents[i].particleeffecttype, effectname, sizeof(sv.customtents[i].particleeffecttype));
	sv.customtents[i].netstyle = nettype;

	if (nettype & CTE_STAINS)
	{
		VectorCopy(stain_rgb, sv.customtents[i].stain);
		sv.customtents[i].radius = stain_radius;
	}
	if (nettype & CTE_GLOWS)
	{
		sv.customtents[i].dlightrgb[0] = dl_rgb[0]*255;
		sv.customtents[i].dlightrgb[1] = dl_rgb[1]*255;
		sv.customtents[i].dlightrgb[2] = dl_rgb[2]*255;
		sv.customtents[i].dlightradius = dl_radius/4;
		sv.customtents[i].dlighttime = dl_time*16;
		if (nettype & CTE_CHANNELFADE)
		{
			sv.customtents[i].dlightcfade[0] = dl_fade[0]*64;
			sv.customtents[i].dlightcfade[1] = dl_fade[1]*64;
			sv.customtents[i].dlightcfade[2] = dl_fade[2]*64;
		}
	}

	return i;
}

static int SV_CustomTEnt_Spawn(int index, float *org, float *org2, int count, float *dir)
{
	static int persist_id;
	int type;
	multicast_t mct = MULTICAST_PVS;
	if (index < 0 || index >= 255)
		return -1;
	type = sv.customtents[index].netstyle;

	MSG_WriteByte(&sv.multicast, svcfte_customtempent);
	MSG_WriteByte(&sv.multicast, index);

	if (type & CTE_PERSISTANT)
	{
		persist_id++;
		if (persist_id >= 0x8000)
			persist_id = 1;
		if (sv.state == ss_loading)
			mct = MULTICAST_INIT;
		else
			mct = MULTICAST_ALL;
		MSG_WriteShort(&sv.multicast, persist_id);
	}
	MSG_WriteCoord(&sv.multicast, org[0]);
	MSG_WriteCoord(&sv.multicast, org[1]);
	MSG_WriteCoord(&sv.multicast, org[2]);

	if (type & CTE_ISBEAM)
	{
		MSG_WriteCoord(&sv.multicast, org2[0]);
		MSG_WriteCoord(&sv.multicast, org2[1]);
		MSG_WriteCoord(&sv.multicast, org2[2]);
	}
	if (type & CTE_CUSTOMCOUNT)
	{
		MSG_WriteByte(&sv.multicast, count);
	}
	if (type & CTE_CUSTOMVELOCITY)
	{
		MSG_WriteCoord(&sv.multicast, dir[0]);
		MSG_WriteCoord(&sv.multicast, dir[1]);
		MSG_WriteCoord(&sv.multicast, dir[2]);
	}
	else if (type & CTE_CUSTOMDIRECTION)
	{
		vec3_t norm;
		VectorNormalize2(dir, norm);
		MSG_WriteDir(&sv.multicast, norm);
	}

	SV_MulticastProtExt (org, mct, pr_global_struct->dimension_send, PEXT_CUSTOMTEMPEFFECTS, 0);	//now send the new multicast to all that will.

	return persist_id;
}
#endif



int externcallsdepth;

float PR_LoadAditionalProgs(char *s);
static void QCBUILTIN PF_addprogs(pubprogfuncs_t *prinst, globalvars_t *pr_globals)
{
	const char *s = PR_GetStringOfs(prinst, OFS_PARM0);
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
static void QCBUILTIN PF_objerror (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*s;
	edict_t	*ed;

	s = PF_VarString(prinst, 0, pr_globals);
/*	Con_Printf ("======OBJECT ERROR in %s:\n%s\n", PR_GetString(pr_xfunction->s_name),s);
*/	ed = PROG_TO_EDICT(prinst, pr_global_struct->self);
/*	ED_Print (ed);
*/
	prinst->ED_Print(prinst, ed);

	PR_RunWarning(prinst, "Program error: %s\n", s);

	ED_Free (prinst, ed);
	PR_AbortStack(prinst);

//	if (sv.time > 10)
//		Cbuf_AddText("restart\n", RESTRICT_LOCAL);
}



/*
==============
PF_makevectors

Writes new values for v_forward, v_up, and v_right based on angles
makevectors(vector)
==============
*/
static void QCBUILTIN PF_makevectors (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
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
static void QCBUILTIN PF_setorigin (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*e;
	float	*org;

	e = G_EDICT(prinst, OFS_PARM0);
	if (e->readonly)
	{
		Con_Printf("setorigin on entity %i\n", e->entnum);
		return;
	}
	org = G_VECTOR(OFS_PARM1);
	VectorCopy (org, e->v->origin);
	World_LinkEdict (&sv.world, (wedict_t*)e, false);
}


/*
=================
PF_setsize

the size box is rotated by the current angle

setsize (entity, minvector, maxvector)
=================
*/
static void QCBUILTIN PF_setsize (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*e;
	float	*min, *max;

	e = G_EDICT(prinst, OFS_PARM0);
	if (ED_ISFREE(e))
	{
		if (progstype != PROG_H2 || developer.ival)
			PR_RunWarning(prinst, "%s edict %i was free\n", "setsize", e->entnum);
		return;
	}
	if (e->readonly)
	{
		Con_TPrintf("setsize on readonly entity %i\n", e->entnum);
		return;
	}
	min = G_VECTOR(OFS_PARM1);
	max = G_VECTOR(OFS_PARM2);
	VectorCopy (min, e->v->mins);
	VectorCopy (max, e->v->maxs);
	VectorSubtract (max, min, e->v->size);
	World_LinkEdict (&sv.world, (wedict_t*)e, false);
}


/*
=================
PF_setmodel

setmodel(entity, model)
Also sets size, mins, and maxs for inline bmodels
=================
*/
void PF_setmodel_Internal (pubprogfuncs_t *prinst, edict_t *e, const char *m)
{
	int		i;
	model_t	*mod;

	if (!e)
	{
		PR_RunWarning(prinst, "%s on invalid entity\n", "setmodel");
		return;
	}
	if (e->readonly)
	{
		PR_RunWarning(prinst, "%s edict %i is read-only\n", "setmodel", e->entnum);
		return;
	}
	if (ED_ISFREE(e))
	{
		PR_RunWarning(prinst, "%s edict %i was free\n", "setmodel", e->entnum);
		return;
	}

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
		if (i==MAX_PRECACHE_MODELS || !sv.strings.model_precache[i])
		{
			if (i!=MAX_PRECACHE_MODELS)
			{
#ifdef VM_Q1
				if (svs.gametype == GT_Q1QVM)
					sv.strings.model_precache[i] = m;	//in a qvm, we expect the caller to have used a static location.
				else
#endif
					m = sv.strings.model_precache[i] = PR_AddString(prinst, m, 0, false);
				if (!strcmp(m + strlen(m) - 4, ".bsp"))	//always precache bsps
					sv.models[i] = Mod_FindName(Mod_FixName(m, sv.strings.model_precache[1]));
				Con_Printf("WARNING: SV_ModelIndex: model %s not precached\n", m);

				if (sv.state != ss_loading)
				{
					Con_DPrintf("Delayed model precache: %s\n", m);
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

	prinst->SetStringField(prinst, e, &e->v->model, m, true);
	e->v->modelindex = i;

	// if it is an inline model, get the size information for it
	if (m && (m[0] == '*' || (*m&&progstype == PROG_H2)))
	{
		mod = Mod_ForName (Mod_FixName(m, sv.modelname), MLV_WARN);
		if (mod)
		{
			while(mod->loadstate == MLS_LOADING)
				COM_WorkerPartialSync(mod, &mod->loadstate, MLS_LOADING);

			VectorCopy (mod->mins, e->v->mins);
			VectorCopy (mod->maxs, e->v->maxs);
			VectorSubtract (mod->maxs, mod->mins, e->v->size);
			World_LinkEdict (&sv.world, (wedict_t*)e, false);
		}

		return;
	}

	/*if (progstype == PROG_H2)
	{
		e->v->mins[0] = 0;
		e->v->mins[1] = 0;
		e->v->mins[2] = 0;

		e->v->maxs[0] = 0;
		e->v->maxs[1] = 0;
		e->v->maxs[2] = 0;

		VectorSubtract (e->v->maxs, e->v->mins, e->v->size);
	}
	else*/
	{
		if (sv_gameplayfix_setmodelrealbox.ival)
			mod = SVPR_GetCModel(&sv.world, i);
		else
			mod = sv.models[i];

		if (progstype != PROG_QW || sv_gameplayfix_setmodelsize_qw.ival)
		{	//also sets size.

			//nq dedicated servers load bsps and mdls
			//qw dedicated servers only load bsps (better)
			if (mod)
			{
				mod = Mod_ForName (Mod_FixName(m, sv.modelname), MLV_WARNSYNC);
				if (mod)
				{
					while(mod->loadstate == MLS_LOADING)
						COM_WorkerPartialSync(mod, &mod->loadstate, MLS_LOADING);

					VectorCopy (mod->mins, e->v->mins);
					VectorCopy (mod->maxs, e->v->maxs);
					VectorSubtract (mod->maxs, mod->mins, e->v->size);
					World_LinkEdict (&sv.world, (wedict_t*)e, false);
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
				World_LinkEdict (&sv.world, (wedict_t*)e, false);
			}
		}
		else
		{
			if (mod && mod->type != mod_alias)
			{
				while(mod->loadstate == MLS_LOADING)
					COM_WorkerPartialSync(mod, &mod->loadstate, MLS_LOADING);

				VectorCopy (mod->mins, e->v->mins);
				VectorCopy (mod->maxs, e->v->maxs);
				VectorSubtract (mod->maxs, mod->mins, e->v->size);
				World_LinkEdict (&sv.world, (wedict_t*)e, false);
			}
			//qw was fixed - it never sets the size of an alias model, mostly because it doesn't know it.
		}
	}
}

static void QCBUILTIN PF_setmodel (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*e;
	const char	*m;

	e = G_EDICT(prinst, OFS_PARM0);
	m = PR_GetStringOfs(prinst, OFS_PARM1);

	PF_setmodel_Internal(prinst, e, m);
}

#ifdef HEXEN2
static void QCBUILTIN PF_h2set_puzzle_model (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{	//qc/hc lacks string manipulation.
	edict_t	*e;
	const char *shortname;
	char fullname[MAX_QPATH];
	e = G_EDICT(prinst, OFS_PARM0);
	shortname = PR_GetStringOfs(prinst, OFS_PARM1);

	snprintf(fullname, sizeof(fullname)-1, "models/puzzle/%s.mdl", shortname);
	PF_setmodel_Internal(prinst, e, fullname);
}
#endif

/*
=================
PF_bprint

broadcast print to everyone on server

bprint(value)
=================
*/
static void QCBUILTIN PF_bprint (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char		*s;
	int			level;

#ifdef SERVER_DEMO_PLAYBACK
	if (sv.demofile)
		return;
#endif

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
static void QCBUILTIN PF_sprint (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char		*s;
	client_t	*client;
	int			entnum;
	int			level;

#ifdef SERVER_DEMO_PLAYBACK
	if (sv.demofile)
		return;
#endif

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
		Con_TPrintf ("tried to sprint to a non-client\n");
		return;
	}

	client = &svs.clients[entnum-1];

	SV_ClientPrintf (client, level, "%s", s);

	if (sv_specprint.ival & SPECPRINT_SPRINT)
	{
		client_t *spec;
		unsigned int i;
		for (i = 0, spec = svs.clients; i < sv.allocated_client_slots; i++, spec++)
		{
			if (spec->state != cs_spawned || !spec->spectator)
				continue;
			if (spec->spec_track == entnum && (spec->spec_print & SPECPRINT_SPRINT))
			{
				if (level < spec->messagelevel)
					continue;
				if (spec->controller)
					SV_PrintToClient(spec->controller, level, s);
				else
					SV_PrintToClient(spec, level, s);
			}
		}
	}
}

//When a client is backbuffered, it's generally not a brilliant plan to send a bazillion stuffcmds. You have been warned.
//This handy function will let the mod know when it shouldn't send more. (use instead of a timer, and you'll never get clients overflowing. yay.)
static void QCBUILTIN PF_isbackbuffered (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
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
void PF_centerprint_Internal (int entnum, qboolean plaque, const char *s)
{
	client_t	*cl;
	int			slen;

#ifdef SERVER_DEMO_PLAYBACK
	if (sv.demofile)
		return;
#endif

	if (entnum < 1 || entnum > sv.allocated_client_slots)
	{
		Con_TPrintf ("tried to sprint to a non-client\n");
		return;
	}

	cl = &svs.clients[entnum-1];
	if (cl->centerprintstring)
		Z_Free(cl->centerprintstring);
	cl->centerprintstring = NULL;

	slen = strlen(s);
	if (plaque && *s)
	{
		cl->centerprintstring = Z_Malloc(slen+3);
		cl->centerprintstring[0] = '/';
		cl->centerprintstring[1] = 'P';
		strcpy(cl->centerprintstring+2, s);
	}
	else
	{
		cl->centerprintstring = Z_Malloc(slen+1);
		strcpy(cl->centerprintstring, s);
	}

	if (sv_specprint.ival & SPECPRINT_CENTERPRINT)
	{
		client_t *spec;
		unsigned int i;
		for (i = 0, spec = svs.clients; i < sv.allocated_client_slots; i++, spec++)
		{
			if (spec->state != cs_spawned || !spec->spectator || spec == cl)
				continue;
			if (spec->spec_track == entnum && (spec->spec_print & SPECPRINT_CENTERPRINT))
			{
				Z_Free(spec->centerprintstring);
				spec->centerprintstring = Z_StrDup(cl->centerprintstring);
			}
		}
	}
}

static void QCBUILTIN PF_centerprint (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char		*s;
	int			entnum;

	entnum = G_EDICTNUM(prinst, OFS_PARM0);
	s = PF_VarString(prinst, 1, pr_globals);
	PF_centerprint_Internal(entnum, false, s);
}

/*
=================
PF_particle

particle(origin, color, count)
=================
*/
static void QCBUILTIN PF_particle (pubprogfuncs_t *prinst, globalvars_t *pr_globals)	//I said it was for compatability only.
{
	float		*org, *dir;
	int		color;
	int		count;
	int i, v;

	org = G_VECTOR(OFS_PARM0);
	dir = G_VECTOR(OFS_PARM1);
	color = G_FLOAT(OFS_PARM2);
	count = G_FLOAT(OFS_PARM3);

	count = bound(0, count, 255);
	color &= 0xff;

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
	MSG_WriteByte (&sv.nqmulticast, color);
	SV_MulticastProtExt(org, MULTICAST_PVS, pr_global_struct->dimension_send, 0, 0);
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
		MSG_WriteByte (&sv.multicast, TEQW_BLOOD);
		MSG_WriteByte (&sv.multicast, count<10?1:(count+10)/20);
		MSG_WriteCoord (&sv.multicast, org[0]);
		MSG_WriteCoord (&sv.multicast, org[1]);
		MSG_WriteCoord (&sv.multicast, org[2]);
		SV_MulticastProtExt(org, MULTICAST_PVS, pr_global_struct->dimension_send, 0, PEXT_HEXEN2);
	}
	else if (color == 225)
	{
		MSG_WriteByte (&sv.multicast, svc_temp_entity);
		MSG_WriteByte (&sv.multicast, TEQW_LIGHTNINGBLOOD);
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

static void QCBUILTIN PF_te_blooddp (pubprogfuncs_t *prinst, globalvars_t *pr_globals)
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

	(void)dir; //FIXME: sould be sending TEDP_BLOOD
	MSG_WriteByte (&sv.multicast, svc_temp_entity);
	MSG_WriteByte (&sv.multicast, TEQW_BLOOD);
	MSG_WriteByte (&sv.multicast, count<10?1:(count+10)/20);
	MSG_WriteCoord (&sv.multicast, org[0]);
	MSG_WriteCoord (&sv.multicast, org[1]);
	MSG_WriteCoord (&sv.multicast, org[2]);
	SV_MulticastProtExt (org, MULTICAST_PVS, pr_global_struct->dimension_send, 0, 0);
}

#ifdef HEXEN2
/*
=================
PF_particle2 - hexen2

particle(origin, dmin, dmax, color, effect, count)
=================
*/
static void QCBUILTIN PF_particle2 (pubprogfuncs_t *prinst, globalvars_t *pr_globals)
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
	MSG_WriteByte (&sv.multicast, bound(0, count, 255));
	MSG_WriteByte (&sv.multicast, effect);

	SV_MulticastProtExt (org, MULTICAST_PVS, pr_global_struct->dimension_send, PEXT_HEXEN2, 0);
}


/*
=================
PF_particle3 - hexen2

particle(origin, box, color, effect, count)
=================
*/
static void QCBUILTIN PF_particle3 (pubprogfuncs_t *prinst, globalvars_t *pr_globals)
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
static void QCBUILTIN PF_particle4 (pubprogfuncs_t *prinst, globalvars_t *pr_globals)
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
	MSG_WriteByte (&sv.multicast, bound(0, radius, 255));

	MSG_WriteShort (&sv.multicast, color);
	MSG_WriteByte (&sv.multicast, bound(0, count, 255));
	MSG_WriteByte (&sv.multicast, effect);

	SV_MulticastProtExt (org, MULTICAST_PVS, pr_global_struct->dimension_send, PEXT_HEXEN2, 0);
}

static void QCBUILTIN PF_h2particleexplosion(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	//used by the (regular) ice staff, and multiple other things.
	float *org;
	int color,radius,count, effect;
	org = G_VECTOR(OFS_PARM0);
	radius = G_FLOAT(OFS_PARM1);
	color = G_FLOAT(OFS_PARM2);
	effect = 255;	//special explosion thing
	count = G_FLOAT(OFS_PARM3);

	MSG_WriteByte (&sv.multicast, svcfte_particle4);
	MSG_WriteCoord (&sv.multicast, org[0]);
	MSG_WriteCoord (&sv.multicast, org[1]);
	MSG_WriteCoord (&sv.multicast, org[2]);
	MSG_WriteByte (&sv.multicast, bound(0, radius, 255));

	MSG_WriteShort (&sv.multicast, color);
	MSG_WriteByte (&sv.multicast, count);
	MSG_WriteByte (&sv.multicast, effect);

	SV_MulticastProtExt (org, MULTICAST_PVS, pr_global_struct->dimension_send, PEXT_HEXEN2, 0);
}
#endif

/*
=================
PF_ambientsound

=================
*/
void PF_ambientsound_Internal (float *pos, const char *samp, float vol, float attenuation)
{
	staticsound_state_t *state;
	int		soundnum;

// check to see if samp was properly precached
	for (soundnum=1 ; *sv.strings.sound_precache[soundnum] ; soundnum++)
		if (!strcmp(sv.strings.sound_precache[soundnum],samp))
			break;

	if (!*sv.strings.sound_precache[soundnum])
	{
		Con_TPrintf ("no precache: %s\n", samp);
		return;
	}

	if (sv.num_static_sounds == sv_max_staticsounds)
	{
		sv_max_staticsounds += 16;
		sv_staticsounds = BZ_Realloc(sv_staticsounds, sizeof(*sv_staticsounds) * sv_max_staticsounds);
	}

	state = &sv_staticsounds[sv.num_static_sounds++];
	memset(state, 0, sizeof(*state));
	VectorCopy(pos, state->position);
	state->soundnum = soundnum;
	state->volume = bound(0, (int)(vol*255), 255);
	state->attenuation = attenuation*64;
}

static void QCBUILTIN PF_ambientsound (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char		*samp;
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
pitchadj is a percent. values greater than 100 will result in a lower pitch, less than 100 gives a higher pitch.

=================
*/
static void QCBUILTIN PF_sound (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char		*sample;
	int			channel;
	edict_t		*entity;
	int 		volume;
	float attenuation;
	float			pitchadj;
	unsigned int chflags;
	float	timeofs;

	entity = G_EDICT(prinst, OFS_PARM0);
	channel = G_FLOAT(OFS_PARM1);
	sample = PR_GetStringOfs(prinst, OFS_PARM2);
	volume = G_FLOAT(OFS_PARM3) * 255;
	attenuation = G_FLOAT(OFS_PARM4);
	if (svprogfuncs->callargc > 5)
		pitchadj = G_FLOAT(OFS_PARM5)*0.01;
	else
		pitchadj = 0;
	if (svprogfuncs->callargc > 6)
	{
		chflags = G_FLOAT(OFS_PARM6);
		if (channel < 0)
			channel = 0;
	}
	else
	{
		//QW uses channel&8 to mean reliable.
		chflags = (channel & 8)?CF_RELIABLE:0;
		//demangle it so the upper bits are still useful.
		channel = (channel & 7) | ((channel & ~15) >> 1);
	}
	timeofs = (svprogfuncs->callargc>7)?G_FLOAT(OFS_PARM7):0;

	if (volume < 0)	//erm...
		return;

	if (volume > 255)
		volume = 255;

	SVQ1_StartSound (NULL, (wedict_t*)entity, channel, sample, volume, attenuation, pitchadj, timeofs, chflags);
}

static void QCBUILTIN PF_pointsound(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *sample;
	float *origin;
	float volume;
	float attenuation;
	float pitchpct;

	origin = G_VECTOR(OFS_PARM0);
	sample = PR_GetStringOfs(prinst, OFS_PARM1);
	volume = G_FLOAT(OFS_PARM2);
	attenuation = G_FLOAT(OFS_PARM3);
	if (prinst->callargc >= 5)
		pitchpct = G_FLOAT(OFS_PARM4)*0.01;
	else
		pitchpct = 0;

	SVQ1_StartSound (origin, sv.world.edicts, 0, sample, volume, attenuation, pitchpct, 0, 0);
}

//an evil one from telejano.
#ifndef SERVERONLY
static void QCBUILTIN PF_ss_LocalSound(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	sfx_t	*sfx;

	const char * s = PR_GetStringOfs(prinst, OFS_PARM0);
	float chan = (prinst->callargc>=1)?G_FLOAT(OFS_PARM1):0;
	float vol = (prinst->callargc>=2)?G_FLOAT(OFS_PARM2):1;

	if (!isDedicated)
	{
		if ((sfx = S_PrecacheSound(s)))
			S_StartSound(cl.playerview[0].playernum, chan, sfx, cl.playerview[0].simorg, NULL, vol, 0.0, 0, 0, CF_NOSPACIALISE);
	}
};
#else
#define PF_ss_LocalSound PF_Fixme
#endif

static void set_trace_globals(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals, trace_t *trace)
{
	pr_global_struct->trace_allsolid = trace->allsolid;
	pr_global_struct->trace_startsolid = trace->startsolid;
	pr_global_struct->trace_fraction = trace->fraction;
	pr_global_struct->trace_inwater = trace->inwater;
	pr_global_struct->trace_inopen = trace->inopen;
	pr_global_struct->trace_surfaceflagsf = trace->surface?trace->surface->flags:0;
	pr_global_struct->trace_surfaceflagsi = trace->surface?trace->surface->flags:0;
	if (pr_global_ptrs->trace_surfacename)
		prinst->SetStringField(prinst, NULL, &pr_global_struct->trace_surfacename, trace->surface?trace->surface->name:NULL, true);
	pr_global_struct->trace_endcontentsf = trace->contents;
	pr_global_struct->trace_endcontentsi = trace->contents;
	pr_global_struct->trace_brush_id = trace->brush_id;
	pr_global_struct->trace_brush_faceid = trace->brush_face;
	pr_global_struct->trace_surface_id = trace->surface_id;
	pr_global_struct->trace_bone_id = trace->bone_id;
	pr_global_struct->trace_triangle_id = trace->triangle_id;

//	if (trace.fraction != 1)
//		VectorMA (trace->endpos, 4, trace->plane.normal, P_VEC(trace_endpos));
//	else
		VectorCopy (trace->endpos, P_VEC(trace_endpos));
	VectorCopy (trace->plane.normal, P_VEC(trace_plane_normal));
	pr_global_struct->trace_plane_dist =  trace->plane.dist;
	if (trace->ent)
		pr_global_struct->trace_ent = EDICT_TO_PROG(svprogfuncs, trace->ent);
	else
		pr_global_struct->trace_ent = EDICT_TO_PROG(svprogfuncs, sv.world.edicts);

	if (trace->startsolid)
		if (!sv_gameplayfix_honest_tracelines.ival)
			pr_global_struct->trace_fraction = 1;
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
void QCBUILTIN PF_svtraceline (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	*v1, *v2, *mins, *maxs;
	trace_t	trace;
	int		nomonsters;
	edict_t	*ent;

	v1 = G_VECTOR(OFS_PARM0);
	v2 = G_VECTOR(OFS_PARM1);
	nomonsters = G_FLOAT(OFS_PARM2);
	if (svprogfuncs->callargc == 3) // QTEST
		ent = PROG_TO_EDICT(prinst, pr_global_struct->self);
	else
		ent = G_EDICT(prinst, OFS_PARM3);

	if (sv_antilag.ival == 2)
		nomonsters |= MOVE_LAGGED;

	if (svprogfuncs->callargc == 6)
	{
		mins = G_VECTOR(OFS_PARM4);
		maxs = G_VECTOR(OFS_PARM5);
	}
	else
	{
		mins = vec3_origin;
		maxs = vec3_origin;
	}

	trace = World_Move (&sv.world, v1, mins, maxs, v2, nomonsters|MOVE_IGNOREHULL, (wedict_t*)ent);

	set_trace_globals(prinst, pr_globals, &trace);
}

#ifdef HEXEN2
static void QCBUILTIN PF_traceboxh2 (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	*v1, *v2, *mins, *maxs;
	trace_t	trace;
	int		nomonsters;
	edict_t	*ent;

	v1 = G_VECTOR(OFS_PARM0);
	v2 = G_VECTOR(OFS_PARM1);
	mins = G_VECTOR(OFS_PARM2);
	maxs = G_VECTOR(OFS_PARM3);
	nomonsters = G_FLOAT(OFS_PARM4);
	ent = G_EDICT(prinst, OFS_PARM5);

	trace = World_Move (&sv.world, v1, mins, maxs, v2, nomonsters|MOVE_IGNOREHULL, (wedict_t*)ent);

	set_trace_globals(prinst, pr_globals, &trace);
}
#endif

static void QCBUILTIN PF_traceboxdp (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	*v1, *v2, *mins, *maxs;
	trace_t	trace;
	int		nomonsters;
	edict_t	*ent;

	v1 = G_VECTOR(OFS_PARM0);
	mins = G_VECTOR(OFS_PARM1);
	maxs = G_VECTOR(OFS_PARM2);
	v2 = G_VECTOR(OFS_PARM3);
	nomonsters = G_FLOAT(OFS_PARM4);
	ent = G_EDICT(prinst, OFS_PARM5);

	trace = World_Move (&sv.world, v1, mins, maxs, v2, nomonsters|MOVE_IGNOREHULL, (wedict_t*)ent);

	set_trace_globals(prinst, pr_globals, &trace);
}

static void QCBUILTIN PF_TraceToss (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	trace_t	trace;
	edict_t	*ent;
	edict_t	*ignore;

	ent = G_EDICT(prinst, OFS_PARM0);
	if (ent == (edict_t*)sv.world.edicts)
		Con_DPrintf("tracetoss: can not use world entity\n");
	ignore = G_EDICT(prinst, OFS_PARM1);

	trace = WPhys_Trace_Toss (&sv.world, (wedict_t*)ent, (wedict_t*)ignore);

	set_trace_globals(prinst, pr_globals, &trace);
}

//============================================================================

qbyte	checkpvsbuffer[MAX_MAP_LEAFS/8];
qbyte	*checkpvs;
vec3_t	checkorg;
extern cvar_t sv_nopvs;

int PF_newcheckclient (pubprogfuncs_t *prinst, int check)
{
	int		i;
//	qbyte	*pvs;
	edict_t	*ent;
	int		cluster;

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
		if (i >= sv.allocated_client_slots+1)
			i = 1;

		ent = EDICT_NUM(prinst, i);

		if (i == check)
			break;	// didn't find anything else

		if (ED_ISFREE(ent))
			continue;
		if (ent->v->health <= 0)
			continue;
		if ((int)ent->v->flags & FL_NOTARGET)
			continue;

	// anything that is a client, or has a client as an enemy
		break;
	}

// get the PVS for the entity
	VectorAdd (ent->v->origin, ent->v->view_ofs, checkorg);
	if (sv.world.worldmodel->type == mod_heightmap || sv_nopvs.ival)
		checkpvs = NULL;
	else
	{
		cluster = sv.world.worldmodel->funcs.ClusterForPoint(sv.world.worldmodel, checkorg);
		checkpvs = sv.world.worldmodel->funcs.ClusterPVS (sv.world.worldmodel, cluster, checkpvsbuffer, sizeof(checkpvsbuffer));
	}

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
int PF_checkclient_Internal (pubprogfuncs_t *prinst)
{
	edict_t	*ent, *self;
	int		clust;
	vec3_t	view;
	vec3_t	dist;
	world_t *w = &sv.world;

// find a new check if on a new frame
	if (w->physicstime - w->lastchecktime >= 0.1)
	{
		w->lastcheck = PF_newcheckclient (prinst, w->lastcheck);
		w->lastchecktime = w->physicstime;
	}

// return check if it might be visible
	ent = EDICT_NUM(prinst, w->lastcheck);
	if (ED_ISFREE(ent) || ent->v->health <= 0)
	{
		return 0;
	}

// if current entity can't possibly see the check entity, return 0
	self = PROG_TO_EDICT(prinst, pr_global_struct->self);
	VectorAdd (self->v->origin, self->v->view_ofs, view);

	VectorSubtract(view, checkorg, dist);
	if (DotProduct(dist, dist) > 2048*2048)
		return 0;

	if (checkpvs)
	{
		clust = w->worldmodel->funcs.ClusterForPoint(w->worldmodel, view);
		if ( (clust<0) || !(checkpvs[clust>>3] & (1<<(clust&7)) ) )
		{
	c_notvis++;
			return 0;
		}
	}

// might be able to see it
c_invis++;
	return w->lastcheck;
}

static void QCBUILTIN PF_checkclient (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	RETURN_EDICT(prinst, EDICT_NUM(prinst, PF_checkclient_Internal(prinst)));
}

//============================================================================

void PF_stuffcmd_Internal(int entnum, const char *str, unsigned int flags)
{
	client_t	*cl;
	int		slen;

	if (entnum < 1 || entnum > sv.allocated_client_slots)
		return;

	cl = &svs.clients[entnum-1];

	if (strcmp(str, "disconnect\n") == 0)
	{
		// so long and thanks for all the fish
		if (cl->netchan.remote_address.type == NA_LOOPBACK)
			return;	//don't drop the local client. It looks wrong.
		cl->drop = true;
		return;
	}

#ifndef NOLEGACY
	//this block is a hack to 'fix' nq mods that expect all clients to support nq commands - but we're a qw engine.
	//FIXME: should buffer the entire command instead.
	if (progstype != PROG_QW)
	{
		static qboolean expectingcolour;
		if (!strncmp(str, "color ", 6)||!strncmp(str, ";color ", 7))	//okay, so this is a hack, but it fixes the qw scoreboard
		{
			expectingcolour = true;
			if (!strcmp(str, "color ")||!strcmp(str, ";color "))
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
			PF_ForceInfoKey_Internal(entnum, "team", tname);

			ClientReliableWrite_Begin (cl, svc_stufftext, 2+strlen("color "));
			ClientReliableWrite_String (cl, "color ");
		}
	}
#endif

	slen = strlen(str);

	if (!(flags & STUFFCMD_DEMOONLY))
		SV_StuffcmdToClient(cl, str);

	if (!(flags & STUFFCMD_IGNOREINDEMO))
	if (sv.mvdrecording)
	{
		sizebuf_t *msg = MVDWrite_Begin (dem_single, entnum - 1, 2 + slen);
		MSG_WriteByte (msg, svc_stufftext);
		MSG_WriteString (msg, str);
	}

	//this seems a little dangerous. v_cshift could leave a spectator's machine unusable if they switch players at unfortunate times.
	if (!(flags & STUFFCMD_DEMOONLY))
	if (sv_specprint.ival & SPECPRINT_STUFFCMD)
	{
		client_t *spec;
		unsigned int i;
		for (i = 0, spec = svs.clients; i < sv.allocated_client_slots; i++, spec++)
		{
			if (spec->state != cs_spawned || !spec->spectator)
				continue;
			if (spec->spec_track == entnum && (spec->spec_print & SPECPRINT_STUFFCMD))
				SV_StuffcmdToClient(spec, str);
		}
	}
}

/*
=================
PF_stuffcmd

Sends text over to the client's execution buffer

stuffcmd (clientent, value)
=================
*/
static void QCBUILTIN PF_stuffcmd (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	PF_stuffcmd_Internal(G_EDICTNUM(prinst, OFS_PARM0), PR_GetStringOfs(prinst, OFS_PARM1), 0);
}

static void QCBUILTIN PF_stuffcmdflags (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	PF_stuffcmd_Internal(G_EDICTNUM(prinst, OFS_PARM0), PR_GetStringOfs(prinst, OFS_PARM2), G_FLOAT(OFS_PARM1));
}

//DP_QC_DROPCLIENT
static void QCBUILTIN PF_dropclient (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int		entnum;
	client_t	*cl;

	entnum = G_EDICTNUM(prinst, OFS_PARM0);
	if (entnum < 1 || entnum > sv.allocated_client_slots)
		return;

	cl = &svs.clients[entnum-1];

	// so long and thanks for all the fish
	if (cl->netchan.remote_address.type == NA_LOOPBACK)
	{
		Cbuf_AddText ("disconnect\n", RESTRICT_INSECURE);
		return;	//don't drop the local client. It looks wrong.
	}
	cl->drop = true;
	return;
}



//DP_SV_BOTCLIENT
//entity() spawnclient = #454;
static void QCBUILTIN PF_spawnclient (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i;
	for (i = 0; i < sv.allocated_client_slots; i++)
	{
		if (!*svs.clients[i].name && !svs.clients[i].protocol && svs.clients[i].state == cs_free)
		{
			svs.clients[i].protocol = SCP_BAD;	//marker for bots
			svs.clients[i].state = cs_spawned;
			sv.spawned_client_slots++;
			svs.clients[i].netchan.message.allowoverflow = true;
			svs.clients[i].netchan.message.maxsize = 0;
			svs.clients[i].datagram.allowoverflow = true;
			svs.clients[i].datagram.maxsize = 0;

			svs.clients[i].edict = EDICT_NUM(prinst, i+1);

			SV_SetUpClientEdict (&svs.clients[i], svs.clients[i].edict);

			RETURN_EDICT(prinst, svs.clients[i].edict);
			return;
		}
	}
	RETURN_EDICT(prinst, sv.world.edicts);
}

//DP_SV_BOTCLIENT
//float(entity client) clienttype = #455;
static void QCBUILTIN PF_clienttype (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int entnum = G_EDICTNUM(prinst, OFS_PARM0);
	if (entnum < 1 || entnum > sv.allocated_client_slots)
	{
		G_FLOAT(OFS_RETURN) = CLIENTTYPE_NOTACLIENT;	//not a client slot
		return;
	}
	entnum--;
	if (svs.clients[entnum].state < cs_connected)
	{
		G_FLOAT(OFS_RETURN) = CLIENTTYPE_DISCONNECTED;	//disconnected
		return;
	}
	if (svs.clients[entnum].protocol == SCP_BAD)
		G_FLOAT(OFS_RETURN) = CLIENTTYPE_BOT;	//an active, bot client.
	else
		G_FLOAT(OFS_RETURN) = CLIENTTYPE_REAL;	//an active, not-bot client.
}

/*
=================
PF_cvar

float cvar (string)
=================
*/
static void QCBUILTIN PF_cvar (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char	*str;

	str = PR_GetStringOfs(prinst, OFS_PARM0);

	if (!strcmp(str, "pr_checkextension"))	//no console changing
		G_FLOAT(OFS_RETURN) = PR_EnableEBFSBuiltin("checkextension", 0);
	else if (!strcmp(str, "pr_builtin_find"))
		G_FLOAT(OFS_RETURN) = PR_EnableEBFSBuiltin("builtin_find", 0);
	else if (!strcmp(str, "pr_map_builtin"))
		G_FLOAT(OFS_RETURN) = PR_EnableEBFSBuiltin("map_builtin", 0);
	else if (!strcmp(str, "halflifebsp"))
		G_FLOAT(OFS_RETURN) = sv.world.worldmodel->fromgame == fg_halflife;
	else
	{
		cvar_t *cv = Cvar_FindVar(str);
		if (!cv)
		{
			//this little chunk is so cvars dp creates are created with meaningful values
			char *def = "";
			if (!strcmp(str, "sv_maxairspeed"))
				def = "30";
			else if (!strcmp(str, "sv_jumpvelocity"))
				def = "270";
			else
				def = "";

			cv = Cvar_Get(str, def, 0, "QC variables");
			Con_Printf("^3Creating cvar %s\n", str);
		}
		if (cv->flags & CVAR_NOUNSAFEEXPAND)
			G_FLOAT(OFS_RETURN) = 0;
		else
			G_FLOAT(OFS_RETURN) = cv->value;
	}
}

static void QCBUILTIN PF_sv_getlight (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	/*not shared with client - clients get more lights*/
	float *point = G_VECTOR(OFS_PARM0);
	vec3_t diffuse, ambient, dir;
	if (sv.world.worldmodel && sv.world.worldmodel->funcs.LightPointValues)
	{
		sv.world.worldmodel->funcs.LightPointValues(sv.world.worldmodel, point, diffuse, ambient, dir);
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

#ifndef QUAKETC
/*
=========
PF_conprint
=========
*/
static void QCBUILTIN PF_conprint (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Sys_Printf ("%s",PF_VarString(prinst, 0, pr_globals));
}
#endif

#ifdef HEXEN2
//dprintf("foo %s\n", 5.0) - its stupid and potentially unsafe
static void QCBUILTIN PF_h2dprintf (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char temp[256];
	char printable[2048];
	char *pct;

	sprintf (temp, "%g", G_FLOAT(OFS_PARM1));

	Q_strncpyz(printable, PR_GetStringOfs(prinst, OFS_PARM0), sizeof(printable));
	while((pct = strstr(printable, "%s")))
	{
		if ((pct-printable) + strlen(temp) + strlen(pct) > sizeof(printable))
			break;
		memmove(pct + strlen(temp), pct+2, strlen(pct+2)+1);
		memcpy(pct, temp, strlen(temp));
	}
	Con_DPrintf ("%s", printable);
}

static void QCBUILTIN PF_h2dprintv (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char temp[256];
	char printable[2048];
	char *pct;

	sprintf (temp, "'%g %g %g'", G_VECTOR(OFS_PARM1)[0], G_VECTOR(OFS_PARM1)[1], G_VECTOR(OFS_PARM1)[2]);

	Q_strncpyz(printable, PR_GetStringOfs(prinst, OFS_PARM0), sizeof(printable));
	while((pct = strstr(printable, "%s")))
	{
		if ((pct-printable) + strlen(temp) + strlen(pct) > sizeof(printable))
			break;
		memmove(pct + strlen(temp), pct+2, strlen(pct+2)+1);
		memcpy(pct, temp, strlen(temp));
	}
	Con_DPrintf ("%s", printable);
}

static void QCBUILTIN PF_h2spawn_temp (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*ed;
	ed = ED_Alloc(prinst, false, 0);
	RETURN_EDICT(prinst, ed);
}
#endif

static void QCBUILTIN PF_Remove (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*ed;

	ed = G_EDICT(prinst, OFS_PARM0);

	if (ED_ISFREE(ed))
	{
		ED_CanFree(ed);	//fake it
		if (developer.value)
		{
			Con_Printf("Tried removing free entity at:\n");
			PR_StackTrace(prinst, false);
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

static int SV_ParticlePrecache_Add(const char *pname)
{
	int i;
	for (i=1 ; i<MAX_SSPARTICLESPRE ; i++)
	{
		if (!sv.strings.particle_precache[i])
		{
			sv.strings.particle_precache[i] = PR_AddString(sv.world.progs, pname, 0, false);

			if (sv.state != ss_loading)
			{
				Con_DPrintf("Delayed particle precache: %s\n", pname);
				MSG_WriteByte(&sv.multicast, svcfte_precache);
				MSG_WriteShort(&sv.multicast, i|PC_PARTICLE);
				MSG_WriteString(&sv.multicast, pname);
#ifdef NQPROT
				MSG_WriteByte(&sv.nqmulticast, svcdp_precache);
				MSG_WriteShort(&sv.nqmulticast, i|PC_PARTICLE);
				MSG_WriteString(&sv.nqmulticast, pname);
#endif

				SV_MulticastProtExt(vec3_origin, MULTICAST_ALL_R, pr_global_struct->dimension_send, PEXT_CSQC, 0);
			}
		}
		if (!strcmp(sv.strings.particle_precache[i], pname))
		{
			return i;
		}
	}
	return 0;
}

//float(string effectname) particleeffectnum (EXT_CSQC)
void QCBUILTIN PF_sv_particleeffectnum(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *s = PR_GetStringOfs(prinst, OFS_PARM0);
	int		i;

	G_FLOAT(OFS_RETURN) = 0;

	if (s[0] <= ' ')
	{
		/*if (!ssqc_deprecated_warned)
		{
			PR_RunWarning(prinst, "PF_precache_particles: Bad string");
			ssqc_deprecated_warned = true;
		}*/
		return;
	}

#ifdef NQPROT
	//DPP7's network protocol depends upon the ordering of these from an external file. unreliable, but if we're meant to be compatible then we need to at least pretend.
	if (!sv.strings.particle_precache[1] && sv_listen_dp.ival)
		COM_Effectinfo_Enumerate(SV_ParticlePrecache_Add);
#endif

	i = SV_ParticlePrecache_Add(s);
	G_FLOAT(OFS_RETURN) = i;
	if (!i)
		PR_BIError (prinst, "PF_precache_particles: overflow");
}

static void QCBUILTIN PF_precache_file (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{	// precache_file is only used to copy files with qcc, it does nothing
	const char	*s = PR_GetStringOfs(prinst, OFS_PARM0);

	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);

	/*touch the file, so any packs will be referenced. this is fte-specific behaviour.*/
	FS_FLocateFile(s, FSLF_IFFOUND, NULL);
}

int PF_precache_sound_Internal (pubprogfuncs_t *prinst, const char *s)
{
	int		i;

	if (s[0] <= ' ')
	{
		PR_BIError (prinst, "PF_precache_sound: Bad string");
		return 0;
	}

	for (i=1 ; i<MAX_PRECACHE_SOUNDS ; i++)
	{
		if (!sv.strings.sound_precache[i])
		{
			sv.strings.sound_precache[i] = PR_AddString(prinst, s, 0, false);

			/*touch the file, so any packs will be referenced*/
			FS_FLocateFile(s, FSLF_IFFOUND, NULL);

			if (sv.state != ss_loading)
			{
				Con_DPrintf("Delayed sound precache: %s\n", s);
				MSG_WriteByte(&sv.reliable_datagram, svcfte_precache);
				MSG_WriteShort(&sv.reliable_datagram, i+PC_SOUND);
				MSG_WriteString(&sv.reliable_datagram, s);
#ifdef NQPROT
				MSG_WriteByte(&sv.nqreliable_datagram, svcdp_precache);
				MSG_WriteShort(&sv.nqreliable_datagram, i+PC_SOUND);
				MSG_WriteString(&sv.nqreliable_datagram, s);
#endif
			}
			return i;
		}
		if (!strcmp(sv.strings.sound_precache[i], s))
			return i;
	}
	PR_BIError (prinst, "PF_precache_sound: overflow");
	return 0;
}
static void QCBUILTIN PF_precache_sound (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char	*s;

	s = PR_GetStringOfs(prinst, OFS_PARM0);

	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);

	PF_precache_sound_Internal(prinst, s);
}

int PF_precache_model_Internal (pubprogfuncs_t *prinst, const char *s, qboolean queryonly)
{
	int		i;

	if (s[0] <= ' ')
	{
		Con_DPrintf ("precache_model: empty string\n");
		return 0;
	}

	for (i=1 ; i<MAX_PRECACHE_MODELS ; i++)
	{
		if (!sv.strings.model_precache[i])
		{
			if (strlen(s)>=MAX_QPATH-1)	//probably safest to keep this.
			{
				PR_BIError (prinst, "Precache name too long");
				return 0;
			}
			if (queryonly)
				return 0;
#ifdef VM_Q1
			if (svs.gametype == GT_Q1QVM)
				sv.strings.model_precache[i] = s;
			else
#endif
				sv.strings.model_precache[i] = PR_AddString(prinst, s, 0, false);
			s = sv.strings.model_precache[i];
			if (!strcmp(s + strlen(s) - 4, ".bsp") || sv_gameplayfix_setmodelrealbox.ival)
				sv.models[i] = Mod_ForName(Mod_FixName(s, sv.modelname), MLV_WARNSYNC);
			else
			{
				/*touch the file, so any packs will be referenced*/
				FS_FLocateFile(s, FSLF_IFFOUND, NULL);
			}

			if (sv.state != ss_loading)
			{
				Con_DPrintf("Delayed model precache: %s\n", s);
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
static void QCBUILTIN PF_precache_model (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char	*s;

	s = PR_GetStringOfs(prinst, OFS_PARM0);

	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);

	PF_precache_model_Internal(prinst, s, false);
}

#ifdef HEXEN2
static void QCBUILTIN PF_h2precache_puzzle_model (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{	//qc/hc lacks string manipulation.
	const char *shortname;
	char fullname[MAX_QPATH];
	shortname = PR_GetStringOfs(prinst, OFS_PARM0);
	snprintf(fullname, sizeof(fullname)-1, "models/puzzle/%s.mdl", shortname);

	PF_precache_model_Internal(prinst, fullname, false);
}
#endif

static void QCBUILTIN PF_getmodelindex (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char	*s = PR_GetStringOfs(prinst, OFS_PARM0);
	qboolean queryonly = (svprogfuncs->callargc >= 2)?G_FLOAT(OFS_PARM1):false;

	G_FLOAT(OFS_RETURN) = PF_precache_model_Internal(prinst, s, queryonly);
}
void QCBUILTIN PF_precache_vwep_model (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i;
	const char	*s;

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
					Con_Printf("PF_precache_vwep_model: not spawn-time\n");
					G_FLOAT(OFS_RETURN) = 0;
					return;
				}
				sv.strings.vw_model_precache[i] = PR_AddString(prinst, s, 0, false);
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

// warning: ‘PF_svcoredump’ defined but not used
/*
static void QCBUILTIN PF_svcoredump (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int size = 1024*1024*8;
	char *buffer = BZ_Malloc(size);
	prinst->save_ents(prinst, buffer, &size, 3);
	COM_WriteFile("ssqccore.txt", buffer, size);
	BZ_Free(buffer);
}
*/

static void QCBUILTIN PF_sv_movetogoal (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	wedict_t	*ent;
	float dist;
	ent = (wedict_t*)PROG_TO_EDICT(prinst, pr_global_struct->self);
	dist = G_FLOAT(OFS_PARM0);
	World_MoveToGoal (&sv.world, ent, dist);
}

/*
===============
PF_walkmove

float(float yaw, float dist) walkmove
===============
*/
static void QCBUILTIN PF_walkmove (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	wedict_t	*ent;
	float	yaw, dist;
	vec3_t	move;
//	dfunction_t	*oldf;
	int 	oldself;
	qboolean settrace;
	vec3_t	axis[3];
	float s;

	ent = PROG_TO_WEDICT(prinst, pr_global_struct->self);
	yaw = G_FLOAT(OFS_PARM0);
	dist = G_FLOAT(OFS_PARM1);
	if (svprogfuncs->callargc >= 3 && G_FLOAT(OFS_PARM2))
		settrace = true;
	else
		settrace = false;

	if ( !( (int)ent->v->flags & (FL_ONGROUND|FL_FLY|FL_SWIM) ) )
	{
		G_FLOAT(OFS_RETURN) = 0;
		return;
	}

	yaw = yaw*M_PI*2 / 360;
	World_GetEntGravityAxis(ent, axis);

	s = cos(yaw)*dist;
	VectorScale(axis[0], s, move);
	s = sin(yaw)*dist;
	VectorMA(move, s, axis[1], move);

// save program state, because SV_movestep may call other progs
//	oldf = pr_xfunction;
	oldself = pr_global_struct->self;

//	if (!dist)
//	{
//		G_FLOAT(OFS_RETURN) = !SV_TestEntityPosition(ent);
//	}
//	else if (!SV_TestEntityPosition(ent))
//	{
		G_FLOAT(OFS_RETURN) = World_movestep(&sv.world, (wedict_t*)ent, move, axis, true, false, settrace?set_trace_globals:NULL, pr_globals);
//		if (SV_TestEntityPosition(ent))
//			Con_Printf("Entity became stuck\n");
//	}


// restore program state
//	pr_xfunction = oldf;
	pr_global_struct->self = oldself;
}

void QCBUILTIN PF_applylightstyle(int style, const char *val, vec3_t rgb)
{
	client_t	*client;
	int			j;

	if (style < 0 || style >= MAX_LIGHTSTYLES)
	{
		Con_Printf("WARNING: Bad lightstyle %i.\n", style);
		return;
	}
	if (strlen(val) >= 64)
		Con_Printf("WARNING: Style string is longer than standard (%i). Some clients could crash.\n", 63);


// change the string in sv
	if (sv.strings.lightstyles[style])
		BZ_Free((void*)sv.strings.lightstyles[style]);
	sv.strings.lightstyles[style] = Z_StrDup(val);

#ifdef PEXT_LIGHTSTYLECOL
	VectorCopy(rgb, sv.lightstylecolours[style]);
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
			if (style >= MAX_STANDARDLIGHTSTYLES)	//only bug out clients if the styles are needed
				if (!*val)
					continue;
#ifdef PEXT_LIGHTSTYLECOL
			if ((client->fteprotocolextensions & PEXT_LIGHTSTYLECOL) && (rgb[0] != 1 || rgb[1] != 1 || rgb[2] != 1))
			{
				ClientReliableWrite_Begin (client, svcfte_lightstylecol, 3+6+strlen(val)+1);
				ClientReliableWrite_Byte (client, style);
				ClientReliableWrite_Char (client, 0x87);
				ClientReliableWrite_Short (client, rgb[0]*1024);
				ClientReliableWrite_Short (client, rgb[1]*1024);
				ClientReliableWrite_Short (client, rgb[2]*1024);
				ClientReliableWrite_String (client, val);
			}
			else
#endif
			{
				ClientReliableWrite_Begin (client, svc_lightstyle, strlen(val)+3);
				ClientReliableWrite_Byte (client, style);
				ClientReliableWrite_String (client, val);
			}
		}
	}

	if (sv.mvdrecording)
	{
		if (style < MAX_STANDARDLIGHTSTYLES || *val)
		{
			sizebuf_t *msg = MVDWrite_Begin(dem_all, 0, 3+6+strlen(val)+1);
#ifdef PEXT_LIGHTSTYLECOL
			if ((demo.recorder.fteprotocolextensions & PEXT_LIGHTSTYLECOL) && (rgb[0] != 1 || rgb[1] != 1 || rgb[2] != 1))
			{
				MSG_WriteByte (msg, svcfte_lightstylecol);
				MSG_WriteByte (msg, style);
				MSG_WriteChar (msg, 0x87);
				MSG_WriteShort (msg, rgb[0]*1024);
				MSG_WriteShort (msg, rgb[1]*1024);
				MSG_WriteShort (msg, rgb[2]*1024);
				MSG_WriteString (msg, val);
			}
			else
#endif
			{
				MSG_WriteByte (msg, svc_lightstyle);
				MSG_WriteByte (msg, style);
				MSG_WriteString (msg, val);
			}
		}
	}
}

/*
===============
PF_lightstyle

void(float style, string value [, float colour]) lightstyle
===============
*/
static void QCBUILTIN PF_lightstyle (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int		style;
	const char	*val;
	vec3_t rgb = {1,1,1};

#ifdef PEXT_LIGHTSTYLECOL
	if (svprogfuncs->callargc >= 3)
		VectorCopy(G_VECTOR(OFS_PARM2), rgb);
#endif

	style = G_FLOAT(OFS_PARM0);
	val = PR_GetStringOfs(prinst, OFS_PARM1);

	PF_applylightstyle(style, val, rgb);
}

static void QCBUILTIN PF_getlightstyle (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	unsigned int		style = G_FLOAT(OFS_PARM0);

	if (style >= countof(sv.strings.lightstyles))
	{
		VectorSet(G_VECTOR(OFS_PARM1), 0, 0, 0);
		G_INT(OFS_RETURN) = 0;
	}
	else
	{
		VectorCopy(sv.lightstylecolours[style], G_VECTOR(OFS_PARM1));
		if (!sv.strings.lightstyles[style])
			G_INT(OFS_RETURN) = 0;
		else
			RETURN_TSTRING(sv.strings.lightstyles[style]);
	}
}
static void QCBUILTIN PF_getlightstylergb (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	unsigned int		style = G_FLOAT(OFS_PARM0);
	int value;
	if (!sv.strings.lightstyles[style])
		value = ('m'-'a')*22;
	else if (sv.strings.lightstyles[style][0] == '=')
		value = atof(sv.strings.lightstyles[style]+1)*256;
	else
		value = sv.strings.lightstyles[style][max(0,(int)(sv.time*10)) % strlen(sv.strings.lightstyles[style])] - 'a';
	VectorScale(sv.lightstylecolours[style], value*(1.0/256), G_VECTOR(OFS_RETURN));
}

#ifdef HEXEN2
static void QCBUILTIN PF_lightstylevalue (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
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

static void QCBUILTIN PF_lightstylestatic (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int		style;
	int	num;
	char	val[2];
	vec3_t rgb = {1,1,1};

	style = G_FLOAT(OFS_PARM0);
	num = G_FLOAT(OFS_PARM1);
#ifdef PEXT_LIGHTSTYLECOL
	if (svprogfuncs->callargc >= 3)
		VectorCopy(G_VECTOR(OFS_PARM2), rgb);
#endif

	//with fte+dp, va("=%g", (num*2.0)/26) should work
	//but will break other clients. so that's a problem.

	val[0] = 'a' + bound(0, num, ('z'-'a')-1);
	val[1] = 0;
	PF_applylightstyle(style, val, rgb);
}
#endif

/*
=============
PF_pointcontents
=============
*/
static void QCBUILTIN PF_pointcontents (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *w = prinst->parms->user;

	float	*v;
	int cont;

	v = G_VECTOR(OFS_PARM0);

	cont = World_PointContents(w, v);
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
	else if (cont & FTECONTENTS_LADDER)
		G_FLOAT(OFS_RETURN) = Q1CONTENTS_LADDER;
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
cvar_t	sv_aim = CVAR("sv_aim", "2");
static void QCBUILTIN PF_aim (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*ent, *check, *bestent;
	vec3_t	start, dir, end, bestdir;
	int		i, j;
	trace_t	tr;
	float	dist, bestdist = sv_aim.value;
	char	*noaim;

	ent = G_EDICT(prinst, OFS_PARM0);
//	speed = G_FLOAT(OFS_PARM1);

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

		noaim = Info_ValueForKey (svs.clients[i-1].userinfo, "aim");
		if (noaim)
		{
			dist = atof(noaim);
			if (dist > 0)
				bestdist = dist;
		}
	}

// try sending a trace straight
	VectorCopy (P_VEC(v_forward), dir);
	VectorMA (start, 2048, dir, end);
	tr = World_Move (&sv.world, start, vec3_origin, vec3_origin, end, false, (wedict_t*)ent);
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

	for (i=1 ; i<sv.world.num_edicts ; i++ )
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
		tr = World_Move (&sv.world, start, vec3_origin, vec3_origin, end, false, (wedict_t*)ent);
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
		if (entnum < 1 || entnum > sv.allocated_client_slots)
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
		if (entnum < 1 || entnum > sv.allocated_client_slots)
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
#else
static sizebuf_t *NQWriteDest (int dest)
{
	return QWWriteDest(dest);
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
void QCBUILTIN PF_WriteByte (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int dest = G_FLOAT(OFS_PARM0);
	qbyte val = 0xff & (int)G_FLOAT(OFS_PARM1);
	if (dest == MSG_CSQC)
	{	//csqc buffers are always written.
		MSG_WriteByte(&csqcmsgbuffer, val);
		return;
	}

	if (pr_nonetaccess.value)
		return;

#ifdef SERVER_DEMO_PLAYBACK
	if (sv.demofile)
		return;
#endif

#ifdef NETPREPARSE
	if (dpcompat_nopreparse.ival)
		;
	else if (progstype != PROG_QW)
	{
		NPP_NQWriteByte(dest, val);
		return;
	}
#ifdef NQPROT
	else
	{
		NPP_QWWriteByte(dest, val);
		return;
	}
#endif
#endif
	if (dest == MSG_ONE)
	{	//WARNING: THIS IS BUGGY. DO NOT MAKE MODS THAT TAKE THIS PATH
		client_t *cl = Write_GetClient();
		if (!cl)
			return;
		ClientReliableCheckBlock(cl, 1);
		ClientReliableWrite_Byte(cl, val);
	}
	else
	{
		if (progstype != PROG_QW)
			MSG_WriteByte (NQWriteDest(dest), val);
		else
			MSG_WriteByte (QWWriteDest(dest), val);
	}
}

void QCBUILTIN PF_WriteChar (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int dest = G_FLOAT(OFS_PARM0);
	char val = 0xff & (int)G_FLOAT(OFS_PARM1);
	if (dest == MSG_CSQC)
	{	//csqc buffers are always written.
		MSG_WriteChar(&csqcmsgbuffer, val);
		return;
	}

	if (pr_nonetaccess.value)
		return;
#ifdef SERVER_DEMO_PLAYBACK
	if (sv.demofile)
		return;
#endif

#ifdef NETPREPARSE
	if (dpcompat_nopreparse.ival)
		;
	else if (progstype != PROG_QW)
	{
		NPP_NQWriteChar(dest, val);
		return;
	}
#ifdef NQPROT
	else
	{
		NPP_QWWriteChar(dest, val);
		return;
	}
#endif
#endif
	if (dest == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		if (!cl)
			return;
		ClientReliableCheckBlock(cl, 1);
		ClientReliableWrite_Char(cl, val);
	}
	else
	{
		if (progstype != PROG_QW)
			MSG_WriteChar (NQWriteDest(dest), val);
		else
			MSG_WriteChar (QWWriteDest(dest), val);
	}
}

void QCBUILTIN PF_WriteShort (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int dest = G_FLOAT(OFS_PARM0);
	short val = (((int)G_FLOAT(OFS_PARM1))&0xffff);
	if (dest == MSG_CSQC)
	{	//csqc buffers are always written.
		MSG_WriteShort(&csqcmsgbuffer, val);
		return;
	}

	if (pr_nonetaccess.value)
		return;
#ifdef SERVER_DEMO_PLAYBACK
	if (sv.demofile)
		return;
#endif

#ifdef NETPREPARSE
	if (dpcompat_nopreparse.ival)
		;
	else if (progstype != PROG_QW)
	{
		NPP_NQWriteShort(dest, val);
		return;
	}
#ifdef NQPROT
	else
	{
		NPP_QWWriteShort(dest, val);
		return;
	}
#endif
#endif

	if (dest == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		if (!cl)
			return;
		ClientReliableCheckBlock(cl, 2);
		ClientReliableWrite_Short(cl, val);
	}
	else
	{
		if (progstype != PROG_QW)
			MSG_WriteShort (NQWriteDest(dest), val);
		else
			MSG_WriteShort (QWWriteDest(dest), val);
	}
}

void QCBUILTIN PF_WriteLong (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int dest = G_FLOAT(OFS_PARM0);
	if (dest == MSG_CSQC)
	{	//csqc buffers are always written.
		MSG_WriteLong(&csqcmsgbuffer, G_FLOAT(OFS_PARM1));
		return;
	}

	if (pr_nonetaccess.value)
		return;
#ifdef SERVER_DEMO_PLAYBACK
	if (sv.demofile)
		return;
#endif

#ifdef NETPREPARSE
	if (dpcompat_nopreparse.ival)
		;
	else if (progstype != PROG_QW)
	{
		NPP_NQWriteLong(dest, G_FLOAT(OFS_PARM1));
		return;
	}
#ifdef NQPROT
	else
	{
		NPP_QWWriteLong(dest, G_FLOAT(OFS_PARM1));
		return;
	}
#endif
#endif

	if (dest == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		if (!cl)
			return;
		ClientReliableCheckBlock(cl, 4);
		ClientReliableWrite_Long(cl, G_FLOAT(OFS_PARM1));
	}
	else
	{
		if (progstype != PROG_QW)
			MSG_WriteLong (NQWriteDest(dest), G_FLOAT(OFS_PARM1));
		else
			MSG_WriteLong (QWWriteDest(dest), G_FLOAT(OFS_PARM1));
	}
}

void QCBUILTIN PF_WriteAngle (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int dest = G_FLOAT(OFS_PARM0);
	if (dest == MSG_CSQC)
	{	//csqc buffers are always written.
		MSG_WriteAngle(&csqcmsgbuffer, G_FLOAT(OFS_PARM1));
		return;
	}

	if (pr_nonetaccess.value)
		return;
#ifdef SERVER_DEMO_PLAYBACK
	if (sv.demofile)
		return;
#endif

#ifdef NETPREPARSE
	if (dpcompat_nopreparse.ival)
		;
	else if (progstype != PROG_QW)
	{
		NPP_NQWriteAngle(dest, G_FLOAT(OFS_PARM1));
		return;
	}
#ifdef NQPROT
	else
	{
		NPP_QWWriteAngle(dest, G_FLOAT(OFS_PARM1));
		return;
	}
#endif
#endif

	if (dest == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		if (!cl)
			return;
		ClientReliableCheckBlock(cl, 4);
		ClientReliableWrite_Angle(cl, G_FLOAT(OFS_PARM1));
	}
	else
	{
		if (progstype != PROG_QW)
			MSG_WriteAngle (NQWriteDest(dest), G_FLOAT(OFS_PARM1));
		else
			MSG_WriteAngle (QWWriteDest(dest), G_FLOAT(OFS_PARM1));
	}
}

void QCBUILTIN PF_WriteCoord (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int dest = G_FLOAT(OFS_PARM0);
	if (dest == MSG_CSQC)
	{	//csqc buffers are always written.
		MSG_WriteCoord(&csqcmsgbuffer, G_FLOAT(OFS_PARM1));
		return;
	}

	if (pr_nonetaccess.value)
		return;
#ifdef SERVER_DEMO_PLAYBACK
	if (sv.demofile)
		return;
#endif

#ifdef NETPREPARSE
	if (dpcompat_nopreparse.ival)
		;
	else if (progstype != PROG_QW)
	{
		NPP_NQWriteCoord(dest, G_FLOAT(OFS_PARM1));
		return;
	}
#ifdef NQPROT
	else
	{
		NPP_QWWriteCoord(dest, G_FLOAT(OFS_PARM1));
		return;
	}
#endif
#endif

	if (dest == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		if (!cl)
			return;
		ClientReliableCheckBlock(cl, 2);
		ClientReliableWrite_Coord(cl, G_FLOAT(OFS_PARM1));
	}
	else
	{
		if (progstype != PROG_QW)
			MSG_WriteCoord (NQWriteDest(dest), G_FLOAT(OFS_PARM1));
		else
			MSG_WriteCoord (QWWriteDest(dest), G_FLOAT(OFS_PARM1));
	}
}

void QCBUILTIN PF_WriteFloat (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int dest = G_FLOAT(OFS_PARM0);
	if (dest == MSG_CSQC)
	{	//csqc buffers are always written.
		MSG_WriteFloat(&csqcmsgbuffer, G_FLOAT(OFS_PARM1));
		return;
	}

	if (pr_nonetaccess.value)
		return;
#ifdef SERVER_DEMO_PLAYBACK
	if (sv.demofile)
		return;
#endif

#ifdef NETPREPARSE
	if (dpcompat_nopreparse.ival)
		;
	else if (progstype != PROG_QW)
	{
		NPP_NQWriteFloat(dest, G_FLOAT(OFS_PARM1));
		return;
	}
#ifdef NQPROT
	else
	{
		NPP_QWWriteFloat(dest, G_FLOAT(OFS_PARM1));
		return;
	}
#endif
#endif

	if (dest == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		if (!cl)
			return;
		ClientReliableCheckBlock(cl, 4);
		ClientReliableWrite_Float(cl, G_FLOAT(OFS_PARM1));
	}
	else
	{
		if (progstype != PROG_QW)
			MSG_WriteFloat (NQWriteDest(dest), G_FLOAT(OFS_PARM1));
		else
			MSG_WriteFloat (QWWriteDest(dest), G_FLOAT(OFS_PARM1));
	}
}

void PF_WriteString_Internal (int target, const char *str)
{
	if (target == MSG_CSQC)
	{	//csqc buffers are always written.
		MSG_WriteString(&csqcmsgbuffer, str);
		return;
	}

	if (pr_nonetaccess.value
#ifdef SERVER_DEMO_PLAYBACK
		|| sv.demofile
#endif
		)
		return;

#ifdef NETPREPARSE
	if (dpcompat_nopreparse.ival)
		;
	else if (progstype != PROG_QW)
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
#endif
#endif

	if (target == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		if (!cl)
			return;
		ClientReliableCheckBlock(cl, 1+strlen(str));
		ClientReliableWrite_String(cl, str);
	}
	else
	{
		if (progstype != PROG_QW)
			MSG_WriteString (NQWriteDest(target), str);
		else
			MSG_WriteString (QWWriteDest(target), str);
	}
}

static void QCBUILTIN PF_WriteString (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *str = PF_VarString(prinst, 1, pr_globals);
	PF_WriteString_Internal(G_FLOAT(OFS_PARM0), str);
}

static void QCBUILTIN PF_WritePicture (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	//name size data
	//this is basically a stub, so we write a size of 0. the client will have to just deal with it.
	int target = G_FLOAT(OFS_PARM0);
	string_t o = G_INT(OFS_PARM1);
	float sizelimit = G_INT(OFS_PARM2);

	(void)sizelimit;	//we don't use this, because we don't bother trying to re-compress the thing here.

	PF_WriteString_Internal(target, PR_GetString(prinst, o));
	G_FLOAT(OFS_PARM1) = 0;
	PF_WriteShort(prinst, pr_globals);

	G_INT(OFS_PARM1) = o;
}

void QCBUILTIN PF_WriteEntity (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int dest = G_FLOAT(OFS_PARM0);
	if (dest == MSG_CSQC)
	{	//csqc buffers are always written.
		MSG_WriteEntity(&csqcmsgbuffer, G_EDICTNUM(prinst, OFS_PARM1));
		return;
	}

	if (pr_nonetaccess.value
#ifdef SERVER_DEMO_PLAYBACK
		|| sv.demofile
#endif
		)
		return;

#ifdef NETPREPARSE
	if (dpcompat_nopreparse.ival)
		;
	else if (progstype != PROG_QW)
	{
		NPP_NQWriteEntity(dest, G_EDICTNUM(prinst, OFS_PARM1));
		return;
	}
#ifdef NQPROT
	else
	{
		NPP_QWWriteEntity(dest, G_EDICTNUM(prinst, OFS_PARM1));
		return;
	}
#endif
#endif

	if (dest == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		if (!cl)
			return;
		ClientReliableCheckBlock(cl, 2);
		ClientReliableWrite_Entity(cl, G_EDICTNUM(prinst, OFS_PARM1));
	}
	else
	{
		if (progstype != PROG_QW)
			MSG_WriteEntity (NQWriteDest(dest), G_EDICTNUM(prinst, OFS_PARM1));
		else
			MSG_WriteEntity (QWWriteDest(dest), G_EDICTNUM(prinst, OFS_PARM1));
	}
}

//small wrapper function.
//void(float target, string str, ...) WriteString2 = #33;
static void QCBUILTIN PF_WriteString2 (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int old;
	char *str;

	if (G_FLOAT(OFS_PARM0) != MSG_CSQC && (pr_nonetaccess.value
#ifdef SERVER_DEMO_PLAYBACK
		|| sv.demofile
#endif
		))
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

#ifndef QUAKETC
//qtest-only builtins.
static void QCBUILTIN PF_qtSingle_WriteByte (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteByte(MSG_PRERELONE, (qbyte)G_FLOAT(OFS_PARM1));
}
static void QCBUILTIN PF_qtSingle_WriteChar (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteChar(MSG_PRERELONE, (char)G_FLOAT(OFS_PARM1));
}
static void QCBUILTIN PF_qtSingle_WriteShort (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteShort(MSG_PRERELONE, (short)G_FLOAT(OFS_PARM1));
}
static void QCBUILTIN PF_qtSingle_WriteLong (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteLong(MSG_PRERELONE, G_FLOAT(OFS_PARM1));
}
static void QCBUILTIN PF_qtSingle_WriteAngle (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteAngle(MSG_PRERELONE, G_FLOAT(OFS_PARM1));
}
static void QCBUILTIN PF_qtSingle_WriteCoord (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteCoord(MSG_PRERELONE, G_FLOAT(OFS_PARM1));
}
static void QCBUILTIN PF_qtSingle_WriteString (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteString(MSG_PRERELONE, PF_VarString(prinst, 1, pr_globals));
}
static void QCBUILTIN PF_qtSingle_WriteEntity (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteEntity(MSG_PRERELONE, (short)G_EDICTNUM(prinst, OFS_PARM1));
}

static void QCBUILTIN PF_qtBroadcast_WriteByte (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteByte(MSG_BROADCAST, (qbyte)G_FLOAT(OFS_PARM0));
}
static void QCBUILTIN PF_qtBroadcast_WriteChar (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteChar(MSG_BROADCAST, (char)G_FLOAT(OFS_PARM0));
}
static void QCBUILTIN PF_qtBroadcast_WriteShort (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteShort(MSG_BROADCAST, (short)G_FLOAT(OFS_PARM0));
}
static void QCBUILTIN PF_qtBroadcast_WriteLong (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteLong(MSG_BROADCAST, G_FLOAT(OFS_PARM0));
}
static void QCBUILTIN PF_qtBroadcast_WriteAngle (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteAngle(MSG_BROADCAST, G_FLOAT(OFS_PARM0));
}
static void QCBUILTIN PF_qtBroadcast_WriteCoord (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteCoord(MSG_BROADCAST, G_FLOAT(OFS_PARM0));
}
static void QCBUILTIN PF_qtBroadcast_WriteString (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteString(MSG_BROADCAST, PF_VarString(prinst, 0, pr_globals));
}
static void QCBUILTIN PF_qtBroadcast_WriteEntity (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	NPP_NQWriteEntity(MSG_BROADCAST, (short)G_EDICTNUM(prinst, OFS_PARM0));
}
#endif

//======================================================

//copes with any qw point entities.
void SV_point_tempentity (vec3_t o, int type, int count)	//count (usually 1) is available for some tent types.
{
	int split=0;

#ifdef SERVER_DEMO_PLAYBACK
	if (sv.demofile)
		return;
#endif

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
	case TEQW_BLOOD:
	case TE_GUNSHOT:
		MSG_WriteByte (&sv.multicast, type);
		MSG_WriteByte (&sv.multicast, count);
#ifdef NQPROT
		MSG_WriteByte (&sv.nqmulticast, type);	//nq doesn't have a count.
#endif
		break;
	case TEQW_EXPLOSION_NOSPRITE:
		MSG_WriteByte (&sv.multicast, TE_EXPLOSION);
#ifdef NQPROT
		MSG_WriteByte (&sv.nqmulticast, TE_EXPLOSION);
#endif
		type = TEQW_EXPLOSION_NOSPRITE;
		split = PEXT_TE_BULLET;
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
	if (type == TEQW_BLOOD || type == TEQW_LIGHTNINGBLOOD)
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
		if (type == TEQW_BLOOD)
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
	MSG_WriteEntity (&sv.multicast, ownerent);
	MSG_WriteCoord (&sv.multicast, start[0]);
	MSG_WriteCoord (&sv.multicast, start[1]);
	MSG_WriteCoord (&sv.multicast, start[2]);
	MSG_WriteCoord (&sv.multicast, end[0]);
	MSG_WriteCoord (&sv.multicast, end[1]);
	MSG_WriteCoord (&sv.multicast, end[2]);
#ifdef NQPROT
	MSG_WriteByte (&sv.nqmulticast, svc_temp_entity);
	MSG_WriteByte (&sv.nqmulticast, (type==TEQW_BEAM)?TENQ_BEAM:type);
	MSG_WriteEntity (&sv.nqmulticast, ownerent);
	MSG_WriteCoord (&sv.nqmulticast, start[0]);
	MSG_WriteCoord (&sv.nqmulticast, start[1]);
	MSG_WriteCoord (&sv.nqmulticast, start[2]);
	MSG_WriteCoord (&sv.nqmulticast, end[0]);
	MSG_WriteCoord (&sv.nqmulticast, end[1]);
	MSG_WriteCoord (&sv.nqmulticast, end[2]);
#endif
	SV_MulticastProtExt (start, MULTICAST_PHS, pr_global_struct->dimension_send, 0, 0);
}

/*
void PF_tempentity (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), G_FLOAT(OFS_PARM1), 1);
}
*/

//=============================================================================

int SV_ModelIndex (const char *name);

void QCBUILTIN PF_makestatic (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*ent;
	entity_state_t *state;

	ent = G_EDICT(prinst, OFS_PARM0);

	if (sv.num_static_entities == sv_max_staticentities)
	{
		sv_max_staticentities += 16;
		sv_staticentities = BZ_Realloc(sv_staticentities, sizeof(*sv_staticentities) * sv_max_staticentities);
	}

	state = &sv_staticentities[sv.num_static_entities++];
	memset(state, 0, sizeof(*state));

	SV_Snapshot_BuildStateQ1(state, ent, NULL, NULL);
	state->number = sv.num_static_entities;

// throw the entity away now
	ED_Free (svprogfuncs, ent);
}

//=============================================================================

/*
==============
PF_setspawnparms
==============
*/
void QCBUILTIN PF_setspawnparms (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
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
		if (pr_global_ptrs->spawnparamglobals[i])
			*pr_global_ptrs->spawnparamglobals[i] = client->spawn_parms[i];
}

/*
==============
PF_changelevel
==============
*/
void QCBUILTIN PF_changelevel (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char newmap[MAX_QPATH];
	char startspot[MAX_QPATH];

// make sure we don't issue two changelevels (unless the last one failed)
	if (sv.mapchangelocked)
		return;
	sv.mapchangelocked = true;

#ifdef HEXEN2
	if (progstype == PROG_H2)
	{
		COM_QuotedString(PR_GetStringOfs(prinst, OFS_PARM1), startspot, sizeof(startspot), false);
		//these flags disable the whole levelcache thing. the spawnspot is meant to still and always be specified.
		//hexen2 ALWAYS specifies two arguments, and it seems that raven left it blank in some single-player maps too.
		//if we don't want to be stupid/broken in deathmatch, we might as well do the fully compatible thing
		if ((int)pr_global_struct->serverflags & (16|32))
		{
			COM_QuotedString(va("*%s", PR_GetStringOfs(prinst, OFS_PARM0)), newmap, sizeof(newmap), false);
			Cbuf_AddText (va("\nchangelevel %s %s\n", newmap, startspot), RESTRICT_LOCAL);
		}
		else
		{
			COM_QuotedString(PR_GetStringOfs(prinst, OFS_PARM0), newmap, sizeof(newmap), false);
			Cbuf_AddText (va("\nchangelevel %s %s\n", newmap, startspot), RESTRICT_LOCAL);
		}
	}
	else
#endif
	{
		COM_QuotedString(PR_GetStringOfs(prinst, OFS_PARM0), newmap, sizeof(newmap), false);
		if (svprogfuncs->callargc == 2)
		{
			COM_QuotedString(PR_GetStringOfs(prinst, OFS_PARM1), startspot, sizeof(startspot), false);
			Cbuf_AddText (va("\nchangelevel %s %s\n", newmap, startspot), RESTRICT_LOCAL);
		}
		else
			Cbuf_AddText (va("\nchangelevel %s\n", newmap), RESTRICT_LOCAL);
	}
}


/*
==============
PF_logfrag

logfrag (killer, killee)
==============
*/
void QCBUILTIN PF_logfrag (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	extern cvar_t fraglog_details;
	edict_t	*ent1, *ent2;
	int		e1, e2;
	char	s[2048];
	int slen;
	sizebuf_t *sz;

	ent1 = G_EDICT(prinst, OFS_PARM0);
	ent2 = G_EDICT(prinst, OFS_PARM1);

	e1 = NUM_FOR_EDICT(prinst, ent1)-1;
	e2 = NUM_FOR_EDICT(prinst, ent2)-1;

	if (e1 < 0 || e1 >= sv.allocated_client_slots
	|| e2 < 0 || e2 >= sv.allocated_client_slots)
		return;

#ifdef SVRANKING
	if (e1 != e2)	//don't get a point for suicide.
		svs.clients[e1].kills += 1;
	svs.clients[e2].deaths += 1;
#endif

	if (!fraglog_details.ival)
		return;	//not logging any details
	else if (fraglog_details.ival == 7)
		Q_snprintfz(s, sizeof(s)-2, "\\frag\\");	//compat with mvdsv.
	else if (fraglog_details.ival == 1)
		strcpy(s, "\\");//vanilla compat
	else	//specify what info is actually in there
		Q_snprintfz(s, sizeof(s)-2, "\\\\%u\\", fraglog_details.ival);
	slen = strlen(s);
	if (fraglog_details.ival & 1)
	{
		Q_snprintfz(s+slen, sizeof(s)-2-slen, "%s\\%s\\", svs.clients[e1].name, svs.clients[e2].name);
		slen += strlen(s+slen);
	}
	if (fraglog_details.ival & 2)
	{
		Q_snprintfz(s+slen, sizeof(s)-2-slen, "%s\\%s\\", svs.clients[e1].team, svs.clients[e2].team);
		slen += strlen(s+slen);
	}
	if (fraglog_details.ival & 4)
	{
		time_t t = time (NULL);
		struct tm *tm = gmtime(&t);
		Q_snprintfz(s+slen, sizeof(s)-2-slen, "%d-%d-%d %d:%d:%d\\", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
		slen += strlen(s+slen);
	}
	if (fraglog_details.ival & 8)
	{
		Q_snprintfz(s+slen, sizeof(s)-2-slen, "%g\\", ent1->v->weapon);
		slen += strlen(s+slen);
	}
	if (fraglog_details.ival & 16)
	{
		Q_snprintfz(s+slen, sizeof(s)-2-slen, "%s\\%s\\", svs.clients[e1].guid, svs.clients[e2].guid);
		slen += strlen(s+slen);
	}
	s[slen++] = '\n';

	//print it to the fraglog buffer for masters/etc to query
	sz = &svs.log[svs.logsequence&(FRAGLOG_BUFFERS-1)];
	if (sz->cursize && sz->cursize+slen+1 >= sz->maxsize)
	{
		// swap buffers and bump sequence
		svs.logtime = realtime;
		svs.logsequence++;
		sz = &svs.log[svs.logsequence&(FRAGLOG_BUFFERS-1)];
		sz->cursize = 0;
		Con_TPrintf ("beginning fraglog sequence %i\n", svs.logsequence);
	}
	SZ_Write(sz, s, slen);

	//print it to our local fraglog file.
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
char *PF_infokey_Internal (int entnum, const char *key)
{
	char	*value;
	static char ov[256];

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
	else if (entnum <= sv.allocated_client_slots)
	{
		value = ov;
		if (!strcmp(key, "ip"))
		{
			if (svs.clients[entnum-1].state > cs_zombie && svs.clients[entnum-1].protocol == SCP_BAD)
				sprintf(ov, "bot");	//bots don't have valid ips
			else if (svs.clients[entnum-1].netchan.remote_address.type == NA_INVALID)
				sprintf(ov, "");	//bots don't have valid ips
			else
				NET_BaseAdrToString (ov, sizeof(ov), &svs.clients[entnum-1].netchan.remote_address);
		}
		else if (!strcmp(key, "realip"))
		{
			if (svs.clients[entnum-1].state > cs_zombie && svs.clients[entnum-1].protocol == SCP_BAD)
				sprintf(ov, "bot");	//bots don't have valid ips
			else if (svs.clients[entnum-1].realip_status)
				NET_BaseAdrToString (ov, sizeof(ov), &svs.clients[entnum-1].realip);
			else if (svs.clients[entnum-1].netchan.remote_address.type == NA_INVALID)
				sprintf(ov, "");	//bots don't have valid ips
			else	//FIXME: should we report the spoofable/proxy address if the real ip is not known?
				NET_BaseAdrToString (ov, sizeof(ov), &svs.clients[entnum-1].netchan.remote_address);
		}
		else if (!strcmp(key, "csqcactive"))
			sprintf(ov, "%d", svs.clients[entnum-1].csqcactive);
		else if (!strcmp(key, "ping"))
			sprintf(ov, "%d", SV_CalcPing (&svs.clients[entnum-1], false));
		else if (!strcmp(key, "svping"))
			sprintf(ov, "%d", SV_CalcPing (&svs.clients[entnum-1], true));
		else if (!strcmp(key, "guid"))
			sprintf(ov, "%s", svs.clients[entnum-1].guid);
		else if (!strcmp(key, "challenge"))
			sprintf(ov, "%u", svs.clients[entnum-1].challenge);
		else if (!strcmp(key, "*userid"))
			sprintf(ov, "%d", svs.clients[entnum-1].userid);
		else if (!strcmp(key, "download"))
			sprintf(ov, "%d", svs.clients[entnum-1].download != NULL ? (int)(100*svs.clients[entnum-1].downloadcount/svs.clients[entnum-1].downloadsize) : -1);
//		else if (!strcmp(key, "login"))	//mvdsv
//			value = "";
		else if (!strcmp(key, "protocol"))
		{
			value = "";
			switch(svs.clients[entnum-1].protocol)
			{
			case SCP_BAD:
				value = "";	//could be a writebyted bot...
				break;
			case SCP_QUAKEWORLD:
				if (!svs.clients[entnum-1].fteprotocolextensions && !svs.clients[entnum-1].fteprotocolextensions)
					value = "quakeworld";
				else
					value = "quakeworld+";
				break;
			case SCP_QUAKE2:
				value = "quake2";	//shouldn't happen
				break;
			case SCP_QUAKE3:
				value = "quake3";	//can actually happen.
				break;
			case SCP_NETQUAKE:
				value = "quake";
				break;
			case SCP_BJP3:
				value = "bjp3";
				break;
			case SCP_FITZ666:
				if (svs.clients[entnum-1].netchan.netprim.coordsize != 2)
					value = "rmq999";
				else
					value = "fitz666";
				break;
			case SCP_DARKPLACES6:
				value = "dpp6";
				break;
			case SCP_DARKPLACES7:
				value = "dpp7";
				break;
			}
		}
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
		else if (!strcmp(key, "*VIP"))
			value = (svs.clients[entnum-1].penalties & BAN_VIP)?"1":"";
		else if (!strcmp(key, "*ismuted"))
			value = (svs.clients[entnum-1].penalties & BAN_MUTE)?"1":"";
		else if (!strcmp(key, "*isdeaf"))
			value = (svs.clients[entnum-1].penalties & BAN_DEAF)?"1":"";
		else if (!strcmp(key, "*iscrippled"))
			value = (svs.clients[entnum-1].penalties & BAN_CRIPPLED)?"1":"";
		else if (!strcmp(key, "*iscuffed"))
			value = (svs.clients[entnum-1].penalties & BAN_CUFF)?"1":"";
		else if (!strcmp(key, "*islagged"))
			value = (svs.clients[entnum-1].penalties & BAN_LAGGED)?"1":"";
		else
			value = Info_ValueForKey (svs.clients[entnum-1].userinfo, key);
	} else
		value = "";

	return value;
}

static void QCBUILTIN PF_infokey (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*e;
	int		e1;
	char	*value;
	const char	*key;

	e = G_EDICT(prinst, OFS_PARM0);
	e1 = NUM_FOR_EDICT(prinst, e);
	key = PR_GetStringOfs(prinst, OFS_PARM1);

	value = PF_infokey_Internal (e1, key);

	G_INT(OFS_RETURN) = PR_TempString(prinst, value);
}

static void QCBUILTIN PF_sv_serverkey (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*value;
	const char	*key;

	key = PR_GetStringOfs(prinst, OFS_PARM1);
	value = Info_ValueForKey (svs.info, key);
	G_INT(OFS_RETURN) = PR_TempString(prinst, value);
}


/*
==============
PF_multicast

void(vector where, float set) multicast
==============
*/
void QCBUILTIN PF_multicast (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	*o;
	int		to;

	o = G_VECTOR(OFS_PARM0);
	to = G_FLOAT(OFS_PARM1);

#ifdef NETPREPARSE
	NPP_Flush();
#endif

	SV_MulticastProtExt(o, to, pr_global_struct->dimension_send, 0, 0);
}

static void QCBUILTIN PF_Ignore(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_INT(OFS_RETURN) = 0;
}

#ifndef QUAKETC
static void QCBUILTIN PF_mvdsv_newstring(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)	//mvdsv
{
	char *s;
	int len;

	const char *in = PR_GetStringOfs(prinst, OFS_PARM0);

	len = strlen(in)+1;
	if (prinst->callargc == 2 && G_FLOAT(OFS_PARM1) > len)
		len = G_FLOAT(OFS_PARM1);

	s = prinst->AddressableAlloc(prinst, len);
	G_INT(OFS_RETURN) = (char*)s - prinst->stringtable;
	strcpy(s, in);
}
static void QCBUILTIN PF_mvdsv_freestring(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)	//mvdsv
{
	prinst->AddressableFree(prinst, prinst->stringtable + G_INT(OFS_PARM0));
}
#endif

// warning: ‘PF_strcatp’ defined but not used
/*
static void QCBUILTIN PF_strcatp(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
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
*/

// warning: ‘PF_redstring’ defined but not used
/*
static void QCBUILTIN PF_redstring(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *string = PR_GetStringOfs(prinst, OFS_PARM0), *s;
	static char buf[1024];

	for (s = buf; *string; s++, string++)
		*s=*string|CON_HIGHCHARSMASK;
	*s = '\0';

	RETURN_TSTRING(buf);
}
*/

#ifdef SVCHAT
void SV_Chat(const char *filename, float starttag, edict_t *edict);
static void QCBUILTIN PF_chat (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_Chat(PR_GetStringOfs(prinst, OFS_PARM0), G_FLOAT(OFS_PARM1), G_EDICT(prinst, OFS_PARM2));
}
#endif








/* FTE_SQL builtins */
#ifdef SQL
void QCBUILTIN PF_sqlconnect (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *paramstr[SQL_CONNECT_PARAMS];
	const char *driver;
	int i;

	if (!SQL_Available())
	{
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}

	// check and fit connection parameters
	for (i = 0; i < SQL_CONNECT_PARAMS; i++)
	{
		if (svprogfuncs->callargc <= (i + 1))
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

	// verify/switch driver choice
	if (svprogfuncs->callargc >= (SQL_CONNECT_PARAMS + 1))
		driver = PR_GetStringOfs(prinst, OFS_PARM0 + SQL_CONNECT_PARAMS * 3);
	else
		driver = "";

	if (!driver[0])
		driver = sql_driver.string;

	G_FLOAT(OFS_RETURN) = SQL_NewServer(driver, paramstr);
}

void QCBUILTIN PF_sqldisconnect (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	sqlserver_t *server;

	if (SQL_Available())
	{
		server = SQL_GetServer(G_FLOAT(OFS_PARM0), false);
		if (server)
		{
			SQL_Disconnect(server);
			return;
		}
	}
}

static qboolean PR_SQLResultAvailable(queryrequest_t *req, int firstrow, int numrows, int numcols, qboolean eof)
{
	edict_t *ent;
	pubprogfuncs_t *prinst = svprogfuncs;
	struct globalvars_s *pr_globals = PR_globals(prinst, PR_CURRENT);

	if (req->user.qccallback)
	{
		G_FLOAT(OFS_PARM0) = req->srvid;
		G_FLOAT(OFS_PARM1) = req->num;
		G_FLOAT(OFS_PARM2) = numrows;
		G_FLOAT(OFS_PARM3) = numcols;
		G_FLOAT(OFS_PARM4) = eof;
		G_FLOAT(OFS_PARM5) = firstrow;

		// recall self and other references
		ent = PROG_TO_EDICT(prinst, req->user.selfent);
		if (ED_ISFREE(ent) || ent->xv->uniquespawnid != req->user.selfid)
			pr_global_struct->self = pr_global_struct->world;
		else
			pr_global_struct->self = req->user.selfent;
		ent = PROG_TO_EDICT(prinst, req->user.otherent);
		if (ED_ISFREE(ent) || ent->xv->uniquespawnid != req->user.otherid)
			pr_global_struct->other = pr_global_struct->world;
		else
			pr_global_struct->other = req->user.otherent;

		PR_ExecuteProgram(svprogfuncs, req->user.qccallback);
	}
	if (eof && req->user.thread)
	{
		qcstate_t *thread = req->user.thread;
		req->user.thread = NULL;
		if (thread)
			thread->waiting = false;
	}

	return req->user.persistant;
}

void QCBUILTIN PF_sqlopenquery (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	sqlserver_t *server;
	int callfunc = G_INT(OFS_PARM1);
	int querytype = G_FLOAT(OFS_PARM2);
	char *querystr = PF_VarString(prinst, 3, pr_globals);

	if (SQL_Available())
	{
		server = SQL_GetServer(G_FLOAT(OFS_PARM0), false);
		if (server)
		{
			queryrequest_t *qreq;

			Con_DPrintf("SQL Query: %s\n", querystr);

			G_FLOAT(OFS_RETURN) = SQL_NewQuery(server, PR_SQLResultAvailable, querystr, &qreq);

			if (qreq)
			{
				//so our C callback knows what to do
				qreq->user.persistant = querytype > 0;
				qreq->user.qccallback = callfunc;

				// save self and other references
				qreq->user.selfent = ED_ISFREE(PROG_TO_EDICT(prinst, pr_global_struct->self))?pr_global_struct->world:pr_global_struct->self;
				qreq->user.selfid = PROG_TO_EDICT(prinst, qreq->user.selfent)->xv->uniquespawnid;
				qreq->user.otherent = ED_ISFREE(PROG_TO_EDICT(prinst, pr_global_struct->other))?pr_global_struct->world:pr_global_struct->other;
				qreq->user.otherid = PROG_TO_EDICT(prinst, qreq->user.otherent)->xv->uniquespawnid;

				if (querytype & 2)
				{
					qreq->user.thread = PR_CreateThread(prinst, G_FLOAT(OFS_RETURN), 0, true);

					svprogfuncs->AbortStack(prinst);
				}
			}
			return;
		}
	}
	// else we failed so return the error
	G_FLOAT(OFS_RETURN) = -1;
}

void QCBUILTIN PF_sqlclosequery (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	sqlserver_t *server;
	queryrequest_t *qreq;

	if (SQL_Available())
	{
		server = SQL_GetServer(G_FLOAT(OFS_PARM0), false);
		if (server)
		{
			qreq = SQL_GetQueryRequest(server, G_FLOAT(OFS_PARM1));
			if (qreq)
			{
				SQL_CloseRequest(server, qreq, false);
				return;
			}
			else 
				Con_Printf("Invalid sql request\n");
		}
	}
	// else nothing to close
}

void QCBUILTIN PF_sqlreadfield (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	sqlserver_t *server;
	queryresult_t *qres;
	char *data;

	if (SQL_Available())
	{
		server = SQL_GetServer(G_FLOAT(OFS_PARM0), false);
		if (server)
		{
			qres = SQL_GetQueryResult(server, G_FLOAT(OFS_PARM1), G_FLOAT(OFS_PARM2));
			if (qres)
			{
				data = SQL_ReadField(server, qres, G_FLOAT(OFS_PARM2), G_FLOAT(OFS_PARM3), true, NULL);
				if (data)
				{
					RETURN_TSTRING(data);
					return;
				}
			}
			else
			{
				Con_Printf("Invalid sql request/row\n");
			}
		}
	}
	// else we failed to get anything
	G_INT(OFS_RETURN) = 0;
}

void QCBUILTIN PF_sqlreadfloat (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	sqlserver_t *server;
	queryrequest_t *qreq;
	queryresult_t *qres;
	char *data;

	if (SQL_Available())
	{
		server = SQL_GetServer(G_FLOAT(OFS_PARM0), false);
		if (server)
		{
			if (G_FLOAT(OFS_PARM2) < 0)
			{
				qreq = SQL_GetQueryRequest(server, G_FLOAT(OFS_PARM1));
				if (qreq->results)
				{
					if (G_FLOAT(OFS_PARM2) == -2)
						G_FLOAT(OFS_RETURN) = qreq->results->columns;
					else if (G_FLOAT(OFS_PARM2) == -3)
						G_FLOAT(OFS_RETURN) = qreq->results->rows + qreq->results->firstrow;
					else
					{
						Con_Printf("Invalid sql row\n");
						G_FLOAT(OFS_RETURN) = 0;
					}
					return;
				}
			}
			else
			{
				qres = SQL_GetQueryResult(server, G_FLOAT(OFS_PARM1), G_FLOAT(OFS_PARM2));
				if (qres)
				{
					if (G_FLOAT(OFS_PARM2) == -1)
					{
						G_FLOAT(OFS_RETURN) = qres->columns;
						return;
					}
					if (G_FLOAT(OFS_PARM2) == -2)
					{
						G_FLOAT(OFS_RETURN) = qres->rows;
						return;
					}

					data = SQL_ReadField(server, qres, G_FLOAT(OFS_PARM2), G_FLOAT(OFS_PARM3), true, NULL);
					if (data)
					{
						G_FLOAT(OFS_RETURN) = Q_atof(data);
						return;
					}
				}
				else
				{
					Con_Printf("Invalid sql request/row\n");
					PR_StackTrace(prinst, false);
				}
			}
		}
	}
	// else we failed to get anything
	G_FLOAT(OFS_RETURN) = 0;
}

void QCBUILTIN PF_sqlreadblob (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	sqlserver_t *server;
	queryresult_t *qres;
	char *data;

	int serveridx = G_FLOAT(OFS_PARM0);
	int queryidx = G_FLOAT(OFS_PARM1);
	int row = G_FLOAT(OFS_PARM2);
	int column = G_FLOAT(OFS_PARM3);
	int dst = G_INT(OFS_PARM4);
	int dstsize = G_INT(OFS_PARM5);

	if (dst <= 0 || dst+dstsize >= prinst->stringtablesize)
	{	//FIXME: this check should be some utility function.
		PR_BIError(prinst, "PF_sqlreadblob: invalid dest\n");
		return;
	}

	if (SQL_Available())
	{
		server = SQL_GetServer(serveridx, false);
		if (server)
		{
			qres = SQL_GetQueryResult(server, queryidx, row);
			if (qres)
			{
				size_t blobsize;
				data = SQL_ReadField(server, qres, row, column, true, &blobsize);
				if (data)
				{	//unsure how to handle overflows. we truncate for now.
					blobsize = min(blobsize, dstsize);
					G_INT(OFS_RETURN) = blobsize;
					memcpy(prinst->stringtable + dst, data, blobsize);
					return;
				}
			}
			else
			{
				Con_Printf("Invalid sql request/row\n");
				PR_StackTrace(prinst, false);
			}
		}
	}
	// else we failed to get anything
	G_INT(OFS_RETURN) = 0;
}
void QCBUILTIN PF_sqlescapeblob (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char hex[16] = "0123456789abcdef";
//	int serveridx = G_FLOAT(OFS_PARM0);	//fixme
	int qcptr = G_INT(OFS_PARM1);
	int size = G_INT(OFS_PARM2);
	char *out;

	qbyte *blob = prinst->stringtable + qcptr;
	
	if (qcptr <= 0 || qcptr+size >= prinst->stringtablesize)
	{	//FIXME: this check should be some utility function.
		PR_BIError(prinst, "PF_sqlescapeblob: invalid blob\n");
		return;
	}

	G_INT(OFS_RETURN) = prinst->AllocTempString(prinst, &out, size*2+4);

	//"x'DEADBEEF'"
	*out++ = 'x';
	*out++ = '\'';
	for (; size > 0; size--, blob++)
	{
		*out++ = hex[*blob>>4];
		*out++ = hex[*blob&15];
	}
	*out++ = '\'';
	*out++ = 0;
}

void QCBUILTIN PF_sqlerror (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	sqlserver_t *server;

	if (SQL_Available())
	{
		server = SQL_GetServer(G_FLOAT(OFS_PARM0), true);
		if (server)
		{
			if (svprogfuncs->callargc == 2)
			{ // query-specific error request
				if (server->active) // didn't check this earlier so check it now
				{
					queryresult_t *qres = SQL_GetQueryResult(server, G_FLOAT(OFS_PARM1), G_FLOAT(OFS_PARM2));
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

void QCBUILTIN PF_sqlescape (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	sqlserver_t *server;
	const char *toescape;
	char escaped[4096];

	if (SQL_Available())
	{
		server = SQL_GetServer(G_FLOAT(OFS_PARM0), false);
		if (server)
		{
			toescape = PR_GetStringOfs(prinst, OFS_PARM1);
			if (toescape)
			{
				SQL_Escape(server, toescape, escaped, sizeof(escaped));
				RETURN_TSTRING(escaped);
				return;
			}
		}
	}
	// else invalid string or server reference
	RETURN_TSTRING("");
}

void QCBUILTIN PF_sqlversion (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	sqlserver_t *server;

	if (SQL_Available())
	{
		server = SQL_GetServer(G_FLOAT(OFS_PARM0), false);
		if (server)
		{
			RETURN_TSTRING(SQL_Info(server));
			return;
		}
	}
	// else invalid string or server reference
	RETURN_TSTRING("");
}

void PR_SQLCycle(void)
{
	SQL_ServerCycle();
}
#endif




lh_extension_t *checkfteextensioncl(int mask, const char *name)	//true if the cient extension mask matches an extension name
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

lh_extension_t *checkfteextensionsv(const char *name)	//true if the server supports an protocol extension.
{
	return checkfteextensioncl(Net_PextMask(1, false), name);
}

lh_extension_t *checkextension(const char *name)
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
static void QCBUILTIN PF_checkextension (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	lh_extension_t *ext = NULL;
	const char *s = PR_GetStringOfs(prinst, OFS_PARM0);

	ext = checkextension(s);
	if (!ext)
	{
		if (svprogfuncs->callargc == 2)
		{
			int clnum = NUM_FOR_EDICT(prinst, G_EDICT(prinst, OFS_PARM1));
			if (clnum >= 1 && clnum <= sv.allocated_client_slots)	//valid client as second parameter
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
			if (*ext->builtinnames[i] == '.' || *ext->builtinnames[i] == '#')
			{
				/*field or global*/
			}
			else if (!PR_EnableEBFSBuiltin(ext->builtinnames[i], 0))
			{
				Con_Printf("Failed to initialise builtin \"%s\" for extension \"%s\"\n", ext->builtinnames[i], s);
				return;	//whoops, we failed.
			}
		}

		if (ext->queried)
			*ext->queried = true;

		G_FLOAT(OFS_RETURN) = true;
		Con_DPrintf("Extension %s is supported\n", s);
	}
	else
		G_FLOAT(OFS_RETURN) = false;
}

static void QCBUILTIN PF_checkbuiltin (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	func_t funcref = G_INT(OFS_PARM0);
	char *funcname = NULL;
	int args;
	int builtinno;
	if (prinst->GetFunctionInfo(prinst, funcref, &args, &builtinno, funcname, sizeof(funcname)))
	{	//qc defines the function at least. nothing weird there...
		if (builtinno > 0 && builtinno < prinst->parms->numglobalbuiltins)
		{
			if (!prinst->parms->globalbuiltins[builtinno] || prinst->parms->globalbuiltins[builtinno] == PF_Fixme)
				G_FLOAT(OFS_RETURN) = false;	//the builtin with that number isn't defined.
			else
			{
				G_FLOAT(OFS_RETURN) = true;		//its defined, within the sane range, mapped, everything. all looks good.
				//we should probably go through the available builtins and validate that the qc's name matches what would be expected
				//this is really intended more for builtins defined as #0 though, in such cases, mismatched assumptions are impossible.
			}
		}
		else
			G_FLOAT(OFS_RETURN) = false;	//not a valid builtin (#0 builtins get remapped according to the function name)
	}
	else
	{	//not valid somehow.
		G_FLOAT(OFS_RETURN) = false;
	}
}

static void QCBUILTIN PF_builtinsupported (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *s = PR_GetStringOfs(prinst, OFS_PARM0);
	int binum = (prinst->callargc < 2)?0:G_FLOAT(OFS_PARM1);

	G_FLOAT(OFS_RETURN) = PR_EnableEBFSBuiltin(s, binum);
}




//mvdsv builtins.
void QCBUILTIN PF_ExecuteCommand  (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)	//83		//void() exec;
{
	int old_other, old_self; // mod_consolecmd will be executed, so we need to store this

	old_self = pr_global_struct->self;
	old_other = pr_global_struct->other;

	Cbuf_Execute();

	pr_global_struct->self = old_self;
	pr_global_struct->other = old_other;
}

#ifndef QUAKETC
/*
=================
PF_teamfield

string teamfield(.string field)
=================
*/

static void QCBUILTIN PF_teamfield (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	pr_teamfield = G_INT(OFS_PARM0)+prinst->fieldadjust;
}

/*
=================
PF_substr

string substr(string str, float start, float len)
=================
*/

static void QCBUILTIN PF_substr (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char dest[4096];
	const char *s;
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

static void QCBUILTIN PF_str2byte (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = (float) *PR_GetStringOfs(prinst, OFS_PARM0);
}

/*
=================
PF_str2short

float str2short (string str)
=================
*/

static void QCBUILTIN PF_str2short (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = (float) LittleShort(*(short*)PR_GetStringOfs(prinst, OFS_PARM0));
}

/*
=================
PF_readcmd

string readmcmd (string str)
=================
*/

static void QCBUILTIN PF_readcmd (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *s;
	extern char sv_redirected_buf[];
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

	SV_BeginRedirect(RD_OBLIVION, TL_FindLanguage(""));
	Cbuf_Execute();
	Con_Printf("PF_readcmd: %s\n%s", s, sv_redirected_buf);
	G_INT(OFS_RETURN) = (int)PR_TempString(prinst, sv_redirected_buf);
	SV_EndRedirect();

	if (old != RD_NONE)
		SV_BeginRedirect(old, oldl);
}

/*
=================
PF_redirectcmd

void redirectcmd (entity to, string str)
=================
*/
/*
static void QCBUILTIN PF_redirectcmd (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *s;
	int entnum;
	extern redirect_t sv_redirected;

	if (sv_redirected)
		return;

	entnum = G_EDICTNUM(OFS_PARM0);
	if (entnum < 1 || entnum > sv.allocated_client_slots)
		PR_RunError ("Parm 0 not a client");

	s = G_STRING(OFS_PARM1);

	Cbuf_AddText (s);

	SV_BeginRedirect(RD_MOD + entnum);
	Cbuf_Execute();
	SV_EndRedirect();
}*/

/*
=================
PF_forcedemoframe

void PF_forcedemoframe(float now)
Forces demo frame
if argument 'now' is set, frame is written instantly
=================
*/

static void QCBUILTIN PF_forcedemoframe (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
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

static void QCBUILTIN PF_MVDSV_strcpy (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	unsigned int dst = G_INT(OFS_PARM0);
	const char *src = PR_GetStringOfs(prinst, OFS_PARM1);
	unsigned int size = strlen(src)+1;

	if (dst <= 0 || dst+size >= prinst->stringtablesize)
	{
		PR_BIError(prinst, "PF_strcpy: invalid dest\n");
		return;
	}

	strcpy(prinst->stringtable+dst, src);
}

/*
=================
PF_strncpy

void strcpy(string dst, string src, float count)
FIXME: check for null pointers first?
=================
*/

static void QCBUILTIN PF_MVDSV_strncpy (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	unsigned int dst = G_INT(OFS_PARM0);
	const char *src = PR_GetStringOfs(prinst, OFS_PARM1);
	unsigned int size = G_FLOAT(OFS_PARM2);

	if (dst <= 0 || dst+size >= prinst->stringtablesize)
	{
		PR_BIError(prinst, "PF_strncpy: invalid dest\n");
		return;
	}

	strncpy(prinst->stringtable+dst, src, size);
}


/*
=================
PF_strstr

string strstr(string str, string sub)
=================
*/

static void QCBUILTIN PF_strstr (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *str, *sub, *p;

	str = PR_GetStringOfs(prinst, OFS_PARM0);
	sub = PR_GetStringOfs(prinst, OFS_PARM1);

	if ((p = strstr(str, sub)) == NULL)
	{
		G_INT(OFS_RETURN) = 0;
		return;
	}

	if (p > prinst->stringtable && p-prinst->stringtable < prinst->stringtablesize)
		G_INT(OFS_RETURN) = p-prinst->stringtable;
	else
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

static void QCBUILTIN PF_logtext(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
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
#endif

static void QCBUILTIN PF_OpenPortal	(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
#ifdef Q2BSPS
	if (sv.world.worldmodel->fromgame == fg_quake2)
	{
		int i, portal;
		int state	= G_FLOAT(OFS_PARM1)!=0;
		client_t *client;
		if (G_INT(OFS_PARM1) >= MAX_EDICTS)
			portal = G_FLOAT(OFS_PARM0);	//old legacy crap.
		else
			portal = G_EDICT(prinst, OFS_PARM0)->xv->style;	//read the func_areaportal's style field.
		for (client = svs.clients, i = 0; i < sv.allocated_client_slots; i++, client++)
		{
			if (client->state >= cs_connected)
			{
				ClientReliableWrite_Begin(client, svc_setportalstate, 4);
				if (portal >= 0x80)
				{	//new pathway, to be enabled at some point
					if (portal > 0xff)
					{
						ClientReliableWrite_Byte(client, 0x80 | 2 | state);
						ClientReliableWrite_Short(client, portal);
					}
					else
					{
						ClientReliableWrite_Byte(client, 0x80 | 0 | state);
						ClientReliableWrite_Byte(client, portal);
					}
				}
				else
					ClientReliableWrite_Short(client, portal | (state<<15));
			}
		}
		CMQ2_SetAreaPortalState(sv.world.worldmodel, portal, state);
	}
#endif
#ifdef Q3BSPS
	if (sv.world.worldmodel->fromgame == fg_quake3)
	{
		int i;
		int state	= G_FLOAT(OFS_PARM1)!=0;
		client_t *client;
		edict_t *portal = G_EDICT(prinst, OFS_PARM0);
		int area1 = portal->pvsinfo.areanum, area2 = portal->pvsinfo.areanum2;
		if (area1 == area2 || area1<0 || area2<0)
			return;
		for (client = svs.clients, i = 0; i < sv.allocated_client_slots; i++, client++)
		{
			if (client->state >= cs_connected)
			{
				ClientReliableWrite_Begin(client, svc_setportalstate, 6);
				if (area1 > 0xff || area2 > 0xff)
				{
					ClientReliableWrite_Byte(client, 0xc0 | 2 | state);
					ClientReliableWrite_Short(client, area1);
					ClientReliableWrite_Short(client, area2);
				}
				else
				{
					ClientReliableWrite_Byte(client, 0xc0 | 0 | state);
					ClientReliableWrite_Byte(client, area1);
					ClientReliableWrite_Byte(client, area2);
				}
			}
		}
		CMQ3_SetAreaPortalState(sv.world.worldmodel, portal->pvsinfo.areanum, portal->pvsinfo.areanum2, state);
	}
#endif
}


//EXTENSION: DP_QC_FINDCHAIN

//entity(string field, string match) findchain = #402
//chained search for strings in entity fields
static void QCBUILTIN PF_sv_findchain (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i, f;
	const char *s;
	string_t t;
	edict_t *ent, *chain;

	chain = (edict_t *) *prinst->parms->sv_edicts;

	f = G_INT(OFS_PARM0)+prinst->fieldadjust;
	s = PR_GetStringOfs(prinst, OFS_PARM1);

	for (i = 1; i < *prinst->parms->sv_num_edicts; i++)
	{
		ent = EDICT_NUM(prinst, i);
		if (ED_ISFREE(ent))
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
static void QCBUILTIN PF_sv_findchainfloat (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
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
		if (ED_ISFREE(ent))
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
static void QCBUILTIN PF_sv_findchainflags (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
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
		if (ED_ISFREE(ent))
			continue;
		if (!((int)((float *)ent->v)[f] & s))
			continue;

		ent->v->chain = EDICT_TO_PROG(prinst, chain);
		chain = ent;
	}

	RETURN_EDICT(prinst, chain);
}

//EXTENSION: KRIMZON_SV_PARSECLIENTCOMMAND

//void(entity e, string s) clientcommand = #440
//executes a command string as if it came from the specified client
static void QCBUILTIN PF_clientcommand (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	client_t *oldhostclient = host_client;
	edict_t *oldsvplayer = sv_player;
	unsigned int i;

	//find client for this entity
	i = NUM_FOR_EDICT(prinst, G_EDICT(prinst, OFS_PARM0));
	if (i <= 0 || i > sv.allocated_client_slots)
		PR_BIError(prinst, "PF_clientcommand: entity is not a client");
	else
	{
		host_client = &svs.clients[i-1];
		sv_player = host_client->edict;
		if (host_client->state == cs_connected || host_client->state == cs_spawned)
		{
			SV_ExecuteUserCommand (PF_VarString(prinst, 1, pr_globals), true);
		}
		else
			Con_Printf("PF_clientcommand: client is not active\n");
	}

	host_client = oldhostclient;
	sv_player = oldsvplayer;
}






const char *SV_CheckRejectConnection(netadr_t *adr, const char *uinfo, unsigned int protocol, unsigned int pext1, unsigned int pext2, char *guid)
{
	char addrstr[256];
	char clfeatures[4096], *bp;
	const char *ret = NULL;
	if (gfuncs.CheckRejectConnection)
	{
		globalvars_t *pr_globals = PR_globals(svprogfuncs, PR_CURRENT);

		NET_AdrToString(addrstr, sizeof(addrstr), adr);

		*clfeatures = 0;
		switch(protocol)
		{
		default:				bp = "unknown";		break;
		case SCP_QUAKEWORLD:	bp = "qw";			break;
		case SCP_QUAKE2:		bp = "q2";			break;
		case SCP_QUAKE3:		bp = "q3";			break;
		case SCP_NETQUAKE:		bp = "nq";			break;
		case SCP_BJP3:			bp = "bjp3";		break;
		case SCP_FITZ666:		bp = "fitz666";		break;
		case SCP_DARKPLACES6:	bp = "dp6";			break;
		case SCP_DARKPLACES7:	bp = "dp7";			break;
		}
		Info_SetValueForKey(clfeatures, "basicprotocol", bp, sizeof(clfeatures));
		Info_SetValueForKey(clfeatures, "guid", guid, sizeof(clfeatures));
		Info_SetValueForKey(clfeatures, "maxsounds", "256", sizeof(clfeatures));
		Info_SetValueForKey(clfeatures, "maxmodels", "256", sizeof(clfeatures));

		//this is not the limits of the client itself, but the limits that the server is able and willing to send to them.

		if ((pext1 & PEXT_SOUNDDBL) || (protocol == SCP_BJP3 || protocol == SCP_FITZ666 || protocol == SCP_DARKPLACES6) || (protocol == SCP_DARKPLACES7))
			Info_SetValueForKey(clfeatures, "maxsounds", va("%i", MAX_PRECACHE_SOUNDS), sizeof(clfeatures));
		else
			Info_SetValueForKey(clfeatures, "maxsounds", "256", sizeof(clfeatures));

		if ((pext1 & PEXT_MODELDBL) || (protocol == SCP_BJP3 || protocol == SCP_FITZ666 || protocol == SCP_DARKPLACES6) || (protocol == SCP_DARKPLACES7))
			Info_SetValueForKey(clfeatures, "maxmodels", va("%i", MAX_PRECACHE_MODELS), sizeof(clfeatures));
		else
			Info_SetValueForKey(clfeatures, "maxmodels", "256", sizeof(clfeatures));

		if (pext2 & PEXT2_REPLACEMENTDELTAS)
			Info_SetValueForKey(clfeatures, "maxentities", va("%i", MAX_EDICTS), sizeof(clfeatures));
		else if (protocol == SCP_BJP3 || protocol == SCP_FITZ666)
//			Info_SetValueForKey(clfeatures, "maxentities", "65535", sizeof(clfeatures));
			Info_SetValueForKey(clfeatures, "maxentities", "32767", sizeof(clfeatures));
		else if (protocol == SCP_DARKPLACES6 || protocol == SCP_DARKPLACES7)
			Info_SetValueForKey(clfeatures, "maxentities", "32767", sizeof(clfeatures));
		else if (pext1 & PEXT_ENTITYDBL2)
			Info_SetValueForKey(clfeatures, "maxentities", "2048", sizeof(clfeatures));
		else if (pext1 & PEXT_ENTITYDBL)
			Info_SetValueForKey(clfeatures, "maxentities", "1024", sizeof(clfeatures));
		else if (protocol == SCP_NETQUAKE)
			Info_SetValueForKey(clfeatures, "maxentities", "600", sizeof(clfeatures));
		else //if (protocol == SCP_QUAKEWORLD)
			Info_SetValueForKey(clfeatures, "maxentities", "512", sizeof(clfeatures));

		if (pext2 & PEXT2_REPLACEMENTDELTAS)			//limited by packetlog/size, but server can track the whole lot, assuming they're not all sent in a single packet.
			Info_SetValueForKey(clfeatures, "maxvisentities", va("%i", MAX_EDICTS), sizeof(clfeatures));
		else if (protocol == SCP_DARKPLACES6 || protocol == SCP_DARKPLACES7)	//deltaing protocol allows all ents to be visible at once
			Info_SetValueForKey(clfeatures, "maxvisentities", "32767", sizeof(clfeatures));
		else if (pext1 & PEXT_256PACKETENTITIES)
			Info_SetValueForKey(clfeatures, "maxvisentities", "256", sizeof(clfeatures));
		else if (protocol == SCP_QUAKEWORLD)
			Info_SetValueForKey(clfeatures, "maxvisentities", "64", sizeof(clfeatures));
		//others are limited by packet sizes, so the count can vary...

		//features
#ifdef PEXT_VIEW2
		if (pext1 & PEXT_VIEW2)
			Info_SetValueForKey(clfeatures, "PEXT_VIEW2", "1", sizeof(clfeatures));
#endif
		if (pext1 & PEXT_LIGHTSTYLECOL)
			Info_SetValueForKey(clfeatures, "PEXT_LIGHTSTYLECOL", "1", sizeof(clfeatures));
		if ((pext1 & PEXT_CSQC) || (protocol == SCP_DARKPLACES6) || (protocol == SCP_DARKPLACES7))
			Info_SetValueForKey(clfeatures, "PEXT_CSQC", "1", sizeof(clfeatures));
		if ((pext1 & PEXT_FLOATCOORDS) || (protocol == SCP_DARKPLACES6) || (protocol == SCP_DARKPLACES7))
			Info_SetValueForKey(clfeatures, "PEXT_FLOATCOORDS", "1", sizeof(clfeatures));
		if ((pext1 & PEXT_ENTITYDBL) || (pext2 & PEXT2_REPLACEMENTDELTAS) || (protocol == SCP_FITZ666 || protocol == SCP_DARKPLACES6) || (protocol == SCP_DARKPLACES7))
			Info_SetValueForKey(clfeatures, "PEXT_ENTITYDBL", "1", sizeof(clfeatures));
		if (pext1 & PEXT_HEXEN2)
			Info_SetValueForKey(clfeatures, "PEXT_HEXEN2", "1", sizeof(clfeatures));
		if ((pext1 & PEXT_SETATTACHMENT) || (protocol == SCP_DARKPLACES6) || (protocol == SCP_DARKPLACES7))
			Info_SetValueForKey(clfeatures, "PEXT_SETATTACHMENT", "1", sizeof(clfeatures));
		if (pext1 & PEXT_CUSTOMTEMPEFFECTS)
			Info_SetValueForKey(clfeatures, "PEXT_CUSTOMTEMPEFFECTS", "1", sizeof(clfeatures));
		if ((pext2 & PEXT2_PRYDONCURSOR) || (protocol == SCP_DARKPLACES6) || (protocol == SCP_DARKPLACES7))
			Info_SetValueForKey(clfeatures, "PEXT2_PRYDONCURSOR", "1", sizeof(clfeatures));
		if (pext2 & PEXT2_VOICECHAT)
			Info_SetValueForKey(clfeatures, "PEXT2_VOICECHAT", "1", sizeof(clfeatures));
		if (pext2 & PEXT2_REPLACEMENTDELTAS)
			Info_SetValueForKey(clfeatures, "PEXT2_REPLACEMENTDELTAS", "1", sizeof(clfeatures));

		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv.world.edicts);
		G_INT(OFS_PARM0) = (int)PR_TempString(svprogfuncs, addrstr);
		G_INT(OFS_PARM1) = (int)PR_TempString(svprogfuncs, uinfo);
		G_INT(OFS_PARM2) = (int)PR_TempString(svprogfuncs, clfeatures);
		PR_ExecuteProgram (svprogfuncs, gfuncs.CheckRejectConnection);
		ret = PR_GetStringOfs(svprogfuncs, OFS_RETURN);
		if (!*ret)
			ret = NULL;
	}
	return ret;
}
void SV_AddDebugPolygons(void)
{
	int i;
	if (gfuncs.AddDebugPolygons)
	{
#ifdef PROGS_DAT
		extern qboolean csqc_dp_lastwas3d;
		csqc_dp_lastwas3d = true;
#endif
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv.world.edicts);
		for (i = 0; i < sv.allocated_client_slots; i++)
			if (svs.clients[i].netchan.remote_address.type == NA_LOOPBACK)
				pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, svs.clients[i].edict);
		PR_ExecuteProgram (svprogfuncs, gfuncs.AddDebugPolygons);
#ifdef PROGS_DAT
		csqc_dp_lastwas3d = false;
#endif
	}
}

#ifdef HEXEN2
static void QCBUILTIN PF_h2AdvanceFrame(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
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

static void QCBUILTIN PF_h2RewindFrame(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
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

static void QCBUILTIN PF_h2advanceweaponframe (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
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

void PRH2_SetPlayerClass(client_t *cl, int classnum, qboolean fromqc)
{
	char		temp[16];
	if (classnum < 1)
		return; //reject it (it would crash the (standard hexen2) mod)
	if (classnum > 5)
		return;

	if (!fromqc)
	{
		if (progstype != PROG_H2)
			return;

		/*if they already have a class, only switch to the class already set by the gamecode
		this is awkward, but matches hexen2*/
		if (cl->playerclass)
		{
			if (cl->edict->xv->playerclass)
				classnum = cl->edict->xv->playerclass;
			else if (cl->playerclass)
				classnum = cl->playerclass;
		}
	}

	if (classnum)
		sprintf(temp,"%i",(int)classnum);
	else
		*temp = 0;
	Info_SetValueForKey (cl->userinfo, "cl_playerclass", temp, sizeof(cl->userinfo));
	if (cl->playerclass != classnum)
	{
		cl->edict->xv->playerclass = classnum;
		cl->playerclass = classnum;

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

static void QCBUILTIN PF_h2setclass (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
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
	Info_SetValueForKey (client->userinfo, "cl_playerclass", temp, sizeof(client->userinfo));
	client->sendinfo = true;
}

static void QCBUILTIN PF_h2v_factor(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
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

static void QCBUILTIN PF_h2v_factorrange(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
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
static void QCBUILTIN PF_h2plaque_draw(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char		*s;

#ifdef SERVER_DEMO_PLAYBACK
	if (sv.demofile)
		return;
#endif

	if (G_FLOAT(OFS_PARM1) == 0)
		s = "";
	else
	{
		s = T_GetString(G_FLOAT(OFS_PARM1)-1);
	}

	if (G_FLOAT(OFS_PARM0) == MSG_ONE)
	{
		edict_t *ent;
		ent = PROG_TO_EDICT(svprogfuncs, pr_global_struct->msg_entity);
		PF_centerprint_Internal(NUM_FOR_EDICT(svprogfuncs, ent), true, s);
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

static void QCBUILTIN PF_h2movestep (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
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

	G_INT(OFS_RETURN) = World_movestep (&sv.world, (wedict_t*)ent, v, NULL, false, true, set_trace?set_trace_globals:NULL, pr_globals);

// restore program state
	pr_global_struct->self = oldself;
}

static void QCBUILTIN PF_h2concatv(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
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

static void QCBUILTIN PF_h2matchAngleToSlope(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*actor;
	vec3_t v_forward, old_forward, old_right, new_angles2 = { 0, 0, 0 };
	float pitch, mod, dot;


	// OFS_PARM0 is used by PF_vectoangles below
	actor = G_EDICT(prinst, OFS_PARM1);

	AngleVectors(actor->v->angles, old_forward, old_right, P_VEC(v_up));

	VectorAngles(G_VECTOR(OFS_PARM0), NULL, G_VECTOR(OFS_RETURN), true/*FIXME*/);

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
/*objective type stuff, this goes into a stat*/
static void QCBUILTIN PF_h2updateinfoplaque(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	unsigned int idx = G_FLOAT(OFS_PARM0);
	int mode = G_FLOAT(OFS_PARM1);	/*0=toggle, 1=force, 2=clear*/

	if (idx >= sizeof(h2infoplaque)*8)
		return;

	if ((mode & 3) == 3)
		/*idiot*/;
	else if (mode & 1)
		h2infoplaque[idx/32] |= 1<<(idx&31);
	else if (mode & 2)
		h2infoplaque[idx/32] &=~(1<<(idx&31));
	else
		h2infoplaque[idx/32] ^= 1<<(idx&31);
}

enum
{
	ce_none,
	ce_rain,
	ce_fountain,
	ce_quake,
	ce_white_smoke,
	ce_bluespark,
	ce_yellowspark,
	ce_sm_circle_exp,
	ce_bg_circle_exp,
	ce_sm_white_flash,
	ce_white_flash		= 10,
	ce_yellowred_flash,
	ce_blue_flash,
	ce_sm_blue_flash,
	ce_red_flash,
	ce_sm_explosion,
	ce_lg_explosion,
	ce_floor_explosion,
	ce_rider_death,
	ce_blue_explosion,
	ce_green_smoke		= 20,
	ce_grey_smoke,
	ce_red_smoke,
	ce_slow_white_smoke,
	ce_redspark,
	ce_greenspark,
	ce_telesmk1,
	ce_telesmk2,
	ce_icehit,
	ce_medusa_hit,
	ce_mezzo_reflect	= 30,
	ce_floor_explosion2,
	ce_xbow_explosion,
	ce_new_explosion,
	ce_magic_missile_explosion,
	ce_ghost,
	ce_bone_explosion,
	ce_redcloud,
	ce_teleporterpuffs,
	ce_teleporterbody,
	ce_boneshard		= 40,
	ce_boneshrapnel,
	ce_flamestream,
	ce_snow,
	ce_gravitywell,
	ce_bldrn_expl,
	ce_acid_muzzfl,
	ce_acid_hit,
	ce_firewall_small,
	ce_firewall_medium,
	ce_firewall_large	= 50,
	ce_lball_expl,
	ce_acid_splat,
	ce_acid_expl,
	ce_fboom,
	ce_chunk,
	ce_bomb,
	ce_brn_bounce,
	ce_lshock,
	ce_flamewall,
	ce_flamewall2		= 60,
	ce_floor_explosion3,
	ce_onfire,


	/*internal effects, here for indexes*/

	ce_teleporterbody_1,	/*de-sheeped*/
	ce_white_smoke_05,
	ce_white_smoke_10,
	ce_white_smoke_15,
	ce_white_smoke_20,
	ce_white_smoke_50,
	ce_green_smoke_05,
	ce_green_smoke_10,
	ce_green_smoke_15,
	ce_green_smoke_20,
	ce_grey_smoke_15,
	ce_grey_smoke_100,

	ce_chunk_1,
	ce_chunk_2,
	ce_chunk_3,
	ce_chunk_4,
	ce_chunk_5,
	ce_chunk_6,
	ce_chunk_7,
	ce_chunk_8,
	ce_chunk_9,
	ce_chunk_10,
	ce_chunk_11,
	ce_chunk_12,
	ce_chunk_13,
	ce_chunk_14,
	ce_chunk_15,
	ce_chunk_16,
	ce_chunk_17,
	ce_chunk_18,
	ce_chunk_19,
	ce_chunk_20,
	ce_chunk_21,
	ce_chunk_22,
	ce_chunk_23,
	ce_chunk_24,

	ce_max
};
int h2customtents[ce_max];
void SV_RegisterH2CustomTents(void)
{
	int i;
	for (i = 0; i < ce_max; i++)
		h2customtents[i] = -1;

	if (progstype == PROG_H2)
	{
		h2customtents[ce_rain]				= SV_CustomTEnt_Register("h2part.ce_rain",				CTE_PERSISTANT|CTE_CUSTOMVELOCITY|CTE_ISBEAM|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_fountain]			= SV_CustomTEnt_Register("h2part.ce_fountain",			CTE_PERSISTANT|CTE_CUSTOMVELOCITY|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_quake]				= SV_CustomTEnt_Register("h2part.ce_quake",			0, NULL, 0, NULL, 0, 0, NULL);
//	ce_white_smoke (special)
		h2customtents[ce_bluespark]			= SV_CustomTEnt_Register("h2part.ce_bluespark",		0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_yellowspark]		= SV_CustomTEnt_Register("h2part.ce_yellowspark",		0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_sm_circle_exp]		= SV_CustomTEnt_Register("h2part.ce_sm_circle_exp",	0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_bg_circle_exp]		= SV_CustomTEnt_Register("h2part.ce_bg_circle_exp",	0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_sm_white_flash]	= SV_CustomTEnt_Register("h2part.ce_sm_white_flash",	0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_white_flash]		= SV_CustomTEnt_Register("h2part.ce_white_flash",		0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_yellowred_flash]	= SV_CustomTEnt_Register("h2part.ce_yellowred_flash",	0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_blue_flash]		= SV_CustomTEnt_Register("h2part.ce_blue_flash",		0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_sm_blue_flash]		= SV_CustomTEnt_Register("h2part.ce_sm_blue_flash",	0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_red_flash]			= SV_CustomTEnt_Register("h2part.ce_red_flash",		0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_sm_explosion]		= SV_CustomTEnt_Register("h2part.ce_sm_explosion",		0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_lg_explosion]		= SV_CustomTEnt_Register("h2part.ce_lg_explosion",		0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_floor_explosion]	= SV_CustomTEnt_Register("h2part.ce_floor_explosion",	0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_rider_death]		= SV_CustomTEnt_Register("h2part.ce_rider_death",		0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_blue_explosion]	= SV_CustomTEnt_Register("h2part.ce_blue_explosion",	0, NULL, 0, NULL, 0, 0, NULL);
//	ce_green_smoke (special)
//	ce_grey_smoke (special)
		h2customtents[ce_red_smoke]			= SV_CustomTEnt_Register("h2part.ce_red_smoke",		0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_slow_white_smoke]	= SV_CustomTEnt_Register("h2part.ce_slow_white_smoke",	0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_redspark]			= SV_CustomTEnt_Register("h2part.ce_redspark",			0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_greenspark]		= SV_CustomTEnt_Register("h2part.ce_greenspark",		0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_telesmk1]			= SV_CustomTEnt_Register("h2part.ce_telesmk1",			CTE_CUSTOMVELOCITY, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_telesmk2]			= SV_CustomTEnt_Register("h2part.ce_telesmk2",			CTE_CUSTOMVELOCITY, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_icehit]			= SV_CustomTEnt_Register("h2part.ce_icehit",			0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_medusa_hit]		= SV_CustomTEnt_Register("h2part.ce_medusa_hit",		0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_mezzo_reflect]		= SV_CustomTEnt_Register("h2part.ce_mezzo_reflect",	0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_floor_explosion2]	= SV_CustomTEnt_Register("h2part.ce_floor_explosion2",	0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_xbow_explosion]	= SV_CustomTEnt_Register("h2part.ce_xbow_explosion",	0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_new_explosion]		= SV_CustomTEnt_Register("h2part.ce_new_explosion",	0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_magic_missile_explosion]	= SV_CustomTEnt_Register("h2part.ce_magic_missile_explosion", 0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_ghost]				= SV_CustomTEnt_Register("h2part.ce_ghost",			CTE_CUSTOMVELOCITY, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_bone_explosion]	= SV_CustomTEnt_Register("h2part.ce_bone_explosion",	0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_redcloud]			= SV_CustomTEnt_Register("h2part.ce_redcloud",			CTE_CUSTOMVELOCITY, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_teleporterpuffs]	= SV_CustomTEnt_Register("h2part.ce_teleporterpuffs",	0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_teleporterbody]	= SV_CustomTEnt_Register("h2part.ce_teleporterbody",	0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_boneshard]			= SV_CustomTEnt_Register("h2part.ce_boneshard",		CTE_CUSTOMVELOCITY, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_boneshrapnel]		= SV_CustomTEnt_Register("h2part.ce_boneshrapnel",		CTE_CUSTOMVELOCITY, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_flamestream]		= SV_CustomTEnt_Register("h2part.ce_flamestream",		CTE_CUSTOMVELOCITY, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_snow]				= SV_CustomTEnt_Register("h2part.ce_snow",				CTE_PERSISTANT|CTE_CUSTOMVELOCITY|CTE_ISBEAM|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_gravitywell]		= SV_CustomTEnt_Register("h2part.ce_gravitywell",		0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_bldrn_expl]		= SV_CustomTEnt_Register("h2part.ce_bldrn_expl",		0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_acid_muzzfl]		= SV_CustomTEnt_Register("h2part.ce_acid_muzzfl",		CTE_CUSTOMVELOCITY, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_acid_hit]			= SV_CustomTEnt_Register("h2part.ce_acid_hit",			0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_firewall_small]	= SV_CustomTEnt_Register("h2part.ce_firewall_small",	0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_firewall_medium]	= SV_CustomTEnt_Register("h2part.ce_firewall_medium",	0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_firewall_large]	= SV_CustomTEnt_Register("h2part.ce_firewall_large",	0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_lball_expl]		= SV_CustomTEnt_Register("h2part.ce_lball_expl",		0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_acid_splat]		= SV_CustomTEnt_Register("h2part.ce_acid_splat",		0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_acid_expl]			= SV_CustomTEnt_Register("h2part.ce_acid_expl",		0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_fboom]				= SV_CustomTEnt_Register("h2part.ce_fboom",			0, NULL, 0, NULL, 0, 0, NULL);
//	ce_chunk (special)
		h2customtents[ce_bomb]				= SV_CustomTEnt_Register("h2part.ce_bomb",				0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_brn_bounce]		= SV_CustomTEnt_Register("h2part.ce_brn_bounce",		0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_lshock]			= SV_CustomTEnt_Register("h2part.ce_lshock",			0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_flamewall]			= SV_CustomTEnt_Register("h2part.ce_flamewall",		CTE_CUSTOMVELOCITY, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_flamewall2]		= SV_CustomTEnt_Register("h2part.ce_flamewall2",		CTE_CUSTOMVELOCITY, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_floor_explosion3]	= SV_CustomTEnt_Register("h2part.ce_floor_explosion3",	0, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_onfire]			= SV_CustomTEnt_Register("h2part.ce_onfire",			CTE_CUSTOMVELOCITY, NULL, 0, NULL, 0, 0, NULL);



		h2customtents[ce_teleporterbody_1]	= SV_CustomTEnt_Register("h2part.ce_teleporterbody_1",	CTE_CUSTOMVELOCITY, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_white_smoke_05]	= SV_CustomTEnt_Register("h2part.ce_white_smoke_05",	CTE_CUSTOMVELOCITY, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_white_smoke_10]	= SV_CustomTEnt_Register("h2part.ce_white_smoke_10",	CTE_CUSTOMVELOCITY, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_white_smoke_15]	= SV_CustomTEnt_Register("h2part.ce_white_smoke_15",	CTE_CUSTOMVELOCITY, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_white_smoke_20]	= SV_CustomTEnt_Register("h2part.ce_white_smoke_20",	CTE_CUSTOMVELOCITY, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_white_smoke_50]	= SV_CustomTEnt_Register("h2part.ce_white_smoke_50",	CTE_CUSTOMVELOCITY, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_green_smoke_05]	= SV_CustomTEnt_Register("h2part.ce_green_smoke_05",	CTE_CUSTOMVELOCITY, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_green_smoke_10]	= SV_CustomTEnt_Register("h2part.ce_green_smoke_10",	CTE_CUSTOMVELOCITY, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_green_smoke_15]	= SV_CustomTEnt_Register("h2part.ce_green_smoke_15",	CTE_CUSTOMVELOCITY, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_green_smoke_20]	= SV_CustomTEnt_Register("h2part.ce_green_smoke_20",	CTE_CUSTOMVELOCITY, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_grey_smoke_15]		= SV_CustomTEnt_Register("h2part.ce_grey_smoke_15",	CTE_CUSTOMVELOCITY, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_grey_smoke_100]	= SV_CustomTEnt_Register("h2part.ce_grey_smoke_100",	CTE_CUSTOMVELOCITY, NULL, 0, NULL, 0, 0, NULL);

		h2customtents[ce_chunk_1]	= SV_CustomTEnt_Register("h2part.ce_chunk_greystone",		CTE_CUSTOMVELOCITY|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_chunk_2]	= SV_CustomTEnt_Register("h2part.ce_chunk_wood",			CTE_CUSTOMVELOCITY|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_chunk_3]	= SV_CustomTEnt_Register("h2part.ce_chunk_metal",			CTE_CUSTOMVELOCITY|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_chunk_4]	= SV_CustomTEnt_Register("h2part.ce_chunk_flesh",			CTE_CUSTOMVELOCITY|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_chunk_5]	= SV_CustomTEnt_Register("h2part.ce_chunk_fire",			CTE_CUSTOMVELOCITY|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_chunk_6]	= SV_CustomTEnt_Register("h2part.ce_chunk_clay",			CTE_CUSTOMVELOCITY|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_chunk_7]	= SV_CustomTEnt_Register("h2part.ce_chunk_leaves",			CTE_CUSTOMVELOCITY|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_chunk_8]	= SV_CustomTEnt_Register("h2part.ce_chunk_hay",			CTE_CUSTOMVELOCITY|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_chunk_9]	= SV_CustomTEnt_Register("h2part.ce_chunk_brownstone",		CTE_CUSTOMVELOCITY|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_chunk_10]	= SV_CustomTEnt_Register("h2part.ce_chunk_cloth",			CTE_CUSTOMVELOCITY|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_chunk_11]	= SV_CustomTEnt_Register("h2part.ce_chunk_wood_leaf",		CTE_CUSTOMVELOCITY|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_chunk_12]	= SV_CustomTEnt_Register("h2part.ce_chunk_wood_metal",		CTE_CUSTOMVELOCITY|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_chunk_13]	= SV_CustomTEnt_Register("h2part.ce_chunk_wood_stone",		CTE_CUSTOMVELOCITY|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_chunk_14]	= SV_CustomTEnt_Register("h2part.ce_chunk_metal_stone",	CTE_CUSTOMVELOCITY|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_chunk_15]	= SV_CustomTEnt_Register("h2part.ce_chunk_metal_cloth",	CTE_CUSTOMVELOCITY|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_chunk_16]	= SV_CustomTEnt_Register("h2part.ce_chunk_webs",			CTE_CUSTOMVELOCITY|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_chunk_17]	= SV_CustomTEnt_Register("h2part.ce_chunk_glass",			CTE_CUSTOMVELOCITY|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_chunk_18]	= SV_CustomTEnt_Register("h2part.ce_chunk_ice",			CTE_CUSTOMVELOCITY|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_chunk_19]	= SV_CustomTEnt_Register("h2part.ce_chunk_clearglass",		CTE_CUSTOMVELOCITY|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_chunk_20]	= SV_CustomTEnt_Register("h2part.ce_chunk_redglass",		CTE_CUSTOMVELOCITY|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_chunk_21]	= SV_CustomTEnt_Register("h2part.ce_chunk_acid",			CTE_CUSTOMVELOCITY|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_chunk_22]	= SV_CustomTEnt_Register("h2part.ce_chunk_meteor",			CTE_CUSTOMVELOCITY|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_chunk_23]	= SV_CustomTEnt_Register("h2part.ce_chunk_greenflesh",		CTE_CUSTOMVELOCITY|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
		h2customtents[ce_chunk_24]	= SV_CustomTEnt_Register("h2part.ce_chunk_bone",			CTE_CUSTOMVELOCITY|CTE_CUSTOMCOUNT, NULL, 0, NULL, 0, 0, NULL);
	}
}
static void QCBUILTIN PF_h2starteffect(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *min, *max, *size;
//	float *angle;
	float colour;
//	float wait, radius, frame, framelength, duration;
//	int flags, skin;
	int type;
	float *org, *dir;
	int count;

	int efnum = G_FLOAT(OFS_PARM0);
	G_FLOAT(OFS_RETURN) = 0;
	switch(efnum)
	{
	case ce_rain:
		/*this effect is meant to be persistant (endeffect is never used)*/
		min = G_VECTOR(OFS_PARM1);
		max = G_VECTOR(OFS_PARM2);
		size = G_VECTOR(OFS_PARM3);
		dir = G_VECTOR(OFS_PARM4);
		colour = G_FLOAT(OFS_PARM5);
		count = G_FLOAT(OFS_PARM6);
		//wait = G_FLOAT(OFS_PARM7);

		/*FIXME: not spawned - this persistant effect is created by a map object, all attributes are custom.*/

		if (colour == 0 && size == 0)
			SV_CustomTEnt_Spawn(h2customtents[efnum], min, max, count, dir);
		else
			Con_Printf("FTE-H2 FIXME: ce_rain not supported!\n");
		return;
	case ce_snow:
		/*this effect is meant to be persistant (endeffect is never used)*/
		min = G_VECTOR(OFS_PARM1);
		max = G_VECTOR(OFS_PARM2);
		//flags = G_FLOAT(OFS_PARM3);
		dir = G_VECTOR(OFS_PARM4);
		count = G_FLOAT(OFS_PARM5);

		/*FIXME: we ignore any happy/fluffy/mixed snow types*/
		SV_CustomTEnt_Spawn(h2customtents[efnum], min, max, count, dir);
//		Con_Printf("FTE-H2 FIXME: ce_snow not supported!\n");
		return;
	case ce_fountain:
		/*this effect is meant to be persistant (endeffect is never used)*/
		org = G_VECTOR(OFS_PARM1);
//		angle = G_VECTOR(OFS_PARM2);
		dir = G_VECTOR(OFS_PARM3);
		colour = G_FLOAT(OFS_PARM4);
		count = G_FLOAT(OFS_PARM5);

		/*FIXME: not spawned - this persistant effect is created by a map object, all attributes are custom.*/
		if (colour == 407)
		{
			dir[2] *= 2;
			SV_CustomTEnt_Spawn(h2customtents[efnum], org, NULL, count, dir);
		}
		else
			Con_Printf("FTE-H2 FIXME: ce_fountain not supported!\n");
		return;
	case ce_quake:
		/*this effect is meant to be persistant*/
		org = G_VECTOR(OFS_PARM1);
		//radius = G_FLOAT(OFS_PARM2);	/*discard: always 500/3 */

		if (h2customtents[efnum] != -1)
		{
			G_FLOAT(OFS_RETURN) = SV_CustomTEnt_Spawn(h2customtents[efnum], org, NULL, 1, NULL);
			return;
		}
		break;
	case ce_white_smoke:		//(1,2,3,4,10)*0.05
	case ce_green_smoke:		//(1,2,3,4)*0.05
	case ce_grey_smoke:			//(3,20)*0.05
		/*transform them to the correct durations*/
		if (efnum == ce_white_smoke)
		{
			int ifl = G_FLOAT(OFS_PARM3) * 100;
			if (ifl == 5)
				efnum = ce_white_smoke_05;
			else if (ifl == 10)
				efnum = ce_white_smoke_10;
			else if (ifl == 15)
				efnum = ce_white_smoke_15;
			else if (ifl == 20)
				efnum = ce_white_smoke_20;
			else if (ifl == 50)
				efnum = ce_white_smoke_50;
		}
		else if (efnum == ce_green_smoke)
		{
			int ifl = G_FLOAT(OFS_PARM3) * 100;
			if (ifl == 05)
				efnum = ce_green_smoke_05;
			else if (ifl == 10)
				efnum = ce_green_smoke_10;
			else if (ifl == 15)
				efnum = ce_green_smoke_15;
			else if (ifl == 20)
				efnum = ce_green_smoke_20;
		}
		else
		{
			int ifl = G_FLOAT(OFS_PARM3) * 100;
			if (ifl == 15)
				efnum = ce_grey_smoke_15;
			if (ifl == 100)
				efnum = ce_grey_smoke_100;
		}
		/*fallthrough*/
	case ce_red_smoke:			//0.15
	case ce_slow_white_smoke:	//0
	case ce_telesmk1:			//0.15
	case ce_telesmk2:			//not used
	case ce_ghost:				//0.1
	case ce_redcloud:			//0.05
		org = G_VECTOR(OFS_PARM1);
		dir = G_VECTOR(OFS_PARM2);
		//framelength = G_FLOAT(OFS_PARM3);	/*FIXME: validate for the other effects?*/

		if (h2customtents[efnum] != -1)
		{
			SV_CustomTEnt_Spawn(h2customtents[efnum], org, NULL, 1, dir);
			return;
		}
		break;
	case ce_acid_muzzfl:
	case ce_flamestream:
	case ce_flamewall:
	case ce_flamewall2:
	case ce_onfire:
		org = G_VECTOR(OFS_PARM1);
		dir = G_VECTOR(OFS_PARM2);
		//frame = G_FLOAT(OFS_PARM3);			/*discard: h2 uses a fixed value for each effect (0, except for acid_muzzfl which is 0.05)*/

		if (h2customtents[efnum] != -1)
		{
			SV_CustomTEnt_Spawn(h2customtents[efnum], org, NULL, 1, dir);
			return;
		}
		break;
	case ce_sm_white_flash:
	case ce_yellowred_flash:
	case ce_bluespark:
	case ce_yellowspark:
	case ce_sm_circle_exp:
	case ce_bg_circle_exp:
	case ce_sm_explosion:
	case ce_lg_explosion:
	case ce_floor_explosion:
	case ce_floor_explosion3:
	case ce_blue_explosion:
	case ce_redspark:
	case ce_greenspark:
	case ce_icehit:
	case ce_medusa_hit:
	case ce_mezzo_reflect:
	case ce_floor_explosion2:
	case ce_xbow_explosion:
	case ce_new_explosion:
	case ce_magic_missile_explosion:
	case ce_bone_explosion:
	case ce_bldrn_expl:
	case ce_acid_hit:
	case ce_acid_splat:
	case ce_acid_expl:
	case ce_lball_expl:
	case ce_firewall_small:
	case ce_firewall_medium:
	case ce_firewall_large:
	case ce_fboom:
	case ce_bomb:
	case ce_brn_bounce:
	case ce_lshock:
	case ce_white_flash:
	case ce_blue_flash:
	case ce_sm_blue_flash:
	case ce_red_flash:
	case ce_rider_death:
	case ce_teleporterpuffs:
		org = G_VECTOR(OFS_PARM1);
		if (h2customtents[efnum] != -1)
		{
			SV_CustomTEnt_Spawn(h2customtents[efnum], org, NULL, 1, NULL);
			return;
		}
		break;
	case ce_gravitywell:
		org = G_VECTOR(OFS_PARM1);
		//colour = G_FLOAT(OFS_PARM2);		/*discard: h2 uses a fixed random range*/
		//duration = G_FLOAT(OFS_PARM3);	/*discard: h2 uses a fixed time limit*/

		if (h2customtents[efnum] != -1)
		{
			SV_CustomTEnt_Spawn(h2customtents[efnum], org, NULL, 1, NULL);
			return;
		}
		break;
	case ce_teleporterbody:
		org = G_VECTOR(OFS_PARM1);
		dir = G_VECTOR(OFS_PARM2);
		if (G_FLOAT(OFS_PARM3))	/*alternate*/
			efnum = ce_teleporterbody_1;

		if (h2customtents[efnum] != -1)
		{
			SV_CustomTEnt_Spawn(h2customtents[efnum], org, NULL, 1, dir);
			return;
		}
		break;
	case ce_boneshard:
	case ce_boneshrapnel:
		org = G_VECTOR(OFS_PARM1);
		dir = G_VECTOR(OFS_PARM2);
		//angle = G_VECTOR(OFS_PARM3);	/*discard: angle is a function of the dir*/
		//avelocity = G_VECTOR(OFS_PARM4);/*discard: avelocity is a function of the dir*/

		/*FIXME: meant to be persistant until removed*/
		if (h2customtents[efnum] != -1)
		{
			G_FLOAT(OFS_RETURN) = SV_CustomTEnt_Spawn(h2customtents[efnum], org, NULL, 1, dir);
			return;
		}
		break;
	case ce_chunk:
		org = G_VECTOR(OFS_PARM1);
		type = G_FLOAT(OFS_PARM2);
		dir = G_VECTOR(OFS_PARM3);
		count = G_FLOAT(OFS_PARM4);

		/*convert it to the requested chunk type*/
		efnum = ce_chunk_1 + type - 1;
		if (efnum < ce_chunk_1 && efnum > ce_chunk_24)
			efnum = ce_chunk;

		if (h2customtents[efnum] != -1)
		{
			SV_CustomTEnt_Spawn(h2customtents[efnum], org, NULL, count, dir);
			return;
		}

		Con_Printf("FTE-H2 FIXME: ce_chunk type=%i not supported!\n", type);
		return;


		break;
	default:
		PR_BIError (prinst, "PF_h2starteffect: bad effect");
		break;
	}

	Con_Printf("FTE-H2 FIXME: Effect %i doesn't have an effect registered\nTell Spike!\n", efnum);
}

static void QCBUILTIN PF_h2endeffect(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
//	int ign = G_FLOAT(OFS_PARM0);
	int index = G_FLOAT(OFS_PARM1);

	Con_DPrintf("Stop effect %i\n", index);
}

static void QCBUILTIN PF_h2rain_go(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	//used by (tomed) icemace.hc 
	float *min = G_VECTOR(OFS_PARM0);
	float *max = G_VECTOR(OFS_PARM1);
//unused	float *size = G_VECTOR(OFS_PARM2);
	float *dir = G_VECTOR(OFS_PARM3);
	float colour = G_FLOAT(OFS_PARM4);
	float count = G_FLOAT(OFS_PARM5);

	//this is some hacky alternative.
	//fixme: the effect isn't bounded to the box
	MSG_WriteByte(&sv.multicast, svc_temp_entity);
	MSG_WriteByte(&sv.multicast, TEDP_PARTICLERAIN);
	// min
	MSG_WriteCoord(&sv.multicast, min[0]);
	MSG_WriteCoord(&sv.multicast, min[1]);
	MSG_WriteCoord(&sv.multicast, max[2]);
	// max
	MSG_WriteCoord(&sv.multicast, max[0]);
	MSG_WriteCoord(&sv.multicast, max[1]);
	MSG_WriteCoord(&sv.multicast, max[2]);
	// velocity
	MSG_WriteCoord(&sv.multicast, dir[0]);
	MSG_WriteCoord(&sv.multicast, dir[1]);
	MSG_WriteCoord(&sv.multicast, -(frandom()*700+256));	//dir not valid. fill in a default downwards direction
	// count
	MSG_WriteShort(&sv.multicast, min(count, 65535));
	// colour
	MSG_WriteByte(&sv.multicast, (int)colour&0xff);
	SV_MulticastProtExt (NULL, MULTICAST_ALL, pr_global_struct->dimension_send, 0, 0);
}

static void QCBUILTIN PF_h2StopSound(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int			channel;
	edict_t		*entity;

	entity = G_EDICT(prinst, OFS_PARM0);
	channel = G_FLOAT(OFS_PARM1);

	SVQ1_StartSound (NULL, (wedict_t*)entity, channel, NULL, 1, 0, 0, 0, CF_RELIABLE);
}

static void QCBUILTIN PF_h2updatesoundpos(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Con_DPrintf("FTE-H2 FIXME: updatesoundpos not implemented\n");
}

static void QCBUILTIN PF_h2whiteflash(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	/*
	broadcast a stuffcmd, I guess, to flash the screen white
	Only seen this occur once: after killing pravus.
	*/
	MSG_WriteByte(&sv.multicast, svc_stufftext);
	MSG_WriteString(&sv.multicast, "wf\n");
	SV_MulticastProtExt (vec3_origin, MULTICAST_ALL_R, pr_global_struct->dimension_send, PEXT_CUSTOMTEMPEFFECTS, 0);	//now send the new multicast to all that will.
}

static void QCBUILTIN PF_h2getstring(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *s = T_GetString(G_FLOAT(OFS_PARM0)-1);
	RETURN_PSTRING(s);
}
#endif

static void QCBUILTIN PF_RegisterTEnt(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int arg;
	int i;
	int nettype = G_FLOAT(OFS_PARM0);
	const char *effectname = PR_GetStringOfs(prinst, OFS_PARM1);
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

	if (arg != prinst->callargc)
		Con_Printf("Bad argument count\n");


	G_FLOAT(OFS_RETURN) = i;
}

static void QCBUILTIN PF_CustomTEnt(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int type;
	int arg;
	float *org;
	multicast_t mcd = MULTICAST_PVS;

	if (sv.multicast.cursize)
		SV_MulticastProtExt (vec3_origin, MULTICAST_ALL, pr_global_struct->dimension_send, 0, PEXT_CUSTOMTEMPEFFECTS);	//do a multicast with the current buffer to all players who won't get the new effect.

	type = G_FLOAT(OFS_PARM0);
	if (type < 0 || type >= 255)
		return;

	MSG_WriteByte(&sv.multicast, svcfte_customtempent);
	MSG_WriteByte(&sv.multicast, type);
	type = sv.customtents[type].netstyle;
	arg = 1;

	if (type & CTE_PERSISTANT)
	{
		int id = G_EDICTNUM(prinst, OFS_PARM0+arg*3);
		arg++;
		mcd = MULTICAST_ALL_R;

		if (arg == prinst->callargc)
		{
			MSG_WriteShort(&sv.multicast, id | 0x8000);
			SV_MulticastProtExt (vec3_origin, mcd, pr_global_struct->dimension_send, PEXT_CUSTOMTEMPEFFECTS, 0);	//now send the new multicast to all that will.
			return;
		}
		MSG_WriteShort(&sv.multicast, id);
	}

	org = G_VECTOR(OFS_PARM0+arg*3);
	MSG_WriteCoord(&sv.multicast, org[0]);
	MSG_WriteCoord(&sv.multicast, org[1]);
	MSG_WriteCoord(&sv.multicast, org[2]);

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
		if (type & CTE_CUSTOMVELOCITY)
		{
			float *vel = G_VECTOR(OFS_PARM0+arg*3);
			MSG_WriteCoord(&sv.multicast, vel[0]);
			MSG_WriteCoord(&sv.multicast, vel[1]);
			MSG_WriteCoord(&sv.multicast, vel[2]);
			arg++;
		}
		else if (type & CTE_CUSTOMDIRECTION)
		{
			vec3_t norm;
			VectorNormalize2(G_VECTOR(OFS_PARM0+arg*3), norm);
			MSG_WriteDir(&sv.multicast, norm);
			arg++;
		}
	}
	if (arg != prinst->callargc)
		Con_Printf("PF_CusromTEnt: bad number of arguments for particle type\n");

	SV_MulticastProtExt (org, mcd, pr_global_struct->dimension_send, PEXT_CUSTOMTEMPEFFECTS, 0);	//now send the new multicast to all that will.
}

//void(float effectnum, entity ent, vector start, vector end) trailparticles (EXT_CSQC),
void QCBUILTIN PF_sv_trailparticles(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
#ifdef PEXT_CSQC
	int efnum;
	int ednum;
	float *start = G_VECTOR(OFS_PARM2);
	float *end = G_VECTOR(OFS_PARM3);

	/*DP gets this wrong*/
	if (G_INT(OFS_PARM1) >= MAX_EDICTS)
	{
		ednum = G_EDICTNUM(prinst, OFS_PARM0);
		efnum = G_FLOAT(OFS_PARM1);
	}
	else
	{
		efnum = G_FLOAT(OFS_PARM0);
		ednum = G_EDICTNUM(prinst, OFS_PARM1);
	}

	if (efnum <= 0)
	{
//		if (!ssqc_deprecated_warned)
//		{
//			PR_RunWarning(prinst, "PF_sv_trailparticles: invalid effect");
//			ssqc_deprecated_warned = true;
//		}
		return;
	}

	MSG_WriteByte(&sv.multicast, svcfte_trailparticles);
	MSG_WriteEntity(&sv.multicast, ednum);
	MSG_WriteShort(&sv.multicast, efnum);
	MSG_WriteCoord(&sv.multicast, start[0]);
	MSG_WriteCoord(&sv.multicast, start[1]);
	MSG_WriteCoord(&sv.multicast, start[2]);
	MSG_WriteCoord(&sv.multicast, end[0]);
	MSG_WriteCoord(&sv.multicast, end[1]);
	MSG_WriteCoord(&sv.multicast, end[2]);

#ifdef NQPROT
	MSG_WriteByte(&sv.nqmulticast, svcdp_trailparticles);
	MSG_WriteEntity(&sv.nqmulticast, ednum);
	MSG_WriteShort(&sv.nqmulticast, efnum);
	MSG_WriteCoord(&sv.nqmulticast, start[0]);
	MSG_WriteCoord(&sv.nqmulticast, start[1]);
	MSG_WriteCoord(&sv.nqmulticast, start[2]);
	MSG_WriteCoord(&sv.nqmulticast, end[0]);
	MSG_WriteCoord(&sv.nqmulticast, end[1]);
	MSG_WriteCoord(&sv.nqmulticast, end[2]);
#endif

	SV_MulticastProtExt(start, MULTICAST_PHS, pr_global_struct->dimension_send, PEXT_CSQC, 0);
#endif
}
//void(float effectnum, vector origin [, vector dir, float count]) pointparticles (EXT_CSQC)
void QCBUILTIN PF_sv_pointparticles(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
#ifdef PEXT_CSQC
	int efnum = G_FLOAT(OFS_PARM0);
	float *org = G_VECTOR(OFS_PARM1);
	float *vel = (prinst->callargc < 3)?vec3_origin:G_VECTOR(OFS_PARM2);
	int count = (prinst->callargc < 4)?1:G_FLOAT(OFS_PARM3);

	if (efnum <= 0)
	{
//		if (!ssqc_deprecated_warned)
//		{
//			PR_RunWarning(prinst, "PF_sv_pointparticles: invalid effect");
//			ssqc_deprecated_warned = true;
//		}
		return;
	}

	if (count > 65535)
		count = 65535;

	if (count == 1 && DotProduct(vel, vel) == 0)
	{
		MSG_WriteByte(&sv.multicast, svcfte_pointparticles1);
		MSG_WriteShort(&sv.multicast, efnum);
		MSG_WriteCoord(&sv.multicast, org[0]);
		MSG_WriteCoord(&sv.multicast, org[1]);
		MSG_WriteCoord(&sv.multicast, org[2]);

#ifdef NQPROT
		MSG_WriteByte(&sv.nqmulticast, svcdp_pointparticles1);
		MSG_WriteShort(&sv.nqmulticast, efnum);
		MSG_WriteCoord(&sv.nqmulticast, org[0]);
		MSG_WriteCoord(&sv.nqmulticast, org[1]);
		MSG_WriteCoord(&sv.nqmulticast, org[2]);
#endif
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

#ifdef NQPROT
		MSG_WriteByte(&sv.nqmulticast, svcdp_pointparticles);
		MSG_WriteShort(&sv.nqmulticast, efnum);
		MSG_WriteCoord(&sv.nqmulticast, org[0]);
		MSG_WriteCoord(&sv.nqmulticast, org[1]);
		MSG_WriteCoord(&sv.nqmulticast, org[2]);
		MSG_WriteCoord(&sv.nqmulticast, vel[0]);
		MSG_WriteCoord(&sv.nqmulticast, vel[1]);
		MSG_WriteCoord(&sv.nqmulticast, vel[2]);
		MSG_WriteShort(&sv.nqmulticast, count);
#endif
	}
	SV_MulticastProtExt(org, MULTICAST_PHS, pr_global_struct->dimension_send, PEXT_CSQC, 0);
#endif
}

void PRSV_RunThreads(void)
{
	struct globalvars_s *pr_globals;

	qcstate_t *state = qcthreads, *next;
	qcthreads = NULL;
	while(state)
	{
		next = state->next;

		if (state->resumetime > sv.time || state->waiting)
		{	//not time yet, reform original list.
			state->next = qcthreads;
			qcthreads = state;
		}
		else
		{	//call it and forget it ever happened. The Sleep biltin will recreate if needed.
			pr_globals = PR_globals(svprogfuncs, PR_CURRENT);

			pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, EDICT_NUM(svprogfuncs, state->self));
			pr_global_struct->other = EDICT_TO_PROG(svprogfuncs, EDICT_NUM(svprogfuncs, state->other));
			G_FLOAT(OFS_RETURN) = state->returnval;

			svprogfuncs->RunThread(svprogfuncs, state->thread);
			svprogfuncs->parms->memfree(state->thread);
			svprogfuncs->parms->memfree(state);
		}

		state = next;
	}
}

static void PRSV_ClearThreads(void)
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

static void QCBUILTIN PF_Sleep(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float sleeptime;

	sleeptime = G_FLOAT(OFS_PARM0);

	PR_CreateThread(prinst, 1, sv.time + sleeptime, false);

	prinst->AbortStack(prinst);
}

static void QCBUILTIN PF_Fork(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float sleeptime;

	if (svprogfuncs->callargc >= 1)
		sleeptime = G_FLOAT(OFS_PARM0);
	else
		sleeptime = 0;

	PR_CreateThread(prinst, 1, sv.time + sleeptime, false);

	PRSV_RunThreads();

	G_FLOAT(OFS_RETURN) = 0;
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(vector org) te_gunshot = #418;
static void QCBUILTIN PF_te_gunshot(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int count;
	if (svprogfuncs->callargc >= 2)
		count = G_FLOAT(OFS_PARM1);
	else
		count = 1;
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TE_GUNSHOT, count);
}
//DP_TE_QUADEFFECTS1
static void QCBUILTIN PF_te_gunshotquad(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int count;
	if (svprogfuncs->callargc >= 2)
		count = G_FLOAT(OFS_PARM1);
	else
		count = 1;
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TEDP_GUNSHOTQUAD, count);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(vector org) te_spike = #419;
static void QCBUILTIN PF_te_spike(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TE_SPIKE, 1);
}

//DP_TE_QUADEFFECTS1
static void QCBUILTIN PF_te_spikequad(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TEDP_SPIKEQUAD, 1);
}

// FTE_TE_STANDARDEFFECTBUILTINS
static void QCBUILTIN PF_te_lightningblood(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TEQW_LIGHTNINGBLOOD, 1);
}

// FTE_TE_STANDARDEFFECTBUILTINS
static void QCBUILTIN PF_te_bloodqw(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int count;
	if (svprogfuncs->callargc >= 2)
		count = G_FLOAT(OFS_PARM1);
	else
		count = 1;
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TEQW_BLOOD, count);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(vector org) te_superspike = #420;
static void QCBUILTIN PF_te_superspike(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TE_SUPERSPIKE, 1);
}
//DP_TE_QUADEFFECTS1
static void QCBUILTIN PF_te_superspikequad(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TEDP_SUPERSPIKEQUAD, 1);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(vector org) te_explosion = #421;
static void QCBUILTIN PF_te_explosion(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (progstype != PROG_QW)
		SV_point_tempentity(G_VECTOR(OFS_PARM0), TEQW_EXPLOSION_NOSPRITE, 1);
	else
		SV_point_tempentity(G_VECTOR(OFS_PARM0), TE_EXPLOSION, 1);
}
//DP_TE_QUADEFFECTS1
static void QCBUILTIN PF_te_explosionquad(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TEDP_EXPLOSIONQUAD, 1);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(vector org) te_tarexplosion = #422;
static void QCBUILTIN PF_te_tarexplosion(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TE_TAREXPLOSION, 1);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(vector org) te_wizspike = #423;
static void QCBUILTIN PF_te_wizspike(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TE_WIZSPIKE, 1);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(vector org) te_knightspike = #424;
static void QCBUILTIN PF_te_knightspike(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TE_KNIGHTSPIKE, 1);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(vector org) te_lavasplash = #425;
static void QCBUILTIN PF_te_lavasplash(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TE_LAVASPLASH, 1);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(vector org) te_teleport = #426;
static void QCBUILTIN PF_te_teleport(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_point_tempentity(G_VECTOR(OFS_PARM0), TE_TELEPORT, 1);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(vector org, float color, float length) te_explosion2 = #427;
static void QCBUILTIN PF_te_explosion2(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);
	int start = G_FLOAT(OFS_PARM1);
	int length = G_FLOAT(OFS_PARM2);
	start = bound(0, start, 255);
	length = bound(0, length, 255-start);

	MSG_WriteByte (&sv.multicast, svc_temp_entity);
	MSG_WriteByte (&sv.multicast, TEQW_EXPLOSION2);
	MSG_WriteCoord (&sv.multicast, org[0]);
	MSG_WriteCoord (&sv.multicast, org[1]);
	MSG_WriteCoord (&sv.multicast, org[2]);
	MSG_WriteByte (&sv.multicast, start);
	MSG_WriteByte (&sv.multicast, length);
#ifdef NQPROT
	MSG_WriteByte (&sv.nqmulticast, svc_temp_entity);
	MSG_WriteByte (&sv.nqmulticast, TENQ_EXPLOSION2);
	MSG_WriteCoord (&sv.nqmulticast, org[0]);
	MSG_WriteCoord (&sv.nqmulticast, org[1]);
	MSG_WriteCoord (&sv.nqmulticast, org[2]);
	MSG_WriteByte (&sv.nqmulticast, start);
	MSG_WriteByte (&sv.nqmulticast, length);
#endif
	SV_MulticastProtExt(org, MULTICAST_PHS, pr_global_struct->dimension_send, 0, 0);
}

//DP_TE_FLAMEJET
static void QCBUILTIN PF_te_flamejet(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);
	float *vel = G_VECTOR(OFS_PARM1);
	float howmany = bound(0,G_FLOAT(OFS_PARM2),255);

	MSG_WriteByte (&sv.multicast, svc_temp_entity);
	MSG_WriteByte (&sv.multicast, TEDP_FLAMEJET);
	MSG_WriteCoord (&sv.multicast, org[0]);
	MSG_WriteCoord (&sv.multicast, org[1]);
	MSG_WriteCoord (&sv.multicast, org[2]);
	MSG_WriteCoord (&sv.multicast, vel[0]);
	MSG_WriteCoord (&sv.multicast, vel[1]);
	MSG_WriteCoord (&sv.multicast, vel[2]);
	MSG_WriteByte (&sv.multicast, howmany);
#ifdef NQPROT
	MSG_WriteByte (&sv.nqmulticast, svc_temp_entity);
	MSG_WriteByte (&sv.nqmulticast, TEDP_FLAMEJET);
	MSG_WriteCoord (&sv.nqmulticast, org[0]);
	MSG_WriteCoord (&sv.nqmulticast, org[1]);
	MSG_WriteCoord (&sv.nqmulticast, org[2]);
	MSG_WriteCoord (&sv.nqmulticast, vel[0]);
	MSG_WriteCoord (&sv.nqmulticast, vel[1]);
	MSG_WriteCoord (&sv.nqmulticast, vel[2]);
	MSG_WriteByte (&sv.nqmulticast, howmany);
#endif
	SV_MulticastProtExt(org, MULTICAST_PHS, pr_global_struct->dimension_send, 0, 0);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(entity own, vector start, vector end) te_lightning1 = #428;
static void QCBUILTIN PF_te_lightning1(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_beam_tempentity(G_EDICTNUM(prinst, OFS_PARM0), G_VECTOR(OFS_PARM1), G_VECTOR(OFS_PARM2), TE_LIGHTNING1);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(entity own, vector start, vector end) te_lightning2 = #429;
static void QCBUILTIN PF_te_lightning2(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_beam_tempentity(G_EDICTNUM(prinst, OFS_PARM0), G_VECTOR(OFS_PARM1), G_VECTOR(OFS_PARM2), TE_LIGHTNING2);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(entity own, vector start, vector end) te_lightning3 = #430;
static void QCBUILTIN PF_te_lightning3(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_beam_tempentity(G_EDICTNUM(prinst, OFS_PARM0), G_VECTOR(OFS_PARM1), G_VECTOR(OFS_PARM2), TE_LIGHTNING3);
}

//DP_TE_STANDARDEFFECTBUILTINS
//void(entity own, vector start, vector end) te_beam = #431;
static void QCBUILTIN PF_te_beam(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	SV_beam_tempentity(G_EDICTNUM(prinst, OFS_PARM0), G_VECTOR(OFS_PARM1), G_VECTOR(OFS_PARM2), TEQW_BEAM);
}

static void QCBUILTIN PF_te_muzzleflash(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t *e = G_EDICT(prinst, OFS_PARM0);
//this is for lamers with old (or unsupported) clients
	if (progstype == PROG_QW)
	{
		MSG_WriteByte (&sv.multicast, svc_muzzleflash);
		MSG_WriteEntity (&sv.multicast, NUM_FOR_EDICT(prinst, e));

		SV_MulticastProtExt (e->v->origin, MULTICAST_PHS, pr_global_struct->dimension_send, 0, 0);
	}
	else
	{	//consistent with nq, this'll be cleaned up elsewhere.
		e->v->effects = (int)e->v->effects | EF_MUZZLEFLASH;
	}
}

//DP_TE_SPARK
static void QCBUILTIN PF_te_spark(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);

	if (G_FLOAT(OFS_PARM2) < 1)
		return;

	MSG_WriteByte(&sv.multicast, svc_temp_entity);
	MSG_WriteByte(&sv.multicast, TEDP_SPARK);
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
	MSG_WriteByte(&sv.nqmulticast, TEDP_SPARK);
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
	SV_MulticastProtExt (org, MULTICAST_PVS, pr_global_struct->dimension_send, 0, 0);
}

// #416 void(vector org) te_smallflash (DP_TE_SMALLFLASH)
static void QCBUILTIN PF_te_smallflash(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);
	SV_point_tempentity(org, TEDP_SMALLFLASH, 0);
}

// #417 void(vector org, float radius, float lifetime, vector color) te_customflash (DP_TE_CUSTOMFLASH)
static void QCBUILTIN PF_te_customflash(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);

	if (G_FLOAT(OFS_PARM1) <= 0 || G_FLOAT(OFS_PARM2) < (1.0 / 256.0))
		return;
	MSG_WriteByte(&sv.multicast, svc_temp_entity);
	MSG_WriteByte(&sv.multicast, TEDP_CUSTOMFLASH);
	// origin
	MSG_WriteCoord(&sv.multicast, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteCoord(&sv.multicast, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteCoord(&sv.multicast, G_VECTOR(OFS_PARM0)[2]);
	// radius
	MSG_WriteByte(&sv.multicast, bound(1, G_FLOAT(OFS_PARM1) / 8 - 1, 255));
	// lifetime
	MSG_WriteByte(&sv.multicast, bound(0, G_FLOAT(OFS_PARM2) * 256 - 1, 255));
	// color
	MSG_WriteByte(&sv.multicast, bound(0, G_VECTOR(OFS_PARM3)[0] * 255, 255));
	MSG_WriteByte(&sv.multicast, bound(0, G_VECTOR(OFS_PARM3)[1] * 255, 255));
	MSG_WriteByte(&sv.multicast, bound(0, G_VECTOR(OFS_PARM3)[2] * 255, 255));

#ifdef NQPROT
	MSG_WriteByte(&sv.nqmulticast, svc_temp_entity);
	MSG_WriteByte(&sv.nqmulticast, TEDP_CUSTOMFLASH);
	// origin
	MSG_WriteCoord(&sv.nqmulticast, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteCoord(&sv.nqmulticast, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteCoord(&sv.nqmulticast, G_VECTOR(OFS_PARM0)[2]);
	// radius
	MSG_WriteByte(&sv.nqmulticast, bound(0, G_FLOAT(OFS_PARM1) / 8 - 1, 255));
	// lifetime
	MSG_WriteByte(&sv.nqmulticast, bound(0, G_FLOAT(OFS_PARM2) * 256 - 1, 255));
	// color
	MSG_WriteByte(&sv.nqmulticast, bound(0, G_VECTOR(OFS_PARM3)[0] * 255, 255));
	MSG_WriteByte(&sv.nqmulticast, bound(0, G_VECTOR(OFS_PARM3)[1] * 255, 255));
	MSG_WriteByte(&sv.nqmulticast, bound(0, G_VECTOR(OFS_PARM3)[2] * 255, 255));
#endif
	SV_MulticastProtExt (org, MULTICAST_PVS, pr_global_struct->dimension_send, 0, 0);
}

//#408 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color, float gravityflag, float randomveljitter) te_particlecube (DP_TE_PARTICLECUBE)
static void QCBUILTIN PF_te_particlecube(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
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
	MSG_WriteByte (&sv.multicast, TEDP_PARTICLECUBE);
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
	MSG_WriteByte (&sv.nqmulticast, TEDP_PARTICLECUBE);
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
	SV_MulticastProtExt (org, MULTICAST_PVS, pr_global_struct->dimension_send, 0, 0);
}

static void QCBUILTIN PF_te_explosionrgb(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);
	float *colour = G_VECTOR(OFS_PARM0);

	MSG_WriteByte(&sv.multicast, svc_temp_entity);
	MSG_WriteByte(&sv.multicast, TEDP_EXPLOSIONRGB);
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
	MSG_WriteByte(&sv.nqmulticast, TEDP_EXPLOSIONRGB);
	// origin
	MSG_WriteCoord(&sv.nqmulticast, org[0]);
	MSG_WriteCoord(&sv.nqmulticast, org[1]);
	MSG_WriteCoord(&sv.nqmulticast, org[2]);
	// color
	MSG_WriteByte(&sv.nqmulticast, bound(0, (int) (colour[0] * 255), 255));
	MSG_WriteByte(&sv.nqmulticast, bound(0, (int) (colour[1] * 255), 255));
	MSG_WriteByte(&sv.nqmulticast, bound(0, (int) (colour[2] * 255), 255));
#endif
	SV_MulticastProtExt (org, MULTICAST_PVS, pr_global_struct->dimension_send, 0, 0);
}

static void QCBUILTIN PF_te_particlerain(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *min = G_VECTOR(OFS_PARM0);
	float *max = G_VECTOR(OFS_PARM1);
	float *velocity = G_VECTOR(OFS_PARM2);
	float count = G_FLOAT(OFS_PARM3);
	float colour = G_FLOAT(OFS_PARM4);

	if (count < 1)
		return;

	MSG_WriteByte(&sv.multicast, svc_temp_entity);
	MSG_WriteByte(&sv.multicast, TEDP_PARTICLERAIN);
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
	MSG_WriteShort(&sv.multicast, min(count, 65535));
	// colour
	MSG_WriteByte(&sv.multicast, colour);

#ifdef NQPROT
	MSG_WriteByte(&sv.nqmulticast, svc_temp_entity);
	MSG_WriteByte(&sv.nqmulticast, TEDP_PARTICLERAIN);
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
	MSG_WriteShort(&sv.nqmulticast, min(count, 65535));
	// colour
	MSG_WriteByte(&sv.nqmulticast, colour);
#endif

	SV_MulticastProtExt (NULL, MULTICAST_ALL, pr_global_struct->dimension_send, 0, 0);
}

static void QCBUILTIN PF_te_particlesnow(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *min = G_VECTOR(OFS_PARM0);
	float *max = G_VECTOR(OFS_PARM1);
	float *velocity = G_VECTOR(OFS_PARM2);
	float count = G_FLOAT(OFS_PARM3);
	float colour = G_FLOAT(OFS_PARM4);

	if (count < 1)
		return;

	MSG_WriteByte(&sv.multicast, svc_temp_entity);
	MSG_WriteByte(&sv.multicast, TEDP_PARTICLESNOW);
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
	MSG_WriteShort(&sv.multicast, min(count, 65535));
	// colour
	MSG_WriteByte(&sv.multicast, colour);

#ifdef NQPROT
	MSG_WriteByte(&sv.nqmulticast, svc_temp_entity);
	MSG_WriteByte(&sv.nqmulticast, TEDP_PARTICLESNOW);
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
	MSG_WriteShort(&sv.nqmulticast, min(count, 65535));
	// colour
	MSG_WriteByte(&sv.nqmulticast, colour);
#endif

	SV_MulticastProtExt (NULL, MULTICAST_ALL, pr_global_struct->dimension_send, 0, 0);
}

// #406 void(vector mincorner, vector maxcorner, float explosionspeed, float howmany) te_bloodshower (DP_TE_BLOODSHOWER)
static void QCBUILTIN PF_te_bloodshower(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	// [vector] min [vector] max [coord] explosionspeed [short] count
	float *min = G_VECTOR(OFS_PARM0);
	float *max = G_VECTOR(OFS_PARM1);
	float speed = G_FLOAT(OFS_PARM2);
	float count = G_FLOAT(OFS_PARM3);
	vec3_t org;

	MSG_WriteByte(&sv.multicast, svc_temp_entity);
	MSG_WriteByte(&sv.multicast, TEDP_BLOODSHOWER);
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
	MSG_WriteByte(&sv.nqmulticast, TEDP_BLOODSHOWER);
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
	SV_MulticastProtExt (org, MULTICAST_PVS, pr_global_struct->dimension_send, 0, 0);	//fixme: should this be phs instead?
}

//DP_SV_EFFECT
//void(vector org, string modelname, float startframe, float endframe, float framerate) effect = #404;
static void QCBUILTIN PF_effect(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);
	const char *name = PR_GetStringOfs(prinst, OFS_PARM1);
	float startframe = G_FLOAT(OFS_PARM2);
	float endframe = G_FLOAT(OFS_PARM3);
	float framerate = G_FLOAT(OFS_PARM4);
	int index = SV_ModelIndex(name);

	SV_Effect(org, index, startframe, endframe, framerate);
}

//DP_TE_PLASMABURN
//void(vector org) te_plasmaburn = #433;
static void QCBUILTIN PF_te_plasmaburn(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);
	SV_point_tempentity(org, TEDP_PLASMABURN, 0);
}


int PF_ForceInfoKey_Internal(unsigned int entnum, const char *key, const char *value)
{
	if (entnum == 0)
	{	//localinfo
		Info_SetValueForKey (localinfo, key, value, MAX_LOCALINFO_STRING);
		return 2;
	}
	else if (entnum <= sv.allocated_client_slots)
	{	//woo. we found a client.
		char *oldvalue;
		if (svs.clients[entnum-1].state == cs_free)
		{
			Con_DPrintf("PF_ForceInfoKey: inactive client\n");
			return 0;
		}
		oldvalue = Info_ValueForKey(svs.clients[entnum-1].userinfo, key);
		if (strcmp(oldvalue, value))
		{
			Info_SetValueForStarKey(svs.clients[entnum-1].userinfo, key, value, sizeof(svs.clients[entnum-1].userinfo));
			
			SV_ExtractFromUserinfo (&svs.clients[entnum-1], false);

			MSG_WriteByte (&sv.reliable_datagram, svc_setinfo);
			MSG_WriteByte (&sv.reliable_datagram, entnum-1);
			MSG_WriteString (&sv.reliable_datagram, key);
			MSG_WriteString (&sv.reliable_datagram, Info_ValueForKey(svs.clients[entnum-1].userinfo, key));

#ifdef NQPROT
			if (!strcmp(key, "name"))
			{
				MSG_WriteByte(&sv.nqreliable_datagram, svc_updatename);
				MSG_WriteByte(&sv.nqreliable_datagram, entnum-1);
				MSG_WriteString (&sv.nqreliable_datagram, Info_ValueForKey(svs.clients[entnum-1].userinfo, key));
			}
			else if (!strcmp(key, "topcolor") || !strcmp(key, "bottomcolor"))
			{
				int c;
				//this sucks, but whatever.
				c = (atoi(Info_ValueForKey(svs.clients[entnum-1].userinfo, "topcolor"   )) & 0xf)<<4;
				c|= (atoi(Info_ValueForKey(svs.clients[entnum-1].userinfo, "bottomcolor")) & 0xf);
				MSG_WriteByte(&sv.nqreliable_datagram, svc_updatecolors);
				MSG_WriteByte(&sv.nqreliable_datagram, entnum-1);
				MSG_WriteByte (&sv.nqreliable_datagram, c);
			}
#endif

			if (!strcmp(key, "*spectator"))
			{
				int ns = !!atoi(value);

				if (svs.clients[entnum-1].state == cs_spawned)
				{
					if (svs.clients[entnum-1].spectator)
						sv.spawned_observer_slots--;
					else
						sv.spawned_client_slots--;
					svs.clients[entnum-1].spectator = ns;
					if (ns)
						sv.spawned_observer_slots++;
					else
						sv.spawned_client_slots++;
				}
				else
					svs.clients[entnum-1].spectator = ns;
			}
#ifdef _DEBUG
			if (!strcmp(key, "*transfer"))
				Con_Printf("WARNING: *transfer is no longer supported\n");
#endif
		}

		return 1;
	}
	else
		Con_DPrintf("PF_ForceInfoKey: not world or client\n");
	return 0;
}

void QCBUILTIN PF_ForceInfoKey(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	edict_t	*e;
	const char	*value;
	const char	*key;

	e = G_EDICT(prinst, OFS_PARM0);
	key = PR_GetStringOfs(prinst, OFS_PARM1);
	value = PR_GetStringOfs(prinst, OFS_PARM2);

	G_FLOAT(OFS_RETURN) = PF_ForceInfoKey_Internal(e->entnum, key, value);
}

/*
=================
PF_setcolors

sets the color of a client and broadcasts the update to all connected clients

setcolors(clientent, value)
=================
*/

//from lh
static void QCBUILTIN PF_setcolors (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	client_t	*client;
	int			entnum, i;
	char number[8];

	entnum = G_EDICTNUM(prinst, OFS_PARM0);
	i = G_FLOAT(OFS_PARM1);

	if (entnum < 1 || entnum > sv.allocated_client_slots)
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
	SV_ExtractFromUserinfo (client, true);
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
static void QCBUILTIN PF_ShowPic(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *slot	= PR_GetStringOfs(prinst, OFS_PARM0);
	const char *picname	= PR_GetStringOfs(prinst, OFS_PARM1);
	float x				= G_FLOAT(OFS_PARM2);
	float y				= G_FLOAT(OFS_PARM3);
	unsigned int zone	= G_FLOAT(OFS_PARM4);
	int entnum;

	client_t *cl;

	ParamNegateFix( &x, &y, zone );

	if (prinst->callargc==6)
	{	//to a single client
		entnum = G_EDICTNUM(prinst, OFS_PARM5)-1;
		if (entnum < 0 || entnum >= sv.allocated_client_slots)
			PR_RunError (prinst, "PF_ShowPic: not a client");

		if (!(svs.clients[entnum].fteprotocolextensions & PEXT_SHOWPIC))
			return;	//need an extension for this. duh.

		cl = ClientReliableWrite_BeginSplit(&svs.clients[entnum], svcfte_showpic, 8 + strlen(slot)+strlen(picname));
		ClientReliableWrite_Byte(cl, zone);
		ClientReliableWrite_String(cl, slot);
		ClientReliableWrite_String(cl, picname);
		ClientReliableWrite_Short(cl, x);
		ClientReliableWrite_Short(cl, y);
	}
	else
	{
		prinst->callargc = 6;
		for (entnum = 0; entnum < sv.allocated_client_slots; entnum++)
		{
			G_INT(OFS_PARM5) = EDICT_TO_PROG(prinst, EDICT_NUM(prinst, entnum+1));
			PF_ShowPic(prinst, pr_globals);
		}
	}
};

static void QCBUILTIN PF_HidePic(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	client_t *cl;
	const char *slot	= PR_GetStringOfs(prinst, OFS_PARM0);
	int entnum;

	if (prinst->callargc==2)
	{	//to a single client
		entnum = G_EDICTNUM(prinst, OFS_PARM1)-1;
		if (entnum < 0 || entnum >= sv.allocated_client_slots)
			PR_RunError (prinst, "PF_HidePic: not a client");

		if (!(svs.clients[entnum].fteprotocolextensions & PEXT_SHOWPIC))
			return;	//need an extension for this. duh.

		cl = ClientReliableWrite_BeginSplit(&svs.clients[entnum], svcfte_hidepic, 2 + strlen(slot));
		ClientReliableWrite_String(cl, slot);
	}
	else
	{
		prinst->callargc = 2;
		for (entnum = 0; entnum < sv.allocated_client_slots; entnum++)
		{
			G_INT(OFS_PARM1) = EDICT_TO_PROG(prinst, EDICT_NUM(prinst, entnum+1));
			PF_HidePic(prinst, pr_globals);
		}
	}
};


static void QCBUILTIN PF_MovePic(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *slot	= PR_GetStringOfs(prinst, OFS_PARM0);
	float x		= G_FLOAT(OFS_PARM1);
	float y		= G_FLOAT(OFS_PARM2);
	float zone	= G_FLOAT(OFS_PARM3);
	int entnum;
	client_t *cl;

	ParamNegateFix( &x, &y, zone );

	if (prinst->callargc==5)
	{	//to a single client
		entnum = G_EDICTNUM(prinst, OFS_PARM4)-1;
		if (entnum < 0 || entnum >= sv.allocated_client_slots)
			PR_RunError (prinst, "PF_MovePic: not a client");

		if (!(svs.clients[entnum].fteprotocolextensions & PEXT_SHOWPIC))
			return;	//need an extension for this. duh.

		cl = ClientReliableWrite_BeginSplit(&svs.clients[entnum], svcfte_movepic, 6 + strlen(slot));
		ClientReliableWrite_String(cl, slot);
		ClientReliableWrite_Byte(cl, zone);
		ClientReliableWrite_Short(cl, x);
		ClientReliableWrite_Short(cl, y);
	}
	else
	{
		prinst->callargc = 5;
		for (entnum = 0; entnum < sv.allocated_client_slots; entnum++)
		{
			G_INT(OFS_PARM4) = EDICT_TO_PROG(prinst, EDICT_NUM(prinst, entnum+1));
			PF_MovePic(prinst, pr_globals);
		}
	}
};

static void QCBUILTIN PF_ChangePic(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *slot	= PR_GetStringOfs(prinst, OFS_PARM0);
	const char *newpic= PR_GetStringOfs(prinst, OFS_PARM1);
	int entnum;
	client_t *cl;

	if (prinst->callargc==3)
	{	//to a single client
		entnum = G_EDICTNUM(prinst, OFS_PARM2)-1;
		if (entnum < 0 || entnum >= sv.allocated_client_slots)
			PR_RunError (prinst, "PF_ChangePic: not a client");

		if (!(svs.clients[entnum].fteprotocolextensions & PEXT_SHOWPIC))
			return;	//need an extension for this. duh.

		cl = ClientReliableWrite_BeginSplit(&svs.clients[entnum], svcfte_updatepic, 3 + strlen(slot)+strlen(newpic));
		ClientReliableWrite_String(cl, slot);
		ClientReliableWrite_String(cl, newpic);
	}
	else
	{
		prinst->callargc = 3;
		for (entnum = 0; entnum < sv.allocated_client_slots; entnum++)
		{
			G_INT(OFS_PARM2) = EDICT_TO_PROG(prinst, EDICT_NUM(prinst, entnum+1));
			PF_ChangePic(prinst, pr_globals);
		}
	}
}

//the first implementation of this function was (float type, float num, string name)
//it is now float num, float type, .field
//EXT_CSQC_1
static void QCBUILTIN PF_clientstat(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
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
static void QCBUILTIN PF_globalstat(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *name = PF_VarString(prinst, 2, pr_globals);
#if 0 //this is the old code
	SV_QCStatName(G_FLOAT(OFS_PARM0), name, G_FLOAT(OFS_PARM1));
#else
	SV_QCStatGlobal(G_FLOAT(OFS_PARM1), name, G_FLOAT(OFS_PARM0));
#endif
}

//void(float num, float type, void *ptr) pointerstat
static void QCBUILTIN PF_pointerstat(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int num = G_FLOAT(OFS_PARM0);
	int type = G_FLOAT(OFS_PARM1);
	int addr = G_INT(OFS_PARM2);
	int size = (type == ev_vector)?sizeof(vec3_t):sizeof(float);
	if (addr < 0 || addr+size >= prinst->stringtablesize)
		prinst->RunError(prinst, "QCVM address %#x is not valid.", addr);
	else
		SV_QCStatPtr(type, prinst->stringtable+addr, num);
}

//EXT_CSQC_1
static void QCBUILTIN PF_runclientphys(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	unsigned int i, n;
	extern qbyte *playertouch;
	extern size_t playertouchmax;
	unsigned int msecs;
	extern cvar_t sv_gravity;
	edict_t *ent = G_EDICT(prinst, OFS_PARM0);
	edict_t *touched;

	if (!ent || ent->readonly)
	{
		Con_Printf("runplayerphysics called on read-only entity\n");
		return;
	}

	if (pr_global_ptrs->clientcommandframe)
		pmove.sequence = *pr_global_ptrs->clientcommandframe;
	else
		pmove.sequence = 0;

	pmove.pm_type = SV_PMTypeForClient((host_client && host_client->edict == ent)?host_client:NULL, ent);

	pmove.jump_msec = 0;

	pmove.jump_held = ((int)ent->xv->pmove_flags)&PMF_JUMP_HELD;
	if (progstype != PROG_QW)	//this is just annoying.
		pmove.waterjumptime = ent->v->teleport_time - sv.time;
	else
		pmove.waterjumptime = ent->v->teleport_time;

//set up the movement command
	msecs = pr_global_struct->input_timelength*1000 + 0.5f;
	//precision inaccuracies. :(
	pmove.cmd.angles[0] = ANGLE2SHORT((pr_global_struct->input_angles)[0]);
	pmove.cmd.angles[1] = ANGLE2SHORT((pr_global_struct->input_angles)[1]);
	pmove.cmd.angles[2] = ANGLE2SHORT((pr_global_struct->input_angles)[2]);
	VectorCopy(pr_global_struct->input_angles, pmove.angles);

	pmove.cmd.forwardmove = bound(-32767, (pr_global_struct->input_movevalues)[0], 32767);
	pmove.cmd.sidemove = bound(-32767, (pr_global_struct->input_movevalues)[1], 32767);
	pmove.cmd.upmove = bound(-32767, (pr_global_struct->input_movevalues)[2], 32767);
	pmove.cmd.buttons = pr_global_struct->input_buttons;

	pmove.safeorigin_known = true;
	VectorCopy(ent->v->oldorigin, pmove.safeorigin);
	VectorCopy(ent->v->origin, pmove.origin);
	VectorCopy(ent->v->velocity, pmove.velocity);
	VectorCopy(ent->v->maxs, pmove.player_maxs);
	VectorCopy(ent->v->mins, pmove.player_mins);
	VectorCopy(ent->xv->gravitydir, pmove.gravitydir);

	//update entity-specific stuff
	movevars.entgravity = sv_gravity.value/movevars.gravity;
	if (ent->xv->gravity)
		movevars.entgravity *= ent->xv->gravity;
	movevars.maxspeed = ent->xv->maxspeed?ent->xv->maxspeed:sv_maxspeed.value;
#ifdef HEXEN2
	if (ent->xv->hasted)
		movevars.maxspeed *= ent->xv->hasted;
#endif

	pmove.numtouch = 0;
	pmove.world = &sv.world;
	pmove.skipent = -1;
	pmove.numphysent = 1;
	pmove.physents[0].model = sv.world.worldmodel;

	pmove.onladder = false;
	pmove.onground = false;
	pmove.groundent = 0;
	pmove.waterlevel = 0;
	pmove.watertype = 0;
	pmove.capsule = (ent->xv->geomtype == GEOMTYPE_CAPSULE);

	for (i=0 ; i<3 ; i++)
	{
		pmove_mins[i] = pmove.origin[i] - 256;
		pmove_maxs[i] = pmove.origin[i] + 256;
	}
	AddLinksToPmove(ent, sv.world.areanodes);
//	AddAllEntsToPmove();
	AddLinksToPmove_Force ( ent, &sv.world.portallist );

	SV_PreRunCmd();

	while(msecs)	//break up longer commands
	{
		if (msecs > 50)
			pmove.cmd.msec = 50;
		else
			pmove.cmd.msec = msecs;
		msecs -= pmove.cmd.msec;
		PM_PlayerMove(1);

		ent->xv->pmove_flags = 0;
		ent->xv->pmove_flags += ((int)pmove.jump_held?PMF_JUMP_HELD:0);
		ent->xv->pmove_flags += ((int)pmove.onladder?PMF_LADDER:0);
		if (progstype != PROG_QW)	//this is just annoying.
			ent->v->teleport_time = sv.time + pmove.waterjumptime;
		else
			ent->v->teleport_time = pmove.waterjumptime;
		VectorCopy(pmove.origin, ent->v->origin);
		VectorCopy(pmove.safeorigin, ent->v->oldorigin);
		VectorCopy(pmove.velocity, ent->v->velocity);


		VectorCopy(pmove.angles, ent->v->v_angle);


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
			ent->v->flags = (int)ent->v->flags | FL_ONGROUND;
			ent->v->groundentity = EDICT_TO_PROG(svprogfuncs, EDICT_NUM(svprogfuncs, pmove.physents[pmove.groundent].info));
		}
		else
			ent->v->flags = (int)ent->v->flags & ~FL_ONGROUND;



		World_LinkEdict(&sv.world, (wedict_t*)ent, true);
		for (i=0 ; i<pmove.numtouch ; i++)
		{
			if (pmove.physents[pmove.touchindex[i]].notouch)
				continue;
			n = pmove.physents[pmove.touchindex[i]].info;
			touched = EDICT_NUM(svprogfuncs, n);
			if (!touched->v->touch || n >= playertouchmax || (playertouch[n/8]&(1<<(n%8))))
				continue;

			sv.world.Event_Touch(&sv.world, (wedict_t*)touched, (wedict_t*)ent);
			playertouch[n/8] |= 1 << (n%8);
		}
		pmove.numtouch = 0;
	}
}


//EXT_CSQC_1 (called when a movement command is received. runs full acceleration + movement)
qboolean SV_RunFullQCMovement(client_t *client, usercmd_t *ucmd)
{
	if (gfuncs.RunClientCommand)
	{
		vec3_t startangle;
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
		VectorCopy(sv_player->v->v_angle, startangle);

#ifdef HEXEN2
		if (progstype == PROG_H2)
			sv_player->xv->light_level = 128;	//hmm... HACK!!!
#endif

		sv_player->v->button0 = ucmd->buttons & 1;
		sv_player->v->button2 = (ucmd->buttons >> 1) & 1;
	// DP_INPUTBUTTONS
		sv_player->xv->button3 = ((ucmd->buttons >> 2) & 1);
		sv_player->xv->button4 = ((ucmd->buttons >> 3) & 1);
		sv_player->xv->button5 = ((ucmd->buttons >> 4) & 1);
		sv_player->xv->button6 = ((ucmd->buttons >> 5) & 1);
		sv_player->xv->button7 = ((ucmd->buttons >> 6) & 1);
		sv_player->xv->button8 = ((ucmd->buttons >> 7) & 1);
		if (ucmd->impulse && SV_FilterImpulse(ucmd->impulse, host_client->trustlevel))
			sv_player->v->impulse = ucmd->impulse;

		if (host_client->penalties & BAN_CUFF)
		{
			sv_player->v->impulse = 0;
			sv_player->v->button0 = 0;
		}

		if (host_client->state && host_client->protocol != SCP_BAD)
		{
			sv_player->xv->movement[0] = ucmd->forwardmove;
			sv_player->xv->movement[1] = ucmd->sidemove;
			sv_player->xv->movement[2] = ucmd->upmove;
		}

		WPhys_CheckVelocity(&sv.world, (wedict_t*)sv_player);

	//
	// angles
	// show 1/3 the pitch angle and all the roll angle
		if (sv_player->v->health > 0)
		{
			if (!sv_player->v->fixangle)
			{
				sv_player->v->angles[PITCH] = r_meshpitch.value * sv_player->v->v_angle[PITCH]/3;
				sv_player->v->angles[YAW] = sv_player->v->v_angle[YAW];
			}
			sv_player->v->angles[ROLL] =
				V_CalcRoll (sv_player->v->angles, sv_player->v->velocity)*4;
		}

		//prethink should be consistant with what the engine normally does
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, client->edict);
		PR_ExecuteProgram (svprogfuncs, pr_global_struct->PlayerPreThink);
		WPhys_RunThink (&sv.world, (wedict_t*)client->edict);










		pr_global_struct->input_timelength = ucmd->msec/1000.0f * sv.gamespeed;
		pr_global_struct->input_impulse = ucmd->impulse;
	//precision inaccuracies. :(
#define ANGLE2SHORT(x) (x) * (65536/360.0)
		if (sv_player->v->fixangle)
		{
			(pr_global_struct->input_angles)[0] = sv_player->v->v_angle[0];
			(pr_global_struct->input_angles)[1] = sv_player->v->v_angle[1];
			(pr_global_struct->input_angles)[2] = sv_player->v->v_angle[2];
		}
		else
		{
			(pr_global_struct->input_angles)[0] = SHORT2ANGLE(ucmd->angles[0]);
			(pr_global_struct->input_angles)[1] = SHORT2ANGLE(ucmd->angles[1]);
			(pr_global_struct->input_angles)[2] = SHORT2ANGLE(ucmd->angles[2]);
		}

		(pr_global_struct->input_movevalues)[0] = ucmd->forwardmove;
		(pr_global_struct->input_movevalues)[1] = ucmd->sidemove;
		(pr_global_struct->input_movevalues)[2] = ucmd->upmove;
		pr_global_struct->input_buttons = ucmd->buttons;

		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, client->edict);
		PR_ExecuteProgram(svprogfuncs, gfuncs.RunClientCommand);


		if (!sv_player->v->fixangle)
		{
			int i;
			vec3_t delta;
			VectorSubtract (sv_player->v->v_angle, startangle, delta);

			if (delta[0] || delta[1] || delta[2])
			{
				//eular angle changes suck
				if (client->fteprotocolextensions2 & PEXT2_SETANGLEDELTA)
				{
					client_t *cl = ClientReliableWrite_BeginSplit(client, svcfte_setangledelta, 7);
					for (i=0 ; i < 3 ; i++)
						ClientReliableWrite_Angle16 (cl, delta[i]);
				}
				else
				{
					client_t *cl = ClientReliableWrite_BeginSplit(client, svc_setangle, 7);
					for (i=0 ; i < 3 ; i++)
						ClientReliableWrite_Angle (cl, sv_player->v->v_angle[i]);
				}
			}

		}
 		return true;
	}
	return false;
}

//entity(string match [, float matchnum]) matchclient = #241;
static void QCBUILTIN PF_matchclient(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int clnum=-1;
	const char *name = PR_GetStringOfs(prinst, OFS_PARM0);
	int matchnum = G_FLOAT(OFS_PARM1);
	client_t *cl;

	if (prinst->callargc < 2)
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

static void QCBUILTIN PF_SendPacket(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	netadr_t to;
	const char *address = PR_GetStringOfs(prinst, OFS_PARM0);
	const char *contents = PF_VarString(prinst, 1, pr_globals);

	if (NET_StringToAdr(address, 0, &to))
		NET_SendPacket(NS_SERVER, strlen(contents), contents, &to);
}

//be careful to not touch the resource unless we're meant to, to avoid stalling
static void QCBUILTIN PF_resourcestatus(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int restype = G_FLOAT(OFS_PARM0);
	int doload = G_FLOAT(OFS_PARM1);
	const char *resname = PR_GetStringOfs(prinst, OFS_PARM2);
	int idx;
	G_FLOAT(OFS_RETURN) = RESSTATE_NOTKNOWN;
	switch(restype)
	{
	case RESTYPE_MODEL:
		for (idx=1 ; idx<MAX_PRECACHE_MODELS && sv.strings.model_precache[idx] ; idx++)
		{
			if (!strcmp(sv.strings.model_precache[idx], resname))
			{
				model_t *mod = sv.models[idx];
				if (!mod && doload)
					mod = sv.models[idx] = Mod_FindName(Mod_FixName(resname, sv.modelname));
				if (!mod)
					G_FLOAT(OFS_RETURN) = RESSTATE_NOTLOADED;
				else
				{
					if (doload && mod->loadstate == MLS_NOTLOADED)
						Mod_LoadModel (mod, MLV_SILENT);	//should avoid blocking.
					switch(mod->loadstate)
					{
					default:
					case MLS_NOTLOADED:
						G_FLOAT(OFS_RETURN) = RESSTATE_NOTLOADED;
						break;
					case MLS_LOADING:
						G_FLOAT(OFS_RETURN) = RESSTATE_LOADING;
						break;
					case MLS_LOADED:
						G_FLOAT(OFS_RETURN) = RESSTATE_LOADED;
						break;
					case MLS_FAILED:
						G_FLOAT(OFS_RETURN) = RESSTATE_FAILED;
						break;
					}
				}
				return;
			}
		}
		break;
	case RESTYPE_SOUND:
		for (idx=1 ; idx<MAX_PRECACHE_SOUNDS && sv.strings.sound_precache[idx] ; idx++)
		{
			if (!strcmp(sv.strings.sound_precache[idx], resname))
			{
				G_FLOAT(OFS_RETURN) = RESSTATE_NOTLOADED;
				return;
			}
		}
		break;
	case RESTYPE_PARTICLE:
	case RESTYPE_SHADER:
	case RESTYPE_SKIN:
	case RESTYPE_TEXTURE:
		//FIXME
	default:
		G_FLOAT(OFS_RETURN) = -1;
		break;
	}
}

static void QCBUILTIN PF_clusterevent(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
#ifdef SUBSERVERS
	const char *dest = PR_GetStringOfs(prinst, OFS_PARM0);
	const char *src = PR_GetStringOfs(prinst, OFS_PARM1);
	const char *cmd = PR_GetStringOfs(prinst, OFS_PARM2);
	const char *info = PF_VarString(prinst, 3, pr_globals);
	SSV_Send(dest, src, cmd, info);
#endif
}
static void QCBUILTIN PF_clustertransfer(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
#ifdef SUBSERVERS
	int p = G_EDICT(prinst, OFS_PARM0)->entnum - 1;
	const char *dest = (prinst->callargc >= 2)?PR_GetStringOfs(prinst, OFS_PARM1):NULL;

	G_INT(OFS_RETURN) = 0;

	if (p < 0 || p >= sv.allocated_client_slots)
	{
		PR_BIError (prinst, "PF_clustertransfer: not a player\n");
		return;
	}

	if (dest)
	{
		if (!SSV_IsSubServer())
		{
			Con_DPrintf("PF_clustertransfer: not running in mapcluster mode, ignoring transfer to %s\n", dest);
			return;
		}
		if (svs.clients[p].transfer)
		{
			Con_DPrintf("PF_clustertransfer: Already transferring to %s, ignoring transfer to %s\n", svs.clients[p].transfer, dest);
			return;
		}
		svs.clients[p].transfer = Z_StrDup(svs.clients[p].transfer);
		SSV_InitiatePlayerTransfer(&svs.clients[p], svs.clients[p].transfer);
	}

	if (svs.clients[p].transfer)
		RETURN_TSTRING(svs.clients[p].transfer);
#else
	G_INT(OFS_RETURN) = 0;
#endif
}

static void QCBUILTIN PF_setpause(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int pause = (G_FLOAT(OFS_PARM0)?PAUSE_EXPLICIT:0) | (sv.paused&~PAUSE_EXPLICIT);
	G_FLOAT(OFS_RETURN) = !!(sv.paused&PAUSE_EXPLICIT);
	if (sv.paused != pause)
	{
		sv.paused = pause;
		sv.pausedstart = Sys_DoubleTime();
	}
}


#define STUB ,NULL,true
#if defined(DEBUG) || defined(_DEBUG)
#define NYI
#else
#define NYI ,true
#endif
#ifdef NOQCDESCRIPTIONS
	#define D(p,d) NULL,NULL
#else
	#define D(p,d) p,d
#endif
BuiltinList_t BuiltinList[] = {				//nq	qw		h2		ebfs
	{"fixme",			PF_Fixme,			0,		0,		0,		0,	D("void()", "Some builtin that should never be called. Ends the game with some weird message.")},

#ifndef SERVERONLY
	//begin menu-only 'standard'
	{"checkextension",	PF_Fixme,			0,		0,		0,		1,	D("float(string ext)", "Checks if the named extension is supported by the running engine.")},
	{"error",			PF_Fixme,			0,		0,		0,		2,	"void(string err,...)"},
	{"objerror",		PF_Fixme,			0,		0,		0,		3,	"void(string err,...)"},
	{"print",			PF_Fixme,			0,		0,		0,		4,	"void(string text,...)"},
	{"bprint",			PF_Fixme,			0,		0,		0,		5,	"void(string text,...)"},
	{"msprint",			PF_Fixme,			0,		0,		0,		6,	"void(float clientnum, string text,...)"},
	{"cprint",			PF_Fixme,			0,		0,		0,		7,	"void(string text,...)"},
	{"normalize",		PF_Fixme,			0,		0,		0,		8,	"vector(vector)"},
	{"vlen",			PF_Fixme,			0,		0,		0,		9,	"float(vector)"},
	{"vectoyaw",		PF_Fixme,			0,		0,		0,		10,	"float(vector)"},
	{"vectoangles",		PF_Fixme,			0,		0,		0,		11,	"vector(vector)"},
	{"random",			PF_Fixme,			0,		0,		0,		12,	"float()"},
	{"localcmd",		PF_Fixme,			0,		0,		0,		13,	"void(string,...)"},
	{"cvar",			PF_Fixme,			0,		0,		0,		14,	"float(string name)"},
	{"cvar_set",		PF_Fixme,			0,		0,		0,		15,	"void(string name, string value)"},
	{"dprint",			PF_Fixme,			0,		0,		0,		16,	"void(string text)"},
	{"ftos",			PF_Fixme,			0,		0,		0,		17,	"string(float)"},
	{"fabs",			PF_Fixme,			0,		0,		0,		18,	"float(float)"},
	{"vtos",			PF_Fixme,			0,		0,		0,		19,	"string(vector)"},
	{"etos",			PF_Fixme,			0,		0,		0,		20,	"string(entity)"},
	{"stof",			PF_Fixme,			0,		0,		0,		21,	"float(string)"},
	{"spawn",			PF_Fixme,			0,		0,		0,		22,	"entity()"},
	{"remove",			PF_Fixme,			0,		0,		0,		23,	"void(entity)"},
	{"find",			PF_Fixme,			0,		0,		0,		24,	"entity(entity start, .string field, string match)"},
	{"findfloat",		PF_Fixme,			0,		0,		0,		25,	"entity(entity start, .__variant field, __variant match)"},
	{"findchain",		PF_Fixme,			0,		0,		0,		26,	"entity(.string field, string match)"},
	{"findchainfloat",	PF_Fixme,			0,		0,		0,		27,	"entity(.__variant field, __variant match)"},
	{"precache_file",	PF_Fixme,			0,		0,		0,		28,	"string(string file)"},
	{"precache_sound",	PF_Fixme,			0,		0,		0,		29,	"string(string sample)"},
	{"coredump",		PF_Fixme,			0,		0,		0,		30,	"void()"},
	{"traceon",			PF_Fixme,			0,		0,		0,		31,	"void()"},
	{"traceoff",		PF_Fixme,			0,		0,		0,		32,	"void()"},
	{"eprint",			PF_Fixme,			0,		0,		0,		33,	"void(entity)"},
	{"rint",			PF_Fixme,			0,		0,		0,		34,	"float(float)"},
	{"floor",			PF_Fixme,			0,		0,		0,		35,	"float(float)"},
	{"ceil",			PF_Fixme,			0,		0,		0,		36,	"float(float)"},
	{"nextent",			PF_Fixme,			0,		0,		0,		37,	"entity(entity)"},
	{"sin",				PF_Fixme,			0,		0,		0,		38,	"float(float)"},
	{"cos",				PF_Fixme,			0,		0,		0,		39,	"float(float)"},
	{"sqrt",			PF_Fixme,			0,		0,		0,		40,	"float(float)"},
	{"randomvector",	PF_Fixme,			0,		0,		0,		41,	"vector()"},
	{"registercvar",	PF_Fixme,			0,		0,		0,		42,	"float(string name, string value, float flags)"},
	{"min",				PF_Fixme,			0,		0,		0,		43,	"float(float,...)"},
	{"max",				PF_Fixme,			0,		0,		0,		44,	"float(float,...)"},
	{"bound",			PF_Fixme,			0,		0,		0,		45,	"float(float min,float value,float max)"},
	{"pow",				PF_Fixme,			0,		0,		0,		46,	"float(float,float)"},
	{"copyentity",		PF_Fixme,			0,		0,		0,		47,	"void(entity src, entity dst)"},
	{"fopen",			PF_Fixme,			0,		0,		0,		48,	"filestream(string filename, float mode)"},
	{"fclose",			PF_Fixme,			0,		0,		0,		49,	"void(filestream fhandle)"},
	{"fgets",			PF_Fixme,			0,		0,		0,		50,	"string(filestream fhandle)"},
	{"fputs",			PF_Fixme,			0,		0,		0,		51,	"void(filestream fhandle, string s)"},
	{"strlen",			PF_Fixme,			0,		0,		0,		52,	"float(string)"},
	{"strcat",			PF_Fixme,			0,		0,		0,		53,	"string(string, optional string, optional string, optional string, optional string, optional string, optional string, optional string)"},
	{"substring",		PF_Fixme,			0,		0,		0,		54,	"string(string s, float start, float length)"},
	{"stov",			PF_Fixme,			0,		0,		0,		55,	"vector(string)"},
	{"strzone",			PF_Fixme,			0,		0,		0,		56,	"string(string)"},
	{"strunzone",		PF_Fixme,			0,		0,		0,		57,	"void(string)"},
	{"tokenize",		PF_Fixme,			0,		0,		0,		58,	"float(string)"},
	{"argv",			PF_Fixme,			0,		0,		0,		59,	"string(float)"},
	{"isserver",		PF_Fixme,			0,		0,		0,		60,	"float()"},
	{"clientcount",		PF_Fixme,			0,		0,		0,		61,	"float()"},
	{"clientstate",		PF_Fixme,			0,		0,		0,		62,	"float()"},
	{"clientcommand",	PF_Fixme,			0,		0,		0,		63,	"void(float client, string s)"},
	{"changelevel",		PF_Fixme,			0,		0,		0,		64,	"void(string map)"},
	{"localsound",		PF_Fixme,			0,		0,		0,		65,	"void(string sample, optional float channel, optional float volume)"},
	{"getmousepos",		PF_Fixme,			0,		0,		0,		66,	"vector()"},
	{"gettime",			PF_Fixme,			0,		0,		0,		67,	"float(optional float timetype)"},
	{"loadfromdata",	PF_Fixme,			0,		0,		0,		68,	"void(string data)"},
	{"loadfromfile",	PF_Fixme,			0,		0,		0,		69,	"void(string data)"},
	{"mod",				PF_Fixme,			0,		0,		0,		70,	"float(float val, float m)"},
	{"cvar_string",		PF_Fixme,			0,		0,		0,		71,	"string(string name)"},
	{"crash",			PF_Fixme,			0,		0,		0,		72,	"void()"},
	{"stackdump",		PF_Fixme,			0,		0,		0,		73,	"void()"},
	{"search_begin",	PF_Fixme,			0,		0,		0,		74,	"searchhandle(string pattern, float caseinsensitive, float quiet)"},
	{"search_end",		PF_Fixme,			0,		0,		0,		75,	"void(searchhandle handle)"},
	{"search_getsize",	PF_Fixme,			0,		0,		0,		76,	"float(searchhandle handle)"},
	{"search_getfilename",PF_Fixme,			0,		0,		0,		77,	"string(searchhandle handle, float num)"},
	{"etof",			PF_Fixme,			0,		0,		0,		79,	"float(entity)"},
	{"ftoe",			PF_Fixme,			0,		0,		0,		80,	"entity(float)"},
	{"validstring",		PF_Fixme,			0,		0,		0,		81,	"float(string)"},
	{"altstr_count",	PF_Fixme,			0,		0,		0, 		82,	"float(string str)"},
	{"altstr_prepare",	PF_Fixme,			0,		0,		0, 		83,	"string(string str)"},
	{"altstr_get",		PF_Fixme,			0,		0,		0,		84,	"string(string str, float num)"},
	{"altstr_set",		PF_Fixme,			0,		0,		0, 		85,	"string(string str, float num, string set) "},
	{"altstr_ins",		PF_Fixme,			0,		0,		0,		86,	"string(string str, float num, string set)"},
	{"findflags",		PF_Fixme,			0,		0,		0,		87,	"entity(entity start, .float field, float match)"},
	{"findchainflags",	PF_Fixme,			0,		0,		0,		88,	"entity(.float field, float match)"},
	{"mcvar_defstring",	PF_Fixme,			0,		0,		0,		89,	"string(string name)" STUB},

	{"setmodel",		PF_Fixme,			0,		0,		0,		90, D("void(entity ent, string mname)",	"Menuqc-specific version.")},
	{"precache_model",	PF_Fixme,			0,		0,		0,		91, D("void(string mname)",				"Menuqc-specific version.")},
	{"setorigin",		PF_Fixme,			0,		0,		0,		92, D("void(entity ent, vector neworg)","Menuqc-specific version.")},
	//end menu-only 'standard'
#endif
										  //nq		qw		h2		ebfs
	{"ignore",			PF_Ignore,			0,		0,		0,		0,	D("void()","Ignored by the engine. Returns 0.")},
	{"makevectors",		PF_makevectors,		1,		1,		1,		0,	D("void(vector vang)","Takes an angle vector (pitch,yaw,roll) (+x=DOWN). Writes its results into v_forward, v_right, v_up vectors.")},
	{"setorigin",		PF_setorigin,		2,		2,		2,		0,	D("void(entity e, vector o)","Changes e's origin to be equal to o. Also relinks collision state (as well as setting absmin+absmax), which is required after changing .solid")},
	{"setmodel",		PF_setmodel,		3,		3,		3,		0,	D("void(entity e, string m)","Looks up m in the model precache list, and sets both e.model and e.modelindex to match. BSP models will set e.mins and e.maxs accordingly, other models depend upon the value of sv_gameplayfix_setmodelrealbox - for compatibility you should always call setsize after all pickups or non-bsp models. Also relinks collision state.")},
	{"setsize",			PF_setsize,			4,		4,		4,		0,	D("void(entity e, vector min, vector max)", "Sets the e's mins and maxs fields. Also relinks collision state, which sets absmin and absmax too.")},
	{"qtest_setabssize",PF_setsize,			5,		0,		0,		0,	D("void(entity e, vector min, vector max)", "qtest"), true},
	{"breakpoint",		PF_break,			6,		6,		6,		0,	D("void()", "Trigger a debugging event. FTE will break into the qc debugger. Other engines may crash with a debug execption.")},
	{"random",			PF_random,			7,		7,		7,		0,	D("float()", "Returns a random value between 0 and 1. Be warned, this builtin can return 1 in most engines, which can break arrays.")},
	{"sound",			PF_sound,			8,		8,		8,		0,	D("void(entity e, float chan, string samp, float vol, float atten, optional float speedpct, optional float flags, optional float timeofs)", "Starts a sound centered upon the given entity.\nchan is the entity sound channel to use, channel 0 will allow you to mix many samples at once, others will replace the old sample\n'samp' must have been precached first\nif specified, 'speedpct' should normally be around 100 (or =0), 200 for double speed or 50 for half speed.\nIf flags is specified, the reliable flag in the channels argument is used for additional channels. Flags should be made from SOUNDFLAG_* constants\ntimeofs should be negative in order to provide a delay before the sound actually starts.")},
	{"normalize",		PF_normalize,		9,		9,		9,		0,	D("vector(vector v)", "Shorten or lengthen a direction vector such that it is only one quake unit long.")},
	{"error",			PF_error,			10,		10,		10,		0,	D("void(string e)", "Ends the game with an easily readable error message.")},
	{"objerror",		PF_objerror,		11,		11,		11,		0,	D("void(string e)", "Displays a non-fatal easily readable error message concerning the self entity, including a field dump. self will be removed!")},
	{"vlen",			PF_vlen,			12,		12,		12,		0,	D("float(vector v)", "Returns the square root of the dotproduct of a vector with itself. Or in other words the length of a distance vector, in quake units.")},
	{"vectoyaw",		PF_vectoyaw,		13,		13,		13,		0,	D("float(vector v, optional entity reference)", "Given a direction vector, returns the yaw angle in which that direction vector points. If an entity is passed, the yaw angle will be relative to that entity's gravity direction.")},
	{"spawn",			PF_Spawn,			14,		14,		14,		0,	D("entity()", "Adds a brand new entity into the world! Hurrah, you're now a parent!")},
	{"remove",			PF_Remove,			15,		15,		15,		0,	D("void(entity e)", "Destroys the given entity and clears some limited fields (including model, modelindex, solid, classname). Any references to the entity following the call are an error. After two seconds, the entity will be reused, in the interim you can unfortunatly still read its fields to see if the reference is no longer valid.")},
	{"traceline",		PF_svtraceline,		16,		16,		16,		0,	D("void(vector v1, vector v2, float flags, entity ent)", "Traces a thin line through the world from v1 towards v2.\nWill not collide with ent, ent.owner, or any entity who's owner field refers to ent.\nThe passed entity will also be used to determine whether to use a capsule trace, the contents that the trace should impact, and a couple of other extra fields that define the trace.\nThere are no side effects beyond the trace_* globals being written.\nflags&MOVE_NOMONSTERS will not impact on non-bsp entities.\nflags&MOVE_MISSILE will impact with increased size.\nflags&MOVE_HITMODEL will impact upon model meshes, instead of their bounding boxes.\nflags&MOVE_TRIGGERS will also stop on triggers\nflags&MOVE_EVERYTHING will stop if it hits anything, even non-solid entities.\nflags&MOVE_LAGGED will backdate entity positions for the purposes of this builtin according to the indicated player ent's latency, to provide lag compensation.")},
	{"checkclient",		PF_checkclient,		17,		17,		17,		0,	D("entity()", "Returns one of the player entities. The returned player will change periodically.")},
	{"find",			PF_FindString,		18,		18,		18,		0,	D("entity(entity start, .string fld, string match)", "Scan for the next entity with a given field set to the given 'match' value. start should be either world, or the previous entity that was found. Returns world on failure/if there are no more.")},
	{"precache_sound",	PF_precache_sound,	19,		19,		19,		0,	D("string(string s)", "Precaches a sound, making it known to clients and loading it from disk. This builtin (strongly) should be called during spawn functions. This builtin must be called for the sound before the sound builtin is called, or it might not even be heard.")},
	{"precache_model",	PF_precache_model,	20,		20,		20,		0,	D("string(string s)", "Precaches a model, making it known to clients and loading it from disk if it has a .bsp extension. This builtin (strongly) should be called during spawn functions. This must be called for each model name before setmodel may use that model name.\nModelindicies precached in SSQC will always be positive. CSQC precaches will be negative if they are not also on the server.")},
	{"stuffcmd",		PF_stuffcmd,		21,		21,		21,		0,	D("void(entity client, string s)", "Sends a console command (or cvar) to the client, where it will be executed. Different clients support different commands. Do NOT forget the final \\n.\nThis builtin is generally considered evil.")},
	{"stuffcmdflags",	PF_stuffcmdflags,	0,		0,		0,		0,	D("void(entity client, float flags, string s)", "Sends a console command (or cvar) to the client, where it will be executed. Different clients support different commands. Do NOT forget the final \\n.\nThis (just as evil) variant allows specifying some flags too. See the STUFFCMD_* constants.")},
	{"findradius",		PF_findradius,		22,		22,		22,		0,	D("entity(vector org, float rad)", "Finds all entities within a distance of the 'org' specified. One entity is returned directly, while other entities are returned via that entity's .chain field.")},
	//both bprint and sprint accept different arguments in QW vs NQ/H2
	{"bprint",			PF_bprint,			23,		0,		23,		0,	D("void(string s, optional string s2, optional string s3, optional string s4, optional string s5, optional string s6, optional string s7, optional string s8)", "NQ: Concatenates all arguments, and prints the messsage on the console of all connected clients.")},
	{"bprint",			PF_bprint,			0,		23,		0,		0,	D("void(float msglvl, string s, optional string s2, optional string s3, optional string s4, optional string s5, optional string s6, optional string s7)", "QW: Concatenates all string arguments, and prints the messsage on the console of only all clients who's 'msg' infokey is set lower or equal to the supplied 'msglvl' argument.")},
	{"sprint",			PF_sprint,			24,		0,		24,		0,	D("void(entity client, string s, optional string s2, optional string s3, optional string s4, optional string s5, optional string s6, optional string s7)", "NQ: Concatenates all string arguments, and prints the messsage on the named client's console")},
	{"sprint",			PF_sprint,			0,		24,		0,		0,	D("void(entity client, float msglvl, string s, optional string s2, optional string s3, optional string s4, optional string s5, optional string s6)", "QW: Concatenates all string arguments, and prints the messsage on the named client's console, but only if that client's 'msg' infokey is set lower or equal to the supplied 'msglvl' argument.")},
	//these have subtly different behaviour, and are implemented using different internal builtins, which is a bit weird in the extensions file. documentation is documentation.
	{"dprint",			PF_dprint,			25,		0,		25,		0,	D("void(string s, ...)", "NQ: Prints the given message on the server's console, but only if the developer cvar is set. Arguments will be concatenated into a single message.")},
	{"dprint",			PF_print,			0,		25,		0,		0,	D("void(string s, ...)", "QW: Unconditionally prints the given message on the server's console.  Arguments will be concatenated into a single message.")},
	{"ftos",			PF_ftos,			26,		26,		26,		0,	D("string(float val)", "Returns a tempstring containing a representation of the given float. Precision depends upon engine.")},
	{"vtos",			PF_vtos,			27,		27,		27,		0,	D("string(vector val)", "Returns a tempstring containing a representation of the given vector. Precision depends upon engine.")},
	{"coredump",		PF_coredump,		28,		28,		28,		0,	D("void()", "Writes out a coredump. This contains stack, globals, and field info for all ents. This can be handy for debugging.")},
	{"traceon",			PF_traceon,			29,		29,		29,		0,	D("void()", "Enables tracing. This may be spammy, slow, and stuff. Set debugger 1 in order to use fte's qc debugger.")},
	{"traceoff",		PF_traceoff,		30,		30,		30,		0,	D("void()", "Disables tracing again.")},
	{"eprint",			PF_eprint,			31,		31,		31,		0,	D("void(entity e)", "Debugging builtin that prints all fields of the given entity to the console.")},// debug print an entire entity
	{"walkmove",		PF_walkmove,		32,		32,		32,		0,	D("float(float yaw, float dist, optional float settraceglobals)", "Attempt to walk the entity at a given angle for a given distance.\nif settraceglobals is set, the trace_* globals will be set, showing the results of the movement.\nThis function will trigger touch events.")},
//	{"qtest_flymove",	NULL,	33},	// float(vector dir) flymove = #33;
//qbism super8's 'private'sound #33
	{"droptofloor",		PF_droptofloor,		34,		34,		34,		0,	D("float()", "Instantly moves the entity downwards until it hits the ground. If the entity is in solid or would need to drop more than 'pr_droptofloorunits' quake units, its position will be considered invalid and the builtin will abort, returning FALSE, otherwise TRUE.")},
	{"lightstyle",		PF_lightstyle,		35,		35,		35,		0,	D("void(float lightstyle, string stylestring, optional vector rgb)", "Specifies an auto-animating string that specifies the light intensity for entities using that lightstyle.\na is off, z is fully lit. Should be lower case only.\nrgb will recolour all lights using that lightstyle.\n")},
	{"rint",			PF_rint,			36,		36,		36,		0,	D("float(float)", "Rounds the given float up or down to the closest integeral value. X.5 rounds away from 0")},
	{"floor",			PF_floor,			37,		37,		37,		0,	D("float(float)", "Rounds the given float downwards, even when negative.")},
	{"ceil",			PF_ceil,			38,		38,		38,		0,	D("float(float)", "Rounds the given float upwards, even when negative.")},
	{"qtest_canreach",	PF_Ignore,			39,		0,		0,		0,	"float(vector v)"}, // QTest builtin called in effectless statement
	{"checkbottom",		PF_checkbottom,		40,		40,		40,		0,	D("float(entity ent)", "Expensive checks to ensure that the entity is actually sitting on something solid, returns true if it is.")},
	{"pointcontents",	PF_pointcontents,	41,		41,		41,		0,	D("float(vector pos)", "Checks the given point to see what is there. Returns one of the SOLID_* constants. Just because a spot is empty does not mean that the player can stand there due to the size of the player - use tracebox for such tests.")},
//	{"qtest_stopsound",	NULL,				42}, // defined QTest builtin that is never called
	{"fabs",			PF_fabs,			43,		43,		43,		0,	D("float(float)", "Removes the sign of the float, making it positive if it is negative.")},
	{"aim",				PF_aim,				44,		44,		44,		0,	D("vector(entity player, float missilespeed)", "Returns a direction vector (specifically v_forward on error). This builtin attempts to guess what pitch angle to fire projectiles at for people that don't know about mouselook. Does not affect yaw angles.")},	//44
	{"cvar",			PF_cvar,			45,		45,		45,		0,	D("float(string)", "Returns the numeric value of the named cvar")},
	{"localcmd",		PF_localcmd,		46,		46,		46,		0,	D("void(string, ...)", "Adds the string to the console command queue. Commands will not be executed immediately, but rather at the start of the following frame.")},
	{"nextent",			PF_nextent,			47,		47,		47,		0,	D("entity(entity)", "Returns the following entity. Skips over removed entities. Returns world when passed the last valid entity.")},
	{"particle",		PF_particle,		48,		0,		48,		48, D("void(vector pos, vector dir, float colour, float count)", "Spawn 'count' particles around 'pos' moving in the direction 'dir', with a palette colour index between 'colour' and 'colour+8'.")}, //48 nq readded. This isn't present in QW protocol (fte added it back).
	{"changeyaw",		PF_changeyaw,		49,		49,		49,		0,	D("#define ChangeYaw changeyaw\nvoid()", "Changes the self.angles_y field towards self.ideal_yaw by up to self.yaw_speed.")},
//	{"qtest_precacheitem", NULL,			50}, // defined QTest builtin that is never called
	{"vectoangles",		PF_vectoangles,		51,		51,		51,		0,	D("vector(vector fwd, optional vector up)", "Returns the angles (+x=UP) required to orient an entity to look in the given direction. The 'up' argument is required if you wish to set a roll angle, otherwise it will be limited to just monster-style turning.")},

	{"WriteByte",		PF_WriteByte,		52,		52,		52,		0,	D("void(float to, float val)", "Writes a single byte into a network message buffer. Typically you will find a more correct alternative to writing arbitary data. 'to' should be one of the MSG_* constants. MSG_ONE must have msg_entity set first.")},	//52
	{"WriteChar",		PF_WriteChar,		53,		53,		53,		0,	"void(float to, float val)"},	//53
	{"WriteShort",		PF_WriteShort,		54,		54,		54,		0,	"void(float to, float val)"},	//54
	{"WriteLong",		PF_WriteLong,		55,		55,		55,		0,	"void(float to, float val)"},	//55
	{"WriteCoord",		PF_WriteCoord,		56,		56,		56,		0,	"void(float to, float val)"},	//56
	{"WriteAngle",		PF_WriteAngle,		57,		57,		57,		0,	"void(float to, float val)"},	//57
	{"WriteString",		PF_WriteString,		58,		58,		58,		0,	"void(float to, string val)"},	//58
	{"WriteEntity",		PF_WriteEntity,		59,		59,		59,		0,	"void(float to, entity val)"},	//59

#ifndef QUAKETC
	{"swritebyte",		PF_qtSingle_WriteByte,			0,		0,		0,		0,	D("void(float val)", "A legacy of qtest - like WriteByte, except writes explicitly to the MSG_ONE target."), true},	//52
	{"swritechar",		PF_qtSingle_WriteChar,			0,		0,		0,		0,	D("void(float val)", NULL), true},	//53
	{"swriteshort",		PF_qtSingle_WriteShort,			0,		0,		0,		0,	D("void(float val)", NULL), true},	//54
	{"swritelong",		PF_qtSingle_WriteLong,			0,		0,		0,		0,	D("void(float val)", NULL), true},	//55
	{"swritecoord",		PF_qtSingle_WriteCoord,			0,		0,		0,		0,	D("void(float val)", NULL), true},	//56
	{"swriteangle",		PF_qtSingle_WriteAngle,			0,		0,		0,		0,	D("void(float val)", NULL), true},	//57
	{"swritestring",	PF_qtSingle_WriteString,		0,		0,		0,		0,	D("void(string val)", NULL), true},	//58
	{"swriteentity",	PF_qtSingle_WriteEntity,		0,		0,		0,		0,	D("void(entity val)", NULL), true},

	{"bwritebyte",		PF_qtBroadcast_WriteByte,		0,		0,		0,		0,	D("void(float byte)", "A legacy of qtest - like WriteByte, except writes explicitly to the MSG_ALL target."), true},	//59
	{"bwritechar",		PF_qtBroadcast_WriteChar,		0,		0,		0,		0,	D("void(float val)", NULL), true},	//60
	{"bwriteshort",		PF_qtBroadcast_WriteShort,		0,		0,		0,		0,	D("void(float val)", NULL), true},	//61
	{"bwritelong",		PF_qtBroadcast_WriteLong,		0,		0,		0,		0,	D("void(float val)", NULL), true},	//62
	{"bwritecoord",		PF_qtBroadcast_WriteCoord,		0,		0,		0,		0,	D("void(float val)", NULL), true},	//63
	{"bwriteangle",		PF_qtBroadcast_WriteAngle,		0,		0,		0,		0,	D("void(float val)", NULL), true},	//64
	{"bwritestring",	PF_qtBroadcast_WriteString,		0,		0,		0,		0,	D("void(string val)", NULL), true},	//65
	{"bwriteentity",	PF_qtBroadcast_WriteEntity,		0,		0,		0,		0,	D("void(entity val)", NULL), true},	//66
#endif

	{"sin",				PF_Sin,				0,		0,		62,		60,	D("float(float angle)", "Forgive me father, for I have trigonometry homework.")},	//60
	{"cos",				PF_Cos,				0,		0,		61,		61,	"float(float angle)"},	//61
	{"sqrt",			PF_Sqrt,			0,		0,		84,		62,	"float(float value)"},	//62
	{"modulo",			PF_mod,				0,		0,		0,		0,	"float(float a, float n)"},	//62

	{"changepitch",		PF_changepitch,		0,		0,		0,		63,	"void(entity ent)"},
	{"tracetoss",		PF_TraceToss,		0,		0,		0,		64,	"void(entity ent, entity ignore)"},
	{"etos",			PF_etos,			0,		0,		0,		65,	"string(entity ent)"},

	{"movetogoal",		PF_sv_movetogoal,	67,		67,		67,		0,	D("void(float step)", "Runs lots and lots of fancy logic in order to try to step the entity the specified distance towards its goalentity.")},	//67
	{"precache_file",	PF_precache_file,	68,		68,		68,		0,	D("string(string s)", "This builtin does nothing. It was used only as a hint for pak generation.")},	//68
	{"makestatic",		PF_makestatic,		69,		69,		69,		0,	D("void(entity e)", "Sends a copy of the entity's renderable fields to all clients, and REMOVES the entity, preventing further changes. This means it will be unmutable and non-solid.")},	//69

	{"changelevel",		PF_changelevel,		70,		70,		70,		0,	D("void(string mapname, optional string newmapstartspot)", "Attempts to change the map to the named map. If 'newmapstartspot' is specified, the state of the current map will be preserved, and the argument will be passed to the next map in the 'startspot' global, and the next map will be loaded from archived state if it was previously visited. If not specified, all archived map states will be purged.")},	//70

	{"cvar_set",		PF_cvar_set,		72,		72,		72,		0,	D("void(string cvarname, string valuetoset)", "Instantly sets a cvar to the given string value.")},	//72
	{"centerprint",		PF_centerprint,		73,		73,		73,		0,	"void(entity ent, string text, optional string text2, optional string text3, optional string text4, optional string text5, optional string text6, optional string text7)"},	//73

	{"ambientsound",	PF_ambientsound,	74,		74,		74,		0,	"void (vector pos, string samp, float vol, float atten)"},	//74

	{"precache_model2",	PF_precache_model,	75,		75,		75,		0,	"string(string str)"},	//75
	{"precache_sound2",	PF_precache_sound,	76,		76,		76,		0,	"string(string str)"},	//76	// precache_sound2 is different only for qcc
	{"precache_file2",	PF_precache_file,	77,		77,		0,		0,	"string(string str)"},	//77

	{"setspawnparms",	PF_setspawnparms,	78,		78,		78,		0,	"void(entity player)"},	//78
	{"logfrag",			PF_logfrag,			0,		79,		0,		79,	"void(entity killer, entity killee)"},	//79

// Tomaz - QuakeC String Manipulation Begin
	{"tq_zone",			PF_strzone,			0,		0,		0,		79, D("string(string s)",NULL), true},	//79
	{"tq_unzone",		PF_strunzone,		0,		0,		0,		80, D("void(string s)",NULL), true},	//80
	{"tq_stof",			PF_stof,			0,		0,		0,		81, D("float(string s)",NULL), true},	//81
	{"tq_strcat",		PF_strcat,			0,		0,		0,		82, D("string(string s1, optional string s2, optional string s3, optional string s4, optional string s5, optional string s6, optional string s7, optional string s8)",NULL), true},	//82
	{"tq_substring",	PF_substring,		0,		0,		0,		83, D("string(string str, float start, float len)",NULL), true},	//83
	{"tq_stof",			PF_stof,			0,		0,		0,		84, D("float(string s)",NULL), true},	//84
	{"tq_stov",			PF_stov,			0,		0,		0,		85, D("vector(string s)",NULL), true},	//85
// Tomaz - QuakeC String Manipulation End

// Tomaz - QuakeC File System Begin (new mods use frik_file instead)
	{"tq_fopen",		PF_fopen,			0,		0,		0,		86, D("filestream(string filename, float mode)",NULL), true},// (QSG_FILE)
	{"tq_fclose",		PF_fclose,			0,		0,		0,		87, D("void(filestream fhandle)",NULL), true},// (QSG_FILE)
	{"tq_fgets",		PF_fgets,			0,		0,		0,		88, D("string(filestream fhandle)",NULL), true},// (QSG_FILE)
	{"tq_fputs",		PF_fputs,			0,		0,		0,		89, D("void(filestream fhandle, string s)",NULL), true},// (QSG_FILE)
// Tomaz - QuakeC File System End

	{"infokey",			PF_infokey,			0,		80,		0,		80, D("string(entity e, string key)", "If e is world, returns the field 'key' from either the serverinfo or the localinfo. If e is a player, returns the value of 'key' from the player's userinfo string. There are a few special exceptions, like 'ip' which is not technically part of the userinfo.")},	//80
	{"stof",			PF_stof,			0,		81,		0,		81,	"float(string)"},	//81
	{"multicast",		PF_multicast,		0,		82,		0,		82,	D("#define unicast(pl,reli) do{msg_entity = pl; multicast('0 0 0', reli?MULITCAST_ONE_R:MULTICAST_ONE);}while(0)\n"
																		"void(vector where, float set)", "Once the MSG_MULTICAST network message buffer has been filled with data, this builtin is used to dispatch it to the given target, filtering by pvs for reduced network bandwidth.")},	//82


#ifndef QUAKETC
//mvdsv (don't require ebfs usage in qw)
	{"executecommand",	PF_ExecuteCommand,	0,		0,		0,		83, D("void()",NULL),				true},
	{"mvdtokenize",		PF_Tokenize, 		0,		0,		0,		84, D("void(string str)",NULL),		true},
	{"mvdargc",			PF_ArgC,			0,		0,		0,		85, D("float()",NULL),				true},
	{"mvdargv",			PF_ArgV,			0,		0,		0,		86, D("string(float num)",NULL),	true},

//mvd commands
//some of these are a little iffy.
//we support them for mvdsv compatability but some of them look very hacky.
//these ones are not honoured with numbers, but can be used via the proper means.
	{"teamfield",		PF_teamfield,		0,		0,		0,		87, D("void(.string teamfield)",NULL), true},
	{"substr",			PF_substr,			0,		0,		0,		88, D("string(string str, float start, float len)",NULL), true},
	{"mvdstrcat",		PF_strcat,			0,		0,		0,		89, D("string(string s1, optional string s2, optional string s3, optional string s4, optional string s5, optional string s6, optional string s7, optional string s8)",NULL), true},
	{"mvdstrlen",		PF_strlen,			0,		0,		0,		90, D("float(string s)",NULL), true},
	{"str2byte",		PF_str2byte,		0,		0,		0,		91, D("float(string str)",NULL), true},
	{"str2short",		PF_str2short,		0,		0,		0,		92, D("float(string str)",NULL), true},
	{"mvdnewstr",		PF_mvdsv_newstring,	0,		0,		0,		93, D("string(string s, optional float bufsize)",NULL), true},
	{"mvdfreestr",		PF_mvdsv_freestring,0,		0,		0,		94, D("void(string s)",NULL), true},
	{"conprint",		PF_conprint,		0,		0,		0,		95, D("void(string s, ...)",NULL), true},
	{"readcmd",			PF_readcmd,			0,		0,		0,		96, D("string(string str)",NULL), true},
	{"mvdstrcpy",		PF_MVDSV_strcpy,	0,		0,		0,		97, D("void(string dst, string src)",NULL), true},
	{"strstr",			PF_strstr,			0,		0,		0,		98, D("string(string str, string sub)",NULL), true},
	{"mvdstrncpy",		PF_MVDSV_strncpy,	0,		0,		0,		99, D("void(string dst, string src, float count)",NULL), true},
	{"logtext",			PF_logtext,			0,		0,		0,		100, D("void(string name, float console, string text)",NULL), true},
//	{"redirectcmd",		PF_redirectcmd,		0,		0,		0,		101, D("void(entity to, string str)",NULL), true},
	{"mvdcalltimeofday",PF_calltimeofday,	0,		0,		0,		102, D("void()",NULL), true},
	{"forcedemoframe",	PF_forcedemoframe,	0,		0,		0,		103, D("void(float now)",NULL), true},
//end of mvdsv
#endif

	{"getlightstyle",	PF_getlightstyle,	0,		0,		0,		0,	D("string(float style, optional __out vector rgb)", "Obtains the light style string for the given style.")},
	{"getlightstylergb",PF_getlightstylergb,0,		0,		0,		0,	D("vector(float style)", "Obtains the current rgb value of the specified light style. In csqc, this is correct with regard to the current frame, while ssqc gives no guarentees about time and ignores client cvars. Note: use getlight if you want the actual light value at a point.")},
#ifdef HEXEN2
	{"lightstylestatic",PF_lightstylestatic,0,		0,		5,		5,	D("void(float style, float val, optional vector rgb)", "Sets the lightstyle to an explicit numerical level. From Hexen2.")},
	{"tracearea",		PF_traceboxh2,		0,		0,		33,		0,	D("void(vector v1, vector v2, vector mins, vector maxs, float nomonsters, entity ent)", "For hexen2 compat")},
	{"vhlen",			PF_vhlen,			0,		0,		50,		0,	D("float(vector)", "Returns the horizontal length of the given vector ignoring z dispalcement - specifically sqrt(x*x+y*y)")},
	{"printfloat",		PF_h2dprintf,		0,		0,		60,		0},	//60
	{"AdvanceFrame",	PF_h2AdvanceFrame,	0,		0,		63,		0},
	{"printvec",		PF_h2dprintv,		0,		0,		64,		0},	//64
	{"RewindFrame",		PF_h2RewindFrame,	0,		0,		65,		0},
	{"particleexplosion",PF_h2particleexplosion,0,	0,		81,		0},
	{"movestep",		PF_h2movestep,		0,		0,		82,		0},
	{"advanceweaponframe",PF_h2advanceweaponframe,0,0,		83,		0},

	{"setclass",		PF_h2setclass,		0,		0,		66,		0},
	{"lightstylevalue",	PF_lightstylevalue,	0,		0,		71,		0,	D("float(float lstyle)", "Returns the last value passed into the lightstylestatic builtin, or the first value specified by the style string passed to the lightstyle builtin")},	//70

	{"plaque_draw",		PF_h2plaque_draw,	0,		0,		79,		0,	"void(entity targ, float stringno)"},	//79
	{"rain_go",			PF_h2rain_go,		0,		0,		80,		0},	//80
	{"setpuzzlemodel",	PF_h2set_puzzle_model,0,	0,		87,		0},
	{"starteffect",		PF_h2starteffect,	0,		0,		88,		0},	//FIXME
	{"endeffect",		PF_h2endeffect,		0,		0,		89,		0},	//FIXME
	{"getstring",		PF_h2getstring,		0,		0,		92,		0},	//FIXME
	{"spawntemp",		PF_h2spawn_temp,	0,		0,		93,		0},

	{"v_factor",		PF_h2v_factor,		0,		0,		94,		0},
	{"v_factorrange",	PF_h2v_factorrange,	0,		0,		95,		0},

	{"precache_puzzle_model",PF_h2precache_puzzle_model,0,0,90,		0},
	{"concatv",			PF_h2concatv,		0,		0,		91,		0},
	{"precache_sound3",	PF_precache_sound,	0,		0,		96,		0},
	{"precache_model3",	PF_precache_model,	0,		0,		97,		0},//please don't use...
	{"matchangletoslope",PF_h2matchAngleToSlope,0,	0,		99,		0},
	{"updateinfoplaque",PF_h2updateinfoplaque,0,	0,		100,	0},
	{"precache_sound4",	PF_precache_sound,	0,		0,		101,	0},
	{"precache_model4",	PF_precache_model,	0,		0,		102,	0},
	{"precache_file4",	PF_precache_file,	0,		0,		103,	0},
	{"dowhiteflash",	PF_h2whiteflash,	0,		0,		104,	0},
	{"updatesoundpos",	PF_h2updatesoundpos,0,		0,		105,	0},
	{"stopsound",		PF_h2StopSound,		0,		0,		106,	0},

	{"precache_model4",	PF_precache_model,	0,		0,		116,	0},//please don't use...
	{"precache_sound4",	PF_precache_sound,	0,		0,		117,	0},
#endif

	{"tracebox",		PF_traceboxdp,		0,		0,		0,		90,	D("void(vector start, vector mins, vector maxs, vector end, float nomonsters, entity ent)", "Exactly like traceline, but a box instead of a uselessly thin point. Acceptable sizes are limited by bsp format, q1bsp has strict acceptable size values.")},

	{"randomvec",		PF_randomvector,	0,		0,		0,		91,	D("vector()", "Returns a vector with random values. Each axis is independantly a value between -1 and 1 inclusive.")},
	{"getlight",		PF_sv_getlight,		0,		0,		0,		92, "vector(vector org)"},// (DP_QC_GETLIGHT),
	{"registercvar",	PF_registercvar,	0,		0,		0,		93,	D("float(string cvarname, string defaultvalue)", "Creates a new cvar on the fly. If it does not already exist, it will be given the specified value. If it does exist, this is a no-op.\nThis builtin has the limitation that it does not apply to configs or commandlines. Such configs will need to use the set or seta command causing this builtin to be a noop.\nIn engines that support it, you will generally find the autocvar feature easier and more efficient to use.")},
	{"min",				PF_min,				0,		0,		0,		94,	D("float(float a, float b, ...)", "Returns the lowest value of its arguments.")},// (DP_QC_MINMAXBOUND)
	{"max",				PF_max,				0,		0,		0,		95,	D("float(float a, float b, ...)", "Returns the highest value of its arguments.")},// (DP_QC_MINMAXBOUND)
	{"bound",			PF_bound,			0,		0,		0,		96,	D("float(float minimum, float val, float maximum)", "Returns val, unless minimum is higher, or maximum is less.")},// (DP_QC_MINMAXBOUND)
	{"pow",				PF_pow,				0,		0,		0,		97,	"float(float value, float exp)"},
	{"logarithm",		PF_Logarithm,		0,		0,		0,		0,	D("float(float v, optional float base)", "Determines the logarithm of the input value according to the specified base. This can be used to calculate how much something was shifted by.")},
	{"tj_cvar_string",	PF_cvar_string,		0,		0,		0,		97, D("string(string cvarname)",NULL), true},	//telejano
//DP_QC_FINDFLOAT
	{"findfloat",		PF_FindFloat,		0,		0,		0,		98, D("#define findentity findfloat\nentity(entity start, .__variant fld, __variant match)", "Equivelent to the find builtin, but instead of comparing strings contents, this builtin compares the raw values. This builtin requires multiple calls in order to scan all entities - set start to the previous call's return value.\nworld is returned when there are no more entities.")},	// #98 (DP_QC_FINDFLOAT)

	{"checkextension",	PF_checkextension,	99,		99,		0,		99,	D("float(string extname)", "Checks for an extension by its name (eg: checkextension(\"FRIK_FILE\") says that its okay to go ahead and use strcat).\nUse cvar(\"pr_checkextension\") to see if this builtin exists.")},	// #99	//darkplaces system - query a string to see if the mod supports X Y and Z.
	{"checkbuiltin",	PF_checkbuiltin,	0,		0,		0,		0,	D("float(__variant funcref)", "Checks to see if the specified builtin is supported/mapped. This is intended as a way to check for #0 functions, allowing for simple single-builtin functions. Warning, if two different engines map different builtins to the same number, then this function will not tell you which will be called, only that it won't crash (the exception being #0, which are remapped as available).")},
	{"builtin_find",	PF_builtinsupported,100,	100,	0,		100,	D("float(string builtinname)", "Looks to see if the named builtin is valid, and returns the builtin number it exists at.")},	// #100	//per builtin system.
	{"anglemod",		PF_anglemod,		0,		0,		0,		102,	"float(float value)"},
	{"qsg_cvar_string",	PF_cvar_string,		0,		0,		0,		103,	D("string(string cvarname)","An old/legacy equivelent of more recent/common builtins in order to read a cvar's string value."), true},

//TEI_SHOWLMP2
	{"showpic",			PF_ShowPic,			0,		0,		0,		104,	"void(string slot, string picname, float x, float y, float zone, optional entity player)"},
	{"hidepic",			PF_HidePic,			0,		0,		0,		105,	"void(string slot, optional entity player)"},
	{"movepic",			PF_MovePic,			0,		0,		0,		106,	"void(string slot, float x, float y, float zone, optional entity player)"},
	{"changepic",		PF_ChangePic,		0,		0,		0,		107,	"void(string slot, string picname, optional entity player)"},
	{"showpicent",		PF_ShowPic,			0,		0,		0,		108,	D("void(string slot, entity player)",NULL), true},
	{"hidepicent",		PF_HidePic,			0,		0,		0,		109,	D("void(string slot, entity player)",NULL), true},
//	{"movepicent",		PF_MovePic,			0,		0,		0,		110,	"void(string slot, float x, float y, float zone, entity player)", true},
//	{"changepicent",	PF_ChangePic,		0,		0,		0,		111,	"void(string slot, string picname, entity player)", true},
//End TEU_SHOWLMP2

//frik file
	{"fopen",			PF_fopen,			0,		0,		0,		110, D("filestream(string filename, float mode, optional float mmapminsize)", "Opens a file, typically prefixed with \"data/\", for either read or write access.")},	// (FRIK_FILE)
	{"fclose",			PF_fclose,			0,		0,		0,		111, "void(filestream fhandle)"},	// (FRIK_FILE)
	{"fgets",			PF_fgets,			0,		0,		0,		112, D("string(filestream fhandle)", "Reads a single line out of the file. The new line character is not returned as part of the string. Returns the null string on EOF (use if not(string) to easily test for this, which distinguishes it from the empty string which is returned if the line being read is blank")},	// (FRIK_FILE)
	{"fputs",			PF_fputs,			0,		0,		0,		113, D("void(filestream fhandle, string s, optional string s2, optional string s3, optional string s4, optional string s5, optional string s6, optional string s7)", "Writes the given string(s) into the file. For compatibility with fgets, you should ensure that the string is terminated with a \\n - this will not otherwise be done for you. It is up to the engine whether dos or unix line endings are actually written.")},	// (FRIK_FILE)
	{"fread",			PF_fread,			0,		0,		0,		0,	 D("int(filestream fhandle, void *ptr, int size)", "Reads binary data out of the file. Returns truncated lengths if the read exceeds the length of the file.")},
	{"fwrite",			PF_fwrite,			0,		0,		0,		0,	 D("int(filestream fhandle, void *ptr, int size)", "Writes binary data out of the file.")},
	{"fseek",			PF_fseek,			0,		0,		0,		0,	 D("#define ftell fseek //c compat\nint(filestream fhandle, optional int newoffset)", "Changes the current position of the file, if specified. Returns prior position, in bytes.")},
	{"fsize",			PF_fsize,			0,		0,		0,		0,	 D("int(filestream fhandle, optional int newsize)", "Reports the total size of the file, in bytes. Can also be used to truncate/extend the file")},
	{"strlen",			PF_strlen,			0,		0,		0,		114, "float(string s)"},	// (FRIK_FILE)
	{"strcat",			PF_strcat,			0,		0,		0,		115, "string(string s1, optional string s2, optional string s3, optional string s4, optional string s5, optional string s6, optional string s7, optional string s8)"},	// (FRIK_FILE)
	{"substring",		PF_substring,		0,		0,		0,		116, "string(string s, float start, float length)"},	// (FRIK_FILE)
	{"stov",			PF_stov,			0,		0,		0,		117, "vector(string s)"},	// (FRIK_FILE)
#ifdef QCGC
	{"strzone",			PF_strzone,			0,		0,		0,		118, D("string(string s, ...)", "Create a semi-permanent copy of a string that only becomes invalid once strunzone is called on the string (instead of when the engine assumes your string has left scope). This builtin has become redundant in FTEQW due to the FTE_QC_PERSISTENTTEMPSTRINGS extension and is now functionally identical to strcat for compatibility with old engines+mods.")},	// (FRIK_FILE)
	{"strunzone",		PF_strunzone,		0,		0,		0,		119, D("void(string s)", "Destroys a string that was allocated by strunzone. Further references to the string MAY crash the game. In FTE, this function became redundant and now does nothing.")},	// (FRIK_FILE)
#else
	{"strzone",			PF_strzone,			0,		0,		0,		118, D("string(string s, ...)", "Create a semi-permanent copy of a string that only becomes invalid once strunzone is called on the string (instead of when the engine assumes your string has left scope).")},	// (FRIK_FILE)
	{"strunzone",		PF_strunzone,		0,		0,		0,		119, D("void(string s)", "Destroys a string that was allocated by strunzone. Further references to the string MAY crash the game.")},	// (FRIK_FILE)
#endif
//end frikfile

//these are telejano's
	{"cvar_setf",		PF_cvar_setf,		0,		0,		0,		176,	"void(string cvar, float val)"},
	{"localsound",		PF_ss_LocalSound,	0,		0,		0,		177,	D("void(string soundname, optional float channel, optional float volume)", "Plays a sound... locally... probably best not to call this from ssqc. Also disables reverb.")},//	#177
//end telejano

//fte extras

//	{"findlist_string",	PF_FindListString,	0,		0,		0,		0,		D("entity*(.string fld, string match)", "Return a list of entities with the given string field set to the given value.")},
//	{"findlist_float",	PF_FindListFloat,	0,		0,		0,		0,		D("entity*(.__variant fld, __variant match)", "Return a list of entities with the given field set to the given value.")},
//	{"findlist_radius",	PF_FindListRadius,	0,		0,		0,		0,		D("entity*(vector pos, float radius)", "Return a list of entities with the given string field set to the given value.")},
//	{"traceboxptr",		PF_TraceBox,		0,		0,		0,		0,		D("typedef struct {\nfloat allsolid;\nfloat startsolid;\nfloat fraction;\nfloat truefraction;\nentity ent;\nvector endpos;\nvector plane_normal;\nfloat plane_dist;\nint surfaceflags;\nint contents;\n} trace_t;\nvoid(trace_t *trace, vector start, vector mins, vector maxs, vector end, float nomonsters, entity forent)", "Like regular tracebox, except doesn't doesn't use any evil globals.")},

	{"getmodelindex",	PF_getmodelindex,	0,		0,		0,		200,	D("float(string modelname, optional float queryonly)", "Acts as an alternative to precache_model(foo);setmodel(bar, foo); return bar.modelindex;\nIf queryonly is set and the model was not previously precached, the builtin will return 0 without needlessly precaching the model.")},
	{"externcall",		PF_externcall,		0,		0,		0,		201,	D("__variant(float prnum, string funcname, ...)", "Directly call a function in a different/same progs by its name.\nprnum=0 is the 'default' or 'main' progs.\nprnum=-1 means current progs.\nprnum=-2 will scan through the active progs and will use the first it finds.")},
	{"addprogs",		PF_addprogs,		0,		0,		0,		202,	D("float(string progsname)", "Loads an additional .dat file into the current qcvm. The returned handle can be used with any of the externcall/externset/externvalue builtins.\nThere are cvars that allow progs to be loaded automatically.")},
	{"externvalue",		PF_externvalue,		0,		0,		0,		203,	D("__variant(float prnum, string varname)", "Reads a global in the named progs by the name of that global.\nprnum=0 is the 'default' or 'main' progs.\nprnum=-1 means current progs.\nprnum=-2 will scan through the active progs and will use the first it finds.")},
	{"externset",		PF_externset,		0,		0,		0,		204,	D("void(float prnum, __variant newval, string varname)", "Sets a global in the named progs by name.\nprnum=0 is the 'default' or 'main' progs.\nprnum=-1 means current progs.\nprnum=-2 will scan through the active progs and will use the first it finds.")},
	{"externrefcall",	PF_externrefcall,	0,		0,		0,		205,	D("__variant(float prnum, void() func, ...)","Calls a function between progs by its reference. No longer needed as direct function calls now switch progs context automatically, and have done for a long time. There is no remaining merit for this function."), true},
	{"instr",			PF_instr,			0,		0,		0,		206,	D("float(string input, string token)", "Returns substring(input, strstrpos(input, token), -1), or the null string if token was not found in input. You're probably better off using strstrpos."), true},
	{"openportal",		PF_OpenPortal,		0,		0,		0,		207,	D("void(entity portal, float state)", "Opens or closes the portals associated with a door or some such on q2 or q3 maps. On Q2BSPs, the entity should be the 'func_areaportal' entity - its style field will say which portal to open. On Q3BSPs, the entity is the door itself, the portal will be determined by the two areas found from a preceding setorigin call.")},

	{"RegisterTempEnt", PF_RegisterTEnt,	0,		0,		0,		208,	"float(float attributes, string effectname, ...)"},
	{"CustomTempEnt",	PF_CustomTEnt,		0,		0,		0,		209,	"void(float type, vector pos, ...)"},
	{"fork",			PF_Fork,			0,		0,		0,		210,	D("float(optional float sleeptime)", "When called, this builtin simply returns. Twice.\nThe current 'thread' will return instantly with a return value of 0. The new 'thread' will return after sleeptime seconds with a return value of 1. See documentation for the 'sleep' builtin for limitations/requirements concerning the new thread. Note that QC should probably call abort in the new thread, as otherwise the function will return to the calling qc function twice also.")},
	{"abort",			PF_Abort,			0,		0,		0,		211,	D("void(optional __variant ret)", "QC execution is aborted. Parent QC functions on the stack will be skipped, effectively this forces all QC functions to 'return ret' until execution returns to the engine. If ret is ommited, it is assumed to be 0.")},
	{"sleep",			PF_Sleep,			0,		0,		0,		212,	D("void(float sleeptime)", "Suspends the current QC execution thread for 'sleeptime' seconds.\nOther QC functions can and will be executed in the interim, including changing globals and field state (but not simultaneously).\nThe self and other globals will be restored when the thread wakes up (or set to world if they were removed since the thread started sleeping). Locals will be preserved, but will not be protected from remove calls.\nIf the engine is expecting the QC to return a value (even in the parent/root function), the value 0 shall be used instead of waiting for the qc to resume.")},
	{"forceinfokey",	PF_ForceInfoKey,	0,		0,		0,		213,	D("void(entity player, string key, string value)", "Directly changes a user's info without pinging off the client. Also allows explicitly setting * keys, including *spectator. Does not affect the user's config or other servers.")},
#ifdef SVCHAT
	{"chat",			PF_chat,			0,		0,		0,		214,	"void(string filename, float starttag, entity edict)"}, //(FTE_NPCCHAT)
#endif

#ifdef HEXEN2
	{"particle2",		PF_particle2,		0,		0,		42,		215,	"void(vector org, vector dmin, vector dmax, float colour, float effect, float count)"},
	{"particle3",		PF_particle3,		0,		0,		85,		216,	"void(vector org, vector box, float colour, float effect, float count)"},
	{"particle4",		PF_particle4,		0,		0,		86,		217,	"void(vector org, float radius, float colour, float effect, float count)"},
#endif

//EXT_DIMENSION_PLANES
	{"bitshift",		PF_bitshift,		0,		0,		0,		218,	"float(float number, float quantity)"},

//I guess this should go under DP_TE_STANDARDEFFECTBUILTINS...
	{"te_lightningblood",PF_te_lightningblood,	0,	0,		0,		219,	"void(vector pos)"},// #219 te_lightningblood

	{"map_builtin",		PF_builtinsupported,0,		0,		0,		220,	D("float(string builtinname, float builtinnum)","Attempts to map the named builtin at a non-standard builtin number. Returns 0 on failure."), true},	//like #100 - takes 2 args. arg0 is builtinname, 1 is number to map to.

//FTE_STRINGS
	{"strstrofs",		PF_strstrofs,		0,		0,		0,		221,	D("float(string s1, string sub, optional float startidx)", "Returns the 0-based offset of sub within the s1 string, or -1 if sub is not in s1.\nIf startidx is set, this builtin will ignore matches before that 0-based offset.")},
	{"str2chr",			PF_str2chr,			0,		0,		0,		222,	D("float(string str, float index)", "Retrieves the character value at offset 'index'.")},
	{"chr2str",			PF_chr2str,			0,		0,		0,		223,	D("string(float chr, ...)", "The input floats are considered character values, and are concatenated.")},
	{"strconv",			PF_strconv,			0,		0,		0,		224,	D("string(float ccase, float redalpha, float redchars, string str, ...)", "Converts quake chars in the input string amongst different representations.\nccase specifies the new case for letters.\n 0: not changed.\n 1: forced to lower case.\n 2: forced to upper case.\nredalpha and redchars switch between colour ranges.\n 0: no change.\n 1: Forced white.\n 2: Forced red.\n 3: Forced gold(low) (numbers only).\n 4: Forced gold (high) (numbers only).\n 5+6: Forced to white and red alternately.\nYou should not use this builtin in combination with UTF-8.")},
	{"strpad",			PF_strpad,			0,		0,		0,		225,	D("string(float pad, string str1, ...)", "Pads the string with spaces, to ensure its a specific length (so long as a fixed-width font is used, anyway). If pad is negative, the spaces are added on the left. If positive the padding is on the right.")},	//will be moved
	{"infoadd",			PF_infoadd,			0,		0,		0,		226,	D("string(infostring old, string key, string value)", "Returns a new tempstring infostring with the named value changed (or added if it was previously unspecified). Key and value may not contain the \\ character.")},
	{"infoget",			PF_infoget,			0,		0,		0,		227,	D("string(infostring info, string key)", "Reads a named value from an infostring. The returned value is a tempstring")},
//	{"strcmp",			PF_strncmp,			0,		0,		0,		228,	D("float(string s1, string s2)", "Compares the two strings exactly. s1ofs allows you to treat s2 as a substring to compare against, or should be 0.\nReturns 0 if the two strings are equal, a negative value if s1 appears numerically lower, and positive if s1 appears numerically higher.")},
	{"strncmp",			PF_strncmp,			0,		0,		0,		228,	D("#define strcmp strncmp\nfloat(string s1, string s2, optional float len, optional float s1ofs, optional float s2ofs)", "Compares up to 'len' chars in the two strings. s1ofs allows you to treat s2 as a substring to compare against, or should be 0.\nReturns 0 if the two strings are equal, a negative value if s1 appears numerically lower, and positive if s1 appears numerically higher.")},
	{"strcasecmp",		PF_strncasecmp,		0,		0,		0,		229,	D("float(string s1, string s2)",  "Compares the two strings without case sensitivity.\nReturns 0 if they are equal. The sign of the return value may be significant, but should not be depended upon.")},
	{"strncasecmp",		PF_strncasecmp,		0,		0,		0,		230,	D("float(string s1, string s2, float len, optional float s1ofs, optional float s2ofs)", "Compares up to 'len' chars in the two strings without case sensitivity. s1ofs allows you to treat s2 as a substring to compare against, or should be 0.\nReturns 0 if they are equal. The sign of the return value may be significant, but should not be depended upon.")},
//END FTE_STRINGS
	{"strtrim",			PF_strtrim,			0,		0,		0,		0,		D("string(string s)", "Trims the whitespace from the start+end of the string.")},

//FTE_CALLTIMEOFDAY
	{"calltimeofday",	PF_calltimeofday,	0,		0,		0,		231,	D("void()", "Asks the engine to instantly call the qc's 'timeofday' function, before returning. For compatibility with mvdsv.\ntimeofday should have the prototype: void(float secs, float mins, float hour, float day, float mon, float year, string strvalue)\nThe strftime builtin is more versatile and less weird.")},

//EXT_CSQC
	{"clientstat",		PF_clientstat,		0,		0,		0,		232,	D("void(float num, float type, .__variant fld)", "Specifies what data to use in order to send various stats, in a client-specific way.\n'num' should be a value between 32 and 127, other values are reserved.\n'type' must be set to one of the EV_* constants, one of EV_FLOAT, EV_STRING, EV_INTEGER, EV_ENTITY.\nfld must be a reference to the field used, each player will be sent only their own copy of these fields.")},	//EXT_CSQC
	{"globalstat",		PF_globalstat,		0,		0,		0,		233,	D("void(float num, float type, string name)", "Specifies what data to use in order to send various stats, in a non-client-specific way. num and type are as in clientstat, name however, is the name of the global to read in the form of a string (pass \"foo\").")},	//EXT_CSQC_1 actually
	{"pointerstat",		PF_pointerstat,		0,		0,		0,		0,		D("void(float num, float type, __variant *address)", "Specifies what data to use in order to send various stats, in a non-client-specific way. num and type are as in clientstat, address however, is the address of the variable you would like to use (pass &foo).")},
//END EXT_CSQC
	{"isbackbuffered",	PF_isbackbuffered,	0,		0,		0,		234,	D("float(entity player)", "Returns if the given player's network buffer will take multiple network frames in order to clear. If this builtin returns non-zero, you should delay or reduce the amount of reliable (and also unreliable) data that you are sending to that client.")},
	{"rotatevectorsbyangle",PF_rotatevectorsbyangles,0,0,	0,		235,	"void(vector angle)"}, // #235
	{"rotatevectorsbyvectors",PF_rotatevectorsbymatrix,0,0,	0,		236,	"void(vector fwd, vector right, vector up)"}, // #236
	{"skinforname",		PF_skinforname,		0,		0,		0,		237,	"float(float mdlindex, string skinname)"},		// #237
	{"shaderforname",	PF_Fixme,			0,		0,		0,		238,	D("float(string shadername, optional string defaultshader, ...)", "Caches the named shader and returns a handle to it.\nIf the shader could not be loaded from disk (missing file or ruleset_allow_shaders 0), it will be created from the 'defaultshader' string if specified, or a 'skin shader' default will be used.\ndefaultshader if not empty should include the outer {} that you would ordinarily find in a shader.")},
	{"te_bloodqw",		PF_te_bloodqw,		0,		0,		0,		239,	"void(vector org, optional float count)"},
	{"te_muzzleflash",	PF_te_muzzleflash,	0,		0,		0,		0,		"void(entity ent)"},

	{"checkpvs",		PF_checkpvs,		0,		0,		0,		240,	"float(vector viewpos, entity entity)"},
	{"matchclientname",	PF_matchclient,		0,		0,		0,		241,	"entity(string match, optional float matchnum)"},
	{"sendpacket",		PF_SendPacket,		0,		0,		0,		242,	"void(string dest, string content)"},// (FTE_QC_SENDPACKET)

//	{"bulleten",		PF_bulleten,		0,		0,		0,		243}, (removed builtin)

	{"rotatevectorsbytag",	PF_Fixme,		0,		0,		0,		244,	"vector(entity ent, float tagnum)"},

	{"mod",				PF_mod,				0,		0,		0,		245,	"float(float dividend, float divisor)"},
//	{"empty",			PF_Fixme,			0,		0,		0,		245,	"void()"},
//	{"empty",			PF_Fixme,			0,		0,		0,		246,	"void()"},
//	{"empty",			PF_Fixme,			0,		0,		0,		247,	"void()"},
//	{"empty",			PF_Fixme,			0,		0,		0,		248,	"void()"},
//	{"empty",			PF_Fixme,			0,		0,		0,		249,	"void()"},

	{"sqlconnect",		PF_sqlconnect,		0,		0,		0,		250,	"float(optional string host, optional string user, optional string pass, optional string defaultdb, optional string driver)"}, // sqlconnect (FTE_SQL)
	{"sqldisconnect",	PF_sqldisconnect,	0,		0,		0,		251,	"void(float serveridx)"}, // sqldisconnect (FTE_SQL)
	{"sqlopenquery",	PF_sqlopenquery,	0,		0,		0,		252,	"float(float serveridx, void(float serveridx, float queryidx, float rows, float columns, float eof, float firstrow) callback, float querytype, string query)"}, // sqlopenquery (FTE_SQL)
	{"sqlclosequery",	PF_sqlclosequery,	0,		0,		0,		253,	"void(float serveridx, float queryidx)"}, // sqlclosequery (FTE_SQL)
	{"sqlreadfield",	PF_sqlreadfield,	0,		0,		0,		254,	"string(float serveridx, float queryidx, float row, float column)"}, // sqlreadfield (FTE_SQL)
	{"sqlerror",		PF_sqlerror,		0,		0,		0,		255,	"string(float serveridx, optional float queryidx)"}, // sqlerror (FTE_SQL)
	{"sqlescape",		PF_sqlescape,		0,		0,		0,		256,	"string(float serveridx, string data)"}, // sqlescape (FTE_SQL)
	{"sqlversion",		PF_sqlversion,		0,		0,		0,		257,	"string(float serveridx)"}, // sqlversion (FTE_SQL)
	{"sqlreadfloat",	PF_sqlreadfloat,	0,		0,		0,		258,	"float(float serveridx, float queryidx, float row, float column)"}, // sqlreadfloat (FTE_SQL)
	{"sqlreadblob",		PF_sqlreadblob,		0,		0,		0,		0,		"int(float serveridx, float queryidx, float row, float column, __variant *ptr, int maxsize)"},
	{"sqlescapeblob",	PF_sqlescapeblob,	0,		0,		0,		0,		"string(float serveridx, __variant *ptr, int maxsize)"},

	{"stoi",			PF_stoi,			0,		0,		0,		259,	D("int(string)", "Converts the given string into a true integer. Base 8, 10, or 16 is determined based upon the format of the string.")},
	{"itos",			PF_itos,			0,		0,		0,		260,	D("string(int)", "Converts the passed true integer into a base10 string.")},
	{"stoh",			PF_stoh,			0,		0,		0,		261,	D("int(string)", "Reads a base-16 string (with or without 0x prefix) as an integer. Bugs out if given a base 8 or base 10 string. :P")},
	{"htos",			PF_htos,			0,		0,		0,		262,	D("string(int)", "Formats an integer as a base16 string, with leading 0s and no prefix. Always returns 8 characters.")},
	{"ftoi",			PF_ftoi,			0,		0,		0,		0,		D("int(float)", "Converts the given float into a true integer without depending on extended qcvm instructions.")},
	{"itof",			PF_itof,			0,		0,		0,		0,		D("float(int)", "Converts the given true integer into a float without depending on extended qcvm instructions.")},

	{"skel_create",		PF_skel_create,		0,		0,		0,		263,	D("float(float modlindex, optional float useabstransforms)", "Allocates a new uninitiaised skeletal object, with enough bone info to animate the given model.\neg: self.skeletonobject = skel_create(self.modelindex);")}, // (FTE_CSQC_SKELETONOBJECTS)
	{"skel_build",		PF_skel_build,		0,		0,		0,		264,	D("float(float skel, entity ent, float modelindex, float retainfrac, float firstbone, float lastbone, optional float addfrac)", "Animation data (according to the entity's frame info) is pulled from the specified model and blended into the specified skeletal object.\nIf retainfrac is set to 0 on the first call and 1 on the others, you can blend multiple animations together according to the addfrac value. The final weight should be 1. Other values will result in scaling and/or other weirdness. You can use firstbone and lastbone to update only part of the skeletal object, to allow legs to animate separately from torso, use 0 for both arguments to specify all, as bones are 1-based.")}, // (FTE_CSQC_SKELETONOBJECTS)
	{"skel_get_numbones",PF_skel_get_numbones,0,	0,		0,		265,	D("float(float skel)", "Retrives the number of bones in the model. The valid range is 1<=bone<=numbones.")}, // (FTE_CSQC_SKELETONOBJECTS)
	{"skel_get_bonename",PF_skel_get_bonename,0,	0,		0,		266,	D("string(float skel, float bonenum)", "Retrieves the name of the specified bone. Mostly only for debugging.")}, // (FTE_CSQC_SKELETONOBJECTS) (returns tempstring)
	{"skel_get_boneparent",PF_skel_get_boneparent,0,0,		0,		267,	D("float(float skel, float bonenum)", "Retrieves which bone this bone's position is relative to. Bone 0 refers to the entity's position rather than an actual bone")}, // (FTE_CSQC_SKELETONOBJECTS)
	{"skel_find_bone",	PF_skel_find_bone,	0,		0,		0,		268,	D("float(float skel, string tagname)", "Finds a bone by its name, from the model that was used to create the skeletal object.")}, // (FTE_CSQC_SKELETONOBJECTS)
	{"skel_get_bonerel",PF_skel_get_bonerel,0,		0,		0,		269,	D("vector(float skel, float bonenum)", "Gets the bone position and orientation relative to the bone's parent. Return value is the offset, and v_forward, v_right, v_up contain the orientation.")}, // (FTE_CSQC_SKELETONOBJECTS) (sets v_forward etc)
	{"skel_get_boneabs",PF_skel_get_boneabs,0,		0,		0,		270,	D("vector(float skel, float bonenum)", "Gets the bone position and orientation relative to the entity. Return value is the offset, and v_forward, v_right, v_up contain the orientation.\nUse gettaginfo for world coord+orientation.")}, // (FTE_CSQC_SKELETONOBJECTS) (sets v_forward etc)
	{"skel_set_bone",	PF_skel_set_bone,	0,		0,		0,		271,	D("void(float skel, float bonenum, vector org, optional vector fwd, optional vector right, optional vector up)", "Sets a bone position relative to its parent. If the orientation arguments are not specified, v_forward+v_right+v_up are used instead.")}, // (FTE_CSQC_SKELETONOBJECTS) (reads v_forward etc)
	{"skel_mul_bone",	PF_skel_mul_bone,	0,		0,		0,		272,	D("void(float skel, float bonenum, vector org, optional vector fwd, optional vector right, optional vector up)", "Transforms a single bone by a matrix. You can use makevectors to generate a rotation matrix from an angle.")}, // (FTE_CSQC_SKELETONOBJECTS) (reads v_forward etc)
	{"skel_mul_bones",	PF_skel_mul_bones,	0,		0,		0,		273,	D("void(float skel, float startbone, float endbone, vector org, optional vector fwd, optional vector right, optional vector up)", "Transforms an entire consecutive range of bones by a matrix. You can use makevectors to generate a rotation matrix from an angle, but you'll probably want to divide the angle by the number of bones.")}, // (FTE_CSQC_SKELETONOBJECTS) (reads v_forward etc)
	{"skel_copybones",	PF_skel_copybones,	0,		0,		0,		274,	D("void(float skeldst, float skelsrc, float startbone, float entbone)", "Copy bone data from one skeleton directly into another.")}, // (FTE_CSQC_SKELETONOBJECTS)
	{"skel_delete",		PF_skel_delete,		0,		0,		0,		275,	D("void(float skel)", "Deletes a skeletal object. The actual delete is delayed, allowing the skeletal object to be deleted in an entity's predraw function yet still be valid by the time the addentity+renderscene builtins need it. Also uninstanciates any ragdoll currently in effect on the skeletal object.")}, // (FTE_CSQC_SKELETONOBJECTS)
	{"frameforname",	PF_frameforname,	0,		0,		0,		276,	D("float(float modidx, string framename)", "Looks up a framegroup from a model by name, avoiding the need for hardcoding. Returns -1 on error.")},// (FTE_CSQC_SKELETONOBJECTS)
	{"frameduration",	PF_frameduration,	0,		0,		0,		277,	D("float(float modidx, float framenum)", "Retrieves the duration (in seconds) of the specified framegroup.")},// (FTE_CSQC_SKELETONOBJECTS)
	{"processmodelevents",PF_processmodelevents,0,	0,		0,		0,		D("void(float modidx, float framenum, __inout float basetime, float targettime, void(float timestamp, int code, string data) callback)", "Calls a callback for each event that has been reached. Basetime is set to targettime.")},
	{"getnextmodelevent",PF_getnextmodelevent,0,	0,		0,		0,		D("float(float modidx, float framenum, __inout float basetime, float targettime, __out int code, __out string data)", "Reports the next event within a model's animation. Returns a boolean if an event was found between basetime and targettime. Writes to basetime,code,data arguments (if an event was found, basetime is set to the event's time, otherwise to targettime).\nWARNING: this builtin cannot deal with multiple events with the same timestamp (only the first will be reported).")},
	{"getmodeleventidx",PF_getmodeleventidx,0,		0,		0,		0,		D("float(float modidx, float framenum, int eventidx, __out float timestamp, __out int code, __out string data)", "Reports an indexed event within a model's animation. Writes to timestamp,code,data arguments on success. Returns false if the animation/event/model was out of range/invalid. Does not consider looping animations (retry from index 0 if it fails and you know that its a looping animation). This builtin is more annoying to use than getnextmodelevent, but can be made to deal with multiple events with the exact same timestamp.")},

	{"crossproduct",	PF_crossproduct,	0,		0,		0,		0,		D("#define dotproduct(v1,v2) ((vector)(v1)*(vector)(v2))\nvector(vector v1, vector v2)", "Small helper function to calculate the crossproduct of two vectors.")},

#ifdef TERRAIN
	{"terrain_edit",	PF_terrain_edit,	0,		0,		0,		278,	D("void(float action, optional vector pos, optional float radius, optional float quant, ...)", "Realtime terrain editing. Actions are the TEREDIT_ constants.")},// (??FTE_TERRAIN_EDIT??

#define qcbrushface				\
	"typedef struct\n{\n"		\
	"\tstring\tshadername;\n"	\
	"\tvector\tplanenormal;\n"	\
	"\tfloat\tplanedist;\n"		\
	"\tvector\tsdir;\n"			\
	"\tfloat\tsbias;\n"		\
	"\tvector\ttdir;\n"			\
	"\tfloat\ttbias;\n"		\
	"} brushface_t;\n"
	{"brush_get",		PF_brush_get,		0,		0,		0,		0,		D(qcbrushface "int(float modelidx, int brushid, brushface_t *out_faces, int maxfaces, int *out_contents)", "Queries a brush's information. You must pre-allocate the face array for the builtin to write to. Return value is the number of faces retrieved, 0 on error.")},
	{"brush_create",	PF_brush_create,	0,		0,		0,		0,		D("int(float modelidx, brushface_t *in_faces, int numfaces, int contents, optional int brushid)", "Inserts a new brush into the model. Return value is the new brush's id.")},
	{"brush_delete",	PF_brush_delete,	0,		0,		0,		0,		D("void(float modelidx, int brushid)", "Destroys the specified brush.")},
	{"brush_selected",	PF_brush_selected,	0,		0,		0,		0,		D("float(float modelid, int brushid, int faceid, float selectedstate)", "Allows you to easily set transient visual properties of a brush. returns old value. selectedstate=-1 changes nothing (called for its return value).")},
	{"brush_getfacepoints",PF_brush_getfacepoints,0,0,		0,		0,		D("int(float modelid, int brushid, int faceid, vector *points, int maxpoints)", "Returns the list of verticies surrounding the given face. If face is 0, returns the center of the brush (if space for 1 point) or the mins+maxs (if space for 2 points).")},
	{"brush_calcfacepoints",PF_brush_calcfacepoints,0,0,	0,		0,		D("int(int faceid, brushface_t *in_faces, int numfaces, vector *points, int maxpoints)", "Determines the points of the specified face, if the specified brush were to actually be created.")},
	{"brush_findinvolume",PF_brush_findinvolume,0,	0,		0,		0,		D("int(float modelid, vector *planes, float *dists, int numplanes, int *out_brushes, int *out_faces, int maxresults)", "Allows you to easily obtain a list of brushes+faces within the given bounding region. If out_faces is not null, the same brush might be listed twice.")},
//	{"brush_editplane",	PF_brush_editplane,	0,		0,		0,		0,		D("float(float modelid, int brushid, int faceid, in brushface *face)", "Changes a surface's texture info.")},
//	{"brush_transformselected",PF_brush_transformselected,0,0,0,	0,		D("int(float modelid, int brushid, float *matrix)", "Transforms selected brushes by the given transform")},
#endif


	{"touchtriggers",	PF_touchtriggers,	0,		0,		0,		279,	D("void(optional entity ent, optional vector neworigin)", "Triggers a touch events between self and every SOLID_TRIGGER entity that it is in contact with. This should typically just be the triggers touch functions. Also optionally updates the origin of the moved entity.")},//
	{"WriteFloat",		PF_WriteFloat,		0,		0,		0,		280,	"void(float buf, float fl)"},//
	{"skel_ragupdate",	PF_skel_ragedit,	0,		0,		0,		281,	D("float(entity skelent, string dollcmd, float animskel)", "Updates the skeletal object attached to the entity according to its origin and other properties.\nif animskel is non-zero, the ragdoll will animate towards the bone state in the animskel skeletal object, otherwise they will pick up the model's base pose which may not give nice results.\nIf dollcmd is not set, the ragdoll will update (this should be done each frame).\nIf the doll is updated without having a valid doll, the model's default .doll will be instanciated.\ncommands:\n doll foo.doll : sets up the entity to use the named doll file\n dollstring TEXT : uses the doll file directly embedded within qc, with that extra prefix.\n cleardoll : uninstanciates the doll without destroying the skeletal object.\n animate 0.5 : specifies the strength of the ragdoll as a whole \n animatebody somebody 0.5 : specifies the strength of the ragdoll on a specific body (0 will disable ragdoll animations on that body).\n enablejoint somejoint 1 : enables (or disables) a joint. Disabling joints will allow the doll to shatter.")}, // (FTE_CSQC_RAGDOLL)
	{"skel_mmap",		PF_skel_mmap,		0,		0,		0,		282,	D("float*(float skel)", "Map the bones in VM memory. They can then be accessed via pointers. Each bone is 12 floats, the four vectors interleaved (sadly).")},// (FTE_QC_RAGDOLL)
	{"skel_set_bone_world",PF_skel_set_bone_world,0,0,		0,		283,	D("void(entity ent, float bonenum, vector org, optional vector angorfwd, optional vector right, optional vector up)", "Sets the world position of a bone within the given entity's attached skeletal object. The world position is dependant upon the owning entity's position. If no orientation argument is specified, v_forward+v_right+v_up are used for the orientation instead. If 1 is specified, it is understood as angles. If 3 are specified, they are the forawrd/right/up vectors to use.")},
	{"frametoname",		PF_frametoname,		0,		0,		0,		284,	"string(float modidx, float framenum)"},
	{"skintoname",		PF_skintoname,		0,		0,		0,		285,	"string(float modidx, float skin)"},
	{"resourcestatus",	PF_resourcestatus,	0,		0,		0,		286,	D("float(float resourcetype, float tryload, string resourcename)", "resourcetype must be one of the RESTYPE_ constants. Returns one of the RESSTATE_ constants. Tryload 0 is a query only. Tryload 1 will attempt to reload the content if it was flushed.")},
	{"hash_createtab",	PF_hash_createtab,	0,		0,		0,		287,	D("hashtable(float tabsize, optional float defaulttype)", "Creates a hash table object with at least 'tabsize' slots. hash table with index 0 is a game-persistant table and will NEVER be returned by this builtin (except as an error return).")},
	{"hash_destroytab",	PF_hash_destroytab,	0,		0,		0,		288,	D("void(hashtable table)", "Destroys a hash table object.")},
	{"hash_add",		PF_hash_add,		0,		0,		0,		289,	D("void(hashtable table, string name, __variant value, optional float typeandflags)", "Adds the given key with the given value to the table.\nIf flags&HASH_REPLACE, the old value will be removed, if not set then multiple values may be added for a single key, they won't overwrite.\nThe type argument describes how the value should be stored and saved to files. While you can claim that all variables are just vectors, being more precise can result in less issues with tempstrings or saved games.")},
	{"hash_get",		PF_hash_get,		0,		0,		0,		290,	D("__variant(hashtable table, string name, optional __variant deflt, optional float requiretype, optional float index)", "looks up the specified key name in the hash table. returns deflt if key was not found. If stringsonly=1, the return value will be in the form of a tempstring, otherwise it'll be the original value argument exactly as it was. If requiretype is specified, then values not of the specified type will be ignored. Hurrah for multiple types with the same name.")},
	{"hash_delete",		PF_hash_delete,		0,		0,		0,		291,	D("__variant(hashtable table, string name)", "removes the named key. returns the value of the object that was destroyed, or 0 on error.")},
	{"hash_getkey",		PF_hash_getkey,		0,		0,		0,		292,	D("string(hashtable table, float idx)", "gets some random key name. add+delete can change return values of this, so don't blindly increment the key index if you're removing all.")},
	{"hash_getcb",		PF_hash_getcb,		0,		0,		0,		293,	D("void(hashtable table, void(string keyname, __variant val) callback, optional string name)", "For each item in the table that matches the name, call the callback. if name is omitted, will enumerate ALL keys."), true},
	{"checkcommand",	PF_checkcommand,	0,		0,		0,		294,	D("float(string name)", "Checks to see if the supplied name is a valid command, cvar, or alias. Returns 0 if it does not exist.")},
	{"argescape",		PF_argescape,		0,		0,		0,		295,	D("string(string s)", "Marks up a string so that it can be reliably tokenized as a single argument later.")},
//	{"cvar_setlatch",	PF_cvar_setlatch,	0,		0,		0,		???,	"void(string cvarname, optional string value)"},
	{"clusterevent",	PF_clusterevent,	0,		0,		0,		0,		D("void(string dest, string from, string cmd, string info)", "Only functions in mapcluster mode. Sends an event to whichever server the named player is on. The destination server can then dispatch the event to the client or handle it itself via the SV_ParseClusterEvent entrypoint. If dest is empty, the event is broadcast to ALL servers. If the named player can't be found, the event will be returned to this server with the cmd prefixed with 'error:'.")},
	{"clustertransfer",	PF_clustertransfer,	0,		0,		0,		0,		D("string(entity player, optional string newnode)", "Only functions in mapcluster mode. Initiate transfer of the player to a different node. Can take some time. If dest is specified, returns null on error. Otherwise returns the current/new target node (or null if not transferring).")},
	{"modelframecount", PF_modelframecount, 0,		0,		0,		0,		D("float(float mdlidx)", "Retrieves the number of frames in the specified model.")},

	{"clearscene",		PF_Fixme,	0,		0,		0,		300,	D("void()", "Forgets all rentities, polygons, and temporary dlights. Resets all view properties to their default values.")},// (EXT_CSQC)
	{"addentities",		PF_Fixme,	0,		0,		0,		301,	D("void(float mask)", "Walks through all entities effectively doing this:\n if (ent.drawmask&mask){ if (!ent.predaw()) addentity(ent); }\nIf mask&MASK_DELTA, non-csqc entities, particles, and related effects will also be added to the rentity list.\n If mask&MASK_STDVIEWMODEL then the default view model will also be added.")},// (EXT_CSQC)
	{"addentity",		PF_Fixme,	0,		0,		0,		302,	D("void(entity ent)", "Copies the entity fields into a new rentity for later rendering via addscene.")},// (EXT_CSQC)
	{"addtrisoup_1",	PF_Fixme,	0,		0,		0,		0,		D("void(string texturename, float flags, void *verts, int *indexes, int numindexes)", "Adds the specified trisoup into the scene as additional geometry. This permits caching geometry to reduce builtin spam. Indexes are a triangle list (so eg quads will need 6 indicies to form two triangles). NOTE: this is not going to be a speedup over polygons if you're still generating lots of new data every frame.")},
	{"setproperty",		PF_Fixme,	0,		0,		0,		303,	D("#define setviewprop setproperty\nfloat(float property, ...)", "Allows you to override default view properties like viewport, fov, and whether the engine hud will be drawn. Different VF_ values have slightly different arguments, some are vectors, some floats.")},// (EXT_CSQC)
	{"renderscene",		PF_Fixme,	0,		0,		0,		304,	D("void()", "Draws all entities, polygons, and particles on the rentity list (which were added via addentities or addentity), using the various view properties set via setproperty. There is no ordering dependancy.\nThe scene must generally be cleared again before more entities are added, as entities will persist even over to the next frame.\nYou may call this builtin multiple times per frame, but should only be called from CSQC_UpdateView.")},// (EXT_CSQC)

	{"dynamiclight_add",PF_Fixme,	0,		0,		0,		305,	D("float(vector org, float radius, vector lightcolours, optional float style, optional string cubemapname, optional float pflags)", "Adds a temporary dlight, ready to be drawn via addscene. Cubemap orientation will be read from v_forward/v_right/v_up.")},// (EXT_CSQC)

	//gonna expose these to ssqc as a debugging extension
	{"R_BeginPolygon",	PF_R_PolygonBegin,0,0,		0,		306,	D("void(string texturename, optional float flags, optional float is2d)", "Specifies the shader to use for the following polygons, along with optional flags.\nIf is2d, the polygon will be drawn as soon as the EndPolygon call is made, rather than waiting for renderscene. This allows complex 2d effects.")},// (EXT_CSQC_???)
	{"R_PolygonVertex",	PF_R_PolygonVertex,0,0,		0,		307,	D("void(vector org, vector texcoords, vector rgb, float alpha)", "Specifies a polygon vertex with its various properties.")},// (EXT_CSQC_???)
	{"R_EndPolygon",	PF_R_PolygonEnd,0,	0,		0,		308,	D("void()", "Ends the current polygon. At least 3 verticies must have been specified. You do not need to call beginpolygon if you wish to draw another polygon with the same shader.")},

	{"getproperty",		PF_Fixme,	0,		0,		0,		309,	D("#define getviewprop getproperty\n__variant(float property)", "Retrieve a currently-set (typically view) property, allowing you to read the current viewport or other things. Due to cheat protection, certain values may be unretrievable.")},// (EXT_CSQC_1)

//310
//maths stuff that uses the current view settings.
	{"unproject",		PF_Fixme,	0,		0,		0,		310,	D("vector (vector v)", "Transform a 2d screen-space point (with depth) into a 3d world-space point, according the various origin+angle+fov etc settings set via setproperty.")},// (EXT_CSQC)
	{"project",			PF_Fixme,	0,		0,		0,		311,	D("vector (vector v)", "Transform a 3d world-space point into a 2d screen-space point, according the various origin+angle+fov etc settings set via setproperty.")},// (EXT_CSQC)
															//312
															//313
//2d (immediate) operations
	{"drawtextfield",	PF_Fixme,	0,		0,		0,	 0/*314*/,	D("void(vector pos, vector size, float alignflags, string text)", "Draws a multi-line block of text, including word wrapping and alignment. alignflags bits are RTLB, typically 3.")},// (EXT_CSQC)
	{"drawline",		PF_Fixme,	0,		0,		0,		315,	D("void(float width, vector pos1, vector pos2, vector rgb, float alpha, optional float drawflag)", "Draws a 2d line between the two 2d points.")},// (EXT_CSQC)
	{"iscachedpic",		PF_Fixme,	0,		0,		0,		316,	D("float(string name)", "Checks to see if the image is currently loaded. Engines might lie, or cache between maps.")},// (EXT_CSQC)
	{"precache_pic",	PF_Fixme,	0,		0,		0,		317,	D("string(string name, optional float trywad)", "Forces the engine to load the named image. If trywad is specified, the specified name must any lack path and extension.")},// (EXT_CSQC)
	{"r_uploadimage",	PF_Fixme,	0,		0,		0,		0,		D("void(string imagename, int width, int height, int *pixeldata)", "Updates a texture with the specified rgba data. Will be created if needed.")},
	{"r_readimage",		PF_Fixme,	0,		0,		0,		0,		D("int*(string filename, __out int width, __out int height)", "Reads and decodes an image from disk, providing raw pixel data. Returns __NULL__ if the image could not be read for any reason. Use memfree to free the data once you're done with it.")},
	{"drawgetimagesize",PF_Fixme,	0,		0,		0,		318,	D("#define draw_getimagesize drawgetimagesize\nvector(string picname)", "Returns the dimensions of the named image. Images specified with .lmp should give the original .lmp's dimensions even if texture replacements use a different resolution.")},// (EXT_CSQC)
	{"freepic",			PF_Fixme,	0,		0,		0,		319,	D("void(string name)", "Tells the engine that the image is no longer needed. The image will appear to be new the next time its needed.")},// (EXT_CSQC)
//320
	{"drawcharacter",	PF_Fixme,	0,		0,		0,		320,	D("float(vector position, float character, vector size, vector rgb, float alpha, optional float drawflag)", "Draw the given quake character at the given position.\nIf flag&4, the function will consider the char to be a unicode char instead (or display as a ? if outside the 32-127 range).\nsize should normally be something like '8 8 0'.\nrgb should normally be '1 1 1'\nalpha normally 1.\nSoftware engines may assume the named defaults.\nNote that ALL text may be rescaled on the X axis due to variable width fonts. The X axis may even be ignored completely.")},// (EXT_CSQC, [EXT_CSQC_???])
	{"drawrawstring",	PF_Fixme,	0,		0,		0,		321,	D("float(vector position, string text, vector size, vector rgb, float alpha, optional float drawflag)", "Draws the specified string without using any markup at all, even in engines that support it.\nIf UTF-8 is globally enabled in the engine, then that encoding is used (without additional markup), otherwise it is raw quake chars.\nSoftware engines may assume a size of '8 8 0', rgb='1 1 1', alpha=1, flag&3=0, but it is not an error to draw out of the screen.")},// (EXT_CSQC, [EXT_CSQC_???])
	{"drawpic",			PF_Fixme,	0,		0,		0,		322,	D("float(vector position, string pic, vector size, vector rgb, float alpha, optional float drawflag)", "Draws an shader within the given 2d screen box. Software engines may omit support for rgb+alpha, but must support rescaling, and must clip to the screen without crashing.")},// (EXT_CSQC, [EXT_CSQC_???])
	{"drawfill",		PF_Fixme,	0,		0,		0,		323,	D("float(vector position, vector size, vector rgb, float alpha, optional float drawflag)", "Draws a solid block over the given 2d box, with given colour, alpha, and blend mode (specified via flags).\nflags&3=0 simple blend.\nflags&3=1 additive blend")},// (EXT_CSQC, [EXT_CSQC_???])
	{"drawsetcliparea",	PF_Fixme,	0,		0,		0,		324,	D("void(float x, float y, float width, float height)", "Specifies a 2d clipping region (aka: scissor test). 2d draw calls will all be clipped to this 2d box, the area outside will not be modified by any 2d draw call (even 2d polygons).")},// (EXT_CSQC_???)
	{"drawresetcliparea",PF_Fixme,	0,		0,		0,		325,	D("void(void)", "Reverts the scissor/clip area to the whole screen.")},// (EXT_CSQC_???)

	{"drawstring",		PF_Fixme,	0,		0,		0,		326,	D("float(vector position, string text, vector size, vector rgb, float alpha, float drawflag)", "Draws a string, interpreting markup and recolouring as appropriate.")},// #326
	{"stringwidth",		PF_Fixme,	0,		0,		0,		327,	D("float(string text, float usecolours, optional vector fontsize)", "Calculates the width of the screen in virtual pixels. If usecolours is 1, markup that does not affect the string width will be ignored. Will always be decoded as UTF-8 if UTF-8 is globally enabled.\nIf the char size is not specified, '8 8 0' will be assumed.")},// EXT_CSQC_'DARKPLACES'
	{"drawsubpic",		PF_Fixme,	0,		0,		0,		328,	D("void(vector pos, vector sz, string pic, vector srcpos, vector srcsz, vector rgb, float alpha, optional float drawflag)", "Draws a rescaled subsection of an image to the screen.")},// #328 EXT_CSQC_'DARKPLACES'
	{"drawrotpic",		PF_Fixme,	0,		0,		0,		0,		D("void(vector pivot, vector mins, vector maxs, string pic, vector rgb, float alpha, float angle)", "Draws an image rotating at the pivot. To rotate in the center, use mins+maxs of half the size with mins negated. Angle is in degrees.")},
	{"drawrotsubpic",	PF_Fixme,	0,		0,		0,		0,		D("void(vector pivot, vector mins, vector maxs, string pic, vector txmin, vector txsize, vector rgb, vector alphaandangles)", "Overcomplicated draw function for over complicated people. Positions follow drawrotpic, while texture coords follow drawsubpic. Due to argument count limitations in builtins, the alpha value and angles are combined into separate fields of a vector (tip: use fteqcc's [alpha, angle] feature.")},

//330
	{"getstati",		PF_Fixme,	0,		0,		0,		330,	D("#define getstati_punf(stnum) (float)(__variant)getstati(stnum)\nint(float stnum)", "Retrieves the numerical value of the given EV_INTEGER or EV_ENTITY stat (converted to a float).")},// (EXT_CSQC)
	{"getstatf",		PF_Fixme,	0,		0,		0,		331,	D("#define getstatbits getstatf\nfloat(float stnum, optional float firstbit, optional float bitcount)", "Retrieves the numerical value of the given EV_FLOAT stat. If firstbit and bitcount are specified, retrieves the upper bits of the STAT_ITEMS stat (converted into a float, so there are no VM dependancies).")},// (EXT_CSQC)
	{"getstats",		PF_Fixme,	0,		0,		0,		332,	D("string(float stnum)", "Retrieves the value of the given EV_STRING stat, as a tempstring.\nOlder engines may use 4 consecutive integer stats, with a limit of 15 chars (yes, really. 15.), but "FULLENGINENAME" uses a separate namespace for string stats and has a much higher length limit.")},
	{"getplayerstat",	PF_Fixme,	0,		0,		0,		0,		D("__variant(float playernum, float statnum, float stattype)", "Retrieves a specific player's stat, matching the type specified on the server. This builtin is primarily intended for mvd playback where ALL players are known. For EV_ENTITY, world will be returned if the entity is not in the pvs, use type-punning with EV_INTEGER to get the entity number if you just want to see if its set. STAT_ITEMS should be queried as an EV_INTEGER on account of runes and items2 being packed into the upper bits.")},

//EXT_CSQC
	{"setmodelindex",	PF_Fixme,	0,		0,		0,		333,	D("void(entity e, float mdlindex)", "Sets a model by precache index instead of by name. Otherwise identical to setmodel.")},//
	{"modelnameforindex",PF_Fixme,	0,		0,		0,		334,	D("string(float mdlindex)", "Retrieves the name of the model based upon a precache index. This can be used to reduce csqc network traffic by enabling model matching.")},//

	{"particleeffectnum",PF_sv_particleeffectnum,0,0,0,		335,	D("float(string effectname)", "Precaches the named particle effect. If your effect name is of the form 'foo.bar' then particles/foo.cfg will be loaded by the client if foo.bar was not already defined.\nDifferent engines will have different particle systems, this specifies the QC API only.")},// (EXT_CSQC)
	{"trailparticles",	PF_sv_trailparticles,0,0,	0,		336,	D("void(float effectnum, entity ent, vector start, vector end)", "Draws the given effect between the two named points. If ent is not world, distances will be cached in the entity in order to avoid framerate dependancies. The entity is not otherwise used.")},// (EXT_CSQC),
//	{"trailparticles_dp",PF_sv_trailparticles,0,0,	0,		336,	D("void(entity ent, float effectnum, vector start, vector end)", "DarkPlaces got the argument order wrong, and failed to fix it due to apathy.")},// (EXT_CSQC),
	{"pointparticles",	PF_sv_pointparticles,0,0,	0,		337,	D("void(float effectnum, vector origin, optional vector dir, optional float count)", "Spawn a load of particles from the given effect at the given point traveling or aiming along the direction specified. The number of particles are scaled by the count argument.")},// (EXT_CSQC)

	{"cprint",			PF_Fixme,	0,		0,		0,		338,	D("void(string s, ...)", "Print into the center of the screen just as ssqc's centerprint would appear.")},//(EXT_CSQC)
	{"print",			PF_print,	0,		0,		0,		339,	D("void(string s, ...)", "Unconditionally print on the local system's console, even in ssqc (doesn't care about the value of the developer cvar).")},//(EXT_CSQC)


	{"keynumtostring",	PF_Fixme,	0,		0,		0,		340,	D("string(float keynum)", "Returns a hunam-readable name for the given keycode, as a tempstring.")},// (EXT_CSQC)
	{"keynumtostring_csqc",PF_Fixme,0,		0,		0,		340,	D("string(float keynum)", "Returns a hunam-readable name for the given keycode, as a tempstring.")},// (found in menuqc)
	{"stringtokeynum",	PF_Fixme,	0,		0,		0,		341,	D("float(string keyname)", "Looks up the key name in the same way that the bind command would, returning the keycode for that key.")},// (EXT_CSQC)
	{"stringtokeynum_csqc",	PF_Fixme,0,		0,		0,		341,	D("float(string keyname)", "Looks up the key name in the same way that the bind command would, returning the keycode for that key.")},// (found in menuqc)
	{"getkeybind",		PF_Fixme,	0,		0,		0,		342,	D("string(float keynum)", "Returns the current binding for the given key (returning only the command executed when no modifiers are pressed).")},// (EXT_CSQC)

	{"setcursormode",	PF_Fixme,	0,		0,		0,		343,	D("void(float usecursor, optional string cursorimage, optional vector hotspot, optional float scale)", "Pass TRUE if you want the engine to release the mouse cursor (absolute input events + touchscreen mode). Pass FALSE if you want the engine to grab the cursor (relative input events + standard looking). If the image name is specified, the engine will use that image for a cursor (use an empty string to clear it again), in a way that will not conflict with the console. Images specified this way will be hardware accelerated, if supported by the platform/port.")},
	{"getcursormode",	PF_Fixme,	0,		0,		0,		0,		D("float(float effective)", "Reports the cursor mode this module previously attempted to use. If 'effective' is true, reports the cursor mode currently active (if was overriden by a different module which has precidence, for instance, or if there is only a touchscreen and no mouse).")},
	{"getmousepos",		PF_Fixme,	0,		0,		0,		344,	D("vector()", "Nasty convoluted DP extension. Typically returns deltas instead of positions. Use CSQC_InputEvent for such things in csqc mods.")},	// #344 This is a DP extension

	{"getinputstate",	PF_Fixme,	0,		0,		0,		345,	D("float(float inputsequencenum)", "Looks up an input frame from the log, setting the input_* globals accordingly.\nThe sequence number range used for prediction should normally be servercommandframe < sequence <= clientcommandframe.\nThe sequence equal to clientcommandframe will change between input frames.")},// (EXT_CSQC)
	{"setsensitivityscaler",PF_Fixme,0,		0,		0,		346,	D("void(float sens)", "Temporarily scales the player's mouse sensitivity based upon something like zoom, avoiding potential cvar saving and thus corruption.")},// (EXT_CSQC)


	{"runstandardplayerphysics",PF_runclientphys,0,0,0,		347,	D("void(entity ent)", "Perform the engine's standard player movement prediction upon the given entity using the input_* globals to describe movement.")},
	{"getplayerkeyvalue",	PF_Fixme,0,		0,		0,		348,	D("string(float playernum, string keyname)", "Look up a player's userinfo, to discover things like their name, topcolor, bottomcolor, skin, team, *ver.\nAlso includes scoreboard info like frags, ping, pl, userid, entertime, as well as voipspeaking and voiploudness.")},// (EXT_CSQC)

	{"isdemo",			PF_Fixme,	0,		0,		0,		349,	D("float()", "Returns if the client is currently playing a demo or not")},// (EXT_CSQC)
	{"isserver",		PF_Fixme,	0,		0,		0,		350,	D("float()", "Returns if the client is acting as the server (aka: listen server)")},//(EXT_CSQC)
	{"SetListener",		PF_Fixme, 	0,		0,		0,		351,	D("void(vector origin, vector forward, vector right, vector up, optional float reverbtype)", "Sets the position of the view, as far as the audio subsystem is concerned. This should be called once per CSQC_UpdateView as it will otherwise revert to default. For reverbtype, see setup_reverb or treat as 'underwater'.")},// (EXT_CSQC)
	{"setup_reverb",	PF_Fixme, 	0,		0,		0,		0,		D("typedef struct {\n\tfloat flDensity;\n\tfloat flDiffusion;\n\tfloat flGain;\n\tfloat flGainHF;\n\tfloat flGainLF;\n\tfloat flDecayTime;\n\tfloat flDecayHFRatio;\n\tfloat flDecayLFRatio;\n\tfloat flReflectionsGain;\n\tfloat flReflectionsDelay;\n\tvector flReflectionsPan;\n\tfloat flLateReverbGain;\n\tfloat flLateReverbDelay;\n\tvector flLateReverbPan;\n\tfloat flEchoTime;\n\tfloat flEchoDepth;\n\tfloat flModulationTime;\n\tfloat flModulationDepth;\n\tfloat flAirAbsorptionGainHF;\n\tfloat flHFReference;\n\tfloat flLFReference;\n\tfloat flRoomRolloffFactor;\n\tint   iDecayHFLimit;\n} reverbinfo_t;\nvoid(float reverbslot, reverbinfo_t *reverbinfo, int sizeofreverinfo_t)", "Reconfigures a reverb slot for weird effects. Slot 0 is reserved for no effects. Slot 1 is reserved for underwater effects. Reserved slots will be reinitialised on snd_restart, but can otherwise be changed. These reverb slots can be activated with SetListener. Note that reverb will currently only work when using OpenAL.")},
	{"registercommand",	PF_Fixme,	0,		0,		0,		352,	D("void(string cmdname)", "Register the given console command, for easy console use.\nConsole commands that are later used will invoke CSQC_ConsoleCommand.")},//(EXT_CSQC)
	{"wasfreed",		PF_WasFreed,0,		0,		0,		353,	D("float(entity ent)", "Quickly check to see if the entity is currently free. This function is only valid during the two-second non-reuse window, after that it may give bad results. Try one second to make it more robust.")},//(EXT_CSQC) (should be availabe on server too)
	{"serverkey",		PF_sv_serverkey,0,	0,		0,		354,	D("string(string key)", "Look up a key in the server's public serverinfo string")},//
	{"getentitytoken",	PF_Fixme,	0,		0,		0,		355,	D("string(optional string resetstring)", "Grab the next token in the map's entity lump.\nIf resetstring is not specified, the next token will be returned with no other sideeffects.\nIf empty, will reset from the map before returning the first token, probably {.\nIf not empty, will tokenize from that string instead.\nAlways returns tempstrings.")},//;
	{"findfont",		PF_Fixme,	0,		0,		0,		356,	D("float(string s)", "Looks up a named font slot. Matches the actual font name as a last resort.")},//;
	{"loadfont",		PF_Fixme,	0,		0,		0,		357,	D("float(string fontname, string fontmaps, string sizes, float slot, optional float fix_scale, optional float fix_voffset)", "too convoluted for me to even try to explain correct usage. Try drawfont = loadfont(\"\", \"cour\", \"16\", -1, 0, 0); to switch to the courier font (optimised for 16 virtual pixels high), if you have the freetype2 library in windows..")},
	{"sendevent",		PF_Fixme,	0,		0,		0,		359,	D("void(string evname, string evargs, ...)", "Invoke Cmd_evname_evargs in ssqc. evargs must be a string of initials refering to the types of the arguments to pass. v=vector, e=entity(.entnum field is sent), f=float, i=int. 6 arguments max - you can get more if you pack your floats into vectors.")},// (EXT_CSQC_1)

	{"readbyte",		PF_Fixme,	0,		0,		0,		360,	"float()"},// (EXT_CSQC)
	{"readchar",		PF_Fixme,	0,		0,		0,		361,	"float()"},// (EXT_CSQC)
	{"readshort",		PF_Fixme,	0,		0,		0,		362,	"float()"},// (EXT_CSQC)
	{"readlong",		PF_Fixme,	0,		0,		0,		363,	"float()"},// (EXT_CSQC)
	{"readcoord",		PF_Fixme,	0,		0,		0,		364,	"float()"},// (EXT_CSQC)

	{"readangle",		PF_Fixme,	0,		0,		0,		365,	"float()"},// (EXT_CSQC)
	{"readstring",		PF_Fixme,	0,		0,		0,		366,	"string()"},// (EXT_CSQC)
	{"readfloat",		PF_Fixme,	0,		0,		0,		367,	"float()"},// (EXT_CSQC)
	{"readentitynum",	PF_Fixme,	0,		0,		0,		368,	"float()"},// (EXT_CSQC)

//	{"readserverentitystate",PF_Fixme,0,	0,		0,		369,	"void(float flags, float simtime)"},// (EXT_CSQC_1)
//	{"readsingleentitystate",PF_Fixme,0,	0,		0,		370},
	{"deltalisten",		PF_Fixme,	0,		0,		0,		371,	D("float(string modelname, float(float isnew) updatecallback, float flags)", "Specifies a per-modelindex callback to listen for engine-networking entity updates. Such entities are automatically interpolated by the engine (unless flags specifies not to).\nThe various standard entity fields will be overwritten each frame before the updatecallback function is called.")},//  (EXT_CSQC_1)

	{"dynamiclight_get",PF_Fixme,	0,		0,		0,		372,	D("__variant(float lno, float fld)", "Retrieves a property from the given dynamic/rt light. Return type depends upon the light field requested.")},
	{"dynamiclight_set",PF_Fixme,	0,		0,		0,		373,	D("void(float lno, float fld, __variant value)", "Changes a property on the given dynamic/rt light. Value type depends upon the light field to be changed.")},
	{"particleeffectquery",PF_Fixme,0,		0,		0,		374,	D("string(float efnum, float body)", "Retrieves either the name or the body of the effect with the given number. The effect body is regenerated from internal state, and can be changed before being reapplied via the localcmd builtin.")},

	{"adddecal",		PF_Fixme,	0,		0,		0,		375,	D("void(string shadername, vector origin, vector up, vector side, vector rgb, float alpha)", "Adds a temporary clipped decal shader to the scene, centered at the given point with given orientation. Will be drawn by the next renderscene call, and freed by the next clearscene call.")},
	{"setcustomskin",	PF_Fixme,	0,		0,		0,		376,	D("void(entity e, string skinfilename, optional string skindata)", "Sets an entity's skin overrides. These are custom per-entity surface->shader lookups. The skinfilename/data should be in .skin format:\nsurfacename,shadername - makes the named surface use the named shader\nreplace \"surfacename\" \"shadername\" - same.\nqwskin \"foo\" - use an unmodified quakeworld player skin (including crop+repalette rules)\nq1lower 0xff0000 - specify an override for the entity's lower colour, in this case to red\nq1upper 0x0000ff - specify an override for the entity's lower colour, in this case to blue\ncompose \"surfacename\" \"shader\" \"imagename@x,y:w,h$s,t,s2,t2?r,g,b,a\" - compose a skin texture from multiple images.\n  The texture is determined to be sufficient to hold the first named image, additional images can be named as extra tokens on the same line.\n  Use a + at the end of the line to continue reading image tokens from the next line also, the named shader must use 'map $diffuse' to read the composed texture (compatible with the defaultskin shader).")},
//END EXT_CSQC

	{"memalloc",		PF_memalloc,		0,		0,		0,		384,	D("__variant*(int size)", "Allocate an arbitary block of memory")},
	{"memfree",			PF_memfree,			0,		0,		0,		385,	D("void(__variant *ptr)", "Frees a block of memory that was allocated with memfree")},
	{"memcpy",			PF_memcpy,			0,		0,		0,		386,	D("void(__variant *dst, __variant *src, int size)", "Copys memory from one location to another")},
	{"memfill8",		PF_memfill8,		0,		0,		0,		387,	D("void(__variant *dst, int val, int size)", "Sets an entire block of memory to a specified value. Pretty much always 0.")},
	{"memgetval",		PF_memgetval,		0,		0,		0,		388,	D("__variant(__variant *dst, float ofs)", "Looks up the 32bit value stored at a pointer-with-offset.")},
	{"memsetval",		PF_memsetval,		0,		0,		0,		389,	D("void(__variant *dst, float ofs, __variant val)", "Changes the 32bit value stored at the specified pointer-with-offset.")},
	{"memptradd",		PF_memptradd,		0,		0,		0,		390,	D("__variant*(__variant *base, float ofs)", "Perform some pointer maths. Woo.")},
	{"memstrsize",		PF_memstrsize,		0,		0,		0,		0,		D("float(string s)", "strlen, except ignores utf-8")},

	{"con_getset",		PF_Fixme,			0,		0,		0,		391,	D("string(string conname, string field, optional string newvalue)", "Reads or sets a property from a console object. The old value is returned. Iterrate through consoles with the 'next' field. Valid properties: 	title, name, next, unseen, markup, forceutf8, close, clear, hidden, linecount")},
	{"con_printf",		PF_Fixme,			0,		0,		0,		392,	D("void(string conname, string messagefmt, ...)", "Prints onto a named console.")},
	{"con_draw",		PF_Fixme,			0,		0,		0,		393,	D("void(string conname, vector pos, vector size, float fontsize)", "Draws the named console.")},
	{"con_input",		PF_Fixme,			0,		0,		0,		394,	D("float(string conname, float inevtype, float parama, float paramb, float paramc)", "Forwards input events to the named console. Mouse updates should be absolute only.")},
	{"setwindowcaption",PF_Fixme,			0,		0,		0,		0,		D("void(string newcaption)", "Replaces the title of the game window, as seen when task switching or just running in windowed mode.")},
	{"cvars_haveunsaved",PF_Fixme,			0,		0,		0,		0,		D("float()", "Returns true if any archived cvar has an unsaved value.")},

	{"entityprotection",PF_entityprotection,0,		0,		0,		0,		D("float(entity e, float nowreadonly)", "Changes the protection on the specified entity to protect it from further edits from QC. The return value is the previous setting. Note that this can be used to unprotect the world, but doing so long term is not advised as you will no longer be able to detect invalid entity references. Also, world is not networked, so results might not be seen by clients (or in other words, world.avelocity_y=64 is a bad idea).")},
//end fte extras

//DP extras

//DP_QC_COPYENTITY
	{"copyentity",		PF_copyentity,		0,		0,		0,		400,	D("entity(entity from, optional entity to)", "Copies all fields from one entity to another.")},// (DP_QC_COPYENTITY)
//DP_SV_SETCOLOR
	{"setcolors",		PF_setcolors,		0,		0,		0,		401,	D("void(entity ent, float colours)", "Changes a player's colours. The bits 0-3 are the lower/trouser colour, bits 4-7 are the upper/shirt colours.")},//DP_SV_SETCOLOR
//DP_QC_FINDCHAIN
	{"findchain",		PF_sv_findchain,	0,		0,		0,		402,	"entity(.string field, string match)"},// (DP_QC_FINDCHAIN)
//DP_QC_FINDCHAINFLOAT
	{"findchainfloat",	PF_sv_findchainfloat,0,		0,		0,		403,	"entity(.float fld, float match)"},// (DP_QC_FINDCHAINFLOAT)
//DP_SV_EFFECT
	{"effect",			PF_effect,			0,		0,		0,		404,	D("void(vector org, string modelname, float startframe, float endframe, float framerate)", "Spawns a self-animating sprite")},// (DP_SV_EFFECT)
//DP_TE_BLOOD
	{"te_blood",		PF_te_blooddp,		0,		0,		0,		405,	"void(vector org, vector dir, float count)"},// #405 te_blood
//DP_TE_BLOODSHOWER
	{"te_bloodshower",	PF_te_bloodshower,	0,		0,		0,		406,	"void(vector mincorner, vector maxcorner, float explosionspeed, float howmany)"},// (DP_TE_BLOODSHOWER)
//DP_TE_EXPLOSIONRGB
	{"te_explosionrgb",	PF_te_explosionrgb,	0,		0,		0,		407,	"void(vector org, vector color)"},// (DP_TE_EXPLOSIONRGB)
//DP_TE_PARTICLECUBE
	{"te_particlecube",	PF_te_particlecube,	0,		0,		0,		408,	"void(vector mincorner, vector maxcorner, vector vel, float howmany, float color, float gravityflag, float randomveljitter)"},// (DP_TE_PARTICLECUBE)
//DP_TE_PARTICLERAIN
	{"te_particlerain",	PF_te_particlerain,	0,		0,		0,		409,	"void(vector mincorner, vector maxcorner, vector vel, float howmany, float color)"},// (DP_TE_PARTICLERAIN)
//DP_TE_PARTICLESNOW
	{"te_particlesnow",	PF_te_particlesnow,	0,		0,		0,		410,	"void(vector mincorner, vector maxcorner, vector vel, float howmany, float color)"},// (DP_TE_PARTICLESNOW)
//DP_TE_SPARK
	{"te_spark",		PF_te_spark,		0,		0,		0,		411,	"void(vector org, vector vel, float howmany)"},// (DP_TE_SPARK)
//DP_TE_QUADEFFECTS1
	{"te_gunshotquad",	PF_te_gunshotquad,	0,		0,		0,		412,	"void(vector org)"},// (DP_TE_QUADEFFECTS1)
	{"te_spikequad",	PF_te_spikequad,	0,		0,		0,		413,	"void(vector org)"},// (DP_TE_QUADEFFECTS1)
	{"te_superspikequad",PF_te_superspikequad,0,	0,		0,		414,	"void(vector org)"},// (DP_TE_QUADEFFECTS1)
	{"te_explosionquad",PF_te_explosionquad,0,		0,		0,		415,	"void(vector org)"},// (DP_TE_QUADEFFECTS1)
//DP_TE_SMALLFLASH
	{"te_smallflash",	PF_te_smallflash,	0,		0,		0,		416,	"void(vector org)"},// (DP_TE_SMALLFLASH)
//DP_TE_CUSTOMFLASH
	{"te_customflash",	PF_te_customflash,	0,		0,		0,		417,	"void(vector org, float radius, float lifetime, vector color)"},// (DP_TE_CUSTOMFLASH)

//DP_TE_STANDARDEFFECTBUILTINS
	{"te_gunshot",		PF_te_gunshot,		0,		0,		0,		418,	"void(vector org, optional float count)"},// #418 te_gunshot
	{"te_spike",		PF_te_spike,		0,		0,		0,		419,	"void(vector org)"},// #419 te_spike
	{"te_superspike",	PF_te_superspike,	0,		0,		0,		420,	"void(vector org)"},// #420 te_superspike
	{"te_explosion",	PF_te_explosion,	0,		0,		0,		421,	"void(vector org)"},// #421 te_explosion
	{"te_tarexplosion",	PF_te_tarexplosion,	0,		0,		0,		422,	"void(vector org)"},// #422 te_tarexplosion
	{"te_wizspike",		PF_te_wizspike,		0,		0,		0,		423,	"void(vector org)"},// #423 te_wizspike
	{"te_knightspike",	PF_te_knightspike,	0,		0,		0,		424,	"void(vector org)"},// #424 te_knightspike
	{"te_lavasplash",	PF_te_lavasplash,	0,		0,		0,		425,	"void(vector org)"},// #425 te_lavasplash
	{"te_teleport",		PF_te_teleport,		0,		0,		0,		426,	"void(vector org)"},// #426 te_teleport
	{"te_explosion2",	PF_te_explosion2,	0,		0,		0,		427,	"void(vector org, float color, float colorlength)"},// #427 te_explosion2
	{"te_lightning1",	PF_te_lightning1,	0,		0,		0,		428,	"void(entity own, vector start, vector end)"},// #428 te_lightning1
	{"te_lightning2",	PF_te_lightning2,	0,		0,		0,		429,	"void(entity own, vector start, vector end)"},// #429 te_lightning2
	{"te_lightning3",	PF_te_lightning3,	0,		0,		0,		430,	"void(entity own, vector start, vector end)"},// #430 te_lightning3
	{"te_beam",			PF_te_beam,			0,		0,		0,		431,	"void(entity own, vector start, vector end)"},// #431 te_beam
	{"vectorvectors",	PF_vectorvectors,	0,		0,		0,		432,	"void(vector dir)"},// (DP_QC_VECTORVECTORS)
	{"te_plasmaburn",	PF_te_plasmaburn,	0,		0,		0,		433,	"void(vector org)"},// (DP_TE_PLASMABURN)
	{"getsurfacenumpoints",PF_getsurfacenumpoints,0,0,		0,		434,	"float(entity e, float s)"},// (DP_QC_GETSURFACE)
	{"getsurfacepoint",PF_getsurfacepoint,	0,		0,		0,		435,	"vector(entity e, float s, float n)"},// (DP_QC_GETSURFACE)
	{"getsurfacenormal",PF_getsurfacenormal,0,		0,		0,		436,	"vector(entity e, float s)"},// (DP_QC_GETSURFACE)
	{"getsurfacetexture",PF_getsurfacetexture,0,	0,		0,		437,	"string(entity e, float s)"},// (DP_QC_GETSURFACE)
	{"getsurfacenearpoint",PF_getsurfacenearpoint,0,0,		0,		438,	"float(entity e, vector p)"},// (DP_QC_GETSURFACE)
	{"getsurfaceclippedpoint",PF_getsurfaceclippedpoint,0,0,0,		439,	"vector(entity e, float s, vector p)"},// (DP_QC_GETSURFACE)

#ifndef SERVERONLY
	//begin menu-only
	{"buf_create",		PF_Fixme,			0,		0,		0,		440,	"strbuf()"},//DP_QC_STRINGBUFFERS
	{"buf_del",			PF_Fixme,			0,		0,		0,		441,	"void(strbuf bufhandle)"},//DP_QC_STRINGBUFFERS
	{"buf_getsize",		PF_Fixme,			0,		0,		0,		442,	"float(strbuf bufhandle)"},//DP_QC_STRINGBUFFERS
	{"buf_copy",		PF_Fixme,			0,		0,		0,		443,	"void(strbuf bufhandle_from, float bufhandle_to)"},//DP_QC_STRINGBUFFERS
	{"buf_sort",		PF_Fixme,			0,		0,		0,		444,	"void(strbuf bufhandle, float sortprefixlen, float backward)"},//DP_QC_STRINGBUFFERS
	{"buf_implode",		PF_Fixme,			0,		0,		0,		445,	"string(strbuf bufhandle, string glue)"},//DP_QC_STRINGBUFFERS
	{"bufstr_get",		PF_Fixme,			0,		0,		0,		446,	"string(strbuf bufhandle, float string_index)"},//DP_QC_STRINGBUFFERS
	{"bufstr_set",		PF_Fixme,			0,		0,		0,		447,	"void(strbuf bufhandle, float string_index, string str)"},//DP_QC_STRINGBUFFERS
	{"bufstr_add",		PF_Fixme,			0,		0,		0,		448,	"float(strbuf bufhandle, string str, float order)"},//DP_QC_STRINGBUFFERS
	{"bufstr_free",		PF_Fixme,			0,		0,		0,		449,	"void(strbuf bufhandle, float string_index)"},//DP_QC_STRINGBUFFERS
	{"iscachedpic",		PF_Fixme,			0,		0,		0,		451,	"float(string name)"},// (EXT_CSQC)
	{"precache_pic",	PF_Fixme,			0,		0,		0,		452,	"string(string name, optional float trywad)"},// (EXT_CSQC)
	{"freepic",			PF_Fixme,			0,		0,		0,		453,	"void(string name)"},// (EXT_CSQC)
	{"drawcharacter",	PF_Fixme,			0,		0,		0,		454,	"float(vector position, float character, vector scale, vector rgb, float alpha, optional float flag)"},// (EXT_CSQC, [EXT_CSQC_???])
	{"drawrawstring",	PF_Fixme,			0,		0,		0,		455,	"float(vector position, string text, vector scale, vector rgb, float alpha, optional float flag)"},// (EXT_CSQC, [EXT_CSQC_???])
	{"drawpic",			PF_Fixme,			0,		0,		0,		456,	"float(vector position, string pic, vector size, vector rgb, float alpha, optional float flag)"},// (EXT_CSQC, [EXT_CSQC_???])
	{"drawfill",		PF_Fixme,			0,		0,		0,		457,	"float(vector position, vector size, vector rgb, float alpha, optional float flag)"},// (EXT_CSQC, [EXT_CSQC_???])
	{"drawsetcliparea",	PF_Fixme,			0,		0,		0,		458,	"void(float x, float y, float width, float height)"},// (EXT_CSQC_???)
	{"drawresetcliparea",PF_Fixme,			0,		0,		0,		459,	"void(void)"},// (EXT_CSQC_???)
	{"drawgetimagesize",PF_Fixme,			0,		0,		0,		460,	"vector(string picname)"},// (EXT_CSQC)
	{"cin_open",		PF_Fixme,			0,		0,		0,		461,	"float(string file, string id)" STUB},
	{"cin_close",		PF_Fixme,			0,		0,		0,		462,	"void(string id)" STUB},
	{"cin_setstate",	PF_Fixme,			0,		0,		0,		463,	"void(string id, float newstate)" STUB},
	{"cin_getstate",	PF_Fixme,			0,		0,		0,		464,	"float(string id)" STUB},
	{"cin_restart",		PF_Fixme,			0,		0,		0, 		465,	"void(string file)" STUB},
	{"drawline",		PF_Fixme,			0,		0,		0,		466,	"void(float width, vector pos1, vector pos2)"},// (EXT_CSQC)
	{"drawstring",		PF_Fixme,			0,		0,		0,		467,	"float(vector position, string text, vector scale, vector rgb, float alpha, float flag)"},// #326
	{"stringwidth",		PF_Fixme,			0,		0,		0,		468,	"float(string text, float usecolours, optional vector fontsize)"},// EXT_CSQC_'DARKPLACES'
	{"drawsubpic",		PF_Fixme,			0,		0,		0,		469,	"void(vector pos, vector sz, string pic, vector srcpos, vector srcsz, vector rgb, float alpha, float flag)"},// #328 EXT_CSQC_'DARKPLACES'
	//end menu-only
#endif
	//begin non-menu
	{"clientcommand",	PF_clientcommand,	0,		0,		0,		440,	"void(entity e, string s)"},// (KRIMZON_SV_PARSECLIENTCOMMAND)
	{"tokenize",		PF_Tokenize,		0,		0,		0,		441,	"float(string s)"},// (KRIMZON_SV_PARSECLIENTCOMMAND)
	{"argv",			PF_ArgV,			0,		0,		0,		442,	"string(float n)"},// (KRIMZON_SV_PARSECLIENTCOMMAND
	{"setattachment",	PF_setattachment,	0,		0,		0,		443,	"void(entity e, entity tagentity, string tagname)"},// (DP_GFX_QUAKE3MODELTAGS)
	{"search_begin",	PF_search_begin,	0,		0,		0,		444,	D("searchhandle(string pattern, float caseinsensitive, float quiet)", "initiate a filesystem scan based upon filenames. Be sure to call search_end on the returned handle.")},
	{"search_end",		PF_search_end,		0,		0,		0,		445,	"void(searchhandle handle)"},
	{"search_getsize",	PF_search_getsize,	0,		0,		0,		446,	D("float(searchhandle handle)", "Retrieves the number of files that were found.")},
	{"search_getfilename", PF_search_getfilename,0,	0,		0,		447,	D("string(searchhandle handle, float num)", "Retrieves name of one of the files that was found by the initial search.")},
	{"search_getfilesize", PF_search_getfilesize,0,	0,		0,		0,		D("float(searchhandle handle, float num)", "Retrieves the size of one of the files that was found by the initial search.")},
	{"search_getfilemtime", PF_search_getfilemtime,0,0,		0,		0,		D("string(searchhandle handle, float num)", "Retrieves modification time of one of the files.")},
	{"cvar_string",		PF_cvar_string,		0,		0,		0,		448,	"string(string cvarname)"},//DP_QC_CVAR_STRING
	{"findflags",		PF_FindFlags,		0,		0,		0,		449,	"entity(entity start, .float fld, float match)"},//DP_QC_FINDFLAGS
	{"findchainflags",	PF_sv_findchainflags,0,		0,		0,		450,	"entity(.float fld, float match)"},//DP_QC_FINDCHAINFLAGS
	{"gettagindex",		PF_gettagindex,		0,		0,		0,		451,	"float(entity ent, string tagname)"},// (DP_MD3_TAGSINFO)
	{"gettaginfo",		PF_gettaginfo,		0,		0,		0,		452,	D("vector(entity ent, float tagindex)", "Obtains the current worldspace position+orientation of the bone or tag from the given entity. The return value is the world coord, v_forward, v_right, v_up are also set according to the bone/tag's orientation.")},// (DP_MD3_TAGSINFO)
	{"dropclient",		PF_dropclient,		0,		0,		0,		453,	"void(entity player)"},//DP_SV_BOTCLIENT
	{"spawnclient",		PF_spawnclient,		0,		0,		0,		454,	"entity()"},//DP_SV_BOTCLIENT
	{"clienttype",		PF_clienttype,		0,		0,		0,		455,	"float(entity client)"},//botclient
	{"WriteUnterminatedString",PF_WriteString2,0,	0,		0,		456,	"void(float target, string str)"},	//writestring but without the null terminator. makes things a little nicer.
	{"te_flamejet",		PF_te_flamejet,		0,		0,		0,		457,	"void(vector org, vector vel, float howmany)"},//DP_TE_FLAMEJET
//	{"undefined",		PF_Fixme,			0,		0,		0,		458,	""},
	{"edict_num",		PF_edict_for_num,	0,		0,		0,		459,	"entity(float entnum)"},//DP_QC_EDICT_NUM
	{"buf_create",		PF_buf_create,		0,		0,		0,		460,	"strbuf()"},//DP_QC_STRINGBUFFERS
	{"buf_del",			PF_buf_del,			0,		0,		0,		461,	"void(strbuf bufhandle)"},//DP_QC_STRINGBUFFERS
	{"buf_getsize",		PF_buf_getsize,		0,		0,		0,		462,	"float(strbuf bufhandle)"},//DP_QC_STRINGBUFFERS
	{"buf_copy",		PF_buf_copy,		0,		0,		0,		463,	"void(strbuf bufhandle_from, strbuf bufhandle_to)"},//DP_QC_STRINGBUFFERS
	{"buf_sort",		PF_buf_sort,		0,		0,		0,		464,	"void(strbuf bufhandle, float sortprefixlen, float backward)"},//DP_QC_STRINGBUFFERS
	{"buf_implode",		PF_buf_implode,		0,		0,		0,		465,	"string(strbuf bufhandle, string glue)"},//DP_QC_STRINGBUFFERS
	{"bufstr_get",		PF_bufstr_get,		0,		0,		0,		466,	"string(strbuf bufhandle, float string_index)"},//DP_QC_STRINGBUFFERS
	{"bufstr_set",		PF_bufstr_set,		0,		0,		0,		467,	"void(strbuf bufhandle, float string_index, string str)"},//DP_QC_STRINGBUFFERS
	{"bufstr_add",		PF_bufstr_add,		0,		0,		0,		468,	"float(strbuf bufhandle, string str, float order)"},//DP_QC_STRINGBUFFERS
	{"bufstr_free",		PF_bufstr_free,		0,		0,		0,		469,	"void(strbuf bufhandle, float string_index)"},//DP_QC_STRINGBUFFERS
	//end non-menu

//	{"undefined",		PF_Fixme,			0,		0,		0,		470,	""},
//MENU VM BUILTINS SHARE THE BELOW BUILTINS
	{"asin",			PF_asin,			0,		0,		0,		471,	"float(float s)"},//DP_QC_ASINACOSATANATAN2TAN
	{"acos",			PF_acos,			0,		0,		0,		472,	"float(float c)"},//DP_QC_ASINACOSATANATAN2TAN
	{"atan",			PF_atan,			0,		0,		0,		473,	"float(float t)"},//DP_QC_ASINACOSATANATAN2TAN
	{"atan2",			PF_atan2,			0,		0,		0,		474,	"float(float c, float s)"},//DP_QC_ASINACOSATANATAN2TAN
	{"tan",				PF_tan,				0,		0,		0,		475,	D("float(float a)", "Forgive me father, for I have a sunbed and I'm not afraid to use it.")},//DP_QC_ASINACOSATANATAN2TAN
	{"strlennocol",		PF_strlennocol,		0,		0,		0,		476,	D("float(string s)", "Returns the number of characters in the string after any colour codes or other markup has been parsed.")},//DP_QC_STRINGCOLORFUNCTIONS
	{"strdecolorize",	PF_strdecolorize,	0,		0,		0,		477,	D("string(string s)", "Flattens any markup/colours, removing them from the string.")},//DP_QC_STRINGCOLORFUNCTIONS
	{"strftime",		PF_strftime,		0,		0,		0,		478,	"string(float uselocaltime, string format, ...)"},	//DP_QC_STRFTIME
	{"tokenizebyseparator",PF_tokenizebyseparator,0,0,		0,		479,	"float(string s, string separator1, ...)"},	//DP_QC_TOKENIZEBYSEPARATOR
	{"strtolower",		PF_strtolower,		0,		0,		0,		480,	"string(string s)"},	//DP_QC_STRING_CASE_FUNCTIONS
	{"strtoupper",		PF_strtoupper,		0,		0,		0,		481,	"string(string s)"},	//DP_QC_STRING_CASE_FUNCTIONS
	{"cvar_defstring",	PF_cvar_defstring,	0,		0,		0,		482,	"string(string s)"},	//DP_QC_CVAR_DEFSTRING
	{"pointsound",		PF_pointsound,		0,		0,		0,		483,	"void(vector origin, string sample, float volume, float attenuation)"},//DP_SV_POINTSOUND
	{"strreplace",		PF_strreplace,		0,		0,		0,		484,	"string(string search, string replace, string subject)"},//DP_QC_STRREPLACE
	{"strireplace",		PF_strireplace,		0,		0,		0,		485,	"string(string search, string replace, string subject)"},//DP_QC_STRREPLACE
	{"getsurfacepointattribute",PF_getsurfacepointattribute,0,0,0,	486,	"vector(entity e, float s, float n, float a)"},//DP_QC_GETSURFACEPOINTATTRIBUTE
	{"gecko_create",	PF_Fixme,			0,		0,		0,		487,	D("float(string name)", "Create a new 'browser tab' shader with the specified name that can then be drawn via drawpic (shader should not already exist - including from map/model textures or disk). In order to function correctly, this builtin depends upon external plugins being available. Use gecko_navigate to navigate it to a page of your choosing.")},//DP_GECKO_SUPPORT
	{"gecko_destroy",	PF_Fixme,			0,		0,		0,		488,	D("void(string name)", "Destroy a shader.")},//DP_GECKO_SUPPORT
	{"gecko_navigate",	PF_Fixme,			0,		0,		0,		489,	D("void(string name, string URI)", "Sends a command to the media decoder attached to the specified shader. In the case of a browser decoder, this changes the url that the browser displays. 'cmd:[un]focus' will tell the decoder that it has focus.")},//DP_GECKO_SUPPORT
	{"gecko_keyevent",	PF_Fixme,			0,		0,		0,		490,	D("float(string name, float key, float eventtype)", "Send a key event to a media decoder. This applies only to interactive decoders like browsers.")},//DP_GECKO_SUPPORT
	{"gecko_mousemove",	PF_Fixme,			0,		0,		0,		491,	D("void(string name, float x, float y)", "Sets a media decoder shader's mouse position. Values should be 0-1.")},//DP_GECKO_SUPPORT
	{"gecko_resize",	PF_Fixme,			0,		0,		0,		492,	D("void(string name, float w, float h)", "Request to resize a media decoder.")},//DP_GECKO_SUPPORT
	{"gecko_get_texture_extent",PF_Fixme,	0,		0,		0,		493,	D("vector(string name)", "Retrieves a media decoder current image pixel sizes.")},//DP_GECKO_SUPPORT
	{"gecko_getproperty",PF_Fixme,			0,		0,		0,		0,		D("string(string shadname, string propname)", "Queries the media decoder (especially browser ones) for decoder-specific properties. The cef plugin recognises url, title, status.")},
	{"cin_open",		PF_Fixme,			0,		0,		0,		0,		D("float(string file, string id)", NULL)},
	{"cin_close",		PF_Fixme,			0,		0,		0,		0,		D("void(string id)", NULL)},
	{"cin_setstate",	PF_Fixme,			0,		0,		0,		0,		D("void(string id, float newstate)", NULL)},
	{"cin_getstate",	PF_Fixme,			0,		0,		0,		0,		D("float(string id)", NULL)},
	{"cin_restart",		PF_Fixme,			0,		0,		0, 		0,		D("void(string file)", NULL)},

	{"crc16",			PF_crc16,			0,		0,		0,		494,	"float(float caseinsensitive, string s, ...)"},//DP_QC_CRC16
	{"cvar_type",		PF_cvar_type,		0,		0,		0,		495,	"float(string name)"},//DP_QC_CVAR_TYPE
	{"numentityfields",	PF_numentityfields,	0,		0,		0,		496,	D("float()", "Gives the number of named entity fields. Note that this is not the size of an entity, but rather just the number of unique names (ie: vectors use 4 names rather than 3).")},//DP_QC_ENTITYDATA
	{"findentityfield",	PF_findentityfield,	0,		0,		0,		0,		D("float(string fieldname)", "Find a field index by name.")},
	{"entityfieldref",	PF_entityfieldref,	0,		0,		0,		0,		D("typedef .__variant field_t;\nfield_t(float fieldnum)", "Returns a field value that can be directly used to read entity fields. Be sure to validate the type with entityfieldtype before using.")},//DP_QC_ENTITYDATA
	{"entityfieldname",	PF_entityfieldname,	0,		0,		0,		497,	D("string(float fieldnum)", "Retrieves the name of the given entity field.")},//DP_QC_ENTITYDATA
	{"entityfieldtype",	PF_entityfieldtype,	0,		0,		0,		498,	D("float(float fieldnum)", "Provides information about the type of the field specified by the field num. Returns one of the EV_ values.")},//DP_QC_ENTITYDATA
	{"getentityfieldstring",PF_getentityfieldstring,0,0,	0,		499,	"string(float fieldnum, entity ent)"},//DP_QC_ENTITYDATA
	{"putentityfieldstring",PF_putentityfieldstring,0,0,	0,		500,	"float(float fieldnum, entity ent, string s)"},//DP_QC_ENTITYDATA
	{"WritePicture",	PF_WritePicture,	0,		0,		0,		501,	D("void(float to, string s, float sz)", "Encodes the named image across the network as-is adhering to some size limit. In FTE, this simply writes the string and is equivelent to writestring and sz is ignored. WritePicture should be paired with ReadPicture in csqc.")},//DP_SV_WRITEPICTURE
	{"ReadPicture",		PF_Fixme,			0,		0,		0,		501,	D("string()", "Reads a picture that was written by ReadPicture, and returns a name that can be used in drawpic and other 2d drawing functions. In FTE, this acts as a readstring-with-downloadcheck - the image will appear normally once it has been downloaded, but its size may be incorrect until then.")},//DP_SV_WRITEPICTURE
	{"boxparticles",	PF_Fixme,			0,		0,		0,		502,	"void(float effectindex, entity own, vector org_from, vector org_to, vector dir_from, vector dir_to, float countmultiplier, optional float flags)"},
	{"whichpack",		PF_whichpack,		0,		0,		0,		503,	D("string(string filename, optional float makereferenced)", "Returns the pak file name that contains the file specified. progs/player.mdl will generally return something like 'pak0.pak'. If makereferenced is true, clients will automatically be told that the returned package should be pre-downloaded and used, even if allow_download_refpackages is not set.")},//DP_QC_WHICHPACK
	{"getentity",		PF_Fixme,			0,		0,		0,		504,	D("__variant(float entnum, float fieldnum)", "Looks up fields from non-csqc-visible entities. The entity will need to be within the player's pvs. fieldnum should be one of the GE_ constants.")},//DP_CSQC_QUERYRENDERENTITY
//	{"undefined",		PF_Fixme,			0,		0,		0,		505,	""},
//	{"undefined",		PF_Fixme,			0,		0,		0,		506,	""},
//	{"undefined",		PF_Fixme,			0,		0,		0,		507,	""},
//	{"undefined",		PF_Fixme,			0,		0,		0,		508,	""},
//	{"undefined",		PF_Fixme,			0,		0,		0,		509,	""},
	{"uri_escape",		PF_uri_escape,		0,		0,		0,		510,	"string(string in)"},//DP_QC_URI_ESCAPE
	{"uri_unescape",	PF_uri_unescape,	0,		0,		0,		511,	"string(string in)"},//DP_QC_URI_ESCAPE
	{"num_for_edict",	PF_num_for_edict,	0,		0,		0,		512,	"float(entity ent)"},//DP_QC_NUM_FOR_EDICT
	{"uri_get",			PF_uri_get,			0,		0,		0,		513,	D("#define uri_post uri_get\nfloat(string uril, float id, optional string postmimetype, optional string postdata)", "uri_get() gets content from an URL and calls a callback \"uri_get_callback\" with it set as string; an unique ID of the transfer is returned\nreturns 1 on success, and then calls the callback with the ID, 0 or the HTTP status code, and the received data in a string\nFor a POST request, you will typically want the postmimetype set to application/x-www-form-urlencoded.\nFor a GET request, omit the mime+data entirely.\nConsult your webserver/php/etc documentation for best-practise.")},//DP_QC_URI_GET
	{"uri_post",		PF_uri_get,			0,		0,		0,		513,	D("float(string uril, float id, optional string postmimetype, optional string postdata)", "uri_get() gets content from an URL and calls a callback \"uri_get_callback\" with it set as string; an unique ID of the transfer is returned\nreturns 1 on success, and then calls the callback with the ID, 0 or the HTTP status code, and the received data in a string"), true},//DP_QC_URI_POST
	{"tokenize_console",PF_tokenize_console,0,		0,		0,		514,	D("float(string str)", "Tokenize a string exactly as the console's tokenizer would do so. The regular tokenize builtin became bastardized for convienient string parsing, which resulted in a large disparity that can be exploited to bypass checks implemented in a naive SV_ParseClientCommand function, therefore you can use this builtin to make sure it exactly matches.")},
	{"argv_start_index",PF_argv_start_index,0,		0,		0,		515,	D("float(float idx)", "Returns the character index that the tokenized arg started at.")},
	{"argv_end_index",	PF_argv_end_index,	0,		0,		0,		516,	D("float(float idx)", "Returns the character index that the tokenized arg stopped at.")},
	{"buf_cvarlist",	PF_buf_cvarlist,	0,		0,		0,		517,	"void(strbuf strbuf, string pattern, string antipattern)"},
	{"cvar_description",PF_cvar_description,0,		0,		0,		518,	D("string(string cvarname)", "Retrieves the description of a cvar, which might be useful for tooltips or help files. This may still not be useful.")},
	{"gettime",			PF_gettime,			0,		0,		0,		519,	"float(optional float timetype)"},
	{"keynumtostring_omgwtf",PF_Fixme,		0,		0,		0,		520,	"string(float keynum)"},	//excessive third version in dp's csqc.
	{"findkeysforcommand",PF_Fixme,			0,		0,		0,		521,	D("string(string command, optional float bindmap)", "Returns a list of keycodes that perform the given console command in a format that can only be parsed via tokenize (NOT tokenize_console). This only and always returns two values - if only one key is actually bound, -1 will be returned. The bindmap argument is listed for compatibility with dp-specific defs, but is ignored in FTE.")},
	{"findkeysforcommandex",PF_Fixme,		0,		0,		0,		0,		D("string(string command, optional float bindmap)", "Returns a list of key bindings in keyname format instead of keynums. Use tokenize to parse. This list may contain modifiers. May return large numbers of keys.")},
//	{"initparticlespawner",PF_Fixme,		0,		0,		0,		522,	"void(float max_themes)"},
//	{"resetparticle",	PF_Fixme,			0,		0,		0,		523,	"void()"},
//	{"particletheme",	PF_Fixme,			0,		0,		0,		524,	"void(float theme)"},
//	{"particlethemesave",PF_Fixme,			0,		0,		0,		525,	"void(float theme)"},
//	{"particlethemefree",PF_Fixme,			0,		0,		0,		526,	"void()"},
//	{"particle",		PF_Fixme,			0,		0,		0,		527,	"float(vector org, vector vel, optional float theme)"},
//	{"delayedparticle",	PF_Fixme,			0,		0,		0,		528,	"float(vector org, vector vel, float delay, float collisiondelay, optional float theme)"},
	{"loadfromdata",	PF_loadfromdata,	0,		0,		0,		529,	D("void(string s)", "Reads a set of entities from the given string. This string should have the same format as a .ent file or a saved game. Entities will be spawned as required. If you need to see the entities that were created, you should use parseentitydata instead.")},
	{"loadfromfile",	PF_loadfromfile,	0,		0,		0,		530,	D("void(string s)", "Reads a set of entities from the named file. This file should have the same format as a .ent file or a saved game. Entities will be spawned as required. If you need to see the entities that were created, you should use parseentitydata instead.")},
	{"setpause",		PF_setpause,		0,		0,		0,		531,	D("void(float pause)", "Sets whether the server should or should not be paused. This does not affect auto-paused things like when the console is down.")},
	//end dp extras
	//begin mvdsv extras
	{"precache_vwep_model",PF_precache_vwep_model,0,0,		0,		532,	"float(string mname)"},
	//end mvdsv extras
	//restart dp extras
	{"log",				PF_Logarithm,		0,		0,		0,		532,	D("float(float v, optional float base)", "Determines the logarithm of the input value according to the specified base. This can be used to calculate how much something was shifted by.")},
	{"soundupdate",		PF_Fixme,			0,		0,		0,		0,		D("float(entity e, float channel, string newsample, float volume, float attenuation, float pitchpct, float flags, float timeoffset)", "Changes the properties of the current sound being played on the given entity channel. newsample may be empty, and will be ignored in this case. timeoffset is relative to the current position (subtract the result of getsoundtime for absolute positions). Negative volume can be used to stop the sound. Return value is a fractional value based upon the number of audio devices that could be updated - test against TRUE rather than non-zero.")},
	{"getsoundtime",	PF_Ignore,			0,		0,		0,		533,	D("float(entity e, float channel)", "Returns the current playback time of the sample on the given entity's channel. Beware CHAN_AUTO (in csqc, channels are not limited by network protocol).")},
	{"soundlength",		PF_Ignore,			0,		0,		0,		534,	D("float(string sample)", "Provides a way to query the duration of a sound sample, allowing you to set up a timer to chain samples.")},
	{"buf_loadfile",	PF_buf_loadfile,	0,		0,		0,		535,	D("float(string filename, strbuf bufhandle)", "Appends the named file into a string buffer (which must have been created in advance). The return value merely says whether the file was readable.")},
	{"buf_writefile",	PF_buf_writefile,	0,		0,		0,		536,	D("float(filestream filehandle, strbuf bufhandle, optional float startpos, optional float numstrings)", "Writes the contents of a string buffer onto the end of the supplied filehandle (you must have already used fopen). Additional optional arguments permit you to constrain the writes to a subsection of the stringbuffer.")},
//	{"bufstr_find",		PF_Fixme,			0,		0,		0,		537,	"float(float bufhandle, string match, float matchrule, float startpos)"},
//	{"matchpattern",	PF_Fixme,			0,		0,		0,		538,	"float(string s, string pattern, float matchrule)"},
//	{"undefined",		PF_Fixme,			0,		0,		0,		539,	""},

#ifdef USERBE
	{"physics_enable",	PF_physics_enable,	0,		0,		0,		540,	D("void(entity e, float physics_enabled)", "Enable or disable the physics attached to a MOVETYPE_PHYSICS entity. Entities which have been disabled in this way will stop taking so much cpu time.")},
	{"physics_addforce",PF_physics_addforce,0,		0,		0,		541,	D("void(entity e, vector force, vector relative_ofs)", "Apply some impulse directional force upon a MOVETYPE_PHYSICS entity.")},
	{"physics_addtorque",PF_physics_addtorque,0,	0,		0,		542,	D("void(entity e, vector torque)", "Apply some impulse rotational force upon a MOVETYPE_PHYSICS entity.")},
#endif

	{"setkeydest",		PF_Fixme,			0,		0,		0,		601,	"void(float dest)"},
	{"getkeydest",		PF_Fixme,			0,		0,		0,		602,	"float()"},
	{"setmousetarget",	PF_Fixme,			0,		0,		0,		603,	"void(float trg)"},
	{"getmousetarget",	PF_Fixme,			0,		0,		0,		604,	"float()"},
	{"callfunction",	PF_callfunction,	0,		0,		0,		605,	D("void(.../*, string funcname*/)", "Invokes the named function. The function name is always passed as the last parameter and must always be present. The others are passed to the named function as-is")},
	{"writetofile",		PF_writetofile,		0,		0,		0,		606,	D("void(filestream fh, entity e)", "Writes an entity's fields to the named frik_file file handle.")},
	{"isfunction",		PF_isfunction,		0,		0,		0,		607,	D("float(string s)", "Returns true if the named function exists and can be called with the callfunction builtin.")},
	{"getresolution",	PF_Fixme,			0,		0,		0,		608,	D("vector(float vidmode, optional float forfullscreen)", "Supposed to query the driver for supported video modes. FTE does not query drivers in this way, nor would it trust drivers anyway.")},
	{"keynumtostring_menu",PF_Fixme,		0,		0,		0,		609,	"string(float keynum)"},	//third copy of this builtin in dp's csqc.
	{"findkeysforcommand_dp",PF_Fixme,		0,		0,		0,		610,	"string(string command, optional float bindmap)"},
	{"keynumtostring",	PF_Fixme,			0,		0,		0,		609,	D("string(float keynum)", "Converts a qscancode key number into a mostly-human-readable name, matching the bind command.")},	//normal name is for menuqc standard.
	{"findkeysforcommand",PF_Fixme,			0,		0,		0,		610,	"string(string command, optional float bindmap)"},
	{"gethostcachevalue",PF_Fixme,			0,		0,		0,		611,	"float(float type)"},
	{"gethostcachestring",PF_Fixme,			0,		0,		0,		612,	"string(float type, float hostnr)"},
	{"parseentitydata",	PF_parseentitydata,	0,		0,		0,		613,	D("float(entity e, string s, optional float offset)", "Reads a single entity's fields into an already-spawned entity. s should contain field pairs like in a saved game: {\"foo1\" \"bar\" \"foo2\" \"5\"}. Returns <=0 on failure, otherwise returns the offset in the string that was read to.")},
	{"generateentitydata",PF_generateentitydata,0,	0,		0,		0,		D("string(entity e)", "Dumps the entities fields into a string which can later be parsed with parseentitydata.")},
	{"stringtokeynum",	PF_Fixme,			0,		0,		0,		614,	D("float(string key)", "Returns the qscancode of a key from its name. Names are identical to the bind command. ctrl/shift/alt modifiers are ignored.")},
	{"stringtokeynum_menu",	PF_Fixme,		0,		0,		0,		614,	"float(string key)"},
	{"resethostcachemasks",PF_Fixme,		0,		0,		0,		615,	"void()"},
	{"sethostcachemaskstring",PF_Fixme,		0,		0,		0,		616,	"void(float mask, float fld, string str, float op)"},
	{"sethostcachemasknumber",PF_Fixme,		0,		0,		0,		617,	"void(float mask, float fld, float num, float op)"},
	{"resorthostcache",	PF_Fixme,			0,		0,		0,		618,	"void()"},
	{"sethostcachesort",PF_Fixme,			0,		0,		0,		619,	"void(float fld, float descending)"},
	{"refreshhostcache",PF_Fixme,			0,		0,		0,		620,	"void()"},
	{"gethostcachenumber",PF_Fixme,			0,		0,		0,		621,	"float(float fld, float hostnr)"},
	{"gethostcacheindexforkey",PF_Fixme,	0,		0,		0,		622,	"float(string key)"},
	{"addwantedhostcachekey",PF_Fixme,		0,		0,		0,		623,	"void(string key)"},
	{"getextresponse",	PF_Fixme,			0,		0,		0,		624,	"string()"},
	{"netaddress_resolve",PF_netaddress_resolve,0,	0,		0,		625,	"string(string dnsname, optional float defport)"},
//	{"getgamedirinfo",	PF_Fixme,			0,		0,		0,		626,	"string(float n, float prop)"},
	{"sprintf",			PF_sprintf,			0,		0,		0,		627,	"string(string fmt, ...)"},
	{"getsurfacenumtriangles",PF_getsurfacenumtriangles,0,0,0,		628,	"float(entity e, float s)"},
	{"getsurfacetriangle",PF_getsurfacetriangle,0,	0,		0,		629,	"vector(entity e, float s, float n)"},
//	{"setkeybind",		PF_Fixme,			0,		0,		0,		630,	"float(float key, string bind, optional float bindmap)"},
	{"getbindmaps",		PF_Fixme,			0,		0,		0,		631,	"vector()"},
	{"setbindmaps",		PF_Fixme,			0,		0,		0,		632,	"float(vector bm)"},
	{"crypto_getkeyfp",	PF_Fixme,			0,		0,		0,		633,	"string(string addr)"  STUB},
	{"crypto_getidfp",	PF_Fixme,			0,		0,		0,		634,	"string(string addr)"  STUB},
	{"crypto_getencryptlevel",PF_Fixme,		0,		0,		0,		635,	"string(string addr)"  STUB},
	{"crypto_getmykeyfp",PF_Fixme,			0,		0,		0,		636,	"string(string addr)"  STUB},
	{"crypto_getmyidfp",PF_Fixme,			0,		0,		0,		637,	"string(float addr)" STUB},
//	{"VM_CL_RotateMoves",PF_Fixme,			0,		0,		0,		638,	""},
	{"digest_hex",		PF_digest_hex,		0,		0,		0,		639,	"string(string digest, string data, ...)"},
//	{"V_CalcRefdef",	PF_Fixme,			0,		0,		0,		640,	"void(entity e)"},
	{"crypto_getmyidstatus",PF_Fixme,		0,		0,		0,		641,	"float(float i)"	STUB},

	//end dp extras

	{"getrmqeffectsversion",PF_Ignore,		0,		0,		0,		666,	"float()" STUB},
	//don't exceed sizeof(pr_builtin)/sizeof(pr_builtin[0]) (currently 1024) without modifing the size of pr_builtin

	{NULL}
};

static void QCBUILTIN PF_Fixme (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int binum;
	char fname[MAX_QPATH];
	int i;
	qboolean printedheader = false;

	SV_EndRedirect();
	if (!prinst->GetBuiltinCallInfo(prinst, &binum, fname, sizeof(fname)))
	{
		binum = 0;
		strcpy(fname, "?unknown?");
	}

	if (binum)
	for (i = 0; BuiltinList[i].bifunc; i++)
	{
		if (BuiltinList[i].ebfsnum == binum)
		{
			if (!printedheader)
			{
				Con_Printf( "\n"
					"Mod forgot to ensure support for builtin %i:%s\n"
							"Please consult the extensionlist_ssqc command.\n"
							"Possible builtins:\n", binum, fname);
				printedheader = true;
			}
			Con_Printf("%s\n", BuiltinList[i].name);
		}
	}

	Con_Printf("\n");

	if (progstype == PROG_QW && (binum >= 83 && binum < 105))
		prinst->RunError(prinst, "\nBuiltin %i:%s not implemented.\nMods designed for mvdsv may need pr_imitatemvdsv to be enabled.", binum, fname);
	else
		prinst->RunError(prinst, "\nBuiltin %i:%s not implemented.\nMod is not compatible.", binum, fname);
	PR_BIError (prinst, "builtin not implemented");
}
int PR_EnableEBFSBuiltin(const char *name, int binum)
{
	int i;
	for (i = 0;BuiltinList[i].name;i++)
	{
		if (!strcmp(BuiltinList[i].name, name) && BuiltinList[i].bifunc != PF_Fixme)
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

static int PDECL PR_SSQC_MapNamedBuiltin(pubprogfuncs_t *progfuncs, int headercrc, const char *builtinname)
{
	int i, binum;
	for (i = 0;BuiltinList[i].name;i++)
	{
		if (!strcmp(BuiltinList[i].name, builtinname) && BuiltinList[i].bifunc != PF_Fixme)
		{
			for (binum = sizeof(pr_builtin)/sizeof(pr_builtin[0]); --binum; )
			{
				if (pr_builtin[binum] && pr_builtin[binum] != PF_Fixme && BuiltinList[i].bifunc)
					continue;
				pr_builtin[binum] = BuiltinList[i].bifunc;
				return binum;
			}
			Con_Printf("No more builtin slots to allocate for %s\n", builtinname);
			break;
		}
	}
	Con_DPrintf("Unknown ssqc builtin: %s\n", builtinname);
	return 0;
}

void PR_ResetBuiltins(progstype_t type)	//fix all nulls to PF_FIXME and add any extras that have a big number.
{
	int i;

	int builtincount[sizeof(pr_builtin)/sizeof(pr_builtin[0])];

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
#ifdef HEXEN2
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
#endif
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

#ifndef QUAKETC
	if (type == PROG_PREREL)
	{
		pr_builtin[52] = PF_qtSingle_WriteByte;
		pr_builtin[53] = PF_qtSingle_WriteChar;
		pr_builtin[54] = PF_qtSingle_WriteShort;
		pr_builtin[55] = PF_qtSingle_WriteLong;
		pr_builtin[56] = PF_qtSingle_WriteCoord;
		pr_builtin[57] = PF_qtSingle_WriteAngle;
		pr_builtin[58] = PF_qtSingle_WriteString;
		//lack of writeentity is intentional (prerel doesn't have it.

		pr_builtin[59] = PF_qtBroadcast_WriteByte;
		pr_builtin[60] = PF_qtBroadcast_WriteChar;
		pr_builtin[61] = PF_qtBroadcast_WriteShort;
		pr_builtin[62] = PF_qtBroadcast_WriteLong;
		pr_builtin[63] = PF_qtBroadcast_WriteCoord;
		pr_builtin[64] = PF_qtBroadcast_WriteAngle;
		pr_builtin[65] = PF_qtBroadcast_WriteString;
		pr_builtin[66] = PF_qtBroadcast_WriteEntity;
	}
#endif

	if (!pr_compatabilitytest.value)
	{
		for (i = 0; BuiltinList[i].name; i++)
		{
			if (BuiltinList[i].ebfsnum && !BuiltinList[i].obsolete && BuiltinList[i].bifunc != PF_Fixme)
				builtincount[BuiltinList[i].ebfsnum]++;
		}
		for (i = 0; BuiltinList[i].name; i++)
		{
			if (BuiltinList[i].ebfsnum)
			{
				if (pr_builtin[BuiltinList[i].ebfsnum] == PF_Fixme && builtincount[BuiltinList[i].ebfsnum] == (BuiltinList[i].obsolete?0:1))
				{
					pr_builtin[BuiltinList[i].ebfsnum] = BuiltinList[i].bifunc;
//					Con_DPrintf("Enabled %s (%i)\n", BuiltinList[i].name, BuiltinList[i].ebfsnum);
				}
//				else if (pr_builtin[i] != BuiltinList[i].bifunc)
//					Con_DPrintf("Not enabled %s (%i)\n", BuiltinList[i].name, BuiltinList[i].ebfsnum);
			}
		}
	}

	{
		char *builtinmap;
		int binum;
		builtinmap = COM_LoadTempFile("fte_bimap.txt", NULL);
		while(1)
		{
			builtinmap = COM_Parse(builtinmap);
			if (!builtinmap)
				break;
			binum = atoi(com_token);
			builtinmap = COM_Parse(builtinmap);

			for (i = 0; BuiltinList[i].name; i++)
			{
				if (!strcmp(BuiltinList[i].name, com_token) && (BuiltinList[i].bifunc != PF_Fixme||!i))
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
		if (QSG_Extensions[i].queried)
			*QSG_Extensions[i].queried = false;
	}

	if (type == PROG_QW)
	{
		//this conflicts with dp's logarithm builtin.
		PR_EnableEBFSBuiltin("precache_vwep_model",	532);

		if (pr_imitatemvdsv.value>0)	//pretend to be mvdsv for a bit.
		{
			if (
				PR_EnableEBFSBuiltin("executecommand",	83) != 83 ||
				PR_EnableEBFSBuiltin("mvdtokenize",		84) != 84 ||
				PR_EnableEBFSBuiltin("mvdargc",			85) != 85 ||
				PR_EnableEBFSBuiltin("mvdargv",			86) != 86 ||
				PR_EnableEBFSBuiltin("teamfield",		87) != 87 ||
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
				PR_EnableEBFSBuiltin("logtext",			100)!= 100 ||
	//			PR_EnableEBFSBuiltin("redirectcmd",		101)!= 101 ||
				PR_EnableEBFSBuiltin("mvdcalltimeofday",102)!= 102 ||
				PR_EnableEBFSBuiltin("forcedemoframe",	103)!= 103)
				Con_Printf("Failed to register all MVDSV builtins\n");
			else
				Con_Printf("Be aware that MVDSV does not follow standards. Please encourage mod developers to not require pr_imitatemvdsv to be set.\n");
		}
	}
}

void PR_SVExtensionList_f(void)
{
	int i, j;
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
				if (!(Net_PextMask(1, false) & (1<<i)))
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
					if (BuiltinList[bi].bifunc == PF_Fixme)
						continue;	//this builtin is unusable in ssqc. some of them are listed because of menuqc
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
						{
							Con_Printf("^4%s was overridden (builtin: %s#%i)\n", extlist[i].name, BuiltinList[bi].name, BuiltinList[bi].ebfsnum);
							for (j = 0; BuiltinList[j].name; j++)
							{
								if (BuiltinList[j].bifunc == pr_builtin[BuiltinList[bi].ebfsnum])
								{
									Con_Printf("^4%s is currently %s (#%i)\n", extlist[i].name, BuiltinList[j].name, BuiltinList[j].ebfsnum);
									break;
								}
							}
						}
					}
					break;
				}
			}
			if (ebi == extlist[i].numbuiltins)
			{
				if (showflags & SHOW_ACTIVEEXT)
				{
					if (extlist[i].description)
					{
						if (!extlist[i].numbuiltins)
							Con_Printf("^[%s\\tip\\%s^] is supported\n", extlist[i].name, extlist[i].description);
						else
							Con_Printf("^[%s\\tip\\%s^] is currently active\n", extlist[i].name, extlist[i].description);
					}
					else 
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
}

builtin_t *pr_builtins = pr_builtin;
int pr_numbuiltins = sizeof(pr_builtin)/sizeof(pr_builtin[0]);

void PR_RegisterFields(void)	//it's just easier to do it this way.
{
#define comfieldfloat(name,desc) PR_RegisterFieldVar(svprogfuncs, ev_float, #name, (size_t)&((stdentvars_t*)0)->name, -1);
#define comfieldint(name,desc) PR_RegisterFieldVar(svprogfuncs, ev_integer, #name, (size_t)&((stdentvars_t*)0)->name, -1);
#define comfieldvector(name,desc) PR_RegisterFieldVar(svprogfuncs, ev_vector, #name, (size_t)&((stdentvars_t*)0)->name, -1);
#define comfieldentity(name,desc) PR_RegisterFieldVar(svprogfuncs, ev_entity, #name, (size_t)&((stdentvars_t*)0)->name, -1);
#define comfieldstring(name,desc) PR_RegisterFieldVar(svprogfuncs, ev_string, (((size_t)&((stdentvars_t*)0)->name==(size_t)&((stdentvars_t*)0)->message)?"_"#name:#name), (size_t)&((stdentvars_t*)0)->name, -1);
#define comfieldfunction(name, typestr,desc) PR_RegisterFieldVar(svprogfuncs, ev_function, #name, (size_t)&((stdentvars_t*)0)->name, -1);
comqcfields
#undef comfieldfloat
#undef comfieldint
#undef comfieldvector
#undef comfieldentity
#undef comfieldstring
#undef comfieldfunction
#ifdef VM_Q1
#define comfieldfloat(name,desc) PR_RegisterFieldVar(svprogfuncs, ev_float, #name, sizeof(stdentvars_t) + (size_t)&((extentvars_t*)0)->name, -1);
#define comfieldint(name,desc) PR_RegisterFieldVar(svprogfuncs, ev_integer, #name, sizeof(stdentvars_t) + (size_t)&((extentvars_t*)0)->name, -1);
#define comfieldvector(name,desc) PR_RegisterFieldVar(svprogfuncs, ev_vector, #name, sizeof(stdentvars_t) + (size_t)&((extentvars_t*)0)->name, -1);
#define comfieldentity(name,desc) PR_RegisterFieldVar(svprogfuncs, ev_entity, #name, sizeof(stdentvars_t) + (size_t)&((extentvars_t*)0)->name, -1);
#define comfieldstring(name,desc) PR_RegisterFieldVar(svprogfuncs, ev_string, #name, sizeof(stdentvars_t) + (size_t)&((extentvars_t*)0)->name, -1);
#define comfieldfunction(name, typestr,desc) PR_RegisterFieldVar(svprogfuncs, ev_function, #name, sizeof(stdentvars_t) + (size_t)&((extentvars_t*)0)->name, -1);
#else
#define comfieldfloat(ssqcname,desc) PR_RegisterFieldVar(svprogfuncs, ev_float, #ssqcname, (size_t)&((stdentvars_t*)0)->ssqcname, -1);
#define comfieldint(ssqcname,desc) PR_RegisterFieldVar(svprogfuncs, ev_integer, #ssqcname, (size_t)&((stdentvars_t*)0)->ssqcname, -1);
#define comfieldvector(ssqcname,desc) PR_RegisterFieldVar(svprogfuncs, ev_vector, #ssqcname, (size_t)&((stdentvars_t*)0)->ssqcname, -1);
#define comfieldentity(ssqcname,desc) PR_RegisterFieldVar(svprogfuncs, ev_entity, #ssqcname, (size_t)&((stdentvars_t*)0)->ssqcname, -1);
#define comfieldstring(ssqcname,desc) PR_RegisterFieldVar(svprogfuncs, ev_string, #ssqcname, (size_t)&((stdentvars_t*)0)->ssqcname, -1);
#define comfieldfunction(ssqcname, typestr,desc) PR_RegisterFieldVar(svprogfuncs, ev_function, #ssqcname, (size_t)&((stdentvars_t*)0)->ssqcname, -1);
#endif

comextqcfields
svextqcfields

#undef comfieldfloat
#undef comfieldint
#undef comfieldvector
#undef comfieldentity
#undef comfieldstring
#undef comfieldfunction

	//Tell the qc library to split the entity fields each side.
	//the fields above become < 0, the remaining fields specified by the qc stay where the mod specified, as far as possible (with addons at least).
	//this means that custom array offsets still work in mods like ktpro.
	if (pr_fixbrokenqccarrays.ival || pr_imitatemvdsv.ival)
		PR_RegisterFieldVar(svprogfuncs, 0, NULL, 0,0);
}

//targets
#define QW 1
#define NQ 2
#define CS 4
#define MENU 8
#define H2 16
//mere flags
#define FTE 32
#ifdef HEXEN2
#define ALL (QW|NQ|H2|CS|MENU)
#else
#define ALL (QW|NQ|CS|MENU)
#endif
#define CORE
typedef struct
{
	char *name;
	char *type;

	unsigned int module;

	char *desc;

	float value;
	char *valuestr;
	qboolean misc;
} knowndef_t;
#include "cl_master.h"
void Key_PrintQCDefines(vfsfile_t *f);
void PR_DumpPlatform_f(void)
{
#ifdef SERVERONLY
	Con_Printf("This command is not available in dedicated servers, sorry.\n");
#elif defined(NOQCDESCRIPTIONS) && NOQCDESCRIPTIONS > 1
	Con_Printf("This command is not available in this build, sorry.\n");
#else
	//eg: pr_dumpplatform -FFTE -TCS -O csplat

	/*const char *keywords[] =
	{
		"ignore"		//0
		"qwqc",			//qw
		"nqqc",			//nq
		"ssqc"			//qw|nq
		"csqc"			//cs
		"csqwqc",		//cs|qw
		"csnqqc",		//cs|nq
		"gameqc"		//cs|nq|qw
		"menuonly"			//mn
		"mnqwqc",		//mn|qw
		"mnnqqc",		//mn|nq
		"mnssqc"		//mn|qw|nq
		"mncsqc"		//mn|cs
		"mncsqwqc",		//mn|cs|qw
		"mncsnqqc",		//mn|cs|nq
		""		//mn|cs|nq|qw
	};*/

	int idx;
	int i, j;
	int d = 0, nd, k;
	vfsfile_t *f;
	char *fname = "";
	char dbgfname[MAX_OSPATH];
	unsigned int targ = 0;
	qboolean defines = false;
	qboolean accessors = false;
	char *comment;

#undef D
#ifdef NOQCDESCRIPTIONS
	#define D(d) NULL
#else
	#define D(d) d
#endif

	/*this list is here to ensure that the file can be used as a valid initial qc file (ignoring precompiler options)*/
	knowndef_t knowndefs[] =
	{
		{"self",				"entity", QW|NQ|CS|MENU, D("The magic me")},
		{"other",				"entity", QW|NQ|CS,	D("Valid in touch functions, this is the entity that we touched.")},
		{"world",				"entity", QW|NQ|CS,	D("The null entity. Hurrah. Readonly after map spawn time.")},
		{"time",				"float", QW|NQ|CS,	D("The current game time. Stops when paused.")},
		{"cltime",				"float", CS,		D("A local timer that ticks relative to local time regardless of latency, packetloss, or pause.")},
		{"frametime",			"float", QW|NQ|CS,	D("The time since the last physics/render/input frame.")},
		{"player_localentnum",	"float", CS,		D("This is entity number the player is seeing from/spectating, or the player themself, can change mid-map.")},
		{"player_localnum",		"float", CS,		D("The 0-based player index, valid for getplayerkeyvalue calls.")},
		{"maxclients",			"float", CS,		D("Maximum number of player slots on the server.")},
		{"clientcommandframe",	"float", CS,		D("This is the input-frame sequence. frames < clientcommandframe have been sent to the server. frame==clientcommandframe is still being generated and can still change.")},
		{"servercommandframe",	"float", CS,		D("This is the input-frame that was last acknowledged by the server. Input frames greater than this should be applied to the player's entity.")},
		{"newmis",				"entity", QW,		D("A named entity that should be run soon, to reduce the effects of latency.")},
		{"force_retouch",		"float", QW|NQ,		D("If positive, causes all entities to check for triggers.")},
		{"mapname",				"string", QW|NQ|CS,	D("The short name of the map.")},
		{"deathmatch",			"float", NQ},
		{"coop",				"float", NQ},
		{"teamplay",			"float", NQ},
		{"serverflags",			"float", QW|NQ},
		{"total_secrets",		"float", QW|NQ},
		{"total_monsters",		"float", QW|NQ},
		{"found_secrets",		"float", QW|NQ},
		{"killed_monsters",		"float", QW|NQ},
		{"parm1, parm2, parm3, parm4, parm5, parm6, parm7, parm8, parm9, parm10, parm11, parm12, parm13, parm14, parm15, parm16", "float", QW|NQ},
		{"intermission",		"float", CS},
		{"v_forward, v_up, v_right",	"vector", QW|NQ|CS},
		{"view_angles",			"vector", CS,		D("+x=DOWN")},
		{"trace_allsolid, trace_startsolid, trace_fraction",		"float", QW|NQ|CS},
		{"trace_endpos, trace_plane_normal",		"vector", QW|NQ|CS},
		{"trace_plane_dist",	"float", QW|NQ|CS},
		{"trace_ent",			"entity", QW|NQ|CS},
		{"trace_inopen",		"float", QW|NQ|CS},
		{"trace_inwater",		"float", QW|NQ|CS},
		{"input_timelength",	"float", CS},
		{"input_angles",		"vector", CS,	D("+x=DOWN")},
		{"input_movevalues",	"vector", CS},
		{"input_buttons",		"float", CS},
		{"input_impulse",		"float", CS},
		{"msg_entity",			"entity", QW|NQ},
		{"main",				"void()", QW|NQ},
		{"StartFrame",			"void()", QW|NQ},
		{"PlayerPreThink",		"void()", QW|NQ},
		{"PlayerPostThink",		"void()", QW|NQ},
		{"ClientKill",			"void()", QW|NQ},
		{"ClientConnect",		"void()", QW|NQ},
		{"PutClientInServer",	"void()", QW|NQ},
		{"ClientDisconnect",	"void()", QW|NQ},
		{"SetNewParms",			"void()", QW|NQ},
		{"SetChangeParms",		"void()", QW|NQ},
		{"end_sys_globals",		"void", QW|NQ|CS|MENU},


		{"modelindex",			".float", QW|NQ|CS,		D("This is the model precache index for the model that was set on the entity, instead of having to look up the model according to the .model field. Use setmodel to change it.")},
		{"absmin",				".vector", QW|NQ|CS,	D("Set by the engine when the entity is relinked (by setorigin, setsize, or setmodel). This is in world coordinates.")},
		{"absmax",				".vector", QW|NQ|CS,	D("Set by the engine when the entity is relinked (by setorigin, setsize, or setmodel). This is in world coordinates.")},
		{"ltime",				".float", QW|NQ,		D("On MOVETYPE_PUSH entities, this is used as an alternative to the 'time' global, and .nextthink is synced to this instead of time. This allows time to effectively freeze if the entity is blocked, ensuring the think happens when the entity reaches the target point instead of randomly.")},
		{"entnum",				".float", CS,			D("The entity number as its known on the server.")},
		{"drawmask",			".float", CS,			D("Acts as a filter in the addentities call.")},
		{"predraw",				".float()", CS,			D("Called by addentities after the filter and before the entity is actually drawn. Do your interpolation and animation in here. Should return one of the PREDRAW_* constants.")},
		{"lastruntime",			".float", QW,			D("This field used to be used to avoid running an entity multiple times in a single frame due to quakeworld's out-of-order thinks. It is no longer used by FTE due to precision issues, but may still be updated for compatibility reasons.")},
		{"movetype",			".float", QW|NQ|CS,		D("Describes how the entity moves. One of the MOVETYPE_ constants.")},
		{"solid",				".float", QW|NQ|CS,		D("Describes whether the entity is solid or not, and any special properties infered by that. Must be one of the SOLID_ constants")},
		{"origin",				".vector", QW|NQ|CS,	D("The current location of the entity in world space. Inline bsp entities (ie: ones placed by a mapper) will typically have a value of '0 0 0' in their neutral pose, as the geometry is offset from that. It is the reference point of the entity rather than the center of its geometry, for non-bsp models, this is often not a significant distinction.")},
		{"oldorigin",			".vector", QW|NQ|CS,	D("This is often used on players to reset the player back to where they were last frame if they somehow got stuck inside something due to fpu precision. Never change a player's oldorigin field to inside a solid, because that might cause them to become pemanently stuck.")},
		{"velocity",			".vector", QW|NQ|CS,	D("The direction and speed that the entity is moving in world space.")},
		{"angles",				".vector", QW|NQ|CS,	D("The eular angles the entity is facing in, in pitch, yaw, roll order. Due to a legacy bug, mdl/iqm/etc formats use +x=UP, bsp/spr/etc formats use +x=DOWN.")},
		{"avelocity",			".vector", QW|NQ|CS,	D("The amount the entity's angles change by per second. Note that this is direct eular angles, and thus the angular change is non-linear and often just looks buggy if you're changing more than one angle at a time.")},
		{"pmove_flags",			".float", CS},
		{"punchangle",			".vector", NQ},
		{"classname",			".string", QW|NQ|CS,	D("Identifies the class/type of the entity. Useful for debugging, also used for loading, but its value is not otherwise significant to the engine, this leaves the mod free to set it to whatever it wants and randomly test strings for values in whatever inefficient way it chooses fit.")},
		{"renderflags",			".float", CS},
		{"model",				".string", QW|NQ|CS,	D("The model name that was set via setmodel, in theory. Often, this is cleared to null to prevent the engine from being seen by clients while not changing modelindex. This behaviour allows inline models to remain solid yet be invisible.")},
		{"frame",				".float", QW|NQ|CS,		D("The current frame the entity is meant to be displayed in. In CSQC, note the lerpfrac and frame2 fields as well. if it specifies a framegroup, the framegroup will autoanimate in ssqc, but not in csqc.")},
		{"frame1time",			".float", CS,			D("The absolute time into the animation/framegroup specified by .frame.")},
		{"frame2",				".float", CS,			D("The alternative frame. Visible only when lerpfrac is set to 1.")},
		{"frame2time",			".float", CS,			D("The absolute time into the animation/framegroup specified by .frame2.")},
		{"lerpfrac",			".float", CS,			D("If 0, use frame1 only. If 1, use frame2 only. Mix them together for values between.")},
		{"skin",				".float", QW|NQ|CS,		D("The skin index to use. on a bsp entity, setting this to 1 will switch to the 'activated' texture instead. A negative value will be understood as a replacement contents value, so setting it to CONTENTS_WATER will make a movable pool of water.")},
		{"effects",				".float", QW|NQ|CS,		D("Lots of random flags that change random effects. See EF_* constants.")},
		{"mins",				".vector", QW|NQ|CS,	D("The minimum extent of the model (ie: the bottom-left coordinate relative to the entity's origin). Change via setsize. May also be changed by setmodel.")},
		{"maxs",				".vector", QW|NQ|CS,	D("like mins, but in the other direction.")},
		{"size",				".vector", QW|NQ|CS,	D("maxs-mins. Updated when the entity is relinked (by setorigin, setsize, setmodel)")},
		{"touch",				".void()", QW|NQ|CS},
		{"use",					".void()", QW|NQ},
		{"think",				".void()", QW|NQ|CS},
		{"blocked",				".void()", QW|NQ|CS},
		{"nextthink",			".float", QW|NQ|CS,		D("The time at which the entity is next scheduled to fire its think event. For MOVETYPE_PUSH entities, this is relative to that entity's ltime field, for all other entities it is relative to the time gloal.")},

		{"groundentity",		".entity", QW|NQ},
		{"health",				".float", QW|NQ},
		{"frags",				".float", QW|NQ},
		{"weapon",				".float", QW|NQ},
		{"weaponmodel",			".string", QW|NQ},
		{"weaponframe",			".float", QW|NQ},
		{"currentammo",			".float", QW|NQ},
		{"ammo_shells",			".float", QW|NQ},
		{"ammo_nails",			".float", QW|NQ},
		{"ammo_rockets",		".float", QW|NQ},
		{"ammo_cells",			".float", QW|NQ},
		{"items",				".float", QW|NQ},
		{"takedamage",			".float", QW|NQ},

		{"chain",				".entity", QW|NQ|CS},
		{"deadflag",			".float", QW|NQ},
		{"view_ofs",			".vector", QW|NQ},
		{"button0",				".float", QW|NQ},
		{"button1",				".float", QW|NQ},
		{"button2",				".float", QW|NQ},
		{"impulse",				".float", QW|NQ},
		{"fixangle",			".float", QW|NQ},
		{"v_angle",				".vector", QW|NQ, "The angles a player is viewing. +x is DOWN (pitch, yaw, roll)"},
		{"idealpitch",			".float", NQ},
		{"netname",				".string", QW|NQ},
		{"enemy",				".entity", QW|NQ|CS},
		{"flags",				".float", QW|NQ|CS},
		{"colormap",			".float", QW|NQ|CS},
		{"team",				".float", QW|NQ},
		{"max_health",			".float", QW|NQ},
		{"teleport_time",		".float", QW|NQ,	D("While active, prevents the player from using the +back command, also blocks waterjumping.")},
		{"armortype",			".float", QW|NQ},
		{"armorvalue",			".float", QW|NQ},
		{"waterlevel",			".float", QW|NQ},
		{"watertype",			".float", QW|NQ},
		{"ideal_yaw",			".float", QW|NQ},
		{"yaw_speed",			".float", QW|NQ},
		{"aiment",				".entity", QW|NQ},
		{"goalentity",			".entity", QW|NQ},
		{"spawnflags",			".float", QW|NQ},
		{"target",				".string", QW|NQ},
		{"targetname",			".string", QW|NQ},
		{"dmg_take",			".float", QW|NQ},
		{"dmg_save",			".float", QW|NQ},
		{"dmg_inflictor",		".entity", QW|NQ},
		{"owner",				".entity", QW|NQ|CS},
		{"movedir",				".vector", QW|NQ},
		{"message",				".string", QW|NQ},
		{"sounds",				".float", QW|NQ},
		{"noise",				".string", QW|NQ},
		{"noise1",				".string", QW|NQ},
		{"noise2",				".string", QW|NQ},
		{"noise3",				".string", QW|NQ},
		{"end_sys_fields",		"void", QW|NQ|CS|MENU},

		{"time",				"float",	MENU,	D("The current local time. Increases while paused.")},
		{"input_timelength",	"float",	QW|NQ},
		{"input_angles",		"vector",	QW|NQ,	D("+x=DOWN")},
		{"input_movevalues",	"vector",	QW|NQ},
		{"input_buttons",		"float",	QW|NQ},
		{"input_impulse",		"float",	QW|NQ},

		{"input_cursor_screen",			"vector",	CS/*|QW|NQ*/},
		{"input_cursor_trace_start",	"vector",	CS/*|QW|NQ*/},
		{"input_cursor_trace_endpos",	"vector",	CS/*|QW|NQ*/},
		{"input_cursor_trace_entnum",	"float",	CS/*|QW|NQ*/},

		{"trace_endcontents",		"int", QW|NQ|CS},
		{"trace_surfaceflags",		"int", QW|NQ|CS},
//		{"trace_surfacename",		"string", QW|NQ|CS},
		{"trace_brush_id",		"int", QW|NQ|CS},
		{"trace_brush_faceid",	"int", QW|NQ|CS},
		{"trace_surface_id",	"int", QW|NQ|CS, D("1-based. 0 if not known.")},
		{"trace_bone_id",		"int", QW|NQ|CS, D("1-based. 0 if not known. typically needs MOVE_HITMODEL.")},
		{"trace_triangle_id",	"int", QW|NQ|CS, D("1-based. 0 if not known.")},

		{"global_gravitydir",	"vector", QW|NQ|CS,	D("The direction gravity should act in if not otherwise specified per entity."), 0,"'0 0 -1'"},
		{"serverid",			"int", QW|NQ|CS,	D("The unique id of this server within the server cluster.")},

#define comfieldfloat(name,desc) {#name, ".float", FL, D(desc)},
#define comfieldint(name,desc) {#name, ".int", FL, D(desc)},
#define comfieldvector(name,desc) {#name, ".vector", FL, D(desc)},
#define comfieldentity(name,desc) {#name, ".entity", FL, D(desc)},
#define comfieldstring(name,desc) {#name, ".string", FL, D(desc)},
#define comfieldfunction(name,typestr,desc) {#name, typestr, FL, D(desc)},
#define FL QW|NQ
		comqcfields
#undef FL
#define FL QW|NQ|CS
		comextqcfields
#undef FL
#define FL QW|NQ
		svextqcfields
#undef FL
#define FL CS
		csqcextfields
#undef FL
#undef comfieldfloat
#undef comfieldint
#undef comfieldvector
#undef comfieldentity
#undef comfieldstring
#undef comfieldfunction

		{"URI_Get_Callback",		"void(float reqid, float responsecode, string resourcebody)",	QW|NQ|CS|MENU, "Called as an eventual result of the uri_get builtin."},
		{"SpectatorConnect",		"void()", QW|NQ, "Called when a spectator joins the game."},
		{"SpectatorDisconnect",		"void()", QW|NQ, "Called when a spectator disconnects from the game."},
		{"SpectatorThink",			"void()", QW|NQ, "Called each frame for each spectator."},
		{"SV_ParseClientCommand",	"void(string cmd)", QW|NQ, "Provides QC with a way to intercept 'cmd foo' commands from the client. Very handy. Self will be set to the sending client, while the 'cmd' argument can be tokenize()d and each element retrieved via argv(argno). Unrecognised cmds MUST be passed on to the clientcommand builtin."},
		{"SV_ParseClusterEvent",	"void(string dest, string from, string cmd, string info)", QW|NQ, "Part of cluster mode. Handles cross-node events that were sent via clusterevent, on behalf of the named client."},
		{"SV_ParseConnectionlessPacket", "float(string sender, string body)", QW|NQ, "Provides QC with a way to communicate between servers, or with client server browsers. Sender is the sender's ip. Body is the body of the message. You'll need to add your own password/etc support as required. Self is not valid."},
		{"SV_PausedTic",			"void(float pauseduration)", QW|NQ, "For each frame that the server is paused, this function will be called to give the gamecode a chance to unpause the server again. the pauseduration argument says how long the server has been paused for (the time global is frozen and will not increment while paused). Self is not valid."},
		{"SV_ShouldPause",			"float(float newstatus)", QW|NQ, "Called to give the qc a change to block pause/unpause requests. Return false for the pause request to be ignored. newstatus is 1 if the user is trying to pause the game. For the duration of the call, self will be set to the player who tried to pause, or to world if it was triggered by a server-side event."},
		{"SV_RunClientCommand",		"void()", QW|NQ, "Called each time a player movement packet was received from a client. Self is set to the player entity which should be updated, while the input_* globals specify the various properties stored within the input packet. The contents of this function should be somewaht identical to the equivelent function in CSQC, or prediction misses will occur. If you're feeling lazy, you can simply call 'runstandardplayerphysics' after modifying the inputs."},
		{"SV_AddDebugPolygons",		"void()", QW|NQ, "Called each video frame. This is the only place where ssqc is allowed to call the R_BeginPolygon/R_PolygonVertex/R_EndPolygon builtins. This is exclusively for debugging, and will break in anything but single player as it will not be called if the engine is not running both a client and a server."},
		{"SV_PlayerPhysics",		"void()", QW|NQ, "Legacy method to tweak player input that does not reliably work with prediction (prediction WILL break). Mods that care about prediction should use SV_RunClientCommand instead. If pr_no_playerphysics is set to 1, this function will never be called, which will either fix prediction or completely break player movement depending on whether the feature was even useful."},
		{"EndFrame",				"void()", QW|NQ, "Called after non-player entities have been run at the end of the physics frame. Player physics is performed out of order and can/will still occur between EndFrame and BeginFrame."},
		{"SV_CheckRejectConnection","string(string addr, string uinfo, string features) ", QW|NQ, "Called to give the mod a chance to ignore connection requests based upon client protocol support or other properties. Use infoget to read the uinfo and features arguments."},
#ifdef HEXEN2
		{"ClassChangeWeapon",		"void()", H2, "Hexen2 support. Called when cl_playerclass changes. Self is set to the player who is changing class."},
#endif
		/* //mvdsv compat
		{"UserInfo_Changed",		"//void()", QW},
		{"localinfoChanged",		"//void()", QW},
		{"ChatMessage",				"//void()", QW},
		{"UserCmd",					"//void()", QW},
		{"ConsoleCmd",				"//void()", QW},
		*/

		{"CSQC_Init",				"void(float apilevel, string enginename, float engineversion)", CS, "Called at startup. enginename and engineversion are arbitary hints and can take any form. enginename should be consistant between revisions, but this cannot truely be relied upon."},
		{"CSQC_WorldLoaded",		"void()", CS, "Called after model+sound precaches have been executed. Gives a chance for the qc to read the entity lump from the bsp."},
		{"CSQC_Shutdown",			"void()", CS, "Specifies that the csqc is going down. Save your persistant settings here."},
		{"CSQC_UpdateView",			"void(float vwidth, float vheight, float notmenu)", CS, "Called every single video frame. The CSQC is responsible for rendering the entire screen."},
		{"CSQC_UpdateViewLoading",	"void(float vwidth, float vheight, float notmenu)", CS, "Alternative to CSQC_UpdateView, called when the engine thinks there should be a loading screen. If present, will inhibit the engine's normal loading screen, deferring to qc to draw it."},
		{"CSQC_Parse_StuffCmd",		"void(string msg)", CS, "Gives the CSQC a chance to intercept stuffcmds. Use the tokenize builtin to parse the message. Unrecognised commands would normally be localcmded, but its probably better to drop unrecognised stuffcmds completely."},
		{"CSQC_Parse_CenterPrint",	"float(string msg)", CS, "Gives the CSQC a chance to intercept centerprints. Return true if you wish the engine to otherwise ignore the centerprint."},
		{"CSQC_Parse_Damage",		"float(float save, float take, vector inflictororg)", CS, "Called as a result of player.dmg_save or player.dmg_take being set on the server.\nReturn true to completely inhibit the engine's colour shift and damage rolls, allowing you to do your own thing.\nYou can use punch_roll += (normalize(inflictororg-player.origin)*v_right)*(take+save)*autocvar_v_kickroll; as a modifier for the roll angle should the player be hit from the side, and slowly fade it away over time."},
		{"CSQC_Parse_Print",		"void(string printmsg, float printlvl)", CS, "Gives the CSQC a chance to intercept sprint/bprint builtin calls. CSQC should filter by the client's current msg setting and then pass the message on to the print command, or handle them itself."},
		{"CSQC_Parse_Event",		"void()", CS, "Called when the client receives an SVC_CGAMEPACKET. The csqc should read the data or call the error builtin if it does not recognise the message."},
		{"CSQC_InputEvent",			"float(float evtype, float scanx, float chary, float devid)", CS, "Called whenever a key is pressed, the mouse is moved, etc. evtype will be one of the IE_* constants. The other arguments vary depending on the evtype. Key presses are not guarenteed to have both scan and unichar values set at the same time."},
		{"CSQC_Input_Frame",		"__used void()", CS, "Called just before each time clientcommandframe is updated. You can edit the input_* globals in order to apply your own player inputs within csqc, which may allow you a convienient way to pass certain info to ssqc."},
		{"CSQC_RendererRestarted",	"void(string rendererdescription)", CS, "Called by the engine after the video was restarted. This serves to notify the CSQC that any render targets that it may have cached were purged, and will need to be regenerated."},
		{"CSQC_ConsoleCommand",		"float(string cmd)", CS, "Called if the user uses any console command registed via registercommand."},
		{"CSQC_ConsoleLink",		"float(string text, string info)", CS, "Called if the user clicks a ^[text\\infokey\\infovalue^] link. Use infoget to read/check each supported key. Return true if you wish the engine to not attempt to handle the link itself."},
		{"CSQC_Ent_Update",			"void(float isnew)", CS},
		{"CSQC_Ent_Remove",			"void()", CS},
		{"CSQC_Event_Sound",		"float(float entnum, float channel, string soundname, float vol, float attenuation, vector pos, float pitchmod, float flags"/*", float timeofs*/")", CS},
//		{"CSQC_ServerSound",		"//void()", CS},
		{"CSQC_LoadResource",		"float(string resname, string restype)", CS, "Called each time some resource is being loaded. CSQC can invoke various draw calls to provide a loading screen, until WorldLoaded is called."},
		{"CSQC_Parse_TempEntity",	"float()", CS,	"Please don't use this. Use CSQC_Parse_Event and multicasts instead."},

		{"GameCommand",				"void(string cmdtext)", CS|MENU},

		{"init",					"void(float prevprogs)", QW|NQ|CS, "Part of FTE_MULTIPROGS. Called as soon as a progs is loaded, called at a time when entities are not valid. This is the only time when it is safe to call addprogs without field assignment. As it is also called as part of addprogs, this also gives you a chance to hook functions in modules that are already loaded (via externget+externget)."},
		{"initents",				"void()", QW|NQ|CS, "Part of FTE_MULTIPROGS. Called after fields have been finalized. This is the first point at which it is safe to call spawn(), and is called before any entity fields have been parsed. You can use this entrypoint to send notifications to other modules."},

		{"m_init",					"void()", MENU},
		{"m_shutdown",				"void()", MENU},
		{"m_draw",					"void(vector screensize)", MENU, "Provides the menuqc with a chance to draw. Will be called even if the menu does not have focus, so be sure to avoid that. COMPAT: screensize is not provided in DP."},
		{"m_drawloading",			"void(vector screensize, float opaque)", MENU, "Additional drawing function to draw loading screens. If opaque is set, then this function must ensure that the entire screen is overdrawn (even if just by a black drawfill)."},
		{"m_keydown",				"void(float scan, float chr)", MENU},
		{"m_keyup",					"void(float scan, float chr)", MENU},
		{"m_toggle",				"void(float wantmode)", MENU},
		{"m_consolecommand",		"float(string cmd)", MENU},

		{"parm17, parm18, parm19, parm20, parm21, parm22, parm23, parm24, parm25, parm26, parm27, parm28, parm29, parm30, parm31, parm32", "float", QW|NQ},
		{"parm33, parm34, parm35, parm36, parm37, parm38, parm39, parm40, parm41, parm42, parm43, parm44, parm45, parm46, parm47, parm48", "float", QW|NQ},
		{"parm49, parm50, parm51, parm52, parm53, parm54, parm55, parm56, parm57, parm58, parm59, parm60, parm61, parm62, parm63, parm64", "float", QW|NQ},
		{"dimension_send",			"var float", QW|NQ, "Used by multicast functionality. Multicasts (and related builtins that multicast internally) will only be sent to players where (player.dimension_see & dimension_send) is non-zero."},
		{"dimension_default",		"//var float", QW|NQ, "Default dimension bitmask", 255},
		{"physics_mode",			"__used var float", QW|NQ|CS, "0: original csqc - physics are not run\n1: DP-compat. Thinks occur, but not true movetypes.\n2: movetypes occur just as they do in ssqc.", 2},
		{"gamespeed",				"float", CS, "Set by the engine, this is the value of the sv_gamespeed cvar"},
		{"numclientseats",			"float", CS, "This is the number of splitscreen clients currently running on this client."},
		{"drawfontscale",			"var vector", CS|MENU, "Specifies a scaler for all text rendering. There are other ways to implement this.", 0, "'1 1 0'"},
		{"drawfont",				"float", CS|MENU, "Allows you to choose exactly which font is to be used to draw text. Fonts can be registered/allocated with the loadfont builtin."},
		{"FONT_DEFAULT",			"const float", CS|MENU, NULL, 0},

		{"TRUE",					"const float", ALL, NULL, 1},
		{"FALSE",					"const float", ALL, "File not found...", 0},
		{"M_PI",					"const float", ALL, NULL, M_PI},

		{"MOVETYPE_NONE",			"const float", QW|NQ|CS, NULL, MOVETYPE_NONE},
		{"MOVETYPE_WALK",			"const float", QW|NQ|CS, NULL, MOVETYPE_WALK},
		{"MOVETYPE_STEP",			"const float", QW|NQ|CS, NULL, MOVETYPE_STEP},
		{"MOVETYPE_FLY",			"const float", QW|NQ|CS, NULL, MOVETYPE_FLY},
		{"MOVETYPE_TOSS",			"const float", QW|NQ|CS, NULL, MOVETYPE_TOSS},
		{"MOVETYPE_PUSH",			"const float", QW|NQ|CS, NULL, MOVETYPE_PUSH},
		{"MOVETYPE_NOCLIP",			"const float", QW|NQ|CS, NULL, MOVETYPE_NOCLIP},
		{"MOVETYPE_FLYMISSILE",		"const float", QW|NQ|CS, NULL, MOVETYPE_FLYMISSILE},
		{"MOVETYPE_BOUNCE",			"const float", QW|NQ|CS, NULL, MOVETYPE_BOUNCE},
		{"MOVETYPE_BOUNCEMISSILE",	"const float", QW|NQ|CS, NULL, MOVETYPE_BOUNCEMISSILE},
		{"MOVETYPE_FOLLOW",			"const float", QW|NQ|CS, NULL, MOVETYPE_FOLLOW},
		{"MOVETYPE_PUSHPULL",		"const float", H2,		 D("Identical to MOVETYPE_STEP. QC may treat the entity differently (typically with movechains)."), MOVETYPE_H2PUSHPULL},
		{"MOVETYPE_SWIM",			"const float", H2,		 D("Equivelent to MOVETYPE_STEP, but additionally walkmove/movetogoal will not allow a movetype_swim entity to move out of water."), MOVETYPE_H2SWIM},
		{"MOVETYPE_6DOF",			"const float", QW|NQ|CS, D("A glorified MOVETYPE_FLY. Players using this movetype will get some flightsim-like physics, with fully independant rotations (order-dependant transforms)."), MOVETYPE_6DOF},
		{"MOVETYPE_WALLWALK",		"const float", QW|NQ|CS, D("Players using this movetype will be able to orient themselves to walls, and then run up them."), MOVETYPE_WALLWALK},
		{"MOVETYPE_PHYSICS",		"const float", QW|NQ|CS, D("Enable the use of ODE physics upon this entity."), MOVETYPE_PHYSICS},

		{"SOLID_NOT",				"const float", QW|NQ|CS, NULL, SOLID_NOT},
		{"SOLID_TRIGGER",			"const float", QW|NQ|CS, NULL, SOLID_TRIGGER},
		{"SOLID_BBOX",				"const float", QW|NQ|CS, NULL, SOLID_BBOX},
		{"SOLID_SLIDEBOX",			"const float", QW|NQ|CS, NULL, SOLID_SLIDEBOX},
		{"SOLID_BSP",				"const float", QW|NQ|CS, D("Does not collide against other SOLID_BSP entities. Normally paired with MOVETYPE_PUSH."), SOLID_BSP},
		{"SOLID_CORPSE",			"const float", QW|NQ|CS, D("Non-solid to SOLID_SLIDEBOX or other SOLID_CORPSE entities. For hitscan weapons to hit corpses, change the player's .solid value to SOLID_BBOX or so, perform the traceline, then revert the player's .solid value."), SOLID_CORPSE},
		{"SOLID_LADDER",			"const float", QW|NQ|CS, D("Obsolete and may be removed at some point. Use skin=CONTENT_LADDER and solid_bsp or solid_trigger instead."), SOLID_LADDER},
		{"SOLID_PORTAL",			"const float", QW|NQ|CS, D("CSG subtraction volume combined with entity transformations on impact."), SOLID_PORTAL},
		{"SOLID_PHYSICS_BOX",		"const float", QW|NQ|CS, NULL, SOLID_PHYSICS_BOX},
		{"SOLID_PHYSICS_SPHERE",	"const float", QW|NQ|CS, NULL, SOLID_PHYSICS_SPHERE},
		{"SOLID_PHYSICS_CAPSULE",	"const float", QW|NQ|CS, NULL, SOLID_PHYSICS_CAPSULE},
		{"SOLID_PHYSICS_TRIMESH",	"const float", QW|NQ|CS, NULL, SOLID_PHYSICS_TRIMESH},
		{"SOLID_PHYSICS_CYLINDER",	"const float", QW|NQ|CS, NULL, SOLID_PHYSICS_CYLINDER},

		{"GEOMTYPE_NONE",			"const float", QW|NQ|CS, NULL, GEOMTYPE_NONE},
		{"GEOMTYPE_SOLID",			"const float", QW|NQ|CS, NULL, GEOMTYPE_SOLID},
		{"GEOMTYPE_BOX",			"const float", QW|NQ|CS, NULL, GEOMTYPE_BOX},
		{"GEOMTYPE_SPHERE",			"const float", QW|NQ|CS, NULL, GEOMTYPE_SPHERE},
		{"GEOMTYPE_CAPSULE",		"const float", QW|NQ|CS, NULL, GEOMTYPE_CAPSULE},
		{"GEOMTYPE_TRIMESH",		"const float", QW|NQ|CS, NULL, GEOMTYPE_TRIMESH},
		{"GEOMTYPE_CYLINDER",		"const float", QW|NQ|CS, NULL, GEOMTYPE_CYLINDER},
		{"GEOMTYPE_CAPSULE_X",		"const float", QW|NQ|CS, NULL, GEOMTYPE_CAPSULE_X},
		{"GEOMTYPE_CAPSULE_Y",		"const float", QW|NQ|CS, NULL, GEOMTYPE_CAPSULE_Y},
		{"GEOMTYPE_CAPSULE_Z",		"const float", QW|NQ|CS, NULL, GEOMTYPE_CAPSULE_Z},
		{"GEOMTYPE_CYLINDER_X",		"const float", QW|NQ|CS, NULL, GEOMTYPE_CYLINDER_X},
		{"GEOMTYPE_CYLINDER_Y",		"const float", QW|NQ|CS, NULL, GEOMTYPE_CYLINDER_Y},
		{"GEOMTYPE_CYLINDER_Z",		"const float", QW|NQ|CS, NULL, GEOMTYPE_CYLINDER_Z},

		{"JOINTTYPE_FIXED",			"const float", QW|NQ|CS, NULL, JOINTTYPE_FIXED},
		{"JOINTTYPE_POINT",			"const float", QW|NQ|CS, NULL, JOINTTYPE_POINT},
		{"JOINTTYPE_HINGE",			"const float", QW|NQ|CS, NULL, JOINTTYPE_HINGE},
		{"JOINTTYPE_SLIDER",		"const float", QW|NQ|CS, NULL, JOINTTYPE_SLIDER},
		{"JOINTTYPE_UNIVERSAL",		"const float", QW|NQ|CS, NULL, JOINTTYPE_UNIVERSAL},
		{"JOINTTYPE_HINGE2",		"const float", QW|NQ|CS, NULL, JOINTTYPE_HINGE2},

		{"GE_MAXENTS",		"const float", CS, "Valid for getentity, ignores the entity argument. Returns the maximum number of entities which may be valid, to avoid having to poll 65k when only 100 are used.", GE_MAXENTS},
		{"GE_ACTIVE",		"const float", CS, "Valid for getentity. Returns whether this entity is known to the client or not.", GE_ACTIVE},
		{"GE_ORIGIN",		"const float", CS, "Valid for getentity. Returns the interpolated .origin.", GE_ORIGIN},
		{"GE_FORWARD",		"const float", CS, "Valid for getentity. Returns the interpolated forward vector.", GE_FORWARD},
		{"GE_RIGHT",		"const float", CS, "Valid for getentity. Returns the entity's right vector.", GE_RIGHT},
		{"GE_UP",			"const float", CS, "Valid for getentity. Returns the entity's up vector.", GE_UP},
		{"GE_SCALE",		"const float", CS, "Valid for getentity. Returns the entity .scale.", GE_SCALE},
		{"GE_ORIGINANDVECTORS","const float", CS, "Valid for getentity. Returns interpolated .origin, but also sets v_forward, v_right, and v_up accordingly. Use vectoangles(v_forward,v_up) to determine the angles.", GE_ORIGINANDVECTORS},
		{"GE_ALPHA",		"const float", CS, "Valid for getentity. Returns the entity alpha.", GE_ALPHA},
		{"GE_COLORMOD",		"const float", CS, "Valid for getentity. Returns the colormod vector.", GE_COLORMOD},
		{"GE_PANTSCOLOR",	"const float", CS, "Valid for getentity. Returns the entity's lower color (from .colormap), as a palette range value.", GE_PANTSCOLOR},
		{"GE_SHIRTCOLOR",	"const float", CS, "Valid for getentity. Returns the entity's lower color (from .colormap), as a palette range value.", GE_SHIRTCOLOR},
		{"GE_SKIN",			"const float", CS, "Valid for getentity. Returns the entity's .skin index.", GE_SKIN},
		{"GE_MINS",			"const float", CS, "Valid for getentity. Guesses the entity's .min vector.", GE_MINS},
		{"GE_MAXS",			"const float", CS, "Valid for getentity. Guesses the entity's .max vector.", GE_MAXS},
		{"GE_ABSMIN",		"const float", CS, "Valid for getentity. Guesses the entity's .absmin vector.", GE_ABSMIN},
		{"GE_ABSMAX",		"const float", CS, "Valid for getentity. Guesses the entity's .absmax vector.", GE_ABSMAX},
//		{"GE_LIGHT",		"const float", CS, NULL, GE_LIGHT},

		{"GE_MODELINDEX",	"const float", CS, D("Valid for getentity. Guesses the entity's .modelindex float."), GE_MODELINDEX},
		{"GE_MODELINDEX2",	"const float", CS, D("Valid for getentity. Guesses the entity's .vw_index float."), GE_MODELINDEX2},
		{"GE_EFFECTS",		"const float", CS, D("Valid for getentity. Guesses the entity's .effects float."), GE_EFFECTS},
		{"GE_FRAME",		"const float", CS, D("Valid for getentity. Guesses the entity's .frame float."), GE_FRAME},
		{"GE_ANGLES",		"const float", CS, D("Valid for getentity. Guesses the entity's .angles vector."), GE_ANGLES},
		{"GE_FATNESS",		"const float", CS, D("Valid for getentity. Guesses the entity's .fatness float."), GE_FATNESS},
		{"GE_DRAWFLAGS",	"const float", CS, D("Valid for getentity. Guesses the entity's .drawflags float."), GE_DRAWFLAGS},
		{"GE_ABSLIGHT",		"const float", CS, D("Valid for getentity. Guesses the entity's .abslight float."), GE_ABSLIGHT},
		{"GE_GLOWMOD",		"const float", CS, D("Valid for getentity. Guesses the entity's .glowmod vector."), GE_GLOWMOD},
		{"GE_GLOWSIZE",		"const float", CS, D("Valid for getentity. Guesses the entity's .glowsize float."), GE_GLOWSIZE},
		{"GE_GLOWCOLOUR",	"const float", CS, D("Valid for getentity. Guesses the entity's .glowcolor float."), GE_GLOWCOLOUR},
		{"GE_RTSTYLE",		"const float", CS, D("Valid for getentity. Guesses the entity's .style float."), GE_RTSTYLE},
		{"GE_RTPFLAGS",		"const float", CS, D("Valid for getentity. Guesses the entity's .pflags float."), GE_RTPFLAGS},
		{"GE_RTCOLOUR",		"const float", CS, D("Valid for getentity. Guesses the entity's .color vector."), GE_RTCOLOUR},
		{"GE_RTRADIUS",		"const float", CS, D("Valid for getentity. Guesses the entity's .light_lev float."), GE_RTRADIUS},
		{"GE_TAGENTITY",	"const float", CS, D("Valid for getentity. Guesses the entity's .tag_entity float."), GE_TAGENTITY},
		{"GE_TAGINDEX",		"const float", CS, D("Valid for getentity. Guesses the entity's .tag_index float."), GE_TAGINDEX},
		{"GE_GRAVITYDIR",	"const float", CS, D("Valid for getentity. Guesses the entity's .gravitydir vector."), GE_GRAVITYDIR},
		{"GE_TRAILEFFECTNUM","const float",CS, D("Valid for getentity. Guesses the entity's .traileffectnum float."), GE_TRAILEFFECTNUM},

		{"DAMAGE_NO",		"const float", QW|NQ, NULL, DAMAGE_NO},
		{"DAMAGE_YES",		"const float", QW|NQ, NULL, DAMAGE_YES},
		{"DAMAGE_AIM",		"const float", QW|NQ, NULL, DAMAGE_AIM},

		{"CONTENT_EMPTY",	"const float", QW|NQ|CS, NULL, Q1CONTENTS_EMPTY},
		{"CONTENT_SOLID",	"const float", QW|NQ|CS, NULL, Q1CONTENTS_SOLID},
		{"CONTENT_WATER",	"const float", QW|NQ|CS, NULL, Q1CONTENTS_WATER},
		{"CONTENT_SLIME",	"const float", QW|NQ|CS, NULL, Q1CONTENTS_SLIME},
		{"CONTENT_LAVA",	"const float", QW|NQ|CS, NULL, Q1CONTENTS_LAVA},
		{"CONTENT_SKY",		"const float", QW|NQ|CS, NULL, Q1CONTENTS_SKY},
		{"CONTENT_LADDER",	"const float", QW|NQ|CS, D("If this value is assigned to a solid_bsp's .skin field, the entity will become a ladder volume."), Q1CONTENTS_LADDER},

		{"CONTENTBIT_NONE",			"const int", QW|NQ|CS, NULL, 0,STRINGIFY(FTECONTENTS_EMPTY)},
		{"CONTENTBIT_SOLID",		"const int", QW|NQ|CS, NULL, 0,STRINGIFY(FTECONTENTS_SOLID)},
		{"CONTENTBIT_LAVA",			"const int", QW|NQ|CS, NULL, 0,STRINGIFY(FTECONTENTS_LAVA)},
		{"CONTENTBIT_SLIME",		"const int", QW|NQ|CS, NULL, 0,STRINGIFY(FTECONTENTS_SLIME)},
		{"CONTENTBIT_WATER",		"const int", QW|NQ|CS, NULL, 0,STRINGIFY(FTECONTENTS_WATER)},
		{"CONTENTBIT_FTELADDER",	"const int", QW|NQ|CS, NULL, 0,STRINGIFY(FTECONTENTS_LADDER)},
		{"CONTENTBIT_PLAYERCLIP",	"const int", QW|NQ|CS, NULL, 0,STRINGIFY(FTECONTENTS_PLAYERCLIP)},
		{"CONTENTBIT_MONSTERCLIP",	"const int", QW|NQ|CS, NULL, 0,STRINGIFY(FTECONTENTS_MONSTERCLIP)},
		{"CONTENTBIT_BODY",			"const int", QW|NQ|CS, NULL, 0,STRINGIFY(FTECONTENTS_BODY)},
		{"CONTENTBIT_CORPSE",		"const int", QW|NQ|CS, NULL, 0,STRINGIFY(FTECONTENTS_CORPSE)},
		{"CONTENTBIT_Q2LADDER",		"const int", QW|NQ|CS, D("Content bit specific to q2bsp"), 0,STRINGIFY(Q2CONTENTS_LADDER)},
		{"CONTENTBIT_SKY",			"const int", QW|NQ|CS, NULL, 0,STRINGIFY(FTECONTENTS_SKY)"i"},
		{"CONTENTBITS_POINTSOLID",	"const int", QW|NQ|CS, D("Bits that traceline would normally consider solid"), 0,"CONTENTBIT_SOLID|"STRINGIFY(Q2CONTENTS_WINDOW)"|CONTENTBIT_BODY"},
		{"CONTENTBITS_BOXSOLID",	"const int", QW|NQ|CS, D("Bits that tracebox would normally consider solid"), 0,"CONTENTBIT_SOLID|"STRINGIFY(Q2CONTENTS_WINDOW)"|CONTENTBIT_BODY|CONTENTBIT_PLAYERCLIP"},
		{"CONTENTBITS_FLUID",		"const int", QW|NQ|CS, NULL, 0,"CONTENTBIT_WATER|CONTENTBIT_SLIME|CONTENTBIT_LAVA|CONTENTBIT_SKY"},

		{"SPA_POSITION",			"const int", QW|NQ|CS, D("These SPA_* constants are to specify which attribute is returned by the getsurfacepointattribute builtin"), 0},
		{"SPA_S_AXIS",				"const int", QW|NQ|CS, NULL, 1},
		{"SPA_T_AXIS",				"const int", QW|NQ|CS, NULL, 2},
		{"SPA_R_AXIS",				"const int", QW|NQ|CS, D("aka: SPA_NORMAL"), 3},
		{"SPA_TEXCOORDS0",			"const int", QW|NQ|CS, NULL, 4},
		{"SPA_LIGHTMAP0_TEXCOORDS",	"const int", QW|NQ|CS, NULL, 5},
		{"SPA_LIGHTMAP0_COLOR",		"const int", QW|NQ|CS, NULL, 6},

		{"CHAN_AUTO",		"const float", QW|NQ|CS, D("The automatic channel, play as many sounds on this channel as you want, and they'll all play, however the other channels will replace each other."), CHAN_AUTO},
		{"CHAN_WEAPON",		"const float", QW|NQ|CS, NULL, CHAN_WEAPON},
		{"CHAN_VOICE",		"const float", QW|NQ|CS, NULL, CHAN_VOICE},
		{"CHAN_ITEM",		"const float", QW|NQ|CS, NULL, CHAN_ITEM},
		{"CHAN_BODY",		"const float", QW|NQ|CS, NULL, CHAN_BODY},
		{"CHANF_RELIABLE",	"const float", QW,		 D("Only valid if the flags argument is not specified. The sound will be sent reliably, which is important if it is intended to replace looping sounds on doors etc."), 8},

		{"SOUNDFLAG_RELIABLE",		"const float",	QW|NQ,		D("The sound will be sent reliably, and without regard to phs."), CF_RELIABLE},
		{"SOUNDFLAG_ABSVOLUME",		"const float",	/*QW|NQ|*/CS,D("The sample's volume is not scaled by the volume cvar. Use with caution"), CF_ABSVOLUME},
		{"SOUNDFLAG_FORCELOOP",		"const float",	QW|NQ|CS,	D("The sound will restart once it reaches the end of the sample."), CF_FORCELOOP},
		{"SOUNDFLAG_NOSPACIALISE",	"const float",	/*QW|NQ|*/CS,D("The different audio channels are played at the same volume regardless of which way the player is facing, without needing to use 0 attenuation."), CF_NOSPACIALISE},
		{"SOUNDFLAG_NOREVERB",		"const float",	QW|NQ|CS,	D("Disables the use of underwater/reverb effects on this sound effect."), CF_NOREVERB},
		{"SOUNDFLAG_UNICAST",		"const float",	QW|NQ,		D("The sound will be heard only by the player specified by msg_entity."), CF_UNICAST},
		{"SOUNDFLAG_FOLLOW",		"const float",	QW|NQ|CS,	D("The sound's origin will updated to follow the emitting entity."), CF_FOLLOW},
		{"SOUNDFLAG_SENDVELOCITY",	"const float",	QW|NQ,		D("The entity's current velocity will be sent to the client, only useful if doppler is enabled."), CF_SENDVELOCITY},

		{"ATTN_NONE",		"const float", QW|NQ|CS, D("Sounds with this attenuation can be heard throughout the map"), ATTN_NONE},
		{"ATTN_NORM",		"const float", QW|NQ|CS, D("Standard attenuation"), ATTN_NORM},
		{"ATTN_IDLE",		"const float", QW|NQ|CS, D("Extra attenuation so that sounds don't travel too far."), 2},	//including these for completeness, despite them being defined by the gamecode rather than the engine api.
		{"ATTN_STATIC",		"const float", QW|NQ|CS, D("Even more attenuation to avoid torches drowing out everything else throughout the map."), 3},

		//not putting other svcs here, qc shouldn't otherwise need to generate svcs directly.
		{"SVC_CGAMEPACKET",		"const float", QW|NQ, D("Direct ssqc->csqc message. Must only be multicast. The data triggers a CSQC_Parse_Event call in the csqc for the csqc to read the contents. The server *may* insert length information for clients connected via proxies which are not able to cope with custom csqc payloads. This should only ever be used in conjunction with the MSG_MULTICAST destination."), svcfte_cgamepacket},

#ifndef QUAKETC
		{"MSG_BROADCAST",		"const float", QW|NQ, D("The byte(s) will be unreliably sent to all players. MSG_ constants are valid arguments to the Write* builtin family."), MSG_BROADCAST},
		{"MSG_ONE",				"const float", QW|NQ, D("The byte(s) will be reliably sent to the player specified in the msg_entity global. WARNING: in quakeworld servers without network preparsing enabled, this can result in illegible server messages (due to individual reliable messages being split between multiple backbuffers/packets). NQ has larger reliable buffers which avoids this issue, but still kicks the client."), MSG_ONE},
		{"MSG_ALL",				"const float", QW|NQ, D("The byte(s) will be reliably sent to all players."), MSG_ALL},
		{"MSG_INIT",			"const float", QW|NQ, D("The byte(s) will be written into the signon buffer. Clients will see these messages when they connect later. This buffer is only flushed on map changes, so spamming it _WILL_ result in overflows."), MSG_INIT},
#endif
		{"MSG_MULTICAST",		"const float", QW|NQ, D("The byte(s) will be written into the multicast buffer for more selective sending. Messages sent this way will never be split across packets, and using this for csqc-only messages will not break protocol translation."), MSG_MULTICAST},
		{"MSG_ENTITY",			"const float", QW|NQ, D("The byte(s) will be written into the entity buffer. This is a special value used only inside 'SendEntity' functions."), MSG_CSQC},

		{"MULTICAST_ALL",		"const float", QW|NQ, D("The multicast message is unreliably sent to all players. MULTICAST_ constants are valid arguments for the multicast builtin, which ignores the specified origin when given this constant."), MULTICAST_ALL},
		{"MULTICAST_PHS",		"const float", QW|NQ, D("The multicast message is unreliably sent to only players that can potentially hear the specified origin. Its quite loose."), MULTICAST_PHS},
		{"MULTICAST_PVS",		"const float", QW|NQ, D("The multicast message is unreliably sent to only players that can potentially see the specified origin."), MULTICAST_PVS},
		{"MULTICAST_ONE",		"const float", QW|NQ, D("The multicast message is unreliably sent to the player specified in the msg_entity global. The specified origin is ignored."), MULTICAST_ONE},
		{"MULTICAST_ALL_R",		"const float", QW|NQ, D("The multicast message is reliably sent to all players. The specified origin is ignored."), MULTICAST_ALL_R},
		{"MULTICAST_PHS_R",		"const float", QW|NQ, D("The multicast message is reliably sent to only players that can potentially hear the specified origin. Players might still not receive it if they are out of range."), MULTICAST_PHS_R},
		{"MULTICAST_PVS_R",		"const float", QW|NQ, D("The multicast message is reliably sent to only players that can potentially see the specified origin. Players might still not receive it if they cannot see the event."), MULTICAST_PVS_R},
		{"MULTICAST_ONE_R",		"const float", QW|NQ, D("The multicast message is reliably sent to the player specified in the msg_entity global. The specified origin is ignored"), MULTICAST_ONE_R},

		{"PRINT_LOW",			"const float", QW, NULL, PRINT_LOW},
		{"PRINT_MEDIUM",		"const float", QW, NULL, PRINT_MEDIUM},
		{"PRINT_HIGH",			"const float", QW, NULL, PRINT_HIGH},
		{"PRINT_CHAT",			"const float", QW, NULL, PRINT_CHAT},

		{"PVSF_NORMALPVS",		"const float", QW|NQ, D("Filter first by PVS, then filter this entity using tracelines if sv_cullentities is enabled."), PVSF_NORMALPVS},
		{"PVSF_NOTRACECHECK",	"const float", QW|NQ, D("Filter strictly by PVS."), PVSF_NOTRACECHECK},
		{"PVSF_USEPHS",			"const float", QW|NQ, D("Send if we're close enough to be able to hear this entity."), PVSF_USEPHS},
		{"PVSF_IGNOREPVS",		"const float", QW|NQ, D("Ignores pvs. This entity is visible whereever you are on the map."), PVSF_IGNOREPVS},
		{"PVSF_NOREMOVE",		"const float", QW|NQ, D("Once visible to a client, this entity will remain visible. This can be useful for csqc and corpses."), PVSF_NOREMOVE},

		//most of these are there for documentation rather than anything else.
		{"INFOKEY_P_IP",		"const string", QW|NQ, D("The apparent ip address of the client. This may be a proxy's ip address."), 0, "\"ip\""},
		{"INFOKEY_P_REALIP",	"const string", QW|NQ, D("If sv_getrealip is set, this gives the ip as determine using that algorithm."), 0, "\"realip\""},
		{"INFOKEY_P_CSQCACTIVE","const string", QW|NQ, D("Client has csqc enabled. CSQC ents etc will be sent to this player."), 0, "\"csqcactive\""},
		{"INFOKEY_P_SVPING",	"const string", QW|NQ, NULL, 0, "\"svping\""},
		{"INFOKEY_P_GUID",		"const string", QW|NQ, D("Some hash string which should be reasonably unique to this player's quake installation."), 0, "\"guid\""},
		{"INFOKEY_P_CHALLENGE",	"const string", QW|NQ, NULL, 0, "\"challenge\""},
		{"INFOKEY_P_USERID",	"const string", QW|NQ, NULL, 0, "\"*userid\""},
		{"INFOKEY_P_DOWNLOADPCT","const string",QW|NQ, D("The client's download percentage for the current file. Additional files are not known."), 0, "\"download\""},
		{"INFOKEY_P_TRUSTLEVEL","const string", QW|NQ, NULL, 0, "\"trustlevel\""},
		{"INFOKEY_P_PROTOCOL",	"const string", QW|NQ, D("The network protocol the client is using to connect to the server."), 0, "\"protocol\""},
		{"INFOKEY_P_VIP",		"const string", QW|NQ, D("1 if the player has the VIP 'penalty'."), 0, "\"*VIP\""},
		{"INFOKEY_P_ISMUTED",	"const string", QW|NQ, D("1 if the player has the 'mute' penalty and is not allowed to use the say/say_team commands."), 0, "\"*ismuted\""},
		{"INFOKEY_P_ISDEAF",	"const string", QW|NQ, D("1 if the player has the 'deaf' penalty and cannot see other people's say/say_team commands."), 0, "\"*isdeaf\""},
		{"INFOKEY_P_ISCRIPPLED","const string", QW|NQ, D("1 if the player has the cripple penalty, and their movement values are ignored (.movement is locked to 0)."), 0, "\"*ismuted\""},
		{"INFOKEY_P_ISCUFFED",	"const string", QW|NQ, D("1 if the player has the cuff penalty, and is unable to attack or use impulses(.button0 and .impulse fields are locked to 0)."), 0, "\"*ismuted\""},
		{"INFOKEY_P_ISLAGGED",	"const string", QW|NQ, D("1 if the player has the fakelag penalty and has an extra 200ms of lag."), 0, "\"*ismuted\""},
		{"INFOKEY_P_PING",		"const string", CS|QW|NQ, D("The player's ping time, in milliseconds."), 0, "\"ping\""},
		{"INFOKEY_P_NAME",		"const string", CS|QW|NQ, D("The player's name."), 0, "\"name\""},
		{"INFOKEY_P_SPECTATOR",	"const string", CS|QW|NQ, D("Whether the player is a spectator or not."), 0, "\"*spectator\""},
		{"INFOKEY_P_TOPCOLOR",	"const string", CS|QW|NQ, D("The player's upper/shirt colour (palette index)."), 0, "\"topcolor\""},
		{"INFOKEY_P_BOTTOMCOLOR","const string", CS|QW|NQ, D("The player's lower/pants/trouser colour (palette index)."), 0, "\"bottomcolor\""},
		{"INFOKEY_P_TOPCOLOR_RGB","const string", CS, D("The player's upper/shirt colour as an rgb value in a format usable with stov."), 0, "\"topcolor_rgb\""},
		{"INFOKEY_P_BOTTOMCOLOR_RGB","const string", CS, D("The player's lower/pants/trouser colour as an rgb value in a format usable with stov."), 0, "\"bottomcolor_rgb\""},
		{"INFOKEY_P_MUTED",		"const string", CS, D("0: we can see the result of the player's say/say_team commands.   1: we see no say/say_team messages from this player. Use the ignore command to toggle this value."), 0, "\"ignored\""},
		{"INFOKEY_P_VOIP_MUTED","const string", CS, D("0: we can hear this player when they speak (assuming voip is generally enabled). 1: we ignore everything this player says. Use cl_voip_mute to change the values."), 0, "\"vignored\""},
		{"INFOKEY_P_ENTERTIME",	"const string", CS, D("Reads the timestamp at which the player entered the game, in terms of csqc's time global."), 0, "\"entertime\""},
		{"INFOKEY_P_FRAGS",		"const string", CS, D("Reads a player's frag count."), 0, "\"frags\""},
		{"INFOKEY_P_PACKETLOSS","const string", CS, D("Reads a player's packetloss, as a percentage."), 0, "\"pl\""},
		{"INFOKEY_P_VOIPSPEAKING","const string", CS, D("Boolean value that says whether the given player is currently sending voice information."), 0, "\"voipspeaking\""},
		{"INFOKEY_P_VOIPLOUDNESS","const string", CS, D("Only valid for the local player. Gives a value between 0 and 1 to indicate to the user how loud their mic is."), 0, "\"voiploudness\""},

		{"SERVERKEY_IP",		"const string",	CS,D("The address of the server we connected to."), 0, "\"ip\""},
		{"SERVERKEY_SERVERNAME","const string",	CS,D("The hostname that was last passed to the connect command."), 0, "\"servername\""},
		{"SERVERKEY_CONSTATE",	"const string",	CS,D("The current connection state. Will be set to one of: disconnected (menu-only mode), active (gamestate received and loaded), connecting(connecting, downloading, or precaching content, aka: loading screen)."), 0, "\"constate\""},
		{"SERVERKEY_TRANSFERRING","const string",CS,D("Set to the hostname of the server that we are attempting to connect or transfer to."), 0, "\"transferring\""},
		{"SERVERKEY_LOADSTATE",	"const string", CS, D("loadstage, loading image name, current step, max steps\nStages are: 1=connecting, 2=serverside, 3=clientside\nKey will be empty if we are not loading."), 0, "\"loadstate\""},
		{"SERVERKEY_PAUSESTATE","const string", CS, D("1 if the server claimed to be paused. 0 otherwise"), 0, "\"pausestate\""},
		{"SERVERKEY_DLSTATE",	"const string", CS,	D("The progress of any current downloads. Empty string if no download is active, otherwise a tokenizable string containing this info:\nfiles-remaining, total-size, unknown-sizes-flag, file-localname, file-remotename, file-percent, file-rate, file-received-bytes, file-total-bytes\nIf the current file info is omitted, then we are waiting for a download to start."), 0, "\"dlstate\""},
		{"SERVERKEY_PROTOCOL",	"const string", CS,	D("The protocol we are connected to the server with."), 0, "\"protocol\""},
		{"SERVERKEY_MAXPLAYERS","const string", CS,	D("The protocol we are connected to the server with."), 0, "\"maxplayers\""},

		{"STUFFCMD_IGNOREINDEMO","const float",	QW|NQ,	D("The protocol we are connected to the server with."), STUFFCMD_IGNOREINDEMO},
		{"STUFFCMD_DEMOONLY",	"const float",	QW|NQ,	D("The protocol we are connected to the server with."), STUFFCMD_DEMOONLY},

/*		{"SOUND_RELIABLE",		"const float",	QW|NQ,	D("The sound will be sent reliably, and without regard to phs."), CF_RELIABLE},
		{"SOUND_FORCELOOP",		"const float",	QW|NQ|CS,D("The sound will restart once it reaches the end of the sample."), CF_FORCELOOP},
		{"SOUND_NOSPACIALISE",	"const float",	QW|NQ|CS,D("The different audio channels are played at the same volume regardless of which way the player is facing, without needing to use 0 attenuation."), CF_NOSPACIALISE},
		{"SOUND_ABSVOLUME",		"const float",	QW|NQ|CS,D("The sample's volume is not scaled by the volume cvar. Use with caution"), CF_ABSVOLUME},
*/
		// edict.flags
		{"FL_FLY",				"const float", QW|NQ|CS, NULL, FL_FLY},
		{"FL_SWIM",				"const float", QW|NQ|CS, NULL, FL_SWIM},
		{"FL_CLIENT",			"const float", QW|NQ|CS, NULL, FL_CLIENT},
		{"FL_INWATER",			"const float", QW|NQ|CS, NULL, FL_INWATER},
		{"FL_MONSTER",			"const float", QW|NQ|CS, NULL, FL_MONSTER},
		{"FL_GODMODE",			"const float", QW|NQ, NULL, FL_GODMODE},
		{"FL_NOTARGET",			"const float", QW|NQ, NULL, FL_NOTARGET},
		{"FL_ITEM",				"const float", QW|NQ|CS, NULL, FL_ITEM},
		{"FL_ONGROUND",			"const float", QW|NQ|CS, NULL, FL_ONGROUND},
		{"FL_PARTIALGROUND",	"const float", QW|NQ|CS, NULL, FL_PARTIALGROUND},
		{"FL_WATERJUMP",		"const float", QW|NQ|CS, NULL, FL_WATERJUMP},
		{"FL_JUMPRELEASED",		"const float",    NQ|CS, NULL, FL_JUMPRELEASED},
		{"FL_FINDABLE_NONSOLID","const float", QW|NQ|CS, D("Allows this entity to be found with findradius"), FL_FINDABLE_NONSOLID},
		{"FL_MOVECHAIN_ANGLE",	"const float", H2, NULL, FL_MOVECHAIN_ANGLE},
		{"FL_LAGGEDMOVE",		"const float", QW|NQ, D("Enables anti-lag on rockets etc."), FLQW_LAGGEDMOVE},
		{"FL_CLASS_DEPENDENT",	"const float", H2, NULL, FL_CLASS_DEPENDENT},

		{"MOVE_NORMAL",			"const float", QW|NQ|CS, NULL, MOVE_NORMAL},
		{"MOVE_NOMONSTERS",		"const float", QW|NQ|CS, D("The trace will ignore all non-solid_bsp entities."), MOVE_NOMONSTERS},
		{"MOVE_MISSILE",		"const float", QW|NQ|CS, D("The trace will use a bbox size of +/- 15 against entities with FL_MONSTER set."), MOVE_MISSILE},
		{"MOVE_HITMODEL",		"const float", QW|NQ|CS, D("Traces will impact the actual mesh of the model instead of merely their bounding box. Should generally only be used for tracelines. Note that this flag is unreliable as an object can animate through projectiles. The bounding box MUST be set to completely encompass the entity or those extra areas will be non-solid (leaving a hole for things to go through)."), MOVE_HITMODEL},
		{"MOVE_TRIGGERS",		"const float", QW|NQ|CS, D("This trace type will impact only triggers. It will ignore non-solid entities."), MOVE_TRIGGERS},
		{"MOVE_EVERYTHING",		"const float", QW|NQ|CS, D("This type of trace will hit solids and triggers alike. Even non-solid entities."), MOVE_EVERYTHING},
		{"MOVE_LAGGED",			"const float", QW|NQ, D("Will use antilag based upon the player's latency. Traces will be performed against old positions for entities instead of their current origin."), MOVE_LAGGED},
		{"MOVE_ENTCHAIN",		"const float", QW|NQ|CS, D("Returns a list of entities impacted via the trace_ent.chain field"), MOVE_ENTCHAIN},
		{"MOVE_OTHERONLY",		"const float", QW|NQ|CS, D("Traces that use this trace type will collide against *only* the entity specified via the 'other' global, and will ignore all owner/solid_not/dimension etc rules, they will still adhere to contents and bsp/bbox rules though."), MOVE_OTHERONLY},

		{"RESTYPE_MODEL",		"const float", ALL,		 D("RESTYPE_* constants are used as arguments with the resourcestatus builtin."), RESTYPE_MODEL},
		{"RESTYPE_SOUND",		"const float", ALL,		 D("precache_sound"), RESTYPE_SOUND},
		{"RESTYPE_PARTICLE",	"const float", ALL,		 D("particleeffectnum"), RESTYPE_PARTICLE},
		{"RESTYPE_PIC",			"const float", CS|MENU,	 D("precache_pic. Status results are an amalgomation of the textures used by the named shader."), RESTYPE_SHADER},
		{"RESTYPE_SKIN",		"const float", CS|MENU,	 D("setcustomskin"), RESTYPE_SKIN},
		{"RESTYPE_TEXTURE",		"const float", CS|MENU,	 D("Individual textures within shaders. These are not directly usable, but may be named as part of a skin file, or a shader."), RESTYPE_TEXTURE},
		{"RESSTATE_NOTKNOWN",	"const float", ALL,		 D("RESSTATE_* constants are return values from the resourcestatus builtin. The engine doesn't know about the resource if it is in this state. This means you will need to precache it. Attempting to use it anyway may result in warnings, errors, or silently succeed, depending on engine version and resource type."), RESSTATE_NOTKNOWN},
		{"RESSTATE_NOTLOADED",	"const float", ALL,		 D("The resource was precached, but has been flushed and there has not been an attempt to reload it. If you use the resource normally, chances are it'll be loaded but at the cost of a stall."), RESSTATE_NOTLOADED},
		{"RESSTATE_LOADING",	"const float", ALL,		 D("Resources in this this state are queued for loading, and will be loaded at the engine's convienience. If you attempt to query the resource now, the engine will stall until the result is available. sounds in this state may be delayed, while models/pics/shaders may be invisible."), RESSTATE_LOADING},
		{"RESSTATE_FAILED",		"const float", ALL,		 D("Resources in this state are unusable/could not be loaded. You will get placeholders or dummy results. Queries will not stall the engine. The engine may display placeholder content."), RESSTATE_FAILED},
		{"RESSTATE_LOADED",		"const float", ALL,		 D("Resources in this state are finally usable, everything will work okay. Hurrah. Queries will not stall the engine."), RESSTATE_LOADED},
		{"EF_BRIGHTFIELD",		"const float", QW|NQ|CS, NULL, EF_BRIGHTFIELD},
		{"EF_MUZZLEFLASH",		"const float",    NQ|CS, NULL, EF_MUZZLEFLASH},
		{"EF_BRIGHTLIGHT",		"const float", QW|NQ|CS, NULL, EF_BRIGHTLIGHT},
		{"EF_DIMLIGHT",			"const float", QW|NQ|CS, NULL, EF_DIMLIGHT},
		{"EF_FLAG1",			"const float", QW      , NULL, QWEF_FLAG1},
		{"EF_FLAG2",			"const float", QW      , NULL, QWEF_FLAG2},
		{"EF_NODRAW",			"const float",    NQ|CS, NULL, NQEF_NODRAW},
		{"EF_ADDITIVE",			"const float",    NQ|CS, D("The entity will be drawn with an additive blend."), NQEF_ADDITIVE},
		{"EF_BLUE",				"const float", QW|NQ|CS, D("A blue glow"), EF_BLUE},
		{"EF_RED",				"const float", QW|NQ|CS, D("A red glow"), EF_RED},
		{"EF_GREEN",			"const float", QW|NQ|CS, D("A green glow"), EF_GREEN},
		{"EF_FULLBRIGHT",		"const float", QW|NQ|CS, D("This entity will ignore lighting"), EF_FULLBRIGHT},
		{"EF_NOSHADOW",			"const float", QW|NQ|CS, D("This entity will not cast shadows"), EF_NOSHADOW},
		{"EF_NODEPTHTEST",		"const float", QW|NQ|CS, D("This entity will be drawn over the top of other things that are closer."), EF_NODEPTHTEST},

		{"EF_NOMODELFLAGS",		"const float", QW|NQ, D("Surpresses the normal flags specified in the model."), EF_NOMODELFLAGS},

		{"MF_ROCKET",			"const float", QW|NQ|CS, NULL, EF_MF_ROCKET>>24},
		{"MF_GRENADE",			"const float", QW|NQ|CS, NULL, EF_MF_GRENADE>>24},
		{"MF_GIB",				"const float", QW|NQ|CS, D("Regular blood trail"), EF_MF_GIB>>24},
		{"MF_ROTATE",			"const float", QW|NQ|CS, NULL, EF_MF_ROTATE>>24},
		{"MF_TRACER",			"const float", QW|NQ|CS, D("AKA: green scrag trail"), EF_MF_TRACER>>24},
		{"MF_ZOMGIB",			"const float", QW|NQ|CS, D("Dark blood trail"), EF_MF_ZOMGIB>>24},
		{"MF_TRACER2",			"const float", QW|NQ|CS, D("AKA: hellknight projectile trail"), EF_MF_TRACER2>>24},
		{"MF_TRACER3",			"const float", QW|NQ|CS, D("AKA: purple vore trail"), EF_MF_TRACER3>>24},


		{"SL_ORG_TL",			"const float", QW|NQ, D("Used with showpic etc, specifies that the x+y values are relative to the top-left of the screen"), SL_ORG_TL},
		{"SL_ORG_TR",			"const float", QW|NQ, NULL, SL_ORG_TR},
		{"SL_ORG_BL",			"const float", QW|NQ, NULL, SL_ORG_BL},
		{"SL_ORG_BR",			"const float", QW|NQ, NULL, SL_ORG_BR},
		{"SL_ORG_MM",			"const float", QW|NQ, NULL, SL_ORG_MM},
		{"SL_ORG_TM",			"const float", QW|NQ, NULL, SL_ORG_TM},
		{"SL_ORG_BM",			"const float", QW|NQ, NULL, SL_ORG_BM},
		{"SL_ORG_ML",			"const float", QW|NQ, NULL, SL_ORG_ML},
		{"SL_ORG_MR",			"const float", QW|NQ, NULL, SL_ORG_MR},

		{"PFLAGS_NOSHADOW",		"const float", QW|NQ|CS, D("Associated RT lights attached will not cast shadows, making them significantly faster to draw."), PFLAGS_NOSHADOW},
		{"PFLAGS_CORONA",		"const float", QW|NQ|CS, D("Enables support of coronas on the associated rtlights."), PFLAGS_CORONA},
		{"PFLAGS_FULLDYNAMIC",	"const float", QW|NQ, D("When set in self.pflags, enables fully-customised dynamic lights. Custom rtlight information is not otherwise used."), PFLAGS_FULLDYNAMIC},

		//including these for csqc stat types.
//		{"EV_VOID",				"const float", QW|NQ|CS, NULL, ev_void},
		{"EV_STRING",			"const float", QW|NQ|CS, NULL, ev_string},
		{"EV_FLOAT",			"const float", QW|NQ|CS, NULL, ev_float},
		{"EV_VECTOR",			"const float", QW|NQ|CS, NULL, ev_vector},
		{"EV_ENTITY",			"const float", QW|NQ|CS, NULL, ev_entity},
		{"EV_FIELD",			"const float", QW|NQ|CS, NULL, ev_field},
		{"EV_FUNCTION",			"const float", QW|NQ|CS, NULL, ev_function},
		{"EV_POINTER",			"const float", QW|NQ|CS, NULL, ev_pointer},
		{"EV_INTEGER",			"const float", QW|NQ|CS, NULL, ev_integer},
		{"EV_VARIANT",			"const float", QW|NQ|CS, NULL, ev_variant},
//		{"EV_STRUCT",			"const float", QW|NQ|CS, NULL, ev_struct},
//		{"EV_UNION",			"const float", QW|NQ|CS, NULL, ev_union},

		{"gamestate",			"hashtable", ALL, D("Special hash table index for hash_add and hash_get. Entries in this table will persist over map changes (and doesn't need to be created/deleted)."), 0},
		{"HASH_REPLACE",		"const float", ALL, D("Used with hash_add. Attempts to remove the old value instead of adding two values for a single key."), 256},
		{"HASH_ADD",			"const float", ALL, D("Used with hash_add. The new entry will be inserted in addition to the existing entry."), 512},

#ifdef QUAKESTATS
		{"STAT_HEALTH",			"const float", CS, D("Player's health."), STAT_HEALTH},
		{"STAT_WEAPONMODELI",	"const float", CS, D("This is the modelindex of the current viewmodel (renamed from the original name 'STAT_WEAPON' due to confusions)."), STAT_WEAPONMODELI},
		{"STAT_AMMO",			"const float", CS, D("player.currentammo"), STAT_AMMO},
		{"STAT_ARMOR",			"const float", CS, NULL, STAT_ARMOR},
		{"STAT_WEAPONFRAME",	"const float", CS, NULL, STAT_WEAPONFRAME},
		{"STAT_SHELLS",			"const float", CS, NULL, STAT_SHELLS},
		{"STAT_NAILS",			"const float", CS, NULL, STAT_NAILS},
		{"STAT_ROCKETS",		"const float", CS, NULL, STAT_ROCKETS},
		{"STAT_CELLS",			"const float", CS, NULL, STAT_CELLS},
		{"STAT_ACTIVEWEAPON",	"const float", CS, D("player.weapon"), STAT_ACTIVEWEAPON},
		{"STAT_TOTALSECRETS",	"const float", CS, NULL, STAT_TOTALSECRETS},
		{"STAT_TOTALMONSTERS",	"const float", CS, NULL, STAT_TOTALMONSTERS},
		{"STAT_FOUNDSECRETS",	"const float", CS, NULL, STAT_SECRETS},
		{"STAT_KILLEDMONSTERS",	"const float", CS, NULL, STAT_MONSTERS},
		{"STAT_ITEMS",			"const float", CS, D("self.items | (self.items2<<23). In order to decode this stat properly, you need to use getstatbits(STAT_ITEMS,0,23) to read self.items, and getstatbits(STAT_ITEMS,23,11) to read self.items2 or getstatbits(STAT_ITEMS,28,4) to read the visible part of serverflags, whichever is applicable."), STAT_ITEMS},
		{"STAT_VIEWHEIGHT",		"const float", CS, D("player.view_ofs_z"), STAT_VIEWHEIGHT},
		{"STAT_VIEW2",			"const float", CS, D("This stat contains the number of the entity in the server's .view2 field."), STAT_VIEW2},
		{"STAT_VIEWZOOM",		"const float", CS, D("Scales fov and sensitiity. Part of DP_VIEWZOOM."), STAT_VIEWZOOM},

		{"STAT_USER",			"const float", QW|NQ|CS, D("Custom user stats start here (lower values are reserved for engine use)."), 32},
#endif

		{"VF_MIN",				"const float", CS|MENU, D("The top-left of the 3d viewport in screenspace. The VF_ values are used via the setviewprop/getviewprop builtins."), VF_MIN},
		{"VF_MIN_X",			"const float", CS|MENU, NULL, VF_MIN_X},
		{"VF_MIN_Y",			"const float", CS|MENU, NULL, VF_MIN_Y},
		{"VF_SIZE",				"const float", CS|MENU, D("The width+height of the 3d viewport in screenspace."), VF_SIZE},
		{"VF_SIZE_X",			"const float", CS|MENU, NULL, VF_SIZE_X},
		{"VF_SIZE_Y",			"const float", CS|MENU, NULL, VF_SIZE_Y},
		{"VF_VIEWPORT",			"const float", CS|MENU, D("vector+vector. Two argument shortcut for VF_MIN and VF_SIZE"), VF_VIEWPORT},
		{"VF_FOV",				"const float", CS|MENU, D("sets both fovx and fovy. consider using afov instead."), VF_FOV},
		{"VF_FOVX",				"const float", CS|MENU, D("horizontal field of view. does not consider aspect at all."), VF_FOVX},
		{"VF_FOVY",				"const float", CS|MENU, D("vertical field of view. does not consider aspect at all."), VF_FOVY},
		{"VF_ORIGIN",			"const float", CS|MENU, D("The origin of the view. Not of the player."), VF_ORIGIN},
		{"VF_ORIGIN_X",			"const float", CS|MENU, NULL, VF_ORIGIN_X},
		{"VF_ORIGIN_Y",			"const float", CS|MENU, NULL, VF_ORIGIN_Y},
		{"VF_ORIGIN_Z",			"const float", CS|MENU, NULL, VF_ORIGIN_Z},
		{"VF_ANGLES",			"const float", CS|MENU, D("The angles the view will be drawn at. Not the angle the client reports to the server."), VF_ANGLES},
		{"VF_ANGLES_X",			"const float", CS|MENU, NULL, VF_ANGLES_X},
		{"VF_ANGLES_Y",			"const float", CS|MENU, NULL, VF_ANGLES_Y},
		{"VF_ANGLES_Z",			"const float", CS|MENU, NULL, VF_ANGLES_Z},
		{"VF_DRAWWORLD",		"const float", CS, D("boolean. If set to 1, the engine will draw the world and static/persistant rtlights. If 0, the world will be skipped and everything will be fullbright."), VF_DRAWWORLD},
		{"VF_DRAWENGINESBAR",	"const float", CS, D("boolean. If set to 1, the sbar will be drawn, and viewsize will be honoured automatically."), VF_ENGINESBAR},
		{"VF_DRAWCROSSHAIR",	"const float", CS, D("boolean. If set to 1, the engine will draw its default crosshair."), VF_DRAWCROSSHAIR},

		{"VF_MINDIST",			"const float", CS|MENU, D("The distance of the near clip plane from the view position. Should generally not be <=0, as this would introduce NANs."), VF_MINDIST},
		{"VF_MAXDIST",			"const float", CS|MENU, D("The distance of the far clip plane from the view position. If 0, will be considered infinite."), VF_MAXDIST},

		{"VF_CL_VIEWANGLES",	"const float", CS, NULL, VF_CL_VIEWANGLES_V},
		{"VF_CL_VIEWANGLES_X",	"const float", CS, NULL, VF_CL_VIEWANGLES_X},
		{"VF_CL_VIEWANGLES_Y",	"const float", CS, NULL, VF_CL_VIEWANGLES_Y},
		{"VF_CL_VIEWANGLES_Z",	"const float", CS, NULL, VF_CL_VIEWANGLES_Z},

		{"VF_PERSPECTIVE",		"const float", CS|MENU, D("1: regular rendering. Fov specifies the angle. 0: isometric-style. Fov specifies the number of Quake Units each side of the viewport, and mindist restrictions are removed, pvs culling should be disabled."), VF_PERSPECTIVE},
		{"VF_ACTIVESEAT",		"#define VF_LPLAYER VF_ACTIVESEAT\nconst float", CS, D("The 'seat' number, used when running splitscreen."), VF_ACTIVESEAT},
		{"VF_AFOV",				"const float", CS|MENU, D("Aproximate fov. Matches the 'fov' cvar. The engine handles the aspect ratio for you."), VF_AFOV},
		{"VF_SCREENVSIZE",		"const float", CS|MENU, D("Provides a reliable way to retrieve the current virtual screen size (even if the screen is automatically scaled to retain aspect)."), VF_SCREENVSIZE},
		{"VF_SCREENPSIZE",		"const float", CS|MENU, D("Provides a reliable way to retrieve the current physical screen size (cvars need vid_restart for them to take effect)."), VF_SCREENPSIZE},
		{"VF_VIEWENTITY",		"const float", CS, D("Changes the RF_EXTERNALMODEL flag on entities to match the new selection, and removes entities flaged with RF_VIEWENTITY. Requires cunning use of .entnum and typically requires calling addentities(MASK_VIEWMODEL) too."), VF_VIEWENTITY},

		{"VF_RT_DESTCOLOUR",	"const float", CS|MENU, D("The texture name to write colour info into, this includes both 3d and 2d drawing.\nAdditional arguments are: format (rgba8=1,rgba16f=2,rgba32f=3), sizexy.\nWritten to by both 3d and 2d rendering.\nNote that any rendertarget textures may be destroyed on video mode changes or so. Shaders can name render targets by prefixing texture names with '$rt:', or $sourcecolour."), VF_RT_DESTCOLOUR0},
//		{"VF_RT_DESTCOLOUR1",	"const float", CS|MENU, D("Like VF_RT_DESTCOLOUR, for multiple render targets."), VF_RT_DESTCOLOUR1},
//		{"VF_RT_DESTCOLOUR2",	"const float", CS|MENU, D("Like VF_RT_DESTCOLOUR, for multiple render targets."), VF_RT_DESTCOLOUR2},
//		{"VF_RT_DESTCOLOUR3",	"const float", CS|MENU, D("Like VF_RT_DESTCOLOUR, for multiple render targets."), VF_RT_DESTCOLOUR3},
//		{"VF_RT_DESTCOLOUR4",	"const float", CS|MENU, D("Like VF_RT_DESTCOLOUR, for multiple render targets."), VF_RT_DESTCOLOUR4},
//		{"VF_RT_DESTCOLOUR5",	"const float", CS|MENU, D("Like VF_RT_DESTCOLOUR, for multiple render targets."), VF_RT_DESTCOLOUR5},
//		{"VF_RT_DESTCOLOUR6",	"const float", CS|MENU, D("Like VF_RT_DESTCOLOUR, for multiple render targets."), VF_RT_DESTCOLOUR6},
//		{"VF_RT_DESTCOLOUR7",	"const float", CS|MENU, D("Like VF_RT_DESTCOLOUR, for multiple render targets."), VF_RT_DESTCOLOUR7},
		{"VF_RT_SOURCECOLOUR",	"const float", CS|MENU, D("The texture name to use with shaders that specify a $sourcecolour map."), VF_RT_SOURCECOLOUR},
		{"VF_RT_DEPTH",			"const float", CS|MENU, D("The texture name to use as a depth buffer. Also used for shaders that specify $sourcedepth. 1-based. Additional arguments are: format (16bit=4,24bit=5,32bit=6), sizexy."), VF_RT_DEPTH},
		{"VF_RT_RIPPLE",		"const float", CS|MENU, D("The texture name to use as a ripplemap (target for shaders with 'sort ripple'). Also used for shaders that specify $ripplemap. 1-based. Additional arguments are: format, sizexy."), VF_RT_RIPPLE},
		{"VF_ENVMAP",			"const float", CS|MENU, D("The cubemap name to use as a fallback for $reflectcube, if a shader was unable to load one. Note that this doesn't automatically change shader permutations or anything."), VF_ENVMAP},

		{"RF_VIEWMODEL",		"const float", CS, D("Specifies that the entity is a view model, and that its origin is relative to the current view position. These entities are also subject to viewweapon bob."), CSQCRF_VIEWMODEL},
		{"RF_EXTERNALMODEL",	"const float", CS, D("Specifies that this entity should be displayed in mirrors (and may still cast shadows), but will not otherwise be visible."), CSQCRF_EXTERNALMODEL},
		{"RF_DEPTHHACK",		"const float", CS|MENU, D("Hacks the depth values such that the entity uses depth values as if it were closer to the screen. This is useful when combined with viewmodels to avoid weapons poking in to walls."), CSQCRF_DEPTHHACK},
		{"RF_ADDITIVE",			"const float", CS|MENU, D("Shaders from this entity will temporarily be hacked to use an additive blend mode instead of their normal blend mode."), CSQCRF_ADDITIVE},
		{"RF_USEAXIS",			"const float", CS, D("The entity will be oriented according to the current v_forward+v_right+v_up vector values instead of the entity's .angles field."), CSQCRF_USEAXIS},
		{"RF_NOSHADOW",			"const float", CS, D("This entity will not cast shadows. Often useful on view models."), CSQCRF_NOSHADOW},
		{"RF_FRAMETIMESARESTARTTIMES","const float", CS, D("Specifies that the frame1time, frame2time field are timestamps (denoting the start of the animation) rather than time into the animation."), CSQCRF_FRAMETIMESARESTARTTIMES},

		{"IE_KEYDOWN",			"const float", CS, D("Specifies that a key was pressed. Second argument is the scan code. Third argument is the unicode (printable) char value. Fourth argument denotes which keyboard(or mouse, if its a mouse 'scan' key) the event came from. Note that some systems may completely separate scan codes and unicode values, with a 0 value for the unspecified argument."), CSIE_KEYDOWN},
		{"IE_KEYUP",			"const float", CS, D("Specifies that a key was released. Arguments are the same as IE_KEYDOWN. On some systems, this may be fired instantly after IE_KEYDOWN was fired."), CSIE_KEYUP},
		{"IE_MOUSEDELTA",		"const float", CS, D("Specifies that a mouse was moved (touch screens and tablets typically give IE_MOUSEABS events instead, use _windowed_mouse 0 to test code to cope with either). Second argument is the X displacement, third argument is the Y displacement. Fourth argument is which mouse or touch event triggered the event."), CSIE_MOUSEDELTA},
		{"IE_MOUSEABS",			"const float", CS, D("Specifies that a mouse cursor or touch event was moved to a specific location relative to the virtual screen space. Second argument is the new X position, third argument is the new Y position. Fourth argument is which mouse or touch event triggered the event."), CSIE_MOUSEABS},
		{"IE_ACCELEROMETER",	"const float", CS, NULL, CSIE_ACCELEROMETER},
		{"IE_FOCUS",			"const float", CS, D("Specifies that input focus was given. parama says mouse focus, paramb says keyboard focus. If either are -1, then it is unchanged."), CSIE_FOCUS},
		{"IE_JOYAXIS",			"const float", CS, D("Specifies that what value a joystick/controller axis currently specifies. x=axis, y=value. Will be called multiple times, once for each axis of each active controller."), CSIE_JOYAXIS},
		{"IE_GYROSCOPE",		"const float", CS, NULL, CSIE_GYROSCOPE},

		{"CLIENTTYPE_DISCONNECTED","const float", QW|NQ, D("Return value from clienttype() builtin. This entity is a player slot that is currently empty."), CLIENTTYPE_DISCONNECTED},
		{"CLIENTTYPE_REAL",		"const float", QW|NQ, D("This is a real player, and not a bot."), CLIENTTYPE_REAL},
		{"CLIENTTYPE_BOT",		"const float", QW|NQ, D("This player slot does not correlate to a real player, any messages sent to this client will be ignored."), CLIENTTYPE_BOT},
		{"CLIENTTYPE_NOTACLIENT","const float",QW|NQ, D("This entity is not even a player slot. This is typically an error condition."), CLIENTTYPE_NOTACLIENT},

		{"FILE_READ",			"const float", ALL, D("The file may be read via fgets to read a single line at a time."), FRIK_FILE_READ},
		{"FILE_APPEND",			"const float", ALL, D("Like FILE_WRITE, but writing starts at the end of the file."), FRIK_FILE_APPEND},
		{"FILE_WRITE",			"const float", ALL, D("fputs will be used to write to the file."), FRIK_FILE_WRITE},
		{"FILE_READNL",			"const float", QW|NQ|CS, D("Like FILE_READ, except newlines are not special. fgets reads the entire file into a tempstring."), FRIK_FILE_READNL},
		{"FILE_MMAP_READ",		"const float", QW|NQ|CS, D("The file will be loaded into memory. fgets returns a pointer to the first byte (and will always return the same value for this file). Cast this to your datatype."), FRIK_FILE_MMAP_READ},
		{"FILE_MMAP_RW",		"const float", QW|NQ|CS, D("Like FILE_MMAP_READ, except any changes to the data will be written back to disk once the file is closed."), FRIK_FILE_MMAP_RW},

		{"MASK_ENGINE",			"const float", CS, D("Valid as an argument for addentities. If specified, all non-csqc entities will be added to the scene."), MASK_DELTA},
		{"MASK_VIEWMODEL",		"const float", CS, D("Valid as an argument for addentities. If specified, the regular engine viewmodel will be added to the scene."), MASK_STDVIEWMODEL},
		{"PREDRAW_AUTOADD",		"const float", CS, D("Valid as a return value from the predraw function. Returning this will cause the engine to automatically invoke addentity(self) for you."), false},
		{"PREDRAW_NEXT",		"const float", CS, D("Valid as a return value from the predraw function. Returning this will simply move on to the next entity without the autoadd behaviour, so can be used for particle/invisible/special entites, or entities that were explicitly drawn with addentity."), true},

		{"LFIELD_ORIGIN",		"const float", CS, NULL, lfield_origin},
		{"LFIELD_COLOUR",		"const float", CS, NULL, lfield_colour},
		{"LFIELD_RADIUS",		"const float", CS, NULL, lfield_radius},
		{"LFIELD_FLAGS",		"const float", CS, NULL, lfield_flags},
		{"LFIELD_STYLE",		"const float", CS, NULL, lfield_style},
		{"LFIELD_ANGLES",		"const float", CS, NULL, lfield_angles},
		{"LFIELD_FOV",			"const float", CS, NULL, lfield_fov},
		{"LFIELD_CORONA",		"const float", CS, NULL, lfield_corona},
		{"LFIELD_CORONASCALE",	"const float", CS, NULL, lfield_coronascale},
		{"LFIELD_CUBEMAPNAME",	"const float", CS, NULL, lfield_cubemapname},
		{"LFIELD_AMBIENTSCALE",	"const float", CS, NULL, lfield_ambientscale},
		{"LFIELD_DIFFUSESCALE",	"const float", CS, NULL, lfield_diffusescale},
		{"LFIELD_SPECULARSCALE","const float", CS, NULL, lfield_specularscale},
		{"LFIELD_ROTATION",		"const float", CS, NULL, lfield_rotation},
		{"LFIELD_DIETIME",		"const float", CS, NULL, lfield_dietime},
		{"LFIELD_RGBDECAY",		"const float", CS, NULL, lfield_rgbdecay},
		{"LFIELD_RADIUSDECAY",	"const float", CS, NULL, lfield_radiusdecay},

		{"LFLAG_NORMALMODE",	"const float", CS, NULL, LFLAG_NORMALMODE},
		{"LFLAG_REALTIMEMODE",	"const float", CS, NULL, LFLAG_REALTIMEMODE},
		{"LFLAG_LIGHTMAP",		"const float", CS, NULL, LFLAG_LIGHTMAP},
		{"LFLAG_FLASHBLEND",	"const float", CS, NULL, LFLAG_FLASHBLEND},
		{"LFLAG_NOSHADOWS",		"const float", CS, NULL, LFLAG_NOSHADOWS},
		{"LFLAG_SHADOWMAP",		"const float", CS, NULL, LFLAG_SHADOWMAP},
		{"LFLAG_CREPUSCULAR",	"const float", CS, NULL, LFLAG_CREPUSCULAR},

#ifdef TERRAIN
		{"TEREDIT_RELOAD",		"const float", CS, NULL, ter_reload},
		{"TEREDIT_SAVE",		"const float", CS, NULL, ter_save},
		{"TEREDIT_SETHOLE",		"const float", CS, NULL, ter_sethole},
		{"TEREDIT_HEIGHT_SET",	"const float", CS, NULL, ter_height_set},
		{"TEREDIT_HEIGHT_SMOOTH","const float",CS, NULL, ter_height_smooth},
		{"TEREDIT_HEIGHT_SPREAD","const float",CS, NULL, ter_height_spread},
		{"TEREDIT_HEIGHT_RAISE","const float", CS, NULL, ter_raise},
		{"TEREDIT_HEIGHT_FLATTEN","const float", CS, NULL, ter_height_flatten},
		{"TEREDIT_HEIGHT_LOWER","const float", CS, NULL, ter_lower},
		{"TEREDIT_TEX_KILL",	"const float", CS, NULL, ter_tex_kill},
		{"TEREDIT_TEX_GET",		"const float", CS, NULL, ter_tex_get},
		{"TEREDIT_TEX_BLEND",	"const float", CS, NULL, ter_tex_blend},
		{"TEREDIT_TEX_UNIFY",	"const float", CS, NULL, ter_tex_concentrate},
		{"TEREDIT_TEX_NOISE",	"const float", CS, NULL, ter_tex_noise},
		{"TEREDIT_TEX_BLUR",	"const float", CS, NULL, ter_tex_blur},
		{"TEREDIT_TEX_REPLACE",	"const float", CS, NULL, ter_tex_replace},
		{"TEREDIT_TEX_SETMASK",	"const float", CS, NULL, ter_tex_mask},
		{"TEREDIT_WATER_SET",	"const float", CS, NULL, ter_water_set},
		{"TEREDIT_MESH_ADD",	"const float", CS, NULL, ter_mesh_add},
		{"TEREDIT_MESH_KILL",	"const float", CS, NULL, ter_mesh_kill},
		{"TEREDIT_TINT",		"const float", CS, NULL, ter_tint},
		{"TEREDIT_RESET_SECT",	"const float", CS, NULL, ter_reset},
		{"TEREDIT_RELOAD_SECT",	"const float", CS, NULL, ter_reloadsect},
		{"TEREDIT_ENTS_WIPE",	"const float", CS, NULL, ter_ents_wipe},
//		{"TEREDIT_ENTS_CONCAT",	"const float", CS, NULL, ter_ents_concat},
//		{"TEREDIT_ENTS_GET",	"const float", CS, NULL, ter_ents_get},
		{"TEREDIT_ENT_GET",		"const float", CS, NULL, ter_ent_get},
		{"TEREDIT_ENT_SET",		"const float", CS, NULL, ter_ent_set},
		{"TEREDIT_ENT_ADD",		"const float", CS, NULL, ter_ent_add},
		{"TEREDIT_ENT_COUNT",		"const float", CS, NULL, ter_ent_count},
#endif

		{"SLIST_HOSTCACHEVIEWCOUNT",	"const float", CS|MENU, NULL, SLIST_HOSTCACHEVIEWCOUNT},
		{"SLIST_HOSTCACHETOTALCOUNT",	"const float", CS|MENU, NULL, SLIST_HOSTCACHETOTALCOUNT},
		{"SLIST_MASTERQUERYCOUNT",		"const float", CS|MENU, NULL, SLIST_MASTERQUERYCOUNT},
		{"SLIST_MASTERREPLYCOUNT",		"const float", CS|MENU, NULL, SLIST_MASTERREPLYCOUNT},
		{"SLIST_SERVERQUERYCOUNT",		"const float", CS|MENU, NULL, SLIST_SERVERQUERYCOUNT},
		{"SLIST_SERVERREPLYCOUNT",		"const float", CS|MENU, NULL, SLIST_SERVERREPLYCOUNT},
		{"SLIST_SORTFIELD",				"const float", CS|MENU, NULL, SLIST_SORTFIELD},
		{"SLIST_SORTDESCENDING",		"const float", CS|MENU, NULL, SLIST_SORTDESCENDING},
		{"SLIST_TEST_CONTAINS",			"const float", CS|MENU, NULL, SLIST_TEST_CONTAINS},
		{"SLIST_TEST_NOTCONTAIN",		"const float", CS|MENU, NULL, SLIST_TEST_NOTCONTAIN},
		{"SLIST_TEST_LESSEQUAL",		"const float", CS|MENU, NULL, SLIST_TEST_LESSEQUAL},
		{"SLIST_TEST_LESS",				"const float", CS|MENU, NULL, SLIST_TEST_LESS},
		{"SLIST_TEST_EQUAL",			"const float", CS|MENU, NULL, SLIST_TEST_EQUAL},
		{"SLIST_TEST_GREATER",			"const float", CS|MENU, NULL, SLIST_TEST_GREATER},
		{"SLIST_TEST_GREATEREQUAL",		"const float", CS|MENU, NULL, SLIST_TEST_GREATEREQUAL},
		{"SLIST_TEST_NOTEQUAL",			"const float", CS|MENU, NULL, SLIST_TEST_NOTEQUAL},
		{"SLIST_TEST_STARTSWITH",		"const float", CS|MENU, NULL, SLIST_TEST_STARTSWITH},
		{"SLIST_TEST_NOTSTARTSWITH",	"const float", CS|MENU, NULL, SLIST_TEST_NOTSTARTSWITH},
		{NULL}
	};

#define DUMPHELP	\
					"Available options:\n"	\
					"-Ffte       - target only FTE (optimations and additional extensions)\n"	\
					"-Tnq        - dump specifically NQ fields\n"	\
					"-Tqw        - dump specifically QW fields\n"	\
					"-Tcs        - dump specifically CSQC fields\n"	\
					"-Tmenu      - dump specifically menuqc fields\n"	\
					"-Fdefines   - generate #defines instead of constants\n"	\
					"-Faccessors - use accessors instead of basic types via defines\n"	\
					"-O          - write to a different qc file\n"
	targ = 0;
	for (i = 1; i < Cmd_Argc(); i++)
	{
		char *arg = Cmd_Argv(i);
		if (!stricmp(arg, "-Ffte"))
			targ |= FTE;
		else if (!stricmp(arg, "-Tnq"))
			targ |= NQ;
		else if (!stricmp(arg, "-Tcs"))
			targ |= CS;
		else if (!stricmp(arg, "-Tqw"))
			targ |= QW;
		else if (!stricmp(arg, "-Tmenu"))
			targ |= MENU;
		else if (!stricmp(arg, "-Th2"))
			targ |= H2;
		else if (!stricmp(arg, "-Fdefines"))
			defines = true;
		else if (!stricmp(arg, "-Fnodefines"))
			defines = false;
		else if (!stricmp(arg, "-Faccessors"))
			accessors = true;
		else if (!stricmp(arg, "-Fnoaccessors"))
			accessors = false;
		else if (!Q_strncasecmp(arg, "-O", 2))
		{
			if (arg[2])
				fname = arg+2;
			else
				fname = Cmd_Argv(++i);
		}
		else
		{
			Con_Printf("Unknown argument \"%s\"\n" DUMPHELP, arg);
			return;
		}
	}
	if (!(targ & ALL))
		targ |= (QW|NQ|CS|MENU);

	if (!*fname)
		fname = "fteextensions";
	fname = va("%s/%s.qc", pr_sourcedir.string, fname);
	FS_NativePath(fname, FS_GAMEONLY, dbgfname, sizeof(dbgfname));
	FS_CreatePath(fname, FS_GAMEONLY);
	f = FS_OpenVFS(fname, "wb", FS_GAMEONLY);
	if (!f)
	{
		Con_Printf("Unable to create \"%s\"\n", dbgfname);
		return;
	}

	VFS_PRINTF(f,	"/*\n"
					"This file was automatically generated by %s v%i.%02i\n"
					"This file can be regenerated by issuing the following command:\n"
					"%s %s\n"
					"Available options:\n"
					"-Ffte       - target only FTE (optimations and additional extensions)\n"
					"-Tnq        - dump specifically NQ fields\n"
					"-Tqw        - dump specifically QW fields\n"
					"-Tcs        - dump specifically CSQC fields\n"
					"-Tmenu      - dump specifically menuqc fields\n"
					"-Fdefines   - generate #defines instead of constants\n"
					"-Faccessors - use accessors instead of basic types via defines\n"
					"-O          - write to a different qc file\n"
					"*/\n"
					, FULLENGINENAME, FTE_VER_MAJOR, FTE_VER_MINOR, Cmd_Argv(0), Cmd_Args());

	VFS_PRINTF(f, "#pragma noref 1\n");
	VFS_PRINTF(f, "//#pragma flag enable logicops\n");

	VFS_PRINTF(f, "#pragma warning error Q101 /*too many parms*/\n");
	VFS_PRINTF(f, "#pragma warning error Q105 /*too few parms*/\n");
	VFS_PRINTF(f, "#pragma warning error Q106 /*assignment to constant/lvalue*/\n");
	VFS_PRINTF(f, "#pragma warning error Q208 /*system crc unknown*/\n");
	VFS_PRINTF(f, "#pragma warning enable F301 /*non-utf-8 strings*/\n");
	VFS_PRINTF(f, "#pragma warning enable F302 /*uninitialised locals*/\n");

	if ((targ&ALL) == H2)
	{
		if (targ&FTE)
			VFS_PRINTF(f, "#pragma target FTEH2\n");
		else
			VFS_PRINTF(f, "#pragma target H2\n");
	}
	else
	{
		if (targ&FTE)
			VFS_PRINTF(f, "#pragma target FTE\n");
	}
	if ((targ&ALL) == CS)
		VFS_PRINTF(f,	"#ifndef CSQC\n"
							"#define CSQC\n"
						"#endif\n"
						);
	else if ((targ&ALL) == NQ)
		VFS_PRINTF(f,	"#ifndef NETQUAKE\n"
							"#define NETQUAKE\n"
						"#endif\n"
						"#ifndef NQSSQC\n"
							"#define NQSSQC\n"
						"#endif\n"
						"#ifndef SSQC\n"
							"#define SSQC\n"
						"#endif\n"
						);
	else if ((targ&ALL) == QW)
		VFS_PRINTF(f,	"#ifndef QUAKEWORLD\n"
							"#define QUAKEWORLD\n"
						"#endif\n"
						"#ifndef QWSSQC\n"
							"#define QWSSQC\n"
						"#endif\n"
						"#ifndef SSQC\n"
							"#define SSQC\n"
						"#endif\n"
						);
	else if ((targ&ALL) == MENU)
		VFS_PRINTF(f,	"#ifndef MENU\n"
							"#define MENU\n"
						"#endif\n"
						);
	else
		VFS_PRINTF(f,	"#if !defined(CSQC) && !defined(NQSSQC) && !defined(QWSSQC)&& !defined(MENU)\n"
							"#ifdef QUAKEWORLD\n"
								"#define QWSSQC\n"
							"#else\n"
								"#define NQSSQC\n"
							"#endif\n"
						"#endif\n"
						"#if !defined(SSQC) && (defined(QWSSQC) || defined(NQSSQC))\n"
							"#define SSQC\n"
						"#endif\n"
						);


	for (i = 0; i < QSG_Extensions_count; i++)
	{
		if (!QSG_Extensions[i].name || *QSG_Extensions[i].name == '?' || *QSG_Extensions[i].name == '_')
			continue;	//FIXME!

		if (QSG_Extensions[i].description)
			VFS_PRINTF(f, "#define %s /* %s */\n", QSG_Extensions[i].name, QSG_Extensions[i].description);
		else
			VFS_PRINTF(f, "#define %s\n", QSG_Extensions[i].name);
	}

	VFS_PRINTF(f, "\n");

	if (accessors)
		VFS_PRINTF(f, "#define _ACCESSORS;\n");
	
	VFS_PRINTF(f, 
			"#ifdef _ACCESSORS\n"
				"accessor strbuf : float;\n"
				"accessor searchhandle : float;\n"
				"accessor hashtable : float;\n"
				"accessor infostring : string;\n"
				"accessor filestream : float;\n"
				"accessor filestream : float;\n"
			"#else\n"
				"#define strbuf float\n"
				"#define searchhandle float\n"
				"#define hashtable float\n"
				"#define infostring string\n"
				"#define filestream float\n"
			"#endif\n"
		);
	VFS_PRINTF(f, "\n");


	for (i = 0; knowndefs[i].name; i++)
	{
		for (j = 0; j < i; j++)
		{
			if (!strcmp(knowndefs[i].name, knowndefs[j].name))
				knowndefs[i].module &= ~knowndefs[j].module; /*clear the flag on the later dupe def*/
		}
	}

	d = ALL & ~targ;
	for (i = 0; knowndefs[i].name; i++)
	{
		nd = (knowndefs[i].module & targ) | (~targ & ALL);
		if (!(nd & targ))
			continue;
		if ((nd&targ) != (d&targ))
		{
			if (d != (ALL & ~targ))
				VFS_PRINTF(f, "#endif\n");
			if (((nd | (~targ)) & ALL) == ALL)
				d = ALL & ~targ;	//every part of the target is specified, so don't do the ifdef thing.
			else
			{
				d = nd;
				switch(d & (ALL & targ))
				{
				case 0:
					continue;
				case QW:
					VFS_PRINTF(f, "#if defined(QWSSQC)\n");
					break;
				case NQ:
					VFS_PRINTF(f, "#if defined(NQSSQC)\n");
					break;
				case QW|NQ:
					VFS_PRINTF(f, "#ifdef SSQC\n");
					break;
				case CS:
					VFS_PRINTF(f, "#ifdef CSQC\n");
					break;
				case QW|CS:
					VFS_PRINTF(f, "#if defined(CSQC) || defined(QWSSQC)\n");
					break;
				case NQ|CS:
					VFS_PRINTF(f, "#if defined(CSQC) || defined(NQSSQC)\n");
					break;
				case NQ|CS|QW:
					VFS_PRINTF(f, "#if defined(CSQC) || defined(SSQC)\n");
					break;
				case MENU:
					VFS_PRINTF(f, "#ifdef MENU\n");
					break;
				case QW|MENU:
					VFS_PRINTF(f, "#if defined(QWSSQC) || defined(MENU)\n");
					break;
				case NQ|MENU:
					VFS_PRINTF(f, "#if defined(NQSSQC) || defined(MENU)\n");
					break;
				case QW|NQ|MENU:
					VFS_PRINTF(f, "#if defined(SSQC) || defined(MENU)\n");
					break;
				case CS|MENU:
					VFS_PRINTF(f, "#if defined(CSQC) || defined(MENU)\n");
					break;
				case QW|CS|MENU:
					VFS_PRINTF(f, "#if defined(CSQC) || defined(QWSSQC) || defined(MENU)\n");
					break;
				case NQ|CS|MENU:
					VFS_PRINTF(f, "#if defined(CSQC) || defined(NQSSQC) || defined(MENU)\n");
					break;
				case ALL:
					break;
				}
			}
		}
		if (knowndefs[i].desc)
		{
			if (!strncmp(knowndefs[i].type, "//", 2))
				comment = va("\n/* %s */", knowndefs[i].desc);
			else
				comment = va("\t/* %s */", knowndefs[i].desc);
		}
		else
			comment = "";
		if (!strcmp(knowndefs[i].type, "const float"))
		{
			if (knowndefs[i].value >= (1<<23))
			{
				if (defines)
					VFS_PRINTF(f, "#define %s %i%s\n", knowndefs[i].name, (int)knowndefs[i].value, comment);
				else
					VFS_PRINTF(f, "%s %s = %i;%s\n", knowndefs[i].type, knowndefs[i].name, (int)knowndefs[i].value, comment);
			}
			else
			{
				if (defines)
					VFS_PRINTF(f, "#define %s %g%s\n", knowndefs[i].name, knowndefs[i].value, comment);
				else
					VFS_PRINTF(f, "%s %s = %g;%s\n", knowndefs[i].type, knowndefs[i].name, knowndefs[i].value, comment);
			}
		}
		else if (!strcmp(knowndefs[i].type, "const string"))
		{
			if (defines)
				VFS_PRINTF(f, "#define %s %s%s\n", knowndefs[i].name, knowndefs[i].valuestr, comment);
			else
				VFS_PRINTF(f, "%s %s = %s;%s\n", knowndefs[i].type, knowndefs[i].name, knowndefs[i].valuestr, comment);
		}
		else if (knowndefs[i].valuestr)
		{
			VFS_PRINTF(f, "%s %s = %s;%s\n", knowndefs[i].type, knowndefs[i].name, knowndefs[i].valuestr, comment);
		}
		else if (knowndefs[i].value)
		{
			VFS_PRINTF(f, "%s %s = %g;%s\n", knowndefs[i].type, knowndefs[i].name, knowndefs[i].value, comment);
		}
		else
			VFS_PRINTF(f, "%s %s;%s\n", knowndefs[i].type, knowndefs[i].name, comment);
	}
	for (i = 0; BuiltinList[i].name; i++)
	{
		if (BuiltinList[i].obsolete)
			continue;
		idx = 0;
		if (BuiltinList[i].ebfsnum)
			idx = BuiltinList[i].ebfsnum;
		else if (BuiltinList[i].nqnum)
			idx = BuiltinList[i].nqnum;
		else if (BuiltinList[i].qwnum)
			idx = BuiltinList[i].qwnum;
		else if (BuiltinList[i].h2num)
			idx = BuiltinList[i].h2num;
		else
			idx = 0;

		nd = 0;

		if (BuiltinList[i].bifunc != PF_Fixme && BuiltinList[i].bifunc != PF_Ignore)
		{
			if (!idx)	//no index is a dynamically linked builtin, and can thus be usable in any gamecode mode (so long as its ssqc anyway)
				nd |= NQ|QW|H2;
			if (BuiltinList[i].ebfsnum == idx)
				nd |= NQ|QW;
			if (BuiltinList[i].nqnum == idx)
				nd |= NQ;
			if (BuiltinList[i].qwnum == idx)
				nd |= QW;
			if (BuiltinList[i].h2num == idx)
				nd |= H2;
		}
		
		if (PR_CSQC_BuiltinValid(BuiltinList[i].name, idx))
			nd |= CS;
#ifdef MENU_DAT
		if (MP_BuiltinValid(BuiltinList[i].name, idx))
			nd |= MENU;
#endif
		
		if (!nd)	/*no idea what its for*/
			continue;
		nd |= (~targ & ALL);
		if (!(nd & targ))
			continue;
		if ((nd&targ) != (d&targ))
		{
			if (d != (ALL & ~targ))
				VFS_PRINTF(f, "#endif\n");

			if (((nd | (~targ)) & ALL) == ALL)
				d = ALL & ~targ;
			else
			{
				d = nd;
				switch(d & (ALL & targ))
				{
				case 0:
					continue;
				case QW:
					VFS_PRINTF(f, "#if defined(QWSSQC)\n");
					break;
				case NQ:
					VFS_PRINTF(f, "#if defined(NQSSQC)\n");
					break;
				case QW|NQ:
					VFS_PRINTF(f, "#ifdef SSQC\n");
					break;
				case CS:
					VFS_PRINTF(f, "#ifdef CSQC\n");
					break;
				case QW|CS:
					VFS_PRINTF(f, "#if defined(CSQC) || defined(QWSSQC)\n");
					break;
				case NQ|CS:
					VFS_PRINTF(f, "#if defined(CSQC) || defined(NQSSQC)\n");
					break;
				case NQ|CS|QW:
					VFS_PRINTF(f, "#if defined(CSQC) || defined(SSQC)\n");
					break;
				case MENU:
					VFS_PRINTF(f, "#ifdef MENU\n");
					break;
				case QW|MENU:
					VFS_PRINTF(f, "#if defined(QWSSQC) || defined(MENU)\n");
					break;
				case NQ|MENU:
					VFS_PRINTF(f, "#if defined(NQSSQC) || defined(MENU)\n");
					break;
				case QW|NQ|MENU:
					VFS_PRINTF(f, "#if defined(SSQC) || defined(MENU)\n");
					break;
				case CS|MENU:
					VFS_PRINTF(f, "#if defined(CSQC) || defined(MENU)\n");
					break;
				case QW|CS|MENU:
					VFS_PRINTF(f, "#if defined(CSQC) || defined(QWSSQC) || defined(MENU)\n");
					break;
				case NQ|CS|MENU:
					VFS_PRINTF(f, "#if defined(CSQC) || defined(NQSSQC) || defined(MENU)\n");
					break;
				case H2:
					VFS_PRINTF(f, "#ifdef H2\n");
					break;
				case H2|QW:
					VFS_PRINTF(f, "#if defined(H2) || defined(QWSSQC)\n");
					break;
				case H2|NQ:
					VFS_PRINTF(f, "#if defined(H2) || defined(NQSSQC)\n");
					break;
				case H2|QW|NQ:
					VFS_PRINTF(f, "#if defined(H2) || defined(SSQC)\n");
					break;
				case H2|CS:
					VFS_PRINTF(f, "#if defined(H2) || defined(CSQC)\n");
					break;
				case H2|QW|CS:
					VFS_PRINTF(f, "#if defined(H2) || defined(CSQC) || defined(QWSSQC)\n");
					break;
				case H2|NQ|CS:
					VFS_PRINTF(f, "#if defined(H2) || defined(CSQC) || defined(NQSSQC)\n");
					break;
				case H2|NQ|CS|QW:
					VFS_PRINTF(f, "#if defined(H2) || defined(CSQC) || defined(SSQC)\n");
					break;
				case H2|MENU:
					VFS_PRINTF(f, "#if defined(H2) || defined(MENU)\n");
					break;
				case H2|QW|MENU:
					VFS_PRINTF(f, "#if defined(H2) || defined(QWSSQC) || defined(MENU)\n");
					break;
				case H2|NQ|MENU:
					VFS_PRINTF(f, "#if defined(H2) || defined(NQSSQC) || defined(MENU)\n");
					break;
				case H2|QW|NQ|MENU:
					VFS_PRINTF(f, "#if defined(H2) || defined(SSQC) || defined(MENU)\n");
					break;
				case H2|CS|MENU:
					VFS_PRINTF(f, "#if defined(H2) || defined(CSQC) || defined(MENU)\n");
					break;
				case H2|QW|CS|MENU:
					VFS_PRINTF(f, "#if defined(H2) || defined(CSQC) || defined(QWSSQC) || defined(MENU)\n");
					break;
				case H2|NQ|CS|MENU:
					VFS_PRINTF(f, "#if defined(H2) || defined(CSQC) || defined(NQSSQC) || defined(MENU)\n");
					break;
				case ALL:
					VFS_PRINTF(f, "#if 1\n");
					break;
				default:
					VFS_PRINTF(f, "#if 0 //???\n");
					break;
				}
			}
		}
		if (idx)
			VFS_PRINTF(f, "%s%s %s = #%u;", BuiltinList[i].obsolete?"//":"", BuiltinList[i].prototype, BuiltinList[i].name, idx);
		else
			VFS_PRINTF(f, "%s%s %s = #%u:%s;", BuiltinList[i].obsolete?"//":"", BuiltinList[i].prototype, BuiltinList[i].name, idx, BuiltinList[i].name);
		nd = 0;
		for (j = 0; j < QSG_Extensions_count; j++)
		{
			for (k = 0; k < QSG_Extensions[j].numbuiltins; k++)
			{
				if (!strcmp(QSG_Extensions[j].builtinnames[k], BuiltinList[i].name))
				{
					if (!nd)
						VFS_PRINTF(f, " /* Part of ");
					else
						VFS_PRINTF(f, ", ");
					nd++;
					VFS_PRINTF(f, "%s", QSG_Extensions[j].name);
				}
			}
		}
		if (BuiltinList[i].biglongdesc)
		{
			char *line = BuiltinList[i].biglongdesc;
			char *term;
			if (!nd)
				VFS_PRINTF(f, " /*");
			while(*line)
			{
				VFS_PRINTF(f, "\n\t\t");
				term = line;
				while(*term && *term != '\n')
					term++;
				VFS_WRITE(f, line, term - line);
				if (*term == '\n')
				{
					term++;
				}
				line = term;
			}
			VFS_PRINTF(f, " */\n\n");
		}
		else if (nd)
			VFS_PRINTF(f, "*/\n");
		else
			VFS_PRINTF(f, "\n");
	}
	if (d != (ALL & ~targ))
		VFS_PRINTF(f, "#endif\n");

	if (targ & (CS|MENU))
	{
		VFS_PRINTF(f, "#if defined(CSQC) || defined(MENU)\n");
		Key_PrintQCDefines(f);
		VFS_PRINTF(f, "#endif\n");
	}

	if (accessors)
	{
		VFS_PRINTF(f, "#ifdef _ACCESSORS\n");
		VFS_PRINTF(f,
			"accessor strbuf : float\n{\n"
				"\tinline get float asfloat[float idx] = {return stof(bufstr_get(this, idx));};\n"
				"\tinline set float asfloat[float idx] = {bufstr_set(this, idx, ftos(value));};\n"
				"\tget string[float] = bufstr_get;\n"
				"\tset string[float] = bufstr_set;\n"
				"\tget float length = buf_getsize;\n"
			"};\n");
		VFS_PRINTF(f,
			"accessor searchhandle : float\n{\n"
				"\tget string[float] = search_getfilename;\n"
				"\tget float length = search_getsize;\n"
			"};\n");
		VFS_PRINTF(f,
			"accessor hashtable : float\n{\n"
				"\tinline get vector v[string key] = {return hash_get(this, key, '0 0 0', EV_VECTOR);};\n"
				"\tinline set vector v[string key] = {hash_add(this, key, value, HASH_REPLACE|EV_VECTOR);};\n"
				"\tinline get string s[string key] = {return hash_get(this, key, \"\", EV_STRING);};\n"
				"\tinline set string s[string key] = {hash_add(this, key, value, HASH_REPLACE|EV_STRING);};\n"
				"\tinline get string f[string key] = {return hash_get(this, key, 0.0, EV_FLOAT);};\n"
				"\tinline set string f[string key] = {hash_add(this, key, value, HASH_REPLACE|EV_FLOAT);};\n"
				"\tinline get __variant[string key] = {return hash_get(this, key, __NULL__);};\n"
				"\tinline set __variant[string key] = {hash_add(this, key, value, HASH_REPLACE);};\n"
			"};\n");
		VFS_PRINTF(f,
			"accessor infostring : string\n{\n"
				"\tget string[string] = infoget;\n"
#ifdef QCGC
				"\tinline set* string[string fld] = {(*this) = infoadd(*this, fld, value);};\n"
#endif
			"};\n");
		VFS_PRINTF(f,
			"accessor filestream : float\n{\n"
				"\tget string = fgets;\n"
				"\tinline set string = {fputs(this,value);};\n"
			"};\n");
		VFS_PRINTF(f, "#endif\n");
	}

	VFS_PRINTF(f, "#pragma noref 0\n");

	VFS_CLOSE(f);

	Con_Printf("Written \"%s\"\n", dbgfname);
#endif
}

#endif
