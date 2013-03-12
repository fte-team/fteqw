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

#include "qwsvdef.h"

#ifdef SERVERONLY
model_t	*loadmodel;
char	loadname[32];	// for hunk tags

qboolean Terr_LoadTerrainModel (model_t *mod, void *buffer);
qboolean Mod_LoadBrushModel (model_t *mod, void *buffer);
qboolean Mod_LoadQ2BrushModel (model_t *mod, void *buffer);
qboolean D3_LoadMap_CollisionMap(model_t *mod, char *buf);

qboolean Mod_LoadQ1Model (model_t *mod, void *buffer);
qboolean Mod_LoadQ2Model (model_t *mod, void *buffer);
qboolean Mod_LoadQ3Model (model_t *mod, void *buffer);
qboolean Mod_LoadZymoticModel (model_t *mod, void *buffer);
qboolean Mod_LoadDarkPlacesModel(model_t *mod, void *buffer);

qbyte	mod_novis[MAX_MAP_LEAFS/8];

#define	MAX_MOD_KNOWN	512
model_t	mod_known[MAX_MOD_KNOWN];
int		mod_numknown;

texture_t	r_notexture_mip_real;
texture_t	*r_notexture_mip = &r_notexture_mip_real;

cvar_t sv_nogetlight = SCVAR("sv_nogetlight", "0");
cvar_t dpcompat_psa_ungroup					= CVAR  ("dpcompat_psa_ungroup", "0");
cvar_t r_noframegrouplerp					= CVARF  ("r_noframegrouplerp", "0", CVAR_ARCHIVE);

unsigned *model_checksum;


int SVQ1_RecursiveLightPoint3C (model_t *model, mnode_t *node, vec3_t start, vec3_t end)
{
	int			r;
	float		front, back, frac;
	int			side;
	mplane_t	*plane;
	vec3_t		mid;
	msurface_t	*surf;
	int			s, t, ds, dt;
	int			i;
	mtexinfo_t	*tex;
	qbyte		*lightmap;
	unsigned	scale;
	int			maps;


	if (model->fromgame == fg_quake2)
	{
		if (node->contents != -1)
			return -1;		// solid
	}
	else
	{
		if (node->contents < 0)
			return -1;		// didn't hit anything
	}
	
// calculate mid point

// FIXME: optimize for axial
	plane = node->plane;
	front = DotProduct (start, plane->normal) - plane->dist;
	back = DotProduct (end, plane->normal) - plane->dist;
	side = front < 0;
	
	if ( (back < 0) == side)
		return SVQ1_RecursiveLightPoint3C (model, node->children[side], start, end);
	
	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0])*frac;
	mid[1] = start[1] + (end[1] - start[1])*frac;
	mid[2] = start[2] + (end[2] - start[2])*frac;
	
// go down front side	
	r = SVQ1_RecursiveLightPoint3C (model, node->children[side], start, mid);
	if (r >= 0)
		return r;		// hit something
		
	if ( (back < 0) == side )
		return -1;		// didn't hit anuthing
		
// check for impact on this node

	surf = model->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->flags & SURF_DRAWTILED)
			continue;	// no lightmaps

		tex = surf->texinfo;
		
		s = DotProduct (mid, tex->vecs[0]) + tex->vecs[0][3];
		t = DotProduct (mid, tex->vecs[1]) + tex->vecs[1][3];

		if (s < surf->texturemins[0] ||
		t < surf->texturemins[1])
			continue;
		
		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];
		
		if ( ds > surf->extents[0] || dt > surf->extents[1] )
			continue;

		if (!surf->samples)
			return 0;

		lightmap = surf->samples;
		r = 0;
		if (lightmap)
		{

			lightmap += (dt * ((surf->extents[0])+1) + ds)*3;

			for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
					maps++)
			{
				scale = sv.strings.lightstyles[surf->styles[maps]][0];
				r += (lightmap[0]+lightmap[1]+lightmap[2])/3 * scale;
				lightmap += ((surf->extents[0])+1) *
						((surf->extents[1])+1)*3;
			}
			
			r >>= 8;
		}
		
		return r;
	}

// go down back side
	return SVQ1_RecursiveLightPoint3C (model, node->children[!side], mid, end);
}

