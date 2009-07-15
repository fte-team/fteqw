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

/*
The aim of this particle system is to have as much as possible configurable.
Some parts still fail here, and are marked FIXME
Effects are flushed on new maps.
The engine has a few builtins.
*/

#include "quakedef.h"

#ifdef PSET_SCRIPT

#ifdef SWQUAKE
#include "r_local.h"
#endif
#ifdef RGLQUAKE
#include "glquake.h"//hack
#endif

#ifdef D3DQUAKE
//d3d is awkward
//we can't include two versions of header files
extern void *d3dexplosiontexture;
extern void *d3dballtexture;
#endif

#include "renderque.h"

#include "r_partset.h"

extern qbyte *host_basepal;

static int pt_pointfile = P_INVALID;
static int pe_default = P_INVALID;
static int pe_size2 = P_INVALID;
static int pe_size3 = P_INVALID;
static int pe_defaulttrail = P_INVALID;

static float psintable[64];

static void buildsintable(void)
{
	int i;
	for (i = 0; i < 64; i++)
		psintable[i] = sin((i*M_PI)/32);
}
#define sin(x) (psintable[(int)(x*(64/M_PI)) & 63])
#define cos(x) (psintable[((int)(x*(64/M_PI)) + 16) & 63])


// !!! if this is changed, it must be changed in d_ifacea.h too !!!
typedef struct particle_s
{
	struct particle_s	*next;
	float		die;

// driver-usable fields
	vec3_t		org;
	float		color;	//used by sw renderer. To be removed.
	vec3_t		rgb;
	float		alpha;
	float		scale;
	float		s1, t1, s2, t2;

	vec3_t		vel;	//renderer uses for sparks
	float		angle;
	union {
		float nextemit;
		trailstate_t *trailstate;
	} state;
// drivers never touch the following fields
	float		rotationspeed;
} particle_t;

typedef struct clippeddecal_s
{
	struct clippeddecal_s	*next;
	float		die;

	vec3_t		center;
	vec3_t		vertex[3];
	vec2_t		texcoords[3];

	vec3_t		rgb;
	float		alpha;
} clippeddecal_t;

#define BS_LASTSEG 0x1 // no draw to next, no delete
#define BS_DEAD    0x2 // segment is dead
#define BS_NODRAW  0x4 // only used for lerp switching

typedef struct beamseg_s
{
	struct beamseg_s *next;  // next in beamseg list

	particle_t *p; 
	int    flags;            // flags for beamseg
	vec3_t dir;

	float texture_s;
} beamseg_t;



typedef struct skytris_s {
	struct skytris_s *next;
	vec3_t org;
	vec3_t x;
	vec3_t y;
	float area;
	float nexttime;
	struct msurface_s *face;
} skytris_t;

//these is the required render state for each particle
//dynamic per-particle stuff isn't important. only static state.
typedef struct {
	enum {PT_NORMAL, PT_SPARK, PT_SPARKFAN, PT_TEXTUREDSPARK, PT_BEAM, PT_DECAL} type;

	blendmode_t blendmode;

	int texturenum;
#ifdef D3DQUAKE
	void *d3dtexture;
#endif

	float scalefactor;
	float invscalefactor;
} plooks_t;

//these could be deltas or absolutes depending on ramping mode.
typedef struct {
	vec3_t rgb;
	float alpha;
	float scale;
	float rotation;
} ramp_t;
// TODO: merge in alpha with rgb to gain benefit of vector opts
typedef struct part_type_s {
	char name[MAX_QPATH];
	char texname[MAX_QPATH];
	vec3_t rgb;
	float alpha;
	vec3_t rgbchange;
	float alphachange;
	vec3_t rgbrand;
	int colorindex;
	int colorrand;
	float rgbchangetime;
	vec3_t rgbrandsync;
	float scale;
	float die, randdie;
	float randomvel, veladd;
	float orgadd;
	float offsetspread;
	float offsetspreadvert;
	float randomvelvert;
	float randscale;
	
	float s1, t1, s2, t2;
	float texsstride;	//addition for s for each random slot.
	int randsmax;	//max times the stride can be added

	plooks_t *slooks;	//shared looks, so state switches don't apply between particles so much
	plooks_t looks;

	float spawntime;
	float spawnchance;

	float rotationstartmin, rotationstartrand;
	float rotationmin, rotationrand;

	float scaledelta;
	float count;
	float countrand;

	int assoc;
	int cliptype;
	int inwater;
	float clipcount;
	int emit;
	float emittime;
	float emitrand;
	float emitstart;

	float areaspread;
	float areaspreadvert;

	float spawnparam1;
	float spawnparam2;
/*	float spawnparam3; */

	float offsetup; // make this into a vec3_t later with dir, possibly for mdls

	enum {
		SM_BOX, //box = even spread within the area
		SM_CIRCLE, //circle = around edge of a circle
		SM_BALL, //ball = filled sphere
		SM_SPIRAL, //spiral = spiral trail
		SM_TRACER, //tracer = tracer trail
		SM_TELEBOX, //telebox = q1-style telebox
		SM_LAVASPLASH, //lavasplash = q1-style lavasplash
		SM_UNICIRCLE, //unicircle = uniform circle
		SM_FIELD, //field = synced field (brightfield, etc)
		SM_DISTBALL // uneven distributed ball
	} spawnmode;

	float gravity;
	vec3_t friction;
	float clipbounce;
	int stains;

	enum {RAMP_NONE, RAMP_DELTA, RAMP_ABSOLUTE} rampmode;
	int rampindexes;
	ramp_t *ramp;

	int loaded;
	particle_t	*particles;
	clippeddecal_t *clippeddecals;
	beamseg_t *beams;
	skytris_t *skytris;
	struct part_type_s *nexttorun;

	unsigned int flags;
#define PT_VELOCITY	     0x001
#define PT_FRICTION	     0x002
#define PT_CHANGESCOLOUR 0x004
#define PT_CITRACER      0x008 // Q1-style tracer behavior for colorindex
#define PT_INVFRAMETIME  0x010 // apply inverse frametime to count (causes emits to be per frame)
#define PT_AVERAGETRAIL  0x020 // average trail points from start to end, useful with t_lightning, etc
#define PT_NOSTATE       0x040 // don't use trailstate for this emitter (careful with assoc...)
#define PT_NOSPREADFIRST 0x080 // don't randomize org/vel for first generated particle
#define PT_NOSPREADLAST  0x100 // don't randomize org/vel for last generated particle
	unsigned int state;
#define PS_INRUNLIST 0x1 // particle type is currently in execution list
} part_type_t;

void PScript_DrawParticleTypes (void (*texturedparticles)(int count, particle_t **,plooks_t*), void (*sparklineparticles)(int count, particle_t **,plooks_t*), void (*sparkfanparticles)(int count, particle_t **,plooks_t*), void (*sparktexturedparticles)(int count, particle_t **,plooks_t*), void (*beamparticlest)(int count, beamseg_t**,plooks_t*), void (*beamparticlesut)(int count, beamseg_t**,plooks_t*), void (*drawdecalparticles)(int count, clippeddecal_t**,plooks_t*));

#ifndef TYPESONLY

//triangle fan sparks use these.
static double sint[7] = {0.000000, 0.781832,  0.974928,  0.433884, -0.433884, -0.974928, -0.781832};
static double cost[7] = {1.000000, 0.623490, -0.222521, -0.900969, -0.900969, -0.222521,  0.623490};

#define crand() (rand()%32767/16383.5f-1)

void D_DrawParticleTrans (vec3_t porg, float palpha, float pscale, unsigned int pcolour, blendmode_t blendmode);

static void P_ReadPointFile_f (void);
static void P_ExportBuiltinSet_f(void);

#define MAX_BEAMSEGS             2048   // default max # of beam segments
#define MAX_PARTICLES			32768	// default max # of particles at one
										//  time
#define MAX_DECALS				 4096	// this is going to be expensive
#define MAX_TRAILSTATES           512   // default max # of trailstates

//int		ramp1[8] = {0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61};
//int		ramp2[8] = {0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66};
//int		ramp3[8] = {0x6d, 0x6b, 6,	  5,    4,    3,    2,    1};

particle_t	*free_particles;
particle_t	*particles;	//contains the initial list of alloced particles.
int			r_numparticles;

beamseg_t   *free_beams;
beamseg_t   *beams;
int			r_numbeams;

clippeddecal_t	*free_decals;
clippeddecal_t	*decals;
int			r_numdecals;

trailstate_t *trailstates;
int			ts_cycle; // current cyclic index of trailstates
int			r_numtrailstates;

static		qboolean r_plooksdirty;	//a particle effect was changed, reevaluate shared looks.

extern cvar_t r_bouncysparks;
extern cvar_t r_part_rain;
extern cvar_t r_bloodstains;
extern cvar_t gl_part_flame;

// callbacks
static void R_ParticlesDesc_Callback(struct cvar_s *var, char *oldvalue);

extern cvar_t r_particlesdesc;
extern cvar_t r_part_rain_quantity;
extern cvar_t r_particle_tracelimit;
extern cvar_t r_part_sparks;
extern cvar_t r_part_sparks_trifan;
extern cvar_t r_part_sparks_textured;
extern cvar_t r_part_beams;
extern cvar_t r_part_beams_textured;
extern cvar_t r_part_contentswitch;

static float particletime;

#define APPLYBLEND(bm)	\
		switch (bm)												\
		{														\
		case BM_ADD:											\
			qglBlendFunc(GL_SRC_ALPHA, GL_ONE);					\
			break;												\
		case BM_SUBTRACT:										\
			qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR);	\
			break;												\
		case BM_BLENDCOLOUR:										\
			qglBlendFunc(GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);	\
			break;												\
		case BM_BLEND:											\
		default:												\
			qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);	\
			break;												\
		}

static int numparticletypes;
static part_type_t *part_type;
static part_type_t *part_run_list;

static struct {
	char *oldn;
	char *newn;
} legacynames[] = 
{
	{"t_rocket", "TR_ROCKET"},
	{"te_explosion", "TE_EXPLOSION"},

	{"t_blastertrail", "TR_BLASTERTRAIL"},
	{"t_rocket", "TR_ROCKET"},
	{"t_grenade", "TR_GRENADE"},
	{"t_gib", "TR_BLOOD"},

	{"te_plasma", "TE_TEI_PLASMAHIT"},
	{"te_smoke", "TE_TEI_SMOKE"},

	{NULL}
};

static part_type_t *P_GetParticleType(char *name)
{
	int i;
	part_type_t *ptype;
	part_type_t *oldlist = part_type;
	for (i = 0; legacynames[i].oldn; i++)
	{
		if (!strcmp(name, legacynames[i].oldn))
		{
			name = legacynames[i].newn;
			break;
		}
	}
	for (i = 0; i < numparticletypes; i++)
	{
		ptype = &part_type[i];
		if (!stricmp(ptype->name, name))
			return ptype;
	}
	part_type = BZ_Realloc(part_type, sizeof(part_type_t)*(numparticletypes+1));
	ptype = &part_type[numparticletypes++];
	memset(ptype, 0, sizeof(*ptype));
	strcpy(ptype->name, name);
	ptype->assoc=P_INVALID;
	ptype->cliptype = P_INVALID;
	ptype->emit = P_INVALID;

	if (oldlist)
	{
		part_run_list=NULL;

		for (i = 0; i < numparticletypes; i++)
			if (part_type[i].nexttorun)
				part_type[i].nexttorun = (part_type_t*)((char*)part_type[i].nexttorun - (char*)oldlist + (char*)part_type);
	}

	ptype->loaded = 0;
	ptype->ramp = NULL;
	ptype->particles = NULL;
	ptype->beams = NULL;

	r_plooksdirty = true;
	return ptype;
}

static int P_AllocateParticleType(char *name)	//guarentees that the particle type exists, returning it's index.
{
	part_type_t *pt = P_GetParticleType(name);
	return pt - part_type;
}

static int PScript_ParticleTypeForName(char *name)
{
	int to;

	to = P_GetParticleType(name) - part_type;
	if (to < 0 || to >= numparticletypes)
	{
		return P_INVALID;
	}

	return to;
}

static int PScript_FindParticleType(char *name)
{
	int i;
	part_type_t *ptype = NULL;

	for (i = 0; legacynames[i].oldn; i++)
	{
		if (!strcmp(name, legacynames[i].oldn))
		{
			name = legacynames[i].newn;
			break;
		}
	}

	for (i = 0; i < numparticletypes; i++)
	{
		if (!stricmp(part_type[i].name, name))
		{
			ptype = &part_type[i];
			break;
		}
	}
	if (!ptype)
		return P_INVALID;
	if (!ptype->loaded)
		return P_INVALID;
	return i;
}

static void P_SetModified(void)	//called when the particle system changes (from console).
{
	if (Cmd_FromGamecode())
		return;	//server stuffed particle descriptions don't count.

	f_modified_particles = true;

	if (care_f_modified)
	{
		care_f_modified = false;
		Cbuf_AddText("say particles description has changed\n", RESTRICT_LOCAL);
	}
}
static int CheckAssosiation(char *name, int from)
{
	int to, orig;

	orig = to = P_AllocateParticleType(name);

	while(to != P_INVALID)
	{
		if (to == from)
		{
			Con_Printf("Assosiation of %s would cause infinate loop\n", name);
			return P_INVALID;
		}
		to = part_type[to].assoc;
	}
	return orig;
}

static void P_LoadTexture(part_type_t *ptype, qboolean warn)
{
	switch (qrenderer)
	{
#ifdef RGLQUAKE
	case QR_OPENGL:
		if (*ptype->texname && strcmp(ptype->texname, "default"))
		{
			ptype->looks.texturenum = Mod_LoadHiResTexture(ptype->texname, "particles", true, true, true);

			if (!ptype->looks.texturenum)
			{
				//note that this could get messy if you depend upon vid_restart to reload your effect without re-execing it after.
				ptype->s1 = 0;
				ptype->t1 = 0;
				ptype->s2 = 1;
				ptype->t2 = 1;
				ptype->randsmax = 1;

				if (warn)
					Con_DPrintf("Couldn't load texture %s for particle effect %s\n", ptype->texname, ptype->name);

				if (strstr(ptype->texname, "glow") || strstr(ptype->texname, "ball") || ptype->looks.type == PT_TEXTUREDSPARK)
					ptype->looks.texturenum = balltexture;
				else
					ptype->looks.texturenum = explosiontexture;
			}
		}
		else
			ptype->looks.texturenum = explosiontexture;
		break;
#endif
#ifdef D3DQUAKE
	case QR_DIRECT3D:
		if (*ptype->texname && strcmp(ptype->texname, "default"))
		{
			ptype->looks.d3dtexture = NULL;//Mod_LoadHiResTexture(ptype->texname, "particles", true, true, true);

			if (!ptype->looks.d3dtexture)
			{
				if (warn)
					Con_DPrintf("Couldn't load texture %s for particle effect %s\n", ptype->texname, ptype->name);

				if (strstr(ptype->texname, "glow") || strstr(ptype->texname, "ball"))
					ptype->looks.d3dtexture = d3dballtexture;
				else
					ptype->looks.d3dtexture = d3dexplosiontexture;
			}
		}
		else
			ptype->looks.d3dtexture = d3dexplosiontexture;
		break;
#endif
	default:
		break;
	}
}

