/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#include "quakedef.h"

#define NUMVERTEXNORMALS	162
float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};


void R_Rockettrail_Callback(struct cvar_s *var, char *oldvalue)
{
	int i;
	model_t *mod;
	extern model_t	*mod_known;
	extern int		mod_numknown;

	if (cls.state == ca_disconnected)
		return; // don't bother parsing while disconnected

	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		if (!mod->needload)
			if (mod->flags & MF_ROCKET)
				P_DefaultTrail(mod);
	}
}

void R_Grenadetrail_Callback(struct cvar_s *var, char *oldvalue)
{
	int i;
	model_t *mod;
	extern model_t	*mod_known;
	extern int		mod_numknown;

	if (cls.state == ca_disconnected)
		return; // don't bother parsing while disconnected

	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		if (!mod->needload)
			if (mod->flags & MF_GRENADE)
				P_DefaultTrail(mod);
	}
}

extern particleengine_t pe_null;
#ifdef PSET_CLASSIC
extern particleengine_t pe_classic;
#endif
particleengine_t pe_darkplaces;
particleengine_t pe_qmb;
#ifdef PSET_SCRIPT
extern particleengine_t pe_script;
#endif

particleengine_t *particlesystem[] =
{
#ifdef PSET_SCRIPT
	&pe_script,
#endif
	&pe_darkplaces,
	&pe_qmb,
#ifdef PSET_CLASSIC
	&pe_classic,
#endif
	&pe_null,
	NULL,
};

void R_ParticleSystem_Callback(struct cvar_s *var, char *oldvalue)
{
	int i;
	if (pe)
	{
		CL_ClearTEntParticleState();
		CL_ClearLerpEntsParticleState();
#ifdef Q2CLIENT
		CLQ2_ClearParticleState();
#endif

		pe->ShutdownParticles();
	}

	if (!qrenderer)
	{
		pe = &pe_null;
	}
	else
	{
		pe = NULL;
		for (i = 0; particlesystem[i]; i++)
		{
			if (   (particlesystem[i]->name1 && !stricmp(var->string, particlesystem[i]->name1))
				|| (particlesystem[i]->name2 && !stricmp(var->string, particlesystem[i]->name2)))
			{
				pe = particlesystem[i];
				break;
			}
			if (!pe)
				if (particlesystem[i]->name1)
					pe = particlesystem[i];
		}
	}
	if (!pe)
		Sys_Error("No particle system available. Please recompile.");

	if (!pe->InitParticles())
	{
		Con_Printf("Particlesystem %s failed to init\n", pe->name1);
		pe = &pe_null;
		pe->InitParticles();
	}
	pe->ClearParticles();
	CL_RegisterParticles();
}

cvar_t r_rockettrail = CVARFC("r_rockettrail", "1", CVAR_SEMICHEAT, R_Rockettrail_Callback);
cvar_t r_grenadetrail = CVARFC("r_grenadetrail", "1", CVAR_SEMICHEAT, R_Grenadetrail_Callback);
cvar_t r_particlesystem = CVARFC("r_particlesystem", IFMINIMAL("classic", "script"), CVAR_SEMICHEAT|CVAR_ARCHIVE, R_ParticleSystem_Callback);
cvar_t r_particledesc = CVARAF("r_particledesc", "classic", "r_particlesdesc", CVAR_SEMICHEAT|CVAR_ARCHIVE);
extern cvar_t r_bouncysparks;
extern cvar_t r_part_rain;
extern cvar_t r_bloodstains;
extern cvar_t gl_part_flame;
cvar_t r_part_rain_quantity = CVARF("r_part_rain_quantity", "1", CVAR_ARCHIVE);

cvar_t r_particle_tracelimit = CVARFD("r_particle_tracelimit", "200", CVAR_ARCHIVE, "Number of traces to allow per frame for particle physics.");
cvar_t r_part_sparks = CVAR("r_part_sparks", "1");
cvar_t r_part_sparks_trifan = CVAR("r_part_sparks_trifan", "1");
cvar_t r_part_sparks_textured = CVAR("r_part_sparks_textured", "1");
cvar_t r_part_beams = CVAR("r_part_beams", "1");
cvar_t r_part_contentswitch = CVARFD("r_part_contentswitch", "1", CVAR_ARCHIVE, "Enable particle effects to change based on content (ex. water).");
cvar_t r_part_density = CVARF("r_part_density", "1", CVAR_ARCHIVE);
cvar_t r_part_classic_expgrav = CVARFD("r_part_classic_expgrav", "10", CVAR_ARCHIVE, "Scaler for how fast classic explosion particles should accelerate due to gravity. 1 for like vanilla, 10 for like zquake.");


particleengine_t *pe;

