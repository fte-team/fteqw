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

#include "r_local.h"
#ifdef SWSTAINS
#include "d_local.h"
#endif


//#define MAX_SWDECALS (1<<8)

#if MAX_SWDECALS
decal_t decals[MAX_SWDECALS];
int nextdecal;

void SWR_AddDecal(vec3_t org);
#endif

drawsurf_t	r_drawsurf;

int				lightleft, sourcesstep, blocksize, sourcetstep;
int				lightdelta, lightdeltastep;
int				lightright, lightleftstep, lightrightstep, blockdivshift;
unsigned		blockdivmask;
void			*prowdestbase;
unsigned char	*pbasesource;
unsigned char   ptexcolor;
int				surfrowbytes;	// used by ASM files
unsigned		*r_lightptr;
int				r_stepback;
int				r_lightwidth;
int				r_numhblocks, r_numvblocks;
unsigned char	*r_source, *r_sourcemax;

void R_DrawSurfaceBlock8_mip0 (void);
void R_DrawSurfaceBlock8_mip1 (void);
void R_DrawSurfaceBlock8_mip2 (void);
void R_DrawSurfaceBlock8_mip3 (void);
void R_DrawSurfaceBlock16From8 (void);
void R_DrawSurfaceBlock32From8 (void);
void R_DrawSurfaceBlock32From8Lit (void);
void R_DrawSurfaceBlock32From32Lit (void);
void R_DrawSurfaceBlock8_notex (void);

static void	(*surfmiptable[4])(void) = {
	R_DrawSurfaceBlock8_mip0,
	R_DrawSurfaceBlock8_mip1,
	R_DrawSurfaceBlock8_mip2,
	R_DrawSurfaceBlock8_mip3
};



unsigned		blocklights[18*18*3];







#ifdef SWSTAINS

extern cvar_t r_stains;
extern cvar_t r_stainfadetime;
extern cvar_t r_stainfadeammount;

#define	LMBLOCK_WIDTH		128
#define	LMBLOCK_HEIGHT		128
#define	MAX_LIGHTMAPS	64

typedef unsigned char stmap;
stmap stainmaps[MAX_LIGHTMAPS*LMBLOCK_WIDTH*LMBLOCK_HEIGHT];	//added to lightmap for added (hopefully) speed.
int			allocated[MAX_LIGHTMAPS][LMBLOCK_WIDTH];

//radius, x y z, a
void SWR_StainSurf (msurface_t *surf, float *parms)
{	
	int			sd, td;
	float		dist, rad, minlight;
	vec3_t		impact, local;
	int			s, t;
	int			i;
	int			smax, tmax;
	float amm;
	int lim;
	mtexinfo_t	*tex;

	stmap *stainbase;

	lim = 255 - (r_stains.value*255);

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;

	stainbase = stainmaps + surf->lightmaptexturenum*LMBLOCK_WIDTH*LMBLOCK_HEIGHT;
	stainbase += (surf->light_t * LMBLOCK_WIDTH + surf->light_s);

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
				amm = stainbase[(s)] - (dist - rad)*parms[4];
				stainbase[(s)] = bound(lim, amm, 255);

				surf->stained = true;
			}
		}
		stainbase += LMBLOCK_WIDTH;
	}

	if (surf->stained)
	{
		for (i = 0; i < 4; i++)
			if (surf->cachespots[i])
				surf->cachespots[i]->dlight = -1;			
	}
}

//combination of R_AddDynamicLights and R_MarkLights
void SWR_Q1BSP_StainNode (mnode_t *node, float *parms)
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
		SWR_Q1BSP_StainNode (node->children[0], parms);
		return;
	}
	if (dist < (-*parms))
	{
		SWR_Q1BSP_StainNode (node->children[1], parms);
		return;
	}

// mark the polygons
	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->flags&~(SURF_DRAWTURB|SURF_PLANEBACK))
			continue;
		SWR_StainSurf(surf, parms);
	}

	SWR_Q1BSP_StainNode (node->children[0], parms);
	SWR_Q1BSP_StainNode (node->children[1], parms);
}

void SWR_AddStain(vec3_t org, float red, float green, float blue, float radius)
{
	physent_t *pe;
	int i;
	float parms[5];

#if MAX_SWDECALS
	SWR_AddDecal(org);
#endif

	if (red != green && red != blue)	//sw only does luminance of stain maps
		return;							//a mix would look wrong.
	if (r_stains.value <= 0 || !cl.worldmodel)
		return;
	parms[0] = radius;
	parms[1] = org[0];
	parms[2] = org[1];
	parms[3] = org[2];
	parms[4] = (red + green + blue)/3;

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
			pe->model->funcs.StainNode(pe->model->nodes+pe->model->hulls[0].firstclipnode, parms);
		}
	}
}

void SWR_WipeStains(void)
{
	memset(stainmaps, 255, sizeof(stainmaps));
}

void SWR_LessenStains(void)
{
	int i, j;
	msurface_t	*surf;

	int			smax, tmax;
	int			s, t;
	stmap *stain;
	int stride;
	int ammount, limit;

	static float time;

	if (r_stains.value <= 0 || cl.paused)
	{
		time = 0;
		return;	
	}

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
			for (j = 0; j < 4; j++)
				if (surf->cachespots[j])
					surf->cachespots[j]->dlight = -1;

			smax = (surf->extents[0]>>4)+1;
			tmax = (surf->extents[1]>>4)+1;	

			stain = stainmaps + surf->lightmaptexturenum*LMBLOCK_WIDTH*LMBLOCK_HEIGHT;
			stain += (surf->light_t * LMBLOCK_WIDTH + surf->light_s);

			stride = (LMBLOCK_WIDTH-smax);

			surf->stained = false;			

			for (t = 0 ; t<tmax ; t++, stain+=stride)
			{
				for (s=0 ; s<smax ; s++)
				{
					if (*stain < limit)
					{
						*stain += ammount;
						surf->stained=true;
					}
					else	//reset to 255.
						*stain = 255;

					stain++;
				}				
			}
		}
	}
