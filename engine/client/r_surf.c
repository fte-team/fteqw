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
#ifndef SERVERONLY
#include "glquake.h"
#include "shader.h"
#include "renderque.h"
#include "com_mesh.h"
#include <math.h>

#if (defined(GLQUAKE) || defined(VKQUAKE)) && defined(MULTITHREAD)
#define THREADEDWORLD
#endif
#ifdef BEF_PUSHDEPTH
qboolean r_pushdepth;
#endif

extern cvar_t		r_ambient;

static vec3_t		modelorg;	/*set before recursively entering the visible surface finder*/

model_t				*currentmodel;

static size_t		maxblocksize;
static vec3_t		*blocknormals;
static unsigned		*blocklights;

lightmapinfo_t **lightmap;
int numlightmaps;
extern const float rgb9e5tab[32];

extern mleaf_t		*r_vischain;		// linked list of visible leafs

extern cvar_t r_stains;
extern cvar_t r_loadlits;
extern cvar_t r_stainfadetime;
extern cvar_t r_stainfadeammount;
extern cvar_t r_lightmap_nearest;
extern cvar_t r_lightmap_format;

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

void QDECL Surf_RebuildLightmap_Callback (struct cvar_s *var, char *oldvalue)
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

	smax = (surf->extents[0]>>surf->lmshift)+1;
	tmax = (surf->extents[1]>>surf->lmshift)+1;
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
		td = local[1] - (t<<surf->lmshift);
		if (td < 0)
			td = -td;
		for (s=0 ; s<smax ; s++)
		{
			sd = local[0] - (s<<surf->lmshift);
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
	if (!cl.worldmodel || cl.worldmodel->loadstate != MLS_LOADED || r_stains.value <= 0)
		return;
	parms[0] = radius;
	parms[1] = org[0];
	parms[2] = org[1];
	parms[3] = org[2];
	parms[4] = red;
	parms[5] = green;
	parms[6] = blue;


	cl.worldmodel->funcs.StainNode(cl.worldmodel->rootnode, parms);

	//now stain inline bsp models other than world.

	for (i=1 ; i< pmove.numphysent ; i++)	//0 is world...
	{
		pe = &pmove.physents[i];
		if (pe->model && pe->model->surfaces == cl.worldmodel->surfaces && pe->model->loadstate == MLS_LOADED)
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


			pe->model->funcs.StainNode(pe->model->rootnode, parms);
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
		memset(lightmap[i]->stainmaps, 255, lightmap[i]->width*lightmap[i]->height*3*sizeof(stmap));
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

	if (!r_stains.value || !r_stainfadeammount.value)
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

			smax = (surf->extents[0]>>surf->lmshift)+1;
			tmax = (surf->extents[1]>>surf->lmshift)+1;

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

	smax = (surf->extents[0]>>surf->lmshift)+1;
	tmax = (surf->extents[1]>>surf->lmshift)+1;
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
			td = local[1] - (t<<surf->lmshift);
			if (td < 0)
				td = -td;
			for (s=0 ; s<smax ; s++)
			{
				sd = local[0] - (s<<surf->lmshift);
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
			td = local[1] - t*surf->lmscale;
			if (td < 0)
				td = -td;
			for (s=0 ; s<smax ; s++)
			{
				sd = local[0] - s*surf->lmscale;
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

	smax = (surf->extents[0]>>surf->lmshift)+1;
	tmax = (surf->extents[1]>>surf->lmshift)+1;
	tex = surf->texinfo;

	for (lnum=rtlights_first; lnum<RTL_FIRST; lnum++)
	{
		if ( !(surf->dlightbits & (1<<lnum) ) )
			continue;		// not lit by this light

		rad = cl_dlights[lnum].radius;
		VectorSubtract(cl_dlights[lnum].origin, currententity->origin, lightofs);
		//FIXME: transform by forward/right/up
		dist = DotProduct (lightofs, surf->plane->normal) - surf->plane->dist;
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

		if (r_dynamic.ival == 2)
			r = g = b = 256;
		else
		{
			r = cl_dlights[lnum].color[0]*128;
			g = cl_dlights[lnum].color[1]*128;
			b = cl_dlights[lnum].color[2]*128;
		}

		bl = blocklights;
		if (r < 0 || g < 0 || b < 0)
		{
			for (t = 0 ; t<tmax ; t++)
			{
				td = local[1] - (t<<surf->lmshift);
				if (td < 0)
					td = -td;
				for (s=0 ; s<smax ; s++)
				{
					sd = local[0] - (s<<surf->lmshift);
					if (sd < 0)
						sd = -sd;
					if (sd > td)
						dist = sd + (td>>1);
					else
						dist = td + (sd>>1);
					if (dist < minlight)
					{
						i = bl[0] + (rad - dist)*r;
						bl[0] = (i<0)?0:i;
						i = bl[1] + (rad - dist)*g;
						bl[1] = (i<0)?0:i;
						i = bl[2] + (rad - dist)*b;
						bl[2] = (i<0)?0:i;
					}
					bl += 3;
				}
			}
		}
		else
		{
			for (t = 0 ; t<tmax ; t++)
			{
				td = local[1] - (t<<surf->lmshift);
				if (td < 0)
					td = -td;
				for (s=0 ; s<smax ; s++)
				{
					sd = local[0] - (s<<surf->lmshift);
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



static void Surf_BuildDeluxMap (model_t *wmodel, msurface_t *surf, qbyte *dest, lightmapinfo_t *lm, vec3_t *blocknormals)
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

	int stride;

	if (!dest)
		return;

	smax = (surf->extents[0]>>surf->lmshift)+1;
	tmax = (surf->extents[1]>>surf->lmshift)+1;
	size = smax*tmax;
	lightmap = surf->samples;

	// set to full bright if no light data
	if (!wmodel->deluxdata)
	{
		for (i=0 ; i<size ; i++)
		{
			blocknormals[i][0] = 0.9;//surf->orientation[2][0];
			blocknormals[i][1] = 0.8;//surf->orientation[2][1];
			blocknormals[i][2] = 1;//surf->orientation[2][2];
		}
		goto store;
	}

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
		switch(wmodel->lightmaps.fmt)
		{
		case LM_E5BGR9:
			deluxmap = ((surf->samples - wmodel->lightdata)/4)*3 + wmodel->deluxdata;
			for (maps = 0 ; maps < MAXQ1LIGHTMAPS && surf->styles[maps] != INVALID_LIGHTSTYLE ; maps++)
			{
				scale = d_lightstylevalue[surf->styles[maps]];
				for (i=0 ; i<size ; i++)
				{
					unsigned lm = ((unsigned int*)lightmap)[i];
					intensity = max3(((lm>>0)&0x1ff),((lm>>9)&0x1ff),((lm>>18)&0x1ff)) * scale * (rgb9e5tab[lm>>27]*(1<<7));
					blocknormals[i][0] += intensity*(deluxmap[i*3+0]-127);
					blocknormals[i][1] += intensity*(deluxmap[i*3+1]-127);
					blocknormals[i][2] += intensity*(deluxmap[i*3+2]-127);
				}
				lightmap += size*4;	// skip to next lightmap
				deluxmap += size*3;
			}
			break;
		case LM_RGB8:
			deluxmap = surf->samples - wmodel->lightdata + wmodel->deluxdata;
			for (maps = 0 ; maps < MAXQ1LIGHTMAPS && surf->styles[maps] != INVALID_LIGHTSTYLE ; maps++)
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
			break;
		case LM_L8:
			deluxmap = (surf->samples - wmodel->lightdata)*3 + wmodel->deluxdata;
			for (maps = 0 ; maps < MAXQ1LIGHTMAPS && surf->styles[maps] != INVALID_LIGHTSTYLE ; maps++)
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
			break;
		}
	}

store:
	// add all the dynamic lights
//	if (surf->dlightframe == r_framecount)
//		GLR_AddDynamicLightNorms (surf);

// bound, invert, and shift

	switch (lm->fmt)
	{
	default:
		Sys_Error("Bad deluxemap format\n");
		break;
	case PTI_A2BGR10:
		{
			unsigned int *destl = (void*)dest, r;

			stride = (lm->width-smax);
			bnorm = blocknormals[0];
			for (i=0 ; i<tmax ; i++, destl += stride)
			{
				for (j=0 ; j<smax ; j++)
				{
					temp[0] = bnorm[0];
					temp[1] = bnorm[1];
					temp[2] = bnorm[2];	//half the effect? so we emulate light's scalecos of 0.5
					bnorm+=3;
					VectorNormalize(temp);
					r  = (unsigned int)((temp[0]+1)/2*1023)<<0;
					r |= (unsigned int)((temp[1]+1)/2*1023)<<10;
					r |= (unsigned int)((temp[2]+1)/2*1023)<<20;
					*destl++ = r;
				}
			}
		}
		break;
	case PTI_BGRX8:
		stride = (lm->width-smax)*4;
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

				dest += 4;
				bnorm+=3;
			}
		}
		break;
	case PTI_RGBX8:
	case PTI_RGB8:
		stride = (lm->width-smax)*lm->pixbytes;
		bnorm = blocknormals[0];
		for (i=0 ; i<tmax ; i++, dest += stride)
		{
			for (j=0 ; j<smax ; j++)
			{
				temp[0] = bnorm[0];
				temp[1] = bnorm[1];
				temp[2] = bnorm[2];	//half the effect? so we emulate light's scalecos of 0.5
				VectorNormalize(temp);
				dest[0] = (temp[0]+1)/2*255;
				dest[1] = (temp[1]+1)/2*255;
				dest[2] = (temp[2]+1)/2*255;

				dest += lm->pixbytes;
				bnorm+=3;
			}
		}
		break;
	}
}

static unsigned int Surf_PackE5BRG9(int r, int g, int b, int shift)
{	//5 bits exponent, 3*9 bits of mantissa. no sign bit.
	int e = 0;
	float m = max(max(r, g), b) / (float)(1<<shift);
	float scale;

	if (m >= 0.5)
	{	//positive exponent
		while (m >= (1<<(e)) && e < 30-15)	//don't do nans.
			e++;
	}
	else
	{	//negative exponent...
		while (m < 1/(1<<-e) && e > -15)	//don't do denormals.
			e--;
	}

	scale = pow(2, e-9);
	scale *= (1<<shift);

	r = bound(0, r/scale + 0.5, 0x1ff);
	g = bound(0, g/scale + 0.5, 0x1ff);
	b = bound(0, b/scale + 0.5, 0x1ff);

	return ((e+15)<<27) | (b<<18) | (g<<9) | r;
}

static unsigned short Surf_GenHalf(float val)
{
	union 
	{
		float f;
		unsigned int u;
	} u = {val};
	int e = 0;
	int m;

	e = ((u.u>>23)&0xff) - 127;
	if (e < -15)
		return 0; //too small exponent, treat it as a 0 denormal
	if (e > 15)
		m = 0; //infinity instead of a nan
	else
		m = (u.u&((1<<23)-1))>>13;
	return ((e+15)<<10) | m;
}
static void Surf_PackRGB16F(void *result, int r, int g, int b, int one)
{
#if 0
	//bulldozer+ or skylake+ supposedly. which means I can't test it, which means I can't enable it.
	__v4sf rgba = (__v4sf){r, g, b, one} / (float)one;
	union
	{
		__v8hi v;
		__v4hi i;
	} tmp;
	//vcvtps2ph writes either a 64bit mem location, or the lower half of an xmm register. unfortunately ts still an xmm register, and that's 128bit.
	//the __vXhi weirdness is because half-floats just don't work anywhere but conversions so __vXhf would cause all sorts of compiler errors, is the theory
	tmp.v = __builtin_ia32_vcvtps2ph(rgba, 1);
	*(__v4hi*)result = tmp.i;
#else
	((unsigned short*)result)[0] = Surf_GenHalf(r / (float)one);
	((unsigned short*)result)[1] = Surf_GenHalf(g / (float)one);
	((unsigned short*)result)[2] = Surf_GenHalf(b / (float)one);
	((unsigned short*)result)[3] = /*Surf_GenHalf(1.0);*/0x0f<<10; //a standard ieee float should have all but the lead bit set of its exponent, and its mantissa 0.
#endif
}
static void Surf_PackRGB32F(void *result, int r, int g, int b, int one)
{
	((float*)result)[0] = r/(float)one;
	((float*)result)[1] = g/(float)one;
	((float*)result)[2] = b/(float)one;
	((float*)result)[3] = 1.0;
}

/*any sane compiler will inline and split this, removing the stainsrc stuff
just unpacks the internal lightmap block into texture info ready for upload
merges stains and oversaturates overbrights.
*/
static void Surf_StoreLightmap_RGB(qbyte *dest, unsigned int *bl, int smax, int tmax, unsigned int shift, stmap *stainsrc, lightmapinfo_t *lm)
{
	int r, g, b, m;
	unsigned int i, j;
	int stride;

	switch(lm->fmt)
	{
	default:
		Sys_Error("Bad lightmap_fmt\n");
		break;
	case PTI_A2BGR10:
		stride = (lm->width-smax)<<2;

		shift -= 2;

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
				if (m > 1023)
				{
					r *= 1023.0/m;
					g *= 1023.0/m;
					b *= 1023.0/m;
				}

				*(unsigned int*)dest = (3<<30) | ((b&0x3ff)<<20) | ((g&0x3ff)<<10) | (r&0x3ff);
				dest += 4;
			}
			if (stainsrc)
				stainsrc += (lm->width - smax)*3;
		}
		break;
	case PTI_E5BGR9:
		stride = (lm->width-smax)<<2;

		//5bit shared exponent, with bias of 15.
		for (i=0 ; i<tmax ; i++, dest += stride)
		{
			for (j=0 ; j<smax ; j++)
			{
				r = *bl++;
				g = *bl++;
				b = *bl++;

				if (stainsrc)	// merge in stain
				{
					r = (127+r*(*stainsrc++)) >> 8;
					g = (127+g*(*stainsrc++)) >> 8;
					b = (127+b*(*stainsrc++)) >> 8;
				}

				*(unsigned int*)dest = Surf_PackE5BRG9(r,g,b,shift+8);
				dest += 4;
			}
			if (stainsrc)
				stainsrc += (lm->width - smax)*3;
		}
		break;
	case PTI_RGBA16F:
		stride = (lm->width-smax)<<3;
		for (i=0 ; i<tmax ; i++, dest += stride)
		{
			for (j=0 ; j<smax ; j++)
			{
				r = *bl++;
				g = *bl++;
				b = *bl++;

				if (stainsrc)	// merge in stain
				{
					r = (127+r*(*stainsrc++)) >> 8;
					g = (127+g*(*stainsrc++)) >> 8;
					b = (127+b*(*stainsrc++)) >> 8;
				}

				Surf_PackRGB16F(dest, r,g,b,1<<(shift+8));
				dest += 8;
			}
			if (stainsrc)
				stainsrc += (lm->width - smax)*3;
		}
		break;
	case PTI_RGBA32F:
		stride = (lm->width-smax)<<4;
		for (i=0 ; i<tmax ; i++, dest += stride)
		{
			for (j=0 ; j<smax ; j++)
			{
				r = *bl++;
				g = *bl++;
				b = *bl++;

				if (stainsrc)	// merge in stain
				{
					r = (127+r*(*stainsrc++)) >> 8;
					g = (127+g*(*stainsrc++)) >> 8;
					b = (127+b*(*stainsrc++)) >> 8;
				}

				Surf_PackRGB32F(dest, r,g,b,1<<(shift+8));
				dest += sizeof(float)*4;
			}
			if (stainsrc)
				stainsrc += (lm->width - smax)*3;
		}
		break;
	case PTI_RGBX8:
	case PTI_RGBA8:
		stride = (lm->width-smax)<<2;

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
				dest[3] = 255;

				dest += 4;
			}
			if (stainsrc)
				stainsrc += (lm->width - smax)*3;
		}
		break;
	case PTI_BGRX8:
	case PTI_BGRA8:
		stride = (lm->width-smax)<<2;

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
				stainsrc += (lm->width - smax)*3;
		}
		break;
/*
	case PTI_BGRX8:
	case PTI_BGRA8:
		stride = (lm->width-smax)<<2;

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
				stainsrc += (lmwidth - smax)*3;
		}
		break;
*/
	case PTI_RGB565:
		stride = (lm->width-smax)<<1;

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

				*(unsigned short*)dest = (b>>3) | ((g>>2)<<5) | ((r>>3)<<11);
				dest += 2;
			}
			if (stainsrc)
				stainsrc += (lm->width - smax)*3;
		}
		break;
	case PTI_RGBA4444:
		stride = (lm->width-smax)<<1;

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

				*(unsigned short*)dest = ((r>>4)<<12) | ((g>>4)<<8) | ((b>>4)<<4) | 0x000f;
				dest += 2;
			}
			if (stainsrc)
				stainsrc += (lm->width - smax)*3;
		}
		break;
	case PTI_RGBA5551:
		stride = (lm->width-smax)<<1;

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

				*(unsigned short*)dest = ((r>>3)<<11) | ((g>>3)<<6) | ((b>>3)<<1) | 0x0001;
				dest += 2;
			}
			if (stainsrc)
				stainsrc += (lm->width - smax)*3;
		}
		break;
	case PTI_ARGB4444:
		stride = (lm->width-smax)<<1;

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

				*(unsigned short*)dest = 0xf000 | ((r>>4)<<8) | ((g>>4)<<4) | ((b>>4)<<0);
				dest += 2;
			}
			if (stainsrc)
				stainsrc += (lm->width - smax)*3;
		}
		break;
	case PTI_ARGB1555:
		stride = (lm->width-smax)<<1;

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

				*(unsigned short*)dest = 0x8000 | ((r>>3)<<10) | ((g>>3)<<5) | ((b>>3)<<0);
				dest += 2;
			}
			if (stainsrc)
				stainsrc += (lm->width - smax)*3;
		}
		break;
	case PTI_RGB8:
		stride = lm->width*3 - (smax*3);

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
				stainsrc += (lm->width - smax)*3;
		}
		break;
	}
}
static void Surf_StoreLightmap_Grey(qbyte *dest, unsigned int *bl, int smax, int tmax, unsigned int shift, stmap *stainsrc, unsigned int lmwidth)
{
	int t;
	unsigned int i, j;

	for (i=0 ; i<tmax ; i++, dest += lmwidth)
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
}

