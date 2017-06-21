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


#if 1//ndef SERVERONLY	//FIXME
#include "glquake.h"
#include "com_mesh.h"

extern cvar_t r_shadow_bumpscale_basetexture;
extern cvar_t r_replacemodels;
extern cvar_t gl_lightmap_average;
cvar_t mod_loadentfiles						= CVAR("sv_loadentfiles", "1");
cvar_t mod_loadentfiles_dir					= CVAR("sv_loadentfiles_dir", "");
cvar_t mod_external_vis						= CVARD("mod_external_vis", "1", "Attempt to load .vis patches for quake maps, allowing transparent water to work properly.");
cvar_t mod_warnmodels						= CVARD("mod_warnmodels", "1", "Warn if any models failed to load. Set to 0 if your mod is likely to lack optional models (like its in development).");	//set to 0 for hexen2 and its otherwise-spammy-as-heck demo.
cvar_t mod_litsprites_force					= CVARD("mod_litsprites_force", "0", "If set to 1, sprites will be lit according to world lighting (including rtlights), like Tenebrae. Ideally use EF_ADDITIVE or EF_FULLBRIGHT to make emissive sprites instead.");
cvar_t temp_lit2support						= CVARD("temp_mod_lit2support", "0", "Set to 1 to enable lit2 support. This cvar will be removed once the format is finalised.");
#ifdef SERVERONLY
cvar_t gl_overbright, gl_specular, gl_load24bit, r_replacemodels, gl_miptexLevel, r_fb_bmodels;	//all of these can/should default to 0
cvar_t r_noframegrouplerp					= CVARF  ("r_noframegrouplerp", "0", CVAR_ARCHIVE);
cvar_t dpcompat_psa_ungroup					= CVAR  ("dpcompat_psa_ungroup", "0");
texture_t	r_notexture_mip_real;
texture_t	*r_notexture_mip = &r_notexture_mip_real;
#endif

void CM_Init(void);
void CM_Shutdown(void);

void Mod_LoadSpriteShaders(model_t *spr);
qboolean QDECL Mod_LoadSpriteModel (model_t *mod, void *buffer, size_t fsize);
qboolean QDECL Mod_LoadSprite2Model (model_t *mod, void *buffer, size_t fsize);
#ifdef Q1BSPS
static qboolean QDECL Mod_LoadBrushModel (model_t *mod, void *buffer, size_t fsize);
#endif
#if defined(Q2BSPS) || defined(Q3BSPS)
qboolean QDECL Mod_LoadQ2BrushModel (model_t *mod, void *buffer, size_t fsize);
#endif
model_t *Mod_LoadModel (model_t *mod, enum mlverbosity_e verbose);
static void Mod_PrintFormats_f(void);
static void Mod_SaveEntFile_f(void);

#ifdef MAP_DOOM
qboolean QDECL Mod_LoadDoomLevel(model_t *mod, void *buffer, size_t fsize);
#endif

#ifdef DSPMODELS
void Mod_LoadDoomSprite (model_t *mod);
#endif

#define	MAX_MOD_KNOWN	8192
model_t	*mod_known;
int		mod_numknown;

extern cvar_t r_loadlits;
#ifdef SPECULAR
extern cvar_t gl_specular;
#endif
extern cvar_t r_fb_bmodels;
void Mod_SortShaders(model_t *mod);
void Mod_LoadAliasShaders(model_t *mod);

#ifdef RUNTIMELIGHTING
model_t *lightmodel;
static struct relight_ctx_s *lightcontext;
static int numlightdata;
static qboolean writelitfile;

long relitsurface;
#ifndef MULTITHREAD
static void Mod_UpdateLightmap(int snum)
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
#endif

static void Mod_MemList_f(void)
{
	int m;
	model_t *mod;
	int total = 0;
	for (m=0 , mod=mod_known ; m<mod_numknown ; m++, mod++)
	{
		if (mod->memgroup.bytes)
			Con_Printf("%s: %i bytes\n", mod->name, mod->memgroup.bytes);
		total += mod->memgroup.bytes;
	}
	Con_Printf("Total: %i bytes\n", total);
}
#ifndef SERVERONLY
static void Mod_BatchList_f(void)
{
	int m, i;
	model_t *mod;
	batch_t *batch;
	unsigned int count;
	for (m=0 , mod=mod_known ; m<mod_numknown ; m++, mod++)
	{
		if (mod->type == mod_brush && mod->loadstate == MLS_LOADED)
		{
			Con_Printf("^1%s:\n", mod->name);
			count = 0;
			for (i = 0; i < SHADER_SORT_COUNT; i++)
			{
				for (batch = mod->batches[i]; batch; batch = batch->next)
				{
#if MAXRLIGHTMAPS > 1
					if (batch->lightmap[3] >= 0)
						Con_Printf("  %s lm=(%i:%i %i:%i %i:%i %i:%i) surfs=%u\n", batch->texture->shader->name, batch->lightmap[0], batch->lmlightstyle[0], batch->lightmap[1], batch->lmlightstyle[1], batch->lightmap[2], batch->lmlightstyle[2], batch->lightmap[3], batch->lmlightstyle[3], batch->maxmeshes);
					else if (batch->lightmap[2] >= 0)
						Con_Printf("  %s lm=(%i:%i %i:%i %i:%i) surfs=%u\n", batch->texture->shader->name, batch->lightmap[0], batch->lmlightstyle[0], batch->lightmap[1], batch->lmlightstyle[1], batch->lightmap[2], batch->lmlightstyle[2], batch->maxmeshes);
					else if (batch->lightmap[1] >= 0)
						Con_Printf("  %s lm=(%i:%i %i:%i) surfs=%u\n", batch->texture->shader->name, batch->lightmap[0], batch->lmlightstyle[0], batch->lightmap[1], batch->lmlightstyle[1], batch->maxmeshes);
					else
#endif
						if (batch->lmlightstyle[0] != 255)
						Con_Printf("  %s lm=(%i:%i) surfs=%u\n", batch->texture->shader->name, batch->lightmap[0], batch->lmlightstyle[0], batch->maxmeshes);
					else
						Con_Printf("  %s lm=%i surfs=%u\n", batch->texture->shader->name, batch->lightmap[0], batch->maxmeshes);
					count++;
				}
			}
			Con_Printf("^h(%u batches, lm %i*%i, lux %s)\n", count, mod->lightmaps.width, mod->lightmaps.height, mod->lightmaps.deluxemapping?"true":"false");
		}
	}
}

static void Mod_TextureList_f(void)
{
	int m, i;
	texture_t *tx;
	model_t *mod;
	qboolean shownmodelname = false;
	int count = 0;
	char *body;
	char editname[MAX_OSPATH];
	for (m=0 , mod=mod_known ; m<mod_numknown ; m++, mod++)
	{
		if (shownmodelname)
			Con_Printf("(%u textures)\n", count);
		shownmodelname = false;

		if (mod->type == mod_brush && mod->loadstate == MLS_LOADED)
		{
			if (*mod->name == '*')
				continue;//	inlines don't count
			count = 0;
			for (i = 0; i < mod->numtextures; i++)
			{
				tx = mod->textures[i];
				if (!tx)
					continue;	//happens on e1m2

				if (!shownmodelname)
				{
					shownmodelname = true;
					Con_Printf("%s\n", mod->name);
					count = 0;
				}

				body = Shader_GetShaderBody(tx->shader, editname, sizeof(editname));
				if (!body)
					body = "SHADER NOT KNOWN";
				else
				{
					char *cr;
					while ((cr = strchr(body, '\r')))
						*cr = ' ';
				}

				if (*editname)
					Con_Printf("  ^[^7%s\\edit\\%s\\tipimg\\%s\\tip\\{%s^]\n", tx->name, editname, tx->name, body);
				else
					Con_Printf("  ^[^7%s\\tipimg\\%s\\tip\\{%s^]\n", tx->name, tx->name, body);
				count++;
			}
		}
	}
	if (shownmodelname)
		Con_Printf("(%u textures)\n", count);
}

static void Mod_BlockTextureColour_f (void)
{
	char texname[64];
	model_t *mod;
	texture_t *tx;
//	shader_t *s;
	char *match = Cmd_Argv(1);

	int i, m;
//	unsigned int colour[8*8];
	unsigned int rgba;

	((char *)&rgba)[0] = atoi(Cmd_Argv(2));
	((char *)&rgba)[1] = atoi(Cmd_Argv(3));
	((char *)&rgba)[2] = atoi(Cmd_Argv(4));
	((char *)&rgba)[3] = 255;

	sprintf(texname, "purergb_%i_%i_%i", (int)((char *)&rgba)[0], (int)((char *)&rgba)[1], (int)((char *)&rgba)[2]);
/*	s = R_RegisterCustom(Cmd_Argv(2), SUF_LIGHTMAP, NULL, NULL);
	if (!s)
	{
		s = R_RegisterCustom (texname, SUF_LIGHTMAP, Shader_DefaultBSPQ1, NULL);

		for (i = 0; i < sizeof(colour)/sizeof(colour[0]); i++)
			colour[i] = rgba;
		s->defaulttextures.base = GL_LoadTexture32(texname, 8, 8, colour, IF_NOMIPMAP);
	}
*/
	for (m=0 , mod=mod_known ; m<mod_numknown ; m++, mod++)
	{
		if (mod->type == mod_brush && mod->loadstate == MLS_LOADED)
		{
			for (i = 0; i < mod->numtextures; i++)
			{
				tx = mod->textures[i];
				if (!tx)
					continue;	//happens on e1m2

				if (!stricmp(tx->name, match))
					tx->shader->defaulttextures->base = Image_GetTexture(texname, NULL, IF_NOMIPMAP|IF_NEAREST, &rgba, NULL, 1, 1, TF_BGRA32);
			}
		}
	}
}
#endif


#ifdef RUNTIMELIGHTING
#if defined(MULTITHREAD)
#ifdef _WIN32
#include <windows.h>
#endif
static void *relightthread[8];
static unsigned int relightthreads;
static volatile qboolean wantrelight;

static int RelightThread(void *arg)
{
	int surf;
	void *threadctx = malloc(lightthreadctxsize);
	while (wantrelight)
	{
#ifdef _WIN32
		surf = InterlockedIncrement(&relitsurface);
#elif defined(__GNUC__)
		surf = __sync_add_and_fetch(&relitsurface, 1);
#else
		surf = relitsurface++;
#endif
		if (surf >= lightmodel->numsurfaces)
			break;
		LightFace(lightcontext, threadctx, surf);
		lightmodel->surfaces[surf].cached_dlight = -1;
	}
	free(threadctx);
	return 0;
}
#else
static void *lightmainthreadctx;
#endif
#endif

void Mod_Think (void)
{
#ifdef RUNTIMELIGHTING
	if (lightmodel)
	{
#ifdef MULTITHREAD
		if (!relightthreads)
		{
			int i;
#if defined(_WIN32) && !defined(WINRT)
			HANDLE me = GetCurrentProcess();
			DWORD_PTR proc, sys;
			/*count cpus*/
			GetProcessAffinityMask(me, &proc, &sys);
			relightthreads = 0;
			for (i = 0; i < sizeof(proc)*8; i++)
				if (proc & ((size_t)1u<<i))
					relightthreads++;
			/*subtract 1*/
			if (relightthreads <= 1)
				relightthreads = 1;
			else
				relightthreads--;
#elif defined(__GNUC__)
			relightthreads = 2;	//erm, lets hope...
#else
			/*can't do atomics*/
			relightthreads = 1;
#endif
			if (relightthreads > sizeof(relightthread)/sizeof(relightthread[0]))
				relightthreads = sizeof(relightthread)/sizeof(relightthread[0]);
			wantrelight = true;
			for (i = 0; i < relightthreads; i++)
				relightthread[i] = Sys_CreateThread("relight", RelightThread, lightmodel, THREADP_NORMAL, 0);
		}
		if (relitsurface < lightmodel->numsurfaces)
		{
			return;
		}
#else
		if (!lightmainthreadctx)
			lightmainthreadctx = malloc(lightthreadctxsize);
		LightFace(lightcontext, lightmainthreadctx, relitsurface);
		Mod_UpdateLightmap(relitsurface);

		relitsurface++;
#endif
		if (relitsurface >= lightmodel->numsurfaces)
		{
			vfsfile_t *f;
			char filename[MAX_QPATH];
			Con_Printf("Finished lighting %s\n", lightmodel->name);

#ifdef MULTITHREAD
			if (relightthreads)
			{
				int i;
				wantrelight = false;
				for (i = 0; i < relightthreads; i++)
				{
					Sys_WaitOnThread(relightthread[i]);
					relightthread[i] = NULL;
				}
				relightthreads = 0;
			}
#else
			free(lightmainthreadctx);
			lightmainthreadctx = NULL;
#endif

			LightShutdown(lightcontext, lightmodel);
			lightcontext = NULL;

			if (lightmodel->deluxdata)
			{
				COM_StripExtension(lightmodel->name, filename, sizeof(filename));
				COM_DefaultExtension(filename, ".lux", sizeof(filename));
				f = FS_OpenVFS(filename, "wb", FS_GAME);
				if (f)
				{
					VFS_WRITE(f, "QLIT\1\0\0\0", 8);
					VFS_WRITE(f, lightmodel->deluxdata, numlightdata*3);
					VFS_CLOSE(f);
				}
				else
					Con_Printf("Unable to write \"%s\"\n", filename);
			}

			if (writelitfile)	//the user might already have a lit file (don't overwrite it).
			{
				COM_StripExtension(lightmodel->name, filename, sizeof(filename));
				COM_DefaultExtension(filename, ".lit", sizeof(filename));

				f = FS_OpenVFS(filename, "wb", FS_GAME);
				if (f)
				{
					VFS_WRITE(f, "QLIT\1\0\0\0", 8);
					VFS_WRITE(f, lightmodel->lightdata, numlightdata*3);
					VFS_CLOSE(f);
				}
				else
					Con_Printf("Unable to write \"%s\"\n", filename);
			}
			lightmodel = NULL;
		}
	}
#endif
}

void Mod_RebuildLightmaps (void)
{
	int i, j;
	msurface_t *surf;
	model_t	*mod;

	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		if (mod->loadstate != MLS_LOADED)
			continue;

		if (mod->type == mod_brush)
		{
			for (j=0, surf = mod->surfaces; j<mod->numsurfaces ; j++, surf++)
				surf->cached_dlight=-1;//force it
		}
	}
}

void Mod_ResortShaders(void)
{
	//called when some shader changed its sort key.
	//this means we have to hunt down all models and update their batches.
	//really its only bsps that need this.
	batch_t *oldlists[SHADER_SORT_COUNT], *b;
	int i, j, bs;
	model_t	*mod;
	for (i=0, mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		if (mod->loadstate != MLS_LOADED)
			continue;

		memcpy(oldlists, mod->batches, sizeof(oldlists));
		memset(mod->batches, 0, sizeof(oldlists));
		mod->numbatches = 0;	//this is a bit of a misnomer. clearing this will cause it to be recalculated, with everything renumbered as needed.
	
		for (j = 0; j < SHADER_SORT_COUNT; j++)
		{
			while((b=oldlists[j]))
			{
				oldlists[j] = b->next;
				bs = b->shader?b->shader->sort:j;

				b->next = mod->batches[bs];
				mod->batches[bs] = b;
			}
		}
	}
}

const char *Mod_GetEntitiesString(model_t *mod)
{
	size_t vl;
	size_t e;
	size_t sz;
	char *o;
	if (!mod)
		return NULL;
	if (mod->entities_raw)	//still cached/correct
		return mod->entities_raw;
	if (!mod->numentityinfo)
		return NULL;

	//reform the entities back into a full string now that we apparently need it
	//find needed buffer size
	for (e = 0, sz = 0; e < mod->numentityinfo; e++)
	{
		if (!mod->entityinfo[e].keyvals)
			continue;
		sz += 2;
		sz += strlen(mod->entityinfo[e].keyvals);
		sz += 2;
	}
	sz+=1;
	o = BZ_Malloc(sz);

	//splurge it out
	for (e = 0, sz = 0; e < mod->numentityinfo; e++)
	{
		if (!mod->entityinfo[e].keyvals)
			continue;
		o[sz+0] = '{';
		o[sz+1] = '\n';
		sz += 2;
		vl = strlen(mod->entityinfo[e].keyvals);
		memcpy(&o[sz], mod->entityinfo[e].keyvals, vl);
		sz += vl;
		o[sz+0] = '}';
		o[sz+1] = '\n';
		sz += 2;
	}
	o[sz+0] = 0;

	mod->entities_raw = o;
	return mod->entities_raw;
}
void Mod_SetEntitiesString(model_t *mod, const char *str, qboolean docopy)
{
	size_t j;
	for (j = 0; j < mod->numentityinfo; j++)
		Z_Free(mod->entityinfo[j].keyvals);
	mod->numentityinfo = 0;
	Z_Free(mod->entityinfo);
	mod->entityinfo = NULL;
	Z_Free((char*)mod->entities_raw);
	mod->entities_raw = NULL;

	if (str)
	{
		if (docopy)
			str = Z_StrDup(str);
		mod->entities_raw = str;
	}
}

void Mod_SetEntitiesStringLen(model_t *mod, const char *str, size_t strsize)
{
	if (str)
	{
		char *cpy = BZ_Malloc(strsize+1);
		memcpy(cpy, str, strsize);
		cpy[strsize] = 0;
		Mod_SetEntitiesString(mod, cpy, false);
	}
	else
		Mod_SetEntitiesString(mod, str, false);
}

void Mod_ParseEntities(model_t *mod)
{
	char key[1024];
	char value[4096];
	const char *entstart;
	const char *entend;
	const char *entdata;
	size_t c, m;

	c = 0; m = 0;

	while (mod->numentityinfo > 0)
		Z_Free(mod->entityinfo[--mod->numentityinfo].keyvals);
	Z_Free(mod->entityinfo);
	mod->entityinfo = NULL;


	entdata = mod->entities_raw;
	while(1)
	{
		if (!(entdata=COM_ParseOut(entdata, key, sizeof(key))))
			break;
		if (strcmp(key, "{"))
			break;

		//skip whitespace to save space.
		while (*entdata == ' ' || *entdata == '\r' || *entdata == '\n' || *entdata == '\t')
			entdata++;

		entstart = entdata;

		while(1)
		{
			entend = entdata;
			entdata=COM_ParseOut(entdata, key, sizeof(key));
			if (!strcmp(key, "}"))
				break;
			entdata=COM_ParseOut(entdata, value, sizeof(value));
		}
		if (!entdata)
			break;	//erk. eof

		if (c == m)
		{
			if (!m)
				m = 64;
			else
				m *= 2;
			mod->entityinfo = BZ_Realloc(mod->entityinfo, sizeof(*mod->entityinfo) * m);
		}
		mod->entityinfo[c].id = c+1;
		mod->entityinfo[c].keyvals = BZ_Malloc(entend-entstart + 1);
		memcpy(mod->entityinfo[c].keyvals, entstart, entend-entstart);
		mod->entityinfo[c].keyvals[entend-entstart] = 0;
		c++;
	}
	mod->numentityinfo = c;
}

/*
===================
Mod_ClearAll
===================

called before new content is loaded.
*/
static int mod_datasequence;
void Mod_ClearAll (void)
{
#ifdef RUNTIMELIGHTING
#ifdef MULTITHREAD
	int		i;
	wantrelight = false;
	for (i = 0; i < relightthreads; i++)
	{
		Sys_WaitOnThread(relightthread[i]);
		relightthread[i] = NULL;
	}
	relightthreads = 0;
#else
	free(lightmainthreadctx);
	lightmainthreadctx = NULL;
#endif
	lightmodel = NULL;
#endif

	mod_datasequence++;
}

