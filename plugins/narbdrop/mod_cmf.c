#if !defined(GLQUAKE) && !defined(FTEENGINE)
#define GLQUAKE
#endif

#include "../plugin.h"
#include "com_mesh.h"
#include "com_bih.h"

static plugmodfuncs_t *modfuncs;

#ifdef FTEPLUGIN
#undef ZG_Malloc
#define ZG_Malloc(g,s) plugfuncs->GMalloc(g,s)
#undef Z_Free
#define Z_Free(p) plugfuncs->Free(p)
#undef Z_Malloc
#define Z_Malloc(s) plugfuncs->Malloc(s)
#undef Z_Realloc
#define Z_Realloc(p,s) plugfuncs->Realloc(p,s)
#undef Mod_AccumulateTextureVectors
#define Mod_AccumulateTextureVectors(vc,tc,nv,sv,tv,idx,numidx,calcnorms) modfuncs->AccumulateTextureVectors(vc,tc,nv,sv,tv,idx,numidx,calcnorms)
#undef Mod_NormaliseTextureVectors
#define Mod_NormaliseTextureVectors(n,s,t,v,calcnorms) modfuncs->NormaliseTextureVectors(n,s,t,v,calcnorms)
void AddPointToBounds(const vec3_t v, vec3_t mins, vec3_t maxs)
{
	int		i;
	vec_t	val;

	for (i=0 ; i<3 ; i++)
	{
		val = v[i];
		if (val < mins[i])
			mins[i] = val;
		if (val > maxs[i])
			maxs[i] = val;
	}
}

void ClearBounds(vec3_t mins, vec3_t maxs)
{
	mins[0] = mins[1] = mins[2] = FLT_MAX;
	maxs[0] = maxs[1] = maxs[2] = -FLT_MAX;
}

vec_t Length(const vec3_t v)
{
	int		i;
	float	length;

	length = 0;
	for (i=0 ; i< 3 ; i++)
		length += v[i]*v[i];
	length = sqrt (length);		// FIXME

	return length;
}

float RadiusFromBounds(const vec3_t mins, const vec3_t maxs)
{
	int		i;
	vec3_t	corner;

	for (i=0 ; i<3 ; i++)
	{
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);
	}

	return Length (corner);
}
#endif

static int CMF_ClusterForPoint(struct model_s *model, const vec3_t point, int *areaout)
{
	if (areaout) *areaout = 0;
	return -1;
}
static qbyte *CMF_ClusterPVS(struct model_s *model, int cluster, pvsbuffer_t *pvsbuffer, pvsmerge_t merge)
{
	return NULL;
}
static unsigned int CMF_FatPVS(struct model_s *model, const vec3_t org, pvsbuffer_t *pvsbuffer, qboolean merge)
{
	return 0;
}
qboolean CMF_EdictInFatPVS(struct model_s *model, const struct pvscache_s *edict, const qbyte *pvs, const int *areas)
{
	return true;
}
void CMF_FindTouchedLeafs(struct model_s *model, struct pvscache_s *ent, const vec3_t cullmins, const vec3_t cullmaxs)
{

}
static void CMF_LightPointValues(struct model_s *model, const vec3_t point, vec3_t res_diffuse, vec3_t res_ambient, vec3_t res_dir)
{
	VectorSet(res_diffuse, 255,255,255);
	VectorSet(res_ambient, 128,128,128);
	VectorSet(res_dir, 0,0,1);
}