/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
static void Surf_BuildLightMap (model_t *currentmodel, msurface_t *surf, int map, int shift, int ambient, int *d_lightstylevalue)
{
	int			smax = (surf->extents[0]>>surf->lmshift)+1;
	int			tmax = (surf->extents[1]>>surf->lmshift)+1;
	int			t;
	int			i;
	size_t		size = (size_t)smax*tmax;
	unsigned	scalergb[3];
	unsigned	scale;
	int			maps;
	unsigned	*bl;
	glRect_t	*theRect;
	void		*dest;
	void		*deluxedest;
	void		*stainsrc;
	lightmapinfo_t *lm = lightmap[surf->lightmaptexturenums[map]];
	qbyte		*src = surf->samples;

	shift += 7; // increase to base value
	surf->cached_dlight = (surf->dlightframe == r_framecount);

	if (size > maxblocksize)
	{	//fixme: fill in?
		//Threading: this should not be a problem, all surfaces should have been built from the main thread at map load so it should have maxed it there.
		BZ_Free(blocklights);
		BZ_Free(blocknormals);

		maxblocksize = size;
		blocknormals = BZ_Malloc(maxblocksize * sizeof(*blocknormals));	//already a vector
		blocklights = BZ_Malloc(maxblocksize * 3*sizeof(*blocklights));
	}

	//make sure we flag the output rect properly.
	theRect = &lm->rectchange;
	if (theRect->t > surf->light_t[map])
		theRect->t = surf->light_t[map];
	if (theRect->l > surf->light_s[map])
		theRect->l = surf->light_s[map];
	if (theRect->r < surf->light_s[map]+smax)
		theRect->r = surf->light_s[map]+smax;
	if (theRect->b < surf->light_t[map]+tmax)
		theRect->b = surf->light_t[map]+tmax;

	dest = lm->lightmaps + (surf->light_t[map] * lm->width + surf->light_s[map]) * lm->pixbytes;
	if (!r_stains.value || !surf->stained)
		stainsrc = NULL;
	else
		stainsrc = lm->stainmaps + (surf->light_t[map] * lm->width + surf->light_s[map]) * 3;

	lm->modified = true;
	if (lm->hasdeluxe && currentmodel->deluxdata)
	{
		lightmapinfo_t *dlm = lightmap[surf->lightmaptexturenums[map]+1];
		dlm->modified = true;
		theRect = &dlm->rectchange;
		if (theRect->t > surf->light_t[map])
			theRect->t = surf->light_t[map];
		if (theRect->l > surf->light_s[map])
			theRect->l = surf->light_s[map];
		if (theRect->r < surf->light_s[map]+smax)
			theRect->r = surf->light_s[map]+smax;
		if (theRect->b < surf->light_t[map]+tmax)
			theRect->b = surf->light_t[map]+tmax;

		deluxedest = dlm->lightmaps + (surf->light_t[map] * dlm->width + surf->light_s[map]) * dlm->pixbytes;

		Surf_BuildDeluxMap(currentmodel, surf, deluxedest, dlm, blocknormals);
	}

	if (lm->fmt != PTI_L8)
	{
		// set to full bright if no light data
		if (ambient < 0)
		{
			t = (-1-ambient)*255;
			for (i=0 ; i<size*3 ; i++)
				blocklights[i] = t;
			for (maps = 0 ; maps < MAXQ1LIGHTMAPS ; maps++)
			{
				surf->cached_light[maps] = -1-ambient;
				surf->cached_colour[maps] = 0xff;
			}
		}
		else if (r_fullbright.value>0)	//not qw
		{
			for (i=0 ; i<size*3 ; i++)
				blocklights[i] = r_fullbright.value*255*256;
		}
		else if (!currentmodel->lightdata)
		{
			/*fullbright if map is not lit. but not overbright*/
			for (i=0 ; i<size*3 ; i++)
				blocklights[i] = 128*256;
		}
		else if (!surf->samples)
		{
			/*no samples, but map is otherwise lit = pure black*/
			for (i=0 ; i<size*3 ; i++)
				blocklights[i] = 0;
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
			if (src)
			{
				if (currentmodel->fromgame == fg_quake3)
					Sys_Error("Surf_BuildLightMap: q3bsp");
				switch(currentmodel->lightmaps.fmt)
				{
				case LM_E5BGR9:
					for (maps = 0 ; maps < MAXQ1LIGHTMAPS && surf->styles[maps] != INVALID_LIGHTSTYLE ; maps++)
					{
						surf->cached_light[maps] = scale = d_lightstylevalue[surf->styles[maps]];	// 8.8 fraction
						surf->cached_colour[maps] = cl_lightstyle[surf->styles[maps]].colourkey;
						if (scale)
						{
							VectorScale(cl_lightstyle[surf->styles[maps]].colours, scale, scalergb);
							for (i=0 ; i<size ; i++)
							{
								unsigned int l = ((unsigned int*)src)[i];
								float e = rgb9e5tab[l>>27]*(1<<7);
								blocklights[i*3+0] += scalergb[0] * e * ((l>> 0)&0x1ff);
								blocklights[i*3+1] += scalergb[1] * e * ((l>> 9)&0x1ff);
								blocklights[i*3+2] += scalergb[2] * e * ((l>>18)&0x1ff);
							}
						}
						src += size*4;	// skip to next lightmap
					}
					break;
				case LM_RGB8:
					for (maps = 0 ; maps < MAXQ1LIGHTMAPS && surf->styles[maps] != INVALID_LIGHTSTYLE ; maps++)
					{
						surf->cached_light[maps] = scale = d_lightstylevalue[surf->styles[maps]];
						surf->cached_colour[maps] = cl_lightstyle[surf->styles[maps]].colourkey;
						if (scale)
						{
							VectorScale(cl_lightstyle[surf->styles[maps]].colours, scale, scalergb);
							bl = blocklights;
							for (i=0 ; i<size ; i++)
							{
								*bl++		+=   *src++ * scalergb[0];
								*bl++		+=   *src++ * scalergb[1];
								*bl++		+=   *src++ * scalergb[2];
							}
						}
						else
							src += size*3;	// skip to next lightmap
					}
					break;

				case LM_L8:
					for (maps = 0 ; maps < MAXQ1LIGHTMAPS && surf->styles[maps] != INVALID_LIGHTSTYLE ;
						 maps++)
					{
						surf->cached_light[maps] = scale = d_lightstylevalue[surf->styles[maps]];	// 8.8 fraction
						surf->cached_colour[maps] = cl_lightstyle[surf->styles[maps]].colourkey;
						if (scale)
						{
							VectorScale(cl_lightstyle[surf->styles[maps]].colours, scale, scalergb);
							bl = blocklights;
							for (i=0 ; i<size ; i++)
							{
								*bl++		+= *src * scalergb[0];
								*bl++		+= *src * scalergb[1];
								*bl++		+= *src * scalergb[2];
								src++;
							}
						}
						else
							src += size;	// skip to next lightmap
					}
					break;
				}
			}
		}

		// add all the dynamic lights
		if (surf->dlightframe == r_framecount)
			Surf_AddDynamicLightsColours (surf);

		Surf_StoreLightmap_RGB(dest, blocklights, smax, tmax, shift, stainsrc, lm);
	}
	else
	{
		// set to full bright if no light data
		if (ambient < 0)
		{
			t = (-1-ambient)*255;
			for (i=0 ; i<size ; i++)
				blocklights[i] = t;
			for (maps = 0 ; maps < MAXQ1LIGHTMAPS ; maps++)
			{
				surf->cached_light[maps] = -1-ambient;
				surf->cached_colour[maps] = 0xff;
			}
		}
		else if (r_fullbright.value > 0)
		{	//r_fullbright is meant to be a scaler.
			for (i=0 ; i<size ; i++)
				blocklights[i] = r_fullbright.value*255*256;
			surf->cached_light[0] = d_lightstylevalue[0];
			surf->cached_colour[0] = cl_lightstyle[0].colourkey;
		}
		else if (!currentmodel->lightdata)
		{	//no scalers here.
			for (i=0 ; i<size ; i++)
				blocklights[i] = 255*256;
			surf->cached_light[0] = d_lightstylevalue[0];
			surf->cached_colour[0] = cl_lightstyle[0].colourkey;
		}
		//surfaces with no light data on lit maps are black
		else if (!surf->samples)
		{
			for (i=0 ; i<size*3 ; i++)
				blocklights[i] = 0;
			surf->cached_light[0] = d_lightstylevalue[0];
			surf->cached_colour[0] = cl_lightstyle[0].colourkey;
		}
		else
		{
// clear to no light
			for (i=0 ; i<size ; i++)
				blocklights[i] = 0;

// add all the lightmaps
			if (src)
			{
				switch(currentmodel->lightmaps.fmt)
				{
				case LM_E5BGR9:
					for (maps = 0 ; maps < MAXQ1LIGHTMAPS && surf->styles[maps] != INVALID_LIGHTSTYLE ; maps++)
					{
						scale = d_lightstylevalue[surf->styles[maps]];
						surf->cached_light[maps] = scale;	// 8.8 fraction
						surf->cached_colour[maps] = cl_lightstyle[surf->styles[maps]].colourkey;
						for (i=0 ; i<size ; i++)
						{
							unsigned int lm = ((unsigned int *)src)[i];
							blocklights[i] += max3(((lm>>0)&0x1ff),((lm>>9)&0x1ff),((lm>>18)&0x1ff)) * scale * (rgb9e5tab[lm>>27]*(1<<7));
						}
						src += size*4;	// skip to next lightmap
					}
					break;
				case LM_RGB8:
					for (maps = 0 ; maps < MAXQ1LIGHTMAPS && surf->styles[maps] != INVALID_LIGHTSTYLE ; maps++)
					{
						scale = d_lightstylevalue[surf->styles[maps]];
						surf->cached_light[maps] = scale;	// 8.8 fraction
						surf->cached_colour[maps] = cl_lightstyle[surf->styles[maps]].colourkey;
						for (i=0 ; i<size ; i++)
							blocklights[i] += max3(src[i*3],src[i*3+1],src[i*3+2]) * scale;
						src += size*3;	// skip to next lightmap
					}
					break;
				case LM_L8:
					for (maps = 0 ; maps < MAXQ1LIGHTMAPS && surf->styles[maps] != INVALID_LIGHTSTYLE ; maps++)
					{
						scale = d_lightstylevalue[surf->styles[maps]];
						surf->cached_light[maps] = scale;	// 8.8 fraction
						surf->cached_colour[maps] = cl_lightstyle[surf->styles[maps]].colourkey;
						for (i=0 ; i<size ; i++)
							blocklights[i] += src[i] * scale;
						src += size;	// skip to next lightmap
					}
					break;
				}
			}
// add all the dynamic lights
			if (surf->dlightframe == r_framecount)
				Surf_AddDynamicLights (surf);
		}

		Surf_StoreLightmap_Grey(dest, blocklights, smax, tmax, shift, stainsrc, lm->width);
	}
}

