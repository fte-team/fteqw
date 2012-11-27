#include "quakedef.h"

#ifndef SERVERONLY
#include "shader.h"

#ifdef _WIN32
#include <windows.h>
#endif

void Font_Init(void);
void Font_Shutdown(void);
struct font_s *Font_LoadFont(int height, char *fontfilename);
void Font_Free(struct font_s *f);
void Font_BeginString(struct font_s *font, int vx, int vy, int *px, int *py);
void Font_BeginScaledString(struct font_s *font, float vx, float vy, float *px, float *py); /*avoid using*/
void Font_Transform(int vx, int vy, int *px, int *py);
int Font_CharHeight(void);
int Font_CharWidth(unsigned int charcode);
int Font_CharEndCoord(struct font_s *font, int x, unsigned int charcode);
int Font_DrawChar(int px, int py, unsigned int charcode);
float Font_DrawScaleChar(float px, float py, float cw, float ch, unsigned int charcode); /*avoid using*/
void Font_EndString(struct font_s *font);
int Font_LineBreaks(conchar_t *start, conchar_t *end, int maxpixelwidth, int maxlines, conchar_t **starts, conchar_t **ends);
struct font_s *font_conchar;
struct font_s *font_tiny;

#ifdef AVAIL_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H 

static FT_Library fontlib;


qboolean triedtoloadfreetype;
dllhandle_t *fontmodule;
FT_Error (VARGS *pFT_Init_FreeType)		(FT_Library  *alibrary);
FT_Error (VARGS *pFT_Load_Char)			(FT_Face face, FT_ULong char_code, FT_Int32 load_flags);
FT_Error (VARGS *pFT_Set_Pixel_Sizes)	(FT_Face face, FT_UInt pixel_width, FT_UInt pixel_height);
FT_Error (VARGS *pFT_New_Face)			(FT_Library library, const char *pathname, FT_Long face_index, FT_Face *aface);
FT_Error (VARGS *pFT_New_Memory_Face)	(FT_Library library, const FT_Byte* file_base, FT_Long file_size, FT_Long face_index, FT_Face *aface);
FT_Error (VARGS *pFT_Done_Face)			(FT_Face face);
#endif

static const char *imgs[] =
{
	//0xe10X
	"e100","e101",
	"inv_shotgun",
	"inv_sshotgun",
	"inv_nailgun",
	"inv_snailgun",
	"inv_rlaunch",
	"inv_srlaunch",
	"inv_lightng",	//8
	"e109","e10a","e10b","e10c","e10d","e10e","e10f",

	//0xe11X
	"e110","e111",
	"inv2_shotgun",
	"inv2_sshotgun",
	"inv2_nailgun",
	"inv2_snailgun",
	"inv2_rlaunch",
	"inv2_srlaunch",
	"inv2_lightng",
	"e119","e11a","e11b","e11c","e11d","e11e","e11f",

	//0xe12X
	"sb_shells",
	"sb_nails",
	"sb_rocket",
	"sb_cells",

	"sb_armor1",
	"sb_armor2",
	"sb_armor3",
	"e127","e128","e129","e12a","e12b","e12c","e12d","e12e","e12f",

	//0xe13X
	"sb_key1",
	"sb_key2",
	"sb_invis",
	"sb_invuln",
	"sb_suit",
	"sb_quad",

	"sb_sigil1",
	"sb_sigil2",
	"sb_sigil3",
	"sb_sigil4",
	"e13a","e13b","e13c","e13d","e13e","e13f",

	//0xe14X
	"face1",
	"face_p1",
	"face2",
	"face_p2",
	"face3",
	"face_p3",
	"face4",
	"face_p4",
	"face5",
	"face_p5",

	"face_invis",
	"face_invul2",
	"face_inv2",
	"face_quad",
	"e14e",
	"e14f",

	//0xe15X
	"e150",
	"e151",
	"e152",
	"e153",
	"e154",
	"e155",
	"e156",
	"e157",
	"e158",
	"e159",
	"e15a",
	"e15b",
	"e15c",
	"e15d",
	"e15e",
	"e15f",

	//0xe16X
	"e160",
	"e161",
	"e162",
	"e163",
	"e164",
	"e165",
	"e166",
	"e167",
	"e168",
	"e169",
	"e16a",
	"e16b",
	"e16c",
	"e16d",
	"e16e",
	"e16f"
};

#define FONTCHARS (1<<16)
#define FONTPLANES (1<<2)	//no more than 16 textures per font
#define PLANEIDXTYPE unsigned short
#define CHARIDXTYPE unsigned short

#define INVALIDPLANE ((1<<(8*sizeof(PLANEIDXTYPE)))-1)
#define BITMAPPLANE ((1<<(8*sizeof(PLANEIDXTYPE)))-2)
#define DEFAULTPLANE ((1<<(8*sizeof(PLANEIDXTYPE)))-3)
#define SINGLEPLANE ((1<<(8*sizeof(PLANEIDXTYPE)))-4)
#define PLANEWIDTH (1<<8)
#define PLANEHEIGHT PLANEWIDTH



#define GEN_CONCHAR_GLYPHS 0	//set to 0 or 1 to define whether to generate glyphs from conchars too, or if it should just draw them as glquake always used to
extern cvar_t cl_noblink;
extern cvar_t con_ocranaleds;

typedef struct font_s
{
	struct charcache_s
	{
		struct charcache_s *nextchar;

		PLANEIDXTYPE texplane;
		unsigned char advance;	//how wide this char is, when drawn
		char pad;

		unsigned char bmx;
		unsigned char bmy;
		unsigned char bmw;
		unsigned char bmh;

		short top;
		short left;
	} chars[FONTCHARS];

	short charheight;
	texid_t singletexture;
#ifdef AVAIL_FREETYPE
	FT_Face face;
	void *membuf;
#endif
	struct font_s *alt;
} font_t;

typedef struct {
	texid_t texnum[FONTPLANES];
	texid_t defaultfont;

	unsigned char plane[PLANEWIDTH*PLANEHEIGHT][4];	//tracks the current plane
	PLANEIDXTYPE activeplane;
	unsigned char planerowx;
	unsigned char planerowy;
	unsigned char planerowh;
	qboolean planechanged;

	struct charcache_s *oldestchar;
	struct charcache_s *newestchar;
	shader_t *shader;
	shader_t *backshader;
} fontplanes_t;

static fontplanes_t fontplanes;
	
#define FONT_CHAR_BUFFER 512
static index_t font_indicies[FONT_CHAR_BUFFER*6];
static vecV_t font_coord[FONT_CHAR_BUFFER*4];
static vec2_t font_texcoord[FONT_CHAR_BUFFER*4];
static byte_vec4_t font_forecoloura[FONT_CHAR_BUFFER*4];
static byte_vec4_t font_backcoloura[FONT_CHAR_BUFFER*4];
static mesh_t font_foremesh;
static mesh_t font_backmesh;
static texid_t font_texture;
static int font_colourmask;
static byte_vec4_t font_forecolour;
static byte_vec4_t font_backcolour;

