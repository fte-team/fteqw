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

#ifdef GLQUAKE
#include "glquake.h"
#include "renderque.h"
#include "shader.h"
#include "gl_draw.h"

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

int			r_visframecount;	// bumped when going to a new PVS
extern int			r_framecount;		// used for dlight push checking

float		r_wateralphaval;	//allowed or not...

//mplane_t	frustum[4];

int			c_brush_polys, c_alias_polys;

qboolean	envmap;				// true during envmap command capture

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

#pragma message("r_polygonoffset_submodel_offset: not implemented at the mo")
cvar_t	r_polygonoffset_submodel_factor = SCVAR("r_polygonoffset_submodel_factor", "0.05");
cvar_t	r_polygonoffset_submodel_offset = SCVAR("r_polygonoffset_submodel_offset", "25");

extern cvar_t	gl_contrast;
extern cvar_t	gl_mindist;

extern cvar_t	ffov;

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
texid_t sceneblur_texture;
texid_t scenepp_texture;
texid_t scenepp_texture_warp;
texid_t scenepp_texture_edge;

int scenepp_ww_program;
int scenepp_ww_parm_texture0i;
int scenepp_ww_parm_texture1i;
int scenepp_ww_parm_texture2i;
int scenepp_ww_parm_ampscalef;

int scenepp_mt_program;
int scenepp_mt_parm_texture0i;
int scenepp_mt_parm_colorf;
int scenepp_mt_parm_inverti;

texid_t scenepp_fisheye_texture;
int scenepp_fisheye_program;
int scenepp_fisheye_parm_fov;
int scenepp_panorama_program;
int scenepp_panorama_parm_fov;

shader_t *shader_brighten;
shader_t *shader_polyblend;

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

void GL_InitFisheyeFov(void)
{
	char *vshader = "\
		varying vec2 texcoord;\
		void main(void)\
		{\
			texcoord = gl_MultiTexCoord0.xy;\
			gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\
		}";
	char *fisheyefshader = "\
		uniform samplerCube source;\
		varying vec2 texcoord;\
		uniform float fov;\
		void main(void)\
		{\
			vec3 tc;	\
			vec2 d;	\
			vec2 ang;	\
			d = texcoord;	\
			ang.x = sqrt(d.x*d.x+d.y*d.y)*fov;	\
			ang.y = -atan(d.y, d.x);	\
			tc.x = sin(ang.x) * cos(ang.y);	\
			tc.y = sin(ang.x) * sin(ang.y);	\
			tc.z = cos(ang.x);	\
			gl_FragColor = textureCube(source, tc);\
		}";

	char *panoramafshader = "\
		uniform samplerCube source;\
		varying vec2 texcoord;\
		uniform float fov;\
		void main(void)\
		{\
			vec3 tc;	\
			float ang;	\
			ang = texcoord.x*fov;	\
			tc.x = sin(ang);	\
			tc.y = -texcoord.y;	\
			tc.z = cos(ang);	\
			gl_FragColor = textureCube(source, tc);\
		}";

	scenepp_fisheye_program = GLSlang_CreateProgram(NULL, vshader, fisheyefshader);
	if (scenepp_fisheye_program)
	{
		GLSlang_UseProgram(scenepp_fisheye_program);
		GLSlang_SetUniform1i(GLSlang_GetUniformLocation(scenepp_fisheye_program, "source"), 0);
		scenepp_fisheye_parm_fov = GLSlang_GetUniformLocation(scenepp_fisheye_program, "fov");
		GLSlang_UseProgram(0);
	}

	scenepp_panorama_program = GLSlang_CreateProgram(NULL, vshader, panoramafshader);
	if (scenepp_panorama_program)
	{
		GLSlang_UseProgram(scenepp_panorama_program);
		GLSlang_SetUniform1i(GLSlang_GetUniformLocation(scenepp_panorama_program, "source"), 0);
		scenepp_panorama_parm_fov = GLSlang_GetUniformLocation(scenepp_panorama_program, "fov");
		GLSlang_UseProgram(0);
	}
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
	shader_brighten = R_RegisterShader("constrastshader", 
			"{\n"
				"{\n"
					"map $whiteimage\n"
					"blendfunc gl_dst_color gl_one\n"
					"rgbgen vertex\n"
					"alphagen vertex\n"
				"}\n"
			"}\n"
	);
	shader_polyblend = R_RegisterShader("polyblendshader",
			"{\n"
				"{\n"
					"map $whiteimage\n"
					"blendfunc gl_src_alpha gl_one_minus_src_alpha\n"
					"rgbgen vertex\n"
					"alphagen vertex\n"
				"}\n"
			"}\n"
	);


	if (gl_config.arb_shader_objects)
	{
		GL_InitSceneProcessingShaders_WaterWarp();
		GL_InitFisheyeFov();
		GL_InitSceneProcessingShaders_MenuTint();
	}
}

