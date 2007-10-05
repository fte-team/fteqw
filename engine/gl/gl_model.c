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
// models.c -- model loading and caching

// models are the only shared resource between a client and server running
// on the same machine.

#include "quakedef.h"


#if defined(RGLQUAKE) || defined(D3DQUAKE)

#if defined(RGLQUAKE)
#include "glquake.h"
#endif

#ifdef D3DQUAKE
#include "d3dquake.h"
#endif

#ifdef Q3SHADERS
#include "shader.h"
#endif

extern cvar_t r_shadow_bumpscale_basetexture;
extern cvar_t r_replacemodels;

extern int gl_bumpmappingpossible;
qboolean isnotmap = true;	//used to not warp ammo models.

#ifndef SWQUAKE
model_t	*loadmodel;
char	loadname[32];	// for hunk tags
#else
extern model_t	*loadmodel;
extern char	loadname[32];	// for hunk tags
#endif

void CM_Init(void);

qboolean Mod_LoadCompositeAnim(model_t *mod, void *buffer);
qboolean GL_LoadHeightmapModel (model_t *mod, void *buffer);
qboolean GLMod_LoadSpriteModel (model_t *mod, void *buffer);
qboolean GLMod_LoadSprite2Model (model_t *mod, void *buffer);
qboolean GLMod_LoadBrushModel (model_t *mod, void *buffer);
#ifdef Q2BSPS
qboolean Mod_LoadQ2BrushModel (model_t *mod, void *buffer);
#endif
qboolean Mod_LoadHLModel (model_t *mod, void *buffer);
#ifdef ZYMOTICMODELS
qboolean Mod_LoadZymoticModel(model_t *mod, void *buffer);
qboolean Mod_LoadDarkPlacesModel(model_t *mod, void *buffer);
#endif
#ifdef MD5MODELS
qboolean Mod_LoadMD5MeshModel(model_t *mod, void *buffer);
#endif
model_t *GLMod_LoadModel (model_t *mod, qboolean crash);

#ifdef DOOMWADS
qboolean Mod_LoadDoomLevel(model_t *mod);
#endif

qboolean Mod_LoadQ1Model (model_t *mod, void *buffer);
#ifdef MD2MODELS
qboolean Mod_LoadQ2Model (model_t *mod, void *buffer);
#endif
#ifdef MD3MODELS
qboolean Mod_LoadQ3Model (model_t *mod, void *buffer);
#endif

#ifdef DOOMWADS
void GLMod_LoadDoomSprite (model_t *mod);
#endif

#define	MAX_MOD_KNOWN	2048
#ifndef SWQUAKE
model_t	mod_known[MAX_MOD_KNOWN];
int		mod_numknown;
#else

extern model_t	mod_known[MAX_MOD_KNOWN];
extern int		mod_numknown;
#endif

extern cvar_t r_loadlits;
#ifdef SPECULAR
extern cvar_t gl_specular;
#endif
extern cvar_t r_fb_bmodels;


#ifdef RUNTIMELIGHTING
model_t *lightmodel;
int numlightdata;
qboolean writelitfile;

int relitsurface;
void GLMod_UpdateLightmap(int snum)
{
	msurface_t *s;	
	if (lightmodel)
	{
//		int i;
//		for (s = lightmodel->surfaces,i=0; i < lightmodel->numsurfaces; i++,s++)
//			s->cached_dlight = -1;

		if (snum < lightmodel->numsurfaces)
		{
			s = lightmodel->surfaces + snum;
			s->cached_dlight = -1;
		}
		else
			Con_Printf("lit non-existant surface\n");
	}
}
#endif


void GLMod_TextureList_f(void)
{
	int m, i;
	texture_t *tx;
	model_t *mod;
	qboolean shownmodelname;
	for (m=0 , mod=mod_known ; m<mod_numknown ; m++, mod++)
	{
		if (mod->type == mod_brush && !mod->needload)
		{
			if (*mod->name == '*')
				continue;//	inlines don't count
			shownmodelname = false;
			for (i = 0; i < mod->numtextures; i++)
			{
				tx = mod->textures[i];
				if (!tx)
					continue;	//happens on e1m2

				if (!shownmodelname)
				{
					shownmodelname = true;
					Con_Printf("%s\n", mod->name);
				}

				Con_Printf("%s\n", tx->name);
			}
		}
	}
}

void GLMod_BlockTextureColour_f (void)
{
	char texname[64];
	model_t *mod;
	texture_t *tx;
	char *match = Cmd_Argv(1);

	int i, m;
	unsigned int colour[8*8];
	unsigned int rgba;
	((char *)&rgba)[0] = atoi(Cmd_Argv(2));
	((char *)&rgba)[1] = atoi(Cmd_Argv(3));
	((char *)&rgba)[2] = atoi(Cmd_Argv(4));
	((char *)&rgba)[3] = 255;

	sprintf(texname, "8*8_%i_%i_%i", (int)((char *)&rgba)[0], (int)((char *)&rgba)[1], (int)((char *)&rgba)[2]);

	for (i = 0; i < sizeof(colour)/sizeof(colour[0]); i++)
		colour[i] = rgba;

	for (m=0 , mod=mod_known ; m<mod_numknown ; m++, mod++)
	{
		if (mod->type == mod_brush && !mod->needload)
		{
			for (i = 0; i < mod->numtextures; i++)
			{
				tx = mod->textures[i];
				if (!tx)
					continue;	//happens on e1m2

				if (!stricmp(tx->name, match))
				{
					tx->gl_texturenum = R_LoadTexture32(texname, 8, 8, colour, true, false);
				}
			}
		}
	}
}
/*
===============
Mod_Init
===============
*/
void GLMod_Init (void)
{
	mod_numknown = 0;
	Q1BSP_Init();

	Cmd_AddRemCommand("mod_texturelist", GLMod_TextureList_f);
	Cmd_AddRemCommand("mod_usetexture", GLMod_BlockTextureColour_f);
}

void GLMod_Shutdown (void)
{
	mod_numknown = 0;

	Cmd_RemoveCommand("mod_texturelist");
	Cmd_RemoveCommand("mod_usetexture");

#ifdef RUNTIMELIGHTING
	lightmodel = NULL;
#endif
}

/*
===============
Mod_Init

Caches the data if needed
===============
*/
void *GLMod_Extradata (model_t *mod)
{
	void	*r;
	
	r = Cache_Check (&mod->cache);
	if (r)
		return r;

	GLMod_LoadModel (mod, true);
	
	if (!mod->cache.data)
		Sys_Error ("Mod_Extradata: caching failed");
	return mod->cache.data;
}

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *GLMod_PointInLeaf (model_t *model, vec3_t p)
{
	mnode_t		*node;
	float		d;
	mplane_t	*plane;
	
	if (!model)
	{		
		Sys_Error ("Mod_PointInLeaf: bad model");
	}
	if (!model->nodes)
		return NULL;
#ifdef Q2BSPS
	if (model->fromgame == fg_quake2 || model->fromgame == fg_quake3)
	{
		return model->leafs + CM_PointLeafnum(model, p);
	}
#endif
	if (model->fromgame == fg_doom)
	{
		return NULL;
	}

	node = model->nodes;
	while (1)
	{
		if (node->contents < 0)
			return (mleaf_t *)node;
		plane = node->plane;
		d = DotProduct (p,plane->normal) - plane->dist;
		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}
	
	return NULL;	// never reached
}

/*
===================
Mod_ClearAll
===================
*/
void GLMod_ClearAll (void)
{
	int		i;
	model_t	*mod;

#ifdef RUNTIMELIGHTING
	lightmodel = NULL;
#endif

	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
		if (mod->type != mod_alias
			&& mod->type != mod_halflife
			)
			mod->needload = true;
}

void GLMod_Think (void)
{
#ifdef RUNTIMELIGHTING
	if (lightmodel)
	{
		if (relitsurface >= lightmodel->numsurfaces)
		{
			return;
		}
		LightFace(relitsurface);
		GLMod_UpdateLightmap(relitsurface);

		relitsurface++;

		if (relitsurface >= lightmodel->numsurfaces)
		{
			char filename[MAX_QPATH];
			char *f;
			Con_Printf("Finished lighting level\n");


			strcpy(filename, lightmodel->name);
			f = COM_SkipPath(filename);
			*f = '\0';

			if (lightmodel->deluxdata)
			{
				COM_StripExtension(lightmodel->name, filename, sizeof(filename));
				COM_DefaultExtension(filename, ".lux", sizeof(filename));
				FS_WriteFile(filename, lightmodel->deluxdata-8, numlightdata*3+8, FS_GAME);
			}

			if (writelitfile)	//the user might already have a lit file (don't overwrite it).
			{
				COM_StripExtension(lightmodel->name, filename, sizeof(filename));
				COM_DefaultExtension(filename, ".lit", sizeof(filename));
				FS_WriteFile(filename, lightmodel->lightdata-8, numlightdata*3+8, FS_GAME);
			}
		}
	}
#endif
}

/*
==================
Mod_FindName

==================
*/
model_t *GLMod_FindName (char *name)
{
	int		i;
	model_t	*mod;
	
//	if (!name[0])
//		Sys_Error ("Mod_ForName: NULL name");
		
//
// search the currently loaded models
//
	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
		if (!strcmp (mod->name, name) )
			break;
			
	if (i == mod_numknown)
	{
		if (mod_numknown == MAX_MOD_KNOWN)
			Sys_Error ("mod_numknown == MAX_MOD_KNOWN");
		memset(mod, 0, sizeof(model_t));	//clear the old model as the renderers use the same globals
		strcpy (mod->name, name);
		mod->needload = true;
		mod_numknown++;
		mod->particleeffect = -1;
		mod->particletrail = -1;
	}

	return mod;
}

/*
==================
Mod_TouchModel

==================
*/
void GLMod_TouchModel (char *name)
{
	model_t	*mod;
	
	mod = GLMod_FindName (name);
	
	if (!mod->needload)
	{
		if (mod->type == mod_alias
			|| mod->type == mod_halflife
			)
			Cache_Check (&mod->cache);
	}
}

