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
#ifndef SERVERONLY
#include "glquake.h"
#include "shader.h"
#include <ctype.h>

static void R_CalcSkyChainBounds (batch_t *s);
static void GL_DrawSkySphere (batch_t *fa, shader_t *shader);
static void GL_SkyForceDepth(batch_t *fa);
static void GL_DrawSkyBox (texid_t *texnums, batch_t *s);

static void GL_DrawSkyGrid (texnums_t *tex);

extern cvar_t gl_skyboxdist;
extern cvar_t r_fastsky;
extern cvar_t r_fastskycolour;

static shader_t *forcedskyshader;
static shader_t *skyboxface;
static shader_t *skygridface;



//=========================================================

void R_SetSky(char *skyname)
{
	if (*skyname)
		forcedskyshader = R_RegisterCustom(va("skybox_%s", skyname), SUF_NONE, Shader_DefaultSkybox, NULL);
	else
		forcedskyshader = NULL;

	skyboxface = R_RegisterShader("skyboxface", SUF_NONE,
			"{\n"
				"program default2d\n"
				"{\n"
					"map $diffuse\n"
					"nodepth\n"	//don't write depth. this stuff is meant to be an infiniteish distance away.
				"}\n"
			"}\n"
		);

	skygridface = R_RegisterShader("skygridface", SUF_NONE,
			"{\n"
				"program default2d\n"
				"{\n"
					"map $diffuse\n"
					"nodepth\n"	//don't write depth. this stuff is meant to be an infiniteish distance away.
				"}\n"
				"{\n"
					"map $fullbright\n"
					"blendfunc blend\n"
					"nodepth\n"	//don't write depth. this stuff is meant to be an infiniteish distance away.
				"}\n"
			"}\n"
		);
}

