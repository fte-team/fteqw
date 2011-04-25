#include "quakedef.h"

/*

  EXT_CSQC is the 'root' extension
  EXT_CSQC_1 are a collection of additional features to cover omissions in the original spec


  note the CHEAT_PARANOID define disables certain EXT_CSQC_1 features,
  in an attempt to prevent the player from finding out where he/she is, thus preventing aimbots.
  This is specifically targetted towards deathmatch mods where each player is a single player.
  In reality, this paranoia provides nothing which could not be done with a cheat proxy.
  Seeing as the client ensures hashes match in the first place, this paranoia gives nothing in the long run.
  Unfortunatly EXT_CSQC was designed around this paranoia.
*/

#ifdef CSQC_DAT

#ifdef GLQUAKE
#include "glquake.h"	//evil to include this
#endif
#include "shader.h"

//#define CHEAT_PARANOID

#include "pr_common.h"

#ifdef CLIENTONLY
//client only builds don't have a qc debugger
#define QCEditor NULL
#endif

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
static int csqc_fakereadbyte;
world_t csqc_world;

static int csqc_lplayernum;
static qboolean csqc_isdarkplaces;

static char csqc_printbuffer[8192];

#define CSQCPROGSGROUP "CSQC progs control"
cvar_t	pr_csmaxedicts = CVAR("pr_csmaxedicts", "3072");	//not tied to protocol nor server.
cvar_t	cl_csqcdebug = CVAR("cl_csqcdebug", "0");	//prints entity numbers which arrive (so I can tell people not to apply it to players...)
cvar_t  cl_nocsqc = CVAR("cl_nocsqc", "0");
cvar_t  pr_csqc_coreonerror = CVAR("pr_csqc_coreonerror", "1");


#define MASK_DELTA 1
#define MASK_STDVIEWMODEL 2


// standard effect cvars/sounds
extern cvar_t r_explosionlight;
extern sfx_t			*cl_sfx_wizhit;
extern sfx_t			*cl_sfx_knighthit;
extern sfx_t			*cl_sfx_tink1;
extern sfx_t			*cl_sfx_ric1;
extern sfx_t			*cl_sfx_ric2;
extern sfx_t			*cl_sfx_ric3;
extern sfx_t			*cl_sfx_r_exp3;


//shared constants
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

	//this is a DP-compatibility hack.
	VF_CL_VIEWANGLES_V = 33,
	VF_CL_VIEWANGLES_X = 34,
	VF_CL_VIEWANGLES_Y = 35,
	VF_CL_VIEWANGLES_Z = 36,

	//33-36 used by DP...
	VF_PERSPECTIVE = 200,
	//201 used by DP... WTF? CLEARSCREEN
	VF_LPLAYER = 202,
	VF_AFOV = 203,	//aproximate fov (match what the engine would normally use for the fov cvar). p0=fov, p1=zoom
} viewflags;

/*FIXME: this should be changed*/
#define CSQC_API_VERSION 1.0f

#define CSQCRF_VIEWMODEL		1 //Not drawn in mirrors
#define CSQCRF_EXTERNALMODEL	2 //drawn ONLY in mirrors
#define CSQCRF_DEPTHHACK		4 //fun depthhack
#define CSQCRF_ADDITIVE			8 //add instead of blend
#define CSQCRF_USEAXIS			16 //use v_forward/v_right/v_up as an axis/matrix - predraw is needed to use this properly
#define CSQCRF_NOSHADOW			32 //don't cast shadows upon other entities (can still be self shadowing, if the engine wishes, and not additive)
#define CSQCRF_FRAMETIMESARESTARTTIMES 64 //EXT_CSQC_1: frame times should be read as (time-frametime).
#define CSQCRF_NOAUTOADD		128 //EXT_CSQC_1: don't automatically add after predraw was called


//If I do it like this, I'll never forget to register something...
#define csqcglobals	\
	globalfunction(init_function,		"CSQC_Init");	\
	globalfunction(worldloaded,			"CSQC_WorldLoaded");	\
	globalfunction(shutdown_function,	"CSQC_Shutdown");	\
	globalfunction(draw_function,		"CSQC_UpdateView");	\
	globalfunction(parse_stuffcmd,		"CSQC_Parse_StuffCmd");	\
	globalfunction(parse_centerprint,	"CSQC_Parse_CenterPrint");	\
	globalfunction(parse_print,			"CSQC_Parse_Print");	\
	globalfunction(input_event,			"CSQC_InputEvent");	\
	globalfunction(input_frame,			"CSQC_Input_Frame");/*EXT_CSQC_1*/	\
	globalfunction(console_command,		"CSQC_ConsoleCommand");	\
	\
	globalfunction(ent_update,			"CSQC_Ent_Update");	\
	globalfunction(ent_remove,			"CSQC_Ent_Remove");	\
	\
	globalfunction(event_sound,			"CSQC_Event_Sound");	\
	globalfunction(serversound,			"CSQC_ServerSound");/*obsolete, use event_sound*/	\
	globalfunction(loadresource,		"CSQC_LoadResource");/*EXT_CSQC_1*/	\
	globalfunction(parse_tempentity,	"CSQC_Parse_TempEntity");/*EXT_CSQC_ABSOLUTLY_VILE*/	\
	\
	/*These are pointers to the csqc's globals.*/	\
	globalfloat(svtime,					"time");				/*float		Written before entering most qc functions*/	\
	globalfloat(frametime,				"frametime");			/*float		Written before entering most qc functions*/	\
	globalfloat(cltime,					"cltime");				/*float		Written before entering most qc functions*/	\
	globalentity(self,					"self");				/*entity	Written before entering most qc functions*/	\
	globalentity(other,					"other");				/*entity	Written before entering most qc functions*/	\
	\
	globalfloat(maxclients,				"maxclients");			/*float		max number of players allowed*/	\
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
	globalfloat(trace_endcontents,		"trace_endcontents");	/*float		written by traceline EXT_CSQC_1*/	\
	\
	globalfloat(clientcommandframe,		"clientcommandframe");	/*float		the next frame that will be sent*/ \
	globalfloat(servercommandframe,		"servercommandframe");	/*float		the most recent frame received from the server*/ \
	\
	globalfloat(player_localentnum,		"player_localentnum");	/*float		the entity number of the local player*/	\
	globalfloat(player_localnum,		"player_localnum");		/*float		the entity number of the local player*/	\
	globalfloat(intermission,			"intermission");		/*float		set when the client receives svc_intermission*/	\
	globalvector(view_angles,			"view_angles");			/*float		set to the view angles at the start of each new frame (EXT_CSQC_1)*/ \
	\
	globalvector(pmove_org,				"pmove_org");			/*deprecated. read/written by runplayerphysics*/ \
	globalvector(pmove_vel,				"pmove_vel");			/*deprecated. read/written by runplayerphysics*/ \
	globalvector(pmove_mins,			"pmove_mins");			/*deprecated. read/written by runplayerphysics*/ \
	globalvector(pmove_maxs,			"pmove_maxs");			/*deprecated. read/written by runplayerphysics*/ \
	globalfloat(pmove_jump_held,		"pmove_jump_held");		/*deprecated. read/written by runplayerphysics*/ \
	globalfloat(pmove_waterjumptime,	"pmove_waterjumptime");	/*deprecated. read/written by runplayerphysics*/ \
	\
	globalfloat(input_timelength,		"input_timelength");	/*float		filled by getinputstate, read by runplayerphysics*/ \
	globalvector(input_angles,			"input_angles");		/*vector	filled by getinputstate, read by runplayerphysics*/ \
	globalvector(input_movevalues,		"input_movevalues");	/*vector	filled by getinputstate, read by runplayerphysics*/ \
	globalfloat(input_buttons,			"input_buttons");		/*float		filled by getinputstate, read by runplayerphysics*/ \
	globalfloat(input_impulse,			"input_impulse");		/*float		filled by getinputstate, read by runplayerphysics*/ \
	globalfloat(input_lightlevel,		"input_lightlevel");	/*unused float		filled by getinputstate, read by runplayerphysics*/ \
	globalfloat(input_weapon,			"input_weapon");		/*unused float		filled by getinputstate, read by runplayerphysics*/ \
	globalfloat(input_servertime,		"input_servertime");	/*float		filled by getinputstate, read by runplayerphysics*/ \
	globalfloat(input_clienttime,		"input_clienttime");	/*float		filled by getinputstate, read by runplayerphysics*/ \


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

static void CSQC_ChangeLocalPlayer(int lplayernum)
{
	csqc_lplayernum = lplayernum;
	if (csqcg.player_localentnum)
		*csqcg.player_localentnum = cl.playernum[lplayernum]+1;
	if (csqcg.player_localnum)
		*csqcg.player_localnum = cl.playernum[lplayernum];

	if (csqcg.view_angles)
	{
		csqcg.view_angles[0] = cl.viewangles[csqc_lplayernum][0];
		csqcg.view_angles[1] = cl.viewangles[csqc_lplayernum][1];
		csqcg.view_angles[2] = cl.viewangles[csqc_lplayernum][2];
	}

}

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

	if (csqcg.svtime)
		*csqcg.svtime = cl.servertime;
	if (csqcg.cltime)
		*csqcg.cltime = cl.time;

	CSQC_ChangeLocalPlayer(0);

	if (csqcg.maxclients)
		*csqcg.maxclients = cl.allocated_client_slots;
}



//this is the list for all the csqc fields.
//(the #define is so the list always matches the ones pulled out)
#define csqcextfields	\
	comfieldfloat(entnum);		\
	comfieldfloat(frame2);		/*EXT_CSQC_1*/\
	comfieldfloat(frame1time);	/*EXT_CSQC_1*/\
	comfieldfloat(frame2time);	/*EXT_CSQC_1*/\
	comfieldfloat(lerpfrac);	/*EXT_CSQC_1*/\
	comfieldfloat(renderflags);\
	comfieldfloat(forceshader);/*FTE_CSQC_SHADERS*/\
							\
	comfieldfloat(baseframe);	/*FTE_CSQC_BASEFRAME*/\
	comfieldfloat(baseframe2);	/*FTE_CSQC_BASEFRAME*/\
	comfieldfloat(baseframe1time);	/*FTE_CSQC_BASEFRAME*/\
	comfieldfloat(baseframe2time);	/*FTE_CSQC_BASEFRAME*/\
	comfieldfloat(baselerpfrac);	/*FTE_CSQC_BASEFRAME*/\
	comfieldfloat(basebone);	/*FTE_CSQC_BASEFRAME*/\
							\
  	comfieldfloat(bonecontrol1);	/*FTE_CSQC_HALFLIFE_MODELS*/\
	comfieldfloat(bonecontrol2);	/*FTE_CSQC_HALFLIFE_MODELS*/\
	comfieldfloat(bonecontrol3);	/*FTE_CSQC_HALFLIFE_MODELS*/\
	comfieldfloat(bonecontrol4);	/*FTE_CSQC_HALFLIFE_MODELS*/\
	comfieldfloat(bonecontrol5);	/*FTE_CSQC_HALFLIFE_MODELS*/\
	comfieldfloat(subblendfrac);	/*FTE_CSQC_HALFLIFE_MODELS*/\
	comfieldfloat(basesubblendfrac);	/*FTE_CSQC_HALFLIFE_MODELS+FTE_CSQC_BASEFRAME*/\
							\
	comfieldfloat(skeletonindex);		/*FTE_CSQC_SKELETONOBJECTS*/\
							\
	comfieldfloat(drawmask);	/*So that the qc can specify all rockets at once or all bannanas at once*/	\
	comfieldfunction(predraw);	/*If present, is called just before it's drawn.*/	\
							\
	comfieldfloat(ideal_pitch);\
	comfieldfloat(pitch_speed);\



//note: doesn't even have to match the clprogs.dat :)
typedef struct {

#define comfieldfloat(ssqcname,sharedname,csqcname) float csqcname
#define comfieldvector(ssqcname,sharedname,csqcname) vec3_t csqcname
#define comfieldentity(ssqcname,sharedname,csqcname) int csqcname
#define comfieldstring(ssqcname,sharedname,csqcname) string_t csqcname
#define comfieldfunction(ssqcname,sharedname,csqcname) func_t csqcname
comqcfields
#undef comfieldfloat
#undef comfieldvector
#undef comfieldentity
#undef comfieldstring
#undef comfieldfunction

#ifdef VM_Q1
} csqcentvars_t;
typedef struct {
#endif

#define comfieldfloat(name) float name
#define comfieldvector(name) vec3_t name
#define comfieldentity(name) int name
#define comfieldstring(name) string_t name
#define comfieldfunction(name) func_t name
comextqcfields
csqcextfields
#undef comfieldfloat
#undef comfieldvector
#undef comfieldentity
#undef comfieldstring
#undef comfieldfunction

#ifdef VM_Q1
} csqcextentvars_t;
#else
} csqcentvars_t;
#endif

typedef struct csqcedict_s
{
	qboolean	isfree;
	float		freetime; // sv.time when the object was freed
	int			entnum;
	qboolean	readonly;	//world
#ifdef VM_Q1
	csqcentvars_t	*v;
	csqcextentvars_t	*xv;
#else
	union {
		csqcentvars_t	*v;
		csqcentvars_t	*xv;
	};
#endif
	/*the above is shared with qclib*/
	link_t	area;
	pvscache_t pvsinfo;
#ifdef USEODE
	entityode_t ode;
#endif
	qbyte solidtype;
	/*the above is shared with ssqc*/

	//add whatever you wish here
	trailstate_t *trailstate;
} csqcedict_t;


static void CSQC_InitFields(void)
{	//CHANGING THIS FUNCTION REQUIRES CHANGES TO csqcentvars_t
#define comfieldfloat(ssqcname,wname,name) PR_RegisterFieldVar(csqcprogs, ev_float, #name, (size_t)&((csqcentvars_t*)0)->name, -1)
#define comfieldvector(ssqcname,wname,name) PR_RegisterFieldVar(csqcprogs, ev_vector, #name, (size_t)&((csqcentvars_t*)0)->name, -1)
#define comfieldentity(ssqcname,wname,name) PR_RegisterFieldVar(csqcprogs, ev_entity, #name, (size_t)&((csqcentvars_t*)0)->name, -1)
#define comfieldstring(ssqcname,wname,name) PR_RegisterFieldVar(csqcprogs, ev_string, #name, (size_t)&((csqcentvars_t*)0)->name, -1)
#define comfieldfunction(ssqcname,wname,name) PR_RegisterFieldVar(csqcprogs, ev_function, #name, (size_t)&((csqcentvars_t*)0)->name, -1)
comqcfields
#undef comfieldfloat
#undef comfieldvector
#undef comfieldentity
#undef comfieldstring
#undef comfieldfunction

#ifdef VM_Q1
#define comfieldfloat(name) PR_RegisterFieldVar(csqcprogs, ev_float, #name, sizeof(csqcentvars_t) + (size_t)&((csqcextentvars_t*)0)->name, -1)
#define comfieldvector(name) PR_RegisterFieldVar(csqcprogs, ev_vector, #name, sizeof(csqcentvars_t) + (size_t)&((csqcextentvars_t*)0)->name, -1)
#define comfieldentity(name) PR_RegisterFieldVar(csqcprogs, ev_entity, #name, sizeof(csqcentvars_t) + (size_t)&((csqcextentvars_t*)0)->name, -1)
#define comfieldstring(name) PR_RegisterFieldVar(csqcprogs, ev_string, #name, sizeof(csqcentvars_t) + (size_t)&((csqcextentvars_t*)0)->name, -1)
#define comfieldfunction(name) PR_RegisterFieldVar(csqcprogs, ev_function, #name, sizeof(csqcentvars_t) + (size_t)&((csqcextentvars_t*)0)->name, -1)
#else
#define comfieldfloat(name) PR_RegisterFieldVar(csqcprogs, ev_float, #name, (size_t)&((csqcentvars_t*)0)->name, -1)
#define comfieldvector(name) PR_RegisterFieldVar(csqcprogs, ev_vector, #name, (size_t)&((csqcentvars_t*)0)->name, -1)
#define comfieldentity(name) PR_RegisterFieldVar(csqcprogs, ev_entity, #name, (size_t)&((csqcentvars_t*)0)->name, -1)
#define comfieldstring(name) PR_RegisterFieldVar(csqcprogs, ev_string, #name, (size_t)&((csqcentvars_t*)0)->name, -1)
#define comfieldfunction(name) PR_RegisterFieldVar(csqcprogs, ev_function, #name, (size_t)&((csqcentvars_t*)0)->name, -1)
#endif
comextqcfields
csqcextfields
#undef fieldfloat
#undef fieldvector
#undef fieldentity
#undef fieldstring
#undef fieldfunction
}

static csqcedict_t **csqcent;
static int maxcsqcentities;

static int csqcentsize;

static char *csqcmapentitydata;
static qboolean csqcmapentitydataloaded;



#define MAX_SKEL_OBJECTS 1024

typedef struct {
	int inuse;

	model_t *model;
	qboolean absolute;

	unsigned int numbones;
	float *bonematrix;
} skelobject_t;

skelobject_t skelobjects[MAX_SKEL_OBJECTS];
int numskelobjectsused;

skelobject_t *skel_get(progfuncs_t *prinst, int skelidx, int bonecount);
void skel_dodelete(void);


qboolean csqc_deprecated_warned;
#define csqc_deprecated(s) do {if (!csqc_deprecated_warned){Con_Printf("csqc warning: %s\n", s); csqc_deprecated_warned = true;}}while(0)


static model_t *CSQC_GetModelForIndex(int index);

static void CS_CheckVelocity(csqcedict_t *ent)
{
}









static void cs_getframestate(csqcedict_t *in, unsigned int rflags, framestate_t *out)
{
	//FTE_CSQC_HALFLIFE_MODELS
#ifdef HALFLIFEMODELS
	out->bonecontrols[0] = in->xv->bonecontrol1;
	out->bonecontrols[1] = in->xv->bonecontrol2;
	out->bonecontrols[2] = in->xv->bonecontrol3;
	out->bonecontrols[3] = in->xv->bonecontrol4;
	out->bonecontrols[4] = in->xv->bonecontrol5;
	out->g[FS_REG].subblendfrac = in->xv->subblendfrac;
	out->g[FST_BASE].subblendfrac = in->xv->subblendfrac;
#endif

	//FTE_CSQC_BASEFRAME
	out->g[FST_BASE].endbone = in->xv->basebone;
	if (out->g[FST_BASE].endbone)
	{	//small optimisation.
		out->g[FST_BASE].endbone -= 1;

		out->g[FST_BASE].frame[0] = in->xv->baseframe;
		out->g[FST_BASE].frame[1] = in->xv->baseframe2;
		out->g[FST_BASE].lerpfrac = in->xv->baselerpfrac;
		if (rflags & CSQCRF_FRAMETIMESARESTARTTIMES)
		{
			out->g[FST_BASE].frametime[0] = *csqcg.svtime - in->xv->baseframe1time;
			out->g[FST_BASE].frametime[1] = *csqcg.svtime - in->xv->baseframe2time;
		}
		else
		{
			out->g[FST_BASE].frametime[0] = in->xv->baseframe1time;
			out->g[FST_BASE].frametime[1] = in->xv->baseframe2time;
		}
	}

	//and the normal frames.
	out->g[FS_REG].frame[0] = in->v->frame;
	out->g[FS_REG].frame[1] = in->xv->frame2;
	out->g[FS_REG].lerpfrac = in->xv->lerpfrac;
	if (rflags & CSQCRF_FRAMETIMESARESTARTTIMES)
	{
		out->g[FS_REG].frametime[0] = *csqcg.svtime - in->xv->frame1time;
		out->g[FS_REG].frametime[1] = *csqcg.svtime - in->xv->frame2time;
	}
	else
	{
		out->g[FS_REG].frametime[0] = in->xv->frame1time;
		out->g[FS_REG].frametime[1] = in->xv->frame2time;
	}

	out->bonecount = 0;
	out->bonestate = NULL;
	if (in->xv->skeletonindex)
	{
		skelobject_t *so;
		so = skel_get(csqcprogs, in->xv->skeletonindex, 0);
		if (so && so->inuse == 1)
		{
			out->bonecount = so->numbones;
			out->bonestate = so->bonematrix;
		}
	}
}


static void QCBUILTIN PF_cs_remove (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ed;

	ed = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);

	if (ed->isfree)
	{
		csqc_deprecated("Tried removing free entity");
		return;
	}

	pe->DelinkTrailstate(&ed->trailstate);
	World_UnlinkEdict((wedict_t*)ed);
	ED_Free (prinst, (void*)ed);
}

static void QCBUILTIN PF_cvar (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	cvar_t	*var;
	char	*str;

	str = PR_GetStringOfs(prinst, OFS_PARM0);

	if (!strcmp(str, "vid_conwidth"))
	{
		csqc_deprecated("vid_conwidth cvar used");
		G_FLOAT(OFS_RETURN) = vid.width;
	}
	else if (!strcmp(str, "vid_conheight"))
	{
		csqc_deprecated("vid_conheight cvar used");
		G_FLOAT(OFS_RETURN) = vid.height;
	}
	else
	{
		var = Cvar_Get(str, "", 0, "csqc cvars");
		if (var)
			G_FLOAT(OFS_RETURN) = var->value;
		else
			G_FLOAT(OFS_RETURN) = 0;
	}
}

//too specific to the prinst's builtins.
static void QCBUILTIN PF_Fixme (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Con_Printf("\n");

	prinst->RunError(prinst, "\nBuiltin %i not implemented.\nCSQC is not compatible.", prinst->lastcalledbuiltinnumber);
	PR_BIError (prinst, "bulitin not implemented");
}
static void QCBUILTIN PF_NoCSQC (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Con_Printf("\n");

	prinst->RunError(prinst, "\nBuiltin %i does not make sense in csqc.\nCSQC is not compatible.", prinst->lastcalledbuiltinnumber);
	PR_BIError (prinst, "bulitin not implemented");
}

static void QCBUILTIN PF_cl_cprint (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *str = PF_VarString(prinst, 0, pr_globals);
	SCR_CenterPrint(csqc_lplayernum, str, true);
}

static void QCBUILTIN PF_cs_makevectors (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (!csqcg.forward || !csqcg.right || !csqcg.up)
		Host_EndGame("PF_makevectors: one of v_forward, v_right or v_up was not defined\n");
	AngleVectors (G_VECTOR(OFS_PARM0), csqcg.forward, csqcg.right, csqcg.up);
}

static model_t *CSQC_GetModelForIndex(int index)
{
	if (index == 0)
		return NULL;
	else if (index > 0 && index < MAX_MODELS)
		return cl.model_precache[index];
	else if (index < 0 && index > -MAX_CSQCMODELS)
	{
		if (!cl.model_csqcprecache[-index])
			cl.model_csqcprecache[-index] = Mod_ForName(cl.model_csqcname[-index], false);
		return cl.model_csqcprecache[-index];
	}
	else
		return NULL;
}