/*
	int i;
	msurface_t	*surf;

	if (rand()&31)
		return;
	surf = cl.worldmodel->surfaces;
	for (i=0 ; i<cl.worldmodel->numsurfaces ; i++, surf++)
	{
		if (surf->stained)
		{
			surf->stained=2;
		}
	}
*/
}



// returns a texture number and the position inside it
int SWAllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
	int		texnum;

	if (!w || !h)
		Sys_Error ("AllocBlock: bad size");

	for (texnum=0 ; texnum<MAX_LIGHTMAPS ; texnum++)
	{
		best = LMBLOCK_HEIGHT;

		for (i=0 ; i<LMBLOCK_WIDTH-w ; i++)
		{
			best2 = 0;

			for (j=0 ; j<w ; j++)
			{
				if (allocated[texnum][i+j] >= best)
					break;
				if (allocated[texnum][i+j] > best2)
					best2 = allocated[texnum][i+j];
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
			allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error ("AllocBlock: full");
	return 0;
}

void SWR_CreateSurfaceLightmap (msurface_t *surf)
{
	int		smax, tmax;

	if (surf->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
		return;
	if (surf->texinfo->flags & (TEX_SPECIAL))
		return;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;

	surf->lightmaptexturenum = SWAllocBlock (smax, tmax, &surf->light_s, &surf->light_t);
}

void SWR_BuildLightmaps(void)
{
	int i;
	msurface_t	*surf;

	memset(allocated, 0, sizeof(allocated));

	SWR_WipeStains();

	surf = cl.worldmodel->surfaces;
	for (i=0 ; i<cl.worldmodel->numsurfaces ; i++, surf++)
	{
//		if ( cl.worldmodel->surfaces[i].flags & SURF_DRAWSKY )
//			P_EmitSkyEffectTris(cl.worldmodel, &cl.worldmodel->surfaces[i]);
		SWR_CreateSurfaceLightmap(surf);
	}
}
#endif

//retrieves the next decal to be used, unlinking if needed.
#if MAX_SWDECALS
decal_t *R_GetFreeDecal(void)
{
	decal_t *dec = &decals[nextdecal];
	if (dec->owner)
	{	//already in use.
		if (dec->prev)
			dec->prev->next = dec->next;
		else
			dec->owner->decal = dec->next;
		if (dec->next)
			dec->next->prev = dec->prev;

		dec->owner->cached_dlight = -1;	//get the surface to redraw.
	}
	nextdecal = (nextdecal+1)&(MAX_SWDECALS-1);

	memset(dec, 0, sizeof(decal_t));

	return dec;
}
#endif

void R_WipeDecals(void)
{
#if MAX_SWDECALS
	int i;

	memset(decals, 0, sizeof(decals));
	for (i=0 ; i<cl.worldmodel->numsurfaces ; i++)
		cl.worldmodel->surfaces[i].decal = NULL;
#endif
}

#if MAX_SWDECALS

static vec3_t decalorg;
static float decalradius;
void SWR_AddSurfDecal (msurface_t *surf)
{	
	int			sd, td;
	float		dist, rad, minlight;
	vec3_t		impact, local;
	int			s, t;
	int			i;
	int			smax, tmax;
	float amm;
	mtexinfo_t	*tex;
	decal_t *dec;
	decal_t *prev;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;

	rad = decalradius;
	dist = DotProduct (decalorg, surf->plane->normal) - surf->plane->dist;
	rad -= fabs(dist);
	minlight = 0;
	if (rad < minlight)	//not hit
		return;
	minlight = rad - minlight;

	for (i=0 ; i<3 ; i++)
	{
		impact[i] = decalorg[i] - surf->plane->normal[i]*dist;
	}

	local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3];
	local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3];

	local[0] -= surf->texturemins[0];
	local[1] -= surf->texturemins[1];

	dec = R_GetFreeDecal();
	if (surf->decal)	//add to the end of the linked list.
	{
		prev = surf->decal;
		while(prev->next)
			prev = prev->next;

		prev->next = dec;
		dec->prev = prev;
	}
	else
		surf->decal = dec;	//no list yet
	dec->owner = surf;
	surf->cached_dlight = -1;
	
	dec->xpos = local[0];
	dec->ypos = local[1];
}

void SWR_Q1BSP_AddNodeDecal (mnode_t *node)
{
	mplane_t	*splitplane;
	float		dist;
	msurface_t	*surf;
	int			i;
	
	if (node->contents < 0)
		return;	

	splitplane = node->plane;
	dist = DotProduct (decalorg, splitplane->normal) - splitplane->dist;
	
	if (dist > (decalradius))
	{
		SWR_Q1BSP_AddNodeDecal (node->children[0]);
		return;
	}
	if (dist < (-decalradius))
	{
		SWR_Q1BSP_AddNodeDecal (node->children[1]);
		return;
	}

// mark the polygons
	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->flags&~(SURF_DRAWTURB|SURF_PLANEBACK))
			continue;
		SWR_AddSurfDecal(surf);
	}

	SWR_Q1BSP_AddNodeDecal (node->children[0]);
	SWR_Q1BSP_AddNodeDecal (node->children[1]);
}

void SWR_AddDecal(vec3_t org)
{
//	VectorCopy(org, decalorg);
//	decalradius = 320;
//	SWR_Q1BSP_AddNodeDecal(cl.worldmodel->nodes+cl.worldmodel->hulls[0].firstclipnode);
}


