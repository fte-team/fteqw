//contains routines for blending images (as well as blitting 32bit to 8bit type stuff)


#include "quakedef.h"
#include "d_local.h"
#include "r_local.h"

void MakeVideoPalette(void);
void MakeSwizzledPalette(void);
void MakePaletteRemaps(void);

int *srctable;
int *dsttable;
qbyte *pal555to8;

int swzpal[TRANS_LEVELS][256];

// menutint
palremap_t *mtpalremap;

// IB remap
palremap_t *ib_remap;

#define palette host_basepal
#define _abs(x) ((x)*(x))

void D_ShutdownTrans(void)
{
	if (pal555to8)
	{
		BZ_Free(pal555to8);
		pal555to8 = NULL;
	}

	if (palremaps)
	{
		BZ_Free(palremaps);
		palremapsize = 0;
		palremaps = NULL;
	}

	mtpalremap = NULL;
	ib_remap = NULL;
}

void D_InitTrans(void)
{
	// create pal555to8 and swizzled palette 
	MakeVideoPalette();
	MakeSwizzledPalette();
	MakePaletteRemaps();

	srctable = swzpal[0];
	dsttable = swzpal[TRANS_MAX];
	ib_remap = D_IdentityRemap();
}

// TODO: INLINE THESE FUNCTIONS
qbyte Trans(qbyte p, qbyte p2)
{	
	int x;

	x = (srctable[p] + dsttable[p2]) | 0x01F07C1F;
	return pal555to8[x & (x >> 15)];

}

qbyte AddBlend(qbyte p, qbyte p2)
{
	unsigned int x, y;

	x = (srctable[p] + dsttable[p2]);
	y = x & 0x40100400; // overflow bits
	x = (x | 0x01F07C1F) & 0x3FFFFFFF;
	y = y - (y >> 5);
	x = x | y;
	return pal555to8[x & (x >> 15)];
}

/*
void Set_TransLevelI(int level)
{
	t_curtable = level/(100.0f/(t_numtables-1));
	t_curlookupp = t_lookup[t_curtable];
}
*/

void D_SetTransLevel(float level, blendmode_t blend)
{
	int ilvl;

	// cap and set level
	ilvl = (bound(0, level, 1) * (TRANS_MAX + 0.99));

	// set blending tables
	switch (blend)
	{
	case BM_ADD:
		dsttable = swzpal[ilvl];
		srctable = swzpal[TRANS_MAX];
		break;
	default:
		dsttable = swzpal[ilvl];
		srctable = swzpal[TRANS_MAX - ilvl];
	}
}


