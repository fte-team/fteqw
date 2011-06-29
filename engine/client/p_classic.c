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

#ifdef PSET_CLASSIC

#include "glquake.h"
#include "shader.h"
#include "renderque.h"

void D_DrawParticleTrans (vec3_t porg, float palpha, float pscale, unsigned int pcolour, blendmode_t blendmode);


typedef enum {
	DODGY,

	ROCKET_TRAIL,
	ALT_ROCKET_TRAIL,
	BLOOD_TRAIL,
	GRENADE_TRAIL,
	BIG_BLOOD_TRAIL,
	TRACER1_TRAIL,
	TRACER2_TRAIL,
	VOOR_TRAIL,

	BLOBEXPLOSION_POINT,
	LAVASPLASH_POINT,
	EXPLOSION_POINT,
	TELEPORTSPLASH_POINT,

	EFFECTTYPE_MAX
} effect_type_t;


typedef struct cparticle_s
{
	avec3_t org;
	float die;
	avec3_t vel;
	float ramp;
	enum
	{
		pt_static,
		pt_fire,
		pt_explode,
		pt_explode2,
		pt_blob,
		pt_blob2,
		pt_grav,
		pt_slowgrav
	} type;
	unsigned int rgb;
	struct cparticle_s *next;
} cparticle_t;

#define DEFAULT_NUM_PARTICLES	2048
#define ABSOLUTE_MIN_PARTICLES	512
#define ABSOLUTE_MAX_PARTICLES	8192
static int r_numparticles;
static cparticle_t	*particles, *active_particles, *free_particles;

extern qbyte default_quakepal[]; /*for ramps more than anything else*/
static int	ramp1[8] = {0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61};
static int	ramp2[8] = {0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66};
static int	ramp3[8] = {0x6d, 0x6b, 6, 5, 4, 3};
#define qpal(q) ((default_quakepal[(q)*3+0]<<0) | (default_quakepal[(q)*3+1]<<8) | (default_quakepal[(q)*3+2]<<16))


#define BUFFERVERTS 2048*3
static vecV_t classicverts[BUFFERVERTS];
static union c
{
	byte_vec4_t b;
	unsigned int i;
} classiccolours[BUFFERVERTS];
static vec2_t classictexcoords[BUFFERVERTS];
static index_t classicindexes[BUFFERVERTS];
mesh_t classicmesh;
static shader_t *classicshader;



//obtains an index for the name, even if it is unknown (one can be loaded after. will only fail if the effect limit is reached)
//technically this function is not meant to fail often, but thats fine so long as the other functions are meant to safely reject invalid effect numbers.
static int PClassic_FindParticleType(char *name)
{
	if (!stricmp("tr_rocket", name))
		return ROCKET_TRAIL;
	if (!stricmp("tr_altrocket", name))
		return ALT_ROCKET_TRAIL;
	if (!stricmp("tr_slightblood", name))
		return BLOOD_TRAIL;
	if (!stricmp("tr_grenade", name))
		return GRENADE_TRAIL;
	if (!stricmp("tr_blood", name))
		return BIG_BLOOD_TRAIL;
	if (!stricmp("tr_wizspike", name))
		return TRACER1_TRAIL;
	if (!stricmp("tr_knightspike", name))
		return TRACER2_TRAIL;
	if (!stricmp("tr_vorespike", name))
		return VOOR_TRAIL;

	if (!stricmp("te_tarexplosion", name))
		return BLOBEXPLOSION_POINT;
	if (!stricmp("te_lavasplash", name))
		return LAVASPLASH_POINT;
	if (!stricmp("te_lavasplash", name))
		return LAVASPLASH_POINT;
	if (!stricmp("te_explosion", name))
		return EXPLOSION_POINT;
	if (!stricmp("te_teleport", name))
		return TELEPORTSPLASH_POINT;

	return P_INVALID;
}

//returns a valid effect if both its existance is known, and it is fully functional
static int PClassic_ParticleTypeForName(char *name)
{
	return P_FindParticleType(name);
}

//a convienience function.
static int PClassic_RunParticleEffectTypeString (vec3_t org, vec3_t dir, float count, char *name)
{
	int efnum = P_FindParticleType(name);
	return P_RunParticleEffectState(org, dir, count, efnum, NULL);
}

//DP extension: add particles within a box that look like rain or snow.
static void PClassic_RunParticleWeather(vec3_t minb, vec3_t maxb, vec3_t dir, float count, int colour, char *efname)
{
}

//DP extension: add particles within a box.
static void PClassic_RunParticleCube(vec3_t minb, vec3_t maxb, vec3_t dir, float count, int colour, qboolean gravity, float jitter)
{
}

