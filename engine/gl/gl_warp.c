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
#if defined(RGLQUAKE) || defined(D3DQUAKE)
#include "glquake.h"
#ifdef D3DQUAKE
#include "d3dquake.h"
#endif
#ifdef Q3SHADERS
#include "shader.h"
#endif
#include <ctype.h>


extern void GL_DrawAliasMesh (mesh_t *mesh, int texnum);

void GL_DrawSkySphere (msurface_t *fa);
void D3D7_DrawSkySphere (msurface_t *fa);
void D3D9_DrawSkySphere (msurface_t *fa);

extern	model_t	*loadmodel;

int		skytexturenum;

int		solidskytexture;
int		alphaskytexture;
float	speedscale;		// for top sky and bottom sky

float skyrotate;
vec3_t skyaxis;

qboolean usingskybox;

msurface_t	*warpface;

extern cvar_t r_skyboxname;
extern cvar_t gl_skyboxdist;
extern cvar_t r_fastsky;
extern cvar_t r_fastskycolour;
char defaultskybox[MAX_QPATH];

int skyboxtex[6];

void GL_DrawSkyBox (msurface_t *s);
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

//=========================================================


/*
// speed up sin calculations - Ed
float	turbsin[] =
{
	#include "gl_warp_sin.h"
};
#define TURBSCALE (256.0 / (2 * M_PI))
*/
/*
=============
EmitWaterPolys

Does a water warp on the pre-fragmented glpoly_t chain
=============
*/
#ifdef RGLQUAKE
void EmitWaterPolys (msurface_t *fa, float basealpha)
{
	float a;
	int l;
	extern cvar_t r_waterlayers;
	//the code prior to april 2005 gave a nicer result, but was incompatable with meshes and required poly lists instead
	//the new code uses vertex arrays but sacrifises the warping. We're left only with scaling.
	//The default settings still look nicer than original quake but not pre-april.
	//in the plus side, you can never see the junction glitches of the old warping. :)

#ifdef Q3SHADERS
	if (fa->texinfo->texture->shader)
	{
		meshbuffer_t mb;
		mb.sortkey = 0;
		mb.infokey = 0;
		mb.dlightbits = 0;
		mb.entity = &r_worldentity;
		mb.shader = fa->texinfo->texture->shader;
		mb.fog = NULL;
		mb.mesh = fa->mesh;
		r_worldentity.shaderRGBAf[3] = basealpha;
		R_PushMesh(mb.mesh, mb.shader->features);
		r_worldentity.shaderRGBAf[3] = 1;
		R_RenderMeshBuffer(&mb, false);
		return;
	}
#endif
	if (r_waterlayers.value>=1)
	{
		qglEnable(GL_BLEND);	//to ensure.
		qglMatrixMode(GL_TEXTURE);
		fa->mesh->colors_array=NULL;
		for (a=basealpha,l = 0; l < r_waterlayers.value; l++,a=a*4/6)
		{
			qglPushMatrix();
			qglColor4f(1, 1, 1, a);
			qglTranslatef (sin(cl.time+l*4) * 0.04f+cos(cl.time/2+l)*0.02f+cl.time/(64+l*8), cos(cl.time+l*4) * 0.06f+sin(cl.time/2+l)*0.02f+cl.time/(16+l*2), 0);
			GL_DrawAliasMesh(fa->mesh, fa->texinfo->texture->gl_texturenum);
			qglPopMatrix();
		}
		qglMatrixMode(GL_MODELVIEW);
		qglDisable(GL_BLEND);	//to ensure.
	}
	else	//dull (fast) single player
	{
		qglMatrixMode(GL_TEXTURE);
		qglPushMatrix();
		qglTranslatef (sin(cl.time) * 0.4f, cos(cl.time) * 0.06f, 0);
		fa->mesh->colors_array = NULL;
		GL_DrawAliasMesh(fa->mesh, fa->texinfo->texture->gl_texturenum);
		qglPopMatrix();
		qglMatrixMode(GL_MODELVIEW);
	}
}

/*
=============
EmitSkyPolys
=============
*/
void EmitSkyPolys (msurface_t *fa)
{

}