void SWR_DrawDecal8(decal_t *dec)
{
	mpic_t *srcpic = (mpic_t *)Draw_SafeCachePic ("gfx/conback.lmp");
	qbyte *srcimg = srcpic->data;
	int srcw=16, srch=16;
	int s, t;
	int stride;	//horizontal pixels to copy
	int lines;	//vertical pixels to copy

	qbyte *dest = r_drawsurf.surfdat;
	int dw = r_drawsurf.surfwidth, dh = r_drawsurf.surfheight;

	stride = dw;
	lines = dh;

	s=0;t=0;
	/*
	s = dec->xpos - srcw/2;
	t = dec->ypos - srch/2;
	if (s < 0)
	{
		stride-=s;
		s = 0;
	}
	if (t < 0)
	{
		stride-=t;
		t = 0;
	}
	*/

	//s and t are at the top left of the image.
	dest += s;	//align to the left
	srcimg += s;
	for (t = 0; t < lines; t++)
	{
		for (s = 0; s < stride; s++)
		{
			dest[s] = srcimg[s];
		}
		dest += dw;
		srcimg += srcw;
	}

	/*
	pixel_t		*surfdat;	// destination for generated surface
	int			rowbytes;	// destination logical width in bytes
	msurface_t	*surf;		// description for surface to generate
	fixed8_t	lightadj[MAXLIGHTMAPS];
							// adjust for lightmap levels for dynamic lighting
	texture_t	*texture;	// corrected for animating textures
	int			surfmip;	// mipmapped ratio of surface texels / world pixels
	int			surfwidth;	// in mipmapped texels
	int			surfheight;	// in mipmapped texels
	*/
}

#endif




/*
===============
R_AddDynamicLights
===============
*/
void SWR_AddDynamicLights (void)
{
	msurface_t *surf;
	int			lnum;
	int			sd, td;
	float		dist, rad, minlight;
	vec3_t		impact, local;
	int			s, t;
	int			i;
	int			smax, tmax;
	mtexinfo_t	*tex;

	float a;

	surf = r_drawsurf.surf;
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

		a = 256*(cl_dlights[lnum].color[0]*1.5 + cl_dlights[lnum].color[1]*2.95 + cl_dlights[lnum].color[2]*0.55);

		for (i=0 ; i<3 ; i++)
		{
			impact[i] = cl_dlights[lnum].origin[i] -
					surf->plane->normal[i]*dist;
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
					blocklights[t*smax + s] += (rad - dist)*a;
			}
		}
	}
}

void SWR_AddDynamicLightsRGB (void)
{
	msurface_t *surf;
	int			lnum;
	int			sd, td;
	float		dist, rad, minlight, r, g, b;
	vec3_t		impact, local;
	int			s, t;
	int			i;
	int			smax, tmax;
	mtexinfo_t	*tex;

	surf = r_drawsurf.surf;
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
					blocklights[(t*smax + s)*3] += (rad - dist)*r;
					blocklights[(t*smax + s)*3+1] += (rad - dist)*g;
					blocklights[(t*smax + s)*3+2] += (rad - dist)*b;
				}
			}
		}
	}
}
/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
void SWR_BuildLightMap (void)
{
	int			smax, tmax;
	int			t;
	int			i, size;
	qbyte		*lightmap;
	unsigned	scale;
	int			maps;
	msurface_t	*surf;

	surf = r_drawsurf.surf;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	size = smax*tmax;
	lightmap = surf->samples;

	if (/* r_fullbright.value || */ !cl.worldmodel->lightdata)
	{
		for (i=0 ; i<size ; i++)
			blocklights[i] = 0;
		return;
	}

// clear to ambient
	for (i=0 ; i<size ; i++)
		blocklights[i] = r_refdef.ambientlight<<8;


// add all the lightmaps
	if (lightmap)
		for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
			 maps++)
		{
			scale = r_drawsurf.lightadj[maps];	// 8.8 fraction		
			for (i=0 ; i<size ; i++)
				blocklights[i] += lightmap[i] * scale;
			lightmap += size;	// skip to next lightmap
		}

// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		SWR_AddDynamicLights ();
#ifdef SWSTAINS
	if (surf->stained)
	{
		stmap *stain;
		int sstride;
		int x, y;

		stain = stainmaps + surf->lightmaptexturenum*LMBLOCK_WIDTH*LMBLOCK_HEIGHT;
		stain += (surf->light_t * LMBLOCK_WIDTH + surf->light_s);

		sstride = LMBLOCK_WIDTH - smax;

		i=0;

		for (x = 0; x < tmax; x++, stain+=sstride)
		{
			for (y = 0; y < smax; y++, i++, stain++)
			{
				t = (255*256*256-127*256-(int)blocklights[i]*(*stain)) >> (16 - VID_CBITS);

				if (t < (1 << 6))
					t = (1 << 6);

				blocklights[i] = t;
			}
		}
		return;
	}
#endif
// bound, invert, and shift
	for (i=0 ; i<size ; i++)
	{
		t = (255*256 - (int)blocklights[i]) >> (8 - VID_CBITS);

		if (t < (1 << 6))
			t = (1 << 6);

		blocklights[i] = t;
	}
}

void SWR_BuildLightMapRGB (void)
{
	int			smax, tmax;
	int			i, size;
	qbyte		*lightmap;
	unsigned	scale;
	int			maps;
	msurface_t	*surf;
	int r,g,b, cr, cg, cb;

	surf = r_drawsurf.surf;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	size = smax*tmax*3;
	lightmap = surf->samples;

	if (/* r_fullbright.value || */ !cl.worldmodel->lightdata)
	{
		for (i=0 ; i<size ; i++)
			blocklights[i] = 0;
		return;
	}

// clear to ambient
	for (i=0 ; i<size ; i++)
		blocklights[i] = r_refdef.ambientlight<<8;


// add all the lightmaps
	if (lightmap)
		for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
			 maps++)
		{
			scale = r_drawsurf.lightadj[maps];	// 8.8 fraction		
			for (i=0 ; i<size ; i+=3)
			{
				blocklights[i]		+= lightmap[i] * scale;
				blocklights[i+1]	+= lightmap[i+1] * scale;
				blocklights[i+2]	+= lightmap[i+2] * scale;
			}
			lightmap += size;	// skip to next lightmap
		}

// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		SWR_AddDynamicLightsRGB ();
#ifdef SWSTAINS
	if (surf->stained)
	{
		stmap *stain;
		int sstride;
		int x, y;

		stain = stainmaps + surf->lightmaptexturenum*LMBLOCK_WIDTH*LMBLOCK_HEIGHT;
		stain += (surf->light_t * LMBLOCK_WIDTH + surf->light_s);

		sstride = LMBLOCK_WIDTH - smax;

		i=0;

		for (x = 0; x < tmax; x++, stain+=sstride)
		{
			for (y = 0; y < smax; y++, i+=3, stain++)
			{
				r = (255*256*256-127*256-(int)blocklights[i]*(*stain)) >> (16 - VID_CBITS);
				g = (255*256*256-127*256-(int)blocklights[i+1]*(*stain)) >> (16 - VID_CBITS);
				b = (255*256*256-127*256-(int)blocklights[i+2]*(*stain)) >> (16 - VID_CBITS);

#define MINL (1<<6)
#define MAXL ((255*256) >> (8 - VID_CBITS))

				cr = 0;
				cg = 0;
				cb = 0;

				if (r < MINL)
				{			
					cg += (MINL-r);
					cb += (MINL-r);
					r = MINL;
				}
				if (g < MINL)
				{			
					cr += (MINL-g);
					cb += (MINL-g);
					g = MINL;
				}
				if (b < MINL)
				{			
					cg += (MINL-b);
					cr += (MINL-b);
					b = MINL;
				}

				if (cr + r < MINL)
					blocklights[i] = MINL;
				else if (cr + r > MAXL)
					blocklights[i] = MAXL;
				else
					blocklights[i] = cr+r;
				if (cg + g < MINL)
					blocklights[i+1] = MINL;
				else if (cg + g > MAXL)
					blocklights[i+1] = MAXL;
				else
					blocklights[i+1] = cg+g;
				if (cb + b < MINL)
					blocklights[i+2] = MINL;
				else if (cb + b > MAXL)
					blocklights[i+2] = MAXL;
				else
					blocklights[i+2] = cb+b;
			}
		}
		return;
	}
#endif

// bound, invert, and shift
	for (i=0 ; i<size ; i+=3)	//this rather bulky and overcomplicated formula allows too much red to reduce the green+blue to give a more satisfying glow.
	{
		r = (255*256-(int)blocklights[i]) >> (8 - VID_CBITS);
		g = (255*256-(int)blocklights[i+1]) >> (8 - VID_CBITS);
		b = (255*256-(int)blocklights[i+2]) >> (8 - VID_CBITS);
#define MINL (1<<6)
#define MAXL ((255*256) >> (8 - VID_CBITS))

		cr = 0;
		cg = 0;
		cb = 0;

		if (r < MINL)
		{			
			cg += (MINL-r);
			cb += (MINL-r);
			r = MINL;
		}
		if (g < MINL)
		{			
			cr += (MINL-g);
			cb += (MINL-g);
			g = MINL;
		}
		if (b < MINL)
		{			
			cg += (MINL-b);
			cr += (MINL-b);
			b = MINL;
		}

		if (cr + r < MINL)
			blocklights[i] = MINL;
		else if (cr + r > MAXL)
			blocklights[i] = MAXL;
		else
			blocklights[i] = cr+r;
		if (cg + g < MINL)
			blocklights[i+1] = MINL;
		else if (cg + g > MAXL)
			blocklights[i+1] = MAXL;
		else
			blocklights[i+1] = cg+g;
		if (cb + b < MINL)
			blocklights[i+2] = MINL;
		else if (cb + b > MAXL)
			blocklights[i+2] = MAXL;
		else
			blocklights[i+2] = cb+b;
	}
}