//hexen2 support: add particles flying out from a point with a randomized speed
static void PClassic_RunParticleEffect2 (vec3_t org, vec3_t dmin, vec3_t dmax, int color, int effect, int count)
{
}

//hexen2 support: add particles within a box.
static void PClassic_RunParticleEffect3 (vec3_t org, vec3_t box, int color, int effect, int count)
{
}

//hexen2 support: add particles around the spot in a radius. no idea what the 'effect' field is.
static void PClassic_RunParticleEffect4 (vec3_t org, float radius, int color, int effect, int count)
{
}

//this function is used as a fallback in case a trail effect is unknown.
static void PClassic_ParticleTrailIndex (vec3_t start, vec3_t end, int color, int crnd, trailstate_t **tsk)
{
}

//this function is called to tell the particle system about surfaces that might emit particles at map startup.
static void PClassic_EmitSkyEffectTris(model_t *mod, msurface_t *fa, int ptype)
{
}

//the one-time initialisation function, called no mater which renderer is active.
static qboolean PClassic_InitParticles (void)
{
	int i;

	if ((i = COM_CheckParm ("-particles")) && i + 1 < com_argc)
	{
		r_numparticles = (int) (Q_atoi(com_argv[i + 1]));
		r_numparticles = bound(ABSOLUTE_MIN_PARTICLES, r_numparticles, ABSOLUTE_MAX_PARTICLES);
	}
	else
	{
		r_numparticles = DEFAULT_NUM_PARTICLES;
	}

	particles = (cparticle_t *) BZ_Malloc (r_numparticles * sizeof(cparticle_t));

	for (i = 0; i < BUFFERVERTS; i += 3)
	{
		classictexcoords[i+1][0] = 1;
		classictexcoords[i+2][1] = 1;

		classicindexes[i+0] = i+0;
		classicindexes[i+1] = i+1;
		classicindexes[i+2] = i+2;
	}
	classicmesh.xyz_array = classicverts;
	classicmesh.st_array = classictexcoords;
	classicmesh.colors4b_array = (byte_vec4_t*)classiccolours;
	classicmesh.indexes = classicindexes;
	classicshader = R_RegisterShader("particles_classic",
		"{\n"
			"nomipmaps\n"
			"{\n"
				"map $diffuse\n"
				"rgbgen vertex\n"
				"alphagen vertex\n"
				"blendfunc blend\n"
			"}\n"
		"}\n"
		);
	classicshader->defaulttextures.base = particlecqtexture;

	return true;
}

static void PClassic_ShutdownParticles(void)
{
	BZ_Free(particles);
	particles = NULL;
}

// a classic trailstate is really just a float stored in a pointer variable...
// assuming float alignment/size is more strict than pointer
static float Classic_GetLeftover(trailstate_t **tsk)
{
	float *f = (float *)tsk;

	if (!f)
		return 0;

	return *f;
}

static void Classic_SetLeftover(trailstate_t **tsk, float leftover)
{
	float *f = (float *)tsk;

	if (f)
		*f = leftover;
}

//called when an entity is removed from the world, taking its trailstate with it.
static void PClassic_DelinkTrailstate(trailstate_t **tsk)
{
	*tsk = NULL;
}

//wipes all the particles ready for the next map.
static void PClassic_ClearParticles (void)
{
	int		i;
	
	free_particles = &particles[0];
	active_particles = NULL;

	for (i = 0;i < r_numparticles; i++)
		particles[i].next = &particles[i+1];
	particles[r_numparticles - 1].next = NULL;
}

