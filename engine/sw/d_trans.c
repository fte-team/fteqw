//contains routines for blending images (as well as blitting 32bit to 8bit type stuff)


#include "quakedef.h"
#include "d_local.h"
#include "r_local.h"

void MakeVideoPalette(void);
int t_numtables;

qbyte p1multitable[] = {1, 99, 49, 97, 48, 19, 47, 93, 23, 91, 9, 89, 22, 87, 43, 17, 21, 83, 41, 81, 4, 79, 39, 77, 19, 3, 37, 73, 18, 71, 7, 69, 17, 67, 33, 13, 16, 63, 31, 61, 3, 59, 29, 57, 14, 11, 27, 53, 13, 51, 1, 49, 12, 47, 23,  9, 11, 43, 21, 41, 2, 39, 19, 37,  9,  7, 17, 33,  8, 31, 3, 29,  7, 27, 13, 1,  6, 23, 11, 21, 1, 19,  9, 17,  4,  3,  7, 13,  3, 11, 1,  9,  2,  7,  3,  1,  1,  3,  1,  1, 0};
qbyte p2multitable[] = {0,  1,  1,  3,  2,  1,  3,  7,  2,  9, 1, 11,  3, 13,  7,  3,  4, 17,  9, 19, 1, 21, 11, 23,  6, 1, 13, 27,  7, 29, 3, 31,  8, 33, 17,  7,  9, 37, 19, 39, 2, 41, 21, 43, 11,  9, 23, 47, 12, 49, 1, 51, 13, 53, 27, 11, 14, 57, 29, 59, 3, 61, 31, 63, 16, 13, 33, 67, 17, 69, 7, 71, 18, 73, 37, 3, 19, 77, 39, 79, 4, 81, 41, 83, 21, 17, 43, 87, 22, 89, 9, 91, 23, 93, 47, 19, 24, 97, 49, 99, 1};
tlookup *t_lookup;
tlookupp *t_curlookupp;
int t_curtable;

int t_numtables;
int t_numtablesinv;//numtables/65546

int t_state;

#define palette host_basepal
#define _abs(x) ((x)*(x))

void R_ReverseTable(int table)
{
	int p, p2, temp;

	for (p = 0; p < 256; p++)
		for (p2 = p+1; p2 < 256; p2++)
		{
			temp = (t_lookup[table])[p][p2];
			(t_lookup[table])[p][p2] = (t_lookup[table])[p2][p];
			(t_lookup[table])[p2][p] = temp;
		}
}

