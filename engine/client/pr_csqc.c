#include "quakedef.h"

#ifdef CSQC_DAT

#ifdef RGLQUAKE
#include "glquake.h"	//evil to include this
#include "shader.h"
#endif

#include "pr_common.h"

static progfuncs_t *csqcprogs;

typedef struct csqctreadstate_s {
	float resumetime;
	struct qcthread_s *thread;
	int self;
	int other;

	struct csqctreadstate_s *next;
} csqctreadstate_t;

static unsigned int csqcchecksum;
static csqctreadstate_t *csqcthreads;
qboolean csqc_resortfrags;
qboolean csqc_drawsbar;
qboolean csqc_addcrosshair;
static int num_csqc_edicts;

#define CSQCPROGSGROUP "CSQC progs control"
cvar_t	pr_csmaxedicts = SCVAR("pr_csmaxedicts", "3072");
cvar_t	cl_csqcdebug = SCVAR("cl_csqcdebug", "0");	//prints entity numbers which arrive (so I can tell people not to apply it to players...)
cvar_t  cl_nocsqc = SCVAR("cl_nocsqc", "0");
cvar_t  pr_csqc_coreonerror = SCVAR("pr_csqc_coreonerror", "1");

// standard effect cvars/sounds
extern cvar_t r_explosionlight;
extern sfx_t			*cl_sfx_wizhit;
extern sfx_t			*cl_sfx_knighthit;
extern sfx_t			*cl_sfx_tink1;
extern sfx_t			*cl_sfx_ric1;
extern sfx_t			*cl_sfx_ric2;
extern sfx_t			*cl_sfx_ric3;
extern sfx_t			*cl_sfx_r_exp3;

//If I do it like this, I'll never forget to register something...
#define csqcglobals	\
	globalfunction(init_function,		"CSQC_Init");	\
	globalfunction(shutdown_function,	"CSQC_Shutdown");	\
	globalfunction(draw_function,		"CSQC_UpdateView");	\
	globalfunction(parse_stuffcmd,		"CSQC_Parse_StuffCmd");	\
	globalfunction(parse_centerprint,	"CSQC_Parse_CenterPrint");	\
	globalfunction(input_event,			"CSQC_InputEvent");	\
	globalfunction(console_command,		"CSQC_ConsoleCommand");	\
	\
	globalfunction(ent_update,			"CSQC_Ent_Update");	\
	globalfunction(ent_remove,			"CSQC_Ent_Remove");	\
	\
	globalfunction(serversound,			"CSQC_ServerSound");	\
	\
	/*These are pointers to the csqc's globals.*/	\
	globalfloat(time,					"time");				/*float		Written before entering most qc functions*/	\
	globalentity(self,					"self");				/*entity	Written before entering most qc functions*/	\
	globalentity(other,					"other");				/*entity	Written before entering most qc functions*/	\
	\
	globalfloat(maxclients,				"maxclients");			/*float		*/	\
	\
	globalvector(forward,				"v_forward");			/*vector	written by anglevectors*/	\
	globalvector(right,					"v_right");				/*vector	written by anglevectors*/	\
	globalvector(up,					"v_up");				/*vector	written by anglevectors*/	\
	\
	globalfloat(trace_allsolid,			"trace_allsolid");		/*bool		written by traceline*/	\
	globalfloat(trace_startsolid,		"trace_startsolid");	/*bool		written by traceline*/	\
	globalfloat(trace_fraction,			"trace_fraction");		/*float		written by traceline*/	\
	globalfloat(trace_inwater,			"trace_inwater");		/*bool		written by traceline*/	\
	globalfloat(trace_inopen,			"trace_inopen");		/*bool		written by traceline*/	\
	globalvector(trace_endpos,			"trace_endpos");		/*vector	written by traceline*/	\
	globalvector(trace_plane_normal,	"trace_plane_normal");	/*vector	written by traceline*/	\
	globalfloat(trace_plane_dist,		"trace_plane_dist");	/*float		written by traceline*/	\
	globalentity(trace_ent,				"trace_ent");			/*entity	written by traceline*/	\
	globalfloat(trace_surfaceflags,		"trace_surfaceflags");	/*float		written by traceline*/	\
	globalfloat(trace_endcontents,		"trace_endcontents");	/*float		written by traceline*/	\
	\
	globalfloat(clientcommandframe,		"clientcommandframe");	\
	globalfloat(servercommandframe,		"servercommandframe");	\
	\
	globalfloat(player_localentnum,		"player_localentnum");	/*float		the entity number of the local player*/	\
	globalfloat(intermission,			"intermission");	/*float		the entity number of the local player*/	\
	globalvector(view_angles,			"view_angles");		\
	\
	globalvector(pmove_org,				"pmove_org");			\
	globalvector(pmove_vel,				"pmove_vel");			\
	globalvector(pmove_mins,			"pmove_mins");			\
	globalvector(pmove_maxs,			"pmove_maxs");			\
	globalfloat(pmove_jump_held,		"pmove_jump_held");		\
	globalfloat(pmove_waterjumptime,		"pmove_waterjumptime");	\
	globalfloat(input_timelength,		"input_timelength");	\
	globalvector(input_angles,			"input_angles");		\
	globalvector(input_movevalues,		"input_movevalues");	\
	globalfloat(input_buttons,			"input_buttons");		\
	\
	globalfloat(movevar_gravity,		"movevar_gravity");		\
	globalfloat(movevar_stopspeed,		"movevar_stopspeed");	\
	globalfloat(movevar_maxspeed,		"movevar_maxspeed");	\
	globalfloat(movevar_spectatormaxspeed,"movevar_spectatormaxspeed");	\
	globalfloat(movevar_accelerate,		"movevar_accelerate");		\
	globalfloat(movevar_airaccelerate,	"movevar_airaccelerate");	\
	globalfloat(movevar_wateraccelerate,"movevar_wateraccelerate");	\
	globalfloat(movevar_friction,		"movevar_friction");		\
	globalfloat(movevar_waterfriction,	"movevar_waterfriction");	\
	globalfloat(movevar_entgravity,		"movevar_entgravity");		\


typedef struct {
#define globalfloat(name,qcname) float *name
#define globalvector(name,qcname) float *name
#define globalentity(name,qcname) int *name
#define globalstring(name,qcname) string_t *name
#define globalfunction(name,qcname) func_t name
//These are the functions the engine will call to, found by name.

	csqcglobals

#undef globalfloat
#undef globalvector
#undef globalentity
#undef globalstring
#undef globalfunction
} csqcglobals_t;
static csqcglobals_t csqcg;

#define plnum 0


static void CSQC_FindGlobals(void)
{
#define globalfloat(name,qcname) csqcg.name = (float*)PR_FindGlobal(csqcprogs, qcname, 0);
#define globalvector(name,qcname) csqcg.name = (float*)PR_FindGlobal(csqcprogs, qcname, 0);
#define globalentity(name,qcname) csqcg.name = (int*)PR_FindGlobal(csqcprogs, qcname, 0);
#define globalstring(name,qcname) csqcg.name = (string_t*)PR_FindGlobal(csqcprogs, qcname, 0);
#define globalfunction(name,qcname) csqcg.name = PR_FindFunction(csqcprogs,qcname,PR_ANY);

	csqcglobals

#undef globalfloat
#undef globalvector
#undef globalentity
#undef globalstring
#undef globalfunction

	if (csqcg.time)
		*csqcg.time = Sys_DoubleTime();

	if (csqcg.player_localentnum)
		*csqcg.player_localentnum = cl.playernum[plnum]+1;

	if (csqcg.maxclients)
		*csqcg.maxclients = MAX_CLIENTS;
}



//this is the list for all the csqc fields.
//(the #define is so the list always matches the ones pulled out)
#define csqcfields	\
	fieldfloat(entnum);		\
	fieldfloat(modelindex);	\
	fieldvector(origin);	\
	fieldvector(angles);	\
	fieldvector(velocity);	\
	fieldfloat(alpha);		/*transparency*/	\
	fieldfloat(scale);		/*model scale*/		\
	fieldfloat(fatness);	/*expand models X units along thier normals.*/	\
	fieldfloat(skin);		\
	fieldfloat(colormap);	\
	fieldfloat(flags);		\
	fieldfloat(frame);		\
	fieldfloat(frame2);		\
	fieldfloat(frame1time);	\
	fieldfloat(frame2time);	\
	fieldfloat(lerpfrac);	\
	fieldfloat(renderflags);\
	fieldfloat(forceshader);\
	fieldfloat(dimension_hit);	\
	fieldfloat(dimension_solid);	\
							\
	fieldfloat(drawmask);	/*So that the qc can specify all rockets at once or all bannanas at once*/	\
	fieldfunction(predraw);	/*If present, is called just before it's drawn.*/	\
							\
	fieldstring(model);		\
	fieldfloat(ideal_yaw);	\
	fieldfloat(ideal_pitch);\
	fieldfloat(yaw_speed);	\
	fieldfloat(pitch_speed);\
							\
	fieldentity(chain);		\
	fieldentity(enemy);		\
	fieldentity(groundentity);	\
	fieldentity(owner);	\
							\
	fieldfloat(solid);		\
	fieldvector(mins);		\
	fieldvector(maxs);		\
	fieldvector(size);		\
	fieldvector(absmin);	\
	fieldvector(absmax);	\
	fieldfloat(hull);		/*(FTE_PEXT_HEXEN2)*/


//note: doesn't even have to match the clprogs.dat :)
typedef struct {
#define fieldfloat(name) float name
#define fieldvector(name) vec3_t name
#define fieldentity(name) int name
#define fieldstring(name) string_t name
#define fieldfunction(name) func_t name
csqcfields
#undef fieldfloat
#undef fieldvector
#undef fieldentity
#undef fieldstring
#undef fieldfunction
} csqcentvars_t;

typedef struct csqcedict_s
{
	qboolean	isfree;
	float		freetime; // sv.time when the object was freed
	int			entnum;
	qboolean	readonly;	//world
	csqcentvars_t	*v;

	//add whatever you wish here
	trailstate_t *trailstate;
	link_t	area;
} csqcedict_t;

static csqcedict_t *csqc_edicts;	//consider this 'world'


static void CSQC_InitFields(void)
{	//CHANGING THIS FUNCTION REQUIRES CHANGES TO csqcentvars_t
#define fieldfloat(name) PR_RegisterFieldVar(csqcprogs, ev_float, #name, (int)&((csqcentvars_t*)0)->name, -1)
#define fieldvector(name) PR_RegisterFieldVar(csqcprogs, ev_vector, #name, (int)&((csqcentvars_t*)0)->name, -1)
#define fieldentity(name) PR_RegisterFieldVar(csqcprogs, ev_entity, #name, (int)&((csqcentvars_t*)0)->name, -1)
#define fieldstring(name) PR_RegisterFieldVar(csqcprogs, ev_string, #name, (int)&((csqcentvars_t*)0)->name, -1)
#define fieldfunction(name) PR_RegisterFieldVar(csqcprogs, ev_function, #name, (int)&((csqcentvars_t*)0)->name, -1)
csqcfields	//any *64->int32 casts are erroneous, it's biased off NULL.
#undef fieldfloat
#undef fieldvector
#undef fieldentity
#undef fieldstring
#undef fieldfunction
}

static csqcedict_t **csqcent;
static int maxcsqcentities;

static int csqcentsize;

static model_t *CSQC_GetModelForIndex(int index);
static void CS_LinkEdict(csqcedict_t *ent, qboolean touchtriggers);

areanode_t	cs_areanodes[AREA_NODES];
int			cs_numareanodes;
areanode_t *CS_CreateAreaNode (int depth, vec3_t mins, vec3_t maxs)
{
	areanode_t	*anode;
	vec3_t		size;
	vec3_t		mins1, maxs1, mins2, maxs2;

	anode = &cs_areanodes[cs_numareanodes];
	cs_numareanodes++;

	ClearLink (&anode->trigger_edicts);
	ClearLink (&anode->solid_edicts);

	if (depth == AREA_DEPTH)
	{
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}

	VectorSubtract (maxs, mins, size);
	if (size[0] > size[1])
		anode->axis = 0;
	else
		anode->axis = 1;

	anode->dist = 0.5 * (maxs[anode->axis] + mins[anode->axis]);
	VectorCopy (mins, mins1);
	VectorCopy (mins, mins2);
	VectorCopy (maxs, maxs1);
	VectorCopy (maxs, maxs2);

	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;

	anode->children[0] = CS_CreateAreaNode (depth+1, mins2, maxs2);
	anode->children[1] = CS_CreateAreaNode (depth+1, mins1, maxs1);

	return anode;
}

void CS_ClearWorld (void)
{
	int i;

	memset (cs_areanodes, 0, sizeof(cs_areanodes));
	cs_numareanodes = 0;
	if (cl.worldmodel)
		CS_CreateAreaNode (0, cl.worldmodel->mins, cl.worldmodel->maxs);
	else
	{
		vec3_t mins, maxs;
		int i;
		for (i = 0; i < 3; i++)
		{
			mins[i] = -4096;
			maxs[i] = 4096;
		}
		CS_CreateAreaNode (0, mins, maxs);
	}

	for (i = 1; i < num_csqc_edicts; i++)
		CS_LinkEdict((csqcedict_t*)EDICT_NUM(csqcprogs, i), false);
}

void CS_UnlinkEdict (csqcedict_t *ent)
{
	if (!ent->area.prev)
		return;		// not linked in anywhere
	RemoveLink (&ent->area);
	ent->area.prev = ent->area.next = NULL;
}

static void CS_LinkEdict(csqcedict_t *ent, qboolean touchtriggers)
{
	areanode_t *node;

	if (ent->area.prev)
		CS_UnlinkEdict (ent);	// unlink from old position

	if (ent == csqc_edicts)
		return;		// don't add the world

	//FIXME: use some sort of area grid ?
	VectorAdd(ent->v->origin, ent->v->mins, ent->v->absmin);
	VectorAdd(ent->v->origin, ent->v->maxs, ent->v->absmax);

	if ((int)ent->v->flags & FL_ITEM)
	{
		ent->v->absmin[0] -= 15;
		ent->v->absmin[1] -= 15;
		ent->v->absmax[0] += 15;
		ent->v->absmax[1] += 15;
	}
	else
	{	// because movement is clipped an epsilon away from an actual edge,
		// we must fully check even when bounding boxes don't quite touch
		ent->v->absmin[0] -= 1;
		ent->v->absmin[1] -= 1;
		ent->v->absmin[2] -= 1;
		ent->v->absmax[0] += 1;
		ent->v->absmax[1] += 1;
		ent->v->absmax[2] += 1;
	}

	if (!ent->v->solid)
		return;

	// find the first node that the ent's box crosses
	node = cs_areanodes;
	while (1)
	{
		if (node->axis == -1)
			break;
		if (ent->v->absmin[node->axis] > node->dist)
			node = node->children[0];
		else if (ent->v->absmax[node->axis] < node->dist)
			node = node->children[1];
		else
			break;		// crosses the node
	}

// link it in

	if (ent->v->solid == SOLID_TRIGGER)
		InsertLinkBefore (&ent->area, &node->trigger_edicts);
	else
		InsertLinkBefore (&ent->area, &node->solid_edicts);
}

typedef struct {
	int type;
	trace_t trace;
	vec3_t boxmins;	//mins/max of total move.
	vec3_t boxmaxs;
	vec3_t start;
	vec3_t end;
	vec3_t mins;	//mins/max of ent
	vec3_t maxs;
	csqcedict_t *passedict;
} moveclip_t;
void CS_ClipToLinks ( areanode_t *node, moveclip_t *clip )
{
	model_t		*model;
	trace_t		tr;
	link_t		*l, *next;
	csqcedict_t		*touch;

	//work out who they are first.
	for (l = node->solid_edicts.next ; l != &node->solid_edicts ; l = next)
	{
		next = l->next;
		touch = (csqcedict_t*)EDICT_FROM_AREA(l);
		if (touch->v->solid == SOLID_NOT)
			continue;
		if (touch == clip->passedict)
			continue;
		if (touch->v->solid == SOLID_TRIGGER || touch->v->solid == SOLID_LADDER)
			continue;

		if (clip->type & MOVE_NOMONSTERS && touch->v->solid != SOLID_BSP)
			continue;

		if (clip->passedict)
		{
			// don't clip corpse against character
			if (clip->passedict->v->solid == SOLID_CORPSE && (touch->v->solid == SOLID_SLIDEBOX || touch->v->solid == SOLID_CORPSE))
				continue;
			// don't clip character against corpse
			if (clip->passedict->v->solid == SOLID_SLIDEBOX && touch->v->solid == SOLID_CORPSE)
				continue;

			if (!((int)clip->passedict->v->dimension_hit & (int)touch->v->dimension_solid))
				continue;
		}

		if (clip->boxmins[0] > touch->v->absmax[0]
		|| clip->boxmins[1] > touch->v->absmax[1]
		|| clip->boxmins[2] > touch->v->absmax[2]
		|| clip->boxmaxs[0] < touch->v->absmin[0]
		|| clip->boxmaxs[1] < touch->v->absmin[1]
		|| clip->boxmaxs[2] < touch->v->absmin[2] )
			continue;

		if (clip->passedict && clip->passedict->v->size[0] && !touch->v->size[0])
			continue;	// points never interact

	// might intersect, so do an exact clip
		if (clip->trace.allsolid)
			return;
		if (clip->passedict)
		{
		 	if ((csqcedict_t*)PROG_TO_EDICT(csqcprogs, touch->v->owner) == clip->passedict)
				continue;	// don't clip against own missiles
			if ((csqcedict_t*)PROG_TO_EDICT(csqcprogs, clip->passedict->v->owner) == touch)
				continue;	// don't clip against owner
		}


		if (!((int)clip->passedict->v->dimension_solid & (int)touch->v->dimension_hit))
			continue;

		model = CSQC_GetModelForIndex(touch->v->modelindex);
		if (!model)
			continue;
		model->funcs.Trace(model, 0, 0, clip->start, clip->end, clip->mins, clip->maxs, &tr);
		if (tr.fraction < clip->trace.fraction)
		{
			tr.ent = (void*)touch;
			clip->trace = tr;
		}
	}
}

