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
#ifdef SWQUAKE
#include "r_local.h"
#endif
#include "glquake.h"//hack

#include "renderque.h"

#include "r_partset.h"

int pt_explosion,
	pt_emp,
	pt_pointfile,
	pt_entityparticles,
	pt_darkfield,
	pt_blob,
	pt_blood,
	pt_lightningblood,
	pt_gunshot,
	pt_wizspike,
	pt_knightspike,
	pt_spike,
	pt_superspike,
	pt_lavasplash,
	pt_teleportsplash,
	pt_blasterparticles,
	pt_superbullet,
	pt_bullet;

int pe_default,
	pe_size2,
	pe_size3;

int rt_blastertrail,
	rt_railtrail,
	rt_bubbletrail,
	rt_rocket;

//triangle fan sparks use these.
static double sint[7] = {0.000000, 0.781832,  0.974928,  0.433884, -0.433884, -0.974928, -0.781832};
static double cost[7] = {1.000000, 0.623490, -0.222521, -0.900969, -0.900969, -0.222521,  0.623490};

void R_RunParticleEffect2 (vec3_t org, vec3_t dmin, vec3_t dmax, int color, int effect, int count);
void R_RunParticleEffect3 (vec3_t org, vec3_t box, int color, int effect, int count);
void R_RunParticleEffect4 (vec3_t org, float radius, int color, int effect, int count);

int R_RunParticleEffectType (vec3_t org, vec3_t dir, float count, int typenum);

#define crand() (rand()%32767/16383.5f-1)

void D_DrawParticleTrans (particle_t *pparticle);
void D_DrawSparkTrans (particle_t *pparticle);

#define MAX_PARTICLES			32768	// default max # of particles at one
										//  time
#define ABSOLUTE_MIN_PARTICLES	512		// no fewer than this no matter what's
										//  on the command line

//int		ramp1[8] = {0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61};
//int		ramp2[8] = {0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66};
//int		ramp3[8] = {0x6d, 0x6b, 6,	  5,    4,    3,    2,    1};

particle_t	*free_particles;

particle_t	*particles;
int			r_numparticles;

vec3_t			r_pright, r_pup, r_ppn;

extern cvar_t r_bouncysparks;
extern cvar_t r_part_rain;
extern cvar_t gl_part_explosionheart, gl_part_emp;
extern cvar_t gl_part_trifansparks;
extern cvar_t r_particles_in_explosion;
extern cvar_t r_particle_explosion_speed;
extern cvar_t r_bloodstains;

cvar_t r_particlesdesc = {"r_particlesdesc", "spikeset", NULL, CVAR_LATCH, CVAR_SEMICHEAT};

cvar_t r_part_rain_colour = {"r_part_rain_colour", "15"};
cvar_t r_part_rain_quantity = {"r_part_rain_quantity", "1"};

cvar_t gl_part_trifansparks = {"gl_part_trifansparks", "0"};

static float particletime;

typedef struct skytris_s {
	struct skytris_s *next;
	vec3_t org;
	vec3_t x;
	vec3_t y;
	float area;
	float nexttime;
	msurface_t *face;
	int ptype;
} skytris_t;

static skytris_t *skytris;







//these could be deltas or absolutes depending on ramping mode.
typedef struct {
	vec3_t rgb;
	float alpha;
	float scale;
} ramp_t;

typedef struct part_type_s {
	char name[MAX_QPATH];
	char texname[MAX_QPATH];
	vec3_t rgb;
	vec3_t rgbchange;
	vec3_t rgbrand;
	int colorindex;
	int colorrand;
	int citracer;
	float rgbchangetime;
	vec3_t rgbrandsync;
	float scale, alpha;
	float alphachange;
	float die, randdie;
	float randomvel, veladd;
	float offsetspread;
	float offsetspreadvert;
	float randomvelvert;
	float randscale;
	enum {BM_MERGE, BM_ADD, BM_SUBTRACT} blendmode;

	float scaledelta;
	float count;
	int texturenum;
	int assoc;
	int cliptype;
	float clipcount;
	int emit;
	float emittime;
	float emitrand;
	float emitstart;

	float areaspread;
	float areaspreadvert;
	float scalefactor;
	float invscalefactor;

	float offsetup; // make this into a vec3_t later with dir, possibly for mdls

	enum {SM_BOX, SM_CIRCLE, SM_BALL, SM_SPIRAL, SM_TRACER, SM_TELEBOX, SM_LAVASPLASH} spawnmode;	
	//box = even spread within the area
	//circle = around edge of a circle
	//ball = filled sphere
	//spiral = spiral trail
	//tracer = tracer trail
	//telebox = q1-style telebox
	//lavasplash = q1-style lavasplash

	float gravity;
	vec3_t friction;
	int stains;

	enum {RAMP_NONE, RAMP_DELTA, RAMP_ABSOLUTE} rampmode;
	int rampindexes;
	ramp_t *ramp;

	int loaded;
	particle_t	*particles;
} part_type_t;
int numparticletypes;
part_type_t *part_type;

part_type_t *GetParticleType(char *name)
{
	int i;
	part_type_t *ptype;
	for (i = 0; i < numparticletypes; i++)
	{
		ptype = &part_type[i];
		if (!strcmp(ptype->name, name))
			return ptype;
	}
	part_type = BZ_Realloc(part_type, sizeof(part_type_t)*(numparticletypes+1));
	ptype = &part_type[numparticletypes++];
	strcpy(ptype->name, name);
	ptype->assoc=-1;
	ptype->cliptype = -1;
	ptype->emit = -1;
	ptype->loaded = 0;
	return ptype;
}

int AllocateParticleType(char *name)
{
	return GetParticleType(name) - part_type;
}

int ParticleTypeForName(char *name)
{
	int to;

	to = GetParticleType(name) - part_type;
	if (to < 0 || to >= numparticletypes)
	{
		return -1;
	}

	return to;
}

int FindParticleType(char *name)
{
	int i;
	for (i = 0; i < numparticletypes; i++)
	{
		if (!strcmp(part_type[i].name, name))
			return i;
	}

	return -1;
}

static void R_Part_Modified(void)
{
	if (Cmd_FromServer())
		return;	//server stuffed particle descriptions don't count.

	f_modified_particles = true;

	if (care_f_modified)
	{
		care_f_modified = false;
		Cbuf_AddText("say particles description has changed\n", RESTRICT_LOCAL);
	}
}
int CheckAssosiation(char *name, int from)
{
	int to, orig;

	orig = to = FindParticleType(name);
	if (to < 0)
	{
		return -1;
	}

	while(to != -1)
	{
		if (to == from)
		{
			Con_Printf("Assosiation would cause infinate loop\n");
			return -1;
		}
		to = part_type[to].assoc;
	}
	return orig;
}

