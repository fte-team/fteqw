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
// r_main.c

#include "quakedef.h"

#ifdef RGLQUAKE
#include "glquake.h"
#include "renderque.h"

#ifdef Q3SHADERS
#include "shader.h"
#endif

void R_RenderBrushPoly (msurface_t *fa);

#define PROJECTION_DISTANCE			200
#define MAX_STENCIL_ENTS			128

extern int		gl_canstencil;

vrect_t gl_truescreenrect;

FTEPFNGLCOMPRESSEDTEXIMAGE2DARBPROC qglCompressedTexImage2DARB;
FTEPFNGLGETCOMPRESSEDTEXIMAGEARBPROC qglGetCompressedTexImageARB;

#define	Q2RF_WEAPONMODEL		4		// only draw through eyes
#define Q2RF_DEPTHHACK 16

entity_t	r_worldentity;

vec3_t		modelorg, r_entorigin;
entity_t	*currententity;

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

float		r_wateralphaval;	//allowed or not...

//mplane_t	frustum[4];

int			c_brush_polys, c_alias_polys;

qboolean	envmap;				// true during envmap command capture 

int			particletexture;	// little dot for particles
int			particlecqtexture;	// little dot for particles
int			explosiontexture;
int			balltexture;

int			mirrortexturenum;	// quake texturenum, not gltexturenum
qboolean	mirror;
mplane_t	*mirror_plane;
msurface_t	*r_mirror_chain;
qboolean	r_inmirror;	//or out-of-body

//
// view origin
//
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

extern float	r_projection_matrix[16];
extern float	r_view_matrix[16];

//
// screen size info
//
refdef_t	r_refdef;

mleaf_t		*r_viewleaf, *r_oldviewleaf;
mleaf_t		*r_viewleaf2, *r_oldviewleaf2;
int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

texture_t	*r_notexture_mip;

//void R_MarkLeaves (void);

cvar_t	r_norefresh = SCVAR("r_norefresh","0");
//cvar_t	r_drawentities = SCVAR("r_drawentities","1");
//cvar_t	r_drawviewmodel = SCVAR("r_drawviewmodel","1");
//cvar_t	r_speeds = SCVAR("r_speeds","0");
//cvar_t	r_fullbright = SCVAR("r_fullbright","0");
cvar_t	r_mirroralpha = SCVARF("r_mirroralpha","1", CVAR_CHEAT);
//cvar_t	r_waterwarp = SCVAR("r_waterwarp", "0");
//cvar_t	r_novis = SCVAR("r_novis","0");
//cvar_t	r_netgraph = SCVAR("r_netgraph","0");

extern cvar_t	gl_part_flame;

cvar_t	gl_clear = SCVAR("gl_clear","0");
cvar_t	gl_cull = SCVAR("gl_cull","1");
cvar_t	gl_smoothmodels = SCVAR("gl_smoothmodels","1");
cvar_t	gl_affinemodels = SCVAR("gl_affinemodels","0");
cvar_t	gl_playermip = SCVAR("gl_playermip","0");
cvar_t	gl_keeptjunctions = SCVAR("gl_keeptjunctions","1");
cvar_t	gl_reporttjunctions = SCVAR("gl_reporttjunctions","0");
cvar_t	gl_finish = SCVAR("gl_finish","0");
cvar_t	gl_dither = SCVAR("gl_dither", "1");
cvar_t	gl_maxdist = SCVAR("gl_maxdist", "8192");

extern cvar_t	gl_contrast;
extern cvar_t	gl_mindist;

extern cvar_t	gl_motionblur;
extern cvar_t	gl_motionblurscale;

extern cvar_t gl_ati_truform;
extern cvar_t gl_ati_truform_type;
extern cvar_t gl_ati_truform_tesselation;

extern cvar_t gl_blendsprites;

#ifdef R_XFLIP
cvar_t	r_xflip = SCVAR("leftisright", "0");
#endif

extern	cvar_t	gl_ztrick;
extern	cvar_t	scr_fov;

// post processing stuff
int sceneblur_texture;
int scenepp_texture;
int scenepp_texture_warp;
int scenepp_texture_edge;

int scenepp_ww_program;
int scenepp_ww_parm_texture0i;
int scenepp_ww_parm_texture1i;
int scenepp_ww_parm_texture2i;
int scenepp_ww_parm_ampscalef;

int scenepp_mt_program;
int scenepp_mt_parm_texture0i;
int scenepp_mt_parm_colorf;
int scenepp_mt_parm_inverti;

// KrimZon - init post processing - called in GL_CheckExtensions, when they're called
// I put it here so that only this file need be changed when messing with the post
// processing shaders
void GL_InitSceneProcessingShaders_WaterWarp (void)
{
	char *genericvert = "\
		varying vec2 v_texCoord0;\
		varying vec2 v_texCoord1;\
		varying vec2 v_texCoord2;\
		void main (void)\
		{\
			vec4 v = vec4( gl_Vertex.x, gl_Vertex.y, gl_Vertex.z, 1.0 );\
			gl_Position = gl_ModelViewProjectionMatrix * v;\
			v_texCoord0 = gl_MultiTexCoord0.xy;\
			v_texCoord1 = gl_MultiTexCoord1.xy;\
			v_texCoord2 = gl_MultiTexCoord2.xy;\
		}\
		";

	char *wwfrag = "\
		varying vec2 v_texCoord0;\
		varying vec2 v_texCoord1;\
		varying vec2 v_texCoord2;\
		uniform sampler2D theTexture0;\
		uniform sampler2D theTexture1;\
		uniform sampler2D theTexture2;\
		uniform float ampscale;\
		void main (void)\
		{\
			float amptemp;\
			vec3 edge;\
			edge = texture2D( theTexture2, v_texCoord2 ).rgb;\
			amptemp = ampscale * edge.x;\
			vec3 offset;\
			offset = texture2D( theTexture1, v_texCoord1 ).rgb;\
			offset.x = (offset.x - 0.5) * 2.0;\
			offset.y = (offset.y - 0.5) * 2.0;\
			vec2 temp;\
			temp.x = v_texCoord0.x + offset.x * amptemp;\
			temp.y = v_texCoord0.y + offset.y * amptemp;\
			gl_FragColor = texture2D( theTexture0, temp );\
		}\
		";

	if (qglGetError())
		Con_Printf("GL Error before initing shader object\n");

	scenepp_ww_program = GLSlang_CreateProgram(NULL, genericvert, wwfrag);

	if (!scenepp_ww_program)
		return;

	scenepp_ww_parm_texture0i	= GLSlang_GetUniformLocation(scenepp_ww_program, "theTexture0");
	scenepp_ww_parm_texture1i	= GLSlang_GetUniformLocation(scenepp_ww_program, "theTexture1");
	scenepp_ww_parm_texture2i	= GLSlang_GetUniformLocation(scenepp_ww_program, "theTexture2");
	scenepp_ww_parm_ampscalef	= GLSlang_GetUniformLocation(scenepp_ww_program, "ampscale");

	GLSlang_UseProgram(scenepp_ww_program);

	GLSlang_SetUniform1i(scenepp_ww_parm_texture0i, 0);
	GLSlang_SetUniform1i(scenepp_ww_parm_texture1i, 1);
	GLSlang_SetUniform1i(scenepp_ww_parm_texture2i, 2);

	GLSlang_UseProgram(0);

	if (qglGetError())
		Con_Printf(CON_ERROR "GL Error initing shader object\n");
}

