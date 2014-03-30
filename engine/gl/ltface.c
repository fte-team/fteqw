#include "quakedef.h"

#ifdef RUNTIMELIGHTING
#ifndef UTILITY


extern model_t *lightmodel;
#define bsptexinfo(i) (*i)
#define dsurfedges lightmodel->surfedges
#define dvertexes lightmodel->vertexes
#define dedges lightmodel->edges
#define texinfo_t mtexinfo_t
#define Q_PI M_PI
#define Error Host_Error
#define byte qbyte

#define dfaces lightmodel->surfaces
#define dplanes lightmodel->planes
#define dface_t msurface_t
#define dvertex_t mvertex_t
#define point position

#define side flags & SURF_PLANEBACK

#define scaledist 1
#define rangescale 0.5
#define extrasamples 1
#define scalecos 0.5




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

	struct mentity_s *targetent;
} mentity_t;

static mentity_t entities[8192];
static int num_entities;

#define bsp_origin vec3_origin

/*
============
CastRay

Returns the distance between the points, or -1 if blocked
=============
*/
vec_t CastRay (vec3_t p1, vec3_t p2)
{
	trace_t	trace;
	vec3_t move;

	lightmodel->funcs.NativeTrace (lightmodel, 0, 0, NULL, p1, p2, vec3_origin, vec3_origin, FTECONTENTS_SOLID, &trace);
	if (trace.fraction < 1)
		return -1;	

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

void LightLoadEntities(char *entstring)
{
#define DEFAULTLIGHTLEVEL 300
	mentity_t *mapent;
	char key[1024];
	int i;
	int switchedstyle=32;
	num_entities = 0;

	while(1)
	{
		entstring = COM_Parse(entstring);
		if (!entstring || !*com_token)
			break;
		if (strcmp(com_token, "{"))
		{	//someone messed up. Stop parsing.
			Con_Printf("token wasn't an open brace\n");
			break;
		}

		mapent = &entities[num_entities];
		memset(mapent, 0, sizeof(*mapent));
		mapent->colour[0] = 0;
		mapent->colour[1] = 0;
		mapent->colour[2] = 0;
		while(1)
		{
			entstring = COM_Parse(entstring);
			if (!strcmp(com_token, "}"))
				break;
			strcpy(key, com_token);
			entstring = COM_Parse(entstring);
			ParseEpair(mapent, key, com_token);
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
				cont = lightmodel->funcs.PointContents (lightmodel, NULL, v);
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
			for (i = 0; i <= num_entities; i++)
			{
				if (entities[i].style >= 32 && !strcmp(entities[i].targetname, mapent->targetname))
				{
					mapent->style = entities[i].style;
					break;
				}
			}

			if (i == num_entities)
				mapent->style = switchedstyle++;
		}


		num_entities++;
	}

	for (mapent = entities; mapent < &entities[num_entities]; mapent++)
	{
		if (*mapent->target)
		{
			for (i = 0; i < num_entities; i++)
			{
				if (mapent == &entities[i])
					continue;

				if (!strcmp(mapent->target, entities[i].targetname))
				{
					mapent->targetent = &entities[i];
					break;
				}
			}
		}
	}
}

#else
#define mentity_t entity_t
#define UTILITY
#include "light.h"

#define bsptexinfo(i) texinfo[i]


/*
============
CastRay

Returns the distance between the points, or -1 if blocked
=============
*/
vec_t CastRay (vec3_t p1, vec3_t p2)
{
	int		i;
	vec_t	t;
	qboolean	trace;
		
	trace = TestLine (p1, p2);
		
	if (!trace)
		return -1;		// ray was blocked
		
	t = 0;
	for (i=0 ; i< 3 ; i++)
		t += (p2[i]-p1[i]) * (p2[i]-p1[i]);
		
	if (t < 1)
		t = 1;		// don't blow up...
	return sqrt(t);
}

#endif

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

#define	SINGLEMAP	(18*18*4)

typedef struct
{
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
	int		lightstyles[256];
	int		surfnum;
	dface_t	*face;
} llightinfo_t;


/*
================
CalcFaceVectors

Fills in texorg, worldtotex. and textoworld
================
*/
static void LightCalcFaceVectors (llightinfo_t *l)
{
	texinfo_t	*tex;
	int			i, j;
	vec3_t	texnormal;
	float	distscale;
	vec_t	dist, len;

	tex = &bsptexinfo(l->face->texinfo);
	
// convert from float to vec_t
	for (i=0 ; i<2 ; i++)
		for (j=0 ; j<3 ; j++)
			l->worldtotex[i][j] = tex->vecs[i][j];

// calculate a normal to the texture axis.  points can be moved along this
// without changing their S/T
	texnormal[0] = tex->vecs[1][1]*tex->vecs[0][2]
		- tex->vecs[1][2]*tex->vecs[0][1];
	texnormal[1] = tex->vecs[1][2]*tex->vecs[0][0]
		- tex->vecs[1][0]*tex->vecs[0][2];
	texnormal[2] = tex->vecs[1][0]*tex->vecs[0][1]
		- tex->vecs[1][1]*tex->vecs[0][0];
	VectorNormalize (texnormal);

// flip it towards plane normal
	distscale = DotProduct (texnormal, l->facenormal);
	if (!distscale)
		Error ("Texture axis perpendicular to face");
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
		l->texorg[i] = -tex->vecs[0][3]* l->textoworld[0][i] - tex->vecs[1][3] * l->textoworld[1][i];

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
static void LightCalcFaceExtents (llightinfo_t *l)
{
	dface_t *s;
	vec_t	mins[2], maxs[2], val;
	int		i,j, e;
	dvertex_t	*v;
	texinfo_t	*tex;
	
	s = l->face;

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

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
			val = v->point[0] * tex->vecs[j][0] + 
				v->point[1] * tex->vecs[j][1] +
				v->point[2] * tex->vecs[j][2] +
				tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{	
		l->exactmins[i] = mins[i];
		l->exactmaxs[i] = maxs[i];
		
		mins[i] = floor(mins[i]/16);
		maxs[i] = ceil(maxs[i]/16);

		l->texmins[i] = mins[i];
		l->texsize[i] = maxs[i] - mins[i];
		if (l->texsize[i] > 17)
			Error ("Bad surface extents");
	}
}

/*
=================
CalcPoints

For each texture aligned grid point, back project onto the plane
to get the world xyz value of the sample point
=================
*/
static int c_bad;
static void LightCalcPoints (llightinfo_t *l)
{
	int		i;
	int		s, t, j;
	int		w, h, step;
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
		starts = (l->texmins[0]-0.5)*16;
		startt = (l->texmins[1]-0.5)*16;
		step = 8;
	}
	else
	{
		h = l->texsize[1]+1;
		w = l->texsize[0]+1;
		starts = l->texmins[0]*16;
		startt = l->texmins[1]*16;
		step = 16;
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

				if (CastRay (facemid, surf) != -1)
					break;	// got it
				if (i & 1)
				{
					if (us > mids)
					{
						us -= 8;
						if (us < mids)
							us = mids;
					}
					else
					{
						us += 8;
						if (us > mids)
							us = mids;
					}
				}
				else
				{
					if (ut > midt)
					{
						ut -= 8;
						if (ut < midt)
							ut = midt;
					}
					else
					{
						ut += 8;
						if (ut > midt)
							ut = midt;
					}
				}
				
				// move surf 8 pixels towards the center
				VectorSubtract (facemid, surf, move);
				VectorNormalize (move);
				VectorMA (surf, 8, move, surf);
			}
			if (i == 2)
				c_bad++;
		}
	}
	
}