void SVQ1_LightPointValues(model_t *model, vec3_t point, vec3_t res_diffuse, vec3_t res_ambient, vec3_t res_dir)
{
	vec3_t		end;
	float r;

	res_dir[0] = 0;	//software doesn't load luxes
	res_dir[1] = 1;
	res_dir[2] = 1;

	end[0] = point[0];
	end[1] = point[1];
	end[2] = point[2] - 2048;

	r = SVQ1_RecursiveLightPoint3C (model, model->nodes, point, end);
	if (r < 0)
	{
		res_diffuse[0] = 0;
		res_diffuse[1] = 0;
		res_diffuse[2] = 0;
	
		res_ambient[0] = 0;
		res_ambient[1] = 0;
		res_ambient[2] = 0;
	}
	else
	{
		res_diffuse[0] = r;
		res_diffuse[1] = r;
		res_diffuse[2] = r;
	
		res_ambient[0] = r;
		res_ambient[1] = r;
		res_ambient[2] = r;
	}
}



/*
===============
Mod_Init
===============
*/
void Mod_Init (void)
{
	memset (mod_novis, 0xff, sizeof(mod_novis));
	Cvar_Register(&sv_nogetlight, "Memory preservation");
	Cvar_Register (&dpcompat_psa_ungroup, "Darkplaces compatibility");
	Cvar_Register (&r_noframegrouplerp, "Oooga booga");
}