static struct font_s *curfont;

//called at load time - initalises font buffers
void Font_Init(void)
{
	int i;
	TEXASSIGN(fontplanes.defaultfont, r_nulltex);

	font_foremesh.indexes = font_indicies;
	font_foremesh.xyz_array = font_coord;
	font_foremesh.st_array = font_texcoord;
	font_foremesh.colors4b_array = font_forecoloura;

	font_backmesh.indexes = font_indicies;
	font_backmesh.xyz_array = font_coord;
	font_backmesh.st_array = font_texcoord;
	font_backmesh.colors4b_array = font_backcoloura;

	for (i = 0; i < FONT_CHAR_BUFFER; i++)
	{
		font_indicies[i*6+0] = i*4+0;
		font_indicies[i*6+1] = i*4+1;
		font_indicies[i*6+2] = i*4+2;
		font_indicies[i*6+3] = i*4+0;
		font_indicies[i*6+4] = i*4+2;
		font_indicies[i*6+5] = i*4+3;
	}

	for (i = 0; i < FONTPLANES; i++)
	{
		TEXASSIGN(fontplanes.texnum[i], R_AllocNewTexture("***fontplane***", PLANEWIDTH, PLANEHEIGHT, IF_NOMIPMAP));
	}

	fontplanes.shader = R_RegisterShader("ftefont",
		"{\n"
			"if $nofixed\n"
			"[\n"
				"program default2d\n"
			"]\n"
			"nomipmaps\n"
			"{\n"
				"map $nearest:$diffuse\n"
				"rgbgen vertex\n"
				"alphagen vertex\n"
				"blendfunc blend\n"
			"}\n"
		"}\n"
		);

	fontplanes.backshader = R_RegisterShader("ftefontback",
		"{\n"
			"nomipmaps\n"
			"{\n"
				"map $whiteimage\n"
				"rgbgen vertex\n"
				"alphagen vertex\n"
				"blendfunc blend\n"
			"}\n"
		"}\n"
		);

	font_colourmask = ~0;
}

//flush the font buffer, by drawing it to the screen
static void Font_Flush(void)
{
	if (!font_foremesh.numindexes)
		return;
	if (fontplanes.planechanged)
	{
		R_Upload(fontplanes.texnum[fontplanes.activeplane], NULL, TF_RGBA32, (void*)fontplanes.plane, NULL, PLANEWIDTH, PLANEHEIGHT, IF_NEAREST|IF_NOPICMIP|IF_NOMIPMAP|IF_NOGAMMA);

		fontplanes.planechanged = false;
	}
	font_foremesh.istrifan = (font_foremesh.numvertexes == 4);
	if (font_colourmask & CON_NONCLEARBG)
	{
		font_backmesh.numindexes = font_foremesh.numindexes;
		font_backmesh.numvertexes = font_foremesh.numvertexes;
		font_backmesh.istrifan = font_foremesh.istrifan;

		BE_DrawMesh_Single(fontplanes.backshader, &font_backmesh, NULL, &fontplanes.backshader->defaulttextures, 0);
	}
	TEXASSIGN(fontplanes.shader->defaulttextures.base, font_texture);
	BE_DrawMesh_Single(fontplanes.shader, &font_foremesh, NULL, &fontplanes.shader->defaulttextures, 0);
	font_foremesh.numindexes = 0;
	font_foremesh.numvertexes = 0;
}

static int Font_BeginChar(texid_t tex)
{
	int fvert;

	if (font_foremesh.numindexes == FONT_CHAR_BUFFER*6 || font_texture.ref != tex.ref)
	{
		Font_Flush();
		TEXASSIGNF(font_texture, tex);
	}

	fvert = font_foremesh.numvertexes;
	
	font_foremesh.numindexes += 6;
	font_foremesh.numvertexes += 4;

	return fvert;
}

//clear the shared planes and free memory etc
void Font_Shutdown(void)
{
	int i;

	for (i = 0; i < FONTPLANES; i++)
		TEXASSIGN(fontplanes.texnum[i], r_nulltex);
	fontplanes.activeplane = 0;
	fontplanes.oldestchar = NULL;
	fontplanes.newestchar = NULL;
	fontplanes.planechanged = 0;
	fontplanes.planerowx = 0;
	fontplanes.planerowy = 0;
	fontplanes.planerowh = 0;
}

//we got too many chars and switched to a new plane - purge the chars in that plane
void Font_FlushPlane(font_t *f)
{
	/*
	assumption:
	oldest chars must be of the oldest plane
	*/

	//we've not broken anything yet, flush while we can
	Font_Flush();

	fontplanes.activeplane++;
	fontplanes.activeplane = fontplanes.activeplane % FONTPLANES;
	fontplanes.planerowh = 0;
	fontplanes.planerowx = 0;
	fontplanes.planerowy = 0;
	for (; fontplanes.oldestchar; fontplanes.oldestchar = fontplanes.oldestchar->nextchar)
	{
		if (fontplanes.oldestchar->texplane != fontplanes.activeplane)
			break;

		//invalidate it
		fontplanes.oldestchar->texplane = INVALIDPLANE;
	}
	if (!fontplanes.oldestchar)
		fontplanes.newestchar = NULL;
}

//obtains a cached char, null if not cached
static struct charcache_s *Font_GetChar(font_t *f, CHARIDXTYPE charidx)
{
	struct charcache_s *c = &f->chars[charidx];
	if (c->texplane == INVALIDPLANE)
	{
		//not cached, can't get.
		return NULL;
	}
	return c;
}

//loads a new image into a given character slot for the given font.
//note: make sure it doesn't already exist or things will get cyclic
//alphaonly says if its a greyscale image. false means rgba.
static struct charcache_s *Font_LoadGlyphData(font_t *f, CHARIDXTYPE charidx, int alphaonly, void *data, unsigned int bmw, unsigned int bmh, unsigned int pitch)
{
	int x, y;
	unsigned char *out;
	struct charcache_s *c = &f->chars[charidx];

	if (fontplanes.planerowx + (int)bmw >= PLANEWIDTH)
	{
		fontplanes.planerowx = 0;
		fontplanes.planerowy += fontplanes.planerowh;
		fontplanes.planerowh = 0;
	}

	if (fontplanes.planerowy+(int)bmh >= PLANEHEIGHT)
		Font_FlushPlane(f);

	if (fontplanes.newestchar)
		fontplanes.newestchar->nextchar = c;
	else
		fontplanes.oldestchar = c;
	fontplanes.newestchar = c;
	c->nextchar = NULL;