#if defined(THREADEDWORLD) && (defined(Q1BSPS)||defined(Q2BSPS))
static void Surf_BuildLightMap_Worker (model_t *wmodel, msurface_t *surf, int shift, int ambient, int *d_lightstylevalue)
{
	int			smax, tmax;
	int			t;
	int			i, j;
	size_t		size;
	lightmapinfo_t *lm = lightmap[surf->lightmaptexturenums[0]];
	qbyte		*src;
	unsigned	scalergb[3];
	unsigned	scale;
	int			maps;
	unsigned	*bl;
	qbyte		*dest;
	qbyte		*deluxedest;
	stmap		*stainsrc;
	glRect_t	*theRect;

	static size_t maxblocksize;
	static vec3_t *blocknormals;
	static unsigned int *blocklights;

	shift += 7; // increase to base value
	surf->cached_dlight = false;

	smax = (surf->extents[0]>>surf->lmshift)+1;
	tmax = (surf->extents[1]>>surf->lmshift)+1;
	size = (size_t)smax*tmax;
	src = surf->samples;

	if (size > maxblocksize)
	{	//fixme: fill in?
		maxblocksize = size;
		blocknormals = BZ_Realloc(blocknormals, maxblocksize * sizeof(*blocknormals));	//already a vector
		blocklights = BZ_Realloc(blocklights, maxblocksize * 3*sizeof(*blocklights));
	}

	dest = lm->lightmaps + (surf->light_t[0] * lm->width + surf->light_s[0]) * lm->pixbytes;
	if (!r_stains.value || !surf->stained)
		stainsrc = NULL;
	else
		stainsrc = lm->stainmaps + (surf->light_t[0] * lm->width + surf->light_s[0]) * 3;

	if (lm->hasdeluxe)
	{
		lightmapinfo_t *dlm = lightmap[surf->lightmaptexturenums[0]+1];
		deluxedest = dlm->lightmaps + (surf->light_t[0] * dlm->width + surf->light_s[0]) * dlm->pixbytes;

		Surf_BuildDeluxMap(wmodel, surf, deluxedest, lm, blocknormals);
	}

	if (lm->fmt != PTI_L8)
	{
		// set to full bright if no light data
		if (ambient < 0)
		{
			t = (-1-ambient)*255;
			for (i=0 ; i<size*3 ; i++)
			{
				blocklights[i] = t;
			}

			for (maps = 0 ; maps < MAXQ1LIGHTMAPS ; maps++)
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
		else if (!wmodel->lightdata)
		{
			/*fullbright if map is not lit. but not overbright*/
			for (i=0 ; i<size*3 ; i++)
			{
				blocklights[i] = 128*256;
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
			if (src)
			{
				if (wmodel->fromgame == fg_quake3)	//rgb
				{
					/*q3 lightmaps are meant to be pre-built
					this code is misguided, and ought never be executed anyway.
					*/
					int pixbytes = lm->pixbytes;
					bl = blocklights;
					for (i = 0; i < tmax; i++)
					{
						for (j = 0; j < smax; j++)
						{
							bl[0]		= 255*src[(i*pixbytes+j)*3];
							bl[1]		= 255*src[(i*pixbytes+j)*3+1];
							bl[2]		= 255*src[(i*pixbytes+j)*3+2];
							bl+=3;
						}
					}
				}
				else switch(cl.worldmodel->lightmaps.fmt)
				{
				case LM_E5BGR9:
					for (maps = 0 ; maps < MAXQ1LIGHTMAPS && surf->styles[maps] != INVALID_LIGHTSTYLE ; maps++)
					{
						surf->cached_light[maps] = scale = d_lightstylevalue[surf->styles[maps]];	// 8.8 fraction
						surf->cached_colour[maps] = cl_lightstyle[surf->styles[maps]].colourkey;
						if (scale)
						{
							VectorScale(cl_lightstyle[surf->styles[maps]].colours, scale, scalergb);
							for (i=0 ; i<size ; i++)
							{
								unsigned int l = ((unsigned int*)src)[i];
								float e = rgb9e5tab[l>>27]*(1<<7);
								blocklights[i*3+0] += scalergb[0] * e * ((l>> 0)&0x1ff);
								blocklights[i*3+1] += scalergb[1] * e * ((l>> 9)&0x1ff);
								blocklights[i*3+2] += scalergb[2] * e * ((l>>18)&0x1ff);
							}
						}
						src += size*4;	// skip to next lightmap
					}
					break;
				case LM_RGB8:
					for (maps = 0 ; maps < MAXQ1LIGHTMAPS && surf->styles[maps] != INVALID_LIGHTSTYLE ; maps++)
					{
						surf->cached_light[maps] = scale = d_lightstylevalue[surf->styles[maps]];
						surf->cached_colour[maps] = cl_lightstyle[surf->styles[maps]].colourkey;
						if (scale)
						{
							VectorScale(cl_lightstyle[surf->styles[maps]].colours, scale, scalergb);
							bl = blocklights;
							for (i=0 ; i<size ; i++)
							{
								*bl++		+=   *src++ * scalergb[0];
								*bl++		+=   *src++ * scalergb[1];
								*bl++		+=   *src++ * scalergb[2];
							}
						}
						else
							src += size*3;	// skip to next lightmap
					}
					break;

				case LM_L8:
					for (maps = 0 ; maps < MAXQ1LIGHTMAPS && surf->styles[maps] != INVALID_LIGHTSTYLE ;
						 maps++)
					{
						surf->cached_light[maps] = scale = d_lightstylevalue[surf->styles[maps]];	// 8.8 fraction
						surf->cached_colour[maps] = cl_lightstyle[surf->styles[maps]].colourkey;
						if (scale)
						{
							VectorScale(cl_lightstyle[surf->styles[maps]].colours, scale, scalergb);
							bl = blocklights;
							for (i=0 ; i<size ; i++)
							{
								*bl++		+= *src * scalergb[0];
								*bl++		+= *src * scalergb[1];
								*bl++		+= *src * scalergb[2];
								src++;
							}
						}
						else
							src += size;	// skip to next lightmap
					}
					break;
				}
			}
		}

		if (!r_stains.value || !surf->stained)
			stainsrc = NULL;

		Surf_StoreLightmap_RGB(dest, blocklights, smax, tmax, shift, stainsrc, lm);
	}
	else
	{
	// set to full bright if no light data
		if (r_fullbright.ival)
		{
			for (i=0 ; i<size ; i++)
				blocklights[i] = 255*256;
		}
		else if (!wmodel->lightdata)
		{
			for (i=0 ; i<size*3 ; i++)
			{
				blocklights[i] = 255*256;
			}
			surf->cached_light[0] = d_lightstylevalue[0];
			surf->cached_colour[0] = cl_lightstyle[0].colourkey;
		}
		else if (!surf->samples)
		{
			for (i=0 ; i<size ; i++)
				blocklights[i] = 0;
		}
		else
		{
// clear to no light
			for (i=0 ; i<size ; i++)
				blocklights[i] = 0;

// add all the lightmaps
			if (src)
			{
				switch(cl.worldmodel->lightmaps.fmt)
				{
				case LM_E5BGR9:
					for (maps = 0 ; maps < MAXQ1LIGHTMAPS && surf->styles[maps] != INVALID_LIGHTSTYLE ; maps++)
					{
						scale = d_lightstylevalue[surf->styles[maps]];
						surf->cached_light[maps] = scale;	// 8.8 fraction
						surf->cached_colour[maps] = cl_lightstyle[surf->styles[maps]].colourkey;
						for (i=0 ; i<size ; i++)
						{
							unsigned int lm = ((unsigned int *)lightmap)[i];
							blocklights[i] += max3(((lm>>0)&0x1ff),((lm>>9)&0x1ff),((lm>>18)&0x1ff)) * scale * (rgb9e5tab[lm>>27]*(1<<7));
						}
						lightmap += size*4;	// skip to next lightmap
					}
					break;
				case LM_RGB8:
					for (maps = 0 ; maps < MAXQ1LIGHTMAPS && surf->styles[maps] != INVALID_LIGHTSTYLE ; maps++)
					{
						scale = d_lightstylevalue[surf->styles[maps]];
						surf->cached_light[maps] = scale;	// 8.8 fraction
						surf->cached_colour[maps] = cl_lightstyle[surf->styles[maps]].colourkey;
						for (i=0 ; i<size ; i++)
							blocklights[i] += max3(src[i*3],src[i*3+1],src[i*3+2]) * scale;
						src += size*3;	// skip to next lightmap
					}
					break;
				case LM_L8:
					for (maps = 0 ; maps < MAXQ1LIGHTMAPS && surf->styles[maps] != INVALID_LIGHTSTYLE ; maps++)
					{
						scale = d_lightstylevalue[surf->styles[maps]];
						surf->cached_light[maps] = scale;	// 8.8 fraction
						surf->cached_colour[maps] = cl_lightstyle[surf->styles[maps]].colourkey;
						for (i=0 ; i<size ; i++)
							blocklights[i] += src[i] * scale;
						src += size;	// skip to next lightmap
					}
					break;
				}
			}
		}

		Surf_StoreLightmap_Grey(dest, blocklights, smax, tmax, shift, stainsrc, lm->width);
	}

	//make sure we flag the output rect properly.
	theRect = &lm->rectchange;
	if (theRect->t > surf->light_t[0])
		theRect->t = surf->light_t[0];
	if (theRect->l > surf->light_s[0])
		theRect->l = surf->light_s[0];
	if (theRect->r < surf->light_s[0]+smax)
		theRect->r = surf->light_s[0]+smax;
	if (theRect->b < surf->light_t[0]+tmax)
		theRect->b = surf->light_t[0]+tmax;
	lm->modified = true;

	if (lm->hasdeluxe)
	{
		lightmapinfo_t *dlm = lm+1;
		theRect = &dlm->rectchange;
		if (theRect->t > surf->light_t[0])
			theRect->t = surf->light_t[0];
		if (theRect->l > surf->light_s[0])
			theRect->l = surf->light_s[0];
		if (theRect->r < surf->light_s[0]+smax)
			theRect->r = surf->light_s[0]+smax;
		if (theRect->b < surf->light_t[0]+tmax)
			theRect->b = surf->light_t[0]+tmax;
		dlm->modified = true;
	}
}
#endif

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
	int			maps;

	//surfaces without lightmaps
	if (fa->lightmaptexturenums[0]<0 || !lightmap)
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
		for (maps = 0 ; maps < MAXQ1LIGHTMAPS && fa->styles[maps] != INVALID_LIGHTSTYLE ;
			 maps++)
			if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps]
				|| cl_lightstyle[fa->styles[maps]].colourkey != fa->cached_colour[maps])
				goto dynamic;
	}

	if (fa->dlightframe == r_framecount	// dynamic this frame
		|| fa->cached_dlight)			// dynamic previously
	{
		RSpeedLocals();
dynamic:
		RSpeedRemark();

#ifdef _DEBUG
		if ((unsigned)fa->lightmaptexturenums[0] >= numlightmaps)
			Sys_Error("Invalid lightmap index\n");
#endif

		Surf_BuildLightMap (currentmodel, fa, 0, lightmap_shift, r_ambient.value*255, d_lightstylevalue);

		RSpeedEnd(RSPEED_DYNAMIC);
	}
}

