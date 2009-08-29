#include "quakedef.h"

#ifdef AVAIL_FREETYPE
#include "glquake.h"

#include <ft2build.h>
#include FT_FREETYPE_H 

static FT_Library fontlib;

#define FONTCHARS (1<<16)
#define FONTPLANES (1<<2)	//no more than 16 textures per font
#define PLANEIDXTYPE unsigned char
#define CHARIDXTYPE unsigned short

#define INVALIDPLANE ((1<<(8*sizeof(PLANEIDXTYPE)))-1)
#define PLANEWIDTH (1<<8)
#define PLANEHEIGHT PLANEWIDTH



#define GEN_CONCHAR_GLYPHS 0	//set to 0 or 1 to define whether to generate glyphs from conchars too, or if it should just draw them as glquake always used to
extern cvar_t cl_noblink;

qboolean triedtoloadfreetype;
dllhandle_t *fontmodule;
FT_Error (VARGS *pFT_Init_FreeType)		(FT_Library  *alibrary);
FT_Error (VARGS *pFT_Load_Char)			(FT_Face face, FT_ULong char_code, FT_Int32 load_flags);
FT_Error (VARGS *pFT_Set_Pixel_Sizes)	(FT_Face face, FT_UInt pixel_width, FT_UInt pixel_height);
FT_Error (VARGS *pFT_New_Face)			(FT_Library library, const char *pathname, FT_Long face_index, FT_Face *aface);
FT_Error (VARGS *pFT_Done_Face)			(FT_Face face);


typedef struct font_s
{
	struct charcache_s
	{
		struct charcache_s *nextchar;
		PLANEIDXTYPE texplane;
		unsigned char bmx;
		unsigned char bmy;
		unsigned char bmw;
		unsigned char bmh;
		short top;
		short left;
		unsigned char advance;	//how wide this char is, when drawn
		char pad;
	} chars[FONTCHARS];

	short charheight;

	FT_Face face;
} font_t;

typedef struct {
	int texnum[FONTPLANES];
	unsigned char plane[PLANEWIDTH*PLANEHEIGHT][4];	//tracks the current plane
	PLANEIDXTYPE activeplane;
	unsigned char planerowx;
	unsigned char planerowy;
	unsigned char planerowh;
	qboolean planechanged;

	struct charcache_s *oldestchar;
	struct charcache_s *newestchar;
} fontplanes_t;

static fontplanes_t fontplanes;

void Font_Shutdown(void)
{
	int i;
	for (i = 0; i < FONTPLANES; i++)
		fontplanes.texnum[i] = 0;
	fontplanes.activeplane = 0;
	fontplanes.oldestchar = NULL;
	fontplanes.newestchar = NULL;
	fontplanes.planechanged = 0;
	fontplanes.planerowx = 0;
	fontplanes.planerowy = 0;
	fontplanes.planerowh = 0;
}

