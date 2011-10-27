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
// cl_tent.c -- client side temporary entities

#include "quakedef.h"
#include "particles.h"
entity_state_t *CL_FindPacketEntity(int num);

#define R_AddDecals(a)	//disabled for now

int
	pt_gunshot=P_INVALID,
	ptdp_gunshotquad=P_INVALID,
	pt_spike=P_INVALID,
	ptdp_spikequad=P_INVALID,
	pt_superspike=P_INVALID,
	ptdp_superspikequad=P_INVALID,
	pt_wizspike=P_INVALID,
	pt_knightspike=P_INVALID,
	pt_explosion=P_INVALID,
	ptdp_explosionquad=P_INVALID,
	pt_tarexplosion=P_INVALID,
	pt_teleportsplash=P_INVALID,
	pt_lavasplash=P_INVALID,
	ptdp_smallflash=P_INVALID,
	ptdp_flamejet=P_INVALID,
	ptdp_flame=P_INVALID,
	ptdp_blood=P_INVALID,
	ptdp_spark=P_INVALID,
	ptdp_plasmaburn=P_INVALID,
	ptdp_tei_g3=P_INVALID,
	ptdp_tei_smoke=P_INVALID,
	ptdp_tei_bigexplosion=P_INVALID,
	ptdp_tei_plasmahit=P_INVALID,
	ptdp_stardust=P_INVALID,
	rt_rocket=P_INVALID,
	rt_grenade=P_INVALID,
	rt_blood=P_INVALID,
	rt_wizspike=P_INVALID,
	rt_slightblood=P_INVALID,
	rt_knightspike=P_INVALID,
	rt_vorespike=P_INVALID,
	rtdp_neharasmoke=P_INVALID,
	rtdp_nexuizplasma=P_INVALID,
	rtdp_glowtrail=P_INVALID,

	ptqw_blood=P_INVALID,
	ptqw_lightningblood=P_INVALID,

	ptq2_blood=P_INVALID,
	rtq2_railtrail=P_INVALID,
	rtq2_blastertrail=P_INVALID,
	ptq2_blasterparticles=P_INVALID,
	rtq2_bubbletrail=P_INVALID,
	rtq2_gib=P_INVALID,
	rtq2_rocket=P_INVALID,
	rtq2_grenade=P_INVALID,

	rtqw_railtrail=P_INVALID,
	rtfte_lightning1=P_INVALID,
	ptfte_lightning1_end=P_INVALID,
	rtfte_lightning2=P_INVALID,
	ptfte_lightning2_end=P_INVALID,
	rtfte_lightning3=P_INVALID,
	ptfte_lightning3_end=P_INVALID,
	ptfte_bullet=P_INVALID,
	ptfte_superbullet=P_INVALID;

#ifdef Q2CLIENT
typedef enum
{
	Q2TE_GUNSHOT,	//0
	Q2TE_BLOOD,
	Q2TE_BLASTER,
	Q2TE_RAILTRAIL,
	Q2TE_SHOTGUN,
	Q2TE_EXPLOSION1,
	Q2TE_EXPLOSION2,
	Q2TE_ROCKET_EXPLOSION,
	Q2TE_GRENADE_EXPLOSION,
	Q2TE_SPARKS,
	Q2TE_SPLASH,	//10
	Q2TE_BUBBLETRAIL,
	Q2TE_SCREEN_SPARKS,
	Q2TE_SHIELD_SPARKS,
	Q2TE_BULLET_SPARKS,
	Q2TE_LASER_SPARKS,
	Q2TE_PARASITE_ATTACK,
	Q2TE_ROCKET_EXPLOSION_WATER,
	Q2TE_GRENADE_EXPLOSION_WATER,
	Q2TE_MEDIC_CABLE_ATTACK,
	Q2TE_BFG_EXPLOSION,	//20
	Q2TE_BFG_BIGEXPLOSION,
	Q2TE_BOSSTPORT,			// used as '22' in a map, so DON'T RENUMBER!!!
	Q2TE_BFG_LASER,
	Q2TE_GRAPPLE_CABLE,
	Q2TE_WELDING_SPARKS,
	Q2TE_GREENBLOOD,
	Q2TE_BLUEHYPERBLASTER,
	Q2TE_PLASMA_EXPLOSION,
	Q2TE_TUNNEL_SPARKS,
//ROGUE
	Q2TE_BLASTER2,	//30
	Q2TE_RAILTRAIL2,
	Q2TE_FLAME,
	Q2TE_LIGHTNING,
	Q2TE_DEBUGTRAIL,
	Q2TE_PLAIN_EXPLOSION,
	Q2TE_FLASHLIGHT,
	Q2TE_FORCEWALL,
	Q2TE_HEATBEAM,
	Q2TE_MONSTER_HEATBEAM,
	Q2TE_STEAM,		//40
	Q2TE_BUBBLETRAIL2,
	Q2TE_MOREBLOOD,
	Q2TE_HEATBEAM_SPARKS,
	Q2TE_HEATBEAM_STEAM,
	Q2TE_CHAINFIST_SMOKE,
	Q2TE_ELECTRIC_SPARKS,
	Q2TE_TRACKER_EXPLOSION,
	Q2TE_TELEPORT_EFFECT,
	Q2TE_DBALL_GOAL,
	Q2TE_WIDOWBEAMOUT,	//50
	Q2TE_NUKEBLAST,
	Q2TE_WIDOWSPLASH,
	Q2TE_EXPLOSION1_BIG,
	Q2TE_EXPLOSION1_NP,
	Q2TE_FLECHETTE,
//ROGUE


//CODERED
	CRTE_LEADERBLASTER,	//56
	CRTE_BLASTER_MUZZLEFLASH,
	CRTE_BLUE_MUZZLEFLASH,
	CRTE_SMART_MUZZLEFLASH,
	CRTE_LEADERFIELD,	//60
	CRTE_DEATHFIELD,
	CRTE_BLASTERBEAM,
	CRTE_STAIN,
	CRTE_FIRE,
	CRTE_CABLEGUT,
	CRTE_SMOKE
//CODERED
} temp_event_t;

#define Q2SPLASH_UNKNOWN		0
#define Q2SPLASH_SPARKS		1
#define Q2SPLASH_BLUE_WATER	2
#define Q2SPLASH_BROWN_WATER	3
#define Q2SPLASH_SLIME		4
#define	Q2SPLASH_LAVA			5
#define Q2SPLASH_BLOOD		6
#endif

#define	MAX_BEAMS	64
typedef struct
{
	int		entity;
	short	tag;
	qbyte	active;
	qbyte	flags;
	qbyte	type;
	qbyte	skin;
	struct model_s	*model;
	float	endtime;
	float	alpha;
	vec3_t	start, end;
	int		particleeffect;
	trailstate_t *trailstate;
	trailstate_t *emitstate;
} beam_t;

beam_t		cl_beams[MAX_BEAMS];

#define	MAX_EXPLOSIONS	256
typedef struct
{
	vec3_t	origin;
	vec3_t	oldorigin;
	vec3_t	velocity;

	int firstframe;
	int numframes;

	int		type;
	vec3_t	angles;
	int		flags;
	float	alpha;
	float	start;
	float	framerate;
	model_t	*model;
	int skinnum;
} explosion_t;

explosion_t	cl_explosions[MAX_EXPLOSIONS];

static int explosions_running;
static int beams_running;

sfx_t			*cl_sfx_wizhit;
sfx_t			*cl_sfx_knighthit;
sfx_t			*cl_sfx_tink1;
sfx_t			*cl_sfx_ric1;
sfx_t			*cl_sfx_ric2;
sfx_t			*cl_sfx_ric3;
sfx_t			*cl_sfx_r_exp3;

cvar_t	cl_expsprite = CVAR("cl_expsprite", "0");
cvar_t  r_explosionlight = CVARC("r_explosionlight", "1", Cvar_Limiter_ZeroToOne_Callback);
cvar_t	cl_truelightning = CVARF("cl_truelightning", "0",	CVAR_SEMICHEAT);
cvar_t  cl_beam_trace = CVAR("cl_beam_trace", "0");

typedef struct {
	sfx_t **sfx;
	char *efname;
} tentsfx_t;
tentsfx_t tentsfx[] =
{
	{&cl_sfx_wizhit, "wizard/hit.wav"},
	{&cl_sfx_knighthit, "hknight/hit.wav"},
	{&cl_sfx_tink1, "weapons/tink1.wav"},
	{&cl_sfx_ric1, "weapons/ric1.wav"},
	{&cl_sfx_ric2, "weapons/ric2.wav"},
	{&cl_sfx_ric3, "weapons/ric3.wav"},
	{&cl_sfx_r_exp3, "weapons/r_exp3.wav"}
};

vec3_t playerbeam_end[MAX_SPLITS];

struct associatedeffect
{
	struct associatedeffect *next;
	char mname[MAX_QPATH];
	char pname[MAX_QPATH];
	enum
	{
		AE_TRAIL,
		AE_EMIT,
		AE_REPLACE
	} type;
} *associatedeffect;
void CL_AssociateEffect_f(void)
{
	char *modelname = Cmd_Argv(1);
	char *effectname = Cmd_Argv(2);
	int type = atoi(Cmd_Argv(3));
	struct associatedeffect *ae;
	if (!strcmp(Cmd_Argv(0), "r_trail"))
		type = AE_TRAIL;
	else
	{
		if (type)
			type = AE_REPLACE;
		else
			type = AE_EMIT;
	}

	if (
		strstr(modelname, "player") ||
		strstr(modelname, "eyes") ||
		strstr(modelname, "flag") ||
		strstr(modelname, "tf_stan") ||
		strstr(modelname, ".bsp") ||
		strstr(modelname, "turr"))
	{
		Con_Printf("Sorry: Not allowed to attach effects to model \"%s\"\n", modelname);
		return;
	}

	if (strlen (modelname) >= MAX_QPATH || strlen(effectname) >= MAX_QPATH)
		return;

	/*replace the old one if it exists*/
	for(ae = associatedeffect; ae; ae = ae->next)
	{
		if (!strcmp(ae->mname, modelname))
			if ((ae->type==AE_TRAIL) == (type==AE_TRAIL))
			{
				strcpy(ae->pname, effectname);
				break;
			}
	}
	if (!ae)
	{
		ae = Z_Malloc(sizeof(*ae));
		ae->type = type;
		strcpy(ae->mname, modelname);
		strcpy(ae->pname, effectname);
		ae->next = associatedeffect;
		associatedeffect = ae;
	}

	//FIXME: overkill
	CL_RegisterParticles();
}

/*
=================
CL_ParseTEnts
=================
*/
void CL_InitTEnts (void)
{
	int i;
	for (i = 0; i < sizeof(tentsfx)/sizeof(tentsfx[0]); i++)
	{
		if (COM_FCheckExists(va("sound/%s", tentsfx[i].efname)))
			*tentsfx[i].sfx = S_PrecacheSound (tentsfx[i].efname);
		else
			*tentsfx[i].sfx = NULL;
	}

	Cmd_AddCommand("r_effect", CL_AssociateEffect_f);
	Cmd_AddCommand("r_trail", CL_AssociateEffect_f);

	Cvar_Register (&cl_expsprite, "Temporary entity control");
	Cvar_Register (&cl_truelightning, "Temporary entity control");
	Cvar_Register (&cl_beam_trace, "Temporary entity control");
	Cvar_Register (&r_explosionlight, "Temporary entity control");
}

void CL_ShutdownTEnts (void)
{
	struct associatedeffect *ae;
	while(associatedeffect)
	{
		ae = associatedeffect;
		associatedeffect = ae->next;
		BZ_Free(ae);
	}
}

void CL_ClearTEntParticleState (void)
{
	int i;
	for (i = 0; i < beams_running; i++)
	{
		pe->DelinkTrailstate(&(cl_beams[i].trailstate));
		pe->DelinkTrailstate(&(cl_beams[i].emitstate));
	}
}

void P_LoadedModel(model_t *mod)
{
	struct associatedeffect *ae;

	mod->particleeffect = P_INVALID;
	mod->particletrail = P_INVALID;
	mod->engineflags &= ~(MDLF_NODEFAULTTRAIL | MDLF_ENGULPHS);
	for(ae = associatedeffect; ae; ae = ae->next)
	{
		if (!strcmp(ae->mname, mod->name))
		{
			switch(ae->type)
			{
			case AE_TRAIL:
				mod->particletrail = P_FindParticleType(ae->pname);
				break;
			case AE_EMIT:
				mod->particleeffect = P_FindParticleType(ae->pname);
				mod->engineflags &= ~MDLF_ENGULPHS;
				break;
			case AE_REPLACE:
				mod->particleeffect = P_FindParticleType(ae->pname);
				mod->engineflags |= MDLF_ENGULPHS;
				break;
			}
		}
	}
	if (mod->particletrail == P_INVALID)
		P_DefaultTrail(mod);
}