/*
==================
Mod_LoadModel

Loads a model into the cache
==================
*/
char *COM_FileExtension (char *in);
model_t *GLMod_LoadModel (model_t *mod, qboolean crash)
{
	void	*d;
	unsigned *buf = NULL;
	qbyte	stackbuf[1024];		// avoid dirtying the cache heap
	char mdlbase[MAX_QPATH];
	qboolean lastload = false;
	char *replstr;
	qboolean doomsprite = false;

	char *ext;

	if (!mod->needload && mod->type != mod_dummy)
	{
		if (mod->type == mod_alias
			|| mod->type == mod_halflife
			)
		{
			d = Cache_Check (&mod->cache);
			if (d)
				return mod;
		}
		else
			return mod;		// not cached at all
	}
	
	loadmodel = mod;

#ifdef Q2BSPS
	if (!*mod->name)
	{
		if (!Mod_LoadQ2BrushModel (mod, buf))
			goto couldntload;
		mod->needload = false;
		P_DefaultTrail(mod);
		return mod;
	}
#endif
	
//
// load the file
//
	// set necessary engine flags for loading purposes
	if (!strcmp(mod->name, "progs/player.mdl"))
		mod->engineflags |= MDLF_PLAYER | MDLF_DOCRC;
	else if (!strcmp(mod->name, "progs/flame.mdl") || 
		!strcmp(mod->name, "progs/flame2.mdl"))
		mod->engineflags |= MDLF_FLAME;
	else if (!strcmp(mod->name, "progs/bolt.mdl") ||
		!strcmp(mod->name, "progs/bolt2.mdl") ||
		!strcmp(mod->name, "progs/bolt3.mdl") ||
		!strcmp(mod->name, "progs/beam.mdl") || 
		!strcmp(mod->name, "models/stsunsf2.mdl") || 
		!strcmp(mod->name, "models/stsunsf1.mdl") ||
		!strcmp(mod->name, "models/stice.mdl"))
		mod->engineflags |= MDLF_BOLT;
	else if (!strcmp(mod->name, "progs/eyes.mdl"))
		mod->engineflags |= MDLF_DOCRC;

	// call the apropriate loader
	mod->needload = false;

	// get string used for replacement tokens
	ext = COM_FileExtension(mod->name);
	if (!Q_strcasecmp(ext, "spr") || !Q_strcasecmp(ext, "sp2"))
		replstr = NULL; // sprite
	else if (!Q_strcasecmp(ext, "dsp")) // doom sprite
	{
		replstr = NULL;
		doomsprite = true;
	}
	else // assume models
		replstr = r_replacemodels.string;

	// gl_load24bit 0 disables all replacements
	if (gl_load24bit.value)
		replstr = NULL;

	COM_StripExtension(mod->name, mdlbase, sizeof(mdlbase));

	while (1)
	{
		for (replstr = COM_ParseStringSet(replstr); com_token[0] && !buf; replstr = COM_ParseStringSet(replstr))
			buf = (unsigned *)COM_LoadStackFile (va("%s.%s", mdlbase, com_token), stackbuf, sizeof(stackbuf));

		if (!buf)
		{
			if (lastload) // only load unreplaced file once
				break;
			lastload = true;
			buf = (unsigned *)COM_LoadStackFile (mod->name, stackbuf, sizeof(stackbuf));
			if (!buf)
			{
#ifdef DOOMWADS
				if (doomsprite) // special case needed for doom sprites
				{
					mod->needload = false;
					GLMod_LoadDoomSprite(mod);
					P_DefaultTrail(mod);
					return mod;
				}
#endif
				break; // failed to load unreplaced file and nothing left
			}
		}
	
//
// allocate a new model
//
		COM_FileBase (mod->name, loadname, sizeof(loadname));

//
// fill it in
//
		
		switch (LittleLong(*(unsigned *)buf))
		{
//The binary 3d mesh model formats
		case IDPOLYHEADER:
			if (!Mod_LoadQ1Model(mod, buf))
				continue;
			break;
		
#ifdef MD2MODELS
		case MD2IDALIASHEADER:
			if (!Mod_LoadQ2Model(mod, buf))
				continue;
			break;
#endif

#ifdef MD3MODELS
		case MD3_IDENT:
			if (!Mod_LoadQ3Model (mod, buf))
				continue;
			break;
#endif

#ifdef HALFLIFEMODELS
		case (('T'<<24)+('S'<<16)+('D'<<8)+'I'):
			if (!Mod_LoadHLModel (mod, buf))
				continue;
			break;
#endif

//Binary skeletal model formats
#ifdef ZYMOTICMODELS
		case (('O'<<24)+('M'<<16)+('Y'<<8)+'Z'):
			if (!Mod_LoadZymoticModel(mod, buf))
				continue;
			break;
		case (('K'<<24)+('R'<<16)+('A'<<8)+'D'):
			if (!Mod_LoadDarkPlacesModel(mod, buf))
				continue;
			break;
#endif


//Binary Sprites
#ifdef SP2MODELS
		case IDSPRITE2HEADER:
			if (!GLMod_LoadSprite2Model (mod, buf))
				continue;
			break;
#endif

		case IDSPRITEHEADER:
			if (!GLMod_LoadSpriteModel (mod, buf))
				continue;
			break;


	//Binary Map formats
#ifdef Q2BSPS
		case ('R'<<0)+('B'<<8)+('S'<<16)+('P'<<24):
		case IDBSPHEADER:	//looks like id switched to have proper ids
			if (!Mod_LoadQ2BrushModel (mod, buf))
				continue;
			break;
#endif
#ifdef DOOMWADS
		case (('D'<<24)+('A'<<16)+('W'<<8)+'I'):	//the id is hacked by the FS .wad loader (main wad).
		case (('D'<<24)+('A'<<16)+('W'<<8)+'P'):	//the id is hacked by the FS .wad loader (patch wad).
			if (!Mod_LoadDoomLevel (mod))
				continue;
			break;
#endif

		case 30:	//hl
		case 29:	//q1
		case 28:	//prerel
			if (!GLMod_LoadBrushModel (mod, buf))
				continue;
			break;

	//Text based misc types.
		default:
			//check for text based headers
			COM_Parse((char*)buf);
#ifdef MD5MODELS
			if (!strcmp(com_token, "MD5Version"))	//doom3 format, text based, skeletal
			{
				if (!Mod_LoadMD5MeshModel (mod, buf))
					continue;
				break;
			}
			if (!strcmp(com_token, "EXTERNALANIM"))	//custom format, text based, specifies skeletal models to load and which md5anim files to use.
			{
				if (!Mod_LoadCompositeAnim (mod, buf))
					continue;
				break;
			}
#endif
#ifdef TERRAIN
			if (!strcmp(com_token, "terrain"))	//custom format, text based.
			{
				if (!GL_LoadHeightmapModel(mod, buf))
					continue;
				break;
			}
#endif

			Con_Printf(CON_WARNING "Unrecognised model format %i\n", LittleLong(*(unsigned *)buf));
			continue;
		}

		P_DefaultTrail(mod);
		Validation_IncludeFile(mod->name, (char *)buf, com_filesize);

		return mod;
	}

couldntload:
	if (crash)
		Host_EndGame ("Mod_NumForName: %s not found or couldn't load", mod->name);

	Con_Printf(CON_ERROR "Unable to load or replace %s\n", mod->name);
	mod->type = mod_dummy;
	mod->mins[0] = -16;
	mod->mins[1] = -16;
	mod->mins[2] = -16;
	mod->maxs[0] = 16;
	mod->maxs[1] = 16;
	mod->maxs[2] = 16;
	mod->needload = true;
	mod->engineflags = 0;
	P_DefaultTrail(mod);
	return mod;
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t *GLMod_ForName (char *name, qboolean crash)
{
	model_t	*mod;
	
	mod = GLMod_FindName (name);
	
	return GLMod_LoadModel (mod, crash);
}


/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

#ifdef SWQUAKE
extern qbyte	*mod_base;
#else
qbyte	*mod_base;
#endif

char *advtexturedesc;
char *mapsection;
char *defaultsection;

static char *GLMod_TD_LeaveSection(char *file)
{	//recursive routine to find the next }
	while(file)
	{
		file = COM_Parse(file);
		if (*com_token == '{')
			file = GLMod_TD_LeaveSection(file);
		else if (*com_token == '}')
			return file;
	}
	return NULL;
}

static char *GLMod_TD_Section(char *file, const char *sectionname)
{	//position within the open brace.
	while(file)
	{
		while(*file <= ' ')	//skip whitespace and new lines.
		{
			if (!*file)
				return NULL;
			file++;
		}
		file = COM_Parse(file);
		if (!stricmp(com_token, sectionname))
		{
			file = COM_Parse(file);
			if (*com_token != '{')
				return NULL;
			return file;
		}

		if (*com_token == '{')
			file = GLMod_TD_LeaveSection(file);
	}
	return NULL;
}
void GLMod_InitTextureDescs(char *mapname)
{
	if (advtexturedesc)
		BZ_Free(advtexturedesc);
	advtexturedesc = COM_LoadMallocFile(va("maps/shaders/%s.shaders", mapname));
	if (!advtexturedesc)
		advtexturedesc = COM_LoadMallocFile(va("shaders/%s.shaders", mapname));
	if (advtexturedesc)
	{
		mapsection = advtexturedesc;
		defaultsection = NULL;
	}
	else
	{
		advtexturedesc = COM_LoadMallocFile(va("map.shaders", mapname));
		mapsection = GLMod_TD_Section(advtexturedesc, mapname);
		defaultsection = GLMod_TD_Section(advtexturedesc, "default");
	}
}
void GLMod_LoadAdvancedTextureSection(char *section, char *name, int *base, int *norm, int *luma, int *gloss, int *alphamode, qboolean *cull) //fixme: add gloss
{
	char stdname[MAX_QPATH] = "";
	char flatname[MAX_QPATH] = "";
	char bumpname[MAX_QPATH] = "";
	char normname[MAX_QPATH] = "";
	char lumaname[MAX_QPATH] = "";
	char glossname[MAX_QPATH] = "";

	section = GLMod_TD_Section(section, name);

	while(section)
	{
		section = COM_Parse(section);
		if (*com_token == '}')
			break;

		while(*section <= ' ')	//get rid of nasty whitespace.
		{
			if (!*section)
				return;
			section++;
		}
		if (*section == '=')
			section++;	//evil notation.

		if (!stricmp(com_token, "texture") || !stricmp(com_token, "base"))
		{
			section = COM_Parse(section);
			Q_strncpyz(stdname, com_token, sizeof(stdname));
		}
		else if (!stricmp(com_token, "flatmap") || !stricmp(com_token, "flat")
			|| !stricmp(com_token, "diffusemap") || !stricmp(com_token, "diffuse"))
		{
			section = COM_Parse(section);
			Q_strncpyz(flatname, com_token, sizeof(flatname));
		}
		else if (!stricmp(com_token, "bumpmap") || !stricmp(com_token, "bump"))
		{
			section = COM_Parse(section);
			Q_strncpyz(bumpname, com_token, sizeof(bumpname));
		}
		else if (!stricmp(com_token, "normalmap") || !stricmp(com_token, "normal"))
		{
			section = COM_Parse(section);
			Q_strncpyz(normname, com_token, sizeof(normname));
		}
		else if (!stricmp(com_token, "glossmap") || !stricmp(com_token, "gloss"))
		{
			section = COM_Parse(section);
			Q_strncpyz(glossname, com_token, sizeof(glossname));
		}
		else if (!stricmp(com_token, "luma") || !stricmp(com_token, "glow")
			|| !stricmp(com_token, "ambient") || !stricmp(com_token, "ambientmap"))
		{
			section = COM_Parse(section);
			Q_strncpyz(lumaname, com_token, sizeof(lumaname));
		}
		else
		{
			//best thing we can do is jump to the end of the line, and hope they were a good creator...
			while(*section && *section != '\n')
				section++;
		}
	}

	//okay it's all parsed. Try and interpret the data now.

	*base = 0;
	if (norm)
		*norm = 0;
	if (luma)
		*luma = 0;
	if (gloss)
		*gloss = 0;

	if (!*stdname && !*flatname)
		return;
TRACE(("dbg: GLMod_LoadAdvancedTextureSection: %s\n", name));

	if (norm && gl_bumpmappingpossible && cls.allow_bump)
	{
		*base = 0;
		*norm = 0;
		if (!*norm && *normname)
			*norm = Mod_LoadHiResTexture(normname, NULL, true, false, false);
		if (!*norm && *bumpname)
			*norm = Mod_LoadBumpmapTexture(bumpname, NULL);

		if (*norm && *flatname)
			*base = Mod_LoadHiResTexture(flatname, NULL, true, false, true);
	}
	else
	{
		*base = 0;
		if (norm)
			*norm = 0;
	}
	if (!*base && *stdname)
		*base = Mod_LoadHiResTexture(stdname, NULL, true, false, true);
	if (!*base && *flatname)
		*base = Mod_LoadHiResTexture(flatname, NULL, true, false, true);
	if (luma && *lumaname)
		*luma = Mod_LoadHiResTexture(lumaname, NULL, true, true, true);

	if (*norm && gloss && *glossname && gl_specular.value)
		*gloss = Mod_LoadHiResTexture(glossname, NULL, true, false, true);
}

void GLMod_LoadAdvancedTexture(char *name, int *base, int *norm, int *luma, int *gloss, int *alphamode, qboolean *cull)	//fixme: add gloss
{
	if (!gl_load24bit.value)
		return;

	if (mapsection)
	{
		GLMod_LoadAdvancedTextureSection(mapsection, name,base,norm,luma,gloss,alphamode,cull);
		if (*base)
			return;
	}
	if (defaultsection)
		GLMod_LoadAdvancedTextureSection(defaultsection, name,base,norm,luma,gloss,alphamode,cull);
}

/*
=================
Mod_LoadTextures
=================
*/
qboolean GLMod_LoadTextures (lump_t *l)
{
	extern cvar_t gl_shadeq1, gl_shadeq1_name;
	extern int gl_bumpmappingpossible;
	int		i, j, pixels, num, max, altmax;
	miptex_t	*mt;
	texture_t	*tx, *tx2;
	texture_t	*anims[10];
	texture_t	*altanims[10];
	char altname[256];
	dmiptexlump_t *m;
	qboolean alphaed;
	qbyte *base;

TRACE(("dbg: GLMod_LoadTextures: inittexturedescs\n"));

	GLMod_InitTextureDescs(loadname);

	if (!l->filelen)
	{
		loadmodel->textures = NULL;
		return true;
	}
	m = (dmiptexlump_t *)(mod_base + l->fileofs);
	
	m->nummiptex = LittleLong (m->nummiptex);
	
	loadmodel->numtextures = m->nummiptex;
	loadmodel->textures = Hunk_AllocName (m->nummiptex * sizeof(*loadmodel->textures) , loadname);

	for (i=0 ; i<m->nummiptex ; i++)
	{
		m->dataofs[i] = LittleLong(m->dataofs[i]);
		if (m->dataofs[i] == -1)	//e1m2, this happens
			continue;
		mt = (miptex_t *)((qbyte *)m + m->dataofs[i]);

	TRACE(("dbg: GLMod_LoadTextures: texture %s\n", loadname));

		if (!*mt->name)	//I HATE MAPPERS!
		{
			sprintf(mt->name, "unnamed%i", i);
			Con_Printf(CON_WARNING "warning: unnamed texture in %s, renaming to %s\n", loadmodel->name, mt->name);
		}

		mt->width = LittleLong (mt->width);
		mt->height = LittleLong (mt->height);
		for (j=0 ; j<MIPLEVELS ; j++)
			mt->offsets[j] = LittleLong (mt->offsets[j]);
		
		if ( (mt->width & 15) || (mt->height & 15) )
			Con_Printf (CON_WARNING "Warning: Texture %s is not 16 aligned", mt->name);
		if (mt->width < 1 || mt->height < 1)
			Con_Printf (CON_WARNING "Warning: Texture %s has no size", mt->name);
		pixels = mt->width*mt->height/64*85;
		tx = Hunk_AllocName (sizeof(texture_t)/* +pixels*/, loadname );
		loadmodel->textures[i] = tx;

		memcpy (tx->name, mt->name, sizeof(tx->name));
		tx->width = mt->width;
		tx->height = mt->height;

		tx->parttype = P_ParticleTypeForName(va("tex_%s", tx->name));

		if (!mt->offsets[0])	//this is a hl external style texture, load it a little later (from a wad)
		{
//			tx->gl_texturenum = Mod_LoadReplacementTexture("light1_4", true, false);
			continue;
		}
//		for (j=0 ; j<MIPLEVELS ; j++)
//			tx->offsets[j] = mt->offsets[j] + sizeof(texture_t) - sizeof(miptex_t);
		// the pixels immediately follow the structures
//		memcpy ( tx+1, mt+1, pixels);	//have to be saved for dynamic screen changing (done by reloading entire vid/draw subsystem and all textures)
		
		if (!Q_strncmp(mt->name,"sky",3))
		{
			tx->offsets[0] = (char *)mt + mt->offsets[0] - (char *)tx;
			R_InitSky (tx);
		}
		else
#ifdef PEXT_BULLETENS
			if (!R_AddBulleten(tx))
#endif
		{
			tx->gl_texturenum = 0;
			GLMod_LoadAdvancedTexture(tx->name, &tx->gl_texturenum, &tx->gl_texturenumbumpmap, &tx->gl_texturenumfb, &tx->gl_texturenumspec, NULL, NULL);
			if (tx->gl_texturenum)
				continue;

			base = (qbyte *)(mt+1);

			if (loadmodel->fromgame == fg_halflife)
			{//external textures have already been filtered.
				base = W_ConvertWAD3Texture(mt, &mt->width, &mt->height, &alphaed);	//convert texture to 32 bit.
				tx->alphaed = alphaed;
				if (!(tx->gl_texturenum = Mod_LoadReplacementTexture(mt->name, loadname, true, alphaed, true)))
					if (!(tx->gl_texturenum = Mod_LoadReplacementTexture(mt->name, "bmodels", true, alphaed, true)))
						tx->gl_texturenum = R_LoadTexture32 (mt->name, tx->width, tx->height, (unsigned int *)base, true, alphaed);

				*tx->name = *mt->name;
			}
			else
			{
				if (!(tx->gl_texturenum = Mod_LoadReplacementTexture(mt->name, loadname, true, false, true)))
					if (!(tx->gl_texturenum = Mod_LoadReplacementTexture(mt->name, "bmodels", true, false, true)))
						tx->gl_texturenum = R_LoadTexture8 (mt->name, tx->width, tx->height, base, true, false);

				if (r_fb_bmodels.value)
				{
					snprintf(altname, sizeof(altname)-1, "%s_luma", mt->name);
					if (gl_load24bit.value)
					{
						tx->gl_texturenumfb = Mod_LoadReplacementTexture(altname, loadname, true, false, true);
						if (!tx->gl_texturenumfb)
							tx->gl_texturenumfb = Mod_LoadReplacementTexture(altname, "bmodels", true, false, true);
					}
					if (!tx->gl_texturenumfb)	//generate one (if possible).
						tx->gl_texturenumfb = R_LoadTextureFB(altname, tx->width, tx->height, base, true, true);
				}
			}

			tx->gl_texturenumbumpmap = 0;
			if (gl_bumpmappingpossible && cls.allow_bump)
			{
				extern cvar_t gl_bump;
				if (gl_bump.value<2)	//set to 2 to have faster loading.
				{
					snprintf(altname, sizeof(altname)-1, "%s_norm", mt->name);
					tx->gl_texturenumbumpmap = Mod_LoadHiResTexture(altname, loadname, true, false, false);
					if (!tx->gl_texturenumbumpmap)
						tx->gl_texturenumbumpmap = Mod_LoadHiResTexture(altname, "bmodels", true, false, false);
				}
				if (!tx->gl_texturenumbumpmap)
				{
					if (gl_load24bit.value)
					{
						snprintf(altname, sizeof(altname)-1, "%s_bump", mt->name);
						tx->gl_texturenumbumpmap = Mod_LoadBumpmapTexture(altname, loadname);
						if (!tx->gl_texturenumbumpmap)
							tx->gl_texturenumbumpmap = Mod_LoadBumpmapTexture(altname, "bmodels");
					}
					else
						snprintf(altname, sizeof(altname)-1, "%s_bump", mt->name);
				}

				if (!(tx->gl_texturenumbumpmap) && loadmodel->fromgame != fg_halflife)
				{
					base = (qbyte *)(mt+1);	//convert to greyscale.
					for (j = 0; j < pixels; j++)
						base[j] = (host_basepal[base[j]*3] + host_basepal[base[j]*3+1] + host_basepal[base[j]*3+2]) / 3;

					tx->gl_texturenumbumpmap = R_LoadTexture8Bump(altname, tx->width, tx->height, base, true, r_shadow_bumpscale_basetexture.value);	//normalise it and then bump it.
				}

				//don't do any complex quake 8bit -> glossmap. It would likly look a little ugly...
				if (gl_specular.value && gl_load24bit.value)
				{
					snprintf(altname, sizeof(altname)-1, "%s_gloss", mt->name);
					tx->gl_texturenumspec = Mod_LoadHiResTexture(altname, loadname, true, false, false);
					if (!tx->gl_texturenumspec)
						tx->gl_texturenumspec = Mod_LoadHiResTexture(altname, "bmodels", true, false, false);
				}
			}
		}
#ifdef Q3SHADERS	//load q3 syntax shader last, after the textures inside the bsp have been loaded and stuff.
		if (cls.allow_shaders && gl_shadeq1.value && *gl_shadeq1_name.string)
		{
			char *star;
			//find the *
			if (!strcmp(gl_shadeq1_name.string, "*"))
				tx->shader = R_RegisterCustom(mt->name, NULL);	//just load the regular name.
			else if (!(star = strchr(gl_shadeq1_name.string, '*')) || (strlen(gl_shadeq1_name.string)+strlen(mt->name)+1>=sizeof(altname)))	//it's got to fit.
				tx->shader = R_RegisterCustom(gl_shadeq1_name.string, NULL);
			else
			{
				strncpy(altname, gl_shadeq1_name.string, star-gl_shadeq1_name.string);	//copy the left
				altname[star-gl_shadeq1_name.string] = '\0';
				strcat(altname, mt->name);	//insert the *
				strcat(altname, star+1);	//add any final text.
				tx->shader = R_RegisterCustom(altname, NULL);
			}
		}
#endif
	}
//
// sequence the animations
//
	for (i=0 ; i<m->nummiptex ; i++)
	{
		tx = loadmodel->textures[i];
		if (!tx || tx->name[0] != '+')
			continue;
		if (tx->anim_next)
			continue;	// already sequenced

	// find the number of frames in the animation
		memset (anims, 0, sizeof(anims));
		memset (altanims, 0, sizeof(altanims));

		max = tx->name[1];
		altmax = 0;
		if (max >= 'a' && max <= 'z')
			max -= 'a' - 'A';
		if (max >= '0' && max <= '9')
		{
			max -= '0';
			altmax = 0;
			anims[max] = tx;
			max++;
		}
		else if (max >= 'A' && max <= 'J')
		{
			altmax = max - 'A';
			max = 0;
			altanims[altmax] = tx;
			altmax++;
		}
		else
		{
			Con_Printf (CON_ERROR "Bad animating texture %s\n", tx->name);
			return false;
		}

		for (j=i+1 ; j<m->nummiptex ; j++)
		{
			tx2 = loadmodel->textures[j];
			if (!tx2 || tx2->name[0] != '+')
				continue;
			if (strcmp (tx2->name+2, tx->name+2))
				continue;

			num = tx2->name[1];
			if (num >= 'a' && num <= 'z')
				num -= 'a' - 'A';
			if (num >= '0' && num <= '9')
			{
				num -= '0';
				anims[num] = tx2;
				if (num+1 > max)
					max = num + 1;
			}
			else if (num >= 'A' && num <= 'J')
			{
				num = num - 'A';
				altanims[num] = tx2;
				if (num+1 > altmax)
					altmax = num+1;
			}
			else
			{
				Con_Printf (CON_ERROR "Bad animating texture %s\n", tx->name);
				return false;
			}
		}
		
#define	ANIM_CYCLE	2
	// link them all together
		for (j=0 ; j<max ; j++)
		{
			tx2 = anims[j];
			if (!tx2)
			{
				Con_Printf (CON_ERROR "Missing frame %i of %s\n",j, tx->name);
				return false;
			}
			tx2->anim_total = max * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j+1) * ANIM_CYCLE;
			tx2->anim_next = anims[ (j+1)%max ];
			if (altmax)
				tx2->alternate_anims = altanims[0];
		}
		for (j=0 ; j<altmax ; j++)
		{
			tx2 = altanims[j];
			if (!tx2)
			{
				Con_Printf (CON_ERROR "Missing frame %i of %s\n",j, tx->name);
				return false;
			}
			tx2->anim_total = altmax * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j+1) * ANIM_CYCLE;
			tx2->anim_next = altanims[ (j+1)%altmax ];
			if (max)
				tx2->alternate_anims = anims[0];
		}
	}

	return true;
}

