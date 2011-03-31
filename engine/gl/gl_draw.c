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

#include <stdlib.h> // is this needed for atoi?
#include <stdio.h> // is this needed for atoi?

//#define GL_USE8BITTEX

static void GL_Upload32 (char *name, unsigned *data, int width, int height, unsigned int flags);
static void GL_Upload32_BGRA (char *name, unsigned *data, int width, int height, unsigned int flags);
static void GL_Upload24BGR_Flip (char *name, qbyte *framedata, int inwidth, int inheight, unsigned int flags);
static void GL_Upload8 (char *name, qbyte *data, int width, int height, unsigned int flags, unsigned int alpha);
static void GL_Upload8Pal32 (qbyte *data, qbyte *pal, int width, int height, unsigned int flags);

void GL_UploadFmt(texid_t tex, char *name, enum uploadfmt fmt, void *data, void *palette, int width, int height, unsigned int flags)
{
	GL_MTBind(0, GL_TEXTURE_2D, tex);
	switch(fmt)
	{
	case TF_INVALID:
		break;

	case TF_RGBX32:
		flags |= IF_NOALPHA;
	case TF_RGBA32:
		GL_Upload32(name, data, width, height, flags);
		break;

	case TF_BGRA32:
		GL_Upload32_BGRA(name, data, width, height, flags);
		break;

//	case TF_BGRA24:
//		GL_Upload24BGR(name, data, width, height, flags);
//		break;

	case TF_BGR24_FLIP:
		GL_Upload24BGR_Flip(name, data, width, height, flags);
		break;

	case TF_SOLID8:
		GL_Upload8(name, data, width, height, flags, 0);
		break;

	case TF_TRANS8:
		GL_Upload8(name, data, width, height, flags, 1);
		break;

	case TF_8PAL24:
		GL_Upload8Pal24(data, palette, width, height, flags);
		break;
	case TF_8PAL32:
		GL_Upload8Pal32(data, palette, width, height, flags);
		break;

	default:
		Sys_Error("Unsupported image format type\n");
		break;
	}
}

texid_t GL_LoadTextureFmt (char *name, int width, int height, enum uploadfmt fmt, void *data, unsigned int flags)
{
	extern cvar_t r_shadow_bumpscale_basetexture;
	switch(fmt)
	{
	case TF_INVALID:
		return r_nulltex;

	case TF_RGBX32:
		flags |= IF_NOALPHA;
	case TF_RGBA32:
		return GL_LoadTexture32(name, width, height, data, flags);

	case TF_TRANS8:
		return GL_LoadTexture(name, width, height, data, flags, 1);

	case TF_TRANS8_FULLBRIGHT:
		return GL_LoadTextureFB(name, width, height, data, flags);

	case TF_SOLID8:
		return GL_LoadTexture(name, width, height, data, flags, 0);

	case TF_H2_T7G1:
		return GL_LoadTexture(name, width, height, data, flags, 2);
	case TF_H2_TRANS8_0:
		return GL_LoadTexture(name, width, height, data, flags, 3);
	case TF_H2_T4A4:
		return GL_LoadTexture(name, width, height, data, flags, 4);

	case TF_HEIGHT8PAL:
	case TF_HEIGHT8:
		return GL_LoadTexture8Bump(name, width, height, data, flags, r_shadow_bumpscale_basetexture.value);

	default:
		Sys_Error("Unsupported image format type\n");
		return r_nulltex;
	}
}

qbyte				*uploadmemorybuffer;
int					sizeofuploadmemorybuffer;
qbyte				*uploadmemorybufferintermediate;
int					sizeofuploadmemorybufferintermediate;

extern qbyte		gammatable[256];

#ifdef GL_USE8BITTEX
unsigned char *d_15to8table;
qboolean inited15to8;
#endif
extern cvar_t crosshair, crosshairimage, crosshairalpha, cl_crossx, cl_crossy, crosshaircolor, crosshairsize;

static texid_t filmtexture;

extern cvar_t		gl_max_size;
extern cvar_t		gl_picmip;
extern cvar_t		gl_lerpimages;
extern cvar_t		gl_picmip2d;
extern cvar_t		gl_compress;
extern cvar_t		gl_smoothcrosshair;
extern cvar_t		gl_texturemode, gl_texture_anisotropic_filtering;

extern cvar_t		gl_savecompressedtex;

texid_t			missing_texture;	//texture used when one is missing.

texid_t			cs_texture; // crosshair texture
shader_t		*crosshair_shader;

static unsigned cs_data[16*16];
static texid_t externalhair;
int gl_anisotropy_factor;

mpic_t		*conback;

#include "hash.h"
hashtable_t gltexturetable;
bucket_t *gltexturetablebuckets[256];

int		gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int		gl_filter_max = GL_LINEAR;
int		gl_filter_max_2d = GL_LINEAR;

typedef struct gltexture_s
{
	texid_t	texnum;
	char	identifier[64];
	int		width, height, bpp;
	unsigned int flags;
	struct gltexture_s *next;
} gltexture_t;

static gltexture_t	*gltextures;

//=============================================================================
/* Support Routines */

typedef struct
{
	char *name;
	char *altname;
	int	minimize, maximize;
} glmode_t;

glmode_t modes[] = {
	{"GL_NEAREST", "n", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", "l", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", "nn", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", "ln", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", "nl", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", "ll", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

void GL_Texture_Anisotropic_Filtering_Callback (struct cvar_s *var, char *oldvalue)
{
	gltexture_t *glt;
	int anfactor;

	if (qrenderer != QR_OPENGL)
		return;

	gl_anisotropy_factor = 0;
	
	if (gl_config.ext_texture_filter_anisotropic < 2)
		return;

	anfactor = bound(1, var->value, gl_config.ext_texture_filter_anisotropic);

	/* change all the existing max anisotropy settings */
	for (glt = gltextures; glt ; glt = glt->next) //redo anisotropic filtering when map is changed
	{
		if (!(glt->flags & IF_NOMIPMAP))
		{
			GL_MTBind(0, GL_TEXTURE_2D, glt->texnum);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anfactor);
		}
	}

	if (anfactor >= 2)
		gl_anisotropy_factor = anfactor;
	else
		gl_anisotropy_factor = 0;
}

/*
===============
Draw_TextureMode_f
===============
*/
void GL_Texturemode_Callback (struct cvar_s *var, char *oldvalue)
{
	int		i;
	gltexture_t	*glt;

	if (qrenderer != QR_OPENGL)
		return;

	for (i=0 ; i< sizeof(modes)/sizeof(modes[0]) ; i++)
	{
		if (!Q_strcasecmp (modes[i].name, var->string ) )
			break;
		if (!Q_strcasecmp (modes[i].altname, var->string ) )
			break;
	}
	if (i == sizeof(modes)/sizeof(modes[0]))
	{
		Con_Printf ("bad gl_texturemode name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for (glt=gltextures ; glt ; glt=glt->next)
	{
		if (!(glt->flags & IF_NOMIPMAP))
		{
			GL_MTBind(0, GL_TEXTURE_2D, glt->texnum);
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		}
	}
}
void GL_Texturemode2d_Callback (struct cvar_s *var, char *oldvalue)
{
	int		i;
	gltexture_t	*glt;

	if (qrenderer != QR_OPENGL)
		return;

	for (i=0 ; i< sizeof(modes)/sizeof(modes[0]) ; i++)
	{
		if (!Q_strcasecmp (modes[i].name, var->string ) )
			break;
		if (!Q_strcasecmp (modes[i].altname, var->string ) )
			break;
	}
	if (i == sizeof(modes)/sizeof(modes[0]))
	{
		Con_Printf ("bad gl_texturemode name\n");
		return;
	}

//	gl_filter_min = modes[i].minimize;
	gl_filter_max_2d = modes[i].maximize;

	// change all the existing mipmap texture objects
	for (glt=gltextures ; glt ; glt=glt->next)
	{
		if (glt->flags & IF_NOMIPMAP)
		{
			GL_MTBind(0, GL_TEXTURE_2D, glt->texnum);
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max_2d);
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max_2d);
		}
	}
}
/*
===============
Draw_Init
===============
*/

void GLDraw_ReInit (void)
{
	char	ver[40];

	int maxtexsize;

	if (gltextures)
	{
		Con_Printf("gl_draw didn't shut down cleanly\n");
		gltextures = NULL;
	}

	memset(gltexturetablebuckets, 0, sizeof(gltexturetablebuckets));
	Hash_InitTable(&gltexturetable, sizeof(gltexturetablebuckets)/sizeof(gltexturetablebuckets[0]), gltexturetablebuckets);

	lightmap_textures=NULL;
	filmtexture=r_nulltex;

	GL_FlushBackEnd();
//	GL_FlushSkinCache();
	TRACE(("dbg: GLDraw_ReInit: GL_GAliasFlushSkinCache\n"));
	GL_GAliasFlushSkinCache();

	qglGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxtexsize);
	if (gl_max_size.value > maxtexsize)
	{
		sprintf(ver, "%i", maxtexsize);
		Cvar_ForceSet (&gl_max_size, ver);
	}

	maxtexsize = gl_max_size.value;

	if (maxtexsize < 2048)	//this needs to be able to hold the image in unscaled form.
		sizeofuploadmemorybufferintermediate = 2048*2048*4;	//make sure we can load 2048*2048 images whatever happens.
	else
		sizeofuploadmemorybufferintermediate = maxtexsize*maxtexsize*4;	//gl supports huge images, so so shall we.

	//required to hold the image after scaling has occured
	sizeofuploadmemorybuffer = maxtexsize*maxtexsize*4;
TRACE(("dbg: GLDraw_ReInit: Allocating upload buffers\n"));
	uploadmemorybuffer = BZ_Realloc(uploadmemorybuffer, sizeofuploadmemorybuffer);
	uploadmemorybufferintermediate = BZ_Realloc(uploadmemorybufferintermediate, sizeofuploadmemorybufferintermediate);

	R2D_Init();

	TRACE(("dbg: GLDraw_ReInit: GL_BeginRendering\n"));
	GL_BeginRendering ();
	TRACE(("dbg: GLDraw_ReInit: SCR_DrawLoading\n"));

	GL_Set2D();

	qglClear(GL_COLOR_BUFFER_BIT);
	{
		mpic_t *pic = R2D_SafeCachePic ("gfx/loading.lmp");
		if (pic)
			R2D_ScalePic ( ((int)vid.width - pic->width)/2,
				((int)vid.height - 48 - pic->height)/2, pic->width, pic->height, pic);
	}

	TRACE(("dbg: GLDraw_ReInit: GL_EndRendering\n"));
	GL_EndRendering ();
	GL_DoSwap();

	cs_texture = GL_AllocNewTexture(16, 16);

	crosshair_shader = R_RegisterShader("crosshairshader",
		"{\n"
			"{\n"
				"map $diffuse\n"
				"blendfunc blend\n"
			"}\n"
		"}\n"
		);

	GL_SetupSceneProcessingTextures();

	//
	// get the other pics we need
	//
	TRACE(("dbg: GLDraw_ReInit: R2D_SafePicFromWad\n"));
	draw_disc = R2D_SafePicFromWad ("disc");

#ifdef GL_USE8BITTEX
	inited15to8 = false;
#endif

	qglClearColor (1,0,0,0);

	TRACE(("dbg: GLDraw_ReInit: PPL_LoadSpecularFragmentProgram\n"));
	GL_InitSceneProcessingShaders();

#ifdef PLUGINS
	Plug_DrawReloadImages();
#endif
}