/*
===============================================================================

FACE LIGHTING

===============================================================================
*/

int		c_culldistplane, c_proper;

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
	int		size;
	int		c, i;
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
	{
		c_culldistplane++;
		return;
	}

	if (light->targetent)
	{
		VectorSubtract (light->targetent->origin, light->origin, spotvec);
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
	}

//
// check it for real
//
	hit = false;
	c_proper++;
	
	surf = l->surfpt[0];
	for (c=0 ; c<l->numsurfpt ; c++, surf+=3)
	{
		dist = CastRay(light->origin, surf)*scaledist;
		if (dist < 0)
			continue;	// light doesn't reach

		VectorSubtract (light->origin, surf, incoming);
		VectorNormalize (incoming);
		if (light->targetent)
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

		norms[c][0] -= add * incoming[0];	//Quake doesn't make sence some times.
		norms[c][1] -= add * incoming[1];
		norms[c][2] -= add * incoming[2];

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
void LightFace (int surfnum)
{
	dface_t *f;
	llightinfo_t	l;
	int		s, t;
	int		i,j,c,ch;
	vec_t	total, mean;
	int		size;
	int		lightmapwidth;
#ifdef UTILITY
	int		lightmapsize;
	byte	*out;
#endif
	byte	*rgbout;
	byte	*dulout;
	vec3_t	*light, *norm;
	vec3_t	wnorm, temp, svector, tvector;
	int		w;
	
	f = dfaces + surfnum;

//
// some surfaces don't need lightmaps
//	
#ifdef UTILITY
	for (j=0 ; j<MAXLIGHTMAPS ; j++)
		f->styles[j] = 255;
#endif
	if ( bsptexinfo(f->texinfo).flags & TEX_SPECIAL)
	{	// non-lit texture
#ifdef UTILITY
		f->lightofs = -1;
#endif
		return;
	}

#ifndef UTILITY
	if (!f->samples)
		return;
#endif

	memset (&l, 0, sizeof(l));
	l.surfnum = surfnum;
	l.face = f;

//
// rotate plane
//
#ifndef UTILITY
	VectorCopy (f->plane->normal, l.facenormal);
	l.facedist = f->plane->dist;
#else
	VectorCopy (dplanes[f->planenum].normal, l.facenormal);
	l.facedist = dplanes[f->planenum].dist;
#endif
	if (f->side)
	{
		VectorNegate (l.facenormal, l.facenormal);
		l.facedist = -l.facedist;
	}
	

	
	LightCalcFaceVectors (&l);
	LightCalcFaceExtents (&l);
	LightCalcPoints (&l);

	lightmapwidth = l.texsize[0]+1;

	size = lightmapwidth*(l.texsize[1]+1);
	if (size > SINGLEMAP)
		Error ("Bad lightmap size");

	for (i=0 ; i<MAXQ1LIGHTMAPS ; i++)
		l.lightstyles[i] = 255;
	
//
// cast all lights
//	
	l.numlightstyles = 0;
	for (i=0 ; i<num_entities ; i++)
	{
		if (entities[i].light)
			SingleLightFace (&entities[i], &l);
	}

	FixMinlight (&l);
		
	if (!l.numlightstyles)
	{	// no light hitting it
#ifdef UTILITY
		f->lightofs = -1;
#endif
		return;
	}

#ifndef UTILITY
	for (j=0 ; j<MAXQ1LIGHTMAPS ; j++)
		f->styles[j] = 255;
#endif
	
//
// save out the values
//
	for (i=0 ; i <MAXQ1LIGHTMAPS ; i++)
		f->styles[i] = l.lightstyles[i];


#ifdef UTILITY
	lightmapsize = size*l.numlightstyles;
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
	rgbout = f->samples;
	if (lightmodel->deluxdata)
	{
		dulout = f->samples - lightmodel->lightdata + lightmodel->deluxdata;

		VectorCopy(bsptexinfo(f->texinfo).vecs[0], svector);
		VectorNegate(bsptexinfo(f->texinfo).vecs[1], tvector);
		VectorNormalize(svector);
		VectorNormalize(tvector);
	}
	else
		dulout = NULL;
#endif


	
// extra filtering
//	h = (l.texsize[1]+1)*2;
	w = (l.texsize[0]+1)*2;

	for (i=0 ; i< l.numlightstyles ; i++)
	{
		if (l.lightstyles[i] == 0xff)
			Error ("Wrote empty lightmap");
		light = l.lightmaps[i];
		norm = l.lightnorm[i];
		c = 0;
		for (t=0 ; t<=l.texsize[1] ; t++)
		{
			for (s=0 ; s<=l.texsize[0] ; s++, c++)
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
					if (total > *rgbout)	//sorry - for qw
						total = *rgbout;
#endif
					if (total < 0)
						Error ("light < 0");
					
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
					temp[2] = DotProduct(wnorm, l.facenormal);
					VectorNormalize(temp);
					temp[2] += 0.5;
					VectorNormalize(temp);
					*dulout++ = (-temp[0]+1)*128;
					*dulout++ = (-temp[1]+1)*128;
					*dulout++ = (-temp[2]+1)*128;
				}
			}
		}
	}
}

#endif
