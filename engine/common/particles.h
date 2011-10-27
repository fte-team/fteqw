#ifndef _PARTICLES_H_
#define _PARTICLES_H_

extern int 
	pt_gunshot,
	ptdp_gunshotquad,
	pt_spike,
	ptdp_spikequad,
	pt_superspike,
	ptdp_superspikequad,
	pt_wizspike,
	pt_knightspike,
	pt_explosion,
	ptdp_explosionquad,
	pt_tarexplosion,
	pt_teleportsplash,
	pt_lavasplash,
	ptdp_smallflash,
	ptdp_flamejet,
	ptdp_flame,
	ptdp_blood,
	ptdp_spark,
	ptdp_plasmaburn,
	ptdp_tei_g3,
	ptdp_tei_smoke,
	ptdp_tei_bigexplosion,
	ptdp_tei_plasmahit,
	ptdp_stardust,
	rt_rocket,
	rt_grenade,
	rt_blood,
	rt_wizspike,
	rt_slightblood,
	rt_knightspike,
	rt_vorespike,
	rtdp_neharasmoke,
	rtdp_nexuizplasma,
	rtdp_glowtrail,

	ptqw_blood,
	ptqw_lightningblood,
	
	ptq2_blood,
	rtq2_railtrail,
	rtq2_blastertrail,
	ptq2_blasterparticles,
	rtq2_bubbletrail,
	rtq2_gib,
	rtq2_rocket,
	rtq2_grenade,

	rtqw_railtrail,	//common to zquake/fuhquake/fte
	rtfte_lightning1,
	ptfte_lightning1_end,
	rtfte_lightning2,
	ptfte_lightning2_end,
	rtfte_lightning3,
	ptfte_lightning3_end,
	ptfte_bullet,
	ptfte_superbullet;

struct beamseg_s;

typedef struct trailstate_s {
	struct trailstate_s **key;  // key to check if ts has been overwriten
	struct trailstate_s *assoc; // assoc linked trail
	struct beamseg_s *lastbeam; // last beam pointer (flagged with BS_LASTSEG)
	union {
		float lastdist;			// last distance used with particle effect
		float statetime;		// time to emit effect again (used by spawntime field)
	} state1;
	union {
		float laststop;			// last stopping point for particle effect
		float emittime;			// used by r_effect emitters
	} state2;
} trailstate_t;

#define PARTICLE_Z_CLIP	8.0

typedef enum { BM_BLEND, BM_BLENDCOLOUR, BM_ADD, BM_SUBTRACT, BM_INVMOD } blendmode_t;

#define frandom() (rand()*(1.0f/RAND_MAX))
#define crandom() (rand()*(2.0f/RAND_MAX)-1.0f)
#define hrandom() (rand()*(1.0f/RAND_MAX)-0.5f)

#define P_INVALID -1

#define P_RunParticleEffectType(a,b,c,d) P_RunParticleEffectState(a,b,c,d,NULL)

// used for callback
extern cvar_t r_particlesdesc;
extern cvar_t r_particlesystem;

struct model_s;
struct msurface_s;

void P_InitParticleSystem(void);
void P_Shutdown(void);
void P_LoadedModel(struct model_s *mod);	/*checks a model's various effects*/
void P_DefaultTrail (struct model_s *model);
void P_EmitEffect (vec3_t pos, int type, trailstate_t **tsk);//this is just a wrapper

#define P_FindParticleType pe->FindParticleType

#define P_RunParticleEffectTypeString pe->RunParticleEffectTypeString
#define P_ParticleTrail pe->ParticleTrail
#define P_RunParticleEffectState pe->RunParticleEffectState
#define P_RunParticleWeather pe->RunParticleWeather
#define P_RunParticleCube pe->RunParticleCube
#define P_RunParticleEffect pe->RunParticleEffect
#define P_RunParticleEffect2 pe->RunParticleEffect2
#define P_RunParticleEffect3 pe->RunParticleEffect3
#define P_RunParticleEffect4 pe->RunParticleEffect4

#define P_ParticleTrailIndex pe->ParticleTrailIndex
#define P_EmitSkyEffectTris pe->EmitSkyEffectTris
#define P_InitParticles pe->InitParticles
#define P_DelinkTrailstate pe->DelinkTrailstate
#define P_ClearParticles pe->ClearParticles
#define P_DrawParticles pe->DrawParticles

typedef struct {
	char *name1;
	char *name2;


	int (*ParticleTypeForName) (char *name);
	int (*FindParticleType) (char *name);

	int (*RunParticleEffectTypeString) (vec3_t org, vec3_t dir, float count, char *name);
	int (*ParticleTrail) (vec3_t startpos, vec3_t end, int type, int dlkey, trailstate_t **tsk);
	int (*RunParticleEffectState) (vec3_t org, vec3_t dir, float count, int typenum, trailstate_t **tsk);
	void (*RunParticleWeather) (vec3_t minb, vec3_t maxb, vec3_t dir, float count, int colour, char *efname);
	void (*RunParticleCube) (vec3_t minb, vec3_t maxb, vec3_t dir, float count, int colour, qboolean gravity, float jitter);
	void (*RunParticleEffect) (vec3_t org, vec3_t dir, int color, int count);
	void (*RunParticleEffect2) (vec3_t org, vec3_t dmin, vec3_t dmax, int color, int effect, int count);
	void (*RunParticleEffect3) (vec3_t org, vec3_t box, int color, int effect, int count);
	void (*RunParticleEffect4) (vec3_t org, float radius, int color, int effect, int count);

	void (*ParticleTrailIndex) (vec3_t start, vec3_t end, int color, int crnd, trailstate_t **tsk);
	void (*EmitSkyEffectTris) (struct model_s *mod, struct msurface_s *fa, int ptype);
	qboolean (*InitParticles) (void);
	void (*ShutdownParticles) (void);
	void (*DelinkTrailstate) (trailstate_t **tsk);
	void (*ClearParticles) (void);
	void (*DrawParticles) (void);
} particleengine_t;
extern particleengine_t *pe;

#endif