void R_ParticleEffect_f(void)
{
	char *var, *value;
	char *buf;
	particle_t *parts;

	part_type_t *ptype;
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
		Cbuf_InsertText(buf, Cmd_ExecLevel);
		Con_Printf("This is a multiline command and should be used within config files\n");
		return;
	}

	ptype = GetParticleType(Cmd_Argv(1));
	if (!ptype)
	{
		Con_Printf("Bad name\n");
		return;
	}

	R_Part_Modified();

	pnum = ptype-part_type;

	parts = ptype->particles;
	if (ptype->ramp)
		BZ_Free(ptype->ramp);
	memset(ptype, 0, sizeof(*ptype));
	ptype->particles = parts;
	strcpy(ptype->name, Cmd_Argv(1));
	ptype->assoc=-1;
	ptype->cliptype = -1;
	ptype->emit = -1;
	ptype->alpha = 1;
	ptype->alphachange = 1;
	ptype->colorindex = -1;

	while(1)
	{
		buf = Cbuf_GetNext(Cmd_ExecLevel);
		while (*buf && *buf <= ' ')
			buf++;	//no whitespace please.
		if (*buf == '}')
			break;
		if (!*buf)
		{
			Con_Printf("Unexpected End Of Buffer\n");
			return;
		}

		Cmd_TokenizeString(buf);
		var = Cmd_Argv(0);
		value = Cmd_Argv(1);

		if (!strcmp(var, "texture"))
			Q_strncpyz(ptype->texname, value, sizeof(ptype->texname));

		else if (!strcmp(var, "scale"))
			ptype->scale = atof(value);
		else if (!strcmp(var, "scalerand"))
			ptype->randscale = atof(value);

		else if (!strcmp(var, "scalefactor"))
			ptype->scalefactor = atof(value);
		else if (!strcmp(var, "scaledelta"))
			ptype->scaledelta = atof(value);

		else if (!strcmp(var, "step"))
			ptype->count = 1/atof(value);
		else if (!strcmp(var, "count"))
			ptype->count = atof(value);

		else if (!strcmp(var, "alpha"))
			ptype->alpha = atof(value);
		else if (!strcmp(var, "alphachange"))
			ptype->alphachange = atof(value);
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

		else if (!strcmp(var, "assoc"))
		{
			assoc = CheckAssosiation(value, pnum);	//careful - this can realloc all the particle types
			ptype = &part_type[pnum];
			ptype->assoc = assoc;
		}

		else if (!strcmp(var, "colorindex"))
			ptype->colorindex = atoi(value);
		else if (!strcmp(var, "colorrand"))
			ptype->colorrand = atoi(value);
		else if (!strcmp(var, "citracer"))
			ptype->citracer = atoi(value);

		else if (!strcmp(var, "red"))
			ptype->rgb[0] = atof(value)/255;
		else if (!strcmp(var, "green"))
			ptype->rgb[1] = atof(value)/255;
		else if (!strcmp(var, "blue"))
			ptype->rgb[2] = atof(value)/255;

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
		else if (!strcmp(var, "rgbdeltatime"))
			ptype->rgbchangetime = atof(value);

		else if (!strcmp(var, "redrand"))
			ptype->rgbrand[0] = atof(value)/255;
		else if (!strcmp(var, "greenrand"))
			ptype->rgbrand[1] = atof(value)/255;
		else if (!strcmp(var, "bluerand"))
			ptype->rgbrand[2] = atof(value)/255;

		else if (!strcmp(var, "rgbrandsync"))
			ptype->rgbrandsync[0] = ptype->rgbrandsync[1] = ptype->rgbrandsync[2] = atof(value);
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
				ptype->blendmode = BM_ADD;
			else if (!strcmp(value, "subtract"))
				ptype->blendmode = BM_SUBTRACT;
			else
				ptype->blendmode = BM_MERGE;
				
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
			else
				ptype->spawnmode = SM_BOX;
				
		}

		else if (!strcmp(var, "cliptype"))
		{
			assoc = ParticleTypeForName(value);//careful - this can realloc all the particle types
			ptype = &part_type[pnum];
			ptype->cliptype = assoc;
		}
		else if (!strcmp(var, "clipcount"))
			ptype->clipcount = atof(value);

		else if (!strcmp(var, "emit"))
		{
			assoc = ParticleTypeForName(value);//careful - this can realloc all the particle types
			ptype = &part_type[pnum];
			ptype->emit = assoc;
		}
		else if (!strcmp(var, "emitinterval"))
			ptype->emittime = atof(value);
		else if (!strcmp(var, "emitintervalrand"))
			ptype->emitrand = atof(value);
		else if (!strcmp(var, "emitstart"))
			ptype->emitstart = atof(value);

		else if (!strcmp(var, "areaspread"))
			ptype->areaspread = atof(value);
		else if (!strcmp(var, "areaspreadvert"))
			ptype->areaspreadvert = atof(value);
		else if (!strcmp(var, "offsetspread"))
			ptype->offsetspread = atof(value);
		else if (!strcmp(var, "offsetspreadvert"))
			ptype->offsetspreadvert  = atof(value);
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
			while (i <= Cmd_Argc())
			{
				ptype->ramp = BZ_Realloc(ptype->ramp, sizeof(ramp_t)*(ptype->rampindexes+1));

				cidx = atoi(Cmd_Argv(i));
				ptype->ramp[ptype->rampindexes].alpha = cidx > 255 ? 0.5 : 1;

				cidx = d_8to24rgbtable[cidx];
				ptype->ramp[ptype->rampindexes].rgb[0] = (cidx & 0xff) * (1/255.0);
				ptype->ramp[ptype->rampindexes].rgb[1] = (cidx >> 8 & 0xff) * (1/255.0);
				ptype->ramp[ptype->rampindexes].rgb[2] = (cidx >> 16 & 0xff) * (1/255.0);

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

			cidx = d_8to24rgbtable[cidx];
			ptype->ramp[ptype->rampindexes].rgb[0] = (cidx & 0xff) * (1/255.0);
			ptype->ramp[ptype->rampindexes].rgb[1] = (cidx >> 8 & 0xff) * (1/255.0);
			ptype->ramp[ptype->rampindexes].rgb[2] = (cidx >> 16 & 0xff) * (1/255.0);

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
					if (Cmd_Argc()>4)	//have we scale changes?
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
		else
			Con_DPrintf("%s is not a recognised particle type field\n", var);
	}
	ptype->invscalefactor = 1-ptype->scalefactor;
	ptype->loaded = 1;

	if (ptype->rampmode && !ptype->ramp)
	{
		ptype->rampmode = RAMP_NONE;
		Con_Printf("Particle type %s has a ramp mode but no ramp\n", ptype->name);
	}
	else if (ptype->ramp && !ptype->rampmode)
	{
		Con_Printf("Particle type %s has a ramp but no ramp mode\n", ptype->name);
	}

#ifdef RGLQUAKE
	if (qrenderer == QR_OPENGL)
	{
		ptype->texturenum = Mod_LoadHiResTexture(ptype->texname, true, true);
		if (!ptype->texturenum)
			ptype->texturenum = explosiontexture;
	}
#endif
}

void R_AssosiateEffect_f (void)
{
	char *modelname = Cmd_Argv(1);
	char *effectname = Cmd_Argv(2);
	int effectnum;
	model_t *model;

	if (	strstr(modelname, "player") || 
		strstr(modelname, "eyes") || 
		strstr(modelname, "flag") ||
		strstr(modelname, "tf_stan") ||
		strstr(modelname, ".bsp") ||
		strstr(modelname, "turr"))
	{
		Con_Printf("Sorry: Not allowed to attach effects to model \"%s\"\n", modelname);
		return;
	}

	model = Mod_FindName(modelname);
	if (model->flags & EF_ROTATE)
	{
		Con_Printf("Sorry: You may not assosiate effects with item model \"%s\"\n", modelname);
		return;
	}
	effectnum = AllocateParticleType(effectname);
	model->particleeffect = effectnum;
	model->particleengulphs = atoi(Cmd_Argv(3));

	R_Part_Modified();
}

void R_AssosiateTrail_f (void)
{
	char *modelname = Cmd_Argv(1);
	char *effectname = Cmd_Argv(2);
	int effectnum;
	model_t *model;
	
	if (	strstr(modelname, "player") ||
		strstr(modelname, "eyes") ||
		strstr(modelname, "flag") ||
		strstr(modelname, "tf_stan"))
	{
		Con_Printf("Sorry, you can't assosiate trails with model \"%s\"\n", modelname);
		return;
	}

	model = Mod_FindName(modelname);
	effectnum = AllocateParticleType(effectname);
	model->particletrail = effectnum;
	model->nodefaulttrail = true;	//we could have assigned the trail to a model that wasn't loaded.

	R_Part_Modified();
}

void R_DefaultTrail (model_t *model)
{
	if (model->nodefaulttrail == true)
		return;

	if (model->flags & EF_ROCKET)
		model->particletrail = rt_rocket;//q2 models do this without flags.
	else if (model->flags & EF_GRENADE)
		model->particletrail = AllocateParticleType("t_grenade");
	else if (model->flags & EF_GIB)
		model->particletrail = AllocateParticleType("t_gib");
	else if (model->flags & EF_TRACER)
		model->particletrail = AllocateParticleType("t_tracer");
	else if (model->flags & EF_ZOMGIB)
		model->particletrail = AllocateParticleType("t_zomgib");
	else if (model->flags & EF_TRACER2)
		model->particletrail = AllocateParticleType("t_tracer2");
	else if (model->flags & EF_TRACER3)
		model->particletrail = AllocateParticleType("t_tracer3");

	else if (model->flags & EF_BLOODSHOT)
		model->particletrail = AllocateParticleType("t_bloodshot");
	else if (model->flags & EF_FIREBALL)
		model->particletrail = AllocateParticleType("t_fireball");
	else if (model->flags & EF_ACIDBALL)
		model->particletrail = AllocateParticleType("t_acidball");
	else if (model->flags & EF_ICE)
		model->particletrail = AllocateParticleType("t_ice");
	else if (model->flags & EF_SPIT)
		model->particletrail = AllocateParticleType("t_spit");
	else if (model->flags & EF_SPELL)
		model->particletrail = AllocateParticleType("t_spell");
	else if (model->flags & EF_VORP_MISSILE)
		model->particletrail = AllocateParticleType("t_vorpmissile");
	else if (model->flags & EF_SET_STAFF)
		model->particletrail = AllocateParticleType("t_setstaff");
	else if (model->flags & EF_MAGICMISSILE)
		model->particletrail = AllocateParticleType("t_magicmissile");
	else if (model->flags & EF_BONESHARD)
		model->particletrail = AllocateParticleType("t_boneshard");
	else if (model->flags & EF_SCARAB)
		model->particletrail = AllocateParticleType("t_scarab");
	else
		model->particletrail = -1;
}