qboolean Mod_PurgeModel(model_t	*mod, enum mod_purge_e ptype)
{
	if (mod->loadstate == MLS_LOADING)
	{
		if (ptype == MP_MAPCHANGED && !mod->submodelof)
			return false;	//don't bother waiting for it on map changes.
		COM_WorkerPartialSync(mod, &mod->loadstate, MLS_LOADING);
	}

#ifdef RUNTIMELIGHTING
	if (lightmodel == mod)
	{
#ifdef MULTITHREAD
		int		i;
		wantrelight = false;
		for (i = 0; i < relightthreads; i++)
		{
			Sys_WaitOnThread(relightthread[i]);
			relightthread[i] = NULL;
		}
		relightthreads = 0;
#else
		free(lightmainthreadctx);
		lightmainthreadctx = NULL;
#endif
		lightmodel = NULL;
	}
#endif

#ifdef TERRAIN
	//we can safely flush all terrain sections at any time
	if (mod->terrain && ptype != MP_MAPCHANGED)
		Terr_PurgeTerrainModel(mod, false, true);
#endif

	//purge any vbos
	if (mod->type == mod_brush)
	{
		//brush models cannot be safely flushed.
		if (ptype != MP_RESET)
			return false;
#ifndef SERVERONLY
		Surf_Clear(mod);
#endif
	}

#ifdef TERRAIN
	if (mod->type == mod_brush || mod->type == mod_heightmap)
	{
		//heightmap/terrain models cannot be safely flushed (brush models might have terrain embedded).
		if (ptype != MP_RESET)
			return false;
		Terr_FreeModel(mod);
	}
#endif
	if (mod->type == mod_alias)
	{
		Mod_DestroyMesh(mod->meshinfo);
		mod->meshinfo = NULL;
	}

	Mod_SetEntitiesString(mod, NULL, false);

#ifdef PSET_SCRIPT
	PScript_ClearSurfaceParticles(mod);
#endif

	//and obliterate anything else remaining in memory.
	ZG_FreeGroup(&mod->memgroup);
	mod->meshinfo = NULL;
	mod->loadstate = MLS_NOTLOADED;

	mod->submodelof = NULL;
	mod->pvs = NULL;
	mod->phs = NULL;

	return true;
}

//can be called in one of two ways.
//force=true: explicit flush. everything goes, even if its still in use.
//force=false: map change. lots of stuff is no longer in use and can be freely flushed.
//certain models cannot be safely flushed while still in use. such models will not be flushed even if forced (they may still be partially flushed).
void Mod_Purge(enum mod_purge_e ptype)
{
	int		i;
	model_t	*mod;
	qboolean unused;

	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		unused = mod->datasequence != mod_datasequence;

		if (mod->loadstate == MLS_NOTLOADED)
			continue;

		//this model isn't active any more.
		if (unused || ptype != MP_MAPCHANGED)
		{
			if (unused)
				Con_DLPrintf(2, "model \"%s\" no longer needed\n", mod->name);
			Mod_PurgeModel(mod, (ptype==MP_FLUSH && unused)?MP_RESET:ptype);
		}
	}
}

/*
===============
Mod_Init
===============
*/
void Mod_Init (qboolean initial)
{
	if (!mod_known)
		mod_known = malloc(MAX_MOD_KNOWN * sizeof(*mod_known));
	if (!initial)
	{
		Mod_ClearAll();	//shouldn't be needed
		Mod_Purge(MP_RESET);//shouldn't be needed
		mod_numknown = 0;
#ifdef Q1BSPS
		Q1BSP_Init();
#endif

		Cmd_AddCommand("mod_memlist", Mod_MemList_f);
#ifndef SERVERONLY
		Cmd_AddCommand("mod_batchlist", Mod_BatchList_f);
		Cmd_AddCommand("mod_texturelist", Mod_TextureList_f);
		Cmd_AddCommand("mod_usetexture", Mod_BlockTextureColour_f);
#endif
	}
	else
	{
		Cvar_Register(&mod_external_vis, "Graphical Nicaties");
		Cvar_Register(&mod_warnmodels, "Graphical Nicaties");
		Cvar_Register(&mod_litsprites_force, "Graphical Nicaties");
		Cvar_Register(&mod_loadentfiles, NULL);
		Cvar_Register(&mod_loadentfiles_dir, NULL);
		Cvar_Register(&temp_lit2support, NULL);
		Cmd_AddCommand("sv_saveentfile", Mod_SaveEntFile_f);
		Cmd_AddCommand("version_modelformats", Mod_PrintFormats_f);
	}

	if (initial)
	{
		Alias_Register();

#ifdef SPRMODELS
		Mod_RegisterModelFormatMagic(NULL, "Quake1 Sprite (spr)",			IDSPRITEHEADER,							Mod_LoadSpriteModel);
#endif
#ifdef SP2MODELS
		Mod_RegisterModelFormatMagic(NULL, "Quake2 Sprite (sp2)",			IDSPRITE2HEADER,						Mod_LoadSprite2Model);
#endif

		//q2/q3bsps
#if defined(Q2BSPS) || defined(Q3BSPS)
		Mod_RegisterModelFormatMagic(NULL, "Quake2/Quake3 Map (bsp)",		IDBSPHEADER,							Mod_LoadQ2BrushModel);
#endif
#ifdef RFBSPS
		Mod_RegisterModelFormatMagic(NULL, "Raven Map (bsp)",				('R'<<0)+('B'<<8)+('S'<<16)+('P'<<24),	Mod_LoadQ2BrushModel);
		Mod_RegisterModelFormatMagic(NULL, "QFusion Map (bsp)",				('F'<<0)+('B'<<8)+('S'<<16)+('P'<<24),	Mod_LoadQ2BrushModel);
#endif

		//doom maps
#ifdef MAP_DOOM
		Mod_RegisterModelFormatMagic(NULL, "Doom IWad Map",					(('D'<<24)+('A'<<16)+('W'<<8)+'I'),		Mod_LoadDoomLevel);
		Mod_RegisterModelFormatMagic(NULL, "Doom PWad Map",					(('D'<<24)+('A'<<16)+('W'<<8)+'P'),		Mod_LoadDoomLevel);
#endif

#ifdef MAP_PROC
		Mod_RegisterModelFormatText(NULL, "Doom3 (cm)",						"CM",									D3_LoadMap_CollisionMap);
#endif

#ifdef Q1BSPS
		//q1-based formats
		Mod_RegisterModelFormatMagic(NULL, "Quake1 2PSB Map(bsp)",			BSPVERSION_LONG1,						Mod_LoadBrushModel);
		Mod_RegisterModelFormatMagic(NULL, "Quake1 BSP2 Map(bsp)",			BSPVERSION_LONG2,						Mod_LoadBrushModel);
		Mod_RegisterModelFormatMagic(NULL, "Half-Life Map (bsp)",			30,										Mod_LoadBrushModel);
		Mod_RegisterModelFormatMagic(NULL, "Quake1 Map (bsp)",				29,										Mod_LoadBrushModel);
		Mod_RegisterModelFormatMagic(NULL, "Quake1 Prerelease Map (bsp)",	28,										Mod_LoadBrushModel);
#endif
	}
}

void Mod_Shutdown (qboolean final)
{
	if (final)
	{
		Mod_ClearAll();
		Mod_Purge(MP_RESET);

		Mod_UnRegisterAllModelFormats(NULL);
#ifdef Q2BSPS
		CM_Shutdown();
#endif
	}
	else
	{
		Mod_ClearAll();
		Mod_Purge(MP_RESET);

		Cmd_RemoveCommand("mod_memlist");
		Cmd_RemoveCommand("mod_batchlist");
		Cmd_RemoveCommand("mod_texturelist");
		Cmd_RemoveCommand("mod_usetexture");
	}
	free(mod_known);
	mod_known = NULL;
	mod_numknown = 0;

#ifndef SERVERONLY
	r_worldentity.model = NULL;	//just in case.
	cl_numvisedicts = 0;	//make sure nothing gets cached.
#endif
}

/*
===============
Mod_Init

Caches the data if needed
===============
*/
void *Mod_Extradata (model_t *mod)
{
	void	*r;
	
	r = mod->meshinfo;
	if (r)
		return r;

	Mod_LoadModel (mod, MLV_ERROR);
	
	if (!mod->meshinfo)
		Sys_Error ("Mod_Extradata: caching failed");
	return mod->meshinfo;
}

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf (model_t *model, vec3_t p)
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

const char *Mod_FixName(const char *modname, const char *worldname)
{
	if (*modname == '*' && worldname && *worldname)
	{
		//make sure that the value is an inline value with no existing extra postfix or anything.
		char *e;
		if (strtoul(modname+1, &e, 10) != 0)
			if (!*e)
				return va("%s:%s", modname, worldname);
	}
	return modname;
}
/*
==================
Mod_FindName

==================
*/
model_t *Mod_FindName (const char *name)
{
	int		i;
	model_t	*mod;
	
//	if (!name[0])
//		Sys_Error ("Mod_ForName: NULL name");
		
//
// search the currently loaded models
//
	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
		if (!strcmp (mod->publicname, name) )
			break;
			
	if (i == mod_numknown)
	{
#ifdef LOADERTHREAD
		Sys_LockMutex(com_resourcemutex);
		for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
			if (!strcmp (mod->publicname, name) )
				break;
		if (i == mod_numknown)
		{
#endif
			if (mod_numknown == MAX_MOD_KNOWN)
				Sys_Error ("mod_numknown == MAX_MOD_KNOWN");
			if (strlen(name) >= sizeof(mod->publicname))
				Sys_Error ("model name is too long: %s", name);
			memset(mod, 0, sizeof(model_t));	//clear the old model as the renderers use the same globals
			Q_strncpyz (mod->publicname, name, sizeof(mod->publicname));
			Q_strncpyz (mod->name, name, sizeof(mod->name));
			mod->loadstate = MLS_NOTLOADED;
			mod_numknown++;
			mod->particleeffect = -1;
			mod->particletrail = -1;
#ifdef LOADERTHREAD
		}
		Sys_UnlockMutex(com_resourcemutex);
#endif
	}

//	if (mod->loadstate == MLS_FAILED)
//		mod->loadstate = MLS_NOTLOADED;

	//mark it as active, so it doesn't get flushed prematurely
	mod->datasequence = mod_datasequence;
	return mod;
}

/*
==================
Mod_TouchModel

==================
*/
void Mod_TouchModel (const char *name)
{
	//findname does this anyway.
	Mod_FindName (name);
}

static struct
{
	void *module;
	char *formatname;
	char *ident;
	unsigned int magic;
	qboolean (QDECL *load) (model_t *mod, void *buffer, size_t buffersize);
} modelloaders[64];

static void Mod_PrintFormats_f(void)
{
	int i;
	for (i = 0; i < sizeof(modelloaders)/sizeof(modelloaders[0]); i++)
	{
		if (modelloaders[i].load && modelloaders[i].formatname)
			Con_Printf("%s\n", modelloaders[i].formatname);
	}
}

int Mod_RegisterModelFormatText(void *module, const char *formatname, char *magictext, qboolean (QDECL *load) (model_t *mod, void *buffer, size_t fsize))
{
	int i, free = -1;
	for (i = 0; i < sizeof(modelloaders)/sizeof(modelloaders[0]); i++)
	{
		if (modelloaders[i].ident && !strcmp(modelloaders[i].ident, magictext))
		{
			free = i;
			break;	//extension match always replaces
		}
		else if (!modelloaders[i].load && free < 0)
			free = i;
	}
	if (free < 0)
		return 0;

	modelloaders[free].module = module;
	modelloaders[free].formatname = Z_StrDup(formatname);
	modelloaders[free].magic = 0;
	modelloaders[free].ident = Z_StrDup(magictext);
	modelloaders[free].load = load;

	return free+1;
}
int Mod_RegisterModelFormatMagic(void *module, const char *formatname, unsigned int magic, qboolean (QDECL *load) (model_t *mod, void *buffer, size_t fsize))
{
	int i, free = -1;
	for (i = 0; i < sizeof(modelloaders)/sizeof(modelloaders[0]); i++)
	{
		if (modelloaders[i].magic && modelloaders[i].magic == magic)
		{
			free = i;
			break;	//extension match always replaces
		}
		else if (!modelloaders[i].load && free < 0)
			free = i;
	}
	if (free < 0)
		return 0;

	modelloaders[free].module = module;
	if (modelloaders[free].formatname)
		Z_Free(modelloaders[free].formatname);
	modelloaders[free].formatname = Z_StrDup(formatname);
	modelloaders[free].magic = magic;
	modelloaders[free].ident = NULL;
	modelloaders[free].load = load;

	return free+1;
}

void Mod_UnRegisterModelFormat(void *module, int idx)
{
	
	idx--;
	if ((unsigned int)(idx) >= sizeof(modelloaders)/sizeof(modelloaders[0]))
		return;
	if (modelloaders[idx].module != module)
		return;

	COM_WorkerFullSync();
	Z_Free(modelloaders[idx].ident);
	modelloaders[idx].ident = NULL;
	Z_Free(modelloaders[idx].formatname);
	modelloaders[idx].formatname = NULL;
	modelloaders[idx].magic = 0;
	modelloaders[idx].load = NULL;
	modelloaders[idx].module = NULL;

	//FS_Restart will be needed
}

void Mod_UnRegisterAllModelFormats(void *module)
{
	int i;
	COM_WorkerFullSync();
	for (i = 0; i < sizeof(modelloaders)/sizeof(modelloaders[0]); i++)
	{
		if (modelloaders[i].module == module)
			Mod_UnRegisterModelFormat(module, i+1);
	}
}

void Mod_ModelLoaded(void *ctx, void *data, size_t a, size_t b)
{
	qboolean previouslyfailed;
	model_t *mod = ctx;
	enum mlverbosity_e verbose = b;
#ifndef SERVERONLY
	P_LoadedModel(mod);
#endif

	previouslyfailed = mod->loadstate == MLS_FAILED;
	mod->loadstate = a;

#ifdef TERRAIN
	if (mod->terrain)
		Terr_FinishTerrain(mod);
#endif
#ifndef SERVERONLY
	if (mod->type == mod_brush)
	{
		Surf_BuildModelLightmaps(mod);
	}
	if (mod->type == mod_sprite)
	{
		Mod_LoadSpriteShaders(mod);
	}
	if (mod->type == mod_alias)
	{
		if (qrenderer != QR_NONE)
			Mod_LoadAliasShaders(mod);


#ifdef RAGDOLL
		{
			int numbones = Mod_GetNumBones(mod, false);
			if (numbones)
			{
				size_t filesize;
				char *buf;
				char dollname[MAX_QPATH];
				Q_snprintfz(dollname, sizeof(dollname), "%s.doll", mod->name);
				buf = COM_LoadFile(dollname, 5, &filesize);
				if (buf)
				{
					mod->dollinfo = rag_createdollfromstring(mod, dollname, numbones, buf);
					BZ_Free(buf);
				}
			}
		}
#endif
	}
#endif

	switch(verbose)
	{
	default:
	case MLV_ERROR:
		Host_EndGame ("Mod_NumForName: %s not found or couldn't load", mod->name);
		break;
	case MLV_WARNSYNC:
	case MLV_WARN:
		if (*mod->name != '*' && strcmp(mod->name, "null") && mod_warnmodels.ival && !previouslyfailed)
			Con_Printf(CON_ERROR "Unable to load %s\n", mod->name);
		break;
	case MLV_SILENT:
		break;
	}
}
/*
==================
Mod_LoadModel

Loads a model into the cache
==================
*/
static void Mod_LoadModelWorker (void *ctx, void *data, size_t a, size_t b)
{
	model_t *mod = ctx;
	enum mlverbosity_e verbose = a;
	unsigned *buf = NULL;
	char mdlbase[MAX_QPATH];
	char *replstr;
#ifdef DSPMODELS
	qboolean doomsprite = false;
#endif
	unsigned int magic, i;
	size_t filesize;
	char ext[8];

	if (!*mod->publicname)
	{
		mod->type = mod_dummy;
		mod->mins[0] = -16;
		mod->mins[1] = -16;
		mod->mins[2] = -16;
		mod->maxs[0] = 16;
		mod->maxs[1] = 16;
		mod->maxs[2] = 16;
		mod->engineflags = 0;
		COM_AddWork(WG_MAIN, Mod_ModelLoaded, mod, NULL, MLS_LOADED, 0);
		return;
	}
	
#ifdef RAGDOLL
	if (mod->dollinfo)
	{
		rag_freedoll(mod->dollinfo);
		mod->dollinfo = NULL;
	}
#endif

	if (mod->loadstate == MLS_FAILED)
		return;

//
// load the file
//
	// set necessary engine flags for loading purposes
	if (!strcmp(mod->publicname, "progs/player.mdl"))
		mod->engineflags |= MDLF_PLAYER | MDLF_DOCRC;
	else if (!strcmp(mod->publicname, "progs/flame.mdl") || 
		!strcmp(mod->publicname, "progs/flame2.mdl") ||
		!strcmp(mod->publicname, "models/flame1.mdl") ||	//hexen2 small standing flame
		!strcmp(mod->publicname, "models/flame2.mdl") ||	//hexen2 large standing flame
		!strcmp(mod->publicname, "models/cflmtrch.mdl"))	//hexen2 wall torch
		mod->engineflags |= MDLF_FLAME;
	else if (!strcmp(mod->publicname, "progs/bolt.mdl") ||
		!strcmp(mod->publicname, "progs/bolt2.mdl") ||
		!strcmp(mod->publicname, "progs/bolt3.mdl") ||
		!strcmp(mod->publicname, "progs/beam.mdl") || 
		!strcmp(mod->publicname, "models/stsunsf2.mdl") || 
		!strcmp(mod->publicname, "models/stsunsf1.mdl") ||
		!strcmp(mod->publicname, "models/stice.mdl"))
		mod->engineflags |= MDLF_BOLT;
	else if (!strcmp(mod->publicname, "progs/backpack.mdl"))
		mod->engineflags |= MDLF_NOTREPLACEMENTS;
	else if (!strcmp(mod->publicname, "progs/eyes.mdl"))
		mod->engineflags |= MDLF_NOTREPLACEMENTS|MDLF_DOCRC;

	/*handle ezquake-originated cheats that would feck over fte users if fte didn't support
	these are the conditions required for r_fb_models on non-players*/
	mod->engineflags |= MDLF_EZQUAKEFBCHEAT;
	if ((mod->engineflags & MDLF_DOCRC) ||
		!strcmp(mod->publicname, "progs/backpack.mdl") ||
		!strcmp(mod->publicname, "progs/gib1.mdl") ||
		!strcmp(mod->publicname, "progs/gib2.mdl") ||
		!strcmp(mod->publicname, "progs/gib3.mdl") ||
		!strcmp(mod->publicname, "progs/h_player.mdl") ||
		!strncmp(mod->publicname, "progs/v_", 8))
		mod->engineflags &= ~MDLF_EZQUAKEFBCHEAT;

	mod->engineflags |= MDLF_RECALCULATERAIN;

	// get string used for replacement tokens
	COM_FileExtension(mod->publicname, ext, sizeof(ext));
	if (!Q_strcasecmp(ext, "spr") || !Q_strcasecmp(ext, "sp2"))
		replstr = ""; // sprite
#ifdef DSPMODELS
	else if (!Q_strcasecmp(ext, "dsp")) // doom sprite
	{
		replstr = "";
		doomsprite = true;
	}
#endif
	else // assume models
		replstr = r_replacemodels.string;

	// gl_load24bit 0 disables all replacements
	if (!gl_load24bit.value)
		replstr = "";

	COM_StripExtension(mod->publicname, mdlbase, sizeof(mdlbase));

	while (replstr)
	{
		char token[256];
		replstr = COM_ParseStringSet(replstr, token, sizeof(token));

		if (replstr)
		{
			char altname[MAX_QPATH];
			Q_snprintfz(altname, sizeof(altname), "%s.%s", mdlbase, token);
			TRACE(("Mod_LoadModel: Trying to load (replacement) model \"%s\"\n", altname));
			buf = (unsigned *)COM_LoadFile (altname, 5, &filesize);

			if (buf)
				Q_strncpyz(mod->name, altname, sizeof(mod->name));
		}
		else
		{
			TRACE(("Mod_LoadModel: Trying to load model \"%s\"\n", mod->publicname));
			buf = (unsigned *)COM_LoadFile (mod->publicname, 5, &filesize);
			if (buf)
				Q_strncpyz(mod->name, mod->publicname, sizeof(mod->name));
			else if (!buf)
			{
#ifdef DSPMODELS
				if (doomsprite) // special case needed for doom sprites
				{
					TRACE(("Mod_LoadModel: doomsprite: \"%s\"\n", mod->name));
					Mod_LoadDoomSprite(mod);
					BZ_Free(buf);
					COM_AddWork(WG_MAIN, Mod_ModelLoaded, mod, NULL, MLS_LOADED, 0);
					return;
				}
#endif
#ifdef TERRAIN
				if (!Q_strcasecmp(ext, "map"))
				{
					const char *dummymap =
						"{\n"
							"classname worldspawn\n"
							"wad \"base.wad\"\n"	//we ARE a quake engine after all, and default.wad is generally wrong
							"message \"Unnamed map\"\n"
							"{\n"
								"(-128  128 0)	( 128  128 0)	( 128 -128 0)	\"WBRICK1_5\" 0 0 0 1 1\n"
								"( 128 -128 -16)( 128  128 -16)	(-128  128 -16)	\"WBRICK1_5\" 0 0 0 1 1\n"
								"( 128  128 0)	(-128  128 0)	(-128  128 -16)	\"WBRICK1_5\" 0 0 0 1 1\n"
								"(-128 -128 0)	( 128 -128 0)	( 128 -128 -16)	\"WBRICK1_5\" 0 0 0 1 1\n"
								"(-128  128 0)	(-128 -128 0)	(-128 -128 -16)	\"WBRICK1_5\" 0 0 0 1 1\n"
								"( 128 -128 0)	( 128  128 0)	( 128  128 -16)	\"WBRICK1_5\" 0 0 0 1 1\n"
							"}\n"
						"}\n"
						"{\n"
							"classname info_player_start\n"
							"origin \"0 0 24\"\n"
						"}\n"
						"{\n"
							"classname light\n"
							"origin \"0 0 64\"\n"
						"}\n";
					buf = (unsigned*)Z_StrDup(dummymap);
					filesize = strlen(dummymap);
				}
				else
#endif
					break; // failed to load unreplaced file and nothing left
			}
		}
		if (!buf)
			continue;
		if (filesize < 4)
		{
			BZ_Free(buf);
			continue;
		}

//
// fill it in
//
		Mod_DoCRC(mod, (char*)buf, filesize);

		if (filesize < 4)
			magic = 0;
		else
			magic = LittleLong(*(unsigned *)buf);
		for(i = 0; i < sizeof(modelloaders) / sizeof(modelloaders[0]); i++)
		{
			if (modelloaders[i].load && modelloaders[i].magic == magic && !modelloaders[i].ident)
				break;
		}
		if (i < sizeof(modelloaders) / sizeof(modelloaders[0]))
		{
			if (!modelloaders[i].load(mod, buf, filesize))
			{
				BZ_Free(buf);
				continue;
			}
		}
		else
		{
			COM_ParseOut((char*)buf, token, sizeof(token));
			for(i = 0; i < sizeof(modelloaders) / sizeof(modelloaders[0]); i++)
			{
				if (modelloaders[i].load && modelloaders[i].ident && !strcmp(modelloaders[i].ident, token))
					break;
			}
			if (i < sizeof(modelloaders) / sizeof(modelloaders[0]))
			{
				if (!modelloaders[i].load(mod, buf, filesize))
				{
					BZ_Free(buf);
					continue;
				}
			}
			else
			{
				Con_Printf(CON_WARNING "Unrecognised model format 0x%x (%c%c%c%c)\n", magic, ((char*)buf)[0], ((char*)buf)[1], ((char*)buf)[2], ((char*)buf)[3]);
				BZ_Free(buf);
				continue;
			}
		}

/*
#ifdef MAP_PROC
			if (!strcmp(com_token, "CM"))	//doom3 map.
			{
				TRACE(("Mod_LoadModel: doom3 CM\n"));
				if (!D3_LoadMap_CollisionMap (mod, (char*)buf))
					continue;
				break;
			}
#endif
*/

		TRACE(("Mod_LoadModel: Loaded\n"));

		BZ_Free(buf);

		COM_AddWork(WG_MAIN, Mod_ModelLoaded, mod, NULL, MLS_LOADED, 0);
		return;
	}

	mod->type = mod_dummy;
	mod->mins[0] = -16;
	mod->mins[1] = -16;
	mod->mins[2] = -16;
	mod->maxs[0] = 16;
	mod->maxs[1] = 16;
	mod->maxs[2] = 16;
	mod->engineflags = 0;
	COM_AddWork(WG_MAIN, Mod_ModelLoaded, mod, NULL, MLS_FAILED, verbose);
}