/*
=================
GL_DrawSkyChain
=================
*/
qboolean R_DrawSkyChain (batch_t *batch)
{
	shader_t *skyshader;
	texid_t *skyboxtex;

	if (forcedskyshader)
		skyshader = forcedskyshader;
	else
		skyshader = batch->shader;

	if (skyshader->prog)
		return false;

	if (skyshader->skydome)
		skyboxtex = skyshader->skydome->farbox_textures;
	else
		skyboxtex = NULL;

	if (skyboxtex && TEXVALID(*skyboxtex))
	{
		R_CalcSkyChainBounds(batch);
		GL_DrawSkyBox (skyboxtex, batch);

		if (skyshader->numpasses)
			GL_DrawSkySphere(batch, skyshader);
	}
	else if (skyshader->numpasses)
	{
		if (*r_fastsky.string && TEXVALID(batch->shader->defaulttextures->base) && TEXVALID(batch->shader->defaulttextures->fullbright))
		{
			R_CalcSkyChainBounds(batch);
			GL_DrawSkyGrid(skyshader->defaulttextures);
		}
		else
			GL_DrawSkySphere(batch, skyshader);
	}

	//neither skydomes nor skyboxes nor skygrids will have been drawn with the correct depth values for the sky.
	//this can result in rooms behind the sky surfaces being visible.
	//so make sure they're correct where they're expected to be.
	//don't do it on q3 bsp, because q3map2 can't do skyrooms without being weird about it. or something. anyway, we get different (buggy) behaviour from q3 if we don't skip this.
	//See: The Edge Of Forever (motef, by sock) for an example of where this needs to be skipped.
	//See dm3 for an example of where the depth needs to be correct (OMG THERE'S PLAYERS IN MY SKYBOX! WALLHAXX!).
	//you can't please them all.
	if (r_worldentity.model->fromgame != fg_quake3)
		GL_SkyForceDepth(batch);

	return true;
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

		if (skymins[0][axis] > s)
			skymins[0][axis] = s;
		if (skymins[1][axis] > t)
			skymins[1][axis] = t;
		if (skymaxs[0][axis] < s)
			skymaxs[0][axis] = s;
		if (skymaxs[1][axis] < t)
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

	if (batch->meshes == 1 && !batch->mesh[batch->firstmesh]->numindexes)
	{	//deal with geometryless skies, like terrain/raw maps
		for (i=0 ; i<6 ; i++)
		{
			skymins[0][i] = skymins[1][i] = -1;
			skymaxs[0][i] = skymaxs[1][i] = 1;
		}
		return;
	}
	for (i=0 ; i<6 ; i++)
	{
		skymins[0][i] = skymins[1][i] = 1;//9999;
		skymaxs[0][i] = skymaxs[1][i] = -1;//-9999;
	}

	// calculate vertex values for sky box
	for (m = batch->firstmesh; m < batch->meshes; m++)
	{
		mesh = batch->mesh[m];
		if (!mesh->xyz_array)
			continue;
		//triangulate
		for (i = 0; i < mesh->numindexes; i+=3)
		{
			VectorSubtract (mesh->xyz_array[mesh->indexes[i+0]], r_origin, verts[0]);
			VectorSubtract (mesh->xyz_array[mesh->indexes[i+1]], r_origin, verts[1]);
			VectorSubtract (mesh->xyz_array[mesh->indexes[i+2]], r_origin, verts[2]);
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
	skymesh.lmst_array[0] = (void*)skysphere_texcoord2f;
	skymesh.xyz_array = (void*)skysphere_vertex3f;

	skymesh.numindexes = skysphere_numtriangles * 3;
	skymesh.numvertexes = skysphere_numverts;

	dx = 1;
	dy = 1;
	dz = 1 / 3.0;
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
	if (!cls.allow_skyboxes && batch->texture)	//allow a little extra fps.
	{
		BE_SelectMode(BEM_DEPTHONLY);
		BE_DrawMesh_List(batch->shader, batch->meshes-batch->firstmesh, batch->mesh+batch->firstmesh, batch->vbo, NULL, batch->flags);
		BE_SelectMode(BEM_STANDARD);	/*skys only render in standard mode anyway, so this is safe*/
	}
}

static void R_DrawSkyMesh(batch_t *batch, mesh_t *m, shader_t *shader)
{
	static entity_t skyent;
	batch_t b;

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
	b.skin = NULL;
	b.texture = NULL;
	b.vbo = NULL;
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


static float	speedscale1;	// for top sky
static float	speedscale2;	// for bottom sky
static void EmitSkyGridVert (vec3_t v, vec2_t tc1, vec2_t tc2)
{
	vec3_t dir;
	float	length;

	VectorSubtract (v, r_origin, dir);
	dir[2] *= 3;	// flatten the sphere

	length = VectorLength (dir);
	length = 6*63/length;

	dir[0] *= length;
	dir[1] *= length;

	tc1[0] = (speedscale1 + dir[0]) * (1.0/128);
	tc1[1] = (speedscale1 + dir[1]) * (1.0/128);

	tc2[0] = (speedscale2 + dir[0]) * (1.0/128);
	tc2[1] = (speedscale2 + dir[1]) * (1.0/128);
}

// s and t range from -1 to 1
static void MakeSkyGridVec2 (float s, float t, int axis, vec3_t v, vec2_t tc1, vec2_t tc2)
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

	EmitSkyGridVert(v, tc1, tc2);
}

#define SUBDIVISIONS	10

static void GL_DrawSkyGridFace (int axis, mesh_t *fte_restrict mesh)
{
	int i, j;
	float s, t;

	float fstep = 2.0 / SUBDIVISIONS;

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

			mesh->indexes[mesh->numindexes++] = mesh->numvertexes+0;
			mesh->indexes[mesh->numindexes++] = mesh->numvertexes+1;
			mesh->indexes[mesh->numindexes++] = mesh->numvertexes+2;
			mesh->indexes[mesh->numindexes++] = mesh->numvertexes+0;
			mesh->indexes[mesh->numindexes++] = mesh->numvertexes+2;
			mesh->indexes[mesh->numindexes++] = mesh->numvertexes+3;

			MakeSkyGridVec2 (s,			t,			axis, mesh->xyz_array[mesh->numvertexes], mesh->st_array[mesh->numvertexes], mesh->lmst_array[0][mesh->numvertexes]); mesh->numvertexes++;
			MakeSkyGridVec2 (s,			t + fstep,	axis, mesh->xyz_array[mesh->numvertexes], mesh->st_array[mesh->numvertexes], mesh->lmst_array[0][mesh->numvertexes]); mesh->numvertexes++;
			MakeSkyGridVec2 (s + fstep, t + fstep,	axis, mesh->xyz_array[mesh->numvertexes], mesh->st_array[mesh->numvertexes], mesh->lmst_array[0][mesh->numvertexes]); mesh->numvertexes++;
			MakeSkyGridVec2 (s + fstep, t,			axis, mesh->xyz_array[mesh->numvertexes], mesh->st_array[mesh->numvertexes], mesh->lmst_array[0][mesh->numvertexes]); mesh->numvertexes++;
		}
	}
}

