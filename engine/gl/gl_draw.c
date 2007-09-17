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
#ifdef RGLQUAKE
#include "glquake.h"
#include "shader.h"

#include <stdlib.h> // is this needed for atoi?
#include <stdio.h> // is this needed for atoi?

//#define GL_USE8BITTEX

int glx, gly, glwidth, glheight;

mesh_t	draw_mesh;
vec3_t	draw_mesh_xyz[4];
vec2_t	draw_mesh_st[4];
byte_vec4_t	draw_mesh_colors[4];

qbyte				*uploadmemorybuffer;
int					sizeofuploadmemorybuffer;
qbyte				*uploadmemorybufferintermediate;
int					sizeofuploadmemorybufferintermediate;

index_t r_quad_indexes[6] = {0, 1, 2, 0, 2, 3};

extern qbyte		gammatable[256];

unsigned char *d_15to8table;
qboolean inited15to8;
extern cvar_t crosshair, crosshairimage, crosshairalpha, cl_crossx, cl_crossy, crosshaircolor, crosshairsize;

static int filmtexture;

extern cvar_t		gl_nobind;
extern cvar_t		gl_max_size;
extern cvar_t		gl_picmip;
extern cvar_t		gl_lerpimages;
extern cvar_t		gl_picmip2d;
extern cvar_t		r_drawdisk;
extern cvar_t		gl_compress;
extern cvar_t		gl_smoothfont, gl_smoothcrosshair, gl_fontinwardstep;
extern cvar_t		gl_texturemode, gl_texture_anisotropic_filtering;
extern cvar_t cl_noblink;

extern cvar_t		gl_savecompressedtex;

extern cvar_t		gl_load24bit;

#ifdef Q3SHADERS
shader_t	*shader_console;
#endif
extern cvar_t		con_ocranaleds;
extern cvar_t		gl_blend2d;
extern cvar_t		scr_conalpha;

qbyte		*draw_chars;				// 8*8 graphic characters
mpic_t		*draw_disc;
mpic_t		*draw_backtile;

int			translate_texture;
int			char_texture, char_tex2, default_char_texture;
int			missing_texture;	//texture used when one is missing.
int			cs_texture; // crosshair texture
extern int detailtexture;

float custom_char_instep, default_char_instep;	//to avoid blending issues
float	char_instep;

static unsigned cs_data[16*16];
static int externalhair;
int gl_anisotropy_factor;

typedef struct
{
	int		texnum;
	float	sl, tl, sh, th;
} glpic_t;

qbyte		conback_buffer[sizeof(mpic_t) + sizeof(glpic_t)];
qbyte		custconback_buffer[sizeof(mpic_t) + sizeof(glpic_t)];
mpic_t		*default_conback = (mpic_t *)&conback_buffer, *conback, *custom_conback = (mpic_t *)&custconback_buffer;

#include "hash.h"
hashtable_t gltexturetable;
bucket_t *gltexturetablebuckets[256];

int		gl_lightmap_format = 4;
int		gl_solid_format = 3;
int		gl_alpha_format = 4;

int		gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int		gl_filter_max = GL_LINEAR;
int		gl_filter_max_2d = GL_LINEAR;

int		texels;

typedef struct gltexture_s
{
	int		texnum;
	char	identifier[64];
	int		width, height, bpp;
	qboolean	mipmap;
	struct gltexture_s *next;
} gltexture_t;

static gltexture_t	*gltextures;
/*
=============================================================================

  scrap allocation

  Allocate all the little status bar obejcts into a single texture
  to crutch up stupid hardware / drivers

=============================================================================
*/

#define	MAX_SCRAPS		4
#define	BLOCK_WIDTH		256
#define	BLOCK_HEIGHT	256

int			scrap_allocated[MAX_SCRAPS][BLOCK_WIDTH];
qbyte		scrap_texels[MAX_SCRAPS][BLOCK_WIDTH*BLOCK_HEIGHT];
qboolean	scrap_dirty;
int			scrap_usedcount;
int			scrap_texnum;

// returns a texture number and the position inside it
int Scrap_AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
	int		texnum;

	for (texnum=0 ; texnum<MAX_SCRAPS ; texnum++)
	{
		best = BLOCK_HEIGHT;

		for (i=0 ; i<BLOCK_WIDTH-w ; i++)
		{
			best2 = 0;

			for (j=0 ; j<w ; j++)
			{
				if (scrap_allocated[texnum][i+j] >= best)
					break;
				if (scrap_allocated[texnum][i+j] > best2)
					best2 = scrap_allocated[texnum][i+j];
			}
			if (j == w)
			{	// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (i=0 ; i<w ; i++)
			scrap_allocated[texnum][*x + i] = best + h;

		if (scrap_usedcount < texnum+1)
			scrap_usedcount = texnum+1;

		return texnum;
	}

	return -1;
}

int	scrap_uploads;

void Scrap_Upload (void)
{
	int i;
	scrap_uploads++;
	for (i = 0; i < scrap_usedcount; i++)
	{
		GL_Bind(scrap_texnum + i);
		GL_Upload8 ("scrap", scrap_texels[i], BLOCK_WIDTH, BLOCK_HEIGHT, false, true);
	}
	scrap_dirty = false;
}

//=============================================================================
/* Support Routines */

typedef struct glcachepic_s
{
	char		name[MAX_QPATH];
	mpic_t		pic;
	qbyte		padding[32];	// for appended glpic
} glcachepic_t;

#define	MAX_CACHED_PICS		512	//a temporary solution
glcachepic_t	glmenu_cachepics[MAX_CACHED_PICS];
int			glmenu_numcachepics;

int		pic_texels;
int		pic_count;

mpic_t *GLDraw_IsCached(char *name)
{
	glcachepic_t *pic;
	int i;

	for (pic=glmenu_cachepics, i=0 ; i<glmenu_numcachepics ; pic++, i++)
		if (!strcmp (name, pic->name))
			return &pic->pic;

	return NULL;
}

qboolean Draw_RealPicFromWad (mpic_t	*out, char *name)
{
	qpic_t	*in;
	glpic_t	*gl;
	int texnum;
	char name2[256];

	if (!strncmp(name, "gfx/", 4))
		in = W_SafeGetLumpName (name+4);
	else
		in = W_SafeGetLumpName (name);
	gl = (glpic_t *)out->data;

	if (in)
	{
		out->width = in->width;
		out->height = in->height;
	}
	else
	{	//default the size.
		out->width = 24;	//hmm...?
		out->height = 24;
	}

	//standard names substitution
	texnum = Mod_LoadReplacementTexture(name, "wad", false, true, false);
	if (!in && !texnum)	//try a q2 texture
	{
		sprintf(name2, "pics/%s", name);
		texnum = Mod_LoadHiResTexture(name2, NULL, false, true, false);
		qglDisable(GL_ALPHA_TEST);
		qglEnable(GL_BLEND);	//make sure.
	}

	if (texnum)
	{
		if (!in)
		{
			out->width = image_width;
			out->height = image_height;
		}
		gl->texnum = texnum;
		gl->sl = 0;
		gl->sh = 1;
		gl->tl = 0;
		gl->th = 1;
		return true;
	}
	//all the others require an actual infile rather than a replacement image
	else if (!in)
	{
		return false;
	}

	// load little ones into the scrap
	else if (in->width < 64 && in->height < 64)
	{
		int		x, y;
		int		i, j, k;
		int		texnum;

		texnum = Scrap_AllocBlock (in->width, in->height, &x, &y);
		if (texnum >= 0)
		{
			scrap_dirty = true;
			k = 0;
			for (i=0 ; i<in->height ; i++)
				for (j=0 ; j<in->width ; j++, k++)
					scrap_texels[texnum][(y+i)*BLOCK_WIDTH + x + j] = in->data[k];
			texnum += scrap_texnum;
			gl->texnum = texnum;
			gl->sl = (x+0.25)/(float)BLOCK_WIDTH;
			gl->sh = (x+in->width-0.25)/(float)BLOCK_WIDTH;
			gl->tl = (y+0.25)/(float)BLOCK_WIDTH;
			gl->th = (y+in->height-0.25)/(float)BLOCK_WIDTH;
			pic_count++;
			pic_texels += in->width*in->height;
		}
		else
		{
			gl->texnum = GL_LoadPicTexture (in);
			gl->sl = 0;
			gl->sh = 1;
			gl->tl = 0;
			gl->th = 1;
		}
	}
	else
	{
		gl->texnum = GL_LoadPicTexture (in);
		gl->sl = 0;
		gl->sh = 1;
		gl->tl = 0;
		gl->th = 1;
	}
	return true;
}

char *failedpic;	//easier this way
mpic_t *GLDraw_SafePicFromWad (char *name)
{
	int i;
	glcachepic_t	*pic;
	for (pic=glmenu_cachepics, i=0 ; i<glmenu_numcachepics ; pic++, i++)
		if (!strcmp (name, pic->name))
			return &pic->pic;

	if (glmenu_numcachepics == MAX_CACHED_PICS)
	{
		Con_Printf ("menu_numcachepics == MAX_CACHED_PICS\n");
		failedpic = name;
		return NULL;
	}

	glmenu_numcachepics++;

	strcpy(pic->name, name);
	if (!Draw_RealPicFromWad(&pic->pic, name))
	{
		glmenu_numcachepics--;
		failedpic = name;
		return NULL;
	}

	return &pic->pic;
}

mpic_t	*GLDraw_SafeCachePic (char *path)
{
	//this is EVIL! WRITE IT!

	int height = 0;
	qbyte *data;
	glcachepic_t	*pic;
	int			i;
	qpic_t		*qpic;
	glpic_t		*gl;

	for (pic=glmenu_cachepics, i=0 ; i<glmenu_numcachepics ; pic++, i++)
		if (!strcmp (path, pic->name))
			return &pic->pic;

	if (glmenu_numcachepics == MAX_CACHED_PICS)
		Sys_Error ("menu_numcachepics == MAX_CACHED_PICS");

//
// load the pic from disk
//
	{
		char *mem;
		char alternatename[MAX_QPATH];
		snprintf(alternatename, sizeof(alternatename), "pics/%s.pcx", path);
		data = COM_LoadMallocFile (alternatename);
		if (data)
		{
			strcpy(pic->name, path);
			if ((mem = ReadPCXFile(data, com_filesize, &pic->pic.width, &height)))
			{
				pic->pic.height = height;
				gl = (glpic_t *)pic->pic.data;
				if (!(gl->texnum = Mod_LoadReplacementTexture(alternatename, "pics", false, true, false)))
					gl->texnum = GL_LoadTexture32(path, pic->pic.width, pic->pic.height, (unsigned *)mem, false, false);
				gl->sl = 0;
				gl->sh = 1;
				gl->tl = 0;
				gl->th = 1;

				BZ_Free(data);
				BZ_Free(mem);
				glmenu_numcachepics++;
				return &pic->pic;
			}
			BZ_Free(data);
		}
	}

	{
		char *mem;
		char alternatename[MAX_QPATH];
		snprintf(alternatename, MAX_QPATH-1, "%s", path);
		data = COM_LoadMallocFile (alternatename);
		if (data)
		{
			strcpy(pic->name, path);
			mem = NULL;
			if (!mem)
				mem = ReadTargaFile((qbyte *)data, com_filesize, &pic->pic.width, &height, 0);
#ifdef AVAIL_PNGLIB
			if (!mem)
				mem = ReadPNGFile((qbyte *)data, com_filesize, &pic->pic.width, &height, alternatename);
#endif
#ifdef AVAIL_JPEGLIB
			if (!mem)
				mem = ReadJPEGFile((qbyte *)data, com_filesize, &pic->pic.width, &height);
#endif
			if (!mem)
				mem = ReadPCXFile((qbyte *)data, com_filesize, &pic->pic.width, &height);
			pic->pic.height = height;
			if (mem)
			{
				gl = (glpic_t *)pic->pic.data;
				if (!(gl->texnum = Mod_LoadReplacementTexture(alternatename, NULL, false, true, false)))
					gl->texnum = GL_LoadTexture32(path, pic->pic.width, pic->pic.height, (unsigned *)mem, false, true);
				gl->sl = 0;
				gl->sh = 1;
				gl->tl = 0;
				gl->th = 1;

				BZ_Free(data);
				BZ_Free(mem);
				glmenu_numcachepics++;
				return &pic->pic;
			}
			BZ_Free(data);
		}
	}

#ifdef AVAIL_JPEGLIB
	{
		char *mem;
		char alternatename[MAX_QPATH];
		snprintf(alternatename, MAX_QPATH-1,"%s.jpg", path);
		data = COM_LoadMallocFile (alternatename);
		if (data)
		{
			strcpy(pic->name, path);
			if ((mem = ReadJPEGFile(data, com_filesize, &pic->pic.width, &height)))
			{
				pic->pic.height = height;
				gl = (glpic_t *)pic->pic.data;
				if (!(gl->texnum = Mod_LoadReplacementTexture(alternatename, NULL, false, true, false)))
					gl->texnum = GL_LoadTexture32(path, pic->pic.width, pic->pic.height, (unsigned *)mem, false, false);
				gl->sl = 0;
				gl->sh = 1;
				gl->tl = 0;
				gl->th = 1;

				BZ_Free(data);
				BZ_Free(mem);
				glmenu_numcachepics++;
				return &pic->pic;
			}
			BZ_Free(data);
		}
	}
#endif
/*
	{
		char *mem;
		char alternatename[MAX_QPATH];
		_snprintf(alternatename, MAX_QPATH-1,"%s.tga", path);
		dat = (qpic_t *)COM_LoadMallocFile (alternatename);
		if (dat)
		{
			strcpy(pic->name, path);
			if (mem = ReadTargaFile ((qbyte *)dat, com_filesize, &pic->pic.width, &pic->pic.height, false))
			{
				gl = (glpic_t *)pic->pic.data;
				if (!(gl->texnum = Mod_LoadReplacementTexture(alternatename, false, true)))
					gl->texnum = GL_LoadTexture32(path, pic->pic.width, pic->pic.height, (unsigned *)dat, false, true);
				gl->sl = 0;
				gl->sh = 1;
				gl->tl = 0;
				gl->th = 1;

				BZ_Free(dat);
				BZ_Free(mem);
				glmenu_numcachepics++;
				return &pic->pic;
			}
			BZ_Free(dat);
		}
	}
*/
	qpic = (qpic_t *)COM_LoadTempFile (path);
	if (!qpic)
	{
		char alternatename[MAX_QPATH];
		sprintf(alternatename, "gfx/%s.lmp", path);
		qpic = (qpic_t *)COM_LoadTempFile (alternatename);
		if (!qpic)
		{
			mpic_t *m;
			m = GLDraw_SafePicFromWad(path);
			return m;
		}
	}

	SwapPic (qpic);

	if (((8+qpic->width*qpic->height+3)&(~3)) != ((com_filesize+3)&(~3)))	//round up to the nearest 4.
	{	//the filesize didn't match what we were expecting, so it can't be a lmp. reject it.
		char alternatename[MAX_QPATH];
		sprintf(alternatename, "gfx/%s.lmp", path);
		qpic = (qpic_t *)COM_LoadTempFile (alternatename);
		if (!qpic)
			return GLDraw_SafePicFromWad(path);
		SwapPic (qpic);
	}

	{
		glmenu_numcachepics++;
		Q_strncpyz (pic->name, path, sizeof(pic->name));
	}

	pic->pic.width = qpic->width;
	pic->pic.height = qpic->height;

	gl = (glpic_t *)pic->pic.data;
	if (!(gl->texnum = Mod_LoadReplacementTexture(path, NULL, false, true, false)))
		gl->texnum = GL_LoadPicTexture (qpic);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return &pic->pic;
}
mpic_t	*GLDraw_CachePic (char *path)
{
	mpic_t	*pic = GLDraw_SafeCachePic (path);
	if (!pic)
		Sys_Error ("GLDraw_CachePic: failed to load %s", path);

	return pic;
}

void GLDraw_CharToConback (int num, qbyte *dest)
{
	int		row, col;
	qbyte	*source;
	int		drawline;
	int		x;

	row = num>>4;
	col = num&15;
	source = draw_chars + (row<<10) + (col<<3);

	drawline = 8;

	while (drawline--)
	{
		for (x=0 ; x<8 ; x++)
			if (source[x] != 255)
				dest[x] = 0x60 + source[x];
		source += 128;
		dest += 320;
	}

}

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
		if (glt->mipmap)
		{
			//qglBindTexture (GL_TEXTURE_2D, glt->texnum);
			GL_Bind (glt->texnum);
			qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, (float)anfactor);
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
		if (glt->mipmap)
		{
			GL_Bind (glt->texnum);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
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
		if (!glt->mipmap)
		{
			GL_Bind (glt->texnum);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max_2d);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max_2d);
		}
	}
	Scrap_Upload();
}

