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
#if defined(GLQUAKE) || defined(D3DQUAKE)
#include "glquake.h"
#include "shader.h"
#include <ctype.h>

static void R_CalcSkyChainBounds (batch_t *s);
static void GL_DrawSkyGrid (texture_t *tex);
static void GL_DrawSkySphere (batch_t *fa, shader_t *shader);
static void GL_SkyForceDepth(batch_t *fa);
static void GL_DrawSkyBox (texid_t *texnums, batch_t *s);

static float	speedscale;		// for top sky and bottom sky

extern cvar_t gl_skyboxdist;
extern cvar_t r_fastsky;
extern cvar_t r_fastskycolour;

static shader_t *forcedskyshader;
static shader_t *skyboxface;



//=========================================================

void R_SetSky(char *skyname)
{
	if (*skyname)
		forcedskyshader = R_RegisterCustom(va("skybox_%s", skyname), Shader_DefaultSkybox, NULL);
	else
		forcedskyshader = NULL;

	skyboxface = R_RegisterShader("skyboxface",
			"{\n"
				"{\n"
					"map $diffuse\n"
				"}\n"
			"}\n"
		);
}

/*
=================
GL_DrawSkyChain
=================
*/
void R_DrawSkyChain (batch_t *batch)
{
	shader_t *skyshader;
	texid_t *skyboxtex;

	if (forcedskyshader)
		skyshader = forcedskyshader;
	else
		skyshader = batch->shader;

	if (skyshader->skydome)
		skyboxtex = skyshader->skydome->farbox_textures;
	else
		skyboxtex = NULL;

	if (skyboxtex && TEXVALID(*skyboxtex))
	{
		R_CalcSkyChainBounds(batch);
		GL_DrawSkyBox (skyboxtex, batch);

		GL_SkyForceDepth(batch);
		return;
	}
#ifdef GLQUAKE
	if (*r_fastsky.string && qrenderer == QR_OPENGL && TEXVALID(batch->shader->defaulttextures.base))
	{
		R_CalcSkyChainBounds(batch);

		R_IBrokeTheArrays();
		GL_DrawSkyGrid(batch->texture);
		R_IBrokeTheArrays();

		GL_SkyForceDepth(batch);
	}
	else
#endif
	{
		GL_DrawSkySphere(batch, skyshader);
		GL_SkyForceDepth(batch);
	}
}

/*
=================================================================

  Quake 2 environment sky

=================================================================
*/

static vec3_t	skyclip[6] = {
	{1,1,0},
	{1,-1,0},
	{0,-1,1},
	{0,1,1},
	{1,0,1},
	{-1,0,1}
};

// 1 = s, 2 = t, 3 = 2048
static int	st_to_vec[6][3] =
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
static int	vec_to_st[6][3] =
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

static float	skymins[2][6], skymaxs[2][6];