void GLMod_NowLoadExternal(void)
{
	extern int gl_bumpmappingpossible;
	int i, width, height;
	qboolean alphaed;
	texture_t	*tx;

	if (loadmodel->fromgame != fg_halflife)
		return;

	for (i=0 ; i<loadmodel->numtextures ; i++)
	{
		tx = loadmodel->textures[i];
		if (!tx)	//e1m2, this happens
			continue;

		if (!tx->gl_texturenum)
		{
#ifdef PEXT_BULLETENS
			if (!R_AddBulleten(tx))
#endif
			{
				qbyte * data;

				data = W_GetTexture(tx->name, &width, &height, &alphaed);
				if (data)
				{	//data is from temp hunk, so no need to free.
					tx->alphaed = alphaed;
				}
				
				if (!(tx->gl_texturenum = Mod_LoadHiResTexture(tx->name, loadname, true, false, true)))
					if (!(tx->gl_texturenum = Mod_LoadHiResTexture(tx->name, "bmodels", true, false, true)))
						tx->gl_texturenum = Mod_LoadReplacementTexture("light1_4", NULL, true, false, true);	//a fallback. :/
			}
		}
		if (!tx->gl_texturenumbumpmap && *tx->name != '{' && gl_bumpmappingpossible && cls.allow_bump)
		{
			tx->gl_texturenumbumpmap = Mod_LoadBumpmapTexture(va("%s_bump", tx->name), loadname);
			if (!tx->gl_texturenumbumpmap)
				tx->gl_texturenumbumpmap = Mod_LoadBumpmapTexture(va("%s_bump", tx->name), "bmodels");
			if (!tx->gl_texturenumbumpmap)
			{
				qbyte *data;
				qbyte *heightmap;
				int width, height;
				int j;

				data = W_GetTexture(tx->name, &width, &height, &alphaed);
				if (!data)
					continue;

				heightmap = Hunk_TempAllocMore(width*height);
				for (j = 0; j < width*height; j++)
				{
					*heightmap++ = (data[j*4+0] + data[j*4+1] + data[j*4+2])/3;
				}
				
				tx->gl_texturenumbumpmap = R_LoadTexture8Bump (va("%s_bump", tx->name), width, height, heightmap-j, true, r_shadow_bumpscale_basetexture.value);
			}
		}
	}
}

