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
#ifdef RGLQUAKE
#include "glquake.h"
#include "shader.h"
#include "renderque.h"
#include <math.h>

int			skytexturenum;

extern cvar_t gl_bump;

extern qbyte			areabits[MAX_Q2MAP_AREAS/8];

model_t		*currentmodel;


int		lightmap_bytes;		// 1, 3 or 4

int		*lightmap_textures;
int		*deluxmap_textures;
int		detailtexture;

#define MAX_LIGHTMAP_SIZE LMBLOCK_WIDTH

vec3_t			blocknormals[MAX_LIGHTMAP_SIZE*MAX_LIGHTMAP_SIZE];
unsigned		blocklights[MAX_LIGHTMAP_SIZE*MAX_LIGHTMAP_SIZE];
#ifdef PEXT_LIGHTSTYLECOL
unsigned		greenblklights[MAX_LIGHTMAP_SIZE*MAX_LIGHTMAP_SIZE];
unsigned		blueblklights[MAX_LIGHTMAP_SIZE*MAX_LIGHTMAP_SIZE];
#endif

lightmapinfo_t **lightmap;
int numlightmaps;

msurface_t  *r_alpha_surfaces = NULL;
extern msurface_t *r_mirror_chain;

mleaf_t		*r_vischain;		// linked list of visible leafs

void R_RenderDynamicLightmaps (msurface_t *fa, int shift);

extern cvar_t gl_detail;
extern cvar_t r_stains;
extern cvar_t r_loadlits;
extern cvar_t r_stainfadetime;
extern cvar_t r_stainfadeammount;

//extern cvar_t gl_lightmapmode;

int GLR_LightmapShift (model_t *model)
{
	extern cvar_t gl_overbright_all, gl_lightmap_shift;

	if (gl_overbright_all.value || (model->engineflags & MDLF_NEEDOVERBRIGHT))
		return bound(0, gl_lightmap_shift.value, 2);
	return 0;
}

//radius, x y z, r g b
void GLR_StainSurf (msurface_t *surf, float *parms)
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
void GLR_StainNode (mnode_t *node, float *parms)
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
		GLR_StainNode (node->children[0], parms);
		return;
	}
	if (dist < (-*parms))
	{
		GLR_StainNode (node->children[1], parms);
		return;
	}

// mark the polygons
	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->flags&~(SURF_DONTWARP|SURF_PLANEBACK))
			continue;
		GLR_StainSurf(surf, parms);
	}

	GLR_StainNode (node->children[0], parms);
	GLR_StainNode (node->children[1], parms);
}
*/

void GLR_StainQ3Node (mnode_t *node, float *parms)
{
//	mplane_t	*splitplane;
//	float		dist;
	int			i;
	
	if (node->contents != -1)
	{
		msurface_t	**mark;
		mleaf_t *leaf;

		// mark the polygons
		leaf = (mleaf_t *)node;
		mark = leaf->firstmarksurface;
		for (i=0 ; i<leaf->nummarksurfaces ; i++)
		{
			GLR_StainSurf(*mark++, parms);
		}

		return;	
	}
	/*
	splitplane = node->plane;
	dist = DotProduct ((parms+1), splitplane->normal) - splitplane->dist;
	
	if (dist > (*parms))
	{
		GLR_StainQ2Node (node->children[0], parms);
		return;
	}
	if (dist < (-*parms))
	{
		GLR_StainQ2Node (node->children[1], parms);
		return;
	}*/

	GLR_StainQ3Node (node->children[0], parms);
	GLR_StainQ3Node (node->children[1], parms);
}

void GLR_AddStain(vec3_t org, float red, float green, float blue, float radius)
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

void GLR_WipeStains(void)
{
	int i;
	for (i = 0; i < numlightmaps; i++)
	{
		if (!lightmap[i])
			break;
		memset(lightmap[i]->stainmaps, 255, sizeof(lightmap[i]->stainmaps));
	}
}

void GLR_LessenStains(void)
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
void GLR_AddDynamicLights (msurface_t *surf)
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

	for (lnum=0 ; lnum<dlights_software ; lnum++)
	{
		if ( !(surf->dlightbits & (1<<lnum) ) )
			continue;		// not lit by this light

		if (cl_dlights[lnum].nodynamic)
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
					blocklights[t*smax + s] += (rad - dist)*a;
			}
		}
	}
}

void GLR_AddDynamicLightNorms (msurface_t *surf)
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

	for (lnum=0 ; lnum<dlights_software ; lnum++)
	{
		if ( !(surf->dlightbits & (1<<lnum) ) )
			continue;		// not lit by this light

		if (cl_dlights[lnum].nodynamic)
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
void GLR_AddDynamicLightsColours (msurface_t *surf)
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

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;

	for (lnum=0 ; lnum<dlights_software ; lnum++)
	{
		if ( !(surf->dlightbits & (1<<lnum) ) )
			continue;		// not lit by this light

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
		{
*/			for (t = 0 ; t<tmax ; t++)
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
						blocklights[t*smax + s]		+= (rad - dist)*r;					
						greenblklights[t*smax + s]	+= (rad - dist)*g;
						blueblklights[t*smax + s]	+= (rad - dist)*b;
					}
				}
			}
//		}
	}
}
#endif



void GLR_BuildDeluxMap (msurface_t *surf, qbyte *dest)
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
			if (temp[2]<0.5)
			{
				temp[2]=0.5;	//don't let it get too dark.
			}
			dest[0] = (temp[0]+1)/2*255;
			dest[1] = (temp[1]+1)/2*255;
			dest[2] = (temp[2]+1)/2*255;
			dest += 3;
			bnorm+=3;
		}
	}		
}

/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
void GLR_BuildLightMap (msurface_t *surf, qbyte *dest, qbyte *deluxdest, stmap *stainsrc, int shift)
{
	int			smax, tmax;
	int			t;
	int			i, j, size;
	qbyte		*lightmap;
	unsigned	scale;
	int			maps;
	unsigned	*bl;
	qboolean isstained;
	extern cvar_t r_ambient;
	extern cvar_t gl_lightmap_shift;
#ifdef PEXT_LIGHTSTYLECOL
	unsigned	*blg;
	unsigned	*blb;

	int r, g, b;
	int cr, cg, cb;
#endif
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
		GLR_BuildDeluxMap(surf, deluxdest);


#ifdef PEXT_LIGHTSTYLECOL
	if (gl_lightmap_format == GL_RGBA || gl_lightmap_format == GL_RGB)
	{
		// set to full bright if no light data
		if (r_fullbright.value>0)	//not qw
		{
			for (i=0 ; i<size ; i++)
			{
				blocklights[i] = r_fullbright.value*255*256;
				greenblklights[i] = r_fullbright.value*255*256;
				blueblklights[i] = r_fullbright.value*255*256;
			}
//			if (r_fullbright.value < 1)
			{
				if (surf->dlightframe == r_framecount)
					GLR_AddDynamicLightsColours (surf);
			}
			goto store;
		}
		if (!currentmodel->lightdata)
		{
			for (i=0 ; i<size ; i++)
			{
				blocklights[i] = 255*256;
				greenblklights[i] = 255*256;
				blueblklights[i] = 255*256;
			}
				if (surf->dlightframe == r_framecount)
					GLR_AddDynamicLightsColours (surf);
			goto store;
		}

// clear to no light
		t = r_ambient.value*255;
		for (i=0 ; i<size ; i++)
		{
			blocklights[i] = t;
			greenblklights[i] = t;
			blueblklights[i] = t;
		}

// add all the lightmaps
		if (lightmap)
		{
			if (currentmodel->fromgame == fg_quake3)	//rgb
			{
			/*	for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
					 maps++)	//no light styles in q3 apparently.
				{
					scale = d_lightstylevalue[surf->styles[maps]];
					surf->cached_light[maps] = scale;	// 8.8 fraction
					surf->cached_colour[maps] = cl_lightstyle[surf->styles[maps]].colour;
				}
					 */
				for (i = 0; i < tmax; i++)	//q3 maps store their light in a block fashion, q1/q2/hl store it in a linear fashion.
				{
					for (j = 0; j < smax; j++)
					{
						blocklights[i*smax+j]		= 255*lightmap[(i*LMBLOCK_WIDTH+j)*3];
						greenblklights[i*smax+j]	= 255*lightmap[(i*LMBLOCK_WIDTH+j)*3+1];
						blueblklights[i*smax+j]		= 255*lightmap[(i*LMBLOCK_WIDTH+j)*3+2];
					}
				}
//				memset(blocklights, 255, sizeof(blocklights));
			}
			else if (currentmodel->engineflags & MDLF_RGBLIGHTING)	//rgb
			{				
				for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
					 maps++)
				{
					scale = d_lightstylevalue[surf->styles[maps]];
					surf->cached_light[maps] = scale;	// 8.8 fraction
					surf->cached_colour[maps] = cl_lightstyle[surf->styles[maps]].colour;


					if (cl_lightstyle[surf->styles[maps]].colour == 7)	//hopefully a faster alternative.
					{
						for (i=0 ; i<size ; i++)
						{
							blocklights[i]		+= lightmap[i*3  ] * scale;
							greenblklights[i]	+= lightmap[i*3+1] * scale;
							blueblklights[i]	+= lightmap[i*3+2] * scale;
						}
					}
					else
					{
						if (cl_lightstyle[surf->styles[maps]].colour & 1)
							for (i=0 ; i<size ; i++)
								blocklights[i]		+= lightmap[i*3  ] * scale;
						if (cl_lightstyle[surf->styles[maps]].colour & 2)
							for (i=0 ; i<size ; i++)
								greenblklights[i]	+= lightmap[i*3+1] * scale;
						if (cl_lightstyle[surf->styles[maps]].colour & 4)
							for (i=0 ; i<size ; i++)
								blueblklights[i]	+= lightmap[i*3+2] * scale;
					}
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
						for (i=0 ; i<size ; i++)
						{
							blocklights[i]		+= lightmap[i] * scale;
							greenblklights[i]	+= lightmap[i] * scale;
							blueblklights[i]	+= lightmap[i] * scale;
						}
					}
					else
					{
						if (cl_lightstyle[surf->styles[maps]].colour & 1)
							for (i=0 ; i<size ; i++)
								blocklights[i] += lightmap[i] * scale;
						if (cl_lightstyle[surf->styles[maps]].colour & 2)
							for (i=0 ; i<size ; i++)
								greenblklights[i] += lightmap[i] * scale;
						if (cl_lightstyle[surf->styles[maps]].colour & 4)
							for (i=0 ; i<size ; i++)
								blueblklights[i] += lightmap[i] * scale;
					}
					lightmap += size;	// skip to next lightmap
				}
		}

// add all the dynamic lights
		if (surf->dlightframe == r_framecount)
			GLR_AddDynamicLightsColours (surf);
	}
	else
	{
#endif
	// set to full bright if no light data
		if (r_fullbright.value || !currentmodel->lightdata)
		{
			for (i=0 ; i<size ; i++)
				blocklights[i] = 255*256;
			goto store;
		}

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
			GLR_AddDynamicLights (surf);
#ifdef PEXT_LIGHTSTYLECOL
	}