//Uses FTE's multiline console stuff.
//This is the function that loads the effect descriptions (via console).
static void P_ParticleEffect_f(void)
{
	char *var, *value;
	char *buf;
	particle_t *parts;
	beamseg_t *beamsegs;
	skytris_t *st;
	qboolean settype = false;
	qboolean setalphadelta = false;
	qboolean setbeamlen = false;

	part_type_t *ptype, *torun;
	char tnamebuf[sizeof(ptype->name)];
	int pnum, assoc;

	if (Cmd_Argc()!=2)
	{
		Con_Printf("No name for particle effect\n");
		return;
	}

	buf = Cbuf_GetNext(Cmd_ExecLevel);
	while (*buf && *buf <= ' ')
		buf++;	//no whitespace please.
	if (*buf != '{')
	{
		Cbuf_InsertText(buf, Cmd_ExecLevel, true);
		Con_Printf("This is a multiline command and should be used within config files\n");
		return;
	}

	ptype = P_GetParticleType(Cmd_Argv(1));
	if (!ptype)
	{
		Con_Printf("Bad name\n");
		return;
	}

	P_SetModified();

	pnum = ptype-part_type;

	st = ptype->skytris;
	if (ptype->ramp)
		BZ_Free(ptype->ramp);

	while (ptype->particles) // empty particle list
	{
		parts = ptype->particles->next;
		ptype->particles->next = free_particles;
		free_particles = ptype->particles;
		ptype->particles = parts;
	}

	// go with a lazy clear of list.. mark everything as DEAD and let
	// the beam rendering handle removing nodes
	beamsegs = ptype->beams;
	while (beamsegs)
	{
		beamsegs->flags |= BS_DEAD;
		beamsegs = beamsegs->next;
	}

	beamsegs = ptype->beams;

	// if we're in the runstate loop through and remove from linked list
	if (ptype->state & PS_INRUNLIST)
	{
		if (part_run_list == ptype)
			part_run_list = part_run_list->nexttorun;
		else
		{
			for (torun = part_run_list; torun != NULL; torun = torun->nexttorun)
			{
				if (torun->nexttorun == ptype)
					torun->nexttorun = torun->nexttorun->nexttorun;
			}
		}
	}

	strcpy(tnamebuf, ptype->name);
	memset(ptype, 0, sizeof(*ptype));
//	ptype->particles = parts;
	ptype->beams = beamsegs;
	ptype->skytris = st;
	strcpy(ptype->name, tnamebuf);
	ptype->assoc=P_INVALID;
	ptype->inwater = P_INVALID;
	ptype->cliptype = P_INVALID;
	ptype->emit = P_INVALID;
	ptype->alpha = 1;
	ptype->alphachange = 1;
	ptype->clipbounce = 0.8;
	ptype->colorindex = -1;
	ptype->rotationstartmin = -M_PI;	//start with a random angle
	ptype->rotationstartrand = M_PI-ptype->rotationstartmin;
	ptype->spawnchance = 1;

	ptype->randsmax = 1;
	ptype->s2 = 1;
	ptype->t2 = 1;

	while(1)
	{
		buf = Cbuf_GetNext(Cmd_ExecLevel);
		if (!*buf)
		{
			Con_Printf("Unexpected end of buffer with effect %s\n", ptype->name);
			return;
		}

		while (*buf && *buf <= ' ')
			buf++;	//no whitespace please.
		if (*buf == '}')
			break;

		Cmd_TokenizeString(buf, true, true);
		var = Cmd_Argv(0);
		value = Cmd_Argv(1);

		// TODO: switch this mess to some sort of binary tree to increase
		// parse speed
		if (!strcmp(var, "texture"))
			Q_strncpyz(ptype->texname, value, sizeof(ptype->texname));
		else if (!strcmp(var, "tcoords"))
		{
			float tscale;

			tscale = atof(Cmd_Argv(5));
			if (tscale < 0)
				tscale = 1;

			ptype->s1 = atof(value)/tscale;
			ptype->t1 = atof(Cmd_Argv(2))/tscale;
			ptype->s2 = atof(Cmd_Argv(3))/tscale;
			ptype->t2 = atof(Cmd_Argv(4))/tscale;

			ptype->randsmax = atoi(Cmd_Argv(6));
			ptype->texsstride = atof(Cmd_Argv(7));

			if (ptype->randsmax < 1 || ptype->texsstride == 0)
				ptype->randsmax = 1;
		}
		else if (!strcmp(var, "rotationstart"))
		{
			ptype->rotationstartmin = atof(value)*M_PI/180;
			if (Cmd_Argc()>2)
				ptype->rotationstartrand = atof(Cmd_Argv(2))*M_PI/180-ptype->rotationstartmin;
			else
				ptype->rotationstartrand = 0;
		}
		else if (!strcmp(var, "rotationspeed"))
		{
			ptype->rotationmin = atof(value)*M_PI/180;
			if (Cmd_Argc()>2)
				ptype->rotationrand = atof(Cmd_Argv(2))*M_PI/180-ptype->rotationmin;
			else
				ptype->rotationrand = 0;
		}
		else if (!strcmp(var, "beamtexstep"))
		{
			ptype->rotationstartmin = 1/atof(value);
			setbeamlen = true;
		}
		else if (!strcmp(var, "beamtexspeed"))
		{
			ptype->rotationmin = atof(value);
		}
		else if (!strcmp(var, "scale"))
		{
			ptype->scale = atof(value);
			if (Cmd_Argc()>2)
				ptype->randscale = atof(Cmd_Argv(2)) - ptype->scale;
		}
		else if (!strcmp(var, "scalerand"))
			ptype->randscale = atof(value);

		else if (!strcmp(var, "scalefactor"))
			ptype->looks.scalefactor = atof(value);
		else if (!strcmp(var, "scaledelta"))
			ptype->scaledelta = atof(value);


		else if (!strcmp(var, "step"))
		{
			ptype->count = 1/atof(value);
			if (Cmd_Argc()>2)
				ptype->countrand = 1/atof(Cmd_Argv(2));
		}
		else if (!strcmp(var, "count"))
		{
			ptype->count = atof(value);
			if (Cmd_Argc()>2)
				ptype->countrand = atof(Cmd_Argv(2));
		}

		else if (!strcmp(var, "alpha"))
			ptype->alpha = atof(value);
		else if (!strcmp(var, "alphachange"))
		{
			Con_DPrintf("alphachange is deprechiated, use alphadelta\n");
			ptype->alphachange = atof(value);
		}
		else if (!strcmp(var, "alphadelta"))
		{
			ptype->alphachange = atof(value);
			setalphadelta = true;
		}
		else if (!strcmp(var, "die"))
			ptype->die = atof(value);
		else if (!strcmp(var, "diesubrand"))
			ptype->randdie = atof(value);

		else if (!strcmp(var, "randomvel"))
		{
			ptype->randomvel = atof(value);
			if (Cmd_Argc()>2)
				ptype->randomvelvert = atof(Cmd_Argv(2));
			else
				ptype->randomvelvert = ptype->randomvel;
		}
		else if (!strcmp(var, "veladd"))
			ptype->veladd = atof(value);
		else if (!strcmp(var, "orgadd"))
			ptype->orgadd = atof(value);
		else if (!strcmp(var, "friction"))
		{
			ptype->friction[2] = ptype->friction[1] = ptype->friction[0] = atof(value);

			if (Cmd_Argc()>3)
			{
				ptype->friction[2] = atof(Cmd_Argv(3));
				ptype->friction[1] = atof(Cmd_Argv(2));
			}
			else if (Cmd_Argc()>2)
			{
				ptype->friction[2] = atof(Cmd_Argv(2));
			}
		}
		else if (!strcmp(var, "gravity"))
			ptype->gravity = atof(value);
		else if (!strcmp(var, "clipbounce"))
			ptype->clipbounce = atof(value);

		else if (!strcmp(var, "assoc"))
		{
			assoc = CheckAssosiation(value, pnum);	//careful - this can realloc all the particle types
			ptype = &part_type[pnum];
			ptype->assoc = assoc;
		}
		else if (!strcmp(var, "inwater"))
		{
			// the underwater effect switch should only occur for
			// 1 level so the standard assoc check works
			assoc = CheckAssosiation(value, pnum);
			ptype = &part_type[pnum];
			ptype->inwater = assoc;
		}
		else if (!strcmp(var, "colorindex"))
		{
			if (Cmd_Argc()>2)
				ptype->colorrand = atof(Cmd_Argv(2));
			ptype->colorindex = atoi(value);
		}
		else if (!strcmp(var, "colorrand"))
			ptype->colorrand = atoi(value); // now obsolete
		else if (!strcmp(var, "citracer"))
			ptype->flags |= PT_CITRACER;

		else if (!strcmp(var, "red"))
			ptype->rgb[0] = atof(value)/255;
		else if (!strcmp(var, "green"))
			ptype->rgb[1] = atof(value)/255;
		else if (!strcmp(var, "blue"))
			ptype->rgb[2] = atof(value)/255;
		else if (!strcmp(var, "rgb"))
		{
			ptype->rgb[0] = ptype->rgb[1] = ptype->rgb[2] = atof(value)/255;
			if (Cmd_Argc()>3)
			{
				ptype->rgb[1] = atof(Cmd_Argv(2))/255;
				ptype->rgb[2] = atof(Cmd_Argv(3))/255;
			}
		}

		else if (!strcmp(var, "reddelta"))
		{
			ptype->rgbchange[0] = atof(value)/255;
			if (!ptype->rgbchangetime)
				ptype->rgbchangetime = ptype->die;
		}
		else if (!strcmp(var, "greendelta"))
		{
			ptype->rgbchange[1] = atof(value)/255;
			if (!ptype->rgbchangetime)
				ptype->rgbchangetime = ptype->die;
		}
		else if (!strcmp(var, "bluedelta"))
		{
			ptype->rgbchange[2] = atof(value)/255;
			if (!ptype->rgbchangetime)
				ptype->rgbchangetime = ptype->die;
		}
		else if (!strcmp(var, "rgbdelta"))
		{
			ptype->rgbchange[0] = ptype->rgbchange[1] = ptype->rgbchange[2] = atof(value)/255;
			if (Cmd_Argc()>3)
			{
				ptype->rgbchange[1] = atof(Cmd_Argv(2))/255;
				ptype->rgbchange[2] = atof(Cmd_Argv(3))/255;
			}
			if (!ptype->rgbchangetime)
				ptype->rgbchangetime = ptype->die;
		}
		else if (!strcmp(var, "rgbdeltatime"))
			ptype->rgbchangetime = atof(value);

		else if (!strcmp(var, "redrand"))
			ptype->rgbrand[0] = atof(value)/255;
		else if (!strcmp(var, "greenrand"))
			ptype->rgbrand[1] = atof(value)/255;
		else if (!strcmp(var, "bluerand"))
			ptype->rgbrand[2] = atof(value)/255;
		else if (!strcmp(var, "rgbrand"))
		{
			ptype->rgbrand[0] = ptype->rgbrand[1] = ptype->rgbrand[2] = atof(value)/255;
			if (Cmd_Argc()>3)
			{
				ptype->rgbrand[1] = atof(Cmd_Argv(2))/255;
				ptype->rgbrand[2] = atof(Cmd_Argv(3))/255;
			}
		}

		else if (!strcmp(var, "rgbrandsync"))
		{
			ptype->rgbrandsync[0] = ptype->rgbrandsync[1] = ptype->rgbrandsync[2] = atof(value);
			if (Cmd_Argc()>3)
			{
				ptype->rgbrandsync[1] = atof(Cmd_Argv(2));
				ptype->rgbrandsync[2] = atof(Cmd_Argv(3));
			}
		}
		else if (!strcmp(var, "redrandsync"))
			ptype->rgbrandsync[0] = atof(value);
		else if (!strcmp(var, "greenrandsync"))
			ptype->rgbrandsync[1] = atof(value);
		else if (!strcmp(var, "bluerandsync"))
			ptype->rgbrandsync[2] = atof(value);

		else if (!strcmp(var, "stains"))
			ptype->stains = atoi(value);
		else if (!strcmp(var, "blend"))
		{
			if (!strcmp(value, "add"))
				ptype->looks.blendmode = BM_ADD;
			else if (!strcmp(value, "subtract"))
				ptype->looks.blendmode = BM_SUBTRACT;
			else if (!strcmp(value, "blendcolour") || !strcmp(value, "blendcolor"))
				ptype->looks.blendmode = BM_BLENDCOLOUR;
			else
				ptype->looks.blendmode = BM_BLEND;
		}
		else if (!strcmp(var, "spawnmode"))
		{
			if (!strcmp(value, "circle"))
				ptype->spawnmode = SM_CIRCLE;
			else if (!strcmp(value, "ball"))
				ptype->spawnmode = SM_BALL;
			else if (!strcmp(value, "spiral"))
				ptype->spawnmode = SM_SPIRAL;
			else if (!strcmp(value, "tracer"))
				ptype->spawnmode = SM_TRACER;
			else if (!strcmp(value, "telebox"))
				ptype->spawnmode = SM_TELEBOX;
			else if (!strcmp(value, "lavasplash"))
				ptype->spawnmode = SM_LAVASPLASH;
			else if (!strcmp(value, "uniformcircle"))
				ptype->spawnmode = SM_UNICIRCLE;
			else if (!strcmp(value, "syncfield"))
				ptype->spawnmode = SM_FIELD;
			else if (!strcmp(value, "distball"))
				ptype->spawnmode = SM_DISTBALL;
			else
				ptype->spawnmode = SM_BOX;

			if (Cmd_Argc()>2)
			{
				if (Cmd_Argc()>3)
					ptype->spawnparam2 = atof(Cmd_Argv(3));
				ptype->spawnparam1 = atof(Cmd_Argv(2));
			}
		}
		else if (!strcmp(var, "type"))
		{
			if (!strcmp(value, "beam"))
				ptype->looks.type = PT_BEAM;
			else if (!strcmp(value, "spark"))
				ptype->looks.type = PT_SPARK;
			else if (!strcmp(value, "sparkfan") || !strcmp(value, "trianglefan"))
				ptype->looks.type = PT_SPARKFAN;
			else if (!strcmp(value, "texturedspark"))
				ptype->looks.type = PT_TEXTUREDSPARK;
			else if (!strcmp(value, "decal"))
				ptype->looks.type = PT_DECAL;
			else
				ptype->looks.type = PT_NORMAL;
			settype = true;
		}
		else if (!strcmp(var, "isbeam"))
		{
			Con_DPrintf("isbeam is deprechiated, use type beam\n");
			ptype->looks.type = PT_BEAM;
		}
		else if (!strcmp(var, "spawntime"))
			ptype->spawntime = atof(value);
		else if (!strcmp(var, "spawnchance"))
			ptype->spawnchance = atof(value);
		else if (!strcmp(var, "cliptype"))
		{
			assoc = P_ParticleTypeForName(value);//careful - this can realloc all the particle types
			ptype = &part_type[pnum];
			ptype->cliptype = assoc;
		}
		else if (!strcmp(var, "clipcount"))
			ptype->clipcount = atof(value);

		else if (!strcmp(var, "emit"))
		{
			assoc = P_ParticleTypeForName(value);//careful - this can realloc all the particle types
			ptype = &part_type[pnum];
			ptype->emit = assoc;
		}
		else if (!strcmp(var, "emitinterval"))
			ptype->emittime = atof(value);
		else if (!strcmp(var, "emitintervalrand"))
			ptype->emitrand = atof(value);
		else if (!strcmp(var, "emitstart"))
			ptype->emitstart = atof(value);

		// old names
		else if (!strcmp(var, "areaspread"))
		{
			Con_DPrintf("areaspread is deprechiated, use spawnorg\n");
			ptype->areaspread = atof(value);
		}
		else if (!strcmp(var, "areaspreadvert"))
		{
			Con_DPrintf("areaspreadvert is deprechiated, use spawnorg\n");
			ptype->areaspreadvert = atof(value);
		}
		else if (!strcmp(var, "offsetspread"))
		{
			Con_DPrintf("offsetspread is deprechiated, use spawnvel\n");
			ptype->offsetspread = atof(value);
		}
		else if (!strcmp(var, "offsetspreadvert"))
		{
			Con_DPrintf("offsetspreadvert is deprechiated, use spawnvel\n");
			ptype->offsetspreadvert  = atof(value);
		}

		// new names
		else if (!strcmp(var, "spawnorg"))
		{
			ptype->areaspreadvert = ptype->areaspread = atof(value);

			if (Cmd_Argc()>2)
				ptype->areaspreadvert = atof(Cmd_Argv(2));
		}
		else if (!strcmp(var, "spawnvel"))
		{
			ptype->offsetspreadvert = ptype->offsetspread = atof(value);

			if (Cmd_Argc()>2)
				ptype->offsetspreadvert = atof(Cmd_Argv(2));
		}

		// spawn mode param fields
		else if (!strcmp(var, "spawnparam1"))
			ptype->spawnparam1 = atof(value);
		else if (!strcmp(var, "spawnparam2"))
			ptype->spawnparam2 = atof(value);
/*		else if (!strcmp(var, "spawnparam3"))
			ptype->spawnparam3 = atof(value); */

		else if (!strcmp(var, "up"))
			ptype->offsetup = atof(value);
		else if (!strcmp(var, "rampmode"))
		{
			if (!strcmp(value, "none"))
				ptype->rampmode = RAMP_NONE;
			else if (!strcmp(value, "absolute"))
				ptype->rampmode = RAMP_ABSOLUTE;
			else //if (!strcmp(value, "delta"))
				ptype->rampmode = RAMP_DELTA;
		}
		else if (!strcmp(var, "rampindexlist"))
		{ // better not use this with delta ramps...
			int cidx, i;

			i = 1;
			while (i < Cmd_Argc())
			{
				ptype->ramp = BZ_Realloc(ptype->ramp, sizeof(ramp_t)*(ptype->rampindexes+1));

				cidx = atoi(Cmd_Argv(i));
				ptype->ramp[ptype->rampindexes].alpha = cidx > 255 ? 0.5 : 1;

				cidx = (cidx & 0xff) * 3;
				ptype->ramp[ptype->rampindexes].rgb[0] = host_basepal[cidx] * (1/255.0);
				ptype->ramp[ptype->rampindexes].rgb[1] = host_basepal[cidx+1] * (1/255.0);
				ptype->ramp[ptype->rampindexes].rgb[2] = host_basepal[cidx+2] * (1/255.0);

				ptype->ramp[ptype->rampindexes].scale = ptype->scale;

				ptype->rampindexes++;
				i++;
			}
		}
		else if (!strcmp(var, "rampindex"))
		{
			int cidx;
			ptype->ramp = BZ_Realloc(ptype->ramp, sizeof(ramp_t)*(ptype->rampindexes+1));

			cidx = atoi(value);
			ptype->ramp[ptype->rampindexes].alpha = cidx > 255 ? 0.5 : 1;

			if (Cmd_Argc() > 2) // they gave alpha
				ptype->ramp[ptype->rampindexes].alpha *= atof(Cmd_Argv(2));

			cidx = (cidx & 0xff) * 3;
			ptype->ramp[ptype->rampindexes].rgb[0] = host_basepal[cidx] * (1/255.0);
			ptype->ramp[ptype->rampindexes].rgb[1] = host_basepal[cidx+1] * (1/255.0);
			ptype->ramp[ptype->rampindexes].rgb[2] = host_basepal[cidx+2] * (1/255.0);

			if (Cmd_Argc() > 3) // they gave scale
				ptype->ramp[ptype->rampindexes].scale = atof(Cmd_Argv(3));
			else
				ptype->ramp[ptype->rampindexes].scale = ptype->scale;


			ptype->rampindexes++;
		}
		else if (!strcmp(var, "ramp"))
		{
			ptype->ramp = BZ_Realloc(ptype->ramp, sizeof(ramp_t)*(ptype->rampindexes+1));

			ptype->ramp[ptype->rampindexes].rgb[0] = atof(value)/255;
			if (Cmd_Argc()>3)	//seperate rgb
			{
				ptype->ramp[ptype->rampindexes].rgb[1] = atof(Cmd_Argv(2))/255;
				ptype->ramp[ptype->rampindexes].rgb[2] = atof(Cmd_Argv(3))/255;

				if (Cmd_Argc()>4)	//have we alpha and scale changes?
				{
					ptype->ramp[ptype->rampindexes].alpha = atof(Cmd_Argv(4));
					if (Cmd_Argc()>5)	//have we scale changes?
						ptype->ramp[ptype->rampindexes].scale = atof(Cmd_Argv(5));
					else
						ptype->ramp[ptype->rampindexes].scale = ptype->scaledelta;
				}
				else
				{
					ptype->ramp[ptype->rampindexes].alpha = ptype->alpha;
					ptype->ramp[ptype->rampindexes].scale = ptype->scaledelta;
				}
			}
			else	//they only gave one value
			{
				ptype->ramp[ptype->rampindexes].rgb[1] = ptype->ramp[ptype->rampindexes].rgb[0];
				ptype->ramp[ptype->rampindexes].rgb[2] = ptype->ramp[ptype->rampindexes].rgb[0];

				ptype->ramp[ptype->rampindexes].alpha = ptype->alpha;
				ptype->ramp[ptype->rampindexes].scale = ptype->scaledelta;
			}

			ptype->rampindexes++;
		}
		else if (!strcmp(var, "perframe"))
			ptype->flags |= PT_INVFRAMETIME;
		else if (!strcmp(var, "averageout"))
			ptype->flags |= PT_AVERAGETRAIL;
		else if (!strcmp(var, "nostate"))
			ptype->flags |= PT_NOSTATE;
		else if (!strcmp(var, "nospreadfirst"))
			ptype->flags |= PT_NOSPREADFIRST;
		else if (!strcmp(var, "nospreadlast"))
			ptype->flags |= PT_NOSPREADLAST;
		else
			Con_DPrintf("%s is not a recognised particle type field (in %s)\n", var, ptype->name);
	}
	ptype->looks.invscalefactor = 1-ptype->looks.scalefactor;
	ptype->loaded = 1;
	if (ptype->clipcount < 1)
		ptype->clipcount = 1;

	//if there is a chance that it moves
	if (ptype->randomvel || ptype->gravity || ptype->veladd || ptype->offsetspread || ptype->offsetspreadvert)
		ptype->flags |= PT_VELOCITY;
	//if it has friction
	if (ptype->friction)
		ptype->flags |= PT_FRICTION;

	if (!settype)
	{
		if (ptype->looks.type == PT_NORMAL && !*ptype->texname)
			ptype->looks.type = PT_SPARK;
		if (ptype->looks.type == PT_SPARK)
		{
			if (*ptype->texname)
				ptype->looks.type = PT_TEXTUREDSPARK;
			if (ptype->scale)
				ptype->looks.type = PT_SPARKFAN;
		}
	}

	if (ptype->looks.type == PT_BEAM && !setbeamlen)
		ptype->rotationstartmin = 1/128.0;

	// use old behavior if not using alphadelta
	if (!setalphadelta)
		ptype->alphachange = (-ptype->alphachange / ptype->die) * ptype->alpha;

	if (ptype->rampmode && !ptype->ramp)
	{
		ptype->rampmode = RAMP_NONE;
		Con_Printf("Particle type %s has a ramp mode but no ramp\n", ptype->name);
	}
	else if (ptype->ramp && !ptype->rampmode)
	{
		Con_Printf("Particle type %s has a ramp but no ramp mode\n", ptype->name);
	}

	P_LoadTexture(ptype, true);

	r_plooksdirty = true;
}

