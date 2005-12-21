//contains routines for blending images (as well as blitting 32bit to 8bit type stuff)


#include "quakedef.h"
#include "d_local.h"
#include "r_local.h"

void MakeVideoPalette(void);
void MakeSwizzledPalette(void);
void MakeFullbrightRemap(void);

int *srctable;
int *dsttable;
qbyte *pal555to8;

int swzpal[TRANS_LEVELS][256];
qbyte nofbremap[256];

#define palette host_basepal
#define _abs(x) ((x)*(x))

void D_InitTrans(void)
{
	// create pal555to8 and swizzled palette 
	MakeVideoPalette();
	MakeSwizzledPalette();
	MakeFullbrightRemap();

	srctable = swzpal[0];
	dsttable = swzpal[TRANS_MAX];
}

#if 0
#define Trans(p, p2)	(t_curlookupp[p][p2])
#else
// TODO: INLINE THESE FUNCTIONS
qbyte FASTCALL Trans(qbyte p, qbyte p2)
{	
	int x;

	x = (srctable[p] + dsttable[p2]) | 0x01F07C1F;
	return pal555to8[x & (x >> 15)];

}
#endif

qbyte FASTCALL AddBlend(qbyte p, qbyte p2)
{
	int x, y;

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

void D_SetTransLevel(float level, blendmode_t blend)	//MUST be between 0 and 1
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
qbyte GetPalette(int red, int green, int blue)
{
	if (pal555to8)	//fast precalculated method
		return FindPalette(red,green,blue);
	else	//slow, horrible method.
		return FindIndexFromRGB(red, green, blue);
}

qbyte GetPaletteNoFB(int red, int green, int blue)
{
	if (pal555to8)	//fast precalculated (but ugly) method
		return nofbremap[FindPalette(red,green,blue)];
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
	if (r_palconvwrite.value)
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

void MakeFullbrightRemap(void)
{
	int i;

	for (i = 0; i < 256 - vid.fullbright; i++)
		nofbremap[i] = i;
	for (i = 256 - vid.fullbright; i < 256; i++)
		nofbremap[i] = FindIndexFromRGBNoFB(host_basepal[i*3], host_basepal[i*3+1], host_basepal[i*3+2]);
}

// colormap functions
void BuildModulatedColormap(qbyte *indexes, int red, int green, int blue, qboolean desaturate, qboolean fullbrights)
{
	qbyte *rgb = host_basepal;
	unsigned int r, g, b, x, invmask = 0;

	if (red < 0 || green < 0 || blue < 0)
		invmask = 0xff;

	// generate colormap
	
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
				indexes[x] = GetPalette(r^invmask, g^invmask, b^invmask);
			else
				indexes[x] = GetPaletteNoFB(r^invmask, g^invmask, b^invmask);
			rgb += 3;
		}
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
				indexes[x] = GetPalette(r^invmask, g^invmask, b^invmask);
			else
				indexes[x] = GetPaletteNoFB(r^invmask, g^invmask, b^invmask);
			rgb += 3;
		}
	}

}

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
