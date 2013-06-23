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
// r_surf.c: surface-related refresh code

#include "quakedef.h"
#if defined(GLQUAKE) || defined(D3DQUAKE)
#include "glquake.h"
#include "shader.h"
#include "renderque.h"
#include "com_mesh.h"
#include <math.h>

extern cvar_t r_ambient;

static vec3_t			modelorg;	/*set before recursively entering the visible surface finder*/
static qbyte			areabits[MAX_Q2MAP_AREAS/8];

model_t		*currentmodel;

int		lightmap_bytes;		// 1, 3 or 4
qboolean lightmap_bgra;

#define MAX_LIGHTMAP_SIZE LMBLOCK_WIDTH

vec3_t			blocknormals[MAX_LIGHTMAP_SIZE*MAX_LIGHTMAP_SIZE];
unsigned		blocklights[3*MAX_LIGHTMAP_SIZE*MAX_LIGHTMAP_SIZE];

lightmapinfo_t **lightmap;
int numlightmaps;

extern mleaf_t		*r_vischain;		// linked list of visible leafs

extern cvar_t r_stains;
extern cvar_t r_loadlits;
extern cvar_t r_stainfadetime;
extern cvar_t r_stainfadeammount;

static int lightmap_shift;
int Surf_LightmapShift (model_t *model)
{
	extern cvar_t gl_overbright_all, gl_overbright;

	if (gl_overbright_all.ival || (model->engineflags & MDLF_NEEDOVERBRIGHT))
		lightmap_shift = bound(0, gl_overbright.ival, 2);
	else
		lightmap_shift = 0;
	return lightmap_shift;
}

void Surf_RebuildLightmap_Callback (struct cvar_s *var, char *oldvalue)
{
	Mod_RebuildLightmaps();
}

//radius, x y z, r g b
void Surf_StainSurf (msurface_t *surf, float *parms)
{
	int			sd, td;
	float		dist, rad, minlight;
	float change;
	vec3_t		impact, local;
	int			s, t;
	int			i;
	int			smax, tmax;
	float amm;
	int lim;
	mtexinfo_t	*tex;
	stmap *stainbase;
	lightmapinfo_t *lm;

	lim = 255 - (r_stains.value*255);

#define stain(x)							\
	change = stainbase[(s)*3+x] + amm*parms[4+x];	\
	stainbase[(s)*3+x] = bound(lim, change, 255);

	if (surf->lightmaptexturenums[0] < 0)
		return;
	lm = lightmap[surf->lightmaptexturenums[0]];

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;

	stainbase = lm->stainmaps;
	stainbase += (surf->light_t[0] * lm->width + surf->light_s[0]) * 3;

	rad = *parms;
	dist = DotProduct ((parms+1), surf->plane->normal) - surf->plane->dist;
	rad -= fabs(dist);
	minlight = 0;
	if (rad < minlight)	//not hit
		return;
	minlight = rad - minlight;

	for (i=0 ; i<3 ; i++)
	{
		impact[i] = (parms+1)[i] - surf->plane->normal[i]*dist;
	}

	local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3];
	local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3];

	local[0] -= surf->texturemins[0];
	local[1] -= surf->texturemins[1];

	for (t = 0 ; t<tmax ; t++)
	{
		td = local[1] - t*16;
		if (td < 0)
			td = -td;
		for (s=0 ; s<smax ; s++)
		{
			sd = local[0] - s*16;
			if (sd < 0)
				sd = -sd;
			if (sd > td)
				dist = sd + (td>>1);
			else
				dist = td + (sd>>1);
			if (dist < minlight)
			{
				amm = (rad - dist);
				stain(0);
				stain(1);
				stain(2);

				surf->stained = true;
			}
		}
		stainbase += 3*lm->width;
	}

	if (surf->stained)
		surf->cached_dlight=-1;
}

//combination of R_AddDynamicLights and R_MarkLights
/*
static void Surf_StainNode (mnode_t *node, float *parms)
{
	mplane_t	*splitplane;
	float		dist;
	msurface_t	*surf;
	int			i;

	if (node->contents < 0)
		return;

	splitplane = node->plane;
	dist = DotProduct ((parms+1), splitplane->normal) - splitplane->dist;

	if (dist > (*parms))
	{
		Surf_StainNode (node->children[0], parms);
		return;
	}
	if (dist < (-*parms))
	{
		Surf_StainNode (node->children[1], parms);
		return;
	}

// mark the polygons
	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->flags&~(SURF_DONTWARP|SURF_PLANEBACK))
			continue;
		Surf_StainSurf(surf, parms);
	}

	Surf_StainNode (node->children[0], parms);
	Surf_StainNode (node->children[1], parms);
}
*/

void Surf_AddStain(vec3_t org, float red, float green, float blue, float radius)
{
	physent_t *pe;
	int i;

	float parms[7];
	if (!cl.worldmodel || cl.worldmodel->needload || r_stains.value <= 0)
		return;
	parms[0] = radius;
	parms[1] = org[0];
	parms[2] = org[1];
	parms[3] = org[2];
	parms[4] = red;
	parms[5] = green;
	parms[6] = blue;


	cl.worldmodel->funcs.StainNode(cl.worldmodel->nodes+cl.worldmodel->hulls[0].firstclipnode, parms);

	//now stain bsp models other than world.

	for (i=1 ; i< pmove.numphysent ; i++)	//0 is world...
	{
		pe = &pmove.physents[i];
		if (pe->model && pe->model->surfaces == cl.worldmodel->surfaces)
		{
			parms[1] = org[0] - pe->origin[0];
			parms[2] = org[1] - pe->origin[1];
			parms[3] = org[2] - pe->origin[2];

			if (pe->angles[0] || pe->angles[1] || pe->angles[2])
			{
				vec3_t f, r, u, temp;
				AngleVectors(pe->angles, f, r, u);
				VectorCopy((parms+1), temp);
				parms[1] = DotProduct(temp, f);
				parms[2] = -DotProduct(temp, r);
				parms[3] = DotProduct(temp, u);
			}


			pe->model->funcs.StainNode(pe->model->nodes+pe->model->hulls[0].firstclipnode, parms);
		}
	}
}

void Surf_WipeStains(void)
{
	int i;
	for (i = 0; i < numlightmaps; i++)
	{
		if (!lightmap[i])
			break;
		memset(lightmap[i]->stainmaps, 255, LMBLOCK_WIDTH*LMBLOCK_HEIGHT*3*sizeof(stmap));
	}
}

void Surf_LessenStains(void)
{
	int i;
	msurface_t	*surf;

	int			smax, tmax;
	int			s, t;
	stmap *stain;
	int stride;
	int ammount;
	int limit;
	lightmapinfo_t *lm;

	static float time;

	if (!r_stains.value)
		return;

	time += host_frametime;
	if (time < r_stainfadetime.value)
		return;
	time-=r_stainfadetime.value;

	ammount = r_stainfadeammount.value;
	limit = 255 - ammount;

	surf = cl.worldmodel->surfaces;
	for (i=0 ; i<cl.worldmodel->numsurfaces ; i++, surf++)
	{
		if (surf->stained)
		{
			lm = lightmap[surf->lightmaptexturenums[0]];

			surf->cached_dlight=-1;//nice hack here...

			smax = (surf->extents[0]>>4)+1;
			tmax = (surf->extents[1]>>4)+1;

			stain = lm->stainmaps;
			stain += (surf->light_t[0] * lm->width + surf->light_s[0]) * 3;

			stride = (lm->width-smax)*3;

			surf->stained = false;

			smax*=3;

			for (t = 0 ; t<tmax ; t++, stain+=stride)
			{
				for (s=0 ; s<smax ; s++)
				{
					if (*stain < limit)	//eventually decay to 255
					{
						*stain += ammount;
						surf->stained=true;
					}
					else	//reset to 255
						*stain = 255;

					stain++;
				}
			}
		}
	}
}

/*
===============
R_AddDynamicLights
===============
*/
static void Surf_AddDynamicLights (msurface_t *surf)
{
	int			lnum;
	int			sd, td;
	float		dist, rad, minlight;
	vec3_t		impact, local;
	int			s, t;
	int			i;
	int			smax, tmax;
	mtexinfo_t	*tex;
	float a;
	unsigned	*bl;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;

	for (lnum=rtlights_first; lnum<RTL_FIRST; lnum++)
	{
		if ( !(surf->dlightbits & (1<<lnum) ) )
			continue;		// not lit by this light

		if (!(cl_dlights[lnum].flags & LFLAG_LIGHTMAP))
			continue;

		rad = cl_dlights[lnum].radius;
		dist = DotProduct (cl_dlights[lnum].origin, surf->plane->normal) -
				surf->plane->dist;
		rad -= fabs(dist);
		minlight = cl_dlights[lnum].minlight;
		if (rad < minlight)
			continue;
		minlight = rad - minlight;

		for (i=0 ; i<3 ; i++)
		{
			impact[i] = cl_dlights[lnum].origin[i] -
					surf->plane->normal[i]*dist;
		}

		local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3];
		local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3];

		local[0] -= surf->texturemins[0];
		local[1] -= surf->texturemins[1];

		a = 256*(cl_dlights[lnum].color[0]*NTSC_RED + cl_dlights[lnum].color[1]*NTSC_GREEN + cl_dlights[lnum].color[2]*NTSC_BLUE);

		bl = blocklights;
		for (t = 0 ; t<tmax ; t++)
		{
			td = local[1] - t*16;
			if (td < 0)
				td = -td;
			for (s=0 ; s<smax ; s++)
			{
				sd = local[0] - s*16;
				if (sd < 0)
					sd = -sd;
				if (sd > td)
					dist = sd + (td>>1);
				else
					dist = td + (sd>>1);
				if (dist < minlight)
					bl[0] += (rad - dist)*a;
				bl++;
			}
		}
	}
}