void CL_RegisterParticles(void)
{
	model_t *mod;
	extern model_t	mod_known[];
	extern int		mod_numknown;
	int i;
	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		if (!mod->needload)
		{
			P_LoadedModel(mod);
		}
	}

	pt_gunshot				= P_FindParticleType("TE_GUNSHOT");	/*shotgun*/
	ptdp_gunshotquad		= P_FindParticleType("TE_GUNSHOTQUAD");	/*DP: quadded shotgun*/
	pt_spike				= P_FindParticleType("TE_SPIKE");	/*nailgun*/
	ptdp_spikequad			= P_FindParticleType("TE_SPIKEQUAD");	/*DP: quadded nailgun*/
	pt_superspike			= P_FindParticleType("TE_SUPERSPIKE");	/*nailgun*/
	ptdp_superspikequad		= P_FindParticleType("TE_SUPERSPIKEQUAD");	/*DP: quadded nailgun*/
	pt_wizspike				= P_FindParticleType("TE_WIZSPIKE");	//scrag missile impact
	pt_knightspike			= P_FindParticleType("TE_KNIGHTSPIKE"); //hellknight missile impact
	pt_explosion			= P_FindParticleType("TE_EXPLOSION");/*rocket/grenade launcher impacts/far too many things*/
	ptdp_explosionquad		= P_FindParticleType("TE_EXPLOSIONQUAD");	/*nailgun*/
	pt_tarexplosion			= P_FindParticleType("TE_TAREXPLOSION");//tarbaby/spawn dying.
	pt_teleportsplash		= P_FindParticleType("TE_TELEPORT");/*teleporters*/
	pt_lavasplash			= P_FindParticleType("TE_LAVASPLASH");	//e1m7 boss dying.
	ptdp_smallflash			= P_FindParticleType("TE_SMALLFLASH");	//DP:
	ptdp_flamejet			= P_FindParticleType("TE_FLAMEJET");	//DP:
	ptdp_flame				= P_FindParticleType("EF_FLAME");	//DP:
	ptdp_blood				= P_FindParticleType("TE_BLOOD"); /*when you hit something with the shotgun/axe/nailgun - nq uses the general particle builtin*/
	ptdp_spark				= P_FindParticleType("TE_SPARK");//DPTE_SPARK
	ptdp_plasmaburn			= P_FindParticleType("TE_PLASMABURN");
	ptdp_tei_g3				= P_FindParticleType("TE_TEI_G3");
	ptdp_tei_smoke			= P_FindParticleType("TE_TEI_SMOKE");
	ptdp_tei_bigexplosion	= P_FindParticleType("TE_TEI_BIGEXPLOSION");
	ptdp_tei_plasmahit		= P_FindParticleType("TE_TEI_PLASMAHIT");
	ptdp_stardust			= P_FindParticleType("EF_STARDUST");
	rt_rocket				= P_FindParticleType("TR_ROCKET");	/*rocket trail*/
	rt_grenade				= P_FindParticleType("TR_GRENADE");	/*grenade trail*/
	rt_blood				= P_FindParticleType("TR_BLOOD");	/*blood trail*/
	rt_wizspike				= P_FindParticleType("TR_WIZSPIKE");
	rt_slightblood			= P_FindParticleType("TR_SLIGHTBLOOD");
	rt_knightspike			= P_FindParticleType("TR_KNIGHTSPIKE");
	rt_vorespike			= P_FindParticleType("TR_VORESPIKE");
	rtdp_neharasmoke		= P_FindParticleType("TR_NEHAHRASMOKE");
	rtdp_nexuizplasma		= P_FindParticleType("TR_NEXUIZPLASMA");
	rtdp_glowtrail			= P_FindParticleType("TR_GLOWTRAIL");
	/*internal to psystem*/   P_FindParticleType("SVC_PARTICLE");

	ptqw_blood				= P_FindParticleType("TE_BLOOD");
	ptqw_lightningblood		= P_FindParticleType("TE_LIGHTNINGBLOOD");

	ptq2_blood				= P_FindParticleType("TE_BLOOD");
	rtq2_railtrail			= P_FindParticleType("TR_RAILTRAIL");
	rtq2_blastertrail		= P_FindParticleType("TR_BLASTERTRAIL");
	ptq2_blasterparticles	= P_FindParticleType("TE_BLASTERPARTICLES");
	rtq2_bubbletrail		= P_FindParticleType("TE_BUBBLETRAIL");
	rtq2_gib				= P_FindParticleType("TR_GIB");
	rtq2_rocket				= P_FindParticleType("TR_ROCKET");
	rtq2_grenade			= P_FindParticleType("TR_GRENADE");

	rtqw_railtrail			= P_FindParticleType("TE_RAILTRAIL");
	rtfte_lightning1		= P_FindParticleType("TE_LIGHTNING1");
	ptfte_lightning1_end	= P_FindParticleType("TE_LIGHTNING1_END");
	rtfte_lightning2		= P_FindParticleType("TE_LIGHTNING2");
	ptfte_lightning2_end	= P_FindParticleType("TE_LIGHTNING2_END");
	rtfte_lightning3		= P_FindParticleType("TE_LIGHTNING3");
	ptfte_lightning3_end	= P_FindParticleType("TE_LIGHTNING3_END");
	ptfte_bullet			= P_FindParticleType("TE_BULLET");
	ptfte_superbullet		= P_FindParticleType("TE_SUPERBULLET");
}

#ifdef Q2CLIENT
enum {
	q2cl_mod_explode,
	q2cl_mod_smoke,
	q2cl_mod_flash,
	q2cl_mod_parasite_segment,
	q2cl_mod_grapple_cable,
	q2cl_mod_parasite_tip,
	q2cl_mod_explo4,
	q2cl_mod_bfg_explo,
	q2cl_mod_powerscreen,
	q2cl_mod_max
};
typedef struct {
	char *modelname;

} tentmodels_t;
tentmodels_t q2tentmodels[q2cl_mod_max] = {
	{"models/objects/explode/tris.md2"},
	{"models/objects/smoke/tris.md2"},
	{"models/objects/flash/tris.md2"},
	{"models/monsters/parasite/segment/tris.md2"},
	{"models/ctf/segment/tris.md2"},
	{"models/monsters/parasite/tip/tris.md2"},
	{"models/objects/r_explode/tris.md2"},
	{"sprites/s_bfg2.sp2"},
	{"models/items/armor/effect/tris.md2"}
};

int CLQ2_RegisterTEntModels (void)
{
//	int i;
//	for (i = 0; i < q2cl_mod_max; i++)
//		if (!CL_CheckOrDownloadFile(q2tentmodels[i].modelname, false))
//			return false;

	return true;
}
#endif
/*
=================
CL_ClearTEnts
=================
*/
void CL_ClearTEnts (void)
{
	memset (&cl_beams, 0, sizeof(cl_beams));
	memset (&cl_explosions, 0, sizeof(cl_explosions));
}

/*
=================
CL_AllocExplosion
=================
*/
explosion_t *CL_AllocExplosion (void)
{
	int		i;
	float	time;
	int		index;

	for (i=0; i < explosions_running; i++)
	{
		if (!cl_explosions[i].model)
		{
			cl_explosions[i].firstframe = -1;
			cl_explosions[i].framerate = 10;
			return &cl_explosions[i];
		}
	}

	if (i == explosions_running && i != MAX_EXPLOSIONS)
	{
		explosions_running++;
		cl_explosions[i].firstframe = -1;
		cl_explosions[i].framerate = 10;
		return &cl_explosions[i];
	}

// find the oldest explosion
	time = cl.time;
	index = 0;

	for (i=0 ; i<MAX_EXPLOSIONS ; i++)
		if (cl_explosions[i].start < time)
		{
			time = cl_explosions[i].start;
			index = i;
		}
	cl_explosions[index].firstframe = -1;
	cl_explosions[index].framerate = 10;
	return &cl_explosions[index];
}

/*
=================
CL_ParseBeam
=================
*/
beam_t	*CL_NewBeam (int entity, int tag)
{
	beam_t	*b;
	int i;

// override any beam with the same entity (unless they used world)
	if (entity)
	{
		for (i=0, b=cl_beams; i < beams_running; i++, b++)
			if (b->entity == entity && b->tag == tag)
			{
				b->active = true;
				return b;
			}
	}

// find a free beam
	for (i=0, b=cl_beams; i < beams_running; i++, b++)
	{
		if (!b->active)
		{
			b->active = true;
			return b;
		}
	}

	if (i == beams_running && i != MAX_BEAMS)
	{
		beams_running++;
		cl_beams[i].active = true;
		return &cl_beams[i];
	}

	return NULL;
}
#define STREAM_ATTACHED			16
#define STREAM_TRANSLUCENT		32
void CL_AddBeam (int tent, int ent, vec3_t start, vec3_t end)	//fixme: use TE_ numbers instead of 0 - 5
{
	beam_t	*b;

	model_t *m;
	int btype, etype;
	int i;
	vec3_t impact, normal;
	vec3_t extra;
	char *mname;

	switch(tent)
	{
	case 0:
		if (ent < 0 && ent >= -512)	//a zquake concept. ent between -1 and -maxplayers is to be taken to be a railtrail from a particular player instead of a beam.
		{
			// TODO: add support for those finnicky colored railtrails...
			if (P_ParticleTrail(start, end, rtqw_railtrail, -ent, NULL))
				P_ParticleTrailIndex(start, end, 208, 8, NULL);
			return;
		}
	default:
		mname = "progs/bolt.mdl";
		btype = rtfte_lightning1;
		etype = ptfte_lightning1_end;
		break;
	case 1:
		if (ent < 0 && ent >= -MAX_CLIENTS)	//based on the railgun concept - this adds a rogue style TE_BEAM effect.
		{
	case 5:
			mname = "progs/beam.mdl";	//remember to precache!
			btype = P_FindParticleType("te_beam");
			etype = P_FindParticleType("te_beam_end");
		}
		else
		{
			mname = "progs/bolt2.mdl";
			btype = rtfte_lightning2;
			etype = ptfte_lightning2_end;
		}
		break;
	case 2:
		mname = "progs/bolt3.mdl";
		btype = rtfte_lightning3;
		etype = ptfte_lightning3_end;
		break;
#ifdef Q2CLIENT
	case 3:
		mname = q2tentmodels[q2cl_mod_parasite_segment].modelname;
		btype = P_FindParticleType("te_parasite_attack");
		etype = P_FindParticleType("te_parasite_attack_end");
		break;
	case 4:
		mname = q2tentmodels[q2cl_mod_grapple_cable].modelname;
		btype = P_FindParticleType("te_grapple_cable");
		etype = P_FindParticleType("te_grapple_cable_end");
		break;
#endif
	}

	/*don't bother loading the model if we have a particle effect for it instead*/
	if (ruleset_allow_particle_lightning.ival && btype >= 0)
		m = NULL;
	else
		m = Mod_ForName(mname, false);

	if (m && m->needload)
		CL_CheckOrEnqueDownloadFile(m->name, NULL, 0);

	// save end position for truelightning
	if (ent)
	{
		for (i = 0; i < cl.splitclients; i++)
		{
			if (ent == (autocam[i]?(spec_track[i]+1):(cl.playernum[i]+1)))
			{
				VectorCopy(end, playerbeam_end[i]);
				break;
			}
		}
	}

	if (etype >= 0 && cls.state == ca_active && etype != P_INVALID)
	{
		if (cl_beam_trace.ival)
		{
			VectorSubtract(end, start, normal);
			VectorNormalize(normal);
			VectorMA(end, 4, normal, extra);	//extend the end-point by four
			if (!TraceLineN(start, extra, impact, normal))
				etype = -1;
		}
		else
		{
			VectorCopy(end, impact);
			normal[0] = normal[1] = normal[2] = 0;
		}
	}

	b = CL_NewBeam(ent, -1);
	if (!b)
	{
		Con_Printf ("beam list overflow!\n");
		return;
	}

	b->entity = ent;
	b->model = m;
	b->particleeffect = btype;
	b->tag = -1;
	b->flags |= /*STREAM_ATTACHED|*/1;
	b->endtime = cl.time + 0.2;
	b->alpha = 1;
	VectorCopy (start, b->start);
	VectorCopy (end, b->end);

	if (etype >= 0)
	{
		P_RunParticleEffectState (impact, normal, 1, etype, &(b->emitstate));
		R_AddDecals(end);
		R_AddStain(end, -10, -10, -10, 20);
	}
}
void CL_ParseBeam (int tent)
{
	int		ent;
	vec3_t	start, end;

	ent = MSG_ReadShort ();

	start[0] = MSG_ReadCoord ();
	start[1] = MSG_ReadCoord ();
	start[2] = MSG_ReadCoord ();

	end[0] = MSG_ReadCoord ();
	end[1] = MSG_ReadCoord ();
	end[2] = MSG_ReadCoord ();

	CL_AddBeam(tent, ent, start, end);
}
void CL_ParseStream (int type)
{
	int		ent;
	vec3_t	start, end;
	beam_t	*b, *b2;
	int flags;
	int tag;
	float duration;
	int skin;

	ent = MSG_ReadShort();
	flags = MSG_ReadByte();
	tag = flags&15;
	flags-=tag;
	duration = (float)MSG_ReadByte()*0.05;
	skin = 0;
	if(type == TEH2_STREAM_COLORBEAM)
	{
		skin = MSG_ReadByte();
	}
	start[0] = MSG_ReadCoord();
	start[1] = MSG_ReadCoord();
	start[2] = MSG_ReadCoord();
	end[0] = MSG_ReadCoord();
	end[1] = MSG_ReadCoord();
	end[2] = MSG_ReadCoord();

	b = CL_NewBeam(ent, tag);
	if (!b)
	{
		Con_Printf ("beam list overflow!\n");
		return;
	}

	b->entity = ent;
	b->tag = tag;
	b->flags = flags;
	b->model = NULL;
	b->particleeffect = -1;
	b->endtime = cl.time + duration;
	b->alpha = 1;
	b->skin = skin;
	VectorCopy (start, b->start);
	VectorCopy (end, b->end);

	switch(type)
	{
	case TEH2_STREAM_LIGHTNING_SMALL:
		b->model = 	Mod_ForName("models/stltng2.mdl", true);
		b->flags |= 2;
		b->particleeffect = P_FindParticleType("te_stream_lightning_small");
		break;
	case TEH2_STREAM_LIGHTNING:
		b->model = 	Mod_ForName("models/stlghtng.mdl", true);
		b->flags |= 2;
		b->particleeffect = P_FindParticleType("te_stream_lightning");
		break;
	case TEH2_STREAM_ICECHUNKS:
		b->model = 	Mod_ForName("models/stice.mdl", true);
		b->flags |= 2;
		b->particleeffect = P_FindParticleType("te_stream_icechunks");
		R_AddStain(end, -10, -10, 0, 20);
		break;
	case TEH2_STREAM_SUNSTAFF1:
		b->model = Mod_ForName("models/stsunsf1.mdl", true);
		b->particleeffect = P_FindParticleType("te_stream_sunstaff1");
		if (b->particleeffect < 0)
		{
			b2 = CL_NewBeam(ent, tag+128);
			if (b2)
			{
				memcpy(b2, b, sizeof(*b2));
				b2->model = Mod_ForName("models/stsunsf2.mdl", true);
				b2->alpha = 0.5;
			}
		}
		break;
	case TEH2_STREAM_SUNSTAFF2:
		b->model = 	Mod_ForName("models/stsunsf1.mdl", true);
		b->particleeffect = P_FindParticleType("te_stream_sunstaff2");
		R_AddStain(end, -10, -10, -10, 20);
		break;
	case TEH2_STREAM_COLORBEAM:
		b->model = Mod_ForName("models/stclrbm.mdl", true);
		b->particleeffect = P_FindParticleType("te_stream_colorbeam");
		break;
	case TEH2_STREAM_GAZE:
		b->model = Mod_ForName("models/stmedgaz.mdl", true);
		b->particleeffect = P_FindParticleType("te_stream_gaze");
		break;
	default:
		Con_Printf("CL_ParseStream: type %i\n", type);
		break;
	}
}

