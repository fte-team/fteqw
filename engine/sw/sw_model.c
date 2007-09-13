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
#include "r_local.h"

model_t	*loadmodel;
char	loadname[32];	// for hunk tags

qboolean SWMod_LoadSpriteModel (model_t *mod, void *buffer);
qboolean SWMod_LoadSprite2Model (model_t *mod, void *buffer);
qboolean SWMod_LoadBrushModel (model_t *mod, void *buffer);
qboolean Mod_LoadQ2BrushModel (model_t *mod, void *buffer);
qboolean SWMod_LoadAliasModel (model_t *mod, void *buffer);
qboolean SWMod_LoadAlias2Model (model_t *mod, void *buffer);
qboolean SWMod_LoadAlias3Model (model_t *mod, void *buffer);
model_t *SWMod_LoadModel (model_t *mod, qboolean crash);

int Mod_ReadFlagsFromMD1(char *name, int md3version);

qbyte	mod_novis[MAX_MAP_LEAFS/8];

#define	MAX_MOD_KNOWN	1024
model_t	mod_known[MAX_MOD_KNOWN];
int		mod_numknown;

#ifdef SERVERONLY
#define SWMod_FindName Mod_FindName
#define SWMod_ForName Mod_ForName
#define SWMod_LeafPVS Mod_LeafPVS
#define SWMod_PointInLeaf Mod_PointInLeaf
#define SWMod_ClearAll Mod_ClearAll
#define SWMod_Init Mod_Init
#endif

/*
===============
Mod_Init
===============
*/
void SWMod_Init (void)
{
	mod_numknown = 0;
	memset (mod_novis, 0xff, sizeof(mod_novis));

	Cmd_RemoveCommand("mod_texturelist");
}

void SWMod_Think(void)
{
}

/*
===============
Mod_Init

Caches the data if needed
===============
*/
void *SWMod_Extradata (model_t *mod)
{
	void	*r;
	
	r = Cache_Check (&mod->cache);
	if (r)
		return r;

	SWMod_LoadModel (mod, true);
	
	if (!mod->cache.data)
		Sys_Error ("Mod_Extradata: caching failed");
	return mod->cache.data;
}

/*
===============
Mod_PointInLeaf
===============
*/
static int SWMod_LeafForPoint (model_t *model, vec3_t p)
{
	mnode_t		*node;
	float		d;
	mplane_t	*plane;

#ifdef Q2BSPS
	if (model->fromgame == fg_quake2 || model->fromgame == fg_quake3)
	{
		return CM_PointLeafnum(cl.worldmodel, p);
	}
#endif
	
	if (!model || !model->nodes)
		Sys_Error ("Mod_PointInLeaf: bad model");

	node = model->nodes;
	while (1)
	{
		if (node->contents < 0)
			return (mleaf_t *)node - model->leafs;
		plane = node->plane;
		d = DotProduct (p,plane->normal) - plane->dist;
		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}
	
	return 0;	// never reached
}

mleaf_t *SWMod_PointInLeaf (model_t *model, vec3_t p)
{
	return model->leafs + SWMod_LeafForPoint(model, p);
}


/*
===================
Mod_DecompressVis
===================
*/
static qbyte *SWMod_DecompressVis (model_t *model, qbyte *in, qbyte *decompressed)
{
	int		c;
	qbyte	*out;
	int		row;

	row = (model->numleafs+7)>>3;	
	out = decompressed;

#if 0
	memcpy (out, in, row);
#else
	if (!in)
	{	// no vis info, so make all visible
		while (row)
		{
			*out++ = 0xff;
			row--;
		}
		return decompressed;		
	}

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}
	
		c = in[1];
		in += 2;
		while (c)
		{
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);
#endif
	
	return decompressed;
}

qbyte *SWMod_LeafPVS (model_t *model, mleaf_t *leaf, qbyte *buffer)
{
	static qbyte	decompressed[MAX_MAP_LEAFS/8];

	if (leaf == model->leafs)
		return mod_novis;

	if (!buffer)
		buffer = decompressed;
	return SWMod_DecompressVis (model, leaf->compressed_vis, buffer);
}

/*
===================
Mod_ClearAll
===================
*/
void SWMod_ClearAll (void)
{
	int		i;
	model_t	*mod;
	
	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
		if (mod->type != mod_alias)
			mod->needload = true;
}

/*
==================
Mod_FindName

==================
*/
model_t *SWMod_FindName (char *name)
{
	int		i;
	model_t	*mod;
	
//	if (!name[0])	//this is allowed to happen for q2 cinematics. :(
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
	}

	return mod;
}

/*
==================
Mod_TouchModel

==================
*/
void SWMod_TouchModel (char *name)
{
	model_t	*mod;
	
	mod = SWMod_FindName (name);
	
	if (!mod->needload)
	{
		if (mod->type == mod_alias)
			Cache_Check (&mod->cache);
	}
}

/*
==================
Mod_LoadModel

Loads a model into the cache
==================
*/
model_t *SWMod_LoadModel (model_t *mod, qboolean crash)
{
	extern cvar_t r_replacemodels;

	void	*d;
	unsigned *buf = NULL;
	qbyte	stackbuf[1024];		// avoid dirtying the cache heap
	char mdlbase[MAX_QPATH];
	qboolean lastload = false;
	char *replstr;
//	qboolean doomsprite = false;

	char *ext;

	if (!mod->needload)
	{
		if (mod->type == mod_alias)
		{
			d = Cache_Check (&mod->cache);
			if (d)
				return mod;
		}
		else
			return mod;		// not cached at all
	}
#ifdef Q2BSPS
	if (!*mod->name)
	{
		loadmodel = mod;
		if (!Mod_LoadQ2BrushModel(mod, NULL))
			goto couldntload;
		mod->needload = false;
		P_DefaultTrail(mod);
		return mod;
	}
#endif

//
// because the world is so huge, load it one piece at a time
//
	// set necessary engine flags for loading purposes
	if (!strcmp(mod->name, "progs/player.mdl"))
	{
		mod->engineflags |= MDLF_PLAYER | MDLF_DOCRC;
	}
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
	if (isDedicated)
		replstr = NULL;
	else if (!Q_strcasecmp(ext, "spr") || !Q_strcasecmp(ext, "sp2"))
		replstr = NULL; // sprite
	else if (!Q_strcasecmp(ext, "dsp")) // doom sprite
		replstr = NULL;
	else // assume models
		replstr = r_replacemodels.string;

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
			if (!buf) // we would attempt Doom sprites here, but SW doesn't support them
				break; // failed to load unreplaced file and nothing left
		}
	
//
// allocate a new model
//
		COM_FileBase (mod->name, loadname, sizeof(loadname));
			
		loadmodel = mod;
//
// fill it in
//
			
		switch (LittleLong(*(unsigned *)buf))
		{
#ifndef SERVERONLY
		case IDPOLYHEADER:
			if (!SWMod_LoadAliasModel (mod, buf))
				continue;
			break;

		case MD2IDALIASHEADER:
			if (!SWMod_LoadAlias2Model (mod, buf))
				continue;
			break;

		case MD3_IDENT:
			if (!SWMod_LoadAlias3Model (mod, buf))
				continue;
			break;

		case IDSPRITEHEADER:
			if (!SWMod_LoadSpriteModel (mod, buf))
				continue;
			break;
			
		case IDSPRITE2HEADER:
			if (!SWMod_LoadSprite2Model (mod, buf))
				continue;
			break;
#endif
#ifdef Q2BSPS
		case IDBSPHEADER:	//looks like id switched to have proper ids
			if (!Mod_LoadQ2BrushModel (mod, buf))
				continue;
			break;
#endif

		case BSPVERSIONHL:
		case BSPVERSION:	//hmm.
		case BSPVERSIONPREREL:
			if (!SWMod_LoadBrushModel (mod, buf))
				continue;
			break;

		default:	//some telejano mods can do this
			Con_Printf(S_WARNING "Unrecognized format %i\n", LittleLong(*(unsigned *)buf));
			continue;
		}

		P_DefaultTrail(mod);

#ifndef SERVERONLY
		if (cl.model_precache[1])	//not the world.
			Validation_IncludeFile(mod->name, (char *)buf, com_filesize);
#endif

		return mod;
	}

couldntload:
	if (crash)
		Host_EndGame ("Mod_NumForName: %s not found or couldn't load", mod->name);

	Con_Printf(S_ERROR "Unable to load or replace %s\n", mod->name);
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
model_t *SWMod_ForName (char *name, qboolean crash)
{
	model_t	*mod;
	
	mod = SWMod_FindName (name);
	
	return SWMod_LoadModel (mod, crash);
}


/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

qbyte	*mod_base;