static qboolean CopyCSQCEdictToEntity(csqcedict_t *in, entity_t *out)
{
	int i, ival;
	model_t *model;
	unsigned int rflags;

	ival = in->v->modelindex;
	model = CSQC_GetModelForIndex(ival);
	if (!model)
		return false; //there might be other ent types later as an extension that stop this.

	memset(out, 0, sizeof(*out));
	out->model = model;

	if (in->xv->renderflags)
	{
		rflags = in->xv->renderflags;
		if (rflags & CSQCRF_VIEWMODEL)
			out->flags |= Q2RF_DEPTHHACK|Q2RF_WEAPONMODEL;
		if (rflags & CSQCRF_EXTERNALMODEL)
			out->externalmodelview = ~0;
		if (rflags & CSQCRF_DEPTHHACK)
			out->flags |= Q2RF_DEPTHHACK;
		if (rflags & CSQCRF_ADDITIVE)
			out->flags |= Q2RF_ADDITIVE;
		//CSQCRF_USEAXIS is below
		if (rflags & CSQCRF_NOSHADOW)
			out->flags |= RF_NOSHADOW;
		//CSQCRF_FRAMETIMESARESTARTTIMES is below
	}
	else
		rflags = 0;

	if ((int)in->v->effects & EF_NODEPTHTEST)
		out->flags |= RF_NODEPTHTEST;

	cs_getframestate(in, rflags, &out->framestate);

	VectorCopy(in->v->origin, out->origin);
	if (rflags & CSQCRF_USEAXIS)
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

		if (!in->xv->scale || in->xv->scale == 1.0f)
			out->scale = 1;
		else
		{
			VectorScale(out->axis[0], in->xv->scale, out->axis[0]);
			VectorScale(out->axis[1], in->xv->scale, out->axis[1]);
			VectorScale(out->axis[2], in->xv->scale, out->axis[2]);
			out->scale = in->xv->scale;
		}
	}

	ival = in->v->colormap;
	if (ival > 0 && ival <= MAX_CLIENTS)
	{
		out->scoreboard = &cl.players[ival-1];
	}
	// TODO: DP COLORMAP extension?

	out->shaderRGBAf[0] = 1;
	out->shaderRGBAf[1] = 1;
	out->shaderRGBAf[2] = 1;
	if (!in->xv->alpha || in->xv->alpha == 1)
	{
		out->shaderRGBAf[3] = 1.0f;
	}
	else
	{
		out->flags |= Q2RF_TRANSLUCENT;
		out->shaderRGBAf[3] = in->xv->alpha;
	}

	out->skinnum = in->v->skin;
	out->fatness = in->xv->fatness;
	ival = in->xv->forceshader;
	if (ival >= 1 && ival <= MAX_SHADERS)
		out->forcedshader = r_shaders + (ival-1);
	else
		out->forcedshader = NULL;

	out->keynum = -in->entnum;

	return true;
}

static void QCBUILTIN PF_cs_makestatic (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{	//still does a remove.
	csqcedict_t *in = (void*)G_EDICT(prinst, OFS_PARM0);
	entity_t *ent;

	if (cl.num_statics == cl_max_static_entities)
	{
		cl_max_static_entities += 16;
		cl_static_entities = BZ_Realloc(cl_static_entities, sizeof(*cl_static_entities) * cl_max_static_entities);
	}

	ent = &cl_static_entities[cl.num_statics].ent;
	if (CopyCSQCEdictToEntity(in, ent))
	{
		cl_static_entities[cl.num_statics].mdlidx = in->v->modelindex;
		cl.num_statics++;
	}

	PF_cs_remove(prinst, pr_globals);
}

static void QCBUILTIN PF_R_AddEntity(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *in = (void*)G_EDICT(prinst, OFS_PARM0);
	entity_t ent;
	if (in->isfree || in->entnum == 0)
	{
		csqc_deprecated("Tried drawing a free/removed/world entity\n");
		return;
	}

	if (CopyCSQCEdictToEntity(in, &ent))
		V_AddAxisEntity(&ent);
}

static void QCBUILTIN PF_R_DynamicLight_Set(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	dlight_t *l;
	unsigned int lno = G_FLOAT(OFS_PARM0);
	int field = G_FLOAT(OFS_PARM1);
	if (lno >= cl_maxdlights)
		return;
	l = cl_dlights+lno;
	switch (field)
	{
	case 0:
		VectorCopy(G_VECTOR(OFS_PARM2), l->origin);
		l->rebuildcache = true;
		break;
	case 1:
		VectorCopy(G_VECTOR(OFS_PARM2), l->color);
		break;
	case 2:
		l->radius = G_FLOAT(OFS_PARM2);
		l->rebuildcache = true;
		break;
	case 3:
		l->flags = G_FLOAT(OFS_PARM2);
		l->rebuildcache = true;
		break;
	case 4:
		l->style = G_FLOAT(OFS_PARM2);
		break;
	default:
		break;
	}
}
static void QCBUILTIN PF_R_DynamicLight_Get(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	dlight_t *l;
	unsigned int lno = G_FLOAT(OFS_PARM0);
	int field = G_FLOAT(OFS_PARM1);
	if (lno >= rtlights_max)
	{
		if (field == -1)
			G_FLOAT(OFS_RETURN) = rtlights_max;
		else
			G_INT(OFS_RETURN) = 0;
		return;
	}
	l = cl_dlights+lno;
	switch (field)
	{
	case 0:
		VectorCopy(l->origin, G_VECTOR(OFS_RETURN));
		break;
	case 1:
		VectorCopy(l->color, G_VECTOR(OFS_RETURN));
		break;
	case 2:
		G_FLOAT(OFS_RETURN) = l->radius;
		break;
	case 3:
		G_FLOAT(OFS_RETURN) = l->flags;
		break;
	case 4:
		G_FLOAT(OFS_RETURN) = l->style;
		break;
	default:
		G_INT(OFS_RETURN) = 0;
		break;
	}
}

static void QCBUILTIN PF_R_DynamicLight_Add(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);
	float radius = G_FLOAT(OFS_PARM1);
	float *rgb = G_VECTOR(OFS_PARM2);
//	float style = G_FLOAT(OFS_PARM3);
//	char *cubemapname = G_STRING(OFS_PARM4);
//	float pflags = G_FLOAT(OFS_PARM5);

	csqcedict_t *self;

	//if the org matches self, then attach it.
	self = (csqcedict_t*)PROG_TO_EDICT(prinst, *csqcg.self);
	if (VectorCompare(self->v->origin, org))
		G_FLOAT(OFS_RETURN) = V_AddLight(-self->entnum, org, radius, rgb[0]/5, rgb[1]/5, rgb[2]/5);
	else
		G_FLOAT(OFS_RETURN) = V_AddLight(0, org, radius, rgb[0]/5, rgb[1]/5, rgb[2]/5);
}

static void QCBUILTIN PF_R_AddEntityMask(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int mask = G_FLOAT(OFS_PARM0);
	csqcedict_t *ent;
	entity_t rent;
	int e;

	int oldself = *csqcg.self;

	if (cl.worldmodel)
	{
		if (mask & MASK_DELTA)
		{
			CL_LinkPlayers ();
			CL_LinkPacketEntities ();
		}
	}

	for (e=1; e < *prinst->parms->sv_num_edicts; e++)
	{
		ent = (void*)EDICT_NUM(prinst, e);
		if (ent->isfree)
			continue;

		if ((int)ent->xv->drawmask & mask)
		{
			if (ent->xv->predraw)
			{
				*csqcg.self = EDICT_TO_PROG(prinst, (void*)ent);
				PR_ExecuteProgram(prinst, ent->xv->predraw);

				if (ent->isfree || (int)ent->xv->renderflags & CSQCRF_NOAUTOADD)
					continue;	//bummer...
			}

			if (CopyCSQCEdictToEntity(ent, &rent))
				V_AddAxisEntity(&rent);
		}
	}
	*csqcg.self = oldself;

	if (cl.worldmodel)
	{
		if (mask & MASK_STDVIEWMODEL)
		{
			CL_LinkViewModel ();
		}
		CL_LinkProjectiles ();
		CL_UpdateTEnts ();
	}
}

static shader_t *csqc_shadern;
static int csqc_startpolyvert;
// #306 void(string texturename) R_BeginPolygon (EXT_CSQC_???)
static void QCBUILTIN PF_R_PolygonBegin(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqc_shadern = R_RegisterSkin(PR_GetStringOfs(prinst, OFS_PARM0));
	csqc_startpolyvert = cl_numstrisvert;
}

// #307 void(vector org, vector texcoords, vector rgb, float alpha) R_PolygonVertex (EXT_CSQC_???)
static void QCBUILTIN PF_R_PolygonVertex(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (cl_numstrisvert == cl_maxstrisvert)
	{
		cl_maxstrisvert+=64;
		cl_strisvertv = BZ_Realloc(cl_strisvertv, sizeof(*cl_strisvertv)*cl_maxstrisvert);
		cl_strisvertt = BZ_Realloc(cl_strisvertt, sizeof(*cl_strisvertt)*cl_maxstrisvert);
		cl_strisvertc = BZ_Realloc(cl_strisvertc, sizeof(*cl_strisvertc)*cl_maxstrisvert);
	}

	VectorCopy(G_VECTOR(OFS_PARM0), cl_strisvertv[cl_numstrisvert]);
	Vector2Copy(G_VECTOR(OFS_PARM1), cl_strisvertt[cl_numstrisvert]);
	VectorCopy(G_VECTOR(OFS_PARM2), cl_strisvertc[cl_numstrisvert]);
	cl_strisvertc[cl_numstrisvert][3] = G_FLOAT(OFS_PARM3);
	cl_numstrisvert++;
}

// #308 void() R_EndPolygon (EXT_CSQC_???)
static void QCBUILTIN PF_R_PolygonEnd(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	scenetris_t *t;
	int i;
	int nv;
	/*if the shader didn't change, continue with the old poly*/
	if (cl_numstris && cl_stris[cl_numstris-1].shader == csqc_shadern)
		t = &cl_stris[cl_numstris-1];
	else
	{
		if (cl_numstris == cl_maxstris)
		{
			cl_maxstris+=8;
			cl_stris = BZ_Realloc(cl_stris, sizeof(*cl_stris)*cl_maxstris);
		}
		t = &cl_stris[cl_numstris++];
		t->shader = csqc_shadern;
		t->firstidx = cl_numstrisidx;
		t->firstvert = csqc_startpolyvert;
		t->numvert = 0;
		t->numidx = 0;
	}

	nv = cl_numstrisvert-csqc_startpolyvert;
	if (cl_numstrisidx+(nv-2)*3 > cl_maxstrisidx)
	{
		cl_maxstrisidx=cl_numstrisidx+(nv-2)*3 + 64;
		cl_strisidx = BZ_Realloc(cl_strisidx, sizeof(*cl_strisidx)*cl_maxstrisidx);
	}

	/*build the triangle fan out of triangles*/
	for (i = 2; i < nv; i++)
	{
		cl_strisidx[cl_numstrisidx++] = t->numvert + 0;
		cl_strisidx[cl_numstrisidx++] = t->numvert + i-1;
		cl_strisidx[cl_numstrisidx++] = t->numvert + i;
	}
	t->numidx = cl_numstrisidx - t->firstidx;
	t->numvert += cl_numstrisvert-csqc_startpolyvert;

	/*set up ready for the next poly*/
	csqc_startpolyvert = cl_numstrisvert;
}


qboolean csqc_rebuildmatricies;
float csqc_proj_matrix[16];
float csqc_proj_matrix_inverse[16];
 void buildmatricies(void)
{
	float modelview[16];
	float proj[16];

	/*build modelview and projection*/
	Matrix4_ModelViewMatrix(modelview, r_refdef.viewangles, r_refdef.vieworg);
	Matrix4_Projection2(proj, r_refdef.fov_x, r_refdef.fov_y, 4);

	/*build the project matrix*/
	Matrix4_Multiply(proj, modelview, csqc_proj_matrix);

	/*build the unproject matrix (inverted project matrix)*/
	Matrix4_Invert(csqc_proj_matrix, csqc_proj_matrix_inverse);

	csqc_rebuildmatricies = false;
}
static void QCBUILTIN PF_cs_project (progfuncs_t *prinst, struct globalvars_s *pr_globals)
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

		Matrix4_Transform4(csqc_proj_matrix, v, tempv);

		tempv[0] /= tempv[3];
		tempv[1] /= tempv[3];
		tempv[2] /= tempv[3];

		out[0] = (1+tempv[0])/2;
		out[1] = 1-(1+tempv[1])/2;
		out[2] = tempv[2];

		out[0] = out[0]*r_refdef.vrect.width + r_refdef.vrect.x;
		out[1] = out[1]*r_refdef.vrect.height + r_refdef.vrect.y;

		if (tempv[3] < 0)
			out[2] *= -1;
	}
}
static void QCBUILTIN PF_cs_unproject (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (csqc_rebuildmatricies)
		buildmatricies();

	{
		float *in = G_VECTOR(OFS_PARM0);
		float *out = G_VECTOR(OFS_RETURN);
		float tx, ty;

		float v[4], tempv[4];

		tx = ((in[0]-r_refdef.vrect.x)/r_refdef.vrect.width);
		ty = ((in[1]-r_refdef.vrect.y)/r_refdef.vrect.height);
		ty = 1-ty;
		v[0] = tx*2-1;
		v[1] = ty*2-1;
		v[2] = in[2];//*2-1;
		v[3] = 1;

		//don't use 1, because the far clip plane really is an infinite distance away
		if (v[2] >= 1)
			v[2] = 0.999999;

		Matrix4_Transform4(csqc_proj_matrix_inverse, v, tempv);

		out[0] = tempv[0]/tempv[3];
		out[1] = tempv[1]/tempv[3];
		out[2] = tempv[2]/tempv[3];
	}
}

//float CalcFov (float fov_x, float width, float height);
//clear scene, and set up the default stuff.
static void QCBUILTIN PF_R_ClearScene (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	extern frame_t		*view_frame;
	extern player_state_t		*view_message;

	if (*prinst->callargc > 0)
		CSQC_ChangeLocalPlayer(G_FLOAT(OFS_PARM0));

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

	skel_dodelete();
	CL_SwapEntityLists();

	view_frame = &cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK];
	view_message = &view_frame->playerstate[cl.playernum[csqc_lplayernum]];
#ifdef NQPROT
	if (cls.protocol == CP_NETQUAKE || !view_message->messagenum)
		view_message->weaponframe = cl.stats[csqc_lplayernum][STAT_WEAPONFRAME];
#endif
	V_CalcRefdef(csqc_lplayernum);	//set up the defaults (for player 0)

	csqc_addcrosshair = false;
	csqc_drawsbar = false;
}

static void QCBUILTIN PF_R_SetViewFlag(progfuncs_t *prinst, struct globalvars_s *pr_globals)
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

	case VF_AFOV:
		{
			float frustumx, frustumy;
			frustumy = tan(p[0] * (M_PI/360)) * 0.75;
			if (*prinst->callargc > 2)
				frustumy *= G_FLOAT(OFS_PARM2);
			frustumx = frustumy * vid.width / vid.height /* / vid.pixelheight*/;
			r_refdef.fov_x = atan2(frustumx, 1) * (360/M_PI);
			r_refdef.fov_y = atan2(frustumy, 1) * (360/M_PI);
		}
		break;

	case VF_ORIGIN:
		VectorCopy(p, r_refdef.vieworg);
		cl.crouch[csqc_lplayernum] = 0;
		break;

	case VF_ORIGIN_Z:
		cl.crouch[csqc_lplayernum] = 0;
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

	case VF_CL_VIEWANGLES_V:
		VectorCopy(p, cl.viewangles[csqc_lplayernum]);
		break;
	case VF_CL_VIEWANGLES_X:
	case VF_CL_VIEWANGLES_Y:
	case VF_CL_VIEWANGLES_Z:
		cl.viewangles[csqc_lplayernum][parametertype-VF_CL_VIEWANGLES_X] = *p;
		break;

	case VF_CARTESIAN_ANGLES:
		Con_Printf(CON_WARNING "WARNING: CARTESIAN ANGLES ARE NOT YET SUPPORTED!\n");
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

static void QCBUILTIN PF_R_GetViewFlag(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	viewflags parametertype = G_FLOAT(OFS_PARM0);
	float *r = G_VECTOR(OFS_RETURN);

	r[0] = 0;
	r[1] = 0;
	r[2] = 0;

	switch(parametertype)
	{
	case VF_FOV:
		r[0] = r_refdef.fov_x;
		r[1] = r_refdef.fov_y;
		break;

	case VF_FOVX:
		*r = r_refdef.fov_x;
		break;

	case VF_FOVY:
		*r = r_refdef.fov_y;
		break;

#ifndef MINGW
#pragma message("fixme: AFOV not retrievable")
#endif
	case VF_AFOV:
		*r = r_refdef.fov_x;
		break;

	case VF_ORIGIN:
#ifdef CHEAT_PARANOID
		VectorClear(r);
#else
		VectorCopy(r_refdef.vieworg, r);
#endif
		break;

	case VF_ORIGIN_Z:
	case VF_ORIGIN_X:
	case VF_ORIGIN_Y:
#ifdef CHEAT_PARANOID
		*r = 0;
#else
		*r = r_refdef.vieworg[parametertype-VF_ORIGIN_X];
#endif
		break;

	case VF_ANGLES:
		VectorCopy(r_refdef.viewangles, r);
		break;
	case VF_ANGLES_X:
	case VF_ANGLES_Y:
	case VF_ANGLES_Z:
		*r = r_refdef.viewangles[parametertype-VF_ANGLES_X];
		break;

	case VF_CL_VIEWANGLES_V:
		VectorCopy(cl.viewangles[csqc_lplayernum], r);
		break;
	case VF_CL_VIEWANGLES_X:
	case VF_CL_VIEWANGLES_Y:
	case VF_CL_VIEWANGLES_Z:
		*r = cl.viewangles[csqc_lplayernum][parametertype-VF_CL_VIEWANGLES_X];
		break;

	case VF_CARTESIAN_ANGLES:
		Con_Printf(CON_WARNING "WARNING: CARTESIAN ANGLES ARE NOT YET SUPPORTED!\n");
		break;

	case VF_VIEWPORT:
		r[0] = r_refdef.vrect.width;
		r[1] = r_refdef.vrect.height;
		break;

	case VF_SIZE_X:
		*r = r_refdef.vrect.width;
		break;
	case VF_SIZE_Y:
		*r = r_refdef.vrect.height;
		break;
	case VF_SIZE:
		r[0] = r_refdef.vrect.width;
		r[1] = r_refdef.vrect.height;
		break;

	case VF_MIN_X:
		*r = r_refdef.vrect.x;
		break;
	case VF_MIN_Y:
		*r = r_refdef.vrect.y;
		break;
	case VF_MIN:
		r[0] = r_refdef.vrect.x;
		r[1] = r_refdef.vrect.y;
		break;

	case VF_DRAWWORLD:
		*r = !(r_refdef.flags&Q2RDF_NOWORLDMODEL);
		break;
	case VF_ENGINESBAR:
		*r = csqc_drawsbar;
		break;
	case VF_DRAWCROSSHAIR:
		*r = csqc_addcrosshair;
		break;

	case VF_PERSPECTIVE:
		*r = r_refdef.useperspective;
		break;

	default:
		Con_DPrintf("GetViewFlag: %i not recognised\n", parametertype);
		break;
	}
}

static void QCBUILTIN PF_R_RenderScene(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (cl.worldmodel)
		R_PushDlights ();

	r_refdef.currentplayernum = csqc_lplayernum;

	VectorCopy (r_refdef.vieworg, cl.viewent[csqc_lplayernum].origin);
	CalcGunAngle(csqc_lplayernum);

	R_RenderView();

#ifdef GLQUAKE
	if (qrenderer == QR_OPENGL)
	{
		GL_Set2D ();
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
		R2D_DrawCrosshair();
}

static void QCBUILTIN PF_cs_getstatf(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int stnum = G_FLOAT(OFS_PARM0);
	float val = cl.statsf[csqc_lplayernum][stnum];	//copy float into the stat
	G_FLOAT(OFS_RETURN) = val;
}
static void QCBUILTIN PF_cs_getstatbits(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{	//convert an int stat into a qc float.

	int stnum = G_FLOAT(OFS_PARM0);
	int val = cl.stats[csqc_lplayernum][stnum];
	if (*prinst->callargc > 1)
	{
		int first, count;
		first = G_FLOAT(OFS_PARM1);
		if (*prinst->callargc > 2)
			count = G_FLOAT(OFS_PARM2);
		else
			count = 1;
		G_FLOAT(OFS_RETURN) = (((unsigned int)val)&(((1<<count)-1)<<first))>>first;
	}
	else
		G_FLOAT(OFS_RETURN) = val;
}
static void QCBUILTIN PF_cs_getstats(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int stnum = G_FLOAT(OFS_PARM0);

	RETURN_TSTRING(cl.statsstr[csqc_lplayernum][stnum]);

	/*
	char out[17];

	//the network protocol byteswaps

	((unsigned int*)out)[0] = LittleLong(cl.stats[csqc_lplayernum][stnum+0]);
	((unsigned int*)out)[1] = LittleLong(cl.stats[csqc_lplayernum][stnum+1]);
	((unsigned int*)out)[2] = LittleLong(cl.stats[csqc_lplayernum][stnum+2]);
	((unsigned int*)out)[3] = LittleLong(cl.stats[csqc_lplayernum][stnum+3]);
	((unsigned int*)out)[4] = 0;	//make sure it's null terminated

	RETURN_TSTRING(out);*/
}

static void QCBUILTIN PF_cs_SetOrigin(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (void*)G_EDICT(prinst, OFS_PARM0);
	float *org = G_VECTOR(OFS_PARM1);

	VectorCopy(org, ent->v->origin);

	World_LinkEdict(&csqc_world, (wedict_t*)ent, false);
}

static void QCBUILTIN PF_cs_SetSize(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (void*)G_EDICT(prinst, OFS_PARM0);
	float *mins = G_VECTOR(OFS_PARM1);
	float *maxs = G_VECTOR(OFS_PARM2);

	VectorCopy(mins, ent->v->mins);
	VectorCopy(maxs, ent->v->maxs);

	World_LinkEdict(&csqc_world, (wedict_t*)ent, false);
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
		*csqcg.trace_ent = EDICT_TO_PROG(csqcprogs, (void*)csqc_world.edicts);
}

static void QCBUILTIN PF_cs_traceline(progfuncs_t *prinst, struct globalvars_s *pr_globals)
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

	savedhull = ent->xv->hull;
	ent->xv->hull = 0;
	trace = World_Move (&csqc_world, v1, mins, maxs, v2, nomonsters, (wedict_t*)ent);
	ent->xv->hull = savedhull;

	cs_settracevars(&trace);
}
static void QCBUILTIN PF_cs_tracebox(progfuncs_t *prinst, struct globalvars_s *pr_globals)
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

	savedhull = ent->xv->hull;
	ent->xv->hull = 0;
	trace = World_Move (&csqc_world, v1, mins, maxs, v2, nomonsters, (wedict_t*)ent);
	ent->xv->hull = savedhull;

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
		*csqcg.trace_ent = EDICT_TO_PROG(prinst, (void*)csqc_world.edicts);
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

	savedhull = tossent->xv->hull;
	tossent->xv->hull = 0;
	for (i = 0;i < 200;i++) // LordHavoc: sanity check; never trace more than 10 seconds
	{
		velocity[2] -= gravity;
		VectorScale (velocity, 0.05, move);
		VectorAdd (origin, move, end);
		trace = World_Move (&csqc_world, origin, tossent->v->mins, tossent->v->maxs, end, MOVE_NORMAL, (wedict_t*)tossent);
		VectorCopy (trace.endpos, origin);

		CS_CheckVelocity (tossent);

		if (trace.fraction < 1 && trace.ent && (void*)trace.ent != ignore)
			break;
	}
	tossent->xv->hull = savedhull;

	trace.fraction = 0; // not relevant
	return trace;
}
static void QCBUILTIN PF_cs_tracetoss (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	trace_t	trace;
	csqcedict_t	*ent;
	csqcedict_t	*ignore;

	ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	if (ent == (csqcedict_t*)csqc_world.edicts)
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
	*csqcg.trace_plane_dist =  trace.plane.dist;
	if (trace.ent)
		*csqcg.trace_ent = EDICT_TO_PROG(prinst, trace.ent);
	else
		*csqcg.trace_ent = EDICT_TO_PROG(prinst, (void*)csqc_world.edicts);
}

static int CS_PointContents(vec3_t org)
{
	if (!cl.worldmodel)
		return FTECONTENTS_EMPTY;
	return cl.worldmodel->funcs.PointContents(cl.worldmodel, NULL, org);
}
static void QCBUILTIN PF_cs_pointcontents(progfuncs_t *prinst, struct globalvars_s *pr_globals)
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
		if (modelindex <= -MAX_MODELS)
			return;
		ent->v->model = PR_SetString(prinst, cl.model_csqcname[-modelindex]);
		if (!cl.model_csqcprecache[-modelindex])
			cl.model_csqcprecache[-modelindex] = Mod_ForName(cl.model_csqcname[-modelindex], false);
		model = cl.model_csqcprecache[-modelindex];
	}
	else
	{
		if (modelindex >= MAX_MODELS)
			return;
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

	World_LinkEdict(&csqc_world, (wedict_t*)ent, false);
}