/*
===============
EmitBothSkyLayers

Does a sky warp on the pre-fragmented glpoly_t chain
This will be called for brushmodels, the world
will have them chained together.
===============
*/
void GL_EmitBothSkyLayers (msurface_t *fa)
{
	GL_DisableMultitexture();

	GL_Bind (solidskytexture);
	speedscale = cl.gametime*8;
	speedscale -= (int)speedscale & ~127 ;

	EmitSkyPolys (fa);

	qglEnable (GL_BLEND);
	GL_Bind (alphaskytexture);
	speedscale = cl.gametime*16;
	speedscale -= (int)speedscale & ~127 ;

	EmitSkyPolys (fa);

	qglDisable (GL_BLEND);
}

#endif

/*
=================
GL_DrawSkyChain
=================
*/
#ifdef RGLQUAKE
void R_DrawSkyBoxChain (msurface_t *s);
void GL_DrawSkyChain (msurface_t *s)
{
	msurface_t	*fa;

	GL_DisableMultitexture();
#ifdef Q3SHADERS
	if (!solidskytexture&&!usingskybox)
	{
		int i;
		if (s->texinfo->texture->shader && s->texinfo->texture->shader->skydome)
		{
			for (i = 0; i < 6; i++)
			{
				skyboxtex[i] = s->texinfo->texture->shader->skydome->farbox_textures[i];		
			}
			solidskytexture = 1;
		}
	}
#endif

	if (r_fastsky.value||(!solidskytexture&&!usingskybox))	//this is for visability only... we'd otherwise not stoop this low (and this IS low)
	{
		int fc;
		qbyte *pal;
		fc = r_fastskycolour.value;
		if (fc > 255)
			fc = 255;
		if (fc < 0)
			fc = 0;
		pal = host_basepal+fc*3;
		qglDisable(GL_TEXTURE_2D);
		qglColor3f(pal[0]/255.0f, pal[1]/255.0f, pal[2]/255.0f);
		qglDisableClientState( GL_COLOR_ARRAY );
		for (fa=s ; fa ; fa=fa->texturechain)
		{
			qglVertexPointer(3, GL_FLOAT, 0, fa->mesh->xyz_array);
			qglDrawElements(GL_TRIANGLES, fa->mesh->numindexes, GL_INDEX_TYPE, fa->mesh->indexes);
		}
		R_IBrokeTheArrays();

		qglColor3f(1, 1, 1);
		qglEnable(GL_TEXTURE_2D);
		return;
	}

	if (usingskybox)
	{
		R_DrawSkyBoxChain(s);
		return;
	}
//	if (usingskydome)
	{
		GL_DrawSkySphere(s);
		return;
	}

	// used when gl_texsort is on
	GL_Bind(solidskytexture);
	speedscale = cl.servertime;
	speedscale*=8;
	speedscale -= (int)speedscale & ~127 ;

	for (fa=s ; fa ; fa=fa->texturechain)
		EmitSkyPolys (fa);

	qglEnable (GL_BLEND);
	GL_Bind (alphaskytexture);
	speedscale = cl.servertime;
	speedscale*=16;
	speedscale -= (int)speedscale & ~127 ;

	for (fa=s ; fa ; fa=fa->texturechain)
		EmitSkyPolys (fa);

	qglDisable (GL_BLEND);
}
#endif

