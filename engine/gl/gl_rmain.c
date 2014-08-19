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

extern int		gl_stencilbits;

FTEPFNGLCOMPRESSEDTEXIMAGE2DARBPROC qglCompressedTexImage2DARB;
FTEPFNGLGETCOMPRESSEDTEXIMAGEARBPROC qglGetCompressedTexImageARB;

extern int			r_visframecount;	// bumped when going to a new PVS
extern int			r_framecount;		// used for dlight push checking

//mplane_t	frustum[4];

//
// view origin
//
//vec3_t	vup;
//vec3_t	vpn;
//vec3_t	vright;
//vec3_t	r_origin;

cvar_t	r_norefresh = SCVAR("r_norefresh","0");

extern cvar_t	gl_part_flame;
extern cvar_t	r_bloom;

cvar_t	gl_affinemodels = SCVAR("gl_affinemodels","0");
cvar_t	gl_reporttjunctions = SCVAR("gl_reporttjunctions","0");
cvar_t	gl_finish = SCVAR("gl_finish","0");
cvar_t	gl_dither = SCVAR("gl_dither", "1");
extern cvar_t	r_stereo_separation;
extern cvar_t	r_stereo_method;
extern cvar_t	r_postprocshader;

extern cvar_t	gl_screenangle;

extern cvar_t	gl_mindist;
extern cvar_t	vid_srgb;

extern cvar_t	ffov;

extern cvar_t	gl_motionblur;
extern cvar_t	gl_motionblurscale;

extern cvar_t gl_ati_truform;
extern cvar_t gl_ati_truform_type;
extern cvar_t gl_ati_truform_tesselation;

extern cvar_t gl_blendsprites;
extern cvar_t r_portaldrawplanes;
extern cvar_t r_portalonly;

#ifdef R_XFLIP
cvar_t	r_xflip = SCVAR("leftisright", "0");
#endif

extern	cvar_t	scr_fov;

shader_t *scenepp_waterwarp;

// post processing stuff
texid_t sceneblur_texture;
texid_t scenepp_texture_warp;
texid_t scenepp_texture_edge;

texid_t scenepp_postproc_cube;
int scenepp_postproc_cube_size;

fbostate_t fbo_gameview;
fbostate_t fbo_postproc;

// KrimZon - init post processing - called in GL_CheckExtensions, when they're called
// I put it here so that only this file need be changed when messing with the post
// processing shaders
void GL_InitSceneProcessingShaders_WaterWarp (void)
{
	scenepp_waterwarp = NULL;
	if (gl_config.arb_shader_objects)
	{
		scenepp_waterwarp = R_RegisterShader("waterwarp", SUF_NONE,
			"{\n"
				"program underwaterwarp\n"
				"{\n"
					"map $sourcecolour\n"
				"}\n"
				"{\n"
					"map $upperoverlay\n"
				"}\n"
				"{\n"
					"map $loweroverlay\n"
				"}\n"
			"}\n"
			);
		scenepp_waterwarp->defaulttextures.upperoverlay = scenepp_texture_warp;
		scenepp_waterwarp->defaulttextures.loweroverlay = scenepp_texture_edge;
	}
}

void GL_ShutdownPostProcessing(void)
{
	GLBE_FBO_Destroy(&fbo_gameview);
	GLBE_FBO_Destroy(&fbo_postproc);
	R_BloomShutdown();
}

void GL_InitSceneProcessingShaders (void)
{
	if (gl_config.arb_shader_objects)
	{
		GL_InitSceneProcessingShaders_WaterWarp();
	}

	gl_dither.modified = true;	//fixme: bad place for this, but hey
	vid_srgb.modified = true;
}

#define PP_WARP_TEX_SIZE 64
#define PP_AMP_TEX_SIZE 64
#define PP_AMP_TEX_BORDER 4
void GL_SetupSceneProcessingTextures (void)
{
	int i, x, y;
	unsigned char pp_warp_tex[PP_WARP_TEX_SIZE*PP_WARP_TEX_SIZE*3];
	unsigned char pp_edge_tex[PP_AMP_TEX_SIZE*PP_AMP_TEX_SIZE*3];

	scenepp_postproc_cube = r_nulltex;

	TEXASSIGN(sceneblur_texture, GL_AllocNewTexture("***postprocess_blur***", 0, 0, 0));

	if (!gl_config.arb_shader_objects)
		return;

	TEXASSIGN(scenepp_texture_warp, GL_AllocNewTexture("***postprocess_warp***", PP_WARP_TEX_SIZE, PP_WARP_TEX_SIZE, IF_NOMIPMAP|IF_NOGAMMA|IF_LINEAR));
	TEXASSIGN(scenepp_texture_edge, GL_AllocNewTexture("***postprocess_edge***", PP_WARP_TEX_SIZE, PP_WARP_TEX_SIZE, IF_NOMIPMAP|IF_NOGAMMA|IF_LINEAR));

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

	GL_MTBind(0, GL_TEXTURE_2D, scenepp_texture_warp);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, PP_WARP_TEX_SIZE, PP_WARP_TEX_SIZE, 0, GL_RGB, GL_UNSIGNED_BYTE, pp_warp_tex);

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

			//avoid any sudden changes.
			fx=sin(fx*M_PI*0.5);
			fy=sin(fy*M_PI*0.5);

			//lame
			fx = fy = min(fx, fy);

			pp_edge_tex[i  ] = fx * 255;
			pp_edge_tex[i+1] = fy * 255;
			pp_edge_tex[i+2] = 0;
		}
	}

//	scenepp_texture_edge = R_LoadTexture32("***postprocess_edge***", PP_AMP_TEX_SIZE, PP_AMP_TEX_SIZE, pp_edge_tex, IF_NOMIPMAP|IF_NOGAMMA|IF_NOPICMIP);

	GL_MTBind(0, GL_TEXTURE_2D, scenepp_texture_edge);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, PP_WARP_TEX_SIZE, PP_WARP_TEX_SIZE, 0, GL_RGB, GL_UNSIGNED_BYTE, pp_edge_tex);
}

