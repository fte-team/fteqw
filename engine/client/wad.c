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
// wad.c

#include "quakedef.h"

int			wad_numlumps;
lumpinfo_t	*wad_lumps;
qbyte		*wad_base;

void SwapPic (qpic_t *pic);

/*
==================
W_CleanupName

Lowercases name and pads with spaces and a terminating 0 to the length of
lumpinfo_t->name.
Used so lumpname lookups can proceed rapidly by comparing 4 chars at a time
Space padding is so names can be printed nicely in tables.
Can safely be performed in place.
==================
*/
void W_CleanupName (char *in, char *out)
{
	int		i;
	int		c;
	
	for (i=0 ; i<16 ; i++ )
	{
		c = in[i];
		if (!c)
			break;
			
		if (c >= 'A' && c <= 'Z')
			c += ('a' - 'A');
		out[i] = c;
	}
	
	for ( ; i< 16 ; i++ )
		out[i] = 0;
}



/*
====================
W_LoadWadFile
====================
*/
qbyte *COM_LoadFile (char *path, int usehunk);
void W_LoadWadFile (char *filename)
{
	lumpinfo_t		*lump_p;
	wadinfo_t		*header;
	unsigned		i;
	int				infotableofs;

	if (wad_base)
		Z_Free(wad_base);
	
	wad_base = COM_LoadFile (filename, 0);
	if (!wad_base)
	{
		wad_numlumps = 0;
		Con_Printf ("W_LoadWadFile: couldn't load %s\n", filename);
		return;
	}

	header = (wadinfo_t *)wad_base;
	
	if (header->identification[0] != 'W'
	|| header->identification[1] != 'A'
	|| header->identification[2] != 'D'
	|| header->identification[3] != '2')
	{
		Con_Printf ("W_LoadWadFile: Wad file %s doesn't have WAD2 id\n",filename);
		wad_numlumps = 0;
		Z_Free(wad_base);
		wad_base = NULL;
		return;
	}
		
	wad_numlumps = LittleLong(header->numlumps);
	infotableofs = LittleLong(header->infotableofs);
	wad_lumps = (lumpinfo_t *)(wad_base + infotableofs);
	
	for (i=0, lump_p = wad_lumps ; i<wad_numlumps ; i++,lump_p++)
	{
		lump_p->filepos = LittleLong(lump_p->filepos);
		lump_p->size = LittleLong(lump_p->size);
		W_CleanupName (lump_p->name, lump_p->name);
		if (lump_p->type == TYP_QPIC)
			SwapPic ( (qpic_t *)(wad_base + lump_p->filepos));
	}
}


/*
=============
W_GetLumpinfo
=============
*/
lumpinfo_t	*W_GetLumpinfo (char *name)
{
	int		i;
	lumpinfo_t	*lump_p;
	char	clean[16];
	
	W_CleanupName (name, clean);
	
	for (lump_p=wad_lumps, i=0 ; i<wad_numlumps ; i++,lump_p++)
	{
		if (!strcmp(clean, lump_p->name))
			return lump_p;
	}
	
	Sys_Error ("W_GetLumpinfo: %s not found", name);
	return NULL;
}

void *W_SafeGetLumpName (char *name)
{
	int		i;
	lumpinfo_t	*lump_p;
	char	clean[16];

	W_CleanupName (name, clean);

	for (lump_p=wad_lumps, i=0 ; i<wad_numlumps ; i++,lump_p++)
	{
		if (!strcmp(clean, lump_p->name))
			return (void *)(wad_base+lump_p->filepos);
	}
	return NULL;
}

void *W_GetLumpName (char *name)
{
	lumpinfo_t	*lump;
	
	lump = W_GetLumpinfo (name);
	
	return (void *)(wad_base + lump->filepos);
}

void *W_GetLumpNum (int num)
{
	lumpinfo_t	*lump;
	
	if (num < 0 || num >= wad_numlumps)
		Sys_Error ("W_GetLumpNum: bad number: %i", num);
		
	lump = wad_lumps + num;
	
	return (void *)(wad_base + lump->filepos);
}

/*
=============================================================================

automatic qbyte swapping

=============================================================================
*/