/*
===============
R_InitParticles
===============
*/
void R_InitParticles (void)
{
	char *particlecvargroupname = "Particle effects";
	int		i;

	if (r_numparticles)	//already inited
		return;

	i = COM_CheckParm ("-particles");

	if (i)
	{
		r_numparticles = (int)(Q_atoi(com_argv[i+1]));
//		if (r_numparticles < ABSOLUTE_MIN_PARTICLES)
//			r_numparticles = ABSOLUTE_MIN_PARTICLES;
	}
	else
	{
		r_numparticles = MAX_PARTICLES;
	}

	particles = (particle_t *)
			Hunk_AllocName (r_numparticles * sizeof(particle_t), "particles");

	Cmd_AddCommand("r_part", R_ParticleEffect_f);
	Cmd_AddCommand("r_effect", R_AssosiateEffect_f);
	Cmd_AddCommand("r_trail", R_AssosiateTrail_f);

	//particles
	Cvar_Register(&r_particlesdesc, particlecvargroupname);
	Cvar_Register(&r_bouncysparks, particlecvargroupname);
	Cvar_Register(&r_particles_in_explosion, particlecvargroupname);
	Cvar_Register(&r_particle_explosion_speed, particlecvargroupname);
	Cvar_Register(&r_part_rain, particlecvargroupname);

	Cvar_Register(&r_part_rain_quantity, particlecvargroupname);
	Cvar_Register(&r_part_rain_colour, particlecvargroupname);

	Cvar_Register(&gl_part_trifansparks, particlecvargroupname);

	pt_explosion		= AllocateParticleType("te_explosion");
	pt_emp				= AllocateParticleType("te_emp");
	pt_pointfile		= AllocateParticleType("te_pointfile");
	pt_entityparticles	= AllocateParticleType("ef_entityparticles");
	pt_darkfield		= AllocateParticleType("ef_darkfield");
	pt_blob				= AllocateParticleType("te_blob");

	pt_blood			= AllocateParticleType("te_blood");
	pt_lightningblood	= AllocateParticleType("te_lightningblood");
	pt_gunshot			= AllocateParticleType("te_gunshot");
	pt_lavasplash		= AllocateParticleType("te_lavasplash");
	pt_teleportsplash	= AllocateParticleType("te_teleportsplash");
	rt_blastertrail		= AllocateParticleType("te_blastertrail");
	pt_blasterparticles = AllocateParticleType("te_blasterparticles");
	pt_wizspike			= AllocateParticleType("te_wizspike");
	pt_knightspike		= AllocateParticleType("te_knightspike");
	pt_spike			= AllocateParticleType("te_spike");
	pt_superspike		= AllocateParticleType("te_superspike");
	rt_railtrail		= AllocateParticleType("te_railtrail");
	rt_bubbletrail		= AllocateParticleType("te_bubbletrail");
	rt_rocket			= AllocateParticleType("t_rocket");

	pt_superbullet		= AllocateParticleType("te_superbullet");
	pt_bullet			= AllocateParticleType("te_bullet");
	pe_default			= AllocateParticleType("pe_default");
	pe_size2			= AllocateParticleType("pe_size2");
	pe_size3			= AllocateParticleType("pe_size3");
}


/*
===============
R_ClearParticles
===============
*/
void R_ClearParticles (void)
{
	int		i;
	
	free_particles = &particles[0];

	for (i=0 ;i<r_numparticles ; i++)
		particles[i].next = &particles[i+1];
	particles[r_numparticles-1].next = NULL;

	particletime = cl.time;

	skytris = NULL;

#ifdef RGLQUAKE
	if (qrenderer == QR_OPENGL)
	{
		for (i = 0; i < numparticletypes; i++)
		{
			if (*part_type[i].texname)
			{
				part_type[i].texturenum = Mod_LoadHiResTexture(part_type[i].texname, true, true);
				if (!part_type[i].texturenum)
					part_type[i].texturenum = explosiontexture;
			}
		}
	}
#endif

	for (i = 0; i < numparticletypes; i++)
		part_type[i].particles = NULL;
}

void R_Part_NewServer(void)
{
	extern model_t	mod_known[];
	extern int		mod_numknown;

	model_t *mod;
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


	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		mod->particleeffect = -1;
		mod->particletrail = -1;
		mod->nodefaulttrail = false;

		R_DefaultTrail(mod);
	}

	f_modified_particles = false;

	//particle descriptions submitted by the server are deemed to not be cheats but game configs.

	if (!stricmp(r_particlesdesc.string, "none"))
		return;
	else if (!stricmp(r_particlesdesc.string, "faithful") || !*r_particlesdesc.string)
		Cbuf_AddText(particle_set_faithful, RESTRICT_SERVER);
	else if (!stricmp(r_particlesdesc.string, "spikeset"))
		Cbuf_AddText(particle_set_spikeset, RESTRICT_SERVER);
	else if (!stricmp(r_particlesdesc.string, "highfps"))
		Cbuf_AddText(particle_set_highfps, RESTRICT_SERVER);
	else
	{
		Cbuf_AddText(va("exec %s.cfg\n", r_particlesdesc.string), RESTRICT_LOCAL);
#if defined(_DEBUG) && defined(WIN32)	//expand the particles cfg into a C style quoted string, and copy to clipboard so I can paste it in.
		{
			char *TL_ExpandToCString(char *in);
			extern HWND mainwindow;
			char *file = COM_LoadTempFile(va("%s.cfg", r_particlesdesc.string));
			char *lptstrCopy, *buf, temp;
			int len;
			HANDLE hglbCopy = GlobalAlloc(GMEM_MOVEABLE, 
				com_filesize*2); 
			lptstrCopy = GlobalLock(hglbCopy); 
			while(file && *file)
			{
				len = strlen(file)+1;
				if (len > 1024)
					len = 1024;
				temp = file[len-1];
				file[len-1] = '\0';
				buf = TL_ExpandToCString(file);
				file[len-1] = temp;
				len-=1;
				com_filesize -= len;
				file+=len;

				len = strlen(buf);
				memcpy(lptstrCopy, buf, len);
				lptstrCopy+=len;
			}
			*lptstrCopy = '\0';
			GlobalUnlock(hglbCopy); 

			if (!OpenClipboard(mainwindow)) 
				return; 
			EmptyClipboard();

			SetClipboardData(CF_TEXT, hglbCopy); 
			CloseClipboard();
		}
#endif
	}
}

void R_ReadPointFile_f (void)
{
	FILE	*f;
	vec3_t	org;
	int		r;
	int		c;
	char	name[MAX_OSPATH];
	
	COM_StripExtension(cl.worldmodel->name, name);
	strcat(name, ".pts");

	COM_FOpenFile (name, &f);
	if (!f)
	{
		Con_Printf ("couldn't open %s\n", name);
		return;
	}

	R_ClearParticles();
	
	Con_Printf ("Reading %s...\n", name);
	c = 0;
	for ( ;; )
	{
		r = fscanf (f,"%f %f %f\n", &org[0], &org[1], &org[2]);
		if (r != 3)
			break;
		c++;

		if (c%8)
			continue;
		
		if (!free_particles)
		{
			Con_Printf ("Not enough free particles\n");
			break;
		}
		R_RunParticleEffectType(org, NULL, 1, pt_pointfile);
	}

	fclose (f);
	Con_Printf ("%i points read\n", c);
}