#ifdef Q3SHADERS
#define FOG_TEXTURE_WIDTH 32
#define FOG_TEXTURE_HEIGHT 32
extern int r_fogtexture;
void GL_InitFogTexture (void)
{
	qbyte data[FOG_TEXTURE_WIDTH*FOG_TEXTURE_HEIGHT];
	int x, y;
	float tw = 1.0f / ((float)FOG_TEXTURE_WIDTH - 1.0f);
	float th = 1.0f / ((float)FOG_TEXTURE_HEIGHT - 1.0f);
	float tx, ty, t;

	if (r_fogtexture)
		return;

	//
	// fog texture
	//
	for ( y = 0, ty = 0.0f; y < FOG_TEXTURE_HEIGHT; y++, ty += th )
	{
		for ( x = 0, tx = 0.0f; x < FOG_TEXTURE_WIDTH; x++, tx += tw )
		{
			t = (float)(sqrt( tx ) * 255.0);
			data[x+y*FOG_TEXTURE_WIDTH] = (qbyte)(min( t, 255.0f ));
		}
	}

	r_fogtexture = texture_extension_number++;
	GL_Bind(r_fogtexture);
	qglTexImage2D (GL_TEXTURE_2D, 0, GL_ALPHA, FOG_TEXTURE_WIDTH, FOG_TEXTURE_HEIGHT, 0, GL_ALPHA, GL_UNSIGNED_BYTE, data);

	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

	qglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}
#endif
/*
===============
Draw_Init
===============
*/

void GLDraw_ReInit (void)
{
	int		i;
	qpic_t	*cb;
	qbyte	*dest;
	int		x;
	char	ver[40];
	glpic_t	*gl;
	qpic_t	*bigfont;
	int start;
	qbyte    *ncdata;
	qbyte *tinyfont;
	extern int		solidskytexture;
	extern int		alphaskytexture;
	extern int skyboxtex[6];
	extern int	*lightmap_textures;

	int maxtexsize;

	gltexture_t *glt;

	TRACE(("dbg: GLDraw_ReInit: Closing old\n"));
	while(gltextures)
	{
		glt = gltextures;
		gltextures = gltextures->next;
		BZ_Free(glt);
	}

	memset(gltexturetablebuckets, 0, sizeof(gltexturetablebuckets));
	Hash_InitTable(&gltexturetable, sizeof(gltexturetablebuckets)/sizeof(gltexturetablebuckets[0]), gltexturetablebuckets);


	texture_extension_number=1;
	solidskytexture=0;
	alphaskytexture=0;
	skyboxtex[0] = 0; skyboxtex[1] = 0; skyboxtex[2] = 0; skyboxtex[3] = 0; skyboxtex[4] = 0; skyboxtex[5] = 0;
	lightmap_textures=NULL;
	filmtexture=0;
	glmenu_numcachepics=0;
#ifdef Q3SHADERS
	r_fogtexture=0;
#endif
	GL_FlushBackEnd();
//	GL_FlushSkinCache();
	TRACE(("dbg: GLDraw_ReInit: GL_GAliasFlushSkinCache\n"));
	GL_GAliasFlushSkinCache();

	memset(scrap_allocated, 0, sizeof(scrap_allocated));
	memset(scrap_texels, 255, sizeof(scrap_texels));


	qglGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxtexsize);
	if (gl_max_size.value > maxtexsize)
	{
		sprintf(ver, "%i", maxtexsize);
		Cvar_ForceSet (&gl_max_size, ver);
	}

	if (maxtexsize < 2048)	//this needs to be able to hold the image in unscaled form.
		sizeofuploadmemorybufferintermediate = 2048*2048*4;	//make sure we can load 2048*2048 images whatever happens.
	else
		sizeofuploadmemorybufferintermediate = maxtexsize*maxtexsize*4;	//gl supports huge images, so so shall we.

	//required to hold the image after scaling has occured
	sizeofuploadmemorybuffer = maxtexsize*maxtexsize*4;