#ifdef D3DQUAKE
void R_DrawSkyBoxChain (msurface_t *s);
void D3D7_DrawSkyChain (msurface_t *s)
{
	//msurface_t	*fa;

#ifdef Q3SHADERS
	if (!solidskytexture&&!usingskybox)
	{
		int i;
		if (s->texinfo->texture->shader && s->texinfo->texture->shader->skydome)
		{
			for (i = 0; i < 6; i++)
			{
				skyboxtex[i] = s->texinfo->texture->shader->skydome->farbox_textures[i];		
			}
			solidskytexture = 1;
		}
	}
#endif
/*
	if (r_fastsky.value||(!solidskytexture&&!usingskybox))	//this is for visability only... we'd otherwise not stoop this low (and this IS low)
	{
		int fc;
		qbyte *pal;
		fc = r_fastskycolour.value;
		if (fc > 255)
			fc = 255;
		if (fc < 0)
			fc = 0;
		pal = host_basepal+fc*3;
		qglDisable(GL_TEXTURE_2D);
		qglColor3f(pal[0]/255.0f, pal[1]/255.0f, pal[2]/255.0f);
		qglDisableClientState( GL_COLOR_ARRAY );
		for (fa=s ; fa ; fa=fa->texturechain)
		{
			qglVertexPointer(3, GL_FLOAT, 0, fa->mesh->xyz_array);
			qglDrawElements(GL_TRIANGLES, fa->mesh->numindexes, GL_INDEX_TYPE, fa->mesh->indexes);
		}
		R_IBrokeTheArrays();

		qglColor3f(1, 1, 1);
		qglEnable(GL_TEXTURE_2D);
		return;
	}
*/
/*	if (usingskybox)
	{
		R_DrawSkyBoxChain(s);
		return;
	}
*/
	D3D7_DrawSkySphere(s);
}

void D3D9_DrawSkyChain (msurface_t *s)
{
	//msurface_t	*fa;

#ifdef Q3SHADERS
	if (!solidskytexture&&!usingskybox)
	{
		int i;
		if (s->texinfo->texture->shader && s->texinfo->texture->shader->skydome)
		{
			for (i = 0; i < 6; i++)
			{
				skyboxtex[i] = s->texinfo->texture->shader->skydome->farbox_textures[i];		
			}
			solidskytexture = 1;
		}
	}
#endif
/*
	if (r_fastsky.value||(!solidskytexture&&!usingskybox))	//this is for visability only... we'd otherwise not stoop this low (and this IS low)
	{
		int fc;
		qbyte *pal;
		fc = r_fastskycolour.value;
		if (fc > 255)
			fc = 255;
		if (fc < 0)
			fc = 0;
		pal = host_basepal+fc*3;
		qglDisable(GL_TEXTURE_2D);
		qglColor3f(pal[0]/255.0f, pal[1]/255.0f, pal[2]/255.0f);
		qglDisableClientState( GL_COLOR_ARRAY );
		for (fa=s ; fa ; fa=fa->texturechain)
		{
			qglVertexPointer(3, GL_FLOAT, 0, fa->mesh->xyz_array);
			qglDrawElements(GL_TRIANGLES, fa->mesh->numindexes, GL_INDEX_TYPE, fa->mesh->indexes);
		}
		R_IBrokeTheArrays();

		qglColor3f(1, 1, 1);
		qglEnable(GL_TEXTURE_2D);
		return;
	}
*/
/*	if (usingskybox)
	{
		R_DrawSkyBoxChain(s);
		return;
	}
*/
	D3D9_DrawSkySphere(s);
}
#endif

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
static char	*skyname_suffix[][6] = {
	{"px", "py", "nx", "ny", "pz", "nz"},
	{"posx", "posy", "negx", "negy", "posz", "negz"},
	{"rt", "bk", "lf", "ft", "up", "dn"},
	{"_px", "_py", "_nx", "_ny", "_pz", "_nz"},
	{"_posx", "_posy", "_negx", "_negy", "_posz", "_negz"},
	{"_rt", "_bk", "_lf", "_ft", "_up", "_dn"}
};

static char *skyname_pattern[] = {
	"%s_%s",
	"%s%s",
	"env/%s%s",
	"gfx/env/%s%s"
};

void GLR_LoadSkys (void)
{
	int		i;
	char	name[MAX_QPATH];
	char *boxname;
	int p, s;

	if (*r_skyboxname.string)
		boxname = r_skyboxname.string;	//user forced
	else
		boxname = defaultskybox;

	if (!*boxname)
	{	//wipe the box
		for (i=0 ; i<6 ; i++)
			skyboxtex[i] = 0;
	}
	else
	{
		for(;;)
		{
			for (i=0 ; i<6 ; i++)
			{
				for (p = 0; p < sizeof(skyname_pattern)/sizeof(skyname_pattern[0]); p++)
				{
					for (s = 0; s < sizeof(skyname_suffix)/sizeof(skyname_suffix[0]); s++)
					{
						snprintf (name, sizeof(name), skyname_pattern[p], boxname, skyname_suffix[s][i]);
						skyboxtex[i] = Mod_LoadHiResTexture(name, NULL, false, false, true);
						if (skyboxtex[i])
							break;
					}
					if (skyboxtex[i])
						break;
				}
				if (!skyboxtex[i])
					break;

#ifdef RGLQUAKE
				//so the user can specify GL_NEAREST and still get nice skyboxes... yeah, a hack
				if (qrenderer == QR_OPENGL)
				{
					qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				}
#endif
			}
			if (boxname != defaultskybox && i < 6 && *defaultskybox)
			{
				boxname = defaultskybox;
				continue;
			}
			break;
		}
	}
}