static trace_t CS_Move(vec3_t v1, vec3_t mins, vec3_t maxs, vec3_t v2, float nomonsters, csqcedict_t *passedict)
{
	moveclip_t clip;

	if (cl.worldmodel)
	{
		cl.worldmodel->funcs.Trace(cl.worldmodel, 0, 0, v1, v2, mins, maxs, &clip.trace);
		clip.trace.ent = (void*)csqc_edicts;
	}
	else
	{
		memset(&clip.trace, 0, sizeof(clip.trace));
		clip.trace.fraction = 1;
		VectorCopy(v2, clip.trace.endpos);
		clip.trace.ent = (void*)csqc_edicts;
	}

//why use trace.endpos instead?
//so that if we hit a wall early, we don't have a box covering the whole world because of a shotgun trace.
	clip.boxmins[0] = ((v1[0] < clip.trace.endpos[0])?v1[0]:clip.trace.endpos[0]) - mins[0]-1;
	clip.boxmins[1] = ((v1[1] < clip.trace.endpos[1])?v1[1]:clip.trace.endpos[1]) - mins[1]-1;
	clip.boxmins[2] = ((v1[2] < clip.trace.endpos[2])?v1[2]:clip.trace.endpos[2]) - mins[2]-1;
	clip.boxmaxs[0] = ((v1[0] > clip.trace.endpos[0])?v1[0]:clip.trace.endpos[0]) + maxs[0]+1;
	clip.boxmaxs[1] = ((v1[1] > clip.trace.endpos[1])?v1[1]:clip.trace.endpos[1]) + maxs[1]+1;
	clip.boxmaxs[2] = ((v1[2] > clip.trace.endpos[2])?v1[2]:clip.trace.endpos[2]) + maxs[2]+1;

	VectorCopy(mins, clip.mins);
	VectorCopy(maxs, clip.maxs);
	VectorCopy(v1, clip.start);
	VectorCopy(v2, clip.end);
	clip.passedict = passedict;

	CS_ClipToLinks(cs_areanodes, &clip);
	return clip.trace;
}

void CS_CheckVelocity(csqcedict_t *ent)
{
}


static void PF_cs_remove (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ed;

	ed = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);

	if (ed->isfree)
	{
		Con_DPrintf("CSQC Tried removing free entity\n");
		return;
	}

	P_DelinkTrailstate(&ed->trailstate);
	CS_UnlinkEdict(ed);
	ED_Free (prinst, (void*)ed);
}

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

	prinst->RunError(prinst, "\nBuiltin %i not implemented.\nCSQC is not compatable.", prinst->lastcalledbuiltinnumber);
	PR_BIError (prinst, "bulitin not implemented");
}
static void PF_NoCSQC (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Con_Printf("\n");

	prinst->RunError(prinst, "\nBuiltin %i does not make sense in csqc.\nCSQC is not compatable.", prinst->lastcalledbuiltinnumber);
	PR_BIError (prinst, "bulitin not implemented");
}

static void PF_cl_cprint (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *str = PF_VarString(prinst, 0, pr_globals);
	SCR_CenterPrint(0, str);
}

static void PF_cs_makevectors (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (!csqcg.forward || !csqcg.right || !csqcg.up)
		Host_EndGame("PF_makevectors: one of v_forward, v_right or v_up was not defined\n");
	AngleVectors (G_VECTOR(OFS_PARM0), csqcg.forward, csqcg.right, csqcg.up);
}
/*
void QuaternainToAngleMatrix(float *quat, vec3_t *mat)
{
	float xx      = quat[0] * quat[0];
    float xy      = quat[0] * quat[1];
    float xz      = quat[0] * quat[2];
    float xw      = quat[0] * quat[3];
    float yy      = quat[1] * quat[1];
    float yz      = quat[1] * quat[2];
    float yw      = quat[1] * quat[3];
    float zz      = quat[2] * quat[2];
    float zw      = quat[2] * quat[3];
    mat[0][0]  = 1 - 2 * ( yy + zz );
    mat[0][1]  =     2 * ( xy - zw );
    mat[0][2]  =     2 * ( xz + yw );
    mat[1][0]  =     2 * ( xy + zw );
    mat[1][1]  = 1 - 2 * ( xx + zz );
    mat[1][2]  =     2 * ( yz - xw );
    mat[2][0]  =     2 * ( xz - yw );
    mat[2][1]  =     2 * ( yz + xw );
    mat[2][2] = 1 - 2 * ( xx + yy );
}

void quaternion_multiply(float *a, float *b, float *c)
{
#define x1 a[0]
#define y1 a[1]
#define z1 a[2]
#define w1 a[3]
#define x2 b[0]
#define y2 b[1]
#define z2 b[2]
#define w2 b[3]
	c[0] = w1*x2 + x1*w2 + y1*z2 - z1*y2;
	c[1] = w1*y2 + y1*w2 + z1*x2 - x1*z2;
	c[2] = w1*z2 + z1*w2 + x1*y2 - y1*x2;
	c[3] = w1*w2 - x1*x2 - y1*y2 - z1*z2;
}

void quaternion_rotation(float pitch, float roll, float yaw, float angle, float *quat)
{
	float sin_a, cos_a;

	sin_a = sin( angle / 360 );
    cos_a = cos( angle / 360 );
    quat[0]    = pitch	* sin_a;
    quat[1]    = yaw	* sin_a;
    quat[2]    = roll	* sin_a;
    quat[3]    = cos_a;
}

void EularToQuaternian(vec3_t angles, float *quat)
{
  float x[4] = {sin(angles[2]/360), 0, 0, cos(angles[2]/360)};
  float y[4] = {0, sin(angles[1]/360), 0, cos(angles[1]/360)};
  float z[4] = {0, 0, sin(angles[0]/360), cos(angles[0]/360)};
  float t[4];
  quaternion_multiply(x, y, t);
  quaternion_multiply(t, z, quat);
}
*/
#define CSQCRF_VIEWMODEL		1 //Not drawn in mirrors
#define CSQCRF_EXTERNALMODEL	2 //drawn ONLY in mirrors
#define CSQCRF_DEPTHHACK		4 //fun depthhack
#define CSQCRF_ADDITIVE			8 //add instead of blend
#define CSQCRF_USEAXIS			16 //use v_forward/v_right/v_up as an axis/matrix - predraw is needed to use this properly
#define CSQCRF_NOSHADOW			32 //don't cast shadows upon other entities (can still be self shadowing, if the engine wishes, and not additive)

static model_t *CSQC_GetModelForIndex(int index)
{
	if (index == 0)
		return NULL;
	else if (index > 0 && index < MAX_MODELS)
		return cl.model_precache[index];
	else if (index < 0 && index > -MAX_CSQCMODELS)
		return cl.model_csqcprecache[-index];
	else
		return NULL;
}

static qboolean CopyCSQCEdictToEntity(csqcedict_t *in, entity_t *out)
{
	int i;
	model_t *model;

	i = in->v->modelindex;
	model = CSQC_GetModelForIndex(in->v->modelindex);
	if (!model)
		return false; //there might be other ent types later as an extension that stop this.

	if (!model)
	{
		Con_Printf("CopyCSQCEdictToEntity: model wasn't precached!\n");
		return false;
	}

	memset(out, 0, sizeof(*out));
	out->model = model;

	if (in->v->renderflags)
	{
		i = in->v->renderflags;
		if (i & CSQCRF_VIEWMODEL)
			out->flags |= Q2RF_DEPTHHACK|Q2RF_WEAPONMODEL;
		if (i & CSQCRF_EXTERNALMODEL)
			out->flags |= Q2RF_EXTERNALMODEL;
		if (i & CSQCRF_DEPTHHACK)
			out->flags |= Q2RF_DEPTHHACK;
		if (i & CSQCRF_ADDITIVE)
			out->flags |= Q2RF_ADDATIVE;
		//CSQCRF_USEAXIS is below
		if (i & CSQCRF_NOSHADOW)
			out->flags |= RF_NOSHADOW;
	}

	out->frame = in->v->frame;
	out->oldframe = in->v->frame2;
	out->lerpfrac = in->v->lerpfrac;
	VectorCopy(in->v->origin, out->origin);
	if ((int)in->v->renderflags & CSQCRF_USEAXIS)
	{
		VectorCopy(csqcg.forward, out->axis[0]);
		VectorNegate(csqcg.right, out->axis[1]);
		VectorCopy(csqcg.up, out->axis[2]);
		out->scale = 1;
	}
	else
	{
		VectorCopy(in->v->angles, out->angles);
		out->angles[0]*=-1;
		AngleVectors(out->angles, out->axis[0], out->axis[1], out->axis[2]);
		VectorInverse(out->axis[1]);

		if (!in->v->scale)
			out->scale = 1;
		else
			out->scale = in->v->scale;
	}

	out->frame1time = in->v->frame1time;
	out->frame2time = in->v->frame2time;

	if (in->v->colormap > 0 && in->v->colormap <= MAX_CLIENTS)
	{
#ifdef SWQUAKE
		out->palremap = cl.players[(int)in->v->colormap-1].palremap;
#endif
		out->scoreboard = &cl.players[(int)in->v->colormap-1];
	} // TODO: DP COLORMAP extension?

	out->shaderRGBAf[0] = 1;
	out->shaderRGBAf[1] = 1;
	out->shaderRGBAf[2] = 1;
	if (!in->v->alpha)
		out->shaderRGBAf[3] = 1;
	else
		out->shaderRGBAf[3] = in->v->alpha;

	out->skinnum = in->v->skin;
	out->fatness = in->v->fatness;
#ifdef Q3SHADERS
	if (in->v->forceshader >= 1)
		out->forcedshader = r_shaders + ((int)in->v->forceshader-1);
	else
		out->forcedshader = NULL;
#endif

	out->keynum = -1;

	return true;
}

static void PF_cs_makestatic (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{	//still does a remove.
	csqcedict_t *in = (void*)G_EDICT(prinst, OFS_PARM0);
	entity_t *ent;

	if (cl.num_statics >= MAX_STATIC_ENTITIES)
	{
		Con_Printf ("Too many static entities");

		PF_cs_remove(prinst, pr_globals);
		return;
	}

	ent = &cl_static_entities[cl.num_statics];
	if (CopyCSQCEdictToEntity(in, ent))
	{
		cl.num_statics++;
		R_AddEfrags(ent);
	}

	PF_cs_remove(prinst, pr_globals);
}

static void PF_R_AddEntity(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *in = (void*)G_EDICT(prinst, OFS_PARM0);
	entity_t ent;

	if (in->v->predraw)
	{
		int oldself = *csqcg.self;
		*csqcg.self = EDICT_TO_PROG(prinst, (void*)in);
		PR_ExecuteProgram(prinst, in->v->predraw);
		*csqcg.self = oldself;

		if (in->isfree)
			return;	//bummer...
	}

	if (CopyCSQCEdictToEntity(in, &ent))
		V_AddAxisEntity(&ent);

/*
	{
		float a[4];
		float q[4];
		float r[4];
		EularToQuaternian(ent.angles, a);

		QuaternainToAngleMatrix(a, ent.axis);
		ent.origin[0] += 16;
		V_AddEntity(&ent);

		quaternion_rotation(0, 0, 1, cl.time*360, r);
		quaternion_multiply(a, r, q);
		QuaternainToAngleMatrix(q, ent.axis);
		ent.origin[0] -= 32;
		ent.angles[1] = cl.time;
		V_AddEntity(&ent);
	}
*/
}

static void PF_R_AddDynamicLight(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);
	float radius = G_FLOAT(OFS_PARM1);
	float *rgb = G_VECTOR(OFS_PARM2);
	V_AddLight(org, radius, rgb[0]/5, rgb[1]/5, rgb[2]/5);
}

#define MASK_ENGINE 1
static void PF_R_AddEntityMask(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int mask = G_FLOAT(OFS_PARM0);
	csqcedict_t *ent;
	int e;

	for (e=1; e < *prinst->parms->sv_num_edicts; e++)
	{
		ent = (void*)EDICT_NUM(prinst, e);
		if (ent->isfree)
			continue;

		if ((int)ent->v->drawmask & mask)
		{
			G_INT(OFS_PARM0) = EDICT_TO_PROG(prinst, (void*)ent);
			PF_R_AddEntity(prinst, pr_globals);
		}
	}

	if (mask & MASK_ENGINE && cl.worldmodel)
	{
		CL_LinkViewModel ();
		CL_LinkPlayers ();
		CL_LinkPacketEntities ();
		CL_LinkProjectiles ();
		CL_UpdateTEnts ();
	}
}

qboolean csqc_rebuildmatricies;
float mvp[12];
float mvpi[12];
static void buildmatricies(void)
{
	float modelview[16];
	float proj[16];

	Matrix4_ModelViewMatrix(modelview, r_refdef.viewangles, r_refdef.vieworg);
	Matrix4_Projection2(proj, r_refdef.fov_x, r_refdef.fov_y, 4);
	Matrix4_Multiply(proj, modelview, mvp);
	Matrix4_Invert_Simple((matrix4x4_t*)mvpi, (matrix4x4_t*)mvp);	//not actually used in this function.

	csqc_rebuildmatricies = false;
}
static void PF_cs_project (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (csqc_rebuildmatricies)
		buildmatricies();


	{
		float *in = G_VECTOR(OFS_PARM0);
		float *out = G_VECTOR(OFS_RETURN);
		float v[4], tempv[4];

		v[0] = in[0];
		v[1] = in[1];
		v[2] = in[2];
		v[3] = 1;

		Matrix4_Transform4(mvp, v, tempv);

		tempv[0] /= tempv[3];
		tempv[1] /= tempv[3];
		tempv[2] /= tempv[3];

		out[0] = (1+tempv[0])/2;
		out[1] = (1+tempv[1])/2;
		out[2] = (1+tempv[2])/2;

		out[0] = out[0]*r_refdef.vrect.width + r_refdef.vrect.x;
		out[1] = out[1]*r_refdef.vrect.height + r_refdef.vrect.y;
	}
}
static void PF_cs_unproject (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (csqc_rebuildmatricies)
		buildmatricies();


	{
		float *in = G_VECTOR(OFS_PARM0);
		float *out = G_VECTOR(OFS_RETURN);

		float v[4], tempv[4];

		out[0] = (out[0]-r_refdef.vrect.x)/r_refdef.vrect.width;
		out[1] = (out[1]-r_refdef.vrect.y)/r_refdef.vrect.height;

		v[0] = in[0]*2-1;
		v[1] = in[1]*2-1;
		v[2] = in[2]*2-1;
		v[3] = 1;

		Matrix4_Transform4(mvpi, v, tempv);

		out[0] = tempv[0];
		out[1] = tempv[1];
		out[2] = tempv[2];
	}
}

//float CalcFov (float fov_x, float width, float height);
//clear scene, and set up the default stuff.
static void PF_R_ClearScene (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	extern frame_t		*view_frame;
	extern player_state_t		*view_message;

	csqc_rebuildmatricies = true;

	CL_DecayLights ();

	if (cl.worldmodel)
	{
		//work out which packet entities are solid
		CL_SetSolidEntities ();

		// Set up prediction for other players
		CL_SetUpPlayerPrediction(false);

		// do client side motion prediction
		CL_PredictMove ();

		// Set up prediction for other players
		CL_SetUpPlayerPrediction(true);
	}

	CL_SwapEntityLists();

	view_frame = NULL;//&cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK];
	view_message = NULL;//&view_frame->playerstate[cl.playernum[plnum]];
	V_CalcRefdef(0);	//set up the defaults (for player 0)
	/*
	VectorCopy(cl.simangles[0], r_refdef.viewangles);
	VectorCopy(cl.simorg[0], r_refdef.vieworg);
	r_refdef.flags = 0;

	r_refdef.vrect.x = 0;
	r_refdef.vrect.y = 0;
	r_refdef.vrect.width = vid.width;
	r_refdef.vrect.height = vid.height;

	r_refdef.fov_x = scr_fov.value;
	r_refdef.fov_y = CalcFov (r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);
	*/

	csqc_addcrosshair = false;
	csqc_drawsbar = false;
}

