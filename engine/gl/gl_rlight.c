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
#include "glquake.h"

int	r_dlightframecount;


/*
==================
R_AnimateLight
==================
*/
void GLR_AnimateLight (void)
{
	int			i,j,k;
	
//
// light animations
// 'm' is normal light, 'a' is no light, 'z' is double bright
	i = (int)(cl.time*10);
	for (j=0 ; j<MAX_LIGHTSTYLES ; j++)
	{
		if (!cl_lightstyle[j].length)
		{
			d_lightstylevalue[j] = 256;
			continue;
		}
		k = i % cl_lightstyle[j].length;
		k = cl_lightstyle[j].map[k] - 'a';
		k = k*22;
		d_lightstylevalue[j] = k;
	}	
}

/*
=============================================================================

DYNAMIC LIGHTS BLEND RENDERING

=============================================================================
*/

void AddLightBlend (float r, float g, float b, float a2)
{
	float	a;

	v_blend[3] = a = v_blend[3] + a2*(1-v_blend[3]);

	a2 = a2/a;

	v_blend[0] = v_blend[1]*(1-a2) + r*a2;
	v_blend[1] = v_blend[1]*(1-a2) + g*a2;
	v_blend[2] = v_blend[2]*(1-a2) + b*a2;
//Con_Printf("AddLightBlend(): %4.2f %4.2f %4.2f %4.6f\n", v_blend[0], v_blend[1], v_blend[2], v_blend[3]);
}

float bubble_sintable[17], bubble_costable[17];

void R_InitBubble() {
	float a;
	int i;
	float *bub_sin, *bub_cos;

	bub_sin = bubble_sintable;
	bub_cos = bubble_costable;

	for (i=16 ; i>=0 ; i--)
	{
		a = i/16.0 * M_PI*2;
		*bub_sin++ = sin(a);
		*bub_cos++ = cos(a);
	}
}

void R_RenderDlight (dlight_t *light)
{
	int		i, j;
//	float	a;
	vec3_t	v;
	float	rad;
	float	*bub_sin, *bub_cos;

	bub_sin = bubble_sintable;
	bub_cos = bubble_costable;
	rad = light->radius * 0.35;

	VectorSubtract (light->origin, r_origin, v);
	if (Length (v) < rad)
	{	// view is inside the dlight
		AddLightBlend (1, 0.5, 0, light->radius * 0.0003);
		return;
	}

	glBegin (GL_TRIANGLE_FAN);
//	glColor3f (0.2,0.1,0.0);
//	glColor3f (0.2,0.1,0.05); // changed dimlight effect
	glColor4f (light->color[0]*2, light->color[1]*2, light->color[2]*2,
		1);//light->color[3]);
	for (i=0 ; i<3 ; i++)
		v[i] = light->origin[i] - vpn[i]*rad/1.5;
	glVertex3fv (v);
	glColor3f (0,0,0);
	for (i=16 ; i>=0 ; i--)
	{
//		a = i/16.0 * M_PI*2;
		for (j=0 ; j<3 ; j++)
			v[j] = light->origin[j] + (vright[j]*(*bub_cos) +
				+ vup[j]*(*bub_sin)) * rad;
		bub_sin++; 
		bub_cos++;
		glVertex3fv (v);
	}
	glEnd ();
}