qboolean GLR_CheckSky()
{
	return true;
}

void GLR_Skyboxname_Callback(struct cvar_s *var, char *oldvalue)
{
	GLR_LoadSkys();
}

void GLR_SetSky(char *name, float rotate, vec3_t axis)	//called from the client code, once per level
{
	Q_strncpyz(defaultskybox, name, sizeof(defaultskybox));
	if (!*r_skyboxname.string)	//don't override a user's settings
		GLR_Skyboxname_Callback(&r_skyboxname, "");

	skyrotate = rotate;
	VectorCopy(axis, skyaxis);
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

	// decide which face it maps to
	VectorClear (v);
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
R_DrawSkyBoxChain
=================
*/
void R_DrawSkyBoxChain (msurface_t *s)
{
	msurface_t	*fa;

	int		i;
	vec3_t	verts[MAX_CLIP_VERTS];

	c_sky = 0;

	// calculate vertex values for sky box

	for (fa=s ; fa ; fa=fa->texturechain)
	{
		//triangulate
		for (i=2 ; i<fa->mesh->numvertexes ; i++)
		{
			VectorSubtract (fa->mesh->xyz_array[0], r_origin, verts[0]);
			VectorSubtract (fa->mesh->xyz_array[i-1], r_origin, verts[1]);
			VectorSubtract (fa->mesh->xyz_array[i], r_origin, verts[2]);
			ClipSkyPolygon (3, verts[0], 0);
		}
	}

#ifdef RGLQUAKE
	if (qrenderer == QR_OPENGL)
	{
		GL_DrawSkyBox (s);
		return;
	}
#endif
}

#define skygridx 16
#define skygridx1 (skygridx + 1)
#define skygridxrecip (1.0f / (skygridx))
#define skygridy 16
#define skygridy1 (skygridy + 1)
#define skygridyrecip (1.0f / (skygridy))
#define skysphere_numverts (skygridx1 * skygridy1)
#define skysphere_numtriangles (skygridx * skygridy * 2)

int skymade;
static index_t skysphere_element3i[skysphere_numtriangles * 3];
static float skysphere_texcoord2f[skysphere_numverts * 2];

#ifdef D3DQUAKE
static float skysphere_d3dvertex[skysphere_numverts * 5];
static d3d_animateskysphere(float time)
{
	int i;
	float *d3dvert, *texcoord2f;

	d3dvert = skysphere_d3dvertex;
	texcoord2f = skysphere_texcoord2f;
	for (i = 0; i < skysphere_numverts; i++)
	{
		d3dvert[3] = time+*texcoord2f++;
		d3dvert[4] = time+*texcoord2f++;

		d3dvert+=5;
	}
}
static void d3d_skyspherecalc(int skytype)
{	//yes, this is basically stolen from DarkPlaces
	int i, j;
	index_t *e;
	float a, b, x, ax, ay, v[3], length, *d3dvert, *texcoord2f;
	float dx, dy, dz;

	float texscale;

	if (skymade == skytype+500)
		return;

	skymade = skytype+500;

	if (skytype == 2)
		texscale = 1/16.0f;
	else
		texscale = 1/1.5f;

	texscale*=3;

	dx = 16;
	dy = 16;
	dz = 16 / 3;

	d3dvert = skysphere_d3dvertex;
	texcoord2f = skysphere_texcoord2f;

	for (j = 0;j <= skygridy;j++)
	{
		a = j * skygridyrecip;
		ax = cos(a * M_PI * 2);
		ay = -sin(a * M_PI * 2);
		for (i = 0;i <= skygridx;i++)
		{
			b = i * skygridxrecip;
			x = cos((b + 0.5) * M_PI);
			v[0] = ax*x * dx;
			v[1] = ay*x * dy;
			v[2] = -sin((b + 0.5) * M_PI) * dz;
			length = texscale / sqrt(v[0]*v[0]+v[1]*v[1]+(v[2]*v[2]*9));

			*d3dvert++ = v[0]*1000;
			*d3dvert++ = v[1]*1000;
			*d3dvert++ = v[2]*1000;

			d3dvert+=2;

			*texcoord2f++ = v[0] * length;
			*texcoord2f++ = v[1] * length;
		}
	}
	e = skysphere_element3i;
	for (j = 0;j < skygridy;j++)
	{
		for (i = 0;i < skygridx;i++)
		{
			*e++ =  j      * skygridx1 + i;
			*e++ =  j      * skygridx1 + i + 1;
			*e++ = (j + 1) * skygridx1 + i;

			*e++ =  j      * skygridx1 + i + 1;
			*e++ = (j + 1) * skygridx1 + i + 1;
			*e++ = (j + 1) * skygridx1 + i;
		}
	}
}
#endif

#ifdef RGLQUAKE
static float skysphere_vertex3f[skysphere_numverts * 3];
mesh_t skymesh;


static void gl_skyspherecalc(int skytype)
{	//yes, this is basically stolen from DarkPlaces
	int i, j;
	index_t *e;
	float a, b, x, ax, ay, v[3], length, *vertex3f, *texcoord2f;
	float dx, dy, dz;

	float texscale;

	if (skymade == skytype)
		return;

	skymade = skytype;

	if (skymade == 2)
		texscale = 1/16.0f;
	else
		texscale = 1/1.5f;

	texscale*=3;

	skymesh.indexes = skysphere_element3i;
	skymesh.st_array = (void*)skysphere_texcoord2f;
	skymesh.lmst_array = (void*)skysphere_texcoord2f;
	skymesh.xyz_array = (void*)skysphere_vertex3f;

	skymesh.numindexes = skysphere_numtriangles * 3;
	skymesh.numvertexes = skysphere_numverts;

	dx = 16;
	dy = 16;
	dz = 16 / 3;
	vertex3f = skysphere_vertex3f;
	texcoord2f = skysphere_texcoord2f;
	for (j = 0;j <= skygridy;j++)
	{
		a = j * skygridyrecip;
		ax = cos(a * M_PI * 2);
		ay = -sin(a * M_PI * 2);
		for (i = 0;i <= skygridx;i++)
		{
			b = i * skygridxrecip;
			x = cos((b + 0.5) * M_PI);
			v[0] = ax*x * dx;
			v[1] = ay*x * dy;
			v[2] = -sin((b + 0.5) * M_PI) * dz;
			length = texscale / sqrt(v[0]*v[0]+v[1]*v[1]+(v[2]*v[2]*9));
			*texcoord2f++ = v[0] * length;
			*texcoord2f++ = v[1] * length;
			*vertex3f++ = v[0];
			*vertex3f++ = v[1];
			*vertex3f++ = v[2];
		}
	}
	e = skysphere_element3i;
	for (j = 0;j < skygridy;j++)
	{
		for (i = 0;i < skygridx;i++)
		{
			*e++ =  j      * skygridx1 + i;
			*e++ =  j      * skygridx1 + i + 1;
			*e++ = (j + 1) * skygridx1 + i;

			*e++ =  j      * skygridx1 + i + 1;
			*e++ = (j + 1) * skygridx1 + i + 1;
			*e++ = (j + 1) * skygridx1 + i;
		}
	}
}

void GL_DrawSkySphere (msurface_t *fa)
{
	extern cvar_t gl_maxdist;
	float time = cl.gametime+realtime-cl.gametimemark;

	float skydist = gl_maxdist.value;
	if (skydist<1)
		skydist=gl_skyboxdist.value;
	skydist/=16;

	//scale sky sphere and place around view origin.
	qglPushMatrix();
	qglTranslatef(r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]);
	qglScalef(skydist, skydist, skydist);

//draw in bulk? this is eeevil
//FIXME: We should use the skybox clipping code and split the sphere into 6 sides.
#ifdef Q3SHADERS
	if (fa->texinfo->texture->shader)
	{	//the shader route.
		meshbuffer_t mb;
		gl_skyspherecalc(2);
		mb.sortkey = 0;
		mb.infokey = -1;
		mb.dlightbits = 0;
		mb.entity = &r_worldentity;
		mb.shader = fa->texinfo->texture->shader;
		mb.fog = NULL;
		mb.mesh = &skymesh;
		R_PushMesh(mb.mesh, mb.shader->features);
		R_RenderMeshBuffer(&mb, false);
	}
	else
#endif
	{	//the boring route.
		gl_skyspherecalc(1);
		qglMatrixMode(GL_TEXTURE);
		qglPushMatrix();
		qglTranslatef(time*8/128, time*8/128, 0);
		GL_DrawAliasMesh(&skymesh, solidskytexture);
		qglColor4f(1,1,1,0.5);
		qglEnable(GL_BLEND);
		qglTranslatef(time*8/128, time*8/128, 0);
		GL_DrawAliasMesh(&skymesh, alphaskytexture);
		qglDisable(GL_BLEND);
		qglPopMatrix();
		qglMatrixMode(GL_MODELVIEW);
	}
	qglPopMatrix();

	if (!cls.allow_skyboxes)	//allow a little extra fps.
	{//Draw the texture chain to only the depth buffer.
		if (qglColorMask)
			qglColorMask(0,0,0,0);
		for (; fa; fa = fa->texturechain)
		{
			GL_DrawAliasMesh(fa->mesh, 0);
		}
		if (qglColorMask)
			qglColorMask(1,1,1,1);
	}
}
#endif

