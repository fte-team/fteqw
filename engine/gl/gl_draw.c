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

#define glfmtsw(qfmt,sz,in,fm,ty,cf,sr,sg,sb,sa)		\
	do {												\
		gl_config.formatinfo[qfmt].sizedformat = sz;	\
		gl_config.formatinfo[qfmt].cformat = cf;		\
		gl_config.formatinfo[qfmt].internalformat = in;	\
		gl_config.formatinfo[qfmt].format = fm;			\
		gl_config.formatinfo[qfmt].type = ty;			\
		gl_config.formatinfo[qfmt].swizzle_r = sr;		\
		gl_config.formatinfo[qfmt].swizzle_g = sg;		\
		gl_config.formatinfo[qfmt].swizzle_b = sb;		\
		gl_config.formatinfo[qfmt].swizzle_a = sa;		\
		sh_config.texfmt[qfmt] = true;					\
	} while(0)

#define glfmt(qfmt,sz,in,fm,ty)			glfmtsw(qfmt, sz, in, fm, ty, 0,  GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA)
#define glfmtc(qfmt,sz,in,fm,ty,cf)		glfmtsw(qfmt, sz, in, fm, ty, cf, GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA)
#define glfmtb(qfmt,in)					glfmtsw(qfmt, in, in, 0,  0,  0,  GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA)

#ifndef GL_RGB565
#define GL_RGB565                         0x8D62
#endif