void R_AddRainParticles(void)
{
	float x;
	float y;
	static float skipped;

	vec3_t org, vdist;

	skytris_t *st;

	if (!r_part_rain.value)
	{
		skipped = true;
		return;
	}

	if (!skytris)	//don't bother (let's use test for the first one for timings)
		return;

	if (skytris->nexttime < particletime - 0.5)
		skipped = true;	//we've gone for half a sec without any new rain. This would cause some strange effects, so reset times.

	if (skipped)
	{
		for (st = skytris; st; st = st->next)
		{
			st->nexttime = particletime;
		}
	}
	skipped = false;

	for (st = skytris; st; st = st->next)
	{
//		if (st->face->visframe != r_framecount)
//			continue;
		while (st->nexttime < particletime)
		{
			if (!free_particles)
				return;

			st->nexttime += 10000/(st->area*r_part_rain_quantity.value);

			x = frandom();
			y = frandom();
			VectorMA(st->org, x, st->x, org);
			VectorMA(org, y, st->y, org);


			VectorSubtract(org, r_refdef.vieworg, vdist);

			if (Length(vdist) > (1024+512)*frandom())
				continue;

			
			if (!(cl.worldmodel->hulls->funcs.HullPointContents(cl.worldmodel->hulls, org) & FTECONTENTS_SOLID))
				R_RunParticleEffectType(org, st->face->plane->normal, 1, st->ptype);
		}
	}
}


void R_Part_SkyTri(float *v1, float *v2, float *v3, msurface_t *surf)
{
	float dot;
	float xm;
	float ym;
	float theta;
	vec3_t xd;
	vec3_t yd;

	skytris_t *st;

	st = Hunk_Alloc(sizeof(skytris_t));
	st->next = skytris;
	VectorCopy(v1, st->org);
	VectorSubtract(v2, st->org, st->x);
	VectorSubtract(v3, st->org, st->y);

	VectorCopy(st->x, xd);
	VectorCopy(st->y, yd);

	xd[2] = 0;	//prevent area from being valid on vertical surfaces
	yd[2] = 0;

	xm = Length(xd);
	ym = Length(yd);

	dot = DotProduct(xd, yd);
	theta = acos(dot/(xm*ym));
	st->area = sin(theta)*xm*ym;
	st->nexttime = particletime;
	st->face = surf;
	st->ptype = surf->texinfo->texture->parttype;

	if (st->area<=0)
		return;//bummer.

	skytris = st;
}



void R_EmitSkyEffectTris(model_t *mod, msurface_t 	*fa)
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






void R_DarkFieldParticles (float *org, qbyte colour)
{
	int			i, j, k;
	vec3_t		dir, norg;

	for (i=-16 ; i<16 ; i+=8)
		for (j=-16 ; j<16 ; j+=8)
			for (k=0 ; k<32 ; k+=8)
			{
				if (!free_particles)
					return;

				norg[0] = org[0] + i + (rand()&3);
				norg[1] = org[1] + j + (rand()&3);
				norg[2] = org[2] + k + (rand()&3);

				dir[0] = j;
				dir[1] = i;
				dir[2] = k;

				R_RunParticleEffectType(norg, dir, 1, pt_darkfield);
			}
}

/*
===============
R_EntityParticles
===============
*/

#define NUMVERTEXNORMALS	162
float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};
vec3_t	avelocities[NUMVERTEXNORMALS];
float	beamlength = 16;
vec3_t	avelocity = {23, 7, 3};
float	partstep = 0.01;
float	timescale = 0.01;

void R_EntityParticles (float *org, qbyte colour, float *radius)
{
	int			count;
	int			i;
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	vec3_t		forward, norg;
	
	count = 50;

if (!avelocities[0][0])
{
for (i=0 ; i<NUMVERTEXNORMALS*3 ; i++)
avelocities[0][i] = (rand()&255) * 0.01;
}


	for (i=0 ; i<NUMVERTEXNORMALS ; i++)	//fixme: make selectable spawnmode.
	{
		if (!free_particles)
			return;

		angle = cl.time * avelocities[i][0];
		sy = sin(angle);
		cy = cos(angle);
		angle = cl.time * avelocities[i][1];
		sp = sin(angle);
		cp = cos(angle);
		angle = cl.time * avelocities[i][2];
		sr = sin(angle);
		cr = cos(angle);

		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;
		
		norg[0] = org[0] + r_avertexnormals[i][0]*radius[0] + forward[0]*beamlength;
		norg[1] = org[1] + r_avertexnormals[i][1]*radius[1] + forward[1]*beamlength;
		norg[2] = org[2] + r_avertexnormals[i][2]*radius[2] + forward[2]*beamlength;

		R_RunParticleEffectType(norg, forward, 1, pt_entityparticles);
	}
}

/*
===============
R_ParticleExplosion

===============
*/
void R_ParticleExplosion (vec3_t org)
{
//	int			i, j;
//	particle_t	*p;

	R_RunParticleEffectType(org, NULL, 1, pt_explosion);

	R_AddStain(org, -1, -1, -1, 100);
/*
	for (i=0 ; i<r_particles_in_explosion.value ; i++)
	{
		if (!free_particles)
			return;
		if ((rand()&3)==3)
		{
			p = free_particles;
			free_particles = p->next;
			p->next = active_sparks;
			active_sparks = p;

			p->scale = 1;
			p->alpha = 0.8 + (rand()&15)/(15.0*5);
			p->die = particletime + 7;
			p->color = ramp1[0];
			p->ramp = rand()&3;
			if (i & 1)
			{
				p->type = st_shrapnal;
				for (j=0 ; j<3 ; j++)
				{
					p->org[j] = org[j];// + ((rand()%32)-16);
					p->vel[j] = ((rand()%512)-256)*r_particle_explosion_speed.value;
				}
			}
			else
			{
				p->type = pt_grav;
				for (j=0 ; j<3 ; j++)
				{
					p->org[j] = org[j];// + ((rand()%32)-16);
					p->vel[j] = ((rand()%512)-256)*r_particle_explosion_speed.value;
				}
			}
		}
		else
		{
			p = free_particles;
			free_particles = p->next;
			p->next = active_sparks;
			active_sparks = p;

			p->scale = 1;
			p->alpha = 0.8;
			p->die = particletime + 7;
			p->color = ramp1[0];
			p->ramp = rand()&3;
			if (i & 1)
			{
				p->type = st_shrapnal;
				for (j=0 ; j<3 ; j++)
				{
					p->org[j] = org[j];// + ((rand()%32)-16);
					p->vel[j] = ((rand()%512)-256)*r_particle_explosion_speed.value;
				}
			}
			else
			{
				p->type = st_shrapnal;
				for (j=0 ; j<3 ; j++)
				{
					p->org[j] = org[j];// + ((rand()%32)-16);
					p->vel[j] = ((rand()%512)-256)*r_particle_explosion_speed.value;
				}
			}
		}
	}*/
}

/*
===============
R_BlobExplosion

===============
*/
void R_BlobExplosion (vec3_t org)
{
	R_RunParticleEffectType(org, NULL, 1, pt_blob);
}