#endif

// bound, invert, and shift
store:
#ifdef INVERTLIGHTMAPS
	switch (gl_lightmap_format)
	{
#ifdef PEXT_LIGHTSTYLECOL
	case GL_RGBA:
		stride -= (smax<<2);
		bl = blocklights;
		blg = greenblklights;
		blb = blueblklights;

		if (!r_stains.value)
			isstained = false;
		else
			isstained = surf->stained;

/*		if (!gl_lightcomponantreduction.value)
		{
			for (i=0 ; i<tmax ; i++, dest += stride)
			{
				for (j=0 ; j<smax ; j++)
				{
					t = *bl++;
					t >>= 7;
					if (t > 255)
						dest[0] = 0;
					else if (t < 0)
						dest[0] = 256;
					else
						dest[0] = (255-t);				

					t = *blg++;
					t >>= 7;
					if (t > 255)
						dest[1] = 0;
					else if (t < 0)
						dest[1] = 256;
					else
						dest[1] = (255-t);

					t = *blb++;
					t >>= 7;
					if (t > 255)
						dest[2] = 0;
					else if (t < 0)
						dest[2] = 256;
					else
						dest[2] = (255-t);

					dest[3] = 0;//(dest[0]+dest[1]+dest[2])/3;
					dest += 4;
				}
			}
		}
		else
*/		{
		stmap *stain;		
			for (i=0 ; i<tmax ; i++, dest += stride)
			{
				stain = stainsrc + i*LMBLOCK_WIDTH*3;
				for (j=0 ; j<smax ; j++)
				{
					r = *bl++;
					g = *blg++;
					b = *blb++;

					r >>= shift;
					g >>= shift;
					b >>= shift;	

					if (isstained)	// merge in stain
					{
						r = (127+r*(*stain++)) >> 8;
						g = (127+g*(*stain++)) >> 8;
						b = (127+b*(*stain++)) >> 8;
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
//					else if (r < 0)					
//						r = 0;				
					
					if (g > 255)
					{					
						cr += (255-g)/4;
						cg -= (255-g)/2;
						cb += (255-g)/4;
						g = 255;
					}
//					else if (g < 0)				
//						g = 0;					

					if (b > 255)
					{
						cr += (255-b)/4;
						cg += (255-b)/4;
						cb -= (255-b)/2;
						b = 255;
					}
//					else if (b < 0)
//						b = 0;
				//*
					if ((r+cr) > 255)
						dest[0] = 0;	//inverse lighting
					else if ((r+cr) < 0)
						dest[0] = 255;
					else
						dest[0] = 255-(r+cr);

					if ((g+cg) > 255)
						dest[1] = 0;
					else if ((g+cg) < 0)
						dest[1] = 255;
					else
						dest[1] = 255-(g+cg);

					if ((b+cb) > 255)
						dest[2] = 0;
					else if ((b+cb) < 0)
						dest[2] = 255;
					else
						dest[2] = 255-(b+cb);
/*/
					if ((r+cr) > 255)
						dest[0] = 255;	//non-inverse lighting
					else if ((r+cr) < 0)
						dest[0] = 0;
					else
						dest[0] = (r+cr);

					if ((g+cg) > 255)
						dest[1] = 255;
					else if ((g+cg) < 0)
						dest[1] = 0;
					else
						dest[1] = (g+cg);

					if ((b+cb) > 255)
						dest[2] = 255;
					else if ((b+cb) < 0)
						dest[2] = 0;
					else
						dest[2] = (b+cb);
*/



					dest[3] = (dest[0]+dest[1]+dest[2])/3;	//alpha?!?!
					dest += 4;					
				}
			}
		}		
		break;

	case GL_RGB:
		stride -= smax*3;
		bl = blocklights;
		blg = greenblklights;
		blb = blueblklights;

		if (!r_stains.value)
			isstained = false;
		else
			isstained = surf->stained;

/*		if (!gl_lightcomponantreduction.value)
		{
			for (i=0 ; i<tmax ; i++, dest += stride)
			{
				for (j=0 ; j<smax ; j++)
				{
					t = *bl++;
					t >>= 7;
					if (t > 255)
						dest[0] = 0;
					else if (t < 0)
						dest[0] = 256;
					else
						dest[0] = (255-t);				

					t = *blg++;
					t >>= 7;
					if (t > 255)
						dest[1] = 0;
					else if (t < 0)
						dest[1] = 256;
					else
						dest[1] = (255-t);

					t = *blb++;
					t >>= 7;
					if (t > 255)
						dest[2] = 0;
					else if (t < 0)
						dest[2] = 256;
					else
						dest[2] = (255-t);

					dest += 3;
				}
			}
		}
		else
*/		{
		stmap *stain;		
			for (i=0 ; i<tmax ; i++, dest += stride)
			{
				stain = stainsrc + i*LMBLOCK_WIDTH*3;
				for (j=0 ; j<smax ; j++)
				{
					r = *bl++;
					g = *blg++;
					b = *blb++;

					r >>= shift;
					g >>= shift;
					b >>= shift;	
					
					if (isstained)	// merge in stain
					{
						r = (127+r*(*stain++)) >> 8;
						g = (127+g*(*stain++)) >> 8;
						b = (127+b*(*stain++)) >> 8;
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
//					else if (r < 0)					
//						r = 0;				
					
					if (g > 255)
					{					
						cr += (255-g)/4;
						cg -= (255-g)/2;
						cb += (255-g)/4;
						g = 255;
					}
//					else if (g < 0)				
//						g = 0;					

					if (b > 255)
					{
						cr += (255-b)/4;
						cg += (255-b)/4;
						cb -= (255-b)/2;
						b = 255;
					}
//					else if (b < 0)
//						b = 0;
				//*
					if ((r+cr) > 255)
						dest[0] = 0;	//inverse lighting
					else if ((r+cr) < 0)
						dest[0] = 255;
					else
						dest[0] = 255-(r+cr);

					if ((g+cg) > 255)
						dest[1] = 0;
					else if ((g+cg) < 0)
						dest[1] = 255;
					else
						dest[1] = 255-(g+cg);

					if ((b+cb) > 255)
						dest[2] = 0;
					else if ((b+cb) < 0)
						dest[2] = 255;
					else
						dest[2] = 255-(b+cb);
/*/
					if ((r+cr) > 255)
						dest[0] = 255;	//non-inverse lighting
					else if ((r+cr) < 0)
						dest[0] = 0;
					else
						dest[0] = (r+cr);

					if ((g+cg) > 255)
						dest[1] = 255;
					else if ((g+cg) < 0)
						dest[1] = 0;
					else
						dest[1] = (g+cg);

					if ((b+cb) > 255)
						dest[2] = 255;
					else if ((b+cb) < 0)
						dest[2] = 0;
					else
						dest[2] = (b+cb);
// */
					dest += 3;	
				}
			}
		}		
		break;
#else
	case GL_RGBA:
		stride -= (smax<<2);
		bl = blocklights;
		for (i=0 ; i<tmax ; i++, dest += stride)
		{
			for (j=0 ; j<smax ; j++)
			{
				t = *bl++;
				t >>= shift;
				if (t > 255)
					t = 255;
				dest[3] = 255-t;
				dest += 4;
			}
		}
		break;
#endif
	case GL_ALPHA:
	case GL_LUMINANCE:
	case GL_INTENSITY:
		bl = blocklights;
		for (i=0 ; i<tmax ; i++, dest += stride)
		{
			for (j=0 ; j<smax ; j++)
			{
				t = *bl++;
				t >>= shift;
				if (t > 255)
					t = 255;
				dest[j] = 255-t;
			}
		}
		break;
	default:
		Sys_Error ("Bad lightmap format");
	}
#else
	switch (gl_lightmap_format)
	{
#ifdef PEXT_LIGHTSTYLECOL
	case GL_RGBA:
		stride -= (smax<<2);
		bl = blocklights;
		blg = greenblklights;
		blb = blueblklights;

		if (!r_stains.value)
			isstained = false;
		else
			isstained = surf->stained;

/*		if (!gl_lightcomponantreduction.value)
		{
			for (i=0 ; i<tmax ; i++, dest += stride)
			{
				for (j=0 ; j<smax ; j++)
				{
					t = *bl++;
					t >>= 7;
					if (t > 255)
						dest[0] = 0;
					else if (t < 0)
						dest[0] = 256;
					else
						dest[0] = (255-t);				

					t = *blg++;
					t >>= 7;
					if (t > 255)
						dest[1] = 0;
					else if (t < 0)
						dest[1] = 256;
					else
						dest[1] = (255-t);

					t = *blb++;
					t >>= 7;
					if (t > 255)
						dest[2] = 0;
					else if (t < 0)
						dest[2] = 256;
					else
						dest[2] = (255-t);

					dest[3] = 0;//(dest[0]+dest[1]+dest[2])/3;
					dest += 4;
				}
			}
		}
		else
*/		{
		stmap *stain;		
			for (i=0 ; i<tmax ; i++, dest += stride)
			{
				stain = stainsrc + i*LMBLOCK_WIDTH*3;
				for (j=0 ; j<smax ; j++)
				{
					r = *bl++;
					g = *blg++;
					b = *blb++;

					r >>= shift;
					g >>= shift;
					b >>= shift;	

					if (isstained)	// merge in stain
					{
						r = (127+r*(*stain++)) >> 8;
						g = (127+g*(*stain++)) >> 8;
						b = (127+b*(*stain++)) >> 8;
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
//					else if (r < 0)					
//						r = 0;				
					
					if (g > 255)
					{					
						cr += (255-g)/4;
						cg -= (255-g)/2;
						cb += (255-g)/4;
						g = 255;
					}
//					else if (g < 0)				
//						g = 0;					

					if (b > 255)
					{
						cr += (255-b)/4;
						cg += (255-b)/4;
						cb -= (255-b)/2;
						b = 255;
					}
//					else if (b < 0)
//						b = 0;
				//*
					if ((r+cr) > 255)
						dest[0] = 0;	//inverse lighting
					else if ((r+cr) < 0)
						dest[0] = 255;
					else
						dest[0] = 255-(r+cr);

					if ((g+cg) > 255)
						dest[1] = 0;
					else if ((g+cg) < 0)
						dest[1] = 255;
					else
						dest[1] = 255-(g+cg);

					if ((b+cb) > 255)
						dest[2] = 0;
					else if ((b+cb) < 0)
						dest[2] = 255;
					else
						dest[2] = 255-(b+cb);
/*/
					if ((r+cr) > 255)
						dest[0] = 255;	//non-inverse lighting
					else if ((r+cr) < 0)
						dest[0] = 0;
					else
						dest[0] = (r+cr);

					if ((g+cg) > 255)
						dest[1] = 255;
					else if ((g+cg) < 0)
						dest[1] = 0;
					else
						dest[1] = (g+cg);

					if ((b+cb) > 255)
						dest[2] = 255;
					else if ((b+cb) < 0)
						dest[2] = 0;
					else
						dest[2] = (b+cb);
*/



					dest[3] = (dest[0]+dest[1]+dest[2])/3;	//alpha?!?!
					dest += 4;					
				}
			}
		}		
		break;

	case GL_RGB:
		stride -= smax*3;
		bl = blocklights;
		blg = greenblklights;
		blb = blueblklights;

		if (!r_stains.value)
			isstained = false;
		else
			isstained = surf->stained;

/*		if (!gl_lightcomponantreduction.value)
		{
			for (i=0 ; i<tmax ; i++, dest += stride)
			{
				for (j=0 ; j<smax ; j++)
				{
					t = *bl++;
					t >>= 7;
					if (t > 255)
						dest[0] = 255;
					else if (t < 0)
						dest[0] = 0;
					else
						dest[0] = t;				

					t = *blg++;
					t >>= 7;
					if (t > 255)
						dest[1] = 255;
					else if (t < 0)
						dest[1] = 0;
					else
						dest[1] = t;

					t = *blb++;
					t >>= 7;
					if (t > 255)
						dest[2] = 255;
					else if (t < 0)
						dest[2] = 0;
					else
						dest[2] = t;

					dest += 3;
				}
			}
		}
		else
*/		{
		stmap *stain;		
			for (i=0 ; i<tmax ; i++, dest += stride)
			{
				stain = stainsrc + i*LMBLOCK_WIDTH*3;
				for (j=0 ; j<smax ; j++)
				{
					r = *bl++;
					g = *blg++;
					b = *blb++;

					r >>= shift;
					g >>= shift;
					b >>= shift;	

					if (isstained)	// merge in stain
					{
						r = (127+r*(*stain++)) >> 8;
						g = (127+g*(*stain++)) >> 8;
						b = (127+b*(*stain++)) >> 8;
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
//					else if (r < 0)					
//						r = 0;				
					
					if (g > 255)
					{					
						cr += (255-g)/4;
						cg -= (255-g)/2;
						cb += (255-g)/4;
						g = 255;
					}
//					else if (g < 0)				
//						g = 0;					

					if (b > 255)
					{
						cr += (255-b)/4;
						cg += (255-b)/4;
						cb -= (255-b)/2;
						b = 255;
					}
//					else if (b < 0)
//						b = 0;
				//*
					if ((r+cr) > 255)
						dest[0] = 255;	//inverse lighting
					else if ((r+cr) < 0)
						dest[0] = 0;
					else
						dest[0] = (r+cr);

					if ((g+cg) > 255)
						dest[1] = 255;
					else if ((g+cg) < 0)
						dest[1] = 0;
					else
						dest[1] = (g+cg);

					if ((b+cb) > 255)
						dest[2] = 255;
					else if ((b+cb) < 0)
						dest[2] = 0;
					else
						dest[2] = (b+cb);
/*/
					if ((r+cr) > 255)
						dest[0] = 255;	//non-inverse lighting
					else if ((r+cr) < 0)
						dest[0] = 0;
					else
						dest[0] = (r+cr);

					if ((g+cg) > 255)
						dest[1] = 255;
					else if ((g+cg) < 0)
						dest[1] = 0;
					else
						dest[1] = (g+cg);

					if ((b+cb) > 255)
						dest[2] = 255;
					else if ((b+cb) < 0)
						dest[2] = 0;
					else
						dest[2] = (b+cb);
// */
					dest += 3;	
				}
			}
		}		
		break;
#else
	case GL_RGBA:
		stride -= (smax<<2);
		bl = blocklights;
		for (i=0 ; i<tmax ; i++, dest += stride)
		{
			for (j=0 ; j<smax ; j++)
			{
				t = *bl++;
				t >>= shift;
				if (t > 255)
					t = 255;
				dest[3] = t;
				dest += 4;
			}
		}
		break;
#endif
	case GL_ALPHA:
	case GL_LUMINANCE:
	case GL_INTENSITY:
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
#endif
}


/*
=============================================================

	BRUSH MODELS

=============================================================
*/


extern	int		solidskytexture;
extern	int		alphaskytexture;
extern	float	speedscale;		// for top sky and bottom sky

#if 0
static void DrawGLWaterPoly (glpoly_t *p);
static void DrawGLWaterPolyLightmap (glpoly_t *p);
#endif

qboolean mtexenabled = false;

void GL_SelectTexture (GLenum target);

void GL_DisableMultitexture(void) 
{
	if (mtexenabled) {
		qglDisable(GL_TEXTURE_2D);
		GL_SelectTexture(mtexid0);
		mtexenabled = false;
	}
}

void GL_EnableMultitexture(void) 
{
	if (gl_mtexable) {
		GL_SelectTexture(mtexid1);
		qglEnable(GL_TEXTURE_2D);
		mtexenabled = true;
	}
}

/*
================
DrawGLPoly
================
*/
static void DrawGLPoly (mesh_t *mesh)
{
//	GL_DrawAliasMesh 
#ifdef Q3SHADERS
	R_UnlockArrays();
#endif

	qglVertexPointer(3, GL_FLOAT, 0, mesh->xyz_array);
	qglEnableClientState( GL_VERTEX_ARRAY );
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	qglTexCoordPointer(2, GL_FLOAT, 0, mesh->st_array);
	qglDrawElements(GL_TRIANGLES, mesh->numindexes, GL_INDEX_TYPE, mesh->indexes);
	R_IBrokeTheArrays();

	/*
	int		i;
	float	*v;

	while(p)
	{
		qglBegin (GL_POLYGON);
		v = p->verts[0];
		for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
		{
			qglTexCoord2f (v[3], v[4]);
			qglVertex3fv (v);
		}
		qglEnd ();
		p=p->next;
	}
	*/
}


/*
================
R_BlendLightmaps
================
*/
#if 0
static void R_BlendLightmaps (void)
{
	int			i, j;
	glpoly_t	*p;
	float		*v;
	glRect_t	*theRect;

#if 0
	if (r_fullbright.value)
		return;
#endif

	glDepthMask (0);		// don't bother writing Z

	if (gl_lightmap_format == GL_LUMINANCE || gl_lightmap_format == GL_RGB)
		glBlendFunc (GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
	else if (gl_lightmap_format == GL_INTENSITY)
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glColor4f (0,0,0,1);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else if (gl_lightmap_format == GL_RGBA)
	{
		glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
	}

	if (!r_lightmap.value)
	{
		glEnable (GL_BLEND);
	}
	else
		glDisable (GL_BLEND);

	for (i=0 ; i<numlightmaps ; i++)
	{
		if (!lightmap[i])
			break;
		p = lightmap[i]->polys;
		if (!p)
			continue;
		lightmap[i]->polys = NULL;
		GL_Bind(lightmap_textures[i]);
		if (lightmap[i]->modified)
		{
			lightmap[i]->modified = false;
			theRect = &lightmap[i]->rectchange;
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
				LMBLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
				lightmap[i]->lightmaps+(theRect->t) *LMBLOCK_WIDTH*lightmap_bytes);
			theRect->l = LMBLOCK_WIDTH;
			theRect->t = LMBLOCK_HEIGHT;
			theRect->h = 0;
			theRect->w = 0;
		}
		for ( ; p ; p=p->chain)
		{
//			if (p->flags & SURF_UNDERWATER)
//				DrawGLWaterPolyLightmap (p);
			if (((r_viewleaf->contents==Q1CONTENTS_EMPTY && (p->flags & SURF_UNDERWATER)) ||
				(r_viewleaf->contents!=Q1CONTENTS_EMPTY && !(p->flags & SURF_UNDERWATER)))
				&& !(p->flags & SURF_DONTWARP))
				DrawGLWaterPolyLightmap (p);
			else
			{
				glBegin (GL_POLYGON);
				v = p->verts[0];
				for (j=0 ; j<p->numverts ; j++, v+= VERTEXSIZE)
				{
					glTexCoord2f (v[5], v[6]);
					glVertex3fv (v);
				}
				glEnd ();
			}
		}
	}

	glDisable (GL_BLEND);
	if (gl_lightmap_format == GL_LUMINANCE)
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA || gl_lightmap_format == GL_RGB);
	else if (gl_lightmap_format == GL_INTENSITY)
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glColor4f (1,1,1,1);
	}
	else if (gl_lightmap_format == GL_RGBA)
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glDepthMask (1);		// back to normal Z buffering
}
#endif

/*
================
R_RenderBrushPoly
================
*/
void R_RenderBrushPoly (msurface_t *fa)
{
	texture_t	*t;

	c_brush_polys++;

	if (fa->flags & SURF_DRAWSKY)
	{	// warp texture, no lightmaps
		GL_EmitBothSkyLayers (fa);
		return;
	}
		
	t = R_TextureAnimation (fa->texinfo->texture);
	GL_Bind (t->gl_texturenum);

	if (fa->flags & SURF_DRAWTURB)
	{	// warp texture, no lightmaps
		EmitWaterPolys (fa, r_wateralphaval);
		qglDisable(GL_BLEND);	//to ensure.
		return;
	}

	DrawGLPoly (fa->mesh);
}

/*
================
R_RenderDynamicLightmaps
Multitexture
================
*/
void R_RenderDynamicLightmaps (msurface_t *fa, int shift)
{
	qbyte		*base, *luxbase;
	stmap *stainbase;
	int			maps;
	glRect_t    *theRect;
	int smax, tmax;

	if (!fa->mesh)
		return;

	c_brush_polys++;

	if (fa->lightmaptexturenum<0)
		return;

	if (fa->flags & ( SURF_DRAWSKY | SURF_DRAWTURB) )
		return;

	if (fa->texinfo->flags & (SURF_SKY|SURF_TRANS33|SURF_TRANS66|SURF_WARP))
		return;

	if (fa->texinfo->flags & (TEX_SPECIAL))
	{
		if (cl.worldmodel->fromgame == fg_halflife)
			return;	//some textures do this.
	}
	
//	fa->polys->chain = lightmap[fa->lightmaptexturenum]->polys;
//	lightmap[fa->lightmaptexturenum]->polys = fa->polys;

	// check for lightmap modification
//	if (cl.worldmodel->fromgame != fg_quake3)	//no lightstyles on q3 maps
	{
		for (maps = 0 ; maps < MAXLIGHTMAPS && fa->styles[maps] != 255 ;
			 maps++)
			if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps]
	#ifdef PEXT_LIGHTSTYLECOL
				|| cl_lightstyle[fa->styles[maps]].colour != fa->cached_colour[maps]
	#endif
				)
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


		base = lightmap[fa->lightmaptexturenum]->lightmaps;
		base += fa->light_t * LMBLOCK_WIDTH * lightmap_bytes + fa->light_s * lightmap_bytes;
		luxbase = lightmap[fa->lightmaptexturenum]->deluxmaps;
		luxbase += fa->light_t * LMBLOCK_WIDTH * 3 + fa->light_s * 3;
		stainbase = lightmap[fa->lightmaptexturenum]->stainmaps;
		stainbase += (fa->light_t * LMBLOCK_WIDTH + fa->light_s) * 3;
		GLR_BuildLightMap (fa, base, luxbase, stainbase, shift);

		RSpeedEnd(RSPEED_DYNAMIC);
	}
}