void GL_InitSceneProcessingShaders_MenuTint(void)
{
	char *vshader = "\
		varying vec2 texcoord;\
		void main(void)\
		{\
			texcoord = gl_MultiTexCoord0.xy;\
			gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\
		}";
	char *fshader = "\
		varying vec2 texcoord;\
		uniform vec3 colorparam;\
		uniform sampler2D source;\
		uniform int invert;\
		const vec3 lumfactors = vec3(0.299, 0.587, 0.114);\
		const vec3 invertvec = vec3(1.0, 1.0, 1.0);\
		void main(void)\
		{\
			vec3 texcolor = texture2D(source, texcoord).rgb;\
			float luminance = dot(lumfactors, texcolor);\
			texcolor = vec3(luminance, luminance, luminance);\
			texcolor *= colorparam;\
			texcolor = invert > 0 ? (invertvec - texcolor) : texcolor;\
			gl_FragColor = vec4(texcolor, 1.0);\
		}";

	if (qglGetError())
		Con_Printf("GL Error before initing shader object\n");

	scenepp_mt_program = GLSlang_CreateProgram(NULL, vshader, fshader);

	if (!scenepp_mt_program)
		return;

	scenepp_mt_parm_texture0i	= GLSlang_GetUniformLocation(scenepp_mt_program, "source");
	scenepp_mt_parm_colorf		= GLSlang_GetUniformLocation(scenepp_mt_program, "colorparam");
	scenepp_mt_parm_inverti		= GLSlang_GetUniformLocation(scenepp_mt_program, "invert");

	GLSlang_UseProgram(scenepp_mt_program);
	GLSlang_SetUniform1i(scenepp_mt_parm_texture0i, 0);

	GLSlang_UseProgram(0);

	if (qglGetError())
		Con_Printf(CON_ERROR "GL Error initing shader object\n");
}

void GL_InitSceneProcessingShaders (void)
{
	GL_InitSceneProcessingShaders_WaterWarp();
	GL_InitSceneProcessingShaders_MenuTint();
}

#define PP_WARP_TEX_SIZE 64
#define PP_AMP_TEX_SIZE 64
#define PP_AMP_TEX_BORDER 4
void GL_SetupSceneProcessingTextures (void)
{
	int i, x, y;
	unsigned char pp_warp_tex[PP_WARP_TEX_SIZE*PP_WARP_TEX_SIZE*3];
	unsigned char pp_edge_tex[PP_AMP_TEX_SIZE*PP_AMP_TEX_SIZE*3];

	sceneblur_texture = GL_AllocNewTexture();

	if (!gl_config.arb_shader_objects)
		return;

	scenepp_texture = GL_AllocNewTexture();
	scenepp_texture_warp = GL_AllocNewTexture();
	scenepp_texture_edge = GL_AllocNewTexture();

	// init warp texture - this specifies offset in 
	for (y=0; y<PP_WARP_TEX_SIZE; y++)
	{
		for (x=0; x<PP_WARP_TEX_SIZE; x++)
		{
			float fx, fy;

			i = (x + y*PP_WARP_TEX_SIZE) * 3;

			fx = sin(((double)y / PP_WARP_TEX_SIZE) * M_PI * 2);
			fy = cos(((double)x / PP_WARP_TEX_SIZE) * M_PI * 2);

			pp_warp_tex[i  ] = (fx+1.0f)*127.0f;
			pp_warp_tex[i+1] = (fy+1.0f)*127.0f;
			pp_warp_tex[i+2] = 0;
		}
	}

	GL_Bind(scenepp_texture_warp);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qglTexImage2D(GL_TEXTURE_2D, 0, 3, PP_WARP_TEX_SIZE, PP_WARP_TEX_SIZE, 0, GL_RGB, GL_UNSIGNED_BYTE, pp_warp_tex);

	// TODO: init edge texture - this is ampscale * 2, with ampscale calculated
	// init warp texture - this specifies offset in 
	for (y=0; y<PP_AMP_TEX_SIZE; y++)
	{
		for (x=0; x<PP_AMP_TEX_SIZE; x++)
		{
			float fx = 1, fy = 1;

			i = (x + y*PP_AMP_TEX_SIZE) * 3;

			if (x < PP_AMP_TEX_BORDER)
			{
				fx = (float)x / PP_AMP_TEX_BORDER;
			}
			if (x > PP_AMP_TEX_SIZE - PP_AMP_TEX_BORDER)
			{
				fx = (PP_AMP_TEX_SIZE - (float)x) / PP_AMP_TEX_BORDER;
			}

			if (y < PP_AMP_TEX_BORDER)
			{
				fy = (float)y / PP_AMP_TEX_BORDER;
			}
			if (y > PP_AMP_TEX_SIZE - PP_AMP_TEX_BORDER)
			{
				fy = (PP_AMP_TEX_SIZE - (float)y) / PP_AMP_TEX_BORDER;
			}

			if (fx < fy)
			{
				fy = fx;
			}

			pp_edge_tex[i  ] = fy * 255;
			pp_edge_tex[i+1] = 0;
			pp_edge_tex[i+2] = 0;
		}
	}

	GL_Bind(scenepp_texture_edge);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, PP_WARP_TEX_SIZE, PP_WARP_TEX_SIZE, 0, GL_RGB, GL_UNSIGNED_BYTE, pp_edge_tex);
}

void R_RotateForEntity (entity_t *e)
{
	float m[16];
	if (e->flags & Q2RF_WEAPONMODEL && r_refdef.currentplayernum>=0)
	{	//rotate to view first
		m[0] = cl.viewent[r_refdef.currentplayernum].axis[0][0];
		m[1] = cl.viewent[r_refdef.currentplayernum].axis[0][1];
		m[2] = cl.viewent[r_refdef.currentplayernum].axis[0][2];
		m[3] = 0;

		m[4] = cl.viewent[r_refdef.currentplayernum].axis[1][0];
		m[5] = cl.viewent[r_refdef.currentplayernum].axis[1][1];
		m[6] = cl.viewent[r_refdef.currentplayernum].axis[1][2];
		m[7] = 0;

		m[8] = cl.viewent[r_refdef.currentplayernum].axis[2][0];
		m[9] = cl.viewent[r_refdef.currentplayernum].axis[2][1];
		m[10] = cl.viewent[r_refdef.currentplayernum].axis[2][2];
		m[11] = 0;

		m[12] = cl.viewent[r_refdef.currentplayernum].origin[0];
		m[13] = cl.viewent[r_refdef.currentplayernum].origin[1];
		m[14] = cl.viewent[r_refdef.currentplayernum].origin[2];
		m[15] = 1;

		qglMultMatrixf(m);
	}

	m[0] = e->axis[0][0];
	m[1] = e->axis[0][1];
	m[2] = e->axis[0][2];
	m[3] = 0;

	m[4] = e->axis[1][0];
	m[5] = e->axis[1][1];
	m[6] = e->axis[1][2];
	m[7] = 0;

	m[8] = e->axis[2][0];
	m[9] = e->axis[2][1];
	m[10] = e->axis[2][2];
	m[11] = 0;

	m[12] = e->origin[0];
	m[13] = e->origin[1];
	m[14] = e->origin[2];
	m[15] = 1;

	qglMultMatrixf(m);
}

