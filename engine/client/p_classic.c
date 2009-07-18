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

void D_DrawParticleTrans (vec3_t porg, float palpha, float pscale, unsigned int pcolour, blendmode_t blendmode);



cvar_t gl_solidparticles = SCVAR("gl_solidparticles", "0");


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


typedef struct cparticle_s {
	enum {
		pt_static,
		pt_fire,
		pt_explode,
		pt_explode2,
		pt_blob,
		pt_blob2,
		pt_grav,
		pt_slowgrav
	} type;
	float die;
	vec3_t org;
	vec3_t vel;
	float ramp;
	unsigned char color;
	struct cparticle_s *next;
} cparticle_t;

#define DEFAULT_NUM_PARTICLES	2048
#define ABSOLUTE_MIN_PARTICLES	512
#define ABSOLUTE_MAX_PARTICLES	8192
static int r_numparticles;
static cparticle_t	*particles, *active_particles, *free_particles;

static int	ramp1[8] = {0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61};
static int	ramp2[8] = {0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66};
static int	ramp3[8] = {0x6d, 0x6b, 6, 5, 4, 3};


//obtains an index for the name, even if it is unknown (one can be loaded after. will only fail if the effect limit is reached)
//technically this function is not meant to fail often, but thats fine so long as the other functions are meant to safely reject invalid effect numbers.
static int PClassic_ParticleTypeForName(char *name)
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
static int PClassic_FindParticleType(char *name)
{
	return P_ParticleTypeForName(name);
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
static void PClassic_EmitSkyEffectTris(model_t *mod, msurface_t 	*fa)
{
}

//the one-time initialisation function, called no mater which renderer is active.
static void PClassic_InitParticles (void)
{
	int i;
	model_t *mod;
	extern model_t	mod_known[];
	extern int		mod_numknown;

	if ((i = COM_CheckParm ("-particles")) && i + 1 < com_argc)	{
		r_numparticles = (int) (Q_atoi(com_argv[i + 1]));
		r_numparticles = bound(ABSOLUTE_MIN_PARTICLES, r_numparticles, ABSOLUTE_MAX_PARTICLES);
	} else {
		r_numparticles = DEFAULT_NUM_PARTICLES;
	}

	particles = (cparticle_t *) BZ_Malloc (r_numparticles * sizeof(cparticle_t));

	CL_RegisterParticles();

	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		mod->particleeffect = P_INVALID;
		mod->particletrail = P_INVALID;
		mod->engineflags &= ~MDLF_NODEFAULTTRAIL;

		P_DefaultTrail(mod);
	}
}

static void PClassic_ShutdownParticles(void)
{
	BZ_Free(particles);
}