TRACE(("dbg: GLDraw_ReInit: Allocating upload buffers\n"));
	uploadmemorybuffer = BZ_Realloc(uploadmemorybuffer, sizeofuploadmemorybuffer);
	uploadmemorybufferintermediate = BZ_Realloc(uploadmemorybufferintermediate, sizeofuploadmemorybufferintermediate);

	// load the console background and the charset
	// by hand, because we need to write the version
	// string into the background before turning
	// it into a texture
	draw_chars = W_SafeGetLumpName ("conchars");
	if (draw_chars)
	{
		// add ocrana leds
		if (con_ocranaleds.value)
		{
			if (con_ocranaleds.value != 2 || QCRC_Block(draw_chars, 128*128) == 798)
				AddOcranaLEDsIndexed (draw_chars, 128, 128);
		}

		for (i=0 ; i<128*128 ; i++)
			if (draw_chars[i] == 0)
				draw_chars[i] = 255;	// proper transparent color
	}

	// now turn them into textures
	image_width = 0;
	image_height = 0;
	TRACE(("dbg: GLDraw_ReInit: looking for conchars\n"));
	if (!(char_texture=Mod_LoadReplacementTexture("gfx/conchars.lmp", NULL, false, true, false))) //no high res
	{
		if (!draw_chars)	//or low res.
		{
			if (!(char_texture=Mod_LoadHiResTexture("pics/conchars.pcx", NULL, false, true, false)))	//try low res q2 path
			if (!(char_texture=Mod_LoadHiResTexture("gfx/2d/bigchars.tga", NULL, false, true, false)))	//try q3 path
			{

				//gulp... so it's come to this has it? rework the hexen2 conchars into the q1 system.
				char *tempchars = COM_LoadMallocFile("gfx/menu/conchars.lmp");
				char *in, *out;
				if (tempchars)
				{
					draw_chars = BZ_Malloc(8*8*256*8);

					out = draw_chars;
					for (i = 0; i < 8*8; i+=1)
					{
						if ((i/8)&1)
						{
							in = tempchars + ((i)/8)*16*8*8+(i&7)*32*8 - 256*4+128;
							for (x = 0; x < 16*8; x++)
								*out++ = *in++;
						}
						else
						{
							in = tempchars + (i/8)*16*8*8+(i&7)*32*8;
							for (x = 0; x < 16*8; x++)
								*out++ = *in++;
						}
					}
					for (i = 0; i < 8*8; i+=1)
					{
						if ((i/8)&1)
						{
							in = tempchars+128*128 + ((i)/8)*16*8*8+(i&7)*32*8 - 256*4+128;
							for (x = 0; x < 16*8; x++)
								*out++ = *in++;
						}
						else
						{
							in = tempchars+128*128 + (i/8)*16*8*8+(i&7)*32*8;
							for (x = 0; x < 16*8; x++)
								*out++ = *in++;
						}
					}
					Z_Free(tempchars);

					// add ocrana leds
					if (con_ocranaleds.value && con_ocranaleds.value != 2)
						AddOcranaLEDsIndexed (draw_chars, 128, 128);

					for (i=0 ; i<128*128 ; i++)
						if (draw_chars[i] == 0)
							draw_chars[i] = 255;	// proper transparent color
					char_texture = GL_LoadTexture ("charset", 128, 128, draw_chars, false, true);
					Z_Free(draw_chars);
					draw_chars = NULL;
				}
				else
				{
					extern qbyte default_conchar[11356];
					int width, height;
					int i;
					qbyte *image;

					image = ReadTargaFile(default_conchar, sizeof(default_conchar), &width, &height, false);
					for (i = 0; i < width*height; i++)
					{
						image[i*4+3] = image[i*4];
						image[i*4+0] = 255;
						image[i*4+1] = 255;
						image[i*4+2] = 255;
					}
					char_texture = GL_LoadTexture32("charset", width, height, (void*)image, false, true);
				}
			}
		}
		else
			char_texture = GL_LoadTexture ("charset", 128, 128, draw_chars, false, true);
	}
	default_char_texture=char_texture;
	//half a pixel
	if (image_width)
		custom_char_instep = default_char_instep = 0.5f/((image_width+image_height)/2);	//you're an idiot if you use non-square conchars
	else
		custom_char_instep = default_char_instep = 0.5f/(128);

	TRACE(("dbg: GLDraw_ReInit: loaded charset\n"));

	TRACE(("dbg: GLDraw_ReInit: GL_BeginRendering\n"));
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	TRACE(("dbg: GLDraw_ReInit: SCR_DrawLoading\n"));

	GL_Set2D();

	qglClear(GL_COLOR_BUFFER_BIT);
	{
		mpic_t *pic = Draw_SafeCachePic ("gfx/loading.lmp");
		if (pic)
			Draw_Pic ( ((int)vid.width - pic->width)/2,
				((int)vid.height - 48 - pic->height)/2, pic);
	}

	TRACE(("dbg: GLDraw_ReInit: GL_EndRendering\n"));
	GL_EndRendering ();
	GL_DoSwap();


#ifdef Q3SHADERS
	Shader_Init();
#endif

	//now emit the conchars picture as if from a wad.
	strcpy(glmenu_cachepics[glmenu_numcachepics].name, "conchars");
	glmenu_cachepics[glmenu_numcachepics].pic.width = 128;
	glmenu_cachepics[glmenu_numcachepics].pic.height = 128;
	gl = (glpic_t *)&glmenu_cachepics[glmenu_numcachepics].pic.data;
	gl->texnum = char_texture;
	gl->sl = 0;
	gl->tl = 0;
	gl->sh = 1;
	gl->th = 1;
	glmenu_numcachepics++;

	TRACE(("dbg: GLDraw_ReInit: W_SafeGetLumpName\n"));
	tinyfont = W_SafeGetLumpName ("tinyfont");
	if (tinyfont)
	{
		for (i=0 ; i<128*32 ; i++)
			if (tinyfont[i] == 0)
				tinyfont[i] = 255;	// proper transparent color
		strcpy(glmenu_cachepics[glmenu_numcachepics].name, "tinyfont");
		glmenu_cachepics[glmenu_numcachepics].pic.width = 128;
		glmenu_cachepics[glmenu_numcachepics].pic.height = 32;
		gl = (glpic_t *)&glmenu_cachepics[glmenu_numcachepics].pic.data;
		gl->texnum = GL_LoadTexture ("tinyfont", 128, 32, tinyfont, false, true);
		gl->sl = 0;
		gl->tl = 0;
		gl->sh = 1;
		gl->th = 1;
		glmenu_numcachepics++;
	}
	TRACE(("dbg: GLDraw_ReInit: gfx/menu/bigfont\n"));
	bigfont = (qpic_t *)COM_LoadMallocFile ("gfx/menu/bigfont.lmp");
	if (bigfont)
	{
		char *data;
		data = bigfont->data;
		for (i=0 ; i<bigfont->width*bigfont->height ; i++)
			if (data[i] == 0)
				data[i] = 255;	// proper transparent color
		strcpy(glmenu_cachepics[glmenu_numcachepics].name, "gfx/menu/bigfont.lmp");
		glmenu_cachepics[glmenu_numcachepics].pic.width = bigfont->width;
		glmenu_cachepics[glmenu_numcachepics].pic.height = bigfont->height;
		gl = (glpic_t *)&glmenu_cachepics[glmenu_numcachepics].pic.data;
		gl->texnum = GL_LoadTexture ("gfx/menu/bigfont.lmp", bigfont->width, bigfont->height, data, false, true);
		gl->sl = 0;
		gl->tl = 0;
		gl->sh = 1;
		gl->th = 1;
		glmenu_numcachepics++;
	}


	TRACE(("dbg: GLDraw_ReInit: gfx/conchars2.lmp\n"));
	if (!(char_tex2=Mod_LoadReplacementTexture("gfx/conchars2.lmp", NULL, false, true, false)))
	{
		if (!draw_chars)
			char_tex2 = char_texture;
		else
			char_tex2 = GL_LoadTexture ("charset", 128, 128, draw_chars, false, true);
	}

	cs_texture = texture_extension_number++;

	missing_texture = GL_LoadTexture("no_texture", 16, 16, (unsigned char*)r_notexture_mip + r_notexture_mip->offsets[0], true, false);

	GL_SetupSceneProcessingTextures();

	start = Hunk_LowMark ();
	conback = default_conback;

	TRACE(("dbg: GLDraw_ReInit: COM_FDepthFile(\"gfx/conback.lmp\", false)\n"));
	if (COM_FDepthFile("gfx/conback.lmp", false) <= COM_FDepthFile("gfx/menu/conback.lmp", false))
		cb = (qpic_t *)COM_LoadHunkFile ("gfx/conback.lmp");
	else
		cb = (qpic_t *)COM_LoadHunkFile ("gfx/menu/conback.lmp");
	if (cb)
	{
		TRACE(("dbg: GLDraw_ReInit: conback opened\n"));
		SwapPic (cb);

		if (draw_chars)
		{
			sprintf (ver, "%i", build_number());
			dest = cb->data + 320 + 320*186 - 11 - 8*strlen(ver);
			for (x=0 ; x<strlen(ver) ; x++)
				GLDraw_CharToConback (ver[x], dest+(x<<3));
		}

#if 0
		conback->width = vid.conwidth;
		conback->height = vid.conheight;

		// scale console to vid size
		dest = ncdata = Hunk_AllocName(vid.conwidth * vid.conheight, "conback");

		TRACE(("dbg: GLDraw_ReInit: conback loading\n");
		for (y=0 ; y<vid.conheight ; y++, dest += vid.conwidth)
		{
			src = cb->data + cb->width * (y*cb->height/vid.conheight);
			if (vid.conwidth == cb->width)
				memcpy (dest, src, vid.conwidth);
			else
			{
				f = 0;
				fstep = cb->width*0x10000/vid.conwidth;
				for (x=0 ; x<vid.conwidth ; x+=4)
				{
					dest[x] = src[f>>16];
					f += fstep;
					dest[x+1] = src[f>>16];
					f += fstep;
					dest[x+2] = src[f>>16];
					f += fstep;
					dest[x+3] = src[f>>16];
					f += fstep;
				}
			}
		}
#else
		conback->width = cb->width;
		conback->height = cb->height;
		ncdata = cb->data;
#endif
	}
	else
	{
		ncdata = NULL;
	}

	TRACE(("dbg: GLDraw_ReInit: conback loaded\n"));
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	gl = (glpic_t *)conback->data;
	if (!(gl->texnum=Mod_LoadReplacementTexture("gfx/conback.lmp", NULL, false, true, false)))
	{
		if (!ncdata)	//no fallback
		{
			if (!(gl->texnum=Mod_LoadHiResTexture("pics/conback.pcx", NULL, false, true, false)))
				if (!(gl->texnum=Mod_LoadReplacementTexture("gfx/menu/conback.lmp", NULL, false, true, false)))
					if (!(gl->texnum=Mod_LoadReplacementTexture("textures/sfx/logo512.jpg", NULL, false, false, false)))
					{
						int data = 0;
						gl->texnum = GL_LoadTexture32("gfx/conback.lmp", 1, 1, (unsigned int *)&data, false, false);
					}
		}
		else
		{
			gl->texnum = GL_LoadTexture ("conback", conback->width, conback->height, ncdata, false, false);
		}
	}
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;
	conback->width = vid.conwidth;
	conback->height = vid.conheight;

	memcpy(custconback_buffer, conback_buffer, sizeof(custconback_buffer));

	custom_conback->width = vid.conwidth;
	custom_conback->height = vid.conheight;
	gl = (glpic_t *)custom_conback->data;
	gl->texnum = 0;
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;
	custom_conback->width = vid.conwidth;
	custom_conback->height = vid.conheight;

	// free loaded console
	Hunk_FreeToLowMark (start);

	// save a texture slot for translated picture
	translate_texture = texture_extension_number++;

	// save slots for scraps
	scrap_texnum = texture_extension_number;
	texture_extension_number += MAX_SCRAPS;

	//
	// get the other pics we need
	//
	TRACE(("dbg: GLDraw_ReInit: Draw_SafePicFromWad\n"));
	draw_disc = Draw_SafePicFromWad ("disc");
	draw_backtile = Draw_SafePicFromWad ("backtile");
	if (!draw_backtile)
		draw_backtile = Draw_SafeCachePic ("gfx/menu/backtile.lmp");

	detailtexture = Mod_LoadHiResTexture("textures/detail", NULL, true, false, false);

	inited15to8 = false;

	qglClearColor (1,0,0,0);

	TRACE(("dbg: GLDraw_ReInit: PPL_LoadSpecularFragmentProgram\n"));
	PPL_CreateShaderObjects();

#ifdef PLUGINS
	Plug_DrawReloadImages();
#endif
}

