#ifndef _PARTICLES_H_
#define _PARTICLES_H_

extern int pt_explosion,
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
	pt_torch,
	pt_flame,
	pt_bullet,
	pt_superbullet,
	pe_default;

extern int rt_blastertrail,
	rt_railtrail,
	rt_bubbletrail,
	rt_rocket,
	rt_grenade,
	rt_gib,
	rt_lightning1,
	rt_lightning2,
	rt_lightning3,
	pt_lightning1_end,
	pt_lightning2_end,
	pt_lightning3_end;

/*
extern int rt_rocket_trail,
	rt_smoke,
	rt_blood,
	rt_tracer,
	rt_slight_blood,
	rt_tracer2,
	rt_voor_trail,
	rt_fireball,
	rt_ice,
	rt_spit,
	rt_spell,
	rt_vorpal,
	rt_setstaff,
	rt_magicmissile,
	rt_boneshard,
	rt_scarab,
	rt_acidball,
	rt_bloodshot,
	rt_blastertrail,
	rt_railtrail,
	rt_bubbletrail;
*/

struct beamseg_s;

typedef struct trailstate_s {
	struct trailstate_s **key;  // key to check if ts has been overwriten
	struct trailstate_s *assoc; // assoc linked trail
	struct beamseg_s *lastbeam; // last beam pointer (flagged with BS_LASTSEG)
	union {
		float lastdist;			// last distance used with particle effect
		float statetime;		// time to emit effect again (used by spawntime field)
	};
	union {
		float laststop;			// last stopping point for particle effect
		float emittime;			// used by r_effect emitters
	};
} trailstate_t;

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

	vec3_t		vel;	//renderer uses for sparks
	float		angle;
	union {
		float nextemit;
		trailstate_t *trailstate;
	};
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

#define PARTICLE_Z_CLIP	8.0

#define frandom() (rand()*(1.0f/RAND_MAX))
#define crandom() (rand()*(2.0f/RAND_MAX)-1.0f)
#define hrandom() (rand()*(1.0f/RAND_MAX)-0.5f)

//main functions
void P_DrawParticles (void);
void P_InitParticles (void);
void P_ClearParticles (void);
void P_NewServer(void);

//allocate a new effect
int P_ParticleTypeForName(char *name);
int P_AllocateParticleType(char *name);	//find one if it exists, or create if it doesn't.
int P_FindParticleType(char *name); //checks if particle description 'name' exists, returns -1 if not.

int P_DescriptionIsLoaded(char *name);	//returns true if it's usable.

void P_SkyTri(float *v1, float *v2, float *v3, struct msurface_s *surf);

// default particle effect
void P_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count);	//aka: the particle builtin.

//wierd effects
void P_RunParticleEffect2 (vec3_t org, vec3_t dmin, vec3_t dmax, int color, int effect, int count);	//these three are needed for hexen2.
void P_RunParticleEffect3 (vec3_t org, vec3_t box, int color, int effect, int count);
void P_RunParticleEffect4 (vec3_t org, float radius, int color, int effect, int count);

void P_DarkFieldParticles (float *org, qbyte colour);
void P_EntityParticles (float *org, qbyte colour, float *radius);	//nq's EF_BRIGHTFIELD
void P_EmitEffect (vec3_t pos, int type, trailstate_t **tsk);	//particles centered around a model, called every frame for those models.

//functions that spawn point effects (basically just pass throughs)
void P_BlobExplosion (vec3_t org);	//tarbaby explosion or TF emp.
void P_ParticleExplosion (vec3_t org);	//rocket explosion (sprite is allocated seperatly :( )
void P_LavaSplash (vec3_t org);	//cthon dying, or a gas grenade.
void P_RunParticleCube(vec3_t minb, vec3_t maxb, vec3_t dir, float count, int colour, qboolean gravity, float jitter);
void P_RunParticleWeather(vec3_t minb, vec3_t maxb, vec3_t dir, float count, int colour, char *efname);

//the core spawn function for trails. (trailstate can be null)
int P_ParticleTrail (vec3_t start, vec3_t end, int type, trailstate_t **trailstate);

void P_DefaultTrail (struct model_s *model);	//fills in the default particle properties for a loaded model. Should already have the model flags set.

//the core spawn function for point effects
int P_RunParticleEffectState (vec3_t org, vec3_t dir, float count, int typenum, trailstate_t **tsk); //1 if failed
int P_RunParticleEffectTypeString (vec3_t org, vec3_t dir, float count, char *name); //1 if failed.
#define P_RunParticleEffectType(a,b,c,d) P_RunParticleEffectState(a,b,c,d,NULL)

void P_EmitSkyEffectTris(struct model_s *mod, struct msurface_s 	*fa);

// trailstate functions
void P_DelinkTrailstate(trailstate_t **tsk);

typedef enum { BM_BLEND, BM_BLENDCOLOUR, BM_ADD, BM_SUBTRACT } blendmode_t;

#endif
