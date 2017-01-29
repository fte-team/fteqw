#include "quakedef.h"

#ifdef RUNTIMELIGHTING

typedef struct mentity_s {
	vec3_t origin;
	float light;
	float angle;
	float cone;
	int style;
	vec3_t colour;
	char classname[64];
	char target[64];
	char targetname[64];

	int targetentnum;
} mentity_t;

struct relight_ctx_s
{
	unsigned int nummodels;
	model_t *models[2048];

	qboolean shadows;
	mentity_t *entities;
	unsigned int num_entities;
	unsigned int max_entities;
};


#define bsptexinfo(i) (*i)
#define dsurfedges lightmodel->surfedges
#define dvertexes lightmodel->vertexes
#define dedges lightmodel->edges
#define texinfo_t mtexinfo_t
#define Q_PI M_PI

#define dfaces lightmodel->surfaces
#define dplanes lightmodel->planes
#define dface_t msurface_t
#define dvertex_t mvertex_t

#define side flags & SURF_PLANEBACK

#define scaledist 1
#define rangescale 0.5
#define extrasamples 0
#define scalecos 0.5


#define bsp_origin vec3_origin

/*
============
CastRay

Returns the distance between the points, or -1 if blocked
=============
*/
static vec_t CastRay (struct relight_ctx_s *ctx, vec3_t p1, vec3_t p2)
{
	trace_t	trace;
	vec3_t move;

	if (ctx->shadows)
	{
		ctx->models[0]->funcs.NativeTrace (ctx->models[0], 0, NULLFRAMESTATE, NULL, p1, p2, vec3_origin, vec3_origin, false, FTECONTENTS_SOLID, &trace);
		if (trace.fraction < 1)
			return -1;
	}

	VectorSubtract(p1, p2, move);
	return VectorLength(move);
}




static void ParseEpair (mentity_t *mapent, char *key, char *value)
{
	double vec[3];

	if (!strcmp(key, "classname"))
		strcpy(mapent->classname, value);

	else if (!strcmp(key, "target"))
		strcpy(mapent->target, value);

	else if (!strcmp(key, "targetname"))
		strcpy(mapent->targetname, value);

	else if (!strcmp(key, "light") || !strcmp(key, "_light"))
		mapent->light = atoi(value);

	else if (!strcmp(key, "style") || !strcmp(key, "_style"))
		mapent->style = atoi(value);

	else if (!strcmp(key, "angle") || !strcmp(key, "_angle"))
		mapent->angle = atof(value);

	else if (!strcmp(key, "cone") || !strcmp(key, "_cone"))
		mapent->cone = atof(value);

	else if (!strcmp(key, "origin"))
	{
		sscanf (value, "%lf %lf %lf", &vec[0], &vec[1], &vec[2]);
		mapent->origin[0]=vec[0];
		mapent->origin[1]=vec[1];
		mapent->origin[2]=vec[2];
	}

	else if (!strcmp(key, "colour") || !strcmp(key, "color") || !strcmp(key, "_colour") || !strcmp(key, "_color"))
	{
		sscanf (value, "%lf %lf %lf", &vec[0], &vec[1], &vec[2]);
		mapent->colour[0]=vec[0];
		mapent->colour[1]=vec[1];
		mapent->colour[2]=vec[2];
	}
}