typedef enum
{
	VF_MIN = 1,
	VF_MIN_X = 2,
	VF_MIN_Y = 3,
	VF_SIZE = 4,
	VF_SIZE_X = 5,
	VF_SIZE_Y = 6,
	VF_VIEWPORT = 7,
	VF_FOV = 8,
	VF_FOVX = 9,
	VF_FOVY = 10,
	VF_ORIGIN = 11,
	VF_ORIGIN_X = 12,
	VF_ORIGIN_Y = 13,
	VF_ORIGIN_Z = 14,
	VF_ANGLES = 15,
	VF_ANGLES_X = 16,
	VF_ANGLES_Y = 17,
	VF_ANGLES_Z = 18,
	VF_DRAWWORLD = 19,
	VF_ENGINESBAR = 20,
	VF_DRAWCROSSHAIR = 21,
	VF_CARTESIAN_ANGLES = 22,
	VF_PERSPECTIVE = 200
} viewflags;

static void PF_R_SetViewFlag(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	viewflags parametertype = G_FLOAT(OFS_PARM0);
	float *p = G_VECTOR(OFS_PARM1);

	csqc_rebuildmatricies = true;

	G_FLOAT(OFS_RETURN) = 1;
	switch(parametertype)
	{
	case VF_FOV:
		r_refdef.fov_x = p[0];
		r_refdef.fov_y = p[1];
		break;

	case VF_FOVX:
		r_refdef.fov_x = *p;
		break;

	case VF_FOVY:
		r_refdef.fov_y = *p;
		break;

	case VF_ORIGIN:
		VectorCopy(p, r_refdef.vieworg);
		cl.crouch[0] = 0;
		break;

	case VF_ORIGIN_Z:
		cl.crouch[0] = 0;
	case VF_ORIGIN_X:
	case VF_ORIGIN_Y:
		r_refdef.vieworg[parametertype-VF_ORIGIN_X] = *p;
		break;

	case VF_ANGLES:
		VectorCopy(p, r_refdef.viewangles);
		break;
	case VF_ANGLES_X:
	case VF_ANGLES_Y:
	case VF_ANGLES_Z:
		r_refdef.viewangles[parametertype-VF_ANGLES_X] = *p;
		break;

	case VF_CARTESIAN_ANGLES:
		Con_Printf(S_WARNING "WARNING: CARTESIAN ANGLES ARE NOT YET SUPPORTED!\n");
		break;

	case VF_VIEWPORT:
		r_refdef.vrect.x = p[0];
		r_refdef.vrect.y = p[1];
		p+=3;
		r_refdef.vrect.width = p[0];
		r_refdef.vrect.height = p[1];
		break;

	case VF_SIZE_X:
		r_refdef.vrect.width = *p;
		break;
	case VF_SIZE_Y:
		r_refdef.vrect.height = *p;
		break;
	case VF_SIZE:
		r_refdef.vrect.width = p[0];
		r_refdef.vrect.height = p[1];
		break;

	case VF_MIN_X:
		r_refdef.vrect.x = *p;
		break;
	case VF_MIN_Y:
		r_refdef.vrect.y = *p;
		break;
	case VF_MIN:
		r_refdef.vrect.x = p[0];
		r_refdef.vrect.y = p[1];
		break;

	case VF_DRAWWORLD:
		r_refdef.flags = (r_refdef.flags&~Q2RDF_NOWORLDMODEL) | (*p?0:Q2RDF_NOWORLDMODEL);
		break;
	case VF_ENGINESBAR:
		csqc_drawsbar = *p;
		break;
	case VF_DRAWCROSSHAIR:
		csqc_addcrosshair = *p;
		break;

	case VF_PERSPECTIVE:
		r_refdef.useperspective = *p;
		break;

	default:
		Con_DPrintf("SetViewFlag: %i not recognised\n", parametertype);
		G_FLOAT(OFS_RETURN) = 0;
		break;
	}
}

static void PF_R_RenderScene(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (cl.worldmodel)
		R_PushDlights ();

#ifdef RGLQUAKE
	if (qrenderer == QR_OPENGL)
	{
		gl_ztrickdisabled|=16;
		qglDisable(GL_ALPHA_TEST);
		qglDisable(GL_BLEND);
	}
#endif

	VectorCopy (r_refdef.vieworg, cl.viewent[0].origin);
	CalcGunAngle(0);

	R_RenderView();

#ifdef RGLQUAKE
	if (qrenderer == QR_OPENGL)
	{
		gl_ztrickdisabled&=~16;
		GL_Set2D ();
		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_TexEnv(GL_MODULATE);
	}
#endif

#ifdef RGLQUAKE
	if (qrenderer == QR_OPENGL)
	{
		qglDisable(GL_ALPHA_TEST);
		qglEnable(GL_BLEND);
	}
#endif

	vid.recalc_refdef = 1;

	if (csqc_drawsbar)
	{
#ifdef PLUGINS
		Plug_SBar();
#else
		if (Sbar_ShouldDraw())
		{
			Sbar_Draw ();
			Sbar_DrawScoreboard ();
		}
#endif
	}

	if (csqc_addcrosshair)
		Draw_Crosshair();
}

static void PF_cs_getstatf(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int stnum = G_FLOAT(OFS_PARM0);
	float val = *(float*)&cl.stats[plnum][stnum];	//copy float into the stat
	G_FLOAT(OFS_RETURN) = val;
}
static void PF_cs_getstati(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{	//convert an int stat into a qc float.

	int stnum = G_FLOAT(OFS_PARM0);
	int val = cl.stats[plnum][stnum];
	if (*prinst->callargc > 1)
	{
		int first, count;
		first = G_FLOAT(OFS_PARM1);
		count = G_FLOAT(OFS_PARM2);
		G_FLOAT(OFS_RETURN) = (((unsigned int)val)&(((1<<count)-1)<<first))>>first;
	}
	else
		G_FLOAT(OFS_RETURN) = val;
}
static void PF_cs_getstats(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int stnum = G_FLOAT(OFS_PARM0);
	char out[8];

	//the network protocol byteswaps

	((unsigned int*)out)[0] = LittleLong(cl.stats[0][stnum+0]);
	((unsigned int*)out)[1] = LittleLong(cl.stats[0][stnum+1]);
	((unsigned int*)out)[2] = LittleLong(cl.stats[0][stnum+2]);
	((unsigned int*)out)[3] = LittleLong(cl.stats[0][stnum+3]);
	((unsigned int*)out)[4] = 0;	//make sure it's null terminated

	RETURN_TSTRING(out);
}

static void PF_cs_SetOrigin(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (void*)G_EDICT(prinst, OFS_PARM0);
	float *org = G_VECTOR(OFS_PARM1);

	VectorCopy(org, ent->v->origin);

	CS_LinkEdict(ent, false);
}

static void PF_cs_SetSize(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (void*)G_EDICT(prinst, OFS_PARM0);
	float *mins = G_VECTOR(OFS_PARM1);
	float *maxs = G_VECTOR(OFS_PARM2);

	VectorCopy(mins, ent->v->mins);
	VectorCopy(maxs, ent->v->maxs);

	CS_LinkEdict(ent, false);
}

static void cs_settracevars(trace_t *tr)
{
	*csqcg.trace_allsolid = tr->allsolid;
	*csqcg.trace_startsolid = tr->startsolid;
	*csqcg.trace_fraction = tr->fraction;
	*csqcg.trace_inwater = tr->inwater;
	*csqcg.trace_inopen = tr->inopen;
	VectorCopy (tr->endpos, csqcg.trace_endpos);
	VectorCopy (tr->plane.normal, csqcg.trace_plane_normal);
	*csqcg.trace_plane_dist =  tr->plane.dist;
	if (csqcg.trace_surfaceflags)
		*csqcg.trace_surfaceflags = tr->surface?tr->surface->flags:0;
	if (csqcg.trace_endcontents)
		*csqcg.trace_endcontents = tr->contents;
	if (tr->ent)
		*csqcg.trace_ent = EDICT_TO_PROG(csqcprogs, (void*)tr->ent);
	else
		*csqcg.trace_ent = EDICT_TO_PROG(csqcprogs, (void*)csqc_edicts);
}

static void PF_cs_traceline(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	*v1, *v2, *mins, *maxs;
	trace_t	trace;
	int		nomonsters;
	csqcedict_t	*ent;
	int savedhull;

	v1 = G_VECTOR(OFS_PARM0);
	v2 = G_VECTOR(OFS_PARM1);
	nomonsters = G_FLOAT(OFS_PARM2);
	ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM3);

//	if (*prinst->callargc == 6)
//	{
//		mins = G_VECTOR(OFS_PARM4);
//		maxs = G_VECTOR(OFS_PARM5);
//	}
//	else
	{
		mins = vec3_origin;
		maxs = vec3_origin;
	}

	savedhull = ent->v->hull;
	ent->v->hull = 0;
	trace = CS_Move (v1, mins, maxs, v2, nomonsters, ent);
	ent->v->hull = savedhull;

	cs_settracevars(&trace);
}
static void PF_cs_tracebox(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	*v1, *v2, *mins, *maxs;
	trace_t	trace;
	int		nomonsters;
	csqcedict_t	*ent;
	int savedhull;

	v1 = G_VECTOR(OFS_PARM0);
	mins = G_VECTOR(OFS_PARM1);
	maxs = G_VECTOR(OFS_PARM2);
	v2 = G_VECTOR(OFS_PARM3);
	nomonsters = G_FLOAT(OFS_PARM4);
	ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM5);

	savedhull = ent->v->hull;
	ent->v->hull = 0;
	trace = CS_Move (v1, mins, maxs, v2, nomonsters, ent);
	ent->v->hull = savedhull;

	*csqcg.trace_allsolid = trace.allsolid;
	*csqcg.trace_startsolid = trace.startsolid;
	*csqcg.trace_fraction = trace.fraction;
	*csqcg.trace_inwater = trace.inwater;
	*csqcg.trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, csqcg.trace_endpos);
	VectorCopy (trace.plane.normal, csqcg.trace_plane_normal);
	*csqcg.trace_plane_dist =  trace.plane.dist;
	if (trace.ent)
		*csqcg.trace_ent = EDICT_TO_PROG(prinst, (void*)trace.ent);
	else
		*csqcg.trace_ent = EDICT_TO_PROG(prinst, (void*)csqc_edicts);
}

static trace_t CS_Trace_Toss (csqcedict_t *tossent, csqcedict_t *ignore)
{
	int i;
	int savedhull;
	float gravity;
	vec3_t move, end;
	trace_t trace;
//	float maxvel = Cvar_Get("sv_maxvelocity", "2000", 0, "CSQC physics")->value;

	vec3_t origin, velocity;

	// this has to fetch the field from the original edict, since our copy is truncated
	gravity = 1;//tossent->v->gravity;
	if (!gravity)
		gravity = 1.0;
	gravity *= Cvar_Get("sv_gravity", "800", 0, "CSQC physics")->value * 0.05;

	VectorCopy (tossent->v->origin, origin);
	VectorCopy (tossent->v->velocity, velocity);

	CS_CheckVelocity (tossent);

	savedhull = tossent->v->hull;
	tossent->v->hull = 0;
	for (i = 0;i < 200;i++) // LordHavoc: sanity check; never trace more than 10 seconds
	{
		velocity[2] -= gravity;
		VectorScale (velocity, 0.05, move);
		VectorAdd (origin, move, end);
		trace = CS_Move (origin, tossent->v->mins, tossent->v->maxs, end, MOVE_NORMAL, tossent);
		VectorCopy (trace.endpos, origin);

		CS_CheckVelocity (tossent);

		if (trace.fraction < 1 && trace.ent && (void*)trace.ent != ignore)
			break;
	}
	tossent->v->hull = savedhull;

	trace.fraction = 0; // not relevant
	return trace;
}
static void PF_cs_tracetoss (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	trace_t	trace;
	csqcedict_t	*ent;
	csqcedict_t	*ignore;

	ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	if (ent == csqc_edicts)
		Con_DPrintf("tracetoss: can not use world entity\n");
	ignore = (csqcedict_t*)G_EDICT(prinst, OFS_PARM1);

	trace = CS_Trace_Toss (ent, ignore);

	*csqcg.trace_allsolid = trace.allsolid;
	*csqcg.trace_startsolid = trace.startsolid;
	*csqcg.trace_fraction = trace.fraction;
	*csqcg.trace_inwater = trace.inwater;
	*csqcg.trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, csqcg.trace_endpos);
	VectorCopy (trace.plane.normal, csqcg.trace_plane_normal);
	pr_global_struct->trace_plane_dist =  trace.plane.dist;
	if (trace.ent)
		*csqcg.trace_ent = EDICT_TO_PROG(prinst, trace.ent);
	else
		*csqcg.trace_ent = EDICT_TO_PROG(prinst, (void*)csqc_edicts);
}

static int CS_PointContents(vec3_t org)
{
	if (!cl.worldmodel)
		return FTECONTENTS_EMPTY;
	return cl.worldmodel->funcs.PointContents(cl.worldmodel, org);
}
static void PF_cs_pointcontents(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	*v;
	int cont;

	v = G_VECTOR(OFS_PARM0);

	cont = CS_PointContents(v);
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

static int FindModel(char *name, int *free)
{
	int i;

	*free = 0;

	if (!name || !*name)
		return 0;

	for (i = 1; i < MAX_CSQCMODELS; i++)
	{
		if (!*cl.model_csqcname[i])
		{
			*free = -i;
			break;
		}
		if (!strcmp(cl.model_csqcname[i], name))
			return -i;
	}
	for (i = 1; i < MAX_MODELS; i++)
	{
		if (!strcmp(cl.model_name[i], name))
			return i;
	}
	return 0;
}

static void csqc_setmodel(progfuncs_t *prinst, csqcedict_t *ent, int modelindex)
{
	model_t *model;

	ent->v->modelindex = modelindex;
	if (modelindex < 0)
	{
		ent->v->model = PR_SetString(prinst, cl.model_csqcname[-modelindex]);
		model = cl.model_csqcprecache[-modelindex];
	}
	else
	{
		ent->v->model = PR_SetString(prinst, cl.model_name[modelindex]);
		model = cl.model_precache[modelindex];
	}
	if (model)
	{
		VectorCopy(model->mins, ent->v->mins);
		VectorCopy(model->maxs, ent->v->maxs);
	}
	else
	{
		VectorClear(ent->v->mins);
		VectorClear(ent->v->maxs);
	}
}

static void PF_cs_SetModel(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (void*)G_EDICT(prinst, OFS_PARM0);
	char *modelname = PR_GetStringOfs(prinst, OFS_PARM1);
	int freei;
	int modelindex = FindModel(modelname, &freei);

	if (!modelindex && modelname && *modelname)
	{
		if (!freei)
			Host_EndGame("CSQC ran out of model slots\n");
		Con_DPrintf("Late caching model \"%s\"\n", modelname);
		Q_strncpyz(cl.model_csqcname[-freei], modelname, sizeof(cl.model_csqcname[-freei]));	//allocate a slot now
		modelindex = freei;

		cl.model_csqcprecache[-freei] = Mod_ForName(cl.model_csqcname[-freei], false);
	}

	csqc_setmodel(prinst, ent, modelindex);
}
static void PF_cs_SetModelIndex(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (void*)G_EDICT(prinst, OFS_PARM0);
	int modelindex = G_FLOAT(OFS_PARM1);

	csqc_setmodel(prinst, ent, modelindex);
}
static void PF_cs_PrecacheModel(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int modelindex, freei;
	char *modelname = PR_GetStringOfs(prinst, OFS_PARM0);
	int i;

	for (i = 1; i < MAX_MODELS; i++)	//Make sure that the server specified model is loaded..
	{
		if (!*cl.model_name[i])
			break;
		if (!strcmp(cl.model_name[i], modelname))
		{
			cl.model_precache[i] = Mod_ForName(cl.model_name[i], false);
			break;
		}
	}

	modelindex = FindModel(modelname, &freei);	//now load it

	if (!modelindex)
	{
		if (!freei)
			Host_EndGame("CSQC ran out of model slots\n");
		Q_strncpyz(cl.model_csqcname[-freei], modelname, sizeof(cl.model_csqcname[-freei]));	//allocate a slot now
		modelindex = freei;

		cl.model_csqcprecache[-freei] = Mod_ForName(cl.model_csqcname[-freei], false);
	}

	G_FLOAT(OFS_RETURN) = modelindex;
}
static void PF_cs_PrecacheSound(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *soundname = PR_GetStringOfs(prinst, OFS_PARM0);
	S_PrecacheSound(soundname);
}

static void PF_cs_ModelnameForIndex(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int modelindex = G_FLOAT(OFS_PARM0);

	if (modelindex < 0)
		G_INT(OFS_RETURN) = (int)PR_SetString(prinst, cl.model_csqcname[-modelindex]);
	else
		G_INT(OFS_RETURN) = (int)PR_SetString(prinst, cl.model_name[modelindex]);
}

static void PF_ReadByte(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = MSG_ReadByte();
}

static void PF_ReadChar(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = MSG_ReadChar();
}

static void PF_ReadShort(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = MSG_ReadShort();
}

static void PF_ReadLong(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = MSG_ReadLong();
}

static void PF_ReadCoord(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = MSG_ReadCoord();
}

static void PF_ReadFloat(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = MSG_ReadFloat();
}

static void PF_ReadString(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *read = MSG_ReadString();

	RETURN_TSTRING(read);
}

static void PF_ReadAngle(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = MSG_ReadAngle();
}


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

static void PF_cs_setsensativityscaler (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	in_sensitivityscale = G_FLOAT(OFS_PARM0);
}

static void PF_cs_pointparticles (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int effectnum = G_FLOAT(OFS_PARM0)-1;
	float *org = G_VECTOR(OFS_PARM1);
	float *vel = G_VECTOR(OFS_PARM2);
	float count = G_FLOAT(OFS_PARM3);

	if (*prinst->callargc < 3)
		vel = vec3_origin;
	if (*prinst->callargc < 4)
		count = 1;

	P_RunParticleEffectType(org, vel, count, effectnum);
}

static void PF_cs_trailparticles (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int efnum = G_FLOAT(OFS_PARM0)-1;
	csqcedict_t *ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM1);
	float *start = G_VECTOR(OFS_PARM2);
	float *end = G_VECTOR(OFS_PARM3);

	if (!ent->entnum)	//world trails are non-state-based.
		P_ParticleTrail(start, end, efnum, NULL);
	else
		P_ParticleTrail(start, end, efnum, &ent->trailstate);
}