	c->texplane = fontplanes.activeplane;
	c->bmx = fontplanes.planerowx;
	c->bmy = fontplanes.planerowy;
	c->bmw = bmw;
	c->bmh = bmh;

	if (fontplanes.planerowh < (int)bmh)
		fontplanes.planerowh = bmh;
	fontplanes.planerowx += bmw;

	out = (unsigned char *)&fontplanes.plane[c->bmx+(int)c->bmy*PLANEHEIGHT];
	if (alphaonly)
	{
		for (y = 0; y < bmh; y++)
		{
			for (x = 0; x < bmw; x++)
			{
				*(unsigned int *)&out[x*4] = 0xffffffff;
				out[x*4+3] = ((unsigned char*)data)[x];
			}
			data = (char*)data + pitch;
			out += PLANEWIDTH*4;
		}
	}
	else
	{
		pitch*=4;
		for (y = 0; y < bmh; y++)
		{
			for (x = 0; x < bmw; x++)
			{
				((unsigned int*)out)[x] = ((unsigned int*)data)[x];
			}
			data = (char*)data + pitch;
			out += PLANEWIDTH*4;
		}
	}
	fontplanes.planechanged = true;
	return c;
}

//loads the given charidx for the given font, importing from elsewhere if needed.
static struct charcache_s *Font_TryLoadGlyph(font_t *f, CHARIDXTYPE charidx)
{
	struct charcache_s *c;

#if GEN_CONCHAR_GLYPHS != 0
	if (charidx >= 0xe000 && charidx <= 0xe0ff)
	{
		int cpos = charidx & 0xff;
		unsigned int img[64*64], *d;
		unsigned char *s;
		int scale;
		int x,y, ys;
		qbyte *draw_chars = W_GetLumpName("conchars");
		if (draw_chars)
		{
			d = img;
			s = draw_chars + 8*(cpos&15)+128*8*(cpos/16);

			scale = f->charheight/8;
			if (scale < 1)
				scale = 1;
			if (scale > 64/8)
				scale = 64/8;
			
			for (y = 0; y < 8; y++)
			{
				for (ys = 0; ys < scale; ys++)
				{
					for (x = 0; x < 8*scale; x++)
						d[x] = d_8to24rgbtable[s[x/scale]];
					d+=8*scale;
				}
				s+=128;
			}
			c = Font_LoadGlyphData(f, charidx, false, img, 8*scale, 8*scale, 8*scale);
			if (c)
			{
				c->advance = 8*scale;
				c->left = 0;
				c->top = 7*scale;
			}
			return c;
		}
		charidx &= 0x7f;
	}
#endif

	if (charidx >= 0xe100 && charidx <= 0xe1ff)
	{
		qpic_t *wadimg;
		unsigned char *src;
		unsigned int img[64*64];
		int nw, nh;
		int x, y;
		unsigned int stepx, stepy;
		unsigned int srcx, srcy;

		if (charidx-0xe100 >= sizeof(imgs)/sizeof(imgs[0]))
			wadimg = NULL;
		else
			wadimg = W_SafeGetLumpName(imgs[charidx-0xe100]);
		if (wadimg)
		{
			nh = wadimg->height;
			nw = wadimg->width;
			while (nh < f->charheight)
			{
				nh *= 2;
				nw *= 2;
			}
			if (nh > f->charheight)
			{
				nw = (nw * f->charheight)/nh;
				nh = f->charheight;
			}
			stepy = 0x10000*((float)wadimg->height/nh);
			stepx = 0x10000*((float)wadimg->width/nw);
			if (nh > 64)
				nh = 64;
			if (nw > 64)
				nw = 64;
			srcy = 0;
			for (y = 0; y < nh; y++)
			{
				src = (unsigned char *)(wadimg->data);
				src += wadimg->width * (srcy>>16);
				srcy += stepy;
				srcx = 0;
				for (x = 0; x < nw; x++)
				{
					img[x+y*64] = d_8to24rgbtable[src[srcx>>16]];
					srcx += stepx;
				}
			}

			c = Font_LoadGlyphData(f, charidx, false, img, nw, nh, 64);
			if (c)
			{
				c->left = 0;
				c->top = f->charheight - nh;
				c->advance = nw;
				return c;
			}
		}
	}
	/*make tab invisible*/
	if (charidx == '\t' || charidx == '\n')
	{
		c = &f->chars[charidx];
		c->left = 0;
		c->advance = f->charheight;
		c->top = 0;

		c->texplane = 0;
		c->bmx = 0;
		c->bmy = 0;
		c->bmw = 0;
		c->bmh = 0;
		return c;
	}

#ifdef AVAIL_FREETYPE
	if (f->face)
	{
		if (pFT_Load_Char(f->face, charidx, FT_LOAD_RENDER) == 0)
		{
			FT_GlyphSlot slot;
			FT_Bitmap *bm;

			slot = f->face->glyph;
			bm = &slot->bitmap;
			c = Font_LoadGlyphData(f, charidx, true, bm->buffer, bm->width, bm->rows, bm->pitch); 

			if (c)
			{
				c->advance = slot->advance.x >> 6;
				c->left = slot->bitmap_left;
				c->top = f->charheight*3/4 - slot->bitmap_top;
				return c;
			}
		}
	}
#endif
	return NULL;
}