/*
=================
CL_ParseTEnt
=================
*/

#ifdef NQPROT
void CL_ParseTEnt (qboolean nqprot)
#else
void CL_ParseTEnt (void)
#endif
{
#ifndef NQPROT
#define nqprot false	//it's easier
#endif
	int		type;
	vec3_t	pos, pos2;
	dlight_t	*dl;
	int		rnd;
//	explosion_t	*ex;
	int		cnt, colour;

	type = MSG_ReadByte ();

#ifdef CSQC_DAT
	if (type != TE_GUNSHOT)
	{
		//I know I'm going to regret this.
		if (CSQC_ParseTempEntity((unsigned char)type))
			return;
	}
#endif


	switch (type)
	{
	case TE_WIZSPIKE:			// spike hitting wall
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		R_AddStain(pos, -10, 0, -10, 20);

		if (P_RunParticleEffectType(pos, NULL, 1, pt_wizspike))
			P_RunParticleEffect (pos, vec3_origin, 20, 30);

		S_StartSound (-2, 0, cl_sfx_wizhit, pos, 1, 1, 0);
		break;

	case TE_KNIGHTSPIKE:			// spike hitting wall
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		R_AddStain(pos, -10, -10, -10, 20);

		if (P_RunParticleEffectType(pos, NULL, 1, pt_knightspike))
			P_RunParticleEffect (pos, vec3_origin, 226, 20);

		S_StartSound (-2, 0, cl_sfx_knighthit, pos, 1, 1, 0);
		break;

	case TEDP_SPIKEQUAD:
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		R_AddStain(pos, -10, -10, -10, 20);
		R_AddDecals(pos);

		if (P_RunParticleEffectType(pos, NULL, 1, ptdp_spikequad))
			if (P_RunParticleEffectType(pos, NULL, 1, pt_spike))
				if (P_RunParticleEffectType(pos, NULL, 10, pt_gunshot))
					P_RunParticleEffect (pos, vec3_origin, 0, 10);

		if ( rand() % 5 )
			S_StartSound (-2, 0, cl_sfx_tink1, pos, 1, 1, 0);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound (-2, 0, cl_sfx_ric1, pos, 1, 1, 0);
			else if (rnd == 2)
				S_StartSound (-2, 0, cl_sfx_ric2, pos, 1, 1, 0);
			else
				S_StartSound (-2, 0, cl_sfx_ric3, pos, 1, 1, 0);
		}
		break;
	case TE_SPIKE:			// spike hitting wall
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		R_AddStain(pos, -10, -10, -10, 20);
		R_AddDecals(pos);

		if (P_RunParticleEffectType(pos, NULL, 1, pt_spike))
			if (P_RunParticleEffectType(pos, NULL, 10, pt_gunshot))
				P_RunParticleEffect (pos, vec3_origin, 0, 10);

		if ( rand() % 5 )
			S_StartSound (-2, 0, cl_sfx_tink1, pos, 1, 1, 0);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound (-2, 0, cl_sfx_ric1, pos, 1, 1, 0);
			else if (rnd == 2)
				S_StartSound (-2, 0, cl_sfx_ric2, pos, 1, 1, 0);
			else
				S_StartSound (-2, 0, cl_sfx_ric3, pos, 1, 1, 0);
		}
		break;
	case TEDP_SUPERSPIKEQUAD:			// super spike hitting wall
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		R_AddStain(pos, -10, -10, -10, 20);
		R_AddDecals(pos);

		if (P_RunParticleEffectType(pos, NULL, 1, ptdp_superspikequad))
			if (P_RunParticleEffectType(pos, NULL, 1, pt_superspike))
				if (P_RunParticleEffectType(pos, NULL, 2, pt_spike))
					if (P_RunParticleEffectType(pos, NULL, 20, pt_gunshot))
						P_RunParticleEffect (pos, vec3_origin, 0, 20);

		if ( rand() % 5 )
			S_StartSound (-2, 0, cl_sfx_tink1, pos, 1, 1, 0);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound (-2, 0, cl_sfx_ric1, pos, 1, 1, 0);
			else if (rnd == 2)
				S_StartSound (-2, 0, cl_sfx_ric2, pos, 1, 1, 0);
			else
				S_StartSound (-2, 0, cl_sfx_ric3, pos, 1, 1, 0);
		}
		break;
	case TE_SUPERSPIKE:			// super spike hitting wall
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		R_AddStain(pos, -10, -10, -10, 20);
		R_AddDecals(pos);

		if (P_RunParticleEffectType(pos, NULL, 1, pt_superspike))
			if (P_RunParticleEffectType(pos, NULL, 2, pt_spike))
				if (P_RunParticleEffectType(pos, NULL, 20, pt_gunshot))
					P_RunParticleEffect (pos, vec3_origin, 0, 20);

		if ( rand() % 5 )
			S_StartSound (-2, 0, cl_sfx_tink1, pos, 1, 1, 0);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound (-2, 0, cl_sfx_ric1, pos, 1, 1, 0);
			else if (rnd == 2)
				S_StartSound (-2, 0, cl_sfx_ric2, pos, 1, 1, 0);
			else
				S_StartSound (-2, 0, cl_sfx_ric3, pos, 1, 1, 0);
		}
		break;

#ifdef PEXT_TE_BULLET
	case TE_BULLET:
		if (!(cls.fteprotocolextensions & PEXT_TE_BULLET))
			Host_EndGame("Thought PEXT_TE_BULLET was disabled");
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		R_AddStain(pos, -10, -10, -10, 20);
		R_AddDecals(pos);

		if (P_RunParticleEffectType(pos, NULL, 1, ptfte_bullet))
			if (P_RunParticleEffectType(pos, NULL, 10, pt_gunshot))
				P_RunParticleEffect (pos, vec3_origin, 0, 10);

		if ( rand() % 5 )
			S_StartSound (-2, 0, cl_sfx_tink1, pos, 1, 1, 0);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound (-2, 0, cl_sfx_ric1, pos, 1, 1, 0);
			else if (rnd == 2)
				S_StartSound (-2, 0, cl_sfx_ric2, pos, 1, 1, 0);
			else
				S_StartSound (-2, 0, cl_sfx_ric3, pos, 1, 1, 0);
		}
		break;
	case TE_SUPERBULLET:
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		R_AddStain(pos, -10, -10, -10, 20);
		R_AddDecals(pos);

		if (P_RunParticleEffectType(pos, NULL, 1, ptfte_superbullet))
			if (P_RunParticleEffectType(pos, NULL, 2, ptfte_bullet))
				if (P_RunParticleEffectType(pos, NULL, 20, pt_gunshot))
					P_RunParticleEffect (pos, vec3_origin, 0, 20);

		if ( rand() % 5 )
			S_StartSound (-2, 0, cl_sfx_tink1, pos, 1, 1, 0);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound (-2, 0, cl_sfx_ric1, pos, 1, 1, 0);
			else if (rnd == 2)
				S_StartSound (-2, 0, cl_sfx_ric2, pos, 1, 1, 0);
			else
				S_StartSound (-2, 0, cl_sfx_ric3, pos, 1, 1, 0);
		}
		break;