#ifdef D3DQUAKE
void D3D7_DrawSkySphere (msurface_t *fa)
{
	extern cvar_t gl_maxdist;
	float time = cl.gametime+realtime-cl.gametimemark;

	float skydist = 99999;//gl_maxdist.value;
	if (skydist<1)
		skydist=gl_skyboxdist.value;
	skydist/=16;

	//scale sky sphere and place around view origin.
//	qglPushMatrix();
//	qglTranslatef(r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]);
//	qglScalef(skydist, skydist, skydist);

//draw in bulk? this is eeevil
//FIXME: We should use the skybox clipping code and split the sphere into 6 sides.
/*#ifdef Q3SHADERS
	if (fa->texinfo->texture->shader)
	{	//the shader route.
		meshbuffer_t mb;
		d3d_skyspherecalc(2);
		mb.sortkey = 0;
		mb.infokey = -1;
		mb.dlightbits = 0;
		mb.entity = &r_worldentity;
		mb.shader = fa->texinfo->texture->shader;
		mb.fog = NULL;
		mb.mesh = &skymesh;
		R_PushMesh(mb.mesh, mb.shader->features);
		R_RenderMeshBuffer(&mb, false);
	}
	else
#endif*/
	{	//the boring route.
		d3d_skyspherecalc(1);
//		qglMatrixMode(GL_TEXTURE);
//		qglPushMatrix();
//		qglTranslatef(time*8/128, time*8/128, 0);

		pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ALPHATESTENABLE, FALSE );

		pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
//		pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_COLORARG2, D3DTA_CURRENT);
		pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);

		pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
