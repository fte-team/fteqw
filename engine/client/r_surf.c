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
#include <math.h>

extern cvar_t r_ambient;
extern cvar_t gl_bump;

static vec3_t			modelorg;	/*set before recursively entering the visible surface finder*/
static qbyte			areabits[MAX_Q2MAP_AREAS/8];

model_t		*currentmodel;

int		lightmap_bytes;		// 1, 3 or 4
qboolean lightmap_bgra;

texid_t	*lightmap_textures;
texid_t	*deluxmap_textures;

#define MAX_LIGHTMAP_SIZE LMBLOCK_WIDTH

vec3_t			blocknormals[MAX_LIGHTMAP_SIZE*MAX_LIGHTMAP_SIZE];
unsigned		blocklights[3*MAX_LIGHTMAP_SIZE*MAX_LIGHTMAP_SIZE];

lightmapinfo_t **lightmap;
int numlightmaps;

mleaf_t		*r_vischain;		// linked list of visible leafs

extern cvar_t r_stains;
extern cvar_t r_loadlits;
extern cvar_t r_stainfadetime;
extern cvar_t r_stainfadeammount;

int Surf_LightmapShift (model_t *model)
{
	extern cvar_t gl_overbright_all, gl_lightmap_shift;

	if (gl_overbright_all.ival || (model->engineflags & MDLF_NEEDOVERBRIGHT))
		return bound(0, gl_lightmap_shift.ival, 2);
	return 0;
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

	lim = 255 - (r_stains.value*255);

#define stain(x)							\
	change = stainbase[(s)*3+x] + amm*parms[4+x];	\
	stainbase[(s)*3+x] = bound(lim, change, 255);

	if (surf->lightmaptexturenum < 0)
		return;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;

	stainbase = lightmap[surf->lightmaptexturenum]->stainmaps;
	stainbase += (surf->light_t * LMBLOCK_WIDTH + surf->light_s) * 3;

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
		stainbase += 3*LMBLOCK_WIDTH;
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
		memset(lightmap[i]->stainmaps, 255, sizeof(lightmap[i]->stainmaps));
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
			surf->cached_dlight=-1;//nice hack here...

			smax = (surf->extents[0]>>4)+1;
			tmax = (surf->extents[1]>>4)+1;	

			stain = lightmap[surf->lightmaptexturenum]->stainmaps;
			stain += (surf->light_t * LMBLOCK_WIDTH + surf->light_s) * 3;

			stride = (LMBLOCK_WIDTH-smax)*3;

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

		a = 256*(cl_dlights[lnum].color[0]*1.5 + cl_dlights[lnum].color[1]*2.95 + cl_dlights[lnum].color[2]*0.55);

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

		a = 256*(cl_dlights[lnum].color[0]*1.5 + cl_dlights[lnum].color[1]*2.95 + cl_dlights[lnum].color[2]*0.55);

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

		r = cl_dlights[lnum].color[0]*3*256;
		g = cl_dlights[lnum].color[1]*3*256;
		b = cl_dlights[lnum].color[2]*3*256;

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
						bl[0]		+= (rad - dist)*r;					
						bl[1]	+= (rad - dist)*g;
						bl[2]	+= (rad - dist)*b;
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

	int stride = LMBLOCK_WIDTH*3;

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

	stride -= smax*3;

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

			dest += 3;
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
	int r, g, b, t;
	int cr, cg, cb;
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

				cr = 0;
				cg = 0;
				cb = 0;

				if (r > 255)	//ak too much red
				{
					cr -= (255-r)/2;
					cg += (255-r)/4;	//reduce it, and indicate to drop the others too.
					cb += (255-r)/4;
					r = 255;
				}			

				if (g > 255)
				{					
					cr += (255-g)/4;
					cg -= (255-g)/2;
					cb += (255-g)/4;
					g = 255;
				}				

				if (b > 255)
				{
					cr += (255-b)/4;
					cg += (255-b)/4;
					cb -= (255-b)/2;
					b = 255;
				}

				r+=cr;
				if (r > 255)
					dest[2] = 255;
				else if (r < 0)
					dest[2] = 0;
				else
					dest[2] = r;

				g+=cg;
				if (g > 255)
					dest[1] = 255;
				else if (g < 0)
					dest[1] = 0;
				else
					dest[1] = g;

				b+=cb;
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

				cr = 0;
				cg = 0;
				cb = 0;

				if (r > 255)	//ak too much red
				{
					cr -= (255-r)/2;
					cg += (255-r)/4;	//reduce it, and indicate to drop the others too.
					cb += (255-r)/4;
					r = 255;
				}			
				
				if (g > 255)
				{					
					cr += (255-g)/4;
					cg -= (255-g)/2;
					cb += (255-g)/4;
					g = 255;
				}				

				if (b > 255)
				{
					cr += (255-b)/4;
					cg += (255-b)/4;
					cb -= (255-b)/2;
					b = 255;
				}

				r+=cr;
				if (r > 255)
					dest[0] = 255;
				else if (r < 0)
					dest[0] = 0;
				else
					dest[0] = (r+cr);

				g+=cg;
				if (g > 255)
					dest[1] = 255;
				else if (g < 0)
					dest[1] = 0;
				else
					dest[1] = g;

				b+=cb;
				if (b > 255)
					dest[2] = 255;
				else if (b < 0)
					dest[2] = 0;
				else
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
	extern cvar_t gl_lightmap_shift;

	int stride = LMBLOCK_WIDTH*lightmap_bytes;

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

		if (lightmap_bytes == 4)
		{
			if (lightmap_bgra)
			{
				if (!r_stains.value || !surf->stained)
					Surf_StoreLightmap(dest, smax, tmax, shift, bgra4_os, NULL);
				else
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
				if (!r_stains.value || !surf->stained)
					Surf_StoreLightmap(dest, smax, tmax, shift, rgb3_os, NULL);
				else
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

		if (!r_stains.value || !surf->stained)
			Surf_StoreLightmap(dest, smax, tmax, shift, lum, NULL);
		else
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
void Surf_RenderDynamicLightmaps (msurface_t *fa, int shift)
{
	qbyte		*base, *luxbase;
	stmap *stainbase;
	int			maps;
	glRect_t    *theRect;
	int smax, tmax;

	if (!fa->mesh)
		return;

	//surfaces without lightmaps
	if (fa->lightmaptexturenum<0)
		return;

	//surfaces with lightmaps that do not animate, supposedly
	if (fa->texinfo->flags & (TI_SKY|TI_TRANS33|TI_TRANS66|TI_WARP))
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

		lightmap[fa->lightmaptexturenum]->modified = true;

		smax = (fa->extents[0]>>4)+1;
		tmax = (fa->extents[1]>>4)+1;

		theRect = &lightmap[fa->lightmaptexturenum]->rectchange;
		if (fa->light_t < theRect->t) {
			if (theRect->h)
				theRect->h += theRect->t - fa->light_t;
			theRect->t = fa->light_t;
		}
		if (fa->light_s < theRect->l) {
			if (theRect->w)
				theRect->w += theRect->l - fa->light_s;
			theRect->l = fa->light_s;
		}
		if ((theRect->w + theRect->l) < (fa->light_s + smax))
			theRect->w = (fa->light_s-theRect->l)+smax;
		if ((theRect->h + theRect->t) < (fa->light_t + tmax))
			theRect->h = (fa->light_t-theRect->t)+tmax;

		if (gl_bump.ival)
		{
			lightmap[fa->lightmaptexturenum]->deluxmodified = true;
			theRect = &lightmap[fa->lightmaptexturenum]->deluxrectchange;
			if (fa->light_t < theRect->t) {
				if (theRect->h)
					theRect->h += theRect->t - fa->light_t;
				theRect->t = fa->light_t;
			}
			if (fa->light_s < theRect->l) {
				if (theRect->w)
					theRect->w += theRect->l - fa->light_s;
				theRect->l = fa->light_s;
			}

			if ((theRect->w + theRect->l) < (fa->light_s + smax))
				theRect->w = (fa->light_s-theRect->l)+smax;
			if ((theRect->h + theRect->t) < (fa->light_t + tmax))
				theRect->h = (fa->light_t-theRect->t)+tmax;

			luxbase = lightmap[fa->lightmaptexturenum]->deluxmaps;
			luxbase += fa->light_t * LMBLOCK_WIDTH * 3 + fa->light_s * 3;
		}
		else
			luxbase = NULL;


		base = lightmap[fa->lightmaptexturenum]->lightmaps;
		base += fa->light_t * LMBLOCK_WIDTH * lightmap_bytes + fa->light_s * lightmap_bytes;
		stainbase = lightmap[fa->lightmaptexturenum]->stainmaps;
		stainbase += (fa->light_t * LMBLOCK_WIDTH + fa->light_s) * 3;
		Surf_BuildLightMap (fa, base, luxbase, stainbase, shift, r_ambient.value*255);

		RSpeedEnd(RSPEED_DYNAMIC);
	}
}

void Surf_RenderAmbientLightmaps (msurface_t *fa, int shift, int ambient)
{
	qbyte		*base, *luxbase;
	stmap *stainbase;
	glRect_t    *theRect;
	int smax, tmax;

	if (!fa->mesh)
		return;

	//surfaces without lightmaps
	if (fa->lightmaptexturenum<0)
		return;

	//surfaces with lightmaps that do not animate, supposedly
	if (fa->texinfo->flags & (TI_SKY|TI_TRANS33|TI_TRANS66|TI_WARP))
		return;

	if (fa->cached_light[0] != ambient || fa->cached_colour[0] != 0xff)
		goto dynamic;

	if (fa->dlightframe == r_framecount	// dynamic this frame
		|| fa->cached_dlight)			// dynamic previously
	{
		RSpeedLocals();
dynamic:
		RSpeedRemark();

		lightmap[fa->lightmaptexturenum]->modified = true;

		smax = (fa->extents[0]>>4)+1;
		tmax = (fa->extents[1]>>4)+1;

		theRect = &lightmap[fa->lightmaptexturenum]->rectchange;
		if (fa->light_t < theRect->t) {
			if (theRect->h)
				theRect->h += theRect->t - fa->light_t;
			theRect->t = fa->light_t;
		}
		if (fa->light_s < theRect->l) {
			if (theRect->w)
				theRect->w += theRect->l - fa->light_s;
			theRect->l = fa->light_s;
		}
		if ((theRect->w + theRect->l) < (fa->light_s + smax))
			theRect->w = (fa->light_s-theRect->l)+smax;
		if ((theRect->h + theRect->t) < (fa->light_t + tmax))
			theRect->h = (fa->light_t-theRect->t)+tmax;

		if (gl_bump.ival)
		{
			lightmap[fa->lightmaptexturenum]->deluxmodified = true;
			theRect = &lightmap[fa->lightmaptexturenum]->deluxrectchange;
			if (fa->light_t < theRect->t) {
				if (theRect->h)
					theRect->h += theRect->t - fa->light_t;
				theRect->t = fa->light_t;
			}
			if (fa->light_s < theRect->l) {
				if (theRect->w)
					theRect->w += theRect->l - fa->light_s;
				theRect->l = fa->light_s;
			}

			if ((theRect->w + theRect->l) < (fa->light_s + smax))
				theRect->w = (fa->light_s-theRect->l)+smax;
			if ((theRect->h + theRect->t) < (fa->light_t + tmax))
				theRect->h = (fa->light_t-theRect->t)+tmax;

			luxbase = lightmap[fa->lightmaptexturenum]->deluxmaps;
			luxbase += fa->light_t * LMBLOCK_WIDTH * 3 + fa->light_s * 3;
		}
		else
			luxbase = NULL;


		base = lightmap[fa->lightmaptexturenum]->lightmaps;
		base += fa->light_t * LMBLOCK_WIDTH * lightmap_bytes + fa->light_s * lightmap_bytes;
		stainbase = lightmap[fa->lightmaptexturenum]->stainmaps;
		stainbase += (fa->light_t * LMBLOCK_WIDTH + fa->light_s) * 3;
		Surf_BuildLightMap (fa, base, luxbase, stainbase, shift, -1-ambient);

		RSpeedEnd(RSPEED_DYNAMIC);
	}
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/

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
					Surf_RenderDynamicLightmaps (surf, shift);

					tex->vbo.meshlist[j] = NULL;
					surf->sbatch->mesh[surf->sbatch->meshes++] = surf->mesh;
				}
			}
		}
	}
	return vis;
}

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
	int shift;