model_t *Mod_LoadModel (model_t *mod, enum mlverbosity_e verbose)
{
	if (mod->loadstate == MLS_NOTLOADED && *mod->name != '*')
	{
		mod->loadstate = MLS_LOADING;
//		if (verbose == MLV_ERROR)	//if its fatal on failure (ie: world), do it on the main thread and block to wait for it.
//			Mod_LoadModelWorker(mod, MLV_WARN, 0);
//		else
		if (verbose == MLV_ERROR || verbose == MLV_WARNSYNC)
			Mod_LoadModelWorker(mod, NULL, verbose, 0);
//			COM_AddWork(WG_MAIN, Mod_LoadModelWorker, mod, NULL, verbose, 0);
		else
			COM_AddWork(WG_LOADER, Mod_LoadModelWorker, mod, NULL, verbose, 0);
	}

	if (verbose == MLV_ERROR)
	{
		//someone already tried to load it without caring if it failed or not. make sure its loaded.
		//fixme: this is a spinloop.
		if (mod->loadstate == MLS_LOADING)
			COM_WorkerPartialSync(mod, &mod->loadstate, MLS_LOADING);

		if (mod->loadstate != MLS_LOADED)
			Host_EndGame ("Mod_NumForName: %s not found or couldn't load", mod->name);
	}
	return mod;
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t *Mod_ForName (const char *name, enum mlverbosity_e verbosity)
{
	model_t	*mod;
	
	mod = Mod_FindName (name);
	
	return Mod_LoadModel (mod, verbosity);
}


/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

#if !defined(SERVERONLY)
static const struct
{
	const char *oldname;
	unsigned int chksum;	//xor-compacted md4
	const char *newname;
} buggytextures[] =
{
	//FIXME: we should load this table from disk or something.
	//old			sum			new
	{"metal5_2",	0x45d110ec,	"metal5_2_arc"},
	{"metal5_2",	0x0d275f87,	"metal5_2_x"},
	{"metal5_4",	0xf8e27da8,	"metal5_4_arc"},
	{"metal5_4",	0xa301c52e,	"metal5_4_double"},
	{"metal5_8",	0xfaa8bf77,	"metal5_8_back"},
	{"metal5_8",	0x88792923,	"metal5_8_rune"},
	{"plat_top1",	0xfe4f9f5a,	"plat_top1_bolt"},
	{"plat_top1",	0x9ac3fccf,	"plat_top1_cable"},
	{"sky4",		0xde688b77,	"sky1"},
//	{"sky4",		0x8a010dc0,	"sky4"},
//	{"window03",	?,			"window03_?"},
//	{"window03",	?,			"window03_?"},


	//FIXME: hexen2 has the same issue.
};
static const char *Mod_RemapBuggyTexture(const char *name, const qbyte *data, unsigned int datalen)
{
	unsigned int i;
	if (!data)
		return NULL;
	for (i = 0; i < sizeof(buggytextures)/sizeof(buggytextures[0]); i++)
	{
		if (!strcmp(name, buggytextures[i].oldname))
		{
			unsigned int sum = Com_BlockChecksum(data, datalen);
			for (; i < sizeof(buggytextures)/sizeof(buggytextures[0]); i++)
			{
				if (strcmp(name, buggytextures[i].oldname))
					break;
				if (sum == buggytextures[i].chksum)
					return buggytextures[i].newname;
			}
			break;
		}
	}
	return NULL;
}

static void Mod_FinishTexture(texture_t *tx, const char *loadname, qboolean safetoloadfromwads)
{
	extern cvar_t gl_shadeq1_name;
	char altname[MAX_QPATH];
	char *star;
	const char *origname = NULL;
	const char *shadername = tx->name;

	if (!safetoloadfromwads)
	{

		/*skies? just replace with the override sky*/
		if (!strncmp(tx->name, "sky", 3) && *cl.skyname)
			tx->shader = R_RegisterCustom (va("skybox_%s", cl.skyname), SUF_NONE, Shader_DefaultSkybox, NULL);	//just load the regular name.
		else
		{
			//remap to avoid bugging out on textures with the same name and different images (vanilla content sucks)
			shadername = Mod_RemapBuggyTexture(shadername, tx->mips[0], tx->width*tx->height);
			if (shadername)
				origname = tx->name;
			else
				shadername = tx->name;

			//find the *
			if (!*gl_shadeq1_name.string || !strcmp(gl_shadeq1_name.string, "*"))
				;
			else if (!(star = strchr(gl_shadeq1_name.string, '*')) || (strlen(gl_shadeq1_name.string)+strlen(tx->name)+1>=sizeof(altname)))	//it's got to fit.
				shadername = gl_shadeq1_name.string;
			else
			{
				strncpy(altname, gl_shadeq1_name.string, star-gl_shadeq1_name.string);	//copy the left
				altname[star-gl_shadeq1_name.string] = '\0';
				strcat(altname, shadername);	//insert the *
				strcat(altname, star+1);	//add any final text.
				shadername = altname;
			}

			tx->shader = R_RegisterCustom (shadername, SUF_LIGHTMAP, Shader_DefaultBSPQ1, NULL);
		}

		if (!tx->mips[0] && !safetoloadfromwads)
			return;
	}
	else
	{	//already loaded. don't waste time / crash (this will be a dead pointer).
		if (tx->mips[0])
			return;
	}

	if (!strncmp(tx->name, "sky", 3))
		R_InitSky (tx->shader, shadername, tx->mips[0], tx->width, tx->height);
	else
	{
		uploadfmt_t fmt;
		unsigned int maps = 0;
		maps |= SHADER_HASPALETTED;
		maps |= SHADER_HASDIFFUSE;
		if (r_fb_bmodels.ival)
			maps |= SHADER_HASFULLBRIGHT;
		if (r_loadbumpmapping || ((r_waterstyle.ival > 1 || r_telestyle.ival > 1) && *tx->name == '*') || tx->shader->defaulttextures->reflectcube)
			maps |= SHADER_HASNORMALMAP;
		if (gl_specular.ival)
			maps |= SHADER_HASGLOSS;

		if (tx->palette)
		{	//halflife, probably...
			if (*tx->name == '{')
				fmt = TF_MIP4_8PAL24_T255;
			else
				fmt = TF_MIP4_8PAL24;
		}
		else
		{
			if (*tx->name == '{')
				fmt = TF_TRANS8;
			else
				fmt = TF_MIP4_SOLID8;
		}

		R_BuildLegacyTexnums(tx->shader, origname, loadname, maps, 0, fmt, tx->width, tx->height, tx->mips, tx->palette);
	}
	BZ_Free(tx->mips[0]);
}
#endif

void Mod_NowLoadExternal(model_t *loadmodel)
{
	//for halflife bsps where wads are loaded after the map.
#if !defined(SERVERONLY)
	int i;
	texture_t	*tx;
	char loadname[32];
	COM_FileBase (cl.worldmodel->name, loadname, sizeof(loadname));
	
	if (!strncmp(loadname, "b_", 2))
		Q_strncpyz(loadname, "bmodels", sizeof(loadname));

	for (i=0 ; i<loadmodel->numtextures ; i++)
	{
		tx = loadmodel->textures[i];
		if (!tx)	//e1m2, this happens
			continue;

		if (tx->mips[0])
			continue;

		Mod_FinishTexture(tx, loadname, true);
	}
#endif
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

typedef struct
{
	unsigned int magic; //"QLIT"
	unsigned int version; //2
	unsigned int numsurfs;
	unsigned int lmsize;	//samples, not bytes (same size as vanilla lighting lump in a q1 bsp).

	//uint		lmoffsets[numsurfs];	//completely overrides the bsp lightmap info
	//ushort	lmextents[numsurfs*2];	//only to avoid precision issues. width+height pairs, actual lightmap sizes on disk (so +1).
	//byte		lmstyles[numsurfs*4];	//completely overrides the bsp lightmap info
	//byte		lmshifts[numsurfs];		//default is 4 (1<<4=16), for 1/16th lightmap-to-texel ratio
	//byte		litdata[lmsize*3];		//rgb data
	//byte		luxdata[lmsize*3];		//stn light dirs (unsigned bytes
} qlit2_t;

/*
=================
Mod_LoadLighting
=================
*/
void Mod_LoadLighting (model_t *loadmodel, qbyte *mod_base, lump_t *l, qboolean interleaveddeluxe, lightmapoverrides_t *overrides)
{
	qboolean luxtmp = true;
	qboolean littmp = true;
	qbyte *luxdata = NULL;
	qbyte *litdata = NULL;
	qbyte *lumdata = NULL;
	qbyte *out;
	unsigned int samples;

	extern cvar_t gl_overbright;
	loadmodel->engineflags &= ~MDLF_RGBLIGHTING;

	//q3 maps have built in 4-fold overbright.
	//if we're not rendering with that, we need to brighten the lightmaps in order to keep the darker parts the same brightness. we loose the 2 upper bits. those bright areas become uniform and indistinct.
	if (loadmodel->fromgame == fg_quake3)
	{
		gl_overbright.flags |= CVAR_RENDERERLATCH;
		BuildLightMapGammaTable(1, (1<<(2-gl_overbright.ival)));
	}
	else
	//lit file light intensity is made to match the world's light intensity.
	BuildLightMapGammaTable(1, 1);

	loadmodel->lightdata = NULL;
	loadmodel->deluxdata = NULL;
	if (loadmodel->fromgame == fg_halflife || loadmodel->fromgame == fg_quake2 || loadmodel->fromgame == fg_quake3)
	{
		litdata = mod_base + l->fileofs;
		samples = l->filelen/3;
	}
	else
	{
		lumdata = mod_base + l->fileofs;
		samples = l->filelen;
	}
	if (interleaveddeluxe)
		samples >>= 1;
	if (!samples)
	{
		litdata = Q1BSPX_FindLump("RGBLIGHTING", &samples);
		samples /= 3;
		if (!samples)
			return;
	}

#ifndef SERVERONLY
	if (!litdata && r_loadlits.value)
	{
		char *litnames[] = {
			"%s.lit2",
			"%s.lit",
			"lits/%s.lit2",
			"lits/%s.lit"
		};
		char litbasep[MAX_QPATH];
		char litbase[MAX_QPATH];
		int depth;
		int bestdepth = 0x7fffffff;
		int best = -1;
		int i;
		char litname[MAX_QPATH];
		size_t litsize;
		qboolean inhibitvalidation = false;

		COM_StripExtension(loadmodel->name, litbasep, sizeof(litbasep));
		COM_FileBase(loadmodel->name, litbase, sizeof(litbase));
		for (i = 0; i < sizeof(litnames)/sizeof(litnames[0]); i++)
		{
			if (temp_lit2support.ival && !(i & 1))
				continue;
			if (strchr(litnames[i], '/'))
				Q_snprintfz(litname, sizeof(litname), litnames[i], litbase);
			else
				Q_snprintfz(litname, sizeof(litname), litnames[i], litbasep);
			depth = COM_FDepthFile(litname, false);
			if (depth < bestdepth)
			{
				bestdepth = depth;
				best = i;
			}
		}
		if (best >= 0)
		{
			if (strchr(litnames[best], '/'))
				Q_snprintfz(litname, sizeof(litname), litnames[best], litbase);
			else
				Q_snprintfz(litname, sizeof(litname), litnames[best], litbasep);
			litdata = FS_LoadMallocGroupFile(&loadmodel->memgroup, litname, &litsize);
		}
		else
		{
			litdata = NULL;
			litsize = 0;
		}

		if (litdata && litsize >= 8)
		{	//validate it, if we loaded one.
			if (litdata[0] != 'Q' || litdata[1] != 'L' || litdata[2] != 'I' || litdata[3] != 'T')
			{
				litdata = NULL;
				Con_Printf("lit \"%s\" isn't a lit\n", litname);
			}
			else if (LittleLong(*(int *)&litdata[4]) == 1 && l->filelen && samples*3 != (litsize-8))
			{
				litdata = NULL;
				Con_Printf("lit \"%s\" doesn't match level. Ignored.\n", litname);
			}
			else if (LittleLong(*(int *)&litdata[4]) == 1)
			{
				//header+version
				litdata += 8;
			}
			else if (LittleLong(*(int *)&litdata[4]) == 2 && overrides)
			{
				qlit2_t *ql2 = (qlit2_t*)litdata;
				unsigned int *offsets = (unsigned int*)(ql2+1);
				unsigned short *extents = (unsigned short*)(offsets+ql2->numsurfs);
				unsigned char *styles = (unsigned char*)(extents+ql2->numsurfs*2);
				unsigned char *shifts = (unsigned char*)(styles+ql2->numsurfs*4);
				if (!temp_lit2support.ival)
				{
					litdata = NULL;
					Con_Printf("lit2 support is disabled, pending format finalisation (%s).\n", litname);
				}
				else if (loadmodel->numsurfaces != ql2->numsurfs)
				{
					litdata = NULL;
					Con_Printf("lit \"%s\" doesn't match level. Ignored.\n", litname);
				}
				else
				{
					inhibitvalidation = true;

					//surface code needs to know the overrides.
					overrides->offsets = offsets;
					overrides->extents = extents;
					overrides->styles = styles;
					overrides->shifts = shifts;

					//we're now using this amount of data.
					samples = ql2->lmsize;

					litdata = shifts+ql2->numsurfs;
					if (r_deluxmapping)
						luxdata = litdata+samples*3;
				}
			}
			else
			{
				Con_Printf("lit \"%s\" isn't version 1 or 2.\n", litname);
				litdata = NULL;
			}
		}

		littmp = false;
		if (!litdata)
		{
			int size;
			/*FIXME: bspx support for extents+lmscale, may require style+offset lumps too, not sure what to do here*/
			litdata = Q1BSPX_FindLump("RGBLIGHTING", &size);
			if (size != samples*3)
				litdata = NULL;
			littmp = true;
		}
		else if (!inhibitvalidation)
		{
			if (lumdata)
			{
				float prop;
				int i;
				qbyte *lum;
				qbyte *lit;

				//now some cheat protection.
				lum = lumdata;
				lit = litdata;

				for (i = 0; i < samples; i++)	//force it to the same intensity. (or less, depending on how you see it...)
				{
#define m(a, b, c) (a>(b>c?b:c)?a:(b>c?b:c))
					prop = (float)m(lit[0],  lit[1], lit[2]);

					if (!prop)
					{
						lit[0] = *lum;
						lit[1] = *lum;
						lit[2] = *lum;
					}
					else
					{
						prop = *lum / prop;
						lit[0] *= prop;
						lit[1] *= prop;
						lit[2] *= prop;
					}

					lum++;
					lit+=3;
				}
				//end anti-cheat
			}
		}
	}


	if (!luxdata && r_loadlits.ival && r_deluxmapping)
	{	//the map util has a '-scalecos X' parameter. use 0 if you're going to use only just lux. without lux scalecos 0 is hideous.
		char luxname[MAX_QPATH];
		size_t luxsz = 0;
		*luxname = 0;
		if (!luxdata)
		{							
			Q_strncpyz(luxname, loadmodel->name, sizeof(luxname));
			COM_StripExtension(loadmodel->name, luxname, sizeof(luxname));
			COM_DefaultExtension(luxname, ".lux", sizeof(luxname));
			luxdata = FS_LoadMallocGroupFile(&loadmodel->memgroup, luxname, &luxsz);
			luxtmp = false;
		}
		if (!luxdata)
		{
			Q_strncpyz(luxname, "luxs/", sizeof(luxname));
			COM_StripExtension(COM_SkipPath(loadmodel->name), luxname+5, sizeof(luxname)-5);
			Q_strncatz(luxname, ".lux", sizeof(luxname));

			luxdata = FS_LoadMallocGroupFile(&loadmodel->memgroup, luxname, &luxsz);
			luxtmp = false;
		}
		if (!luxdata) //dp...
		{
			COM_StripExtension(loadmodel->name, luxname, sizeof(luxname));
			COM_DefaultExtension(luxname, ".dlit", sizeof(luxname));
			luxdata = FS_LoadMallocGroupFile(&loadmodel->memgroup, luxname, &luxsz);
			luxtmp = false;
		}
		//make sure the .lux has the correct size
		if (luxdata && l->filelen && l->filelen != (luxsz-8)/3)
		{
			Con_Printf("deluxmap \"%s\" doesn't match level. Ignored.\n", luxname);
			luxdata=NULL;
		}
		if (!luxdata)
		{
			int size;
			luxdata = Q1BSPX_FindLump("LIGHTINGDIR", &size);
			if (size != samples*3)
				luxdata = NULL;
			luxtmp = true;
		}
		else
		{
			if (luxdata[0] == 'Q' && luxdata[1] == 'L' && luxdata[2] == 'I' && luxdata[3] == 'T')
			{
				if (LittleLong(*(int *)&luxdata[4]) == 1)
					luxdata+=8;
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
#endif

#ifdef RUNTIMELIGHTING
	if (!lightmodel && r_loadlits.value == 2 && (!litdata || (!luxdata && r_deluxmapping)))
	{
		writelitfile = !litdata;
		numlightdata = l->filelen;
		lightmodel = loadmodel;
		relitsurface = 0;
	}
	else if (!lightmodel && r_deluxmapping_cvar.value>1 && r_deluxmapping && !luxdata
#ifdef RTLIGHTS
		&& !(r_shadow_realtime_world.ival && r_shadow_realtime_world_lightmaps.value<=0)
#endif
		)
	{	//if deluxemapping is on, generate missing lux files a little more often, but don't bother if we have rtlights on anyway.
		writelitfile = false;
		numlightdata = l->filelen;
		lightmodel = loadmodel;
		relitsurface = 0;
	}

	/*if we're relighting, make sure there's the proper lit data to be updated*/
	if (lightmodel == loadmodel && !litdata)
	{
		int i;
		litdata = ZG_Malloc(&loadmodel->memgroup, samples*3);
		littmp = false;
		if (lumdata)
		{
			for (i = 0; i < samples; i++)
			{
				litdata[i*3+0] = lumdata[i];
				litdata[i*3+1] = lumdata[i];
				litdata[i*3+2] = lumdata[i];
			}
			lumdata = NULL;
		}
	}
	/*if we're relighting, make sure there's the proper lux data to be updated*/
	if (lightmodel == loadmodel && r_deluxmapping && !luxdata)
	{
		int i;
		luxdata = ZG_Malloc(&loadmodel->memgroup, samples*3);
		for (i = 0; i < samples; i++)
		{
			luxdata[i*3+0] = 0.5f*255;
			luxdata[i*3+1] = 0.5f*255;
			luxdata[i*3+2] = 255;
		}
	}
#endif

	if (overrides && !overrides->shifts)
	{
		int size;
		overrides->shifts = Q1BSPX_FindLump("LMSHIFT", &size);
		if (size != loadmodel->numsurfaces)
			overrides->shifts = NULL;

		//if we have shifts, then we probably also have legacy data in the surfaces that we want to override
		if (!overrides->offsets)
		{
			int size;
			overrides->offsets = Q1BSPX_FindLump("LMOFFSET", &size);
			if (size != loadmodel->numsurfaces * sizeof(int))
				overrides->offsets = NULL;
		}
		if (!overrides->styles)
		{
			int size;
			overrides->styles = Q1BSPX_FindLump("LMSTYLE", &size);
			if (size != loadmodel->numsurfaces * sizeof(qbyte)*MAXQ1LIGHTMAPS)
				overrides->styles = NULL;
		}
	}
	
	if (luxdata && luxtmp)
	{
		loadmodel->engineflags |= MDLF_RGBLIGHTING;
		loadmodel->deluxdata = ZG_Malloc(&loadmodel->memgroup, samples*3);
		memcpy(loadmodel->deluxdata, luxdata, samples*3);
	}
	else if (luxdata)
	{
		loadmodel->deluxdata = luxdata;
	}
	else if (interleaveddeluxe)
		loadmodel->deluxdata = ZG_Malloc(&loadmodel->memgroup, samples*3);

	if (litdata && littmp)
	{
		loadmodel->engineflags |= MDLF_RGBLIGHTING;
		loadmodel->lightdata = ZG_Malloc(&loadmodel->memgroup, samples*3);
		/*the memcpy is below*/
		samples*=3;
	}
	else if (litdata)
	{
		loadmodel->engineflags |= MDLF_RGBLIGHTING;
		loadmodel->lightdata = litdata;
		samples*=3;
	}
	else if (lumdata)
	{
		loadmodel->engineflags &= ~MDLF_RGBLIGHTING;
		loadmodel->lightdata = ZG_Malloc(&loadmodel->memgroup, samples);
		litdata = lumdata;
	}

	/*apply lightmap gamma to the entire lightmap*/
	loadmodel->lightdatasize = samples;
	out = loadmodel->lightdata;
	if (interleaveddeluxe)
	{
		qbyte *luxout = loadmodel->deluxdata;
		samples /= 3;
		while(samples-- > 0)
		{
			*out++ = lmgamma[*litdata++];
			*out++ = lmgamma[*litdata++];
			*out++ = lmgamma[*litdata++];
			*luxout++ = *litdata++;
			*luxout++ = *litdata++;
			*luxout++ = *litdata++;
		}
	}
	else
	{
		while(samples-- > 0)
		{
			*out++ = lmgamma[*litdata++];
		}
	}

#ifndef SERVERONLY
	if ((loadmodel->engineflags & MDLF_RGBLIGHTING) && r_lightmap_saturation.value != 1.0f)
		SaturateR8G8B8(loadmodel->lightdata, l->filelen, r_lightmap_saturation.value);
#endif
}

//scans through the worldspawn for a single specific key.
const char *Mod_ParseWorldspawnKey(model_t *mod, const char *key, char *buffer, size_t sizeofbuffer)
{
	char keyname[64];
	char value[1024];
	const char *ents = Mod_GetEntitiesString(mod);
	while(ents && *ents)
	{
		ents = COM_ParseOut(ents, keyname, sizeof(keyname));
		if (*keyname == '{')	//an entity
		{
			while (ents && *ents)
			{
				ents = COM_ParseOut(ents, keyname, sizeof(keyname));
				if (*keyname == '}')
					break;
				ents = COM_ParseOut(ents, value, sizeof(value));
				if (!strcmp(keyname, key) || (*keyname == '_' && !strcmp(keyname+1, key)))
				{
					Q_strncpyz(buffer, value, sizeofbuffer);
					return buffer;
				}
			}
			return "";	//worldspawn only.
		}
	}
	return "";	//err...
}

static void Mod_SaveEntFile_f(void)
{
	char fname[MAX_QPATH];
	model_t *mod = NULL;
	char *n = Cmd_Argv(1);
	const char *ents;
	if (*n)
		mod = Mod_ForName(n, MLV_WARN);
#ifndef CLIENTONLY
	if (sv.state && !mod)
		mod = sv.world.worldmodel;
#endif
#ifndef SERVERONLY
	if (cls.state && !mod)
		mod = cl.worldmodel;
#endif
	if (mod && mod->loadstate == MLS_LOADING)
		COM_WorkerPartialSync(mod, &mod->loadstate, MLS_LOADING);
	if (!mod || mod->loadstate != MLS_LOADED)
	{
		Con_Printf("Map not loaded\n");
		return;
	}
	ents = Mod_GetEntitiesString(mod);
	if (!ents)
	{
		Con_Printf("Map is not a map, and has no entities\n");
		return;
	}

	if (*mod_loadentfiles_dir.string && !strncmp(mod->name, "maps/", 5))
	{
		Q_snprintfz(fname, sizeof(fname), "maps/%s/%s", mod_loadentfiles_dir.string, mod->name+5);
		COM_StripExtension(fname, fname, sizeof(fname));
		Q_strncatz(fname, ".ent", sizeof(fname));
	}
	else
	{
		COM_StripExtension(mod->name, fname, sizeof(fname));
		Q_strncatz(fname, ".ent", sizeof(fname));
	}

	COM_WriteFile(fname, FS_GAMEONLY, ents, strlen(ents));
}

/*
=================
Mod_LoadEntities
=================
*/
void Mod_LoadEntities (model_t *loadmodel, qbyte *mod_base, lump_t *l)
{
	char fname[MAX_QPATH];
	size_t sz;
	char keyname[64];
	char value[1024];
	char *ents = NULL, *k;
	int t;

	Mod_SetEntitiesString(loadmodel, NULL, false);
	if (!l->filelen)
		return;

	if (mod_loadentfiles.value && !ents && *mod_loadentfiles_dir.string)
	{
		if (!strncmp(loadmodel->name, "maps/", 5))
		{
			Q_snprintfz(fname, sizeof(fname), "maps/%s/%s", mod_loadentfiles_dir.string, loadmodel->name+5);
			COM_StripExtension(fname, fname, sizeof(fname));
			Q_strncatz(fname, ".ent", sizeof(fname));
			ents = FS_LoadMallocFile(fname, &sz);
		}
	}
	if (mod_loadentfiles.value && !ents)
	{
		COM_StripExtension(loadmodel->name, fname, sizeof(fname));
		Q_strncatz(fname, ".ent", sizeof(fname));
		ents = FS_LoadMallocFile(fname, &sz);
	}
	if (mod_loadentfiles.value && !ents)
	{	//tenebrae compat
		COM_StripExtension(loadmodel->name, fname, sizeof(fname));
		Q_strncatz(fname, ".edo", sizeof(fname));
		ents = FS_LoadMallocFile(fname, &sz);
	}
	if (!ents)
	{
		ents = Z_Malloc(l->filelen + 1);	
		memcpy (ents, mod_base + l->fileofs, l->filelen);
		ents[l->filelen] = 0;
	}
	else
		loadmodel->entitiescrc = QCRC_Block(ents, strlen(ents));

	Mod_SetEntitiesString(loadmodel, ents, false);

	while(ents && *ents)
	{
		ents = COM_ParseOut(ents, keyname, sizeof(keyname));
		if (*keyname == '{')	//an entity
		{
			while (ents && *ents)
			{
				ents = COM_ParseOut(ents, keyname, sizeof(keyname));
				if (*keyname == '}')
					break;
				ents = COM_ParseOut(ents, value, sizeof(value));
				if (!strncmp(keyname, "_texpart_", 9) || !strncmp(keyname, "texpart_", 8))
				{
					k = keyname + ((*keyname=='_')?9:8);
					for (t = 0; t < loadmodel->numtextures; t++)
					{
						if (!strcmp(k, loadmodel->textures[t]->name))
						{
							loadmodel->textures[t]->partname = ZG_Malloc(&loadmodel->memgroup, strlen(value)+1);
							strcpy(loadmodel->textures[t]->partname, value);
							break;
						}
					}
					if (t == loadmodel->numtextures)
						Con_Printf("\"%s\" is not valid for %s\n", keyname, loadmodel->name);
				}
			}
		}
	}
}


/*
=================
Mod_LoadVertexes
=================
*/
qboolean Mod_LoadVertexes (model_t *loadmodel, qbyte *mod_base, lump_t *l)
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
	out = ZG_Malloc(&loadmodel->memgroup, count*sizeof(*out));	

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

qboolean Mod_LoadVertexNormals (model_t *loadmodel, qbyte *mod_base, lump_t *l)
{
	float	*in;
	float	*out;
	int			i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(vec3_t))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n", loadmodel->name);
		return false;
	}
	count = l->filelen / sizeof(vec3_t);

	if (count != loadmodel->numvertexes)
		return false;	//invalid number of verts there, can't use this.
	out = ZG_Malloc(&loadmodel->memgroup, count*sizeof(vec3_t));	
	loadmodel->normals = (vec3_t*)out;
	for ( i=0 ; i<count ; i++, in+=3, out+=3)
	{
		out[0] = LittleFloat (in[0]);
		out[1] = LittleFloat (in[1]);
		out[2] = LittleFloat (in[2]);
	}
	return true;
}

#if defined(Q1BSPS) || defined(Q2BSPS)
void ModQ1_Batches_BuildQ1Q2Poly(model_t *mod, msurface_t *surf, builddata_t *cookie)
{
	unsigned int vertidx;
	int i, lindex, edgevert;
	mesh_t *mesh = surf->mesh;
	float *vec;
	float s, t, d;
	int sty;
//	int w,h;

	if (!mesh)
	{
		mesh = surf->mesh = ZG_Malloc(&mod->memgroup, sizeof(mesh_t) + (sizeof(vecV_t)+sizeof(vec2_t)*(1+1)+sizeof(vec3_t)*3+sizeof(vec4_t)*1)* surf->numedges + sizeof(index_t)*(surf->numedges-2)*3);
		mesh->numvertexes = surf->numedges;
		mesh->numindexes = (mesh->numvertexes-2)*3;
		mesh->xyz_array = (vecV_t*)(mesh+1);
		mesh->st_array = (vec2_t*)(mesh->xyz_array+mesh->numvertexes);
		mesh->lmst_array[0] = (vec2_t*)(mesh->st_array+mesh->numvertexes);
		mesh->normals_array = (vec3_t*)(mesh->lmst_array[0]+mesh->numvertexes);
		mesh->snormals_array = (vec3_t*)(mesh->normals_array+mesh->numvertexes);
		mesh->tnormals_array = (vec3_t*)(mesh->snormals_array+mesh->numvertexes);
		mesh->colors4f_array[0] = (vec4_t*)(mesh->tnormals_array+mesh->numvertexes);
		mesh->indexes = (index_t*)(mesh->colors4f_array[0]+mesh->numvertexes);
	}
	mesh->istrifan = true;

	//output the mesh's indicies
	for (i=0 ; i<mesh->numvertexes-2 ; i++)
	{
		mesh->indexes[i*3] = 0;
		mesh->indexes[i*3+1] = i+1;
		mesh->indexes[i*3+2] = i+2;
	}
	//output the renderable verticies
	for (i=0 ; i<mesh->numvertexes ; i++)
	{
		lindex = mod->surfedges[surf->firstedge + i];
		edgevert = lindex <= 0;
		if (edgevert)
			lindex = -lindex;
		if (lindex < 0 || lindex >= mod->numedges)
			vertidx = 0;
		else
			vertidx = mod->edges[lindex].v[edgevert];
		vec = mod->vertexes[vertidx].position;

		s = DotProduct (vec, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3];
		t = DotProduct (vec, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3];

		VectorCopy (vec, mesh->xyz_array[i]);

/*		if (R_GetShaderSizes(surf->texinfo->texture->shader, &w, &h, false) > 0)
		{
			mesh->st_array[i][0] = s/w;
			mesh->st_array[i][1] = t/h;
		}
		else
*/
		{
			mesh->st_array[i][0] = s;
			mesh->st_array[i][1] = t;
			if (surf->texinfo->texture->width)
				mesh->st_array[i][0] /= surf->texinfo->texture->width;
			if (surf->texinfo->texture->height)
				mesh->st_array[i][1] /= surf->texinfo->texture->height;
		}

#ifndef SERVERONLY
		if (gl_lightmap_average.ival)
		{
			for (sty = 0; sty < 1; sty++)
			{
				mesh->lmst_array[sty][i][0] = (surf->extents[0]*0.5 + (surf->light_s[sty]<<surf->lmshift) + (1<<surf->lmshift)*0.5) / (mod->lightmaps.width<<surf->lmshift);
				mesh->lmst_array[sty][i][1] = (surf->extents[1]*0.5 + (surf->light_t[sty]<<surf->lmshift) + (1<<surf->lmshift)*0.5) / (mod->lightmaps.height<<surf->lmshift);
			}
		}
		else
#endif
		{
			for (sty = 0; sty < 1; sty++)
			{
				mesh->lmst_array[sty][i][0] = (s - surf->texturemins[0] + (surf->light_s[sty]<<surf->lmshift) + (1<<surf->lmshift)*0.5) / (mod->lightmaps.width<<surf->lmshift);
				mesh->lmst_array[sty][i][1] = (t - surf->texturemins[1] + (surf->light_t[sty]<<surf->lmshift) + (1<<surf->lmshift)*0.5) / (mod->lightmaps.height<<surf->lmshift);
			}
		}

		//figure out the texture directions, for bumpmapping and stuff
		if (mod->normals && (surf->texinfo->flags & 0x800) && (mod->normals[vertidx][0] || mod->normals[vertidx][1] || mod->normals[vertidx][2])) 
		{
			//per-vertex normals - used for smoothing groups and stuff.
			VectorCopy(mod->normals[vertidx], mesh->normals_array[i]);
		}
		else
		{
			if (surf->flags & SURF_PLANEBACK)
				VectorNegate(surf->plane->normal, mesh->normals_array[i]);
			else
				VectorCopy(surf->plane->normal, mesh->normals_array[i]);
		}
		VectorCopy(surf->texinfo->vecs[0], mesh->snormals_array[i]);
		VectorNegate(surf->texinfo->vecs[1], mesh->tnormals_array[i]);
		//the s+t vectors are axis-aligned, so fiddle them so they're normal aligned instead
		d = -DotProduct(mesh->normals_array[i], mesh->snormals_array[i]);
		VectorMA(mesh->snormals_array[i], d, mesh->normals_array[i], mesh->snormals_array[i]);
		d = -DotProduct(mesh->normals_array[i], mesh->tnormals_array[i]);
		VectorMA(mesh->tnormals_array[i], d, mesh->normals_array[i], mesh->tnormals_array[i]);
		VectorNormalize(mesh->snormals_array[i]);
		VectorNormalize(mesh->tnormals_array[i]);

		//q1bsp has no colour information (fixme: sample from the lightmap?)
		for (sty = 0; sty < 1; sty++)
		{
			mesh->colors4f_array[sty][i][0] = 1;
			mesh->colors4f_array[sty][i][1] = 1;
			mesh->colors4f_array[sty][i][2] = 1;
			mesh->colors4f_array[sty][i][3] = 1;
		}
	}
}
#endif

#ifndef SERVERONLY
static void Mod_Batches_BuildModelMeshes(model_t *mod, int maxverts, int maxindicies, void (*build)(model_t *mod, msurface_t *surf, builddata_t *bd), builddata_t *bd, int lmmerge)
{
	batch_t *batch;
	msurface_t *surf;
	mesh_t *mesh;
	int numverts = 0;
	int numindicies = 0;
	int j, i;
	int sortid;
	int sty;
	vbo_t vbo;
	int styles = mod->lightmaps.surfstyles;
	char *ptr;

	memset(&vbo, 0, sizeof(vbo));
	vbo.indicies.sysptr = ZG_Malloc(&mod->memgroup, sizeof(index_t) * maxindicies);
	ptr = ZG_Malloc(&mod->memgroup, (sizeof(vecV_t)+sizeof(vec2_t)*(1+styles)+sizeof(vec3_t)*3+sizeof(vec4_t)*styles)* maxverts);

	vbo.coord.sysptr = ptr;
	ptr += sizeof(vecV_t)*maxverts;
	for (sty = 0; sty < styles; sty++)
	{
		vbo.colours[sty].sysptr = ptr;
		ptr += sizeof(vec4_t)*maxverts;
	}
	for (; sty < MAXRLIGHTMAPS; sty++)
		vbo.colours[sty].sysptr = NULL;
	vbo.texcoord.sysptr = ptr;
	ptr += sizeof(vec2_t)*maxverts;
	sty = 0;
	for (; sty < styles; sty++)
	{
		vbo.lmcoord[sty].sysptr = ptr;
		ptr += sizeof(vec2_t)*maxverts;
	}
	for (; sty < MAXRLIGHTMAPS; sty++)
		vbo.lmcoord[sty].sysptr = NULL;
	vbo.normals.sysptr = ptr;
	ptr += sizeof(vec3_t)*maxverts;
	vbo.svector.sysptr = ptr;
	ptr += sizeof(vec3_t)*maxverts;
	vbo.tvector.sysptr = ptr;
	ptr += sizeof(vec3_t)*maxverts;

	numindicies = 0;
	numverts = 0;

	//build each mesh
	for (sortid=0; sortid<SHADER_SORT_COUNT; sortid++)
	{
		for (batch = mod->batches[sortid]; batch; batch = batch->next)
		{
			for (j = 0; j < batch->maxmeshes; j++)
			{
				surf = (msurface_t*)batch->mesh[j];
				mesh = surf->mesh;
				batch->mesh[j] = mesh;

				mesh->vbofirstvert = numverts;
				mesh->vbofirstelement = numindicies;
				numverts += mesh->numvertexes;
				numindicies += mesh->numindexes;

				//set up the arrays. the arrangement is required for the backend to optimise vbos
				mesh->xyz_array = (vecV_t*)vbo.coord.sysptr + mesh->vbofirstvert;
				mesh->st_array = (vec2_t*)vbo.texcoord.sysptr + mesh->vbofirstvert;
				for (sty = 0; sty < MAXRLIGHTMAPS; sty++)
				{
					if (vbo.lmcoord[sty].sysptr)
						mesh->lmst_array[sty] = (vec2_t*)vbo.lmcoord[sty].sysptr + mesh->vbofirstvert;
					else
						mesh->lmst_array[sty] = NULL;
					if (vbo.colours[sty].sysptr)
						mesh->colors4f_array[sty] = (vec4_t*)vbo.colours[sty].sysptr + mesh->vbofirstvert;
					else
						mesh->colors4f_array[sty] = NULL;
				}
				mesh->normals_array = (vec3_t*)vbo.normals.sysptr + mesh->vbofirstvert;
				mesh->snormals_array = (vec3_t*)vbo.svector.sysptr + mesh->vbofirstvert;
				mesh->tnormals_array = (vec3_t*)vbo.tvector.sysptr + mesh->vbofirstvert;
				mesh->indexes = (index_t*)vbo.indicies.sysptr + mesh->vbofirstelement;

				mesh->vbofirstvert = 0;
				mesh->vbofirstelement = 0;

				build(mod, surf, bd);

				if (lmmerge != 1)
				for (sty = 0; sty < MAXRLIGHTMAPS; sty++)
				{
					if (surf->lightmaptexturenums[sty] >= 0)
					{
						if (mesh->lmst_array[sty])
						{
							for (i = 0; i < mesh->numvertexes; i++)
							{
								mesh->lmst_array[sty][i][1] += surf->lightmaptexturenums[sty] % lmmerge;
								mesh->lmst_array[sty][i][1] /= lmmerge;
							}
						}
						surf->lightmaptexturenums[sty] /= lmmerge;
					}
				}
			}
			batch->meshes = 0;
			batch->firstmesh = 0;
		}
	}
}

#ifdef Q1BSPS
//q1 autoanimates. if the frame is set, it uses the alternate animation.
static void Mod_UpdateBatchShader_Q1 (struct batch_s *batch)
{
	texture_t *base = batch->texture;
	int		reletive;
	int		count;

	if (batch->ent->framestate.g[FS_REG].frame[0])
	{
		if (base->alternate_anims)
			base = base->alternate_anims;
	}

	if (base->anim_total)
	{
		reletive = (int)(cl.time*10) % base->anim_total;

		count = 0;
		while (base->anim_min > reletive || base->anim_max <= reletive)
		{
			base = base->anim_next;
			if (!base)
				Sys_Error ("R_TextureAnimation: broken cycle");
			if (++count > 100)
				Sys_Error ("R_TextureAnimation: infinite cycle");
		}
	}

	batch->shader = base->shader;
}
#endif

#ifdef Q2BSPS
//q2 has direct control over the texture frames used, but typically has the client generate the frame (different flags autogenerate different ranges).
static void Mod_UpdateBatchShader_Q2 (struct batch_s *batch)
{
	texture_t *base = batch->texture;
	int		reletive;
	int frame = batch->ent->framestate.g[FS_REG].frame[0];
	if (batch->ent == &r_worldentity)
		frame = cl.time*2;

	if (base->anim_total)
	{
		reletive = frame % base->anim_total;
		while (reletive --> 0)
		{
			base = base->anim_next;
			if (!base)
				Sys_Error ("R_TextureAnimation: broken cycle");
		}
	}

	batch->shader = base->shader;
}
#endif

#define lmmerge(i) ((i>=0)?i/merge:i)
/*
batch->firstmesh is set only in and for this function, its cleared out elsewhere
*/
static int Mod_Batches_Generate(model_t *mod)
{
	int i;
	msurface_t *surf;
	shader_t *shader;
	int sortid;
	batch_t *batch, *lbatch = NULL;
	vec4_t plane;

	int merge = mod->lightmaps.merge;
	if (!merge)
		merge = 1;

	mod->lightmaps.count = (mod->lightmaps.count+merge-1) & ~(merge-1);
	mod->lightmaps.count /= merge;
	mod->lightmaps.height *= merge;

	mod->numbatches = 0;

	//for each surface, find a suitable batch to insert it into.
	//we use 'firstmesh' to avoid chucking out too many verts in a single vbo (gl2 hardware tends to have a 16bit limit)
	for (i=0; i<mod->nummodelsurfaces; i++)
	{
		surf = mod->surfaces + mod->firstmodelsurface + i;
		shader = surf->texinfo->texture->shader;

		if (surf->flags & SURF_NODRAW)
		{
			shader = R_RegisterShader("nodraw", SUF_NONE, "{\nsurfaceparm nodraw\n}");
			sortid = shader->sort;
			VectorClear(plane);
			plane[3] = 0;
		}
		else if (shader)
		{
			sortid = shader->sort;

			//shaders that are portals need to be split into separate batches to have the same surface planes
			if (sortid == SHADER_SORT_PORTAL || (shader->flags & (SHADER_HASREFLECT | SHADER_HASREFRACT)))
			{
				if (surf->flags & SURF_PLANEBACK)
				{
					VectorNegate(surf->plane->normal, plane);
					plane[3] = -surf->plane->dist;
				}
				else
				{
					VectorCopy(surf->plane->normal, plane);
					plane[3] = surf->plane->dist;
				}
			}
			else
			{
				VectorClear(plane);
				plane[3] = 0;
			}
		}
		else
		{
			sortid = SHADER_SORT_OPAQUE;
			VectorClear(plane);
			plane[3] = 0;
		}

		if (lbatch && (
					lbatch->texture == surf->texinfo->texture &&
					lbatch->shader == shader &&
					lbatch->lightmap[0] == lmmerge(surf->lightmaptexturenums[0]) &&
					Vector4Compare(plane, lbatch->plane) &&
					lbatch->firstmesh + surf->mesh->numvertexes <= MAX_INDICIES) &&
#if MAXRLIGHTMAPS > 1
					lbatch->lightmap[1] == lmmerge(surf->lightmaptexturenums[1]) &&
					lbatch->lightmap[2] == lmmerge(surf->lightmaptexturenums[2]) &&
					lbatch->lightmap[3] == lmmerge(surf->lightmaptexturenums[3]) &&
#endif
					lbatch->fog == surf->fog)
			batch = lbatch;
		else
		{
			for (batch = mod->batches[sortid]; batch; batch = batch->next)
			{
				if (
							batch->texture == surf->texinfo->texture &&
							batch->shader == shader &&
							batch->lightmap[0] == lmmerge(surf->lightmaptexturenums[0]) &&
							Vector4Compare(plane, batch->plane) &&
							batch->firstmesh + surf->mesh->numvertexes <= MAX_INDICIES &&
#if MAXRLIGHTMAPS > 1
							batch->lightmap[1] == lmmerge(surf->lightmaptexturenums[1]) &&
							batch->lightmap[2] == lmmerge(surf->lightmaptexturenums[2]) &&
							batch->lightmap[3] == lmmerge(surf->lightmaptexturenums[3]) &&
#endif
							batch->fog == surf->fog)
					break;
			}
		}
		if (!batch)
		{
			batch = ZG_Malloc(&mod->memgroup, sizeof(*batch));
			batch->lightmap[0] = lmmerge(surf->lightmaptexturenums[0]);
#if MAXRLIGHTMAPS > 1
			batch->lightmap[1] = lmmerge(surf->lightmaptexturenums[1]);
			batch->lightmap[2] = lmmerge(surf->lightmaptexturenums[2]);
			batch->lightmap[3] = lmmerge(surf->lightmaptexturenums[3]);
#endif
			batch->texture = surf->texinfo->texture;
			batch->shader = shader;
			if (surf->texinfo->texture->alternate_anims || surf->texinfo->texture->anim_total)
			{
				switch (mod->fromgame)
				{
#ifdef Q2BSPS
				case fg_quake2:
					batch->buildmeshes = Mod_UpdateBatchShader_Q2;
					break;
#endif
#ifdef Q1BSPS
				case fg_quake:
				case fg_halflife:
					batch->buildmeshes = Mod_UpdateBatchShader_Q1;
					break;
#endif
				default:
					break;
				}
			}
			batch->next = mod->batches[sortid];
			batch->ent = &r_worldentity;
			batch->fog = surf->fog;
			Vector4Copy(plane, batch->plane);

			mod->batches[sortid] = batch;
		}

		surf->sbatch = batch;	//let the surface know which batch its in
		batch->maxmeshes++;
		batch->firstmesh += surf->mesh->numvertexes;

		lbatch = batch;
	}

	return merge;
#undef lmmerge
}

void Mod_LightmapAllocInit(lmalloc_t *lmallocator, qboolean hasdeluxe, unsigned int width, unsigned int height, int firstlm)
{
	memset(lmallocator, 0, sizeof(*lmallocator));
	lmallocator->deluxe = hasdeluxe;
	lmallocator->lmnum = firstlm;
	lmallocator->firstlm = firstlm;

	lmallocator->width = width;
	lmallocator->height = height;
}
void Mod_LightmapAllocDone(lmalloc_t *lmallocator, model_t *mod)
{
	mod->lightmaps.first = lmallocator->firstlm;
	mod->lightmaps.count = (lmallocator->lmnum - lmallocator->firstlm);
	if (lmallocator->allocated[0])	//lmnum was only *COMPLETE* lightmaps that we allocated, and does not include the one we're currently building.
		mod->lightmaps.count++;

	if (lmallocator->deluxe)
	{
		mod->lightmaps.first*=2;
		mod->lightmaps.count*=2;
		mod->lightmaps.deluxemapping = true;
	}
	else
		mod->lightmaps.deluxemapping = false;
}
void Mod_LightmapAllocBlock(lmalloc_t *lmallocator, int w, int h, unsigned short *x, unsigned short *y, int *tnum)
{
	int best, best2;
	int i, j;

	for(;;)
	{
		best = lmallocator->height;

		for (i = 0; i <= lmallocator->width - w; i++)
		{
			best2 = 0;

			for (j=0; j < w; j++)
			{
				if (lmallocator->allocated[i+j] >= best)
					break;
				if (lmallocator->allocated[i+j] > best2)
					best2 = lmallocator->allocated[i+j];
			}
			if (j == w)
			{	// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > lmallocator->height)
		{
			memset(lmallocator->allocated, 0, sizeof(lmallocator->allocated));
			lmallocator->lmnum++;
			continue;
		}

		for (i=0; i < w; i++)
			lmallocator->allocated[*x + i] = best + h;

		if (lmallocator->deluxe)
			*tnum = lmallocator->lmnum*2;
		else
			*tnum = lmallocator->lmnum;
		break;
	}
}

#ifdef Q3BSPS
static void Mod_Batches_SplitLightmaps(model_t *mod, int lmmerge)
{
	batch_t *batch;
	batch_t *nb;
	int i, j, sortid;
	msurface_t *surf;
	int sty;

	for (sortid = 0; sortid < SHADER_SORT_COUNT; sortid++)
	for (batch = mod->batches[sortid]; batch != NULL; batch = batch->next)
	{
		surf = (msurface_t*)batch->mesh[0];
		for (sty = 0; sty < MAXRLIGHTMAPS; sty++)
		{
			batch->lightmap[sty] = (surf->lightmaptexturenums[sty]>=0)?surf->lightmaptexturenums[sty]/lmmerge:surf->lightmaptexturenums[sty];
			batch->lmlightstyle[sty] = surf->styles[sty];
		}

		for (j = 1; j < batch->maxmeshes; j++)
		{
			surf = (msurface_t*)batch->mesh[j];
			for (sty = 0; sty < MAXRLIGHTMAPS; sty++)
			{
				int lm = (surf->lightmaptexturenums[sty]>=0)?surf->lightmaptexturenums[sty]/lmmerge:surf->lightmaptexturenums[sty];
				if (lm != batch->lightmap[sty] ||
					//fixme: we should merge later (reverted matching) surfaces into the prior batch
					surf->styles[sty] != batch->lmlightstyle[sty] ||
					surf->vlstyles[sty] != batch->vtlightstyle[sty])
					break;
			}
			if (sty < MAXRLIGHTMAPS)
			{
				nb = ZG_Malloc(&mod->memgroup, sizeof(*batch));
				*nb = *batch;
				batch->next = nb;

				nb->mesh = batch->mesh + j*2;
				nb->maxmeshes = batch->maxmeshes - j;
				batch->maxmeshes = j;
				for (sty = 0; sty < MAXRLIGHTMAPS; sty++)
				{
					int lm = (surf->lightmaptexturenums[sty]>=0)?surf->lightmaptexturenums[sty]/lmmerge:surf->lightmaptexturenums[sty];
					nb->lightmap[sty] = lm;
					nb->lmlightstyle[sty] = surf->styles[sty];
					nb->vtlightstyle[sty] = surf->vlstyles[sty];
				}

				memmove(nb->mesh, batch->mesh+j, sizeof(msurface_t*)*nb->maxmeshes);

				for (i = 0; i < nb->maxmeshes; i++)
				{
					surf = (msurface_t*)nb->mesh[i];
					surf->sbatch = nb;
				}

				batch = nb;
				j = 1;
			}
		}
	}
}
#endif

#if defined(Q1BSPS) || defined(Q2BSPS)
static void Mod_LightmapAllocSurf(lmalloc_t *lmallocator, msurface_t *surf, int surfstyle)
{
	int smax, tmax;
	smax = (surf->extents[0]>>surf->lmshift)+1;
	tmax = (surf->extents[1]>>surf->lmshift)+1;

	if (isDedicated ||
		(surf->texinfo->texture->shader && !(surf->texinfo->texture->shader->flags & SHADER_HASLIGHTMAP)) || //fte
		(surf->flags & (SURF_DRAWSKY|SURF_DRAWTURB)) ||	//q1
		(surf->texinfo->flags & TEX_SPECIAL) ||	//the original 'no lightmap'
		(surf->texinfo->flags & (TI_SKY|TI_TRANS33|TI_TRANS66|TI_WARP)) ||	//q2 surfaces
		smax > lmallocator->width || tmax > lmallocator->height || smax < 0 || tmax < 0)	//bugs/bounds/etc
	{
		surf->lightmaptexturenums[surfstyle] = -1;
		return;
	}

	Mod_LightmapAllocBlock (lmallocator, smax, tmax, &surf->light_s[surfstyle], &surf->light_t[surfstyle], &surf->lightmaptexturenums[surfstyle]);
}

/*
allocates lightmaps and splits batches upon lightmap boundaries
*/
static void Mod_Batches_AllocLightmaps(model_t *mod)
{
	batch_t *batch;
	batch_t *nb;
	lmalloc_t lmallocator;
	int i, j, sortid;
	msurface_t *surf;
	int sty;

	size_t samps = 0;

	//small models don't have many surfaces, don't allocate a smegging huge lightmap that simply won't be used.
	for (i=0, j=0; i<mod->nummodelsurfaces; i++)
	{
		surf = mod->surfaces + mod->firstmodelsurface + i;
		if (surf->texinfo->flags & TEX_SPECIAL)
			continue;	//surfaces with no lightmap should not count torwards anything.
		samps += ((surf->extents[0]>>surf->lmshift)+1) * ((surf->extents[1]>>surf->lmshift)+1);

		if (j < (surf->extents[0]>>surf->lmshift)+1)
			j = (surf->extents[0]>>surf->lmshift)+1;
		if (j < (surf->extents[1]>>surf->lmshift)+1)
			j = (surf->extents[1]>>surf->lmshift)+1;
	}
	samps /= 4;
	samps = sqrt(samps);
	if (j > 128 || r_dynamic.ival <= 0)
		samps *= 2;
	mod->lightmaps.width = bound(j, samps, LMBLOCK_SIZE_MAX);
	mod->lightmaps.height = bound(j, samps, LMBLOCK_SIZE_MAX);
	for (i = 0; (1<<i) < mod->lightmaps.width; i++);
	mod->lightmaps.width = 1<<i;
	for (i = 0; (1<<i) < mod->lightmaps.height; i++);
	mod->lightmaps.height = 1<<i;
	mod->lightmaps.width = bound(64, mod->lightmaps.width, sh_config.texture_maxsize);
	mod->lightmaps.height = bound(64, mod->lightmaps.height, sh_config.texture_maxsize);

	Mod_LightmapAllocInit(&lmallocator, mod->deluxdata != NULL, mod->lightmaps.width, mod->lightmaps.height, 0x50);

	for (sortid = 0; sortid < SHADER_SORT_COUNT; sortid++)
	for (batch = mod->batches[sortid]; batch != NULL; batch = batch->next)
	{
		surf = (msurface_t*)batch->mesh[0];
		Mod_LightmapAllocSurf (&lmallocator, surf, 0);
		for (sty = 1; sty < MAXRLIGHTMAPS; sty++)
			surf->lightmaptexturenums[sty] = -1;
		for (sty = 0; sty < MAXRLIGHTMAPS; sty++)
		{
			batch->lightmap[sty] = surf->lightmaptexturenums[sty];
			batch->lmlightstyle[sty] = 255;//don't do special backend rendering of lightstyles.
			batch->vtlightstyle[sty] = 255;//don't do special backend rendering of lightstyles.
		}

		for (j = 1; j < batch->maxmeshes; j++)
		{
			surf = (msurface_t*)batch->mesh[j];
			Mod_LightmapAllocSurf (&lmallocator, surf, 0);
			for (sty = 1; sty < MAXRLIGHTMAPS; sty++)
				surf->lightmaptexturenums[sty] = -1;
			if (surf->lightmaptexturenums[0] != batch->lightmap[0])
			{
				nb = ZG_Malloc(&mod->memgroup, sizeof(*batch));
				*nb = *batch;
				batch->next = nb;

				nb->mesh = batch->mesh + j*2;
				nb->maxmeshes = batch->maxmeshes - j;
				batch->maxmeshes = j;
				for (sty = 0; sty < MAXRLIGHTMAPS; sty++)
					nb->lightmap[sty] = surf->lightmaptexturenums[sty];

				memmove(nb->mesh, batch->mesh+j, sizeof(msurface_t*)*nb->maxmeshes);

				for (i = 0; i < nb->maxmeshes; i++)
				{
					surf = (msurface_t*)nb->mesh[i];
					surf->sbatch = nb;
				}

				batch = nb;
				j = 0;
			}
		}
	}

	Mod_LightmapAllocDone(&lmallocator, mod);
}
#endif

extern void Surf_CreateSurfaceLightmap (msurface_t *surf, int shift);
//if build is NULL, uses q1/q2 surf generation, and allocates lightmaps
static void Mod_Batches_Build(model_t *mod, builddata_t *bd)
{
	int i;
	int numverts = 0, numindicies=0;
	msurface_t *surf;
	mesh_t *mesh;
	mesh_t **bmeshes;
	int sortid;
	batch_t *batch;
	mesh_t *meshlist;
	int merge = 1;

	currentmodel = mod;

	if (!mod->textures)
		return;

	if (mod->firstmodelsurface + mod->nummodelsurfaces > mod->numsurfaces)
		Sys_Error("submodel %s surface range is out of bounds\n", mod->name);

	if (bd)
		meshlist = NULL;
	else
		meshlist = ZG_Malloc(&mod->memgroup, sizeof(mesh_t) * mod->nummodelsurfaces);

	for (i=0; i<mod->nummodelsurfaces; i++)
	{
		surf = mod->surfaces + i + mod->firstmodelsurface;
		if (meshlist)
		{
			mesh = surf->mesh = &meshlist[i];
			mesh->numvertexes = surf->numedges;
			mesh->numindexes = (surf->numedges-2)*3;
		}
		else
			mesh = surf->mesh;

		numverts += mesh->numvertexes;
		numindicies += mesh->numindexes;
//		surf->lightmaptexturenum = -1;
	}

	/*assign each mesh to a batch, generating as needed*/
	merge = Mod_Batches_Generate(mod);

	bmeshes = ZG_Malloc(&mod->memgroup, sizeof(*bmeshes)*mod->nummodelsurfaces*R_MAX_RECURSE);

	//we now know which batch each surface is in, and how many meshes there are in each batch.
	//allocate the mesh-pointer-lists for each batch. *2 for recursion.
	for (i = 0, sortid = 0; sortid < SHADER_SORT_COUNT; sortid++)
	for (batch = mod->batches[sortid]; batch != NULL; batch = batch->next)
	{
		batch->mesh = bmeshes + i;
		i += batch->maxmeshes*R_MAX_RECURSE;
	}
	//store the *surface* into the batch's mesh list (yes, this is an evil cast hack, but at least both are pointers)
	for (i=0; i<mod->nummodelsurfaces; i++)
	{
		surf = mod->surfaces + mod->firstmodelsurface + i;
		surf->sbatch->mesh[surf->sbatch->meshes++] = (mesh_t*)surf;
	}

#if defined(Q1BSPS) || defined(Q2BSPS)
	if (!bd)
	{
		Mod_Batches_AllocLightmaps(mod);

		mod->lightmaps.surfstyles = 1;
		Mod_Batches_BuildModelMeshes(mod, numverts, numindicies, ModQ1_Batches_BuildQ1Q2Poly, bd, merge);
	}
#endif
#if defined(Q3BSPS)
	if (bd)
	{
		Mod_Batches_SplitLightmaps(mod, merge);
		Mod_Batches_BuildModelMeshes(mod, numverts, numindicies, bd->buildfunc, bd, merge);
	}
#endif

	if (BE_GenBrushModelVBO)
		BE_GenBrushModelVBO(mod);
}
#endif


/*
=================
Mod_SetParent
=================
*/
void Mod_SetParent (mnode_t *node, mnode_t *parent)
{
	if (!node)
		return;
	node->parent = parent;
	if (node->contents < 0)
		return;
	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}

#if defined(Q1BSPS) || defined(Q2BSPS)
/*
=================
Mod_LoadEdges
=================
*/
qboolean Mod_LoadEdges (model_t *loadmodel, qbyte *mod_base, lump_t *l, qboolean lm)
{
	medge_t *out;
	int 	i, count;
	
	if (lm)
	{
		dledge_t *in = (void *)(mod_base + l->fileofs);
		if (l->filelen % sizeof(*in))
		{
			Con_Printf ("MOD_LoadBmodel: funny lump size in %s\n", loadmodel->name);
			return false;
		}
		count = l->filelen / sizeof(*in);
		out = ZG_Malloc(&loadmodel->memgroup, (count + 1) * sizeof(*out));	

		loadmodel->edges = out;
		loadmodel->numedges = count;

		for ( i=0 ; i<count ; i++, in++, out++)
		{
			out->v[0] = LittleLong(in->v[0]);
			out->v[1] = LittleLong(in->v[1]);
		}
	}
	else
	{
		dsedge_t *in = (void *)(mod_base + l->fileofs);
		if (l->filelen % sizeof(*in))
		{
			Con_Printf ("MOD_LoadBmodel: funny lump size in %s\n", loadmodel->name);
			return false;
		}
		count = l->filelen / sizeof(*in);
		out = ZG_Malloc(&loadmodel->memgroup, (count + 1) * sizeof(*out));

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
Mod_LoadMarksurfaces
=================
*/
qboolean Mod_LoadMarksurfaces (model_t *loadmodel, qbyte *mod_base, lump_t *l, qboolean lm)
{	
	int		i, j, count;
	msurface_t **out;

	if (lm)
	{
		int		*inl;
		inl = (void *)(mod_base + l->fileofs);
		if (l->filelen % sizeof(*inl))
		{
			Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
			return false;
		}
		count = l->filelen / sizeof(*inl);
		out = ZG_Malloc(&loadmodel->memgroup, count*sizeof(*out));

		loadmodel->marksurfaces = out;
		loadmodel->nummarksurfaces = count;

		for ( i=0 ; i<count ; i++)
		{
			j = (unsigned int)LittleLong(inl[i]);
			if (j >= loadmodel->numsurfaces)
			{
				Con_Printf (CON_ERROR "Mod_ParseMarksurfaces: bad surface number\n");
				return false;
			}
			out[i] = loadmodel->surfaces + j;
		}
	}
	else
	{
		short		*ins;
		ins = (void *)(mod_base + l->fileofs);
		if (l->filelen % sizeof(*ins))
		{
			Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
			return false;
		}
		count = l->filelen / sizeof(*ins);
		out = ZG_Malloc(&loadmodel->memgroup, count*sizeof(*out));

		loadmodel->marksurfaces = out;
		loadmodel->nummarksurfaces = count;

		for ( i=0 ; i<count ; i++)
		{
			j = (unsigned short)LittleShort(ins[i]);
			if (j >= loadmodel->numsurfaces)
			{
				Con_Printf (CON_ERROR "Mod_ParseMarksurfaces: bad surface number\n");
				return false;
			}
			out[i] = loadmodel->surfaces + j;
		}
	}

	return true;
}

/*
=================
Mod_LoadSurfedges
=================
*/
qboolean Mod_LoadSurfedges (model_t *loadmodel, qbyte *mod_base, lump_t *l)
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
	out = ZG_Malloc(&loadmodel->memgroup, count*sizeof(*out));

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for ( i=0 ; i<count ; i++)
		out[i] = LittleLong (in[i]);

	return true;
}
#endif
#ifdef Q1BSPS
/*
=================
Mod_LoadVisibility
=================
*/
static void Mod_LoadVisibility (model_t *loadmodel, qbyte *mod_base, lump_t *l, qbyte *ptr, size_t len)
{
	if (!ptr)
	{
		ptr = mod_base + l->fileofs;
		len = l->filelen;
	}
	if (!len)
	{
		loadmodel->visdata = NULL;
		return;
	}
	loadmodel->visdata = ZG_Malloc(&loadmodel->memgroup, len);	
	memcpy (loadmodel->visdata, ptr, len);
}

#ifndef SERVERONLY
static void Mod_LoadMiptex(model_t *loadmodel, texture_t *tx, miptex_t *mt)
{
	unsigned int size = 
		(mt->width>>0)*(mt->height>>0) +
		(mt->width>>1)*(mt->height>>1) +
		(mt->width>>2)*(mt->height>>2) +
		(mt->width>>3)*(mt->height>>3);

	if (loadmodel->fromgame == fg_halflife && *(short*)((qbyte *)mt + mt->offsets[3] + (mt->width>>3)*(mt->height>>3)) == 256)
	{	//mostly identical, just a specific palette hidden at the end. handle fences elsewhere.
		tx->mips[0] = BZ_Malloc(size + 768);
		tx->palette = tx->mips[0] + size;
		memcpy(tx->palette, (qbyte *)mt + mt->offsets[3] + (mt->width>>3)*(mt->height>>3) + 2, 768);
	}
	else
	{
		tx->mips[0] = BZ_Malloc(size);
		tx->palette = NULL;
	}

	tx->mips[1] = tx->mips[0] + (mt->width>>0)*(mt->height>>0);
	tx->mips[2] = tx->mips[1] + (mt->width>>1)*(mt->height>>1);
	tx->mips[3] = tx->mips[2] + (mt->width>>2)*(mt->height>>2);
	memcpy(tx->mips[0], (qbyte *)mt + mt->offsets[0], (mt->width>>0)*(mt->height>>0));
	memcpy(tx->mips[1], (qbyte *)mt + mt->offsets[1], (mt->width>>1)*(mt->height>>1));
	memcpy(tx->mips[2], (qbyte *)mt + mt->offsets[2], (mt->width>>2)*(mt->height>>2));
	memcpy(tx->mips[3], (qbyte *)mt + mt->offsets[3], (mt->width>>3)*(mt->height>>3));

}
#endif

/*
=================
Mod_LoadTextures
=================
*/
static qboolean Mod_LoadTextures (model_t *loadmodel, qbyte *mod_base, lump_t *l)
{
	int		i, j, num, max, altmax;
	miptex_t	*mt;
	texture_t	*tx, *tx2;
	texture_t	*anims[10];
	texture_t	*altanims[10];
	dmiptexlump_t *m;

TRACE(("dbg: Mod_LoadTextures: inittexturedescs\n"));

//	Mod_InitTextureDescs(loadname);

	if (!l->filelen)
	{
		Con_Printf(CON_WARNING "warning: %s contains no texture data\n", loadmodel->name);

		loadmodel->numtextures = 1;
		loadmodel->textures = ZG_Malloc(&loadmodel->memgroup, 1 * sizeof(*loadmodel->textures));

		i = 0;
		tx = ZG_Malloc(&loadmodel->memgroup, sizeof(texture_t));
		memcpy(tx, r_notexture_mip, sizeof(texture_t));
		sprintf(tx->name, "unnamed%i", i);
		loadmodel->textures[i] = tx;

		return true;
	}
	m = (dmiptexlump_t *)(mod_base + l->fileofs);

	m->nummiptex = LittleLong (m->nummiptex);

	loadmodel->numtextures = m->nummiptex;
	loadmodel->textures = ZG_Malloc(&loadmodel->memgroup, m->nummiptex * sizeof(*loadmodel->textures));

	for (i=0 ; i<m->nummiptex ; i++)
	{
		m->dataofs[i] = LittleLong(m->dataofs[i]);
		if (m->dataofs[i] == -1)	//e1m2, this happens
		{
			tx = ZG_Malloc(&loadmodel->memgroup, sizeof(texture_t));
			memcpy(tx, r_notexture_mip, sizeof(texture_t));
			sprintf(tx->name, "unnamed%i", i);
			loadmodel->textures[i] = tx;
			continue;
		}
		mt = (miptex_t *)((qbyte *)m + m->dataofs[i]);

	TRACE(("dbg: Mod_LoadTextures: texture %s\n", loadname));

		if (!*mt->name)	//I HATE MAPPERS!
		{
			sprintf(mt->name, "unnamed%i", i);
			Con_DPrintf(CON_WARNING "warning: unnamed texture in %s, renaming to %s\n", loadmodel->name, mt->name);
		}

		mt->width = LittleLong (mt->width);
		mt->height = LittleLong (mt->height);
		for (j=0 ; j<MIPLEVELS ; j++)
			mt->offsets[j] = LittleLong (mt->offsets[j]);

		if ( (mt->width & 15) || (mt->height & 15) )
			Con_Printf (CON_WARNING "Warning: Texture %s is not 16 aligned", mt->name);
		if (mt->width < 1 || mt->height < 1)
			Con_Printf (CON_WARNING "Warning: Texture %s has no size", mt->name);
		tx = ZG_Malloc(&loadmodel->memgroup, sizeof(texture_t));
		loadmodel->textures[i] = tx;

		memcpy (tx->name, mt->name, sizeof(tx->name));
		tx->width = mt->width;
		tx->height = mt->height;

		if (!mt->offsets[0])	//this is a hl external style texture, load it a little later (from a wad)
		{
			continue;
		}

#ifndef SERVERONLY
		Mod_LoadMiptex(loadmodel, tx, mt);
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

/*
=================
Mod_LoadSubmodels
=================
*/
static qboolean Mod_LoadSubmodels (model_t *loadmodel, qbyte *mod_base, lump_t *l, qboolean *hexen2map)
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
		*hexen2map = true;
		if (l->filelen % sizeof(*inh))
		{
			Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
			return false;
		}
		count = l->filelen / sizeof(*inh);
		out = ZG_Malloc(&loadmodel->memgroup, count*sizeof(*out));

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
		*hexen2map = false;
		if (l->filelen % sizeof(*inq))
		{
			Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
			return false;
		}
		count = l->filelen / sizeof(*inq);
		out = ZG_Malloc(&loadmodel->memgroup, count*sizeof(*out));	

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
			for (j=0 ; j<4 ; j++)
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
Mod_LoadTexinfo
=================
*/
static qboolean Mod_LoadTexinfo (model_t *loadmodel, qbyte *mod_base, lump_t *l)
{
	texinfo_t *in;
	mtexinfo_t *out;
	int 	i, j, count;
	int		miptex;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
		return false;
	}
	count = l->filelen / sizeof(*in);
	out = ZG_Malloc(&loadmodel->memgroup, count*sizeof(*out));

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<4 ; j++)
		{
			out->vecs[0][j] = LittleFloat (in->vecs[0][j]);
			out->vecs[1][j] = LittleFloat (in->vecs[1][j]);
		}
		out->vecscale[0] = 1.0/Length (out->vecs[0]);
		out->vecscale[1] = 1.0/Length (out->vecs[1]);

		miptex = LittleLong (in->miptex);
		out->flags = LittleLong (in->flags);
	
		if (loadmodel->numtextures)
			out->texture = loadmodel->textures[miptex % loadmodel->numtextures];
		else
			out->texture = NULL;
		if (!out->texture)
		{
			out->texture = r_notexture_mip; // texture not found
			out->flags = 0;
		}
		else if (!strncmp(out->texture->name, "scroll", 6) || ((*out->texture->name == '*' || *out->texture->name == '{' || *out->texture->name == '!') && !strncmp(out->texture->name+1, "scroll", 6)))
			out->flags |= TI_FLOWING;
	}

	return true;
}

/*
================
CalcSurfaceExtents

Fills in s->texturemins[] and s->extents[]
================
*/

void CalcSurfaceExtents (model_t *mod, msurface_t *s);
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
static qboolean Mod_LoadFaces (model_t *loadmodel, qbyte *mod_base, lump_t *l, lump_t *lightlump, qboolean lm)
{
	dsface_t		*ins;
	dlface_t		*inl;
	msurface_t 	*out;
	int			count, surfnum;
	int			i, planenum, side;
	int tn;
	unsigned int lofs, lend;

	unsigned short lmshift, lmscale;
	char buf[64];
	lightmapoverrides_t overrides;

	memset(&overrides, 0, sizeof(overrides));

	lmscale = atoi(Mod_ParseWorldspawnKey(loadmodel, "lightmap_scale", buf, sizeof(buf)));
	if (!lmscale)
		lmshift = LMSHIFT_DEFAULT;
	else
	{
		for(lmshift = 0; lmscale > 1; lmshift++)
			lmscale >>= 1;
	}

	if (lm)
	{
		ins = NULL;
		inl = (void *)(mod_base + l->fileofs);
		if (l->filelen % sizeof(*inl))
		{
			Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
			return false;
		}
		count = l->filelen / sizeof(*inl);
	}
	else
	{
		ins = (void *)(mod_base + l->fileofs);
		inl = NULL;
		if (l->filelen % sizeof(*ins))
		{
			Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
			return false;
		}
		count = l->filelen / sizeof(*ins);
	}
	out = ZG_Malloc(&loadmodel->memgroup, count*sizeof(*out));

//	*meshlist = ZG_Malloc(&loadmodel->memgroup, count*sizeof(**meshlist));
	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	Mod_LoadLighting (loadmodel, mod_base, lightlump, false, &overrides);

	for ( surfnum=0 ; surfnum<count ; surfnum++, out++)
	{
		if (lm)
		{
			planenum = LittleLong(inl->planenum);
			side = LittleLong(inl->side);
			out->firstedge = LittleLong(inl->firstedge);
			out->numedges = LittleLong(inl->numedges);
			tn = LittleLong (inl->texinfo);
			for (i=0 ; i<MAXQ1LIGHTMAPS ; i++)
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
			for (i=0 ; i<MAXQ1LIGHTMAPS ; i++)
				out->styles[i] = ins->styles[i];
			lofs = LittleLong(ins->lightofs);
			ins++;
		}
//		(*meshlist)[surfnum].vbofirstvert = out->firstedge;
//		(*meshlist)[surfnum].numvertexes = out->numedges;
		out->flags = 0;

		if (side)
			out->flags |= SURF_PLANEBACK;			

		out->plane = loadmodel->planes + planenum;

		if (tn < 0 || tn >= loadmodel->numtexinfo)
		{
			Con_Printf("texinfo 0 <= %i < %i\n", tn, loadmodel->numtexinfo);
			return false;
		}
		out->texinfo = loadmodel->texinfo + tn;

		if (overrides.shifts)
			out->lmshift = overrides.shifts[surfnum];
		else
			out->lmshift = lmshift;
		if (overrides.offsets)
			lofs = overrides.offsets[surfnum];
		if (overrides.styles)
			for (i=0 ; i<MAXRLIGHTMAPS ; i++)
				out->styles[i] = overrides.styles[surfnum*4+i];

		CalcSurfaceExtents (loadmodel, out);
		if (lofs != (unsigned int)-1 && (loadmodel->engineflags & MDLF_RGBLIGHTING) && loadmodel->fromgame != fg_halflife)
			lofs *= 3;
		lend = lofs+(out->extents[0]+1)*(out->extents[1]+1);
		if (lofs > loadmodel->lightdatasize || lend < lofs)
			out->samples = NULL;	//should includes -1
		else
			out->samples = loadmodel->lightdata + lofs;

		if (!out->texinfo->texture)
			continue;

		
	// set the drawing flags flag		
		if (!Q_strncmp(out->texinfo->texture->name,"sky",3))	// sky
		{
			out->flags |= (SURF_DRAWSKY | SURF_DRAWTILED);
			continue;
		}
		
		if (*out->texinfo->texture->name == '*' || (*out->texinfo->texture->name == '!' && loadmodel->fromgame == fg_halflife))		// turbulent
		{
			out->flags |= (SURF_DRAWTURB | SURF_DRAWTILED);
			for (i=0 ; i<2 ; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			continue;
		}

		/*if (*out->texinfo->texture->name == '~')
		{
			out->texinfo->flags |= SURF_BLENDED;
			continue;
		}*/
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
Mod_LoadNodes
=================
*/
static qboolean Mod_LoadNodes (model_t *loadmodel, qbyte *mod_base, lump_t *l, int lm)
{
	int			i, j, count, p;
	mnode_t 	*out;

	if (lm == 2)
	{
		dl2node_t		*in;
		in = (void *)(mod_base + l->fileofs);
		if (l->filelen % sizeof(*in))
		{
			Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
			return false;
		}
		count = l->filelen / sizeof(*in);
		out = ZG_Malloc(&loadmodel->memgroup, count*sizeof(*out));

		loadmodel->nodes = out;
		loadmodel->numnodes = count;

		for ( i=0 ; i<count ; i++, in++, out++)
		{
			for (j=0 ; j<3 ; j++)
			{
				out->minmaxs[j] = LittleFloat (in->mins[j]);
				out->minmaxs[3+j] = LittleFloat (in->maxs[j]);
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
	else if (lm)
	{
		dl1node_t		*in;
		in = (void *)(mod_base + l->fileofs);
		if (l->filelen % sizeof(*in))
		{
			Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
			return false;
		}
		count = l->filelen / sizeof(*in);
		out = ZG_Malloc(&loadmodel->memgroup, count*sizeof(*out));

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
		out = ZG_Malloc(&loadmodel->memgroup, count*sizeof(*out));

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

			out->firstsurface = (unsigned short)LittleShort (in->firstface);
			out->numsurfaces = (unsigned short)LittleShort (in->numfaces);

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
static qboolean Mod_LoadLeafs (model_t *loadmodel, qbyte *mod_base, lump_t *l, int lm, qboolean isnotmap, qbyte *ptr, size_t len)
{
	mleaf_t 	*out;
	int			i, j, count, p;

	if (!ptr)
	{
		ptr = mod_base + l->fileofs;
		len = l->filelen;
	}

	if (lm==2)
	{
		dl2leaf_t 	*in;
		in = (void *)ptr;
		if (len % sizeof(*in))
		{
			Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
			return false;
		}
		count = len / sizeof(*in);
		if (count > SANITY_MAX_MAP_LEAFS)
		{
			Con_Printf (CON_ERROR "Mod_LoadLeafs: %s has more than %i leafs\n",loadmodel->name, SANITY_MAX_MAP_LEAFS);
			return false;
		}
		out = ZG_Malloc(&loadmodel->memgroup, count*sizeof(*out));

		loadmodel->leafs = out;
		loadmodel->numleafs = count;
		loadmodel->numclusters = count-1;
		loadmodel->pvsbytes = ((loadmodel->numclusters+31)>>3)&~3;

		for ( i=0 ; i<count ; i++, in++, out++)
		{
			for (j=0 ; j<3 ; j++)
			{
				out->minmaxs[j] = LittleFloat (in->mins[j]);
				out->minmaxs[3+j] = LittleFloat (in->maxs[j]);
			}

			p = LittleLong(in->contents);
			out->contents = p;

			out->firstmarksurface = loadmodel->marksurfaces +
				LittleLong(in->firstmarksurface);
			out->nummarksurfaces = LittleLong(in->nummarksurfaces);
			
			p = LittleLong(in->visofs);
			if (p == -1)
				out->compressed_vis = NULL;
			else
				out->compressed_vis = loadmodel->visdata + p;
			
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
	}
	else if (lm)
	{
		dl1leaf_t 	*in;
		in = (void *)(ptr);
		if (len % sizeof(*in))
		{
			Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
			return false;
		}
		count = len / sizeof(*in);
		if (count > SANITY_MAX_MAP_LEAFS)
		{
			Con_Printf (CON_ERROR "Mod_LoadLeafs: %s has more than %i leafs\n",loadmodel->name, SANITY_MAX_MAP_LEAFS);
			return false;
		}
		out = ZG_Malloc(&loadmodel->memgroup, count*sizeof(*out));

		loadmodel->leafs = out;
		loadmodel->numleafs = count;
		loadmodel->numclusters = count-1;
		loadmodel->pvsbytes = ((loadmodel->numclusters+31)>>3)&~3;

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
				LittleLong(in->firstmarksurface);
			out->nummarksurfaces = LittleLong(in->nummarksurfaces);
			
			p = LittleLong(in->visofs);
			if (p == -1)
				out->compressed_vis = NULL;
			else
				out->compressed_vis = loadmodel->visdata + p;
			
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
	}
	else
	{
		dsleaf_t 	*in;
		in = (void *)(ptr);
		if (len % sizeof(*in))
		{
			Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
			return false;
		}
		count = len / sizeof(*in);
		if (count > SANITY_MAX_MAP_LEAFS)
		{
			Con_Printf (CON_ERROR "Mod_LoadLeafs: %s has more than %i leafs\n",loadmodel->name, SANITY_MAX_MAP_LEAFS);
			return false;
		}
		out = ZG_Malloc(&loadmodel->memgroup, count*sizeof(*out));

		loadmodel->leafs = out;
		loadmodel->numleafs = count;
		loadmodel->numclusters = count-1;
		loadmodel->pvsbytes = ((loadmodel->numclusters+31)>>3)&~3;

		for ( i=0 ; i<count ; i++, in++, out++)
		{
			for (j=0 ; j<3 ; j++)
			{
				out->minmaxs[j] = LittleShort (in->mins[j]);
				out->minmaxs[3+j] = LittleShort (in->maxs[j]);
			}

			p = LittleLong(in->contents);
			out->contents = p;

			out->firstmarksurface = loadmodel->marksurfaces + (unsigned short)LittleShort(in->firstmarksurface);
			out->nummarksurfaces = (unsigned short)LittleShort(in->nummarksurfaces);
			
			p = LittleLong(in->visofs);
			if (p == -1)
				out->compressed_vis = NULL;
			else
				out->compressed_vis = loadmodel->visdata + p;
			
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
	}

	return true;
}




//these are used to boost other info sizes
static int numsuplementryplanes;
static int numsuplementryclipnodes;
static void *suplementryclipnodes;
static void *suplementryplanes;
static void *crouchhullfile;

static void Mod_LoadCrouchHull(model_t *loadmodel)
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

	FS_LoadFile(crouchhullname, &crouchhullfile);
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
static qboolean Mod_LoadClipnodes (model_t *loadmodel, qbyte *mod_base, lump_t *l, qboolean lm, qboolean hexen2map)
{
	dsclipnode_t *ins;
	dlclipnode_t *inl;
	mclipnode_t *out;
	int			i, count;
	hull_t		*hull;

	if (lm)
	{
		ins = NULL;
		inl = (void *)(mod_base + l->fileofs);
		if (l->filelen % sizeof(*inl))
		{
			Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
			return false;
		}
		count = l->filelen / sizeof(*inl);
	}
	else
	{
		ins = (void *)(mod_base + l->fileofs);
		inl = NULL;
		if (l->filelen % sizeof(*ins))
		{
			Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
			return false;
		}
		count = l->filelen / sizeof(*ins);
	}
	out = ZG_Malloc(&loadmodel->memgroup, (count+numsuplementryclipnodes)*sizeof(*out));//space for both

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
		hull->clip_mins[2] = -36;//-36 is correct here, but mvdsv uses -32 instead. This breaks prediction between the two
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

	if (lm)
	{
		for (i=0 ; i<count ; i++, out++, inl++)
		{
			out->planenum = LittleLong(inl->planenum);
			out->children[0] = LittleLong(inl->children[0]);
			out->children[1] = LittleLong(inl->children[1]);
		}
	}
	else
	{
		for (i=0 ; i<count ; i++, out++, ins++)
		{
			out->planenum = LittleLong(ins->planenum);
			out->children[0] = (unsigned short)LittleShort(ins->children[0]);
			out->children[1] = (unsigned short)LittleShort(ins->children[1]);

			//if these 'overflow', then they're meant to refer to contents instead, and should be negative
			if (out->children[0] >= count)
				out->children[0] -= 0x10000;
			if (out->children[1] >= count)
				out->children[1] -= 0x10000;
		}
	}

	if (numsuplementryclipnodes)	//now load the crouch ones.
	{
/*This looks buggy*/
		for (i = 3; i < MAX_MAP_HULLSM; i++)
		{
			hull = &loadmodel->hulls[i];
			hull->planes = suplementryplanes;
			hull->clipnodes = out-1;
			hull->firstclipnode = 0;
			hull->lastclipnode = numsuplementryclipnodes;
			hull->available = true;
		}

		ins = suplementryclipnodes;

		for (i=0 ; i<numsuplementryclipnodes ; i++, out++, ins++)
		{
			out->planenum = LittleLong(ins->planenum);
			out->children[0] = LittleShort(ins->children[0]);
			out->children[0] += out->children[0]>=0?1:0;
			out->children[1] = LittleShort(ins->children[1]);
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
static void Mod_MakeHull0 (model_t *loadmodel)
{
	mnode_t		*in, *child;
	mclipnode_t *out;
	int			i, j, count;
	hull_t		*hull;

	hull = &loadmodel->hulls[0];	

	in = loadmodel->nodes;
	count = loadmodel->numnodes;
	out = ZG_Malloc(&loadmodel->memgroup, count*sizeof(*out));

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
Mod_LoadPlanes
=================
*/
static qboolean Mod_LoadPlanes (model_t *loadmodel, qbyte *mod_base, lump_t *l)
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
	out = ZG_Malloc(&loadmodel->memgroup, (count+numsuplementryplanes)*2*sizeof(*out));
	
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

#ifndef SERVERONLY
//combination of R_AddDynamicLights and R_MarkLights
static void Q1BSP_StainNode (mnode_t *node, float *parms)
{
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
		if (surf->flags&~(SURF_DRAWALPHA|SURF_DONTWARP|SURF_PLANEBACK))
			continue;
		Surf_StainSurf(surf, parms);
	}

	Q1BSP_StainNode (node->children[0], parms);
	Q1BSP_StainNode (node->children[1], parms);
}
#endif

static void Mod_FixupNodeMinsMaxs (mnode_t *node, mnode_t *parent)
{
	if (!node)
		return;

	if (node->contents >= 0)
	{
		Mod_FixupNodeMinsMaxs (node->children[0], node);
		Mod_FixupNodeMinsMaxs (node->children[1], node);
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

static void Mod_FixupMinsMaxs(model_t *loadmodel)
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
					lindex = loadmodel->surfedges[surf->firstedge + en];

					if (lindex > 0)
					{
						e = &pedges[lindex];
						v = loadmodel->vertexes[e->v[0]].position;
					}
					else
					{
						e = &pedges[-lindex];
						v = loadmodel->vertexes[e->v[1]].position;
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
	Mod_FixupNodeMinsMaxs (loadmodel->nodes, NULL);	// sets nodes and leafs
}

#endif

void ModBrush_LoadGLStuff(void *ctx, void *data, size_t a, size_t b)
{
#ifndef SERVERONLY
	model_t *mod = ctx;
	char loadname[MAX_QPATH];

	if (!a)
	{	//submodels share textures, so only do this if 'a' is 0 (inline index, 0 = world).
		for (a = 0; a < mod->numfogs; a++)
		{
			mod->fogs[a].shader = R_RegisterShader_Lightmap(mod->fogs[a].shadername);
			R_BuildDefaultTexnums(NULL, mod->fogs[a].shader);
			if (!mod->fogs[a].shader->fog_dist)
			{
				//invalid fog shader, don't use.
				mod->fogs[a].shader = NULL;
				mod->fogs[a].numplanes = 0;
			}
		}

		if (mod->fromgame == fg_quake3)
		{
			for(a = 0; a < mod->numtexinfo; a++)
			{
				mod->textures[a]->shader = R_RegisterShader_Lightmap(mod->textures[a]->name);
				R_BuildDefaultTexnums(NULL, mod->textures[a]->shader);

				mod->textures[a+mod->numtexinfo]->shader = R_RegisterShader_Vertex (mod->textures[a+mod->numtexinfo]->name);
				R_BuildDefaultTexnums(NULL, mod->textures[a+mod->numtexinfo]->shader);
			}
			mod->textures[2*mod->numtexinfo]->shader = R_RegisterShader_Flare("noshader");
		}
		else if (mod->fromgame == fg_quake2)
		{
			COM_FileBase (mod->name, loadname, sizeof(loadname));
			for(a = 0; a < mod->numtextures; a++)
			{
				unsigned int maps = 0;
				mod->textures[a]->shader = R_RegisterCustom (mod->textures[a]->name, SUF_LIGHTMAP, Shader_DefaultBSPQ2, NULL);

				maps |= SHADER_HASPALETTED;
				maps |= SHADER_HASDIFFUSE;
				if (r_fb_bmodels.ival)
					maps |= SHADER_HASFULLBRIGHT;
//				if (r_loadbumpmapping || (r_waterstyle.ival > 1 && *tx->name == '*'))
//					maps |= SHADER_HASNORMALMAP;
				if (gl_specular.ival)
					maps |= SHADER_HASGLOSS;
				R_BuildLegacyTexnums(mod->textures[a]->shader, mod->textures[a]->name, loadname, maps, 0, TF_MIP4_SOLID8, mod->textures[a]->width, mod->textures[a]->height, mod->textures[a]->mips, mod->textures[a]->palette);
				BZ_Free(mod->textures[a]->mips[0]);
			}
		}
		else
		{
			COM_FileBase (mod->name, loadname, sizeof(loadname));
			if (!strncmp(loadname, "b_", 2))
				Q_strncpyz(loadname, "bmodels", sizeof(loadname));
			for(a = 0; a < mod->numtextures; a++)
				Mod_FinishTexture(mod->textures[a], loadname, false);
		}
	}
	Mod_Batches_Build(mod, data);
	if (data)
		BZ_Free(data);
#endif
}

#ifdef Q1BSPS

struct vispatch_s
{
	void *fileptr;
	size_t filelen;

	void *visptr;
	int vislen;

	void *leafptr;
	int leaflen;
};

static void Mod_FindVisPatch(struct vispatch_s *patch, model_t *mod, size_t leaflumpsize)
{
	char patchname[MAX_QPATH];
	int *lenptr, len;
	int ofs;
	qbyte *file;
	char *mapname;
	memset(patch, 0, sizeof(*patch));

	if (!mod_external_vis.ival)
		return;

	mapname = COM_SkipPath(mod->name);

	COM_StripExtension(mod->name, patchname, sizeof(patchname));
	Q_strncatz(patchname, ".vis", sizeof(patchname));

	//ignore the patch file if its in a different gamedir.
	//this file format sucks too much for other verification.
	if (FS_FLocateFile(mod->name,FSLF_DEEPONFAILURE, NULL) != FS_FLocateFile(patchname,FSLF_DEEPONFAILURE, NULL))
		return;

	patch->filelen = FS_LoadFile(patchname, &patch->fileptr);
	if (!patch->fileptr)
		return;

	ofs = 0;
	while (ofs+36 <= patch->filelen)
	{
		file = patch->fileptr;
		file += ofs;
		memcpy(patchname, file, 32);
		patchname[32] = 0;
		file += 32;
		lenptr = (int*)file;
		file += sizeof(int);
		len = LittleLong(*lenptr);
		if (ofs+36+len > patch->filelen)
			break;

		if (!Q_strcasecmp(patchname, mapname))
		{
			lenptr = (int*)file;
			patch->vislen = LittleLong(*lenptr);
			file += sizeof(int);
			patch->visptr = file;
			file += patch->vislen;

			lenptr = (int*)file;
			patch->leaflen = LittleLong(*lenptr);
			file += sizeof(int);
			patch->leafptr = file;
			file += patch->leaflen;

			if (sizeof(int)*2 + patch->vislen + patch->leaflen != len || patch->leaflen != leaflumpsize)
			{
				patch->visptr = NULL;
				patch->leafptr = NULL;
			}
			else
				break;
		}
		ofs += 36+len;
	}
}

/*
=================
Mod_LoadBrushModel
=================
*/
static qboolean QDECL Mod_LoadBrushModel (model_t *mod, void *buffer, size_t fsize)
{
	struct vispatch_s vispatch;
	int			i, j;
	dheader_t	*header;
	mmodel_t 	*bm;
	model_t *submod;
	unsigned int chksum;
	qboolean noerrors;
	int longm = false;
	char loadname[32];
	qbyte *mod_base = buffer;
	qboolean hexen2map = false;
	qboolean isnotmap;
	qboolean using_rbe = true;

	COM_FileBase (mod->name, loadname, sizeof(loadname));
	mod->type = mod_brush;
	
	header = (dheader_t *)buffer;

#ifdef SERVERONLY
	isnotmap = !!sv.world.worldmodel;
#else
	if ((!cl.worldmodel && cls.state>=ca_connected)
#ifndef CLIENTONLY
		|| (!sv.world.worldmodel && sv.state)
#endif
		)
		isnotmap = false;
	else
		isnotmap = true;
#endif

	i = LittleLong (header->version);

	if (i == BSPVERSION || i == BSPVERSIONPREREL)
	{
		mod->fromgame = fg_quake;
		mod->engineflags |= MDLF_NEEDOVERBRIGHT;
	}
	else if (i == BSPVERSION_LONG1)
	{
		longm = true;
		mod->fromgame = fg_quake;
		mod->engineflags |= MDLF_NEEDOVERBRIGHT;
	}
	else if (i == BSPVERSION_LONG2)
	{
		longm = 2;
		mod->fromgame = fg_quake;
		mod->engineflags |= MDLF_NEEDOVERBRIGHT;
	}
	else if (i == BSPVERSIONHL)	//halflife support
		mod->fromgame = fg_halflife;
	else
	{
		Con_Printf (CON_ERROR "Mod_LoadBrushModel: %s has wrong version number (%i should be %i)\n", mod->name, i, BSPVERSION);
		return false;
	}

	mod->lightmaps.width = 128;//LMBLOCK_WIDTH;
	mod->lightmaps.height = 128;//LMBLOCK_HEIGHT; 

// swap all the lumps
	mod_base = (qbyte *)header;

	for (i=0 ; i<sizeof(dheader_t)/4 ; i++)
		((int *)header)[i] = LittleLong ( ((int *)header)[i]);

	Q1BSPX_Setup(mod, mod_base, fsize, header->lumps, HEADER_LUMPS);

// checksum all of the map, except for entities
	mod->checksum = 0;
	mod->checksum2 = 0;

	for (i = 0; i < HEADER_LUMPS; i++)
	{
		if ((unsigned)header->lumps[i].fileofs + (unsigned)header->lumps[i].filelen > fsize)
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

	Mod_FindVisPatch(&vispatch, mod, header->lumps[LUMP_LEAFS].filelen);

// load into heap
	if (!isDedicated || using_rbe)
	{
		TRACE(("Loading verts\n"));
		noerrors = noerrors && Mod_LoadVertexes (mod, mod_base, &header->lumps[LUMP_VERTEXES]);
		TRACE(("Loading edges\n"));
		noerrors = noerrors && Mod_LoadEdges (mod, mod_base, &header->lumps[LUMP_EDGES], longm);
		TRACE(("Loading Surfedges\n"));
		noerrors = noerrors && Mod_LoadSurfedges (mod, mod_base, &header->lumps[LUMP_SURFEDGES]);
	}
	if (!isDedicated)
	{
		TRACE(("Loading Textures\n"));
		noerrors = noerrors && Mod_LoadTextures (mod, mod_base, &header->lumps[LUMP_TEXTURES]);
	}
	TRACE(("Loading Submodels\n"));
	noerrors = noerrors && Mod_LoadSubmodels (mod, mod_base, &header->lumps[LUMP_MODELS], &hexen2map);
	if (noerrors)
	{
		TRACE(("Loading CH\n"));
		Mod_LoadCrouchHull(mod);
	}
	TRACE(("Loading Planes\n"));
	noerrors = noerrors && Mod_LoadPlanes (mod, mod_base, &header->lumps[LUMP_PLANES]);
	TRACE(("Loading Entities\n"));
	Mod_LoadEntities (mod, mod_base, &header->lumps[LUMP_ENTITIES]);
	if (!isDedicated || using_rbe)
	{
		TRACE(("Loading Texinfo\n"));
		noerrors = noerrors && Mod_LoadTexinfo (mod, mod_base, &header->lumps[LUMP_TEXINFO]);
		TRACE(("Loading Faces\n"));
		noerrors = noerrors && Mod_LoadFaces (mod, mod_base, &header->lumps[LUMP_FACES], &header->lumps[LUMP_LIGHTING], longm);
	}
	if (!isDedicated)
	{
		TRACE(("Loading MarkSurfaces\n"));
		noerrors = noerrors && Mod_LoadMarksurfaces (mod, mod_base, &header->lumps[LUMP_MARKSURFACES], longm);	
	}
	if (noerrors)
	{
		TRACE(("Loading Vis\n"));
		Mod_LoadVisibility (mod, mod_base, &header->lumps[LUMP_VISIBILITY], vispatch.visptr, vispatch.vislen);
	}
	noerrors = noerrors && Mod_LoadLeafs (mod, mod_base, &header->lumps[LUMP_LEAFS], longm, isnotmap, vispatch.leafptr, vispatch.leaflen);
	TRACE(("Loading Nodes\n"));
	noerrors = noerrors && Mod_LoadNodes (mod, mod_base, &header->lumps[LUMP_NODES], longm);
	TRACE(("Loading Clipnodes\n"));
	noerrors = noerrors && Mod_LoadClipnodes (mod, mod_base, &header->lumps[LUMP_CLIPNODES], longm, hexen2map);
	if (noerrors)
	{
		TRACE(("Loading hull 0\n"));
		Mod_MakeHull0 (mod);
	}

	TRACE(("sorting shaders\n"));
	if (!isDedicated && noerrors)
		Mod_SortShaders(mod);

	if (crouchhullfile)
	{
		FS_FreeFile(crouchhullfile);
		crouchhullfile=NULL;
	}

	BZ_Free(vispatch.fileptr);

	if (!noerrors)
	{
		return false;
	}

	TRACE(("LoadBrushModel %i\n", __LINE__));
	Q1BSP_LoadBrushes(mod);
	TRACE(("LoadBrushModel %i\n", __LINE__));
	Q1BSP_SetModelFuncs(mod);
	TRACE(("LoadBrushModel %i\n", __LINE__));
#ifndef SERVERONLY
	mod->funcs.LightPointValues		= GLQ1BSP_LightPointValues;
	mod->funcs.MarkLights			= Q1BSP_MarkLights;
	mod->funcs.StainNode			= Q1BSP_StainNode;
#endif

	mod->numframes = 2;		// regular and alternate animation
	

//
// set up the submodels (FIXME: this is confusing)
//
	for (i=0, submod = mod; i<mod->numsubmodels ; i++)
	{
		bm = &mod->submodels[i];

		submod->rootnode = submod->nodes + bm->headnode[0];
		submod->hulls[0].firstclipnode = bm->headnode[0];
		submod->hulls[0].available = true;
		Q1BSP_CheckHullNodes(&submod->hulls[0]);

TRACE(("LoadBrushModel %i\n", __LINE__));
		for (j=1 ; j<MAX_MAP_HULLSM ; j++)
		{
			submod->hulls[j].firstclipnode = bm->headnode[j];
			submod->hulls[j].lastclipnode = submod->numclipnodes-1;

			submod->hulls[j].available &= bm->hullavailable[j];
			if (submod->hulls[j].firstclipnode > submod->hulls[j].lastclipnode)
				submod->hulls[j].available = false;

			if (submod->hulls[j].available)
				Q1BSP_CheckHullNodes(&submod->hulls[j]);
		}

		if (mod->fromgame == fg_halflife && i)
		{
			for (j=bm->firstface ; j<bm->firstface+bm->numfaces ; j++)
			{
				if (mod->surfaces[j].flags & SURF_DRAWTURB)
				{
					if (mod->surfaces[j].plane->type == PLANE_Z && mod->surfaces[j].plane->dist == bm->maxs[2]-1)
						continue;
					mod->surfaces[j].flags |= SURF_NODRAW;
				}
			}
		}
		
		submod->firstmodelsurface = bm->firstface;
		submod->nummodelsurfaces = bm->numfaces;
		
		VectorCopy (bm->maxs, submod->maxs);
		VectorCopy (bm->mins, submod->mins);

		submod->radius = RadiusFromBounds (submod->mins, submod->maxs);

		submod->numclusters = bm->visleafs;

		if (i)
			submod->entities_raw = NULL;

		memset(&submod->batches, 0, sizeof(submod->batches));
		submod->vbos = NULL;
		TRACE(("LoadBrushModel %i\n", __LINE__));
		if (!isDedicated || using_rbe)
		{
			COM_AddWork(WG_MAIN, ModBrush_LoadGLStuff, submod, NULL, i, 0);
		}
		TRACE(("LoadBrushModel %i\n", __LINE__));

		if (i)
			COM_AddWork(WG_MAIN, Mod_ModelLoaded, submod, NULL, MLS_LOADED, 0);
		if (i < submod->numsubmodels-1)
		{	// duplicate the basic information
			char	name[MAX_QPATH];
			model_t *nextmod;

			Q_snprintfz (name, sizeof(name), "*%i:%s", i+1, mod->publicname);
			nextmod = Mod_FindName (name);
			*nextmod = *submod;
			nextmod->submodelof = mod;
			Q_strncpyz(nextmod->publicname, name, sizeof(nextmod->publicname));
			Q_snprintfz (nextmod->name, sizeof(nextmod->publicname), "*%i:%s", i+1, mod->publicname);
			submod = nextmod;
			memset(&submod->memgroup, 0, sizeof(submod->memgroup));
		}
		TRACE(("LoadBrushModel %i\n", __LINE__));
	}
#ifdef RUNTIMELIGHTING
	TRACE(("LoadBrushModel %i\n", __LINE__));
	if (lightmodel == mod)
	{
		lightcontext = LightStartup(NULL, lightmodel, true, !writelitfile);
		LightReloadEntities(lightcontext, Mod_GetEntitiesString(lightmodel), false);
	}
#endif
TRACE(("LoadBrushModel %i\n", __LINE__));
	if (!isDedicated)
		Mod_FixupMinsMaxs(mod);
TRACE(("LoadBrushModel %i\n", __LINE__));

#ifdef TERRAIN
	mod->terrain = Mod_LoadTerrainInfo(mod, loadname, false);
#endif
	return true;
}
#endif

/*
==============================================================================

SPRITES

==============================================================================
*/

//=========================================================

#ifdef SERVERONLY
//dedicated servers should not need to load sprites.
//dedicated servers need *.bsp to be loaded for setmodel to get the correct size (or all model types with sv_gameplayfix_setmodelrealbox).
//otherwise other model types(actually: names) only need to be loaded once reflection or hitmodel is used.
//for sprites we don't really care ever.
qboolean QDECL Mod_LoadSpriteModel (model_t *mod, void *buffer, size_t fsize)
{
	mod->type = mod_dummy;
	return true;
}
qboolean QDECL Mod_LoadSprite2Model (model_t *mod, void *buffer, size_t fsize)
{
	return Mod_LoadSpriteModel(mod, buffer, fsize);
}
void Mod_LoadDoomSprite (model_t *mod)
{
	mod->type = mod_dummy;
}
#else

//we need to override the rtlight shader for sprites so they get lit properly ignoring n+s+t dirs
//so lets split the shader into parts to avoid too many dupes
#define SPRITE_SHADER_MAIN									\
			"{\n"											\
				"if gl_blendsprites\n"						\
					"program defaultsprite\n"				\
				"else\n"									\
					"program defaultsprite#MASK=0.666\n"	\
				"endif\n"									\
				"{\n"										\
					"map $diffuse\n"						\
					"if gl_blendsprites\n"					\
						"blendfunc GL_SRC_ALPHA GL_ONE\n"	\
					"else\n"								\
						"alphafunc ge128\n"					\
						"depthwrite\n"						\
					"endif\n"								\
					"rgbgen vertex\n"						\
					"alphagen vertex\n"						\
				"}\n"										\
				"surfaceparm noshadows\n"
#define SPRITE_SHADER_UNLIT	"surfaceparm nodlight\n"
#define SPRITE_SHADER_LIT								\
				"sort seethrough\n"						\
				"bemode rtlight\n"						\
				"{\n"									\
					"program rtlight#NOBUMP\n"			\
					"{\n"								\
						"map $diffuse\n"				\
						"blendfunc add\n"				\
					"}\n"								\
				"}\n"
#define SPRITE_SHADER_FOOTER "}\n"

void Mod_LoadSpriteFrameShader(model_t *spr, int frame, int subframe, mspriteframe_t *frameinfo)
{
#ifndef SERVERONLY
	char *shadertext;
	char name[MAX_QPATH];
	qboolean litsprite = false;

	if (qrenderer == QR_NONE)
		return;

	if (subframe == -1)
		Q_snprintfz(name, sizeof(name), "%s_%i.tga", spr->name, frame);
	else
		Q_snprintfz(name, sizeof(name), "%s_%i_%i.tga", spr->name, frame, subframe);

	if (mod_litsprites_force.ival || strchr(spr->publicname, '!'))
		litsprite = true;
#ifndef NOLEGACY
	else
	{
		int i;
		/*
		A quick note on tenebrae and sprites: In tenebrae, sprites are always lit, unless the light_lev field is set (which makes it fullbright).
		While its generally preferable and more consistent to assume lit sprites, this is incompatible with vanilla quake and thus unacceptable to us, but you can set the mod_assumelitsprites cvar if you want it.
		So for better compatibility, we have a whitelist of 'well-known' sprites that tenebrae uses in this way, which we do lighting on.
		You should still be able to use EF_FULLBRIGHT on these, but light_lev is an imprecise setting and will result in issues. Just be specific about fullbright or additive.
		DP on the other hand, supports lit sprites only when the sprite contains a ! in its name. We support that too.
		*/
		static char *forcelitsprites[] =
		{
			"progs/smokepuff.spr",
			NULL
		};

		for (i = 0; forcelitsprites[i]; i++)
			if (!strcmp(spr->publicname, forcelitsprites[i]))
			{
				litsprite = true;
				break;
			}
	}
#endif

	if (litsprite)	// a ! in the filename makes it non-fullbright (and can also be lit by rtlights too).
	{
		shadertext = 
			"{\n"
				"program defaultsprite\n"
				"{\n"
					"map $diffuse\n"
					"blendfunc GL_SRC_ALPHA GL_ONE\n"
					"rgbgen vertex\n"
					"alphagen vertex\n"
				"}\n"
				"surfaceparm noshadows\n"
				"sort seethrough\n"
				"bemode rtlight\n"
				"{\n"
					"program rtlight#NOBUMP\n"
					"{\n"
						"map $diffuse\n"
						"blendfunc add\n"
					"}\n"
				"}\n"
			"}\n"
			;
	}
	else
		shadertext = SPRITE_SHADER_MAIN SPRITE_SHADER_UNLIT SPRITE_SHADER_FOOTER;
	frameinfo->shader = R_RegisterShader(name, SUF_NONE, shadertext);
	frameinfo->shader->defaulttextures->base = frameinfo->image;
	frameinfo->shader->width = frameinfo->right-frameinfo->left;
	frameinfo->shader->height = frameinfo->up-frameinfo->down;
#endif
}
void Mod_LoadSpriteShaders(model_t *spr)
{
	msprite_t *psprite = spr->meshinfo;
	int i, j;
	mspritegroup_t *group;

	for (i = 0; i < psprite->numframes; i++)
	{
		switch (psprite->frames[i].type)
		{
		case SPR_SINGLE:
			Mod_LoadSpriteFrameShader(spr, i, -1, psprite->frames[i].frameptr);
			break;
		case SPR_ANGLED:
		case SPR_GROUP:
			group = (mspritegroup_t *)psprite->frames[i].frameptr;
			for (j = 0; j < group->numframes; j++)
				Mod_LoadSpriteFrameShader(spr, i, j, group->frames[j]);
			break;
		}
	}
}

#ifdef SPRMODELS
/*
=================
Mod_LoadSpriteFrame
=================
*/
static void * Mod_LoadSpriteFrame (model_t *mod, void *pin, void *pend, mspriteframe_t **ppframe, int framenum, int version, unsigned char *palette)
{
	dspriteframe_t		*pinframe;
	mspriteframe_t		*pspriteframe;
	int					width, height, size, origin[2];
	char				name[64];
	uploadfmt_t			lowresfmt;
	void				*dataptr;

	pinframe = (dspriteframe_t *)pin;

	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	size = width * height;

	pspriteframe = ZG_Malloc(&mod->memgroup, sizeof (mspriteframe_t));

	Q_memset (pspriteframe, 0, sizeof (mspriteframe_t));

	*ppframe = pspriteframe;

	origin[0] = LittleLong (pinframe->origin[0]);
	origin[1] = LittleLong (pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];

	dataptr = (pinframe + 1);

	if (version == SPRITE32_VERSION)
	{
		size *= 4;
		lowresfmt = TF_RGBA32;
	}
	else if (version == SPRITEHL_VERSION)
		lowresfmt = TF_8PAL32;
	else
		lowresfmt = TF_TRANS8;

	if ((qbyte*)dataptr + size > (qbyte*)pend)
	{
		//tenebrae has a couple of dodgy truncated sprites. yay for replacement textures.
		dataptr = NULL;
		lowresfmt = TF_INVALID;
	}

	Q_snprintfz(name, sizeof(name), "%s_%i.tga", mod->name, framenum);
	pspriteframe->image = Image_GetTexture(name, "sprites", IF_NOMIPMAP|IF_NOGAMMA|IF_CLAMP, dataptr, palette, width, height, lowresfmt);

	return (void *)((qbyte *)(pinframe+1) + size);
}


/*
=================
Mod_LoadSpriteGroup
=================
*/
static void * Mod_LoadSpriteGroup (model_t *mod, void * pin, void *pend, mspriteframe_t **ppframe, int framenum, int version, unsigned char *palette)
{
	dspritegroup_t		*pingroup;
	mspritegroup_t		*pspritegroup;
	int					i, numframes;
	dspriteinterval_t	*pin_intervals;
	float				*poutintervals;
	void				*ptemp;
	float				prevtime;

	pingroup = (dspritegroup_t *)pin;

	numframes = LittleLong (pingroup->numframes);

	pspritegroup = ZG_Malloc(&mod->memgroup, sizeof (mspritegroup_t) + (numframes - 1) * sizeof (pspritegroup->frames[0]));

	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *)pspritegroup;

	pin_intervals = (dspriteinterval_t *)(pingroup + 1);

	poutintervals = ZG_Malloc(&mod->memgroup, numframes * sizeof (float));

	pspritegroup->intervals = poutintervals;

	for (i=0, prevtime=0 ; i<numframes ; i++)
	{
		*poutintervals = LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0)
		{
			Con_Printf (CON_ERROR "Mod_LoadSpriteGroup: interval<=0\n");
			return NULL;
		}
		prevtime = *poutintervals = prevtime+*poutintervals;

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *)pin_intervals;

	for (i=0 ; i<numframes ; i++)
	{
		ptemp = Mod_LoadSpriteFrame (mod, ptemp, pend, &pspritegroup->frames[i], framenum * 100 + i, version, palette);
	}

	return ptemp;
}

/*
=================
Mod_LoadSpriteModel
=================
*/
qboolean QDECL Mod_LoadSpriteModel (model_t *mod, void *buffer, size_t fsize)
{
	int					i;
	int					version;
	dsprite_t			*pin;
	msprite_t			*psprite;
	int					numframes;
	int					size;
	dspriteframetype_t	*pframetype;
	int rendertype=SPRHL_ALPHATEST;
	unsigned char pal[256*4];
	int sptype;
	
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
		rendertype = LittleLong (pin->type);	//not sure what the values mean.
	}

	numframes = LittleLong (pin->numframes);

	size = sizeof (msprite_t) +	(numframes - 1) * sizeof (psprite->frames);

	psprite = ZG_Malloc(&mod->memgroup, size);

	mod->meshinfo = psprite;
	switch(sptype)
	{
	case SPR_VP_PARALLEL_UPRIGHT:
	case SPR_FACING_UPRIGHT:
	case SPR_VP_PARALLEL:
	case SPR_ORIENTED:
//	case SPR_VP_PARALLEL_ORIENTED:
//	case SPRDP_LABEL:
//	case SPRDP_LABEL_SCALE:
//	case SPRDP_OVERHEAD:
		break;
	default:
		Con_DPrintf(CON_ERROR "%s has unsupported sprite type %i\n", mod->name, sptype);
		sptype = SPR_VP_PARALLEL;
		break;
	}
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
	if (qrenderer == QR_NONE)
	{
		mod->type = mod_dummy;
		return true;
	}

	if (version == SPRITEHL_VERSION)
	{
		int i;
		short *numi = (short*)(pin+1);
		unsigned char *src = (unsigned char *)(numi+1);
		if (LittleShort(*numi) != 256)
		{
			Con_Printf(CON_ERROR "%s has wrong number of palette indexes (we only support 256)\n", mod->name);
			return false;
		}

		if (rendertype == SPRHL_INDEXALPHA)
		{
			Con_Printf(CON_ERROR "%s: SPRHL_INDEXALPHA sprites are not supported\n", mod->name);
			return false;
		}
		else
		{
			for (i = 0; i < 256; i++)
			{//FIXME: bgr?
				pal[i*4+0] = *src++;
				pal[i*4+1] = *src++;
				pal[i*4+2] = *src++;
				pal[i*4+3] = 255;
			}
			if (rendertype == SPRHL_ALPHATEST)
			{
				pal[255*4+0] = 0;
				pal[255*4+1] = 0;
				pal[255*4+2] = 0;
				pal[255*4+3] = 0;
			}
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
					Mod_LoadSpriteFrame (mod, pframetype + 1, (qbyte*)buffer + fsize,
										 &psprite->frames[i].frameptr, i, version, pal);
		}
		else
		{
			pframetype = (dspriteframetype_t *)
					Mod_LoadSpriteGroup (mod, pframetype + 1, (qbyte*)buffer + fsize,
										 &psprite->frames[i].frameptr, i, version, pal);
		}
		if (pframetype == NULL)
		{
			return false;
		}
	}

	mod->type = mod_sprite;

	return true;
}
#endif

#ifdef SP2MODELS
qboolean QDECL Mod_LoadSprite2Model (model_t *mod, void *buffer, size_t fsize)
{
	int					i;
	int					version;
	dmd2sprite_t		*pin;
	msprite_t			*psprite;
	int					numframes;
	int					size;
	dmd2sprframe_t		*pframetype;
	mspriteframe_t		*frame;
	int w, h;
	float origin[2];

	
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

	psprite = ZG_Malloc(&mod->memgroup, size);

	mod->meshinfo = psprite;

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
		return false;
	}

	mod->numframes = numframes;

	pframetype = pin->frames;

	for (i=0 ; i<numframes ; i++)
	{
		spriteframetype_t	frametype;

		frametype = SPR_SINGLE;
		psprite->frames[i].type = frametype;

		frame = psprite->frames[i].frameptr = ZG_Malloc(&mod->memgroup, sizeof(mspriteframe_t));

		frame->image = Image_GetTexture(pframetype->name, NULL, IF_NOMIPMAP|IF_NOGAMMA|IF_CLAMP, NULL, NULL, 0, 0, TF_INVALID);

		w = LittleLong(pframetype->width);
		h = LittleLong(pframetype->height);
		origin[0] = LittleLong (pframetype->origin_x);
		origin[1] = LittleLong (pframetype->origin_y);

		frame->down = -origin[1];
		frame->up = h - origin[1];
		frame->left = -origin[0];
		frame->right = w - origin[0];

		pframetype++;
	}

	mod->type = mod_sprite;

	return true;
}
#endif

#ifdef DSPMODELS

typedef struct {
	short width;
	short height;
	short xpos;
	short ypos;
} doomimage_t;
static int QDECL FindDoomSprites(const char *name, qofs_t size, void *param, searchpathfuncs_t *spath)
{
	if (*(int *)param + strlen(name)+1 > 16000)
		Sys_Error("Too many doom sprites\n");

	strcpy((char *)param + *(int *)param, name);
	*(int *)param += strlen(name)+1;

	return true;
}


static void LoadDoomSpriteFrame(model_t *mod, char *imagename, mspriteframedesc_t *pdesc, int anglenum, qboolean xmirrored)
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
		pdesc->frameptr = pframe = ZG_Malloc(&mod->memgroup, sizeof(*pframe));
	}
	else
	{
		mspritegroup_t *group;

		if (!pdesc->frameptr || pdesc->type != SPR_ANGLED)
		{
			pdesc->type = SPR_ANGLED;
			group = ZG_Malloc(&mod->memgroup, sizeof(*group)+sizeof(mspriteframe_t *)*(8-1));
			pdesc->frameptr = (mspriteframe_t *)group;
			group->numframes = 8;
		}
		else
			group = (mspritegroup_t *)pdesc->frameptr;

		pframe = ZG_Malloc(&mod->memgroup, sizeof(*pframe));
		group->frames[anglenum-1] = pframe;
	}

	palette = COM_LoadTempFile("wad/playpal");
	header = (doomimage_t *)COM_LoadTempMoreFile(imagename);
	data = (qbyte *)header;
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

	pframe->shader = R_RegisterShader(imagename, SUF_NONE, 
		"{\n{\nmap $diffuse\nblendfunc blend\n}\n}\n");
	pframe->shader->defaulttextures.base = R_LoadTexture8Pal24(imagename, header->width, header->height, image, palette, IF_CLAMP);
	R_BuildDefaultTexnums(NULL, pframe->shader);
}

/*
=================
Doom Sprites
=================
*/
void Mod_LoadDoomSprite (model_t *mod)
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
	psprite = ZG_Malloc(&mod->memgroup, size);

	psprite->numframes = numframes;

	//do the actual loading.
	for (ofs = 4; ofs < *(int*)files; ofs+=strlen(files+ofs)+1)
	{
		name = files+ofs;
		framenum = name[baselen+0] - 'a';
		anglenum = name[baselen+1] - '0';

		LoadDoomSpriteFrame(mod, name, &psprite->frames[framenum], anglenum, false);

		if (name[baselen+2])	//is there a second element?
		{
			framenum = name[baselen+2] - 'a';
			anglenum = name[baselen+3] - '0';

			LoadDoomSpriteFrame(mod, name, &psprite->frames[framenum], anglenum, true);
		}
	}


	psprite->type = SPR_FACING_UPRIGHT;
	mod->type = mod_sprite;

	mod->meshinfo = psprite;
}
#endif

#endif

//=============================================================================

/*
================
Mod_Print
================
*/
void Mod_Print (void)
{
	int		i;
	model_t	*mod;

	Con_Printf ("Cached models:\n");
	for (i=0, mod=mod_known ; i < mod_numknown ; i++, mod++)
	{
		Con_Printf ("%8p : %s\n", mod->meshinfo, mod->name);
	}
}


#endif