//assosiate a point effect with a model.
//the effect will be spawned every frame with count*frametime
//has the capability to hide models.
static void P_AssosiateEffect_f (void)
{
	char *modelname = Cmd_Argv(1);
	char *effectname = Cmd_Argv(2);
	int effectnum;
	model_t *model;

	if (!cls.demoplayback && (
		strstr(modelname, "player") ||
		strstr(modelname, "eyes") ||
		strstr(modelname, "flag") ||
		strstr(modelname, "tf_stan") ||
		strstr(modelname, ".bsp") ||
		strstr(modelname, "turr")))
	{
		Con_Printf("Sorry: Not allowed to attach effects to model \"%s\"\n", modelname);
		return;
	}

	model = Mod_FindName(modelname);
	if (!model)
		return;
	if (!cls.demoplayback && (model->flags & EF_ROTATE))
	{
		Con_Printf("Sorry: You may not assosiate effects with item model \"%s\"\n", modelname);
		return;
	}
	effectnum = P_AllocateParticleType(effectname);
	model->particleeffect = effectnum;
	if (atoi(Cmd_Argv(3)))
		model->engineflags |= MDLF_ENGULPHS;

	P_SetModified();	//make it appear in f_modified.
}

//assosiate a particle trail with a model.
//the effect will be spawned between two points when an entity with the model moves.
static void P_AssosiateTrail_f (void)
{
	char *modelname = Cmd_Argv(1);
	char *effectname = Cmd_Argv(2);
	int effectnum;
	model_t *model;

	if (!cls.demoplayback && (
		strstr(modelname, "player") ||
		strstr(modelname, "eyes") ||
		strstr(modelname, "flag") ||
		strstr(modelname, "tf_stan")))
	{
		Con_Printf("Sorry, you can't assosiate trails with model \"%s\"\n", modelname);
		return;
	}

	model = Mod_FindName(modelname);
	if (!model)
		return;
	effectnum = P_AllocateParticleType(effectname);
	model->particletrail = effectnum;
	model->engineflags |= MDLF_NODEFAULTTRAIL;	//we could have assigned the trail to a model that wasn't loaded.

	P_SetModified();	//make it appear in f_modified.
}

#if _DEBUG
// R_BeamInfo_f - debug junk
static void P_BeamInfo_f (void)
{
	beamseg_t *bs;
	int i, j, k, l, m;

	i = 0;

	for (bs = free_beams; bs; bs = bs->next)
		i++;

	Con_Printf("%i free beams\n", i);

	for (i = 0; i < numparticletypes; i++)
	{
		m = l = k = j = 0;
		for (bs = part_type[i].beams; bs; bs = bs->next)
		{
			if (!bs->p)
				k++;

			if (bs->flags & BS_DEAD)
				l++;

			if (bs->flags & BS_LASTSEG)
				m++;

			j++;
		}

		if (j)
			Con_Printf("Type %i = %i NULL p, %i DEAD, %i LASTSEG, %i total\n", i, k, l, m, j);
	}
}

static void P_PartInfo_f (void)
{
	particle_t *p;

	int i, j;

	i = 0;

	for (p = free_particles; p; p = p->next)
		i++;

	Con_Printf("%i free particles\n", i);

	for (i = 0; i < numparticletypes; i++)
	{
		j = 0;
		for (p = part_type[i].particles; p; p = p->next)
			j++;

		if (j)
			Con_Printf("Type %s = %i total\n", part_type[i].name, j);
	}
}
#endif

/*
===============
R_InitParticles
===============
*/
static void PScript_InitParticles (void)
{
	int		i;

	if (r_numparticles)	//already inited
		return;

	buildsintable();

	i = COM_CheckParm ("-particles");

	if (i)
	{
		r_numparticles = (int)(Q_atoi(com_argv[i+1]));
	}
	else
	{
		r_numparticles = MAX_PARTICLES;
	}

	r_numbeams = MAX_BEAMSEGS;

	r_numdecals = MAX_DECALS;

	r_numtrailstates = MAX_TRAILSTATES;

	particles = (particle_t *)
			BZ_Malloc (r_numparticles * sizeof(particle_t));

	beams = (beamseg_t *)
			BZ_Malloc (r_numbeams * sizeof(beamseg_t));

	decals = (clippeddecal_t *)
			BZ_Malloc (r_numdecals * sizeof(clippeddecal_t));

	trailstates = (trailstate_t *)
			BZ_Malloc (r_numtrailstates * sizeof(trailstate_t));
	memset(trailstates, 0, r_numtrailstates * sizeof(trailstate_t));
	ts_cycle = 0;

	Cmd_AddRemCommand("pointfile", P_ReadPointFile_f);	//load the leak info produced from qbsp into the particle system to show a line. :)

	Cmd_AddRemCommand("r_part", P_ParticleEffect_f);
	Cmd_AddRemCommand("r_effect", P_AssosiateEffect_f);
	Cmd_AddRemCommand("r_trail", P_AssosiateTrail_f);

	Cmd_AddRemCommand("r_exportbuiltinparticles", P_ExportBuiltinSet_f);

#if _DEBUG
	Cmd_AddRemCommand("r_partinfo", P_PartInfo_f);
	Cmd_AddRemCommand("r_beaminfo", P_BeamInfo_f);
#endif

	CL_RegisterParticles();

	pt_pointfile		= P_AllocateParticleType("PT_POINTFILE");
	pe_default			= P_AllocateParticleType("PE_DEFAULT");
	pe_size2			= P_AllocateParticleType("PE_SIZE2");
	pe_size3			= P_AllocateParticleType("PE_SIZE3");
	pe_defaulttrail		= P_AllocateParticleType("PE_DEFAULTTRAIL");

	Cvar_Hook(&r_particlesdesc, R_ParticlesDesc_Callback);
	Cvar_ForceCallback(&r_particlesdesc);
}

static void PScript_Shutdown (void)
{
	Cvar_Unhook(&r_particlesdesc);

	Cmd_RemoveCommand("pointfile");	//load the leak info produced from qbsp into the particle system to show a line. :)

	Cmd_RemoveCommand("r_part");
	Cmd_RemoveCommand("r_effect");
	Cmd_RemoveCommand("r_trail");

	Cmd_RemoveCommand("r_exportbuiltinparticles");

#if _DEBUG
	Cmd_RemoveCommand("r_partinfo");
	Cmd_RemoveCommand("r_beaminfo");
#endif

	BZ_Free (particles);
	BZ_Free (beams);
	BZ_Free (decals);
	BZ_Free (trailstates);

	r_numparticles = 0;
}


/*
===============
P_ClearParticles
===============
*/
static void PScript_ClearParticles (void)
{
	int		i;

	free_particles = &particles[0];
	for (i=0 ;i<r_numparticles ; i++)
		particles[i].next = &particles[i+1];
	particles[r_numparticles-1].next = NULL;

	free_decals = &decals[0];
	for (i=0 ;i<r_numdecals ; i++)
		decals[i].next = &decals[i+1];
	decals[r_numdecals-1].next = NULL;

	free_beams = &beams[0];
	for (i=0 ;i<r_numbeams ; i++)
	{
		beams[i].p = NULL;
		beams[i].flags = BS_DEAD;
		beams[i].next = &beams[i+1];
	}
	beams[r_numbeams-1].next = NULL;

	particletime = cl.time;

	for (i = 0; i < numparticletypes; i++)
	{
		P_LoadTexture(&part_type[i], false);
	}

	for (i = 0; i < numparticletypes; i++)
	{
		part_type[i].clippeddecals = NULL;
		part_type[i].particles = NULL;
		part_type[i].beams = NULL;
		part_type[i].skytris = NULL;
	}
}

static void P_ExportBuiltinSet_f(void)
{
	char *efname = Cmd_Argv(1);
	char *file;

	if (!*efname)
	{
		Con_Printf("Please name the built in effect (faithful, spikeset, tsshaft, minimal or highfps)\n");
		return;
	}
	else if (!stricmp(efname, "faithful"))
		file = particle_set_faithful;
	else if (!stricmp(efname, "spikeset"))
		file = particle_set_spikeset;
	else if (!stricmp(efname, "highfps"))
		file = particle_set_highfps;
	else if (!stricmp(efname, "minimal"))
		file = particle_set_minimal;
	else if (!stricmp(efname, "tsshaft"))
		file = particle_set_tsshaft;
	else
	{
		if (!stricmp(efname, "none"))
		{
			Con_Printf("nothing to export\n");
			return;
		}
		Con_Printf("'%s' is not a built in particle set\n", efname);
		return;
	}

	COM_WriteFile(va("particles/%s.cfg", efname), file, strlen(file));
	Con_Printf("Written particles/%s.cfg\n", efname);
}

static void P_LoadParticleSet(char *name, qboolean first)
{
	int restrictlevel = Cmd_FromGamecode() ? RESTRICT_SERVER : RESTRICT_LOCAL; 

	//particle descriptions submitted by the server are deemed to not be cheats but game configs.
	if (!stricmp(name, "none"))
		return;
	else if (!stricmp(name, "faithful") || (first && !*name))
		Cbuf_AddText(particle_set_faithful, RESTRICT_LOCAL);
	else if (!stricmp(name, "spikeset"))
		Cbuf_AddText(particle_set_spikeset, RESTRICT_LOCAL);
	else if (!stricmp(name, "highfps"))
		Cbuf_AddText(particle_set_highfps, RESTRICT_LOCAL);
	else if (!stricmp(name, "minimal"))
		Cbuf_AddText(particle_set_minimal, RESTRICT_LOCAL);
	else if (!stricmp(name, "tsshaft"))
		Cbuf_AddText(particle_set_tsshaft, RESTRICT_LOCAL);
	else
	{
		char *file;
		FS_LoadFile(va("particles/%s.cfg", name), (void**)&file);
		if (!file)
			FS_LoadFile(va("%s.cfg", name), (void**)&file);
		if (file)
		{
			Cbuf_AddText(file, restrictlevel);
			Cbuf_AddText("\n", restrictlevel);
			FS_FreeFile(file);
		}
		else if (first)
		{
			Con_Printf(CON_WARNING "Couldn't find particle description %s, using spikeset\n", name);
			Cbuf_AddText(particle_set_spikeset, RESTRICT_LOCAL);
		}
		else
			Con_Printf(CON_WARNING "Couldn't find particle description %s\n", name);
	}
}

static void R_ParticlesDesc_Callback(struct cvar_s *var, char *oldvalue)
{
	extern model_t	mod_known[];
	extern int		mod_numknown;
	qboolean		first;

	model_t *mod;
	int i;
	char *c;

	if (cls.state == ca_disconnected)
		return; // don't bother parsing while disconnected

	for (i = 0; i < numparticletypes; i++)
	{
		*part_type[i].texname = '\0';
		part_type[i].scale = 0;
		part_type[i].loaded = 0;
		if (part_type->ramp)
			BZ_Free(part_type->ramp);
		part_type->ramp = NULL;
	}

	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		mod->particleeffect = P_INVALID;
		mod->particletrail = P_INVALID;
		mod->engineflags &= ~MDLF_NODEFAULTTRAIL;

		P_DefaultTrail(mod);
	}

	f_modified_particles = false;

	first = true;
	for (c = COM_ParseStringSet(var->string); com_token[0]; c = COM_ParseStringSet(c))
	{
		P_LoadParticleSet(com_token, first);
		first = false;
	}
}

static void P_ReadPointFile_f (void)
{
	vfsfile_t	*f;
	vec3_t	org;
	//int		r; //unreferenced
	int		c;
	char	name[MAX_OSPATH];
	char line[1024];
	char *s;

	COM_StripExtension(cl.worldmodel->name, name, sizeof(name));
	strcat(name, ".pts");

	f = FS_OpenVFS(name, "rb", FS_GAME);
	if (!f)
	{
		Con_Printf ("couldn't open %s\n", name);
		return;
	}

	P_ClearParticles();	//so overflows arn't as bad.

	Con_Printf ("Reading %s...\n", name);
	c = 0;
	for ( ;; )
	{
		VFS_GETS(f, line, sizeof(line));

		s = COM_Parse(line);
		org[0] = atof(com_token);

		s = COM_Parse(s);
		if (!s)
			continue;
		org[1] = atof(com_token);

		s = COM_Parse(s);
		if (!s)
			continue;
		org[2] = atof(com_token);
		if (COM_Parse(s))
			continue;

		c++;

		if (c%8)
			continue;

		if (!free_particles)
		{
			Con_Printf ("Not enough free particles\n");
			break;
		}
		P_RunParticleEffectType(org, NULL, 1, pt_pointfile);
	}

	VFS_CLOSE (f);
	Con_Printf ("%i points read\n", c);
}

