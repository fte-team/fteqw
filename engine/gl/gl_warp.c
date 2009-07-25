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

static void GL_DrawProgram_WaterChain(msurface_t *fa);
static void GL_DrawProgram_SkyChain(msurface_t *fa);
static void R_CalcSkyChainBounds (msurface_t *s);
static void GL_DrawSkyGrid (texture_t *tex);
static void GL_DrawSkySphere (msurface_t *fa);
static void GL_SkyForceDepth(msurface_t *fa);
void D3D7_DrawSkySphere (msurface_t *fa);
void D3D9_DrawSkySphere (msurface_t *fa);

extern	model_t	*loadmodel;

int		skytexturenum;

static float	speedscale;		// for top sky and bottom sky

static float skyrotate;
static vec3_t skyaxis;

static qboolean usingskybox;

static msurface_t	*warpface;

extern cvar_t r_skyboxname;
extern cvar_t gl_skyboxdist;
extern cvar_t r_fastsky;
extern cvar_t r_fastskycolour;
static char defaultskybox[MAX_QPATH];

/*static*/ int skyprogram;
/*static*/ int skyprogram_time;
/*static*/ int skyprogram_eyepos;

/*static*/ int waterprogram;
/*static*/ int waterprogram_time;

int skyboxtex[6];
static vec3_t glskycolor;

void GLR_Fastskycolour_Callback(struct cvar_s *var, char *oldvalue)
{
	SCR_StringToRGB(var->string, glskycolor, 255);
}


static void BoundPoly (int numverts, float *verts, vec3_t mins, vec3_t maxs)
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

void GL_Warp_Init(void)
{
	char *progtext;
	static char *skyglslprog = 
		"#ifdef VERTEX_SHADER\n"
		"varying vec3 pos;\n"

		"void main (void)\n"
		"{\n"
		"	pos = gl_Vertex.xyz;\n"
		"	gl_Position = ftransform();\n"
		"}\n"
		"#endif\n"

		"#ifdef FRAGMENT_SHADER\n"
		"uniform sampler2D solidt;\n"
		"uniform sampler2D transt;\n"

		"uniform float time;\n"
		"uniform vec3 eyepos;\n"
		"varying vec3 pos;\n"

		"void main (void)\n"
		"{\n"
		"	vec2 tccoord;\n"

		"	vec3 dir = pos - eyepos;\n"

		"	dir.z *= 3;\n"
		"	dir.xy /= 0.5*length(dir);\n"

		"	tccoord = (dir.xy + time*0.03125);\n"
		"	vec3 solid = vec3(texture2D(solidt, tccoord));\n"

		"	tccoord = (dir.xy + time*0.0625);\n"
		"	vec4 clouds = texture2D(transt, tccoord);\n"

		"	gl_FragColor.rgb = solid*(1-clouds.a) + clouds.rgb*clouds.a;\n"
		"}\n"
		"#endif\n"
		;

	static char *waterglslprog = 
		"#ifdef VERTEX_SHADER\n"
		"varying vec3 pos;\n"
		"varying vec2 tc;\n"

		"void main (void)\n"
		"{\n"
		"	tc = gl_MultiTexCoord0.st;\n"
		"	gl_Position = ftransform();\n"
		"}\n"
		"#endif\n"

		"#ifdef FRAGMENT_SHADER\n"
		"uniform sampler2D watertexture;\n"
		"uniform float time;\n"
		"varying vec2 tc;\n"

		"void main (void)\n"
		"{\n"
		"	vec2 ntc;\n"
		"	ntc.s = tc.s + sin(tc.t+time)*0.125;\n"
		"	ntc.t = tc.t + sin(tc.s+time)*0.125;\n"
		"	vec3 ts = vec3(texture2D(watertexture, ntc));\n"

		"	gl_FragColor.rgb = ts;\n"
		"}\n"
		"#endif\n"
		;

	if (FS_LoadFile("quakesky.glsl", &progtext) < 0)
		progtext = skyglslprog;
	skyprogram = GLSlang_CreateProgram("", progtext, progtext);
	if (progtext != skyglslprog)
		FS_FreeFile(progtext);

	if (skyprogram)
	{
		GLSlang_UseProgram(skyprogram);

		qglUniform1iARB(qglGetUniformLocationARB(skyprogram, "solidt"), 0);
		qglUniform1iARB(qglGetUniformLocationARB(skyprogram, "transt"), 1);

		skyprogram_time = qglGetUniformLocationARB(skyprogram, "time");
		skyprogram_eyepos = qglGetUniformLocationARB(skyprogram, "eyepos");

		GLSlang_UseProgram(0);
	}

	if (FS_LoadFile("quakewater.glsl", &progtext) < 0)
		progtext = waterglslprog;
	waterprogram = GLSlang_CreateProgram("", progtext, progtext);
	if (progtext != waterglslprog)
		FS_FreeFile(progtext);

	if (waterprogram)
	{
		GLSlang_UseProgram(waterprogram);

		qglUniform1iARB(qglGetUniformLocationARB(waterprogram, "watertexture"), 0);
		waterprogram_time = qglGetUniformLocationARB(waterprogram, "time");

		GLSlang_UseProgram(0);
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
	//the code prior to april 2005 gave a nicer result, but was incompatible with meshes and required poly lists instead
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
	if (!*r_waterlayers.string)
	{
		if (waterprogram)
		{
			GL_DrawProgram_SkyChain(fa);
			return;
		}
		r_waterlayers.value = 3;
	}
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
			GL_DrawAliasMesh(fa->mesh, fa->texinfo->texture->tn.base);
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
		GL_DrawAliasMesh(fa->mesh, fa->texinfo->texture->tn.base);
		qglPopMatrix();
		qglMatrixMode(GL_MODELVIEW);
	}
}