void P_InitParticleSystem(void)
{
	char *particlecvargroupname = "Particle effects";

	Cvar_Register(&r_particlesystem, "Particles");

	//particles
	Cvar_Register(&r_particledesc, particlecvargroupname);
	Cvar_Register(&r_bouncysparks, particlecvargroupname);
	Cvar_Register(&r_part_rain, particlecvargroupname);

	Cvar_Register(&r_part_rain_quantity, particlecvargroupname);

	Cvar_Register(&r_particle_tracelimit, particlecvargroupname);

	Cvar_Register(&r_part_sparks, particlecvargroupname);
	Cvar_Register(&r_part_sparks_trifan, particlecvargroupname);
	Cvar_Register(&r_part_sparks_textured, particlecvargroupname);
	Cvar_Register(&r_part_beams, particlecvargroupname);
	Cvar_Register(&r_part_contentswitch, particlecvargroupname);
	Cvar_Register(&r_part_density, particlecvargroupname);
	Cvar_Register(&r_part_classic_expgrav, particlecvargroupname);

	Cvar_Register (&gl_part_flame, particlecvargroupname);

	Cvar_Register (&r_rockettrail, particlecvargroupname);
	Cvar_Register (&r_grenadetrail, particlecvargroupname);
}

void P_Shutdown(void)
{
	if (pe)
	{
		CL_ClearTEntParticleState();
		CL_ClearLerpEntsParticleState();
#ifdef Q2CLIENT
		CLQ2_ClearParticleState();
#endif
		pe->ShutdownParticles();
	}
	pe = NULL;
}

