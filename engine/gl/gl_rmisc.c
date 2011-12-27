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
#ifdef GLQUAKE
#include "glquake.h"
#include "gl_draw.h"

static void R_ReloadRTLights_f(void);
static void R_SaveRTLights_f(void);

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










#if 0
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
	
	normalisationCubeMap = GL_AllocNewTexture();
	GL_BindType(GL_TEXTURE_CUBE_MAP_ARB, normalisationCubeMap);

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
		

	qglTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qglTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	qglTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	return true;
}


texid_t normalisationCubeMap;
#endif

/*
===============
R_Init
===============
*/
void GLR_ReInit (void)
{
	R_NetgraphInit();

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

extern cvar_t v_contrast, r_drawflat;
extern cvar_t r_stains, r_stainfadetime, r_stainfadeammount;

// callback defines
extern cvar_t gl_font;
extern cvar_t vid_conautoscale, vid_conheight, vid_conwidth;
extern cvar_t crosshair, crosshairimage, crosshaircolor, r_skyboxname;
extern cvar_t r_floorcolour, r_wallcolour, r_floortexture, r_walltexture;
extern cvar_t r_fastskycolour;
void GLV_Gamma_Callback(struct cvar_s *var, char *oldvalue);

void GLR_DeInit (void)
{
	Cmd_RemoveCommand ("timerefresh");
	Cmd_RemoveCommand ("r_editlights_reload");
	Cmd_RemoveCommand ("r_editlights_save");

	Cmd_RemoveCommand ("makewad");

	Cvar_Unhook(&r_skyboxname);
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

	Surf_DeInit();

	GLDraw_DeInit();
}

void GLR_Init (void)
{	
	Cmd_AddRemCommand ("timerefresh", GLR_TimeRefresh_f);
#ifdef RTLIGHTS
	Cmd_AddRemCommand ("r_editlights_reload", R_ReloadRTLights_f);
	Cmd_AddRemCommand ("r_editlights_save", R_SaveRTLights_f);
#endif

//	Cmd_AddRemCommand ("makewad", R_MakeTexWad_f);

//	Cvar_Hook(&r_floorcolour, GLR_Floorcolour_Callback);
//	Cvar_Hook(&r_fastskycolour, GLR_Fastskycolour_Callback);
//	Cvar_Hook(&r_wallcolour, GLR_Wallcolour_Callback);
//	Cvar_Hook(&r_floortexture, GLR_Floortexture_Callback);
//	Cvar_Hook(&r_walltexture, GLR_Walltexture_Callback);
//	Cvar_Hook(&r_drawflat, GLR_Drawflat_Callback);
	Cvar_Hook(&v_gamma, GLV_Gamma_Callback);
	Cvar_Hook(&v_contrast, GLV_Gamma_Callback);

	GLR_ReInit();
}

#ifdef RTLIGHTS
static void R_ImportRTLights(char *entlump)
{
	typedef enum lighttype_e {LIGHTTYPE_MINUSX, LIGHTTYPE_RECIPX, LIGHTTYPE_RECIPXX, LIGHTTYPE_NONE, LIGHTTYPE_SUN, LIGHTTYPE_MINUSXX} lighttype_t;

	/*I'm using the DP code so I know I'll get the DP results*/
	int entnum, style, islight, skin, pflags, effects, n;
	lighttype_t type;
	float origin[3], angles[3], radius, color[3], light[4], fadescale, lightscale, originhack[3], overridecolor[3], vec[4];
	char key[256], value[8192];
	int nest;

	COM_Parse(entlump);
	if (!strcmp(com_token, "Version"))
	{
		entlump = COM_Parse(entlump);
		entlump = COM_Parse(entlump);
	}

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
		nest = 1;
		while (1)
		{
			entlump = COM_Parse(entlump);
			if (!entlump)
				break; // error
			if (com_token[0] == '{')
			{
				nest++;
				continue;
			}
			if (com_token[0] == '}')
			{
				nest--;
				if (!nest)
					break; // end of entity
				continue;
			}
			if (nest!=1)
				continue;
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

			else if (!strcmp("light_radius", key))
			{
				light[0] = 1;
				light[1] = 1;
				light[2] = 1;
				light[3] = atof(value);
			}
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
			dlight_t *dl = CL_AllocSlight();
			if (!dl)
				break;
			VectorCopy(origin, dl->origin);
			AngleVectors(angles, dl->axis[0], dl->axis[1], dl->axis[2]);
			dl->radius = radius;
			VectorCopy(color, dl->color);
			dl->flags = 0;
			dl->flags |= LFLAG_REALTIMEMODE;
			dl->flags |= (pflags & PFLAGS_CORONA)?LFLAG_FLASHBLEND:0;
			dl->flags |= (pflags & PFLAGS_NOSHADOW)?LFLAG_NOSHADOWS:0;
			dl->style = style+1;

			//FIXME: cubemaps if skin >= 16
		}
	}
}