#if defined(THREADEDWORLD) && (defined(Q1BSPS)||defined(Q2BSPS))
static void Surf_RenderDynamicLightmaps_Worker (model_t *wmodel, msurface_t *fa, int *d_lightstylevalue)
{
	int			maps;

	//surfaces without lightmaps
	if (fa->lightmaptexturenums[0]<0 || !lightmap)
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
		for (maps = 0 ; maps < MAXQ1LIGHTMAPS && fa->styles[maps] != INVALID_LIGHTSTYLE ;
			 maps++)
			if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps]
				|| cl_lightstyle[fa->styles[maps]].colourkey != fa->cached_colour[maps])
				goto dynamic;
	}

	return;

dynamic:

#ifdef _DEBUG
	if ((unsigned)fa->lightmaptexturenums[0] >= numlightmaps)
		Sys_Error("Invalid lightmap index\n");
#endif


	Surf_BuildLightMap_Worker (wmodel, fa, lightmap_shift, r_ambient.value*255, d_lightstylevalue);
}
#endif //THREADEDWORLD

void Surf_RenderAmbientLightmaps (msurface_t *fa, int ambient)
{
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

		Surf_BuildLightMap (currentmodel, fa, 0, lightmap_shift, -1-ambient, d_lightstylevalue);

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

	for (i=0 ; i<cl.worldmodel->numvisleafs ; i++)
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

#ifdef Q1BSPS
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

	for (c = 0, clipplane = r_refdef.frustum; c < r_refdef.frustum_numworldplanes; c++, clipplane++)
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

		c = (pleaf - cl.worldmodel->leafs)-1;
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

	for (c = 0, clipplane = r_refdef.frustum; c < r_refdef.frustum_numworldplanes; c++, clipplane++)
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
#endif

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
		if (! (r_refdef.areabits[pleaf->area>>3] & (1<<(pleaf->area&7)) ) )
			return;		// not visible

		c = pleaf->cluster;
		if (c >= 0)
			frustumvis[c>>3] |= 1<<(c&7);

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

		surf->visframe = 0;//r_framecount+1;//-1;

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

	for (c = 0, clipplane = r_refdef.frustum; c < r_refdef.frustum_numworldplanes; c++, clipplane++)
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

		if (! (r_refdef.areabits[pleaf->area>>3] & (1<<(pleaf->area&7)) ) )
			return;		// not visible

		c = pleaf->cluster;
		if (c >= 0)
			frustumvis[c>>3] |= 1<<(c&7);

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

static void Surf_PushChains(batch_t **batches)
{
	batch_t *batch;
	int i;

	if (r_refdef.recurse == R_MAX_RECURSE)
		Sys_Error("Recursed too deep\n");

	if (!r_refdef.recurse)
	{
		for (i = 0; i < SHADER_SORT_COUNT; i++)
		for (batch = batches[i]; batch; batch = batch->next)
		{
			batch->firstmesh = 0;
		}
	}
#if R_MAX_RECURSE > 2
	else if (r_refdef.recurse > 1)
	{
		for (i = 0; i < SHADER_SORT_COUNT; i++)
		for (batch = batches[i]; batch; batch = batch->next)
		{
			batch->recursefirst[r_refdef.recurse] = batch->firstmesh;
			batch->firstmesh = batch->meshes;
		}
	}
#endif
	else
	{
		for (i = 0; i < SHADER_SORT_COUNT; i++)
		for (batch = batches[i]; batch; batch = batch->next)
		{
			batch->firstmesh = batch->meshes;
		}
	}
}
static void Surf_PopChains(batch_t **batches)
{
	batch_t *batch;
	int i;

	if (!r_refdef.recurse)
	{
		for (i = 0; i < SHADER_SORT_COUNT; i++)
		for (batch = batches[i]; batch; batch = batch->next)
		{
			batch->meshes = 0;
		}
	}
#if R_MAX_RECURSE > 2
	else if (r_refdef.recurse > 1)
	{
		for (i = 0; i < SHADER_SORT_COUNT; i++)
		for (batch = batches[i]; batch; batch = batch->next)
		{
			batch->meshes = batch->firstmesh;
			batch->firstmesh = batch->recursefirst[r_refdef.recurse];
		}
	}
#endif
	else
	{
		for (i = 0; i < SHADER_SORT_COUNT; i++)
		for (batch = batches[i]; batch; batch = batch->next)
		{
			batch->meshes = batch->firstmesh;
			batch->firstmesh = 0;
		}
	}
}

//most of this is a direct copy from gl
void Surf_SetupFrame(void)
{
	mleaf_t	*leaf;
	vec3_t	temp, pvsorg;
	int viewcontents;

	if (!cl.worldmodel || (!cl.worldmodel->nodes && cl.worldmodel->type != mod_heightmap))
		r_refdef.flags |= RDF_NOWORLDMODEL;

	R_AnimateLight();
	r_framecount++;

	if (r_refdef.recurse)
	{
		VectorCopy(r_refdef.pvsorigin, pvsorg);
	}
	else
	{
		VectorCopy(r_refdef.vieworg, pvsorg);
		R_UpdateHDR(r_refdef.vieworg);
	}

	r_viewarea = 0;
	viewcontents = 0;
	if (r_refdef.flags & RDF_NOWORLDMODEL)
	{
	}
	else if (!cl.worldmodel || cl.worldmodel->loadstate != MLS_LOADED || cl.worldmodel->fromgame == fg_doom3 )
	{
		r_viewcluster = -1;
		r_viewcluster2 = -1;
	}
#if defined(Q2BSPS) || defined(Q3BSPS)
	else if (cl.worldmodel->fromgame == fg_quake2 || cl.worldmodel->fromgame == fg_quake3)
	{
		leaf = Mod_PointInLeaf (cl.worldmodel, pvsorg);
		r_viewarea = leaf->area;
		viewcontents = cl.worldmodel->funcs.PointContents(cl.worldmodel, NULL, pvsorg);
		r_viewcluster = r_viewcluster2 = leaf->cluster;

		// check above and below so crossing solid water doesn't draw wrong
		if (!leaf->contents)
		{	// look down a bit
			vec3_t	temp;

			VectorCopy (pvsorg, temp);
			temp[2] -= 16;
			leaf = Mod_PointInLeaf (cl.worldmodel, temp);
			if ( !(leaf->contents & Q2CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2) )
				r_viewcluster2 = leaf->cluster;
		}
		else
		{	// look up a bit
			vec3_t	temp;

			VectorCopy (pvsorg, temp);
			temp[2] += 16;
			leaf = Mod_PointInLeaf (cl.worldmodel, temp);
			if ( !(leaf->contents & Q2CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2) )
				r_viewcluster2 = leaf->cluster;
		}
	}
#endif
	else
	{
		leaf = Mod_PointInLeaf (cl.worldmodel, pvsorg);
		r_viewcluster = (leaf - cl.worldmodel->leafs)-1;
		r_viewcluster2 = -1;
		if (leaf)
		{
			switch(leaf->contents)
			{
			case Q1CONTENTS_WATER:
				viewcontents |= FTECONTENTS_WATER;
				break;
			case Q1CONTENTS_LAVA:
				viewcontents |= FTECONTENTS_LAVA;
				break;
			case Q1CONTENTS_SLIME:
				viewcontents |= FTECONTENTS_SLIME;
				break;
			case Q1CONTENTS_SKY:
				viewcontents |= FTECONTENTS_SKY;
				break;
			case Q1CONTENTS_SOLID:
				viewcontents |= FTECONTENTS_SOLID;
				break;
			case Q1CONTENTS_LADDER:
				viewcontents |= FTECONTENTS_LADDER;
				break;
			}

			if (leaf->contents == Q1CONTENTS_EMPTY)
			{	//look down a bit
				VectorCopy (pvsorg, temp);
				temp[2] -= 16;
				leaf = Mod_PointInLeaf (cl.worldmodel, temp);
				if (leaf->contents <= Q1CONTENTS_WATER && leaf->contents >= Q1CONTENTS_LAVA)
					r_viewcluster2 = (leaf - cl.worldmodel->leafs)-1;
			}
			else if (leaf->contents <= Q1CONTENTS_WATER && leaf->contents >= Q1CONTENTS_LAVA)
			{	//in water, look up a bit.

				VectorCopy (pvsorg, temp);
				temp[2] += 16;
				leaf = Mod_PointInLeaf (cl.worldmodel, temp);
				if (leaf->contents == Q1CONTENTS_EMPTY)
					r_viewcluster2 = (leaf - cl.worldmodel->leafs)-1;
			}
		}
	}

#ifdef TERRAIN
	if (!(r_refdef.flags & RDF_NOWORLDMODEL) && cl.worldmodel && cl.worldmodel->terrain)
	{
		viewcontents |= Heightmap_PointContents(cl.worldmodel, NULL, pvsorg);
	}
#endif

	/*pick up any extra water entities*/
	{
		vec3_t t1,t2;
		VectorCopy(pmove.player_mins, t1);
		VectorCopy(pmove.player_maxs, t2);
		VectorClear(pmove.player_maxs);
		VectorClear(pmove.player_mins);
		viewcontents |= PM_ExtraBoxContents(pvsorg);
		VectorCopy(t1, pmove.player_mins);
		VectorCopy(t2, pmove.player_maxs);
	}
	if (!r_refdef.recurse)
	{
		r_viewcontents = viewcontents;
		if (!r_secondaryview)
			V_SetContentsColor (viewcontents);
	}


	if (r_refdef.playerview->audio.defaulted)
	{
		//first scene is the 'main' scene and audio defaults to that (unless overridden later in the frame)
		r_refdef.playerview->audio.defaulted = false;
		r_refdef.playerview->audio.entnum = r_refdef.playerview->viewentity;
		VectorCopy(r_refdef.vieworg, r_refdef.playerview->audio.origin);
		AngleVectors(r_refdef.viewangles, r_refdef.playerview->audio.forward,r_refdef.playerview->audio.right, r_refdef.playerview->audio.up);
		if (r_viewcontents & FTECONTENTS_FLUID)
			r_refdef.playerview->audio.reverbtype = 1;
		else
			r_refdef.playerview->audio.reverbtype = 0;
		VectorCopy(r_refdef.playerview->simvel, r_refdef.playerview->audio.velocity);
	}
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
	if (model->fromgame != fg_quake3 && model->fromgame != fg_doom3 && lightmap)
	{
		int k;

		currententity = ent;
		currentmodel = ent->model;
		if (model->nummodelsurfaces != 0 && r_dynamic.ival > 0)
		{
			for (k=rtlights_first; k<RTL_FIRST; k++)
			{
				if (!cl_dlights[k].radius)
					continue;
				if (!(cl_dlights[k].flags & LFLAG_LIGHTMAP))
					continue;

				model->funcs.MarkLights (&cl_dlights[k], 1<<k, model->rootnode);
			}
		}

		Surf_LightmapShift(model);
#ifdef HEXEN2
		if ((ent->drawflags & MLS_MASK) == MLS_ABSLIGHT)
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
#endif
		{
			//update lightmaps.
			for (s = model->surfaces+model->firstmodelsurface,i = 0; i < model->nummodelsurfaces; i++, s++)
				Surf_RenderDynamicLightmaps (s);
		}
		currententity = NULL;
	}

#ifdef BEF_PUSHDEPTH
	if (r_pushdepth)
		bef = BEF_PUSHDEPTH;
	else
		bef = 0;
#else
	bef = 0;
#endif
	if (ent->flags & RF_ADDITIVE)
		bef |= BEF_FORCEADDITIVE;
#ifdef HEXEN2
	else if ((ent->drawflags & DRF_TRANSLUCENT) && r_wateralpha.value != 1)
	{
		bef |= BEF_FORCETRANSPARENT;
		ent->shaderRGBAf[3] = r_wateralpha.value;
	}
#endif
	else if ((ent->flags & RF_TRANSLUCENT) && cls.protocol != CP_QUAKE3)
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
//		if (b->texture)
//			b->shader = R_TextureAnimation(ent->framestate.g[FS_REG].frame[0], b->texture)->shader;
		b->meshes = b->maxmeshes;
		b->ent = ent;
		b->flags = bef;

		if (b->buildmeshes)
			b->buildmeshes(b);

		if (!b->shader)
			b->shader = R_TextureAnimation(ent->framestate.g[FS_REG].frame[0], b->texture)->shader;

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

#ifdef THREADEDWORLD
struct webostate_s
{
	char dbgid[12];
	struct webostate_s *next;
	model_t *wmodel;
	int cluster[2];
	qboolean generating;
	pvsbuffer_t pvs;
	vboarray_t ebo;
	void *ebomem;
	size_t idxcount;
	int numbatches;
	int lightstylevalues[MAX_NET_LIGHTSTYLES];	//when using workers that only reprocessing lighting at 10fps, things get too ugly when things go out of sync

	vec3_t lastpos;

	batch_t *rbatches[SHADER_SORT_COUNT];

	struct wesbatch_s
	{
		size_t numidx;
		size_t maxidx;
		index_t *idxbuffer;
		batch_t b;
		mesh_t m;
		mesh_t *pm;
		vbo_t vbo;
	} batches[1];
};
static struct webostate_s *webostates;
static struct webostate_s *webogenerating;
static int webogeneratingstate;	//1 if generating, 0 if not, for waiting for sync.
static void R_DestroyWorldEBO(struct webostate_s *es)
{
	if (!es)
		return;

#ifdef GLQUAKE
	if (qrenderer == QR_OPENGL)
		qglDeleteBuffersARB(1, &es->ebo.gl.vbo);
#endif
#ifdef VKQUAKE
	if (qrenderer == QR_VULKAN)
		BE_VBO_Destroy(&es->ebo, es->ebomem);
#endif
	BZ_Free(es);
}
void R_GeneratedWorldEBO(void *ctx, void *data, size_t a_, size_t b_)
{
	size_t idxcount;
	unsigned int i;
	model_t *mod;
	batch_t *b, *batch;
	mesh_t *m;
	int sortid;
	struct webostate_s *webostate = ctx;
	webostate->next = webostates;
	webostates = webostate;
	webogenerating = NULL;
	webogeneratingstate = 0;
	mod = webostate->wmodel;

	for (i = 0, idxcount = 0; i < webostate->numbatches; i++)
		idxcount += webostate->batches[i].numidx;
#ifdef GLQUAKE
	if (qrenderer == QR_OPENGL)
	{
		GL_DeselectVAO();

		webostate->ebo.gl.addr = NULL;
		qglGenBuffersARB(1, &webostate->ebo.gl.vbo);
		GL_SelectEBO(webostate->ebo.gl.vbo);
		qglBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, idxcount*sizeof(index_t), NULL, GL_STATIC_DRAW_ARB);
		for (i = 0, idxcount = 0; i < webostate->numbatches; i++)
		{
			qglBufferSubDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, idxcount*sizeof(index_t), webostate->batches[i].numidx*sizeof(index_t), webostate->batches[i].idxbuffer);
			BZ_Free(webostate->batches[i].idxbuffer);
			webostate->batches[i].idxbuffer = (index_t*)NULL + idxcount;
			idxcount += webostate->batches[i].numidx;
		}
	}
#endif
#ifdef VKQUAKE
	if (qrenderer == QR_VULKAN)
	{	//this malloc is stupid.
		//with vulkan we really should be doing this on the worker instead, at least the staging part.
		index_t *indexes = malloc(sizeof(*indexes) * idxcount);
		webostate->ebo.vk.offs = 0;
		for (i = 0, idxcount = 0; i < webostate->numbatches; i++)
		{
			memcpy(indexes + idxcount, webostate->batches[i].idxbuffer, webostate->batches[i].numidx*sizeof(index_t));
			BZ_Free(webostate->batches[i].idxbuffer);
			webostate->batches[i].idxbuffer = (index_t*)NULL + idxcount;
			idxcount += webostate->batches[i].numidx;
		}
		if (idxcount)
			BE_VBO_Finish(NULL, indexes, sizeof(*indexes) * idxcount, &webostate->ebo, NULL, &webostate->ebomem);
		else
		{
			memset(&webostate->ebo, 0, sizeof(webostate->ebo));
			webostate->ebomem = NULL;
		}
		free(indexes);
	}
#endif

	//should be doing this on the worker, but whatever
	for (i = 0, sortid = 0; sortid < SHADER_SORT_COUNT; sortid++)
	{
		webostate->rbatches[sortid] = NULL;
		for (batch = mod->batches[sortid]; batch != NULL; batch = batch->next, i++)
		{
			if (!webostate->batches[i].numidx)
				continue;

			if (batch->shader->flags & SHADER_NODRAW)
				continue;

			m = &webostate->batches[i].m;
			webostate->batches[i].pm = m;
			b = &webostate->batches[i].b;
			memcpy(b, batch, sizeof(*b));
			memset(m, 0, sizeof(*m));

			if (b->shader->flags & SHADER_NEEDSARRAYS)
			{	//this ebo cache stuff tracks only indexes, we don't know the actual surfs any more.
				//if NEEDSARRAYS is flagged then the cpu will need access to the mesh data - which it doesn't have.
				//while we could figure out this info, there would be a lot of vertexes that are not referenced, which would be horrendously slow.
				if (b->shader->flags & SHADER_SKY)
					continue;
				b->shader = R_RegisterShader_Vertex("unsupported");
			}

			m->numvertexes = webostate->batches[i].b.vbo->vertcount;
			b->mesh = &webostate->batches[i].pm;
			b->meshes = 1;
			m->numindexes = webostate->batches[i].numidx;
			m->vbofirstelement = webostate->batches[i].idxbuffer - (index_t*)NULL;
			m->vbofirstvert = 0;
			m->indexes = NULL;
			b->vbo = &webostate->batches[i].vbo;
			*b->vbo = *batch->vbo;
			b->vbo->indicies = webostate->ebo;
			b->vbo->vao = 0;

			b->next = webostate->rbatches[sortid];
			webostate->rbatches[sortid] = b;
		}
	}
}
#ifdef Q1BSPS
static void Surf_SimpleWorld_Q1BSP(struct webostate_s *es, qbyte *pvs)
{
	mleaf_t		*leaf;
	msurface_t	*surf, **mark, **end;
	mesh_t		*mesh;
	model_t *wmodel = es->wmodel;
	int l = wmodel->numclusters;
	int fc = -r_framecount;
	for (leaf = wmodel->leafs+l; l-- > 0; leaf--)
	{
		if ((pvs[l>>3] & (1u<<(l&7))) && leaf->nummarksurfaces)
		{
			mark = leaf->firstmarksurface;
			end = mark+leaf->nummarksurfaces;
			while(mark < end)
			{
				surf = *mark++;
				if (surf->visframe != fc)
				{
					int i;
					struct wesbatch_s *eb;
					surf->visframe = fc;
					Surf_RenderDynamicLightmaps_Worker (wmodel, surf, es->lightstylevalues);

					mesh = surf->mesh;
					eb = &es->batches[surf->sbatch->ebobatch];
					if (eb->maxidx < eb->numidx + mesh->numindexes)
					{
						//FIXME: pre-allocate
						eb->maxidx = eb->numidx + surf->mesh->numindexes + 512;
						eb->idxbuffer = BZ_Realloc(eb->idxbuffer, eb->maxidx * sizeof(index_t));
					}
					for (i = 0; i < mesh->numindexes; i++)
						eb->idxbuffer[eb->numidx+i] = mesh->indexes[i] + mesh->vbofirstvert;
					eb->numidx += mesh->numindexes;
				}
			}
		}
	}
}
#endif
#if defined(Q2BSPS) || defined(Q3BSPS)
static void Surf_SimpleWorld_Q3BSP(struct webostate_s *es, qbyte *pvs)
{
	mleaf_t		*leaf;
	msurface_t	*surf, **mark, **end;
	mesh_t		*mesh;
	model_t *wmodel = es->wmodel;
	int l = wmodel->numleafs;	//is this doing submodels too?
	int c;
	int fc = -r_framecount;
	for (leaf = wmodel->leafs; l-- > 0; leaf++)
	{
		c = leaf->cluster;
		if (c < 0)
			continue;	//o.O
		if ((pvs[c>>3] & (1u<<(c&7))) && leaf->nummarksurfaces)
		{
			mark = leaf->firstmarksurface;
			end = mark+leaf->nummarksurfaces;
			while(mark < end)
			{
				surf = *mark++;
				if (surf->visframe != fc)
				{
					int i;
					struct wesbatch_s *eb;
					surf->visframe = fc;

					mesh = surf->mesh;
					eb = &es->batches[surf->sbatch->ebobatch];
					if (eb->maxidx < eb->numidx + mesh->numindexes)
					{
						//FIXME: pre-allocate
						eb->maxidx = eb->numidx + surf->mesh->numindexes + 512;
						eb->idxbuffer = BZ_Realloc(eb->idxbuffer, eb->maxidx * sizeof(index_t));
					}
					for (i = 0; i < mesh->numindexes; i++)
						eb->idxbuffer[eb->numidx+i] = mesh->indexes[i] + mesh->vbofirstvert;
					eb->numidx += mesh->numindexes;
				}
			}
		}
	}
}
#endif
void R_GenWorldEBO(void *ctx, void *data, size_t a, size_t b)
{
	int i;
	struct webostate_s *es = ctx;
	qbyte *pvs;

	es->numbatches = es->wmodel->numbatches;

	for (i = 0; i < es->numbatches; i++)
	{
		es->batches[i].numidx = 0;
		es->batches[i].maxidx = 0;
		es->batches[i].idxbuffer = NULL;
	}

	//maybe we should just use fatpvs instead, and wait for completion when outside?
	if (es->cluster[1] != -1 && es->cluster[0] != es->cluster[1])
	{	//view is near to a water boundary. this implies the water crosses the near clip plane. we need both leafs.
		pvs = es->wmodel->funcs.ClusterPVS(es->wmodel, es->cluster[0], &es->pvs, PVM_REPLACE);
		pvs = es->wmodel->funcs.ClusterPVS(es->wmodel, es->cluster[1], &es->pvs, PVM_MERGE);
	}
	else
		pvs = es->wmodel->funcs.ClusterPVS(es->wmodel, es->cluster[0], &es->pvs, PVM_FAST);

#if defined(Q2BSPS) || defined(Q3BSPS)
	if (es->wmodel->fromgame == fg_quake2 || es->wmodel->fromgame == fg_quake3)
		Surf_SimpleWorld_Q3BSP(es, pvs);
	else
#endif
#ifdef Q1BSPS
	if (es->wmodel->fromgame == fg_quake || es->wmodel->fromgame == fg_halflife)
		Surf_SimpleWorld_Q1BSP(es, pvs);
	else
#endif
	{
		//panic
	}

	COM_AddWork(WG_MAIN, R_GeneratedWorldEBO, es, NULL, 0, 0);
}
#endif