void GLDraw_Init (void)
{
//	memset(scrap_allocated, 0, sizeof(scrap_allocated));
//	memset(scrap_texels, 255, sizeof(scrap_texels));

	GLDraw_ReInit();
}
void GLDraw_DeInit (void)
{
	Cmd_RemoveCommand ("gl_texture_anisotropic_filtering");

	if (font_conchar)
		Font_Free(font_conchar);
	font_conchar = NULL; 
	if (font_tiny)
		Font_Free(font_tiny);
	font_tiny = NULL; 

	draw_disc = NULL;

	if (uploadmemorybuffer)
		BZ_Free(uploadmemorybuffer);	//free the mem
	if (uploadmemorybufferintermediate)
		BZ_Free(uploadmemorybufferintermediate);
	uploadmemorybuffer = NULL;	//make sure we know it's free
	uploadmemorybufferintermediate = NULL;
	sizeofuploadmemorybuffer = 0;	//and give a nice safe sys_error if we try using it.
	sizeofuploadmemorybufferintermediate = 0;

#ifdef RTLIGHTS
	Sh_Shutdown();
#endif
	Shader_Shutdown();
	
	while(gltextures)
	{
		gltexture_t *glt;
		glt = gltextures;
		gltextures = gltextures->next;
		BZ_Free(glt);
	}

}

#include "crosshairs.dat"
vec3_t chcolor;

void GLCrosshairimage_Callback(struct cvar_s *var, char *oldvalue)
{
	if (*(var->string))
		externalhair = R_LoadHiResTexture (var->string, "crosshairs", IF_NOMIPMAP);
}

void GLCrosshair_Callback(struct cvar_s *var, char *oldvalue)
{
	unsigned int c, c2;

	if (!var->value)
		return;

	c = (unsigned int)(chcolor[0] * 255) | // red
		((unsigned int)(chcolor[1] * 255) << 8) | // green
		((unsigned int)(chcolor[2] * 255) << 16) | // blue
		0xff000000; // alpha
	c2 = c;

#define Pix(x,y,c) {	\
		if (y+8<0)c=0;	\
		if (y+8>=16)c=0;	\
		if (x+8<0)c=0;	\
		if (x+8>=16)c=0;	\
			\
		cs_data[(y+8)*16+(x+8)] = c;	\
	}
	memset(cs_data, 0, sizeof(cs_data));
	switch((int)var->value)
	{
	default:
#include "crosshairs.dat"
	}