static void P_AddRainParticles(void)
{
	float x;
	float y;
	static float skipped;
	static float lastrendered;
	int ptype;

	vec3_t org, vdist;

	skytris_t *st;

	if (!r_part_rain.value || !r_part_rain_quantity.value)
	{
		skipped = true;
		return;
	}

	if (lastrendered < particletime - 0.5)
		skipped = true;	//we've gone for half a sec without any new rain. This would cause some strange effects, so reset times.

	if (skipped)
	{
		for (ptype = 0; ptype<numparticletypes; ptype++)
		{
			for (st = part_type[ptype].skytris; st; st = st->next)
			{
				st->nexttime = particletime;
			}
		}
	}
	skipped = false;

	lastrendered = particletime;
/*
{
	int i;

glDisable(GL_TEXTURE_2D);
glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
glDisable(GL_DEPTH_TEST);
glBegin(GL_TRIANGLES);

	st = skytris;
	for (i = 0; i < r_part_rain_quantity.value; i++)
		st = st->next;
		glVertex3f(st->org[0], st->org[1], st->org[2]);
		glVertex3f(st->org[0]+st->x[0], st->org[1]+st->x[1], st->org[2]+st->x[2]);
		glVertex3f(st->org[0]+st->y[0], st->org[1]+st->y[1], st->org[2]+st->y[2]);
glEnd();
glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
glBegin(GL_POINTS);
		for (i = 0; i < 1000; i++)
		{
			x = frandom()*frandom();
			y = frandom() * (1-x);
			VectorMA(st->org, x, st->x, org);
			VectorMA(org, y, st->y, org);

			glVertex3f(org[0], org[1], org[2]);
		}
glEnd();
glEnable(GL_DEPTH_TEST);
}
*/
	for (ptype = 0; ptype<numparticletypes; ptype++)
	{
		if (!part_type[ptype].loaded)	//woo, batch skipping.
			continue;

		for (st = part_type[ptype].skytris; st; st = st->next)
		{
	//		if (st->face->visframe != r_framecount)
	//			continue;

			if (st->face->visframe != r_framecount)
			{
				st->nexttime = particletime;
				continue;
			}

			while (st->nexttime < particletime)
			{
				if (!free_particles)
					return;

				st->nexttime += 10000/(st->area*r_part_rain_quantity.value);

				x = frandom()*frandom();
				y = frandom() * (1-x);
				VectorMA(st->org, x, st->x, org);
				VectorMA(org, y, st->y, org);


				VectorSubtract(org, r_refdef.vieworg, vdist);

				if (Length(vdist) > (1024+512)*frandom())
					continue;

				if (st->face->flags & SURF_PLANEBACK)
					VectorMA(org, -0.5, st->face->plane->normal, org);
				else
					VectorMA(org, 0.5, st->face->plane->normal, org);

				if (!(cl.worldmodel->funcs.PointContents(cl.worldmodel, org) & FTECONTENTS_SOLID))
				{
					if (st->face->flags & SURF_PLANEBACK)
					{
						vdist[0] = -st->face->plane->normal[0];
						vdist[1] = -st->face->plane->normal[1];
						vdist[2] = -st->face->plane->normal[2];
						P_RunParticleEffectType(org, vdist, 1, ptype);
					}
					else
						P_RunParticleEffectType(org, st->face->plane->normal, 1, ptype);
				}
			}
		}
	}
}


static void R_Part_SkyTri(float *v1, float *v2, float *v3, msurface_t *surf)
{
	float dot;
	float xm;
	float ym;
	float theta;
	vec3_t xd;
	vec3_t yd;

	skytris_t *st;

	st = Hunk_Alloc(sizeof(skytris_t));
	st->next = part_type[surf->texinfo->texture->parttype].skytris;
	VectorCopy(v1, st->org);
	VectorSubtract(v2, st->org, st->x);
	VectorSubtract(v3, st->org, st->y);

	VectorCopy(st->x, xd);
	VectorCopy(st->y, yd);
/*
	xd[2] = 0;	//prevent area from being valid on vertical surfaces
	yd[2] = 0;
*/
	xm = Length(xd);
	ym = Length(yd);

	dot = DotProduct(xd, yd);
	theta = acos(dot/(xm*ym));
	st->area = sin(theta)*xm*ym;
	st->nexttime = particletime;
	st->face = surf;

	if (st->area<=0)
		return;//bummer.

	part_type[surf->texinfo->texture->parttype].skytris = st;
}



static void PScript_EmitSkyEffectTris(model_t *mod, msurface_t 	*fa)
{
	vec3_t		verts[64];
	int v1;
	int v2;
	int v3;
	int numverts;
	int i, lindex;
	float *vec;

	//
	// convert edges back to a normal polygon
	//
	numverts = 0;
	for (i=0 ; i<fa->numedges ; i++)
	{
		lindex = mod->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = mod->vertexes[mod->edges[lindex].v[0]].position;
		else
			vec = mod->vertexes[mod->edges[-lindex].v[1]].position;
		VectorCopy (vec, verts[numverts]);
		numverts++;

		if (numverts>=64)
		{
			Con_Printf("Too many verts on sky surface\n");
			return;
		}
	}

	v1 = 0;
	v2 = 1;
	for (v3 = 2; v3 < numverts; v3++)
	{
		R_Part_SkyTri(verts[v1], verts[v2], verts[v3], fa);

		v2 = v3;
	}
}

// Trailstate functions
static void P_CleanTrailstate(trailstate_t *ts)
{
	// clear LASTSEG flag from lastbeam so it can be reused
	if (ts->lastbeam)
	{
		ts->lastbeam->flags &= ~BS_LASTSEG;
		ts->lastbeam->flags |= BS_NODRAW;
	}

	// clean structure
	memset(ts, 0, sizeof(trailstate_t));
}

static void PScript_DelinkTrailstate(trailstate_t **tsk)
{
	trailstate_t *ts;
	trailstate_t *assoc;

	if (*tsk == NULL)
		return; // not linked to a trailstate

	ts = *tsk; // store old pointer
	*tsk = NULL; // clear pointer

	if (ts->key != tsk)
		return; // prevent overwrite

	assoc = ts->assoc; // store assoc
	P_CleanTrailstate(ts); // clean directly linked trailstate

	// clean trailstates assoc linked
	while (assoc)
	{
		ts = assoc->assoc;
		P_CleanTrailstate(assoc);
		assoc = ts;
	}
}

static trailstate_t *P_NewTrailstate(trailstate_t **key)
{
	trailstate_t *ts;

	// bounds check here in case r_numtrailstates changed
	if (ts_cycle >= r_numtrailstates)
		ts_cycle = 0;

	// get trailstate
	ts = trailstates + ts_cycle;

	// clear trailstate
	P_CleanTrailstate(ts);

	// set key
	ts->key = key;

	// advance index cycle
	ts_cycle++;

	// return clean trailstate
	return ts;
}

#define NUMVERTEXNORMALS	162
static float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};
static vec2_t	avelocities[NUMVERTEXNORMALS];
#define BEAMLENGTH 16
// vec3_t	avelocity = {23, 7, 3};
// float	partstep = 0.01;
// float	timescale = 0.01;

int Q1BSP_ClipDecal(vec3_t center, vec3_t normal, vec3_t tangent, vec3_t tangent2, float size, float **out);
static int PScript_RunParticleEffectState (vec3_t org, vec3_t dir, float count, int typenum, trailstate_t **tsk)
{
	part_type_t *ptype = &part_type[typenum];
	int i, j, k, l, spawnspc;
	float m, pcount;
	particle_t	*p;
	beamseg_t *b, *bfirst;
	vec3_t ofsvec, arsvec; // offsetspread vec, areaspread vec
	trailstate_t *ts;

	if (typenum < 0 || typenum >= numparticletypes)
		return 1;

	if (!ptype->loaded)
		return 1;

	// inwater check, switch only once
	if (r_part_contentswitch.value && ptype->inwater >= 0 && cl.worldmodel)
	{
		int cont;
		cont = cl.worldmodel->funcs.PointContents(cl.worldmodel, org);

		if (cont & FTECONTENTS_WATER)
			ptype = &part_type[ptype->inwater];
	}

	// eliminate trailstate if flag set
	if (ptype->flags & PT_NOSTATE)
		tsk = NULL;

	// trailstate allocation/deallocation
	if (tsk)
	{
		// if *tsk = NULL get a new one
		if (*tsk == NULL)
		{
			ts = P_NewTrailstate(tsk);
			*tsk = ts;
		}
		else
		{
			ts = *tsk;

			if (ts->key != tsk) // trailstate was overwritten
			{
				ts = P_NewTrailstate(tsk); // so get a new one
				*tsk = ts;
			}
		}
	}
	else
		ts = NULL;

	if (ptype->looks.type == PT_DECAL)
	{
		clippeddecal_t *d;
		int decalcount;
		float dist;
		vec3_t tangent, t2;
		vec3_t vec={0.5, 0.5, 0.5};
		float *decverts;
		int i;
		trace_t tr;

		vec3_t bestdir;

		if (!free_decals)
			return 0;

		if (!dir)
		{
			bestdir[0] = 0;
			bestdir[1] = 0.73;
			bestdir[2] = 0.73;
			dist = 1;
			for (i = 0; i < 6; i++)
			{
				if (i >= 3)
				{
					t2[0] = ((i&3)==0)*8;
					t2[1] = ((i&3)==1)*8;
					t2[2] = ((i&3)==2)*8;
				}
				else
				{
					t2[0] = -((i&3)==0)*8;
					t2[1] = -((i&3)==1)*8;
					t2[2] = -((i&3)==2)*8;
				}
				VectorSubtract(org, t2, tangent);
				VectorAdd(org, t2, t2);

				if (cl.worldmodel->funcs.Trace (cl.worldmodel, 0, 0,tangent, t2, vec3_origin, vec3_origin, &tr))
				{
					if (tr.fraction < dist)
					{
						dist = tr.fraction;
						VectorCopy(tr.plane.normal, bestdir);
					}
				}
			}
			dir = bestdir;
		}
		VectorInverse(dir);
		VectorNormalize(dir);

		VectorNormalize(vec);
		CrossProduct(dir, vec, tangent);
		CrossProduct(dir, tangent, t2);

		decalcount = Q1BSP_ClipDecal(org, dir, tangent, t2, ptype->scale, &decverts);
		while(decalcount)
		{
			if (!free_decals)
				break;

			d = free_decals;
			free_decals = d->next;
			d->next = ptype->clippeddecals;
			ptype->clippeddecals = d;

			VectorCopy((decverts+0), d->vertex[0]);
			VectorCopy((decverts+3), d->vertex[1]);
			VectorCopy((decverts+6), d->vertex[2]);

			for (i = 0; i < 3; i++)
			{
				VectorSubtract(d->vertex[i], org, vec);
				d->texcoords[i][0] = (DotProduct(vec, t2)/ptype->scale)+0.5;
				d->texcoords[i][1] = (DotProduct(vec, tangent)/ptype->scale)+0.5;
			}

			d->die = ptype->randdie*frandom();

			if (ptype->die)
				d->alpha = ptype->alpha+d->die*ptype->alphachange;
			else
				d->alpha = ptype->alpha;

			if (ptype->colorindex >= 0)
			{
				int cidx;
				cidx = ptype->colorrand > 0 ? rand() % ptype->colorrand : 0;
				cidx = ptype->colorindex + cidx;
				if (cidx > 255)
					d->alpha = d->alpha / 2; // Hexen 2 style transparency
				cidx = (cidx & 0xff) * 3;
				d->rgb[0] = host_basepal[cidx] * (1/255.0);
				d->rgb[1] = host_basepal[cidx+1] * (1/255.0);
				d->rgb[2] = host_basepal[cidx+2] * (1/255.0);
			}
			else
				VectorCopy(ptype->rgb, d->rgb);

			vec[2] = frandom();
			vec[0] = vec[2]*ptype->rgbrandsync[0] + frandom()*(1-ptype->rgbrandsync[0]);
			vec[1] = vec[2]*ptype->rgbrandsync[1] + frandom()*(1-ptype->rgbrandsync[1]);
			vec[2] = vec[2]*ptype->rgbrandsync[2] + frandom()*(1-ptype->rgbrandsync[2]);
			d->rgb[0] += vec[0]*ptype->rgbrand[0] + ptype->rgbchange[0]*d->die;
			d->rgb[1] += vec[1]*ptype->rgbrand[1] + ptype->rgbchange[1]*d->die;
			d->rgb[2] += vec[2]*ptype->rgbrand[2] + ptype->rgbchange[2]*d->die;

			d->die = particletime + ptype->die - d->die;

			decverts += 3*3;
			decalcount--;


			// maintain run list
			if (!(ptype->state & PS_INRUNLIST))
			{
				ptype->nexttorun = part_run_list;
				part_run_list = ptype;
				ptype->state |= PS_INRUNLIST;
			}
		}

		return 0;
	}

	// get msvc to shut up
	j = k = l = 0;
	m = 0;

	while(ptype)
	{
		// init spawn specific variables
		b = bfirst = NULL;
		spawnspc = 8;
		pcount = count*(ptype->count+ptype->countrand*frandom());
		if (ptype->flags & PT_INVFRAMETIME)
			pcount /= host_frametime;
		if (ts)
			pcount += ts->state2.emittime;

		switch (ptype->spawnmode)
		{
		case SM_UNICIRCLE:
			m = pcount;
			if (ptype->looks.type == PT_BEAM)
				m--;

			if (m < 1)
				m = 0;
			else
				m = (M_PI*2)/m;

			if (ptype->spawnparam1) /* use for weird shape hacks */
				m *= ptype->spawnparam1;
			break;
		case SM_TELEBOX:
			spawnspc = 4;
			l = -ptype->areaspreadvert;
		case SM_LAVASPLASH:
			j = k = -ptype->areaspread;
			if (ptype->spawnparam1)
				m = ptype->spawnparam1;
			else
				m = 0.55752; /* default weird number for tele/lavasplash used in vanilla Q1 */

			if (ptype->spawnparam2)
				spawnspc = (int)ptype->spawnparam2;
			break;
		case SM_FIELD:
			if (!avelocities[0][0])
			{
				for (j=0 ; j<NUMVERTEXNORMALS*2 ; j++)
					avelocities[0][j] = (rand()&255) * 0.01;
			}

			j = 0;
			m = 0;
			break;
		default:	//others don't need intitialisation
			break;
		}

		// time limit (for completeness)
		if (ptype->spawntime && ts)
		{
			if (ts->state1.statetime > particletime)
				return 0; // timelimit still in effect

			ts->state1.statetime = particletime + ptype->spawntime; // record old time
		}

		// random chance for point effects
		if (ptype->spawnchance < frandom())
		{
			i = ceil(pcount);
			break;
		}

		if (!ptype->die && ptype->count == 1 && ptype->countrand == 0)
		{
			i = 0;
			pcount = 1;
		}

		// particle spawning loop
		for (i = 0; i < pcount; i++)
		{
			if (!free_particles)
				break;
			p = free_particles;
			if (ptype->looks.type == PT_BEAM)
			{
				if (!free_beams)
					break;
				if (b)
				{
					b = b->next = free_beams;
					free_beams = free_beams->next;
				}
				else
				{
					b = bfirst = free_beams;
					free_beams = free_beams->next;
				}
				b->texture_s = i; // TODO: FIX THIS NUMBER
				b->flags = 0;
				b->p = p;
				VectorClear(b->dir);
			}
			free_particles = p->next;
			p->next = ptype->particles;
			ptype->particles = p;

			p->die = ptype->randdie*frandom();
			p->scale = ptype->scale+ptype->randscale*frandom();
			if (ptype->die)
				p->alpha = ptype->alpha+p->die*ptype->alphachange;
			else
				p->alpha = ptype->alpha;
			// p->color = 0;
			if (ptype->emittime < 0)
				p->state.trailstate = NULL;
			else
				p->state.nextemit = particletime + ptype->emitstart - p->die;

			p->rotationspeed = ptype->rotationmin + frandom()*ptype->rotationrand;
			p->angle = ptype->rotationstartmin + frandom()*ptype->rotationstartrand;
			p->s1 = ptype->s1;
			p->t1 = ptype->t1;
			p->s2 = ptype->s2;
			p->t2 = ptype->t2;
			if (ptype->randsmax!=1)
			{
				m = ptype->texsstride * (rand()%ptype->randsmax);
				p->s1 += m;
				p->s2 += m;
			}

			if (ptype->colorindex >= 0)
			{
				int cidx;
				cidx = ptype->colorrand > 0 ? rand() % ptype->colorrand : 0;
				cidx = ptype->colorindex + cidx;
				if (cidx > 255)
					p->alpha = p->alpha / 2; // Hexen 2 style transparency
				cidx = (cidx & 0xff) * 3;
				p->rgb[0] = host_basepal[cidx] * (1/255.0);
				p->rgb[1] = host_basepal[cidx+1] * (1/255.0);
				p->rgb[2] = host_basepal[cidx+2] * (1/255.0);
			}
			else
				VectorCopy(ptype->rgb, p->rgb);

			// use org temporarily for rgbsync
			p->org[2] = frandom();
			p->org[0] = p->org[2]*ptype->rgbrandsync[0] + frandom()*(1-ptype->rgbrandsync[0]);
			p->org[1] = p->org[2]*ptype->rgbrandsync[1] + frandom()*(1-ptype->rgbrandsync[1]);
			p->org[2] = p->org[2]*ptype->rgbrandsync[2] + frandom()*(1-ptype->rgbrandsync[2]);

			p->rgb[0] += p->org[0]*ptype->rgbrand[0] + ptype->rgbchange[0]*p->die;
			p->rgb[1] += p->org[1]*ptype->rgbrand[1] + ptype->rgbchange[1]*p->die;
			p->rgb[2] += p->org[2]*ptype->rgbrand[2] + ptype->rgbchange[2]*p->die;

			// randomvel
			p->vel[0] = crandom()*ptype->randomvel;
			p->vel[1] = crandom()*ptype->randomvel;
			p->vel[2] = crandom()*ptype->randomvelvert;

			// handle spawn modes (org/vel)
			switch (ptype->spawnmode)
			{
			case SM_BOX:
				ofsvec[0] = crandom();
				ofsvec[1] = crandom();
				ofsvec[2] = crandom();

				arsvec[0] = ofsvec[0]*ptype->areaspread;
				arsvec[1] = ofsvec[1]*ptype->areaspread;
				arsvec[2] = ofsvec[2]*ptype->areaspreadvert;
				break;
			case SM_TELEBOX:
				ofsvec[0] = k;
				ofsvec[1] = j;
				ofsvec[2] = l+4;
				VectorNormalize(ofsvec);
				VectorScale(ofsvec, 1.0-(frandom())*m, ofsvec);

				// org is just like the original
				arsvec[0] = j + (rand()%spawnspc);
				arsvec[1] = k + (rand()%spawnspc);
				arsvec[2] = l + (rand()%spawnspc);

				// advance telebox loop
				j += spawnspc;
				if (j >= ptype->areaspread)
				{
					j = -ptype->areaspread;
					k += spawnspc;
					if (k >= ptype->areaspread)
					{
						k = -ptype->areaspread;
						l += spawnspc;
						if (l >= ptype->areaspreadvert)
							l = -ptype->areaspreadvert;
					}
				}
				break;
			case SM_LAVASPLASH:
				// calc directions, org with temp vector
				ofsvec[0] = k + (rand()%spawnspc);
				ofsvec[1] = j + (rand()%spawnspc);
				ofsvec[2] = 256;

				arsvec[0] = ofsvec[0];
				arsvec[1] = ofsvec[1];
				arsvec[2] = frandom()*ptype->areaspreadvert;

				VectorNormalize(ofsvec);
				VectorScale(ofsvec, 1.0-(frandom())*m, ofsvec);

				// advance splash loop
				j += spawnspc;
				if (j >= ptype->areaspread)
				{
					j = -ptype->areaspread;
					k += spawnspc;
					if (k >= ptype->areaspread)
						k = -ptype->areaspread;
				}
				break;
			case SM_UNICIRCLE:
				ofsvec[0] = cos(m*i);
				ofsvec[1] = sin(m*i);
				ofsvec[2] = 0;
				VectorScale(ofsvec, ptype->areaspread, arsvec);
				break;
			case SM_FIELD:
				arsvec[0] = cl.time * (avelocities[i][0] + m);
				arsvec[1] = cl.time * (avelocities[i][1] + m);
				arsvec[2] = cos(arsvec[1]);

				ofsvec[0] = arsvec[2]*cos(arsvec[0]);
				ofsvec[1] = arsvec[2]*sin(arsvec[0]);
				ofsvec[2] = -sin(arsvec[1]);

				arsvec[0] = r_avertexnormals[j][0]*ptype->areaspread + ofsvec[0]*BEAMLENGTH;
				arsvec[1] = r_avertexnormals[j][1]*ptype->areaspread + ofsvec[1]*BEAMLENGTH;
				arsvec[2] = r_avertexnormals[j][2]*ptype->areaspreadvert + ofsvec[2]*BEAMLENGTH;

				VectorNormalize(ofsvec);

				j++;
				if (j >= NUMVERTEXNORMALS)
				{
					j = 0;
					m += 0.1762891; // some BS number to try to "randomize" things
				}
				break;
			case SM_DISTBALL:
				{
					float rdist;

					rdist = ptype->spawnparam2 - crandom()*(1-(crandom() * ptype->spawnparam1));

					// this is a strange spawntype, which is based on the fact that
					// crandom()*crandom() provides something similar to an exponential
					// probability curve
					ofsvec[0] = hrandom();
					ofsvec[1] = hrandom();
					if (ptype->areaspreadvert)
						ofsvec[2] = hrandom();
					else
						ofsvec[2] = 0;

					VectorNormalize(ofsvec);
					VectorScale(ofsvec, rdist, ofsvec);

					arsvec[0] = ofsvec[0]*ptype->areaspread;
					arsvec[1] = ofsvec[1]*ptype->areaspread;
					arsvec[2] = ofsvec[2]*ptype->areaspreadvert;
				}
				break;
			default: // SM_BALL, SM_CIRCLE
				ofsvec[0] = hrandom();
				ofsvec[1] = hrandom();
				if (ptype->areaspreadvert)
					ofsvec[2] = hrandom();
				else
					ofsvec[2] = 0;

				VectorNormalize(ofsvec);
				if (ptype->spawnmode != SM_CIRCLE)
					VectorScale(ofsvec, frandom(), ofsvec);

				arsvec[0] = ofsvec[0]*ptype->areaspread;
				arsvec[1] = ofsvec[1]*ptype->areaspread;
				arsvec[2] = ofsvec[2]*ptype->areaspreadvert;
				break;
			}

			p->org[0] = org[0] + arsvec[0];
			p->org[1] = org[1] + arsvec[1];
			p->org[2] = org[2] + arsvec[2] + ptype->offsetup;

			// apply arsvec+ofsvec
			if (dir)
			{
				p->vel[0] += dir[0]*ptype->veladd+ofsvec[0]*ptype->offsetspread;
				p->vel[1] += dir[1]*ptype->veladd+ofsvec[1]*ptype->offsetspread;
				p->vel[2] += dir[2]*ptype->veladd+ofsvec[2]*ptype->offsetspreadvert;

				p->org[0] += dir[0]*ptype->orgadd;
				p->org[1] += dir[1]*ptype->orgadd;
				p->org[2] += dir[2]*ptype->orgadd;
			}
			else
			{
				p->vel[0] += ofsvec[0]*ptype->offsetspread;
				p->vel[1] += ofsvec[1]*ptype->offsetspread;
				p->vel[2] += ofsvec[2]*ptype->offsetspreadvert - ptype->veladd;

				p->org[2] -= ptype->orgadd;
			}

			p->die = particletime + ptype->die - p->die;
		}

		// update beam list
		if (ptype->looks.type == PT_BEAM)
		{
			if (b)
			{
				// update dir for bfirst for certain modes since it will never get updated
				switch (ptype->spawnmode)
				{
				case SM_UNICIRCLE:
					// kinda hackish here, assuming ofsvec contains the point at i-1
					arsvec[0] = cos(m*(i-2));
					arsvec[1] = sin(m*(i-2));
					arsvec[2] = 0;
					VectorSubtract(ofsvec, arsvec, bfirst->dir);
					VectorNormalize(bfirst->dir);
					break;
				default:
					break;
				}

				b->flags |= BS_NODRAW;
				b->next = ptype->beams;
				ptype->beams = bfirst;
			}
		}

		// save off emit times in trailstate
		if (ts)
			ts->state2.emittime = pcount - i;

		// maintain run list
		if (!(ptype->state & PS_INRUNLIST))
		{
			ptype->nexttorun = part_run_list;
			part_run_list = ptype;
			ptype->state |= PS_INRUNLIST;
		}

		// go to next associated effect
		if (ptype->assoc < 0)
			break;

		// new trailstate
		if (ts)
		{
			tsk = &(ts->assoc);
			// if *tsk = NULL get a new one
			if (*tsk == NULL)
			{
				ts = P_NewTrailstate(tsk);
				*tsk = ts;
			}
			else
			{
				ts = *tsk;

				if (ts->key != tsk) // trailstate was overwritten
				{
					ts = P_NewTrailstate(tsk); // so get a new one
					*tsk = ts;
				}
			}
		}

		ptype = &part_type[ptype->assoc];
	}

	return 0;
}

