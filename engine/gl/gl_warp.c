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
// gl_warp.c -- sky and water polygons

#include "quakedef.h"
#include "glquake.h"

extern	model_t	*loadmodel;

int		skytexturenum;

int		solidskytexture;
int		alphaskytexture;
float	speedscale;		// for top sky and bottom sky

qboolean usingskybox;

msurface_t	*warpface;

extern cvar_t gl_skyboxname;
extern cvar_t gl_waterripples;
extern cvar_t gl_skyboxdist;
extern cvar_t r_fastsky;
extern cvar_t r_fastskycolour;
char loadedskybox[256];

void R_DrawSkyBox (msurface_t *s);
void BoundPoly (int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	int		i, j;
	float	*v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for (i=0 ; i<numverts ; i++)
		for (j=0 ; j<3 ; j++, v++)
		{
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
}

void SubdividePolygon (int numverts, float *verts, float dividesize)
{
	int		i, j, k;
	vec3_t	mins, maxs;
	float	m;
	float	*v;
	vec3_t	front[64], back[64];
	int		f, b;
	float	dist[64];
	float	frac;
	glpoly_t	*poly;
	float	s, t;

	if (numverts > 60 || numverts <= 0)
		Sys_Error ("numverts = %i", numverts);

	BoundPoly (numverts, verts, mins, maxs);

	for (i=0 ; i<3 ; i++)
	{
		m = (mins[i] + maxs[i]) * 0.5;
		m = dividesize * floor (m/dividesize + 0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		v = verts + i;
		for (j=0 ; j<numverts ; j++, v+= 3)
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v-=i;
		VectorCopy (verts, v);

		f = b = 0;
		v = verts;
		for (j=0 ; j<numverts ; j++, v+= 3)
		{
			if (dist[j] >= 0)
			{
				VectorCopy (v, front[f]);
				f++;
			}
			if (dist[j] <= 0)
			{
				VectorCopy (v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j+1] == 0)
				continue;
			if ( (dist[j] > 0) != (dist[j+1] > 0) )
			{
				// clip point
				frac = dist[j] / (dist[j] - dist[j+1]);
				for (k=0 ; k<3 ; k++)
					front[f][k] = back[b][k] = v[k] + frac*(v[3+k] - v[k]);
				f++;
				b++;
			}
		}

		SubdividePolygon (f, front[0], dividesize);
		SubdividePolygon (b, back[0], dividesize);
		return;
	}

	poly = Hunk_AllocName (sizeof(glpoly_t) + (numverts-4) * VERTEXSIZE*sizeof(float), "subpoly");
	poly->next = warpface->polys;
	warpface->polys = poly;
	poly->numverts = numverts;
	for (i=0 ; i<numverts ; i++, verts+= 3)
	{
		VectorCopy (verts, poly->verts[i]);
		s = DotProduct (verts, warpface->texinfo->vecs[0]);
		t = DotProduct (verts, warpface->texinfo->vecs[1]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;
	}
}

/*
================
GL_SubdivideSurface

Breaks a polygon up along axial 64 unit
boundaries so that turbulent and sky warps
can be done reasonably.
================
*/
void GL_SubdivideSurface (msurface_t *fa, float dividesize)
{
	vec3_t		verts[64];
	int			numverts;
	int			i;
	int			lindex;
	float		*vec;

	warpface = fa;

	//
	// convert edges back to a normal polygon
	//
	numverts = 0;
	for (i=0 ; i<fa->numedges ; i++)
	{
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy (vec, verts[numverts]);
		numverts++;

		if(numverts >= 64)
		{
			Con_Printf("Too many verts on surface\n");
			break;
		}
	}

	SubdividePolygon (numverts, verts[0], dividesize);
}

//=========================================================



// speed up sin calculations - Ed
float	turbsin[] =
{
	#include "gl_warp_sin.h"
};
#define TURBSCALE (256.0 / (2 * M_PI))

/*
=============
EmitWaterPolys

Does a water warp on the pre-fragmented glpoly_t chain
=============
*/
void EmitWaterPolys (msurface_t *fa, float basealpha)
{
	glpoly_t	*p;
	float		*v;
	int			i;
	float		s, t, os, ot;

#ifdef WATERLAYERS
	extern cvar_t r_waterlayers;


	if (gl_waterripples.value)
	{
		float f = 10;

		glEnable(GL_AUTO_NORMAL);
		for (p=fa->polys ; p ; p=p->next)
		{
			glBegin (GL_POLYGON);
			for (i=0,v=p->verts[0] ; i<p->numverts ; i++, v+=VERTEXSIZE)
			{
				os = v[3];
				ot = v[4];

				s = os + turbsin[(int)((ot*0.125+realtime) * TURBSCALE) & 255];
				s *= (1.0/64);

				t = ot + turbsin[(int)((os*0.125+realtime) * TURBSCALE) & 255];
				t *= (1.0/64);

				glNormal3f(fa->plane->normal[0] + (sin(realtime+v[0]/f+v[1]/f))/4, fa->plane->normal[1] +(sin(realtime-v[1]/f))/4, fa->plane->normal[2] + (sin(realtime+v[2]/f))/4);
				glTexCoord2f (s, t);
				glVertex3fv (v);
			}
			glEnd ();
		}
		glDisable(GL_AUTO_NORMAL);
	}
	else if (r_waterlayers.value>=1)
	{
		float a, stm, ttm;
		int l;
		glDisable(GL_ALPHA_TEST);
		glEnable(GL_BLEND);	//to ensure.
		for (a=basealpha,l = 0; l < r_waterlayers.value; l++,a=a*4/6)
		{
			glColor4f(1, 1, 1, a);
			stm =cos(l)/10;
			ttm =sin(l)/10;
			for (p=fa->polys ; p ; p=p->next)
			{		
				glBegin (GL_POLYGON);
				for (i=0,v=p->verts[0] ; i<p->numverts ; i++, v+=VERTEXSIZE)
				{
					os = v[3]/(l*0.5+0.2);
					ot = v[4]/(l*0.5+0.2);

					s = os + turbsin[(int)((ot*0.125+cl.time+l*8) * TURBSCALE) & 255];//*r_watersurfacewarp.value;
					s *= (1.0/64);

					t = ot + turbsin[(int)((os*0.125+cl.time+l*8) * TURBSCALE) & 255];//*r_watersurfacewarp.value;
					t *= (1.0/64);

					glTexCoord2f (s+cl.time*stm, t+cl.time*ttm);
					glVertex3fv (v);
				}
				glEnd ();
			}
		}

	}
	else
	{
#endif
		for (p=fa->polys ; p ; p=p->next)
		{
			glBegin (GL_POLYGON);
			for (i=0,v=p->verts[0] ; i<p->numverts ; i++, v+=VERTEXSIZE)
			{
				os = v[3];
				ot = v[4];

				s = os + turbsin[(int)((ot*0.125+realtime) * TURBSCALE) & 255];
				s *= (1.0/64);

				t = ot + turbsin[(int)((os*0.125+realtime) * TURBSCALE) & 255];
				t *= (1.0/64);

				glTexCoord2f (s, t);
				glVertex3fv (v);
			}
			glEnd ();
		}
#ifdef WATERLAYERS
	}
#endif
}




/*
=============
EmitSkyPolys
=============
*/
void EmitSkyPolys (msurface_t *fa)
{
	glpoly_t	*p;
	float		*v;
	int			i;
	float	s, t;
	vec3_t	dir;
	float	length;

	for (p=fa->polys ; p ; p=p->next)
	{
		glBegin (GL_POLYGON);
		for (i=0,v=p->verts[0] ; i<p->numverts ; i++, v+=VERTEXSIZE)
		{
			VectorSubtract (v, r_origin, dir);
			dir[2] *= 3;	// flatten the sphere

			length = dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2];
			length = sqrt (length);
			length = 6*63/length;

			dir[0] *= length;
			dir[1] *= length;

			s = (speedscale + dir[0]) * (1.0/128);
			t = (speedscale + dir[1]) * (1.0/128);

			glTexCoord2f (s, t);
			glVertex3fv (v);
		}
		glEnd ();
	}
}

/*
===============
EmitBothSkyLayers

Does a sky warp on the pre-fragmented glpoly_t chain
This will be called for brushmodels, the world
will have them chained together.
===============
*/
void EmitBothSkyLayers (msurface_t *fa)
{
	GL_DisableMultitexture();

	GL_Bind (solidskytexture);
	speedscale = cl.gametime*8;
	speedscale -= (int)speedscale & ~127 ;

	EmitSkyPolys (fa);

	glEnable (GL_BLEND);
	GL_Bind (alphaskytexture);
	speedscale = cl.gametime*16;
	speedscale -= (int)speedscale & ~127 ;

	EmitSkyPolys (fa);

	glDisable (GL_BLEND);
}

/*
=================
R_DrawSkyChain
=================
*/
void R_DrawSkyBoxChain (msurface_t *s);
void R_DrawSkyChain (msurface_t *s)
{
	msurface_t	*fa;

	GL_DisableMultitexture();

	if (r_fastsky.value)	//this is for visability only... we'd otherwise not stoop this low (and this IS low)
	{
		int fc;
		qbyte *pal;
		extern unsigned char	vid_curpal[256*3];
		fc = r_fastskycolour.value;
		if (fc > 255)
			fc = 255;
		if (fc < 0)
			fc = 0;
		pal = vid_curpal+fc*3;
		glDisable(GL_TEXTURE_2D);
		glColor3f(pal[0]/255.0f, pal[1]/255.0f, pal[2]/255.0f);
		for (fa=s ; fa ; fa=fa->texturechain)
			EmitSkyPolys (fa);

		glColor3f(1, 1, 1);
		glEnable(GL_TEXTURE_2D);
		return;
	}

	if (usingskybox)
	{
		R_DrawSkyBoxChain(s);
		return;
	}

	// used when gl_texsort is on
	GL_Bind(solidskytexture);
	speedscale = cl.gametime;
	speedscale += realtime - cl.gametimemark;
	speedscale*=8;
	speedscale -= (int)speedscale & ~127 ;

	for (fa=s ; fa ; fa=fa->texturechain)
		EmitSkyPolys (fa);

	glEnable (GL_BLEND);
	GL_Bind (alphaskytexture);
	speedscale = cl.gametime;
	speedscale += realtime - cl.gametimemark;
	speedscale*=16;
	speedscale -= (int)speedscale & ~127 ;

	for (fa=s ; fa ; fa=fa->texturechain)
		EmitSkyPolys (fa);

	glDisable (GL_BLEND);
}

/*
=================================================================

  Quake 2 environment sky

=================================================================
*/


/*
==================
R_LoadSkys
==================
*/
static char	*suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
int skyboxtex[6];
void R_LoadSkys (void)
{
	int		i;
	char	name[64];

	for (i=0 ; i<6 ; i++)
	{		
		sprintf (name, "env/%s%s.tga", gl_skyboxname.string, suf[i]);

		skyboxtex[i] = Mod_LoadHiResTexture(name, false, false, true);

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	Q_strncpyz(loadedskybox, gl_skyboxname.string, sizeof(loadedskybox));
	gl_skyboxname.modified = false;
}


qboolean GLR_CheckSky()
{
	return true;
}
void GLR_SetSky(char *name, float rotate, vec3_t axis)
{
	if (*name)
		Cvar_Set(&gl_skyboxname, name);
}

vec3_t	skyclip[6] = {
	{1,1,0},
	{1,-1,0},
	{0,-1,1},
	{0,1,1},
	{1,0,1},
	{-1,0,1} 
};
int	c_sky;

// 1 = s, 2 = t, 3 = 2048
int	st_to_vec[6][3] =
{
	{3,-1,2},
	{-3,1,2},

	{1,3,2},
	{-1,-3,2},

	{-2,-1,3},		// 0 degrees yaw, look straight up
	{2,-1,-3}		// look straight down

//	{-1,2,3},
//	{1,2,-3}
};

// s = [0]/[2], t = [1]/[2]
int	vec_to_st[6][3] =
{
	{-2,3,1},
	{2,3,-1},

	{1,3,2},
	{-1,3,-2},

	{-2,-1,3},
	{-2,1,-3}

//	{-1,2,3},
//	{1,2,-3}
};

float	skymins[2][6], skymaxs[2][6];

void DrawSkyPolygon (int nump, vec3_t vecs)
{
	int		i,j;
	vec3_t	v, av;
	float	s, t, dv;
	int		axis;
	float	*vp;

	c_sky++;
#if 0
glBegin (GL_POLYGON);
for (i=0 ; i<nump ; i++, vecs+=3)
{
	VectorAdd(vecs, r_origin, v);
	glVertex3fv (v);
}
glEnd();
return;
#endif
	// decide which face it maps to
	VectorCopy (vec3_origin, v);
	for (i=0, vp=vecs ; i<nump ; i++, vp+=3)
	{
		VectorAdd (vp, v, v);
	}
	av[0] = fabs(v[0]);
	av[1] = fabs(v[1]);
	av[2] = fabs(v[2]);
	if (av[0] > av[1] && av[0] > av[2])
	{
		if (v[0] < 0)
			axis = 1;
		else
			axis = 0;
	}
	else if (av[1] > av[2] && av[1] > av[0])
	{
		if (v[1] < 0)
			axis = 3;
		else
			axis = 2;
	}
	else
	{
		if (v[2] < 0)
			axis = 5;
		else
			axis = 4;
	}

	// project new texture coords
	for (i=0 ; i<nump ; i++, vecs+=3)
	{
		j = vec_to_st[axis][2];
		if (j > 0)
			dv = vecs[j - 1];
		else
			dv = -vecs[-j - 1];

		if (dv < 0.001)
			continue;	// don't divide by zero

		j = vec_to_st[axis][0];
		if (j < 0)
			s = -vecs[-j -1] / dv;
		else
			s = vecs[j-1] / dv;
		j = vec_to_st[axis][1];
		if (j < 0)
			t = -vecs[-j -1] / dv;
		else
			t = vecs[j-1] / dv;

		if (s < skymins[0][axis])
			skymins[0][axis] = s;
		if (t < skymins[1][axis])
			skymins[1][axis] = t;
		if (s > skymaxs[0][axis])
			skymaxs[0][axis] = s;
		if (t > skymaxs[1][axis])
			skymaxs[1][axis] = t;
	}
}

#define	MAX_CLIP_VERTS	64
void ClipSkyPolygon (int nump, vec3_t vecs, int stage)
{
	float	*norm;
	float	*v;
	qboolean	front, back;
	float	d, e;
	float	dists[MAX_CLIP_VERTS];
	int		sides[MAX_CLIP_VERTS];
	vec3_t	newv[2][MAX_CLIP_VERTS];
	int		newc[2];
	int		i, j;

	if (nump > MAX_CLIP_VERTS-2)
		Sys_Error ("ClipSkyPolygon: MAX_CLIP_VERTS");
	if (stage == 6)
	{	// fully clipped, so draw it
		DrawSkyPolygon (nump, vecs);
		return;
	}

	front = back = false;
	norm = skyclip[stage];
	for (i=0, v = vecs ; i<nump ; i++, v+=3)
	{
		d = DotProduct (v, norm);
		if (d > ON_EPSILON)
		{
			front = true;
			sides[i] = SIDE_FRONT;
		}
		else if (d < -ON_EPSILON)
		{
			back = true;
			sides[i] = SIDE_BACK;
		}
		else
			sides[i] = SIDE_ON;
		dists[i] = d;
	}

	if (!front || !back)
	{	// not clipped
		ClipSkyPolygon (nump, vecs, stage+1);
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy (vecs, (vecs+(i*3)) );
	newc[0] = newc[1] = 0;

	for (i=0, v = vecs ; i<nump ; i++, v+=3)
	{
		switch (sides[i])
		{
		case SIDE_FRONT:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			break;
		case SIDE_BACK:
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		case SIDE_ON:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		}

		if (sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		d = dists[i] / (dists[i] - dists[i+1]);
		for (j=0 ; j<3 ; j++)
		{
			e = v[j] + d*(v[j+3] - v[j]);
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// continue
	ClipSkyPolygon (newc[0], newv[0][0], stage+1);
	ClipSkyPolygon (newc[1], newv[1][0], stage+1);
}

/*
=================
R_DrawSkyChain
=================
*/
void R_DrawSkyBoxChain (msurface_t *s)
{
	msurface_t	*fa;

	int		i;
	vec3_t	verts[MAX_CLIP_VERTS];
	glpoly_t	*p;

	c_sky = 0;
//	GL_Bind(solidskytexture);

	// calculate vertex values for sky box

	for (fa=s ; fa ; fa=fa->texturechain)
	{
		for (p=fa->polys ; p ; p=p->next)
		{
			for (i=0 ; i<p->numverts ; i++)
			{
				VectorSubtract (p->verts[i], r_origin, verts[i]);
			}
			ClipSkyPolygon (p->numverts, verts[0], 0);
		}
	}

	R_DrawSkyBox (s);

	glColorMask(0, 0, 0, 0);
	for (fa=s ; fa ; fa=fa->texturechain)
	{
		for (p=fa->polys ; p ; p=p->next)
		{
			glBegin(GL_POLYGON);
			for (i = 0; i < p->numverts; i++)
				glVertex3fv(p->verts[i]);
			glEnd();
		}
	}
	glColorMask(1, 1, 1, 1);
}

void R_AddSkySurface (msurface_t *fa)
{
	int			i;
	vec3_t		verts[MAX_CLIP_VERTS];
	glpoly_t	*p;

	// calculate vertex values for sky box
	for (p=fa->polys ; p ; p=p->next)
	{
		for (i=0 ; i<p->numverts ; i++)
		{
			VectorSubtract (p->verts[i], r_origin, verts[i]);
		}
		ClipSkyPolygon (p->numverts, verts[0], 0);
	}
}


/*
==============
R_ClearSkyBox
==============
*/
void R_ClearSkyBox (void)
{
	int		i;

	if (!cl.worldmodel)	//allow skyboxes on non quake1 maps. (expect them even)
	{
		usingskybox = false;		
		return;
	}
	if (gl_skyboxname.modified)
		R_LoadSkys();

	if (!skyboxtex[0] || !skyboxtex[1] || !skyboxtex[2] || !skyboxtex[3] || !skyboxtex[4] || !skyboxtex[5])
	{
		usingskybox = false;
		return;
	}

	usingskybox = true;

	for (i=0 ; i<6 ; i++)
	{
		skymins[0][i] = skymins[1][i] = 9999;
		skymaxs[0][i] = skymaxs[1][i] = -9999;
	}
}


void MakeSkyVec (float s, float t, int axis)
{
	vec3_t		v, b;
	int			j, k;
	float skydist = gl_skyboxdist.value;

	if (r_shadows.value)	//because r_shadows comes with an infinate depth perspective.
		skydist*=20;		//so we can put the distance at whatever distance needed.

	b[0] = s*skydist;
	b[1] = t*skydist;
	b[2] = skydist;

	for (j=0 ; j<3 ; j++)
	{
		k = st_to_vec[axis][j];
		if (k < 0)
			v[j] = -b[-k - 1];
		else
			v[j] = b[k - 1];
		v[j] += r_origin[j];
	}

	// avoid bilerp seam
	s = (s+1)*0.5;
	t = (t+1)*0.5;

	if (s < 1.0/512)
		s = 1.0/512;
	else if (s > 511.0/512)
		s = 511.0/512;
	if (t < 1.0/512)
		t = 1.0/512;
	else if (t > 511.0/512)
		t = 511.0/512;

	t = 1.0 - t;
	glTexCoord2f (s, t);
	glVertex3fv (v);
}

/*
==============
R_DrawSkyBox
==============
*/
int	skytexorder[6] = {0,2,1,3,4,5};
void R_DrawSkyBox (msurface_t *s)
{
	msurface_t *fa;
	glpoly_t *poly;
	int i;

	if (!usingskybox)
		return;

#if 0
glEnable (GL_BLEND);
glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
glColor4f (1,1,1,0.5);
glDisable (GL_DEPTH_TEST);
#endif
	for (i=0 ; i<6 ; i++)
	{
		if (skymins[0][i] >= skymaxs[0][i]
		|| skymins[1][i] >= skymaxs[1][i])
			continue;

		GL_Bind (skyboxtex[skytexorder[i]]);
#if 0
skymins[0][i] = -1;
skymins[1][i] = -1;
skymaxs[0][i] = 1;
skymaxs[1][i] = 1;
#endif
		glBegin (GL_QUADS);
		MakeSkyVec (skymins[0][i], skymins[1][i], i);
		MakeSkyVec (skymins[0][i], skymaxs[1][i], i);
		MakeSkyVec (skymaxs[0][i], skymaxs[1][i], i);
		MakeSkyVec (skymaxs[0][i], skymins[1][i], i);
		glEnd ();
	}
#if 0
glDisable (GL_BLEND);
glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
glColor4f (1,1,1,0.5);
glEnable (GL_DEPTH_TEST);
#endif

	if (!cls.allow_skyboxes && s)	//allow a little extra fps.
	{
		glColorMask(0, 0, 0, 0);	//depth only.
		for (fa = s; fa; fa = fa->texturechain)
		{
			for (poly = fa->polys; poly; poly = poly->next)
			{
				glBegin (GL_POLYGON);
				for (i = 0; i < poly->numverts; i++)
					glVertex3fv (&poly->verts[0][0]+i*VERTEXSIZE);
				glEnd ();
			}
		}
		glColorMask(1, 1, 1, 1);
	}
}



//===============================================================

/*
=============
R_InitSky

A sky texture is 256*128, with the right side being a masked overlay
==============
*/
void GLR_InitSky (texture_t *mt)
{
	int			i, j, p;
	qbyte		*src;
	unsigned	trans[128*128];
	unsigned	transpix;
	int			r, g, b;
	unsigned	*rgba;
	char name[MAX_QPATH];

	src = (qbyte *)mt + mt->offsets[0];

	// make an average value for the back to avoid
	// a fringe on the top level

	r = g = b = 0;
	for (i=0 ; i<128 ; i++)
		for (j=0 ; j<128 ; j++)
		{
			p = src[i*256 + j + 128];
			rgba = &d_8to24rgbtable[p];
			trans[(i*128) + j] = *rgba;
			r += ((qbyte *)rgba)[0];
			g += ((qbyte *)rgba)[1];
			b += ((qbyte *)rgba)[2];
		}

	((qbyte *)&transpix)[0] = r/(128*128);
	((qbyte *)&transpix)[1] = g/(128*128);
	((qbyte *)&transpix)[2] = b/(128*128);
	((qbyte *)&transpix)[3] = 0;

	sprintf(name, "%s_solid", mt->name);
	strlwr(name);
	solidskytexture = Mod_LoadReplacementTexture(name, true, false, true);
	if (!solidskytexture)
		solidskytexture = GL_LoadTexture32(name, 128, 128, trans, true, false);
/*
	if (!solidskytexture)
		solidskytexture = texture_extension_number++;

	GL_Bind (solidskytexture );
	glTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
*/

	for (i=0 ; i<128 ; i++)
		for (j=0 ; j<128 ; j++)
		{
			p = src[i*256 + j];
			if (p == 0)
				trans[(i*128) + j] = transpix;
			else
				trans[(i*128) + j] = d_8to24rgbtable[p];
		}

	sprintf(name, "%s_trans", mt->name);
	strlwr(name);
	alphaskytexture = Mod_LoadReplacementTexture(name, true, true, true);
	if (!alphaskytexture)
		alphaskytexture = GL_LoadTexture32(name, 128, 128, trans, true, true);
/*
	if (!alphaskytexture)
		alphaskytexture = texture_extension_number++;
	GL_Bind(alphaskytexture);
	glTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
*/
}