// warning: ‘Surf_AddDynamicLightNorms’ defined but not used
/*
static void Surf_AddDynamicLightNorms (msurface_t *surf)
{
	int			lnum;
	int			sd, td;
	float		dist, rad, minlight;
	vec3_t		impact, local;
	int			s, t;
	int			i;
	int			smax, tmax;
	mtexinfo_t	*tex;
	float a;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;

	for (lnum=rtlights_first; lnum<RTL_FIRST; lnum++)
	{
		if ( !(surf->dlightbits & (1<<lnum) ) )
			continue;		// not lit by this light

		if (!(cl_dlights[lnum].flags & LFLAG_ALLOW_LMHACK))
			continue;

		rad = cl_dlights[lnum].radius;
		dist = DotProduct (cl_dlights[lnum].origin, surf->plane->normal) -
				surf->plane->dist;
		rad -= fabs(dist);
		minlight = cl_dlights[lnum].minlight;
		if (rad < minlight)
			continue;
		minlight = rad - minlight;

		for (i=0 ; i<3 ; i++)
		{
			impact[i] = cl_dlights[lnum].origin[i] -
					surf->plane->normal[i]*dist;
		}

		local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3];
		local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3];

		local[0] -= surf->texturemins[0];
		local[1] -= surf->texturemins[1];

		a = 256*(cl_dlights[lnum].color[0]*NTSC_RED + cl_dlights[lnum].color[1]*NTSC_GREEN + cl_dlights[lnum].color[2]*NTSC_BLUE);

		for (t = 0 ; t<tmax ; t++)
		{
			td = local[1] - t*16;
			if (td < 0)
				td = -td;
			for (s=0 ; s<smax ; s++)
			{
				sd = local[0] - s*16;
				if (sd < 0)
					sd = -sd;
				if (sd > td)
					dist = sd + (td>>1);
				else
					dist = td + (sd>>1);
				if (dist < minlight)
				{
//					blocknormals[t*smax + s][0] -= (rad - dist)*(impact[0]-local[0])/8192.0;
//					blocknormals[t*smax + s][1] -= (rad - dist)*(impact[1]-local[1])/8192.0;
					blocknormals[t*smax + s][2] += 0.5*blocknormals[t*smax + s][2]*(rad - dist)/256;
				}
			}
		}
	}
}
*/

#ifdef PEXT_LIGHTSTYLECOL
static void Surf_AddDynamicLightsColours (msurface_t *surf)
{
	int			lnum;
	int			sd, td;
	float		dist, rad, minlight;
	vec3_t		impact, local;
	int			s, t;
	int			i;
	int			smax, tmax;
	mtexinfo_t	*tex;
//	float temp;
	float r, g, b;
	unsigned	*bl;
	vec3_t lightofs;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;

	for (lnum=rtlights_first; lnum<RTL_FIRST; lnum++)
	{
		if ( !(surf->dlightbits & (1<<lnum) ) )
			continue;		// not lit by this light

		rad = cl_dlights[lnum].radius;
		VectorSubtract(cl_dlights[lnum].origin, currententity->origin, lightofs);
		dist = DotProduct (lightofs, surf->plane->normal) -
				surf->plane->dist;
		rad -= fabs(dist);
		minlight = cl_dlights[lnum].minlight;
		if (rad < minlight)
			continue;
		minlight = rad - minlight;

		for (i=0 ; i<3 ; i++)
		{
			impact[i] = lightofs[i] -
					surf->plane->normal[i]*dist;
		}

		local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3];
		local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3];

		local[0] -= surf->texturemins[0];
		local[1] -= surf->texturemins[1];

		r = cl_dlights[lnum].color[0]*128;
		g = cl_dlights[lnum].color[1]*128;
		b = cl_dlights[lnum].color[2]*128;

/*		if (cl_dlights[lnum].type == 1)	//a wierd effect.
		{
			for (t = 0 ; t<tmax ; t++)
			{
				td = local[1] - t*16;
				if (td < 0)
					td = -td;
				for (s=0 ; s<smax ; s++)
				{
					sd = local[0] - s*16;
					if (sd < 0)
						sd = -sd;
					if (sd > td)
						dist = sd + (td>>1);
					else
						dist = td + (sd>>1);
					if (dist < minlight)
					{
						blocklights[t*smax + s]		+= 2*sin(dist/10+cl.time*20)*(rad - dist)*256 * cl_dlights[lnum].colour[0]*3;
						greenblklights[t*smax + s]	+= 2*sin(M_PI/3+dist/10+cl.time*20)*(rad - dist)*256 * cl_dlights[lnum].colour[1]*3;
						blueblklights[t*smax + s]	+= 2*sin(2*M_PI/3+dist/10+cl.time*20)*(rad - dist)*256 * cl_dlights[lnum].colour[2]*3;
					}
				}
			}
		}
		else
*/		{
			bl = blocklights;
			for (t = 0 ; t<tmax ; t++)
			{
				td = local[1] - t*16;
				if (td < 0)
					td = -td;
				for (s=0 ; s<smax ; s++)
				{
					sd = local[0] - s*16;
					if (sd < 0)
						sd = -sd;
					if (sd > td)
						dist = sd + (td>>1);
					else
						dist = td + (sd>>1);
					if (dist < minlight)
					{
						bl[0] += (rad - dist)*r;
						bl[1] += (rad - dist)*g;
						bl[2] += (rad - dist)*b;
					}
					bl += 3;
				}
			}
		}
	}
}
#endif



static void Surf_BuildDeluxMap (msurface_t *surf, qbyte *dest)
{
	int			smax, tmax;
	int			i, j, size;
	qbyte		*lightmap;
	qbyte		*deluxmap;
	unsigned	scale;
	int			maps;
	float intensity;
	vec_t		*bnorm;
	vec3_t temp;

	int stride = LMBLOCK_WIDTH*lightmap_bytes;

	if (!dest)
		return;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	size = smax*tmax;
	lightmap = surf->samples;

	// set to full bright if no light data
	if (!currentmodel->deluxdata)
	{
		for (i=0 ; i<size ; i++)
		{
			blocknormals[i][0] = 0.9;//surf->orientation[2][0];
			blocknormals[i][1] = 0.8;//surf->orientation[2][1];
			blocknormals[i][2] = 1;//surf->orientation[2][2];
		}
		goto store;
	}

	if (currentmodel->engineflags & MDLF_RGBLIGHTING)
		deluxmap = surf->samples - currentmodel->lightdata + currentmodel->deluxdata;
	else
		deluxmap = (surf->samples - currentmodel->lightdata)*3 + currentmodel->deluxdata;


// clear to no light
	for (i=0 ; i<size ; i++)
	{
		blocknormals[i][0] = 0;
		blocknormals[i][1] = 0;
		blocknormals[i][2] = 0;
	}

// add all the lightmaps
	if (lightmap)
	{
		if (currentmodel->engineflags & MDLF_RGBLIGHTING)
		{
			deluxmap = surf->samples - currentmodel->lightdata + currentmodel->deluxdata;

			for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
				 maps++)
			{
				scale = d_lightstylevalue[surf->styles[maps]];
				for (i=0 ; i<size ; i++)
				{
					intensity = (lightmap[i*3]+lightmap[i*3+1]+lightmap[i*3+2]) * scale;
					blocknormals[i][0] += intensity*(deluxmap[i*3+0]-127);
					blocknormals[i][1] += intensity*(deluxmap[i*3+1]-127);
					blocknormals[i][2] += intensity*(deluxmap[i*3+2]-127);
				}
				lightmap += size*3;	// skip to next lightmap
				deluxmap += size*3;
			}
		}
		else
		{
			deluxmap = (surf->samples - currentmodel->lightdata)*3 + currentmodel->deluxdata;

			for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
				 maps++)
			{
				scale = d_lightstylevalue[surf->styles[maps]];
				for (i=0 ; i<size ; i++)
				{
					intensity = (lightmap[i]) * scale;
					blocknormals[i][0] += intensity*(deluxmap[i*3+0]-127);
					blocknormals[i][1] += intensity*(deluxmap[i*3+1]-127);
					blocknormals[i][2] += intensity*(deluxmap[i*3+2]-127);
				}
				lightmap += size;	// skip to next lightmap
				deluxmap += size*3;
			}
		}
	}

store:
	// add all the dynamic lights
//	if (surf->dlightframe == r_framecount)
//		GLR_AddDynamicLightNorms (surf);

// bound, invert, and shift

	stride -= smax*lightmap_bytes;

	bnorm = blocknormals[0];
	for (i=0 ; i<tmax ; i++, dest += stride)
	{
		for (j=0 ; j<smax ; j++)
		{
			temp[0] = bnorm[0];
			temp[1] = bnorm[1];
			temp[2] = bnorm[2];	//half the effect? so we emulate light's scalecos of 0.5
			VectorNormalize(temp);
			dest[2] = (temp[0]+1)/2*255;
			dest[1] = (temp[1]+1)/2*255;
			dest[0] = (temp[2]+1)/2*255;

			dest += lightmap_bytes;
			bnorm+=3;
		}
	}
}

