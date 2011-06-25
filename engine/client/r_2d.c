#include "quakedef.h"
#ifndef SERVERONLY
#include "shader.h"
#include "gl_draw.h"

texid_t missing_texture;

texid_t translate_texture;
shader_t *translate_shader;

texid_t ch_int_texture;
vec3_t ch_color;
shader_t *shader_crosshair;

static mpic_t *conback;
static mpic_t *draw_backtile;
static shader_t *shader_draw_fill, *shader_draw_fill_trans;
mpic_t		*draw_disc;

shader_t *shader_brighten;
shader_t *shader_polyblend;
shader_t *shader_menutint;

static mesh_t	draw_mesh;
static vecV_t	draw_mesh_xyz[4];
vec2_t	draw_mesh_st[4];
static avec4_t	draw_mesh_colors[4];
index_t r_quad_indexes[6] = {0, 1, 2, 2, 3, 0};
unsigned int r2d_be_flags;

extern cvar_t scr_conalpha;
extern cvar_t gl_conback;
extern cvar_t gl_font;
extern cvar_t gl_contrast;
extern cvar_t gl_screenangle;
extern cvar_t vid_conautoscale;
extern cvar_t vid_conheight;
extern cvar_t vid_conwidth;
void R2D_Font_Callback(struct cvar_s *var, char *oldvalue);
void R2D_Conautoscale_Callback(struct cvar_s *var, char *oldvalue);
void R2D_ScreenAngle_Callback(struct cvar_s *var, char *oldvalue);
void R2D_Conheight_Callback(struct cvar_s *var, char *oldvalue);
void R2D_Conwidth_Callback(struct cvar_s *var, char *oldvalue);

extern cvar_t crosshair;
extern cvar_t crosshaircolor;
extern cvar_t crosshairsize;
extern cvar_t crosshairimage;
extern cvar_t crosshairalpha;
void R2D_Crosshair_Callback(struct cvar_s *var, char *oldvalue);
void R2D_CrosshairImage_Callback(struct cvar_s *var, char *oldvalue);
void R2D_CrosshairColor_Callback(struct cvar_s *var, char *oldvalue);


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
	conback = NULL;

	Shader_Init();

	BE_Init();
	draw_mesh.istrifan = true;
	draw_mesh.numvertexes = 4;
	draw_mesh.numindexes = 6;
	draw_mesh.xyz_array = draw_mesh_xyz;
	draw_mesh.st_array = draw_mesh_st;
	draw_mesh.colors4f_array = draw_mesh_colors;
	draw_mesh.indexes = r_quad_indexes;


	Font_Init();