static void QCBUILTIN PF_cs_SetModel(progfuncs_t *prinst, struct globalvars_s *pr_globals)
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

		cl.model_csqcprecache[-freei] = NULL;
	}

	csqc_setmodel(prinst, ent, modelindex);
}
static void QCBUILTIN PF_cs_SetModelIndex(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (void*)G_EDICT(prinst, OFS_PARM0);
	int modelindex = G_FLOAT(OFS_PARM1);

	csqc_setmodel(prinst, ent, modelindex);
}
static void QCBUILTIN PF_cs_PrecacheModel(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int modelindex, freei;
	char *modelname = PR_GetStringOfs(prinst, OFS_PARM0);
	int i;

	if (!*modelname)
	{
		G_FLOAT(OFS_RETURN) = 0;
		return;
	}

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

		CL_CheckOrEnqueDownloadFile(modelname, modelname, 0);
		cl.model_csqcprecache[-freei] = NULL;
	}

	G_FLOAT(OFS_RETURN) = modelindex;
}
static void QCBUILTIN PF_cs_PrecacheSound(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *soundname = PR_GetStringOfs(prinst, OFS_PARM0);
	Sound_CheckDownload(soundname);
	S_PrecacheSound(soundname);
}

static void QCBUILTIN PF_cs_ModelnameForIndex(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int modelindex = G_FLOAT(OFS_PARM0);

	if (modelindex < 0)
		G_INT(OFS_RETURN) = (int)PR_SetString(prinst, cl.model_csqcname[-modelindex]);
	else
		G_INT(OFS_RETURN) = (int)PR_SetString(prinst, cl.model_name[modelindex]);
}

static void QCBUILTIN PF_ReadByte(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (csqc_fakereadbyte != -1)
	{
		G_FLOAT(OFS_RETURN) = csqc_fakereadbyte;
		csqc_fakereadbyte = -1;
	}
	else
	{
		G_FLOAT(OFS_RETURN) = MSG_ReadByte();
	}
}

static void QCBUILTIN PF_ReadChar(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = MSG_ReadChar();
}

static void QCBUILTIN PF_ReadShort(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = MSG_ReadShort();
}

static void QCBUILTIN PF_ReadEntityNum(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	unsigned short val;
	val = MSG_ReadShort();
	if (val & 0x8000)
	{	//our protocol only supports 15bits of revelent entity number (16th bit is used as 'remove').
		//so warn with badly coded mods.
		Con_Printf("ReadEntityNumber read bad entity number\n");
		G_FLOAT(OFS_RETURN)	= 0;
	}
	else
		G_FLOAT(OFS_RETURN) = val;
}

static void QCBUILTIN PF_ReadLong(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = MSG_ReadLong();
}

static void QCBUILTIN PF_ReadCoord(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = MSG_ReadCoord();
}

static void QCBUILTIN PF_ReadFloat(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = MSG_ReadFloat();
}

static void QCBUILTIN PF_ReadString(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *read = MSG_ReadString();

	RETURN_TSTRING(read);
}

static void QCBUILTIN PF_ReadAngle(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = MSG_ReadAngle();
}


static void QCBUILTIN PF_objerror (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char	*s;
	struct edict_s	*ed;

	s = PF_VarString(prinst, 0, pr_globals);
/*	Con_Printf ("======OBJECT ERROR in %s:\n%s\n", PR_GetString(pr_xfunction->s_name),s);
*/	ed = PROG_TO_EDICT(prinst, *csqcg.self);
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
	}
}

static void QCBUILTIN PF_cs_setsensativityscaler (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	in_sensitivityscale = G_FLOAT(OFS_PARM0);
}

static void QCBUILTIN PF_cs_pointparticles (progfuncs_t *prinst, struct globalvars_s *pr_globals)
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

static void QCBUILTIN PF_cs_trailparticles (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int efnum;
	csqcedict_t *ent;
	float *start = G_VECTOR(OFS_PARM2);
	float *end = G_VECTOR(OFS_PARM3);

	if (csqc_isdarkplaces)
	{
		efnum = G_FLOAT(OFS_PARM1)-1;
		ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	}
	else
	{
		efnum = G_FLOAT(OFS_PARM0)-1;
		ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM1);
	}

	if (!ent->entnum)	//world trails are non-state-based.
		pe->ParticleTrail(start, end, efnum, NULL);
	else
		pe->ParticleTrail(start, end, efnum, &ent->trailstate);
}

static void QCBUILTIN PF_cs_particleeffectnum (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *effectname = PR_GetStringOfs(prinst, OFS_PARM0);

	//keep the effectinfo synced between server and client.
	COM_Effectinfo_ForName(effectname);

	G_FLOAT(OFS_RETURN) = pe->FindParticleType(effectname)+1;
}

static void QCBUILTIN PF_cs_sendevent (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent;
	int i;
	char *eventname = PR_GetStringOfs(prinst, OFS_PARM0);
	char *argtypes = PR_GetStringOfs(prinst, OFS_PARM1);

	MSG_WriteByte(&cls.netchan.message, clc_qcrequest);
	for (i = 0; i < 6; i++)
	{
		if (argtypes[i] == 's')
		{
			MSG_WriteByte(&cls.netchan.message, ev_string);
			MSG_WriteString(&cls.netchan.message, PR_GetStringOfs(prinst, OFS_PARM2+i*3));
		}
		else if (argtypes[i] == 'f')
		{
			MSG_WriteByte(&cls.netchan.message, ev_float);
			MSG_WriteFloat(&cls.netchan.message, G_FLOAT(OFS_PARM2+i*3));
		}
		else if (argtypes[i] == 'i')
		{
			MSG_WriteByte(&cls.netchan.message, ev_integer);
			MSG_WriteFloat(&cls.netchan.message, G_FLOAT(OFS_PARM2+i*3));
		}
		else if (argtypes[i] == 'v')
		{
			MSG_WriteByte(&cls.netchan.message, ev_vector);
			MSG_WriteFloat(&cls.netchan.message, G_FLOAT(OFS_PARM2+i*3+0));
			MSG_WriteFloat(&cls.netchan.message, G_FLOAT(OFS_PARM2+i*3+1));
			MSG_WriteFloat(&cls.netchan.message, G_FLOAT(OFS_PARM2+i*3+2));
		}
		else if (argtypes[i] == 'e')
		{
			ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM2+i*3);
			MSG_WriteByte(&cls.netchan.message, ev_entity);
			MSG_WriteShort(&cls.netchan.message, ent->xv->entnum);
		}
		else
			break;
	}
	MSG_WriteByte(&cls.netchan.message, 0);
	MSG_WriteString(&cls.netchan.message, eventname);
}

static void cs_set_input_state (usercmd_t *cmd)
{
	if (csqcg.input_timelength)
		*csqcg.input_timelength = cmd->msec/1000.0f;
	if (csqcg.input_angles)
	{
		csqcg.input_angles[0] = SHORT2ANGLE(cmd->angles[0]);
		csqcg.input_angles[1] = SHORT2ANGLE(cmd->angles[1]);
		csqcg.input_angles[2] = SHORT2ANGLE(cmd->angles[2]);
	}
	if (csqcg.input_movevalues)
	{
		csqcg.input_movevalues[0] = cmd->forwardmove;
		csqcg.input_movevalues[1] = cmd->sidemove;
		csqcg.input_movevalues[2] = cmd->upmove;
	}
	if (csqcg.input_buttons)
		*csqcg.input_buttons = cmd->buttons;

	if (csqcg.input_impulse)
		*csqcg.input_impulse = cmd->impulse;
	if (csqcg.input_lightlevel)
		*csqcg.input_lightlevel = cmd->lightlevel;
	if (csqcg.input_weapon)
		*csqcg.input_weapon = cmd->weapon;
	if (csqcg.input_servertime)
		*csqcg.input_servertime = cmd->servertime/1000.0f;
	if (csqcg.input_clienttime)
		*csqcg.input_clienttime = cmd->fclienttime/1000.0f;
}

static void cs_get_input_state (usercmd_t *cmd)
{
	if (csqcg.input_timelength)
		cmd->msec = *csqcg.input_timelength*1000;
	if (csqcg.input_angles)
	{
		cmd->angles[0] = ANGLE2SHORT(csqcg.input_angles[0]);
		cmd->angles[1] = ANGLE2SHORT(csqcg.input_angles[1]);
		cmd->angles[2] = ANGLE2SHORT(csqcg.input_angles[2]);
	}
	if (csqcg.input_movevalues)
	{
		cmd->forwardmove = csqcg.input_movevalues[0];
		cmd->sidemove = csqcg.input_movevalues[1];
		cmd->upmove = csqcg.input_movevalues[2];
	}
	if (csqcg.input_buttons)
		cmd->buttons = *csqcg.input_buttons;

	if (csqcg.input_impulse)
		cmd->impulse = *csqcg.input_impulse;
	if (csqcg.input_lightlevel)
		cmd->lightlevel = *csqcg.input_lightlevel;
	if (csqcg.input_weapon)
		cmd->weapon = *csqcg.input_weapon;
	if (csqcg.input_servertime)
		cmd->servertime = *csqcg.input_servertime*1000;
}

//get the input commands, and stuff them into some globals.
static void QCBUILTIN PF_cs_getinputstate (progfuncs_t *prinst, struct globalvars_s *pr_globals)
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
	cmd = &cl.frames[f&UPDATE_MASK].cmd[csqc_lplayernum];

	cs_set_input_state(cmd);

	G_FLOAT(OFS_RETURN) = true;
}

//read lots of globals, run the default player physics, write lots of globals.
//not intended to affect client state at all
static void QCBUILTIN PF_cs_runplayerphysics (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	unsigned int msecs;
	extern vec3_t	player_mins;
	extern vec3_t	player_maxs;

	csqcedict_t *ent;
	if (*prinst->callargc >= 1)
		ent = (void*)G_EDICT(prinst, OFS_PARM0);
	else
		ent = NULL;

	if (!cl.worldmodel)
		return;	//urm..

	//debugging field
	pmove.sequence = *csqcg.clientcommandframe;

	pmove.pm_type = PM_NORMAL;

	pmove.jump_msec = 0;//(cls.z_ext & Z_EXT_PM_TYPE) ? 0 : from->jump_msec;

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

	if (ent)
	{
		pmove.jump_held = (int)ent->xv->pmove_flags & PMF_JUMP_HELD;
		pmove.waterjumptime = 0;
		VectorCopy(ent->v->origin, pmove.origin);
		VectorCopy(ent->v->velocity, pmove.velocity);
		VectorCopy(ent->v->maxs, player_maxs);
		VectorCopy(ent->v->mins, player_mins);
	}
	else
	{
		csqc_deprecated("runplayerphysics with no ent");

		if (csqcg.pmove_jump_held)
			pmove.jump_held = *csqcg.pmove_jump_held;
		if (csqcg.pmove_waterjumptime)
			pmove.waterjumptime = *csqcg.pmove_waterjumptime;
		VectorCopy(csqcg.pmove_org, pmove.origin);
		VectorCopy(csqcg.pmove_vel, pmove.velocity);
		VectorCopy(csqcg.pmove_maxs, player_maxs);
		VectorCopy(csqcg.pmove_mins, player_mins);
	}
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

	if (ent)
	{
		VectorCopy(pmove.angles, ent->v->angles);
		ent->v->angles[0] *= -1/3.0f;
		VectorCopy(pmove.origin, ent->v->origin);
		VectorCopy(pmove.velocity, ent->v->velocity);
		ent->xv->pmove_flags = 0;
		ent->xv->pmove_flags += pmove.jump_held ? PMF_JUMP_HELD : 0;
		ent->xv->pmove_flags += pmove.onladder ? PMF_LADDER : 0;
	}
	else
	{
		//Legacy path
		if (csqcg.pmove_jump_held)
			*csqcg.pmove_jump_held = pmove.jump_held;
		if (csqcg.pmove_waterjumptime)
			*csqcg.pmove_waterjumptime = pmove.waterjumptime;
		VectorCopy(pmove.origin, csqcg.pmove_org);
		VectorCopy(pmove.velocity, csqcg.pmove_vel);
	}

	//fixme: touch solids

	World_LinkEdict (&csqc_world, (wedict_t*)ent, true);
}

static void QCBUILTIN PF_cs_getentitytoken (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (!csqcmapentitydata)
	{
		//nothing more to parse
		G_INT(OFS_RETURN) = 0;
	}
	else
	{
		com_tokentype = TTP_LINEENDING;
		while(com_tokentype == TTP_LINEENDING)
		{
			csqcmapentitydata = COM_ParseToken(csqcmapentitydata, "{}()\'\":,");
		}
		RETURN_TSTRING(com_token);
	}
}

static void CheckSendPings(void)
{	//quakeworld sends a 'pings' client command to retrieve the frequently updating stuff
	if (realtime - cl.last_ping_request > 2)
	{
		cl.last_ping_request = realtime;
		CL_SendClientCommand(true, "pings");
	}
}

static void QCBUILTIN PF_cs_serverkey (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *keyname = PF_VarString(prinst, 0, pr_globals);
	char *ret;
	char adr[MAX_ADR_SIZE];

	if (!strcmp(keyname, "ip"))
		ret = NET_AdrToString(adr, sizeof(adr), cls.netchan.remote_address);
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
			if (cls.fteprotocolextensions||cls.fteprotocolextensions2)
				ret = "QuakeWorld FTE";
			else if (cls.z_ext)
				ret = "QuakeWorld ZQuake";
			else
				ret = "QuakeWorld";
			break;
		case CP_NETQUAKE:
			switch (cls.protocol_nq)
			{
			default:
				ret = "NetQuake";
				break;
			case CPNQ_DP5:
				ret = "NetQuake DarkPlaces 5";
				break;
			case CPNQ_DP6:
				ret = "NetQuake DarkPlaces 6";
				break;
			case CPNQ_DP7:
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
static void QCBUILTIN PF_cs_getplayerkey (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char buffer[64];
	char *ret;
	int pnum = G_FLOAT(OFS_PARM0);
	char *keyname = PR_GetStringOfs(prinst, OFS_PARM1);
	if (pnum < 0)
	{
		if (csqc_resortfrags)
		{
			Sbar_SortFrags(false, false);
			csqc_resortfrags = false;
		}
		if (pnum >= -scoreboardlines)
		{//sort by
			pnum = fragsort[-(pnum+1)];
		}
	}

	if (pnum < 0 || pnum >= cl.allocated_client_slots)
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
	else if (!strcmp(keyname, "viewentity"))	//compat with DP
	{
		ret = buffer;
		sprintf(ret, "%i", pnum+1);
	}
#ifdef VOICECHAT
	else if (!strcmp(keyname, "voipspeaking"))
	{
		ret = buffer;
		sprintf(ret, "%i", S_Voip_Speaking(pnum));
	}
	else if (!strcmp(keyname, "voiploudness"))
	{
		ret = buffer;
		if (pnum == cl.playernum[0])
			sprintf(ret, "%i", S_Voip_Loudness(false));
		else
			*ret = 0;
	}
#endif
	else
	{
		ret = Info_ValueForKey(cl.players[pnum].userinfo, keyname);
	}
	if (*ret)
		RETURN_TSTRING(ret);
	else
		G_INT(OFS_RETURN) = 0;
}

static void QCBUILTIN PF_checkextension (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *extname = PR_GetStringOfs(prinst, OFS_PARM0);
	int i;
	for (i = 0; i < QSG_Extensions_count; i++)
	{
		if (!QSG_Extensions[i].name)
			continue;

		if (i < 32 && cls.protocol == CP_QUAKEWORLD)
			if (!(cls.fteprotocolextensions & (1<<i)))
				continue;

		if (!strcmp(QSG_Extensions[i].name, extname))
		{
			G_FLOAT(OFS_RETURN) = true;
			return;
		}
	}
	G_FLOAT(OFS_RETURN) = false;
}

static void QCBUILTIN PF_cs_sound(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char		*sample;
	int			channel;
	csqcedict_t		*entity;
	float volume;
	float attenuation;
	float pitchpct;

	sfx_t *sfx;

	entity = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	channel = G_FLOAT(OFS_PARM1);
	sample = PR_GetStringOfs(prinst, OFS_PARM2);
	volume = G_FLOAT(OFS_PARM3);
	attenuation = G_FLOAT(OFS_PARM4);
	if (*prinst->callargc >= 6)
		pitchpct = G_FLOAT(OFS_PARM5);
	else
		pitchpct = 0;

	sfx = S_PrecacheSound(sample);
	if (sfx)
		S_StartSound(-entity->entnum, channel, sfx, entity->v->origin, volume, attenuation, pitchpct);
};

static void QCBUILTIN PF_cs_pointsound(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *sample;
	float *origin;
	float volume;
	float attenuation;
	float pitchpct;

	sfx_t *sfx;

	origin = G_VECTOR(OFS_PARM0);
	sample = PR_GetStringOfs(prinst, OFS_PARM1);
	volume = G_FLOAT(OFS_PARM2);
	attenuation = G_FLOAT(OFS_PARM3);
	if (*prinst->callargc >= 5)
		pitchpct = G_FLOAT(OFS_PARM4);
	else
		pitchpct = 0;

	sfx = S_PrecacheSound(sample);
	if (sfx)
		S_StartSound(0, 0, sfx, origin, volume, attenuation, pitchpct);
}

static void QCBUILTIN PF_cs_particle(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);
	float *dir = G_VECTOR(OFS_PARM1);
	float colour = G_FLOAT(OFS_PARM2);
	float count = G_FLOAT(OFS_PARM2);

	pe->RunParticleEffect(org, dir, colour, count);
}
static void QCBUILTIN PF_cs_particle2(progfuncs_t *prinst, struct globalvars_s *pr_globals)
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

	pe->RunParticleEffect2 (org, dmin, dmax, colour, effect, count);
}

static void QCBUILTIN PF_cs_particle3(progfuncs_t *prinst, struct globalvars_s *pr_globals)
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

	pe->RunParticleEffect3(org, box, colour, effect, count);
}

static void QCBUILTIN PF_cs_particle4(progfuncs_t *prinst, struct globalvars_s *pr_globals)
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

	pe->RunParticleEffect4(org, radius, colour, effect, count);
}


void QCBUILTIN PF_cl_effect(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);
	char *name = PR_GetStringOfs(prinst, OFS_PARM1);
	float startframe = G_FLOAT(OFS_PARM2);
	float endframe = G_FLOAT(OFS_PARM3);
	float framerate = G_FLOAT(OFS_PARM4);
	model_t *mdl;

	mdl = Mod_ForName(name, false);
	if (mdl)
		CL_SpawnSpriteEffect(org, NULL, mdl, startframe, endframe, framerate, 1);
	else
		Con_Printf("PF_cl_effect: Couldn't load model %s\n", name);
}

void QCBUILTIN PF_cl_ambientsound(progfuncs_t *prinst, struct globalvars_s *pr_globals)
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

static void QCBUILTIN PF_cs_vectorvectors (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	VectorCopy(G_VECTOR(OFS_PARM0), csqcg.forward);
	VectorNormalize(csqcg.forward);
	VectorVectors(csqcg.forward, csqcg.right, csqcg.up);
}

static void QCBUILTIN PF_cs_lightstyle (progfuncs_t *prinst, struct globalvars_s *pr_globals)
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