static int PScript_RunParticleEffectTypeString (vec3_t org, vec3_t dir, float count, char *name)
{
	int type = P_FindParticleType(name);
	if (type < 0)
		return 1;

	return P_RunParticleEffectType(org, dir, count, type);
}

/*
===============
P_RunParticleEffect

===============
*/
static void PScript_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
	int ptype;

	ptype = P_FindParticleType(va("pe_%i", color));
	if (P_RunParticleEffectType(org, dir, count, ptype))
	{
		color &= ~0x7;
		if (count > 130 && part_type[pe_size3].loaded)
		{
			part_type[pe_size3].colorindex = color;
			part_type[pe_size3].colorrand = 8;
			P_RunParticleEffectType(org, dir, count, pe_size3);
			return;
		}
		if (count > 20 && part_type[pe_size2].loaded)
		{
			part_type[pe_size2].colorindex = color;
			part_type[pe_size2].colorrand = 8;
			P_RunParticleEffectType(org, dir, count, pe_size2);
			return;
		}
		part_type[pe_default].colorindex = color;
		part_type[pe_default].colorrand = 8;
		P_RunParticleEffectType(org, dir, count, pe_default);
		return;
	}
}

//h2 stylie
static void PScript_RunParticleEffect2 (vec3_t org, vec3_t dmin, vec3_t dmax, int color, int effect, int count)
{
	int			i, j;
	float		num;
	float invcount;
	vec3_t	nvel;

	int ptype = P_FindParticleType(va("pe2_%i_%i", effect, color));
	if (ptype < 0)
	{
		ptype = P_FindParticleType(va("pe2_%i", effect));
		if (ptype < 0)
			ptype = pe_default;

		part_type[ptype].colorindex = color;
	}

	invcount = 1/part_type[ptype].count; // using this to get R_RPET to always spawn 1
	count = count * part_type[ptype].count;

	for (i=0 ; i<count ; i++)
	{
		if (!free_particles)
			return;

		for (j=0 ; j<3 ; j++)
		{
			num = rand() / (float)RAND_MAX;
			nvel[j] = dmin[j] + ((dmax[j] - dmin[j]) * num);
		}
		P_RunParticleEffectType(org, nvel, invcount, ptype);

	}
}

/*
===============
P_RunParticleEffect3

===============
*/
//h2 stylie
static void PScript_RunParticleEffect3 (vec3_t org, vec3_t box, int color, int effect, int count)
{
	int			i, j;
	vec3_t	nvel;
	float		num;
	float invcount;

	int ptype = P_FindParticleType(va("pe3_%i_%i", effect, color));
	if (ptype < 0)
	{
		ptype = P_FindParticleType(va("pe3_%i", effect));
		if (ptype < 0)
			ptype = pe_default;

		part_type[ptype].colorindex = color;
	}

	invcount = 1/part_type[ptype].count; // using this to get R_RPET to always spawn 1
	count = count * part_type[ptype].count;

	for (i=0 ; i<count ; i++)
	{
		if (!free_particles)
			return;

		for (j=0 ; j<3 ; j++)
		{
			num = rand() / (float)RAND_MAX;
			nvel[j] = (box[j] * num * 2) - box[j];
		}

		P_RunParticleEffectType(org, nvel, invcount, ptype);
	}
}

/*
===============
P_RunParticleEffect4

===============
*/
//h2 stylie
static void PScript_RunParticleEffect4 (vec3_t org, float radius, int color, int effect, int count)
{
	int			i, j;
	vec3_t	nvel;
	float		num;
	float invcount;

	int ptype = P_FindParticleType(va("pe4_%i_%i", effect, color));
	if (ptype < 0)
	{
		ptype = P_FindParticleType(va("pe4_%i", effect));
		if (ptype < 0)
			ptype = pe_default;

		part_type[ptype].colorindex = color;
	}

	invcount = 1/part_type[ptype].count; // using this to get R_RPET to always spawn 1
	count = count * part_type[ptype].count;

	for (i=0 ; i<count ; i++)
	{
		if (!free_particles)
			return;

		for (j=0 ; j<3 ; j++)
		{
			num = rand() / (float)RAND_MAX;
			nvel[j] = (radius * num * 2) - radius;
		}
		P_RunParticleEffectType(org, nvel, invcount, ptype);
	}
}

static void PScript_RunParticleCube(vec3_t minb, vec3_t maxb, vec3_t dir, float count, int colour, qboolean gravity, float jitter)
{
	vec3_t org;
	int			i, j;
	float		num;
	float invcount;

	int ptype = P_FindParticleType(va("te_cube%s_%i", gravity?"_g":"", colour));
	if (ptype < 0)
	{
		ptype = P_FindParticleType(va("te_cube%s", gravity?"_g":""));
		if (ptype < 0)
			ptype = pe_default;

		part_type[ptype].colorindex = colour;
	}

	invcount = 1/part_type[ptype].count; // using this to get R_RPET to always spawn 1
	count = count * part_type[ptype].count;

	for (i=0 ; i<count ; i++)
	{
		if (!free_particles)
			return;

		for (j=0 ; j<3 ; j++)
		{
			num = rand() / (float)RAND_MAX;
			org[j] = minb[j] + num*(maxb[j]-minb[j]);
		}
		P_RunParticleEffectType(org, dir, invcount, ptype);
	}
}

static void PScript_RunParticleWeather(vec3_t minb, vec3_t maxb, vec3_t dir, float count, int colour, char *efname)
{
	vec3_t org;
	int			i, j;
	float		num;
	float invcount;

	int ptype = P_FindParticleType(va("te_%s_%i", efname, colour));
	if (ptype < 0)
	{
		ptype = P_FindParticleType(va("te_%s", efname));
		if (ptype < 0)
			ptype = pe_default;

		part_type[ptype].colorindex = colour;
	}

	invcount = 1/part_type[ptype].count; // using this to get R_RPET to always spawn 1
	count = count * part_type[ptype].count;

	for (i=0 ; i<count ; i++)
	{
		if (!free_particles)
			return;

		for (j=0 ; j<3 ; j++)
		{
			num = rand() / (float)RAND_MAX;
			org[j] = minb[j] + num*(maxb[j]-minb[j]);
		}
		P_RunParticleEffectType(org, dir, invcount, ptype);
	}
}