#ifdef _MSC_VER
#pragma message("Fixme: move conwidth handling into here")
#endif

	missing_texture = R_LoadTexture8("no_texture", 16, 16, (unsigned char*)r_notexture_mip + r_notexture_mip->offsets[0], IF_NOALPHA|IF_NOGAMMA, 0);
	translate_texture = r_nulltex;
	ch_int_texture = r_nulltex;

	draw_backtile = R_RegisterShader("gfx/backtile.lmp",
		"{\n"
			"if $nofixed\n"
			"[\n"
				"program default2d\n"
			"]\n"
			"nomipmaps\n"
			"{\n"
				"map $diffuse\n"
			"}\n"
		"}\n");
	if (!TEXVALID(draw_backtile->defaulttextures.base))
		draw_backtile->defaulttextures.base = R_LoadHiResTexture("gfx/backtile", NULL, IF_NOPICMIP|IF_NOMIPMAP);
	if (!TEXVALID(draw_backtile->defaulttextures.base))
		draw_backtile->defaulttextures.base = R_LoadHiResTexture("gfx/menu/backtile", NULL, IF_NOPICMIP|IF_NOMIPMAP);

	shader_draw_fill = R_RegisterShader("fill_opaque",
		"{\n"
			"program defaultfill\n"
			"{\n"
				"map $whiteimage\n"
				"rgbgen vertex\n"
			"}\n"
		"}\n");
	shader_draw_fill_trans = R_RegisterShader("fill_trans",
		"{\n"
			"program defaultfill\n"
			"{\n"
				"map $whiteimage\n"
				"rgbgen vertex\n"
				"alphagen vertex\n"
				"blendfunc blend\n"
			"}\n"
		"}\n");
	shader_brighten = R_RegisterShader("constrastshader",
		"{\n"
			"program defaultfill\n"
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
			"program defaultfill\n"
			"{\n"
				"map $whiteimage\n"
				"blendfunc gl_src_alpha gl_one_minus_src_alpha\n"
				"rgbgen vertex\n"
				"alphagen vertex\n"
			"}\n"
		"}\n"
	);
	shader_menutint = R_RegisterShader("menutint_glsl",
		"{\n"
			"if $glsl && gl_menutint_shader != 0\n"
			"[\n"
				"glslprogram\n"
				"{\n"
			"#ifdef VERTEX_SHADER\n"
			"\
					attribute vec2 v_texcoord;\
					varying vec2 texcoord;\
					uniform vec3 rendertexturescale;\
					void main(void)\
					{\
						texcoord.x = v_texcoord.x*rendertexturescale.x;\
						texcoord.y = (1.0-v_texcoord.y)*rendertexturescale.y;\
						gl_Position = ftetransform();\
					}\
			\n"
			"#endif\n"
			"#ifdef FRAGMENT_SHADER\n"
			"\
					varying vec2 texcoord;\
					uniform vec3 colorparam;\
					uniform sampler2D s_t0;\
					uniform int invert;\
					const vec3 lumfactors = vec3(0.299, 0.587, 0.114);\
					const vec3 invertvec = vec3(1.0, 1.0, 1.0);\
					void main(void)\
					{\
						vec3 texcolor = texture2D(s_t0, texcoord).rgb;\
						float luminance = dot(lumfactors, texcolor);\
						texcolor = vec3(luminance, luminance, luminance);\
						texcolor *= colorparam;\
						texcolor = (invert > 0) ? (invertvec - texcolor) : texcolor;\
						gl_FragColor = vec4(texcolor, 1.0);\
					}\n"
			"#endif\n"
				"}\n"
				"param cvari r_menutint_inverse invert\n"
				"param cvar3f r_menutint colorparam\n"
				"param rendertexturescale rendertexturescale\n"

				"{\n"
					"map $currentrender\n"
				"}\n"
			"][\n"
				"{\n"
					"map $whitetexture\n"
					"blendfunc gl_dst_color gl_zero\n"
					"rgbgen const $r_menutint\n"
				"}\n"
			"]\n"
		"}\n"
	);
	shader_crosshair = R_RegisterShader("crosshairshader",
		"{\n"
			"if $nofixed\n"
			"[\n"
				"program default2d\n"
			"]\n"
			"nomipmaps\n"
			"{\n"
				"map $diffuse\n"
				"blendfunc blend\n"
				"rgbgen vertex\n"
				"alphagen vertex\n"
			"}\n"
		"}\n"
		);


	Cvar_Hook(&gl_font, R2D_Font_Callback);
	Cvar_Hook(&vid_conautoscale, R2D_Conautoscale_Callback);
	Cvar_Hook(&gl_screenangle, R2D_ScreenAngle_Callback);
	Cvar_Hook(&vid_conheight, R2D_Conheight_Callback);
	Cvar_Hook(&vid_conwidth, R2D_Conwidth_Callback);

	Cvar_Hook(&crosshair, R2D_Crosshair_Callback);
	Cvar_Hook(&crosshairimage, R2D_CrosshairImage_Callback);
	Cvar_Hook(&crosshaircolor, R2D_CrosshairColor_Callback);

	Cvar_ForceCallback(&gl_conback);
	Cvar_ForceCallback(&vid_conautoscale);
	Cvar_ForceCallback(&gl_font);

	Cvar_ForceCallback(&crosshair);
	Cvar_ForceCallback(&crosshaircolor);

#ifdef PLUGINS
	Plug_DrawReloadImages();
#endif
}

