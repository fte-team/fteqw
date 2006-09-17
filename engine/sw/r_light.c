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
// r_light.c

#include "quakedef.h"
#include "r_local.h"

int	r_dlightframecount;


/*
==================
R_AnimateLight
==================
*/
void SWR_AnimateLight (void)
{
	int			i,j;
	int v1, v2;
	float f;
	
//
// light animations
// 'm' is normal light, 'a' is no light, 'z' is double bright
	f = (cl.time*r_lightstylespeed.value);
	if (f < 0)
		f = 0;
	i = (int)f;

	if (r_lightstylesmooth.value)
		f -= i;	//this can require updates at 1000 times a second.. Depends on your framerate of course
	else
		f = 0;	//only update them 10 times a second
	for (j=0 ; j<MAX_LIGHTSTYLES ; j++)
	{
		if (!cl_lightstyle[j].length)
		{
			d_lightstylevalue[j] = 256;
			cl_lightstyle[j].colour = 7;
			continue;
		}
		v1 = i % cl_lightstyle[j].length;
		v1 = cl_lightstyle[j].map[v1] - 'a';

		v2 = (i+1) % cl_lightstyle[j].length;
		v2 = cl_lightstyle[j].map[v2] - 'a';

		d_lightstylevalue[j] = (v1*(1-f) + v2*(f))*22;
	}
}


/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/

/*
=============
R_MarkLights
=============
*/
void SWR_MarkLights (dlight_t *light, int bit, mnode_t *node)
{
	mplane_t	*splitplane;
	float		dist;
	msurface_t	*surf;
	int			i;
	
	if (node->contents < 0)
		return;

	splitplane = node->plane;
	dist = DotProduct (light->origin, splitplane->normal) - splitplane->dist;
	
	if (dist > light->radius)
	{
		SWR_MarkLights (light, bit, node->children[0]);
		return;
	}
	if (dist < -light->radius)
	{
		SWR_MarkLights (light, bit, node->children[1]);
		return;
	}
		
// mark the polygons
	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->dlightframe != r_dlightframecount)
		{
			surf->dlightbits = 0;
			surf->dlightframe = r_dlightframecount;
		}
		surf->dlightbits |= bit;
	}

	SWR_MarkLights (light, bit, node->children[0]);
	SWR_MarkLights (light, bit, node->children[1]);
}

void SWR_Q2MarkLights (dlight_t *light, int bit, mnode_t *node)
{
	mplane_t	*splitplane;
	float		dist;
	msurface_t	*surf;
	int			i;
	
	if (node->contents != -1)
		return;

	splitplane = node->plane;
	dist = DotProduct (light->origin, splitplane->normal) - splitplane->dist;
	
	if (dist > light->radius)
	{
		SWR_Q2MarkLights (light, bit, node->children[0]);
		return;
	}
	if (dist < -light->radius)
	{
		SWR_Q2MarkLights (light, bit, node->children[1]);
		return;
	}
		
// mark the polygons
	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->dlightframe != r_dlightframecount)
		{
			surf->dlightbits = 0;
			surf->dlightframe = r_dlightframecount;
		}
		surf->dlightbits |= bit;
	}

	SWR_Q2MarkLights (light, bit, node->children[0]);
	SWR_Q2MarkLights (light, bit, node->children[1]);
}

/*
=============
R_PushDlights
=============
*/
void SWR_PushDlights (void)
{
	int		i;
	dlight_t	*l;

	r_dlightframecount = r_framecount + 1;	// because the count hasn't
											//  advanced yet for this frame

	if (!r_dynamic.value)
		return;

	l = cl_dlights;

	if (cl.worldmodel->fromgame == fg_quake2)
	{
		for (i=0 ; i<dlights_software ; i++, l++)
		{
			if (!l->radius)
				continue;
			SWR_Q2MarkLights ( l, 1<<i, cl.worldmodel->nodes );
		}

	}
	else
	{
		for (i=0 ; i<dlights_software ; i++, l++)
		{
			if (!l->radius)
				continue;
			SWR_MarkLights ( l, 1<<i, cl.worldmodel->nodes );
		}
	}
}


/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

int SWRecursiveLightPoint (mnode_t *node, vec3_t start, vec3_t end)
{
	int			r;
	float		front, back, frac;
	int			side;
	mplane_t	*plane;
	vec3_t		mid;
	msurface_t	*surf;
	int			s, t, ds, dt;
	int			i;
	mtexinfo_t	*tex;
	qbyte		*lightmap;
	unsigned	scale;
	int			maps;

	if (cl.worldmodel->fromgame == fg_quake2)
	{
		if (node->contents != -1)
			return -1;		// solid
	}
	else if (node->contents < 0)
		return -1;		// didn't hit anything
	
// calculate mid point

// FIXME: optimize for axial
	plane = node->plane;
	front = DotProduct (start, plane->normal) - plane->dist;
	back = DotProduct (end, plane->normal) - plane->dist;
	side = front < 0;
	
	if ( (back < 0) == side)
		return SWRecursiveLightPoint (node->children[side], start, end);
	
	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0])*frac;
	mid[1] = start[1] + (end[1] - start[1])*frac;
	mid[2] = start[2] + (end[2] - start[2])*frac;
	
// go down front side	
	r = SWRecursiveLightPoint (node->children[side], start, mid);
	if (r >= 0)
		return r;		// hit something
		
	if ( (back < 0) == side )
		return -1;		// didn't hit anuthing
		