static void PF_cs_particlesloaded (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *effectname = PR_GetStringOfs(prinst, OFS_PARM0);
	G_FLOAT(OFS_RETURN) = P_DescriptionIsLoaded(effectname);
}

//get the input commands, and stuff them into some globals.
static void PF_cs_getinputstate (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int f;
	usercmd_t *cmd;

	f = G_FLOAT(OFS_PARM0);
	if (f >= cls.netchan.outgoing_sequence)
	{
		G_FLOAT(OFS_RETURN) = false;
		return;
	}
	if (f < cls.netchan.outgoing_sequence - UPDATE_MASK || f < 0)
	{
		G_FLOAT(OFS_RETURN) = false;
		return;
	}

	// save this command off for prediction
	cmd = &cl.frames[f&UPDATE_MASK].cmd[plnum];

	if (csqcg.input_timelength)
		*csqcg.input_timelength = cmd->msec/1000.0f;
	if (csqcg.input_angles)
	{
		csqcg.input_angles[0] = SHORT2ANGLE(cmd->angles[0]+0.5);
		csqcg.input_angles[1] = SHORT2ANGLE(cmd->angles[1]+0.5);
		csqcg.input_angles[2] = SHORT2ANGLE(cmd->angles[2]+0.5);
	}
	if (csqcg.input_movevalues)
	{
		csqcg.input_movevalues[0] = cmd->forwardmove;
		csqcg.input_movevalues[1] = cmd->sidemove;
		csqcg.input_movevalues[2] = cmd->upmove;
	}
	if (csqcg.input_buttons)
		*csqcg.input_buttons = cmd->buttons;

	G_FLOAT(OFS_RETURN) = true;
}
#define ANGLE2SHORT(x) ((x/360.0)*65535)
//read lots of globals, run the default player physics, write lots of globals.
static void PF_cs_runplayerphysics (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	unsigned int msecs;
	extern vec3_t	player_mins;
	extern vec3_t	player_maxs;

	if (!cl.worldmodel)
		return;	//urm..
/*
	int			sequence;	// just for debugging prints

	// player state
	vec3_t		origin;
	vec3_t		angles;
	vec3_t		velocity;
	qboolean		jump_held;
	int			jump_msec;	// msec since last jump
	float		waterjumptime;
	int			pm_type;
	int			hullnum;

	// world state
	int			numphysent;
	physent_t	physents[MAX_PHYSENTS];	// 0 should be the world

	// input
	usercmd_t	cmd;

	qboolean onladder;

	// results
	int			numtouch;
	int			touchindex[MAX_PHYSENTS];
	qboolean		onground;
	int			groundent;		// index in physents array, only valid
								// when onground is true
	int			waterlevel;
	int			watertype;
} playermove_t;

typedef struct {
	float	gravity;
	float	stopspeed;
	float	maxspeed;
	float	spectatormaxspeed;
	float	accelerate;
	float	airaccelerate;
	float	wateraccelerate;
	float	friction;
	float	waterfriction;
	float	entgravity;
	float	bunnyspeedcap;
	float	ktjump;
	qboolean	slidefix;
	qboolean	airstep;
	qboolean	walljump;


	*/

	pmove.sequence = *csqcg.clientcommandframe;
	pmove.pm_type = PM_NORMAL;

	pmove.jump_msec = 0;//(cls.z_ext & Z_EXT_PM_TYPE) ? 0 : from->jump_msec;
	if (csqcg.pmove_jump_held)
		pmove.jump_held = *csqcg.pmove_jump_held;
	if (csqcg.pmove_waterjumptime)
		pmove.waterjumptime = *csqcg.pmove_waterjumptime;

//set up the movement command
	msecs = *csqcg.input_timelength*1000 + 0.5f;
	//precision inaccuracies. :(
	pmove.cmd.angles[0] = ANGLE2SHORT(csqcg.input_angles[0]);
	pmove.cmd.angles[1] = ANGLE2SHORT(csqcg.input_angles[1]);
	pmove.cmd.angles[2] = ANGLE2SHORT(csqcg.input_angles[2]);
	VectorCopy(csqcg.input_angles, pmove.angles);

	pmove.cmd.forwardmove = csqcg.input_movevalues[0];
	pmove.cmd.sidemove = csqcg.input_movevalues[1];
	pmove.cmd.upmove = csqcg.input_movevalues[2];
	pmove.cmd.buttons = *csqcg.input_buttons;

	VectorCopy(csqcg.pmove_org, pmove.origin);
	VectorCopy(csqcg.pmove_vel, pmove.velocity);
	VectorCopy(csqcg.pmove_maxs, player_maxs);
	VectorCopy(csqcg.pmove_mins, player_mins);
	pmove.hullnum = 1;

	CL_SetSolidEntities();



	while(msecs)	//break up longer commands
	{
		pmove.cmd.msec = msecs;
		if (pmove.cmd.msec > 50)
			pmove.cmd.msec = 50;
		msecs -= pmove.cmd.msec;
		PM_PlayerMove(1);
	}

	if (csqcg.pmove_jump_held)
		*csqcg.pmove_jump_held = pmove.jump_held;
	if (csqcg.pmove_waterjumptime)
		*csqcg.pmove_waterjumptime = pmove.waterjumptime;
	VectorCopy(pmove.origin, csqcg.pmove_org);
	VectorCopy(pmove.velocity, csqcg.pmove_vel);
}

static void CheckSendPings(void)
{	//quakeworld sends a 'pings' client command to retrieve the frequently updating stuff
	if (realtime - cl.last_ping_request > 2)
	{
		cl.last_ping_request = realtime;
		CL_SendClientCommand(false, "pings");
	}
}

static void PF_cs_serverkey (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *keyname = PF_VarString(prinst, 0, pr_globals);
	char *ret;

	if (!strcmp(keyname, "ip"))
		ret = NET_AdrToString(cls.netchan.remote_address);
	else if (!strcmp(keyname, "protocol"))
	{	//using this is pretty acedemic, really. Not particuarly portable.
		switch (cls.protocol)
		{	//a tokenizable string
			//first is the base game qw/nq
			//second is branch (custom engine name)
			//third is protocol version.
		default:
		case CP_UNKNOWN:
			ret = "Unknown";
			break;
		case CP_QUAKEWORLD:
			if (cls.fteprotocolextensions)
				ret = "QuakeWorld FTE";
			else if (cls.z_ext)
				ret = "QuakeWorld ZQuake";
			else
				ret = "QuakeWorld";
			break;
		case CP_NETQUAKE:
			switch (nq_dp_protocol)
			{
			default:
				ret = "NetQuake";
				break;
			case 5:
				ret = "NetQuake DarkPlaces 5";
				break;
			case 6:
				ret = "NetQuake DarkPlaces 6";
				break;
			case 7:
				ret = "NetQuake DarkPlaces 7";
				break;
			}
			break;
		case CP_QUAKE2:
			ret = "Quake2";
			break;
		case CP_QUAKE3:
			ret = "Quake3";
			break;
		case CP_PLUGIN:
			ret = "External";
			break;
		}
	}
	else
	{
		ret = Info_ValueForKey(cl.serverinfo, keyname);
	}

	if (*ret)
		RETURN_TSTRING(ret);
	else
		G_INT(OFS_RETURN) = 0;
}

//string(float pnum, string keyname)
static void PF_cs_getplayerkey (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char buffer[64];
	char *ret;
	int pnum = G_FLOAT(OFS_PARM0);
	char *keyname = PR_GetStringOfs(prinst, OFS_PARM1);
	if (pnum < 0)
	{
		if (csqc_resortfrags)
		{
			Sbar_SortFrags(false);
			csqc_resortfrags = false;
		}
		if (pnum >= -scoreboardlines)
		{//sort by
			pnum = fragsort[-(pnum+1)];
		}
	}

	if (pnum < 0 || pnum >= MAX_CLIENTS)
		ret = "";
	else if (!*cl.players[pnum].userinfo)
		ret = "";	//player isn't on the server.
	else if (!strcmp(keyname, "ping"))
	{
		CheckSendPings();

		ret = buffer;
		sprintf(ret, "%i", cl.players[pnum].ping);
	}
	else if (!strcmp(keyname, "frags"))
	{
		ret = buffer;
		sprintf(ret, "%i", cl.players[pnum].frags);
	}
	else if (!strcmp(keyname, "pl"))	//packet loss
	{
		CheckSendPings();

		ret = buffer;
		sprintf(ret, "%i", cl.players[pnum].pl);
	}
	else if (!strcmp(keyname, "entertime"))	//packet loss
	{
		ret = buffer;
		sprintf(ret, "%i", (int)cl.players[pnum].entertime);
	}
	else
	{
		ret = Info_ValueForKey(cl.players[pnum].userinfo, keyname);
	}
	if (*ret)
		RETURN_TSTRING(ret);
	else
		G_INT(OFS_RETURN) = 0;
}

#define lh_extension_t void
lh_extension_t *checkfteextensionsv(char *name);
lh_extension_t *checkextension(char *name);

static void PF_checkextension (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *extname = PR_GetStringOfs(prinst, OFS_PARM0);
	G_FLOAT(OFS_RETURN) = checkextension(extname) || checkfteextensionsv(extname);
}

void PF_cs_sound(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char		*sample;
	int			channel;
	csqcedict_t		*entity;
	float volume;
	float attenuation;

	sfx_t *sfx;

	entity = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	channel = G_FLOAT(OFS_PARM1);
	sample = PR_GetStringOfs(prinst, OFS_PARM2);
	volume = G_FLOAT(OFS_PARM3);
	attenuation = G_FLOAT(OFS_PARM4);

	sfx = S_PrecacheSound(sample);
	if (sfx)
		S_StartSound(-entity->entnum, channel, sfx, entity->v->origin, volume, attenuation);
};

static void PF_cs_particle(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);
	float *dir = G_VECTOR(OFS_PARM1);
	float colour = G_FLOAT(OFS_PARM2);
	float count = G_FLOAT(OFS_PARM2);

	P_RunParticleEffect(org, dir, colour, count);
}
static void PF_cs_particle2(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float		*org, *dmin, *dmax;
	float		colour;
	float		count;
	float    effect;

	org = G_VECTOR(OFS_PARM0);
	dmin = G_VECTOR(OFS_PARM1);
	dmax = G_VECTOR(OFS_PARM2);
	colour = G_FLOAT(OFS_PARM3);
	effect = G_FLOAT(OFS_PARM4);
	count = G_FLOAT(OFS_PARM5);

	P_RunParticleEffect2 (org, dmin, dmax, colour, effect, count);
}

static void PF_cs_particle3(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float		*org, *box;
	float		colour;
	float		count;
	float    effect;

	org = G_VECTOR(OFS_PARM0);
	box = G_VECTOR(OFS_PARM1);
	colour = G_FLOAT(OFS_PARM2);
	effect = G_FLOAT(OFS_PARM3);
	count = G_FLOAT(OFS_PARM4);

	P_RunParticleEffect3(org, box, colour, effect, count);
}

static void PF_cs_particle4(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float		*org;
	float		radius;
	float		colour;
	float		count;
	float    effect;

	org = G_VECTOR(OFS_PARM0);
	radius = G_FLOAT(OFS_PARM1);
	colour = G_FLOAT(OFS_PARM2);
	effect = G_FLOAT(OFS_PARM3);
	count = G_FLOAT(OFS_PARM4);

	P_RunParticleEffect4(org, radius, colour, effect, count);
}


void CL_SpawnSpriteEffect(vec3_t org, model_t *model, int startframe, int framecount, int framerate);
void PF_cl_effect(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);
	char *name = PR_GetStringOfs(prinst, OFS_PARM1);
	float startframe = G_FLOAT(OFS_PARM2);
	float endframe = G_FLOAT(OFS_PARM3);
	float framerate = G_FLOAT(OFS_PARM4);
	model_t *mdl;

	mdl = Mod_ForName(name, false);
	if (mdl)
		CL_SpawnSpriteEffect(org, mdl, startframe, endframe, framerate);
	else
		Con_Printf("PF_cl_effect: Couldn't load model %s\n", name);
}

void PF_cl_ambientsound(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char		*samp;
	float		*pos;
	float 		vol, attenuation;

	pos = G_VECTOR (OFS_PARM0);
	samp = PR_GetStringOfs(prinst, OFS_PARM1);
	vol = G_FLOAT(OFS_PARM2);
	attenuation = G_FLOAT(OFS_PARM3);

	S_StaticSound (S_PrecacheSound (samp), pos, vol, attenuation);
}

static void PF_cs_vectorvectors (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	VectorCopy(G_VECTOR(OFS_PARM0), csqcg.forward);
	VectorNormalize(csqcg.forward);
	VectorVectors(csqcg.forward, csqcg.right, csqcg.up);
}

static void PF_cs_lightstyle (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int stnum = G_FLOAT(OFS_PARM0);
	char *str = PR_GetStringOfs(prinst, OFS_PARM1);
	int colourflags = 7;

	if ((unsigned)stnum >= MAX_LIGHTSTYLES)
	{
		Con_Printf ("PF_cs_lightstyle: stnum > MAX_LIGHTSTYLES");
		return;
	}
	cl_lightstyle[stnum].colour = colourflags;
	Q_strncpyz (cl_lightstyle[stnum].map,  str, sizeof(cl_lightstyle[stnum].map));
	cl_lightstyle[stnum].length = Q_strlen(cl_lightstyle[stnum].map);
}