//		pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
		pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);

		pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_SRCBLEND,  D3DBLEND_SRCALPHA);
		pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_DESTBLEND,  D3DBLEND_INVSRCALPHA);

		d3d_animateskysphere(time*8/128);
		pD3DDev->lpVtbl->SetTexture(pD3DDev, 0, (LPDIRECTDRAWSURFACE7)solidskytexture);
		pD3DDev->lpVtbl->DrawIndexedPrimitive(pD3DDev, D3DPT_TRIANGLELIST, D3DFVF_XYZ|D3DFVF_TEX1, skysphere_d3dvertex, skysphere_numverts, skysphere_element3i, skysphere_numtriangles * 3, 0);

		pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ALPHABLENDENABLE, TRUE);
//		pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ALPHATESTENABLE, TRUE);

		pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_COLORARG2, D3DTA_CURRENT);
		pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_COLOROP, D3DTOP_MODULATE);

		pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
//		pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
		pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);

//		qglTranslatef(time*8/128, time*8/128, 0);
		d3d_animateskysphere(time*16/128);
		pD3DDev->lpVtbl->SetTexture(pD3DDev, 0, (LPDIRECTDRAWSURFACE7)alphaskytexture);
		pD3DDev->lpVtbl->DrawIndexedPrimitive(pD3DDev, D3DPT_TRIANGLELIST, D3DFVF_XYZ|D3DFVF_TEX1, skysphere_d3dvertex, skysphere_numverts, skysphere_element3i, skysphere_numtriangles * 3, 0);

		pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ALPHABLENDENABLE, FALSE);
