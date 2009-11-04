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

#define QCLIB	//as opposed to standard qc stuff. One or other. All references+changes were by DMW unless specified otherwise. Starting 1/10/02

struct client_s;
struct edict_s;

#define MAX_PROGS 64
#define MAXADDONS 16

#define	NUM_SPAWN_PARMS			32	//moved from server.h because of include ordering :(.

#define NewGetEdictFieldValue GetEdictFieldValue
void Q_SetProgsParms(qboolean forcompiler);
void PR_Deinit(void);
void PR_LoadGlabalStruct(void);
void Q_InitProgs(void);
void PR_RegisterFields(void);
void PR_Init(void);
void ED_Spawned (struct edict_s *ent, int loading);
qboolean SV_RunFullQCMovement(struct client_s *client, usercmd_t *ucmd);
qboolean PR_KrimzonParseCommand(char *s);
qboolean PR_UserCmd(char *cmd);
qboolean PR_ConsoleCmd(void);

void PR_RunThreads(void);


#define PR_MAINPROGS 0	//this is a constant that should really be phased out. But seeing as QCLIB requires some sort of master progs due to extern funcs...
	//maybe go through looking for extern funcs, and remember which were not allocated. It would then be a first come gets priority. Not too bad I supppose.

#include "progtype.h"
#include "progdefs.h"

extern int compileactive;

typedef enum {PROG_NONE, PROG_QW, PROG_NQ, PROG_H2, PROG_PREREL, PROG_UNKNOWN} progstype_t;	//unknown obtains NQ behaviour
extern progstype_t progstype;
                                 

//extern globalvars_t *glob0;


//extern progparms_t progparms;

//extern progsnum_t mainprogs;

#if defined(ODE_STATIC) || defined(ODE_DYNAMIC)
#define USEODE 1
#endif

#ifdef USEODE
typedef struct {
	// physics parameters
	qboolean ode_physics;
	void *ode_body;
	void *ode_geom;
	void *ode_joint;
	float *ode_vertex3f;
	int *ode_element3i;
	int ode_numvertices;
	int ode_numtriangles;
	vec3_t ode_mins;
	vec3_t ode_maxs;
	vec_t ode_mass;
	vec3_t ode_origin;
	vec3_t ode_velocity;
	vec3_t ode_angles;
	vec3_t ode_avelocity;
	qboolean ode_gravity;
	int ode_modelindex;
	vec_t ode_movelimit; // smallest component of (maxs[]-mins[])
	float ode_offsetmatrix[16];
	float ode_offsetimatrix[16];
	int ode_joint_type;
	int ode_joint_enemy;
	int ode_joint_aiment;
	vec3_t ode_joint_origin; // joint anchor
	vec3_t ode_joint_angles; // joint axis
	vec3_t ode_joint_velocity; // second joint axis
	vec3_t ode_joint_movedir; // parameters
	void *ode_massbuf;
} entityode_t;

typedef struct
{
	// for ODE physics engine
	qboolean ode; // if true then ode is activated
	void *ode_world;
	void *ode_space;
	void *ode_contactgroup;
	// number of constraint solver iterations to use (for dWorldStepFast)
	int ode_iterations;
	// actual step (server frametime / ode_iterations)
	vec_t ode_step;
	// max velocity for a 1-unit radius object at current step to prevent
	// missed collisions
	vec_t ode_movelimit;
} worldode_t;
#endif

#define	MAX_ENT_LEAFS	16
typedef struct edict_s
{
	//these 5 shared with qclib
	qboolean	isfree;
	float		freetime; // sv.time when the object was freed
	int			entnum;
	qboolean	readonly;	//world
#ifdef VM_Q1
	stdentvars_t	*v;
	extentvars_t	*xv;
#else
	union {
		stdentvars_t	*v;
		stdentvars_t	*xv;
	};
#endif
	/*qc lib doesn't care about the rest*/

	/*these are shared with csqc*/
	link_t	area;
	int			num_leafs;
	short		leafnums[MAX_ENT_LEAFS];
#ifdef Q2BSPS
	int areanum;
	int areanum2;
	int headnode;
#endif
#ifdef USEODE
	entityode_t ode;
#endif
	qbyte solidtype;
	/*csqc doesn't reference the rest*/

	entity_state_t	baseline;
// other fields from progs come immediately after
} edict_t;
  


#include "progslib.h"

#undef pr_global_struct
//#define pr_nqglobal_struct *((nqglobalvars_t*)pr_globals)
#define pr_global_struct *pr_nqglobal_struct

float *spawnparamglobals[NUM_SPAWN_PARMS];

extern nqglobalvars_t *pr_nqglobal_struct;

extern progfuncs_t *svprogfuncs;	//instance
extern progparms_t svprogparms;
extern progsnum_t svmainprogs;
extern progsnum_t clmainprogs;
#define	EDICT_FROM_AREA(l) STRUCT_FROM_LINK(l,wedict_t,area)
#define	HLEDICT_FROM_AREA(l) STRUCT_FROM_LINK(l,hledict_t,area)
#define	Q2EDICT_FROM_AREA(l) STRUCT_FROM_LINK(l,q2edict_t,area)
#define	Q3EDICT_FROM_AREA(l) STRUCT_FROM_LINK(l,q3serverEntity_t,area)

extern func_t SpectatorConnect;
extern func_t SpectatorThink;
extern func_t SpectatorDisconnect;

extern func_t SV_PlayerPhysicsQC;
extern func_t EndFrameQC;

qboolean PR_QCChat(char *text, int say_type);

void PR_ClientUserInfoChanged(char *name, char *oldivalue, char *newvalue);
void PR_LocalInfoChanged(char *name, char *oldivalue, char *newvalue);
void PF_InitTempStrings(progfuncs_t *prinst);

#ifdef VM_Q1
struct client_s;
void Q1QVM_Shutdown(void);
qboolean PR_LoadQ1QVM(void);
void Q1QVM_ClientConnect(struct client_s *cl);
qboolean Q1QVM_GameConsoleCommand(void);
qboolean Q1QVM_ClientSay(edict_t *player, qboolean team);
qboolean Q1QVM_UserInfoChanged(edict_t *player);
void Q1QVM_PlayerPreThink(void);
void Q1QVM_RunPlayerThink(void);
void Q1QVM_PostThink(void);
void Q1QVM_StartFrame(void);
void Q1QVM_Touch(void);
void Q1QVM_Think(void);
void Q1QVM_Blocked(void);
void Q1QVM_SetNewParms(void);
void Q1QVM_SetChangeParms(void);
void Q1QVM_ClientCommand(void);
void Q1QVM_GameCodePausedTic(float pausedduration);
void Q1QVM_DropClient(struct client_s *cl);
void Q1QVM_ChainMoved(void);
void Q1QVM_EndFrame(void);
void Q1QVMED_ClearEdict (edict_t *e, qboolean wipe);
#endif