mpic_t	*R2D_SafeCachePic (char *path)
{
	shader_t *s;
	if (!qrenderer)
		return NULL;
	s = R_RegisterPic(path);
	if (s->flags & SHADER_NOIMAGE)
		return NULL;
	return s;
}


char *failedpic;	//easier this way
mpic_t *R2D_SafePicFromWad (char *name)
{
	char newname[32];
	shader_t *s;
	snprintf(newname, sizeof(newname), "gfx/%s.lmp", name);
	s = R_RegisterPic(newname);
	if (!(s->flags & SHADER_NOIMAGE))
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
	draw_mesh_colors[0][0] = host_basepal[i*3+0]/255.0;
	draw_mesh_colors[0][1] = host_basepal[i*3+1]/255.0;
	draw_mesh_colors[0][2] = host_basepal[i*3+2]/255.0;
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

	BE_DrawMesh_Single(pic, &draw_mesh, NULL, &pic->defaulttextures, r2d_be_flags);
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
		BE_DrawMesh_Single(shader_draw_fill_trans, &draw_mesh, NULL, &shader_draw_fill_trans->defaulttextures, r2d_be_flags);
	else
		BE_DrawMesh_Single(shader_draw_fill, &draw_mesh, NULL, &shader_draw_fill->defaulttextures, r2d_be_flags);
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

/* this is an ugly special case drawing func that's only used for the player color selection menu */
void R2D_TransPicTranslate (int x, int y, int width, int height, qbyte *pic, qbyte *translation)
{
	int				v, u, c;
	unsigned		trans[64*64], *dest;
	qbyte			*src;
	int				p;

	c = width * height;

	dest = trans;
	for (v=0 ; v<64 ; v++, dest += 64)
	{
		src = &pic[ ((v*height)>>6) *width];
		for (u=0 ; u<64 ; u++)
		{
			p = src[(u*width)>>6];
			if (p == 255)
				dest[u] = 0x0;
			else
				dest[u] =  d_8to24rgbtable[translation[p]];
		}
	}

	if (!TEXVALID(translate_texture))
	{
		translate_texture = R_AllocNewTexture(64, 64);
		translate_shader = R_RegisterShader("translatedpic", "{\n"
			"if $nofixed\n"
			"[\n"
				"program default2d\n"
			"]\n"
			"nomipmaps\n"
			"{\n"
				"map $diffuse\n"
				"blendfunc blend\n"
			"}\n"
		"}\n");
		translate_shader->defaulttextures.base = translate_texture;
	}
	/* could avoid reuploading already translated textures but this func really isn't used enough anyway */
	R_Upload(translate_texture, NULL, TF_RGBA32, trans, NULL, 64, 64, IF_NOMIPMAP|IF_NOGAMMA);
	R2D_ScalePic(x, y, width, height, translate_shader);
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

	w = vid.width;
	h = vid.height;

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
		R2D_ScalePic(0, lastline-(int)vid.height, w, h, conback);
	}
	else
	{
		R2D_ImageColours(1, 1, 1, a);
		R2D_ScalePic (0, lastline - (int)vid.height, w, h, conback);
		R2D_ImageColours(1, 1, 1, 1);
	}
}

void R2D_EditorBackground (void)
{
	R2D_ScalePic(0, 0, vid.width, vid.height, conback);
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

	BE_DrawMesh_Single(draw_backtile, &draw_mesh, NULL, &draw_backtile->defaulttextures, r2d_be_flags);
}

void R2D_Conback_Callback(struct cvar_s *var, char *oldvalue)
{
	if (qrenderer == QR_NONE)
	{
		conback = NULL;
		return;
	}

	if (*var->string)
		conback = R_RegisterPic(var->string);
	if (!conback || conback->flags & SHADER_NOIMAGE)
	{
		conback = R_RegisterCustom("console", NULL, NULL);
		if (!conback || conback->flags & SHADER_NOIMAGE)
		{
			if (M_GameType() == MGT_HEXEN2)
				conback = R_RegisterPic("gfx/menu/conback.lmp");
			else if (M_GameType() == MGT_QUAKE2)
				conback = R_RegisterPic("pics/conback.pcx");
			else
				conback = R_RegisterPic("gfx/conback.lmp");
		}
	}
}