/*
===============
Mod_LeafForPoint
===============
*/
int Mod_LeafForPoint (model_t *model, vec3_t p)
{
	mnode_t		*node;
	float		d;
	mplane_t	*plane;
#ifdef Q2BSPS
	if (model->fromgame == fg_quake2 || model->fromgame == fg_quake3)
	{
		return CM_PointLeafnum(model, p);
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

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf (model_t *model, vec3_t p)
{
	return model->leafs + Mod_LeafForPoint(model, p);
}



/*
===================
Mod_DecompressVis
===================
*/
qbyte *Mod_DecompressVis (qbyte *in, model_t *model, qbyte *decompressed)
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

qbyte *Mod_LeafPVS (mleaf_t *leaf, model_t *model, qbyte *buffer)
{
	static qbyte	decompressed[MAX_MAP_LEAFS/8];

	if (leaf == model->leafs)
		return mod_novis;

	if (!buffer)
		buffer = decompressed;
	return Mod_DecompressVis (leaf->compressed_vis, model, buffer);
}

qbyte *Mod_LeafnumPVS (int ln, model_t *model, qbyte *buffer)
{
	return Mod_LeafPVS(model->leafs + ln, model, buffer);
}


/*
===================
Mod_ClearAll
===================
*/
void Mod_ClearAll (void)
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
model_t *Mod_FindName (char *name)
{
	int		i;
	model_t	*mod;

	if (!name[0])
		SV_Error ("Mod_ForName: NULL name");

//
// search the currently loaded models
//
	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
		if (!strcmp (mod->name, name) )
			break;

	if (i == mod_numknown)
	{
		if (mod_numknown == MAX_MOD_KNOWN)
			SV_Error ("mod_numknown == MAX_MOD_KNOWN");
		strcpy (mod->name, name);
		mod->needload = true;
		mod_numknown++;
	}

	return mod;
}


/*
==================
Mod_LoadModel

Loads a model into the cache
==================
*/
model_t *Mod_LoadModel (model_t *mod, qboolean crash)
{
	void	*d;
	unsigned *buf;
	qbyte	stackbuf[1024];		// avoid dirtying the cache heap

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

//
// load the file
//
	buf = (unsigned *)COM_LoadStackFile (mod->name, stackbuf, sizeof(stackbuf));
	if (!buf)
	{
		if (crash)
			SV_Error ("Mod_NumForName: %s not found", mod->name);
		return NULL;
	}

//
// allocate a new model
//
	COM_FileBase (mod->name, loadname, sizeof(loadname));

	loadmodel = mod;

//
// fill it in
//

// call the apropriate loader
	mod->needload = false;

	// set necessary engine flags for loading purposes
	if (!strcmp(mod->name, "progs/player.mdl"))
	{
		mod->engineflags |= MDLF_PLAYER | MDLF_DOCRC;
	}
	else if (!strcmp(mod->name, "progs/eyes.mdl"))
		mod->engineflags |= MDLF_DOCRC;

	switch (LittleLong(*(unsigned *)buf))
	{
#if defined(Q2BSPS)
	case IDBSPHEADER:	//looks like id switched to have proper ids
		if (!Mod_LoadQ2BrushModel (mod, buf))
			goto couldntload;
		break;
#endif

	case BSPVERSIONPREREL:
	case BSPVERSION:
	case BSPVERSIONHL:
		if (!Mod_LoadBrushModel (mod, buf))
			goto couldntload;
		break;

	case RAPOLYHEADER:
	case IDPOLYHEADER:
		if (!Mod_LoadQ1Model(mod, buf))
			goto couldntload;
		break;
#ifdef MD2MODELS
	case MD2IDALIASHEADER:
		if (!Mod_LoadQ2Model(mod, buf))
			goto couldntload;
		break;
#endif
#ifdef MD3MODELS
	case MD3_IDENT:
		if (!Mod_LoadQ3Model (mod, buf))
			goto couldntload;
		break;
#endif
#ifdef ZYMOTICMODELS
	case (('O'<<24)+('M'<<16)+('Y'<<8)+'Z'):
		if (!Mod_LoadZymoticModel(mod, buf))
			goto couldntload;
		break;
#endif
#ifdef ZYMOTICMODELS
	case (('K'<<24)+('R'<<16)+('A'<<8)+'D'):
		if (!Mod_LoadDarkPlacesModel(mod, buf))
			goto couldntload;
		break;
#endif

	default:
		COM_Parse((char*)buf);
#ifdef MAP_PROC
		if (!strcmp(com_token, "CM"))	//doom3 map.
		{
			if (!D3_LoadMap_CollisionMap (mod, (char*)buf))
				goto couldntload;
			break;
		}
#endif
#ifdef TERRAIN
		if (!strcmp(com_token, "terrain"))	//custom format, text based.
		{
			if (!Terr_LoadTerrainModel(mod, buf))
				goto couldntload;
			break;
		}
#endif

		Con_Printf (CON_ERROR "Mod_NumForName: %s: format not recognised\n", mod->name);
couldntload:
		if (crash)
			SV_Error ("Load failed on critical model %s", mod->name);
		return NULL;
	}

	return mod;
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t *Mod_ForName (char *name, qboolean crash)
{
	model_t	*mod;

	mod = Mod_FindName (name);

	return Mod_LoadModel (mod, crash);
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
void Mod_LoadTextures (lump_t *l)
{
	int		i, j, pixels, num, max, altmax;
	miptex_t	*mt;
	texture_t	*tx, *tx2;
	texture_t	*anims[10];
	texture_t	*altanims[10];
	dmiptexlump_t *m;

	if (loadmodel->fromgame != fg_quake)
		return;

	if (!l->filelen)
	{
		loadmodel->textures = NULL;
		return;
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
			SV_Error ("Texture %s is not 16 aligned", mt->name);
		pixels = mt->width*mt->height/64*85;
		tx = Hunk_AllocName (sizeof(texture_t) +pixels, loadname );
		loadmodel->textures[i] = tx;

		memcpy (tx->name, mt->name, sizeof(tx->name));
		tx->width = mt->width;
		tx->height = mt->height;
		for (j=0 ; j<MIPLEVELS ; j++)
			tx->offsets[j] = mt->offsets[j] + sizeof(texture_t) - sizeof(miptex_t);
		// the pixels immediately follow the structures
		memcpy ( tx+1, mt+1, pixels);
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
			SV_Error ("Bad animating texture %s", tx->name);

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
				SV_Error ("Bad animating texture %s", tx->name);
		}

#define	ANIM_CYCLE	2
	// link them all together
		for (j=0 ; j<max ; j++)
		{
			tx2 = anims[j];
			if (!tx2)
				SV_Error ("Missing frame %i of %s",j, tx->name);
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
				SV_Error ("Missing frame %i of %s",j, tx->name);
			tx2->anim_total = altmax * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j+1) * ANIM_CYCLE;
			tx2->anim_next = altanims[ (j+1)%altmax ];
			if (max)
				tx2->alternate_anims = anims[0];
		}
	}
}

/*
=================
Mod_LoadLighting
=================
*/
qboolean Mod_LoadLighting (lump_t *l)
{
	int i;
	char *in;
	char *out;
	if (!l->filelen)
	{
		loadmodel->lightdata = NULL;
		return true;
	}

	if (loadmodel->fromgame == fg_halflife)
	{
		loadmodel->lightdata = Hunk_AllocName ( l->filelen, loadname);
		memcpy (loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
	}
	else
	{
		loadmodel->lightdata = Hunk_AllocName ( l->filelen*3, loadname);

		in = mod_base + l->fileofs;
		out = loadmodel->lightdata;

		for (i = 0; i < l->filelen; i++)
		{
			*out++ = *in;
			*out++ = *in;
			*out++ = *in++;
		}
	}

	return true;
}


/*
=================
Mod_LoadVisibility
=================
*/
void Mod_LoadVisibility (lump_t *l)
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
void Mod_LoadEntities (lump_t *l)
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
qboolean Mod_LoadVertexes (lump_t *l)
{
	dvertex_t	*in;
	mvertex_t	*out;
	int			i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
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
qboolean Mod_LoadSubmodels (lump_t *l)
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
qboolean Mod_LoadEdges (lump_t *l, qboolean lm)
{
	medge_t *out;
	int 	i, count;

	if (lm)
	{
		dledge_t *in = (void *)(mod_base + l->fileofs);
		if (l->filelen % sizeof(*in))
		{
			Con_Printf ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
			return false;
		}
		count = l->filelen / sizeof(*in);
		out = Hunk_AllocName ( (count + 1) * sizeof(*out), loadname);

		loadmodel->edges = out;
		loadmodel->numedges = count;

		for ( i=0 ; i<count ; i++, in++, out++)
		{
			out->v[0] = (unsigned int)LittleLong(in->v[0]);
			out->v[1] = (unsigned int)LittleLong(in->v[1]);
		}
	}
	else
	{
		dsedge_t *in = (void *)(mod_base + l->fileofs);
		if (l->filelen % sizeof(*in))
		{
			Con_Printf ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
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
	}
	return true;
}

/*
=================
Mod_LoadTexinfo
=================
*/
qboolean Mod_LoadTexinfo (lump_t *l)
{
	texinfo_t *in;
	mtexinfo_t *out;
	int 	i, j, count;
	int		miptex;
	float	len1, len2;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
		return false;
	}
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
#if 0
		for (j=0 ; j<8 ; j++)
			out->vecs[0][j] = LittleFloat (in->vecs[0][j]);
		len1 = Length (in->vecs[0]);
		len2 = Length (in->vecs[1]);
#else
		for (j=0 ; j<4 ; j++) {
			out->vecs[0][j] = LittleFloat (in->vecs[0][j]);
			out->vecs[1][j] = LittleFloat (in->vecs[1][j]);
		}
		len1 = Length (out->vecs[0]);
		len2 = Length (out->vecs[1]);
#endif
		if (len1 + len2 < 2 /*0.001*/)
			out->mipadjust = 1;
		else
			out->mipadjust = 1 / floor( (len1+len2)/2 );

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
				SV_Error ("miptex >= loadmodel->numtextures");
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

		s->texturemins[i] = bmins[i];
		s->extents[i] = (bmaxs[i] - bmins[i]);
//		if ( !(tex->flags & TEX_SPECIAL) && s->extents[i] > 256)
//			SV_Error ("Bad surface extents");
	}
}
*/

/*
=================
Mod_LoadFaces
=================
*/
qboolean Mod_LoadFaces (lump_t *l, qboolean lm)
{
	dsface_t		*ins;
	dlface_t		*inl;
	msurface_t 	*out;
	int			i, count, surfnum;
	int			planenum, side;
	int tn, lofs;

	if (lm)
	{
		ins = NULL;
		inl = (void *)(mod_base + l->fileofs);
		if (l->filelen % sizeof(*inl))
		{
			Con_Printf ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
			return false;
		}
		count = l->filelen / sizeof(*inl);
	}
	else
	{
		inl = NULL;
		ins = (void *)(mod_base + l->fileofs);
		if (l->filelen % sizeof(*ins))
		{
			Con_Printf ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
			return false;
		}
		count = l->filelen / sizeof(*ins);
	}
	out = Hunk_AllocName ( count*sizeof(*out), loadname);

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for ( surfnum=0 ; surfnum<count ; surfnum++, out++)
	{
		if (lm)
		{
			planenum = LittleLong(inl->planenum);
			side = LittleLong(inl->side);
			out->firstedge = LittleLong(inl->firstedge);
			out->numedges = LittleLong(inl->numedges);
			tn = LittleLong (inl->texinfo);
			for (i=0 ; i<MAXLIGHTMAPS ; i++)
				out->styles[i] = inl->styles[i];
			lofs = LittleLong(inl->lightofs);
			inl++;
		}
		else
		{
			planenum = LittleShort(ins->planenum);
			side = LittleShort(ins->side);
			out->firstedge = LittleLong(ins->firstedge);
			out->numedges = LittleShort(ins->numedges);
			tn = LittleShort (ins->texinfo);
			for (i=0 ; i<MAXLIGHTMAPS ; i++)
				out->styles[i] = ins->styles[i];
			lofs = LittleLong(ins->lightofs);
			ins++;
		}

		out->flags = 0;

		if (side)
			out->flags |= SURF_PLANEBACK;

		out->plane = loadmodel->planes + planenum;

		out->texinfo = loadmodel->texinfo + tn;

		CalcSurfaceExtents (out);

	// lighting info
		if (lofs == -1)
			out->samples = NULL;
		else if (loadmodel->fromgame == fg_halflife)
			out->samples = loadmodel->lightdata + lofs;
		else
			out->samples = loadmodel->lightdata + lofs*3;

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
	}

	return true;
}


/*
=================
Mod_SetParent
=================
*/
void Mod_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents < 0)
		return;
	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}

/*
=================
Mod_LoadNodes
=================
*/
qboolean Mod_LoadNodes (lump_t *l, qboolean lm)
{
	int			i, j, count, p;
	mnode_t 	*out;

	if (lm)
	{
		dl1node_t		*in;
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

			out->firstsurface = LittleLong (in->firstface);
			out->numsurfaces = LittleLong (in->numfaces);

			for (j=0 ; j<2 ; j++)
			{
				p = LittleLong (in->children[j]);
				if (p >= 0)
					out->children[j] = loadmodel->nodes + p;
				else
					out->children[j] = (mnode_t *)(loadmodel->leafs + (-1 - p));
			}
		}
	}
	else
	{
		dsnode_t		*in;
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
	}
	Mod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs

	return true;
}

