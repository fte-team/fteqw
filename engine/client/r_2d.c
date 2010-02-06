#include "quakedef.h"
#ifndef SERVERONLY
#include "shader.h"
#include "gl_draw.h"

texid_t missing_texture;
static mpic_t *conback;
static mpic_t *draw_backtile;
static mpic_t *draw_fill, *draw_fill_trans;
mpic_t		*draw_disc;

static mesh_t	draw_mesh;
static vecV_t	draw_mesh_xyz[4];
static vec2_t	draw_mesh_st[4];
static avec4_t	draw_mesh_colors[4];
static index_t r_quad_indexes[6] = {0, 1, 2, 0, 2, 3};

extern cvar_t scr_conalpha;
extern cvar_t gl_conback;
extern cvar_t gl_font;

void R2D_Conback_Callback(struct cvar_s *var, char *oldvalue);


//We need this for minor things though, so we'll just use the slow accurate method.
//this is unlikly to be called too often.			
qbyte GetPaletteIndex(int red, int green, int blue)
{
	//slow, horrible method.
	{
		int i, best=15;
		int bestdif=256*256*256, curdif;
		extern qbyte *host_basepal;
		qbyte *pa;

	#define _abs(x) ((x)*(x))

		pa = host_basepal;
		for (i = 0; i < 256; i++, pa+=3)
		{
			curdif = _abs(red - pa[0]) + _abs(green - pa[1]) + _abs(blue - pa[2]);
			if (curdif < bestdif)
			{
				if (curdif<1)
					return i;
				bestdif = curdif;
				best = i;
			}
		}
		return best;
	}
}

/*
Iniitalise the 2d rendering functions (including font).
Image loading code must be ready for use at this point.
*/
void R2D_Init(void)
{
	BE_Init();
	draw_mesh.numvertexes = 4;
	draw_mesh.numindexes = 6;
	draw_mesh.xyz_array = draw_mesh_xyz;
	draw_mesh.st_array = draw_mesh_st;
	draw_mesh.colors4f_array = draw_mesh_colors;
	draw_mesh.indexes = r_quad_indexes;


	Font_Init();

#pragma message("Fixme: move conwidth handling into here")


	if (font_conchar)
		Font_Free(font_conchar);
	font_conchar = Font_LoadFont(8*vid.pixelheight/vid.height, gl_font.string);
	if (!font_conchar && *gl_font.string)
		font_conchar = Font_LoadFont(8*vid.pixelheight/vid.height, "");

	missing_texture = R_LoadTexture8("no_texture", 16, 16, (unsigned char*)r_notexture_mip + r_notexture_mip->offsets[0], IF_NOALPHA|IF_NOGAMMA, 0);

	draw_backtile = Draw_SafePicFromWad ("backtile");
	if (!draw_backtile)
		draw_backtile = Draw_SafeCachePic ("gfx/menu/backtile.lmp");

	draw_fill = R_RegisterShader("fill_opaque",
		"{\n"
			"{\n"
				"map $whiteimage\n"
				"rgbgen vertex\n"
			"}\n"
		"}\n");
	draw_fill_trans = R_RegisterShader("fill_trans",
		"{\n"
			"{\n"
				"map $whiteimage\n"
				"rgbgen vertex\n"
				"alphagen vertex\n"
				"blendfunc blend\n"
			"}\n"
		"}\n");

	Cvar_Hook(&gl_conback, R2D_Conback_Callback);
}

mpic_t	*R2D_SafeCachePic (char *path)
{
	shader_t *s = R_RegisterPic(path);
	if (s->width)
		return s;
	return NULL;
}


char *failedpic;	//easier this way
mpic_t *R2D_SafePicFromWad (char *name)
{
	char newname[32];
	shader_t *s;
	snprintf(newname, sizeof(newname), "gfx/%s.lmp", name);
	s = R_RegisterPic(newname);
	if (s->width)
		return s;
	failedpic = name;
	return NULL;
}


void R2D_ImageColours(float r, float g, float b, float a)
{
	draw_mesh_colors[0][0] = r;
	draw_mesh_colors[0][1] = g;
	draw_mesh_colors[0][2] = b;
	draw_mesh_colors[0][3] = a;
	Vector4Copy(draw_mesh_colors[0], draw_mesh_colors[1]);
	Vector4Copy(draw_mesh_colors[0], draw_mesh_colors[2]);
	Vector4Copy(draw_mesh_colors[0], draw_mesh_colors[3]);
}
void R2D_ImagePaletteColour(unsigned int i, float a)
{
	draw_mesh_colors[0][0] = host_basepal[i*3+0]/255;
	draw_mesh_colors[0][1] = host_basepal[i*3+1]/255;
	draw_mesh_colors[0][2] = host_basepal[i*3+2]/255;
	draw_mesh_colors[0][3] = a;
	Vector4Copy(draw_mesh_colors[0], draw_mesh_colors[1]);
	Vector4Copy(draw_mesh_colors[0], draw_mesh_colors[2]);
	Vector4Copy(draw_mesh_colors[0], draw_mesh_colors[3]);
}