#endif

	case TEDP_EXPLOSIONQUAD:			// rocket explosion
	// particles
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
		if (P_RunParticleEffectType(pos, NULL, 1, ptdp_explosionquad))
			if (P_RunParticleEffectType(pos, NULL, 1, pt_explosion))
				P_RunParticleEffect(pos, NULL, 107, 1024); // should be 97-111

		R_AddStain(pos, -1, -1, -1, 100);

	// light
		if (r_explosionlight.value)
		{
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


	// sound
		S_StartSound (-2, 0, cl_sfx_r_exp3, pos, 1, 1, 0);

	// sprite
		if (cl_expsprite.ival) // temp hopefully
		{
			explosion_t *ex = CL_AllocExplosion ();
			VectorCopy (pos, ex->origin);
			ex->start = cl.time;
			ex->model = Mod_ForName ("progs/s_explod.spr", true);
		}
		break;
	case TE_EXPLOSION:			// rocket explosion
	// particles
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
		if (P_RunParticleEffectType(pos, NULL, 1, pt_explosion))
			P_RunParticleEffect(pos, NULL, 107, 1024); // should be 97-111

		R_AddStain(pos, -1, -1, -1, 100);

	// light
		if (r_explosionlight.value)
		{
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


	// sound
		S_StartSound (-2, 0, cl_sfx_r_exp3, pos, 1, 1, 0);

	// sprite
		if (cl_expsprite.ival && !nqprot) // temp hopefully
		{
			explosion_t *ex = CL_AllocExplosion ();
			VectorCopy (pos, ex->origin);
			ex->start = cl.time;
			ex->model = Mod_ForName ("progs/s_explod.spr", true);
		}
		break;

	case TEDP_EXPLOSIONRGB:
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
		if (P_RunParticleEffectType(pos, NULL, 1, pt_explosion))
			P_RunParticleEffect(pos, NULL, 107, 1024); // should be 97-111

		R_AddStain(pos, -1, -1, -1, 100);


	// light
		if (r_explosionlight.value)
		{
			dl = CL_AllocDlight (0);
			VectorCopy (pos, dl->origin);
			dl->radius = 150 + r_explosionlight.value*200;
			dl->die = cl.time + 0.5;
			dl->decay = 300;

			dl->color[0] = 0.4f*MSG_ReadByte()/255.0f;
			dl->color[1] = 0.4f*MSG_ReadByte()/255.0f;
			dl->color[2] = 0.4f*MSG_ReadByte()/255.0f;
			dl->channelfade[0] = 0;
			dl->channelfade[1] = 0;
			dl->channelfade[2] = 0;
		}

		S_StartSound (-2, 0, cl_sfx_r_exp3, pos, 1, 1, 0);
		break;

	case TEDP_TEI_BIGEXPLOSION:
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
		if (P_RunParticleEffectType(pos, NULL, 1, ptdp_tei_bigexplosion))
			if (P_RunParticleEffectType(pos, NULL, 1, pt_explosion))
				P_RunParticleEffect(pos, NULL, 107, 1024); // should be 97-111

		R_AddStain(pos, -1, -1, -1, 100);

	// light
		if (r_explosionlight.value)
		{
			dl = CL_AllocDlight (0);
			VectorCopy (pos, dl->origin);
			// no point in doing this the fuh/ez way
			dl->radius = 500*r_explosionlight.value;
			dl->die = cl.time + 1;
			dl->decay = 500;

			dl->color[0] = 0.4f;
			dl->color[1] = 0.3f;
			dl->color[2] = 0.15f;
			dl->channelfade[0] = 0;
			dl->channelfade[1] = 0;
			dl->channelfade[2] = 0;
		}

		S_StartSound (-2, 0, cl_sfx_r_exp3, pos, 1, 1, 0);
		break;

	case TE_TAREXPLOSION:			// tarbaby explosion
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
		P_RunParticleEffectType(pos, NULL, 1, pt_tarexplosion);

		S_StartSound (-2, 0, cl_sfx_r_exp3, pos, 1, 1, 0);
		break;

	case TE_LIGHTNING1:				// lightning bolts
	case TE_LIGHTNING2:				// lightning bolts
		CL_ParseBeam (type - TE_LIGHTNING1);
		break;
	case TE_LIGHTNING3:				// lightning bolts
		CL_ParseBeam (2);
		break;

	case TE_LAVASPLASH:
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
		P_RunParticleEffectType(pos, NULL, 1, pt_lavasplash);
		break;

	case TE_TELEPORT:
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
		P_RunParticleEffectType(pos, NULL, 1, pt_teleportsplash);
		break;

	case TEDP_GUNSHOTQUAD:			// bullet hitting wall
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		R_AddStain(pos, -10, -10, -10, 20);

		if (P_RunParticleEffectType(pos, NULL, 1, ptdp_gunshotquad))
			if (P_RunParticleEffectType(pos, NULL, 1, pt_gunshot))
				P_RunParticleEffect (pos, vec3_origin, 0, 20);

		break;
	case TE_GUNSHOT:			// bullet hitting wall
		if (nqprot)
			cnt = 1;
		else
			cnt = MSG_ReadByte ();
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		R_AddStain(pos, -10, -10, -10, 20);

		if (P_RunParticleEffectType(pos, NULL, cnt, pt_gunshot))
			P_RunParticleEffect (pos, vec3_origin, 0, 20*cnt);

		break;

	case TEQW_BLOOD:				// bullets hitting body
		cnt = MSG_ReadByte ();
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		R_AddStain(pos, 0, -10, -10, 40);

		if (P_RunParticleEffectType(pos, NULL, cnt, ptqw_blood))
			if (P_RunParticleEffectType(pos, NULL, cnt, ptdp_blood))
				P_RunParticleEffect (pos, vec3_origin, 73, 20*cnt);

		break;

	case TEQW_LIGHTNINGBLOOD:		// lightning hitting body
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		R_AddStain(pos, 1, -10, -10, 20);

		if (P_RunParticleEffectType(pos, NULL, 1, ptqw_lightningblood))
			P_RunParticleEffect (pos, vec3_origin, 225, 50);

		break;

	case TE_RAILTRAIL:
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		pos2[0] = MSG_ReadCoord ();
		pos2[1] = MSG_ReadCoord ();
		pos2[2] = MSG_ReadCoord ();

		if (P_ParticleTrail(pos, pos2, rtqw_railtrail, 0, NULL))
			if (P_ParticleTrail(pos, pos2, rtq2_railtrail, 0, NULL))
				P_ParticleTrailIndex(pos, pos2, 208, 8, NULL);
		break;

	case TEH2_STREAM_LIGHTNING_SMALL:
	case TEH2_STREAM_CHAIN:
	case TEH2_STREAM_SUNSTAFF1:
	case TEH2_STREAM_SUNSTAFF2:
	case TEH2_STREAM_LIGHTNING:
	case TEH2_STREAM_COLORBEAM:
	case TEH2_STREAM_ICECHUNKS:
	case TEH2_STREAM_GAZE:
	case TEH2_STREAM_FAMINE:
		CL_ParseStream (type);
		break;

	case TEDP_BLOOD:
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		pos2[0] = MSG_ReadChar ();
		pos2[1] = MSG_ReadChar ();
		pos2[2] = MSG_ReadChar ();

		cnt = MSG_ReadByte ();

		P_RunParticleEffectType(pos, pos2, cnt, ptdp_blood);
		break;

	case TEDP_SPARK:
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		pos2[0] = MSG_ReadChar ();
		pos2[1] = MSG_ReadChar ();
		pos2[2] = MSG_ReadChar ();

		cnt = MSG_ReadByte ();
		{
			P_RunParticleEffectType(pos, pos2, cnt, ptdp_spark);
		}
		break;

	case TEDP_BLOODSHOWER:
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		pos2[0] = MSG_ReadCoord ();
		pos2[1] = MSG_ReadCoord ();
		pos2[2] = MSG_ReadCoord ();

		cnt = MSG_ReadCoord ();	//speed

		cnt = MSG_ReadShort ();

		{
			VectorAdd(pos, pos2, pos);
			VectorScale(pos, 0.5, pos);
			P_RunParticleEffectTypeString(pos, NULL, cnt, "te_bloodshower");
		}
		break;

	case TEDP_SMALLFLASH:
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		// light
		dl = CL_AllocDlight (0);
		VectorCopy (pos, dl->origin);
		dl->radius = 200;
		dl->decay = 1000;
		dl->die = cl.time + 0.2;
		dl->color[0] = 0.4;
		dl->color[1] = 0.4;
		dl->color[2] = 0.4;
		break;

	case TEDP_CUSTOMFLASH:
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

			// light
		dl = CL_AllocDlight (0);
		VectorCopy (pos, dl->origin);
		dl->radius = MSG_ReadByte()*8;
		pos2[0] = (MSG_ReadByte() + 1) * (1.0 / 256.0);
		dl->die = cl.time + pos2[0];
		dl->decay = dl->radius / pos2[0];

		// DP's range is 0-2 for lights, FTE is 0-0.4.. 255/637.5 = 0.4
		dl->color[0] = MSG_ReadByte()*(1.0f/637.5f);
		dl->color[1] = MSG_ReadByte()*(1.0f/637.5f);
		dl->color[2] = MSG_ReadByte()*(1.0f/637.5f);

		break;

	case TEDP_FLAMEJET:
		// origin
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		// velocity
		pos2[0] = MSG_ReadCoord ();
		pos2[1] = MSG_ReadCoord ();
		pos2[2] = MSG_ReadCoord ();

		// count
		cnt = MSG_ReadByte ();

		if (P_RunParticleEffectType(pos, pos2, cnt, ptdp_flamejet))
			P_RunParticleEffect (pos, pos2, 232, cnt);
		break;

	case TEDP_PLASMABURN:
		// origin
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		// light
		dl = CL_AllocDlight (0);
		VectorCopy (pos, dl->origin);
		dl->radius = 200;
		dl->decay = 1000;
		dl->die = cl.time + 0.2;
		dl->color[0] = 0.2;
		dl->color[1] = 0.2;
		dl->color[2] = 0.2;

		// stain (Hopefully this is close to how DP does it)
		R_AddStain(pos, -10, -10, -10, 30);

		if (P_ParticleTrail(pos, pos2, P_FindParticleType("te_plasmaburn"), 0, NULL))
			P_ParticleTrailIndex(pos, pos2, 15, 0, NULL);
		break;

	case TEDP_TEI_G3:	//nexuiz's nex beam
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		pos2[0] = MSG_ReadCoord ();
		pos2[1] = MSG_ReadCoord ();
		pos2[2] = MSG_ReadCoord ();

		//sigh...
		MSG_ReadCoord ();
		MSG_ReadCoord ();
		MSG_ReadCoord ();

		if (P_ParticleTrail(pos, pos2, P_FindParticleType("te_nexbeam"), 0, NULL))
			P_ParticleTrailIndex(pos, pos2, 15, 0, NULL);
		break;

	case TEDP_SMOKE:
		//org
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		//dir
		pos2[0] = MSG_ReadCoord ();
		pos2[1] = MSG_ReadCoord ();
		pos2[2] = MSG_ReadCoord ();

		//count
		cnt = MSG_ReadByte ();
		{
			P_RunParticleEffectType(pos, pos2, cnt, ptdp_tei_smoke);
		}
		break;

	case TEDP_TEI_PLASMAHIT:
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		//dir
		pos2[0] = MSG_ReadCoord ();
		pos2[1] = MSG_ReadCoord ();
		pos2[2] = MSG_ReadCoord ();
		cnt = MSG_ReadByte ();

		{
			P_RunParticleEffectType(pos, pos2, cnt, ptdp_tei_plasmahit);
		}
		break;

	case TEDP_PARTICLECUBE:
		{
			vec3_t dir;
			int jitter;
			int gravity;

			//min
			pos[0] = MSG_ReadCoord();
			pos[1] = MSG_ReadCoord();
			pos[2] = MSG_ReadCoord();

			//max
			pos2[0] = MSG_ReadCoord();
			pos2[1] = MSG_ReadCoord();
			pos2[2] = MSG_ReadCoord();

			//dir
			dir[0] = MSG_ReadCoord();
			dir[1] = MSG_ReadCoord();
			dir[2] = MSG_ReadCoord();

			cnt = MSG_ReadShort();	//count
			colour = MSG_ReadByte ();	//colour
			gravity = MSG_ReadByte ();	//gravity flag
			jitter = MSG_ReadCoord();	//jitter

			P_RunParticleCube(pos, pos2, dir, cnt, colour, gravity, jitter);
		}
		break;
	case TEDP_PARTICLERAIN:
		{
			vec3_t dir;

			//min
			pos[0] = MSG_ReadCoord();
			pos[1] = MSG_ReadCoord();
			pos[2] = MSG_ReadCoord();

			//max
			pos2[0] = MSG_ReadCoord();
			pos2[1] = MSG_ReadCoord();
			pos2[2] = MSG_ReadCoord();

			//dir
			dir[0] = MSG_ReadCoord();
			dir[1] = MSG_ReadCoord();
			dir[2] = MSG_ReadCoord();

			cnt = MSG_ReadShort();	//count
			colour = MSG_ReadByte ();	//colour

			P_RunParticleWeather(pos, pos2, dir, cnt, colour, "rain");
		}
		break;
	case TEDP_PARTICLESNOW:
		{
			vec3_t dir;

			//min
			pos[0] = MSG_ReadCoord();
			pos[1] = MSG_ReadCoord();
			pos[2] = MSG_ReadCoord();

			//max
			pos2[0] = MSG_ReadCoord();
			pos2[1] = MSG_ReadCoord();
			pos2[2] = MSG_ReadCoord();

			//dir
			dir[0] = MSG_ReadCoord();
			dir[1] = MSG_ReadCoord();
			dir[2] = MSG_ReadCoord();

			cnt = MSG_ReadShort();	//count
			colour = MSG_ReadByte ();	//colour

			P_RunParticleWeather(pos, pos2, dir, cnt, colour, "snow");
		}
		break;

	default:
		Host_EndGame ("CL_ParseTEnt: bad type - %i", type);
	}
}

void MSG_ReadPos (vec3_t pos);
void MSG_ReadDir (vec3_t dir);
typedef struct {
	char name[64];
	int netstyle;
	int particleeffecttype;
	char stain[3];
	qbyte radius;
	vec3_t dlightrgb;
	float dlightradius;
	float dlighttime;
	vec3_t dlightcfade;
} clcustomtents_t;