static void QCBUILTIN PF_cs_changeyaw (progfuncs_t *prinst, struct globalvars_s *pr_globals)
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
static void QCBUILTIN PF_cs_changepitch (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t		*ent;
	float		ideal, current, move, speed;

	ent = (void*)PROG_TO_EDICT(prinst, *csqcg.self);
	current = anglemod( ent->v->angles[0] );
	ideal = ent->xv->ideal_pitch;
	speed = ent->xv->pitch_speed;

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

static void QCBUILTIN PF_cs_findradius (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t	*ent, *chain;
	float	rad;
	float	*org;
	vec3_t	eorg;
	int		i, j;

	chain = (csqcedict_t *)*prinst->parms->sv_edicts;

	org = G_VECTOR(OFS_PARM0);
	rad = G_FLOAT(OFS_PARM1);

	for (i=1 ; i<*prinst->parms->sv_num_edicts ; i++)
	{
		ent = (void*)EDICT_NUM(prinst, i);
		if (ent->isfree)
			continue;
//		if (ent->v->solid == SOLID_NOT && !sv_gameplayfix_blowupfallenzombies.value)
//			continue;
		for (j=0 ; j<3 ; j++)
			eorg[j] = org[j] - (ent->v->origin[j] + (ent->v->mins[j] + ent->v->maxs[j])*0.5);
		if (Length(eorg) > rad)
			continue;

		ent->v->chain = EDICT_TO_PROG(prinst, (void*)chain);
		chain = ent;
	}

	RETURN_EDICT(prinst, (void*)chain);
}

//entity(string field, float match) findchainflags = #450
//chained search for float, int, and entity reference fields
static void QCBUILTIN PF_cs_findchainflags (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i, f;
	int s;
	csqcedict_t	*ent, *chain;

	chain = (csqcedict_t *) *prinst->parms->sv_edicts;

	f = G_INT(OFS_PARM0)+prinst->fieldadjust;
	s = G_FLOAT(OFS_PARM1);

	for (i = 1; i < *prinst->parms->sv_num_edicts; i++)
	{
		ent = (csqcedict_t*)EDICT_NUM(prinst, i);
		if (ent->isfree)
			continue;
		if (!((int)((float *)ent->v)[f] & s))
			continue;

		ent->v->chain = EDICT_TO_PROG(prinst, (edict_t*)chain);
		chain = ent;
	}

	RETURN_EDICT(prinst, (edict_t*)chain);
}

//entity(string field, float match) findchainfloat = #403
static void QCBUILTIN PF_cs_findchainfloat (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i, f;
	float s;
	csqcedict_t	*ent, *chain;

	chain = (csqcedict_t *) *prinst->parms->sv_edicts;

	f = G_INT(OFS_PARM0)+prinst->fieldadjust;
	s = G_FLOAT(OFS_PARM1);

	for (i = 1; i < *prinst->parms->sv_num_edicts; i++)
	{
		ent = (csqcedict_t*)EDICT_NUM(prinst, i);
		if (ent->isfree)
			continue;
		if (((float *)ent->v)[f] != s)
			continue;

		ent->v->chain = EDICT_TO_PROG(prinst, (edict_t*)chain);
		chain = ent;
	}

	RETURN_EDICT(prinst, (edict_t*)chain);
}


//entity(string field, string match) findchain = #402
//chained search for strings in entity fields
static void QCBUILTIN PF_cs_findchain (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i, f;
	char *s;
	string_t t;
	csqcedict_t *ent, *chain;

	chain = (csqcedict_t *) *prinst->parms->sv_edicts;

	f = G_INT(OFS_PARM0)+prinst->fieldadjust;
	s = PR_GetStringOfs(prinst, OFS_PARM1);

	for (i = 1; i < *prinst->parms->sv_num_edicts; i++)
	{
		ent = (csqcedict_t*)EDICT_NUM(prinst, i);
		if (ent->isfree)
			continue;
		t = *(string_t *)&((float*)ent->v)[f];
		if (!t)
			continue;
		if (strcmp(PR_GetString(prinst, t), s))
			continue;

		ent->v->chain = EDICT_TO_PROG(prinst, (edict_t*)chain);
		chain = ent;
	}

	RETURN_EDICT(prinst, (edict_t*)chain);
}

static void QCBUILTIN PF_cl_te_gunshot (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	float scaler = 1;
	if (*prinst->callargc >= 2)	//fte is a quakeworld engine
		scaler = G_FLOAT(OFS_PARM1);
	if (P_RunParticleEffectType(pos, NULL, scaler, pt_gunshot))
		P_RunParticleEffect (pos, vec3_origin, 0, 20*scaler);
}
static void QCBUILTIN PF_cl_te_bloodqw (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	float scaler = 1;
	if (*prinst->callargc >= 2)	//fte is a quakeworld engine
		scaler = G_FLOAT(OFS_PARM1);
	if (P_RunParticleEffectType(pos, NULL, scaler, ptqw_blood))
		if (P_RunParticleEffectType(pos, NULL, scaler, ptdp_blood))
			P_RunParticleEffect (pos, vec3_origin, 73, 20*scaler);
}
static void QCBUILTIN PF_cl_te_blooddp (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	float *dir = G_VECTOR(OFS_PARM1);
	float scaler = G_FLOAT(OFS_PARM2);

	if (P_RunParticleEffectType(pos, dir, scaler, ptdp_blood))
		if (P_RunParticleEffectType(pos, dir, scaler, ptqw_blood))
			P_RunParticleEffect (pos, dir, 73, 20*scaler);
}
static void QCBUILTIN PF_cl_te_lightningblood (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	if (P_RunParticleEffectType(pos, NULL, 1, ptqw_lightningblood))
		P_RunParticleEffect (pos, vec3_origin, 225, 50);
}
static void QCBUILTIN PF_cl_te_spike (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	if (P_RunParticleEffectType(pos, NULL, 1, pt_spike))
		if (P_RunParticleEffectType(pos, NULL, 10, pt_gunshot))
			P_RunParticleEffect (pos, vec3_origin, 0, 10);
}
static void QCBUILTIN PF_cl_te_superspike (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	if (P_RunParticleEffectType(pos, NULL, 1, pt_superspike))
		if (P_RunParticleEffectType(pos, NULL, 2, pt_spike))
			if (P_RunParticleEffectType(pos, NULL, 20, pt_gunshot))
				P_RunParticleEffect (pos, vec3_origin, 0, 20);
}
static void QCBUILTIN PF_cl_te_explosion (progfuncs_t *prinst, struct globalvars_s *pr_globals)
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

	S_StartSound (-2, 0, cl_sfx_r_exp3, pos, 1, 1, 0);
}
static void QCBUILTIN PF_cl_te_tarexplosion (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	P_RunParticleEffectType(pos, NULL, 1, pt_tarexplosion);

	S_StartSound (-2, 0, cl_sfx_r_exp3, pos, 1, 1, 0);
}
static void QCBUILTIN PF_cl_te_wizspike (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	if (P_RunParticleEffectType(pos, NULL, 1, pt_wizspike))
		P_RunParticleEffect (pos, vec3_origin, 20, 30);

	S_StartSound (-2, 0, cl_sfx_knighthit, pos, 1, 1, 0);
}
static void QCBUILTIN PF_cl_te_knightspike (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	if (P_RunParticleEffectType(pos, NULL, 1, pt_knightspike))
		P_RunParticleEffect (pos, vec3_origin, 226, 20);

	S_StartSound (-2, 0, cl_sfx_knighthit, pos, 1, 1, 0);
}
static void QCBUILTIN PF_cl_te_lavasplash (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	P_RunParticleEffectType(pos, NULL, 1, pt_lavasplash);
}
static void QCBUILTIN PF_cl_te_teleport (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	P_RunParticleEffectType(pos, NULL, 1, pt_teleportsplash);
}
static void QCBUILTIN PF_cl_te_gunshotquad (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	if (P_RunParticleEffectTypeString(pos, vec3_origin, 1, "te_gunshotquad"))
		if (P_RunParticleEffectType(pos, NULL, 1, pt_gunshot))
			P_RunParticleEffect (pos, vec3_origin, 0, 20);
}
static void QCBUILTIN PF_cl_te_spikequad (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	if (P_RunParticleEffectTypeString(pos, vec3_origin, 1, "te_spikequad"))
		if (P_RunParticleEffectType(pos, NULL, 1, pt_spike))
			if (P_RunParticleEffectType(pos, NULL, 10, pt_gunshot))
				P_RunParticleEffect (pos, vec3_origin, 0, 10);
}
static void QCBUILTIN PF_cl_te_superspikequad (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	if (P_RunParticleEffectTypeString(pos, vec3_origin, 1, "te_superspikequad"))
		if (P_RunParticleEffectType(pos, NULL, 1, pt_superspike))
			if (P_RunParticleEffectType(pos, NULL, 2, pt_spike))
				if (P_RunParticleEffectType(pos, NULL, 20, pt_gunshot))
					P_RunParticleEffect (pos, vec3_origin, 0, 20);
}
static void QCBUILTIN PF_cl_te_explosionquad (progfuncs_t *prinst, struct globalvars_s *pr_globals)
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

	S_StartSound (-2, 0, cl_sfx_r_exp3, pos, 1, 1, 0);
}

//void(vector org, float radius, float lifetime, vector color) te_customflash
static void QCBUILTIN PF_cl_te_customflash (progfuncs_t *prinst, struct globalvars_s *pr_globals)
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

static void QCBUILTIN PF_cl_te_bloodshower (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
}
static void QCBUILTIN PF_cl_te_particlecube (progfuncs_t *prinst, struct globalvars_s *pr_globals)
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
static void QCBUILTIN PF_cl_te_spark (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
}
static void QCBUILTIN PF_cl_te_smallflash (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
}
static void QCBUILTIN PF_cl_te_explosion2 (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
}
static void QCBUILTIN PF_cl_te_lightning1 (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	float *start = G_VECTOR(OFS_PARM1);
	float *end = G_VECTOR(OFS_PARM2);

	CL_AddBeam(0, ent->entnum+MAX_EDICTS, start, end);
}
static void QCBUILTIN PF_cl_te_lightning2 (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	float *start = G_VECTOR(OFS_PARM1);
	float *end = G_VECTOR(OFS_PARM2);

	CL_AddBeam(1, ent->entnum+MAX_EDICTS, start, end);
}
static void QCBUILTIN PF_cl_te_lightning3 (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	float *start = G_VECTOR(OFS_PARM1);
	float *end = G_VECTOR(OFS_PARM2);

	CL_AddBeam(2, ent->entnum+MAX_EDICTS, start, end);
}
static void QCBUILTIN PF_cl_te_beam (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	float *start = G_VECTOR(OFS_PARM1);
	float *end = G_VECTOR(OFS_PARM2);

	CL_AddBeam(5, ent->entnum+MAX_EDICTS, start, end);
}
static void QCBUILTIN PF_cl_te_plasmaburn (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
}
static void QCBUILTIN PF_cl_te_explosionrgb (progfuncs_t *prinst, struct globalvars_s *pr_globals)
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

	S_StartSound (-2, 0, cl_sfx_r_exp3, org, 1, 1, 0);
}
static void QCBUILTIN PF_cl_te_particlerain (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *min = G_VECTOR(OFS_PARM0);
	float *max = G_VECTOR(OFS_PARM1);
	float *vel = G_VECTOR(OFS_PARM2);
	float howmany = G_FLOAT(OFS_PARM3);
	float colour = G_FLOAT(OFS_PARM4);

	P_RunParticleWeather(min, max, vel, howmany, colour, "rain");
}
static void QCBUILTIN PF_cl_te_particlesnow (progfuncs_t *prinst, struct globalvars_s *pr_globals)
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

static void QCBUILTIN PF_cs_addprogs (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *s = PR_GetStringOfs(prinst, OFS_PARM0);
	if (!s || !*s)
		G_FLOAT(OFS_RETURN) = -1;
	else
		G_FLOAT(OFS_RETURN) = PR_LoadProgs(prinst, s, 0, NULL, 0);
}

