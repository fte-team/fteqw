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
// world.h

typedef struct plane_s
{
	vec3_t	normal;
	float	dist;
} plane_t;

typedef struct csurface_s
{
	char		name[16];
	int			flags;
	int			value;
} q2csurface_t;

typedef struct cplane_s
{
	vec3_t	normal;
	float	dist;
	qbyte	type;			// for fast side tests
	qbyte	signbits;		// signx + (signy<<1) + (signz<<1)
	qbyte	pad[2];
} cplane_t;

/*
typedef struct trace_s
{
	qboolean	allsolid;	// if true, plane is not valid
	qboolean	startsolid;	// if true, the initial point was in a solid area
	qboolean	inopen, inwater;
	float	fraction;		// time completed, 1.0 = didn't hit anything
	vec3_t	endpos;			// final position
	plane_t	plane;			// surface normal at impact
	edict_t	*ent;			// entity the surface is on
} trace_t;
*/

//these two structures must match (except for extra qw members)
typedef struct trace_s
{
//DON'T ADD ANYTHING BETWEEN THIS LINE
//q2 game dll code will memcpy the lot from trace_t to q2trace_t.
	qboolean	allsolid;	// if true, plane is not valid
	qboolean	startsolid;	// if true, the initial point was in a solid area
	float		fraction;	// time completed, 1.0 = didn't hit anything
	vec3_t		endpos;		// final position
	cplane_t	plane;		// surface normal at impact
	q2csurface_t	*surface;	// surface hit
	int			contents;	// contents on other side of surface hit
	void		*ent;		// not set by CM_*() functions
//AND THIS LINE
	int entnum;

	qboolean	inopen, inwater;
} trace_t;

typedef struct q2trace_s
{
	qboolean	allsolid;	// if true, plane is not valid
	qboolean	startsolid;	// if true, the initial point was in a solid area
	float		fraction;	// time completed, 1.0 = didn't hit anything
	vec3_t		endpos;		// final position
	cplane_t	plane;		// surface normal at impact
	q2csurface_t	*surface;	// surface hit
	int			contents;	// contents on other side of surface hit
	void	*ent;		// not set by CM_*() functions
} q2trace_t;

#define	MOVE_NORMAL		0
#define	MOVE_NOMONSTERS	1
#define	MOVE_MISSILE	2
#define	MOVE_HITMODEL	4
#define MOVE_RESERVED	8	//so we are less likly to get into tricky situations when we want to steal annother future DP extension.
#define MOVE_TRIGGERS	16	//triggers must be marked with FINDABLE_NONSOLID	(an alternative to solid-corpse)
#define MOVE_EVERYTHING	32	//can return triggers and non-solid items if they're marked with FINDABLE_NONSOLID (works even if the items are not properly linked)
#define MOVE_LAGGED		64	//trace touches current last-known-state, instead of actual ents (just affects players for now)
#define MOVE_ENTCHAIN	128 //chain of impacted ents, otherwise result shows only world

typedef struct areanode_s
{
	int		axis;		// -1 = leaf node
	float	dist;
	struct areanode_s	*children[2];
	link_t	edicts;
} areanode_t;

#define	AREA_DEPTH	4
#define	AREA_NODES	32 //pow(2, AREA_DEPTH+1)
#define	EDICT_FROM_AREA(l) STRUCT_FROM_LINK(l,wedict_t,area)

typedef struct wedict_s wedict_t;
#define PROG_TO_WEDICT (wedict_t*)PROG_TO_EDICT
#define WEDICT_NUM (wedict_t *)EDICT_NUM

typedef struct
{
	qboolean present;
	vec3_t laggedpos;
} laggedentinfo_t;

struct world_s
{
	void (*Event_Touch)(struct world_s *w, wedict_t *s, wedict_t *o);
	void (*Event_Think)(struct world_s *w, wedict_t *s);
	void (*Event_Sound) (wedict_t *entity, int channel, char *sample, int volume, float attenuation, int pitchadj);
	model_t *(*Get_CModel)(struct world_s *w, int modelindex);
	void (*Get_FrameState)(struct world_s *w, wedict_t *s, framestate_t *fstate);