void R_RotateForEntity (float *m, float *modelview, const entity_t *e, const model_t *mod)
{
	if ((e->flags & RF_WEAPONMODEL) && r_refdef.playerview->viewentity > 0)
	{
		float em[16];
		float vm[16];

		vm[0] = r_refdef.playerview->vw_axis[0][0];
		vm[1] = r_refdef.playerview->vw_axis[0][1];
		vm[2] = r_refdef.playerview->vw_axis[0][2];
		vm[3] = 0;

		vm[4] = r_refdef.playerview->vw_axis[1][0];
		vm[5] = r_refdef.playerview->vw_axis[1][1];
		vm[6] = r_refdef.playerview->vw_axis[1][2];
		vm[7] = 0;

		vm[8] = r_refdef.playerview->vw_axis[2][0];
		vm[9] = r_refdef.playerview->vw_axis[2][1];
		vm[10] = r_refdef.playerview->vw_axis[2][2];
		vm[11] = 0;

		vm[12] = r_refdef.playerview->vw_origin[0];
		vm[13] = r_refdef.playerview->vw_origin[1];
		vm[14] = r_refdef.playerview->vw_origin[2];
		vm[15] = 1;

		em[0] = e->axis[0][0];
		em[1] = e->axis[0][1];
		em[2] = e->axis[0][2];
		em[3] = 0;

		em[4] = e->axis[1][0];
		em[5] = e->axis[1][1];
		em[6] = e->axis[1][2];
		em[7] = 0;

		em[8] = e->axis[2][0];
		em[9] = e->axis[2][1];
		em[10] = e->axis[2][2];
		em[11] = 0;

		em[12] = e->origin[0];
		em[13] = e->origin[1];
		em[14] = e->origin[2];
		em[15] = 1;

		Matrix4_Multiply(vm, em, m);
	}
	else
	{
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
	}

	if (e->scale != 1 && e->scale != 0)	//hexen 2 stuff
	{
		float z;
		float escale;
		escale = e->scale;
		switch(e->drawflags&SCALE_TYPE_MASKIN)
		{
		default:
		case SCALE_TYPE_UNIFORM:
			VectorScale((m+0), escale, (m+0));
			VectorScale((m+4), escale, (m+4));
			VectorScale((m+8), escale, (m+8));
			break;
		case SCALE_TYPE_XYONLY:
			VectorScale((m+0), escale, (m+0));
			VectorScale((m+4), escale, (m+4));
			break;
		case SCALE_TYPE_ZONLY:
			VectorScale((m+8), escale, (m+8));
			break;
		}
		if (mod && (e->drawflags&SCALE_TYPE_MASKIN) != SCALE_TYPE_XYONLY)
		{
			switch(e->drawflags&SCALE_ORIGIN_MASKIN)
			{
			case SCALE_ORIGIN_CENTER:
				z = ((mod->maxs[2] + mod->mins[2]) * (1-escale))/2;
				VectorMA((m+12), z, e->axis[2], (m+12));
				break;
			case SCALE_ORIGIN_BOTTOM:
				VectorMA((m+12), mod->mins[2]*(1-escale), e->axis[2], (m+12));
				break;
			case SCALE_ORIGIN_TOP:
				VectorMA((m+12), -mod->maxs[2], e->axis[2], (m+12));
				break;
			}
		}
	}
	else if (mod && !strcmp(mod->name, "progs/eyes.mdl"))
	{
		/*resize eyes, to make them easier to see*/
		m[14] -= (22 + 8);
		VectorScale((m+0), 2, (m+0));
		VectorScale((m+4), 2, (m+4));
		VectorScale((m+8), 2, (m+8));
	}
	if (mod && !ruleset_allow_larger_models.ival && mod->clampscale != 1 && mod->type == mod_alias)
	{	//possibly this should be on a per-frame basis, but that's a real pain to do
		Con_DPrintf("Rescaling %s by %f\n", mod->name, mod->clampscale);
		VectorScale((m+0), mod->clampscale, (m+0));
		VectorScale((m+4), mod->clampscale, (m+4));
		VectorScale((m+8), mod->clampscale, (m+8));
	}

	Matrix4_Multiply(r_refdef.m_view, m, modelview);
}

//==================================================================================

qboolean R_GameRectIsFullscreen(void);
/*
=============
R_SetupGL
=============
*/
void R_SetupGL (float stereooffset)
{
	int		x, x2, y2, y, w, h;
	vec3_t newa;

	float fov_x, fov_y;

	if (!r_refdef.recurse)
	{
		AngleVectors (r_refdef.viewangles, vpn, vright, vup);
		VectorCopy (r_refdef.vieworg, r_origin);

		VectorMA(r_origin, stereooffset, vright, r_origin);

		//
		// set up viewpoint
		//
		if (r_refdef.flags & (RDF_ALLPOSTPROC))
		{
			//with fbo postprocessing, we disable all viewport.
			r_refdef.pxrect.x = 0;
			r_refdef.pxrect.y = 0;
			r_refdef.pxrect.width = vid.fbpwidth;
			r_refdef.pxrect.height = vid.fbpheight;
			r_refdef.pxrect.maxheight = vid.fbpheight;
		}
		else if (*r_refdef.rt_destcolour[0].texname)
		{
			//with fbo rendering, we disable all virtual scaling.
			x = r_refdef.vrect.x;
			x2 = r_refdef.vrect.x + r_refdef.vrect.width;
			y = r_refdef.vrect.y;
			y2 = r_refdef.vrect.y + r_refdef.vrect.height;

			w = x2 - x;
			h = y2 - y;

			r_refdef.pxrect.x = x;
			r_refdef.pxrect.y = y;
			r_refdef.pxrect.width = w;
			r_refdef.pxrect.height = h;
			r_refdef.pxrect.maxheight = vid.fbpheight;
		}
		else
		{
			x = r_refdef.vrect.x * (int)vid.fbpwidth/(int)vid.width;
			x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * (int)vid.fbpwidth/(int)vid.width;
			y = (r_refdef.vrect.y) * (int)vid.fbpheight/(int)vid.height;
			y2 = (r_refdef.vrect.y + r_refdef.vrect.height) * (int)vid.fbpheight/(int)vid.height;


			// fudge around because of frac screen scale
			if (x > 0)
				x--;
			if (x2 < vid.fbpwidth)
				x2++;
			if (y2 < vid.fbpheight)
				y2++;
			if (y > 0)
				y--;

			w = x2 - x;
			h = y2 - y;

			if (stereooffset && r_stereo_method.ival == 5)
			{
				w /= 2;
				if (stereooffset > 0)
					x += vid.fbpwidth/2;
			}

			r_refdef.pxrect.x = x;
			r_refdef.pxrect.y = y;
			r_refdef.pxrect.width = w;
			r_refdef.pxrect.height = h;
			r_refdef.pxrect.maxheight = vid.fbpheight;
		}
		fov_x = r_refdef.fov_x;//+sin(cl.time)*5;
		fov_y = r_refdef.fov_y;//-sin(cl.time+1)*5;

		GL_ViewportUpdate();

		if ((r_refdef.flags & RDF_UNDERWATER) && !(r_refdef.flags & RDF_WATERWARP))
		{
			fov_x *= 1 + (((sin(cl.time * 4.7) + 1) * 0.015) * r_waterwarp.value);
			fov_y *= 1 + (((sin(cl.time * 3.0) + 1) * 0.015) * r_waterwarp.value);
		}

		if (r_refdef.useperspective)
		{
			int stencilshadows = Sh_StencilShadowsActive();

			if ((!stencilshadows || !gl_stencilbits) && gl_maxdist.value>=100)//gl_nv_range_clamp)
			{
		//		yfov = 2*atan((float)r_refdef.vrect.height/r_refdef.vrect.width)*180/M_PI;
		//		yfov = (2.0 * tan (scr_fov.value/360*M_PI)) / screenaspect;
		//		yfov = 2*atan((float)r_refdef.vrect.height/r_refdef.vrect.width)*(scr_fov.value*2)/M_PI;
		//		MYgluPerspective (yfov,  screenaspect,  4,  4096);

				Matrix4x4_CM_Projection_Far(r_refdef.m_projection, fov_x, fov_y, gl_mindist.value, gl_maxdist.value);
			}
			else
			{
				Matrix4x4_CM_Projection_Inf(r_refdef.m_projection, fov_x, fov_y, gl_mindist.value);
			}
		}
		else
		{
			Matrix4x4_CM_Orthographic(r_refdef.m_projection, -fov_x/2, fov_x/2, -fov_y/2, fov_y/2, 0, gl_maxdist.value>=1?gl_maxdist.value:9999);
		}

		newa[0] = r_refdef.viewangles[0];
		newa[1] = r_refdef.viewangles[1];
		newa[2] = r_refdef.viewangles[2] + gl_screenangle.value;
		Matrix4x4_CM_ModelViewMatrix(r_refdef.m_view, newa, r_origin);
	}

	if (qglLoadMatrixf)
	{
		qglMatrixMode(GL_PROJECTION);
		qglLoadMatrixf(r_refdef.m_projection);

		qglMatrixMode(GL_MODELVIEW);
		qglLoadMatrixf(r_refdef.m_view);
	}

	if (!gl_config.gles && gl_dither.modified)
	{
		gl_dither.modified = false;
		if (gl_dither.ival)
		{
			qglEnable(GL_DITHER);
		}
		else
		{
			qglDisable(GL_DITHER);
		}
	}
	if (vid_srgb.modified)
	{
		if (vid_srgb.ival)
			qglEnable(GL_FRAMEBUFFER_SRGB);
		else
			qglDisable(GL_FRAMEBUFFER_SRGB);
	}
}

