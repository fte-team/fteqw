#include "quakedef.h"
#include "glquake.h"

#include "particles.h"

//obtains an index for the name, even if it is unknown (one can be loaded after. will only fail if the effect limit is reached)
static int PNULL_ParticleTypeForName(char *name)
{
	Con_Printf("P_ParticleTypeForName %s\n", name);
	return P_INVALID;
}

//returns a valid effect if its existance is known, even if simply referenced. This should be phased out.
static int PNULL_FindParticleType(char *name)
{
	Con_Printf("P_FindParticleType %s\n", name);
	return P_INVALID;
}

static int PNULL_RunParticleEffectTypeString (vec3_t org, vec3_t dir, float count, char *name){return 1;}
static int PNULL_ParticleTrail (vec3_t startpos, vec3_t end, int type, trailstate_t **tsk){return 1;}
static int PNULL_RunParticleEffectState (vec3_t org, vec3_t dir, float count, int typenum, trailstate_t **tsk){return 1;}
static void PNULL_RunParticleWeather(vec3_t minb, vec3_t maxb, vec3_t dir, float count, int colour, char *efname){}
static void PNULL_RunParticleCube(vec3_t minb, vec3_t maxb, vec3_t dir, float count, int colour, qboolean gravity, float jitter){}
static void PNULL_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count){}
static void PNULL_RunParticleEffect2 (vec3_t org, vec3_t dmin, vec3_t dmax, int color, int effect, int count){}
static void PNULL_RunParticleEffect3 (vec3_t org, vec3_t box, int color, int effect, int count){}
static void PNULL_RunParticleEffect4 (vec3_t org, float radius, int color, int effect, int count){}

static void PNULL_ParticleTrailIndex (vec3_t start, vec3_t end, int color, int crnd, trailstate_t **tsk){}
static void PNULL_EmitSkyEffectTris(model_t *mod, msurface_t 	*fa){}

static void PNULL_InitParticles (void)
{
	CL_RegisterParticles();
}

static void PNULL_ShutdownParticles(void)
{
}

static void PNULL_DelinkTrailstate(trailstate_t **tsk){}
static void PNULL_ClearParticles (void){}
static void PNULL_DrawParticles(void)
{
	RSpeedLocals();

	RSpeedRemark();
#ifdef GLQUAKE
	RQ_RenderDistAndClear();
#endif
	RSpeedEnd(RSPEED_PARTICLESDRAW);
}
static void PNULL_FlushRenderer(void)
{
}


particleengine_t pe_null =
{
	"null",
	"none",

	PNULL_ParticleTypeForName,
	PNULL_FindParticleType,

	PNULL_RunParticleEffectTypeString,
	PNULL_ParticleTrail,
	PNULL_RunParticleEffectState,
	PNULL_RunParticleWeather,
	PNULL_RunParticleCube,
	PNULL_RunParticleEffect,
	PNULL_RunParticleEffect2,
	PNULL_RunParticleEffect3,
	PNULL_RunParticleEffect4,

	PNULL_ParticleTrailIndex,
	PNULL_EmitSkyEffectTris,
	PNULL_InitParticles,
	PNULL_ShutdownParticles,
	PNULL_DelinkTrailstate,
	PNULL_ClearParticles,
	PNULL_DrawParticles,
	PNULL_FlushRenderer
};