/*
=============
R_RenderDlights
=============
*/
void R_RenderDlights (void)
{
	int		i;
	dlight_t	*l;

	if (!r_flashblend.value)
		return;

//	r_dlightframecount = r_framecount + 1;	// because the count hasn't
											//  advanced yet for this frame
	glDepthMask (0);
	glDisable (GL_TEXTURE_2D);
	glShadeModel (GL_SMOOTH);
	glEnable (GL_BLEND);
	glBlendFunc (GL_ONE, GL_ONE);

	l = cl_dlights;
	for (i=0 ; i<MAX_DLIGHTS ; i++, l++)
	{
		if (l->die < cl.time || !l->radius)
			continue;
		R_RenderDlight (l);
	}

	glColor3f (1,1,1);
	glDisable (GL_BLEND);
	glEnable (GL_TEXTURE_2D);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask (1);
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
/*void GLR_MarkLights (dlight_t *light, int bit, mnode_t *node)
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
		GLR_MarkLights (light, bit, node->children[0]);
		return;
	}
	if (dist < -light->radius)
	{
		GLR_MarkLights (light, bit, node->children[1]);
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

	GLR_MarkLights (light, bit, node->children[0]);
	GLR_MarkLights (light, bit, node->children[1]);
}*/
/*void Q2BSP_MarkLights (dlight_t *light, int bit, mnode_t *node)
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
		Q2BSP_MarkLights (light, bit, node->children[0]);
		return;
	}
	if (dist < -light->radius)
	{
		Q2BSP_MarkLights (light, bit, node->children[1]);
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

	Q2BSP_MarkLights (light, bit, node->children[0]);
	Q2BSP_MarkLights (light, bit, node->children[1]);
}*/

void GLR_MarkQ3Lights (dlight_t *light, int bit, mnode_t *node)
{
	mplane_t	*splitplane;
	float		dist;
	msurface_t	*surf;
	int			i;

	return;	//we need to get the texinfos right first.

/*
	//mark all
	for (surf = cl.worldmodel->surfaces, i = 0;  i < cl.worldmodel->numsurfaces; i++, surf++)
	{
		if (surf->dlightframe != r_dlightframecount)
		{
			surf->dlightbits = 0;
			surf->dlightframe = r_dlightframecount;
		}
		surf->dlightbits |= bit;
	}
	return;
*/
	if (node->contents != -1)
	{
		msurface_t	**mark;
		mleaf_t *leaf;

		// mark the polygons
		leaf = (mleaf_t *)node;
		mark = leaf->firstmarksurface;
		for (i=0 ; i<leaf->nummarksurfaces ; i++, surf++)
		{
			surf = *mark++;
			if (surf->dlightframe != r_dlightframecount)
			{
				surf->dlightbits = 0;
				surf->dlightframe = r_dlightframecount;
			}
			surf->dlightbits |= bit;
		}

		return;	
	}

	splitplane = node->plane;
	dist = DotProduct (light->origin, splitplane->normal) - splitplane->dist;
	
	if (dist > light->radius)
	{
		GLR_MarkQ3Lights (light, bit, node->children[0]);
		return;
	}
	if (dist < -light->radius)
	{
		GLR_MarkQ3Lights (light, bit, node->children[1]);
		return;
	}

	GLR_MarkQ3Lights (light, bit, node->children[0]);
	GLR_MarkQ3Lights (light, bit, node->children[1]);
}



/*
=============
R_PushDlights
=============
*/
void GLR_PushDlights (void)
{
	int		i;
	dlight_t	*l;

	r_dlightframecount = r_framecount + 1;	// because the count hasn't
											//  advanced yet for this frame

	if (!r_dynamic.value)
		return;

//	if (!cl.worldmodel->nodes)
//		return;

	
	l = cl_dlights;
	for (i=0 ; i<MAX_DLIGHTS ; i++, l++)
	{
		if (l->die < cl.time || !l->radius)
			continue;
		cl.worldmodel->funcs.MarkLights( l, 1<<i, cl.worldmodel->nodes );
	}

/*
	if (cl.worldmodel->fromgame == fg_quake3)
	{
		for (i=0 ; i<MAX_DLIGHTS ; i++, l++)
		{
			if (l->die < cl.time || !l->radius)
				continue;
			GLR_MarkQ3Lights ( l, 1<<i, cl.worldmodel->nodes );
		}
		return;
	}
	if (cl.worldmodel->fromgame == fg_quake2)
	{
		for (i=0 ; i<MAX_DLIGHTS ; i++, l++)
		{
			if (l->die < cl.time || !l->radius)
				continue;
			GLR_MarkQ2Lights ( l, 1<<i, cl.worldmodel->nodes );
		}
		return;
	}
	for (i=0 ; i<MAX_DLIGHTS ; i++, l++)
	{
		if (l->die < cl.time || !l->radius)
			continue;
		GLR_MarkLights ( l, 1<<i, cl.worldmodel->nodes );
	}*/
}


/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

mplane_t		*lightplane;
vec3_t			lightspot;

void GLQ3_LightGrid(vec3_t point, vec3_t res_diffuse, vec3_t res_ambient, vec3_t res_dir)
{
	q3lightgridinfo_t *lg = (q3lightgridinfo_t *)cl.worldmodel->lightgrid;
	int index[4];
	int vi[3];
	int i, j;
	float t[8], direction_uv[3];
	vec3_t vf, vf2;
	vec3_t ambient, diffuse;

	res_dir[0] = 1;
	res_dir[1] = 1;
	res_dir[2] = 0.1;

	if (!lg)
	{
		res_ambient[0] = 255;
		res_ambient[1] = 255;
		res_ambient[2] = 255;
		return;
	}

	//If in doubt, steal someone else's code...
	//Thanks QFusion.

	for ( i = 0; i < 3; i++ )
	{
		vf[i] = (point[i] - lg->gridMins[i]) / lg->gridSize[i];
		vi[i] = (int)vf[i];
		vf[i] = vf[i] - floor(vf[i]);
		vf2[i] = 1.0f - vf[i];
	}

	index[0] = vi[2]*lg->gridBounds[3] + vi[1]*lg->gridBounds[0] + vi[0];
	index[1] = index[0] + lg->gridBounds[0];
	index[2] = index[0] + lg->gridBounds[3];
	index[3] = index[2] + lg->gridBounds[0];

	for ( i = 0; i < 4; i++ )
	{
		if ( index[i] < 0 || index[i] >= (lg->numlightgridelems-1) )
		{
			res_ambient[0] = 255;	//out of the map
			res_ambient[1] = 255;
			res_ambient[2] = 255;
			return;
		}
	}

	t[0] = vf2[0] * vf2[1] * vf2[2];
	t[1] = vf[0] * vf2[1] * vf2[2];
	t[2] = vf2[0] * vf[1] * vf2[2];
	t[3] = vf[0] * vf[1] * vf2[2];
	t[4] = vf2[0] * vf2[1] * vf[2];
	t[5] = vf[0] * vf2[1] * vf[2];
	t[6] = vf2[0] * vf[1] * vf[2];
	t[7] = vf[0] * vf[1] * vf[2];

	for ( j = 0; j < 3; j++ )
	{
		ambient[j] = 0;
		diffuse[j] = 0;

		for ( i = 0; i < 4; i++ )
		{
			ambient[j] += t[i*2] * lg->lightgrid[ index[i] ].ambient[j];
			ambient[j] += t[i*2+1] * lg->lightgrid[ index[i] + 1 ].ambient[j];

			diffuse[j] += t[i*2] * lg->lightgrid[ index[i] ].diffuse[j];
			diffuse[j] += t[i*2+1] * lg->lightgrid[ index[i] + 1 ].diffuse[j];
		}
	}

	for ( j = 0; j < 2; j++ )
	{
		direction_uv[j] = 0;

		for ( i = 0; i < 4; i++ )
		{
			direction_uv[j] += t[i*2] * lg->lightgrid[ index[i] ].direction[j];
			direction_uv[j] += t[i*2+1] * lg->lightgrid[ index[i] + 1 ].direction[j];
		}

		direction_uv[j] = anglemod ( direction_uv[j] );
	}

	VectorCopy(ambient, res_ambient);
	if (res_diffuse)
		VectorCopy(diffuse, res_diffuse);
	if (res_dir)
	{
		vec3_t right, left;
		direction_uv[2] = 0;
		AngleVectors(direction_uv, res_dir, right, left);
	}
}

int GLRecursiveLightPoint (mnode_t *node, vec3_t start, vec3_t end)
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
		return GLRecursiveLightPoint (node->children[side], start, end);
	
	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0])*frac;
	mid[1] = start[1] + (end[1] - start[1])*frac;
	mid[2] = start[2] + (end[2] - start[2])*frac;
	