qboolean Font_LoadFreeTypeFont(struct font_s *f, int height, char *fontfilename)
{
#ifdef AVAIL_FREETYPE
	FT_Face face = NULL;
	FT_Error error;
	flocation_t loc;
	void *fbase = NULL;
	if (!*fontfilename)
		return false;
	if (!fontlib)
	{
		dllfunction_t ft2funcs[] =
		{
			{(void**)&pFT_Init_FreeType, "FT_Init_FreeType"},
			{(void**)&pFT_Load_Char, "FT_Load_Char"},
			{(void**)&pFT_Set_Pixel_Sizes, "FT_Set_Pixel_Sizes"},
			{(void**)&pFT_New_Face, "FT_New_Face"},
			{(void**)&pFT_New_Memory_Face, "FT_New_Memory_Face"},
			{(void**)&pFT_Init_FreeType, "FT_Init_FreeType"},
			{(void**)&pFT_Done_Face, "FT_Done_Face"},
			{NULL, NULL}
		};
		if (triedtoloadfreetype)
			return false;
		triedtoloadfreetype = true;

		fontmodule = Sys_LoadLibrary("freetype6", ft2funcs);
		if (!fontmodule)
			return false;
		error = pFT_Init_FreeType(&fontlib);
		if (error)
		{
			Sys_CloseLibrary(fontmodule);
			return false;
		}
		/*any other errors leave freetype open*/
	}

	error = FT_Err_Cannot_Open_Resource;
	if (FS_FLocateFile(fontfilename, FSLFRT_LENGTH, &loc)>0)
	{
		if (*loc.rawname && !loc.offset)
		{
			fbase = NULL;
			/*File is directly fopenable with no bias (not in a pk3/pak). Use the system-path form, so we don't have to eat the memory cost*/
			error = pFT_New_Face(fontlib, loc.rawname, 0, &face);
		}
		else
		{
			/*File is inside an archive, we need to read it and pass it as memory (and keep it available)*/
			vfsfile_t *f;
			f = FS_OpenReadLocation(&loc);
			if (f && loc.len > 0)
			{
				fbase = malloc(loc.len);
				VFS_READ(f, fbase, loc.len);
				VFS_CLOSE(f);

				error = pFT_New_Memory_Face(fontlib, fbase, loc.len, 0, &face);
			}
		}
	}

#ifdef _WIN32
	if (error)
	{
		static qboolean firsttime = true;
		static char fontdir[MAX_OSPATH];

		if (firsttime)
		{
			HMODULE shfolder = LoadLibrary("shfolder.dll");
			firsttime = false;
			if (shfolder)
			{
				HRESULT (WINAPI *dSHGetFolderPath) (HWND hwndOwner, int nFolder, HANDLE hToken, DWORD dwFlags, LPTSTR pszPath);
				dSHGetFolderPath = (void *)GetProcAddress(shfolder, "SHGetFolderPathA");
				if (dSHGetFolderPath)
				{
					// 0x14 == CSIDL_FONTS
					if (dSHGetFolderPath(NULL, 0x14, NULL, 0, fontdir) != S_OK)
						*fontdir = 0;
				}
				FreeLibrary(shfolder);
			}
		}
		if (*fontdir)
		{
			error = pFT_New_Face(fontlib, va("%s/%s.ttf", fontdir, fontfilename), 0, &face);
			if (error)
				error = pFT_New_Face(fontlib, va("%s/%s", fontdir, fontfilename), 0, &face);
		}
	}
#endif
	if (!error)
	{
		error = pFT_Set_Pixel_Sizes(face, 0, height);
		if (!error)
		{
			/*success!*/
			f->membuf = fbase;
			f->face = face;
			return true;
		}
	}
	if (fbase)
		free(fbase);
#endif

	return false;
}

static texid_t Font_LoadReplacementConchars(void)
{
	texid_t tex;
	//q1 replacement
	tex = R_LoadReplacementTexture("gfx/conchars.lmp", NULL, IF_NOMIPMAP|IF_NOGAMMA);
	if (TEXVALID(tex))
		return tex;
	//q2
	tex = R_LoadHiResTexture("pics/conchars.pcx", NULL, IF_NOMIPMAP|IF_NOGAMMA);
	if (TEXVALID(tex))
		return tex;
	//q3
	tex = R_LoadHiResTexture("gfx/2d/bigchars.tga", NULL, IF_NOMIPMAP|IF_NOGAMMA);
	if (TEXVALID(tex))
		return tex;
	return r_nulltex;
}
static texid_t Font_LoadQuakeConchars(void)
{
	unsigned int i;
	qbyte *lump;
	lump = W_SafeGetLumpName ("conchars");
	if (lump)
	{
		// add ocrana leds
		if (con_ocranaleds.ival)
		{
			if (con_ocranaleds.ival != 2 || QCRC_Block(lump, 128*128) == 798)
				AddOcranaLEDsIndexed (lump, 128, 128);
		}

		for (i=0 ; i<128*128 ; i++)
			if (lump[i] == 0)
				lump[i] = 255;	// proper transparent color

		return R_LoadTexture8("charset", 128, 128, (void*)lump, IF_NOMIPMAP|IF_NOGAMMA, 1);
	}
	return r_nulltex;
}

static texid_t Font_LoadHexen2Conchars(qboolean iso88591)
{
	//gulp... so it's come to this has it? rework the hexen2 conchars into the q1 system.
	texid_t tex;
	unsigned int i, x;
	unsigned char *tempchars;
	unsigned char *in, *out, *outbuf;
	FS_LoadFile("gfx/menu/conchars.lmp", (void**)&tempchars);

	/*hexen2's conchars are arranged 32-wide, 16 high.
	the upper 8 rows are 256 8859-1 chars
	the lower 8 rows are a separate set of recoloured 8859-1 chars.

	if we're loading for the fallback then we're loading this data for quake compatibility, 
	so we grab only the first 4 rows of each set of chars (128 low chars, 128 high chars).

	if we're loading a proper charset, then we load only the first set of chars, we can recolour the rest anyway (com_parseutf8 will do so anyway).
	as a final note, parsing iso8859-1 french/german/etc as utf8 will generally result in decoding errors which can gracefully revert to 8859-1 safely. If this premise fails too much, we can always change the parser for different charsets - the engine always uses unicode and thus 8859-1 internally.
	*/
	if (tempchars)
	{
		outbuf = BZ_Malloc(8*8*256*8);

		out = outbuf;
		i = 0;
		/*read the low chars*/
		for (; i < 8*8*1; i+=1)
		{
			if (i&(1<<3))
				in = tempchars + (i>>3)*16*8*8+(i&7)*32*8 - 256*4+128;
			else
				in = tempchars + (i>>3)*16*8*8+(i&7)*32*8;
			for (x = 0; x < 16*8; x++)
				*out++ = *in++;
		}
		if (iso88591)
		{
			/*read the non 8859-1 quake-compat control chars*/
			for (; i < 8*8*1 + 16; i+=1)
			{
				if (i&(1<<3))
					in = tempchars+128*128 + ((i>>3)&7)*16*8*8+(i&7)*32*8 - 256*4+128;
				else
					in = tempchars+128*128 + ((i>>3)&7)*16*8*8+(i&7)*32*8;
				for (x = 0; x < 16*8; x++)
					*out++ = *in++;
			}

			/*read the final low chars (final if 8859-1 anyway)*/
			for (; i < 8*8*2; i+=1)
			{
				if (i&(1<<3))
					in = tempchars + (i>>3)*16*8*8+(i&7)*32*8 - 256*4+128;
				else
					in = tempchars + (i>>3)*16*8*8+(i&7)*32*8;
				for (x = 0; x < 16*8; x++)
					*out++ = *in++;
			}
		}
		else
		{
			/*read the high chars*/
			for (; i < 8*8*2; i+=1)
			{
				if (i&(1<<3))
					in = tempchars+128*128 + ((i>>3)&15)*16*8*8+(i&7)*32*8 - 256*4+128;
				else
					in = tempchars+128*128 + ((i>>3)&15)*16*8*8+(i&7)*32*8;
				for (x = 0; x < 16*8; x++)
					*out++ = *in++;
			}
		}
		FS_FreeFile(tempchars);

		// add ocrana leds
		if (!iso88591 && con_ocranaleds.value && con_ocranaleds.value != 2)
			AddOcranaLEDsIndexed (outbuf, 128, 128);

		for (i=0 ; i<128*128 ; i++)
			if (outbuf[i] == 0)
				outbuf[i] = 255;	// proper transparent color
		tex = R_LoadTexture8 (iso88591?"gfx/menu/8859-1.lmp":"charset", 128, 128, outbuf, IF_NOMIPMAP|IF_NOGAMMA, 1);
		Z_Free(outbuf);
		return tex;
	}
	return r_nulltex;
}