/*
================
R_MirrorChain
================
*/
void R_MirrorChain (msurface_t *s)
{
	if (mirror)
		return;
	r_mirror_chain = s;
	mirror = true;
	mirror_plane = s->plane;
}


/*
================
R_DrawWaterSurfaces
================
*/
void GLR_DrawWaterSurfaces (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;

	if (r_wateralphaval == 1.0)
		return;

	//
	// go back to the world matrix
	//

    qglLoadMatrixf (r_view_matrix);

	if (r_wateralphaval < 1.0) {
		qglEnable (GL_BLEND);
		qglDisable (GL_ALPHA_TEST);
		qglColor4f (1,1,1,r_wateralphaval);
		GL_TexEnv(GL_MODULATE);
	}
	else
	{
		qglDisable (GL_BLEND);
		qglDisable (GL_ALPHA_TEST);
		GL_TexEnv(GL_REPLACE);
	}

	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		t = cl.worldmodel->textures[i];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s)
			continue;
		if ( !(s->flags & SURF_DRAWTURB ) )
			continue;
		
		GL_Bind (t->gl_texturenum);

		for ( ; s ; s=s->texturechain)
			EmitWaterPolys (s, r_wateralphaval);
		
		t->texturechain = NULL;
	}

	if (r_wateralphaval < 1.0) {
		GL_TexEnv(GL_REPLACE);

		qglColor4f (1,1,1,1);
		qglDisable (GL_BLEND);
	}
}