static void QCBUILTIN PF_cs_OpenPortal (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
#ifdef Q2BSPS
	if (cl.worldmodel->fromgame == fg_quake2)
		CMQ2_SetAreaPortalState(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
#endif
}

#ifndef NOMEDIA

// #487 float(string name) gecko_create( string name )
static void QCBUILTIN PF_cs_gecko_create (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *shader = PR_GetStringOfs(prinst, OFS_PARM0);
	cin_t *cin;
	cin = R_ShaderFindCinematic(shader);

	if (!cin)
		G_FLOAT(OFS_RETURN) = 0;
	else
		G_FLOAT(OFS_RETURN) = 1;
}
// #488 void(string name) gecko_destroy( string name )
static void QCBUILTIN PF_cs_gecko_destroy (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
}
// #489 void(string name) gecko_navigate( string name, string URI )
static void QCBUILTIN PF_cs_gecko_navigate (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *shader = PR_GetStringOfs(prinst, OFS_PARM0);
	char *command = PR_GetStringOfs(prinst, OFS_PARM1);
	cin_t *cin;
	cin = R_ShaderFindCinematic(shader);

	if (!cin)
		return;

	Media_Send_Command(cin, command);
}
// #490 float(string name) gecko_keyevent( string name, float key, float eventtype )
static void QCBUILTIN PF_cs_gecko_keyevent (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *shader = PR_GetStringOfs(prinst, OFS_PARM0);
	int key = G_FLOAT(OFS_PARM1);
	int eventtype = G_FLOAT(OFS_PARM2);
	cin_t *cin;
	cin = R_ShaderFindCinematic(shader);

	if (!cin)
		return;
	Media_Send_KeyEvent(cin, MP_TranslateDPtoFTECodes(key), (key>127)?0:key, eventtype);
}
// #491 void gecko_mousemove( string name, float x, float y )
static void QCBUILTIN PF_cs_gecko_mousemove (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *shader = PR_GetStringOfs(prinst, OFS_PARM0);
	float posx = G_FLOAT(OFS_PARM1);
	float posy = G_FLOAT(OFS_PARM2);
	cin_t *cin;
	cin = R_ShaderFindCinematic(shader);

	if (!cin)
		return;
	Media_Send_MouseMove(cin, posx, posy);
}
// #492 void gecko_resize( string name, float w, float h )
static void QCBUILTIN PF_cs_gecko_resize (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *shader = PR_GetStringOfs(prinst, OFS_PARM0);
	float sizex = G_FLOAT(OFS_PARM1);
	float sizey = G_FLOAT(OFS_PARM2);
	cin_t *cin;
	cin = R_ShaderFindCinematic(shader);
	if (!cin)
		return;
	Media_Send_Resize(cin, sizex, sizey);
}
// #493 vector gecko_get_texture_extent( string name )
static void QCBUILTIN PF_cs_gecko_get_texture_extent (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *shader = PR_GetStringOfs(prinst, OFS_PARM0);
	float *ret = G_VECTOR(OFS_RETURN);
	int sx, sy;
	cin_t *cin;
	cin = R_ShaderFindCinematic(shader);
	if (cin)
	{
		Media_Send_GetSize(cin, &sx, &sy);
	}
	else
	{
		sx = 0;
		sy = 0;
	}
	ret[0] = sx;
	ret[1] = sy;
	ret[2] = 0;
}
#endif

static void QCBUILTIN PF_cs_droptofloor (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t		*ent;
	vec3_t		end;
	vec3_t		start;
	trace_t		trace;

	ent = (csqcedict_t*)PROG_TO_EDICT(prinst, *csqcg.self);

	VectorCopy (ent->v->origin, end);
	end[2] -= 512;

	VectorCopy (ent->v->origin, start);
	trace = World_Move (&csqc_world, start, ent->v->mins, ent->v->maxs, end, MOVE_NORMAL, (wedict_t*)ent);

	if (trace.fraction == 1 || trace.allsolid)
		G_FLOAT(OFS_RETURN) = 0;
	else
	{
		VectorCopy (trace.endpos, ent->v->origin);
		World_LinkEdict(&csqc_world, (wedict_t*)ent, false);
		ent->v->flags = (int)ent->v->flags | FL_ONGROUND;
		ent->v->groundentity = EDICT_TO_PROG(prinst, trace.ent);
		G_FLOAT(OFS_RETURN) = 1;
	}
}

static void QCBUILTIN PF_cs_copyentity (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *in, *out;

	in = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	out = (csqcedict_t*)G_EDICT(prinst, OFS_PARM1);

	memcpy(out->v, in->v, csqcentsize);

	World_LinkEdict (&csqc_world, (wedict_t*)out, false);
}

static void QCBUILTIN PF_cl_playingdemo (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = !!cls.demoplayback;
}

static void QCBUILTIN PF_cl_runningserver (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
#ifdef CLIENTONLY
	G_FLOAT(OFS_RETURN) = false;
#else
	G_FLOAT(OFS_RETURN) = !!sv.active;
#endif
}

static void QCBUILTIN PF_cl_getlight (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	vec3_t ambient, diffuse, dir;
	cl.worldmodel->funcs.LightPointValues(cl.worldmodel, G_VECTOR(OFS_PARM0), ambient, diffuse, dir);
	VectorMA(ambient, 0.5, diffuse, G_VECTOR(OFS_RETURN));
}

/*
static void QCBUILTIN PF_Stub (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Con_Printf("Obsolete csqc builtin (%i) executed\n", prinst->lastcalledbuiltinnumber);
}
*/

static void QCBUILTIN PF_rotatevectorsbytag (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	int tagnum = G_FLOAT(OFS_PARM1);

	float *srcorg = ent->v->origin;
	int modelindex = ent->v->modelindex;

	float *retorg = G_VECTOR(OFS_RETURN);

	model_t *mod = CSQC_GetModelForIndex(modelindex);
	float transforms[12];
	float src[12];
	float dest[12];
	int i;
	framestate_t fstate;

	cs_getframestate(ent, ent->xv->renderflags, &fstate);

	if (Mod_GetTag(mod, tagnum, &fstate, transforms))
	{
		VectorCopy(csqcg.forward, src+0);
		src[3] = 0;
		VectorNegate(csqcg.right, src+4);
		src[7] = 0;
		VectorCopy(csqcg.up, src+8);
		src[11] = 0;

		if (ent->xv->scale)
		{
			for (i = 0; i < 12; i+=4)
			{
				transforms[i+0] *= ent->xv->scale;
				transforms[i+1] *= ent->xv->scale;
				transforms[i+2] *= ent->xv->scale;
				transforms[i+3] *= ent->xv->scale;
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

static void EdictToTransform(csqcedict_t *ed, float *trans)
{
	AngleVectors(ed->v->angles, trans+0, trans+4, trans+8);
	VectorInverse(trans+4);

	trans[3] = ed->v->origin[0];
	trans[7] = ed->v->origin[1];
	trans[11] = ed->v->origin[2];
}

static void QCBUILTIN PF_cs_gettaginfo (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	int tagnum = G_FLOAT(OFS_PARM1);

	float *origin = G_VECTOR(OFS_RETURN);

	int modelindex = ent->v->modelindex;

	model_t *mod = CSQC_GetModelForIndex(modelindex);

	float transent[12];
	float transforms[12];
	float result[12];

	framestate_t fstate;

	cs_getframestate(ent, ent->xv->renderflags, &fstate);

#pragma message("PF_cs_gettaginfo: This function doesn't honour attachments (but setattachment isn't implemented yet anyway)")
	if (!Mod_GetTag(mod, tagnum, &fstate, transforms))
	{
		memset(transforms, 0, sizeof(transforms));
	}

	EdictToTransform(ent, transent);
	R_ConcatTransforms((void*)transent, (void*)transforms, (void*)result);

	origin[0] = result[3];
	origin[1] = result[7];
	origin[2] = result[11];
	VectorCopy((result+0), csqcg.forward);
	VectorCopy((result+4), csqcg.right);
	VectorCopy((result+8), csqcg.up);

}
static void QCBUILTIN PF_cs_gettagindex (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	char *tagname = PR_GetStringOfs(prinst, OFS_PARM1);

	model_t *mod = CSQC_GetModelForIndex(ent->v->modelindex);
	G_FLOAT(OFS_RETURN) = Mod_TagNumForName(mod, tagname);
}
static void QCBUILTIN PF_rotatevectorsbyangles (progfuncs_t *prinst, struct globalvars_s *pr_globals)
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
static void QCBUILTIN PF_rotatevectorsbymatrix (progfuncs_t *prinst, struct globalvars_s *pr_globals)
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
static void QCBUILTIN PF_frameforname (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int modelindex = G_FLOAT(OFS_PARM0);
	char *str = PF_VarString(prinst, 1, pr_globals);
	model_t *mod = CSQC_GetModelForIndex(modelindex);

	if (mod && Mod_FrameForName)
		G_FLOAT(OFS_RETURN) = Mod_FrameForName(mod, str);
	else
		G_FLOAT(OFS_RETURN) = -1;
}
static void QCBUILTIN PF_frameduration (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int modelindex = G_FLOAT(OFS_PARM0);
	int frameno = G_FLOAT(OFS_PARM1);
	model_t *mod = CSQC_GetModelForIndex(modelindex);

	if (mod && Mod_GetFrameDuration)
		G_FLOAT(OFS_RETURN) = Mod_GetFrameDuration(mod, frameno);
	else
		G_FLOAT(OFS_RETURN) = 0;
}
static void QCBUILTIN PF_skinforname (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int modelindex = G_FLOAT(OFS_PARM0);
	char *str = PF_VarString(prinst, 1, pr_globals);
	model_t *mod = CSQC_GetModelForIndex(modelindex);


	if (mod && Mod_SkinForName)
		G_FLOAT(OFS_RETURN) = Mod_SkinForName(mod, str);
	else
		G_FLOAT(OFS_RETURN) = -1;
}
static void QCBUILTIN PF_shaderforname (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *str = PF_VarString(prinst, 0, pr_globals);

	shader_t *shad;
	shad = R_RegisterSkin(str);
	if (shad)
		G_FLOAT(OFS_RETURN) = shad-r_shaders + 1;
	else
		G_FLOAT(OFS_RETURN) = 0;
}

void skel_reset(void)
{
	while (numskelobjectsused > 0)
	{
		numskelobjectsused--;
		skelobjects[numskelobjectsused].numbones = 0;
		skelobjects[numskelobjectsused].inuse = false;
	}
}

void skel_dodelete(void)
{
	int skelidx;
	for (skelidx = 0; skelidx < numskelobjectsused; skelidx++)
	{
		if (skelobjects[skelidx].inuse == 2)
			skelobjects[skelidx].inuse = 0;
	}
}

skelobject_t *skel_get(progfuncs_t *prinst, int skelidx, int bonecount)
{
	if (skelidx == 0)
	{
		//allocation
		if (!bonecount)
			return NULL;

		for (skelidx = 0; skelidx < numskelobjectsused; skelidx++)
		{
			if (!skelobjects[skelidx].inuse && skelobjects[skelidx].numbones == bonecount)
				return &skelobjects[skelidx];
		}

		for (skelidx = 0; skelidx <= numskelobjectsused; skelidx++)
		{
			if (!skelobjects[skelidx].inuse && !skelobjects[skelidx].numbones)
			{
				skelobjects[skelidx].numbones = bonecount;
				skelobjects[skelidx].bonematrix = (float*)PR_AddString(prinst, "", sizeof(float)*12*bonecount);
				if (skelidx <= numskelobjectsused)
				{
					numskelobjectsused = skelidx + 1;
					skelobjects[skelidx].model = NULL;
					skelobjects[skelidx].inuse = 1;
				}
				return &skelobjects[skelidx];
			}
		}

		return NULL;
	}
	else
	{
		skelidx--;
		if ((unsigned int)skelidx >= numskelobjectsused)
			return NULL;
		if (skelobjects[skelidx].inuse != 1)
			return NULL;
		if (bonecount && skelobjects[skelidx].numbones != bonecount)
			return NULL;
		return &skelobjects[skelidx];
	}
}

//float(float modelindex) skel_create (FTE_CSQC_SKELETONOBJECTS)
static void QCBUILTIN PF_skel_create (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int numbones;
	skelobject_t *skelobj;
	qboolean isabs;
	model_t *model;
	int midx;

	midx = G_FLOAT(OFS_PARM0);

	//default to failure
	G_FLOAT(OFS_RETURN) = 0;

	model = CSQC_GetModelForIndex(midx);
	if (!model)
		return; //no model set, can't get a skeleton

	isabs = false;
	numbones = Mod_GetNumBones(model, isabs);
	if (!numbones)
	{
//		isabs = true;
//		numbones = Mod_GetNumBones(model, isabs);
//		if (!numbones)
			return;	//this isn't a skeletal model.
	}

	skelobj = skel_get(prinst, 0, numbones);
	if (!skelobj)
		return;	//couldn't get one, ran out of memory or something?

	skelobj->model = model;
	skelobj->absolute = isabs;

	G_FLOAT(OFS_RETURN) = (skelobj - skelobjects) + 1;
}

//float(float skel, entity ent, float modelindex, float retainfrac, float firstbone, float lastbone) skel_build (FTE_CSQC_SKELETONOBJECTS)
static void QCBUILTIN PF_skel_build(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	#define MAX_BONES 256
	int skelidx = G_FLOAT(OFS_PARM0);
	csqcedict_t *ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM1);
	int midx = G_FLOAT(OFS_PARM2);
	float retainfrac = G_FLOAT(OFS_PARM3);
	int firstbone = G_FLOAT(OFS_PARM4)-1;
	int lastbone = G_FLOAT(OFS_PARM5)-1;
	float addition = 1?G_FLOAT(OFS_PARM6):1-retainfrac;

	int i, j;
	int numbones;
	framestate_t fstate;
	skelobject_t *skelobj;
	model_t *model;

	//default to failure
	G_FLOAT(OFS_RETURN) = 0;

	model = CSQC_GetModelForIndex(midx);
	if (!model)
		return; //invalid model, can't get a skeleton

	cs_getframestate(ent, ent->xv->renderflags, &fstate);

	//heh... don't copy.
	fstate.bonecount = 0;
	fstate.bonestate = NULL;

	numbones = Mod_GetNumBones(model, false);
	if (!numbones)
	{
		return;	//this isn't a skeletal model.
	}

	skelobj = skel_get(prinst, skelidx, 0);
	if (!skelobj)
		return;	//couldn't get one, ran out of memory or something?

	if (lastbone < 0)
		lastbone = numbones;
	if (lastbone > numbones)
		lastbone = numbones;
	if (firstbone < 0)
		firstbone = 0;

	if (retainfrac == 0 && addition == 1)
	{
		/*replace everything*/
		Mod_GetBoneRelations(model, firstbone, lastbone, &fstate, skelobj->bonematrix);
	}
	else
	{
		if (retainfrac != 1)
		{
			//rescale the existing bones
			for (i = firstbone; i < lastbone; i++)
			{
				for (j = 0; j < 12; j++)
					skelobj->bonematrix[i*12+j] *= retainfrac;
			}
		}
		if (addition == 1)
		{
			//just add
			float relationsbuf[MAX_BONES*12];
			Mod_GetBoneRelations(model, firstbone, lastbone, &fstate, relationsbuf);
			for (i = firstbone; i < lastbone; i++)
			{
				for (j = 0; j < 12; j++)
					skelobj->bonematrix[i*12+j] += relationsbuf[i*12+j];
			}
		}
		else if (addition)
		{
			//add+scale
			float relationsbuf[MAX_BONES*12];
			Mod_GetBoneRelations(model, firstbone, lastbone, &fstate, relationsbuf);
			for (i = firstbone; i < lastbone; i++)
			{
				for (j = 0; j < 12; j++)
					skelobj->bonematrix[i*12+j] += addition*relationsbuf[i*12+j];
			}
		}
	}

	G_FLOAT(OFS_RETURN) = (skelobj - skelobjects) + 1;
}

//float(float skel) skel_get_numbones (FTE_CSQC_SKELETONOBJECTS)
static void QCBUILTIN PF_skel_get_numbones (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int skelidx = G_FLOAT(OFS_PARM0);
	skelobject_t *skelobj;

	skelobj = skel_get(prinst, skelidx, 0);

	if (!skelobj)
		G_FLOAT(OFS_RETURN) = 0;
	else
		G_FLOAT(OFS_RETURN) = skelobj->numbones;
}

//string(float skel, float bonenum) skel_get_bonename (FTE_CSQC_SKELETONOBJECTS) (returns tempstring)
static void QCBUILTIN PF_skel_get_bonename (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int skelidx = G_FLOAT(OFS_PARM0);
	int boneidx = G_FLOAT(OFS_PARM1);
	skelobject_t *skelobj;

	skelobj = skel_get(prinst, skelidx, 0);

	if (!skelobj)
		G_INT(OFS_RETURN) = 0;
	else
	{
		RETURN_TSTRING(Mod_GetBoneName(skelobj->model, boneidx));
	}
}

//float(float skel, float bonenum) skel_get_boneparent (FTE_CSQC_SKELETONOBJECTS)
static void QCBUILTIN PF_skel_get_boneparent (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int skelidx = G_FLOAT(OFS_PARM0);
	int boneidx = G_FLOAT(OFS_PARM1);
	skelobject_t *skelobj;

	skelobj = skel_get(prinst, skelidx, 0);

	if (!skelobj)
		G_FLOAT(OFS_RETURN) = 0;
	else
		G_FLOAT(OFS_RETURN) = Mod_GetBoneParent(skelobj->model, boneidx);
}

//float(float skel, string tagname) gettagindex (DP_MD3_TAGSINFO)
static void QCBUILTIN PF_skel_find_bone (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int skelidx = G_FLOAT(OFS_PARM0);
	char *bname = PR_GetStringOfs(prinst, OFS_PARM1);
	skelobject_t *skelobj;

	skelobj = skel_get(prinst, skelidx, 0);
	if (!skelobj)
		G_FLOAT(OFS_RETURN) = 0;
	else
		G_FLOAT(OFS_RETURN) = Mod_TagNumForName(skelobj->model, bname);
}

static void bonemat_fromqcvectors(float *out, const float vx[3], const float vy[3], const float vz[3], const float t[3])
{
	out[0] = vx[0];
	out[1] = -vy[0];
	out[2] = vz[0];
	out[3] = t[0];
	out[4] = vx[1];
	out[5] = -vy[1];
	out[6] = vz[1];
	out[7] = t[1];
	out[8] = vx[2];
	out[9] = -vy[2];
	out[10] = vz[2];
	out[11] = t[2];
}
void bonemat_toqcvectors(const float *in, float vx[3], float vy[3], float vz[3], float t[3])
{
	vx[0] = in[0];
	vx[1] = in[4];
	vx[2] = in[8];
	vy[0] = -in[1];
	vy[1] = -in[5];
	vy[2] = -in[9];
	vz[0] = in[2];
	vz[1] = in[6];
	vz[2] = in[10];
	t [0] = in[3];
	t [1] = in[7];
	t [2] = in[11];
}

void bonematident_toqcvectors(float vx[3], float vy[3], float vz[3], float t[3])
{
	vx[0] = 1;
	vx[1] = 0;
	vx[2] = 0;
	vy[0] = -0;
	vy[1] = -1;
	vy[2] = -0;
	vz[0] = 0;
	vz[1] = 0;
	vz[2] = 1;
	t [0] = 0;
	t [1] = 0;
	t [2] = 0;
}

//vector(float skel, float bonenum) skel_get_bonerel (FTE_CSQC_SKELETONOBJECTS) (sets v_forward etc)
static void QCBUILTIN PF_skel_get_bonerel (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int skelidx = G_FLOAT(OFS_PARM0);
	int boneidx = G_FLOAT(OFS_PARM1)-1;
	skelobject_t *skelobj = skel_get(prinst, skelidx, 0);
	if (!skelobj || skelobj->absolute || (unsigned int)boneidx >= skelobj->numbones)
		bonematident_toqcvectors(csqcg.forward, csqcg.right, csqcg.up, G_VECTOR(OFS_RETURN));
	else
		bonemat_toqcvectors(skelobj->bonematrix+12*boneidx, csqcg.forward, csqcg.right, csqcg.up, G_VECTOR(OFS_RETURN));
}

//vector(float skel, float bonenum) skel_get_boneabs (FTE_CSQC_SKELETONOBJECTS) (sets v_forward etc)
static void QCBUILTIN PF_skel_get_boneabs (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int skelidx = G_FLOAT(OFS_PARM0);
	int boneidx = G_FLOAT(OFS_PARM1)-1;
	float workingm[12], tempmatrix[3][4];
	int i;
	skelobject_t *skelobj = skel_get(prinst, skelidx, 0);

	if (!skelobj || (unsigned int)boneidx >= skelobj->numbones)
		bonematident_toqcvectors(csqcg.forward, csqcg.right, csqcg.up, G_VECTOR(OFS_RETURN));
	else if (skelobj->absolute)
	{
		//can just copy it out
		bonemat_toqcvectors(skelobj->bonematrix + boneidx*12, csqcg.forward, csqcg.right, csqcg.up, G_VECTOR(OFS_RETURN));
	}
	else
	{
		//we need to work out the abs position

		//testme

		//set up an identity matrix
		for (i = 0;i < 12;i++)
			workingm[i] = 0;
		workingm[0] = 1;
		workingm[5] = 1;
		workingm[10] = 1;

		while(boneidx >= 0)
		{
			//copy out the previous working matrix, so we don't stomp on it
			memcpy(tempmatrix, workingm, sizeof(tempmatrix));
			R_ConcatTransforms((void*)(skelobj->bonematrix + boneidx*12), (void*)tempmatrix, (void*)workingm);

			boneidx = Mod_GetBoneParent(skelobj->model, boneidx+1)-1;
		}
		bonemat_toqcvectors(workingm, csqcg.forward, csqcg.right, csqcg.up, G_VECTOR(OFS_RETURN));
	}
}

//void(float skel, float bonenum, vector org) skel_set_bone (FTE_CSQC_SKELETONOBJECTS) (reads v_forward etc)
static void QCBUILTIN PF_skel_set_bone (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int skelidx = G_FLOAT(OFS_PARM0);
	unsigned int boneidx = G_FLOAT(OFS_PARM1)-1;
	float *matrix[3];
	skelobject_t *skelobj;
	float *bone;

	if (*prinst->callargc > 5)
	{
		matrix[0] = G_VECTOR(OFS_PARM3);
		matrix[1] = G_VECTOR(OFS_PARM4);
		matrix[2] = G_VECTOR(OFS_PARM5);
	}
	else
	{
		matrix[0] = csqcg.forward;
		matrix[1] = csqcg.right;
		matrix[2] = csqcg.up;
	}

	skelobj = skel_get(prinst, skelidx, 0);
	if (!skelobj || boneidx >= skelobj->numbones)
		return;

	bone = skelobj->bonematrix+12*boneidx;
	bonemat_fromqcvectors(skelobj->bonematrix+12*boneidx, matrix[0], matrix[1], matrix[2], G_VECTOR(OFS_PARM2));
}

//void(float skel, float bonenum, vector org) skel_mul_bone (FTE_CSQC_SKELETONOBJECTS) (reads v_forward etc)
static void QCBUILTIN PF_skel_mul_bone (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int skelidx = G_FLOAT(OFS_PARM0);
	int boneidx = G_FLOAT(OFS_PARM1)-1;
	float temp[3][4];
	float mult[3][4];
	skelobject_t *skelobj;
	if (*prinst->callargc > 5)
		bonemat_fromqcvectors((float*)mult, G_VECTOR(OFS_PARM3), G_VECTOR(OFS_PARM4), G_VECTOR(OFS_PARM5), G_VECTOR(OFS_PARM2));
	else
		bonemat_fromqcvectors((float*)mult, csqcg.forward, csqcg.right, csqcg.up, G_VECTOR(OFS_PARM2));

	skelobj = skel_get(prinst, skelidx, 0);
	if (!skelobj || boneidx >= skelobj->numbones)
		return;
//testme
	Vector4Copy(skelobj->bonematrix+12*boneidx+0, temp[0]);
	Vector4Copy(skelobj->bonematrix+12*boneidx+4, temp[1]);
	Vector4Copy(skelobj->bonematrix+12*boneidx+8, temp[2]);
	R_ConcatTransforms(mult, temp, (float(*)[4])(skelobj->bonematrix+12*boneidx));
}

//void(float skel, float startbone, float endbone, vector org) skel_mul_bone (FTE_CSQC_SKELETONOBJECTS) (reads v_forward etc)
static void QCBUILTIN PF_skel_mul_bones (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int skelidx = G_FLOAT(OFS_PARM0);
	unsigned int startbone = G_FLOAT(OFS_PARM1)-1;
	unsigned int endbone = G_FLOAT(OFS_PARM2)-1;
	float temp[3][4];
	float mult[3][4];
	skelobject_t *skelobj;
	if (*prinst->callargc > 6)
		bonemat_fromqcvectors((float*)mult, G_VECTOR(OFS_PARM4), G_VECTOR(OFS_PARM5), G_VECTOR(OFS_PARM6), G_VECTOR(OFS_PARM3));
	else
		bonemat_fromqcvectors((float*)mult, csqcg.forward, csqcg.right, csqcg.up, G_VECTOR(OFS_PARM3));

	skelobj = skel_get(prinst, skelidx, 0);
	if (!skelobj)
		return;

	if (startbone == -1)
		startbone = 0;
//testme
	while(startbone < endbone && startbone < skelobj->numbones)
	{
		Vector4Copy(skelobj->bonematrix+12*startbone+0, temp[0]);
		Vector4Copy(skelobj->bonematrix+12*startbone+4, temp[1]);
		Vector4Copy(skelobj->bonematrix+12*startbone+8, temp[2]);
		R_ConcatTransforms(mult, temp, (float(*)[4])(skelobj->bonematrix+12*startbone));
	}
}

//void(float skeldst, float skelsrc, float startbone, float entbone) skel_copybones (FTE_CSQC_SKELETONOBJECTS)
static void QCBUILTIN PF_skel_copybones (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int skeldst = G_FLOAT(OFS_PARM0);
	int skelsrc = G_FLOAT(OFS_PARM1);
	int startbone = G_FLOAT(OFS_PARM2)-1;
	int endbone = G_FLOAT(OFS_PARM3)-1;

	skelobject_t *skelobjdst;
	skelobject_t *skelobjsrc;

	skelobjdst = skel_get(prinst, skeldst, 0);
	skelobjsrc = skel_get(prinst, skelsrc, 0);
	if (!skelobjdst || !skelobjsrc)
		return;
	if (skelobjsrc->absolute != skelobjdst->absolute)
		return;

	if (startbone == -1)
		startbone = 0;
//testme
	while(startbone < endbone && startbone < skelobjdst->numbones && startbone < skelobjsrc->numbones)
	{
		Vector4Copy(skelobjsrc->bonematrix+12*startbone+0, skelobjdst->bonematrix+12*startbone+0);
		Vector4Copy(skelobjsrc->bonematrix+12*startbone+4, skelobjdst->bonematrix+12*startbone+4);
		Vector4Copy(skelobjsrc->bonematrix+12*startbone+8, skelobjdst->bonematrix+12*startbone+8);
	}
}

//void(float skel) skel_delete (FTE_CSQC_SKELETONOBJECTS)
static void QCBUILTIN PF_skel_delete (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int skelidx = G_FLOAT(OFS_PARM0);
	skelobject_t *skelobj;

	skelobj = skel_get(prinst, skelidx, 0);
	if (skelobj)
		skelobj->inuse = 2;	//2 means don't reuse yet.
}





static void QCBUILTIN PF_cs_checkbottom (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t	*ent;

	ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);

	G_FLOAT(OFS_RETURN) = World_CheckBottom (&csqc_world, (wedict_t*)ent);
}

static void QCBUILTIN PF_cs_break (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Con_Printf ("break statement\n");
#ifdef TEXTEDITOR
	(*prinst->pr_trace)++;
#endif
}

static void QCBUILTIN PF_cs_walkmove (progfuncs_t *prinst, struct globalvars_s *pr_globals)
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

	G_FLOAT(OFS_RETURN) = World_movestep(&csqc_world, (wedict_t*)ent, move, true, false, NULL, pr_globals);

// restore program state
	*csqcg.self = oldself;
}

static void QCBUILTIN PF_cs_movetogoal (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	wedict_t	*ent;
	float dist;
	ent = (wedict_t*)PROG_TO_EDICT(prinst, *csqcg.self);
	dist = G_FLOAT(OFS_PARM0);
	World_MoveToGoal (&csqc_world, ent, dist);
}

static void CS_ConsoleCommand_f(void)
{	//FIXME: unregister them.
	char cmd[2048];
	Q_snprintfz(cmd, sizeof(cmd), "%s %s", Cmd_Argv(0), Cmd_Args());
	CSQC_ConsoleCommand(cmd);
}
static void QCBUILTIN PF_cs_registercommand (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *str = PF_VarString(prinst, 0, pr_globals);
	if (!strcmp(str, "+showscores") || !strcmp(str, "-showscores") ||
		!strcmp(str, "+showteamscores") || !strcmp(str, "-showteamscores"))
		return;
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
static void QCBUILTIN PF_cs_setlistener (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *origin = G_VECTOR(OFS_PARM0);
	float *forward = G_VECTOR(OFS_PARM1);
	float *right = G_VECTOR(OFS_PARM2);
	float *up = G_VECTOR(OFS_PARM3);
	csqc_usinglistener = true;
	S_UpdateListener(origin, forward, right, up);
	S_Update();
}

#define RSES_NOLERP 1
#define RSES_NOROTATE 2
#define RSES_NOTRAILS 4
#define RSES_NOLIGHTS 8

static void CSQC_LerpStateToCSQC(lerpents_t *le, csqcedict_t *ent, qboolean nolerp)
{
	ent->v->frame = le->newframe;
	ent->xv->frame1time = max(0, cl.servertime - le->newframestarttime);
	ent->xv->frame2 = le->oldframe;
	ent->xv->frame2time = max(0, cl.servertime - le->newframestarttime);

	ent->xv->lerpfrac = bound(0, cl.servertime - le->newframestarttime, 0.1);


/*	if (nolerp)
	{
		ent->v->origin[0] = le->neworigin[0];
		ent->v->origin[1] = le->neworigin[1];
		ent->v->origin[2] = le->neworigin[2];
		ent->v->angles[0] = le->newangle[0];
		ent->v->angles[1] = le->newangle[1];
		ent->v->angles[2] = le->newangle[2];
	}
	else*/
	{
		ent->v->origin[0] = le->origin[0];
		ent->v->origin[1] = le->origin[1];
		ent->v->origin[2] = le->origin[2];
		ent->v->angles[0] = le->angles[0];
		ent->v->angles[1] = le->angles[1];
		ent->v->angles[2] = le->angles[2];
	}
}

void CSQC_EntStateToCSQC(unsigned int flags, float lerptime, entity_state_t *src, csqcedict_t *ent)
{
	model_t *model;
	lerpents_t		*le;

	le = &cl.lerpents[src->number];

	CSQC_LerpStateToCSQC(le, ent, flags & RSES_NOLERP);


	model = cl.model_precache[src->modelindex];
	if (!(flags & RSES_NOTRAILS))
	{
		//use entnum as a test to see if its new (if the old origin isn't usable)
		if (ent->xv->entnum && model->particletrail >= 0)
		{
			if (pe->ParticleTrail (ent->v->origin, src->origin, model->particletrail, &(le->trailstate)))
				pe->ParticleTrailIndex(ent->v->origin, src->origin, model->traildefaultindex, 0, &(le->trailstate));
		}
	}

	ent->xv->entnum = src->number;
	ent->v->modelindex = src->modelindex;
//	ent->v->bitmask = src->bitmask;
	ent->v->flags = src->flags;
//	ent->v->effects = src->effects;

//we ignore the q2 state fields

	ent->v->colormap = src->colormap;
	ent->v->skin = src->skinnum;
//	ent->v->glowsize = src->glowsize;
//	ent->v->glowcolor = src->glowcolour;
	ent->xv->scale = src->scale/16.0f;
	ent->xv->fatness = src->fatness/16.0f;
//	ent->v->hexen2flags = src->hexen2flags;
//	ent->v->abslight = src->abslight;
//	ent->v->dpflags = src->dpflags;
//	ent->v->colormod[0] = (src->colormod[0]/255.0f)*8;
//	ent->v->colormod[1] = (src->colormod[1]/255.0f)*8;
//	ent->v->colormod[2] = (src->colormod[2]/255.0f)*8;
	ent->xv->alpha = src->trans/255.0f;
//	ent->v->lightstyle = src->lightstyle;
//	ent->v->lightpflags = src->lightpflags;
//	ent->v->solid = src->solid;
//	ent->v->light[0] = src->light[0];
//	ent->v->light[1] = src->light[1];
//	ent->v->light[2] = src->light[2];
//	ent->v->light[3] = src->light[3];
//	ent->v->tagentity = src->tagentity;
//	ent->v->tagindex = src->tagindex;

	if (model)
	{
		if (!(flags & RSES_NOROTATE) && (model->flags & EF_ROTATE))
		{
			ent->v->angles[0] = 0;
			ent->v->angles[1] = 100*lerptime;
			ent->v->angles[2] = 0;
		}
	}
}
void CSQC_PlayerStateToCSQC(int pnum, player_state_t *srcp, csqcedict_t *ent)
{
	ent->xv->entnum = pnum+1;

	if (cl.spectator && !Cam_DrawPlayer(0, pnum))
	{
		ent->v->modelindex = 0;
	}
	else
		ent->v->modelindex = srcp->modelindex;
	ent->v->skin = srcp->skinnum;

	CSQC_LerpStateToCSQC(&cl.lerpplayers[pnum], ent, true);


	VectorCopy(srcp->origin, ent->v->origin);
	VectorCopy(srcp->viewangles, ent->v->angles);

	VectorCopy(srcp->velocity, ent->v->velocity);
	ent->v->angles[0] *= -0.333;
	ent->v->colormap = pnum+1;
	ent->xv->scale = srcp->scale/16.0f;
	//ent->v->fatness = srcp->fatness;
	ent->xv->alpha = srcp->alpha/255.0f;

//	ent->v->colormod[0] = (srcp->colormod[0]/255.0f)*8;
//	ent->v->colormod[1] = (srcp->colormod[1]/255.0f)*8;
//	ent->v->colormod[2] = (srcp->colormod[2]/255.0f)*8;
//	ent->v->effects = srcp->effects;
}

unsigned int deltaflags[MAX_MODELS];
func_t deltafunction[MAX_MODELS];

typedef struct
{
	unsigned int readpos;	//pos
	unsigned int numents;	//present
	unsigned int maxents;	//buffer size
	struct
	{
		unsigned short n;	//don't rely on the ent->v->entnum
		csqcedict_t *e;	//the csqc ent
	} *e;
} csqcdelta_pack_t;
static csqcdelta_pack_t csqcdelta_pack_new;
static csqcdelta_pack_t csqcdelta_pack_old;
float csqcdelta_time;

static csqcedict_t *csqcdelta_playerents[MAX_CLIENTS];


qboolean CSQC_DeltaPlayer(int playernum, player_state_t *state)
{
	func_t func;

	if (!state || !state->modelindex)
	{
		if (csqcdelta_playerents[playernum])
		{
			*csqcg.self = EDICT_TO_PROG(csqcprogs, (void*)csqcdelta_playerents[playernum]);
			PR_ExecuteProgram(csqcprogs, csqcg.ent_remove);
			csqcdelta_playerents[playernum] = NULL;
		}
		return false;
	}

	func = deltafunction[state->modelindex];
	if (func)
	{
		void *pr_globals;
		csqcedict_t *ent;

		ent = csqcdelta_playerents[playernum];
		if (!ent)
			ent = (csqcedict_t *)ED_Alloc(csqcprogs);

		CSQC_PlayerStateToCSQC(playernum, state, ent);
		ent->xv->drawmask = MASK_DELTA;

		*csqcg.self = EDICT_TO_PROG(csqcprogs, (void*)ent);
		pr_globals = PR_globals(csqcprogs, PR_CURRENT);
		G_FLOAT(OFS_PARM0) = !csqcdelta_playerents[playernum];
		PR_ExecuteProgram(csqcprogs, func);

		csqcdelta_playerents[playernum] = ent;

		return true;
	}
	else if (csqcdelta_playerents[playernum])
	{
		*csqcg.self = EDICT_TO_PROG(csqcprogs, (void*)csqcdelta_playerents[playernum]);
		PR_ExecuteProgram(csqcprogs, csqcg.ent_remove);
		csqcdelta_playerents[playernum] = NULL;
	}
	return false;
}

void CSQC_DeltaStart(float time)
{
	csqcdelta_pack_t tmp;
	csqcdelta_time = time;

	tmp = csqcdelta_pack_new;
	csqcdelta_pack_new = csqcdelta_pack_old;
	csqcdelta_pack_old = tmp;

	csqcdelta_pack_new.numents = 0;

	csqcdelta_pack_new.readpos = 0;
	csqcdelta_pack_old.readpos = 0;
}
qboolean CSQC_DeltaUpdate(entity_state_t *src)
{
	//FTE ensures that this function is called with increasing ent numbers each time
	func_t func;
	func = deltafunction[src->modelindex];
	if (func)
	{
		void *pr_globals;
		csqcedict_t *ent, *oldent;




		if (csqcdelta_pack_old.readpos == csqcdelta_pack_old.numents)
		{	//reached the end of the old frame's ents
			oldent = NULL;
		}
		else
		{
			while (csqcdelta_pack_old.readpos < csqcdelta_pack_old.numents && csqcdelta_pack_old.e[csqcdelta_pack_old.readpos].n < src->number)
			{
				//this entity is stale, remove it.
				oldent = csqcdelta_pack_old.e[csqcdelta_pack_old.readpos].e;
				*csqcg.self = EDICT_TO_PROG(csqcprogs, (void*)oldent);
				PR_ExecuteProgram(csqcprogs, csqcg.ent_remove);
				csqcdelta_pack_old.readpos++;
			}

			if (src->number < csqcdelta_pack_old.e[csqcdelta_pack_old.readpos].n)
				oldent = NULL;
			else
			{
				oldent = csqcdelta_pack_old.e[csqcdelta_pack_old.readpos].e;
				csqcdelta_pack_old.readpos++;
			}
		}

		if (src->number < maxcsqcentities && csqcent[src->number])
		{
			//in the csqc list (don't permit in the delta list too)
			if (oldent)
			{
				*csqcg.self = EDICT_TO_PROG(csqcprogs, (void*)oldent);
				PR_ExecuteProgram(csqcprogs, csqcg.ent_remove);
			}
			return false;
		}




		if (oldent)
			ent = oldent;
		else
			ent = (csqcedict_t *)ED_Alloc(csqcprogs);

		CSQC_EntStateToCSQC(deltaflags[src->modelindex], csqcdelta_time, src, ent);
		ent->xv->drawmask = MASK_DELTA;


		*csqcg.self = EDICT_TO_PROG(csqcprogs, (void*)ent);
		pr_globals = PR_globals(csqcprogs, PR_CURRENT);
		G_FLOAT(OFS_PARM0) = !oldent;
		PR_ExecuteProgram(csqcprogs, func);


		if (csqcdelta_pack_new.maxents <= csqcdelta_pack_new.numents)
		{
			csqcdelta_pack_new.maxents = csqcdelta_pack_new.numents + 64;
			csqcdelta_pack_new.e = BZ_Realloc(csqcdelta_pack_new.e, sizeof(*csqcdelta_pack_new.e)*csqcdelta_pack_new.maxents);
		}
		csqcdelta_pack_new.e[csqcdelta_pack_new.numents].e = ent;
		csqcdelta_pack_new.e[csqcdelta_pack_new.numents].n = src->number;
		csqcdelta_pack_new.numents++;

		return G_FLOAT(OFS_RETURN);
	}
	return false;
}

void CSQC_DeltaEnd(void)
{
	//remove any unreferenced ents stuck on the end
	while (csqcdelta_pack_old.readpos < csqcdelta_pack_old.numents)
	{
		*csqcg.self = EDICT_TO_PROG(csqcprogs, (void*)csqcdelta_pack_old.e[csqcdelta_pack_old.readpos].e);
		PR_ExecuteProgram(csqcprogs, csqcg.ent_remove);
		csqcdelta_pack_old.readpos++;
	}
}

static void QCBUILTIN PF_DeltaListen(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i;
	char *mname = PR_GetStringOfs(prinst, OFS_PARM0);
	func_t func = G_INT(OFS_PARM1);
	unsigned int flags = G_FLOAT(OFS_PARM2);

	if (PR_GetFuncArgCount(prinst, func) < 0)
	{
		Con_Printf("PF_DeltaListen: Bad function index\n");
		return;
	}

	if (!strcmp(mname, "*"))
	{
		//yes, even things that are not allocated yet
		for (i = 0; i < MAX_MODELS; i++)
		{
			deltafunction[i] = func;
			deltaflags[i] = flags;
		}
	}
	else
	{
		for (i = 1; i < MAX_MODELS; i++)
		{
			if (!*cl.model_name[i])
				break;
			if (!strcmp(cl.model_name[i], mname))
			{
				deltafunction[i] = func;
				deltaflags[i] = flags;
				break;
			}
		}
	}
}



#if 1
static void QCBUILTIN PF_ReadServerEntityState(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
}
#else
packet_entities_t *CL_ProcessPacketEntities(float *servertime, qboolean nolerp);
static void QCBUILTIN PF_ReadServerEntityState(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	//read the arguments the csqc gave us
	unsigned int flags = G_FLOAT(OFS_PARM0);
	float servertime = G_FLOAT(OFS_PARM1);

	//locals
	packet_entities_t *pack;
	csqcedict_t *ent;
	entity_state_t *src;
	unsigned int i;
	lerpents_t		*le;
	csqcedict_t *oldent;
	oldcsqcpack_t *oldlist, *newlist;
	int oldidx = 0, newidx = 0;
	model_t *model;
	player_state_t *srcp;

	//setup
	servertime += cl.servertime;
	pack = CL_ProcessPacketEntities(&servertime, (flags & RSES_NOLERP));
	if (!pack)
		return;	//we're lagging. can't do anything, just don't update

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		srcp = &cl.frames[cl.validsequence&UPDATE_MASK].playerstate[i];
		ent = deltaedplayerents[i];
		if (srcp->messagenum == cl.validsequence && (i+1 >= maxcsqcentities || !csqcent[i+1]))
		{
			if (!ent)
			{
				ent = (csqcedict_t *)ED_Alloc(prinst);
				deltaedplayerents[i] = ent;
				G_FLOAT(OFS_PARM0) = true;
			}
			else
			{
				G_FLOAT(OFS_PARM0) = false;
			}

			CSQC_PlayerStateToCSQC(i, srcp, ent);

			if (csqcg.delta_update)
			{
				*csqcg.self = EDICT_TO_PROG(prinst, (void*)ent);
				PR_ExecuteProgram(prinst, csqcg.delta_update);
			}
		}
		else if (ent)
		{
			*csqcg.self = EDICT_TO_PROG(prinst, (void*)ent);
			PR_ExecuteProgram(prinst, csqcg.delta_remove);
			deltaedplayerents[i] = NULL;
		}
	}

	oldlist = &loadedcsqcpack[loadedcsqcpacknum];
	loadedcsqcpacknum ^= 1;
	newlist = &loadedcsqcpack[loadedcsqcpacknum];
	newlist->numents = 0;

	for (i = 0; i < pack->num_entities; i++)
	{
		src = &pack->entities[i];
// CL_LinkPacketEntities

#ifndef _MSC_VER
#warning what to do here?
#endif
//		if (csqcent[src->number])
//			continue;	//don't add the entity if we have one sent specially via csqc protocols.

		if (oldidx == oldlist->numents)
		{	//reached the end of the old frame's ents
			oldent = NULL;
		}
		else
		{
			while (oldidx < oldlist->numents && oldlist->entnum[oldidx] < src->number)
			{
				//this entity is stale, remove it.
				oldent = oldlist->entptr[oldidx];
				*csqcg.self = EDICT_TO_PROG(prinst, (void*)oldent);
				PR_ExecuteProgram(prinst, csqcg.delta_remove);
				oldidx++;
			}

			if (src->number < oldlist->entnum[oldidx])
				oldent = NULL;
			else
			{
				oldent = oldlist->entptr[oldidx];
				oldidx++;
			}
		}

		if (src->number < maxcsqcentities && csqcent[src->number])
		{
			//in the csqc list
			if (oldent)
			{
				*csqcg.self = EDICT_TO_PROG(prinst, (void*)oldent);
				PR_ExecuteProgram(prinst, csqcg.delta_remove);
			}
			continue;
		}

		//note: we don't delta the state here. we just replace the old.
		//its already lerped

		if (oldent)
			ent = oldent;
		else
			ent = (csqcedict_t *)ED_Alloc(prinst);

		CSQC_EntStateToCSQC(flags, servertime, src, ent);

		if (csqcg.delta_update)
		{
			*csqcg.self = EDICT_TO_PROG(prinst, (void*)ent);
			G_FLOAT(OFS_PARM0) = !oldent;
			PR_ExecuteProgram(prinst, csqcg.delta_update);
		}

		if (newlist->maxents <= newidx)
		{
			newlist->maxents = newidx + 64;
			newlist->entptr = BZ_Realloc(newlist->entptr, sizeof(*newlist->entptr)*newlist->maxents);
			newlist->entnum = BZ_Realloc(newlist->entnum, sizeof(*newlist->entnum)*newlist->maxents);
		}
		newlist->entptr[newidx] = ent;
		newlist->entnum[newidx] = src->number;
		newidx++;

	}

	//remove any unreferenced ents stuck on the end
	while (oldidx < oldlist->numents)
	{
		oldent = oldlist->entptr[oldidx];
		*csqcg.self = EDICT_TO_PROG(prinst, (void*)oldent);
		PR_ExecuteProgram(prinst, csqcg.delta_remove);
		oldidx++;
	}

	newlist->numents = newidx;
}
#endif

#define PF_FixTen PF_Fixme,PF_Fixme,PF_Fixme,PF_Fixme,PF_Fixme,PF_Fixme,PF_Fixme,PF_Fixme,PF_Fixme,PF_Fixme

//prefixes:
//PF_ - common, works on any vm
//PF_cs_ - works in csqc only (dependant upon globals or fields)
//PF_cl_ - works in csqc and menu (if needed...)

//these are the builtins that still need to be added.
#define PS_cs_setattachment		PF_Fixme

//warning: functions that depend on globals are bad, mkay?
static struct {
	char *name;
	builtin_t bifunc;
	int ebfsnum;
}  BuiltinList[] = {
//0
	{"makevectors",	PF_cs_makevectors, 1},		// #1 void() makevectors (QUAKE)
	{"setorigin",	PF_cs_SetOrigin, 2},		// #2 void(entity e, vector org) setorigin (QUAKE)
	{"setmodel",	PF_cs_SetModel, 3},			// #3 void(entity e, string modl) setmodel (QUAKE)
	{"setsize",	PF_cs_SetSize, 4},			// #4 void(entity e, vector mins, vector maxs) setsize (QUAKE)
//5
	{"debugbreak",	PF_cs_break, 6},			// #6 void() debugbreak (QUAKE)
	{"random",	PF_random,	7},				// #7 float() random (QUAKE)
	{"sound",	PF_cs_sound,	8},			// #8 void(entity e, float chan, string samp, float vol, float atten) sound (QUAKE)
	{"normalize",	PF_normalize,	9},			// #9 vector(vector in) normalize (QUAKE)
//10
	{"error",	PF_error,	10},				// #10 void(string errortext) error (QUAKE)
	{"objerror",	PF_objerror,	11},			// #11 void(string errortext) onjerror (QUAKE)
	{"vlen",	PF_vlen,	12},				// #12 float(vector v) vlen (QUAKE)
	{"vectoyaw",	PF_vectoyaw,	13},			// #13 float(vector v) vectoyaw (QUAKE)
	{"spawn",	PF_Spawn,	14},				// #14 entity() spawn (QUAKE)
	{"remove",	PF_cs_remove,	15},			// #15 void(entity e) remove (QUAKE)
	{"traceline",	PF_cs_traceline,	16},		// #16 void(vector v1, vector v2, float nomonst, entity forent) traceline (QUAKE)
	{"checkclient",	PF_NoCSQC,	17},				// #17 entity() checkclient (QUAKE) (don't support)
	{"findstring",	PF_FindString,	18},			// #18 entity(entity start, .string fld, string match) findstring (QUAKE)
	{"precache_sound",	PF_cs_PrecacheSound,	19},	// #19 void(string str) precache_sound (QUAKE)
//20
	{"precache_model",	PF_cs_PrecacheModel,	20},	// #20 void(string str) precache_model (QUAKE)
	{"stuffcmd",	PF_NoCSQC,	21},		// #21 void(entity client, string s) stuffcmd (QUAKE) (don't support)
	{"findradius",	PF_cs_findradius,	22},		// #22 entity(vector org, float rad) findradius (QUAKE)
	{"bprint",	PF_NoCSQC,	23},				// #23 void(string s, ...) bprint (QUAKE) (don't support)
	{"sprint",	PF_NoCSQC,	24},				// #24 void(entity e, string s, ...) sprint (QUAKE) (don't support)
	{"dprint",	PF_dprint,	25},				// #25 void(string s, ...) dprint (QUAKE)
	{"ftos",	PF_ftos,	26},				// #26 string(float f) ftos (QUAKE)
	{"vtos",	PF_vtos,	27},				// #27 string(vector f) vtos (QUAKE)
	{"coredump",	PF_coredump,	28},			// #28 void(void) coredump (QUAKE)
	{"traceon",	PF_traceon,	29},				// #29 void() traceon (QUAKE)
//30
	{"traceoff",	PF_traceoff,	30},			// #30 void() traceoff (QUAKE)
	{"eprint",	PF_eprint,	31},				// #31 void(entity e) eprint (QUAKE)
	{"walkmove",	PF_cs_walkmove,	32},			// #32 float(float yaw, float dist) walkmove (QUAKE)
	{"?",	PF_Fixme,	33},				// #33
	{"droptofloor",	PF_cs_droptofloor,	34},		// #34
	{"lightstyle",	PF_cs_lightstyle,	35},		// #35 void(float lightstyle, string stylestring) lightstyle (QUAKE)
	{"rint",	PF_rint,	36},				// #36 float(float f) rint (QUAKE)
	{"floor",	PF_floor,	37},				// #37 float(float f) floor (QUAKE)
	{"ceil",	PF_ceil,	38},				// #38 float(float f) ceil (QUAKE)
//	{"?",	PF_Fixme,	39},				// #39
//40
	{"checkbottom",	PF_cs_checkbottom,	40},	// #40 float(entity e) checkbottom (QUAKE)
	{"pointcontents",	PF_cs_pointcontents,	41},	// #41 float(vector org) pointcontents (QUAKE)
//	{"?",	PF_Fixme,	42},				// #42
	{"fabs",	PF_fabs,	43},				// #43 float(float f) fabs (QUAKE)
	{"aim",	PF_NoCSQC,	44},				// #44 vector(entity e, float speed) aim (QUAKE) (don't support)
	{"cvar",	PF_cvar,	45},				// #45 float(string cvarname) cvar (QUAKE)
	{"localcmd",	PF_localcmd,	46},			// #46 void(string str) localcmd (QUAKE)
	{"nextent",	PF_nextent,	47},				// #47 entity(entity e) nextent (QUAKE)
	{"particle",	PF_cs_particle,	48},		// #48 void(vector org, vector dir, float colour, float count) particle (QUAKE)
	{"changeyaw",	PF_cs_changeyaw,	49},		// #49 void() changeyaw (QUAKE)
//50
//	{"?",	PF_Fixme,	50},				// #50
	{"vectoangles",	PF_vectoangles,	51},			// #51 vector(vector v) vectoangles (QUAKE)
//	{"WriteByte",	PF_Fixme,	52},				// #52 void(float to, float f) WriteByte (QUAKE)
//	{"WriteChar",	PF_Fixme,	53},				// #53 void(float to, float f) WriteChar (QUAKE)
//	{"WriteShort",	PF_Fixme,	54},				// #54 void(float to, float f) WriteShort (QUAKE)

//	{"WriteLong",	PF_Fixme,	55},				// #55 void(float to, float f) WriteLong (QUAKE)
//	{"WriteCoord",	PF_Fixme,	56},				// #56 void(float to, float f) WriteCoord (QUAKE)
//	{"WriteAngle",	PF_Fixme,	57},				// #57 void(float to, float f) WriteAngle (QUAKE)
//	{"WriteString",	PF_Fixme,	58},				// #58 void(float to, float f) WriteString (QUAKE)
//	{"WriteEntity",	PF_Fixme,	59},				// #59 void(float to, float f) WriteEntity (QUAKE)

//60
	{"sin",	PF_Sin,	60},					// #60 float(float angle) sin (DP_QC_SINCOSSQRTPOW)
	{"cos",	PF_Cos,	61},					// #61 float(float angle) cos (DP_QC_SINCOSSQRTPOW)
	{"sqrt",	PF_Sqrt,	62},				// #62 float(float value) sqrt (DP_QC_SINCOSSQRTPOW)
	{"changepitch",	PF_cs_changepitch,	63},		// #63 void(entity ent) changepitch (DP_QC_CHANGEPITCH)
	{"tracetoss",	PF_cs_tracetoss,	64},		// #64 void(entity ent, entity ignore) tracetoss (DP_QC_TRACETOSS)

	{"etos",	PF_etos,	65},				// #65 string(entity ent) etos (DP_QC_ETOS)
	{"?",	PF_Fixme,	66},				// #66
	{"movetogoal",	PF_cs_movetogoal,	67},				// #67 void(float step) movetogoal (QUAKE)
	{"precache_file",	PF_NoCSQC,	68},				// #68 void(string s) precache_file (QUAKE) (don't support)
	{"makestatic",	PF_cs_makestatic,	69},		// #69 void(entity e) makestatic (QUAKE)
//70
	{"changelevel",	PF_NoCSQC,	70},				// #70 void(string mapname) changelevel (QUAKE) (don't support)
//	{"?",	PF_Fixme,	71},				// #71
	{"cvar_set",	PF_cvar_set,	72},			// #72 void(string cvarname, string valuetoset) cvar_set (QUAKE)
	{"centerprint",	PF_NoCSQC,	73},				// #73 void(entity ent, string text) centerprint (QUAKE) (don't support - cprint is supported instead)
	{"ambientsound",	PF_cl_ambientsound,	74},		// #74 void (vector pos, string samp, float vol, float atten) ambientsound (QUAKE)

	{"precache_model2",	PF_cs_PrecacheModel,	80},	// #75 void(string str) precache_model2 (QUAKE)
	{"precache_sound2",	PF_cs_PrecacheSound,	76},	// #76 void(string str) precache_sound2 (QUAKE)
	{"precache_file2",	PF_NoCSQC,	77},				// #77 void(string str) precache_file2 (QUAKE)
	{"setspawnparms",	PF_NoCSQC,	78},				// #78 void() setspawnparms (QUAKE) (don't support)
	{"logfrag",	PF_NoCSQC,	79},				// #79 void(entity killer, entity killee) logfrag (QW_ENGINE) (don't support)

//80
	{"infokey",	PF_NoCSQC,	80},				// #80 string(entity e, string keyname) infokey (QW_ENGINE) (don't support)
	{"stof",	PF_stof,	81},				// #81 float(string s) stof (FRIK_FILE or QW_ENGINE)
	{"multicast",	PF_NoCSQC,	82},				// #82 void(vector where, float set) multicast (QW_ENGINE) (don't support)


//90
	{"tracebox",	PF_cs_tracebox,	90},
	{"randomvec",	PF_randomvector,	91},		// #91 vector() randomvec (DP_QC_RANDOMVEC)
	{"getlight",	PF_cl_getlight,	92},			// #92 vector(vector org) getlight (DP_QC_GETLIGHT)
	{"registercvar",	PF_registercvar,	93},		// #93 void(string cvarname, string defaultvalue) registercvar (DP_QC_REGISTERCVAR)
	{"min",	PF_min,	94},				// #94 float(float a, floats) min (DP_QC_MINMAXBOUND)

	{"max",	PF_max,	95},					// #95 float(float a, floats) max (DP_QC_MINMAXBOUND)
	{"bound",	PF_bound,	96},				// #96 float(float minimum, float val, float maximum) bound (DP_QC_MINMAXBOUND)
	{"pow",	PF_pow,	97},					// #97 float(float value) pow (DP_QC_SINCOSSQRTPOW)
	{"findfloat",	PF_FindFloat,	98},			// #98 entity(entity start, .float fld, float match) findfloat (DP_QC_FINDFLOAT)
	{"checkextension",	PF_checkextension,	99},		// #99 float(string extname) checkextension (EXT_CSQC)

//110
	{"fopen",	PF_fopen,	110},				// #110 float(string strname, float accessmode) fopen (FRIK_FILE)
	{"fclose",	PF_fclose,	111},				// #111 void(float fnum) fclose (FRIK_FILE)
	{"fgets",	PF_fgets,	112},				// #112 string(float fnum) fgets (FRIK_FILE)
	{"fputs",	PF_fputs,	113},				// #113 void(float fnum, string str) fputs (FRIK_FILE)
	{"strlen",	PF_strlen,	114},				// #114 float(string str) strlen (FRIK_FILE)

	{"strcat",	PF_strcat,	115},				// #115 string(string str1, string str2, ...) strcat (FRIK_FILE)
	{"substring",	PF_substring,	116},			// #116 string(string str, float start, float length) substring (FRIK_FILE)
	{"stov",	PF_stov,	117},				// #117 vector(string str) stov (FRIK_FILE)
	{"strzone",	PF_dupstring,	118},			// #118 string(string str) dupstring (FRIK_FILE)
	{"strunzone",	PF_forgetstring,	119},		// #119 void(string str) freestring (FRIK_FILE)

//200
	{"precachemodel",	PF_cs_PrecacheModel,	200},
	{"eterncall",	PF_externcall,	201},
	{"addprogs",	PF_cs_addprogs,	202},
	{"externvalue",	PF_externvalue,	203},
	{"externset",	PF_externset,	204},

	{"externrefcall",	PF_externrefcall,	205},
	{"instr",	PF_instr,	206},
	{"openportal",	PF_cs_OpenPortal,	207},	//q2bsps
	{"registertempent",	PF_NoCSQC,	208},//{"RegisterTempEnt", PF_RegisterTEnt,	0,		0,		0,		208},
	{"customtempent",	PF_NoCSQC,	209},//{"CustomTempEnt",	PF_CustomTEnt,		0,		0,		0,		209},
//210
//	{"fork",	PF_Fixme,	210},//{"fork",			PF_Fork,			0,		0,		0,		210},
	{"abort",	PF_Abort,	211}, //#211 void() abort (FTE_MULTITHREADED)
//	{"sleep",	PF_Fixme,	212},//{"sleep",			PF_Sleep,			0,		0,		0,		212},
	{"forceinfokey",	PF_NoCSQC,	213},//{"forceinfokey",	PF_ForceInfoKey,	0,		0,		0,		213},
	{"chat",	PF_NoCSQC,	214},//{"chat",			PF_chat,			0,		0,		0,		214},// #214 void(string filename, float starttag, entity edict) SV_Chat (FTE_NPCCHAT)

	{"particle2",	PF_cs_particle2,	215}, //215 (FTE_PEXT_HEXEN2)
	{"particle3",	PF_cs_particle3,	216}, //216 (FTE_PEXT_HEXEN2)
	{"particle4",	PF_cs_particle4,	217}, //217 (FTE_PEXT_HEXEN2)

//EXT_DIMENSION_PLANES
	{"bitshift",	PF_bitshift,	218},		//#218 bitshift (EXT_DIMENSION_PLANES)
	{"te_lightningblood",	PF_cl_te_lightningblood,	219},// #219 te_lightningblood void(vector org) (FTE_TE_STANDARDEFFECTBUILTINS)

//220
//	{"map_builtin",		PF_Fixme,	220},	//like #100 - takes 2 args. arg0 is builtinname, 1 is number to map to.
	{"strstrofs",	PF_strstrofs,	221},	// #221 float(string s1, string sub) strstrofs (FTE_STRINGS)
	{"str2chr",	PF_str2chr,	222},		// #222 float(string str, float index) str2chr (FTE_STRINGS)
	{"chr2str",	PF_chr2str,	223},		// #223 string(float chr, ...) chr2str (FTE_STRINGS)
	{"strconv",	PF_strconv,	224},		// #224 string(float ccase, float redalpha, float redchars, string str, ...) strconv (FTE_STRINGS)

	{"strpad",	PF_strpad,	225},		// #225 string strpad(float pad, string str1, ...) strpad (FTE_STRINGS)
	{"infoadd",	PF_infoadd,	226},		// #226 string(string old, string key, string value) infoadd
	{"infoget",	PF_infoget,	227},		// #227 string(string info, string key) infoget
	{"strncmp",	PF_strncmp,	228},		// #228 float(string s1, string s2, float len) strncmp (FTE_STRINGS)
	{"strcasecmp",	PF_strcasecmp,	229},	// #229 float(string s1, string s2) strcasecmp (FTE_STRINGS)

//230
	{"strncasecmp",	PF_strncasecmp,	230},	// #230 float(string s1, string s2, float len) strncasecmp (FTE_STRINGS)
	{"clientstat",	PF_NoCSQC,	231},		// #231 clientstat
	{"runclientphys",	PF_NoCSQC,	232},		// #232 runclientphys
	{"isbackbuffered",	PF_NoCSQC,	233},		// #233 float(entity ent) isbackbuffered
	{"rotatevectorsbytag",	PF_rotatevectorsbytag,	234},	// #234

	{"rotatevectorsbyangle",	PF_rotatevectorsbyangles,	235}, // #235
	{"rotatevectorsbymatrix",	PF_rotatevectorsbymatrix,	236}, // #236
	{"skinforname",	PF_skinforname,	237},		// #237
	{"shaderforname",	PF_shaderforname,	238},	// #238
	{"te_bloodqw",	PF_cl_te_bloodqw,	239},	// #239 void te_bloodqw(vector org[, float count]) (FTE_TE_STANDARDEFFECTBUILTINS)

	{"stoi",			PF_stoi,			259},
	{"itos",			PF_itos,			260},
	{"stoh",			PF_stoh,			261},
	{"htos",			PF_htos,			262},

	{"skel_create",			PF_skel_create,			263},//float(float modlindex) skel_create = #263; // (FTE_CSQC_SKELETONOBJECTS)
	{"skel_build",			PF_skel_build,			264},//float(float skel, entity ent, float modelindex, float retainfrac, float firstbone, float lastbone) skel_build = #264; // (FTE_CSQC_SKELETONOBJECTS)
	{"skel_get_numbones",	PF_skel_get_numbones,	265},//float(float skel) skel_get_numbones = #265; // (FTE_CSQC_SKELETONOBJECTS)
	{"skel_get_bonename",	PF_skel_get_bonename,	266},//string(float skel, float bonenum) skel_get_bonename = #266; // (FTE_CSQC_SKELETONOBJECTS) (returns tempstring)
	{"skel_get_boneparent",	PF_skel_get_boneparent,	267},//float(float skel, float bonenum) skel_get_boneparent = #267; // (FTE_CSQC_SKELETONOBJECTS)
	{"skel_find_bone",		PF_skel_find_bone,		268},//float(float skel, string tagname) skel_get_boneidx = #268; // (FTE_CSQC_SKELETONOBJECTS)
	{"skel_get_bonerel",	PF_skel_get_bonerel,	269},//vector(float skel, float bonenum) skel_get_bonerel = #269; // (FTE_CSQC_SKELETONOBJECTS) (sets v_forward etc)
	{"skel_get_boneabs",	PF_skel_get_boneabs,	270},//vector(float skel, float bonenum) skel_get_boneabs = #270; // (FTE_CSQC_SKELETONOBJECTS) (sets v_forward etc)
	{"skel_set_bone",		PF_skel_set_bone,		271},//void(float skel, float bonenum, vector org) skel_set_bone = #271; // (FTE_CSQC_SKELETONOBJECTS) (reads v_forward etc)
	{"skel_mul_bone",		PF_skel_mul_bone,		272},//void(float skel, float bonenum, vector org) skel_mul_bone = #272; // (FTE_CSQC_SKELETONOBJECTS) (reads v_forward etc)
	{"skel_mul_bones",		PF_skel_mul_bones,		273},//void(float skel, float startbone, float endbone, vector org) skel_mul_bone = #273; // (FTE_CSQC_SKELETONOBJECTS) (reads v_forward etc)
	{"skel_copybones",		PF_skel_copybones,		274},//void(float skeldst, float skelsrc, float startbone, float entbone) skel_copybones = #274; // (FTE_CSQC_SKELETONOBJECTS)
	{"skel_delete",			PF_skel_delete,			275},//void(float skel) skel_delete = #275; // (FTE_CSQC_SKELETONOBJECTS)
	{"frameforname",		PF_frameforname,		276},//void(float modidx, string framename) frameforname = #276 (FTE_CSQC_SKELETONOBJECTS)
	{"frameduration",		PF_frameduration,		277},//void(float modidx, float framenum) frameduration = #277 (FTE_CSQC_SKELETONOBJECTS)

//300
	{"clearscene",	PF_R_ClearScene,	300},				// #300 void() clearscene (EXT_CSQC)
	{"addentities",	PF_R_AddEntityMask,	301},				// #301 void(float mask) addentities (EXT_CSQC)
	{"addentity",	PF_R_AddEntity,	302},					// #302 void(entity ent) addentity (EXT_CSQC)
	{"setproperty",	PF_R_SetViewFlag,	303},				// #303 float(float property, ...) setproperty (EXT_CSQC)
	{"renderscene",	PF_R_RenderScene,	304},				// #304 void() renderscene (EXT_CSQC)

	{"dynamiclight_add",	PF_R_DynamicLight_Add,	305},			// #305 float(vector org, float radius, vector lightcolours) adddynamiclight (EXT_CSQC)

	{"R_BeginPolygon",	PF_R_PolygonBegin,	306},				// #306 void(string texturename) R_BeginPolygon (EXT_CSQC_???)
	{"R_PolygonVertex",	PF_R_PolygonVertex,	307},				// #307 void(vector org, vector texcoords, vector rgb, float alpha) R_PolygonVertex (EXT_CSQC_???)
	{"R_EndPolygon",	PF_R_PolygonEnd,	308},				// #308 void() R_EndPolygon (EXT_CSQC_???)

	{"getproperty",	PF_R_GetViewFlag,	309},				// #309 vector/float(float property) getproperty (EXT_CSQC_1)

//310
//maths stuff that uses the current view settings.
	{"unproject",	PF_cs_unproject,	310},				// #310 vector (vector v) unproject (EXT_CSQC)
	{"project",	PF_cs_project,		311},				// #311 vector (vector v) project (EXT_CSQC)

//	{"?",	PF_Fixme,			312},				// #312
//	{"?",	PF_Fixme,		313},					// #313

//2d (immediate) operations
//	{"drawtextfield", PF_CL_DrawTextField,  314},
	{"drawline",	PF_CL_drawline,			315},			// #315 void(float width, vector pos1, vector pos2) drawline (EXT_CSQC)
	{"iscachedpic",	PF_CL_is_cached_pic,		316},		// #316 float(string name) iscachedpic (EXT_CSQC)
	{"precache_pic",	PF_CL_precache_pic,			317},		// #317 string(string name, float trywad) precache_pic (EXT_CSQC)
	{"draw_getimagesize",	PF_CL_drawgetimagesize,		318},		// #318 vector(string picname) draw_getimagesize (EXT_CSQC)
	{"freepic",	PF_CL_free_pic,				319},		// #319 void(string name) freepic (EXT_CSQC)
//320
	{"drawcharacter",	PF_CL_drawcharacter,		320},		// #320 float(vector position, float character, vector scale, vector rgb, float alpha [, float flag]) drawcharacter (EXT_CSQC, [EXT_CSQC_???])
	{"drawrawstring",	PF_CL_drawrawstring,				321},	// #321 float(vector position, string text, vector scale, vector rgb, float alpha [, float flag]) drawstring (EXT_CSQC, [EXT_CSQC_???])
	{"drawpic",	PF_CL_drawpic,				322},		// #322 float(vector position, string pic, vector size, vector rgb, float alpha [, float flag]) drawpic (EXT_CSQC, [EXT_CSQC_???])
	{"drawfill",	PF_CL_drawfill,				323},		// #323 float(vector position, vector size, vector rgb, float alpha [, float flag]) drawfill (EXT_CSQC, [EXT_CSQC_???])
	{"drawsetcliparea",	PF_CL_drawsetcliparea,			324},	// #324 void(float x, float y, float width, float height) drawsetcliparea (EXT_CSQC_???)
	{"drawresetcliparea",	PF_CL_drawresetcliparea,	325},		// #325 void(void) drawresetcliparea (EXT_CSQC_???)

	{"drawstring",	PF_CL_drawcolouredstring,						326},	// #326
	{"stringwidth",	PF_CL_stringwidth,					327},	// #327 EXT_CSQC_'DARKPLACES'
	{"drawsubpic",	PF_CL_drawsubpic,						328},	// #328 EXT_CSQC_'DARKPLACES'
//	{"?",	PF_Fixme,						329},	// #329 EXT_CSQC_'DARKPLACES'

//330
	{"getstatf",	PF_cs_getstatf,					330},	// #330 float(float stnum) getstatf (EXT_CSQC)
	{"getstatbits",	PF_cs_getstatbits,					331},	// #331 float(float stnum) getstatbits (EXT_CSQC)
	{"getstats",	PF_cs_getstats,					332},	// #332 string(float firststnum) getstats (EXT_CSQC)
	{"setmodelindex",	PF_cs_SetModelIndex,			333},	// #333 void(entity e, float mdlindex) setmodelindex (EXT_CSQC)
	{"modelnameforindex",	PF_cs_ModelnameForIndex,		334},	// #334 string(float mdlindex) modelnameforindex (EXT_CSQC)

	{"particleeffectnum",	PF_cs_particleeffectnum,			335},	// #335 float(string effectname) particleeffectnum (EXT_CSQC)
	{"trailparticles",	PF_cs_trailparticles,			336},	// #336 void(float effectnum, entity ent, vector start, vector end) trailparticles (EXT_CSQC),
	{"pointparticles",	PF_cs_pointparticles,			337},	// #337 void(float effectnum, vector origin [, vector dir, float count]) pointparticles (EXT_CSQC)

	{"cprint",	PF_cl_cprint,					338},	// #338 void(string s) cprint (EXT_CSQC)
	{"print",	PF_print,						339},	// #339 void(string s) print (EXT_CSQC)

//340
	{"keynumtostring",	PF_cl_keynumtostring,			340},	// #340 string(float keynum) keynumtostring (EXT_CSQC)
	{"stringtokeynum",	PF_cl_stringtokeynum,			341},	// #341 float(string keyname) stringtokeynum (EXT_CSQC)
	{"getkeybind",	PF_cl_getkeybind,				342},	// #342 string(float keynum) getkeybind (EXT_CSQC)

//	{"?",	PF_Fixme,						343},	// #343
//	{"?",	PF_Fixme,						344},	// #344

	{"getinputstate",	PF_cs_getinputstate,			345},	// #345 float(float framenum) getinputstate (EXT_CSQC)
	{"setsensitivityscaler",	PF_cs_setsensativityscaler, 	346},	// #346 void(float sens) setsensitivityscaler (EXT_CSQC)

	{"runstandardplayerphysics",	PF_cs_runplayerphysics,			347},	// #347 void() runstandardplayerphysics (EXT_CSQC)

	{"getplayerkeyvalue",	PF_cs_getplayerkey,				348},	// #348 string(float playernum, string keyname) getplayerkeyvalue (EXT_CSQC)

	{"isdemo",	PF_cl_playingdemo,				349},	// #349 float() isdemo (EXT_CSQC)
//350
	{"isserver",	PF_cl_runningserver,			350},	// #350 float() isserver (EXT_CSQC)

	{"SetListener",	PF_cs_setlistener, 				351},	// #351 void(vector origin, vector forward, vector right, vector up) SetListener (EXT_CSQC)
	{"registercommand",	PF_cs_registercommand,			352},	// #352 void(string cmdname) registercommand (EXT_CSQC)
	{"wasfreed",	PF_WasFreed,					353},	// #353 float(entity ent) wasfreed (EXT_CSQC) (should be availabe on server too)

	{"serverkey",	PF_cs_serverkey,				354},	// #354 string(string key) serverkey;
	{"getentitytoken",	PF_cs_getentitytoken,			355},	// #355 string() getentitytoken;
//	{"?",	PF_Fixme,						356},	// #356
//	{"?",	PF_Fixme,						357},	// #357
//	{"?",	PF_Fixme,						358},	// #358
	{"sendevent",	PF_cs_sendevent,				359},	// #359	void(string evname, string evargs, ...) (EXT_CSQC_1)

//360
//note that 'ReadEntity' is pretty hard to implement reliably. Modders should use a combination of ReadShort, and findfloat, and remember that it might not be known clientside (pvs culled or other reason)
	{"readbyte",	PF_ReadByte,					360},	// #360 float() readbyte (EXT_CSQC)
	{"readchar",	PF_ReadChar,					361},	// #361 float() readchar (EXT_CSQC)
	{"readshort",	PF_ReadShort,					362},	// #362 float() readshort (EXT_CSQC)
	{"readlong",	PF_ReadLong,					363},	// #363 float() readlong (EXT_CSQC)
	{"readcoord",	PF_ReadCoord,					364},	// #364 float() readcoord (EXT_CSQC)

	{"readangle",	PF_ReadAngle,					365},	// #365 float() readangle (EXT_CSQC)
	{"readstring",	PF_ReadString,					366},	// #366 string() readstring (EXT_CSQC)
	{"readfloat",	PF_ReadFloat,					367},	// #367 string() readfloat (EXT_CSQC)
	{"readentitynum",	PF_ReadEntityNum,				368},	// #368 float() readentitynum (EXT_CSQC)

	{"readserverentitystate",	PF_ReadServerEntityState,		369},	// #369 void(float flags, float simtime) readserverentitystate (EXT_CSQC_1)
//	{"readsingleentitystate",	PF_ReadSingleEntityState,		370},
	{"deltalisten",	PF_DeltaListen,					371},		// #371 float(string modelname, float flags) deltalisten  (EXT_CSQC_1)

	{"dynamiclight_get",	PF_R_DynamicLight_Get,	372},
	{"dynamiclight_set",	PF_R_DynamicLight_Set,	373},

//400
	{"copyentity",	PF_cs_copyentity,		400},	// #400 void(entity from, entity to) copyentity (DP_QC_COPYENTITY)
	{"setcolors",	PF_NoCSQC,				401},	// #401 void(entity cl, float colours) setcolors (DP_SV_SETCOLOR) (don't implement)
	{"findchain",	PF_cs_findchain,			402},	// #402 entity(string field, string match) findchain (DP_QC_FINDCHAIN)
	{"findchainfloat",	PF_cs_findchainfloat,		403},	// #403 entity(float fld, float match) findchainfloat (DP_QC_FINDCHAINFLOAT)
	{"effect",	PF_cl_effect,		404},		// #404 void(vector org, string modelname, float startframe, float endframe, float framerate) effect (DP_SV_EFFECT)

	{"te_blood",	PF_cl_te_blooddp,		405},	// #405 void(vector org, vector velocity, float howmany) te_blood (DP_TE_BLOOD)
	{"te_bloodshower",	PF_cl_te_bloodshower,406},		// #406 void(vector mincorner, vector maxcorner, float explosionspeed, float howmany) te_bloodshower (DP_TE_BLOODSHOWER)
	{"te_explosionrgb",	PF_cl_te_explosionrgb,	407},	// #407 void(vector org, vector color) te_explosionrgb (DP_TE_EXPLOSIONRGB)
	{"te_particlecube",	PF_cl_te_particlecube,408},		// #408 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color, float gravityflag, float randomveljitter) te_particlecube (DP_TE_PARTICLECUBE)
	{"te_particlerain",	PF_cl_te_particlerain,	409},	// #409 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color) te_particlerain (DP_TE_PARTICLERAIN)

	{"te_particlesnow",	PF_cl_te_particlesnow,410},		// #410 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color) te_particlesnow (DP_TE_PARTICLESNOW)
	{"te_spark",	PF_cl_te_spark,		411},		// #411 void(vector org, vector vel, float howmany) te_spark (DP_TE_SPARK)
	{"te_gunshotquad",	PF_cl_te_gunshotquad,	412},	// #412 void(vector org) te_gunshotquad (DP_TE_QUADEFFECTS1)
	{"te_spikequad",	PF_cl_te_spikequad,	413},		// #413 void(vector org) te_spikequad (DP_TE_QUADEFFECTS1)
	{"te_superspikequad",	PF_cl_te_superspikequad,414},	// #414 void(vector org) te_superspikequad (DP_TE_QUADEFFECTS1)

	{"te_explosionquad",	PF_cl_te_explosionquad,	415},	// #415 void(vector org) te_explosionquad (DP_TE_QUADEFFECTS1)
	{"te_smallflash",	PF_cl_te_smallflash,	416},	// #416 void(vector org) te_smallflash (DP_TE_SMALLFLASH)
	{"te_customflash",	PF_cl_te_customflash,	417},	// #417 void(vector org, float radius, float lifetime, vector color) te_customflash (DP_TE_CUSTOMFLASH)
	{"te_gunshot",	PF_cl_te_gunshot,	418},		// #418 void(vector org) te_gunshot (DP_TE_STANDARDEFFECTBUILTINS)
	{"te_spike",	PF_cl_te_spike,		419},		// #419 void(vector org) te_spike (DP_TE_STANDARDEFFECTBUILTINS)

	{"te_superspike",	PF_cl_te_superspike,420},		// #420 void(vector org) te_superspike (DP_TE_STANDARDEFFECTBUILTINS)
	{"te_explosion",	PF_cl_te_explosion,	421},		// #421 void(vector org) te_explosion (DP_TE_STANDARDEFFECTBUILTINS)
	{"te_tarexplosion",	PF_cl_te_tarexplosion,422},		// #422 void(vector org) te_tarexplosion (DP_TE_STANDARDEFFECTBUILTINS)
	{"te_wizspike",	PF_cl_te_wizspike,	423},		// #423 void(vector org) te_wizspike (DP_TE_STANDARDEFFECTBUILTINS)
	{"te_knightspike",	PF_cl_te_knightspike,424},		// #424 void(vector org) te_knightspike (DP_TE_STANDARDEFFECTBUILTINS)

	{"te_lavasplash",	PF_cl_te_lavasplash,425},		// #425 void(vector org) te_lavasplash  (DP_TE_STANDARDEFFECTBUILTINS)
	{"te_teleport",	PF_cl_te_teleport,	426},		// #426 void(vector org) te_teleport (DP_TE_STANDARDEFFECTBUILTINS)
	{"te_explosion2",	PF_cl_te_explosion2,427},		// #427 void(vector org, float color, float colorlength) te_explosion2 (DP_TE_STANDARDEFFECTBUILTINS)
	{"te_lightning1",	PF_cl_te_lightning1,	428},	// #428 void(entity own, vector start, vector end) te_lightning1 (DP_TE_STANDARDEFFECTBUILTINS)
	{"te_lightning2",	PF_cl_te_lightning2,429},		// #429 void(entity own, vector start, vector end) te_lightning2 (DP_TE_STANDARDEFFECTBUILTINS)

	{"te_lightning3",	PF_cl_te_lightning3,430},		// #430 void(entity own, vector start, vector end) te_lightning3 (DP_TE_STANDARDEFFECTBUILTINS)
	{"te_beam",	PF_cl_te_beam,		431},		// #431 void(entity own, vector start, vector end) te_beam (DP_TE_STANDARDEFFECTBUILTINS)
	{"vectorvectors",	PF_cs_vectorvectors,432},		// #432 void(vector dir) vectorvectors (DP_QC_VECTORVECTORS)
	{"te_plasmaburn",	PF_cl_te_plasmaburn,433},		// #433 void(vector org) te_plasmaburn (DP_TE_PLASMABURN)
//	{"getsurfacenumpoints",	PF_Fixme,					434},		// #434 float(entity e, float s) getsurfacenumpoints (DP_QC_GETSURFACE)

//	{"getsurfacepoint",	PF_Fixme,					435},		// #435 vector(entity e, float s, float n) getsurfacepoint (DP_QC_GETSURFACE)
//	{"getsurfacenormal",	PF_Fixme,					436},		// #436 vector(entity e, float s) getsurfacenormal (DP_QC_GETSURFACE)
//	{"getsurfacetexture",	PF_Fixme,				437},			// #437 string(entity e, float s) getsurfacetexture (DP_QC_GETSURFACE)
//	{"getsurfacenearpoint",	PF_Fixme,					438},		// #438 float(entity e, vector p) getsurfacenearpoint (DP_QC_GETSURFACE)
//	{"getsurfaceclippedpoint",	PF_Fixme,				439},			// #439 vector(entity e, float s, vector p) getsurfaceclippedpoint (DP_QC_GETSURFACE)

	{"clientcommand",	PF_NoCSQC,			440},		// #440 void(entity e, string s) clientcommand (KRIMZON_SV_PARSECLIENTCOMMAND) (don't implement)
	{"tokenize",	PF_Tokenize,		441},		// #441 float(string s) tokenize (KRIMZON_SV_PARSECLIENTCOMMAND)
	{"argv",	PF_ArgV,			442},		// #442 string(float n) argv (KRIMZON_SV_PARSECLIENTCOMMAND)
	{"setattachment",	PS_cs_setattachment,443},		// #443 void(entity e, entity tagentity, string tagname) setattachment (DP_GFX_QUAKE3MODELTAGS)
	{"search_begin",	PF_search_begin,	444},		// #444 float	search_begin(string pattern, float caseinsensitive, float quiet) (DP_QC_FS_SEARCH)

	{"search_end",	PF_search_end,			445},	// #445 void	search_end(float handle) (DP_QC_FS_SEARCH)
	{"search_getsize",	PF_search_getsize,	446},		// #446 float	search_getsize(float handle) (DP_QC_FS_SEARCH)
	{"search_getfilename",	PF_search_getfilename,447},		// #447 string	search_getfilename(float handle, float num) (DP_QC_FS_SEARCH)
	{"dp_cvar_string",	PF_cvar_string,		448},		// #448 string(float n) cvar_string (DP_QC_CVAR_STRING)
	{"findflags",	PF_FindFlags,		449},		// #449 entity(entity start, .entity fld, float match) findflags (DP_QC_FINDFLAGS)

	{"findchainflags",	PF_cs_findchainflags,	450},		// #450 entity(.float fld, float match) findchainflags (DP_QC_FINDCHAINFLAGS)
	{"gettagindex",	PF_cs_gettagindex,	451},		// #451 float(entity ent, string tagname) gettagindex (DP_MD3_TAGSINFO)
	{"gettaginfo",	PF_cs_gettaginfo,	452},		// #452 vector(entity ent, float tagindex) gettaginfo (DP_MD3_TAGSINFO)
	{"dropclient",	PF_NoCSQC,			453},		// #453 void(entity player) dropclient (DP_SV_BOTCLIENT) (don't implement)
	{"spawnclient",	PF_NoCSQC,			454},		// #454	entity() spawnclient (DP_SV_BOTCLIENT) (don't implement)

	{"clienttype",	PF_NoCSQC,			455},		// #455 float(entity client) clienttype (DP_SV_BOTCLIENT) (don't implement)


//	{"WriteUnterminatedString",PF_WriteString2,		456},	//writestring but without the null terminator. makes things a little nicer.

//DP_TE_FLAMEJET
//	{"te_flamejet",		PF_te_flamejet,			457},	// #457 void(vector org, vector vel, float howmany) te_flamejet

	//no 458 documented.

//DP_QC_EDICT_NUM
	{"edict_num",		PF_edict_for_num,		459},	// #459 entity(float entnum) edict_num

//DP_QC_STRINGBUFFERS
	{"buf_create",		PF_buf_create,		460},	// #460 float() buf_create
	{"buf_del",			PF_buf_del,				461},	// #461 void(float bufhandle) buf_del
	{"buf_getsize",		PF_buf_getsize,		462},	// #462 float(float bufhandle) buf_getsize
	{"buf_copy",		PF_buf_copy,		463},	// #463 void(float bufhandle_from, float bufhandle_to) buf_copy
	{"buf_sort",		PF_buf_sort,		464},	// #464 void(float bufhandle, float sortpower, float backward) buf_sort
	{"buf_implode",		PF_buf_implode,		465},	// #465 string(float bufhandle, string glue) buf_implode
	{"bufstr_get",		PF_bufstr_get,		466},	// #466 string(float bufhandle, float string_index) bufstr_get
	{"bufstr_set",		PF_bufstr_set,		467},	// #467 void(float bufhandle, float string_index, string str) bufstr_set
	{"bufstr_add",		PF_bufstr_add,		468},	// #468 float(float bufhandle, string str, float order) bufstr_add
	{"bufstr_free",		PF_bufstr_free,			469},	// #469 void(float bufhandle, float string_index) bufstr_free

	//no 470 documented

//DP_QC_ASINACOSATANATAN2TAN
	{"asin",			PF_asin,			471},	// #471 float(float s) asin
	{"acos",			PF_acos,			472},	// #472 float(float c) acos
	{"atan",			PF_atan,			473},	// #473 float(float t) atan
	{"atan2",			PF_atan2,			474},	// #474 float(float c, float s) atan2
	{"tan",				PF_tan,				475},	// #475 float(float a) tan


////DP_QC_STRINGCOLORFUNCTIONS
	{"strlennocol",		PF_strlennocol,		476},	// #476 float(string s) strlennocol
	{"strdecolorize",	PF_strdecolorize,	477},	// #477 string(string s) strdecolorize

//DP_QC_STRFTIME
	{"strftime",		PF_strftime,		478},	// #478 string(float uselocaltime, string format, ...) strftime

//DP_QC_TOKENIZEBYSEPARATOR
	{"tokenizebyseparator",PF_tokenizebyseparator,	479},	// #479 float(string s, string separator1, ...) tokenizebyseparator

//DP_QC_STRING_CASE_FUNCTIONS
	{"strtolower",		PF_strtolower,		480},	// #476 string(string s) strtolower
	{"strtoupper",		PF_strtoupper,		481},	// #476 string(string s) strlennocol

//DP_QC_CVAR_DEFSTRING
	{"cvar_defstring",	PF_cvar_defstring,	482},	// #482 string(string s) cvar_defstring

//DP_SV_POINTSOUND
	{"pointsound",		PF_cs_pointsound,		483},	// #483 void(vector origin, string sample, float volume, float attenuation) pointsound

//DP_QC_STRREPLACE
	{"strreplace",		PF_strreplace,		484},	// #484 string(string search, string replace, string subject) strreplace
	{"strireplace",		PF_strireplace,		485},	// #485 string(string search, string replace, string subject) strireplace


//DP_QC_GETSURFACEPOINTATTRIBUTE
	{"getsurfacepointattribute",PF_getsurfacepointattribute,	486},	// #486vector(entity e, float s, float n, float a) getsurfacepointattribute

#ifndef NOMEDIA
//DP_GECKO_SUPPORT
	{"gecko_create",	PF_cs_gecko_create,		487},	// #487 float(string name) gecko_create( string name )
	{"gecko_destroy",	PF_cs_gecko_destroy,	488},	// #488 void(string name) gecko_destroy( string name )
	{"gecko_navigate",	PF_cs_gecko_navigate,	489},	// #489 void(string name) gecko_navigate( string name, string URI )
	{"gecko_keyevent",	PF_cs_gecko_keyevent,	490},	// #490 float(string name) gecko_keyevent( string name, float key, float eventtype )
	{"gecko_mousemove",	PF_cs_gecko_mousemove,	491},	// #491 void gecko_mousemove( string name, float x, float y )
	{"gecko_resize",	PF_cs_gecko_resize,	492},	// #492 void gecko_resize( string name, float w, float h )
	{"gecko_get_texture_extent",PF_cs_gecko_get_texture_extent,	493},	// #493 vector gecko_get_texture_extent( string name )
#endif

//DP_QC_CRC16
	{"crc16",			PF_crc16,				494},	// #494 float(float caseinsensitive, string s, ...) crc16

//DP_QC_CVAR_TYPE
	{"cvar_type",		PF_cvar_type,		495},	// #495 float(string name) cvar_type

//DP_QC_ENTITYDATA
	{"numentityfields",	PF_numentityfields,			496},	// #496 float() numentityfields
	{"entityfieldname",	PF_entityfieldname,			497},	// #497 string(float fieldnum) entityfieldname
	{"entityfieldtype",	PF_entityfieldtype,		498},	// #498 float(float fieldnum) entityfieldtype
	{"getentityfieldstring",PF_getentityfieldstring,		499},	// #499 string(float fieldnum, entity ent) getentityfieldstring
	{"putentityfieldstring",PF_putentityfieldstring,	500},	// #500 float(float fieldnum, entity ent, string s) putentityfieldstring

//DP_SV_WRITEPICTURE
	{"WritePicture",	PF_ReadPicture,		501},	// #501 void(float to, string s, float sz) WritePicture

	//no 502 documented

//DP_QC_WHICHPACK
	{"whichpack",		PF_whichpack,			503},	// #503 string(string filename) whichpack

//DP_QC_URI_ESCAPE
	{"uri_escape",		PF_uri_escape,				510},	// #510 string(string in) uri_escape
	{"uri_unescape",	PF_uri_unescape,	511},	// #511 string(string in) uri_unescape = #511;

//DP_QC_NUM_FOR_EDICT
	{"num_for_edict",	PF_num_for_edict,		512},	// #512 float(entity ent) num_for_edict

//DP_QC_URI_GET
	{"uri_get",			PF_uri_get,			513},	// #513 float(string uril, float id) uri_get

	{"tokenize_console",	PF_tokenize_console,		514},
	{"argv_start_index",	PF_argv_start_index,		515},
	{"argv_end_index",		PF_argv_end_index,			516},
	{"buf_cvarlist",		PF_buf_cvarlist,			517},
	{"cvar_description",	PF_cvar_description,		518},

	{"keynumtostring",		PF_cl_keynumtostring,		520},
	{"findkeysforcommand",	PF_cl_findkeysforcommand,	521},

	{NULL}
};

static builtin_t pr_builtin[550];




static jmp_buf csqc_abort;
static progparms_t csqcprogparms;


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

void CSQC_EntSpawn (struct edict_s *e, int loading)
{
	struct csqcedict_s *ent = (csqcedict_t*)e;
#ifdef VM_Q1
	if (!ent->xv)
		ent->xv = (csqcextentvars_t *)(ent->v+1);
#endif
}
pbool CSQC_EntFree (struct edict_s *e)
{
	struct csqcedict_s *ent = (csqcedict_t*)e;
	ent->v->solid = SOLID_NOT;
	ent->xv->drawmask = 0;
	ent->v->modelindex = 0;
	ent->v->think = 0;
	ent->v->nextthink = 0;

#ifdef USEODE
	World_Physics_RemoveFromEntity(&csqc_world, (wedict_t*)ent);
	World_Physics_RemoveJointFromEntity(&csqc_world, (wedict_t*)ent);
#endif

	return true;
}

void CSQC_Event_Touch(world_t *w, wedict_t *s, wedict_t *o)
{
	int oself = *csqcg.self;
	int oother = *csqcg.other;

	*csqcg.self = EDICT_TO_PROG(w->progs, (edict_t*)s);
	*csqcg.other = EDICT_TO_PROG(w->progs, (edict_t*)o);

	PR_ExecuteProgram (w->progs, s->v->touch);

	*csqcg.self = oself;
	*csqcg.other = oother;
}

model_t *CSQC_World_ModelForIndex(world_t *w, int modelindex)
{
	return CSQC_GetModelForIndex(modelindex);
}

void CSQC_Shutdown(void)
{
	search_close_progs(csqcprogs, false);
	if (csqcprogs)
	{
		CSQC_ForgetThreads();
		CloseProgs(csqcprogs);
	}
	csqcprogs = NULL;

#ifdef USEODE
	World_Physics_End(&csqc_world);
#endif

	Z_Free(csqcdelta_pack_new.e);
	memset(&csqcdelta_pack_new, 0, sizeof(csqcdelta_pack_new));
	Z_Free(csqcdelta_pack_old.e);
	memset(&csqcdelta_pack_old, 0, sizeof(csqcdelta_pack_old));

	memset(&deltafunction, 0, sizeof(deltafunction));
	memset(csqcdelta_playerents, 0, sizeof(csqcdelta_playerents));

	csqcmapentitydata = NULL;
	csqcmapentitydataloaded = false;

	in_sensitivityscale = 1;
	csqc_world.num_edicts = 0;

	csqc_usinglistener = false;
}

//when the qclib needs a file, it calls out to this function.
qbyte *CSQC_PRLoadFile (const char *path, void *buffer, int bufsize)
{
	qbyte *file;

	if (!strcmp(path, "csprogs.dat"))
	{
		char newname[MAX_QPATH];
		snprintf(newname, MAX_QPATH, "csprogsvers/%x.dat", csqcchecksum);

		if (csqcchecksum)
		{
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
					if (LittleLong(Com_BlockChecksum(file, com_filesize)) == csqcchecksum)	//and the user wasn't trying to be cunning.
						return file;
				}
			}
		}

		file = COM_LoadStackFile(path, buffer, bufsize);
		if (file && !cls.demoplayback)	//allow them to use csprogs.dat if playing a demo, and don't care about the checksum
		{
			if (csqcchecksum)
			{
				if (cls.protocol == CP_NETQUAKE)
				{
					if (QCRC_Block(file, com_filesize) != csqcchecksum)
						return NULL;
				}
				else
				{
					if (LittleLong(Com_BlockChecksum(file, com_filesize)) != csqcchecksum)
						return NULL;	//not valid
				}

				//back it up
				COM_WriteFile(newname, file, com_filesize);
			}
		}

		return file;

	}

	return COM_LoadStackFile(path, buffer, bufsize);
}