/*
=============
R_DrawWorld
=============
*/

static pvsbuffer_t surf_frustumvis[R_MAX_RECURSE];
void Surf_DrawWorld (void)
{
	//surfvis vs entvis - the key difference is that surfvis is surfaces while entvis is volume. though surfvis should be frustum culled also for lighting. entvis doesn't care.
	qbyte *surfvis, *entvis;
	int areas[2];
	RSpeedLocals();

	if (r_refdef.flags & RDF_NOWORLDMODEL)
	{
		r_refdef.flags |= RDF_NOWORLDMODEL;
		r_refdef.scenevis = NULL;
		BE_DrawWorld(NULL);
		return;
	}
	if (!cl.worldmodel || cl.worldmodel->loadstate != MLS_LOADED)
	{
		/*Don't act as a wallhack*/
		return;
	}

	currentmodel = cl.worldmodel;
	currententity = &r_worldentity;

	{
		RSpeedRemark();

		Surf_LightmapShift(currentmodel);

#ifdef THREADEDWORLD
		if ((r_dynamic.ival < 0 || currentmodel->numbatches) && !r_refdef.recurse && currentmodel->type == mod_brush)
		{
			struct webostate_s *webostate, *best = NULL;
			vec_t bestdist = FLT_MAX;
			for (webostate = webostates; webostate; webostate = webostate->next)
			{
				if (webostate->wmodel != currentmodel)
					continue;
				if (webostate->cluster[0] == r_viewcluster && webostate->cluster[1] == r_viewcluster2)
				{
					best = webostate;
					break;
				}
				else
				{
					vec3_t m;
					float d;
					VectorSubtract(webostate->lastpos, r_refdef.vieworg, m);
					d = DotProduct(m,m);
					if (bestdist > d)
					{
						bestdist = d;
						best = webostate;
					}
				}
			}
			webostate = best;

			if (qrenderer != QR_OPENGL && qrenderer != QR_VULKAN)
				;
#ifdef Q1BSPS
			else if (currentmodel->fromgame == fg_quake || currentmodel->fromgame == fg_halflife)
			{
				int i = cl_max_lightstyles;
				if (webostate && !webogenerating)
					for (i = 0; i < cl_max_lightstyles; i++)
					{
						if (webostate->lightstylevalues[i] != d_lightstylevalue[i])
							break;
					}
				if (webostate && i == cl_max_lightstyles)
				{
				}
				else
				{
					if (!webogenerating)
					{
						int i;
						if (!currentmodel->numbatches)
						{
							int sortid;
							batch_t *batch;
							currentmodel->numbatches = 0;
							for (sortid = 0; sortid < SHADER_SORT_COUNT; sortid++)
								for (batch = currentmodel->batches[sortid]; batch != NULL; batch = batch->next)
								{
									batch->ebobatch = currentmodel->numbatches;
									currentmodel->numbatches++;
								}
						}
						webogeneratingstate = true;
						webogenerating = BZ_Malloc(sizeof(*webogenerating) + sizeof(webogenerating->batches[0]) * (currentmodel->numbatches-1) + currentmodel->pvsbytes);
						webogenerating->wmodel = currentmodel;
						webogenerating->cluster[0] = r_viewcluster;
						webogenerating->cluster[1] = r_viewcluster2;
						webogenerating->pvs.buffer = (qbyte*)(webogenerating+1) + sizeof(webogenerating->batches[0])*(currentmodel->numbatches-1);
						webogenerating->pvs.buffersize = currentmodel->pvsbytes;
						for (i = 0; i < cl_max_lightstyles; i++)
							webogenerating->lightstylevalues[i] = d_lightstylevalue[i];
						Q_strncpyz(webogenerating->dbgid, "webostate", sizeof(webogenerating->dbgid));
						COM_AddWork(WG_LOADER, R_GenWorldEBO, webogenerating, NULL, 0, 0);
					}
				}
			}
#endif
#ifdef Q3BSPS
			else if (currentmodel->fromgame == fg_quake3)
			{
				if (webostate && webostate->cluster[0] == r_viewcluster && webostate->cluster[1] == r_viewcluster2)
				{
				}
				else
				{
					if (!webogenerating)
					{
						if (!currentmodel->numbatches)
						{
							int sortid;
							batch_t *batch;
							currentmodel->numbatches = 0;
							for (sortid = 0; sortid < SHADER_SORT_COUNT; sortid++)
								for (batch = currentmodel->batches[sortid]; batch != NULL; batch = batch->next)
								{
									batch->ebobatch = currentmodel->numbatches;
									currentmodel->numbatches++;
								}
						}
						webogeneratingstate = true;
						webogenerating = BZ_Malloc(sizeof(*webogenerating) + sizeof(webogenerating->batches[0]) * (currentmodel->numbatches-1) + currentmodel->pvsbytes);
						webogenerating->wmodel = currentmodel;
						webogenerating->cluster[0] = r_viewcluster;
						webogenerating->cluster[1] = r_viewcluster2;
						webogenerating->pvs.buffer = (qbyte*)(webogenerating+1) + sizeof(webogenerating->batches[0])*(currentmodel->numbatches-1);
						webogenerating->pvs.buffersize = currentmodel->pvsbytes;
						Q_strncpyz(webogenerating->dbgid, "webostate", sizeof(webogenerating->dbgid));
						COM_AddWork(WG_LOADER, R_GenWorldEBO, webogenerating, NULL, 0, 0);
					}
				}
			}
#endif

			if (webostate)
			{
				entvis = surfvis = webostate->pvs.buffer;

				RSpeedEnd(RSPEED_WORLDNODE);

				areas[0] = 1;
				areas[1] = r_viewarea;
				CL_LinkStaticEntities(entvis, areas);
				TRACE(("dbg: calling R_DrawParticles\n"));
				if (!r_refdef.recurse && !(r_refdef.flags & RDF_DISABLEPARTICLES))
					P_DrawParticles ();

				TRACE(("dbg: calling BE_DrawWorld\n"));
				r_refdef.scenevis = surfvis;
				BE_DrawWorld(webostate->rbatches);

				/*FIXME: move this away*/
				if (currentmodel->fromgame == fg_quake || currentmodel->fromgame == fg_halflife)
					Surf_LessenStains();
				return;
			}
		}
#endif


		Surf_PushChains(currentmodel->batches);

#ifdef TERRAIN
		if (currentmodel->type == mod_heightmap)
		{
			frustumvis = NULL;
			entvis = surfvis = NULL;
		}
		else
#endif
#if defined(Q2BSPS) || defined(Q3BSPS)
		if (currentmodel->fromgame == fg_quake2 || currentmodel->fromgame == fg_quake3)
		{
			pvsbuffer_t *vis = &surf_frustumvis[r_refdef.recurse];
			if (vis->buffersize < currentmodel->pvsbytes)
				vis->buffer = BZ_Realloc(vis->buffer, vis->buffersize=currentmodel->pvsbytes);
			frustumvis = vis->buffer;
			memset(frustumvis, 0, currentmodel->pvsbytes);

			if (!r_refdef.areabitsknown)
			{	//generate the info each frame, as the gamecode didn't tell us what to use.
				int leafnum = CM_PointLeafnum (currentmodel, r_refdef.vieworg);
				int clientarea = CM_LeafArea (currentmodel, leafnum);
				CM_WriteAreaBits(currentmodel, r_refdef.areabits, clientarea, false);
				r_refdef.areabitsknown = true;
			}
#ifdef Q3BSPS
			if (currentmodel->fromgame == fg_quake3)
			{
				entvis = surfvis = R_MarkLeaves_Q3 ();
				Surf_RecursiveQ3WorldNode (currentmodel->nodes, (1<<r_refdef.frustum_numworldplanes)-1);
				//Surf_LeafWorldNode ();
			}
			else
#endif
#ifdef Q2BSPS
			if (currentmodel->fromgame == fg_quake2)
			{
				entvis = surfvis = R_MarkLeaves_Q2 ();
				VectorCopy (r_refdef.vieworg, modelorg);
				Surf_RecursiveQ2WorldNode (currentmodel->nodes);
			}
			else
#endif
			{
				entvis = surfvis = NULL;
			}

			surfvis = frustumvis;
		}
		else
#endif
#ifdef MAP_PROC
			if (currentmodel->fromgame == fg_doom3)
		{
			entvis = surfvis = D3_CalcVis(currentmodel, r_origin);
		}
		else
#endif
#ifdef MAP_DOOM
			if (currentmodel->fromgame == fg_doom)
		{
			entvis = surfvis = NULL;
			R_DoomWorld();
		}
		else
#endif
#ifdef Q1BSPS
		if (1)
		{
			//extern cvar_t temp1;
//			if (0)//temp1.value)
//				entvis = surfvis = R_MarkLeafSurfaces_Q1();
//			else
			{
				pvsbuffer_t *vis = &surf_frustumvis[r_refdef.recurse];

				entvis = R_MarkLeaves_Q1 (false);
				if (!(r_novis.ival & 2))
					VectorCopy (r_origin, modelorg);

				if (vis->buffersize < currentmodel->pvsbytes)
					vis->buffer = BZ_Realloc(vis->buffer, vis->buffersize=currentmodel->pvsbytes);
				frustumvis = vis->buffer;
				memset(frustumvis, 0, currentmodel->pvsbytes);

				if (r_refdef.useperspective)
					Surf_RecursiveWorldNode (currentmodel->nodes, 0x1f);
				else
					Surf_OrthoRecursiveWorldNode (currentmodel->nodes, 0x1f);
				surfvis = frustumvis;
			}
		}
		else
#endif
		{
			frustumvis = NULL;
			entvis = surfvis = NULL;
		}

		RSpeedEnd(RSPEED_WORLDNODE);

		areas[0] = 1;
		areas[1] = r_viewarea;
		r_refdef.sceneareas = areas;
		if (!(r_refdef.flags & RDF_NOWORLDMODEL))
		{
			CL_LinkStaticEntities(entvis, r_refdef.sceneareas);
			TRACE(("dbg: calling R_DrawParticles\n"));
			if (!r_refdef.recurse && !(r_refdef.flags & RDF_DISABLEPARTICLES))
				P_DrawParticles ();
		}

		TRACE(("dbg: calling BE_DrawWorld\n"));
		r_refdef.scenevis = surfvis;
		BE_DrawWorld(cl.worldmodel->batches);

		Surf_PopChains(cl.worldmodel->batches);

		/*FIXME: move this away*/
		if (cl.worldmodel->fromgame == fg_quake || cl.worldmodel->fromgame == fg_halflife)
			Surf_LessenStains();
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

#ifdef THREADEDWORLD
	while(webogenerating)
		COM_WorkerPartialSync(webogenerating, &webogeneratingstate, true);
	while (webostates)
	{
		void *webostate = webostates;
		webostates = webostates->next;
		R_DestroyWorldEBO(webostate);
	}
#endif

	for (i = 0; i < numlightmaps; i++)
	{
		if (!lightmap[i])
			continue;
		if (!lightmap[i]->external)
			Image_DestroyTexture(lightmap[i]->lightmap_texture);
		BZ_Free(lightmap[i]);
		lightmap[i] = NULL;
	}

	if (lightmap)
		BZ_Free(lightmap);

	for (i = 0; i < R_MAX_RECURSE; i++)
		Z_Free(surf_frustumvis[i].buffer);
	memset(surf_frustumvis, 0, sizeof(surf_frustumvis));

	CL_FreeDlights();

	lightmap=NULL;
	numlightmaps=0;

	Alias_Shutdown();
	Shader_ResetRemaps();
}

void Surf_Clear(model_t *mod)
{
	int i;
	vbo_t *vbo;
//	if (mod->fromgame == fg_doom3)
//		return;/*they're on the hunk*/

#ifdef THREADEDWORLD
	struct webostate_s **link, *t;
	while(webogenerating)
		COM_WorkerPartialSync(webogenerating, &webogeneratingstate, true);

	for (link = &webostates; (t=*link); )
	{
		if (t->wmodel == mod)
		{
			*link = t->next;
			R_DestroyWorldEBO(t);
		}
		else
			link = &(*link)->next;
	}
#endif

	while(mod->vbos)
	{
		vbo = mod->vbos;
		mod->vbos = vbo->next;
		BE_ClearVBO(vbo, false);
	}

	if (!mod->submodelof)
	{
		for (i = 0; i < mod->numtextures; i++)
		{
			R_UnloadShader(mod->textures[i]->shader);
			mod->textures[i]->shader = NULL;
		}
	}
	mod->numtextures = 0;

	BZ_Free(mod->shadowbatches);
	mod->numshadowbatches = 0;
	mod->shadowbatches = NULL;
#ifdef RTLIGHTS
	Sh_PurgeShadowMeshes();
#endif

	BZ_Free(blocklights);
	BZ_Free(blocknormals);
	blocklights = NULL;
	blocknormals = NULL;
	maxblocksize = 0;
}

uploadfmt_t Surf_NameToFormat(const char *nam)
{
	static uploadfmt_t tab[] = {PTI_L8, PTI_RGB8, PTI_BGRA8, PTI_A2BGR10, PTI_E5BGR9, PTI_RGBA16F, PTI_RGBA32F, PTI_RGB565, PTI_RGBA4444, PTI_RGBA5551};
	int idx = atoi(nam)-1;
	if (idx>=0 && idx < countof(tab))
		return tab[idx];

	if (!Q_strcasecmp(nam, "e5bgr9") || !Q_strcasecmp(nam, "rgb9e5"))
		return PTI_E5BGR9;	//prefered hdr format, for some reason.
	if (!Q_strcasecmp(nam, "a2bgr10") || !Q_strcasecmp(nam, "rgb10a2") || !Q_strcasecmp(nam, "rgb10"))
		return PTI_A2BGR10;	//prefered ldr format. hurrah for 10 bits.
	if (!Q_strcasecmp(nam, "rgba32f"))
		return PTI_RGBA32F;	//big bulky hdr format
	if (!Q_strcasecmp(nam, "rgba16f"))
		return PTI_RGBA16F;	//tolerable hdr format
//	if (!Q_strcasecmp(nam, "rgba8s"))
//		return PTI_RGBA8_SIGNED;
	if (!Q_strcasecmp(nam, "rgb565") || !Q_strcasecmp(nam, "rgb5"))
		return PTI_RGB565;	//boo hiss
	if (!Q_strcasecmp(nam, "rgba4444") || !Q_strcasecmp(nam, "rgba4"))
		return PTI_RGBA4444;	//erk
	if (!Q_strcasecmp(nam, "rgba5551") || !Q_strcasecmp(nam, "rgba51") || !Q_strcasecmp(nam, "rgb5a1"))
		return PTI_RGBA5551;
	if (!Q_strcasecmp(nam, "argb4444"))
		return PTI_ARGB4444;
	if (!Q_strcasecmp(nam, "argb1555"))
		return PTI_ARGB1555;
	if (!Q_strcasecmp(nam, "rgbx8") || !Q_strcasecmp(nam, "bgrx8") || !Q_strcasecmp(nam, "rgba8") || !Q_strcasecmp(nam, "bgra8"))
		return PTI_BGRX8;	//most common formats...
	if (!Q_strcasecmp(nam, "rgb8") || !Q_strcasecmp(nam, "bgr8"))
		return PTI_RGB8;	//generally not recommended (misaligned so the gpu has to compensate)
	if (!Q_strcasecmp(nam, "l8"))
		return PTI_L8;
	if (*nam)
		Con_Printf("Unknown lightmap format: %s\n", nam);
	return PTI_INVALID;
}

//pick fastest mode for lightmap data
uploadfmt_t Surf_LightmapMode(model_t *model)
{
	uploadfmt_t fmt = Surf_NameToFormat(r_lightmap_format.string);
	if (!sh_config.texfmt[fmt])
	{
		qboolean hdr = (vid.flags&VID_SRGBAWARE), rgb = false;

		if (fmt != PTI_INVALID)
			Con_Printf("lightmap format %s not supported by renderer\n", r_lightmap_format.string);

		if (model)
		{
			switch (model->lightmaps.fmt)
			{
			case LM_E5BGR9:
				hdr = rgb = true;
				break;
			case LM_RGB8:
				rgb = true;
				break;
			case LM_L8:
				break;
			}
			if (model->deluxdata)
				rgb = true;

			if (model->terrain)	//the terrain code requires rgba8.
				hdr = false;
		}

		if (sh_config.texfmt[PTI_E5BGR9] && hdr)
			fmt = PTI_E5BGR9;
		else if (sh_config.texfmt[PTI_RGBA16F] && hdr)
			fmt = PTI_RGBA16F;
		else if (sh_config.texfmt[PTI_RGBA32F] && hdr)
			fmt = PTI_RGBA32F;
		else if (sh_config.texfmt[PTI_A2BGR10] && rgb)
			fmt = PTI_A2BGR10;
		else if (sh_config.texfmt[PTI_L8] && !rgb && !r_deluxemapping && r_dynamic.ival<=0)
			fmt = PTI_L8;
		else if (sh_config.texfmt[PTI_BGRX8])
			fmt = PTI_BGRX8;
		else if (sh_config.texfmt[PTI_RGB8])
			fmt = PTI_RGB8;
		else
			fmt = PTI_RGBX8;

	}

	if (!model->submodelof)
		Con_DPrintf("%s: Using lightmap format %s\n", model->name, Image_FormatName(fmt));

	return fmt;
}

//needs to be followed by a BE_UploadAllLightmaps at some point
int Surf_NewLightmaps(int count, int width, int height, uploadfmt_t fmt, qboolean deluxe)
{
	int first = numlightmaps;
	int i;

	unsigned int pixbytes, pixw, pixh;
	unsigned int dpixbytes, dpixw, dpixh;
	uploadfmt_t dfmt;

	if (!count)
		return -1;

	if (deluxe && (count & 1))
	{
		deluxe = false;
//		count+=1;
		Con_Print("WARNING: Deluxemapping with odd number of lightmaps\n");
	}

	Image_BlockSizeForEncoding(fmt, &pixbytes, &pixw, &pixh);
	if (pixw != 1 || pixh != 1)
		return -1;	//compressed formats are unsupported
	dfmt = PTI_A2BGR10;	//favour this one, because it tends to be slightly faster.
	if (!sh_config.texfmt[dfmt])
		dfmt = PTI_BGRX8;
	if (!sh_config.texfmt[dfmt])
		dfmt = PTI_RGBX8;
	if (!sh_config.texfmt[dfmt])
		dfmt = PTI_RGB8;
	Image_BlockSizeForEncoding(dfmt, &dpixbytes, &dpixw, &dpixh);

	Sys_LockMutex(com_resourcemutex);

	i = numlightmaps + count;
	lightmap = BZ_Realloc(lightmap, sizeof(*lightmap)*(i));
	while(i > first)
	{
		i--;

		if (deluxe && ((i - numlightmaps)&1))
		{	//deluxemaps always use a specific format.
			lightmap[i] = Z_Malloc(sizeof(*lightmap[i]) + (sizeof(qbyte)*dpixbytes)*width*height);
			lightmap[i]->width = width;
			lightmap[i]->height = height;
			lightmap[i]->lightmaps = (qbyte*)(lightmap[i]+1);
			lightmap[i]->stainmaps = NULL;
			lightmap[i]->hasdeluxe = false;
			lightmap[i]->pixbytes = dpixbytes;
			lightmap[i]->fmt = dfmt;
		}
		else
		{
			lightmap[i] = Z_Malloc(sizeof(*lightmap[i]) + (sizeof(qbyte)*pixbytes + sizeof(stmap)*3)*width*height);
			lightmap[i]->width = width;
			lightmap[i]->height = height;
			lightmap[i]->lightmaps = (qbyte*)(lightmap[i]+1);
			lightmap[i]->stainmaps = (stmap*)(lightmap[i]->lightmaps+pixbytes*width*height);
			lightmap[i]->hasdeluxe = deluxe;
			lightmap[i]->pixbytes = pixbytes;
			lightmap[i]->fmt = fmt;
		}

		lightmap[i]->rectchange.l = 0;
		lightmap[i]->rectchange.t = 0;
		lightmap[i]->rectchange.b = lightmap[i]->height;
		lightmap[i]->rectchange.r = lightmap[i]->width;


		lightmap[i]->lightmap_texture = r_nulltex;
		lightmap[i]->modified = true;
//			lightmap[i]->shader = NULL;
		lightmap[i]->external = false;
		// reset stainmap since it now starts at 255
		if (lightmap[i]->stainmaps)
			memset(lightmap[i]->stainmaps, 255, width*height*3*sizeof(stmap));
	}

	numlightmaps += count;

	Sys_UnlockMutex(com_resourcemutex);

	return first;
}
int Surf_NewExternalLightmaps(int count, char *filepattern, qboolean deluxe)
{
	unsigned int nulllight = 0xffffffff;
	unsigned int nulldeluxe = 0xffff7f7f;
	int first = numlightmaps;
	int i;
	char nname[MAX_QPATH];
	qboolean odd = (count & 1) && deluxe;

	if (!count)
		return -1;

	if (odd)
		count++;

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
		lightmap[i]->hasdeluxe = (deluxe && !((i - numlightmaps)&1));

		Q_snprintfz(nname, sizeof(nname), filepattern, i - numlightmaps);

		TEXASSIGN(lightmap[i]->lightmap_texture, R_LoadHiResTexture(nname, NULL, (r_lightmap_nearest.ival?IF_NEAREST:IF_LINEAR)|IF_NOMIPMAP));
		if (lightmap[i]->lightmap_texture->status == TEX_LOADING)
			COM_WorkerPartialSync(lightmap[i]->lightmap_texture, &lightmap[i]->lightmap_texture->status, TEX_LOADING);
		if (lightmap[i]->lightmap_texture->status == TEX_FAILED)
		{
			if ((i&1) && deluxe)
				lightmap[i]->lightmap_texture = R_LoadReplacementTexture("*nulldeluxe", NULL, IF_LOADNOW, &nulldeluxe, 1, 1, TF_RGBX32);
			else
				lightmap[i]->lightmap_texture = R_LoadReplacementTexture("*nulllight", NULL, IF_LOADNOW, &nulllight, 1, 1, TF_RGBX32);
		}
		lightmap[i]->width = lightmap[i]->lightmap_texture->width;
		lightmap[i]->height = lightmap[i]->lightmap_texture->height;
		lightmap[i]->fmt = lightmap[i]->lightmap_texture->format;
	}

	if (odd)
	{
		i = numlightmaps+count-1;
		if (!TEXVALID(lightmap[i]->lightmap_texture))
		{	//FIXME: no deluxemaps after all...
			Z_Free(lightmap[i]);
			lightmap[i] = NULL;
			count--;
		}
	}

	numlightmaps += count;

	return first;
}