/*
=============================================================

  SPRITE MODELS

=============================================================
*/

/*
================
R_GetSpriteFrame
================
*/
/*
mspriteframe_t *R_GetSpriteFrame (entity_t *currententity)
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe;
	int				i, numframes, frame;
	float			*pintervals, fullinterval, targettime, time;

	psprite = currententity->model->cache.data;
	frame = currententity->frame;

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		Con_DPrintf ("R_DrawSprite: no such frame %d (%s)\n", frame, currententity->model->name);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else if (psprite->frames[frame].type == SPR_ANGLED)
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pspriteframe = pspritegroup->frames[(int)((r_refdef.viewangles[1]-currententity->angles[1])/360*8 + 0.5-4)&7];
	}
	else
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = currententity->frame1time;

	// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
	// are positive, so we don't have to worry about division by 0
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i=0 ; i<(numframes-1) ; i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}
*/

/*
=================
R_DrawSpriteModel

=================
*/
void R_DrawSpriteModel (entity_t *e)
{
	vec3_t	point;
	mspriteframe_t	*frame;
	vec3_t		forward, right, up;
	msprite_t		*psprite;

#ifdef Q3SHADERS
	qbyte coloursb[4];

	if (e->forcedshader)
	{
		meshbuffer_t mb;
		mesh_t mesh;
		vec2_t texcoords[4]={{0, 1},{0,0},{1,0},{1,1}};
		vec3_t vertcoords[4];
		index_t indexes[6] = {0, 1, 2, 0, 2, 3};
		byte_vec4_t colours[4];
		float x, y;

#define VectorSet(a,b,c,v) {v[0]=a;v[1]=b;v[2]=c;}
		x = cos(e->rotation+225*M_PI/180)*e->scale;
		y = sin(e->rotation+225*M_PI/180)*e->scale;
		VectorSet (e->origin[0] - y*vright[0] + x*vup[0], e->origin[1] - y*vright[1] + x*vup[1], e->origin[2] - y*vright[2] + x*vup[2], vertcoords[3]);
		VectorSet (e->origin[0] - x*vright[0] - y*vup[0], e->origin[1] - x*vright[1] - y*vup[1], e->origin[2] - x*vright[2] - y*vup[2], vertcoords[2]);
		VectorSet (e->origin[0] + y*vright[0] - x*vup[0], e->origin[1] + y*vright[1] - x*vup[1], e->origin[2] + y*vright[2] - x*vup[2], vertcoords[1]);
		VectorSet (e->origin[0] + x*vright[0] + y*vup[0], e->origin[1] + x*vright[1] + y*vup[1], e->origin[2] + x*vright[2] + y*vup[2], vertcoords[0]);

		coloursb[0] = e->shaderRGBAf[0]*255;
		coloursb[1] = e->shaderRGBAf[1]*255;
		coloursb[2] = e->shaderRGBAf[2]*255;
		coloursb[3] = e->shaderRGBAf[3]*255;
		*(int*)colours[0] = *(int*)colours[1] = *(int*)colours[2] = *(int*)colours[3] = *(int*)coloursb;

		mesh.colors_array = colours;
		mesh.indexes = indexes;
		mesh.lmst_array = NULL;
		mesh.st_array = texcoords;
		mesh.normals_array = NULL;
		mesh.numvertexes = 4;
		mesh.numindexes = 6;
		mesh.radius = e->scale;
		mesh.xyz_array = vertcoords;
		mesh.normals_array = NULL;

		
		R_IBrokeTheArrays();

		mb.entity = e;
		mb.shader = e->forcedshader;
		mb.fog = NULL;//fog;
		mb.mesh = &mesh;
		mb.infokey = -1;
		mb.dlightbits = 0;

		R_PushMesh(&mesh, mb.shader->features | MF_NONBATCHED|MF_COLORS);

		R_RenderMeshBuffer ( &mb, false );
		return;
	}
#endif
	if (!e->model)
		return;

	if (e->flags & RF_NODEPTHTEST)
		qglDisable(GL_DEPTH_TEST);

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	frame = R_GetSpriteFrame (e);
	psprite = e->model->cache.data;
//	frame = 0x05b94140;

	switch(psprite->type)
	{
	case SPR_ORIENTED:
		// bullet marks on walls
		AngleVectors (e->angles, forward, right, up);
		break;

	case SPR_FACING_UPRIGHT:
		up[0] = 0;up[1] = 0;up[2]=1;
		right[0] = e->origin[1] - r_origin[1];
		right[1] = -(e->origin[0] - r_origin[0]);
		right[2] = 0;
		VectorNormalize (right);
		break;
	case SPR_VP_PARALLEL_UPRIGHT:
		up[0] = 0;up[1] = 0;up[2]=1;
		VectorCopy (vright, right);
		break;

	default:
	case SPR_VP_PARALLEL:
		//normal sprite
		VectorCopy(vup, up);
		VectorCopy(vright, right);
		break;
	}
	up[0]*=e->scale;
	up[1]*=e->scale;
	up[2]*=e->scale;
	right[0]*=e->scale;
	right[1]*=e->scale;
	right[2]*=e->scale;

	qglColor4fv (e->shaderRGBAf);

	GL_DisableMultitexture();

    GL_Bind(frame->p.d.gl.texnum);

	{
		extern int gldepthfunc;
		qglDepthFunc(gldepthfunc);
		qglDepthMask(0);
		if (gldepthmin == 0.5) 
			qglCullFace ( GL_BACK );
		else
			qglCullFace ( GL_FRONT );

		GL_TexEnv(GL_MODULATE);

		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		qglDisable (GL_ALPHA_TEST);
		qglDisable(GL_BLEND);
	}

	if (e->flags & Q2RF_ADDATIVE)
	{
		qglEnable(GL_BLEND);
		qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE);
	}
	else if (e->shaderRGBAf[3]<1 || gl_blendsprites.value)
	{
		qglEnable(GL_BLEND);
		qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}
	else
		qglEnable (GL_ALPHA_TEST);

	qglDisable(GL_CULL_FACE);
	qglBegin (GL_QUADS);

	qglTexCoord2f (0, 1);
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->left, right, point);
	qglVertex3fv (point);

	qglTexCoord2f (0, 0);
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->left, right, point);
	qglVertex3fv (point);

	qglTexCoord2f (1, 0);
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->right, right, point);
	qglVertex3fv (point);

	qglTexCoord2f (1, 1);
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->right, right, point);
	qglVertex3fv (point);
	
	qglEnd ();

	qglDisable(GL_BLEND);
	qglDisable (GL_ALPHA_TEST);
	qglEnable(GL_DEPTH_TEST);

	qglEnable(GL_CULL_FACE);
	qglEnable(GL_BLEND);

	if (e->flags & Q2RF_ADDATIVE)	//back to regular blending for us!
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

//==================================================================================