static void GLR_DrawAlphaSurface(msurface_t	*s)
{
	qglPushMatrix();
	R_RotateForEntity(s->ownerent);
#ifdef Q3SHADERS
	if (s->texinfo->texture->shader)
	{
		meshbuffer_t mb;
		mb.dlightbits = 0;
		mb.entity = s->ownerent;
		mb.shader = s->texinfo->texture->shader;
		mb.sortkey = 0;

		mb.infokey = s->lightmaptexturenum;
		mb.mesh = s->mesh;
		mb.fog = s->fog;
		currententity = s->ownerent;
		if (s->mesh)
		{
			R_PushMesh(s->mesh, mb.shader->features|MF_NONBATCHED);
			R_RenderMeshBuffer ( &mb, false );
		}

		qglPopMatrix();
		return;
	}
#endif
	GL_Bind(s->texinfo->texture->gl_texturenum);

	if (s->texinfo->flags & SURF_TRANS33)
		qglColor4f (1,1,1,0.33);
	else if (s->texinfo->flags & SURF_TRANS66)
		qglColor4f (1,1,1,0.66);
	else
	{
		if (s->flags & SURF_DRAWTURB)
		{
			qglColor4f (1,1,1,1);
			EmitWaterPolys (s, r_wateralphaval);
		}
		else
		{
			Sys_Error("GLR_DrawAlphaSurface needs work");
			/*
			if (gl_mtexable)
			{
				int i;
				float *v;
				glpoly_t *p;
				GL_TexEnv(GL_REPLACE);
				GL_EnableMultitexture();
				GL_Bind(lightmap_textures[s->lightmaptexturenum]);
				GL_TexEnv(GL_BLEND);
				p = s->polys;

				qglColor4f (1,1,1,1);
				while(p)
				{
					qglBegin (GL_POLYGON);
					v = p->verts[0];
					for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
					{
						qglMTexCoord2fSGIS (mtexid0, v[3], v[4]);
						qglMTexCoord2fSGIS (mtexid1, v[5], v[6]);
						qglVertex3fv (v);
					}
					qglEnd ();
					p=p->next;
				}
				GL_DisableMultitexture();
			}
			else
			*/
			{
				if (s->samples)	//could do true vertex lighting... ?
					qglColor4ub (*s->samples,*s->samples,*s->samples,255);
				else
					qglColor4f (1,1,1,1);
				DrawGLPoly (s->mesh);
			}
		}

		qglPopMatrix();
		return;
	}

	if (s->flags & SURF_DRAWTURB || s->texinfo->flags & SURF_WARP)
		EmitWaterPolys (s, r_wateralphaval);
//	else if(s->texinfo->flags & SURF_FLOWING)			// PGM	9/16/98
//		DrawGLFlowingPoly (s);							// PGM
	else
		DrawGLPoly (s->mesh);

	qglPopMatrix();
}