void Surf_BuildModelLightmaps (model_t *m)
{
	int		i;
	int shift;
	msurface_t *surf;
	batch_t *batch;
	int sortid;
	int newfirst;
	uploadfmt_t fmt;

	if (m->loadstate != MLS_LOADED)
		return;

#ifdef TERRAIN
	//easiest way to deal with heightmap lightmaps is to just purge the entire thing.
	if (m->terrain)
		Terr_PurgeTerrainModel(m, false, false);	//FIXME: cop out. middle arg should be 'true'.
#endif

	if (m->type != mod_brush)
		return;

	if (!m->lightmaps.count)
		return;

	currentmodel = m;
	shift = Surf_LightmapShift(currentmodel);

	fmt = Surf_LightmapMode(m);

#ifdef THREADEDWORLD
	//make sure nothing is poking the lightmaps while we're rewriting them
	while(webogenerating)
		COM_WorkerPartialSync(webogenerating, &webogeneratingstate, true);
#endif

	R_BumpLightstyles(m->lightmaps.maxstyle);	//should only really happen with lazy loading

	if (m->submodelof && m->fromgame == fg_quake3)	//FIXME: should be all bsp formats
	{
		if (m->submodelof->loadstate != MLS_LOADED)
			return;
		newfirst = m->submodelof->lightmaps.first;
	}
	else
	{
		if (!m->lightdata && m->lightmaps.count && m->fromgame == fg_quake3)
		{
			char pattern[MAX_QPATH];
			COM_StripAllExtensions(m->name, pattern, sizeof(pattern));
			Q_strncatz(pattern, "/lm_%04u.tga", sizeof(pattern));
			newfirst = Surf_NewExternalLightmaps(m->lightmaps.count, pattern, m->lightmaps.deluxemapping);
			m->lightmaps.count = numlightmaps - newfirst;
		}
		else
			newfirst = Surf_NewLightmaps(m->lightmaps.count, m->lightmaps.width, m->lightmaps.height, fmt, m->lightmaps.deluxemapping);
	}

	//fixup batch lightmaps
	for (sortid = 0; sortid < SHADER_SORT_COUNT; sortid++)
	for (batch = m->batches[sortid]; batch != NULL; batch = batch->next)
	{
		for (i = 0; i < MAXRLIGHTMAPS; i++)
		{
			if (batch->lightmap[i] < 0)
				continue;
			batch->lightmap[i] = batch->lightmap[i] - m->lightmaps.first + newfirst;
		}
	}

	if (m->fromgame == fg_quake3)
	{
		int j;
		unsigned char *src, *stop;
		unsigned char *dst;


		//fixup surface lightmaps, and paint
		for (i=0; i<m->nummodelsurfaces; i++)
		{
			surf = m->surfaces + i + m->firstmodelsurface;
			for (j = 0; j < MAXRLIGHTMAPS; j++)
			{
				if (surf->lightmaptexturenums[j] < m->lightmaps.first)
				{
					surf->lightmaptexturenums[j] = -1;
					continue;
				}
				if (surf->lightmaptexturenums[j] >= m->lightmaps.first+m->lightmaps.count)
				{
					surf->lightmaptexturenums[j] = -1;
					continue;
				}
				surf->lightmaptexturenums[j] = surf->lightmaptexturenums[0] - m->lightmaps.first + newfirst;
			}
		}

		if (!m->submodelof)
		for (i = 0; i < m->lightmaps.count; i++)
		{
			if (lightmap[newfirst+i]->external)
				continue;

			dst = lightmap[newfirst+i]->lightmaps;
			src = m->lightdata + i*m->lightmaps.width*m->lightmaps.height*3;
			stop = m->lightdata + (i+1)*m->lightmaps.width*m->lightmaps.height*3;
			if (stop-m->lightdata > m->lightdatasize)
				stop = m->lightdata + m->lightdatasize;
			if (m->lightdata)
			{
				switch(lightmap[newfirst+i]->fmt)
				{
				default:
					Sys_Error("Bad lightmap_fmt\n");
					break;
				case PTI_A2BGR10:
					for (; src < stop; dst += 4, src += 3)
						*(unsigned int*)dst = (0x3<<30) | (src[2]<<22) | (src[1]<<12) | (src[0]<<2);
					break;
				case PTI_E5BGR9:
					for (; src < stop; dst += 4, src += 3)
						*(unsigned int*)dst = Surf_PackE5BRG9(src[0], src[1], src[2], 8);
					break;
				case PTI_BGRA8:
				case PTI_BGRX8:
					for (; src < stop; dst += 4, src += 3)
					{
						dst[0] = src[2];
						dst[1] = src[1];
						dst[2] = src[0];
						dst[3] = 255;
					}
					break;
				/*case TF_RGBA32:
					for (; src < stop; dst += 4, src += 3)
					{
						dst[0] = src[0];
						dst[1] = src[1];
						dst[2] = src[2];
						dst[3] = 255;
					}
					break;
				case TF_BGR24:
					for (; src < stop; dst += 3, src += 3)
					{
						dst[0] = src[2];
						dst[1] = src[1];
						dst[2] = src[0];
					}
					break;*/
				case TF_RGB24:
					for (; src < stop; dst += 3, src += 3)
					{
						dst[0] = src[0];
						dst[1] = src[1];
						dst[2] = src[2];
					}
					break;
				case PTI_RGB565:
					for (; src < stop; dst += 2, src += 3)
						*(unsigned short*)dst = ((src[0]>>3)<<11)|((src[1]>>2)<<5)|((src[2]>>3)<<0);
					break;
				case PTI_L8:
					for (; src < stop; dst += 1, src += 3)
					{
						dst[0] = max(max(src[0], src[1]), src[2]);
					}
					break;
				}
			}
		}
	}
	else
	{
		int j;

//		if (*m->name == '*')
//		{
//			if (!cl.worldmodel || cl.worldmodel->loadstate != MLS_LOADED)
//				return;
//		}
		//fixup surface lightmaps, and paint
		for (i=0; i<m->nummodelsurfaces; i++)
		{
			surf = m->surfaces + i + m->firstmodelsurface;
			for (j = 0; j < MAXRLIGHTMAPS; j++)
			{
				if (surf->lightmaptexturenums[j] < m->lightmaps.first)
				{
					surf->lightmaptexturenums[j] = -1;
					continue;
				}
				if (surf->lightmaptexturenums[j] >= m->lightmaps.first+m->lightmaps.count)
				{
					surf->lightmaptexturenums[j] = -1;
					continue;
				}
				surf->lightmaptexturenums[j] = surf->lightmaptexturenums[j] - m->lightmaps.first + newfirst;

				Surf_BuildLightMap (m, surf, j, shift, r_ambient.value*255, d_lightstylevalue);
			}
		}
	}
	m->lightmaps.first = newfirst;
}