int CSQC_PRFileSize (const char *path)
{
	qbyte *file;

	if (!strcmp(path, "csprogs.dat"))
	{
		char newname[MAX_QPATH];
		snprintf(newname, MAX_QPATH, "csprogsvers/%x.dat", csqcchecksum);

		if (csqcchecksum)
		{
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
					if (LittleLong(Com_BlockChecksum(file, com_filesize)) == csqcchecksum)	//and the user wasn't trying to be cunning.
						return com_filesize+1;
				}
			}
		}

		file = COM_LoadTempFile(path);
		if (file && !cls.demoplayback)	//allow them to use csprogs.dat if playing a demo, and don't care about the checksum
		{
			if (csqcchecksum)
			{
				if (cls.protocol == CP_NETQUAKE)
				{
					if (QCRC_Block(file, com_filesize) != csqcchecksum)
						return -1;	//not valid
				}
				else
				{
					if (LittleLong(Com_BlockChecksum(file, com_filesize)) != csqcchecksum)
						return -1;	//not valid
				}
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
	int i;
	string_t *str;
	csqcedict_t *worldent;
	csqcchecksum = checksum;

	csqc_usinglistener = false;

	//its already running...
	if (csqcprogs)
		return false;

	if (qrenderer == QR_NONE)
	{
		return false;
	}

	if (cl_nocsqc.value)
		return false;

	for (i = 0; i < sizeof(pr_builtin)/sizeof(pr_builtin[0]); i++)
		pr_builtin[i] = PF_Fixme;
	for (i = 0; BuiltinList[i].bifunc; i++)
	{
		if (BuiltinList[i].ebfsnum)
			pr_builtin[BuiltinList[i].ebfsnum] = BuiltinList[i].bifunc;
	}

	csqc_deprecated_warned = false;
	skel_reset();
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

	csqcprogparms.entspawn = CSQC_EntSpawn;//void (*entspawn) (struct edict_s *ent);	//ent has been spawned, but may not have all the extra variables (that may need to be set) set
	csqcprogparms.entcanfree = CSQC_EntFree;//bool (*entcanfree) (struct edict_s *ent);	//return true to stop ent from being freed
	csqcprogparms.stateop = NULL;//StateOp;//void (*stateop) (float var, func_t func);
	csqcprogparms.cstateop = NULL;//CStateOp;
	csqcprogparms.cwstateop = NULL;//CWStateOp;
	csqcprogparms.thinktimeop = NULL;//ThinkTimeOp;

	//used when loading a game
	csqcprogparms.builtinsfor = NULL;//builtin_t *(*builtinsfor) (int num);	//must return a pointer to the builtins that were used before the state was saved.
	csqcprogparms.loadcompleate = NULL;//void (*loadcompleate) (int edictsize);	//notification to reset any pointers.

	csqcprogparms.memalloc = PR_CB_Malloc;//void *(*memalloc) (int size);	//small string allocation	malloced and freed randomly
	csqcprogparms.memfree = PR_CB_Free;//void (*memfree) (void * mem);


	csqcprogparms.globalbuiltins = pr_builtin;//builtin_t *globalbuiltins;	//these are available to all progs
	csqcprogparms.numglobalbuiltins = sizeof(pr_builtin)/sizeof(pr_builtin[0]);

	csqcprogparms.autocompile = PR_COMPILEIGNORE;//enum {PR_NOCOMPILE, PR_COMPILENEXIST, PR_COMPILECHANGED, PR_COMPILEALWAYS} autocompile;

	csqcprogparms.gametime = &csqctime;

	csqcprogparms.sv_edicts = (struct edict_s **)&csqc_world.edicts;
	csqcprogparms.sv_num_edicts = &csqc_world.num_edicts;

	csqcprogparms.useeditor = QCEditor;//void (*useeditor) (char *filename, int line, int nump, char **parms);

	csqctime = Sys_DoubleTime();
	if (!csqcprogs)
	{
		in_sensitivityscale = 1;
		csqcmapentitydataloaded = true;
		csqcprogs = InitProgs(&csqcprogparms);
		csqc_world.progs = csqcprogs;
		csqc_world.usesolidcorpse = true;
		PR_Configure(csqcprogs, -1, 16);
		csqc_world.worldmodel = cl.worldmodel;
		csqc_world.Event_Touch = CSQC_Event_Touch;
		csqc_world.GetCModel = CSQC_World_ModelForIndex;
		World_ClearWorld(&csqc_world);
		CSQC_InitFields();	//let the qclib know the field order that the engine needs.

		csqc_isdarkplaces = false;
		if (PR_LoadProgs(csqcprogs, "csprogs.dat", 22390, NULL, 0) < 0) //no per-progs builtins.
		{
			if (PR_LoadProgs(csqcprogs, "csprogs.dat", 52195, NULL, 0) < 0) //no per-progs builtins.
			{
				if (PR_LoadProgs(csqcprogs, "csprogs.dat", 0, NULL, 0) < 0) //no per-progs builtins.
				{
					CSQC_Shutdown();
					//failed to load or something
					return false;
				}
			}
			else
				csqc_isdarkplaces = true;

			Con_Printf(CON_WARNING "Running outdated or unknown csprogs.dat version\n");
		}
		if (setjmp(csqc_abort))
		{
			CSQC_Shutdown();
			return false;
		}


		PF_InitTempStrings(csqcprogs);

		CSQC_FindGlobals();

		csqc_world.physicstime = 0;

		csqc_fakereadbyte = -1;
		memset(csqcent, 0, sizeof(*csqcent)*maxcsqcentities);	//clear the server->csqc entity translations.

		csqcentsize = PR_InitEnts(csqcprogs, pr_csmaxedicts.value);

		ED_Alloc(csqcprogs);	//we need a world entity.

		//world edict becomes readonly
		worldent = (csqcedict_t *)EDICT_NUM(csqcprogs, 0);
		worldent->isfree = false;

		str = (string_t*)csqcprogs->GetEdictFieldValue(csqcprogs, (edict_t*)worldent, "message", NULL);
		if (str)
			*str = PR_SetString(csqcprogs, cl.levelname);

		str = (string_t*)PR_FindGlobal(csqcprogs, "mapname", 0);
		if (str)
		{
			char *s = Info_ValueForKey(cl.serverinfo, "map");
			if (!*s)
				s = "unknown";
			*str = PR_NewString(csqcprogs, s, strlen(s)+1);
		}

		if (csqcg.init_function)
		{
			void *pr_globals = PR_globals(csqcprogs, PR_CURRENT);
			G_FLOAT(OFS_PARM0) = CSQC_API_VERSION;	//api version
			(((string_t *)pr_globals)[OFS_PARM1] = PR_TempString(csqcprogs, FULLENGINENAME));
			G_FLOAT(OFS_PARM2) = version_number();
			PR_ExecuteProgram(csqcprogs, csqcg.init_function);
		}

		Con_Printf("Loaded csqc\n");
		csqcmapentitydataloaded = false;
	}

	return true; //success!
}

void CSQC_WorldLoaded(void)
{
	csqcedict_t *worldent;

	if (!csqcprogs)
		return;
	if (csqcmapentitydataloaded)
		return;
	csqcmapentitydataloaded = true;
	csqcmapentitydata = cl.worldmodel->entities;

	csqc_world.worldmodel = cl.worldmodel;
#ifdef USEODE
	World_Physics_Start(&csqc_world);
#endif

	worldent = (csqcedict_t *)EDICT_NUM(csqcprogs, 0);
	worldent->v->modelindex = 1;
	worldent->v->model = PR_SetString(csqcprogs, cl.model_name[(int)worldent->v->modelindex]);
	worldent->v->solid = SOLID_BSP;

	if (csqcg.worldloaded)
		PR_ExecuteProgram(csqcprogs, csqcg.worldloaded);
	csqcmapentitydata = NULL;

	worldent->readonly = true;
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

void PR_CSExtensionList_f(void)
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

	//make sure the info is valid
	if (!pr_builtin[0])
	{
		for (i = 0; i < sizeof(pr_builtin)/sizeof(pr_builtin[0]); i++)
			pr_builtin[i] = PF_Fixme;
		for (i = 0; BuiltinList[i].bifunc; i++)
		{
			if (BuiltinList[i].ebfsnum)
				pr_builtin[BuiltinList[i].ebfsnum] = BuiltinList[i].bifunc;
		}
	}


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
				if (!(cls.fteprotocolextensions & (1<<i)))
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

void CSQC_RegisterCvarsAndThings(void)
{
	PF_Common_RegisterCvars();

	Cmd_AddCommand("coredump_csqc", CSQC_CoreDump);
	Cmd_AddCommand ("extensionlist_csqc", PR_CSExtensionList_f);


	Cvar_Register(&pr_csmaxedicts, CSQCPROGSGROUP);
	Cvar_Register(&cl_csqcdebug, CSQCPROGSGROUP);
	Cvar_Register(&cl_nocsqc, CSQCPROGSGROUP);
	Cvar_Register(&pr_csqc_coreonerror, CSQCPROGSGROUP);
}

qboolean CSQC_DrawView(void)
{
#ifdef USEODE
	int ticlimit = 10;
	float ft;
	float mintic = 0.01;
#endif

	if (!csqcg.draw_function || !csqcprogs || !cl.worldmodel)
		return false;

	r_secondaryview = 0;

	CL_CalcClientTime();

	if (csqcg.frametime)
		*csqcg.frametime = host_frametime;

#ifdef USEODE
	while(1)
	{
		ft = cl.servertime - csqc_world.physicstime;
		if (ft < mintic)
			break;
		if (!--ticlimit)
		{
			csqc_world.physicstime = cl.servertime;
			break;
		}
		if (ft > mintic)
			ft = mintic;
		csqc_world.physicstime += ft;

		World_Physics_Frame(&csqc_world, ft, 800);
	}
#else
	csqc_world.physicstime = cl.servertime;
#endif

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

	CSQC_ChangeLocalPlayer(0);

	if (csqcg.cltime)
		*csqcg.cltime = cl.time;
	if (csqcg.svtime)
		*csqcg.svtime = cl.servertime;

	CSQC_RunThreads();	//wake up any qc threads

	//EXT_CSQC_1
	{
		void *pr_globals = PR_globals(csqcprogs, PR_CURRENT);
		G_FLOAT(OFS_PARM0) = vid.width;
		G_FLOAT(OFS_PARM1) = vid.height;
		G_FLOAT(OFS_PARM2) = !m_state;
	}
	//end EXT_CSQC_1
	PR_ExecuteProgram(csqcprogs, csqcg.draw_function);

	return true;
}

qboolean CSQC_KeyPress(int key, int unicode, qboolean down)
{
	void *pr_globals;

	if (!csqcprogs || !csqcg.input_event)
		return false;

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);
	G_FLOAT(OFS_PARM0) = !down;
	G_FLOAT(OFS_PARM1) = MP_TranslateFTEtoDPCodes(key);
	G_FLOAT(OFS_PARM2) = unicode;

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

#pragma message("do we really need the firstbyte parameter here?")
qboolean CSQC_ParseTempEntity(unsigned char firstbyte)
{
	void *pr_globals;
	if (!csqcprogs || !csqcg.parse_tempentity)
		return false;

	csqc_fakereadbyte = firstbyte;
	pr_globals = PR_globals(csqcprogs, PR_CURRENT);
	PR_ExecuteProgram (csqcprogs, csqcg.parse_tempentity);
	csqc_fakereadbyte = -1;
	return !!G_FLOAT(OFS_RETURN);
}

qboolean CSQC_ParseGamePacket(void)
{
	return false;
}

qboolean CSQC_LoadResource(char *resname, char *restype)
{
	void *pr_globals;
	if (!csqcprogs || !csqcg.loadresource)
		return true;

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);
	(((string_t *)pr_globals)[OFS_PARM0] = PR_TempString(csqcprogs, resname));
	(((string_t *)pr_globals)[OFS_PARM0] = PR_TempString(csqcprogs, restype));

	PR_ExecuteProgram (csqcprogs, csqcg.loadresource);

	return !!G_FLOAT(OFS_RETURN);
}

qboolean CSQC_ParsePrint(char *message, int printlevel)
{
	void *pr_globals;
	int bufferpos;
	char *nextline;
	qboolean doflush;
	if (!csqcprogs || !csqcg.parse_print)
	{
		return false;
	}

	bufferpos = strlen(csqc_printbuffer);

	//fix-up faked bot chat
	if (*message == '\1' && *csqc_printbuffer == '\1')
		message++;

	while(*message)
	{
		nextline = strchr(message, '\n');
		if (nextline)
		{
			nextline+=1;
			doflush = true;
		}
		else
		{
			nextline = message+strlen(message);
			doflush = false;
		}

		if (bufferpos + nextline-message >= sizeof(csqc_printbuffer))
		{
			//if this would overflow the buffer, cap its length and flush the buffer
			//this copes with too many strings and too long strings.
			nextline = message + sizeof(csqc_printbuffer)-1 - bufferpos;
			doflush = true;
		}

		memcpy(csqc_printbuffer+bufferpos, message, nextline-message);
		bufferpos += nextline-message;
		csqc_printbuffer[bufferpos] = '\0';
		message = nextline;

		if (doflush)
		{
			pr_globals = PR_globals(csqcprogs, PR_CURRENT);
			(((string_t *)pr_globals)[OFS_PARM0] = PR_TempString(csqcprogs, csqc_printbuffer));
			G_FLOAT(OFS_PARM1) = printlevel;
			PR_ExecuteProgram (csqcprogs, csqcg.parse_print);

			bufferpos = 0;
			csqc_printbuffer[bufferpos] = 0;
		}
	}
	return true;
}

qboolean CSQC_StuffCmd(int lplayernum, char *cmd, char *cmdend)
{
	void *pr_globals;
	char tmp[2];
	if (!csqcprogs || !csqcg.parse_stuffcmd)
		return false;

	CSQC_ChangeLocalPlayer(lplayernum);

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);
	tmp[0] = cmdend[0];
	tmp[1] = cmdend[1];
	cmdend[0] = '\n';
	cmdend[1] = 0;
	(((string_t *)pr_globals)[OFS_PARM0] = PR_TempString(csqcprogs, cmd));
	cmdend[0] = tmp[0];
	cmdend[1] = tmp[1];

	PR_ExecuteProgram (csqcprogs, csqcg.parse_stuffcmd);
	return true;
}
qboolean CSQC_CenterPrint(int lplayernum, char *cmd)
{
	void *pr_globals;
	if (!csqcprogs || !csqcg.parse_centerprint)
		return false;

	CSQC_ChangeLocalPlayer(lplayernum);

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);
	(((string_t *)pr_globals)[OFS_PARM0] = PR_TempString(csqcprogs, cmd));

	PR_ExecuteProgram (csqcprogs, csqcg.parse_centerprint);
	return G_FLOAT(OFS_RETURN);
}