//draws all the active particles.
static void PClassic_DrawParticles(void)
{
	cparticle_t *p, *kill;
	int i;
	float time2, time3, time1, dvel, frametime, grav;
	vec3_t up, right;
	float dist, scale, r_partscale=0;
	union c usecolours;
	unsigned int *palette;
	RSpeedMark();

/*#ifdef D3DQUAKE
	if (qrenderer == QR_DIRECT3D)
		palette = d_8to24bgrtable;
	else
#endif*/
		palette = d_8to24rgbtable;

	//make sure all ents are pushed through first
	RQ_RenderBatchClear();

	if (!active_particles)
	{
		return;
	}

	r_partscale = 0.004 * tan (r_refdef.fov_x * (M_PI / 180) * 0.5f);
	VectorScale (vup, 1.5, up);
	VectorScale (vright, 1.5, right);

	frametime = host_frametime;
	if (cl.paused || r_secondaryview || r_refdef.recurse)
		frametime = 0;
	time3 = frametime * 15;
	time2 = frametime * 10; // 15;
	time1 = frametime * 5;
	grav = frametime * 800 * 0.05;
	dvel = 4 * frametime;

	while(1)
	{
		kill = active_particles;
		if (kill && kill->die < cl.time)
		{
			active_particles = kill->next;
			kill->next = free_particles;
			free_particles = kill;
			continue;
		}
		break;
	}

	for (p = active_particles; p ; p = p->next)
	{
		while (1)
		{
			kill = p->next;
			if (kill && kill->die < cl.time)
			{
				p->next = kill->next;
				kill->next = free_particles;
				free_particles = kill;
				continue;
			}
			break;
		}

		if (classicmesh.numvertexes >= BUFFERVERTS-3)
		{
			classicmesh.numindexes = classicmesh.numvertexes;
			BE_DrawMesh_Single(classicshader, &classicmesh, NULL, &classicshader->defaulttextures, 0);
			classicmesh.numvertexes = 0;
		}

		// hack a scale up to keep particles from disapearing
		dist = (p->org[0] - r_origin[0]) * vpn[0] + (p->org[1] - r_origin[1]) * vpn[1] + (p->org[2] - r_origin[2]) * vpn[2];
		scale = 1 + dist * r_partscale;


		usecolours.i = p->rgb;
		if (p->type == pt_fire)
			usecolours.b[3] = 255 * (6 - p->ramp) / 6;
		else
			usecolours.b[3] = 255;
		classiccolours[classicmesh.numvertexes].i = usecolours.i;
		VectorCopy(p->org, classicverts[classicmesh.numvertexes]);
		classicmesh.numvertexes++;
		classiccolours[classicmesh.numvertexes].i = usecolours.i;
		VectorMA(p->org, scale, up, classicverts[classicmesh.numvertexes]);
		classicmesh.numvertexes++;
		classiccolours[classicmesh.numvertexes].i = usecolours.i;
		VectorMA(p->org, scale, right, classicverts[classicmesh.numvertexes]);
		classicmesh.numvertexes++;




		p->org[0] += p->vel[0] * frametime;
		p->org[1] += p->vel[1] * frametime;
		p->org[2] += p->vel[2] * frametime;
		
		switch (p->type)
		{
		case pt_static:
			break;
		case pt_fire:
			p->ramp += time1;
			if (p->ramp >= 6)
				p->die = -1;
			else
				p->rgb = qpal(ramp3[(int) p->ramp]);
			p->vel[2] += grav;
			break;
		case pt_explode:
			p->ramp += time2;
			if (p->ramp >=8)
				p->die = -1;
			else
				p->rgb = qpal(ramp1[(int) p->ramp]);
			for (i = 0; i < 3; i++)
				p->vel[i] += p->vel[i] * dvel;
			p->vel[2] -= grav*10;
			break;
		case pt_explode2:
			p->ramp += time3;
			if (p->ramp >=8)
				p->die = -1;
			else
				p->rgb = qpal(ramp2[(int) p->ramp]);
			for (i = 0; i < 3; i++)
				p->vel[i] -= p->vel[i] * frametime;
			p->vel[2] -= grav*10;
			break;
		case pt_blob:
			for (i = 0; i < 3; i++)
				p->vel[i] += p->vel[i] * dvel;
			p->vel[2] -= grav;
			break;
		case pt_blob2:
			for (i = 0; i < 2; i++)
				p->vel[i] -= p->vel[i] * dvel;
			p->vel[2] -= grav;
			break;
		case pt_slowgrav:
		case pt_grav:
			p->vel[2] -= grav;
			break;
		}
	}

	if (classicmesh.numvertexes)
	{
		classicmesh.numindexes = classicmesh.numvertexes;
		BE_DrawMesh_Single(classicshader, &classicmesh, NULL, &classicshader->defaulttextures, 0);
		classicmesh.numvertexes = 0;
	}

	RSpeedEnd(RSPEED_PARTICLESDRAW);
}


static void Classic_ParticleExplosion (vec3_t org)
{
	int	i, j;
	cparticle_t	*p;
	
	for (i = 0; i < 1024; i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = cl.time + 5;
		p->rgb = d_8to24rgbtable[ramp1[0]];
		p->ramp = rand() & 3;
		if (i & 1)
		{
			p->type = pt_explode;
			for (j = 0; j < 3; j++)
			{
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
			}
		}
		else
		{
			p->type = pt_explode2;
			for (j = 0; j < 3; j++)
			{
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand()%512) - 256;
			}
		}
	}
}