int R_RunParticleEffectType (vec3_t org, vec3_t dir, float count, int typenum)
{
	part_type_t *ptype = &part_type[typenum];
	int i, j, k, l;
	particle_t	*p;

	if (typenum == 0)
		typenum = rand()&15;

	if (typenum < 0)
		return 1;

	if (!ptype->loaded)
		return 1;

	if (!ptype->scale)
		return 1;

	while(ptype)
	{
		switch (ptype->spawnmode) 
		{
		case SM_BOX:
			for (i = 0; i < count*ptype->count; i++)
			{
				if (!free_particles)
					return 0;
				p = free_particles;
				free_particles = p->next;
				p->next = ptype->particles;
				ptype->particles = p;

				p->die = ptype->randdie*frandom();
				p->scale = ptype->scale+ptype->randscale*frandom();
				p->alpha = ptype->alpha-p->die*(ptype->alpha/ptype->die)*ptype->alphachange;
				p->color = 0;
				p->nextemit = particletime + ptype->emitstart - p->die;

				if (ptype->colorindex >= 0)
				{
					int cidx;
					cidx = ptype->colorrand > 0 ? rand() % ptype->colorrand : 0;
					cidx = ptype->colorindex + cidx;
					if (cidx > 255)
						p->alpha = p->alpha / 2;
					cidx = d_8to24rgbtable[cidx & 0xff];
					p->rgb[0] = (cidx & 0xff) * (1/255.0);
					p->rgb[1] = (cidx >> 8 & 0xff) * (1/255.0);
					p->rgb[2] = (cidx >> 16 & 0xff) * (1/255.0);
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

				p->org[0] = crandom();
				p->org[1] = crandom();
				p->org[2] = crandom();

				p->vel[0] = crandom()*ptype->randomvel;
				p->vel[1] = crandom()*ptype->randomvel;
				p->vel[2] = crandom()*ptype->randomvelvert;

				if (dir)
				{
					p->vel[0] += dir[0]*ptype->veladd+p->org[0]*ptype->offsetspread;
					p->vel[1] += dir[1]*ptype->veladd+p->org[1]*ptype->offsetspread;
					p->vel[2] += dir[2]*ptype->veladd+p->org[2]*ptype->offsetspreadvert;
				}
				else
				{
					p->vel[0] += p->org[0]*ptype->offsetspread;
					p->vel[1] += p->org[1]*ptype->offsetspread;
					p->vel[2] += p->org[2]*ptype->offsetspreadvert - ptype->veladd;
				}
				p->org[0] = org[0] + p->org[0]*ptype->areaspread;
				p->org[1] = org[1] + p->org[1]*ptype->areaspread;
				p->org[2] = org[2] + p->org[2]*ptype->areaspreadvert + ptype->offsetup;

				p->die = particletime + ptype->die - p->die;
			}
			break;
		case SM_TELEBOX:
			j = k = -ptype->areaspread;
			l = -ptype->areaspreadvert;

			for (i = 0; i < count*ptype->count; i++)
			{
				if (!free_particles)
					return 0;
				p = free_particles;
				free_particles = p->next;
				p->next = ptype->particles;
				ptype->particles = p;

				p->die = ptype->randdie*frandom();
				p->scale = ptype->scale+ptype->randscale*frandom();
				p->alpha = ptype->alpha-p->die*(ptype->alpha/ptype->die)*ptype->alphachange;
				p->color = 0;
				p->nextemit = particletime + ptype->emitstart - p->die;

				if (ptype->colorindex >= 0)
				{
					int cidx;
					cidx = ptype->colorrand > 0 ? rand() % ptype->colorrand : 0;
					cidx = ptype->colorindex + cidx;
					if (cidx > 255)
						p->alpha = p->alpha / 2;
					cidx = d_8to24rgbtable[cidx & 0xff];
					p->rgb[0] = (cidx & 0xff) * (1/255.0);
					p->rgb[1] = (cidx >> 8 & 0xff) * (1/255.0);
					p->rgb[2] = (cidx >> 16 & 0xff) * (1/255.0);
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

				p->vel[0] = crandom()*ptype->randomvel;
				p->vel[1] = crandom()*ptype->randomvel;
				p->vel[2] = crandom()*ptype->randomvelvert;

				// use org to store temp for particle dir
				p->org[0] = k;
				p->org[1] = j;
				p->org[2] = l+4;
				VectorNormalize(p->org);
				VectorScale(p->org, 1.0-(frandom())*0.55752, p->org);

				if (dir)
				{
					p->vel[0] += dir[0]*ptype->veladd+p->org[0]*ptype->offsetspread;
					p->vel[1] += dir[1]*ptype->veladd+p->org[1]*ptype->offsetspread;
					p->vel[2] += dir[2]*ptype->veladd+p->org[2]*ptype->offsetspreadvert;
				}
				else
				{
					p->vel[0] += p->org[0]*ptype->offsetspread;
					p->vel[1] += p->org[1]*ptype->offsetspread;
					p->vel[2] += p->org[2]*ptype->offsetspreadvert - ptype->veladd;
				}

				// org is just like the original
				p->org[0] = org[0] + j + (rand()&3);
				p->org[1] = org[1] + k + (rand()&3);
				p->org[2] = org[2] + l + (rand()&3) + ptype->offsetup;

				p->die = particletime + ptype->die - p->die;

				// advance telebox loop
				j += 4;
				if (j >= ptype->areaspread)
				{
					j = -ptype->areaspread;
					k += 4;
					if (k >= ptype->areaspread)
					{
						k = -ptype->areaspread;
						l += 4;
						if (l >= ptype->areaspreadvert)
							l = -ptype->areaspreadvert;
					}
				}
			}
			break;
		case SM_LAVASPLASH:
			j = k = -ptype->areaspread;

			for (i = 0; i < count*ptype->count; i++)
			{
				if (!free_particles)
					return 0;
				p = free_particles;
				free_particles = p->next;
				p->next = ptype->particles;
				ptype->particles = p;

				p->die = ptype->randdie*frandom();
				p->scale = ptype->scale+ptype->randscale*frandom();
				p->alpha = ptype->alpha-p->die*(ptype->alpha/ptype->die)*ptype->alphachange;
				p->color = 0;
				p->nextemit = particletime + ptype->emitstart - p->die;

				if (ptype->colorindex >= 0)
				{
					int cidx;
					cidx = ptype->colorrand > 0 ? rand() % ptype->colorrand : 0;
					cidx = ptype->colorindex + cidx;
					if (cidx > 255)
						p->alpha = p->alpha / 2;
					cidx = d_8to24rgbtable[cidx & 0xff];
					p->rgb[0] = (cidx & 0xff) * (1/255.0);
					p->rgb[1] = (cidx >> 8 & 0xff) * (1/255.0);
					p->rgb[2] = (cidx >> 16 & 0xff) * (1/255.0);
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

				p->vel[0] = crandom()*ptype->randomvel;
				p->vel[1] = crandom()*ptype->randomvel;
				p->vel[2] = crandom()*ptype->randomvelvert;

				// calc directions, org with temp vector
				{
					vec3_t temp;

					temp[0] = k*8 + (rand()&7);
					temp[1] = j*8 + (rand()&7);
					temp[2] = 256;

					// calc org first
					p->org[0] = org[0] + temp[0];
					p->org[1] = org[1] + temp[1];
					p->org[2] = org[2] + frandom()*ptype->areaspreadvert + ptype->offsetup;

					VectorNormalize(temp);
					VectorScale(temp, 1.0-(frandom())*0.55752, temp);

					if (dir)
					{
						p->vel[0] += dir[0]*ptype->veladd+temp[0]*ptype->offsetspread;
						p->vel[1] += dir[1]*ptype->veladd+temp[1]*ptype->offsetspread;
						p->vel[2] += dir[2]*ptype->veladd+temp[2]*ptype->offsetspreadvert;
					}
					else
					{
						p->vel[0] += temp[0]*ptype->offsetspread;
						p->vel[1] += temp[1]*ptype->offsetspread;
						p->vel[2] += temp[2]*ptype->offsetspreadvert - ptype->veladd;
					}

				}

				p->die = particletime + ptype->die - p->die;

				// advance splash loop
				j++;
				if (j >= ptype->areaspread)
				{
					j = -ptype->areaspread;
					k++;
					if (k >= ptype->areaspread)
					k = -ptype->areaspread;
				}
			}
			break;
		default: // circle
			for (i = 0; i < count*ptype->count; i++)
			{
				if (!free_particles)
					return 0;
				p = free_particles;
				free_particles = p->next;
				p->next = ptype->particles;
				ptype->particles = p;

				p->die = ptype->randdie*frandom();
				p->scale = ptype->scale+ptype->randscale*frandom();
				p->alpha = ptype->alpha-p->die*(ptype->alpha/ptype->die)*ptype->alphachange;
				p->color = 0;
				p->nextemit = particletime + ptype->emitstart - p->die;

				if (ptype->colorindex >= 0)
				{
					int cidx;
					cidx = ptype->colorrand > 0 ? rand() % ptype->colorrand : 0;
					cidx = ptype->colorindex + cidx;
					if (cidx > 255)
						p->alpha = p->alpha / 2;
					cidx = d_8to24rgbtable[cidx & 0xff];
					p->rgb[0] = (cidx & 0xff) * (1/255.0);
					p->rgb[1] = (cidx >> 8 & 0xff) * (1/255.0);
					p->rgb[2] = (cidx >> 16 & 0xff) * (1/255.0);
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

				p->org[0] = hrandom();
				p->org[1] = hrandom();
				if (ptype->areaspreadvert)
					p->org[2] = hrandom();
				else
					p->org[2] = 0;

				VectorNormalize(p->org);
				if (ptype->spawnmode != SM_CIRCLE)
					VectorScale(p->org, frandom(), p->org);

				p->vel[0] = crandom()*ptype->randomvel;
				p->vel[1] = crandom()*ptype->randomvel;
				p->vel[2] = crandom()*ptype->randomvelvert;

				if (dir)
				{
					p->vel[0] += dir[0]*ptype->veladd+org[0]*ptype->offsetspread;
					p->vel[1] += dir[1]*ptype->veladd+org[1]*ptype->offsetspread;
					p->vel[2] += dir[2]*ptype->veladd+org[2]*ptype->offsetspreadvert;
				}
				else
				{
					p->vel[0] += p->org[0]*ptype->offsetspread;
					p->vel[1] += p->org[1]*ptype->offsetspread;
					p->vel[2] += p->org[2]*ptype->offsetspreadvert - ptype->veladd;

				}
				p->org[0] = org[0] + p->org[0]*ptype->areaspread;
				p->org[1] = org[1] + p->org[1]*ptype->areaspread;
				p->org[2] = org[2] + p->org[2]*ptype->areaspreadvert + ptype->offsetup;

				p->die = particletime + ptype->die - p->die;
			}
		}


		if (ptype->assoc < 0)
			break;
		ptype = &part_type[ptype->assoc];
	}

	return 0;
}

/*
===============
R_RunParticleEffect

===============
*/
void R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
	int ptype;

#if 0
	if (color == 73)
	{	//blood
		R_RunParticleEffectType(org, dir, count, pt_blood);
		return;
	}
	if (color == 225)
	{	//lightning blood	//a brighter red...
		R_RunParticleEffectType(org, dir, count, pt_lightningblood);
		return;
	}

	if (color == 0)
	{	//lightning blood
		R_RunParticleEffectType(org, dir, count, pt_gunshot);
		return;
	}
#endif

	ptype = FindParticleType(va("pe_%i", color));
	if (R_RunParticleEffectType(org, dir, count, ptype))
	{
		if (count > 130 && part_type[pe_size3].loaded)
		{
			part_type[pe_size3].colorindex = color & ~0x7;
			part_type[pe_size3].colorrand = 8;
			R_RunParticleEffectType(org, dir, count, pe_size3);
			return;
		}
		if (count > 20 && part_type[pe_size2].loaded)
		{
			part_type[pe_size2].colorindex = color & ~0x7;
			part_type[pe_size2].colorrand = 8;
			R_RunParticleEffectType(org, dir, count, pe_size2);
			return;
		}
		part_type[pe_default].colorindex = color & ~0x7;
		part_type[pe_default].colorrand = 8;
		R_RunParticleEffectType(org, dir, count, pe_default);
		return;
	}
}

//h2 stylie
void R_RunParticleEffect2 (vec3_t org, vec3_t dmin, vec3_t dmax, int color, int effect, int count)
{
	int			i, j;
	float		num;
	vec3_t	nvel;

	int ptype = FindParticleType(va("pe2_%i_%i", effect, color));
	if (ptype < 0)
		return;

	for (i=0 ; i<count ; i++)
	{
		if (!free_particles)
			return;

		for (j=0 ; j<3 ; j++)
		{
			num = rand() / (float)RAND_MAX;
			nvel[j] = dmin[j] + ((dmax[j] - dmin[j]) * num);
		}
		R_RunParticleEffectType(org, nvel, 1, ptype);

	}
}

/*
===============
R_RunParticleEffect3

===============
*/
//h2 stylie
void R_RunParticleEffect3 (vec3_t org, vec3_t box, int color, int effect, int count)
{
	int			i, j;
	vec3_t	nvel;
	float		num;

	int ptype = FindParticleType(va("pe3_%i_%i", effect, color));
	if (ptype < 0)
		return;

	for (i=0 ; i<count ; i++)
	{
		if (!free_particles)
			return;

		for (j=0 ; j<3 ; j++)
		{
			num = rand() / (float)RAND_MAX;
			nvel[j] = (box[j] * num * 2) - box[j];
		}

		R_RunParticleEffectType(org, nvel, 1, ptype);
	}
}

/*
===============
R_RunParticleEffect4

===============
*/
//h2 stylie
void R_RunParticleEffect4 (vec3_t org, float radius, int color, int effect, int count)
{
	int			i, j;
	vec3_t	nvel;
	float		num;

	int ptype = FindParticleType(va("pe4_%i_%i", effect, color));
	if (ptype < 0)
		return;

	for (i=0 ; i<count ; i++)
	{
		if (!free_particles)
			return;

		for (j=0 ; j<3 ; j++)
		{
			num = rand() / (float)RAND_MAX;
			nvel[j] = (radius * num * 2) - radius;
		}
		R_RunParticleEffectType(org, nvel, 1, ptype);
	}
}





/*
===============
R_LavaSplash

===============
*/
void R_LavaSplash (vec3_t org)
{
	R_RunParticleEffectType(org, NULL, 1, pt_lavasplash);
}

/*
===============
R_TeleportSplash

===============
*/
void R_TeleportSplash (vec3_t org)
{
	R_RunParticleEffectType(org, NULL, 1, pt_teleportsplash);
}

void CLQ2_BlasterTrail (vec3_t start, vec3_t end)
{
	R_RocketTrail(start, end, rt_blastertrail, 0);
}
void R_BlasterParticles (vec3_t start, vec3_t dir)
{
	R_RunParticleEffectType(start, dir, 1, pt_blasterparticles);
}

void MakeNormalVectors (vec3_t forward, vec3_t right, vec3_t up)
{
	float		d;

	// this rotate and negat guarantees a vector
	// not colinear with the original
	right[1] = -forward[0];
	right[2] = forward[1];
	right[0] = forward[2];

	d = DotProduct (right, forward);
	VectorMA (right, -d, forward, right);
	VectorNormalize (right);
	CrossProduct (right, forward, up);
}

void CLQ2_RailTrail (vec3_t start, vec3_t end)
{
	R_RocketTrail(start, end, rt_railtrail, 0);
}


float R_RocketTrail (vec3_t start, vec3_t end, int type, float lastdistance)
{
	vec3_t	vec, right, up;
	float	len;
	int			j;
	static int tcount;
	particle_t	*p;
	part_type_t *ptype = &part_type[type];

	float veladd = -ptype->veladd;
	float randvel = ptype->randomvel*2;
	float step;
	float stop;

	if (!ptype->loaded)
		return lastdistance;

	if (ptype->assoc>=0)
	{
		VectorCopy(start, vec);
		R_RocketTrail(vec, end, ptype->assoc, lastdistance);
	}
	step = 1/ptype->count;

	if (!ptype->scale)
		return lastdistance;
	if (step < 0.01)
		step = 0.01;

	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);
	if (len > 1024)
		return lastdistance;

	// add offset
	start[2] += ptype->offsetup;

	if (ptype->spawnmode == SM_SPIRAL)
	{
		VectorVectors(vec, right, up);
		VectorScale(right, ptype->offsetspread, right);
		VectorScale(up, ptype->offsetspread, up);
	}

	stop = lastdistance + len;	//when to stop

	len = lastdistance/step;
	len = (len - (int)len)*step;
//	VectorMA (start, -len, vec, start);

	len = lastdistance;

	while (len < stop)
	{
		len += step;

		if (!free_particles)
			return stop;
		p = free_particles;
		free_particles = p->next;
		p->next = ptype->particles;
		ptype->particles = p;

		p->die = ptype->randdie*frandom();
		p->scale = ptype->scale+ptype->randscale*frandom();
		VectorCopy (vec3_origin, p->vel);
		p->alpha = ptype->alpha-p->die*(ptype->alpha/ptype->die)*ptype->alphachange;
		p->color = 0;
		p->nextemit = particletime + ptype->emitstart - p->die;

		if (ptype->colorindex >= 0)
		{
			int cidx;
			cidx = ptype->colorrand > 0 ? rand() % ptype->colorrand : 0;
			if (ptype->citracer) // colorindex behavior as per tracers in std Q1
				cidx += ((tcount & 4) << 1);

			cidx = ptype->colorindex + cidx;
			if (cidx > 255)
				p->alpha = p->alpha / 2;
			cidx = d_8to24rgbtable[cidx & 0xff];
			p->rgb[0] = (cidx & 0xff) * (1/255.0);
			p->rgb[1] = (cidx >> 8 & 0xff) * (1/255.0);
			p->rgb[2] = (cidx >> 16 & 0xff) * (1/255.0);
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

		switch(ptype->spawnmode)
		{
		case SM_TRACER:
			VectorCopy(start, p->org);
			tcount++;

			if (tcount & 1)
			{
				p->vel[0] = vec[1]*ptype->areaspread;
				p->vel[1] = -vec[0]*ptype->areaspread;
			}
			else
			{
				p->vel[0] = -vec[1]*ptype->areaspread;
				p->vel[1] = vec[0]*ptype->areaspread;
			}

			p->vel[0] += vec[0]*veladd+crandom()*randvel;
			p->vel[1] += vec[1]*veladd+crandom()*randvel;
			p->vel[2] = vec[2]*veladd+crandom()*randvel;
			break;
		case SM_SPIRAL:
			p->vel[0] = cos(len/50);
			p->vel[1] = sin(len/50);
			for (j=0 ; j<3 ; j++)
				p->org[j] = start[j] + right[j]*p->vel[0] + up[j]*p->vel[1];

			p->vel[0] = vec[0]*veladd+crandom()*randvel;
			p->vel[1] = vec[1]*veladd+crandom()*randvel;
			p->vel[2] = vec[2]*veladd+crandom()*randvel;
			break;
		default:
			p->org[0] = start[0] + crandom()*ptype->areaspread;
			p->org[1] = start[1] + crandom()*ptype->areaspread;
			p->org[2] = start[2] + crandom()*ptype->areaspreadvert;

			p->vel[0] = vec[0]*veladd+crandom()*randvel;
			p->vel[1] = vec[1]*veladd+crandom()*randvel;
			p->vel[2] = vec[2]*veladd+crandom()*randvel;
			break;
		}

		VectorMA (start, step, vec, start);

		p->die = particletime + ptype->die - p->die;
	}

	return len;	//distance the trail actually moved.
}

void R_TorchEffect (vec3_t pos, int type)
{
#ifdef SIDEVIEWS
	if (r_secondaryview)	//this is called when the models are actually drawn.
		return;
#endif
	if (cl.paused)
		return;

 	R_RunParticleEffectType(pos, NULL, host_frametime, type);
}

void CLQ2_BubbleTrail (vec3_t start, vec3_t end)
{
	R_RocketTrail(start, end, rt_bubbletrail, 0);
}

#ifdef Q2BSPS
qboolean Q2TraceLineN (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal)
{
	vec3_t nul = {0,0,0};
	trace_t trace = CM_BoxTrace(start, end, nul, nul, pmove.physents[0].model->hulls[0].firstclipnode, MASK_SOLID);

	if (trace.fraction < 1)
	{
		VectorCopy (trace.plane.normal, normal);
		VectorCopy (trace.endpos, impact);
		return true;
	}
	return false;
}
#endif

qboolean DoomTraceLineN (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal)
{
	return false;
}

#if 1	//extra code to make em bounce of doors.
qboolean TraceLineN (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal)
{
	trace_t		trace;
	float len, bestlen;
	int i;
	vec3_t delta, ts, te;
	physent_t *pe;
	hull_t *hull;
	qboolean clipped=false;

	memset (&trace, 0, sizeof(trace));

	VectorSubtract(end, start, delta);
	bestlen = Length(delta);

	VectorCopy (end, impact);

	for (i=0 ; i< pmove.numphysent ; i++)
	{
		pe = &pmove.physents[i];	
		if (pe->model)
		{
			hull = &pe->model->hulls[0];
			memset (&trace, 0, sizeof(trace));
			VectorSubtract(start, pe->origin, ts);
			VectorSubtract(end, pe->origin, te);
			if (!hull->funcs.RecursiveHullCheck (hull, hull->firstclipnode, 0, 1, ts, te, &trace))
			{
				VectorSubtract(trace.endpos, ts, delta);
				len = Length(delta);
				if (len < bestlen)
				{
					bestlen = len;
					VectorCopy (trace.plane.normal, normal);
					VectorAdd (pe->origin, trace.endpos, impact);
				}

				clipped=true;
			}
			if (trace.startsolid)
			{
				VectorNormalize(delta);
				normal[0] = -delta[0];
				normal[1] = -delta[1];
				normal[2] = -delta[2];
				return true;
			}

		}
	}

	if (clipped)
	{
		return true;
	}
	else
	{
		return false;
	}
}

#else	//basic (faster)
qboolean TraceLineN (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal)
{
	pmtrace_t	trace;

	memset (&trace, 0, sizeof(trace));
	trace.fraction = 1;
	if (cl.worldmodel->hulls->funcs.RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, start, end, &trace))
		return false;

	if (trace.startsolid)
		return true;

	VectorCopy (trace.endpos, impact);
	VectorCopy (trace.plane.normal, normal);

	return true;
}
#endif


part_type_t *lasttype;
static vec3_t pright, pup;
float pframetime;
#ifdef RGLQUAKE
void GL_DrawTexturedParticle(particle_t *p, part_type_t *type)
{
	float scale;
	 float texrot[7][2] ={
		{0, 0},
		{1, 0},
		{1, 1},
		{0, 1}
	};

	if (lasttype != type)
	{
		if (type-part_type>=numparticletypes||type-part_type<0)	//FIXME:! Work out why this is needed...
		{
			Con_Printf("Serious bug alert\n");
			return;
		}

		lasttype = type;
		glEnd();
		glEnable(GL_TEXTURE_2D);
		GL_Bind(type->texturenum);
		if (type->blendmode == BM_ADD)		//addative
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
//		else if (type->blendmode == BM_SUBTRACT)	//subtractive
//			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		else
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glShadeModel(GL_FLAT);
		glBegin(GL_QUADS);
	}

	scale = (p->org[0] - r_origin[0])*vpn[0] + (p->org[1] - r_origin[1])*vpn[1]
		+ (p->org[2] - r_origin[2])*vpn[2];
	scale = (scale*p->scale)*(type->invscalefactor) + p->scale * (type->scalefactor*250);
	if (scale < 20)
		scale = 1;
	else
		scale = 1 + scale * 0.004;
	scale/=4;
	
	glColor4f (	p->rgb[0],
				p->rgb[1],
				p->rgb[2],
				p->alpha);

	glTexCoord2fv (texrot[((int)p/sizeof(*p))&3]);
	glVertex3f (p->org[0] - pright[0]*scale - pup[0]*scale, p->org[1] - pright[1]*scale - pup[1]*scale, p->org[2] - pright[2]*scale - pup[2]*scale);
	glTexCoord2fv (texrot[((int)p/sizeof(*p)+1)&3]);	//bottom left
	glVertex3f (p->org[0] - pright[0]*scale + pup[0]*scale, p->org[1] - pright[1]*scale + pup[1]*scale, p->org[2] - pright[2]*scale + pup[2]*scale);
	glTexCoord2fv (texrot[((int)p/sizeof(*p)+2)&3]);	//bottom right
	glVertex3f (p->org[0] + pright[0]*scale + pup[0]*scale, p->org[1] + pright[1]*scale + pup[1]*scale, p->org[2] + pright[2]*scale + pup[2]*scale);
	glTexCoord2fv (texrot[((int)p/sizeof(*p)+3)&3]);	//top right
	glVertex3f (p->org[0] + pright[0]*scale - pup[0]*scale, p->org[1] + pright[1]*scale - pup[1]*scale, p->org[2] + pright[2]*scale - pup[2]*scale);
}

void GL_DrawTrifanParticle(particle_t *p, part_type_t *type)
{
	int i;
	vec3_t v;
	float scale;

	if (lasttype != type)
	{
		if (type-part_type>=numparticletypes||type-part_type<0)	//FIXME:! Work out why this is needed...
		{
			Con_Printf("Serious bug alert\n");
			return;
		}

		lasttype = type;
		glEnd();
		glDisable(GL_TEXTURE_2D);
		if (type->blendmode == BM_ADD)		//addative
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
//		else if (type->blendmode == BM_SUBTRACT)	//subtractive
//			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		else
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glShadeModel(GL_SMOOTH);
	}

	scale = (p->org[0] - r_origin[0])*vpn[0] + (p->org[1] - r_origin[1])*vpn[1]
		+ (p->org[2] - r_origin[2])*vpn[2];
	scale = (scale*p->scale)*(type->invscalefactor) + p->scale * (type->scalefactor*250);
	if (scale < 20)
		scale = 1;
	else
		scale = 1 + scale * 0.004;
	scale/=4;
	scale/=5;
/*
	if ((p->vel[0]*p->vel[0]+p->vel[1]*p->vel[1]+p->vel[2]*p->vel[2])*2*scale > 30*30)
		scale = 1+1/30/Length(p->vel)*2;*/
	
	glBegin (GL_TRIANGLE_FAN);
	glColor4f (	p->rgb[0],
				p->rgb[1],
				p->rgb[2],
				p->alpha);
	glVertex3fv (p->org);
	glColor4f (	p->rgb[0]/2,
				p->rgb[1]/2,
				p->rgb[2]/2,
				0);
	for (i=7 ; i>=0 ; i--)
	{
		v[0] = p->org[0] - p->vel[0]*scale + vright[0]*cost[i%7]*p->scale + vup[0]*sint[i%7]*p->scale;
		v[1] = p->org[1] - p->vel[1]*scale + vright[1]*cost[i%7]*p->scale + vup[1]*sint[i%7]*p->scale;
		v[2] = p->org[2] - p->vel[2]*scale + vright[2]*cost[i%7]*p->scale + vup[2]*sint[i%7]*p->scale;
		glVertex3fv (v);
	}
	glEnd ();
}

void GL_DrawSparkedParticle(particle_t *p, part_type_t *type)
{
	if (lasttype != type)
	{
		if (type-part_type>=numparticletypes||type-part_type<0)	//FIXME:! Work out why this is needed...
		{
			Con_Printf("Serious bug alert\n");
			return;
		}
		lasttype = type;
		glEnd();
		glDisable(GL_TEXTURE_2D);
		GL_Bind(type->texturenum);
		if (type->blendmode == BM_ADD)		//addative
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
//		else if (type->blendmode == BM_SUBTRACT)	//subtractive
//			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		else
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glShadeModel(GL_SMOOTH);
		glBegin(GL_LINES);
	}

	glColor4f (	p->rgb[0],
				p->rgb[1],
				p->rgb[2],
				p->alpha);
	glVertex3f (p->org[0], p->org[1], p->org[2]);

	glColor4f (	p->rgb[0],
				p->rgb[1],
				p->rgb[2],
				0);
	glVertex3f (p->org[0]-p->vel[0]/10, p->org[1]-p->vel[1]/10, p->org[2]-p->vel[2]/10);

}
#endif
#ifdef SWQUAKE
void SWD_DrawParticleSpark(particle_t *p, part_type_t *type)
{
	int r,g,b;	//if you have a cpu with mmx, good for you...
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
	p->color = GetPalette(r, g, b);
	D_DrawSparkTrans(p);
}
void SWD_DrawParticleBlob(particle_t *p, part_type_t *type)
{
	int r,g,b;	//This really shouldn't be like this. Pitty the 32 bit renderer...
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
	p->color = GetPalette(r, g, b);
	D_DrawParticleTrans(p);
}
#endif
void DrawParticleTypes (void texturedparticles(void*,void*), void sparkparticles(void*,void*))
{
	qboolean (*tr) (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal);

	int i;
	vec3_t oldorg;
	vec3_t stop, normal;
	part_type_t *type;
	particle_t		*p, *kill;
	ramp_t *ramp;
	float grav;
	vec3_t friction;
	float dist;

	lasttype = NULL;

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


	for (i = 0; i < numparticletypes; i++)
	{
		type = &part_type[i];

		if (!type->die)
		{
			while ((p=type->particles))
			{
				if (*type->texname)
					RQ_AddDistReorder(texturedparticles, p, type, p->org);
				else
					RQ_AddDistReorder(sparkparticles, p, type, p->org);

				type->particles = p->next;
				p->next = free_particles;
				free_particles = p;
			}
			continue;
		}

		grav = type->gravity*pframetime;
		VectorScale(type->friction, pframetime, friction);
		for ( ;; ) 
		{
			kill = type->particles;
			if (kill && kill->die < particletime)
			{
				type->particles = kill->next;
				kill->next = free_particles;
				free_particles = kill;
				continue;
			}
			break;
		}

		for (p=type->particles ; p ; p=p->next)
		{
			for ( ;; )
			{
				kill = p->next;
				if (kill && kill->die < particletime)
				{
					p->next = kill->next;
					kill->next = free_particles;
					free_particles = kill;
					continue;
				}
				break;
			}
			VectorCopy(p->org, oldorg);
			p->org[0] += p->vel[0]*pframetime;
			p->org[1] += p->vel[1]*pframetime;
			p->org[2] += p->vel[2]*pframetime;
			p->vel[0] -= friction[0]*p->vel[0];
			p->vel[1] -= friction[1]*p->vel[1];
			p->vel[2] -= friction[2]*p->vel[2];
			p->vel[2] -= grav;

			switch (type->rampmode)
			{
			case RAMP_ABSOLUTE:
				ramp = type->ramp + (int)(type->rampindexes * (type->die - (p->die - particletime)) / type->die);
				VectorCopy(ramp->rgb, p->rgb);
				p->alpha = ramp->alpha;
				p->scale = ramp->scale;
				break;
			case RAMP_DELTA:	//particle ramps
				ramp = type->ramp + (int)(type->rampindexes * (type->die - (p->die - particletime)) / type->die);
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
				p->alpha -= pframetime*(type->alpha/type->die)*type->alphachange;
				p->scale += pframetime*type->scaledelta;
			}

			if (type->emit >= 0)
			{
				if (type->emittime < 0)
					R_RocketTrail(oldorg, p->org, type->emit, 0);
				else if (p->nextemit < particletime)
				{
					p->nextemit = particletime + type->emittime + frandom()*type->emitrand;
					R_RunParticleEffectType(p->org, p->vel, 1, type->emit);
				}
			}

			if (type->cliptype>=0 && r_bouncysparks.value)
			{
				if (tr(oldorg, p->org, stop, normal))
				{
					if (type->stains && r_bloodstains.value)
						R_AddStain(stop,	p->rgb[1]*-10+p->rgb[2]*-10,
											p->rgb[0]*-10+p->rgb[2]*-10,
											p->rgb[0]*-10+p->rgb[1]*-10,
											30*p->alpha);

					if (type->cliptype == i)
					{	//bounce
						dist = DotProduct(p->vel, normal) * (-1-(rand()/(float)0x7fff)/2);
				
						VectorMA(p->vel, dist, normal, p->vel);
						VectorCopy(stop, p->org);
						p->vel[0] *= 0.8;
						p->vel[1] *= 0.8;
						p->vel[2] *= 0.8;

						if (!*type->texname && Length(p->vel)<1000*pframetime)
							p->die = -1;
					}
					else
					{
						p->die = -1;
						VectorNormalize(p->vel);
						R_RunParticleEffectType(stop, p->vel, type->clipcount/part_type[type->cliptype].count, type->cliptype);
					}

					continue;
				}
			}
			else if (type->stains && r_bloodstains.value)
			{
				if (tr(oldorg, p->org, stop, normal))
				{
					R_AddStain(stop,	p->rgb[1]*-10+p->rgb[2]*-10,
										p->rgb[0]*-10+p->rgb[2]*-10,
										p->rgb[0]*-10+p->rgb[1]*-10,
										30*p->alpha);
					p->die = -1;
					continue;
				}
			}

			if (*type->texname)
				RQ_AddDistReorder(texturedparticles, p, type, p->org);
			else
				RQ_AddDistReorder(sparkparticles, p, type, p->org);
		}
	}

	RQ_RenderDistAndClear();


	particletime += pframetime;
}

/*
===============
R_DrawParticles
===============
*/
void R_DrawParticles (void)
{
	R_AddRainParticles();
#if defined(RGLQUAKE)
	if (qrenderer == QR_OPENGL)
	{
		glDepthMask(0);
		
		glDisable(GL_ALPHA_TEST);
		glEnable (GL_BLEND);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glBegin(GL_QUADS);

		if (gl_part_trifansparks.value)
			DrawParticleTypes(GL_DrawTexturedParticle, GL_DrawTrifanParticle);
		else
			DrawParticleTypes(GL_DrawTexturedParticle, GL_DrawSparkedParticle);

		glEnd();
		glEnable(GL_TEXTURE_2D);

		glDepthMask(1);
		return;
	}
#endif
#ifdef SWQUAKE
	if (qrenderer == QR_SOFTWARE)
	{
		D_StartParticles();
		DrawParticleTypes(SWD_DrawParticleBlob, SWD_DrawParticleSpark);
		D_EndParticles();
		return;
	}
#endif
}