void PF_cs_changeyaw (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t		*ent;
	float		ideal, current, move, speed;

	ent = (void*)PROG_TO_EDICT(prinst, *csqcg.self);
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
void PF_cs_changepitch (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t		*ent;
	float		ideal, current, move, speed;

	ent = (void*)PROG_TO_EDICT(prinst, *csqcg.self);
	current = anglemod( ent->v->angles[0] );
	ideal = ent->v->ideal_pitch;
	speed = ent->v->pitch_speed;

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

	ent->v->angles[0] = anglemod (current + move);
}

static void PF_cs_findradius (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t	*ent, *chain;
	float	rad;
	float	*org;
	vec3_t	eorg;
	int		i, j;

	chain = (csqcedict_t *)sv.edicts;

	org = G_VECTOR(OFS_PARM0);
	rad = G_FLOAT(OFS_PARM1);

	for (i=1 ; i<sv.num_edicts ; i++)
	{
		ent = (void*)EDICT_NUM(prinst, i);
		if (ent->isfree)
			continue;
		if (ent->v->solid == SOLID_NOT)
			continue;
		for (j=0 ; j<3 ; j++)
			eorg[j] = org[j] - (ent->v->origin[j] + (ent->v->mins[j] + ent->v->maxs[j])*0.5);
		if (Length(eorg) > rad)
			continue;

		ent->v->chain = EDICT_TO_PROG(prinst, (void*)chain);
		chain = ent;
	}

	RETURN_EDICT(prinst, (void*)chain);
}

static void PF_cl_te_gunshot (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	float scaler = 1;
	if (*prinst->callargc >= 2)	//fte is a quakeworld engine
		scaler = G_FLOAT(OFS_PARM1);
	if (P_RunParticleEffectType(pos, NULL, scaler, pt_gunshot))
		P_RunParticleEffect (pos, vec3_origin, 0, 20*scaler);
}
static void PF_cl_te_bloodqw (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	float scaler = 1;
	if (*prinst->callargc >= 2)	//fte is a quakeworld engine
		scaler = G_FLOAT(OFS_PARM1);
	if (P_RunParticleEffectType(pos, NULL, scaler, pt_blood))
		P_RunParticleEffect (pos, vec3_origin, 73, 20*scaler);
}
static void PF_cl_te_blooddp (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	float *dir = G_VECTOR(OFS_PARM1);
	float scaler = G_FLOAT(OFS_PARM2);

	if (P_RunParticleEffectType(pos, dir, scaler, pt_blood))
		P_RunParticleEffect (pos, dir, 73, 20*scaler);
}
static void PF_cl_te_lightningblood (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	if (P_RunParticleEffectType(pos, NULL, 1, pt_lightningblood))
		P_RunParticleEffect (pos, vec3_origin, 225, 50);
}
static void PF_cl_te_spike (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	if (P_RunParticleEffectType(pos, NULL, 1, pt_spike))
		if (P_RunParticleEffectType(pos, NULL, 10, pt_gunshot))
			P_RunParticleEffect (pos, vec3_origin, 0, 10);
}
static void PF_cl_te_superspike (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	if (P_RunParticleEffectType(pos, NULL, 1, pt_superspike))
		if (P_RunParticleEffectType(pos, NULL, 2, pt_spike))
			if (P_RunParticleEffectType(pos, NULL, 20, pt_gunshot))
				P_RunParticleEffect (pos, vec3_origin, 0, 20);
}
static void PF_cl_te_explosion (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);

	// light
	if (r_explosionlight.value) {
		dlight_t *dl;

		dl = CL_AllocDlight (0);
		VectorCopy (pos, dl->origin);
		dl->radius = 150 + r_explosionlight.value*200;
		dl->die = cl.time + 1;
		dl->decay = 300;

		dl->color[0] = 0.2;
		dl->color[1] = 0.155;
		dl->color[2] = 0.05;
		dl->channelfade[0] = 0.196;
		dl->channelfade[1] = 0.23;
		dl->channelfade[2] = 0.12;
	}

	if (P_RunParticleEffectType(pos, NULL, 1, pt_explosion))
		P_RunParticleEffect(pos, NULL, 107, 1024); // should be 97-111

	R_AddStain(pos, -1, -1, -1, 100);

	S_StartSound (-2, 0, cl_sfx_r_exp3, pos, 1, 1);
}
static void PF_cl_te_tarexplosion (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	P_BlobExplosion (pos);

	S_StartSound (-2, 0, cl_sfx_r_exp3, pos, 1, 1);
}
static void PF_cl_te_wizspike (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	if (P_RunParticleEffectType(pos, NULL, 1, pt_wizspike))
		P_RunParticleEffect (pos, vec3_origin, 20, 30);

	S_StartSound (-2, 0, cl_sfx_knighthit, pos, 1, 1);
}
static void PF_cl_te_knightspike (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	if (P_RunParticleEffectType(pos, NULL, 1, pt_knightspike))
		P_RunParticleEffect (pos, vec3_origin, 226, 20);

	S_StartSound (-2, 0, cl_sfx_knighthit, pos, 1, 1);
}
static void PF_cl_te_lavasplash (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	P_LavaSplash (pos);
}
static void PF_cl_te_teleport (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	P_RunParticleEffectType(pos, NULL, 1, pt_teleportsplash);
}
static void PF_cl_te_gunshotquad (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	if (P_RunParticleEffectTypeString(pos, vec3_origin, 1, "te_gunshotquad"))
		if (P_RunParticleEffectType(pos, NULL, 1, pt_gunshot))
			P_RunParticleEffect (pos, vec3_origin, 0, 20);
}
static void PF_cl_te_spikequad (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	if (P_RunParticleEffectTypeString(pos, vec3_origin, 1, "te_spikequad"))
		if (P_RunParticleEffectType(pos, NULL, 1, pt_spike))
			if (P_RunParticleEffectType(pos, NULL, 10, pt_gunshot))
				P_RunParticleEffect (pos, vec3_origin, 0, 10);
}
static void PF_cl_te_superspikequad (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	if (P_RunParticleEffectTypeString(pos, vec3_origin, 1, "te_superspikequad"))
		if (P_RunParticleEffectType(pos, NULL, 1, pt_superspike))
			if (P_RunParticleEffectType(pos, NULL, 2, pt_spike))
				if (P_RunParticleEffectType(pos, NULL, 20, pt_gunshot))
					P_RunParticleEffect (pos, vec3_origin, 0, 20);
}
static void PF_cl_te_explosionquad (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	if (P_RunParticleEffectTypeString(pos, vec3_origin, 1, "te_explosionquad"))
		if (P_RunParticleEffectType(pos, NULL, 1, pt_explosion))
			P_RunParticleEffect(pos, NULL, 107, 1024); // should be 97-111

	R_AddStain(pos, -1, -1, -1, 100);

	// light
	if (r_explosionlight.value) {
		dlight_t *dl;

		dl = CL_AllocDlight (0);
		VectorCopy (pos, dl->origin);
		dl->radius = 150 + r_explosionlight.value*200;
		dl->die = cl.time + 1;
		dl->decay = 300;

		dl->color[0] = 0.2;
		dl->color[1] = 0.155;
		dl->color[2] = 0.05;
		dl->channelfade[0] = 0.196;
		dl->channelfade[1] = 0.23;
		dl->channelfade[2] = 0.12;
	}

	S_StartSound (-2, 0, cl_sfx_r_exp3, pos, 1, 1);
}

//void(vector org, float radius, float lifetime, vector color) te_customflash
static void PF_cl_te_customflash (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);
	float radius = G_FLOAT(OFS_PARM1);
	float lifetime = G_FLOAT(OFS_PARM2);
	float *colour = G_VECTOR(OFS_PARM3);

	dlight_t *dl;
	// light
	dl = CL_AllocDlight (0);
	VectorCopy (org, dl->origin);
	dl->radius = radius;
	dl->die = cl.time + lifetime;
	dl->decay = dl->radius / lifetime;
	dl->color[0] = colour[0]*0.5f;
	dl->color[1] = colour[1]*0.5f;
	dl->color[2] = colour[2]*0.5f;
}

static void PF_cl_te_bloodshower (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
}
static void PF_cl_te_particlecube (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *minb = G_VECTOR(OFS_PARM0);
	float *maxb = G_VECTOR(OFS_PARM1);
	float *vel = G_VECTOR(OFS_PARM2);
	float howmany = G_FLOAT(OFS_PARM3);
	float color = G_FLOAT(OFS_PARM4);
	float gravity = G_FLOAT(OFS_PARM5);
	float jitter = G_FLOAT(OFS_PARM6);

	P_RunParticleCube(minb, maxb, vel, howmany, color, gravity, jitter);
}
static void PF_cl_te_spark (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
}
static void PF_cl_te_smallflash (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
}
static void PF_cl_te_explosion2 (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
}
static void PF_cl_te_lightning1 (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	float *start = G_VECTOR(OFS_PARM1);
	float *end = G_VECTOR(OFS_PARM1);

	CL_AddBeam(0, ent->entnum+MAX_EDICTS, start, end);
}
static void PF_cl_te_lightning2 (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	float *start = G_VECTOR(OFS_PARM1);
	float *end = G_VECTOR(OFS_PARM1);

	CL_AddBeam(1, ent->entnum+MAX_EDICTS, start, end);
}
static void PF_cl_te_lightning3 (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	float *start = G_VECTOR(OFS_PARM1);
	float *end = G_VECTOR(OFS_PARM1);

	CL_AddBeam(2, ent->entnum+MAX_EDICTS, start, end);
}
static void PF_cl_te_beam (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	float *start = G_VECTOR(OFS_PARM1);
	float *end = G_VECTOR(OFS_PARM1);

	CL_AddBeam(5, ent->entnum+MAX_EDICTS, start, end);
}
static void PF_cl_te_plasmaburn (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
}
static void PF_cl_te_explosionrgb (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);
	float *colour = G_VECTOR(OFS_PARM1);

	dlight_t *dl;

	if (P_RunParticleEffectType(org, NULL, 1, pt_explosion))
		P_RunParticleEffect(org, NULL, 107, 1024); // should be 97-111

	R_AddStain(org, -1, -1, -1, 100);

	// light
	if (r_explosionlight.value)
	{
		dl = CL_AllocDlight (0);
		VectorCopy (org, dl->origin);
		dl->radius = 150 + r_explosionlight.value*200;
		dl->die = cl.time + 0.5;
		dl->decay = 300;

		dl->color[0] = 0.4f*colour[0];
		dl->color[1] = 0.4f*colour[1];
		dl->color[2] = 0.4f*colour[2];
		dl->channelfade[0] = 0;
		dl->channelfade[1] = 0;
		dl->channelfade[2] = 0;
	}

	S_StartSound (-2, 0, cl_sfx_r_exp3, org, 1, 1);
}
static void PF_cl_te_particlerain (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *min = G_VECTOR(OFS_PARM0);
	float *max = G_VECTOR(OFS_PARM1);
	float *vel = G_VECTOR(OFS_PARM2);
	float howmany = G_FLOAT(OFS_PARM3);
	float colour = G_FLOAT(OFS_PARM4);

	P_RunParticleWeather(min, max, vel, howmany, colour, "rain");
}
static void PF_cl_te_particlesnow (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *min = G_VECTOR(OFS_PARM0);
	float *max = G_VECTOR(OFS_PARM1);
	float *vel = G_VECTOR(OFS_PARM2);
	float howmany = G_FLOAT(OFS_PARM3);
	float colour = G_FLOAT(OFS_PARM4);

	P_RunParticleWeather(min, max, vel, howmany, colour, "snow");
}

void CSQC_RunThreads(void)
{
	csqctreadstate_t *state = csqcthreads, *next;
	float ctime = Sys_DoubleTime();
	csqcthreads = NULL;
	while(state)
	{
		next = state->next;

		if (state->resumetime > ctime)
		{	//not time yet, reform original list.
			state->next = csqcthreads;
			csqcthreads = state;
		}
		else
		{	//call it and forget it ever happened. The Sleep biltin will recreate if needed.


			*csqcg.self = EDICT_TO_PROG(csqcprogs, EDICT_NUM(csqcprogs, state->self));
			*csqcg.other = EDICT_TO_PROG(csqcprogs, EDICT_NUM(csqcprogs, state->other));

			csqcprogs->RunThread(csqcprogs, state->thread);
			csqcprogs->parms->memfree(state->thread);
			csqcprogs->parms->memfree(state);
		}

		state = next;
	}
}

static void PF_cs_addprogs (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *s = PR_GetStringOfs(prinst, OFS_PARM0);
	if (!s || !*s)
		G_FLOAT(OFS_RETURN) = -1;
	else
		G_FLOAT(OFS_RETURN) = PR_LoadProgs(prinst, s, 0, NULL, 0);
}

static void PF_cs_OpenPortal (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
#ifdef Q2BSPS
	if (sv.worldmodel->fromgame == fg_quake2)
		CMQ2_SetAreaPortalState(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
#endif
}

void PF_cs_droptofloor (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t		*ent;
	vec3_t		end;
	vec3_t		start;
	trace_t		trace;

	ent = (csqcedict_t*)PROG_TO_EDICT(prinst, *csqcg.self);

	VectorCopy (ent->v->origin, end);
	end[2] -= 512;

	VectorCopy (ent->v->origin, start);
	trace = CS_Move (start, ent->v->mins, ent->v->maxs, end, MOVE_NORMAL, ent);

	if (trace.fraction == 1 || trace.allsolid)
		G_FLOAT(OFS_RETURN) = 0;
	else
	{
		VectorCopy (trace.endpos, ent->v->origin);
		CS_LinkEdict (ent, false);
		ent->v->flags = (int)ent->v->flags | FL_ONGROUND;
		ent->v->groundentity = EDICT_TO_PROG(prinst, trace.ent);
		G_FLOAT(OFS_RETURN) = 1;
	}
}

static void PF_cs_copyentity (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *in, *out;

	in = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	out = (csqcedict_t*)G_EDICT(prinst, OFS_PARM1);

	memcpy(out->v, in->v, csqcentsize);

	CS_LinkEdict (out, false);
}

static void PF_cl_playingdemo (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = !!cls.demoplayback;
}

static void PF_cl_runningserver (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = !!sv.active;
}

static void PF_cl_getlight (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	vec3_t ambient, diffuse, dir;
	cl.worldmodel->funcs.LightPointValues(cl.worldmodel, G_VECTOR(OFS_PARM0), ambient, diffuse, dir);
	VectorMA(ambient, 0.5, diffuse, G_VECTOR(OFS_RETURN));
}

/*
static void PF_Stub (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Con_Printf("Obsolete csqc builtin (%i) executed\n", prinst->lastcalledbuiltinnumber);
}
*/

void PF_rotatevectorsbytag (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	int tagnum = G_FLOAT(OFS_PARM1);

	float *srcorg = ent->v->origin;
	int modelindex = ent->v->modelindex;
	int frame1 = ent->v->frame;
	int frame2 = ent->v->frame2;
	float lerp = ent->v->lerpfrac;
	float frame1time = ent->v->frame1time;
	float frame2time = ent->v->frame2time;

	float *retorg = G_VECTOR(OFS_RETURN);

	model_t *mod = CSQC_GetModelForIndex(modelindex);
	float transforms[12];
	float src[12];
	float dest[12];
	int i;

	if (lerp < 0) lerp = 0;
	if (lerp > 1) lerp = 1;

	if (Mod_GetTag(mod, tagnum, frame1, frame2, lerp, frame1time, frame2time, transforms))
	{
		VectorCopy(csqcg.forward, src+0);
		src[3] = 0;
		VectorNegate(csqcg.right, src+4);
		src[7] = 0;
		VectorCopy(csqcg.up, src+8);
		src[11] = 0;

		if (ent->v->scale)
		{
			for (i = 0; i < 12; i+=4)
			{
				transforms[i+0] *= ent->v->scale;
				transforms[i+1] *= ent->v->scale;
				transforms[i+2] *= ent->v->scale;
				transforms[i+3] *= ent->v->scale;
			}
		}

		R_ConcatRotationsPad((void*)transforms, (void*)src, (void*)dest);

		VectorCopy(dest+0, csqcg.forward);
		VectorNegate(dest+4, csqcg.right);
		VectorCopy(dest+8, csqcg.up);

		VectorCopy(srcorg, retorg);
		for (i = 0 ; i < 3 ; i++)
		{
			retorg[0] += transforms[i*4+3]*src[4*i+0];
			retorg[1] += transforms[i*4+3]*src[4*i+1];
			retorg[2] += transforms[i*4+3]*src[4*i+2];
		}
		return;
	}

	VectorCopy(srcorg, retorg);
}
static void PF_cs_gettagindex (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	char *tagname = PR_GetStringOfs(prinst, OFS_PARM1);

	model_t *mod = CSQC_GetModelForIndex(ent->v->modelindex);
	G_FLOAT(OFS_RETURN) = Mod_TagNumForName(mod, tagname);
}
static void PF_rotatevectorsbyangles (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *ang = G_VECTOR(OFS_PARM0);
	vec3_t src[3], trans[3], res[3];
	ang[0]*=-1;
	AngleVectors(ang, trans[0], trans[1], trans[2]);
	ang[0]*=-1;
	VectorInverse(trans[1]);

	VectorCopy(csqcg.forward, src[0]);
	VectorNegate(csqcg.right, src[1]);
	VectorCopy(csqcg.up, src[2]);

	R_ConcatRotations(trans, src, res);

	VectorCopy(res[0], csqcg.forward);
	VectorNegate(res[1], csqcg.right);
	VectorCopy(res[2], csqcg.up);
}
static void PF_rotatevectorsbymatrix (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	vec3_t src[3], trans[3], res[3];

	VectorCopy(G_VECTOR(OFS_PARM0), src[0]);
	VectorNegate(G_VECTOR(OFS_PARM1), src[1]);
	VectorCopy(G_VECTOR(OFS_PARM2), src[2]);

	VectorCopy(csqcg.forward, src[0]);
	VectorNegate(csqcg.right, src[1]);
	VectorCopy(csqcg.up, src[2]);

	R_ConcatRotations(trans, src, res);

	VectorCopy(res[0], csqcg.forward);
	VectorNegate(res[1], csqcg.right);
	VectorCopy(res[2], csqcg.up);
}
static void PF_skinforname (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int modelindex = G_FLOAT(OFS_PARM0);
	char *str = PF_VarString(prinst, 1, pr_globals);
	model_t *mod = CSQC_GetModelForIndex(modelindex);


	if (Mod_SkinForName)
		G_FLOAT(OFS_RETURN) = Mod_SkinForName(mod, str);
	else
		G_FLOAT(OFS_RETURN) = -1;
}
static void PF_shaderforname (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *str = PF_VarString(prinst, 0, pr_globals);
#ifdef Q3SHADERS
	shader_t *shad;
	shad = R_RegisterSkin(str);
	if (shad)
		G_FLOAT(OFS_RETURN) = shad-r_shaders + 1;
	else
		G_FLOAT(OFS_RETURN) = 0;
#else
	G_FLOAT(OFS_RETURN) = 0;
#endif
}

qboolean CS_CheckBottom (csqcedict_t *ent)
{
	int savedhull;
	vec3_t	mins, maxs, start, stop;
	trace_t	trace;
	int		x, y;
	float	mid, bottom;

	if (!cl.worldmodel)
		return false;

	VectorAdd (ent->v->origin, ent->v->mins, mins);
	VectorAdd (ent->v->origin, ent->v->maxs, maxs);

// if all of the points under the corners are solid world, don't bother
// with the tougher checks
// the corners must be within 16 of the midpoint
	start[2] = mins[2] - 1;
	for	(x=0 ; x<=1 ; x++)
		for	(y=0 ; y<=1 ; y++)
		{
			start[0] = x ? maxs[0] : mins[0];
			start[1] = y ? maxs[1] : mins[1];
			if (!(CS_PointContents (start) & FTECONTENTS_SOLID))
				goto realcheck;
		}

//	c_yes++;
	return true;		// we got out easy

realcheck:
//	c_no++;
//
// check it for real...
//
	start[2] = mins[2];

// the midpoint must be within 16 of the bottom
	start[0] = stop[0] = (mins[0] + maxs[0])*0.5;
	start[1] = stop[1] = (mins[1] + maxs[1])*0.5;
	stop[2] = start[2] - 2*movevars.stepheight;
	trace = CS_Move (start, vec3_origin, vec3_origin, stop, true, ent);

	if (trace.fraction == 1.0)
		return false;
	mid = bottom = trace.endpos[2];

// the corners must be within 16 of the midpoint
	for	(x=0 ; x<=1 ; x++)
		for	(y=0 ; y<=1 ; y++)
		{
			start[0] = stop[0] = x ? maxs[0] : mins[0];
			start[1] = stop[1] = y ? maxs[1] : mins[1];

			savedhull = ent->v->hull;
			ent->v->hull = 0;
			trace = CS_Move (start, vec3_origin, vec3_origin, stop, true, ent);
			ent->v->hull = savedhull;

			if (trace.fraction != 1.0 && trace.endpos[2] > bottom)
				bottom = trace.endpos[2];
			if (trace.fraction == 1.0 || mid - trace.endpos[2] > movevars.stepheight)
				return false;
		}

//	c_yes++;
	return true;
}
static void PF_cs_checkbottom (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t	*ent;

	ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);

	G_FLOAT(OFS_RETURN) = CS_CheckBottom (ent);
}

