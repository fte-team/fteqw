#ifndef _PARTICLES_H_
#define _PARTICLES_H_

extern int pt_explosion,
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
	pt_torch,
	pt_flame,
	pt_bullet,
	pt_superbullet,
	pe_default;

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

// !!! if this is changed, it must be changed in d_ifacea.h too !!!
typedef struct particle_s
{
// driver-usable fields
	vec3_t		org;
	float		color;
	vec3_t		rgb;
	float		alpha;
	float		scale;

	union {
		struct {
			vec3_t		vel;
			float		angle;
			float		rotationspeed;
			float		nextemit;
		} p;	//point blob
/*		struct {
			vec3_t		org2;
			vec3_t		lastdir;
		} b;
		*/
	} u;

// drivers never touch the following fields

	struct particle_s	*next;
	float		die;
} particle_t;

#define BS_LASTSEG 0x1 // no draw to next, no delete
#define BS_DEAD    0x2 // segment is dead
#define BS_NODRAW  0x4 // only used for lerp switching

typedef struct beamseg_s
{
	particle_t *p; 
	struct beamseg_s *next;  // next in beamseg list
	int    flags;            // flags for beamseg
	vec3_t dir;

	float texture_s;
} beamseg_t;

#define PARTICLE_Z_CLIP	8.0

#define frandom() (rand()*(1.0f/RAND_MAX))
#define crandom() (rand()*(2.0f/RAND_MAX)-1.0f)
#define hrandom() (rand()*(1.0f/RAND_MAX)-0.5f)

#endif
