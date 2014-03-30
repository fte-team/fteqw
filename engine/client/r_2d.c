#include "quakedef.h"
#ifndef SERVERONLY
#include "shader.h"
#include "gl_draw.h"

qboolean r2d_canhwgamma;	//says the video code has successfully activated hardware gamma
texid_t missing_texture;
texid_t missing_texture_gloss;
texid_t missing_texture_normal;

texid_t translate_texture;
shader_t *translate_shader;

texid_t ch_int_texture;
vec3_t ch_color;
shader_t *shader_crosshair;

static mpic_t *conback;
static mpic_t *draw_backtile;
static shader_t *shader_draw_fill, *shader_draw_fill_trans;
mpic_t		*draw_disc;

shader_t *shader_contrastup;
shader_t *shader_contrastdown;
shader_t *shader_brightness;
shader_t *shader_gammacb;
shader_t *shader_polyblend;
shader_t *shader_menutint;

static mesh_t	draw_mesh;
static vecV_t	draw_mesh_xyz[4];
vec2_t	draw_mesh_st[4];
static avec4_t	draw_mesh_colors[4];
index_t r_quad_indexes[6] = {0, 1, 2, 2, 3, 0};
unsigned int r2d_be_flags;

static struct
{
	texid_t id;
	int width;
	int height;
	uploadfmt_t fmt;
} *rendertargets;
static unsigned int numrendertargets;

extern cvar_t scr_conalpha;
extern cvar_t gl_conback;
extern cvar_t gl_font;
extern cvar_t gl_screenangle;
extern cvar_t vid_conautoscale;
extern cvar_t vid_conheight;
extern cvar_t vid_conwidth;
extern cvar_t con_textsize;
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

void R2D_Shutdown(void)
{
	Cvar_Unhook(&gl_font);
	Cvar_Unhook(&vid_conautoscale);
	Cvar_Unhook(&gl_screenangle);
	Cvar_Unhook(&vid_conheight);
	Cvar_Unhook(&vid_conwidth);

	Cvar_Unhook(&crosshair);
	Cvar_Unhook(&crosshairimage);
	Cvar_Unhook(&crosshaircolor);

	BZ_Free(cl_stris);
	cl_stris = NULL;
	BZ_Free(cl_strisvertv);
	cl_strisvertv = NULL;
	BZ_Free(cl_strisvertc);
	cl_strisvertc = NULL;
	BZ_Free(cl_strisvertt);
	cl_strisvertt = NULL;
	BZ_Free(cl_strisidx);
	cl_strisidx = NULL;
	cl_numstrisidx = 0;
	cl_maxstrisidx = 0;
	cl_numstrisvert = 0;
	cl_maxstrisvert = 0;
	cl_numstris = 0;
	cl_maxstris = 0;

	while (numrendertargets>0)
		R_DestroyTexture(rendertargets[--numrendertargets].id);
	free(rendertargets);
	rendertargets = NULL;

	if (font_console == font_default)
		font_console = NULL;

	if (font_console)
		Font_Free(font_console);
	font_console = NULL; 
	if (font_default)
		Font_Free(font_default);
	font_default = NULL; 
	if (font_tiny)
		Font_Free(font_tiny);
	font_tiny = NULL; 
}