#undef Pix

	R_Upload(cs_texture, NULL, TF_RGBA32, cs_data, NULL, 16, 16, IF_NOMIPMAP|IF_NOGAMMA);

	if (gl_smoothcrosshair.ival)
	{
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else
	{
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
}

void GLCrosshaircolor_Callback(struct cvar_s *var, char *oldvalue)
{
	SCR_StringToRGB(var->string, chcolor, 255);

	chcolor[0] = bound(0, chcolor[0], 1);
	chcolor[1] = bound(0, chcolor[1], 1);
	chcolor[2] = bound(0, chcolor[2], 1);

	GLCrosshair_Callback(&crosshair, "");
}

void GLDraw_Crosshair(void)
{
	int x, y;
	int sc;
	float sx, sy, sizex, sizey;

	float size, chc;
	shader_t *shader;

	qboolean usingimage = false;

	if (crosshair.ival == 1 && !*crosshairimage.string)
	{
		for (sc = 0; sc < cl.splitclients; sc++)
		{
			SCR_CrosshairPosition(sc, &x, &y);
			Font_BeginString(font_conchar, x, y, &x, &y);
			x -= Font_CharWidth('+' | 0xe000 | CON_WHITEMASK)/2;
			y -= Font_CharHeight()/2;
			Font_DrawChar(x, y, '+' | 0xe000 | CON_WHITEMASK);
			Font_EndString(font_conchar);
		}
		return;
	}

	shader = crosshair_shader;
	if (*crosshairimage.string)
	{
		usingimage = true;
		chc = 0;

		shader->defaulttextures.base = externalhair;
	}
	else if (crosshair.ival)
	{
		chc = 1/16.0;

		// force crosshair refresh with animated crosshairs
		if (crosshair.ival >= FIRSTANIMATEDCROSHAIR)
			GLCrosshair_Callback(&crosshair, "");

		shader->defaulttextures.base = cs_texture;
	}
	else
		return;

	size = crosshairsize.value;

	if (size < 0)
	{
		size = -size;
		sizex = size;
		sizey = size;
		chc = 0;
	}
	else
	{
		sizex = (size*vid.rotpixelwidth) / (float)vid.width;
		sizey = (size*vid.rotpixelheight) / (float)vid.height;
		chc = size * chc;
	}

	sizex = (int)sizex;
	sizex = ((sizex)*(int)vid.width) / (float)vid.rotpixelwidth;

	sizey = (int)sizey;
	sizey = ((sizey)*(int)vid.height) / (float)vid.rotpixelheight;

	for (sc = 0; sc < cl.splitclients; sc++)
	{
		SCR_CrosshairPosition(sc, &x, &y);

		//translate to pixel coord, for rounding
		x = ((x-sizex-chc)*vid.rotpixelwidth) / (float)vid.width;
		y = ((y-sizey-chc)*vid.rotpixelheight) / (float)vid.height;

		//translate to screen coords
		sx = ((x)*(int)vid.width) / (float)vid.rotpixelwidth;
		sy = ((y)*(int)vid.height) / (float)vid.rotpixelheight;

		R2D_Image(sx, sy, sizex*2, sizey*2, 0, 0, 1, 1, shader);
	}
}



//=============================================================================

/*
================
GL_Set2D

Setup as if the screen was 320*200
================
*/
void GL_Set2D (void)
{
	extern cvar_t gl_screenangle;
	float rad, ang;
	float tmp[16], tmp2[16];
	float *Matrix4_NewRotation(float a, float x, float y, float z);
	float *Matrix4_NewTranslation(float x, float y, float z);
	float w = vid.width, h = vid.height;

	GL_SetShaderState2D(true);

	ang = (gl_screenangle.value>0?(gl_screenangle.value+45):(gl_screenangle.value-45))/90;
	ang = (int)ang * 90;
	if (ang)
	{ /*more expensive maths*/
		rad = (ang * M_PI) / 180;

		w = fabs(cos(rad)) * (vid.width) + fabs(sin(rad)) * (vid.height);
		h = fabs(sin(rad)) * (vid.width) + fabs(cos(rad)) * (vid.height);

		Matrix4_Orthographic(r_refdef.m_projection, w/-2.0f, w/2.0f, h/2.0f, h/-2.0f, -99999, 99999);

		Matrix4_Identity(tmp);
		Matrix4_Multiply(Matrix4_NewTranslation((vid.width/-2.0f), (vid.height/-2.0f), 0), tmp, tmp2);
		Matrix4_Multiply(Matrix4_NewRotation(-ang,  0, 0, 1), tmp2, r_refdef.m_view);
	}
	else
	{
		Matrix4_Orthographic(r_refdef.m_projection, 0, vid.width, vid.height, 0, -99999, 99999);
		Matrix4_Identity(r_refdef.m_view);
	}
	r_refdef.time = realtime;
	/*flush that gl state*/
	qglViewport (0, 0, vid.pixelwidth, vid.pixelheight);

	if (qglLoadMatrixf)
	{
		qglMatrixMode(GL_PROJECTION);
		qglLoadMatrixf(r_refdef.m_projection);

		qglMatrixMode(GL_MODELVIEW);
		qglLoadMatrixf(r_refdef.m_view);
	}
}

//====================================================================

/*
================
GL_FindTexture
================
*/
texid_t GL_FindTexture (char *identifier)
{
	gltexture_t	*glt;

	glt = Hash_Get(&gltexturetable, identifier);
	if (glt)
	{
		image_width = glt->width;
		image_height = glt->height;
		return glt->texnum;
	}

	return r_nulltex;
}

gltexture_t	*GL_MatchTexture (char *identifier, int bits, int width, int height)
{
	gltexture_t	*glt;

	glt = Hash_Get(&gltexturetable, identifier);
	while(glt)
	{
		if (glt->bpp == bits && width == glt->width && height == glt->height)
			return glt;

		glt = Hash_GetNext(&gltexturetable, identifier, glt);
	}

/*
	for (glt=gltextures ; glt ; glt=glt->next)
	{
		if (glt->bpp == bits && width == glt->width && height == glt->height)
		{
			if (!strcmp (identifier, glt->identifier))
			{
				return glt;
			}
		}
	}
*/

	return NULL;
}



static void Image_Resample32LerpLine (const qbyte *in, qbyte *out, int inwidth, int outwidth)
{
	int		j, xi, oldx = 0, f, fstep, endx, lerp;
	fstep = (int) (inwidth*65536.0f/outwidth);
	endx = (inwidth-1);
	for (j = 0,f = 0;j < outwidth;j++, f += fstep)
	{
		xi = f >> 16;
		if (xi != oldx)
		{
			in += (xi - oldx) * 4;
			oldx = xi;
		}
		if (xi < endx)
		{
			lerp = f & 0xFFFF;
			*out++ = (qbyte) ((((in[4] - in[0]) * lerp) >> 16) + in[0]);
			*out++ = (qbyte) ((((in[5] - in[1]) * lerp) >> 16) + in[1]);
			*out++ = (qbyte) ((((in[6] - in[2]) * lerp) >> 16) + in[2]);
			*out++ = (qbyte) ((((in[7] - in[3]) * lerp) >> 16) + in[3]);
		}
		else // last pixel of the line has no pixel to lerp to
		{
			*out++ = in[0];
			*out++ = in[1];
			*out++ = in[2];
			*out++ = in[3];
		}
	}
}

//yes, this is lordhavok's code.
//superblur away!
#define LERPBYTE(i) r = row1[i];out[i] = (qbyte) ((((row2[i] - r) * lerp) >> 16) + r)
static void Image_Resample32Lerp(const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight)
{
	int i, j, r, yi, oldy, f, fstep, lerp, endy = (inheight-1), inwidth4 = inwidth*4, outwidth4 = outwidth*4;
	qbyte *out;
	const qbyte *inrow;
	qbyte *tmem, *row1, *row2;

	tmem = row1 = BZ_Malloc(2*(outwidth*4));
	row2 = row1 + (outwidth * 4);

	out = outdata;
	fstep = (int) (inheight*65536.0f/outheight);

	inrow = indata;
	oldy = 0;
	Image_Resample32LerpLine (inrow, row1, inwidth, outwidth);
	Image_Resample32LerpLine (inrow + inwidth4, row2, inwidth, outwidth);
	for (i = 0, f = 0;i < outheight;i++,f += fstep)
	{
		yi = f >> 16;
		if (yi < endy)
		{
			lerp = f & 0xFFFF;
			if (yi != oldy)
			{
				inrow = (qbyte *)indata + inwidth4*yi;
				if (yi == oldy+1)
					memcpy(row1, row2, outwidth4);
				else
					Image_Resample32LerpLine (inrow, row1, inwidth, outwidth);
				Image_Resample32LerpLine (inrow + inwidth4, row2, inwidth, outwidth);
				oldy = yi;
			}
			j = outwidth - 4;
			while(j >= 0)
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				LERPBYTE( 4);
				LERPBYTE( 5);
				LERPBYTE( 6);
				LERPBYTE( 7);
				LERPBYTE( 8);
				LERPBYTE( 9);
				LERPBYTE(10);
				LERPBYTE(11);
				LERPBYTE(12);
				LERPBYTE(13);
				LERPBYTE(14);
				LERPBYTE(15);
				out += 16;
				row1 += 16;
				row2 += 16;
				j -= 4;
			}
			if (j & 2)
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				LERPBYTE( 4);
				LERPBYTE( 5);
				LERPBYTE( 6);
				LERPBYTE( 7);
				out += 8;
				row1 += 8;
				row2 += 8;
			}
			if (j & 1)
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				out += 4;
				row1 += 4;
				row2 += 4;
			}
			row1 -= outwidth4;
			row2 -= outwidth4;
		}
		else
		{
			if (yi != oldy)
			{
				inrow = (qbyte *)indata + inwidth4*yi;
				if (yi == oldy+1)
					memcpy(row1, row2, outwidth4);
				else
					Image_Resample32LerpLine (inrow, row1, inwidth, outwidth);
				oldy = yi;
			}
			memcpy(out, row1, outwidth4);
		}
	}
	BZ_Free(tmem);
}


/*
================
GL_ResampleTexture
================
*/
static void GL_ResampleTexture (unsigned *in, int inwidth, int inheight, unsigned *out,  int outwidth, int outheight)
{
	int		i, j;
	unsigned	*inrow;
	unsigned	frac, fracstep;

	if (gl_lerpimages.ival)
	{
		Image_Resample32Lerp(in, inwidth, inheight, out, outwidth, outheight);
		return;
	}

	fracstep = inwidth*0x10000/outwidth;
	for (i=0 ; i<outheight ; i++, out += outwidth)
	{
		inrow = in + inwidth*(i*inheight/outheight);
		frac = outwidth*fracstep;
		j=outwidth-1;
		while ((j+1)&3)
		{
			out[j] = inrow[frac>>16];
			frac -= fracstep;
			j--;
		}
		for ( ; j>=0 ; j-=4)
		{
			out[j+3] = inrow[frac>>16];
			frac -= fracstep;
			out[j+2] = inrow[frac>>16];
			frac -= fracstep;
			out[j+1] = inrow[frac>>16];
			frac -= fracstep;
			out[j+0] = inrow[frac>>16];
			frac -= fracstep;
		}
	}
}

/*
================
GL_Resample8BitTexture -- JACK
================
*/
static void GL_Resample8BitTexture (unsigned char *in, int inwidth, int inheight, unsigned char *out,  int outwidth, int outheight)
{
	int		i, j;
	unsigned	char *inrow;
	unsigned	frac, fracstep;

	fracstep = inwidth*0x10000/outwidth;
	for (i=0 ; i<outheight ; i++, out += outwidth)
	{
		inrow = in + inwidth*(i*inheight/outheight);
		frac = fracstep >> 1;
		for (j=0 ; j<outwidth ; j+=4)
		{
			out[j] = inrow[frac>>16];
			frac += fracstep;
			out[j+1] = inrow[frac>>16];
			frac += fracstep;
			out[j+2] = inrow[frac>>16];
			frac += fracstep;
			out[j+3] = inrow[frac>>16];
			frac += fracstep;
		}
	}
}

/*
================
GL_MipMap

Operates in place, quartering the size of the texture
================
*/
static void GL_MipMap (qbyte *in, int width, int height)
{
	int		i, j;
	qbyte	*out;
	qbyte	*inrow;

	//with npot
	int rowwidth = width*4;	//rowwidth is the byte width of the input
	inrow = in;

	width >>= 1;	//ensure its truncated, so don't merge with the *8
	height >>= 1;
	out = in;


	for (i=0 ; i<height ; i++, inrow+=rowwidth*2)
	{
		for (in = inrow, j=0 ; j<width ; j++, out+=4, in+=8)
		{
			out[0] = (in[0] + in[4] + in[rowwidth+0] + in[rowwidth+4])>>2;
			out[1] = (in[1] + in[5] + in[rowwidth+1] + in[rowwidth+5])>>2;
			out[2] = (in[2] + in[6] + in[rowwidth+2] + in[rowwidth+6])>>2;
			out[3] = (in[3] + in[7] + in[rowwidth+3] + in[rowwidth+7])>>2;
		}
	}
}