static void P_ParticleTrailDraw (vec3_t startpos, vec3_t end, part_type_t *ptype, trailstate_t **tsk)
{
	vec3_t	vec, vstep, right, up, start;
	float	len;
	int			tcount;
	particle_t	*p;
	beamseg_t   *b;
	beamseg_t   *bfirst;
	trailstate_t *ts;

	float veladd = -ptype->veladd;
	float randvel = ptype->randomvel;
	float randvelvert = ptype->randomvelvert;
	float step;
	float stop;
	float tdegree = 2*M_PI/256; /* MSVC whine */
	float sdegree = 0;
	float nrfirst, nrlast;

	VectorCopy(startpos, start);

	// eliminate trailstate if flag set
	if (ptype->flags & PT_NOSTATE)
		tsk = NULL;

	// trailstate allocation/deallocation
	if (tsk)
	{
		// if *tsk = NULL get a new one
		if (*tsk == NULL)
		{
			ts = P_NewTrailstate(tsk);
			*tsk = ts;
		}
		else
		{
			ts = *tsk;

			if (ts->key != tsk) // trailstate was overwritten
			{
				ts = P_NewTrailstate(tsk); // so get a new one
				*tsk = ts;
			}
		}
	}
	else
		ts = NULL;

	if (ptype->assoc>=0)
	{
		if (ts)
			P_ParticleTrail(start, end, ptype->assoc, &(ts->assoc));
		else
			P_ParticleTrail(start, end, ptype->assoc, NULL);
	}

	// time limit for trails
	if (ptype->spawntime && ts)
	{
		if (ts->state1.statetime > particletime)
			return; // timelimit still in effect

		ts->state1.statetime = particletime + ptype->spawntime; // record old time
		ts = NULL; // clear trailstate so we don't save length/lastseg
	}

	// random chance for trails
	if (ptype->spawnchance < frandom())
		return; // don't spawn but return success

	if (!ptype->die)
		ts = NULL;

	// use ptype step to calc step vector and step size
	step = 1/ptype->count;

	if (step < 0.01)
		step = 0.01;

	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	if (ptype->flags & PT_AVERAGETRAIL)
	{
		float tavg;
		// mangle len/step to get last point to be at end
		tavg = len / step;
		tavg = tavg / ceil(tavg);
		step *= tavg;
		len += step;
	}

	VectorScale(vec, step, vstep);

	// add offset
	start[2] += ptype->offsetup;

	// spawn mode precalculations
	if (ptype->spawnmode == SM_SPIRAL)
	{
		VectorVectors(vec, right, up);

		// precalculate degree of rotation
		if (ptype->spawnparam1)
			tdegree = 2*M_PI/ptype->spawnparam1; /* distance per rotation inversed */
		sdegree = ptype->spawnparam2*(M_PI/180);
	}
	else if (ptype->spawnmode == SM_CIRCLE)
	{
		VectorVectors(vec, right, up);
	}

	// store last stop here for lack of a better solution besides vectors
	if (ts)
	{
		ts->state2.laststop = stop = ts->state2.laststop + len;	//when to stop
		len = ts->state1.lastdist;
	}
	else
	{
		stop = len;
		len = 0;
	}

//	len = ts->lastdist/step;
//	len = (len - (int)len)*step;
//	VectorMA (start, -len, vec, start);

	if (ptype->flags & PT_NOSPREADFIRST)
		nrfirst = len + step*1.5;
	else
		nrfirst = len;

	if (ptype->flags & PT_NOSPREADLAST)
		nrlast = stop;
	else
		nrlast = stop + step;

	b = bfirst = NULL;

	while (len < stop)
	{
		len += step;

		if (!free_particles)
		{
			len = stop;
			break;
		}

		p = free_particles;
		if (ptype->looks.type == PT_BEAM)
		{
			if (!free_beams)
			{
				len = stop;
				break;
			}
			if (b)
			{
				b = b->next = free_beams;
				free_beams = free_beams->next;
			}
			else
			{
				b = bfirst = free_beams;
				free_beams = free_beams->next;
			}
			b->texture_s = len; // not sure how to calc this
			b->flags = 0;
			b->p = p;
			VectorCopy(vec, b->dir);
		}

		free_particles = p->next;
		p->next = ptype->particles;
		ptype->particles = p;

		p->die = ptype->randdie*frandom();
		p->scale = ptype->scale+ptype->randscale*frandom();
		if (ptype->die)
			p->alpha = ptype->alpha+p->die*ptype->alphachange;
		else
			p->alpha = ptype->alpha;
//		p->color = 0;

//		if (ptype->spawnmode == SM_TRACER)
		if (ptype->spawnparam1)
			tcount = (int)(len * ptype->count / ptype->spawnparam1);
		else
			tcount = (int)(len * ptype->count);

		if (ptype->colorindex >= 0)
		{
			int cidx;
			cidx = ptype->colorrand > 0 ? rand() % ptype->colorrand : 0;
			if (ptype->flags & PT_CITRACER) // colorindex behavior as per tracers in std Q1
				cidx += ((tcount & 4) << 1);

			cidx = ptype->colorindex + cidx;
			if (cidx > 255)
				p->alpha = p->alpha / 2;
			cidx = (cidx & 0xff) * 3;
			p->rgb[0] = host_basepal[cidx] * (1/255.0);
			p->rgb[1] = host_basepal[cidx+1] * (1/255.0);
			p->rgb[2] = host_basepal[cidx+2] * (1/255.0);
		}
		else
			VectorCopy(ptype->rgb, p->rgb);

		// use org temporarily for rgbsync
		p->org[2] = frandom();
		p->org[0] = p->org[2]*ptype->rgbrandsync[0] + frandom()*(1-ptype->rgbrandsync[0]);
		p->org[1] = p->org[2]*ptype->rgbrandsync[1] + frandom()*(1-ptype->rgbrandsync[1]);
		p->org[2] = p->org[2]*ptype->rgbrandsync[2] + frandom()*(1-ptype->rgbrandsync[2]);
		if (ptype->orgadd)
		{
			p->org[0] += vec[0]*ptype->orgadd;
			p->org[1] += vec[1]*ptype->orgadd;
			p->org[2] += vec[2]*ptype->orgadd;
		}

		p->rgb[0] += p->org[0]*ptype->rgbrand[0] + ptype->rgbchange[0]*p->die;
		p->rgb[1] += p->org[1]*ptype->rgbrand[1] + ptype->rgbchange[1]*p->die;
		p->rgb[2] += p->org[2]*ptype->rgbrand[2] + ptype->rgbchange[2]*p->die;

		VectorClear (p->vel);
		if (ptype->emittime < 0)
			p->state.trailstate = NULL; // init trailstate
		else
			p->state.nextemit = particletime + ptype->emitstart - p->die;

		p->rotationspeed = ptype->rotationmin + frandom()*ptype->rotationrand;
		p->angle = ptype->rotationstartmin + frandom()*ptype->rotationstartrand;
		p->s1 = ptype->s1;
		p->t1 = ptype->t1;
		p->s2 = ptype->s2;
		p->t2 = ptype->t2;
		if (ptype->randsmax!=1)
		{
			float offs;
			offs = ptype->texsstride * (rand()%ptype->randsmax);
			p->s1 += offs;
			p->s2 += offs;
		}

		if (len < nrfirst || len >= nrlast)
		{
			// no offset or areaspread for these particles...
			p->vel[0] = vec[0]*veladd+crandom()*randvel;
			p->vel[1] = vec[1]*veladd+crandom()*randvel;
			p->vel[2] = vec[2]*veladd+crandom()*randvelvert;

			VectorCopy(start, p->org);
		}
		else
		{
			switch(ptype->spawnmode)
			{
			case SM_TRACER:
				if (tcount & 1)
				{
					p->vel[0] = vec[1]*ptype->offsetspread;
					p->vel[1] = -vec[0]*ptype->offsetspread;
					p->org[0] = vec[1]*ptype->areaspread;
					p->org[1] = -vec[0]*ptype->areaspread;
				}
				else
				{
					p->vel[0] = -vec[1]*ptype->offsetspread;
					p->vel[1] = vec[0]*ptype->offsetspread;
					p->org[0] = -vec[1]*ptype->areaspread;
					p->org[1] = vec[0]*ptype->areaspread;
				}

				p->vel[0] += vec[0]*veladd+crandom()*randvel;
				p->vel[1] += vec[1]*veladd+crandom()*randvel;
				p->vel[2] = vec[2]*veladd+crandom()*randvelvert;

				p->org[0] += start[0];
				p->org[1] += start[1];
				p->org[2] = start[2];
				break;
			case SM_SPIRAL:
				{
					float tsin, tcos;
					float tright, tup;

					tcos = cos(len*tdegree+sdegree);
					tsin = sin(len*tdegree+sdegree);

					tright = tcos*ptype->areaspread;
					tup = tsin*ptype->areaspread;

					p->org[0] = start[0] + right[0]*tright + up[0]*tup;
					p->org[1] = start[1] + right[1]*tright + up[1]*tup;
					p->org[2] = start[2] + right[2]*tright + up[2]*tup;

					tright = tcos*ptype->offsetspread;
					tup = tsin*ptype->offsetspread;

					p->vel[0] = vec[0]*veladd+crandom()*randvel + right[0]*tright + up[0]*tup;
					p->vel[1] = vec[1]*veladd+crandom()*randvel + right[1]*tright + up[1]*tup;
					p->vel[2] = vec[2]*veladd+crandom()*randvelvert + right[2]*tright + up[2]*tup;
				}
				break;
			// TODO: directionalize SM_BALL/SM_CIRCLE/SM_DISTBALL
			case SM_BALL:
				p->org[0] = crandom();
				p->org[1] = crandom();
				p->org[2] = crandom();
				VectorNormalize(p->org);
				VectorScale(p->org, frandom(), p->org);

				p->vel[0] = vec[0]*veladd+crandom()*randvel + p->org[0]*ptype->offsetspread;
				p->vel[1] = vec[1]*veladd+crandom()*randvel + p->org[1]*ptype->offsetspread;
				p->vel[2] = vec[2]*veladd+crandom()*randvelvert + p->org[2]*ptype->offsetspreadvert;

				p->org[0] = p->org[0]*ptype->areaspread + start[0];
				p->org[1] = p->org[1]*ptype->areaspread + start[1];
				p->org[2] = p->org[2]*ptype->areaspreadvert + start[2];
				break;

			case SM_CIRCLE:
				{
					float tsin, tcos;

					tcos = cos(len*tdegree)*ptype->areaspread;
					tsin = sin(len*tdegree)*ptype->areaspread;

					p->org[0] = start[0] + right[0]*tcos + up[0]*tsin + vstep[0] * (len*tdegree);
					p->org[1] = start[1] + right[1]*tcos + up[1]*tsin + vstep[1] * (len*tdegree);
					p->org[2] = start[2] + right[2]*tcos + up[2]*tsin + vstep[2] * (len*tdegree)*50;

					tcos = cos(len*tdegree)*ptype->offsetspread;
					tsin = sin(len*tdegree)*ptype->offsetspread;

					p->vel[0] = vec[0]*veladd+crandom()*randvel + right[0]*tcos + up[0]*tsin;
					p->vel[1] = vec[1]*veladd+crandom()*randvel + right[1]*tcos + up[1]*tsin;
					p->vel[2] = vec[2]*veladd+crandom()*randvelvert + right[2]*tcos + up[2]*tsin;
				}
				break;

			case SM_DISTBALL:
				{
					float rdist;

					rdist = ptype->spawnparam2 - crandom()*(1-(crandom() * ptype->spawnparam1));

					// this is a strange spawntype, which is based on the fact that
					// crandom()*crandom() provides something similar to an exponential
					// probability curve
					p->org[0] = crandom();
					p->org[1] = crandom();
					p->org[2] = crandom();

					VectorNormalize(p->org);
					VectorScale(p->org, rdist, p->org);

					p->vel[0] = vec[0]*veladd+crandom()*randvel + p->org[0]*ptype->offsetspread;
					p->vel[1] = vec[1]*veladd+crandom()*randvel + p->org[1]*ptype->offsetspread;
					p->vel[2] = vec[2]*veladd+crandom()*randvelvert + p->org[2]*ptype->offsetspreadvert;

					p->org[0] = p->org[0]*ptype->areaspread + start[0];
					p->org[1] = p->org[1]*ptype->areaspread + start[1];
					p->org[2] = p->org[2]*ptype->areaspreadvert + start[2];
				}
				break;
			default:
				p->org[0] = crandom();
				p->org[1] = crandom();
				p->org[2] = crandom();

				p->vel[0] = vec[0]*veladd+crandom()*randvel + p->org[0]*ptype->offsetspread;
				p->vel[1] = vec[1]*veladd+crandom()*randvel + p->org[1]*ptype->offsetspread;
				p->vel[2] = vec[2]*veladd+crandom()*randvelvert + p->org[2]*ptype->offsetspreadvert;

				p->org[0] = p->org[0]*ptype->areaspread + start[0];
				p->org[1] = p->org[1]*ptype->areaspread + start[1];
				p->org[2] = p->org[2]*ptype->areaspreadvert + start[2];
				break;
			}
		}

		VectorAdd (start, vstep, start);

		if (ptype->countrand)
		{
			float rstep = frandom() / ptype->countrand;
			VectorMA(start, rstep, vec, start);
			step += rstep;
		}

		p->die = particletime + ptype->die - p->die;
	}

	if (ts)
	{
		ts->state1.lastdist = len;

		// update beamseg list
		if (ptype->looks.type == PT_BEAM)
		{
			if (b)
			{
				if (ptype->beams)
				{
					if (ts->lastbeam)
					{
						b->next = ts->lastbeam->next;
						ts->lastbeam->next = bfirst;
						ts->lastbeam->flags &= ~BS_LASTSEG;
					}
					else
					{
						b->next = ptype->beams;
						ptype->beams = bfirst;
					}
				}
				else
				{
					ptype->beams = bfirst;
					b->next = NULL;
				}

				b->flags |= BS_LASTSEG;
				ts->lastbeam = b;
			}

			if ((!free_particles || !free_beams) && ts->lastbeam)
			{
				ts->lastbeam->flags &= ~BS_LASTSEG;
				ts->lastbeam->flags |= BS_NODRAW;
				ts->lastbeam = NULL;
			}
		}
	}
	else if (ptype->looks.type == PT_BEAM)
	{
		if (b)
		{
			b->flags |= BS_NODRAW;
			b->next = ptype->beams;
			ptype->beams = bfirst;
		}
	}

	// maintain run list
	if (!(ptype->state & PS_INRUNLIST))
	{
		ptype->nexttorun = part_run_list;
		part_run_list = ptype;
		ptype->state |= PS_INRUNLIST;
	}

	return;
}

static int PScript_ParticleTrail (vec3_t startpos, vec3_t end, int type, trailstate_t **tsk)
{
	part_type_t *ptype = &part_type[type];

	if (type < 0 || type >= numparticletypes)
		return 1;	//bad value

	if (!ptype->loaded)
		return 1;

	// inwater check, switch only once
	if (r_part_contentswitch.value && ptype->inwater >= 0)
	{
		int cont;
		cont = cl.worldmodel->funcs.PointContents(cl.worldmodel, startpos);

		if (cont & FTECONTENTS_WATER)
			ptype = &part_type[ptype->inwater];
	}

	P_ParticleTrailDraw (startpos, end, ptype, tsk);
	return 0;
}

static void PScript_ParticleTrailIndex (vec3_t start, vec3_t end, int color, int crnd, trailstate_t **tsk)
{
	part_type[pe_defaulttrail].colorindex = color;
	part_type[pe_defaulttrail].colorrand = crnd;
	P_ParticleTrail(start, end, pe_defaulttrail, tsk);
}

vec3_t pright, pup;
static float pframetime;
#ifdef RGLQUAKE
static void GL_DrawTexturedParticle(int count, particle_t **plist, plooks_t *type)
{
	particle_t *p;
	float x,y;
	float scale;


	qglEnable(GL_TEXTURE_2D);
	GL_Bind(type->texturenum);
	APPLYBLEND(type->blendmode);
	qglShadeModel(GL_FLAT);
	qglBegin(GL_QUADS);


	while (count--)
	{
		p = *plist++;

		if (type->scalefactor == 1)
		{
			scale = (p->org[0] - r_origin[0])*vpn[0] + (p->org[1] - r_origin[1])*vpn[1]
				+ (p->org[2] - r_origin[2])*vpn[2];
			scale = (scale*p->scale)*(type->invscalefactor) + p->scale * (type->scalefactor*250);
			if (scale < 20)
				scale = 0.25;
			else
				scale = 0.25 + scale * 0.001;
		}
		else
			scale = 1;

		qglColor4f (p->rgb[0],
					p->rgb[1],
					p->rgb[2],
					p->alpha);

		if (p->angle)
		{
			x = sin(p->angle)*scale;
			y = cos(p->angle)*scale;

			qglTexCoord2f(p->s1,p->t1);
			qglVertex3f (p->org[0] - x*pright[0] - y*pup[0], p->org[1] - x*pright[1] - y*pup[1], p->org[2] - x*pright[2] - y*pup[2]);
			qglTexCoord2f(p->s1,p->t2);
			qglVertex3f (p->org[0] - y*pright[0] + x*pup[0], p->org[1] - y*pright[1] + x*pup[1], p->org[2] - y*pright[2] + x*pup[2]);
			qglTexCoord2f(p->s2,p->t2);
			qglVertex3f (p->org[0] + x*pright[0] + y*pup[0], p->org[1] + x*pright[1] + y*pup[1], p->org[2] + x*pright[2] + y*pup[2]);
			qglTexCoord2f(p->s2,p->t1);
			qglVertex3f (p->org[0] + y*pright[0] - x*pup[0], p->org[1] + y*pright[1] - x*pup[1], p->org[2] + y*pright[2] - x*pup[2]);
		}
		else
		{
			qglTexCoord2f(p->s1,p->t1);
			qglVertex3f (p->org[0] - scale*pup[0], p->org[1] - scale*pup[1], p->org[2] - scale*pup[2]);
			qglTexCoord2f(p->s1,p->t2);
			qglVertex3f (p->org[0] - scale*pright[0], p->org[1] - scale*pright[1], p->org[2] - scale*pright[2]);
			qglTexCoord2f(p->s2,p->t2);
			qglVertex3f (p->org[0] + scale*pup[0], p->org[1] + scale*pup[1], p->org[2] + scale*pup[2]);
			qglTexCoord2f(p->s2,p->t1);
			qglVertex3f (p->org[0] + scale*pright[0], p->org[1] + scale*pright[1], p->org[2] + scale*pright[2]);
		}
	}
	qglEnd();
}


static void GL_DrawSketchParticle(int count, particle_t **plist, plooks_t *type)
{
	particle_t *p;
	float x,y;
	float scale;

	int quant;

	qglDisable(GL_TEXTURE_2D);
//	if (type->blendmode == BM_ADD)		//addative
//		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
//	else if (type->blendmode == BM_SUBTRACT)	//subtractive
//		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//	else
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglShadeModel(GL_SMOOTH);
	qglBegin(GL_LINES);

	while (count--)
	{
		p = *plist++;

		scale = (p->org[0] - r_origin[0])*vpn[0] + (p->org[1] - r_origin[1])*vpn[1]
			+ (p->org[2] - r_origin[2])*vpn[2];
		scale = (scale*p->scale)*(type->invscalefactor) + p->scale * (type->scalefactor*250);
		if (scale < 20)
			scale = 0.25;
		else
			scale = 0.25 + scale * 0.001;

		qglColor4f (p->rgb[0]/2,
					p->rgb[1]/2,
					p->rgb[2]/2,
					p->alpha*2);

		quant = scale;

		if (p->angle)
		{
			x = sin(p->angle)*scale;
			y = cos(p->angle)*scale;

			qglVertex3f (p->org[0] - x*pright[0] - y*pup[0], p->org[1] - x*pright[1] - y*pup[1], p->org[2] - x*pright[2] - y*pup[2]);
			qglVertex3f (p->org[0] + x*pright[0] + y*pup[0], p->org[1] + x*pright[1] + y*pup[1], p->org[2] + x*pright[2] + y*pup[2]);
			qglVertex3f (p->org[0] + y*pright[0] - x*pup[0], p->org[1] + y*pright[1] - x*pup[1], p->org[2] + y*pright[2] - x*pup[2]);
			qglVertex3f (p->org[0] - y*pright[0] + x*pup[0], p->org[1] - y*pright[1] + x*pup[1], p->org[2] - y*pright[2] + x*pup[2]);
		}
		else
		{
			qglVertex3f (p->org[0] - scale*pup[0], p->org[1] - scale*pup[1], p->org[2] - scale*pup[2]);
			qglVertex3f (p->org[0] + scale*pup[0], p->org[1] + scale*pup[1], p->org[2] + scale*pup[2]);
			qglVertex3f (p->org[0] + scale*pright[0], p->org[1] + scale*pright[1], p->org[2] + scale*pright[2]);
			qglVertex3f (p->org[0] - scale*pright[0], p->org[1] - scale*pright[1], p->org[2] - scale*pright[2]);
		}
	}
	qglEnd();
}