void Surf_ClearLightmaps(void)
{
#ifdef THREADEDWORLD
	while(webogenerating)
		COM_WorkerPartialSync(webogenerating, &webogeneratingstate, true);
	while (webostates)
	{
		void *webostate = webostates;
		webostates = webostates->next;
		R_DestroyWorldEBO(webostate);
	}
#endif
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
	int		i;
	model_t	*m;

	extern model_t	*mod_known;
	extern int		mod_numknown;

	int maxstyle;

	//make sure the lightstyle values are correct (and be sure that the sizes cover all models).
	for (i = 0, maxstyle=0; i < mod_numknown; i++)
	{
		m = &mod_known[i];
		if (m->loadstate == MLS_LOADED)
			if (maxstyle < m->lightmaps.maxstyle)
				maxstyle = m->lightmaps.maxstyle;
	}
	R_BumpLightstyles(maxstyle);	//should only really happen with lazy loading
	R_AnimateLight();

	r_framecount = 1;		// no dlightcache

	while(numlightmaps > 0)
	{
		numlightmaps--;
		if (!lightmap[numlightmaps])
			continue;

		if (!lightmap[numlightmaps]->external)
			Image_DestroyTexture(lightmap[numlightmaps]->lightmap_texture);
		BZ_Free(lightmap[numlightmaps]);
		lightmap[numlightmaps] = NULL;
	}

	r_oldviewcluster = -1;
	r_oldviewcluster2 = -1;

	//FIXME: unload stuff that's no longer relevant somehow.
	for (i = 0; i < mod_numknown; i++)
	{
		m = &mod_known[i];
		if (m->loadstate != MLS_LOADED)
			continue;
		Surf_BuildModelLightmaps(m);
	}
	BE_UploadAllLightmaps();
}