static void DrawSkyPolygon (int nump, vec3_t vecs)
{
	int		i,j;
	vec3_t	v, av;
	float	s, t, dv;
	int		axis;
	float	*vp;

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
static void ClipSkyPolygon (int nump, vec3_t vecs, int stage)
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
static void R_CalcSkyChainBounds (batch_t *batch)
{
	mesh_t *mesh;

	int		i, m;
	vec3_t	verts[MAX_CLIP_VERTS];

	for (i=0 ; i<6 ; i++)
	{
		skymins[0][i] = skymins[1][i] = -1;//9999;
		skymaxs[0][i] = skymaxs[1][i] = 1;//9999;
	}

	// calculate vertex values for sky box
	for (m = batch->firstmesh; m < batch->meshes; m++)
	{
		mesh = batch->mesh[m];
		//triangulate
		for (i=2 ; i<mesh->numvertexes ; i++)
		{
			VectorSubtract (mesh->xyz_array[0], r_origin, verts[0]);
			VectorSubtract (mesh->xyz_array[i-1], r_origin, verts[1]);
			VectorSubtract (mesh->xyz_array[i], r_origin, verts[2]);
			ClipSkyPolygon (3, verts[0], 0);
		}
	}
}

#define skygridx 16
#define skygridx1 (skygridx + 1)
#define skygridxrecip (1.0f / (skygridx))
#define skygridy 16
#define skygridy1 (skygridy + 1)
#define skygridyrecip (1.0f / (skygridy))
#define skysphere_numverts (skygridx1 * skygridy1)
#define skysphere_numtriangles (skygridx * skygridy * 2)

static int skymade;
static index_t skysphere_element3i[skysphere_numtriangles * 3];
static float skysphere_texcoord2f[skysphere_numverts * 2];

static vecV_t skysphere_vertex3f[skysphere_numverts];
static mesh_t skymesh;


static void gl_skyspherecalc(int skytype)
{	//yes, this is basically stolen from DarkPlaces
	int i, j;
	index_t *e;
	float a, b, x, ax, ay, v[3], length, *texcoord2f;
	vecV_t* vertex;
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
	vertex = skysphere_vertex3f;
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
			(*vertex)[0] = v[0];
			(*vertex)[1] = v[1];
			(*vertex)[2] = v[2];
			vertex++;
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

static void GL_SkyForceDepth(batch_t *batch)
{
	if (!cls.allow_skyboxes)	//allow a little extra fps.
	{
		BE_SelectMode(BEM_DEPTHONLY);
		BE_DrawMesh_List(batch->shader, batch->meshes-batch->firstmesh, batch->mesh+batch->firstmesh, &batch->texture->vbo, &batch->shader->defaulttextures, batch->flags);
		BE_SelectMode(BEM_STANDARD);	/*skys only render in standard mode anyway, so this is safe*/
	}
}

static void R_DrawSkyMesh(batch_t *batch, mesh_t *m, shader_t *shader)
{
	static entity_t skyent;
	batch_t b;
	//float time = cl.gametime+realtime-cl.gametimemark; //warning: unused variable ‘time’

	float skydist = gl_skyboxdist.value;
	if (skydist<1)
		skydist=gl_maxdist.value * 0.577;
	if (skydist<1)
		skydist = 10000000;

	VectorCopy(r_refdef.vieworg, skyent.origin);
	skyent.axis[0][0] = skydist;
	skyent.axis[0][1] = 0;
	skyent.axis[0][2] = 0;
	skyent.axis[1][0] = 0;
	skyent.axis[1][1] = skydist;
	skyent.axis[1][2] = 0;
	skyent.axis[2][0] = 0;
	skyent.axis[2][1] = 0;
	skyent.axis[2][2] = skydist;
	skyent.scale = 1;

//FIXME: We should use the skybox clipping code and split the sphere into 6 sides.
	b = *batch;
	b.meshes = 1;
	b.firstmesh = 0;
	b.mesh = &m;
	b.ent = &skyent;
	b.shader = shader;
	b.skin = &shader->defaulttextures;
	b.texture = NULL;
	BE_SubmitBatch(&b);
}

static void GL_DrawSkySphere (batch_t *batch, shader_t *shader)
{
	//FIXME: We should use the skybox clipping code and split the sphere into 6 sides.
	gl_skyspherecalc(2);
	R_DrawSkyMesh(batch, &skymesh, shader);
}

static void GL_MakeSkyVec (float s, float t, int axis, float *vc, float *tc)
{
	vec3_t		b;
	int			j, k;

	b[0] = s;
	b[1] = t;
	b[2] = 1;

	for (j=0 ; j<3 ; j++)
	{
		k = st_to_vec[axis][j];
		if (k < 0)
			vc[j] = -b[-k - 1];
		else
			vc[j] = b[k - 1];
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

	tc[0] = s;
	tc[1] = 1.0 - t;
}


#ifdef GLQUAKE



static void EmitSkyGridVert (vec3_t v)
{
	vec3_t dir;
	float	s, t;
	float	length;

	VectorSubtract (v, r_origin, dir);
	dir[2] *= 3;	// flatten the sphere

	length = VectorLength (dir);
	length = 6*63/length;

	dir[0] *= length;
	dir[1] *= length;

	s = (speedscale + dir[0]) * (1.0/128);
	t = (speedscale + dir[1]) * (1.0/128);

	qglTexCoord2f (s, t);
	qglVertex3fv (v);
}

// s and t range from -1 to 1
static void MakeSkyGridVec2 (float s, float t, int axis, vec3_t v)
{
	vec3_t		b;
	int			j, k;

	float skydist = gl_skyboxdist.value;
	if (skydist<1)
		skydist=gl_maxdist.value * 0.577;
	if (skydist<1)
		skydist = 10000000;

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

}

#define SUBDIVISIONS	10

static void GL_DrawSkyGridFace (int axis)
{
	int i, j;
	vec3_t	vecs[4];
	float s, t;

	float fstep = 2.0 / SUBDIVISIONS;

	qglBegin (GL_QUADS);

	for (i = 0; i < SUBDIVISIONS; i++)
	{
		s = (float)(i*2 - SUBDIVISIONS) / SUBDIVISIONS;

		if (s + fstep < skymins[0][axis] || s > skymaxs[0][axis])
			continue;

		for (j = 0; j < SUBDIVISIONS; j++)
		{
			t = (float)(j*2 - SUBDIVISIONS) / SUBDIVISIONS;

			if (t + fstep < skymins[1][axis] || t > skymaxs[1][axis])
				continue;

			MakeSkyGridVec2 (s, t, axis, vecs[0]);
			MakeSkyGridVec2 (s, t + fstep, axis, vecs[1]);
			MakeSkyGridVec2 (s + fstep, t + fstep, axis, vecs[2]);
			MakeSkyGridVec2 (s + fstep, t, axis, vecs[3]);

			EmitSkyGridVert (vecs[0]);
			EmitSkyGridVert (vecs[1]);
			EmitSkyGridVert (vecs[2]);
			EmitSkyGridVert (vecs[3]);
		}
	}

	qglEnd ();
}

static void GL_DrawSkyGrid (texture_t *tex)
{
	int i;
	float time = cl.gametime+realtime-cl.gametimemark;

	GL_LazyBind(0, GL_TEXTURE_2D, tex->shader->defaulttextures.base, false);

	speedscale = time*8;
	speedscale -= (int)speedscale & ~127;

	for (i = 0; i < 6; i++)
	{
		if ((skymins[0][i] >= skymaxs[0][i]	|| skymins[1][i] >= skymaxs[1][i]))
			continue;
		GL_DrawSkyGridFace (i);
	}

	qglEnable (GL_BLEND);
	GL_LazyBind(0, GL_TEXTURE_2D, tex->shader->defaulttextures.fullbright, false);

	speedscale = time*16;
	speedscale -= (int)speedscale & ~127;

	for (i = 0; i < 6; i++)
	{
		if ((skymins[0][i] >= skymaxs[0][i]	|| skymins[1][i] >= skymaxs[1][i]))
			continue;
		GL_DrawSkyGridFace (i);
	}
	qglDisable (GL_BLEND);
}

#endif

/*
==============
R_DrawSkyBox
==============
*/
static int	skytexorder[6] = {0,2,1,3,4,5};
static void GL_DrawSkyBox (texid_t *texnums, batch_t *s)
{
	int i;

	vecV_t skyface_vertex[4];
	vec2_t skyface_texcoord[4];
	index_t skyface_index[6] = {0, 1, 2, 0, 2, 3};
	mesh_t skyfacemesh = {0};

	if (cl.skyrotate)
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

	skyfacemesh.indexes = skyface_index;
	skyfacemesh.st_array = skyface_texcoord;
	skyfacemesh.xyz_array = skyface_vertex;
	skyfacemesh.numindexes = 6;
	skyfacemesh.numvertexes = 4;

	for (i=0 ; i<6 ; i++)
	{
		if (skymins[0][i] >= skymaxs[0][i]
		|| skymins[1][i] >= skymaxs[1][i])
			continue;

		GL_MakeSkyVec (skymins[0][i], skymins[1][i], i, skyface_vertex[0], skyface_texcoord[0]);
		GL_MakeSkyVec (skymins[0][i], skymaxs[1][i], i, skyface_vertex[1], skyface_texcoord[1]);
		GL_MakeSkyVec (skymaxs[0][i], skymaxs[1][i], i, skyface_vertex[2], skyface_texcoord[2]);
		GL_MakeSkyVec (skymaxs[0][i], skymins[1][i], i, skyface_vertex[3], skyface_texcoord[3]);

		skyboxface->defaulttextures.base = texnums[skytexorder[i]];
		R_DrawSkyMesh(s, &skyfacemesh, skyboxface);
	}
}

//===============================================================

/*
=============
R_InitSky

A sky texture is 256*128, with the right side being a masked overlay
==============
*/
void R_InitSky (struct texnums_s *tn, texture_t *mt, qbyte *src)
{
	int			i, j, p;
	unsigned	trans[128*128];
	unsigned	transpix, alphamask;
	int			r, g, b;
	unsigned	*rgba;
	char name[MAX_QPATH];

	memset(tn, 0, sizeof(*tn));

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

	Q_snprintfz(name, sizeof(name), "%s_solid", mt->name);
	Q_strlwr(name);
	tn->base = R_LoadReplacementTexture(name, NULL, IF_NOALPHA);
	if (!TEXVALID(tn->base))
		tn->base = R_LoadTexture32(name, 128, 128, trans, IF_NOALPHA|IF_NOGAMMA);

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

	Q_snprintfz(name, sizeof(name), "%s_trans", mt->name);
	Q_strlwr(name);
	tn->fullbright = R_LoadReplacementTexture(name, NULL, 0);
	if (!TEXVALID(tn->fullbright))
		tn->fullbright = R_LoadTexture32(name, 128, 128, trans, IF_NOGAMMA);
}
#endif