enum lm_mode
{
	bgra4_os,
	bgra4,
	rgb3_os,
	lum
};
/*any sane compiler will inline and split this, removing the stainsrc stuff
just unpacks the internal lightmap block into texture info ready for upload
merges stains and oversaturates overbrights.
*/
static void Surf_StoreLightmap(qbyte *dest, int smax, int tmax, unsigned int shift, enum lm_mode lm_mode, stmap *stainsrc)
{
	int r, g, b, t, m;
	unsigned int i, j;
	unsigned int *bl;
	int stride;

	switch (lm_mode)
	{
	case bgra4_os:
		stride = LMBLOCK_WIDTH*4 - (smax<<2);

		bl = blocklights;

		for (i=0 ; i<tmax ; i++, dest += stride)
		{
			for (j=0 ; j<smax ; j++)
			{
				r = *bl++ >> shift;
				g = *bl++ >> shift;
				b = *bl++ >> shift;

				if (stainsrc)	// merge in stain
				{
					r = (127+r*(*stainsrc++)) >> 8;
					g = (127+g*(*stainsrc++)) >> 8;
					b = (127+b*(*stainsrc++)) >> 8;
				}

				// quake 2 method, scale highest down to
				// maintain hue
				m = max(max(r, g), b);
				if (m > 255)
				{
					r *= 255.0/m;
					g *= 255.0/m;
					b *= 255.0/m;
				}

				dest[0] = b;
				dest[1] = g;
				dest[2] = r;
				dest[3] = 255;

				dest += 4;
			}
			if (stainsrc)
				stainsrc += (LMBLOCK_WIDTH - smax)*3;
		}
		break;
/*
	case bgra4:
		stride = LMBLOCK_WIDTH*4 - (smax<<2);

		bl = blocklights;

		for (i=0 ; i<tmax ; i++, dest += stride)
		{
			for (j=0 ; j<smax ; j++)
			{
				r = *bl++ >> shift;
				g = *bl++ >> shift;
				b = *bl++ >> shift;

				if (stainsrc)	// merge in stain
				{
					r = (127+r*(*stainsrc++)) >> 8;
					g = (127+g*(*stainsrc++)) >> 8;
					b = (127+b*(*stainsrc++)) >> 8;
				}

				if (r > 255)
					dest[2] = 255;
				else if (r < 0)
					dest[2] = 0;
				else
					dest[2] = r;

				if (g > 255)
					dest[1] = 255;
				else if (g < 0)
					dest[1] = 0;
				else
					dest[1] = g;

				if (b > 255)
					dest[0] = 255;
				else if (b < 0)
					dest[0] = 0;
				else
					dest[0] = b;

				dest[3] = 255;
				dest += 4;
			}
			if (stainsrc)
				stainsrc += (LMBLOCK_WIDTH - smax)*3;
		}
		break;
*/
	case rgb3_os:
		stride = LMBLOCK_WIDTH*3 - (smax*3);
		bl = blocklights;

		for (i=0 ; i<tmax ; i++, dest += stride)
		{
			for (j=0 ; j<smax ; j++)
			{
				r = *bl++ >> shift;
				g = *bl++ >> shift;
				b = *bl++ >> shift;

				if (stainsrc)	// merge in stain
				{
					r = (127+r*(*stainsrc++)) >> 8;
					g = (127+g*(*stainsrc++)) >> 8;
					b = (127+b*(*stainsrc++)) >> 8;
				}

				// quake 2 method, scale highest down to
				// maintain hue
				m = max(max(r, g), b);
				if (m > 255)
				{
					r *= 255.0/m;
					g *= 255.0/m;
					b *= 255.0/m;
				}

				dest[0] = r;
				dest[1] = g;
				dest[2] = b;
				dest += 3;
			}
			if (stainsrc)
				stainsrc += (LMBLOCK_WIDTH - smax)*3;
		}
		break;
	case lum:
		stride = LMBLOCK_WIDTH;
		bl = blocklights;
		for (i=0 ; i<tmax ; i++, dest += stride)
		{
			for (j=0 ; j<smax ; j++)
			{
				t = *bl++;
				t >>= shift;
				if (t > 255)
					t = 255;
				dest[j] = t;
			}
		}
		break;
	default:
		Sys_Error ("Bad lightmap format");
	}
}

/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
static void Surf_BuildLightMap (msurface_t *surf, qbyte *dest, qbyte *deluxdest, stmap *stainsrc, int shift, int ambient)
{
	int			smax, tmax;
	int			t;
	int			i, j, size;
	qbyte		*lightmap;
	unsigned	scale;
	int			maps;
	unsigned	*bl;

	//int stride = LMBLOCK_WIDTH*lightmap_bytes; //warning: unused variable ‘stride’

	shift += 7; // increase to base value
	surf->cached_dlight = (surf->dlightframe == r_framecount);

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	size = smax*tmax;
	lightmap = surf->samples;

	if (size > MAX_LIGHTMAP_SIZE*MAX_LIGHTMAP_SIZE)
	{	//fixme: fill in?
		Con_Printf("Lightmap too large\n");
		return;
	}

	if (currentmodel->deluxdata)
		Surf_BuildDeluxMap(surf, deluxdest);

#ifdef PEXT_LIGHTSTYLECOL
	if (lightmap_bytes == 4 || lightmap_bytes == 3)
	{
		// set to full bright if no light data
		if (ambient < 0)
		{
			t = (-1-ambient)*255;
			for (i=0 ; i<size*3 ; i++)
			{
				blocklights[i] = t;
			}

			for (maps = 0 ; maps < MAXLIGHTMAPS ; maps++)
			{
				surf->cached_light[maps] = -1-ambient;
				surf->cached_colour[maps] = 0xff;
			}
		}
		else if (r_fullbright.value>0)	//not qw
		{
			for (i=0 ; i<size*3 ; i++)
			{
				blocklights[i] = r_fullbright.value*255*256;
			}
		}
		else if (!currentmodel->lightdata)
		{
			/*fullbright if map is not lit*/
			for (i=0 ; i<size*3 ; i++)
			{
				blocklights[i] = 255*256;
			}
		}
		else if (!surf->samples)
		{
			/*no samples, but map is otherwise lit = pure black*/
			for (i=0 ; i<size*3 ; i++)
			{
				blocklights[i] = 0;
			}
			surf->cached_light[0] = 0;
			surf->cached_colour[0] = 0;
		}
		else
		{
// clear to no light
			t = ambient;
			if (t == 0)
				memset(blocklights, 0, size*3*sizeof(*bl));
			else
			{
				for (i=0 ; i<size*3 ; i++)
				{
					blocklights[i] = t;
				}
			}

// add all the lightmaps
			if (lightmap)
			{
				if (currentmodel->fromgame == fg_quake3)	//rgb
				{
					/*q3 lightmaps are meant to be pre-built
					this code is misguided, and ought never be executed anyway.
					*/
					bl = blocklights;
					for (i = 0; i < tmax; i++)
					{
						for (j = 0; j < smax; j++)
						{
							bl[0]		= 255*lightmap[(i*LMBLOCK_WIDTH+j)*3];
							bl[1]		= 255*lightmap[(i*LMBLOCK_WIDTH+j)*3+1];
							bl[2]		= 255*lightmap[(i*LMBLOCK_WIDTH+j)*3+2];
							bl+=3;
						}
					}
				}
				else if (currentmodel->engineflags & MDLF_RGBLIGHTING)	//rgb
				{
					for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
						 maps++)
					{
						scale = d_lightstylevalue[surf->styles[maps]];
						surf->cached_light[maps] = scale;	// 8.8 fraction
						surf->cached_colour[maps] = cl_lightstyle[surf->styles[maps]].colour;

						if (scale)
						{
							if (cl_lightstyle[surf->styles[maps]].colour == 7)	//hopefully a faster alternative.
							{
								bl = blocklights;
								for (i=0 ; i<size*3 ; i++)
								{
									*bl++		+=   *lightmap++ * scale;
								}
							}
							else
							{
								if (cl_lightstyle[surf->styles[maps]].colour & 1)
									for (i=0 ; i<size ; i++)
										blocklights[i+0]	+= lightmap[i*3+0] * scale;
								if (cl_lightstyle[surf->styles[maps]].colour & 2)
									for (i=0 ; i<size ; i++)
										blocklights[i+1]	+= lightmap[i*3+1] * scale;
								if (cl_lightstyle[surf->styles[maps]].colour & 4)
									for (i=0 ; i<size ; i++)
										blocklights[i+2]	+= lightmap[i*3+2] * scale;
								lightmap += size*3;	// skip to next lightmap
							}
						}
						else
							lightmap += size*3;	// skip to next lightmap
					}
				}
				else
					for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
						 maps++)
					{
						scale = d_lightstylevalue[surf->styles[maps]];
						surf->cached_light[maps] = scale;	// 8.8 fraction
						surf->cached_colour[maps] = cl_lightstyle[surf->styles[maps]].colour;

						if (cl_lightstyle[surf->styles[maps]].colour == 7)	//hopefully a faster alternative.
						{
							bl = blocklights;
							for (i=0 ; i<size ; i++)
							{
								*bl++		+= *lightmap * scale;
								*bl++		+= *lightmap * scale;
								*bl++		+= *lightmap * scale;
								lightmap++;
							}
						}
						else
						{
							if (cl_lightstyle[surf->styles[maps]].colour & 1)
								for (i=0, bl = blocklights; i<size; i++, bl+=3)
									*bl += lightmap[i] * scale;
							if (cl_lightstyle[surf->styles[maps]].colour & 2)
								for (i=0, bl = blocklights+1; i<size; i++, bl+=3)
									*bl += lightmap[i] * scale;
							if (cl_lightstyle[surf->styles[maps]].colour & 4)
								for (i=0, bl = blocklights+2; i<size; i++, bl+=3)
									*bl += lightmap[i] * scale;
							lightmap += size;	// skip to next lightmap
						}
					}
			}
		}

		// add all the dynamic lights
		if (surf->dlightframe == r_framecount)
			Surf_AddDynamicLightsColours (surf);

		if (!r_stains.value || !surf->stained)
			stainsrc = NULL;

		if (lightmap_bytes == 4)
		{
			if (lightmap_bgra)
			{
				Surf_StoreLightmap(dest, smax, tmax, shift, bgra4_os, stainsrc);
			}
			else
			{
				/*if (!r_stains.value || !surf->stained)
					Surf_StoreLightmap(dest, smax, tmax, shift, rgba4, NULL);
				else
					Surf_StoreLightmap(dest, smax, tmax, shift, rgba4, stainsrc);
				*/
			}
		}
		else if (lightmap_bytes == 3)
		{
			if (lightmap_bgra)
			{
				/*
				if (!r_stains.value || !surf->stained)
					Surf_StoreLightmap(dest, smax, tmax, shift, bgr3, NULL);
				else
					Surf_StoreLightmap(dest, smax, tmax, shift, bgr3, stainsrc);
				*/
			}
			else
			{
				Surf_StoreLightmap(dest, smax, tmax, shift, rgb3_os, stainsrc);
			}
		}
	}
	else