void Surf_SetupFrame(void);

/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/
void R_RenderScene (void)
{
	float stereooffset[2];
	int stereoframes = 1;
	int stereomode;
	int i;
	int tmpvisents = cl_numvisedicts;	/*world rendering is allowed to add additional ents, but we don't want to keep them for recursive views*/

	stereomode = r_stereo_method.ival;
	if (stereomode == 1)
	{
#ifdef GL_STEREO
		GLint glb;
		qglGetIntegerv(GL_STEREO, &glb);
		if (!glb)
#endif
			stereomode = 0;	//we are not a stereo context, so no stereoscopic rendering (this encourages it to otherwise be left enabled, which means the user is more likely to spot that they asked it to give a slower context.
	}


	if (r_refdef.recurse || !stereomode || !r_stereo_separation.value)
	{
		stereooffset[0] = 0;
		stereoframes = 1;
		stereomode = 0;
	}
	else	
	{
		stereooffset[0] = -r_stereo_separation.value;
		stereooffset[1] = r_stereo_separation.value;
		stereoframes = 2;
	}

	for (i = 0; i < stereoframes; i++)
	{
		switch (stereomode)
		{
		default:
		case 0:	//off
			if (i)
				return;
			break;
#ifdef GL_STEREO
		case 1:	//proper gl stereo rendering
			if (stereooffset[i] < 0)
				qglDrawBuffer(GL_BACK_LEFT);
			else
				qglDrawBuffer(GL_BACK_RIGHT);
			break;
#endif
		case 2:	//red/cyan(green+blue)
			if (stereooffset[i] < 0)
				qglColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_TRUE);
			else
				qglColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_TRUE);
			break;
		case 3: //red/blue
			if (stereooffset[i] < 0)
				qglColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_TRUE);
			else
				qglColorMask(GL_FALSE, GL_FALSE, GL_TRUE, GL_TRUE);
			break;
		case 4:	//red/green
			if (stereooffset[i] < 0)
				qglColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_TRUE);
			else
				qglColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE);
			break;
		case 5:	//eyestrain
			break;
		}
		if (i)
			qglClear (GL_DEPTH_BUFFER_BIT);

		TRACE(("dbg: calling R_SetupGL\n"));
		R_SetupGL (stereooffset[i]);

		TRACE(("dbg: calling R_SetFrustrum\n"));
		if (!r_refdef.recurse)
			R_SetFrustum (r_refdef.m_projection, r_refdef.m_view);

		RQ_BeginFrame();

		TRACE(("dbg: calling Surf_DrawWorld\n"));
		Surf_DrawWorld ();		// adds static entities to the list

		S_ExtraUpdate ();	// don't let sound get messed up if going slow

	//	R_DrawDecals();

		TRACE(("dbg: calling R_RenderDlights\n"));
		R_RenderDlights ();

		if (r_refdef.recurse)
			RQ_RenderBatch();
		else
			RQ_RenderBatchClear();

		cl_numvisedicts = tmpvisents;
	}

	switch (stereomode)
	{
	default:
	case 0:
		break;
	case 1:
		qglDrawBuffer(GL_BACK);
		break;
	case 3:
		qglColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
		qglClear(GL_COLOR_BUFFER_BIT);
		qglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		break;
	case 4:
		qglColorMask(GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE);
		qglClear(GL_COLOR_BUFFER_BIT);
		qglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	case 2:
		qglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		break;
	case 5:
		break;
	}
}
/*generates a new modelview matrix, as well as vpn vectors*/
static void R_MirrorMatrix(plane_t *plane)
{
	float mirror[16];
	float view[16];
	float result[16];

	vec3_t pnorm;
	VectorNegate(plane->normal, pnorm);

	mirror[0] = 1-2*pnorm[0]*pnorm[0];
	mirror[1] = -2*pnorm[0]*pnorm[1];
	mirror[2] = -2*pnorm[0]*pnorm[2];
	mirror[3] = 0;

	mirror[4] = -2*pnorm[1]*pnorm[0];
	mirror[5] = 1-2*pnorm[1]*pnorm[1];
	mirror[6] = -2*pnorm[1]*pnorm[2] ;
	mirror[7] = 0;

	mirror[8]  = -2*pnorm[2]*pnorm[0];
	mirror[9]  = -2*pnorm[2]*pnorm[1];
	mirror[10] = 1-2*pnorm[2]*pnorm[2];
	mirror[11] = 0;

	mirror[12] = -2*pnorm[0]*plane->dist;
	mirror[13] = -2*pnorm[1]*plane->dist;
	mirror[14] = -2*pnorm[2]*plane->dist;
	mirror[15] = 1;

	view[0] = vpn[0];
	view[1] = vpn[1];
	view[2] = vpn[2];
	view[3] = 0;

	view[4] = -vright[0];
	view[5] = -vright[1];
	view[6] = -vright[2];
	view[7] = 0;

	view[8]  = vup[0];
	view[9]  = vup[1];
	view[10] = vup[2];
	view[11] = 0;

	view[12] = r_refdef.vieworg[0];
	view[13] = r_refdef.vieworg[1];
	view[14] = r_refdef.vieworg[2];
	view[15] = 1;

	VectorMA(r_refdef.vieworg, 0.25, plane->normal, r_refdef.pvsorigin);

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
static entity_t *R_NearestPortal(plane_t *plane)
{
	int i;
	entity_t *best = NULL;
	float dist, bestd = 0;
	//for q3-compat, portals on world scan for a visedict to use for their view.
	for (i = 0; i < cl_numvisedicts; i++)
	{
		if (cl_visedicts[i].rtype == RT_PORTALSURFACE)
		{
			dist = DotProduct(cl_visedicts[i].origin, plane->normal)-plane->dist;
			dist = fabs(dist);
			if (dist < 64 && (!best || dist < bestd))
				best = &cl_visedicts[i];
		}
	}
	return best;
}

static void TransformCoord(vec3_t in, vec3_t planea[3], vec3_t planeo, vec3_t viewa[3], vec3_t viewo, vec3_t result)
{
	int		i;
	vec3_t	local;
	vec3_t	transformed;
	float	d;

	local[0] = in[0] - planeo[0];
	local[1] = in[1] - planeo[1];
	local[2] = in[2] - planeo[2];

	VectorClear(transformed);
	for ( i = 0 ; i < 3 ; i++ )
	{
		d = DotProduct(local, planea[i]);
		VectorMA(transformed, d, viewa[i], transformed);
	}

	result[0] = transformed[0] + viewo[0];
	result[1] = transformed[1] + viewo[1];
	result[2] = transformed[2] + viewo[2];
}
static void TransformDir(vec3_t in, vec3_t planea[3], vec3_t viewa[3], vec3_t result)
{
	int		i;
	float	d;
	vec3_t tmp;

	VectorCopy(in, tmp);

	VectorClear(result);
	for ( i = 0 ; i < 3 ; i++ )
	{
		d = DotProduct(tmp, planea[i]);
		VectorMA(result, d, viewa[i], result);
	}
}
static float sgn(float a)
{
    if (a > 0.0F) return (1.0F);
    if (a < 0.0F) return (-1.0F);
    return (0.0F);
}
void R_ObliqueNearClip(float *viewmat, mplane_t *wplane)
{
	float f;
	vec4_t q, c;
	vec3_t ping, pong;
	vec4_t vplane;

	//convert world plane into view space
	Matrix4x4_CM_Transform3x3(viewmat, wplane->normal, vplane);
	VectorScale(wplane->normal, wplane->dist, ping);
	Matrix4x4_CM_Transform3(viewmat, ping, pong);
	vplane[3] = -DotProduct(pong, vplane);

	// Calculate the clip-space corner point opposite the clipping plane
	// as (sgn(clipPlane.x), sgn(clipPlane.y), 1, 1) and
	// transform it into camera space by multiplying it
	// by the inverse of the projection matrix

	q[0] = (sgn(vplane[0]) + r_refdef.m_projection[8]) / r_refdef.m_projection[0];
	q[1] = (sgn(vplane[1]) + r_refdef.m_projection[9]) / r_refdef.m_projection[5];
	q[2] = -1.0F;
	q[3] = (1.0F + r_refdef.m_projection[10]) / r_refdef.m_projection[14];

	// Calculate the scaled plane vector
	f = 2.0F / DotProduct4(vplane, q);
	Vector4Scale(vplane, f, c);

	// Replace the third row of the projection matrix
	r_refdef.m_projection[2] = c[0];
	r_refdef.m_projection[6] = c[1];
	r_refdef.m_projection[10] = c[2] + 1.0F;
	r_refdef.m_projection[14] = c[3];
}

void CL_DrawDebugPlane(float *normal, float dist, float r, float g, float b, qboolean enqueue);
void GLR_DrawPortal(batch_t *batch, batch_t **blist, batch_t *depthmasklist[2], int portaltype)
{
	entity_t *view;
//	GLdouble glplane[4];
	plane_t plane, oplane;
	float vmat[16];
	refdef_t oldrefdef;
	vec3_t r;
	int i;
	mesh_t *mesh = batch->mesh[batch->firstmesh];
	qbyte newvis[(MAX_MAP_LEAFS+7)/8];
	float ivmat[16], trmat[16];

	if (r_refdef.recurse >= R_MAX_RECURSE-1)
		return;

	if (!mesh->normals_array)
	{
		VectorSet(plane.normal, 0, 0, 1);
	}
	else
	{
		VectorCopy(mesh->normals_array[0], plane.normal);
	}

	if (batch->ent == &r_worldentity)
	{
		plane.dist = DotProduct(mesh->xyz_array[0], plane.normal);
	}
	else
	{
		vec3_t point;
		VectorCopy(plane.normal, oplane.normal);
		//rotate the surface normal around its entity's matrix
		plane.normal[0] = oplane.normal[0]*batch->ent->axis[0][0] + oplane.normal[1]*batch->ent->axis[1][0] + oplane.normal[2]*batch->ent->axis[2][0];
		plane.normal[1] = oplane.normal[0]*batch->ent->axis[0][1] + oplane.normal[1]*batch->ent->axis[1][1] + oplane.normal[2]*batch->ent->axis[2][1];
		plane.normal[2] = oplane.normal[0]*batch->ent->axis[0][2] + oplane.normal[1]*batch->ent->axis[1][2] + oplane.normal[2]*batch->ent->axis[2][2];

		//rotate some point on the mesh around its entity's matrix
		point[0] = mesh->xyz_array[0][0]*batch->ent->axis[0][0] + mesh->xyz_array[0][1]*batch->ent->axis[1][0] + mesh->xyz_array[0][2]*batch->ent->axis[2][0] + batch->ent->origin[0];
		point[1] = mesh->xyz_array[0][0]*batch->ent->axis[0][1] + mesh->xyz_array[0][1]*batch->ent->axis[1][1] + mesh->xyz_array[0][2]*batch->ent->axis[2][1] + batch->ent->origin[1];
		point[2] = mesh->xyz_array[0][0]*batch->ent->axis[0][2] + mesh->xyz_array[0][1]*batch->ent->axis[1][2] + mesh->xyz_array[0][2]*batch->ent->axis[2][2] + batch->ent->origin[2];

		//now we can figure out the plane dist
		plane.dist = DotProduct(point, plane.normal);
	}

	//if we're too far away from the surface, don't draw anything
	if (batch->shader->flags & SHADER_AGEN_PORTAL)
	{
		/*there's a portal alpha blend on that surface, that fades out after this distance*/
		if (DotProduct(r_refdef.vieworg, plane.normal)-plane.dist > batch->shader->portaldist)
			return;
	}
	//if we're behind it, then also don't draw anything. for our purposes, behind is when the entire near clipplane is behind.
	if (DotProduct(r_refdef.vieworg, plane.normal)-plane.dist < -gl_mindist.value)
		return;

	TRACE(("GLR_DrawPortal: portal type %i\n", portaltype));

	oldrefdef = r_refdef;
	r_refdef.recurse+=1;

	r_refdef.externalview = true;

	switch(portaltype)
	{
	case 1: /*fbo explicit mirror (fucked depth, working clip plane)*/
		//fixme: pvs is surely wrong?
//		r_refdef.flipcull ^= SHADER_CULL_FLIP;
		R_MirrorMatrix(&plane);
		Matrix4x4_CM_ModelViewMatrixFromAxis(vmat, vpn, vright, vup, r_refdef.vieworg);

		VectorCopy(mesh->xyz_array[0], r_refdef.pvsorigin);
		for (i = 1; i < mesh->numvertexes; i++)
			VectorAdd(r_refdef.pvsorigin, mesh->xyz_array[i], r_refdef.pvsorigin);
		VectorScale(r_refdef.pvsorigin, 1.0/mesh->numvertexes, r_refdef.pvsorigin);
		break;
	
	case 2:	/*fbo refraction (fucked depth, working clip plane)*/
	case 3:	/*screen copy refraction (screen depth, fucked clip planes)*/
		/*refraction image (same view, just with things culled*/
		r_refdef.externalview = oldrefdef.externalview;
		VectorNegate(plane.normal, plane.normal);
		plane.dist = -plane.dist;

		//use the player's origin for r_viewleaf, because there's not much we can do anyway*/
		VectorCopy(r_origin, r_refdef.pvsorigin);

		if (cl.worldmodel && cl.worldmodel->funcs.ClusterPVS && !r_novis.ival)
		{
			int clust, i, j;
			float d;
			vec3_t point;
			int pvsbytes = (cl.worldmodel->numclusters+7)>>3;
			if (pvsbytes > sizeof(newvis))
				pvsbytes = sizeof(newvis);
			r_refdef.forcevis = true;
			r_refdef.forcedvis = NULL;
			for (i = batch->firstmesh; i < batch->meshes; i++)
			{
				mesh = batch->mesh[i];
				VectorClear(point);
				for (j = 0; j < mesh->numvertexes; j++)
					VectorAdd(point, mesh->xyz_array[j], point);
				VectorScale(point, 1.0f/mesh->numvertexes, point);
				d = DotProduct(point, plane.normal) - plane.dist;
				d += 0.1;	//an epsilon on the far side
				VectorMA(point, d, plane.normal, point);

				clust = cl.worldmodel->funcs.ClusterForPoint(cl.worldmodel, point);
				if (i == batch->firstmesh)
					r_refdef.forcedvis = cl.worldmodel->funcs.ClusterPVS(cl.worldmodel, clust, newvis, sizeof(newvis));
				else
				{
					if (r_refdef.forcedvis != newvis)
					{
						memcpy(newvis, r_refdef.forcedvis, pvsbytes);
					}
					r_refdef.forcedvis = cl.worldmodel->funcs.ClusterPVS(cl.worldmodel, clust, NULL, sizeof(newvis));

					for (j = 0; j < pvsbytes; j+= 4)
					{
						*(int*)&newvis[j] |= *(int*)&r_refdef.forcedvis[j];
					}
					r_refdef.forcedvis = newvis;
				}
			}
//			memset(newvis, 0xff, pvsbytes);
		}
		Matrix4x4_CM_ModelViewMatrixFromAxis(vmat, vpn, vright, vup, r_refdef.vieworg);
		break;

	case 0:		/*q3 portal*/
	default:
#ifdef CSQC_DAT
		if (CSQC_SetupToRenderPortal(batch->ent->keynum))
		{
			oplane = plane;

			//transform the old surface plane into the new view matrix
			Matrix4_Invert(r_refdef.m_view, ivmat);
			Matrix4x4_CM_ModelViewMatrixFromAxis(vmat, vpn, vright, vup, r_refdef.vieworg);
			Matrix4_Multiply(ivmat, vmat, trmat);
			plane.normal[0] = -(oplane.normal[0] * trmat[0] + oplane.normal[1] * trmat[1] + oplane.normal[2] * trmat[2]);
			plane.normal[1] = -(oplane.normal[0] * trmat[4] + oplane.normal[1] * trmat[5] + oplane.normal[2] * trmat[6]);
			plane.normal[2] = -(oplane.normal[0] * trmat[8] + oplane.normal[1] * trmat[9] + oplane.normal[2] * trmat[10]);
			plane.dist = -oplane.dist + trmat[12]*oplane.normal[0] + trmat[13]*oplane.normal[1] + trmat[14]*oplane.normal[2];

			if (Cvar_Get("temp_useplaneclip", "1", 0, "temp")->ival)
				portaltype = 1;	//make sure the near clipplane is used.
		}
		else
#endif
			if (!(view = R_NearestPortal(&plane)) || VectorCompare(view->origin, view->oldorigin))
		{
			//a portal with no portal entity, or a portal rentity with an origin equal to its oldorigin, is a mirror.
//			r_refdef.flipcull ^= SHADER_CULL_FLIP;
			R_MirrorMatrix(&plane);
			Matrix4x4_CM_ModelViewMatrixFromAxis(vmat, vpn, vright, vup, r_refdef.vieworg);

			VectorCopy(mesh->xyz_array[0], r_refdef.pvsorigin);
			for (i = 1; i < mesh->numvertexes; i++)
				VectorAdd(r_refdef.pvsorigin, mesh->xyz_array[i], r_refdef.pvsorigin);
			VectorScale(r_refdef.pvsorigin, 1.0/mesh->numvertexes, r_refdef.pvsorigin);

			portaltype = 1;
		}
		else
		{
			float d;
			vec3_t paxis[3], porigin, vaxis[3], vorg;
			void PerpendicularVector( vec3_t dst, const vec3_t src );

			oplane = plane;

			/*calculate where the surface is meant to be*/
			VectorCopy(mesh->normals_array[0], paxis[0]);
			PerpendicularVector(paxis[1], paxis[0]);
			CrossProduct(paxis[0], paxis[1], paxis[2]);
			d = DotProduct(view->origin, plane.normal) - plane.dist;
			VectorMA(view->origin, -d, paxis[0], porigin);

			/*grab the camera origin*/
			VectorNegate(view->axis[0], vaxis[0]);
			VectorNegate(view->axis[1], vaxis[1]);
			VectorCopy(view->axis[2], vaxis[2]);
			VectorCopy(view->oldorigin, vorg);

			VectorCopy(vorg, r_refdef.pvsorigin);

			/*rotate it a bit*/
			if (view->framestate.g[FS_REG].frame[1])	//oldframe
			{
				if (view->framestate.g[FS_REG].frame[0])	//newframe
					d = realtime * view->framestate.g[FS_REG].frame[0];	//newframe
				else
					d = view->skinnum + sin(realtime)*4;
			}
			else
				d = view->skinnum;

			if (d)
			{
				vec3_t rdir;
				VectorCopy(vaxis[1], rdir);
				RotatePointAroundVector(vaxis[1], vaxis[0], rdir, d);
				CrossProduct(vaxis[0], vaxis[1], vaxis[2]);
			}

			TransformCoord(oldrefdef.vieworg, paxis, porigin, vaxis, vorg, r_refdef.vieworg);
			TransformDir(vpn, paxis, vaxis, vpn);
			TransformDir(vright, paxis, vaxis, vright);
			TransformDir(vup, paxis, vaxis, vup);
			Matrix4x4_CM_ModelViewMatrixFromAxis(vmat, vpn, vright, vup, r_refdef.vieworg);


			//transform the old surface plane into the new view matrix
			if (Matrix4_Invert(r_refdef.m_view, ivmat))
			{
				Matrix4_Multiply(ivmat, vmat, trmat);
				plane.normal[0] = -(oplane.normal[0] * trmat[0] + oplane.normal[1] * trmat[1] + oplane.normal[2] * trmat[2]);
				plane.normal[1] = -(oplane.normal[0] * trmat[4] + oplane.normal[1] * trmat[5] + oplane.normal[2] * trmat[6]);
				plane.normal[2] = -(oplane.normal[0] * trmat[8] + oplane.normal[1] * trmat[9] + oplane.normal[2] * trmat[10]);
				plane.dist = -oplane.dist + trmat[12]*oplane.normal[0] + trmat[13]*oplane.normal[1] + trmat[14]*oplane.normal[2];
				portaltype = 1;
			}
		}
		break;
	}

	/*FIXME: can we get away with stenciling the screen?*/
	/*Add to frustum culling instead of clip planes?*/
/*	if (qglClipPlane && portaltype)
	{
		GLdouble glplane[4];
		glplane[0] = plane.normal[0];
		glplane[1] = plane.normal[1];
		glplane[2] = plane.normal[2];
		glplane[3] = plane.dist;
		qglClipPlane(GL_CLIP_PLANE0, glplane);
		qglEnable(GL_CLIP_PLANE0);
	}
*/	//fixme: we can probably scissor a smaller frusum
	R_SetFrustum (r_refdef.m_projection, vmat);
	if (r_refdef.frustum_numplanes < MAXFRUSTUMPLANES)
	{
		extern int SignbitsForPlane (mplane_t *out);
		mplane_t fp;
		VectorCopy(plane.normal, fp.normal);
		fp.dist = plane.dist;

//		if (DotProduct(fp.normal, vpn) < 0)
//		{
//			VectorNegate(fp.normal, fp.normal);
//			fp.dist *= -1;
//		}

		fp.type = PLANE_ANYZ;
		fp.signbits = SignbitsForPlane (&fp);

		if (portaltype == 1 || portaltype == 2)
			R_ObliqueNearClip(vmat, &fp);

		//our own culling should be an epsilon forwards so we don't still draw things behind the line due to precision issues.
		fp.dist += 0.01;
		r_refdef.frustum[r_refdef.frustum_numplanes++] = fp;
	}

	//force culling to update to match the new front face.
//	memcpy(r_refdef.m_view, vmat, sizeof(float)*16);
	if (depthmasklist)
	{
		/*draw already-drawn portals as depth-only, to ensure that their contents are not harmed*/
		/*we can only do this AFTER the oblique perspective matrix is calculated, to avoid depth inconsistancies, while we still have the old view matrix*/
		int i;
		batch_t *dmask = NULL;
		if (qglLoadMatrixf)
		{
			qglMatrixMode(GL_PROJECTION);
			qglLoadMatrixf(r_refdef.m_projection);

			//portals to mask are relative to the old view still.
			qglMatrixMode(GL_MODELVIEW);
			qglLoadMatrixf(r_refdef.m_view);
		}
		currententity = NULL;
		if (gl_config.arb_depth_clamp)
			qglEnable(GL_DEPTH_CLAMP_ARB);	//ignore the near clip plane(ish), this means nearer portals can still mask further ones.
		GL_ForceDepthWritable();
		GLBE_SelectMode(BEM_DEPTHONLY);
		for (i = 0; i < 2; i++)
		{
			for (dmask = depthmasklist[i]; dmask; dmask = dmask->next)
			{
				if (dmask == batch)
					continue;
				if (dmask->meshes == dmask->firstmesh)
					continue;
				GLBE_SubmitBatch(dmask);
			}
		}
		GLBE_SelectMode(BEM_STANDARD);
		if (gl_config.arb_depth_clamp)
			qglDisable(GL_DEPTH_CLAMP_ARB);

		currententity = NULL;
	}

	//now determine the stuff the backend will use.
	memcpy(r_refdef.m_view, vmat, sizeof(float)*16);
	VectorAngles(vpn, vup, r_refdef.viewangles);
	r_refdef.viewangles[0] *= -1;
	VectorCopy(r_refdef.vieworg, r_origin);

	//determine r_refdef.flipcull & SHADER_CULL_FLIP based upon whether right is right or not.
	CrossProduct(vpn, vup, r);
	if (DotProduct(r, vright) < 0)
		r_refdef.flipcull |= SHADER_CULL_FLIP;
	else
		r_refdef.flipcull &= ~SHADER_CULL_FLIP;
	GL_CullFace(0);//make sure flipcull takes effect

	//FIXME: just call Surf_DrawWorld instead?
	R_RenderScene();
//	if (qglClipPlane)
//		qglDisable(GL_CLIP_PLANE0);

	if (r_portaldrawplanes.ival)
	{
		//the front of the plane should generally point away from the camera, and will be drawn in bright green. woo
		CL_DrawDebugPlane(plane.normal, plane.dist+0.01, 0.0, 0.5, 0.0, false);
		CL_DrawDebugPlane(plane.normal, plane.dist-0.01, 0.0, 0.5, 0.0, false);
		//the back of the plane points towards the camera, and will be drawn in blue, for the luls
		VectorNegate(plane.normal, plane.normal);
		plane.dist *= -1;
		CL_DrawDebugPlane(plane.normal, plane.dist+0.01, 0.0, 0.0, 0.2, false);
		CL_DrawDebugPlane(plane.normal, plane.dist-0.01, 0.0, 0.0, 0.2, false);
	}


	r_refdef = oldrefdef;

	/*broken stuff*/
	AngleVectors (r_refdef.viewangles, vpn, vright, vup);
	VectorCopy (r_refdef.vieworg, r_origin);

	if (qglLoadMatrixf)
	{
		/*put GL back the way it was*/
		qglMatrixMode(GL_PROJECTION);
		qglLoadMatrixf(r_refdef.m_projection);

		qglMatrixMode(GL_MODELVIEW);
		qglLoadMatrixf(r_refdef.m_view);
	}

	GL_CullFace(0);//make sure flipcull reversion takes effect

	TRACE(("GLR_DrawPortal: portal drawn\n"));

#ifdef warningmsg
#pragma warningmsg("warning: there's a bug with rtlights in portals, culling is broken or something. May also be loading the wrong matrix")
#endif
	currententity = NULL;
}


/*
=============
R_Clear
=============
*/
qboolean R_GameRectIsFullscreen(void)
{
	return r_refdef.grect.x == 0 && r_refdef.grect.y == 0 && (unsigned)r_refdef.grect.width == vid.fbvwidth && (unsigned)r_refdef.grect.height == vid.fbvheight;
}

int gldepthfunc = GL_LEQUAL;
void R_Clear (void)
{
	/*tbh, this entire function should be in the backend*/
	GL_ForceDepthWritable();
	{
		if (r_clear.ival && R_GameRectIsFullscreen() && !(r_refdef.flags & RDF_NOWORLDMODEL))
		{
			qglClearColor(1, 0, 0, 0);
			qglClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		}
		else
			qglClear (GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 1;
		gldepthfunc=GL_LEQUAL;
	}
}

#if 0
void GLR_SetupFog (void)
{
	if (r_viewleaf)// && r_viewcontents != FTECONTENTS_EMPTY)
	{
		//	static fogcolour;
		float fogcol[4]={0};
		float fogperc;
		float fogdist;

		fogperc=0;
		fogdist=512;
		switch(r_viewcontents)
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
#if !defined(ANDROID) && !defined(NACL)
	int vwidth = 1, vheight = 1;
	float vs, vt, cs, ct;
	shader_t *shader;

	//figure out the size of our texture.
	if (r_config.texture_non_power_of_two)
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

	//blend the last frame onto the scene
	//the maths is because our texture is over-sized (must be power of two)
	cs = vs = (float)vid.pixelwidth / vwidth * 0.5;
	ct = vt = (float)vid.pixelheight / vheight * 0.5;
	vs *= gl_motionblurscale.value;
	vt *= gl_motionblurscale.value;

	//render using our texture
	shader = R_RegisterShader("postproc_motionblur", SUF_NONE,
		"{\n"
			"program default2d\n"
			"{\n"
				"map $sourcecolour\n"
				"blendfunc blend\n"
			"}\n"
		"}\n"
		);
//	GLBE_RenderToTexture(sceneblur_texture, r_nulltex, r_nulltex, r_nulltex, false);
	Con_Printf("FIXME: tex_sourcecolour = sceneblur_texture\n");
	R2D_ImageColours(1, 1, 1, gl_motionblur.value);
	R2D_Image(0, 0, vid.width, vid.height, cs-vs, ct+vt, cs+vs, ct-vt, shader);
	Con_Printf("FIXME: tex_sourcecolour = reset\n");
//	GLBE_RenderToTexture(r_nulltex, r_nulltex, r_nulltex, r_nulltex, false);

	//grab the current image so we can feed that back into the next frame.
	GL_MTBind(0, GL_TEXTURE_2D, sceneblur_texture);
	//copy the image into the texture so that we can play with it next frame too!
	qglCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, vwidth, vheight, 0);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#endif
}

#if 0
/*FIXME: we could use geometry shaders to draw to all 6 faces at once*/
qboolean R_RenderScene_Cubemap(void)
{
	int cmapsize = 512;
	int i;
	static vec3_t ang[6] =
				{	{0, -90, 0}, {0, 90, 0},
					{90, 0, 0}, {-90, 0, 0},
					{0, 0, 0}, {0, -180, 0}	};
	vec3_t saveang;

	vrect_t vrect;
	pxrect_t prect;

	shader_t *shader;
	int facemask;

	/*needs glsl*/
	if (!gl_config.arb_shader_objects)
		return false;
	if (!ffov.value)
		return false;
	if (!cls.allow_postproc)
		return false;

	facemask = 0;
	if (ffov.value < 0)
	{
		shader = R_RegisterShader("postproc_panorama", SUF_NONE,
				"{\n"
					"program postproc_panorama\n"
					"{\n"
						"map $sourcecube\n"
					"}\n"
				"}\n"
				);

		//panoramic view needs at most the four sides
		facemask |= 1<<4; /*front view*/
		if (ffov.value < -90)
		{
			facemask |= (1<<0) | (1<<1); /*side views*/
			if (ffov.value < -270)
				facemask |= 1<<5; /*back view*/
		}
	}
	else
	{
		shader = R_RegisterShader("postproc_fisheye", SUF_NONE,
				"{\n"
					"program postproc_fisheye\n"
					"{\n"
						"map $sourcecube\n"
					"}\n"
				"}\n"
				);

		//fisheye view sees up to a full sphere
		facemask |= 1<<4; /*front view*/
		if (ffov.value > 77)
			facemask |= (1<<0) | (1<<1) | (1<<2) | (1<<3); /*side/top/bottom views*/
		if (ffov.value > 270)
			facemask |= 1<<5; /*back view*/
	}

	vrect = r_refdef.vrect;
	prect = r_refdef.pxrect;
//	prect.x = (vrect.x * vid.pixelwidth)/vid.width;
//	prect.width = (vrect.width * vid.pixelwidth)/vid.width;
//	prect.y = (vrect.y * vid.pixelheight)/vid.height;
//	prect.height = (vrect.height * vid.pixelheight)/vid.height;

	if (r_config.texture_non_power_of_two)
	{
		if (prect.width < prect.height)
			cmapsize = prect.width;
		else
			cmapsize = prect.height;
	}
	else
	{
		while (cmapsize > prect.width || cmapsize > prect.height)
		{
			cmapsize /= 2;
		}
	}

	VectorCopy(r_refdef.viewangles, saveang);
	saveang[2] = 0;

	if (!TEXVALID(scenepp_postproc_cube) || cmapsize != scenepp_postproc_cube_size)
	{
		if (TEXVALID(scenepp_postproc_cube))
			GL_DestroyTexture(scenepp_postproc_cube);
		scenepp_postproc_cube = GL_AllocNewTexture("***fish***", cmapsize, cmapsize, 0);

		GL_MTBind(0, GL_TEXTURE_CUBE_MAP_ARB, scenepp_postproc_cube);
		for (i = 0; i < 6; i++)
			qglCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + i, 0, GL_RGB, 0, 0, cmapsize, cmapsize, 0);
		qglTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		qglTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		qglTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		qglTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		scenepp_postproc_cube_size = cmapsize;
	}

	vrect = r_refdef.vrect;	//save off the old vrect

	r_refdef.vrect.width = (cmapsize * vid.width) / vid.pixelwidth;
	r_refdef.vrect.height = (cmapsize * vid.height) / vid.pixelheight;
	r_refdef.vrect.x = 0;
	r_refdef.vrect.y = prect.y;

	ang[0][0] = -saveang[0];
	ang[0][1] = -90;
	ang[0][2] = -saveang[0];

	ang[1][0] = -saveang[0];
	ang[1][1] = 90;
	ang[1][2] = saveang[0];
	ang[5][0] = -saveang[0]*2;
	for (i = 0; i < 6; i++)
	{
		if (!(facemask & (1<<i)))
			continue;

		r_refdef.fov_x = 90;
		r_refdef.fov_y = 90;
		r_refdef.viewangles[0] = saveang[0]+ang[i][0];
		r_refdef.viewangles[1] = saveang[1]+ang[i][1];
		r_refdef.viewangles[2] = saveang[2]+ang[i][2];

		R_Clear ();

		GL_SetShaderState2D(false);

		// render normal view
		R_RenderScene ();

		GL_MTBind(0, GL_TEXTURE_CUBE_MAP_ARB, scenepp_postproc_cube);
//FIXME: use a render target instead.
		qglCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + i, 0, 0, 0, 0, vid.pixelheight - (prect.y + cmapsize), cmapsize, cmapsize);
	}

	r_refdef.vrect = vrect;
	r_refdef.pxrect = prect;

	//GL_ViewportUpdate();
	GL_Set2D(false);
	// go 2d
/*	qglMatrixMode(GL_PROJECTION);
	qglPushMatrix();
	qglLoadIdentity ();
	qglOrtho  (0, vid.width, vid.height, 0, -99999, 99999);
	qglMatrixMode(GL_MODELVIEW);
	qglPushMatrix();
	qglLoadIdentity ();
*/
	// draw it through the shader
	R2D_Image(0, 0, vid.width, vid.height, -0.5, 0.5, 0.5, -0.5, shader);

	//revert the matricies
/*	qglMatrixMode(GL_PROJECTION);
	qglPopMatrix();
	qglMatrixMode(GL_MODELVIEW);
	qglPopMatrix();
*/
	return true;
}
#endif