/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
texture_t *SWR_TextureAnimation (texture_t *base)
{
	int		reletive;
	int		count;

	if (currententity->frame1)
	{
		if (base->alternate_anims)
			base = base->alternate_anims;
	}
	
	if (!base->anim_total)
		return base;

	reletive = (int)(cl.time*10) % base->anim_total;

	count = 0;	
	while (base->anim_min > reletive || base->anim_max <= reletive)
	{
		base = base->anim_next;
		if (!base)
			Sys_Error ("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Sys_Error ("R_TextureAnimation: infinite cycle");
	}

	return base;
}

/*
===============
R_DrawSurface
===============
*/
void R_DrawSurface (void)
{
	extern cvar_t r_drawflat;
	extern int r_wallindex, r_floorindex;
	extern unsigned char ptexcolor;
	unsigned char	*basetptr;
	int				smax, tmax, twidth;
	int				u;
	int				soffset, basetoffset, texwidth;
	int				horzblockstep;
	unsigned char	*pcolumndest;
	void			(*pblockdrawer)(void);
	texture_t		*mt;
#if MAX_SWDECALS
	decal_t			*dec;
#endif

// calculate the lightings
	SWR_BuildLightMap ();

	surfrowbytes = r_drawsurf.rowbytes;

	mt = r_drawsurf.texture;

	r_source = (qbyte *)mt + mt->offsets[r_drawsurf.surfmip];

// the fractional light values should range from 0 to (VID_GRADES - 1) << 16
// from a source range of 0 - 255
	
	texwidth = mt->width >> r_drawsurf.surfmip;

	blocksize = 16 >> r_drawsurf.surfmip;
	blockdivshift = 4 - r_drawsurf.surfmip;
	blockdivmask = (1 << blockdivshift) - 1;
	
	r_lightwidth = (r_drawsurf.surf->extents[0]>>4)+1;

	r_numhblocks = r_drawsurf.surfwidth >> blockdivshift;
	r_numvblocks = r_drawsurf.surfheight >> blockdivshift;

//==============================

	if (r_pixbytes == 1 || r_pixbytes == 4)	//if we are using 4, textures are stored as 1 and expanded acording to palette
	{
		if (r_drawflat.value && !(r_drawsurf.surf->flags & SURF_NOFLAT))
		{
			if (r_drawsurf.surf->plane->normal[2] <= 0.5)
				ptexcolor = r_wallindex;
			else
				ptexcolor = r_floorindex;
			pblockdrawer = R_DrawSurfaceBlock8_notex;
		}
		else
			pblockdrawer = surfmiptable[r_drawsurf.surfmip];
	// TODO: only needs to be set when there is a display settings change
		horzblockstep = blocksize;
	}
	else
	{
		pblockdrawer = R_DrawSurfaceBlock16From8;//16bit rendering uses 16bit caches.
	// TODO: only needs to be set when there is a display settings change
		horzblockstep = blocksize << 1;
	}

	smax = mt->width >> r_drawsurf.surfmip;
	twidth = texwidth;
	tmax = mt->height >> r_drawsurf.surfmip;
	sourcetstep = texwidth;
	r_stepback = tmax * twidth;

	r_sourcemax = r_source + (tmax * smax);

	soffset = r_drawsurf.surf->texturemins[0];
	basetoffset = r_drawsurf.surf->texturemins[1];

// << 16 components are to guarantee positive values for %
	soffset = ((soffset >> r_drawsurf.surfmip) + (smax << 16)) % smax;
	basetptr = &r_source[((((basetoffset >> r_drawsurf.surfmip) 
		+ (tmax << 16)) % tmax) * twidth)];

	pcolumndest = r_drawsurf.surfdat;

	for (u=0 ; u<r_numhblocks; u++)
	{
		r_lightptr = blocklights + u;

		prowdestbase = pcolumndest;

		pbasesource = basetptr + soffset;

		(*pblockdrawer)();

		soffset = soffset + blocksize;
		if (soffset >= smax)
			soffset = 0;

		pcolumndest += horzblockstep;
	}

#if MAX_SWDECALS
	if (r_drawsurf.surf->decal && !r_drawsurf.surfmip)
	{
		if (r_pixbytes == 1 || r_pixbytes == 4)
		{
			for (dec = r_drawsurf.surf->decal; dec; dec = dec->next)
			{
				int x, y;
				x = rand()%smax;
				y = rand()%tmax;
				pcolumndest = r_drawsurf.surfdat;
				pcolumndest[y*smax+x] = 15;
				SWR_DrawDecal8(dec);
			}
		}
	}
#endif
}

void R_DrawSurface32 (void)
{
	unsigned char	*basetptr;
	int				smax, tmax, twidth;
	int				u;
	int				soffset, basetoffset, texwidth;
	int				horzblockstep;
	unsigned char	*pcolumndest;
	texture_t		*mt;

// calculate the lightings
	SWR_BuildLightMapRGB ();
	
	surfrowbytes = r_drawsurf.rowbytes;

	mt = r_drawsurf.texture;
	
	r_source = (qbyte *)mt + mt->offsets[r_drawsurf.surfmip];
	
// the fractional light values should range from 0 to (VID_GRADES - 1) << 16
// from a source range of 0 - 255
	
	texwidth = (mt->width*mt->pixbytes) >> r_drawsurf.surfmip;

	blocksize = 16 >> r_drawsurf.surfmip;
	blockdivshift = 4 - r_drawsurf.surfmip;
	blockdivmask = (1 << blockdivshift) - 1;
	
	r_lightwidth = ((r_drawsurf.surf->extents[0]>>4)+1)*3;

	r_numhblocks = r_drawsurf.surfwidth >> blockdivshift;
	r_numvblocks = r_drawsurf.surfheight >> blockdivshift;

//==============================

	horzblockstep = blocksize*4;

	smax = (mt->width*mt->pixbytes) >> r_drawsurf.surfmip;
	twidth = texwidth;
	tmax = mt->height >> r_drawsurf.surfmip;
	sourcetstep = texwidth;
	r_stepback = tmax * twidth;	

	r_sourcemax = r_source + (tmax * smax);

	soffset = r_drawsurf.surf->texturemins[0]*mt->pixbytes;
	basetoffset = r_drawsurf.surf->texturemins[1];

// << 16 components are to guarantee positive values for %
	basetptr = &r_source[((((basetoffset >> r_drawsurf.surfmip) 
		+ (tmax << 16)) % tmax) * twidth)];
	soffset = ((soffset >> r_drawsurf.surfmip) + (smax << 16)) % (smax);

	pcolumndest = r_drawsurf.surfdat;

	if (mt->pixbytes == 4)
	{		
		for (u=0 ; u<r_numhblocks; u++)
		{
			r_lightptr = blocklights + u*3;

			prowdestbase = pcolumndest;

			pbasesource = basetptr + soffset;

			R_DrawSurfaceBlock32From32Lit();

			soffset = soffset + blocksize*4;
			if (soffset >= smax)
				soffset = 0;

			pcolumndest += horzblockstep;
		}
	}
	else
	{		
		for (u=0 ; u<r_numhblocks; u++)
		{
			r_lightptr = blocklights + u*3;

			prowdestbase = pcolumndest;

			pbasesource = basetptr + soffset;

			R_DrawSurfaceBlock32From8Lit();

			soffset = soffset + blocksize;
			if (soffset >= smax)
				soffset = 0;

			pcolumndest += horzblockstep;
		}
	}
}

//=============================================================================
void R_DrawSurfaceBlock8_notex (void)
{
	int				v, i, b, lightstep, lighttemp, light;
	unsigned char	pix, *prowdest;

	pix = ptexcolor;
	prowdest = prowdestbase;

	for (v=0 ; v<r_numvblocks ; v++)
	{
	// FIXME: make these locals?
	// FIXME: use delta rather than both right and left, like ASM?
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> blockdivshift;
		lightrightstep = (r_lightptr[1] - lightright) >> blockdivshift;

		for (i=0 ; i<blocksize ; i++)
		{
			lighttemp = lightleft - lightright;
			lightstep = lighttemp >> blockdivshift;

			light = lightright;

			for (b=blocksize-1; b>=0; b--)
			{
				prowdest[b] = ((unsigned char *)vid.colormap)
						[(light & 0xFF00) + pix];
				light += lightstep;
			}
	
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += surfrowbytes;
		}
	}
}

#if	!id386

/*
================
R_DrawSurfaceBlock8_mip0
================
*/
void R_DrawSurfaceBlock8_mip0 (void)
{
	int				v, i, b, lightstep, lighttemp, light;
	unsigned char	pix, *psource, *prowdest;

	psource = pbasesource;
	prowdest = prowdestbase;

	for (v=0 ; v<r_numvblocks ; v++)
	{
	// FIXME: make these locals?
	// FIXME: use delta rather than both right and left, like ASM?
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 4;
		lightrightstep = (r_lightptr[1] - lightright) >> 4;

		for (i=0 ; i<16 ; i++)
		{
			lighttemp = lightleft - lightright;
			lightstep = lighttemp >> 4;

			light = lightright;

			for (b=15; b>=0; b--)
			{
				pix = psource[b];
				prowdest[b] = ((unsigned char *)vid.colormap)
						[(light & 0xFF00) + pix];
				light += lightstep;
			}
	
			psource += sourcetstep;
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += surfrowbytes;
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}


/*
================
R_DrawSurfaceBlock8_mip1
================
*/
void R_DrawSurfaceBlock8_mip1 (void)
{
	int				v, i, b, lightstep, lighttemp, light;
	unsigned char	pix, *psource, *prowdest;

	psource = pbasesource;
	prowdest = prowdestbase;

	for (v=0 ; v<r_numvblocks ; v++)
	{
	// FIXME: make these locals?
	// FIXME: use delta rather than both right and left, like ASM?
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 3;
		lightrightstep = (r_lightptr[1] - lightright) >> 3;

		for (i=0 ; i<8 ; i++)
		{
			lighttemp = lightleft - lightright;
			lightstep = lighttemp >> 3;

			light = lightright;

			for (b=7; b>=0; b--)
			{
				pix = psource[b];
				prowdest[b] = ((unsigned char *)vid.colormap)
						[(light & 0xFF00) + pix];
				light += lightstep;
			}
	
			psource += sourcetstep;
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += surfrowbytes;
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}


/*
================
R_DrawSurfaceBlock8_mip2
================
*/
void R_DrawSurfaceBlock8_mip2 (void)
{
	int				v, i, b, lightstep, lighttemp, light;
	unsigned char	pix, *psource, *prowdest;

	psource = pbasesource;
	prowdest = prowdestbase;

	for (v=0 ; v<r_numvblocks ; v++)
	{
	// FIXME: make these locals?
	// FIXME: use delta rather than both right and left, like ASM?
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 2;
		lightrightstep = (r_lightptr[1] - lightright) >> 2;

		for (i=0 ; i<4 ; i++)
		{
			lighttemp = lightleft - lightright;
			lightstep = lighttemp >> 2;

			light = lightright;

			for (b=3; b>=0; b--)
			{
				pix = psource[b];
				prowdest[b] = ((unsigned char *)vid.colormap)
						[(light & 0xFF00) + pix];
				light += lightstep;
			}
	
			psource += sourcetstep;
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += surfrowbytes;
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}


/*
================
R_DrawSurfaceBlock8_mip3
================
*/
void R_DrawSurfaceBlock8_mip3 (void)
{
	int				v, i, b, lightstep, lighttemp, light;
	unsigned char	pix, *psource, *prowdest;

	psource = pbasesource;
	prowdest = prowdestbase;

	for (v=0 ; v<r_numvblocks ; v++)
	{
	// FIXME: make these locals?
	// FIXME: use delta rather than both right and left, like ASM?
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 1;
		lightrightstep = (r_lightptr[1] - lightright) >> 1;

		for (i=0 ; i<2 ; i++)
		{
			lighttemp = lightleft - lightright;
			lightstep = lighttemp >> 1;

			light = lightright;

			for (b=1; b>=0; b--)
			{
				pix = psource[b];
				prowdest[b] = ((unsigned char *)vid.colormap)
						[(light & 0xFF00) + pix];
				light += lightstep;
			}
	
			psource += sourcetstep;
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += surfrowbytes;
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}

#endif

//add lits?
void R_DrawSurfaceBlock16From8 (void)
{
	int				v, i, b, lightstep, lighttemp, light;
	unsigned char	pix, *psource;
	unsigned short	*prowdest;

	psource = pbasesource;
	prowdest = prowdestbase;

	for (v=0 ; v<r_numvblocks ; v++)
	{
	// FIXME: make these locals?
	// FIXME: use delta rather than both right and left, like ASM?
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> blockdivshift;
		lightrightstep = (r_lightptr[1] - lightright) >> blockdivshift;
		
		for (i=0 ; i<blocksize ; i++)
		{
			lighttemp = lightleft - lightright;
			lightstep = lighttemp >> blockdivshift;

			light = lightright;

			for (b=blocksize-1; b>=0; b--)
			{
				pix = psource[b];
				prowdest[b] = vid.colormap16[(light & 0xFF00) + pix];
				light += lightstep;
			}
	
			psource += sourcetstep;
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += surfrowbytes;
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}



//8 bit disk texture to 32 bit mem cache
void R_DrawSurfaceBlock32From8 (void)
{
	int				v, i, b, lightstep, lighttemp, light;
	unsigned char	pix, *psource;
	unsigned int	*prowdest;

	psource = pbasesource;
	prowdest = prowdestbase;

	for (v=0 ; v<r_numvblocks ; v++)
	{
	// FIXME: make these locals?
	// FIXME: use delta rather than both right and left, like ASM?
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> blockdivshift;
		lightrightstep = (r_lightptr[1] - lightright) >> blockdivshift;
		
		for (i=0 ; i<blocksize ; i++)
		{
			lighttemp = lightleft - lightright;
			lightstep = lighttemp >> blockdivshift;

			light = lightright;

			for (b=blocksize-1; b>=0; b--)
			{
				pix = psource[b];
				prowdest[b] = d_8to32table[((unsigned char *)vid.colormap)[(light & 0xFF00) + pix]];
				light += lightstep;
			}
	
			psource += sourcetstep;
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += surfrowbytes;
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}

//8 bit disk texture to 32 bit mem cache, with added rgb lighting
void R_DrawSurfaceBlock32From8Lit (void)
{
//#define SMOOTHLIGHT
	int				v, i, b;
	int				lightstepr;
	int				lightleftr, lightrightr, lightleftstepr, lightrightstepr;
	int				lightstepg;
	int				lightleftg, lightrightg, lightleftstepg, lightrightstepg;
	int				lightstepb;
	int				lightleftb, lightrightb, lightleftstepb, lightrightstepb;
	unsigned char	pix, *psource;
	unsigned int	lightb, lightg, lightr;
#ifdef SMOOTHLIGHT
	unsigned char	*prowdest;
#else
	unsigned int	*prowdest;
#endif

	psource = pbasesource;
	prowdest = prowdestbase;

	for (v=0 ; v<r_numvblocks ; v++)
	{
	// FIXME: make these locals?
	// FIXME: use delta rather than both right and left, like ASM?
		lightleftr	= r_lightptr[0];
		lightrightr	= r_lightptr[3];
		lightleftg	= r_lightptr[0+1];
		lightrightg	= r_lightptr[3+1];
		lightleftb	= r_lightptr[0+2];
		lightrightb	= r_lightptr[3+2];

		r_lightptr += r_lightwidth;

		lightleftstepr	= (r_lightptr[0] - lightleftr) >> blockdivshift;
		lightrightstepr	= (r_lightptr[3] - lightrightr) >> blockdivshift;
		lightleftstepg	= (r_lightptr[0+1] - lightleftg) >> blockdivshift;
		lightrightstepg	= (r_lightptr[3+1] - lightrightg) >> blockdivshift;
		lightleftstepb	= (r_lightptr[0+2] - lightleftb) >> blockdivshift;
		lightrightstepb	= (r_lightptr[3+2] - lightrightb) >> blockdivshift;
		for (i=0 ; i<blocksize ; i++)
		{			
			lightstepr = (lightleftr - lightrightr) >> blockdivshift;
			lightstepg = (lightleftg - lightrightg) >> blockdivshift;
			lightstepb = (lightleftb - lightrightb) >> blockdivshift;

			lightr = lightrightr;
			lightg = lightrightg;
			lightb = lightrightb;

			for (b=blocksize-1; b>=0; b--)
			{
				pix = psource[b];
#ifndef SMOOTHLIGHT
#if 1
				prowdest[b] =	(d_8to32table[((unsigned char *)vid.colormap)[(lightb & 0xFF00) + pix]]&0xff) |
								(d_8to32table[((unsigned char *)vid.colormap)[(lightg & 0xFF00) + pix]]&0xff00) |
								(d_8to32table[((unsigned char *)vid.colormap)[(lightr & 0xFF00) + pix]]&0xff0000);
#else
				prowdest[b] =	(d_8to32table[((unsigned char *)vid.colormap)[(lightb & 0xFF00) + 15]]&0xff) |
								(d_8to32table[((unsigned char *)vid.colormap)[(lightg & 0xFF00) + 15]]&0xff00) |
								(d_8to32table[((unsigned char *)vid.colormap)[(lightr & 0xFF00) + 15]]&0xff0000);
#endif
#else
#if 1
				prowdest[b*4]	=	(((d_8to32table[pix]&0x0000ff)    )	* (0x3f00-(lightb& 0x3fff)))>>13;
				prowdest[b*4+1]	=	(((d_8to32table[pix]&0x00ff00)>>8 )	* (0x3f00-(lightg& 0x3fff)))>>13;
				prowdest[b*4+2]	=	(((d_8to32table[pix]&0xff0000)>>16)	* (0x3f00-(lightr& 0x3fff)))>>13;
#else
				prowdest[b*4]	=	(((d_8to32table[pix]&0x0000ff)    )	* (0x4000-(lightb& 0x4000)))>>15;
				prowdest[b*4+1]	=	((255)	* (0xFFFF-(lightg& 0xFF00)))>>14;
				prowdest[b*4+2]	=	((255)	* (0xFFFF-(lightr& 0xFF00)))>>14;
#endif
#endif
				lightr += lightstepr;
				lightg += lightstepg;
				lightb += lightstepb;
			}

			psource += sourcetstep;
			lightrightr += lightrightstepr;
			lightleftr += lightleftstepr;
			lightrightg += lightrightstepg;
			lightleftg += lightleftstepg;
			lightrightb += lightrightstepb;
			lightleftb += lightleftstepb;
#ifdef SMOOTHLIGHT
			prowdest += surfrowbytes*4;
#else
			prowdest += surfrowbytes;
#endif
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}

extern qbyte		gammatable[];
//32 bit disk texture to 32 bit mem cache, with added rgb lighting
void R_DrawSurfaceBlock32From32Lit (void)
{
	int				v, i, b;
	int				lightstepr, lightr;
	int				lightleftr, lightrightr, lightleftstepr, lightrightstepr;
	int				lightstepg, lightg;
	int				lightleftg, lightrightg, lightleftstepg, lightrightstepg;
	int				lightstepb, lightb;
	int				lightleftb, lightrightb, lightleftstepb, lightrightstepb;
	qbyte	pix, *psource;
	qbyte	*prowdest;

	psource = pbasesource;
	prowdest = prowdestbase;

	for (v=0 ; v<r_numvblocks ; v++)
	{
	// FIXME: make these locals?
	// FIXME: use delta rather than both right and left, like ASM?
		lightleftr	= r_lightptr[0];
		lightrightr	= r_lightptr[3];
		lightleftg	= r_lightptr[0+1];
		lightrightg	= r_lightptr[3+1];
		lightleftb	= r_lightptr[0+2];
		lightrightb	= r_lightptr[3+2];

		r_lightptr += r_lightwidth;

		lightleftstepr	= (r_lightptr[0] - lightleftr) >> blockdivshift;
		lightrightstepr	= (r_lightptr[3] - lightrightr) >> blockdivshift;
		lightleftstepg	= (r_lightptr[0+1] - lightleftg) >> blockdivshift;
		lightrightstepg	= (r_lightptr[3+1] - lightrightg) >> blockdivshift;
		lightleftstepb	= (r_lightptr[0+2] - lightleftb) >> blockdivshift;
		lightrightstepb	= (r_lightptr[3+2] - lightrightb) >> blockdivshift;
		for (i=0 ; i<blocksize ; i++)
		{			
			lightstepr = (lightleftr - lightrightr) >> blockdivshift;
			lightstepg = (lightleftg - lightrightg) >> blockdivshift;
			lightstepb = (lightleftb - lightrightb) >> blockdivshift;

			lightr = lightrightr;
			lightg = lightrightg;
			lightb = lightrightb;

			for (b=(blocksize-1)<<2; b>=0; b-=4)
			{
				pix = psource[b+2];
				prowdest[b+0] = pix;
				
				pix = psource[b+1];
				prowdest[b+1] = pix;

				pix = psource[b+0];
				prowdest[b+2] = pix;

				prowdest[b+0] = gammatable[((int)psource[b+2]*(0x3FFF-(lightb & 0x3FFF))) / 0x3FFF];
				prowdest[b+1] = gammatable[((int)psource[b+1]*(0x3FFF-(lightg & 0x3FFF))) / 0x3FFF];
				prowdest[b+2] = gammatable[((int)psource[b+0]*(0x3FFF-(lightr & 0x3FFF))) / 0x3FFF];
				prowdest[b+3] = psource[b+3];

				lightr += lightstepr;
				lightg += lightstepg;
				lightb += lightstepb;
			}
	
			psource += sourcetstep;
			lightrightr += lightrightstepr;
			lightleftr += lightleftstepr;
			lightrightg += lightrightstepg;
			lightleftg += lightleftstepg;
			lightrightb += lightrightstepb;
			lightleftb += lightleftstepb;
			prowdest += surfrowbytes<<2;
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}

//============================================================================

/*
================
R_GenTurbTile
================
*/
void R_GenTurbTile (pixel_t *pbasetex, void *pdest)
{
	int		*turb;
	int		i, j, s, t;
	qbyte	*pd;
	
	turb = sintable + ((int)(cl.time*SPEED)&(CYCLE-1));
	pd = (qbyte *)pdest;

	for (i=0 ; i<TILE_SIZE ; i++)
	{
		for (j=0 ; j<TILE_SIZE ; j++)
		{	
			s = (((j << 16) + turb[i & (CYCLE-1)]) >> 16) & 63;
			t = (((i << 16) + turb[j & (CYCLE-1)]) >> 16) & 63;
			*pd++ = *(pbasetex + (t<<6) + s);
		}
	}
}


/*
================
R_GenTurbTile16
================
*/
void R_GenTurbTile16 (pixel_t *pbasetex, void *pdest)
{
	int				*turb;
	int				i, j, s, t;
	unsigned short	*pd;

	turb = sintable + ((int)(cl.time*SPEED)&(CYCLE-1));
	pd = (unsigned short *)pdest;

	for (i=0 ; i<TILE_SIZE ; i++)
	{
		for (j=0 ; j<TILE_SIZE ; j++)
		{	
			s = (((j << 16) + turb[i & (CYCLE-1)]) >> 16) & 63;
			t = (((i << 16) + turb[j & (CYCLE-1)]) >> 16) & 63;
			*pd++ = d_8to16table[*(pbasetex + (t<<6) + s)];
		}
	}
}


/*
================
R_GenTile
================
*/
void R_GenTile (msurface_t *psurf, void *pdest)
{
	if (psurf->flags & SURF_DRAWTURB)
	{
		if (r_pixbytes == 1)
		{
			R_GenTurbTile ((pixel_t *)
				((qbyte *)psurf->texinfo->texture + psurf->texinfo->texture->offsets[0]), pdest);
		}
		else
		{
			R_GenTurbTile16 ((pixel_t *)
				((qbyte *)psurf->texinfo->texture + psurf->texinfo->texture->offsets[0]), pdest);
		}
	}
	else if (psurf->flags & SURF_DRAWSKY)
	{
		if (r_pixbytes == 1)
		{
			R_GenSkyTile (pdest);
		}
		else
		{
			R_GenSkyTile16 (pdest);
		}
	}
	else
	{
		Sys_Error ("Unknown tile type");
	}
}