static void PF_cs_break (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Con_Printf ("break statement\n");
#ifdef TEXTEDITOR
	(*prinst->pr_trace)++;
#endif
}

qboolean CS_movestep (csqcedict_t *ent, vec3_t move, qboolean relink, qboolean noenemy, qboolean set_trace)
{
	float		dz;
	vec3_t		oldorg, neworg, end;
	trace_t		trace;
	int			i;
	csqcedict_t		*enemy = csqc_edicts;

// try the move
	VectorCopy (ent->v->origin, oldorg);
	VectorAdd (ent->v->origin, move, neworg);

// flying monsters don't step up
	if ( (int)ent->v->flags & (FL_SWIM | FL_FLY) )
	{
	// try one move with vertical motion, then one without
		for (i=0 ; i<2 ; i++)
		{
			VectorAdd (ent->v->origin, move, neworg);
			if (!noenemy)
			{
				enemy = (csqcedict_t*)PROG_TO_EDICT(csqcprogs, ent->v->enemy);
				if (i == 0 && enemy != csqc_edicts)
				{
					dz = ent->v->origin[2] - ((csqcedict_t*)PROG_TO_EDICT(csqcprogs, ent->v->enemy))->v->origin[2];
					if (dz > 40)
						neworg[2] -= 8;
					if (dz < 30)
						neworg[2] += 8;
				}
			}
			trace = CS_Move (ent->v->origin, ent->v->mins, ent->v->maxs, neworg, false, ent);
			if (set_trace)
				cs_settracevars(&trace);

			if (trace.fraction == 1)
			{
				if ( ((int)ent->v->flags & FL_SWIM) && !(CS_PointContents(trace.endpos) & FTECONTENTS_FLUID))
					return false;	// swim monster left water

				VectorCopy (trace.endpos, ent->v->origin);
				if (relink)
					CS_LinkEdict (ent, true);
				return true;
			}

			if (noenemy || enemy == csqc_edicts)
				break;
		}

		return false;
	}

// push down from a step height above the wished position
	neworg[2] += movevars.stepheight;
	VectorCopy (neworg, end);
	end[2] -= movevars.stepheight*2;

	trace = CS_Move (neworg, ent->v->mins, ent->v->maxs, end, false, ent);
	if (set_trace)
		cs_settracevars(&trace);

	if (trace.allsolid)
		return false;

	if (trace.startsolid)
	{
		neworg[2] -= movevars.stepheight;
		trace = CS_Move (neworg, ent->v->mins, ent->v->maxs, end, false, ent);
		if (set_trace)
			cs_settracevars(&trace);
		if (trace.allsolid || trace.startsolid)
			return false;
	}
	if (trace.fraction == 1)
	{
	// if monster had the ground pulled out, go ahead and fall
		if ( (int)ent->v->flags & FL_PARTIALGROUND )
		{
			VectorAdd (ent->v->origin, move, ent->v->origin);
			if (relink)
				CS_LinkEdict (ent, true);
			ent->v->flags = (int)ent->v->flags & ~FL_ONGROUND;
//	Con_Printf ("fall down\n");
			return true;
		}

		return false;		// walked off an edge
	}

// check point traces down for dangling corners
	VectorCopy (trace.endpos, ent->v->origin);

	if (!CS_CheckBottom (ent))
	{
		if ( (int)ent->v->flags & FL_PARTIALGROUND )
		{	// entity had floor mostly pulled out from underneath it
			// and is trying to correct
			if (relink)
				CS_LinkEdict (ent, true);
			return true;
		}
		VectorCopy (oldorg, ent->v->origin);
		return false;
	}

	if ( (int)ent->v->flags & FL_PARTIALGROUND )
	{
//		Con_Printf ("back on ground\n");
		ent->v->flags = (int)ent->v->flags & ~FL_PARTIALGROUND;
	}
	ent->v->groundentity = EDICT_TO_PROG(csqcprogs, trace.ent);

// the move is ok
	if (relink)
		CS_LinkEdict (ent, true);
	return true;
}

static void PF_cs_walkmove (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t	*ent;
	float	yaw, dist;
	vec3_t	move;
//	dfunction_t	*oldf;
	int 	oldself;
	qboolean settrace;

	ent = (csqcedict_t*)PROG_TO_EDICT(prinst, *csqcg.self);
	yaw = G_FLOAT(OFS_PARM0);
	dist = G_FLOAT(OFS_PARM1);
	if (*prinst->callargc >= 3 && G_FLOAT(OFS_PARM2))
		settrace = true;
	else
		settrace = false;

	if ( !( (int)ent->v->flags & (FL_ONGROUND|FL_FLY|FL_SWIM) ) )
	{
		G_FLOAT(OFS_RETURN) = 0;
		return;
	}

	yaw = yaw*M_PI*2 / 360;

	move[0] = cos(yaw)*dist;
	move[1] = sin(yaw)*dist;
	move[2] = 0;

// save program state, because CS_movestep may call other progs
	oldself = *csqcg.self;

	G_FLOAT(OFS_RETURN) = CS_movestep(ent, move, true, false, settrace);

// restore program state
	*csqcg.self = oldself;
}

static void CS_ConsoleCommand_f(void)
{	//FIXME: unregister them.
	char cmd[2048];
	sprintf(cmd, "%s %s", Cmd_Argv(0), Cmd_Args());
	CSQC_ConsoleCommand(cmd);
}
static void PF_cs_registercommand (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *str = PF_VarString(prinst, 0, pr_globals);
	Cmd_AddRemCommand(str, CS_ConsoleCommand_f);
}

static qboolean csqc_usinglistener;
qboolean CSQC_SettingListener(void)
{	//stops the engine from setting the listener positions.
	if (csqc_usinglistener)
	{
		csqc_usinglistener = false;
		return true;
	}
	return false;
}
static void PF_cs_setlistener (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *origin = G_VECTOR(OFS_PARM0);
	float *forward = G_VECTOR(OFS_PARM1);
	float *right = G_VECTOR(OFS_PARM2);
	float *up = G_VECTOR(OFS_PARM3);
	csqc_usinglistener = true;
	S_Update(origin, forward, right, up);
}

#define PF_FixTen PF_Fixme,PF_Fixme,PF_Fixme,PF_Fixme,PF_Fixme,PF_Fixme,PF_Fixme,PF_Fixme,PF_Fixme,PF_Fixme

//prefixes:
//PF_ - common, works on any vm
//PF_cs_ - works in csqc only (dependant upon globals or fields)
//PF_cl_ - works in csqc and menu (if needed...)

//these are the builtins that still need to be added.
#define PF_cs_gettaginfo		PF_Fixme
#define PS_cs_setattachment		PF_Fixme

#define PF_R_PolygonBegin		PF_Fixme			// #306 void(string texturename) R_BeginPolygon (EXT_CSQC_???)
#define PF_R_PolygonVertex		PF_Fixme			// #307 void(vector org, vector texcoords, vector rgb, float alpha) R_PolygonVertex (EXT_CSQC_???)
#define PF_R_PolygonEnd			PF_Fixme			// #308 void() R_EndPolygon (EXT_CSQC_???)