void R_CalcTransTable(int table, int level)
{
	FILE * f;		
	int p;
	int p2;
	int r, g, b,  j;
	int i;
	unsigned char *pa;
	int m, rvr;
	int dif, curdif;

	qbyte p1multi;
	qbyte p2multi;
	qbyte pixdivide;

	p1multi = p1multitable[level];
	p2multi = p2multitable[level];

	pixdivide = p1multi + p2multi;

	if (level > 99) // trivial generate for p2
	{
		for (p = 0; p < 256; p++)
			for (p2 = 0; p2 < 256; p2++)
				(t_lookup[table])[p][p2] = p2;
		return;
	}

	if (level < 1) // trivial generate for p
	{
		for (p = 0; p < 256; p++)
			for (p2 = 0; p2 < 256; p2++)
				(t_lookup[table])[p][p2] = p;
		return;
	}

	if (level > 50)
	{
		level = 100 - level;
		rvr = 1;
	}
	else
		rvr = 0;

	COM_FOpenFile (va("data/ttable%i.dat", (int) level) , &f);  //we can ignore the filesize return value
	if (f)
	{
		if (fread (t_lookup[table], 256, 256, f) == 256)
		{
			if (rvr)
				R_ReverseTable(table);
			fclose(f);
			return;
		}		
		fclose(f);
	}

	Con_Printf("Generating transtable %i%%\n", level);

	for (p = 0; p < 256; p++)
	{
		j = p*3;
		for (p2 = 0; p2 < 256; p2++)
		{
			dif = 0x7fffffff;
			m=0;
			
			i = p2*3;
			r = (palette[j+0] * p1multi + palette[i+0] * p2multi) / pixdivide;
			g = (palette[j+1] * p1multi + palette[i+1] * p2multi) / pixdivide;
			b = (palette[j+2] * p1multi + palette[i+2] * p2multi) / pixdivide;				
			for (i = 0,pa=palette; i < 256-16; i++,pa+=3)
			{
				curdif = _abs(r - pa[0]) + _abs(g - pa[1]) + _abs(b - pa[2]);
				if (curdif <= 0)	//force 0
				{						
					m = i;
					break;
				}
				if (curdif < dif)
				{
					dif = curdif;
					m = i;
				}
			}
			(t_lookup[table])[p][p2] = m;
		}
	}	

	if (r_transtablewrite.value)
	{
		COM_CreatePath(va("%s/data/",  com_gamedir));
#if 1
		f = fopen (va("%s/data/ttable%i.dat",  com_gamedir, (int) level), "wb");
		if (f)
		{
			if (fwrite (t_lookup[table], 256, 256, f) != 256)
			{
					Con_Printf("Couldn't write data to \"data/ttable%i.dat\"\n", (int) level);
					fclose(f);
					if (rvr)
						R_ReverseTable(table); // make sure it gets reversed if needed
					return;
			}
			fclose(f);		
		}
		else
			Con_Printf("Couldn't write data to \"data/ttable%i.dat\"\n", (int) level);
#else		
		COM_WriteFile(va("data/ttable%i.dat", (int)level, t_lookup[table], 256*256);
#endif	
	}

	if (rvr) // just reverse it here instead of having to do reversed writes
		R_ReverseTable(table);
}

void D_InitTrans(void)
{
	int i;
	int table;

	if (t_lookup)
		BZ_Free(t_lookup);
//no trans palette yet..
	Con_SafePrintf("Making/loading transparency lookup tables\nPlease wait...\n");

	MakeVideoPalette();

	t_numtables = 5;

	i = r_transtables.value;
	if (i > 0 && i < 50) // might need a max bound sanity check here
	{
		t_numtables = i;
	}

	if ((i = COM_CheckParm("-ttables")) != 0)
	{
		t_numtables = Q_atoi(com_argv[i+1]);
		if (t_numtables < 1)
			t_numtables = 1;
		if (t_numtables > 50)
			t_numtables = 50;
	}

t_numtablesinv = ((float)65536/t_numtables)+1;//65546/numtables

t_state = TT_ZERO;
t_curtable=0;
//t_lookup = Hunk_AllocName(sizeof(tlookup)*t_numtables, "Transtables");
t_lookup = BZ_Malloc(sizeof(tlookup)*t_numtables);
t_curlookupp = t_lookup[t_curtable];

	if (r_transtablehalf.value)
	{
		t_state = TT_ZERO|TT_USEHALF;
		for (table = 0; table < t_numtables; table++)
			R_CalcTransTable(table, (int)floor(((table+1)/(float)(t_numtables*2+1))*100 + 0.5));
	}
	else
	{
		if (t_numtables == 1)
			R_CalcTransTable(0, 50);
		else if (t_numtables == 2)
		{
			R_CalcTransTable(0, 33);
			R_CalcTransTable(1, 67);
		}
		else
		{
			for (table = 0; table < t_numtables; table++)	
				R_CalcTransTable(table, (int)floor(100/((float)(t_numtables-1)/table) + 0.5));	
		}
	}
	Con_Printf("Done\n");
}

#ifndef Trans
byte _fastcall Trans(byte p, byte p2)
{	
	return t_curlookupp[p][p2];

}
#endif

/*
void Set_TransLevelI(int level)
{
	t_curtable = level/(100.0f/(t_numtables-1));
	t_curlookupp = t_lookup[t_curtable];
}
*/

void Set_TransLevelF(float level)	//MUST be between 0 and 1
{
	if (level>1)
		level = 1;

	if (t_state & TT_USEHALF)
	{
		t_state = TT_ZERO;
		t_curtable = floor(level*(t_numtables*2+1) + 0.5);
		if (t_curtable > t_numtables)
		{
			t_curtable = (t_numtables*2+1)-t_curtable;
			t_state = TT_REVERSE|TT_ONE;
		}

		if (t_curtable > 0)
		{
			t_state &= ~(TT_ZERO|TT_ONE);
			t_curlookupp = t_lookup[t_curtable-1];
		}
		

		t_state |= TT_USEHALF;
	}
	else if (t_numtables == 1)
	{
		if (level < 0.33)
			t_state = TT_ZERO;
		else if (level > 0.67)
			t_state = TT_ONE;
		else
			t_state = 0;
	}
	else if (t_numtables == 2)
	{
		if (level > 0.75)
			t_state = TT_ONE;
		else if (level > 0.50)
		{
			t_state = 0;
			t_curtable = 1;
			t_curlookupp = t_lookup[t_curtable];
		}
		else if (level > 0.25)
		{
			t_state = 0;
			t_curtable = 0;
			t_curlookupp = t_lookup[t_curtable];
		}
		else
			t_state = TT_ZERO;
	}
	else
	{
		t_curtable = level*t_numtables;
		if (t_curtable >= t_numtables)
			t_state = TT_ONE;
		else if (t_curtable <= 0)
			t_state = TT_ZERO;
		else
		{
			t_state = 0;
			t_curlookupp = t_lookup[t_curtable];
		}
	}
}

qbyte *palxxxto8;
int palmask[3];
int palshift[3];



#define FindPallete(r,g,b) palxxxto8[((r&palmask[0])>>palshift[0]) | ((g&palmask[1])<<palshift[1]) | ((b&palmask[2])<<palshift[2])]
//#define FindPallete(r,g,b) (pal777to8[r>>1][g>>1][b>>1])
qbyte GetPalette(int red, int green, int blue)
{
	if (palxxxto8)	//fast precalculated method
		return FindPallete(red,green,blue);
	else	//slow, horrible method.
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

void MakeVideoPalette(void)
{
//	pal77 *temp;
	qbyte *temp;
	int r, g, b;
	int rs, gs, bs, size;
	int rstep, gstep, bstep;
	int gshift, bshift;
	FILE *f;
	char filename[11];

	if (strlen(r_palconvbits.string) < 3)
	{
		// r5g6b5 is default
		rs = 5;
		gs = 6;
		bs = 5;
	}
	else
	{
		// convert to int
		rs = r_palconvbits.string[0] - '0';
		gs = r_palconvbits.string[1] - '0';
		bs = r_palconvbits.string[2] - '0';

		// limit to 4-8 (can't have 3 because the forumla breaks)
		if (rs < 4)
			rs = 4;
		else if (rs > 8)
			rs = 8;

		if (gs < 4)
			gs = 4;
		else if (gs > 8)
			gs = 8;

		if (bs < 4)
			bs = 4;
		else if (bs > 8)
			bs = 8;
	}

	Q_strcpy(filename, "rgb000.pal");
	filename[3] = rs + '0';
	filename[4] = gs + '0';
	filename[5] = bs + '0';

	palshift[0] = 1<<rs;
	palshift[1] = 1<<gs;
	palshift[2] = 1<<bs;

	size = palshift[0]*palshift[1]*palshift[2];

	gshift = rs;
	bshift = rs+gs;
	rs = 8-rs;
	gs = 8-gs;
	bs = 8-bs;

	rstep = 1<<rs;
	gstep = 1<<gs;
	bstep = 1<<bs;

	palmask[0] = 0xff ^ (rstep - 1);
	palmask[1] = 0xff ^ (gstep - 1);
	palmask[2] = 0xff ^ (bstep - 1);

	palxxxto8 = Hunk_AllocName(size, "RGB data");
	if (!palxxxto8)
		BZ_Free(palxxxto8);
	palxxxto8 = NULL;

	temp = BZ_Malloc(size);
	COM_FOpenFile (filename, &f);
	if (f)
	{
		fread(temp, 1, size, f);	//cached
		fclose(f);

		palxxxto8 = temp;

		// update shifts
		palshift[0] = rs;
		palshift[1] = (8 - palshift[0]) - gs;
		palshift[2] = palshift[1] + (8 - bs);
		return;
	}

	rstep >>= 1;
	gstep >>= 1;
	bstep >>= 1;

	for (r = palshift[0] - 1; r >= 0; r--)
	for (g = palshift[1] - 1; g >= 0; g--)
	for (b = palshift[2] - 1; b >= 0; b--)
	{
		temp[r+(g<<gshift)+(b<<bshift)] = GetPalette((r<<rs)+rstep, (g<<gs)+gstep, (b<<bs)+bstep);
	}
	palxxxto8 = temp;

	// update shifts
	palshift[0] = rs;
	palshift[1] = (8 - palshift[0]) - gs;
	palshift[2] = palshift[1] + (8 - bs);

	if (r_palconvwrite.value)
		COM_WriteFile(filename, palxxxto8, size);
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
			src = framedata + v*inwidth*4;
			{
				f = 0;
				fstep = ((inwidth)*0x10000)/vid.conwidth;
				for (x=0 ; x<vid.conwidth ; x+=4)
				{
					dest[x] = FindPallete(src[(f>>16)*4], src[(f>>16)*4+1], src[(f>>16)*4+2]);
					f += fstep;
					dest[x+1] = FindPallete(src[(f>>16)*4], src[(f>>16)*4+1], src[(f>>16)*4+2]);
					f += fstep;
					dest[x+2] = FindPallete(src[(f>>16)*4], src[(f>>16)*4+1], src[(f>>16)*4+2]);
					f += fstep;
					dest[x+3] = FindPallete(src[(f>>16)*4], src[(f>>16)*4+1], src[(f>>16)*4+2]);
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
							dest[x] = FindPallete(src[(f>>16)*4], src[(f>>16)*4+1], src[(f>>16)*4+2]);
							f += fstep;
							dest[x+1] = FindPallete(src[(f>>16)*4], src[(f>>16)*4+1], src[(f>>16)*4+2]);
							f += fstep;
							dest[x+2] = FindPallete(src[(f>>16)*4], src[(f>>16)*4+1], src[(f>>16)*4+2]);
							f += fstep;
							dest[x+3] = FindPallete(src[(f>>16)*4], src[(f>>16)*4+1], src[(f>>16)*4+2]);
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
							dest[x] = FindPallete(src[(f>>16)*3+2], src[(f>>16)*3+1], src[(f>>16)*3]);
							f += fstep;
							dest[x+1] = FindPallete(src[(f>>16)*3+2], src[(f>>16)*3+1], src[(f>>16)*3]);
							f += fstep;
							dest[x+2] = FindPallete(src[(f>>16)*3+2], src[(f>>16)*3+1], src[(f>>16)*3]);
							f += fstep;
							dest[x+3] = FindPallete(src[(f>>16)*3+2], src[(f>>16)*3+1], src[(f>>16)*3]);
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