#endif
	{
	// set to full bright if no light data
		if (!surf->samples || !currentmodel->lightdata)
		{
			for (i=0 ; i<size*3 ; i++)
			{
				blocklights[i] = 255*256;
			}
			surf->cached_light[0] = d_lightstylevalue[0];
			surf->cached_colour[0] = cl_lightstyle[0].colour;
		}
		else if (r_fullbright.ival)
		{
			for (i=0 ; i<size ; i++)
				blocklights[i] = 255*256;
		}
		else
		{
// clear to no light
			for (i=0 ; i<size ; i++)
				blocklights[i] = 0;

// add all the lightmaps
			if (lightmap)
			{
				if (currentmodel->engineflags & MDLF_RGBLIGHTING)	//rgb
					for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
						 maps++)
					{
						scale = d_lightstylevalue[surf->styles[maps]]/3;
						surf->cached_light[maps] = scale;	// 8.8 fraction
						surf->cached_colour[maps] = cl_lightstyle[surf->styles[maps]].colour;
						for (i=0 ; i<size ; i++)
							blocklights[i] += (lightmap[i*3]+lightmap[i*3+1]+lightmap[i*3+2]) * scale;
						lightmap += size*3;	// skip to next lightmap
					}

				else
					for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
						 maps++)
					{
						scale = d_lightstylevalue[surf->styles[maps]];
						surf->cached_light[maps] = scale;	// 8.8 fraction
						surf->cached_colour[maps] = cl_lightstyle[surf->styles[maps]].colour;
						for (i=0 ; i<size ; i++)
							blocklights[i] += lightmap[i] * scale;
						lightmap += size;	// skip to next lightmap
					}
			}
// add all the dynamic lights
			if (surf->dlightframe == r_framecount)
				Surf_AddDynamicLights (surf);
		}

		Surf_StoreLightmap(dest, smax, tmax, shift, lum, stainsrc);
	}
}


/*
=============================================================

	BRUSH MODELS

=============================================================
*/

/*
================
R_RenderDynamicLightmaps
Multitexture
================
*/
void Surf_RenderDynamicLightmaps (msurface_t *fa)
{
	qbyte		*base, *luxbase;
	stmap *stainbase;
	int			maps;
	glRect_t    *theRect;
	int smax, tmax;
	lightmapinfo_t *lm, *dlm;

	//surfaces without lightmaps
	if (fa->lightmaptexturenums[0]<0)
		return;

	// check for lightmap modification
	if (!fa->samples)
	{
		if (fa->cached_light[0] != 0
			|| fa->cached_colour[0] != 0)
			goto dynamic;
	}
	else
	{
		for (maps = 0 ; maps < MAXLIGHTMAPS && fa->styles[maps] != 255 ;
			 maps++)
			if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps]
				|| cl_lightstyle[fa->styles[maps]].colour != fa->cached_colour[maps])
				goto dynamic;
	}

	if (fa->dlightframe == r_framecount	// dynamic this frame
		|| fa->cached_dlight)			// dynamic previously
	{
		RSpeedLocals();
dynamic:
		RSpeedRemark();

		lm = lightmap[fa->lightmaptexturenums[0]];

		lm->modified = true;

		smax = (fa->extents[0]>>4)+1;
		tmax = (fa->extents[1]>>4)+1;

		theRect = &lm->rectchange;
		if (fa->light_t[0] < theRect->t) {
			if (theRect->h)
				theRect->h += theRect->t - fa->light_t[0];
			theRect->t = fa->light_t[0];
		}
		if (fa->light_s[0] < theRect->l) {
			if (theRect->w)
				theRect->w += theRect->l - fa->light_s[0];
			theRect->l = fa->light_s[0];
		}
		if ((theRect->w + theRect->l) < (fa->light_s[0] + smax))
			theRect->w = (fa->light_s[0]-theRect->l)+smax;
		if ((theRect->h + theRect->t) < (fa->light_t[0] + tmax))
			theRect->h = (fa->light_t[0]-theRect->t)+tmax;

		if (lm->hasdeluxe)
		{
			dlm = lightmap[fa->lightmaptexturenums[0]+1];
			dlm->modified = true;
			theRect = &dlm->rectchange;
			if (fa->light_t[0] < theRect->t) {
				if (theRect->h)
					theRect->h += theRect->t - fa->light_t[0];
				theRect->t = fa->light_t[0];
			}
			if (fa->light_s[0] < theRect->l) {
				if (theRect->w)
					theRect->w += theRect->l - fa->light_s[0];
				theRect->l = fa->light_s[0];
			}

			if ((theRect->w + theRect->l) < (fa->light_s[0] + smax))
				theRect->w = (fa->light_s[0]-theRect->l)+smax;
			if ((theRect->h + theRect->t) < (fa->light_t[0] + tmax))
				theRect->h = (fa->light_t[0]-theRect->t)+tmax;

			luxbase = dlm->lightmaps;
			luxbase += fa->light_t[0] * dlm->width * lightmap_bytes + fa->light_s[0] * lightmap_bytes;
		}
		else
			luxbase = NULL;


		base = lm->lightmaps;
		base += fa->light_t[0] * lm->width * lightmap_bytes + fa->light_s[0] * lightmap_bytes;
		stainbase = lm->stainmaps;
		stainbase += (fa->light_t[0] * lm->width + fa->light_s[0]) * 3;
		Surf_BuildLightMap (fa, base, luxbase, stainbase, lightmap_shift, r_ambient.value*255);

		RSpeedEnd(RSPEED_DYNAMIC);
	}
}