void GLR_DrawSprite(int count, void **e, void *parm)
{
	while(count--)
	{
		currententity = *e++;
		qglEnable(GL_TEXTURE_2D);

		R_DrawSpriteModel (currententity);
	}
}
/*
=============
R_DrawEntitiesOnList
=============
*/
void GLR_DrawEntitiesOnList (void)
{
	int		i;

	if (!r_drawentities.value)
		return;

	// draw sprites seperately, because of alpha blending
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = &cl_visedicts[i];

		if (!PPL_ShouldDraw())
			continue;


		switch (currententity->rtype)
		{
		case RT_SPRITE:
			RQ_AddDistReorder(GLR_DrawSprite, currententity, NULL, currententity->origin);
//			R_DrawSpriteModel(currententity);
			continue;
#ifdef Q3SHADERS
		case RT_BEAM:
		case RT_RAIL_RINGS:
		case RT_LIGHTNING:
			R_DrawLightning(currententity);
			continue;
		case RT_RAIL_CORE:
			R_DrawRailCore(currententity);
			continue;
#endif
		case RT_MODEL:	//regular model
			break;
		case RT_PORTALSURFACE:
			continue;	//this doesn't do anything anyway, does it?
		default:
		case RT_POLY:	//these are a little painful, we need to do them some time... just not yet.
			continue;
		}
		if (currententity->flags & Q2RF_BEAM)
		{
			R_DrawBeam(currententity);
			continue;
		}
		if (!currententity->model)
			continue;


		if (cl.lerpents && (cls.allow_anyparticles || currententity->visframe))	//allowed or static
		{
			if (gl_part_flame.value)
			{
				if (currententity->model->engineflags & MDLF_ENGULPHS)
					continue;
			}
		}

		if (currententity->model->engineflags & MDLF_NOTREPLACEMENTS)
		{
			if (currententity->model->fromgame != fg_quake || currententity->model->type != mod_alias)
				if (!ruleset_allow_sensative_texture_replacements.value)
					continue;
		}

		switch (currententity->model->type)
		{
		case mod_alias:
			if (r_refdef.flags & Q2RDF_NOWORLDMODEL || !cl.worldmodel || cl.worldmodel->type != mod_brush || cl.worldmodel->fromgame == fg_doom)
				R_DrawGAliasModel (currententity);
			break;
		
#ifdef HALFLIFEMODELS
		case mod_halflife:
			R_DrawHLModel (currententity);
			break;
#endif

		case mod_brush:
			if (!cl.worldmodel || cl.worldmodel->type != mod_brush || cl.worldmodel->fromgame == fg_doom)
				PPL_BaseBModelTextures (currententity);
			break;

		case mod_sprite:
			RQ_AddDistReorder(GLR_DrawSprite, currententity, NULL, currententity->origin);
			break;

#ifdef TERRAIN
		case mod_heightmap:
			GL_DrawHeightmapModel(currententity);
			break;
#endif

		default:
			break;
		}
	}
}

/*
============
R_PolyBlend
============
*/
void GLV_CalcBlendServer (float colors[4]);
void R_PolyBlend (void)
{
	float shift[4];
	extern qboolean gammaworks;
	if ((!v_blend[3] || !gl_nohwblend.value) && !cl.cshifts[CSHIFT_SERVER].percent)
		return;

	if (r_refdef.flags & Q2RDF_NOWORLDMODEL)
		return;

	GLV_CalcBlendServer(shift);	//figure out the shift we need (normally just the server specified one)

//Con_Printf("R_PolyBlend(): %4.2f %4.2f %4.2f %4.2f\n",shift[0], shift[1],	shift[2],	shift[3]);

	GL_DisableMultitexture();

	qglDisable (GL_ALPHA_TEST);
	qglEnable (GL_BLEND);
	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_TEXTURE_2D);
	
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	qglLoadIdentity ();

	qglRotatef (-90,  1, 0, 0);	    // put Z going up
	 qglRotatef (90,  0, 0, 1);	    // put Z going up

	qglColor4fv (shift);

	qglBegin (GL_QUADS);

	qglVertex3f (10, 100, 100);
	qglVertex3f (10, -100, 100);
	qglVertex3f (10, -100, -100);
	qglVertex3f (10, 100, -100);
	qglEnd ();

	qglDisable (GL_BLEND);
	qglEnable (GL_TEXTURE_2D);
	qglEnable (GL_ALPHA_TEST);
}

void GLR_BrightenScreen (void)
{
	float f;

	RSpeedMark();

	if (gl_contrast.value <= 1.0)
		return;

	if (r_refdef.flags & Q2RDF_NOWORLDMODEL)
		return;

	f = gl_contrast.value;
	f = min (f, 3);

	qglDisable (GL_TEXTURE_2D);
	qglEnable (GL_BLEND);
	qglBlendFunc (GL_DST_COLOR, GL_ONE);
	qglBegin (GL_QUADS);
	while (f > 1) {
		if (f >= 2)
			qglColor3f (1,1,1);
		else
			qglColor3f (f - 1, f - 1, f - 1);
		qglVertex2f (0, 0);
		qglVertex2f (vid.width, 0);
		qglVertex2f (vid.width, vid.height);
		qglVertex2f (0, vid.height);
		f *= 0.5;
	}
	qglEnd ();
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglEnable (GL_TEXTURE_2D);
	qglDisable (GL_BLEND);
	qglColor3f(1, 1, 1);

	RSpeedEnd(RSPEED_PALETTEFLASHES);
}

/*
===============
R_SetupFrame
===============
*/
void GLR_SetupFrame (void)
{
// don't allow cheats in multiplayer
	r_wateralphaval = r_wateralpha.value;
	if (!cls.allow_watervis)
		r_wateralphaval = 1;

	if (!mirror)
	{
		GLR_AnimateLight ();

	// build the transformation matrix for the given view angles

		AngleVectors (r_refdef.viewangles, vpn, vright, vup);

		r_framecount++;
	}
	VectorCopy (r_refdef.vieworg, r_origin);

// current viewleaf
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
		leaf = GLMod_PointInLeaf (cl.worldmodel, r_origin);
		r_viewcluster = r_viewcluster2 = leaf->cluster;

		// check above and below so crossing solid water doesn't draw wrong
		if (!leaf->contents)
		{	// look down a bit
			vec3_t	temp;

			VectorCopy (r_origin, temp);
			temp[2] -= 16;
			leaf = GLMod_PointInLeaf (cl.worldmodel, temp);
			if ( !(leaf->contents & Q2CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2) )
				r_viewcluster2 = leaf->cluster;
		}
		else
		{	// look up a bit
			vec3_t	temp;

			VectorCopy (r_origin, temp);
			temp[2] += 16;
			leaf = GLMod_PointInLeaf (cl.worldmodel, temp);
			if ( !(leaf->contents & Q2CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2) )
				r_viewcluster2 = leaf->cluster;
		}
	}
