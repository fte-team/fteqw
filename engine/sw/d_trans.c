#include "quakedef.h"
#include "d_local.h"
#include "r_local.h"

#ifdef PEXT_TRANS
int t_numtables;

qbyte p1multitable[] = {1, 99, 49, 97, 48, 19, 47, 93, 23, 91, 9, 89, 22, 87, 43, 17, 21, 83, 41, 81, 4, 79, 39, 77, 19, 3, 37, 73, 18, 71, 7, 69, 17, 67, 33, 13, 16, 63, 31, 61, 3, 59, 29, 57, 14, 11, 27, 53, 13, 51, 1, 49, 12, 47, 23,  9, 11, 43, 21, 41, 2, 39, 19, 37,  9,  7, 17, 33,  8, 31, 3, 29,  7, 27, 13, 1,  6, 23, 11, 21, 1, 19,  9, 17,  4,  3,  7, 13,  3, 11, 1,  9,  2,  7,  3,  1,  1,  3,  1,  1, 0};
qbyte p2multitable[] = {0,  1,  1,  3,  2,  1,  3,  7,  2,  9, 1, 11,  3, 13,  7,  3,  4, 17,  9, 19, 1, 21, 11, 23,  6, 1, 13, 27,  7, 29, 3, 31,  8, 33, 17,  7,  9, 37, 19, 39, 2, 41, 21, 43, 11,  9, 23, 47, 12, 49, 1, 51, 13, 53, 27, 11, 14, 57, 29, 59, 3, 61, 31, 63, 16, 13, 33, 67, 17, 69, 7, 71, 18, 73, 37, 3, 19, 77, 39, 79, 4, 81, 41, 83, 21, 17, 43, 87, 22, 89, 9, 91, 23, 93, 47, 19, 24, 97, 49, 99, 1};
tlookup *t_lookup;
tlookupp *t_curlookupp;
int t_curtable;

int t_numtables;
int t_numtablesinv;//numtables/65546

#define palette host_basepal
#define _abs(x) ((x)*(x))


void R_CalcTransTable(int table, int level)
{
	FILE * f;		
	int p;
	int p2;
	int r, g, b,  j;
	int i;
	unsigned char *pa;
	int m;
	int dif, curdif;

	qbyte p1multi;
	qbyte p2multi;
	qbyte pixdivide;

	p1multi = p1multitable[level];
	p2multi = p2multitable[level];

	pixdivide = p1multi + p2multi;

	COM_FOpenFile (va("data/ttable%i.dat", (int) level) , &f);  //we can ignore the filesize return value
	if (f)
	{
		if (fread (t_lookup[table], 256, 256, f) == 256)
		{
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

	COM_CreatePath(va("%s/data/",  com_gamedir));
#if 1
	f = fopen (va("%s/data/ttable%i.dat",  com_gamedir, (int) level), "wb");
	if (f)
	{
		if (fwrite (t_lookup[table], 256, 256, f) != 256)
		{
				Con_Printf("Couldn't write data to \"data/ttable%i.dat\"\n", (int) level);
				fclose(f);
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

void D_InitTrans(void)
{
	int i;
	int table;

	if (t_lookup)
		BZ_Free(t_lookup);
//no trans palette yet..
	Con_SafePrintf("Making/loading transparency lookup tables\nPlease wait...\n");

MakeVideoPalette();

	if ((i = COM_CheckParm("-ttables")) != 0)
	{
		t_numtables = Q_atoi(com_argv[i+1]);
		if (t_numtables < 1)
			t_numtables = 1;
	}
	else
		t_numtables = 5;

t_numtablesinv = ((float)65536/t_numtables)+1;//65546/numtables

t_curtable=0;
//t_lookup = Hunk_AllocName(sizeof(tlookup)*t_numtables, "Transtables");
t_lookup = BZ_Malloc(sizeof(tlookup)*t_numtables);
t_curlookupp = t_lookup[t_curtable];

	if (t_numtables == 1)
		R_CalcTransTable(0, 50);
	else if (t_numtables == 2)
	{
		R_CalcTransTable(0, 50);
		R_CalcTransTable(1, 100);
	}
	else
	{
		for (table = 0; table < t_numtables; table++)	
			R_CalcTransTable(table, 100/((float)(t_numtables-1)/table));	
	}
	Con_Printf("Done\n");
}

#ifndef Trans
byte _fastcall Trans(byte p, byte p2)
{	
	return t_curlookupp[p][p2];

}
#endif

void Set_TransLevelI(int level)
{
	/*
	0		=0
	12.5	=1
	25		=2
	37.5	=3
	50		=4
	62.5	=5
	75		=6
	87.5	=7
	100		=8
	*/
	t_curtable = level/(100.0f/(t_numtables-1));
	t_curlookupp = t_lookup[t_curtable];
}
void Set_TransLevelF(float level)	//MUST be between 0 and 1
{
//	level+=1.0/(NUMTABLES-1);
	if (level>1)
		level = 1;
	t_curtable = level*(t_numtables-1);
	t_curlookupp = t_lookup[t_curtable];
}

#endif