static void R_LoadRTLights(void)
{
	dlight_t *dl;
	char fname[MAX_QPATH];
	char cubename[MAX_QPATH];
	char *file;
	char *end;
	int style;

	vec3_t org;
	float radius;
	vec3_t rgb;
	unsigned int flags;

	float coronascale;
	float corona;
	float ambientscale, diffusescale, specularscale;
	vec3_t angles;

	//delete all old lights, even dynamic ones
	rtlights_first = RTL_FIRST;
	rtlights_max = RTL_FIRST;

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

		while(*file == ' ' || *file == '\t')
			file++;
		if (*file == '!')
		{
			flags = LFLAG_NOSHADOWS;
			file++;
		}
		else
			flags = 0;

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
		Q_strncpyz(cubename, com_token, sizeof(cubename));

		file = COM_Parse(file);
		//corona
		corona = file?atof(com_token):0;

		file = COM_Parse(file);
		angles[0] = file?atof(com_token):0;
		file = COM_Parse(file);
		angles[1] = file?atof(com_token):0;
		file = COM_Parse(file);
		angles[2] = file?atof(com_token):0;

		file = COM_Parse(file);
		//corrona scale
		coronascale = file?atof(com_token):0.25;

		file = COM_Parse(file);
		//ambient
		ambientscale = file?atof(com_token):0;

		file = COM_Parse(file);
		//diffuse
		diffusescale = file?atof(com_token):1;

		file = COM_Parse(file);
		//specular
		specularscale = file?atof(com_token):1;

		file = COM_Parse(file);
		flags |= file?atoi(com_token):LFLAG_REALTIMEMODE;

		if (radius)
		{
			dl = CL_AllocSlight();
			if (!dl)
				break;

			VectorCopy(org, dl->origin);
			dl->radius = radius;
			VectorCopy(rgb, dl->color);
			dl->corona = corona;
			dl->coronascale = coronascale;
			dl->die = 0;
			dl->flags = flags;
			AngleVectors(angles, dl->axis[0], dl->axis[1], dl->axis[2]);

			Q_strncpyz(dl->cubemapname, cubename, sizeof(dl->cubemapname));
			if (*dl->cubemapname)
				dl->cubetexture = R_LoadReplacementTexture(dl->cubemapname, "", IF_CUBEMAP);
			else
				dl->cubetexture = r_nulltex;

			dl->style = style+1;
		}
		file = end+1;
	}
}

static void R_SaveRTLights_f(void)
{
	dlight_t *light;
	vfsfile_t *f;
	unsigned int i;
	char fname[MAX_QPATH];
	vec3_t ang;
	COM_StripExtension(cl.worldmodel->name, fname, sizeof(fname));
	strncat(fname, ".rtlights", MAX_QPATH-1);

	FS_CreatePath(fname, FS_GAMEONLY);
	f = FS_OpenVFS(fname, "wb", FS_GAMEONLY);
	if (!f)
	{
		Con_Printf("couldn't open %s\n", fname);
		return;
	}
	for (light = cl_dlights+rtlights_first, i=rtlights_first; i<rtlights_max; i++, light++)
	{
		if (light->die)
			continue;
		if (!light->radius)
			continue;
		VectorAngles(light->axis[0], light->axis[2], ang);
		VFS_PUTS(f, va(
			"%s%f %f %f "
			"%f %f %f %f "
			"%i "
			"\"%s\" %f "
			"%f %f %f "
			"%f %f %f %f %i "
			"\n"
			,
			(light->flags & LFLAG_NOSHADOWS)?"!":"", light->origin[0], light->origin[1], light->origin[2],
			light->radius, light->color[0], light->color[1], light->color[2], 
			light->style-1,
			light->cubemapname, light->corona,
			ang[0], ang[1], ang[2],
			light->coronascale, light->ambientscale, light->diffusescale, light->specularscale, light->flags&(LFLAG_NORMALMODE|LFLAG_REALTIMEMODE|LFLAG_CREPUSCULAR)
			));
	}
	VFS_CLOSE(f);
	Con_Printf("rtlights saved to %s\n", fname);
}