#endif

void EmitWaterPolyChain (msurface_t *s, float basealpha)
{
	float a;
	int l;
	extern cvar_t r_waterlayers;

#ifdef Q3SHADERS
	if (s->texinfo->texture->shader)
	{
		meshbuffer_t mb;
		mb.sortkey = 0;
		mb.infokey = 0;
		mb.dlightbits = 0;
		mb.entity = &r_worldentity;
		mb.shader = s->texinfo->texture->shader;
		mb.fog = NULL;
		mb.mesh = s->mesh;
		r_worldentity.shaderRGBAf[3] = basealpha;
		while(s)
		{
			if (R_MeshWillExceed(s->mesh))
				R_RenderMeshBuffer(&mb, false);
			R_PushMesh(s->mesh, mb.shader->features);
		}
		r_worldentity.shaderRGBAf[3] = 1;
		R_RenderMeshBuffer(&mb, false);
		return;
	}
#endif
	if (!*r_waterlayers.string)
	{
		if (waterprogram)
		{
			GL_DrawProgram_WaterChain(s);
			return;
		}
		r_waterlayers.value = 3;
	}
	if (r_waterlayers.value>=1)
	{
		msurface_t *fa;
		qglEnable(GL_BLEND);	//to ensure.
		qglMatrixMode(GL_TEXTURE);
		for (a=basealpha,l = 0; l < r_waterlayers.value; l++,a=a*4/6)
		{
			qglPushMatrix();
			qglColor4f(1, 1, 1, a);
			qglTranslatef (sin(cl.time+l*4) * 0.04f+cos(cl.time/2+l)*0.02f+cl.time/(64+l*8), cos(cl.time+l*4) * 0.06f+sin(cl.time/2+l)*0.02f+cl.time/(16+l*2), 0);
			for (fa = s; fa; fa = fa->texturechain)
			{
				fa->mesh->colors_array=NULL;
				GL_DrawAliasMesh(fa->mesh, fa->texinfo->texture->tn.base);
			}
			qglPopMatrix();
		}
		qglMatrixMode(GL_MODELVIEW);
		qglDisable(GL_BLEND);	//to ensure.
	}
	else	//dull (fast) single player
	{
		msurface_t *fa;
		qglMatrixMode(GL_TEXTURE);
		qglPushMatrix();
		qglTranslatef (sin(cl.time) * 0.4f, cos(cl.time) * 0.06f, 0);
		for (fa = s; fa; fa = fa->texturechain)
		{
			fa->mesh->colors_array=NULL;
			GL_DrawAliasMesh(fa->mesh, fa->texinfo->texture->tn.base);
		}
		qglPopMatrix();
		qglMatrixMode(GL_MODELVIEW);
	}
}