void R2D_Font_Callback(struct cvar_s *var, char *oldvalue)
{
	if (font_conchar)
		Font_Free(font_conchar);

	if (qrenderer == QR_NONE)
	{
		font_conchar = NULL;
		return;
	}

	font_conchar = Font_LoadFont(8*vid.rotpixelheight/vid.height, var->string);
	if (!font_conchar && *var->string)
		font_conchar = Font_LoadFont(8*vid.rotpixelheight/vid.height, "");
}

// console size manipulation callbacks
void R2D_Console_Resize(void)
{
	extern cvar_t gl_font;
	extern cvar_t vid_conwidth, vid_conheight;
	int cwidth, cheight;
	float xratio;
	float yratio=0;
	float ang, rad;
	extern cvar_t gl_screenangle;

	ang = (gl_screenangle.value>0?(gl_screenangle.value+45):(gl_screenangle.value-45))/90;
	ang = (int)ang * 90;
	if (ang)
	{
		rad = (ang * M_PI) / 180;
		vid.rotpixelwidth = fabs(cos(rad)) * (vid.pixelwidth) + fabs(sin(rad)) * (vid.pixelheight);
		vid.rotpixelheight = fabs(sin(rad)) * (vid.pixelwidth) + fabs(cos(rad)) * (vid.pixelheight);
	}
	else
	{
		vid.rotpixelwidth = vid.pixelwidth;
		vid.rotpixelheight = vid.pixelheight;
	}

	cwidth = vid_conwidth.value;
	cheight = vid_conheight.value;

	xratio = vid_conautoscale.value;
	if (xratio > 0)
	{
		char *s = strchr(vid_conautoscale.string, ' ');
		if (s)
			yratio = atof(s + 1);

		if (yratio <= 0)
			yratio = xratio;

		xratio = 1 / xratio;
		yratio = 1 / yratio;

		//autoscale overrides conwidth/height (without actually changing them)
		cwidth = vid.rotpixelwidth;
		cheight = vid.rotpixelheight;
	}
	else
	{
		xratio = 1;
		yratio = 1;
	}


	if (!cwidth)
		cwidth = vid.rotpixelwidth;
	if (!cheight)
		cheight = vid.rotpixelheight;

	cwidth*=xratio;
	cheight*=yratio;

	if (cwidth < 320)
		cwidth = 320;
	if (cheight < 200)
		cheight = 200;

	vid.width = cwidth;
	vid.height = cheight;

	vid.recalc_refdef = true;

	if (font_tiny)
		Font_Free(font_tiny);
	font_tiny = NULL;
	if (font_conchar)
		Font_Free(font_conchar);
	font_conchar = NULL;

	Cvar_ForceCallback(&gl_font);

#ifdef PLUGINS
	Plug_ResChanged();
#endif
}

void R2D_Conheight_Callback(struct cvar_s *var, char *oldvalue)
{
	if (var->value > 1536)	//anything higher is unreadable.
	{
		Cvar_ForceSet(var, "1536");
		return;
	}
	if (var->value < 200 && var->value)	//lower would be wrong
	{
		Cvar_ForceSet(var, "200");
		return;
	}

	R2D_Console_Resize();
}

void R2D_Conwidth_Callback(struct cvar_s *var, char *oldvalue)
{
	//let let the user be too crazy
	if (var->value > 2048)	//anything higher is unreadable.
	{
		Cvar_ForceSet(var, "2048");
		return;
	}
	if (var->value < 320 && var->value)	//lower would be wrong
	{
		Cvar_ForceSet(var, "320");
		return;
	}

	R2D_Console_Resize();
}

void R2D_Conautoscale_Callback(struct cvar_s *var, char *oldvalue)
{
	R2D_Console_Resize();
}

void R2D_ScreenAngle_Callback(struct cvar_s *var, char *oldvalue)
{
	R2D_Console_Resize();
}