void GLR_DrawAlphaSurfaces (void)
{
	msurface_t	*s;
	vec3_t v;

	//
	// go back to the world matrix
	//

    qglLoadMatrixf (r_view_matrix);
	GL_TexEnv(GL_MODULATE);
	
	qglEnable(GL_ALPHA_TEST);
	qglDisable(GL_BLEND);
	if (cl.worldmodel && (cl.worldmodel->fromgame == fg_quake2))
	{	//this is a mahoosive hack.
		qglDepthMask(0);	//this makes no difference to the cheating.

		qglDisable(GL_ALPHA_TEST);
		qglEnable(GL_BLEND);
	}
	qglColor4f (1,1,1,1);
	for (s=r_alpha_surfaces ; s ; s=s->nextalphasurface)
	{
		if (s->flags&0x80000)
		{
			Con_Printf("Infinate alpha surface loop detected\n");
			break;
		}
		s->flags |= 0x80000;
		if (*s->texinfo->texture->name == '{')
		{	//simple alpha testing.

			if (s->ownerent != currententity)
			{
				currententity = s->ownerent;
				qglPopMatrix();
				qglPushMatrix();
				R_RotateForEntity(currententity);
			}
			Sys_Error("GLR_DrawAlphaSurfaces needs work");
/*
			if (gl_mtexable)
			{
				int i;
				float *v;
				glpoly_t *p;
				GL_Bind(s->texinfo->texture->gl_texturenum);
				GL_TexEnv(GL_REPLACE);
				GL_EnableMultitexture();
				GL_Bind(lightmap_textures[s->lightmaptexturenum]);
				GL_TexEnv(GL_BLEND);
				p = s->polys;


				while(p)
				{
					qglBegin (GL_POLYGON);
					v = p->verts[0];
					for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
					{
						qglMTexCoord2fSGIS (mtexid0, v[3], v[4]);
						qglMTexCoord2fSGIS (mtexid1, v[5], v[6]);
						qglVertex3fv (v);
					}
					qglEnd ();
					p=p->next;
				}
				GL_DisableMultitexture();
			}
			else
*/
			{
				if (s->samples)	//could do true vertex lighting... ?
					qglColor4ub (*s->samples,*s->samples,*s->samples,255);
				else
					qglColor4f (1,1,1,1);
				DrawGLPoly (s->mesh);
				qglColor4f (1,1,1,1);
			}
			continue;
		}
		v[0] = s->plane->normal[0] * s->plane->dist+s->ownerent->origin[0];
		v[1] = s->plane->normal[1] * s->plane->dist+s->ownerent->origin[1];
		v[2] = s->plane->normal[2] * s->plane->dist+s->ownerent->origin[2];
		RQ_AddDistReorder((void*)GLR_DrawAlphaSurface, s, NULL, v);
	}
	for (s=r_alpha_surfaces ; s ; s=s->nextalphasurface)
	{
		if (!(s->flags&0x80000))
			break;
		s->flags &= ~0x80000;
	}
	RQ_RenderDistAndClear();
	qglDepthMask(1);

	GL_TexEnv(GL_REPLACE);

	qglColor4f (1,1,1,1);
	qglDisable (GL_BLEND);

	r_alpha_surfaces = NULL;

	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

#if 0
static void
vecMatMult(GLfloat vecIn[3], GLfloat m[16], GLfloat vecOut[3]) {
  vecOut[0] = (vecIn[0]*m[ 0]) + (vecIn[1]*m[ 4]) + (vecIn[2]*m[ 8]) + m[12];
  vecOut[1] = (vecIn[0]*m[ 1]) + (vecIn[1]*m[ 5]) + (vecIn[2]*m[ 9]) + m[13];
  vecOut[2] = (vecIn[0]*m[ 2]) + (vecIn[1]*m[ 6]) + (vecIn[2]*m[10]) + m[14];
}

static void
matrixInvert(GLfloat in[16], GLfloat out[16])
{
  // Transpose rotation
  out[ 0] = in[ 0];  out[ 1] = in[ 4];  out[ 2] = in[ 8];
  out[ 4] = in[ 1];  out[ 5] = in[ 5];  out[ 6] = in[ 9];
  out[ 8] = in[ 2];  out[ 9] = in[ 6];  out[10] = in[10];
  
  // Clear shearing terms
  out[3] = 0.0f; out[7] = 0.0f; out[11] = 0.0f; out[15] = 1.0f;

  // Translation is minus the dot of tranlation and rotations
  out[12] = -(in[12]*in[ 0]) - (in[13]*in[ 1]) - (in[14]*in[ 2]);
  out[13] = -(in[12]*in[ 4]) - (in[13]*in[ 5]) - (in[14]*in[ 6]);
  out[14] = -(in[12]*in[ 8]) - (in[13]*in[ 9]) - (in[14]*in[10]);
}
#endif

void VectorVectors(vec3_t forward, vec3_t right, vec3_t up);
/*
================
DrawTextureChains
================
*/
/*
#if 0
static void DrawTextureChains (model_t *model, float alpha, vec3_t relativelightorigin)
{
	int		i;
	msurface_t	*s, *last = NULL, *first=NULL, *cf;
	texture_t	*t;

	int vi;
	glRect_t    *theRect;
	glpoly_t *p;
	float *v;

	extern int gl_bumpmappingpossible;
	extern int normalisationCubeMap;
	qboolean bumpmapping=gl_bump.value && gl_bumpmappingpossible && (alpha == 1) && (normalisationCubeMap || currentmodel->deluxdata);

	if (model == cl.worldmodel && skytexturenum>=0)
	{
		t = model->textures[skytexturenum];
		if (t)
		{
			s = t->texturechain;
			if (s)
			{
				t->texturechain = NULL;
				GL_DrawSkyChain (s);
			}
		}
	}
	if (alpha == 1)
	{
		glDisable(GL_BLEND);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	}
	else
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}
	if (currententity->drawflags & MLS_ABSLIGHT)
		glColor4f(currententity->abslight/255.0f, currententity->abslight/255.0f, currententity->abslight/255.0f, alpha);
	else
		glColor4f(1, 1, 1, alpha);

	for (i=0 ; i<model->numtextures ; i++)
	{
		t = model->textures[i];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s)
			continue;
		t->texturechain = NULL;
		if (i == skytexturenum && model == cl.worldmodel)
			GL_DrawSkyChain (s);
		else if (i == mirrortexturenum && model == cl.worldmodel && r_mirroralpha.value != 1.0)
			R_MirrorChain (s);
		else
		{
			if ((s->flags & SURF_DRAWTURB) && r_wateralphaval != 1.0)
			{
				t->texturechain = s;
				continue;	// draw translucent water later
			}

			if (last)
				last->texturechain = s;
			else
				first = s;

			t = R_TextureAnimation (t);

			cf = s;

			if (gl_mtexable && alpha == 1)
			{
				if (s->lightmaptexturenum<0 || currententity->drawflags & MLS_ABSLIGHT)
				{	//vertex lighting required.
					GL_DisableMultitexture();
					glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
					for (s=cf ; s ; s=s->texturechain)
					{
						R_RenderBrushPoly (s);
					}
					continue;
				}


				if (cf->flags & SURF_DRAWTURB)
				{
					GL_DisableMultitexture();
					glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					GL_Bind (s->texinfo->texture->gl_texturenum);
					for (s=cf; s ; s=s->texturechain)
						EmitWaterPolys (s);

					if (alpha == 1)
					{
						glDisable(GL_BLEND);
						glColor4f(1, 1, 1, 1);
					}
					else
					{
						glEnable(GL_BLEND);
						glColor4f(1, 1, 1, alpha);
					}

					if (last)	//don't include this chain for details.
						last->texturechain = NULL;
					continue;
				}

				if (bumpmapping && t->gl_texturenumbumpmap)
				{
					vec3_t light;

					GL_DisableMultitexture();
//					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

//					glEnable(GL_ALPHA_TEST);
					glColor4f(1, 1, 1, 1);
					glDisable(GL_BLEND);

					//Bind normal map to texture unit 0
					GL_BindType(GL_TEXTURE_2D, t->gl_texturenumbumpmap);
					glEnable(GL_TEXTURE_2D);

					//Set up texture environment to do (tex0 dot tex1)*color
					GL_TexEnv(GL_COMBINE_ARB);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
					glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE);

					qglActiveTextureARB(GL_TEXTURE1_ARB);
				
					GL_TexEnv(GL_COMBINE_ARB);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
					glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_DOT3_RGB_ARB);
				
					if (gl_bump.value < 0)
					{
						if (currentmodel->deluxdata)
						{
							glEnable(GL_TEXTURE_2D);
							for (s = cf; s ; s=s->texturechain)
							{
								vi = s->lightmaptexturenum;
								GL_BindType(GL_TEXTURE_2D, deluxmap_textures[vi] );
								if (lightmap[vi]->deluxmodified)
								{
									lightmap[vi]->deluxmodified = false;
									theRect = &lightmap[vi]->deluxrectchange;
									glTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
										LMBLOCK_WIDTH, theRect->h, GL_RGB, GL_UNSIGNED_BYTE,
										lightmap[vi]->deluxmaps+(theRect->t) *LMBLOCK_WIDTH*3);
									theRect->l = LMBLOCK_WIDTH;
									theRect->t = LMBLOCK_HEIGHT;
									theRect->h = 0;
									theRect->w = 0;
								}

								for (p = s->polys; p; p=p->next)
								{
									glBegin(GL_POLYGON);
									v = p->verts[0];
									for (vi=0 ; vi<p->numverts ; vi++, v+= VERTEXSIZE)
									{									
										qglMultiTexCoord2fARB(GL_TEXTURE0_ARB, v[3], v[4]);
										qglMultiTexCoord2fARB(GL_TEXTURE1_ARB, v[5], v[6]);
										glVertex3fv (v);
									}
									glEnd ();
								}
							}
							glDisable(GL_TEXTURE_2D);
						}
						else
						{
							GL_BindType(GL_TEXTURE_CUBE_MAP_ARB, normalisationCubeMap);
							glEnable(GL_TEXTURE_CUBE_MAP_ARB);
							qglMultiTexCoord3fARB(GL_TEXTURE1_ARB, sin(-r_refdef.viewangles[1]/180*M_PI), cos(-r_refdef.viewangles[1]/180*M_PI), 1);
							for (s = cf; s ; s=s->texturechain)
							{
								vi = s->lightmaptexturenum;
								for (p = s->polys; p; p=p->next)
								{
									glBegin(GL_POLYGON);
									v = p->verts[0];
									for (vi=0 ; vi<p->numverts ; vi++, v+= VERTEXSIZE)
									{									
										qglMultiTexCoord2fARB(GL_TEXTURE0_ARB, v[3], v[4]);
										glVertex3fv (v);
									}
									glEnd ();
								}
							}
							glDisable(GL_TEXTURE_CUBE_MAP_ARB);
						}
					}
					else
					{
						GL_BindType(GL_TEXTURE_CUBE_MAP_ARB, normalisationCubeMap);
						glEnable(GL_TEXTURE_CUBE_MAP_ARB);
						for (s = cf; s ; s=s->texturechain)
						{
							for (p = s->polys; p; p=p->next)
							{
								glBegin(GL_POLYGON);
								v = p->verts[0];
								for (vi=0 ; vi<p->numverts ; vi++, v+= VERTEXSIZE)
								{
									light[0] = relativelightorigin[0] - v[0];
									light[1] = relativelightorigin[1] - v[1];
									light[2] = relativelightorigin[2] - v[2];
									
									qglMultiTexCoord2fARB(GL_TEXTURE0_ARB, v[3], v[4]);
									qglMultiTexCoord3fARB(GL_TEXTURE1_ARB, -DotProduct(vup, light), -DotProduct(vright, light), gl_bump.value/2*-DotProduct(vpn, light));
									glVertex3fv (v);
								}
								glEnd ();
							}
						}
						glDisable(GL_TEXTURE_CUBE_MAP_ARB);
					}

					qglActiveTextureARB(GL_TEXTURE0_ARB);
					currenttexture=0;
					glEnable (GL_BLEND);
					glBlendFunc(GL_DST_COLOR, GL_ZERO);
					glColor4f(1, 1, 1, 1);


					// Binds world to texture env 0
					GL_SelectTexture(mtexid0);
					GL_Bind (t->gl_texturenum);
					glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
					GL_EnableMultitexture(); // Same as SelectTexture (TEXTURE1)
					glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
				}
				else
				{

				// Binds world to texture env 0
					GL_SelectTexture(mtexid0);
					GL_Bind (t->gl_texturenum);
					glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
					GL_EnableMultitexture(); // Same as SelectTexture (TEXTURE1)
					glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
				}

				for (s=cf; s; s=s->texturechain)
				{
//					R_RenderDynamicLightmaps (s);
					vi = s->lightmaptexturenum;
					// Binds lightmap to texenv 1
					GL_Bind (lightmap_textures[vi]);
					if (lightmap[vi]->modified)
					{
						lightmap[vi]->modified = false;
						theRect = &lightmap[vi]->rectchange;
						glTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
							LMBLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
							lightmap[vi]->lightmaps+(theRect->t) *LMBLOCK_WIDTH*lightmap_bytes);
						theRect->l = LMBLOCK_WIDTH;
						theRect->t = LMBLOCK_HEIGHT;
						theRect->h = 0;
						theRect->w = 0;
					}
					for (p = s->polys; p; p=p->next)
					{
						glBegin(GL_POLYGON);
						v = p->verts[0];
						for (vi=0 ; vi<p->numverts ; vi++, v+= VERTEXSIZE)
						{
							qglMTexCoord2fSGIS (mtexid0, v[3], v[4]);
							qglMTexCoord2fSGIS (mtexid1, v[5], v[6]);
							glVertex3fv (v);
						}
						glEnd ();
					}
					last = s;
				}
			}
			else
			{
				for (s=cf ; s ; s=s->texturechain)
				{
					R_RenderBrushPoly (s);
					last = s;
				}
			}

			if (alpha == 1)
			{
				glDisable(GL_BLEND);
				glColor4f(1, 1, 1, 1);
			}
			else
			{
				glEnable(GL_BLEND);
				glColor4f(1, 1, 1, alpha);
			}

			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
	}

	if (gl_mtexable)
		GL_DisableMultitexture();
	else
		R_BlendLightmaps();

	//add luminance?
	if (first && detailtexture && gl_detail.value && alpha == 1)
	{
		GL_Bind(detailtexture);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
		glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
		glEnable(GL_BLEND);
		glDepthMask(0);

		for (s=first ; s ; s=s->texturechain)
		{
			for (p = s->polys; p; p=p->next)
			{
				glBegin(GL_POLYGON);
				v = p->verts[0];
				for (i = 0; i < p->numverts; i++, v += VERTEXSIZE)
				{
					glTexCoord2f (v[5] * 18, v[6] * 18);
					glVertex3fv (v);
				}
				glEnd();
			}
		}

		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glDisable(GL_BLEND);

		glDepthMask(1);
	}

	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
#endif
*/
/*
=================
R_DrawBrushModel
=================
*/
/*
#if 0
static void R_DrawBrushModel (entity_t *e)
{
	int			i;
	int			k;
	vec3_t		mins, maxs;
	msurface_t	*psurf, *first;
	float		dot;
	mplane_t	*pplane;
	qboolean	rotated;

	currententity = e;
	currenttexture = -1;

	currentmodel = e->model;

	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		rotated = true;
		for (i=0 ; i<3 ; i++)
		{
			mins[i] = e->origin[i] - currentmodel->radius;
			maxs[i] = e->origin[i] + currentmodel->radius;
		}
	}
	else
	{
		rotated = false;
		VectorAdd (e->origin, currentmodel->mins, mins);
		VectorAdd (e->origin, currentmodel->maxs, maxs);
	}

	if (R_CullBox (mins, maxs))
		return;

	VectorSubtract (r_refdef.vieworg, e->origin, modelorg);
	if (rotated)
	{
		vec3_t	temp;
		vec3_t	forward, right, up;

		VectorCopy (modelorg, temp);
		AngleVectors (e->angles, forward, right, up);
		modelorg[0] = DotProduct (temp, forward);
		modelorg[1] = -DotProduct (temp, right);
		modelorg[2] = DotProduct (temp, up);
	}

	psurf = &currentmodel->surfaces[currentmodel->firstmodelsurface];

// calculate dynamic lighting for bmodel if it's not an
// instanced model
	if (currentmodel->firstmodelsurface != 0 && !r_flashblend.value)
	{
		for (k=0 ; k<dlights_software ; k++)
		{
			if ((cl_dlights[k].die < cl.time) ||
				(!cl_dlights[k].radius))
				continue;

			currentmodel->funcs.MarkLights (&cl_dlights[k], 1<<k,
				currentmodel->nodes + currentmodel->hulls[0].firstclipnode);
		}
	}

    glPushMatrix ();
e->angles[0] = -e->angles[0];	// stupid quake bug
	glTranslatef(-0.03, -0.03, 0.03);
	R_RotateForEntity (e);
e->angles[0] = -e->angles[0];	// stupid quake bug


	first = NULL;
	//
	// draw texture
	//
	for (i=0 ; i<currentmodel->nummodelsurfaces ; i++, psurf++)
	{
	// find which side of the node we are on
		pplane = psurf->plane;

//		if (psurf->plane)
		{
			dot = DotProduct (modelorg, pplane->normal) - pplane->dist;
		

	// draw the polygon
			if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
				(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
			{
				R_RenderDynamicLightmaps (psurf);
				if (psurf->flags & SURF_DRAWALPHA || psurf->texinfo->flags & (SURF_TRANS33|SURF_TRANS66) )
				{	// add to the translucent chain
					psurf->nextalphasurface = r_alpha_surfaces;
					r_alpha_surfaces = psurf;
					psurf->ownerent = e;
				}
				else
				{
					psurf->texturechain = psurf->texinfo->texture->texturechain;
					psurf->texinfo->texture->texturechain = psurf;
				}
			}
		}
	}

	VectorSubtract(r_refdef.vieworg, e->origin, mins);	//fixme: rotation.
	if (e->drawflags & DRF_TRANSLUCENT)
		DrawTextureChains(currentmodel, e->alpha*0.4, mins);
	else
		DrawTextureChains(currentmodel, e->alpha, mins);

	glPopMatrix ();
}
#endif
*/
/*
=============================================================

	WORLD MODEL

=============================================================
*/

/*
================
R_RecursiveWorldNode
================
*/
static void GLR_RecursiveWorldNode (mnode_t *node)
{
	int			c, side;
	mplane_t	*plane;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	double		dot;
	int shift;

start:

	if (node->contents == Q1CONTENTS_SOLID)
		return;		// solid

	if (node->visframe != r_visframecount)
		return;
	if (R_CullBox (node->minmaxs, node->minmaxs+3))
		return;

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

	// deal with model fragments in this leaf
		if (pleaf->efrags)
			R_StoreEfrags (&pleaf->efrags);
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
	GLR_RecursiveWorldNode (node->children[side]);

// draw stuff
  	c = node->numsurfaces;

	if (c)
	{
		surf = cl.worldmodel->surfaces + node->firstsurface;

		shift = GLR_LightmapShift(cl.worldmodel);

//		if (dot < 0 -BACKFACE_EPSILON)
//			side = SURF_PLANEBACK;
//		else if (dot > BACKFACE_EPSILON)
//			side = 0;
		{
			for ( ; c ; c--, surf++)
			{
				if (surf->visframe != r_framecount)
					continue;

//				if (((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK)))
//					continue;		// wrong side

				R_RenderDynamicLightmaps (surf, shift);
				// if sorting by texture, just store it out
/*				if (surf->flags & SURF_DRAWALPHA)
				{	// add to the translucent chain
					surf->nextalphasurface = r_alpha_surfaces;
					r_alpha_surfaces = surf;
					surf->ownerent = &r_worldentity;
				}
				else
*/				{
					surf->texturechain = surf->texinfo->texture->texturechain;
					surf->texinfo->texture->texturechain = surf;
				}
			}
		}
	}

// recurse down the back side
	//GLR_RecursiveWorldNode (node->children[!side]);
	node = node->children[!side];
	goto start;
}

#ifdef Q2BSPS
static void GLR_RecursiveQ2WorldNode (mnode_t *node)
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
	GLR_RecursiveQ2WorldNode (node->children[side]);

	shift = GLR_LightmapShift(currentmodel);

	// draw stuff
	for ( c = node->numsurfaces, surf = currentmodel->surfaces + node->firstsurface; c ; c--, surf++)
	{
		if (surf->visframe != r_framecount)
			continue;

		if ( (surf->flags & SURF_PLANEBACK) != sidebit )
			continue;		// wrong side

		surf->visframe = r_framecount+1;//-1;

		R_RenderDynamicLightmaps (surf, shift);

		if (surf->texinfo->flags & (SURF_TRANS33|SURF_TRANS66))
		{	// add to the translucent chain
			surf->nextalphasurface = r_alpha_surfaces;
			r_alpha_surfaces = surf;
			surf->ownerent = &r_worldentity;
			continue;
		}

		surf->texturechain = surf->texinfo->texture->texturechain;
		surf->texinfo->texture->texturechain = surf;
	}


// recurse down the back side
	GLR_RecursiveQ2WorldNode (node->children[!side]);
}
#endif