//		qglDisable(GL_BLEND);
//		qglPopMatrix();
//		qglMatrixMode(GL_MODELVIEW);
	}
//	qglPopMatrix();

/*
	if (!cls.allow_skyboxes)	//allow a little extra fps.
	{//Draw the texture chain to only the depth buffer.
		if (qglColorMask)
			qglColorMask(0,0,0,0);
		for (; fa; fa = fa->texturechain)
		{
			GL_DrawAliasMesh(fa->mesh, 0);
		}
		if (qglColorMask)
			qglColorMask(1,1,1,1);
	}
	*/
}
void D3D9_DrawSkySphere (msurface_t *fa)
{
	extern cvar_t gl_maxdist;
	float time = cl.gametime+realtime-cl.gametimemark;

	float skydist = 99999;//gl_maxdist.value;
	if (skydist<1)
		skydist=gl_skyboxdist.value;
	skydist/=16;

	//scale sky sphere and place around view origin.
//	qglPushMatrix();
//	qglTranslatef(r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]);
//	qglScalef(skydist, skydist, skydist);

//draw in bulk? this is eeevil
//FIXME: We should use the skybox clipping code and split the sphere into 6 sides.
/*#ifdef Q3SHADERS
	if (fa->texinfo->texture->shader)
	{	//the shader route.
		meshbuffer_t mb;
		d3d_skyspherecalc(2);
		mb.sortkey = 0;
		mb.infokey = -1;
		mb.dlightbits = 0;
		mb.entity = &r_worldentity;
		mb.shader = fa->texinfo->texture->shader;
		mb.fog = NULL;
		mb.mesh = &skymesh;
		R_PushMesh(mb.mesh, mb.shader->features);
		R_RenderMeshBuffer(&mb, false);
	}
	else
#endif*/
	{	//the boring route.
		d3d_skyspherecalc(1);
//		qglMatrixMode(GL_TEXTURE);
//		qglPushMatrix();
//		qglTranslatef(time*8/128, time*8/128, 0);

		d3d_animateskysphere(time*8/128);
		D3D9_DrawSkyMesh(0, solidskytexture, skysphere_d3dvertex, skysphere_numverts, skysphere_element3i, skysphere_numtriangles);
		d3d_animateskysphere(time*16/128);
		D3D9_DrawSkyMesh(1, alphaskytexture, skysphere_d3dvertex, skysphere_numverts, skysphere_element3i, skysphere_numtriangles);

//		qglDisable(GL_BLEND);
//		qglPopMatrix();
//		qglMatrixMode(GL_MODELVIEW);
	}
//	qglPopMatrix();

/*
	if (!cls.allow_skyboxes)	//allow a little extra fps.
	{//Draw the texture chain to only the depth buffer.
		if (qglColorMask)
			qglColorMask(0,0,0,0);
		for (; fa; fa = fa->texturechain)
		{
			GL_DrawAliasMesh(fa->mesh, 0);
		}
		if (qglColorMask)
			qglColorMask(1,1,1,1);
	}
	*/
}
#endif

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

void R_ForceSkyBox (void)
{
	int		i;

	for (i=0 ; i<6 ; i++)
	{
		skymins[0][i] = skymins[1][i] = -1;
		skymaxs[0][i] = skymaxs[1][i] = 1;
	}
}