void GLDraw_Init (void)
{

	memset(scrap_allocated, 0, sizeof(scrap_allocated));
	memset(scrap_texels, 255, sizeof(scrap_texels));

	GLDraw_ReInit();

	R_BackendInit();



	draw_mesh.numindexes = 6;
	draw_mesh.indexes = r_quad_indexes;
	draw_mesh.trneighbors = NULL;

	draw_mesh.numvertexes = 4;
	draw_mesh.xyz_array = draw_mesh_xyz;
	draw_mesh.normals_array = NULL;
	draw_mesh.st_array = draw_mesh_st;
	draw_mesh.lmst_array = NULL;

}
void GLDraw_DeInit (void)
{
	Cmd_RemoveCommand ("gl_texture_anisotropic_filtering");

	draw_disc = NULL;

	if (uploadmemorybuffer)
		BZ_Free(uploadmemorybuffer);	//free the mem
	if (uploadmemorybufferintermediate)
		BZ_Free(uploadmemorybufferintermediate);
	uploadmemorybuffer = NULL;	//make sure we know it's free
	uploadmemorybufferintermediate = NULL;
	sizeofuploadmemorybuffer = 0;	//and give a nice safe sys_error if we try using it.
	sizeofuploadmemorybufferintermediate = 0;

#ifdef Q3SHADERS
	Shader_Shutdown();
#endif
}

void GL_DrawAliasMesh (mesh_t *mesh, int texnum);

void GL_DrawMesh(mesh_t *msh, int texturenum)
{
	GL_DrawAliasMesh(msh, texturenum);
}


/*
================
Draw_Character

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void GLDraw_Character (int x, int y, unsigned int num)
{
	int				row, col;
	float			frow, fcol, size;

	if (y <= -8)
		return;			// totally off screen

	num &= 255;

	if (num == 32)
		return;		// space

	num &= 255;

	row = num>>4;
	col = num&15;

	frow = row*0.0625+char_instep;
	fcol = col*0.0625+char_instep;
	size = 0.0625-char_instep*2;
	draw_mesh_xyz[0][0] = x;
	draw_mesh_xyz[0][1] = y;
	draw_mesh_st[0][0] = fcol;
	draw_mesh_st[0][1] = frow;

	draw_mesh_xyz[1][0] = x+8;
	draw_mesh_xyz[1][1] = y;
	draw_mesh_st[1][0] = fcol+size;
	draw_mesh_st[1][1] = frow;

	draw_mesh_xyz[2][0] = x+8;
	draw_mesh_xyz[2][1] = y+8;
	draw_mesh_st[2][0] = fcol+size;
	draw_mesh_st[2][1] = frow+size;

	draw_mesh_xyz[3][0] = x;
	draw_mesh_xyz[3][1] = y+8;
	draw_mesh_st[3][0] = fcol;
	draw_mesh_st[3][1] = frow+size;

	qglEnable(GL_BLEND);
	qglDisable(GL_ALPHA_TEST);

	if (num&CON_2NDCHARSETTEXT)
		GL_DrawMesh(&draw_mesh, char_tex2);
	else
		GL_DrawMesh(&draw_mesh, char_texture);
}

void GLDraw_FillRGB (int x, int y, int w, int h, float r, float g, float b);
void GLDraw_ColouredCharacter (int x, int y, unsigned int num)
{
	unsigned int col;

	// draw background
	if (num & CON_NONCLEARBG)
	{
		col = (num & CON_BGMASK) >> CON_BGSHIFT;
		GLDraw_FillRGB(x, y, 8, 8, consolecolours[col].fr, consolecolours[col].fg, consolecolours[col].fb);
	}

	if (num & CON_BLINKTEXT)
	{
		if (!cl_noblink.value)
			if ((int)(realtime*3) & 1)
				return;
	}

	// render character with foreground color
	col = (num & CON_FGMASK) >> CON_FGSHIFT;
	qglColor4f(consolecolours[col].fr, consolecolours[col].fg, consolecolours[col].fb, (num & CON_HALFALPHA)?0.5:1);
	Draw_Character(x, y, num);
}
/*
================
Draw_String
================
*/
void GLDraw_String (int x, int y, const qbyte *str)
{
	float xstart = x;
	while (*str)
	{
		if (*str == '\n')
		{
			x = xstart;
			y += 8;
			str++;
			continue;
		}
		Draw_Character (x, y, *str);
		str++;
		x += 8;
	}
}

/*
================
Draw_Alt_String
================
*/
void GLDraw_Alt_String (int x, int y, const qbyte *str)
{
	while (*str)
	{
		Draw_Character (x, y, (*str) | 0x80);
		str++;
		x += 8;
	}
}

#include "crosshairs.dat"
vec3_t chcolor;

void GLCrosshairimage_Callback(struct cvar_s *var, char *oldvalue)
{
	if (*(var->string))
		externalhair = Mod_LoadHiResTexture (var->string, "crosshairs", false, true, true);
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

	GL_Bind (cs_texture);
	GL_Upload32(NULL, cs_data, 16, 16, 0, true);

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

	float x1, x2, y1, y2;
	float size, chc;

	qboolean usingimage = false;

	if (crosshair.value == 1 && !*crosshairimage.string)
	{
		for (sc = 0; sc < cl.splitclients; sc++)
		{
			SCR_CrosshairPosition(sc, &x, &y);
			GLDraw_Character (x-4, y-4, '+');
		}
		return;
	}
	GL_TexEnv(GL_MODULATE);

	if (*crosshairimage.string)
	{
		usingimage = true;
		GL_Bind (externalhair);
		chc = 0;

		qglEnable (GL_BLEND);
		qglDisable(GL_ALPHA_TEST);
	}
	else if (crosshair.value)
	{
		GL_Bind (cs_texture);
		chc = 1/16.0;

		// force crosshair refresh with animated crosshairs
		if (crosshair.value >= FIRSTANIMATEDCROSHAIR)
			GLCrosshair_Callback(&crosshair, "");

		if (crosshairalpha.value<1)
		{
			qglEnable (GL_BLEND);
			qglDisable(GL_ALPHA_TEST);
		}
		else
		{
			qglDisable (GL_BLEND);
			qglEnable(GL_ALPHA_TEST);
		}
	}
	else
		return;

	if (usingimage)
		qglColor4f(chcolor[0], chcolor[1], chcolor[2], crosshairalpha.value);
	else
		qglColor4f(1, 1, 1, crosshairalpha.value);

	size = crosshairsize.value;
	chc = size * chc;

	if (gl_smoothcrosshair.value && (size > 16 || usingimage))
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	for (sc = 0; sc < cl.splitclients; sc++)
	{
		SCR_CrosshairPosition(sc, &x, &y);

		x1 = x - size - chc;
		x2 = x + size - chc;
		y1 = y - size - chc;
		y2 = y + size - chc;
		qglBegin (GL_QUADS);
		qglTexCoord2f (0, 0);
		qglVertex2f (x1, y1);
		qglTexCoord2f (1, 0);
		qglVertex2f (x2, y1);
		qglTexCoord2f (1, 1);
		qglVertex2f (x2, y2);
		qglTexCoord2f (0, 1);
		qglVertex2f (x1, y2);
		qglEnd ();
	}

//	GL_TexEnv ( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
//	GL_TexEnv ( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	qglColor4f(1, 1, 1, 1);
}


/*
================
Draw_DebugChar

Draws a single character directly to the upper right corner of the screen.
This is for debugging lockups by drawing different chars in different parts
of the code.
================
*/
void GLDraw_DebugChar (qbyte num)
{
}

/*
=============
Draw_Pic
=============
*/
void GLDraw_Pic (int x, int y, mpic_t *pic)
{
	glpic_t			*gl;

	if (!pic)
		return;

	if (scrap_dirty)
		Scrap_Upload ();
	gl = (glpic_t *)pic->data;

	draw_mesh_xyz[0][0] = x;
	draw_mesh_xyz[0][1] = y;
	draw_mesh_st[0][0] = gl->sl;
	draw_mesh_st[0][1] = gl->tl;

	draw_mesh_xyz[1][0] = x+pic->width;
	draw_mesh_xyz[1][1] = y;
	draw_mesh_st[1][0] = gl->sh;
	draw_mesh_st[1][1] = gl->tl;

	draw_mesh_xyz[2][0] = x+pic->width;
	draw_mesh_xyz[2][1] = y+pic->height;
	draw_mesh_st[2][0] = gl->sh;
	draw_mesh_st[2][1] = gl->th;

	draw_mesh_xyz[3][0] = x;
	draw_mesh_xyz[3][1] = y+pic->height;
	draw_mesh_st[3][0] = gl->sl;
	draw_mesh_st[3][1] = gl->th;

	if (gl_blend2d.value)
	{
		qglDisable(GL_ALPHA_TEST);
		qglEnable(GL_BLEND);
	}
	else
	{
		qglEnable(GL_ALPHA_TEST);
		qglDisable(GL_BLEND);
	}

	GL_DrawMesh(&draw_mesh, gl->texnum);
}