qbyte lmgamma[256];
void BuildLightMapGammaTable (float g, float c)
{
	int i, inf;

//	g = bound (0.1, g, 3);
//	c = bound (1, c, 3);

	if (g == 1 && c == 1)
	{
		for (i = 0; i < 256; i++)
			lmgamma[i] = i;
		return;
	}

	for (i = 0; i < 256; i++)
	{
		inf = 255 * pow ((i + 0.5) / 255.5 * c, g) + 0.5;
		if (inf < 0)
			inf = 0;
		else if (inf > 255)
			inf = 255;		
		lmgamma[i] = inf;
	}
}


/*
=================
Mod_LoadLighting
=================
*/
void GLMod_LoadLighting (lump_t *l)
{	
	qbyte *luxdata = NULL;
	int mapcomeswith24bitcolouredlighting = false;
	loadmodel->engineflags &= ~MDLF_RGBLIGHTING;

	//lit file light intensity is made to match the world's light intensity.
//	if (cls.allow_lightmapgamma)
//		BuildLightMapGammaTable(0.6, 2);
//	else
		BuildLightMapGammaTable(1, 1);

	loadmodel->lightdata = NULL;
	loadmodel->deluxdata = NULL;
	if (!l->filelen)
	{
		return;
	}

	if (loadmodel->fromgame == fg_halflife || loadmodel->fromgame == fg_quake2 || loadmodel->fromgame == fg_quake3)
		mapcomeswith24bitcolouredlighting = true;

	if (!mapcomeswith24bitcolouredlighting && r_loadlits.value && gl_bumpmappingpossible)	//fixme: adjust the light intensities.
	{	//the map util has a '-scalecos X' parameter. use 0 if you're going to use only just lux. without lux scalecos 0 is hideous.
		char luxname[MAX_QPATH];		
		if (!luxdata)
		{							
			strcpy(luxname, loadmodel->name);
			COM_StripExtension(loadmodel->name, luxname, sizeof(luxname));
			COM_DefaultExtension(luxname, ".lux", sizeof(luxname));
			luxdata = COM_LoadHunkFile(luxname);
		}
		if (!luxdata)
		{
			strcpy(luxname, "luxs/");
			COM_StripExtension(COM_SkipPath(loadmodel->name), luxname+5, sizeof(luxname)-5);
			strcat(luxname, ".lux");

			luxdata = COM_LoadHunkFile(luxname);
		}
		COM_StripExtension(COM_SkipPath(loadmodel->name), luxname+5, sizeof(luxname)-5);
		strcat(luxname, ".lux");
		if (luxdata)
		{
			if (l->filelen && l->filelen != (com_filesize-8)/3)
			{
				Con_Printf("deluxmap \"%s\" doesn't match level. Ignored.\n", luxname);
				luxdata=NULL;
			}
			else if (luxdata[0] == 'Q' && luxdata[1] == 'L' && luxdata[2] == 'I' && luxdata[3] == 'T')
			{
				if (LittleLong(*(int *)&luxdata[4]) == 1)
				{
					luxdata+=8;
					loadmodel->deluxdata = luxdata;
				}
				else
				{
					Con_Printf("\"%s\" isn't a version 1 deluxmap\n", luxname);
					luxdata=NULL;
				}
			}
			else
			{
				Con_Printf("lit \"%s\" isn't a deluxmap\n", luxname);
				luxdata=NULL;
			}
		}	
	}

	if (!mapcomeswith24bitcolouredlighting && r_loadlits.value)
	{
		qbyte *litdata = NULL;
		char *litname;
		char litnamemaps[MAX_QPATH];
		char litnamelits[MAX_QPATH];
		int depthmaps;
		int depthlits;
		
		{							
			strcpy(litnamemaps, loadmodel->name);
			COM_StripExtension(loadmodel->name, litnamemaps, sizeof(litnamemaps));
			COM_DefaultExtension(litnamemaps, ".lit", sizeof(litnamemaps));
			depthmaps = COM_FDepthFile(litnamemaps, false); 
		}
		{
			strcpy(litnamelits, "lits/");
			COM_StripExtension(COM_SkipPath(loadmodel->name), litnamelits+5, sizeof(litnamelits) - 5);
			strcat(litnamelits, ".lit");
			depthlits = COM_FDepthFile(litnamelits, false);
		}

		if (depthmaps <= depthlits)
			litname = litnamemaps;	//maps has priority over lits
		else
		{
			litname = litnamelits;
		}

		litdata = COM_LoadHunkFile(litname);
		COM_StripExtension(COM_SkipPath(loadmodel->name), litname+5, sizeof(litname)-5);
		strcat(litname, ".lit");
		if (litdata && (litdata[0] == 'Q' && litdata[1] == 'L' && litdata[2] == 'I' && litdata[3] == 'T'))
		{
			if (LittleLong(*(int *)&litdata[4]) == 1 && l->filelen && l->filelen != (com_filesize-8)/3)
				Con_Printf("lit \"%s\" doesn't match level. Ignored.\n", litname);
			else if (LittleLong(*(int *)&litdata[4]) != 1)
				Con_Printf("lit \"%s\" isn't version 1.\n", litname);
			else
			{
				float prop;
				int i;
				qbyte *normal;

				//load it
				loadmodel->lightdata = litdata+8;
				loadmodel->engineflags |= MDLF_RGBLIGHTING;


				//now some cheat protection.

				normal = mod_base + l->fileofs;
				litdata = loadmodel->lightdata;

				for (i = 0; i < l->filelen; i++)	//force it to the same intensity. (or less, depending on how you see it...)
				{
#define m(a, b, c) (a>(b>c?b:c)?a:(b>c?b:c))
					prop = (float)m(litdata[0],  litdata[1], litdata[2]);

					if (!prop)
					{
						litdata[0] = lmgamma[*normal];
						litdata[1] = lmgamma[*normal];
						litdata[2] = lmgamma[*normal];
					}
					else
					{
						prop = lmgamma[*normal] / prop;
						litdata[0] *= prop;
						litdata[1] *= prop;
						litdata[2] *= prop;
					}

					normal++;
					litdata+=3;
				}
				//end anti-cheat
			}
		}
		else if (litdata)
			Con_Printf("lit \"%s\" isn't a lit\n", litname);
//		else
			//failed to find
	}
	if (mapcomeswith24bitcolouredlighting)
		loadmodel->engineflags |= MDLF_RGBLIGHTING;

#ifdef RUNTIMELIGHTING
	else if (r_loadlits.value == 2 && !lightmodel && (!(loadmodel->engineflags & MDLF_RGBLIGHTING) || (!luxdata && gl_bumpmappingpossible)))
	{
		qbyte *litdata = NULL;
		int i;
		qbyte *normal;
		writelitfile = !(loadmodel->engineflags & MDLF_RGBLIGHTING);
		loadmodel->engineflags |= MDLF_RGBLIGHTING;
		loadmodel->lightdata = Hunk_AllocName ( l->filelen*3+8, loadname);
		strcpy(loadmodel->lightdata, "QLIT");
		((int*)loadmodel->lightdata)[1] = LittleLong(1);
		loadmodel->lightdata += 8;

		litdata = loadmodel->lightdata;
		normal = mod_base + l->fileofs;
		for (i = 0; i < l->filelen; i++)
		{
			*litdata++ = lmgamma[*normal];
			*litdata++ = lmgamma[*normal];
			*litdata++ = lmgamma[*normal];
			normal++;
		}

		if (gl_bumpmappingpossible)
		{
			loadmodel->deluxdata = Hunk_AllocName ( l->filelen*3+8, loadname);
			strcpy(loadmodel->deluxdata, "QLIT");
			((int*)loadmodel->deluxdata)[1] = LittleLong(1);
			loadmodel->deluxdata += 8;
			litdata = loadmodel->deluxdata;
			{
				for (i = 0; i < l->filelen*3; i++)
					*litdata++ = 0.5f*255;
			}
		}

		numlightdata = l->filelen;
		lightmodel = loadmodel;
		relitsurface = 0;
		return;
	}
#endif

	if (loadmodel->lightdata)
	{
		if ((loadmodel->engineflags & MDLF_RGBLIGHTING) && r_lightmap_saturation.value != 1.0f)
		{
			// desaturate lightmap according to cvar
			SaturateR8G8B8(loadmodel->lightdata, l->filelen, r_lightmap_saturation.value);
		}

		return;
	}

	loadmodel->lightdata = Hunk_AllocName ( l->filelen, loadname);

	{
		int i;
		qbyte *in, *out;

		in = mod_base + l->fileofs;
		out = loadmodel->lightdata;
		for (i = 0; i < l->filelen; i++)
		{
			*out++ = lmgamma[*in++];
		}

		if ((loadmodel->engineflags & MDLF_RGBLIGHTING) && r_lightmap_saturation.value != 1.0f)
			SaturateR8G8B8(loadmodel->lightdata, l->filelen, r_lightmap_saturation.value);
	}
	//memcpy (loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
}

/*
=================
Mod_LoadVisibility
=================
*/
void GLMod_LoadVisibility (lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->visdata = NULL;
		return;
	}
	loadmodel->visdata = Hunk_AllocName ( l->filelen, loadname);	
	memcpy (loadmodel->visdata, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadEntities
=================
*/
void GLMod_LoadEntities (lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->entities = NULL;
		return;
	}
	loadmodel->entities = Hunk_AllocName ( l->filelen + 1, loadname);	
	memcpy (loadmodel->entities, mod_base + l->fileofs, l->filelen);
	loadmodel->entities[l->filelen] = 0;
}


/*
=================
Mod_LoadVertexes
=================
*/
qboolean GLMod_LoadVertexes (lump_t *l)
{
	dvertex_t	*in;
	mvertex_t	*out;
	int			i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n", loadmodel->name);
		return false;
	}
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->position[0] = LittleFloat (in->point[0]);
		out->position[1] = LittleFloat (in->point[1]);
		out->position[2] = LittleFloat (in->point[2]);
	}

	return true;
}

/*
=================
Mod_LoadSubmodels
=================
*/
static qboolean hexen2map;
qboolean GLMod_LoadSubmodels (lump_t *l)
{
	dq1model_t	*inq;
	dh2model_t	*inh;
	mmodel_t	*out;
	int			i, j, count;

	//this is crazy!

	inq = (void *)(mod_base + l->fileofs);
	inh = (void *)(mod_base + l->fileofs);
	if (!inq->numfaces)
	{
		hexen2map = true;
		if (l->filelen % sizeof(*inh))
		{
			Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
			return false;
		}
		count = l->filelen / sizeof(*inh);
		out = Hunk_AllocName ( count*sizeof(*out), loadname);	

		loadmodel->submodels = out;
		loadmodel->numsubmodels = count;

		for ( i=0 ; i<count ; i++, inh++, out++)
		{
			for (j=0 ; j<3 ; j++)
			{	// spread the mins / maxs by a pixel
				out->mins[j] = LittleFloat (inh->mins[j]) - 1;
				out->maxs[j] = LittleFloat (inh->maxs[j]) + 1;
				out->origin[j] = LittleFloat (inh->origin[j]);
			}
			for (j=0 ; j<MAX_MAP_HULLSDH2 ; j++)
			{
				out->headnode[j] = LittleLong (inh->headnode[j]);
			}
			for ( ; j<MAX_MAP_HULLSM ; j++)
				out->headnode[j] = 0;
			for (j=0 ; j<MAX_MAP_HULLSDH2 ; j++)
				out->hullavailable[j] = true;
			for ( ; j<MAX_MAP_HULLSM ; j++)
				out->hullavailable[j] = false;
			out->visleafs = LittleLong (inh->visleafs);
			out->firstface = LittleLong (inh->firstface);
			out->numfaces = LittleLong (inh->numfaces);
		}

	}
	else
	{
		hexen2map = false;
		if (l->filelen % sizeof(*inq))
		{
			Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
			return false;
		}
		count = l->filelen / sizeof(*inq);
		out = Hunk_AllocName ( count*sizeof(*out), loadname);	

		loadmodel->submodels = out;
		loadmodel->numsubmodels = count;

		for ( i=0 ; i<count ; i++, inq++, out++)
		{
			for (j=0 ; j<3 ; j++)
			{	// spread the mins / maxs by a pixel
				out->mins[j] = LittleFloat (inq->mins[j]) - 1;
				out->maxs[j] = LittleFloat (inq->maxs[j]) + 1;
				out->origin[j] = LittleFloat (inq->origin[j]);
			}
			for (j=0 ; j<MAX_MAP_HULLSDQ1 ; j++)
			{
				out->headnode[j] = LittleLong (inq->headnode[j]);
			}
			for ( ; j<MAX_MAP_HULLSM ; j++)
				out->headnode[j] = 0;
			for (j=0 ; j<3 ; j++)
				out->hullavailable[j] = true;
			for ( ; j<MAX_MAP_HULLSM ; j++)
				out->hullavailable[j] = false;
			out->visleafs = LittleLong (inq->visleafs);
			out->firstface = LittleLong (inq->firstface);
			out->numfaces = LittleLong (inq->numfaces);
		}
	}

	return true;
}