// go down front side	
	r = GLRecursiveLightPoint (node->children[side], start, mid);
	if (r >= 0)
		return r;		// hit something
		
	if ( (back < 0) == side )
		return -1;		// didn't hit anuthing
		
// check for impact on this node
	VectorCopy (mid, lightspot);
	lightplane = plane;

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
			if (cl.worldmodel->rgblighting)
			{
				lightmap += (dt * ((surf->extents[0]>>4)+1) + ds)*3;

				for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
						maps++)
				{
					scale = d_lightstylevalue[surf->styles[maps]];
					r += (lightmap[0]+lightmap[1]+lightmap[2]) * scale / 3;
					lightmap += ((surf->extents[0]>>4)+1) *
							((surf->extents[1]>>4)+1)*3;
				}

			}
			else
			{
				lightmap += dt * ((surf->extents[0]>>4)+1) + ds;

				for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
						maps++)
				{
					scale = d_lightstylevalue[surf->styles[maps]];
					r += *lightmap * scale;
					lightmap += ((surf->extents[0]>>4)+1) *
							((surf->extents[1]>>4)+1);
				}
			}
			
			r >>= 8;
		}
		
		return r;
	}

// go down back side
	return GLRecursiveLightPoint (node->children[!side], mid, end);
}



int GLR_LightPoint (vec3_t p)
{
	vec3_t		end;
	int			r;

	if (r_refdef.flags & 1)
		return 255;
	
	if (!cl.worldmodel || !cl.worldmodel->lightdata)
		return 255;

	if (cl.worldmodel->fromgame == fg_quake3)
	{
		GLQ3_LightGrid(p, NULL, end, NULL);
		return (end[0] + end[1] + end[2])/3;
	}
	
	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;
	
	r = GLRecursiveLightPoint (cl.worldmodel->nodes, p, end);
	
	if (r == -1)
		r = 0;

	return r;
}