//awkward and weird to use
void R2D_Image(float x, float y, float w, float h, float s1, float t1, float s2, float t2, mpic_t *pic)
{
	if (!pic)
		return;
/*
	if (w == 0 && h == 0)
	{
		w = pic->width;
		h = pic->height;
	}
*/
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

	BE_DrawMeshChain(pic, &draw_mesh, NULL, &pic->defaulttextures);
}

/*draws a block of the current colour on the screen*/
void R2D_FillBlock(int x, int y, int w, int h)
{
	draw_mesh_xyz[0][0] = x;
	draw_mesh_xyz[0][1] = y;

	draw_mesh_xyz[1][0] = x+w;
	draw_mesh_xyz[1][1] = y;

	draw_mesh_xyz[2][0] = x+w;
	draw_mesh_xyz[2][1] = y+h;

	draw_mesh_xyz[3][0] = x;
	draw_mesh_xyz[3][1] = y+h;

	if (draw_mesh_colors[0][3] != 1)
		BE_DrawMeshChain(draw_fill_trans, &draw_mesh, NULL, &draw_fill_trans->defaulttextures);
	else
		BE_DrawMeshChain(draw_fill, &draw_mesh, NULL, &draw_fill->defaulttextures);
}

void R2D_ScalePic (int x, int y, int width, int height, mpic_t *pic)
{
	R2D_Image(x, y, width, height, 0, 0, 1, 1, pic);
}

void R2D_SubPic(int x, int y, int width, int height, mpic_t *pic, int srcx, int srcy, int srcwidth, int srcheight)
{
	float newsl, newtl, newsh, newth;

	newsl = (srcx)/(float)srcwidth;
	newsh = newsl + (width)/(float)srcwidth;

	newtl = (srcy)/(float)srcheight;
	newth = newtl + (height)/(float)srcheight;

	R2D_Image(x, y, width, height, newsl, newtl, newsh, newth, pic);
}

/*
================
Draw_ConsoleBackground

================
*/
void R2D_ConsoleBackground (int firstline, int lastline, qboolean forceopaque)
{
	float a;
	int w, h;
	if (!conback)
		return;

	w = vid.conwidth;
	h = vid.conheight;

	if (forceopaque)
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
		h>>=1;
		w>>=1;
	}
	if (a >= 1)
	{
		R2D_ImageColours(1, 1, 1, 1);
		R2D_ScalePic(0, lastline-(int)vid.conheight, w, h, conback);
	}
	else
	{
		R2D_ImageColours(1, 1, 1, a);
		R2D_ScalePic (0, lastline - (int)vid.conheight, w, h, conback);
		R2D_ImageColours(1, 1, 1, 1);
	}
}

void R2D_EditorBackground (void)
{
	R2D_ScalePic(0, 0, vid.conwidth, vid.conheight, conback);
}

/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void R2D_TileClear (int x, int y, int w, int h)
{
	float newsl, newsh, newtl, newth;
	newsl = (x)/(float)64;
	newsh = newsl + (w)/(float)64;

	newtl = (y)/(float)64;
	newth = newtl + (h)/(float)64;

	R2D_ImageColours(1,1,1,1);

	draw_mesh_xyz[0][0] = x;
	draw_mesh_xyz[0][1] = y;
	draw_mesh_st[0][0] = newsl;
	draw_mesh_st[0][1] = newtl;

	draw_mesh_xyz[1][0] = x+w;
	draw_mesh_xyz[1][1] = y;
	draw_mesh_st[1][0] = newsh;
	draw_mesh_st[1][1] = newtl;

	draw_mesh_xyz[2][0] = x+w;
	draw_mesh_xyz[2][1] = y+h;
	draw_mesh_st[2][0] = newsh;
	draw_mesh_st[2][1] = newth;

	draw_mesh_xyz[3][0] = x;
	draw_mesh_xyz[3][1] = y+h;
	draw_mesh_st[3][0] = newsl;
	draw_mesh_st[3][1] = newth;

	BE_DrawMeshChain(draw_backtile, &draw_mesh, NULL, NULL);
}

void R2D_Conback_Callback(struct cvar_s *var, char *oldvalue)
{
	if (*var->string)
		conback = R_RegisterPic(var->string);
	if (!conback || !conback->width)
		conback = R_RegisterCustom("console", NULL, NULL);
	if (!conback || !conback->width)
		conback = R_RegisterPic("gfx/conback.lmp");
}

#endif