void GL_SetupFormats(void)
{
	int tc_ru = 0, tc_rs = 0, tc_rgu = 0, tc_rgs = 0, tc_rgb = 0, tc_rgba1 = 0, tc_rgba8 = 0, tc_srgb = 0, tc_srgba8 = 0;

	qboolean bc1=false, bc2=false, bc3=false, bc45=false, bc67=false;
	float ver = gl_config.glversion;
	qboolean srgb = (gl_config.glversion >= (gl_config_gles?3.0:2.1)) || GL_CheckExtension("GL_EXT_texture_sRGB");

	if (gl_config_gles && ver >= 3.0 && ver <= 3.3)
		ver = 3.3;	//treat gles3.0 as desktop 3.3, they're roughly equivelent in feature set.

	if (GL_CheckExtension("GL_EXT_texture_compression_s3tc"))
		bc1=bc2=bc3=true;
	if ((!gl_config_gles && ver >= 3.0) || GL_CheckExtension("GL_ARB_texture_compression_rgtc") || GL_CheckExtension("GL_EXT_texture_compression_rgtc"))
		bc45 = true;
	if ((!gl_config.gles && ver >= 4.2) || GL_CheckExtension("GL_ARB_texture_compression_bptc"))
		bc67 = true;

	if (bc45)
		tc_ru = GL_COMPRESSED_RED_RGTC1;
	if (bc45)
		tc_rs = GL_COMPRESSED_SIGNED_RED_RGTC1;
	if (bc45)
		tc_rgu = GL_COMPRESSED_RG_RGTC2;
	if (bc45)
		tc_rgs = GL_COMPRESSED_SIGNED_RG_RGTC2;

	if (bc1)
		tc_rgb = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
	if (bc1)
		tc_srgb = GL_COMPRESSED_SRGB_S3TC_DXT1_EXT;
	if (bc3)
		tc_rgba8 = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
	if (bc3)
		tc_srgba8 = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
	if (bc1)
		tc_rgba1 = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
//	if (bc1)
//		tc_srgba1 = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;

	bc1 |= GL_CheckExtension("GL_EXT_texture_compression_dxt1");
	bc2 |= GL_CheckExtension("GL_ANGLE_texture_compression_dxt3");
	bc3 |= GL_CheckExtension("GL_ANGLE_texture_compression_dxt5");

	/*else if (sh_config.texfmt[PTI_ETC2_RGB8A8])
	{	//these are probably a bad choice...
		tc_ru = GL_COMPRESSED_R11_EAC;
		tc_rgu = GL_COMPRESSED_RG11_EAC;
		tc_rgb = GL_COMPRESSED_RGB8_ETC2;
		tc_rgba = GL_COMPRESSED_RGBA8_ETC2_EAC;
	}*/

#ifdef FTE_TARGET_WEB
	glfmt(PTI_WHOLEFILE, 0, 0, 0, 0);
//	sh_config.texfmt[PTI_WHOLEFILE] = true;
#endif

	if (gl_config_gles)
	{
		//pre-3 gles doesn't support sized formats, and only a limited number of them too
		glfmtc(PTI_RGB8,	GL_RGB,				GL_RGB,					GL_RGB,					GL_UNSIGNED_BYTE,			tc_rgb);
		glfmtc(PTI_RGBA8,	GL_RGBA,			GL_RGBA,				GL_RGBA,				GL_UNSIGNED_BYTE,			tc_rgba8);
		glfmt(PTI_L8A8,		GL_LUMINANCE_ALPHA,	GL_LUMINANCE_ALPHA,		GL_LUMINANCE_ALPHA,		GL_UNSIGNED_BYTE);
		glfmt(PTI_L8,		GL_LUMINANCE,		GL_LUMINANCE,			GL_LUMINANCE,			GL_UNSIGNED_BYTE);
//		glfmt(PTI_RGBA8,	GL_ALPHA,			GL_ALPHA,				GL_ALPHA,				GL_UNSIGNED_BYTE);

		if (!gl_config.webgl_ie)
		{	//these should work on all gles2+webgl1 devices, but microsoft doesn't give a shit.
			glfmtc(PTI_RGB565,	GL_RGB,				GL_RGB,					GL_RGB,					GL_UNSIGNED_SHORT_5_6_5,	tc_rgb);
//			glfmtc(PTI_RGBA4444,GL_RGBA,			GL_RGBA,				GL_RGBA,				GL_UNSIGNED_SHORT_4_4_4_4,	tc_rgba8);
//			glfmtc(PTI_RGBA5551,GL_RGBA,			GL_RGBA,				GL_RGBA,				GL_UNSIGNED_SHORT_5_5_5_1,	tc_rgba1);
		}
		if (GL_CheckExtension("GL_OES_texture_half_float")) 
			glfmtc(PTI_RGBA16F,	GL_RGBA,			GL_RGBA,				GL_RGBA,				GL_HALF_FLOAT_OES,		0);	//not to be confused with GL_HALF_FLOAT[_ARB] which has a different value
		if (GL_CheckExtension("GL_OES_texture_float"))
			glfmtc(PTI_RGBA32F,	GL_RGBA,			GL_RGBA,				GL_RGBA,				GL_FLOAT,				0);

		if (GL_CheckExtension("GL_OES_depth_texture"))
		{	//16+32, not 24.
			glfmt(PTI_DEPTH16,	GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT,	GL_DEPTH_COMPONENT,		GL_UNSIGNED_SHORT);
			glfmt(PTI_DEPTH32,	GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT,	GL_DEPTH_COMPONENT,		GL_UNSIGNED_INT);
		}

		if (GL_CheckExtension("GL_EXT_texture_format_BGRA8888"))
			glfmtc(PTI_BGRA8,	GL_BGRA_EXT,		GL_BGRA_EXT,			GL_BGRA_EXT,			GL_UNSIGNED_BYTE,		tc_rgba8);
		if (GL_CheckExtension("GL_EXT_texture_type_2_10_10_10_REV"))
			glfmtc(PTI_BGRA8,	GL_RGBA,			GL_RGBA,				GL_RGBA,				GL_UNSIGNED_INT_2_10_10_10_REV,		tc_rgba8);
	}
	if (!gl_config_gles || ver >= 3.0)
	{
		if (ver >= 1.4 || GL_CheckExtension("GL_ARB_depth_texture"))
		{	//depth formats
			glfmt(PTI_DEPTH16,	GL_DEPTH_COMPONENT16_ARB, GL_DEPTH_COMPONENT,	GL_DEPTH_COMPONENT,		GL_UNSIGNED_SHORT);
			glfmt(PTI_DEPTH24,	GL_DEPTH_COMPONENT24_ARB, GL_DEPTH_COMPONENT,	GL_DEPTH_COMPONENT,		GL_UNSIGNED_INT/*FIXME*/);
			if (gl_config_gles)// || ver >= 3.0)
				glfmt(PTI_DEPTH32,	GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT,	GL_DEPTH_COMPONENT,		GL_FLOAT);
			else
				glfmt(PTI_DEPTH32,	GL_DEPTH_COMPONENT32_ARB, GL_DEPTH_COMPONENT,	GL_DEPTH_COMPONENT,		GL_FLOAT);
		}
//		if (ver >= 3.0)
//			glfmt(PTI_DEPTH32,	GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT,	GL_DEPTH_COMPONENT,		GL_FLOAT);
		if (GL_CheckExtension("GL_EXT_packed_depth_stencil"))
			glfmt(PTI_DEPTH24_8,GL_DEPTH24_STENCIL8_EXT,  GL_DEPTH_STENCIL_EXT,	GL_DEPTH_STENCIL_EXT,	GL_UNSIGNED_INT_24_8_EXT);

		glfmtc(PTI_RGBA8,		GL_RGBA8,			GL_RGBA,				GL_RGBA,				GL_UNSIGNED_BYTE,	tc_rgba8);
		if (srgb)
			glfmtc(PTI_RGBA8_SRGB,	GL_SRGB8_ALPHA8_EXT,GL_SRGB_ALPHA_EXT,		GL_RGBA,				GL_UNSIGNED_BYTE,	tc_srgba8);
		if (!gl_config_gles)
		{
			if (ver >= 3.3)	//I'm paranoid about performance, so lets swizzle the alpha to 1 to make the alignment explicit.
				glfmtsw(PTI_RGBX8,		GL_RGBA8,			GL_RGBA,				GL_RGBA,				GL_UNSIGNED_BYTE,	tc_rgb,		GL_RED, GL_GREEN, GL_BLUE, GL_ONE);
			else
				glfmtc(PTI_RGBX8,		GL_RGB8,			GL_RGB,					GL_RGBA,				GL_UNSIGNED_BYTE,	tc_rgb);
			if (srgb)
				glfmtc(PTI_RGBX8_SRGB,	GL_SRGB8_EXT,		GL_SRGB_EXT,			GL_RGBA,				GL_UNSIGNED_BYTE,	tc_srgb);
		}
		if (ver >= 1.2 && !gl_config_gles)
		{
			glfmt(PTI_BGR8,			GL_RGB8,			GL_RGB,					GL_BGR_EXT,				GL_UNSIGNED_BYTE);
			glfmtc(PTI_BGRX8,		GL_RGB8,			GL_RGB,					GL_BGRA_EXT,			GL_UNSIGNED_INT_8_8_8_8_REV,	tc_rgb);
			glfmtc(PTI_BGRA8,		GL_RGBA8,			GL_RGBA,				GL_BGRA_EXT,			GL_UNSIGNED_INT_8_8_8_8_REV,	tc_rgba8);
			if (srgb)
			{
				glfmtc(PTI_BGRX8_SRGB,	GL_SRGB8_EXT,		GL_SRGB_EXT,			GL_BGRA_EXT,			GL_UNSIGNED_BYTE,	tc_srgb);
				glfmtc(PTI_BGRA8_SRGB,	GL_SRGB8_ALPHA8_EXT,GL_SRGB_ALPHA_EXT,		GL_BGRA_EXT,			GL_UNSIGNED_BYTE,	tc_srgba8);
			}
		}
		else if (ver >= 3.3)
		{
			glfmtsw(PTI_BGR8,		GL_RGB8,			GL_RGB,					GL_RGB,				GL_UNSIGNED_BYTE,	0,			GL_BLUE, GL_GREEN, GL_RED, GL_ONE);
			glfmtsw(PTI_BGRX8,		GL_RGB8,			GL_RGB,					GL_RGBA,			GL_UNSIGNED_BYTE,	tc_rgb,		GL_BLUE, GL_GREEN, GL_RED, GL_ONE);
			glfmtsw(PTI_BGRA8,		GL_RGBA8,			GL_RGBA,				GL_RGBA,			GL_UNSIGNED_BYTE,	tc_rgba8,	GL_BLUE, GL_GREEN, GL_RED, GL_ALPHA);
			if (srgb)
			{
				glfmtc(PTI_BGRX8_SRGB,	GL_SRGB8_EXT,		GL_SRGB_EXT,			GL_BGRA_EXT,			GL_UNSIGNED_BYTE,	tc_srgb);
				glfmtc(PTI_BGRA8_SRGB,	GL_SRGB8_ALPHA8_EXT,GL_SRGB_ALPHA_EXT,		GL_BGRA_EXT,			GL_UNSIGNED_BYTE,	tc_srgba8);
			}
		}
		if (ver >= 3.0 || GL_CheckExtension("GL_EXT_texture_shared_exponent"))
			glfmt(PTI_E5BGR9,		GL_RGB9_E5,			GL_RGB9_E5,				GL_RGB,					GL_UNSIGNED_INT_5_9_9_9_REV);
		if (ver >= 3.0 || GL_CheckExtension("GL_EXT_packed_pixels"))	//so gl1.2 then.
			glfmt(PTI_A2BGR10,		GL_RGB10_A2,		GL_RGB10_A2,			GL_RGBA,				GL_UNSIGNED_INT_2_10_10_10_REV);
		if (ver >= 3.0 || GL_CheckExtension("GL_ARB_texture_rg"))
		{
			glfmtc(PTI_R8,			GL_R8,				GL_RED,					GL_RED,					GL_UNSIGNED_BYTE,	tc_ru);
			glfmtc(PTI_RG8,			GL_RG8,				GL_RG,					GL_RG,					GL_UNSIGNED_BYTE,	tc_rs);
		}
		if (ver >= 3.1 || (GL_CheckExtension("GL_EXT_texture_snorm") && GL_CheckExtension("GL_ARB_texture_rg")))
		{
			glfmtc(PTI_R8_SNORM,	GL_R8_SNORM,		GL_R8_SNORM,			GL_RED,					GL_BYTE,			tc_rgu);
			glfmtc(PTI_RG8_SNORM,	GL_RG8_SNORM,		GL_RG8_SNORM,			GL_RG,					GL_BYTE,			tc_rgs);
		}

		if (ver >= 3.0)
		{
			glfmtc(PTI_RGBA16F,	GL_RGBA16F_ARB,		GL_RGBA,				GL_RGBA,				GL_HALF_FLOAT,		0);
			glfmtc(PTI_RGBA32F,	GL_RGBA32F_ARB,		GL_RGBA,				GL_RGBA,				GL_FLOAT,			0);
		}
		if (ver >= 1.2 && !gl_config_gles)
		{
			glfmtc(PTI_RGBA4444,	GL_RGBA4,			GL_RGBA,				GL_RGBA,				GL_UNSIGNED_SHORT_4_4_4_4,		tc_srgba8);
			glfmtc(PTI_RGBA5551,	GL_RGB5_A1,			GL_RGBA,				GL_RGBA,				GL_UNSIGNED_SHORT_5_5_5_1,		tc_rgba1);

			glfmtc(PTI_ARGB4444,	GL_RGBA4,			GL_RGBA,				GL_BGRA_EXT,			GL_UNSIGNED_SHORT_4_4_4_4_REV,	tc_srgba8);
			glfmtc(PTI_ARGB1555,	GL_RGB5_A1,			GL_RGBA,				GL_BGRA_EXT,			GL_UNSIGNED_SHORT_1_5_5_5_REV,	tc_rgba1);
		}
		if (gl_config_gles || ver > 4.1)	//rgb565 was a gles thing, desktop gl just has a 555 internal format despite the 565 data...
			glfmtc(PTI_RGB565,	GL_RGB565,			GL_RGB,					GL_RGB,					GL_UNSIGNED_SHORT_5_6_5,		tc_rgb);
		else
			glfmtc(PTI_RGB565,	GL_RGB5,			GL_RGB,					GL_RGB,					GL_UNSIGNED_SHORT_5_6_5,		tc_rgb);
		glfmt(PTI_RGB8,			GL_RGB8,			GL_RGB,					GL_RGB,					GL_UNSIGNED_BYTE);
		if (!gl_config_nofixedfunc)
		{	//if we have fixed function, then we still have proper support. the driver can emulate with swizzles if it wants.
			glfmtc(PTI_L8,		GL_LUMINANCE8,		GL_LUMINANCE,			GL_LUMINANCE,			GL_UNSIGNED_BYTE,	tc_ru);
			glfmtc(PTI_L8A8,	GL_LUMINANCE8_ALPHA8,GL_LUMINANCE_ALPHA,	GL_LUMINANCE_ALPHA,		GL_UNSIGNED_BYTE,	tc_rgu);
		}
		else if (ver >= 3.3)
		{	//can emulate them with swizzles.
			glfmtsw(PTI_L8,		GL_R8,				GL_RED,					GL_RED,					GL_UNSIGNED_BYTE,	tc_ru,	GL_RED, GL_RED, GL_RED, GL_ONE);
			glfmtsw(PTI_L8A8,	GL_RG8,				GL_RG,					GL_RG,					GL_UNSIGNED_BYTE,	tc_rgu,	GL_RED, GL_RED, GL_RED, GL_GREEN);
		}
	}

	//block compresion formats.
	if (bc1)
	{
		glfmtb(PTI_BC1_RGB,				GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
		glfmtb(PTI_BC1_RGBA,			GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);
		if (srgb)
		{
			glfmtb(PTI_BC1_RGB_SRGB,		GL_COMPRESSED_SRGB_S3TC_DXT1_EXT);
			glfmtb(PTI_BC1_RGBA_SRGB,		GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT);
		}
	}
	if (bc2)
	{
		glfmtb(PTI_BC2_RGBA,			GL_COMPRESSED_RGBA_S3TC_DXT3_EXT);
		if (srgb)
			glfmtb(PTI_BC2_RGBA_SRGB,		GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT);
	}
	if (bc3)
	{
		glfmtb(PTI_BC3_RGBA,			GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
		if (srgb)
			glfmtb(PTI_BC3_RGBA_SRGB,		GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT);
	}
	if (bc45)
	{
		glfmtb(PTI_BC4_R8,				GL_COMPRESSED_RED_RGTC1);
		glfmtb(PTI_BC4_R8_SNORM,		GL_COMPRESSED_SIGNED_RED_RGTC1);
		glfmtb(PTI_BC5_RG8,				GL_COMPRESSED_RG_RGTC2);
		glfmtb(PTI_BC5_RG8_SNORM,		GL_COMPRESSED_SIGNED_RG_RGTC2);
	}
	if (bc67)
	{
		glfmtb(PTI_BC6_RGB_UFLOAT,		GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB);
		glfmtb(PTI_BC6_RGB_SFLOAT,		GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB);
		glfmtb(PTI_BC7_RGBA,			GL_COMPRESSED_RGBA_BPTC_UNORM_ARB);
		glfmtb(PTI_BC7_RGBA_SRGB,		GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB);
	}

#ifdef FTE_TARGET_WEB
	if (GL_CheckExtension("WEBGL_compressed_texture_etc"))
#else
	if ((gl_config.gles && gl_config.glversion >= 3.0) || (!gl_config.gles && (gl_config.glversion >= 4.3 || GL_CheckExtension("GL_ARB_ES3_compatibility"))))
#endif
	{
		glfmtb(PTI_ETC1_RGB8,			GL_COMPRESSED_RGB8_ETC2);
		glfmtb(PTI_ETC2_RGB8,			GL_COMPRESSED_RGB8_ETC2);
		glfmtb(PTI_ETC2_RGB8A1,			GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2);
		glfmtb(PTI_ETC2_RGB8A8,			GL_COMPRESSED_RGBA8_ETC2_EAC);
		glfmtb(PTI_ETC2_RGB8_SRGB,		GL_COMPRESSED_SRGB8_ETC2);
		glfmtb(PTI_ETC2_RGB8A1_SRGB,	GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2);
		glfmtb(PTI_ETC2_RGB8A8_SRGB,	GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC);
		glfmtb(PTI_EAC_R11,				GL_COMPRESSED_R11_EAC);
		glfmtb(PTI_EAC_R11_SNORM,		GL_COMPRESSED_SIGNED_R11_EAC);
		glfmtb(PTI_EAC_RG11,			GL_COMPRESSED_RG11_EAC);
		glfmtb(PTI_EAC_RG11_SNORM,		GL_COMPRESSED_SIGNED_RG11_EAC);
	}
	else
	{
		if (GL_CheckExtension("GL_OES_compressed_ETC1_RGB8_texture"))
			glfmtb(PTI_ETC1_RGB8,			GL_ETC1_RGB8_OES);
		if (GL_CheckExtension("GL_OES_compressed_ETC2_RGB8_texture"))
			glfmtb(PTI_ETC2_RGB8,			GL_COMPRESSED_RGB8_ETC2);
		if (GL_CheckExtension("GL_OES_compressed_ETC2_sRGB8_texture"))
			glfmtb(PTI_ETC2_RGB8_SRGB,		GL_COMPRESSED_SRGB8_ETC2);
		if (GL_CheckExtension("GL_OES_compressed_ETC2_punchthroughA_RGBA8_texture"))
			glfmtb(PTI_ETC2_RGB8A1,			GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2);
		if (GL_CheckExtension("GL_OES_compressed_ETC2_punchthroughA_sRGB8_alpha_texture"))
			glfmtb(PTI_ETC2_RGB8A1_SRGB,	GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2);
		if (GL_CheckExtension("GL_OES_compressed_ETC2_RGBA8_texture"))
			glfmtb(PTI_ETC2_RGB8A8,			GL_COMPRESSED_RGBA8_ETC2_EAC);
		if (GL_CheckExtension("GL_OES_compressed_ETC2_sRGB8_alpha8_texture"))
			glfmtb(PTI_ETC2_RGB8A8_SRGB,	GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC);
		if (GL_CheckExtension("GL_OES_compressed_EAC_R11_unsigned_texture"))
			glfmtb(PTI_EAC_R11,				GL_COMPRESSED_R11_EAC);
		if (GL_CheckExtension("GL_OES_compressed_EAC_R11_signed_texture"))
			glfmtb(PTI_EAC_R11_SNORM,		GL_COMPRESSED_SIGNED_R11_EAC);
		if (GL_CheckExtension("GL_OES_compressed_EAC_RG11_unsigned_texture"))
			glfmtb(PTI_EAC_RG11,			GL_COMPRESSED_RG11_EAC);
		if (GL_CheckExtension("GL_OES_compressed_EAC_RG11_signed_texture"))
			glfmtb(PTI_EAC_RG11_SNORM,		GL_COMPRESSED_SIGNED_RG11_EAC);
	}

	if (GL_CheckExtension("GL_KHR_texture_compression_astc_ldr") || (gl_config_gles && gl_config.glversion >= 3.2))
	{	//astc ldr profile is a core part of gles 3.2
		glfmtb(PTI_ASTC_4X4,			GL_COMPRESSED_RGBA_ASTC_4x4_KHR);
		glfmtb(PTI_ASTC_4X4_SRGB,		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR);
		glfmtb(PTI_ASTC_5X4,			GL_COMPRESSED_RGBA_ASTC_5x4_KHR);
		glfmtb(PTI_ASTC_5X4_SRGB,		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR);
		glfmtb(PTI_ASTC_5X5,			GL_COMPRESSED_RGBA_ASTC_5x5_KHR);
		glfmtb(PTI_ASTC_5X5_SRGB,		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR);
		glfmtb(PTI_ASTC_6X5,			GL_COMPRESSED_RGBA_ASTC_6x5_KHR);
		glfmtb(PTI_ASTC_6X5_SRGB,		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR);
		glfmtb(PTI_ASTC_6X6,			GL_COMPRESSED_RGBA_ASTC_6x6_KHR);
		glfmtb(PTI_ASTC_6X6_SRGB,		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR);
		glfmtb(PTI_ASTC_8X5,			GL_COMPRESSED_RGBA_ASTC_8x5_KHR);
		glfmtb(PTI_ASTC_8X5_SRGB,		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR);
		glfmtb(PTI_ASTC_8X6,			GL_COMPRESSED_RGBA_ASTC_8x6_KHR);
		glfmtb(PTI_ASTC_8X6_SRGB,		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR);
		glfmtb(PTI_ASTC_10X5,			GL_COMPRESSED_RGBA_ASTC_10x5_KHR);
		glfmtb(PTI_ASTC_10X5_SRGB,		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR);
		glfmtb(PTI_ASTC_10X6,			GL_COMPRESSED_RGBA_ASTC_10x6_KHR);
		glfmtb(PTI_ASTC_10X6_SRGB,		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR);
		glfmtb(PTI_ASTC_8X8,			GL_COMPRESSED_RGBA_ASTC_8x8_KHR);
		glfmtb(PTI_ASTC_8X8_SRGB,		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR);
		glfmtb(PTI_ASTC_10X8,			GL_COMPRESSED_RGBA_ASTC_10x8_KHR);
		glfmtb(PTI_ASTC_10X8_SRGB,		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR);
		glfmtb(PTI_ASTC_10X10,			GL_COMPRESSED_RGBA_ASTC_10x10_KHR);
		glfmtb(PTI_ASTC_10X10_SRGB,		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR);
		glfmtb(PTI_ASTC_12X10,			GL_COMPRESSED_RGBA_ASTC_12x10_KHR);
		glfmtb(PTI_ASTC_12X10_SRGB,		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR);
		glfmtb(PTI_ASTC_12X12,			GL_COMPRESSED_RGBA_ASTC_12x12_KHR);
		glfmtb(PTI_ASTC_12X12_SRGB,		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR);
	}
}

/*
===============
Draw_Init
===============
*/
void GLDraw_Init (void)
{
	extern cvar_t scr_showloading;
	extern cvar_t vid_srgb;
	if ((vid.flags & VID_SRGB_CAPABLE) && gl_config.arb_framebuffer_srgb)
	{	//srgb-capable
		if (!vid_srgb.ival)
		{	//srgb framebuffer not wanted...
			qglDisable(GL_FRAMEBUFFER_SRGB);
			vid.flags &= ~VID_SRGB_FB_LINEAR;
		}
		else
		{
			vid.flags |= VID_SRGB_FB_LINEAR;
			qglEnable(GL_FRAMEBUFFER_SRGB);
		}
	}
	if ((vid.flags & VID_SRGB_FB) && vid_srgb.ival >= 0)
		vid.flags |= VID_SRGBAWARE;
	else
		vid.flags &= ~VID_SRGBAWARE;


	//figure out which extra features we can support on these drivers.
	r_deluxemapping = r_deluxemapping_cvar.ival;
	r_lightprepass = r_lightprepass_cvar.ival && sh_config.progs_supported;
	r_softwarebanding = r_softwarebanding_cvar.ival && sh_config.progs_supported;
	if (gl_config.gles && gl_config.glversion < 3.0)
		r_softwarebanding = false;

	GL_SetupFormats();

	R2D_Init();

	qglDisable(GL_SCISSOR_TEST);
	GL_Set2D(false);

	if (scr_showloading.ival)
	{
		mpic_t *pic = R2D_SafeCachePic ("gfx/loading.lmp");
		if (pic && R_GetShaderSizes(pic, NULL, NULL, true))
		{	//if its too big for the screen, letterbox it.
			qglClearColor(0, 0, 0, 1);
			qglClear(GL_COLOR_BUFFER_BIT);
			if (pic->width > vid.width || pic->height > vid.height)
				R2D_Letterbox(0, 0, vid.width, vid.height, pic, pic->width, pic->height);
			else	//otherwise draw it centred
				R2D_ScalePic ( ((int)vid.width - pic->width)/2, ((int)vid.height - 48 - pic->height)/2, pic->width, pic->height, pic);
		}

		if (R2D_Flush)
			R2D_Flush();
		VID_SwapBuffers();
	}

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
}

void GLDraw_DeInit (void)
{
	R2D_Shutdown();

	R_GAliasFlushSkinCache(true);

	draw_disc = NULL;
	GL_ShutdownPostProcessing();

#ifdef RTLIGHTS
	Sh_Shutdown();
#endif

	GLBE_Shutdown();	//to release its images.
	Shader_Shutdown();

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

		Matrix4x4_CM_Orthographic(r_refdef.m_projection_std, w/-2.0f, w/2.0f, h/2.0f, h/-2.0f, -99999, 99999);

		Matrix4x4_Identity(tmp);
		Matrix4_Multiply(Matrix4x4_CM_NewTranslation((vid.width/-2.0f), (vid.height/-2.0f), 0), tmp, tmp2);
		Matrix4_Multiply(Matrix4x4_CM_NewRotation(-ang,  0, 0, 1), tmp2, r_refdef.m_view);
	}
	else
	{
		w = vid.fbvwidth;
		h = vid.fbvheight;
		if (flipped)
			Matrix4x4_CM_Orthographic(r_refdef.m_projection_std, 0, w, 0, h, -99999, 99999);
		else
			Matrix4x4_CM_Orthographic(r_refdef.m_projection_std, 0, w, h, 0, -99999, 99999);
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
		qglLoadMatrixf(r_refdef.m_projection_std);

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
	int i, j, ifmt;
	int nummips = mips->mipcount;
	uploadfmt_t encoding = mips->encoding;
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
	case 3:
		targ = GL_TEXTURE_2D_ARRAY;
		break;
	}

	GL_MTBind(0, targ, tex);

	if (tex->num && qglTexStorage2D)
	{
		qglDeleteTextures(1, &tex->num);
		qglGenTextures(1, &tex->num);
		GL_MTBind(0, targ, tex);
		qglBindTexture (targ, tex->num);	//GL_MTBind caches, which is problematic when things are getting deleted.
	}
	else
	{
		if (!tex->num)
			qglGenTextures(1, &tex->num);
		GL_MTBind(0, targ, tex);
	}

	if (tex->flags&IF_CLAMP)
	{
		if (gl_config.glversion < 1.2 && !gl_config_gles)
		{	//warning: crappy path! gl1.1 is shite and doesn't support clamp-to-edge! there's ALWAYS some wrap component!
			qglTexParameteri(targ, GL_TEXTURE_WRAP_S, GL_CLAMP);
			qglTexParameteri(targ, GL_TEXTURE_WRAP_T, GL_CLAMP);
			if (targ != GL_TEXTURE_2D)
				qglTexParameteri(targ, GL_TEXTURE_WRAP_R, GL_CLAMP);
		}
		else
		{
			qglTexParameteri(targ, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			qglTexParameteri(targ, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			if (targ != GL_TEXTURE_2D)
				qglTexParameteri(targ, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		}
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

#ifdef FTE_TARGET_WEB
	if (encoding == PTI_WHOLEFILE)
	{
		emscriptenfte_gl_loadtexturefile(tex->num, &tex->width, &tex->height, mips->mip[i].data, mips->mip[i].datasize);
		return true;
	}
#endif

	//arb_texture_compression is core in gl1.3
	//gles doesn't support autocompression as of gles3.
	//only autocompress if we have actually have data (gl errors otherwise).
	if (gl_config.arb_texture_compression && mips->mip[0].data && !(tex->flags & IF_RENDERTARGET))
		compress = !!gl_compress.ival;
	else
		compress = false;
	if (compress & gl_config.formatinfo[encoding].cformat)
		ifmt = gl_config.formatinfo[encoding].cformat;
	else
		ifmt = gl_config.formatinfo[encoding].sizedformat;

	if (!ifmt)
		return false;

	if (gl_config.formatinfo[encoding].swizzle_r != GL_RED || gl_config.formatinfo[encoding].swizzle_g != GL_GREEN ||
		gl_config.formatinfo[encoding].swizzle_b != GL_BLUE || gl_config.formatinfo[encoding].swizzle_a != GL_ALPHA)
	{
		qglTexParameteri(targ, GL_TEXTURE_SWIZZLE_R, gl_config.formatinfo[encoding].swizzle_r);
		qglTexParameteri(targ, GL_TEXTURE_SWIZZLE_G, gl_config.formatinfo[encoding].swizzle_g);
		qglTexParameteri(targ, GL_TEXTURE_SWIZZLE_B, gl_config.formatinfo[encoding].swizzle_b);
		qglTexParameteri(targ, GL_TEXTURE_SWIZZLE_A, gl_config.formatinfo[encoding].swizzle_a);
	}

	if (targ == GL_TEXTURE_3D || targ == GL_TEXTURE_2D_ARRAY)
	{
		//FIXME: support array textures properly
		if (qglTexStorage3D)
		{
			if (tex->flags & IF_TEXTYPE)
				qglTexStorage3D(targ, nummips/countof(cubeface), ifmt, mips->mip[0].width, mips->mip[0].height, mips->mip[0].depth);
			else
				qglTexStorage3D(targ, nummips, ifmt, mips->mip[0].width, mips->mip[0].height, mips->mip[0].depth);

			for (i = 0; i < nummips; i++)
			{
				if (!mips->mip[i].data)	//already specified by gltexstorage
					continue;

				if (gl_config.formatinfo[encoding].type)
					qglTexSubImage3D			(targ, i, 0, 0, 0, mips->mip[i].width, mips->mip[i].height, mips->mip[0].depth, gl_config.formatinfo[encoding].format, gl_config.formatinfo[encoding].type,	mips->mip[i].data);
				else
					qglCompressedTexSubImage3D	(targ, i, 0, 0, 0, mips->mip[i].width, mips->mip[i].height, mips->mip[0].depth,									ifmt, mips->mip[i].datasize,				mips->mip[i].data);
			}
		}
		else
		{
			for (i = 0; i < nummips; i++)
			{
				if (gl_config.formatinfo[encoding].type)
					qglTexImage3D				(targ, i, ifmt, mips->mip[i].width, mips->mip[i].height, mips->mip[0].depth, 0, gl_config.formatinfo[encoding].format, gl_config.formatinfo[encoding].type,	mips->mip[i].data);
				else
					qglCompressedTexImage3D		(targ, i, ifmt, mips->mip[i].width, mips->mip[i].height, mips->mip[0].depth, 0,								mips->mip[i].datasize,							mips->mip[i].data);
			}
		}
	}
	else
	{
		if (qglTexStorage2D)
		{	//FIXME: destroy the old texture
			if (tex->flags & IF_TEXTYPE)
				qglTexStorage2D(targ, nummips/countof(cubeface), ifmt, mips->mip[0].width, mips->mip[0].height);
			else
				qglTexStorage2D(targ, nummips, ifmt, mips->mip[0].width, mips->mip[0].height);
		
			for (i = 0; i < nummips; i++)
			{
				if (tex->flags & IF_TEXTYPE)
				{	//cubemap face
					targface = cubeface[i%countof(cubeface)];
					j = i/countof(cubeface);
				}
				else
				{	//2d
					targface = targ;
					j = i;
				}

				if (!mips->mip[i].data)	//already specified by gltexstorage, don't bother wiping it or anything.
					continue;

				if (gl_config.formatinfo[encoding].type)
					qglTexSubImage2D			(targface, j, 0, 0, mips->mip[i].width, mips->mip[i].height, gl_config.formatinfo[encoding].format, gl_config.formatinfo[encoding].type, mips->mip[i].data);
				else
					qglCompressedTexSubImage2D	(targface, j, 0, 0, mips->mip[i].width, mips->mip[i].height,								ifmt, mips->mip[i].datasize,				 mips->mip[i].data);
			}
		}
		else
		{
			for (i = 0; i < nummips; i++)
			{
				if (tex->flags & IF_TEXTYPE)
				{	//cubemap face
					targface = cubeface[i%countof(cubeface)];
					j = i/countof(cubeface);
				}
				else
				{	//2d
					targface = targ;
					j = i;
				}

				if (gl_config.formatinfo[encoding].type)
					qglTexImage2D				(targface, j, ifmt, mips->mip[i].width, mips->mip[i].height, 0, gl_config.formatinfo[encoding].format, gl_config.formatinfo[encoding].type,	mips->mip[i].data);
				else
					qglCompressedTexImage2D		(targface, j, ifmt, mips->mip[i].width, mips->mip[i].height, 0,								mips->mip[i].datasize,							mips->mip[i].data);
			}
		}

#ifdef IMAGEFMT_KTX
		if (compress && gl_compress.ival>1 && gl_config.formatinfo[encoding].type)
		{
			GLint fmt;
			GLint csize;
			struct pendingtextureinfo out = {mips->type};
			out.type = mips->type;
			out.mipcount = mips->mipcount;
			out.encoding = 0;
			out.extrafree = NULL;

			qglGetTexLevelParameteriv(targ, 0, GL_TEXTURE_INTERNAL_FORMAT, &fmt);

			switch(fmt)
			{
			case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:				out.encoding = PTI_BC1_RGB;				break;
			case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:				out.encoding = PTI_BC1_RGBA;			break;
			case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:				out.encoding = PTI_BC2_RGBA;			break;
			case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:				out.encoding = PTI_BC3_RGBA;			break;
			case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:				out.encoding = PTI_BC1_RGB_SRGB;		break;
			case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:		out.encoding = PTI_BC1_RGBA_SRGB;		break;
			case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:		out.encoding = PTI_BC2_RGBA_SRGB;		break;
			case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:		out.encoding = PTI_BC3_RGBA_SRGB;		break;
			case GL_COMPRESSED_RED_RGTC1:						out.encoding = PTI_BC4_R8;				break;
			case GL_COMPRESSED_SIGNED_RED_RGTC1:				out.encoding = PTI_BC4_R8_SNORM;		break;
			case GL_COMPRESSED_RG_RGTC2:						out.encoding = PTI_BC5_RG8;				break;
			case GL_COMPRESSED_SIGNED_RG_RGTC2:					out.encoding = PTI_BC5_RG8_SNORM;		break;
			case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB:		out.encoding = PTI_BC6_RGB_UFLOAT;		break;
			case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB:		out.encoding = PTI_BC6_RGB_SFLOAT;		break;
			case GL_COMPRESSED_RGBA_BPTC_UNORM_ARB:				out.encoding = PTI_BC7_RGBA;			break;
			case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB:		out.encoding = PTI_BC7_RGBA_SRGB;		break;
			case GL_ETC1_RGB8_OES:								out.encoding = PTI_ETC1_RGB8;			break;
			case GL_COMPRESSED_RGB8_ETC2:						out.encoding = PTI_ETC2_RGB8;			break;
			case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:	out.encoding = PTI_ETC2_RGB8A1;			break;
			case GL_COMPRESSED_RGBA8_ETC2_EAC:					out.encoding = PTI_ETC2_RGB8A8;			break;
			case GL_COMPRESSED_SRGB8_ETC2:						out.encoding = PTI_ETC2_RGB8_SRGB;		break;
			case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:	out.encoding = PTI_ETC2_RGB8A1_SRGB;	break;
			case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:			out.encoding = PTI_ETC2_RGB8A8_SRGB;	break;
			case GL_COMPRESSED_R11_EAC:							out.encoding = PTI_EAC_R11;				break;
			case GL_COMPRESSED_SIGNED_R11_EAC:					out.encoding = PTI_EAC_R11_SNORM;		break;
			case GL_COMPRESSED_RG11_EAC:						out.encoding = PTI_EAC_RG11;			break;
			case GL_COMPRESSED_SIGNED_RG11_EAC:					out.encoding = PTI_EAC_RG11_SNORM;		break;
			case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:				out.encoding = PTI_ASTC_4X4;			break;
			case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:		out.encoding = PTI_ASTC_4X4_SRGB;		break;
			case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:				out.encoding = PTI_ASTC_5X4;			break;
			case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:		out.encoding = PTI_ASTC_5X4_SRGB;		break;
			case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:				out.encoding = PTI_ASTC_5X5;			break;
			case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:		out.encoding = PTI_ASTC_5X5_SRGB;		break;
			case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:				out.encoding = PTI_ASTC_6X5;			break;
			case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:		out.encoding = PTI_ASTC_6X5_SRGB;		break;
			case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:				out.encoding = PTI_ASTC_6X6;			break;
			case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:		out.encoding = PTI_ASTC_6X6_SRGB;		break;
			case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:				out.encoding = PTI_ASTC_8X5;			break;
			case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:		out.encoding = PTI_ASTC_8X5_SRGB;		break;
			case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:				out.encoding = PTI_ASTC_8X6;			break;
			case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:		out.encoding = PTI_ASTC_8X6_SRGB;		break;
			case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:				out.encoding = PTI_ASTC_10X5;			break;
			case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:		out.encoding = PTI_ASTC_10X5_SRGB;		break;
			case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:				out.encoding = PTI_ASTC_10X6;			break;
			case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:		out.encoding = PTI_ASTC_10X6_SRGB;		break;
			case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:				out.encoding = PTI_ASTC_8X8;			break;
			case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:		out.encoding = PTI_ASTC_8X8_SRGB;		break;
			case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:				out.encoding = PTI_ASTC_10X8;			break;
			case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:		out.encoding = PTI_ASTC_10X8_SRGB;		break;
			case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:				out.encoding = PTI_ASTC_10X10;			break;
			case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:		out.encoding = PTI_ASTC_10X10_SRGB;		break;
			case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:				out.encoding = PTI_ASTC_12X10;			break;
			case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:		out.encoding = PTI_ASTC_12X10_SRGB;		break;
			case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:				out.encoding = PTI_ASTC_12X12;			break;
			case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:		out.encoding = PTI_ASTC_12X12_SRGB;		break;
			}

			if (out.encoding)
			{
				for (i = 0; i < nummips; i++)
				{
					if (tex->flags & IF_TEXTYPE)
					{	//cubemap face
						targface = cubeface[i%countof(cubeface)];
						j = i/countof(cubeface);
					}
					else
					{	//2d
						targface = targ;
						j = i;
					}

					qglGetTexLevelParameteriv(targface, j, GL_TEXTURE_COMPRESSED_IMAGE_SIZE_ARB, &csize);
					if (!csize)
						break;	//some kind of error. the gpu didn't store it?
					out.mip[i].datasize = csize;
					out.mip[i].data = BZ_Malloc(csize);
					out.mip[i].needfree = true;
					out.mip[i].width = mips->mip[i].width;
					out.mip[i].height = mips->mip[i].height;
					out.mip[i].depth = mips->mip[i].depth;
					qglGetCompressedTexImage(targ, j, out.mip[i].data);
				}

				if (i)
				{
					out.mipcount = i;
					Image_WriteKTXFile(va("textures/%s.ktx", tex->ident), FS_GAMEONLY, &out);
				}
				while (i-- > 0)
					if (out.mip[i].needfree)
						BZ_Free(out.mip[i].data);
			}
		}
#endif
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