/*
===============
Surf_NewMap
===============
*/
void Surf_NewMap (void)
{
	char namebuf[MAX_QPATH];
	extern cvar_t host_mapname;
#ifdef BEF_PUSHDEPTH
	extern cvar_t r_polygonoffset_submodel_maps;
	char *s;
#endif
	int		i;

	//evil haxx
	r_dynamic.ival = r_dynamic.value;
	if (r_dynamic.ival > 0 && cl.worldmodel->fromgame == fg_quake3) //quake3 has no lightmaps, disable r_dynamic
		r_dynamic.ival = 0;

	memset (&r_worldentity, 0, sizeof(r_worldentity));
	AngleVectors(r_worldentity.angles, r_worldentity.axis[0], r_worldentity.axis[1], r_worldentity.axis[2]);
	VectorInverse(r_worldentity.axis[1]);
	r_worldentity.model = cl.worldmodel;
	Vector4Set(r_worldentity.shaderRGBAf, 1, 1, 1, 1);
	VectorSet(r_worldentity.light_avg, 1, 1, 1);


	if (cl.worldmodel)
		COM_StripExtension(COM_SkipPath(cl.worldmodel->name), namebuf, sizeof(namebuf));
	else
		*namebuf = '\0';
	Cvar_Set(&host_mapname, namebuf);

	Surf_DeInit();

	r_viewcluster = -1;
	r_oldviewcluster = 0;
	r_viewcluster2 = -1;
	r_oldviewcluster2 = 0;
#ifdef BEF_PUSHDEPTH
	r_pushdepth = false;
	for (s = r_polygonoffset_submodel_maps.string; s && *s; )
	{
		s = COM_Parse(s);
		if (*com_token)
			if (wildcmp(com_token, namebuf))
			{
				r_pushdepth = true;
				break;
			}
	}
#endif

	TRACE(("dbg: Surf_NewMap: clear particles\n"));
	P_ClearParticles ();
	CL_RegisterParticles();

	Shader_DoReload();
	if (cl.worldmodel)
	{
		if (cl.worldmodel->loadstate == MLS_LOADING)
			COM_WorkerPartialSync(cl.worldmodel, &cl.worldmodel->loadstate, MLS_LOADING);
		Mod_ParseInfoFromEntityLump(cl.worldmodel);
	}
	Shader_DoReload();

	if (!pe)
		Cvar_ForceCallback(&r_particlesystem);
	R_Clutter_Purge();
TRACE(("dbg: Surf_NewMap: wiping them stains (getting the cloth out)\n"));
	Surf_WipeStains();
TRACE(("dbg: Surf_NewMap: building lightmaps\n"));
	Surf_BuildLightmaps ();


TRACE(("dbg: Surf_NewMap: ui\n"));
#ifdef VM_UI
	UI_Reset();
#endif
TRACE(("dbg: Surf_NewMap: tp\n"));
	TP_NewMap();

	for (i = 0; i < cl.num_statics; i++)
	{
		vec3_t mins, maxs;
		//fixme: no rotation
		if (!cl_static_entities[i].ent.model && cl_static_entities[i].mdlidx > 0 && cl_static_entities[i].mdlidx < countof(cl.model_precache))
			cl_static_entities[i].ent.model = cl.model_precache[cl_static_entities[i].mdlidx];
		else if (!cl_static_entities[i].ent.model && cl_static_entities[i].mdlidx < 0 && (-cl_static_entities[i].mdlidx) < countof(cl.model_csqcprecache))
			cl_static_entities[i].ent.model = cl.model_csqcprecache[-cl_static_entities[i].mdlidx];
		if (cl_static_entities[i].ent.model)
		{
			//unfortunately, we need to know the actual size so that we can get this right. bum.
			if (cl_static_entities[i].ent.model->loadstate == MLS_NOTLOADED)
				Mod_LoadModel(cl_static_entities[i].ent.model, MLV_WARNSYNC);
			if (cl_static_entities[i].ent.model->loadstate == MLS_LOADING)
				COM_WorkerPartialSync(cl_static_entities[i].ent.model, &cl_static_entities[i].ent.model->loadstate, MLS_LOADING);
			VectorAdd(cl_static_entities[i].ent.origin, cl_static_entities[i].ent.model->mins, mins);
			VectorAdd(cl_static_entities[i].ent.origin, cl_static_entities[i].ent.model->maxs, maxs);
		}
		else
		{
			VectorCopy(mins, cl_static_entities[i].ent.origin);
			VectorCopy(maxs, cl_static_entities[i].ent.origin);
		}
		if (cl.worldmodel->funcs.FindTouchedLeafs)
			cl.worldmodel->funcs.FindTouchedLeafs(cl.worldmodel, &cl_static_entities[i].ent.pvscache, mins, maxs);
		cl_static_entities[i].emit = NULL;
	}

	CL_InitDlights();
#ifdef RTLIGHTS
	Sh_PreGenerateLights();
#endif
}

void Surf_PreNewMap(void)
{
	r_loadbumpmapping = r_deluxemapping || r_glsl_offsetmapping.ival;
#ifdef RTLIGHTS
	r_loadbumpmapping |= r_shadow_realtime_world.ival || r_shadow_realtime_dlight.ival;
#endif
	r_viewcluster = -1;
	r_oldviewcluster = -1;
	r_viewcluster2 = -1;
	r_oldviewcluster2 = -1;

	Shader_DoReload();
}



static float sgn(float a)
{
    if (a > 0.0F) return (1.0F);
    if (a < 0.0F) return (-1.0F);
    return (0.0F);
}
void R_ObliqueNearClip(float *viewmat, mplane_t *wplane)
{
	float f;
	vec4_t q, c;
	vec3_t ping, pong;
	vec4_t vplane;

	//convert world plane into view space
	Matrix4x4_CM_Transform3x3(viewmat, wplane->normal, vplane);
	VectorScale(wplane->normal, wplane->dist, ping);
	Matrix4x4_CM_Transform3(viewmat, ping, pong);
	vplane[3] = -DotProduct(pong, vplane);

	// Calculate the clip-space corner point opposite the clipping plane
	// as (sgn(clipPlane.x), sgn(clipPlane.y), 1, 1) and
	// transform it into camera space by multiplying it
	// by the inverse of the projection matrix

	q[0] = (sgn(vplane[0]) + r_refdef.m_projection_std[8]) / r_refdef.m_projection_std[0];
	q[1] = (sgn(vplane[1]) + fabs(r_refdef.m_projection_std[9])) / fabs(r_refdef.m_projection_std[5]);
	q[2] = -1.0F;
	q[3] = (1.0F + r_refdef.m_projection_std[10]) / r_refdef.m_projection_std[14];

	// Calculate the scaled plane vector
	f = 2.0F / DotProduct4(vplane, q);
	Vector4Scale(vplane, f, c);

	// Replace the third row of the projection matrix
	r_refdef.m_projection_std[2] = c[0];
	r_refdef.m_projection_std[6] = c[1];
	r_refdef.m_projection_std[10] = c[2] + 1.0F;
	r_refdef.m_projection_std[14] = c[3];
}


#endif