static texid_t Font_LoadFallbackConchars(void)
{
	texid_t tex;
	extern qbyte default_conchar[11356];
	int width, height;
	unsigned int i;
	qbyte *lump;
	qboolean hasalpha;
	lump = ReadTargaFile(default_conchar, sizeof(default_conchar), &width, &height, &hasalpha, false);
	if (!lump)
		Sys_Error("Corrupt internal drawchars");
	/*convert greyscale to alpha*/
	for (i = 0; i < width*height; i++)
	{
		lump[i*4+3] = lump[i*4];
		lump[i*4+0] = 255;
		lump[i*4+1] = 255;
		lump[i*4+2] = 255;
	}
	tex = R_LoadTexture32("charset", width, height, (void*)lump, IF_NOMIPMAP|IF_NOGAMMA);
	BZ_Free(lump);
	return tex;
}

/*loads a fallback image. not allowed to fail (use syserror if needed)*/
static texid_t Font_LoadDefaultConchars(void)
{
	texid_t tex;
	tex = Font_LoadReplacementConchars();
	if (TEXVALID(tex))
		return tex;
	tex = Font_LoadQuakeConchars();
	if (TEXVALID(tex))
		return tex;
	tex = Font_LoadHexen2Conchars(true);
	if (TEXVALID(tex))
		return tex;
	tex = Font_LoadFallbackConchars();
	if (TEXVALID(tex))
		return tex;
	Sys_Error("Unable to load any conchars\n");
}

typedef struct 
{ 
    short		width;
    short		height; 
    short		leftoffset;	// pixels to the left of origin 
    short		topoffset;	// pixels below the origin 
    int			columnofs[1];
} doompatch_t;
typedef struct
{
    unsigned char		topdelta;	// -1 is the last post in a column
    unsigned char		length; 	// length data bytes follows
} doomcolumn_t;
void Doom_ExpandPatch(doompatch_t *p, unsigned char *b, int stride)
{
	doomcolumn_t *col;
	unsigned char *src, *dst;
	int x, y;
	for (x = 0; x < p->width; x++)
	{
		col = (doomcolumn_t *)((unsigned char *)p + p->columnofs[x]);
		while(col->topdelta != 0xff)
		{
			//exploit protection
			if (col->length + col->topdelta > p->height)
				break;

			src = (unsigned char *)col + 3; /*why 3? why not, I suppose*/
			dst = b + stride*col->topdelta;
			for (y = 0; y < col->length; y++)
			{
				*dst = *src++;
				dst += stride;
			}
			col = (doomcolumn_t *)((unsigned char*)col + col->length + 4);
		}
		b++;
	}
}

//creates a new font object from the given file, with each text row with the given height.
//width is implicit and scales with height and choice of font.
struct font_s *Font_LoadFont(int height, char *fontfilename)
{
	struct font_s *f;
	int i = 0;
	int defaultplane;
	char *aname;

	f = Z_Malloc(sizeof(*f));
	f->charheight = height;

#ifdef DOOMWADS
	if (!*fontfilename)
	{
		unsigned char buf[PLANEWIDTH*PLANEHEIGHT];
		int i;
		int x=0,y=0,h=0;
		doompatch_t *dp;

		memset(buf, 0, sizeof(buf));

		for (i = '!'; i <= '_'; i++)
		{
			dp = NULL;
			FS_LoadFile(va("wad/stcfn%.3d", i), (void**)&dp);
			if (!dp)
				break;

			/*make sure it can fit*/
			if (x + dp->width > PLANEWIDTH)
			{
				x = 0;
				y += h;
				h = 0;
			}

			f->chars[i].advance = dp->width;	/*this is how much line space the char takes*/
			f->chars[i].left = -dp->leftoffset;
			f->chars[i].top = -dp->topoffset;
			f->chars[i].nextchar = 0;
			f->chars[i].pad = 0;
			f->chars[i].texplane = SINGLEPLANE;

			f->chars[i].bmx = x;
			f->chars[i].bmy = y;
			f->chars[i].bmh = dp->height;
			f->chars[i].bmw = dp->width;

			Doom_ExpandPatch(dp, &buf[y*PLANEWIDTH + x], PLANEWIDTH);

			x += dp->width;
			if (dp->height > h)
			{
				h = dp->height;
				if (h > f->charheight)
					f->charheight = h;
			}


			FS_FreeFile(dp);
		}

		/*if all loaded okay, replicate the chars to the quake-compat range (both white+red chars)*/
		if (i == '_'+1)
		{
			f->chars[' '].advance = 8;
			f->singletexture = R_LoadTexture8("doomfont", PLANEWIDTH, PLANEHEIGHT, buf, 0, true);
			for (i = 0xe000; i <= 0xe0ff; i++)
			{
				f->chars[i] = f->chars[toupper(i&0x7f)];
			}
			return f;
		}
	}
#endif
	if (!strcmp(fontfilename, "gfx/tinyfont"))
	{
		unsigned int *img;
		int x, y;
		unsigned char *w = W_SafeGetLumpName(fontfilename+4);
		if (!w)
		{
			Z_Free(f);
			return NULL;
		}
		img = Z_Malloc(PLANEWIDTH*PLANEWIDTH*4);
		for (y = 0; y < 32; y++)
			for (x = 0; x < 128; x++)
				img[x + y*PLANEWIDTH] = w[x + y*128]?d_8to24rgbtable[w[x + y*128]]:0;

		f->singletexture = R_LoadTexture(fontfilename,PLANEWIDTH,PLANEWIDTH,TF_RGBA32,img,IF_NOPICMIP|IF_NOMIPMAP);
		Z_Free(img);

		for (i = 0x00; i <= 0xff; i++)
		{
			f->chars[i].advance = height;
			f->chars[i].left = 0;
			f->chars[i].top = 0;
			f->chars[i].nextchar = 0;	//these chars are not linked in
			f->chars[i].pad = 0;
			f->chars[i].texplane = BITMAPPLANE;	/*if its a 'raster' font, don't use the default chars, always use the raster images*/


			if (i >= 'a' && i <= 'z')
			{
				f->chars[i].bmx = ((i - 64)&15)*8;
				f->chars[i].bmy = ((i - 64)/16)*8;
				f->chars[i].bmh = 8;
				f->chars[i].bmw = 8;
			}
			else if (i >= 32 && i < 96)
			{
				f->chars[i].bmx = ((i - 32)&15)*8;
				f->chars[i].bmy = ((i - 32)/16)*8;
				f->chars[i].bmh = 8;
				f->chars[i].bmw = 8;
			}
			else
			{
				f->chars[i].bmh = 0;
				f->chars[i].bmw = 0;
				f->chars[i].bmx = 0;
				f->chars[i].bmy = 0;
			}
		}
		for (i = 0xe000; i <= 0xe0ff; i++)
		{
			f->chars[i] = f->chars[i&0xff];
		}
		return f;
	}