void SwapPic (qpic_t *pic)
{
	pic->width = LittleLong(pic->width);
	pic->height = LittleLong(pic->height);	
}















































// based on original code by LordHavoc

//FIXME: convert to linked list. is hunk possible?
//hash tables?
#define TEXWAD_MAXIMAGES 16384
typedef struct
{
	char name[16];
	vfsfile_t *file;
	int position;
	int size;
} texwadlump_t;
int numwadtextures;
static texwadlump_t texwadlump[TEXWAD_MAXIMAGES];

typedef struct wadfile_s {
	char name[64];
	FILE *file;
	struct wadfile_s *next;
} wadfile_t;

wadfile_t *openwadfiles;

void Wads_Flush (void)
{
	wadfile_t *wf;
	while(openwadfiles)
	{
		fclose(openwadfiles->file);

		wf = openwadfiles->next;
		Z_Free(openwadfiles);
		openwadfiles = wf;
	}

	numwadtextures=0;
}
/*
====================
W_LoadTextureWadFile
====================
*/
void W_LoadTextureWadFile (char *filename, int complain)
{
	lumpinfo_t		*lumps, *lump_p;
	wadinfo_t		header;
	int				i, j;
	int				infotableofs;
	vfsfile_t		*file;
	int				numlumps;

	wadfile_t *wf = openwadfiles;
	while(wf)
	{
		if (!strcmp(wf->name, filename))	//already loaded
			return;

		wf = wf->next;
	}

	file = FS_OpenVFS(filename, "rb", FS_GAME);
	if (!file)
		file = FS_OpenVFS(va("textures/halflife/%s", filename), "rb", FS_GAME);
	if (!file)
	{
		if (complain)
			Con_Printf ("W_LoadTextureWadFile: couldn't find %s", filename);
		return;
	}

	if (VFS_READ(file, &header, sizeof(wadinfo_t)) != sizeof(wadinfo_t))
	{Con_Printf ("W_LoadTextureWadFile: unable to read wad header");return;}

	if(memcmp(header.identification, "WAD3", 4))
	{Con_Printf ("W_LoadTextureWadFile: Wad file %s doesn't have WAD3 id\n",filename);return;}

	numlumps = LittleLong(header.numlumps);
	if (numlumps < 1 || numlumps > TEXWAD_MAXIMAGES)
	{Con_Printf ("W_LoadTextureWadFile: invalid number of lumps (%i)\n", numlumps);return;}
	infotableofs = LittleLong(header.infotableofs);
	if (!VFS_SEEK(file, infotableofs))
	{Con_Printf ("W_LoadTextureWadFile: unable to seek to lump table");return;}
	if (!((lumps = Hunk_TempAlloc(sizeof(lumpinfo_t)*numlumps))))
	{Con_Printf ("W_LoadTextureWadFile: unable to allocate temporary memory for lump table");return;}

	if (VFS_READ(file, lumps, sizeof(lumpinfo_t)*numlumps) != (int)sizeof(lumpinfo_t) * numlumps)
	{Con_Printf ("W_LoadTextureWadFile: unable to read lump table");return;}

	for (i=0, lump_p = lumps ; i<numlumps ; i++,lump_p++)
	{
		W_CleanupName (lump_p->name, lump_p->name);
		for (j = 0;j < numwadtextures;j++)
		{
			if (!strcmp(lump_p->name, texwadlump[j].name)) // name match, replace old one
				break;
		}
		if (j >= TEXWAD_MAXIMAGES)
			break; // abort loading
		if (j == numwadtextures)
		{
			W_CleanupName (lump_p->name, texwadlump[j].name);
			texwadlump[j].file = file;
			texwadlump[j].position = LittleLong(lump_p->filepos);
			texwadlump[j].size = LittleLong(lump_p->disksize);
			numwadtextures++;
		}
	}	
	// leaves the file open
}