void CSQC_Input_Frame(int lplayernum, usercmd_t *cmd)
{
	if (!csqcprogs || !csqcg.input_frame)
		return;

	CSQC_ChangeLocalPlayer(lplayernum);

	CL_CalcClientTime();
	if (csqcg.svtime)
		*csqcg.svtime = cl.servertime;
	if (csqcg.cltime)
		*csqcg.cltime = cl.time;

	if (csqcg.clientcommandframe)
		*csqcg.clientcommandframe = cls.netchan.outgoing_sequence;

	cs_set_input_state(cmd);
	PR_ExecuteProgram (csqcprogs, csqcg.input_frame);
	cs_get_input_state(cmd);
}

//this protocol allows up to 32767 edicts.
#ifdef PEXT_CSQC
static void CSQC_EntityCheck(int entnum)
{
	int newmax;

	if (entnum >= maxcsqcentities)
	{
		newmax = entnum+64;
		csqcent = BZ_Realloc(csqcent, sizeof(*csqcent)*newmax);
		memset(csqcent + maxcsqcentities, 0, (newmax - maxcsqcentities)*sizeof(csqcent));
		maxcsqcentities = newmax;
	}
}

int CSQC_StartSound(int entnum, int channel, char *soundname, vec3_t pos, float vol, float attenuation)
{
	void *pr_globals;
	csqcedict_t *ent;

	if (!csqcprogs)
		return false;
	if (csqcg.event_sound)
	{
		pr_globals = PR_globals(csqcprogs, PR_CURRENT);

		G_FLOAT(OFS_PARM0) = entnum;
		G_FLOAT(OFS_PARM1) = channel;
		G_INT(OFS_PARM2) = PR_TempString(csqcprogs, soundname);
		G_FLOAT(OFS_PARM3) = vol;
		G_FLOAT(OFS_PARM4) = attenuation;
		VectorCopy(pos, G_VECTOR(OFS_PARM5));

		PR_ExecuteProgram(csqcprogs, csqcg.event_sound);

		return G_FLOAT(OFS_RETURN);
	}
	else if (csqcg.serversound)
	{
		CSQC_EntityCheck(entnum);
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
	return false;
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

	CL_CalcClientTime();
	if (csqcg.svtime)		//estimated server time
		*csqcg.svtime = cl.servertime;
	if (csqcg.cltime)	//smooth client time.
		*csqcg.cltime = cl.time;

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

			CSQC_EntityCheck(entnum);

			if (cl_csqcdebug.ival)
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
			CSQC_EntityCheck(entnum);

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
				ent->xv->entnum = entnum;
				G_FLOAT(OFS_PARM0) = true;

				if (cl_csqcdebug.ival)
					Con_Printf("Add %i\n", entnum);
			}
			else
			{
				G_FLOAT(OFS_PARM0) = false;
				if (cl_csqcdebug.ival)
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