static void GL_DrawSkyGrid (texnums_t *tex)
{
	static entity_t skyent;
	static batch_t b;
	static mesh_t skymesh, *meshptr=&skymesh;

	vecV_t coords[SUBDIVISIONS*SUBDIVISIONS*4*6];
	vec2_t texcoords1[SUBDIVISIONS*SUBDIVISIONS*4*6];
	vec2_t texcoords2[SUBDIVISIONS*SUBDIVISIONS*4*6];
	index_t indexes[SUBDIVISIONS*SUBDIVISIONS*6*6];

	int i;
	float time = cl.gametime+realtime-cl.gametimemark;

	speedscale1 = time*8;
	speedscale1 -= (int)speedscale1 & ~127;
	speedscale2 = time*16;
	speedscale2 -= (int)speedscale2 & ~127;

	skymesh.indexes = indexes;
	skymesh.st_array = texcoords1;
	skymesh.lmst_array[0] = texcoords2;
	skymesh.xyz_array = coords;
	skymesh.numindexes = 0;
	skymesh.numvertexes = 0;

	for (i = 0; i < 6; i++)
	{
		if ((skymins[0][i] >= skymaxs[0][i]	|| skymins[1][i] >= skymaxs[1][i]))
			continue;
		GL_DrawSkyGridFace (i, &skymesh);
	}

	VectorCopy(r_refdef.vieworg, skyent.origin);
	skyent.axis[0][0] = 1;
	skyent.axis[0][1] = 0;
	skyent.axis[0][2] = 0;
	skyent.axis[1][0] = 0;
	skyent.axis[1][1] = 1;
	skyent.axis[1][2] = 0;
	skyent.axis[2][0] = 0;
	skyent.axis[2][1] = 0;
	skyent.axis[2][2] = 1;
	skyent.scale = 1;

//FIXME: We should use the skybox clipping code and split the sphere into 6 sides.
	b.meshes = 1;
	b.firstmesh = 0;
	b.mesh = &meshptr;
	b.ent = &skyent;
	b.shader = skygridface;
	b.skin = tex;
	b.texture = NULL;
	b.vbo = NULL;
	BE_SubmitBatch(&b);
}

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
	vec4_t skyface_colours[4] = {{1,1,1,1},{1,1,1,1},{1,1,1,1},{1,1,1,1}};
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
	skyfacemesh.colors4f_array[0] = skyface_colours;
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

		skyboxface->defaulttextures->base = texnums[skytexorder[i]];
		R_DrawSkyMesh(s, &skyfacemesh, skyboxface);
	}
}

//===============================================================