static void GL_DrawTrifanParticle(int count, particle_t **plist, plooks_t *type)
{
	particle_t *p;
	int i;
	vec3_t v;
	float scale;

	qglDisable(GL_TEXTURE_2D);
	APPLYBLEND(type->blendmode);
	qglShadeModel(GL_SMOOTH);

	while (count--)
	{
		p = *plist++;

		scale = (p->org[0] - r_origin[0])*vpn[0] + (p->org[1] - r_origin[1])*vpn[1]
			+ (p->org[2] - r_origin[2])*vpn[2];
		scale = (scale*p->scale)*(type->invscalefactor) + p->scale * (type->scalefactor*250);
		if (scale < 20)
			scale = 0.05;
		else
			scale = 0.05 + scale * 0.0001;
	/*
		if ((p->vel[0]*p->vel[0]+p->vel[1]*p->vel[1]+p->vel[2]*p->vel[2])*2*scale > 30*30)
			scale = 1+1/30/Length(p->vel)*2;*/

		qglBegin (GL_TRIANGLE_FAN);
		qglColor4f (p->rgb[0],
					p->rgb[1],
					p->rgb[2],
					p->alpha);
		qglVertex3fv (p->org);
		qglColor4f (p->rgb[0]/2,
					p->rgb[1]/2,
					p->rgb[2]/2,
					0);
		for (i=7 ; i>=0 ; i--)
		{
			v[0] = p->org[0] - p->vel[0]*scale + vright[0]*cost[i%7]*p->scale + vup[0]*sint[i%7]*p->scale;
			v[1] = p->org[1] - p->vel[1]*scale + vright[1]*cost[i%7]*p->scale + vup[1]*sint[i%7]*p->scale;
			v[2] = p->org[2] - p->vel[2]*scale + vright[2]*cost[i%7]*p->scale + vup[2]*sint[i%7]*p->scale;
			qglVertex3fv (v);
		}
		qglEnd ();
	}
}

static void GL_DrawLineSparkParticle(int count, particle_t **plist, plooks_t *type)
{
	particle_t *p;

	qglDisable(GL_TEXTURE_2D);
	APPLYBLEND(type->blendmode);
	qglShadeModel(GL_SMOOTH);
	qglBegin(GL_LINES);
	
	while (count--)
	{
		p = *plist++;

		qglColor4f (p->rgb[0],
					p->rgb[1],
					p->rgb[2],
					p->alpha);
		qglVertex3f (p->org[0], p->org[1], p->org[2]);

		qglColor4f (p->rgb[0],
					p->rgb[1],
					p->rgb[2],
					0);
		qglVertex3f (p->org[0]-p->vel[0]/10, p->org[1]-p->vel[1]/10, p->org[2]-p->vel[2]/10);
	}
	qglEnd();
}

static void GL_DrawTexturedSparkParticle(int count, particle_t **plist, plooks_t *type)
{
	particle_t *p;
	vec3_t v, cr, o2, point;

	qglEnable(GL_TEXTURE_2D);
	GL_Bind(type->texturenum);
	APPLYBLEND(type->blendmode);
	qglShadeModel(GL_SMOOTH);
	qglBegin(GL_QUADS);


	while(count--)
	{
		p = *plist++;

		qglColor4f (p->rgb[0],
					p->rgb[1],
					p->rgb[2],
					p->alpha);

		VectorSubtract(r_refdef.vieworg, p->org, v);
		CrossProduct(v, p->vel, cr);
		VectorNormalize(cr);

		VectorMA(p->org, -p->scale/2, cr, point);
		qglTexCoord2f(p->s1, p->t1);
		qglVertex3fv(point);
		VectorMA(p->org, p->scale/2, cr, point);
		qglTexCoord2f(p->s1, p->t2);
		qglVertex3fv(point);


		VectorMA(p->org, 0.1, p->vel, o2);

		VectorSubtract(r_refdef.vieworg, o2, v);
		CrossProduct(v, p->vel, cr);
		VectorNormalize(cr);

		VectorMA(o2, p->scale/2, cr, point);
		qglTexCoord2f(p->s2, p->t2);
		qglVertex3fv(point);
		VectorMA(o2, -p->scale/2, cr, point);
		qglTexCoord2f(p->s2, p->t1);
		qglVertex3fv(point);
	}
	qglEnd();
}

static void GL_DrawSketchSparkParticle(int count, particle_t **plist, plooks_t *type)
{
	particle_t *p;

	qglDisable(GL_TEXTURE_2D);
	APPLYBLEND(type->blendmode);
	qglShadeModel(GL_SMOOTH);
	qglBegin(GL_LINES);

	while(count--)
	{
		p = *plist++;
		qglColor4f (p->rgb[0],
					p->rgb[1],
					p->rgb[2],
					p->alpha);
		qglVertex3f (p->org[0], p->org[1], p->org[2]);

		qglColor4f (p->rgb[0],
					p->rgb[1],
					p->rgb[2],
					0);
		qglVertex3f (p->org[0]-p->vel[0]/10, p->org[1]-p->vel[1]/10, p->org[2]-p->vel[2]/10);
	}
	qglEnd();
}

static void GL_DrawParticleBeam_Textured(int count, beamseg_t **blist, plooks_t *type)
{
	beamseg_t *b;
	vec3_t v, point;
	vec3_t cr;
	beamseg_t *c;
	particle_t *p;
	particle_t *q;
	float ts;

	qglEnable(GL_TEXTURE_2D);
	GL_Bind(type->texturenum);
	APPLYBLEND(type->blendmode);
	qglShadeModel(GL_SMOOTH);
	qglBegin(GL_QUADS);
	
	while(count--)
	{
		b = *blist++;

		c = b->next;

		q = c->p;
//		if (!q)
//			continue;

		p = b->p;
//		if (!p)
//			continue;

		qglColor4f(q->rgb[0],
				  q->rgb[1],
				  q->rgb[2],
				  q->alpha);
	//	qglBegin(GL_LINE_LOOP);
		VectorSubtract(r_refdef.vieworg, q->org, v);
		VectorNormalize(v);
		CrossProduct(c->dir, v, cr);
		ts = c->texture_s*q->angle + particletime*q->rotationspeed;

		VectorMA(q->org, -q->scale, cr, point);
		qglTexCoord2f(ts, p->t1);
		qglVertex3fv(point);
		VectorMA(q->org, q->scale, cr, point);
		qglTexCoord2f(ts, p->t2);
		qglVertex3fv(point);

		qglColor4f(p->rgb[0],
				  p->rgb[1],
				  p->rgb[2],
				  p->alpha);

		VectorSubtract(r_refdef.vieworg, p->org, v);
		VectorNormalize(v);
		CrossProduct(b->dir, v, cr); // replace with old p->dir?
		ts = b->texture_s*p->angle + particletime*p->rotationspeed;

		VectorMA(p->org, p->scale, cr, point);
		qglTexCoord2f(ts, p->t2);
		qglVertex3fv(point);
		VectorMA(p->org, -p->scale, cr, point);
		qglTexCoord2f(ts, p->t1);
		qglVertex3fv(point);
	}
	qglEnd();
}

static void GL_DrawParticleBeam_Untextured(int count, beamseg_t **blist, plooks_t *type)
{
	vec3_t v;
	vec3_t cr;
	beamseg_t *c;
	particle_t *p;
	particle_t *q;
	beamseg_t *b;

	vec3_t point[4];


	qglDisable(GL_TEXTURE_2D);
	APPLYBLEND(type->blendmode);
	qglShadeModel(GL_SMOOTH);
	qglBegin(GL_QUADS);

	while(count--)
	{
		b = *blist++;

		c = b->next;

		q = c->p;
	//	if (!q)
	//		continue;

		p = b->p;
	//	if (!p)
	//		continue;

		VectorSubtract(r_refdef.vieworg, q->org, v);
		VectorNormalize(v);
		CrossProduct(c->dir, v, cr);

		VectorMA(q->org, -q->scale, cr, point[0]);
		VectorMA(q->org, q->scale, cr, point[1]);


		VectorSubtract(r_refdef.vieworg, p->org, v);
		VectorNormalize(v);
		CrossProduct(b->dir, v, cr); // replace with old p->dir?

		VectorMA(p->org, p->scale, cr, point[2]);
		VectorMA(p->org, -p->scale, cr, point[3]);


		//one half
		//back out
		//back in
		//front in
		//front out
		qglColor4f(q->rgb[0],
			  q->rgb[1],
			  q->rgb[2],
			  0);
		qglVertex3fv(point[0]);
		qglColor4f(q->rgb[0],
			  q->rgb[1],
			  q->rgb[2],
			  q->alpha);
		qglVertex3fv(q->org);

		qglColor4f(p->rgb[0],
			  p->rgb[1],
			  p->rgb[2],
			  p->alpha);
		qglVertex3fv(p->org);
		qglColor4f(p->rgb[0],
			  p->rgb[1],
			  p->rgb[2],
			  0);
		qglVertex3fv(point[3]);

		//front out
		//front in
		//back in
		//back out
		qglColor4f(p->rgb[0],
			  p->rgb[1],
			  p->rgb[2],
			  0);
		qglVertex3fv(point[2]);
		qglColor4f(p->rgb[0],
			  p->rgb[1],
			  p->rgb[2],
			  p->alpha);
		qglVertex3fv(p->org);

		qglColor4f(q->rgb[0],
			  q->rgb[1],
			  q->rgb[2],
			  q->alpha);
		qglVertex3fv(q->org);
		qglColor4f(q->rgb[0],
			  q->rgb[1],
			  q->rgb[2],
			  0);
		qglVertex3fv(point[1]);
	}
	qglEnd();
}

static void GL_DrawClippedDecal(int count, clippeddecal_t **dlist, plooks_t *type)
{
	clippeddecal_t *d;

	qglEnable(GL_TEXTURE_2D);
	GL_Bind(type->texturenum);
	APPLYBLEND(type->blendmode);
	qglShadeModel(GL_SMOOTH);

//	qglDisable(GL_TEXTURE_2D);
//	qglBegin(GL_LINE_LOOP);

	qglBegin(GL_TRIANGLES);

	while (count--)
	{
		d = *dlist++;

		qglColor4f(d->rgb[0],
			  d->rgb[1],
			  d->rgb[2],
			  d->alpha);

		qglTexCoord2fv(d->texcoords[0]);
		qglVertex3fv(d->vertex[0]);
		qglTexCoord2fv(d->texcoords[1]);
		qglVertex3fv(d->vertex[1]);
		qglTexCoord2fv(d->texcoords[2]);
		qglVertex3fv(d->vertex[2]);
	}
	qglEnd();
}

#endif
#ifdef SWQUAKE
static void SWD_DrawParticleSpark(int count, particle_t **plist, plooks_t *type)
{
	float speed;
	vec3_t src, dest;
	particle_t *p;

	int r,g,b;	//if you have a cpu with mmx, good for you...

	while (count--)
	{
		p = *plist++;

		r = p->rgb[0]*255;
		if (r < 0)
			r = 0;
		else if (r > 255)
			r = 255;
		g = p->rgb[1]*255;
		if (g < 0)
			g = 0;
		else if (g > 255)
			g = 255;
		b = p->rgb[2]*255;
		if (b < 0)
			b = 0;
		else if (b > 255)
			b = 255;
		p->color = GetPaletteIndex(r, g, b);

		speed = Length(p->vel);
		if ((speed) < 1)
		{
			VectorCopy(p->org, src);
			VectorCopy(p->org, dest);
		}
		else
		{	//causes flickers with lower vels (due to bouncing in physics)
			if (speed < 50)
				speed *= 50/speed;
			VectorMA(p->org, 2.5/(speed), p->vel, src);
			VectorMA(p->org, -2.5/(speed), p->vel, dest);
		}

		D_DrawSparkTrans(src, dest, p->alpha, p->color, type->blendmode);
	}
}
static void SWD_DrawParticleBlob(int count, particle_t **plist, plooks_t *type)
{
	particle_t *p;
	int r,g,b;	//This really shouldn't be like this. Pitty the 32 bit renderer...

	while(count--)
	{
		p = *plist++;

		r = p->rgb[0]*255;
		if (r < 0)
			r = 0;
		else if (r > 255)
			r = 255;
		g = p->rgb[1]*255;
		if (g < 0)
			g = 0;
		else if (g > 255)
			g = 255;
		b = p->rgb[2]*255;
		if (b < 0)
			b = 0;
		else if (b > 255)
			b = 255;
		p->color = GetPaletteIndex(r, g, b);
		D_DrawParticleTrans(p->org, p->alpha, p->scale, p->color, type->blendmode);
	}
}
static void SWD_DrawParticleBeam(int count, beamseg_t **blist, plooks_t *type)
{
	int r,g,b;	//if you have a cpu with mmx, good for you...
	beamseg_t *beam;
	beamseg_t *c;
	particle_t *p;
	particle_t *q;

//	if (!b->next)
//		return;

	while(count--)
	{
		beam = *blist++;

		c = beam->next;

		q = c->p;
	//	if (!q)
	//		return;

		p = beam->p;

		r = p->rgb[0]*255;
		if (r < 0)
			r = 0;
		else if (r > 255)
			r = 255;
		g = p->rgb[1]*255;
		if (g < 0)
			g = 0;
		else if (g > 255)
			g = 255;
		b = p->rgb[2]*255;
		if (b < 0)
			b = 0;
		else if (b > 255)
			b = 255;
		p->color = GetPaletteIndex(r, g, b);
		D_DrawSparkTrans(p->org, q->org, p->alpha, p->color, type->blendmode);
	}
}
#endif

