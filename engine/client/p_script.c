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

#ifdef GLQUAKE
#include "glquake.h"//hack
#endif
#include "shader.h"

#ifdef D3DQUAKE
//d3d is awkward
//we can't include two versions of header files
extern void *d3dexplosiontexture;
extern void *d3dballtexture;
#endif

#include "renderque.h"

#include "r_partset.h"

struct
{
	char *name;
	char **data;
} partset_list[] =
{
	{"none", NULL},
	R_PARTSET_BUILTINS
	{NULL}
};

extern qbyte *host_basepal;

extern particleengine_t pe_classic;
particleengine_t *fallback = NULL; //does this really need to be 'extern'?
#define FALLBACKBIAS 0x1000000

static int pt_pointfile = P_INVALID;
static int pe_default = P_INVALID;
static int pe_size2 = P_INVALID;
static int pe_size3 = P_INVALID;
static int pe_defaulttrail = P_INVALID;

static float psintable[256];

static void buildsintable(void)
{
	int i;
	for (i = 0; i < 256; i++)
		psintable[i] = sin((i*M_PI)/128);
}
#define sin(x) (psintable[(int)((x)*(128/M_PI)) & 255])
#define cos(x) (psintable[((int)((x)*(128/M_PI)) + 64) & 255])

typedef struct particle_s
{
	struct particle_s	*next;
	float		die;

// driver-usable fields
	vec3_t		org;
	vec4_t		rgba;
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

	vec4_t		rgba;
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
	shader_t *shader;

	float scalefactor;
	float invscalefactor;
	float stretch;
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
	char modelname[MAX_QPATH];

	model_t *model;
	float modelframestart;
	float modelframeend;
	float modelframerate;
	float modelalpha;

	vec3_t rgb;	//initial colour
	float alpha;
	vec3_t rgbchange;	//colour delta (per second)
	float alphachange;
	vec3_t rgbrand;		//random rgb colour to start with
	float alpharand;
	int colorindex;		//get colour from a palette
	int colorrand;		//and add up to this amount
	float rgbchangetime;//colour stops changing at this time
	vec3_t rgbrandsync;	//like rgbrand, but a single random value instead of separate (can mix)
	float scale;		//initial scale
	float scalerand;	//with up to this much extra
	float die, randdie;	//how long it lasts (plus some rand)
	float randomvel, randomvelvert; //random velocity (unaligned)
	float veladd;		//scale the incoming velocity by this much
	float orgadd;		//spawn the particle this far along its velocity direction
	float spawnvel, spawnvelvert; //spawn the particle with a velocity based upon its spawn type (generally so it flies outwards)

	float s1, t1, s2, t2;	//texture coords
	float texsstride;	//addition for s for each random slot.
	int randsmax;	//max times the stride can be added

	plooks_t *slooks;	//shared looks, so state switches don't apply between particles so much
	plooks_t looks;

	float spawntime;	//time limit for trails
	float spawnchance;	//if < 0, particles might not spawn so many

	float rotationstartmin, rotationstartrand;
	float rotationmin, rotationrand;

	float scaledelta;
	int countextra;
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
	int stainonimpact;

	vec3_t dl_rgb;
	float dl_radius;
	float dl_time;
	vec4_t dl_decay;
	vec3_t stain_rgb;
	float stain_radius;

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

#ifndef TYPESONLY

//triangle fan sparks use these. // defined but not used
//static double sint[7] = {0.000000, 0.781832,  0.974928,  0.433884, -0.433884, -0.974928, -0.781832};
//static double cost[7] = {1.000000, 0.623490, -0.222521, -0.900969, -0.900969, -0.222521,  0.623490};

#define crand() (rand()%32767/16383.5f-1)

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
static void R_ParticleDesc_Callback(struct cvar_s *var, char *oldvalue);

extern cvar_t r_particledesc;
extern cvar_t r_part_rain_quantity;
extern cvar_t r_particle_tracelimit;
extern cvar_t r_part_sparks;
extern cvar_t r_part_sparks_trifan;
extern cvar_t r_part_sparks_textured;
extern cvar_t r_part_beams;
extern cvar_t r_part_contentswitch;

static float particletime;

#define BUFFERVERTS 2048*4
static vecV_t pscriptverts[BUFFERVERTS];
static avec4_t pscriptcolours[BUFFERVERTS];
static vec2_t pscripttexcoords[BUFFERVERTS];
static index_t pscriptquadindexes[(BUFFERVERTS/4)*6];
static index_t pscripttriindexes[BUFFERVERTS];
static mesh_t pscriptmesh;
static mesh_t pscripttmesh;

static int numparticletypes;
static part_type_t *part_type;
static part_type_t *part_run_list;

static struct {
	char *oldn;
	char *newn;
} legacynames[] =
{
	{"t_rocket", "TR_ROCKET"},

	//{"t_blastertrail", "TR_BLASTERTRAIL"},
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
	ptype->assoc = P_INVALID;
	ptype->inwater = P_INVALID;
	ptype->cliptype = P_INVALID;
	ptype->emit = P_INVALID;

	if (oldlist)
	{
		if (part_run_list)
			part_run_list = (part_type_t*)((char*)part_run_list - (char*)oldlist + (char*)part_type);

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
	if (!ptype || !ptype->loaded)
	{
		if (fallback)
		{
			if (!strncmp(name, "classic_", 8))
				i = fallback->FindParticleType(name+8);
			else
				i = fallback->FindParticleType(name);
			if (i != P_INVALID)
				return i+FALLBACKBIAS;
		}
		return P_INVALID;
	}
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
	texnums_t tn;
	char *defaultshader;
	char *namepostfix;
	if (qrenderer == QR_NONE)
		return;

	ptype->model = NULL;

	if (*ptype->texname)
	{
		/*try and load the shader, fail if we would need to generate one*/
		ptype->looks.shader = R_RegisterCustom(ptype->texname, NULL, NULL);
	}
	else
		ptype->looks.shader = NULL;

	if (!ptype->looks.shader)
	{
		/*okay, so no shader, generate a shader that matches the legacy/shaderless mode*/
		switch(ptype->looks.blendmode)
		{
		case BM_BLEND:
		default:
			namepostfix = "_blend";
			defaultshader =
				"{\n"
					"nomipmaps\n"
					"{\n"
						"map $diffuse\n"
						"blendfunc blend\n"
						"rgbgen vertex\n"
						"alphagen vertex\n"
					"}\n"
//					"polygonoffset\n"
				"}\n"
				;
			break;
		case BM_BLENDCOLOUR:
			namepostfix = "_bc";
			defaultshader =
				"{\n"
					"nomipmaps\n"
					"{\n"
						"map $diffuse\n"
						"blendfunc GL_SRC_COLOR GL_ONE_MINUS_SRC_COLOR\n"
						"rgbgen vertex\n"
						"alphagen vertex\n"
					"}\n"
//					"polygonoffset\n"
				"}\n"
				;
			break;
		case BM_ADD:
			namepostfix = "_add";
			defaultshader =
				"{\n"
					"nomipmaps\n"
					"{\n"
						"map $diffuse\n"
						"blendfunc GL_SRC_ALPHA GL_ONE\n"
						"rgbgen vertex\n"
						"alphagen vertex\n"
					"}\n"
//					"polygonoffset\n"
				"}\n"
				;
			break;
		case BM_INVMOD:
			namepostfix = "_invmod";
			defaultshader =
				"{\n"
					"nomipmaps\n"
					"{\n"
						"map $diffuse\n"
						"blendfunc GL_ZERO GL_ONE_MINUS_SRC_COLOR\n"
						"rgbgen vertex\n"
						"alphagen vertex\n"
					"}\n"
//					"polygonoffset\n"
				"}\n"
				;
			break;
		case BM_SUBTRACT:
			namepostfix = "_sub";
			defaultshader =
				"{\n"
					"nomipmaps\n"
					"{\n"
						"map $diffuse\n"
						"blendfunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_COLOR\n"
						"rgbgen vertex\n"
						"alphagen vertex\n"
					"}\n"
//					"polygonoffset\n"
				"}\n"
				;
			break;
		}

		memset(&tn, 0, sizeof(tn));
		tn.base = R_LoadHiResTexture(ptype->texname, "particles", 0);
		if (!TEXVALID(tn.base))
		{
			/*okay, so the texture they specified wasn't valid either. use a fully default one*/

			//note that this could get messy if you depend upon vid_restart to reload your effect without re-execing it after.
			ptype->s1 = 0;
			ptype->t1 = 0;
			ptype->s2 = 1;
			ptype->t2 = 1;
			ptype->randsmax = 1;
			if (ptype->looks.type == PT_BEAM)
			{
				/*untextured beams get a single continuous blob*/
				ptype->looks.shader = R_RegisterShader(va("beam%s", namepostfix), defaultshader);
				TEXASSIGNF(tn.base, beamtexture);
			}
			else if (ptype->looks.type == PT_SPARKFAN)
			{
				/*untextured beams get a single continuous blob*/
				ptype->looks.shader = R_RegisterShader(va("fan%s", namepostfix), defaultshader);
				TEXASSIGNF(tn.base, ptritexture);
			}
			else if (strstr(ptype->texname, "glow") || strstr(ptype->texname, "ball") || ptype->looks.type == PT_TEXTUREDSPARK)
			{
				/*sparks and special names get a nice circular texture.
				as these are fully default, we can basically discard the texture name in the shader, and get better batching*/
				ptype->looks.shader = R_RegisterShader(va("ball%s", namepostfix), defaultshader);
				TEXASSIGNF(tn.base, balltexture);
			}
			else
			{
				/*anything else gets a fuzzy texture*/
				ptype->looks.shader = R_RegisterShader(va("default%s", namepostfix), defaultshader);
				TEXASSIGNF(tn.base, explosiontexture);
			}
		}
		else
		{
			/*texture looks good, make a shader, and give it the texture as a diffuse stage*/
			ptype->looks.shader = R_RegisterShader(va("%s%s", ptype->texname, namepostfix), defaultshader);
		}
		R_BuildDefaultTexnums(&tn, ptype->looks.shader);
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

	var = Cmd_Argv(1);
	if (*var == '+')
	{
		ptype = P_GetParticleType(var+1);
		if (ptype->loaded)
		{
			int i, parenttype;
			char newname[256];
			for (i = 0; i < 64; i++)
			{
				parenttype = ptype - part_type;
				snprintf(newname, sizeof(newname), "+%i%s", i, var);
				ptype = P_GetParticleType(newname);
				if (!ptype->loaded)
				{
					if (part_type[parenttype].assoc != P_INVALID)
						Con_Printf("warning: assoc on particle chain %s overridden\n", var+1);
					part_type[parenttype].assoc = ptype - part_type;
					break;
				}
			}
			if (i == 64)
			{
				Con_Printf("Too many duplicate names, gave up\n");
				return;
			}
		}
	}
	else
	{
		ptype = P_GetParticleType(Cmd_Argv(1));
		if (ptype->loaded)
		{
			assoc = ptype->assoc;
			while (assoc != P_INVALID && assoc < FALLBACKBIAS)
			{
				if (*part_type[assoc].name == '+')
				{
					part_type[assoc].loaded = false;
					assoc = part_type[assoc].assoc;
				}
				else
					break;
			}
		}
	}
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
			ptype->rotationstartrand = 0;
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
				ptype->scalerand = atof(Cmd_Argv(2)) - ptype->scale;
		}
		else if (!strcmp(var, "scalerand"))
			ptype->scalerand = atof(value);

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
			if (Cmd_Argc()>3)
				ptype->countextra = atof(Cmd_Argv(3));
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
		else if (!strcmp(var, "model"))
		{
			Q_strncpyz(ptype->modelname, Cmd_Argv(1), sizeof(ptype->modelname));
			ptype->modelframestart = atof(Cmd_Argv(2));
			ptype->modelframeend = atof(Cmd_Argv(3));
			ptype->modelframerate = atof(Cmd_Argv(4));
			ptype->modelalpha = atof(Cmd_Argv(5));
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
			ptype->stainonimpact = atoi(value);
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
			assoc = PScript_ParticleTypeForName(value);//careful - this can realloc all the particle types
			ptype = &part_type[pnum];
			ptype->cliptype = assoc;
		}
		else if (!strcmp(var, "clipcount"))
			ptype->clipcount = atof(value);

		else if (!strcmp(var, "emit"))
		{
			assoc = PScript_ParticleTypeForName(value);//careful - this can realloc all the particle types
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
			ptype->spawnvel = atof(value);
		}
		else if (!strcmp(var, "offsetspreadvert"))
		{
			Con_DPrintf("offsetspreadvert is deprechiated, use spawnvel\n");
			ptype->spawnvelvert  = atof(value);
		}

		// current names
		else if (!strcmp(var, "spawnorg"))
		{
			ptype->areaspreadvert = ptype->areaspread = atof(value);

			if (Cmd_Argc()>2)
				ptype->areaspreadvert = atof(Cmd_Argv(2));
		}
		else if (!strcmp(var, "spawnvel"))
		{
			ptype->spawnvelvert = ptype->spawnvel = atof(value);

			if (Cmd_Argc()>2)
				ptype->spawnvelvert = atof(Cmd_Argv(2));
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

		else if (!strcmp(var, "lightradius"))
			ptype->dl_radius = atof(value);
		else if (!strcmp(var, "lightradiusfade"))
			ptype->dl_decay[3] = atof(value);
		else if (!strcmp(var, "lightrgb"))
		{
			ptype->dl_rgb[0] = atof(value);
			ptype->dl_rgb[1] = atof(Cmd_Argv(2));
			ptype->dl_rgb[2] = atof(Cmd_Argv(3));
		}
		else if (!strcmp(var, "lightrgbfade"))
		{
			ptype->dl_decay[0] = atof(value);
			ptype->dl_decay[1] = atof(Cmd_Argv(2));
			ptype->dl_decay[2] = atof(Cmd_Argv(3));
		}
		else if (!strcmp(var, "lighttime"))
			ptype->dl_time = atof(value);
		else
			Con_DPrintf("%s is not a recognised particle type field (in %s)\n", var, ptype->name);
	}
	ptype->looks.invscalefactor = 1-ptype->looks.scalefactor;
	ptype->loaded = 1;
	if (ptype->clipcount < 1)
		ptype->clipcount = 1;

	//if there is a chance that it moves
	if (ptype->randomvel || ptype->gravity || ptype->veladd || ptype->spawnvel || ptype->spawnvelvert)
		ptype->flags |= PT_VELOCITY;
	//if it has friction
	if (ptype->friction[0] || ptype->friction[1] || ptype->friction[2])
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
	part_type_t *ptype;

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
		{
			Con_Printf("Type %s = %i total\n", part_type[i].name, j);
			if (!(part_type[i].state & PS_INRUNLIST))
				Con_Printf("  NOT RUNNING\n");
		}
	}

	Con_Printf("Running effects:\n");
	// maintain run list
	for (ptype = part_run_list; ptype; ptype = ptype->nexttorun)
	{
		j = 0;
		for (p = ptype->particles; p; p = p->next)
			j++;


		Con_Printf("Type %s = %i total\n", ptype->name, j);
	}
	Con_Printf("End of list\n");
}
#endif

void FinishParticleType(part_type_t *ptype)
{
	//if there is a chance that it moves
	if (ptype->randomvel || ptype->gravity || ptype->veladd || ptype->spawnvel || ptype->spawnvelvert)
		ptype->flags |= PT_VELOCITY;
	//if it has friction
	if (ptype->friction[0] || ptype->friction[1] || ptype->friction[2])
		ptype->flags |= PT_FRICTION;

	P_LoadTexture(ptype, true);
	if (ptype->die == 9999)
	{
		if (ptype->alphachange)
			ptype->die = (ptype->alpha+ptype->alpharand)/-ptype->alphachange;
		else
			ptype->die = 15;
	}
	if (ptype->looks.scalefactor > 1 && !ptype->looks.invscalefactor)
	{
		ptype->scale *= ptype->looks.scalefactor;
		ptype->scalerand *= ptype->looks.scalefactor;
		/*too lazy to go through ramps*/
		ptype->looks.scalefactor = 1;
	}
	if (ptype->looks.type == PT_TEXTUREDSPARK)
		ptype->looks.stretch *= 0.04;
}

static void P_ImportEffectInfo_f(void)
{
	part_type_t *ptype = NULL;
	int parenttype;
	char *file, *line;
	char *cmd;
	char arg[8][1024];
	int args = 0;
	FS_LoadFile("effectinfo.txt", (void**)&file);

	if (!file)
	{
		Con_Printf("effectinfo.txt not found\n");
		return;
	}
	line = file;
	for (;;)
	{
		if (!*line)
			break;
		if (args == 8)
		{
			Con_Printf("Too many args!\n");
			args--;
		}
		line = COM_StringParse(line, com_token, sizeof(com_token), false, false);
		Q_strncpyz(arg[args], com_token, sizeof(arg[args]));
		args++;
		if (*com_token == '\n')
			args--;
		else if (*line)
			continue;

		if (args <= 0)
			continue;

		cmd = arg[0];
		if (!strcmp(arg[0], "effect"))
		{
			char newname[64];
			int i;

			if (ptype)
			{
				FinishParticleType(ptype);
			}

			ptype = P_GetParticleType(arg[1]);
			if (ptype->loaded)
			{
				for (i = 0; i < 64; i++)
				{
					parenttype = ptype - part_type;
					snprintf(newname, sizeof(newname), "%i+%s", i, arg[1]);
					ptype = P_GetParticleType(newname);
					if (!ptype->loaded)
					{
						part_type[parenttype].assoc = ptype - part_type;
						break;
					}
				}
				if (i == 64)
				{
					Con_Printf("Too many duplicate names, gave up\n");
					break;
				}
			}
			ptype->loaded = true;
			ptype->scale = 1;
			ptype->alpha = 0;
			ptype->alpharand = 1;
			ptype->alphachange = -1;
			ptype->die = 9999;
			strcpy(ptype->texname, "particles/particlefont.tga");
			ptype->rgb[0] = 1;
			ptype->rgb[1] = 1;
			ptype->rgb[2] = 1;
			ptype->colorindex = -1;

			ptype->spawnmode = SM_BOX;

			ptype->spawnchance = 1;
			ptype->randsmax = 1;
			ptype->looks.scalefactor = 2;
			ptype->looks.invscalefactor = 0;
			ptype->looks.type = PT_NORMAL;
			ptype->looks.blendmode = BM_BLEND;
			ptype->looks.stretch = 1;
		}
		else if (!ptype)
		{
			Con_Printf("Bad effectinfo file\n");
			break;
		}
		else if (!strcmp(arg[0], "countabsolute") && args == 2)
			ptype->countextra = atof(arg[1]);
		else if (!strcmp(arg[0], "count") && args == 2)
			ptype->count = atof(arg[1]);
		else if (!strcmp(arg[0], "type") && args == 2)
		{
			if (!strcmp(arg[1], "decal"))
			{
				ptype->looks.type = PT_DECAL;
				ptype->looks.blendmode = BM_INVMOD;
			}
			else if (!strcmp(arg[1], "alphastatic"))
			{
				ptype->looks.type = PT_NORMAL;
				ptype->looks.blendmode = BM_BLEND;
			}
			else if (!strcmp(arg[1], "static"))
			{
				ptype->looks.type = PT_NORMAL;
				ptype->looks.blendmode = BM_ADD;
			}
			else if (!strcmp(arg[1], "smoke"))
			{
				ptype->looks.type = PT_NORMAL;
				ptype->looks.blendmode = BM_ADD;
			}
			else if (!strcmp(arg[1], "spark"))
			{
				ptype->looks.type = PT_TEXTUREDSPARK;
			}
			else if (!strcmp(arg[1], "bubble"))
			{
				ptype->looks.type = PT_NORMAL;
				ptype->looks.blendmode = BM_ADD;
			}
			else if (!strcmp(arg[1], "blood"))
			{
				ptype->looks.type = PT_NORMAL;
				ptype->looks.blendmode = BM_INVMOD;
			}
			else if (!strcmp(arg[1], "beam"))
			{
				ptype->looks.type = PT_BEAM;
				ptype->looks.blendmode = BM_ADD;
			}
			else
			{
				Con_Printf("effectinfo type %s not supported\n", arg[1]);
			}
		}
		else if (!strcmp(arg[0], "tex") && args == 3)
		{
			int mini = atoi(arg[1]);
			int maxi = atoi(arg[2]);
			/*number range between 0 and 63*/
			ptype->s1 = 1/8.0 * (mini & 7);
			ptype->s2 = 1/8.0 * (1+(mini & 7));
			ptype->t1 = 1/8.0 * (mini>>3);
			ptype->t2 = 1/8.0 * (1+(mini>>3));
			ptype->texsstride = 1/8.0;
			ptype->randsmax = (maxi - mini);
			if (ptype->randsmax < 1)
				ptype->randsmax = 1;
		}
		else if (!strcmp(arg[0], "size") && args == 3)
		{
			float s1 = atof(arg[1]), s2 = atof(arg[2]);
			ptype->scale = s1;
			ptype->scalerand = s2-s1;
		}
		else if (!strcmp(arg[0], "sizeincrease") && args == 2)
			ptype->scaledelta = atof(arg[1]);
		else if (!strcmp(arg[0], "color") && args == 3)
		{
			unsigned int rgb1 = strtoul(arg[1], NULL, 0), rgb2 = strtoul(arg[2], NULL, 0);
			int i;
			for (i = 0; i < 3; i++)
			{
				ptype->rgb[i] = ((rgb1>>(16-i*8)) & 0xff)/255.0;
				ptype->rgbrandsync[i] = (((rgb2>>(16-i*8)) & 0xff) - ((rgb1>>(16-i*8)) & 0xff))/255.0;
			}
		}
		else if (!strcmp(arg[0], "alpha") && args == 4)
		{
			float a1 = atof(arg[1]), a2 = atof(arg[2]), f = atof(arg[3]);
			ptype->alpha = a1/255;
			ptype->alpharand = (a2-a1)/255;
			ptype->alphachange = -f/255;
		}
		else if (!strcmp(arg[0], "velocityoffset") && args == 4)
			; /*a 3d world-coord addition*/
		else if (!strcmp(arg[0], "velocityjitter") && args == 4)
		{
			ptype->spawnvel = (atof(arg[1]) + atof(arg[2]))*0.5;
			ptype->spawnvelvert = atof(arg[3]);
		}
		else if (!strcmp(arg[0], "originoffset") && args == 4)
			; /*a 3d world-coord addition*/
		else if (!strcmp(arg[0], "originjitter") && args == 4)
		{
			ptype->areaspread = (atof(arg[1]) + atof(arg[2]))*0.5;
			ptype->areaspreadvert = atof(arg[3]);
		}
		else if (!strcmp(arg[0], "gravity") && args == 2)
		{
			ptype->gravity = 800*atof(arg[1]);
		}
		else if (!strcmp(arg[0], "bounce") && args == 2)
		{
			ptype->clipbounce = atof(arg[1]);
			ptype->cliptype = ptype - part_type;
		}
		else if (!strcmp(arg[0], "airfriction") && args == 2)
			ptype->friction[2] = ptype->friction[1] = ptype->friction[0] = atof(arg[1]);
		else if (!strcmp(arg[0], "liquidfriction") && args == 2)
			;
		else if (!strcmp(arg[0], "underwater") && args == 1)
			;
		else if (!strcmp(arg[0], "notunderwater") && args == 1)
			;
		else if (!strcmp(arg[0], "velocitymultiplier") && args == 2)
			ptype->veladd = atof(arg[1]);
		else if (!strcmp(arg[0], "lightradius") && args == 2)
			;
		else if (!strcmp(arg[0], "lightradiusfade") && args == 2)
			;
		else if (!strcmp(arg[0], "lightcolor") && args == 4)
			;
		else if (!strcmp(arg[0], "lighttime") && args == 2)
			;
		else if (!strcmp(arg[0], "trailspacing") && args == 2)
			ptype->count = 1 / atof(arg[1]);
		else if (!strcmp(arg[0], "time") && args == 3)
		{
			ptype->die = atof(arg[1]);
			ptype->randdie = atof(arg[2]) - ptype->die;
			if (ptype->randdie < 0)
			{
				ptype->die = atof(arg[2]);
				ptype->randdie = atof(arg[1]) - ptype->die;
			}
		}
		else if (!strcmp(arg[0], "stretchfactor") && args == 2)
			ptype->looks.stretch = atof(arg[1]);
#if 0
		else if (!strcmp(arg[0], "blend") && args == 2)
			; /*overrides blendmode*/
		else if (!strcmp(arg[0], "orientation") && args == 2)
			; /*overrides type*/
		else if (!strcmp(arg[0], "lightshadow") && args == 2)
			;
		else if (!strcmp(arg[0], "lightcubemapnum") && args == 2)
			;
		else if (!strcmp(arg[0], "staincolor") && args == 2)
			;
		else if (!strcmp(arg[0], "stainalpha") && args == 2)
			;
		else if (!strcmp(arg[0], "stainsize") && args == 2)
			;
		else if (!strcmp(arg[0], "staintex") && args == 2)
			;
		else if (!strcmp(arg[0], "stainless") && args == 1)
			;
		else if (!strcmp(arg[0], "rotate") && args == 2)
			;
#endif
		else
			Con_Printf("Particle effect token not recognised, or invalid args: %s %s %s %s %s %s\n", arg[0], args<2?"":arg[1], args<3?"":arg[2], args<4?"":arg[3], args<5?"":arg[4], args<6?"":arg[5]);
		args = 0;
	}

	if (ptype)
	{
		FinishParticleType(ptype);
	}

	FS_FreeFile(file);
	r_plooksdirty = true;
}

/*
===============
R_InitParticles
===============
*/
static qboolean PScript_InitParticles (void)
{
	int		i;

	if (r_numparticles)	//already inited
		return true;

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

	Cmd_AddRemCommand("r_exportbuiltinparticles", P_ExportBuiltinSet_f);
	Cmd_AddRemCommand("r_importeffectinfo", P_ImportEffectInfo_f);

#if _DEBUG
	Cmd_AddRemCommand("r_partinfo", P_PartInfo_f);
	Cmd_AddRemCommand("r_beaminfo", P_BeamInfo_f);
#endif


	pt_pointfile		= P_AllocateParticleType("PT_POINTFILE");
	pe_default			= P_AllocateParticleType("PE_DEFAULT");
	pe_size2			= P_AllocateParticleType("PE_SIZE2");
	pe_size3			= P_AllocateParticleType("PE_SIZE3");
	pe_defaulttrail		= P_AllocateParticleType("PE_DEFAULTTRAIL");

	Cvar_Hook(&r_particledesc, R_ParticleDesc_Callback);
	Cvar_ForceCallback(&r_particledesc);


	for (i = 0; i < (BUFFERVERTS>>2)*6; i += 6)
	{
		pscriptquadindexes[i+0] = ((i/6)<<2)+0;
		pscriptquadindexes[i+1] = ((i/6)<<2)+1;
		pscriptquadindexes[i+2] = ((i/6)<<2)+2;
		pscriptquadindexes[i+3] = ((i/6)<<2)+0;
		pscriptquadindexes[i+4] = ((i/6)<<2)+2;
		pscriptquadindexes[i+5] = ((i/6)<<2)+3;
	}
	pscriptmesh.xyz_array = pscriptverts;
	pscriptmesh.st_array = pscripttexcoords;
	pscriptmesh.colors4f_array = pscriptcolours;
	pscriptmesh.indexes = pscriptquadindexes;
	for (i = 0; i < BUFFERVERTS; i++)
	{
		pscripttriindexes[i] = i;
	}
	pscripttmesh.xyz_array = pscriptverts;
	pscripttmesh.st_array = pscripttexcoords;
	pscripttmesh.colors4f_array = pscriptcolours;
	pscripttmesh.indexes = pscripttriindexes;

	if (fallback)
		fallback->InitParticles();
	return true;
}

static void PScript_Shutdown (void)
{
	if (fallback)
		fallback->ShutdownParticles();

	Cvar_Unhook(&r_particledesc);

	Cmd_RemoveCommand("pointfile");	//load the leak info produced from qbsp into the particle system to show a line. :)

	Cmd_RemoveCommand("r_part");

	Cmd_RemoveCommand("r_exportbuiltinparticles");
	Cmd_RemoveCommand("r_importeffectinfo");

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

	if (fallback)
		fallback->ClearParticles();

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
	char *file = NULL;
	int i;

	if (!*efname)
	{
		Con_Printf("Please name the built in effect (faithful, spikeset, tsshaft, minimal or highfps)\n");
		return;
	}

	for (i = 0; partset_list[i].name; i++)
	{
		if (!stricmp(efname, partset_list[i].name))
		{
			file = *partset_list[i].data;
			if (file)
			{
				COM_WriteFile(va("particles/%s.cfg", efname), file, strlen(file));
				Con_Printf("Written particles/%s.cfg\n", efname);
			}
			else
				Con_Printf("nothing to export\n");
			return;
		}
	}

	Con_Printf("'%s' is not a built in particle set\n", efname);
}

static void P_LoadParticleSet(char *name, qboolean first)
{
	char *file;
	int i;
	int restrictlevel = Cmd_FromGamecode() ? RESTRICT_SERVER : RESTRICT_LOCAL;

	/*set up a default*/
	if (first && !*name)
		name = "faithful";

	if (!strcmp(name, "classic"))
	{
		if (fallback)
			fallback->ShutdownParticles();
		fallback = &pe_classic;
		if (fallback)
		{
			fallback->InitParticles();
			fallback->ClearParticles();
		}
		return;
	}

	for (i = 0; partset_list[i].name; i++)
	{
		if (!stricmp(name, partset_list[i].name))
		{
			if (partset_list[i].data)
			{
				Cbuf_AddText(*partset_list[i].data, RESTRICT_LOCAL);
			}
			return;
		}
	}

	if (!strcmp(name, "effectinfo"))
	{
		P_ImportEffectInfo_f();
		return;
	}


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

static void R_Particles_KillAllEffects(void)
{
	int i;

	for (i = 0; i < numparticletypes; i++)
	{
		*part_type[i].texname = '\0';
		part_type[i].scale = 0;
		part_type[i].loaded = 0;
		if (part_type->ramp)
			BZ_Free(part_type->ramp);
		part_type->ramp = NULL;
	}
//	numparticletypes = 0;
//	BZ_Free(part_type);
//	part_type = NULL;

	f_modified_particles = false;

	if (fallback)
	{
		fallback->ShutdownParticles();
		fallback = NULL;
	}
}

static void R_ParticleDesc_Callback(struct cvar_s *var, char *oldvalue)
{
	qboolean		first;

	char *c;

	if (qrenderer == QR_NONE)
		return; // don't bother parsing early

	R_Particles_KillAllEffects();

	first = true;
	for (c = COM_ParseStringSet(var->string); com_token[0]; c = COM_ParseStringSet(c))
	{
		P_LoadParticleSet(com_token, first);
		first = false;
	}

	Cbuf_AddText("r_effect\n", RESTRICT_LOCAL);
	CL_RegisterParticles();
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

	if (!r_part_rain.ival || !r_part_rain_quantity.ival)
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

	for (ptype = 0; ptype<numparticletypes; ptype++)
	{
		if (!part_type[ptype].loaded)	//woo, batch skipping.
			continue;

		for (st = part_type[ptype].skytris; st; st = st->next)
		{
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

				if (!(cl.worldmodel->funcs.PointContents(cl.worldmodel, NULL, org) & FTECONTENTS_SOLID))
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

static void R_Part_SkyTri(float *v1, float *v2, float *v3, msurface_t *surf, int ptype)
{
	float dot;
	float xm;
	float ym;
	float theta;
	vec3_t xd;
	vec3_t yd;

	skytris_t *st;

	st = Hunk_Alloc(sizeof(skytris_t));
	st->next = part_type[ptype].skytris;
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

	part_type[ptype].skytris = st;
}



static void PScript_EmitSkyEffectTris(model_t *mod, msurface_t 	*fa, int ptype)
{
	vec3_t		verts[64];
	int v1;
	int v2;
	int v3;
	int numverts;
	int i, lindex;
	float *vec;

	if (ptype < 0 || ptype >= numparticletypes)
		return;

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
		R_Part_SkyTri(verts[v1], verts[v2], verts[v3], fa, ptype);

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


static void PScript_EffectSpawned(part_type_t *ptype, vec3_t org, vec3_t dir, int dlkey)
{
	if (*ptype->modelname)
	{
		if (!ptype->model)
			ptype->model = Mod_ForName(ptype->modelname, false);
		if (ptype->model && !ptype->model->needload)
			CL_SpawnSpriteEffect(org, dir, ptype->model, ptype->modelframestart, (ptype->modelframeend?ptype->modelframeend:(ptype->model->numframes - ptype->modelframestart)), ptype->modelframerate?ptype->modelframerate:10, ptype->modelalpha?ptype->modelalpha:1);
	}
	if (ptype->dl_radius)
	{
		dlight_t *dl = CL_NewDlightRGB(dlkey, org, ptype->dl_radius, ptype->dl_time, ptype->dl_rgb[0], ptype->dl_rgb[1], ptype->dl_rgb[2]);
		dl->channelfade[0] = ptype->dl_decay[0];
		dl->channelfade[1] = ptype->dl_decay[1];
		dl->channelfade[2] = ptype->dl_decay[2];
		dl->decay =          ptype->dl_decay[3];
	}
	if (ptype->stain_radius)
		R_AddStain(org, ptype->stain_rgb[0], ptype->stain_rgb[1], ptype->stain_rgb[2], ptype->stain_radius);
}

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

	if (typenum >= FALLBACKBIAS && fallback)
		return fallback->RunParticleEffectState(org, dir, count, typenum-FALLBACKBIAS, NULL);

	if (typenum < 0 || typenum >= numparticletypes)
		return 1;

	if (!ptype->loaded)
		return 1;

	// inwater check, switch only once
	if (r_part_contentswitch.ival && ptype->inwater >= 0 && cl.worldmodel)
	{
		int cont;
		cont = cl.worldmodel->funcs.PointContents(cl.worldmodel, NULL, org);

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

	// get msvc to shut up
	j = k = l = 0;
	m = 0;

	while(ptype)
	{
		PScript_EffectSpawned(ptype, org, dir, 0);

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
			float sb,sw,tb,tw;

			vec3_t bestdir;

			if (!free_decals)
				return 0;

			if (!dir || (dir[0] == 0 && dir[1] == 0 && dir[2] == 0))
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

					if (cl.worldmodel->funcs.Trace (cl.worldmodel, 0, 0, NULL, tangent, t2, vec3_origin, vec3_origin, &tr))
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

			sw = ptype->s2 - ptype->s1;
			sb = ptype->s1 + sw/2;
			tw = ptype->t2 - ptype->t1;
			tb = ptype->t1 + tw/2;
			sw /= ptype->scale;
			tw /= ptype->scale;

			decalcount = Q1BSP_ClipDecal(org, dir, tangent, t2, ptype->scale, &decverts);
			while(decalcount)
			{
				if (!free_decals)
					break;

				d = free_decals;
				free_decals = d->next;
				d->next = ptype->clippeddecals;
				ptype->clippeddecals = d;

				VectorCopy((decverts+0*(sizeof(vec3_t)/sizeof(vec_t))), d->vertex[0]);
				VectorCopy((decverts+1*(sizeof(vec3_t)/sizeof(vec_t))), d->vertex[1]);
				VectorCopy((decverts+2*(sizeof(vec3_t)/sizeof(vec_t))), d->vertex[2]);

				for (i = 0; i < 3; i++)
				{
					VectorSubtract(d->vertex[i], org, vec);
					d->texcoords[i][0] = (DotProduct(vec, t2)*sw)+sb;
					d->texcoords[i][1] = (DotProduct(vec, tangent)*tw)+tb;
				}

				d->die = ptype->randdie*frandom();

				if (ptype->die)
					d->rgba[3] = ptype->alpha + d->die*ptype->alphachange;
				else
					d->rgba[3] = ptype->alpha;
				d->rgba[3] += ptype->alpharand*frandom();

				if (ptype->colorindex >= 0)
				{
					int cidx;
					cidx = ptype->colorrand > 0 ? rand() % ptype->colorrand : 0;
					cidx = ptype->colorindex + cidx;
					if (cidx > 255)
						d->rgba[3] = d->rgba[3] / 2; // Hexen 2 style transparency
					cidx = (cidx & 0xff) * 3;
					d->rgba[0] = host_basepal[cidx] * (1/255.0);
					d->rgba[1] = host_basepal[cidx+1] * (1/255.0);
					d->rgba[2] = host_basepal[cidx+2] * (1/255.0);
				}
				else
					VectorCopy(ptype->rgb, d->rgba);

				vec[2] = frandom();
				vec[0] = vec[2]*ptype->rgbrandsync[0] + frandom()*(1-ptype->rgbrandsync[0]);
				vec[1] = vec[2]*ptype->rgbrandsync[1] + frandom()*(1-ptype->rgbrandsync[1]);
				vec[2] = vec[2]*ptype->rgbrandsync[2] + frandom()*(1-ptype->rgbrandsync[2]);
				d->rgba[0] += vec[0]*ptype->rgbrand[0] + ptype->rgbchange[0]*d->die;
				d->rgba[1] += vec[1]*ptype->rgbrand[1] + ptype->rgbchange[1]*d->die;
				d->rgba[2] += vec[2]*ptype->rgbrand[2] + ptype->rgbchange[2]*d->die;

				d->die = particletime + ptype->die - d->die;

				decverts += (sizeof(vec3_t)/sizeof(vec_t))*3;
				decalcount--;


				// maintain run list
				if (!(ptype->state & PS_INRUNLIST))
				{
					ptype->nexttorun = part_run_list;
					part_run_list = ptype;
					ptype->state |= PS_INRUNLIST;
				}
			}

			if (ptype->assoc < 0)
				break;

			ptype = &part_type[ptype->assoc];
			continue;
		}
		// init spawn specific variables
		b = bfirst = NULL;
		spawnspc = 8;
		pcount = ptype->countextra + count*(ptype->count+ptype->countrand*frandom());
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

		/*this is a hack, use countextra=1, count=0*/
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
			p->scale = ptype->scale+ptype->scalerand*frandom();
			if (ptype->die)
				p->rgba[3] = ptype->alpha+p->die*ptype->alphachange;
			else
				p->rgba[3] = ptype->alpha;
			p->rgba[3] += ptype->alpharand*frandom();
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
					p->rgba[3] = p->rgba[3] / 2; // Hexen 2 style transparency
				cidx = (cidx & 0xff) * 3;
				p->rgba[0] = host_basepal[cidx] * (1/255.0);
				p->rgba[1] = host_basepal[cidx+1] * (1/255.0);
				p->rgba[2] = host_basepal[cidx+2] * (1/255.0);
			}
			else
				VectorCopy(ptype->rgb, p->rgba);

			// use org temporarily for rgbsync
			p->org[2] = frandom();
			p->org[0] = p->org[2]*ptype->rgbrandsync[0] + frandom()*(1-ptype->rgbrandsync[0]);
			p->org[1] = p->org[2]*ptype->rgbrandsync[1] + frandom()*(1-ptype->rgbrandsync[1]);
			p->org[2] = p->org[2]*ptype->rgbrandsync[2] + frandom()*(1-ptype->rgbrandsync[2]);

			p->rgba[0] += p->org[0]*ptype->rgbrand[0] + ptype->rgbchange[0]*p->die;
			p->rgba[1] += p->org[1]*ptype->rgbrand[1] + ptype->rgbchange[1]*p->die;
			p->rgba[2] += p->org[2]*ptype->rgbrand[2] + ptype->rgbchange[2]*p->die;

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
				p->vel[0] += dir[0]*ptype->veladd+ofsvec[0]*ptype->spawnvel;
				p->vel[1] += dir[1]*ptype->veladd+ofsvec[1]*ptype->spawnvel;
				p->vel[2] += dir[2]*ptype->veladd+ofsvec[2]*ptype->spawnvelvert;

				p->org[0] += dir[0]*ptype->orgadd;
				p->org[1] += dir[1]*ptype->orgadd;
				p->org[2] += dir[2]*ptype->orgadd;
			}
			else
			{
				p->vel[0] += ofsvec[0]*ptype->spawnvel;
				p->vel[1] += ofsvec[1]*ptype->spawnvel;
				p->vel[2] += ofsvec[2]*ptype->spawnvelvert - ptype->veladd;

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
		if (count > 130 && part_type[pe_size3].loaded)
		{
			part_type[pe_size3].colorindex = color & ~0x7;
			part_type[pe_size3].colorrand = 8;
			P_RunParticleEffectType(org, dir, count, pe_size3);
		}
		else if (count > 20 && part_type[pe_size2].loaded)
		{
			part_type[pe_size2].colorindex = color & ~0x7;
			part_type[pe_size2].colorrand = 8;
			P_RunParticleEffectType(org, dir, count, pe_size2);
		}
		else if (part_type[pe_default].loaded || !fallback)
		{
			part_type[pe_default].colorindex = color & ~0x7;
			part_type[pe_default].colorrand = 8;
			P_RunParticleEffectType(org, dir, count, pe_default);
		}
		else
			fallback->RunParticleEffect(org, dir, color, count);
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

static void P_ParticleTrailDraw (vec3_t startpos, vec3_t end, part_type_t *ptype, trailstate_t **tsk, int dlkey)
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
	float tdegree = 2.0*M_PI/256; /* MSVC whine */
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

	PScript_EffectSpawned(ptype, start, vec3_origin, dlkey);

	if (ptype->assoc>=0)
	{
		if (ts)
			P_ParticleTrail(start, end, ptype->assoc, dlkey, &(ts->assoc));
		else
			P_ParticleTrail(start, end, ptype->assoc, dlkey, NULL);
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
			tdegree = 2.0*M_PI/ptype->spawnparam1; /* distance per rotation inversed */
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
		p->scale = ptype->scale+ptype->scalerand*frandom();
		if (ptype->die)
			p->rgba[3] = ptype->alpha+p->die*ptype->alphachange;
		else
			p->rgba[3] = ptype->alpha;
		p->rgba[3] += ptype->alpharand*frandom();
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
				p->rgba[3] = p->rgba[3] / 2;
			cidx = (cidx & 0xff) * 3;
			p->rgba[0] = host_basepal[cidx] * (1/255.0);
			p->rgba[1] = host_basepal[cidx+1] * (1/255.0);
			p->rgba[2] = host_basepal[cidx+2] * (1/255.0);
		}
		else
			VectorCopy(ptype->rgb, p->rgba);

		// use org temporarily for rgbsync
		p->org[2] = frandom();
		p->org[0] = p->org[2]*ptype->rgbrandsync[0] + frandom()*(1-ptype->rgbrandsync[0]);
		p->org[1] = p->org[2]*ptype->rgbrandsync[1] + frandom()*(1-ptype->rgbrandsync[1]);
		p->org[2] = p->org[2]*ptype->rgbrandsync[2] + frandom()*(1-ptype->rgbrandsync[2]);

		p->rgba[0] += p->org[0]*ptype->rgbrand[0] + ptype->rgbchange[0]*p->die;
		p->rgba[1] += p->org[1]*ptype->rgbrand[1] + ptype->rgbchange[1]*p->die;
		p->rgba[2] += p->org[2]*ptype->rgbrand[2] + ptype->rgbchange[2]*p->die;

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
			while (p->s1 >= 1)
			{
				p->s1 -= 1;
				p->s2 -= 1;
				p->t1 += ptype->texsstride;
				p->t2 += ptype->texsstride;
			}
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
					p->vel[0] = vec[1]*ptype->spawnvel;
					p->vel[1] = -vec[0]*ptype->spawnvel;
					p->org[0] = vec[1]*ptype->areaspread;
					p->org[1] = -vec[0]*ptype->areaspread;
				}
				else
				{
					p->vel[0] = -vec[1]*ptype->spawnvel;
					p->vel[1] = vec[0]*ptype->spawnvel;
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

					tright = tcos*ptype->spawnvel;
					tup = tsin*ptype->spawnvel;

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

				p->vel[0] = vec[0]*veladd+crandom()*randvel + p->org[0]*ptype->spawnvel;
				p->vel[1] = vec[1]*veladd+crandom()*randvel + p->org[1]*ptype->spawnvel;
				p->vel[2] = vec[2]*veladd+crandom()*randvelvert + p->org[2]*ptype->spawnvelvert;

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

					tcos = cos(len*tdegree)*ptype->spawnvel;
					tsin = sin(len*tdegree)*ptype->spawnvel;

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

					p->vel[0] = vec[0]*veladd+crandom()*randvel + p->org[0]*ptype->spawnvel;
					p->vel[1] = vec[1]*veladd+crandom()*randvel + p->org[1]*ptype->spawnvel;
					p->vel[2] = vec[2]*veladd+crandom()*randvelvert + p->org[2]*ptype->spawnvelvert;

					p->org[0] = p->org[0]*ptype->areaspread + start[0];
					p->org[1] = p->org[1]*ptype->areaspread + start[1];
					p->org[2] = p->org[2]*ptype->areaspreadvert + start[2];
				}
				break;
			default:
				p->org[0] = crandom();
				p->org[1] = crandom();
				p->org[2] = crandom();

				p->vel[0] = vec[0]*veladd+crandom()*randvel + p->org[0]*ptype->spawnvel;
				p->vel[1] = vec[1]*veladd+crandom()*randvel + p->org[1]*ptype->spawnvel;
				p->vel[2] = vec[2]*veladd+crandom()*randvelvert + p->org[2]*ptype->spawnvelvert;

				p->org[0] = p->org[0]*ptype->areaspread + start[0];
				p->org[1] = p->org[1]*ptype->areaspread + start[1];
				p->org[2] = p->org[2]*ptype->areaspreadvert + start[2];
				break;
			}

			if (ptype->orgadd)
			{
				p->org[0] += vec[0]*ptype->orgadd;
				p->org[1] += vec[1]*ptype->orgadd;
				p->org[2] += vec[2]*ptype->orgadd;
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

static int PScript_ParticleTrail (vec3_t startpos, vec3_t end, int type, int dlkey, trailstate_t **tsk)
{
	part_type_t *ptype = &part_type[type];

	// TODO: fallback particle system won't have a decent trailstate which will mess up
	// high fps trails
	if (type >= FALLBACKBIAS && fallback)
		return fallback->ParticleTrail(startpos, end, type-FALLBACKBIAS, dlkey, NULL);

	if (type < 0 || type >= numparticletypes)
		return 1;	//bad value

	if (!ptype->loaded)
		return 1;

	// inwater check, switch only once
	if (r_part_contentswitch.ival && ptype->inwater >= 0)
	{
		int cont;
		cont = cl.worldmodel->funcs.PointContents(cl.worldmodel, NULL, startpos);

		if (cont & FTECONTENTS_WATER)
			ptype = &part_type[ptype->inwater];
	}

	P_ParticleTrailDraw (startpos, end, ptype, tsk, dlkey);
	return 0;
}

static void PScript_ParticleTrailIndex (vec3_t start, vec3_t end, int color, int crnd, trailstate_t **tsk)
{
	part_type[pe_defaulttrail].colorindex = color;
	part_type[pe_defaulttrail].colorrand = crnd;
	P_ParticleTrail(start, end, pe_defaulttrail, 0, tsk);
}

static vec3_t pright, pup;
static float pframetime;

static void GL_DrawTexturedParticle(int count, particle_t **plist, plooks_t *type)
{
	particle_t *p;
	float x,y;
	float scale;

	while (count--)
	{
		p = *plist++;

		if (pscriptmesh.numvertexes >= BUFFERVERTS-4)
		{
			pscriptmesh.numindexes = pscriptmesh.numvertexes/4*6;
			BE_DrawMesh_Single(type->shader, &pscriptmesh, NULL, &type->shader->defaulttextures, 0);
			pscriptmesh.numvertexes = 0;
		}

		if (type->scalefactor == 1)
			scale = p->scale*0.25;
		else
		{
			scale = (p->org[0] - r_origin[0])*vpn[0] + (p->org[1] - r_origin[1])*vpn[1]
				+ (p->org[2] - r_origin[2])*vpn[2];
			scale = (scale*p->scale)*(type->invscalefactor) + p->scale * (type->scalefactor*250);
			if (scale < 20)
				scale = 0.25;
			else
				scale = 0.25 + scale * 0.001;
		}

		Vector4Copy(p->rgba, pscriptcolours[pscriptmesh.numvertexes+0]);
		Vector4Copy(p->rgba, pscriptcolours[pscriptmesh.numvertexes+1]);
		Vector4Copy(p->rgba, pscriptcolours[pscriptmesh.numvertexes+2]);
		Vector4Copy(p->rgba, pscriptcolours[pscriptmesh.numvertexes+3]);

		Vector2Set(pscripttexcoords[pscriptmesh.numvertexes+0], p->s1, p->t1);
		Vector2Set(pscripttexcoords[pscriptmesh.numvertexes+1], p->s1, p->t2);
		Vector2Set(pscripttexcoords[pscriptmesh.numvertexes+2], p->s2, p->t2);
		Vector2Set(pscripttexcoords[pscriptmesh.numvertexes+3], p->s2, p->t1);

		if (p->angle)
		{
			x = sin(p->angle)*scale;
			y = cos(p->angle)*scale;

			pscriptverts[pscriptmesh.numvertexes+0][0] = p->org[0] - x*pright[0] - y*pup[0];
			pscriptverts[pscriptmesh.numvertexes+0][1] = p->org[1] - x*pright[1] - y*pup[1];
			pscriptverts[pscriptmesh.numvertexes+0][2] = p->org[2] - x*pright[2] - y*pup[2];
			pscriptverts[pscriptmesh.numvertexes+1][0] = p->org[0] - y*pright[0] + x*pup[0];
			pscriptverts[pscriptmesh.numvertexes+1][1] = p->org[1] - y*pright[1] + x*pup[1];
			pscriptverts[pscriptmesh.numvertexes+1][2] = p->org[2] - y*pright[2] + x*pup[2];
			pscriptverts[pscriptmesh.numvertexes+2][0] = p->org[0] + x*pright[0] + y*pup[0];
			pscriptverts[pscriptmesh.numvertexes+2][1] = p->org[1] + x*pright[1] + y*pup[1];
			pscriptverts[pscriptmesh.numvertexes+2][2] = p->org[2] + x*pright[2] + y*pup[2];
			pscriptverts[pscriptmesh.numvertexes+3][0] = p->org[0] + y*pright[0] - x*pup[0];
			pscriptverts[pscriptmesh.numvertexes+3][1] = p->org[1] + y*pright[1] - x*pup[1];
			pscriptverts[pscriptmesh.numvertexes+3][2] = p->org[2] + y*pright[2] - x*pup[2];
		}
		else
		{
			VectorMA(p->org, -scale, pup, pscriptverts[pscriptmesh.numvertexes+0]);
			VectorMA(p->org, -scale, pright, pscriptverts[pscriptmesh.numvertexes+1]);
			VectorMA(p->org, scale, pup, pscriptverts[pscriptmesh.numvertexes+2]);
			VectorMA(p->org, scale, pright, pscriptverts[pscriptmesh.numvertexes+3]);
		}
		pscriptmesh.numvertexes += 4;
	}

	if (pscriptmesh.numvertexes)
	{
		pscriptmesh.numindexes = pscriptmesh.numvertexes/4*6;
		BE_DrawMesh_Single(type->shader, &pscriptmesh, NULL, &type->shader->defaulttextures, 0);
		pscriptmesh.numvertexes = 0;
	}
}

static void GL_DrawTrifanParticle(int count, particle_t **plist, plooks_t *type)
{
	particle_t *p;
	vec3_t v, cr, o2;
	float scale;

	while (count--)
	{
		p = *plist++;

		if (pscripttmesh.numvertexes >= BUFFERVERTS-3)
		{
			pscripttmesh.numindexes = pscripttmesh.numvertexes;
			BE_DrawMesh_Single(type->shader, &pscripttmesh, NULL, &type->shader->defaulttextures, 0);
			pscripttmesh.numvertexes = 0;
		}

		scale = (p->org[0] - r_origin[0])*vpn[0] + (p->org[1] - r_origin[1])*vpn[1]
			+ (p->org[2] - r_origin[2])*vpn[2];
		scale = (scale*p->scale)*(type->invscalefactor) + p->scale * (type->scalefactor*250);
		if (scale < 20)
			scale = 0.05;
		else
			scale = 0.05 + scale * 0.0001;

		Vector4Copy(p->rgba, pscriptcolours[pscripttmesh.numvertexes+0]);
		Vector4Copy(p->rgba, pscriptcolours[pscripttmesh.numvertexes+1]);
		Vector4Copy(p->rgba, pscriptcolours[pscripttmesh.numvertexes+2]);

		Vector2Set(pscripttexcoords[pscripttmesh.numvertexes+0], p->s1, p->t1);
		Vector2Set(pscripttexcoords[pscripttmesh.numvertexes+1], p->s1, p->t2);
		Vector2Set(pscripttexcoords[pscripttmesh.numvertexes+2], p->s2, p->t1);


		VectorMA(p->org, -scale, p->vel, o2);
		VectorSubtract(r_refdef.vieworg, o2, v);
		CrossProduct(v, p->vel, cr);
		VectorNormalize(cr);

		VectorCopy(p->org, pscriptverts[pscripttmesh.numvertexes+0]);
		VectorMA(o2, -p->scale, cr, pscriptverts[pscripttmesh.numvertexes+1]);
		VectorMA(o2, p->scale, cr, pscriptverts[pscripttmesh.numvertexes+2]);

		pscripttmesh.numvertexes += 3;
	}

	if (pscripttmesh.numvertexes)
	{
		pscripttmesh.numindexes = pscripttmesh.numvertexes;
		BE_DrawMesh_Single(type->shader, &pscripttmesh, NULL, &type->shader->defaulttextures, 0);
		pscripttmesh.numvertexes = 0;
	}
}

static void GL_DrawLineSparkParticle(int count, particle_t **plist, plooks_t *type)
{
#ifdef warningmsg
#pragma warningmsg("fixme: no line sparks")
#endif
#if 0
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
#endif
}

static void R_AddTSparkParticle(scenetris_t *t, particle_t *p, plooks_t *type)
{
	vec3_t v, cr, o2;
	float scale;

	if (cl_numstrisvert+4 > cl_maxstrisvert)
	{
		cl_maxstrisvert+=64*4;
		cl_strisvertv = BZ_Realloc(cl_strisvertv, sizeof(*cl_strisvertv)*cl_maxstrisvert);
		cl_strisvertt = BZ_Realloc(cl_strisvertt, sizeof(*cl_strisvertt)*cl_maxstrisvert);
		cl_strisvertc = BZ_Realloc(cl_strisvertc, sizeof(*cl_strisvertc)*cl_maxstrisvert);
	}

	if (type->scalefactor == 1)
		scale = p->scale*0.25;
	else
	{
		scale = (p->org[0] - r_origin[0])*vpn[0] + (p->org[1] - r_origin[1])*vpn[1]
			+ (p->org[2] - r_origin[2])*vpn[2];
		scale = (scale*p->scale)*(type->invscalefactor) + p->scale * (type->scalefactor*250);
		if (scale < 20)
			scale = 0.25;
		else
			scale = 0.25 + scale * 0.001;
	}

	Vector4Copy(p->rgba, cl_strisvertc[cl_numstrisvert+0]);
	Vector4Copy(p->rgba, cl_strisvertc[cl_numstrisvert+1]);
	Vector4Copy(p->rgba, cl_strisvertc[cl_numstrisvert+2]);
	Vector4Copy(p->rgba, cl_strisvertc[cl_numstrisvert+3]);

	Vector2Set(cl_strisvertt[cl_numstrisvert+0], p->s1, p->t1);
	Vector2Set(cl_strisvertt[cl_numstrisvert+1], p->s1, p->t2);
	Vector2Set(cl_strisvertt[cl_numstrisvert+2], p->s2, p->t2);
	Vector2Set(cl_strisvertt[cl_numstrisvert+3], p->s2, p->t1);



	if (type->stretch)
	{
		VectorMA(p->org, type->stretch, p->vel, o2);
		VectorMA(p->org, -type->stretch, p->vel, v);
		VectorSubtract(r_refdef.vieworg, v, v);
	}
	else
	{
		VectorMA(p->org, 0.1, p->vel, o2);
		VectorSubtract(r_refdef.vieworg, p->org, v);
	}

	CrossProduct(v, p->vel, cr);
	VectorNormalize(cr);

	VectorMA(p->org, -p->scale/2, cr, cl_strisvertv[cl_numstrisvert+0]);
	VectorMA(p->org, p->scale/2, cr, cl_strisvertv[cl_numstrisvert+1]);

	VectorSubtract(r_refdef.vieworg, o2, v);
	CrossProduct(v, p->vel, cr);
	VectorNormalize(cr);

	VectorMA(o2, p->scale/2, cr, cl_strisvertv[cl_numstrisvert+2]);
	VectorMA(o2, -p->scale/2, cr, cl_strisvertv[cl_numstrisvert+3]);



	if (cl_numstrisidx+6 > cl_maxstrisidx)
	{
		cl_maxstrisidx += 64*6;
		cl_strisidx = BZ_Realloc(cl_strisidx, sizeof(*cl_strisidx)*cl_maxstrisidx);
	}
	cl_strisidx[cl_numstrisidx++] = (cl_numstrisvert - t->firstvert) + 0;
	cl_strisidx[cl_numstrisidx++] = (cl_numstrisvert - t->firstvert) + 1;
	cl_strisidx[cl_numstrisidx++] = (cl_numstrisvert - t->firstvert) + 2;
	cl_strisidx[cl_numstrisidx++] = (cl_numstrisvert - t->firstvert) + 0;
	cl_strisidx[cl_numstrisidx++] = (cl_numstrisvert - t->firstvert) + 2;
	cl_strisidx[cl_numstrisidx++] = (cl_numstrisvert - t->firstvert) + 3;

	cl_numstrisvert += 4;

	t->numvert += 4;
	t->numidx += 6;
}

static void GL_DrawTexturedSparkParticle(int count, particle_t **plist, plooks_t *type)
{
	particle_t *p;
	vec3_t v, cr, o2;

	while (count--)
	{
		p = *plist++;

		if (pscriptmesh.numvertexes >= BUFFERVERTS-4)
		{
			pscriptmesh.numindexes = pscriptmesh.numvertexes/4*6;
			BE_DrawMesh_Single(type->shader, &pscriptmesh, NULL, &type->shader->defaulttextures, 0);
			pscriptmesh.numvertexes = 0;
		}

		Vector4Copy(p->rgba, pscriptcolours[pscriptmesh.numvertexes+0]);
		Vector4Copy(p->rgba, pscriptcolours[pscriptmesh.numvertexes+1]);
		Vector4Copy(p->rgba, pscriptcolours[pscriptmesh.numvertexes+2]);
		Vector4Copy(p->rgba, pscriptcolours[pscriptmesh.numvertexes+3]);

		Vector2Set(pscripttexcoords[pscriptmesh.numvertexes+0], p->s1, p->t1);
		Vector2Set(pscripttexcoords[pscriptmesh.numvertexes+1], p->s1, p->t2);
		Vector2Set(pscripttexcoords[pscriptmesh.numvertexes+2], p->s2, p->t2);
		Vector2Set(pscripttexcoords[pscriptmesh.numvertexes+3], p->s2, p->t1);


		if (type->stretch)
		{
			VectorMA(p->org, type->stretch, p->vel, o2);
			VectorMA(p->org, -type->stretch, p->vel, v);
			VectorSubtract(r_refdef.vieworg, v, v);
		}
		else
		{
			VectorMA(p->org, 0.1, p->vel, o2);
			VectorSubtract(r_refdef.vieworg, p->org, v);
		}

		CrossProduct(v, p->vel, cr);
		VectorNormalize(cr);

		VectorMA(p->org, -p->scale/2, cr, pscriptverts[pscriptmesh.numvertexes+0]);
		VectorMA(p->org, p->scale/2, cr, pscriptverts[pscriptmesh.numvertexes+1]);

		VectorSubtract(r_refdef.vieworg, o2, v);
		CrossProduct(v, p->vel, cr);
		VectorNormalize(cr);

		VectorMA(o2, p->scale/2, cr, pscriptverts[pscriptmesh.numvertexes+2]);
		VectorMA(o2, -p->scale/2, cr, pscriptverts[pscriptmesh.numvertexes+3]);

		pscriptmesh.numvertexes += 4;
	}

	if (pscriptmesh.numvertexes)
	{
		pscriptmesh.numindexes = pscriptmesh.numvertexes/4*6;
		BE_DrawMesh_Single(type->shader, &pscriptmesh, NULL, &type->shader->defaulttextures, 0);
		pscriptmesh.numvertexes = 0;
	}
}

static void GL_DrawParticleBeam(int count, beamseg_t **blist, plooks_t *type)
{
	beamseg_t *b;
	vec3_t v;
	vec3_t cr;
	beamseg_t *c;
	particle_t *p;
	particle_t *q;
	float ts;

	while(count--)
	{
		b = *blist++;

		if (pscriptmesh.numvertexes >= BUFFERVERTS-4)
		{
			pscriptmesh.numindexes = pscriptmesh.numvertexes/4*6;
			BE_DrawMesh_Single(type->shader, &pscriptmesh, NULL, &type->shader->defaulttextures, 0);
			pscriptmesh.numvertexes = 0;
		}

		c = b->next;

		q = c->p;
		p = b->p;

		VectorSubtract(r_refdef.vieworg, q->org, v);
		VectorNormalize(v);
		CrossProduct(c->dir, v, cr);
		VectorNormalize(cr);
		ts = c->texture_s*q->angle + particletime*q->rotationspeed;
		Vector4Copy(q->rgba, pscriptcolours[pscriptmesh.numvertexes+0]);
		Vector4Copy(q->rgba, pscriptcolours[pscriptmesh.numvertexes+1]);
		Vector2Set(pscripttexcoords[pscriptmesh.numvertexes+0], ts, p->t1);
		Vector2Set(pscripttexcoords[pscriptmesh.numvertexes+1], ts, p->t2);
		VectorMA(q->org, -q->scale, cr, pscriptverts[pscriptmesh.numvertexes+0]);
		VectorMA(q->org, q->scale, cr, pscriptverts[pscriptmesh.numvertexes+1]);

		VectorSubtract(r_refdef.vieworg, p->org, v);
		VectorNormalize(v);
		CrossProduct(b->dir, v, cr); // replace with old p->dir?
		VectorNormalize(cr);
		ts = b->texture_s*p->angle + particletime*p->rotationspeed;
		Vector4Copy(p->rgba, pscriptcolours[pscriptmesh.numvertexes+2]);
		Vector4Copy(p->rgba, pscriptcolours[pscriptmesh.numvertexes+3]);
		Vector2Set(pscripttexcoords[pscriptmesh.numvertexes+2], ts, p->t2);
		Vector2Set(pscripttexcoords[pscriptmesh.numvertexes+3], ts, p->t1);
		VectorMA(p->org, p->scale, cr, pscriptverts[pscriptmesh.numvertexes+2]);
		VectorMA(p->org, -p->scale, cr, pscriptverts[pscriptmesh.numvertexes+3]);

		pscriptmesh.numvertexes += 4;
	}

	if (pscriptmesh.numvertexes)
	{
		pscriptmesh.numindexes = pscriptmesh.numvertexes/4*6;
		BE_DrawMesh_Single(type->shader, &pscriptmesh, NULL, &type->shader->defaulttextures, 0);
		pscriptmesh.numvertexes = 0;
	}
}

static void GL_DrawClippedDecal(int count, clippeddecal_t **dlist, plooks_t *type)
{
	clippeddecal_t *d;

	while (count--)
	{
		d = *dlist++;

		if (pscripttmesh.numvertexes >= BUFFERVERTS-3)
		{
			pscripttmesh.numindexes = pscripttmesh.numvertexes;
			BE_DrawMesh_Single(type->shader, &pscripttmesh, NULL, &type->shader->defaulttextures, 0);
			pscripttmesh.numvertexes = 0;
		}

		Vector4Copy(d->rgba, pscriptcolours[pscripttmesh.numvertexes+0]);
		Vector4Copy(d->rgba, pscriptcolours[pscripttmesh.numvertexes+1]);
		Vector4Copy(d->rgba, pscriptcolours[pscripttmesh.numvertexes+2]);

		Vector2Copy(d->texcoords[0], pscripttexcoords[pscripttmesh.numvertexes+0]);
		Vector2Copy(d->texcoords[1], pscripttexcoords[pscripttmesh.numvertexes+1]);
		Vector2Copy(d->texcoords[2], pscripttexcoords[pscripttmesh.numvertexes+2]);

		VectorCopy(d->vertex[0], pscriptverts[pscripttmesh.numvertexes+0]);
		VectorCopy(d->vertex[1], pscriptverts[pscripttmesh.numvertexes+1]);
		VectorCopy(d->vertex[2], pscriptverts[pscripttmesh.numvertexes+2]);

		pscripttmesh.numvertexes += 3;
	}

	if (pscripttmesh.numvertexes)
	{
		pscripttmesh.numindexes = pscripttmesh.numvertexes;
		BE_DrawMesh_Single(type->shader, &pscripttmesh, NULL, &type->shader->defaulttextures, 0);
		pscripttmesh.numvertexes = 0;
	}
}

static void R_AddTexturedParticle(scenetris_t *t, particle_t *p, plooks_t *type)
{
	float scale, x, y;

	if (cl_numstrisvert+4 > cl_maxstrisvert)
	{
		cl_maxstrisvert+=64*4;
		cl_strisvertv = BZ_Realloc(cl_strisvertv, sizeof(*cl_strisvertv)*cl_maxstrisvert);
		cl_strisvertt = BZ_Realloc(cl_strisvertt, sizeof(*cl_strisvertt)*cl_maxstrisvert);
		cl_strisvertc = BZ_Realloc(cl_strisvertc, sizeof(*cl_strisvertc)*cl_maxstrisvert);
	}

	if (type->scalefactor == 1)
		scale = p->scale*0.25;
	else
	{
		scale = (p->org[0] - r_origin[0])*vpn[0] + (p->org[1] - r_origin[1])*vpn[1]
			+ (p->org[2] - r_origin[2])*vpn[2];
		scale = (scale*p->scale)*(type->invscalefactor) + p->scale * (type->scalefactor*250);
		if (scale < 20)
			scale = 0.25;
		else
			scale = 0.25 + scale * 0.001;
	}

	Vector4Copy(p->rgba, cl_strisvertc[cl_numstrisvert+0]);
	Vector4Copy(p->rgba, cl_strisvertc[cl_numstrisvert+1]);
	Vector4Copy(p->rgba, cl_strisvertc[cl_numstrisvert+2]);
	Vector4Copy(p->rgba, cl_strisvertc[cl_numstrisvert+3]);

	Vector2Set(cl_strisvertt[cl_numstrisvert+0], p->s1, p->t1);
	Vector2Set(cl_strisvertt[cl_numstrisvert+1], p->s1, p->t2);
	Vector2Set(cl_strisvertt[cl_numstrisvert+2], p->s2, p->t2);
	Vector2Set(cl_strisvertt[cl_numstrisvert+3], p->s2, p->t1);

	if (p->angle)
	{
		x = sin(p->angle)*scale;
		y = cos(p->angle)*scale;

		cl_strisvertv[cl_numstrisvert+0][0] = p->org[0] - x*pright[0] - y*pup[0];
		cl_strisvertv[cl_numstrisvert+0][1] = p->org[1] - x*pright[1] - y*pup[1];
		cl_strisvertv[cl_numstrisvert+0][2] = p->org[2] - x*pright[2] - y*pup[2];
		cl_strisvertv[cl_numstrisvert+1][0] = p->org[0] - y*pright[0] + x*pup[0];
		cl_strisvertv[cl_numstrisvert+1][1] = p->org[1] - y*pright[1] + x*pup[1];
		cl_strisvertv[cl_numstrisvert+1][2] = p->org[2] - y*pright[2] + x*pup[2];
		cl_strisvertv[cl_numstrisvert+2][0] = p->org[0] + x*pright[0] + y*pup[0];
		cl_strisvertv[cl_numstrisvert+2][1] = p->org[1] + x*pright[1] + y*pup[1];
		cl_strisvertv[cl_numstrisvert+2][2] = p->org[2] + x*pright[2] + y*pup[2];
		cl_strisvertv[cl_numstrisvert+3][0] = p->org[0] + y*pright[0] - x*pup[0];
		cl_strisvertv[cl_numstrisvert+3][1] = p->org[1] + y*pright[1] - x*pup[1];
		cl_strisvertv[cl_numstrisvert+3][2] = p->org[2] + y*pright[2] - x*pup[2];
	}
	else
	{
		VectorMA(p->org, -scale, pup, cl_strisvertv[cl_numstrisvert+0]);
		VectorMA(p->org, -scale, pright, cl_strisvertv[cl_numstrisvert+1]);
		VectorMA(p->org, scale, pup, cl_strisvertv[cl_numstrisvert+2]);
		VectorMA(p->org, scale, pright, cl_strisvertv[cl_numstrisvert+3]);
	}

	if (cl_numstrisidx+6 > cl_maxstrisidx)
	{
		cl_maxstrisidx += 64*6;
		cl_strisidx = BZ_Realloc(cl_strisidx, sizeof(*cl_strisidx)*cl_maxstrisidx);
	}
	cl_strisidx[cl_numstrisidx++] = (cl_numstrisvert - t->firstvert) + 0;
	cl_strisidx[cl_numstrisidx++] = (cl_numstrisvert - t->firstvert) + 1;
	cl_strisidx[cl_numstrisidx++] = (cl_numstrisvert - t->firstvert) + 2;
	cl_strisidx[cl_numstrisidx++] = (cl_numstrisvert - t->firstvert) + 0;
	cl_strisidx[cl_numstrisidx++] = (cl_numstrisvert - t->firstvert) + 2;
	cl_strisidx[cl_numstrisidx++] = (cl_numstrisvert - t->firstvert) + 3;

	cl_numstrisvert += 4;

	t->numvert += 4;
	t->numidx += 6;
}

static void PScript_DrawParticleTypes (void)
{
	void (*sparklineparticles)(int count, particle_t **,plooks_t*)=GL_DrawLineSparkParticle;
	void (*sparkfanparticles)(int count, particle_t **,plooks_t*)=GL_DrawTrifanParticle;
	void (*sparktexturedparticles)(int count, particle_t **,plooks_t*)=GL_DrawTexturedSparkParticle;

	qboolean (*tr) (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal);
	void *pdraw, *bdraw;
	void (*tdraw)(scenetris_t *t, particle_t *p, plooks_t *type);

	vec3_t oldorg;
	vec3_t stop, normal;
	part_type_t *type, *lastvalidtype;
	particle_t		*p, *kill;
	clippeddecal_t *d, *dkill;
	ramp_t *ramp;
	float grav;
	vec3_t friction;
	scenetris_t *scenetri;
	float dist;
	particle_t *kill_list, *kill_first;	//the kill list is to stop particles from being freed and reused whilst still in this loop
										//which is bad because beams need to find out when particles died. Reuse can do wierd things.
										//remember that they're not drawn instantly either.
	beamseg_t *b, *bkill;

	int traces=r_particle_tracelimit.ival;
	int rampind;
	RSpeedMark();

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
		CL_RegisterParticles();
	}

	pframetime = host_frametime;
	if (cl.paused || r_secondaryview || r_refdef.recurse)
		pframetime = 0;

	VectorScale (vup, 1.5, pup);
	VectorScale (vright, 1.5, pright);

#ifdef Q2BSPS
	if (cl.worldmodel->fromgame == fg_quake2 || cl.worldmodel->fromgame == fg_quake3)
		tr = Q2TraceLineN;
	else
#endif
		tr = TraceLineN;

	kill_list = kill_first = NULL;

	if (r_part_sparks_textured.ival < 0)
		sparktexturedparticles = NULL;
	else if (!r_part_sparks_textured.ival)
		sparktexturedparticles = sparklineparticles;

	if (r_part_sparks_trifan.ival < 0)
		sparkfanparticles = NULL;
	else if (!r_part_sparks_trifan.ival)
		sparkfanparticles = sparklineparticles;

	if (r_part_sparks.ival < 0)
		sparklineparticles = NULL;
	else if (!r_part_sparks.ival)
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
					VectorCopy(ramp->rgb, d->rgba);
					d->rgba[3] = ramp->alpha;
					break;
				case RAMP_DELTA:	//particle ramps
					ramp = type->ramp + (int)(type->rampindexes * (type->die - (d->die - particletime)) / type->die);
					VectorMA(d->rgba, pframetime, ramp->rgb, d->rgba);
					d->rgba[3] -= pframetime*ramp->alpha;
					break;
				case RAMP_NONE:	//particle changes acording to it's preset properties.
					if (particletime < (d->die-type->die+type->rgbchangetime))
					{
						d->rgba[0] += pframetime*type->rgbchange[0];
						d->rgba[1] += pframetime*type->rgbchange[1];
						d->rgba[2] += pframetime*type->rgbchange[2];
					}
					d->rgba[3] += pframetime*type->alphachange;
				}

				GL_DrawClippedDecal(1, &d, &type->looks);
			}
		}

		bdraw = NULL;
		pdraw = NULL;
		tdraw = NULL;

		// set drawing methods by type and cvars and hope branch
		// prediction takes care of the rest
		switch(type->looks.type)
		{
		case PT_BEAM:
			if (r_part_beams.ival <= 0)
				bdraw = NULL;
			else
				bdraw = GL_DrawParticleBeam;
			break;
		case PT_DECAL:
			break;
		case PT_NORMAL:
			pdraw = GL_DrawTexturedParticle;
			tdraw = R_AddTexturedParticle;
			break;
		case PT_SPARK:
			pdraw = sparklineparticles;
			break;
		case PT_SPARKFAN:
			pdraw = sparkfanparticles;
			break;
		case PT_TEXTUREDSPARK:
			pdraw = sparktexturedparticles;
			tdraw = R_AddTSparkParticle;
			break;
		}

		if (!tdraw || type->looks.shader->sort == SHADER_SORT_BLEND)
			scenetri = NULL;
		else if (cl_numstris && cl_stris[cl_numstris-1].shader == type->looks.shader)
			scenetri = &cl_stris[cl_numstris-1];
		else
		{
			if (cl_numstris == cl_maxstris)
			{
				cl_maxstris+=8;
				cl_stris = BZ_Realloc(cl_stris, sizeof(*cl_stris)*cl_maxstris);
			}
			scenetri = &cl_stris[cl_numstris++];
			scenetri->shader = type->looks.shader;
			scenetri->firstidx = cl_numstrisidx;
			scenetri->firstvert = cl_numstrisvert;
			scenetri->numvert = 0;
			scenetri->numidx = 0;
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
				if (type->stainonimpact && r_bloodstains.ival)
				{
					if (traces-->0&&tr(oldorg, p->org, stop, normal))
					{
						R_AddStain(stop,	(p->rgba[1]*-10+p->rgba[2]*-10),
											(p->rgba[0]*-10+p->rgba[2]*-10),
											(p->rgba[0]*-10+p->rgba[1]*-10),
											30*p->rgba[3]*type->stainonimpact);
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

			goto endtype;
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
		friction[0] = 1 - type->friction[0]*pframetime;
		friction[1] = 1 - type->friction[1]*pframetime;
		friction[2] = 1 - type->friction[2]*pframetime;

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
					p->vel[0] *= friction[0];
					p->vel[1] *= friction[1];
					p->vel[2] *= friction[2];
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
				VectorCopy(ramp->rgb, p->rgba);
				p->rgba[3] = ramp->alpha;
				p->scale = ramp->scale;
				break;
			case RAMP_DELTA:	//particle ramps
				rampind = (int)(type->rampindexes * (type->die - (p->die - particletime)) / type->die);
				if (rampind >= type->rampindexes)
					rampind = type->rampindexes - 1;
				ramp = type->ramp + rampind;
				VectorMA(p->rgba, pframetime, ramp->rgb, p->rgba);
				p->rgba[3] -= pframetime*ramp->alpha;
				p->scale += pframetime*ramp->scale;
				break;
			case RAMP_NONE:	//particle changes acording to it's preset properties.
				if (particletime < (p->die-type->die+type->rgbchangetime))
				{
					p->rgba[0] += pframetime*type->rgbchange[0];
					p->rgba[1] += pframetime*type->rgbchange[1];
					p->rgba[2] += pframetime*type->rgbchange[2];
				}
				p->rgba[3] += pframetime*type->alphachange;
				p->scale += pframetime*type->scaledelta;
			}

			if (type->emit >= 0)
			{
				if (type->emittime < 0)
					P_ParticleTrail(oldorg, p->org, type->emit, 0, &p->state.trailstate);
				else if (p->state.nextemit < particletime)
				{
					p->state.nextemit = particletime + type->emittime + frandom()*type->emitrand;
					P_RunParticleEffectType(p->org, p->vel, 1, type->emit);
				}
			}

			if (type->cliptype>=0 && r_bouncysparks.ival)
			{
				if (traces-->0&&tr(oldorg, p->org, stop, normal))
				{
					if (type->stainonimpact && r_bloodstains.ival)
						R_AddStain(stop,	p->rgba[1]*-10+p->rgba[2]*-10,
											p->rgba[0]*-10+p->rgba[2]*-10,
											p->rgba[0]*-10+p->rgba[1]*-10,
											30*p->rgba[3]);

					if (part_type + type->cliptype == type)
					{	//bounce
						dist = DotProduct(p->vel, normal) * (-1-(rand()/(float)0x7fff)/2);

						VectorMA(p->vel, dist, normal, p->vel);
						VectorCopy(stop, p->org);
						p->vel[0] *= type->clipbounce;
						p->vel[1] *= type->clipbounce;
						p->vel[2] *= type->clipbounce;

						if (!*type->texname && Length(p->vel)<1000*pframetime && type->looks.type == PT_NORMAL)
						{
							p->die = -1;
							continue;
						}
					}
					else
					{
						p->die = -1;
						VectorNormalize(p->vel);
						P_RunParticleEffectType(stop, p->vel, type->clipcount/part_type[type->cliptype].count, type->cliptype);
						continue;
					}
				}
			}
			else if (type->stainonimpact && r_bloodstains.ival)
			{
				if (traces-->0&&tr(oldorg, p->org, stop, normal))
				{
					R_AddStain(stop,	(p->rgba[1]*-10+p->rgba[2]*-10),
										(p->rgba[0]*-10+p->rgba[2]*-10),
										(p->rgba[0]*-10+p->rgba[1]*-10),
										30*p->rgba[3]*type->stainonimpact);
					p->die = -1;
					continue;
				}
			}

			if (scenetri)
			{
				tdraw(scenetri, p, type->slooks);
			}
			else if (pdraw)
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

						if (b->p->die < particletime)
							b->flags |= BS_DEAD;
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

endtype:

		// delete from run list if necessary
		if (!type->particles && !type->beams && !type->clippeddecals)
		{
			if (!lastvalidtype)
				part_run_list = type->nexttorun;
			else
				lastvalidtype->nexttorun = type->nexttorun;
			type->state &= ~PS_INRUNLIST;
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

/*
===============
R_DrawParticles
===============
*/
static void PScript_DrawParticles (void)
{
	P_AddRainParticles();

	PScript_DrawParticleTypes();

	if (fallback)
		fallback->DrawParticles();
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
	PScript_DrawParticles
};

#endif
#endif
