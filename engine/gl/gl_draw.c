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
#include "glquake.h"
#include "shader.h"

//#define GL_USE8BITTEX

int glx, gly, glwidth, glheight;

mesh_t	draw_mesh;
vec4_t	draw_mesh_xyz[4];
vec3_t	draw_mesh_normals[4];	
vec2_t	draw_mesh_st[4];
vec2_t	draw_mesh_lmst[4];
//byte_vec4_t	draw_mesh_colors[4];

qbyte				*uploadmemorybuffer;
int					sizeofuploadmemorybuffer;
qbyte				*uploadmemorybufferintermediate;
int					sizeofuploadmemorybufferintermediate;

index_t r_quad_indexes[6] = {0, 1, 2, 0, 2, 3};

extern qbyte		gammatable[256];

unsigned char *d_15to8table;
qboolean inited15to8;
extern cvar_t crosshair, cl_crossx, cl_crossy, crosshaircolor, crosshairsize;

static int filmtexture;

extern cvar_t		gl_nobind;
extern cvar_t		gl_max_size;
extern cvar_t		gl_picmip;
extern cvar_t		r_drawdisk;
extern cvar_t		gl_compress;
extern cvar_t		gl_font, gl_conback, gl_smoothfont;

extern cvar_t		gl_savecompressedtex;

extern cvar_t		gl_load24bit;

qbyte		*draw_chars;				// 8*8 graphic characters
qpic_t		*draw_disc;
qpic_t		*draw_backtile;

int			translate_texture;
int			char_texture, char_tex2, default_char_texture;
int			cs_texture; // crosshair texture
extern int detailtexture;

static unsigned cs_data[16*16];
int cachedcrosshair;

typedef struct
{
	int		texnum;
	float	sl, tl, sh, th;
} glpic_t;

qbyte		conback_buffer[sizeof(qpic_t) + sizeof(glpic_t)];
qbyte		custconback_buffer[sizeof(qpic_t) + sizeof(glpic_t)];
qpic_t		*default_conback = (qpic_t *)&conback_buffer, *conback, *custom_conback = (qpic_t *)&custconback_buffer;

#include "hash.h"
hashtable_t gltexturetable;
bucket_t *gltexturetablebuckets[256];

int		gl_lightmap_format = 4;
int		gl_solid_format = 3;
int		gl_alpha_format = 4;

int		gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int		gl_filter_max = GL_LINEAR;


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
qbyte		scrap_texels[MAX_SCRAPS][BLOCK_WIDTH*BLOCK_HEIGHT*4];
qboolean	scrap_dirty;
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

		return texnum;
	}

	Sys_Error ("Scrap_AllocBlock: full");
	return 0;
}

int	scrap_uploads;

void Scrap_Upload (void)
{
	scrap_uploads++;
	GL_Bind(scrap_texnum);
	GL_Upload8 (scrap_texels[0], BLOCK_WIDTH, BLOCK_HEIGHT, false, true);
	scrap_dirty = false;
}

//=============================================================================
/* Support Routines */

typedef struct glcachepic_s
{
	char		name[MAX_QPATH];
	qpic_t		pic;
	qbyte		padding[32];	// for appended glpic
} glcachepic_t;

#define	MAX_CACHED_PICS		512	//a temporary solution
glcachepic_t	glmenu_cachepics[MAX_CACHED_PICS];
int			glmenu_numcachepics;

qbyte		menuplyr_pixels[4096];

int		pic_texels;
int		pic_count;

