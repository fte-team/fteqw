/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// r_misc.c

#include "quakedef.h"
#ifdef RGLQUAKE
#include "glquake.h"
#include "gl_draw.h"

void R_ReloadRTLights_f(void);


#ifdef WATERLAYERS
cvar_t	r_waterlayers = SCVAR("r_waterlayers","3");
#endif

extern void R_InitBubble();

#ifndef SWQUAKE
//SW rendering has a faster method, which takes more memory and stuff.
//We need this for minor things though, so we'll just use the slow accurate method.
//this is unlikly to be called very often.			
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
#endif

/*
==================
R_InitTextures
==================
*
void	GLR_InitTextures (void)
{
	int		x,y, m;
	qbyte	*dest;

// create a simple checkerboard texture for the default
	r_notexture_mip = Hunk_AllocName (sizeof(texture_t) + 16*16+8*8+4*4+2*2, "notexture");
	
	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof(texture_t);
	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16*16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8*8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4*4;
	
	for (m=0 ; m<4 ; m++)
	{
		dest = (qbyte *)r_notexture_mip + r_notexture_mip->offsets[m];
		for (y=0 ; y< (16>>m) ; y++)
			for (x=0 ; x< (16>>m) ; x++)
			{
				if (  (y< (8>>m) ) ^ (x< (8>>m) ) )
					*dest++ = 0;
				else
					*dest++ = 0xff;
			}
	}	
}*/
//we could go for nice smooth round particles... but then we would loose a little bit of the chaotic nature of the particles.
static qbyte	dottexture[8][8] =
{
	{0,0,0,0,0,0,0,0},
	{0,0,0,1,1,0,0,0},
	{0,0,1,1,1,1,0,0},
	{0,1,1,1,1,1,1,0},
	{0,1,1,1,1,1,1,0},
	{0,0,1,1,1,1,0,0},
	{0,0,0,1,1,0,0,0},
	{0,0,0,0,0,0,0,0},
};
static qbyte	exptexture[16][16] =
{
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	{0,0,0,0,1,0,0,0,1,0,0,1,0,0,0,0},
	{0,0,0,1,1,1,1,1,3,1,1,2,1,0,0,0},
	{0,0,0,1,1,1,1,4,4,4,5,4,2,1,1,0},
	{0,0,1,1,6,5,5,8,6,8,3,6,3,2,1,0},
	{0,0,1,5,6,7,5,6,8,8,8,3,3,1,0,0},
	{0,0,0,1,6,8,9,9,9,9,4,6,3,1,0,0},
	{0,0,2,1,7,7,9,9,9,9,5,3,1,0,0,0},
	{0,0,2,4,6,8,9,9,9,9,8,6,1,0,0,0},
	{0,0,2,2,3,5,6,8,9,8,8,4,4,1,0,0},
	{0,0,1,2,4,1,8,7,8,8,6,5,4,1,0,0},
	{0,1,1,1,7,8,1,6,7,5,4,7,1,0,0,0},
	{0,1,2,1,1,5,1,3,4,3,1,1,0,0,0,0},
	{0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
};

void R_InitParticleTexture (void)
{
#define PARTICLETEXTURESIZE 64
	int		x,y;
	float dx, dy, d;
	qbyte	data[PARTICLETEXTURESIZE*PARTICLETEXTURESIZE][4];

	//
	// particle texture
	//
	particletexture = GL_AllocNewTexture();
    GL_Bind(particletexture);

	for (x=0 ; x<8 ; x++)
	{
		for (y=0 ; y<8 ; y++)
		{
			data[y*8+x][0] = 255;
			data[y*8+x][1] = 255;
			data[y*8+x][2] = 255;
			data[y*8+x][3] = dottexture[x][y]*255;
		}
	}
	qglTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


	//
	// particle triangle texture
	//
	particlecqtexture = GL_AllocNewTexture();
    GL_Bind(particlecqtexture);

	// clear to transparent white
	for (x = 0; x < 32 * 32; x++)
	{
			data[x][0] = 255;
			data[x][1] = 255;
			data[x][2] = 255;
			data[x][3] = 0;
	}
	//draw a circle in the top left.
	for (x=0 ; x<16 ; x++)
	{
		for (y=0 ; y<16 ; y++)
		{
			if ((x - 7.5) * (x - 7.5) + (y - 7.5) * (y - 7.5) <= 8 * 8)
				data[y*32+x][3] = 255;
		}
	}
	qglTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);





	explosiontexture = GL_AllocNewTexture();
    GL_Bind(explosiontexture);

	for (x=0 ; x<16 ; x++)
	{
		for (y=0 ; y<16 ; y++)
		{
			data[y*16+x][0] = 255;
			data[y*16+x][1] = 255;
			data[y*16+x][2] = 255;
			data[y*16+x][3] = exptexture[x][y]*255/9.0;
		}
	}
	qglTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


	memset(data, 255, sizeof(data));
	for (y = 0;y < PARTICLETEXTURESIZE;y++)
	{
		dy = (y - 0.5f*PARTICLETEXTURESIZE) / (PARTICLETEXTURESIZE*0.5f-1);
		for (x = 0;x < PARTICLETEXTURESIZE;x++)
		{
			dx = (x - 0.5f*PARTICLETEXTURESIZE) / (PARTICLETEXTURESIZE*0.5f-1);
			d = 256 * (1 - (dx*dx+dy*dy));
			d = bound(0, d, 255);
			data[y*PARTICLETEXTURESIZE+x][3] = (qbyte) d;
		}
	}
	balltexture = GL_AllocNewTexture();
    GL_Bind(balltexture);
	qglTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, PARTICLETEXTURESIZE, PARTICLETEXTURESIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

/*
===============
R_Envmap_f

Grab six views for environment mapping tests
===============
*/
void R_Envmap_f (void)
{
	qbyte	buffer[256*256*4];

	qglDrawBuffer  (GL_FRONT);
	qglReadBuffer  (GL_FRONT);
	envmap = true;

	r_refdef.vrect.x = 0;
	r_refdef.vrect.y = 0;
	r_refdef.vrect.width = 256;
	r_refdef.vrect.height = 256;

	r_refdef.viewangles[0] = 0;
	r_refdef.viewangles[1] = 0;
	r_refdef.viewangles[2] = 0;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	qglReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env0.rgb", buffer, sizeof(buffer));		

	r_refdef.viewangles[1] = 90;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	qglReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env1.rgb", buffer, sizeof(buffer));		

	r_refdef.viewangles[1] = 180;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	qglReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env2.rgb", buffer, sizeof(buffer));		

	r_refdef.viewangles[1] = 270;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	qglReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env3.rgb", buffer, sizeof(buffer));		

	r_refdef.viewangles[0] = -90;
	r_refdef.viewangles[1] = 0;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	qglReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env4.rgb", buffer, sizeof(buffer));		

	r_refdef.viewangles[0] = 90;
	r_refdef.viewangles[1] = 0;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	qglReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env5.rgb", buffer, sizeof(buffer));		

	envmap = false;
	qglDrawBuffer  (GL_BACK);
	qglReadBuffer  (GL_BACK);
	GL_EndRendering ();
	GL_DoSwap();
}












qboolean GenerateNormalisationCubeMap()
{
	unsigned char data[32*32*3];

	//some useful variables
	int size=32;
	float offset=0.5f;
	float halfSize=16.0f;
	vec3_t tempVector;
	unsigned char * bytePtr;

	int i, j;

	//positive x
	bytePtr=data;

	for(j=0; j<size; j++)
	{
		for(i=0; i<size; i++)
		{
			tempVector[0] = halfSize;			
			tempVector[1] = -(j+offset-halfSize);
			tempVector[2] = -(i+offset-halfSize);

			VectorNormalize(tempVector);

			bytePtr[0]=(unsigned char)((tempVector[0]/2 + 0.5)*255);
			bytePtr[1]=(unsigned char)((tempVector[1]/2 + 0.5)*255);
			bytePtr[2]=(unsigned char)((tempVector[2]/2 + 0.5)*255);

			bytePtr+=3;
		}
	}
	qglTexImage2D(	GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB,
					0, GL_RGBA8, 32, 32, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

	//negative x
	bytePtr=data;

	for(j=0; j<size; j++)
	{
		for(i=0; i<size; i++)
		{
			tempVector[0] = (-halfSize);
			tempVector[1] = (-(j+offset-halfSize));
			tempVector[2] = ((i+offset-halfSize));

			VectorNormalize(tempVector);

			bytePtr[0]=(unsigned char)((tempVector[0]/2 + 0.5)*255);
			bytePtr[1]=(unsigned char)((tempVector[1]/2 + 0.5)*255);
			bytePtr[2]=(unsigned char)((tempVector[2]/2 + 0.5)*255);

			bytePtr+=3;
		}
	}
	qglTexImage2D(	GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB,
					0, GL_RGBA8, 32, 32, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

	//positive y
	bytePtr=data;

	for(j=0; j<size; j++)
	{
		for(i=0; i<size; i++)
		{
			tempVector[0] = (i+offset-halfSize);
			tempVector[1] = (halfSize);
			tempVector[2] = ((j+offset-halfSize));

			VectorNormalize(tempVector);

			bytePtr[0]=(unsigned char)((tempVector[0]/2 + 0.5)*255);
			bytePtr[1]=(unsigned char)((tempVector[1]/2 + 0.5)*255);
			bytePtr[2]=(unsigned char)((tempVector[2]/2 + 0.5)*255);

			bytePtr+=3;
		}
	}
	qglTexImage2D(	GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB,
					0, GL_RGBA8, 32, 32, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

	//negative y
	bytePtr=data;

	for(j=0; j<size; j++)
	{
		for(i=0; i<size; i++)
		{
			tempVector[0] = (i+offset-halfSize);
			tempVector[1] = (-halfSize);
			tempVector[2] = (-(j+offset-halfSize));

			VectorNormalize(tempVector);

			bytePtr[0]=(unsigned char)((tempVector[0]/2 + 0.5)*255);
			bytePtr[1]=(unsigned char)((tempVector[1]/2 + 0.5)*255);
			bytePtr[2]=(unsigned char)((tempVector[2]/2 + 0.5)*255);

			bytePtr+=3;
		}
	}
	qglTexImage2D(	GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB,
					0, GL_RGBA8, 32, 32, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

	//positive z
	bytePtr=data;

	for(j=0; j<size; j++)
	{
		for(i=0; i<size; i++)
		{
			tempVector[0] = (i+offset-halfSize);
			tempVector[1] = (-(j+offset-halfSize));
			tempVector[2] = (halfSize);

			VectorNormalize(tempVector);

			bytePtr[0]=(unsigned char)((tempVector[0]/2 + 0.5)*255);
			bytePtr[1]=(unsigned char)((tempVector[1]/2 + 0.5)*255);
			bytePtr[2]=(unsigned char)((tempVector[2]/2 + 0.5)*255);

			bytePtr+=3;
		}
	}
	qglTexImage2D(	GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB,
					0, GL_RGBA8, 32, 32, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

	//negative z
	bytePtr=data;

	for(j=0; j<size; j++)
	{
		for(i=0; i<size; i++)
		{
			tempVector[0] = (-(i+offset-halfSize));
			tempVector[1] = (-(j+offset-halfSize));
			tempVector[2] = (-halfSize);

			VectorNormalize(tempVector);

			bytePtr[0]=(unsigned char)((tempVector[0]/2 + 0.5)*255);
			bytePtr[1]=(unsigned char)((tempVector[1]/2 + 0.5)*255);
			bytePtr[2]=(unsigned char)((tempVector[2]/2 + 0.5)*255);

			bytePtr+=3;
		}
	}
	qglTexImage2D(	GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB,
					0, GL_RGBA8, 32, 32, 0, GL_RGB, GL_UNSIGNED_BYTE, data);	

	return true;
}


int normalisationCubeMap;
/*
===============
R_Init
===============
*/
void GLR_ReInit (void)
{		
	extern int gl_bumpmappingpossible;
	R_InitParticleTexture ();

#ifdef GLTEST
	Test_Init ();
#endif

	netgraphtexture = GL_AllocNewTexture();

	if (gl_bumpmappingpossible)
	{
		//Create normalisation cube map
		normalisationCubeMap = GL_AllocNewTexture();
		GL_BindType(GL_TEXTURE_CUBE_MAP_ARB, normalisationCubeMap);
		GenerateNormalisationCubeMap();
		qglTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		qglTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		qglTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		qglTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	}
	else
		normalisationCubeMap = 0;

	R_InitBloomTextures();

}
/*
typedef struct
{
   long offset;                 	// Position of the entry in WAD
   long dsize;                  	// Size of the entry in WAD file
   long size;                   	// Size of the entry in memory
   char type;                   	// type of entry
   char cmprs;                  	// Compression. 0 if none.
   short dummy;                 	// Not used
   char name[16];               	// we use only first 8
} wad2entry_t;
typedef struct
{
   char magic[4]; 			//should be WAD2
   long num;				//number of entries
   long offset;				//location of directory
} wad2_t;
void R_MakeTexWad_f(void)
{
	miptex_t dummymip = {"", 0, 0, {0, 0, 0, 0}};
	wad2_t wad2 = {"WAD2",0,0};
	wad2entry_t entry[2048];
	int entries = 0, i;
	FILE *f;
	char base[128];
	char *texname;
//	qbyte b;
	float scale;
	int width, height;

	qbyte *buf, *outmip;
	qbyte *mip, *stack;

//	WIN32_FIND_DATA fd;
//	HANDLE h;

	scale = atof(Cmd_Argv(2));
	if (!scale)
		scale = 2;

//	h = FindFirstFile(va("%s/textures/ *.tga", com_gamedir), &fd);	//if this is uncommented, clear that space... (gcc warning fix)
	if (!shader)
		return;
	mip = BZ_Malloc(1024*1024);
//	initbuf = BZ_Malloc(1024*1024*4);
	stack = BZ_Malloc(1024*1024*4+1024);
	f=fopen(va("%s/shadrtex.wad", com_gamedir), "wb");
	fwrite(&wad2, 1, sizeof(wad2_t), f);

	for (shad = shader; shad; shad=shad->next)
	{
		texname = shad->editorname;
		if (!*texname)
			continue;
		COM_StripExtension(shad->name, base);
		base[15]=0;
		for (i =0; i < entries; i++)
			if (!strcmp(entry[entries].name, base))
				break;
		if (i != entries)
		{
			Con_Printf("Skipped %s - duplicated shrunken name\n", texname);
			continue;
		}
		entry[entries].offset = ftell(f);
		entry[entries].dsize = entry[entries].size = 0;
		entry[entries].type = TYP_MIPTEX;
		entry[entries].cmprs = 0;
		entry[entries].dummy = 0;
		strcpy(entry[entries].name, base);

		strcpy(dummymip.name, base);

		{
	
			qbyte *data;
			int h;
			float x, xi;
			float y, yi;			

			char *path[] ={
		"%s",
		"override/%s.tga",
		"override/%s.pcx",
		"%s.tga",
		"progs/%s"};
			for (h = 0, buf=NULL; h < sizeof(path)/sizeof(char *); h++)
			{			
				buf = COM_LoadStackFile(va(path[h], texname), stack, 1024*1024*4+1024);
				if (buf)
					break;
			}
			if (!buf)
			{
				Con_Printf("Failed to find texture \"%s\"\n", texname);
				continue;
			}


data = ReadTargaFile(buf, com_filesize, &width, &height, false);
if (!data)
{
	BZ_Free(data);
	Con_Printf("Skipped %s - file type not supported (bad bpp?)\n", texname);
	continue;
}

			dummymip.width = (int)(width/scale) & ~0xf;
			dummymip.height = (int)(height/scale) & ~0xf;
			if (dummymip.width<=0)
				dummymip.width=16;
			if (dummymip.height<=0)
				dummymip.height=16;

			dummymip.offsets[0] = sizeof(dummymip);
			dummymip.offsets[1] = dummymip.offsets[0]+dummymip.width*dummymip.height;
			dummymip.offsets[2] = dummymip.offsets[1]+dummymip.width/2*dummymip.height/2;
			dummymip.offsets[3] = dummymip.offsets[2]+dummymip.width/4*dummymip.height/4;
			entry[entries].dsize = entry[entries].size = dummymip.offsets[3]+dummymip.width/8*dummymip.height/8;

			xi = (float)width/dummymip.width;
			yi = (float)height/dummymip.height;


			fwrite(&dummymip, 1, sizeof(dummymip), f);
			outmip=mip;
			for (outmip=mip, y = 0; y < height; y+=yi)
			for (x = 0; x < width; x+=xi)
			{
				*outmip++ = GetPaletteIndex(	data[(int)(x+y*width)*4+0],
								data[(int)(x+y*width)*4+1],
								data[(int)(x+y*width)*4+2]);
			}
			fwrite(mip, dummymip.width, dummymip.height, f);
			for (outmip=mip, y = 0; y < height; y+=yi*2)
			for (x = 0; x < width; x+=xi*2)
			{
				*outmip++ = GetPaletteIndex(	data[(int)(x+y*width)*4+0],
								data[(int)(x+y*width)*4+1],
								data[(int)(x+y*width)*4+2]);				
			}
			fwrite(mip, dummymip.width/2, dummymip.height/2, f);
			for (outmip=mip, y = 0; y < height; y+=yi*4)
			for (x = 0; x < width; x+=xi*4)
			{
				*outmip++ = GetPaletteIndex(	data[(int)(x+y*width)*4+0],
								data[(int)(x+y*width)*4+1],
								data[(int)(x+y*width)*4+2]);				
			}
			fwrite(mip, dummymip.width/4, dummymip.height/4, f);
			for (outmip=mip, y = 0; y < height; y+=yi*8)
			for (x = 0; x < width; x+=xi*8)
			{
				*outmip++ = GetPaletteIndex(	data[(int)(x+y*width)*4+0],
								data[(int)(x+y*width)*4+1],
								data[(int)(x+y*width)*4+2]);
			}
			fwrite(mip, dummymip.width/8, dummymip.height/8, f);

			BZ_Free(data);
		}
		entries++;
		Con_Printf("Added %s\n", base);
		GLSCR_UpdateScreen();
	}

	wad2.offset = ftell(f);
	wad2.num = entries;
	fwrite(entry, entries, sizeof(wad2entry_t), f);
	fseek(f, 0, SEEK_SET);
	fwrite(&wad2, 1, sizeof(wad2_t), f);
	fclose(f);


	BZ_Free(mip);
//	BZ_Free(initbuf);
	BZ_Free(stack);

	Con_Printf("Written %i mips to textures.wad\n", entries);
}
*/
void GLR_TimeRefresh_f (void);

extern cvar_t gl_bump, v_contrast, r_drawflat;
extern cvar_t r_stains, r_stainfadetime, r_stainfadeammount;

// callback defines
extern cvar_t gl_conback, gl_font, gl_smoothfont, gl_fontinwardstep, r_menutint;
extern cvar_t vid_conautoscale, vid_conheight, vid_conwidth;
extern cvar_t crosshair, crosshairimage, crosshaircolor, r_skyboxname;
extern cvar_t r_floorcolour, r_wallcolour, r_floortexture, r_walltexture;
extern cvar_t r_fastskycolour;
void GLCrosshairimage_Callback(struct cvar_s *var, char *oldvalue);
void GLCrosshair_Callback(struct cvar_s *var, char *oldvalue);
void GLCrosshaircolor_Callback(struct cvar_s *var, char *oldvalue);
void GLR_Skyboxname_Callback(struct cvar_s *var, char *oldvalue);
void GLR_Menutint_Callback (struct cvar_s *var, char *oldvalue);
void GL_Conback_Callback (struct cvar_s *var, char *oldvalue);
void GL_Font_Callback (struct cvar_s *var, char *oldvalue);
void GL_Smoothfont_Callback (struct cvar_s *var, char *oldvalue);
void GL_Fontinwardstep_Callback (struct cvar_s *var, char *oldvalue);
void GLVID_Conwidth_Callback(struct cvar_s *var, char *oldvalue);
void GLVID_Conautoscale_Callback(struct cvar_s *var, char *oldvalue);
void GLVID_Conheight_Callback(struct cvar_s *var, char *oldvalue);
void GLR_Wallcolour_Callback(struct cvar_s *var, char *oldvalue);
void GLR_Floorcolour_Callback(struct cvar_s *var, char *oldvalue);
void GLR_Walltexture_Callback(struct cvar_s *var, char *oldvalue);
void GLR_Floortexture_Callback(struct cvar_s *var, char *oldvalue);
void GLR_Drawflat_Callback(struct cvar_s *var, char *oldvalue);
void GLV_Gamma_Callback(struct cvar_s *var, char *oldvalue);
void GLR_Fastskycolour_Callback(struct cvar_s *var, char *oldvalue);

void GLR_DeInit (void)
{
	Cmd_RemoveCommand ("timerefresh");
	Cmd_RemoveCommand ("envmap");
	Cmd_RemoveCommand ("r_editlights_reload");
	Cmd_RemoveCommand ("pointfile");

	Cmd_RemoveCommand ("makewad");

	Cvar_Unhook(&crosshair);
	Cvar_Unhook(&crosshairimage);
	Cvar_Unhook(&crosshaircolor);
	Cvar_Unhook(&r_skyboxname);
	Cvar_Unhook(&r_menutint);
	Cvar_Unhook(&gl_conback);
	Cvar_Unhook(&gl_font);
	Cvar_Unhook(&gl_smoothfont);
	Cvar_Unhook(&gl_fontinwardstep);
	Cvar_Unhook(&vid_conautoscale);
	Cvar_Unhook(&vid_conheight);
	Cvar_Unhook(&vid_conwidth);
	Cvar_Unhook(&r_wallcolour);
	Cvar_Unhook(&r_floorcolour);
	Cvar_Unhook(&r_walltexture);
	Cvar_Unhook(&r_floortexture);
	Cvar_Unhook(&r_fastskycolour);
	Cvar_Unhook(&r_drawflat);
	Cvar_Unhook(&v_gamma);
	Cvar_Unhook(&v_contrast);

	GLDraw_DeInit();

	GLSurf_DeInit();
}

void GLR_Init (void)
{	
	Cmd_AddRemCommand ("timerefresh", GLR_TimeRefresh_f);
	Cmd_AddRemCommand ("envmap", R_Envmap_f);
	Cmd_AddRemCommand ("r_editlights_reload", R_ReloadRTLights_f);

//	Cmd_AddRemCommand ("makewad", R_MakeTexWad_f);

	Cvar_Hook(&crosshair, GLCrosshair_Callback);
	Cvar_Hook(&crosshairimage, GLCrosshairimage_Callback);
	Cvar_Hook(&crosshaircolor, GLCrosshaircolor_Callback);
	Cvar_Hook(&r_skyboxname, GLR_Skyboxname_Callback);
	Cvar_Hook(&r_menutint, GLR_Menutint_Callback);
	Cvar_Hook(&gl_conback, GL_Conback_Callback);
	Cvar_Hook(&gl_font, GL_Font_Callback);
	Cvar_Hook(&gl_smoothfont, GL_Smoothfont_Callback);
	Cvar_Hook(&gl_fontinwardstep, GL_Fontinwardstep_Callback);
	Cvar_Hook(&vid_conautoscale, GLVID_Conautoscale_Callback);
	Cvar_Hook(&vid_conheight, GLVID_Conheight_Callback);
	Cvar_Hook(&vid_conwidth, GLVID_Conwidth_Callback);
	Cvar_Hook(&r_floorcolour, GLR_Floorcolour_Callback);
	Cvar_Hook(&r_fastskycolour, GLR_Fastskycolour_Callback);
	Cvar_Hook(&r_wallcolour, GLR_Wallcolour_Callback);
	Cvar_Hook(&r_floortexture, GLR_Floortexture_Callback);
	Cvar_Hook(&r_walltexture, GLR_Walltexture_Callback);
	Cvar_Hook(&r_drawflat, GLR_Drawflat_Callback);
	Cvar_Hook(&v_gamma, GLV_Gamma_Callback);
	Cvar_Hook(&v_contrast, GLV_Gamma_Callback);

	R_InitBubble();

	GLR_ReInit();
}

void R_ImportRTLights(char *entlump)
{
	typedef enum lighttype_e {LIGHTTYPE_MINUSX, LIGHTTYPE_RECIPX, LIGHTTYPE_RECIPXX, LIGHTTYPE_NONE, LIGHTTYPE_SUN, LIGHTTYPE_MINUSXX} lighttype_t;

	/*I'm using the DP code so I know I'll get the DP results*/
	int entnum, style, islight, skin, pflags, effects, n;
	lighttype_t type;
	float origin[3], angles[3], radius, color[3], light[4], fadescale, lightscale, originhack[3], overridecolor[3], vec[4];
	char key[256], value[8192];

	for (entnum = 0; ;entnum++)
	{
		entlump = COM_Parse(entlump);
		if (com_token[0] != '{')
			break;

		type = LIGHTTYPE_MINUSX;
		origin[0] = origin[1] = origin[2] = 0;
		originhack[0] = originhack[1] = originhack[2] = 0;
		angles[0] = angles[1] = angles[2] = 0;
		color[0] = color[1] = color[2] = 1;
		light[0] = light[1] = light[2] = 1;light[3] = 300;
		overridecolor[0] = overridecolor[1] = overridecolor[2] = 1;
		fadescale = 1;
		lightscale = 1;
		style = 0;
		skin = 0;
		pflags = 0;
		effects = 0;
		islight = false;
		while (1)
		{
			entlump = COM_Parse(entlump);
			if (!entlump)
				break; // error
			if (com_token[0] == '}')
				break; // end of entity
			if (com_token[0] == '_')
				Q_strncpyz(key, com_token + 1, sizeof(key));
			else
				Q_strncpyz(key, com_token, sizeof(key));
			while (key[strlen(key)-1] == ' ') // remove trailing spaces
				key[strlen(key)-1] = 0;
			entlump = COM_Parse(entlump);
			if (!entlump)
				break; // error
			Q_strncpyz(value, com_token, sizeof(value));

			// now that we have the key pair worked out...
			if (!strcmp("light", key))
			{
				n = sscanf(value, "%f %f %f %f", &vec[0], &vec[1], &vec[2], &vec[3]);
				if (n == 1)
				{
					// quake
					light[0] = vec[0] * (1.0f / 256.0f);
					light[1] = vec[0] * (1.0f / 256.0f);
					light[2] = vec[0] * (1.0f / 256.0f);
					light[3] = vec[0];
				}
				else if (n == 4)
				{
					// halflife
					light[0] = vec[0] * (1.0f / 255.0f);
					light[1] = vec[1] * (1.0f / 255.0f);
					light[2] = vec[2] * (1.0f / 255.0f);
					light[3] = vec[3];
				}
			}
			else if (!strcmp("delay", key))
				type = atoi(value);
			else if (!strcmp("origin", key))
				sscanf(value, "%f %f %f", &origin[0], &origin[1], &origin[2]);
			else if (!strcmp("angle", key))
				angles[0] = 0, angles[1] = atof(value), angles[2] = 0;
			else if (!strcmp("angles", key))
				sscanf(value, "%f %f %f", &angles[0], &angles[1], &angles[2]);
			else if (!strcmp("color", key))
				sscanf(value, "%f %f %f", &color[0], &color[1], &color[2]);
			else if (!strcmp("wait", key))
				fadescale = atof(value);
			else if (!strcmp("classname", key))
			{
				if (!strncmp(value, "light", 5))
				{
					islight = true;
					if (!strcmp(value, "light_fluoro"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1;
						overridecolor[1] = 1;
						overridecolor[2] = 1;
					}
					if (!strcmp(value, "light_fluorospark"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1;
						overridecolor[1] = 1;
						overridecolor[2] = 1;
					}
					if (!strcmp(value, "light_globe"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1;
						overridecolor[1] = 0.8;
						overridecolor[2] = 0.4;
					}
					if (!strcmp(value, "light_flame_large_yellow"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1;
						overridecolor[1] = 0.5;
						overridecolor[2] = 0.1;
					}
					if (!strcmp(value, "light_flame_small_yellow"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1;
						overridecolor[1] = 0.5;
						overridecolor[2] = 0.1;
					}
					if (!strcmp(value, "light_torch_small_white"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1;
						overridecolor[1] = 0.5;
						overridecolor[2] = 0.1;
					}
					if (!strcmp(value, "light_torch_small_walltorch"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1;
						overridecolor[1] = 0.5;
						overridecolor[2] = 0.1;
					}
				}
			}
			else if (!strcmp("style", key))
				style = atoi(value);
			else if (!strcmp("skin", key))
				skin = (int)atof(value);
			else if (!strcmp("pflags", key))
				pflags = (int)atof(value);
			else if (!strcmp("effects", key))
				effects = (int)atof(value);

			else if (!strcmp("scale", key))
				lightscale = atof(value);
			else if (!strcmp("fade", key))
				fadescale = atof(value);
		}
		if (!islight)
			continue;
		if (lightscale <= 0)
			lightscale = 1;
		if (fadescale <= 0)
			fadescale = 1;
		if (color[0] == color[1] && color[0] == color[2])
		{
			color[0] *= overridecolor[0];
			color[1] *= overridecolor[1];
			color[2] *= overridecolor[2];
		}
		radius = light[3] * 1/*r_editlights_quakelightsizescale*/ * lightscale / fadescale;
		color[0] = color[0] * light[0];
		color[1] = color[1] * light[1];
		color[2] = color[2] * light[2];
		switch (type)
		{
		case LIGHTTYPE_MINUSX:
			break;
		case LIGHTTYPE_RECIPX:
			radius *= 2;
			VectorScale(color, (1.0f / 16.0f), color);
			break;
		case LIGHTTYPE_RECIPXX:
			radius *= 2;
			VectorScale(color, (1.0f / 16.0f), color);
			break;
		default:
		case LIGHTTYPE_NONE:
			break;
		case LIGHTTYPE_SUN:
			break;
		case LIGHTTYPE_MINUSXX:
			break;
		}
		VectorAdd(origin, originhack, origin);
		if (radius >= 1)
		{
			dlight_t *dl = CL_AllocDlight(0);
			VectorCopy(origin, dl->origin);
			AngleVectors(angles, dl->axis[0], dl->axis[1], dl->axis[2]);
			dl->radius = radius;
			VectorCopy(color, dl->color);
			dl->flags |= LFLAG_REALTIMEMODE;
			dl->flags |= (pflags & PFLAGS_CORONA)?LFLAG_ALLOW_FLASH:0;
			dl->flags |= (pflags & PFLAGS_NOSHADOW)?LFLAG_NOSHADOWS:0;
			dl->style = style+1;

			//FIXME: cubemaps if skin >= 16
		}
	}
}

void R_LoadRTLights(void)
{
	dlight_t *dl;
	char fname[MAX_QPATH];
	char *file;
	char *end;
	int style;

	vec3_t org;
	float radius;
	vec3_t rgb;
	unsigned int flags;

	vec3_t angles;

	//delete all old lights
	dlights_running = 0;
	dlights_software = 0;

	COM_StripExtension(cl.worldmodel->name, fname, sizeof(fname));
	strncat(fname, ".rtlights", MAX_QPATH-1);

	file = COM_LoadTempFile(fname);
	if (file)
	while(1)
	{
		end = strchr(file, '\n');
		if (!end)
			end = file + strlen(file);
		if (end == file)
			break;
		*end = '\0';

		file = COM_Parse(file);
		org[0] = atof(com_token);
		file = COM_Parse(file);
		org[1] = atof(com_token);
		file = COM_Parse(file);
		org[2] = atof(com_token);

		file = COM_Parse(file);
		radius = atof(com_token);

		file = COM_Parse(file);
		rgb[0] = file?atof(com_token):1;
		file = COM_Parse(file);
		rgb[1] = file?atof(com_token):1;
		file = COM_Parse(file);
		rgb[2] = file?atof(com_token):1;

		file = COM_Parse(file);
		style = file?atof(com_token):0;

		file = COM_Parse(file);
		//cubemap

		file = COM_Parse(file);
		//corona

		file = COM_Parse(file);
		angles[0] = atof(com_token);
		file = COM_Parse(file);
		angles[1] = atof(com_token);
		file = COM_Parse(file);
		angles[2] = atof(com_token);

		file = COM_Parse(file);
		//corrona scale

		file = COM_Parse(file);
		//ambient

		file = COM_Parse(file);
		//diffuse

		file = COM_Parse(file);
		//specular

		file = COM_Parse(file);
		if (*com_token)
			flags = atoi(com_token);
		else
			flags = LFLAG_REALTIMEMODE;

		if (radius)
		{
			dl = CL_AllocDlight(0);
			VectorCopy(org, dl->origin);
			dl->radius = radius;
			VectorCopy(rgb, dl->color);
			dl->die = 0;
			dl->flags = flags;
			AngleVectors(angles, dl->axis[0], dl->axis[1], dl->axis[2]);

			dl->style = style+1;
		}
		file = end+1;
	}
}

void R_ReloadRTLights_f(void)
{
	if (!cl.worldmodel)
	{
		Con_Printf("Cannot reload lights at this time\n");
		return;
	}
	dlights_running = 0;
	dlights_software = 0;
	if (strcmp(Cmd_Argv(1), "bsp"))
		R_LoadRTLights();

	if (!dlights_running)
	{
		Con_Printf("Importing rtlights from BSP\n");
		R_ImportRTLights(cl.worldmodel->entities);
	}
}

/*
===============
R_NewMap
===============
*/
void GLR_NewMap (void)
{
	char namebuf[MAX_QPATH];
	extern cvar_t host_mapname;
	int		i;

/*
	if (cl.worldmodel->fromgame == fg_quake3 && cls.netchan.remote_address.type != NA_LOOPBACK)
	{
		if (!cls.allow_cheats)
		{
			CL_Disconnect();
			Host_EndGame("\n\nThe quake3 map implementation is still experimental and contains many bugs that could be considered cheats. Therefore, the engine is handicapped to quake3 maps only when hosting - it's single player only.\n\nYou can allow it on the server by activating cheats, at which point this check will be ignored\n");
			return;
		}
//		Cbuf_AddText("disconnect\n", RESTRICT_LOCAL);
	}
*/
	
	for (i=0 ; i<256 ; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	memset (&r_worldentity, 0, sizeof(r_worldentity));
	AngleVectors(r_worldentity.angles, r_worldentity.axis[0], r_worldentity.axis[1], r_worldentity.axis[2]);
	VectorInverse(r_worldentity.axis[1]);
	r_worldentity.model = cl.worldmodel;


	COM_StripExtension(COM_SkipPath(cl.worldmodel->name), namebuf, sizeof(namebuf));
	Cvar_Set(&host_mapname, namebuf);

// clear out efrags in case the level hasn't been reloaded
// FIXME: is this one short?
	for (i=0 ; i<cl.worldmodel->numleafs ; i++)
		cl.worldmodel->leafs[i].efrags = NULL;

	GLSurf_DeInit();

	r_viewleaf = NULL;
	r_viewcluster = -1;
	r_oldviewcluster = 0;
	r_viewcluster2 = -1;
TRACE(("dbg: GLR_NewMap: clear particles\n"));
	P_ClearParticles ();
TRACE(("dbg: GLR_NewMap: wiping them stains (getting the cloth out)\n"));
	GLR_WipeStains();
TRACE(("dbg: GLR_NewMap: building lightmaps\n"));
	GL_BuildLightmaps ();
TRACE(("dbg: GLR_NewMap: figuring out skys and mirrors\n"));
	// identify sky texture
	if (cl.worldmodel->fromgame != fg_quake2 && cl.worldmodel->fromgame != fg_quake3)
	{
		skytexturenum = -1;
		mirrortexturenum = -1;
	}
	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		if (!cl.worldmodel->textures[i])
			continue;
		if (!Q_strncmp(cl.worldmodel->textures[i]->name,"sky",3) )
			skytexturenum = i;
		if (!Q_strncmp(cl.worldmodel->textures[i]->name,"window02_1",10) )
			mirrortexturenum = i;
 		cl.worldmodel->textures[i]->texturechain = NULL;
	}
TRACE(("dbg: GLR_NewMap: that skybox thang\n"));
//#ifdef QUAKE2
	GLR_LoadSkys ();
//#endif
TRACE(("dbg: GLR_NewMap: ui\n"));
#ifdef VM_UI
	UI_Reset();
#endif
TRACE(("dbg: GLR_NewMap: tp\n"));
	TP_NewMap();

	if (r_shadows.value)
	{
		R_LoadRTLights();
	}
}

void GLR_PreNewMap(void)
{
	extern int solidskytexture;
	solidskytexture = 0;
}


/*
====================
R_TimeRefresh_f

For program optimization
====================
*/
void GLR_TimeRefresh_f (void)
{
	int			i;
	float		start, stop, time;

	qglDrawBuffer  (GL_FRONT);
	qglFinish ();

	start = Sys_DoubleTime ();
	for (i=0 ; i<128 ; i++)
	{
		r_refdef.viewangles[1] = i/128.0*360.0;
		R_RenderView ();
	}

	qglFinish ();
	stop = Sys_DoubleTime ();
	time = stop-start;
	Con_Printf ("%f seconds (%f fps)\n", time, 128/time);

	qglDrawBuffer  (GL_BACK);
	GL_EndRendering ();
	GL_DoSwap();
}

#ifndef SWQUAKE
void D_FlushCaches (void)
{
}
#endif

#endif