#ifdef Q3BSPS
mleaf_t		*r_vischain;		// linked list of visible leafs
static void GLR_LeafWorldNode (void)
{
	int			i;
	int			clipflags;
	msurface_t	**mark, *surf;
	mleaf_t		*pleaf;


	int clipped;
	mplane_t *clipplane;


	for ( pleaf = r_vischain; pleaf; pleaf = pleaf->vischain )
	{
		// check for door connected areas
//		if ( areabits )
		{
//			if (! (areabits[pleaf->area>>3] & (1<<(pleaf->area&7)) ) )
//			{
//				continue;		// not visible
//			}
		}

		clipflags = 15;		// 1 | 2 | 4 | 8
//		if ( !r_nocull->value )
		{

			for (i=0,clipplane=frustum ; i<4 ; i++,clipplane++)
			{
				clipped = BoxOnPlaneSide ( pleaf->minmaxs, pleaf->minmaxs+3, clipplane );
				if ( clipped == 2 ) {
					break;
				} else if ( clipped == 1 ) {
					clipflags &= ~(1<<i);	// node is entirely on screen
				}
			}

			if ( i != 4 ) {
				continue;
			}
		}

		i = pleaf->nummarksurfaces;
		mark = pleaf->firstmarksurface;

		do
		{
			surf = *mark++;
			if ( surf->visframe != r_framecount )	//sufraces exist in multiple leafs.
			{
				surf->visframe = r_framecount;

				surf->texturechain = surf->texinfo->texture->texturechain;
				surf->texinfo->texture->texturechain = surf;
			}
		} while (--i);

//		c_world_leafs++;
	}
}
#endif