/*
============
R_PolyBlend
============
*/
//bright flashes and stuff
void R2D_PolyBlend (void)
{
	if (!sw_blend[3])
		return;

	if (r_refdef.flags & Q2RDF_NOWORLDMODEL)
		return;

	R2D_ImageColours (sw_blend[0], sw_blend[1], sw_blend[2], sw_blend[3]);
	R2D_ScalePic(0, 0, vid.width, vid.height, shader_polyblend);
	R2D_ImageColours (1, 1, 1, 1);
}

//for lack of hardware gamma
void R2D_BrightenScreen (void)
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
	R2D_ImageColours (1, 1, 1, 1);

	RSpeedEnd(RSPEED_PALETTEFLASHES);
}

//for menus
void R2D_FadeScreen (void)
{
	R2D_ScalePic(0, 0, vid.width, vid.height, shader_menutint);

	Sbar_Changed();
}

//crosshairs
#define CS_HEIGHT 8
#define CS_WIDTH 8
unsigned char crosshair_pixels[] =
{
	// 2 + (spaced)
	0x08,
	0x00,
	0x08,
	0x55,
	0x08,
	0x00,
	0x08,
	0x00,
	// 3 +
	0x00,
	0x08,
	0x08,
	0x36,
	0x08,
	0x08,
	0x00,
	0x00,
	// 4 X
	0x00,
	0x22,
	0x14,
	0x00,
	0x14,
	0x22,
	0x00,
	0x00,
	// 5 X (spaced)
	0x41,
	0x00,
	0x14,
	0x00,
	0x14,
	0x00,
	0x41,
	0x00,
	// 6 diamond (unconnected)
	0x00,
	0x14,
	0x22,
	0x00,
	0x22,
	0x14,
	0x00,
	0x00,
	// 7 diamond
	0x00,
	0x08,
	0x14,
	0x22,
	0x14,
	0x08,
	0x00,
	0x00,
	// 8 four points
	0x00,
	0x08,
	0x00,
	0x22,
	0x00,
	0x08,
	0x00,
	0x00,
	// 9 three points
	0x00,
	0x00,
	0x08,
	0x00,
	0x22,
	0x00,
	0x00,
	0x00,
	// 10
	0x08,
	0x2a,
	0x00,
	0x63,
	0x00,
	0x2a,
	0x08,
	0x00,
	// 11
	0x49,
	0x2a,
	0x00,
	0x63,
	0x00,
	0x2a,
	0x49,
	0x00,
	// 12 horizontal line
	0x00,
	0x00,
	0x00,
	0x77,
	0x00,
	0x00,
	0x00,
	0x00,
	// 13 vertical line
	0x08,
	0x08,
	0x08,
	0x00,
	0x08,
	0x08,
	0x08,
	0x00,
	// 14 larger +
	0x08,
	0x08,
	0x08,
	0x77,
	0x08,
	0x08,
	0x08,
	0x00,
	// 15 angle
	0x00,
	0x00,
	0x00,
	0x70,
	0x08,
	0x08,
	0x08,
	0x00,
	// 16 dot
	0x00,
	0x00,
	0x00,
	0x08,
	0x00,
	0x00,
	0x00,
	0x00,
	// 17 weird angle thing
	0x00,
	0x00,
	0x00,
	0x38,
	0x48,
	0x08,
	0x10,
	0x00,
	// 18 circle w/ dot
	0x00,
	0x00,
	0x00,
	0x6b,
	0x41,
	0x63,
	0x3e,
	0x08,
	// 19 tripoint
	0x08,
	0x08,
	0x08,
	0x00,
	0x14,
	0x22,
	0x41,
	0x00,
};