/*
=================
Mod_LoadTextures
=================
*/
#ifndef SERVERONLY
qboolean SWMod_LoadTextures (lump_t *l)
{
	int		i, j, pixels, num, max, altmax;
	miptex_t	*mt;
	texture_t	*tx, *tx2;
	texture_t	*anims[10];
	texture_t	*altanims[10];
	dmiptexlump_t *m;	

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
		if (m->dataofs[i] == -1)
			continue;
		mt = (miptex_t *)((qbyte *)m + m->dataofs[i]);
		mt->width = LittleLong (mt->width);
		mt->height = LittleLong (mt->height);
		for (j=0 ; j<MIPLEVELS ; j++)
			mt->offsets[j] = LittleLong (mt->offsets[j]);
		
		if ( (mt->width & 15) || (mt->height & 15) )
		{
			Con_Printf (S_ERROR "Texture %s is not 16 aligned\n", mt->name);
			return false;
		}

		if (!mt->offsets[0])	//external hl texture.
		{
			int pb;
			if (r_usinglits)	//allocate enough mem
				pb = 4;
			else
				pb = 1;

			pixels = mt->width*pb*mt->height/64*85;

			tx = Hunk_AllocName (sizeof(texture_t) +pixels, mt->name );	//allocate enough to cover it.
			tx->pixbytes = pb;
			loadmodel->textures[i] = tx;

			memcpy (tx->name, mt->name, sizeof(tx->name));
			tx->width = mt->width;
			tx->height = mt->height;
			for (j=0 ; j<MIPLEVELS ; j++)
				tx->offsets[j] = 0;//mt->offsets[j] + sizeof(texture_t) - sizeof(miptex_t);
			// the pixels immediately follow the structures
			memset ( tx+1, 7, pixels);	//set it all to 7 for no particular reason.


			R_AddBulleten(tx);
			continue;
		}
		else if (loadmodel->fromgame == fg_halflife)	//internal hl texture
		{
			qboolean alphaed;
			int pb;
			if (r_usinglits)	//allocate enough mem
				pb = 4;
			else
				pb = 1;
			
			pixels = (mt->width*pb*mt->height)*85/64;

			tx = Hunk_AllocName (sizeof(texture_t) +pixels, loadname );
			tx->pixbytes = pb;
			loadmodel->textures[i] = tx;
			
			memcpy (tx->name, mt->name, sizeof(tx->name));
			tx->width = mt->width;
			tx->height = mt->height;
			for (j=0 ; j<MIPLEVELS ; j++)
				tx->offsets[j] = (mt->offsets[j]-sizeof(miptex_t))*4 + sizeof(texture_t);
			// the pixels immediately follow the structures
			if (pb == 4)
			{
				memcpy ( tx+1, W_ConvertWAD3Texture(mt, &mt->width, &mt->height, &alphaed), pixels);
			}
			else
			{	//need to convert it down
				int k;
				qbyte *in;
				qbyte *out;
				out = (qbyte *)(tx+1);
				in = W_ConvertWAD3Texture(mt, &mt->width, &mt->height, &alphaed);
				tx->offsets[0] = (char *)out - (char *)tx;
				for (j = 0; j < mt->width*mt->height; j++, in+=4)
				{
					if (in[3] == 0)
						*out++ = 255;
					else
						*out++ = GetPaletteNoFB(in[0], in[1], in[2]);
				}

				in = out-mt->width*mt->height;	//shrink mips.

				tx->offsets[1] = (char *)out - (char *)tx;
				for (j = 0; j < tx->height; j+=2)	//we could convert mip[1], but shrinking is probably faster.
				for (k = 0; k < tx->width; k+=2)			
					*out++ = in[k + tx->width*j];

				tx->offsets[2] = (char *)out - (char *)tx;
				for (j = 0; j < tx->height; j+=4)
				for (k = 0; k < tx->width; k+=4)			
					*out++ = in[k + tx->width*j];

				tx->offsets[3] = (char *)out - (char *)tx;
				for (j = 0; j < tx->height; j+=8)
				for (k = 0; k < tx->width; k+=8)			
					*out++ = in[k + tx->width*j];
			}

			
//			if (!Q_strncmp(mt->name,"sky",3))	
//				R_InitSky (tx);
#ifdef PEXT_BULLETENS
//			else 
				R_AddBulleten(tx);
#endif
			continue;
		}
//internal 8bit texture
		pixels = mt->width*mt->height/64*85;

		tx = Hunk_AllocName (sizeof(texture_t) +pixels, loadname );
		tx->pixbytes = 1;
		loadmodel->textures[i] = tx;

		tx->parttype = P_ParticleTypeForName(va("tex_%s", tx->name));

		memcpy (tx->name, mt->name, sizeof(tx->name));
		tx->width = mt->width;
		tx->height = mt->height;
		for (j=0 ; j<MIPLEVELS ; j++)
			tx->offsets[j] = mt->offsets[j] + sizeof(texture_t) - sizeof(miptex_t);
		// the pixels immediately follow the structures
		memcpy ( tx+1, mt+1, pixels);
	
		if (!Q_strncmp(mt->name,"sky",3))
			SWR_InitSky (tx);
#ifdef PEXT_BULLETENS
		else R_AddBulleten(tx);
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
			Con_Printf (S_ERROR "Bad animating texture %s\n", tx->name);
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
				Con_Printf (S_ERROR "Bad animating texture %s\n", tx->name);
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
				Con_Printf (S_ERROR "Missing frame %i of %s\n",j, tx->name);
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
				Con_Printf (S_ERROR "Missing frame %i of %s\n",j, tx->name);
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

void SWMod_NowLoadExternal(void)
{
	int i, j, m, x, y;
	texture_t	*tx;
	qbyte *in;
	qbyte *out;
	qboolean alphaed;
	unsigned int *out32;
	int t;
	for (t=0 ; t<loadmodel->numtextures ; t++)
	{
		tx = loadmodel->textures[t];
		if (tx && !tx->offsets[0])
		{
			in = W_GetTexture(tx->name, &(tx->width), &(tx->height), &alphaed);
			i=0;
			tx->offsets[0] = sizeof(texture_t);
			tx->offsets[1] = tx->offsets[0] + tx->width*tx->pixbytes*tx->height;
			tx->offsets[2] = tx->offsets[1] + (tx->width*tx->pixbytes*tx->height)/4;
			tx->offsets[3] = tx->offsets[2] + (tx->width*tx->pixbytes*tx->height)/16;
			
			if (!in)
			{
				if (tx->pixbytes == 4)
				{
//					out32 = (unsigned int *)((qbyte *)tx+tx->offsets[0]);
//					memset(out32, 255, tx->width*tx->pixbytes*tx->height*85/64);
					for (m=0 ; m<4 ; m++)
					{
						out32 = (int *)((qbyte *)tx + tx->offsets[m]);
						for (y=0 ; y< (tx->height>>m) ; y++)
							for (x=0 ; x< (tx->width>>m) ; x++)
							{
								if ((y/2+x/2)&1)
									*out32++ = 0;
								else if (y/2&1&&x/2&1)
									*out32++ = 0x00007700;
								else
									*out32++ = 0x00000077;
							}
					}
				}
				else
				{
//					out = (qbyte *)tx+tx->offsets[0];
//					memset(out32, 15, tx->width*tx->height*85/64);	//usually white, anyway...
					for (m=0 ; m<4 ; m++)
					{
						out = (qbyte *)tx + tx->offsets[m];
						for (y=0 ; y< (tx->height>>m) ; y++)
							for (x=0 ; x< (tx->width>>m) ; x++)
							{
								if ((y+x)&1)
									*out++ = 0;
								else
									*out++ = 0xff;
							}
					}
				}
				continue;	//bother.
			}

			if (tx->pixbytes == 4)
			{
				out32 = (unsigned int *)((qbyte *)tx+tx->offsets[0]);
				memcpy(out32, in, tx->width*tx->pixbytes*tx->height*85/64);
			}
			else
			{
				out = (qbyte *)tx+tx->offsets[0];
				for (i=0; i < tx->width*tx->height; i++)	//downgrade colour
				{
					if (in[3] == 0)
						*out++ = 255;
					else
						*out++ = GetPaletteNoFB(in[0], in[1], in[2]);
					in += 4;
				}
				in = (qbyte *)tx+tx->offsets[0];	//shrink mips.

				for (j = 0; j < tx->height; j+=2)	//we could convert mip[1], but shrinking is probably faster.
				for (i = 0; i < tx->width; i+=2)			
					*out++ = in[i + tx->width*j];

				for (j = 0; j < tx->height; j+=4)
				for (i = 0; i < tx->width; i+=4)			
					*out++ = in[i + tx->width*j];

				for (j = 0; j < tx->height; j+=8)
				for (i = 0; i < tx->width; i+=8)			
					*out++ = in[i + tx->width*j];
			}
		}
			
	}
}

/*
=================
Mod_LoadLighting
=================
*/
void SWMod_LoadLighting (lump_t *l)
{
	extern cvar_t r_loadlits;
	if (!l->filelen)
	{
		loadmodel->lightdata = NULL;
		return;
	}
	if (r_pixbytes == 4)	//using 24 bit lighting
	{
		r_usinglits = true;
		if (r_loadlits.value && r_usinglits)
		{
			char litname[MAX_QPATH];
			qbyte *litdata = NULL;
			if (!litdata)
			{							
				strcpy(litname, loadmodel->name);
				COM_StripExtension(loadmodel->name, litname, sizeof(litname));
				COM_DefaultExtension(litname, ".lit", sizeof(litname));
				litdata = COM_LoadHunkFile(litname);
			}
			if (!litdata)
			{
				strcpy(litname, "lits/");
				COM_StripExtension(COM_SkipPath(loadmodel->name), litname+5, sizeof(litname)-5);
				strcat(litname, ".lit");
				
				litdata = COM_LoadHunkFile(litname);
			}
			if (litdata)
			{
				if (l->filelen != (com_filesize-8)/3)
					Con_Printf("lit \"%s\" doesn't match level. Ignored.\n", litname);
				else if (litdata[0] == 'Q' && litdata[1] == 'L' && litdata[2] == 'I' && litdata[3] == 'T')
				{
					if (LittleLong(*(int *)&litdata[4]) == 1)
					{
						float prop;
						int i;
						qbyte *normal;
	//					qbyte max;
						loadmodel->lightdata = litdata+8;						

						//now some cheat protection.
						normal = mod_base + l->fileofs;
						litdata = loadmodel->lightdata;					

						for (i = 0; i < l->filelen; i++)	//force it to the same intensity. (or less, depending on how you see it...)
						{
	#define m(a, b, c) (a>(b>c?b:c)?a:(b>c?b:c))
							prop = *normal / (float)m(litdata[0],  litdata[1], litdata[2]);
							litdata[0] *= prop;
							litdata[1] *= prop;
							litdata[2] *= prop;

							normal++;
							litdata+=3;
						}
						//end anti-cheat
						return;
					}
					else
						Con_Printf("\"%s\" isn't a standard version 1 lit\n", litname);
				}
				else
					Con_Printf("lit \"%s\" isn't a lit\n", litname);
			}	
		}
		if (loadmodel->fromgame == fg_halflife || loadmodel->fromgame == fg_quake2 || loadmodel->fromgame == fg_quake3)	//half-life levels use 24 bit anyway.
		{
			loadmodel->lightdata = Hunk_AllocName ( l->filelen, loadname);
			memcpy (loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
		}
		else if (!r_usinglits)
		{
			loadmodel->lightdata = Hunk_AllocName ( l->filelen, loadname);
			memcpy (loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
		}
		else
		{	//expand to 24 bit
			int i;
			qbyte *dest, *src = mod_base + l->fileofs;
			loadmodel->lightdata = Hunk_AllocName ( l->filelen*3, loadname);
			dest = loadmodel->lightdata;
			for (i = 0; i<l->filelen; i++)
			{
				dest[0] = *src;
				dest[1] = *src;
				dest[2] = *src;

				src++;
				dest+=3;
			}
		}

		if (r_lightmap_saturation.value != 1.0f)
			SaturateR8G8B8(loadmodel->lightdata, l->filelen, r_lightmap_saturation.value);
	}
	else
	{
		r_usinglits = false;
		if (loadmodel->fromgame == fg_halflife || loadmodel->fromgame == fg_quake2 || loadmodel->fromgame == fg_quake3)
		{
			int i;
			qbyte *out;
			qbyte *in;
			out = loadmodel->lightdata = Hunk_AllocName ( l->filelen/3, loadname);
			in = mod_base + l->fileofs;	//24 bit to luminance.
			for (i = 0; i < l->filelen; i+=3)
				*out++ = ((int)in[i] + (int)in[i] + (int)in[i])/3;

		}
		else
		{//standard Quake
			loadmodel->lightdata = Hunk_AllocName ( l->filelen, loadname);
			memcpy (loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
		}
	}
}
#endif

/*
=================
Mod_LoadVisibility
=================
*/
void SWMod_LoadVisibility (lump_t *l)
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
void SWMod_LoadEntities (lump_t *l)
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
qboolean SWMod_LoadVertexes (lump_t *l)
{
	dvertex_t	*in;
	mvertex_t	*out;
	int			i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (S_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
		return false;
	}
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( (count+8)*sizeof(*out), loadname);	//spare for skybox

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

static qboolean hexen2map;
/*
=================
Mod_LoadSubmodels
=================
*/
qboolean SWMod_LoadSubmodels (lump_t *l)
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
			Con_Printf (S_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
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
			Con_Printf (S_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
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
qboolean SWMod_LoadEdges (lump_t *l)
{
	dedge_t *in;
	medge_t *out;
	int 	i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (S_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
		return false;
	}
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( (count + 1 + 12) * sizeof(*out), loadname);	//spare for skybox

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
#ifndef SERVERONLY
qboolean SWMod_LoadTexinfo (lump_t *l)
{
	texinfo_t *in;
	mtexinfo_t *out;
	int 	i, j, count;
	int		miptex;
	float	len1, len2;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (S_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
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
	
		if (!loadmodel->textures)
		{
			out->texture = r_notexture_mip;	// checkerboard texture
			out->flags = 0;
		}
		else
		{
			if (miptex >= loadmodel->numtextures)
				Sys_Error ("miptex >= loadmodel->numtextures");
			out->texture = loadmodel->textures[miptex];
			if (!out->texture)
			{
				out->texture = r_notexture_mip; // texture not found
				out->flags = 0;
			}
			else if (!strncmp(out->texture->name, "sky", 3))
			{
				out->flags |= SURF_SKY;
			}
			else if (!strncmp(out->texture->name, "glass", 5))	//halflife levels
			{
				out->flags |= SURF_TRANS66;
			}
			else if (*out->texture->name == '{')	//halflife levels
			{
//				out->flags |= SURF_TRANS66;
			}
			else if (*out->texture->name == '!')	//halflife levels
			{
				out->flags |= SURF_WARP | SURF_TRANS33;
			}
		}
	}

	return true;
}
#endif
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
		if (s->extents[i] < 16)
			s->extents[i] = 16;	// take at least one cache block

//		if ( !(tex->flags & TEX_SPECIAL) && s->extents[i] > 512)
//			Sys_Error ("Bad surface extents");
	}
}
*/

/*
=================
Mod_LoadFaces
=================
*/
#ifndef SERVERONLY
qboolean SWMod_LoadFaces (lump_t *l)
{
	dface_t		*in;
	msurface_t 	*out;
	int			i, count, surfnum;
	int			planenum, side;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (S_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
		return false;
	}
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( (count+6)*sizeof(*out), loadname);	//spare for skybox

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

		out->texinfo = loadmodel->texinfo + LittleShort (in->texinfo);

		if (!out->texinfo->texture)
			out->texinfo->texture = r_notexture_mip;

		CalcSurfaceExtents (out);
				
	// lighting info

		for (i=0 ; i<MAXLIGHTMAPS ; i++)
			out->styles[i] = in->styles[i];
		i = LittleLong(in->lightofs);
		if (i == -1)
			out->samples = NULL;
		else if (r_usinglits)
		{
			if (loadmodel->fromgame == fg_halflife)
				out->samples = loadmodel->lightdata + i;
			else
				out->samples = loadmodel->lightdata + i*3;
		}
		else
		{
			if (loadmodel->fromgame == fg_halflife)
				out->samples = loadmodel->lightdata + i/3;
			else
				out->samples = loadmodel->lightdata + i;
		}
		
	// set the drawing flags flag
		
		if (!Q_strncmp(out->texinfo->texture->name,"sky",3))	// sky
		{
			if (loadmodel->fromgame == fg_halflife)
				out->flags |= SURF_DRAWBACKGROUND;
			else
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
	}

	return true;
}
#endif

/*
=================
Mod_SetParent
=================
*/
void SWMod_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents < 0)
		return;
	SWMod_SetParent (node->children[0], node);
	SWMod_SetParent (node->children[1], node);
}

/*
=================
Mod_LoadNodes
=================
*/
qboolean SWMod_LoadNodes (lump_t *l)
{
	int			i, j, count, p;
	dnode_t		*in;
	mnode_t 	*out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (S_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
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
	
	SWMod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs

	return true;
}

/*
=================
Mod_LoadLeafs
=================
*/
qboolean SWMod_LoadLeafs (lump_t *l)
{
	dleaf_t 	*in;
	mleaf_t 	*out;
	int			i, j, count, p;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (S_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
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
void SWMod_LoadCrouchHull(void)
{
	int i;
	int numsm;
	char crouchhullname[MAX_QPATH];
	int *data;

//	dclipnode_t *cn;

	numsuplementryplanes = numsuplementryclipnodes = 0;

	//find a name for a ccn and try to load it.
	strcpy(crouchhullname, loadmodel->name);
	COM_StripExtension(loadmodel->name, crouchhullname, sizeof(crouchhullname));
	COM_DefaultExtension(crouchhullname, ".crh", sizeof(crouchhullname));	//crouch hull

	crouchhullfile = COM_LoadMallocFile(crouchhullname);	//or otherwise temporary storage. load on hunk if you want, but that would be a waste.
	if (!crouchhullfile)
		return;

	if (LittleLong(((int *)crouchhullfile)[0]) != ('Q') + ('C'<<8) + ('C'<<16) + ('N'<<24))	//make sure it's the right version
		return;

	if (LittleLong(((int *)crouchhullfile)[1]) != 0)	//make sure it's the right version
		return;

	numsm = LittleLong(((int *)crouchhullfile)[2]);
	if (numsm != loadmodel->numsubmodels)	//not compatable
		return;

	numsuplementryplanes = LittleLong(((int *)crouchhullfile)[3]);
	numsuplementryclipnodes = LittleLong(((int *)crouchhullfile)[4]);

	data = &((int *)crouchhullfile)[5];

	for (i = 0; i < numsm; i++)	//load headnode references
	{
		loadmodel->submodels[i].headnode[3] = LittleLong(*data)+1;
		data++;
	}

	suplementryplanes = data;
	suplementryclipnodes = (qbyte*)data + sizeof(dplane_t)*numsuplementryplanes;
}

/*
=================
Mod_LoadClipnodes
=================
*/
qboolean SWMod_LoadClipnodes (lump_t *l)
{
	dclipnode_t *in, *out;
	int			i, count;
	hull_t		*hull;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (S_ERROR "MOD_LoadBmodel: funny lump size in %s\n", loadmodel->name);
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
		hull->clip_mins[2] = -50;
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
		in = suplementryclipnodes;
		hull->planes = suplementryplanes;
		hull->clipnodes = out-1;
		hull->lastclipnode = numsuplementryclipnodes;
		hull->available = true;

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
void SWMod_MakeHull0 (void)
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
qboolean SWMod_LoadMarksurfaces (lump_t *l)
{	
	int		i, j, count;
	short		*in;
	msurface_t **out;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (S_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
		return false;
	}
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for ( i=0 ; i<count ; i++)
	{
		j = LittleShort(in[i]);
		if (j >= loadmodel->numsurfaces)
		{
			Con_Printf (S_ERROR "Mod_ParseMarksurfaces: bad surface number\n");
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
qboolean SWMod_LoadSurfedges (lump_t *l)
{	
	int		i, count;
	int		*in, *out;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
		return false;
	}
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( (count+24)*sizeof(*out), loadname);	//spare for skybox

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
qboolean SWMod_LoadPlanes (lump_t *l)
{
	int			i, j;
	mplane_t	*out;
	dplane_t 	*in;
	int			count;
	int			bits;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (S_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
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


void Q1BSP_MarkLights (dlight_t *light, int bit, mnode_t *node);
void SWQ1BSP_LightPointValues(model_t *mod, vec3_t point, vec3_t res_diffuse, vec3_t res_ambient, vec3_t res_dir);

void SWR_Q1BSP_StainNode (mnode_t *node, float *parms);
/*
=================
Mod_LoadBrushModel
=================
*/
qboolean SWMod_LoadBrushModel (model_t *mod, void *buffer)
{
	int			i, j;
	dheader_t	*header;
	mmodel_t 	*bm;
	int start;
	qboolean noerrors;

	start = Hunk_LowMark();
	
	loadmodel->type = mod_brush;
	
	header = (dheader_t *)buffer;

	i = LittleLong (header->version);

	if (i == BSPVERSION || i == BSPVERSIONPREREL)
		loadmodel->fromgame = fg_quake;
	else if (i == BSPVERSIONHL)
		loadmodel->fromgame = fg_halflife;
	else
	{
		Con_Printf (S_ERROR "Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", mod->name, i, BSPVERSION);
		return false;
	}
//	if (i != BSPVERSION)
//		Sys_Error ("Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", mod->name, i, BSPVERSION);

// swap all the lumps
	mod_base = (qbyte *)header;

	for (i=0 ; i<sizeof(dheader_t)/4 ; i++)
		((int *)header)[i] = LittleLong ( ((int *)header)[i]);

	mod->checksum = 0;
	mod->checksum2 = 0;

// checksum all of the map, except for entities
	for (i = 0; i < HEADER_LUMPS; i++)
	{
		if ((unsigned)header->lumps[i].fileofs + (unsigned)header->lumps[i].filelen > com_filesize)
		{
			Con_Printf (S_ERROR "Mod_LoadBrushModel: %s appears truncated\n", mod->name);
			return false;
		}
		if (i == LUMP_ENTITIES)
			continue;
		mod->checksum ^= Com_BlockChecksum(mod_base + header->lumps[i].fileofs, 
			header->lumps[i].filelen);

		if (i == LUMP_VISIBILITY || i == LUMP_LEAFS || i == LUMP_NODES)
			continue;
		mod->checksum2 ^= Com_BlockChecksum(mod_base + header->lumps[i].fileofs, 
			header->lumps[i].filelen);
	}
	
	noerrors = true;
	crouchhullfile = NULL;

// load into heap
#ifndef CLIENTONLY
	if (!isDedicated)
#endif
	{
		noerrors = noerrors && SWMod_LoadVertexes (&header->lumps[LUMP_VERTEXES]);
		noerrors = noerrors && SWMod_LoadEdges (&header->lumps[LUMP_EDGES]);
		noerrors = noerrors && SWMod_LoadSurfedges (&header->lumps[LUMP_SURFEDGES]);
		if (noerrors)
			SWMod_LoadLighting (&header->lumps[LUMP_LIGHTING]);	//DMW, made lighting load first. (so we know if lighting is rgb or luminance)
		noerrors = noerrors && SWMod_LoadTextures (&header->lumps[LUMP_TEXTURES]);
	}

	noerrors = noerrors && SWMod_LoadSubmodels (&header->lumps[LUMP_MODELS]);	//needs to come before we set the headnodes[3]
	if (noerrors)
		SWMod_LoadCrouchHull ();
	noerrors = noerrors && SWMod_LoadPlanes (&header->lumps[LUMP_PLANES]);

#ifndef CLIENTONLY
	if (!isDedicated)
#endif
	{
		noerrors = noerrors && SWMod_LoadTexinfo (&header->lumps[LUMP_TEXINFO]);
		noerrors = noerrors && SWMod_LoadFaces (&header->lumps[LUMP_FACES]);
		noerrors = noerrors && SWMod_LoadMarksurfaces (&header->lumps[LUMP_MARKSURFACES]);
	}

	if (noerrors)
		SWMod_LoadVisibility (&header->lumps[LUMP_VISIBILITY]);
	noerrors = noerrors && SWMod_LoadLeafs (&header->lumps[LUMP_LEAFS]);
	noerrors = noerrors && SWMod_LoadNodes (&header->lumps[LUMP_NODES]);
	noerrors = noerrors && SWMod_LoadClipnodes (&header->lumps[LUMP_CLIPNODES]);
	if (noerrors)
		SWMod_LoadEntities (&header->lumps[LUMP_ENTITIES]);	

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

	Q1BSP_SetModelFuncs(mod);
	mod->funcs.LightPointValues		= SWQ1BSP_LightPointValues;
	mod->funcs.StainNode			= SWR_Q1BSP_StainNode;
	mod->funcs.MarkLights			= Q1BSP_MarkLights;


//We ONLY do this for the world model
#ifndef SERVERONLY
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
#endif

	SWMod_MakeHull0 ();
	
	mod->numframes = 2;		// regular and alternate animation
	
//
// set up the submodels (FIXME: this is confusing)
//
	for (i=0 ; i<mod->numsubmodels ; i++)
	{
		bm = &mod->submodels[i];

		mod->hulls[0].firstclipnode = bm->headnode[0];
		Q1BSP_SetHullFuncs(&mod->hulls[0]);
		for (j=1 ; j<MAX_MAP_HULLSM ; j++)
		{
			mod->hulls[j].firstclipnode = bm->headnode[j];
			mod->hulls[j].lastclipnode = mod->numclipnodes-1;
			Q1BSP_SetHullFuncs(&mod->hulls[j]);
		}
		
		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;
		mod->radius = RadiusFromBounds (mod->mins, mod->maxs);
		
		VectorCopy (bm->maxs, mod->maxs);
		VectorCopy (bm->mins, mod->mins);
	
		mod->numleafs = bm->visleafs;

		if (i < mod->numsubmodels-1)
		{	// duplicate the basic information
			char	name[10];

			sprintf (name, "*%i", i+1);
			loadmodel = SWMod_FindName (name);
			*loadmodel = *mod;
			strcpy (loadmodel->name, name);
			mod = loadmodel;

			P_DefaultTrail(mod);
		}
	}

	return true;
}
#ifndef SERVERONLY
/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

void * SWMod_LoadAliasQTestFrame (void * pin, int *pframeindex, int numv,
	dtrivertx_t *pbboxmin, dtrivertx_t *pbboxmax, aliashdr_t *pheader, char *name)
{
	dtrivertx_t		*pframe, *pinframe;
	int				i, j;
	qtestaliasframe_t	*pdaliasframe;

	pdaliasframe = (qtestaliasframe_t *)pin;

	name[0] = '\0';

	for (i=0 ; i<3 ; i++)
	{
	// these are byte values, so we don't have to worry about
	// endianness
		pbboxmin->v[i] = pdaliasframe->bboxmin.v[i];
		pbboxmax->v[i] = pdaliasframe->bboxmax.v[i];
	}

	pinframe = (dtrivertx_t *)(pdaliasframe + 1);
	pframe = Hunk_AllocName (numv * sizeof(*pframe), loadname);

	*pframeindex = (qbyte *)pframe - (qbyte *)pheader;

	for (j=0 ; j<numv ; j++)
	{
		int		k;

	// these are all byte values, so no need to deal with endianness
		pframe[j].lightnormalindex = pinframe[j].lightnormalindex;

		for (k=0 ; k<3 ; k++)
		{
			pframe[j].v[k] = pinframe[j].v[k];
		}
	}

	pinframe += numv;

	return (void *)pinframe;
}

/*
=================
Mod_LoadAliasFrame
=================
*/
void * SWMod_LoadAliasFrame (void * pin, int *pframeindex, int numv,
	dtrivertx_t *pbboxmin, dtrivertx_t *pbboxmax, aliashdr_t *pheader, char *name)
{
	dtrivertx_t		*pframe, *pinframe;
	int				i, j;
	daliasframe_t	*pdaliasframe;

	pdaliasframe = (daliasframe_t *)pin;

	strcpy (name, pdaliasframe->name);

	for (i=0 ; i<3 ; i++)
	{
	// these are byte values, so we don't have to worry about
	// endianness
		pbboxmin->v[i] = pdaliasframe->bboxmin.v[i];
		pbboxmax->v[i] = pdaliasframe->bboxmax.v[i];
	}

	pinframe = (dtrivertx_t *)(pdaliasframe + 1);
	pframe = Hunk_AllocName (numv * sizeof(*pframe), loadname);

	*pframeindex = (qbyte *)pframe - (qbyte *)pheader;

	for (j=0 ; j<numv ; j++)
	{
		int		k;

	// these are all byte values, so no need to deal with endianness
		pframe[j].lightnormalindex = pinframe[j].lightnormalindex;

		for (k=0 ; k<3 ; k++)
		{
			pframe[j].v[k] = pinframe[j].v[k];
		}
	}

	pinframe += numv;

	return (void *)pinframe;
}


/*
=================
Mod_LoadAliasGroup
=================
*/
void * SWMod_LoadAliasGroup (void * pin, int *pframeindex, int numv,
	dtrivertx_t *pbboxmin, dtrivertx_t *pbboxmax, aliashdr_t *pheader, char *name)
{
	daliasgroup_t		*pingroup;
	maliasgroup_t		*paliasgroup;
	int					i, numframes;
	daliasinterval_t	*pin_intervals;
	float				*poutintervals;
	void				*ptemp;
	
	pingroup = (daliasgroup_t *)pin;

	numframes = LittleLong (pingroup->numframes);

	paliasgroup = Hunk_AllocName (sizeof (maliasgroup_t) +
			(numframes - 1) * sizeof (paliasgroup->frames[0]), loadname);

	paliasgroup->numframes = numframes;

	for (i=0 ; i<3 ; i++)
	{
	// these are byte values, so we don't have to worry about endianness
		pbboxmin->v[i] = pingroup->bboxmin.v[i];
		pbboxmax->v[i] = pingroup->bboxmax.v[i];
	}

	*pframeindex = (qbyte *)paliasgroup - (qbyte *)pheader;

	pin_intervals = (daliasinterval_t *)(pingroup + 1);

	poutintervals = Hunk_AllocName (numframes * sizeof (float), loadname);

	paliasgroup->intervals = (qbyte *)poutintervals - (qbyte *)pheader;

	for (i=0 ; i<numframes ; i++)
	{
		*poutintervals = LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0)
			return NULL;

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *)pin_intervals;

	for (i=0 ; i<numframes ; i++)
	{
		ptemp = SWMod_LoadAliasFrame (ptemp,
									&paliasgroup->frames[i].frame,
									numv,
									&paliasgroup->frames[i].bboxmin,
									&paliasgroup->frames[i].bboxmax,
									pheader, name);
		if (ptemp == NULL)
			return NULL;
	}

	return ptemp;
}


/*
=================
Mod_LoadAliasSkin
=================
*/
void * SWMod_LoadAliasSkin (void * pin, int *pskinindex, int skinsize,
	aliashdr_t *pheader)
{
	int		i;
	qbyte	*pskin, *pinskin;
//	unsigned short	*pusskin;
	unsigned int	*p32skin;

	pskin = Hunk_AllocName (skinsize * r_pixbytes, loadname);
	pinskin = (qbyte *)pin;
	*pskinindex = (qbyte *)pskin - (qbyte *)pheader;

	if (r_pixbytes == 1 || r_pixbytes == 2)
	{
		Q_memcpy (pskin, pinskin, skinsize);
	}
	else if (r_pixbytes == 4)
	{
		extern qbyte *host_basepal;
		p32skin = (unsigned int *)pskin;

		for (i=0 ; i<skinsize ; i++)
			p32skin[i] = (255<<24) | (host_basepal[pinskin[i]*3+0]<<16) | (host_basepal[pinskin[i]*3+1]<<8) | (host_basepal[pinskin[i]*3+2]<<0);//d_8to32table[pinskin[i]];
	}
	else
	{
		Sys_Error ("Mod_LoadAliasSkin: driver set invalid r_pixbytes: %d\n",
				 r_pixbytes);
	}

	pinskin += skinsize;

	return ((void *)pinskin);
}

void * SWMod_LoadAlias2Skin (void * pin, int *pskinindex, int skinsize,
	aliashdr_t *pheader)
{
	return ((void *)pin);
}

/*
=================
Mod_LoadAliasSkinGroup
=================
*/
void * SWMod_LoadAliasSkinGroup (void * pin, int *pskinindex, int skinsize,
	aliashdr_t *pheader)
{
	daliasskingroup_t		*pinskingroup;
	maliasskingroup_t		*paliasskingroup;
	int						i, numskins;
	daliasskininterval_t	*pinskinintervals;
	float					*poutskinintervals;
	void					*ptemp;

	pinskingroup = (daliasskingroup_t *)pin;

	numskins = LittleLong (pinskingroup->numskins);

	paliasskingroup = Hunk_AllocName (sizeof (maliasskingroup_t) +
			(numskins - 1) * sizeof (paliasskingroup->skindescs[0]),
			loadname);

	paliasskingroup->numskins = numskins;

	*pskinindex = (qbyte *)paliasskingroup - (qbyte *)pheader;

	pinskinintervals = (daliasskininterval_t *)(pinskingroup + 1);

	poutskinintervals = Hunk_AllocName (numskins * sizeof (float),loadname);

	paliasskingroup->intervals = (qbyte *)poutskinintervals - (qbyte *)pheader;

	for (i=0 ; i<numskins ; i++)
	{
		*poutskinintervals = LittleFloat (pinskinintervals->interval);
		if (*poutskinintervals <= 0)
			Sys_Error ("Mod_LoadAliasSkinGroup: interval<=0");

		poutskinintervals++;
		pinskinintervals++;
	}

	ptemp = (void *)pinskinintervals;

	for (i=0 ; i<numskins ; i++)
	{
		ptemp = SWMod_LoadAliasSkin (ptemp,
				&paliasskingroup->skindescs[i].skin, skinsize, pheader);
	}

	return ptemp;
}


/*
=================
Mod_LoadAliasModel
=================
*/
qboolean SWMod_LoadAliasModel (model_t *mod, void *buffer)
{
	int					i;
	mmdl_t				*pmodel;
	dmdl_t				*pinmodel;
	mstvert_t			*pstverts;
	dstvert_t			*pinstverts;
	aliashdr_t			*pheader;
	mtriangle_t			*ptri;
	dtriangle_t			*pintriangles;
	int					version, numframes, numskins;
	int					size;
	daliasframetype_t	*pframetype;
	daliasskintype_t	*pskintype;
	maliasskindesc_t	*pskindesc;
	int					skinsize;
	int					start, end, total;
	qboolean qtest = false;
	
	if (loadmodel->engineflags & MDLF_DOCRC)
	{
		unsigned short crc;
		qbyte *p;
		int len;
		char st[40];

		QCRC_Init(&crc);
		for (len = com_filesize, p = buffer; len; len--, p++)
			QCRC_ProcessByte(&crc, *p);
	
		sprintf(st, "%d", (int) crc);
		Info_SetValueForKey (cls.userinfo, 
			(loadmodel->engineflags & MDLF_PLAYER) ? pmodel_name : emodel_name,
			st, MAX_INFO_STRING);

		if (cls.state >= ca_connected)
		{
			CL_SendClientCommand(true, "setinfo %s %d", 
				(loadmodel->engineflags & MDLF_PLAYER) ? pmodel_name : emodel_name,
				(int)crc);
		}
	}

	start = Hunk_LowMark ();

	pinmodel = (dmdl_t *)buffer;

	version = LittleLong (pinmodel->version);
	if (version == QTESTALIAS_VERSION)
		qtest = true;
	else if (version != ALIAS_VERSION)
	{
		Con_Printf (S_ERROR "%s has wrong version number (%i should be %i)\n",
				 mod->name, version, ALIAS_VERSION);
		return false;
	}

//
// allocate space for a working header, plus all the data except the frames,
// skin and group info
//
	size = 	sizeof (aliashdr_t) + (LittleLong (pinmodel->numframes) - 1) *
			 sizeof (pheader->frames[0]) +
			sizeof (mmdl_t) +
			LittleLong (pinmodel->numverts)*2 * sizeof (mstvert_t) +
			LittleLong (pinmodel->numtris) * sizeof (mtriangle_t);

	pheader = Hunk_AllocName (size, loadname);
	pmodel = (mmdl_t *) ((qbyte *)&pheader[1] +
			(LittleLong (pinmodel->numframes) - 1) *
			 sizeof (pheader->frames[0]));
	
//	mod->cache.data = pheader;
	if (qtest)
		mod->flags = 0;
	else
		mod->flags = LittleLong (pinmodel->flags);

//
// endian-adjust and copy the data, starting with the alias model header
//
	pmodel->boundingradius = LittleFloat (pinmodel->boundingradius);
	pmodel->numskins = LittleLong (pinmodel->numskins);
	pmodel->skinwidth = LittleLong (pinmodel->skinwidth);
	pmodel->skinheight = LittleLong (pinmodel->skinheight);

	if (pmodel->skinheight > MAX_LBM_HEIGHT)
	{
		// TODO: at least downsize the skin
		Con_Printf (S_ERROR "model %s has a skin taller than %d\n", mod->name,
				   MAX_LBM_HEIGHT);
		Hunk_FreeToLowMark(start);
		return false;
	}

	pmodel->numstverts = pmodel->numverts = LittleLong (pinmodel->numverts);

	if (pmodel->numverts <= 0)
	{
		Con_Printf (S_ERROR "model %s has no vertices\n", mod->name);
		Hunk_FreeToLowMark(start);
		return false;
	}

	if (pmodel->numverts > MAXALIASVERTS)
	{
		Con_Printf (S_ERROR "model %s has too many vertices\n", mod->name);
		Hunk_FreeToLowMark(start);
		return false;
	}

	pmodel->numtris = LittleLong (pinmodel->numtris);

	if (pmodel->numtris <= 0)
	{
		Con_Printf (S_ERROR "model %s has no triangles\n", mod->name);
		Hunk_FreeToLowMark(start);
		return false;
	}

	pmodel->numframes = LittleLong (pinmodel->numframes);
	if (qtest)
		pmodel->size = 1.0 * ALIAS_BASE_SIZE_RATIO;
	else
		pmodel->size = LittleFloat (pinmodel->size) * ALIAS_BASE_SIZE_RATIO;
	mod->synctype = LittleLong (pinmodel->synctype);
	mod->numframes = pmodel->numframes;

	for (i=0 ; i<3 ; i++)
	{
		pmodel->scale[i] = LittleFloat (pinmodel->scale[i]);
		pmodel->scale_origin[i] = LittleFloat (pinmodel->scale_origin[i]);
		pmodel->eyeposition[i] = LittleFloat (pinmodel->eyeposition[i]);
	}

	numskins = pmodel->numskins;
	numframes = pmodel->numframes;

	if (pmodel->skinwidth & 0x03)
	{
		Con_Printf (S_ERROR "Mod_LoadAliasModel: \"%s\" skinwidth not multiple of 4\n", loadmodel->name);
		Hunk_FreeToLowMark(start);
		return false;
	}

	pheader->model = (qbyte *)pmodel - (qbyte *)pheader;

//
// load the skins
//
	skinsize = pmodel->skinheight * pmodel->skinwidth;

	if (numskins < 1)
	{
		Con_Printf (S_ERROR "Mod_LoadAliasModel: %s, invalid # of skins: %d\n", loadmodel->name, numskins);
		Hunk_FreeToLowMark(start);
		return false;
	}

	if (qtest)
		pskintype = (daliasskintype_t *)((char *)&pinmodel[1] - sizeof(int)*2);
	else
		pskintype = (daliasskintype_t *)&pinmodel[1];

	pskindesc = Hunk_AllocName (numskins * sizeof (maliasskindesc_t),
								loadname);

	pheader->skindesc = (qbyte *)pskindesc - (qbyte *)pheader;

	for (i=0 ; i<numskins ; i++)
	{
		aliasskintype_t	skintype;

		skintype = LittleLong (pskintype->type);
		pskindesc[i].type = skintype;

		if (skintype == ALIAS_SKIN_SINGLE)
		{
			pskintype = (daliasskintype_t *)
					SWMod_LoadAliasSkin (pskintype + 1,
									   &pskindesc[i].skin,
									   skinsize, pheader);
		}
		else
		{
			pskintype = (daliasskintype_t *)
					SWMod_LoadAliasSkinGroup (pskintype + 1,
											&pskindesc[i].skin,
											skinsize, pheader);
		}
	}

//
// set base s and t vertices
//
	pstverts = (mstvert_t *)&pmodel[1];
	pinstverts = (dstvert_t *)pskintype;

	pheader->stverts = (qbyte *)pstverts - (qbyte *)pheader;

	for (i=0 ; i<pmodel->numverts ; i++)	//fixme: really, we only need to duplicate the onseem ones.
	{
	// put s and t in 16.16 format
		pstverts[i].s = LittleLong (pinstverts[i].s) << 16;
		pstverts[i].t = LittleLong (pinstverts[i].t) << 16;

		if (LittleLong (pinstverts[i].onseam))
			pstverts[i+pmodel->numverts].s = pstverts[i].s + ((pmodel->skinwidth>>1) << 16);
		else
			pstverts[i+pmodel->numverts].s = pstverts[i].s;	//FIXME: prevent duplication.
		pstverts[i+pmodel->numverts].t = pstverts[i].t;
	}

	pmodel->numstverts = pmodel->numverts*2;

//
// set up the triangles
//
	ptri = (mtriangle_t *)&pstverts[pmodel->numstverts];
	pintriangles = (dtriangle_t *)&pinstverts[pmodel->numverts];

	pheader->triangles = (qbyte *)ptri - (qbyte *)pheader;

	for (i=0 ; i<pmodel->numtris ; i++)
	{
		int		j;

		for (j=0 ; j<3 ; j++)
		{
			ptri[i].xyz_index[j] =
					LittleLong (pintriangles[i].vertindex[j]);
			if (LittleLong (pintriangles[i].facesfront))
				ptri[i].st_index[j] = ptri[i].xyz_index[j];
			else
				ptri[i].st_index[j] = ptri[i].xyz_index[j]+pmodel->numverts;
		}
	}

//
// load the frames
//
	if (numframes < 1)
	{
		Con_Printf (S_ERROR "Mod_LoadAliasModel: %s, invalid # of frames: %d\n", mod->name, numframes);
		Hunk_FreeToLowMark(start);
		return false;
	}

	pframetype = (daliasframetype_t *)&pintriangles[pmodel->numtris];

	for (i=0 ; i<numframes ; i++)
	{
		aliasframetype_t	frametype;

		frametype = LittleLong (pframetype->type);
		pheader->frames[i].type = frametype;
		VectorCopy(pmodel->scale_origin, pheader->frames[i].scale_origin);
		VectorCopy(pmodel->scale, pheader->frames[i].scale);

		if (qtest)
		{
			pframetype = (daliasframetype_t *)
					SWMod_LoadAliasQTestFrame (pframetype + 1,
										&pheader->frames[i].frame,
										pmodel->numverts,
										&pheader->frames[i].bboxmin,
										&pheader->frames[i].bboxmax,
										pheader, pheader->frames[i].name);
		}
		else if (frametype == ALIAS_SINGLE)
		{
			pframetype = (daliasframetype_t *)
					SWMod_LoadAliasFrame (pframetype + 1,
										&pheader->frames[i].frame,
										pmodel->numverts,
										&pheader->frames[i].bboxmin,
										&pheader->frames[i].bboxmax,
										pheader, pheader->frames[i].name);
		}
		else
		{
			pframetype = (daliasframetype_t *)
					SWMod_LoadAliasGroup (pframetype + 1,
										&pheader->frames[i].frame,
										pmodel->numverts,
										&pheader->frames[i].bboxmin,
										&pheader->frames[i].bboxmax,
										pheader, pheader->frames[i].name);
		}

		if (pframetype == NULL)
		{
			Con_Printf (S_ERROR "SWMod_LoadAliasModel: %s, couldn't load frame data\n", mod->name);
			Hunk_FreeToLowMark(start);
			return false;
		}
	}

	mod->type = mod_alias;

// FIXME: do this right

	/*
	mod->mins[0] = mod->mins[1] = mod->mins[2] = -16;
	mod->maxs[0] = mod->maxs[1] = mod->maxs[2] = 16;
	*/
	VectorCopy (pmodel->scale_origin, mod->mins);
	VectorMA (mod->mins, 255, pmodel->scale, mod->maxs);

//
// move the complete, relocatable alias model to the cache
//	
	end = Hunk_LowMark ();
	total = end - start;

	Hunk_Check();
	
	Cache_Alloc (&mod->cache, total, loadname);
	if (!mod->cache.data)
		return false;
	memcpy (mod->cache.data, pheader, total);

	Hunk_FreeToLowMark (start);
	return true;
}

typedef struct
{
	float		scale[3];	// multiply qbyte verts by this
	float		translate[3];	// then add this
	char		name[16];	// frame name from grabbing
	dtrivertx_t	verts[1];	// variable sized
} dmd2aliasframe_t;

qboolean SWMod_LoadAlias2Model (model_t *mod, void *buffer)
{
	int					i, j;
	mmdl_t				*pmodel;
	md2_t				*pinmodel;
	mstvert_t			*pstverts;
	dmd2stvert_t		*pinstverts;
	aliashdr_t			*pheader;
	mtriangle_t			*ptri;
	dmd2triangle_t		*pintriangles;
	int					version, numframes, numskins;
	int					size;
	dmd2aliasframe_t	*pinframe;
	maliasframedesc_t	*poutframe;
	maliasskindesc_t	*pskindesc;
	int					skinsize;
	int					start, end, total;
	dtrivertx_t			*frameverts;

	vec3_t				mins, maxs;


	if (loadmodel->engineflags & MDLF_DOCRC)
	{
		unsigned short crc;
		qbyte *p;
		int len;
		char st[40];

		QCRC_Init(&crc);
		for (len = com_filesize, p = buffer; len; len--, p++)
			QCRC_ProcessByte(&crc, *p);
	
		sprintf(st, "%d", (int) crc);
		Info_SetValueForKey (cls.userinfo, 
			(loadmodel->engineflags & MDLF_PLAYER) ? pmodel_name : emodel_name,
			st, MAX_INFO_STRING);

		if (cls.state >= ca_connected)
		{
			CL_SendClientCommand(true, "setinfo %s %d", 
				(loadmodel->engineflags & MDLF_PLAYER) ? pmodel_name : emodel_name,
				(int)crc);
		}
	}

	start = Hunk_LowMark ();

	pinmodel = (md2_t *)buffer;

	version = LittleLong (pinmodel->version);
	if (version != MD2ALIAS_VERSION)
	{
		Con_Printf (S_ERROR "%s has wrong version number (%i should be %i)\n",
				 mod->name, version, MD2ALIAS_VERSION);
		return false;
	}

//
// allocate space for a working header, plus all the data except the frames,
// skin and group info
//
	size = 	sizeof (aliashdr_t) + (LittleLong (pinmodel->num_frames) - 1) *
			 sizeof (pheader->frames[0]) +
			sizeof (mmdl_t) +
			LittleLong (pinmodel->num_st) * sizeof (mstvert_t) +
			LittleLong (pinmodel->num_tris) * sizeof (mtriangle_t);

	pheader = Hunk_AllocName (size, loadname);
	pmodel = (mmdl_t *) ((qbyte *)&pheader[1] +
			(LittleLong (pinmodel->num_frames) - 1) *
			 sizeof (pheader->frames[0]));
	
	mod->flags = 0;//LittleLong (pinmodel->flags);

//
// endian-adjust and copy the data, starting with the alias model header
//
	pmodel->boundingradius = 100;//LittleFloat (pinmodel->boundingradius);
	pmodel->numskins = LittleLong (pinmodel->num_skins);
	pmodel->skinwidth = LittleLong (pinmodel->skinwidth);
	pmodel->skinheight = LittleLong (pinmodel->skinheight);

	if (pmodel->skinheight > MAX_LBM_HEIGHT)
	{
		Con_Printf (S_ERROR "model %s has a skin taller than %d\n", mod->name,
				   MAX_LBM_HEIGHT);
		Hunk_FreeToLowMark(start);
		return false;
	}

	pmodel->numverts = LittleLong (pinmodel->num_xyz);
	pmodel->numstverts = LittleLong (pinmodel->num_st);

	if (pmodel->numverts <= 0)
	{
		Con_Printf (S_ERROR "model %s has no vertices\n", mod->name);
		Hunk_FreeToLowMark(start);
		return false;
	}

	if (pmodel->numverts > MAXALIASVERTS)
	{
		Con_Printf (S_ERROR "model %s has too many vertices\n", mod->name);
		Hunk_FreeToLowMark(start);
		return false;
	}

	pmodel->numtris = LittleLong (pinmodel->num_tris);

	if (pmodel->numtris <= 0)
	{
		Con_Printf (S_ERROR "model %s has no triangles\n", mod->name);
		Hunk_FreeToLowMark(start);
		return false;
	}

	pmodel->numframes = LittleLong (pinmodel->num_frames);
	pmodel->size = 1000;//LittleFloat (pinmodel->size) * ALIAS_BASE_SIZE_RATIO;
	mod->synctype = 1;//LittleLong (pinmodel->synctype);
	mod->numframes = pmodel->numframes;

	for (i=0 ; i<3 ; i++)
	{
		pmodel->scale[i] = 0;
		pmodel->scale_origin[i] = 0;
		pmodel->eyeposition[i] = 0;
	}

	numskins = pmodel->numskins;
	numframes = pmodel->numframes;

	if (pmodel->skinwidth & 0x03)
	{
		Con_Printf (S_ERROR "Mod_LoadAliasModel: %s, skinwidth not multiple of 4\n", mod->name);
		Hunk_FreeToLowMark(start);
		return false;
	}

	pheader->model = (qbyte *)pmodel - (qbyte *)pheader;

//
// set base s and t vertices
//
	pstverts = (mstvert_t *)&pmodel[1];
	pinstverts = (dmd2stvert_t *)((qbyte *)pinmodel + LittleLong(pinmodel->ofs_st));

	pheader->stverts = (qbyte *)pstverts - (qbyte *)pheader;

	for (i=0 ; i<pmodel->numstverts ; i++)
	{
	// put s and t in 16.16 format
		pstverts[i].s = LittleShort (pinstverts[i].s)<<16;
		pstverts[i].t = LittleShort (pinstverts[i].t)<<16;
	}

//
// set up the triangles
//
	ptri = (mtriangle_t *)&pstverts[pmodel->numstverts];
	pintriangles = (dmd2triangle_t *)((qbyte *)pinmodel + LittleLong(pinmodel->ofs_tris));

	pheader->triangles = (qbyte *)ptri - (qbyte *)pheader;

	for (i=0 ; i<pmodel->numtris ; i++)
	{
		int		j;

		for (j=0 ; j<3 ; j++)
		{
			ptri[i].xyz_index[j]	= LittleShort (pintriangles[i].xyz_index[j]);
			ptri[i].st_index[j]		= LittleShort (pintriangles[i].st_index[j]);
		}
	}

//
// load the frames
//
	if (numframes < 1)
	{
		Con_Printf (S_ERROR "Mod_LoadAliasModel: %s, Invalid # of frames: %d\n", mod->name, numframes);
		Hunk_FreeToLowMark(start);
		return false;
	}

	for (i=0 ; i<numframes ; i++)
	{
		pinframe = (dmd2aliasframe_t *) ((qbyte *)pinmodel 
			+ LittleLong(pinmodel->ofs_frames) + i * LittleLong(pinmodel->framesize));
		poutframe = &pheader->frames[i];

		memcpy (poutframe->name, pinframe->name, sizeof(poutframe->name));

		for (j=0 ; j<3 ; j++)
		{
			poutframe->scale[j] = LittleFloat (pinframe->scale[j]);
			poutframe->scale_origin[j] = LittleFloat (pinframe->translate[j]);
		}
		VectorCopy (poutframe->scale_origin, mins);	//work out precise size.
		VectorMA (mins, 255, poutframe->scale, maxs);
		poutframe->bboxmin.v[0] = poutframe->bboxmin.v[1] = poutframe->bboxmin.v[2] = 0;
		poutframe->bboxmax.v[0] = poutframe->bboxmax.v[1] = poutframe->bboxmax.v[2] = 255;
		
		if (i == 0)
		{
			VectorCopy (mins, mod->mins);	//first frame - nothing to compare against.
			VectorCopy (maxs, mod->maxs);
		}
		else
		{
			for (j = 0; j < 3; j++)
			{
				if (mod->mins[j] > mins[j])	//and make sure that the biggest ends up as the model size.
					mod->mins[j] = mins[j];
				if (mod->maxs[j] < maxs[j])
					mod->maxs[j] = maxs[j];
			}
		}

		// verts are all 8 bit, so no swapping needed
		frameverts = Hunk_AllocName(pmodel->numverts*sizeof(dtrivertx_t), loadname);
		poutframe->frame = (qbyte *)frameverts - (qbyte *)pheader;
		for (j = 0; j < pmodel->numverts; j++)
		{
			frameverts[j].lightnormalindex = pinframe->verts[j].lightnormalindex;
			frameverts[j].v[0] = pinframe->verts[j].v[0];
			frameverts[j].v[1] = pinframe->verts[j].v[1];
			frameverts[j].v[2] = pinframe->verts[j].v[2];
		}

	}

	VectorCopy (mod->mins, pmodel->scale_origin);	//work out global scale
	pmodel->scale[0] = (mod->maxs[0] - mod->mins[0])/255;
	pmodel->scale[1] = (mod->maxs[1] - mod->mins[1])/255;
	pmodel->scale[2] = (mod->maxs[2] - mod->mins[2])/255;

	{
		int width;
		int height;
		int j;
		qbyte *buffer;
		qbyte *texture;
		char *skinnames;
		qbyte *skin;
		pmodel->numskins = 0;
		skinnames = Hunk_AllocName(numskins*MAX_SKINNAME, loadname);
		memcpy(skinnames, (qbyte *)pinmodel + LittleLong(pinmodel->ofs_skins), numskins*MAX_SKINNAME);

		skinsize = pmodel->skinheight * pmodel->skinwidth;
		pskindesc = Hunk_AllocName (numskins * sizeof (maliasskindesc_t),
								loadname);
		pheader->skindesc = (qbyte *)pskindesc - (qbyte *)pheader;

		for (i=0 ; i<numskins ; i++, skinnames+=MAX_SKINNAME)
		{
			buffer = COM_LoadTempFile(skinnames);
			if (!buffer)
			{
				Con_Printf(S_WARNING "Skin %s not found\n", skinnames);
				continue;
			}
			texture = ReadPCXFile(buffer, com_filesize, &width, &height);
//			BZ_Free(buffer);
			if (!texture)
			{
				Con_Printf(S_WARNING "Skin %s not a pcx\n", skinnames);
				continue;
			}
			if (width != pmodel->skinwidth || height != pmodel->skinheight)	//FIXME: scale
			{
				BZ_Free(texture);
				Con_Printf(S_WARNING "Skin %s not same size as model specifies it should be\n", skinnames);
				continue;
			}

			skin = Hunk_AllocName(skinsize*r_pixbytes, loadname);
			if (r_pixbytes == 4)
			{
				for (j = 0; j < skinsize*4; j+=4)
				{
					skin[j+0] = texture[j+2];
					skin[j+1] = texture[j+1];
					skin[j+2] = texture[j+0];
					skin[j+3] = texture[j+3];
				}
			}
			else
			{
				for (j = 0; j < skinsize; j++)	//you know when you've been palettized.
				{
					skin[j+0] = GetPaletteNoFB(texture[j*4+0], texture[j*4+1], texture[j*4+2]);
				}
			}

			BZ_Free(texture);

			pskindesc[pmodel->numskins].type = ALIAS_SKIN_SINGLE;
			pskindesc[pmodel->numskins].skin = (qbyte *)skin - (qbyte *)pheader;
			pmodel->numskins++;
		}
	}

	mod->type = mod_alias;

//
// move the complete, relocatable alias model to the cache
//	
	end = Hunk_LowMark ();
	total = end - start;
	
	Cache_Alloc (&mod->cache, total, loadname);
	if (!mod->cache.data)
		return false;
	memcpy (mod->cache.data, pheader, total);

	Hunk_FreeToLowMark (start);
	return true;
}


//structures from Tenebrae
typedef struct {
	int			ident;
	int			version;

	char		name[MAX_QPATH];

	int			flags;	//Does anyone know what these are?

	int			numFrames;
	int			numTags;			
	int			numSurfaces;

	int			numSkins;

	int			ofsFrames;
	int			ofsTags;
	int			ofsSurfaces;
	int			ofsEnd;
} md3Header_t;

//then has header->numFrames of these at header->ofs_Frames
typedef struct md3Frame_s {
	vec3_t		bounds[2];
	vec3_t		localOrigin;
	float		radius;
	char		name[16];
} md3Frame_t;

//there are header->numSurfaces of these at header->ofsSurfaces, following from ofsEnd
typedef struct {
	int		ident;				// 

	char	name[MAX_QPATH];	// polyset name

	int		flags;
	int		numFrames;			// all surfaces in a model should have the same

	int		numShaders;			// all surfaces in a model should have the same
	int		numVerts;

	int		numTriangles;
	int		ofsTriangles;

	int		ofsShaders;			// offset from start of md3Surface_t
	int		ofsSt;				// texture coords are common for all frames
	int		ofsXyzNormals;		// numVerts * numFrames

	int		ofsEnd;				// next surface follows
} md3Surface_t;

//at surf+surf->ofsXyzNormals
typedef struct {
	short		xyz[3];
	unsigned short normal;
} md3XyzNormal_t;

//surf->numTriangles at surf+surf->ofsTriangles
typedef struct {
	int			indexes[3];
} md3Triangle_t;

//surf->numVerts at surf+surf->ofsSt
typedef struct {
	float		s;
	float		t;
} md3St_t;

typedef struct {
	char			name[MAX_QPATH];
	int				shaderIndex;
} md3Shader_t;
//End of Tenebrae 'assistance'

typedef struct {
	char name[MAX_QPATH];
	vec3_t org;
	float ang[3][3];
} md3tag_t;

qbyte *LoadTextureFile(char *texturename)
{
	qbyte *tex;
	if ((tex = COM_LoadMallocFile(texturename)))
		return tex;
	if ((tex = COM_LoadMallocFile(va("textures/%s.tga", texturename))))
		return tex;
	if ((tex = COM_LoadMallocFile(va("textures/%s.jpg", texturename))))
		return tex;
	if ((tex = COM_LoadMallocFile(va("%s.tga", texturename))))
		return tex;
	if ((tex = COM_LoadMallocFile(va("%s.jpg", texturename))))
		return tex;

	return NULL;
}

qboolean SWMod_LoadAlias3Model (model_t *mod, void *buffer)
{
	int					i, j;
	mmdl_t				*pmodel;
	aliashdr_t			*pheader;

	md3Header_t			*pinmodel;
	mstvert_t			*pstverts;
	md3St_t				*pinstverts;
	mtriangle_t			*ptri;
	md3Triangle_t		*pintriangles;
	int					version, numframes, numskins;
	int					size;
	md3Frame_t	*pinframe;
	maliasframedesc_t	*poutframe;
	maliasskindesc_t	*pskindesc;
	int					skinsize;
	int					start, end, total;
	dtrivertx_t 		*frameverts;
	md3XyzNormal_t		*pinverts;

	md3Surface_t		*surface;

	vec3_t				mins, maxs;

	if (loadmodel->engineflags & MDLF_DOCRC)
	{
		unsigned short crc;
		qbyte *p;
		int len;
		char st[40];

		QCRC_Init(&crc);
		for (len = com_filesize, p = buffer; len; len--, p++)
			QCRC_ProcessByte(&crc, *p);
	
		sprintf(st, "%d", (int) crc);
		Info_SetValueForKey (cls.userinfo, 
			(loadmodel->engineflags & MDLF_PLAYER) ? pmodel_name : emodel_name,
			st, MAX_INFO_STRING);

		if (cls.state >= ca_connected)
		{
			CL_SendClientCommand(true, "setinfo %s %d", 
				(loadmodel->engineflags & MDLF_PLAYER) ? pmodel_name : emodel_name,
				(int)crc);
		}
	}

	start = Hunk_LowMark ();

	pinmodel = (md3Header_t *)buffer;

	version = LittleLong (pinmodel->version);
//	if (version != MD3ALIAS_VERSION)
//		Sys_Error ("%s has wrong version number (%i should be %i)",
//				 mod->name, version, MD3ALIAS_VERSION);

	surface = (md3Surface_t*)((qbyte *)pinmodel + pinmodel->ofsSurfaces);

//
// allocate space for a working header, plus all the data except the frames,
// skin and group info
//
	size = 	sizeof (aliashdr_t) + (LittleLong (pinmodel->numFrames) - 1) *
			 sizeof (pheader->frames[0]) +
			sizeof (mmdl_t) +
			LittleLong (surface->numVerts) * sizeof (mstvert_t) +
			LittleLong (surface->numTriangles) * sizeof (mtriangle_t);

	pheader = Hunk_AllocName (size, loadname);
	pmodel = (mmdl_t *) ((qbyte *)&pheader[1] +
			(LittleLong (surface->numFrames) - 1) *
			 sizeof (pheader->frames[0]));
	
	mod->flags = 0;//LittleLong (pinmodel->flags);

//
// endian-adjust and copy the data, starting with the alias model header
//
	pmodel->boundingradius = 100;//LittleFloat (pinmodel->boundingradius);
	pmodel->numskins = LittleLong (surface->numShaders);
//	pmodel->skinwidth = LittleLong (pinmodel->skinwidth);	//fill in later.
//	pmodel->skinheight = LittleLong (pinmodel->skinheight);

	if (pmodel->skinheight > MAX_LBM_HEIGHT)
	{
		Con_Printf (S_ERROR "model %s has a skin taller than %d\n", mod->name,
				   MAX_LBM_HEIGHT);
		Hunk_FreeToLowMark(start);
		return false;
	}

	pmodel->numverts = LittleLong (surface->numVerts);
	pmodel->numstverts = LittleLong (surface->numVerts);

	if (surface->numVerts <= 0)
	{
		Con_Printf (S_ERROR "model %s has no vertices\n", mod->name);
		Hunk_FreeToLowMark(start);
		return false;
	}

	if (pmodel->numverts > MAXALIASVERTS)
	{
		Con_Printf (S_ERROR "model %s has too many vertices\n", mod->name);
		Hunk_FreeToLowMark(start);
		return false;
	}

	pmodel->numtris = LittleLong (surface->numTriangles);

	if (pmodel->numtris <= 0)
	{
		Con_Printf (S_ERROR "model %s has no triangles\n", mod->name);
		Hunk_FreeToLowMark(start);
		return false;
	}

	pmodel->numframes = LittleLong (surface->numFrames);
	pmodel->size = 1000;//LittleFloat (pinmodel->size) * ALIAS_BASE_SIZE_RATIO;
	mod->synctype = 1;//LittleLong (pinmodel->synctype);
	mod->numframes = pmodel->numframes;

	for (i=0 ; i<3 ; i++)
	{
		pmodel->scale[i] = 0;
		pmodel->scale_origin[i] = 0;
		pmodel->eyeposition[i] = 0;
	}

	numskins = pmodel->numskins;
	numframes = pmodel->numframes;


	pheader->model = (qbyte *)pmodel - (qbyte *)pheader;
	
	{
		int width;
		int height;
		int j;
		qbyte *buffer;
		qbyte *texture;
		md3Shader_t *pinskin;
		qbyte *skin;
		pmodel->numskins = 0;
		
		pinskin = (md3Shader_t *)((qbyte *)surface + surface->ofsShaders);

		skinsize = pmodel->skinheight * pmodel->skinwidth;
		pskindesc = Hunk_AllocName (numskins * sizeof (maliasskindesc_t),
								loadname);
		pheader->skindesc = (qbyte *)pskindesc - (qbyte *)pheader;

		for (i=0 ; i<numskins ; i++, pinskin++)
		{
			buffer = LoadTextureFile(pinskin->name);
			if (!buffer)
			{
				char altname[256];
				strcpy(altname, mod->name);	//backup
				strcpy(COM_SkipPath(altname), COM_SkipPath(pinskin->name));
				buffer = LoadTextureFile(altname);
			}

			if (!buffer)
			{
				Con_Printf(S_WARNING "Skin %s not found\n", pinskin->name);
				continue;
			}
			texture = ReadTargaFile(buffer, com_filesize, &width, &height, false);
#ifdef AVAIL_JPEGLIB
			if (!texture)
				texture = ReadJPEGFile(buffer, com_filesize, &width, &height);
#endif
#ifdef AVAIL_PNGLIB
			if (!texture)
				texture = ReadPNGFile(buffer, com_filesize, &width, &height, pinskin->name);
#endif
			if (!texture)
				texture = ReadPCXFile(buffer, com_filesize, &width, &height);
			BZ_Free(buffer);
			if (!texture)
			{
				Con_Printf(S_WARNING "Skin %s filetype not recognised\n", pinskin->name);
				continue;
			}
			if (!pmodel->numskins)	//this is the first skin.
			{
				pmodel->skinwidth = width;
				pmodel->skinheight = height;
			}
			skinsize = width*height;
			if (width != pmodel->skinwidth || height != pmodel->skinheight)	//FIXME: scale
			{
				BZ_Free(texture);
				Con_Printf(S_WARNING "Skin %s not same size as model specifies it should be\n", pinskin->name);
				continue;
			}

			skin = Hunk_AllocName(skinsize*r_pixbytes, loadname);
			if (r_pixbytes == 4)
			{
				for (j = 0; j < skinsize*4; j+=4)
				{
					skin[j+0] = texture[j+2];
					skin[j+1] = texture[j+1];
					skin[j+2] = texture[j+0];
					skin[j+3] = texture[j+3];
				}
			}
			else
			{
				for (j = 0; j < skinsize; j++)	//you know when you've been palettized.
				{
					skin[j+0] = GetPaletteNoFB(texture[j*4+0], texture[j*4+1], texture[j*4+2]);
				}
			}

			BZ_Free(texture);

			pskindesc[pmodel->numskins].type = ALIAS_SKIN_SINGLE;
			pskindesc[pmodel->numskins].skin = (qbyte *)skin - (qbyte *)pheader;
			pmodel->numskins++;
		}
	}

	if (!pmodel->numskins)
		Con_Printf(S_WARNING "model %s has no skins\n", loadmodel->name);

	if (pmodel->skinwidth & 0x03)
	{
		Con_Printf (S_ERROR "Mod_LoadAliasModel: %s, skinwidth not multiple of 4\n", mod->name);
		Hunk_FreeToLowMark(start);
		return false;
	}

//
// set base s and t vertices
//
	pstverts = (mstvert_t *)&pmodel[1];
	pinstverts = (md3St_t *)((qbyte *)surface + LittleLong(surface->ofsSt));

	pheader->stverts = (qbyte *)pstverts - (qbyte *)pheader;

	for (i=0 ; i<pmodel->numstverts ; i++)
	{
	// put s and t in 16.16 format
		pstverts[i].s = (int)(LittleFloat (pinstverts[i].s)*pmodel->skinwidth)<<16;
		pstverts[i].t = (int)(LittleFloat (pinstverts[i].t)*pmodel->skinheight)<<16;
	}

//
// set up the triangles
//
	ptri = (mtriangle_t *)&pstverts[pmodel->numstverts];
	pintriangles = (md3Triangle_t *)((qbyte *)surface + LittleLong(surface->ofsTriangles));

	pheader->triangles = (qbyte *)ptri - (qbyte *)pheader;

	for (i=0 ; i<pmodel->numtris ; i++)
	{
		int		j;

		for (j=0 ; j<3 ; j++)
		{
			ptri[i].st_index[j] = ptri[i].xyz_index[j] = LittleLong (pintriangles[i].indexes[j]);
		}
	}

//
// load the frames
//
	if (numframes < 1)
	{
		Con_Printf (S_ERROR "Mod_LoadAliasModel: %s, Invalid # of frames: %d\n", mod->name, numframes);
		Hunk_FreeToLowMark(start);
		return false;
	}

	pinverts = (md3XyzNormal_t *)((qbyte *)surface + surface->ofsXyzNormals);
	for (i=0 ; i<numframes ; i++)
	{
		pinframe = (md3Frame_t *) ((qbyte *)pinmodel 
			+ LittleLong(pinmodel->ofsFrames) + i * sizeof(md3Frame_t));
		poutframe = &pheader->frames[i];

		memcpy (poutframe->name, pinframe->name, sizeof(poutframe->name));

		for (j=0 ; j<3 ; j++)
		{
			poutframe->scale_origin[j] = LittleFloat (pinframe->bounds[0][j]);
			poutframe->scale[j] = (LittleFloat (pinframe->bounds[1][j])-poutframe->scale_origin[j])/255;
		}
		VectorCopy (poutframe->scale_origin, mins);	//work out precise size.
		VectorMA (mins, 255, poutframe->scale, maxs);
		poutframe->bboxmin.v[0] = poutframe->bboxmin.v[1] = poutframe->bboxmin.v[2] = 0;
		poutframe->bboxmax.v[0] = poutframe->bboxmax.v[1] = poutframe->bboxmax.v[2] = 255;
		
		if (i == 0)
		{
			VectorCopy (mins, mod->mins);	//first frame - nothing to compare against.
			VectorCopy (maxs, mod->maxs);
		}
		else
		{
			for (j = 0; j < 3; j++)
			{
				if (mod->mins[j] > mins[j])	//and make sure that the biggest ends up as the model size.
					mod->mins[j] = mins[j];
				if (mod->maxs[j] < maxs[j])
					mod->maxs[j] = maxs[j];
			}
		}

		// verts are all 8 bit, so no swapping needed
		frameverts = Hunk_AllocName(pmodel->numverts*sizeof(dtrivertx_t), loadname);
		poutframe->frame = (qbyte *)frameverts - (qbyte *)pheader;
		for (j = 0; j < pmodel->numverts; j++)
		{
			frameverts[j].lightnormalindex = LittleShort(pinverts->normal)/256;

			//scale down to 0 - 255 within the scaleorigin and scaleorigin+scale*255
			//	i/64 = scaleorigin+scale*o
			//	i/64-scaleorigin = scale*o
			//	i/64-scaleorigin/scale = 0
			frameverts[j].v[0] = ((double)LittleShort(pinverts[j].xyz[0])/64.0-poutframe->scale_origin[0])/(poutframe->scale[0]);
			frameverts[j].v[1] = ((double)LittleShort(pinverts[j].xyz[1])/64.0-poutframe->scale_origin[1])/(poutframe->scale[1]);
			frameverts[j].v[2] = ((double)LittleShort(pinverts[j].xyz[2])/64.0-poutframe->scale_origin[2])/(poutframe->scale[2]);
		}
		pinverts += pmodel->numverts;

	}

	VectorCopy (mod->mins, pmodel->scale_origin);	//work out global scale
	pmodel->scale[0] = (mod->maxs[0] - mod->mins[0])/255;
	pmodel->scale[1] = (mod->maxs[1] - mod->mins[1])/255;
	pmodel->scale[2] = (mod->maxs[2] - mod->mins[2])/255;

	mod->type = mod_alias;

//
// move the complete, relocatable alias model to the cache
//
	end = Hunk_LowMark ();
	total = end - start;
	
	Cache_Alloc (&mod->cache, total, loadname);
	if (!mod->cache.data)
		return false;
	memcpy (mod->cache.data, pheader, total);

	Hunk_FreeToLowMark (start);
#ifdef RGLQUAKE
	mod->flags = Mod_ReadFlagsFromMD1(mod->name, 0);
#endif

	return true;
}


//=============================================================================

/*
=================
Mod_LoadSpriteFrame
=================
*/
void * SWMod_LoadSpriteFrame (void * pin, mspriteframe_t **ppframe, int version)
{
	dspriteframe_t		*pinframe;
	mspriteframe_t		*pspriteframe;
	int					i, width, height, size, origin[2];

	pinframe = (dspriteframe_t *)pin;

	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	size = width * height;

	pspriteframe = Hunk_AllocName (sizeof (mspriteframe_t) + size*r_pixbytes,
								   loadname);

	Q_memset (pspriteframe, 0, sizeof (mspriteframe_t) + size);
	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong (pinframe->origin[0]);
	origin[1] = LittleLong (pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];

	if (r_pixbytes == 1)
	{
		qbyte		*ppixout, *ppixin;

		if (version == SPRITE32_VERSION)
		{	//downgrade quality

			ppixin = (unsigned char *)(pinframe + 1);
			ppixout = (unsigned char *)&pspriteframe->pixels[0];

			for (i=0 ; i<size ; i++)
			{
				if (ppixin[i*4+3] < 128)
					ppixout[i] = 255;	//transparent.
				else
					ppixout[i] = GetPaletteNoFB(ppixin[i*4], ppixin[i*4+1], ppixin[i*4+2]);
			}
			size *= 4;
		}
		else
			Q_memcpy (&pspriteframe->pixels[0], (qbyte *)(pinframe + 1), size);
	}
	else if (r_pixbytes == 2)
	{
		qbyte		*ppixin;
		unsigned short *p16out;
		if (version == SPRITE32_VERSION)
		{	//downgrade quality

			ppixin = (unsigned char *)(pinframe + 1);
			p16out = (unsigned short *)&pspriteframe->pixels[0];

			for (i=0 ; i<size ; i++)
			{
				if (ppixin[i*4+3] < 128)
					p16out[i] = 0xffff;	//transparent.
				else
					p16out[i] = ((ppixin[i*4]*32/255)<<10) + ((ppixin[i*4+1]*32/255)<<5) + ((ppixin[i*4+2]*32/255)<<0);
			}
			size *= 4;
		}
		else
		{
			ppixin = (unsigned char *)(pinframe + 1);
			p16out = (unsigned short *)&pspriteframe->pixels[0];
			for (i=0 ; i<size ; i++)
			{
				if (ppixin[i] == 255)
					p16out[i] = 0xffff;	//transparent.
				else
					p16out[i] = d_8to16table[ppixin[i]];
			}
		}
	}
	else if (r_pixbytes == 4)
	{
		unsigned int		*p32out;
		if (version == SPRITE32_VERSION)
		{	//copy accross
			unsigned int	*p32in;
			p32in = (unsigned int *)(pinframe + 1);
			p32out = (unsigned int *)&pspriteframe->pixels[0];

			for (i=0 ; i<size ; i++)
				p32out[i] = p32in[i];

			size *= 4;
		}
		else
		{	//upgrade
			qbyte	*ppixin;
			ppixin = (qbyte *)(pinframe + 1);
			p32out = (unsigned int *)&pspriteframe->pixels[0];

			for (i=0 ; i<size ; i++)
				p32out[i] = d_8to32table[ppixin[i]];
		}
	}
	else
	{
		Sys_Error ("Mod_LoadSpriteFrame: driver set invalid r_pixbytes: %d\n",
				 r_pixbytes);
	}

	return (void *)((qbyte *)pinframe + sizeof (dspriteframe_t) + size);
}


/*
=================
Mod_LoadSpriteGroup
=================
*/
void * SWMod_LoadSpriteGroup (void * pin, mspriteframe_t **ppframe, int version)
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
			Con_Printf (S_ERROR "Mod_LoadSpriteGroup: interval<=0\n");
			return NULL;
		}

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *)pin_intervals;

	for (i=0 ; i<numframes ; i++)
	{
		ptemp = SWMod_LoadSpriteFrame (ptemp, &pspritegroup->frames[i], version);
	}

	return ptemp;
}


/*
=================
Mod_LoadSpriteModel
=================
*/
qboolean SWMod_LoadSpriteModel (model_t *mod, void *buffer)
{
	int					i;
	int					version;
	dsprite_t			*pin;
	msprite_t			*psprite;
	int					numframes;
	int					size;
	dspriteframetype_t	*pframetype;
	int hunkstart;

	hunkstart = Hunk_LowMark();
	
	pin = (dsprite_t *)buffer;

	version = LittleLong (pin->version);
	if (version != SPRITE32_VERSION)
	if (version != SPRITE_VERSION)
	{
		Con_Printf (S_ERROR "%s has wrong version number "
				 "(%i should be %i)\n", mod->name, version, SPRITE_VERSION);
		return false;
	}

	numframes = LittleLong (pin->numframes);

	size = sizeof (msprite_t) +	(numframes - 1) * sizeof (psprite->frames);

	psprite = Hunk_AllocName (size, loadname);

	mod->cache.data = psprite;

	psprite->type = LittleLong (pin->type);
	psprite->maxwidth = LittleLong (pin->width);
	psprite->maxheight = LittleLong (pin->height);
	psprite->beamlength = LittleFloat (pin->beamlength);
	mod->synctype = LittleLong (pin->synctype);
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
		Con_Printf (S_ERROR "Mod_LoadSpriteModel: Invalid # of frames: %d\n", numframes);
		return false;
	}

	mod->numframes = numframes;

	pframetype = (dspriteframetype_t *)(pin + 1);

	for (i=0 ; i<numframes ; i++)
	{
		spriteframetype_t	frametype;

		frametype = LittleLong (pframetype->type);
		psprite->frames[i].type = frametype;

		if (frametype == SPR_SINGLE)
		{
			pframetype = (dspriteframetype_t *)
					SWMod_LoadSpriteFrame (pframetype + 1,
										 &psprite->frames[i].frameptr, version);
		}
		else
		{
			pframetype = (dspriteframetype_t *)
					SWMod_LoadSpriteGroup (pframetype + 1,
										 &psprite->frames[i].frameptr, version);
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

qboolean SWMod_LoadSprite2Model (model_t *mod, void *buffer)
{
	int					i, j;
	int					version;
	dmd2sprite_t		*pin;
	msprite_t			*psprite;
	int					numframes;
	int					size;
	dmd2sprframe_t		*pframetype;
	mspriteframe_t		*frame;
	float origin[2];
	int width;
	int height;
	qbyte *framefile;
	qbyte *framedata;
	int hunkstart;

	hunkstart = Hunk_LowMark();
	
	pin = (dmd2sprite_t *)buffer;

	version = LittleLong (pin->version);
	if (version != SPRITE2_VERSION)
	{
		Con_Printf ("%s has wrong version number "
				 "(%i should be %i)\n", mod->name, version, SPRITE2_VERSION);
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
		Con_Printf (S_ERROR "Mod_LoadSpriteModel: Invalid # of frames: %d\n", numframes);
		Hunk_FreeToLowMark(hunkstart);
		return false;
	}

	mod->numframes = 0;

	pframetype = pin->frames;
	for (i=0 ; i<numframes ; i++)
	{
		psprite->frames[mod->numframes].type = SPR_SINGLE;

		width = LittleLong(pframetype->width);
		height = LittleLong(pframetype->height);

		framefile = COM_LoadMallocFile(pframetype->name);
		if (!framefile)
		{
			Con_Printf("Couldn't open sprite frame %s\n", pframetype->name);
			continue;	//skip this frame - is this a bad idea?
		}
		framedata = ReadPCXFile(framefile, com_filesize, &width, &height);
		BZ_Free(framefile);
		if (!framedata)
		{
			Con_Printf("Sprite frame %s is not a pcx\n", pframetype->name);
			continue;	//skip this frame - is this a bad idea?
		}

		frame = psprite->frames[mod->numframes].frameptr = Hunk_AllocName(sizeof(mspriteframe_t)+width*r_pixbytes*height, loadname);

		frame->width = width;
		frame->height = height;
		origin[0] = LittleLong (pframetype->origin_x);
		origin[1] = LittleLong (pframetype->origin_y);

		frame->up = -origin[1];
		frame->down = frame->height - origin[1];
		frame->left = -origin[0];
		frame->right = frame->width - origin[0];

		if (r_pixbytes == 4)
		{
			for (j = 0; j < width*height; j++)
			{
				frame->pixels[j*4+0] = framedata[j*4+2];
				frame->pixels[j*4+1] = framedata[j*4+1];
				frame->pixels[j*4+2] = framedata[j*4+0];
				frame->pixels[j*4+3] = framedata[j*4+3];
			}
		}
		else
		{
			for (j = 0; j < width*height; j++)
			{
				if (!framedata[j*4+3])	//make sure
					frame->pixels[j] = 255;
				else
					frame->pixels[j] = GetPaletteNoFB(framedata[j*4+0], framedata[j*4+1], framedata[j*4+2]);
			}
		}
		BZ_Free(framedata);

		mod->numframes++;
	}

	mod->type = mod_sprite;
	return true;
}

//=============================================================================
#endif
/*
================
Mod_Print
================
*/
void SWMod_Print (void)
{
	int		i;
	model_t	*mod;

	Con_Printf ("Cached models:\n");
	for (i=0, mod=mod_known ; i < mod_numknown ; i++, mod++)
	{
		Con_Printf ("%8p : %s\n",mod->cache.data, mod->name);
	}
}


