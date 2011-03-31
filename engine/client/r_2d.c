#include "quakedef.h"
#ifndef SERVERONLY
#include "shader.h"
#include "gl_draw.h"

texid_t missing_texture;

texid_t translate_texture;
shader_t *translate_shader;

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

#pragma message("Fixme: move conwidth handling into here")

	missing_texture = R_LoadTexture8("no_texture", 16, 16, (unsigned char*)r_notexture_mip + r_notexture_mip->offsets[0], IF_NOALPHA|IF_NOGAMMA, 0);
	translate_texture = r_nulltex;

	draw_backtile = R_RegisterShader("gfx/backtile.lmp",
		"{\n"
#ifdef USE_EGL
			"program default2d\n"
#endif
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
			"{\n"
				"map $whiteimage\n"
				"rgbgen vertex\n"
			"}\n"
		"}\n");
	shader_draw_fill_trans = R_RegisterShader("fill_trans",
		"{\n"
			"{\n"
				"map $whiteimage\n"
				"rgbgen vertex\n"
				"alphagen vertex\n"
				"blendfunc blend\n"
			"}\n"
		"}\n");
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
	shader_menutint = R_RegisterShader("menutint_glsl",
		"{\n"
			"if $glsl && gl_menutint_shader != 0\n"
			"[\n"
				"glslprogram\n"
				"{\n"
			"#ifdef VERTEX_SHADER\n"
			"\
					uniform mat4 m_view;\
					uniform mat4 m_projection;\
					attribute vec3 v_position;\
					attribute vec2 v_texcoord;\
					varying vec2 texcoord;\
					uniform vec3 rendertexturescale;\
					void main(void)\
					{\
						texcoord.x = v_texcoord.x*rendertexturescale.x;\
						texcoord.y = (1.0-v_texcoord.y)*rendertexturescale.y;\
						gl_Position = m_projection * m_view * vec4(v_position, 1.0);\
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

	Cvar_Hook(&gl_font, R2D_Font_Callback);
	Cvar_Hook(&vid_conautoscale, R2D_Conautoscale_Callback);
	Cvar_Hook(&gl_screenangle, R2D_ScreenAngle_Callback);
	Cvar_Hook(&vid_conheight, R2D_Conheight_Callback);
	Cvar_Hook(&vid_conwidth, R2D_Conwidth_Callback);

	Cvar_ForceCallback(&gl_conback);
	Cvar_ForceCallback(&vid_conautoscale);
	Cvar_ForceCallback(&gl_font);
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

	BE_DrawMesh_Single(pic, &draw_mesh, NULL, &pic->defaulttextures);
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
		BE_DrawMesh_Single(shader_draw_fill_trans, &draw_mesh, NULL, &shader_draw_fill_trans->defaulttextures);
	else
		BE_DrawMesh_Single(shader_draw_fill, &draw_mesh, NULL, &shader_draw_fill->defaulttextures);
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
#ifdef USE_EGL
			"program default2d\n"
#endif
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

	BE_DrawMesh_Single(draw_backtile, &draw_mesh, NULL, &draw_backtile->defaulttextures);
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
	if (!conback || !conback->width)
	{
		conback = R_RegisterCustom("console", NULL, NULL);
		if (!conback)
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

#endif