// check for impact on this node

	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->flags & SURF_DRAWTILED)
			continue;	// no lightmaps

		tex = surf->texinfo;
		
		s = DotProduct (mid, tex->vecs[0]) + tex->vecs[0][3];
		t = DotProduct (mid, tex->vecs[1]) + tex->vecs[1][3];;

		if (s < surf->texturemins[0] ||
		t < surf->texturemins[1])
			continue;
		
		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];
		
		if ( ds > surf->extents[0] || dt > surf->extents[1] )
			continue;

		if (!surf->samples)
			return 0;

		ds >>= 4;
		dt >>= 4;

		lightmap = surf->samples;
		r = 0;
		if (lightmap)
		{

			lightmap += (dt * ((surf->extents[0]>>4)+1) + ds);

			for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
					maps++)
			{
				scale = d_lightstylevalue[surf->styles[maps]];
				r += *lightmap * scale;
				lightmap += ((surf->extents[0]>>4)+1) *
						((surf->extents[1]>>4)+1);
			}
			
			r >>= 8;
		}
		
		return r;
	}

// go down back side
	return SWRecursiveLightPoint (node->children[!side], mid, end);
}
int SWRecursiveLightPoint3C (mnode_t *node, vec3_t start, vec3_t end)
{
	int			r;
	float		front, back, frac;
	int			side;
	mplane_t	*plane;
	vec3_t		mid;
	msurface_t	*surf;
	int			s, t, ds, dt;
	int			i;
	mtexinfo_t	*tex;
	qbyte		*lightmap;
	unsigned	scale;
	int			maps;


	if (cl.worldmodel->fromgame == fg_quake2)
	{
		if (node->contents != -1)
			return -1;		// solid
	}
	else if (node->contents < 0)
		return -1;		// didn't hit anything
	
// calculate mid point

// FIXME: optimize for axial
	plane = node->plane;
	front = DotProduct (start, plane->normal) - plane->dist;
	back = DotProduct (end, plane->normal) - plane->dist;
	side = front < 0;
	
	if ( (back < 0) == side)
		return SWRecursiveLightPoint3C (node->children[side], start, end);
	
	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0])*frac;
	mid[1] = start[1] + (end[1] - start[1])*frac;
	mid[2] = start[2] + (end[2] - start[2])*frac;
	
// go down front side	
	r = SWRecursiveLightPoint (node->children[side], start, mid);
	if (r >= 0)
		return r;		// hit something
		
	if ( (back < 0) == side )
		return -1;		// didn't hit anuthing
		
// check for impact on this node

	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->flags & SURF_DRAWTILED)
			continue;	// no lightmaps

		tex = surf->texinfo;
		
		s = DotProduct (mid, tex->vecs[0]) + tex->vecs[0][3];
		t = DotProduct (mid, tex->vecs[1]) + tex->vecs[1][3];;

		if (s < surf->texturemins[0] ||
		t < surf->texturemins[1])
			continue;
		
		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];
		
		if ( ds > surf->extents[0] || dt > surf->extents[1] )
			continue;

		if (!surf->samples)
			return 0;

		ds >>= 4;
		dt >>= 4;

		lightmap = surf->samples;
		r = 0;
		if (lightmap)
		{

			lightmap += (dt * ((surf->extents[0]>>4)+1) + ds)*3;

			for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
					maps++)
			{
				scale = d_lightstylevalue[surf->styles[maps]];
				r += (lightmap[0]+lightmap[1]+lightmap[2])/3 * scale;
				lightmap += ((surf->extents[0]>>4)+1) *
						((surf->extents[1]>>4)+1)*3;
			}
			
			r >>= 8;
		}
		
		return r;
	}

// go down back side
	return SWRecursiveLightPoint3C (node->children[!side], mid, end);
}
int SWR_LightPoint (vec3_t p)
{
	vec3_t		end;
	int			r;
	extern qboolean r_usinglits; 
	
	if (r_refdef.flags & 1 || !cl.worldmodel || !cl.worldmodel->lightdata)
		return 255;
	
	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;
	
	if (r_usinglits)
		r = SWRecursiveLightPoint3C (cl.worldmodel->nodes, p, end);
	else
		r = SWRecursiveLightPoint (cl.worldmodel->nodes, p, end);
	
	if (r == -1)
		r = 0;

	if (r < r_refdef.ambientlight)
		r = r_refdef.ambientlight;

	return r;
}

void SWQ1BSP_LightPointValues(model_t *mod, vec3_t point, vec3_t res_diffuse, vec3_t res_ambient, vec3_t res_dir)
{
	vec3_t		end;
	float r;

	res_dir[0] = 0;	//software doesn't load luxes
	res_dir[1] = 1;
	res_dir[2] = 1;

	end[0] = point[0];
	end[1] = point[1];
	end[2] = point[2] - 2048;

	r = SWRecursiveLightPoint3C(mod->nodes, point, end);
	if (r < 0)
	{
		res_diffuse[0] = 0;
		res_diffuse[1] = 0;
		res_diffuse[2] = 0;
	
		res_ambient[0] = 0;
		res_ambient[1] = 0;
		res_ambient[2] = 0;
	}
	else
	{
		res_diffuse[0] = r;
		res_diffuse[1] = r;
		res_diffuse[2] = r;
	
		res_ambient[0] = r;
		res_ambient[1] = r;
		res_ambient[2] = r;
	}
}