	aname = strstr(fontfilename, ":");
	if (aname)
	{
		*aname = 0;
		f->alt = Font_LoadFont(height, aname+1);
	}
	if (!Font_LoadFreeTypeFont(f, height, fontfilename))
	{
		if (*fontfilename)
			f->singletexture = R_LoadHiResTexture(fontfilename, "fonts", IF_NOMIPMAP);

		/*force it to load, even if there's nothing there*/
		for (; i < 256; i++)
		{
			f->chars[i].advance = f->charheight;
			f->chars[i].bmh = PLANEWIDTH/16;
			f->chars[i].bmw = PLANEWIDTH/16;
			f->chars[i].bmx = (i&15)*(PLANEWIDTH/16);
			f->chars[i].bmy = (i/16)*(PLANEWIDTH/16);
			f->chars[i].left = 0;
			f->chars[i].top = 0;
			f->chars[i].nextchar = 0;	//these chars are not linked in
			f->chars[i].pad = 0;
			f->chars[i].texplane = BITMAPPLANE;
		}
	}
	if (aname)
		*aname = ':';

	defaultplane = BITMAPPLANE;/*assume the bitmap plane - don't use the fallback as people don't think to use com_parseutf8*/
	if (!TEXVALID(f->singletexture))
	{
		if (!TEXVALID(f->singletexture) && !TEXVALID(fontplanes.defaultfont))
			fontplanes.defaultfont = Font_LoadDefaultConchars();

		if (!strcmp(fontfilename, "gfx/hexen2"))
		{
			f->singletexture = Font_LoadHexen2Conchars(false);
			defaultplane = DEFAULTPLANE;
		}
		if (!TEXVALID(f->singletexture))
			f->singletexture = fontplanes.defaultfont;
	}

	for (; i < FONTCHARS; i++)
	{
		f->chars[i].texplane = INVALIDPLANE;
	}

	/*pack the default chars into it*/
	for (i = 0xe000; i <= 0xe0ff; i++)
	{
		f->chars[i].advance = f->charheight;
		f->chars[i].bmh = PLANEWIDTH/16;
		f->chars[i].bmw = PLANEWIDTH/16;
		f->chars[i].bmx = (i&15)*(PLANEWIDTH/16);
		f->chars[i].bmy = ((i/16)*(PLANEWIDTH/16)) & 0xff;
		f->chars[i].left = 0;
		f->chars[i].top = 0;
		f->chars[i].nextchar = 0;	//these chars are not linked in
		f->chars[i].pad = 0;
		f->chars[i].texplane = defaultplane;
	}
	return f;
}

//removes a font from memory.
void Font_Free(struct font_s *f)
{
	struct charcache_s **link;

	if (f->alt)
	{
		Font_Free(f->alt);
		f->alt = NULL;
	}
	for (link = &fontplanes.oldestchar; *link; )
	{
		if (*link >= f->chars && *link <= f->chars + FONTCHARS)
		{
			*link = (*link)->nextchar;
			if (!*link)
				fontplanes.newestchar = NULL;
		}
		else
			link = &(*link)->nextchar;
	}

#ifdef AVAIL_FREETYPE
	if (f->face)
		pFT_Done_Face(f->face);
	if (f->membuf)
		free(f->membuf);
#endif
	free(f);
}

//maps a given virtual screen coord to a pixel coord, which matches the font's height/width values
void Font_BeginString(struct font_s *font, int vx, int vy, int *px, int *py)
{
	curfont = font;
	*px = (vx*(int)vid.rotpixelwidth) / (float)vid.width;
	*py = (vy*(int)vid.rotpixelheight) / (float)vid.height;
}
void Font_Transform(int vx, int vy, int *px, int *py)
{
	if (px)
		*px = (vx*(int)vid.rotpixelwidth) / (float)vid.width;
	if (py)
		*py = (vy*(int)vid.rotpixelheight) / (float)vid.height;
}
void Font_BeginScaledString(struct font_s *font, float vx, float vy, float *px, float *py)
{
	curfont = font;
	*px = vx;
	*py = vy;
}

void Font_EndString(struct font_s *font)
{
	Font_Flush();
	curfont = NULL;
}

//obtains the font's row height (each row of chars should be drawn using this increment)
int Font_CharHeight(void)
{
	return curfont->charheight;
}

/*
This is where the character ends.
Note: this function supports tabs - x must always be based off 0, with Font_LineDraw actually used to draw the line.
*/
int Font_CharEndCoord(struct font_s *font, int x, unsigned int charcode)
{
	struct charcache_s *c;
#define TABWIDTH (8*20)
	if (charcode&CON_HIDDEN)
		return x;
	if ((charcode&CON_CHARMASK) == '\t')
		return x + ((TABWIDTH - (x % TABWIDTH)) % TABWIDTH);

	if (charcode & CON_2NDCHARSETTEXT)
	{
		if (font->alt)
			font = font->alt;
		else if ((charcode & CON_CHARMASK) >= 0x20 && (charcode&CON_CHARMASK) < 0x80)
			charcode |= 0xe000;
	}

	c = Font_GetChar(font, (CHARIDXTYPE)(charcode&CON_CHARMASK));
	if (!c)
	{
		c = Font_TryLoadGlyph(font, (CHARIDXTYPE)(charcode&CON_CHARMASK));
		if (!c)
			return x+0;
	}

	return x+c->advance;
}

//obtains the width of a character from a given font. This is how wide it is. The next char should be drawn at x + result.
int Font_CharWidth(unsigned int charcode)
{
	struct charcache_s *c;
	struct font_s *font = curfont;
	if (charcode&CON_HIDDEN)
		return 0;
	if (charcode & CON_2NDCHARSETTEXT)
	{
		if (font->alt)
			font = font->alt;
		else if ((charcode & CON_CHARMASK) >= 0x20 && (charcode&CON_CHARMASK) < 0x80)
			charcode |= 0xe000;
	}

	c = Font_GetChar(curfont, (CHARIDXTYPE)(charcode&CON_CHARMASK));
	if (!c)
	{
		c = Font_TryLoadGlyph(curfont, (CHARIDXTYPE)(charcode&CON_CHARMASK));
		if (!c)
			return 0;
	}

	return c->advance;
}