#ifdef GL_USE8BITTEX
#ifdef GL_EXT_paletted_texture
void GLDraw_Init15to8(void)
{
	int i, r, g, b, v, k;
	int r1, g1, b1;
	qbyte *pal;
	float dist, bestdist;
	vfsfile_t *f;

	qboolean savetable;

	// JACK: 3D distance calcs - k is last closest, l is the distance.
	if (inited15to8)
		return;
	if (!d_15to8table)
		d_15to8table = BZ_Malloc(sizeof(qbyte) * 32768);
	inited15to8 = true;

	savetable = COM_CheckParm("-save15to8");

	if (savetable)
		f = FS_OpenVFS("glquake/15to8.pal");
	else
		f = NULL;
	if (f)
	{
		VFS_READ(f, d_15to8table, 1<<15);
		VFS_CLOSE(f);
	}
	else
	{
		for (i=0; i < (1<<15); i++)
		{
			/* Maps
 			000000000000000
 			000000000011111 = Red  = 0x1F
 			000001111100000 = Blue = 0x03E0
 			111110000000000 = Grn  = 0x7C00
 			*/
 			r = ((i & 0x1F) << 3)+4;
 			g = ((i & 0x03E0) >> 2)+4;
 			b = ((i & 0x7C00) >> 7)+4;
			pal = (unsigned char *)d_8to24rgbtable;
			for (v=0,k=0,bestdist=10000.0; v<256; v++,pal+=4) {
 				r1 = (int)r - (int)pal[0];
 				g1 = (int)g - (int)pal[1];
 				b1 = (int)b - (int)pal[2];
				dist = sqrt(((r1*r1)+(g1*g1)+(b1*b1)));
				if (dist < bestdist) {
					k=v;
					bestdist = dist;
				}
			}
			d_15to8table[i]=k;
		}
		if (savetable)
		{
			FS_WriteFile("glquake/15to8.pal", d_15to8table, 1<<15, FS_GAME);
		}
	}
}

/*
================
GL_MipMap8Bit

Mipping for 8 bit textures
================
*/
static void GL_MipMap8Bit (qbyte *in, int width, int height)
{
	int		i, j;
	qbyte	*out;
	unsigned short     r,g,b;
	qbyte	*at1, *at2, *at3, *at4;

	height >>= 1;
	out = in;
	for (i=0 ; i<height ; i++, in+=width)
		for (j=0 ; j<width ; j+=2, out+=1, in+=2)
		{
			at1 = (qbyte *) &d_8to24rgbtable[in[0]];
			at2 = (qbyte *) &d_8to24rgbtable[in[1]];
			at3 = (qbyte *) &d_8to24rgbtable[in[width+0]];
			at4 = (qbyte *) &d_8to24rgbtable[in[width+1]];

 			r = (at1[0]+at2[0]+at3[0]+at4[0]); r>>=5;
 			g = (at1[1]+at2[1]+at3[1]+at4[1]); g>>=5;
 			b = (at1[2]+at2[2]+at3[2]+at4[2]); b>>=5;

			out[0] = d_15to8table[(r<<0) + (g<<5) + (b<<10)];
		}
}
#endif
#endif

qboolean GL_UploadCompressed (qbyte *file, int *out_width, int *out_height, unsigned int *out_flags)
{
	int miplevel;
	int width;
	int height;
	int compressed_size;
	int internalformat;
	int nummips;
#define GETVAR(var) memcpy(var, file, sizeof(*var));file+=sizeof(*var);

	if (!gl_config.arb_texture_compression || !gl_compress.value)
		return false;

	GETVAR(&nummips)
	GETVAR(out_width)
	GETVAR(out_height)
	GETVAR(out_flags)
	for (miplevel = 0; miplevel < nummips; miplevel++)
	{
		GETVAR(&width);
		GETVAR(&height);
		GETVAR(&compressed_size);
		GETVAR(&internalformat);
		width = LittleLong(width);
		height = LittleLong(height);
		compressed_size = LittleLong(compressed_size);
		internalformat = LittleLong(internalformat);

		qglCompressedTexImage2DARB(GL_TEXTURE_2D, miplevel, internalformat, width, height, 0, compressed_size, file);
		file += compressed_size;
	}

	if (!((*out_flags) & IF_NOMIPMAP))
	{
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max_2d);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max_2d);
	}
	return true;
}


void GL_RoundDimensions(int *scaled_width, int *scaled_height, qboolean mipmap)
{
	if (gl_config.arb_texture_non_power_of_two)	//NPOT is a simple extension that relaxes errors.
	{
		TRACE(("dbg: GL_RoundDimensions: GL_ARB_texture_non_power_of_two\n"));
	}
	else
	{
		int width = *scaled_width;
		int height = *scaled_height;
		for (*scaled_width = 1 ; *scaled_width < width ; *scaled_width<<=1)
			;
		for (*scaled_height = 1 ; *scaled_height < height ; *scaled_height<<=1)
			;
	}

	if (mipmap)
	{
		TRACE(("dbg: GL_RoundDimensions: %f\n", gl_picmip.value));
		*scaled_width >>= (int)gl_picmip.value;
		*scaled_height >>= (int)gl_picmip.value;
	}
	else
	{
		*scaled_width >>= (int)gl_picmip2d.value;
		*scaled_height >>= (int)gl_picmip2d.value;
	}

	TRACE(("dbg: GL_RoundDimensions: %f\n", gl_max_size.value));
	if (gl_max_size.value)
	{
		if (*scaled_width > gl_max_size.value)
			*scaled_width = gl_max_size.value;
		if (*scaled_height > gl_max_size.value)
			*scaled_height = gl_max_size.value;
	}

	if (*scaled_width < 1)
		*scaled_width = 1;
	if (*scaled_height < 1)
		*scaled_height = 1;
}
/*
===============
GL_Upload32
===============
*/
void GL_Upload32_Int (char *name, unsigned *data, int width, int height, unsigned int flags, GLenum glcolormode)
{
	int		miplevel=0;
	int			samples;
	unsigned	*scaled = (unsigned *)uploadmemorybuffer;
	int			scaled_width, scaled_height;

	TRACE(("dbg: GL_Upload32: %s %i %i\n", name, width, height));

	scaled_width = width;
	scaled_height = height;
	GL_RoundDimensions(&scaled_width, &scaled_height, !(flags & IF_NOMIPMAP));

	if (!(flags & IF_NOALPHA))
	{	//make sure it does actually have those alpha pixels
		int i;
		flags |= IF_NOALPHA;
		for (i = 3; i < width*height*4; i+=4)
		{
			if (((unsigned char*)data)[i] < 255)
			{
				flags &= ~IF_NOALPHA;
				break;
			}
		}
	}

	TRACE(("dbg: GL_Upload32: %i %i\n", scaled_width, scaled_height));

	if (scaled_width * scaled_height > sizeofuploadmemorybuffer/4)
		Sys_Error ("GL_LoadTexture: too big");

	if (gl_config.gles)
		samples = GL_RGBA; /* GL ES doesn't allow for format conversion */
	else
		samples = (flags&IF_NOALPHA) ? GL_RGB : GL_RGBA;

	if (gl_config.arb_texture_compression && gl_compress.value && name && !(flags&IF_NOMIPMAP))
		samples = (flags&IF_NOALPHA) ? GL_COMPRESSED_RGB_ARB : GL_COMPRESSED_RGBA_ARB;

	if (gl_config.sgis_generate_mipmap && !(flags&IF_NOMIPMAP))
	{
		TRACE(("dbg: GL_Upload32: GL_SGIS_generate_mipmap\n"));
		qglTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
	}

	if (scaled_width == width && scaled_height == height)
	{
		if ((flags&IF_NOMIPMAP)||gl_config.sgis_generate_mipmap)	//gotta love this with NPOT textures... :)
		{
			TRACE(("dbg: GL_Upload32: non-mipmapped/unscaled\n"));
			qglTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, glcolormode, GL_UNSIGNED_BYTE, data);
			goto done;
		}
		memcpy (scaled, data, width*height*4);
	}
	else
		GL_ResampleTexture (data, width, height, scaled, scaled_width, scaled_height);

	TRACE(("dbg: GL_Upload32: recaled\n"));
	qglTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, glcolormode, GL_UNSIGNED_BYTE, scaled);
	if (!(flags&IF_NOMIPMAP) && !gl_config.sgis_generate_mipmap)
	{
		miplevel = 0;
		TRACE(("dbg: GL_Upload32: mips\n"));
		while (scaled_width > 1 || scaled_height > 1)
		{
			GL_MipMap ((qbyte *)scaled, scaled_width, scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;
			if (scaled_width < 1)
				scaled_width = 1;
			if (scaled_height < 1)
				scaled_height = 1;
			miplevel++;
			qglTexImage2D (GL_TEXTURE_2D, miplevel, samples, scaled_width, scaled_height, 0, glcolormode, GL_UNSIGNED_BYTE, scaled);
		}
	}
	if (gl_config.arb_texture_compression && gl_compress.value && gl_savecompressedtex.value && name && !(flags&IF_NOMIPMAP))
	{
		vfsfile_t *out;
		int miplevels;
		GLint compressed;
		GLint compressed_size;
		GLint internalformat;
		unsigned char *img;
		char outname[MAX_OSPATH];
		int i;
		miplevels = miplevel+1;
		qglGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_ARB, &compressed);
		if (compressed == GL_TRUE && !strstr(name, ".."))	//is there any point in bothering with the whole endian thing?
		{
			sprintf(outname, "tex/%s.tex", name);
			FS_CreatePath(outname, FS_GAME);
			out = FS_OpenVFS(outname, "wb", FS_GAME);
			if (out)
			{
				i = LittleLong(miplevels);
				VFS_WRITE(out, &i, sizeof(i));
				i = LittleLong(width);
				VFS_WRITE(out, &i, sizeof(i));
				i = LittleLong(height);
				VFS_WRITE(out, &i, sizeof(i));
				i = LittleLong(flags);
				VFS_WRITE(out, &i, sizeof(i));
				for (miplevel = 0; miplevel < miplevels; miplevel++)
				{
					qglGetTexLevelParameteriv(GL_TEXTURE_2D, miplevel, GL_TEXTURE_COMPRESSED_ARB, &compressed);
					qglGetTexLevelParameteriv(GL_TEXTURE_2D, miplevel, GL_TEXTURE_INTERNAL_FORMAT, &internalformat);
					qglGetTexLevelParameteriv(GL_TEXTURE_2D, miplevel, GL_TEXTURE_COMPRESSED_IMAGE_SIZE_ARB, &compressed_size);
					qglGetTexLevelParameteriv(GL_TEXTURE_2D, miplevel, GL_TEXTURE_WIDTH, &width);
					qglGetTexLevelParameteriv(GL_TEXTURE_2D, miplevel, GL_TEXTURE_HEIGHT, &height);
					img = (unsigned char *)BZ_Malloc(compressed_size * sizeof(unsigned char));
					qglGetCompressedTexImageARB(GL_TEXTURE_2D, miplevel, img);

					i = LittleLong(width);
					VFS_WRITE(out, &i, sizeof(i));
					i = LittleLong(height);
					VFS_WRITE(out, &i, sizeof(i));
					i = LittleLong(compressed_size);
					VFS_WRITE(out, &i, sizeof(i));
					i = LittleLong(internalformat);
					VFS_WRITE(out, &i, sizeof(i));
					VFS_WRITE(out, img, compressed_size);
					BZ_Free(img);
				}
				VFS_CLOSE(out);
			}
		}
	}