/*
=================
Mod_LoadEdges
=================
*/
qboolean GLMod_LoadEdges (lump_t *l)
{
	dedge_t *in;
	medge_t *out;
	int 	i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf ("MOD_LoadBmodel: funny lump size in %s\n", loadmodel->name);
		return false;
	}
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( (count + 1) * sizeof(*out), loadname);	

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->v[0] = (unsigned short)LittleShort(in->v[0]);
		out->v[1] = (unsigned short)LittleShort(in->v[1]);
	}

	return true;
}

/*
=================
Mod_LoadTexinfo
=================
*/
qboolean GLMod_LoadTexinfo (lump_t *l)
{
	texinfo_t *in;
	mtexinfo_t *out;
	int 	i, j, count;
	int		miptex;
	float	len1, len2;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
		return false;
	}
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<8 ; j++)
			out->vecs[0][j] = LittleFloat (in->vecs[0][j]);
		len1 = Length (out->vecs[0]);
		len2 = Length (out->vecs[1]);
		len1 = (len1 + len2)/2;
		if (len1 < 0.32)
			out->mipadjust = 4;
		else if (len1 < 0.49)
			out->mipadjust = 3;
		else if (len1 < 0.99)
			out->mipadjust = 2;
		else
			out->mipadjust = 1;
#if 0
		if (len1 + len2 < 0.001)
			out->mipadjust = 1;		// don't crash
		else
			out->mipadjust = 1 / floor( (len1+len2)/2 + 0.1 );
#endif

		miptex = LittleLong (in->miptex);
		out->flags = LittleLong (in->flags);
	
		if (!loadmodel->textures || miptex < 0 || miptex >= loadmodel->numtextures)
		{
			out->texture = r_notexture_mip;	// checkerboard texture
			out->flags = 0;
		}
		else
		{
			out->texture = loadmodel->textures[miptex];
			if (!out->texture)
			{
				out->texture = r_notexture_mip; // texture not found
				out->flags = 0;
			}
		}
	}

	return true;
}

/*
================
CalcSurfaceExtents

Fills in s->texturemins[] and s->extents[]
================
*/

void CalcSurfaceExtents (msurface_t *s);
/*
{
	float	mins[2], maxs[2], val;
	int		i,j, e;
	mvertex_t	*v;
	mtexinfo_t	*tex;
	int		bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;
	
	for (i=0 ; i<s->numedges ; i++)
	{
		e = loadmodel->surfedges[s->firstedge+i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];
		
		for (j=0 ; j<2 ; j++)
		{
			val = v->position[0] * tex->vecs[j][0] + 
				v->position[1] * tex->vecs[j][1] +
				v->position[2] * tex->vecs[j][2] +
				tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{	
		bmins[i] = floor(mins[i]/16);
		bmaxs[i] = ceil(maxs[i]/16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;

//		if ( !(tex->flags & TEX_SPECIAL) && s->extents[i] > 512 )	//q2 uses 512.
//			Sys_Error ("Bad surface extents");
	}
}
*/

/*
=================
Mod_LoadFaces
=================
*/
qboolean GLMod_LoadFaces (lump_t *l)
{
	dface_t		*in;
	msurface_t 	*out;
	int			i, count, surfnum;
	int			planenum, side;
	int tn;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
		return false;
	}
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;
	for ( surfnum=0 ; surfnum<count ; surfnum++, in++, out++)
	{
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);		
		out->flags = 0;

		planenum = LittleShort(in->planenum);
		side = LittleShort(in->side);
		if (side)
			out->flags |= SURF_PLANEBACK;			

		out->plane = loadmodel->planes + planenum;

		tn = LittleShort (in->texinfo);
		if (tn < 0 || tn >= loadmodel->numtexinfo)
			Host_EndGame("Hey! That map has texinfos out of bounds!\n");
		out->texinfo = loadmodel->texinfo + tn;

		CalcSurfaceExtents (out);
				
	// lighting info

		for (i=0 ; i<MAXLIGHTMAPS ; i++)
			out->styles[i] = in->styles[i];
		i = LittleLong(in->lightofs);
		if (i == -1)
			out->samples = NULL;
		else if ((loadmodel->engineflags & MDLF_RGBLIGHTING) && loadmodel->fromgame != fg_halflife)
			out->samples = loadmodel->lightdata + i*3;
		else
			out->samples = loadmodel->lightdata + i;

		if (!out->texinfo->texture)
			continue;

		
	// set the drawing flags flag		
		if (!Q_strncmp(out->texinfo->texture->name,"sky",3))	// sky
		{
			out->flags |= (SURF_DRAWSKY | SURF_DRAWTILED);
			continue;
		}
		
		if (!Q_strncmp(out->texinfo->texture->name,"*",1))		// turbulent
		{
			out->flags |= (SURF_DRAWTURB | SURF_DRAWTILED);
			for (i=0 ; i<2 ; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			continue;
		}

		if (!Q_strncmp(out->texinfo->texture->name,"{",1))		// alpha
		{
			out->flags |= (SURF_DRAWALPHA);
			continue;
		}
		if (!Q_strncmp(out->texinfo->texture->name,"glass",5))		// alpha
		{
			out->flags |= (SURF_DRAWALPHA);
			continue;
		}
		if (out->flags & SURF_DRAWALPHA)
			out->flags &= ~SURF_DRAWALPHA;
	}

	return true;
}


/*
=================
Mod_SetParent
=================
*/
void GLMod_SetParent (mnode_t *node, mnode_t *parent)
{
	if (!node)
		return;
	node->parent = parent;
	if (node->contents < 0)
		return;
	GLMod_SetParent (node->children[0], node);
	GLMod_SetParent (node->children[1], node);
}

/*
=================
Mod_LoadNodes
=================
*/
qboolean GLMod_LoadNodes (lump_t *l)
{
	int			i, j, count, p;
	dnode_t		*in;
	mnode_t 	*out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
		return false;
	}
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}
	
		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleShort (in->firstface);
		out->numsurfaces = LittleShort (in->numfaces);
		
		for (j=0 ; j<2 ; j++)
		{
			p = LittleShort (in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = (mnode_t *)(loadmodel->leafs + (-1 - p));
		}
	}
	
	GLMod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs
	return true;
}

/*
=================
Mod_LoadLeafs
=================
*/
qboolean GLMod_LoadLeafs (lump_t *l)
{
	dleaf_t 	*in;
	mleaf_t 	*out;
	int			i, j, count, p;
//	char s[80];

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
		return false;
	}
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		out->firstmarksurface = loadmodel->marksurfaces +
			LittleShort(in->firstmarksurface);
		out->nummarksurfaces = LittleShort(in->nummarksurfaces);
		
		p = LittleLong(in->visofs);
		if (p == -1)
			out->compressed_vis = NULL;
		else
			out->compressed_vis = loadmodel->visdata + p;
		out->efrags = NULL;
		
		for (j=0 ; j<4 ; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];

#ifndef CLIENTONLY
		if (!isDedicated)
#endif
		{
			// gl underwater warp
			if (out->contents != Q1CONTENTS_EMPTY)
			{
				for (j=0 ; j<out->nummarksurfaces ; j++)
					out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
			}
			if (isnotmap)
			{
				for (j=0 ; j<out->nummarksurfaces ; j++)
					out->firstmarksurface[j]->flags |= SURF_DONTWARP;
			}
		}
	}	

	return true;
}




//these are used to boost other info sizes
int numsuplementryplanes;
int numsuplementryclipnodes;
void *suplementryclipnodes;
void *suplementryplanes;
void *crouchhullfile;

qbyte *COM_LoadMallocFile (char *path);
void GLMod_LoadCrouchHull(void)
{
	int i, h;
	int numsm;
	char crouchhullname[MAX_QPATH];
	int *data;
	int hulls;

//	dclipnode_t *cn;

	memset(loadmodel->hulls, 0, sizeof(loadmodel->hulls));	//ensure all the sizes are 0 (this is how we check for the existance of a hull

	numsuplementryplanes = numsuplementryclipnodes = 0;

	//find a name for a ccn and try to load it.
	strcpy(crouchhullname, loadmodel->name);
	COM_StripExtension(loadmodel->name, crouchhullname, sizeof(crouchhullname));
	COM_DefaultExtension(crouchhullname, ".crh",sizeof(crouchhullname));	//crouch hull

	crouchhullfile = COM_LoadMallocFile(crouchhullname);	//or otherwise temporary storage. load on hunk if you want, but that would be a waste.
	if (!crouchhullfile)
		return;

	data = crouchhullfile;

	if (LittleLong(*data++) != ('S') + ('C'<<8) + ('N'<<16) + ('P'<<24))	//make sure it's the right version
		return;

	if (LittleLong(*data) == 2)
	{
		data++;
		hulls = LittleLong(*data++);
	}
	else
		return;

	if (hulls > MAX_MAP_HULLSM - MAX_MAP_HULLSDQ1)
	{
		return;
	}

	numsm = LittleLong(*data++);
	if (numsm != loadmodel->numsubmodels)	//not compatible
		return;

	numsuplementryplanes = LittleLong(*data++);
	numsuplementryclipnodes = LittleLong(*data++);

	for (h = 0; h < hulls; h++)
	{
		for (i = 0; i < 3; i++)
			loadmodel->hulls[3+h].clip_mins[i] = LittleLong(*data++);
		for (i = 0; i < 3; i++)
			loadmodel->hulls[3+h].clip_maxs[i] = LittleLong(*data++);

		for (i = 0; i < numsm; i++)	//load headnode references
		{
			loadmodel->submodels[i].headnode[3+h] = LittleLong(*data)+1;
			data++;
		}
	}

	suplementryplanes = data;
	suplementryclipnodes = (qbyte*)data + sizeof(dplane_t)*numsuplementryplanes;
}