#ifdef Q3SHADERS
void GLDraw_ShaderPic (int x, int y, int width, int height, shader_t *pic, float r, float g, float b, float a)
{
	meshbuffer_t mb;

	if (!pic)
		return;

	R_IBrokeTheArrays();

	mb.entity = &r_worldentity;
	mb.shader = pic;
	mb.fog = NULL;
	mb.mesh = &draw_mesh;
	mb.infokey = 0;
	mb.dlightbits = 0;


	draw_mesh_xyz[0][0] = x;
	draw_mesh_xyz[0][1] = y;
	draw_mesh_st[0][0] = 0;
	draw_mesh_st[0][1] = 0;

	draw_mesh_xyz[1][0] = x+width;
	draw_mesh_xyz[1][1] = y;
	draw_mesh_st[1][0] = 1;
	draw_mesh_st[1][1] = 0;

	draw_mesh_xyz[2][0] = x+width;
	draw_mesh_xyz[2][1] = y+height;
	draw_mesh_st[2][0] = 1;
	draw_mesh_st[2][1] = 1;

	draw_mesh_xyz[3][0] = x;
	draw_mesh_xyz[3][1] = y+height;
	draw_mesh_st[3][0] = 0;
	draw_mesh_st[3][1] = 1;

	draw_mesh_colors[0][0] = r*255;
	draw_mesh_colors[0][1] = g*255;
	draw_mesh_colors[0][2] = b*255;
	draw_mesh_colors[0][3] = a*255;
	((int*)draw_mesh_colors)[1] = ((int*)draw_mesh_colors)[0];
	((int*)draw_mesh_colors)[2] = ((int*)draw_mesh_colors)[0];
	((int*)draw_mesh_colors)[3] = ((int*)draw_mesh_colors)[0];

	draw_mesh.colors_array = draw_mesh_colors;

	R_PushMesh(&draw_mesh, mb.shader->features | MF_COLORS | MF_NONBATCHED);
	R_RenderMeshBuffer ( &mb, false );
	draw_mesh.colors_array = NULL;

	qglEnable(GL_BLEND);
}

void GLDraw_ShaderImage (int x, int y, int w, int h, float s1, float t1, float s2, float t2, shader_t *pic)
{
	meshbuffer_t mb;

	if (!pic)
		return;

	R_IBrokeTheArrays();

	mb.entity = &r_worldentity;
	mb.shader = pic;
	mb.fog = NULL;
	mb.mesh = &draw_mesh;
	mb.infokey = -1;
	mb.dlightbits = 0;


	draw_mesh_xyz[0][0] = x;
	draw_mesh_xyz[0][1] = y;
	draw_mesh_st[0][0] = s1;
	draw_mesh_st[0][1] = t1;

	draw_mesh_xyz[1][0] = x+w;
	draw_mesh_xyz[1][1] = y;
	draw_mesh_st[1][0] = s2;
	draw_mesh_st[1][1] = t1;

	draw_mesh_xyz[2][0] = x+w;
	draw_mesh_xyz[2][1] = y+h;
	draw_mesh_st[2][0] = s2;
	draw_mesh_st[2][1] = t2;

	draw_mesh_xyz[3][0] = x;
	draw_mesh_xyz[3][1] = y+h;
	draw_mesh_st[3][0] = s1;
	draw_mesh_st[3][1] = t2;

/*	draw_mesh_colors[0][0] = r*255;
	draw_mesh_colors[0][1] = g*255;
	draw_mesh_colors[0][2] = b*255;
	draw_mesh_colors[0][3] = a*255;
	((int*)draw_mesh_colors)[1] = ((int*)draw_mesh_colors)[0];
	((int*)draw_mesh_colors)[2] = ((int*)draw_mesh_colors)[0];
	((int*)draw_mesh_colors)[3] = ((int*)draw_mesh_colors)[0];
*/
/*
	draw_mesh_colors[0][0] = 255;
	draw_mesh_colors[0][1] = 255;
	draw_mesh_colors[0][2] = 255;
	draw_mesh_colors[0][3] = 255;
*/
	draw_mesh.colors_array = draw_mesh_colors;

	draw_mesh.numvertexes = 4;
	draw_mesh.numindexes = 6;

	R_PushMesh(&draw_mesh, mb.shader->features | MF_COLORS | MF_NONBATCHED);
	R_RenderMeshBuffer ( &mb, false );
	draw_mesh.colors_array = NULL;
	qglEnable(GL_BLEND);
}
#endif

void GLDraw_ScalePic (int x, int y, int width, int height, mpic_t *pic)
{
	glpic_t			*gl;

	if (!pic)
		return;

	if (scrap_dirty)
		Scrap_Upload ();
	gl = (glpic_t *)pic->data;
//	qglColor4f (1,1,1,1);
	GL_Bind (gl->texnum);
	qglBegin (GL_QUADS);
	qglTexCoord2f (gl->sl, gl->tl);
	qglVertex2f (x, y);
	qglTexCoord2f (gl->sh, gl->tl);
	qglVertex2f (x+width, y);
	qglTexCoord2f (gl->sh, gl->th);
	qglVertex2f (x+width, y+height);
	qglTexCoord2f (gl->sl, gl->th);
	qglVertex2f (x, y+height);
	qglEnd ();
}

/*
=============
Draw_AlphaPic
=============
*/
void GLDraw_AlphaPic (int x, int y, mpic_t *pic, float alpha)
{
	glpic_t			*gl;

	if (scrap_dirty)
		Scrap_Upload ();
	gl = (glpic_t *)pic->data;
	qglDisable(GL_ALPHA_TEST);
	qglEnable (GL_BLEND);
//	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglCullFace(GL_FRONT);
	qglColor4f (1,1,1,alpha);
	GL_Bind (gl->texnum);
	qglBegin (GL_QUADS);
	qglTexCoord2f (gl->sl, gl->tl);
	qglVertex2f (x, y);
	qglTexCoord2f (gl->sh, gl->tl);
	qglVertex2f (x+pic->width, y);
	qglTexCoord2f (gl->sh, gl->th);
	qglVertex2f (x+pic->width, y+pic->height);
	qglTexCoord2f (gl->sl, gl->th);
	qglVertex2f (x, y+pic->height);
	qglEnd ();
	qglColor4f (1,1,1,1);
	qglEnable(GL_ALPHA_TEST);
	qglDisable (GL_BLEND);
}

void GLDraw_SubPic(int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height)
{
	glpic_t			*gl;
	float newsl, newtl, newsh, newth;
	float oldglwidth, oldglheight;

	if (scrap_dirty)
		Scrap_Upload ();
	gl = (glpic_t *)pic->data;

	oldglwidth = gl->sh - gl->sl;
	oldglheight = gl->th - gl->tl;

	newsl = gl->sl + (srcx*oldglwidth)/pic->width;
	newsh = newsl + (width*oldglwidth)/pic->width;

	newtl = gl->tl + (srcy*oldglheight)/pic->height;
	newth = newtl + (height*oldglheight)/pic->height;

	draw_mesh_xyz[0][0] = x;
	draw_mesh_xyz[0][1] = y;
	draw_mesh_st[0][0] = newsl;
	draw_mesh_st[0][1] = newtl;

	draw_mesh_xyz[1][0] = x+width;
	draw_mesh_xyz[1][1] = y;
	draw_mesh_st[1][0] = newsh;
	draw_mesh_st[1][1] = newtl;

	draw_mesh_xyz[2][0] = x+width;
	draw_mesh_xyz[2][1] = y+height;
	draw_mesh_st[2][0] = newsh;
	draw_mesh_st[2][1] = newth;

	draw_mesh_xyz[3][0] = x;
	draw_mesh_xyz[3][1] = y+height;
	draw_mesh_st[3][0] = newsl;
	draw_mesh_st[3][1] = newth;

	GL_DrawMesh(&draw_mesh, gl->texnum);
}

/*
=============
Draw_TransPic
=============
*/
void GLDraw_TransPic (int x, int y, mpic_t *pic)
{
	if (!pic)
		return;
	if (x < 0 || (unsigned)(x + pic->width) > vid.width || y < 0 ||
		 (unsigned)(y + pic->height) > vid.height)
	{
		Con_DPrintf("Draw_TransPic: bad coordinates\n");
		return;
//		Sys_Error ("Draw_TransPic: bad coordinates");
	}

	GLDraw_Pic (x, y, pic);
}


/*
=============
Draw_TransPicTranslate

Only used for the player color selection menu
=============
*/
void GLDraw_TransPicTranslate (int x, int y, int width, int height, qbyte *pic, qbyte *translation)
{
	int				v, u, c;
	unsigned		trans[64*64], *dest;
	qbyte			*src;
	int				p;

	GL_Bind (translate_texture);

	c = width * height;

	dest = trans;
	for (v=0 ; v<64 ; v++, dest += 64)
	{
		src = &pic[ ((v*height)>>6) *width];
		for (u=0 ; u<64 ; u++)
		{
			p = src[(u*width)>>6];
			if (p == 255)
				dest[u] = p;
			else
				dest[u] =  d_8to24rgbtable[translation[p]];
		}
	}

	qglTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	qglColor3f (1,1,1);
	qglBegin (GL_QUADS);
	qglTexCoord2f (0, 0);
	qglVertex2f (x, y);
	qglTexCoord2f (1, 0);
	qglVertex2f (x+width, y);
	qglTexCoord2f (1, 1);
	qglVertex2f (x+width, y+height);
	qglTexCoord2f (0, 1);
	qglVertex2f (x, y+height);
	qglEnd ();
}