texid_t R_RenderPostProcess (texid_t sourcetex, int type, shader_t *shader, char *restexname)
{
	if (r_refdef.flags & type)
	{
		r_refdef.flags &= ~type;
		GLBE_FBO_Sources(sourcetex, r_nulltex);

		if (r_refdef.flags & RDF_ALLPOSTPROC)
		{	//there's other post-processing passes that still need to be applied.
			//thus we need to write this output to a texture.
			int w = (r_refdef.vrect.width * vid.pixelwidth) / vid.width;
			int h = (r_refdef.vrect.height * vid.pixelheight) / vid.height;
			sourcetex = R2D_RT_Configure(restexname, w, h, TF_RGBA32);
			GLBE_FBO_Update(&fbo_postproc, 0, &sourcetex, 1, r_nulltex, w, h);
			R2D_ScalePic(0, vid.pixelheight-r_refdef.vrect.height, r_refdef.vrect.width, r_refdef.vrect.height, scenepp_waterwarp);
			GLBE_RenderToTextureUpdate2d(true);
		}
		else
		{	//yay, dump it to the screen
			//update stuff now that we're not rendering the 3d scene
			R2D_ScalePic(r_refdef.vrect.x, r_refdef.vrect.y, r_refdef.vrect.width, r_refdef.vrect.height, scenepp_waterwarp);
		}
	}

	return sourcetex;
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void GLR_RenderView (void)
{
	int dofbo = *r_refdef.rt_destcolour[0].texname || *r_refdef.rt_depth.texname;
	double	time1 = 0, time2;
	texid_t sourcetex = r_nulltex;
	shader_t *custompostproc = NULL;

	checkglerror();

	if (r_norefresh.value || !vid.pixelwidth || !vid.pixelheight)
		return;

	//when loading/bugged, its possible that the world is still loading.
	//in this case, don't act as a wallhack (unless the world is meant to be hidden anyway)
	if (!(r_refdef.flags & RDF_NOWORLDMODEL))
	{
		//FIXME: fbo stuff
		if (!r_worldentity.model || r_worldentity.model->needload || !cl.worldmodel)
		{
			GL_Set2D (false);
			R2D_ImageColours(0, 0, 0, 1);
			R2D_FillBlock(r_refdef.vrect.x, r_refdef.vrect.y, r_refdef.vrect.width, r_refdef.vrect.height);
			R2D_ImageColours(1, 1, 1, 1);
			return;
		}
//		Sys_Error ("R_RenderView: NULL worldmodel");
	}

	//check if we're underwater (this also limits damage from stereo wallhacks).
	Surf_SetupFrame();
	r_refdef.flags &= ~RDF_ALLPOSTPROC;

	//if bloom is 
	if (R_CanBloom())
		r_refdef.flags |= RDF_BLOOM;

	//check if we can do underwater warp
	if (cls.protocol != CP_QUAKE2)	//quake2 tells us directly
	{
		if (r_viewcontents & FTECONTENTS_FLUID)
			r_refdef.flags |= RDF_UNDERWATER;
		else
			r_refdef.flags &= ~RDF_UNDERWATER;
	}
	if (r_refdef.flags & RDF_UNDERWATER)
	{
		if (!r_waterwarp.value)
			r_refdef.flags &= ~RDF_UNDERWATER;	//no warp at all
		else if (r_waterwarp.value > 0 && scenepp_waterwarp)
			r_refdef.flags |= RDF_WATERWARP;	//try fullscreen warp instead if we can
	}

	//
	if (*r_postprocshader.string)
	{
		custompostproc = R_RegisterCustom(r_postprocshader.string, SUF_NONE, NULL, NULL);
		if (custompostproc)
			r_refdef.flags |= RDF_CUSTOMPOSTPROC;
	}

	//disable stuff if its simply not supported.
	if (dofbo || !gl_config.arb_shader_objects || !gl_config.ext_framebuffer_objects || !r_config.texture_non_power_of_two)
		r_refdef.flags &= ~(RDF_ALLPOSTPROC);	//block all of this stuff


	BE_Scissor(NULL);
	if (dofbo)
	{
		unsigned int flags = 0;
		texid_t col[R_MAX_RENDERTARGETS], depth = r_nulltex;
		unsigned int cw=0, ch=0, dw=0, dh=0;
		int mrt;
		//3d views generally ignore source colour+depth.
		//FIXME: support depth with no colour
		for (mrt = 0; mrt < R_MAX_RENDERTARGETS; mrt++)
		{
			if (*r_refdef.rt_destcolour[mrt].texname)
			{
				col[mrt] = R2D_RT_GetTexture(r_refdef.rt_destcolour[mrt].texname, &cw, &ch);
				if (!TEXVALID(col[mrt]))
					break;
			}
			else
			{
				col[mrt] = r_nulltex;
				break;
			}
		}
		if (*r_refdef.rt_depth.texname)
			depth = R2D_RT_GetTexture(r_refdef.rt_depth.texname, &dw, &dh);

		if (mrt)
		{ 	//colour (with or without depth)
			if (*r_refdef.rt_depth.texname && (dw != cw || dh != dh))
			{
				Con_Printf("RT: destcolour and depth render targets are of different sizes\n");	//should check rgb/depth modes too I guess.
				depth = r_nulltex;
			}
			vid.fbvwidth = vid.fbpwidth = cw;
			vid.fbvheight = vid.fbpheight = ch;
		}
		else
		{	//depth, with no colour
			vid.fbvwidth = vid.fbpwidth = dw;
			vid.fbvheight = vid.fbpheight = dh;
		}
		if (TEXVALID(depth))
			flags |= FBO_TEX_DEPTH;
		else
			flags |= FBO_RB_DEPTH;
		GLBE_FBO_Update(&fbo_gameview, flags, col, mrt, depth, vid.fbpwidth, vid.fbpheight);
	}
	else if (r_refdef.flags & (RDF_ALLPOSTPROC))
	{
		//the game needs to be drawn to a texture for post processing
		vid.fbvwidth = vid.fbpwidth = (r_refdef.vrect.width * vid.pixelwidth) / vid.width;
		vid.fbvheight = vid.fbpheight = (r_refdef.vrect.height * vid.pixelheight) / vid.height;

		sourcetex = R2D_RT_Configure("rt/$lastgameview", vid.fbpwidth, vid.fbpheight, TF_RGBA32);

		GLBE_FBO_Update(&fbo_gameview, FBO_RB_DEPTH, &sourcetex, 1, r_nulltex, vid.fbpwidth, vid.fbpheight);
		dofbo = true;
	}
	else
	{
		vid.fbvwidth = vid.width;
		vid.fbvheight = vid.height;
		vid.fbpwidth = vid.pixelwidth;
		vid.fbpheight = vid.pixelheight;
	}

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
	}

//	if (!dofbo && !(r_refdef.flags & RDF_NOWORLDMODEL) && R_RenderScene_Cubemap())
//	{
//
//	}
//	else
	{
		GL_SetShaderState2D(false);

		R_Clear ();

	//	GLR_SetupFog ();

		// render normal view
		R_RenderScene ();
	}

//	qglDisable(GL_FOG);

	if (r_speeds.ival)
	{
//		glFinish ();
		time2 = Sys_DoubleTime ();

		RQuantAdd(RQUANT_MSECS, (int)((time2-time1)*1000000));

	//	Con_Printf ("%3i ms  %4i wpoly %4i epoly\n", (int)((time2-time1)*1000), c_brush_polys, c_alias_polys);
	}

	checkglerror();

	//update stuff now that we're not rendering the 3d scene
	if (dofbo)
		GLBE_RenderToTextureUpdate2d(true);
	else
	{
		GLBE_RenderToTextureUpdate2d(false);
		GL_Set2D (false);
	}

	// SCENE POST PROCESSING

	sourcetex = R_RenderPostProcess (sourcetex, RDF_WATERWARP, scenepp_waterwarp, "rt/$waterwarped");
	sourcetex = R_RenderPostProcess (sourcetex, RDF_CUSTOMPOSTPROC, custompostproc, "rt/$postproced");
	if (r_refdef.flags & RDF_BLOOM)
		R_BloomBlend(sourcetex, r_refdef.vrect.x, r_refdef.vrect.y, r_refdef.vrect.width, r_refdef.vrect.height);

	GLBE_FBO_Sources(r_nulltex, r_nulltex);

//	if (gl_motionblur.value>0 && gl_motionblur.value < 1 && qglCopyTexImage2D)
//		R_RenderMotionBlur();

	checkglerror();
}

#endif