void R2D_Crosshair_Update(void)
{
	int crossdata[CS_WIDTH*CS_HEIGHT];
	int c;
	int w, h;
	unsigned char *x;

	c = crosshair.ival;
	if (!crosshairimage.string)
		return;
	else if (crosshairimage.string[0] && c == 1)
	{
		shader_crosshair->defaulttextures.base = R_LoadHiResTexture (crosshairimage.string, "crosshairs", IF_NOMIPMAP|IF_NOGAMMA);
		if (TEXVALID(shader_crosshair->defaulttextures.base))
			return;
	}
	else if (c <= 1)
		return;

	c -= 2;
	c = c % (sizeof(crosshair_pixels) / (CS_HEIGHT*sizeof(*crosshair_pixels)));

	if (!TEXVALID(ch_int_texture))
		ch_int_texture = R_AllocNewTexture(CS_WIDTH, CS_HEIGHT);
	shader_crosshair->defaulttextures.base = ch_int_texture;

	Q_memset(crossdata, 0, sizeof(crossdata));

	x = crosshair_pixels + (CS_HEIGHT * c);
	for (h = 0; h < CS_HEIGHT; h++)
	{
		int pix = x[h];
		for (w = 0; w < CS_WIDTH; w++)
		{
			if (pix & 0x1)
				crossdata[CS_WIDTH * h + w] = 0xffffffff;
			pix >>= 1;
		}
	}

	R_Upload(ch_int_texture, NULL, TF_RGBA32, crossdata, NULL, CS_WIDTH, CS_HEIGHT, IF_NOMIPMAP|IF_NOGAMMA);

}

void R2D_CrosshairImage_Callback(struct cvar_s *var, char *oldvalue)
{
	R2D_Crosshair_Update();
}

void R2D_Crosshair_Callback(struct cvar_s *var, char *oldvalue)
{
	R2D_Crosshair_Update();
}

void R2D_CrosshairColor_Callback(struct cvar_s *var, char *oldvalue)
{
	SCR_StringToRGB(var->string, ch_color, 255);

	ch_color[0] = bound(0, ch_color[0], 1);
	ch_color[1] = bound(0, ch_color[1], 1);
	ch_color[2] = bound(0, ch_color[2], 1);
}

void R2D_DrawCrosshair(void)
{
	int x, y;
	int sc;
	float sx, sy, sizex, sizey;

	float size, chc;

	if (crosshair.ival < 1)
		return;

	// old style
	if (crosshair.ival == 1 && !crosshairimage.string[0])
	{
		// adjust console crosshair scale to match default
		size = crosshairsize.value / 8;
		if (size == 0)
			size = 8;
		else if (size < 0)
			size = -size;
		for (sc = 0; sc < cl.splitclients; sc++)
		{
			SCR_CrosshairPosition(sc, &x, &y);
			Font_BeginScaledString(font_conchar, x, y, &sx, &sy);
			sizex = Font_CharWidth('+' | 0xe000 | CON_WHITEMASK) * size;
			sizey = Font_CharHeight() * size;
			sx -= sizex/2;
			sy -= sizey/2;
			Font_ForceColour(ch_color[0], ch_color[1], ch_color[2], crosshairalpha.value);
			Font_DrawScaleChar(sx, sy, sizex, sizey, '+' | 0xe000 | CON_WHITEMASK);
			Font_InvalidateColour();
			Font_EndString(font_conchar);
		}
		return;
	}

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
		chc = size / 16.0;
	}

	sizex = (int)sizex;
	sizex = ((sizex)*(int)vid.width) / (float)vid.rotpixelwidth;

	sizey = (int)sizey;
	sizey = ((sizey)*(int)vid.height) / (float)vid.rotpixelheight;

	R2D_ImageColours(ch_color[0], ch_color[1], ch_color[2], crosshairalpha.value);
	for (sc = 0; sc < cl.splitclients; sc++)
	{
		SCR_CrosshairPosition(sc, &x, &y);

		//translate to pixel coord, for rounding
		x = ((x-sizex-chc)*vid.rotpixelwidth) / (float)vid.width;
		y = ((y-sizey-chc)*vid.rotpixelheight) / (float)vid.height;

		//translate to screen coords
		sx = ((x)*(int)vid.width) / (float)vid.rotpixelwidth;
		sy = ((y)*(int)vid.height) / (float)vid.rotpixelheight;

		R2D_Image(sx, sy, sizex*2, sizey*2, 0, 0, 1, 1, shader_crosshair);
	}
	R2D_ImageColours(1, 1, 1, 1);
}


#endif