done:
	if (gl_config.sgis_generate_mipmap && !(flags&IF_NOMIPMAP))
		qglTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_FALSE);

	if (gl_anisotropy_factor)
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_anisotropy_factor); // without this, you could loose anisotropy on mapchange

	if (!(flags&IF_NOMIPMAP))
	{
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max_2d);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max_2d);
	}

	if (flags&IF_CLAMP)
	{
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
}

void GL_Upload32 (char *name, unsigned *data, int width, int height, unsigned int flags)
{
	GL_Upload32_Int(name, data, width, height, flags, GL_RGBA);
}
void GL_Upload32_BGRA (char *name, unsigned *data, int width, int height, unsigned int flags)
{
	GL_Upload32_Int(name, data, width, height, flags, GL_BGRA_EXT);
}

void GL_Upload24BGR (char *name, qbyte *framedata, int inwidth, int inheight, unsigned int flags)
{
	int outwidth, outheight;
	int y, x;

	int v;
	unsigned int f, fstep;
	qbyte *src, *dest;
	dest = uploadmemorybufferintermediate;
	//change from bgr bottomup to rgba topdown

	for (outwidth = 1; outwidth < inwidth; outwidth*=2)
		;
	for (outheight = 1; outheight < inheight; outheight*=2)
		;

	if (outwidth > 512)
		outwidth = 512;
	if (outheight > 512)
		outheight = 512;

	if (outwidth*outheight > sizeofuploadmemorybufferintermediate/4)
		Sys_Error("MediaGL_ShowFrameBGR_24_Flip: image too big (%i*%i)", inwidth, inheight);

	for (y=0 ; y<outheight ; y++)
	{
		v = (y*(float)inheight/outheight);
		src = framedata + v*(inwidth*3);
		{
			f = 0;
			fstep = ((inwidth)*0x10000)/outwidth;

			for (x=outwidth ; x&3 ; x--)	//do the odd ones first. (bigger condition)
			{
				*dest++	= src[(f>>16)*3+2];
				*dest++	= src[(f>>16)*3+1];
				*dest++	= src[(f>>16)*3+0];
				*dest++	= 255;
				f += fstep;
			}
			for ( ; x ; x-=4)	//loop through the remaining chunks.
			{
				dest[0]		= src[(f>>16)*3+2];
				dest[1]		= src[(f>>16)*3+1];
				dest[2]		= src[(f>>16)*3+0];
				dest[3]		= 255;
				f += fstep;

				dest[4]		= src[(f>>16)*3+2];
				dest[5]		= src[(f>>16)*3+1];
				dest[6]		= src[(f>>16)*3+0];
				dest[7]		= 255;
				f += fstep;

				dest[8]		= src[(f>>16)*3+2];
				dest[9]		= src[(f>>16)*3+1];
				dest[10]	= src[(f>>16)*3+0];
				dest[11]	= 255;
				f += fstep;

				dest[12]	= src[(f>>16)*3+2];
				dest[13]	= src[(f>>16)*3+1];
				dest[14]	= src[(f>>16)*3+0];
				dest[15]	= 255;
				f += fstep;

				dest += 16;
			}
		}
	}

	GL_Upload32 (name, (unsigned int*)uploadmemorybufferintermediate, outwidth, outheight, flags);
}
void GL_Upload24BGR_Flip (char *name, qbyte *framedata, int inwidth, int inheight, unsigned int flags)
{
	int outwidth, outheight;
	int y, x;

	int v;
	unsigned int f, fstep;
	qbyte *src, *dest;
	dest = uploadmemorybufferintermediate;
	//change from bgr bottomup to rgba topdown

	for (outwidth = 1; outwidth < inwidth; outwidth*=2)
		;
	for (outheight = 1; outheight < inheight; outheight*=2)
		;

	if (outwidth > 512)
		outwidth = 512;
	if (outheight > 512)
		outheight = 512;

	if (outwidth*outheight > sizeofuploadmemorybufferintermediate/4)
		Sys_Error("MediaGL_ShowFrameBGR_24_Flip: image too big (%i*%i)", inwidth, inheight);

	for (y=1 ; y<=outheight ; y++)
	{
		v = ((outheight - y)*(float)inheight/outheight);
		src = framedata + v*(inwidth*3);
		{
			f = 0;
			fstep = ((inwidth)*0x10000)/outwidth;

			for (x=outwidth ; x&3 ; x--)	//do the odd ones first. (bigger condition)
			{
				*dest++	= src[(f>>16)*3+2];
				*dest++	= src[(f>>16)*3+1];
				*dest++	= src[(f>>16)*3+0];
				*dest++	= 255;
				f += fstep;
			}
			for ( ; x ; x-=4)	//loop through the remaining chunks.
			{
				dest[0]		= src[(f>>16)*3+2];
				dest[1]		= src[(f>>16)*3+1];
				dest[2]		= src[(f>>16)*3+0];
				dest[3]		= 255;
				f += fstep;

				dest[4]		= src[(f>>16)*3+2];
				dest[5]		= src[(f>>16)*3+1];
				dest[6]		= src[(f>>16)*3+0];
				dest[7]		= 255;
				f += fstep;

				dest[8]		= src[(f>>16)*3+2];
				dest[9]		= src[(f>>16)*3+1];
				dest[10]	= src[(f>>16)*3+0];
				dest[11]	= 255;
				f += fstep;

				dest[12]	= src[(f>>16)*3+2];
				dest[13]	= src[(f>>16)*3+1];
				dest[14]	= src[(f>>16)*3+0];
				dest[15]	= 255;
				f += fstep;

				dest += 16;
			}
		}
	}

	GL_Upload32 (name, (unsigned int*)uploadmemorybufferintermediate, outwidth, outheight, flags);
}