clcustomtents_t customtenttype[255];	//network based.
void CL_ParseCustomTEnt(void)
{
	int count;
	vec3_t pos;
	vec3_t pos2;
	vec3_t dir;
	char *str;
	clcustomtents_t *t;
	int type = MSG_ReadByte();
	qboolean failed;

	if (type == 255)	//255 is register
	{
		type = MSG_ReadByte();
		if (type == 255)
			Host_EndGame("Custom temp type 255 isn't valid\n");
		t = &customtenttype[type];

		t->netstyle = MSG_ReadByte();
		str = MSG_ReadString();
		Q_strncpyz(t->name, str, sizeof(t->name));
		t->particleeffecttype = P_FindParticleType(str);

		if (t->netstyle & CTE_STAINS)
		{
			t->stain[0] = MSG_ReadChar();
			t->stain[1] = MSG_ReadChar();
			t->stain[2] = MSG_ReadChar();
			t->radius = MSG_ReadByte();
		}
		else
			t->radius = 0;
		if (t->netstyle & CTE_GLOWS)
		{
			t->dlightrgb[0] = MSG_ReadByte()/255.0f;
			t->dlightrgb[1] = MSG_ReadByte()/255.0f;
			t->dlightrgb[2] = MSG_ReadByte()/255.0f;
			t->dlightradius = MSG_ReadByte();
			t->dlighttime = MSG_ReadByte()/16.0f;
			if (t->netstyle & CTE_CHANNELFADE)
			{
				t->dlightcfade[0] = MSG_ReadByte()/64.0f;
				t->dlightcfade[1] = MSG_ReadByte()/64.0f;
				t->dlightcfade[2] = MSG_ReadByte()/64.0f;
			}
		}
		else
			t->dlighttime = 0;
		return;
	}

	t = &customtenttype[type];

	if (t->netstyle & CTE_ISBEAM)
	{
		MSG_ReadPos (pos);
		MSG_ReadPos (pos2);
		failed = P_ParticleTrail(pos, pos2, t->particleeffecttype, 0, NULL);
	}
	else
	{
		if (t->netstyle & CTE_CUSTOMCOUNT)
			count = MSG_ReadByte();
		else
			count = 1;

		MSG_ReadPos (pos);
		VectorCopy(pos, pos2);

		if (t->netstyle & CTE_CUSTOMVELOCITY)
		{
			dir[0] = MSG_ReadCoord();
			dir[1] = MSG_ReadCoord();
			dir[2] = MSG_ReadCoord();
			failed = P_RunParticleEffectType(pos, dir, count, t->particleeffecttype);
		}
		else if (t->netstyle & CTE_CUSTOMDIRECTION)
		{
			MSG_ReadDir (dir);
			failed = P_RunParticleEffectType(pos, dir, count, t->particleeffecttype);
		}
		else failed = P_RunParticleEffectType(pos, NULL, count, t->particleeffecttype);
	}

	if (failed)
		Con_DPrintf("Failed to create effect %s\n", t->name);

	if (t->netstyle & CTE_STAINS)
	{	//added at pos2 - end of trail
		R_AddStain(pos2, t->stain[0], t->stain[1], t->stain[2], 40);
	}
	if (t->netstyle & CTE_GLOWS)
	{	//added at pos1 firer's end.
		dlight_t	*dl;
		dl = CL_AllocDlight (0);
		VectorCopy (pos, dl->origin);
		dl->radius = t->dlightradius*4;
		dl->die = cl.time + t->dlighttime;
		dl->decay = t->radius/t->dlighttime;

		dl->color[0] = t->dlightrgb[0];
		dl->color[1] = t->dlightrgb[1];
		dl->color[2] = t->dlightrgb[2];

		if (t->netstyle & CTE_CHANNELFADE)
		{
			dl->channelfade[0] = t->dlightcfade[0];
			dl->channelfade[1] = t->dlightcfade[1];
			dl->channelfade[2] = t->dlightcfade[2];
		}

		/*
		if (dl->color[0] < 0)
			dl->channelfade[0] = 0;
		else
			dl->channelfade[0] = dl->color[0]/t->dlighttime;

		if (dl->color[1] < 0)
			dl->channelfade[1] = 0;
		else
			dl->channelfade[1] = dl->color[0]/t->dlighttime;

		if (dl->color[2] < 0)
			dl->channelfade[2] = 0;
		else
			dl->channelfade[2] = dl->color[0]/t->dlighttime;
		*/
	}
}
void CL_ClearCustomTEnts(void)
{
	int i;
	for (i = 0; i < sizeof(customtenttype)/sizeof(customtenttype[0]); i++)
		customtenttype[i].particleeffecttype = -1;
}

void CLDP_ParseTrailParticles(void)
{
	int entityindex;
	int effectindex;
	vec3_t start, end;
	trailstate_t **ts;

	entityindex = (unsigned short)MSG_ReadShort();
	effectindex = (unsigned short)MSG_ReadShort();

	start[0] = MSG_ReadCoord();
	start[1] = MSG_ReadCoord();
	start[2] = MSG_ReadCoord();
	end[0] = MSG_ReadCoord();
	end[1] = MSG_ReadCoord();
	end[2] = MSG_ReadCoord();

	if (entityindex && (unsigned int)entityindex < MAX_EDICTS)
		ts = &cl.lerpents[entityindex].trailstate;
	else
		ts = NULL;

	effectindex = P_FindParticleType(COM_Effectinfo_ForNumber(effectindex));
	if (P_ParticleTrail(start, end, effectindex, entityindex, ts))
		P_ParticleTrail(start, end, rt_blood, entityindex, ts);
}

void CLDP_ParsePointParticles(qboolean compact)
{
	vec3_t		org, dir;
	unsigned int count, effectindex;

	effectindex = (unsigned short)MSG_ReadShort();
	org[0] = MSG_ReadCoord();
	org[1] = MSG_ReadCoord();
	org[2] = MSG_ReadCoord();
	if (compact)
	{
		dir[0] = dir[1] = dir[2] = 0;
		count = 1;
	}
	else
	{
		dir[0] = MSG_ReadCoord();
		dir[1] = MSG_ReadCoord();
		dir[2] = MSG_ReadCoord();
		count = (unsigned short)MSG_ReadShort();
	}

	effectindex = P_FindParticleType(COM_Effectinfo_ForNumber(effectindex));
	if (P_RunParticleEffectType(org, dir, count, effectindex))
		P_RunParticleEffect (org, dir, 15, 15);
}

void CLNQ_ParseParticleEffect (void)
{
	vec3_t		org, dir;
	int			i, msgcount, color;

	for (i=0 ; i<3 ; i++)
		org[i] = MSG_ReadCoord ();
	for (i=0 ; i<3 ; i++)
		dir[i] = MSG_ReadChar () * (1.0/16);
	msgcount = MSG_ReadByte ();
	color = MSG_ReadByte ();

	if (msgcount == 255)
	{
		// treat as spriteless explosion (qtest/some mods require this)
		if (P_RunParticleEffectType(org, NULL, 1, pt_explosion))
			P_RunParticleEffect(org, NULL, 107, 1024); // should be 97-111
	}
	else
		P_RunParticleEffect (org, dir, color, msgcount);
}
void CL_ParseParticleEffect2 (void)
{
	vec3_t		org, dmin, dmax;
	int			i, msgcount, color, effect;

	for (i=0 ; i<3 ; i++)
		org[i] = MSG_ReadCoord ();
	for (i=0 ; i<3 ; i++)
		dmin[i] = MSG_ReadFloat ();
	for (i=0 ; i<3 ; i++)
		dmax[i] = MSG_ReadFloat ();
	color = MSG_ReadShort ();
	msgcount = MSG_ReadByte ();
	effect = MSG_ReadByte ();

	P_RunParticleEffect2 (org, dmin, dmax, color, effect, msgcount);
}
void CL_ParseParticleEffect3 (void)
{
	vec3_t		org, box;
	int			i, msgcount, color, effect;

	for (i=0 ; i<3 ; i++)
		org[i] = MSG_ReadCoord ();
	for (i=0 ; i<3 ; i++)
		box[i] = MSG_ReadByte ();
	color = MSG_ReadShort ();
	msgcount = MSG_ReadByte ();
	effect = MSG_ReadByte ();

	P_RunParticleEffect3 (org, box, color, effect, msgcount);
}
void CL_ParseParticleEffect4 (void)
{
	vec3_t		org;
	int			i, msgcount, color, effect;
	float		radius;

	for (i=0 ; i<3 ; i++)
		org[i] = MSG_ReadCoord ();
	radius = MSG_ReadByte();
	color = MSG_ReadShort ();
	msgcount = MSG_ReadByte ();
	effect = MSG_ReadByte ();

	P_RunParticleEffect4 (org, radius, color, effect, msgcount);
}

void CL_SpawnSpriteEffect(vec3_t org, vec3_t dir, model_t *model, int startframe, int framecount, int framerate, float alpha)
{
	explosion_t	*ex;

	ex = CL_AllocExplosion ();
	VectorCopy (org, ex->origin);
	ex->start = cl.time;
	ex->model = model;
	ex->firstframe = startframe;
	ex->numframes = framecount;
	ex->framerate = framerate;
	ex->alpha = alpha;

	ex->angles[0] = 0;
	ex->angles[1] = 0;
	ex->angles[2] = 0;

	if (dir)
		VectorCopy(dir, ex->velocity);
	else
		VectorClear(ex->velocity);
}

// [vector] org [byte] modelindex [byte] startframe [byte] framecount [byte] framerate
// [vector] org [short] modelindex [short] startframe [byte] framecount [byte] framerate
void CL_ParseEffect (qboolean effect2)
{
	vec3_t org;
	int modelindex;
	int startframe;
	int framecount;
	int framerate;

	org[0] = MSG_ReadCoord();
	org[1] = MSG_ReadCoord();
	org[2] = MSG_ReadCoord();

	if (effect2)
		modelindex = MSG_ReadShort();
	else
		modelindex = MSG_ReadByte();

	if (effect2)
		startframe = MSG_ReadShort();
	else
		startframe = MSG_ReadByte();

	framecount = MSG_ReadByte();
	framerate = MSG_ReadByte();


	CL_SpawnSpriteEffect(org, vec3_origin, cl.model_precache[modelindex], startframe, framecount, framerate, 1);
}

#ifdef Q2CLIENT
void CL_SmokeAndFlash(vec3_t origin)
{
	explosion_t	*ex;

	ex = CL_AllocExplosion ();
	VectorCopy (origin, ex->origin);
	VectorClear(ex->angles);
//	ex->type = ex_misc;
	ex->numframes = 4;
	ex->flags = Q2RF_TRANSLUCENT;
	ex->alpha = 1;
	ex->start = cl.time;
	ex->model = Mod_ForName (q2tentmodels[q2cl_mod_smoke].modelname, false);

	ex = CL_AllocExplosion ();
	VectorCopy (origin, ex->origin);
	VectorClear(ex->angles);
//	ex->type = ex_flash;
	ex->flags = Q2RF_FULLBRIGHT;
	ex->numframes = 2;
	ex->start = cl.time;
	ex->model = Mod_ForName (q2tentmodels[q2cl_mod_flash].modelname, false);
}

void CL_Laser (vec3_t start, vec3_t end, int colors)
{
	explosion_t	*ex = CL_AllocExplosion();
	ex->firstframe = 0;
	ex->numframes = 10;
	ex->alpha = 0.33f;
	ex->model = NULL;
	ex->skinnum = (colors >> ((rand() % 4)*8)) & 0xff;
	VectorCopy (start, ex->origin);
	VectorCopy (end, ex->oldorigin);
	ex->flags = Q2RF_TRANSLUCENT | Q2RF_BEAM;
	ex->start = cl.time;
	ex->framerate = 100; // smoother fading
}

static qbyte splash_color[] = {0x00, 0xe0, 0xb0, 0x50, 0xd0, 0xe0, 0xe8};

