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

// draw.c -- this is the only file outside the refresh that touches the
// vid buffer

#include "quakedef.h"
#ifdef GLQUAKE
#include "glquake.h"
#include "shader.h"
#include "gl_draw.h"


extern cvar_t		gl_max_size;
extern cvar_t		gl_picmip;
extern cvar_t		gl_lerpimages;
extern cvar_t		gl_picmip2d;
extern cvar_t		gl_compress;
extern cvar_t		gl_smoothcrosshair;
extern cvar_t		gl_texturemode, gl_texture_anisotropic_filtering;

float	gl_anisotropy_factor;

static int	gl_filter_pic[3];	//ui elements
static int	gl_filter_mip[3];	//everything else
int		gl_mipcap_min = 0;
int		gl_mipcap_max = 1000;

void GL_DestroyTexture(texid_t tex)
{
	if (!tex)
		return;
	if (tex->num)
		qglDeleteTextures(1, &tex->num);
	tex->num = 0;
}

/*
===============
Draw_Init
===============
*/
void GLDraw_Init (void)
{
	//figure out which extra features we can support on these drivers.
	r_deluxmapping = r_deluxmapping_cvar.ival;
	r_lightprepass = r_lightprepass_cvar.ival && sh_config.progs_supported;
	r_softwarebanding = r_softwarebanding_cvar.ival && sh_config.progs_supported;
	if (gl_config.gles && gl_config.glversion < 3.0)
		r_softwarebanding = false;

	if (gl_config.arb_framebuffer_srgb)
	{
		extern cvar_t vid_srgb;
		vid.srgb = vid_srgb.ival>1;
		if (vid.srgb)
			qglEnable(GL_FRAMEBUFFER_SRGB);
	}
	else
		vid.srgb = false;

	R2D_Init();

	qglDisable(GL_SCISSOR_TEST);
	GL_Set2D(false);

	qglClearColor(0, 0, 0, 1);
	qglClear(GL_COLOR_BUFFER_BIT);
	{
		mpic_t *pic = R2D_SafeCachePic ("gfx/loading.lmp");
		if (pic)
			R2D_ScalePic ( ((int)vid.width - pic->width)/2,
				((int)vid.height - 48 - pic->height)/2, pic->width, pic->height, pic);
	}

	if (R2D_Flush)
		R2D_Flush();
	VID_SwapBuffers();

	GL_SetupSceneProcessingTextures();

	//
	// get the other pics we need
	//
	TRACE(("dbg: GLDraw_ReInit: R2D_SafePicFromWad\n"));
	draw_disc = R2D_SafePicFromWad ("disc");

#ifdef GL_USE8BITTEX
	inited15to8 = false;
#endif

	qglClearColor (1,0,0,1);

	TRACE(("dbg: GLDraw_ReInit: PPL_LoadSpecularFragmentProgram\n"));
	GL_InitSceneProcessingShaders();

//	Cmd_AddCommandD ("r_imagelist", GLDraw_ImageList_f, "Debug command. Reveals current list of loaded images.");
}

void GLDraw_DeInit (void)
{
	Cmd_RemoveCommand ("r_imagelist");

	R2D_Shutdown();

	R_GAliasFlushSkinCache(true);

	draw_disc = NULL;
	GL_ShutdownPostProcessing();

#ifdef RTLIGHTS
	Sh_Shutdown();
#endif
	Shader_Shutdown();

	GLBE_Shutdown();	//to release its images.
	Image_Shutdown();
}



//=============================================================================