/*
=================
Mod_LoadLeafs
=================
*/
qboolean Mod_LoadLeafs (lump_t *l, qboolean lm)
{
	mleaf_t 	*out;
	int			i, j, count, p;

	if (lm)
	{
		dl1leaf_t 	*in;
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
				(unsigned int)LittleLong(in->firstmarksurface);
			out->nummarksurfaces = (unsigned int)LittleLong(in->nummarksurfaces);

			p = LittleLong(in->visofs);
			if (p == -1)
				out->compressed_vis = NULL;
			else
				out->compressed_vis = loadmodel->visdata + p;

			for (j=0 ; j<4 ; j++)
				out->ambient_sound_level[j] = in->ambient_level[j];
		}
	}
	else
	{
		dsleaf_t 	*in;
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
				(unsigned short)LittleShort(in->firstmarksurface);
			out->nummarksurfaces = (unsigned short)LittleShort(in->nummarksurfaces);

			p = LittleLong(in->visofs);
			if (p == -1)
				out->compressed_vis = NULL;
			else
				out->compressed_vis = loadmodel->visdata + p;

			for (j=0 ; j<4 ; j++)
				out->ambient_sound_level[j] = in->ambient_level[j];
		}
	}

	return true;
}

/*
=================
Mod_LoadClipnodes
=================
*/
qboolean Mod_LoadClipnodes (lump_t *l, qboolean lm)
{
	mclipnode_t *out;
	int			i, count;
	hull_t		*hull;

	if (lm)
	{
		dlclipnode_t *in;
		in = (void *)(mod_base + l->fileofs);
		if (l->filelen % sizeof(*in))
		{
			Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
			return false;
		}
		count = l->filelen / sizeof(*in);

		out = Hunk_AllocName ( count*sizeof(*out), loadname);
		for (i=0 ; i<count ; i++, in++)
		{
			out[i].planenum = LittleLong(in->planenum);
			out[i].children[0] = LittleLong(in->children[0]);
			out[i].children[1] = LittleLong(in->children[1]);
		}
	}
	else
	{
		dsclipnode_t *in;
		in = (void *)(mod_base + l->fileofs);
		if (l->filelen % sizeof(*in))
		{
			Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
			return false;
		}
		count = l->filelen / sizeof(*in);

		out = Hunk_AllocName ( count*sizeof(*out), loadname);
		for (i=0 ; i<count ; i++, in++)
		{
			out[i].planenum = LittleLong(in->planenum);
			out[i].children[0] = LittleShort(in->children[0]);
			out[i].children[1] = LittleShort(in->children[1]);
		}
	}

	loadmodel->clipnodes = out;
	loadmodel->numclipnodes = count;

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

		/*
		There is some mission-pack weirdness here
		in the missionpack, hull 4 is meant to be '-8 -8 -8' '8 8 8'
		in the original game, hull 4 is '-40 -40 -42' '40 40 42'
		*/
		hull = &loadmodel->hulls[4];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->planes;
		hull->clip_mins[0] = -8;
		hull->clip_mins[1] = -8;
		hull->clip_mins[2] = -8;
		hull->clip_maxs[0] = 8;
		hull->clip_maxs[1] = 8;
		hull->clip_maxs[2] = 8;
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

		//6 isn't used.
		//7 isn't used.
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
		hull->clip_mins[2] = -32;//-36 is correct here, but we'll just copy mvdsv instead.
		hull->clip_maxs[0] = 16;
		hull->clip_maxs[1] = 16;
		hull->clip_maxs[2] = hull->clip_mins[2]+72;
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
		hull->clip_maxs[2] = hull->clip_mins[2]+64;
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
		hull->clip_maxs[2] = hull->clip_mins[2]+36;
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

	return true;
}