#define ATTN_NONE	0
#define ATTN_NORM 1
#define ATTN_STATIC 1
void Q2S_StartSound(vec3_t origin, int entnum, int entchannel, sfx_t *sfx, float fvol, float attenuation, float timeofs)
{
	S_StartSoundDelayed(entnum, entchannel, sfx, origin, fvol, attenuation, timeofs);
}
void CLQ2_ParseTEnt (void)
{
	int		type;
	vec3_t	pos, pos2, dir;
	explosion_t	*ex;
	int		cnt;
	int		color;
	int		r;
//	int		ent;
//	int		magnitude;

	type = MSG_ReadByte ();

	switch (type)
	{
	case Q2TE_BLOOD:			// bullet hitting flesh
		MSG_ReadPos (pos);
		MSG_ReadDir (dir);
		if (P_RunParticleEffectType(pos, dir, 1, ptq2_blood))
			if (P_RunParticleEffectType(pos, dir, 1, ptqw_blood))
				P_RunParticleEffect(pos, dir, 0xe8, 60);
		R_AddStain(pos, 0, -10, -10, 40);
		break;

	case Q2TE_GUNSHOT:			// bullet hitting wall
	case Q2TE_SPARKS:
	case Q2TE_BULLET_SPARKS:
		MSG_ReadPos (pos);
		MSG_ReadDir (dir);
		if (type == Q2TE_GUNSHOT)
			P_RunParticleEffect (pos, dir, 0, 40);
		else
			P_RunParticleEffect (pos, dir, 0xe0, 6);

		R_AddStain(pos, -10, -10, -10, 20);

		if (type != Q2TE_SPARKS)
		{
			CL_SmokeAndFlash(pos);

			// impact sound (nope, not the same as Q1...)
			cnt = rand()&15;
			if (cnt == 1)
				Q2S_StartSound (pos, 0, 0, S_PrecacheSound ("world/ric1.wav"), 1, ATTN_NORM, 0);
			else if (cnt == 2)
				Q2S_StartSound (pos, 0, 0, S_PrecacheSound ("world/ric2.wav"), 1, ATTN_NORM, 0);
			else if (cnt == 3)
				Q2S_StartSound (pos, 0, 0, S_PrecacheSound ("world/ric3.wav"), 1, ATTN_NORM, 0);
		}

		break;

	case Q2TE_SCREEN_SPARKS:
	case Q2TE_SHIELD_SPARKS:
		MSG_ReadPos (pos);
		MSG_ReadDir (dir);
		if (type == Q2TE_SCREEN_SPARKS)
			P_RunParticleEffect (pos, dir, 0xd0, 40);
		else
			P_RunParticleEffect (pos, dir, 0xb0, 40);
		//FIXME : replace or remove this sound
		S_StartSound (-2, 0, S_PrecacheSound ("weapons/lashit.wav"), pos, 1, 1, 0);
		break;

	case Q2TE_SHOTGUN:			// bullet hitting wall
		MSG_ReadPos (pos);
		MSG_ReadDir (dir);
		P_RunParticleEffect (pos, dir, 0, 20);
		CL_SmokeAndFlash(pos);
		R_AddStain(pos, -10, -10, -10, 20);
		break;

	case Q2TE_SPLASH:			// bullet hitting water
		cnt = MSG_ReadByte ();
		MSG_ReadPos (pos);
		MSG_ReadDir (dir);
		r = MSG_ReadByte ();
		if (r > 6)
			color = 0x00;
		else
			color = splash_color[r];
		P_RunParticleEffect (pos, dir, color, cnt);

		if (r == Q2SPLASH_SPARKS)
		{
			r = rand() & 3;
			if (r == 1)
				Q2S_StartSound (pos, 0, 0, S_PrecacheSound ("world/spark5.wav"), 1, ATTN_NORM, 0);
			else if (r == 2)
				Q2S_StartSound (pos, 0, 0, S_PrecacheSound ("world/spark6.wav"), 1, ATTN_NORM, 0);
			else
				Q2S_StartSound (pos, 0, 0, S_PrecacheSound ("world/spark7.wav"), 1, ATTN_NORM, 0);

//			if (r == 0)
//				Q2S_StartSound (pos, 0, 0, cl_sfx_spark5, 1, ATTN_STATIC, 0);
//			else if (r == 1)
//				Q2S_StartSound (pos, 0, 0, cl_sfx_spark6, 1, ATTN_STATIC, 0);
//			else
//				Q2S_StartSound (pos, 0, 0, cl_sfx_spark7, 1, ATTN_STATIC, 0);
		}
		break;

	case Q2TE_LASER_SPARKS:
		cnt = MSG_ReadByte ();
		MSG_ReadPos (pos);
		MSG_ReadDir (dir);
		color = MSG_ReadByte ();
		P_RunParticleEffect (pos, dir, color, cnt);
		break;

	// RAFAEL
	case Q2TE_BLUEHYPERBLASTER:
		MSG_ReadPos (pos);
		MSG_ReadPos (dir);
		if (P_RunParticleEffectType(pos, dir, 1, ptq2_blasterparticles))
			P_RunParticleEffect (pos, dir, 0xe0, 40);
		break;

	case Q2TE_BLASTER:			// blaster hitting wall
		MSG_ReadPos (pos);
		MSG_ReadDir (dir);

		if (P_RunParticleEffectType(pos, dir, 1, ptq2_blasterparticles))
			P_RunParticleEffect (pos, dir, 0xe0, 40);

		R_AddStain(pos, 0, -5, -10, 20);

		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->origin);
		ex->start = cl.time;
		ex->model = Mod_ForName (q2tentmodels[q2cl_mod_explode].modelname, false);
		ex->firstframe = 0;
		ex->numframes = 4;
		ex->flags = Q2RF_FULLBRIGHT|Q2RF_ADDITIVE|RF_NOSHADOW|Q2RF_TRANSLUCENT;
		ex->alpha = 1;

		ex->angles[0] = acos(dir[2])/M_PI*180;
	// PMM - fixed to correct for pitch of 0
		if (dir[0])
			ex->angles[1] = atan2(dir[1], dir[0])/M_PI*180;
		else if (dir[1] > 0)
			ex->angles[1] = 90;
		else if (dir[1] < 0)
			ex->angles[1] = 270;
		else
			ex->angles[1] = 0;
		ex->angles[0]*=-1;

		S_StartSound (-2, 0, S_PrecacheSound ("weapons/lashit.wav"), pos, 1, 1, 0);

	// light
		if (r_explosionlight.value)
		{
			dlight_t *dl;
			dl = CL_AllocDlight (0);
			VectorCopy (pos, dl->origin);
			dl->radius = 150 * r_explosionlight.value;
			dl->die = cl.time + 0.4;
			dl->decay = 400;
			dl->color[0] = 0.2;
			dl->color[1] = 0.2;
			dl->color[2] = 0.0;
			dl->channelfade[0] = 0.5;
			dl->channelfade[1] = 0.51;
			dl->channelfade[2] = 0.0;
		}

		break;

	case Q2TE_RAILTRAIL:			// railgun effect
		MSG_ReadPos (pos);
		MSG_ReadPos (pos2);
		if (P_ParticleTrail(pos, pos2, rtq2_railtrail, 0, NULL))
			P_ParticleTrailIndex(pos, pos2, 0x74, 8, NULL);
		Q2S_StartSound (pos, 0, 0, S_PrecacheSound ("weapons/railgf1a.wav"), 1, ATTN_NORM, 0);
		break;

	case Q2TE_EXPLOSION2:
	case Q2TE_GRENADE_EXPLOSION:
	case Q2TE_GRENADE_EXPLOSION_WATER:
		MSG_ReadPos (pos);

		if (P_RunParticleEffectType(pos, NULL, 1, pt_explosion))
			P_RunParticleEffect(pos, NULL, 0xe0, 256);

		R_AddStain(pos, -1, -1, -1, 100);

	// light
		if (r_explosionlight.value)
		{
			dlight_t *dl;
			dl = CL_AllocDlight (0);
			VectorCopy (pos, dl->origin);
			dl->radius = 150 + r_explosionlight.value*200;
			dl->die = cl.time + 0.5;
			dl->decay = 300;
			dl->color[0] = 0.2;
			dl->color[1] = 0.1;
			dl->color[2] = 0.1;
			dl->channelfade[0] = 0.36;
			dl->channelfade[1] = 0.19;
			dl->channelfade[2] = 0.19;
		}

	// sound
		if (type == Q2TE_GRENADE_EXPLOSION_WATER)
			S_StartSound (-2, 0, S_PrecacheSound ("weapons/xpld_wat.wav"), pos, 1, 1, 0);
		else
			S_StartSound (-2, 0, S_PrecacheSound ("weapons/grenlx1a.wav"), pos, 1, 1, 0);

	// sprite

//		if (!R_ParticleExplosionHeart(pos))
		{
			ex = CL_AllocExplosion ();
			VectorCopy (pos, ex->origin);
			VectorClear(ex->angles);
			ex->start = cl.time;
			ex->model = Mod_ForName (q2tentmodels[q2cl_mod_explo4].modelname, true);
			ex->firstframe = 30;
			ex->alpha = 1;
			ex->flags |= Q2RF_TRANSLUCENT;
			ex->numframes = 19;
		}
		break;
/*
		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->ent.origin);
		ex->type = ex_poly;
		ex->ent.flags = RF_FULLBRIGHT;
		ex->start = cl.frame.servertime - 100;
		ex->light = 350;
		ex->lightcolor[0] = 1.0;
		ex->lightcolor[1] = 0.5;
		ex->lightcolor[2] = 0.5;
		ex->ent.model = cl_mod_explo4;
		ex->frames = 19;
		ex->baseframe = 30;
		ex->ent.angles[1] = rand() % 360;
		CL_ExplosionParticles (pos);
		if (type == TE_GRENADE_EXPLOSION_WATER)
			Q2S_StartSound (pos, 0, 0, cl_sfx_watrexp, 1, ATTN_NORM, 0);
		else
			Q2S_StartSound (pos, 0, 0, cl_sfx_grenexp, 1, ATTN_NORM, 0);
		break;
*/
	// RAFAEL
	case Q2TE_PLASMA_EXPLOSION:
		MSG_ReadPos (pos);
/*		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->ent.origin);
		ex->type = ex_poly;
		ex->ent.flags = RF_FULLBRIGHT;
		ex->start = cl.frame.servertime - 100;
		ex->light = 350;
		ex->lightcolor[0] = 1.0;
		ex->lightcolor[1] = 0.5;
		ex->lightcolor[2] = 0.5;
		ex->ent.angles[1] = rand() % 360;
		ex->ent.model = cl_mod_explo4;
		if (frand() < 0.5)
			ex->baseframe = 15;
		ex->frames = 15;
		CL_ExplosionParticles (pos);
		Q2S_StartSound (pos, 0, 0, cl_sfx_rockexp, 1, ATTN_NORM, 0);
*/		break;

	case Q2TE_EXPLOSION1:
	case Q2TE_EXPLOSION1_BIG:						// PMM
	case Q2TE_ROCKET_EXPLOSION:
	case Q2TE_ROCKET_EXPLOSION_WATER:
	case Q2TE_EXPLOSION1_NP:						// PMM
		MSG_ReadPos (pos);

	// particle effect
		if (type != Q2TE_EXPLOSION1_BIG && type != Q2TE_EXPLOSION1_NP)
		{
			if (P_RunParticleEffectType(pos, NULL, 1, pt_explosion))
				P_RunParticleEffect(pos, NULL, 0xe0, 256);

			R_AddStain(pos, -1, -1, -1, 100);
		}

	// light
		if (r_explosionlight.value)
		{
			dlight_t *dl;
			dl = CL_AllocDlight (0);
			VectorCopy (pos, dl->origin);
			dl->radius = 150 + r_explosionlight.value*200;
			dl->die = cl.time + 0.5;
			dl->decay = 300;
			dl->color[0] = 0.2;
			dl->color[1] = 0.1;
			dl->color[2] = 0.08;
			dl->channelfade[0] = 0.36;
			dl->channelfade[1] = 0.19;
			dl->channelfade[2] = 0.19;
		}

	// sound
		if (type == Q2TE_ROCKET_EXPLOSION_WATER)
			S_StartSound (-2, 0, S_PrecacheSound ("weapons/xpld_wat.wav"), pos, 1, 1, 0);
		else
			S_StartSound (-2, 0, S_PrecacheSound ("weapons/rocklx1a.wav"), pos, 1, 1, 0);

	// sprite
//		if (!R_ParticleExplosionHeart(pos))
		{
			ex = CL_AllocExplosion ();
			VectorCopy (pos, ex->origin);
			VectorClear(ex->angles);
			ex->start = cl.time;
			ex->model = Mod_ForName (q2tentmodels[q2cl_mod_explo4].modelname, false);
			ex->alpha = 1;
			ex->flags |= Q2RF_TRANSLUCENT;
			if (rand()&1)
				ex->firstframe = 15;
			else
				ex->firstframe = 0;
			ex->numframes = 15;
		}
		break;
/*
		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->ent.origin);
		ex->type = ex_poly;
		ex->ent.flags = RF_FULLBRIGHT;
		ex->start = cl.frame.servertime - 100;
		ex->light = 350;
		ex->lightcolor[0] = 1.0;
		ex->lightcolor[1] = 0.5;
		ex->lightcolor[2] = 0.5;
		ex->ent.angles[1] = rand() % 360;
		if (type != TE_EXPLOSION1_BIG)				// PMM
			ex->ent.model = cl_mod_explo4;			// PMM
		else
			ex->ent.model = cl_mod_explo4_big;
		if (frand() < 0.5)
			ex->baseframe = 15;
		ex->frames = 15;
		if ((type != TE_EXPLOSION1_BIG) && (type != TE_EXPLOSION1_NP))		// PMM
			CL_ExplosionParticles (pos);									// PMM
		if (type == TE_ROCKET_EXPLOSION_WATER)
			Q2S_StartSound (pos, 0, 0, cl_sfx_watrexp, 1, ATTN_NORM, 0);
		else
			Q2S_StartSound (pos, 0, 0, cl_sfx_rockexp, 1, ATTN_NORM, 0);
		break;

*/	case Q2TE_BFG_EXPLOSION:
		MSG_ReadPos (pos);
/*		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->ent.origin);
		ex->type = ex_poly;
		ex->flags = RF_FULLBRIGHT;
		ex->start = cl.q2frame.servertime - 100;
		ex->light = 350;
		ex->lightcolor[0] = 0.0;
		ex->lightcolor[1] = 1.0;
		ex->lightcolor[2] = 0.0;
		ex->model = cl_mod_bfg_explo;
		ex->flags |= RF_TRANSLUCENT;
		ex->alpha = 0.30;
		ex->frames = 4;
*/		break;

	case Q2TE_BFG_BIGEXPLOSION:
		MSG_ReadPos (pos);
//		CL_BFGExplosionParticles (pos);
		if (P_RunParticleEffectTypeString(pos, dir, 1, "te_bfg_bigexplosion"))
			P_RunParticleEffect(pos, dir, 0xd0, 256); // TODO: x+(r%8) unstead of x&7+(r&7)
		break;

	case Q2TE_BFG_LASER:
		MSG_ReadPos (pos);
		MSG_ReadPos (pos2);
		CL_Laser(pos, pos2, 0xd0d1d2d3);
		break;

	case Q2TE_BUBBLETRAIL:
		MSG_ReadPos (pos);
		MSG_ReadPos (pos2);
		if (P_ParticleTrail(pos, pos2, rtq2_bubbletrail, 0, NULL))
			P_ParticleTrailIndex(pos, pos2, 4, 8, NULL);
		break;

	case Q2TE_PARASITE_ATTACK:
	case Q2TE_MEDIC_CABLE_ATTACK:
		CL_ParseBeam (3);
		break;

	case Q2TE_BOSSTPORT:			// boss teleporting to station
		MSG_ReadPos (pos);
/*		CL_BigTeleportParticles (pos);
*/		Q2S_StartSound (pos, 0, 0, S_PrecacheSound ("misc/bigtele.wav"), 1, ATTN_NONE, 0);
		break;

	case Q2TE_GRAPPLE_CABLE:
		CL_ParseBeam (4);
		MSG_ReadPos (pos);
		break;

	// RAFAEL
	case Q2TE_WELDING_SPARKS:
		cnt = MSG_ReadByte ();
		MSG_ReadPos (pos);
		MSG_ReadDir (dir);
		color = MSG_ReadByte ();

		// TODO: fix to Q2's standards
		P_RunParticleEffect(pos, dir, color, cnt);
/*		CL_ParticleEffect2 (pos, dir, color, cnt);

		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->ent.origin);
		ex->type = ex_flash;
		// note to self
		// we need a better no draw flag
		ex->ent.flags = RF_BEAM;
		ex->start = cl.frame.servertime - 0.1;
		ex->light = 100 + (rand()%75);
		ex->lightcolor[0] = 1.0;
		ex->lightcolor[1] = 1.0;
		ex->lightcolor[2] = 0.3;
		ex->ent.model = cl_mod_flash;
		ex->frames = 2;
*/		break;

	case Q2TE_GREENBLOOD:
		MSG_ReadPos (pos);
		MSG_ReadDir (dir);
		if (P_RunParticleEffectTypeString(pos, dir, 1, "te_greenblood"))
			P_RunParticleEffect(pos, dir, 0xdf, 30); // TODO: x+(r%8) unstead of x&7+(r&7)
//		CL_ParticleEffect2 (pos, dir, 0xdf, 30);
		break;

	// RAFAEL
	case Q2TE_TUNNEL_SPARKS:
		cnt = MSG_ReadByte ();
		MSG_ReadPos (pos);
		MSG_ReadDir (dir);
		color = MSG_ReadByte ();
//		CL_ParticleEffect3 (pos, dir, color, cnt);
		break;

//=============
//PGM
		// PMM -following code integrated for flechette (different color)
	case Q2TE_BLASTER2:			// green blaster hitting wall
		MSG_ReadPos (pos);
		MSG_ReadDir (dir);

		if (P_RunParticleEffectTypeString(pos, dir, 1, "te_blaster2"))
			if (P_RunParticleEffectType(pos, dir, 1, ptq2_blasterparticles))
				P_RunParticleEffect (pos, dir, 0xd0, 40);

		R_AddStain(pos, -10, 0, -10, 20);

		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->origin);
		ex->start = cl.time;
		ex->model = Mod_ForName (q2tentmodels[q2cl_mod_explode].modelname, false);
		ex->firstframe = 0;
		ex->numframes = 4;
		ex->flags = Q2RF_FULLBRIGHT|RF_NOSHADOW;

		ex->angles[0] = acos(dir[2])/M_PI*180;
	// PMM - fixed to correct for pitch of 0
		if (dir[0])
			ex->angles[1] = atan2(dir[1], dir[0])/M_PI*180;
		else if (dir[1] > 0)
			ex->angles[1] = 90;
		else if (dir[1] < 0)
			ex->angles[1] = 270;
		else
			ex->angles[1] = 0;
		ex->angles[0]*=-1;

		S_StartSound (-2, 0, S_PrecacheSound ("weapons/lashit.wav"), pos, 1, 1, 0);

	// light
		if (r_explosionlight.value)
		{
			dlight_t *dl;
			dl = CL_AllocDlight (0);
			VectorCopy (pos, dl->origin);
			dl->radius = 150 * r_explosionlight.value;
			dl->die = cl.time + 0.4;
			dl->decay = 400;
			dl->color[0] = 0.01;
			dl->color[1] = 0.2;
			dl->color[2] = 0.01;
			dl->channelfade[0] = 0.1;
			dl->channelfade[1] = 0.5;
			dl->channelfade[2] = 0.1;
		}
		break;

	case Q2TE_FLECHETTE:			// blue blaster effect
		MSG_ReadPos (pos);
		MSG_ReadDir (dir);

		if (P_RunParticleEffectTypeString(pos, dir, 1, "te_blaster2"))
			if (P_RunParticleEffectType(pos, dir, 1, ptq2_blasterparticles))
				P_RunParticleEffect (pos, dir, 0x6f, 40);

		R_AddStain(pos, -10, -2, 0, 20);

		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->origin);
		ex->start = cl.time;
		ex->model = Mod_ForName (q2tentmodels[q2cl_mod_explode].modelname, false);
		ex->firstframe = 0;
		ex->numframes = 4;
		ex->flags = Q2RF_FULLBRIGHT|RF_NOSHADOW;

		ex->angles[0] = acos(dir[2])/M_PI*180;
	// PMM - fixed to correct for pitch of 0
		if (dir[0])
			ex->angles[1] = atan2(dir[1], dir[0])/M_PI*180;
		else if (dir[1] > 0)
			ex->angles[1] = 90;
		else if (dir[1] < 0)
			ex->angles[1] = 270;
		else
			ex->angles[1] = 0;
		ex->angles[0]*=-1;

		S_StartSound (-2, 0, S_PrecacheSound ("weapons/lashit.wav"), pos, 1, 1, 0);

	// light
		if (r_explosionlight.value)
		{
			dlight_t *dl;
			dl = CL_AllocDlight (0);
			VectorCopy (pos, dl->origin);
			dl->radius = 150 * r_explosionlight.value;
			dl->die = cl.time + 0.4;
			dl->decay = 400;
			dl->color[0] = 0.038;
			dl->color[1] = 0.082;
			dl->color[2] = 0.150;
			dl->channelfade[0] = 0.085;
			dl->channelfade[1] = 0.180;
			dl->channelfade[2] = 0.300;
		}
		break;


	case Q2TE_LIGHTNING:
		CL_ParseBeam(TE_LIGHTNING1);
		Q2S_StartSound (pos, 0, 0, S_PrecacheSound("weapons/tesla.wav"), 1, ATTN_NORM, 0);
		break;

	case Q2TE_DEBUGTRAIL:
		MSG_ReadPos (pos);
		MSG_ReadPos (pos2);
		if (P_ParticleTrail(pos, pos2, P_FindParticleType("te_debugtrail"), 0, NULL))
			P_ParticleTrailIndex(pos, pos2, 116, 8, NULL);
		break;

	case Q2TE_PLAIN_EXPLOSION:
		MSG_ReadPos (pos);

		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->origin);