void GL_Upload8Grey (unsigned char*data, int width, int height, unsigned int flags)
{
	int			samples;
	unsigned char	*scaled = uploadmemorybuffer;
	int			scaled_width, scaled_height;

	scaled_width = width;
	scaled_height = height;
	GL_RoundDimensions(&scaled_width, &scaled_height, !(flags&IF_NOMIPMAP));

	if (scaled_width * scaled_height > sizeofuploadmemorybuffer/4)
		Sys_Error ("GL_LoadTexture: too big");

	samples = 1;//alpha ? gl_alpha_format : gl_solid_format;

	if (scaled_width == width && scaled_height == height)
	{
		if (flags&IF_NOMIPMAP)
		{
			qglTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
			goto done;
		}
		memcpy (scaled, data, width*height);
	}
	else
		GL_Resample8BitTexture (data, width, height, scaled, scaled_width, scaled_height);

	qglTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, scaled);
	if (!(flags&IF_NOMIPMAP))
	{
		int		miplevel;

		miplevel = 0;
		while (scaled_width > 1 || scaled_height > 1)
		{
			GL_MipMap ((qbyte *)scaled, scaled_width, scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;
			if (scaled_width < 1)
				scaled_width = 1;
			if (scaled_height < 1)
				scaled_height = 1;
			miplevel++;
			qglTexImage2D (GL_TEXTURE_2D, miplevel, samples, scaled_width, scaled_height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, scaled);
		}
	}
done: ;

	if (!(flags&IF_NOMIPMAP))
	{
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max_2d);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max_2d);
	}
}











void GL_MipMapNormal (qbyte *in, int width, int height)
{
	int		i, j;
	qbyte	*out;
	float	inv255	= 1.0f/255.0f;
	float	inv127	= 1.0f/127.0f;
	float	x,y,z,l,mag00,mag01,mag10,mag11;


	width <<=2;
	height >>= 1;
	out = in;
	for (i=0 ; i<height ; i++, in+=width)
	{
		for (j=0 ; j<width ; j+=8, out+=4, in+=8)
		{

			mag00 = inv255 * in[3];
			mag01 = inv255 * in[7];
			mag10 = inv255 * in[width+3];
			mag11 = inv255 * in[width+7];

			x = mag00*(inv127*in[0]-1.0)+
				mag01*(inv127*in[4]-1.0)+
				mag10*(inv127*in[width+0]-1.0)+
				mag11*(inv127*in[width+4]-1.0);
			y = mag00*(inv127*in[1]-1.0)+
				mag01*(inv127*in[5]-1.0)+
				mag10*(inv127*in[width+1]-1.0)+
				mag11*(inv127*in[width+5]-1.0);
			z = mag00*(inv127*in[2]-1.0)+
				mag01*(inv127*in[6]-1.0)+
				mag10*(inv127*in[width+2]-1.0)+
				mag11*(inv127*in[width+6]-1.0);

			l = sqrt(x*x+y*y+z*z);
			if (l == 0.0) {
				x = 0.0;
				y = 0.0;
				z = 1.0;
			} else {
				//normalize it.
				l=1/l;
				x *=l;
				y *=l;
				z *=l;
			}
			out[0] = (unsigned char)128 + 127*x;
			out[1] = (unsigned char)128 + 127*y;
			out[2] = (unsigned char)128 + 127*z;

			l = l/4.0;
			if (l > 1.0) {
				out[3] = 255;
			} else {
				out[3] = (qbyte)(255.0*l);
			}
		}
	}
}

//PENTA

//sizeofuploadmemorybufferintermediate is guarenteed to be bigger or equal to the normal uploadbuffer size
unsigned int * genNormalMap(qbyte *pixels, int w, int h, float scale)
{
  int i, j, wr, hr;
  unsigned char r, g, b;
  unsigned *nmap = (unsigned *)uploadmemorybufferintermediate;
  float sqlen, reciplen, nx, ny, nz;

  const float oneOver255 = 1.0f/255.0f;

  float c, cx, cy, dcx, dcy;

  wr = w;
  hr = h;

  for (i=0; i<h; i++) {
    for (j=0; j<w; j++) {
      /* Expand [0,255] texel values to the [0,1] range. */
      c = pixels[i*wr + j] * oneOver255;
      /* Expand the texel to its right. */
      cx = pixels[i*wr + (j+1)%wr] * oneOver255;
      /* Expand the texel one up. */
      cy = pixels[((i+1)%hr)*wr + j] * oneOver255;
      dcx = scale * (c - cx);
      dcy = scale * (c - cy);

      /* Normalize the vector. */
      sqlen = dcx*dcx + dcy*dcy + 1;
      reciplen = 1.0f/(float)sqrt(sqlen);
      nx = dcx*reciplen;
      ny = -dcy*reciplen;
      nz = reciplen;

      /* Repack the normalized vector into an RGB unsigned qbyte
         vector in the normal map image. */
      r = (qbyte) (128 + 127*nx);
      g = (qbyte) (128 + 127*ny);
      b = (qbyte) (128 + 127*nz);

      /* The highest resolution mipmap level always has a
         unit length magnitude. */
      nmap[i*w+j] = LittleLong ((pixels[i*wr + j] << 24)|(b << 16)|(g << 8)|(r));	// <AWE> Added support for big endian.
    }
  }

  return &nmap[0];
}

//PENTA
void GL_UploadBump(qbyte *data, int width, int height, qboolean mipmap, float bumpscale)
{
    unsigned char	*scaled = uploadmemorybuffer;
	int			scaled_width, scaled_height;
	qbyte			*nmap;

	TRACE(("dbg: GL_UploadBump entered: %i %i\n", width, height));

	scaled_width = width;
	scaled_height = height;
	GL_RoundDimensions(&scaled_width, &scaled_height, mipmap);

	if (scaled_width * scaled_height > sizeofuploadmemorybuffer/4)
		Sys_Error ("GL_LoadTexture: too big");

	//To resize or not to resize
	if (scaled_width == width && scaled_height == height)
	{
		memcpy (scaled, data, width*height);
		scaled_width = width;
		scaled_height = height;
	}
	else {
		//Just picks pixels so grayscale is equivalent with 8 bit.
		GL_Resample8BitTexture (data, width, height, scaled, scaled_width, scaled_height);
	}

	nmap = (qbyte *)genNormalMap(scaled,scaled_width,scaled_height,bumpscale);

	qglTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA
		, scaled_width, scaled_height, 0,
					GL_RGBA, GL_UNSIGNED_BYTE, nmap);

	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	if (mipmap)
	{
		int		miplevel;

		miplevel = 0;
		while (scaled_width > 1 || scaled_height > 1)
		{
			GL_MipMapNormal(nmap,scaled_width,scaled_height);
			//GL_MipMapGray((qbyte *)scaled, scaled_width, scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;
			if (scaled_width < 1)
				scaled_width = 1;
			if (scaled_height < 1)
				scaled_height = 1;
			miplevel++;

			qglTexImage2D (GL_TEXTURE_2D, miplevel, GL_RGBA, scaled_width, scaled_height, 0, GL_RGBA,
						GL_UNSIGNED_BYTE, nmap);
			//glTexImage2D (GL_TEXTURE_2D, miplevel, GL_RGBA, scaled_width, scaled_height, 0, GL_RGBA,
			//			GL_UNSIGNED_BYTE, genNormalMap(scaled,scaled_width,scaled_height,4.0f));
		}
	}

	if (mipmap)
	{
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max_2d);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max_2d);
	}

//	if (gl_texturefilteranisotropic)
//		glTexParameterfv (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &gl_texureanisotropylevel);

	TRACE(("dbg: GL_UploadBump: escaped %i %i\n", width, height));
}




#ifdef GL_USE8BITTEX
#ifdef GL_EXT_paletted_texture
void GL_Upload8_EXT (qbyte *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	int			i, s;
	qboolean	noalpha;
	int			samples;
    unsigned char *scaled = uploadmemorybuffer;
	int			scaled_width, scaled_height;

	GLDraw_Init15to8();

	s = width*height;
	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if (alpha)
	{
		noalpha = true;
		for (i=0 ; i<s ; i++)
		{
			if (data[i] == 255)
				noalpha = false;
		}

		if (alpha && noalpha)
			alpha = false;
	}

	scaled_width = width;
	scaled_height = height;
	GL_RoundDimensions(&scaled_width, &scaled_height, mipmap);

	if (scaled_width * scaled_height > sizeofuploadmemorybufferintermediate/4)
		Sys_Error ("GL_LoadTexture: too big");

	samples = 1; // alpha ? gl_alpha_format : gl_solid_format;

	if (scaled_width == width && scaled_height == height)
	{
		if (!mipmap)
		{
			glTexImage2D (GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, scaled_width, scaled_height, 0, GL_COLOR_INDEX , GL_UNSIGNED_BYTE, data);
			goto done;
		}
		memcpy (scaled, data, width*height);
	}
	else
		GL_Resample8BitTexture (data, width, height, scaled, scaled_width, scaled_height);

	glTexImage2D (GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, scaled_width, scaled_height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, scaled);
	if (mipmap)
	{
		int		miplevel;

		miplevel = 0;
		while (scaled_width > 1 || scaled_height > 1)
		{
			GL_MipMap8Bit ((qbyte *)scaled, scaled_width, scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;
			if (scaled_width < 1)
				scaled_width = 1;
			if (scaled_height < 1)
				scaled_height = 1;
			miplevel++;
			glTexImage2D (GL_TEXTURE_2D, miplevel, GL_COLOR_INDEX8_EXT, scaled_width, scaled_height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, scaled);
		}
	}
done: ;

	if (mipmap)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max_2d);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max_2d);
	}
}
#endif
#endif