/*
Iniitalise the 2d rendering functions (including font).
Image loading code must be ready for use at this point.
*/
void R2D_Init(void)
{
	unsigned int nonorm[4*4];
	unsigned int nogloss[4*4];
	int i;
	unsigned int glossval;
	unsigned int normval;
	extern cvar_t gl_specular_fallback;
	conback = NULL;

	Shader_Init();

	BE_Init();
	draw_mesh.istrifan = true;
	draw_mesh.numvertexes = 4;
	draw_mesh.numindexes = 6;
	draw_mesh.xyz_array = draw_mesh_xyz;
	draw_mesh.st_array = draw_mesh_st;
	draw_mesh.colors4f_array[0] = draw_mesh_colors;
	draw_mesh.indexes = r_quad_indexes;


	Font_Init();

#ifdef warningmsg
#pragma warningmsg("Fixme: move conwidth handling into here")
#endif

	glossval = min(gl_specular_fallback.value*255, 255);
	glossval *= 0x10101;
	glossval |= 0xff000000;
	glossval = LittleLong(glossval);
	normval = 0xffff8080;
	normval = LittleLong(normval);
	for (i = 0; i < 4*4; i++)
	{
		nogloss[i] = glossval;
		nonorm[i] = normval;
	}
	missing_texture = R_LoadHiResTexture("no_texture", NULL, IF_NEAREST);
	if (!TEXVALID(missing_texture))
		missing_texture = R_LoadTexture8("no_texture", 16, 16, (unsigned char*)r_notexture_mip + r_notexture_mip->offsets[0], IF_NOALPHA|IF_NOGAMMA, 0);
	missing_texture_gloss = R_LoadTexture("no_texture_gloss", 4, 4, TF_RGBA32, (unsigned char*)nogloss, IF_NOGAMMA);
	missing_texture_normal = R_LoadTexture("no_texture_normal", 4, 4, TF_RGBA32, (unsigned char*)nonorm, IF_NOGAMMA);
	translate_texture = r_nulltex;
	ch_int_texture = r_nulltex;

	draw_backtile = R_RegisterShader("gfx/backtile.lmp", SUF_NONE,
		"{\n"
			"if $nofixed\n"
				"program default2d\n"
			"endif\n"
			"affine\n"
			"nomipmaps\n"
			"{\n"
				"map $diffuse\n"
			"}\n"
		"}\n");
	if (!TEXVALID(draw_backtile->defaulttextures.base))
		draw_backtile->defaulttextures.base = R_LoadHiResTexture("gfx/backtile", NULL, IF_UIPIC|IF_NOPICMIP|IF_NOMIPMAP);
	if (!TEXVALID(draw_backtile->defaulttextures.base))
		draw_backtile->defaulttextures.base = R_LoadHiResTexture("gfx/menu/backtile", NULL, IF_UIPIC|IF_NOPICMIP|IF_NOMIPMAP);

	shader_draw_fill = R_RegisterShader("fill_opaque", SUF_NONE,
		"{\n"
			"program defaultfill\n"
			"{\n"
				"map $whiteimage\n"
				"rgbgen exactvertex\n"
				"alphagen vertex\n"
			"}\n"
		"}\n");
	shader_draw_fill_trans = R_RegisterShader("fill_trans", SUF_NONE,
		"{\n"
			"program defaultfill\n"
			"{\n"
				"map $whiteimage\n"
				"rgbgen vertex\n"
				"alphagen vertex\n"
				"blendfunc blend\n"
			"}\n"
		"}\n");
	shader_contrastup = R_RegisterShader("contrastupshader", SUF_NONE,
		"{\n"
			"program defaultfill\n"
			"{\n"
				"nodepthtest\n"
				"map $whiteimage\n"
				"blendfunc gl_dst_color gl_one\n"
				"rgbgen vertex\n"
				"alphagen vertex\n"
			"}\n"
		"}\n"
	);
	shader_contrastdown = R_RegisterShader("contrastdownshader", SUF_NONE,
		"{\n"
			"program defaultfill\n"
			"{\n"
				"nodepthtest\n"
				"map $whiteimage\n"
				"blendfunc gl_dst_color gl_zero\n"
				"rgbgen vertex\n"
				"alphagen vertex\n"
			"}\n"
		"}\n"
	);
	shader_brightness = R_RegisterShader("brightnessshader", SUF_NONE,
		"{\n"
			"program defaultfill\n"
			"{\n"
				"nodepthtest\n"
				"map $whiteimage\n"
				"blendfunc gl_one gl_one\n"
				"rgbgen vertex\n"
				"alphagen vertex\n"
			"}\n"
		"}\n"
	);
	shader_gammacb = R_RegisterShader("gammacbshader", SUF_NONE,
		"{\n"
			"program defaultgammacb\n"
			"affine\n"
			"cull back\n"
			"{\n"
				"map $currentrender\n"
				"nodepthtest\n"
			"}\n"
		"}\n"
	);
	shader_polyblend = R_RegisterShader("polyblendshader", SUF_NONE,
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
	shader_menutint = R_RegisterShader("menutint", SUF_NONE,
		"{\n"
			"affine\n"
			"if $glsl && gl_menutint_shader != 0\n"
				"program menutint\n"
				"{\n"
					"map $currentrender\n"
				"}\n"
			"else\n"
				"{\n"
					"map $whiteimage\n"
					"blendfunc gl_dst_color gl_zero\n"
					"rgbgen const $r_menutint\n"
				"}\n"
			"endif\n"
		"}\n"
	);
	shader_crosshair = R_RegisterShader("crosshairshader", SUF_NONE,
		"{\n"
			"if $nofixed\n"
				"program default2d\n"
			"endif\n"
			"affine\n"
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

	R2D_Font_Changed();
}

mpic_t	*R2D_SafeCachePic (const char *path)
{
	shader_t *s;
	if (!qrenderer)
		return NULL;
	s = R_RegisterPic(path);
	if (s->flags & SHADER_NOIMAGE)
		return NULL;
	return s;
}


mpic_t *R2D_SafePicFromWad (const char *name)
{
	char newnamewad[32];
	char newnamegfx[32];
	shader_t *s;
	
	snprintf(newnamewad, sizeof(newnamewad), "wad/%s", name);
	snprintf(newnamegfx, sizeof(newnamegfx), "gfx/%s", name);

	s = R_RegisterPic(newnamewad);
	if (!(s->flags & SHADER_NOIMAGE))
		return s;
	
	s = R_RegisterPic(newnamegfx);
	if (!(s->flags & SHADER_NOIMAGE))
		return s;

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
void R2D_FillBlock(float x, float y, float w, float h)
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

void R2D_ScalePic (float x, float y, float width, float height, mpic_t *pic)
{
	R2D_Image(x, y, width, height, 0, 0, 1, 1, pic);
}

void R2D_SubPic(float x, float y, float width, float height, mpic_t *pic, float srcx, float srcy, float srcwidth, float srcheight)
{
	float newsl, newtl, newsh, newth;

	newsl = (srcx)/(float)srcwidth;
	newsh = newsl + (width)/(float)srcwidth;

	newtl = (srcy)/(float)srcheight;
	newth = newtl + (height)/(float)srcheight;

	R2D_Image(x, y, width, height, newsl, newtl, newsh, newth, pic);
}

/* this is an ugly special case drawing func that's only used for the player color selection menu */
void R2D_TransPicTranslate (float x, float y, int width, int height, qbyte *pic, unsigned int *palette)
{
	int				v, u;
	unsigned		trans[64*64], *dest;
	qbyte			*src;

	dest = trans;
	for (v=0 ; v<64 ; v++, dest += 64)
	{
		src = &pic[ ((v*height)>>6) *width];
		for (u=0 ; u<64 ; u++)
			dest[u] = palette[src[(u*width)>>6]];
	}

	if (!TEXVALID(translate_texture))
	{
		translate_texture = R_AllocNewTexture("***translatedpic***", 64, 64, 0);
		translate_shader = R_RegisterShader("translatedpic", SUF_2D,
			"{\n"
				"if $nofixed\n"
					"program default2d\n"
				"endif\n"
				"nomipmaps\n"
				"{\n"
					"map $diffuse\n"
					"blendfunc blend\n"
					"rgbgen vertex\n"
					"alphagen vertex\n"
				"}\n"
			"}\n");
		translate_shader->defaulttextures.base = translate_texture;
	}
	/* could avoid reuploading already translated textures but this func really isn't used enough anyway */
	R_Upload(translate_texture, NULL, TF_RGBA32, trans, NULL, 64, 64, IF_UIPIC|IF_NOMIPMAP|IF_NOGAMMA);
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
	if (!conback)
	{
		R2D_ImageColours(0, 0, 0, a);
		R2D_FillBlock(0, lastline-(int)vid.height, w, h);
		R2D_ImageColours(1, 1, 1, 1);
	}
	else if (a >= 1)
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
	R2D_ImageColours(0, 0, 0, 1);
	R2D_FillBlock(0, 0, vid.width, vid.height);
//	R2D_ScalePic(0, 0, vid.width, vid.height, conback);
}

/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void R2D_TileClear (float x, float y, float w, float h)
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
	if (qrenderer == QR_NONE || !strcmp(var->string, "none"))
	{
		conback = NULL;
		return;
	}

	if (*var->string)
		conback = R_RegisterPic(var->string);
	if (!conback || conback->flags & SHADER_NOIMAGE)
	{
		conback = R_RegisterCustom("console", SUF_2D, NULL, NULL);
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

#if defined(_WIN32) && !defined(FTE_SDL) && !defined(WINRT)
#include <windows.h>
qboolean R2D_Font_WasAdded(char *buffer, char *fontfilename)
{
	char *match;
	if (!fontfilename)
		return true;
	match = strstr(buffer, fontfilename);
	if (!match)
		return false;
	if (!(match == buffer || match[-1] == ','))
		return false;
	match += strlen(fontfilename);
	if (*match && *match != ',')
		return false;
	return true;
}
extern qboolean	WinNT;
qboolean MyRegGetStringValue(HKEY base, char *keyname, char *valuename, void *data, int datalen);
qboolean MyRegGetStringValueMultiSz(HKEY base, char *keyname, char *valuename, void *data, int datalen);
void R2D_Font_AddFontLink(char *buffer, int buffersize, char *fontname)
{
	char link[1024];
	char *res, *comma, *othercomma, *nl;
	if (fontname)
	if (MyRegGetStringValueMultiSz(HKEY_LOCAL_MACHINE, WinNT?"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\FontLink\\SystemLink":"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\FontLink\\SystemLink", fontname, link, sizeof(link)))
	{
		res = nl = link;
		while (*nl)
		{
			nl += strlen(nl);
			nl++;
			comma = strchr(res, ',');
			if (comma)
			{
				*comma++ = 0;
				othercomma = strchr(comma, ',');
				if (othercomma)
					*othercomma = 0;
			}
			else
				comma = "";
			if (!R2D_Font_WasAdded(buffer, res))
			{
				Q_strncatz(buffer, ",", buffersize);
				Q_strncatz(buffer, res, buffersize);
				R2D_Font_AddFontLink(buffer, buffersize, comma);
			}
			res = nl;
		}
	}
}
#endif
void R2D_Font_Changed(void)
{
	if (!con_textsize.modified)
		return;
	con_textsize.modified = false;

	if (font_console == font_default)
		font_console = NULL;
	if (font_console)
		Font_Free(font_console);
	font_console = NULL;
	if (font_default)
		Font_Free(font_default);
	font_default = NULL;

	if (font_tiny)
		Font_Free(font_tiny);
	font_tiny = NULL; 

#if defined(MENU_DAT) || defined(CSQC_DAT)
	PR_ResetFonts(0);
#endif

	if (qrenderer == QR_NONE)
		return;

#if defined(_WIN32) && !defined(FTE_SDL) && !defined(WINRT)
	if (!strcmp(gl_font.string, "?"))
	{
		BOOL (APIENTRY *pChooseFontA)(LPCHOOSEFONTA) = NULL;
		dllfunction_t funcs[] =
		{
			{(void*)&pChooseFontA, "ChooseFontA"},
			{NULL}
		};
		qboolean MyRegGetStringValue(HKEY base, char *keyname, char *valuename, void *data, int datalen);
		LOGFONT lf = {0};
		CHOOSEFONTA cf = {sizeof(cf)};
		extern HWND	mainwindow;
		font_default = Font_LoadFont(8, "");
		if (con_textsize.ival != 8 && con_textsize.ival >= 1)
			font_console = Font_LoadFont(con_textsize.ival, "");
		if (!font_console)
			font_console = font_default;

		cf.hwndOwner = mainwindow;
		cf.iPointSize = (8 * vid.rotpixelheight)/vid.height;
		cf.Flags = CF_FORCEFONTEXIST | CF_TTONLY;
		cf.lpLogFont = &lf;

		Sys_LoadLibrary("Comdlg32.dll", funcs);
					
		if (pChooseFontA && pChooseFontA(&cf))
		{
			char fname[MAX_OSPATH*8];
			char *keyname;
			*fname = 0;
			//FIXME: should enumerate and split & and ignore sizes and () crap.
			if (!*fname)
			{
				keyname = va("%s%s%s (TrueType)", lf.lfFaceName, lf.lfWeight>=FW_BOLD?" Bold":"", lf.lfItalic?" Italic":"");
				if (!MyRegGetStringValue(HKEY_LOCAL_MACHINE, WinNT?"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts":"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Fonts", keyname, fname, sizeof(fname)))
					*fname = 0;
			}
			if (!*fname)
			{
				keyname = va("%s (OpenType)", lf.lfFaceName);
				if (!MyRegGetStringValue(HKEY_LOCAL_MACHINE, WinNT?"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts":"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Fonts", keyname, fname, sizeof(fname)))
					*fname = 0;
			}

			R2D_Font_AddFontLink(fname, sizeof(fname), lf.lfFaceName);
			Cvar_Set(&gl_font, fname);
		}
		return;
	}
#endif

	font_default = Font_LoadFont(8, gl_font.string);
	if (!font_default && *gl_font.string)
		font_default = Font_LoadFont(8, "");

	if (con_textsize.ival != 8 && con_textsize.ival >= 1)
	{
		font_console = Font_LoadFont(con_textsize.ival, gl_font.string);
		if (!font_console)
			font_console = Font_LoadFont(con_textsize.ival, "");
	}
	if (!font_console)
		font_console = font_default;
}

void R2D_Font_Callback(struct cvar_s *var, char *oldvalue)
{
	con_textsize.modified = true;
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

	if (!cwidth && !cheight)
		cheight = 480;
	if (cheight && !cwidth)
		cwidth = (cheight*vid.rotpixelwidth)/vid.rotpixelheight;
	if (cwidth && !cheight)
		cheight = (cwidth*vid.rotpixelheight)/vid.rotpixelwidth;

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
//bright flashes and stuff, game only, doesn't touch sbar
void R2D_PolyBlend (void)
{
	if (!sw_blend[3])
		return;

	if (r_refdef.flags & Q2RDF_NOWORLDMODEL)
		return;

	R2D_ImageColours (sw_blend[0], sw_blend[1], sw_blend[2], sw_blend[3]);
	R2D_ScalePic(r_refdef.vrect.x, r_refdef.vrect.y, r_refdef.vrect.width, r_refdef.vrect.height, shader_polyblend);
	R2D_ImageColours (1, 1, 1, 1);
}

//for lack of hardware gamma
void R2D_BrightenScreen (void)
{
	float f;

	RSpeedMark();

	if (fabs(v_contrast.value - 1.0) < 0.05 && fabs(v_brightness.value - 0) < 0.05 && fabs(v_gamma.value - 1) < 0.05)
		return;

	//don't go crazy with brightness. that makes it unusable and is thus unsafe - and worse, lots of people assume its based around 1 (like gamma and contrast are). cap to 0.5
	if (v_brightness.value > 0.5)
		v_brightness.value = 0.5;

	if (r2d_canhwgamma)
		return;

	TRACE(("R2D_BrightenScreen: brightening\n"));
	if (v_gamma.value != 1 && shader_gammacb->prog)
	{
		//this should really be done properly, with render-to-texture
		R2D_ImageColours (v_gamma.value, v_contrast.value, v_brightness.value, 1);
		R2D_ScalePic(0, vid.height, vid.width, -(int)vid.height, shader_gammacb);
	}
	else
	{
		f = v_contrast.value;
		f = min (f, 3);

		while (f > 1)
		{
			if (f >= 2)
			{
				R2D_ImageColours (1, 1, 1, 1);
				f *= 0.5;
			}
			else
			{
				R2D_ImageColours (f - 1, f - 1, f - 1, 1);
				f = 1;
			}
			R2D_ScalePic(0, 0, vid.width, vid.height, shader_contrastup);
		}
		if (f < 1)
		{
			R2D_ImageColours (f, f, f, 1);
			R2D_ScalePic(0, 0, vid.width, vid.height, shader_contrastdown);
		}

		if (v_brightness.value)
		{
			R2D_ImageColours (v_brightness.value, v_brightness.value, v_brightness.value, 1);
			R2D_ScalePic(0, 0, vid.width, vid.height, shader_brightness);
		}
	}
	R2D_ImageColours (1, 1, 1, 1);

	/*make sure the hud is redrawn after if needed*/
	Sbar_Changed();

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
		shader_crosshair->defaulttextures.base = R_LoadHiResTexture (crosshairimage.string, "crosshairs", IF_UIPIC|IF_NOMIPMAP|IF_NOGAMMA);
		if (TEXVALID(shader_crosshair->defaulttextures.base))
			return;
	}
	else if (c <= 1)
		return;

	c -= 2;
	c = c % (sizeof(crosshair_pixels) / (CS_HEIGHT*sizeof(*crosshair_pixels)));

	if (!TEXVALID(ch_int_texture))
		ch_int_texture = R_AllocNewTexture("***crosshair***", CS_WIDTH, CS_HEIGHT, IF_UIPIC|IF_NOMIPMAP);
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

	R_Upload(ch_int_texture, NULL, TF_RGBA32, crossdata, NULL, CS_WIDTH, CS_HEIGHT, IF_UIPIC|IF_NOMIPMAP|IF_NOGAMMA);

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

	float size;

	if (crosshair.ival < 1)
		return;

	// old style
	if (crosshair.ival == 1 && !crosshairimage.string[0])
	{
		// adjust console crosshair scale to match default
		size = crosshairsize.value;
		if (size == 0)
			size = 8;
		else if (size < 0)
			size = -size;
		for (sc = 0; sc < cl.splitclients; sc++)
		{
			SCR_CrosshairPosition(&cl.playerview[sc], &sx, &sy);
			Font_BeginScaledString(font_default, sx, sy, size, size, &sx, &sy);
			sx -= Font_CharScaleWidth('+' | 0xe000 | CON_WHITEMASK)/2;
			sy -= Font_CharScaleHeight()/2;
			Font_ForceColour(ch_color[0], ch_color[1], ch_color[2], crosshairalpha.value);
			Font_DrawScaleChar(sx, sy, '+' | 0xe000 | CON_WHITEMASK);
			Font_InvalidateColour();
			Font_EndString(font_default);
		}
		return;
	}

	size = crosshairsize.value;

	if (size < 0)
	{
		size = -size;
		sizex = size;
		sizey = size;
	}
	else
	{
		sizex = (size*vid.rotpixelwidth) / (float)vid.width;
		sizey = (size*vid.rotpixelheight) / (float)vid.height;
	}

	sizex = (int)sizex;
	sizex = ((sizex)*(int)vid.width) / (float)vid.rotpixelwidth;

	sizey = (int)sizey;
	sizey = ((sizey)*(int)vid.height) / (float)vid.rotpixelheight;

	R2D_ImageColours(ch_color[0], ch_color[1], ch_color[2], crosshairalpha.value);
	for (sc = 0; sc < cl.splitclients; sc++)
	{
		SCR_CrosshairPosition(&cl.playerview[sc], &sx, &sy);

		//translate to pixel coord, for rounding
		x = ((sx-sizex+(sizex/CS_WIDTH))*vid.rotpixelwidth) / (float)vid.width;
		y = ((sy-sizey+(sizey/CS_HEIGHT))*vid.rotpixelheight) / (float)vid.height;

		//translate to screen coords
		sx = ((x)*(int)vid.width) / (float)vid.rotpixelwidth;
		sy = ((y)*(int)vid.height) / (float)vid.rotpixelheight;

		R2D_Image(sx, sy, sizex*2, sizey*2, 0, 0, 1, 1, shader_crosshair);
	}
	R2D_ImageColours(1, 1, 1, 1);
}

//resize a texture for a render target and specify the format of it.
//pass TF_INVALID and sizes=0 to get without configuring (shaders that hardcode an $rt1 etc).
texid_t R2D_RT_Configure(unsigned int id, int width, int height, uploadfmt_t rtfmt)
{
	id--;	//0 is invalid.
	if (id < 0 || id > 255)	//sanity limit
		return r_nulltex;

	//extend the array if needed. these should be fairly light.
	if (id >= numrendertargets)
	{
		rendertargets = realloc(rendertargets, (id+1) * sizeof(*rendertargets));
		while(numrendertargets <= id)
		{
			rendertargets[numrendertargets].id = r_nulltex;
			rendertargets[numrendertargets].width = 0;
			rendertargets[numrendertargets].height = 0;
			rendertargets[numrendertargets].fmt = TF_INVALID;
			numrendertargets++;
		}
	}

	if (!TEXVALID(rendertargets[id].id))
		rendertargets[id].id = R_AllocNewTexture(va("", id+1), 0, 0, IF_NOMIPMAP);
	if (rtfmt)
	{
		switch(rtfmt)
		{
		case 1: rtfmt = TF_RGBA32;	break;
		case 2: rtfmt = TF_RGBA16F;	break;
		case 3: rtfmt = TF_RGBA32F;	break;
		case 4: rtfmt = TF_DEPTH16;	break;
		case 5: rtfmt = TF_DEPTH24;	break;
		case 6: rtfmt = TF_DEPTH32;	break;
		default:rtfmt = TF_INVALID;	break;
		}

//		if (rendertargets[id].fmt != rtfmt || rendertargets[id].width != width || rendertargets[id].height != height)
		{
			rendertargets[id].fmt = rtfmt;
			rendertargets[id].width = width;
			rendertargets[id].height = height;
			R_Upload(rendertargets[id].id, "", rtfmt, NULL, NULL, width, height, IF_NOMIPMAP);
		}
	}
	return rendertargets[id].id;
}
texid_t R2D_RT_GetTexture(unsigned int id, unsigned int *width, unsigned int *height)
{
	if (!id)
	{
		*width = 0;
		*height = 0;
		return r_nulltex;
	}
	id--;
	if (id >= numrendertargets)
	{
		Con_Printf("Render target %u is not configured\n", id);
		R2D_RT_Configure(id, 0, 0, TF_INVALID);
		if (id >= numrendertargets)
		{
			*width = 0;
			*height = 0;
			return r_nulltex;
		}
	}
	*width = rendertargets[id].width;
	*height = rendertargets[id].height;
	return rendertargets[id].id;
}

texid_t R2D_RT_DetachTexture(unsigned int id)
{
	texid_t r;
	id--;
	if (id >= numrendertargets)
		return r_nulltex;

	r = rendertargets[id].id;
	rendertargets[id].id = r_nulltex;
	rendertargets[id].fmt = TF_INVALID;
	rendertargets[id].width = 0;
	rendertargets[id].height = 0;
	return r;
}

#endif