/*
================
GL_Set2D

Setup as if the screen was 320*200
================
*/
void GL_Set2D (qboolean flipped)
{
	extern cvar_t gl_screenangle;
	float rad, ang;
	float tmp[16], tmp2[16];
	float w = vid.width, h = vid.height;
	qboolean fbo = !!*r_refdef.rt_destcolour[0].texname;

	if (vid.framebuffer)
	{
		vid.fbvwidth = vid.width;
		vid.fbvheight = vid.height;
		vid.fbpwidth = vid.framebuffer->width;
		vid.fbpheight = vid.framebuffer->height;
	}
	else if (fbo)
	{
		R2D_RT_GetTexture(r_refdef.rt_destcolour[0].texname, &vid.fbpwidth, &vid.fbpheight);
		vid.fbvwidth = vid.fbpwidth;
		vid.fbvheight = vid.fbpheight;

		if (strcmp(r_refdef.rt_destcolour[0].texname, "megascreeny"))
			flipped ^= true;
	}
	else
	{
		vid.fbvwidth = vid.width;
		vid.fbvheight = vid.height;
		vid.fbpwidth = vid.pixelwidth;
		vid.fbpheight = vid.pixelheight;
	}

	ang = (gl_screenangle.value>0?(gl_screenangle.value+45):(gl_screenangle.value-45))/90;
	ang = (int)ang * 90;
	if (ang && !fbo)
	{ /*more expensive maths*/
		rad = (ang * M_PI) / 180;

		w = fabs(cos(rad)) * (vid.width) + fabs(sin(rad)) * (vid.height);
		h = fabs(sin(rad)) * (vid.width) + fabs(cos(rad)) * (vid.height);

		Matrix4x4_CM_Orthographic(r_refdef.m_projection, w/-2.0f, w/2.0f, h/2.0f, h/-2.0f, -99999, 99999);

		Matrix4x4_Identity(tmp);
		Matrix4_Multiply(Matrix4x4_CM_NewTranslation((vid.width/-2.0f), (vid.height/-2.0f), 0), tmp, tmp2);
		Matrix4_Multiply(Matrix4x4_CM_NewRotation(-ang,  0, 0, 1), tmp2, r_refdef.m_view);
	}
	else
	{
		w = vid.fbvwidth;
		h = vid.fbvheight;
		if (flipped)
			Matrix4x4_CM_Orthographic(r_refdef.m_projection, 0, w, 0, h, -99999, 99999);
		else
			Matrix4x4_CM_Orthographic(r_refdef.m_projection, 0, w, h, 0, -99999, 99999);
		Matrix4x4_Identity(r_refdef.m_view);
	}
	//current physical position on the current render target.
	r_refdef.pxrect.x = 0;
	r_refdef.pxrect.y = 0;
	r_refdef.pxrect.width = vid.fbpwidth;
	r_refdef.pxrect.height = vid.fbpheight;
	r_refdef.pxrect.maxheight = vid.fbpheight;
	r_refdef.time = realtime;
	/*flush that gl state*/
	GL_ViewportUpdate();

	if (qglLoadMatrixf)
	{
		qglMatrixMode(GL_PROJECTION);
		qglLoadMatrixf(r_refdef.m_projection);

		qglMatrixMode(GL_MODELVIEW);
		qglLoadMatrixf(r_refdef.m_view);
	}

	if (flipped)
		r_refdef.flipcull = SHADER_CULL_FLIP;
	else
		r_refdef.flipcull = 0;

	GL_SetShaderState2D(true);
}

//====================================================================