#ifdef PEXT_LIGHTSTYLECOL

float *GLRecursiveLightPoint3C (mnode_t *node, vec3_t start, vec3_t end)
{
	static float l[6];
	float *r;
	float		front, back, frac;
	int			side;
	mplane_t	*plane;
	vec3_t		mid;
	msurface_t	*surf;
	int			s, t, ds, dt;
	int			i;
	mtexinfo_t	*tex;
	qbyte		*lightmap, *deluxmap;
	float	scale;
	int			maps;

	if (cl.worldmodel->fromgame == fg_quake2)
	{
		if (node->contents != -1)
			return NULL;		// solid
	}
	else if (node->contents < 0)
		return NULL;		// didn't hit anything
	
// calculate mid point

// FIXME: optimize for axial
	plane = node->plane;
	front = DotProduct (start, plane->normal) - plane->dist;
	back = DotProduct (end, plane->normal) - plane->dist;
	side = front < 0;
	
	if ( (back < 0) == side)
		return GLRecursiveLightPoint3C (node->children[side], start, end);
	
	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0])*frac;
	mid[1] = start[1] + (end[1] - start[1])*frac;
	mid[2] = start[2] + (end[2] - start[2])*frac;
	
// go down front side	
	r = GLRecursiveLightPoint3C (node->children[side], start, mid);
	if (r && r[0]+r[1]+r[2] >= 0)
		return r;		// hit something
		
	if ( (back < 0) == side )
		return NULL;		// didn't hit anuthing
		
