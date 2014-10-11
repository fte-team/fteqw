/*
Copyright (C) 1997-2001 Id Software, Inc.

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
// gl_bloom.c: 2D lighting post process effect


/*
info about bloom algo:
bloom is basically smudging.
screen is nearest-downsampled to some usable scale and filtered to remove low-value light (this is what stops non-bright stuff from blooming)
this filtered image is then downsized multiple times
the downsized image is then blured 
the downsized images are then blured horizontally, and then vertically.
final pass simply adds each blured level to the original image.
all samples are then added together for final rendering (with some kind of tone mapping if you want proper hdr).

note: the horizontal/vertical bluring is a guassian filter
note: bloom comes from the fact that the most downsampled image doesn't have too many pixels. the pixels that it does have are spread over a large area.

http://prideout.net/archive/bloom/ contains some sample code
*/


//http://www.quakesrc.org/forums/viewtopic.php?t=4340&start=0

#include "quakedef.h"

#ifdef GLQUAKE
#include "shader.h"
#include "glquake.h"
cvar_t		r_bloom = CVARAFD("r_bloom", "0", "gl_bloom", CVAR_ARCHIVE, "Enables bloom (light bleeding from bright objects). Fractional values reduce the amount shown.");
cvar_t		r_bloom_filter = CVARD("r_bloom_filter", "0.5 0.5 0.5", "Controls how bright the image must get before it will bloom (3 separate values, in RGB order).");
cvar_t		r_bloom_scale = CVARD("r_bloom_scale", "0.5", "Controls the initial downscale size. Smaller values will bloom further but be more random.");
static shader_t *bloomfilter;
static shader_t *bloomrescale;
static shader_t *bloomblur;
static shader_t *bloomfinal;

#define MAXLEVELS 3
texid_t pingtex[2][MAXLEVELS];
fbostate_t fbo_bloom;
static int scrwidth, scrheight;
static int texwidth[MAXLEVELS], texheight[MAXLEVELS];