//warning: functions that depend on globals are bad, mkay?
static builtin_t csqc_builtins[] = {
//0
PF_Fixme,				// #0
PF_cs_makevectors,		// #1 void() makevectors (QUAKE)
PF_cs_SetOrigin,		// #2 void(entity e, vector org) setorigin (QUAKE)
PF_cs_SetModel,			// #3 void(entity e, string modl) setmodel (QUAKE)
PF_cs_SetSize,			// #4 void(entity e, vector mins, vector maxs) setsize (QUAKE)
PF_Fixme,				// #5
PF_cs_break,			// #6 void() debugbreak (QUAKE)
PF_random,				// #7 float() random (QUAKE)
PF_cs_sound,			// #8 void(entity e, float chan, string samp, float vol, float atten) sound (QUAKE)
PF_normalize,			// #9 vector(vector in) normalize (QUAKE)
//10
PF_error,				// #10 void(string errortext) error (QUAKE)
PF_objerror,			// #11 void(string errortext) onjerror (QUAKE)
PF_vlen,				// #12 float(vector v) vlen (QUAKE)
PF_vectoyaw,			// #13 float(vector v) vectoyaw (QUAKE)
PF_Spawn,				// #14 entity() spawn (QUAKE)
PF_cs_remove,			// #15 void(entity e) remove (QUAKE)
PF_cs_traceline,		// #16 void(vector v1, vector v2, float nomonst, entity forent) traceline (QUAKE)
PF_NoCSQC,				// #17 entity() checkclient (QUAKE) (don't support)
PF_FindString,			// #18 entity(entity start, .string fld, string match) findstring (QUAKE)
PF_cs_PrecacheSound,	// #19 void(string str) precache_sound (QUAKE)
//20
PF_cs_PrecacheModel,	// #20 void(string str) precache_model (QUAKE)
PF_NoCSQC,				// #21 void(entity client, string s) stuffcmd (QUAKE) (don't support)
PF_cs_findradius,		// #22 entity(vector org, float rad) findradius (QUAKE)
PF_NoCSQC,				// #23 void(string s, ...) bprint (QUAKE) (don't support)
PF_NoCSQC,				// #24 void(entity e, string s, ...) sprint (QUAKE) (don't support)
PF_dprint,				// #25 void(string s, ...) dprint (QUAKE)
PF_ftos,				// #26 string(float f) ftos (QUAKE)
PF_vtos,				// #27 string(vector f) vtos (QUAKE)
PF_coredump,			// #28 void(void) coredump (QUAKE)
PF_traceon,				// #29 void() traceon (QUAKE)
//30
PF_traceoff,			// #30 void() traceoff (QUAKE)
PF_eprint,				// #31 void(entity e) eprint (QUAKE)
PF_cs_walkmove,			// #32 float(float yaw, float dist) walkmove (QUAKE)
PF_Fixme,				// #33
PF_cs_droptofloor,		// #34
PF_cs_lightstyle,		// #35 void(float lightstyle, string stylestring) lightstyle (QUAKE)
PF_rint,				// #36 float(float f) rint (QUAKE)
PF_floor,				// #37 float(float f) floor (QUAKE)
PF_ceil,				// #38 float(float f) ceil (QUAKE)
PF_Fixme,				// #39
//40
PF_cs_checkbottom,		// #40 float(entity e) checkbottom (QUAKE)
PF_cs_pointcontents,	// #41 float(vector org) pointcontents (QUAKE)
PF_Fixme,				// #42
PF_fabs,				// #43 float(float f) fabs (QUAKE)
PF_NoCSQC,				// #44 vector(entity e, float speed) aim (QUAKE) (don't support)
PF_cvar,				// #45 float(string cvarname) cvar (QUAKE)
PF_localcmd,			// #46 void(string str) localcmd (QUAKE)
PF_nextent,				// #47 entity(entity e) nextent (QUAKE)
PF_cs_particle,			// #48 void(vector org, vector dir, float colour, float count) particle (QUAKE)
PF_cs_changeyaw,		// #49 void() changeyaw (QUAKE)
//50
PF_Fixme,				// #50
PF_vectoangles,			// #51 vector(vector v) vectoangles (QUAKE)
PF_Fixme,				// #52 void(float to, float f) WriteByte (QUAKE)
PF_Fixme,				// #53 void(float to, float f) WriteChar (QUAKE)
PF_Fixme,				// #54 void(float to, float f) WriteShort (QUAKE)

PF_Fixme,				// #55 void(float to, float f) WriteLong (QUAKE)
PF_Fixme,				// #56 void(float to, float f) WriteCoord (QUAKE)
PF_Fixme,				// #57 void(float to, float f) WriteAngle (QUAKE)
PF_Fixme,				// #58 void(float to, float f) WriteString (QUAKE)
PF_Fixme,				// #59 void(float to, float f) WriteEntity (QUAKE)

//60
PF_Sin,					// #60 float(float angle) sin (DP_QC_SINCOSSQRTPOW)
PF_Cos,					// #61 float(float angle) cos (DP_QC_SINCOSSQRTPOW)
PF_Sqrt,				// #62 float(float value) sqrt (DP_QC_SINCOSSQRTPOW)
PF_cs_changepitch,		// #63 void(entity ent) changepitch (DP_QC_CHANGEPITCH)
PF_cs_tracetoss,		// #64 void(entity ent, entity ignore) tracetoss (DP_QC_TRACETOSS)

PF_etos,				// #65 string(entity ent) etos (DP_QC_ETOS)
PF_Fixme,				// #66
PF_Fixme,				// #67 void(float step) movetogoal (QUAKE)
PF_NoCSQC,				// #68 void(string s) precache_file (QUAKE) (don't support)
PF_cs_makestatic,		// #69 void(entity e) makestatic (QUAKE)
//70
PF_NoCSQC,				// #70 void(string mapname) changelevel (QUAKE) (don't support)
PF_Fixme,				// #71
PF_cvar_set,			// #72 void(string cvarname, string valuetoset) cvar_set (QUAKE)
PF_NoCSQC,				// #73 void(entity ent, string text) centerprint (QUAKE) (don't support - cprint is supported instead)
PF_cl_ambientsound,		// #74 void (vector pos, string samp, float vol, float atten) ambientsound (QUAKE)

PF_cs_PrecacheModel,	// #75 void(string str) precache_model2 (QUAKE)
PF_cs_PrecacheSound,	// #76 void(string str) precache_sound2 (QUAKE)
PF_NoCSQC,				// #77 void(string str) precache_file2 (QUAKE)
PF_NoCSQC,				// #78 void() setspawnparms (QUAKE) (don't support)
PF_NoCSQC,				// #79 void(entity killer, entity killee) logfrag (QW_ENGINE) (don't support)

//80
PF_NoCSQC,				// #80 string(entity e, string keyname) infokey (QW_ENGINE) (don't support)
PF_stof,				// #81 float(string s) stof (FRIK_FILE or QW_ENGINE)
PF_NoCSQC,				// #82 void(vector where, float set) multicast (QW_ENGINE) (don't support)
PF_Fixme,
PF_Fixme,

PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,

//90
PF_cs_tracebox,
PF_randomvector,		// #91 vector() randomvec (DP_QC_RANDOMVEC)
PF_cl_getlight,			// #92 vector(vector org) getlight (DP_QC_GETLIGHT)
PF_registercvar,		// #93 void(string cvarname, string defaultvalue) registercvar (DP_QC_REGISTERCVAR)
PF_min,					// #94 float(float a, floats) min (DP_QC_MINMAXBOUND)

PF_max,					// #95 float(float a, floats) max (DP_QC_MINMAXBOUND)
PF_bound,				// #96 float(float minimum, float val, float maximum) bound (DP_QC_MINMAXBOUND)
PF_pow,					// #97 float(float value) pow (DP_QC_SINCOSSQRTPOW)
PF_FindFloat,			// #98 entity(entity start, .float fld, float match) findfloat (DP_QC_FINDFLOAT)
PF_checkextension,		// #99 float(string extname) checkextension (EXT_CSQC)

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
PF_fopen,				// #110 float(string strname, float accessmode) fopen (FRIK_FILE)
PF_fclose,				// #111 void(float fnum) fclose (FRIK_FILE)
PF_fgets,				// #112 string(float fnum) fgets (FRIK_FILE)
PF_fputs,				// #113 void(float fnum, string str) fputs (FRIK_FILE)
PF_strlen,				// #114 float(string str) strlen (FRIK_FILE)

PF_strcat,				// #115 string(string str1, string str2, ...) strcat (FRIK_FILE)
PF_substring,			// #116 string(string str, float start, float length) substring (FRIK_FILE)
PF_stov,				// #117 vector(string str) stov (FRIK_FILE)
PF_dupstring,			// #118 string(string str) dupstring (FRIK_FILE)
PF_forgetstring,		// #119 void(string str) freestring (FRIK_FILE)

//120
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

//130
PF_FixTen,

//140
PF_FixTen,

//150
PF_FixTen,

//160
PF_FixTen,

//170
PF_FixTen,

//180
PF_FixTen,

//190
PF_FixTen,

//200
PF_cs_PrecacheModel,
PF_externcall,
PF_cs_addprogs,
PF_externvalue,
PF_externset,

PF_externrefcall,
PF_instr,
	PF_cs_OpenPortal,	//q2bsps
	PF_NoCSQC,//{"RegisterTempEnt", PF_RegisterTEnt,	0,		0,		0,		208},
	PF_NoCSQC,//{"CustomTempEnt",	PF_CustomTEnt,		0,		0,		0,		209},
//210
	PF_Fixme,//{"fork",			PF_Fork,			0,		0,		0,		210},
	PF_Abort, //#211 void() abort (FTE_MULTITHREADED)
	PF_Fixme,//{"sleep",			PF_Sleep,			0,		0,		0,		212},
	PF_NoCSQC,//{"forceinfokey",	PF_ForceInfoKey,	0,		0,		0,		213},
	PF_NoCSQC,//{"chat",			PF_chat,			0,		0,		0,		214},// #214 void(string filename, float starttag, entity edict) SV_Chat (FTE_NPCCHAT)

	PF_cs_particle2, //215 (FTE_PEXT_HEXEN2)
	PF_cs_particle3, //216 (FTE_PEXT_HEXEN2)
	PF_cs_particle4, //217 (FTE_PEXT_HEXEN2)

//EXT_DIMENSION_PLANES
	PF_bitshift,		//#218 bitshift (EXT_DIMENSION_PLANES)
	PF_cl_te_lightningblood,// #219 te_lightningblood void(vector org) (FTE_TE_STANDARDEFFECTBUILTINS)

//220
	PF_Fixme, //{"map_builtin",		PF_builtinsupported,0,		0,		0,		220},	//like #100 - takes 2 args. arg0 is builtinname, 1 is number to map to.
PF_strstrofs,	// #221 float(string s1, string sub) strstrofs (FTE_STRINGS)
PF_str2chr,		// #222 float(string str, float index) str2chr (FTE_STRINGS)
PF_chr2str,		// #223 string(float chr, ...) chr2str (FTE_STRINGS)
PF_strconv,		// #224 string(float ccase, float redalpha, float redchars, string str, ...) strconv (FTE_STRINGS)

PF_strpad,		// #225 string strpad(float pad, string str1, ...) strpad (FTE_STRINGS)
PF_infoadd,		// #226 string(string old, string key, string value) infoadd
PF_infoget,		// #227 string(string info, string key) infoget
PF_strncmp,		// #228 float(string s1, string s2, float len) strncmp (FTE_STRINGS)
PF_strcasecmp,	// #229 float(string s1, string s2) strcasecmp (FTE_STRINGS)

//230
PF_strncasecmp,	// #230 float(string s1, string s2, float len) strncasecmp (FTE_STRINGS)
PF_Fixme,		// #231 clientstat
PF_Fixme,		// #232 runclientphys
PF_Fixme,		// #233 float(entity ent) isbackbuffered
PF_rotatevectorsbytag,	// #234

PF_rotatevectorsbyangles, // #235
PF_rotatevectorsbymatrix, // #236
PF_skinforname,		// #237
PF_shaderforname,	// #238
PF_cl_te_bloodqw,	// #239 void te_bloodqw(vector org[, float count]) (FTE_TE_STANDARDEFFECTBUILTINS)

//240
PF_FixTen,

//250
PF_FixTen,

//260
PF_FixTen,

//270
PF_FixTen,

//280
PF_FixTen,

//290
PF_FixTen,

//300
PF_R_ClearScene,				// #300 void() clearscene (EXT_CSQC)
PF_R_AddEntityMask,				// #301 void(float mask) addentities (EXT_CSQC)
PF_R_AddEntity,					// #302 void(entity ent) addentity (EXT_CSQC)
PF_R_SetViewFlag,				// #303 float(float property, ...) setproperty (EXT_CSQC)
PF_R_RenderScene,				// #304 void() renderscene (EXT_CSQC)

PF_R_AddDynamicLight,			// #305 void(vector org, float radius, vector lightcolours) adddynamiclight (EXT_CSQC)

PF_R_PolygonBegin,				// #306 void(string texturename) R_BeginPolygon (EXT_CSQC_???)
PF_R_PolygonVertex,				// #307 void(vector org, vector texcoords, vector rgb, float alpha) R_PolygonVertex (EXT_CSQC_???)
PF_R_PolygonEnd,				// #308 void() R_EndPolygon (EXT_CSQC_???)

PF_Fixme,						// #309

//310
//maths stuff that uses the current view settings.
PF_cs_unproject,				// #310 vector (vector v) unproject (EXT_CSQC)
PF_cs_project,					// #311 vector (vector v) project (EXT_CSQC)

PF_Fixme,						// #312
PF_Fixme,						// #313
PF_Fixme,						// #314

//2d (immediate) operations
PF_CL_drawline,					// #315 void(float width, vector pos1, vector pos2) drawline (EXT_CSQC)
PF_CL_is_cached_pic,			// #316 float(string name) iscachedpic (EXT_CSQC)
PF_CL_precache_pic,				// #317 string(string name, float trywad) precache_pic (EXT_CSQC)
PF_CL_drawgetimagesize,			// #318 vector(string picname) draw_getimagesize (EXT_CSQC)
PF_CL_free_pic,					// #319 void(string name) freepic (EXT_CSQC)
//320
PF_CL_drawcharacter,			// #320 float(vector position, float character, vector scale, vector rgb, float alpha [, float flag]) drawcharacter (EXT_CSQC, [EXT_CSQC_???])
PF_CL_drawstring,				// #321 float(vector position, string text, vector scale, vector rgb, float alpha [, float flag]) drawstring (EXT_CSQC, [EXT_CSQC_???])
PF_CL_drawpic,					// #322 float(vector position, string pic, vector size, vector rgb, float alpha [, float flag]) drawpic (EXT_CSQC, [EXT_CSQC_???])
PF_CL_drawfill,					// #323 float(vector position, vector size, vector rgb, float alpha [, float flag]) drawfill (EXT_CSQC, [EXT_CSQC_???])
PF_CL_drawsetcliparea,			// #324 void(float x, float y, float width, float height) drawsetcliparea (EXT_CSQC_???)
PF_CL_drawresetcliparea,		// #325 void(void) drawresetcliparea (EXT_CSQC_???)

PF_Fixme,						// #326
PF_Fixme,						// #327
PF_Fixme,						// #328
PF_Fixme,						// #329

//330
PF_cs_getstatf,					// #330 float(float stnum) getstatf (EXT_CSQC)
PF_cs_getstati,					// #331 float(float stnum) getstati (EXT_CSQC)
PF_cs_getstats,					// #332 string(float firststnum) getstats (EXT_CSQC)
PF_cs_SetModelIndex,			// #333 void(entity e, float mdlindex) setmodelindex (EXT_CSQC)
PF_cs_ModelnameForIndex,		// #334 string(float mdlindex) modelnameforindex (EXT_CSQC)

PF_cs_particlesloaded,			// #335 float(string effectname) particleeffectnum (EXT_CSQC)
PF_cs_trailparticles,			// #336 void(entity ent, float effectnum, vector start, vector end) trailparticles (EXT_CSQC),
PF_cs_pointparticles,			// #337 void(float effectnum, vector origin [, vector dir, float count]) pointparticles (EXT_CSQC)

PF_cl_cprint,					// #338 void(string s) cprint (EXT_CSQC)
PF_print,						// #339 void(string s) print (EXT_CSQC)

//340
PF_cl_keynumtostring,			// #340 string(float keynum) keynumtostring (EXT_CSQC)
PF_cl_stringtokeynum,			// #341 float(string keyname) stringtokeynum (EXT_CSQC)
PF_cl_getkeybind,				// #342 string(float keynum) getkeybind (EXT_CSQC)

PF_Fixme,						// #343
PF_Fixme,						// #344

PF_cs_getinputstate,			// #345 float(float framenum) getinputstate (EXT_CSQC)
PF_cs_setsensativityscaler, 	// #346 void(float sens) setsensitivityscaler (EXT_CSQC)

PF_cs_runplayerphysics,			// #347 void() runstandardplayerphysics (EXT_CSQC)

PF_cs_getplayerkey,				// #348 string(float playernum, string keyname) getplayerkeyvalue (EXT_CSQC)

PF_cl_playingdemo,				// #349 float() isdemo (EXT_CSQC)
//350
PF_cl_runningserver,			// #350 float() isserver (EXT_CSQC)

PF_cs_setlistener, 				// #351 void(vector origin, vector forward, vector right, vector up) SetListener (EXT_CSQC)
PF_cs_registercommand,			// #352 void(string cmdname) registercommand (EXT_CSQC)
PF_WasFreed,					// #353 float(entity ent) wasfreed (EXT_CSQC) (should be availabe on server too)

PF_cs_serverkey,				// #354 string(string key) serverkey;
PF_Fixme,						// #355
PF_Fixme,						// #356
PF_Fixme,						// #357
PF_Fixme,						// #358
PF_Fixme,						// #359

//360
//note that 'ReadEntity' is pretty hard to implement reliably. Modders should use a combination of ReadShort, and findfloat, and remember that it might not be known clientside (pvs culled or other reason)
PF_ReadByte,					// #360 float() readbyte (EXT_CSQC)
PF_ReadChar,					// #361 float() readchar (EXT_CSQC)
PF_ReadShort,					// #362 float() readshort (EXT_CSQC)
PF_ReadLong,					// #363 float() readlong (EXT_CSQC)
PF_ReadCoord,					// #364 float() readcoord (EXT_CSQC)

PF_ReadAngle,					// #365 float() readangle (EXT_CSQC)
PF_ReadString,					// #366 string() readstring (EXT_CSQC)
PF_ReadFloat,					// #367 string() readfloat (EXT_CSQC)

PF_Fixme,						// #368
PF_Fixme,						// #369

//370
PF_FixTen,

//380
PF_FixTen,

//390
PF_FixTen,

//400
PF_cs_copyentity,		// #400 void(entity from, entity to) copyentity (DP_QC_COPYENTITY)
PF_NoCSQC,				// #401 void(entity cl, float colours) setcolors (DP_SV_SETCOLOR) (don't implement)
PF_findchain,			// #402 entity(string field, string match) findchain (DP_QC_FINDCHAIN)
PF_findchainfloat,		// #403 entity(float fld, float match) findchainfloat (DP_QC_FINDCHAINFLOAT)
PF_cl_effect,			// #404 void(vector org, string modelname, float startframe, float endframe, float framerate) effect (DP_SV_EFFECT)

PF_cl_te_blooddp,		// #405 void(vector org, vector velocity, float howmany) te_blood (DP_TE_BLOOD)
PF_cl_te_bloodshower,	// #406 void(vector mincorner, vector maxcorner, float explosionspeed, float howmany) te_bloodshower (DP_TE_BLOODSHOWER)
PF_cl_te_explosionrgb,	// #407 void(vector org, vector color) te_explosionrgb (DP_TE_EXPLOSIONRGB)
PF_cl_te_particlecube,	// #408 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color, float gravityflag, float randomveljitter) te_particlecube (DP_TE_PARTICLECUBE)
PF_cl_te_particlerain,	// #409 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color) te_particlerain (DP_TE_PARTICLERAIN)

PF_cl_te_particlesnow,	// #410 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color) te_particlesnow (DP_TE_PARTICLESNOW)
PF_cl_te_spark,			// #411 void(vector org, vector vel, float howmany) te_spark (DP_TE_SPARK)
PF_cl_te_gunshotquad,	// #412 void(vector org) te_gunshotquad (DP_TE_QUADEFFECTS1)
PF_cl_te_spikequad,		// #413 void(vector org) te_spikequad (DP_TE_QUADEFFECTS1)
PF_cl_te_superspikequad,// #414 void(vector org) te_superspikequad (DP_TE_QUADEFFECTS1)

PF_cl_te_explosionquad,	// #415 void(vector org) te_explosionquad (DP_TE_QUADEFFECTS1)
PF_cl_te_smallflash,	// #416 void(vector org) te_smallflash (DP_TE_SMALLFLASH)
PF_cl_te_customflash,	// #417 void(vector org, float radius, float lifetime, vector color) te_customflash (DP_TE_CUSTOMFLASH)
PF_cl_te_gunshot,		// #418 void(vector org) te_gunshot (DP_TE_STANDARDEFFECTBUILTINS)
PF_cl_te_spike,			// #419 void(vector org) te_spike (DP_TE_STANDARDEFFECTBUILTINS)

PF_cl_te_superspike,	// #420 void(vector org) te_superspike (DP_TE_STANDARDEFFECTBUILTINS)
PF_cl_te_explosion,		// #421 void(vector org) te_explosion (DP_TE_STANDARDEFFECTBUILTINS)
PF_cl_te_tarexplosion,	// #422 void(vector org) te_tarexplosion (DP_TE_STANDARDEFFECTBUILTINS)
PF_cl_te_wizspike,		// #423 void(vector org) te_wizspike (DP_TE_STANDARDEFFECTBUILTINS)
PF_cl_te_knightspike,	// #424 void(vector org) te_knightspike (DP_TE_STANDARDEFFECTBUILTINS)

PF_cl_te_lavasplash,	// #425 void(vector org) te_lavasplash  (DP_TE_STANDARDEFFECTBUILTINS)
PF_cl_te_teleport,		// #426 void(vector org) te_teleport (DP_TE_STANDARDEFFECTBUILTINS)
PF_cl_te_explosion2,	// #427 void(vector org, float color, float colorlength) te_explosion2 (DP_TE_STANDARDEFFECTBUILTINS)
PF_cl_te_lightning1,	// #428 void(entity own, vector start, vector end) te_lightning1 (DP_TE_STANDARDEFFECTBUILTINS)
PF_cl_te_lightning2,	// #429 void(entity own, vector start, vector end) te_lightning2 (DP_TE_STANDARDEFFECTBUILTINS)

PF_cl_te_lightning3,	// #430 void(entity own, vector start, vector end) te_lightning3 (DP_TE_STANDARDEFFECTBUILTINS)
PF_cl_te_beam,			// #431 void(entity own, vector start, vector end) te_beam (DP_TE_STANDARDEFFECTBUILTINS)
PF_cs_vectorvectors,	// #432 void(vector dir) vectorvectors (DP_QC_VECTORVECTORS)
PF_cl_te_plasmaburn,	// #433 void(vector org) te_plasmaburn (DP_TE_PLASMABURN)
PF_Fixme,				// #434 float(entity e, float s) getsurfacenumpoints (DP_QC_GETSURFACE)

PF_Fixme,				// #435 vector(entity e, float s, float n) getsurfacepoint (DP_QC_GETSURFACE)
PF_Fixme,				// #436 vector(entity e, float s) getsurfacenormal (DP_QC_GETSURFACE)
PF_Fixme,				// #437 string(entity e, float s) getsurfacetexture (DP_QC_GETSURFACE)
PF_Fixme,				// #438 float(entity e, vector p) getsurfacenearpoint (DP_QC_GETSURFACE)
PF_Fixme,				// #439 vector(entity e, float s, vector p) getsurfaceclippedpoint (DP_QC_GETSURFACE)

PF_NoCSQC,				// #440 void(entity e, string s) clientcommand (KRIMZON_SV_PARSECLIENTCOMMAND) (don't implement)
PF_Tokenize,			// #441 float(string s) tokenize (KRIMZON_SV_PARSECLIENTCOMMAND)
PF_ArgV,				// #442 string(float n) argv (KRIMZON_SV_PARSECLIENTCOMMAND)
PS_cs_setattachment,	// #443 void(entity e, entity tagentity, string tagname) setattachment (DP_GFX_QUAKE3MODELTAGS)
PF_search_begin,		// #444 float	search_begin(string pattern, float caseinsensitive, float quiet) (DP_QC_FS_SEARCH)

PF_search_end,			// #445 void	search_end(float handle) (DP_QC_FS_SEARCH)
PF_search_getsize,		// #446 float	search_getsize(float handle) (DP_QC_FS_SEARCH)
PF_search_getfilename,	// #447 string	search_getfilename(float handle, float num) (DP_QC_FS_SEARCH)
PF_cvar_string,			// #448 string(float n) cvar_string (DP_QC_CVAR_STRING)
PF_FindFlags,			// #449 entity(entity start, .entity fld, float match) findflags (DP_QC_FINDFLAGS)

PF_findchainflags,		// #450 entity(.float fld, float match) findchainflags (DP_QC_FINDCHAINFLAGS)
PF_cs_gettagindex,		// #451 float(entity ent, string tagname) gettagindex (DP_MD3_TAGSINFO)
PF_cs_gettaginfo,		// #452 vector(entity ent, float tagindex) gettaginfo (DP_MD3_TAGSINFO)
PF_NoCSQC,				// #453 void(entity player) dropclient (DP_QC_BOTCLIENT) (don't implement)
PF_NoCSQC,				// #454	entity() spawnclient (DP_QC_BOTCLIENT) (don't implement)

PF_NoCSQC,				// #455 float(entity client) clienttype (DP_QC_BOTCLIENT) (don't implement)
PF_Fixme,				// #456
PF_Fixme,				// #457
PF_Fixme,				// #458
PF_Fixme,				// #459

//460
PF_FixTen,

//470
PF_FixTen,
//480
PF_FixTen,
//490
PF_FixTen,
//500
PF_FixTen,
};
static int csqc_numbuiltins = sizeof(csqc_builtins)/sizeof(csqc_builtins[0]);