void LightShutdown(struct relight_ctx_s *ctx, model_t *mod)
{
	qboolean stillheld = false;
	unsigned int i;
	for (i = 0; i < ctx->nummodels; i++)
	{
		if (ctx->models[i] == mod)
			ctx->models[i] = NULL;
		if (ctx->models[i])
			stillheld = true;
	}
	if (stillheld)
		return;
	Z_Free(ctx->entities);
	Z_Free(ctx);
}
struct relight_ctx_s *LightStartup(struct relight_ctx_s *ctx, model_t *model, qboolean shadows)
{
	if (!ctx)
	{
		ctx = Z_Malloc(sizeof(*ctx));
		ctx->shadows = shadows;
	}
	ctx->models[ctx->nummodels++] = model;
	return ctx;
}
void LightReloadEntities(struct relight_ctx_s *ctx, const char *entstring, qboolean ignorestyles)
{
#define DEFAULTLIGHTLEVEL 300
	mentity_t *mapent;
	char key[1024];
	char value[1024];
	int i;
	int switchedstyle=32;
	ctx->num_entities = 0;

	while(1)
	{
		entstring = COM_ParseOut(entstring, key, sizeof(key));
		if (!entstring || !*key)
			break;
		if (strcmp(key, "{"))
		{	//someone messed up. Stop parsing.
			Con_Printf("token wasn't an open brace\n");
			break;
		}

		if (ctx->num_entities == ctx->max_entities)
		{
			ctx->max_entities = ctx->max_entities + 128;
			ctx->entities = BZ_Realloc(ctx->entities, sizeof(*ctx->entities) * ctx->max_entities);
		}

		mapent = &ctx->entities[ctx->num_entities];
		memset(mapent, 0, sizeof(*mapent));
		mapent->colour[0] = 0;
		mapent->colour[1] = 0;
		mapent->colour[2] = 0;
		mapent->targetentnum = -1;
		while(entstring)
		{
			entstring = COM_ParseOut(entstring, key, sizeof(key));
			if (!strcmp(key, "}"))
				break;
			entstring = COM_ParseOut(entstring, value, sizeof(value));
			ParseEpair(mapent, key, value);
		}
		if (!mapent->colour[0] && !mapent->colour[1] && !mapent->colour[2])
		{
			int cont;
			vec3_t v;
			v[0] = mapent->origin[0];
			v[1] = mapent->origin[1];
			cont=0;
			for (i = 0; i < 256; i+=16)
			{
				v[2] = mapent->origin[2]-i;
				cont = ctx->models[0]->funcs.PointContents (ctx->models[0], NULL, v);
				if (cont & (FTECONTENTS_LAVA | FTECONTENTS_SLIME | FTECONTENTS_SOLID))
					break;
			}			
			if (cont & FTECONTENTS_LAVA)
			{
				mapent->colour[0] = 1;
				mapent->colour[1] = i/256.0;
				mapent->colour[2] = i/256.0;
			}	
			else if (cont & FTECONTENTS_SLIME)
			{
				mapent->colour[0] = 0.5+0.5*i/256.0;
				mapent->colour[1] = 1;				
				mapent->colour[2] = 0.5+0.5*i/256.0;
			}
			else
			{
				if (mapent->style == 9)	//hmm..
				{
					mapent->colour[1] = 1;
				}
				else
				{
					if (!strncmp(mapent->classname, "light_torch_small_walltorch", 12))
					{
						mapent->colour[0] = 1;
						mapent->colour[1] = 0.7;
						mapent->colour[2] = 0.7;
					}
					else
					{
						mapent->colour[0] = 1;
						mapent->colour[1] = 1;
						if (strncmp(mapent->classname, "light_fluoro", 12))
							mapent->colour[2] = 1;
					}
				}
			}
		}

		if (!mapent->light && !strncmp (mapent->classname, "light", 5))
			mapent->light = DEFAULTLIGHTLEVEL;

		if (*mapent->targetname && !mapent->style && !strcmp(mapent->classname, "light"))
		{
			for (i = 0; i <= ctx->num_entities; i++)
			{
				if (ctx->entities[i].style >= 32 && !strcmp(ctx->entities[i].targetname, mapent->targetname))
				{
					mapent->style = ctx->entities[i].style;
					break;
				}
			}

			if (i == ctx->num_entities)
				mapent->style = switchedstyle++;
		}

		if (ignorestyles)
			mapent->style = 0;

		ctx->num_entities++;
	}

	for (mapent = ctx->entities; mapent < &ctx->entities[ctx->num_entities]; mapent++)
	{
		if (*mapent->target)
		{
			for (i = 0; i < ctx->num_entities; i++)
			{
				if (mapent == &ctx->entities[i])
					continue;

				if (!strcmp(mapent->target, ctx->entities[i].targetname))
				{
					mapent->targetentnum = i;
					break;
				}
			}
		}
	}
}