static void Classic_BlobExplosion (vec3_t org)
{
	int i, j;
	cparticle_t *p;
	
	for (i = 0; i < 1024; i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = cl.time + 1 + (rand() & 8) * 0.05;

		if (i & 1)
		{
			p->type = pt_blob;
			p->rgb = d_8to24rgbtable[66 + rand() % 6];
			for (j = 0; j < 3; j++)
			{
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
			}
		}
		else
		{
			p->type = pt_blob2;
			p->rgb = d_8to24rgbtable[150 + rand() % 6];
			for (j = 0; j < 3; j++)
			{
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
			}
		}
	}
}

static void Classic_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
	int i, j, scale;
	cparticle_t *p;

	if (!dir)
		dir = vec3_origin;

	scale = (count > 130) ? 3 : (count > 20) ? 2  : 1;

	for (i = 0; i < count; i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = cl.time + 0.1 * (rand() % 5);
		p->rgb = d_8to24rgbtable[(color & ~7) + (rand() & 7)];
		p->type = pt_grav;
		for (j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + scale * ((rand() & 15) - 8);
			p->vel[j] = dir[j] * 15;
		}
	}
}

static void Classic_LavaSplash (vec3_t org)
{
	int i, j, k;
	cparticle_t *p;
	float vel;
	vec3_t dir;

	for (i = -16; i < 16; i++)
	{
		for (j = -16; j < 16; j++)
		{
			for (k = 0; k < 1; k++)
			{
				if (!free_particles)
					return;
				p = free_particles;
				free_particles = p->next;
				p->next = active_particles;
				active_particles = p;

				p->die = cl.time + 2 + (rand() & 31) * 0.02;
				p->rgb = d_8to24rgbtable[224 + (rand() & 7)];
				p->type = pt_grav;

				dir[0] = j * 8 + (rand() & 7);
				dir[1] = i * 8 + (rand() & 7);
				dir[2] = 256;

				p->org[0] = org[0] + dir[0];
				p->org[1] = org[1] + dir[1];
				p->org[2] = org[2] + (rand() & 63);

				VectorNormalizeFast (dir);
				vel = 50 + (rand() & 63);
				VectorScale (dir, vel, p->vel);
			}
		}
	}
}

static void Classic_TeleportSplash (vec3_t org)
{
	int i, j, k;
	cparticle_t *p;
	float vel;
	vec3_t dir;

	for (i = -16; i < 16; i += 4)
	{
		for (j = -16; j < 16; j += 4)
		{
			for (k = -24; k < 32; k += 4)
			{
				if (!free_particles)
					return;
				p = free_particles;
				free_particles = p->next;
				p->next = active_particles;
				active_particles = p;

				p->die = cl.time + 0.2 + (rand() & 7) * 0.02;
				p->rgb = d_8to24rgbtable[7 + (rand() & 7)];
				p->type = pt_grav;

				dir[0] = j * 8;
				dir[1] = i * 8;
				dir[2] = k * 8;

				p->org[0] = org[0] + i + (rand() & 3);
				p->org[1] = org[1] + j + (rand() & 3);
				p->org[2] = org[2] + k + (rand() & 3);

				VectorNormalizeFast (dir);
				vel = 50 + (rand() & 63);
				VectorScale (dir, vel, p->vel);
			}
		}
	}
}