	unsigned int	max_edicts;	//limiting factor... 1024 fields*4*MAX_EDICTS == a heck of a lot.
	unsigned int	num_edicts;			// increases towards MAX_EDICTS
/*FTE_DEPRECATED*/	unsigned int	edict_size; //still used in copyentity
	wedict_t		*edicts;			// can NOT be array indexed.
	struct progfuncs_s *progs;
	qboolean		usesolidcorpse;
	model_t			*worldmodel;
	areanode_t	areanodes[AREA_NODES];
	int			numareanodes;

	double		physicstime;
	int			lastcheck;			// used by PF_checkclient
	double		lastchecktime;		// for monster ai

	float lagentsfrac;
	laggedentinfo_t *lagents;
	unsigned int maxlagents;

	struct {
		int     *self;
		int     *other;
		int     *newmis;
		float	*time;
		float	*frametime;
		float	*force_retouch;
		float	*physics_mode;
		float	*v_forward;
		float	*v_right;
		float	*v_up;
	} g;

#ifdef USEODE
	worldode_t ode;
#endif
};
typedef struct world_s world_t;

#ifdef USEODE
void World_ODE_RemoveFromEntity(world_t *world, wedict_t *ed);
void World_ODE_RemoveJointFromEntity(world_t *world, wedict_t *ed);
void World_ODE_Frame(world_t *world, double frametime, double gravity);
void World_ODE_Init(void);
void World_ODE_Start(world_t *world);
void World_ODE_End(world_t *world);
void World_ODE_Shutdown(void);
#endif

void World_ClearWorld (world_t *w);
// called after the world model has been loaded, before linking any entities

void World_UnlinkEdict (wedict_t *ent);
// call before removing an entity, and before trying to move one,
// so it doesn't clip against itself
// flags ent->v.modified

void World_LinkEdict (world_t *w, wedict_t *ent, qboolean touch_triggers);
// Needs to be called any time an entity changes origin, mins, maxs, or solid
// flags ent->v.modified
// sets ent->v.absmin and ent->v.absmax
// if touchtriggers, calls prog functions for the intersected triggers

void World_TouchLinks (world_t *w, wedict_t *ent, areanode_t *node);

int World_PointContents (world_t *w, vec3_t p);
// returns the CONTENTS_* value from the world at the given point.
// does not check any entities at all

wedict_t	*World_TestEntityPosition (world_t *w, wedict_t *ent);

/*
 World_Move:
 mins and maxs are reletive

 if the entire move stays in a solid volume, trace.allsolid will be set

 if the starting point is in a solid, it will be allowed to move out
 to an open area

 nomonsters is used for line of sight or edge testing, where mosnters
 shouldn't be considered solid objects

 passedict is explicitly excluded from clipping checks (normally NULL)
*/
trace_t World_Move (world_t *w, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, wedict_t *passedict);


#ifdef Q2SERVER
typedef struct q2edict_s q2edict_t;

void VARGS WorldQ2_LinkEdict(world_t *w, q2edict_t *ent);
void VARGS WorldQ2_UnlinkEdict(world_t *w, q2edict_t *ent);
int VARGS WorldQ2_AreaEdicts (world_t *w, vec3_t mins, vec3_t maxs, q2edict_t **list,
	int maxcount, int areatype);
trace_t WorldQ2_Move (world_t *w, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, q2edict_t *passedict);

unsigned int Q2BSP_FatPVS (model_t *mod, vec3_t org, qbyte *buffer, unsigned int buffersize, qboolean add);
qboolean Q2BSP_EdictInFatPVS(model_t *mod, struct pvscache_s *ent, qbyte *pvs);
void Q2BSP_FindTouchedLeafs(model_t *mod, struct pvscache_s *ent, float *mins, float *maxs);

#endif


/*sv_move.c*/
qboolean World_CheckBottom (world_t *world, wedict_t *ent);
qboolean World_movestep (world_t *world, wedict_t *ent, vec3_t move, qboolean relink, qboolean noenemy, void (*set_move_trace)(trace_t *trace, struct globalvars_s *pr_globals), struct globalvars_s *set_trace_globs);
qboolean World_MoveToGoal (world_t *world, wedict_t *ent, float dist);