start:

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

		shift = Surf_LightmapShift(cl.worldmodel);

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

				Surf_RenderDynamicLightmaps (surf, shift);
				surf->sbatch->mesh[surf->sbatch->meshes++] = surf->mesh;
			}
		}
	}

// recurse down the back side
	//GLR_RecursiveWorldNode (node->children[!side], clipflags);
	node = node->children[!side];
	goto start;
}

#ifdef Q2BSPS
static void Surf_RecursiveQ2WorldNode (mnode_t *node)
{
	int			c, side;
	mplane_t	*plane;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	double		dot;
	int shift;

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

	shift = Surf_LightmapShift(currentmodel);

	// draw stuff
	for ( c = node->numsurfaces, surf = currentmodel->surfaces + node->firstsurface; c ; c--, surf++)
	{
		if (surf->visframe != r_framecount)
			continue;

		if ( (surf->flags & SURF_PLANEBACK) != sidebit )
			continue;		// wrong side

		surf->visframe = r_framecount+1;//-1;

		Surf_RenderDynamicLightmaps (surf, shift);

		surf->sbatch->mesh[surf->sbatch->meshes++] = surf->mesh;
	}


// recurse down the back side
	Surf_RecursiveQ2WorldNode (node->children[!side]);
}
#endif

#ifdef Q3BSPS
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

			for (i=0,clipplane=frustum ; i<4 ; i++,clipplane++)
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

			if (i != 4)
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
	vec3_t	temp;

	R_AnimateLight();
	r_framecount++;

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
	else
	{
		r_oldviewleaf = r_viewleaf;
		r_oldviewleaf2 = r_viewleaf2;
		if (r_refdef.recurse)
			r_viewleaf = RMod_PointInLeaf (cl.worldmodel, r_refdef.pvsorigin);
		else
			r_viewleaf = RMod_PointInLeaf (cl.worldmodel, r_origin);

		if (!r_viewleaf)
		{
		}
		else if (r_viewleaf->contents == Q1CONTENTS_EMPTY)
		{	//look down a bit			
			VectorCopy (r_origin, temp);
			temp[2] -= 16;
			leaf = RMod_PointInLeaf (cl.worldmodel, temp);
			if (leaf->contents <= Q1CONTENTS_WATER && leaf->contents >= Q1CONTENTS_LAVA)
				r_viewleaf2 = leaf;
			else
				r_viewleaf2 = NULL;
		}
		else if (r_viewleaf->contents <= Q1CONTENTS_WATER && r_viewleaf->contents >= Q1CONTENTS_LAVA)
		{	//in water, look up a bit.
		
			VectorCopy (r_origin, temp);
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
			V_SetContentsColor (r_viewleaf->contents);
	}
}


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