/*
void W_ApplyGamma (qbyte *data, int len, int skipalpha)
{
	int		i, inf;
	qbyte gammatable[256];
	
	if (v_gamma.value == 1.0)
	{
		for (i=0 ; i<256 ; i++)
			gammatable[i] = i;		
	}
	else
	{
		for (i=0 ; i<256 ; i++)
		{
			inf = 255 * pow ( (i+0.5)/255.5 , v_gamma.value ) + 0.5;
			if (inf < 0)
				inf = 0;
			if (inf > 255)
				inf = 255;
			gammatable[i] = inf;
		}
	}
}
*/
qbyte *W_ConvertWAD3Texture(miptex_t *tex, int *width, int *height, qboolean *usesalpha)	//returns rgba
{	
	qbyte *in, *data, *out, *pal;
	int d, p;

	int alpha = 0;

	if (tex->name[0] == '{')
		alpha = 1;
	else if (!strncmp(tex->name, "window", 6) || !strncmp(tex->name, "glass", 5))
		alpha = 2;

//use malloc here if you want, but you'll have to free it again... NUR!
#ifndef SWQUAKE	//quantity optimisation.
	data = out = Hunk_TempAllocMore(tex->width * tex->height * 4);
#else
	data = out = Hunk_TempAllocMore(((tex->width*4 * tex->height) * 85)/64); //sw mip
#endif

	if (!data)
		return NULL;

	in = (qbyte *)tex + tex->offsets[0];

	*width = tex->width;
	*height = tex->height;
	pal = in + (((tex->width * tex->height) * 85) >> 6);
	pal += 2;
#ifndef SWQUAKE
	for (d = 0;d < tex->width * tex->height;d++)	
#else
	for (d = 0;d < (tex->width * tex->height* 85)/64;d++)	//sw mip
#endif
	{
		p = *in++;
		if (alpha==1 && p == 255)	//only allow alpha on '{' textures
			out[0] = out[1] = out[2] = out[3] = 0;
		else if (alpha == 2)
		{
			p *= 3;
			out[0] = pal[p];
			out[1] = pal[p+1];
			out[2] = pal[p+2];
			out[3] = (out[0]+out[1]+out[2])/3;
		}
		else
		{
			p *= 3;
			out[0] = pal[p];
			out[1] = pal[p+1];
			out[2] = pal[p+2];
			out[3] = 255;
		}
		out += 4;
	}
	BoostGamma(data, tex->width, tex->height);
	*usesalpha = !!alpha;
	return data;
}

qbyte *W_GetTexture(char *name, int *width, int *height, qboolean *usesalpha)//returns rgba
{
	char texname[17];
	int i, j;
	vfsfile_t *file;
	miptex_t *tex;
	qbyte *data;

	texname[16] = 0;
	W_CleanupName (name, texname);
	for (i = 0;i < numwadtextures;i++)
	{
		if (!strcmp(texname, texwadlump[i].name)) // found it
		{
			file = texwadlump[i].file;
			if (!VFS_SEEK(file, texwadlump[i].position))
			{Con_Printf("W_GetTexture: corrupt WAD3 file");return NULL;}

			tex = BZ_Malloc(texwadlump[i].size);	//temp buffer for disk info (was hunk_tempalloc, but that wiped loading maps and the like
			if (!tex)
				return NULL;
			if (VFS_READ(file, tex, texwadlump[i].size) < texwadlump[i].size)
			{Con_Printf("W_GetTexture: corrupt WAD3 file");return NULL;}

			tex->width = LittleLong(tex->width);
			tex->height = LittleLong(tex->height);
			for (j = 0;j < MIPLEVELS;j++)
				tex->offsets[j] = LittleLong(tex->offsets[j]);

			data = W_ConvertWAD3Texture(tex, width, height, usesalpha);	//this will add to the temp
			BZ_Free(tex);
			return data;
		}
	}	
	return NULL;
}


//extern model_t	*loadmodel;