/*
=================
Mod_LoadClipnodes
=================
*/
qboolean GLMod_LoadClipnodes (lump_t *l)
{
	dclipnode_t *in, *out;
	int			i, count;
	hull_t		*hull;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
		return false;
	}
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( (count+numsuplementryclipnodes)*sizeof(*out), loadname);//space for both

	loadmodel->clipnodes = out;
	loadmodel->numclipnodes = count+numsuplementryclipnodes;


	if (hexen2map)
	{	//hexen2.
		hexen2map=false;
		hull = &loadmodel->hulls[1];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->planes;
		hull->clip_mins[0] = -16;
		hull->clip_mins[1] = -16;
		hull->clip_mins[2] = -24;
		hull->clip_maxs[0] = 16;
		hull->clip_maxs[1] = 16;
		hull->clip_maxs[2] = 32;
		hull->available = true;

		hull = &loadmodel->hulls[2];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->planes;
		hull->clip_mins[0] = -24;
		hull->clip_mins[1] = -24;
		hull->clip_mins[2] = -20;
		hull->clip_maxs[0] = 24;
		hull->clip_maxs[1] = 24;
		hull->clip_maxs[2] = 20;
		hull->available = true;

		hull = &loadmodel->hulls[3];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->planes;
		hull->clip_mins[0] = -16;
		hull->clip_mins[1] = -16;
		hull->clip_mins[2] = -12;
		hull->clip_maxs[0] = 16;
		hull->clip_maxs[1] = 16;
		hull->clip_maxs[2] = 16;
		hull->available = true;

		hull = &loadmodel->hulls[4];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->planes;
		hull->clip_mins[0] = -40;
		hull->clip_mins[1] = -40;
		hull->clip_mins[2] = -42;
		hull->clip_maxs[0] = 40;
		hull->clip_maxs[1] = 40;
		hull->clip_maxs[2] = 42;
		hull->available = true;

		hull = &loadmodel->hulls[5];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->planes;
		hull->clip_mins[0] = -48;
		hull->clip_mins[1] = -48;
		hull->clip_mins[2] = -50 - 24;
		hull->clip_maxs[0] = 48;
		hull->clip_maxs[1] = 48;
		hull->clip_maxs[2] = 50;
		hull->available = true;
	}
	else if (loadmodel->fromgame == fg_halflife)
	{
		hull = &loadmodel->hulls[1];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->planes;
		hull->clip_mins[0] = -16;
		hull->clip_mins[1] = -16;
		hull->clip_mins[2] = -36;
		hull->clip_maxs[0] = 16;
		hull->clip_maxs[1] = 16;
		hull->clip_maxs[2] = 36;
		hull->available = true;

		hull = &loadmodel->hulls[2];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->planes;
		hull->clip_mins[0] = -32;
		hull->clip_mins[1] = -32;
		hull->clip_mins[2] = -32;
		hull->clip_maxs[0] = 32;
		hull->clip_maxs[1] = 32;
		hull->clip_maxs[2] = 32;
		hull->available = true;

		hull = &loadmodel->hulls[3];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->planes;
		hull->clip_mins[0] = -16;
		hull->clip_mins[1] = -16;
		hull->clip_mins[2] = -18;
		hull->clip_maxs[0] = 16;
		hull->clip_maxs[1] = 16;
		hull->clip_maxs[2] = 18;
		hull->available = true;
	}
	else
	{
		hull = &loadmodel->hulls[1];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->planes;
		hull->clip_mins[0] = -16;
		hull->clip_mins[1] = -16;
		hull->clip_mins[2] = -24;
		hull->clip_maxs[0] = 16;
		hull->clip_maxs[1] = 16;
		hull->clip_maxs[2] = 32;
		hull->available = true;

		hull = &loadmodel->hulls[2];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->planes;
		hull->clip_mins[0] = -32;
		hull->clip_mins[1] = -32;
		hull->clip_mins[2] = -24;
		hull->clip_maxs[0] = 32;
		hull->clip_maxs[1] = 32;
		hull->clip_maxs[2] = 64;
		hull->available = true;

		hull = &loadmodel->hulls[3];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->planes;
		hull->clip_mins[0] = -16;
		hull->clip_mins[1] = -16;
		hull->clip_mins[2] = -6;
		hull->clip_maxs[0] = 16;
		hull->clip_maxs[1] = 16;
		hull->clip_maxs[2] = 30;
		hull->available = false;
	}

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->planenum = LittleLong(in->planenum);
		out->children[0] = LittleShort(in->children[0]);
		out->children[1] = LittleShort(in->children[1]);
	}

	if (numsuplementryclipnodes)	//now load the crouch ones.
	{
		for (i = 3; i < MAX_MAP_HULLSM; i++)
		{
			hull = &loadmodel->hulls[i];
			hull->planes = suplementryplanes;
			hull->clipnodes = out-1;
			hull->firstclipnode = 0;
			hull->lastclipnode = numsuplementryclipnodes;
			hull->available = true;
		}

		in = suplementryclipnodes;

		for (i=0 ; i<numsuplementryclipnodes ; i++, out++, in++)
		{
			out->planenum = LittleLong(in->planenum);
			out->children[0] = LittleShort(in->children[0]);
			out->children[0] += out->children[0]>=0?1:0;
			out->children[1] = LittleShort(in->children[1]);
			out->children[1] += out->children[1]>=0?1:0;
		}
	}

	return true;
}

/*
=================
Mod_MakeHull0

Deplicate the drawing hull structure as a clipping hull
=================
*/
void GLMod_MakeHull0 (void)
{
	mnode_t		*in, *child;
	dclipnode_t *out;
	int			i, j, count;
	hull_t		*hull;

	hull = &loadmodel->hulls[0];	

	in = loadmodel->nodes;
	count = loadmodel->numnodes;
	out = Hunk_AllocName ( count*sizeof(*out), loadname);

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count-1;
	hull->planes = loadmodel->planes;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->planenum = in->plane - loadmodel->planes;
		for (j=0 ; j<2 ; j++)
		{
			child = in->children[j];
			if (child->contents < 0)
				out->children[j] = child->contents;
			else
				out->children[j] = child - loadmodel->nodes;
		}
	}
}

/*
=================
Mod_LoadMarksurfaces
=================
*/
qboolean GLMod_LoadMarksurfaces (lump_t *l)
{	
	int		i, j, count;
	short		*in;
	msurface_t **out;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
		return false;
	}
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for ( i=0 ; i<count ; i++)
	{
		j = LittleShort(in[i]);
		if (j < 0 || j >= loadmodel->numsurfaces)
		{
			Con_Printf (CON_ERROR "Mod_ParseMarksurfaces: bad surface number\n");
			return false;
		}
		out[i] = loadmodel->surfaces + j;
	}

	return true;
}

/*
=================
Mod_LoadSurfedges
=================
*/
qboolean GLMod_LoadSurfedges (lump_t *l)
{	
	int		i, count;
	int		*in, *out;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
		return false;
	}
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for ( i=0 ; i<count ; i++)
		out[i] = LittleLong (in[i]);

	return true;
}


/*
=================
Mod_LoadPlanes
=================
*/
qboolean GLMod_LoadPlanes (lump_t *l)
{
	int			i, j;
	mplane_t	*out;
	dplane_t 	*in;
	int			count;
	int			bits;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
		return false;
	}
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( (count+numsuplementryplanes)*2*sizeof(*out), loadname);	
	
	loadmodel->planes = out;
	loadmodel->numplanes = count+numsuplementryplanes;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		bits = 0;
		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1<<j;
		}

		out->dist = LittleFloat (in->dist);
		out->type = LittleLong (in->type);
		out->signbits = bits;
	}

	if (numsuplementryplanes)
	{
		in = suplementryplanes;
		suplementryplanes = out;
		for ( i=0 ; i<numsuplementryplanes ; i++, in++, out++)
		{
			bits = 0;
			for (j=0 ; j<3 ; j++)
			{
				out->normal[j] = LittleFloat (in->normal[j]);
				if (out->normal[j] < 0)
					bits |= 1<<j;
			}

			out->dist = LittleFloat (in->dist);
			out->type = LittleLong (in->type);
			out->signbits = bits;
		}
	}

	return true;
}

/*
=================
RadiusFromBounds
=================
*/

float RadiusFromBounds (vec3_t mins, vec3_t maxs);
/*
{
	int		i;
	vec3_t	corner;

	for (i=0 ; i<3 ; i++)
	{
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);
	}

	return Length (corner);
}
*/

//combination of R_AddDynamicLights and R_MarkLights
void GLR_StainSurf (msurface_t *surf, float *parms);
static void Q1BSP_StainNode (mnode_t *node, float *parms)
{
#ifdef RGLQUAKE
	mplane_t	*splitplane;
	float		dist;
	msurface_t	*surf;
	int			i;
	
	if (node->contents < 0)
		return;	

	splitplane = node->plane;
	dist = DotProduct ((parms+1), splitplane->normal) - splitplane->dist;
	
	if (dist > (*parms))
	{
		Q1BSP_StainNode (node->children[0], parms);
		return;
	}
	if (dist < (-*parms))
	{
		Q1BSP_StainNode (node->children[1], parms);
		return;
	}

// mark the polygons
	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->flags&~(SURF_DONTWARP|SURF_PLANEBACK))
			continue;
		GLR_StainSurf(surf, parms);
	}

	Q1BSP_StainNode (node->children[0], parms);
	Q1BSP_StainNode (node->children[1], parms);
#endif
}


void Q1BSP_MarkLights (dlight_t *light, int bit, mnode_t *node);
qboolean Q1BSP_Trace(model_t *model, int forcehullnum, int frame, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, trace_t *trace);
void GLQ1BSP_LightPointValues(model_t *model, vec3_t point, vec3_t res_diffuse, vec3_t res_ambient, vec3_t res_dir);


void GLMod_FixupNodeMinsMaxs (mnode_t *node, mnode_t *parent)
{
	if (!node)
		return;

	if (node->contents >= 0)
	{
		GLMod_FixupNodeMinsMaxs (node->children[0], node);
		GLMod_FixupNodeMinsMaxs (node->children[1], node);
	}

	if (parent)
	{
		if (parent->minmaxs[0] > node->minmaxs[0])
			parent->minmaxs[0] = node->minmaxs[0];
		if (parent->minmaxs[1] > node->minmaxs[1])
			parent->minmaxs[1] = node->minmaxs[1];
		if (parent->minmaxs[2] > node->minmaxs[2])
			parent->minmaxs[2] = node->minmaxs[2];

		if (parent->minmaxs[3] < node->minmaxs[3])
			parent->minmaxs[3] = node->minmaxs[3];
		if (parent->minmaxs[4] < node->minmaxs[4])
			parent->minmaxs[4] = node->minmaxs[4];
		if (parent->minmaxs[5] < node->minmaxs[5])
			parent->minmaxs[5] = node->minmaxs[5];
	}

}
void GLMod_FixupMinsMaxs(void)
{
	//q1 bsps are capped to +/- 32767 by the nodes/leafs
	//verts arn't though
	//so if the map is too big, let's figure out what they should be
	float *v;
	msurface_t **mark, *surf;
	mleaf_t *pleaf;
	medge_t *e, *pedges;
	int en, lindex;
	int i, c, lnumverts;
	qboolean needsfixup = false;

	if (loadmodel->mins[0] < -32768)
		needsfixup = true;
	if (loadmodel->mins[1] < -32768)
		needsfixup = true;
	if (loadmodel->mins[2] < -32768)
		needsfixup = true;

	if (loadmodel->maxs[0] > 32767)
		needsfixup = true;
	if (loadmodel->maxs[1] > 32767)
		needsfixup = true;
	if (loadmodel->maxs[2] > 32767)
		needsfixup = true;

	if (!needsfixup)
		return;

	//this is insane.
	//why am I writing this?
	//by the time the world actually gets this large, the floating point errors are going to be so immensly crazy that it's just not worth it.

	pedges = loadmodel->edges;

	for (i = 0; i < loadmodel->numleafs; i++)
	{
		pleaf = &loadmodel->leafs[i];

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c)
		{
			do
			{
				surf = (*mark++);

				lnumverts = surf->numedges;
				for (en=0 ; en<lnumverts ; en++)
				{
					lindex = currentmodel->surfedges[surf->firstedge + en];

					if (lindex > 0)
					{
						e = &pedges[lindex];
						v = currentmodel->vertexes[e->v[0]].position;
					}
					else
					{
						e = &pedges[-lindex];
						v = currentmodel->vertexes[e->v[1]].position;
					}

					if (pleaf->minmaxs[0] > v[0])
						pleaf->minmaxs[0] = v[0];
					if (pleaf->minmaxs[1] > v[1])
						pleaf->minmaxs[1] = v[1];
					if (pleaf->minmaxs[2] > v[2])
						pleaf->minmaxs[2] = v[2];

					if (pleaf->minmaxs[3] < v[0])
						pleaf->minmaxs[3] = v[0];
					if (pleaf->minmaxs[4] < v[1])
						pleaf->minmaxs[4] = v[1];
					if (pleaf->minmaxs[5] < v[2])
						pleaf->minmaxs[5] = v[2];

				}
			} while (--c);
		}
	}
	GLMod_FixupNodeMinsMaxs (loadmodel->nodes, NULL);	// sets nodes and leafs
}