//for a given font, calculate the line breaks and word wrapping for a block of text
//start+end are the input string
//starts+ends are an array of line start and end points, which have maxlines elements.
//(end is the terminator, null or otherwise)
//maxpixelwidth is the width of the display area in pixels
int Font_LineBreaks(conchar_t *start, conchar_t *end, int maxpixelwidth, int maxlines, conchar_t **starts, conchar_t **ends)
{
	int l, bt;
	int px;
	int foundlines = 0;
	struct font_s *font = curfont;

	while (start < end)
	{
	// scan the width of the line
		for (px=0, l=0 ; px <= maxpixelwidth; )
		{
			l++;
			if (start+l >= end || (start[l-1]&CON_CHARMASK) == '\n')
				break;
			px = Font_CharEndCoord(font, px, start[l]);
		}
		//if we did get to the end
		if (px > maxpixelwidth)
		{
			bt = l;
			//backtrack until we find a space
			while(l > 0 && (start[l-1]&CON_CHARMASK)>' ')
			{
				l--;
			}
			if (l == 0 && bt>0)
				l = bt-1;
		}

		starts[foundlines] = start;
		ends[foundlines] = start+l;
		foundlines++;
		if (foundlines == maxlines)
			break;

		start+=l;
		if (start == end)
			break;

		if ((*start&CON_CHARMASK) == '\n'||!l)
			start++;                // skip the \n
	}

	return foundlines;
}

int Font_LineWidth(conchar_t *start, conchar_t *end)
{
	int x = 0;
	struct font_s *font = curfont;
	for (; start < end; start++)
	{
		x = Font_CharEndCoord(font, x, *start);
	}
	return x;
}
void Font_LineDraw(int x, int y, conchar_t *start, conchar_t *end)
{
	int lx = 0;
	struct font_s *font = curfont;
	for (; start < end; start++)
	{
		Font_DrawChar(x+lx, y, *start);
		lx = Font_CharEndCoord(font, lx, *start);
	}
}

/*Note: *all* strings after the current one will inherit the same colour, until one changes it explicitly
correct usage of this function thus requires calling this with 1111 before Font_EndString*/
void Font_ForceColour(float r, float g, float b, float a)
{
	if (font_colourmask & CON_NONCLEARBG)
		Font_Flush();
	font_colourmask = CON_WHITEMASK;

	font_forecolour[0] = r*255;
	font_forecolour[1] = g*255;
	font_forecolour[2] = b*255;
	font_forecolour[3] = a*255;

	font_backcolour[3] = 0;

	/*Any drawchars that are now drawn will get the forced colour*/
}
void Font_InvalidateColour(void)
{
	Font_Flush();
	font_colourmask = ~0;
}

//draw a character from the current font at a pixel location.
int Font_DrawChar(int px, int py, unsigned int charcode)
{
	struct charcache_s *c;
	float s0, s1;
	float t0, t1;
	float nextx;
	float sx, sy, sw, sh;
	int col;
	int v;
	struct font_s *font = curfont;
	if (charcode & CON_HIDDEN)
		return px;

	if (charcode & CON_2NDCHARSETTEXT)
	{
		if (font->alt)
			font = font->alt;
		else if ((charcode & CON_CHARMASK) >= 0x20 && (charcode&CON_CHARMASK) < 0x80)
			charcode |= 0xe000;
	}

	//crash if there is no current font.
	c = Font_GetChar(font, (CHARIDXTYPE)(charcode&CON_CHARMASK));
	if (!c)
	{
		c = Font_TryLoadGlyph(font, (CHARIDXTYPE)(charcode&CON_CHARMASK));
		if (!c)
			return px;
	}

	nextx = px + c->advance;

	if ((charcode & CON_CHARMASK) == '\t')
		return px + ((TABWIDTH - (px % TABWIDTH)) % TABWIDTH);
	if ((charcode & CON_CHARMASK) == ' ')
		return nextx;

	if (charcode & CON_BLINKTEXT)
	{
		if (!cl_noblink.ival)
			if ((int)(realtime*3) & 1)
				return nextx;
	}

	col = charcode & (CON_NONCLEARBG|CON_BGMASK|CON_FGMASK|CON_HALFALPHA);
	if (col != font_colourmask)
	{
		if ((col ^ font_colourmask) & CON_NONCLEARBG)
			Font_Flush();
		font_colourmask = col;

		col = (charcode&CON_FGMASK)>>CON_FGSHIFT;
		font_forecolour[0] = consolecolours[col].fr*255;
		font_forecolour[1] = consolecolours[col].fg*255;
		font_forecolour[2] = consolecolours[col].fb*255;
		font_forecolour[3] = (charcode & CON_HALFALPHA)?127:255;

		col = (charcode&CON_BGMASK)>>CON_BGSHIFT;
		font_backcolour[0] = consolecolours[col].fr*255;
		font_backcolour[1] = consolecolours[col].fg*255;
		font_backcolour[2] = consolecolours[col].fb*255;
		font_backcolour[3] = (charcode & CON_NONCLEARBG)?127:0;
	}

	s0 = (float)c->bmx/PLANEWIDTH;
	t0 = (float)c->bmy/PLANEWIDTH;
	s1 = (float)(c->bmx+c->bmw)/PLANEWIDTH;
	t1 = (float)(c->bmy+c->bmh)/PLANEWIDTH;

	switch(c->texplane)
	{
	case DEFAULTPLANE:
		sx = ((px+c->left)*(int)vid.width) / (float)vid.rotpixelwidth;
		sy = ((py+c->top)*(int)vid.height) / (float)vid.rotpixelheight;
		sw = ((font->charheight)*vid.width) / (float)vid.rotpixelwidth;
		sh = ((font->charheight)*vid.height) / (float)vid.rotpixelheight;
		v = Font_BeginChar(fontplanes.defaultfont);
		break;
	case BITMAPPLANE:
		sx = ((px+c->left)*(int)vid.width) / (float)vid.rotpixelwidth;
		sy = ((py+c->top)*(int)vid.height) / (float)vid.rotpixelheight;
		sw = ((font->charheight)*vid.width) / (float)vid.rotpixelwidth;
		sh = ((font->charheight)*vid.height) / (float)vid.rotpixelheight;
		v = Font_BeginChar(font->singletexture);
		break;
	case SINGLEPLANE:
		sx = ((px+c->left)*(int)vid.width) / (float)vid.rotpixelwidth;
		sy = ((py+c->top)*(int)vid.height) / (float)vid.rotpixelheight;
		sw = ((c->bmw)*vid.width) / (float)vid.rotpixelwidth;
		sh = ((c->bmh)*vid.height) / (float)vid.rotpixelheight;
		v = Font_BeginChar(font->singletexture);
		break;
	default:
		sx = ((px+c->left)*(int)vid.width) / (float)vid.rotpixelwidth;
		sy = ((py+c->top)*(int)vid.height) / (float)vid.rotpixelheight;
		sw = ((c->bmw)*vid.width) / (float)vid.rotpixelwidth;
		sh = ((c->bmh)*vid.height) / (float)vid.rotpixelheight;
		v = Font_BeginChar(fontplanes.texnum[c->texplane]);
		break;
	}

	font_texcoord[v+0][0] = s0;
	font_texcoord[v+0][1] = t0;
	font_texcoord[v+1][0] = s1;
	font_texcoord[v+1][1] = t0;
	font_texcoord[v+2][0] = s1;
	font_texcoord[v+2][1] = t1;
	font_texcoord[v+3][0] = s0;
	font_texcoord[v+3][1] = t1;

	font_coord[v+0][0] = sx;
	font_coord[v+0][1] = sy;
	font_coord[v+1][0] = sx+sw;
	font_coord[v+1][1] = sy;
	font_coord[v+2][0] = sx+sw;
	font_coord[v+2][1] = sy+sh;
	font_coord[v+3][0] = sx;
	font_coord[v+3][1] = sy+sh;

	*(int*)font_forecoloura[v+0] = *(int*)font_forecolour;
	*(int*)font_forecoloura[v+1] = *(int*)font_forecolour;
	*(int*)font_forecoloura[v+2] = *(int*)font_forecolour;
	*(int*)font_forecoloura[v+3] = *(int*)font_forecolour;
	*(int*)font_backcoloura[v+0] = *(int*)font_backcolour;
	*(int*)font_backcoloura[v+1] = *(int*)font_backcolour;
	*(int*)font_backcoloura[v+2] = *(int*)font_backcolour;
	*(int*)font_backcoloura[v+3] = *(int*)font_backcolour;

	return nextx;
}