//		ex->type = ex_poly;
		ex->flags = Q2RF_FULLBRIGHT|RF_NOSHADOW;
		ex->angles[1] = rand() % 360;
		ex->model = Mod_ForName (q2tentmodels[q2cl_mod_explo4].modelname, false);
		if (rand() < RAND_MAX/2)
			ex->firstframe = 15;
		ex->numframes = 15;
		Q2S_StartSound (pos, 0, 0, S_PrecacheSound("weapons/rocklx1a.wav"), 1, ATTN_NORM, 0);

	// light
		if (r_explosionlight.value)
		{
			dlight_t *dl;
			dl = CL_AllocDlight (0);
			VectorCopy (pos, dl->origin);
			dl->radius = 150 + r_explosionlight.value*200;
			dl->die = cl.time + 0.5;
			dl->decay = 300;
			dl->color[0] = 0.2;
			dl->color[1] = 0.1;
			dl->color[2] = 0.08;
			dl->channelfade[0] = 0.36;
			dl->channelfade[1] = 0.19;
			dl->channelfade[2] = 0.19;
		}

		break;
/*
	case Q2TE_FLASHLIGHT:
		MSG_ReadPos(&net_message, pos);
		ent = MSG_ReadShort(&net_message);
		CL_Flashlight(ent, pos);
		break;

	case Q2TE_FORCEWALL:
		MSG_ReadPos(&net_message, pos);
		MSG_ReadPos(&net_message, pos2);
		color = MSG_ReadByte (&net_message);
		CL_ForceWall(pos, pos2, color);
		break;
*/
	case Q2TE_HEATBEAM:
		MSG_ReadPos(pos);
		MSG_ReadPos(pos2);
//		ent = CL_ParsePlayerBeam (cl_mod_heatbeam);
		break;
/*
	case Q2TE_MONSTER_HEATBEAM:
		ent = CL_ParsePlayerBeam (cl_mod_monster_heatbeam);
		break;

	case Q2TE_HEATBEAM_SPARKS:
//		cnt = MSG_ReadByte (&net_message);
		cnt = 50;
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
//		r = MSG_ReadByte (&net_message);
//		magnitude = MSG_ReadShort (&net_message);
		r = 8;
		magnitude = 60;
		color = r & 0xff;
		CL_ParticleSteamEffect (pos, dir, color, cnt, magnitude);
		S_StartSound (pos,  0, 0, cl_sfx_lashit, 1, ATTN_NORM, 0);
		break;

	case Q2TE_HEATBEAM_STEAM:
//		cnt = MSG_ReadByte (&net_message);
		cnt = 20;
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
//		r = MSG_ReadByte (&net_message);
//		magnitude = MSG_ReadShort (&net_message);
//		color = r & 0xff;
		color = 0xe0;
		magnitude = 60;
		CL_ParticleSteamEffect (pos, dir, color, cnt, magnitude);
		S_StartSound (pos,  0, 0, cl_sfx_lashit, 1, ATTN_NORM, 0);
		break;

	case Q2TE_STEAM:
		CL_ParseSteam();
		break;

	case Q2TE_BUBBLETRAIL2:
//		cnt = MSG_ReadByte (&net_message);
		cnt = 8;
		MSG_ReadPos (&net_message, pos);
		MSG_ReadPos (&net_message, pos2);
		CL_BubbleTrail2 (pos, pos2, cnt);
		S_StartSound (pos,  0, 0, cl_sfx_lashit, 1, ATTN_NORM, 0);
		break;
*/
	case Q2TE_MOREBLOOD:
		MSG_ReadPos (pos);
		MSG_ReadDir (dir);
		if (P_RunParticleEffectTypeString(pos, dir, 1, "te_moreblood"))
			if (P_RunParticleEffectType(pos, dir, 4, ptqw_blood))
				P_RunParticleEffect(pos, dir, 0xe8, 250);
		break;
/*
	case Q2TE_CHAINFIST_SMOKE:
		dir[0]=0; dir[1]=0; dir[2]=1;
		MSG_ReadPos(&net_message, pos);
		CL_ParticleSmokeEffect (pos, dir, 0, 20, 20);
		break;

	case Q2TE_ELECTRIC_SPARKS:
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
//		CL_ParticleEffect (pos, dir, 109, 40);
		CL_ParticleEffect (pos, dir, 0x75, 40);
		//FIXME : replace or remove this sound
		S_StartSound (pos, 0, 0, cl_sfx_lashit, 1, ATTN_NORM, 0);
		break;

*/
	case Q2TE_TRACKER_EXPLOSION:
		MSG_ReadPos (pos);

		// effect
		if (P_RunParticleEffectTypeString(pos, NULL, 1, "te_tracker_explosion"))
			P_RunParticleEffect(pos, NULL, 0, 128); // TODO: needs to be nonrandom instead of 0+r%8

		// light
	// light
		if (r_explosionlight.value)
		{
			dlight_t *dl;
			dl = CL_AllocDlight (0);
			VectorCopy (pos, dl->origin);
			dl->radius = 150 * r_explosionlight.value;
			dl->die = cl.time + 0.1;
			dl->minlight = 250;
			dl->color[0] = -0.2;
			dl->color[1] = -0.2;
			dl->color[2] = -0.2;
		}

		// sound
		Q2S_StartSound (pos, 0, 0, S_PrecacheSound("weapons/disrupthit.wav"), 1, ATTN_NORM, 0);
		break;
	case Q2TE_TELEPORT_EFFECT:
	case Q2TE_DBALL_GOAL:
		MSG_ReadPos (pos);
		if (P_RunParticleEffectType(pos, NULL, 1, pt_teleportsplash))
			P_RunParticleEffect(pos, NULL, 8, 768);
		// This effect won't match ---
		// Color should be 7+(rand()%8)
		// not 8&~7+(rand()%8)
		break;

	case Q2TE_WIDOWBEAMOUT:
		// this one is really annoying, it's supposed to be a random choice
		// between 2*8, 13*8, 21*8, 18*8, and it respreads every frame
		// into a circle but it could be faked well enough, well except for
		// the fact that these effects have ids associated with them
		// sort of how beams have ents associated
		MSG_ReadShort(); // id
		if (P_RunParticleEffectTypeString(pos, NULL, 1, "te_widowbeamout"))
			P_RunParticleEffect(pos, NULL, 13*8, 300);
		break;

	case Q2TE_NUKEBLAST:
		// same problem as te_widowbeamout, but colors are a bit easier to manage
		// and there's no id to read in
		MSG_ReadPos (pos);
		if (P_RunParticleEffectTypeString(pos, NULL, 1, "te_nukeblast"))
			P_RunParticleEffect(pos, NULL, 110, 700);
		break;

	case Q2TE_WIDOWSPLASH:
		// there's the color issue like with te_widowbeamout, but the particles
		// are spawned in an immediate circle and not substained, so it's much
		// easier to manage
		MSG_ReadPos (pos);
		if (P_RunParticleEffectTypeString(pos, NULL, 1, "te_widowsplash"))
			P_RunParticleEffect(pos, NULL, 13*8, 256);
		break;