/*
================
Draw_ConsoleBackground

================
*/
void GLDraw_ConsoleBackground (int lines)
{
//	char ver[80];
//	int x, i;
	float a;
	extern qboolean scr_con_forcedraw;

	conback->width = vid.conwidth;
	conback->height = vid.conheight;

	if (scr_con_forcedraw)
	{
		a = 1; // console background is necessary
	}
	else
	{
		if (!scr_conalpha.value)
			return; 

		a = scr_conalpha.value;
	}

	if (scr_chatmode == 2)
	{
		conback->height>>=1;
		conback->width>>=1;
	}
#ifdef Q3SHADERS
	{
		if (shader_console)
		{
			currententity = &r_worldentity;
			GLDraw_ShaderPic(0, lines - conback->height, vid.width, vid.height, shader_console, 1, 1, 1, a);
			qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			return;
		}
	}
#endif
	if (a >= 1)
	{
		qglColor3f (1,1,1);
		GLDraw_Pic(0, lines-conback->height, conback);
	}
	else
	{
		GLDraw_AlphaPic (0, lines - conback->height, conback, a);
	}
}

void GLDraw_EditorBackground (int lines)
{
	int y;

	y = (vid.height * 3) >> 2;
	if (lines > y)
		GLDraw_Pic(0, lines-vid.height, conback);
	else
		GLDraw_AlphaPic (0, lines - vid.height, conback, (float)(1.2 * lines)/y);
}

/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void GLDraw_TileClear (int x, int y, int w, int h)
{
	qglColor3f (1,1,1);
	if (!draw_backtile)
	{
		qglDisable(GL_TEXTURE_2D);
		qglBegin (GL_QUADS);
		qglTexCoord2f (x/64.0, y/64.0);
		qglVertex2f (x, y);
		qglTexCoord2f ( (x+w)/64.0, y/64.0);
		qglVertex2f (x+w, y);
		qglTexCoord2f ( (x+w)/64.0, (y+h)/64.0);
		qglVertex2f (x+w, y+h);
		qglTexCoord2f ( x/64.0, (y+h)/64.0 );
		qglVertex2f (x, y+h);
		qglEnd ();
		qglEnable(GL_TEXTURE_2D);
	}
	else
	{
		GL_Bind (*(int *)draw_backtile->data);
		qglBegin (GL_QUADS);
		qglTexCoord2f (x/64.0, y/64.0);
		qglVertex2f (x, y);
		qglTexCoord2f ( (x+w)/64.0, y/64.0);
		qglVertex2f (x+w, y);
		qglTexCoord2f ( (x+w)/64.0, (y+h)/64.0);
		qglVertex2f (x+w, y+h);
		qglTexCoord2f ( x/64.0, (y+h)/64.0 );
		qglVertex2f (x, y+h);
		qglEnd ();
	}
}

void GLDraw_FillRGB (int x, int y, int w, int h, float r, float g, float b)
{
	qglDisable (GL_TEXTURE_2D);
	qglColor3f (r, g, b);

	qglBegin (GL_QUADS);

	qglVertex2f (x,y);
	qglVertex2f (x+w, y);
	qglVertex2f (x+w, y+h);
	qglVertex2f (x, y+h);

	qglEnd ();
	qglColor3f (1,1,1);
	qglEnable (GL_TEXTURE_2D);
}

/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void GLDraw_Fill (int x, int y, int w, int h, int c)
{
	extern qboolean gammaworks;
	if (gammaworks)
	{
		GLDraw_FillRGB (x, y, w, h,
			host_basepal[c*3]/255.0,
			host_basepal[c*3+1]/255.0,
			host_basepal[c*3+2]/255.0);
	}
	else
	{
		GLDraw_FillRGB (x, y, w, h,
			gammatable[host_basepal[c*3]]/255.0,
			gammatable[host_basepal[c*3+1]]/255.0,
			gammatable[host_basepal[c*3+2]]/255.0);
	}
}
//=============================================================================

/*
================
Draw_FadeScreen

================
*/
vec3_t fadecolor;
int faderender;

void GLR_Menutint_Callback (struct cvar_s *var, char *oldvalue)
{
	// parse r_menutint and clear defaults
	faderender = GL_DST_COLOR;

	if (var->string[0])
		SCR_StringToRGB(var->string, fadecolor, 1);
	else
		faderender = 0;

	// bounds check and inverse check
	if (faderender)
	{
		if (fadecolor[0] < 0)
		{
			faderender = GL_ONE_MINUS_DST_COLOR;
			fadecolor[0] = -(fadecolor[0]);
		}
		if (fadecolor[1] < 0)
		{
			faderender = GL_ONE_MINUS_DST_COLOR;
			fadecolor[1] = -(fadecolor[1]);
		}
		if (fadecolor[2] < 0)
		{
			faderender = GL_ONE_MINUS_DST_COLOR;
			fadecolor[2] = -(fadecolor[2]);
		}
	}
}

void GLDraw_FadeScreen (void)
{
	extern cvar_t gl_menutint_shader;
	extern int scenepp_texture, scenepp_mt_program, scenepp_mt_parm_colorf, scenepp_mt_parm_inverti;

	if (!faderender)
		return;

	if (scenepp_mt_program && gl_menutint_shader.value)
	{
		float vwidth = 1, vheight = 1;
		float vs, vt;

		// get the powers of 2 for the size of the texture that will hold the scene
		while (vwidth < glwidth)
			vwidth *= 2;
		while (vheight < glheight)
			vheight *= 2;

		// get the maxtexcoords while we're at it (cache this or just use largest?)
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

		GL_Bind(scenepp_texture);
		qglCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, glx, gly, vwidth, vheight, 0);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		if (qglGetError())
			Con_Printf(S_ERROR "GL Error after qglCopyTexImage2D\n");

		GLSlang_UseProgram(scenepp_mt_program);
		qglUniform3fvARB(scenepp_mt_parm_colorf, 1, fadecolor);
		if (faderender == GL_ONE_MINUS_DST_COLOR)
			qglUniform1iARB(scenepp_mt_parm_inverti, 1);
		else
			qglUniform1iARB(scenepp_mt_parm_inverti, 0);

		if (qglGetError())
			Con_Printf(S_ERROR "GL Error after GLSlang_UseProgram\n");

		qglEnable(GL_TEXTURE_2D);
		GL_Bind(scenepp_texture);

		qglBegin(GL_QUADS);

		qglTexCoord2f (0, 0);
		qglVertex2f(0, 0);
		qglTexCoord2f (vs, 0);
		qglVertex2f(glwidth, 0);
		qglTexCoord2f (vs, vt);
		qglVertex2f(glwidth, glheight);
		qglTexCoord2f (0, vt);
		qglVertex2f(0, glheight);
	
		qglEnd();

		GLSlang_UseProgram(0);

		// After all the post processing, pop the matrices
		qglMatrixMode(GL_PROJECTION);
		qglPopMatrix();
		qglMatrixMode(GL_MODELVIEW);
		qglPopMatrix();

		if (qglGetError())
			Con_Printf(S_ERROR "GL Error after drawing with shaderobjects\n");
	}
	else
	{
		// shaderless way
		qglEnable (GL_BLEND);
		qglBlendFunc(faderender, GL_ZERO);
		qglDisable(GL_ALPHA_TEST);
		qglDisable (GL_TEXTURE_2D);
		qglColor4f (fadecolor[0], fadecolor[1], fadecolor[2], 1);
		qglBegin (GL_QUADS);

		qglVertex2f (0,0);
		qglVertex2f (vid.width, 0);
		qglVertex2f (vid.width, vid.height);
		qglVertex2f (0, vid.height);

		qglEnd ();
		qglColor4f (1,1,1,1);
		qglEnable (GL_TEXTURE_2D);
		qglDisable (GL_BLEND);
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		qglEnable(GL_ALPHA_TEST);
	}

	Sbar_Changed();
}

void GLDraw_ImageColours(float r, float g, float b, float a)
{
	draw_mesh_colors[0][0] = r*255;
	draw_mesh_colors[0][1] = g*255;
	draw_mesh_colors[0][2] = b*255;
	draw_mesh_colors[0][3] = a*255;
	((int*)draw_mesh_colors)[1] = ((int*)draw_mesh_colors)[0];
	((int*)draw_mesh_colors)[2] = ((int*)draw_mesh_colors)[0];
	((int*)draw_mesh_colors)[3] = ((int*)draw_mesh_colors)[0];

	qglColor4f(r, g, b, a);
}

void GLDraw_Image(float x, float y, float w, float h, float s1, float t1, float s2, float t2, mpic_t *pic)
{
	glpic_t			*gl;

	if (!pic)
		return;

	if (w == 0 && h == 0)
	{
		w = 64;
		h = 64;
	}

	if (scrap_dirty)
		Scrap_Upload ();
	gl = (glpic_t *)pic->data;
/*
	s2 = s2

	newsl = gl->sl + (srcx*oldglwidth)/pic->width;
	newsh = newsl + (width*oldglwidth)/pic->width;

	newtl = gl->tl + (srcy*oldglheight)/pic->height;
	newth = newtl + (height*oldglheight)/pic->height;
*/
	s2 = s1 + (s2-s1)*gl->sh;
	s1 += gl->sl;
	t2 = t1 + (t2-t1)*gl->th;
	t1 += gl->tl;

	draw_mesh_xyz[0][0] = x;
	draw_mesh_xyz[0][1] = y;
	draw_mesh_st[0][0] = s1;
	draw_mesh_st[0][1] = t1;

	draw_mesh_xyz[1][0] = x+w;
	draw_mesh_xyz[1][1] = y;
	draw_mesh_st[1][0] = s2;
	draw_mesh_st[1][1] = t1;

	draw_mesh_xyz[2][0] = x+w;
	draw_mesh_xyz[2][1] = y+h;
	draw_mesh_st[2][0] = s2;
	draw_mesh_st[2][1] = t2;

	draw_mesh_xyz[3][0] = x;
	draw_mesh_xyz[3][1] = y+h;
	draw_mesh_st[3][0] = s1;
	draw_mesh_st[3][1] = t2;

	if (gl_blend2d.value)
	{
		qglDisable(GL_ALPHA_TEST);
		qglEnable(GL_BLEND);
	}
	else
	{
		qglEnable(GL_ALPHA_TEST);
		qglDisable(GL_BLEND);
	}


	GL_DrawMesh(&draw_mesh, gl->texnum);
}

//=============================================================================

/*
================
Draw_BeginDisc

Draws the little blue disc in the corner of the screen.
Call before beginning any disc IO.
================
*/
void GLDraw_BeginDisc (void)
{
	if (!draw_disc || !r_drawdisk.value)
		return;
	qglDrawBuffer  (GL_FRONT);
	Draw_Pic (vid.width - draw_disc->width, 0, draw_disc);
	qglDrawBuffer  (GL_BACK);
}


/*
================
Draw_EndDisc

Erases the disc icon.
Call after completing any disc IO
================
*/
void GLDraw_EndDisc (void)
{
}