void Surf_RenderAmbientLightmaps (msurface_t *fa, int ambient)
{
	qbyte		*base, *luxbase;
	stmap *stainbase;
	glRect_t    *theRect;
	int smax, tmax;
	lightmapinfo_t *lm, *dlm;

	if (!fa->mesh)
		return;

	//surfaces without lightmaps
	if (fa->lightmaptexturenums[0]<0)
		return;

	if (fa->cached_light[0] != ambient || fa->cached_colour[0] != 0xff)
		goto dynamic;

	if (fa->dlightframe == r_framecount	// dynamic this frame
		|| fa->cached_dlight)			// dynamic previously
	{
		RSpeedLocals();
dynamic:
		RSpeedRemark();

		lm = lightmap[fa->lightmaptexturenums[0]];

		lm->modified = true;

		smax = (fa->extents[0]>>4)+1;
		tmax = (fa->extents[1]>>4)+1;

		theRect = &lm->rectchange;
		if (fa->light_t[0] < theRect->t)
		{
			if (theRect->h)
				theRect->h += theRect->t - fa->light_t[0];
			theRect->t = fa->light_t[0];
		}
		if (fa->light_s[0] < theRect->l)
		{
			if (theRect->w)
				theRect->w += theRect->l - fa->light_s[0];
			theRect->l = fa->light_s[0];
		}
		if ((theRect->w + theRect->l) < (fa->light_s[0] + smax))
			theRect->w = (fa->light_s[0]-theRect->l)+smax;
		if ((theRect->h + theRect->t) < (fa->light_t[0] + tmax))
			theRect->h = (fa->light_t[0]-theRect->t)+tmax;

		if (lm->hasdeluxe)
		{
			dlm = lightmap[fa->lightmaptexturenums[0]+1];
			lm->modified = true;
			theRect = &lm->rectchange;
			if (fa->light_t[0] < theRect->t)
			{
				if (theRect->h)
					theRect->h += theRect->t - fa->light_t[0];
				theRect->t = fa->light_t[0];
			}
			if (fa->light_s[0] < theRect->l)
			{
				if (theRect->w)
					theRect->w += theRect->l - fa->light_s[0];
				theRect->l = fa->light_s[0];
			}

			if ((theRect->w + theRect->l) < (fa->light_s[0] + smax))
				theRect->w = (fa->light_s[0]-theRect->l)+smax;
			if ((theRect->h + theRect->t) < (fa->light_t[0] + tmax))
				theRect->h = (fa->light_t[0]-theRect->t)+tmax;

			luxbase = dlm->lightmaps;
			luxbase += fa->light_t[0] * dlm->width * lightmap_bytes + fa->light_s[0] * lightmap_bytes;
		}
		else
			luxbase = NULL;


		base = lm->lightmaps;
		base += fa->light_t[0] * lm->width * lightmap_bytes + fa->light_s[0] * lightmap_bytes;
		stainbase = lm->stainmaps;
		stainbase += (fa->light_t[0] * lm->width + fa->light_s[0]) * 3;
		Surf_BuildLightMap (fa, base, luxbase, stainbase, lightmap_shift, -1-ambient);

		RSpeedEnd(RSPEED_DYNAMIC);
	}
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/

#if 0
static qbyte *R_MarkLeafSurfaces_Q1 (void)
{
	qbyte	*vis;
	mleaf_t	*leaf;
	int		i, j;
	msurface_t *surf;
	int shift;

	vis = R_CalcVis_Q1();

	for (i=0 ; i<cl.worldmodel->numleafs ; i++)
	{
		if (vis[i>>3] & (1<<(i&7)))
		{
			leaf = (mleaf_t *)&cl.worldmodel->leafs[i+1];

			if (R_CullBox (leaf->minmaxs, leaf->minmaxs+3))
				continue;
			leaf->visframe = r_visframecount;

			for (j = 0; j < leaf->nummarksurfaces; j++)
			{
				surf = leaf->firstmarksurface[j];
				if (surf->visframe == r_visframecount)
					continue;
				surf->visframe = r_visframecount;

				*surf->mark = surf;
			}
		}
	}

	{
		texture_t *tex;

		shift = Surf_LightmapShift(cl.worldmodel);

		for (i = 0; i < cl.worldmodel->numtextures; i++)
		{
			tex = cl.worldmodel->textures[i];
			if (!tex)
				continue;
			for (j = 0; j < tex->vbo.meshcount; j++)
			{
				surf = tex->vbo.meshlist[j];
				if (surf)
				{
					Surf_RenderDynamicLightmaps (surf);

					tex->vbo.meshlist[j] = NULL;
					surf->sbatch->mesh[surf->sbatch->meshes++] = surf->mesh;
				}
			}
		}
	}
	return vis;
}
#endif

/*
static qbyte *Surf_MaskVis(qbyte *src, qbyte *dest)
{
	int i;
	if (cl.worldmodel->leafs[i].ma
}
*/
qbyte *frustumvis;
/*
================
R_RecursiveWorldNode
================
*/
static void Surf_RecursiveWorldNode (mnode_t *node, unsigned int clipflags)
{
	int			c, side, clipped;
	mplane_t	*plane, *clipplane;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	double		dot;

start:

	if (node->contents == Q1CONTENTS_SOLID)
		return;		// solid

	if (node->visframe != r_visframecount)
		return;

	for (c = 0, clipplane = frustum; c < FRUSTUMPLANES; c++, clipplane++)
	{
		if (!(clipflags & (1 << c)))
			continue;	// don't need to clip against it

		clipped = BOX_ON_PLANE_SIDE (node->minmaxs, node->minmaxs + 3, clipplane);
		if (clipped == 2)
			return;
		else if (clipped == 1)
			clipflags -= (1<<c);	// node is entirely on screen
	}

// if a leaf node, draw stuff
	if (node->contents < 0)
	{
		pleaf = (mleaf_t *)node;

		c = pleaf - cl.worldmodel->leafs;
		frustumvis[c>>3] |= 1<<(c&7);

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c)
		{
			do
			{
				(*mark++)->visframe = r_framecount;
			} while (--c);
		}
		return;
	}

// node is just a decision point, so go down the apropriate sides

// find which side of the node we are on
	plane = node->plane;

	switch (plane->type)
	{
	case PLANE_X:
		dot = modelorg[0] - plane->dist;
		break;
	case PLANE_Y:
		dot = modelorg[1] - plane->dist;
		break;
	case PLANE_Z:
		dot = modelorg[2] - plane->dist;
		break;
	default:
		dot = DotProduct (modelorg, plane->normal) - plane->dist;
		break;
	}

	if (dot >= 0)
		side = 0;
	else
		side = 1;

// recurse down the children, front side first
	Surf_RecursiveWorldNode (node->children[side], clipflags);

// draw stuff
  	c = node->numsurfaces;

	if (c)
	{
		surf = cl.worldmodel->surfaces + node->firstsurface;

		if (dot < 0 -BACKFACE_EPSILON)
			side = SURF_PLANEBACK;
		else if (dot > BACKFACE_EPSILON)
			side = 0;
		{
			for ( ; c ; c--, surf++)
			{
				if (surf->visframe != r_framecount)
					continue;

				if (((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK)))
					continue;		// wrong side

				Surf_RenderDynamicLightmaps (surf);
				surf->sbatch->mesh[surf->sbatch->meshes++] = surf->mesh;
			}
		}
	}

// recurse down the back side
	//GLR_RecursiveWorldNode (node->children[!side], clipflags);
	node = node->children[!side];
	goto start;
}

static void Surf_OrthoRecursiveWorldNode (mnode_t *node, unsigned int clipflags)
{
	//when rendering as ortho the front and back sides are technically equal. the only culling comes from frustum culling.

	int			c, clipped;
	mplane_t	*clipplane;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;

	if (node->contents == Q1CONTENTS_SOLID)
		return;		// solid

	if (node->visframe != r_visframecount)
		return;

	for (c = 0, clipplane = frustum; c < 4; c++, clipplane++)
	{
		if (!(clipflags & (1 << c)))
			continue;	// don't need to clip against it

		clipped = BOX_ON_PLANE_SIDE (node->minmaxs, node->minmaxs + 3, clipplane);
		if (clipped == 2)
			return;
		else if (clipped == 1)
			clipflags -= (1<<c);	// node is entirely on screen
	}

// if a leaf node, draw stuff
	if (node->contents < 0)
	{
		pleaf = (mleaf_t *)node;

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c)
		{
			do
			{
				(*mark++)->visframe = r_framecount;
			} while (--c);
		}
		return;
	}

// recurse down the children
	Surf_OrthoRecursiveWorldNode (node->children[0], clipflags);
	Surf_OrthoRecursiveWorldNode (node->children[1], clipflags);

// draw stuff
  	c = node->numsurfaces;

	if (c)
	{
		surf = cl.worldmodel->surfaces + node->firstsurface;

		for ( ; c ; c--, surf++)
		{
			if (surf->visframe != r_framecount)
				continue;

			Surf_RenderDynamicLightmaps (surf);
			surf->sbatch->mesh[surf->sbatch->meshes++] = surf->mesh;
		}
	}
	return;
}

#ifdef Q2BSPS
static void Surf_RecursiveQ2WorldNode (mnode_t *node)
{
	int			c, side;
	mplane_t	*plane;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	double		dot;

	int sidebit;

	if (node->contents == Q2CONTENTS_SOLID)
		return;		// solid

	if (node->visframe != r_visframecount)
		return;
	if (R_CullBox (node->minmaxs, node->minmaxs+3))
		return;

// if a leaf node, draw stuff
	if (node->contents != -1)
	{
		pleaf = (mleaf_t *)node;

		// check for door connected areas
//		if (areabits)
		{
			if (! (areabits[pleaf->area>>3] & (1<<(pleaf->area&7)) ) )
				return;		// not visible
		}

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c)
		{
			do
			{
				(*mark)->visframe = r_framecount;
				mark++;
			} while (--c);
		}
		return;
	}

// node is just a decision point, so go down the apropriate sides

// find which side of the node we are on
	plane = node->plane;

	switch (plane->type)
	{
	case PLANE_X:
		dot = modelorg[0] - plane->dist;
		break;
	case PLANE_Y:
		dot = modelorg[1] - plane->dist;
		break;
	case PLANE_Z:
		dot = modelorg[2] - plane->dist;
		break;
	default:
		dot = DotProduct (modelorg, plane->normal) - plane->dist;
		break;
	}

	if (dot >= 0)
	{
		side = 0;
		sidebit = 0;
	}
	else
	{
		side = 1;
		sidebit = SURF_PLANEBACK;
	}

// recurse down the children, front side first
	Surf_RecursiveQ2WorldNode (node->children[side]);

	// draw stuff
	for ( c = node->numsurfaces, surf = currentmodel->surfaces + node->firstsurface; c ; c--, surf++)
	{
		if (surf->visframe != r_framecount)
			continue;

		if ( (surf->flags & SURF_PLANEBACK) != sidebit )
			continue;		// wrong side

		surf->visframe = r_framecount+1;//-1;

		Surf_RenderDynamicLightmaps (surf);

		surf->sbatch->mesh[surf->sbatch->meshes++] = surf->mesh;
	}


// recurse down the back side
	Surf_RecursiveQ2WorldNode (node->children[!side]);
}
#endif

#ifdef Q3BSPS
#if 0
static void Surf_LeafWorldNode (void)
{
	int			i;
	int			clipflags;
	msurface_t	**mark, *surf;
	mleaf_t		*pleaf;


	int clipped;
	mplane_t *clipplane;


	for (pleaf = r_vischain; pleaf; pleaf = pleaf->vischain)
	{
		// check for door connected areas
//		if (areabits)
		{
//			if (!(areabits[pleaf->area>>3] & (1<<(pleaf->area&7))))
//			{
//				continue;		// not visible
//			}
		}

		clipflags = 15;		// 1 | 2 | 4 | 8
//		if (!r_nocull->value)
		{

			for (i=0,clipplane=frustum ; i<FRUSTUMPLANES ; i++,clipplane++)
			{
				clipped = BoxOnPlaneSide (pleaf->minmaxs, pleaf->minmaxs+3, clipplane);
				if (clipped == 2)
				{
					break;
				}
				else if (clipped == 1)
				{
					clipflags &= ~(1<<i);	// node is entirely on screen
				}
			}

			if (i != FRUSTUMPLANES)
			{
				continue;
			}
		}

		i = pleaf->nummarksurfaces;
		mark = pleaf->firstmarksurface;

		do
		{
			surf = *mark++;
			if (surf->visframe != r_framecount)	//sufraces exist in multiple leafs.
			{
				surf->visframe = r_framecount;
				if (surf->mark)
					*surf->mark = surf;
			}
		} while (--i);

//		c_world_leafs++;
	}



	{
		int j;
		texture_t *tex;
		for (i = 0; i < cl.worldmodel->numtextures; i++)
		{
			tex = cl.worldmodel->textures[i];
			if (!tex)
				continue;
			for (j = 0; j < tex->vbo.meshcount; j++)
			{
				surf = tex->vbo.meshlist[j];
				if (surf)
				{
					tex->vbo.meshlist[j] = NULL;
					surf->sbatch->mesh[surf->sbatch->meshes++] = surf->mesh;
				}
			}
		}
	}
}
#endif