/*
=================
GL_DrawSkyChain
=================
*/
#ifdef RGLQUAKE
static void R_DrawSkyBoxChain (msurface_t *s);
void GL_DrawSkyChain (msurface_t *s)
{
	msurface_t	*fa;

	GL_DisableMultitexture();
#ifdef Q3SHADERS
	if (!skyboxtex[0] && !usingskybox)
	{
		int i;
		if (s->texinfo->texture->shader && s->texinfo->texture->shader->skydome)
		{
			for (i = 0; i < 6; i++)
			{
				skyboxtex[i] = s->texinfo->texture->shader->skydome->farbox_textures[i];		
			}
		}
	}
#endif

	if (r_fastsky.value>0)	//this is for visability only... we'd otherwise not stoop this low (and this IS low)
	{
		R_IBrokeTheArrays();
		qglDisable(GL_BLEND);
		qglDisable(GL_TEXTURE_2D);
		qglDisable(GL_ALPHA_TEST);
		qglColor3f(glskycolor[0], glskycolor[1], glskycolor[2]);
		qglDisableClientState( GL_COLOR_ARRAY );
		qglEnableClientState( GL_VERTEX_ARRAY );
		for (fa=s ; fa ; fa=fa->texturechain)
		{
			qglVertexPointer(3, GL_FLOAT, 0, fa->mesh->xyz_array);
			qglDrawElements(GL_TRIANGLES, fa->mesh->numindexes, GL_INDEX_TYPE, fa->mesh->indexes);
		}
		qglColor3f(1, 1, 1);
		qglEnable(GL_TEXTURE_2D);
		R_IBrokeTheArrays();
		return;
	}

	if (skyprogram && !usingskybox)
	{
		GL_DrawProgram_SkyChain(s);
		return;
	}

	R_CalcSkyChainBounds(s);

#ifdef RGLQUAKE
	if (usingskybox)
	if (qrenderer == QR_OPENGL)
	{
		GL_DrawSkyBox (s);
		GL_SkyForceDepth(s);
		return;
	}
#endif

	if (*r_fastsky.string)
	{
		GL_DrawSkyGrid(s->texinfo->texture);
		GL_SkyForceDepth(s);
	}
	else
	{
		GL_DrawSkySphere(s);
		GL_SkyForceDepth(s);
	}
}
#endif

#ifdef D3DQUAKE
static void R_DrawSkyBoxChain (msurface_t *s);
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

static vec3_t	skyclip[6] = {
	{1,1,0},
	{1,-1,0},
	{0,-1,1},
	{0,1,1},
	{1,0,1},
	{-1,0,1} 
};
static int	c_sky;

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
static void R_CalcSkyChainBounds (msurface_t *s)
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
static mesh_t skymesh;


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

static void GL_SkyForceDepth(msurface_t *fa)
{
	vbo_t *v;
	mesh_t *m;

	if (!cls.allow_skyboxes)	//allow a little extra fps.
	{//Draw the texture chain to only the depth buffer.
		v = &fa->texinfo->texture->vbo;
		qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, v->vboe);
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, v->vbocoord);
		qglVertexPointer(3, GL_FLOAT, 0, v->coord);
		qglDisable(GL_TEXTURE_2D);

		if (qglColorMask)
			qglColorMask(0,0,0,0);
		for (; fa; fa = fa->texturechain)
		{
			m = fa->mesh;
			qglDrawRangeElements(GL_TRIANGLES, m->vbofirstvert, m->vbofirstvert+m->numvertexes, m->numindexes, GL_INDEX_TYPE, v->indicies+m->vbofirstelement);
		}
		if (qglColorMask)
			qglColorMask(1,1,1,1);

		qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

		R_IBrokeTheArrays();
	}
}

static void GL_DrawProgram_SkyChain(msurface_t *fa)
{
	vbo_t *v;
	mesh_t *m;

	v = &fa->texinfo->texture->vbo;
	qglUseProgramObjectARB(skyprogram);
	qglUniform1fARB(skyprogram_time, cl.time);
	qglUniform3fvARB(skyprogram_eyepos, 1, r_origin);


	qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, v->vboe);
	qglBindBufferARB(GL_ARRAY_BUFFER_ARB, v->vbocoord);
	qglVertexPointer(3, GL_FLOAT, 0, v->coord);

	GL_MBind(mtexid0, fa->texinfo->texture->tn.base);
	qglEnable(GL_TEXTURE_2D);
	GL_MBind(mtexid1, fa->texinfo->texture->tn.fullbright);
	qglEnable(GL_TEXTURE_2D);

	for (; fa; fa = fa->texturechain)
	{
		m = fa->mesh;
		qglDrawRangeElements(GL_TRIANGLES, m->vbofirstvert, m->vbofirstvert+m->numvertexes, m->numindexes, GL_INDEX_TYPE, v->indicies+m->vbofirstelement);
	}

	qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	qglUseProgramObjectARB(0);
	qglDisable(GL_TEXTURE_2D);
	GL_SelectTexture(mtexid0);
}