/*
=================
Mod_LoadBrushModel
=================
*/
qboolean GLMod_LoadBrushModel (model_t *mod, void *buffer)
{
	int			i, j;
	dheader_t	*header;
	mmodel_t 	*bm;
	model_t *lm=mod;
	unsigned int chksum;
	int start;
	qboolean noerrors;
	
	start = Hunk_LowMark();

	loadmodel->type = mod_brush;
	
	header = (dheader_t *)buffer;

	if ((!cl.worldmodel && cls.state>=ca_connected)
#ifndef CLIENTONLY
		|| (!sv.worldmodel && sv.active)
#endif
		)
		isnotmap = false;
	else
		isnotmap = true;

	i = LittleLong (header->version);

	if (i == BSPVERSION || i == BSPVERSIONPREREL)
	{
		loadmodel->fromgame = fg_quake;
		loadmodel->engineflags |= MDLF_NEEDOVERBRIGHT;
	}
	else if (i == BSPVERSIONHL)	//halflife support
		loadmodel->fromgame = fg_halflife;
	else
	{
		Con_Printf (CON_ERROR "Mod_LoadBrushModel: %s has wrong version number (%i should be %i)\n", mod->name, i, BSPVERSION);
		return false;
	}

// swap all the lumps
	mod_base = (qbyte *)header;

	for (i=0 ; i<sizeof(dheader_t)/4 ; i++)
		((int *)header)[i] = LittleLong ( ((int *)header)[i]);


// checksum all of the map, except for entities
	mod->checksum = 0;
	mod->checksum2 = 0;

	for (i = 0; i < HEADER_LUMPS; i++)
	{
		if ((unsigned)header->lumps[i].fileofs + (unsigned)header->lumps[i].filelen > com_filesize)
		{
			Con_Printf (CON_ERROR "Mod_LoadBrushModel: %s appears truncated\n", mod->name);
			return false;
		}
		if (i == LUMP_ENTITIES)
			continue;
		chksum = Com_BlockChecksum(mod_base + header->lumps[i].fileofs, header->lumps[i].filelen);
		mod->checksum ^= chksum;

		if (i == LUMP_VISIBILITY || i == LUMP_LEAFS || i == LUMP_NODES)
			continue;
		mod->checksum2 ^= chksum;
	}

	if (1)//mod_ebfs.value)
	{
		char *id;
		id = (char *)(header + 1);
		if (id[0]=='P' && id[1]=='A' && id[2]=='C' && id[3]=='K')
		{	//EBFS detected.
			COM_LoadMapPackFile(mod->name, sizeof(dheader_t));
		}
	}
		
	noerrors = true;

	crouchhullfile = NULL;

// load into heap
#ifndef CLIENTONLY
	if (!isDedicated)
#endif
	{
		noerrors = noerrors && GLMod_LoadVertexes (&header->lumps[LUMP_VERTEXES]);
		noerrors = noerrors && GLMod_LoadEdges (&header->lumps[LUMP_EDGES]);
		noerrors = noerrors && GLMod_LoadSurfedges (&header->lumps[LUMP_SURFEDGES]);
		noerrors = noerrors && GLMod_LoadTextures (&header->lumps[LUMP_TEXTURES]);
		if (noerrors)
			GLMod_LoadLighting (&header->lumps[LUMP_LIGHTING]);	
	}
	noerrors = noerrors && GLMod_LoadSubmodels (&header->lumps[LUMP_MODELS]);
	if (noerrors)
		GLMod_LoadCrouchHull();
	noerrors = noerrors && GLMod_LoadPlanes (&header->lumps[LUMP_PLANES]);
#ifndef CLIENTONLY
	if (!isDedicated)
#endif
	{
		noerrors = noerrors && GLMod_LoadTexinfo (&header->lumps[LUMP_TEXINFO]);
		noerrors = noerrors && GLMod_LoadFaces (&header->lumps[LUMP_FACES]);
		noerrors = noerrors && GLMod_LoadMarksurfaces (&header->lumps[LUMP_MARKSURFACES]);
	}	
	if (noerrors)
		GLMod_LoadVisibility (&header->lumps[LUMP_VISIBILITY]);
	noerrors = noerrors && GLMod_LoadLeafs (&header->lumps[LUMP_LEAFS]);
	noerrors = noerrors && GLMod_LoadNodes (&header->lumps[LUMP_NODES]);
	noerrors = noerrors && GLMod_LoadClipnodes (&header->lumps[LUMP_CLIPNODES]);
	if (noerrors)
	{
		GLMod_LoadEntities (&header->lumps[LUMP_ENTITIES]);
		GLMod_MakeHull0 ();
	}

	if (crouchhullfile)
	{
		BZ_Free(crouchhullfile);
		crouchhullfile=NULL;
	}

	if (!noerrors)
	{
		Hunk_FreeToLowMark(start);
		return false;
	}

#ifndef CLIENTONLY
	if (sv.state)	//if the server is running
	{
		if (!strcmp(loadmodel->name, va("maps/%s.bsp", sv.name)))
			Mod_ParseInfoFromEntityLump(mod_base + header->lumps[LUMP_ENTITIES].fileofs);
	}
	else
#endif
	{
		if (!cl.model_precache[1])	//not copied across yet
			Mod_ParseInfoFromEntityLump(mod_base + header->lumps[LUMP_ENTITIES].fileofs);
	}

	Q1BSP_SetModelFuncs(mod);
	mod->funcs.LightPointValues		= GLQ1BSP_LightPointValues;
	mod->funcs.StainNode			= Q1BSP_StainNode;
	mod->funcs.MarkLights			= Q1BSP_MarkLights;

	mod->numframes = 2;		// regular and alternate animation
	
//
// set up the submodels (FIXME: this is confusing)
//
	for (i=0 ; i<mod->numsubmodels ; i++)
	{
		bm = &mod->submodels[i];

		mod->hulls[0].firstclipnode = bm->headnode[0];
		mod->hulls[0].available = true;
		Q1BSP_SetHullFuncs(&mod->hulls[0]);

		for (j=1 ; j<MAX_MAP_HULLSM ; j++)
		{
			mod->hulls[j].firstclipnode = bm->headnode[j];
			mod->hulls[j].lastclipnode = mod->numclipnodes-1;

			mod->hulls[j].available = bm->hullavailable[j];
			if (mod->hulls[j].firstclipnode > mod->hulls[j].lastclipnode)
				mod->hulls[j].available = false;

			Q1BSP_SetHullFuncs(&mod->hulls[j]);
		}
		
		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;
		
		VectorCopy (bm->maxs, mod->maxs);
		VectorCopy (bm->mins, mod->mins);

		mod->radius = RadiusFromBounds (mod->mins, mod->maxs);

		mod->numleafs = bm->visleafs;

		if (i < mod->numsubmodels-1)
		{	// duplicate the basic information
			char	name[10];

			sprintf (name, "*%i", i+1);
			loadmodel = Mod_FindName (name);
			*loadmodel = *mod;
			strcpy (loadmodel->name, name);
			mod = loadmodel;

			P_DefaultTrail(mod);
		}
	}
#ifdef RUNTIMELIGHTING
	if (lightmodel == lm)
		LightLoadEntities(lightmodel->entities);
#endif

	if (1)
		GLMod_FixupMinsMaxs();

	return true;
}

/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

//aliashdr_t	*pheader;

//mstvert_t	stverts[MAXALIASVERTS*2];
//mtriangle_t	triangles[MAXALIASTRIS];

// a pose is a single set of vertexes.  a frame may be
// an animating sequence of poses
//dtrivertx_t	*poseverts[MAXALIASFRAMES];
//int			posenum;

qbyte		*player_8bit_texels/*[320*200]*/;


//=========================================================

/*
=================
Mod_FloodFillSkin

Fill background pixels so mipmapping doesn't have haloes - Ed
=================
*/

typedef struct
{
	short		x, y;
} floodfill_t;

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) fdc = pos[off]; \
}

void GLMod_FloodFillSkin( qbyte *skin, int skinwidth, int skinheight )
{
	qbyte				fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t			fifo[FLOODFILL_FIFO_SIZE];
	int					inpt = 0, outpt = 0;
	int					filledcolor = -1;
	int					i;

	if (filledcolor == -1)
	{
		filledcolor = 0;
		// attempt to find opaque black
		for (i = 0; i < 256; ++i)
			if (d_8to24rgbtable[i] == (255 << 0)) // alpha 1.0
			{
				filledcolor = i;
				break;
			}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255))
	{
		//printf( "not filling skin from %d to %d\n", fillcolor, filledcolor );
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		int			x = fifo[outpt].x, y = fifo[outpt].y;
		int			fdc = filledcolor;
		qbyte		*pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)				FLOODFILL_STEP( -1, -1, 0 );
		if (x < skinwidth - 1)	FLOODFILL_STEP( 1, 1, 0 );
		if (y > 0)				FLOODFILL_STEP( -skinwidth, 0, -1 );
		if (y < skinheight - 1)	FLOODFILL_STEP( skinwidth, 0, 1 );
		skin[x + skinwidth * y] = fdc;
	}
}

//=============================================================================

/*
=================
Mod_LoadSpriteFrame
=================
*/
void * GLMod_LoadSpriteFrame (void * pin, mspriteframe_t **ppframe, int framenum, int version, unsigned char *palette)
{
	dspriteframe_t		*pinframe;
	mspriteframe_t		*pspriteframe;
	int					width, height, size, origin[2];
	char				name[64];

	pinframe = (dspriteframe_t *)pin;

	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	size = width * height;

	pspriteframe = Hunk_AllocName (sizeof (mspriteframe_t),loadname);

	Q_memset (pspriteframe, 0, sizeof (mspriteframe_t));

	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong (pinframe->origin[0]);
	origin[1] = LittleLong (pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];

	pspriteframe->gl_texturenum = 0;

	if (!pspriteframe->gl_texturenum)
	{	//the dp way
		strcat(name, va("_%i", framenum));
		pspriteframe->gl_texturenum = Mod_LoadReplacementTexture(name, "sprites", true, true, true);
	}
	if (!pspriteframe->gl_texturenum)
	{	//the older fte way.
		COM_StripExtension(loadmodel->name, name, sizeof(name));
		strcat(name, va("_%i", framenum));
		pspriteframe->gl_texturenum = Mod_LoadReplacementTexture(name, "sprites", true, true, true);
	}
	if (!pspriteframe->gl_texturenum)
	{	//the fuhquake way
		COM_StripExtension(COM_SkipPath(loadmodel->name), name, sizeof(name));
		strcat(name, va("_%i", framenum));
		pspriteframe->gl_texturenum = Mod_LoadReplacementTexture(name, "sprites", true, true, true);
	}

	if (version == SPRITE32_VERSION)
	{
		size *= 4;
		if (!pspriteframe->gl_texturenum)
			pspriteframe->gl_texturenum = R_LoadTexture32 (name, width, height, (unsigned *)(pinframe + 1), true, true);
	}
	else if (version == SPRITEHL_VERSION)
	{
		if (!pspriteframe->gl_texturenum)
			pspriteframe->gl_texturenum = R_LoadTexture8Pal32 (name, width, height, (qbyte *)(pinframe + 1), (qbyte*)palette, true, true);
	}
	else
	{
		if (!pspriteframe->gl_texturenum)
			pspriteframe->gl_texturenum = R_LoadTexture8 (name, width, height, (qbyte *)(pinframe + 1), true, true);
	}

	return (void *)((qbyte *)(pinframe+1) + size);
}


/*
=================
Mod_LoadSpriteGroup
=================
*/
void * GLMod_LoadSpriteGroup (void * pin, mspriteframe_t **ppframe, int framenum, int version, unsigned char *palette)
{
	dspritegroup_t		*pingroup;
	mspritegroup_t		*pspritegroup;
	int					i, numframes;
	dspriteinterval_t	*pin_intervals;
	float				*poutintervals;
	void				*ptemp;

	pingroup = (dspritegroup_t *)pin;

	numframes = LittleLong (pingroup->numframes);

	pspritegroup = Hunk_AllocName (sizeof (mspritegroup_t) +
				(numframes - 1) * sizeof (pspritegroup->frames[0]), loadname);

	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *)pspritegroup;

	pin_intervals = (dspriteinterval_t *)(pingroup + 1);

	poutintervals = Hunk_AllocName (numframes * sizeof (float), loadname);

	pspritegroup->intervals = poutintervals;

	for (i=0 ; i<numframes ; i++)
	{
		*poutintervals = LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0)
		{
			Con_Printf (CON_ERROR "Mod_LoadSpriteGroup: interval<=0\n");
			return NULL;
		}

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *)pin_intervals;

	for (i=0 ; i<numframes ; i++)
	{
		ptemp = GLMod_LoadSpriteFrame (ptemp, &pspritegroup->frames[i], framenum * 100 + i, version, palette);
	}

	return ptemp;
}