static void R_InitBloomTextures(void)
{
	int i;

	bloomfilter = NULL;
	bloomblur = NULL;
	bloomfinal = NULL;
	scrwidth = 0, scrheight = 0;

	for (i = 0; i < MAXLEVELS; i++)
	{
		pingtex[0][i] = r_nulltex;
		pingtex[1][i] = r_nulltex;
	}
}
void R_BloomRegister(void)
{
	Cvar_Register (&r_bloom, "bloom");
	Cvar_Register (&r_bloom_filter, "bloom");
	Cvar_Register (&r_bloom_scale, "bloom");
}
static void R_SetupBloomTextures(int w, int h)
{
	int i, j;
	char name[64];
	if (w == scrwidth && h == scrheight && !r_bloom_scale.modified)
		return;
	r_bloom_scale.modified = false;
	scrwidth = w;
	scrheight = h;
	//I'm depending on npot here
	w *= r_bloom_scale.value;
	h *= r_bloom_scale.value;
	for (i = 0; i < MAXLEVELS; i++)
	{
		w /= 2;
		h /= 2;
		/*I'm paranoid*/
		if (w < 4)
			w = 4;
		if (h < 4)
			h = 4;

		texwidth[i] = w;
		texheight[i] = h;
	}

	/*now create textures for each level*/
	for (j = 0; j < MAXLEVELS; j++)
	{
		for (i = 0; i < 2; i++)
		{
			if (!TEXVALID(pingtex[i][j]))
			{
				sprintf(name, "***bloom*%c*%i***", 'a'+i, j);
				TEXASSIGN(pingtex[i][j], Image_CreateTexture(name, NULL, IF_CLAMP|IF_NOMIPMAP|IF_NOPICMIP|IF_LINEAR));
				qglGenTextures(1, &pingtex[i][j]->num);
			}
			GL_MTBind(0, GL_TEXTURE_2D, pingtex[i][j]);
			qglTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, texwidth[j], texheight[j], 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
	}


	bloomfilter = R_RegisterShader("bloom_filter", SUF_NONE,
		"{\n"
			"cull none\n"
			"program bloom_filter\n"
			"{\n"
				"map $sourcecolour\n"
			"}\n"
		"}\n");
	bloomrescale = R_RegisterShader("bloom_rescale", SUF_NONE,
		"{\n"
			"cull none\n"
			"program default2d\n"
			"{\n"
				"map $sourcecolour\n"
			"}\n"
		"}\n");
	bloomblur = R_RegisterShader("bloom_blur", SUF_NONE,
		"{\n"
			"cull none\n"
			"program bloom_blur\n"
			"{\n"
				"map $sourcecolour\n"
			"}\n"
		"}\n");
	bloomfinal = R_RegisterShader("bloom_final", SUF_NONE,
		"{\n"
			"cull none\n"
			"program bloom_final\n"
			"{\n"
				"map $sourcecolour\n"
			"}\n"
			"{\n"
				"map $diffuse\n"
			"}\n"
			"{\n"
				"map $loweroverlay\n"
			"}\n"
			"{\n"
				"map $upperoverlay\n"
			"}\n"
		"}\n");
	bloomfinal->defaulttextures.base			= pingtex[0][0];
	bloomfinal->defaulttextures.loweroverlay	= pingtex[0][1];
	bloomfinal->defaulttextures.upperoverlay	= pingtex[0][2];
}
qboolean R_CanBloom(void)
{
	if (!r_bloom.value)
		return false;
	if (!gl_config.ext_framebuffer_objects)
		return false;
	if (!gl_config.arb_shader_objects)
		return false;
	if (!sh_config.texture_non_power_of_two_pic)
		return false;

	return true;
}

void R_BloomBlend (texid_t source, int x, int y, int w, int h)
{
	int i;
	int oldfbo = 0;

	/*whu?*/
	if (!w || !h)
		return;

	/*update textures if we need to resize them*/
	R_SetupBloomTextures(w, h);

	for (i = 0; i < MAXLEVELS; i++)
	{
		if (i == 0)
		{
			/*filter the screen into a downscaled image*/
			oldfbo = GLBE_FBO_Update(&fbo_bloom, 0, &pingtex[0][0], 1, r_nulltex, 0, 0);
			GLBE_FBO_Sources(source, r_nulltex);
			qglViewport (0, 0, texwidth[0], texheight[0]);
			R2D_ScalePic(0, vid.height, vid.width, -(int)vid.height, bloomfilter);
		}
		else
		{
			/*simple downscale that multiple times*/
			GLBE_FBO_Update(&fbo_bloom, 0, &pingtex[0][i], 1, r_nulltex, 0, 0);
			GLBE_FBO_Sources(pingtex[0][i-1], r_nulltex);
			qglViewport (0, 0, texwidth[i], texheight[i]);
			R2D_ScalePic(0, vid.height, vid.width, -(int)vid.height, bloomrescale);
		}
//	}
//	for (i = 0; i < MAXLEVELS; i++)
//	{
		/*gaussian filter the mips to bloom more smoothly
		the blur is done with two passes. first samples horizontally then vertically.
		the 1.2 pixels thing gives us a 5*5 filter by weighting the edge accordingly
		*/
		r_worldentity.glowmod[0] = 1.2 / texwidth[i];
		r_worldentity.glowmod[1] = 0;
		GLBE_FBO_Update(&fbo_bloom, 0, &pingtex[1][i], 1, r_nulltex, 0, 0);
		GLBE_FBO_Sources(pingtex[0][i], r_nulltex);
		qglViewport (0, 0, texwidth[i], texheight[i]);
		R2D_ScalePic(0, vid.height, vid.width, -(int)vid.height, bloomblur);

		r_worldentity.glowmod[0] = 0;
		r_worldentity.glowmod[1] = 1.2 / texheight[i];
		GLBE_FBO_Update(&fbo_bloom, 0, &pingtex[0][i], 1, r_nulltex, 0, 0);
		GLBE_FBO_Sources(pingtex[1][i], r_nulltex);
		qglViewport (0, 0, texwidth[i], texheight[i]);
		R2D_ScalePic(0, vid.height, vid.width, -(int)vid.height, bloomblur);
	}
	r_worldentity.glowmod[0] = 0;
	r_worldentity.glowmod[1] = 0;

	GL_Set2D(false);

	/*combine them onto the screen*/
	GLBE_FBO_Pop(oldfbo);
	GLBE_FBO_Sources(source, r_nulltex);
	R2D_ScalePic(x, y + h, w, -h, bloomfinal);
}
void R_BloomShutdown(void)
{
	GLBE_FBO_Destroy(&fbo_bloom);

	R_InitBloomTextures();
}

#endif