// conback/font callbacks
void GL_Smoothfont_Callback(struct cvar_s *var, char *oldvalue)
{
	GL_Bind(char_texture);
	if (var->value)
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
}

void GL_Fontinwardstep_Callback(struct cvar_s *var, char *oldvalue)
{
	if (var->value)
		char_instep = custom_char_instep*bound(0, var->value, 1);
	else
		char_instep = 0;
}

void GL_Font_Callback(struct cvar_s *var, char *oldvalue)
{
	mpic_t *pic;
	int old_char_texture = char_texture;

	if (!*var->string
		|| (!(char_texture=Mod_LoadHiResTexture(var->string, "fonts", false, true, true))
		&& !(char_texture=Mod_LoadHiResTexture(var->string, "charsets", false, true, true))))
	{
		char_texture = default_char_texture;
		custom_char_instep = default_char_instep;
	}
	else
		custom_char_instep = 0.5f/((image_width+image_height)/2);

	// update the conchars texture within the menu cache
	if (old_char_texture != char_texture)
	{
		pic = GLDraw_IsCached("conchars");
		if (pic)
		{
			glpic_t *gl = (glpic_t *)pic->data;
			gl->texnum = char_texture;
		}
		else
			Con_Printf(S_ERROR "ERROR: Unable to update conchars texture!");
	}
	
	GL_Smoothfont_Callback(&gl_smoothfont, "");
	GL_Fontinwardstep_Callback(&gl_fontinwardstep, "");
}

void GL_Conback_Callback(struct cvar_s *var, char *oldvalue)
{
	int newtex = 0;
#ifdef Q3SHADERS
	if (*var->string && (shader_console = R_RegisterCustom(var->string, NULL)))
	{
		conback = default_conback;
	}
	else
#endif
	if (!*var->string || !(newtex=Mod_LoadHiResTexture(var->string, "gfx", false, true, true)))
	{
		conback = default_conback;
	}
	else
	{
		conback = custom_conback;
		((glpic_t *)conback->data)->texnum = newtex;
	}
}

/*
================
GL_Set2D

Setup as if the screen was 320*200
================
*/
void GL_Set2D (void)
{
	GL_SetShaderState2D(true);

	qglViewport (glx, gly, glwidth, glheight);

	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity ();
	qglOrtho  (0, vid.width, vid.height, 0, -99999, 99999);

	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity ();

	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_CULL_FACE);

	if (gl_blend2d.value)
	{
		qglEnable (GL_BLEND);
		qglDisable (GL_ALPHA_TEST);
	}
	else
	{
		qglDisable (GL_BLEND);
		qglEnable (GL_ALPHA_TEST);
	}
//	qglDisable (GL_ALPHA_TEST);

	qglColor4f (1,1,1,1);

	r_refdef.time = realtime;
}






void MediaGL_ShowFrame8bit(qbyte *framedata, int inwidth, int inheight, qbyte *palette)	//bottom up
{
	if (!filmtexture)
	{
		filmtexture=texture_extension_number;
		texture_extension_number++;
	}

	GL_Set2D ();

	GL_Bind(filmtexture);
	GL_Upload8Pal24(framedata, palette, inwidth, inheight, false, false);	//we may need to rescale the image
//		glTexImage2D (GL_TEXTURE_2D, 0, 3, roqfilm->width, roqfilm->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, framedata);
//		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
//		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

	qglDisable(GL_BLEND);
	qglDisable(GL_ALPHA_TEST);
	qglBegin(GL_QUADS);
	qglTexCoord2f(0, 0);
	qglVertex2f(0, 0);
	qglTexCoord2f(0, 1);
	qglVertex2f(0, vid.height);
	qglTexCoord2f(1, 1);
	qglVertex2f(vid.width, vid.height);
	qglTexCoord2f(1, 0);
	qglVertex2f(vid.width, 0);
	qglEnd();
	qglEnable(GL_ALPHA_TEST);


	SCR_SetUpToDrawConsole();
	if  (scr_con_current)
	SCR_DrawConsole (false);

	M_Draw(0);
}

void MediaGL_ShowFrameRGBA_32(qbyte *framedata, int inwidth, int inheight)//top down
{
	if (!filmtexture)
	{
		filmtexture=texture_extension_number;
		texture_extension_number++;
	}

	GL_Set2D ();

	GL_Bind(filmtexture);
	GL_Upload32("", (unsigned *)framedata, inwidth, inheight, false, false);	//we may need to rescale the image

	qglDisable(GL_BLEND);
	qglDisable(GL_ALPHA_TEST);
	qglBegin(GL_QUADS);
	qglTexCoord2f(0, 0);
	qglVertex2f(0, 0);
	qglTexCoord2f(0, 1);
	qglVertex2f(0, vid.height);
	qglTexCoord2f(1, 1);
	qglVertex2f(vid.width, vid.height);
	qglTexCoord2f(1, 0);
	qglVertex2f(vid.width, 0);
	qglEnd();
	qglEnable(GL_ALPHA_TEST);


	SCR_SetUpToDrawConsole();
	if  (scr_con_current)
	SCR_DrawConsole (false);
}

int filmnwidth = 640;
int filmnheight = 640;

void MediaGL_ShowFrameBGR_24_Flip(qbyte *framedata, int inwidth, int inheight)
{
	//we need these as we resize it as we convert to rgba

	int y, x;

	int v;
	unsigned int f, fstep;
	qbyte *src, *dest;
	dest = uploadmemorybufferintermediate;
	//change from bgr bottomup to rgba topdown

	for (filmnwidth = 1; filmnwidth < inwidth; filmnwidth*=2)
		;
	for (filmnheight = 1; filmnheight < inheight; filmnheight*=2)
		;

	if (filmnwidth > 512)
		filmnwidth = 512;
	if (filmnheight > 512)
		filmnheight = 512;

	if (inwidth*inheight > sizeofuploadmemorybufferintermediate/4)
		Sys_Error("MediaGL_ShowFrameBGR_24_Flip: image too big (%i*%i)", inwidth, inheight);

	for (y=1 ; y<=filmnheight ; y++)
	{
		v = ((filmnheight - y)*(float)inheight/filmnheight);
		src = framedata + v*(inwidth*3);
		{
			f = 0;
			fstep = ((inwidth)*0x10000)/filmnwidth;

			for (x=filmnwidth ; x&3 ; x--)	//do the odd ones first. (bigger condition)
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

	if (!filmtexture)
	{
		filmtexture=texture_extension_number;
		texture_extension_number++;
	}

	GL_Set2D ();

	GL_Bind(filmtexture);
	GL_Upload32("", (unsigned *)uploadmemorybufferintermediate, filmnwidth, filmnheight, false, false);	//we may need to rescale the image

	qglDisable(GL_BLEND);
	qglDisable(GL_ALPHA_TEST);
	qglBegin(GL_QUADS);
	qglTexCoord2f(0, 0);
	qglVertex2f(0, 0);
	qglTexCoord2f(0, 1);
	qglVertex2f(0, vid.height);
	qglTexCoord2f(1, 1);
	qglVertex2f(vid.width, vid.height);
	qglTexCoord2f(1, 0);
	qglVertex2f(vid.width, 0);
	qglEnd();
	qglEnable(GL_ALPHA_TEST);


	SCR_SetUpToDrawConsole();
	if  (scr_con_current)
	SCR_DrawConsole (false);
}



//====================================================================

/*
================
GL_FindTexture
================
*/
int GL_FindTexture (char *identifier)
{
	gltexture_t	*glt;

	glt = Hash_Get(&gltexturetable, identifier);
	if (glt)
		return glt->texnum;
/*
	for (glt=gltextures ; glt ; glt=glt->next)
	{
		if (!strcmp (identifier, glt->identifier))
			return glt->texnum;
	}
*/

	return -1;
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
void GL_ResampleTexture (unsigned *in, int inwidth, int inheight, unsigned *out,  int outwidth, int outheight)
{
	int		i, j;
	unsigned	*inrow;
	unsigned	frac, fracstep;

	if (gl_lerpimages.value)
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
void GL_Resample8BitTexture (unsigned char *in, int inwidth, int inheight, unsigned char *out,  int outwidth, int outheight)
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
void GL_MipMap (qbyte *in, int width, int height)
{
	int		i, j;
	qbyte	*out;

	width <<=2;
	height >>= 1;
	out = in;
	for (i=0 ; i<height ; i++, in+=width)
	{
		for (j=0 ; j<width ; j+=8, out+=4, in+=8)
		{
			out[0] = (in[0] + in[4] + in[width+0] + in[width+4])>>2;
			out[1] = (in[1] + in[5] + in[width+1] + in[width+5])>>2;
			out[2] = (in[2] + in[6] + in[width+2] + in[width+6])>>2;
			out[3] = (in[3] + in[7] + in[width+3] + in[width+7])>>2;
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
void GL_MipMap8Bit (qbyte *in, int width, int height)
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

qboolean GL_UploadCompressed (qbyte *file, int *out_width, int *out_height, unsigned int *out_mipmap)
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
	GETVAR(out_mipmap)
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

	if (*out_mipmap)
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max_2d);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max_2d);
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
void GL_Upload32 (char *name, unsigned *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	int		miplevel=0;
	int			samples;
	unsigned	*scaled = (unsigned *)uploadmemorybuffer;
	int			scaled_width, scaled_height;

	TRACE(("dbg: GL_Upload32: %s %i %i\n", name, width, height));

	scaled_width = width;
	scaled_height = height;
	GL_RoundDimensions(&scaled_width, &scaled_height, mipmap);

	TRACE(("dbg: GL_Upload32: %i %i\n", scaled_width, scaled_height));

	if (scaled_width * scaled_height > sizeofuploadmemorybuffer/4)
		Sys_Error ("GL_LoadTexture: too big");

	samples = alpha ? gl_alpha_format : gl_solid_format;
	if (gl_config.arb_texture_compression && gl_compress.value && name&&mipmap)
		samples = alpha ? GL_COMPRESSED_RGBA_ARB : GL_COMPRESSED_RGB_ARB;

texels += scaled_width * scaled_height;

	if (gl_config.sgis_generate_mipmap&&mipmap)
	{
		TRACE(("dbg: GL_Upload32: GL_SGIS_generate_mipmap\n"));
		qglTexParameterf(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
	}

	if (scaled_width == width && scaled_height == height)
	{
		if (!mipmap||gl_config.sgis_generate_mipmap)	//gotta love this with NPOT textures... :)
		{
			TRACE(("dbg: GL_Upload32: non-mipmapped/unscaled\n"));
			qglTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			goto done;
		}
		memcpy (scaled, data, width*height*4);
	}
	else
		GL_ResampleTexture (data, width, height, scaled, scaled_width, scaled_height);

	TRACE(("dbg: GL_Upload32: recaled\n"));
	qglTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	if (mipmap && !gl_config.sgis_generate_mipmap)
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
			qglTexImage2D (GL_TEXTURE_2D, miplevel, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
		}
	}
	if (gl_config.arb_texture_compression && gl_compress.value && gl_savecompressedtex.value && name&&mipmap)
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
				i = LittleLong(mipmap);
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
	if (gl_config.sgis_generate_mipmap&&mipmap)
		qglTexParameterf(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_FALSE);

	if (gl_anisotropy_factor)
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_anisotropy_factor); // without this, you could loose anisotropy on mapchange

	if (mipmap)
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max_2d);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max_2d);
	}
}

void GL_Upload24BGR (char *name, qbyte *framedata, int inwidth, int inheight,  qboolean mipmap, qboolean alpha)
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

	GL_Upload32 (name, (unsigned int*)uploadmemorybufferintermediate, outwidth, outheight, mipmap, alpha);
}
void GL_Upload24BGR_Flip (char *name, qbyte *framedata, int inwidth, int inheight,  qboolean mipmap, qboolean alpha)
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

	GL_Upload32 (name, (unsigned int*)uploadmemorybufferintermediate, outwidth, outheight, mipmap, alpha);
}