/*
=============
R_DrawWorld
=============
*/

void R_DrawWorld (void)
{
	RSpeedLocals();
	entity_t	ent;

	memset (&ent, 0, sizeof(ent));
	ent.model = cl.worldmodel;
	currentmodel = cl.worldmodel;

	VectorCopy (r_refdef.vieworg, modelorg);

	currententity = &ent;
#ifdef TERRAIN
	if (currentmodel->type == mod_heightmap)
		GL_DrawHeightmapModel(currententity);
	else
#endif
	{
		qglColor3f (1,1,1);
	//#ifdef QUAKE2
		R_ClearSkyBox ();
	//#endif

		RSpeedRemark();

#ifdef Q2BSPS
		if (ent.model->fromgame == fg_quake2 || ent.model->fromgame == fg_quake3)
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
			if (ent.model->fromgame == fg_quake3)
			{
				GLR_LeafWorldNode ();
			}
			else
#endif
				GLR_RecursiveQ2WorldNode (cl.worldmodel->nodes);
		}
		else
#endif
			GLR_RecursiveWorldNode (cl.worldmodel->nodes);

		RSpeedEnd(RSPEED_WORLDNODE);
		TRACE(("dbg: calling PPL_DrawWorld\n"));
//		if (r_shadows.value >= 2 && gl_canstencil && gl_mtexable)
			PPL_DrawWorld();
//		else
//			DrawTextureChains (cl.worldmodel, 1, r_refdef.vieworg);


qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		GLR_LessenStains();
	}
}

qbyte *Q1BSP_LeafPVS (model_t *model, mleaf_t *leaf, qbyte *buffer);



/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/

// returns a texture number and the position inside it
int GLAllocBlock (int w, int h, int *x, int *y)
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
			lightmap_textures[numlightmaps+0] = texture_extension_number++;
			lightmap_textures[numlightmaps+1] = texture_extension_number++;
			lightmap_textures[numlightmaps+2] = texture_extension_number++;
			lightmap_textures[numlightmaps+3] = texture_extension_number++;

			deluxmap_textures = BZ_Realloc(deluxmap_textures, sizeof(*deluxmap_textures)*(numlightmaps+4));
			deluxmap_textures[numlightmaps+0] = texture_extension_number++;
			deluxmap_textures[numlightmaps+1] = texture_extension_number++;
			deluxmap_textures[numlightmaps+2] = texture_extension_number++;
			deluxmap_textures[numlightmaps+3] = texture_extension_number++;
			numlightmaps+=4;
		}
		if (!lightmap[texnum])
		{
			lightmap[texnum] = Z_Malloc(sizeof(*lightmap[texnum]));
			// reset stainmap since it now starts at 255
			memset(lightmap[texnum]->stainmaps, 255, sizeof(lightmap[texnum]->stainmaps));
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
int GLFillBlock (int texnum, int w, int h, int x, int y)
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
		lightmap_textures[numlightmaps+0] = texture_extension_number++;
		lightmap_textures[numlightmaps+1] = texture_extension_number++;
		lightmap_textures[numlightmaps+2] = texture_extension_number++;
		lightmap_textures[numlightmaps+3] = texture_extension_number++;

		deluxmap_textures = BZ_Realloc(deluxmap_textures, sizeof(*deluxmap_textures)*(numlightmaps+4)); 
		deluxmap_textures[numlightmaps+0] = texture_extension_number++;
		deluxmap_textures[numlightmaps+1] = texture_extension_number++;
		deluxmap_textures[numlightmaps+2] = texture_extension_number++;
		deluxmap_textures[numlightmaps+3] = texture_extension_number++;
		numlightmaps+=4;
	}
	for (i = texnum; i >= 0; i--)
	{
		if (!lightmap[i])
		{
			lightmap[i] = BZ_Malloc(sizeof(*lightmap[i]));
			for (l=0 ; l<LMBLOCK_HEIGHT ; l++)
			{
				lightmap[i]->allocated[l] = LMBLOCK_HEIGHT;
			}

			//maybe someone screwed with my lightmap...
			memset(lightmap[i]->lightmaps, 255, LMBLOCK_HEIGHT*LMBLOCK_HEIGHT*3);
			if (cl.worldmodel->lightdata)
				memcpy(lightmap[i]->lightmaps, cl.worldmodel->lightdata+3*LMBLOCK_HEIGHT*LMBLOCK_HEIGHT*i, LMBLOCK_HEIGHT*LMBLOCK_HEIGHT*3);

		}
		else
			break;
	}
	return texnum;
}

mvertex_t	*r_pcurrentvertbase;

int	nColinElim;

/*
================
BuildSurfaceDisplayList
================
*/
void GL_BuildSurfaceDisplayList (msurface_t *fa)
{
	int			i, lindex, lnumverts;
	medge_t		*pedges, *r_pedge;
	int			vertpage;
	float		*vec;
	float		s, t;
	int	lm;

// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	if (lnumverts<3)
		return;	//q3 map.
//#ifdef Q3SHADERS
//	if (fa->texinfo->texture->shader)
	{	//build a nice mesh instead of a poly.
		int size = sizeof(mesh_t) + sizeof(index_t)*(lnumverts-2)*3 + (sizeof(vec3_t) + sizeof(vec3_t) + 2*sizeof(vec2_t) + sizeof(byte_vec4_t))*lnumverts;
		mesh_t *mesh;

		fa->mesh = mesh = Hunk_Alloc(size);
		mesh->xyz_array = (vec3_t*)(mesh + 1);
		mesh->normals_array = (vec3_t*)(mesh->xyz_array + lnumverts);
		mesh->st_array = (vec2_t*)(mesh->normals_array + lnumverts);
		mesh->lmst_array = (vec2_t*)(mesh->st_array + lnumverts);
		mesh->colors_array = (byte_vec4_t*)(mesh->lmst_array + lnumverts);
		mesh->indexes = (index_t*)(mesh->colors_array + lnumverts);

		mesh->numindexes = (lnumverts-2)*3;
		mesh->numvertexes = lnumverts;
		mesh->patchWidth = mesh->patchHeight = 1;

		for (i=0 ; i<lnumverts-2 ; i++)
		{
			mesh->indexes[i*3] = 0;
			mesh->indexes[i*3+1] = i+1;
			mesh->indexes[i*3+2] = i+2;
		}

		for (i=0 ; i<lnumverts ; i++)
		{
			lindex = currentmodel->surfedges[fa->firstedge + i];

			if (lindex > 0)
			{
				r_pedge = &pedges[lindex];
				vec = r_pcurrentvertbase[r_pedge->v[0]].position;
			}
			else
			{
				r_pedge = &pedges[-lindex];
				vec = r_pcurrentvertbase[r_pedge->v[1]].position;
			}

			s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
			t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];

			VectorCopy (vec, mesh->xyz_array[i]);
			mesh->xyz_array[i][3] = 1;
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

			mesh->colors_array[i][0] = 255;
			mesh->colors_array[i][1] = 255;
			mesh->colors_array[i][2] = 255;
			mesh->colors_array[i][3] = 255;
		}

		return;
	}