/*
=============
R_InitSky

A sky image is 256*128 and comprises two logical textures.
the left is the transparent/blended part. the right is the opaque/background part.
==============
*/
void R_InitSky (shader_t *shader, const char *skyname, qbyte *src, unsigned int width, unsigned int height)
{
	int			i, j, p;
	unsigned	*temp;
	unsigned	transpix, alphamask;
	int			r, g, b;
	unsigned	*rgba;
	char name[MAX_QPATH*2];

	unsigned int stride = width;
	width /= 2;

	if (width < 1 || height < 1 || stride != width*2 || !src)
		return;

	//try to load dual-layer-single-image skies.
	//this is always going to be lame special case crap
	{
		size_t filesize = 0;
		qbyte *filedata = NULL;
		if (!filedata)
		{
			Q_snprintfz(name, sizeof(name), "textures/%s.tga", skyname);
			filedata = FS_LoadMallocFile(name, &filesize);
		}
		if (!filedata)
		{
			Q_snprintfz(name, sizeof(name), "textures/%s.png", skyname);
			filedata = FS_LoadMallocFile(name, &filesize);
		}

		if (filedata)
		{
			int imagewidth, imageheight;
			qboolean hasalpha;	//fixme, if this is false, is it worth all this code?
			unsigned int *imagedata = (unsigned int*)Read32BitImageFile(filedata, filesize, &imagewidth, &imageheight, &hasalpha, name);
			Z_Free(filedata);

			if (imagedata && !(imagewidth&1))
			{
				imagewidth>>=1;

				temp = BZF_Malloc(imagewidth*imageheight*sizeof(*temp));
				if (temp)
				{
					for (i=0 ; i<height ; i++)
						for (j=0 ; j<width ; j++)
						{
							temp[i*width+j] = imagedata[i*(width<<1)+j+width];
						}
					Q_snprintfz(name, sizeof(name), "%s_solid", skyname);
					Q_strlwr(name);
					shader->defaulttextures->base = R_LoadReplacementTexture(name, NULL, IF_NOALPHA, temp, imagewidth, imageheight, TF_RGBX32);

					for (i=0 ; i<height ; i++)
						for (j=0 ; j<width ; j++)
						{
							temp[i*width+j] = imagedata[i*(width<<1)+j];
						}
					BZ_Free(imagedata);
					Q_snprintfz(name, sizeof(name), "%s_alpha:%s_trans", skyname, skyname);
					Q_strlwr(name);
					shader->defaulttextures->fullbright = R_LoadReplacementTexture(name, NULL, 0, temp, imagewidth, imageheight, TF_RGBA32);
					BZ_Free(temp);
					return;
				}
			}
			BZ_Free(imagedata);
		}
	}

	temp = BZ_Malloc(width*height*sizeof(*temp));

	// make an average value for the back to avoid
	// a fringe on the top level

	r = g = b = 0;
	for (i=0 ; i<height ; i++)
		for (j=0 ; j<width ; j++)
		{
			p = src[i*stride + j + width];
			rgba = &d_8to24rgbtable[p];
			temp[(i*width) + j] = *rgba;
			r += ((qbyte *)rgba)[0];
			g += ((qbyte *)rgba)[1];
			b += ((qbyte *)rgba)[2];
		}

	if (!shader->defaulttextures->base)
	{
		Q_snprintfz(name, sizeof(name), "%s_solid", skyname);
		Q_strlwr(name);
		shader->defaulttextures->base = R_LoadReplacementTexture(name, NULL, IF_NOALPHA, temp, width, height, TF_RGBX32);
	}

	if (!shader->defaulttextures->fullbright)
	{
		//fixme: use premultiplied alpha here.
		((qbyte *)&transpix)[0] = r/(width*height);
		((qbyte *)&transpix)[1] = g/(width*height);
		((qbyte *)&transpix)[2] = b/(width*height);
		((qbyte *)&transpix)[3] = 0;
		alphamask = LittleLong(0x7fffffff);
		for (i=0 ; i<height ; i++)
			for (j=0 ; j<width ; j++)
			{
				p = src[i*stride + j];
				if (p == 0)
					temp[(i*width) + j] = transpix;
				else
					temp[(i*width) + j] = d_8to24rgbtable[p] & alphamask;
			}

		//FIXME: support _trans
		Q_snprintfz(name, sizeof(name), "%s_alpha:%s_trans", skyname, skyname);
		Q_strlwr(name);
		shader->defaulttextures->fullbright = R_LoadReplacementTexture(name, NULL, 0, temp, width, height, TF_RGBA32);
	}
	BZ_Free(temp);
}
#endif