#ifdef RGLQUAKE
void GL_MakeSkyVec (float s, float t, int axis)
{
	vec3_t		v, b;
	int			j, k;
	float skydist = gl_skyboxdist.value;
	extern cvar_t gl_maxdist;

	if (r_shadows.value || !gl_maxdist.value)	//because r_shadows comes with an infinate depth perspective.
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
	qglTexCoord2f (s, t);
	qglVertex3fv (v);
}
#endif

/*
==============
R_DrawSkyBox
==============
*/
int	skytexorder[6] = {0,2,1,3,4,5};
#ifdef RGLQUAKE
void GL_DrawSkyBox (msurface_t *s)
{
	msurface_t *fa;
	int i;

	if (!usingskybox)
		return;

	if (skyrotate)
	{
		for (i=0 ; i<6 ; i++)
		{
			if (skymins[0][i] < skymaxs[0][i]
				&& skymins[1][i] < skymaxs[1][i])
					break;

			skymins[0][i] = -1;	//fully visible
			skymins[1][i] = -1;
			skymaxs[0][i] = 1;
			skymaxs[1][i] = 1;
		}
		if (i == 6)
			return;	//can't see anything
		for ( ; i<6 ; i++)
		{
			skymins[0][i] = -1;
			skymins[1][i] = -1;
			skymaxs[0][i] = 1;
			skymaxs[1][i] = 1;
		}
	}

	qglPushMatrix ();	
	qglTranslatef (r_origin[0], r_origin[1], r_origin[2]);
	if (skyrotate)
		qglRotatef (cl.time * skyrotate, skyaxis[0], skyaxis[1], skyaxis[2]);

	for (i=0 ; i<6 ; i++)
	{
		if (skymins[0][i] >= skymaxs[0][i]
		|| skymins[1][i] >= skymaxs[1][i])
			continue;

		GL_Bind (skyboxtex[skytexorder[i]]);

		qglBegin (GL_QUADS);
		GL_MakeSkyVec (skymins[0][i], skymins[1][i], i);
		GL_MakeSkyVec (skymins[0][i], skymaxs[1][i], i);
		GL_MakeSkyVec (skymaxs[0][i], skymaxs[1][i], i);
		GL_MakeSkyVec (skymaxs[0][i], skymins[1][i], i);
		qglEnd ();
	}
	
	qglPopMatrix ();

	if (!cls.allow_skyboxes && s)	//allow a little extra fps.
	{
		//write the depth correctly
		if (qglColorMask)
			qglColorMask(0, 0, 0, 0);	//depth only.
		for (fa = s; fa; fa = fa->texturechain)
			GL_DrawAliasMesh(fa->mesh, 1);

		if (qglColorMask)
			qglColorMask(1, 1, 1, 1);
	}
}
#endif



//===============================================================

/*
=============
R_InitSky

A sky texture is 256*128, with the right side being a masked overlay
==============
*/
void R_InitSky (texture_t *mt)
{
	int			i, j, p;
	qbyte		*src;
	unsigned	trans[128*128];
	unsigned	transpix, alphamask;
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
	Q_strlwr(name);
	solidskytexture = Mod_LoadReplacementTexture(name, NULL, true, false, true);
	if (!solidskytexture)
		solidskytexture = R_LoadTexture32(name, 128, 128, trans, true, false);
/*
	if (!solidskytexture)
		solidskytexture = texture_extension_number++;

	GL_Bind (solidskytexture );
	glTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
*/
	alphamask = LittleLong(0x7fffffff);
	for (i=0 ; i<128 ; i++)
		for (j=0 ; j<128 ; j++)
		{
			p = src[i*256 + j];
			if (p == 0)
				trans[(i*128) + j] = transpix;
			else
				trans[(i*128) + j] = d_8to24rgbtable[p] & alphamask;
		}

	sprintf(name, "%s_trans", mt->name);
	Q_strlwr(name);
	alphaskytexture = Mod_LoadReplacementTexture(name, NULL, true, true, true);
	if (!alphaskytexture)
		alphaskytexture = R_LoadTexture32(name, 128, 128, trans, true, true);
/*
	if (!alphaskytexture)
		alphaskytexture = texture_extension_number++;
	GL_Bind(alphaskytexture);
	glTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
*/
}
#endif