static void Surf_RecursiveQ3WorldNode (mnode_t *node, unsigned int clipflags)
{
	int			c, side, clipped;
	mplane_t	*plane, *clipplane;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	double		dot;

start:

	if (node->visframe != r_visframecount)
		return;

	for (c = 0, clipplane = frustum; c < FRUSTUMPLANES; c++, clipplane++)
	{
		if (!(clipflags & (1 << c)))
			continue;	// don't need to clip against it

		clipped = BOX_ON_PLANE_SIDE (node->minmaxs, node->minmaxs + 3, clipplane);
		if (clipped == 2)
			return;
		else if (clipped == 1)
			clipflags -= (1<<c);	// node is entirely on screen
	}

// if a leaf node, draw stuff
	if (node->contents != -1)
	{
		pleaf = (mleaf_t *)node;

		if (! (areabits[pleaf->area>>3] & (1<<(pleaf->area&7)) ) )
			return;		// not visible

		mark = pleaf->firstmarksurface;
		for (c = pleaf->nummarksurfaces; c; c--)
		{
			surf = *mark++;
			if (surf->visframe == r_framecount)
				continue;
			surf->visframe = r_framecount;

//			if (((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK)))
//				continue;		// wrong side

			surf->sbatch->mesh[surf->sbatch->meshes++] = surf->mesh;
		}
		return;
	}

// node is just a decision point, so go down the apropriate sides

// find which side of the node we are on
	plane = node->plane;

	switch (plane->type)
	{
	case PLANE_X:
		dot = modelorg[0] - plane->dist;
		break;
	case PLANE_Y:
		dot = modelorg[1] - plane->dist;
		break;
	case PLANE_Z:
		dot = modelorg[2] - plane->dist;
		break;
	default:
		dot = DotProduct (modelorg, plane->normal) - plane->dist;
		break;
	}

	if (dot >= 0)
		side = 0;
	else
		side = 1;

// recurse down the children, front side first
	Surf_RecursiveQ3WorldNode (node->children[side], clipflags);

// q3 nodes contain no drawables

// recurse down the back side
	//GLR_RecursiveWorldNode (node->children[!side], clipflags);
	node = node->children[!side];
	goto start;
}
#endif

static void Surf_CleanChains(void)
{
	model_t *model = cl.worldmodel;
	batch_t *batch;
	int i;

	if (r_refdef.recurse)
	{
		for (i = 0; i < SHADER_SORT_COUNT; i++)
		for (batch = model->batches[i]; batch; batch = batch->next)
		{
			batch->meshes = batch->firstmesh;
		}
	}
	else
	{
		for (i = 0; i < SHADER_SORT_COUNT; i++)
		for (batch = model->batches[i]; batch; batch = batch->next)
		{
			batch->meshes = batch->firstmesh;
		}
	}
}

//most of this is a direct copy from gl
void Surf_SetupFrame(void)
{
	mleaf_t	*leaf;
	vec3_t	temp, pvsorg;

	R_AnimateLight();
	r_framecount++;

	r_viewcontents = 0;
	if (r_refdef.flags & Q2RDF_NOWORLDMODEL)
	{
	}
#ifdef Q2BSPS
	else if (cl.worldmodel && (cl.worldmodel->fromgame == fg_quake2 || cl.worldmodel->fromgame == fg_quake3))
	{
		static mleaf_t fakeleaf;
		mleaf_t	*leaf;

		r_viewleaf = &fakeleaf;	//so we can use quake1 rendering routines for q2 bsps.
		r_viewleaf->contents = Q1CONTENTS_EMPTY;
		r_viewleaf2 = NULL;

		r_oldviewcluster = r_viewcluster;
		r_oldviewcluster2 = r_viewcluster2;
		if (r_refdef.recurse)
			leaf = RMod_PointInLeaf (cl.worldmodel, r_refdef.pvsorigin);
		else
			leaf = RMod_PointInLeaf (cl.worldmodel, r_origin);
		r_viewcluster = r_viewcluster2 = leaf->cluster;

		r_viewcontents = leaf->contents & (FTECONTENTS_LAVA|FTECONTENTS_SLIME|FTECONTENTS_WATER);

		// check above and below so crossing solid water doesn't draw wrong
		if (!leaf->contents)
		{	// look down a bit
			vec3_t	temp;

			VectorCopy (r_origin, temp);
			temp[2] -= 16;
			leaf = RMod_PointInLeaf (cl.worldmodel, temp);
			if ( !(leaf->contents & Q2CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2) )
				r_viewcluster2 = leaf->cluster;
		}
		else
		{	// look up a bit
			vec3_t	temp;

			VectorCopy (r_origin, temp);
			temp[2] += 16;
			leaf = RMod_PointInLeaf (cl.worldmodel, temp);
			if ( !(leaf->contents & Q2CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2) )
				r_viewcluster2 = leaf->cluster;
		}
	}
#endif
	else if (cl.worldmodel && cl.worldmodel->fromgame == fg_doom3)
	{
		r_viewleaf = NULL;
		r_viewleaf2 = NULL;
	}
	else
	{
		if (r_refdef.recurse)
		{
			VectorCopy(r_refdef.pvsorigin, pvsorg);
		}
		else
		{
			VectorCopy(r_origin, pvsorg);
		}

		r_viewleaf = RMod_PointInLeaf (cl.worldmodel, pvsorg);

		if (!r_viewleaf)
		{
		}
		else if (r_viewleaf->contents == Q1CONTENTS_EMPTY)
		{	//look down a bit
			VectorCopy (pvsorg, temp);
			temp[2] -= 16;
			leaf = RMod_PointInLeaf (cl.worldmodel, temp);
			if (leaf->contents <= Q1CONTENTS_WATER && leaf->contents >= Q1CONTENTS_LAVA)
				r_viewleaf2 = leaf;
			else
				r_viewleaf2 = NULL;
		}
		else if (r_viewleaf->contents <= Q1CONTENTS_WATER && r_viewleaf->contents >= Q1CONTENTS_LAVA)
		{	//in water, look up a bit.

			VectorCopy (pvsorg, temp);
			temp[2] += 16;
			leaf = RMod_PointInLeaf (cl.worldmodel, temp);
			if (leaf->contents == Q1CONTENTS_EMPTY)
				r_viewleaf2 = leaf;
			else
				r_viewleaf2 = NULL;
		}
		else
			r_viewleaf2 = NULL;

		if (r_viewleaf)
		{
			switch(r_viewleaf->contents)
			{
			case Q1CONTENTS_WATER:
				r_viewcontents |= FTECONTENTS_WATER;
				break;
			case Q1CONTENTS_LAVA:
				r_viewcontents |= FTECONTENTS_LAVA;
				break;
			case Q1CONTENTS_SLIME:
				r_viewcontents |= FTECONTENTS_SLIME;
				break;
			case Q1CONTENTS_SKY:
				r_viewcontents |= FTECONTENTS_SKY;
				break;
			case Q1CONTENTS_SOLID:
				r_viewcontents |= FTECONTENTS_SOLID;
				break;
			}
		}
	}

#ifdef TERRAIN
	if (!(r_refdef.flags & Q2RDF_NOWORLDMODEL) && cl.worldmodel && cl.worldmodel->terrain)
	{
		r_viewcontents |= Heightmap_PointContents(cl.worldmodel, NULL, r_origin);
	}
#endif

	/*pick up any extra water entities*/
	{
		extern vec3_t player_maxs, player_mins;
		vec3_t t1,t2;
		VectorCopy(player_mins, t1);
		VectorCopy(player_maxs, t2);
		VectorClear(player_maxs);
		VectorClear(player_mins);
		r_viewcontents |= PM_ExtraBoxContents(r_origin);
		VectorCopy(t1, player_mins);
		VectorCopy(t2, player_maxs);
	}
	V_SetContentsColor (r_viewcontents);
}

/*
static mesh_t *surfbatchmeshes[256];
static void Surf_BuildBrushBatch(batch_t *batch)
{
	model_t *model = batch->ent->model;
	unsigned int i;
	batch->mesh = surfbatchmeshes;
	batch->meshes = batch->surf_count;
	for (i = 0; i < batch->surf_count; i++)
	{
		surfbatchmeshes[i] = model->surfaces[batch->surf_first + i].mesh;
	}
}
*/