void Surf_GenBrushBatches(batch_t **batches, entity_t *ent)
{
	int i;
	msurface_t *s;
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
	if (model->fromgame != fg_quake3)
	{
		int k;
		int shift;

		currententity = ent;
		currentmodel = ent->model;
		if (model->nummodelsurfaces != 0 && r_dynamic.ival)
		{
			for (k=rtlights_first; k<RTL_FIRST; k++)
			{
				if (!cl_dlights[k].radius)
					continue;
				if (!(cl_dlights[k].flags & LFLAG_ALLOW_LMHACK))
					continue;

				model->funcs.MarkLights (&cl_dlights[k], 1<<k,
					model->nodes + model->hulls[0].firstclipnode);
			}
		}

		shift = Surf_LightmapShift(model);
		if ((ent->drawflags & MLS_MASKIN) == MLS_ABSLIGHT)
		{
			//update lightmaps.
			for (s = model->surfaces+model->firstmodelsurface,i = 0; i < model->nummodelsurfaces; i++, s++)
				Surf_RenderAmbientLightmaps (s, shift, ent->abslight);
		}
		else if (ent->drawflags & DRF_TRANSLUCENT)
		{
			//update lightmaps.
			for (s = model->surfaces+model->firstmodelsurface,i = 0; i < model->nummodelsurfaces; i++, s++)
				Surf_RenderAmbientLightmaps (s, shift, 255);
		}
		else
		{
			//update lightmaps.
			for (s = model->surfaces+model->firstmodelsurface,i = 0; i < model->nummodelsurfaces; i++, s++)
				Surf_RenderDynamicLightmaps (s, shift);
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
	else if (ent->shaderRGBAf[3] < 1 && cls.protocol != CP_QUAKE3)
		bef |= BEF_FORCETRANSPARENT;
	if (ent->flags & RF_NODEPTHTEST)
		bef |= BEF_FORCENODEPTH;

	b = NULL;
	for (s = model->surfaces+model->firstmodelsurface,i = 0; i < model->nummodelsurfaces; i++, s++)
	{
		if (!b || b->lightmap != s->lightmaptexturenum || b->texture != s->texinfo->texture || b->surf_count >= sizeof(surfbatchmeshes)/sizeof(surfbatchmeshes[0]))
		{
			b = BE_GetTempBatch();
			if (!b)
				break;
			b->buildmeshes = NULL;
			b->ent = ent;
			b->texture = s->texinfo->texture;
			b->shader = R_TextureAnimation(ent->framestate.g[FS_REG].frame[0], b->texture)->shader;
			b->skin = &b->shader->defaulttextures;
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
			b->surf_first = s - model->surfaces;
			b->surf_count = 0;
			b->buildmeshes = Surf_BuildBrushBatch;
			b->meshes = 0;
			b->firstmesh = 0;
			b->lightmap = s->lightmaptexturenum;
			b->mesh = NULL;
			b->vbo = NULL;
		}

		b->surf_count++;
		b->meshes++;
	}
}

/*
=============
R_DrawWorld
=============
*/

void Surf_DrawWorld (void)
{
	qbyte *vis;
	RSpeedLocals();

	if (r_refdef.flags & Q2RDF_NOWORLDMODEL)
	{
		r_refdef.flags |= Q2RDF_NOWORLDMODEL;
		BE_DrawWorld(NULL);
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

#ifdef MAP_DOOM
	if (currentmodel->fromgame = fg_doom)
		GLR_DoomWorld();
	else
#endif
#ifdef TERRAIN
	if (currentmodel->type == mod_heightmap)
		GL_DrawHeightmapModel(currententity);
	else
#endif
	{
		RSpeedRemark();

#ifdef Q2BSPS
		if (cl.worldmodel->fromgame == fg_quake2 || cl.worldmodel->fromgame == fg_quake3)
		{
			int leafnum;
			int clientarea;
#ifdef QUAKE2
			if (cls.protocol == CP_QUAKE2)	//we can get server sent info
				memcpy(areabits, cl.q2frame.areabits, sizeof(areabits));
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
				vis = R_MarkLeaves_Q3 ();
				Surf_LeafWorldNode ();
			}
			else
#endif
			{
				vis = R_MarkLeaves_Q2 ();
				VectorCopy (r_refdef.vieworg, modelorg);
				Surf_RecursiveQ2WorldNode (cl.worldmodel->nodes);
			}
		}
		else
#endif
		{
			//extern cvar_t temp1;
			if (0)//temp1.value)
				vis = R_MarkLeafSurfaces_Q1();
			else
			{
				vis = R_MarkLeaves_Q1 ();
				VectorCopy (r_refdef.vieworg, modelorg);
				Surf_RecursiveWorldNode (cl.worldmodel->nodes, 0xf);
			}
		}

		CL_LinkStaticEntities(vis);

		RSpeedEnd(RSPEED_WORLDNODE);
		TRACE(("dbg: calling BE_DrawWorld\n"));
		BE_DrawWorld(vis);


		/*FIXME: move this away*/
		if (cl.worldmodel->fromgame == fg_quake || cl.worldmodel->fromgame == fg_halflife)
			Surf_LessenStains();

		Surf_CleanChains();
	}
}



/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/

// returns a texture number and the position inside it
static int Surf_LM_AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
	int		texnum;

	for (texnum=0 ;  ; texnum++)
	{
		if (texnum == numlightmaps)	//allocate 4 more lightmap slots. not much memory usage, but we don't want any caps here.
		{
			lightmap = BZ_Realloc(lightmap, sizeof(*lightmap)*(numlightmaps+4)); 
			lightmap[numlightmaps+0] = NULL;
			lightmap[numlightmaps+1] = NULL;
			lightmap[numlightmaps+2] = NULL;
			lightmap[numlightmaps+3] = NULL;

			lightmap_textures = BZ_Realloc(lightmap_textures, sizeof(*lightmap_textures)*(numlightmaps+4));
			lightmap_textures[numlightmaps+0] = R_AllocNewTexture(LMBLOCK_WIDTH, LMBLOCK_HEIGHT);
			lightmap_textures[numlightmaps+1] = R_AllocNewTexture(LMBLOCK_WIDTH, LMBLOCK_HEIGHT);
			lightmap_textures[numlightmaps+2] = R_AllocNewTexture(LMBLOCK_WIDTH, LMBLOCK_HEIGHT);
			lightmap_textures[numlightmaps+3] = R_AllocNewTexture(LMBLOCK_WIDTH, LMBLOCK_HEIGHT);

			deluxmap_textures = BZ_Realloc(deluxmap_textures, sizeof(*deluxmap_textures)*(numlightmaps+4));
			deluxmap_textures[numlightmaps+0] = R_AllocNewTexture(LMBLOCK_WIDTH, LMBLOCK_HEIGHT);
			deluxmap_textures[numlightmaps+1] = R_AllocNewTexture(LMBLOCK_WIDTH, LMBLOCK_HEIGHT);
			deluxmap_textures[numlightmaps+2] = R_AllocNewTexture(LMBLOCK_WIDTH, LMBLOCK_HEIGHT);
			deluxmap_textures[numlightmaps+3] = R_AllocNewTexture(LMBLOCK_WIDTH, LMBLOCK_HEIGHT);
			numlightmaps+=4;
		}
		if (!lightmap[texnum])
		{
			lightmap[texnum] = Z_Malloc(sizeof(*lightmap[texnum]));
			lightmap[texnum]->meshchain = NULL;
			lightmap[texnum]->modified = true;
			// reset stainmap since it now starts at 255
			memset(lightmap[texnum]->stainmaps, 255, sizeof(lightmap[texnum]->stainmaps));

			//clear out the deluxmaps incase there is none on the map.
			for (j = 0; j < LMBLOCK_HEIGHT*LMBLOCK_HEIGHT*3; j+=3)
			{
				lightmap[texnum]->deluxmaps[j+0] = 128;
				lightmap[texnum]->deluxmaps[j+1] = 128;
				lightmap[texnum]->deluxmaps[j+2] = 255;
			}
		}


		best = LMBLOCK_HEIGHT;

		for (i=0 ; i<LMBLOCK_WIDTH-w ; i++)
		{
			best2 = 0;

			for (j=0 ; j<w ; j++)
			{
				if (lightmap[texnum]->allocated[i+j] >= best)
					break;
				if (lightmap[texnum]->allocated[i+j] > best2)
					best2 = lightmap[texnum]->allocated[i+j];
			}
			if (j == w)
			{	// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > LMBLOCK_HEIGHT)
			continue;

		for (i=0 ; i<w ; i++)
			lightmap[texnum]->allocated[*x + i] = best + h;

		return texnum;
	}

	Sys_Error ("AllocBlock: full");
	return 0;
}

//quake3 maps have their lightmaps in gl style already.
//rather than forgetting that and redoing it, let's just keep the data.
static int Surf_LM_FillBlock (int texnum, int w, int h, int x, int y)
{
	int		i, l;
	while (texnum >= numlightmaps)	//allocate 4 more lightmap slots. not much memory usage, but we don't want any caps here.
	{
		lightmap = BZ_Realloc(lightmap, sizeof(*lightmap)*(numlightmaps+4)); 
		lightmap[numlightmaps+0] = NULL;
		lightmap[numlightmaps+1] = NULL;
		lightmap[numlightmaps+2] = NULL;
		lightmap[numlightmaps+3] = NULL;

		lightmap_textures = BZ_Realloc(lightmap_textures, sizeof(*lightmap_textures)*(numlightmaps+4)); 
		lightmap_textures[numlightmaps+0] = R_AllocNewTexture(LMBLOCK_WIDTH, LMBLOCK_HEIGHT);
		lightmap_textures[numlightmaps+1] = R_AllocNewTexture(LMBLOCK_WIDTH, LMBLOCK_HEIGHT);
		lightmap_textures[numlightmaps+2] = R_AllocNewTexture(LMBLOCK_WIDTH, LMBLOCK_HEIGHT);
		lightmap_textures[numlightmaps+3] = R_AllocNewTexture(LMBLOCK_WIDTH, LMBLOCK_HEIGHT);

		deluxmap_textures = BZ_Realloc(deluxmap_textures, sizeof(*deluxmap_textures)*(numlightmaps+4)); 
		deluxmap_textures[numlightmaps+0] = R_AllocNewTexture(LMBLOCK_WIDTH, LMBLOCK_HEIGHT);
		deluxmap_textures[numlightmaps+1] = R_AllocNewTexture(LMBLOCK_WIDTH, LMBLOCK_HEIGHT);
		deluxmap_textures[numlightmaps+2] = R_AllocNewTexture(LMBLOCK_WIDTH, LMBLOCK_HEIGHT);
		deluxmap_textures[numlightmaps+3] = R_AllocNewTexture(LMBLOCK_WIDTH, LMBLOCK_HEIGHT);
		numlightmaps+=4;
	}
	for (i = texnum; i >= 0; i--)
	{
		if (!lightmap[i])
		{
			lightmap[i] = BZ_Malloc(sizeof(*lightmap[i]));
			lightmap[i]->meshchain = NULL;
			lightmap[i]->modified = true;
			for (l=0 ; l<LMBLOCK_HEIGHT ; l++)
			{
				lightmap[i]->allocated[l] = LMBLOCK_HEIGHT;
			}
			lightmap[i]->rectchange.l = 0;
			lightmap[i]->rectchange.t = 0;
			lightmap[i]->rectchange.w = LMBLOCK_WIDTH;
			lightmap[i]->rectchange.h = LMBLOCK_HEIGHT;

			//clear out the deluxmaps incase there is none on the map.
			for (l = 0; l < LMBLOCK_HEIGHT*LMBLOCK_HEIGHT*3; l+=3)
			{
				lightmap[i]->deluxmaps[l+0] = 0;
				lightmap[i]->deluxmaps[l+1] = 0;
				lightmap[i]->deluxmaps[l+2] = 255;
			}

			if (cl.worldmodel->lightdata)
			{
				memcpy(lightmap[i]->lightmaps, cl.worldmodel->lightdata+3*LMBLOCK_HEIGHT*LMBLOCK_HEIGHT*i, LMBLOCK_HEIGHT*LMBLOCK_HEIGHT*3);
			}
			else
			{
				char basename[MAX_QPATH];
				//maybe someone screwed with my lightmap...
				memset(lightmap[i]->lightmaps, 255, LMBLOCK_HEIGHT*LMBLOCK_HEIGHT*3);

				COM_StripExtension(cl.worldmodel->name, basename, sizeof(basename));
				lightmap_textures[i] = R_LoadHiResTexture(va("%s/lm_%04i", basename, i), NULL, IF_NOALPHA|IF_NOGAMMA);
				lightmap[i]->modified = false;
			}

		}
		else
			break;
	}
	return texnum;
}

/*
================
BuildSurfaceDisplayList
FIXME: this is probably misplaced
lightmaps are already built by the time this is called
================
*/
void Surf_BuildSurfaceDisplayList (model_t *model, msurface_t *fa)
{
	int			i, lindex, lnumverts;
	medge_t		*pedges, *r_pedge;
	int			vertpage;
	float		*vec;
	float		s, t;
	int	lm;

// reconstruct the polygon
	pedges = model->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	if (!lnumverts)
	{
		fa->mesh = &nullmesh;
		return;
	}

	{	//build a nice mesh instead of a poly.
		int size = sizeof(mesh_t) + sizeof(index_t)*(lnumverts-2)*3 + (sizeof(vecV_t) + 3*sizeof(vec3_t) + 2*sizeof(vec2_t) + sizeof(vec4_t))*lnumverts;
		mesh_t *mesh;

		fa->mesh = mesh = Hunk_Alloc(size);
		mesh->xyz_array = (vecV_t*)(mesh + 1);
		mesh->normals_array = (vec3_t*)(mesh->xyz_array + lnumverts);
		mesh->snormals_array = (vec3_t*)(mesh->normals_array + lnumverts);
		mesh->tnormals_array = (vec3_t*)(mesh->snormals_array + lnumverts);
		mesh->st_array = (vec2_t*)(mesh->tnormals_array + lnumverts);
		mesh->lmst_array = (vec2_t*)(mesh->st_array + lnumverts);
		mesh->colors4f_array = (vec4_t*)(mesh->lmst_array + lnumverts);
		mesh->indexes = (index_t*)(mesh->colors4f_array + lnumverts);

		mesh->numindexes = (lnumverts-2)*3;
		mesh->numvertexes = lnumverts;
		mesh->istrifan = true;

		for (i=0 ; i<lnumverts-2 ; i++)
		{
			mesh->indexes[i*3] = 0;
			mesh->indexes[i*3+1] = i+1;
			mesh->indexes[i*3+2] = i+2;
		}

		for (i=0 ; i<lnumverts ; i++)
		{
			lindex = model->surfedges[fa->firstedge + i];

			if (lindex > 0)
			{
				r_pedge = &pedges[lindex];
				vec = model->vertexes[r_pedge->v[0]].position;
			}
			else
			{
				r_pedge = &pedges[-lindex];
				vec = model->vertexes[r_pedge->v[1]].position;
			}

			s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
			t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];

			VectorCopy (vec, mesh->xyz_array[i]);
			mesh->st_array[i][0] = s/fa->texinfo->texture->width;
			mesh->st_array[i][1] = t/fa->texinfo->texture->height;

			s -= fa->texturemins[0];
			lm = s*fa->light_t;
			s += fa->light_s*16;
			s += 8;
			s /= LMBLOCK_WIDTH*16;

			t -= fa->texturemins[1];
			lm += t;
			t += fa->light_t*16;
			t += 8;
			t /= LMBLOCK_HEIGHT*16;

			mesh->lmst_array[i][0] = s;
			mesh->lmst_array[i][1] = t;

			if (fa->flags & SURF_PLANEBACK)
				VectorNegate(fa->plane->normal, mesh->normals_array[i]);
			else
				VectorCopy(fa->plane->normal, mesh->normals_array[i]);
			VectorNegate(fa->texinfo->vecs[0], mesh->snormals_array[i]);
			VectorNegate(fa->texinfo->vecs[1], mesh->tnormals_array[i]);
			VectorNormalize(mesh->snormals_array[i]);
			VectorNormalize(mesh->tnormals_array[i]);

			mesh->colors4f_array[i][0] = 1;
			mesh->colors4f_array[i][1] = 1;
			mesh->colors4f_array[i][2] = 1;
			mesh->colors4f_array[i][3] = 1;
		}
	}
}

/*
========================
GL_CreateSurfaceLightmap
========================
*/
static void Surf_CreateSurfaceLightmap (msurface_t *surf, int shift)
{
	int		smax, tmax;
	qbyte	*base, *luxbase; stmap *stainbase;

	if (surf->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
		surf->lightmaptexturenum = -1;
	if (surf->texinfo->flags & TEX_SPECIAL)
		surf->lightmaptexturenum = -1;
	if (surf->lightmaptexturenum<0)
	{
		surf->lightmaptexturenum = -1;
		return;
	}

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;

	if (smax > LMBLOCK_WIDTH || tmax > LMBLOCK_HEIGHT || smax < 0 || tmax < 0)
	{	//whoa, buggy.
		surf->lightmaptexturenum = -1;
		return;
	}

	if (currentmodel->fromgame == fg_quake3)
		Surf_LM_FillBlock(surf->lightmaptexturenum, smax, tmax, surf->light_s, surf->light_t);
	else
		surf->lightmaptexturenum = Surf_LM_AllocBlock (smax, tmax, &surf->light_s, &surf->light_t);
	base = lightmap[surf->lightmaptexturenum]->lightmaps;
	base += (surf->light_t * LMBLOCK_WIDTH + surf->light_s) * lightmap_bytes;

	luxbase = lightmap[surf->lightmaptexturenum]->deluxmaps;
	luxbase += (surf->light_t * LMBLOCK_WIDTH + surf->light_s) * 3;

	stainbase = lightmap[surf->lightmaptexturenum]->stainmaps;
	stainbase += (surf->light_t * LMBLOCK_WIDTH + surf->light_s) * 3;

	Surf_BuildLightMap (surf, base, luxbase, stainbase, shift, r_ambient.value*255);
}



void Surf_DeInit(void)
{
	int i;

	for (i = 0; i < numlightmaps; i++)
	{
		if (!lightmap[i])
			break;
		BZ_Free(lightmap[i]);
		lightmap[i] = NULL;
	}

	if (lightmap_textures)
	{
		for (i = 0; i < numlightmaps; i++)
			R_DestroyTexture(lightmap_textures[i]);
		BZ_Free(lightmap_textures);
	}
	if (lightmap)
		BZ_Free(lightmap);

	lightmap_textures=NULL;
	lightmap=NULL;
	numlightmaps=0;
}

void Surf_Clear(model_t *mod)
{
	batch_t *b;
	int i;
	for (i = 0; i < SHADER_SORT_COUNT; i++)
	{
		while ((b = mod->batches[i]))
		{
			mod->batches[i] = b->next;

			BZ_Free(b->mesh);
			Z_Free(b);
		}
	}
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
	int		i, j, t;
	model_t	*m;
	int shift;
	msurface_t *surf;
	batch_t *batch, *bstop;
	vec3_t sn;
	int sortid;
	int ptype;

	r_framecount = 1;		// no dlightcache

	for (i = 0; i < numlightmaps; i++)
	{
		if (!lightmap[i])
			break;
		BZ_Free(lightmap[i]);
		lightmap[i] = NULL;
	}

	if (cl.worldmodel->fromgame == fg_doom)
		return;	//no lightmaps.

	lightmap_bgra = true;

	switch(qrenderer)
	{
#ifdef D3DQUAKE
	case QR_DIRECT3D:
		/*always bgra, hope your card supports it*/
		lightmap_bytes = 4;
		lightmap_bgra = true;
		break;
#endif
#ifdef GLQUAKE
	case QR_OPENGL:
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
	}

	for (j=1 ; j<MAX_MODELS ; j++)
	{
		m = cl.model_precache[j];
		if (!m)
			break;
		if (m->name[0] == '*')
			continue;

		currentmodel = m;
		shift = Surf_LightmapShift(currentmodel);

		for (t = m->numtextures-1; t >= 0; t--)
		{
			if (m == cl.worldmodel)
				ptype = P_FindParticleType(va("tex_%s", m->textures[t]->name));
			else
				ptype = P_INVALID;
			m->textures[t]->wtexno = t;

			sortid = m->textures[t]->shader->sort;
			bstop = m->batches[sortid];
			batch = NULL;
			for (i=0 ; i<m->numsurfaces ; i++)
			{//extra texture loop so we get slightly less texture switches
				surf = m->surfaces + i;
				if (surf->texinfo->texture == m->textures[t])
				{
					P_EmitSkyEffectTris(m, surf, ptype);
					Surf_CreateSurfaceLightmap (surf, shift);

					/*the excessive logic is to give portals separate batches for separate planes*/
					if (sortid == SHADER_SORT_PORTAL)
					{
						if (surf->flags & SURF_PLANEBACK)
							VectorNegate(surf->plane->normal, sn);
						else
							VectorCopy(surf->plane->normal, sn);
					}
					else
						VectorClear(sn);
					if (!batch || batch->lightmap != surf->lightmaptexturenum || (sortid == SHADER_SORT_PORTAL && !VectorCompare(sn, batch->normal)))
					{
						if (sortid == SHADER_SORT_PORTAL)
						{
							for (batch = m->batches[sortid]; batch != bstop; batch = batch->next)
							{
								if (batch->lightmap == surf->lightmaptexturenum && VectorCompare(sn, batch->normal))
									break;
							}
						}
						else
						{
							for (batch = m->batches[sortid]; batch != bstop; batch = batch->next)
							{
								if (batch->lightmap == surf->lightmaptexturenum)
									break;
							}
						}
						if (batch == bstop)
						{
							batch = Z_Malloc(sizeof(*batch));
							batch->lightmap = surf->lightmaptexturenum;
							batch->texture = m->textures[t];
							batch->next = m->batches[sortid];
							batch->ent = &r_worldentity;
							VectorCopy(sn, batch->normal);
							m->batches[sortid] = batch;
						}
					}
					surf->sbatch = batch;
					batch->maxmeshes++;

					if (m->surfaces[i].mesh)	//there are some surfaces that have a display list already (q3 ones)
						continue;
					Surf_BuildSurfaceDisplayList (m, surf);
				}
			}
		}
		for (sortid = 0; sortid < SHADER_SORT_COUNT; sortid++)
		for (batch = m->batches[sortid]; batch != NULL; batch = batch->next)
		{
			batch->mesh = BZ_Malloc(sizeof(*batch->mesh)*batch->maxmeshes*2);
		}
		BE_GenBrushModelVBO(m);
	}

	BE_UploadAllLightmaps();
}
#endif