//note: needs to be bound first, so the 'targ' argument shouldn't be a problem.
static void GL_Texturemode_Apply(GLenum targ, unsigned int flags)
{
	int min, mag;
	int *filter = (flags & IF_UIPIC)?gl_filter_pic:gl_filter_mip;

	if (targ == GL_TEXTURE_CUBE_MAP_ARB)
		flags |= IF_NOMIPMAP;

	if ((filter[2] && !(flags & IF_NEAREST)) || (flags & IF_LINEAR))
		mag = GL_LINEAR;
	else
		mag = GL_NEAREST;
	if (filter[1] == -1 || (flags & IF_NOMIPMAP))
	{
		if ((filter[0] && !(flags & IF_NEAREST)) || (flags & IF_LINEAR))
			min = GL_LINEAR;
		else
			min = GL_NEAREST;
	}
	else
	{
		if ((filter[1] && !(flags & IF_NEAREST)) || (flags & IF_LINEAR))
		{
			if ((filter[0] && !(flags & IF_NEAREST)) || (flags & IF_LINEAR))
				min = GL_LINEAR_MIPMAP_LINEAR;
			else
				min = GL_NEAREST_MIPMAP_LINEAR;
		}
		else
		{
			if ((filter[0] && !(flags & IF_NEAREST)) || (flags & IF_LINEAR))
				min = GL_LINEAR_MIPMAP_NEAREST;
			else
				min = GL_NEAREST_MIPMAP_NEAREST;
		}
	}

	qglTexParameteri(targ, GL_TEXTURE_MIN_FILTER, min);
	qglTexParameteri(targ, GL_TEXTURE_MAG_FILTER, mag);
	if (gl_anisotropy_factor)	//0 means driver doesn't support
	{
		//only use anisotrophy when using linear any linear, because of drivers that forces linear sampling when anis is active (annoyingly this is allowed by the spec).
		//(also protects r_softwarebanding)
		if (min == GL_LINEAR_MIPMAP_LINEAR || min == GL_LINEAR_MIPMAP_NEAREST)
			qglTexParameterf(targ, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_anisotropy_factor);
		else
			qglTexParameterf(targ, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
	}
}

qboolean GL_LoadTextureMips(texid_t tex, const struct pendingtextureinfo *mips)
{
	static int cubeface[] =
	{
		GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB
	};
	int targ, targface;
	int i, j;
	int nummips = mips->mipcount;
	int encoding = mips->encoding;
	qboolean compress;


	if (gl_config.gles)
	{
		//gles requires that the internal format must match format
		//this means we can't specify 24.0 modes with a 24.8 datatype.
		//arguably we shouldn't do this anyway, but there are differences that q3 shaders can notice.
		//fixme: move elsewhere?
		if (encoding == PTI_RGBX8)
			encoding = PTI_RGBA8;
		if (encoding == PTI_BGRX8)
			encoding = PTI_BGRA8;
	}
	
	if (!tex->num)
		qglGenTextures(1, &tex->num);

	switch((tex->flags & IF_TEXTYPE) >> IF_TEXTYPESHIFT)
	{
	default:
	case 0:
		targ = GL_TEXTURE_2D;
		break;
	case 1:
		targ = GL_TEXTURE_3D;
		break;
	case 2:
		targ = GL_TEXTURE_CUBE_MAP_ARB;
		break;
	}

	GL_MTBind(0, targ, tex);

	if (tex->flags&IF_CLAMP)
	{
		qglTexParameteri(targ, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		qglTexParameteri(targ, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		if (targ != GL_TEXTURE_2D)
			qglTexParameteri(targ, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	}
	else
	{
		qglTexParameteri(targ, GL_TEXTURE_WRAP_S, GL_REPEAT);
		qglTexParameteri(targ, GL_TEXTURE_WRAP_T, GL_REPEAT);
		if (targ != GL_TEXTURE_2D)
			qglTexParameteri(targ, GL_TEXTURE_WRAP_R, GL_REPEAT);
	}

	if (targ == GL_TEXTURE_2D && nummips > 1)
	{	//npot mipmapped textures are awkward.
		//opengl floors.
		for (i = 1; i < nummips; i++)
		{
			if (mips->mip[i].width != max(1,(mips->mip[i-1].width>>1)) ||
				mips->mip[i].height != max(1,(mips->mip[i-1].height>>1)))
			{	//okay, this mip looks like it was sized wrongly. this can easily happen with npot dds files made for direct3d.
				nummips = i;
				break;
			}
		}
	}

	//make sure the texture is complete even if the mips are not.
	//note that some drivers will just ignore levels that are not valid.
	//this means that we can't make this setting dynamic, so we might as well let the drivers know BEFORE we do the uploads, to be kind to those that are buggy..
	//this is available in gles3
	if (sh_config.can_mipcap)
	{
		if (targ != GL_TEXTURE_CUBE_MAP_ARB)
		{
			if (tex->flags & IF_MIPCAP)
			{
				qglTexParameteri(targ, GL_TEXTURE_BASE_LEVEL, min(nummips-1, gl_mipcap_min));
				qglTexParameteri(targ, GL_TEXTURE_MAX_LEVEL, min(nummips-1, gl_mipcap_max));
			}
			else
			{
				qglTexParameteri(targ, GL_TEXTURE_BASE_LEVEL, 0);
				qglTexParameteri(targ, GL_TEXTURE_MAX_LEVEL, nummips-1);
			}
		}
	}

//	tex->width = mips->mip[0].width;
//	tex->height = mips->mip[0].height;
	GL_Texturemode_Apply(targ, tex->flags);
	if (targ == GL_TEXTURE_3D)
	{
		targface = targ;
		for (i = 0; i < nummips; i++)
		{
			int size = mips->mip[i].height;
			switch(encoding)
			{
			case PTI_RGBX8:
				qglTexImage3D(targface, i, GL_RGB, size, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, mips->mip[i].data);
				break;
			case PTI_RGBA8:
				qglTexImage3D(targface, i, GL_RGBA, size, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, mips->mip[i].data);
				break;
			case PTI_BGRX8:
				qglTexImage3D(targface, i, GL_RGB, size, size, size, 0, GL_BGRA_EXT, GL_UNSIGNED_INT_8_8_8_8_REV, mips->mip[i].data);
				break;
			default:
			case PTI_BGRA8:
				qglTexImage3D(targface, i, GL_RGBA, size, size, size, 0, GL_BGRA_EXT, GL_UNSIGNED_INT_8_8_8_8_REV, mips->mip[i].data);
				break;
			case PTI_RGBA4444:
				qglTexImage3D(targface, i, GL_RGBA, size, size, size, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, mips->mip[i].data);
				break;
			case PTI_RGBA5551:
				qglTexImage3D(targface, i, GL_RGBA, size, size, size, 0, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, mips->mip[i].data);
				break;
			case PTI_RGB565:
				qglTexImage3D(targface, i, GL_RGB, size, size, size, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, mips->mip[i].data);
				break;
			}
		}
	}
	else
	{
		//2d or cubemaps
		for (i = 0; i < nummips; i++)
		{
			//arb_texture_compression is core in gl1.3
			//gles doesn't support autocompression as of gles3.
			//only autocompress if we have actually have data (gl errors otherwise).
			if (gl_config.arb_texture_compression && mips->mip[i].data)
				compress = !!gl_compress.ival;
			else
				compress = false;

			if (tex->flags & IF_TEXTYPE)
			{
				targface = cubeface[i];
				j = 0;
			}
			else
			{
				targface = targ;
				j = i;
			}
			switch(encoding)
			{
#ifdef FTE_TARGET_WEB
			case PTI_WHOLEFILE:
				if (!i)
					emscriptenfte_gl_loadtexturefile(tex->num, &tex->width, &tex->height, mips->mip[i].data, mips->mip[i].datasize);
				break;
#endif
			case PTI_DEPTH16:
				qglTexImage2D(targface, j, gl_config.gles?GL_DEPTH_COMPONENT:GL_DEPTH_COMPONENT16_ARB, mips->mip[i].width, mips->mip[i].height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, mips->mip[i].data);
				break;
			case PTI_DEPTH24:
				qglTexImage2D(targface, j, gl_config.gles?GL_DEPTH_COMPONENT:GL_DEPTH_COMPONENT24_ARB, mips->mip[i].width, mips->mip[i].height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, mips->mip[i].data);
				break;
			case PTI_DEPTH32:
				qglTexImage2D(targface, j, gl_config.gles?GL_DEPTH_COMPONENT:GL_DEPTH_COMPONENT32_ARB, mips->mip[i].width, mips->mip[i].height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, mips->mip[i].data);
				break;
			case PTI_DEPTH24_8:
				qglTexImage2D(targface, j, GL_DEPTH24_STENCIL8_EXT, mips->mip[i].width, mips->mip[i].height, 0, GL_DEPTH_STENCIL_EXT, GL_UNSIGNED_INT_24_8_EXT, mips->mip[i].data);
				break;
			//32bit formats
			case PTI_RGBX8:
				qglTexImage2D(targface, j, compress?GL_COMPRESSED_RGB_ARB:GL_RGB, mips->mip[i].width, mips->mip[i].height, 0, GL_RGBA, GL_UNSIGNED_BYTE, mips->mip[i].data);
				break;
			case PTI_RGBA8:
				qglTexImage2D(targface, j, compress?GL_COMPRESSED_RGBA_ARB:GL_RGBA, mips->mip[i].width, mips->mip[i].height, 0, GL_RGBA, GL_UNSIGNED_BYTE, mips->mip[i].data);
				break;
			case PTI_BGRX8:
				qglTexImage2D(targface, j, compress?GL_COMPRESSED_RGB_ARB:GL_RGB, mips->mip[i].width, mips->mip[i].height, 0, GL_BGRA_EXT, GL_UNSIGNED_INT_8_8_8_8_REV, mips->mip[i].data);
				break;
			default:
			case PTI_BGRA8:
				qglTexImage2D(targface, j, compress?GL_COMPRESSED_RGBA_ARB:GL_RGBA, mips->mip[i].width, mips->mip[i].height, 0, GL_BGRA_EXT, GL_UNSIGNED_INT_8_8_8_8_REV, mips->mip[i].data);
				break;
			case PTI_RGBX8_SRGB:
				qglTexImage2D(targface, j, compress?GL_COMPRESSED_SRGB_EXT:GL_SRGB_EXT, mips->mip[i].width, mips->mip[i].height, 0, GL_RGBA, GL_UNSIGNED_BYTE, mips->mip[i].data);
				break;
			case PTI_RGBA8_SRGB:
				qglTexImage2D(targface, j, compress?GL_COMPRESSED_SRGB_ALPHA_EXT:GL_SRGB_ALPHA_EXT, mips->mip[i].width, mips->mip[i].height, 0, gl_config.gles?GL_SRGB_ALPHA_EXT:GL_RGBA, GL_UNSIGNED_BYTE, mips->mip[i].data);
				break;
			case PTI_BGRX8_SRGB:
				qglTexImage2D(targface, j, compress?GL_COMPRESSED_SRGB_EXT:GL_SRGB_EXT, mips->mip[i].width, mips->mip[i].height, 0, GL_BGRA_EXT, GL_UNSIGNED_INT_8_8_8_8_REV, mips->mip[i].data);
				break;
			case PTI_BGRA8_SRGB:
				qglTexImage2D(targface, j, compress?GL_COMPRESSED_SRGB_ALPHA_EXT:GL_SRGB_ALPHA_EXT, mips->mip[i].width, mips->mip[i].height, 0, GL_BGRA_EXT, GL_UNSIGNED_INT_8_8_8_8_REV, mips->mip[i].data);
				break;

			case PTI_RGBA16F:
				qglTexImage2D(targface, j, GL_RGBA16F_ARB, mips->mip[i].width, mips->mip[i].height, 0, GL_RGBA, GL_UNSIGNED_BYTE, mips->mip[i].data);
				break;
			case PTI_RGBA32F:
				qglTexImage2D(targface, j, GL_RGBA32F_ARB, mips->mip[i].width, mips->mip[i].height, 0, GL_RGBA, GL_UNSIGNED_BYTE, mips->mip[i].data);
				break;
			//16bit formats
			case PTI_RGBA4444:
				qglTexImage2D(targface, j, compress?GL_COMPRESSED_RGBA_ARB:GL_RGBA, mips->mip[i].width, mips->mip[i].height, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, mips->mip[i].data);
				break;
			case PTI_RGBA5551:
				qglTexImage2D(targface, j, compress?GL_COMPRESSED_RGBA_ARB:GL_RGBA, mips->mip[i].width, mips->mip[i].height, 0, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, mips->mip[i].data);
				break;
			case PTI_ARGB4444:
				qglTexImage2D(targface, j, compress?GL_COMPRESSED_RGBA_ARB:GL_RGBA, mips->mip[i].width, mips->mip[i].height, 0, GL_BGRA_EXT, GL_UNSIGNED_SHORT_4_4_4_4_REV, mips->mip[i].data);
				break;
			case PTI_ARGB1555:
				qglTexImage2D(targface, j, compress?GL_COMPRESSED_RGBA_ARB:GL_RGBA, mips->mip[i].width, mips->mip[i].height, 0, GL_BGRA_EXT, GL_UNSIGNED_SHORT_1_5_5_5_REV, mips->mip[i].data);
				break;
			case PTI_RGB565:
				qglTexImage2D(targface, j, compress?GL_COMPRESSED_RGBA_ARB:GL_RGB, mips->mip[i].width, mips->mip[i].height, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, mips->mip[i].data);
				break;
			//(desktop) compressed formats
			case PTI_S3RGB1:
				qglCompressedTexImage2DARB(targface, j, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, mips->mip[i].width, mips->mip[i].height, 0, mips->mip[i].datasize, mips->mip[i].data);
				break;
			case PTI_S3RGBA1:
				qglCompressedTexImage2DARB(targface, j, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, mips->mip[i].width, mips->mip[i].height, 0, mips->mip[i].datasize, mips->mip[i].data);
				break;
			case PTI_S3RGBA3:
				qglCompressedTexImage2DARB(targface, j, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, mips->mip[i].width, mips->mip[i].height, 0, mips->mip[i].datasize, mips->mip[i].data);
				break;
			case PTI_S3RGBA5:
				qglCompressedTexImage2DARB(targface, j, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, mips->mip[i].width, mips->mip[i].height, 0, mips->mip[i].datasize, mips->mip[i].data);
				break;
			//(mobile) compressed formats
			case PTI_ETC1_RGB8:
			case PTI_ETC2_RGB8:
				//etc2 is a superset of etc1. we distinguish only for hardware that cannot recognise etc2's 'invalid' encodings
				if (sh_config.texfmt[PTI_ETC2_RGB8])
					qglCompressedTexImage2DARB(targface, j, GL_COMPRESSED_RGB8_ETC2, mips->mip[i].width, mips->mip[i].height, 0, mips->mip[i].datasize, mips->mip[i].data);
				else
					qglCompressedTexImage2DARB(targface, j, GL_ETC1_RGB8_OES, mips->mip[i].width, mips->mip[i].height, 0, mips->mip[i].datasize, mips->mip[i].data);
				break;
			case PTI_ETC2_RGB8A1:
				qglCompressedTexImage2DARB(targface, j, GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2, mips->mip[i].width, mips->mip[i].height, 0, mips->mip[i].datasize, mips->mip[i].data);
				break;
			case PTI_ETC2_RGB8A8:
				qglCompressedTexImage2DARB(targface, j, GL_COMPRESSED_RGBA8_ETC2_EAC, mips->mip[i].width, mips->mip[i].height, 0, mips->mip[i].datasize, mips->mip[i].data);
				break;
			}
		}
	}

	return true;
}

void GL_UpdateFiltering(image_t *imagelist, int filtermip[3], int filterpic[3], int mipcap[2], float anis)
{
	int targ;
	image_t *img;

	gl_mipcap_min = mipcap[0];
	gl_mipcap_max = mipcap[1];

	VectorCopy(filterpic, gl_filter_pic);
	VectorCopy(filtermip, gl_filter_mip);

	//bound carefully, so that we get 0 if anisrophy is not supported at all (1 is fine). we can then test against 0 (which is an otherwise-invalid value) avoiding gl errors.
	if (anis > gl_config.ext_texture_filter_anisotropic)
		gl_anisotropy_factor = gl_config.ext_texture_filter_anisotropic;
	else if (anis < 1)
		gl_anisotropy_factor = 1;
	else
		gl_anisotropy_factor = anis;

	// change all the existing mipmap texture objects
	for (img=imagelist ; img ; img=img->next)
	{
		if (img->status != TEX_LOADED)
			continue;
		switch((img->flags & IF_TEXTYPE) >> IF_TEXTYPESHIFT)
		{
		case 0:
			targ = GL_TEXTURE_2D;
			break;
		case 1:
			targ = GL_TEXTURE_3D;
			break;
		default:
			targ = GL_TEXTURE_CUBE_MAP_ARB;
			break;
		}

		GL_MTBind(0, targ, img);
		GL_Texturemode_Apply(targ, img->flags);

		//should we do dynamic mipcap settings? this bugs out ATI.
		/*
		if (!gl_config.gles && (tex->flags & IF_MIPCAP))
		{
			qglTexParameteri(targ, GL_TEXTURE_BASE_LEVEL, gl_mipcap_min);
			qglTexParameteri(targ, GL_TEXTURE_MAX_LEVEL, gl_mipcap_max);
		}
		*/
	}
}

#endif