static jmp_buf csqc_abort;
static progparms_t csqcprogparms;



int COM_FileSize(char *path);
pbool QC_WriteFile(char *name, void *data, int len);
void *VARGS PR_CB_Malloc(int size);	//these functions should be tracked by the library reliably, so there should be no need to track them ourselves.
void VARGS PR_CB_Free(void *mem);

//Any menu builtin error or anything like that will come here.
void VARGS CSQC_Abort (char *format, ...)	//an error occured.
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr, format);
	vsnprintf (string,sizeof(string)-1, format,argptr);
	va_end (argptr);

	Con_Printf("CSQC_Abort: %s\nShutting down csqc\n", string);

	if (pr_csqc_coreonerror.value)
	{
		int size = 1024*1024*8;
		char *buffer = BZ_Malloc(size);
		csqcprogs->save_ents(csqcprogs, buffer, &size, 3);
		COM_WriteFile("csqccore.txt", buffer, size);
		BZ_Free(buffer);
	}

	Host_EndGame("csqc error");
}

void CSQC_ForgetThreads(void)
{
	csqctreadstate_t *state = csqcthreads, *next;
	csqcthreads = NULL;
	while(state)
	{
		next = state->next;

		csqcprogs->parms->memfree(state->thread);
		csqcprogs->parms->memfree(state);

		state = next;
	}
}

void CSQC_Shutdown(void)
{
	search_close_progs(csqcprogs, false);
	if (csqcprogs)
	{
		CSQC_ForgetThreads();
		CloseProgs(csqcprogs);
		Con_Printf("Closed csqc\n");
	}
	csqcprogs = NULL;

	in_sensitivityscale = 1;
}

//when the qclib needs a file, it calls out to this function.
qbyte *CSQC_PRLoadFile (char *path, void *buffer, int bufsize)
{
	qbyte *file;

	if (!strcmp(path, "csprogs.dat"))
	{
		char newname[MAX_QPATH];
		snprintf(newname, MAX_QPATH, "csprogsvers/%x.dat", csqcchecksum);

		file = COM_LoadStackFile(newname, buffer, bufsize);
		if (file)
		{
			if (cls.protocol == CP_NETQUAKE)
			{
				if (QCRC_Block(file, com_filesize) == csqcchecksum)
					return file;
			}
			else
			{
				if (Com_BlockChecksum(file, com_filesize) == csqcchecksum)	//and the user wasn't trying to be cunning.
					return file;
			}
		}

		file = COM_LoadStackFile(path, buffer, bufsize);
		if (file && !cls.demoplayback)	//allow them to use csprogs.dat if playing a demo, and don't care about the checksum
		{
			if (cls.protocol == CP_NETQUAKE)
			{
				if (QCRC_Block(file, com_filesize) != csqcchecksum)
					return NULL;
			}
			else
			{
				if (Com_BlockChecksum(file, com_filesize) != csqcchecksum)
					return NULL;	//not valid
			}

			//back it up
			COM_WriteFile(newname, file, com_filesize);
		}

		return file;

	}

	return COM_LoadStackFile(path, buffer, bufsize);
}

int CSQC_PRFileSize (char *path)
{
	qbyte *file;

	if (!strcmp(path, "csprogs.dat"))
	{
		char newname[MAX_QPATH];
		snprintf(newname, MAX_QPATH, "csprogsvers/%x.dat", csqcchecksum);

		file = COM_LoadTempFile (newname);
		if (file)
		{
			if (cls.protocol == CP_NETQUAKE)
			{
				if (QCRC_Block(file, com_filesize) == csqcchecksum)
					return com_filesize+1;
			}
			else
			{
				if (Com_BlockChecksum(file, com_filesize) == csqcchecksum)	//and the user wasn't trying to be cunning.
					return com_filesize+1;
			}
		}

		file = COM_LoadTempFile(path);
		if (file && !cls.demoplayback)	//allow them to use csprogs.dat if playing a demo, and don't care about the checksum
		{
			if (cls.protocol == CP_NETQUAKE)
			{
				if (QCRC_Block(file, com_filesize) != csqcchecksum)
					return -1;	//not valid
			}
			else
			{
				if (Com_BlockChecksum(file, com_filesize) != csqcchecksum)
					return -1;	//not valid
			}
		}
		if (!file)
			return -1;

		return com_filesize;
	}

	return COM_FileSize(path);
}

double  csqctime;
qboolean CSQC_Init (unsigned int checksum)
{
	csqcchecksum = checksum;

	CSQC_Shutdown();

	if (!qrenderer)
	{
		return false;
	}

	if (cl_nocsqc.value)
		return false;

	memset(cl.model_csqcname, 0, sizeof(cl.model_csqcname));
	memset(cl.model_csqcprecache, 0, sizeof(cl.model_csqcprecache));

	csqcprogparms.progsversion = PROGSTRUCT_VERSION;
	csqcprogparms.ReadFile = CSQC_PRLoadFile;//char *(*ReadFile) (char *fname, void *buffer, int *len);
	csqcprogparms.FileSize = CSQC_PRFileSize;//int (*FileSize) (char *fname);	//-1 if file does not exist
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

	csqcprogparms.memalloc = PR_CB_Malloc;//void *(*memalloc) (int size);	//small string allocation	malloced and freed randomly
	csqcprogparms.memfree = PR_CB_Free;//void (*memfree) (void * mem);


	csqcprogparms.globalbuiltins = csqc_builtins;//builtin_t *globalbuiltins;	//these are available to all progs
	csqcprogparms.numglobalbuiltins = csqc_numbuiltins;

	csqcprogparms.autocompile = PR_NOCOMPILE;//enum {PR_NOCOMPILE, PR_COMPILENEXIST, PR_COMPILECHANGED, PR_COMPILEALWAYS} autocompile;

	csqcprogparms.gametime = &csqctime;

	csqcprogparms.sv_edicts = (edict_t **)&csqc_edicts;
	csqcprogparms.sv_num_edicts = &num_csqc_edicts;

	csqcprogparms.useeditor = QCEditor;//void (*useeditor) (char *filename, int line, int nump, char **parms);

	csqctime = Sys_DoubleTime();
	if (!csqcprogs)
	{
		in_sensitivityscale = 1;
		csqcprogs = InitProgs(&csqcprogparms);
		PR_Configure(csqcprogs, -1, 16);

		CSQC_InitFields();	//let the qclib know the field order that the engine needs.

		if (PR_LoadProgs(csqcprogs, "csprogs.dat", 0, NULL, 0) < 0) //no per-progs builtins.
		{
			CSQC_Shutdown();
			//failed to load or something
			return false;
		}
		if (setjmp(csqc_abort))
		{
			CSQC_Shutdown();
			return false;
		}

		num_csqc_edicts = 0;
		CS_ClearWorld();

		PF_InitTempStrings(csqcprogs);

		memset(csqcent, 0, sizeof(*csqcent)*maxcsqcentities);	//clear the server->csqc entity translations.

		csqcentsize = PR_InitEnts(csqcprogs, pr_csmaxedicts.value);

		CSQC_FindGlobals();

		ED_Alloc(csqcprogs);	//we need a word entity.
		//world edict becomes readonly
		EDICT_NUM(csqcprogs, 0)->readonly = true;
		EDICT_NUM(csqcprogs, 0)->isfree = false;

		if (csqcg.init_function)
			PR_ExecuteProgram(csqcprogs, csqcg.init_function);

		Con_Printf("Loaded csqc\n");
	}

	return true; //success!
}

void CSQC_CoreDump(void)
{
	if (!csqcprogs)
	{
		Con_Printf("Can't core dump, you need to be running the CSQC progs first.");
		return;
	}

	{
		int size = 1024*1024*8;
		char *buffer = BZ_Malloc(size);
		csqcprogs->save_ents(csqcprogs, buffer, &size, 3);
		COM_WriteFile("csqccore.txt", buffer, size);
		BZ_Free(buffer);
	}

}

void CSQC_RegisterCvarsAndThings(void)
{
	Cmd_AddCommand("coredump_csqc", CSQC_CoreDump);

	Cvar_Register(&pr_csmaxedicts, CSQCPROGSGROUP);
	Cvar_Register(&cl_csqcdebug, CSQCPROGSGROUP);
	Cvar_Register(&cl_nocsqc, CSQCPROGSGROUP);
	Cvar_Register(&pr_csqc_coreonerror, CSQCPROGSGROUP);
}

qboolean CSQC_DrawView(void)
{
	if (!csqcg.draw_function || !csqcprogs || !cl.worldmodel)
		return false;

	r_secondaryview = 0;

	CL_CalcClientTime();

	DropPunchAngle (0);
	if (cl.worldmodel)
		R_LessenStains();

	csqc_resortfrags = true;

	if (csqcg.clientcommandframe)
		*csqcg.clientcommandframe = cls.netchan.outgoing_sequence;
	if (csqcg.servercommandframe)
		*csqcg.servercommandframe = cl.ackedinputsequence;
	if (csqcg.intermission)
		*csqcg.intermission = cl.intermission;

	if (csqcg.view_angles)
	{
		csqcg.view_angles[0] = cl.viewangles[0][0];
		csqcg.view_angles[1] = cl.viewangles[0][1];
		csqcg.view_angles[2] = cl.viewangles[0][2];
	}

	if (csqcg.time)
		*csqcg.time = Sys_DoubleTime();

	CSQC_RunThreads();	//wake up any qc threads

	PR_ExecuteProgram(csqcprogs, csqcg.draw_function);

	return true;
}

qboolean CSQC_KeyPress(int key, qboolean down)
{
	void *pr_globals;

	if (!csqcprogs || !csqcg.input_event)
		return false;

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);
	G_FLOAT(OFS_PARM0) = !down;
	G_FLOAT(OFS_PARM1) = MP_TranslateFTEtoDPCodes(key);
	G_FLOAT(OFS_PARM2) = 0;

	PR_ExecuteProgram (csqcprogs, csqcg.input_event);

	return G_FLOAT(OFS_RETURN);
}
qboolean CSQC_MouseMove(float xdelta, float ydelta)
{
	void *pr_globals;

	if (!csqcprogs || !csqcg.input_event)
		return false;

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);
	G_FLOAT(OFS_PARM0) = 2;
	G_FLOAT(OFS_PARM1) = xdelta;
	G_FLOAT(OFS_PARM2) = ydelta;

	PR_ExecuteProgram (csqcprogs, csqcg.input_event);

	return G_FLOAT(OFS_RETURN);
}

qboolean CSQC_ConsoleCommand(char *cmd)
{
	void *pr_globals;
	if (!csqcprogs || !csqcg.console_command)
		return false;

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);
	(((string_t *)pr_globals)[OFS_PARM0] = PR_TempString(csqcprogs, cmd));

	PR_ExecuteProgram (csqcprogs, csqcg.console_command);
	return G_FLOAT(OFS_RETURN);
}

qboolean CSQC_StuffCmd(char *cmd)
{
	void *pr_globals;
	if (!csqcprogs || !csqcg.parse_stuffcmd)
		return false;

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);
	(((string_t *)pr_globals)[OFS_PARM0] = PR_TempString(csqcprogs, cmd));

	PR_ExecuteProgram (csqcprogs, csqcg.parse_stuffcmd);
	return true;
}
qboolean CSQC_CenterPrint(char *cmd)
{
	void *pr_globals;
	if (!csqcprogs || !csqcg.parse_centerprint)
		return false;

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);
	(((string_t *)pr_globals)[OFS_PARM0] = PR_TempString(csqcprogs, cmd));

	PR_ExecuteProgram (csqcprogs, csqcg.parse_centerprint);
	return G_FLOAT(OFS_RETURN);
}

//this protocol allows up to 32767 edicts.
#ifdef PEXT_CSQC

int CSQC_StartSound(int entnum, int channel, char *soundname, vec3_t pos, float vol, float attenuation)
{
	void *pr_globals;
	csqcedict_t *ent;

	if (!csqcprogs || !csqcg.serversound)
		return false;

	if (entnum >= maxcsqcentities)
	{
		maxcsqcentities = entnum+64;
		csqcent = BZ_Realloc(csqcent, sizeof(*csqcent)*maxcsqcentities);
	}
	ent = csqcent[entnum];
	if (!ent)
		return false;

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);

	*csqcg.self = EDICT_TO_PROG(csqcprogs, (void*)ent);
	G_FLOAT(OFS_PARM0) = channel;
	G_INT(OFS_PARM1) = PR_TempString(csqcprogs, soundname);
	VectorCopy(pos, G_VECTOR(OFS_PARM2));
	G_FLOAT(OFS_PARM3) = vol;
	G_FLOAT(OFS_PARM4) = attenuation;

	PR_ExecuteProgram(csqcprogs, csqcg.serversound);
	return G_FLOAT(OFS_RETURN);
}

void CSQC_ParseEntities(void)
{
	csqcedict_t *ent;
	unsigned short entnum;
	void *pr_globals;
	int packetsize;
	int packetstart;

	if (!csqcprogs)
		Host_EndGame("CSQC needs to be initialized for this server.\n");

	if (!csqcg.ent_update || !csqcg.self)
		Host_EndGame("CSQC is unable to parse entities\n");

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);

	if (csqcg.time)
		*csqcg.time = Sys_DoubleTime();

	if (csqcg.clientcommandframe)
		*csqcg.clientcommandframe = cls.netchan.outgoing_sequence;
	if (csqcg.servercommandframe)
		*csqcg.servercommandframe = cl.ackedinputsequence;

	for(;;)
	{
		entnum = MSG_ReadShort();
		if (!entnum || msg_badread)
			break;
		if (entnum & 0x8000)
		{	//remove
			entnum &= ~0x8000;

			if (!entnum)
				Host_EndGame("CSQC cannot remove world!\n");

			if (entnum >= maxcsqcentities)
			{
				maxcsqcentities = entnum+64;
				csqcent = BZ_Realloc(csqcent, sizeof(*csqcent)*maxcsqcentities);
			}
			if (cl_csqcdebug.value)
				Con_Printf("Remove %i\n", entnum);

			ent = csqcent[entnum];
			csqcent[entnum] = NULL;

			if (!ent)	//hrm.
				continue;

			*csqcg.self = EDICT_TO_PROG(csqcprogs, (void*)ent);
			PR_ExecuteProgram(csqcprogs, csqcg.ent_remove);
			//the csqc is expected to call the remove builtin.
		}
		else
		{
			if (entnum >= maxcsqcentities)
			{
				maxcsqcentities = entnum+64;
				csqcent = BZ_Realloc(csqcent, sizeof(*csqcent)*maxcsqcentities);
			}

			if (cl.csqcdebug)
			{
				packetsize = MSG_ReadShort();
				packetstart = msg_readcount;
			}
			else
			{
				packetsize = 0;
				packetstart = 0;
			}

			ent = csqcent[entnum];
			if (!ent)
			{
				ent = (csqcedict_t*)ED_Alloc(csqcprogs);
				csqcent[entnum] = ent;
				ent->v->entnum = entnum;
				G_FLOAT(OFS_PARM0) = true;

				if (cl_csqcdebug.value)
					Con_Printf("Add %i\n", entnum);
			}
			else
			{
				G_FLOAT(OFS_PARM0) = false;
				if (cl_csqcdebug.value)
					Con_Printf("Update %i\n", entnum);
			}

			*csqcg.self = EDICT_TO_PROG(csqcprogs, (void*)ent);
			PR_ExecuteProgram(csqcprogs, csqcg.ent_update);

			if (cl.csqcdebug)
			{
				if (msg_readcount != packetstart+packetsize)
				{
					if (msg_readcount > packetstart+packetsize)
						Con_Printf("CSQC overread entity %i. Size %i, read %i\n", entnum, packetsize, msg_readcount - packetsize);
					else
						Con_Printf("CSQC underread entity %i. Size %i, read %i\n", entnum, packetsize, msg_readcount - packetsize);
					Con_Printf("First byte is %i\n", net_message.data[msg_readcount]);
#ifndef CLIENTONLY
					if (sv.state)
					{
						Con_Printf("Server classname: \"%s\"\n", PR_GetString(svprogfuncs, EDICT_NUM(svprogfuncs, entnum)->v->classname));
					}
#endif
				}
				msg_readcount = packetstart+packetsize;	//leetism.
			}
		}
	}
}
#endif

#endif