#define _abs(x) ((x)*(x))
qbyte FindIndexFromRGB(int red, int green, int blue)
{
	int i, best=15;
	int bestdif=256*256*256, curdif;
	extern qbyte *host_basepal;
	qbyte *pa;

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

qbyte FindIndexFromRGBNoFB(int red, int green, int blue)
{
	int i, best=15;
	int bestdif=256*256*256, curdif;
	extern qbyte *host_basepal;
	qbyte *pa;

	pa = host_basepal;
	for (i = 0; i < 256 - vid.fullbright; i++, pa+=3)
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

#define FindPalette(r,g,b) pal555to8[((r&0xF8)>>3)|((g&0xF8)<<2)|((b&0xF8)<<7)]
qbyte GetPaletteIndex(int red, int green, int blue)
{
	if (pal555to8)	//fast precalculated method
		return FindPalette(red,green,blue);
	else	//slow, horrible method.
		return FindIndexFromRGB(red, green, blue);
}

qbyte GetPaletteNoFB(int red, int green, int blue)
{
	if (pal555to8)	//fast precalculated (but ugly) method
		return fbremapidx(FindPalette(red,green,blue));
	else	//slow, horrible (but accurate) method.
		return FindIndexFromRGBNoFB(red, green, blue);
}

void MakeVideoPalette(void)
{
	vfsfile_t *f;
	int r, g, b;

	// allocate memory
	if (!pal555to8)
		pal555to8 = BZ_Malloc(PAL555_SIZE);
	// pal555to8 = Hunk_AllocName(PAL555_SIZE, "RGB data");

	// load in previously created table
	if ((f = FS_OpenVFS("pal555.pal", "rb", FS_BASE)))
	{
		VFS_READ(f, pal555to8, PAL555_SIZE);
		VFS_CLOSE(f);
		return;
	}

	// create palette conversion table
	for (b = 0; b < 32; b++)
		for (g = 0; g < 32; g++)
			for (r = 0; r < 32; r++)
				pal555to8[r | (g << 5) | (b << 10)] =
					FindIndexFromRGB(r<<3|r>>2, g<<3|g>>2, b<<3|b>>2);

	// write palette conversion table
	if (d_palconvwrite.value)
		COM_WriteFile("pal555.pal", pal555to8, PAL555_SIZE);
}

void MakeSwizzledPalette(void)
{
	int idx, lvl;
	qbyte *pa;

	// create swizzled palettes
	for (lvl = 0; lvl < TRANS_LEVELS; lvl++)
	{
		pa = host_basepal;
		for (idx = 0; idx < 256; idx++)
		{
			// create a b10r10g10 table for each alpha level
			// may need some hacking due to the tendancy of
			// identity merges becoming darker
			swzpal[lvl][idx]  = ( (pa[0] * lvl) >> 4 ) << 10; // red
			swzpal[lvl][idx] |= ( (pa[1] * lvl) >> 4 );       // green
			swzpal[lvl][idx] |= ( (pa[2] * lvl) >> 4 ) << 20; // blue
			swzpal[lvl][idx]  = swzpal[lvl][idx] & 0x3feffbff;
			pa += 3;
		}	
	}
}

// colormap functions
// REMAPKEY macro defines the palette remap key
// d = desaturate (0/1), f = fullbrights (0/1), t = top color, b = bottom color
#define REMAPKEY(d, f, t, b) (0x1 ^ (d<<1) ^ (f<<2) ^ (t<<3) ^ (b<<7))
#define DEREFDEFAULT -2147483647 // lowest negative 32-bit number (without MSVC being stupid)

void MakePaletteRemaps(void)
{
	int i;

	palremapsize = d_palremapsize.value;

	if (palremapsize < 4)
	{
		Con_Printf("Invalid size for d_palremapsize, defaulting to 4.\n");
		palremapsize = 4;
	}

	palremaps = BZ_Malloc(sizeof(palremap_t)*palremapsize);

	// build identity remap
	palremaps[0].r = palremaps[0].g = palremaps[0].b = 255;
	palremaps[0].key = REMAPKEY(0, 1, TOP_DEFAULT, BOTTOM_DEFAULT);
	palremaps[0].references = 999;
	for (i = 0; i < 256; i++)
		palremaps[0].pal[i] = i;

	// build fullbright remap
	palremaps[1].r = palremaps[1].g = palremaps[1].b = 255;
	palremaps[1].key = REMAPKEY(0, 0, TOP_DEFAULT, BOTTOM_DEFAULT);
	palremaps[1].references = 999;
	for (i = 0; i < 256 - vid.fullbright; i++)
		palremaps[1].pal[i] = i;
	for (i = 256 - vid.fullbright; i < 256; i++)
		palremaps[1].pal[i] = FindIndexFromRGBNoFB(host_basepal[i*3], host_basepal[i*3+1], host_basepal[i*3+2]);

	for (i = 2; i < palremapsize; i++)
	{
		palremaps[i].key = 0;
		palremaps[i].references = DEREFDEFAULT;
	}
}

void BuildModulatedPalette(qbyte *indexes, int red, int green, int blue, qboolean desaturate, qboolean fullbrights, int topcolor, int bottomcolor)
{
	qbyte *rgb = host_basepal;
	unsigned int r, g, b, x, invmask = 0;

	if (red < 0 || green < 0 || blue < 0)
		invmask = 0xff;

	// generate palette remap
	if (desaturate)
	{
		int s;

		for (x = 0; x < 256; x++)
		{
			s = rgb[0]*76 + rgb[1]*151 + rgb[2]*29 + 128;
			r = abs((127*256 + s*red) >> 16);
			g = abs((127*256 + s*green) >> 16);
			b = abs((127*256 + s*blue) >> 16);

			if (r > 255)
				r = 255;
			if (g > 255)
				g = 255;
			if (b > 255)
				b = 255;

			if (fullbrights) // relying on branch prediction here...
				indexes[x] = GetPaletteIndex(r^invmask, g^invmask, b^invmask);
			else
				indexes[x] = GetPaletteNoFB(r^invmask, g^invmask, b^invmask);
			rgb += 3;
		}
	}
	else if (red == 255 && green == 255 && blue == 255)
	{
		// identity merge
		if (fullbrights)
			memcpy(indexes, identityremap.pal, sizeof(identityremap.pal));
		else
			memcpy(indexes, fullbrightremap.pal, sizeof(fullbrightremap.pal));
	}
	else
	{
		for (x = 0; x < 256; x++)
		{
			// modulus math
			r = abs((127 + rgb[0]*red) >> 8);
			g = abs((127 + rgb[1]*green) >> 8);
			b = abs((127 + rgb[2]*blue) >> 8);

			if (r > 255)
				r = 255;
			if (g > 255)
				g = 255;
			if (b > 255)
				b = 255;

			if (fullbrights) // relying on branch prediction here...
				indexes[x] = GetPaletteIndex(r^invmask, g^invmask, b^invmask);
			else
				indexes[x] = GetPaletteNoFB(r^invmask, g^invmask, b^invmask);
			rgb += 3;
		}
	}

	// handle top/bottom remap
	if (topcolor == TOP_DEFAULT && bottomcolor == BOTTOM_DEFAULT)
		return;

	{
		qbyte topcolors[16];
		qbyte bottomcolors[16];

		topcolor = topcolor * 16;
		bottomcolor = bottomcolor * 16;

		for (x = 0; x < 16; x++)
		{
			if (topcolor < 128)
				topcolors[x] = indexes[topcolor + x];
			else
				topcolors[x] = indexes[topcolor + 15 - x];

			if (bottomcolor < 128)
				bottomcolors[x] = indexes[bottomcolor + x];
			else
				bottomcolors[x] = indexes[bottomcolor + 15 - x];
		}

		for (x = 0; x < 16; x++)
		{
			indexes[TOP_RANGE + x] = topcolors[x];
			indexes[BOTTOM_RANGE + x] = bottomcolors[x];
		}
	}
}

palremap_t *D_GetPaletteRemap(int red, int green, int blue, qboolean desaturate, qboolean fullbrights, int topcolor, int bottomcolor)
{
	int i, key, deref = -1, dereflast = 1;

	topcolor = topcolor & 0xf;
	bottomcolor = bottomcolor & 0xf;

	key = REMAPKEY(desaturate, fullbrights, topcolor, bottomcolor);

	for (i = 0; i < palremapsize; i++)
	{
		if (palremaps[i].r == red &&
			palremaps[i].g == green &&
			palremaps[i].b == blue && 
			palremaps[i].key == key)
		{
			if (palremaps[i].references < 1)
				palremaps[i].references = 1;
			else
				palremaps[i].references++;
			return palremaps + i;
		}
		else if (palremaps[i].references < dereflast)
		{
			deref = i;
			dereflast = palremaps[i].references;
		}
	}

	if (deref < 2) // no remaps found and all maps are referenced
		return palremaps; // identity remap

	// return non-referenced map
	BuildModulatedPalette(palremaps[deref].pal, red, green, blue, desaturate, fullbrights, topcolor, bottomcolor);
	if (palremaps[deref].references < 1)
		palremaps[deref].references = 1;
	else
		palremaps[deref].references++;
	palremaps[deref].r = red;
	palremaps[deref].g = green;
	palremaps[deref].b = blue;
	palremaps[deref].key = key;
	return palremaps + deref;
}

palremap_t *RebuildMenuTint(struct cvar_s *var)
{
	vec3_t rgb;

	if (var->string[0])
		SCR_StringToRGB(var->string, rgb, 1);
	else
		return NULL;

	return D_GetPaletteRemap(rgb[0]*255, rgb[1]*255, rgb[2]*255, true, true, TOP_DEFAULT, BOTTOM_DEFAULT);
}

void D_DereferenceRemap(palremap_t *palremap)
{
	static int dereftime;

	if (palremap && palremap >= palremaps+2)
	{
		if (palremap->references < 2)
		{
			if (dereftime >= 0)
				dereftime = DEREFDEFAULT;
			palremap->references = dereftime;
			dereftime++;
		}
		else
			palremap->references--;
	}
}

void SWR_Menutint_Callback(struct cvar_s *var, char *oldvalue)
{
	if (mtpalremap)
		D_DereferenceRemap(mtpalremap);

	mtpalremap = RebuildMenuTint(var);
}

qbyte *D_GetMenuTintPal(void)
{
	if (mtpalremap && mtpalremap != palremaps)
		return mtpalremap->pal;	
	else
		return NULL;
}

struct palremap_s *D_IdentityRemap(void) // TODO: explicitly inline this
{
	return palremaps;
}

#undef REMAPKEY
#undef DEREFDEFAULT

void MediaSW_ShowFrame8bit(qbyte *framedata, int inwidth, int inheight, qbyte *palette)
{
	int y, x;

	D_EnableBackBufferAccess ();	// of all overlay stuff if drawing directly
	if (r_pixbytes == 1)
	{
		qbyte *dest, *src;
		int lines=vid.conheight;
		int v;
		int f, fstep;

		dest = vid.conbuffer;

		for (y=0 ; y<lines ; y++, dest += vid.conrowbytes)
		{
			v = (vid.conheight - lines + y)*inheight/vid.conheight;
			src = framedata + v*inwidth;
			{
				f = 0;
				fstep = (inwidth<<16)/vid.conwidth;
				for (x=0 ; x<vid.conwidth ; x+=4)
				{
					dest[x] = FindPalette(palette[src[(f>>16)]*3], palette[src[(f>>16)]*3+1], palette[src[(f>>16)]*3+2]);
					f += fstep;
					dest[x+1] = FindPalette(palette[src[(f>>16)]*3], palette[src[(f>>16)]*3+1], palette[src[(f>>16)]*3+2]);
					f += fstep;
					dest[x+2] = FindPalette(palette[src[(f>>16)]*3], palette[src[(f>>16)]*3+1], palette[src[(f>>16)]*3+2]);
					f += fstep;
					dest[x+3] = FindPalette(palette[src[(f>>16)]*3], palette[src[(f>>16)]*3+1], palette[src[(f>>16)]*3+2]);
					f += fstep;
				}
			}
		}
	}
	else if (r_pixbytes == 2)
	{
		/*	this still expects 32bit input
extern int redbits, redshift;
extern int greenbits, greenshift;
extern int bluebits, blueshift;

		unsigned short *dest;
		qbyte *src;
		int lines=vid.conheight;
		int v;
		int f, fstep;

		dest = (unsigned short *)vid.conbuffer;

		for (y=0 ; y<lines ; y++, dest += vid.conrowbytes)
		{
			v = (vid.conheight - lines + y)*inheight/vid.conheight;
			src = framedata + v*inwidth*4;
			{
				f = 0;
				fstep = ((inwidth)*0x10000)/vid.conwidth;
				for (x=0 ; x<vid.conwidth; x++)	//sw 32 bit rendering is bgrx
				{
					dest[x] = (((src[(f>>16)*4]*(1<<redbits))/256)<<redshift) + (((src[(f>>16)*4+1]*(1<<greenbits))/256)<<greenshift) + (((src[(f>>16)*4+2]*(1<<bluebits))/256)<<blueshift);
					f += fstep;
				}
			}
		}
		*/
	}
	else if (r_pixbytes == 4)
	{
		qbyte *dest, *src;
		int lines=vid.conheight;
		int v;
		int f, fstep;

		dest = vid.conbuffer;

		for (y=0 ; y<lines ; y++, dest += vid.conrowbytes*4)
		{
			v = (vid.conheight - lines + y)*inheight/vid.conheight;
			src = framedata + v*inwidth;
			{
				f = 0;
				fstep = ((inwidth)*0x10000)/vid.conwidth;
				for (x=0 ; x<vid.conwidth*4 ; x+=4)	//sw 32 bit rendering is bgrx
				{
					dest[x] = palette[src[(f>>16)]*3+2];
					dest[x+1] = palette[src[(f>>16)]*3+1];
					dest[x+2] = palette[src[(f>>16)]*3];
					f += fstep;
				}
			}
		}
	}
	else
		Sys_Error("24 bit rendering?");

	D_DisableBackBufferAccess ();	// for adapters that can't stay mapped in

	SCR_SetUpToDrawConsole();
	if  (scr_con_current)
	SCR_DrawConsole (false);

	M_Draw(0);
}

void MediaSW_ShowFrameRGBA_32(qbyte *framedata, int inwidth, int inheight)	//top down
{
	int y, x;

		D_EnableBackBufferAccess ();	// of all overlay stuff if drawing directly
		if (r_pixbytes == 1)
		{
			qbyte *dest, *src;
			int lines=vid.conheight;
			int v;
			int f, fstep;

			dest = vid.conbuffer;

				for (y=0 ; y<lines ; y++, dest += vid.conrowbytes)
				{
					v = (vid.conheight - lines + y)*inheight/vid.conheight;
					src = framedata + v*inwidth*4;
					{
						f = 0;
						fstep = ((inwidth)*0x10000)/vid.conwidth;
						for (x=0 ; x<vid.conwidth ; x+=4)
						{
							dest[x] = FindPalette(src[(f>>16)*4], src[(f>>16)*4+1], src[(f>>16)*4+2]);
							f += fstep;
							dest[x+1] = FindPalette(src[(f>>16)*4], src[(f>>16)*4+1], src[(f>>16)*4+2]);
							f += fstep;
							dest[x+2] = FindPalette(src[(f>>16)*4], src[(f>>16)*4+1], src[(f>>16)*4+2]);
							f += fstep;
							dest[x+3] = FindPalette(src[(f>>16)*4], src[(f>>16)*4+1], src[(f>>16)*4+2]);
							f += fstep;
						}
					}
				}
		}
		else if (r_pixbytes == 2)
		{
extern int redbits, redshift;
extern int greenbits, greenshift;
extern int bluebits, blueshift;

				unsigned short *dest;
				qbyte *src;
				int lines=vid.conheight;
				int v;
				int f, fstep;

				dest = (unsigned short *)vid.conbuffer;

				for (y=0 ; y<lines ; y++, dest += vid.conrowbytes)
				{
					v = (vid.conheight - lines + y)*inheight/vid.conheight;
					src = framedata + v*inwidth*4;
					{
						f = 0;
						fstep = ((inwidth)*0x10000)/vid.conwidth;
						for (x=0 ; x<vid.conwidth; x++)	//sw 32 bit rendering is bgrx
						{
							dest[x] = (((src[(f>>16)*4]*(1<<redbits))/256)<<redshift) + (((src[(f>>16)*4+1]*(1<<greenbits))/256)<<greenshift) + (((src[(f>>16)*4+2]*(1<<bluebits))/256)<<blueshift);
							f += fstep;
						}
					}
				}
		}
		else if (r_pixbytes == 4)
		{
				qbyte *dest, *src;
				int lines=vid.conheight;
				int v;
				int f, fstep;

				dest = vid.conbuffer;

				for (y=0 ; y<lines ; y++, dest += vid.conrowbytes*4)
				{
					v = (vid.conheight - lines + y)*inheight/vid.conheight;
					src = framedata + v*inwidth*4;
					{
						f = 0;
						fstep = ((inwidth)*0x10000)/vid.conwidth;
						for (x=0 ; x<vid.conwidth*4 ; x+=4)	//sw 32 bit rendering is bgrx
						{
							dest[x] = src[(f>>16)*4+2];
							dest[x+1] = src[(f>>16)*4+1];
							dest[x+2] = src[(f>>16)*4];
							f += fstep;
						}
					}
				}
		}
		else
			Sys_Error("24 bit rendering?");

		D_DisableBackBufferAccess ();	// for adapters that can't stay mapped in

	SCR_SetUpToDrawConsole();
	if  (scr_con_current)
	SCR_DrawConsole (false);
}

void MediaSW_ShowFrameBGR_24_Flip(qbyte *framedata, int inwidth, int inheight)	//input is bottom up...
{
	int y, x;

		D_EnableBackBufferAccess ();	// of all overlay stuff if drawing directly
		if (r_pixbytes == 1)
		{
			qbyte *dest, *src;
			int lines=vid.conheight;
			int v;
			int f, fstep;

			dest = vid.conbuffer;

				for (y=0 ; y<lines ; y++, dest += vid.conrowbytes)
				{
					v = (lines - y)*inheight/vid.conheight;
					src = framedata + v*inwidth*3;
					{
						f = 0;
						fstep = ((inwidth)*0x10000)/vid.conwidth;
						for (x=0 ; x<vid.conwidth ; x+=4)
						{
							dest[x] = FindPalette(src[(f>>16)*3+2], src[(f>>16)*3+1], src[(f>>16)*3]);
							f += fstep;
							dest[x+1] = FindPalette(src[(f>>16)*3+2], src[(f>>16)*3+1], src[(f>>16)*3]);
							f += fstep;
							dest[x+2] = FindPalette(src[(f>>16)*3+2], src[(f>>16)*3+1], src[(f>>16)*3]);
							f += fstep;
							dest[x+3] = FindPalette(src[(f>>16)*3+2], src[(f>>16)*3+1], src[(f>>16)*3]);
							f += fstep;
						}
					}
				}
		}
		else if (r_pixbytes == 2)
		{
extern int redbits, redshift;
extern int greenbits, greenshift;
extern int bluebits, blueshift;

				unsigned short *dest;
				qbyte *src;
				int lines=vid.conheight;
				int v;
				int f, fstep;

				dest = (unsigned short *)vid.conbuffer;

				for (y=0 ; y<lines ; y++, dest += vid.conrowbytes)
				{
					v = (lines - y)*inheight/vid.conheight;
					src = framedata + v*inwidth*3;
					{
						f = 0;
						fstep = ((inwidth)*0x10000)/vid.conwidth;
						for (x=0 ; x<vid.conwidth; x++)	//sw 32 bit rendering is bgrx
						{
							dest[x] = (((src[(f>>16)*3+2]*(1<<redbits))/256)<<redshift) + (((src[(f>>16)*3+1]*(1<<greenbits))/256)<<greenshift) + (((src[(f>>16)*3+0]*(1<<bluebits))/256)<<blueshift);
							f += fstep;
						}
					}
				}
		}
		else if (r_pixbytes == 4)
		{
				unsigned int *dest;
				qbyte *src;
				int lines=vid.conheight;
				int v;
				int f, fstep;

				dest = (unsigned int *)vid.conbuffer;

				for (y=0 ; y<lines ; y++, dest += vid.conrowbytes)
				{
					v = (lines - y)*inheight/vid.conheight;
					src = framedata + v*inwidth*3;
					{
						f = 0;
						fstep = ((inwidth)*0x10000)/vid.conwidth;
						for (x=0 ; x<vid.conwidth ; x++)	//sw 32 bit rendering is bgrx
						{
							*(dest+x) = *(int *)(src + (f>>16)*3);
							f += fstep;
						}
					}
				}
		}
		else
			Sys_Error("24 bit rendering?");

		D_DisableBackBufferAccess ();	// for adapters that can't stay mapped in

	SCR_SetUpToDrawConsole();
	if  (scr_con_current)
	SCR_DrawConsole (false);
}