void PScript_DrawParticleTypes (void (*texturedparticles)(int count, particle_t **,plooks_t*), void (*sparklineparticles)(int count, particle_t **,plooks_t*), void (*sparkfanparticles)(int count, particle_t **,plooks_t*), void (*sparktexturedparticles)(int count, particle_t **,plooks_t*), void (*beamparticlest)(int count, beamseg_t**,plooks_t*), void (*beamparticlesut)(int count, beamseg_t**,plooks_t*), void (*drawdecalparticles)(int count, clippeddecal_t**,plooks_t*))
{
	RSpeedMark();

	qboolean (*tr) (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal);
	void *pdraw, *bdraw;

	vec3_t oldorg;
	vec3_t stop, normal;
	part_type_t *type, *lastvalidtype;
	particle_t		*p, *kill;
	clippeddecal_t *d, *dkill;
	ramp_t *ramp;
	float grav;
	vec3_t friction;
	float dist;
	particle_t *kill_list, *kill_first;	//the kill list is to stop particles from being freed and reused whilst still in this loop
										//which is bad because beams need to find out when particles died. Reuse can do wierd things.
										//remember that they're not drawn instantly either.
	beamseg_t *b, *bkill;

	int traces=r_particle_tracelimit.value;
	int rampind;

	if (r_plooksdirty)
	{
		int i, j;
		for (i = 0; i < numparticletypes; i++)
		{
			//set the fallback
			part_type[i].slooks = &part_type[i].looks;
			for (j = i-1; j-- > 0;)
			{
				if (!memcmp(&part_type[i].looks, &part_type[j].looks, sizeof(plooks_t)))
				{
					part_type[i].slooks = part_type[j].slooks;
					break;
				}
			}
		}
		r_plooksdirty = false;
	}

	pframetime = host_frametime;
	if (cl.paused || r_secondaryview)
		pframetime = 0;

	VectorScale (vup, 1.5, pup);
	VectorScale (vright, 1.5, pright);
#ifdef SWQUAKE
	VectorScale (vright, xscaleshrink, r_pright);
	VectorScale (vup, yscaleshrink, r_pup);
	VectorCopy (vpn, r_ppn);
#endif

#ifdef Q2BSPS
	if (cl.worldmodel->fromgame == fg_quake2 || cl.worldmodel->fromgame == fg_quake3)
		tr = Q2TraceLineN;
	else
#endif
		tr = TraceLineN;

	kill_list = kill_first = NULL;

	// reassign drawing methods by cvars
	if (r_part_beams_textured.value < 0)
		beamparticlest = NULL;
	else if (!r_part_beams_textured.value)
		beamparticlest = beamparticlesut;

	if (r_part_beams.value < 0)
		beamparticlesut = NULL;
	else if (!r_part_beams.value)
	{
		beamparticlest = NULL;
		beamparticlesut = NULL;
	}

	if (r_part_sparks_textured.value < 0)
		sparktexturedparticles = NULL;
	else if (!r_part_sparks_textured.value)
		sparktexturedparticles = sparklineparticles;

	if (r_part_sparks_trifan.value < 0)
		sparkfanparticles = NULL;
	else if (!r_part_sparks_trifan.value)
		sparkfanparticles = sparklineparticles;

	if (r_part_sparks.value < 0)
		sparklineparticles = NULL;
	else if (!r_part_sparks.value)
	{
		sparktexturedparticles = NULL;
		sparkfanparticles = NULL;
		sparklineparticles = NULL;
	}

	for (type = part_run_list, lastvalidtype = NULL; type != NULL; type = type->nexttorun)
	{
		if (type->clippeddecals)
		{
			for ( ;; )
			{
				dkill = type->clippeddecals;
				if (dkill && dkill->die < particletime)
				{
					type->clippeddecals = dkill->next;
					dkill->next = free_decals;
					free_decals = dkill;
					continue;
				}
				break;
			}
			for (d=type->clippeddecals ; d ; d=d->next)
			{
				for ( ;; )
				{
					dkill = d->next;
					if (dkill && dkill->die < particletime)
					{
						d->next = dkill->next;
						dkill->next = free_decals;
						free_decals = dkill;
						continue;
					}
					break;
				}



				switch (type->rampmode)
				{
				case RAMP_ABSOLUTE:
					rampind = (int)(type->rampindexes * (type->die - (d->die - particletime)) / type->die);
					if (rampind >= type->rampindexes)
						rampind = type->rampindexes - 1;
					ramp = type->ramp + rampind;
					VectorCopy(ramp->rgb, d->rgb);
					d->alpha = ramp->alpha;
					break;
				case RAMP_DELTA:	//particle ramps
					ramp = type->ramp + (int)(type->rampindexes * (type->die - (d->die - particletime)) / type->die);
					VectorMA(d->rgb, pframetime, ramp->rgb, d->rgb);
					d->alpha -= pframetime*ramp->alpha;
					break;
				case RAMP_NONE:	//particle changes acording to it's preset properties.
					if (particletime < (d->die-type->die+type->rgbchangetime))
					{
						d->rgb[0] += pframetime*type->rgbchange[0];
						d->rgb[1] += pframetime*type->rgbchange[1];
						d->rgb[2] += pframetime*type->rgbchange[2];
					}
					d->alpha += pframetime*type->alphachange;
				}

				drawdecalparticles(1, &d, &type->looks);
			}
		}

		bdraw = NULL;
		pdraw = NULL;

		// set drawing methods by type and cvars and hope branch
		// prediction takes care of the rest
		switch(type->looks.type)
		{
		case PT_BEAM:
			if (*type->texname)
				bdraw = beamparticlest;
			else
				bdraw = beamparticlesut;
			break;
		case PT_DECAL:
			break;
		case PT_NORMAL:
			pdraw = texturedparticles;
			break;
		case PT_SPARK:
			pdraw = sparklineparticles;
			break;
		case PT_SPARKFAN:
			pdraw = sparkfanparticles;
			break;
		case PT_TEXTUREDSPARK:
			pdraw = sparktexturedparticles;
			break;
		}

		if (!type->die)
		{
			while ((p=type->particles))
			{
				if (pdraw)
					RQ_AddDistReorder(pdraw, p, type->slooks, p->org);

				// make sure emitter runs at least once
				if (type->emit >= 0 && type->emitstart <= 0)
					P_RunParticleEffectType(p->org, p->vel, 1, type->emit);

				// make sure stain effect runs
				if (type->stains && r_bloodstains.value)
				{
					if (traces-->0&&tr(oldorg, p->org, stop, normal))
					{
						R_AddStain(stop,	(p->rgb[1]*-10+p->rgb[2]*-10),
											(p->rgb[0]*-10+p->rgb[2]*-10),
											(p->rgb[0]*-10+p->rgb[1]*-10),
											30*p->alpha*type->stains);
					}
				}

				type->particles = p->next;
//				p->next = free_particles;
//				free_particles = p;
				p->next = kill_list;
				kill_list = p;
				if (!kill_first) // branch here is probably faster than list traversal later
					kill_first = p;
			}

			if (type->beams)
			{
				b = type->beams;
			}

			while ((b=type->beams) && (b->flags & BS_DEAD))
			{
				type->beams = b->next;
				b->next = free_beams;
				free_beams = b;
			}

			while (b)
			{
				if (!(b->flags & BS_NODRAW))
				{
					// no BS_NODRAW implies b->next != NULL
					// BS_NODRAW should imply b->next == NULL or b->next->flags & BS_DEAD
					VectorCopy(b->next->p->org, stop);
					VectorCopy(b->p->org, oldorg);
					VectorSubtract(stop, oldorg, b->next->dir);
					VectorNormalize(b->next->dir);
					if (bdraw)
					{
						VectorAdd(stop, oldorg, stop);
						VectorScale(stop, 0.5, stop);

						RQ_AddDistReorder(bdraw, b, type->slooks, stop);
					}
				}

				// clean up dead entries ahead of current
				for ( ;; )
				{
					bkill = b->next;
					if (bkill && (bkill->flags & BS_DEAD))
					{
						b->next = bkill->next;
						bkill->next = free_beams;
						free_beams = bkill;
						continue;
					}
					break;
				}

				b->flags |= BS_DEAD;
				b = b->next;
			}

			continue;
		}

		//kill off early ones.
		if (type->emittime < 0)
		{
			for ( ;; )
			{
				kill = type->particles;
				if (kill && kill->die < particletime)
				{
					P_DelinkTrailstate(&kill->state.trailstate);
					type->particles = kill->next;
					kill->next = kill_list;
					kill_list = kill;
					if (!kill_first)
						kill_first = kill;
					continue;
				}
				break;
			}
		}
		else
		{
			for ( ;; )
			{
				kill = type->particles;
				if (kill && kill->die < particletime)
				{
					type->particles = kill->next;
					kill->next = kill_list;
					kill_list = kill;
					if (!kill_first)
						kill_first = kill;
					continue;
				}
				break;
			}
		}

		grav = type->gravity*pframetime;
		VectorScale(type->friction, pframetime, friction);

		for (p=type->particles ; p ; p=p->next)
		{
			if (type->emittime < 0)
			{
				for ( ;; )
				{
					kill = p->next;
					if (kill && kill->die < particletime)
					{
						P_DelinkTrailstate(&kill->state.trailstate);
						p->next = kill->next;
						kill->next = kill_list;
						kill_list = kill;
						if (!kill_first)
							kill_first = kill;
						continue;
					}
					break;
				}
			}
			else
			{
				for ( ;; )
				{
					kill = p->next;
					if (kill && kill->die < particletime)
					{
						p->next = kill->next;
						kill->next = kill_list;
						kill_list = kill;
						if (!kill_first)
							kill_first = kill;
						continue;
					}
					break;
				}
			}

			VectorCopy(p->org, oldorg);
			if (type->flags & PT_VELOCITY)
			{
				p->org[0] += p->vel[0]*pframetime;
				p->org[1] += p->vel[1]*pframetime;
				p->org[2] += p->vel[2]*pframetime;
				if (type->flags & PT_FRICTION)
				{
					p->vel[0] -= friction[0]*p->vel[0];
					p->vel[1] -= friction[1]*p->vel[1];
					p->vel[2] -= friction[2]*p->vel[2];
				}
				p->vel[2] -= grav;
			}

			p->angle += p->rotationspeed*pframetime;

			switch (type->rampmode)
			{
			case RAMP_ABSOLUTE:
				rampind = (int)(type->rampindexes * (type->die - (p->die - particletime)) / type->die);
				if (rampind >= type->rampindexes)
					rampind = type->rampindexes - 1;
				ramp = type->ramp + rampind;
				VectorCopy(ramp->rgb, p->rgb);
				p->alpha = ramp->alpha;
				p->scale = ramp->scale;
				break;
			case RAMP_DELTA:	//particle ramps
				rampind = (int)(type->rampindexes * (type->die - (p->die - particletime)) / type->die);
				if (rampind >= type->rampindexes)
					rampind = type->rampindexes - 1;
				ramp = type->ramp + rampind;
				VectorMA(p->rgb, pframetime, ramp->rgb, p->rgb);
				p->alpha -= pframetime*ramp->alpha;
				p->scale += pframetime*ramp->scale;
				break;
			case RAMP_NONE:	//particle changes acording to it's preset properties.
				if (particletime < (p->die-type->die+type->rgbchangetime))
				{
					p->rgb[0] += pframetime*type->rgbchange[0];
					p->rgb[1] += pframetime*type->rgbchange[1];
					p->rgb[2] += pframetime*type->rgbchange[2];
				}
				p->alpha += pframetime*type->alphachange;
				p->scale += pframetime*type->scaledelta;
			}

			if (type->emit >= 0)
			{
				if (type->emittime < 0)
					P_ParticleTrail(oldorg, p->org, type->emit, &p->state.trailstate);
				else if (p->state.nextemit < particletime)
				{
					p->state.nextemit = particletime + type->emittime + frandom()*type->emitrand;
					P_RunParticleEffectType(p->org, p->vel, 1, type->emit);
				}
			}

			if (type->cliptype>=0 && r_bouncysparks.value)
			{
				if (traces-->0&&tr(oldorg, p->org, stop, normal))
				{
					if (type->stains && r_bloodstains.value)
						R_AddStain(stop,	p->rgb[1]*-10+p->rgb[2]*-10,
											p->rgb[0]*-10+p->rgb[2]*-10,
											p->rgb[0]*-10+p->rgb[1]*-10,
											30*p->alpha);

					if (part_type + type->cliptype == type)
					{	//bounce
						dist = DotProduct(p->vel, normal) * (-1-(rand()/(float)0x7fff)/2);

						VectorMA(p->vel, dist, normal, p->vel);
						VectorCopy(stop, p->org);
						p->vel[0] *= type->clipbounce;
						p->vel[1] *= type->clipbounce;
						p->vel[2] *= type->clipbounce;

						if (!*type->texname && Length(p->vel)<1000*pframetime && type->looks.type == PT_NORMAL)
							p->die = -1;
					}
					else
					{
						p->die = -1;
						VectorNormalize(p->vel);
						P_RunParticleEffectType(stop, p->vel, type->clipcount/part_type[type->cliptype].count, type->cliptype);
					}

					continue;
				}
			}
			else if (type->stains && r_bloodstains.value)
			{
				if (traces-->0&&tr(oldorg, p->org, stop, normal))
				{
					R_AddStain(stop,	(p->rgb[1]*-10+p->rgb[2]*-10),
										(p->rgb[0]*-10+p->rgb[2]*-10),
										(p->rgb[0]*-10+p->rgb[1]*-10),
										30*p->alpha*type->stains);
					p->die = -1;
					continue;
				}
			}

			if (pdraw)
				RQ_AddDistReorder((void*)pdraw, p, type->slooks, p->org);
		}

		// beams are dealt with here

		// kill early entries
		for ( ;; )
		{
			bkill = type->beams;
			if (bkill && (bkill->flags & BS_DEAD || bkill->p->die < particletime) && !(bkill->flags & BS_LASTSEG))
			{
				type->beams = bkill->next;
				bkill->next = free_beams;
				free_beams = bkill;
				continue;
			}
			break;
		}


		b = type->beams;
		if (b)
		{
			for ( ;; )
			{
				if (b->next)
				{
					// mark dead entries
					if (b->flags & (BS_LASTSEG|BS_DEAD|BS_NODRAW))
					{
						// kill some more dead entries
						for ( ;; )
						{
							bkill = b->next;
							if (bkill && (bkill->flags & BS_DEAD) && !(bkill->flags & BS_LASTSEG))
							{
								b->next = bkill->next;
								bkill->next = free_beams;
								free_beams = bkill;
								continue;
							}
							break;
						}

						if (!bkill) // have to check so we don't hit NULL->next
							continue;
					}
					else
					{
						if (!(b->next->flags & BS_DEAD))
						{
							VectorCopy(b->next->p->org, stop);
							VectorCopy(b->p->org, oldorg);
							VectorSubtract(stop, oldorg, b->next->dir);
							VectorNormalize(b->next->dir);
							if (bdraw)
							{
								VectorAdd(stop, oldorg, stop);
								VectorScale(stop, 0.5, stop);

								RQ_AddDistReorder(bdraw, b, type->slooks, stop);
							}
						}

	//					if (b->p->die < particletime)
	//						b->flags |= BS_DEAD;
					}
				}
				else
				{
					if (b->p->die < particletime) // end of the list check
						b->flags |= BS_DEAD;

					break;
				}

				if (b->p->die < particletime)
					b->flags |= BS_DEAD;

				b = b->next;
			}
		}

		// delete from run list if necessary
		if (!type->particles && !type->beams && !type->clippeddecals)
		{
//			if (!lastvalidtype)
//				part_run_list = type->nexttorun;
//			else
//				lastvalidtype->nexttorun = type->nexttorun;
//			type->state &= ~PS_INRUNLIST;
		}
		else
			lastvalidtype = type;
	}

	RSpeedEnd(RSPEED_PARTICLES);

	// lazy delete for particles is done here
	if (kill_list)
	{
		kill_first->next = free_particles;
		free_particles = kill_list;
	}

	particletime += pframetime;
}

static void PScript_FlushRenderer(void)
{
#ifdef RGLQUAKE
	qglDepthMask(0);	//primarily to stop close particles from obscuring each other
	qglDisable(GL_ALPHA_TEST);
	qglEnable (GL_BLEND);
	GL_TexEnv(GL_MODULATE);
	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#endif
}

/*
===============
R_DrawParticles
===============
*/
static void PScript_DrawParticles (void)
{
	RSpeedMark();

	P_AddRainParticles();
#if defined(RGLQUAKE)
	if (qrenderer == QR_OPENGL)
	{
		extern int gldepthfunc;
		extern cvar_t r_drawflat;

		P_FlushRenderer();

		if (qglPolygonOffset)
			qglPolygonOffset(-1, 0);
		qglEnable(GL_POLYGON_OFFSET_FILL);
		qglEnable(GL_BLEND);

		qglDepthFunc(gldepthfunc);

		qglDisable(GL_ALPHA_TEST);
		if (r_drawflat.value == 2)
			PScript_DrawParticleTypes(GL_DrawSketchParticle, GL_DrawSketchSparkParticle, GL_DrawSketchSparkParticle, GL_DrawSketchSparkParticle, GL_DrawParticleBeam_Textured, GL_DrawParticleBeam_Untextured, GL_DrawClippedDecal);
		else
			PScript_DrawParticleTypes(GL_DrawTexturedParticle, GL_DrawLineSparkParticle, GL_DrawTrifanParticle, GL_DrawTexturedSparkParticle, GL_DrawParticleBeam_Textured, GL_DrawParticleBeam_Untextured, GL_DrawClippedDecal);
		qglDisable(GL_POLYGON_OFFSET_FILL);



		RSpeedRemark();
		RQ_RenderBatchClear();
		RSpeedEnd(RSPEED_PARTICLESDRAW);

		qglEnable(GL_TEXTURE_2D);

		GL_TexEnv(GL_MODULATE);
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		qglDepthMask(1);
		return;
	}
#endif
#ifdef SWQUAKE
	if (qrenderer == QR_SOFTWARE)
	{
		PScript_DrawParticleTypes(SWD_DrawParticleBlob, SWD_DrawParticleSpark, SWD_DrawParticleSpark, SWD_DrawParticleSpark, SWD_DrawParticleBeam, SWD_DrawParticleBeam, NULL);

		RSpeedRemark();
		D_StartParticles();
		RQ_RenderDistAndClear();
		D_EndParticles();
		RSpeedEnd(RSPEED_PARTICLESDRAW);
		return;
	}
#endif
#if defined(D3DQUAKE)
	if (qrenderer == QR_DIRECT3D)
	{
		if (!D3D7_DrawParticles(particletime))
			D3D9_DrawParticles(particletime);
	}
#endif

	if (qrenderer)
	{
			RSpeedRemark();
			RQ_RenderDistAndClear();
			RSpeedEnd(RSPEED_PARTICLESDRAW);
	}
}


particleengine_t pe_script =
{
	"script",
	"fte",

	PScript_ParticleTypeForName,
	PScript_FindParticleType,

	PScript_RunParticleEffectTypeString,
	PScript_ParticleTrail,
	PScript_RunParticleEffectState,
	PScript_RunParticleWeather,
	PScript_RunParticleCube,
	PScript_RunParticleEffect,
	PScript_RunParticleEffect2,
	PScript_RunParticleEffect3,
	PScript_RunParticleEffect4,

	PScript_ParticleTrailIndex,
	PScript_EmitSkyEffectTris,
	PScript_InitParticles,
	PScript_Shutdown,
	PScript_DelinkTrailstate,
	PScript_ClearParticles,
	PScript_DrawParticles,
	PScript_FlushRenderer
};

#endif
#endif