#define PP_WARP_TEX_SIZE 64
#define PP_AMP_TEX_SIZE 64
#define PP_AMP_TEX_BORDER 4
void GL_SetupSceneProcessingTextures (void)
{
	int i, x, y;
	unsigned char pp_warp_tex[PP_WARP_TEX_SIZE*PP_WARP_TEX_SIZE*3];
	unsigned char pp_edge_tex[PP_AMP_TEX_SIZE*PP_AMP_TEX_SIZE*3];

	scenepp_fisheye_texture = r_nulltex;

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
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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
=================
R_DrawSpriteModel

=================
*/
void R_DrawSpriteModel (entity_t *e)
{
	vec3_t	point;
	mspriteframe_t	*frame, genframe;
	vec3_t		forward, right, up;
	msprite_t		*psprite;
	vec3_t sprorigin;
	unsigned int fl;
	unsigned int sprtype;

	static vec2_t texcoords[4]={{0, 1},{0,0},{1,0},{1,1}};
	static index_t indexes[6] = {0, 1, 2, 0, 2, 3};
	vecV_t vertcoords[4];
	avec4_t colours[4];
	mesh_t mesh;


	if (e->flags & Q2RF_WEAPONMODEL && r_refdef.currentplayernum >= 0)
	{
		sprorigin[0] = cl.viewent[r_refdef.currentplayernum].origin[0];
		sprorigin[1] = cl.viewent[r_refdef.currentplayernum].origin[1];
		sprorigin[2] = cl.viewent[r_refdef.currentplayernum].origin[2];
		VectorMA(sprorigin, e->origin[0], cl.viewent[r_refdef.currentplayernum].axis[0], sprorigin);
		VectorMA(sprorigin, e->origin[1], cl.viewent[r_refdef.currentplayernum].axis[1], sprorigin);
		VectorMA(sprorigin, e->origin[2], cl.viewent[r_refdef.currentplayernum].axis[2], sprorigin);
		VectorMA(sprorigin, 12, vpn, sprorigin);

		e->flags |= RF_NODEPTHTEST;
	}
	else
		VectorCopy(e->origin, sprorigin);

	if (!e->model || e->forcedshader)
	{
		genframe.shader = e->forcedshader;
		genframe.up = genframe.left = -1;
		genframe.down = genframe.right = 1;
		sprtype = SPR_VP_PARALLEL;
		frame = &genframe;
	}
	else
	{
		// don't even bother culling, because it's just a single
		// polygon without a surface cache
		frame = R_GetSpriteFrame (e);
		psprite = e->model->cache.data;
		sprtype = psprite->type;
	}
	if (!frame->shader)
		return;

	switch(sprtype)
	{
	case SPR_ORIENTED:
		// bullet marks on walls
		AngleVectors (e->angles, forward, right, up);
		break;

	case SPR_FACING_UPRIGHT:
		up[0] = 0;up[1] = 0;up[2]=1;
		right[0] = sprorigin[1] - r_origin[1];
		right[1] = -(sprorigin[0] - r_origin[0]);
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


	Vector4Copy(e->shaderRGBAf, colours[0]);
	Vector4Copy(e->shaderRGBAf, colours[1]);
	Vector4Copy(e->shaderRGBAf, colours[2]);
	Vector4Copy(e->shaderRGBAf, colours[3]);

	fl = 0;
	if (e->flags & Q2RF_ADDITIVE)
		fl |= BEF_FORCEADDITIVE;
	if (e->shaderRGBAf[3]<1 || gl_blendsprites.value)
		fl |= BEF_FORCETRANSPARENT;
	if (e->flags & RF_NODEPTHTEST)
		fl |= BEF_FORCENODEPTH;
	BE_SelectMode(BEM_STANDARD, fl);

	VectorMA (sprorigin, frame->down, up, point);
	VectorMA (point, frame->left, right, vertcoords[0]);

	VectorMA (sprorigin, frame->up, up, point);
	VectorMA (point, frame->left, right, vertcoords[1]);

	VectorMA (sprorigin, frame->up, up, point);
	VectorMA (point, frame->right, right, vertcoords[2]);

	VectorMA (sprorigin, frame->down, up, point);
	VectorMA (point, frame->right, right, vertcoords[3]);


	memset(&mesh, 0, sizeof(mesh));
	mesh.vbofirstelement = 0;
	mesh.vbofirstvert = 0;
	mesh.xyz_array = vertcoords;
	mesh.indexes = indexes;
	mesh.numindexes = sizeof(indexes)/sizeof(indexes[0]);
	mesh.colors4f_array = colours;
	mesh.lmst_array = NULL;
	mesh.normals_array = NULL;
	mesh.numvertexes = 4;
	mesh.st_array = texcoords;
	mesh.istrifan = true;
	BE_DrawMeshChain(frame->shader, &mesh, NULL, &frame->shader->defaulttextures);
}

//==================================================================================

void GLR_DrawSprite(int count, void **e, void *parm)
{
	while(count--)
	{
#pragma message("this needs merging or q3 railgun will lag like hell")
		currententity = *e++;

		R_DrawSpriteModel (currententity);
	}
}


#ifdef Q3CLIENT

//q3 lightning gun
void R_DrawLightning(entity_t *e)
{
	vec3_t v;
	vec3_t dir, cr;
	float scale = e->scale;
	float length;

	vecV_t points[4];
	vec2_t texcoords[4] = {{0, 0}, {0, 1}, {1, 1}, {1, 0}};
	index_t indexarray[6] = {0, 1, 2, 0, 2, 3};

	mesh_t mesh;

	if (!e->forcedshader)
		return;

	if (!scale)
		scale = 10;


	VectorSubtract(e->origin, e->oldorigin, dir);
	length = Length(dir);

	//this seems to be about right.
	texcoords[2][0] = length/128;
	texcoords[3][0] = length/128;

	VectorSubtract(r_refdef.vieworg, e->origin, v);
	CrossProduct(v, dir, cr);
	VectorNormalize(cr);

	VectorMA(e->origin, -scale/2, cr, points[0]);
	VectorMA(e->origin, scale/2, cr, points[1]);

	VectorSubtract(r_refdef.vieworg, e->oldorigin, v);
	CrossProduct(v, dir, cr);
	VectorNormalize(cr);

	VectorMA(e->oldorigin, scale/2, cr, points[2]);
	VectorMA(e->oldorigin, -scale/2, cr, points[3]);

	memset(&mesh, 0, sizeof(mesh));
	mesh.vbofirstelement = 0;
	mesh.vbofirstvert = 0;
	mesh.xyz_array = points;
	mesh.indexes = indexarray;
	mesh.numindexes = sizeof(indexarray)/sizeof(indexarray[0]);
	mesh.colors4f_array = NULL;
	mesh.lmst_array = NULL;
	mesh.normals_array = NULL;
	mesh.numvertexes = 4;
	mesh.st_array = texcoords;
	BE_DrawMeshChain(e->forcedshader, &mesh, NULL, NULL);
}
//q3 railgun beam
void R_DrawRailCore(entity_t *e)
{
	vec3_t v;
	vec3_t dir, cr;
	float scale = e->scale;
	float length;

	mesh_t mesh;
	vecV_t points[4];
	vec2_t texcoords[4] = {{0, 0}, {0, 1}, {1, 1}, {1, 0}};
	index_t indexarray[6] = {0, 1, 2, 0, 2, 3};
	vec4_t colors[4];

	if (!e->forcedshader)
		return;

	if (!scale)
		scale = 10;


	VectorSubtract(e->origin, e->oldorigin, dir);
	length = Length(dir);

	//this seems to be about right.
	texcoords[2][0] = length/128;
	texcoords[3][0] = length/128;

	VectorSubtract(r_refdef.vieworg, e->origin, v);
	CrossProduct(v, dir, cr);
	VectorNormalize(cr);

	VectorMA(e->origin, -scale/2, cr, points[0]);
	VectorMA(e->origin, scale/2, cr, points[1]);

	VectorSubtract(r_refdef.vieworg, e->oldorigin, v);
	CrossProduct(v, dir, cr);
	VectorNormalize(cr);

	VectorMA(e->oldorigin, scale/2, cr, points[2]);
	VectorMA(e->oldorigin, -scale/2, cr, points[3]);

	Vector4Copy(e->shaderRGBAf, colors[0]);
	Vector4Copy(e->shaderRGBAf, colors[1]);
	Vector4Copy(e->shaderRGBAf, colors[2]);
	Vector4Copy(e->shaderRGBAf, colors[3]);

	memset(&mesh, 0, sizeof(mesh));
	mesh.vbofirstelement = 0;
	mesh.vbofirstvert = 0;
	mesh.xyz_array = points;
	mesh.indexes = indexarray;
	mesh.numindexes = sizeof(indexarray)/sizeof(indexarray[0]);
	mesh.colors4f_array = (vec4_t*)colors;
	mesh.lmst_array = NULL;
	mesh.normals_array = NULL;
	mesh.numvertexes = 4;
	mesh.st_array = texcoords;

	BE_DrawMeshChain(e->forcedshader, &mesh, NULL, NULL);
}
#endif

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
#ifdef Q3CLIENT
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
				R_DrawGAliasModel (currententity, BEM_STANDARD);
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
//bright flashes and stuff
void R_PolyBlend (void)
{
	if (!sw_blend[3])
		return;

	if (r_refdef.flags & Q2RDF_NOWORLDMODEL)
		return;

	R2D_ImageColours (sw_blend[0], sw_blend[1], sw_blend[2], sw_blend[3]);
	R2D_ScalePic(0, 0, vid.width, vid.height, shader_polyblend);
}

//for lack of hardware gamma
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

	while (f > 1)
	{
		if (f >= 2)
			R2D_ImageColours (1, 1, 1, 1);
		else
			R2D_ImageColours (f - 1, f - 1, f - 1, 1);
		R2D_ScalePic(0, 0, vid.width, vid.height, shader_brighten);
		f *= 0.5;
	}

	RSpeedEnd(RSPEED_PALETTEFLASHES);
}

/*
===============
R_SetupFrame
===============
*/
static void GLR_SetupFrame (void)
{
// don't allow cheats in multiplayer
	r_wateralphaval = r_wateralpha.value;
	if (!cls.allow_watervis)
		r_wateralphaval = 1;

	if (!mirror)
	{
		R_AnimateLight ();

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
		leaf = RMod_PointInLeaf (cl.worldmodel, r_origin);
		r_viewcluster = r_viewcluster2 = leaf->cluster;

		// check above and below so crossing solid water doesn't draw wrong
		if (!leaf->contents)
		{	// look down a bit
			vec3_t	temp;

			VectorCopy (r_origin, temp);
			temp[2] -= 16;
			leaf = RMod_PointInLeaf (cl.worldmodel, temp);
			if ( !(leaf->contents & Q2CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2) )
				r_viewcluster2 = leaf->cluster;
		}
		else
		{	// look up a bit
			vec3_t	temp;

			VectorCopy (r_origin, temp);
			temp[2] += 16;
			leaf = RMod_PointInLeaf (cl.worldmodel, temp);
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
		r_viewleaf = RMod_PointInLeaf (cl.worldmodel, r_origin);

		if (!r_viewleaf)
		{
		}
		else if (r_viewleaf->contents == Q1CONTENTS_EMPTY)
		{	//look down a bit
			VectorCopy (r_origin, temp);
			temp[2] -= 16;
			leaf = RMod_PointInLeaf (cl.worldmodel, temp);
			if (leaf->contents <= Q1CONTENTS_WATER && leaf->contents >= Q1CONTENTS_LAVA)
				r_viewleaf2 = leaf;
			else
				r_viewleaf2 = NULL;
		}
		else if (r_viewleaf->contents <= Q1CONTENTS_WATER && r_viewleaf->contents >= Q1CONTENTS_LAVA)
		{	//in water, look up a bit.

			VectorCopy (r_origin, temp);
			temp[2] += 16;
			leaf = RMod_PointInLeaf (cl.worldmodel, temp);
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
	int		x, x2, y2, y, w, h;

	float fov_x, fov_y;

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);
	VectorCopy (r_refdef.vieworg, r_origin);

	//
	// set up viewpoint
	//
	x = r_refdef.vrect.x * vid.pixelwidth/(int)vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * vid.pixelwidth/(int)vid.width;
	y = (vid.height-r_refdef.vrect.y) * vid.pixelheight/(int)vid.height;
	y2 = ((int)vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)) * vid.pixelheight/(int)vid.height;

	// fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < vid.pixelwidth)
		x2++;
	if (y2 < 0)
		y2--;
	if (y < vid.pixelheight)
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

	qglViewport (x, y2, w, h);

	qglMatrixMode(GL_PROJECTION);

	fov_x = r_refdef.fov_x;//+sin(cl.time)*5;
	fov_y = r_refdef.fov_y;//-sin(cl.time+1)*5;

	if (r_waterwarp.value<0 && r_viewleaf && r_viewleaf->contents <= Q1CONTENTS_WATER)
	{
		fov_x *= 1 + (((sin(cl.time * 4.7) + 1) * 0.015) * r_waterwarp.value);
		fov_y *= 1 + (((sin(cl.time * 3.0) + 1) * 0.015) * r_waterwarp.value);
	}

	screenaspect = (float)r_refdef.vrect.width/r_refdef.vrect.height;
	if (r_refdef.useperspective)
	{
		int stencilshadows = 0;
#ifdef RTLIGHTS
		stencilshadows |= r_shadow_realtime_dlight.value && r_shadow_realtime_dlight_shadows.value;
		stencilshadows |= r_shadow_realtime_world.value && r_shadow_realtime_world_shadows.value;
#endif

		if ((!stencilshadows || !gl_canstencil) && gl_maxdist.value>=100)//gl_nv_range_clamp)
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

	qglMatrixMode(GL_MODELVIEW);


	Matrix4_ModelViewMatrixFromAxis(r_view_matrix, vpn, vright, vup, r_refdef.vieworg);
	qglLoadMatrixf(r_view_matrix);

	if (gl_dither.value)
	{
		qglEnable(GL_DITHER);
	}
	else
	{
		qglDisable(GL_DITHER);
	}
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

#ifdef RTLIGHTS
	if (!(r_refdef.flags & Q2RDF_NOWORLDMODEL))
		Sh_GenShadowMaps();
#endif

	TRACE(("dbg: calling R_SetupGL\n"));
	R_SetupGL ();

	TRACE(("dbg: calling R_SetFrustrum\n"));
	R_SetFrustum (r_projection_matrix, r_view_matrix);

	if (!(r_refdef.flags & Q2RDF_NOWORLDMODEL))
	{
#ifdef DOOMWADS
		if (!GLR_DoomWorld ())
#endif
		{
			TRACE(("dbg: calling R_DrawWorld\n"));
			Surf_DrawWorld ();		// adds static entities to the list
		}
	}

	S_ExtraUpdate ();	// don't let sound get messed up if going slow

	TRACE(("dbg: calling GLR_DrawEntitiesOnList\n"));
	GLR_DrawEntitiesOnList ();

//	R_DrawDecals();

	TRACE(("dbg: calling R_RenderDlights\n"));
	GLR_RenderDlights ();

	if (!(r_refdef.flags & Q2RDF_NOWORLDMODEL))
	{
		TRACE(("dbg: calling R_DrawParticles\n"));
		P_DrawParticles ();
	}
	RQ_RenderBatchClear();
}


/*
=============
R_Clear
=============
*/
int gldepthfunc = GL_LEQUAL;
void R_Clear (void)
{
	/*tbh, this entire function should be in the backend*/
	GL_ForceDepthWritable();
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

#pragma message("backend fixme")
	Con_Printf("mirrors are not updated for the backend\n");

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
			qglVertexPointer(3, GL_FLOAT, sizeof(vecV_t), s->mesh->xyz_array);
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
			//R_RenderBrushPoly (s);
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

static void R_RenderMotionBlur(void)
{
	int vwidth = 1, vheight = 1;
	float vs, vt, cs, ct;
#pragma message("backend fixme")
	Con_Printf("motionblur is not updated for the backend\n");

	if (gl_config.arb_texture_non_power_of_two)
	{	//we can use any size, supposedly
		vwidth = vid.pixelwidth;
		vheight = vid.pixelheight;
	}
	else
	{	//limit the texture size to square and use padding.
		while (vwidth < vid.pixelwidth)
			vwidth *= 2;
		while (vheight < vid.pixelheight)
			vheight *= 2;
	}

	qglViewport (0, 0, vid.pixelwidth, vid.pixelheight);

	PPL_RevertToKnownState();

	GL_Bind(sceneblur_texture);

	// go 2d
	qglMatrixMode(GL_PROJECTION);
	qglPushMatrix();
	qglLoadIdentity ();
	qglOrtho  (0, vid.pixelwidth, 0, vid.pixelheight, -99999, 99999);
	qglMatrixMode(GL_MODELVIEW);
	qglPushMatrix();
	qglLoadIdentity ();

	//blend the last frame onto the scene
	//the maths is because our texture is over-sized (must be power of two)
	cs = vs = (float)vid.pixelwidth / vwidth * 0.5;
	ct = vt = (float)vid.pixelheight / vheight * 0.5;
	vs *= gl_motionblurscale.value;
	vt *= gl_motionblurscale.value;

	qglDisable (GL_DEPTH_TEST);
	GL_CullFace(0);
	qglDisable (GL_ALPHA_TEST);
	qglEnable(GL_BLEND);
	qglColor4f(1, 1, 1, gl_motionblur.value);
	qglBegin(GL_QUADS);
	qglTexCoord2f(cs-vs, ct-vt);
	qglVertex2f(0, 0);
	qglTexCoord2f(cs+vs, ct-vt);
	qglVertex2f(vid.pixelwidth, 0);
	qglTexCoord2f(cs+vs, ct+vt);
	qglVertex2f(vid.pixelwidth, vid.pixelheight);
	qglTexCoord2f(cs-vs, ct+vt);
	qglVertex2f(0, vid.pixelheight);
	qglEnd();

	qglMatrixMode(GL_PROJECTION);
	qglPopMatrix();
	qglMatrixMode(GL_MODELVIEW);
	qglPopMatrix();


	//copy the image into the texture so that we can play with it next frame too!
	qglCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, vwidth, vheight, 0);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	PPL_RevertToKnownState();
}

static void R_RenderWaterWarp(void)
{
	float vwidth = 1, vheight = 1;
	float vs, vt;

	PPL_RevertToKnownState();

#pragma message("backend fixme")
	Con_Printf("waterwarp is not updated for the backend\n");

	// get the powers of 2 for the size of the texture that will hold the scene

	if (gl_config.arb_texture_non_power_of_two)
	{
		vwidth = vid.pixelwidth;
		vheight = vid.pixelheight;
	}
	else
	{
		while (vwidth < vid.pixelwidth)
		{
			vwidth *= 2;
		}
		while (vheight < vid.pixelheight)
		{
			vheight *= 2;
		}
	}

	// get the maxtexcoords while we're at it
	vs = vid.pixelwidth / vwidth;
	vt = vid.pixelheight / vheight;

	// 2d mode, but upside down to quake's normal 2d drawing
	// this makes grabbing the sreen a lot easier
	qglViewport (0, 0, vid.pixelwidth, vid.pixelheight);

	qglMatrixMode(GL_PROJECTION);
	// Push the matrices to go into 2d mode, that matches opengl's mode
	qglPushMatrix();
	qglLoadIdentity ();
	// TODO: use actual window width and height
	qglOrtho  (0, vid.pixelwidth, 0, vid.pixelheight, -99999, 99999);

	qglMatrixMode(GL_MODELVIEW);
	qglPushMatrix();
	qglLoadIdentity ();

	qglDisable (GL_DEPTH_TEST);
	GL_CullFace(0);
	qglDisable (GL_BLEND);
	qglEnable (GL_ALPHA_TEST);

	// copy the scene to texture
	GL_Bind(scenepp_texture);
	qglEnable(GL_TEXTURE_2D);
	qglCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, vwidth, vheight, 0);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

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

		GL_SelectTexture(1);
		qglEnable(GL_TEXTURE_2D);
		GL_Bind (scenepp_texture_warp);

		GL_SelectTexture(2);
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
		qglVertex2f(vid.pixelwidth, 0);

		qglMTexCoord2fSGIS (mtexid0, vs, vt);
		qglMTexCoord2fSGIS (mtexid1, xmax, ymax);
		qglMTexCoord2fSGIS (mtexid1+1, 1, 1);
		qglVertex2f(vid.pixelwidth, vid.pixelheight);

		qglMTexCoord2fSGIS (mtexid0, 0, vt);
		qglMTexCoord2fSGIS (mtexid1, xmin, ymax);
		qglMTexCoord2fSGIS (mtexid1+1, 0, 1);
		qglVertex2f(0, vid.pixelheight);

		qglEnd();

		qglDisable(GL_TEXTURE_2D);
		GL_SelectTexture(1);
		qglDisable(GL_TEXTURE_2D);
		GL_SelectTexture(0);
	}

	// Disable shaders
	GLSlang_UseProgram(0);

	// After all the post processing, pop the matrices
	qglMatrixMode(GL_PROJECTION);
	qglPopMatrix();
	qglMatrixMode(GL_MODELVIEW);
	qglPopMatrix();

	PPL_RevertToKnownState();

	if (qglGetError())
		Con_Printf("GL Error after drawing with shaderobjects\n");
}

#ifdef FISH
qboolean R_RenderScene_Fish(void)
{
	int cmapsize = 512;
	int i;
	static vec3_t ang[6] =
				{	{0, -90, 0}, {0, 90, 0},
					{90, 0, 0}, {-90, 0, 0},
					{0, 0, 0}, {0, -180, 0}	};
	int order[6] = {4, 0, 1, 5, 3, 2};
	int numsides = 4;
	vec3_t saveang;
	int rot45 = 0;

#pragma message("backend fixme")
	Con_Printf("fisheye/panorama is not updated for the backend\n");

	if (!scenepp_panorama_program)
		return false;

	if (gl_config.arb_texture_non_power_of_two)
	{
		if (vid.pixelwidth < vid.pixelheight)
			cmapsize = vid.pixelwidth;
		else
			cmapsize = vid.pixelheight;
	}
	else
	{
		while (cmapsize > vid.pixelwidth || cmapsize > vid.pixelheight)
		{
			cmapsize /= 2;
		}
	}

	VectorCopy(r_refdef.viewangles, saveang);
	saveang[2] = 0;

	if (ffov.value < 0)
	{
		//panoramic view needs at most the four sides
		if (ffov.value >= -90)
			numsides = 1;
//			else if (ffov.value >= -180)
//			{
//				numsides = 2;
//				rot45 = 1;
//			}
		else if (ffov.value >= -270)
			numsides = 3;
		else
			numsides = 4;

		order[0] = 4;
		order[1] = 0;
		order[2] = 1;
		order[3] = 5;
	}
	else
	{
		//fisheye view sees a full sphere
		//
		if (ffov.value <= 77)
			numsides = 1;
//			else if (ffov.value <= 180)
//			{
//				numsides = 3;
//				rot45 = 3;
//			}
		else if (ffov.value <= 270)
			numsides = 5;
		else
			numsides = 6;

		order[0] = 4;
		order[1] = 0;
		order[2] = 3;
		order[3] = 1;
		order[4] = 2;
		order[5] = 5;
	}

	qglViewport (0, vid.pixelheight - cmapsize, cmapsize, cmapsize);

	if (!TEXVALID(scenepp_fisheye_texture))
	{
		scenepp_fisheye_texture = GL_AllocNewTexture();

		qglDisable(GL_TEXTURE_2D);
		qglEnable(GL_TEXTURE_CUBE_MAP_ARB);

		GL_BindType(GL_TEXTURE_CUBE_MAP_ARB, scenepp_fisheye_texture);
		for (i = 0; i < 6; i++)
			qglCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + i, 0, GL_RGB, 0, 0, cmapsize, cmapsize, 0);
		qglTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		qglTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		qglTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		qglTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		qglEnable(GL_TEXTURE_2D);
		qglDisable(GL_TEXTURE_CUBE_MAP_ARB);
	}

	r_refdef.vrect.width = (cmapsize+0.99)*vid.width/vid.pixelwidth;
	r_refdef.vrect.height = (cmapsize+0.99)*vid.height/vid.pixelheight;
	r_refdef.vrect.x = 0;
	r_refdef.vrect.y = vid.height - r_refdef.vrect.height;

	ang[0][0] = -saveang[0];
	ang[0][1] = -90;
	ang[0][2] = -saveang[0];

	ang[1][0] = -saveang[0];
	ang[1][1] = 90;
	ang[1][2] = saveang[0];
	ang[5][0] = -saveang[0]*2;
	for (i = 0; i < numsides; i++)
	{
		mirror = false;

		r_refdef.fov_x = 90;
		r_refdef.fov_y = 90;
		r_refdef.viewangles[0] = saveang[0]+ang[order[i]][0];
		r_refdef.viewangles[1] = saveang[1]+ang[order[i]][1];
		r_refdef.viewangles[2] = saveang[2]+ang[order[i]][2];

		R_Clear ();

	//	GLR_SetupFog ();

		GL_SetShaderState2D(false);

		// render normal view
		R_RenderScene ();

		// render mirror view
		R_Mirror ();

		qglDisable(GL_TEXTURE_2D);
		qglEnable(GL_TEXTURE_CUBE_MAP_ARB);
		GL_BindType(GL_TEXTURE_CUBE_MAP_ARB, scenepp_fisheye_texture);
		qglCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + order[i], 0, 0, 0, 0, 0, cmapsize, cmapsize);
		qglEnable(GL_TEXTURE_2D);
		qglDisable(GL_TEXTURE_CUBE_MAP_ARB);
	}