static qboolean QDECL Mod_LoadCMFModel(model_t *mod, void *buffer, size_t fsize)
{
	galiasinfo_t *gai;
	int i, j, k;
	uint32_t num_wads, num_entities, num_textures;
	galiasskin_t *skins;
	qbyte *bufptr = (qbyte *)buffer;
	int len_entities;
	char *entities;
	char *entptr;
	qbyte **polygons = NULL;
	int totalpolys = 0;
	struct bihleaf_s *bihleaf;
	int totaltris = 0;
	int totalleafs = 0;

	// sanity check
	if (memcmp(bufptr, "CMF", 3) != 0 || bufptr[3] > 3)
	{
		Con_Printf("%s: format not recognised\n", mod->name);
		return false;
	}

	// skip preview image
	if (bufptr[3] > 2)
	{
		uint32_t w, h;
		w = *(uint32_t *)(bufptr + 4);
		h = *(uint32_t *)(bufptr + 8);
		bufptr += 12 + (w * h * 4);
	}
	else
	{
		bufptr += 4;
	}

	// get counts
	num_wads = *(uint32_t *)bufptr; bufptr += 4;
	num_entities = *(uint32_t *)bufptr; bufptr += 4;
	num_textures = *(uint32_t *)bufptr; bufptr += 4;

	// skip past wads
	for (i = 0; i < num_wads; i++)
	{
		char name[MAX_QPATH];
		strncpy(name, bufptr, sizeof(name));
		bufptr += strlen(name) + 1;
	}

	// allocate skins
	skins = ZG_Malloc(&mod->memgroup, sizeof(*skins) * num_textures);

	// read in skins
	// TODO: should add the video/ prefix for these, since the engine won't know to look there?
	for (i = 0; i < num_textures; i++)
	{
		char name[MAX_QPATH];
		strncpy(name, bufptr, sizeof(name));
		bufptr += strlen(name) + 1;

		skins[i].skinspeed = 0.1;
		skins[i].numframes = 1;
		skins[i].frame = ZG_Malloc(&mod->memgroup, sizeof(*skins[i].frame));
		Q_snprintfz(skins[i].name, sizeof(skins[i].name), "video/%s", name);
		Q_snprintfz(skins[i].frame[0].shadername, sizeof(skins[i].frame[0].shadername), "video/%s", name);
	}

	// allocate one gai for each texture
	gai = ZG_Malloc(&mod->memgroup, sizeof(*gai) * num_textures);

	// clear bounds
	ClearBounds(mod->mins, mod->maxs);

	// allocate entities text buffer that is hopefully big enough
	// TODO: dont be stupid, calculate this from the number of entities
	len_entities = num_entities * 256;
	entities = Z_Malloc(len_entities);
	entptr = entities;

	// read entities
	for (i = 0; i < num_entities; i++)
	{
		// read classname
		uint32_t num_properties, num_polygons;
		char classname[MAX_QPATH];
		strncpy(classname, bufptr, sizeof(classname));
		bufptr += strlen(classname) + 1;

		entptr += snprintf(entptr, len_entities - (entptr - entities), "{\"classname\"\"%s\"", classname);

		// read properties
		num_properties = *(uint32_t *)bufptr; bufptr += 4;
		for (j = 0; j < num_properties; j++)
		{
			char key[MAX_QPATH];
			char val[MAX_QPATH];
			strncpy(key, bufptr, sizeof(key));
			bufptr += strlen(key) + 1;
			strncpy(val, bufptr, sizeof(val));
			bufptr += strlen(val) + 1;

			entptr += snprintf(entptr, len_entities - (entptr - entities), "\"%s\"\"%s\"", key, val);
		}

		*entptr++ = '}';

		// read polygons
		num_polygons = *(uint32_t *)bufptr; bufptr += 4;
		if (num_polygons)
		{
			totalpolys += num_polygons;
			polygons = Z_Realloc(polygons, sizeof(*polygons) * totalpolys);
			for (j = 0; j < num_polygons; j++)
			{
				uint32_t texture;
				uint32_t num_vertices;

				polygons[totalpolys - num_polygons + j] = bufptr;

				texture = *(uint32_t *)bufptr; bufptr += 4;
				bufptr += 32; // skip plane

				// read vertices
				num_vertices = *(uint32_t *)bufptr; bufptr += 4;
				gai[texture].numverts += num_vertices;
				gai[texture].numindexes += 3 * (num_vertices - 2);
				bufptr += num_vertices * 40;
			}
		}
	}

	// load entities
	modfuncs->LoadEntities(mod, entities, entptr - entities);
	Z_Free(entities);

	// read in polygons
	for (i = 0; i < num_textures; i++)
	{
		int nverts, ntris;

		// allocate arrays
		gai[i].ofs_skel_xyz = ZG_Malloc(&mod->memgroup, sizeof(*gai[i].ofs_skel_xyz) * gai[i].numverts);
		gai[i].ofs_st_array = ZG_Malloc(&mod->memgroup, sizeof(*gai[i].ofs_st_array) * gai[i].numverts);
		gai[i].ofs_skel_norm = ZG_Malloc(&mod->memgroup, sizeof(*gai[i].ofs_skel_norm) * gai[i].numverts);
		gai[i].ofs_skel_svect = ZG_Malloc(&mod->memgroup, sizeof(*gai[i].ofs_skel_svect) * gai[i].numverts);
		gai[i].ofs_skel_tvect = ZG_Malloc(&mod->memgroup, sizeof(*gai[i].ofs_skel_tvect) * gai[i].numverts);

		gai[i].ofs_indexes = ZG_Malloc(&mod->memgroup, sizeof(*gai[i].ofs_indexes) * gai[i].numindexes);

		// read in our vertex positions and uvs
		nverts = ntris = 0;
		for (j = 0; j < totalpolys; j++)
		{
			uint32_t texture, num_vertices;
			qbyte *ptr = polygons[j];
			texture = *(uint32_t *)ptr; ptr += 4;
			if (texture == i)
			{
				ptr += 32; // skip plane
				num_vertices = *(uint32_t *)ptr; ptr += 4;

				// make triangle fan
				for (k = 0; k < num_vertices - 2; k++)
				{
					gai[i].ofs_indexes[ntris * 3 + 0] = nverts;
					gai[i].ofs_indexes[ntris * 3 + 1] = nverts + k + 1;
					gai[i].ofs_indexes[ntris * 3 + 2] = nverts + k + 2;
					ntris++;
					totaltris++;
				}

				// make verts
				for (k = 0; k < num_vertices; k++)
				{
					double *v = (double *)ptr; ptr += 40;
					gai[i].ofs_skel_xyz[nverts][0] = v[0];
					gai[i].ofs_skel_xyz[nverts][1] = v[2];
					gai[i].ofs_skel_xyz[nverts][2] = v[1];
					gai[i].ofs_st_array[nverts][0] = v[3];
					gai[i].ofs_st_array[nverts][1] = v[4];
					AddPointToBounds(gai[i].ofs_skel_xyz[nverts], mod->mins, mod->maxs);
					nverts++;
				}
			}
		}

		// generate normals and texture vectors
		Mod_AccumulateTextureVectors(gai[i].ofs_skel_xyz, gai[i].ofs_st_array, gai[i].ofs_skel_norm, gai[i].ofs_skel_svect, gai[i].ofs_skel_tvect, gai[i].ofs_indexes, gai[i].numindexes, true);
		Mod_NormaliseTextureVectors(gai[i].ofs_skel_norm, gai[i].ofs_skel_svect, gai[i].ofs_skel_tvect, gai[i].numverts, true);

		// setup skin
		gai[i].numskins = 1;
		gai[i].ofsskins = &skins[i];

		// setup misc info
		Q_strlcpy(gai[i].surfacename, skins[i].name, sizeof(gai[i].surfacename));
		gai[i].contents = FTECONTENTS_BODY;
		gai[i].geomset = ~0; //invalid set = always visible
		gai[i].geomid = 0;
		gai[i].mindist = 0;
		gai[i].maxdist = 0;
		gai[i].surfaceid = i;
		gai[i].shares_verts = i;
		gai[i].nextsurf = &gai[i+1];
	}

	// reset the last pointer so it doesnt point to nothing
	gai[i-1].nextsurf = NULL;

	// build BIH tree
	bihleaf = Z_Malloc(sizeof(*bihleaf) * totaltris);
	for (i = 0; i < num_textures; i++)
	{
		for (j = 0; j < gai[i].numindexes; j += 3)
		{
			bihleaf[totalleafs].type = BIH_TRIANGLE;
			bihleaf[totalleafs].data.contents = FTECONTENTS_BODY;
			bihleaf[totalleafs].data.tri.indexes = &gai[i].ofs_indexes[j];
			bihleaf[totalleafs].data.tri.xyz = gai[i].ofs_skel_xyz;
			AddPointToBounds(gai[i].ofs_skel_xyz[gai[i].ofs_indexes[j + 0]], bihleaf[totalleafs].mins, bihleaf[totalleafs].maxs);
			AddPointToBounds(gai[i].ofs_skel_xyz[gai[i].ofs_indexes[j + 1]], bihleaf[totalleafs].mins, bihleaf[totalleafs].maxs);
			AddPointToBounds(gai[i].ofs_skel_xyz[gai[i].ofs_indexes[j + 2]], bihleaf[totalleafs].mins, bihleaf[totalleafs].maxs);
			totalleafs++;
		}
	}
	modfuncs->BIH_Build(mod, bihleaf, totaltris);
	Z_Free(bihleaf);

	// set funcs
	mod->funcs.FatPVS = CMF_FatPVS;
	mod->funcs.EdictInFatPVS = CMF_EdictInFatPVS;
	mod->funcs.FindTouchedLeafs = CMF_FindTouchedLeafs;
	mod->funcs.LightPointValues = CMF_LightPointValues;
	mod->funcs.ClusterForPoint = CMF_ClusterForPoint;
	mod->funcs.ClusterPVS = CMF_ClusterPVS;

	// finish up
	mod->meshinfo = gai;
	mod->type = mod_alias;
	mod->radius = RadiusFromBounds(mod->mins, mod->maxs);
	mod->flags = 0;

	return true;
}

qboolean CMF_Init(void)
{
	modfuncs = plugfuncs->GetEngineInterface(plugmodfuncs_name, sizeof(*modfuncs));
	if (!modfuncs || modfuncs->version != MODPLUGFUNCS_VERSION)
		return false;

	modfuncs->RegisterModelFormatMagic("CSG Map File (cmf)", "CMF", 3, Mod_LoadCMFModel);
	return true;
}