static float Classic_ParticleTrail (vec3_t start, vec3_t end, float leftover, effect_type_t type)
{
	vec3_t point, delta, dir;
	float len, rlen, scale;
	int i, j, num_particles;
	cparticle_t *p;
	static int tracercount;

	VectorCopy (start, point);
	VectorSubtract (end, start, delta);
	if (!(len = VectorLength (delta)))
		goto done;
	VectorScale(delta, 1 / len, dir);	//unit vector in direction of trail

	len += leftover;
	rlen = len;

	switch (type)
	{
	case ALT_ROCKET_TRAIL:
		scale = 1.5; break;
	case BLOOD_TRAIL:
		scale = 6; break;
	default:
		scale = 3; break;
	}

	leftover = scale - leftover;
	VectorMA(point, leftover, delta, point);

	len /= scale;
	leftover = rlen - ((int)(len) * scale);

	if (!(num_particles = (int) len))
		goto done;

	VectorScale (delta, scale, delta);

	for (i = 0; i < num_particles && free_particles; i++)
	{
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorClear (p->vel);
		p->die = cl.time + 2;

		switch(type)
		{		
		case GRENADE_TRAIL:
			p->ramp = (rand() & 3) + 2;
			p->rgb = d_8to24rgbtable[ramp3[(int) p->ramp]];
			p->type = pt_fire;
			for (j = 0; j < 3; j++)
				p->org[j] = point[j] + ((rand() % 6) - 3);
			break;
		case BLOOD_TRAIL:
			p->type = pt_slowgrav;
			p->rgb = d_8to24rgbtable[67 + (rand() & 3)];
			for (j = 0; j < 3; j++)
				p->org[j] = point[j] + ((rand() % 6) - 3);
			break;
		case BIG_BLOOD_TRAIL:
			p->type = pt_slowgrav;
			p->rgb = d_8to24rgbtable[67 + (rand() & 3)];
			for (j = 0; j < 3; j++)
				p->org[j] = point[j] + ((rand() % 6) - 3);
			break;
		case TRACER1_TRAIL:
		case TRACER2_TRAIL:
			p->die = cl.time + 0.5;
			p->type = pt_static;
			if (type == TRACER1_TRAIL)
				p->rgb = d_8to24rgbtable[52 + ((tracercount & 4) << 1)];
			else
				p->rgb = d_8to24rgbtable[230 + ((tracercount & 4) << 1)];

			tracercount++;

			VectorCopy (point, p->org);
			if (tracercount & 1)
			{
				p->vel[0] = 90 * dir[1];
				p->vel[1] = 90 * -dir[0];
			}
			else
			{
				p->vel[0] = 90 * -dir[1];
				p->vel[1] = 90 * dir[0];
			}
			break;
		case VOOR_TRAIL:
			p->rgb = d_8to24rgbtable[9 * 16 + 8 + (rand() & 3)];
			p->type = pt_static;
			p->die = cl.time + 0.3;
			for (j = 0; j < 3; j++)
				p->org[j] = point[j] + ((rand() & 15) - 8);
			break;
		case ALT_ROCKET_TRAIL:
			p->ramp = (rand() & 3);
			p->rgb = d_8to24rgbtable[ramp3[(int) p->ramp]];
			p->type = pt_fire;
			for (j = 0; j < 3; j++)
				p->org[j] = point[j] + ((rand() % 6) - 3);
			break;
		case ROCKET_TRAIL:
		default:		
			p->ramp = (rand() & 3);
			p->rgb = d_8to24rgbtable[ramp3[(int) p->ramp]];
			p->type = pt_fire;
			for (j = 0; j < 3; j++)
				p->org[j] = point[j] + ((rand() % 6) - 3);
			break;
		}
		VectorAdd (point, delta, point);
	}
done:
	return leftover;
}



//builds a trail from here to there. The trail state can be used to remember how far you got last frame.
static int PClassic_ParticleTrail (vec3_t startpos, vec3_t end, int type, trailstate_t **tsk)
{
	float leftover;

	if (type == P_INVALID)
		return 1;

	leftover = Classic_ParticleTrail(startpos, end, Classic_GetLeftover(tsk), type);
	Classic_SetLeftover(tsk, leftover);
	return 0;
}

//svc_tempentity support: this is the function that handles 'special' point effects.
//use the trail state so fast/slow frames keep the correct particle counts on certain every-frame effects
static int PClassic_RunParticleEffectState (vec3_t org, vec3_t dir, float count, int typenum, trailstate_t **tsk)
{
	switch(typenum)
	{
	case BLOBEXPLOSION_POINT:
		Classic_BlobExplosion(org);
		break;
	case LAVASPLASH_POINT:
		Classic_LavaSplash(org);
		break;
	case EXPLOSION_POINT:
		Classic_ParticleExplosion(org);
		break;
	case TELEPORTSPLASH_POINT:
		Classic_TeleportSplash(org);
		break;
	default:
		return 1;
	}
	return 0;
}

//svc_particle support: add X particles with the given colour, velocity, and aproximate origin.
static void PClassic_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
	Classic_RunParticleEffect(org, dir, color, count);
}


particleengine_t pe_classic =
{
	"Classic",
	NULL,

	PClassic_ParticleTypeForName,
	PClassic_FindParticleType,

	PClassic_RunParticleEffectTypeString,
	PClassic_ParticleTrail,
	PClassic_RunParticleEffectState,
	PClassic_RunParticleWeather,
	PClassic_RunParticleCube,
	PClassic_RunParticleEffect,
	PClassic_RunParticleEffect2,
	PClassic_RunParticleEffect3,
	PClassic_RunParticleEffect4,

	PClassic_ParticleTrailIndex,
	PClassic_EmitSkyEffectTris,
	PClassic_InitParticles,
	PClassic_ShutdownParticles,
	PClassic_DelinkTrailstate,
	PClassic_ClearParticles,
	PClassic_DrawParticles
};

#endif