/*
=================
Mod_MakeHull0

Deplicate the drawing hull structure as a clipping hull
=================
*/
void Mod_MakeHull0 (void)
{
	mnode_t		*in, *child;
	mclipnode_t *out;
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
void Mod_LoadMarksurfaces (lump_t *l)
{
	int		i, j, count;
	short		*in;
	msurface_t **out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		SV_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for ( i=0 ; i<count ; i++)
	{
		j = (unsigned short)LittleShort(in[i]);
		if (j >= loadmodel->numsurfaces)
			SV_Error ("Mod_ParseMarksurfaces: bad surface number");
		out[i] = loadmodel->surfaces + j;
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
qboolean Mod_LoadSurfedges (lump_t *l)
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
qboolean Mod_LoadPlanes (lump_t *l)
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
	out = Hunk_AllocName ( count*2*sizeof(*out), loadname);

	loadmodel->planes = out;
	loadmodel->numplanes = count;

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

	return true;
}

/*
=================
Mod_LoadBrushModel
=================
*/
qboolean Mod_LoadBrushModel (model_t *mod, void *buffer)
{
	int			i, j;
	dheader_t	*header;
	mmodel_t 	*bm;
	unsigned int chksum;
	int start;
	qboolean noerrors;
	qboolean longm = false;
#ifdef TERRAIN
	model_t *lm = loadmodel;
#endif

	start = Hunk_LowMark();

	loadmodel->type = mod_brush;

	header = (dheader_t *)buffer;

	i = LittleLong (header->version);


	if (i == BSPVERSION_LONG1)
	{
		loadmodel->fromgame = fg_quake;
		longm = true;
	}
	else if (i == BSPVERSION || i == BSPVERSIONPREREL)
		loadmodel->fromgame = fg_quake;
	else if (i == BSPVERSIONHL)
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

// load into heap

	mod->checksum = 0;
	mod->checksum2 = 0;

	// checksum all of the map, except for entities
	for (i = 0; i < HEADER_LUMPS; i++)
	{
		if ((unsigned)header->lumps[i].fileofs + (unsigned)header->lumps[i].filelen > com_filesize)
		{
			Con_Printf (CON_ERROR "Mod_LoadBrushModel: %s appears truncated\n", mod->name);
			return false;
		}

		if (i == LUMP_ENTITIES)
			continue;
		chksum = Com_BlockChecksum(mod_base + header->lumps[i].fileofs,
			header->lumps[i].filelen);
		mod->checksum ^= chksum;

		if (i == LUMP_VISIBILITY || i == LUMP_LEAFS || i == LUMP_NODES)
			continue;
		mod->checksum2 ^= chksum;
	}

	noerrors = true;
	if (!sv_nogetlight.value)
	{
		noerrors = noerrors && Mod_LoadVertexes (&header->lumps[LUMP_VERTEXES]);
		noerrors = noerrors && Mod_LoadEdges (&header->lumps[LUMP_EDGES], longm);
		noerrors = noerrors && Mod_LoadSurfedges (&header->lumps[LUMP_SURFEDGES]);
///*/on server?*/	Mod_LoadTextures (&header->lumps[LUMP_TEXTURES]);
		noerrors = noerrors && Mod_LoadLighting (&header->lumps[LUMP_LIGHTING]);
	}
	noerrors = noerrors && Mod_LoadSubmodels (&header->lumps[LUMP_MODELS]);
	noerrors = noerrors && Mod_LoadPlanes (&header->lumps[LUMP_PLANES]);
	if (!sv_nogetlight.value)
	{
		noerrors = noerrors && Mod_LoadTexinfo (&header->lumps[LUMP_TEXINFO]);
		noerrors = noerrors && Mod_LoadFaces (&header->lumps[LUMP_FACES], longm);
	}
//	Mod_LoadMarksurfaces (&header->lumps[LUMP_MARKSURFACES]);
	if (noerrors)
		Mod_LoadVisibility (&header->lumps[LUMP_VISIBILITY]);
	noerrors = noerrors && Mod_LoadLeafs (&header->lumps[LUMP_LEAFS], longm);
	noerrors = noerrors && Mod_LoadNodes (&header->lumps[LUMP_NODES], longm);
	noerrors = noerrors && Mod_LoadClipnodes (&header->lumps[LUMP_CLIPNODES], longm);
	if (noerrors)
	{
		Mod_LoadEntities (&header->lumps[LUMP_ENTITIES]);
		Mod_MakeHull0 ();
	}

	if (!noerrors)
	{
		Hunk_FreeToLowMark(start);
		return false;
	}

	Q1BSP_LoadBrushes(mod);
	Q1BSP_SetModelFuncs(mod);

	if (mod->surfaces && mod->lightdata)
		mod->funcs.LightPointValues = SVQ1_LightPointValues;


	mod->numframes = 2;		// regular and alternate animation
//
// set up the submodels (FIXME: this is confusing)
//
	for (i=0 ; i<mod->numsubmodels ; i++)
	{
		bm = &mod->submodels[i];

		mod->hulls[0].firstclipnode = bm->headnode[0];
		Q1BSP_CheckHullNodes(&mod->hulls[0]);
		for (j=1 ; j<MAX_MAP_HULLSM ; j++)
		{
			mod->hulls[j].firstclipnode = bm->headnode[j];
			mod->hulls[j].lastclipnode = mod->numclipnodes-1;
			if (mod->hulls[j].available)
				Q1BSP_CheckHullNodes(&mod->hulls[j]);
		}

		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;

		VectorCopy (bm->maxs, mod->maxs);
		VectorCopy (bm->mins, mod->mins);

		mod->numleafs = bm->visleafs;

		if (i < mod->numsubmodels-1)
		{	// duplicate the basic information
			char	name[10];

			sprintf (name, "*%i", i+1);
			loadmodel = Mod_FindName (name);
			*loadmodel = *mod;
			strcpy (loadmodel->name, name);
			mod = loadmodel;
		}
	}

#ifdef TERRAIN
	lm->terrain = Mod_LoadTerrainInfo(lm, loadname);
#endif

	return true;
}



void *Mod_Extradata (model_t *mod)
{
	void	*r;
	
	r = Cache_Check (&mod->cache);
	if (r)
		return r;

	Mod_LoadModel (mod, true);
	
	if (!mod->cache.data)
		Sys_Error ("Mod_Extradata: caching failed");
	return mod->cache.data;
}






#endif