qboolean Draw_RealPicFromWad (qpic_t	*out, char *name)
{
	qpic_t	*in;
	glpic_t	*gl;
	int texnum;
	char name2[256];

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
	texnum = Mod_LoadReplacementTexture(name, false, true, false);
	if (!in && !texnum)	//try a q2 texture
	{
		sprintf(name2, "pics/%s", name);
		texnum = Mod_LoadHiResTexture(name2, false, true, false);
	}
	
	if (texnum)
	{
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
		scrap_dirty = true;
		k = 0;
		for (i=0 ; i<in->height ; i++)
			for (j=0 ; j<in->width ; j++, k++)
				scrap_texels[texnum][(y+i)*BLOCK_WIDTH + x + j] = in->data[k];
		texnum += scrap_texnum;
		gl->texnum = texnum;
		gl->sl = (x+0.01)/(float)BLOCK_WIDTH;
		gl->sh = (x+in->width-0.01)/(float)BLOCK_WIDTH;
		gl->tl = (y+0.01)/(float)BLOCK_WIDTH;
		gl->th = (y+in->height-0.01)/(float)BLOCK_WIDTH;

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
	return true;
}

char *failedpic;	//easier this way
qpic_t *GLDraw_SafePicFromWad (char *name)
{
	int i;
	glcachepic_t	*pic;
	for (pic=glmenu_cachepics, i=0 ; i<glmenu_numcachepics ; pic++, i++)
		if (!strcmp (name, pic->name))
			return &pic->pic;

	if (glmenu_numcachepics == MAX_CACHED_PICS)
		Sys_Error ("menu_numcachepics == MAX_CACHED_PICS");
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

qpic_t *GLDraw_PicFromWad (char *name)
{
	qpic_t	*pic = GLDraw_SafePicFromWad (name);
	if (!pic)
		Sys_Error ("GLDraw_PicFromWad: failed to load %s", name);

	return pic;
}

qpic_t	*GLDraw_SafeCachePic (char *path)
{
	glcachepic_t	*pic;
	int			i;
	qpic_t		*dat;
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
		_snprintf(alternatename, MAX_QPATH-1, "pics/%s.pcx", path);
		dat = (qpic_t *)COM_LoadMallocFile (alternatename);
		if (dat)
		{
			strcpy(pic->name, path);
			if ((mem = ReadPCXFile((qbyte *)dat, com_filesize, &pic->pic.width, &pic->pic.height)))
			{
				gl = (glpic_t *)pic->pic.data;
				if (!(gl->texnum = Mod_LoadReplacementTexture(alternatename, false, true, false)))
					gl->texnum = GL_LoadTexture32(path, pic->pic.width, pic->pic.height, (unsigned *)dat, false, false);
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

	{
		char *mem;
		char alternatename[MAX_QPATH];
		_snprintf(alternatename, MAX_QPATH-1, "%s", path);
		dat = (qpic_t *)COM_LoadMallocFile (alternatename);
		if (dat)
		{
			strcpy(pic->name, path);
			mem = NULL;
			if (!mem)
				mem = ReadTargaFile((qbyte *)dat, com_filesize, &pic->pic.width, &pic->pic.height, 0);
#ifdef AVAIL_PNGLIB
			if (!mem);
				mem = ReadPNGFile((qbyte *)dat, com_filesize, &pic->pic.width, &pic->pic.height);
#endif
#ifdef AVAIL_JPEGLIB
			if (!mem)
				mem = ReadJPEGFile((qbyte *)dat, com_filesize, &pic->pic.width, &pic->pic.height);
#endif
			if (!mem)
				mem = ReadPCXFile((qbyte *)dat, com_filesize, &pic->pic.width, &pic->pic.height);
			if (mem)
			{
				gl = (glpic_t *)pic->pic.data;
				if (!(gl->texnum = Mod_LoadReplacementTexture(alternatename, false, true, false)))
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

#ifdef AVAIL_JPEGLIB
	{
		char *mem;
		char alternatename[MAX_QPATH];
		_snprintf(alternatename, MAX_QPATH-1,"%s.jpg", path);
		dat = (qpic_t *)COM_LoadMallocFile (alternatename);
		if (dat)
		{
			strcpy(pic->name, path);
			if ((mem = ReadJPEGFile((qbyte *)dat, com_filesize, &pic->pic.width, &pic->pic.height)))
			{
				gl = (glpic_t *)pic->pic.data;
				if (!(gl->texnum = Mod_LoadReplacementTexture(alternatename, false, true, false)))
					gl->texnum = GL_LoadTexture32(path, pic->pic.width, pic->pic.height, (unsigned *)mem, false, false);
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
	dat = (qpic_t *)COM_LoadTempFile (path);
	if (!dat)
	{
		char alternatename[MAX_QPATH];
		sprintf(alternatename, "gfx/%s.lmp", path);
		dat = (qpic_t *)COM_LoadTempFile (alternatename);
		if (!dat)
			return GLDraw_SafePicFromWad(path);
	}

	SwapPic (dat);

	if (((8+dat->width*dat->height+3)&(~3)) != ((com_filesize+3)&(~3)))	//round up to the nearest 4.
	{
		char alternatename[MAX_QPATH];
		sprintf(alternatename, "gfx/%s.lmp", path);
		dat = (qpic_t *)COM_LoadTempFile (alternatename);
		if (!dat)
			return GLDraw_SafePicFromWad(path);
	}

	// HACK HACK HACK --- we need to keep the bytes for
	// the translatable player picture just for the menu
	// configuration dialog

	if (!strncmp (path, "gfx/player/", 11) || !strcmp (path, "gfx/menuplyr.lmp"))	//these arn't cached. I hate hacks.
		memcpy (menuplyr_pixels, dat->data, dat->width*dat->height);
	else
	{
		glmenu_numcachepics++;
		Q_strncpyz (pic->name, path, sizeof(pic->name));
	}

	pic->pic.width = dat->width;
	pic->pic.height = dat->height;

	gl = (glpic_t *)pic->pic.data;
	if (!(gl->texnum = Mod_LoadReplacementTexture(path, false, true, false)))
		gl->texnum = GL_LoadPicTexture (dat);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return &pic->pic;
}
qpic_t	*GLDraw_CachePic (char *path)
{
	qpic_t	*pic = GLDraw_SafeCachePic (path);
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
	int	minimize, maximize;
} glmode_t;

glmode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

/*
===============
Draw_TextureMode_f
===============
*/
void GLDraw_TextureMode_f (void)
{
	int		i;
	gltexture_t	*glt;

	if (Cmd_Argc() == 1)
	{
		for (i=0 ; i< 6 ; i++)
			if (gl_filter_min == modes[i].minimize)
			{
				Con_Printf ("%s\n", modes[i].name);
				return;
			}
		Con_Printf ("current filter is unknown???\n");
		return;
	}

	for (i=0 ; i< 6 ; i++)
	{
		if (!Q_strcasecmp (modes[i].name, Cmd_Argv(1) ) )
			break;
	}
	if (i == 6)
	{
		Con_Printf ("bad filter name\n");
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
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		}
	}
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
	extern int	lightmap_textures;

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
	lightmap_textures=0;
	filmtexture=0;
	glmenu_numcachepics=0;
#ifdef Q3SHADERS
	r_fogtexture=0;
#endif
	GL_FlushBinds();
//	GL_FlushSkinCache();
	TRACE(("dbg: GLDraw_ReInit: GL_GAliasFlushSkinCache\n"));
	GL_GAliasFlushSkinCache();

	memset(scrap_allocated, 0, sizeof(scrap_allocated));


	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxtexsize);
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
		for (i=0 ; i<128*128 ; i++)
			if (draw_chars[i] == 0)
				draw_chars[i] = 255;	// proper transparent color
	}

	// now turn them into textures
	TRACE(("dbg: GLDraw_ReInit: looking for conchars\n"));
	if (!(char_texture=Mod_LoadReplacementTexture("gfx/conchars.lmp", false, true, false))) //no high res
	{
		if (!draw_chars)	//or low res.
		{
			if (!(char_texture=Mod_LoadHiResTexture("pics/conchars.pcx", false, true, false)))	//try low res q2 path
			{
				char *tempchars = COM_LoadMallocFile("gfx/menu/conchars.lmp");
				char *in, *out;
				if (!tempchars)
					Sys_Error("No charset found\n");

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

				for (i=0 ; i<128*128 ; i++)
					if (draw_chars[i] == 0)
						draw_chars[i] = 255;	// proper transparent color
				char_texture = GL_LoadTexture ("charset", 128, 128, draw_chars, false, true);
				Z_Free(draw_chars);
				draw_chars = NULL;
			}
		}
		else
			char_texture = GL_LoadTexture ("charset", 128, 128, draw_chars, false, true);
	}
	default_char_texture=char_texture;

	TRACE(("dbg: GLDraw_ReInit: loaded charset\n"));

	gl_font.modified = true;

	gl_smoothfont.modified = 1;


	TRACE(("dbg: GLDraw_ReInit: GL_BeginRendering\n"));
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	TRACE(("dbg: GLDraw_ReInit: SCR_DrawLoading\n"));

	GL_Set2D();

	glClear(GL_COLOR_BUFFER_BIT);
	{
		qpic_t *pic = Draw_SafeCachePic ("loading");
		if (pic)
			Draw_Pic ( (vid.width - pic->width)/2, 
				(vid.height - 48 - pic->height)/2, pic);
	}

	TRACE(("dbg: GLDraw_ReInit: GL_EndRendering\n"));
	GL_EndRendering ();	


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
	if (!(char_tex2=Mod_LoadReplacementTexture("gfx/conchars2.lmp", false, true, false)))
	{
		if (!draw_chars)
			char_tex2 = char_texture;
		else
			char_tex2 = GL_LoadTexture ("charset", 128, 128, draw_chars, false, true);
	}

	cs_texture = texture_extension_number++;
	cachedcrosshair=0;

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
#ifdef DISTRIBUTION
			sprintf (ver, "%s %4.2f", DISTRIBUTION, VERSION);
#else
			sprintf (ver, "%4.2f", VERSION);
#endif
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
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	gl = (glpic_t *)conback->data;
	if (!(gl->texnum=Mod_LoadReplacementTexture("gfx/conback.lmp", false, true, false)))
	{
		if (!ncdata)	//no fallback
		{
			if (!(gl->texnum=Mod_LoadHiResTexture("pics/conback.pcx", false, true, false)))
				if (!(gl->texnum=Mod_LoadReplacementTexture("gfx/menu/conback.lmp", false, true, false)))
					Sys_Error ("Couldn't load gfx/conback.lmp");	//that's messed it up, hasn't it?...
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

	gl_conback.modified = true;

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

	detailtexture = Mod_LoadReplacementTexture("textures/detail", true, false, false);

	inited15to8 = false;

	glClearColor (1,0,0,0);

	TRACE(("dbg: GLDraw_ReInit: PPL_LoadSpecularFragmentProgram\n"));
	PPL_LoadSpecularFragmentProgram();

#ifdef PLUGINS
	Plug_DrawReloadImages();
#endif
}

void GLDraw_Init (void)
{

	memset(scrap_allocated, 0, sizeof(scrap_allocated));

	R_BackendInit();

	Cmd_AddRemCommand ("gl_texturemode", &GLDraw_TextureMode_f);

	GLDraw_ReInit();



	draw_mesh.numindexes = 6;
	draw_mesh.indexes = r_quad_indexes;
	draw_mesh.trneighbors = NULL;

	draw_mesh.numvertexes = 4;
	draw_mesh.xyz_array = draw_mesh_xyz;
	draw_mesh.normals_array = draw_mesh_normals;
	draw_mesh.st_array = draw_mesh_st;
	draw_mesh.lmst_array = draw_mesh_lmst;

}
void GLDraw_DeInit (void)
{
	Cmd_RemoveCommand("gl_texturemode");
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

	if (num == 32)
		return;		// space

	if (y <= -8)
		return;			// totally off screen


	num &= 255;

	row = num>>4;
	col = num&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;

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

#ifndef Q3SHADERS
	if (num&CON_2NDCHARSETTEXT)
		GL_DrawMesh(&draw_mesh, NULL, char_tex2, 0);
	else
		GL_DrawMesh(&draw_mesh, NULL, char_texture, 0);
#else
	
	if (num&CON_2NDCHARSETTEXT)
		GL_Bind (char_tex2);
	else
		GL_Bind (char_texture);	

	num &= 255;

	row = num>>4;
	col = num&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;
	
	glBegin (GL_QUADS);
	glTexCoord2f (fcol, frow);
	glVertex2f (x, y);
	glTexCoord2f (fcol + size, frow);
	glVertex2f (x+8, y);
	glTexCoord2f (fcol + size, frow + size);
	glVertex2f (x+8, y+8);
	glTexCoord2f (fcol, frow + size);
	glVertex2f (x, y+8);
	glEnd ();

#endif
}

void GLDraw_ColouredCharacter (int x, int y, unsigned int num)
{
	int col;
	
	if (num & CON_BLINKTEXT)
	{
		if ((int)(realtime*3) & 1)
			return;
	}

	{
		col = (num&CON_COLOURMASK)/256;
		glColor3f(consolecolours[col].r, consolecolours[col].g, consolecolours[col].b);
	}

	Draw_Character(x, y, num);
}
/*
================
Draw_String
================
*/
void GLDraw_String (int x, int y, const qbyte *str)
{
	while (*str)
	{
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
void GLDraw_Crosshair(void)
{
	int x, y;
	int sc;

	float x1, x2, y1, y2;
	float size;

	if (crosshair.value == 1)
	{
		for (sc = 0; sc < cl.splitclients; sc++)
		{
			SCR_CrosshairPosition(sc, &x, &y);
			GLDraw_Character (x-4, y-4, '+');
		}
		return;
	}
	glDisable (GL_BLEND);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (crosshair.value)
	{
		GL_Bind (cs_texture);

		if (cachedcrosshair != crosshair.value || crosshair.value >= FIRSTANIMATEDCROSHAIR)
		{
			int c = d_8to24rgbtable[(qbyte) crosshaircolor.value];
			int c2 = d_8to24rgbtable[(qbyte) crosshaircolor.value];

#define Pix(x,y,c) {	\
			if (y+8<0)c=0;	\
			if (y+8>=16)c=0;	\
			if (x+8<0)c=0;	\
			if (x+8>=16)c=0;	\
				\
			cs_data[(y+8)*16+(x+8)] = c;	\
		}
			memset(cs_data, 0, sizeof(cs_data));
			switch((int)crosshair.value)
			{
			default:
#include "crosshairs.dat"
			}
			GL_Upload32(NULL, cs_data, 16, 16, 0, true);

#undef Pix
			cachedcrosshair = crosshair.value;
		}
	}
	else if ((*crosshair.string>='a' && *crosshair.string<='z') || (*crosshair.string>='A' && *crosshair.string<='Z'))
	{				
		int i = Mod_LoadHiResTexture (crosshair.string, false, true, true);
		GL_Bind (i);
	}
	else
		return;

	for (sc = 0; sc < cl.splitclients; sc++)
	{
		SCR_CrosshairPosition(sc, &x, &y);

		size = crosshairsize.value;
		x1 = x - size;
		x2 = x + size;
		y1 = y - size;
		y2 = y + size;
		glBegin (GL_QUADS);
		glTexCoord2f (0, 0);
		glVertex2f (x1, y1);
		glTexCoord2f (1, 0);
		glVertex2f (x2, y1);
		glTexCoord2f (1, 1);
		glVertex2f (x2, y2);
		glTexCoord2f (0, 1);
		glVertex2f (x1, y2);
		glEnd ();
	}
	
//	glTexEnvf ( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
//	glTexEnvf ( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
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
void GLDraw_Pic (int x, int y, qpic_t *pic)
{
	glpic_t			*gl;

	if (!pic)
		return;

	if (scrap_dirty)
		Scrap_Upload ();
	gl = (glpic_t *)pic->data;

#ifndef Q3SHADERS
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

	GL_DrawMesh(&draw_mesh, NULL, gl->texnum, 0);
#else

	glColor4f (1,1,1,1);
	GL_Bind (gl->texnum);
	glBegin (GL_QUADS);
	glTexCoord2f (gl->sl, gl->tl);
	glVertex2f (x, y);
	glTexCoord2f (gl->sh, gl->tl);
	glVertex2f (x+pic->width, y);
	glTexCoord2f (gl->sh, gl->th);
	glVertex2f (x+pic->width, y+pic->height);
	glTexCoord2f (gl->sl, gl->th);
	glVertex2f (x, y+pic->height);
	glEnd ();
#endif
}

void GLDraw_LevelPic (qpic_t *pic)	//Fullscreen and stuff
{
	glpic_t			*gl;

	if (!pic)
		return;

	if (scrap_dirty)
		Scrap_Upload ();
	gl = (glpic_t *)pic->data;
	glColor4f (1,1,1,1);
	GL_Bind (gl->texnum);
	glBegin (GL_QUADS);
	glTexCoord2f (gl->sl, gl->tl);
	glVertex2f (0, 0);
	glTexCoord2f (gl->sh, gl->tl);
	glVertex2f (vid.conwidth, 0);
	glTexCoord2f (gl->sh, gl->th);
	glVertex2f (vid.conwidth, vid.conheight);
	glTexCoord2f (gl->sl, gl->th);
	glVertex2f (0, vid.conheight);
	glEnd ();
}

/*
=============
Draw_AlphaPic
=============
*/
void GLDraw_AlphaPic (int x, int y, qpic_t *pic, float alpha)
{
	glpic_t			*gl;

	if (scrap_dirty)
		Scrap_Upload ();
	gl = (glpic_t *)pic->data;
	glDisable(GL_ALPHA_TEST);
	glEnable (GL_BLEND);
//	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glCullFace(GL_FRONT);
	glColor4f (1,1,1,alpha);
	GL_Bind (gl->texnum);
	glBegin (GL_QUADS);
	glTexCoord2f (gl->sl, gl->tl);
	glVertex2f (x, y);
	glTexCoord2f (gl->sh, gl->tl);
	glVertex2f (x+pic->width, y);
	glTexCoord2f (gl->sh, gl->th);
	glVertex2f (x+pic->width, y+pic->height);
	glTexCoord2f (gl->sl, gl->th);
	glVertex2f (x, y+pic->height);
	glEnd ();
	glColor4f (1,1,1,1);
	glEnable(GL_ALPHA_TEST);
	glDisable (GL_BLEND);
}

void GLDraw_SubPic(int x, int y, qpic_t *pic, int srcx, int srcy, int width, int height)
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

#ifdef Q3SHADERS
	GL_DrawAliasMesh(&draw_mesh, gl->texnum);
#else
	GL_DrawMesh(&draw_mesh, NULL, gl->texnum, 0);
#endif
}

/*
=============
Draw_TransPic
=============
*/
void GLDraw_TransPic (int x, int y, qpic_t *pic)
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
void GLDraw_TransPicTranslate (int x, int y, qpic_t *pic, qbyte *translation)
{
	int				v, u, c;
	unsigned		trans[64*64], *dest;
	qbyte			*src;
	int				p;

	GL_Bind (translate_texture);

	c = pic->width * pic->height;

	dest = trans;
	for (v=0 ; v<64 ; v++, dest += 64)
	{
		src = &menuplyr_pixels[ ((v*pic->height)>>6) *pic->width];
		for (u=0 ; u<64 ; u++)
		{
			p = src[(u*pic->width)>>6];
			if (p == 255)
				dest[u] = p;
			else
				dest[u] =  d_8to24rgbtable[translation[p]];
		}
	}

	glTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glColor3f (1,1,1);
	glBegin (GL_QUADS);
	glTexCoord2f (0, 0);
	glVertex2f (x, y);
	glTexCoord2f (1, 0);
	glVertex2f (x+pic->width, y);
	glTexCoord2f (1, 1);
	glVertex2f (x+pic->width, y+pic->height);
	glTexCoord2f (0, 1);
	glVertex2f (x, y+pic->height);
	glEnd ();
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
	int y;

	y = (vid.height * 3) >> 2;
	conback->width = vid.conwidth;
	conback->height = vid.conheight;

	if (lines > y)
	{
		glColor3f (1,1,1);
		GLDraw_Pic(0, lines-vid.height, conback);
	}
	else
	{
		GLDraw_AlphaPic (0, lines - vid.height, conback, (float)(1.2 * lines)/y);
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
	glColor3f (1,1,1);
	if (!draw_backtile)
	{
		glDisable(GL_TEXTURE_2D);
		glBegin (GL_QUADS);
		glTexCoord2f (x/64.0, y/64.0);
		glVertex2f (x, y);
		glTexCoord2f ( (x+w)/64.0, y/64.0);
		glVertex2f (x+w, y);
		glTexCoord2f ( (x+w)/64.0, (y+h)/64.0);
		glVertex2f (x+w, y+h);
		glTexCoord2f ( x/64.0, (y+h)/64.0 );
		glVertex2f (x, y+h);
		glEnd ();
		glEnable(GL_TEXTURE_2D);
	}
	else
	{
		GL_Bind (*(int *)draw_backtile->data);
		glBegin (GL_QUADS);
		glTexCoord2f (x/64.0, y/64.0);
		glVertex2f (x, y);
		glTexCoord2f ( (x+w)/64.0, y/64.0);
		glVertex2f (x+w, y);
		glTexCoord2f ( (x+w)/64.0, (y+h)/64.0);
		glVertex2f (x+w, y+h);
		glTexCoord2f ( x/64.0, (y+h)/64.0 );
		glVertex2f (x, y+h);
		glEnd ();
	}
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void GLDraw_Fill (int x, int y, int w, int h, int c)
{
	glDisable (GL_TEXTURE_2D);
	glColor3f (gammatable[host_basepal[c*3]]/255.0,
		gammatable[host_basepal[c*3+1]]/255.0,
		gammatable[host_basepal[c*3+2]]/255.0);

	glBegin (GL_QUADS);

	glVertex2f (x,y);
	glVertex2f (x+w, y);
	glVertex2f (x+w, y+h);
	glVertex2f (x, y+h);

	glEnd ();
	glColor3f (1,1,1);
	glEnable (GL_TEXTURE_2D);
}
//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void GLDraw_FadeScreen (void)
{
	glEnable (GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glDisable (GL_TEXTURE_2D);
	glColor4f (0, 0, 0, 0.5);
	glBegin (GL_QUADS);

	glVertex2f (0,0);
	glVertex2f (vid.width, 0);
	glVertex2f (vid.width, vid.height);
	glVertex2f (0, vid.height);

	glEnd ();
	glColor4f (1,1,1,1);
	glEnable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);
	glEnable(GL_ALPHA_TEST);

	Sbar_Changed();
}

void GLDraw_Image(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qpic_t *pic)
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

#ifdef Q3SHADERS
	GL_DrawAliasMesh(&draw_mesh, gl->texnum);
#else
	GL_DrawMesh(&draw_mesh, NULL, gl->texnum, 0);
#endif
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
	glDrawBuffer  (GL_FRONT);
	Draw_Pic (vid.width - 24, 0, draw_disc);
	glDrawBuffer  (GL_BACK);
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

/*
================
GL_Set2D

Setup as if the screen was 320*200
================
*/
void GL_Set2D (void)
{
	glViewport (glx, gly, glwidth, glheight);

	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();
	glOrtho  (0, vid.width, vid.height, 0, -99999, 99999);

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();

	glDisable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);
	glDisable (GL_BLEND);
	glEnable (GL_ALPHA_TEST);
//	glDisable (GL_ALPHA_TEST);

	glColor4f (1,1,1,1);

	if (gl_font.modified)
	{
		gl_font.modified = 0;
		if (!*gl_font.string || !(char_texture=Mod_LoadHiResTexture(va("fonts/%s", gl_font.string), false, true, true)))
			char_texture = default_char_texture;

		gl_smoothfont.modified = 1;
	}
	if (gl_conback.modified)
	{
		int newtex = 0;
		gl_conback.modified = 0;
		if (!*gl_conback.string || !(newtex=Mod_LoadHiResTexture(va("conbacks/%s", gl_conback.string), false, true, true)))
			conback = default_conback;
		else
		{
			conback = custom_conback;
			((glpic_t *)conback->data)->texnum = newtex;
		}
	}

	if (gl_smoothfont.modified)
	{
		GL_Bind(char_texture);
		if (gl_smoothfont.value)
		{
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		else
		{
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}
	}
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

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2f(0, 0);
	glTexCoord2f(0, 1);
	glVertex2f(0, vid.height);
	glTexCoord2f(1, 1);
	glVertex2f(vid.width, vid.height);
	glTexCoord2f(1, 0);
	glVertex2f(vid.width, 0);
	glEnd();
	glEnable(GL_ALPHA_TEST);


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

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2f(0, 0);
	glTexCoord2f(0, 1);
	glVertex2f(0, vid.height);
	glTexCoord2f(1, 1);
	glVertex2f(vid.width, vid.height);
	glTexCoord2f(1, 0);
	glVertex2f(vid.width, 0);
	glEnd();
	glEnable(GL_ALPHA_TEST);


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

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2f(0, 0);
	glTexCoord2f(0, 1);
	glVertex2f(0, vid.height);
	glTexCoord2f(1, 1);
	glVertex2f(vid.width, vid.height);
	glTexCoord2f(1, 0);
	glVertex2f(vid.width, 0);
	glEnd();
	glEnable(GL_ALPHA_TEST);


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
	FILE *f;

	qboolean savetable;

	// JACK: 3D distance calcs - k is last closest, l is the distance.
	if (inited15to8)
		return;
	if (!d_15to8table)
		d_15to8table = BZ_Malloc(sizeof(qbyte) * 32768);
	inited15to8 = true;

	savetable = COM_CheckParm("-save15to8");

	if (savetable)
		COM_FOpenFile("glquake/15to8.pal", &f);
	else
		f = NULL;
	if (f)
	{
		fread(d_15to8table, 1<<15, 1, f);
		fclose(f);
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
			char s[256];
			sprintf(s, "%s/glquake", com_gamedir);
 			Sys_mkdir (s);
			sprintf(s, "%s/glquake/15to8.pal", com_gamedir);
			if ((f = fopen(s, "wb")) != NULL) {
				fwrite(d_15to8table, 1<<15, 1, f);
				fclose(f);
			}
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
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	return true;
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

	if (gl_config.arb_texture_non_power_of_two)	//NPOT is a simple extension that relaxes errors.
	{
		TRACE(("dbg: GL_Upload32: GL_ARB_texture_non_power_of_two\n"));
		scaled_width = width;
		scaled_height = height;
	}
	else
	{
		for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1)
			;
		for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1)
			;
	}

	TRACE(("dbg: GL_Upload32: %f\n", gl_picmip.value));
	scaled_width >>= (int)gl_picmip.value;
	scaled_height >>= (int)gl_picmip.value;

	TRACE(("dbg: GL_Upload32: %f\n", gl_max_size.value));
	if (gl_max_size.value)
	{
		if (scaled_width > gl_max_size.value)
			scaled_width = gl_max_size.value;
		if (scaled_height > gl_max_size.value)
			scaled_height = gl_max_size.value;
	}

	if (scaled_width < 1)
		scaled_width = 1;
	if (scaled_height < 1)
		scaled_height = 1;

	TRACE(("dbg: GL_Upload32: %i %i\n", scaled_width, scaled_height));

	if (scaled_width * scaled_height > sizeofuploadmemorybuffer/4)
		Sys_Error ("GL_LoadTexture: too big");

	samples = alpha ? gl_alpha_format : gl_solid_format;
	if (gl_config.arb_texture_compression && gl_compress.value && name&&mipmap)
		samples = alpha ? GL_COMPRESSED_RGBA_ARB : GL_COMPRESSED_RGB_ARB;	

#if 0
	if (mipmap)
		gluBuild2DMipmaps (GL_TEXTURE_2D, samples, width, height, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	else if (scaled_width == width && scaled_height == height)
		glTexImage2D (GL_TEXTURE_2D, 0, samples, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	else
	{
		gluScaleImage (GL_RGBA, width, height, GL_UNSIGNED_BYTE, trans,
			scaled_width, scaled_height, GL_UNSIGNED_BYTE, scaled);
		glTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	}
#else
texels += scaled_width * scaled_height;

	if (gl_config.sgis_generate_mipmap&&mipmap)
	{
		TRACE(("dbg: GL_Upload32: GL_SGIS_generate_mipmap\n"));
		glTexParameterf(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
	}

	if (scaled_width == width && scaled_height == height)
	{
		if (!mipmap||gl_config.sgis_generate_mipmap)	//gotta love this with NPOT textures... :)
		{
			TRACE(("dbg: GL_Upload32: non-mipmapped/unscaled\n"));
			glTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			goto done;
		}
		memcpy (scaled, data, width*height*4);
	}
	else
		GL_ResampleTexture (data, width, height, scaled, scaled_width, scaled_height);

	TRACE(("dbg: GL_Upload32: recaled\n"));
	glTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
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
			glTexImage2D (GL_TEXTURE_2D, miplevel, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
		}
	}
	if (gl_config.arb_texture_compression && gl_compress.value && gl_savecompressedtex.value && name&&mipmap)
	{
		FILE *out;
		int miplevels;
		GLint compressed;
		GLint compressed_size;
		GLint internalformat;
		unsigned char *img;
		char outname[MAX_OSPATH];
		int i;
		miplevels = miplevel+1;
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_ARB, &compressed); 
		if (compressed == GL_TRUE && !strstr(name, ".."))	//is there any point in bothering with the whole endian thing?
		{
			sprintf(outname, "%s/tex/%s.tex", com_gamedir, name);
			COM_CreatePath(outname);			
			out = fopen(outname, "wb");
			if (out)
			{
				i = LittleLong(miplevels);
				fwrite(&i, 1, sizeof(i), out);
				i = LittleLong(width);
				fwrite(&i, 1, sizeof(i), out);
				i = LittleLong(height);
				fwrite(&i, 1, sizeof(i), out);
				i = LittleLong(mipmap);
				fwrite(&i, 1, sizeof(i), out);
				for (miplevel = 0; miplevel < miplevels; miplevel++)
				{
					glGetTexLevelParameteriv(GL_TEXTURE_2D, miplevel, GL_TEXTURE_COMPRESSED_ARB, &compressed);
					glGetTexLevelParameteriv(GL_TEXTURE_2D, miplevel, GL_TEXTURE_INTERNAL_FORMAT, &internalformat);
					glGetTexLevelParameteriv(GL_TEXTURE_2D, miplevel, GL_TEXTURE_COMPRESSED_IMAGE_SIZE_ARB, &compressed_size);
					glGetTexLevelParameteriv(GL_TEXTURE_2D, miplevel, GL_TEXTURE_WIDTH, &width);
					glGetTexLevelParameteriv(GL_TEXTURE_2D, miplevel, GL_TEXTURE_HEIGHT, &height);
					img = (unsigned char *)BZ_Malloc(compressed_size * sizeof(unsigned char));
					qglGetCompressedTexImageARB(GL_TEXTURE_2D, miplevel, img);

					i = LittleLong(width);
					fwrite(&i, 1, sizeof(i), out);
					i = LittleLong(height);
					fwrite(&i, 1, sizeof(i), out);
					i = LittleLong(compressed_size);
					fwrite(&i, 1, sizeof(i), out);
					i = LittleLong(internalformat);
					fwrite(&i, 1, sizeof(i), out);
					fwrite(img, 1, compressed_size, out);
					BZ_Free(img);
				}
				fclose(out);
			}
		}
	}
done:
	if (gl_config.sgis_generate_mipmap&&mipmap)
		glTexParameterf(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_FALSE);
#endif


	if (mipmap)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
}

void GL_Upload8Grey (unsigned char*data, int width, int height,  qboolean mipmap)
{
	int			samples;
	unsigned char	*scaled = uploadmemorybuffer;
	int			scaled_width, scaled_height;

	if (gl_config.arb_texture_non_power_of_two)	//NPOT is a simple extension that relaxes errors.
	{
		TRACE(("dbg: GL_Upload32: GL_ARB_texture_non_power_of_two\n"));
		scaled_width = width;
		scaled_height = height;
	}
	else
	{
		for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1)
			;
		for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1)
			;
	}

	scaled_width >>= (int)gl_picmip.value;
	scaled_height >>= (int)gl_picmip.value;

	if (gl_max_size.value)
	{
		if (scaled_width > gl_max_size.value)
			scaled_width = gl_max_size.value;
		if (scaled_height > gl_max_size.value)
			scaled_height = gl_max_size.value;
	}

	if (scaled_width < 1)
		scaled_width = 1;
	if (scaled_height < 1)
		scaled_height = 1;

	if (scaled_width * scaled_height > sizeofuploadmemorybuffer/4)
		Sys_Error ("GL_LoadTexture: too big");

	samples = 1;//alpha ? gl_alpha_format : gl_solid_format;

texels += scaled_width * scaled_height;

	if (scaled_width == width && scaled_height == height)
	{
		if (!mipmap)
		{
			glTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
			goto done;
		}
		memcpy (scaled, data, width*height);
	}
	else
		GL_Resample8BitTexture (data, width, height, scaled, scaled_width, scaled_height);

	glTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, scaled);
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
			glTexImage2D (GL_TEXTURE_2D, miplevel, samples, scaled_width, scaled_height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, scaled);
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
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
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
      nmap[i*w+j] = LittleLong ((255 << 24)|(b << 16)|(g << 8)|(r));	// <AWE> Added support for big endian.
    }
  }

  return &nmap[0];
}

//PENTA
void GL_UploadBump(qbyte *data, int width, int height, qboolean mipmap) {
	
	int			s;
    unsigned char	*scaled = uploadmemorybuffer;
	int			scaled_width, scaled_height;
	qbyte			*nmap;

	TRACE(("dbg: GL_UploadBump entered: %i %i\n", width, height));

	s = width*height;

	//Resize to power of 2 and maximum texture size
	if (gl_config.arb_texture_non_power_of_two)	//NPOT is a simple extension that relaxes errors.
	{
		TRACE(("dbg: GL_Upload32: GL_ARB_texture_non_power_of_two\n"));
		scaled_width = width;
		scaled_height = height;
	}
	else
	{
		for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1)
			;
		for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1)
			;
	}

	scaled_width >>= (int)gl_picmip.value;
	scaled_height >>= (int)gl_picmip.value;

	if (gl_max_size.value)
	{
		if (scaled_width > gl_max_size.value)
			scaled_width = gl_max_size.value;
		if (scaled_height > gl_max_size.value)
			scaled_height = gl_max_size.value;
	}

	if (scaled_width < 1)
		scaled_width = 1;
	if (scaled_height < 1)
		scaled_height = 1;

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

//	if (is_overriden)
//		nmap = (qbyte *)genNormalMap(scaled,scaled_width,scaled_height,10.0f); 
//	else
		nmap = (qbyte *)genNormalMap(scaled,scaled_width,scaled_height,4.0f);

	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA
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

			glTexImage2D (GL_TEXTURE_2D, miplevel, GL_RGBA, scaled_width, scaled_height, 0, GL_RGBA,
						GL_UNSIGNED_BYTE, nmap);
			//glTexImage2D (GL_TEXTURE_2D, miplevel, GL_RGBA, scaled_width, scaled_height, 0, GL_RGBA,
			//			GL_UNSIGNED_BYTE, genNormalMap(scaled,scaled_width,scaled_height,4.0f));
		}
	}

	if (mipmap)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
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
	if (supported_GL_ARB_texture_non_power_of_two)	//NPOT is a simple extension that relaxes errors.
	{
		TRACE(("dbg: GL_Upload32: GL_ARB_texture_non_power_of_two\n"));
		scaled_width = width;
		scaled_height = height;
	}
	else
	{
		for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1)
			;
		for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1)
			;
	}

	scaled_width >>= (int)gl_picmip.value;
	scaled_height >>= (int)gl_picmip.value;

	if (gl_max_size.value)
	{
		if (scaled_width > gl_max_size.value)
			scaled_width = gl_max_size.value;
		if (scaled_height > gl_max_size.value)
			scaled_height = gl_max_size.value;
	}

	if (scaled_width < 1)
		scaled_width = 1;
	if (scaled_height < 1)
		scaled_height = 1;

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
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
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

void GL_Upload8 (qbyte *data, int width, int height,  qboolean mipmap, qboolean alpha)
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
				noalpha = false;
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
		if (s&3)
			Sys_Error ("GL_Upload8: s&3");
		for (i=0 ; i<s ; i+=4)
		{
			trans[i] = d_8to24rgbtable[data[i]];
			trans[i+1] = d_8to24rgbtable[data[i+1]];
			trans[i+2] = d_8to24rgbtable[data[i+2]];
			trans[i+3] = d_8to24rgbtable[data[i+3]];
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

	GL_Upload32 (NULL, trans, width, height, mipmap, alpha);
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

	s = width*height;
	if (s > sizeofuploadmemorybufferintermediate/4)
		Sys_Error("GL_Upload8Pal24: image too big (%i*%i)", width, height);

	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
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
	GL_Upload32 (NULL, (unsigned*)trans, width, height, mipmap, alpha);
}
/*
================
GL_LoadTexture
================
*/
int GL_LoadTexture (char *identifier, int width, int height, qbyte *data, qboolean mipmap, qboolean alpha)
{
	gltexture_t	*glt;

	// see if the texture is allready present
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

	Hash_Add2(&gltexturetable, glt->identifier, glt, (bucket_t*)(glt+1));

	GL_Bind(texture_extension_number );

	GL_Upload8 (data, width, height, mipmap, alpha);

	texture_extension_number++;

	return texture_extension_number-1;
}

int GL_LoadTextureFB (char *identifier, int width, int height, qbyte *data, qboolean mipmap, qboolean alpha)
{
	int			i;
	gltexture_t	*glt;

	// see if the texture is allready present
	if (identifier[0])
	{
		glt = GL_MatchTexture(identifier, 8, width, height);
		if (glt)
			return glt->texnum;
	}

	glt = BZ_Malloc(sizeof(*glt)+sizeof(bucket_t));
	glt->next = gltextures;
	gltextures = glt;

	for (i = 0; i < width*height; i++)
		if (data[i] > 255-vid.fullbright)
			break;

	if (i == width*height)
		return 0;	//none found, don't bother uploading.

	strcpy (glt->identifier, identifier);
	glt->texnum = texture_extension_number;
	glt->width = width;
	glt->height = height;
	glt->bpp = 8;
	glt->mipmap = mipmap;

	Hash_Add2(&gltexturetable, glt->identifier, glt, (bucket_t*)(glt+1));

	GL_Bind(texture_extension_number );

	GL_Upload8FB (data, width, height, mipmap);

	texture_extension_number++;

	return texture_extension_number-1;
}

int GL_LoadTexture8Pal24 (char *identifier, int width, int height, qbyte *data, qbyte *palette24, qboolean mipmap, qboolean alpha)
{
	gltexture_t	*glt;

		// see if the texture is allready present
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

	Hash_Add2(&gltexturetable, glt->identifier, glt, (bucket_t*)(glt+1));

	GL_Bind(texture_extension_number );

	GL_Upload8Pal24 (data, palette24, width, height, mipmap, alpha);

	texture_extension_number++;

	return texture_extension_number-1;
}

int GL_LoadTexture32 (char *identifier, int width, int height, unsigned *data, qboolean mipmap, qboolean alpha)
{
//	qboolean	noalpha;
//	int			p, s;
	gltexture_t	*glt;

	// see if the texture is allready present
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

	Hash_Add2(&gltexturetable, glt->identifier, glt, (bucket_t*)(glt+1));

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


	// see if the texture is allready present
	if (name[0])
	{
		int num = GL_FindTexture(name);
		if (num != -1)
			return num;
	}
	else
		return 0;


	_snprintf(inname, sizeof(inname)-1, "tex/%s.tex", name);
	file = COM_LoadFile(inname, 5);	
	if (!file)
		return 0;
	
	glt = BZ_Malloc(sizeof(*glt)+sizeof(bucket_t));
	glt->next = gltextures;
	gltextures = glt;

	strcpy (glt->identifier, name);
	glt->texnum = texture_extension_number;
	glt->bpp = 32;

	Hash_Add2(&gltexturetable, glt->identifier, glt, (bucket_t*)(glt+1));

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

	// see if the texture is allready present
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

	Hash_Add2(&gltexturetable, glt->identifier, glt, (bucket_t*)(glt+1));

//	if (!isDedicated)
	{
		GL_Bind(texture_extension_number );

		GL_Upload8Grey (data, width, height, mipmap);
	}

	texture_extension_number++;

	return texture_extension_number-1;
}

int GL_LoadTexture8Bump (char *identifier, int width, int height, unsigned char *data, qboolean mipmap)
{
//	qboolean	noalpha;
	//	int			p, s;
	gltexture_t	*glt;

	// see if the texture is allready present
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

	Hash_Add2(&gltexturetable, glt->identifier, glt, (bucket_t*)(glt+1));

//	if (!isDedicated)
	{
		GL_Bind(texture_extension_number );

		GL_UploadBump (data, width, height, mipmap);
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