void Surf_GenBrushBatches(batch_t **batches, entity_t *ent)
{
	int i;
	msurface_t *s;
	batch_t *ob;
	model_t *model;
	batch_t *b;
	unsigned int bef;

	model = ent->model;

	if (R_CullEntityBox (ent, model->mins, model->maxs))
		return;

#ifdef RTLIGHTS
	if (BE_LightCullModel(ent->origin, model))
		return;
#endif

// calculate dynamic lighting for bmodel if it's not an
// instanced model
	if (model->fromgame != fg_quake3 && model->fromgame != fg_doom3)
	{
		int k;

		currententity = ent;
		currentmodel = ent->model;
		if (model->nummodelsurfaces != 0 && r_dynamic.ival)
		{
			for (k=rtlights_first; k<RTL_FIRST; k++)
			{
				if (!cl_dlights[k].radius)
					continue;
				if (!(cl_dlights[k].flags & LFLAG_LIGHTMAP))
					continue;

				model->funcs.MarkLights (&cl_dlights[k], 1<<k,
					model->nodes + model->hulls[0].firstclipnode);
			}
		}

		Surf_LightmapShift(model);
		if ((ent->drawflags & MLS_MASKIN) == MLS_ABSLIGHT)
		{
			//update lightmaps.
			for (s = model->surfaces+model->firstmodelsurface,i = 0; i < model->nummodelsurfaces; i++, s++)
				Surf_RenderAmbientLightmaps (s, ent->abslight);
		}
		else if (ent->drawflags & DRF_TRANSLUCENT)
		{
			//update lightmaps.
			for (s = model->surfaces+model->firstmodelsurface,i = 0; i < model->nummodelsurfaces; i++, s++)
				Surf_RenderAmbientLightmaps (s, 255);
		}
		else
		{
			//update lightmaps.
			for (s = model->surfaces+model->firstmodelsurface,i = 0; i < model->nummodelsurfaces; i++, s++)
				Surf_RenderDynamicLightmaps (s);
		}
		currententity = NULL;
	}

	bef = BEF_PUSHDEPTH;
	if (ent->flags & Q2RF_ADDITIVE)
		bef |= BEF_FORCEADDITIVE;
	else if (ent->drawflags & DRF_TRANSLUCENT && r_wateralpha.value != 1)
	{
		bef |= BEF_FORCETRANSPARENT;
		ent->shaderRGBAf[3] = r_wateralpha.value;
	}
	else if (ent->flags & Q2RF_TRANSLUCENT && cls.protocol != CP_QUAKE3)
		bef |= BEF_FORCETRANSPARENT;
	if (ent->flags & RF_NODEPTHTEST)
		bef |= BEF_FORCENODEPTH;
	if (ent->flags & RF_NOSHADOW)
		bef |= BEF_NOSHADOWS;

	for (i = 0; i < SHADER_SORT_COUNT; i++)
	for (ob = model->batches[i]; ob; ob = ob->next)
	{
		b = BE_GetTempBatch();
		if (!b)
			continue;
		*b = *ob;
		b->shader = R_TextureAnimation(ent->framestate.g[FS_REG].frame[0], b->texture)->shader;
		b->meshes = b->maxmeshes;
		b->ent = ent;
		b->flags = bef;

		if (bef & BEF_FORCEADDITIVE)
		{
			b->next = batches[SHADER_SORT_ADDITIVE];
			batches[SHADER_SORT_ADDITIVE] = b;
		}
		else if (bef & BEF_FORCETRANSPARENT)
		{
			b->next = batches[SHADER_SORT_BLEND];
			batches[SHADER_SORT_BLEND] = b;
		}
		else
		{
			b->next = batches[b->shader->sort];
			batches[b->shader->sort] = b;
		}
	}
}

/*
=============
R_DrawWorld
=============
*/

void Surf_DrawWorld (void)
{
	//surfvis vs entvis - the key difference is that surfvis is surfaces while entvis is volume. though surfvis should be frustum culled also for lighting. entvis doesn't care.
	qbyte *surfvis, *entvis;
	qbyte frustumvis_[MAX_MAP_LEAFS/8];
	RSpeedLocals();

	if (r_refdef.flags & Q2RDF_NOWORLDMODEL)
	{
		r_refdef.flags |= Q2RDF_NOWORLDMODEL;
		BE_DrawWorld(false, NULL);
		return;
	}
	if (!cl.worldmodel || cl.worldmodel->needload)
	{
		/*Don't act as a wallhack*/
		return;
	}

	Surf_SetupFrame();

	currentmodel = cl.worldmodel;
	currententity = &r_worldentity;

	{
		RSpeedRemark();

		Surf_LightmapShift(cl.worldmodel);

#ifdef Q2BSPS
		if (cl.worldmodel->fromgame == fg_quake2 || cl.worldmodel->fromgame == fg_quake3)
		{
			int leafnum;
			int clientarea;
#ifdef Q2CLIENT
			if (cls.protocol == CP_QUAKE2)	//we can get server sent info
			{
				memcpy(areabits, cl.q2frame.areabits, sizeof(areabits));
			}
			else
#endif
			{	//generate the info each frame.
				leafnum = CM_PointLeafnum (cl.worldmodel, r_refdef.vieworg);
				clientarea = CM_LeafArea (cl.worldmodel, leafnum);
				CM_WriteAreaBits(cl.worldmodel, areabits, clientarea);
			}
#ifdef Q3BSPS
			if (currententity->model->fromgame == fg_quake3)
			{
				entvis = surfvis = R_MarkLeaves_Q3 ();
				Surf_RecursiveQ3WorldNode (cl.worldmodel->nodes, (1<<FRUSTUMPLANES)-1);
				//Surf_LeafWorldNode ();
			}
			else
#endif
			{
				entvis = surfvis = R_MarkLeaves_Q2 ();
				VectorCopy (r_refdef.vieworg, modelorg);
				Surf_RecursiveQ2WorldNode (cl.worldmodel->nodes);
			}
		}
		else
#endif
#ifdef MAP_PROC
		     if (cl.worldmodel->fromgame == fg_doom3)
		{
			entvis = surfvis = D3_CalcVis(cl.worldmodel, r_origin);
		}
		else
#endif
#ifdef MAP_DOOM
			if (currentmodel->fromgame == fg_doom)
		{
			entvis = surfvis = NULL;
			GLR_DoomWorld();
		}
		else
#endif
#ifdef TERRAIN
		if (currentmodel->type == mod_heightmap)
		{
			entvis = surfvis = NULL;
		}
		else
#endif
		{
			//extern cvar_t temp1;
//			if (0)//temp1.value)
//				entvis = surfvis = R_MarkLeafSurfaces_Q1();
//			else
			{
				entvis = R_MarkLeaves_Q1 ();
				if (!(r_novis.ival & 2))
					VectorCopy (r_origin, modelorg);

				frustumvis = frustumvis_;
				memset(frustumvis, 0, (cl.worldmodel->numleafs + 7)>>3);

				if (r_refdef.useperspective)
					Surf_RecursiveWorldNode (cl.worldmodel->nodes, 0x1f);
				else
					Surf_OrthoRecursiveWorldNode (cl.worldmodel->nodes, 0x1f);
				surfvis = frustumvis;
			}
		}

		if (!(r_refdef.flags & Q2RDF_NOWORLDMODEL))
		{
			CL_LinkStaticEntities(entvis);
			TRACE(("dbg: calling R_DrawParticles\n"));
			if (!r_refdef.recurse)
				P_DrawParticles ();
		}

		RSpeedEnd(RSPEED_WORLDNODE);
		TRACE(("dbg: calling BE_DrawWorld\n"));
		BE_DrawWorld(true, surfvis);

		/*FIXME: move this away*/
		if (cl.worldmodel->fromgame == fg_quake || cl.worldmodel->fromgame == fg_halflife)
			Surf_LessenStains();

		Surf_CleanChains();
	}
}

unsigned int Surf_CalcMemSize(msurface_t *surf)
{
	if (surf->mesh)
		return 0;

	if (!surf->numedges)
		return 0;

	//figure out how much space this surface needs
	return sizeof(mesh_t) + 
	sizeof(index_t)*(surf->numedges-2)*3 +
	(sizeof(vecV_t)+sizeof(vec2_t)*2+sizeof(vec3_t)*3+sizeof(vec4_t))*surf->numedges;
}

void Surf_DeInit(void)
{
	int i;

	for (i = 0; i < numlightmaps; i++)
	{
		if (!lightmap[i])
			break;
		if (!lightmap[i]->external)
		{
			R_DestroyTexture(lightmap[i]->lightmap_texture);
		}
		BZ_Free(lightmap[i]);
		lightmap[i] = NULL;
	}

	if (lightmap)
		BZ_Free(lightmap);

	lightmap=NULL;
	numlightmaps=0;

	Alias_Shutdown();
}

void Surf_Clear(model_t *mod)
{
	vbo_t *vbo;
	if (mod->fromgame == fg_doom3)
		return;/*they're on the hunk*/
	while(mod->vbos)
	{
		vbo = mod->vbos;
		mod->vbos = vbo->next;
		BE_ClearVBO(vbo);
	}

	BZ_Free(mod->shadowbatches);
	mod->numshadowbatches = 0;
	mod->shadowbatches = NULL;
#ifdef RTLIGHTS
	Sh_PurgeShadowMeshes();
#endif
}

//pick fastest mode for lightmap data
void Surf_LightmapMode(void)
{
	lightmap_bgra = true;

	switch(qrenderer)
	{
	case QR_SOFTWARE:
		lightmap_bytes = 4;
		lightmap_bgra = true;
		break;
#ifdef D3DQUAKE
	case QR_DIRECT3D9:
	case QR_DIRECT3D11:
		/*always bgra, hope your card supports it*/
		lightmap_bytes = 4;
		lightmap_bgra = true;
		break;
#endif
	case QR_OPENGL:
#ifdef GLQUAKE
		/*favour bgra if the gpu supports it, otherwise use rgb only if it'll be used*/
		lightmap_bgra = false;
		if (gl_config.gles)
		{
			lightmap_bytes = 3;
			lightmap_bgra = false;
		}
		else if (gl_config.glversion >= 1.2)
		{
			/*the more common case*/
			lightmap_bytes = 4;
			lightmap_bgra = true;
		}
		else if (cl.worldmodel->fromgame == fg_quake3 || (cl.worldmodel->engineflags & MDLF_RGBLIGHTING) || cl.worldmodel->deluxdata || r_loadlits.value)
		{
			lightmap_bgra = false;
			lightmap_bytes = 3;
		}
		else
			lightmap_bytes = 1;
		break;
#endif
	default:
		break;
	}
}