void R_ReloadRTLights_f(void)
{
	if (!cl.worldmodel)
	{
		Con_Printf("Cannot reload lights at this time\n");
		return;
	}
	rtlights_first = RTL_FIRST;
	rtlights_max = RTL_FIRST;
	if (!strcmp(Cmd_Argv(1), "bsp"))
		R_ImportRTLights(cl.worldmodel->entities);
	else if (!strcmp(Cmd_Argv(1), "rtlights"))
		R_LoadRTLights();
	else if (strcmp(Cmd_Argv(1), "none"))
	{
		R_LoadRTLights();
		if (rtlights_first == rtlights_max)
			R_ImportRTLights(cl.worldmodel->entities);
	}
}
#endif

/*
===============
R_NewMap
===============
*/
void GLR_NewMap (void)
{
	char namebuf[MAX_QPATH];
	extern cvar_t host_mapname, r_shadow_realtime_dlight, r_shadow_realtime_world;
	int		i;
	
	for (i=0 ; i<256 ; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	memset (&r_worldentity, 0, sizeof(r_worldentity));
	AngleVectors(r_worldentity.angles, r_worldentity.axis[0], r_worldentity.axis[1], r_worldentity.axis[2]);
	VectorInverse(r_worldentity.axis[1]);
	r_worldentity.model = cl.worldmodel;


	COM_StripExtension(COM_SkipPath(cl.worldmodel->name), namebuf, sizeof(namebuf));
	Cvar_Set(&host_mapname, namebuf);

	Surf_DeInit();

	r_viewleaf = NULL;
	r_viewcluster = -1;
	r_oldviewcluster = 0;
	r_viewcluster2 = -1;

	Mod_ParseInfoFromEntityLump(cl.worldmodel, cl.worldmodel->entities, cl.worldmodel->name);

TRACE(("dbg: GLR_NewMap: clear particles\n"));
	P_ClearParticles ();
TRACE(("dbg: GLR_NewMap: wiping them stains (getting the cloth out)\n"));
	Surf_WipeStains();
	CL_RegisterParticles();
TRACE(("dbg: GLR_NewMap: building lightmaps\n"));
	Surf_BuildLightmaps ();


TRACE(("dbg: GLR_NewMap: ui\n"));
#ifdef VM_UI
	UI_Reset();
#endif
TRACE(("dbg: GLR_NewMap: tp\n"));
	TP_NewMap();
	R_SetSky(cl.skyname);

#ifdef MAP_PROC
	if (cl.worldmodel->fromgame == fg_doom3)
		D3_GenerateAreas(cl.worldmodel);
#endif

#ifdef RTLIGHTS
	if (r_shadow_realtime_dlight.ival || r_shadow_realtime_world.ival)
	{
		R_LoadRTLights();
		if (rtlights_first == rtlights_max)
			R_ImportRTLights(cl.worldmodel->entities);
	}
	Sh_PreGenerateLights();
#endif
}

void GLR_PreNewMap(void)
{
	r_loadbumpmapping = r_deluxemapping.ival || r_shadow_realtime_world.ival || r_shadow_realtime_dlight.ival;
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
	qboolean	finish;
	int			frames = 128;

	finish = atoi(Cmd_Argv(1));
	frames = atoi(Cmd_Argv(2));
	if (frames < 1)
		frames = 128;

#if defined(_WIN32) && !defined(_SDL)
	if (finish == 2)
	{
		extern HDC		maindc;
		qglFinish ();
		start = Sys_DoubleTime ();
		for (i=0 ; i<frames ; i++)
		{
			r_refdef.viewangles[1] = i/(float)frames*360.0;
			R_RenderView ();
			qSwapBuffers(maindc);
		}
	}
	else
#endif
	{
		qglDrawBuffer  (GL_FRONT);
		qglFinish ();

		start = Sys_DoubleTime ();
		for (i=0 ; i<frames ; i++)
		{
			r_refdef.viewangles[1] = i/(float)frames*360.0;
			R_RenderView ();
			if (finish)
				qglFinish ();
		}
	}
	qglFinish ();
	stop = Sys_DoubleTime ();
	time = stop-start;
	Con_Printf ("%f seconds (%f fps)\n", time, frames/time);

	qglDrawBuffer  (GL_BACK);
	GL_EndRendering ();
	GL_DoSwap();
}

#endif