qboolean TraceLineN (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal)
{
	trace_t		trace;
	float len, bestlen;
	int i;
	vec3_t delta, ts, te;
	physent_t *pe;
	qboolean clipped=false;
	vec3_t axis[3];

	memset (&trace, 0, sizeof(trace));

	VectorSubtract(end, start, delta);
	bestlen = Length(delta);

	VectorCopy (end, impact);

	for (i=0 ; i< pmove.numphysent ; i++)
	{
		pe = &pmove.physents[i];
		if (pe->nonsolid)
			continue;
		if (pe->model && !pe->model->needload)
		{
			VectorSubtract(start, pe->origin, ts);
			VectorSubtract(end, pe->origin, te);
			if (pe->angles[0] || pe->angles[1] || pe->angles[2])
			{
				AngleVectors(pe->angles, axis[0], axis[1], axis[2]);
				VectorNegate(axis[1], axis[1]);
				pe->model->funcs.NativeTrace(pe->model, 0, 0, axis, ts, te, vec3_origin, vec3_origin, false, MASK_WORLDSOLID, &trace);
			}
			else
				pe->model->funcs.NativeTrace(pe->model, 0, 0, NULL, ts, te, vec3_origin, vec3_origin, false, MASK_WORLDSOLID, &trace);
			if (trace.fraction<1)
			{
				VectorSubtract(trace.endpos, ts, delta);
				len = Length(delta);
				if (len < bestlen)
				{
					bestlen = len;
					if (normal)
						VectorCopy (trace.plane.normal, normal);
					VectorAdd (pe->origin, trace.endpos, impact);
				}

				clipped=true;
			}
			if (trace.startsolid)
			{
				VectorNormalize(delta);
				if (normal)
				{
					normal[0] = -delta[0];
					normal[1] = -delta[1];
					normal[2] = -delta[2];
				}
				VectorCopy (end, impact);
				return false;
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

//handy utility...
void P_EmitEffect (vec3_t pos, int type, trailstate_t **tsk)
{
	if (cl.paused)
		return;

	pe->RunParticleEffectState(pos, NULL, ((host_frametime>0.1)?0.1:host_frametime), type, tsk);
}











// P_SelectableTrail: given default/opposite effects, model pointer, and a user selection cvar
// changes model to the appropriate trail effect and default trail index
void P_SelectableTrail(model_t *model, cvar_t *selection, int mdleffect, int mdlcidx, int oppeffect, int oppcidx)
{
	int select = (int)(selection->value);

	switch (select)
	{
	case 0: // check for string, otherwise no trail
		if (selection->string[0] == '0')
		{
			model->particletrail = P_INVALID;
			model->traildefaultindex = -1;
			break;
		}
		else
		{
			int effect = P_FindParticleType(selection->string);

			if (effect >= 0)
			{
				model->particletrail = effect;
				model->traildefaultindex = mdlcidx;
				break;
			}
		}
		// fall through to default (so semicheat will work properly)
	case 1: // default model effect
	default:
		model->particletrail = mdleffect;
		model->traildefaultindex = mdlcidx;
		break;
	case 2: // opposite effect
		model->particletrail = oppeffect;
		model->traildefaultindex= oppcidx;
		break;
	case 3: // alt rocket effect
		model->particletrail = P_FindParticleType("TR_ALTROCKET");
		model->traildefaultindex = 107;
		break;
	case 4: // gib
		model->particletrail = P_FindParticleType("TR_BLOOD");
		model->traildefaultindex = 70;
		break;
	case 5: // zombie gib
		model->particletrail = P_FindParticleType("TR_SLIGHTBLOOD");
		model->traildefaultindex = 70;
		break;
	case 6: // Scrag tracer
		model->particletrail = P_FindParticleType("TR_WIZSPIKE");
		model->traildefaultindex = 60;
		break;
	case 7: // Knight tracer
		model->particletrail = P_FindParticleType("TR_KNIGHTSPIKE");
		model->traildefaultindex = 238;
		break;
	case 8: // Vore tracer
		model->particletrail = P_FindParticleType("TR_VORESPIKE");
		model->traildefaultindex = 154;
		break;
	case 9: // rail trail
		model->particletrail = P_FindParticleType("TE_RAILTRAIL");
		model->traildefaultindex = 15;
		break;
	}
}



//figure out which particle trail to use for the given model, filling in its values as required.
void P_DefaultTrail (model_t *model)
{
	// TODO: EF_BRIGHTFIELD should probably be handled in here somewhere
	// TODO: make trail default color into RGB values instead of indexes
	if (model->engineflags & MDLF_NODEFAULTTRAIL)
		return;
	if (!pe)
		return;

	if (model->flags & MF_ROCKET)
		P_SelectableTrail(model, &r_rockettrail, P_FindParticleType("TR_ROCKET"), 109, P_FindParticleType("TR_GRENADE"), 6);
	else if (model->flags & MF_GRENADE)
		P_SelectableTrail(model, &r_grenadetrail, P_FindParticleType("TR_GRENADE"), 6, P_FindParticleType("TR_ROCKET"), 109);
	else if (model->flags & MF_GIB)
	{
		model->particletrail = P_FindParticleType("TR_BLOOD");
		model->traildefaultindex = 70;
	}
	else if (model->flags & MF_TRACER)
	{
		model->particletrail = P_FindParticleType("TR_WIZSPIKE");
		model->traildefaultindex = 60;
	}
	else if (model->flags & MF_ZOMGIB)
	{
		model->particletrail = P_FindParticleType("TR_SLIGHTBLOOD");
		model->traildefaultindex = 70;
	}
	else if (model->flags & MF_TRACER2)
	{
		model->particletrail = P_FindParticleType("TR_KNIGHTSPIKE");
		model->traildefaultindex = 238;
	}
	else if (model->flags & MF_TRACER3)
	{
		model->particletrail = P_FindParticleType("TR_VORESPIKE");
		model->traildefaultindex = 154;
	}
	else if (model->flags & MFH2_BLOODSHOT)	//these are the hexen2 ones.
	{
		model->particletrail = P_FindParticleType("tr_bloodshot");
		model->traildefaultindex = 136;
	}
	else if (model->flags & MFH2_FIREBALL)
	{
		model->particletrail = P_FindParticleType("tr_fireball");
		model->traildefaultindex = 424;
	}
	else if (model->flags & MFH2_ACIDBALL)
	{
		model->particletrail = P_FindParticleType("tr_acidball");
		model->traildefaultindex = 440;
	}
	else if (model->flags & MFH2_ICE)
	{
		model->particletrail = P_FindParticleType("tr_ice");
		model->traildefaultindex = 408;
	}
	else if (model->flags & MFH2_SPIT)
	{
		model->particletrail = P_FindParticleType("tr_spit");
		model->traildefaultindex = 260;
	}
	else if (model->flags & MFH2_SPELL)
	{
		model->particletrail = P_FindParticleType("tr_spell");
		model->traildefaultindex = 260;
	}
	else if (model->flags & MFH2_VORP_MISSILE)
	{
		model->particletrail = P_FindParticleType("tr_vorpmissile");
		model->traildefaultindex = 302;
	}
	else if (model->flags & MFH2_SET_STAFF)
	{
		model->particletrail = P_FindParticleType("tr_setstaff");
		model->traildefaultindex = 424;
	}
	else if (model->flags & MFH2_MAGICMISSILE)
	{
		model->particletrail = P_FindParticleType("tr_magicmissile");
		model->traildefaultindex = 149;
	}
	else if (model->flags & MFH2_BONESHARD)
	{
		model->particletrail = P_FindParticleType("tr_boneshard");
		model->traildefaultindex = 384;
	}
	else if (model->flags & MFH2_SCARAB)
	{
		model->particletrail = P_FindParticleType("tr_scarab");
		model->traildefaultindex = 254;
	}
	else
	{
		model->particletrail = P_INVALID;
		model->traildefaultindex = -1;
	}
}