/*there is no sane way to make this pixel-correct*/
float Font_DrawScaleChar(float px, float py, float cw, float ch, unsigned int charcode)
{
	struct charcache_s *c;
	float s0, s1;
	float t0, t1;
	float nextx;
	float sx, sy, sw, sh;
	int col;
	int v;
	struct font_s *font = curfont;
	if (charcode & CON_2NDCHARSETTEXT)
	{
		if (font->alt)
			font = font->alt;
		else if ((charcode & CON_CHARMASK) >= 0x20 && (charcode&CON_CHARMASK) < 0x80)
			charcode |= 0xe000;
	}

	cw /= font->charheight;
	ch /= font->charheight;


	//crash if there is no current font.
	c = Font_GetChar(font, (CHARIDXTYPE)(charcode&CON_CHARMASK));
	if (!c)
	{
		c = Font_TryLoadGlyph(font, (CHARIDXTYPE)(charcode&CON_CHARMASK));
		if (!c)
			return px;
	}

	nextx = px + c->advance*cw;

	if ((charcode & CON_CHARMASK) == ' ')
		return nextx;

	if (charcode & CON_BLINKTEXT)
	{
		if (!cl_noblink.ival)
			if ((int)(realtime*3) & 1)
				return nextx;
	}

	col = charcode & (CON_NONCLEARBG|CON_BGMASK|CON_FGMASK|CON_HALFALPHA);
	if (col != font_colourmask)
	{
		if ((col ^ font_colourmask) & CON_NONCLEARBG)
			Font_Flush();
		font_colourmask = col;

		col = (charcode&CON_FGMASK)>>CON_FGSHIFT;
		font_forecolour[0] = consolecolours[col].fr*255;
		font_forecolour[1] = consolecolours[col].fg*255;
		font_forecolour[2] = consolecolours[col].fb*255;
		font_forecolour[3] = (charcode & CON_HALFALPHA)?127:255;

		col = (charcode&CON_BGMASK)>>CON_BGSHIFT;
		font_backcolour[0] = consolecolours[col].fr*255;
		font_backcolour[1] = consolecolours[col].fg*255;
		font_backcolour[2] = consolecolours[col].fb*255;
		font_backcolour[3] = (charcode & CON_NONCLEARBG)?127:0;
	}

	s0 = (float)c->bmx/PLANEWIDTH;
	t0 = (float)c->bmy/PLANEWIDTH;
	s1 = (float)(c->bmx+c->bmw)/PLANEWIDTH;
	t1 = (float)(c->bmy+c->bmh)/PLANEWIDTH;

	if (c->texplane >= DEFAULTPLANE)
	{
		sx = ((px+c->left));
		sy = ((py+c->top));
		sw = ((font->charheight*cw));
		sh = ((font->charheight*ch));

		if (c->texplane == DEFAULTPLANE)
			v = Font_BeginChar(fontplanes.defaultfont);
		else
			v = Font_BeginChar(font->singletexture);
	}
	else
	{
		sx = ((px+(c->left*(int)vid.width) / (float)vid.rotpixelwidth));
		sy = ((py+(c->top*(int)vid.height) / (float)vid.rotpixelheight));
		sw = ((c->bmw*cw));
		sh = ((c->bmh*ch));
		v = Font_BeginChar(fontplanes.texnum[c->texplane]);
	}

	font_texcoord[v+0][0] = s0;
	font_texcoord[v+0][1] = t0;
	font_texcoord[v+1][0] = s1;
	font_texcoord[v+1][1] = t0;
	font_texcoord[v+2][0] = s1;
	font_texcoord[v+2][1] = t1;
	font_texcoord[v+3][0] = s0;
	font_texcoord[v+3][1] = t1;

	font_coord[v+0][0] = sx;
	font_coord[v+0][1] = sy;
	font_coord[v+1][0] = sx+sw;
	font_coord[v+1][1] = sy;
	font_coord[v+2][0] = sx+sw;
	font_coord[v+2][1] = sy+sh;
	font_coord[v+3][0] = sx;
	font_coord[v+3][1] = sy+sh;

	*(int*)font_forecoloura[v+0] = *(int*)font_forecolour;
	*(int*)font_forecoloura[v+1] = *(int*)font_forecolour;
	*(int*)font_forecoloura[v+2] = *(int*)font_forecolour;
	*(int*)font_forecoloura[v+3] = *(int*)font_forecolour;
	*(int*)font_backcoloura[v+0] = *(int*)font_backcolour;
	*(int*)font_backcoloura[v+1] = *(int*)font_backcolour;
	*(int*)font_backcoloura[v+2] = *(int*)font_backcolour;
	*(int*)font_backcoloura[v+3] = *(int*)font_backcolour;

	return nextx;
}

#endif	//!SERVERONLY