/*
=================
Mod_LoadSpriteModel
=================
*/
qboolean GLMod_LoadSpriteModel (model_t *mod, void *buffer)
{
	int					i;
	int					version;
	dsprite_t			*pin;
	msprite_t			*psprite;
	int					numframes;
	int					size;
	dspriteframetype_t	*pframetype;
	int rendertype=0;
	unsigned char pal[256*4];
	int sptype;
	int hunkstart;
	
	hunkstart = Hunk_LowMark();
	pin = (dsprite_t *)buffer;

	version = LittleLong (pin->version);
	if (version != SPRITE_VERSION)
	if (version != SPRITE32_VERSION)
	if (version != SPRITEHL_VERSION)
	{
		Con_Printf (CON_ERROR "%s has wrong version number "
				 "(%i should be %i)\n", mod->name, version, SPRITE_VERSION);
		return false;
	}

	sptype = LittleLong (pin->type);

	if (LittleLong(pin->version) == SPRITEHL_VERSION)
	{
		pin = (dsprite_t*)((char*)pin + 4);
		rendertype = LittleLong (pin->type);
	}

	numframes = LittleLong (pin->numframes);

	size = sizeof (msprite_t) +	(numframes - 1) * sizeof (psprite->frames);

	psprite = Hunk_AllocName (size, loadname);

	mod->cache.data = psprite;
	psprite->type = sptype;

	psprite->maxwidth = LittleLong (pin->width);
	psprite->maxheight = LittleLong (pin->height);
	psprite->beamlength = LittleFloat (pin->beamlength);
	mod->synctype = LittleLong (pin->synctype);
	psprite->numframes = numframes;

	mod->mins[0] = mod->mins[1] = -psprite->maxwidth/2;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth/2;
	mod->mins[2] = -psprite->maxheight/2;
	mod->maxs[2] = psprite->maxheight/2;

	if (pin->version == SPRITEHL_VERSION)
	{
		int i;
		short *numi = (short*)(pin+1);
		unsigned char *src = (unsigned char *)(numi+1);
		if (LittleShort(*numi) != 256)
		{
			Con_Printf(CON_ERROR "%s has wrong number of palette indexes (we only support 256)\n", mod->name);
			Hunk_FreeToLowMark(hunkstart);
			return false;
		}

		for (i = 0; i < 256; i++)
		{
			pal[i*4+0] = *src++;
			pal[i*4+1] = *src++;
			pal[i*4+2] = *src++;
			pal[i*4+3] = 255;
		}

		pframetype = (dspriteframetype_t *)(src);
	}
	else
		pframetype = (dspriteframetype_t *)(pin + 1);

//
// load the frames
//
	if (numframes < 1)
	{
		Con_Printf (CON_ERROR "Mod_LoadSpriteModel: Invalid # of frames: %d\n", numframes);
		Hunk_FreeToLowMark(hunkstart);
		return false;
	}

	mod->numframes = numframes;

	for (i=0 ; i<numframes ; i++)
	{
		spriteframetype_t	frametype;

		frametype = LittleLong (pframetype->type);
		psprite->frames[i].type = frametype;

		if (frametype == SPR_SINGLE)
		{
			pframetype = (dspriteframetype_t *)
					GLMod_LoadSpriteFrame (pframetype + 1,
										 &psprite->frames[i].frameptr, i, version, pal);
		}
		else
		{
			pframetype = (dspriteframetype_t *)
					GLMod_LoadSpriteGroup (pframetype + 1,
										 &psprite->frames[i].frameptr, i, version, pal);
		}
		if (pframetype == NULL)
		{
			Hunk_FreeToLowMark(hunkstart);
			return false;
		}
	}

	mod->type = mod_sprite;

	return true;
}

qboolean GLMod_LoadSprite2Model (model_t *mod, void *buffer)
{
	int					i;
	int					version;
	dmd2sprite_t		*pin;
	msprite_t			*psprite;
	int					numframes;
	int					size;
	dmd2sprframe_t		*pframetype;
	mspriteframe_t		*frame;
	float origin[2];
	int hunkstart;

	hunkstart = Hunk_LowMark();
	
	pin = (dmd2sprite_t *)buffer;

	version = LittleLong (pin->version);
	if (version != SPRITE2_VERSION)
	{
		Con_Printf (CON_ERROR "%s has wrong version number "
				 "(%i should be %i)", mod->name, version, SPRITE2_VERSION);
		return false;
	}

	numframes = LittleLong (pin->numframes);

	size = sizeof (msprite_t) +	(numframes - 1) * sizeof (psprite->frames);

	psprite = Hunk_AllocName (size, loadname);

	mod->cache.data = psprite;

	psprite->type = SPR_VP_PARALLEL;
	psprite->maxwidth = 1;
	psprite->maxheight = 1;
	psprite->beamlength = 1;
	mod->synctype = 0;
	psprite->numframes = numframes;

	mod->mins[0] = mod->mins[1] = -psprite->maxwidth/2;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth/2;
	mod->mins[2] = -psprite->maxheight/2;
	mod->maxs[2] = psprite->maxheight/2;
	
//
// load the frames
//
	if (numframes < 1)
	{
		Con_Printf (CON_ERROR "Mod_LoadSpriteModel: Invalid # of frames: %d\n", numframes);
		Hunk_FreeToLowMark(hunkstart);
		return false;
	}

	mod->numframes = numframes;

	pframetype = pin->frames;

	for (i=0 ; i<numframes ; i++)
	{
		spriteframetype_t	frametype;

		frametype = SPR_SINGLE;
		psprite->frames[i].type = frametype;

		frame = psprite->frames[i].frameptr = Hunk_AllocName(sizeof(mspriteframe_t), loadname);

		frame->gl_texturenum = Mod_LoadHiResTexture(pframetype->name, NULL, true, true, true);

		frame->width = LittleLong(pframetype->width);
		frame->height = LittleLong(pframetype->height);
		origin[0] = LittleLong (pframetype->origin_x);
		origin[1] = LittleLong (pframetype->origin_y);

		frame->up = -origin[1];
		frame->down = frame->height - origin[1];
		frame->left = -origin[0];
		frame->right = frame->width - origin[0];

		pframetype++;
	}

	mod->type = mod_sprite;

	return true;
}


#ifdef DOOMWADS

typedef struct {
	short width;
	short height;
	short xpos;
	short ypos;
} doomimage_t;
static int FindDoomSprites(char *name, int size, void *param)
{
	if (*(int *)param + strlen(name)+1 > 16000)
		Sys_Error("Too many doom sprites\n");

	strcpy((char *)param + *(int *)param, name);
	*(int *)param += strlen(name)+1;

	return true;
}


static void LoadDoomSpriteFrame(char *imagename, mspriteframedesc_t *pdesc, int anglenum, qboolean xmirrored)
{
	int c;
	int fr;
	int rc;
	unsigned int *colpointers;
	qbyte *data;
	doomimage_t *header;

	qbyte image[256*256];
	qbyte *palette;
	qbyte *coldata;
	mspriteframe_t *pframe;

	if (!anglenum)
	{
		pdesc->type = SPR_SINGLE;
		pdesc->frameptr = pframe = Hunk_AllocName(sizeof(*pframe), loadname);
	}
	else
	{
		mspritegroup_t *group;

		if (!pdesc->frameptr || pdesc->type != SPR_ANGLED)
		{
			pdesc->type = SPR_ANGLED;
			group = Hunk_AllocName(sizeof(*group)+sizeof(mspriteframe_t *)*(8-1), loadname);
			pdesc->frameptr = (mspriteframe_t *)group;
			group->numframes = 8;
		}
		else
			group = (mspritegroup_t *)pdesc->frameptr;

		pframe = Hunk_AllocName(sizeof(*pframe), loadname);
		group->frames[anglenum-1] = pframe;
	}

	palette = COM_LoadTempFile("wad/playpal");
	header = (doomimage_t *)COM_LoadTempFile2(imagename);
	data = (qbyte *)header;
	pframe->width = header->width;
	pframe->height = header->height;
	pframe->up = +header->ypos;
	pframe->down = -header->height + header->ypos;

	if (xmirrored)
	{
		pframe->right = -header->xpos;
		pframe->left = header->width - header->xpos;
	}
	else
	{
		pframe->left = -header->xpos;
		pframe->right = header->width - header->xpos;
	}

	if (header->width*header->height > sizeof(image))
		return;

	memset(image, 255, header->width*header->height);
	colpointers = (unsigned int*)(data+sizeof(doomimage_t));
	for (c = 0; c < header->width; c++)
	{
		if (colpointers[c] >= com_filesize)
			break;
		coldata = data + colpointers[c];
		while(1)
		{
			fr = *coldata++;
			if (fr == 255)
				break;

			rc = *coldata++;

			coldata++;

			if ((fr+rc) > header->height)
				break;

			while(rc)
			{
				image[c + fr*header->width] = *coldata++;
				fr++;
				rc--;
			}

			coldata++;
		}
	}

	pframe->gl_texturenum = GL_LoadTexture8Pal24(imagename, pframe->width, pframe->height, image, palette, true, true);
}

/*
=================
Doom Sprites
=================
*/
void GLMod_LoadDoomSprite (model_t *mod)
{
	char files[16384];
	char basename[MAX_QPATH];
	int baselen;
	char *name;

	int numframes=0;
	int ofs;

	int size;

	int elements=0;

	int framenum;
	int anglenum;

	msprite_t *psprite;


	COM_StripExtension(mod->name, basename, sizeof(basename));
	baselen = strlen(basename);
	strcat(basename, "*");
	*(int *)files=4;
	COM_EnumerateFiles(basename, FindDoomSprites, files);

	//find maxframes and validate the rest.
	for (ofs = 4; ofs < *(int*)files; ofs+=strlen(files+ofs)+1)
	{
		name = files+ofs+baselen;

		if (!*name)
			Host_Error("Doom sprite componant lacks frame name");
		if (*name - 'a'+1 > numframes)
			numframes = *name - 'a'+1;
		if (name[1] < '0' || name[1] > '8')
			Host_Error("Doom sprite componant has bad angle number");
		if (name[1] == '0')
			elements+=8;
		else
			elements++;
		if (name[2])	//is there a second element?
		{
			if (name[2] - 'a'+1 > numframes)
				numframes = name[2] - 'a'+1;
			if (name[3] < '0' || name[3] > '8')
				Host_Error("Doom sprite componant has bad angle number");

			if (name[3] == '0')
				elements+=8;
			else
				elements++;
		}
	}
	if (elements != numframes*8)
		Host_Error("Doom sprite has wrong componant count");
	if (!numframes)
		Host_Error("Doom sprite componant has no frames");

	size = sizeof (msprite_t) +	(elements - 1) * sizeof (psprite->frames);
	psprite = Hunk_AllocName (size, loadname);

	psprite->numframes = numframes;

	//do the actual loading.
	for (ofs = 4; ofs < *(int*)files; ofs+=strlen(files+ofs)+1)
	{
		name = files+ofs;
		framenum = name[baselen+0] - 'a';
		anglenum = name[baselen+1] - '0';

		LoadDoomSpriteFrame(name, &psprite->frames[framenum], anglenum, false);

		if (name[baselen+2])	//is there a second element?
		{
			framenum = name[baselen+2] - 'a';
			anglenum = name[baselen+3] - '0';

			LoadDoomSpriteFrame(name, &psprite->frames[framenum], anglenum, true);
		}
	}


	psprite->type = SPR_FACING_UPRIGHT;
	mod->type = mod_sprite;

	mod->cache.data = psprite;
}
#endif

//=============================================================================

/*
================
Mod_Print
================
*/
void GLMod_Print (void)
{
	int		i;
	model_t	*mod;

	Con_Printf ("Cached models:\n");
	for (i=0, mod=mod_known ; i < mod_numknown ; i++, mod++)
	{
		Con_Printf ("%8p : %s\n",mod->cache.data, mod->name);
	}
}


#endif