void Font_FlushPlane(font_t *f)
{
	/*
	assumption:
	oldest chars must be of the oldest plane
	*/
	if (fontplanes.planechanged)
	{
		GL_Bind(fontplanes.texnum[fontplanes.activeplane]);
		GL_Upload32(NULL, (void*)fontplanes.plane, PLANEWIDTH, PLANEHEIGHT, false, true);
		fontplanes.planechanged = false;
	}

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

struct charcache_s *Font_GetChar(font_t *f, CHARIDXTYPE charidx)
{
	struct charcache_s *c = &f->chars[charidx];
	if (c->texplane == INVALIDPLANE)
	{
		//not cached, can't get.
		return NULL;
	}
	return c;
}

struct charcache_s *Font_LoadGlyphData(font_t *f, CHARIDXTYPE charidx, int alphaonly, void *data, unsigned int bmw, unsigned int bmh, unsigned int pitch)
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

struct charcache_s *Font_TryLoadGlyph(font_t *f, CHARIDXTYPE charidx)
{
	struct charcache_s *c;
	FT_GlyphSlot slot;
	FT_Bitmap *bm;

#if GEN_CONCHAR_GLYPHS != 0
	if (charidx >= 0xe000 && charidx <= 0xe0ff && draw_chars)
	{
		int cpos = charidx & 0xff;
		unsigned int img[64*64], *d;
		unsigned char *s;
		int scale;
		int x,y, ys;
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
		static const char *imgs[] =
		{
			"inv_shotgun",
			"inv_sshotgun",
			"inv_nailgun",
			"inv_snailgun",
			"inv_rlaunch",
			"inv_srlaunch",
			"inv_lightng",

			"inv2_shotgun",
			"inv2_sshotgun",
			"inv2_nailgun",
			"inv2_snailgun",
			"inv2_rlaunch",
			"inv2_srlaunch",
			"inv2_lightng",

			"sb_shells",
			"sb_nails",
			"sb_rocket",
			"sb_cells",

			"sb_armor1",
			"sb_armor2",
			"sb_armor3",

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
			"face_quad"
		};
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
				c->top = f->charheight - (f->charheight - nh) - 1;
				c->advance = nw;
				return c;
			}
		}
	}

	if (pFT_Load_Char(f->face, charidx, FT_LOAD_RENDER))
		return NULL;

	slot = f->face->glyph;
	bm = &slot->bitmap;
	c = Font_LoadGlyphData(f, charidx, true, bm->buffer, bm->width, bm->rows, bm->pitch); 

	if (c)
	{
		c->advance = slot->advance.x >> 6;
		c->left = slot->bitmap_left;
		c->top = slot->bitmap_top;
	}
	return c;
}

struct font_s *Font_LoadFont(int height, char *fontfilename)
{
	int i;
	struct font_s *f;
	FT_Face face;
	int error;
	if (!fontlib)
	{
		dllfunction_t ft2funcs[] =
		{
			{(void**)&pFT_Init_FreeType, "FT_Init_FreeType"},
			{(void**)&pFT_Load_Char, "FT_Load_Char"},
			{(void**)&pFT_Set_Pixel_Sizes, "FT_Set_Pixel_Sizes"},
			{(void**)&pFT_New_Face, "FT_New_Face"},
			{(void**)&pFT_Init_FreeType, "FT_Init_FreeType"},
			{(void**)&pFT_Done_Face, "FT_Done_Face"},
			{NULL, NULL}
		};
		if (triedtoloadfreetype)
			return NULL;
		triedtoloadfreetype = true;

		fontmodule = Sys_LoadLibrary("freetype6", ft2funcs);
		if (!fontmodule)
			return NULL;
		error = pFT_Init_FreeType(&fontlib);
		if (error)
			return NULL;
	}

	//fixme: use FT_Open_Face eventually
	error = pFT_New_Face(fontlib, fontfilename, 0, &face);
#ifdef _WIN32
	if (error)
	{
		char fontdir[MAX_OSPATH];
		HMODULE shfolder = LoadLibrary("shfolder.dll");
		DWORD winver = (DWORD)LOBYTE(LOWORD(GetVersion()));

		if (shfolder)
		{
			HRESULT (WINAPI *dSHGetFolderPath) (HWND hwndOwner, int nFolder, HANDLE hToken, DWORD dwFlags, LPTSTR pszPath);
			dSHGetFolderPath = (void *)GetProcAddress(shfolder, "SHGetFolderPathA");
			if (dSHGetFolderPath)
			{
				// 0x14 == CSIDL_FONTS
				if (dSHGetFolderPath(NULL, 0x14, NULL, 0, fontdir) == S_OK)
				{
					error = pFT_New_Face(fontlib, va("%s/%s.ttf", fontdir, fontfilename), 0, &face);
					if (error)
						error = pFT_New_Face(fontlib, va("%s/%s", fontdir, fontfilename), 0, &face);
				}
			}
			FreeLibrary(shfolder);
		}
	}
#endif
	if (error)
		return NULL;

	error = pFT_Set_Pixel_Sizes(face, 0, height);
	if (error)
	{
		return NULL;
	}