//qglClear (GL_COLOR_BUFFER_BIT);
	qglViewport (0, 0, vid.pixelwidth, vid.pixelheight);

	qglDisable(GL_TEXTURE_2D);
	GL_BindType(GL_TEXTURE_CUBE_MAP_ARB, scenepp_fisheye_texture);
	qglEnable(GL_TEXTURE_CUBE_MAP_ARB);

	if (scenepp_panorama_program && ffov.value < 0)
	{
		GLSlang_UseProgram(scenepp_panorama_program);
		GLSlang_SetUniform1f(scenepp_panorama_parm_fov, -ffov.value*3.1415926535897932384626433832795/180);
	}
	else
	{
		GLSlang_UseProgram(scenepp_fisheye_program);
		GLSlang_SetUniform1f(scenepp_fisheye_parm_fov, ffov.value*3.1415926535897932384626433832795/180);
	}


	// go 2d
	qglMatrixMode(GL_PROJECTION);
	qglPushMatrix();
	qglLoadIdentity ();
	qglOrtho  (0, vid.pixelwidth, 0, vid.pixelheight, -99999, 99999);
	qglMatrixMode(GL_MODELVIEW);
	qglPushMatrix();
	qglLoadIdentity ();

	qglDisable (GL_DEPTH_TEST);
	GL_CullFace(0);
	qglDisable (GL_ALPHA_TEST);
	qglDisable(GL_BLEND);
	qglBegin(GL_QUADS);
	qglTexCoord2f(-0.5, -0.5);
	qglVertex2f(0, 0);
	qglTexCoord2f(0.5, -0.5);
	qglVertex2f(vid.pixelwidth, 0);
	qglTexCoord2f(0.5, 0.5);
	qglVertex2f(vid.pixelwidth, vid.pixelheight);
	qglTexCoord2f(-0.5, 0.5);
	qglVertex2f(0, vid.pixelheight);
	qglEnd();

	qglMatrixMode(GL_PROJECTION);
	qglPopMatrix();
	qglMatrixMode(GL_MODELVIEW);
	qglPopMatrix();

	qglDisable(GL_TEXTURE_CUBE_MAP_ARB);
	qglEnable(GL_TEXTURE_2D);

	GLSlang_UseProgram(0);

	return true;
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
	double	time1 = 0, time2;

	if (qglGetError())
		Con_Printf("GL Error before drawing scene\n");

	if (r_norefresh.value || !vid.pixelwidth || !vid.pixelheight)
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
		if (gl_ati_truform_type.ival)
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

	if (gl_finish.ival)
	{
		RSpeedMark();
		qglFinish ();
		RSpeedEnd(RSPEED_FINISH);
	}

	if (r_speeds.ival)
	{
		time1 = Sys_DoubleTime ();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

#ifdef FISH
	if (ffov.value && cls.allow_fish && !(r_refdef.flags & Q2RDF_NOWORLDMODEL) && R_RenderScene_Fish())
	{
		//fisheye does its own rendering.
	}
	else
#endif
	{
		mirror = false;

		GL_SetShaderState2D(false);

		R_Clear ();

	//	GLR_SetupFog ();

		// render normal view
		R_RenderScene ();

		// render mirror view
		R_Mirror ();
	}

	R_BloomBlend();

//	qglDisable(GL_FOG);

	if (r_speeds.ival)
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
		R_RenderWaterWarp();



	if (gl_motionblur.value>0 && gl_motionblur.value < 1 && qglCopyTexImage2D)
		R_RenderMotionBlur();
}

#endif