//needs to be followed by a BE_UploadAllLightmaps at some point
int Surf_NewLightmaps(int count, int width, int height, qboolean deluxe)
{
	int first = numlightmaps;
	int i;

	if (!count)
		return -1;

	if (deluxe && (count & 1))
	{
		deluxe = false;
		Con_Print("WARNING: Deluxemapping with odd number of lightmaps\n");
	}

	i = numlightmaps + count;
	lightmap = BZ_Realloc(lightmap, sizeof(*lightmap)*(i));
	while(i > first)
	{
		i--;

		if (deluxe && ((i - numlightmaps)&1))
		{
			lightmap[i] = Z_Malloc(sizeof(*lightmap[i]) + (sizeof(qbyte)*8)*width*height);
			lightmap[i]->width = width;
			lightmap[i]->height = height;
			lightmap[i]->lightmaps = (qbyte*)(lightmap[i]+1);
			lightmap[i]->stainmaps = NULL;
			lightmap[i]->hasdeluxe = false;
		}
		else
		{
			lightmap[i] = Z_Malloc(sizeof(*lightmap[i]) + (sizeof(qbyte)*8 + sizeof(stmap)*3)*width*height);
			lightmap[i]->width = width;
			lightmap[i]->height = height;
			lightmap[i]->lightmaps = (qbyte*)(lightmap[i]+1);
			lightmap[i]->stainmaps = (stmap*)(lightmap[i]->lightmaps+4*width*height);
			lightmap[i]->hasdeluxe = deluxe;
		}

		lightmap[i]->rectchange.l = 0;
		lightmap[i]->rectchange.t = 0;
		lightmap[i]->rectchange.h = LMBLOCK_WIDTH;
		lightmap[i]->rectchange.w = LMBLOCK_WIDTH;


		lightmap[i]->lightmap_texture = r_nulltex;
		lightmap[i]->modified = true;
//			lightmap[i]->shader = NULL;
		lightmap[i]->external = false;
		// reset stainmap since it now starts at 255
		if (lightmap[i]->stainmaps)
			memset(lightmap[i]->stainmaps, 255, width*height*3*sizeof(stmap));
	}

	numlightmaps += count;

	return first;
}
int Surf_NewExternalLightmaps(int count, char *filepattern, qboolean deluxe)
{
	int first = numlightmaps;
	int i;
	char nname[MAX_QPATH];

	if (!count)
		return -1;

	i = numlightmaps + count;
	lightmap = BZ_Realloc(lightmap, sizeof(*lightmap)*(i));
	while(i > first)
	{
		i--;

		lightmap[i] = Z_Malloc(sizeof(*lightmap[i]));
		lightmap[i]->width = 0;
		lightmap[i]->height = 0;
		lightmap[i]->lightmaps = NULL;
		lightmap[i]->stainmaps = NULL;

		lightmap[i]->modified = false;
		lightmap[i]->external = true;
		lightmap[i]->hasdeluxe = (deluxe && ((i - numlightmaps)&1));

		Q_snprintfz(nname, sizeof(nname), filepattern, i - numlightmaps);

		TEXASSIGN(lightmap[i]->lightmap_texture, R_LoadHiResTexture(nname, NULL, 0));
		lightmap[i]->width = image_width;
		lightmap[i]->height = image_height;
	}

	numlightmaps += count;

	return first;
}

void Surf_BuildModelLightmaps (model_t *m)
{
	int		i, t;
	int shift;
	msurface_t *surf;
	batch_t *batch;
	int sortid;
	int ptype;
	int newfirst;

	if (!lightmap_bytes)
		return;

#ifdef TERRAIN
	if (m->terrain)
		Terr_PurgeTerrainModel(m, true, false);
#endif

	if (m->type != mod_brush)
		return;

	if (!m->lightmaps.count)
		return;
	if (m->needload)
		return;

	currentmodel = m;
	shift = Surf_LightmapShift(currentmodel);

	if (*m->name == '*' && m->fromgame == fg_quake3)	//FIXME: should be all bsp formats
		newfirst = cl.model_precache[1]->lightmaps.first;
	else
	{
		if (!m->lightdata && m->lightmaps.count && m->fromgame == fg_quake3)
		{
			char pattern[MAX_QPATH];
			COM_StripAllExtensions(m->name, pattern, sizeof(pattern));
			Q_strncatz(pattern, "/lm_%04u.tga", sizeof(pattern));
			newfirst = Surf_NewExternalLightmaps(m->lightmaps.count, pattern, m->lightmaps.deluxemapping);
		}
		else
			newfirst = Surf_NewLightmaps(m->lightmaps.count, m->lightmaps.width, m->lightmaps.height, m->lightmaps.deluxemapping);
	}

	//fixup batch lightmaps
	for (sortid = 0; sortid < SHADER_SORT_COUNT; sortid++)
	for (batch = m->batches[sortid]; batch != NULL; batch = batch->next)
	{
		for (i = 0; i < MAXLIGHTMAPS; i++)
		{
			if (batch->lightmap[i] < 0)
				continue;
			batch->lightmap[i] = batch->lightmap[i] - m->lightmaps.first + newfirst;
		}
	}

	
	/*particle emision based upon texture. this is lazy code*/
	if (m == cl.worldmodel)
	{
		for (t = m->numtextures-1; t >= 0; t--)
		{
			char *pn = va("tex_%s", m->textures[t]->name);
			char *h = strchr(pn, '#');
			if (h)
				*h = 0;
			ptype = P_FindParticleType(pn);
		
			if (ptype != P_INVALID)
			{
				for (i=0; i<m->nummodelsurfaces; i++)
				{
					surf = m->surfaces + i + m->firstmodelsurface;
					if (surf->texinfo->texture == m->textures[t])
						P_EmitSkyEffectTris(m, surf, ptype);
				}
			}
		}
	}


	if (m->fromgame == fg_quake3)
	{
		int j;
		unsigned char *src;
		unsigned char *dst;
		for (i = 0; i < m->lightmaps.count; i++)
		{
			if (lightmap[newfirst+i]->external)
				continue;

			dst = lightmap[newfirst+i]->lightmaps;
			src = m->lightdata + i*m->lightmaps.width*m->lightmaps.height*3;
			if (lightmap_bytes == 4 && m->lightdata)
			{
				if (lightmap_bgra)
				{
					for (j = 0; j < m->lightmaps.width*m->lightmaps.height; j++, dst += 4, src += 3)
					{
						dst[0] = src[2];
						dst[1] = src[1];
						dst[2] = src[0];
						dst[3] = 255;
					}
				}
				else
				{
					for (j = 0; j < m->lightmaps.width*m->lightmaps.height; j++, dst += 4, src += 3)
					{
						dst[0] = src[0];
						dst[1] = src[1];
						dst[2] = src[2];
						dst[3] = 255;
					}
				}
			}
		}
	}
	else
	{
		int j;
		lightmapinfo_t *lm, *dlm;
		qbyte *deluxemap;
		//fixup surface lightmaps, and paint
		for (i=0; i<m->nummodelsurfaces; i++)
		{
			surf = m->surfaces + i + m->firstmodelsurface;
			for (j = 0; j < 4; j++)
			{
				if (surf->lightmaptexturenums[j] < m->lightmaps.first)
				{
					surf->lightmaptexturenums[j] = -1;
					continue;
				}
				surf->lightmaptexturenums[j] = surf->lightmaptexturenums[0] - m->lightmaps.first + newfirst;

				lm = lightmap[surf->lightmaptexturenums[j]];
				if (lm->hasdeluxe)
				{
					dlm = lightmap[surf->lightmaptexturenums[j]+1];
					deluxemap = dlm->lightmaps + (surf->light_t[j] * dlm->width + surf->light_s[j]) * lightmap_bytes;
				}
				else
					deluxemap = NULL;

				Surf_BuildLightMap (surf, 
					lm->lightmaps + (surf->light_t[j] * lm->width + surf->light_s[j]) * lightmap_bytes,
					deluxemap,
					lm->stainmaps + (surf->light_t[j] * lm->width + surf->light_s[j]) * 3,
					shift, r_ambient.value*255);
			}
		}
	}
	m->lightmaps.first = newfirst;
}

void Surf_ClearLightmaps(void)
{
	lightmap_bytes = 0;
}

/*
==================
GL_BuildLightmaps

Builds the lightmap texture
with all the surfaces from all brush models
Groups surfaces into their respective batches (based on the lightmap number).
==================
*/
void Surf_BuildLightmaps (void)
{
	int		i, j;
	model_t	*m;

	r_framecount = 1;		// no dlightcache

	for (i = 0; i < numlightmaps; i++)
	{
		if (!lightmap[i])
			break;
		BZ_Free(lightmap[i]);
		lightmap[i] = NULL;
	}

	Surf_LightmapMode();

	r_oldviewleaf = NULL;
	r_oldviewleaf2 = NULL;
	r_oldviewcluster = -1;
	r_oldviewcluster2 = -1;
	numlightmaps = 0;

	if (cl.worldmodel->fromgame == fg_doom)
		return;	//no lightmaps.

	for (j=1 ; j<MAX_MODELS ; j++)
	{
		m = cl.model_precache[j];
		if (!m)
			break;
		Surf_BuildModelLightmaps(m);
	}
	for (j=1 ; j<MAX_CSMODELS ; j++)
	{
		m = cl.model_csqcprecache[j];
		if (!m)
			break;
		Surf_BuildModelLightmaps(m);
	}
	BE_UploadAllLightmaps();
}
#endif