/*
===============================================================================

SAMPLE POINT DETERMINATION

void SetupBlock (dface_t *f) Returns with surfpt[] set

This is a little tricky because the lightmap covers more area than the face.
If done in the straightforward fashion, some of the
sample points will be inside walls or on the other side of walls, causing
false shadows and light bleeds.

To solve this, I only consider a sample point valid if a line can be drawn
between it and the exact midpoint of the face.  If invalid, it is adjusted
towards the center until it is valid.

(this doesn't completely work)

===============================================================================
*/

#define MAXIMUMEXTENT 128
#define	SINGLEMAP	(MAXIMUMEXTENT*MAXIMUMEXTENT*4)

typedef struct llightinfo_s
{
	struct relight_ctx_s *ctx;	//relight context, shared between threads.

	vec3_t	lightmaps[MAXQ1LIGHTMAPS][SINGLEMAP];
	vec3_t	lightnorm[MAXQ1LIGHTMAPS][SINGLEMAP];
	int		numlightstyles;
	vec_t	*light;
	vec_t	facedist;
	vec3_t	facenormal;

	int		numsurfpt;
	vec3_t	surfpt[SINGLEMAP];

	vec3_t	texorg;
	vec3_t	worldtotex[2];	// s = (world - texorg) . worldtotex[0]
	vec3_t	textoworld[2];	// world = texorg + s * textoworld[0]

	vec_t	exactmins[2], exactmaxs[2];
	
	int		texmins[2], texsize[2];
	int		lightstyles[MAXQ1LIGHTMAPS];
} llightinfo_t;

const size_t lightthreadctxsize = sizeof(llightinfo_t);


/*
================
CalcFaceVectors

Fills in texorg, worldtotex. and textoworld
================
*/
static void LightCalcFaceVectors (llightinfo_t *l, vec4_t surf_texplanes[2])
{
	int			i, j;
	vec3_t	texnormal;
	float	distscale;
	vec_t	dist, len;
	
// convert from float to vec_t
	for (i=0 ; i<2 ; i++)
		for (j=0 ; j<3 ; j++)
			l->worldtotex[i][j] = surf_texplanes[i][j];

// calculate a normal to the texture axis.  points can be moved along this
// without changing their S/T
	texnormal[0] =	surf_texplanes[1][1]*surf_texplanes[0][2]
				  - surf_texplanes[1][2]*surf_texplanes[0][1];
	texnormal[1] =	surf_texplanes[1][2]*surf_texplanes[0][0]
				  - surf_texplanes[1][0]*surf_texplanes[0][2];
	texnormal[2] =	surf_texplanes[1][0]*surf_texplanes[0][1]
				  - surf_texplanes[1][1]*surf_texplanes[0][0];
	VectorNormalize (texnormal);

// flip it towards plane normal
	distscale = DotProduct (texnormal, l->facenormal);
	if (!distscale)
	{
		VectorCopy(l->facenormal, texnormal);
		distscale = 1;
		Con_Printf ("Texture axis perpendicular to face\n");
	}
	if (distscale < 0)
	{
		distscale = -distscale;
		VectorNegate (texnormal, texnormal);
	}	

// distscale is the ratio of the distance along the texture normal to
// the distance along the plane normal
	distscale = 1/distscale;

	for (i=0 ; i<2 ; i++)
	{
		len = VectorLength (l->worldtotex[i]);
		dist = DotProduct (l->worldtotex[i], l->facenormal);
		dist *= distscale;
		VectorMA (l->worldtotex[i], -dist, texnormal, l->textoworld[i]);
		VectorScale (l->textoworld[i], (1/len)*(1/len), l->textoworld[i]);
	}


// calculate texorg on the texture plane
	for (i=0 ; i<3 ; i++)
		l->texorg[i] = -surf_texplanes[0][3]* l->textoworld[0][i] - surf_texplanes[1][3] * l->textoworld[1][i];

// project back to the face plane
	dist = DotProduct (l->texorg, l->facenormal) - l->facedist - 1;
	dist *= distscale;
	VectorMA (l->texorg, -dist, texnormal, l->texorg);
	
}