	f = malloc(sizeof(*f));
	memset(f, 0, sizeof(*f));
	f->face = face;
	f->charheight = height;

	for (i = 0; i < FONTPLANES; i++)
	{
		if (!fontplanes.texnum[i])
			fontplanes.texnum[i] = GL_AllocNewTexture();
	}

	for (i = 0; i < FONTCHARS; i++)
	{
		f->chars[i].texplane = INVALIDPLANE;
	}
	return f;
}

void Font_Free(struct font_s *f)
{
	struct charcache_s **link;
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

	pFT_Done_Face(f->face);
	free(f);
}

void GLFont_BeginString(struct font_s *font, int vx, int vy, int *px, int *py)
{
	*px = (vx*glwidth) / (float)vid.width;
	*py = (vy*glheight) / (float)vid.height;
}

int GLFont_CharHeight(struct font_s *font)
{
	if (!font)
		return 8;

	return font->charheight;
}

int GLFont_CharWidth(struct font_s *font, unsigned int charcode)
{
	struct charcache_s *c;

	if (!font)
		return 8;

#if GEN_CONCHAR_GLYPHS == 0
	if ((charcode&CON_CHARMASK) >= 0xe000 && (charcode&CON_CHARMASK) <= 0xe0ff)
		return font->charheight;
#endif

	c = Font_GetChar(font, (CHARIDXTYPE)(charcode&CON_CHARMASK));
	if (!c)
	{
		c = Font_TryLoadGlyph(font, (CHARIDXTYPE)(charcode&CON_CHARMASK));
		if (!c)
			return 0;
	}

	return c->advance;
}

int GLFont_LineBreaks(struct font_s *font, conchar_t *start, conchar_t *end, int scrwidth, int maxlines, conchar_t **starts, conchar_t **ends)
{
	int l, bt;
	int px;
	int foundlines = 0;

	while (start < end)
	{
	// scan the width of the line
		for (px=0, l=0 ; px <= scrwidth;)
		{
			if ((start[l]&CON_CHARMASK) == '\n' || (start+l >= end))
				break;
			l++;
			px += GLFont_CharWidth(font, start[l]);
		}
		//if we did get to the end
		if (px > scrwidth)
		{
			bt = l;
			//backtrack until we find a space
			while(l > 0 && (start[l-1]&CON_CHARMASK)>' ')
			{
				l--;
			}
			if (l == 0 && bt>0)
				l = bt-1;
			px -= GLFont_CharWidth(font, start[l]);
		}

		starts[foundlines] = start;
		ends[foundlines] = start+l;
		foundlines++;
		if (foundlines == maxlines)
			break;

		start+=l;
//		for (l=0 ; l<40 && *start && *start != '\n'; l++)
 //			start++;

		if ((*start&CON_CHARMASK) == '\n'||!l)
			start++;                // skip the \n
	}

	return foundlines;
}