//PGM
//==============


	case CRTE_LEADERBLASTER:
		Host_EndGame ("CLQ2_ParseTEnt: bad/non-implemented type %i", type);
	case CRTE_BLASTER_MUZZLEFLASH:
		MSG_ReadPos (pos);
		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->origin);
		ex->flags = Q2RF_FULLBRIGHT|RF_NOSHADOW;
		ex->start = cl.q2frame.servertime - 100;
		CL_NewDlightRGB(0, pos, 350, 0.5, 0.2, 0.1, 0);
		P_RunParticleEffectTypeString(pos, NULL, 1, "te_muzzleflash");
		break;
	case CRTE_BLUE_MUZZLEFLASH:
		MSG_ReadPos (pos);
		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->origin);
		ex->flags = Q2RF_FULLBRIGHT|RF_NOSHADOW;
		ex->start = cl.q2frame.servertime - 100;
		CL_NewDlightRGB(0, pos, 350, 0.5, 0.2, 0.1, 0);
		P_RunParticleEffectTypeString(pos, NULL, 1, "te_blue_muzzleflash");
		break;
	case CRTE_SMART_MUZZLEFLASH:
		MSG_ReadPos (pos);
		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->origin);
		ex->flags = Q2RF_FULLBRIGHT|RF_NOSHADOW;
		ex->start = cl.q2frame.servertime - 100;
		CL_NewDlightRGB(0, pos, 350, 0.5, 0.2, 0, 0.2);
		P_RunParticleEffectTypeString(pos, NULL, 1, "te_smart_muzzleflash");
		break;
	case CRTE_LEADERFIELD:
		Host_EndGame ("CLQ2_ParseTEnt: bad/non-implemented type %i", type);
	case CRTE_DEATHFIELD:
		MSG_ReadPos (pos);
		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->origin);
		ex->flags = Q2RF_FULLBRIGHT|RF_NOSHADOW;
		ex->start = cl.q2frame.servertime - 100;
		CL_NewDlightRGB(0, pos, 350, 0.5, 0.2, 0, 0.2);
		P_RunParticleEffectTypeString(pos, NULL, 1, "te_deathfield");
		break;
	case CRTE_BLASTERBEAM:
		MSG_ReadPos (pos);
		MSG_ReadPos (pos2);
		CLQ2_BlasterTrail2 (pos, pos2);
		break;
	case CRTE_STAIN:
		Host_EndGame ("CLQ2_ParseTEnt: bad/non-implemented type %i", type);
	case CRTE_FIRE:
		Host_EndGame ("CLQ2_ParseTEnt: bad/non-implemented type %i", type);
	case CRTE_CABLEGUT:
		Host_EndGame ("CLQ2_ParseTEnt: bad/non-implemented type %i", type);
	case CRTE_SMOKE:
		Host_EndGame ("CLQ2_ParseTEnt: bad/non-implemented type %i", type);

	default:
		Host_EndGame ("CLQ2_ParseTEnt: bad/non-implemented type %i", type);
	}
}
#endif


/*
=================
CL_NewTempEntity
=================
*/
entity_t *CL_NewTempEntity (void)
{
	entity_t	*ent;

	if (cl_numvisedicts == MAX_VISEDICTS)
		return NULL;
	ent = &cl_visedicts[cl_numvisedicts];
	cl_numvisedicts++;
	ent->keynum = 0;

	memset (ent, 0, sizeof(*ent));

#ifdef PEXT_SCALE
	ent->scale = 1;
#endif
	ent->shaderRGBAf[0] = 1;
	ent->shaderRGBAf[1] = 1;
	ent->shaderRGBAf[2] = 1;
	ent->shaderRGBAf[3] = 1;
	return ent;
}


/*
=================
CL_UpdateBeams
=================
*/
void CL_UpdateBeams (void)
{
	int bnum;
	int			i, j;
	beam_t		*b;
	vec3_t		dist, org;
	float		*vieworg;
	float		d;
	entity_t	*ent;
	entity_state_t *st;
	float		yaw, pitch;
	float		forward, offset;
	int lastrunningbeam = -1;

	extern cvar_t cl_truelightning, v_viewheight;

// update lightning
	for (bnum=0, b=cl_beams; bnum < beams_running; bnum++, b++)
	{
		if (!b->active)
			continue;

		if (b->endtime < cl.time)
		{
			if (!cl.paused)
			{	/*don't let lightning decay while paused*/
				P_DelinkTrailstate(&b->trailstate);
				P_DelinkTrailstate(&b->emitstate);
				b->active = false;
				continue;
			}
		}

		lastrunningbeam = bnum;

	// if coming from the player, update the start position
		if ((b->flags & 1) && b->entity > 0 && b->entity <= cl.allocated_client_slots)
		{
			for (j = 0; j < cl.splitclients; j++)
			{
				if (b->entity == ((cl.spectator&&autocam[j])?spec_track[j]+1:(cl.playernum[j]+1)))
				{
					player_state_t	*pl;
		//			VectorSubtract(cl.simorg, b->start, org);
		//			VectorAdd(b->end, org, b->end);		//move the end point by simorg-start

					pl = &cl.frames[cl.parsecount&UPDATE_MASK].playerstate[b->entity-1];
					if (pl->messagenum == cl.parsecount || cls.protocol == CP_NETQUAKE)
					{
						vec3_t	fwd, org, ang;
						float	delta, f, len;

						if (cl.spectator && autocam[j])
						{
							vieworg = pl->origin;
						}
						else
							vieworg = cl.simorg[j];

						VectorCopy (vieworg, b->start);
						b->start[2] += cl.crouch[j] + bound(-7, v_viewheight.value, 4);

						f = bound(0, cl_truelightning.value, 1);

						if (!f)
							break;

						VectorSubtract (playerbeam_end[j], vieworg, org);
						len = VectorLength(org);
						org[2] -= 22;		// adjust for view height
						VectorAngles (org, NULL, ang);

						// lerp pitch
						ang[0] = -ang[0];
						if (ang[0] < -180)
							ang[0] += 360;
						ang[0] += (cl.simangles[j][0] - ang[0]) * f;

						// lerp yaw
						delta = cl.simangles[j][1] - ang[1];
						if (delta > 180)
							delta -= 360;
						if (delta < -180)
							delta += 360;
						ang[1] += delta * f;
						ang[2] = 0;

						AngleVectors (ang, fwd, ang, ang);
						VectorCopy(fwd, ang);
						VectorScale (fwd, len, fwd);
						VectorCopy (cl.simorg[j], org);
						org[2] += 16;
						VectorAdd (org, fwd, b->end);

						if (cl_beam_trace.ival)
						{
							vec3_t normal;
							VectorMA(org, len+4, ang, fwd);
							if (TraceLineN(org, fwd, ang, normal))
								VectorCopy (ang, b->end);
						}
						break;
					}
				}
			}
		}
		else if (b->flags & STREAM_ATTACHED)
		{
			player_state_t	*pl;
			st = CL_FindPacketEntity(b->entity);
			if (st)
			{
				VectorCopy(st->origin, b->start);
			}
			else if (b->entity <= cl.allocated_client_slots && b->entity > 0)
			{
				pl = &cl.frames[cl.parsecount&UPDATE_MASK].playerstate[b->entity-1];
				VectorCopy(pl->origin, b->start);
				b->start[2]+=16;
			}
		}

	// calculate pitch and yaw
		VectorSubtract (b->end, b->start, dist);

		if (dist[1] == 0 && dist[0] == 0)
		{
			yaw = 0;
			if (dist[2] > 0)
				pitch = 90;
			else
				pitch = 270;
		}
		else
		{
			yaw = (int) (atan2(dist[1], dist[0]) * 180 / M_PI);
			if (yaw < 0)
				yaw += 360;

			forward = sqrt (dist[0]*dist[0] + dist[1]*dist[1]);
			pitch = (int) (atan2(dist[2], forward) * 180 / M_PI);
			if (pitch < 0)
				pitch += 360;
		}

		if (ruleset_allow_particle_lightning.ival || !b->model)
			if (b->particleeffect >= 0 && !P_ParticleTrail(b->start, b->end, b->particleeffect, b->entity, &b->trailstate))
				continue;
		if (!b->model)
			continue;

	// add new entities for the lightning
		VectorCopy (b->start, org);
		d = VectorNormalize(dist);

		if(b->flags & 2)
		{
			offset = (int)(cl.time*40)%30;
			for(i = 0; i < 3; i++)
			{
				org[i] += dist[i]*offset;
			}
		}

		while (d > 0)
		{
			ent = CL_NewTempEntity ();
			if (!ent)
				return;
			VectorCopy (org, ent->origin);
			ent->model = b->model;
			ent->drawflags |= MLS_ABSLIGHT;
			ent->abslight = 192;
			ent->shaderRGBAf[3] = b->alpha;

			ent->angles[0] = -pitch;
			ent->angles[1] = yaw;
			ent->angles[2] = rand()%360;
			AngleVectors(ent->angles, ent->axis[0], ent->axis[1], ent->axis[2]);
			ent->angles[0] = pitch;

			for (i=0 ; i<3 ; i++)
				org[i] += dist[i]*30;
			d -= 30;
		}
	}

	beams_running = lastrunningbeam+1;
}

/*
=================
CL_UpdateExplosions
=================
*/
void CL_UpdateExplosions (void)
{
	int			i;
	float		f;
	int			of;
	int numframes;
	int firstframe;
	explosion_t	*ex;
	entity_t	*ent;
	int lastrunningexplosion = -1;

	for (i=0, ex=cl_explosions; i < explosions_running; i++, ex++)
	{
		if (!ex->model && !ex->flags)
			continue;

		lastrunningexplosion = i;
		f = ex->framerate*(cl.time - ex->start);
		if (ex->firstframe >= 0)
		{
			firstframe = ex->firstframe;
			numframes = ex->numframes;
		}
		else
		{
			firstframe = 0;
			numframes = ex->model->numframes;
		}

		of = (int)f-1;
		if ((int)f >= numframes || (int)f < 0)
		{
			ex->model = NULL;
			ex->flags = 0;
			continue;
		}
		if (of < 0)
			of = 0;

		ent = CL_NewTempEntity ();
		if (!ent)
			return;
		VectorMA (ex->origin, f, ex->velocity, ent->origin);
		VectorCopy (ex->oldorigin, ent->oldorigin);
		VectorCopy (ex->angles, ent->angles);
		ent->skinnum = ex->skinnum;
		ent->angles[0]*=-1;
		AngleVectors(ent->angles, ent->axis[0], ent->axis[1], ent->axis[2]);
		VectorInverse(ent->axis[1]);
		ent->model = ex->model;
		ent->framestate.g[FS_REG].frame[1] = (int)f+firstframe;
		ent->framestate.g[FS_REG].frame[0] = of+firstframe;
		ent->framestate.g[FS_REG].lerpfrac = (f - (int)f);
		if (ent->model && ent->model->type == mod_sprite)
			ent->shaderRGBAf[3] = ex->alpha;	/*sprites don't fade over time, the animation should do it*/
		else
			ent->shaderRGBAf[3] = (1.0 - f/(numframes))*ex->alpha;
		ent->flags = ex->flags;

		if (ex->flags & Q2RF_BEAM)
		{
			ent->rtype = RT_BEAM;
			ent->shaderRGBAf[0] = ((d_8to24rgbtable[ex->skinnum & 0xFF] >>  0) & 0xFF)/255.0;
			ent->shaderRGBAf[1] = ((d_8to24rgbtable[ex->skinnum & 0xFF] >>  8) & 0xFF)/255.0;
			ent->shaderRGBAf[2] = ((d_8to24rgbtable[ex->skinnum & 0xFF] >> 16) & 0xFF)/255.0;
		}
		else
		{
			ent->skinnum = 7*f/(numframes);
		}
	}

	explosions_running = lastrunningexplosion + 1;
}

entity_state_t *CL_FindPacketEntity(int num);

/*
=================
CL_UpdateTEnts
=================
*/
void CL_UpdateTEnts (void)
{
	CL_UpdateBeams ();
	CL_UpdateExplosions ();
}