void GL_Upload8Grey (unsigned char*data, int width, int height,  qboolean mipmap)
{
	int			samples;
	unsigned char	*scaled = uploadmemorybuffer;
	int			scaled_width, scaled_height;

	scaled_width = width;
	scaled_height = height;
	GL_RoundDimensions(&scaled_width, &scaled_height, mipmap);

	if (scaled_width * scaled_height > sizeofuploadmemorybuffer/4)
		Sys_Error ("GL_LoadTexture: too big");

	samples = 1;//alpha ? gl_alpha_format : gl_solid_format;

texels += scaled_width * scaled_height;

	if (scaled_width == width && scaled_height == height)
	{
		if (!mipmap)
		{
			qglTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
			goto done;
		}
		memcpy (scaled, data, width*height);
	}
	else
		GL_Resample8BitTexture (data, width, height, scaled, scaled_width, scaled_height);

	qglTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, scaled);
	if (mipmap)
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

	if (mipmap)
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max_2d);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max_2d);
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
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max_2d);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max_2d);
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

	texels += scaled_width * scaled_height;

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

void GL_Upload8 (char *name, qbyte *data, int width, int height,  qboolean mipmap, qboolean alpha)
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
	if (alpha)
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
	}
	else
	{
		for (i=(s&~3)-4 ; i>=0 ; i-=4)
		{
			trans[i] = d_8to24rgbtable[data[i]];
			trans[i+1] = d_8to24rgbtable[data[i+1]];
			trans[i+2] = d_8to24rgbtable[data[i+2]];
			trans[i+3] = d_8to24rgbtable[data[i+3]];
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

	GL_Upload32 (name, trans, width, height, mipmap, alpha);
}

void GL_Upload8FB (qbyte *data, int width, int height,  qboolean mipmap)
{
	unsigned	*trans = (unsigned *)uploadmemorybufferintermediate;
	int			i, s;
	qboolean	noalpha;
	int			p;

	s = width*height;
	if (s > sizeofuploadmemorybufferintermediate/4)
		Sys_Error("GL_Upload8FB: image too big (%i*%i)", width, height);
	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	noalpha = true;
	for (i=0 ; i<s ; i++)
	{
		p = data[i];
		if (p <= 255-vid.fullbright)
			trans[i] = 0;
		else
			trans[i] = d_8to24rgbtable[p];
	}

	GL_Upload32 (NULL, trans, width, height, mipmap, true);
}

void GL_Upload8Pal24 (qbyte *data, qbyte *pal, int width, int height,  qboolean mipmap, qboolean alpha)
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
		if (alpha)
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

			if (alpha && noalpha)
				alpha = false;
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
		if (alpha)
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

			if (alpha && noalpha)
				alpha = false;
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
	GL_Upload32 (NULL, (unsigned*)trans, width, height, mipmap, alpha);
}
void GL_Upload8Pal32 (qbyte *data, qbyte *pal, int width, int height,  qboolean mipmap, qboolean alpha)
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

	GL_Upload32 (NULL, (unsigned*)trans, width, height, mipmap, true);
}
/*
================
GL_LoadTexture
================
*/
int GL_LoadTexture (char *identifier, int width, int height, qbyte *data, qboolean mipmap, qboolean alpha)
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
	glt->texnum = texture_extension_number;
	glt->width = width;
	glt->height = height;
	glt->bpp = 8;
	glt->mipmap = mipmap;

	Hash_Add(&gltexturetable, glt->identifier, glt, (bucket_t*)(glt+1));

	GL_Bind(texture_extension_number );

	GL_Upload8 ("8bit", data, width, height, mipmap, alpha);

	texture_extension_number++;

	return texture_extension_number-1;
}

int GL_LoadTextureFB (char *identifier, int width, int height, qbyte *data, qboolean mipmap, qboolean alpha)
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
		return 0;	//none found, don't bother uploading.

	glt = BZ_Malloc(sizeof(*glt)+sizeof(bucket_t));
	glt->next = gltextures;
	gltextures = glt;

	strcpy (glt->identifier, identifier);
	glt->texnum = texture_extension_number;
	glt->width = width;
	glt->height = height;
	glt->bpp = 8;
	glt->mipmap = mipmap;

	Hash_Add(&gltexturetable, glt->identifier, glt, (bucket_t*)(glt+1));

	GL_Bind(texture_extension_number );

	GL_Upload8FB (data, width, height, mipmap);

	texture_extension_number++;

	return texture_extension_number-1;
}

int GL_LoadTexture8Pal24 (char *identifier, int width, int height, qbyte *data, qbyte *palette24, qboolean mipmap, qboolean alpha)
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
	glt->texnum = texture_extension_number;
	glt->width = width;
	glt->height = height;
	glt->bpp = 24;
	glt->mipmap = mipmap;

	Hash_Add(&gltexturetable, glt->identifier, glt, (bucket_t*)(glt+1));

	GL_Bind(texture_extension_number );

	GL_Upload8Pal24 (data, palette24, width, height, mipmap, alpha);

	texture_extension_number++;

	return texture_extension_number-1;
}
int GL_LoadTexture8Pal32 (char *identifier, int width, int height, qbyte *data, qbyte *palette32, qboolean mipmap, qboolean alpha)
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
	glt->texnum = texture_extension_number;
	glt->width = width;
	glt->height = height;
	glt->bpp = 32;
	glt->mipmap = mipmap;

	Hash_Add(&gltexturetable, glt->identifier, glt, (bucket_t*)(glt+1));

	GL_Bind(texture_extension_number );

	GL_Upload8Pal32 (data, palette32, width, height, mipmap, alpha);

	texture_extension_number++;

	return texture_extension_number-1;
}

int GL_LoadTexture32 (char *identifier, int width, int height, unsigned *data, qboolean mipmap, qboolean alpha)
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
	glt->texnum = texture_extension_number;
	glt->width = width;
	glt->height = height;
	glt->bpp = 32;
	glt->mipmap = mipmap;

	Hash_Add(&gltexturetable, glt->identifier, glt, (bucket_t*)(glt+1));

//	if (!isDedicated)
	{
		GL_Bind(texture_extension_number );

		GL_Upload32 (identifier, data, width, height, mipmap, alpha);
	}

	texture_extension_number++;

	return texture_extension_number-1;
}

int GL_LoadCompressed(char *name)
{
	qbyte *COM_LoadFile (char *path, int usehunk);
	unsigned char *file;
	gltexture_t	*glt;
	char inname[MAX_OSPATH];

	if (!gl_config.arb_texture_compression || !gl_compress.value)
		return 0;


	// see if the texture is already present
	if (name[0])
	{
		int num = GL_FindTexture(name);
		if (num != -1)
			return num;
	}
	else
		return 0;


	snprintf(inname, sizeof(inname)-1, "tex/%s.tex", name);
	file = COM_LoadFile(inname, 5);
	if (!file)
		return 0;

	glt = BZ_Malloc(sizeof(*glt)+sizeof(bucket_t));
	glt->next = gltextures;
	gltextures = glt;

	strcpy (glt->identifier, name);
	glt->texnum = texture_extension_number;
	glt->bpp = 32;

	Hash_Add(&gltexturetable, glt->identifier, glt, (bucket_t*)(glt+1));

	GL_Bind(texture_extension_number );

	if (!GL_UploadCompressed (file, &glt->width, &glt->height, (unsigned int *)&glt->mipmap))
		return 0;

	texture_extension_number++;

	return texture_extension_number-1;
}

int GL_LoadTexture8Grey (char *identifier, int width, int height, unsigned char *data, qboolean mipmap)
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

	glt = BZ_Malloc(sizeof(*glt)+sizeof(bucket_t));
	glt->next = gltextures;
	gltextures = glt;

	strcpy (glt->identifier, identifier);
	glt->texnum = texture_extension_number;
	glt->width = width;
	glt->height = height;
	glt->bpp = 8;
	glt->mipmap = mipmap;

	Hash_Add(&gltexturetable, glt->identifier, glt, (bucket_t*)(glt+1));

//	if (!isDedicated)
	{
		GL_Bind(texture_extension_number );

		GL_Upload8Grey (data, width, height, mipmap);
	}

	texture_extension_number++;

	return texture_extension_number-1;
}

int GL_LoadTexture8Bump (char *identifier, int width, int height, unsigned char *data, qboolean mipmap, float bumpscale)
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
	glt->texnum = texture_extension_number;
	glt->width = width;
	glt->height = height;
	glt->bpp = 8;
	glt->mipmap = mipmap;

	Hash_Add(&gltexturetable, glt->identifier, glt, (bucket_t*)(glt+1));

//	if (!isDedicated)
	{
		GL_Bind(texture_extension_number );

		GL_UploadBump (data, width, height, mipmap, bumpscale);
	}

	texture_extension_number++;

	return texture_extension_number-1;
}

/*
================
GL_LoadPicTexture
================
*/
int GL_LoadPicTexture (qpic_t *pic)
{
	return GL_LoadTexture ("", pic->width, pic->height, pic->data, false, true);
}

/****************************************/
#endif