/*
===============
GL_Upload8
===============
*/
int ColorIndex[16] =
{
	0, 31, 47, 63, 79, 95, 111, 127, 143, 159, 175, 191, 199, 207, 223, 231
};

unsigned ColorPercent[16] =
{
	25, 51, 76, 102, 114, 127, 140, 153, 165, 178, 191, 204, 216, 229, 237, 247
};

void GL_Upload8 (char *name, qbyte *data, int width, int height, unsigned int flags, unsigned int alpha)
{
	unsigned	*trans = (unsigned *)uploadmemorybufferintermediate;
	int			i, s;
	qboolean	noalpha;
	int			p;

	if (width*height > sizeofuploadmemorybufferintermediate/4)
		Sys_Error("GL_Upload8: image too big (%i*%i)", width, height);

	s = width*height;
	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if (alpha && !(flags & IF_NOALPHA))
	{
		noalpha = true;
		for (i=0 ; i<s ; i++)
		{
			p = data[i];
			if (p == 255)
			{
				noalpha = false;
				trans[i] = 0;
			}
			else
				trans[i] = d_8to24rgbtable[p];
		}

		switch( alpha )
		{
		default:
			if (alpha && noalpha)
				alpha = false;
			break;
		case 2:
			alpha = true;
			for (i=0 ; i<s ; i++)
			{
				p = data[i];
				if (p == 0)
					trans[i] &= 0x00ffffff;
				else if( p & 1 )
				{
					trans[i] &= 0x00ffffff;
					trans[i] |= ( ( int )( 255 * 0.5 ) ) << 24;
				}
				else
				{
					trans[i] |= 0xff000000;
				}
			}
			break;
		case 3:
			alpha = true;
			for (i=0 ; i<s ; i++)
			{
				p = data[i];
				if (p == 0)
					trans[i] &= 0x00ffffff;
			}
			break;
		case 4:
			alpha = true;
			for (i=0 ; i<s ; i++)
			{
				p = data[i];
				trans[i] = d_8to24rgbtable[ColorIndex[p>>4]] & 0x00ffffff;
				trans[i] |= ( int )ColorPercent[p&15] << 24;
				//trans[i] = 0x7fff0000;
			}
			break;
		}
		//2:H2_T7G1
		//3:H2_TRANS8_0
		//4:H2_T4A4
	}
	else
	{
		for (i=(s&~3)-4 ; i>=0 ; i-=4)
		{
			trans[i] = d_8to24rgbtable[data[i]]|0xff000000;
			trans[i+1] = d_8to24rgbtable[data[i+1]]|0xff000000;
			trans[i+2] = d_8to24rgbtable[data[i+2]]|0xff000000;
			trans[i+3] = d_8to24rgbtable[data[i+3]]|0xff000000;
		}
		for (i=s&~3 ; i<s ; i++)	//wow, funky
		{
			trans[i] = d_8to24rgbtable[data[i]];
		}
	}

#ifdef GL_USE8BITTEX
#ifdef GL_EXT_paletted_texture
	if (GLVID_Is8bit() && !alpha && (data!=scrap_texels[0])) {
		GL_Upload8_EXT (data, width, height, mipmap, alpha);
		return;
	}
#endif
#endif
checkglerror();
	GL_Upload32 (name, trans, width, height, flags);
checkglerror();
}

void GL_Upload8FB (qbyte *data, int width, int height, unsigned flags)
{
	unsigned	*trans = (unsigned *)uploadmemorybufferintermediate;
	int			i, s;
	int			p;

	s = width*height;
	if (s > sizeofuploadmemorybufferintermediate/4)
		Sys_Error("GL_Upload8FB: image too big (%i*%i)", width, height);
	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	for (i=0 ; i<s ; i++)
	{
		p = data[i];
		if (p <= 255-vid.fullbright)
			trans[i] = 0;
		else
			trans[i] = d_8to24rgbtable[p];
	}

	GL_Upload32 (NULL, trans, width, height, flags);
}

void GL_Upload8Pal24 (qbyte *data, qbyte *pal, int width, int height, unsigned int flags)
{
	qbyte		*trans = uploadmemorybufferintermediate;
	int			i, s;
	qboolean	noalpha;
	int			p;
	extern qbyte gammatable[256];
	extern qboolean		gammaworks;

	s = width*height;
	if (s > sizeofuploadmemorybufferintermediate/4)
		Sys_Error("GL_Upload8Pal24: image too big (%i*%i)", width, height);

	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if (gammaworks)
	{
		if (!(flags & IF_NOALPHA))
		{
			noalpha = true;
			for (i=0 ; i<s ; i++)
			{
				p = data[i];
				if (p == 255)
					noalpha = false;
				trans[(i<<2)+0] = pal[p*3+0];
				trans[(i<<2)+1] = pal[p*3+1];
				trans[(i<<2)+2] = pal[p*3+2];
				trans[(i<<2)+3] = (p==255)?0:255;
			}

			if (noalpha)
				flags |= IF_NOALPHA;
		}
		else
		{
			if (s&3)
				Sys_Error ("GL_Upload8: s&3");
			for (i=0 ; i<s ; i+=1)
			{
				trans[(i<<2)+0] = pal[data[i]*3+0];
				trans[(i<<2)+1] = pal[data[i]*3+1];
				trans[(i<<2)+2] = pal[data[i]*3+2];
				trans[(i<<2)+3] = 255;
			}
		}

	}
	else 
	{
		if (!(flags & IF_NOALPHA))
		{
			noalpha = true;
			for (i=0 ; i<s ; i++)
			{
				p = data[i];
				if (p == 255)
					noalpha = false;
				trans[(i<<2)+0] = gammatable[pal[p*3+0]];
				trans[(i<<2)+1] = gammatable[pal[p*3+1]];
				trans[(i<<2)+2] = gammatable[pal[p*3+2]];
				trans[(i<<2)+3] = (p==255)?0:255;
			}

			if (noalpha)
				flags |= IF_NOALPHA;
		}
		else
		{
			if (s&3)
				Sys_Error ("GL_Upload8: s&3");
			for (i=0 ; i<s ; i+=1)
			{
				trans[(i<<2)+0] = gammatable[pal[data[i]*3+0]];
				trans[(i<<2)+1] = gammatable[pal[data[i]*3+1]];
				trans[(i<<2)+2] = gammatable[pal[data[i]*3+2]];
				trans[(i<<2)+3] = 255;
			}
		}
	}
	GL_Upload32 (NULL, (unsigned*)trans, width, height, flags);
}
static void GL_Upload8Pal32 (qbyte *data, qbyte *pal, int width, int height, unsigned int flags)
{
	qbyte		*trans = uploadmemorybufferintermediate;
	int			i, s;
	extern qbyte gammatable[256];

	s = width*height;
	if (s > sizeofuploadmemorybufferintermediate/4)
		Sys_Error("GL_Upload8Pal32: image too big (%i*%i)", width, height);

	if (s&3)
		Sys_Error ("GL_Upload8: s&3");
	for (i=0 ; i<s ; i+=1)
	{
		trans[(i<<2)+0] = gammatable[pal[data[i]*4+0]];
		trans[(i<<2)+1] = gammatable[pal[data[i]*4+1]];
		trans[(i<<2)+2] = gammatable[pal[data[i]*4+2]];
		trans[(i<<2)+3] = gammatable[pal[data[i]*4+3]];
	}

	GL_Upload32 (NULL, (unsigned*)trans, width, height, flags);
}
/*
================
GL_LoadTexture
================
*/
texid_t GL_LoadTexture (char *identifier, int width, int height, qbyte *data, unsigned int flags, unsigned int transtype)
{
	gltexture_t	*glt;

	// see if the texture is already present
	if (identifier[0])
	{
		glt = GL_MatchTexture(identifier, 8, width, height);
		if (glt)
		{

TRACE(("dbg: GL_LoadTexture: duplicate %s\n", identifier));
			return glt->texnum;
		}
	}

TRACE(("dbg: GL_LoadTexture: new %s\n", identifier));

	glt = BZ_Malloc(sizeof(*glt)+sizeof(bucket_t));
	glt->next = gltextures;
	gltextures = glt;

	strcpy (glt->identifier, identifier);
	glt->texnum = GL_AllocNewTexture(width, height);
	glt->width = width;
	glt->height = height;
	glt->bpp = 8;
	glt->flags = flags;

	Hash_Add(&gltexturetable, glt->identifier, glt, (bucket_t*)(glt+1));
checkglerror();
	GL_MTBind(0, GL_TEXTURE_2D, glt->texnum);

	GL_Upload8 ("8bit", data, width, height, flags, transtype);
checkglerror();
	return glt->texnum;
}