void GLDraw_FillRGB (int x, int y, int w, int h, float r, float g, float b);
int GLFont_DrawChar(struct font_s *font, int px, int py, unsigned int charcode)
{
	struct charcache_s *c;
	float s0, s1;
	float t0, t1;
	float nextx;
	float sx, sy, sw, sh;
	int col;

	if (!font)
		return px;

#if GEN_CONCHAR_GLYPHS == 0
	if ((charcode&CON_CHARMASK) >= 0xe000 && (charcode&CON_CHARMASK) <= 0xe0ff)
	{
		extern char_texture;

		if (charcode == 32)
			return px+font->charheight;		// space

		if (charcode & CON_BLINKTEXT)
		{
			if (!cl_noblink.value)
				if ((int)(realtime*3) & 1)
					return px+font->charheight;
		}

		qglEnable(GL_BLEND);
		qglDisable(GL_ALPHA_TEST);

		col = (charcode & CON_FGMASK) >> CON_FGSHIFT;
		qglColor4f(consolecolours[col].fr, consolecolours[col].fg, consolecolours[col].fb, (charcode & CON_HALFALPHA)?0.5:1);

		charcode &= 0xff;

		sx = ((px)*(int)vid.width) / (float)glwidth;
		sy = ((py+font->charheight/8)*(int)vid.height) / (float)glheight;
		sw = ((font->charheight)*vid.width) / (float)glwidth;
		sh = ((font->charheight)*vid.height) / (float)glheight;

		col = charcode&15;
		s0 = (float)col/16;
		s1 = (float)(col+1)/16;

		col = charcode>>4;
		t0 = (float)col/16;
		t1 = (float)(col+1)/16;
		
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_Bind(char_texture);

		qglBegin(GL_QUADS);
		qglTexCoord2f(s0, t0);
		qglVertex2f(sx, sy);
		qglTexCoord2f(s1, t0);
		qglVertex2f(sx+sw, sy);
		qglTexCoord2f(s1, t1);
		qglVertex2f(sx+sw, sy+sh);
		qglTexCoord2f(s0, t1);
		qglVertex2f(sx, sy+sh);
		qglEnd();
		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		return px+font->charheight;
	}
#endif

	c = Font_GetChar(font, (CHARIDXTYPE)(charcode&CON_CHARMASK));
	if (!c)
	{
		c = Font_TryLoadGlyph(font, (CHARIDXTYPE)(charcode&CON_CHARMASK));
		if (!c)
			return px;
	}

	nextx = px + c->advance;

	if (charcode & CON_BLINKTEXT)
	{
		if (!cl_noblink.value)
			if ((int)(realtime*3) & 1)
				return nextx;
	}

	// draw background
	if (charcode & CON_NONCLEARBG)
	{
		sx = ((px)*(int)vid.width) / (float)glwidth;
		sy = ((py+(int)font->charheight/3)*(int)vid.height) / (float)glheight;
		sw = ((c->advance)*vid.width) / (float)glwidth;
		sh = ((font->charheight)*vid.height) / (float)glheight;

		col = (charcode & CON_BGMASK) >> CON_BGSHIFT;
		GLDraw_FillRGB(sx, sy, sw, sh, consolecolours[col].fr, consolecolours[col].fg, consolecolours[col].fb);
	}


	sx = ((px+c->left)*(int)vid.width) / (float)glwidth;
	sy = ((py+font->charheight-c->top)*(int)vid.height) / (float)glheight;
	sw = ((c->bmw)*vid.width) / (float)glwidth;
	sh = ((c->bmh)*vid.height) / (float)glheight;

	s0 = (float)c->bmx/PLANEWIDTH;
	t0 = (float)c->bmy/PLANEWIDTH;
	s1 = (float)(c->bmx+c->bmw)/PLANEWIDTH;
	t1 = (float)(c->bmy+c->bmh)/PLANEWIDTH;

	qglEnable(GL_BLEND);
	qglDisable(GL_ALPHA_TEST);

	col = (charcode & CON_FGMASK) >> CON_FGSHIFT;
	qglColor4f(consolecolours[col].fr, consolecolours[col].fg, consolecolours[col].fb, (charcode & CON_HALFALPHA)?0.5:1);

	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if (fontplanes.planechanged)
	{
		GL_Bind(fontplanes.texnum[fontplanes.activeplane]);
		GL_Upload32(NULL, (void*)fontplanes.plane, PLANEWIDTH, PLANEHEIGHT, false, true);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		fontplanes.planechanged = false;
	}
	GL_Bind(fontplanes.texnum[c->texplane]);

	qglBegin(GL_QUADS);
	qglTexCoord2f(s0, t0);
	qglVertex2f(sx, sy);
	qglTexCoord2f(s1, t0);
	qglVertex2f(sx+sw, sy);
	qglTexCoord2f(s1, t1);
	qglVertex2f(sx+sw, sy+sh);
	qglTexCoord2f(s0, t1);
	qglVertex2f(sx, sy+sh);
	qglEnd();
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	return nextx;
}
#endif