char wads[4096];
void Mod_ParseInfoFromEntityLump(char *data)	//actually, this should be in the model code.
{
	extern model_t *loadmodel;
	char key[128];
	char skyname[64];
	float skyrotate = 0;
	vec3_t skyaxis = {0, 0, 0};

	wads[0] = '\0';

#ifndef CLIENTONLY
	if (isDedicated)	//don't bother
		return;
#endif

	// this hack is necessary to ensure Quake 2 maps get their
	// default skybox
	if (loadmodel->fromgame == fg_quake2)
		strcpy(skyname, "unit1_");
	else
		skyname[0] = '\0';

	if (data)
	if ((data=COM_Parse(data)))	//read the map info.
	if (com_token[0] == '{')
	while (1)
	{
		if (!(data=COM_Parse(data)))
			break; // error
		if (com_token[0] == '}')
			break; // end of worldspawn
		if (com_token[0] == '_')
			strcpy(key, com_token + 1);	//_ vars are for comments/utility stuff that arn't visible to progs. Ignore them.
		else
			strcpy(key, com_token);
		if (!((data=COM_Parse(data))))
			break; // error		
		if (!strcmp("wad", key)) // for HalfLife maps
		{
			if (loadmodel->fromgame == fg_halflife)
			{
				strncat(wads, ";", 4095);	//cache it for later (so that we don't play with any temp memory yet)
				strncat(wads, com_token, 4095);	//cache it for later (so that we don't play with any temp memory yet)
			}
		}
		else if (!strcmp("skyname", key)) // for HalfLife maps
		{
			Q_strncpyz(skyname, com_token, sizeof(skyname));
		}
		else if (!strcmp("sky", key)) // for Quake2 maps
		{
			Q_strncpyz(skyname, com_token, sizeof(skyname));
		}
		else if (!strcmp("skyrotate", key))
		{
			skyrotate = atof(com_token);
		}
		else if (!strcmp("skyaxis", key))
		{
			char *s;
			Q_strncpyz(key, com_token, sizeof(key));
			s = COM_Parse(key);
			if (s)
			{
				skyaxis[0] = atof(s);
				s = COM_Parse(s);
				if (s)
				{
					skyaxis[1] = atof(s);
					COM_Parse(s);
					if (s)
						skyaxis[2] = atof(s);
				}
			}
		}
	}

	skyrotate = VectorNormalize(skyaxis);
	R_SetSky(skyname, skyrotate, skyaxis);
}

//textures/fred.wad is the DP standard - I wanna go for that one.
//textures/halfline/fred.wad is what fuhquake can use (yuck). 
//fred.wad is what half-life supports.

//we only try one download, for textures/fred.wad
//but we will load wads from the other two paths if we have them locally.
qboolean Wad_NextDownload (void)
{
	char wadname[4096+9]="textures/";
	int i, j, k;

	if (*wads)	//now go about checking the wads
	{
		j = 0;
		wads[4095] = '\0';
		for (i = 0;i < 4095;i++)
			if (wads[i] != ';' && wads[i] != '\\' && wads[i] != '/' && wads[i] != ':')
				break;
		if (wads[i])
		{
			j=i;
			for (;i < 4095;i++)
			{
				// ignore path...
				if (wads[i] == '\\' || wads[i] == '/' || wads[i] == ':')
					j = i+1;
				else if (wads[i] == ';' || wads[i] == 0)
				{
					k = wads[i];
					wads[i] = 0;
					strcpy(wadname, &wads[j]);
					if (wadname[9])
					{
						if (COM_FCheckExists(wadname+9))	//wad is in root dir, so we don't need to try textures.
							CL_CheckOrEnqueDownloadFile(wadname, wadname, DLLF_REQUIRED);	//don't skip this one, or the world is white.
					}
					wads[i] = k;
					
					j = i+1;
					if (!k)
						break;
				}
			}
		}
	}
	Wads_Flush();
	if (*wads)	//now go about loading the wads, we are now safe from tempallocs
	{
		j = 0;
		wads[4095] = '\0';
		for (i = 0;i < 4095;i++)
			if (wads[i] != ';' && wads[i] != '\\' && wads[i] != '/' && wads[i] != ':')
				break;
		if (wads[i])
		{
			j=i;
			for (;i < 4095;i++)
			{
				// ignore path...
				if (wads[i] == '\\' || wads[i] == '/' || wads[i] == ':')
					j = i+1;
				else if (wads[i] == ';' || wads[i] == 0)
				{
					k = wads[i];
					wads[i] = 0;
					strcpy(wadname+9, &wads[j]);
					if (wadname[9])
					{
						if (COM_FCheckExists(wadname+9))
							W_LoadTextureWadFile (wadname+9, false);
						else
							W_LoadTextureWadFile (wadname, false);
					}
					j = i+1;
					if (!k)
						break;
				}
			}
		}
	}
	return true;
}