//#endif
	//
	// draw texture
	//
	/*
	poly = Hunk_AllocName (sizeof(glpoly_t) + (lnumverts-4) * VERTEXSIZE*sizeof(float), "SDList");
	poly->next = fa->polys;
	fa->polys = poly;
	poly->numverts = lnumverts;

	for (i=0 ; i<lnumverts ; i++)
	{
		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec = r_pcurrentvertbase[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = r_pcurrentvertbase[r_pedge->v[1]].position;
		}

		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];

		VectorCopy (vec, poly->verts[i]);
		poly->verts[i][3] = s/fa->texinfo->texture->width;
		poly->verts[i][4] = t/fa->texinfo->texture->height;

		//
		// lightmap texture coordinates
		//
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

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;

#ifdef SPECULAR
		if (fa->flags & SURF_PLANEBACK)
			VectorNegate(fa->plane->normal, (poly->verts[i]+7));
		else
			VectorCopy(fa->plane->normal, (poly->verts[i]+7));
#endif
	}

	//
	// remove co-linear points - Ed
	//
	if (!gl_keeptjunctions.value && !(fa->flags & SURF_UNDERWATER) )
	{
		for (i = 0 ; i < lnumverts ; ++i)
		{
			vec3_t v1, v2;
			float *prev, *this, *next;

			prev = poly->verts[(i + lnumverts - 1) % lnumverts];
			this = poly->verts[i];
			next = poly->verts[(i + 1) % lnumverts];

			VectorSubtract( this, prev, v1 );
			VectorNormalize( v1 );
			VectorSubtract( next, prev, v2 );
			VectorNormalize( v2 );

			// skip co-linear points
			#define COLINEAR_EPSILON 0.001
			if ((fabs( v1[0] - v2[0] ) <= COLINEAR_EPSILON) &&
				(fabs( v1[1] - v2[1] ) <= COLINEAR_EPSILON) && 
				(fabs( v1[2] - v2[2] ) <= COLINEAR_EPSILON))
			{
				int j;
				for (j = i + 1; j < lnumverts; ++j)
				{
					int k;
					for (k = 0; k < VERTEXSIZE; ++k)
						poly->verts[j - 1][k] = poly->verts[j][k];
				}
				--lnumverts;
				++nColinElim;
				// retry next vertex next time, which is now current vertex
				--i;
			}
		}
	}

	poly->numverts = lnumverts;
	*/
}

/*
========================
GL_CreateSurfaceLightmap
========================
*/
void GL_CreateSurfaceLightmap (msurface_t *surf, int shift)
{
	int		smax, tmax;
	qbyte	*base, *luxbase; stmap *stainbase;

	if (surf->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
		surf->lightmaptexturenum = -1;
	if (surf->texinfo->flags & TEX_SPECIAL)
		surf->lightmaptexturenum = -1;
	if (surf->lightmaptexturenum<0)
		return;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;

	if (smax > LMBLOCK_WIDTH || tmax > LMBLOCK_HEIGHT || smax < 0 || tmax < 0)
	{	//whoa, buggy.
		surf->lightmaptexturenum = -1;
		return;
	}

	if (currentmodel->fromgame == fg_quake3)
		GLFillBlock(surf->lightmaptexturenum, smax, tmax, surf->light_s, surf->light_t);
	else
		surf->lightmaptexturenum = GLAllocBlock (smax, tmax, &surf->light_s, &surf->light_t);
	base = lightmap[surf->lightmaptexturenum]->lightmaps;
	base += (surf->light_t * LMBLOCK_WIDTH + surf->light_s) * lightmap_bytes;

	luxbase = lightmap[surf->lightmaptexturenum]->deluxmaps;
	luxbase += (surf->light_t * LMBLOCK_WIDTH + surf->light_s) * 3;

	stainbase = lightmap[surf->lightmaptexturenum]->stainmaps;
	stainbase += (surf->light_t * LMBLOCK_WIDTH + surf->light_s) * 3;
	
	GLR_BuildLightMap (surf, base, luxbase, stainbase, shift);
}



void GLSurf_DeInit(void)
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
		BZ_Free(lightmap_textures);
	if (lightmap)
		BZ_Free(lightmap);

	lightmap_textures=NULL;
	lightmap=NULL;
	numlightmaps=0;
}


/*
==================
GL_BuildLightmaps

Builds the lightmap texture
with all the surfaces from all brush models
==================
*/
void GL_BuildLightmaps (void)
{
	int		i, j;
	model_t	*m;
	int shift;

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

	if ((cl.worldmodel->engineflags & MDLF_RGBLIGHTING) || cl.worldmodel->deluxdata || r_loadlits.value)
		gl_lightmap_format = GL_RGB;
	else
		gl_lightmap_format = GL_LUMINANCE;

/*
	if (COM_CheckParm ("-lm_1"))
		gl_lightmap_format = GL_LUMINANCE;
	if (COM_CheckParm ("-lm_a"))
		gl_lightmap_format = GL_ALPHA;
	if (COM_CheckParm ("-lm_i"))
		gl_lightmap_format = GL_INTENSITY;
	if (COM_CheckParm ("-lm_3"))
		gl_lightmap_format = GL_RGB;
	if (COM_CheckParm ("-lm_4"))
		gl_lightmap_format = GL_RGBA;
	if (*gl_lightmapmode.string)
	{
		switch(*gl_lightmapmode.string)
		{
		case '1':
			gl_lightmap_format = GL_LUMINANCE;
			break;
		case 'a':
			gl_lightmap_format = GL_ALPHA;
			break;
		case 'i':
			gl_lightmap_format = GL_INTENSITY;
			break;
		case '3':
			gl_lightmap_format = GL_RGB;
			break;
		case '4':
			gl_lightmap_format = GL_RGBA;
			break;
		default:
			Con_Printf("%s contains unrecognised type\n", gl_lightmapmode.name);
		case '0':
			break;
		}
	}
*/
	if (cl.worldmodel->fromgame == fg_quake3 && gl_lightmap_format != GL_RGB && gl_lightmap_format != GL_RGBA)
		gl_lightmap_format = GL_RGB;


	switch (gl_lightmap_format)
	{
	case GL_RGBA:
		lightmap_bytes = 4;
		break;
	case GL_RGB:
		lightmap_bytes = 3;
		break;
	case GL_LUMINANCE:
	case GL_INTENSITY:
	case GL_ALPHA:
		lightmap_bytes = 1;
		break;
	}

	for (j=1 ; j<MAX_MODELS ; j++)
	{
		m = cl.model_precache[j];
		if (!m)
			break;
		if (m->name[0] == '*')
			continue;

		r_pcurrentvertbase = m->vertexes;
		currentmodel = m;
		shift = GLR_LightmapShift(currentmodel);

		for (i=0 ; i<m->numsurfaces ; i++)
		{
			GL_CreateSurfaceLightmap (m->surfaces + i, shift);
			P_EmitSkyEffectTris(m, &m->surfaces[i]);
			if (m->surfaces[i].mesh)	//there are some surfaces that have a display list already (the subdivided ones)
				continue;
			GL_BuildSurfaceDisplayList (m->surfaces + i);
		}
	}

	//
	// upload all lightmaps that were filled
	//
	for (i=0 ; i<numlightmaps ; i++)
	{
		if (!lightmap[i])
			break;		// no more used
		lightmap[i]->modified = false;
		lightmap[i]->rectchange.l = LMBLOCK_WIDTH;
		lightmap[i]->rectchange.t = LMBLOCK_HEIGHT;
		lightmap[i]->rectchange.w = 0;
		lightmap[i]->rectchange.h = 0;
		GL_Bind(lightmap_textures[i]);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		qglTexImage2D (GL_TEXTURE_2D, 0, lightmap_bytes
		, LMBLOCK_WIDTH, LMBLOCK_HEIGHT, 0, 
		gl_lightmap_format, GL_UNSIGNED_BYTE, lightmap[i]->lightmaps);

		if (gl_bump.value)
		{
			lightmap[i]->deluxmodified = false;
			lightmap[i]->deluxrectchange.l = LMBLOCK_WIDTH;
			lightmap[i]->deluxrectchange.t = LMBLOCK_HEIGHT;
			lightmap[i]->deluxrectchange.w = 0;
			lightmap[i]->deluxrectchange.h = 0;
			GL_Bind(deluxmap_textures[i]);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			qglTexImage2D (GL_TEXTURE_2D, 0, 3
			, LMBLOCK_WIDTH, LMBLOCK_HEIGHT, 0, 
			GL_RGB, GL_UNSIGNED_BYTE, lightmap[i]->deluxmaps);
		}
	}
}
#endif