//called when an entity is removed from the world, taking its trailstate with it.
static void PClassic_DelinkTrailstate(trailstate_t **tsk)
{
	//classic has no concept of trail states.
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

#define USEARRAYS

#define BUFFERVERTS 2048*3
vec3_t classicverts[BUFFERVERTS];
union c
{
	byte_vec4_t b;
	unsigned int i;
} classiccolours[BUFFERVERTS];
vec2_t classictexcoords[BUFFERVERTS];
int classicnumverts;
int setuptexcoords;

//draws all the active particles.
static void PClassic_DrawParticles(void)
{
	RSpeedLocals();

	cparticle_t *p, *kill;
	int i;
	float time2, time3, time1, dvel, frametime, grav;
#ifdef RGLQUAKE
#ifndef USEARRAYS
	unsigned char *at, theAlpha;
#endif
	vec3_t up, right;
	float dist, scale, r_partscale=0;

	union c usecolours;
#endif

	if (!active_particles)
	{
		RQ_RenderDistAndClear();
		return;
	}

	switch(qrenderer)
	{
#ifdef RGLQUAKE
	case QR_OPENGL:
		r_partscale = 0.004 * tan (r_refdef.fov_x * (M_PI / 180) * 0.5f);

		GL_Bind(particlecqtexture);

		qglEnable (GL_BLEND);
		if (!gl_solidparticles.value)
			qglDepthMask (GL_FALSE);
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

#ifdef USEARRAYS
		if (!setuptexcoords)
		{
			setuptexcoords = true;
			for (i = 0; i < BUFFERVERTS; i += 3)
			{
				classictexcoords[i+1][0] = 1;
				classictexcoords[i+2][1] = 1;
			}
		}
		qglTexCoordPointer(2, GL_FLOAT, 0, classictexcoords);
		qglVertexPointer(3, GL_FLOAT, 0, classicverts);
		qglColorPointer(4, GL_UNSIGNED_BYTE, 0, classiccolours);

		qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
		qglEnableClientState(GL_COLOR_ARRAY);
		qglEnableClientState(GL_VERTEX_ARRAY);
#else
		qglBegin (GL_TRIANGLES);
#endif

		VectorScale (vup, 1.5, up);
		VectorScale (vright, 1.5, right);

		classicnumverts = 0;
		break;
#endif
	default:
		RQ_RenderDistAndClear();
		return;
	}

	frametime = host_frametime;
	if (cl.paused)
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

		switch(qrenderer)
		{
#ifdef RGLQUAKE
		case QR_OPENGL:
#ifdef USEARRAYS
			if (classicnumverts >= BUFFERVERTS-3)
			{
				qglDrawArrays(GL_TRIANGLES, 0, classicnumverts);
				classicnumverts = 0;
			}
#endif

			// hack a scale up to keep particles from disapearing
			dist = (p->org[0] - r_origin[0]) * vpn[0] + (p->org[1] - r_origin[1]) * vpn[1] + (p->org[2] - r_origin[2]) * vpn[2];
			scale = 1 + dist * r_partscale;

#ifdef USEARRAYS
			usecolours.i = d_8to24rgbtable[(int)p->color];
			if (p->type == pt_fire)
				usecolours.b[3] = 255 * (6 - p->ramp) / 6;
			else
				usecolours.b[3] = 255;

			classiccolours[classicnumverts].i = usecolours.i;
			VectorCopy(p->org, classicverts[classicnumverts]);
			classicnumverts++;
			classiccolours[classicnumverts].i = usecolours.i;
			VectorMA(p->org, scale, up, classicverts[classicnumverts]);
			classicnumverts++;
			classiccolours[classicnumverts].i = usecolours.i;
			VectorMA(p->org, scale, right, classicverts[classicnumverts]);
			classicnumverts++;
#else

			at = (qbyte *) &d_8to24rgbtable[(int)p->color];
			if (p->type == pt_fire)
				theAlpha = 255 * (6 - p->ramp) / 6;
			else
				theAlpha = 255;
			qglColor4ub (*at, *(at + 1), *(at + 2), theAlpha);
			qglTexCoord2f (0, 0); qglVertex3fv (p->org);
			qglTexCoord2f (1, 0); qglVertex3f (p->org[0] + up[0] * scale, p->org[1] + up[1] * scale, p->org[2] + up[2] * scale);
			qglTexCoord2f (0, 1); qglVertex3f (p->org[0] + right[0] * scale, p->org[1] + right[1] * scale, p->org[2] + right[2] * scale);
#endif
			break;
#endif
		}

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
				p->color = ramp3[(int) p->ramp];
			p->vel[2] += grav;
			break;
		case pt_explode:
			p->ramp += time2;
			if (p->ramp >=8)
				p->die = -1;
			else
				p->color = ramp1[(int) p->ramp];
			for (i = 0; i < 3; i++)
				p->vel[i] += p->vel[i] * dvel;
			p->vel[2] -= grav * 30;
			break;
		case pt_explode2:
			p->ramp += time3;
			if (p->ramp >=8)
				p->die = -1;
			else
				p->color = ramp2[(int) p->ramp];
			for (i = 0; i < 3; i++)
				p->vel[i] -= p->vel[i] * frametime;
			p->vel[2] -= grav * 30;
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

	switch(qrenderer)
	{
#ifdef RGLQUAKE
	case QR_OPENGL:
#ifdef USEARRAYS
		if (classicnumverts)
		{
			qglDrawArrays(GL_TRIANGLES, 0, classicnumverts);
			classicnumverts = 0;
		}
#else
		qglEnd ();
#endif
		qglDisable (GL_BLEND);
		qglDepthMask (GL_TRUE);
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		qglColor3ub (255, 255, 255);
		break;
#endif
	default:
		break;
	}





	RSpeedRemark();
	RQ_RenderDistAndClear();
	RSpeedEnd(RSPEED_PARTICLESDRAW);
}

//called to set up the rendering state (opengl)
static void PClassic_FlushRenderer(void)
{
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
		p->color = ramp1[0];
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
			p->color = 66 + rand() % 6;
			for (j = 0; j < 3; j++)
			{
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
			}
		}
		else
		{
			p->type = pt_blob2;
			p->color = 150 + rand() % 6;
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
		p->color = (color & ~7) + (rand() & 7);
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
				p->color = 224 + (rand() & 7);
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
				p->color = 7 + (rand() & 7);
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

static void Classic_ParticleTrail (vec3_t start, vec3_t end, vec3_t *trail_origin, effect_type_t type)
{
	vec3_t point, delta, dir;
	float len;
	int i, j, num_particles;
	cparticle_t *p;
	static int tracercount;

	VectorCopy (start, point);
	VectorSubtract (end, start, delta);
	if (!(len = VectorLength (delta)))
		goto done;
	VectorScale(delta, 1 / len, dir);	//unit vector in direction of trail

	switch (type) {
	case ALT_ROCKET_TRAIL:
		len /= 1.5; break;
	case BLOOD_TRAIL:
		len /= 6; break;
	default:
		len /= 3; break;	
	}

	if (!(num_particles = (int) len))
		goto done;

	VectorScale (delta, 1.0 / num_particles, delta);

	for (i = 0; i < num_particles && free_particles; i++) {
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorClear (p->vel);
		p->die = cl.time + 2;

		switch(type) {		
		case GRENADE_TRAIL:
			p->ramp = (rand() & 3) + 2;
			p->color = ramp3[(int) p->ramp];
			p->type = pt_fire;
			for (j = 0; j < 3; j++)
				p->org[j] = point[j] + ((rand() % 6) - 3);
			break;
		case BLOOD_TRAIL:
			p->type = pt_slowgrav;
			p->color = 67 + (rand() & 3);
			for (j = 0; j < 3; j++)
				p->org[j] = point[j] + ((rand() % 6) - 3);
			break;
		case BIG_BLOOD_TRAIL:
			p->type = pt_slowgrav;
			p->color = 67 + (rand() & 3);
			for (j = 0; j < 3; j++)
				p->org[j] = point[j] + ((rand() % 6) - 3);
			break;
		case TRACER1_TRAIL:
		case TRACER2_TRAIL:
			p->die = cl.time + 0.5;
			p->type = pt_static;
			if (type == TRACER1_TRAIL)
				p->color = 52 + ((tracercount & 4) << 1);
			else
				p->color = 230 + ((tracercount & 4) << 1);

			tracercount++;

			VectorCopy (point, p->org);
			if (tracercount & 1) {
				p->vel[0] = 90 * dir[1];
				p->vel[1] = 90 * -dir[0];
			} else {
				p->vel[0] = 90 * -dir[1];
				p->vel[1] = 90 * dir[0];
			}
			break;
		case VOOR_TRAIL:
			p->color = 9 * 16 + 8 + (rand() & 3);
			p->type = pt_static;
			p->die = cl.time + 0.3;
			for (j = 0; j < 3; j++)
				p->org[j] = point[j] + ((rand() & 15) - 8);
			break;
		case ALT_ROCKET_TRAIL:
			p->ramp = (rand() & 3);
			p->color = ramp3[(int) p->ramp];
			p->type = pt_fire;
			for (j = 0; j < 3; j++)
				p->org[j] = point[j] + ((rand() % 6) - 3);
			break;
		case ROCKET_TRAIL:
		default:		
			p->ramp = (rand() & 3);
			p->color = ramp3[(int) p->ramp];
			p->type = pt_fire;
			for (j = 0; j < 3; j++)
				p->org[j] = point[j] + ((rand() % 6) - 3);
			break;
		}
		VectorAdd (point, delta, point);
	}
done:
	if (trail_origin)
		VectorCopy(point, *trail_origin);
}



//builds a trail from here to there. The trail state can be used to remember how far you got last frame.
static int PClassic_ParticleTrail (vec3_t startpos, vec3_t end, int type, trailstate_t **tsk)
{
	if (type == P_INVALID)
		return 1;

	Classic_ParticleTrail(startpos, end, NULL, type);
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
	PClassic_DrawParticles,
	PClassic_FlushRenderer
};

#endif