#endif
	else
	{
		mleaf_t	*leaf;
		vec3_t	temp;

		r_oldviewleaf = r_viewleaf;
		r_oldviewleaf2 = r_viewleaf2;
		r_viewleaf = GLMod_PointInLeaf (cl.worldmodel, r_origin);

		if (!r_viewleaf)
		{
		}
		else if (r_viewleaf->contents == Q1CONTENTS_EMPTY)
		{	//look down a bit			
			VectorCopy (r_origin, temp);
			temp[2] -= 16;
			leaf = GLMod_PointInLeaf (cl.worldmodel, temp);
			if (leaf->contents <= Q1CONTENTS_WATER && leaf->contents >= Q1CONTENTS_LAVA)
				r_viewleaf2 = leaf;
			else
				r_viewleaf2 = NULL;
		}
		else if (r_viewleaf->contents <= Q1CONTENTS_WATER && r_viewleaf->contents >= Q1CONTENTS_LAVA)
		{	//in water, look up a bit.
		
			VectorCopy (r_origin, temp);
			temp[2] += 16;
			leaf = GLMod_PointInLeaf (cl.worldmodel, temp);
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
	GLV_CalcBlend ();

	c_brush_polys = 0;
	c_alias_polys = 0;

}

/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	float	screenaspect;
	extern	int glwidth, glheight;
	int		x, x2, y2, y, w, h;

	float fov_x, fov_y;
	//
	// set up viewpoint
	//
	x = r_refdef.vrect.x * glwidth/(int)vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth/(int)vid.width;
	y = (vid.height-r_refdef.vrect.y) * glheight/(int)vid.height;
	y2 = ((int)vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight/(int)vid.height;

	// fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < glwidth)
		x2++;
	if (y2 < 0)
		y2--;
	if (y < glheight)
		y++;

	w = x2 - x;
	h = y - y2;

	if (envmap)
	{
		x = y2 = 0;
		w = h = 256;
	}

	gl_truescreenrect.x = x;
	gl_truescreenrect.y = y;
	gl_truescreenrect.width = w;
	gl_truescreenrect.height = h;

	qglViewport (glx + x, gly + y2, w, h);

	qglMatrixMode(GL_PROJECTION);

	fov_x = r_refdef.fov_x;//+sin(cl.time)*5;
	fov_y = r_refdef.fov_y;//-sin(cl.time+1)*5;

	if (r_waterwarp.value<0 && r_viewleaf->contents <= Q1CONTENTS_WATER)
	{
		fov_x *= 1 + (((sin(cl.time * 4.7) + 1) * 0.015) * r_waterwarp.value);
		fov_y *= 1 + (((sin(cl.time * 3.0) + 1) * 0.015) * r_waterwarp.value);
	}

	screenaspect = (float)r_refdef.vrect.width/r_refdef.vrect.height;
	if (r_refdef.useperspective)
	{
		if ((!r_shadows.value || !gl_canstencil) && gl_maxdist.value>=100)//gl_nv_range_clamp)
		{
	//		yfov = 2*atan((float)r_refdef.vrect.height/r_refdef.vrect.width)*180/M_PI;
	//		yfov = (2.0 * tan (scr_fov.value/360*M_PI)) / screenaspect;
	//		yfov = 2*atan((float)r_refdef.vrect.height/r_refdef.vrect.width)*(scr_fov.value*2)/M_PI;
	//		MYgluPerspective (yfov,  screenaspect,  4,  4096);

			Matrix4_Projection_Far(r_projection_matrix, fov_x, fov_y, gl_mindist.value, gl_maxdist.value);
		}
		else
		{
			Matrix4_Projection_Inf(r_projection_matrix, fov_x, fov_y, gl_mindist.value);
		}
	}
	else
	{
		if (gl_maxdist.value>=1)
			GL_ParallelPerspective(-fov_x/2, fov_x/2, fov_y/2, -fov_y/2, -gl_maxdist.value, gl_maxdist.value);
		else
			GL_ParallelPerspective(0, r_refdef.vrect.width, 0, r_refdef.vrect.height, -9999, 9999);
	}
	qglLoadMatrixf(r_projection_matrix);

	if (mirror)
	{
//		if (mirror_plane->normal[2])
//			qglScalef (1, -1, 1);
//		else
//			qglScalef (-1, 1, 1);
		qglCullFace(GL_BACK);
	}
	else
	{
#ifdef R_XFLIP
		if (r_xflip.value)
		{
			qglScalef (1, -1, 1);
			qglCullFace(GL_BACK);
		}
		else
#endif
		qglCullFace(GL_FRONT);
	}

	qglMatrixMode(GL_MODELVIEW);


	Matrix4_ModelViewMatrixFromAxis(r_view_matrix, vpn, vright, vup, r_refdef.vieworg);
	qglLoadMatrixf(r_view_matrix);

	//
	// set drawing parms
	//
	if (gl_cull.value)
		qglEnable(GL_CULL_FACE);
	else
		qglDisable(GL_CULL_FACE);

	qglDisable(GL_BLEND);
	qglDisable(GL_ALPHA_TEST);
	qglEnable(GL_DEPTH_TEST);

//#ifndef D3DQUAKE
//	glClearDepth(1.0f);
//#endif

//		if (gl_lightmap_format == GL_LUMINANCE)
//		glBlendFunc (GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
/*	else if (gl_lightmap_format == GL_INTENSITY)
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glColor4f (0,0,0,1);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else if (gl_lightmap_format == GL_RGBA)
	{
		glBlendFunc (GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
	}

  */
	if (gl_dither.value)
	{
		qglEnable(GL_DITHER);
	}
	else
	{
		qglDisable(GL_DITHER);
	}

	GL_DisableMultitexture();
}

/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/
void R_RenderScene (void)
{
	qboolean GLR_DoomWorld(void);

	if (!cl.worldmodel || (!cl.worldmodel->nodes && cl.worldmodel->type != mod_heightmap))
		r_refdef.flags |= Q2RDF_NOWORLDMODEL;

#ifdef NEWBACKEND
	Sh_GenShadowMaps();
#endif

	GLR_SetupFrame ();

	TRACE(("dbg: calling R_SetupGL\n"));
	R_SetupGL ();

	TRACE(("dbg: calling R_SetFrustrum\n"));
	R_SetFrustum ();

	if (!(r_refdef.flags & Q2RDF_NOWORLDMODEL))
	{
#ifdef DOOMWADS
		if (!GLR_DoomWorld ())
#endif
		{
			TRACE(("dbg: calling R_DrawWorld\n"));
			R_DrawWorld ();		// adds static entities to the list
		}
	}

	S_ExtraUpdate ();	// don't let sound get messed up if going slow

	TRACE(("dbg: calling GLR_DrawEntitiesOnList\n"));
	GLR_DrawEntitiesOnList ();

//	R_DrawDecals();

	TRACE(("dbg: calling GL_DisableMultitexture\n"));
	GL_DisableMultitexture();

	TRACE(("dbg: calling R_RenderDlights\n"));
	GLR_RenderDlights ();

	if (!(r_refdef.flags & Q2RDF_NOWORLDMODEL))
	{
		TRACE(("dbg: calling R_DrawParticles\n"));
		P_DrawParticles ();
	}

#ifdef GLTEST
	Test_Draw ();
#endif

}


/*
=============
R_Clear
=============
*/
int gldepthfunc = GL_LEQUAL;
void R_Clear (void)
{
	qglDepthMask(1);
	if (r_mirroralpha.value != 1.0)
	{
		if (gl_clear.value && !r_secondaryview)
			qglClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			qglClear (GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 0.5;
		gldepthfunc=GL_LEQUAL;
	}
#ifdef SIDEVIEWS
	else if (gl_ztrick.value && !gl_ztrickdisabled)
#else
	else if (gl_ztrick.value)
#endif
	{
		static int trickframe;

		if (gl_clear.value && !(r_refdef.flags & Q2RDF_NOWORLDMODEL))
			qglClear (GL_COLOR_BUFFER_BIT);

		trickframe++;
		if (trickframe & 1)
		{
			gldepthmin = 0;
			gldepthmax = 0.49999;
			gldepthfunc=GL_LEQUAL;
		}
		else
		{
			gldepthmin = 1;
			gldepthmax = 0.5;
			gldepthfunc=GL_GEQUAL;
		}
	}
	else
	{
		if (gl_clear.value && !r_secondaryview && !(r_refdef.flags & Q2RDF_NOWORLDMODEL))
			qglClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			qglClear (GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 1;
		gldepthfunc=GL_LEQUAL;
	}

	qglDepthFunc (gldepthfunc);
	qglDepthRange (gldepthmin, gldepthmax);
}

void R_Mirror (void)
{
	msurface_t	*s, *prevs, *prevr, *rejects;
//	entity_t	*ent;
	mplane_t *mirror_plane;

	vec3_t oldangles, oldorg, oldvpn, oldvright, oldvup;	//cache - for rear view mirror and stuff.
	float	base_view_matrix[16];

	if (!mirror)
	{
		r_inmirror = false;
		return;
	}

	r_inmirror = true;

	memcpy(oldangles, r_refdef.viewangles, sizeof(vec3_t));
	memcpy(oldorg, r_refdef.vieworg, sizeof(vec3_t));
	memcpy(oldvpn, vpn, sizeof(vec3_t));
	memcpy(oldvright, vright, sizeof(vec3_t));
	memcpy(oldvup, vup, sizeof(vec3_t));
	memcpy (base_view_matrix, r_view_matrix, sizeof(base_view_matrix));

	s = r_mirror_chain;
	while(s)	//okay, so this is a hack
	{
		s->nextalphasurface = s->texturechain;
		s = s->nextalphasurface;
	}
	cl.worldmodel->textures[mirrortexturenum]->texturechain = NULL;

	while(r_mirror_chain)
	{
		s = r_mirror_chain;
		r_mirror_chain = r_mirror_chain->nextalphasurface;
#if 0
		s->nextalphasurface = NULL;

#else
		//this loop figures out all surfaces with the same plane.
		//yes, this can mean that the list is reversed a few times, but we do have depth testing to solve that anyway.
		for(prevs = s,prevr=NULL,rejects=NULL;r_mirror_chain;r_mirror_chain=r_mirror_chain->nextalphasurface)
		{
			if (s->plane->dist != r_mirror_chain->plane->dist || s->plane->signbits != r_mirror_chain->plane->signbits 
				|| s->plane->normal[0] != r_mirror_chain->plane->normal[0] || s->plane->normal[1] != r_mirror_chain->plane->normal[1] || s->plane->normal[2] != r_mirror_chain->plane->normal[2])
			{	//reject
				if (prevr)
					prevr->nextalphasurface = r_mirror_chain;
				else
					rejects = r_mirror_chain;
				prevr = r_mirror_chain;
			}
			else
			{	//matches
				prevs->nextalphasurface = r_mirror_chain;
				prevs = r_mirror_chain;
			}
		}
		prevs->nextalphasurface = NULL;
		if (prevr)
			prevr->nextalphasurface = NULL;

		r_mirror_chain = rejects;
#endif
		mirror_plane = s->plane;

		//enable stencil writing
		qglClearStencil(0);
		qglClear(GL_STENCIL_BUFFER_BIT);
		qglDisable(GL_ALPHA_TEST);
		qglDisable(GL_STENCIL_TEST);
		qglEnable(GL_STENCIL_TEST);
		qglStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);	//replace where it passes
		qglStencilFunc( GL_ALWAYS, 1, ~0 );	//always pass (where z passes set to 1)
		qglDisable(GL_TEXTURE_2D);
		qglColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
		qglDepthMask( GL_FALSE );

		qglEnableClientState( GL_VERTEX_ARRAY );
		for (prevs = s; s; s=s->nextalphasurface)	//write the polys to the stencil buffer.
		{
			qglVertexPointer(3, GL_FLOAT, 0, s->mesh->xyz_array);
			qglDrawElements(GL_TRIANGLES, s->mesh->numindexes, GL_INDEX_TYPE, s->mesh->indexes);
		}


		qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
		qglStencilFunc( GL_EQUAL, 1, ~0 );	//pass if equal to 1

//now clear the depth buffer where the stencil passed
//we achieve this by changing the projection matrix underneath.
//the stencil only shows where the final surface will appear, and only where not obscured
//we rewrite the depth with the blending pass after.
		qglEnable(GL_DEPTH_TEST);	//use only the stencil test
		qglDepthRange(1, 1);
		qglDepthFunc (GL_ALWAYS);
		qglDepthMask( GL_TRUE );
		qglColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );

		qglMatrixMode(GL_PROJECTION);
		qglLoadIdentity();
		qglOrtho  (0, 1, 1, 0, -99999, 99999);
		qglMatrixMode(GL_MODELVIEW);
		qglLoadIdentity ();

		qglBegin(GL_QUADS);
		qglVertex3f(0, 0, -99999);
		qglVertex3f(1, 0, -99999);
		qglVertex3f(1, 1, -99999);
		qglVertex3f(0, 1, -99999);
		qglEnd();

		qglEnable(GL_DEPTH_TEST);	//use only the stencil test
		qglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
/*
Thus the final mirror matrix for any given plane p*<nx,ny,nz>+k=0 is:

| 1-2*nx*nx    -2*nx*ny     -2*nx*nz     -2*nx*k |

|  -2*ny*nx   1-2*ny*ny     -2*ny*nz     -2*ny*k |

|  -2*nz*nx    -2*nz*ny    1-2*nz*nz     -2*nz*k |

|      0           0            0            1   |
*/
	{
		float mirror[16];
		float view[16];
		float result[16];
		float nx = mirror_plane->normal[0];
		float ny = mirror_plane->normal[1];
		float nz = mirror_plane->normal[2];
		float k = -mirror_plane->dist;

		mirror[0] = 1-2*nx*nx;
		mirror[1] = -2*nx*ny;
		mirror[2] = -2*nx*nz;
		mirror[3] = 0;

		mirror[4] = -2*ny*nx;
		mirror[5] = 1-2*ny*ny;
		mirror[6] = -2*ny*nz;
		mirror[7] = 0;

		mirror[8]  = -2*nz*nx;
		mirror[9]  = -2*nz*ny;
		mirror[10] = 1-2*nz*nz;
		mirror[11] = 0;

		mirror[12] = -2*nx*k;
		mirror[13] = -2*ny*k;
		mirror[14] = -2*nz*k;
		mirror[15] = 1;

		view[0] = oldvpn[0];
		view[1] = oldvpn[1];
		view[2] = oldvpn[2];
		view[3] = 0;

		view[4] = -oldvright[0];
		view[5] = -oldvright[1];
		view[6] = -oldvright[2];
		view[7] = 0;

		view[8]  = oldvup[0];
		view[9]  = oldvup[1];
		view[10] = oldvup[2];
		view[11] = 0;

		view[12] = oldorg[0];
		view[13] = oldorg[1];
		view[14] = oldorg[2];
		view[15] = 1;

		Matrix4_Multiply(mirror, view, result); 

		vpn[0] = result[0];
		vpn[1] = result[1];
		vpn[2] = result[2];

		vright[0] = -result[4];
		vright[1] = -result[5];
		vright[2] = -result[6];

		vup[0] = result[8];
		vup[1] = result[9];
		vup[2] = result[10];

		r_refdef.vieworg[0] = result[12];
		r_refdef.vieworg[1] = result[13];
		r_refdef.vieworg[2] = result[14];
	}

	r_refdef.viewangles[0] = 0;
	r_refdef.viewangles[1] = 0;
	r_refdef.viewangles[2] = 0;


	gldepthmin = 0.5;
	gldepthmax = 1;
	qglDepthRange (gldepthmin, gldepthmax);
	qglDepthFunc (gldepthfunc);

	R_RenderScene ();

//	GLR_DrawWaterSurfaces ();


	gldepthmin = 0;
	gldepthmax = 0.5;
	qglDepthRange (gldepthmin, gldepthmax);
	qglDepthFunc (gldepthfunc);	

	
	memcpy(r_refdef.viewangles, oldangles, sizeof(vec3_t));
	memcpy(r_refdef.vieworg, oldorg, sizeof(vec3_t));

	qglCullFace(GL_FRONT);
	qglMatrixMode(GL_MODELVIEW);

	qglLoadMatrixf (base_view_matrix);

	qglDisable(GL_STENCIL_TEST);

	// blend on top
	qglDisable(GL_ALPHA_TEST);
	qglEnable (GL_BLEND);
	qglEnable(GL_TEXTURE_2D);
	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglColor4f (1,1,1,r_mirroralpha.value);
qglDisable(GL_STENCIL_TEST);
qglPolygonOffset(1, 0);
qglEnable(GL_POLYGON_OFFSET_FILL);
		for (s=prevs ; s ; s=s->nextalphasurface)
		{
			qglEnable (GL_BLEND);
			R_RenderBrushPoly (s);
		}
qglDisable(GL_POLYGON_OFFSET_FILL);
qglPolygonOffset(0, 0);
	qglEnable(GL_TEXTURE_2D);
	qglDisable (GL_BLEND);
	qglColor4f (1,1,1,1);
	}
	qglDisable(GL_STENCIL_TEST);

	memcpy(r_refdef.viewangles, oldangles, sizeof(vec3_t));
	memcpy(r_refdef.vieworg, oldorg, sizeof(vec3_t));

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

	r_inmirror = false;
}
//#endif

#if 0
void GLR_SetupFog (void)
{
	if (r_viewleaf)// && r_viewleaf->contents != CONTENTS_EMPTY)
	{
		//	static fogcolour;
		float fogcol[4]={0};
		float fogperc;
		float fogdist;

		fogperc=0;
		fogdist=512;
		switch(r_viewleaf->contents)
		{
		case FTECONTENTS_WATER:
			fogcol[0] = 64/255.0;
			fogcol[1] = 128/255.0;
			fogcol[2] = 192/255.0;
			fogperc=0.2;
			fogdist=512;
			break;
		case FTECONTENTS_SLIME:
			fogcol[0] = 32/255.0;
			fogcol[1] = 192/255.0;
			fogcol[2] = 92/255.0;
			fogperc=1;
			fogdist=256;
			break;
		case FTECONTENTS_LAVA:
			fogcol[0] = 192/255.0;
			fogcol[1] = 32/255.0;
			fogcol[2] = 64/255.0;
			fogperc=1;
			fogdist=128;
			break;
		default:
			fogcol[0] = 192/255.0;
			fogcol[1] = 192/255.0;
			fogcol[2] = 192/255.0;
			fogperc=1;
			fogdist=1024;
			break;
		}
		if (fogperc)
		{
			qglFogi(GL_FOG_MODE, GL_LINEAR);
			qglFogfv(GL_FOG_COLOR, fogcol);
			qglFogf(GL_FOG_DENSITY, fogperc);
			qglFogf(GL_FOG_START, 1);
			qglFogf(GL_FOG_END, fogdist);
			qglEnable(GL_FOG);
		}
	}
}
#endif

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void GLR_RenderView (void)
{
	extern msurface_t  *r_alpha_surfaces;
	double	time1 = 0, time2;

	if (qglGetError())
		Con_Printf("GL Error before drawing scene\n");

	if (r_norefresh.value || !glwidth || !glheight)
	{
		GL_DoSwap();
		return;
	}

	if (!(r_refdef.flags & Q2RDF_NOWORLDMODEL))
		if (!r_worldentity.model || !cl.worldmodel)
		{
			GL_DoSwap();
			return;
		}
//		Sys_Error ("R_RenderView: NULL worldmodel");



	if (qglPNTrianglesiATI)
	{
		if (gl_ati_truform_type.value)
		{	//linear
			qglPNTrianglesiATI(GL_PN_TRIANGLES_NORMAL_MODE_ATI, GL_PN_TRIANGLES_NORMAL_MODE_LINEAR_ATI);
			qglPNTrianglesiATI(GL_PN_TRIANGLES_POINT_MODE_ATI, GL_PN_TRIANGLES_POINT_MODE_CUBIC_ATI);
		}
		else
		{	//quadric
			qglPNTrianglesiATI(GL_PN_TRIANGLES_NORMAL_MODE_ATI, GL_PN_TRIANGLES_NORMAL_MODE_QUADRATIC_ATI);
			qglPNTrianglesiATI(GL_PN_TRIANGLES_POINT_MODE_ATI, GL_PN_TRIANGLES_POINT_MODE_CUBIC_ATI);
		}
	    qglPNTrianglesfATI(GL_PN_TRIANGLES_TESSELATION_LEVEL_ATI, gl_ati_truform_tesselation.value);
	}

	if (gl_finish.value)
	{
		RSpeedMark();
		qglFinish ();
		RSpeedEnd(RSPEED_FINISH);
	}

	if (r_speeds.value)
	{
		time1 = Sys_DoubleTime ();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	mirror = false;

	R_Clear ();

//	GLR_SetupFog ();

	r_alpha_surfaces = NULL;

	GL_SetShaderState2D(false);

	// render normal view
	R_RenderScene ();	

	GLR_DrawWaterSurfaces ();
	GLR_DrawAlphaSurfaces ();

	// render mirror view
	R_Mirror ();

	R_BloomBlend();

	R_PolyBlend ();

//	qglDisable(GL_FOG);

	if (r_speeds.value)
	{
//		glFinish ();
		time2 = Sys_DoubleTime ();

		RQuantAdd(RQUANT_MSECS, (int)((time2-time1)*1000000));

		RQuantAdd(RQUANT_WPOLYS, c_brush_polys);
		RQuantAdd(RQUANT_EPOLYS, c_alias_polys);
	//	Con_Printf ("%3i ms  %4i wpoly %4i epoly\n", (int)((time2-time1)*1000), c_brush_polys, c_alias_polys); 
	}

	if (qglGetError())
		Con_Printf("GL Error drawing scene\n");

	if (r_refdef.flags & Q2RDF_NOWORLDMODEL)
		return;

	// SCENE POST PROCESSING
	// we check if we need to use any shaders - currently it's just waterwarp
	if (scenepp_ww_program)
	if ((r_waterwarp.value>0 && r_viewleaf && r_viewleaf->contents <= Q1CONTENTS_WATER))
	{
		float vwidth = 1, vheight = 1;
		float vs, vt;

		// get the powers of 2 for the size of the texture that will hold the scene
		while (vwidth < glwidth)
		{
			vwidth *= 2;
		}
		while (vheight < glheight)
		{
			vheight *= 2;
		}

		// get the maxtexcoords while we're at it
		vs = glwidth / vwidth;
		vt = glheight / vheight;

		// 2d mode, but upside down to quake's normal 2d drawing
		// this makes grabbing the sreen a lot easier
		qglViewport (glx, gly, glwidth, glheight);

		qglMatrixMode(GL_PROJECTION);
		// Push the matrices to go into 2d mode, that matches opengl's mode
		qglPushMatrix();
		qglLoadIdentity ();
		// TODO: use actual window width and height
		qglOrtho  (0, glwidth, 0, glheight, -99999, 99999);

		qglMatrixMode(GL_MODELVIEW);
		qglPushMatrix();
		qglLoadIdentity ();

		qglDisable (GL_DEPTH_TEST);
		qglDisable (GL_CULL_FACE);
		qglDisable (GL_BLEND);
		qglEnable (GL_ALPHA_TEST);

		// copy the scene to texture
		GL_Bind(scenepp_texture);
		qglCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, glx, gly, vwidth, vheight, 0);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		if (qglGetError())
			Con_Printf(CON_ERROR "GL Error after qglCopyTexImage2D\n");

		// Here we apply the shaders - currently just waterwarp
		GLSlang_UseProgram(scenepp_ww_program);
		//keep the amp proportional to the size of the scene in texture coords
		// WARNING - waterwarp can change the amplitude, but if it's too big it'll exceed
		// the size determined by the edge texture, after which black bits will be shown.
		// Suggest clamping to a suitable range.
		if (r_waterwarp.value<0)
		{
			GLSlang_SetUniform1f(scenepp_ww_parm_ampscalef, (0.005 / 0.625) * vs*(-r_waterwarp.value));
		}
		else
		{
			GLSlang_SetUniform1f(scenepp_ww_parm_ampscalef, (0.005 / 0.625) * vs*r_waterwarp.value);
		}

		if (qglGetError())
			Con_Printf("GL Error after GLSlang_UseProgram\n");

		{
			float xmin, xmax, ymin, ymax;

			xmin = cl.time * 0.25;
			ymin = cl.time * 0.25;
			xmax = xmin + 1;
			ymax = ymin + 1/vt*vs;

			GL_EnableMultitexture();
			GL_Bind (scenepp_texture_warp);

			GL_SelectTexture(mtexid1+1);
			qglEnable(GL_TEXTURE_2D);
			GL_Bind(scenepp_texture_edge);

			qglBegin(GL_QUADS);

			qglMTexCoord2fSGIS (mtexid0, 0, 0);
			qglMTexCoord2fSGIS (mtexid1, xmin, ymin);
			qglMTexCoord2fSGIS (mtexid1+1, 0, 0);
			qglVertex2f(0, 0);

			qglMTexCoord2fSGIS (mtexid0, vs, 0);
			qglMTexCoord2fSGIS (mtexid1, xmax, ymin);
			qglMTexCoord2fSGIS (mtexid1+1, 1, 0);
			qglVertex2f(glwidth, 0);

			qglMTexCoord2fSGIS (mtexid0, vs, vt);
			qglMTexCoord2fSGIS (mtexid1, xmax, ymax);
			qglMTexCoord2fSGIS (mtexid1+1, 1, 1);
			qglVertex2f(glwidth, glheight);

			qglMTexCoord2fSGIS (mtexid0, 0, vt);
			qglMTexCoord2fSGIS (mtexid1, xmin, ymax);
			qglMTexCoord2fSGIS (mtexid1+1, 0, 1);
			qglVertex2f(0, glheight);
			
			qglEnd();

			qglDisable(GL_TEXTURE_2D);
			GL_SelectTexture(mtexid1);

			GL_DisableMultitexture();
		}

		// Disable shaders
		GLSlang_UseProgram(0);

		// After all the post processing, pop the matrices
		qglMatrixMode(GL_PROJECTION);
		qglPopMatrix();
		qglMatrixMode(GL_MODELVIEW);
		qglPopMatrix();

		if (qglGetError())
			Con_Printf("GL Error after drawing with shaderobjects\n");
	}



	if (gl_motionblur.value>0 && gl_motionblur.value < 1 && qglCopyTexImage2D)
	{
		int vwidth = 1, vheight = 1;
		float vs, vt, cs, ct;

		if (gl_config.arb_texture_non_power_of_two)
		{	//we can use any size, supposedly
			vwidth = glwidth;
			vheight = glheight;
		}
		else
		{	//limit the texture size to square and use padding.
			while (vwidth < glwidth)
				vwidth *= 2;
			while (vheight < glheight)
				vheight *= 2;
		}

		qglViewport (glx, gly, glwidth, glheight);

		GL_Bind(sceneblur_texture);

		// go 2d
		qglMatrixMode(GL_PROJECTION);
		qglPushMatrix();
		qglLoadIdentity ();
		qglOrtho  (0, glwidth, 0, glheight, -99999, 99999);
		qglMatrixMode(GL_MODELVIEW);
		qglPushMatrix();
		qglLoadIdentity ();

		//blend the last frame onto the scene
		//the maths is because our texture is over-sized (must be power of two)
		cs = vs = (float)glwidth / vwidth * 0.5;
		ct = vt = (float)glheight / vheight * 0.5;
		vs *= gl_motionblurscale.value;
		vt *= gl_motionblurscale.value;

		qglDisable (GL_DEPTH_TEST);
		qglDisable (GL_CULL_FACE);
		qglDisable (GL_ALPHA_TEST);
		qglEnable(GL_BLEND);
		qglColor4f(1, 1, 1, gl_motionblur.value);
		qglBegin(GL_QUADS);
		qglTexCoord2f(cs-vs, ct-vt);
		qglVertex2f(0, 0);
		qglTexCoord2f(cs+vs, ct-vt);
		qglVertex2f(glwidth, 0);
		qglTexCoord2f(cs+vs, ct+vt);
		qglVertex2f(glwidth, glheight);
		qglTexCoord2f(cs-vs, ct+vt);
		qglVertex2f(0, glheight);
		qglEnd();

		qglMatrixMode(GL_PROJECTION);
		qglPopMatrix();
		qglMatrixMode(GL_MODELVIEW);
		qglPopMatrix();


		//copy the image into the texture so that we can play with it next frame too!
		qglCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, glx, gly, vwidth, vheight, 0);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
}

#endif