texid_t GL_LoadTextureFB (char *identifier, int width, int height, qbyte *data, unsigned int flags)
{
	int			i;
	gltexture_t	*glt;

	// see if the texture is already present
	if (identifier[0])
	{
		glt = GL_MatchTexture(identifier, 8, width, height);
		if (glt)
			return glt->texnum;
	}

	for (i = 0; i < width*height; i++)
		if (data[i] > 255-vid.fullbright)
			break;

	if (i == width*height)
		return r_nulltex;	//none found, don't bother uploading.

	glt = BZ_Malloc(sizeof(*glt)+sizeof(bucket_t));
	glt->next = gltextures;
	gltextures = glt;

	strcpy (glt->identifier, identifier);
	glt->texnum = GL_AllocNewTexture(width, height);
	glt->width = width;
	glt->height = height;
	glt->bpp = 8;
	glt->flags = flags;

	Hash_Add(&gltexturetable, glt->identifier, glt, (bucket_t*)(glt+1));

	GL_MTBind(0, GL_TEXTURE_2D, glt->texnum);

	GL_Upload8FB (data, width, height, flags);

	return glt->texnum;
}

texid_t GL_LoadTexture8Pal24 (char *identifier, int width, int height, qbyte *data, qbyte *palette24, unsigned int flags)
{
	gltexture_t	*glt;

		// see if the texture is already present
	if (identifier[0])
	{
		glt = GL_MatchTexture(identifier, 24, width, height);
		if (glt)
			return glt->texnum;
	}

	glt = BZ_Malloc(sizeof(*glt)+sizeof(bucket_t));
	glt->next = gltextures;
	gltextures = glt;


	strcpy (glt->identifier, identifier);
	glt->texnum = GL_AllocNewTexture(width, height);
	glt->width = width;
	glt->height = height;
	glt->bpp = 24;
	glt->flags = flags;

	Hash_Add(&gltexturetable, glt->identifier, glt, (bucket_t*)(glt+1));

	GL_MTBind(0, GL_TEXTURE_2D, glt->texnum);

	GL_Upload8Pal24 (data, palette24, width, height, flags);

	return glt->texnum;
}
texid_t GL_LoadTexture8Pal32 (char *identifier, int width, int height, qbyte *data, qbyte *palette32, unsigned int flags)
{
	gltexture_t	*glt;

		// see if the texture is already present
	if (identifier[0])
	{
		glt = GL_MatchTexture(identifier, 32, width, height);
		if (glt)
			return glt->texnum;
	}

	glt = BZ_Malloc(sizeof(*glt)+sizeof(bucket_t));
	glt->next = gltextures;
	gltextures = glt;


	strcpy (glt->identifier, identifier);
	glt->texnum = GL_AllocNewTexture(width, height);
	glt->width = width;
	glt->height = height;
	glt->bpp = 32;
	glt->flags = flags;

	Hash_Add(&gltexturetable, glt->identifier, glt, (bucket_t*)(glt+1));

	GL_MTBind(0, GL_TEXTURE_2D, glt->texnum);

	GL_Upload8Pal32 (data, palette32, width, height, flags);

	return glt->texnum;
}

texid_t GL_LoadTexture32 (char *identifier, int width, int height, void *data, unsigned int flags)
{
//	qboolean	noalpha;
//	int			p, s;
	gltexture_t	*glt;

	// see if the texture is already present
	if (identifier[0])
	{
		glt = GL_MatchTexture(identifier, 32, width, height);
		if (glt)
			return glt->texnum;
	}

	glt = BZ_Malloc(sizeof(*glt)+sizeof(bucket_t));
	glt->next = gltextures;
	gltextures = glt;

	strcpy (glt->identifier, identifier);
	glt->texnum = GL_AllocNewTexture(width, height);
	glt->width = width;
	glt->height = height;
	glt->bpp = 32;
	glt->flags = flags;

	Hash_Add(&gltexturetable, glt->identifier, glt, (bucket_t*)(glt+1));

	GL_MTBind(0, GL_TEXTURE_2D, glt->texnum);

	GL_Upload32 (identifier, data, width, height, flags);

	return glt->texnum;
}

texid_t GL_LoadTexture32_BGRA (char *identifier, int width, int height, unsigned *data, unsigned int flags)
{
//	qboolean	noalpha;
//	int			p, s;
	gltexture_t	*glt;

	// see if the texture is already present
	if (identifier[0])
	{
		glt = GL_MatchTexture(identifier, 32, width, height);
		if (glt)
			return glt->texnum;
	}

	glt = BZ_Malloc(sizeof(*glt)+sizeof(bucket_t));
	glt->next = gltextures;
	gltextures = glt;

	strcpy (glt->identifier, identifier);
	glt->texnum = GL_AllocNewTexture(width, height);
	glt->width = width;
	glt->height = height;
	glt->bpp = 32;
	glt->flags = flags;

	Hash_Add(&gltexturetable, glt->identifier, glt, (bucket_t*)(glt+1));

	GL_MTBind(0, GL_TEXTURE_2D, glt->texnum);

	GL_Upload32_BGRA (identifier, data, width, height, flags);

	return glt->texnum;
}

texid_t GL_LoadCompressed(char *name)
{
	unsigned char *file;
	gltexture_t	*glt;
	char inname[MAX_OSPATH];

	if (!gl_config.arb_texture_compression || !gl_compress.ival)
		return r_nulltex;


	// see if the texture is already present
	if (name[0])
	{
		texid_t num = GL_FindTexture(name);
		if (TEXVALID(num))
			return num;
	}
	else
		return r_nulltex;


	snprintf(inname, sizeof(inname)-1, "tex/%s.tex", name);
	file = COM_LoadFile(inname, 5);
	if (!file)
		return r_nulltex;

	glt = BZ_Malloc(sizeof(*glt)+sizeof(bucket_t));
	glt->next = gltextures;
	gltextures = glt;

	strcpy (glt->identifier, name);
	glt->texnum = GL_AllocNewTexture(0, 0);
	glt->bpp = 32;
	glt->flags = 0;

	Hash_Add(&gltexturetable, glt->identifier, glt, (bucket_t*)(glt+1));

	GL_MTBind(0, GL_TEXTURE_2D, glt->texnum);

	if (!GL_UploadCompressed(file, &glt->width, &glt->height, (unsigned int *)&glt->flags))
		return r_nulltex;

	return glt->texnum;
}

texid_t GL_LoadTexture8Grey (char *identifier, int width, int height, unsigned char *data, unsigned int flags)
{
//	qboolean	noalpha;
//	int			p, s;
	gltexture_t	*glt;

	// see if the texture is already present
	if (identifier[0])
	{
		glt = GL_MatchTexture(identifier, 8, width, height);
		if (glt)
			return glt->texnum;
	}

	flags |= IF_NOALPHA;

	glt = BZ_Malloc(sizeof(*glt)+sizeof(bucket_t));
	glt->next = gltextures;
	gltextures = glt;

	strcpy (glt->identifier, identifier);
	glt->texnum = GL_AllocNewTexture(width, height);
	glt->width = width;
	glt->height = height;
	glt->bpp = 8;
	glt->flags = flags;

	Hash_Add(&gltexturetable, glt->identifier, glt, (bucket_t*)(glt+1));

	GL_MTBind(0, GL_TEXTURE_2D, glt->texnum);

	GL_Upload8Grey (data, width, height, flags);

	return glt->texnum;
}

texid_t GL_LoadTexture8Bump (char *identifier, int width, int height, unsigned char *data, unsigned int flags, float bumpscale)
{
//	qboolean	noalpha;
	//	int			p, s;
	gltexture_t	*glt;

	// see if the texture is already present
	if (identifier[0])
	{
		glt = GL_MatchTexture(identifier, 8, width, height);
		if (glt)
		{
	TRACE(("dbg: GL_LoadTexture8Bump: duplicated %s\n", identifier));
			return glt->texnum;
		}
	}

	TRACE(("dbg: GL_LoadTexture8Bump: new %s\n", identifier));

	glt = BZ_Malloc(sizeof(*glt)+sizeof(bucket_t));
	glt->next = gltextures;
	gltextures = glt;

	strcpy (glt->identifier, identifier);
	glt->texnum = GL_AllocNewTexture(width, height);
	glt->width = width;
	glt->height = height;
	glt->bpp = 8;
	glt->flags = flags;

	Hash_Add(&gltexturetable, glt->identifier, glt, (bucket_t*)(glt+1));

	GL_MTBind(0, GL_TEXTURE_2D, glt->texnum);

	GL_UploadBump (data, width, height, flags, bumpscale);

	return glt->texnum;
}

/*
================
GL_LoadPicTexture
================
*/
texid_t GL_LoadPicTexture (qpic_t *pic)
{
	return GL_LoadTexture ("", pic->width, pic->height, pic->data, IF_NOMIPMAP, 1);
}

#endif