/*
================
CalcFaceExtents

Fills in s->texmins[] and s->texsize[]
also sets exactmins[] and exactmaxs[]
================
*/
static void LightCalcFaceExtents (model_t *lightmodel, dface_t *s, vec2_t exactmins, vec2_t exactmaxs, int texmins[2], int texsize[2])
{
	vec_t	mins[2], maxs[2], val;
	int		i,j, e;
	dvertex_t	*v;
	texinfo_t	*tex;

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -999999;

	tex = &bsptexinfo(s->texinfo);
	
	for (i=0 ; i<s->numedges ; i++)
	{
		e = dsurfedges[s->firstedge+i];
		if (e >= 0)
			v = dvertexes + dedges[e].v[0];
		else
			v = dvertexes + dedges[-e].v[1];
		
		for (j=0 ; j<2 ; j++)
		{
			val = v->position[0] * tex->vecs[j][0] + 
				v->position[1] * tex->vecs[j][1] +
				v->position[2] * tex->vecs[j][2] +
				tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{	
		exactmins[i] = mins[i];
		exactmaxs[i] = maxs[i];
		
		mins[i] = floor(mins[i]/(1<<s->lmshift));
		maxs[i] = ceil(maxs[i]/(1<<s->lmshift));

		texmins[i] = mins[i];
		texsize[i] = maxs[i] - mins[i];
		if (texsize[i] > MAXIMUMEXTENT-1)
		{
			texsize[i] = MAXIMUMEXTENT-1;
			Con_Printf("Bad surface extents");
		}
	}
}

/*
=================
CalcPoints

For each texture aligned grid point, back project onto the plane
to get the world xyz value of the sample point
=================
*/
static void LightCalcPoints (llightinfo_t *l, float lmscale)
{
	int		i;
	int		s, t, j;
	int		w, h;
	vec_t	step;
	vec_t	starts, startt, us, ut;
	vec_t	*surf;
	vec_t	mids, midt;
	vec3_t	facemid, move;

//
// fill in surforg
// the points are biased towards the center of the surface
// to help avoid edge cases just inside walls
//
	surf = l->surfpt[0];
	mids = (l->exactmaxs[0] + l->exactmins[0])/2;
	midt = (l->exactmaxs[1] + l->exactmins[1])/2;

	for (j=0 ; j<3 ; j++)
		facemid[j] = l->texorg[j] + l->textoworld[0][j]*mids + l->textoworld[1][j]*midt;

	if (extrasamples)
	{	// extra filtering
		h = (l->texsize[1]+1)*2;
		w = (l->texsize[0]+1)*2;
		starts = (l->texmins[0]-0.5)*lmscale;
		startt = (l->texmins[1]-0.5)*lmscale;
		step = 0.5 * lmscale;
	}
	else
	{
		h = l->texsize[1]+1;
		w = l->texsize[0]+1;
		starts = l->texmins[0]*lmscale;
		startt = l->texmins[1]*lmscale;
		step = lmscale;
	}

	l->numsurfpt = w * h;
	for (t=0 ; t<h ; t++)
	{
		for (s=0 ; s<w ; s++, surf+=3)
		{
			us = starts + s*step;
			ut = startt + t*step;

		// if a line can be traced from surf to facemid, the point is good
			for (i=0 ; i<6 ; i++)
			{
			// calculate texture point
				for (j=0 ; j<3 ; j++)
					surf[j] = l->texorg[j] + l->textoworld[0][j]*us
					+ l->textoworld[1][j]*ut;

				if (CastRay (l->ctx, facemid, surf) != -1)
					break;	// got it
				if (i & 1)
				{
					if (us > mids)
					{
						us -= lmscale*0.5;
						if (us < mids)
							us = mids;
					}
					else
					{
						us += lmscale*0.5;
						if (us > mids)
							us = mids;
					}
				}
				else
				{
					if (ut > midt)
					{
						ut -= lmscale*0.5;
						if (ut < midt)
							ut = midt;
					}
					else
					{
						ut += lmscale*0.5;
						if (ut > midt)
							ut = midt;
					}
				}
				
				// move surf 8 pixels towards the center
				VectorSubtract (facemid, surf, move);
				VectorNormalize (move);
				VectorMA (surf, 8, move, surf);
			}
		}
	}
	
}


/*
===============================================================================

FACE LIGHTING

===============================================================================
*/

/*
================
SingleLightFace
================
*/
static void SingleLightFace (mentity_t *light, llightinfo_t *l)
{
	vec_t	dist;
	vec3_t	incoming;
	vec_t	angle;
	vec_t	add;
	vec_t	*surf;
	qboolean	hit;
	int		mapnum;
	int		c;
	vec3_t	rel;
	vec3_t	spotvec;
	vec_t	falloff;
	vec3_t	*lightsamp;
	vec3_t	*norms;
	
	VectorSubtract (light->origin, bsp_origin, rel);
	dist = scaledist * (DotProduct (rel, l->facenormal) - l->facedist);
	
// don't bother with lights behind the surface
	if (dist <= 0)
		return;
		
// don't bother with light too far away
	if (dist > light->light)
		return;

	if (light->targetentnum>=0)
	{
		VectorSubtract (l->ctx->entities[light->targetentnum].origin, light->origin, spotvec);
		VectorNormalize (spotvec);
		if (!light->angle)
			falloff = -cos(20*Q_PI/180);	
		else
			falloff = -cos(light->angle/2*Q_PI/180);
	}
	else
		falloff = 0;	// shut up compiler warnings
	
	mapnum = 0;
	for (mapnum=0 ; mapnum<l->numlightstyles ; mapnum++)
		if (l->lightstyles[mapnum] == light->style)
			break;
	
	lightsamp = l->lightmaps[mapnum];
	norms = l->lightnorm[mapnum];
	if (mapnum == l->numlightstyles)
	{	// init a new light map
#ifdef UTILITY
		if (mapnum == MAXQ1LIGHTMAPS)
		{
			printf ("WARNING: Too many light styles on a face\n");
			return;
		}
		size = (l->texsize[1]+1)*(l->texsize[0]+1);
		for (i=0 ; i<size ; i++)
		{
			lightsamp[i][0] = 0;
			lightsamp[i][1] = 0;
			lightsamp[i][2] = 0;
			norms[i][0] = 0;
			norms[i][1] = 0;
			norms[i][2] = 0;
		}
#else
		return;	//can't light a surface with a lightstyle that did not previously exist, due to lightmap space limits.
#endif
	}

//
// check it for real
//
	hit = false;
	
	surf = l->surfpt[0];
	for (c=0 ; c<l->numsurfpt ; c++, surf+=3)
	{
		dist = CastRay(l->ctx, light->origin, surf)*scaledist;
		if (dist < 0)
			continue;	// light doesn't reach

		VectorSubtract (light->origin, surf, incoming);
		VectorNormalize (incoming);
		if (light->targetentnum >= 0)
		{	// spotlight cutoff
			if (DotProduct (spotvec, incoming) > falloff)
				continue;
		}
		angle = DotProduct (incoming, l->facenormal);

		angle = (1.0-scalecos) + scalecos*angle;
		add = light->light - dist;
		add *= angle;
		if (add < 0)
			continue;

		lightsamp[c][0] += add*light->colour[0];
		lightsamp[c][1] += add*light->colour[1];
		lightsamp[c][2] += add*light->colour[2];

		norms[c][0] += add * incoming[0];
		norms[c][1] += add * incoming[1];
		norms[c][2] += add * incoming[2];

		if (add > 1)		// ignore real tiny lights
			hit = true;
	}

	if (mapnum == l->numlightstyles && hit)
	{
		l->lightstyles[mapnum] = light->style;
		l->numlightstyles++;	// the style has some real data now
	}
}

/*
============
FixMinlight
============
*/
static void FixMinlight (llightinfo_t *l)
{
	int		i, j;
	float	minlight;
	
	minlight = 0;

// if minlight is set, there must be a style 0 light map
	if (!minlight)
		return;
	
	for (i=0 ; i< l->numlightstyles ; i++)
	{
		if (l->lightstyles[i] == 0)
			break;
	}
	if (i == l->numlightstyles)
	{
		if (l->numlightstyles == MAXQ1LIGHTMAPS)
			return;		// oh well..
		for (j=0 ; j<l->numsurfpt ; j++)
		{
			l->lightmaps[i][j][0] = minlight;
			l->lightmaps[i][j][1] = minlight;
			l->lightmaps[i][j][2] = minlight;
		}
		l->lightstyles[i] = 0;
		l->numlightstyles++;
	}
	else
	{
		for (j=0 ; j<l->numsurfpt ; j++)
		{
			if ( l->lightmaps[i][j][0] < minlight)
				l->lightmaps[i][j][0] = minlight;
			if ( l->lightmaps[i][j][1] < minlight)
				l->lightmaps[i][j][1] = minlight;
			if ( l->lightmaps[i][j][2] < minlight)
				l->lightmaps[i][j][2] = minlight;
		}
	}
}


/*
============
LightFace
============
*/
void LightPlane (struct relight_ctx_s *ctx, struct llightinfo_s *l, qbyte surf_styles[MAXQ1LIGHTMAPS], qbyte *surf_rgbsamples, qbyte *surf_deluxesamples, vec4_t surf_plane, vec4_t surf_texplanes[2], vec2_t exactmins, vec2_t exactmaxs, int texmins[2], int texsize[2], float lmscale)
{
	int		s, t;
	int		i,c,ch;
	vec_t	total, mean;
	int		size;
	int		lightmapwidth;
#ifdef UTILITY
	int		lightmapsize;
	byte	*out;
#endif
	qbyte	*rgbout;
	qbyte	*dulout;
	vec3_t	*light, *norm;
	vec3_t	wnorm, temp, svector, tvector;
	int		w;
	

//
// some surfaces don't need lightmaps
//	
	if (!surf_rgbsamples)
		return;

//	memset (l, 0, sizeof(*l));
	l->ctx = ctx;

//
// rotate plane
//
	VectorCopy (surf_plane, l->facenormal);
	l->facedist = surf_plane[3];

	LightCalcFaceVectors (l, surf_texplanes);
	Vector2Copy(exactmins, l->exactmins);
	Vector2Copy(exactmaxs, l->exactmaxs);
	Vector2Copy(texmins, l->texmins);
	Vector2Copy(texsize, l->texsize);
	LightCalcPoints (l, lmscale);

	lightmapwidth = l->texsize[0]+1;

	size = lightmapwidth*(l->texsize[1]+1);
	if (size > SINGLEMAP)
		Host_Error ("Bad lightmap size");

	i = 0;
#ifndef UTILITY
	for (; surf_styles[i] != 255 && i < MAXQ1LIGHTMAPS; i++)
	{
		l->lightstyles[i] = surf_styles[i];
		memset(&l->lightmaps[i], 0, sizeof(l->lightmaps[i][0])*l->numsurfpt);
		memset(&l->lightnorm[i], 0, sizeof(l->lightnorm[i][0])*l->numsurfpt);
	}
#endif
	l->numlightstyles = i;
	for ( ; i<MAXQ1LIGHTMAPS ; i++)
		l->lightstyles[i] = 255;
	
//
// cast all lights
//	
	for (i=0 ; i<ctx->num_entities ; i++)
	{
		if (ctx->entities[i].light)
			SingleLightFace (&ctx->entities[i], l);
	}

	FixMinlight (l);
		
	if (!l->numlightstyles)
	{	// no light hitting it
#ifdef UTILITY
		f->lightofs = -1;
#endif
		return;
	}

//
// save out the values
//
	for (i=0 ; i <MAXQ1LIGHTMAPS ; i++)
		surf_styles[i] = l->lightstyles[i];


#ifdef UTILITY
	lightmapsize = size*l->numlightstyles;
	if (runningrgblightdatabase)
	{
		out = GetFakeFileSpace(&f->lightofs, lightmapsize);
		rgbout = runningrgblightdatabase + f->lightofs*3;
		dulout = runninglightnormbase + f->lightofs*3;
	}
	else
	{
		out = GetFileSpace (&f->lightofs, lightmapsize);

		rgbout = GetRGBFileSpace (f->lightofs, lightmapsize);
		dulout = GetNormFileSpace (f->lightofs, lightmapsize);
	}
#else
	rgbout = surf_rgbsamples;
	if (l->ctx->models[0]->deluxdata)
	{
		dulout = surf_deluxesamples;

		VectorCopy(surf_texplanes[0], svector);
		VectorNegate(surf_texplanes[1], tvector);
		VectorNormalize(svector);
		VectorNormalize(tvector);
	}
	else
		dulout = NULL;
#endif


	
// extra filtering
//	h = (l.texsize[1]+1)*2;
	w = l->texsize[0]+1;
	if (extrasamples)
		w *= 2;

	for (i=0 ; i< l->numlightstyles ; i++)
	{
		if (l->lightstyles[i] == 0xff)
			Host_Error ("Wrote empty lightmap");
		light = l->lightmaps[i];
		norm = l->lightnorm[i];
		c = 0;
		for (t=0 ; t<=l->texsize[1] ; t++)
		{
			for (s=0 ; s<=l->texsize[0] ; s++, c++)
			{
				mean = 0;

				for (ch = 0; ch < 3; ch++)
				{
					if (extrasamples)
					{	// filtered sample
						total = light[t*2*w+s*2][ch] + light[t*2*w+s*2+1][ch]
						+ light[(t*2+1)*w+s*2][ch] + light[(t*2+1)*w+s*2+1][ch];
						total *= 0.25;

						wnorm[ch] = norm[t*2*w+s*2][ch] + norm[t*2*w+s*2+1][ch]
						+ norm[(t*2+1)*w+s*2][ch] + norm[(t*2+1)*w+s*2+1][ch];
					}
					else
					{
						total = light[c][ch];
						wnorm[ch] = norm[c][ch];
					}
					total *= rangescale;	// scale before clamping
#ifndef UTILITY
//					if (total > *rgbout)	//sorry - for qw
//						total = *rgbout;
#endif
					if (total < 0)
						total = 0;
					if (total > 0xff)
						total = 0xff;
					
					*rgbout++ = total;
					mean += total;
				}
#ifdef UTILITY
				*out++ = mean/3;
#endif

				if (dulout)
				{
					temp[0] = DotProduct(wnorm, svector);
					temp[1] = DotProduct(wnorm, tvector);
					temp[2] = DotProduct(wnorm, l->facenormal);
					if (!temp[0] && !temp[1] && !temp[2])
						VectorSet(temp, 0, 0, 1);
					else
						VectorNormalize(temp);
					*dulout++ = (temp[0]+1)*128;
					*dulout++ = (temp[1]+1)*128;
					*dulout++ = (temp[2]+1)*128;
				}
			}
		}
	}
}
void LightFace (struct relight_ctx_s *ctx, struct llightinfo_s *threadctx, int facenum)
{
	dface_t *f = ctx->models[0]->surfaces + facenum;
	vec4_t plane;
	vec2_t exactmins;
	vec2_t exactmaxs;
	int texmins[2], texsize[2];

	VectorCopy (f->plane->normal, plane);
	plane[3] = f->plane->dist;
	if (f->flags & SURF_PLANEBACK)
	{
		VectorNegate (plane, plane);
		plane[3] = -plane[3];
	}

	//no lighting on these.
	if (f->texinfo->flags & TEX_SPECIAL)
		return;

	LightCalcFaceExtents(ctx->models[0], f, exactmins, exactmaxs, texmins, texsize);
	LightPlane(ctx, threadctx, f->styles, f->samples, f->samples - ctx->models[0]->lightdata + ctx->models[0]->deluxdata, plane, f->texinfo->vecs, exactmins, exactmaxs, texmins, texsize, 1<<f->lmshift);
}
#endif