static void GL_DrawProgram_WaterChain(msurface_t *fa)
{
	vbo_t *v;
	mesh_t *m;

	v = &fa->texinfo->texture->vbo;
	qglUseProgramObjectARB(waterprogram);
	qglUniform1fARB(waterprogram_time, cl.time);


	qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, v->vboe);
	qglBindBufferARB(GL_ARRAY_BUFFER_ARB, v->vbocoord);
	qglVertexPointer(3, GL_FLOAT, 0, v->coord);

	GL_MBind(mtexid0, fa->texinfo->texture->tn.base);
	qglEnable(GL_TEXTURE_2D);

	qglBindBufferARB(GL_ARRAY_BUFFER_ARB, v->vbotexcoord);
	qglTexCoordPointer(2, GL_FLOAT, 0, v->texcoord);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);

	for (; fa; fa = fa->texturechain)
	{
		m = fa->mesh;
		qglDrawRangeElements(GL_TRIANGLES, m->vbofirstvert, m->vbofirstvert+m->numvertexes, m->numindexes, GL_INDEX_TYPE, v->indicies+m->vbofirstelement);
	}

	qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	qglUseProgramObjectARB(0);
	GL_SelectTexture(mtexid0);
}

static void GL_DrawSkySphere (msurface_t *fa)
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
		GL_DrawAliasMesh(&skymesh, fa->texinfo->texture->tn.base);
		qglColor4f(1,1,1,0.5);
		qglEnable(GL_BLEND);
		qglTranslatef(time*8/128, time*8/128, 0);
		GL_DrawAliasMesh(&skymesh, fa->texinfo->texture->tn.fullbright);
		qglDisable(GL_BLEND);
		qglPopMatrix();
		qglMatrixMode(GL_MODELVIEW);
	}
	qglPopMatrix();
}
#endif

#ifdef D3DQUAKE
static void D3D7_DrawSkySphere (msurface_t *fa)
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
static void D3D9_DrawSkySphere (msurface_t *fa)
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

	for (i=0 ; i<6 ; i++)
	{
		skymins[0][i] = skymins[1][i] = 9999;
		skymaxs[0][i] = skymaxs[1][i] = -9999;
	}

	if (!skyboxtex[0] || !skyboxtex[1] || !skyboxtex[2] || !skyboxtex[3] || !skyboxtex[4] || !skyboxtex[5])
	{
		usingskybox = false;
		return;
	}

	usingskybox = true;
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
static void GL_MakeSkyVec (float s, float t, int axis)
{
	vec3_t		v, b;
	int			j, k;
	float skydist = gl_skyboxdist.value;
	extern cvar_t gl_maxdist;

	if (!skydist)
	{
		if (r_shadows.value || !gl_maxdist.value)
			skydist = 1000000;	//inifite distance
		else
			skydist = gl_maxdist.value * 0.577;
	}

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
	extern cvar_t gl_maxdist;

	if (!skydist)
	{
		if (r_shadows.value || !gl_maxdist.value)
			skydist = 1000000;	//inifite distance
		else
			skydist = gl_maxdist.value * 0.577;
	}

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

	GL_DisableMultitexture();
	GL_Bind (tex->tn.base);

	speedscale = time*8;
	speedscale -= (int)speedscale & ~127;

	for (i = 0; i < 6; i++)
	{
		if ((skymins[0][i] >= skymaxs[0][i]	|| skymins[1][i] >= skymaxs[1][i]))
			continue;
		GL_DrawSkyGridFace (i);
	}

	qglEnable (GL_BLEND);
	GL_Bind (tex->tn.fullbright);

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
int	skytexorder[6] = {0,2,1,3,4,5};
#ifdef RGLQUAKE
void GL_DrawSkyBox (msurface_t *s)
{
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

	int		solidskytexture;
	int		alphaskytexture;

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

	Q_snprintfz(name, sizeof(name), "%s_solid", mt->name);
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

	Q_snprintfz(name, sizeof(name), "%s_trans", mt->name);
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

	mt->tn.base = solidskytexture;
	mt->tn.fullbright = alphaskytexture;
}
#endif