// check for impact on this node
	VectorCopy (mid, lightspot);
	lightplane = plane;

	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->flags & SURF_DRAWTILED)
			continue;	// no lightmaps

		tex = surf->texinfo;
		
		s = DotProduct (mid, tex->vecs[0]) + tex->vecs[0][3];
		t = DotProduct (mid, tex->vecs[1]) + tex->vecs[1][3];

		if (s < surf->texturemins[0] ||
		t < surf->texturemins[1])
			continue;
		
		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];
		
		if ( ds > surf->extents[0] || dt > surf->extents[1] )
			continue;

		if (!surf->samples)
		{
			l[0]=0;l[1]=0;l[2]=0;
			l[3]=0;l[4]=1;l[5]=1;
			return l;
		}

		ds >>= 4;
		dt >>= 4;

		lightmap = surf->samples;
		l[0]=0;l[1]=0;l[2]=0;
		l[3]=0;l[4]=0;l[5]=0;
		if (lightmap)
		{
			if (cl.worldmodel->deluxdata)
			{
				if (cl.worldmodel->rgblighting)
				{
					deluxmap = surf->samples - cl.worldmodel->lightdata + cl.worldmodel->deluxdata;

					lightmap += (dt * ((surf->extents[0]>>4)+1) + ds)*3;
					deluxmap += (dt * ((surf->extents[0]>>4)+1) + ds)*3;
					for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
							maps++)
					{
						scale = d_lightstylevalue[surf->styles[maps]]/256.0f;

						if (cl_lightstyle[surf->styles[maps]].colour & 1)
							l[0] += lightmap[0] * scale;
						if (cl_lightstyle[surf->styles[maps]].colour & 2)
							l[1] += lightmap[1] * scale;
						if (cl_lightstyle[surf->styles[maps]].colour & 4)
							l[2] += lightmap[2] * scale;

						l[3] += (deluxmap[0]-127)*scale;
						l[4] += (deluxmap[1]-127)*scale;
						l[5] += (deluxmap[2]-127)*scale;

						lightmap += ((surf->extents[0]>>4)+1) *
								((surf->extents[1]>>4)+1) * 3;
						deluxmap += ((surf->extents[0]>>4)+1) *
								((surf->extents[1]>>4)+1) * 3;
					}

				}
				else
				{
					deluxmap = (surf->samples - cl.worldmodel->lightdata)*3 + cl.worldmodel->deluxdata;

					lightmap += (dt * ((surf->extents[0]>>4)+1) + ds);
					deluxmap += (dt * ((surf->extents[0]>>4)+1) + ds)*3;
					for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
							maps++)
					{
						scale = d_lightstylevalue[surf->styles[maps]]/256.0f;

						if (cl_lightstyle[surf->styles[maps]].colour & 1)
							l[0] += *lightmap * scale;
						if (cl_lightstyle[surf->styles[maps]].colour & 2)
							l[1] += *lightmap * scale;
						if (cl_lightstyle[surf->styles[maps]].colour & 4)
							l[2] += *lightmap * scale;

						l[3] += deluxmap[0]*scale;
						l[4] += deluxmap[1]*scale;
						l[5] += deluxmap[2]*scale;

						lightmap += ((surf->extents[0]>>4)+1) *
								((surf->extents[1]>>4)+1);
						deluxmap += ((surf->extents[0]>>4)+1) *
								((surf->extents[1]>>4)+1) * 3;
					}
				}

			}
			else
			{
				if (cl.worldmodel->rgblighting)
				{
					lightmap += (dt * ((surf->extents[0]>>4)+1) + ds)*3;
					for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
							maps++)
					{
						scale = d_lightstylevalue[surf->styles[maps]]/256.0f;

						if (cl_lightstyle[surf->styles[maps]].colour & 1)
							l[0] += lightmap[0] * scale;
						if (cl_lightstyle[surf->styles[maps]].colour & 2)
							l[1] += lightmap[1] * scale;
						if (cl_lightstyle[surf->styles[maps]].colour & 4)
							l[2] += lightmap[2] * scale;

						lightmap += ((surf->extents[0]>>4)+1) *
								((surf->extents[1]>>4)+1) * 3;
					}

				}
				else
				{
					lightmap += (dt * ((surf->extents[0]>>4)+1) + ds);
					for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
							maps++)
					{
						scale = d_lightstylevalue[surf->styles[maps]]/256.0f;

						if (cl_lightstyle[surf->styles[maps]].colour & 1)
							l[0] += *lightmap * scale;
						if (cl_lightstyle[surf->styles[maps]].colour & 2)
							l[1] += *lightmap * scale;
						if (cl_lightstyle[surf->styles[maps]].colour & 4)
							l[2] += *lightmap * scale;

						lightmap += ((surf->extents[0]>>4)+1) *
								((surf->extents[1]>>4)+1);
					}
				}
			}
		}
		
		return l;
	}

// go down back side
	return GLRecursiveLightPoint3C (node->children[!side], mid, end);
}

#endif

void GLQ1BSP_LightPointValues(vec3_t point, vec3_t res_diffuse, vec3_t res_ambient, vec3_t res_dir)
{
	vec3_t		end;
	float *r;

	end[0] = point[0];
	end[1] = point[1];
	end[2] = point[2] - 2048;

	r = GLRecursiveLightPoint3C(cl.worldmodel->nodes, point, end);
	if (r == NULL)
	{
		res_diffuse[0] = 0;
		res_diffuse[1] = 0;
		res_diffuse[2] = 0;
	
		res_ambient[0] = 0;
		res_ambient[1] = 0;
		res_ambient[2] = 0;

		res_dir[0] = 0;
		res_dir[1] = 1;
		res_dir[2] = 1;
	}
	else
	{
		res_diffuse[0] = r[0];
		res_diffuse[1] = r[1];
		res_diffuse[2] = r[2];
	
		res_ambient[0] = r[0];
		res_ambient[1] = r[1];
		res_ambient[2] = r[2];

		res_dir[0] = r[3];
		res_dir[1] = r[4];
		res_dir[2] = -r[5];
		VectorNormalize(res_dir);

		if (!res_dir[0] && !res_dir[1] && !res_dir[2])
			res_dir[1] = res_dir[2] = 1;
	}
}


